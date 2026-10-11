#!/usr/bin/env python3
"""The small scattered rocks, remade: fifteen angular stones painted in the hand-painted
method of the desert rocks (ROCK1 to ROCK7: tools/art/remaster/build_ROCKS.py and
pass_ROCKS.py), one pack per theater.

    python3 tools/art/rocks/build_stones.py --out-dir <dir> [--theater DESERT ...]
        [--sheet <png>] [--quick]

writes <dir>/rock3d_DESERT.pack, rock3d_TEMPERAT.pack and rock3d_WINTER.pack (or the
theaters named), each in the leafy trees' TREE3D4 format exactly as rock3d.pack is
(game/bake_tree3d.py write_pack3; tools/art/rocks/bake_rock3d.py): the codes
RK<stone><A|B|C>, stones 1 to 10 boulders and 11 to 15 the small detail stones, each
centred on its bounding box with its longest side one, drawn as bark (an albedo sheet and
a flat normal sheet). --sheet draws every stone of every pack side by side without the
game; --quick paints at one sample a texel (a fast look, never shipped).

WHAT THE GAME NEEDS FROM EACH MODEL (game/foliage3d_mod.h, THE ROCKS and ROCK PHYSICS):
  - y up, centred on the bounding box, longest side one: the size dial and the physics
    both measure that, and the lowest vertex says how far the centre stands above the
    ground before rock_sink buries a share of the size;
  - the extreme vertices along 26 directions are the body's contacts with the ground, so
    a convex low-poly stone is its own hull and tips and rolls by its true shape;
  - three colourings A, B, C, which the F5 shares weigh (rock_c1..c3).

THE STONES. Each is a convex block of large flat facets meeting in crisp ridges, as split
stone is: the hull of a few points scattered over a rounded box of the stone's own
proportions (few points, big facets), then a flat split or two across it (the weather's
and the frost's), by kind: a block, a chunk (more and smaller facets), a slab (low, its top
one tilted split), a wedge (one steep face cut deep), a shard (drawn in to a ridge), a
boulder split down a joint with its halves leaning a little apart, a slab with a smaller
block set into its top. Heights are 0.7 to 1.0 of the longest side, because the game
buries 0.41 of a rock's size (rock_sink) and what stands above the ground is the top of the
stone: a slab shows as a low flat slab, a chunk as a chunk. Every facet is its own chart,
planar mapped, so no texel stretches. Normals are flat per facet, smoothed only across a
fold shallower than 22 degrees. A convex stone is its own hull, so the physics' contacts
are its true corners.

THE PAINT is pass_ROCKS.py's stone, layer for layer, every pattern a function of the 3D
position (in cells, at the stone's reference size: the default rock size, 0.30 cells, for
a boulder, and 0.45 of that for a detail stone, the game's own ratio), at four sub-texel
samples: beds of close tones with a few open partings, flakes split by a fine joint
pattern each a shade of its own, mottling at two scales, a grain two texels of the set's
232 to the cell across, a few long cracks with a lit lip, lichen crusts on the faces, and
the theater's own dressing on the faces turned up (desert sand, temperate moss and earth,
winter snow), then the set's wash and a soft lift along every ridge. The painted light is
the form only, from straight above, because a stone is laid at any yaw: a face turned up
shows its tone, a side about three quarters of it; the game's sun does the rest, and no
cast shadow or large occlusion is painted.

THE PALETTES (lit tones chosen by eye, as the set's base colours are):
  DESERT from the approved desert rocks and the Enhanced desert ground's own pebbles: pale
    sandy ochre and warm tan, a near-cream face turned up, warm brown cracks, nothing grey.
  TEMPERAT from the Enhanced temperate cliffs: warm grey-beige stone, olive lichen, a
    little moss on the tops, dark earth at the foot.
  WINTER from the Enhanced winter cliffs: cool grey stone, a lavender cast in the cracks,
    snow lying on the faces turned up.
  The three colourings are three stones of the theater's own palette, in the slots the F5
  shares name: A the darker stone, B the pale lichened one, C the warm one.

The pack holds 45 sheets (the boulders at 256 texels, the detail stones at 128), which at
the default size carry 300 to 420 texels a cell, more than the set's 232 (asserted), so a
stone the size dial or the size variation enlarges stays sharp.

THE MARK AND THE EXPOSURE. Each pack carries one extra code, RKPAINT, which names no stone.
The game reads it to know these are the painted stones and draws them at the rock exposure
dial times ROCK_PAINTED_EXPOSURE (game/foliage3d_mod.h), because the dial's default was set
on the experimental pack's photographed albedo, about half as bright as this paint.

THE GAME READS rock3d_<THEATER>.pack from the play folder in place of rock3d.pack when the
map's theater has one (the snow theater takes WINTER), and rock3d.pack otherwise. Both
builds put these packs in their folder through tools/art/rocks/stones.sh, as the flag
ROCK_STONES_DEFAULT in game/make-build.sh says (painted, the default).

Deterministic: the same script writes the same packs, byte for byte.
"""
import argparse, math, os, struct, sys, time
import numpy as np
from PIL import Image
from scipy.optimize import linprog
from scipy.spatial import ConvexHull, HalfspaceIntersection

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'game'))
import bake_tree3d as bt  # noqa: E402

THEATERS = ('DESERT', 'TEMPERAT', 'WINTER')
DENS = 232.0                  # the set's texels a cell
REF_BOULDER = 0.30            # the default rock size (fx_state.h rock_size), cells
REF_DETAIL = 0.30 * 0.45      # a detail stone is drawn at 0.45 of it (rock_place_cell)
CREASE = 22.0                 # degrees: a fold shallower than this is shaded round


def rng_for(seed):
    return np.random.RandomState(seed)


# ======================================================================= the shapes
# A stone, in its own frame (x, y across, z up, before it is turned y up for the game):
# its kind, half sizes (x, y, z), and a seed. Kinds:
#   block   a box, its corners and edges split off
#   slab    a low box, split mostly round its sides, its top tilted
#   wedge   a box with one steep face cut deep through it
#   shard   a box drawn in toward a ridge at its top
#   chunk   many shallow splits: the most facets, a rounder stone, still faceted
#   split   a block cut in two down a joint, the halves leaning a little apart
#   stack   a slab with a smaller block resting on it, set into it
STONES = [
    # the boulders (1 to 10)
    dict(kind='block', h=(0.50, 0.42, 0.48), seed=201),
    dict(kind='slab',  h=(0.50, 0.42, 0.38), seed=202),
    dict(kind='split', h=(0.50, 0.42, 0.46), seed=203),
    dict(kind='wedge', h=(0.50, 0.38, 0.46), seed=204),
    dict(kind='shard', h=(0.46, 0.34, 0.50), seed=205),
    dict(kind='chunk', h=(0.50, 0.44, 0.46), seed=206),
    dict(kind='stack', h=(0.50, 0.42, 0.48), seed=207),
    dict(kind='block', h=(0.50, 0.32, 0.44), seed=208),
    dict(kind='slab',  h=(0.50, 0.46, 0.38), seed=209),
    dict(kind='split', h=(0.50, 0.40, 0.44), seed=210),
    # the small detail stones (11 to 15)
    dict(kind='slab',  h=(0.50, 0.38, 0.36), seed=211),   # a flat pebble
    dict(kind='chunk', h=(0.50, 0.42, 0.46), seed=212),
    dict(kind='wedge', h=(0.50, 0.40, 0.44), seed=213),
    dict(kind='shard', h=(0.50, 0.34, 0.46), seed=214),   # a chip
    dict(kind='block', h=(0.46, 0.40, 0.46), seed=215),
]


