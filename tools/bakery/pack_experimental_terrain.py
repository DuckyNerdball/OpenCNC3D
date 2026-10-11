#!/usr/bin/env python3
"""Build 256px cells from committed original art, then pack F5 runtime bytes."""

from pathlib import Path
import csv
import struct
from PIL import Image
from make_temperate_256 import main as make_temperate_256
from make_temperate_shores_256 import main as make_temperate_shores_256, visible_slots
from make_temperate_bridges_256 import main as make_temperate_bridges_256
from make_temperate_rivers_256 import main as make_temperate_rivers_256

ROOT = Path(__file__).resolve().parents[2]
GRASS = ROOT / "art/generated/temperate_grass/v3_color_matched"
CLIFF = ROOT / "art/generated/temperate_cliffs/approved_style_v1"
ROAD = ROOT / "art/generated/temperate_roads/approved_style_v1"
SHORE = ROOT / "art/generated/temperate_shores/remastered_style_v1"
BRIDGE = ROOT / "art/generated/temperate_bridges/remastered_style_v1"
RIVER = ROOT / "art/generated/temperate_rivers/remastered_style_v1"
HIRES = ROOT / "art/generated/temperate_256"
CELL = 256
SLOTS = 38 * 6
HEADER = 12 + SLOTS * 4


def pixels(path: Path, mode: str) -> bytes:
    with Image.open(path) as image:
        if image.size != (CELL, CELL):
            raise ValueError(f"{path} is {image.size}, expected {CELL}x{CELL}")
        return image.convert(mode).tobytes()


def pack(magic: bytes, suffix: str, mode: str, output: Path, expected: int) -> None:
    offsets = [0] * SLOTS
    payload = bytearray()
    count = 0
    for template in range(1, 39):
        folder = HIRES / "cliffs" / f"S{template:02}"
        for shape in range(6):
            base = f"S{template:02}_{shape:02}"
            path = folder / f"{base}{suffix}.png"
            if not path.exists():
                continue
            offsets[(template - 1) * 6 + shape] = HEADER + len(payload)
            payload.extend(pixels(path, mode))
            count += 1
    if count != expected:
        raise ValueError(f"{output}: {count} cells, expected {expected}")
    output.write_bytes(magic + struct.pack("<I", 2)
                       + struct.pack(f"<{SLOTS}I", *offsets) + payload)
    print(f"{output.relative_to(ROOT)}: {count} cells")


def pack_roads(magic: bytes, suffix: str, mode: str, output: Path) -> None:
    slots = 43 * 12
    header = 12 + slots * 4
    offsets = [0] * slots
    payload = bytearray()
    count = 0
    for template in range(1, 44):
        for icon in range(12):
            path = HIRES / "roads" / f"D{template:02}" / f"D{template:02}_{icon:02}{suffix}.png"
            if not path.exists():
                continue
            offsets[(template-1)*12+icon] = header+len(payload)
            payload.extend(pixels(path, mode))
            count += 1
    if count != 206:
        raise ValueError(f"{output}: {count} road cells, expected 206")
    output.write_bytes(magic + struct.pack("<I", 2)
                       + struct.pack(f"<{slots}I", *offsets) + payload)
    print(f"{output.relative_to(ROOT)}: {count} cells")


def pack_shores(magic: bytes, suffix: str, mode: str, output: Path, expected: int) -> None:
    slots = 758
    header = 12 + slots * 4
    offsets = [0] * slots
    payload = bytearray()
    count = 0
    with (ROOT / "docs/temperate-texture-cell-status.csv").open(newline="") as f:
        rows = list(csv.DictReader(f))
    land_slots = visible_slots()
    for row in rows:
        if row["family"] != "shore" or int(row["bank_slot"]) not in land_slots:
            continue
        slot = int(row["bank_slot"])
        name = row["art_key"]
        path = HIRES / "shores" / name / f'{name}_{int(row["icon"]):02}{suffix}.png'
        if not path.exists():
            if suffix:
                continue  # only rock-bearing shore cells have relief channels
            raise FileNotFoundError(path)
        offsets[slot] = header + len(payload)
        payload.extend(pixels(path, mode))
        count += 1
    if count != expected:
        raise ValueError(f"{output}: {count} cells, expected {expected}")
    output.write_bytes(magic + struct.pack("<I", 2)
                       + struct.pack(f"<{slots}I", *offsets) + payload)
    print(f"{output.relative_to(ROOT)}: {count} cells")


