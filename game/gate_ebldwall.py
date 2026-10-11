#!/usr/bin/env python3
"""G303's two halves: the scene it stages and the claims it checks.

    python3 gate_ebldwall.py maps DIR        write the test mission and its scripts
    python3 gate_ebldwall.py check LOGS DIR  read the runs' logs (shots/g303*.log) and print
                                             "1 message" or "0 why"

THE SCENE. A copy of the skirmish map SCM01EA (a large open map) turned into a single-player
mission, SCG94EA: for each of the three remastered walls (sandbags, chain-link, concrete) a
run of five cells with a spur of two to the north, so the run holds ends, straights and a
junction, and two GoodGuy Medium Tanks four cells south of it, ordered to force-fire on the
second cell of the run. Real shells wear the cell down level by level (the concrete wall has
three levels, the chain-link two, the sandbags one) until it is gone. Every remastered
structure type the SCG90EA scene holds stays on that map for the light-pass claims, which run
there.

The script dumps the system's state every ten ticks (ebldump, walldump) with shots on the
way; the engine prints every stage change as it happens (EBLD|hit, damaged, wreck, sink,
gone).
"""
import json
import os
import re
import sys

KINDS = ("SBAG", "CYCL", "BRIK")
LEVELS = {"SBAG": 1, "CYCL": 2, "BRIK": 3}
ORIGIN = {"SBAG": (12, 30), "CYCL": (26, 30), "BRIK": (40, 30)}
TAIL = ["", "[TERRAIN]", "", "[SMUDGE]", "", "[Triggers]", "", "[CellTriggers]", "",
        "[Teams]", "", "[Waypoints]", "", "[Base]", "Count=0", ""]


def cell(x, y):
    return y * 64 + x


def layout(kind):
    x0, y0 = ORIGIN[kind]
    run = [(x0 + i, y0) for i in range(5)]
    spur = [(x0 + 2, y0 - 1), (x0 + 2, y0 - 2)]
    return run + spur, (x0 + 1, y0)


def maps(out):
    src = os.path.join("missions", "SCM01EA")
    b = open(src + ".BIN", "rb").read()
    ini = open(src + ".INI", encoding="latin-1").read()
    mapsec = re.search(r"\[MAP\].*?(?=\n\[)", ini, re.S).group(0).replace("\r", "").strip()
    os.makedirs(out, exist_ok=True)
    ov, un, info = [], [], {}
    for kind in KINDS:
        cells, target = layout(kind)
        ov += ["%d=%s" % (cell(x, y), kind) for x, y in cells]
        x0, y0 = ORIGIN[kind]
        for tx in (x0 + 1, x0 + 3):
            un.append("%03d=GoodGuy,MTNK,256,%d,0,Guard,None" % (len(un), cell(tx, y0 + 4)))
        info[kind] = dict(cells=cells, target=target)
    text = ["[Basic]", "Player=GoodGuy", "Action=x", "Lose=x", "Win=x", "Brief=x", "Intro=x",
            "BuildLevel=98", "Theme=No theme", "Percent=0", "CarryOverCap=-1",
            "CarryOverMoney=0", "", mapsec, "",
            "[GoodGuy]", "Allies=GoodGuy,Neutral", "MaxBuilding=150", "MaxUnit=150",
            "Credits=0", "Edge=South", "", "[BadGuy]", "Allies=BadGuy", "MaxBuilding=150",
            "MaxUnit=150", "Credits=0", "Edge=North", "",
            "[STRUCTURES]", "", "[UNITS]"] + un + ["", "[INFANTRY]", "", "[OVERLAY]"] + ov + TAIL
    open(os.path.join(out, "SCG94EA.INI"), "w", encoding="latin-1", newline="").write("\r\n".join(text))
    open(os.path.join(out, "SCG94EA.BIN"), "wb").write(b)
    s = ["tick 3", "zoom max", "walldump", "ebldump"]
    for i, kind in enumerate(KINDS):
        tx, ty = info[kind]["target"]
        s += ["cam %d %d" % (tx + 1, ty + 2), "tick 1"]
        for j in (0, 1):
            s += ["lclickobj MTNK %d" % (2 * i + j), "actcell %d %d c" % (tx, ty)]
    for k in range(50):
        s += ["tick 10", "echo g303 dump %d" % k, "walldump", "ebldump"]
        if k in (6, 12, 20):
            x0, y0 = ORIGIN["BRIK"]
            s += ["cam %.1f %.1f" % (x0 + 2.5, y0 + 1.0), "dist 1500",
                  "shot shots/g303_brik_%d.png" % k]
    s.append("quit")
    open(os.path.join(out, "SCG94EA.txt"), "w").write("\n".join(s) + "\n")
    json.dump(info, open(os.path.join(out, "layout.json"), "w"), indent=1)
    # THE LIGHT PASS on SCG90EA's GDI base: the Refinery, the Weapons Factory, the Power
    # Plant and the yard in one frame (run with and without --noebldnormals), then the
    # Barracks twice, four ticks apart, for its flag
    s = ["tick 5", "cam 51.5 49.5", "dist 1500", "tick 1", "shot shots/g303e_light.png",
         "ebldump", "cam 58 54.6", "tick 1", "shot shots/g303e_flag0.png", "ebldump",
         "tick 4", "shot shots/g303e_flag1.png", "ebldump", "quit"]
    open(os.path.join(out, "light.txt"), "w").write("\n".join(s) + "\n")


def rd(path):
    try:
        with open(path, errors="ignore") as f:
            return f.read().splitlines()
    except OSError:
        return []


def wall_key(x, y):
    return 9000000000 + y * 512 + x


