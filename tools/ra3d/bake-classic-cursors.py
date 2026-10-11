#!/usr/bin/env python3
"""Bake Red Alert's original MOUSE.SHP into the Classic cursor pack format."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "menu" / "tools"))
sys.path.insert(0, str(ROOT / "game"))
from mixshp import ShapeBlock  # noqa: E402
from bake_dossidebar import pack_name, widen  # noqa: E402
from ramix import RaMix  # noqa: E402


def bake(data: Path, out: Path, theater: str) -> None:
    local = RaMix(data / "LOCAL.MIX")
    palette = local.read(theater.upper() + ".PAL")
    mouse = RaMix(data / "LORES.MIX").read("MOUSE.SHP")
    if palette is None or len(palette) != 768 or mouse is None:
        raise ValueError(f"original {theater} palette or MOUSE.SHP is missing")
    shape = ShapeBlock(mouse)
    if shape.count != 222:
        raise ValueError(f"expected 222 Red Alert cursor frames, got {shape.count}")
    frames = bytearray()
    for index in range(shape.count):
        pixels, header = shape.frame(index)
        if (header["width"], header["height"]) != (30, 24) or len(pixels) != 720:
            raise ValueError(f"cursor frame {index} has unexpected dimensions")
        frames.extend(pixels)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("wb") as stream:
        stream.write(b"DOSBAR01")
        stream.write(struct.pack("<IIII", 1, 1, 0, 256))
        stream.write(palette)
        stream.write(widen(palette))
        stream.write(b"\xff" * 512)  # cursor-only pack; no build clock
        stream.write(pack_name("MOUSE", 12))
        stream.write(struct.pack("<III", shape.count, 30, 24))
        stream.write(frames)
    print(f"RA Classic {theater} cursors: 222 original frames in {out}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--theater", required=True,
                        choices=("SNOW", "TEMPERAT", "INTERIOR"))
    args = parser.parse_args()
    bake(args.data, args.out, args.theater)
