#!/usr/bin/env python3
"""Add missing original DOS shape members to a playable MIX archive.

Existing playable entries are copied byte for byte. This archive is used by the
engine to resolve Draw_It sprites, including in the layer export for DOS Classic.
Theater shapes use .TEM or .DES names; shared sprites use .SHP.
"""
import argparse
import pathlib
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(REPO / 'menu' / 'tools'))
from mixshp import MixFile, mix_id
from bake_dosclassic import names_from_engine


def merge(base_path, original_path, out_path, extension="SHP"):
    base = MixFile(str(base_path))
    original = MixFile(str(original_path))
    members = {key: base.read_id(key) for key in base.index}
    added = []
    for name in names_from_engine():
        key = mix_id(name + '.' + extension)
        if key in original.index and key not in members:
            members[key] = original.read_id(key)
            added.append(name)
    if not added:
        print('DOS2D|archive already contains all known %s shapes' % extension)
    if len(members) > 65535:
        raise ValueError('too many MIX members')
    keys = sorted(members, key=lambda k: k if k < 0x80000000 else k - 0x100000000)
    data_size = sum(len(members[k]) for k in keys)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open('wb') as out:
        out.write(struct.pack('<HI', len(keys), data_size))
        offset = 0
        for key in keys:
            blob = members[key]
            out.write(struct.pack('<III', key, offset, len(blob)))
            offset += len(blob)
        for key in keys:
            out.write(members[key])
    check = MixFile(str(out_path))
    for key in base.index:
        if check.read_id(key) != base.read_id(key):
            raise AssertionError('playable entry changed: %08x' % key)
    print('DOS2D|archive ext=%s base=%d added=%d merged=%d bytes=%d' %
          (extension, len(base.index), len(added), len(check.index), out_path.stat().st_size))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('base', type=pathlib.Path)
    ap.add_argument('original', type=pathlib.Path)
    ap.add_argument('output', type=pathlib.Path)
    ap.add_argument('--ext', default='SHP', choices=('SHP', 'TEM', 'DES'))
    a = ap.parse_args()
    merge(a.base, a.original, a.output, a.ext)
