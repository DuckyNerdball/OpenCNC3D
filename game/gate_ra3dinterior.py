#!/usr/bin/env python3
"""G368: Red Alert's indoor missions stand their walls up in 3D under Enhanced
(game/ra3d_interior_mod.h; the files baked by tools/ra3d/bake-interior.py into
game/einw_ra and staged into the module's interior/ folder by tools/ra3d/stage-structures.py).

    python3 game/gate_ra3dinterior.py script DIR     write the runs' scripts
    python3 game/gate_ra3dinterior.py check DIR      read the logs: "1 message" or "0 message"

Run from a play folder with a staged Red Alert module. Hidden runs, every one Enhanced:

  <SCEN>   each of the seven missions set indoors (SCG10EB, SCG13EA, SCU07EA on the 1996
           discs; SCA04EA, SCG21EA, SCU33EA, SCU42EA on the expansion discs), the shroud
           off, the frame at the mission's start: `rainterior` and a shot
  sh       Soviet 7 with the shroud on, as the mission starts: `rainterior`
  off, on  Allied 13 with the shroud off, `rainterior 0` (the 1996 art) and `rainterior 1`
           set before the first tick: a shot each

Claims (from the runs' own lines: the loader's RA3D|interior|walls and |meshes lines, the
ground's RA3D|interior|floors line and `rainterior`'s RA3DINT line):
  (1) every one of the seven reads its own file, with exactly the wall cells and triangles
      the staged manifest names, and builds a mesh a wall cell; its walls are drawn in the
      colour pass, the sun's casters or the relight bake, and the light pass's normal target;
  (2) the floor swap: the ground draws every shadow floor piece plain (no FLOR0002..0007 or
      GFLR0002..0005 left) and swaps exactly the cells the map holds of them, the cut
      corners' and the wall consoles' floor icons with them;
  (3) the shroud: with it on, the walls of cells the player has not revealed are not drawn
      (some are hidden, and no more cells are drawn than are revealed); with it off only the
      walls on the engine's always-shrouded rim round the playable rectangle are hidden;
  (4) the switch: `rainterior 0` draws the 1996 art (no walls drawn, no floor swapped) and
      the two frames differ in more than 5 percent of their pixels (measured: 18 percent
      of Allied 13's opening frame; 0 with no walls drawn).
PROVEN TO FAIL: a renderer without ra3d_interior_mod.h answers no `rainterior` (a script
failure) and prints none of the loader's lines; a module without the files reads none.
NOT COVERED: how the walls look (the shots, by eye); units hidden behind the walls.
"""
import json
import os
import re
import sys

PLAY = os.getcwd()
HERE = os.path.dirname(os.path.abspath(__file__))
MISSIONS = ("SCG10EB", "SCG13EA", "SCU07EA", "SCA04EA", "SCG21EA", "SCU33EA", "SCU42EA")
SHADOW = tuple("FLOR%04d" % k for k in range(2, 8)) + tuple("GFLR%04d" % k for k in range(2, 6))


def script(d):
    os.makedirs(d, exist_ok=True)
    for s in MISSIONS:
        with open(os.path.join(d, s + ".txt"), "w") as f:
            f.write("tick 30\nshot %s/%s_look.png\nrainterior\nquit\n" % (d, s))
    with open(os.path.join(d, "sh.txt"), "w") as f:
        f.write("tick 30\nshot %s/sh.png\nrainterior\nquit\n" % d)
    for tag, on in (("off", 0), ("on", 1)):
        with open(os.path.join(d, tag + ".txt"), "w") as f:
            f.write("rainterior %d\ntick 30\nshot %s/%s.png\nrainterior\nquit\n" % (on, d, tag))


def fields(line):
    return dict(p.split("=", 1) for p in line.strip().split("|") if "=" in p)


def lines(path, prefix):
    try:
        return [l for l in open(path, errors="replace") if l.startswith(prefix)]
    except OSError:
        return []


