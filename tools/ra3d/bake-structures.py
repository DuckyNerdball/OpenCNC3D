#!/usr/bin/env python3
"""Export approved intact RA structures, preserving the lab and pending later stages.

Usage: tools/ra3d/bake-structures.py --export-root build-native/remaster-ra
The engine source supplies footprints; the RA lab supplies plot turns and textures.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / 'tools/art/remaster'
# Walls use the separate connection-mask path, not a building mesh.
TYPES = 'AFLD AGUN APWR ATEK BARR DOME FACT FCOM FIX FTUR GAP GUN HBOX HPAD IRON KENN MSLO PBOX PDOX POWR PROC SAM SILO SPEN STEK SYRD TENT TSLA WEAP'.split()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--export-root', type=Path, default=ROOT / 'build-native/remaster-ra')
    ap.add_argument('--reviews', type=Path, default=TOOLS / 'reviews-ra.json')
    ap.add_argument('--out', type=Path, default=ROOT / 'game/ebld_ra')
    ap.add_argument('types', nargs='*', choices=TYPES)
    args = ap.parse_args()
    reviews = json.loads(args.reviews.read_text())
    locked = json.loads((ROOT / 'tools/ra3d/structure-inputs.json').read_text())['models']
    sys.path[:0] = [str(TOOLS / 'ra'), str(TOOLS / 'infantry')]
    structs = load('ra_structure_source', TOOLS / 'ra/structs.py')
    source = structs.classes()
    baker = load('ra_structure_baker', TOOLS / 'bake_ebld.py')
    baker.FOOTPRINT = {k: tuple(v['size']) for k, v in source.items()}
    # The RA anti-aircraft gun is authored north-facing. SAM and GUN keep their
    # existing south/north rest directions. Aiming follows the engine, not lab spin.
    baker.TURRET_FORWARD = {'GUN': 0, 'SAM': 128, 'AGUN': 0}
    baker.YAW_FILE = str(ROOT / 'art/remaster-ra/yaw.json')
    meta = {v['type']: v for v in json.loads((ROOT / 'art/remaster-ra/structures.json').read_text())}
    manifest = {'game': 'Red Alert', 'stage': 'approved intact only', 'models': {}}
    for code in args.types or TYPES:
        review = reviews[code]
        if review.get('state') != 'approved' and review.get('phase') != 'stages':
            ap.error(f'{code}: intact model is still awaiting review')
        view = args.export_root / code / 'viewer'
        approved = locked[code]
        for name, digest in approved['sources'].items():
            if hashlib.sha256((view / name).read_bytes()).hexdigest() != digest:
                ap.error(f'{code}: {name} differs from the approved intact export')
        if baker.plot_yaw(code) != approved['yaw'] or list(baker.FOOTPRINT[code]) != approved['footprint']:
            ap.error(f'{code}: its approved plot turn or footprint changed')
        rm = json.loads((view / 'remaster.json').read_text())
        if rm['triangles'] > 10000:
            ap.error(f'{code}: {rm["triangles"]} triangles exceeds the structure budget')
        if tuple(meta[code]['cells']) != baker.FOOTPRINT[code]:
            ap.error(f'{code}: recorded plot disagrees with the current engine footprint')
        info = baker.bake(code, str(view), str(args.out), str(args.export_root), intact_only=True)
        info['export'] = f'build-native/remaster-ra/{code}/viewer'
        info['footprint_source'] = 'brain/vanilla/redalert/bdata.cpp'
        info['approval'] = {k: review[k] for k in ('state', 'phase', 'at', 'remaster_approved') if k in review}
        # Detect concurrent revisions during the bake, including every sampled texture.
        for name, digest in info['sources'].items():
            if hashlib.sha256((view / name).read_bytes()).hexdigest() != digest:
                raise RuntimeError(f'{code}: {name} changed while exporting; review that version first')
        manifest['models'][code] = info
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n')
    print(f'RA structures: {len(manifest["models"])} intact exports verified; later stages excluded')


if __name__ == '__main__':
    main()
