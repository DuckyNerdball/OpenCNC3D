#!/usr/bin/env python3
"""
OpenCNC3D RA3D -- does every Red Alert map's 3D scene carry its own terrain and heights?

    python3 tools/ra3d/check-3d-packs.py <module folder>      e.g. playable/ra3d

For every map in missions_3d.py, reads <module>/3d/<SCEN>.pack and checks that its
theater is the Red Alert bank the mission's [Map] names (RASNOW, RATEMP, RAINT), that it
is the whole 128 x 128 grid, and that its corner heights are data/ra3d/autoheight's file
byte for byte. The multiplayer maps cannot be booted until Red Alert has a skirmish
screen, so this is what stands between their scenes and a silent flat or wrong map.
Exit 1 naming each pack that fails.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "bakery"))
import packinspect                  # noqa: E402
import ra_map                       # noqa: E402
import missions_3d                  # noqa: E402
from bake_ra_terrain3d import BANK_OF   # noqa: E402


def main():
    if len(sys.argv) != 2:
        print(__doc__); sys.exit(2)
    module = sys.argv[1]
    bad = 0
    for scen in missions_3d.ALL:
        pk = os.path.join(module, "3d", scen + ".pack")
        name = "RA" + scen[2:]
        if not os.path.exists(pk):
            print(f"{scen}: no 3D scene in {module}/3d"); bad += 1; continue
        p = packinspect.read(pk)
        want_th = BANK_OF[ra_map.translate(scen)["theater"]]
        th = p.get("theater")
        th = th.decode() if isinstance(th, bytes) else th
        hgt = open(os.path.join(ROOT, "data", "ra3d", "autoheight", name + ".HGT"), "rb").read()
        got = p.get("corner")
        why = []
        if th != want_th:
            why.append(f"theater {th}, want {want_th}")
        if (p.get("mapW"), p.get("mapH")) not in ((128, 128), (None, None)):
            why.append(f"grid {p.get('mapW')} x {p.get('mapH')}")
        if got is None or bytes(got) != hgt:
            why.append(f"heights are not data/ra3d/autoheight/{name}.HGT")
        if why:
            print(f"{scen}: " + "; ".join(why)); bad += 1
    n = len(missions_3d.ALL)
    print(f"RA 3D scenes: {n - bad} of {n} carry their own terrain and heights")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
