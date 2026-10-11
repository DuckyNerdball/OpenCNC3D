#!/usr/bin/env python3
"""Derive 256px Temperate shore cells from Remastered-led generated source sheets.

The source sheets were edited from the matching Remastered Temperate templates.
Do not use the old cartridge art as a color or material guide. The engine keeps
its existing cartridge alpha solely as the water cutout mechanism.
"""

import csv
import re
from collections import defaultdict
from pathlib import Path

import numpy as np
from PIL import Image

from make_temperate_256 import (N, OUT, ROOT, LUMA, blur, normal,
                                periodic_patch, rgb, save, seam, smoothstep)

SOURCE = ROOT / "art/generated/temperate_shores/remastered_style_v1"
INVENTORY = ROOT / "docs/temperate-texture-cell-status.csv"
ROCK_TEMPLATES = {"SH2", "SH3", "SH4", "SH11", "SH12", "SH13", "SH14", "SH15", "SH16"}
WATER_ONLY = {"SH17", "SH18"}


def stitch_connected_edges(cells):
    """Share exact border texels across shore icons that meet in shipped maps.

    Generated sheets are independent images, so matching their mean material
    still leaves abrupt changes where two different templates touch. The map
    adjacency graph identifies the legal joins. One real generated edge is
    selected for each connected edge class, and the correction tapers inward
    without smoothing its photographic detail.
    """
    table = (ROOT / "game/edit_tables.h").read_text()
    names = {int(index): name for name, index in re.findall(
        r'\{ "([^"]+)",[^\n]+/\* (\d+) \*/', table)}
    parent = {}
    grass_borders = set()

    def root(node):
        parent.setdefault(node, node)
        if parent[node] != node:
            parent[node] = root(parent[node])
        return parent[node]

    def connect(a, b):
        a, b = root(a), root(b)
        if a != b:
            parent[b] = a

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
                a = (names.get(data[i]), data[i+1])
                if a not in cells:
                    continue
                for dx, dy, side in ((-1, 0, "L"), (1, 0, "R"),
                                     (0, -1, "T"), (0, 1, "B")):
                    nx, ny = x+dx, y+dy
                    if 0 <= nx < 64 and 0 <= ny < 64:
                        j = (ny*64+nx)*2
                        if data[j] == 255 or names.get(data[j]) == "CLEAR1":
                            grass_borders.add(a+(side,))
                if x < 63:
                    j = i+2
                    b = (names.get(data[j]), data[j+1])
                    if b in cells:
                        connect(a+("R",), b+("L",))
                if y < 63:
                    j = i+128
                    b = (names.get(data[j]), data[j+1])
                    if b in cells:
                        connect(a+("B",), b+("T",))

    classes = defaultdict(list)
    for node in parent:
        classes[root(node)].append(node)

    def border(image, side):
        return {"L": image[:, 0], "R": image[:, -1],
                "T": image[0], "B": image[-1]}[side]

    corrections = defaultdict(list)
    for nodes in classes.values():
        if len(nodes) < 2:
            continue
        samples = np.stack([border(cells[node[:2]], node[2]) for node in nodes])
        red, green, blue = samples[..., 0], samples[..., 1], samples[..., 2]
        # An edge class can contain land and open-water positions because the
        # game's alpha cutout admits several legal shoreline contours. Never
        # transfer a blue water texel into opaque grass/sand (or vice versa).
        material = np.zeros(red.shape, dtype=np.uint8)
        material[(green > red+3) & (green > blue+3)] = 1
        material[(blue > red+8) & (blue > green+1)] = 2
        targets = samples.copy()
        for pos in range(N):
            for kind in range(3):
                member = np.flatnonzero(material[:, pos] == kind)
                if len(member) < 2:
                    continue
                segment = samples[member, pos]
                median = np.median(segment, axis=0)
                medoid = member[np.argmin(np.square(segment-median).sum(axis=1))]
                targets[member, pos] = samples[medoid, pos]
        for node, sample, target in zip(nodes, samples, targets):
            if node in grass_borders:
                grass = (sample[:, 1] > sample[:, 0]+3) & (
                    sample[:, 1] > sample[:, 2]+3)
                # A mostly grassy exit faces the existing CLEAR tiles; its
                # complete source edge is safer than a beach/water medoid from
                # a different map's shore adjacency.
                target = sample if grass.mean() > .65 else np.where(
                    grass[:, None], sample, target)
            corrections[node[:2]].append((node[2], target-sample))

    width = 24
    falloff = 1-smoothstep(0, width-1, np.arange(width, dtype=np.float32))
    for key, patches in corrections.items():
        image = cells[key]
        delta = np.zeros_like(image)
        weight = np.zeros(image.shape[:2], dtype=np.float32)
        for side, offset in patches:
            if side == "L":
                band = (slice(None), slice(0, width))
                w = falloff[None, :]
                v = offset[:, None, :]
            elif side == "R":
                band = (slice(None), slice(-width, None))
                w = falloff[::-1][None, :]
                v = offset[:, None, :]
            elif side == "T":
                band = (slice(0, width), slice(None))
                w = falloff[:, None]
                v = offset[None, :, :]
            else:
                band = (slice(-width, None), slice(None))
                w = falloff[::-1][:, None]
                v = offset[None, :, :]
            delta[band] += v*w[..., None]
            weight[band] += w
        # Averaging only at corner overlaps avoids additive overcorrection.
        image += delta / np.maximum(weight[..., None], 1)
    return len(classes), sum(len(x) for x in corrections.values())


