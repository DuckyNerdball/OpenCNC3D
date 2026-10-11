#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- the generator: a 65x65 corner heightmap from tiles alone.

    python3 generate.py SCG22EA [--out DIR]     write <SCEN>.HGT (4225 raw bytes)

Everything it knows it measured from the 55 cartridge maps that ship a hand-made
heightmap (train()), and it works in three stages.

1. LEVELS. Every cliff placement (and every waterfall, which behaves the same way)
   says one thing with great reliability: the ground two cells beyond its high side
   is one rung above the ground two cells beyond its low side. Measured over 4150
   placements, 74% land within 58..70 bytes of each other, and a rung is 64. So
   each placement is one equation between the two sides, and every pair of
   neighbouring open cells is a weaker equation saying "same level". The solve is
   ROBUST (iteratively reweighted toward an L1 fit), and that is what makes it
   behave like the designers did: where a cliff line has gaps, the cheapest way to
   satisfy every placement is to close the plateau with the SHORTEST boundary across
   each gap, not to sag the whole plateau; and where two pieces of art contradict
   each other, the minority is out-voted instead of tearing the map.

2. RUNGS. The levels are rounded onto the console's ladder (0, 64, 128, 191, 255),
   the lowest substantial land on rung 1 so that carved water has room below it.

3. SHAPE. Each cell's four corners come from what its icon looked like on the
   cartridge: a cliff icon's corners sit at their measured fraction of the way from
   the low rung to the high one; every other icon's corners sit at a measured
   offset from its terrace (zero for grass, below for a river groove or a shore).
   Corners shared by several cells take the confidence-weighted mean of what each
   cell predicts. Where two terraces meet with no cliff art between them -- the gap
   in a cliff line, the designer's own ramp -- the step is spread into a ramp.
"""
import os, sys
import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as spl

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import corpus as C

N = 65
NC = N * N
RUNG = 64.0

# ------------------------------------------------------------- step pieces
# The high side of every piece that carries a whole rung, in grid axes (dy
# positive SOUTH). The SLOPE compass is the one the editor uses and that
# tools/romdump/cliff_compass.py checks per placement (97.96% agree); the two
# waterfalls were read the same way: FALLS1 is high in the east, FALLS2 in the north.
STEP_DIR = {}
STEP_MIN_SHARE = 0.75  # a cliff stamp counts as a step only with this share of its cells left
for _i in range(1, 8):   STEP_DIR[12 + _i] = (0, -1)
for _i in range(8, 15):  STEP_DIR[12 + _i] = (1, 0)
for _i in range(15, 22): STEP_DIR[12 + _i] = (0, 1)
for _i in range(22, 29): STEP_DIR[12 + _i] = (-1, 0)
for _i in (29, 35, 38):  STEP_DIR[12 + _i] = (1, -1)
for _i in (30, 36, 37):  STEP_DIR[12 + _i] = (1, 1)
for _i in (31, 33):      STEP_DIR[12 + _i] = (-1, 1)
for _i in (32, 34):      STEP_DIR[12 + _i] = (-1, -1)
STEP_DIR[C.TN.index("FALLS1")] = (1, 0)
STEP_DIR[C.TN.index("FALLS2")] = (0, -1)


def step_placements(m):
    """[(t, cells, (ux,uy), hi_cells, lo_cells)]: every cliff or waterfall placement
    with the open cells one and two beyond its high and its low side."""
    out = []
    stepmask = np.zeros((64, 64), bool)
    pls = []
    for (t, cx, cy, w, h, cells) in m.placements():
        if t in STEP_DIR and cells and len(cells) >= STEP_MIN_SHARE * w * h:
            pls.append((t, cells))
            for x, y, _ in cells:
                stepmask[y, x] = True
    for t, cells in pls:
        ux, uy = STEP_DIR[t]
        box = {(x, y) for x, y, _ in cells}
        hi, lo = set(), set()
        for x, y, _ in cells:
            for sgn, acc in ((1, hi), (-1, lo)):
                k = 1
                while (x + sgn * ux * k, y + sgn * uy * k) in box:
                    k += 1
                for d in (0, 1):
                    px, py = x + sgn * ux * (k + d), y + sgn * uy * (k + d)
                    if 0 <= px < 64 and 0 <= py < 64 and not stepmask[py, px]:
                        acc.add((px, py))
        out.append((t, cells, (ux, uy), sorted(hi), sorted(lo)))
    return out, stepmask


def cell_mean(h):
    return (h[:-1, :-1] + h[:-1, 1:] + h[1:, :-1] + h[1:, 1:]) / 4.0


def corners_of(h, x, y):
    return np.array([h[y, x], h[y, x + 1], h[y + 1, x], h[y + 1, x + 1]], np.float64)


def ladder(r):
    r = np.clip(r, 0, 4)
    return np.array(C.LADDER, np.float64)[r]


# ------------------------------------------------------------- water
WATER_BANK_SHARE = 0.15  # a bank terrace this large a share of the bank counts


def water_bodies(m, stepmask):
    """Connected bodies of water cells (the 1995 land type, so a shore's wet half
    counts and its beach half does not) and, for each, the open cells within two
    of it: its banks. Measured on the cartridge, a body's surface sits 20 to 24
    bytes below the LOWEST terrace that touches it (40 under 64, 108 under 128,
    171 under 191): water belongs to the low side, it never floats between two."""
    from scipy import ndimage as nd
    wet = (m.land == C.L_WATER) & ~stepmask
    lab, n = nd.label(wet)
    banks = []
    for k in range(1, n + 1):
        c = lab == k
        ring = nd.binary_dilation(c, iterations=2) & ~c & ~stepmask & (m.land != C.L_WATER)
        banks.append(ring)
    return lab, n, banks


def settle_water(rungs, lab, n, banks):
    """Every water body sits on the LOWEST terrace that makes up a real share of its
    bank (a stray cell does not count): a sea between a beach on rung 1 and a
    plateau on rung 2 is the beach's sea."""
    out = rungs.copy()
    for k in range(1, n + 1):
        b = rungs[banks[k - 1]]
        if not b.size:
            continue
        vals, cnt = np.unique(b, return_counts=True)
        ok = vals[cnt >= max(2, WATER_BANK_SHARE * b.size)]
        out[lab == k] = int(ok.min() if ok.size else vals.min())
    return out


