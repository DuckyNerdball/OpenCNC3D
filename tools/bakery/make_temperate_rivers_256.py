#!/usr/bin/env python3
"""Bake 115 Remastered-led Temperate river cells at 256px.

The generated high-resolution sheets are source art. The installed Remastered
DDS files are local design references only and are never part of the build.
All exposed river rocks receive physical height and tangent-space normals.
"""

import csv
import re
from collections import defaultdict
from pathlib import Path

import numpy as np
from PIL import Image

from make_temperate_256 import (LUMA, N, OUT, ROOT, blur, join_internal_edges,
                                normal, periodic_patch, rgb, save, smoothstep)
from make_temperate_bridges_256 import seal_edges

SOURCE = ROOT / "art/generated/temperate_rivers/remastered_style_v1"
INVENTORY = ROOT / "docs/temperate-texture-cell-status.csv"


def dimensions(name, table):
    found = re.search(r'\{ "' + re.escape(name) + r'",[^\n]*?, (\d+), (\d+) \},', table)
    if not found:
        raise ValueError(f"missing edit-table dimensions for {name}")
    return tuple(map(int, found.groups()))


def match_clear_ground_edges(cells, grass_tile):
    """Join grassy river exits to the already approved CLEAR ground material."""
    table = (ROOT / "game/edit_tables.h").read_text()
    names = {int(index): name for name, index in re.findall(
        r'\{ "([^"]+)",[^\n]+/\* (\d+) \*/', table)}
    borders = set()
    maps = [*sorted((ROOT / "game/missions").glob("*.BIN")),
            *sorted((ROOT / "game/authored").glob("*.BIN")),
            *sorted((ROOT / "game/examples").glob("*.BIN"))]
    for path in maps:
        data = path.read_bytes()
        if len(data) != 64*64*2:
            continue
        for y in range(64):
            for x in range(64):
                i = (y*64+x)*2
                key = (names.get(data[i]), data[i+1])
                if key not in cells:
                    continue
                for dx, dy, side in ((-1, 0, "L"), (1, 0, "R"),
                                     (0, -1, "T"), (0, 1, "B")):
                    nx, ny = x+dx, y+dy
                    if not (0 <= nx < 64 and 0 <= ny < 64):
                        continue
                    j = (ny*64+nx)*2
                    if data[j] == 255 or names.get(data[j]) == "CLEAR1":
                        borders.add(key+(side,))
    band = 16
    fade = 1-smoothstep(0, band-1, np.arange(band, dtype=np.float32))
    targets = {"L": grass_tile[:, 0], "R": grass_tile[:, -1],
               "T": grass_tile[0], "B": grass_tile[-1]}
    for name, icon, side in borders:
        image = cells[(name, icon)]
        edge = {"L": image[:, 0], "R": image[:, -1],
                "T": image[0], "B": image[-1]}[side].copy()
        green = (edge[:, 1] > edge[:, 0]+3) & (edge[:, 1] > edge[:, 2]+3)
        offset = np.where(green[:, None], targets[side]-edge, 0)
        if side == "L":
            image[:, :band] += offset[:, None, :]*fade[None, :, None]
        elif side == "R":
            image[:, -band:] += offset[:, None, :]*fade[::-1][None, :, None]
        elif side == "T":
            image[:band] += offset[None, :, :]*fade[:, None, None]
        else:
            image[-band:] += offset[None, :, :]*fade[::-1][:, None, None]
    return len(borders)


