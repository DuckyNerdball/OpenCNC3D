"""Measure Enhanced C3M materials against the shared Mammoth/Yard style envelope.

Usage: python3 tools/art/style_audit.py [MODEL.c3m ...] [--json REPORT.json]
The numerical gate catches large value/roughness mistakes; art approval still uses
the fixed USER92 game view described in docs/enhanced-model-style.md.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np

ROOT = Path(__file__).resolve().parents[2]

GATE = json.loads((ROOT / 'art/style/enhanced_v1.json').read_text())['gate']


def read_c3m(path):
    """Read the common material/geometry prefix of C3M v1, v2 and v3.

    v2 skin/clip data and v3 LOD data follow the textures and do not change the
    full-resolution material draws used for the appearance audit.
    """
    data = path.read_bytes()
    if data[:8] != b'C3DMODL1' or len(data) < 72:
        raise ValueError(f'{path}: missing C3M header')
    version, nv, ni, npart, ns, nm, nt, nbone = struct.unpack_from('<8I', data, 8)
    if version not in (1, 2, 3) or (version == 1 and nbone) or ni % 3:
        raise ValueError(f'{path}: unsupported C3M header')
    off = 72
    verts = np.frombuffer(data, '<f4', nv * 8, off).reshape(nv, 8)
    off += nv * 32
    indices = np.frombuffer(data, '<u4', ni, off)
    off += ni * 4 + npart * 32
    subs = np.frombuffer(data, '<u4', ns * 4, off).reshape(ns, 4)
    off += ns * 16
    mats = np.frombuffer(data, '<i4', nm * 4, off).reshape(nm, 4)
    off += nm * 16
    textures = []
    for _ in range(nt):
        width, height, size = struct.unpack_from('<3I', data, off)
        off += 12
        if size != width * height * 4 or off + size > len(data):
            raise ValueError(f'{path}: malformed texture payload')
        textures.append(np.frombuffer(data, 'u1', size, off).reshape(height, width, 4))
        off += size
    if version == 1 and off != len(data):
        raise ValueError(f'{path}: unexpected v1 tail')
    if not np.isfinite(verts).all() or (ni and indices.max() >= nv):
        raise ValueError(f'{path}: invalid vertices or indices')
    if sum(int(s[3]) for s in subs) != ni:
        raise ValueError(f'{path}: submesh index counts disagree')
    return {'version': version, 'vertices': verts, 'indices': indices,
            'subs': subs, 'mats': mats, 'textures': textures,
            'triangles': ni // 3, 'parts': npart, 'trailing_bytes': len(data) - off}


def weighted_quantile(values, weights, q):
    order = np.argsort(values)
    values, weights = values[order], weights[order]
    return float(np.interp(q * weights.sum(), np.cumsum(weights), values))


def summarize(values, weights):
    return {
        'mean': round(float(np.average(values, weights=weights)), 4),
        'p10': round(weighted_quantile(values, weights, .10), 4),
        'median': round(weighted_quantile(values, weights, .50), 4),
        'p90': round(weighted_quantile(values, weights, .90), 4),
    }


def srgb_to_linear(rgb):
    rgb = rgb.astype(np.float64) / 255.0
    return np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)


def measure(path):
    model = read_c3m(path)
    verts = model['vertices']
    indices = model['indices']
    results = []
    failures = []
    for mi, material in enumerate(model['mats']):
        base_i, normal_i, orm_i, flags = map(int, material)
        if not all(0 <= i < len(model['textures']) for i in (base_i, normal_i, orm_i)):
            failures.append(f'material {mi}: missing BASE/NORM/ORM texture')
            continue
        base, normal, orm = (model['textures'][i] for i in (base_i, normal_i, orm_i))
        if base.shape != normal.shape or base.shape != orm.shape or base.shape[0] != base.shape[1]:
            failures.append(f'material {mi}: maps must be equal-sized square RGBA atlases')
            continue
        if base.shape[0] not in (256, 512, 1024, 2048):
            failures.append(f'material {mi}: unsupported atlas dimension {base.shape[0]}')
        batches = []
        for sub in model['subs']:
            if int(sub[1]) != mi:
                continue
            start, count = map(int, sub[2:4])
            batches.append(indices[start:start + count].reshape(-1, 3))
        if not batches:
            failures.append(f'material {mi}: no triangles')
            continue
        triangles = np.concatenate(batches)
        points = verts[triangles, :3]
        area = np.linalg.norm(np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]), axis=1) / 2
        valid = area > 1e-7
        if not valid.all():
            failures.append(f'material {mi}: {int((~valid).sum())} zero-area triangles')
        triangles, area = triangles[valid], area[valid]
        if not len(area):
            continue
        # Three interior samples reduce sensitivity to triangulation and UV seams.
        uv = verts[triangles, 6:8]
        bary = np.array([[.6, .2, .2], [.2, .6, .2], [.2, .2, .6]])
        sample_uv = np.einsum('sv,tvc->tsc', bary, uv).reshape(-1, 2) % 1.0
        weights = np.repeat(area / 3, 3)
        height, width = base.shape[:2]
        x = np.minimum((sample_uv[:, 0] * width).astype(int), width - 1)
        y = np.minimum((sample_uv[:, 1] * height).astype(int), height - 1)
        rgb = base[y, x, :3]
        linear = srgb_to_linear(rgb)
        luminance = linear @ np.array([.2126, .7152, .0722])
        rough = orm[y, x, 1].astype(float) / 255
        metal = orm[y, x, 2].astype(float) / 255
        mask = base[y, x, 3].astype(float) / 255
        painted = metal < .15
        row = {
            'material_index': mi,
            'material_flags': flags,
            'triangles': len(triangles),
            'atlas_pixels': width,
            'surface_area': round(float(area.sum()), 2),
            'linear_luminance': summarize(luminance, weights),
            'roughness': summarize(rough, weights),
            'metallic': summarize(metal, weights),
            'player_mask_coverage': round(float(weights[mask > .5].sum() / weights.sum()), 4),
            'painted_surface_fraction': round(float(weights[painted].sum() / weights.sum()), 4),
        }
        if painted.any():
            row['paint_linear_luminance'] = summarize(luminance[painted], weights[painted])
            row['paint_roughness'] = summarize(rough[painted], weights[painted])
        results.append(row)
        # C3M flags identify cloth (4), emblem (8), tread (1) and no-player (2).
        # A white patch or an unpainted rifle must not be graded as tank paint.
        if flags & 8:
            gate_class, max_lum, min_rough = 'emblem', GATE['emblem_max_luminance'], GATE['emblem_min_roughness']
        elif flags & 4:
            gate_class, max_lum, min_rough = 'cloth', GATE['cloth_max_luminance'], GATE['cloth_min_roughness']
        elif flags & 2:
            gate_class, max_lum, min_rough = 'unrecoloured', GATE['unrecoloured_max_luminance'], GATE['unrecoloured_min_roughness']
        else:
            gate_class = 'paintable_hard_surface'
            max_lum = GATE['max_area_weighted_linear_luminance']
            min_rough = GATE['min_material_median_roughness']
        row['gate_class'] = gate_class
        if row['linear_luminance']['mean'] > max_lum:
            failures.append(f'material {mi} ({gate_class}): area-weighted linear luminance exceeds {max_lum}')
        if row['roughness']['median'] < min_rough:
            failures.append(f'material {mi} ({gate_class}): median roughness below {min_rough}')
        if gate_class == 'paintable_hard_surface' and painted.any() and row['paint_roughness']['median'] < GATE['min_painted_median_roughness']:
            failures.append(f'material {mi}: painted-surface median roughness below {GATE["min_painted_median_roughness"]}')
    try:
        model_name = str(path.relative_to(ROOT))
    except ValueError:
        model_name = str(path)
    return {
        'model': model_name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        'version': model['version'], 'triangles': model['triangles'], 'parts': model['parts'],
        'materials': results, 'failures': failures, 'passed': not failures,
        'sampling': 'three interior UV samples per triangle, weighted by world-space area',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('models', nargs='*', type=Path)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    paths = args.models or sorted((ROOT / 'game/models3d').glob('*.c3m'))
    if not paths:
        parser.error('no C3M models found')
    report = {'style_version': 'enhanced-v1', 'assets': [measure(p.resolve()) for p in paths]}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    if any(not a['passed'] for a in report['assets']):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
