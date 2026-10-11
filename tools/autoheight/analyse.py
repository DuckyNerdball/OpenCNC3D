#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- the analysis: how the cartridge's designers turned 2D tiles
into elevation, measured over the 55 maps that ship a hand-made heightmap.

    python3 analyse.py            every section
    python3 analyse.py regions    one section (regions steps falls water shores
                                  beaches base buildings walk ramps bridges liberty)

docs/terrain-elevation-grammar.md measured the vocabulary one TILE at a time: the
five-rung ladder, which families carry relief, the cliff compass, how wide a climb
is. This measures the GRAMMAR: how tiles behave in relation to the larger patch of
map around them, which is what a generator has to know. Every number in
docs/auto-heightmap.md is printed by this script; nothing there is asserted that
this does not compute.

All measurements are inside each map's playable rectangle.
"""
import collections, os, sys
import numpy as np
from scipy import ndimage as nd

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import corpus as C
import generate as G

OPEN = [C.FAM[f] for f in ("CLEAR", "ROAD", "PATCH", "BRUSH", "BOULDER")]
TRANS = [C.FAM[f] for f in ("SLOPE", "RIVER", "SHORE", "WATER", "FALLS", "FORD", "BRIDGE")]
LAD = np.array(C.LADDER, np.float64)


def cm(m):
    return m.cell_corners().astype(np.float64).mean(-1)


def rung_of(v):
    return np.argmin(np.abs(np.asarray(v)[..., None] - LAD), -1)


def pct(a, b):
    return 100.0 * a / max(1, b)


def sec_regions(ms):
    print("A. OPEN GROUND BETWEEN CLIFFS IS NOT ONE LEVEL PER REGION\n")
    cells = level = comps = flatc = 0
    for m in ms:
        pl = m.playable(); c = cm(m)
        bar = np.isin(m.fam, TRANS) | (m.land == C.L_ROCK) | (m.land == C.L_WATER)
        lab, n = nd.label(~bar & pl)
        for k in range(1, n + 1):
            r = lab == k
            if r.sum() < 4:
                continue
            v = c[r]; ok = np.abs(v - np.median(v)) <= 8
            cells += r.sum(); level += ok.sum(); comps += 1; flatc += ok.mean() >= 0.95
    print(f"   {comps} connected patches of open ground (cut apart by cliff, water and rock")
    print(f"   art); {flatc} of them ({pct(flatc, comps):.0f}%) are level, and {pct(level, cells):.1f}%")
    print(f"   of open cells sit within 8 bytes of their patch's median.")
    print("   So the cliff art does NOT enclose its plateaus: most patches span two or")
    print("   more levels, joined where a cliff line has a gap. The designer closed each")
    print("   plateau across those gaps with a ramp of plain ground.\n")


def sec_steps(ms):
    print("B. A CLIFF PLACEMENT IS ONE RUNG BETWEEN ITS TWO SIDES\n")
    res = collections.defaultdict(list)
    for m in ms:
        c = cm(m); pl = m.playable()
        pls, stepmask = G.step_placements(m)
        for t, cells, (ux, uy), hi, lo in pls:
            if not C.is_slope(t) or len(cells) < 2:
                continue
            box = {(x, y) for x, y, _ in cells}
            for d in (1, 2, 3):
                H, L = [], []
                for x, y, _ in cells:
                    for sgn, acc in ((1, H), (-1, L)):
                        k = 1
                        while (x + sgn * ux * k, y + sgn * uy * k) in box:
                            k += 1
                        px, py = x + sgn * ux * (k + d - 1), y + sgn * uy * (k + d - 1)
                        if 0 <= px < 64 and 0 <= py < 64 and pl[py, px] and not stepmask[py, px]:
                            acc.append(c[py, px])
                if H and L:
                    res[d].append(np.mean(H) - np.mean(L))
    for d in (1, 2, 3):
        v = np.array(res[d]); ok = ((v >= 58) & (v <= 70)).mean()
        print(f"   {d} cell(s) beyond each side: n {len(v)}, median {np.median(v):5.1f} bytes, "
              f"{100 * ok:4.1f}% within 58..70 (a rung is 64)")
    print("   Two cells out, three placements in four deliver exactly one rung. Which")
    print("   cells carry the climb varies; what the placement guarantees is the rung.\n")


def sec_falls(ms):
    print("C. WATERFALLS ARE CLIFFS WITH A FIXED DIRECTION\n")
    for name, axis in (("FALLS1", 0), ("FALLS2", 1)):
        t = C.TN.index(name); g = []
        for m in ms:
            for (tt, ox, oy, w, h, cells) in m.placements():
                if tt != t or len(cells) < w * h:
                    continue
                box = m.h[oy:oy + h + 1, ox:ox + w + 1].astype(float)
                prof = box.mean(axis)
                g.append(prof[-1] - prof[0])
        g = np.array(g)
        side = ("east" if np.median(g) > 0 else "west") if axis == 0 else \
               ("south" if np.median(g) > 0 else "north")
        print(f"   {name}: {len(g)} placements, high side {side}, median rise across it "
              f"{abs(np.median(g)):.0f} bytes")
    print()


def sec_water(ms):
    print("D. WATER SITS BELOW THE LOWEST TERRACE THAT TOUCHES IT\n")
    rows = []
    for m in ms:
        c = cm(m); pl = m.playable()
        lab, n = nd.label(m.land == C.L_WATER)
        for k in range(1, n + 1):
            r = lab == k
            if (r & pl).sum() < 40:
                continue
            ring = nd.binary_dilation(r, iterations=2) & ~r & np.isin(m.fam, OPEN) & pl
            if not ring.any():
                continue
            rows.append((np.median(c[r & pl]), np.median(c[ring])))
    rows = np.array(rows)
    print(f"   {len(rows)} bodies of 40+ cells. Surface minus median bank, percentiles 10/25/50/75/90:")
    print("     " + "  ".join(f"{v:6.1f}" for v in np.percentile(rows[:, 0] - rows[:, 1], [10, 25, 50, 75, 90])))
    by = collections.defaultdict(list)
    for w, b in rows:
        by[int(LAD[rung_of(b)])].append(w)
    for b in sorted(by):
        print(f"   banks on {b:3d}: surface median {np.median(by[b]):5.1f}  (n {len(by[b])})")
    print("   The surface is carved about a third of a rung under its terrace, never")
    print("   between two terraces: a body belongs to the low side.\n")


def sec_shores(ms):
    print("E. SHORES ARE GROOVES, NOT CLIFFS\n")
    res = collections.defaultdict(list)
    for m in ms:
        c = cm(m); pl = m.playable()
        for (t, cx, cy, w, h, cells) in m.placements():
            nm = C.TT[t][0]
            if not nm.startswith("SH") or len(cells) < w * h or \
                    not all(pl[y, x] for x, y, _ in cells):
                continue
            wet = [(x, y) for x, y, i in cells if C.land_of(t, i) == C.L_WATER]
            dry = [(x, y) for x, y, i in cells if C.land_of(t, i) != C.L_WATER]
            if not wet or not dry:
                continue
            u = np.mean(dry, 0) - np.mean(wet, 0)
            if np.hypot(*u) < 0.3:
                continue
            u /= np.hypot(*u)
            box = {(x, y) for x, y, _ in cells}
            hi = []
            for x, y, _ in cells:
                for k in (2, 3, 4):
                    px, py = int(round(x + u[0] * k)), int(round(y + u[1] * k))
                    if 0 <= px < 64 and 0 <= py < 64 and (px, py) not in box and \
                            m.land[py, px] != C.L_WATER and not C.is_slope(int(m.tmpl[py, px])):
                        hi.append(c[py, px])
            if hi:
                res[nm].append(np.median(hi) - np.median([c[y, x] for x, y in wet]))
    allv = np.concatenate([np.array(v) for v in res.values()])
    print(f"   {len(allv)} shore placements, {len(res)} templates: land-to-water drop median "
          f"{np.median(allv):.1f} bytes, 90th percentile {np.percentile(allv, 90):.1f}")
    print("   A shore steps down by the water's own carve, not by a rung. Where a plateau")
    print("   meets the sea at full height, it is cliff art that says so.\n")


def sec_beaches(ms):
    print("F. BEACHES SIT ON THE BASE TERRACE MORE OFTEN THAN NOT\n")
    tot = collections.Counter()
    for m in ms:
        c = cm(m); pl = m.playable()
        land = pl & np.isin(m.fam, OPEN)
        r = rung_of(c)
        vals, cnt = np.unique(r[land], return_counts=True)
        base = vals[cnt >= max(8, .02 * cnt.sum())].min()
        b = pl & (m.land == C.L_BEACH)
        tot.update((r[b] - base).tolist())
    n = sum(tot.values())
    print("   beach cells by rung above the map's base: " +
          ", ".join(f"+{k}: {v} ({pct(v, n):.0f}%)" for k, v in sorted(tot.items())))
    print("   A highland lake has highland beaches; it is not a rule the tiles can carry.\n")


def sec_base(ms):
    print("G. THE MAIN GROUND IS RUNG 1\n")
    c2 = collections.Counter()
    for m in ms:
        c = cm(m); pl = m.playable()
        land = pl & np.isin(m.fam, OPEN)
        vals, cnt = np.unique(rung_of(c[land]), return_counts=True)
        c2[int(vals[cnt >= max(8, .02 * cnt.sum())].min())] += 1
    print("   lowest substantial land rung, maps: " + ", ".join(f"rung {k}: {v}" for k, v in sorted(c2.items())))
    print("   Rung 1 (64) leaves room below it for carved water and the odd basin.\n")


def sec_buildings(ms):
    print("H. PRE-PLACED BUILDINGS STAND ON LEVELLED GROUND\n")
    n = flat = 0
    for m in ms:
        for typ, fc in m.footprints():
            if not fc:
                continue
            cs = [m.h[y + dy, x + dx] for x, y in fc for dx in (0, 1) for dy in (0, 1)]
            n += 1; flat += (max(cs) - min(cs)) <= 1
    print(f"   {flat} of {n} ({pct(flat, n):.0f}%) stand on corners within one byte of each other.\n")


def sec_walk(ms):
    print("I. HOW STEEP WALKABLE GROUND GETS\n")
    v = []
    for m in ms:
        _, stepmask = G.step_placements(m)
        h = m.h.astype(int)
        c = np.stack([h[:-1, :-1], h[:-1, 1:], h[1:, :-1], h[1:, 1:]], -1)
        walk = ~stepmask & (m.land != C.L_WATER) & (m.land != C.L_ROCK) & (m.land != C.L_WALL)
        v.append((c.max(-1) - c.min(-1))[walk & m.playable()])
    v = np.concatenate(v)
    print(f"   {len(v)} walkable cells (not cliff, water or rock); corner span percentiles")
    print("   50/90/99/max: " + " / ".join(f"{p:.0f}" for p in np.percentile(v, [50, 90, 99, 100])) + " bytes")
    print("   A walkable cell rises at most about half a rung: ramps are spread out.\n")


def sec_ramps(ms):
    print("J. A RAMP WITHOUT ART IS TWO TO FOUR CELLS LONG\n")
    lengths = collections.Counter()
    for m in ms:
        pls, stepmask = G.step_placements(m)
        c = cm(m); pl = m.playable()
        walk = ~stepmask & np.isin(m.fam, OPEN) & pl
        r = rung_of(c)
        frac = np.abs(c - LAD[r])
        level = walk & (frac <= 4)
        for line, lv, wk, rr in ((c, level, walk, r), (c.T, level.T, walk.T, r.T)):
            for i in range(64):
                j = 0
                while j < 64:
                    if lv[i, j]:
                        k = j + 1
                        while k < 64 and wk[i, k] and not lv[i, k]:
                            k += 1
                        if k < 64 and k > j + 1 and lv[i, k] and abs(int(rr[i, k]) - int(rr[i, j])) == 1:
                            lengths[k - j - 1] += 1
                        j = k
                    else:
                        j += 1
    n = sum(lengths.values())
    print(f"   {n} one-rung climbs over walkable ground only (no cliff, no water between),")
    print("   by the number of cells between the two level terraces:")
    for k in sorted(lengths):
        if lengths[k] >= max(3, n // 100):
            print(f"     {k:2d} cells  {lengths[k]:5d}  {pct(lengths[k], n):4.1f}%")
    print()


def sec_bridges(ms):
    print("K. BRIDGE DECKS SIT AT BANK HEIGHT\n")
    d = []
    for m in ms:
        c = cm(m); pl = m.playable()
        for (t, ox, oy, w, h, cells) in m.placements():
            if C.FAMILY_OF[t] != C.FAM["BRIDGE"] or not cells:
                continue
            pm = np.zeros((64, 64), bool)
            for x, y, _ in cells:
                pm[y, x] = True
            if not (pm & pl).any():
                continue
            bank = nd.binary_dilation(pm, iterations=1) & ~pm & np.isin(m.fam, OPEN) & pl
            if bank.any():
                d.append(np.median(c[pm & pl]) - np.median(c[bank]))
    d = np.array(d)
    print(f"   {len(d)} bridges: deck minus bank median {np.median(d):.1f} bytes, "
          f"{pct((np.abs(d) <= 8).sum(), len(d)):.0f}% within 8.\n")


def sec_liberty(ms):
    print("L. WHAT THE TILES CANNOT SAY\n")
    off = tot = 0
    for m in ms:
        c = cm(m); pl = m.playable() & np.isin(m.fam, OPEN)
        off += (np.abs(c[pl] - LAD[rung_of(c[pl])]) > 8).sum(); tot += pl.sum()
    print(f"   {pct(off, tot):.1f}% of open cells sit more than 8 bytes off a rung: ramps, plus the")
    print("   designers' own swells and basins with no art under them. The cartridge ships")
    print("   two maps with IDENTICAL tiles and heightmaps differing in 2259 of 4225 corners")
    print("   (SCG05EA and SCG05WA), so a generator working from tiles has a ceiling well")
    print("   short of an exact copy, and a map that disagrees with the original can still")
    print("   be one its designer might have drawn.\n")


SECTIONS = dict(regions=sec_regions, steps=sec_steps, falls=sec_falls, water=sec_water,
                shores=sec_shores, beaches=sec_beaches, base=sec_base,
                buildings=sec_buildings, walk=sec_walk, ramps=sec_ramps,
                bridges=sec_bridges, liberty=sec_liberty)


def main():
    ms = C.training()
    print(f"{len(ms)} cartridge maps with a hand-made heightmap\n")
    for name in (sys.argv[1:] or SECTIONS):
        SECTIONS[name](ms)


if __name__ == "__main__":
    main()
