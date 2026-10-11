#!/usr/bin/env python3
"""Make the shoulder pads' per-side emblems for a Tempest Rising soldier's symbol sheet.

    python3 tools/art/emblem_masks.py \\
        art/models/tempest_rising_gdi_rifleman/textures/T_GDI_Symbol \\
        data/assets/models_fbx/logo_nod_spin.gif

reads <stem>_D.png (the pads, with the GDI eagle painted on one) and the Nod emblem, and
writes four sheets at the colour map's size:

  <stem>_Blank.png    the colour map with the eagle painted out; ue_c3m.py uses it in
                      place of _D when it exists
  <stem>_C.png        the TEAM MASK: the two pads, so they take the side's colour
  <stem>_LogoGDI.png  the eagle and its ring, as a stencil
  <stem>_LogoNod.png  the Nod emblem, the same size, in the same place, as a stencil

ue_c3m.py packs the two stencils into the alphas of the surface and normal maps and flags
the material M3_MAT_EMBLEM; the model shader then paints one of them, by the soldier's
side, in the eagle's own paint colour (M3_LOGO_COLOUR, measured here and printed).

THE EAGLE is every near-white, unsaturated texel inside the disk it sits in; the pads are
the tan texels flood-filled from a seed in each (so the dark straps' tan stitching is not
a pad), closed over the eagle's hole. Painting it out is a diffusion fill from the pad
around it, with the pad's own grain added back from the other pad.

THE NOD EMBLEM is the cartridge's: the first, head-on frame of a render of the mission briefing
screen's spinning 3D logo (BRF_LOGO_NOD), which draws it at twice the size of the sidebar
plate. Its metal rim (the emblem's outline less the red field's, so it is even all round)
and the tail's segments (what is not red inside the field) become a light stencil, the
way Tempest Rising painted the eagle. It is
scaled to the eagle's height and centred on the eagle's centre."""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

# measured on T_GDI_Symbol_D: where the eagle sits, and a point inside each pad
EAGLE_CENTRE, EAGLE_R = (340, 340), 95
PAD_SEEDS = ((290, 120), (230, 420))


def _img(a):
    return Image.fromarray(np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8))


def _arr(im):
    return np.asarray(im, np.float32) / 255.0


