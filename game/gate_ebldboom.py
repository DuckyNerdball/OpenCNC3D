#!/usr/bin/env python3
"""G309's two halves: the scene it stages and the claims it checks.

    python3 gate_ebldboom.py maps DIR        write the two test missions and their scripts
    python3 gate_ebldboom.py check LOGS DIR  read the runs' logs (LOGS/g309*.log) and print
                                             "1 message" or "0 why"

THE SCENE. Copies of two skirmish maps turned into single-player missions: SCG88EA
(temperate, from SCM01EA) holds the Weapons Factory, the Refinery, the Power Plant and the
Construction Yard (GoodGuy's, so they wear the lab's GDI paint; no money to repair), the civilian houses V01 and V04 and
the oil pump V19 (Neutral's), a run of barbed wire and a run of concrete wall; SCG89EA
(desert, from SCM02EA) holds the film studio V37. Two GoodGuy Medium Tanks a few cells south
of each building.

THE SCRIPT takes the buildings one at a time, so each death has the screen to itself and its
moments are known to the tick: the building is shot whole; its two tanks force-fire on a
cell it blocks until it breaks (tickstr: the tick its strength falls below half), then are
stopped; the damaged stage is shot at 1 and 3 seconds; then 1000 points of HE on that cell
(hurtcell) kill it, and the blast is shot 2, 6, 15, 45 and 120 ticks later (0.13, 0.4, 1,
3 and 8 seconds: the lab's Destroy shot at the same moments). A wall cell is knocked down a
level at a time with hurtcell (the concrete wall's three, the wire's one) and shot the same
way. The system's state is dumped after every shot (ebldump, efxdump).

The run folder must hold missions/SCM01EA and SCM02EA and the packs named in gates.sh.
"""
import json
import os
import re
import sys

# bdata.cpp: each building's size and the cells its occupy list blocks (dx, dy)
FOOT = {"WEAP": ((3, 3), [(0, 1), (1, 1), (2, 1), (0, 2), (1, 2), (2, 2)]),
        "PROC": ((3, 3), [(1, 0), (0, 1), (1, 1), (2, 1)]),
        "NUKE": ((2, 2), [(0, 0), (0, 1), (1, 1)]),
        "FACT": ((3, 2), [(0, 0), (1, 0), (2, 0), (0, 1), (1, 1), (2, 1)]),
        "V01": ((2, 2), [(0, 1), (1, 1)]),
        "V04": ((2, 2), [(0, 1), (1, 1)]),
        "V19": ((1, 1), [(0, 0)]),
        "V37": ((4, 2), [(1, 0), (2, 0), (3, 0), (1, 1), (2, 1), (3, 1)])}
# the four structures are the player's (GDI, the lab's own paint), with no money to repair
OWNER = {"WEAP": "GoodGuy", "PROC": "GoodGuy", "NUKE": "GoodGuy", "FACT": "GoodGuy"}
MAPS = {"SCG88EA": ("SCM01EA", ["WEAP", "PROC", "NUKE", "FACT", "V01", "V04", "V19"],
                    {"BARB": 1, "BRIK": 3}),
        "SCG89EA": ("SCM02EA", ["V37"], {})}
# the blast moments, ticks after the killing hit, and the damaged stage's, after the break
BLAST = (2, 6, 15, 45, 120)
HELD = (15, 45)
TAIL = ["", "[TERRAIN]", "", "[SMUDGE]", "", "[Triggers]", "", "[CellTriggers]", "",
        "[Teams]", "", "[Waypoints]", "", "[Base]", "Count=0", ""]


def cell(x, y):
    return y * 64 + x


