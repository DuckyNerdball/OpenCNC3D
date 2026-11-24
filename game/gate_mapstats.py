#!/usr/bin/env python3
"""G432's eyes: the picked country lit, the rest of the map dark, and the page typed.

Usage: gate_mapstats.py campaign.pack before.png after.png SIDE SHAPE CX CY TX TY LINES

before.png is flow_mapcountry.png, the bare map with the picked country drawn over it
and the palette not yet faded; after.png is flow_mapstats.png, the same plate once the
palette has reached DARK_E and the page has finished typing. SHAPE/CX/CY are the
CAMPAIGN|mapcountry line's shape and centre, TX/TY the CAMPAIGN|mapstats line's page
corner, and LINES the number of labelled lines on the page (7 for GDI, 9 for Nod).

The country's own pixels come from the pack, from the same COUNTRYE or COUNTRYA frame
the screen draws, placed the way CC_Draw_Shape's SHAPE_CENTER places it. So the mask is
the country's real outline rather than a box around it.

Prints five numbers on one line:
  lit      percent of the country's brightness kept after the fade (want about 100)
  rest     percent of the rest of the map's brightness kept (want well under 100)
  ink      plate pixels of green ink inside the page block on after.png
  border   plate pixels of bright green on the rest of the map on after.png (want few:
           the bare map's green borders are lit on before.png and DARK_E puts them out)
  click    plate pixels of green ink on the CLICK TO CONTINUE line
"""
import struct
import sys

import numpy as np
from PIL import Image


def read_shape(pack, tag, frame):
    d = open(pack, "rb").read()
    n = struct.unpack_from("<I", d, 12)[0]
    o = 16
    for _ in range(n):
        name = d[o:o + 16].split(b"\0")[0].decode()
        o += 16
        kind, nf, w, h, _x, _y = struct.unpack_from("<6I", d, o)
        o += 24 + 768
        if kind == 0:
            sizes = struct.unpack_from("<%dI" % nf, d, o)
            o += 4 * nf + sum(sizes)
            continue
        if kind == 3:
            continue
        if name == tag:
            px = np.frombuffer(d, np.uint8, w * h, o + frame * w * h).reshape(h, w)
            return px, w, h
        o += nf * w * h
    raise SystemExit("no %s in %s" % (tag, pack))


def plate(path):
    im = np.asarray(Image.open(path).convert("RGB")).astype(int)
    h, w, _ = im.shape
    s = min(w // 320, h // 200)
    x0, y0 = (w - 320 * s) // 2, (h - 200 * s) // 2
    ys = y0 + np.arange(200) * s + s // 2
    xs = x0 + np.arange(320) * s + s // 2
    return im[ys][:, xs]  # 200 x 320 x 3, one sample per plate pixel


pack, before, after = sys.argv[1:4]
side, shape, cx, cy, tx, ty, lines = (int(v) for v in sys.argv[4:11])
px, w, h = read_shape(pack, "COUNTRYA" if side else "COUNTRYE", shape)
mask = np.zeros((200, 320), bool)
ox, oy = cx - w // 2, cy - h // 2
for yy in range(h):
    for xx in range(w):
        X, Y = ox + xx, oy + yy
        if px[yy, xx] and 0 <= X < 320 and 0 <= Y < 200:
            mask[Y, X] = True

pre, post = plate(before), plate(after)
lum_pre, lum_post = pre.sum(axis=2), post.sum(axis=2)

page = np.zeros((200, 320), bool)
page[ty:min(200, ty + 16 + 8 * lines + 8), tx:min(320, tx + 184)] = True
clickline = np.zeros((200, 320), bool)
clickline[186:195, 105:215] = True
text = page | clickline

country = mask & ~text
rest = ~mask & ~text & (lum_pre > 30)
lit = 100 * lum_post[country].sum() // max(1, lum_pre[country].sum())
kept = 100 * lum_post[rest].sum() // max(1, lum_pre[rest].sum())


def green(img, region):
    r, g, b = img[..., 0], img[..., 1], img[..., 2]
    return int((region & (g > 100) & (g > r + 40) & (g > b + 40)).sum())


print(lit, kept, green(post, page), green(post, rest), green(post, clickline))