def check(d):
    man = json.load(open(os.path.join(PLAY, "ra3d", "interior", "manifest.json")))
    sys.path.insert(0, os.path.join(HERE, "..", "tools", "art", "remaster", "ra"))
    swaps_expected = {}
    try:
        import interior as I
        for s in MISSIONS:
            g = I.mission(s, ring=1)          # the engine's export: the rectangle and a ring
            X, Y = g["rect"][0], g["rect"][1]
            n = 0
            for y, row in enumerate(g["grid"]):
                for x, (name, icon) in enumerate(row):
                    if I.swapped(name, icon, X + x, Y + y) != (name, icon):
                        n += 1
            swaps_expected[s] = n
    except Exception as e:                    # a play folder with no source beside it
        swaps_expected = {}
        print("(the floor swap's expected counts are not checked: %s)" % e, file=sys.stderr)
    # each mission's wall cells on the rim: listed in its staged file, outside its playable rectangle
    rim = {}
    try:
        import struct
        for s in MISSIONS:
            X, Y, w, h = I.mission(s)["rect"]
            b = open(os.path.join(PLAY, "ra3d", "interior", s + ".einw"), "rb").read()
            nc = struct.unpack_from("<I", b, 8)[0]
            xy = [struct.unpack_from("<HH", b, 16 + 12 * i) for i in range(nc)]
            rim[s] = sum(1 for x, y in xy if not (X <= x < X + w and Y <= y < Y + h))
    except Exception as e:
        print("(the rim's wall cells are not counted: %s)" % e, file=sys.stderr)
    notes = []
    for s in MISSIONS:
        log = os.path.join(d, s + ".log")
        want = man["missions"][s]
        w = [fields(l) for l in lines(log, "RA3D|interior|walls|") if "|read|" in l]
        if not w or w[0].get("scene") != s or int(w[0]["cells"]) != want["cells"] \
                or int(w[0]["triangles"]) != want["triangles"]:
            return 0, "%s did not read its walls (%s; the manifest: %d cells, %d triangles)" % (
                s, w[0] if w else "no RA3D|interior|walls line", want["cells"], want["triangles"])
        m = [fields(l) for l in lines(log, "RA3D|interior|meshes|")]
        if not m or int(m[0]["cells"]) != want["cells"]:
            return 0, "%s built no wall meshes" % s
        r = [fields(l) for l in lines(log, "RA3DINT|scene=")]
        if not r:
            return 0, "%s: `rainterior` answered nothing" % s
        r = r[-1]
        # with the shroud off only the edge ring stays hidden (the engine's always-shrouded rim
        # round the playable rectangle, whose walls the file carries for their neighbours' sake)
        ring = rim.get(s)
        if int(r["drawn"]) <= 0 or int(r["aux"]) <= 0 or int(r["normals"]) <= 0 \
                or (ring is not None and int(r["hidden"]) != ring):
            return 0, "%s: walls not drawn in every pass with the shroud off (%s; %s wall cells on the rim)" % (s, r, ring)
        f = [fields(l) for l in lines(log, "RA3D|interior|floors|")]
        if not f or f[-1].get("swap") != "1" or int(f[-1]["shadow_left"]) != 0:
            return 0, "%s: shadow floor pieces still drawn (%s)" % (s, f[-1] if f else "no floors line")
        if s in swaps_expected and int(f[-1]["swapped"]) != swaps_expected[s]:
            return 0, "%s: %s floor cells swapped, the map holds %d to swap" % (s, f[-1]["swapped"], swaps_expected[s])
        notes.append("%s %s/%d" % (s, r["drawn"], want["cells"]))
    r = [fields(l) for l in lines(os.path.join(d, "sh.log"), "RA3DINT|scene=")]
    if not r:
        return 0, "Soviet 7 with the shroud on: `rainterior` answered nothing"
    r = r[-1]
    cells, hidden, drawn = int(r["cells"]), int(r["hidden"]), int(r["drawn"])
    if hidden <= 0 or drawn > cells - hidden or drawn <= 0:
        return 0, "the shroud does not hold the walls back (cells %d, hidden %d, drawn %d)" % (cells, hidden, drawn)
    off = [fields(l) for l in lines(os.path.join(d, "off.log"), "RA3DINT|scene")]
    on = [fields(l) for l in lines(os.path.join(d, "on.log"), "RA3DINT|scene")]
    if not off or not on or int(off[-1]["drawn"]) != 0 or int(on[-1]["drawn"]) <= 0:
        return 0, "`rainterior 0` and 1 do not take the walls away and back (%s / %s)" % (off, on)
    f = [fields(l) for l in lines(os.path.join(d, "off.log"), "RA3D|interior|floors|")]
    if not f or f[-1].get("swap") != "0" or int(f[-1]["shadow_left"]) <= 0:
        return 0, "`rainterior 0` does not bring the 1996 shadow floors back (%s)" % (f[-1] if f else "no floors line")
    try:
        from PIL import Image, ImageChops
        a = Image.open(os.path.join(d, "off.png")).convert("L")
        b = Image.open(os.path.join(d, "on.png")).convert("L")
        diff = ImageChops.difference(a, b).point(lambda v: 255 if v > 12 else 0)
        share = sum(diff.histogram()[255:]) / float(a.size[0] * a.size[1])
    except Exception as e:
        return 0, "the off and on frames could not be compared: %s" % e
    if share < 0.05:
        return 0, "the walls on and off differ in only %.0f%% of the frame" % (100 * share)
    return 1, ("Red Alert's seven indoor missions stand their walls up in 3D, every pass, the shadow floors "
               "plain (walls drawn/cells: %s); the shroud holds back %d of Soviet 7's %d wall cells at its start; "
               "the walls off and on differ in %.0f%% of Allied 13's frame") % (", ".join(notes), hidden, cells, 100 * share)


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("script", "check"):
        sys.exit(__doc__)
    if sys.argv[1] == "script":
        script(sys.argv[2])
    else:
        ok, msg = check(sys.argv[2])
        print("%d %s" % (ok, msg))