def check(logs, d):
    A, A2, K = (rd(os.path.join(logs, n + ".log")) for n in ("g303a", "g303a2", "g303k"))
    E, N = rd(os.path.join(logs, "g303e.log")), rd(os.path.join(logs, "g303n.log"))
    info = json.load(open(os.path.join(d, "layout.json")))
    why = []

    def need(c, m):
        if not c:
            why.append(m)
        return c
    said = []
    for kind in KINDS:
        tx, ty = info[kind]["target"]
        key = wall_key(tx, ty)
        ev = {}
        for l in A:
            m = re.match(r"^EBLD\|(hit|damaged|wreck|sink|gone)\|key=(\d+)\|(.*)", l)
            if m and int(m.group(2)) == key:
                ev.setdefault(m.group(1), []).append(l)
        lv = LEVELS[kind]
        wr = [int(re.search(r"pieces=(\d+)", l).group(1)) for l in ev.get("wreck", [])]
        need(len(wr) == 1 and wr[0] > 0, "%s: the shot cell's wreck %s, want one with pieces" % (kind, wr))
        need("sink" in ev and "gone" in ev, "%s: its wreck never sank and went (%s)" % (kind, sorted(ev)))
        if lv >= 2:
            dm = ev.get("damaged", [])
            need(len(dm) == 1, "%s: %d damaged stages for the shot cell, want 1" % (kind, len(dm)))
        else:
            need("damaged" not in ev, "%s: sandbags have no level between whole and gone, yet "
                 "it was damaged" % kind)
        if lv >= 3:
            hits = [l for l in ev.get("hit", []) if "|chip=" in l and "|chip=none" not in l]
            need(hits, "%s: the concrete wall's first level knocked no chip off" % kind)
        # the walldump says the cell is gone after the wreck
        gone = [l for l in A if l.startswith("WALLDUMP|%d,%d|" % (tx, ty))]
        need(gone and len(gone) < sum(1 for l in A if l.startswith("WALLDUMP-END")),
             "%s: the shot cell never left the wall dump" % kind)
        said.append("%s %s" % (kind, "/".join(k for k in ("hit", "damaged", "wreck", "sink", "gone") if k in ev)))
    # the light pass's normal target and the flag, on SCG90EA
    nrm = [re.match(r"^EBLDNRM\|on=(\d)\|objects=(\d+)$", l) for l in E]
    nrm = [m for m in nrm if m]
    need(nrm and all(m.group(1) == "1" for m in nrm) and max(int(m.group(2)) for m in nrm) >= 10,
         "the remasters drew no normals into the light pass's target %s" % [m.group(0) for m in nrm[:2]])
    nn = [re.match(r"^EBLDNRM\|on=(\d)\|objects=(\d+)$", l) for l in N]
    nn = [m for m in nn if m]
    need(nn and all(m.group(1) == "0" and m.group(2) == "0" for m in nn),
         "--noebldnormals still drew normals")
    wv = [re.match(r"^EBLDWAVE\|PYLE\|waves=1\|tris=(\d+),(\d+)\|posed=([-0-9.]+),", l) for l in E]
    wv = [m for m in wv if m]
    posed = sorted(set(float(m.group(3)) for m in wv))
    need(wv and int(wv[0].group(1)) > 0 and len(posed) >= 2 and posed[0] > 0,
         "the Barracks' flag was not posed at several times: %s" % [m.group(0) for m in wv[:3]])
    diff = -1
    try:
        from PIL import Image
        import numpy as np
        a = np.asarray(Image.open(os.path.join(logs, "g303e_light.png")).convert("RGB")).astype(int)
        b = np.asarray(Image.open(os.path.join(logs, "g303n_light.png")).convert("RGB")).astype(int)
        diff = int((np.abs(a - b).max(axis=2) > 8)[:, :1120].sum())
    except Exception as e:
        why.append("the light frames could not be compared (%s)" % e)
    need(diff >= 2000, "the remasters' normals changed %d pixels by more than 8 levels, want 2000" % diff)
    # determinism, and Classic stays out
    ours = ("EBLD|", "EBLDOBJ|", "WALLDUMP|")
    la, lb = [l for l in A if l.startswith(ours)], [l for l in A2 if l.startswith(ours)]
    need(la and la == lb, "two runs of SCG94EA printed different lines")
    same = 0
    for k in (6, 12, 20):
        try:
            same += open(os.path.join(logs, "g303_brik_%d.png" % k), "rb").read() == \
                open(os.path.join(logs, "g303r_brik_%d.png" % k), "rb").read()
        except OSError:
            pass
    need(same == 3, "only %d of the 3 shot pairs of the two runs are identical" % same)
    need(not any(l.startswith(("EBLD|hit", "EBLD|wreck", "EBLD|damaged")) for l in K),
         "Classic: a wall cell was staged by the system")
    if why:
        more = " (and %d more)" % (len(why) - 6) if len(why) > 6 else ""
        print("0 " + "; ".join(why[:6]) + more)
    else:
        print("1 real shells wore the shot cell of each wall down and blew it apart (%s), "
              "every wreck sank and went; the remasters drew their normals into the light "
              "pass's target (%d whole buildings, %d pixels changed against --noebldnormals) "
              "and the Barracks' flag was posed at %d different times; two runs are one run "
              "and Classic stayed out of it" % ("; ".join(said),
                                                max(int(m.group(2)) for m in nrm), diff,
                                                len(posed)))


if __name__ == "__main__":
    if sys.argv[1] == "maps":
        maps(sys.argv[2])
    else:
        check(sys.argv[2], sys.argv[3])
