"""The generic repairs of the fix-and-improve recipe, in bmesh. Run inside Blender.

Every operation here keeps the cartridge's own texture mapping and baked vertex colour
wherever the cartridge stated them, and derives them for new faces from the faces that
already exist, never from a guess:

  complete_by_rotation   a round part with segments missing: copy an existing segment
                         round the fitted axis into each gap.
  densify_curve          double a round part's segment count. Quads along the curve are
                         cut in half, UV and colour interpolate exactly, and the new
                         vertices are pushed out onto the fitted circle so the silhouette
                         rounds instead of staying a polygon.
  close_convex           a small single-sided prop (pipe, bracket, crate) whose back was
                         never modelled: its convex hull, keeping every original face.
  double_side            a thin plate that is one-sided: a reversed copy behind it.
  fit_uv / fit_colour    what a new face borrows from its neighbours.

Positions are Blender cells, Z up (see blend_common.to_blender).
"""
import math

import bmesh
import numpy as np
from mathutils import Vector


# ---------------------------------------------------------------- measuring
def fit_circle(pts):
    """Least-squares circle through 2D points: (cx, cy, r)."""
    p = np.asarray(pts, float)
    A = np.c_[2 * p[:, 0], 2 * p[:, 1], np.ones(len(p))]
    b = (p ** 2).sum(1)
    cx, cy, c = np.linalg.lstsq(A, b, rcond=None)[0]
    return float(cx), float(cy), float(math.sqrt(max(c + cx * cx + cy * cy, 0.0)))


def angle_of(co, c):
    return math.atan2(co.y - c[1], co.x - c[0])


def radius_of(co, c):
    return math.hypot(co.x - c[0], co.y - c[1])


def layers(bm):
    return (bm.loops.layers.uv.active, bm.loops.layers.color.get("Col"),
            bm.faces.layers.int.get("pack_tex"))


# ---------------------------------------------------------------- borrowing
def fit_uv(faces, uvl, feats):
    """Least-squares linear map from a per-corner feature vector to UV, over the given
    faces. Returns (predict(feature) -> (u, v), rms). A cartridge part whose texture was
    projected (planar, cylindrical) fits to within a fraction of a texel."""
    X, Y = [], []
    for f in faces:
        for lp in f.loops:
            X.append(feats(lp.vert.co))
            Y.append(tuple(lp[uvl].uv))
    X, Y = np.asarray(X, float), np.asarray(Y, float)
    M = np.linalg.lstsq(X, Y, rcond=None)[0]
    rms = float(np.sqrt(((X @ M - Y) ** 2).sum(1).mean()))
    return (lambda co: tuple(np.asarray(feats(co), float) @ M)), rms


def ring_colour(faces, col, centre, zq=0.02):
    """Baked colour as a function of (height ring, angle) round an axis, from the given
    faces. A missing arc of angles interpolates across the gap, so a rebuilt back wall
    shades from one side of the existing front to the other. Returns colour(co)."""
    rings = {}
    for f in faces:
        for lp in f.loops:
            z = round(lp.vert.co.z / zq)
            rings.setdefault(z, []).append((angle_of(lp.vert.co, centre), tuple(lp[col])))
    for z in rings:
        rings[z].sort()

    def colour(co):
        zk = min(rings, key=lambda k: abs(k - co.z / zq))
        ring = rings[zk]
        a = angle_of(co, centre)
        best = None
        for i in range(len(ring)):
            a0, c0 = ring[i]
            a1, c1 = ring[(i + 1) % len(ring)]
            span = (a1 - a0) % (2 * math.pi) or 2 * math.pi
            t = ((a - a0) % (2 * math.pi)) / span
            if 0.0 <= t <= 1.0:
                best = tuple(c0[k] * (1 - t) + c1[k] * t for k in range(4))
                break
        return best or ring[0][1]
    return colour


