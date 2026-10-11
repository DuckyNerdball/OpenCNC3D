#!/usr/bin/env python3
"""Levels of detail for a skinned .c3m mesh, by half-edge collapse under quadric error.

    lods = simplify_chain(pos, nrm, tris, skin_b, skin_w, [0.5, 0.25])

pos, nrm   (n, 3) float, the bind pose in pack units, one row per ATTRIBUTE vertex (a
           point on a texture seam is two or more rows with the same position)
tris       (m, 3) int, triangles over those rows, wound outward
skin_b/w   (n, 4) uint8, the four bone influences per row
fractions  the triangle counts wanted, as fractions of the input, each smaller than the
           one before

Returns one (tris, ids) pair per fraction: the level's (m_k, 3) triangles and, for each,
the index of the input triangle it descends from (so it keeps that triangle's material). Each level is simplified from the one
before, so the levels are NESTED: every vertex a coarser level uses is also used by
every finer one. The writer relies on that to put the coarsest level's vertices first.

WHY HALF-EDGE COLLAPSE. A collapse u -> v deletes u and hands its triangles to v, which
does not move. No vertex is ever created, so every level is an index list over the
original vertex array: the UVs, normals and skin weights of whatever survives are the
artist's own, exactly, and the renderer can skin a level by skinning only the vertices it
uses. The price is a little accuracy against a collapse that places the merged vertex
optimally, which at a soldier's size on screen nobody can see.

WHAT MAY COLLAPSE. The mesh is welded by POSITION to find its shape; the attribute
vertices at one position are that position's "corners". A collapse of position U onto
position V is allowed when every surviving triangle corner at U can be given a corner at
V that it already shares an edge with: an interior point simply joins its neighbour, a
point on a texture seam may only slide along that seam (each side's corner onto the same
side's corner), and a seam point may not cut across into one side's interior. Open borders
(edges with one triangle) may only collapse along themselves. A collapse is refused when it
would flip a surviving triangle, or when U and V are skinned too differently (the joint
would crease when it bends).

COST. Garland-Heckbert quadrics of the planes around each position, plus a heavy quadric
on the plane through every border edge, so the silhouette of an open shell holds. The
error of U -> V is U's quadric at V's position, plus a penalty for the skinning difference.
"""
import heapq

import numpy as np

SKIN_PENALTY = 0.5      # per unit of weight difference, times the edge's length squared
SKIN_REFUSE = 0.75      # refuse outright past this much weight difference (sum of |dw|)
BORDER_WEIGHT = 10.0
FLIP_COS = 0.2          # a surviving triangle's normal may not turn past ~78 degrees


def _weights(skin_b, skin_w):
    """Each row's influences as a dict bone -> weight in 0..1."""
    out = []
    for b, w in zip(skin_b, skin_w):
        d = {}
        for j in range(4):
            if w[j]:
                d[int(b[j])] = d.get(int(b[j]), 0.0) + w[j] / 255.0
        out.append(d)
    return out


def _wdiff(a, b):
    return sum(abs(a.get(k, 0.0) - b.get(k, 0.0)) for k in set(a) | set(b))