def main():
    rows = [r for r in csv.DictReader(INVENTORY.open(newline="")) if r["family"] == "river"]
    groups = defaultdict(list)
    for row in rows:
        groups[row["art_key"]].append(row)
    if len(rows) != 115 or len(groups) != 13:
        raise ValueError(f"expected 115 river cells in 13 templates, got {len(rows)}/{len(groups)}")
    table = (ROOT / "game/edit_tables.h").read_text()

    # Use the exact approved grass and beach-edge materials at cross-template
    # joins. The photographic source detail is retained inside each cell.
    grass_edge = periodic_patch(rgb(OUT / "grass/CLEAR1_00.png"), 3)
    shore = rgb(ROOT / "art/generated/temperate_shores/remastered_style_v1/SH1/source_gpt_image.png")
    dry_sand = shore[390:582, 480:672]
    sand = np.asarray(Image.fromarray(np.uint8(dry_sand)).resize(
        (N, N), Image.Resampling.LANCZOS), dtype=np.float32)
    sand_edge = periodic_patch(np.array([112, 94, 73], dtype=np.float32)
                               + (sand-blur(sand, 22))*.78, 7)
    cy, cx = np.indices((N, N))
    cell_border = np.minimum.reduce((cx, cy, N-1-cx, N-1-cy))
    detail_weight = smoothstep(0, 7, cell_border)
    broad_weight = smoothstep(0, 48, cell_border)

    color_cells = {}
    for name, members in groups.items():
        w, h = dimensions(name, table)
        source = SOURCE / name / "source_gpt_image.png"
        if not source.is_file():
            raise FileNotFoundError(source)
        color = rgb(source, (w*N, h*N))
        r, g, b = color[..., 0], color[..., 1], color[..., 2]
        luma = color @ LUMA
        # Shallow water can be almost neutral in blue versus green; its blue
        # versus red separation remains clear. Requiring blue > green turned
        # those shallow runs into dark grass-shaped holes.
        water = smoothstep(8, 24, b-r)
        stone = (smoothstep(102, 143, blur(luma, 2))
                 * smoothstep(3, 20, r-b)
                 * (1-smoothstep(0, 10, g-r))
                 * (1-water))
        # Source foliage varies much more than the approved CLEAR field. Match
        # the *whole* grassy bank, not only visibly green blades: yellow/dark
        # pixels left behind made broad rectangular template borders in-game.
        # White foam and shaded rock are protected by their surrounding water
        # and stone even when those individual pixels have no diagnostic hue.
        water_region = smoothstep(.18, .52, blur(water*255, 10)/255)
        rock_region = smoothstep(.025, .20, blur(stone*255, 10)/255)
        near_water = smoothstep(.015, .14, blur(water*255, 34)/255)
        earth = (smoothstep(1, 8, r-g) * smoothstep(6, 19, r-b)
                 * (1-smoothstep(148, 183, luma)) * near_water
                 * (1-rock_region) * (1-water_region))
        grass = (1-earth) * (1-rock_region) * (1-water_region)
        ground = np.clip(grass+earth, 0, 1)
        base = np.tile(grass_edge, (h, w, 1))
        sand_base = np.tile(sand_edge, (h, w, 1))
        common = base*grass[..., None] + sand_base*earth[..., None] \
                 + color*(1-ground[..., None])
        low = blur(color, 12)
        high = color-low
        dw = np.tile(detail_weight, (h, w))[..., None]
        bw = np.tile(broad_weight, (h, w))[..., None]
        color = (common + grass[..., None]*high*.25*dw
                 + earth[..., None]*(high*.72*dw + (low-common)*.24*bw))

        # Positive relief is confined to stone. Wet banks and open water stay
        # close to the terrain datum; source facets give the rock its normal.
        luma = color @ LUMA
        detail = color-blur(color, 3)
        macro = smoothstep(82, 172, blur(luma, 13))
        height = np.clip(128 + stone*(46 + 66*macro)
                         + stone*(detail @ LUMA)*.24, 0, 255)
        normals = normal(height, detail, stone, .00010)
        icons = {int(row["icon"]) for row in members}
        color = seal_edges(join_internal_edges(color, w, h, icons, feather=3), w, h, icons)
        height = seal_edges(join_internal_edges(height, w, h, icons, feather=3), w, h, icons)
        normals = seal_edges(join_internal_edges(normals, w, h, icons, feather=3), w, h, icons)
        for row in members:
            icon = int(row["icon"])
            if icon >= w*h:
                raise ValueError(f"{name} icon {icon} outside {w}x{h}")
            x, y = (icon%w)*N, (icon//w)*N
            box = np.s_[y:y+N, x:x+N]
            color_cells[(name, icon)] = color[box].copy()
            dest = OUT / "rivers" / name
            stem = f"{name}_{icon:02}"
            save(dest / f"{stem}_height.png", height[box], "L")
            save(dest / f"{stem}_normal.png", normals[box], "RGB")
    # The source sheets already carry continuous water and bank geometry across
    # their internal cells. The shore edge matcher mixes unlike materials at
    # river junctions (shallow water can be classified as land), producing
    # rectangular dark strips through the water. Preserve these sheet joins.
    ground_edges = match_clear_ground_edges(color_cells, grass_edge)
    for (name, icon), image in color_cells.items():
        save(OUT / "rivers" / name / f"{name}_{icon:02}.png", image, "RGB")
    print("river continuity: source-sheet internal joins preserved")
    print(f"river/CLEAR continuity: {ground_edges} grassy exit profiles")
    print(f"256px Remastered-led rivers: {len(color_cells)} RGB/height/normal cells")


if __name__ == "__main__":
    main()