def smooth_ring_shade(faces, col, centre, zq=0.02, order=1):
    """One smooth shading model for a whole round part, fitted to the cartridge's own
    baked colours: per height ring, colour = a0 + a1 cos(angle) + b1 sin(angle) (order 1
    is what a single light on a cylinder looks like). The cartridge often paints a round
    part flat per face, so a completed part whose new half is interpolated would read as
    two halves; this re-shades old and new alike. Returns colour(co). The fit is made on
    the faces given, so call it BEFORE adding the new half."""
    rings = {}
    for f in faces:
        for lp in f.loops:
            z = round(lp.vert.co.z / zq)
            rings.setdefault(z, []).append((angle_of(lp.vert.co, centre), tuple(lp[col])))
    fits = {}
    for z, samples in rings.items():
        A = []
        for a, _ in samples:
            row = [1.0]
            for k in range(1, order + 1):
                row += [math.cos(k * a), math.sin(k * a)]
            A.append(row)
        A = np.asarray(A)
        Y = np.asarray([c for _, c in samples], float)
        coef = np.linalg.lstsq(A, Y, rcond=None)[0]
        fits[z] = (coef, Y.min(0), Y.max(0))

    def colour(co):
        zk = min(fits, key=lambda k: abs(k - co.z / zq))
        coef, lo, hi = fits[zk]
        a = angle_of(co, centre)
        row = [1.0]
        for k in range(1, order + 1):
            row += [math.cos(k * a), math.sin(k * a)]
        c = np.clip(np.asarray(row) @ coef, lo, hi)
        return tuple(float(x) for x in c)
    return colour


def apply_colour(faces, col, colour):
    for f in faces:
        for lp in f.loops:
            lp[col] = colour(lp.vert.co)


# ---------------------------------------------------------------- round parts
def complete_by_rotation(bm, seg_faces, centre, step_deg, targets_deg, uv_pred=None,
                         colour=None):
    """Copy the faces of one segment round the axis through `centre`, once per target
    angle offset (degrees), and return the new faces. UV and colour of the copies come
    from uv_pred(co) / colour(co) when given, otherwise they are carried from the source
    segment unchanged (right for a texture that repeats per segment)."""
    uvl, col, tex = layers(bm)
    new = []
    for off in targets_deg:
        r = math.radians(off)
        cr, sr = math.cos(r), math.sin(r)
        vmap = {}
        for f in seg_faces:
            for v in f.verts:
                if v in vmap:
                    continue
                x, y = v.co.x - centre[0], v.co.y - centre[1]
                vmap[v] = bm.verts.new((centre[0] + x * cr - y * sr,
                                        centre[1] + x * sr + y * cr, v.co.z))
        for f in seg_faces:
            nf = bm.faces.new([vmap[v] for v in f.verts])
            nf.material_index = f.material_index
            nf[tex] = f[tex]
            for lo, ln in zip(f.loops, nf.loops):
                ln[uvl].uv = uv_pred(ln.vert.co) if uv_pred else lo[uvl].uv
                ln[col] = colour(ln.vert.co) if colour else lo[col]
            new.append(nf)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=3e-3)
    return [f for f in new if f.is_valid]


def along_curve(e, centre, min_deg=6.0, max_deg=45.0):
    """True for an edge that runs round the axis (a chord of a ring), not up it, not
    across it, and not a long chord through the middle."""
    p0, p1 = e.verts[0].co, e.verts[1].co
    a0, a1 = angle_of(p0, centre), angle_of(p1, centre)
    span = abs((a1 - a0 + math.pi) % (2 * math.pi) - math.pi)
    if not (math.radians(min_deg) < span < math.radians(max_deg)):
        return False
    d = p1 - p0
    m = 0.5 * (p0 + p1)
    rad = Vector((m.x - centre[0], m.y - centre[1], 0.0))
    if rad.length < 1e-9:
        return False
    rad.normalize()
    tang = d - rad * d.dot(rad)
    tang.z = 0.0
    return tang.length > 0.8 * d.length


def symmetry_axes(verts, centre, step_deg=1.0, tol_deg=0.5):
    """Every vertical mirror plane through `centre` under which the part's set of side
    ANGLES maps onto itself: [(angle in degrees, mean miss in degrees)]. Judged on the
    angles alone, not the vertices, because the half that is missing has no vertices to
    compare; a complete ring anywhere on the part (a rim, a base) states every side.
    The cartridge often spaces a round part's sides unevenly but symmetrically, so a
    mirror copy meets the existing edges exactly where a rotated copy does not."""
    rmax = max(radius_of(w.co, centre) for w in verts)
    raw = sorted(math.degrees(angle_of(v.co, centre)) % 360 for v in verts
                 if radius_of(v.co, centre) >= 0.5 * rmax)
    # cluster the side angles (sorted, so the result does not depend on vertex order)
    angs, cur = [], [raw[0]]
    for a in raw[1:]:
        if a - cur[-1] <= 2.0:
            cur.append(a)
        else:
            angs.append(sum(cur) / len(cur))
            cur = [a]
    angs.append(sum(cur) / len(cur))
    if len(angs) > 1 and (angs[0] + 360) - angs[-1] <= 2.0:
        merged = [angs[-1]] + [a + 360 for a in [angs[0]]]
        angs = [(sum(merged) / 2) % 360] + angs[1:-1]
    found = []
    for k in range(int(180 / step_deg)):
        t = k * step_deg
        miss = sum(min(abs(((2 * t - a) - b + 180) % 360 - 180) for b in angs) for a in angs)
        miss /= max(len(angs), 1)
        if miss < tol_deg:
            found.append((t, miss))
    return found


