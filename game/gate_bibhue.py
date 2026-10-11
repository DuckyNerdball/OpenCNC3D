"""G322: the Enhanced building aprons take no pink cast from the ground under them.

usage: gate_bibhue.py SHOTS_DIR

Three views, each shot three times in one run (gate_bibhue.txt): the apron as the game
draws it (`bibhue 1`, the Tier 2 pass that reads the ground), the apron by the three
fixed-function passes alone (`bibhue 0`, what was drawn before), and no apron
(`bib_enh_strength 0`), so the apron's pixels are the difference of two pictures of one
frame. The apron's footprint is where the old picture differs from the bare ground by more
than 24 levels summed over the channels, eroded by 2 px so edge pixels do not decide it.

  tmpl    SCG15EA, the Temple of Nod at cell 55,56, whose apron lies half on a dirt road
          and half on grass: the view the pink was reported in.
  grass   SCG05EA, the Nod base at the Construction Yard: a map the temperate multiply
          was calibrated on.
  desert  SCB70EA, the Temple of Nod at 3,43: a map the desert multiply was calibrated on.

THE PINK BAND is the one bibenh_mod.h's probe uses: HSV hue below 22 or from 330 degrees,
saturation at least 0.12, value at least 0.10. The road went to hue 13 to 17 under the old
apron; the approved apron on grass is at 40 to 60, the dirt painting itself at 26.

Claims:
  (1) ENGINE STATE: the pack carries a calibration ground per theater; the first shot was
      drawn by the shader (`path=shader`, hue 1), the second by the passes, the third by
      none, each with apron cells on screen; the renderer's own read-back of its scene
      buffer before the post chain finds the shader's apron at most 1% pink in both
      temperate views, and the passes' at least 5%.
  (2) NO PINK ON SCREEN in the temperate views: at most 1% of the apron's pixels in the
      band (the bare ground itself has a fraction of a percent there), where the old
      drawing has at least 10% at the Temple (the arm that shows the gate can see it).
  (3) ROAD GOES TO DIRT, NOT SALMON: at the Temple, over the ground that is not grass,
      the apron's mean colour has a hue between 25 and 60 degrees (the old: below 22).
  (4) THE APPROVED APRON IS UNCHANGED where it was calibrated: over grass in both
      temperate views, and over the whole apron in the desert view, the mean colour is
      within 3 levels a channel of the old drawing.
  (5) NOTHING ELSE MOVES: in the grass and desert views (no water, which animates between
      shots), every pixel more than 4 px from any apron change is the old picture's, to
      within 2 levels.
"""
import os
import re
import sys

import numpy as np
from PIL import Image, ImageFilter

d = sys.argv[1]
bad = []
notes = []


def img(name):
    return np.asarray(Image.open(os.path.join(d, name)).convert("RGB")).astype(np.int32)


def pinkmask(a):
    a = a.astype(np.float64) / 255.0
    mx, mn = a.max(2), a.min(2)
    dd = np.maximum(mx - mn, 1e-9)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    h = np.where(mx == r, 60.0 * np.mod((g - b) / dd, 6.0),
                 np.where(mx == g, 60.0 * ((b - r) / dd + 2.0), 60.0 * ((r - g) / dd + 4.0)))
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0.0)
    return (mx >= 0.10) & (s >= 0.12) & ((h < 22.0) | (h >= 330.0))


def hue(c):
    r, g, b = (float(v) / 255.0 for v in c)
    mx, mn = max(r, g, b), min(r, g, b)
    if mx - mn < 1e-9:
        return 0.0
    if mx == r:
        return (60.0 * ((g - b) / (mx - mn))) % 360.0
    if mx == g:
        return 60.0 * ((b - r) / (mx - mn) + 2.0)
    return 60.0 * ((r - g) / (mx - mn) + 4.0)


def morph(m, size, grow):
    im = Image.fromarray((m * 255).astype(np.uint8))
    im = im.filter(ImageFilter.MaxFilter(size) if grow else ImageFilter.MinFilter(size))
    return np.asarray(im) > 127


def fmt(c):
    return "(%.1f, %.1f, %.1f)" % tuple(c)


