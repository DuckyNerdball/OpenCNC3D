#!/usr/bin/env python3
"""G324's two halves: the scripts it plays and the claims it checks.

    python3 gate_weapdoor.py scripts DIR   write DIR/intact.txt and DIR/damaged.txt
    python3 gate_weapdoor.py check LOGS    read LOGS/g324_intact.log and LOGS/g324_damaged.log
                                           and print "1 message" or "0 why"

THE DOOR. Under Enhanced the Weapons Factory is the remastered model (game/ebld/WEAP.ebld),
and its door is that model's clips (tools/art/remaster/door_WEAP.py): the intact model's
clip and, once damaged, the cut's, both on the engine's own door, the frame the cartridge
gives the door stage, stage * 59 / 9 (weap_door_frame). Before them the remastered door was
one fixed piece and stayed shut while every vehicle drove out through it.

THE RUNS. intact: SCG90EA (the Weapons Factory at 53,46, nothing attacking it), a Medium
Tank built, then tick by tick a frame drawn and the state dumped while the door opens, the
tank rolls out and the door shuts again. damaged: SCG01EC, whose Nod guns wear the factory
down from the start; a Humvee is started at tick 200 so it comes out once the factory is
below half strength and in its damaged stage, then the same tick by tick dumps. Both runs
shoot a few frames for a person (the door shut, opening, open with the vehicle in it).

THE CLAIMS, from the engine's own dump lines:
  (1) the factory's model carries its door: two clip parts in the intact model and two in
      the cut's clip (EBLDART: clipparts=2, cutclip=2);
  (2) on every dump the door frame is the door stage's (EBLDWEAPDOOR at = door * 59 / 9) and
      the intact clip's frame is its floor;
  (3) the frame each door piece was DRAWN with (EBLDPOSE, kind clip, read off the draw) is
      the frame of the same tick's door, on every dump, and it walked the whole door, shut
      to open (0 and 59 both drawn, and at least eight frames between) and shut again;
  (4) damaged: the cut clip's frame is the door's, the upper panel stands closed while the
      door is shut and is moved further at each wider stage, and the jammed lower panel
      stays on its loops (kind loop, its tries counted) and is carried on down by the
      door's travel, more than a third of a cell beyond its place with the door shut.
NOT COVERED: how the door looks, which no counter says; the shots are for a person.
"""
import os
import re
import sys

PER_STAGE = 59.0 / 9.0


def scripts(d):
    os.makedirs(d, exist_ok=True)
    shots = {"intact": {0: "shut", 4: "opening", 18: "open", 40: "out"},
             "damaged": {0: "shut", 4: "opening", 18: "open", 40: "out"}}
    head = {"intact": ["tick 2", "player", "cam 54 47", "zoom max", "build MTNK", "ready MTNK 4000"],
            "damaged": ["tick 200", "player", "cam 51 47", "zoom max", "build JEEP", "ready JEEP 2000"]}
    for run in ("intact", "damaged"):
        L = list(head[run])
        for i in range(0, 121):
            if i:
                L += ["tick 1"]
            L += ["frames 1", "anims", "ebldump"]
            if i in shots[run]:
                L += ["shot shots/g324_%s_%s.png" % (run, shots[run][i])]
        open(os.path.join(d, run + ".txt"), "w").write("\n".join(L) + "\n")
    print("1 scripts written")


def blocks(path):
    """Per dump: the door stage from `anims`, the EBLDWEAPDOOR line, the drawn door poses and the
    standing door pieces' moves."""
    out, cur = [], None
    for l in open(path, errors="replace"):
        l = l.rstrip("\n")
        m = re.match(r"^ANIM\|WEAP\|id=\d+\|.*\|door=(-?\d+)\|", l)
        if m:
            cur = dict(door=int(m.group(1)), dd=None, pose=[], move={})
            out.append(cur)
            continue
        if cur is None:
            continue
        m = re.match(r"^EBLDWEAPDOOR\|WEAP\|key=\d+\|stage=(\w+)\|door=(-?\d+)\|at=(-?[0-9.]+)\|intact=(-?\d+)\|cut=(-?\d+)$", l)
        if m:
            cur["dd"] = dict(stage=m.group(1), door=int(m.group(2)), at=float(m.group(3)),
                             intact=int(m.group(4)), cut=int(m.group(5)))
        m = re.match(r"^EBLDPOSE\|WEAP\|key=\d+\|id=\d+\|piece=(\d+)\|kind=clip\|frame=(-?[0-9.]+)$", l)
        if m:
            cur["pose"].append(float(m.group(2)))
        m = re.match(r"^EBLDMOVE\|WEAP\|key=\d+\|piece=(door_\w+)\|kind=(\w+)\|deg=([0-9.]+)\|moved=([0-9.]+)\|try=(-?\d+)", l)
        if m:
            cur["move"][m.group(1)] = (m.group(2), float(m.group(4)), int(m.group(5)))
    return out