def maps(out):
    os.makedirs(out, exist_ok=True)
    layout = {}
    for name, (src_name, types, walls) in MAPS.items():
        src = os.path.join("missions", src_name)
        b = open(src + ".BIN", "rb").read()
        ini = open(src + ".INI", encoding="latin-1").read()
        mapsec = re.search(r"\[MAP\].*?(?=\n\[)", ini, re.S).group(0).replace("\r", "").strip()
        mx, my = int(re.search(r"\bX=(\d+)", mapsec).group(1)), int(re.search(r"\bY=(\d+)", mapsec).group(1))
        mw = int(re.search(r"\bWidth=(\d+)", mapsec).group(1))
        mh = int(re.search(r"\bHeight=(\d+)", mapsec).group(1))
        clear = lambda x, y: (mx + 1 <= x < mx + mw - 1 and my + 1 <= y < my + mh - 1
                              and b[(y * 64 + x) * 2] in (0, 0xFF))
        taken = []

        def spot(w, h):
            for y in range(my + 3, my + mh - 9):
                for x in range(mx + 3, mx + mw - 6):
                    if not all(clear(xx, yy) for xx in range(x - 2, x + w + 2)
                               for yy in range(y - 2, y + h + 6)):
                        continue
                    if any(abs(x - tx) < 10 and abs(y - ty) < 10 for tx, ty in taken):
                        continue
                    taken.append((x, y))
                    return x, y
            sys.exit("gate_ebldboom: no room on %s" % name)
        st, un, ov = [], [], []
        L = layout[name] = {"types": [], "walls": {}}
        for i, t in enumerate(types):
            (fw, fh), occ = FOOT[t]
            x, y = spot(fw, fh)
            st.append("%03d=%s,%s,256,%d,0,None" % (i, OWNER.get(t, "Neutral"), t, cell(x, y)))
            for tx in (x, x + fw - 1):
                un.append("%03d=GoodGuy,MTNK,256,%d,0,Guard,None" % (len(un), cell(tx, y + fh + 3)))
            ox, oy = occ[-1]                  # a cell it blocks, its south-east one
            L["types"].append(dict(type=t, x=x, y=y, fw=fw, fh=fh, aim=[x + ox, y + oy],
                                   tanks=[len(un) - 2, len(un) - 1]))
        for kind, levels in sorted(walls.items()):
            x, y = spot(5, 1)
            run = [(x + i, y) for i in range(5)]
            ov += ["%d=%s" % (cell(cx, cy), kind) for cx, cy in run]
            L["walls"][kind] = dict(cells=run, target=list(run[1]), levels=levels)
        text = ["[Basic]", "Player=GoodGuy", "Action=x", "Lose=x", "Win=x", "Brief=x",
                "Intro=x", "BuildLevel=98", "Theme=No theme", "Percent=0", "CarryOverCap=-1",
                "CarryOverMoney=0", "", mapsec, "",
                "[GoodGuy]", "Allies=GoodGuy,Neutral", "MaxBuilding=150", "MaxUnit=150",
                "Credits=0", "Edge=South", "", "[Neutral]", "Allies=Neutral,GoodGuy",
                "MaxBuilding=150", "MaxUnit=150", "Credits=0", "Edge=North", "",
                "[BadGuy]", "Allies=BadGuy", "MaxBuilding=150", "MaxUnit=150", "Credits=0",
                "Edge=North", "", "[STRUCTURES]"] + st + ["", "[UNITS]"] + un + \
            ["", "[INFANTRY]", "", "[OVERLAY]"] + ov + TAIL
        open(os.path.join(out, name + ".INI"), "w", encoding="latin-1", newline="").write("\r\n".join(text))
        open(os.path.join(out, name + ".BIN"), "wb").write(b)
        s = ["tick 3", "zoom max", "dist 900", "echo g309 start", "ebldump"]

        def look(x, y):
            return "cam %.1f %.1f" % (x, y)

        def dumps(tag):
            return ["echo g309 %s" % tag, "ebldump", "efxdump"]
        for d in L["types"]:
            t = d["type"]
            cx, cy = d["x"] + d["fw"] / 2.0, d["y"] + d["fh"] / 2.0 + 0.3
            s += [look(cx, cy), "tick 1", "shot shots/g309_%s_intact.png" % t]
            # the tanks force-fire until it breaks, and stop
            s += ["cam %.1f %.1f" % (cx, d["y"] + d["fh"] + 2.0), "tick 1"]
            for n in d["tanks"]:
                s += ["lclickobj MTNK %d" % n, "actcell %d %d c" % tuple(d["aim"])]
            s += ["tickstr %s 0 49 1500" % t]
            for n in d["tanks"]:
                s += ["lclickobj MTNK %d" % n, "stop"]
            s += ["deselect", look(cx, cy)]
            s += dumps("%s broke" % t)
            now = 0
            for k in HELD:
                s += ["tick %d" % (k - now), "shot shots/g309_%s_dmg%d.png" % (t, k)]
                s += dumps("%s dmg %d" % (t, k))
                now = k
            # the killing hit, and the blast's moments
            s += ["hurtcell %d %d" % tuple(d["aim"])]
            now = 0
            for k in BLAST:
                s += ["tick %d" % (k - now), "shot shots/g309_%s_b%03d.png" % (t, k)]
                s += dumps("%s blast %d" % (t, k)) + (["shakedump"] if k == BLAST[0] else [])
                now = k
        for kind, w in sorted(L["walls"].items()):
            tx, ty = w["target"]
            s += [look(tx + 0.5, ty + 0.8), "tick 1", "shot shots/g309_%s_intact.png" % kind]
            for lv in range(w["levels"] - 1):
                s += ["hurtcell %d %d" % (tx, ty), "tick 15"] + dumps("%s level %d" % (kind, lv + 1))
            s += ["hurtcell %d %d" % (tx, ty)]
            now = 0
            for k in BLAST:
                s += ["tick %d" % (k - now), "shot shots/g309_%s_b%03d.png" % (kind, k)]
                s += dumps("%s blast %d" % (kind, k))
                now = k
        s += ["tick 30", "echo g309 end", "ebldump", "efxdump", "quit"]
        open(os.path.join(out, name + ".txt"), "w").write("\n".join(s) + "\n")
    json.dump(layout, open(os.path.join(out, "layout.json"), "w"), indent=1)


