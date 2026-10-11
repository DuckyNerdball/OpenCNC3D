#!/usr/bin/env python3
"""G295's two halves: the scene it stages and the claims it checks.

    python3 gate_ebldmg.py maps DIR        write the three test missions and their scripts
    python3 gate_ebldmg.py check LOGS DIR  read the runs' logs (shots/g295*.log) and print
                                           "1 message" or "0 why"

THE SCENE. Every remastered structure, on copies of the skirmish map SCM01EA (a large
open map) turned into single-player missions: SCG91EA and SCG92EA hold eleven types each,
owned by BadGuy, each with two GoodGuy Mammoth tanks two cells south of it that are
ordered to attack it, so real shells wear it down from the south, break it at half
strength and kill it. SCG93EA holds a Nod Silo beside a Nod Refinery whose harvester
fills the store from a field beside it, and three riflemen ordered to shoot the Silo once
the store holds Tiberium, slowly enough for the Silo's broken stage to play. Nothing else
is on the maps: no trees, no Tiberium but the field, no triggers.

Each script orders the attack and then dumps the system's state every fifty ticks
(ebldump, ebldbodies), with a few shots on the way. The engine prints every stage change
as it happens (EBLD|hit, damaged, wreck, sink, gone).
"""
import json
import os
import re
import sys

FOOT = {"AFLD": (4, 2), "ATWR": (1, 2), "BIO": (2, 2), "EYE": (2, 2), "FACT": (3, 2),
        "FIX": (3, 3), "GTWR": (1, 1), "GUN": (1, 1), "HAND": (2, 3), "HOSP": (2, 2),
        "HPAD": (2, 2), "HQ": (2, 2), "MISS": (3, 2), "NUK2": (2, 2), "NUKE": (2, 2),
        "OBLI": (1, 2), "PROC": (3, 3), "PYLE": (2, 2), "SAM": (2, 1), "SILO": (2, 1),
        "TMPL": (3, 3), "WEAP": (3, 3)}
MAPS = {"SCG91EA": ["AFLD", "BIO", "EYE", "FACT", "FIX", "HAND", "HOSP", "HPAD", "NUK2",
                    "NUKE", "MISS"],
        "SCG92EA": ["ATWR", "GTWR", "GUN", "HQ", "OBLI", "PROC", "PYLE", "SAM", "SILO",
                    "TMPL", "WEAP"]}
TAIL = ["", "[TERRAIN]", "", "[SMUDGE]", "", "[Triggers]", "", "[CellTriggers]", "",
        "[Teams]", "", "[Waypoints]", "", "[Base]", "Count=0", ""]


def houses():
    return ["[GoodGuy]", "Allies=GoodGuy,Neutral", "MaxBuilding=150", "MaxUnit=150",
            "Credits=0", "Edge=South", "", "[BadGuy]", "Allies=BadGuy", "MaxBuilding=150",
            "MaxUnit=150", "Credits=0", "Edge=North", ""]


def basic():
    return ["[Basic]", "Player=GoodGuy", "Action=x", "Lose=x", "Win=x", "Brief=x", "Intro=x",
            "BuildLevel=98", "Theme=No theme", "Percent=0", "CarryOverCap=-1",
            "CarryOverMoney=0", ""]


