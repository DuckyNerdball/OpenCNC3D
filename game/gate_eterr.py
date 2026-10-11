#!/usr/bin/env python3
"""G307's two halves: the scenes it stages and the claims it checks.

    python3 gate_eterr.py maps DIR        write the test missions and their scripts
    python3 gate_eterr.py check DIR       read the runs' logs and shots in DIR and print
                                          "1 message" or "0 why"

THE REMASTERED TERRAIN OBJECTS AND CELL DECORATIONS under Enhanced: the desert rocks
(ROCK1..ROCK7), the blossom trees (SPLIT2, SPLIT3, a look per theater, an idle loop and its
spore bursts), the bonus crates (WCRATE, SCRATE, drawn until opened), the fields and
haystacks placed as overlays (V12..V18) and the crashed Orca on rock patch P04 (a look per
theater).

THE SCENES, one test mission per theater, each a shipped mission's own map and pack (so
the ground drawn is the ground the engine reads) with nothing on it but what is tested, on
a block of clear cells:
  SCG85EA  temperate (SCG10EA's map): the seven field overlays in a row, both crates, a
           SPLIT2 and a SPLIT3, a GoodGuy jeep ordered onto the wooden crate, which it
           opens, and the map's own P04 cell beside the block.
  SCG86EA  desert (SCB33EA's map): the seven rocks, a SPLIT3, both crates, and the map's
           own P04 cells (rock patches in its ground).
  SCG87EA  winter (SCW01EA's map): a SPLIT2 and a SPLIT3.
And the remastered trees (TREE_FILE): in the temperate scene a T01 and a T16 drawn whole from
T01_TEM (T16 through the engine's alias), a T02, a T05 and a T07 from their own _TEM files;
the fir T06, the broadleaf trees and saplings T03, T10 to T15, T17 from their _TEM files; in the desert
scene the cacti T04 and T09 and the acacias T08 and T18 from their _DES files and a T01 from
no remastered file (the desert's own mesh); in the winter one a T16, a T02 and a T13 from
none (no winter look yet). Every one drawn from its file carries its wind and drew with it on.

Every run dumps `eterrdump` (the cell decorations: which model each type draws with and
where every remastered cell was put) and `ebldump` (EBLDART: which file each terrain type
attached from and how many whole draws it had; EBLDCLIP: the idle clip's frame for each
tree; EBLDEMIT: the spore bursts) after a shot.
"""
import json
import os
import re
import sys

TAIL = ["", "[SMUDGE]", "", "[Triggers]", "", "[CellTriggers]", "", "[Teams]", "",
        "[Waypoints]", "", "[Base]", "Count=0", ""]
ROCKS = ["ROCK%d" % i for i in range(1, 8)]
# the remastered trees: the type, the file it draws from and the one theater that file is
TREE_FILE = {"T01": ("T01", "TEM"), "T16": ("T01", "TEM"), "T02": ("T02", "TEM"),
             "T05": ("T05", "TEM"), "T07": ("T07", "TEM"), "T04": ("T04", "DES"),
             "T09": ("T09", "DES"), "T08": ("T08", "DES"), "T18": ("T18", "DES")}
TREE_FILE.update({t: (t, "TEM") for t in ("T03", "T06", "T10", "T11", "T12", "T13", "T14",
                                          "T15", "T17")})
FIELDS = ["V12", "V13", "V14", "V15", "V16", "V17", "V18"]

# where each scene's block of clear cells starts (found once in the map's BIN: sixteen by
# ten cells whose template is clear, nearest the map's middle) and what stands on it
SCENES = {
    "SCG85EA": dict(src="SCG10EA", theater="TEM", at=(26, 45), wreck=(38, 44)),
    "SCG86EA": dict(src="SCB33EA", theater="DES", at=(5, 20), wreck=(32, 20)),
    "SCG87EA": dict(src="SCW01EA", theater="WIN", at=(24, 27), wreck=None),
}


def cell(x, y):
    return y * 64 + x


def clear_block(b, x0, y0, w=16, h=10):
    return all(b[cell(x0 + i, y0 + j) * 2] in (0, 0xFF) for i in range(w) for j in range(h))


