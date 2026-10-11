#!/usr/bin/env python3
"""Bake original Red Alert snow, temperate and interior icons for Classic mode."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
from ramix import RaMix


def icons(data: bytes, name: str) -> dict[int, bytes]:
    """Decode IconControlType from the original 24 by 24 theater file."""
    width, height, count, _allocated, _mw, _mh, size, image_offset, \
        _pal, _rem, _trans, _color_map, map_offset = struct.unpack_from("<6h7i", data)
    if (width, height, size, image_offset) != (24, 24, len(data), 0x28):
        raise ValueError(f"{name}: unexpected theater template header")
    result = {}
    for icon, image in enumerate(data[map_offset:map_offset + count]):
        if image == 0xFF:
            continue
        pixels = data[image_offset + image * 576:image_offset + (image + 1) * 576]
        if len(pixels) != 576:
            raise ValueError(f"{name}: icon {icon} is truncated")
        result[icon] = pixels
    return result


def bake(data: Path, output: Path) -> None:
    source = (ROOT / "brain" / "vanilla" / "redalert" / "cdata.cpp").read_text(
        errors="ignore")
    names = sorted(set(re.findall(r'"([A-Z0-9_-]{2,8})"', source)))
    local = RaMix(data / "LOCAL.MIX")
    records = []
    palettes = []
    for theater, archive, extension, palname in (
        (b"S", "SNOW.MIX", "SNO", "SNOW.PAL"),
        (b"T", "TEMPERAT.MIX", "TEM", "TEMPERAT.PAL"),
        (b"I", "INTERIOR.MIX", "INT", "INTERIOR.PAL"),
    ):
        palette = local.read(palname)
        if palette is None or len(palette) != 768:
            raise ValueError(f"missing original palette {palname}")
        palettes.append(bytes((v << 2) | (v >> 4) for v in palette))
        mix = RaMix(data / archive)
        for name in names:
            original = mix.read(f"{name}.{extension}")
            if original is None:
                continue
            try:
                tile_set = icons(original, name)
            except (ValueError, struct.error) as error:
                print(f"skip {name}.{extension}: {error}", file=sys.stderr)
                continue
            for icon, pixels in tile_set.items():
                records.append((theater, name, icon, pixels))
    if not any(theater == b"S" and name == "CLEAR1" for theater, name, _, _ in records):
        raise ValueError("snow clear terrain is missing")
    if not any(theater == b"I" and name == "CLEAR1" for theater, name, _, _ in records):
        raise ValueError("interior clear terrain is missing")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        stream.write(b"RA2DT002")
        stream.write(struct.pack("<I", len(records)))
        for palette in palettes:
            stream.write(palette)
        for theater, name, icon, pixels in records:
            stream.write(struct.pack("<c16sH", theater, name.encode(), icon))
            stream.write(pixels)
    print(f"RA Classic terrain: {len(records)} original icons, "
          f"{output.stat().st_size} bytes in {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    bake(args.data, args.out)
