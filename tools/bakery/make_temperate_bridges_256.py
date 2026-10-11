#!/usr/bin/env python3
"""Bake continuous Remastered-led bridge sheets into 256px RGB/height/normal cells.

The committed generated sheets are new art edited from a local Remastered
reference. The Remastered DDS files themselves are never copied into the repo.
The cartridge atlas continues to provide the water alpha cutout at runtime.
"""

import csv
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from make_temperate_256 import (LUMA, N, OUT, ROOT, blur, join_internal_edges,
                                normal, periodic_patch, rgb, road_gravel_material,
                                save, smoothstep)

SOURCE = ROOT / "art/generated/temperate_bridges/remastered_style_v1"
INVENTORY = ROOT / "docs/temperate-texture-cell-status.csv"
SHEETS = {"BRIDGE1": (4, 4), "BRIDGE2": (5, 5)}

# The polylines mark the two structural parapets in each approved isometric
# source. Work on the complete sheet: independent per-cell detection gives
# every bridge block a different height and breaks continuous stonework.
PARAPETS = {
    "BRIDGE1": [
        [(75, 822), (203, 680), (329, 540), (470, 403), (682, 224)],
        [(605, 1010), (727, 884), (857, 747), (993, 603), (1192, 409)],
    ],
    "BRIDGE2": [
        [(67, 320), (170, 416), (279, 524), (391, 631), (517, 751)],
        [(458, 213), (552, 314), (659, 422), (771, 535), (900, 658)],
    ],
}
ROAD_CENTRE = {
    "BRIDGE1": ([(0, 1240), (1254, 0)], 485),
    "BRIDGE2": ([(0, 0), (1254, 1254)], 325),
}


def line_mask(name, size, width):
    image = Image.new("L", size)
    draw = ImageDraw.Draw(image)
    for points in PARAPETS[name]:
        points = [(round(x * size[0] / 1254), round(y * size[1] / 1254))
                  for x, y in points]
        draw.line(points, fill=255, width=round(width * size[0] / 1254), joint="curve")
        radius = round(width * size[0] / 1254 / 2)
        for x, y in points:
            draw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=255)
    return np.asarray(image.filter(ImageFilter.GaussianBlur(3)), dtype=np.float32) / 255


def road_mask(name, size):
    points, width = ROAD_CENTRE[name]
    image = Image.new("L", size)
    draw = ImageDraw.Draw(image)
    points = [(round(x * size[0] / 1254), round(y * size[1] / 1254))
              for x, y in points]
    draw.line(points, fill=255, width=round(width * size[0] / 1254))
    return np.asarray(image.filter(ImageFilter.GaussianBlur(9)), dtype=np.float32) / 255


def seal_edges(a, columns, rows, icons):
    """Make final shared texels identical, including partial-template corners."""
    a = a.copy()
    used = set(icons)
    for row in range(rows):
        for column in range(1, columns):
            left, right = row*columns+column-1, row*columns+column
            if left in used and right in used:
                x, y = column*N, row*N
                common = (a[y:y+N, x-1].copy()+a[y:y+N, x].copy())*.5
                a[y:y+N, x-1] = a[y:y+N, x] = common
    for row in range(1, rows):
        for column in range(columns):
            top, bottom = (row-1)*columns+column, row*columns+column
            if top in used and bottom in used:
                x, y = column*N, row*N
                common = (a[y-1, x:x+N].copy()+a[y, x:x+N].copy())*.5
                a[y-1, x:x+N] = a[y, x:x+N] = common
    for row in range(1, rows):
        for column in range(1, columns):
            names = [(row-1)*columns+column-1, (row-1)*columns+column,
                     row*columns+column-1, row*columns+column]
            points = [(row*N-1, column*N-1), (row*N-1, column*N),
                      (row*N, column*N-1), (row*N, column*N)]
            active = [i for i, icon in enumerate(names) if icon in used]
            if len(active) < 2 or (len(active) == 2 and set(active) in ({0, 3}, {1, 2})):
                continue
            common = np.mean([a[points[i]] for i in active], axis=0)
            for i in active:
                a[points[i]] = common
    return a