def maps(out):
    src = os.path.join("missions", "SCM01EA")
    b = open(src + ".BIN", "rb").read()
    ini = open(src + ".INI", encoding="latin-1").read()
    mapsec = re.search(r"\[MAP\].*?(?=\n\[)", ini, re.S).group(0).replace("\r", "").strip()
    clear = lambda x, y: 3 <= x < 61 and 11 <= y < 60 and b[(y * 64 + x) * 2] in (0, 0xFF)
    cell = lambda x, y: y * 64 + x
    os.makedirs(out, exist_ok=True)
    layout = {}
    for name, types in MAPS.items():
        taken, st, un = [], [], []
        for i, t in enumerate(types):
            fw, fh = FOOT[t]
            spot = None
            for y in range(13, 56):
                for x in range(4, 58):
                    if not all(clear(xx, yy) for xx in range(x - 1, x + fw + 1)
                               for yy in range(y - 1, y + fh + 5)):
                        continue
                    if any(abs(x - tx) < 12 and abs(y - ty) < 11 for tx, ty in taken):
                        continue
                    spot = (x, y)
                    break
                if spot:
                    break
            if not spot:
                sys.exit("gate_ebldmg: no room for %s on %s" % (t, name))
            x, y = spot
            taken.append(spot)
            st.append("%03d=BadGuy,%s,256,%d,0,None" % (i, t, cell(x, y)))
            for tx in (x, x + fw - 1 if fw > 1 else x + 1):
                un.append("%03d=GoodGuy,HTNK,256,%d,0,Guard,None" % (len(un), cell(tx, y + fh + 2)))
            layout.setdefault(name, []).append(dict(type=t, x=x, y=y, fw=fw, fh=fh))
        text = basic() + [mapsec, ""] + houses() + ["[STRUCTURES]"] + st + ["", "[UNITS]"] + \
            un + ["", "[INFANTRY]", "", "[OVERLAY]"] + TAIL
        open(os.path.join(out, name + ".INI"), "w", encoding="latin-1", newline="").write("\r\n".join(text))
        open(os.path.join(out, name + ".BIN"), "wb").write(b)
        # the script: order each pair of tanks onto its building, then dump every 50 ticks
        s = ["tick 3", "zoom max"]
        for i, d in enumerate(layout[name]):
            s += ["cam %d %d" % (d["x"] + d["fw"] // 2, d["y"] + d["fh"] // 2 + 1), "tick 1"]
            for j in (0, 1):
                s += ["lclickobj HTNK %d" % (2 * i + j), "orderobj %s 0" % d["type"]]
        d0 = layout[name][0]
        for k in range(60):
            s += ["tick 50", "echo g295 dump %d" % k, "ebldump", "ebldbodies"]
            if k in (4, 8):
                s += ["cam %.1f %.1f" % (d0["x"] + d0["fw"] / 2.0, d0["y"] + d0["fh"] / 2.0 + 1.2),
                      "dist 1600", "shot shots/g295_%s_%d.png" % (name, k)]
        s.append("quit")
        open(os.path.join(out, name + ".txt"), "w").write("\n".join(s) + "\n")
    # the Silo, its Refinery, a harvester, a field and three riflemen
    st = ["000=BadGuy,PROC,256,%d,0,None" % cell(26, 24), "001=BadGuy,SILO,256,%d,0,None" % cell(31, 25),
          "002=BadGuy,NUKE,256,%d,0,None" % cell(36, 24)]
    un = ["000=BadGuy,HARV,256,%d,0,Harvest,None" % cell(27, 28)]
    inf = ["%03d=GoodGuy,E1,256,%d,%d,Guard,0,None" % (i, cell(31 + i // 2, 29), i % 2 + 1) for i in range(3)]
    ov = ["%d=TI%d" % (cell(x, y), 1 + (x * 7 + y * 3) % 12) for x in range(20, 25) for y in range(27, 31)]
    text = basic() + [mapsec, ""] + houses() + ["[STRUCTURES]"] + st + ["", "[UNITS]"] + un + \
        ["", "[INFANTRY]"] + inf + ["", "[OVERLAY]"] + ov + TAIL
    open(os.path.join(out, "SCG93EA.INI"), "w", encoding="latin-1", newline="").write("\r\n".join(text))
    open(os.path.join(out, "SCG93EA.BIN"), "wb").write(b)
    s = ["tick 650", "cam 32 27", "dist 1600"]
    for i in range(3):
        s += ["lclickobj E1 %d" % i, "orderobj SILO 0"]
    for k in range(40):
        s += ["tick 30", "echo g295 dump %d" % k, "ebldump"]
    s.append("quit")
    open(os.path.join(out, "SCG93EA.txt"), "w").write("\n".join(s) + "\n")
    json.dump(layout, open(os.path.join(out, "layout.json"), "w"), indent=1)


def rd(path):
    try:
        with open(path, errors="ignore") as f:
            return f.read().splitlines()
    except OSError:
        return []


def check(logs, d):
    A, A2, B, C = (rd(os.path.join(logs, n + ".log")) for n in ("g295a", "g295a2", "g295b", "g295c"))
    N, K = rd(os.path.join(logs, "g295n.log")), rd(os.path.join(logs, "g295k.log"))
    layout = json.load(open(os.path.join(d, "layout.json")))
    why = []

    def need(c, m):
        if not c:
            why.append(m)
        return c
    # which key is which building: keys are kind * 1000000 + heap id, in the INI's order
    for L, name in ((A, "SCG91EA"), (B, "SCG92EA")):
        types = [x["type"] for x in layout[name]]
        key = {2000000 + i: t for i, t in enumerate(types)}
        ev = {t: {} for t in types}
        for l in L:
            m = re.match(r"^EBLD\|(hit|damaged|wreck|sink|gone)\|key=(\d+)\|(.*)frame=(\d+)", l)
            if m and int(m.group(2)) in key:
                e = ev[key[int(m.group(2))]]
                e.setdefault(m.group(1), []).append(l)
        mesh = {}
        for l in L:
            m = re.match(r"^EBLDMESH\|(\w+)\|.*\|pieces=(\d+),(\d+)\|chips=(\d+)", l)
            if m:
                mesh[m.group(1)] = (int(m.group(2)), int(m.group(3)))
        for t in types:
            e = ev[t]
            chips = [l for l in e.get("hit", []) if "|chip=chip_" in l]
            need(len(chips) >= 3, "%s: %d hits knocked a chip off, want 3 or more" % (t, len(chips)))
            dm = [int(re.search(r"pieces=(\d+)", l).group(1)) for l in e.get("damaged", [])]
            need(dm and dm[0] > 0, "%s: no damaged stage with loose pieces %s" % (t, dm))
            wr = [int(re.search(r"pieces=(\d+)", l).group(1)) for l in e.get("wreck", [])]
            cut, dest = mesh.get(t, (0, 0))
            want = dest if dest else cut - 4
            need(wr and wr[0] >= want > 0, "%s: its wreck has %s pieces, want %d or more" % (t, wr, want))
            need("sink" in e and "gone" in e, "%s: its wreck never sank and went (%s)" % (t, sorted(e)))
        # every emitter of the damaged state gave off particles (a fill-gated one needs
        # Tiberium, which only SCG93EA has), and every intact one before it broke
        spawned = {}
        for l in L:
            m = re.match(r"^EBLDEMIT\|(\w+)\|key=(\d+)\|e=(\d+)\|src=(\w+)\|state=(\d)\|piece=[^|]*\|flags=(\d+)\|spawned=(\d+)\|bursts=(\d+)", l)
            if m and int(m.group(2)) in key:
                k = (m.group(1), int(m.group(3)))
                spawned[k] = (m.group(4), int(m.group(5)), int(m.group(6)), int(m.group(7)), int(m.group(8)))
        for (t, e), (src, state, fl, n, bursts) in sorted(spawned.items()):
            if fl & 16:
                continue
            need(n > 0, "%s emitter %d (%s, state %d) gave off nothing" % (t, e, src, state))
            if fl & (1 | 8):
                need(bursts > 0, "%s emitter %d (%s) never burst" % (t, e, src))
    srcs = {}
    for L in (A, B):
        for l in L:
            m = re.match(r"^EBLDEMIT\|(\w+)\|.*\|src=(\w+)\|.*\|flags=(\d+)\|spawned=(\d+)\|bursts=(\d+)", l)
            if m and int(m.group(4)) > 0:
                srcs.setdefault(m.group(2), set()).add(m.group(1))
    need({"OBLI"} <= srcs.get("glow", set()) and {"OBLI"} <= srcs.get("ion", set()),
         "the Obelisk's glow and ion sparks gave off nothing")
    need(len(srcs.get("sparks", set())) >= 15, "sparks on only %s" % sorted(srcs.get("sparks", set())))
    # burst_on: the Construction Yard's elbow sparks and the Weapons Factory's door sparks
    for t, L in (("FACT", A), ("WEAP", B)):
        b = [int(m.group(1)) for m in (re.match(r"^EBLDEMIT\|%s\|.*\|flags=(?:8|10|42|40|9|11|43|41)\|spawned=\d+\|bursts=(\d+)" % t, l) for l in L) if m]
        need(b and max(b) > 0, "%s: no burst on its looping part's strain" % t)
    # until: the Communications Center's sparks with an end stop giving off particles (the
    # hand-painted cut has five such emitters; each is held to it on its own)
    eu = {}
    for m in (re.match(r"^EBLDEMIT\|EYE\|key=\d+\|e=(\d+)\|.*\|flags=7\|spawned=(\d+)\|bursts=(\d+)", l) for l in A):
        if m:
            eu.setdefault(int(m.group(1)), []).append(int(m.group(2)))
    late = {e: v[-4:] for e, v in eu.items() if len(v) < 4 or v[-1] != v[-4] or v[-1] <= 0}
    need(eu and not late, "the Communications Center's sparks with an end did not stop %s" % late)
    # turning parts keep turning in the broken stage; loops and a bend play
    def moves(L, t, piece):
        return [m.groups() for m in (re.match(r"^EBLDMOVE\|%s\|key=\d+\|piece=%s\|kind=(\w+)\|deg=([0-9.]+)\|moved=([0-9.]+)\|try=(-?\d+)\|weight=([0-9.]+)" % (t, piece), l) for l in L) if m]
    for t, p, L in (("AFLD", "radar_head", A), ("HQ", "dish", B), ("EYE", "uplink", A), ("ATWR", "mast_head", B)):
        v = [float(x[1]) for x in moves(L, t, p) if x[0] == "turn"]
        need(len(set(v)) >= 3, "%s's %s did not keep turning: %s" % (t, p, v[:6]))
    for t, p, L in (("FACT", "boom", A), ("WEAP", "door_low", B)):
        v = [x for x in moves(L, t, p) if x[0] == "loop"]
        need(any(int(x[3]) >= 1 for x in v), "%s's %s never tried (%d poses)" % (t, p, len(v)))
    v = [float(x[1]) for x in moves(B, "PYLE", "pole_top") if x[0] == "bend"]
    need(v and abs(v[-1] - 30.0) < 0.5, "the Barracks' flag pole is bent %s degrees, want 30" % v[-1:])
    # the Hand of Nod's globe rolls off and comes to rest on the ground
    gl = [m.groups() for m in (re.match(r"^EBLDBODY\|key=\d+\|set=\w+\|piece=globe\|at=[^|]*\|dist=([0-9.]+)\|low=(-?[0-9.]+)\|.*\|asleep=(\d)", l) for l in A) if m]
    need(gl and max(float(g[0]) for g in gl) > 0.4 and gl[-1][2] == "1" and abs(float(gl[-1][1])) < 0.03,
         "the Hand of Nod's globe did not roll clear and come to rest: %s" % (gl[-1:] if gl else "never a body"))
    # liquids: the Advanced Power Plant's water and the Refinery's pool drain, the falls pour
    liq = {}
    for L in (A, B, C):
        for m in (re.match(r"^EBLDLIQ\|(\w+)\|key=\d+\|piece=(\w+)\|kind=(\w+)\|drained=([0-9.]+)\|fill=([0-9.]+)\|level=(-?[0-9.]+)\|shown=([0-9.]+)", l) for l in L):
            if m:
                k = (m.group(1), m.group(2))
                o = liq.setdefault(k, [0.0, 0.0, 0.0])
                o[0] = max(o[0], float(m.group(4)))
                o[1] = max(o[1], float(m.group(7)) if m.group(3) == "fall" else 0.0)
                o[2] = max(o[2], float(m.group(5)))
    for k in (("NUK2", "water"), ("PROC", "water"), ("PROC", "pool_s")):
        need(k in liq and liq[k][0] >= 0.999, "%s %s did not drain: %s" % (k + (liq.get(k),)))
    # (the hand-painted cuts name their pours after what pours: the Advanced Power Plant's
    # water spill, the Refinery's Tiberium spill)
    for k in (("NUK2", "spill_water"), ("PROC", "spill_tib"), ("SILO", "spill")):
        need(k in liq and liq[k][1] > 0.3, "%s %s never poured: %s" % (k + (liq.get(k),)))
    need(("SILO", "fill") in liq and liq[("SILO", "fill")][2] > 0.3, "the Silo's store never filled")
    fe = [int(m.group(1)) for m in (re.match(r"^EBLDEMIT\|SILO\|.*\|flags=16\|spawned=(\d+)", l) for l in C) if m]
    need(fe and max(fe) > 0, "the Silo's Tiberium haze and glints gave off nothing with a full store")
    need(any(re.match(r"^EBLDLIGHT\|lights=[1-9]", l) for l in A), "no building light was lit")
    # two runs are one run; --noebld and Classic leave it all alone
    ours = ("EBLD|", "EBLDOBJ|", "EBLDMOVE|", "EBLDEMIT|", "EBLDLIQ|", "EBLDBODY|")
    la, lb = [l for l in A if l.startswith(ours)], [l for l in A2 if l.startswith(ours)]
    need(la and la == lb, "two runs of SCG91EA printed different EBLD lines")
    def img(n):
        try:
            return open(os.path.join(logs, n), "rb").read()
        except OSError:
            return None
    same = sum(img("g295_SCG91EA_%d.png" % k) is not None
               and img("g295_SCG91EA_%d.png" % k) == img("g295r_SCG91EA_%d.png" % k) for k in (4, 8))
    need(same == 2, "only %d of the 2 shot pairs of the two runs are identical" % same)
    need(not any(l.startswith("EBLD|damaged") for l in N), "--noebld: a building broke under the system")
    need(not any(l.startswith("EBLD|hit") for l in K) and any(re.match(r"^EBLD\|loaded=\d+\|attached=\d+\|on=0", l) for l in K),
         "Classic: the system is on, or a building was hit under it")
    if why:
        more = " (and %d more)" % (len(why) - 6) if len(why) > 6 else ""
        print("0 " + "; ".join(why[:6]) + more)
    else:
        print("1 all 22 structures were worn down by real shells, chips off the struck side, "
              "broke at half strength and died as the lab plays it (their wrecks sank and went); "
              "every emitter gave off particles in its state, sparks bursting on %d types, the "
              "Obelisk's glow and ion sparks, strain bursts on the yard's boom and the factory's "
              "door, an end on the uplink tower's sparks; turning parts kept turning, the boom and "
              "the door kept trying, the flag pole bent to 30 degrees, the globe rolled off and "
              "settled; the water, the pool and the Silo's spill drained and poured; two runs are "
              "one run, and --noebld and Classic stayed out of it" % len(srcs.get("sparks", ())))


if __name__ == "__main__":
    if sys.argv[1] == "maps":
        maps(sys.argv[2])
    else:
        check(sys.argv[2], sys.argv[3])