def visible_slots():
    """Slots where the engine's existing 24px alpha exposes solid shore art.

    This technical cutout is not a design or color source. Fully transparent
    cells belong to the later open-water pass even if their template says SH.
    """
    with Image.open(ROOT / "tools/bakery/sharecopy/assets/terrain/TEMPERAT_tiles.png") as image:
        alpha = np.asarray(image.getchannel("A"))
    result = set()
    for row in csv.DictReader(INVENTORY.open(newline="")):
        if row["family"] != "shore":
            continue
        slot = int(row["bank_slot"])
        x, y = (slot%32)*26+1, (slot//32)*26+1
        if (alpha[y:y+24, x:x+24] > 127).any():
            result.add(slot)
    return result


def dimensions(name, table):
    match = re.search(r'\{ "' + re.escape(name) + r'",[^\n]*?, (\d+), (\d+) \},', table)
    if not match:
        raise ValueError(f"missing edit table dimensions for {name}")
    return tuple(map(int, match.groups()))


def main():
    rows = [r for r in csv.DictReader(INVENTORY.open(newline="")) if r["family"] == "shore"]
    land_slots = visible_slots()
    groups = {}
    for row in rows:
        groups.setdefault(row["art_key"], []).append(row)
    table = (ROOT / "game/edit_tables.h").read_text()
    grass_edge = periodic_patch(rgb(OUT / "grass/CLEAR1_00.png"), 3)
    sand_source = rgb(SOURCE / "SH1/source_gpt_image.png")
    # A pure dry-sand window in the approved SH1 source. The old window reached
    # the darker wet beach/wave at its bottom, so its 256px repeat created a
    # broad dark rectangle in every other visible shore cell.
    sample = sand_source[390:582, 480:672]
    sample = np.asarray(Image.fromarray(np.uint8(sample)).resize(
        (N, N), Image.Resampling.LANCZOS), dtype=np.float32)
    low = blur(sample, 22)
    sand_edge = (np.array([112, 94, 73], dtype=np.float32)
                 + (sample-low)*.78 + (low-low.mean(axis=(0, 1)))*.10)
    sand_edge = periodic_patch(sand_edge, 7)
    yy, xx = np.indices((N, N))
    cell_edge = np.minimum.reduce((xx, yy, N-1-xx, N-1-yy))
    detail_weight = smoothstep(0, 7, cell_edge)[..., None]
    broad_weight = smoothstep(0, 56, cell_edge)[..., None]
    color_count = relief_count = 0
    output_cells = {}
    for name, cells in groups.items():
        if name in WATER_ONLY:
            continue
        w, h = dimensions(name, table)
        source = SOURCE / name / "source_gpt_image.png"
        if not source.is_file():
            raise FileNotFoundError(source)
        # Resize a whole Remastered-led template before cutting its cells so
        # image detail continues through the internal template boundaries.
        color = rgb(source, (w*N, h*N))
        r, g, b = color[..., 0], color[..., 1], color[..., 2]
        luma = color @ LUMA
        grass = (smoothstep(3, 13, g-r) * smoothstep(2, 11, g-b)
                 * (1-smoothstep(80, 115, g)))
        if name not in ("SH3", "SH4"):
            grass_sample = (g-r > 3) & (g-b > 5) & (g < 90)
            if grass_sample.sum() > 1000:
                grass_target = np.array([44.5, 58.1, 43.4], dtype=np.float32)
                color += grass[..., None] * (grass_target-color[grass_sample].mean(axis=0))

        # Different generated templates otherwise meet with a conspicuous
        # light/dark sand seam. Match only their warm midtone sand to one common
        # Remastered-led target, preserving every high-resolution grain.
        r, g, b = color[..., 0], color[..., 1], color[..., 2]
        luma = color @ LUMA
        sand_sample = ((r-g > 5) & (r-b > 15) & (luma > 65) & (luma < 125))
        if sand_sample.sum() > 1000:
            sand = (smoothstep(3, 10, r-g) * smoothstep(10, 23, r-b)
                    * (1-smoothstep(110, 145, luma)) * (1-grass))
            sand_target = np.array([112, 94, 73], dtype=np.float32)
            color += sand[..., None] * (sand_target-color[sand_sample].mean(axis=0))
            broad = blur(color, 48)
            color += sand[..., None] * (sand_target-broad)
            # Different templates can be neighbours on a map. Only their
            # outside three RGB pixels approach the same high-resolution
            # grass/sand material; internal cell joins stay untouched.
            grass_ref = np.tile(grass_edge, (h, w, 1))
            sand_ref = np.tile(sand_edge, (h, w, 1))
            edge_weight = np.clip(grass+sand, 0, 1)
            edge_target = (color*(1-edge_weight[..., None])
                           + grass_ref*grass[..., None]
                           + sand_ref*sand[..., None])
            color = seam(color, edge_target, 3)

        # Matching only three edge pixels leaves the large light/dark blocks
        # visible in-game when two independently generated templates meet. Use
        # the same periodic high-resolution ground material on *every* cell,
        # retaining source grit and broad wet/dry variation inside the cell.
        # Both source contributions vanish smoothly at the boundary; the
        # shared material continues there without a soft painted grid line.
        base = np.tile(grass_edge, (h, w, 1))
        sand_base = np.tile(sand_edge, (h, w, 1))
        ry, rx = color[..., 0], color[..., 1]
        by = color[..., 2]
        lum = color @ LUMA
        green = (smoothstep(3, 12, rx-ry) * smoothstep(3, 12, rx-by)
                 * (1-smoothstep(76, 112, rx)))
        earth = (smoothstep(1, 8, ry-rx) * smoothstep(6, 19, ry-by)
                 * (1-smoothstep(148, 183, lum)) * (1-green))
        ground = np.clip(green+earth, 0, 1)
        common = (base*green[..., None] + sand_base*earth[..., None]
                  + color*(1-ground[..., None]))
        low = blur(color, 12)
        high = color-low
        tile_detail = np.tile(detail_weight, (h, w, 1))
        tile_broad = np.tile(broad_weight, (h, w, 1))
        color = (common + ground[..., None]*
                 (high*.72*tile_detail + (low-common)*.24*tile_broad))

        if name in ROCK_TEMPLATES:
            luma = color @ LUMA
            # Isolate bright neutral stone from blue water, green vegetation,
            # and darker sand. Macro volume and fine source detail both enter
            # the physical height map; water and flat shore stay at mid-height.
            stone = (smoothstep(104, 139, blur(luma, 2))
                     * smoothstep(3, 18, r-b)
                     * (1-smoothstep(0, 9, g-r)))
            macro = smoothstep(75, 170, blur(luma, 14))
            detail = color - blur(color, 3)
            height = np.clip(128 + stone*(48 + 69*macro)
                             + stone*(detail @ LUMA)*.22, 0, 255)
            # Keep the image-derived facets in the normal, including details
            # finer than the 24x24 geometry lattice can represent.
            normals = normal(height, detail, stone, .00010)
        for row in cells:
            if int(row["bank_slot"]) not in land_slots:
                continue
            icon = int(row["icon"])
            if icon >= w*h:
                raise ValueError(f"{name} icon {icon} outside {w}x{h}")
            x, y = (icon%w)*N, (icon//w)*N
            box = np.s_[y:y+N, x:x+N]
            target = OUT / "shores" / name
            stem = f"{name}_{icon:02}"
            output_cells[(name, icon)] = color[box].copy()
            color_count += 1
            if name in ROCK_TEMPLATES:
                save(target / f"{stem}_height.png", height[box], "L")
                save(target / f"{stem}_normal.png", normals[box], "RGB")
                relief_count += 1
    if (color_count, relief_count) != (131, 56):
        raise ValueError(f"expected 131 shore RGB and 56 relief cells, got {color_count}/{relief_count}")
    classes, joined = stitch_connected_edges(output_cells)
    for (name, icon), tile in output_cells.items():
        save(OUT / "shores" / name / f"{name}_{icon:02}.png", tile, "RGB")
    print(f"shore continuity: {joined} edge profiles in {classes} map adjacency classes")
    print(f"256px Remastered-led shores: {color_count} visible RGB, {relief_count} rock relief, 28 water-only deferred")


if __name__ == "__main__":
    main()