def simplify_chain(pos, nrm, tris, skin_b, skin_w, fractions, log=print):
    pos = np.asarray(pos, np.float64)
    tris = np.asarray(tris, np.int64)
    n = len(pos)
    # ---- weld by position --------------------------------------------------------------
    key = {}
    pclass = np.zeros(n, np.int64)
    for i, p in enumerate(map(tuple, np.round(pos, 5))):
        pclass[i] = key.setdefault(p, len(key))
    npos = len(key)
    ppos = np.zeros((npos, 3))
    ppos[pclass] = pos
    # costs in a unit box, so the skin penalty weighs the same at any model scale
    ppos = (ppos - ppos.min(0)) / max(1e-9, float(np.linalg.norm(ppos.max(0) - ppos.min(0))))
    corners = [set() for _ in range(npos)]
    for i in range(n):
        corners[pclass[i]].add(i)
    wts = _weights(skin_b, skin_w)
    # the skin a POSITION carries: its corners agree on it (they are one point of skin)
    pw = [wts[min(c)] for c in corners]

    alive = np.ones(len(tris), bool)
    T = tris.copy()                               # corner (attribute vertex) indices
    P = pclass[T]                                  # position indices
    ptris = [set() for _ in range(npos)]           # position -> live triangles
    for t in range(len(T)):
        for k in range(3):
            ptris[P[t, k]].add(t)

    # ---- quadrics ----------------------------------------------------------------------
    Q = np.zeros((npos, 4, 4))

    def plane(a, b, c):
        nn = np.cross(b - a, c - a)
        ln = np.linalg.norm(nn)
        if ln < 1e-12:
            return None, 0.0
        nn /= ln
        return np.append(nn, -nn @ a), ln * 0.5

    # AREA-WEIGHTED, BUT RELATIVE TO THE MEAN TRIANGLE, so a cost is a squared distance
    # in the unit box times about the number of planes around the point, whatever the
    # triangle count; the skin penalty is scaled to match (see cost).
    areas = []
    for t in range(len(T)):
        areas.append(plane(*ppos[P[t]])[1])
    mean_area = max(1e-18, float(np.mean(areas)))
    for t in range(len(T)):
        pl, area = plane(*ppos[P[t]])
        if pl is None:
            continue
        K = np.outer(pl, pl) * (area / mean_area)
        for k in range(3):
            Q[P[t, k]] += K
    # border edges: an edge of the welded shape that only one triangle uses
    ecount = {}
    for t in range(len(T)):
        for k in range(3):
            a, b = P[t, k], P[t, (k + 1) % 3]
            e = (min(a, b), max(a, b))
            ecount.setdefault(e, []).append(t)
    border = set(e for e, ts in ecount.items() if len(ts) == 1)
    border_pos = set()
    for (a, b) in border:
        border_pos.add(a)
        border_pos.add(b)
        t = ecount[(a, b)][0]
        pl, area = plane(*ppos[P[t]])
        if pl is None:
            continue
        en = np.cross(ppos[b] - ppos[a], pl[:3])
        ln = np.linalg.norm(en)
        if ln < 1e-12:
            continue
        en /= ln
        bp = np.append(en, -en @ ppos[a])
        K = np.outer(bp, bp) * BORDER_WEIGHT
        Q[a] += K
        Q[b] += K

    # ---- the collapse test ----------------------------------------------------------------
    def corner_map(U, V):
        """For each corner of U used by a triangle that survives U -> V, the corner of V
        it shares an edge with; None when some corner has no such partner (or two)."""
        vcs = corners[V]
        m = {}
        for t in ptris[U]:
            ks = list(P[t])
            if V in ks:
                continue
            m.setdefault(T[t][ks.index(U)], None)
        if not m:
            return m
        # partners: corner c of U and corner d of V appear together in a live triangle
        partners = {c: set() for c in m}
        for t in ptris[U] & ptris[V]:
            row = T[t]
            cu = row[list(P[t]).index(U)]
            cv = row[list(P[t]).index(V)]
            if cu in partners:
                partners[cu].add(cv)
        for c in m:
            if len(partners[c]) == 1:
                m[c] = next(iter(partners[c]))
            elif len(vcs) == 1 and len(corners[U]) == 1:
                m[c] = next(iter(vcs))              # both interior: nothing to match
            else:
                return None
        return m

    def allowed(U, V):
        if not (ptris[U] & ptris[V]):
            return None
        if U in border_pos:
            e = (min(U, V), max(U, V))
            if e not in border:
                return None                       # a border point slides along the border
        if _wdiff(pw[U], pw[V]) > SKIN_REFUSE:
            return None
        cm = corner_map(U, V)
        if cm is None:
            return None
        # no surviving triangle may flip or collapse to a sliver
        for t in ptris[U]:
            ks = list(P[t])
            if V in ks:
                continue
            a, b, c = ppos[ks[0]], ppos[ks[1]], ppos[ks[2]]
            n0 = np.cross(b - a, c - a)
            q = [ppos[V] if k == U else ppos[k] for k in ks]
            n1 = np.cross(q[1] - q[0], q[2] - q[0])
            l0, l1 = np.linalg.norm(n0), np.linalg.norm(n1)
            if l1 < 1e-9 or l0 < 1e-12:
                return None
            if (n0 @ n1) / (l0 * l1) < FLIP_COS:
                return None
        return cm

    def cost(U, V):
        v = np.append(ppos[V], 1.0)
        e2 = float(np.sum((ppos[U] - ppos[V]) ** 2))
        return float(v @ Q[U] @ v) + SKIN_PENALTY * _wdiff(pw[U], pw[V]) * e2

    heap = []
    stamp = np.zeros(npos, np.int64)
    removed = np.zeros(npos, bool)

    def push_all(U):
        nb = set()
        for t in ptris[U]:
            nb.update(P[t])
        nb.discard(U)
        for V in nb:
            heapq.heappush(heap, (cost(U, V), int(U), int(V), int(stamp[U]), int(stamp[V])))

    for U in range(npos):
        push_all(U)

    live = int(alive.sum())
    total = live
    out = []
    for frac in fractions:
        target = int(round(total * frac))
        while live > target and heap:
            c, U, V, su, sv = heapq.heappop(heap)
            if removed[U] or removed[V] or su != stamp[U] or sv != stamp[V]:
                continue
            cm = allowed(U, V)
            if cm is None:
                continue
            # ---- collapse U -> V ----
            for t in list(ptris[U]):
                ks = list(P[t])
                if V in ks:                        # the triangles on the edge die
                    alive[t] = False
                    live -= 1
                    for k in ks:
                        ptris[k].discard(t)
                    continue
                j = ks.index(U)
                T[t][j] = cm[T[t][j]]
                P[t][j] = V
                ptris[V].add(t)
            ptris[U] = set()
            removed[U] = True
            Q[V] += Q[U]
            if U in border_pos:
                border_pos.add(V)
            # the border edges U had now run from V
            for (a, b) in [e for e in border if U in e]:
                border.discard((a, b))
                o = b if a == U else a
                if o != V:
                    border.add((min(o, V), max(o, V)))
            stamp[V] += 1
            nb = set()
            for t in ptris[V]:
                nb.update(P[t])
            nb.discard(V)
            for W in nb:
                stamp[W] += 1
            push_all(V)
            for W in nb:
                push_all(W)
        lv = T[alive].copy()
        out.append((lv, np.nonzero(alive)[0]))
        log("LOD|%.2f|%d of %d triangles|%d vertices" % (
            frac, len(lv), total, len(np.unique(lv))))
    return out
