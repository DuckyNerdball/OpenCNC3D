"""Validate the Yard delivery and optionally run its actual Windows engine gate.

python tools/art/test_construction_yard.py [--runtime PATH_TO_STAGED_GAME]
Requires numpy and Pillow. Runtime results go under the staged game's shots/yard.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


def read_c3m(path):
    data = path.read_bytes()
    assert data[:8] == b'C3DMODL1'
    version, nv, ni, npart, ns, nm, nt, reserved = struct.unpack_from('<8I', data, 8)
    assert version == 1 and reserved == 0 and ni % 3 == 0
    bounds = struct.unpack_from('<6f', data, 48)
    off = 72
    vertices = np.frombuffer(data, '<f4', nv * 8, off).reshape(nv, 8)
    off += nv * 32
    indices = np.frombuffer(data, '<u4', ni, off)
    off += ni * 4 + npart * 32
    subs = np.frombuffer(data, '<u4', ns * 4, off).reshape(ns, 4)
    off += ns * 16
    mats = np.frombuffer(data, '<i4', nm * 4, off).reshape(nm, 4)
    off += nm * 16
    textures = []
    for _ in range(nt):
        w, h, size = struct.unpack_from('<3I', data, off)
        off += 12
        assert size == w * h * 4
        textures.append(np.frombuffer(data, 'u1', size, off).reshape(h, w, 4))
        off += size
    assert off == len(data) and np.isfinite(vertices).all()
    assert indices.max() < nv and sum(int(s[3]) for s in subs) == ni
    return dict(vertices=vertices, indices=indices, subs=subs, mats=mats,
                textures=textures, bounds=bounds, triangles=ni // 3, parts=npart)


def asset_gate():
    path = ROOT / 'game/models3d/FACT.c3m'
    model = read_c3m(path)
    assert model['triangles'] == 9820 <= 10000
    assert model['parts'] == 1 and len(model['mats']) == 2
    assert np.allclose(model['bounds'], [-1273, 0, -1353, 1488, 1104.676, 1072], atol=.1)
    assert np.allclose(np.linalg.norm(model['vertices'][:, 3:6], axis=1), 1, atol=.001)
    art = ROOT / 'art/construction_yard/runtime_v01'
    manifest = json.loads((art / 'bake_manifest.json').read_text())
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    assert sha(art / manifest['output']) == manifest['output_sha256']
    for row in manifest['maps']:
        assert sha(art / row['file']) == row['sha256']
        assert Image.open(art / row['file']).size == (2048, 2048)
    for mi, atlas in enumerate(['Structure', 'Mechanisms']):
        packed = model['textures'][model['mats'][mi][0]][:, :, 3]
        mask = np.asarray(Image.open(art / ('Yard_' + atlas + '_PlayerColorMask.png')))
        mask = mask[:, :, 0] if mask.ndim == 3 else mask
        # The source is a 16-bit linear PNG. Pillow exposes RGBA as eight-bit;
        # allow quantisation of both the source read and runtime box filter.
        if mask.max() > 255:
            mask = mask / 257
        expected = mask[::-1].reshape(1024, 2, 1024, 2).mean(axis=(1, 3))
        assert np.abs(packed.astype(float) - expected).max() <= 2
        # Both atlases contain some painted trim; neither is globally all black.
        # The separate Blender surface audit verifies the concrete/roof UV islands.
        assert packed.min() == 0 and np.count_nonzero(packed > 127) > 1000
    ini = (ROOT / 'game/examples/USER92.INI').read_text()
    assert 'GoodGuy,FACT,256,2147,0,None' in ini
    assert 'GoodGuy,HTNK,256,2080,96,Guard,None' in ini
    height = (ROOT / 'game/examples/USER92.HGT').read_bytes()
    assert len(height) == 65 * 65
    assert len({height[y * 65 + x] for y in range(30, 37) for x in range(30, 39)}) == 1
    return dict(triangles=9820, bounds=list(model['bounds']), meshes=211,
                c3m_sha256=sha(path), player_mask_pairing='explicit per-atlas grayscale, BASE alpha',
                map='USER92: FACT (35,33), HTNK (32,32), same flat plateau')


def engine_gate(runtime):
    runtime = runtime.resolve()
    assert (runtime / 'CONQUER.MIX').is_file(), 'The brain needs its buildup data to sell buildings'
    out = runtime / 'shots/yard'
    out.mkdir(parents=True, exist_ok=True)
    mission = runtime / 'missions/USER92.INI'
    original = mission.read_bytes()
    setup = 'gfx experimental_textures 1\ncam 34 33\nzoom max\ntick 6\n'

    def run(name, commands, enhanced=True, replacement=None):
        mission.write_bytes(original if replacement is None else replacement)
        script = out / (name + '.txt')
        script.write_text(setup + commands + '\nquit\n')
        for image in re.findall(r'^shot (.+)$', commands, re.M):
            p = runtime / image
            if p.exists():
                p.unlink()  # Only this gate's explicitly named stale screenshot.
        args = [str(runtime / 'cnc_eyes.exe'), '--scen', 'USER92', '--pack', 'SCG01EA.pack',
                '--dylib', 'TiberianDawn.dll', '--dir', 'missions/', '--content', 'content/',
                '--cameos', 'cameos.pack', '--dospack', 'dossidebar.pack', '--dosinf',
                'dosinfantry.pack', '--noshroud', '--w', '1600', '--h', '1000',
                '--script', str(script)]
        if enhanced:
            args.append('--enhanced')
        log = out / (name + '.log')
        with log.open('w') as handle:
            result = subprocess.run(args, cwd=runtime, stdout=handle, stderr=subprocess.STDOUT,
                                    timeout=180)
        text = log.read_text(errors='replace')
        assert result.returncode == 0 and re.search(r'SCRIPT.*0 failures', text), text[-2000:]
        for image in re.findall(r'^shot (.+)$', commands, re.M):
            assert (runtime / image).stat().st_size > 10000
        return text

    def shot(name):
        return 'shot shots/yard/' + name + '.png\nm3dump'

    try:
        text = run('switches', shot('on') + '\ngfx experimental_textures 0\n' + shot('off')
                   + '\ngfx experimental_textures 1\n' + shot('restored')
                   + '\ngfx m3_on 0\n' + shot('stock') + '\nfbdump')
        assert text.count('|drawn=2|tris=19810|') == 2
        assert text.count('|drawn=0|tris=0|') == 2
        assert '|cell=35,33|model=1|visible=1|construct=0|damaged=0|' in text
        assert 'unresolved=0' in text
        assert (out / 'on.png').read_bytes() == (out / 'restored.png').read_bytes()
        assert (out / 'on.png').read_bytes() != (out / 'stock.png').read_bytes()
        repeated = run('repeat', shot('repeat'))
        assert '|drawn=2|tris=19810|' in repeated
        assert (out / 'on.png').read_bytes() == (out / 'repeat.png').read_bytes()
        classic = run('classic', shot('classic'), enhanced=False)
        assert '|on=0|' in classic and '|drawn=0|tris=0|' in classic
        red = run('player_red', shot('red'), replacement=original.replace(
            b'GoodGuy,FACT,256,2147', b'BadGuy,FACT,256,2147'))
        assert '|drawn=2|tris=19810|' in red
        damaged = run('damaged', shot('damaged'), replacement=original.replace(
            b'GoodGuy,FACT,256,2147', b'GoodGuy,FACT,64,2147'))
        assert '|cell=35,33|model=-1|visible=1|construct=0|damaged=1|' in damaged
        assert '|drawn=1|tris=9990|' in damaged
        selling = run('selling', 'sellmode 1\nlclickobj FACT\ntick 5\n' + shot('selling'))
        assert '|construct=1|' in selling and '|drawn=1|tris=9990|' in selling
        a, b = [np.asarray(Image.open(out / (n + '.png')).convert('RGB')) for n in ['on', 'red']]
        changed = np.max(np.abs(a.astype(int) - b.astype(int)), axis=2) > 3
        # The Yard is on the right. Changing its owner must not repaint the Mammoth.
        assert changed[350:520, 560:720].sum() == 0
        assert changed[380:425, 1220:1260].sum() == 0, 'The metal roof must not recolour'
        assert changed[400:585, 990:1260].sum() > 100
        return dict(switches=True, deterministic=True, classic_fallback=True,
                    damaged_fallback=True, selling_fallback=True, two_player_colours=True,
                    changed_player_pixels=int(changed.sum()), engine_triangles=19810)
    finally:
        mission.write_bytes(original)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path)
    args = parser.parse_args()
    report = {'asset': asset_gate()}
    if args.runtime:
        report['engine'] = engine_gate(args.runtime)
        (args.runtime / 'shots/yard/results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
