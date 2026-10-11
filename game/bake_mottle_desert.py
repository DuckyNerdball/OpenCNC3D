#!/usr/bin/env python3
"""Build Desert soil/gravel patches from the installed source-art pack.

Reuses the licensed dirt/gravel images and masks in mottle.pack. Grass, roots,
moss and leaf materials stay Temperate. Match 60% toward Enhanced Desert CLEAR1
using the same measured mean/spread process as the original mottle baker.
No source texture is enlarged and each material retains its own mip chain.
"""
import argparse
import struct
import zlib
from pathlib import Path

import numpy as np
from PIL import Image
from bake_mottle import chain, toward_ground

ROOT = Path(__file__).resolve().parents[1]
MATERIALS = {'GDM_12_Dirt', 'GDM_13_Dirt', 'GDM_14_Dirt',
             'GDM_18_Gravel_Ground', 'GDM_19_Gravel_and_Stones'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, default=ROOT/'data/mottle/mottle.pack')
    p.add_argument('--ground', type=Path,
                   default=ROOT/'art/remaster/textures/DESERT/CLEAR1/final_144.png')
    p.add_argument('--out', type=Path, default=ROOT/'data/mottle/mottle_desert.pack')
    a = p.parse_args()
    ground = np.asarray(Image.open(a.ground).convert('RGB')).astype(np.float32)
    ref = ground.mean((0, 1)), ground.std((0, 1))
    data = a.source.read_bytes()
    assert data[:8] == b'CNC3DMOT' and struct.unpack_from('<I', data, 8)[0] == 1
    count, at = struct.unpack_from('<I', data, 12)[0], 16
    selected = {}
    for _ in range(count):
        n = struct.unpack_from('<I', data, at)[0]; at += 4
        name = data[at:at+n].decode('ascii'); at += n
        levels = struct.unpack_from('<I', data, at)[0]; at += 4
        for level in range(levels):
            w, h, raw, comp = struct.unpack_from('<4I', data, at); at += 16
            payload = data[at:at+comp]; at += comp
            if name in MATERIALS and level == 0:
                rgba = zlib.decompress(payload)
                assert len(rgba) == raw == w*h*4
                selected[name] = Image.frombytes('RGBA', (w, h), rgba)
    assert at == len(data) and set(selected) == MATERIALS
    out = bytearray(b'CNC3DMOT' + struct.pack('<II', 1, len(selected)))
    for name, original in sorted(selected.items()):
        img = toward_ground(original, ref, 0.60)
        assert img.getchannel('A').tobytes() == original.getchannel('A').tobytes()
        levels = chain(img)
        n = name.encode('ascii')
        out += struct.pack('<I', len(n)) + n + struct.pack('<I', len(levels))
        for level in levels:
            raw = level.tobytes(); comp = zlib.compress(raw, 9)
            out += struct.pack('<4I', level.width, level.height, len(raw), len(comp)) + comp
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_bytes(out)
    print(f'{a.out.name}: {len(selected)} soil/gravel patches, {len(out)} bytes')


if __name__ == '__main__':
    main()
