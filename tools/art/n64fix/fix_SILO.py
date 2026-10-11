"""The Tiberium Silo's repair plan: the cartridge model, completed and rounded.

    blender -b --python tools/art/n64fix/fix_SILO.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/SILO_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the 1995 sprite and its
construction animation: a round glass dome inside a low ring, two house-coloured
arches crossing over it, dark foot blocks where the arches land, a house-coloured
annex on the east and a pipe from it, on a slab.

  dome    (part 1, its own object and pivot) 8 sides, every one modelled, on a slightly
          oval footprint (the north-south radius is 0.91 of the east-west one). Rebuilt
          in the frame where that oval is a circle: 16 sides (the cartridge's 8 corners
          kept, one between each pair on the circle) and a ring added in the lower band
          and in the cap, set out halfway from the cartridge's straight band towards the
          ellipse through its own base, waist and apex. Halfway, not all the way: the full
          ellipse would push the dome through the arches, which hug the cartridge's
          straight bands 30 to 40 model units above them. Re-shaded as one from a smooth
          fit of its own painted sides.
  ring    the 10-sided band round the dome's foot, whole on the cartridge and oval like
          the dome: doubled to 20 sides in its own circular frame, its sides re-shaded
          as one, its flat top remapped by the top's own planar projection.
  blocks  the four dark foot blocks under the arch ends each lack the face toward the
          dome (the west one also its north side, and that side's foot corner). Closed
          with the cartridge's own corners dropped to the ground; the blocks are painted
          flat, so the new faces take the one light that best fits their painted faces.
  annex   the long house-coloured block on the north-east lacks its north-east long
          wall: closed with the mirror of the south-west wall (same UVs, same colour).
  pipe    a flat strip at pipe-axis height, one pipe wide, painted with a cylinder: made
          the round pipe it paints, 8 sides, the texture laid across it as the painting
          projects from above, both ends capped inside the annex and the foot block it
          runs into.
  arches  solid on the cartridge (their bottoms are buried in the dome) and stay.
  slab    already closed (its underside is never seen) and stays.
"""
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402
import repair as R  # noqa: E402

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
B.reset_scene()
doc, objs = B.build_cartridge(src, "SILO")
report = {"cartridge_triangles": sum(1 for t in doc["tris"] if t["mode"] != "shadow")}