def unit(v):
    v = np.asarray(v, float)
    return v / max(np.linalg.norm(v), 1e-12)


def solve(planes):
    """The convex block inside the planes: its corner points (from the point deepest
    inside them, the Chebyshev centre)."""
    A = np.array([n for n, d in planes]); b = np.array([d for n, d in planes])
    norm = np.linalg.norm(A, axis=1)
    res = linprog(np.r_[0, 0, 0, -1.0], A_ub=np.c_[A, norm], b_ub=b,
                  bounds=[(None, None)] * 3 + [(0, None)], method='highs')
    hi = HalfspaceIntersection(np.c_[A, -b], res.x[:3])
    return hi.intersections


def support(pts, d):
    return float((pts @ d).max())


def piece_of(planes):
    pts = solve(planes)
    hull = ConvexHull(pts)
    return dict(planes=planes, pts=hull.points, hull=hull)


def blob_planes(h, r, npts, expo=2.0, jitter=0.12, taper=0.0, squash_top=0.0):
    """The planes of an irregular convex stone: the hull of npts points scattered over a
    rounded box (a superellipsoid of exponent expo) of half sizes h, each pushed in or out
    a little, the top drawn in by taper. Few points make big facets."""
    pts = []
    for _ in range(npts):
        d = unit(r.randn(3))
        d = d * (np.abs(d) ** expo).sum() ** (-1.0 / expo)
        d = d * (1.0 - jitter * r.rand())
        p = d * h
        t = (p[2] / h[2] + 1.0) / 2.0
        p[:2] *= 1.0 - taper * t
        if squash_top and p[2] > 0:
            p[2] *= 1.0 - squash_top
        pts.append(p)
    # the extreme points along the axes kept, so the proportions hold
    for ax in range(3):
        for sg in (1, -1):
            q = np.zeros(3); q[ax] = sg * h[ax] * (0.92 + 0.08 * r.rand())
            q[(ax + 1) % 3] = h[(ax + 1) % 3] * 0.35 * (r.rand() - 0.5)
            q[(ax + 2) % 3] = h[(ax + 2) % 3] * 0.35 * (r.rand() - 0.5)
            if ax < 2:
                t = (q[2] / h[2] + 1.0) / 2.0
                q[:2] *= 1.0 - taper * t
            pts.append(q)
    hull = ConvexHull(np.array(pts))
    P = []
    for eq in hull.equations:
        n = eq[:3]; d = -eq[3]
        if not any(n @ m > 0.9995 and abs(d - e) < 1e-4 for m, e in P):
            P.append((n, d))
    return P


def cut_dir(r, h, elev):
    az = 2 * math.pi * r.rand()
    el = math.asin(elev[0] + (elev[1] - elev[0]) * r.rand())
    return unit(np.array([math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el)]))


def splits(P, r, h, k, depth, elev=(-0.3, 0.95)):
    """k flat splits, each a seeded share in from the stone's reach that way."""
    P = list(P)
    for _ in range(k):
        d = cut_dir(r, h, elev)
        reach = support(solve(P), d)
        dp = depth[0] + (depth[1] - depth[0]) * r.rand()
        P.append((d, reach * (1.0 - dp)))
    return P


def make_stone(spec):
    """The stone's pieces: each a convex block (its planes and hull)."""
    r = rng_for(spec['seed'])
    h = np.array(spec['h'], float)
    kind = spec['kind']
    pieces = []
    if kind == 'block':        # a blocky stone: few points on a rounded box, two splits
        P = blob_planes(h, r, 9, 3.0, 0.16, 0.18)
        P = splits(P, r, h, 2, (0.12, 0.30))
    elif kind == 'chunk':      # more points on a round body: more, smaller facets
        P = blob_planes(h, r, 16, 2.0, 0.10, 0.22)
        P = splits(P, r, h, 2, (0.06, 0.16))
    elif kind == 'slab':       # low and wide, the top a flat split, tilted
        P = blob_planes(h, r, 10, 3.5, 0.12, 0.10)
        n = unit([0.5 * (r.rand() - 0.5), 0.5 * (r.rand() - 0.5), 1.0])
        P.append((n, support(solve(P), n) * 0.82))
        P = splits(P, r, h, 2, (0.10, 0.25), elev=(-0.2, 0.4))
    elif kind == 'wedge':      # one steep face cut deep through it
        P = blob_planes(h, r, 10, 2.5, 0.14, 0.15)
        az = 2 * math.pi * r.rand()
        n = unit([math.cos(az), math.sin(az), 0.85])
        P.append((n, support(solve(P), n) * 0.50))
        P = splits(P, r, h, 1, (0.10, 0.25))
    elif kind == 'shard':      # drawn in to a ridge at its top
        P = blob_planes(h, r, 9, 2.5, 0.14, 0.40)
        az = 2 * math.pi * r.rand()
        for sgn in (1.0, -1.0):
            n = unit([sgn * math.cos(az), sgn * math.sin(az), 1.0 + 0.4 * r.rand()])
            P.append((n, support(solve(P), n) * (0.70 + 0.08 * r.rand())))
    elif kind == 'split':      # one boulder split down a joint, the halves apart
        P = blob_planes(h, r, 13, 2.4, 0.12, 0.20)
        P = splits(P, r, h, 2, (0.08, 0.22))
        az = 2 * math.pi * r.rand()
        jn = unit([math.cos(az), math.sin(az), 0.30 * (r.rand() - 0.5)])
        off = 0.12 * h[0] * (r.rand() - 0.5)
        gap = 0.06 * h[0] * (1.0 + 0.5 * r.rand())
        lean = math.radians(5.0 + 5.0 * r.rand())
        for sgn in (1.0, -1.0):
            Q = list(P) + [(-sgn * jn, -sgn * off)]
            pts = solve(Q)
            ax = unit(np.cross(jn, [0, 0, 1]))
            R = rot(ax, sgn * lean)
            c = pts.mean(0)
            pivot = np.array([c[0], c[1], pts[:, 2].min()])
            Q2 = []
            for n, d in Q:
                n2 = R @ n
                p2 = R @ (n * d - pivot) + pivot + sgn * jn * gap * 0.5
                Q2.append((n2, float(n2 @ p2)))
            pieces.append(piece_of(Q2))
        return pieces
    elif kind == 'stack':      # a slab with a smaller block resting on it
        lo = np.array([h[0], h[1], h[2] * 0.50])
        P = blob_planes(lo, r, 10, 3.5, 0.12, 0.08)
        P = shift_planes(P, np.array([0, 0, -h[2] * 0.45]))
        pieces.append(piece_of(P))
        up = np.array([h[0] * 0.55, h[1] * 0.60, h[2] * 0.52])
        Q = blob_planes(up, r, 14, 4.0, 0.12, 0.20)
        Q = rotz_planes(Q, math.radians(25 + 30 * r.rand()))
        # set on the slab: its middle a little above the slab's top, its foot a flat bed
        # cut just below that top, so it rests broad on the slab and never balances on a
        # point or floats
        top = support(solve(P), np.array([0.0, 0.0, 1.0]))
        Q = shift_planes(Q, np.array([h[0] * 0.25 * (r.rand() - 0.4), h[1] * 0.20 * (r.rand() - 0.5),
                                      top + 0.30 * up[2]]))
        Q.append((np.array([0.0, 0.0, -1.0]), -(top - 0.08 * h[2])))
        pieces.append(piece_of(Q))
        return pieces
    else:
        raise ValueError(kind)
    # a little turn about a level axis, so no stone sits square
    R = rot(unit([r.rand() - 0.5, r.rand() - 0.5, 0.0]), math.radians(6 * (r.rand() - 0.5)))
    P = [(R @ n, d) for n, d in P]
    pieces.append(piece_of(P))
    return pieces


