#!/usr/bin/env python3
"""Bake the original Red Alert DOS or Windows 95 sidebar for each theater.

The pack uses DOSBAR01's indexed shape/font container. Every pixel comes from
the 1996 LORES.MIX or HIRES.MIX and the theater palette in LOCAL.MIX.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "menu" / "tools"), str(ROOT / "game")]
from mixshp import Shape  # noqa: E402
from bake_dossidebar import bake_font, pack_name, widen, build_fading_table  # noqa: E402
from ramix import RaMix  # noqa: E402


class FontSource:
    def __init__(self, lores: RaMix, local: RaMix):
        self.lores = lores
        self.local = local

    def read(self, name: str) -> bytes:
        value = self.lores.read(name) or self.local.read(name)
        if value is None:
            raise ValueError(f"missing original font {name}")
        return value


def shape(lores: RaMix, name: str) -> tuple[str, int, int, int, bytes]:
    raw = lores.read(name + ".SHP")
    if raw is None:
        raise ValueError(f"missing original shape {name}.SHP")
    decoded = Shape(raw)
    frames = b"".join(decoded.frame(i) for i in range(decoded.frames))
    if len(frames) != decoded.frames * decoded.width * decoded.height:
        raise ValueError(f"bad shape dimensions for {name}")
    return name, decoded.frames, decoded.width, decoded.height, frames


CHROME = ("SIDE1NA", "SIDE2NA", "SIDE3NA", "SIDE1US", "SIDE2US", "SIDE3US",
          "STRIPNA", "STRIPUS", "REPAIR", "SELL", "MAP", "STRIPUP", "STRIPDN",
          "CLOCK", "PIPS", "TABS", "POWER", "POWERBAR", "NATORADR", "USSRRADR")
SPECIAL_CAMEOS = ("SONR", "ATOM", "WARP", "PBMB", "PINF", "CAM", "INFX", "GPSS")


def bake(data: Path, theater: str, out: Path, hires: bool = False) -> None:
    art = RaMix(data / ("HIRES.MIX" if hires else "LORES.MIX"))
    local = RaMix(data / "LOCAL.MIX")
    palette = local.read(theater + ".PAL")
    rules = local.read("RULES.INI")
    if palette is None or len(palette) != 768 or max(palette) > 63 or rules is None:
        raise ValueError(f"missing original {theater} palette or RULES.INI")

    # RULES.INI is the base game's object registry. Include every section that has an
    # actual cameo, with no invented names or TD art fallback.
    stems = sorted(set(re.findall(rb"^\[([A-Za-z0-9_]+)\]", rules, re.M)))
    cameos = sorted(set(stem.decode("ascii") for stem in stems
                         if art.read(stem.decode("ascii") + "ICON.SHP") is not None)
                    | set(SPECIAL_CAMEOS))
    shapes = [shape(art, stem + "ICON") for stem in cameos]
    shapes = [(stem, n, w, h, pixels) for stem, (_, n, w, h, pixels) in zip(cameos, shapes)]
    shapes.extend(shape(art, stem) for stem in CHROME)
    fonts = [(name,) + bake_font(FontSource(art, local), name)
             for name in ("6POINT", "GRAD6FNT", "8POINT")]

    # RA sidebar.cpp:1223 uses {GREEN, BLACK, 100, 0}; its ghost table maps the
    # green clock ink to a palette fade toward black, while 0xFF means opaque.
    cycling = bytearray(palette)
    cycling[32 * 3:39 * 3] = b"\x3f" * (7 * 3)
    ghost = bytearray(b"\xff" * 256)
    ghost[3] = 0
    clock = bytes(ghost) + bytes(build_fading_table(cycling, 12, 100))

    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("wb") as stream:
        stream.write(b"DOSBAR01")
        stream.write(struct.pack("<IIII", 1, len(shapes), len(fonts), 256))
        stream.write(palette)
        stream.write(widen(palette))
        stream.write(clock)
        for name, count, width, height, pixels in shapes:
            stream.write(pack_name(name, 12))
            stream.write(struct.pack("<III", count, width, height))
            stream.write(pixels)
        for name, count, width, height, widths, ytop, rows, cells in fonts:
            stream.write(pack_name(name, 8))
            stream.write(struct.pack("<III", count, width, height))
            stream.write(widths)
            stream.write(ytop)
            stream.write(rows)
            stream.write(cells)
    print(f"RA {'Win95' if hires else 'DOS'} sidebar {theater}: {len(cameos)} original cameos, "
          f"{len(CHROME)} chrome shapes, {out.stat().st_size} bytes in {out}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--theater", required=True,
                        choices=("SNOW", "TEMPERAT", "INTERIOR"))
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--hires", action="store_true",
                        help="use original Windows 95 shapes and fonts from HIRES.MIX")
    args = parser.parse_args()
    bake(args.data, args.theater, args.out, args.hires)