# ------------------------------------------------------------- training
def train(maps):
    """What the generator knows, measured from `maps` (each with real heights):
         step[t]           rung delivered across a placement of template t
         frac[(t,i)]       4 corner fractions of the way from low side to high side
         off[(t,i)]        4 corner offsets from the cell's terrace, and their spread
    """
    steps, frac, off = {}, {}, {}
    for m in maps:
        cm = cell_mean(m.h.astype(np.float64))
        pl = m.playable()
        pls, stepmask = step_placements(m)
        for t, cells, u, hi, lo in pls:
            hi = [c for c in hi if pl[c[1], c[0]]]
            lo = [c for c in lo if pl[c[1], c[0]]]
            if not hi or not lo or not all(pl[y, x] for x, y, _ in cells):
                continue
            H = np.median([cm[y, x] for x, y in hi]); L = np.median([cm[y, x] for x, y in lo])
            steps.setdefault(t, []).append(H - L)
            if H - L < 24:
                continue                       # a level or backwards placement shapes nothing
            for x, y, i in cells:
                frac.setdefault((t, i), []).append((corners_of(m.h, x, y) - L) / (H - L))
        # the terrace each non-step cell sits on, read off the real map the same way
        # the generator will assign it: land cells on the rung nearest the median of
        # the open cells around them, water bodies settled onto their lowest bank
        lad = np.array(C.LADDER, np.float64)
        rr = np.zeros((64, 64), int)
        ys, xs = np.nonzero(~stepmask)
        for y, x in zip(ys, xs):
            y0, y1, x0, x1 = max(0, y - 2), min(64, y + 3), max(0, x - 2), min(64, x + 3)
            nb = ~stepmask[y0:y1, x0:x1] & np.isin(m.fam[y0:y1, x0:x1], OPEN)
            v = np.median(cm[y0:y1, x0:x1][nb]) if nb.any() else cm[y, x]
            rr[y, x] = int(np.argmin(np.abs(lad - v)))
        lab, n, banks = water_bodies(m, stepmask)
        rr = settle_water(rr, lab, n, banks)
        for y, x in zip(ys, xs):
            if not pl[y, x]:
                continue
            key = (int(m.tmpl[y, x]), int(m.icon[y, x]))
            off.setdefault(key, []).append(corners_of(m.h, x, y) - lad[rr[y, x]])
    model = {"step": {}, "frac": {}, "off": {}, "off_t": {}}
    for t, v in steps.items():
        model["step"][t] = float(np.median(v))
    for k, v in frac.items():
        v = np.array(v)
        model["frac"][k] = (np.median(v, 0), len(v))
    byt = {}
    for k, v in off.items():
        v = np.array(v)
        model["off"][k] = (np.median(v, 0), np.maximum(np.mean(np.abs(v - np.median(v, 0)), 0), 2.0), len(v))
        byt.setdefault(k[0], []).append(v)
    for t, vs in byt.items():
        v = np.concatenate(vs)
        model["off_t"][t] = (np.median(v, 0), np.maximum(np.mean(np.abs(v - np.median(v, 0)), 0), 2.0), len(v))
    return model