def _covered(p, faces, tol):
    from mathutils.geometry import closest_point_on_tri
    for f in faces:
        vs = [v.co for v in f.verts]
        for i in range(1, len(vs) - 1):
            if (closest_point_on_tri(p, vs[0], vs[i], vs[i + 1]) - p).length < tol:
                return True
    return False


def complete_by_mirror(bm, faces, centre, axes, uv_pred=None, colour=None, tol=3e-3):
    """Mirror a part's faces across a vertical plane through `centre`, keeping only the
    mirrored faces that land where nothing was modelled, and weld them in. Of the
    candidate `axes` (degrees) the one that fills the most area wins. Returns
    (new faces, chosen axis)."""
    uvl, col, tex = layers(bm)

    def mirror_fn(axis_deg):
        t = math.radians(axis_deg)
        c2, s2 = math.cos(2 * t), math.sin(2 * t)

        def m(co):
            x, y = co.x - centre[0], co.y - centre[1]
            return Vector((centre[0] + x * c2 + y * s2, centre[1] + x * s2 - y * c2, co.z))
        return m

    best = None
    for axis in axes:
        m = mirror_fn(axis)
        keep, area = [], 0.0
        for f in faces:
            pts = [m(v.co) for v in reversed(f.verts)]
            cm = sum(pts, Vector()) / len(pts)
            if _covered(cm, faces, tol * 4):
                continue
            keep.append((f, pts))
            area += f.calc_area()
        if best is None or area > best[0]:
            best = (area, axis, keep)
    if best is None:
        return [], None
    _, axis, keep = best
    new = []
    for f, pts in keep:
        nf = bm.faces.new([bm.verts.new(p) for p in pts])
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = uv_pred(ln.vert.co) if uv_pred else lo[uvl].uv
            ln[col] = colour(ln.vert.co) if colour else lo[col]
        new.append(nf)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=tol)
    return [f for f in new if f.is_valid], axis


def densify_curve(bm, faces, centre, min_deg=6.0, max_deg=45.0):
    """Double the angular resolution of a round part. Returns the faces afterwards."""
    faces = [f for f in faces if f.is_valid]
    res = bmesh.ops.join_triangles(bm, faces=faces, angle_face_threshold=math.radians(5),
                                   angle_shape_threshold=math.radians(180),
                                   cmp_uvs=True, cmp_vcols=True)
    faces = [f for f in set(faces) | set(res.get("faces", [])) if f.is_valid]
    edges = set()
    for f in faces:
        for e in f.edges:
            if along_curve(e, centre, min_deg, max_deg):
                edges.add(e)
    ends = {e: (e.verts[0].co.copy(), e.verts[1].co.copy()) for e in edges}
    out = bmesh.ops.subdivide_edges(bm, edges=list(edges), cuts=1, use_grid_fill=False,
                                    use_single_edge=False, use_only_quads=False)
    # push each new vertex out to the circle its edge was a chord of
    for v in [g for g in out["geom_inner"] if isinstance(g, bmesh.types.BMVert)]:
        near = [ends[e] for e in ends
                if (0.5 * (ends[e][0] + ends[e][1]) - v.co).length < 1e-5]
        if not near:
            continue
        p0, p1 = near[0]
        r = 0.5 * (radius_of(p0, centre) + radius_of(p1, centre))
        a0, a1 = angle_of(p0, centre), angle_of(p1, centre)
        a = a0 + ((a1 - a0 + math.pi) % (2 * math.pi) - math.pi) * 0.5
        v.co.x = centre[0] + r * math.cos(a)
        v.co.y = centre[1] + r * math.sin(a)
    allf = {f for f in bm.faces if f.is_valid}
    touched = [f for f in allf if any(v in f.verts for g in out["geom_inner"]
                                      if isinstance(g, bmesh.types.BMVert) for v in [g])]
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    return touched


