#!/usr/bin/env python3
"""Bake original Red Alert SHP frames and theater palettes for Classic mode."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "menu" / "tools"))
sys.path.insert(0, str(ROOT / "menu"))
from mixshp import Shape, ShpError  # noqa: E402
from bake_dosmenu import load_cps, widen  # noqa: E402
from ramix import RaMix  # noqa: E402

# aadata: the aircraft (HIND, HELI, YAK, MIG ...); bbdata: the bullets (V2, BOMB ...);
# vessel: the ships' turrets, named in code rather than in a table (TURR, SSAM, MGUN).
TABLES = ("bdata", "udata", "idata", "adata", "vdata", "tdata", "odata", "sdata",
          "aadata", "bbdata", "vessel",
          "animdata", "bullet", "smudge", "overlay", "terrain", "object",
          "unit", "infantry")
EXTRA = {"WAKE", "SMOKEY", "DRAGON", "RROTOR", "LROTOR", "SHADOW", "TRAN"}


def names_from_engine() -> list[str]:
    source = ROOT / "brain" / "vanilla" / "redalert"
    names = set(EXTRA) | {"OREMINE"}
    for stem in TABLES:
        path = source / f"{stem}.cpp"
        if path.is_file():
            names.update(re.findall(r'"([A-Z0-9_-]{2,8})"',
                                    path.read_text(errors="ignore")))
    building_source = (source / "bdata.cpp").read_text(errors="ignore")
    buildings = set(re.findall(r'"([A-Z0-9_-]{2,8})"', building_source))
    names.update(name + "MAKE" for name in buildings if len(name) <= 4)
    return sorted(names)


def image_aliases(rules: bytes) -> dict[str, str]:
    """Match the original engine's Graphic_Name overrides from RULES.INI."""
    aliases: dict[str, str] = {}
    section = ""
    for line in rules.decode("latin-1").splitlines():
        line = line.split(";", 1)[0].strip()
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].upper()
        elif line.upper().startswith("IMAGE=") and section:
            image = line.split("=", 1)[1].strip().upper()
            if re.fullmatch(r"[A-Z0-9_-]{2,8}", image) and image != section:
                aliases[section] = image
                aliases[section + "MAKE"] = image + "MAKE"
    return aliases


def bake(data: Path, out: Path) -> None:
    conquer = RaMix(data / "CONQUER.MIX")
    lores = RaMix(data / "LORES.MIX")
    snow = RaMix(data / "SNOW.MIX")
    temperat = RaMix(data / "TEMPERAT.MIX")
    interior = RaMix(data / "INTERIOR.MIX")
    local = RaMix(data / "LOCAL.MIX")
    rules = local.read("RULES.INI")
    if rules is None:
        raise ValueError("LOCAL.MIX is missing the original RULES.INI art aliases")
    aliases = image_aliases(rules)
    # The DLL interface renames the ore mine (terrain type MINE) to OREMINE for its
    # draw records; its art is still MINE's.
    aliases["OREMINE"] = "MINE"
    palettes = (local.read("SNOW.PAL"), local.read("TEMPERAT.PAL"),
                local.read("INTERIOR.PAL"))
    remap_cps = local.read("PALETTE.CPS")
    if any(palette is None or len(palette) != 768 for palette in palettes) or remap_cps is None:
        raise ValueError("all three theater palettes and PALETTE.CPS must come from the RA disc")
    remap_picture, _ = load_cps(remap_cps)
    base = remap_picture[:16]
    remaps = b"".join(remap_picture[row * 320:row * 320 + 16]
                      for row in range(8))
    records: list[tuple[str, int, int, int, bytes]] = []
    # DisplayClass::TransIconset is an ICN, not a SHP. CellClass draws its
    # green/red hash stamps under the pending building, one stamp per occupied
    # cell. Keep all four original frames in the same indexed sprite pack.
    trans = conquer.read("TRANS.ICN")
    if trans is None:
        raise ValueError("CONQUER.MIX is missing the RA placement cursor TRANS.ICN")
    width, height, count, _alloc, _mw, _mh, size, image_offset, \
        _pal, _rem, _trans, _colour_map, map_offset = struct.unpack_from("<6h7i", trans)
    if (width, height, count, size, image_offset) != (24, 24, 4, len(trans), 40):
        raise ValueError("TRANS.ICN has an unexpected format")
    placement = []
    for icon in trans[map_offset:map_offset + count]:
        if icon == 0xFF or image_offset + (icon + 1) * 576 > len(trans):
            raise ValueError("TRANS.ICN has a missing placement frame")
        placement.append(trans[image_offset + icon * 576:image_offset + (icon + 1) * 576])
    records.append(("TRANS", 24, 24, count, zlib.compress(b"".join(placement), 9)))
    # Projectiles (V2, BOMB ...) are named only by RULES.INI's Image= lines, so every
    # image that file names is baked under its own name as well.
    wanted = sorted(set(names_from_engine()) | {image for image in aliases.values()
                                                if not image.endswith("MAKE")})
    for name in wanted:
        graphic = aliases.get(name, name)
        for prefix, extension, sources in (
            ("", "SHP", (lores, conquer)),
            ("S/", "SNO", (snow,)),
            ("T/", "TEM", (temperat,)),
            ("I/", "INT", (interior,)),
        ):
            member = f"{graphic}.{extension}"
            content = next((entry for source in sources
                            if (entry := source.read(member)) is not None), None)
            if content is None:
                continue
            try:
                shape = Shape(content)
                if shape.width > 192 or shape.height > 192 or shape.frames > 1024:
                    continue
                frames = b"".join(shape.frame(index)
                                  for index in range(shape.frames))
            except (ShpError, IndexError, ValueError) as error:
                print(f"skip {member}: {error}", file=sys.stderr)
                continue
            records.append((prefix + name, shape.width, shape.height,
                            shape.frames, zlib.compress(frames, 9)))
    if not any(name == "JEEP" for name, *_ in records) or \
       not any(name == "1TNK" for name, *_ in records):
        raise ValueError("essential RA vehicle sprites are missing")
    # Every base-game AnimTypeClass must have art in at least one source archive.
    # BEACON is a remaster-only virtual animation; ANTDIE belongs to the
    # FIXIT_ANTS expansion, whose data is not on the two original retail discs.
    animation_source = (ROOT / "brain" / "vanilla" / "redalert" / "adata.cpp").read_text(
        errors="ignore")
    animation_names = set(re.findall(
        r'"([A-Z0-9_-]{2,8})"\s*,\s*// Data name of animation',
        animation_source)) - {"BEACON", "ANTDIE"}
    packed_names = {name for name, *_ in records}
    missing_animations = sorted(name for name in animation_names
                                if not any(prefix + name in packed_names
                                           for prefix in ("", "S/", "T/", "I/")))
    if missing_animations:
        raise ValueError("original RA animation art missing: " +
                         ", ".join(missing_animations))
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("wb") as stream:
        stream.write(b"DOS2D005")
        stream.write(struct.pack("<I", len(records)))
        stream.write(base)
        stream.write(remaps)
        for palette in palettes:
            stream.write(widen(palette))
        for name, width, height, count, compressed in records:
            stream.write(struct.pack("<16sHHII", name.encode(), width, height,
                                     count, len(compressed)))
            stream.write(compressed)
    print(f"RA Classic: {len(records)} original shapes, "
          f"{out.stat().st_size} bytes in {out} "
          f"({len(aliases)} RULES.INI image aliases considered)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args()
    bake(options.data, options.out)