OPEN = [C.FAM[f] for f in ("CLEAR", "ROAD", "PATCH", "BRUSH", "BOULDER")]


def _nearest_rung(v):
    lad = np.array(C.LADDER, np.float64)
    return lad[np.argmin(np.abs(lad - v))]


def offset_prior(model, t, i):
    r = model["off"].get((t, i))
    if r is not None and r[2] >= 6:
        return r[0], r[1]
    r = model["off_t"].get(t)
    if r is not None:
        return r[0], r[1]
    return np.zeros(4), np.full(4, 8.0)


def frac_prior(model, t, i, w, h):
    r = model["frac"].get((t, i))
    if r is not None and r[1] >= 3:
        return r[0]
    # unseen icon: a plain linear climb across the template along its compass
    ux, uy = STEP_DIR[t]
    x, y = i % w, i // w
    out = []
    for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
        px = (x + dx) / w if ux > 0 else (1 - (x + dx) / w) if ux < 0 else 0.5
        py = (y + dy) / h if uy > 0 else (1 - (y + dy) / h) if uy < 0 else 0.5
        out.append((px + py) / 2 if ux and uy else (px if ux else py))
    return np.array(out)


# ------------------------------------------------------------- stage 1: levels
W_SMOOTH = 1.0
W_DIAG = 0.7
W_PLACE = 40.0
BASE_SITE_R = 5      # cells: the level clearing round a multiplayer start
W_FOOT = 10.0     # the cells of one pre-placed building share one level
LEVEL_FOOTPRINTS = True
W_BASE = 0.03      # every cell leans, weakly, toward the base level
W_WATERLAND = 1.0  # water is tied to its banks like any open ground, and settles afterwards
PLAYABLE_ONLY = True
W_BEACHLAND = 1.0  # beach to inland ground
W_BASE_WATER = 0.03  # water's own lean toward the base level
SETTLE_WATER = True
IRLS_ITERS = 15
IRLS_EPS = 1.0


