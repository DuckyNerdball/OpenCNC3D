#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- does the editor's C++ generator give the Python's ground?

    python3 port_check.py [SCEN ...]      default: the Covert Ops and multiplayer maps

Builds port_check.cpp (game/autoheight_mod.h on its own), runs it on each map's tiles
and compares it corner by corner with generate.py. With the swells off the two must
agree: the same method, the same learned tables, the same tuning. With them on, only
their size and slopes can be compared, because the rolling field is drawn from
different random generators.
"""
import os, struct, subprocess, sys, tempfile, zlib
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import corpus as C
import generate as G

HERE = os.path.dirname(os.path.abspath(__file__))


def build():
    exe = os.path.join(tempfile.gettempdir(), "ah_port_check")
    subprocess.run(["c++", "-std=c++17", "-O2", "-o", exe, os.path.join(HERE, "port_check.cpp")], check=True)
    return exe


def run_cpp(exe, m, swells):
    p = struct.pack("<8i", 64, 64, m.X, m.Y, m.W, m.H, zlib.crc32(m.id.encode()) & 0x7fffffff, int(swells))
    p += m.tmpl.astype(np.uint8).tobytes() + m.icon.astype(np.uint8).tobytes()
    fps = [fc for typ, fc in m.footprints()]
    p += struct.pack("<i", len(fps))
    for fc in fps:
        p += struct.pack("<i", len(fc)) + b"".join(struct.pack("<2i", x, y) for x, y in fc)
    p += struct.pack("<i", len(m.starts)) + b"".join(struct.pack("<2i", x, y) for x, y in m.starts)
    with tempfile.TemporaryDirectory() as d:
        a, b = os.path.join(d, "in.bin"), os.path.join(d, "out.hgt")
        open(a, "wb").write(p)
        subprocess.run([exe, a, b], check=True)
        raw = open(b, "rb").read()
    h = np.frombuffer(raw[:4225], np.uint8).reshape(65, 65).astype(int)
    stats = struct.unpack("<4i", raw[4225 + 4096:])
    return h, stats


def main():
    ids = sys.argv[1:] or list(C.COVERT_OPS + C.MULTIPLAYER)
    exe = build()
    model = G.train(C.training())
    worst = 0.0
    print(f"  {'map':8s} {'swells off: mean |d|':>20s} {'within 2':>9s} {'max':>4s} | {'swells on: max lift py/c++':>27s}")
    for s in ids:
        m = C.Map(s)
        G.SWELLS = False
        py0 = G.generate(m, model).astype(int)
        c0, st = run_cpp(exe, m, False)
        G.SWELLS = True
        py1 = G.generate(m, model).astype(int)
        c1, _ = run_cpp(exe, m, True)
        pc = m.playable_corners()
        d = np.abs(py0 - c0)[pc]
        worst = max(worst, d.mean())
        print(f"  {s:8s} {d.mean():20.2f} {100 * (d <= 2).mean():8.1f}% {d.max():4d} | "
              f"{int((py1 - py0)[pc].max()):>13d} / {int((c1 - c0)[pc].max()):<12d}  places {st[0]} outvoted {st[1]}")
    print(f"\n  worst mean difference with the swells off: {worst:.2f} bytes")
    sys.exit(0 if worst <= 2.0 else 1)


if __name__ == "__main__":
    main()