def rd(path):
    try:
        with open(path, errors="ignore") as f:
            return f.read().splitlines()
    except OSError:
        return []


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "maps":
        maps(sys.argv[2])
        return 0
    if len(sys.argv) >= 4 and sys.argv[1] == "check":
        print(check(sys.argv[2], sys.argv[3]))
        return 0
    sys.exit(__doc__)


# the death's centre the lab gives the ones that stand on part of their footprint (fx.json
# explosion `at`, cells from the footprint centre, x east, y north)
AT = {"V01": (0.0, -0.5), "V04": (0.0, -0.5), "V37": (0.5, 0.0)}

# A building turned on its plot (art/remaster/yaw.json: degrees counter-clockwise seen from
# above, about the footprint centre) has its death's centre turned with it, as the baker
# turns it.
def _turned_at(at):
    import json, math, os
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "art", "remaster", "yaw.json")
    y = json.load(open(p)) if os.path.isfile(p) else {}
    out = {}
    for t, (e, n) in at.items():
        v = y.get(t, 0)
        r = math.radians(float(v)) if isinstance(v, (int, float)) else 0.0
        c, s = math.cos(r), math.sin(r)
        out[t] = (e * c - n * s, e * s + n * c)
    return out


AT = _turned_at(AT)
# the engine's own anims of a death and of the damage, by name
DEATH = ("FBALL1", "FIRE1", "FIRE2", "FIRE3", "FIRE4", "SMOKE_M")
BURN = ("FIRE1", "FIRE2", "FIRE3", "FIRE4", "BURN-S", "BURN-M", "BURN-L", "FLMSPT", "SMOKE_M")
# the lab's death jolt in footprint cells, as the files carry it
JOLT = {"WEAP": 2, "PROC": 3, "NUKE": 2, "FACT": 3, "V01": 1, "V04": 1, "V19": 1, "V37": 2}