def solve_levels(m, model, pls, stepmask):
    rows, cols, vals, rhs, w0 = [], [], [], [], []
    r = 0

    def add(entries, target, w):
        nonlocal r
        for c, v in entries:
            rows.append(r); cols.append(c); vals.append(v)
        rhs.append(target); w0.append(w); r += 1

    idx = lambda x, y: y * 64 + x
    # THE PLAYABLE RECTANGLE ONLY. Outside it nothing is drawn as play space and the
    # designers ran plateaus straight off its edge; a graph that went on outside it
    # would charge a plateau for closing in the border, and a single long cliff
    # would raise a thin ridge instead of everything on its high side.
    pl = m.playable() if PLAYABLE_ONLY else np.ones((64, 64), bool)
    wet = m.land == C.L_WATER
    beach = m.land == C.L_BEACH
    for y in range(64):
        for x in range(64):
            if not pl[y, x]:
                add(((idx(x, y), 1.0),), 0.0, 1.0)
                continue
            add(((idx(x, y), 1.0),), 0.0, W_BASE_WATER if wet[y, x] else W_BASE)
            for dx, dy, wb in ((1, 0, W_SMOOTH), (0, 1, W_SMOOTH), (1, 1, W_DIAG), (-1, 1, W_DIAG)):
                px, py = x + dx, y + dy
                if not (0 <= px < 64 and 0 <= py < 64) or not pl[py, px]:
                    continue
                if stepmask[y, x] or stepmask[py, px]:
                    w = 0.01
                elif wet[y, x] != wet[py, px]:
                    w = W_WATERLAND
                elif (beach[y, x] != beach[py, px]) and not wet[y, x]:
                    w = W_BEACHLAND * wb
                else:
                    w = wb
                if w > 0:
                    add(((idx(x, y), 1.0), (idx(px, py), -1.0)), 0.0, w)
    # A PRE-PLACED BUILDING STANDS ON ONE LEVEL. On the cartridge 77% of them stand on
    # corners the artists levelled; tie each footprint's cells together so a base is
    # never split across a ramp.
    if LEVEL_FOOTPRINTS:
        for typ, fc in m.footprints():
            fc = [(x, y) for x, y in fc if pl[y, x] and not stepmask[y, x]]
            for (x0, y0), (x1, y1) in zip(fc, fc[1:]):
                add(((idx(x0, y0), 1.0), (idx(x1, y1), -1.0)), 0.0, W_FOOT)
    # A MULTIPLAYER START IS A BASE SITE: the ground round it is tied to one level, so
    # a player's first buildings do not straddle a ramp
    if m.starts:
        site = m.base_sites(BASE_SITE_R) & pl & ~stepmask & (m.land != C.L_WATER)
        for y, x in zip(*np.nonzero(site)):
            for dx, dy in ((1, 0), (0, 1)):
                px, py = x + dx, y + dy
                if px < 64 and py < 64 and site[py, px]:
                    add(((idx(x, y), 1.0), (idx(px, py), -1.0)), 0.0, W_FOOT)
    for t, cells, u, hi, lo in pls:
        hi = [c for c in hi if pl[c[1], c[0]]]
        lo = [c for c in lo if pl[c[1], c[0]]]
        if not hi or not lo:
            continue
        step = model["step"].get(t, RUNG)
        step = RUNG if step < 32 else step
        e = [(idx(x, y), 1.0 / len(hi)) for x, y in hi] + [(idx(x, y), -1.0 / len(lo)) for x, y in lo]
        add(e, step, W_PLACE)
    A = sp.csr_matrix((vals, (rows, cols)), shape=(r, 4096))
    b = np.array(rhs); w0 = np.array(w0); w = w0.copy()
    reg = 1e-6 * sp.identity(4096)
    L = np.zeros(4096)
    for _ in range(IRLS_ITERS):
        At = A.T.multiply(w).tocsr()
        L = spl.spsolve((At @ A + reg).tocsc(), At @ b)
        res = A @ L - b
        w = w0 * IRLS_EPS / np.maximum(np.abs(res), IRLS_EPS)
    return L.reshape(64, 64)


# ------------------------------------------------------------- stage 2: rungs
def to_rungs(m, L, stepmask):
    open_ = ~stepmask & m.playable()
    dry = open_ & (m.land != C.L_WATER)
    v = L[dry] / RUNG if dry.any() else L.ravel() / RUNG
    ang = 2 * np.pi * v
    o = (np.arctan2(np.sin(ang).mean(), np.cos(ang).mean()) / (2 * np.pi)) % 1.0
    r = np.round(L / RUNG - o).astype(int)
    land = open_ & (m.land != C.L_WATER)
    lv = r[land] if land.any() else r[open_]
    vals, cnt = np.unique(lv, return_counts=True)
    keep = vals[cnt >= max(8, 0.02 * cnt.sum())]
    base = keep.min() if len(keep) else vals.min()
    r = np.clip(r - base + 1, 0, 4)
    # outside the playable rectangle: the nearest playable cell's rung, so the
    # terrain runs on past the edge instead of stepping down to nothing
    from scipy import ndimage as nd
    pl = m.playable()
    _, (iy, ix) = nd.distance_transform_edt(~pl, return_indices=True)
    r = r[iy, ix]
    if not SETTLE_WATER:
        return r
    lab, n, banks = water_bodies(m, stepmask)
    return settle_water(r, lab, n, banks)


# ------------------------------------------------------------- stage 3: shape
RAMP_LIMIT = 20     # bytes: a one-rung ramp spread over three to four cells, as the cartridge draws them
RAMP_ROUNDS = 12
RAMP_RING = 2
RAMP_ITERS = 8