def _hull(mask):
    """The convex hull of a mask's set texels, filled."""
    ys, xs = np.nonzero(mask)
    pts = sorted(set(zip(xs.tolist(), ys.tolist())))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, hi = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(hi) >= 2 and cross(hi[-2], hi[-1], p) <= 0:
            hi.pop()
        hi.append(p)
    img = Image.new("L", (mask.shape[1], mask.shape[0]), 0)
    ImageDraw.Draw(img).polygon(lo[:-1] + hi[:-1], fill=255)
    return np.asarray(img) > 127


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    stem, nod_path = argv
    src = Image.open(stem + "_D.png").convert("RGB")
    n = src.size[0]
    d = _arr(src)
    hsv = _arr(src.convert("HSV"))
    h, s, v = hsv[..., 0] * 360.0, hsv[..., 1], hsv[..., 2]
    yy, xx = np.mgrid[:n, :n]
    k = n / 512.0

    # ---- the eagle
    cx, cy = EAGLE_CENTRE[0] * k, EAGLE_CENTRE[1] * k
    disk = (xx - cx) ** 2 + (yy - cy) ** 2 < (EAGLE_R * k) ** 2
    eagle = np.clip((v - 0.6) / 0.15, 0, 1) * (s < 0.22) * disk
    sel = eagle > 0.5
    ys, xs = np.nonzero(sel)
    ex0, ex1, ey0, ey1 = xs.min(), xs.max(), ys.min(), ys.max()
    logo_srgb = d[sel].mean(axis=0)
    logo_lin = np.where(logo_srgb <= 0.04045, logo_srgb / 12.92,
                        ((logo_srgb + 0.055) / 1.055) ** 2.4)

    # ---- the pads
    tan = ((s > 0.14) & (h > 15) & (h < 62)) | sel
    t = Image.fromarray((tan * 255).astype(np.uint8)).filter(ImageFilter.MedianFilter(5))
    pads = Image.new("L", t.size, 0)
    for sx, sy in PAD_SEEDS:
        f = t.copy()
        ImageDraw.floodfill(f, (int(sx * k), int(sy * k)), 128, thresh=0)
        pads = Image.fromarray(np.maximum(_arr(pads), (_arr(f) > 0.4) & (_arr(f) < 0.6)).astype(np.uint8) * 255)
    pads = pads.filter(ImageFilter.MaxFilter(7)).filter(ImageFilter.MinFilter(7))
    pads = Image.fromarray(np.maximum(_arr(pads), sel).astype(np.uint8) * 255)
    team = _arr(pads.filter(ImageFilter.GaussianBlur(0.7)))

    # ---- the eagle painted out: diffusion from the pad around it, then the other pad's grain
    hole = _arr(Image.fromarray((sel * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 0.5
    fill = d.copy()
    known = (~hole).astype(np.float32)
    # onion peel: each pass fills the hole's rim from what is known and adds it to the known
    while (known < 0.5).any():
        num = _arr(_img(fill * known[..., None]).filter(ImageFilter.BoxBlur(3)))
        den = _arr(_img(known).filter(ImageFilter.BoxBlur(3)))
        rim = (known < 0.5) & (den > 0.02)
        fill[rim] = num[rim] / den[rim][..., None]
        known[rim] = 1.0
    for _ in range(6):                 # then smooth the fill against its own border
        est = _arr(_img(fill).filter(ImageFilter.BoxBlur(4)))
        fill = np.where(hole[..., None], est, d)
    blur = _arr(_img(d).filter(ImageFilter.GaussianBlur(2)))
    grain = d - blur
    dy = int(-230 * k)
    dx = int(-50 * k)
    shifted = np.roll(np.roll(grain, dy, axis=0), dx, axis=1)
    clean = np.roll(np.roll(_arr(pads) > 0.5, dy, axis=0), dx, axis=1) & ~np.roll(np.roll(hole, dy, 0), dx, 1)
    blank = np.where(hole[..., None], fill + shifted * clean[..., None], d)

    # ---- the Nod emblem, as a stencil the eagle's size
    nod = Image.open(nod_path)
    nod.seek(0)                                # an animation's first frame is head-on
    nod = nod.convert("RGB")
    na = _arr(nod)
    nh = _arr(nod.convert("HSV"))
    shape = np.abs(na - na[0, 0]).sum(axis=2) > 0.08       # not the backdrop
    hue = nh[..., 0] * 360.0
    red = (nh[..., 1] > 0.5) & ((hue < 25) | (hue > 335))
    # the rim from the outline, so it is even all round rather than carrying the render's
    # bevel and light: the emblem's hull minus the red field's hull
    rim = _hull(shape) & ~_hull(red)
    # the tail: what inside the field is not red, its own shading holes closed
    tail = _hull(red) & ~red
    t_img = Image.fromarray((tail * 255).astype(np.uint8)).filter(ImageFilter.MedianFilter(5))
    t_img = t_img.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5))
    stencil = Image.fromarray(((rim | (_arr(t_img) > 0.5)) * 255).astype(np.uint8))
    bys, bxs = np.nonzero(shape)
    stencil = stencil.crop((bxs.min(), bys.min(), bxs.max() + 1, bys.max() + 1))
    tall = (ey1 - ey0 + 1)
    wide = int(round(stencil.size[0] * tall / stencil.size[1]))
    big = stencil.resize((wide * 4, tall * 4), Image.BICUBIC).filter(ImageFilter.GaussianBlur(3))
    big = _arr(big)
    big = np.clip((big - 0.35) / 0.3, 0, 1)
    small = _img(big).resize((wide, tall), Image.BOX)
    nodm = Image.new("L", (n, n), 0)
    ox = int(round((ex0 + ex1) / 2 - wide / 2))
    oy = int(ey0)
    nodm.paste(small, (ox, oy))

    _img(blank).save(stem + "_Blank.png", optimize=True)
    _img(team).convert("RGB").save(stem + "_C.png", optimize=True)
    _img(eagle).save(stem + "_LogoGDI.png", optimize=True)
    nodm.save(stem + "_LogoNod.png", optimize=True)
    print("EMBLEM|%s|pads %.1f%% of the sheet|eagle %dx%d at %d,%d|nod %dx%d|logo colour linear %.4f %.4f %.4f" % (
        os.path.basename(stem), 100 * team.mean(), ex1 - ex0 + 1, tall, ex0, ey0, wide, tall,
        logo_lin[0], logo_lin[1], logo_lin[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
