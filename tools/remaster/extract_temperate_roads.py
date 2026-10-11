#!/usr/bin/env python3
"""Extract local Remastered D01-D43 references; never package EA source art."""

import argparse
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from PIL import Image

from dds import decode
from meg import Meg

ROOT = Path(__file__).resolve().parents[2]


def dimensions():
    result = {}
    for line in (ROOT / "game/edit_tables.h").read_text().splitlines():
        match = re.search(r'\{ "(D\d\d)",.*?, (\d+), (\d+) \},\s*/\* (\d+) \*/', line)
        if match and 1 <= int(match[1][1:]) <= 43:
            result[match[1]] = (int(match[2]), int(match[3]))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, required=True,
                        help="Remastered Collection CnCRemastered/Data directory")
    parser.add_argument("--out", type=Path, required=True,
                        help="local, uncommitted reference directory")
    args = parser.parse_args()
    config = Meg(args.data / "CONFIG.MEG")
    textures = Meg(args.data / "TEXTURES_TD_SRGB.MEG")
    xml = ET.fromstring(config.read(r"DATA\XML\TILESETS\TD_TERRAIN_TEMPERATE.XML"))
    dims = dimensions()
    args.out.mkdir(parents=True, exist_ok=True)
    found = 0
    for tile in xml.findall(".//Tile"):
        name = tile.findtext("./Key/Name")
        if name not in dims:
            continue
        icon = int(tile.findtext("./Key/Shape"))
        frame = tile.findtext("./Value/Frames/Frame")
        if not frame:
            continue
        archive_path = ("DATA\\ART\\TEXTURES\\SRGB\\Tiberian_Dawn\\Terrain\\Temperate\\"
                        + frame[:-4] + ".DDS")
        raw = textures.read(archive_path)
        if raw is None:
            raise FileNotFoundError(archive_path)
        Image.fromarray(decode(raw)[..., :3], "RGB").save(
            args.out / f"{name}_{icon:02}.png")
        found += 1
    if found != 215:
        raise ValueError(f"found {found} road references, expected 215")
    print(f"extracted {found} road references to {args.out}")


if __name__ == "__main__":
    main()