def shape(m, model, rungs, pls, stepmask):
    T = ladder(rungs)
    num = np.zeros((N, N)); den = np.zeros((N, N))
    snum = np.zeros((N, N)); sden = np.zeros((N, N))
    for t, cells, u, hi, lo in pls:
        _, _, _, _, w, h = C.TT[t]
        rh = np.median([rungs[y, x] for x, y in hi]) if hi else None
        rl = np.median([rungs[y, x] for x, y in lo]) if lo else None
        if rh is None and rl is None:
            continue
        if rl is None: rl = rh - 1
        if rh is None: rh = rl + 1
        if rh <= rl:
            rh = rl + 1
        lo_v = float(np.interp(rl, range(5), C.LADDER))
        hi_v = float(np.interp(rh, range(5), C.LADDER))
        for x, y, i in cells:
            f = frac_prior(model, t, i, w, h)
            for k, (dx, dy) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
                snum[y + dy, x + dx] += lo_v + f[k] * (hi_v - lo_v); sden[y + dy, x + dx] += 1
    ys, xs = np.nonzero(~stepmask)
    for y, x in zip(ys, xs):
        mu, spread = offset_prior(model, int(m.tmpl[y, x]), int(m.icon[y, x]))
        for k, (dx, dy) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
            wgt = 1.0 / (spread[k] ** 2)
            num[y + dy, x + dx] += wgt * (T[y, x] + mu[k]); den[y + dy, x + dx] += wgt
    out = np.where(sden > 0, snum / np.maximum(sden, 1e-9), num / np.maximum(den, 1e-9))
    fixed = sden > 0
    # A BRIDGE IS A LEVEL DECK AT THE HEIGHT OF THE BANKS IT JOINS: measured on the
    # cartridge, a bridge cell sits a median 0 bytes from its banks (the deck spans the
    # groove instead of dipping into it).
    from scipy import ndimage as nd
    for (t, cx, cy, w, h, cells) in m.placements():
        if C.FAMILY_OF[t] != C.FAM["BRIDGE"] or not cells:
            continue
        pm = np.zeros((64, 64), bool)
        for x, y, _ in cells:
            pm[y, x] = True
        bank = nd.binary_dilation(pm, iterations=1) & ~pm & ~stepmask & \
            (m.land != C.L_WATER) & (m.fam != C.FAM["BRIDGE"])
        if not bank.any():
            continue
        deck = float(np.median(T[bank]))
        for x, y, _ in cells:
            out[y:y + 2, x:x + 2] = deck
            fixed[y:y + 2, x:x + 2] = True
    if LEVEL_FOOTPRINTS:
        # the building's corners on the highest terrace it stands on: the renderer's
        # own pad rule (raise, never bury), applied to the ground itself
        for typ, fc in m.footprints():
            fc = [(x, y) for x, y in fc if not stepmask[y, x] and m.land[y, x] != C.L_WATER]
            if not fc:
                continue
            top = max(T[y, x] for x, y in fc)
            for x, y in fc:
                out[y:y + 2, x:x + 2] = top
                fixed[y:y + 2, x:x + 2] = True
    # RAMPS. Where two terraces meet with no cliff art between them -- the gap in a
    # cliff line, the end of a fragment, the designer's own way up -- the step has to
    # become a slope a unit can be seen to walk. The cartridge's walkable cells
    # (not cliff, not water, not rock) span at most 33 bytes in 99 cases of 100, so
    # any walkable cell steeper than that has its free corners relaxed toward their
    # neighbours, together with the ring around it, until none is or nothing moves.
    # Cliff corners, building pads and water stay exactly where they were put.
    from scipy import ndimage as nd
    # Only a corner with water on all four sides is the water's: a shoreline corner
    # is shared with the land and may slope with it.
    wc = np.pad((m.land == C.L_WATER), 1, mode="edge")
    wet = wc[:-1, :-1] & wc[:-1, 1:] & wc[1:, :-1] & wc[1:, 1:]
    free = ~fixed & ~wet
    walk = ~stepmask & (m.land != C.L_WATER) & (m.land != C.L_ROCK) & (m.land != C.L_WALL)
    for _ in range(RAMP_ROUNDS):
        c = np.stack([out[:-1, :-1], out[:-1, 1:], out[1:, :-1], out[1:, 1:]], -1)
        bad = walk & ((c.max(-1) - c.min(-1)) > RAMP_LIMIT)
        if not bad.any():
            break
        cb = np.zeros((N, N), bool)
        cb[:-1, :-1] |= bad; cb[:-1, 1:] |= bad; cb[1:, :-1] |= bad; cb[1:, 1:] |= bad
        band = nd.binary_dilation(cb, iterations=RAMP_RING) & free
        for _ in range(RAMP_ITERS):
            p = np.pad(out, 1, mode="edge")
            avg = (p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]) / 4.0
            out = np.where(band, avg, out)
    if SWELLS:
        out = out + swells(m, out, stepmask, fixed)
    return out