def layout(name):
    """(overlay cells, terrain cells, units, the jeep's crate, the camera) of a scene."""
    x0, y0 = SCENES[name]["at"]
    ov, te, un, crate, trees = [], [], [], None, []
    if name == "SCG85EA":
        ov += [(x0 + 1 + i, y0 + 1, f) for i, f in enumerate(FIELDS)]
        ov += [(x0 + 10, y0 + 1, "WCRATE"), (x0 + 12, y0 + 1, "SCRATE")]
        te += [(x0 + 2, y0 + 4, "SPLIT2"), (x0 + 5, y0 + 4, "SPLIT3")]
        # the remastered spruce, T01 and T16 drawn from its one file (T01_TEM)
        te += [(x0 + 11, y0 + 5, "T01"), (x0 + 12, y0 + 7, "T16")]
        # and the other remastered conifers, each its own temperate file
        te += [(x0 + 14, y0 + 2, "T02"), (x0 + 14, y0 + 5, "T05"), (x0 + 1, y0 + 7, "T07")]
        # the broadleaf trees and saplings along the block's foot and its top corner
        te += [(x0 + 3, y0 + 8, "T03"), (x0 + 5, y0 + 8, "T10"), (x0 + 8, y0 + 8, "T11"),
               (x0 + 11, y0 + 8, "T12"), (x0 + 13, y0 + 8, "T13"), (x0 + 15, y0 + 8, "T14"),
               (x0 + 13, y0 + 0, "T15"), (x0 + 15, y0 + 0, "T17"), (x0 + 0, y0 + 0, "T06")]
        # a small Tiberium field beside the trees: its crystals' ground cracks are the
        # look the trees' stain is held against
        ov += [(x0 + 7 + i, y0 + 4 + j, "TI%d" % (1 + (3 * i + j) % 12))
               for i in range(2) for j in range(3)]
        un.append(("JEEP", x0 + 10, y0 + 6))
        crate = (x0 + 10, y0 + 1)
    elif name == "SCG86EA":
        xs = [1, 4, 7, 10, 12]
        te += [(x0 + xs[i], y0 + 1, ROCKS[i]) for i in range(5)]
        # THE BLOSSOM TREE STANDS A CELL EAST OF THE CAMERA, not four. At four the tree
        # and its ground stain sat in the east twelfth of the frame, which the Enhanced
        # sidebar's panel now covers at 1280x720: 119 stain pixels showed past its edge
        # and the stain claim, 300 a tree, failed with every stain drawn. One cell east it
        # stands on open sand between ROCK7's east end and the T01.
        te += [(x0 + 1, y0 + 5, "ROCK6"), (x0 + 5, y0 + 5, "ROCK7"), (x0 + 8, y0 + 5, "SPLIT3")]
        ov += [(x0 + 2, y0 + 8, "WCRATE"), (x0 + 4, y0 + 8, "SCRATE")]
        # a T01 on a desert map is the cartridge's desert tree, never the spruce
        te += [(x0 + 9, y0 + 7, "T01")]
        # the remastered cacti, the desert's own look of T04 and T09
        te += [(x0 + 13, y0 + 8, "T04"), (x0 + 7, y0 + 8, "T09")]
        # and the desert acacias: T08's desert look and T18
        te += [(x0 + 13, y0 + 3, "T18"), (x0 + 15, y0 + 6, "T08")]
    else:
        te += [(x0 + 3, y0 + 3, "SPLIT2"), (x0 + 7, y0 + 3, "SPLIT3")]
        # no winter look of the spruce yet: winter keeps the rebuilt tree
        te += [(x0 + 10, y0 + 4, "T16"), (x0 + 12, y0 + 4, "T02"), (x0 + 14, y0 + 4, "T13")]
    return ov, te, un, crate, (x0 + 7.0, y0 + 3.5)