for tag, theater in (("tmpl", "TEMPERATE"), ("grass", "TEMPERATE"), ("desert", "DESERT")):
    log = open(os.path.join(d, "g322_%s.log" % tag), errors="replace").read()
    # (1) engine state
    if not re.search(r"bibenh: %s, multiply [\d. ]+, calibrated on [\d.]+ [\d.]+ [\d.]+" % theater, log):
        bad.append("%s: the pack carries no calibration ground for %s (a version 3 pack?)" % (tag, theater))
    dumps = re.findall(r"BIBENH\|path=(\w+)\|have=(\d)\|theater=(\d+)\|cells=(\d+)\|hue=(\d)", log)
    want = [("shader", "1"), ("passes", "0"), ("none", "0")]
    if len(dumps) != 3:
        bad.append("%s: %d bibdump lines, want 3 (is `bibdump` in this binary?)" % (tag, len(dumps)))
    else:
        for i, (p, have, th, cells, h) in enumerate(dumps):
            if (p, h) != want[i] or have != "1" or (i < 2 and int(cells) <= 0):
                bad.append("%s: shot %d drawn by path=%s hue=%s cells=%s, want %s hue %s with cells"
                           % (tag, i + 1, p, h, cells, want[i][0], want[i][1]))
    probes = re.findall(r"BIBENH\|probe\|px=(\d+)\|pink=(\d+)\|", log)
    if len(probes) < 2:
        bad.append("%s: %d probe read-backs, want 2" % (tag, len(probes)))
    elif theater == "TEMPERATE":
        (pn, kn), (po, ko) = [(int(a), int(b)) for a, b in probes[:2]]
        fn, fo = kn / max(pn, 1), ko / max(po, 1)
        notes.append("%s scene buffer pink %.2f%% (passes %.2f%%)" % (tag, 100 * fn, 100 * fo))
        if fn > 0.01:
            bad.append("%s: the scene buffer under the shader's apron is %.2f%% pink (%d of %d), want <= 1%%"
                       % (tag, 100 * fn, kn, pn))
        if fo < 0.05:
            bad.append("%s: the passes' apron is only %.2f%% pink in the scene buffer: the view no longer "
                       "shows the fault this gate is about" % (tag, 100 * fo))

    new, old, gr = img("g322_%s_new.png" % tag), img("g322_%s_old.png" % tag), img("g322_%s_ground.png" % tag)
    chg_old = np.abs(old - gr).sum(2) > 24
    apron = morph(chg_old, 5, False)
    n = int(apron.sum())
    if n < 2000:
        bad.append("%s: the apron covers %d pixels, want >= 2000 (is it on screen?)" % (tag, n))
        continue
    green = (gr[..., 1] > gr[..., 0] + 3) & (gr[..., 1] > gr[..., 2] + 3)
    if theater == "TEMPERATE":
        # (2) no pink on screen
        pn = int((pinkmask(new) & apron).sum())
        po = int((pinkmask(old) & apron).sum())
        pg = int((pinkmask(gr) & apron).sum())
        notes.append("%s pink %d of %d (old %d, bare ground %d)" % (tag, pn, n, po, pg))
        if pn > 0.01 * n:
            bad.append("%s: %d of %d apron pixels in the pink band (%.2f%%), want <= 1%%"
                       % (tag, pn, n, 100.0 * pn / n))
        if tag == "tmpl" and po < 0.10 * n:
            bad.append("tmpl: the old drawing has only %d of %d pink pixels: the view no longer "
                       "shows the fault this gate is about" % (po, n))
        # (4) grass unchanged
        mg = apron & green
        if mg.sum() < 1000:
            bad.append("%s: only %d apron pixels over grass" % (tag, int(mg.sum())))
        else:
            dn, do = new[mg].mean(0), old[mg].mean(0)
            notes.append("%s grass apron %s (old %s)" % (tag, fmt(dn), fmt(do)))
            if np.abs(dn - do).max() > 3.0:
                bad.append("%s: over grass the apron is %s, the old %s: more than 3 levels apart"
                           % (tag, fmt(dn), fmt(do)))
    if tag == "tmpl":
        # (3) the road under the Temple's apron
        mr = apron & ~green
        if mr.sum() < 2000:
            bad.append("tmpl: only %d apron pixels over the road" % int(mr.sum()))
        else:
            cn, co = new[mr].mean(0), old[mr].mean(0)
            hn, ho = hue(cn), hue(co)
            notes.append("road under the apron %s hue %.0f (old %s hue %.0f)" % (fmt(cn), hn, fmt(co), ho))
            if not (25.0 <= hn <= 60.0):
                bad.append("tmpl: over the road the apron is %s, hue %.1f, want 25..60 (dirt, not salmon)"
                           % (fmt(cn), hn))
            if ho >= 22.0:
                bad.append("tmpl: the old drawing over the road is hue %.1f, no longer the salmon this "
                           "gate is about" % ho)
    if tag == "desert":
        dn, do = new[apron].mean(0), old[apron].mean(0)
        notes.append("desert apron %s (old %s)" % (fmt(dn), fmt(do)))
        if np.abs(dn - do).max() > 3.0:
            bad.append("desert: the apron is %s, the old %s: more than 3 levels apart" % (fmt(dn), fmt(do)))
    if tag in ("grass", "desert"):
        # (5) nothing else moves
        near = morph(chg_old | (np.abs(new - gr).sum(2) > 0), 9, True)
        off = (~near) & (np.abs(new - old).max(2) > 2)
        if off.sum() > 0:
            ys, xs = np.nonzero(off)
            bad.append("%s: %d pixels away from any apron changed (first at %d,%d)"
                       % (tag, int(off.sum()), int(xs[0]), int(ys[0])))

if bad:
    print("0 " + "; ".join(bad))
    for b in bad:
        print("BAD " + b)
else:
    print("1 no pink in any apron: " + "; ".join(notes))
for s in notes:
    print("NOTE " + s)
sys.exit(0 if not bad else 1)
