#!/usr/bin/env python3
"""Check Red Alert's baked Interior walls (tools/ra3d/bake-interior.py) against the missions
they were baked for.

    python3 tools/ra3d/check-interior.py [DIR]      default DIR: game/einw_ra

For each of the seven missions set indoors: the file matches its manifest's hash and its
header; every cell it lists is a cell of the mission's playable rectangle that the engine
blocks and that the kit builds a wall on, and every such cell is listed; every vertex lies
inside its cell's square (with the broken faces' push, still inside), between the floor's
foot and the walls' top; every normal is a unit vector; every atlas coordinate is inside
the atlas. Prints one line per mission and exits non-zero on the first failure.
"""
import hashlib
import json
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "art", "remaster", "ra"))
import interior as I                         # noqa: E402
import build_INT_WALLS as K                  # noqa: E402

# the engine's map export: the playable rectangle and a ring of one cell round it, which
# the game's ground draws too, so its walls stand there as well
RING = 1


def read(path):
    b = open(path, "rb").read()
    if b[:8] != b"RAINTW01":
        raise SystemExit("%s: not an interior walls file" % path)
    nc, nv = struct.unpack_from("<II", b, 8)
    cells = [struct.unpack_from("<HHII", b, 16 + 12 * i) for i in range(nc)]
    v = np.frombuffer(b, np.float32, nv * 8, 16 + 12 * nc).reshape(nv, 8)
    return b, cells, v


def main(d):
    man = json.load(open(os.path.join(d, "manifest.json")))
    for scen in I.MISSIONS:
        path = os.path.join(d, scen + ".einw")
        b, cells, v = read(path)
        rec = man["missions"][scen]
        if hashlib.sha256(b).hexdigest() != rec["sha256"]:
            raise SystemExit("%s: the file is not the one the manifest names" % scen)
        m = I.mission(scen, ring=RING)
        X, Y, w, h = m["rect"]
        g = m["grid"]
        kit = K.Kit(type("P", (), {"w": w, "h": h, "xy": lambda s, u, vv: (u - w / 2.0, h / 2.0 - vv)})(), g)
        want = {(X + u, Y + vv) for vv in range(h) for u in range(w) if kit.solid(u, vv)}
        got = {(c[0], c[1]) for c in cells}
        if got != want:
            raise SystemExit("%s: %d wall cells missing, %d listed that are not walls" % (
                scen, len(want - got), len(got - want)))
        for x, y, first, count in cells:
            if count % 3:
                raise SystemExit("%s: cell %d,%d has %d vertices, not whole triangles" % (scen, x, y, count))
            name, icon = g[y - Y][x - X]
            if not I.blocked(name, icon):
                raise SystemExit("%s: walls on %d,%d, which the engine lets units walk" % (scen, x, y))
            q = v[first:first + count]
            if (q[:, 0] < x - 1e-4).any() or (q[:, 0] > x + 1 + 1e-4).any() or \
               (q[:, 1] < y - 1e-4).any() or (q[:, 1] > y + 1 + 1e-4).any():
                raise SystemExit("%s: cell %d,%d reaches out of its square" % (scen, x, y))
        if (v[:, 2] < K.VOID_Z - 1e-4).any() or (v[:, 2] > K.H + 1e-4).any():
            raise SystemExit("%s: a vertex under the void's ground or over the walls' top" % scen)
        n = np.linalg.norm(v[:, 5:8], axis=1)
        if (np.abs(n - 1) > 1e-3).any():
            raise SystemExit("%s: a normal that is not a unit vector" % scen)
        if (v[:, 3:5] < 0).any() or (v[:, 3:5] > 1).any():
            raise SystemExit("%s: an atlas coordinate outside the atlas" % scen)
        print("EINW|check|%s|%d wall cells, all and only the blocked walls|%d triangles|ok" % (scen, len(cells), len(v) // 3))
    print("EINW|check|all %d missions ok" % len(I.MISSIONS))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "game", "einw_ra"))
