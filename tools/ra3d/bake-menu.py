#!/usr/bin/env python3
"""Bake the original Red Alert title palette, pointer and fonts into a menu pack."""

from __future__ import annotations

import argparse
from io import BytesIO
from pathlib import Path
import struct
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "menu"))
from bake_dosmenu import bake_font, bake_mouse, load_cps, pack_name, widen  # noqa: E402
from ramix import RaMix  # noqa: E402

class RedAlertMixes:
    """Find each menu asset in the archive used by the 1996 low resolution UI."""

    SOURCES = {
        "MOUSE.SHP": "LORES.MIX",
        "6POINT.FNT": "LORES.MIX",
        "GRAD6FNT.FNT": "LORES.MIX",
        "8POINT.FNT": "LOCAL.MIX",
        "3POINT.FNT": "LOCAL.MIX",
        "SCOREFNT.FNT": "LORES.MIX",
        "SCOREHI.FNT": "HIRES.MIX",
        "6PTHI.FNT": "HIRES.MIX",
        "GRAD6HI.FNT": "HIRES.MIX",
    }

    def __init__(self, data: Path):
        self.archives = {name: RaMix(data / name) for name in set(self.SOURCES.values())}

    def read(self, name: str) -> bytes:
        archive = self.SOURCES[name]
        source_name = {
            "SCOREHI.FNT": "SCOREFNT.FNT",
            "6PTHI.FNT": "6POINT.FNT",
            "GRAD6HI.FNT": "GRAD6FNT.FNT",
        }.get(name, name)
        result = self.archives[archive].read(source_name)
        if result is None:
            raise ValueError(f"{archive} has no {name}")
        return result


DEFAULT_TITLE = ROOT / "menu" / "art" / "ra3d-title.png"
DEFAULT_TITLE_HI = ROOT / "menu" / "art" / "ra3d-title-win95.png"


def indexed_title(image_path: Path, palette: bytes, size: tuple[int, int]) -> tuple[bytes, float]:
    """Put title artwork in the original Windows 95 indexed palette."""
    image = Image.open(image_path).convert("RGB")
    if image.size != size:
        raise ValueError(f"{image_path} must be exactly {size[0]}x{size[1]} pixels")
    rgb = image.tobytes()
    pal8 = widen(palette)
    colors = [tuple(pal8[3 * n + channel] for channel in range(3))
              for n in range(256)]
    cache: dict[tuple[int, int, int], tuple[int, int]] = {}
    output = bytearray(size[0] * size[1])
    error = 0
    for i in range(len(output)):
        color = (rgb[3 * i], rgb[3 * i + 1], rgb[3 * i + 2])
        if color not in cache:
            differences = [sum((color[channel] - candidate[channel]) ** 2
                               for channel in range(3)) for candidate in colors]
            index = min(range(256), key=differences.__getitem__)
            cache[color] = index, differences[index]
        index, distance = cache[color]
        output[i] = index
        error += distance
    return bytes(output), error / len(output)


def bake(ra_data: Path, output_path: Path, image_path: Path = DEFAULT_TITLE_HI) -> None:
    title_bytes = RaMix(ra_data / "GENERAL.MIX").read("TITLE.CPS")
    title_hi_bytes = RaMix(ra_data / "HIRES.MIX").read("TITLE.PCX")
    palette_bytes = RaMix(ra_data / "LOCAL.MIX").read("PALETTE.CPS")
    if title_bytes is None or title_hi_bytes is None or palette_bytes is None:
        raise ValueError("Red Alert title or color-remap picture is missing")
    pixels, palette_dos = load_cps(title_bytes)
    pcx = Image.open(BytesIO(title_hi_bytes))
    if pcx.size != (640, 400) or pcx.mode != "P":
        raise ValueError("HIRES.MIX TITLE.PCX is not the 640x400 indexed title")
    palette = bytes(v >> 2 for v in pcx.getpalette()[:768])
    remaps, _ = load_cps(palette_bytes)
    if len(pixels) != 320 * 200 or palette_dos is None or len(palette) != 768:
        raise ValueError("TITLE.CPS needs a 320x200 image and a 256-color palette")
    if len(remaps) != 320 * 200 or max(palette) > 63:
        raise ValueError("PALETTE.CPS or the six-bit title palette is invalid")
    pixels, _ = indexed_title(DEFAULT_TITLE, palette, (320, 200))
    pixels_hi, title_error = indexed_title(image_path, palette, (640, 400))

    # Init_Color_Remaps in Red Alert's init.cpp reads row PCOLOR_DIALOG_BLUE (10)
    # from PALETTE.CPS. Its menu buttons use those indices against TITLE.CPS.
    scheme = [remaps[10 * 320 + col] for col in range(16)]
    if scheme[4] == scheme[9] or scheme[9] == scheme[10]:
        raise ValueError("unexpected Red Alert dialog color remap")

    mixes = RedAlertMixes(ra_data)
    shapes = [("TITLE", 1, 320, 200, pixels),
              ("TITLEHI", 1, 640, 400, pixels_hi),
              ("MOUSE",) + bake_mouse(mixes, 0)]
    fonts = [(stem,) + bake_font(mixes, stem)
             for stem in ("6POINT", "GRAD6FNT", "8POINT", "3POINT", "SCOREFNT",
                          "SCOREHI", "6PTHI", "GRAD6HI")]

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("wb") as out:
        out.write(b"DOSBAR01")
        out.write(struct.pack("<IIII", 1, len(shapes), len(fonts), 256))
        out.write(palette)
        out.write(widen(palette))
        out.write(b"\0" * 512)  # the sidebar clock table is unused in menus
        for name, frames, width, height, shape_pixels in shapes:
            out.write(pack_name(name, 12))
            out.write(struct.pack("<III", frames, width, height))
            out.write(shape_pixels)
        for name, count, width, height, widths, ytop, rows, cells in fonts:
            out.write(pack_name(name, 8))
            out.write(struct.pack("<III", count, width, height))
            out.write(widths)
            out.write(ytop)
            out.write(rows)
            out.write(cells)
    print(f"Red Alert title {image_path} -> {output_path}; "
          f"palette mean RGB squared error {title_error:.2f}; "
          f"dialog remap row 10: {scheme}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--title", type=Path, default=DEFAULT_TITLE_HI)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    bake(args.data, args.out, args.title)


if __name__ == "__main__":
    main()
