#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- does every staged pack carry its generated heightmap?

    python3 check_packs.py <play folder>

For each data/autoheight/<SCEN>.HGT, reads <play folder>/<SCEN>.pack and compares its
65x65 corner heights with the committed file byte for byte. Exit 1 on any mismatch or
missing pack, naming it. make-build.sh runs this after staging packs, because a pack
that lost its heights plays flat and says nothing.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "bakery"))
import packinspect


def main():
    dest = sys.argv[1]
    data = os.path.join(ROOT, "data", "autoheight")
    bad = 0
    n = 0
    for f in sorted(os.listdir(data) if os.path.isdir(data) else []):
        if not f.endswith(".HGT"):
            continue
        s = f[:-4]; n += 1
        want = open(os.path.join(data, f), "rb").read()
        pk = os.path.join(dest, s + ".pack")
        if not os.path.exists(pk):
            print(f"auto heightmap: {s}.pack is not in {dest}"); bad += 1; continue
        got = packinspect.read(pk).get("corner")
        if got is None or bytes(got) != want:
            print(f"auto heightmap: {s}.pack does NOT carry data/autoheight/{f}"); bad += 1
    print(f"auto heightmap: {n - bad} of {n} packs carry their generated heights")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