def rot(ax, a):
    ax = unit(ax); x, y, z = ax
    c, s = math.cos(a), math.sin(a); C = 1 - c
    return np.array([[c + x * x * C, x * y * C - z * s, x * z * C + y * s],
                     [y * x * C + z * s, c + y * y * C, y * z * C - x * s],
                     [z * x * C - y * s, z * y * C + x * s, c + z * z * C]])


def shift_planes(P, t):
    return [(n, d + float(n @ t)) for n, d in P]


def rotz_planes(P, a):
    R = rot([0, 0, 1], a)
    return [(R @ n, d) for n, d in P]


def inside(planes, p, eps=1e-4):
    ok = np.ones(len(p), bool)
    for n, d in planes:
        ok &= p @ n < d - eps
    return ok


# ======================================================================= the mesh
def build_mesh(spec):
    """Facet charts of the stone in the game's frame (y up), centred on the bounding box,
    longest side one. Returns the charts: each its triangles (3D), its facet normal, the
    piece it belongs to, and the facet's neighbours across each edge."""
    pieces = make_stone(spec)
    tris, tn, tpiece, tplane = [], [], [], []
    for pi, pc in enumerate(pieces):
        hull = pc['hull']
        P = hull.points
        for s, eq in zip(hull.simplices, hull.equations):
            t = P[s]
            n = eq[:3]
            if np.cross(t[1] - t[0], t[2] - t[0]) @ n < 0:
                t = t[[0, 2, 1]]
            # a triangle wholly inside another piece is never seen: left out
            hidden = False
            for pj, other in enumerate(pieces):
                if pj != pi and inside(other['planes'], np.vstack([t, t.mean(0)[None]])).all():
                    hidden = True
            if hidden:
                continue
            tris.append(t); tn.append(n); tpiece.append(pi)
            tplane.append((pi, tuple(np.round(eq, 4))))
    tris = np.array(tris)
    # z up to y up: (x, y, z) -> (x, z, -y), a right-handed turn, so the winding holds
    def to_game(a):
        return np.stack([a[..., 0], a[..., 2], -a[..., 1]], -1)
    tris = to_game(tris)
    tn = to_game(np.array(tn))
    lo, hi = tris.reshape(-1, 3).min(0), tris.reshape(-1, 3).max(0)
    tris = tris - 0.5 * (lo + hi)
    ext = float((hi - lo).max())
    tris = tris / ext
    # the charts: the triangles of one facet (one plane of one piece)
    keys = {}
    chart_of = []
    for i, k in enumerate(tplane):
        if k not in keys:
            keys[k] = len(keys)
        chart_of.append(keys[k])
    charts = []
    for c in range(len(keys)):
        idx = [i for i in range(len(tris)) if chart_of[i] == c]
        n = unit(tn[idx].sum(0))
        charts.append(dict(tris=tris[idx], n=n, piece=tpiece[idx[0]]))
    return charts, len(pieces)


def chart_normals(charts):
    """Per chart, a normal at each triangle corner: the facet's own, averaged with the
    neighbouring facets of the same piece meeting at that corner across a fold shallower
    than CREASE (area weighted)."""
    cosc = math.cos(math.radians(CREASE))
    at = {}
    for ci, c in enumerate(charts):
        area = sum(0.5 * np.linalg.norm(np.cross(t[1] - t[0], t[2] - t[0])) for t in c['tris'])
        for t in c['tris']:
            for v in t:
                at.setdefault((c['piece'], tuple(np.round(v, 5))), set()).add((ci, area))
    for ci, c in enumerate(charts):
        cn = []
        for t in c['tris']:
            ns = []
            for v in t:
                s = np.zeros(3)
                for cj, a in at[(c['piece'], tuple(np.round(v, 5)))]:
                    nj = charts[cj]['n']
                    if nj @ c['n'] >= cosc:
                        s += a * nj
                ns.append(unit(s))
            cn.append(ns)
        c['cn'] = np.array(cn)


def chart_edges(charts):
    """The ridges: every edge where two facets of one piece meet, with both normals."""
    owner = {}
    for ci, c in enumerate(charts):
        for t in c['tris']:
            for a in range(3):
                p, q = tuple(np.round(t[a], 5)), tuple(np.round(t[(a + 1) % 3], 5))
                owner.setdefault((c['piece'],) + tuple(sorted((p, q))), []).append(ci)
    E = []
    for key, cs in owner.items():
        cs = sorted(set(cs))
        if len(cs) != 2:
            continue
        a, b = charts[cs[0]]['n'], charts[cs[1]]['n']
        ang = math.degrees(math.acos(max(-1.0, min(1.0, float(a @ b)))))
        if ang < 18.0:
            continue
        E.append((np.array(key[1]), np.array(key[2]), cs[0], cs[1], unit(a + b)))
    return E