# ------------------------------------------------------------- stage 4: swells
# ROLLING GROUND IN THE BIG OPEN PATCHES. Not the cartridge's: its designers left most
# open ground level. A deliberate addition: where a map has a large stretch with no
# cliff, water, shore, bridge, ramp or starting building, that stretch is given broad,
# rounded rises, taller the further they are from anything that must stay where it is,
# with long smooth falloffs and flat-topped crowns rather than peaks.
SWELLS = True
SWELL_MAX = 96.0       # bytes at the crown of a swell in the most open ground (a rung is 64)
SWELL_NEAR = 1.5       # cells from the nearest fixed thing where a swell may begin
SWELL_FULL = 11.0      # cells from it where a swell may reach its full height
SWELL_SIGMA = 6.0      # cells: the breadth of the rolling field
SWELL_LOW, SWELL_HIGH = -0.1, 1.2   # field values (in its own spread) where a rise starts and crowns
SWELL_BIAS = 0.9       # how far (in the field's spread) the most open ground leans upward
SWELL_STEP = 9.0       # bytes a corner may stand above a neighbour: a cell climbs at most ~18
SWELL_PASSES = 40


def _smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def swells(m, out, stepmask, fixed):
    """The lift to add to every corner (0 wherever anything must stay put)."""
    import zlib
    from scipy import ndimage as nd
    keep = stepmask | (m.land == C.L_WATER) | (m.land == C.L_ROCK) | (m.land == C.L_WALL)
    keep |= np.isin(m.fam, [C.FAM[f] for f in ("WATER", "RIVER", "SHORE", "FORD", "FALLS", "BRIDGE")])
    for typ, fc in m.footprints():
        for x, y in fc:
            keep[y, x] = True
    if m.starts:
        keep |= m.base_sites(BASE_SITE_R)       # a base site stays level
    c = np.stack([out[:-1, :-1], out[:-1, 1:], out[1:, :-1], out[1:, 1:]], -1)
    keep |= (c.max(-1) - c.min(-1)) > 2          # any ramp, any edge of a terrace
    kc = np.zeros((N, N), bool)
    kc[:-1, :-1] |= keep; kc[:-1, 1:] |= keep; kc[1:, :-1] |= keep; kc[1:, 1:] |= keep
    kc |= fixed
    room = nd.distance_transform_edt(~kc)
    w = _smoothstep(SWELL_NEAR, SWELL_FULL, room)
    if not (w > 0).any():
        return np.zeros((N, N))
    # one rolling field per map, the same every run: seeded by the scenario's name
    rng = np.random.default_rng(zlib.crc32(m.id.encode()))
    f = nd.gaussian_filter(rng.standard_normal((N + 24, N + 24)), SWELL_SIGMA)[12:-12, 12:-12]
    f /= f.std()
    # the most open ground is where a rise belongs: the field leans upward there, so a
    # big empty stretch reliably gets a crown and not just whatever the dice gave it
    f = f + SWELL_BIAS * w
    lift = SWELL_MAX * w * _smoothstep(SWELL_LOW, SWELL_HIGH, f)
    lift = np.minimum(lift, np.maximum(0.0, 250.0 - out))
    # GENTLE EVERYWHERE, LOCALLY. A corner may stand at most SWELL_STEP above any of its
    # four neighbours, so a cell climbs at most about twice that; where the field asks
    # for more, the high side is cut back (never the low side raised), and a light blur
    # then rounds the cut so it reads as a slope and not as a crease. Crowns with room
    # around them keep their full height.
    cross = np.array([[0, 1, 0], [1, 1, 1], [0, 1, 0]], bool)
    for _ in range(SWELL_PASSES):
        lift = np.minimum(lift, nd.minimum_filter(lift, footprint=cross, mode="nearest") + SWELL_STEP)
    lift = nd.gaussian_filter(lift, 1.0, mode="nearest")
    lift[kc] = 0.0
    for _ in range(SWELL_PASSES):
        lift = np.minimum(lift, nd.minimum_filter(lift, footprint=cross, mode="nearest") + SWELL_STEP)
    return lift