def section(L, name):
    out, on = [], False
    for l in L:
        if l.startswith("ECHO|"):
            on = (l == "ECHO|g309 " + name)
            continue
        if on:
            out.append(l)
    return out


def check(logs, d):
    A, A2, B = (rd(os.path.join(logs, n + ".log")) for n in ("g309a", "g309a2", "g309b"))
    N, K = rd(os.path.join(logs, "g309n.log")), rd(os.path.join(logs, "g309k.log"))
    layout = json.load(open(os.path.join(d, "layout.json")))
    why = []

    def need(c, m):
        if not c:
            why.append(m)
        return c
    said = {"booms": 0, "fball": 0, "death": 0, "damage": 0, "parts": 0, "smudge": 0}
    for L, name in ((A, "SCG88EA"), (B, "SCG89EA")):
        lay = layout[name]
        if not need(L, "%s: no log" % name):
            continue
        for i, x in enumerate(lay["types"]):
            t, key = x["type"], 2000000 + i
            booms = [l for l in L if re.match(r"^EBLD\|boom\|key=%d\|" % key, l)]
            wreck = [l for l in L if re.match(r"^EBLD\|wreck\|key=%d\|" % key, l)]
            # (1) ONE EXPLOSION, THE FILE'S, ON THE DEATH'S OWN TICK, AT THE DEATH'S CENTRE
            if not need(len(booms) == 1 and len(wreck) == 1,
                        "%s: %s exploded %d times and was wrecked %d times, want once each"
                        % (name, t, len(booms), len(wreck))):
                continue
            m = re.match(r"^EBLD\|boom\|key=\d+\|frame=(\d+)\|x=(-?[0-9.]+)\|z=(-?[0-9.]+)\|recipe=(\w+)\|slots=(\d+)\|jolt=([0-9.]+)$", booms[0])
            if not need(m, "%s: %s's boom line is not the format: %s" % (name, t, booms[0])):
                continue
            wf = int(re.search(r"frame=(\d+)", wreck[0]).group(1))
            need(int(m.group(1)) == wf, "%s: %s's explosion at tick %s, its wreck at %d"
                 % (name, t, m.group(1), wf))
            need(m.group(4) == "file" and m.group(5) == "33",
                 "%s: %s's explosion is %s with %s slots, want the file's 33" % (name, t, m.group(4), m.group(5)))
            ax, az = AT.get(t, (0.0, 0.0))
            cx, cz = x["x"] + x["fw"] / 2.0 + ax, x["y"] + x["fh"] / 2.0 - az
            need(abs(float(m.group(2)) - cx) < 0.02 and abs(float(m.group(3)) - cz) < 0.02,
                 "%s: %s exploded at %s,%s, want %.2f,%.2f" % (name, t, m.group(2), m.group(3), cx, cz))
            need(abs(float(m.group(6)) - JOLT[t]) < 1e-3, "%s: %s jolts %s cells, the lab %d"
                 % (name, t, m.group(6), JOLT[t]))
            sh = [l for l in section(L, "%s blast 2" % t) if l.startswith("SHAKE|")]
            sa = float(re.search(r"amp=([0-9.]+)", sh[-1]).group(1)) if sh else -1
            need(abs(sa - 0.028 * JOLT[t]) < 2e-4, "%s: %s's jolt amplitude %.4f, want %.4f"
                 % (name, t, sa, 0.028 * JOLT[t]))
            # (2) THE ENGINE'S DEATH STOOD DOWN: a fireball for every cell it blocks, taken
            sd = [re.match(r"^EBLD\|standdown\|anim=([\w-]+)\|id=\d+\|why=(\w+)\|key=(\d+)\|", l)
                  for l in L if l.startswith("EBLD|standdown|")]
            mine = [q for q in sd if q and int(q.group(3)) == key]
            fb = sum(1 for q in mine if q.group(1) == "FBALL1" and q.group(2) == "death")
            need(fb == len(FOOT[t][1]), "%s: %d of %s's %d death fireballs stood down"
                 % (name, fb, t, len(FOOT[t][1])))
            said["fball"] += fb
            # and the craters the engine's death puts on its cells
            sm = sum(1 for l in L if re.match(r"^EBLD\|standdown\|smudge=\d+\|cell=\d+,\d+\|why=death\|key=%d\|" % key, l))
            need(sm >= 1, "%s: none of %s's death craters stood down" % (name, t))
            said["smudge"] += sm
            said["death"] += sum(1 for q in mine if q.group(2) == "death")
            said["damage"] += sum(1 for q in mine if q.group(2) == "damage")
            # (3) NONE OF THE ENGINE'S DEATH OR DAMAGE ANIMS IS LIVE once it is broken, and
            # (4) THE LAB'S RECIPE EMITTED: the shards' streaks, the big fireball, the embers
            for sec in ["%s broke" % t] + ["%s dmg %d" % (t, k) for k in HELD] + \
                       ["%s blast %d" % (t, k) for k in BLAST]:
                S = section(L, sec)
                live = [l for l in S if re.match(r"^EFXDUMP\|ANIM\|(%s)\|" % "|".join(
                    re.escape(n) for n in set(DEATH + BURN)), l)]
                need(not live, "%s: %s, an engine anim is live: %s" % (name, sec, live[:1]))
            S = section(L, "%s blast 120" % t)
            eb = [re.match(r"^EBLDBOOM\|%s\|key=%d\|born=(\d+)\|stage=(\d+)\|parts=(\d+)\|chunks=(\d+)" % (t, key), l)
                  for l in S]
            eb = [q for q in eb if q]
            if need(eb, "%s: no EBLDBOOM line for %s at 8 seconds" % (name, t)):
                q = eb[0]
                need(int(q.group(2)) == 119 and int(q.group(3)) >= 400 and int(q.group(4)) >= 3,
                     "%s: %s's explosion at 8 s: stage %s, %s particles, %s chunks"
                     % (name, t, q.group(2), q.group(3), q.group(4)))
                said["parts"] += int(q.group(3))
            seq = {}
            for k in BLAST[:3]:
                for l in section(L, "%s blast %d" % (t, k)):
                    mm = re.match(r"^EFXDRAWSEQ\|slot=\d+\|(\w+)\|enh=(\d+)", l)
                    if mm:
                        seq[mm.group(1)] = seq.get(mm.group(1), 0) + int(mm.group(2))
            need(all(seq.get(k, 0) > 0 for k in ("BIGBOOM", "COAL", "SHOCK", "SMOKE", "DEBRIS")),
                 "%s: %s's blast drew sheets %s" % (name, t, seq))
            # (5) ITS DAMAGED STAGE IS ITS OWN EFFECTS: the file's damaged emitters gave off
            dm = [re.match(r"^EBLDEMIT\|%s\|key=%d\|e=\d+\|src=(\w+)\|state=1\|.*\|spawned=(\d+)" % (t, key), l)
                  for l in section(L, "%s dmg 45" % t)]
            dm = [q for q in dm if q]
            need(dm and sum(int(q.group(2)) for q in dm) > 0,
                 "%s: %s's damaged emitters gave off nothing by 3 seconds" % (name, t))
            said["booms"] += 1
        # the oil pump's flame spout, which the engine attaches at half strength, stood down
        if name == "SCG88EA":
            need(any(re.match(r"^EBLD\|standdown\|anim=FLMSPT\|id=\d+\|why=damage\|", l) for l in L),
                 "the oil pump's flame spout did not stand down")
            # (6) THE WALL CELLS: no explosion, the wreck and its collapse dust, and no
            # engine anim on the cell
            for kind, w in sorted(lay["walls"].items()):
                tx, ty = w["target"]
                k = 9000000000 + ty * 512 + tx
                need(not any(re.match(r"^EBLD\|boom\|key=%d\|" % k, l) for l in L),
                     "%s's cell exploded" % kind)
                need(any(re.match(r"^EBLD\|wreck\|key=%d\|" % k, l) for l in L),
                     "%s's cell was not wrecked" % kind)
                ds = [re.match(r"^EBLDEMIT\|%s\|key=%d\|e=\d+\|src=\w+\|state=2\|.*\|spawned=(\d+)" % (kind, k), l)
                      for l in section(L, "%s blast 15" % kind)]
                ds = [q for q in ds if q]
                need(ds and sum(int(q.group(1)) for q in ds) > 0, "%s's collapse dust did not play" % kind)
                for b in BLAST:
                    live = [l for l in section(L, "%s blast %d" % (kind, b))
                            if re.match(r"^EFXDUMP\|ANIM\|(%s)\|" % "|".join(re.escape(n) for n in set(DEATH + BURN)), l)]
                    need(not live, "%s blast %d: an engine anim is live: %s" % (kind, b, live[:1]))
    # (7) --noebld AND CLASSIC: the engine's own death and damage, nothing of this system's
    for nm, L in (("--noebld", N), ("Classic", K)):
        need(L and not any(l.startswith(("EBLD|boom|", "EBLD|standdown|")) for l in L),
             "%s: an explosion or a stand-down" % nm)
        sp = {}
        for l in L:
            mm = re.match(r"^EFXSPAWN\|([\w-]+)\|recipe=[^|]*\|particles=(\d+)\|chunks=(\d+)", l)
            if mm:
                sp[mm.group(1)] = max(sp.get(mm.group(1), 0), int(mm.group(2)) + int(mm.group(3)))
        need(sp.get("FBALL1", 0) > 0 and sp.get("FIRE3", 0) > 0,
             "%s: the engine's death effects did not play (%s)" % (nm, sp))
    # and under Enhanced the engine's death recipes emitted nothing at all
    sp = {}
    for l in A + B:
        mm = re.match(r"^EFXSPAWN\|([\w-]+)\|recipe=[^|]*\|particles=(\d+)\|chunks=(\d+)", l)
        if mm:
            sp[mm.group(1)] = max(sp.get(mm.group(1), 0), int(mm.group(2)) + int(mm.group(3)))
    need(not any(sp.get(n, 0) for n in DEATH + BURN),
         "Enhanced: an engine death or damage recipe emitted: %s" % {n: sp[n] for n in sp if n in DEATH + BURN})
    # (8) TWO RUNS ARE ONE RUN
    ours = ("EBLD|", "EBLDOBJ|", "EBLDBOOM|", "EBLDOFF|", "EBLDEMIT|")
    la = [l for l in A if l.startswith(ours)]
    lb = [l for l in A2 if l.startswith(ours)]
    need(la == lb and la, "the two Enhanced runs printed different EBLD lines")
    same, tot = 0, 0
    for nm in sorted(os.listdir(logs)):
        if nm.startswith("g309_") and nm.endswith(".png"):
            o = os.path.join(logs, "g309r_" + nm[5:])
            if os.path.exists(o):
                tot += 1
                same += open(os.path.join(logs, nm), "rb").read() == open(o, "rb").read()
    need(tot >= 30 and same == tot, "only %d of %d shot pairs of the two runs are identical" % (same, tot))
    if why:
        more = " (and %d more)" % (len(why) - 6) if len(why) > 6 else ""
        return "0 " + "; ".join(why[:6]) + more
    return ("1 %d buildings each exploded once with the lab's recipe from its file, on its death's "
            "tick, at its death's centre, jolting by the lab's cells; %d engine fireballs (one a "
            "blocked cell), %d death anims and %d death craters in all stood down, %d damage fires; "
            "no engine death or damage anim live or emitting under Enhanced; %d explosion particles by 8 seconds; "
            "the damaged stages played their own emitters; both wall cells collapsed in dust with "
            "no explosion; --noebld and Classic keep the engine's own; %d shots byte-identical "
            "across two runs" % (said["booms"], said["fball"], said["death"], said["smudge"], said["damage"],
                                 said["parts"], tot))


if __name__ == "__main__":
    sys.exit(main())
