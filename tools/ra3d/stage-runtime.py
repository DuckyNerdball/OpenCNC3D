#!/usr/bin/env python3
"""Stage an existing RA module's media with binaries for the target platform."""
import argparse
from pathlib import Path
import shutil
from importlib import import_module


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--app', type=Path, required=True)
    ap.add_argument('--brain', type=Path, required=True)
    args = ap.parse_args()
    dll = args.app.suffix == '.exe'
    if args.brain.name != ('RedAlert.dll' if dll else 'RedAlert.dylib'):
        ap.error('app and Red Alert brain must target the same platform')
    for path in (args.app, args.brain, args.source / 'data/GENERAL.MIX', args.source / 'dosmenu.pack'):
        if not path.is_file():
            ap.error(f'missing runtime input: {path}')
    args.out.mkdir(parents=True, exist_ok=True)
    for name in ('data', 'movies', 'audio', 'missions', 'einf', '3d'):
        src = args.source / name
        if src.is_dir():
            shutil.copytree(src, args.out / name, dirs_exist_ok=True)
    for pack in args.source.glob('*.pack'):
        shutil.copy2(pack, args.out / pack.name)
    for binary in (args.app, args.brain):
        shutil.copy2(binary, args.out / binary.name)
    libraries = ('SDL2.dll',) if dll else ('libSDL2-2.0.0.dylib', 'libSDL3.dylib')
    for name in libraries:
        shutil.copy2(args.app.parent / name, args.out / name)
    import_module('stage-structures').stage(args.out)
    print(f'RA runtime staged in {args.out}')


if __name__ == '__main__':
    main()