def pack_bridges(magic: bytes, suffix: str, mode: str, output: Path) -> None:
    slots = 758
    header = 12 + slots * 4
    offsets = [0] * slots
    payload = bytearray()
    count = 0
    with (ROOT / "docs/temperate-texture-cell-status.csv").open(newline="") as f:
        rows = list(csv.DictReader(f))
    for row in rows:
        if row["family"] != "bridge":
            continue
        slot = int(row["bank_slot"])
        name = row["template"]
        stem = f'{name}_{int(row["icon"]):02}'
        path = HIRES / "bridges" / name / f"{stem}{suffix}.png"
        offsets[slot] = header + len(payload)
        payload.extend(pixels(path, mode))
        count += 1
    if count != 35:
        raise ValueError(f"{output}: {count} bridge cells, expected 35")
    output.write_bytes(magic + struct.pack("<I", 2)
                       + struct.pack(f"<{slots}I", *offsets) + payload)
    print(f"{output.relative_to(ROOT)}: {count} cells")


def pack_rivers(magic: bytes, suffix: str, mode: str, output: Path) -> None:
    slots = 758
    header = 12 + slots * 4
    offsets = [0] * slots
    payload = bytearray()
    count = 0
    with (ROOT / "docs/temperate-texture-cell-status.csv").open(newline="") as f:
        rows = list(csv.DictReader(f))
    for row in rows:
        if row["family"] != "river":
            continue
        slot = int(row["bank_slot"])
        name = row["art_key"]
        stem = f'{name}_{int(row["icon"]):02}'
        path = HIRES / "rivers" / name / f"{stem}{suffix}.png"
        offsets[slot] = header + len(payload)
        payload.extend(pixels(path, mode))
        count += 1
    if count != 115:
        raise ValueError(f"{output}: {count} river cells, expected 115")
    output.write_bytes(magic + struct.pack("<I", 2)
                       + struct.pack(f"<{slots}I", *offsets) + payload)
    print(f"{output.relative_to(ROOT)}: {count} cells")


def main() -> None:
    make_temperate_256()
    make_temperate_shores_256()
    make_temperate_bridges_256()
    make_temperate_rivers_256()
    grass = b"".join(pixels(HIRES / "grass" / f"CLEAR1_{i:02}.png", "RGB") for i in range(16))
    (GRASS / "grass_rgb.bin").write_bytes(grass)
    pack(b"C3DCLIFF", "", "RGB", CLIFF / "cliffs_rgb_color_matched.pack", 157)
    pack(b"C3DHEIGH", "_height", "L", CLIFF / "cliffs_height.pack", 157)
    pack(b"C3DNORML", "_normal", "RGB", CLIFF / "cliffs_normal.pack", 157)
    pack_roads(b"C3DROAD0", "", "RGB", ROAD / "roads_rgb.pack")
    pack_roads(b"C3DROADH", "_height", "L", ROAD / "roads_height.pack")
    pack_roads(b"C3DROADN", "_normal", "RGB", ROAD / "roads_normal.pack")
    pack_roads(b"C3DROADR", "_roughness", "L", ROAD / "roads_roughness.pack")
    pack_shores(b"C3DSHOR0", "", "RGB", SHORE / "shores_rgb.pack", 131)
    pack_shores(b"C3DSHORH", "_height", "L", SHORE / "shores_height.pack", 56)
    pack_shores(b"C3DSHORN", "_normal", "RGB", SHORE / "shores_normal.pack", 56)
    pack_bridges(b"C3DBRID0", "", "RGB", BRIDGE / "bridges_rgb.pack")
    pack_bridges(b"C3DBRIDH", "_height", "L", BRIDGE / "bridges_height.pack")
    pack_bridges(b"C3DBRIDN", "_normal", "RGB", BRIDGE / "bridges_normal.pack")
    pack_bridges(b"C3DBRIDR", "_roughness", "L", BRIDGE / "bridges_roughness.pack")
    pack_rivers(b"C3DRIVR0", "", "RGB", RIVER / "rivers_rgb.pack")
    pack_rivers(b"C3DRIVRH", "_height", "L", RIVER / "rivers_height.pack")
    pack_rivers(b"C3DRIVRN", "_normal", "RGB", RIVER / "rivers_normal.pack")


if __name__ == "__main__":
    main()