def check(d):
    why = []
    logs = {r: os.path.join(d, "g324_%s.log" % r) for r in ("intact", "damaged")}
    for r, p in logs.items():
        if not os.path.exists(p):
            print("0 no %s run log" % r)
            return
    txt = open(logs["intact"], errors="replace").read()
    m = re.search(r"^EBLDART\|WEAP\|.*$", txt, re.M)
    mline = m.group(0) if m else ""
    cp = re.search(r"\|clipparts=(\d+)", mline)
    cc = re.search(r"\|cutclip=(\d+)", mline)
    if not (cp and cc and int(cp.group(1)) == 2 and int(cc.group(1)) == 2):
        why.append("(1) the factory's model line does not carry the door's clips: %s" % (mline[:160] or "no model line"))
    B = {r: blocks(p) for r, p in logs.items()}
    # (2) and (3), both runs; the intact run must walk the whole door while intact
    seen = {}
    for r, bs in B.items():
        bad2 = bad3 = 0
        for b in bs:
            dd = b["dd"]
            if dd is None:
                bad2 += 1
                continue
            want = max(b["door"], 0) * PER_STAGE
            if dd["door"] != b["door"] or abs(dd["at"] - want) > 0.01 or dd["intact"] != int(want):
                bad2 += 1
            if b["pose"]:
                if any(abs(p - int(want)) > 0.001 for p in b["pose"]):
                    bad3 += 1
                if dd["stage"] == "INTACT":
                    seen.setdefault(r, set()).update(int(p) for p in b["pose"])
        if not bs:
            why.append("(2) the %s run printed no door dumps" % r)
        if bad2:
            why.append("(2) %s: %d of %d dumps put the door frame off the door stage" % (r, bad2, len(bs)))
        if bad3:
            why.append("(3) %s: %d dumps drew a door piece at another frame than the door's" % (r, bad3))
    fr = seen.get("intact", set())
    doors = [b["door"] for b in B["intact"]]
    shut_again = 9 in doors and 0 in doors[doors.index(9):]
    if not (0 in fr and 59 in fr and len(fr) >= 10 and shut_again):
        why.append("(3) the intact door drew frames %s and its stages ran %s..." % (sorted(fr), doors[:3]))
    # (4) the damaged door
    dm = [b for b in B["damaged"] if b["dd"] and b["dd"]["stage"] == "DAMAGED"]
    opened = [b for b in dm if b["door"] > 0]
    if not opened:
        why.append("(4) the damaged factory never opened its door (%d damaged dumps; the Humvee came out "
                   "before the damage, or the factory died first)" % len(dm))
    else:
        if any(b["dd"]["cut"] != int(b["door"] * PER_STAGE) for b in dm):
            why.append("(4) the cut clip's frame left the door's")
        up = {}
        low = {}
        for b in dm:
            mv = b["move"]
            if "door_up" in mv and mv["door_up"][0] == "clip":
                up.setdefault(b["door"], set()).add(round(mv["door_up"][1], 1))
            if "door_low" in mv:
                low.setdefault(b["door"], []).append(mv["door_low"])
        ups = [(s, max(v)) for s, v in sorted(up.items())]
        if not ups or max(up.get(0, {0.0})) > 1.0 or any(b[1] <= a[1] for a, b in zip(ups, ups[1:])):
            why.append("(4) the upper panel's moves by door stage: %s" % ups)
        kinds = {x[0] for v in low.values() for x in v}
        tries = max((x[2] for v in low.values() for x in v), default=-1)
        shut = [x[1] for x in low.get(0, [])]
        wide = [x[1] for x in low.get(9, [])]
        if kinds != {"loop"} or tries < 0 or not shut or not wide or min(wide) - max(shut) < 340.0:
            why.append("(4) the jammed lower panel: kinds %s, tries to %d, moved shut %s open %s" % (
                sorted(kinds), tries, [round(x) for x in shut[:4]], [round(x) for x in wide[:4]]))
    if why:
        print("0 " + "; ".join(why))
        return
    print("1 the door follows the engine's door: intact frames %d..%d drawn on the stage's frame on all %d "
          "dumps and shut again; damaged, %d dumps open, the upper panel lifting stage by stage and the "
          "jammed lower one carried %.0f pack units further down, still on its loops" % (
              min(fr), max(fr), len(B["intact"]), len(opened), min(wide) - max(shut)))


if __name__ == "__main__":
    {"scripts": scripts, "check": check}[sys.argv[1]](sys.argv[2])