# ======================================================================= the atlas
def layout(charts, sheet, pad):
    """Every chart planar mapped on its own facet, turned to its smallest rectangle, and
    packed on shelves at the greatest scale that fits the sheet. Sets each chart's uv
    (texels) and returns the texels per model unit."""
    rects = []
    for c in charts:
        n = c['n']
        a = unit(np.cross(n, [0, 1, 0]) if abs(n[1]) < 0.9 else np.cross(n, [1, 0, 0]))
        b = np.cross(n, a)
        p2 = np.stack([c['tris'] @ a, c['tris'] @ b], -1)          # (t, 3, 2)
        flat = p2.reshape(-1, 2)
        best = None
        for deg in range(0, 90, 3):
            th = math.radians(deg)
            R = np.array([[math.cos(th), -math.sin(th)], [math.sin(th), math.cos(th)]])
            q = flat @ R.T
            ext = q.max(0) - q.min(0)
            if best is None or ext[0] * ext[1] < best[0]:
                best = (ext[0] * ext[1], R, q.min(0), ext)
        _, R, mn, ext = best
        c['_p2'] = (p2.reshape(-1, 2) @ R.T - mn).reshape(p2.shape)
        if ext[0] < ext[1]:                                         # long side across
            c['_p2'] = c['_p2'][..., ::-1]
            ext = ext[::-1]
        c['_ext'] = ext
        rects.append(ext)

    def pack(scale):
        order = sorted(range(len(charts)), key=lambda i: -rects[i][1])
        x = y = pad; shelf = 0.0; pos = {}
        for i in order:
            w, h = rects[i] * scale
            if x + w + pad > sheet:
                x = pad; y += shelf + pad; shelf = 0.0
            if w + 2 * pad > sheet:
                return None
            pos[i] = (x, y)
            x += w + pad; shelf = max(shelf, h)
        if y + shelf + pad > sheet:
            return None
        return pos
    lo, hi = 1.0, 4096.0
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        if pack(mid) is None:
            hi = mid
        else:
            lo = mid
    pos = pack(lo)
    for i, c in enumerate(charts):
        c['uv'] = c['_p2'] * lo + np.array(pos[i])
    return lo


