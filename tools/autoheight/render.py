#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- pictures of a map's tiles and its heights.

    python3 render.py SCG01EA [more ...] [--out DIR] [--px 8]

For each map writes <SCEN>_tiles.png (the cartridge's own tile art), and, when
the map has heights, <SCEN>_relief.png (the heights as a lit relief tinted by
height band) and <SCEN>_over.png (the relief laid over the tile art). Used by
the analysis and by the remaster lab's heightmap category.
"""
import os, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import corpus as C

ATLAS = os.path.join(C.ROOT, "tools", "bakery", "sharecopy", "assets", "terrain")
TS, GUTTER, COLS = 24, 1, 32
PITCH = TS + 2 * GUTTER
_atl = {}


def atlas(root):
    if root not in _atl:
        _atl[root] = np.asarray(Image.open(os.path.join(ATLAS, root + "_tiles.png")).convert("RGB"))
    return _atl[root]


def tiles_image(m, px=24):
    """The 64x64 map in its own tile art, px pixels a cell."""
    a = atlas(m.root)
    out = np.zeros((64 * TS, 64 * TS, 3), np.uint8)
    for y in range(64):
        for x in range(64):
            tid = int(m.tid[y, x])
            ok = tid < m.n4 or (1024 <= tid < 1024 + (len(C.theater(m.root)["tl8"]) // 2))
            if tid == 0xFFFF or not ok:
                slot = (x & 3) | ((y & 3) << 2)
            else:
                slot = tid if tid < m.n4 else m.n4 + (tid - 1024)
            col, row = slot % COLS, slot // COLS
            sx, sy = col * PITCH + GUTTER, row * PITCH + GUTTER
            out[y * TS:(y + 1) * TS, x * TS:(x + 1) * TS] = a[sy:sy + TS, sx:sx + TS]
    im = Image.fromarray(out)
    if px != TS:
        im = im.resize((64 * px, 64 * px), Image.BILINEAR)
    return im


def _upsample(h, px):
    """Bilinear corner heights to a (64*px, 64*px) field, sampled at pixel centres."""
    n = 64 * px
    u = (np.arange(n) + 0.5) / px
    x0 = np.clip(np.floor(u).astype(int), 0, 63)
    f = u - x0
    hh = h.astype(np.float32)
    a = hh[:, x0] * (1 - f) + hh[:, x0 + 1] * f            # [65, n]
    b = a[x0, :] * (1 - f)[:, None] + a[x0 + 1, :] * f[:, None]
    return b


BANDS = np.array([[40, 70, 140], [70, 120, 60], [150, 150, 80],
                  [170, 120, 70], [200, 190, 180], [255, 255, 255]], np.float32)


def relief_image(h, px=8, tint=True):
    """Lit relief: light from the north-west the way the console lights its
    terrain, tinted by height band so the rungs read at a glance."""
    f = _upsample(h, px) * 4.0                  # world units
    pitch = 256.0 / px
    gy, gx = np.gradient(f, pitch)
    nx, ny, nz = -gx, -gy, np.ones_like(f)
    n = np.sqrt(nx * nx + ny * ny + nz * nz)
    L = np.array([-1.0, -1.0, 1.4]); L /= np.linalg.norm(L)
    lit = np.clip((nx * L[0] + ny * L[1] + nz * L[2]) / n, 0, 1)
    shade = 0.35 + 0.65 * lit
    if tint:
        t = np.clip(f / 4.0 / 64.0, 0, 4.999)
        i = np.floor(t).astype(int); k = (t - i)[..., None]
        col = BANDS[i] * (1 - k) + BANDS[i + 1] * k
    else:
        col = np.full(f.shape + (3,), 200, np.float32)
    return Image.fromarray(np.clip(col * shade[..., None], 0, 255).astype(np.uint8))


def overlay_image(m, h, px=8):
    """The tile art lit by the heights: what the 2D map looks like with this relief."""
    t = np.asarray(tiles_image(m, px)).astype(np.float32)
    f = _upsample(h, px) * 4.0
    gy, gx = np.gradient(f, 256.0 / px)
    nx, ny, nz = -gx, -gy, np.ones_like(f)
    n = np.sqrt(nx * nx + ny * ny + nz * nz)
    L = np.array([-1.0, -1.0, 1.4]); L /= np.linalg.norm(L)
    lit = np.clip((nx * L[0] + ny * L[1] + nz * L[2]) / n, 0, 1)
    flat = L[2]
    shade = np.clip(0.55 + 0.9 * (lit - flat) + 0.45, 0.25, 1.4)
    # height bands as contour lines every half rung
    lines = (np.abs(((f / 4.0) / 32.0) - np.round((f / 4.0) / 32.0)) < 0.06)
    out = t * shade[..., None]
    out[lines] = out[lines] * 0.55
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))


def crop_playable(im, m, px, pad=1):
    x0 = max(0, m.X - pad) * px; y0 = max(0, m.Y - pad) * px
    x1 = min(64, m.X + m.W + pad) * px; y1 = min(64, m.Y + m.H + pad) * px
    return im.crop((x0, y0, x1, y1))


def main():
    args = sys.argv[1:]
    out, px = ".", 8
    ids = []
    i = 0
    while i < len(args):
        if args[i] == "--out":
            out = args[i + 1]; i += 2
        elif args[i] == "--px":
            px = int(args[i + 1]); i += 2
        else:
            ids.append(args[i]); i += 1
    os.makedirs(out, exist_ok=True)
    for s in ids:
        m = C.Map(s)
        crop_playable(tiles_image(m, px), m, px).save(os.path.join(out, s + "_tiles.png"))
        if m.h is not None:
            crop_playable(relief_image(m.h, px), m, px).save(os.path.join(out, s + "_relief.png"))
            crop_playable(overlay_image(m, m.h, px), m, px).save(os.path.join(out, s + "_over.png"))
        print(s, "written")


if __name__ == "__main__":
    main()


def compare_image(m, gens, px=8, labels=None):
    """Tiles, then the real relief (when the map has one), then each generated
    relief, side by side, cropped to the playable rectangle."""
    panels = [crop_playable(tiles_image(m, px), m, px)]
    if m.h is not None:
        panels.append(crop_playable(relief_image(m.h, px), m, px))
    for g in gens:
        panels.append(crop_playable(relief_image(g, px), m, px))
    w, h = panels[0].size
    out = Image.new("RGB", (len(panels) * (w + 8) - 8, h), "white")
    for i, p in enumerate(panels):
        out.paste(p, (i * (w + 8), 0))
    return out