def main():
    rows = list(csv.DictReader(INVENTORY.open(newline="")))
    selected = {name: [r for r in rows if r["template"] == name]
                for name in SHEETS}
    grass = periodic_patch(rgb(OUT / "grass/CLEAR1_00.png"), 3)
    approved_gravel = road_gravel_material()
    total = 0
    for name, (columns, rows_count) in SHEETS.items():
        size = (columns * N, rows_count * N)
        source = rgb(SOURCE / name / "source_gpt_image.png", size)
        r, g, b = source[..., 0], source[..., 1], source[..., 2]
        green = smoothstep(4, 19, g-r) * smoothstep(3, 15, g-b)
        blue = smoothstep(7, 25, b-r) * smoothstep(3, 15, b-g)
        grass_base = np.tile(grass, (rows_count, columns, 1))
        # Match the approved grass at every bridge approach, while retaining
        # high-frequency foliage detail from the new source away from its edge.
        fine = source - blur(source, 9)
        color = source*(1-green[..., None]) + green[..., None]*(grass_base + fine*.35)

        yy, xx = np.indices((size[1], size[0]))
        parapet = line_mask(name, size, 50)
        corridor = road_mask(name, size)
        # The source sheets already contain authored stone paving and worn
        # wheel tracks on the deck. Gravel belongs on the approaches; extending
        # its mask through the middle hid the entire paved surface.
        travel = ((xx-yy+size[1]) if name == "BRIDGE1" else (xx+yy)) / \
                 (size[0]+size[1])
        paved_span = smoothstep(.20, .39, travel) * (1-smoothstep(.61, .80, travel))
        road = np.clip(corridor * (1-blue) * (1-line_mask(name, size, 92))
                       * (1-.58*paved_span), 0, 1)
        road = smoothstep(.22, .80, road)
        outer = np.minimum.reduce((xx, yy, size[0]-1-xx, size[1]-1-yy))
        interior = smoothstep(0, 24, outer)
        gravel = np.tile(approved_gravel, (rows_count, columns, 1))
        road_color = gravel + (source-blur(source, 3))*.28*interior[..., None]
        road_color += (blur(source, 10)-blur(source, 54))*.25*interior[..., None]
        color = color*(1-road[..., None]) + road_color*road[..., None]
        # A deck rises slightly over the banks. Parapet coping is substantially
        # higher. Approach gravel uses recessed ruts and roughness; the exposed
        # deck paving keeps its source stone detail and raised relief.
        deck = corridor * (1-blue)
        height = 128 + 12*deck + 67*parapet
        high = (source - blur(source, 4)) @ LUMA
        height += (deck*.48 + parapet*.42) * np.clip(high, -28, 28)
        gravel_luma = gravel @ LUMA
        gravel_height = 120 + (gravel_luma-blur(gravel_luma, 4))*.34
        height = height*(1-road) + gravel_height*road
        height = np.clip(height, 100, 230)
        # Fade only the outer template edge into neighbouring plain terrain.
        edge_height = 128-8*road
        height = edge_height + (height-edge_height)*smoothstep(0, 18, outer)
        rough = 255-78*road + road*(gravel_luma-blur(gravel_luma, 6))*.25
        icons = {int(row["icon"]) for row in selected[name]}
        color = seal_edges(join_internal_edges(color, columns, rows_count, icons, feather=3),
                           columns, rows_count, icons)
        height = seal_edges(join_internal_edges(height, columns, rows_count, icons, feather=3),
                            columns, rows_count, icons)
        rough = seal_edges(join_internal_edges(rough, columns, rows_count, icons, feather=3),
                           columns, rows_count, icons)
        detail = color - blur(color, 3)
        normals = normal(height, detail, np.clip(deck+parapet, 0, 1), .00010)
        normals = seal_edges(join_internal_edges(normals, columns, rows_count, icons,
                                                  feather=3), columns, rows_count, icons)
        for row in selected[name]:
            icon = int(row["icon"])
            if icon >= columns*rows_count:
                raise ValueError(f"{name} icon {icon} outside {columns}x{rows_count}")
            x, y = (icon % columns)*N, (icon // columns)*N
            box = np.s_[y:y+N, x:x+N]
            dest = OUT / "bridges" / name
            stem = f"{name}_{icon:02}"
            save(dest / f"{stem}.png", color[box], "RGB")
            save(dest / f"{stem}_height.png", height[box], "L")
            save(dest / f"{stem}_normal.png", normals[box], "RGB")
            save(dest / f"{stem}_roughness.png", rough[box], "L")
            total += 1
    if total != 35:
        raise ValueError(f"expected 35 bridge cells, got {total}")
    print(f"256px Remastered-led bridges: {total} RGB/height/normal cells")


if __name__ == "__main__":
    main()