# ------------------------------------------------------------- the generators
def generate(m, model):
    pls, stepmask = step_placements(m)
    L = solve_levels(m, model, pls, stepmask)
    rungs = to_rungs(m, L, stepmask)
    return np.clip(np.round(shape(m, model, rungs, pls, stepmask)), 0, 255).astype(np.int16)


def levels_only(m, model):
    """Stage 1 alone, offset to sit on rung 1: for looking at the raw solve."""
    pls, stepmask = step_placements(m)
    L = solve_levels(m, model, pls, stepmask)
    h = np.zeros((N, N)); h[:-1, :-1] = L; h[-1, :] = h[-2, :]; h[:, -1] = h[:, -2]
    return np.clip(h - np.percentile(L, 10) + 64, 0, 255).astype(np.int16)


def flat(m, model=None):
    return np.full((N, N), 64, np.int16)


def write_hgt(h, path):
    open(path, "wb").write(np.clip(h, 0, 255).astype(np.uint8).tobytes())


DATA = os.path.join(C.ROOT, "data", "autoheight")


def _tiles_key(s):
    return open(os.path.join(C.EX, "MAP", s + ".MAP"), "rb").read()[:8192]


def heights_for(m, model):
    """A map's heights, honouring its TWINS. Several scenarios share another's tiles
    and playable rectangle byte for byte (a test variant built on a campaign mission's ground, two copies of
    one development map). Where the twin ships a hand-made heightmap, that is the
    answer: the cartridge's own art beats anything generated. Otherwise every twin
    gets the heights generated for the first of them by name, so the same ground never
    rolls two different ways."""
    rect = lambda mm: (mm.X, mm.Y, mm.W, mm.H)
    key = _tiles_key(m.id)
    # the same tiles AND the same playable rectangle: the solve works inside that
    # rectangle, and a hand-made heightmap is only art inside its own mission's
    twins = sorted(t for t in C.all_ids() if _tiles_key(t) == key and rect(C.Map(t)) == rect(m))
    for t in twins:
        tm = C.Map(t)
        if tm.h is not None:
            return tm.h.copy(), t
    first = twins[0]
    return generate(m if first == m.id else C.Map(first), model), (None if first == m.id else first)


def main():
    """generate.py SCEN ... [--out DIR]   write <SCEN>.HGT for each map
       generate.py --covert-ops            the thirteen, into data/autoheight
       generate.py --multiplayer           the nine skirmish maps, into data/autoheight
       generate.py --bonus                 the bonus missions and test scenarios, likewise
       generate.py --verify                regenerate everything in data/autoheight and
                                           say which committed maps the generator would
                                           now draw differently (it never writes)"""
    args = sys.argv[1:]
    out = "."
    if "--out" in args:
        k = args.index("--out"); out = args[k + 1]; del args[k:k + 2]
    model = train(C.training())
    if "--verify" in args:
        drift = 0
        for f in sorted(os.listdir(DATA)):
            if not f.endswith(".HGT"):
                continue
            s = f[:-4]
            have = np.frombuffer(open(os.path.join(DATA, f), "rb").read(), np.uint8).reshape(N, N)
            now = np.clip(heights_for(C.Map(s), model)[0], 0, 255).astype(np.uint8)
            d = int((have != now).sum())
            drift += d > 0
            print(f"{s}: {'unchanged' if not d else f'{d} corners differ, max {int(np.abs(have.astype(int) - now).max())} bytes'}")
        sys.exit(1 if drift else 0)
    if "--covert-ops" in args:
        args = list(C.COVERT_OPS)
        out = DATA
    if "--bonus" in args:
        args = list(C.BONUS + C.TEST)
        out = DATA
    if "--multiplayer" in args:
        args = list(C.MULTIPLAYER)
        out = DATA
    os.makedirs(out, exist_ok=True)
    for s in args:
        m = C.Map(s)
        if m.h is not None:
            print(f"{s}: the cartridge ships a hand-made heightmap; generating anyway")
        h, src = heights_for(m, model)
        write_hgt(h, os.path.join(out, s + ".HGT"))
        print(s, "->", os.path.join(out, s + ".HGT"), f"(the heights of its twin {src})" if src else "")


if __name__ == "__main__":
    main()