# ------------------------------------------------------------------ helpers
def open_bm(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
    bm.normal_update()
    return bm


def close_bm(bm, ob):
    # deleting faces to rebuild them leaves their edges and corners behind, loose
    bmesh.ops.delete(bm, geom=[e for e in bm.edges if not e.link_faces], context="EDGES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    bm.normal_update()
    n = len(bm.faces)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return n


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def oval_frame(pts):
    """The frame in which a slightly oval ring of points is a circle: fit the conic
    a x^2 + b xy + c y^2 + d x + e y = 1, move its centre to the origin, turn its major
    axis onto x and stretch the minor axis to the major's length. Returns (to_frame,
    from_frame, stretch), each map taking and returning a Vector (z untouched)."""
    P = np.asarray(pts, float)
    x, y = P[:, 0], P[:, 1]
    a, b, c, d, e = np.linalg.lstsq(np.c_[x * x, x * y, y * y, x, y], np.ones(len(P)), rcond=None)[0]
    cx, cy = np.linalg.solve([[2 * a, b], [b, 2 * c]], [-d, -e])
    lam, vec = np.linalg.eigh([[a, b / 2], [b / 2, c]])
    u, w = vec[:, 0], vec[:, 1]          # major (smaller eigenvalue), minor
    k = math.sqrt(lam[1] / lam[0])

    def to_frame(p):
        qx, qy = p.x - cx, p.y - cy
        return Vector((qx * u[0] + qy * u[1], (qx * w[0] + qy * w[1]) * k, p.z))

    def from_frame(q):
        s, t = q.x, q.y / k
        return Vector((cx + s * u[0] + t * w[0], cy + s * u[1] + t * w[1], q.z))
    return to_frame, from_frame, k


def light_fit(faces, col):
    """Least-squares colour = a + b.normal over flat-painted faces: the one light that
    best explains them. Returns colour(normal), rms."""
    X, Y = [], []
    for f in faces:
        for lp in f.loops:
            X.append((1.0, f.normal.x, f.normal.y, f.normal.z))
            Y.append(tuple(lp[col]))
    X, Y = np.asarray(X), np.asarray(Y, float)
    M = np.linalg.lstsq(X, Y, rcond=None)[0]
    rms = float(np.sqrt(((X @ M - Y) ** 2).sum(1).mean()))

    def colour(n):
        return tuple(float(v) for v in np.clip(np.asarray((1.0, n.x, n.y, n.z)) @ M, 0.0, 1.0))
    return colour, rms


def drop_partner(t, faces, z_floor, made):
    """The ground corner under top vertex t of a block: the lowest-lying vertex joined to
    t by an edge of the block and nearest to it in plan (a vertical corner, or the foot
    of a sloped one); where the cartridge modelled none, a new vertex straight under t."""
    fs = set(faces)
    cand = [e.other_vert(t) for e in t.link_edges
            if any(f in fs for f in e.link_faces) and e.other_vert(t).co.z < z_floor]
    if cand:
        return min(cand, key=lambda w: (w.co.xy - t.co.xy).length)
    if t not in made:
        low = min(v.co.z for v in verts_of(faces))
        made[t] = t.co.copy()
        made[t].z = low
    return made[t]


def close_by_drop(bm, faces, z_floor):
    """Close a block that sits on the ground and is missing side walls: every open top
    edge (both ends above z_floor) gets a wall down to the ground corners under its ends.
    Returns the new faces (UV and colour left for the caller)."""
    fs = set(faces)
    made = {}
    new = []
    tops = [e for e in R.open_edges(bm, faces) if all(v.co.z > z_floor for v in e.verts)]
    for e in tops:
        a, b = e.verts
        pa, pb = drop_partner(a, faces, z_floor, made), drop_partner(b, faces, z_floor, made)
        if isinstance(pa, Vector):
            pa = made[a] = bm.verts.new(pa)
        if isinstance(pb, Vector):
            pb = made[b] = bm.verts.new(pb)
        vs = [a, b, pb, pa]
        if pa is pb:
            vs = [a, b, pb]
        ref = next(f for f in e.link_faces if f in fs)
        nf = bm.faces.new(vs)
        nf.material_index = ref.material_index
        new.append(nf)
    R.agree_winding(bm, new)
    for f in new:
        f.normal_update()
    return new


# ------------------------------------------------------------------ body
body = objs[0]
bm = open_bm(body)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")
BLOCK_A, BLOCK_B, BLOCK_C, BLOCK_D, PIPE, SLAB, ANNEX, RING, RING_TOP, ARCH = range(1, 11)
# parts by cartridge triangle index (extract.py order)
BY_TRI = {}
for ids, p in ((range(0, 8), BLOCK_A), (range(8, 16), BLOCK_B), (range(16, 22), BLOCK_C),
               (range(22, 30), BLOCK_D), (range(30, 34), PIPE), (range(34, 47), SLAB),
               ((47, 48, 113, 114, 115, 116, 117, 118), ANNEX), (range(49, 69), RING),
               (range(69, 77), RING_TOP), (range(77, 113), ARCH)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI[f[tri]]


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


# ------------------------------------------------------------------ foot blocks
blocks = (BLOCK_A, BLOCK_B, BLOCK_C, BLOCK_D)
block_light, block_rms = light_fit(faces_of(*blocks), col)
report["block_light_fit_rms"] = round(block_rms * 255, 1)
added = []
for bp in blocks:
    new = close_by_drop(bm, faces_of(bp), 0.1)
    for f in new:
        f[part] = bp
        f[tex] = -1
        c = block_light(f.normal)
        for lp in f.loops:
            lp[uvl].uv = (0.0, 0.0)
            lp[col] = c
    added.append(len(new))
report["block_walls_added"] = added

# ------------------------------------------------------------------ annex
annex = faces_of(ANNEX)
wall = close_by_drop(bm, annex, 0.1)
report["annex_walls_added"] = len(wall)
# the wall is the mirror of the south-west wall across the annex's long axis: each new
# corner's twin is one annex width away, and the twin's corner on the wall parallel to
# the new one lends its UV and colour
for f in wall:
    f[part] = ANNEX
    f[tex] = annex[0][tex]
    top = [v for v in f.verts if v.co.z > 0.1]
    others = [v for v in verts_of(annex) if v.co.z > 0.1 and v not in top]
    width = min((w.co - top[0].co for w in others), key=lambda d: d.length)
    for lp in f.loops:
        twin = min(verts_of(annex), key=lambda w: (w.co - (lp.vert.co + width)).length)
        src_lp = max((l for l in twin.link_loops if l.face in annex),
                     key=lambda l: -l.face.normal.dot(f.normal))
        lp[uvl].uv = src_lp[uvl].uv
        lp[col] = src_lp[col]
report["annex_width"] = round(width.length * B.CELL, 1)

# ------------------------------------------------------------------ pipe
pipe = faces_of(PIPE)
seg_tris = ((30, 31), (32, 33))
segs = [[f for f in pipe if f[tri] in ids] for ids in seg_tris]
sv = [set(verts_of(s)) for s in segs]
elbow = list(sv[0] & sv[1])
if len(elbow) != 2:
    raise SystemExit("pipe: expected the two segments to share one mitred edge")
X, Y = elbow


def uv_at(seg, v):
    return next(lp[uvl].uv.copy() for f in seg for lp in f.loops if lp.vert is v)


pipe_like = pipe[0]
pipe_mat = {i: s[0].material_index for i, s in enumerate(segs)}
pipe_tex = pipe_like[tex]
seg_col = [tuple(segs[i][0].loops[0][col]) for i in range(2)]
ends = []
for i, seg in enumerate(segs):
    far = [v for v in sv[i] if v not in elbow]
    uX = uv_at(seg, X).x
    fX = min(far, key=lambda v: abs(uv_at(seg, v).x - uX))
    fY = far[0] if far[1] is fX else far[1]
    ends.append(dict(fX=fX, fY=fY, uv={v: uv_at(seg, v) for v in (X, Y, fX, fY)},
                     r=0.5 * (fX.co - fY.co).length))
r_elbow = 0.5 * (ends[0]["r"] + ends[1]["r"])
report["pipe_radius"] = [round(e["r"] * B.CELL, 1) for e in ends]
bmesh.ops.delete(bm, geom=pipe, context="FACES_ONLY")
SIDES = 8


def pipe_ring(a, b, r):
    """A ring round the pipe's axis through the end edge a-b (a at angle 0, b at 180):
    horizontal half-width that of the edge (wider at the mitre), vertical radius r."""
    c = 0.5 * (a.co + b.co)
    h = 0.5 * (a.co - b.co)
    ring = []
    for k in range(SIDES):
        phi = 2.0 * math.pi * k / SIDES
        if k == 0:
            ring.append(a)
        elif k == SIDES // 2:
            ring.append(b)
        else:
            ring.append(bm.verts.new(c + h * math.cos(phi) + Vector((0, 0, r * math.sin(phi)))))
    return ring, c


elbow_ring, _ = pipe_ring(X, Y, r_elbow)
pipe_new = []
for i, e in enumerate(ends):
    far_ring, far_c = pipe_ring(e["fX"], e["fY"], e["r"])
    # the painting is a cylinder seen from above: the texel across the strip follows the
    # corner's plan position, so u and v go with (1 - cos phi) / 2 between the two sides
    def paint(lp, ring, a, b):
        k = ring.index(lp.vert)
        phi = 2.0 * math.pi * k / SIDES
        w = 0.5 * (1.0 - math.cos(phi))
        lp[uvl].uv = e["uv"][a].lerp(e["uv"][b], w)
        s = math.sin(phi)
        shade = 1.0 if s >= 0 else 1.0 + 0.4 * s
        c = seg_col[i]
        lp[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
    for k in range(SIDES):
        q = [elbow_ring[k], elbow_ring[(k + 1) % SIDES], far_ring[(k + 1) % SIDES], far_ring[k]]
        f = bm.faces.new(q)
        f[part] = PIPE
        f[tex] = pipe_tex
        f.material_index = pipe_mat[i]
        f.normal_update()
        m = f.calc_center_median()
        axis_pt = 0.5 * (0.5 * (X.co + Y.co) + far_c)
        if f.normal.dot(m - axis_pt) < 0:
            f.normal_flip()
        for lp in f.loops:
            if lp.vert in elbow_ring:
                paint(lp, elbow_ring, X, Y)
            else:
                paint(lp, far_ring, e["fX"], e["fY"])
        pipe_new.append(f)
    # the far end is buried in the annex or the foot block: capped all the same, so the
    # pipe is a closed solid, mapped and shaded like its own side in shadow
    capf = bm.faces.new(far_ring)
    capf[part] = PIPE
    capf[tex] = pipe_tex
    capf.material_index = pipe_mat[i]
    capf.normal_update()
    if capf.normal.dot(far_c - 0.5 * (X.co + Y.co)) < 0:
        capf.normal_flip()
    cuv = e["uv"][e["fX"]].lerp(e["uv"][e["fY"]], 0.5)
    for lp in capf.loops:
        lp[uvl].uv = cuv
        c = seg_col[i]
        lp[col] = (c[0] * 0.6, c[1] * 0.6, c[2] * 0.6, c[3])
    pipe_new.append(capf)
report["pipe_faces"] = len(pipe_new)

# ------------------------------------------------------------------ ring
ring_side = faces_of(RING)
rv = verts_of(ring_side)
z_foot = min(v.co.z for v in rv)
to_f, from_f, k_ring = oval_frame([(v.co.x, v.co.y) for v in rv if v.co.z < z_foot + 0.01])
report["ring_oval"] = round(1.0 / k_ring, 3)
top_uv, top_rms = R.fit_uv(faces_of(RING_TOP), uvl, lambda co: (co.x, co.y, 1.0))
report["ring_top_uv_rms"] = round(top_rms, 4)
ring_verts = verts_of(faces_of(RING, RING_TOP))
for v in ring_verts:
    v.co = to_f(v.co)
shade_ring = R.smooth_ring_shade(faces_of(RING), col, (0.0, 0.0))
R.densify_curve(bm, faces_of(RING, RING_TOP), (0.0, 0.0, 1.0))
R.apply_colour(faces_of(RING), col, shade_ring)
for v in verts_of(faces_of(RING, RING_TOP)):
    v.co = from_f(v.co)
for f in faces_of(RING_TOP):
    for lp in f.loops:
        lp[uvl].uv = top_uv(lp.vert.co)
report["ring_sides"] = sum(1 for v in verts_of(faces_of(RING)) if v.co.z < z_foot + 0.01)

report["body_winding_fixed"] = R.agree_winding(bm, faces_of(*blocks, ANNEX, PIPE))
report["body_triangles"] = close_bm(bm, body)
bm = bmesh.new()
bm.from_mesh(body.data)
report["body_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
bm.free()

# ------------------------------------------------------------------ dome (part 1)
dome_ob = objs[1]
dm = open_bm(dome_ob)
uvl, col, tex = R.layers(dm)
old = list(dm.faces)
dv = verts_of(old)
z_b = min(v.co.z for v in dv)
z_t = max(v.co.z for v in dv)
apex = max(dv, key=lambda v: v.co.z)
base = [v for v in dv if v.co.z < z_b + 0.01]
waist = [v for v in dv if v is not apex and v not in base]
to_f, from_f, k_dome = oval_frame([(v.co.x, v.co.y) for v in base])
report["dome_oval"] = round(1.0 / k_dome, 3)
fr = {v: to_f(v.co) for v in dv}


def ang(q):
    return math.atan2(q.y, q.x)


def rad(q):
    return math.hypot(q.x, q.y)


base.sort(key=lambda v: ang(fr[v]))
waist.sort(key=lambda v: ang(fr[v]))
z_m = sum(v.co.z for v in waist) / len(waist)
rb = sum(rad(fr[v]) for v in base) / len(base)
rm = sum(rad(fr[v]) for v in waist) / len(waist)

# the ellipse through the base, waist and apex (axis vertical, top at the apex)
best = None
for z0 in np.linspace(-0.5, z_m - 1e-3, 20001):
    b_ = z_t - z0
    s = 1.0 - ((z_m - z0) / b_) ** 2
    if s <= 0:
        continue
    a_ = rm / math.sqrt(s)
    err = (rb / a_) ** 2 + ((z_b - z0) / b_) ** 2 - 1.0
    if best is None or abs(err) < abs(best[0]):
        best = (err, z0, a_, b_)
_, e_z0, e_a, e_b = best
report["dome_profile_ellipse"] = [round(x * B.CELL, 1) for x in (e_z0, e_a, e_b)]


def ellipse_r(z):
    return e_a * math.sqrt(max(0.0, 1.0 - ((z - e_z0) / e_b) ** 2))


BULGE = 0.5
z_1, z_2 = 0.5 * (z_b + z_m), 0.5 * (z_m + z_t)
bulge_1 = ellipse_r(z_1) - 0.5 * (rb + rm)
bulge_2 = ellipse_r(z_2) - 0.5 * rm
report["dome_bulge_units"] = [round(BULGE * x * B.CELL, 1) for x in (bulge_1, bulge_2)]

# one smooth shade for the whole dome, fitted to its painted sides in the circular frame:
# per ring (base, waist) colour = a0 + a1 cos + b1 sin, interpolated in height, and the
# apex's mean at the top
rings = {"b": [], "m": []}
apex_cols = []
for f in old:
    for lp in f.loops:
        q = fr[lp.vert]
        if lp.vert is apex:
            apex_cols.append(tuple(lp[col]))
        else:
            rings["b" if lp.vert in base else "m"].append((ang(q), tuple(lp[col])))
fits = {}
for key, s in rings.items():
    A = np.asarray([[1.0, math.cos(a), math.sin(a)] for a, _ in s])
    fits[key] = np.linalg.lstsq(A, np.asarray([c for _, c in s], float), rcond=None)[0]
allc = np.asarray([c for s in rings.values() for _, c in s] + apex_cols, float)
lo, hi = allc.min(0), allc.max(0)
apex_col = np.asarray(apex_cols, float).mean(0)


def dome_colour(q):
    row = np.asarray([1.0, math.cos(ang(q)), math.sin(ang(q))])
    cb, cm = row @ fits["b"], row @ fits["m"]
    if q.z <= z_m:
        t = (q.z - z_b) / (z_m - z_b)
        c = cb * (1 - t) + cm * t
    else:
        t = (q.z - z_m) / (z_t - z_m)
        c = cm * (1 - t) + apex_col * t
    return tuple(float(x) for x in np.clip(c, lo, hi))


# the cartridge's UVs: v climbs with height (piecewise, through the three rings), u is a
# plan projection (the texture is 2 texels wide, so u hardly matters)
uv_old = {v: next(lp[uvl].uv.copy() for lp in v.link_loops) for v in dv}
v_b = sum(uv_old[v].y for v in base) / len(base)
v_m = sum(uv_old[v].y for v in waist) / len(waist)
v_t = uv_old[apex].y
u_fit, _ = R.fit_uv(old, uvl, lambda co: (co.x, co.y, 1.0))


def dome_uv(v, p):
    if v in uv_old:
        return uv_old[v]
    if p.z <= z_m:
        vv = v_b + (v_m - v_b) * (p.z - z_b) / (z_m - z_b)
    else:
        vv = v_m + (v_t - v_m) * (p.z - z_m) / (z_t - z_m)
    return Vector((u_fit(p)[0], vv))


like = old[0]
d_tex, d_mat = like[tex], like.material_index
bmesh.ops.delete(dm, geom=old, context="FACES_ONLY")


def between(ring):
    """16 corners: the cartridge's 8 and one between each pair, on the circle (the mean
    radius and height of the pair, at the mean angle), in the circular frame."""
    outp = []
    for i, v in enumerate(ring):
        w = ring[(i + 1) % len(ring)]
        outp.append(v)
        qa, qb = fr[v], fr[w]
        a = ang(qa) + ((ang(qb) - ang(qa)) % (2 * math.pi)) * 0.5
        r = 0.5 * (rad(qa) + rad(qb))
        q = Vector((r * math.cos(a), r * math.sin(a), 0.5 * (qa.z + qb.z)))
        nv = dm.verts.new(from_f(q))
        fr[nv] = q
        outp.append(nv)
    return outp


ring_b = between(base)
ring_m = between(waist)
n_side = len(ring_b)


def mid_ring(lo_ring, hi_ring, z, bulge, r_ref):
    """A ring halfway up a band: at each column, the straight band's mid point pushed out
    by BULGE of the ellipse's bulge there (scaled to the column's size)."""
    outp = []
    for a, b in zip(lo_ring, hi_ring):
        qa, qb = fr[a], fr[b]
        theta = ang(qa)
        r = 0.5 * (rad(qa) + rad(qb)) + BULGE * bulge * rad(qa) / r_ref
        q = Vector((r * math.cos(theta), r * math.sin(theta), z))
        nv = dm.verts.new(from_f(q))
        fr[nv] = q
        outp.append(nv)
    return outp


ring_1 = mid_ring(ring_b, ring_m, z_1, bulge_1, rb)
ring_2 = mid_ring(ring_m, [apex] * n_side, z_2, bulge_2, rm)
centre = from_f(Vector((0.0, 0.0, e_z0)))
dome_faces = []


def dome_face(vs):
    f = dm.faces.new(vs)
    f[tex] = d_tex
    f.material_index = d_mat
    f.normal_update()
    if f.normal.dot(f.calc_center_median() - centre) < 0:
        f.normal_flip()
    for lp in f.loops:
        lp[uvl].uv = dome_uv(lp.vert, lp.vert.co)
        lp[col] = dome_colour(fr[lp.vert])
    dome_faces.append(f)


for lo_r, hi_r in ((ring_b, ring_1), (ring_1, ring_m), (ring_m, ring_2)):
    for i in range(n_side):
        j = (i + 1) % n_side
        dome_face([lo_r[i], lo_r[j], hi_r[j], hi_r[i]])
for i in range(n_side):
    dome_face([ring_2[i], ring_2[(i + 1) % n_side], apex])
report["dome_sides"] = n_side
report["dome_winding_fixed"] = R.agree_winding(dm, dome_faces)
report["dome_triangles"] = close_bm(dm, dome_ob)

# the arches must still stand clear of the rounded dome: from every dome corner under an
# arch's top, the arch's top is still above it
arch_tris = [[B.to_blender(c["p"]) for c in doc["tris"][i]["v"]] for i in range(77, 113)]
tree = BVHTree.FromPolygons([Vector(p) for t in arch_tris for p in t],
                            [(3 * i, 3 * i + 1, 3 * i + 2) for i in range(len(arch_tris))])
clear = []
poke = 0
for v in dome_ob.data.vertices:
    p = dome_ob.matrix_world @ v.co
    up = tree.ray_cast(p + Vector((0, 0, 1e-4)), Vector((0, 0, 1)))
    if up[0] is not None:
        clear.append(up[0].z - p.z)
    down = tree.ray_cast(p - Vector((0, 0, 1e-4)), Vector((0, 0, -1)))
    if down[0] is not None and up[0] is None and p.z > 0.2:
        poke += 1
report["dome_under_arch_min_clear_units"] = round(min(clear) * B.CELL, 1) if clear else None
report["dome_corners_through_arches"] = poke

report["fixed_triangles"] = sum(len(o.data.polygons) for o in objs)
report["ratio"] = round(report["fixed_triangles"] / float(report["cartridge_triangles"]), 2)

# ------------------------------------------------------------------ textures, 4x
# upscale.py --dir has already written <src_dir>/x4/ (Blender's Python has no PIL)
houses = {}
for t, info in doc["textures"].items():
    pair = {h: os.path.join(src, "x4", info[h]) for h in ("gdi", "nod")}
    if not all(os.path.exists(p) for p in pair.values()):
        raise SystemExit("run upscale.py --dir %s first" % src)
    houses[info["gdi"]] = pair
for img in list(bpy.data.images):
    if img.name in houses:
        img.filepath = houses[img.name]["gdi"]
        img.reload()
report["textures"] = "%d at 4x (both palettes)" % len(houses)

os.makedirs(out, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "SILO_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