# ---------------------------------------------------------------- props
def close_convex(bm, faces, skip_below_z=None, uv_faces=None):
    """Close a single-sided convex prop with its hull. Existing faces are kept; each
    hull face not already covered by one becomes a new face, textured by a planar fit
    of the prop's own UVs and coloured by the mean of the prop's existing corners times
    a shade that darkens faces pointing away from the old front. Returns new faces."""
    uvl, col, tex = layers(bm)
    faces = [f for f in faces if f.is_valid]
    verts = list({v for f in faces for v in f.verts})
    pts = [v.co.copy() for v in verts]
    tmp = bmesh.new()
    tv = [tmp.verts.new(p) for p in pts]
    hull = bmesh.ops.convex_hull(tmp, input=tv, use_existing_faces=False)
    hfaces = [g for g in hull["geom"] if isinstance(g, bmesh.types.BMFace)]
    # front: the area-weighted mean normal of what the cartridge modelled
    front = Vector()
    for f in faces:
        front += f.normal * f.calc_area()
    front = front.normalized() if front.length > 1e-9 else Vector((0, 0, 1))
    uvf = [f for f in (uv_faces or faces) if f.is_valid]
    pred, rms = fit_uv(uvf, uvl, lambda co: (co.x, co.y, co.z, 1.0))
    mean = [0.0] * 4
    n = 0
    for f in faces:
        for lp in f.loops:
            for k in range(4):
                mean[k] += lp[col][k]
            n += 1
    mean = [m / max(n, 1) for m in mean]
    byco = {tuple(round(c, 5) for c in v.co): v for v in verts}
    new = []
    for hf in hfaces:
        c = hf.calc_center_median()
        if skip_below_z is not None and all(v.co.z <= skip_below_z for v in hf.verts):
            continue
        covered = False
        for f in faces:
            if f.normal.dot(hf.normal) > 0.98 and abs((c - f.verts[0].co).dot(f.normal)) < 2e-3:
                covered = True
                break
        if covered:
            continue
        vs = [byco.get(tuple(round(x, 5) for x in v.co)) for v in hf.verts]
        if None in vs:
            continue
        try:
            nf = bm.faces.new(vs)
        except ValueError:
            continue
        nf.normal_update()
        if nf.normal.dot(hf.normal) < 0:
            nf.normal_flip()
        nf.material_index = uvf[0].material_index
        nf[tex] = uvf[0][tex]
        shade = 0.55 + 0.45 * max(0.0, nf.normal.dot(front))
        for lp in nf.loops:
            lp[uvl].uv = pred(lp.vert.co)
            lp[col] = (mean[0] * shade, mean[1] * shade, mean[2] * shade, mean[3])
        new.append(nf)
    tmp.free()
    bmesh.ops.triangulate(bm, faces=[f for f in new if len(f.verts) > 3])
    return new, rms


def double_side(bm, faces, shade=0.6):
    """A reversed copy behind each face, darkened, for plates with no thickness."""
    uvl, col, tex = layers(bm)
    new = []
    for f in faces:
        vs = [bm.verts.new(v.co) for v in reversed(f.verts)]
        nf = bm.faces.new(vs)
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo[uvl].uv
            c = lo[col]
            ln[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
        new.append(nf)
    return new


def agree_winding(bm, faces, rounds=4):
    """Flip any of `faces` whose winding disagrees with most of its neighbours. A new
    face that closes a gap should run the same way round as the cartridge faces it
    meets; checking that is more reliable than guessing an outward direction when the
    gap's corners are not coplanar. Returns the number flipped."""
    flipped = 0
    for _ in range(rounds):
        changed = 0
        for f in faces:
            if not f.is_valid:
                continue
            agree = disagree = 0
            for lp in f.loops:
                e = lp.edge
                for other in e.link_faces:
                    if other is f:
                        continue
                    ol = [l for l in e.link_loops if l.face is other][0]
                    if ol.vert is lp.vert:
                        disagree += 1
                    else:
                        agree += 1
            if disagree > agree:
                f.normal_flip()
                changed += 1
        flipped += changed
        if not changed:
            break
    return flipped


def open_edges(bm, faces=None):
    fs = set(faces) if faces else None
    out = []
    for e in bm.edges:
        lf = [f for f in e.link_faces if fs is None or f in fs]
        if len(lf) == 1:
            out.append(e)
    return out
