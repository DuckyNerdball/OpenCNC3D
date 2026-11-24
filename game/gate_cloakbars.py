#!/usr/bin/env python3
"""G520 to G524: the health bars and the post chain, and the cloaking shimmer up close.

    python3 gate_cloakbars.py maps DIR             write the test missions and scripts
    python3 gate_cloakbars.py shadow DIR TAG       G520: the ground beside a health bar
    python3 gate_cloakbars.py leak DIR TAG         G521: no bar colour in a cloaked tank
    python3 gate_cloakbars.py grid LOG             G522: the shimmer's grid at the nearest zoom
    python3 gate_cloakbars.py tall DIR LOGDIR      G523: a building's bar at the nearest zoom
    python3 gate_cloakbars.py rim DIR TAG          G524: the rim on the player's own cloaked tank

Each check prints "1 message" or "0 why".

WHAT IS UNDER TEST. A health bar (and a pip row, and a repair wrench) stands in the world
as the cartridge's own quads do, depth tested and depth written. Under Enhanced the post
chain's passes that look at the pixels round a pixel took it for a solid object standing
on the ground:

  G520  the contact march drew a dark slab on the ground beside every bar, and the
        occlusion a soft halo round it (fx_overlay_depth_keep, fx_overlay_stamp);
  G521  the shimmer over the player's cloaked Stealth Tank copied the bar's green into
        the tank beside it (the same copy, read by fx_cloak_shimmer_apply);
  G522  the shimmer's 1995 pixel grew with the zoom, so at the nearest zoom it moved
        whole slabs of a cloaking tank sideways; it now keeps the size it has at the
        default zoom from there in (cloak_shimmer_mask).
  G523  a building's bar has the cartridge's world length, three or four cells for an
        Airfield, and at Enhanced's nearest zooms it ran as a long thin line into the
        corner of the screen; nearer than the default zoom it now shrinks with the
        camera (hb_height_scale), and N64 keeps the cartridge's length.

THE SCENES. Tiberian Dawn: SCG99EA from gate_cloak3d.py (a Nod Stealth Tank and one of
the player's on open grass, both cloaking on their own from frame 2). Red Alert: a flat
temperate field with one of the player's Medium Tanks. Each scene is shot twice, the same
script with the health bars and with --nohb, so what the bar did to the picture is the
difference of two pictures of one frame.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TD_GDI = (20, 34)
TD_NOD = (12, 28)
TD_AFLD = (20, 34)
RA_TANK = (50, 50)
RA_AFLD = (44, 44)
TAIL = ["", "[TERRAIN]", "", "[SMUDGE]", "", "[Triggers]", "", "[CellTriggers]", "",
        "[Teams]", "", "[Waypoints]", "", "[Base]", "Count=0", ""]


def run(cmd):
    subprocess.check_call(cmd)


def maps(out):
    os.makedirs(out, exist_ok=True)
    td = os.path.join(out, "td")
    run([sys.executable, os.path.join(HERE, "gate_cloak3d.py"), "maps", td])
    # G520 (Tiberian Dawn): the player's tank selected while still solid, at the default
    # zoom and the nearest, under Enhanced; and the same under N64, where no light pass
    # runs and the bar must leave the ground as it was
    for mode, pre in (("enh", []), ("n64", ["gfxmode 1"])):
        s = pre + ["cam %d %d" % TD_GDI, "tick 1", "lclickobj STNK:GoodGuy 0", "tick 1",
                   "shot SHOTS/%s_def.png" % mode, "zoom 1", "cam %d %d" % TD_GDI, "tick 1",
                   "shot SHOTS/%s_near.png" % mode, "quit"]
        open(os.path.join(td, "solid_%s.txt" % mode), "w").write("\n".join(s) + "\n")
    # G521 and G522: the player's tank cloaked and selected, at the default zoom and the
    # nearest; the probe after each shot says the shimmer's grid
    s = ["cam %d %d" % TD_GDI, "tick 50", "lclickobj STNK:GoodGuy 0", "tick 1",
         "shot SHOTS/cl_def.png", "cloakprobe", "zoom 1", "cam %d %d" % TD_GDI, "tick 1",
         "shot SHOTS/cl_near.png", "cloakprobe", "quit"]
    open(os.path.join(td, "cloak.txt"), "w").write("\n".join(s) + "\n")

    # G524: the same map twice, each with one of the two tanks moved far off (so both
    # houses still have a unit and the mission runs on), the camera on the one left: the
    # player's own tank, and the Nod one. Shot while cloaking and once cloaked, at the
    # default zoom and the nearest.
    ini = open(os.path.join(td, "SCG99EA.INI"), encoding="latin-1").read()
    far = 50 * 64 + 50
    for side, keep, move in (("own", "GoodGuy", "BadGuy"), ("nod", "BadGuy", "GoodGuy")):
        d = os.path.join(out, "td_" + side)
        os.makedirs(d, exist_ok=True)
        v = re.sub(r"(=%s,STNK,256,)\d+" % move, lambda m: m.group(1) + str(far), ini)
        open(os.path.join(d, "SCG99EA.INI"), "w", encoding="latin-1", newline="").write(v)
        with open(os.path.join(td, "SCG99EA.BIN"), "rb") as f:
            open(os.path.join(d, "SCG99EA.BIN"), "wb").write(f.read())
        at = TD_GDI if keep == "GoodGuy" else TD_NOD
        s = ["cam %d %d" % at, "tick 6", "shot SHOTS/rim_mid.png", "tick 44",
             "shot SHOTS/rim_def.png", "zoom 1", "cam %d %d" % at, "tick 1",
             "shot SHOTS/rim_near.png", "quit"]
        open(os.path.join(d, "rim.txt"), "w").write("\n".join(s) + "\n")

    # G523 (Tiberian Dawn): an Airfield and a Weapons Factory on SCM01EA's ground, each
    # selected in turn, framed up and left of the middle as in the report, at the default
    # zoom and the nearest, under Enhanced and under N64
    src = os.path.join("missions", "SCM01EA")
    ini = open(src + ".INI", encoding="latin-1").read()
    mapsec = re.search(r"\[MAP\].*?(?=\n\[)", ini, re.S).group(0).replace("\r", "").strip()
    tdb = os.path.join(out, "tdb")
    os.makedirs(tdb, exist_ok=True)
    c64 = lambda x, y: y * 64 + x
    text = ["[Basic]", "Player=GoodGuy", "Action=x", "Lose=x", "Win=x", "Brief=x", "Intro=x",
            "BuildLevel=98", "Theme=No theme", "Percent=0", "CarryOverCap=-1",
            "CarryOverMoney=0", "", mapsec, "",
            "[GoodGuy]", "Allies=GoodGuy", "MaxBuilding=150", "MaxUnit=150", "Credits=0",
            "Edge=South", "", "[BadGuy]", "Allies=BadGuy", "MaxBuilding=150", "MaxUnit=150",
            "Credits=0", "Edge=North", "", "[STRUCTURES]",
            "000=GoodGuy,AFLD,256,%d,0,None" % c64(*TD_AFLD),
            "001=GoodGuy,WEAP,256,%d,0,None" % c64(TD_AFLD[0] + 8, TD_AFLD[1]),
            "", "[UNITS]"] + TAIL
    open(os.path.join(tdb, "SCG98EA.INI"), "w", encoding="latin-1",
         newline="").write("\r\n".join(text))
    open(os.path.join(tdb, "SCG98EA.BIN"), "wb").write(open(src + ".BIN", "rb").read())
    for mode, pre in (("enh", []), ("n64", ["gfxmode 1"])):
        s2 = pre + ["cam %d %d" % TD_AFLD, "tick 3"]
        for b, (bx, by) in (("AFLD", TD_AFLD), ("WEAP", (TD_AFLD[0] + 8, TD_AFLD[1]))):
            s2 += ["zoom 3000", "cam %d %d" % (bx, by), "tick 1", "lclickobj %s:GoodGuy 0" % b,
                   "cam %d %d" % (bx + 4, by + 4),
                   "tick 1", "hbdump", "shot SHOTS/%s_%s_def.png" % (mode, b), "zoom 1",
                   "cam %d %d" % (bx + 4, by + 4), "tick 1", "hbdump",
                   "shot SHOTS/%s_%s_near.png" % (mode, b), "deselect"]
        open(os.path.join(tdb, "tall_%s.txt" % mode), "w").write("\n".join(s2 + ["quit"]) + "\n")

    # Red Alert: one Medium Tank on a flat field, selected, at the nearest zoom
    sys.path.insert(0, os.path.join(HERE, "..", "tools", "regressions"))
    from attack_move import packed  # noqa: E402
    ra = os.path.join(out, "ra")
    os.makedirs(ra, exist_ok=True)
    cell = lambda x, y: y * 128 + x
    size = 100
    text = ("[Basic]\nName=bar fixture\nPlayer=Greece\nBuildLevel=98\nNewINIFormat=3\n"
            "[Map]\nTheater=TEMPERATE\nX=1\nY=1\nWidth=%d\nHeight=%d\n" % (size, size)
            + "[Greece]\nCredits=0\nMaxBuilding=150\nMaxUnit=150\nMaxInfantry=150\n"
              "MaxAircraft=150\nTechLevel=10\nAllies=Greece\n"
            + "[USSR]\nCredits=0\nMaxBuilding=150\nMaxUnit=150\nMaxInfantry=150\n"
              "MaxAircraft=150\nTechLevel=10\nAllies=USSR\n"
            + "[Waypoints]\n0=%d\n" % cell(size // 2, size // 2)
            + "[Structures]\n0=Greece,FACT,256,%d,0,None\n1=USSR,FACT,256,%d,0,None\n"
              "2=Greece,AFLD,256,%d,0,None\n"
              % (cell(5, size - 5), cell(size - 5, size - 5), cell(*RA_AFLD))
            + "[Units]\n0=Greece,2TNK,256,%d,0,Guard,None\n" % cell(*RA_TANK)
            + "[Infantry]\n"
            + "[MapPack]\n" + packed(b"\xff" * (128 * 128 * 2) + b"\0" * (128 * 128)) + "\n")
    open(os.path.join(ra, "SCG88EA.INI"), "w").write(text)
    s = ["cam %d %d" % RA_TANK, "tick 5", "lclickobj 2TNK:Greece 0", "tick 1",
         "zoom 1", "cam %d %d" % RA_TANK, "tick 1", "shot SHOTS/enh_near.png", "quit"]
    open(os.path.join(ra, "solid_enh.txt"), "w").write("\n".join(s) + "\n")
    # G523 (Red Alert): the player's own Airfield, framed as above
    s = ["cam %d %d" % RA_AFLD, "tick 5", "zoom 3000", "lclickobj AFLD:Greece 0",
         "cam %d %d" % (RA_AFLD[0] + 4, RA_AFLD[1] + 4), "tick 1", "hbdump",
         "shot SHOTS/enh_AFLD_def.png", "zoom 1", "cam %d %d" % (RA_AFLD[0] + 4, RA_AFLD[1] + 4),
         "tick 1", "hbdump", "shot SHOTS/enh_AFLD_near.png", "quit"]
    open(os.path.join(ra, "tall_enh.txt"), "w").write("\n".join(s) + "\n")


def load(p):
    from PIL import Image
    import numpy as np
    if not os.path.exists(p) or os.path.getsize(p) == 0:
        return None
    return np.asarray(Image.open(p).convert("RGB")).astype(float)


def bar_box(hb, nohb):
    """The bar's own pixels: where the two pictures differ by a lot and the picture with
    the bar is the bar's colour (green, yellow or red over black). Returns the bounding
    box (x0, y0, x1, y1) of the largest column of them, or None."""
    import numpy as np
    d = np.abs(hb - nohb).max(axis=2)
    r, g, b = hb[:, :, 0], hb[:, :, 1], hb[:, :, 2]
    col = (g > 110) & (g > 2.0 * b) & (g > 1.4 * r)
    m = (d > 40) & col
    if m.sum() < 20:
        return None
    xs = np.where(m.sum(axis=0) >= 8)[0]
    if len(xs) == 0:
        return None
    # the longest run of adjacent columns
    runs, start = [], xs[0]
    for a, b2 in zip(xs, xs[1:]):
        if b2 != a + 1:
            runs.append((start, a)); start = b2
    runs.append((start, xs[-1]))
    x0, x1 = max(runs, key=lambda t: t[1] - t[0])
    ys = np.where(m[:, x0:x1 + 1].any(axis=1))[0]
    return int(x0), int(ys.min()), int(x1), int(ys.max())


def shadow(d, tag):
    """G520: beside each bar the ground is the ground. Over the bar's lower two thirds,
    every column from two pixels right of its right edge out to a bar's height (the
    contact march's reach and the occlusion's) is on average no more than 3 levels darker
    in the picture with the bar than in the picture without it. Before: the four to eight
    columns next to the bar 10 to 20 levels darker (a dark slab) under Enhanced."""
    import numpy as np
    out = []
    ok = True
    for name in sorted(os.listdir(os.path.join(d, "hb"))):
        hb = load(os.path.join(d, "hb", name))
        no = load(os.path.join(d, "nohb", name))
        if hb is None or no is None:
            return "0 %s: a shot is missing (%s)" % (tag, name)
        box = bar_box(hb, no)
        if box is None:
            return "0 %s %s: no health bar found in the picture" % (tag, name)
        x0, y0, x1, y1 = box
        h = y1 - y0 + 1
        sy0 = y0 + h // 3
        sx0 = x1 + 2
        sx1 = min(hb.shape[1], sx0 + max(12, h))
        dk = (no[sy0:y1 + 1, sx0:sx1].mean(axis=2) - hb[sy0:y1 + 1, sx0:sx1].mean(axis=2))
        cols = dk.mean(axis=0)
        worst = float(cols.max())
        slab = int((cols > 3.0).sum())
        good = worst <= 3.0
        ok &= good
        out.append("%s bar %dx%d, darkest column beside it %.1f levels, %d columns over 3%s"
                   % (name[:-4], x1 - x0 + 1, h, worst, slab, "" if good else " (FAIL)"))
    return ("1 " if ok else "0 ") + tag + ": " + "; ".join(out)


def leak(d, tag):
    """G521: no pixel of the bar's colour away from the bar. Over the cloaked tank's
    selection box (the bar's right edge plus four pixels to the box's right), no pixel is
    the bar's green (green over 110 and over twice red and blue). Before: 12 to 30 such
    pixels at both zooms, copied into the tank by the shimmer."""
    out = []
    ok = True
    for name in ("cl_def.png", "cl_near.png"):
        hb = load(os.path.join(d, "hb", name))
        no = load(os.path.join(d, "nohb", name))
        if hb is None or no is None:
            return "0 %s: a shot is missing (%s)" % (tag, name)
        box = bar_box(hb, no)
        if box is None:
            return "0 %s %s: no health bar found in the picture" % (tag, name)
        x0, y0, x1, y1 = box
        h = y1 - y0 + 1
        r, g, b = hb[:, :, 0], hb[:, :, 1], hb[:, :, 2]
        green = (g > 110) & (g > 2.0 * r) & (g > 2.0 * b)
        reg = green[max(0, y0 - h // 2):y1 + h, x1 + 4:min(hb.shape[1], x1 + 4 + 3 * h)]
        n = int(reg.sum())
        ok &= n == 0
        out.append("%s %d bar-coloured pixels in the tank" % (name[:-4], n))
    return ("1 " if ok else "0 ") + tag + ": " + "; ".join(out)


def grid(log):
    """G522: the shimmer reached the tank at both zooms, and its 1995 pixel at the nearest
    zoom is at most 1.05 times the default one's (the camera tilts a little as it nears,
    which alone gives 1.02; before: 1.56 times, 3000/1920)."""
    try:
        text = open(log, errors="ignore").read()
    except OSError:
        return "0 G522: no log"
    rows = re.findall(r"CLOAK3D\|frame\|[^\n]*?shimmer=(\d+)\|pix=([0-9.]+)", text)
    if len(rows) < 2:
        return "0 G522: the probe gave no shimmer grid (a build without it)"
    (s0, p0), (s1, p1) = rows[0], rows[1]
    p0, p1 = float(p0), float(p1)
    if int(s0) < 1 or int(s1) < 1 or p0 <= 0:
        return "0 G522: the shimmer did not reach the cloaked tank (%s, %s)" % (s0, s1)
    ratio = p1 / p0
    msg = "G522: the shimmer's 1995 pixel %.2f at the default zoom, %.2f at the nearest " \
          "(%.2f of it)" % (p0, p1, ratio)
    return ("1 " if ratio <= 1.05 else "0 ") + msg


def rim_pixels(img):
    """The rim's pale blue: blue over 140 and well over red, green over red. The grass,
    the tank's own paint and the shimmer's copies of the ground have none of it. Counted
    in the middle of the picture only, where the camera puts the tank: the sidebar at
    the right edge carries a few pixels of that colour of its own."""
    h, w = img.shape[0], img.shape[1]
    img = img[int(h * 0.15):int(h * 0.85), int(w * 0.2):int(w * 0.75)]
    r, g, b = (img[:, :, i].astype(int) for i in range(3))
    return int(((b > 140) & (b > r + 35) & (g > r + 15)).sum())


def rim(d, tag):
    """G524: the player's own cloaked Stealth Tank wears a flickering pale rim, so it can
    always be found, and a cloaked enemy never does. Round the player's tank, at least 10
    rim pixels while cloaking (the rim comes in with the cloak) and 50 once cloaked and at
    the nearest zoom (measured: 17, 121, 186); none at all in the
    same shots of the Nod tank, which would give a hidden enemy away. Before the rim: none
    in either."""
    out = []
    ok = True
    for side in ("own", "nod"):
        ns = []
        for name in ("rim_mid.png", "rim_def.png", "rim_near.png"):
            img = load(os.path.join(d, "td_" + side, "shots", name))
            if img is None:
                return "0 %s: a shot is missing (td_%s %s)" % (tag, side, name)
            n = rim_pixels(img)
            ns.append(n)
            want = 10 if name == "rim_mid.png" else 50
            ok &= (n >= want) if side == "own" else (n == 0)
        out.append("%s tank %s rim pixels (cloaking, cloaked, nearest)" % (
            "the player's" if side == "own" else "the Nod", "/".join(map(str, ns))))
    return ("1 " if ok else "0 ") + tag + ": " + "; ".join(out)


def bar_extent(hb, nohb):
    """The rows the health bar covers: pixels that differ by a lot between the two
    pictures and are the bar's colour (whatever its slant). (top, bottom) or None."""
    import numpy as np
    d = np.abs(hb - nohb).max(axis=2)
    r, g, b = hb[:, :, 0], hb[:, :, 1], hb[:, :, 2]
    m = (d > 40) & (((g > 110) & (g > 2.0 * b) & (g > 1.4 * r))
                    | ((r > 150) & (b < 90)))
    ys = np.where(m.any(axis=1))[0]
    if len(ys) < 3:
        return None
    return int(ys.min()), int(ys.max())


