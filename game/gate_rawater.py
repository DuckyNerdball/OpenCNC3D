#!/usr/bin/env python3
"""G311: Red Alert's sea under Enhanced (game/water_mod.h on game/ra3d_terrain.h's map).

    python3 gate_rawater.py check DIR

DIR holds the runs gates.sh made (logs and shots, names below). Prints one line,
"1 <message>" or "0 <what failed>", and exits 0 either way: the shell reads the verdict.

What each run is for:
  a.log a.png b.png   Allied 1 (snow) in Enhanced, the cove at 54,56, tick 0 and tick 2
  c.log c.png d.png   the same with water_fx 0: Red Alert's painted water, the control
  e.log               the same mission on the 1995 tiles (texset 1)
  f.log               Allied 5 (temperate) in Enhanced
  g.log               Soviet 7 (interior) in Enhanced: no sea at all
  h.log               Allied 1 in Classic: the sea is an Enhanced thing only
"""
import os
import re
import sys

# The cove on Allied 1 at cam 54,56, 1280x720: open water bounded by the coast on three
# sides (the same rectangle in every frame; the shots are taken with the shroud off).
BOX = (420, 230, 700, 400)


def lines(path, prefix):
    try:
        return [l.rstrip("\n") for l in open(path, errors="replace") if l.startswith(prefix)]
    except OSError:
        return []


def fields(line):
    out = {}
    for part in line.split("|"):
        if "=" in part:
            k, v = part.split("=", 1)
            out[k] = v
    return out


def box_stats(pa, pb):
    from PIL import Image
    import numpy as np
    a = np.asarray(Image.open(pa).convert("RGB").crop(BOX)).astype(int)
    b = np.asarray(Image.open(pb).convert("RGB").crop(BOX)).astype(int)
    d = np.abs(a - b).sum(2)
    return float((d > 6).mean()), float(d.mean()), float(a.mean())


def check(d):
    fails = []
    j = lambda n: os.path.join(d, n)

    # 1. THE CUT: the snow theater's templates that can hold water were found and cut
    census = lines(j("a.log"), "WATER|ra|cut|theater=SNOW")
    if not census:
        fails.append("no cut census for SNOW")
        tmpl = icons = 0
    else:
        f = fields(census[0])
        tmpl, icons = int(f.get("templates", 0)), int(f.get("icons", 0))
        if tmpl < 100 or icons < 500:
            fails.append("SNOW cut found only %d templates / %d icons with water" % (tmpl, icons))

    # 2. THE SOURCE: Allied 1's map, its open water from the 144-texel painting
    src = lines(j("a.log"), "WATER|ra|source|SCG01EA|SNOW|enhanced|")
    if not src:
        fails.append("Allied 1 built no sea source under Enhanced")
        srcMean = None
        wetCells = 0
    else:
        f = fields(src[0])
        wetCells = int(f.get("wet_cells", 0))
        srcMean = f.get("mean")
        if "base=W2 painted" not in src[0]:
            fails.append("the base is not Red Alert's painted W2 (%s)" % src[0])
        if wetCells < 200:
            fails.append("only %d wet cells on Allied 1" % wetCells)

    # 3. THE FIELD: built from that cut, with wet texels, and its grain read off Red
    #    Alert's painting (TD's WATER1 x WATER2 measures 0.452; this must be the source's)
    fl = lines(j("a.log"), "WATER|field|scen=RAG01EA|")
    wet = 0
    if not fl:
        fails.append("no shore field was built for RAG01EA")
    else:
        f = fields(fl[0])
        wet = int(f.get("wet_texels", 0))
        grain = f.get("grain", "").split("/")[0]
        if wet < 10000:
            fails.append("the field holds only %d wet texels" % wet)
        if srcMean is None or grain != srcMean:
            fails.append("the field's grain %s is not the Red Alert painting's mean %s" % (grain, srcMean))

    # 4. DRAWN: the frame says so, and the control says it did not
    if not lines(j("a.log"), "WATER|ra|frame|SCG01EA|sea=1|field=1|"):
        fails.append("Allied 1's frame did not draw the sea")
    if not lines(j("c.log"), "WATER|ra|frame|SCG01EA|sea=0|"):
        fails.append("the water_fx 0 control drew the sea anyway")

    # 5. THE PIXELS: the sea moves between two ticks, the painted control does not, and the
    #    sea is a different picture from the painted water it replaced
    try:
        anim, _, mean = box_stats(j("a.png"), j("b.png"))
        still, _, _ = box_stats(j("c.png"), j("d.png"))
        drawn, drawnMean, _ = box_stats(j("a.png"), j("c.png"))
    except Exception as e:                       # a missing or unreadable shot
        return "0 the shots could not be read (%s)" % e
    if mean < 25:
        fails.append("the cove is dark (mean %.1f)" % mean)
    if anim < 0.25:
        fails.append("the sea did not move between tick 0 and tick 2 (%.0f%% of the cove changed)" % (anim * 100))
    if still > 0.10:
        fails.append("the control moved (%.0f%% changed), so the measure is not the sea" % (still * 100))
    if drawn < 0.5 or drawnMean < 6:
        fails.append("the sea frame is the painted control's picture (%.0f%% differ)" % (drawn * 100))

    # 6. THE 1995 SET, TEMPERATE, INTERIOR AND CLASSIC
    if not lines(j("e.log"), "WATER|ra|source|SCG01EA|SNOW|1995|") or \
       not any("base=W2 1995" in l for l in lines(j("e.log"), "WATER|ra|source|")):
        fails.append("on the 1995 tiles the sea's base is not the 1995 W2")
    fs = lines(j("f.log"), "WATER|ra|source|SCG05EA|TEMPERAT|enhanced|")
    if not fs or "base=W2 painted" not in fs[0] or not lines(j("f.log"), "WATER|ra|frame|SCG05EA|sea=1|field=1|"):
        fails.append("Allied 5's temperate sea was not built and drawn from its painted W2")
    if not lines(j("g.log"), "WATER|ra|none|theater=INTERIOR") or lines(j("g.log"), "WATER|field|"):
        fails.append("Soviet 7's interior did not skip the sea cleanly")
    if lines(j("h.log"), "WATER|ra|frame|") or lines(j("h.log"), "WATER|field|"):
        fails.append("Classic built or drew the Enhanced sea")

    if fails:
        return "0 " + "; ".join(fails)
    return ("1 Red Alert's sea: SNOW cut %d templates / %d icons; Allied 1 %d wet cells, field %d "
            "wet texels, base W2 painted (grain %s, the painting's own); %.0f%% of the cove moves "
            "in two ticks against %.0f%% in the painted control, %.0f%% differs from it; the 1995 "
            "set uses the 1995 W2; Allied 5 temperate drawn; Interior and Classic draw none"
            % (tmpl, icons, wetCells, wet, srcMean, anim * 100, still * 100, drawn * 100))


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] != "check":
        print(__doc__)
        sys.exit(2)
    print(check(sys.argv[2]))