def maps(out):
    os.makedirs(out, exist_ok=True)
    plan = {}
    for name, sc in SCENES.items():
        src = os.path.join("missions", sc["src"])
        b = open(src + ".BIN", "rb").read()
        if not clear_block(b, *sc["at"]):
            sys.exit("gate_eterr: %s's block at %s is not clear ground any more" % (sc["src"], sc["at"]))
        ini = open(src + ".INI", encoding="latin-1").read()
        mapsec = re.search(r"\[MAP\].*?(?=\n\[)", ini, re.S).group(0).replace("\r", "").strip()
        ov, te, un, crate, cam = layout(name)
        units = ["%03d=GoodGuy,%s,256,%d,0,Guard,None" % (i, t, cell(x, y))
                 for i, (t, x, y) in enumerate(un)]
        text = ["[Basic]", "Player=GoodGuy", "Action=x", "Lose=x", "Win=x", "Brief=x",
                "Intro=x", "BuildLevel=98", "Theme=No theme", "Percent=0",
                "CarryOverCap=-1", "CarryOverMoney=0", "", mapsec, "",
                "[GoodGuy]", "Allies=GoodGuy,Neutral", "MaxBuilding=150", "MaxUnit=150",
                "Credits=0", "Edge=South", "", "[BadGuy]", "Allies=BadGuy",
                "MaxBuilding=150", "MaxUnit=150", "Credits=0", "Edge=North", "",
                "[STRUCTURES]", "", "[UNITS]"] + units + ["", "[INFANTRY]", "", "[OVERLAY]"]
        text += ["%d=%s" % (cell(x, y), t) for x, y, t in ov]
        text += ["", "[TERRAIN]"] + ["%d=%s,None" % (cell(x, y), t) for x, y, t in te]
        text += TAIL
        open(os.path.join(out, name + ".INI"), "w", encoding="latin-1",
             newline="").write("\r\n".join(text))
        open(os.path.join(out, name + ".BIN"), "wb").write(b)
        plan[name] = dict(overlay=ov, terrain=te, units=un, crate=crate, cam=cam,
                          pack=sc["src"], theater=sc["theater"], wreck=sc["wreck"])
        s = ["tick 3", "cam %.1f %.1f" % cam, "zoom max", "tick 1",
             "shot %s/%s_a.png" % (out, name), "eterrdump", "ebldump"]
        if sc["wreck"]:
            s += ["cam %.1f %.1f" % (sc["wreck"][0] + 0.5, sc["wreck"][1] + 0.5), "tick 1",
                  "shot %s/%s_w.png" % (out, name), "eterrdump", "wreckdump",
                  "cam %.1f %.1f" % cam, "tick 1"]
        head = len(s)
        # the trees' loop: nine dumps a second apart, more than the eight-second loop, so
        # every tree has squeezed and burst once whatever its head start
        for k in range(9):
            s += ["tick 15"]
            if k % 3 == 0:
                s += ["shot %s/%s_t%d.png" % (out, name, k)]
            s += ["ebldump"]
        if crate:
            s += ["lclickobj JEEP 0", "order %d %d" % crate]
            for k in range(12):
                s += ["tick 10", "cratedump"]
            s += ["cam %.1f %.1f" % cam, "tick 1", "shot %s/%s_b.png" % (out, name),
                  "eterrdump", "cratedump"]
        s.append("quit")
        open(os.path.join(out, name + ".txt"), "w").write("\n".join(s) + "\n")
        # the same first frames with the remasters off (_n), in Classic (_c), and the
        # whole Enhanced run again (_r): two runs must be one picture
        for tag in ("n", "c", "r", "s"):
            body = s if tag == "r" else s[:head] + ["eterrdump", "ebldump", "quit"]
            open(os.path.join(out, "%s_%s.txt" % (name, tag)), "w").write(
                "\n".join(l.replace("%s_" % name, "%s_%s_" % (name, tag)) if l.startswith("shot")
                          else l for l in body) + "\n")
    json.dump(plan, open(os.path.join(out, "plan.json"), "w"), indent=1)


def lines(path, tag):
    try:
        return [l.rstrip("\n") for l in open(path, encoding="latin-1") if l.startswith(tag)]
    except OSError:
        return []


def kv(line):
    return dict(p.split("=", 1) for p in line.split("|") if "=" in p)


def changed(a, b, box=None):
    """Pixels that differ between two shots (inside box), and those in the sidebar."""
    from PIL import Image, ImageChops
    try:
        A, B = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    except OSError:
        return -1, -1
    d = ImageChops.difference(A, B).convert("L").point(lambda v: 255 if v > 8 else 0)
    w, h = d.size
    side = d.crop((int(w * 0.875), 0, w, h)).histogram()[255]
    if box:
        d = d.crop(box)
    return d.histogram()[255], side