# ======================================================================= the paint
# the noise and the joints, as the family's pass has them (pass_ROCKS.py, unchanged
# where the name is the same)
def smoothstep(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def vnoise(p, cell, seed):
    q = p / cell
    i0 = np.floor(q).astype(np.int64); fq = q - i0
    def h(ix, iy):
        x = np.sin(ix * 127.1 + iy * 311.7 + seed * 74.7) * 43758.5453
        return x - np.floor(x)
    sx, sy = smoothstep(fq[:, 0]), smoothstep(fq[:, 1])
    a, b = h(i0[:, 0], i0[:, 1]), h(i0[:, 0] + 1, i0[:, 1])
    c, d = h(i0[:, 0], i0[:, 1] + 1), h(i0[:, 0] + 1, i0[:, 1] + 1)
    return (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy


def hsh(*a):
    x = np.sin(sum(v * k for v, k in zip(a, (12.9898, 78.233, 37.719, 4.581)))) * 43758.5453
    return x - np.floor(x)


def h3(ix, iy, iz, seed):
    x = np.sin(ix * 127.1 + iy * 311.7 + iz * 74.7 + seed * 19.19) * 43758.5453
    return x - np.floor(x)


def vn3(p, cell, seed):
    q = p / cell
    i0 = np.floor(q); f = q - i0
    s = f * f * (3 - 2 * f)
    out = np.zeros(len(p))
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (s[:, 0] if dx else 1 - s[:, 0]) * (s[:, 1] if dy else 1 - s[:, 1]) * (s[:, 2] if dz else 1 - s[:, 2])
                out += w * h3(i0[:, 0] + dx, i0[:, 1] + dy, i0[:, 2] + dz, seed)
    return out


def fbm3(p, cell, seed, octaves=3):
    v, a, t = np.zeros(len(p)), 1.0, 0.0
    for o in range(octaves):
        v += a * vn3(p, cell / (2 ** o), seed + o * 3.1)
        t += a; a *= 0.5
    return v / t


def cracks3(p, cell, seed, jit=0.85):
    q = p / cell
    i0 = np.floor(q)
    n = len(p)
    d1 = np.full(n, 1e9); d2 = np.full(n, 1e9)
    s1 = np.zeros((n, 3)); s2 = np.zeros((n, 3)); k1 = np.zeros(n); k2 = np.zeros(n)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                c = i0 + np.array([dx, dy, dz], float)
                o = 0.5 + jit * (np.c_[h3(c[:, 0], c[:, 1], c[:, 2], seed), h3(c[:, 0], c[:, 1], c[:, 2], seed + 1.3),
                                       h3(c[:, 0], c[:, 1], c[:, 2], seed + 2.9)] - 0.5)
                s = c + o
                d = np.sum((q - s) ** 2, axis=1)
                kk = h3(c[:, 0], c[:, 1], c[:, 2], seed + 7.7)
                a = d < d1
                b = ~a & (d < d2)
                d2 = np.where(a, d1, np.where(b, d, d2))
                s2 = np.where(a[:, None], s1, np.where(b[:, None], s, s2))
                k2 = np.where(a, k1, np.where(b, kk, k2))
                d1 = np.where(a, d, d1)
                s1 = np.where(a[:, None], s, s1)
                k1 = np.where(a, kk, k1)
    span = np.linalg.norm(s2 - s1, axis=1)
    wall = (d2 - d1) / (2 * np.maximum(span, 1e-9)) * cell
    wn = (s2 - s1) / np.maximum(span, 1e-9)[:, None]
    return wall, np.round((k1 + k2) * 1000) % 997 / 997.0, wn


def oblique(n, wn):
    return smoothstep((0.90 - np.abs((n * wn).sum(1))) / 0.20)


# the painted light: the form only, from straight above (a stone lies at any yaw), a
# face turned up shows its tone and an upright one about seven tenths of it
AMB, DIF, FORM = 0.50, 0.62, 0.40
K_LEVEL = AMB + DIF * 1.0 + 0.06


def light(n):
    lam = np.clip(n[:, 2], 0, 1)
    lam = 1.0 + FORM * (lam - 1.0)
    return (AMB + DIF * lam + 0.06 * np.clip(n[:, 2], 0, 1)) / K_LEVEL


def C(*rgb):
    return np.array(rgb, float)


# The theaters' palettes, per colouring: the bed tones (and how often each), the crack,
# varnish, lichen and the dressing on the faces turned up.
#   beds     [(tone, share), ...]
#   crack    what a crack multiplies toward
#   lichen   (colour, amount); lichen2 a second, rarer crust colour
#   top      (colour, amount, kind): what lies on the faces turned up
#   foot     (colour, amount): the band at the stone's foot
#   varnish  dark streaks down the steep faces (amount)
PAL = {
    'DESERT': {
        # A: warm tan sandstone, the darker stone
        'A': dict(beds=[(C(214, 150, 100), 0.45), (C(202, 140, 92), 0.35), (C(222, 160, 108), 0.15),
                        (C(190, 128, 84), 0.05)],
                  crack=C(116, 72, 48), lichen=(C(176, 160, 120), 0.12), lichen2=C(200, 124, 66),
                  top=(C(230, 180, 124), 0.55, 'sand'), foot=(C(176, 120, 82), 0.55), varnish=0.30),
        # B: the pale lichened sandstone
        'B': dict(beds=[(C(232, 190, 140), 0.50), (C(226, 182, 130), 0.35), (C(218, 174, 124), 0.15)],
                  crack=C(130, 88, 62), lichen=(C(180, 168, 128), 0.60), lichen2=C(204, 136, 76),
                  top=(C(240, 200, 150), 0.50, 'sand'), foot=(C(186, 136, 96), 0.50), varnish=0.15),
        # C: pale sandy ochre, the warm stone (weighed heaviest by default)
        'C': dict(beds=[(C(230, 166, 104), 0.45), (C(222, 156, 94), 0.35), (C(236, 178, 118), 0.15),
                        (C(212, 146, 88), 0.05)],
                  crack=C(122, 76, 50), lichen=(C(184, 164, 120), 0.10), lichen2=C(204, 124, 64),
                  top=(C(240, 192, 132), 0.60, 'sand'), foot=(C(180, 122, 82), 0.55), varnish=0.25),
    },
    'TEMPERAT': {
        # A: cool grey-beige stone, the darker one
        'A': dict(beds=[(C(110, 106, 97), 0.45), (C(103, 99, 92), 0.35), (C(117, 111, 101), 0.15),
                        (C(96, 92, 85), 0.05)],
                  crack=C(57, 54, 49), lichen=(C(104, 110, 84), 0.55), lichen2=C(122, 111, 68),
                  top=(C(84, 94, 56), 0.35, 'moss'), foot=(C(73, 63, 49), 0.55), varnish=0.20),
        # B: pale grey stone, much lichen
        'B': dict(beds=[(C(139, 135, 124), 0.50), (C(132, 128, 117), 0.35), (C(125, 122, 113), 0.15)],
                  crack=C(78, 74, 68), lichen=(C(120, 125, 97), 1.00), lichen2=C(137, 124, 78),
                  top=(C(103, 110, 77), 0.30, 'moss'), foot=(C(84, 73, 57), 0.50), varnish=0.10),
        # C: warm grey-beige, as the temperate cliffs
        'C': dict(beds=[(C(132, 118, 97), 0.45), (C(125, 111, 92), 0.35), (C(139, 125, 104), 0.15),
                        (C(118, 103, 84), 0.05)],
                  crack=C(68, 57, 45), lichen=(C(111, 113, 85), 0.45), lichen2=C(130, 104, 63),
                  top=(C(87, 94, 57), 0.30, 'moss'), foot=(C(78, 66, 50), 0.55), varnish=0.20),
    },
    'WINTER': {
        # A: cool blue-grey stone, the darker one
        'A': dict(beds=[(C(104, 104, 110), 0.45), (C(97, 97, 103), 0.35), (C(111, 110, 113), 0.15),
                        (C(90, 90, 97), 0.05)],
                  crack=C(56, 52, 63), lichen=(C(106, 110, 94), 0.35), lichen2=C(122, 108, 80),
                  top=(C(226, 230, 238), 0.45, 'snow'), foot=(C(78, 75, 71), 0.45), varnish=0.15),
        # B: pale grey stone, lichened, the most snow
        'B': dict(beds=[(C(134, 132, 132), 0.50), (C(127, 125, 127), 0.35), (C(120, 118, 120), 0.15)],
                  crack=C(75, 70, 80), lichen=(C(117, 122, 104), 0.70), lichen2=C(132, 118, 87),
                  top=(C(232, 236, 244), 0.60, 'snow'), foot=(C(89, 84, 80), 0.40), varnish=0.08),
        # C: warm grey-beige, as the winter cliffs
        'C': dict(beds=[(C(125, 117, 104), 0.45), (C(118, 111, 101), 0.35), (C(132, 124, 111), 0.15),
                        (C(111, 103, 94), 0.05)],
                  crack=C(66, 57, 64), lichen=(C(111, 113, 92), 0.40), lichen2=C(127, 108, 75),
                  top=(C(228, 232, 240), 0.45, 'snow'), foot=(C(84, 75, 68), 0.45), varnish=0.15),
    },
}
UMBER = np.array([0.82, 0.72, 0.58])         # the set's wash colour (build_ROCKS.py)
WASH = 0.28                                  # lighter than ROCK1-7's 0.38: a loose stone is cleaner


def patterns(w, n, seed, tx, bed_n, top):
    """Every layer's mask at the points w (cells) with normals n: computed once a stone and
    shared by every theater and colouring, which only change the colours."""
    L = {}
    warp = (fbm3(w, 0.30, seed + 1.0) - 0.5) * 0.07 + (vn3(w, 0.07, seed + 2.0) - 0.5) * 0.014
    s = w @ bed_n + warp
    th = 0.050
    t = s / th + 0.35 * np.sin(2 * np.pi * s / (th * 2.7) + seed) + 0.25 * np.sin(2 * np.pi * s / (th * 7.3))
    i = np.floor(t); fr = t - i
    L['bed_i'] = i
    L['bed_k'] = smoothstep((fr - 0.94) / 0.06)

    def line(d, hw):
        return np.clip((hw - d) / tx + 0.5, 0, 1)
    d = (1.0 - fr) * th
    seg = smoothstep((vn3(w, 0.22, seed + 21.0) - 0.52) / 0.08)
    groove = line(d, 1.0 * tx) * seg * (hsh(i, seed, 8.0) > 0.50)
    lip = line(np.abs(fr * th - 2.2 * tx), 0.8 * tx) * seg * (hsh(i - 1, seed, 8.0) > 0.50)
    ob = oblique(n, np.tile(bed_n, (len(n), 1)))
    L['groove'], L['lip'] = groove * ob, lip * ob
    wall2, k2, wn2 = cracks3(w, 0.13, seed + 12.0)
    L['flake'] = k2
    L['mot'] = 1.0 + 0.08 * (fbm3(w, 0.45, seed + 3.0) - 0.5) + 0.05 * (vn3(w, 0.11, seed + 4.0) - 0.5)
    L['warm'] = smoothstep((fbm3(w, 0.26, seed + 5.0) - 0.60) / 0.12) * 0.25
    L['grain'] = vn3(w, 2.2 * tx, seed + 6.0)
    steep = smoothstep((0.55 - n[:, 2]) / 0.35)
    up = smoothstep((n[:, 2] - 0.55) / 0.30)
    L['up'] = up
    ht = np.c_[-n[:, 1], n[:, 0]]
    hl = np.linalg.norm(ht, axis=1, keepdims=True)
    ht = np.where(hl > 1e-6, ht / np.maximum(hl, 1e-9), np.array([[1.0, 0.0]]))
    along = (w[:, :2] * ht).sum(1)
    col_n = vnoise(np.c_[along / 0.016, w[:, 2] / 0.30], 1.0, seed + 7.0)
    place = smoothstep((fbm3(w, 0.35, seed + 8.0) - 0.54) / 0.12)
    reach = smoothstep((w[:, 2] - (top - 0.25 * top)) / max(top * 0.6, 0.02))
    L['varnish'] = smoothstep((col_n - 0.55) / 0.25) * place * steep * (0.35 + 0.65 * reach)
    # lichen: small crusts in loose clusters, on the side faces and the tops alike (a
    # stone lies at any yaw, so no face is turned from the sun for good)
    side = smoothstep((n[:, 2] + 0.3) / 0.4)
    clus = smoothstep((fbm3(w, 0.16, seed + 9.0) - 0.58) / 0.10)
    sp = vn3(w, 0.018, seed + 10.0) + 0.25 * (vn3(w, 0.006, seed + 10.5) - 0.5)
    L['crust'] = smoothstep((sp - 0.65) / 0.02) * clus * side
    L['orange'] = (hsh(np.floor(w[:, 0] / 0.05), np.floor(w[:, 1] / 0.05), np.floor(w[:, 2] / 0.05)) > 0.85)
    wall1, k1, wn1 = cracks3(w, 0.40, seed + 11.0)
    on1 = (k1 < 0.45) & (vn3(w, 0.35, seed + 13.0) > 0.42)
    on2 = (k2 < 0.32) & (vn3(w, 0.22, seed + 14.0) > 0.50)
    o1, o2 = oblique(n, wn1), oblique(n, wn2)
    c1 = line(wall1, 1.3 * tx) * on1 * o1
    c2 = line(wall2, 0.8 * tx) * on2 * 0.85 * o2
    crack = np.maximum(c1, c2)
    # the lit lip: the crack seen from a little upward along the face
    lt = np.array([0.0, 0.0, 1.0]) - n * n[:, 2:3]
    lt /= np.maximum(np.linalg.norm(lt, axis=1, keepdims=True), 1e-9)
    w1 = w - lt * (2.4 * tx)
    l1, _, _ = cracks3(w1, 0.40, seed + 11.0)
    l2, _, _ = cracks3(w1, 0.13, seed + 12.0)
    lipc = np.maximum(line(l1, 1.1 * tx) * on1 * o1, line(l2, 0.8 * tx) * on2 * 0.85 * o2)
    L['crack'], L['lipc'] = crack, lipc * (1 - crack)
    L['dmask'] = 0.45 + 0.55 * fbm3(w, 0.10, seed + 15.0)
    # the dressing on the faces turned up comes in patches: a few large ones and a fine edge
    L['patch'] = fbm3(w, 0.09, seed + 17.0) + 0.25 * (vn3(w, 0.02, seed + 18.0) - 0.5)
    L['field'] = 0.55 * vnoise(w[:, :2] + w[:, 2:3] * 0.7, 0.35, 95) + 0.45 * vnoise(w[:, :2] * 1.0 + 3.1, 0.12, 96)
    L['light'] = light(n)
    return L


def compose(L, pal, seed, zfoot):
    """One theater's colouring over the masks."""
    i = L['bed_i']
    beds = pal['beds']
    cum = np.cumsum([b[1] for b in beds]); cum = cum / cum[-1]

    def tone(k):
        h = hsh(k, seed, 5.0)
        out = np.zeros((len(k), 3))
        lo = 0.0
        for (c, _), hi in zip(beds, cum):
            m = (h >= lo) & (h < hi + (1e-9 if hi >= 1 else 0))
            out[m] = c
            lo = hi
        return out * (0.98 + 0.04 * hsh(k, seed, 6.0))[:, None]
    c0, c1 = tone(i), tone(i + 1)
    col = c0 + (c1 - c0) * L['bed_k'][:, None]
    col = col * (0.975 + 0.05 * L['flake'])[:, None]
    col = col * L['mot'][:, None]
    warmc = beds[min(2, len(beds) - 1)][0] * np.array([1.0, 0.94, 0.86])
    col = col + (warmc - col) * L['warm'][:, None]
    col = col * (0.955 + 0.09 * L['grain'])[:, None]
    var = L['varnish'] * pal['varnish']
    vcol = pal['crack'] / 255.0 * 0.85
    col = col * (1 - var[:, None] * (1 - vcol[None]))
    lc = np.where(L['orange'][:, None], pal['lichen2'][None], pal['lichen'][0][None])
    col = col + (lc - col) * (L['crust'] * 0.45 * pal['lichen'][1])[:, None]
    # the dressing on the faces turned up, and the foot
    tc, ta, kind = pal['top']
    if kind == 'snow':
        cover = smoothstep((L['patch'] - (0.62 - 0.30 * L['up'])) / 0.05) * L['up'] * ta
    elif kind == 'moss':
        cover = smoothstep((L['patch'] - 0.52) / 0.10) * L['up'] * ta
    else:
        cover = L['up'] * L['dmask'] * ta
    cover = cover * (0.8 + 0.2 * L['grain'])
    col = col + (tc - col) * cover[:, None]
    fc, fa = pal['foot']
    foot = smoothstep(((0.04 + 0.04 * L['dmask']) - zfoot) / 0.04) * fa * (0.6 + 0.4 * L['dmask'])
    col = col + (fc - col) * foot[:, None]
    dust = np.maximum(cover, foot)
    k = np.clip(np.maximum(L['crack'] * 0.70, L['groove'] * 0.30) * (1 - 0.5 * dust), 0, 1)
    col = col * (1 - k[:, None] * (1 - pal['crack'][None] / 255.0))
    litl = np.clip(L['lipc'] * 0.16 + L['lip'] * 0.10, 0, 1)
    col = col * (1 + litl[:, None])
    # the painted form, then the set's wash (stronger low)
    col = col * L['light'][:, None]
    kw = WASH * (0.45 + 0.9 * L['field'])
    kw = np.clip(kw * (1.0 + 0.7 * np.clip(1 - zfoot / 0.10, 0, 1) ** 1.5), 0, 1)[:, None]
    col = col * (1 - kw + kw * UMBER[None])
    return col


# ======================================================================= one stone
def raster_charts(charts, sheet, sub):
    """For every texel the charts cover, at each sub-texel offset: its chart, its 3D point
    and its normal. The margins are bled three texels out from their neighbours."""
    fid = np.full((sheet, sheet), -1, np.int64)
    tri_of = []
    for ci, c in enumerate(charts):
        for ti, (t, uv) in enumerate(zip(c['tris'], c['uv'])):
            tri_of.append((ci, ti))
            k = len(tri_of) - 1
            u0, v0 = np.floor(uv.min(0)).astype(int) - 1
            u1, v1 = np.ceil(uv.max(0)).astype(int) + 1
            u0, v0 = max(u0, 0), max(v0, 0); u1, v1 = min(u1, sheet), min(v1, sheet)
            U, V = np.meshgrid(np.arange(u0, u1) + 0.5, np.arange(v0, v1) + 0.5)
            lam = bary(uv, U.ravel(), V.ravel())
            ok = (lam >= -1e-6).all(1)
            vv, uu = np.nonzero(ok.reshape(U.shape))
            fid[vv + v0, uu + u0] = k
    inner = fid >= 0
    for _ in range(3):
        empty = fid < 0
        for dv, du in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            src = np.roll(np.roll(fid, dv, 0), du, 1)
            fill = empty & (src >= 0)
            fid[fill] = src[fill]
            empty = fid < 0
    vv, uu = np.nonzero(fid >= 0)
    ks = fid[vv, uu]
    W, Nn = [], []
    for ox, oy in sub:
        w = np.zeros((len(vv), 3)); nn = np.zeros((len(vv), 3))
        for k in np.unique(ks):
            m = ks == k
            ci, ti = tri_of[k]
            c = charts[ci]
            lam = bary(c['uv'][ti], uu[m] + ox, vv[m] + oy)       # extrapolated in margins
            w[m] = lam @ c['tris'][ti]
            n = lam @ c['cn'][ti]
            nn[m] = n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)
        W.append(w); Nn.append(nn)
    return vv, uu, ks, tri_of, W, Nn, inner


def bary(uv, U, V):
    (ax, ay), (bx, by), (cx, cy) = uv
    den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(den) < 1e-12:
        return np.zeros((len(U), 3)) - 1
    w0 = ((by - cy) * (U - cx) + (cx - bx) * (V - cy)) / den
    w1 = ((cy - ay) * (U - cx) + (ax - cx) * (V - cy)) / den
    return np.c_[w0, w1, 1 - w0 - w1]


def ridge_lift(W, Nn, ks, tri_of, charts, edges, ref, tx):
    """The set's soft lift along every ridge (crisp_edges on stone: a wide soft lift and no
    line), keyed to the light from above: a ridge across the top lifts most."""
    out = np.zeros(len(W))
    if not edges:
        return out
    chart_k = np.array([tri_of[k][0] for k in ks])
    width = 2.4 * tx / ref                       # in model units
    for a, b, c0, c1, nb in edges:
        k_lit = float(np.clip((nb[1] - 0.05) / 0.75, 0.0, 1.0))      # y is up here
        if k_lit <= 0:
            continue
        # convex: the two facets' normals point apart across the edge
        m = (chart_k == c0) | (chart_k == c1)
        if not m.any():
            continue
        p = W[m]
        d_ = b - a; L_ = np.linalg.norm(d_); d_ = d_ / max(L_, 1e-9)
        t = np.clip((p - a) @ d_, 0, L_)
        dist = np.linalg.norm(p - (a + t[:, None] * d_[None]), axis=1)
        out[m] = np.maximum(out[m], np.clip(1 - dist / width, 0, 1) ** 2.2 * k_lit)
    return out


def paint_stone(idx, spec, sheet, quick=False):
    """The stone's mesh, its atlas and its painted sheets for every theater and colouring."""
    detail = idx >= 11
    ref = REF_DETAIL if detail else REF_BOULDER
    charts, npieces = build_mesh(spec)
    chart_normals(charts)
    edges = chart_edges(charts)
    tpu = layout(charts, sheet, pad=3)
    sub = [(0.5, 0.5)] if quick else [(0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75)]
    vv, uu, ks, tri_of, W, Nn, inner = raster_charts(charts, sheet, sub)
    tx = 1.0 / DENS
    allp = np.concatenate([c['tris'].reshape(-1, 3) for c in charts])
    ylo, yhi = allp[:, 1].min(), allp[:, 1].max()
    # the bedding: dipping a little, its own way per stone
    r = rng_for(spec['seed'] + 7)
    bed_n = unit([0.30 * (r.rand() - 0.5), 0.30 * (r.rand() - 0.5), 1.0])
    out = {th: {c: np.zeros((len(vv), 3)) for c in 'ABC'} for th in THEATERS}
    for w_m, n_m in zip(W, Nn):
        # model (x, y up, z) to the family's frame (x, y, z up), in cells at the
        # stone's reference size
        w = np.c_[w_m[:, 0], -w_m[:, 2], w_m[:, 1]] * ref
        n = np.c_[n_m[:, 0], -n_m[:, 2], n_m[:, 1]]
        L = patterns(w, n, float(spec['seed']), tx, bed_n, (yhi - ylo) * ref)
        zfoot = (w_m[:, 1] - ylo) * ref
        lift = ridge_lift(w_m, n_m, ks, tri_of, charts, edges, ref, tx)
        for th in THEATERS:
            for c in 'ABC':
                col = compose(L, PAL[th][c], float(spec['seed']), zfoot)
                lum = (col @ np.array([0.299, 0.587, 0.114])) / 255.0
                lifted = col * np.minimum(1 + 0.75 * (1 - lum) + 0.12,
                                          254.0 / np.maximum(col.max(1), 1.0))[:, None]
                col = col + (lifted - col) * (lift * 0.45)[:, None]
                out[th][c] += col / len(W)
    sheets = {}
    for th in THEATERS:
        for c in 'ABC':
            img = np.zeros((sheet, sheet, 4), np.uint8)
            img[..., :3] = np.array([128, 110, 90], np.uint8)
            img[vv, uu, :3] = np.clip(out[th][c] + 0.5, 0, 255).astype(np.uint8)
            img[..., 3] = 255
            sheets[(th, c)] = img
    # the mesh, flat per chart: each chart's own vertices
    P, N, U, T = [], [], [], []
    for c in charts:
        for t, uv, cn in zip(c['tris'], c['uv'], c['cn']):
            base = len(P)
            for j in range(3):
                P.append(t[j]); N.append(cn[j])
                U.append((uv[j][0] / sheet, uv[j][1] / sheet))
            T.append((base, base + 1, base + 2))
    P = np.array(P); N = np.array(N); U = np.array(U); T = np.array(T)
    # welded where a chart's own triangles share a corner
    key = np.c_[np.round(P, 6), np.round(U, 6), np.round(N, 5)]
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    order = np.argsort(first)
    remap = np.empty(len(order), np.int64); remap[order] = np.arange(len(order))
    sel = first[order]
    P, N, U = P[sel], N[sel], U[sel]
    T = remap[inv.ravel()][T]
    ext = P.max(0) - P.min(0)
    info = dict(pieces=npieces, charts=len(charts), tris=len(T), verts=len(P), ext=ext,
                tpu=tpu, cells_dens=tpu / ref)
    return dict(P=P, N=N, U=U, T=T), sheets, info


# ======================================================================= the pack
def write_packs(out_dir, theaters, quick=False, sheet_png=None):
    os.makedirs(out_dir, exist_ok=True)
    meshes, sheets_all, infos = [], [], []
    t0 = time.time()
    for i, spec in enumerate(STONES):
        idx = i + 1
        px = 128 if idx >= 11 else 256
        mesh, sheets, info = paint_stone(idx, spec, px, quick)
        meshes.append(mesh); sheets_all.append(sheets); infos.append(info)
        print('RK%02d %-5s %d piece(s) %2d facets %3d tris  size %.2f x %.2f x %.2f (y up)  '
              '%.0f texels a unit = %.0f a cell at its reference size' %
              (idx, spec['kind'], info['pieces'], info['charts'], info['tris'],
               info['ext'][0], info['ext'][1], info['ext'][2], info['tpu'], info['cells_dens']))
        assert abs(info['ext'].max() - 1.0) < 1e-6, 'longest side one'
        assert info['cells_dens'] >= DENS, 'below the set\'s texel density at the reference size'
    print('painted in %.0f s' % (time.time() - t0))
    flat = None
    for th in theaters:
        models, sets, mapping = [], [], {}
        for i, (mesh, sheets) in enumerate(zip(meshes, sheets_all)):
            for ci, c in enumerate('ABC'):
                bark = sheets[(th, c)]
                px = bark.shape[0]
                bark_n = np.zeros((px, px, 4), np.uint8)
                bark_n[...] = (128, 128, 255, 200)      # flat: the facets are geometry
                dummy = np.zeros((4, 4, 4), np.uint8) + 128
                sets.append(dict(atlas=dummy, atlas_n=dummy, bark=bark, bark_n=bark_n,
                                 aw=4, ah=4, bp=px))
                P = mesh['P']; n = len(P)
                models.append(dict(P=P.astype('<f4'), N=mesh['N'].astype('<f4'),
                                   U=mesh['U'].astype('<f4'), T=mesh['T'].astype('<u4'),
                                   sway=np.zeros(n), AO=np.ones(n), TH=np.ones(n),
                                   MAT=np.zeros(n), set=len(sets) - 1,
                                   radius=float(np.sqrt((P[:, 0] ** 2 + P[:, 2] ** 2).max()))))
                mapping['RK%02d%s' % (i + 1, c)] = len(models) - 1
        # THE MARK: a code the game reads to know the stones are these painted ones (it
        # lights them at their own exposure, foliage3d_mod.h). No stone is called that,
        # and a reader of the RK<stone><colouring> codes passes over it.
        mapping['RKPAINT'] = 0
        path = os.path.join(out_dir, 'rock3d_%s.pack' % th)
        bt.write_pack3(path, models, sets, mapping)
    if sheet_png:
        draw_sheet(sheet_png, meshes, sheets_all, theaters)
    return meshes, sheets_all


# ======================================================================= the sheet
def render(P, T, U, N, tex, S, L, bg, yaw=0.6, pitch=0.5):
    """One stone, orthographic, from the south-east and above, lit by L with a little sky,
    its texture sampled bilinear; a z buffer."""
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    # turned by yaw about y, then seen from above at pitch: screen x, screen y, depth
    R = np.array([[cy, 0, -sy], [-sp * sy, cp, -sp * cy], [cp * sy, sp, cp * cy]])
    Q = P @ R.T
    img = np.zeros((S, S, 3)) + bg
    zb = np.full((S, S), -1e9)
    tw = tex.shape[0]
    for tri in T:
        p = Q[tri]
        xs = (p[:, 0] * 0.78 + 0.5) * S
        ys = (0.5 - p[:, 1] * 0.78) * S
        x0, x1 = int(max(xs.min(), 0)), int(min(xs.max() + 1, S))
        y0, y1 = int(max(ys.min(), 0)), int(min(ys.max() + 1, S))
        (ax, ay), (bx, by), (cx, cy_) = zip(xs, ys)
        den = (by - cy_) * (ax - cx) + (cx - bx) * (ay - cy_)
        if x1 <= x0 or y1 <= y0 or abs(den) < 1e-9:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        w0 = ((by - cy_) * (gx - cx) + (cx - bx) * (gy - cy_)) / den
        w1 = ((cy_ - ay) * (gx - cx) + (ax - cx) * (gy - cy_)) / den
        w2 = 1 - w0 - w1
        ins = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        z = w0 * p[0, 2] + w1 * p[1, 2] + w2 * p[2, 2]
        sub = zb[y0:y1, x0:x1]
        ok = ins & (z > sub)
        if not ok.any():
            continue
        uv = w0[..., None] * U[tri[0]] + w1[..., None] * U[tri[1]] + w2[..., None] * U[tri[2]]
        fx = np.clip(uv[..., 0] * tw - 0.5, 0, tw - 1.001); fy = np.clip(uv[..., 1] * tw - 0.5, 0, tw - 1.001)
        ix, iy = fx.astype(int), fy.astype(int); ax_, ay_ = fx - ix, fy - iy
        t = tex[..., :3].astype(float)
        c = (t[iy, ix] * ((1 - ax_) * (1 - ay_))[..., None] + t[iy, ix + 1] * (ax_ * (1 - ay_))[..., None]
             + t[iy + 1, ix] * ((1 - ax_) * ay_)[..., None] + t[iy + 1, ix + 1] * (ax_ * ay_)[..., None])
        nn = w0[..., None] * N[tri[0]] + w1[..., None] * N[tri[1]] + w2[..., None] * N[tri[2]]
        nn /= np.maximum(np.linalg.norm(nn, axis=-1, keepdims=True), 1e-9)
        sh = 0.42 + 0.75 * np.clip(nn @ L, 0, 1) + 0.12 * np.clip(nn[..., 1], 0, 1)
        sub[ok] = z[ok]
        img[y0:y1, x0:x1][ok] = np.clip(c[ok] * sh[ok][:, None], 0, 255)
    return img


SHEET_SUN = unit([-0.55, 0.62, 0.45])       # from the upper left, in the model's frame


def draw_sheet(path, meshes, sheets_all, theaters, S=150):
    rows = [(th, c) for th in theaters for c in 'ABC']
    W = S * len(meshes); H = S * len(rows)
    out = Image.new('RGB', (W, H), (52, 54, 60))
    for r_, key in enumerate(rows):
        for i, (m, sh) in enumerate(zip(meshes, sheets_all)):
            img = render(m['P'], m['T'], m['U'], m['N'], sh[key], S, SHEET_SUN, (52, 54, 60))
            out.paste(Image.fromarray(img.astype(np.uint8)), (i * S, r_ * S))
    out.save(path)
    print('sheet:', path)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out-dir', required=True)
    ap.add_argument('--theater', nargs='*', default=list(THEATERS))
    ap.add_argument('--sheet')
    ap.add_argument('--quick', action='store_true')
    a = ap.parse_args()
    for th in a.theater:
        assert th in THEATERS, th
    write_packs(a.out_dir, a.theater, a.quick, a.sheet)


if __name__ == '__main__':
    main()
