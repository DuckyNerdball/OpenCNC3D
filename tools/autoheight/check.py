#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- the playability check.

    python3 check.py [SCEN ...]        default: the thirteen Covert Ops missions

The 1995 engine has no height at all: passability, pathing, line of sight and
weapons ranges are decided on the flat grid, so a heightmap can never make an area
unreachable or a base unbuildable. What it CAN do is look wrong for play: units
walking up a wall on open ground, a bridge deck on a tilt, a lake that slopes, a
starting base split across a ramp, a hole or a spike. Each of those is measured
here against the cartridge's own 55 hand-made heightmaps, and a generated map
passes when it is no worse than the worst the designers shipped.

    steep    passable cells (not cliff, not water, not rock) whose corners span more
             than the cartridge's 99th percentile on such cells
    bridge   bridge cells whose deck is not level (corner span > 8 bytes)
    water    open-water cells (water on all eight sides, falls excepted) whose
             corners span more than 8 bytes: a surface that slopes
    base     pre-placed buildings standing on unlevelled corners
    spike    corners more than one rung away from all four neighbours' mean
    peak     walkable corners standing more than 6 bytes above their four neighbours'
             mean: a pointed top rather than a rounded one
"""
import os, sys
import numpy as np
from scipy import ndimage as nd

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import corpus as C
import generate as G

BRIDGE = C.FAM["BRIDGE"]
FALLS = C.FAM["FALLS"]


def spans(h):
    c = np.stack([h[:-1, :-1], h[:-1, 1:], h[1:, :-1], h[1:, 1:]], -1).astype(int)
    return c.max(-1) - c.min(-1)


def passable(m, stepmask):
    return ~stepmask & (m.land != C.L_WATER) & (m.land != C.L_ROCK) & (m.land != C.L_WALL)


def measure(m, h, steep_limit):
    pl = m.playable()
    _, stepmask = G.step_placements(m)
    sp = spans(h)
    pas = passable(m, stepmask) & pl
    steep = int((pas & (sp > steep_limit)).sum())
    br = pl & (m.fam == BRIDGE)
    bridge = int((br & (sp > 8)).sum())
    w = (m.land == C.L_WATER) & (m.fam != FALLS) & ~stepmask
    inner = w & (nd.minimum_filter(w.astype(np.uint8), size=3, mode="nearest") > 0)
    water = int((pl & inner & (sp > 8)).sum())
    base = n = 0
    for typ, fc in m.footprints():
        fc = [(x, y) for x, y in fc if pl[y, x]]
        if not fc:
            continue
        cs = [int(h[y + dy, x + dx]) for x, y in fc for dx in (0, 1) for dy in (0, 1)]
        n += 1
        base += (max(cs) - min(cs)) > 1
    hp = np.pad(h.astype(float), 1, mode="edge")
    nb = (hp[:-2, 1:-1] + hp[2:, 1:-1] + hp[1:-1, :-2] + hp[1:-1, 2:]) / 4.0
    spike = int((np.abs(h - nb)[m.playable_corners()] > 64).sum())
    # a sharp crown: a walkable corner standing more than 6 bytes above the mean of its
    # four neighbours is a point, not a rounded top
    wc = np.zeros((65, 65), bool)
    ok = passable(m, stepmask) & pl
    wc[:-1, :-1] |= ok; wc[:-1, 1:] |= ok; wc[1:, :-1] |= ok; wc[1:, 1:] |= ok
    bc = np.zeros((65, 65), bool)
    nok = ~ok
    bc[:-1, :-1] |= nok; bc[:-1, 1:] |= nok; bc[1:, :-1] |= nok; bc[1:, 1:] |= nok
    peak = int(((h - nb) > 6)[wc & ~bc].sum())
    return dict(cells=int(pl.sum()), steep=steep, bridge=bridge, water=water,
                base=base, buildings=n, spike=spike, peak=peak, max_pass=int(sp[pas].max()) if pas.any() else 0)


def cartridge_limits():
    v = []
    for m in C.training():
        _, stepmask = G.step_placements(m)
        v.append(spans(m.h)[passable(m, stepmask) & m.playable()])
    v = np.concatenate(v)
    return int(np.percentile(v, 99)), int(v.max())


def main():
    ids = sys.argv[1:] or list(C.COVERT_OPS)
    lim, worst = cartridge_limits()
    print(f"cartridge passable cells: 99th percentile span {lim} bytes, steepest {worst}")
    model = G.train(C.training())
    real = {m.id: m for m in C.training()}
    print(f"\n  {'map':8s} {'cells':>5s} {'steep':>5s} {'maxP':>4s} {'bridge':>6s} {'water':>5s} "
          f"{'base':>9s} {'spike':>5s} {'peak':>4s}")
    for s in ids:
        m = real.get(s) or C.Map(s)
        h = G.generate(m, model)
        r = measure(m, h, lim)
        print(f"  {s:8s} {r['cells']:5d} {r['steep']:5d} {r['max_pass']:4d} {r['bridge']:6d} "
              f"{r['water']:5d} {r['base']:4d}/{r['buildings']:<4d} {r['spike']:5d} {r['peak']:4d}")
        if s in real:
            r = measure(m, m.h, lim)
            print(f"  {'(real)':8s} {r['cells']:5d} {r['steep']:5d} {r['max_pass']:4d} {r['bridge']:6d} "
                  f"{r['water']:5d} {r['base']:4d}/{r['buildings']:<4d} {r['spike']:5d}")


if __name__ == "__main__":
    main()
