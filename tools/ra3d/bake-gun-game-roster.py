#!/usr/bin/env python3
"""Build the private Dawn Gun Game supplement from an installed Red Alert module."""
import argparse
import struct
import sys
import zlib
from pathlib import Path

from ramix import RaMix

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "menu/tools"))
from mixlib import mix_id
from mixshp import Shape

# TD IDs are distinct where the original INI identifiers would collide.
UNITS = {
    "RTANY": "E7", "SHOK": "SHOK", "3TNK": "3TNK",
    "V2RL": "V2RL", "TTNK": "TTNK", "CTNK": "CTNK",
    "HIND": "HIND", "YAK": "YAK", "MIG": "MIG",
}


def entry(archives, name):
    for archive in archives:
        data = archive.read(name)
        if data is not None:
            return data
    raise ValueError(f"Red Alert asset is missing: {name}")


def write_mix(path, records):
    data = bytearray()
    index = bytearray()
    # Dawn searches the archive index using signed 32-bit comparison.
    for key, contents in sorted(records.items(), key=lambda row: mix_id(row[0]) ^ 0x80000000):
        index.extend(struct.pack("<III", mix_id(key), len(data), len(contents)))
        data.extend(contents)
    path.write_bytes(struct.pack("<HI", len(records), len(data)) + index + data)


def write_cameo(output, key, raw, palette):
    icon = Shape(raw)
    width, height = icon.width, icon.height
    pot_w = 1 << (width - 1).bit_length()
    pot_h = 1 << (height - 1).bit_length()
    rgba = bytearray(pot_w * pot_h * 4)
    for pixel, index in enumerate(icon.frame(0)):
        if index:
            x, y = pixel % width, pixel // width
            offset = (y * pot_w + x) * 4
            rgba[offset:offset + 4] = palette[index * 3:index * 3 + 3] + b"\xff"
    output.extend(struct.pack("<8sIIII", key.encode(), pot_w, pot_h, width, height) + rgba)


def bake(module, out):
    data = module / "data"
    local = RaMix(data / "LOCAL.MIX")
    body = [RaMix(data / name) for name in ("EXPAND2.MIX", "LORES1.MIX", "LORES.MIX", "CONQUER.MIX")
            if (data / name).is_file()]
    high = [RaMix(data / name) for name in ("HIRES1.MIX", "HIRES.MIX") if (data / name).is_file()]
    native = (module / "dosclassic.pack").read_bytes()[:28]
    if native[:8] != b"DOS2D005":
        raise ValueError("installed Red Alert sprite palette is missing")
    palette = bytes(min(255, value * 4) for value in entry([local], "TEMPERAT.PAL"))
    mix = {}
    records = bytearray()
    used_colours = set()
    cameos = bytearray(struct.pack("<8sII", b"CNC3DCAM", 1, len(UNITS)))
    for key, art in UNITS.items():
        source = entry(body, art + ".SHP")
        mix[key + ".SHP"] = source
        decoded = Shape(source)
        pixels = b"".join(decoded.frame(i) for i in range(decoded.frames))
        used_colours.update(pixels)
        frames = zlib.compress(pixels, 9)
        records.extend(struct.pack("<16sHHII", key.encode(), decoded.width, decoded.height,
                                  decoded.frames, len(frames)) + frames)
        try:
            icon = entry(high, art + "ICON.SHP")
        except ValueError:
            icon = entry(body, art + "ICON.SHP")
        write_cameo(cameos, key, icon, palette)
        print(f"{key:5} {art:5} {decoded.width}x{decoded.height} {decoded.frames} frames")
    out.mkdir(parents=True, exist_ok=True)
    write_mix(out / "GUNRA.MIX", mix)
    # Carry only the colour values used by these sprites. No theater palette is installed.
    swatches = b"".join(palette[i * 3:i * 3 + 3] if i in used_colours else b"\x00" * 3
                        for i in range(256))
    header = struct.pack("<8sI", b"GUNRA001", len(UNITS)) + native[12:28] + swatches
    (out / "gunra-sprites.pack").write_bytes(header + records)
    (out / "gunra-cameos.pack").write_bytes(cameos)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    bake(args.module, args.out)
