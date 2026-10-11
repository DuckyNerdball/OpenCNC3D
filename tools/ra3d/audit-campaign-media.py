#!/usr/bin/env python3
"""Check every original Red Alert campaign movie and map choice staged for play."""

from __future__ import annotations

import argparse
from configparser import ConfigParser
from pathlib import Path
import re


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", required=True, type=Path)
    args = parser.parse_args()
    module = args.module
    source = Path(__file__).resolve().parents[2] / "brain/vanilla/redalert/mapsel.cpp"
    missions = sorted((module / "missions").glob("SC[GU]*.INI"))
    if len(missions) != 42:
        raise RuntimeError(f"expected 42 base campaign INIs, found {len(missions)}")

    movie_keys = ("Intro", "Brief", "Action", "Win", "Win2", "Win3", "Win4", "Lose")
    movie_names: set[str] = set()
    missing: list[str] = []
    for mission in missions:
        ini = ConfigParser(interpolation=None, strict=False)
        ini.read(mission, encoding="latin-1")
        for key in movie_keys:
            name = ini.get("Basic", key, fallback="").strip().upper()
            if not name or name in ("<NONE>", "NONE", "X"):
                continue
            movie_names.add(name)
            if not (module / "movies" / f"{name}.VQA").is_file():
                missing.append(f"{mission.stem}: {key}={name}")

    table = source.read_text().split("MapCoords[2][14][3] = ", 1)[1].split(";", 1)[0]
    coords = re.findall(r"\{(-?\d+),\s*(-?\d+)\}", table)
    if len(coords) != 2 * 14 * 3:
        raise RuntimeError(f"expected 84 source map coordinates, found {len(coords)}")
    choices = 0
    for side, prefix in enumerate(("SCG", "SCU")):
        for completed in range(1, 14):
            for choice in range(3):
                x, _ = map(int, coords[(side * 14 + completed) * 3 + choice])
                if x < 0:
                    continue
                choices += 1
                next_name = f"{prefix}{completed + 1:02d}E{chr(65 + choice)}.INI"
                if not (module / "missions" / next_name).is_file():
                    missing.append(f"map choice after {prefix}{completed:02d}: {next_name}")
    if missing:
        raise RuntimeError("missing campaign content:\n" + "\n".join(missing))
    print(f"RA campaign inventory: {len(missions)} INIs, "
          f"{len(movie_names)} movie names, {choices} map choices; all staged")


if __name__ == "__main__":
    main()
