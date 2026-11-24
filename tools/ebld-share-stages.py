#!/usr/bin/env python3
"""Keep each tree stage that is byte-identical to its temperate stage once, in a package.

Usage: tools/ebld-share-stages.py EBLD_DIR

A tree's burning and snag stages are files of their own beside its file
(T01_WIN_BURN.ebld, T01_WIN_SNAG.ebld). Many winter stages are the same bytes as the
temperate ones, and a download carries both. This removes every
T<nn>_<THEATER>_<BURN|SNAG>.ebld whose bytes, and whose .spec when it has one, equal
T<nn>_TEM_<stage>.ebld's, and names it in EBLD_DIR/shared-stages.txt as
"<removed name> <temperate name>". The renderer (ebld_stage_path in game/ebld_mod.h)
reads that list and loads the temperate file for every name it holds. The list decides,
never the presence of a file, because a launcher update does not delete files an older
version installed. Lines already in the list are kept.

Run on a package's folder, never on the tree: game/ebld keeps every file.
"""
import hashlib
import re
import sys
from pathlib import Path

LIST = 'shared-stages.txt'
STAGE = re.compile(r'^(T\d\d)_([A-Z]{3})_(BURN|SNAG)\.ebld$')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def share(folder):
    folder = Path(folder)
    if not folder.is_dir():
        raise SystemExit(f'ebld-share-stages: {folder} is not a folder')
    listing = folder / LIST
    pairs = {}
    if listing.is_file():
        for line in listing.read_text().splitlines():
            parts = line.split()
            if len(parts) == 2:
                pairs[parts[0]] = parts[1]
    saved = 0
    for f in sorted(folder.glob('T*_*_*.ebld')):
        m = STAGE.match(f.name)
        if not m or m.group(2) == 'TEM':
            continue
        keep = folder / f'{m.group(1)}_TEM_{m.group(3)}.ebld'
        if not keep.is_file() or digest(f) != digest(keep):
            continue
        spec, keep_spec = f.with_suffix('.spec'), keep.with_suffix('.spec')
        if digest(spec) != digest(keep_spec):
            continue
        saved += f.stat().st_size
        f.unlink()
        if spec.is_file():
            saved += spec.stat().st_size
            spec.unlink()
        pairs[f.name] = keep.name
    if pairs:
        listing.write_text(''.join(f'{a} {b}\n' for a, b in sorted(pairs.items())))
    print(f'ebld-share-stages: {len(pairs)} tree stages read from their temperate file '
          f'({saved / 1e6:.1f} MB not stored twice) in {folder}')
    return pairs


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    share(sys.argv[1])
