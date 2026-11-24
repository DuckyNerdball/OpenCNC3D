#!/usr/bin/env python3
"""Stage Gun Game's Red Alert supplement into a play folder, from its staged Red Alert module.

Usage: tools/ra3d/stage-gun-game.py --module PLAY/ra3d --out PLAY

Tiberian Dawn's Gun Game ladder borrows Red Alert units (gun_game.cpp's Ladder). A match
refuses to start without three roster files beside the program (cnc_eyes.cpp checks them at
boot), and under Enhanced it draws the borrowed vehicles and aircraft from a folder of their
own. This script writes all four into PLAY:

  evhc/gunra/GUNRA.MIX, gunra-sprites.pack, gunra-cameos.pack
        the 1996 sprites and cameos of the borrowed units, baked from the module's own data
        by bake-gun-game-roster.py;
  evhc/gunra/<TYPE>.evhc and .spec
        the remastered models, one per type evhc_load_gun_ra (game/evhc_mod.h) reads. The
        list is read from that function, so the engine and this script cannot disagree.
        Each file is copied byte for byte from the module's own evhc/ folder, where
        stage-vehicles.py has already put the Red Alert lab's approved files from
        game/evhc_ra: the same model and the same approval rule as Red Alert 3D, so the two
        games never draw different versions of one vehicle. A type the module does not
        carry is left out and named; Gun Game then draws its 1996 sprite.

WHY evhc/gunra/. The launcher installs a module only from a fixed list of top-level names
and folders, so an update can never write over player data, and every launcher already in
players' hands carries its own copy of that list. A top-level GUNRA.MIX or a gunra-evhc/
folder is on none of them, and v0.9.8's Tiberian Dawn module was refused whole for it.
evhc/ is on all of them. The engine (init.cpp) and the renderer look there first.

The evhc/gunra folder is REPLACED, so a type withdrawn from the list or from the module is
not left behind, and a top-level GUNRA.MIX or gunra-evhc/ from the older layout is removed.
Writes evhc/gunra/manifest.json: each type staged with its SHA-256.
"""
import argparse
import hashlib
import json
import re
import shutil
import tempfile
import sys
from importlib import import_module
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
EVHC_MOD = ROOT / 'game/evhc_mod.h'
FOLDER = 'evhc/gunra'
OLD_FOLDER = 'gunra-evhc'
ROSTER_MIX = 'GUNRA.MIX'


def gun_types():
    """The types evhc_load_gun_ra reads, from its own source."""
    src = EVHC_MOD.read_text(encoding='utf-8', errors='replace')
    body = re.search(r'static void evhc_load_gun_ra\(.*?\n}\n', src, re.S)
    if not body:
        raise RuntimeError(f'{EVHC_MOD.relative_to(ROOT)} has no evhc_load_gun_ra')
    line = re.search(r'types\[\]\s*=\s*\{([^}]*)\}', body.group(0))
    if not line:
        raise RuntimeError('evhc_load_gun_ra has no types[] list')
    types = re.findall(r'"([A-Z0-9]{2,8})"', line.group(1))
    if not types:
        raise RuntimeError('evhc_load_gun_ra lists no types')
    return types


def stage_models(module, out):
    src_dir = module / 'evhc'
    dst = out / FOLDER
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True)
    staged, left = {}, []
    for t in gun_types():
        src = src_dir / f'{t}.evhc'
        if not src.is_file():
            left.append(t)
            continue
        shutil.copy2(src, dst / src.name)
        spec = src.with_suffix('.spec')
        if spec.is_file():
            shutil.copy2(spec, dst / spec.name)
        staged[t] = {'from': f'ra3d/evhc/{src.name}',
                     'sha256': hashlib.sha256(src.read_bytes()).hexdigest(),
                     'spec': spec.is_file()}
    (dst / 'manifest.json').write_text(json.dumps(
        {'what': "the Red Alert models Tiberian Dawn's Gun Game draws under Enhanced",
         'types': staged, 'left_out': left}, indent=1) + '\n')
    return staged, left


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--module', type=Path, help='the staged ra3d/ folder')
    src.add_argument('--roster-source', type=Path,
                     help='a Red Alert module folder to bake the roster from, for a package '
                          'that does not carry the module: the models are then staged from '
                          "the tree's approved files exactly as stage-vehicles.py stages "
                          "Red Alert 3D's, so both games draw the same file")
    ap.add_argument('--out', type=Path, required=True, help='the play folder')
    a = ap.parse_args()
    base = a.module or a.roster_source
    if not (base / 'data').is_dir():
        sys.exit(f'stage-gun-game: {base} is not a staged Red Alert module (no data/)')
    sys.path.insert(0, str(HERE))
    roster = import_module('bake-gun-game-roster')
    try:
        roster.bake(base, a.out)
        if (a.out / OLD_FOLDER).exists():
            shutil.rmtree(a.out / OLD_FOLDER)
        if a.module:
            staged, left = stage_models(a.module, a.out)
        else:
            with tempfile.TemporaryDirectory() as tmp:
                models = Path(tmp) / 'ra3d'
                import_module('stage-vehicles').stage(models, quiet=True)
                staged, left = stage_models(models, a.out)
        # the roster beside the models (the bake writes it at the top of PLAY)
        (a.out / ROSTER_MIX).replace(a.out / FOLDER / ROSTER_MIX)
    except (RuntimeError, ValueError, OSError) as e:
        sys.exit(f'stage-gun-game: {e}')
    print(f'Gun Game Red Alert models staged in {a.out / FOLDER}: {", ".join(staged) or "none"}')
    # A type left out is not an error (its row in reviews-ra.json may have been withdrawn, and
    # Gun Game then draws its 1996 sprite, as Red Alert 3D does), but it is said on stderr so
    # a build that discards this script's ordinary output still shows it.
    if left:
        print(f'stage-gun-game: WARNING: no model for {", ".join(left)}; '
              'Gun Game draws the 1996 sprite for these', file=sys.stderr)


if __name__ == '__main__':
    main()