def tall(d, logdir):
    """G523, per run (its log hb-tall_*.log and its shots): (1) ENGINE: at the default zoom
    each bar is the cartridge's length, dimw/24 cells; at the nearest zoom under Enhanced it
    is that times the camera's distance over the default's, and under N64 the cartridge's
    length still; (2) PICTURE: under Enhanced the bar at the nearest zoom is at most 1.15
    times as tall on screen as at the default zoom, and its top is inside the picture (row
    2 or lower). Before: the length unchanged at the nearest zoom, and the bar up to 1.5
    times as tall on screen and running off the top of the picture."""
    out, ok = [], True
    for log in sorted(f for f in os.listdir(logdir) if f.startswith("hb-tall_") and f.endswith(".log")):
        mode = log[len("hb-tall_"):-4]
        text = open(os.path.join(logdir, log), errors="ignore").read()
        dumps = re.findall(r"HBDUMP-BEGIN[^\n]*\n(.*?)HBDUMP-END", text, re.S)
        cams = [float(x) for x in re.findall(r"CAM\|zoom\|[^\n]*?dist=([0-9.]+)", text)]
        if not dumps or len(dumps) != len(cams):
            return "0 G523 %s: %d bar dumps for %d zooms" % (mode, len(dumps), len(cams))
        for k, dump in enumerate(dumps):
            hb = re.search(r"HB\|(\w+)\|\w+\|[^\n]*?sel=1\|dimw=(\d+)\|[^\n]*?y0=([-0-9.]+)\|"
                           r"yfill=[-0-9.]+\|ytop=([-0-9.]+)", dump)
            if not hb:
                return "0 G523 %s: no selected bar in dump %d" % (mode, k)
            typ, dimw, y0, yt = hb.group(1), int(hb.group(2)), float(hb.group(3)), float(hb.group(4))
            dist = cams[k]
            want = dimw / 24.0
            if mode.endswith("enh") and dist < 3000.0:
                want *= dist / 3000.0
            got = yt - y0
            good = abs(got - want) < 0.002
            ok &= good
            if k % 2 == 1:
                name = "%s_%s" % ("n64" if "n64" in mode else "enh", typ)
                pd = os.path.join(d, mode.split("_")[0], "hb")
                pn = os.path.join(d, mode.split("_")[0], "nohb")
                e0 = bar_extent(load(os.path.join(pd, name + "_def.png")),
                                load(os.path.join(pn, name + "_def.png")))
                e1 = bar_extent(load(os.path.join(pd, name + "_near.png")),
                                load(os.path.join(pn, name + "_near.png")))
                if e0 is None or e1 is None:
                    return "0 G523 %s %s: the bar is not in the picture" % (mode, typ)
                h0, h1 = e0[1] - e0[0] + 1, e1[1] - e1[0] + 1
                pgood = True
                if "enh" in mode:
                    pgood = h1 <= 1.15 * h0 and e1[0] >= 2
                ok &= pgood
                out.append("%s %s length %.2f cells at %.0f (want %.2f), on screen %d px at "
                           "the default zoom and %d px from row %d at the nearest%s"
                           % (mode.split("_")[0].upper() + " " + ("N64" if "n64" in mode else "Enhanced"),
                              typ, got, dist, want, h0, h1, e1[0],
                              "" if good and pgood else " (FAIL)"))
    return ("1 " if ok else "0 ") + "G523: " + "; ".join(out)


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "maps":
        maps(sys.argv[2])
    elif len(sys.argv) >= 4 and sys.argv[1] == "shadow":
        print(shadow(sys.argv[2], sys.argv[3]))
    elif len(sys.argv) >= 4 and sys.argv[1] == "leak":
        print(leak(sys.argv[2], sys.argv[3]))
    elif len(sys.argv) >= 4 and sys.argv[1] == "tall":
        print(tall(sys.argv[2], sys.argv[3]))
    elif len(sys.argv) >= 4 and sys.argv[1] == "rim":
        print(rim(sys.argv[2], sys.argv[3]))
    elif len(sys.argv) >= 3 and sys.argv[1] == "grid":
        print(grid(sys.argv[2]))
    else:
        print(__doc__)
        sys.exit(2)
