#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- the scoreboard.

    python3 score.py [generator ...] [--maps SCG05EA,SCB22EB] [--json out.json]

For each of the 55 cartridge maps that ship a hand-made heightmap: throw the
heights away, regenerate them from the tiles alone with a model trained on the
OTHER missions, and compare with what the designers drew. Leave-one-MISSION-out,
not leave-one-map-out: SCG05EA and SCG05WA share an identical tile map, and most
EA/EB variants share most of their ground, so a model that has seen a sister
variant would be scored on a map it has effectively memorised.

All measurements are inside the playable rectangle, the only part ever drawn.

    mae     mean |generated - real| per corner, bytes (64 = one cell of height)
    amae    the same after the best constant offset: whether the SHAPE is right
            even when the whole map sits a rung higher or lower
    tier    open-land cells whose rung (after that offset) matches the real one
    steepF  F1 of "this cell is steep" (corner range >= 16 bytes): is the relief
            in the right PLACES
    wet     water-family cells that sit below their banks, generated vs real

Generators are named functions in generate.py taking (map, model). Default:
flat and generate.
"""
import os, sys, json, collections
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import corpus as C
import generate as G

WATERY = [C.FAM[f] for f in ("WATER", "RIVER", "SHORE", "FORD", "FALLS")]
OPEN = [C.FAM[f] for f in ("CLEAR", "ROAD", "PATCH", "BRUSH", "BOULDER")]


def mission(sid):
    return sid[:5]


def steep(h):
    c = np.stack([h[:-1, :-1], h[:-1, 1:], h[1:, :-1], h[1:, 1:]], -1)
    return (c.max(-1) - c.min(-1)) >= 16


def below_banks(m, h):
    """Fraction of water-family cells whose mean sits below the mean of the open
    land within two cells."""
    cm = (h[:-1, :-1] + h[:-1, 1:] + h[1:, :-1] + h[1:, 1:]) / 4.0
    wet = np.isin(m.fam, WATERY) & m.playable()
    land = np.isin(m.fam, OPEN)
    n = k = 0
    for y, x in zip(*np.nonzero(wet)):
        y0, y1, x0, x1 = max(0, y - 2), min(64, y + 3), max(0, x - 2), min(64, x + 3)
        l = land[y0:y1, x0:x1]
        if not l.any():
            continue
        n += 1
        k += cm[y, x] < cm[y0:y1, x0:x1][l].mean() - 2
    return k / n if n else float("nan")


def score_one(m, gen):
    real = m.h.astype(np.float64)
    g = gen.astype(np.float64)
    pc = m.playable_corners()
    d = (g - real)[pc]
    off = np.median(d)
    pl = m.playable()
    cr = (real[:-1, :-1] + real[:-1, 1:] + real[1:, :-1] + real[1:, 1:]) / 4.0
    cg = (g[:-1, :-1] + g[:-1, 1:] + g[1:, :-1] + g[1:, 1:]) / 4.0 - off
    op = pl & np.isin(m.fam, OPEN)
    tier = (np.round(cr[op] / 64) == np.round(cg[op] / 64)).mean()
    sr, sg = steep(real) & pl, steep(g) & pl
    tp = (sr & sg).sum()
    f1 = 2 * tp / max(1, sr.sum() + sg.sum())
    return dict(mae=float(np.abs(d).mean()), amae=float(np.abs(d - off).mean()),
                tier=float(tier), steepF=float(f1),
                wet=float(below_banks(m, g)), wet_real=float(below_banks(m, m.h.astype(float))))


def run(gens, only=None):
    maps = C.training()
    by = collections.defaultdict(list)
    for m in maps:
        by[mission(m.id)].append(m)
    res = {g: {} for g in gens}
    for mid, group in sorted(by.items()):
        if only and not any(m.id in only for m in group):
            continue
        model = G.train([m for m in maps if mission(m.id) != mid])
        for m in group:
            if only and m.id not in only:
                continue
            for gname in gens:
                res[gname][m.id] = score_one(m, getattr(G, gname)(m, model))
    return res


def summary(res):
    keys = ("mae", "amae", "tier", "steepF", "wet")
    print(f"  {'generator':14s} " + " ".join(f"{k:>7s}" for k in keys))
    for g, per in res.items():
        vals = [np.nanmean([r[k] for r in per.values()]) for k in keys]
        print(f"  {g:14s} " + " ".join(f"{v:7.3f}" for v in vals))
    real = [r["wet_real"] for r in next(iter(res.values())).values()]
    print(f"  {'(cartridge)':14s} {'':>7s} {'':>7s} {'':>7s} {'':>7s} {np.nanmean(real):7.3f}")


def main():
    args = sys.argv[1:]
    only, jout = None, None
    if "--maps" in args:
        k = args.index("--maps"); only = set(args[k + 1].split(",")); del args[k:k + 2]
    if "--json" in args:
        k = args.index("--json"); jout = args[k + 1]; del args[k:k + 2]
    gens = args or ["flat", "generate"]
    res = run(gens, only)
    if len(next(iter(res.values()))) <= 12:
        for g, per in res.items():
            for sid, r in per.items():
                print(f"  {g:12s} {sid}  " + "  ".join(f"{k} {v:.3f}" for k, v in r.items()))
    summary(res)
    if jout:
        json.dump(res, open(jout, "w"), indent=1)


if __name__ == "__main__":
    main()