def dumps(log, tag):
    """The lines of each dump in a log, split at every line starting with `tag`."""
    out, cur = [], None
    for l in open(log, encoding="latin-1"):
        l = l.rstrip("\n")
        if l.startswith(tag):
            cur = [l]
            out.append(cur)
        elif cur is not None:
            cur.append(l)
    return out


def greener(a, b):
    """Of the pixels that differ (by more than 8), the share where the first shot is greener
    than the second by more than it is redder or bluer: an added green glow."""
    from PIL import Image
    import numpy as np
    try:
        A = np.asarray(Image.open(a).convert("RGB")).astype(int)
        B = np.asarray(Image.open(b).convert("RGB")).astype(int)
    except OSError:
        return 0.0
    d = A - B
    m = np.abs(d).max(axis=2) > 8
    if not m.any():
        return 0.0
    g = (d[..., 1] > d[..., 0]) & (d[..., 1] > d[..., 2])
    return float((g & m).sum()) / float(m.sum())


def same_png(a, b):
    from PIL import Image, ImageChops
    try:
        return ImageChops.difference(Image.open(a).convert("RGB"),
                                     Image.open(b).convert("RGB")).getbbox() is None
    except OSError:
        return False


def check(out):
    plan = json.load(open(os.path.join(out, "plan.json")))
    why, facts = [], []
    look = {"TEM": "", "DES": "_DES", "WIN": "_WIN"}
    for name, P in plan.items():
        log = os.path.join(out, name + ".log")
        if not os.path.exists(log):
            why.append("%s: no log" % name)
            continue
        th = P["theater"]
        # (1) THE CELL DECORATIONS, drawn as remasters where their cells are: the first
        #     eterrdump, after the first frame
        ets = dumps(log, "ETERR|")
        if not ets:
            why.append("%s: no eterrdump" % name)
            continue
        first = ets[0]
        at = {}
        for l in first:
            if l.startswith("ETERRAT|"):
                f = l.split("|")
                at[(int(f[2]), int(f[3]))] = (f[1], f[4], kv(l))
        n_ok = 0
        for x, y, t in P["overlay"]:
            if t.startswith("TI"):
                continue
            kind = "field" if t in FIELDS else "crate"
            typ = "V13" if t == "V12" else t
            got = at.get((x, y))
            if not got:
                why.append("%s: %s at %d,%d not drawn as a remaster" % (name, t, x, y))
                continue
            k, ty, d = got
            if k != kind or ty != typ or abs(float(d["ax"]) - x - 0.5) > 1e-3 \
               or abs(float(d["az"]) - y - 0.5) > 1e-3 or abs(float(d["lift"])) > 1e-3:
                why.append("%s: %s at %d,%d drawn as %s %s %s" % (name, t, x, y, k, ty, d))
                continue
            n_ok += 1
        # the wreck view's dump: the map's own P04 cell in the theater's look
        if P["wreck"]:
            wx, wy = P["wreck"]
            hit = None
            for dmp in ets[1:2]:
                for l in dmp:
                    if l.startswith("ETERRAT|wreck|%d|%d|" % (wx, wy)):
                        hit = l
            want = "P04" + ("_DES" if th == "DES" else "")
            if not hit:
                why.append("%s: the P04 cell %d,%d was not drawn as a remaster" % (name, wx, wy))
            else:
                d = kv(hit)
                if hit.split("|")[4] != want or abs(float(d["lift"])) > 1e-3 \
                   or abs(float(d["ax"]) - wx - 0.5) > 1e-3 or abs(float(d["az"]) - wy - 0.5) > 1e-3:
                    why.append("%s: the P04 cell drew as %s" % (name, hit))
                else:
                    n_ok += 1
        # (2) THE TERRAIN OBJECTS, each from its theater's own file and drawn whole: the
        #     first ebldump's EBLDART lines
        eds = dumps(log, "EBLD|")
        arts = {}
        for l in (eds[0] if eds else []):
            if l.startswith("EBLDART|"):
                arts[l.split("|")[1]] = kv(l)
        for x, y, t in P["terrain"]:
            if t in TREE_FILE:
                # THE REMASTERED TREES: each drawn from its own theater's file (T16 through
                # T01's alias), and from no file in any other theater
                base, want_th = TREE_FILE[t]
                want = "%s_%s" % (base, want_th)
                a = arts.get(want) if th == want_th else None
                drawn = int(a.get("drawn", "0")) if a else 0
                other = [k for k, v in arts.items() if k.startswith(base)
                         and k != want and int(v.get("drawn", "0")) > 0]
                if th != want_th:
                    other = [k for k, v in arts.items() if k.startswith(base)
                             and int(v.get("drawn", "0")) > 0]
                if th == want_th and (drawn <= 0 or other):
                    why.append("%s: %s not drawn whole as %s (%d, %s)" % (name, t, want, drawn, other))
                elif th == want_th and (int(a.get("wind", "0")) <= 0
                                        or int(a.get("winddrawn", "0")) <= 0):
                    # its wind: the model carries its weights (the file refuses a count
                    # that does not cover every intact triangle), and the lit draws had
                    # the wind on
                    why.append("%s: %s's wind weights %s, wind-on draws %s" % (
                        name, t, a.get("wind"), a.get("winddrawn")))
                elif th != want_th and other:
                    why.append("%s: %s drew a remastered tree in %s (%s)" % (name, t, th, other))
                else:
                    n_ok += 1
                continue
            name_ = t + (look[th] if t.startswith("SPLIT") else "")
            a = arts.get(name_)
            if not a or int(a.get("drawn", "0")) <= 0:
                why.append("%s: %s not drawn whole as %s" % (name, t, name_))
            else:
                n_ok += 1
        # (3) THE TREES' LOOP: every tree's idle frame advances one second's frames per
        #     second, and no two trees of a scene share a phase
        clips = {}
        bursts = {}
        for dmp in eds:
            for l in dmp:
                if l.startswith("EBLDCLIP|SPLIT"):
                    d = kv(l)
                    clips.setdefault(d["key"], []).append((int(d["intact"]), d["idle"], d["phase"]))
        trees = [t for _, _, t in P["terrain"] if t.startswith("SPLIT")]
        if len(clips) != len(trees):
            why.append("%s: %d trees, %d idle clips" % (name, len(trees), len(clips)))
        for key, seq in clips.items():
            if any(i != "1" for _, i, _ in seq):
                why.append("%s: tree %s's clip is not the idle loop" % (name, key))
            steps = [(b[0] - a[0]) % 120 for a, b in zip(seq[1:], seq[2:])]
            # (the idle clock is engine_time() in float, so a frame boundary can land a
            # rounding either side: 14 to 16)
            if not steps or any(not 14 <= st <= 16 for st in steps) or sum(steps) != 15 * len(steps) \
               and abs(sum(steps) - 15 * len(steps)) > 1:
                why.append("%s: tree %s's frames %s" % (name, key, [q[0] for q in seq]))
        phases = [seq[0][2] for seq in clips.values()]
        if len(set(phases)) != len(phases):
            why.append("%s: two trees share a phase %s" % (name, phases))
        # (4) THE SPORES: by the last dump (nine seconds, more than one loop) every tree's
        #     synced emitters have burst and spawned
        last = {}
        for l in (eds[-1] if eds else []):
            if l.startswith("EBLDEMIT|SPLIT"):
                d = kv(l)
                if int(d["flags"]) & 64:
                    b = last.setdefault(d["key"], [0, 0])
                    b[0] += int(d["bursts"])
                    b[1] += int(d["spawned"])
        for key in clips:
            b = last.get(key, [0, 0])
            if b[0] <= 0 or b[1] <= 0:
                why.append("%s: tree %s never burst (%s)" % (name, key, b))
        # (9) THE GROUND STAIN under every blossom tree: one spread per tree, all its quads,
        #     centred on the roots, which stand inside the tree's one blocked cell (the cell
        #     a row below the one the INI names), lit; none in Classic
        st = [kv(l) | {"x": l.split("|")[2], "y": l.split("|")[3]}
              for l in first if l.startswith("ETERRSTAIN|tree|")]
        trees_at = [(x, y) for x, y, t in P["terrain"] if t.startswith("SPLIT")]
        if len(st) != len(trees_at):
            why.append("%s: %d stains for %d trees" % (name, len(st), len(trees_at)))
        for d in st:
            ax, az = (float(v) for v in d["at"].split(","))
            x, y = int(d["x"]), int(d["y"])
            if (x, y) not in trees_at or d["quads"] != "6" or float(d["lit"]) <= 0.0 \
               or not (x <= ax < x + 1 and y + 1 <= az < y + 2) or not 0.2 < float(d["reach"]) < 0.8:
                why.append("%s: stain %s" % (name, d))
        if trees_at:
            n, side = changed(os.path.join(out, "%s_a.png" % name),
                              os.path.join(out, "%s_s_a.png" % name))
            g = greener(os.path.join(out, "%s_a.png" % name), os.path.join(out, "%s_s_a.png" % name))
            if n < 300 * len(trees_at) or side or g < 0.8:
                why.append("%s: the stains changed %d pixels (%d sidebar), %.0f%% of them greener"
                           % (name, n, side, 100 * g))
            facts.append("%s stains %dpx %.0f%% greener" % (name, n, 100 * g))
        cl0 = os.path.join(out, name + "_c.log")
        cd0 = dumps(cl0, "ETERR|") if os.path.exists(cl0) else []
        cs = [l for l in (cd0[0] if cd0 else []) if l.startswith("ETERRSTAIN|")]
        if cs:
            why.append("%s: Classic drew a stain" % name)
        facts.append("%s %d placed, %d trees" % (name, n_ok, len(clips)))
        # (5) THE PIXELS: the remasters against the same frames with them off; no sidebar
        for v in ("a", "w"):
            if v == "w" and not P["wreck"]:
                continue
            n, side = changed(os.path.join(out, "%s_%s.png" % (name, v)),
                              os.path.join(out, "%s_n_%s.png" % (name, v)))
            if n < (4000 if v == "a" else 800) or side:
                why.append("%s_%s: %d pixels changed, %d in the sidebar" % (name, v, n, side))
            facts.append("%s_%s %dpx" % (name, v, n))
        # (6) CLASSIC: nothing remastered drawn, and (7) two Enhanced runs are one picture
        cl = os.path.join(out, name + "_c.log")
        ce = dumps(cl, "ETERR|") if os.path.exists(cl) else []
        if not ce or kv(ce[0][0]).get("enhanced") != "0" or kv(ce[0][0]).get("drawn") != "0":
            why.append("%s: Classic drew a remastered cell (%s)" % (name, ce[0][0] if ce else "no dump"))
        for l in (dumps(cl, "EBLD|")[0] if os.path.exists(cl) and dumps(cl, "EBLD|") else []):
            if l.startswith("EBLDART|") and kv(l).get("drawn") not in ("0", None):
                why.append("%s: Classic drew %s" % (name, l.split("|")[1]))
        for v in ("a", "t3"):
            if not same_png(os.path.join(out, "%s_%s.png" % (name, v)),
                            os.path.join(out, "%s_r_%s.png" % (name, v))):
                why.append("%s_%s: two runs differ" % (name, v))
    # (8) THE CRATE THE JEEP DROVE ONTO went, and the steel one stayed
    P = plan["SCG85EA"]
    x, y = P["crate"]
    ets = dumps(os.path.join(out, "SCG85EA.log"), "ETERR|")
    lastd = ets[-1] if ets else []
    if any(l.startswith("ETERRAT|crate|%d|%d|" % (x, y)) for l in lastd):
        why.append("the wooden crate is still drawn after the jeep drove onto it")
    if not any(l.startswith("ETERRAT|crate|") for l in lastd):
        why.append("the steel crate went too")
    cd = [kv(l) for l in lines(os.path.join(out, "SCG85EA.log"), "CRATEDUMP|")]
    if not cd or cd[0].get("cells") != "2" or cd[-1].get("cells") != "1":
        why.append("the engine's crates went %s" % [c.get("cells") for c in cd])
    else:
        facts.append("crate opened")
    if why:
        return "0 " + "; ".join(why)
    return "1 " + ", ".join(facts)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "maps":
        maps(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == "check":
        print(check(sys.argv[2]))
    else:
        sys.exit(__doc__)
