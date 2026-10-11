"""The Hand of Nod's repair plan: the cartridge model, completed and the globe rounded.

    blender -b --python tools/art/n64fix/fix_HAND.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/HAND_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building, which
shows a stone forearm rising out of a draped cloth, a fist on top gripping a globe, and
two small gabled huts at its foot with their far ends buried in the cloth:

  fist    (part 0) the wrist, hand and fingers are modelled on the south, the top and
          both flanks only: the back of the hand, from the wrist to the knuckles, is one
          open rim of 18 corners. Closed by a membrane over that rim (a constrained
          Delaunay fill of its outline seen from the north, interior depths relaxed to
          the average of their neighbours plus a small outward pressure, so the back is
          smooth and slightly convex), mapped by the fist's own front projection and
          shaded by the one light that best explains the fist's painted faces. A small
          open quad beside the thumb, mostly inside the globe, is closed too.
  forearm (part 0) a tapered 4-sided box with its north face left out. Added, mapped 0
          to 1 like its three other faces, its foot coloured by one fit round the other
          three feet and its top by their shared colour. It stays 4-sided: its base is
          a skewed kite, and rounding it pushes the base 60 to 120 model units into the
          cloth on every side.
  cloth   (part 0) the mound's north skirt, from the forearm's foot to the ground, was
          never modelled. Closed on the cartridge's own ground corners, coloured from the
          skirt corners it meets.
  huts    (part 0) the west hut's north wall was left out, and both huts' gable ends
          where their roofs run into the cloth are open. The wall is added in the plane
          of its roof's eave and cut where it enters the cloth, with a small wedge of
          cloth closing the gap between the two. It carries the south wall's windows at
          the south wall's texel density (the eave runs on 44% further, as plain wall),
          and the south wall's colours scaled by the cartridge's own north to south roof
          ratio (150 to 240). The two gables are mirror copies of the huts' other gables,
          with their UVs and their flat colour.
  globe   (part 1, animated) a sphere: 34 vertices all within one model unit of radius
          264.6, 64 triangles. Every triangle split in four, each new vertex pushed out
          onto the fitted sphere; UVs and the baked colours (which carry the land and
          sea tint) interpolate exactly along each cut. It stays in its rest pose.

Only the ground contact stays open: every hut's and the mound's bottom edge rests on
the ground.
"""
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Vector
from mathutils.geometry import delaunay_2d_cdt

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402
import repair as R  # noqa: E402

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
B.reset_scene()
doc, objs = B.build_cartridge(src, "HAND")
report = {"cartridge_triangles": sum(p["ntri"] for p in doc["parts"])}
U = 1.0 / B.CELL


def P(x, y, z):
    """A point in the pack frame (x east, y up, z south, model units) in Blender cells."""
    return Vector(B.to_blender((x, y, z)))


# ------------------------------------------------------------------ helpers
def face_uv_map(f, uvl):
    """The affine map from position to UV that a flat face's own corners state. Exact
    at the corners, so a point moved along the face's plane keeps the face's mapping."""
    pred, _ = R.fit_uv([f], uvl, lambda co: (co.x, co.y, co.z, 1.0))
    return pred


def boundary_loops(bm):
    """The open-edge loops of a mesh, each as an ordered list of vertices. A vertex
    where two loops touch is walked through on the first free edge."""
    seen, loops = set(), []
    for e0 in bm.edges:
        if not e0.is_boundary or e0 in seen:
            continue
        seen.add(e0)
        ring, cur = [e0.verts[0]], e0.verts[1]
        while cur is not ring[0]:
            ring.append(cur)
            nxt = [e for e in cur.link_edges if e.is_boundary and e not in seen]
            if not nxt:
                break
            seen.add(nxt[0])
            cur = nxt[0].other_vert(cur)
        loops.append(ring)
    return loops


def orient_to_neighbours(faces):
    """A patch of new faces, wound consistently among themselves, is flipped as a whole
    if most of the old faces it meets run the other way round. Returns True if flipped."""
    fs = set(faces)
    agree = disagree = 0
    for f in faces:
        for lp in f.loops:
            for other in lp.edge.link_faces:
                if other in fs:
                    continue
                ol = [l for l in lp.edge.link_loops if l.face is other][0]
                if ol.vert is lp.vert:
                    disagree += 1
                else:
                    agree += 1
    if disagree > agree:
        for f in faces:
            f.normal_flip()
        return True
    return False


def light_fit(faces, col):
    """Least-squares colour = a + b.normal over the given painted faces: the one light
    that best explains them. Returns colour(normal) clipped to the range they use."""
    X, Y = [], []
    for f in faces:
        for lp in f.loops:
            X.append((1.0, f.normal.x, f.normal.y, f.normal.z))
            Y.append(tuple(lp[col]))
    X, Y = np.asarray(X), np.asarray(Y, float)
    M = np.linalg.lstsq(X, Y, rcond=None)[0]
    lo, hi = Y.min(0), Y.max(0)

    def colour(n):
        c = np.clip(np.asarray((1.0, n.x, n.y, n.z)) @ M, lo, hi)
        return tuple(float(x) for x in c)
    return colour


def point_in_poly(p, poly):
    inside = False
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        if (a[1] > p[1]) != (b[1] > p[1]):
            x = a[0] + (p[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1])
            if x > p[0]:
                inside = not inside
    return inside


def dist_to_poly(p, poly):
    best = 1e9
    n = len(poly)
    for i in range(n):
        a, b = np.asarray(poly[i]), np.asarray(poly[(i + 1) % n])
        d = b - a
        t = np.clip(np.dot(np.asarray(p) - a, d) / max(np.dot(d, d), 1e-12), 0.0, 1.0)
        best = min(best, float(np.linalg.norm(np.asarray(p) - (a + t * d))))
    return best


def membrane_fill(bm, rim, to2d, from2d, spacing, pressure, iters=200):
    """Close an open rim with a smooth membrane. The rim is laid flat by to2d(co) ->
    (s, t, depth), interior points go on a grid of the given spacing inside it, a
    constrained Delaunay triangulation joins them, and each interior depth is relaxed
    to the mean of its neighbours plus `pressure` (a gentle outward bulge). from2d(s,
    t, depth) puts a point back. Returns the new faces, wound consistently."""
    flat = [to2d(v.co) for v in rim]
    poly = [(s, t) for s, t, _ in flat]
    lo = np.min(poly, 0)
    hi = np.max(poly, 0)
    inner = []
    ys = np.arange(lo[1] + 0.5 * spacing, hi[1], spacing)
    for j, t in enumerate(ys):
        off = 0.5 * spacing if j % 2 else 0.0
        for s in np.arange(lo[0] + 0.25 * spacing + off, hi[0], spacing):
            if point_in_poly((s, t), poly) and dist_to_poly((s, t), poly) > 0.45 * spacing:
                inner.append((float(s), float(t)))
    n_rim = len(poly)
    pts = [Vector((s, t)) for s, t in poly + inner]
    cdt = delaunay_2d_cdt(pts, [], [list(range(n_rim))], 1, 1e-9)
    vco, _, tris, orig, _, _ = cdt
    if len(vco) != len(pts):
        raise SystemExit("membrane: the rim crosses itself seen flat (%d -> %d points)"
                         % (len(pts), len(vco)))
    idx = [o[0] for o in orig]
    depth = [d for _, _, d in flat] + [float(np.mean([d for _, _, d in flat]))] * len(inner)
    nbr = [set() for _ in pts]
    for t in tris:
        a, b, c = (idx[i] for i in t)
        nbr[a] |= {b, c}
        nbr[b] |= {a, c}
        nbr[c] |= {a, b}
    for _ in range(iters):
        for i in range(n_rim, len(pts)):
            depth[i] = sum(depth[j] for j in nbr[i]) / len(nbr[i]) + pressure
    verts = list(rim) + [bm.verts.new(from2d(s, t, depth[n_rim + k]))
                         for k, (s, t) in enumerate(inner)]
    new = []
    for t in tris:
        new.append(bm.faces.new([verts[idx[i]] for i in t]))
    return new, len(inner)


# ------------------------------------------------------------------ part 0: the building
body = objs[0]
bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 * U)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")
bm.normal_update()

FIST, CLOTH, HUT, ARM, WIN, BACK, SKIRT, WALL, GABLE, WEDGE, THUMB = range(1, 12)
for f in bm.faces:
    f[part] = {55: FIST, 56: CLOTH, 57: HUT, 58: ARM, 59: WIN}[f[tex]]


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def find(x, y, z, tol=8.0):
    p = P(x, y, z)
    v = min(bm.verts, key=lambda w: (w.co - p).length)
    if (v.co - p).length > tol * U:
        raise SystemExit("no vertex at (%d, %d, %d)" % (x, y, z))
    return v


def tri_face(i):
    return next(f for f in bm.faces if f[tri] == i)


def new_face(vs, like, fpart, uv_of, colour_of):
    f = bm.faces.new(vs)
    f.material_index = like.material_index
    f[tex] = like[tex]
    f[part] = fpart
    for lp in f.loops:
        lp[uvl].uv = uv_of(lp.vert.co)
        lp[col] = colour_of(lp.vert.co)
    return f


def orient(f, want):
    f.normal_update()
    if f.normal.dot(want) < 0:
        f.normal_flip()


def vert_colour(v, parts):
    """The mean of the cartridge's colours at a vertex, over faces of the given parts."""
    cs = [tuple(lp[col]) for lp in v.link_loops if lp.face[part] in parts]
    return tuple(float(x) for x in np.mean(cs, 0))


added = []

# ---- huts: the west hut's north wall, cut where it enters the cloth
A = find(-736, 0, 436)            # north-west foot
Bt = find(-736, 354, 436)         # north-west eave
Ct = find(-273, 348, 308)         # the eave's east end, on the cloth
Mg = find(-585, 0, 334)           # the cloth's foot just north of the wall
d = Vector((Ct.co.x - A.co.x, Ct.co.y - A.co.y, 0.0))
eave_len = d.length
d.normalize()
north = Vector((-d.y, d.x, 0.0))
if north.y < 0:
    north = -north
# the foot of the cloth's edge Mg-Ct on the wall's plane: the wall stops there and a
# wedge of cloth closes the gap between it and the cloth's own edge
s_q = (Mg.co - A.co).dot(d)
Q = bm.verts.new((A.co.x + d.x * s_q, A.co.y + d.y * s_q, 0.0))
report["hut_wall_to_cloth_gap_units"] = round((Mg.co - Q.co).length * B.CELL, 1)
# the south wall: u 0 to 1 over 334 units of eave (0.93 over 306 at the foot), v 0.1 at
# the ground to 0.9 at the eave. The north wall runs on further before the cloth, so
# the windows keep their size and the rest samples the texture's plain east frame.
south_wall = [tri_face(153), tri_face(154)]
sw_top = [lp for f in south_wall for lp in f.loops if lp.vert.co.z > 0.2]
per_u = (find(-276, 348, 780).co - find(-592, 354, 887).co).length
eave_z = Bt.co.z
s_w = per_u
# the eave is one edge of the north roof slope: split it where the windows end, the
# new corner mapped and coloured as that roof face states
roof_f = next(f for f in bm.edges.get([Bt, Ct]).link_faces)
roof_uv = face_uv_map(roof_f, uvl)
roof_c = tuple(roof_f.loops[0][col])
_, W_t = bmesh.utils.edge_split(bm.edges.get([Bt, Ct]), Bt, s_w / eave_len)
W_t.co = Bt.co.lerp(Ct.co, s_w / eave_len)
for lp in W_t.link_loops:
    lp[uvl].uv = roof_uv(W_t.co)
    lp[col] = roof_c
# where the vertical under W_t meets the wall's slanted foot Q-Ct
t_w = (s_w - s_q) / (eave_len - s_q)
W_l = bm.verts.new(Q.co.lerp(Ct.co, t_w))
report["hut_wall_eave_units"] = round(eave_len * B.CELL, 1)
report["hut_wall_windows_units"] = round(s_w * B.CELL, 1)
ratio = 150.0 / 240.0        # the cartridge's north roof slope to its south one
foot_w = next(tuple(lp[col]) for lp in find(-592, 0, 887).link_loops if lp.face[tri] == 153)
foot_e = next(tuple(lp[col]) for lp in find(-303, 0, 788).link_loops if lp.face[tri] == 153)
top_c = tuple(sw_top[0][col])


def wall_uv(co):
    return ((co - A.co).dot(d) / per_u, 0.1 + 0.8 * co.z / eave_z)


def wall_col(co):
    s = min(max((co - A.co).dot(d) / per_u, 0.0), 1.0)
    h = min(max(co.z / eave_z, 0.0), 1.0)
    foot = [foot_w[k] * (1 - s) + foot_e[k] * s for k in range(4)]
    c = [foot[k] * (1 - h) + top_c[k] * h for k in range(4)]
    return (c[0] * ratio, c[1] * ratio, c[2] * ratio, 1.0)


def wall_uv_east(co):
    return (0.98, 0.1 + 0.8 * co.z / eave_z)


wall = [new_face([A, Q, W_l, W_t, Bt], south_wall[0], WALL, wall_uv, wall_col),
        new_face([W_l, Ct, W_t], south_wall[0], WALL, wall_uv_east, wall_col)]
for f in wall:
    orient(f, north)
cloth128 = tri_face(128)
w_uv = face_uv_map(cloth128, uvl)
w_c = tuple(cloth128.loops[0][col])
wedge = new_face([Mg, Q, W_l, Ct], cloth128, WEDGE, w_uv, lambda co: w_c)
orient(wedge, north)
added += wall + [wedge]

# ---- huts: the two gables where the roofs run into the cloth, mirror copies of the
# other gable of the same hut carrying its UVs and its flat colour. The west hut's gables
# pair north corner with north corner, the south-east hut's west corner with west corner.
for gable_tri, corners, side in (
        (144, [(-273, 348, 308), (-276, 348, 780), (-194, 456, 520)], lambda co: co.y),
        (145, [(19, 348, 939), (499, 307, 876), (268, 454, 893)], lambda co: co.x)):
    g = tri_face(gable_tri)
    eave_lp = sorted(g.loops, key=lambda lp: lp.vert.co.z)[:2]
    ridge_lp = max(g.loops, key=lambda lp: lp.vert.co.z)
    vs = [find(*c) for c in corners]
    src_sorted = sorted(eave_lp, key=lambda lp: side(lp.vert.co))
    new_sorted = sorted(vs[:2], key=lambda v: side(v.co))
    pairs = dict(zip(new_sorted, src_sorted))
    pairs[vs[2]] = ridge_lp
    gf = bm.faces.new(vs)
    gf.material_index = g.material_index
    gf[tex] = g[tex]
    gf[part] = GABLE
    for lp in gf.loops:
        lp[uvl].uv = pairs[lp.vert][uvl].uv
        lp[col] = pairs[lp.vert][col]
    orient_to_neighbours([gf])
    added.append(gf)

# ---- forearm: its north face
SWb, SEb, NEb, NWb = find(-273, 348, 308), find(180, 533, 458), find(301, 348, -89), find(-265, 348, 19)
SWt, SEt, NEt, NWt = find(-207, 1092, 113), find(43, 1092, 129), find(208, 1092, -155), find(-224, 1092, -54)
arm = faces_of(ARM)
arm_top = float(np.mean([lp[col][0] for f in arm for lp in f.loops if lp.vert.co.z > 1.0]))
# the three painted feet round the arm, fitted as a0 + a1 cos + b1 sin of their facing
A_ = [(1.0, f.normal.x, f.normal.y) for f in arm for lp in f.loops if lp.vert.co.z < 0.6]
Y_ = [tuple(lp[col]) for f in arm for lp in f.loops if lp.vert.co.z < 0.6]
coef = np.linalg.lstsq(np.asarray(A_), np.asarray(Y_, float), rcond=None)[0]
fn = Vector(((NWb.co - NEb.co).cross(NEt.co - NEb.co))).normalized()
if fn.y < 0:
    fn = -fn
foot = np.clip(np.asarray((1.0, fn.x, fn.y)) @ coef, np.min(Y_, 0), np.max(Y_, 0))
report["forearm_north_foot_grey"] = int(round(foot[0] * 255))
arm_uv = {NEb: (0.0, 0.0), NWb: (1.0, 0.0), NWt: (1.0, 1.0), NEt: (0.0, 1.0)}
top_rgba = (arm_top, arm_top, arm_top, 1.0)
arm_n = new_face([NEb, NWb, NWt, NEt], arm[0], ARM, lambda co: (0, 0),
                 lambda co: top_rgba if co.z > 1.0 else tuple(float(x) for x in foot))
for lp in arm_n.loops:
    lp[uvl].uv = arm_uv[lp.vert]
orient(arm_n, fn)
added.append(arm_n)

# ---- cloth: the north skirt from the forearm's foot to the cartridge's ground corners
G1, G2, T = find(-444, 0, -58), find(738, 0, -18), find(553, 348, 48)
skirt_like = tri_face(124)
sk_col = {v: tuple(0.85 * c for c in vert_colour(v, (CLOTH,))[:3]) + (1.0,)
          for v in (G1, G2, T, NEb, NWb)}
skirt = []
for q in ([G1, NWb, NEb], [G1, NEb, G2], [NEb, T, G2]):
    f = new_face(q, skirt_like, SKIRT, lambda co: (co.x * 1.4, co.z * 1.4),
                 lambda co: (0, 0, 0, 1))
    for lp in f.loops:
        lp[col] = sk_col[lp.vert]
    orient(f, Vector((0.0, 1.0, 0.0)))
    skirt.append(f)
added += skirt

# ---- fist: the small open quad beside the thumb
thumb_rim = [find(-346, 1544, 163), find(-237, 1479, 183), find(-191, 1603, 167), find(-218, 1641, 128)]
near_thumb = list({f for v in thumb_rim for f in v.link_faces})
th_uv, th_rms = R.fit_uv(near_thumb, uvl, lambda co: (co.x, co.y, co.z, 1.0))
report["thumb_uv_fit_rms"] = round(th_rms, 4)
th = new_face(thumb_rim, near_thumb[0], THUMB, th_uv, lambda co: (0, 0, 0, 1))
for lp in th.loops:
    lp[col] = vert_colour(lp.vert, (FIST,))
orient_to_neighbours([th])
added.append(th)

# ---- fist: the back of the hand
loops = boundary_loops(bm)
rim = max(loops, key=lambda r: min(v.co.z for v in r))
report["fist_rim_corners"] = len(rim)
if min(v.co.z for v in rim) < 1.0:
    raise SystemExit("fist: expected its back rim above the forearm")
fist = faces_of(FIST)
# laid flat as seen from the north: s east to west, t up, depth north
back_uv, back_rms = R.fit_uv(fist, uvl, lambda co: (co.x, co.z, 1.0))
report["fist_front_projection_rms"] = round(back_rms, 4)
fist_light = light_fit(fist, col)
membrane, n_inner = membrane_fill(
    bm, rim, lambda co: (co.x, co.z, co.y), lambda s, t, dp: (s, dp, t),
    spacing=160.0 * U, pressure=12.0 * U)
for f in membrane:
    f[part] = BACK
    f[tex] = fist[0][tex]
    f.material_index = fist[0].material_index
report["fist_back_flipped"] = orient_to_neighbours(membrane)
report["fist_back_interior_points"] = n_inner
report["fist_back_triangles"] = len(membrane)
bm.normal_update()
# shaded per corner by the smoothed normal of the new back, from one light fitted to
# the fist's painted faces
back_set = set(membrane)
for f in membrane:
    for lp in f.loops:
        n = Vector()
        for g in lp.vert.link_faces:
            if g in back_set:
                n += g.normal * g.calc_area()
        lp[col] = fist_light(n.normalized())
        lp[uvl].uv = back_uv(lp.vert.co)
report["fist_back_grey_range"] = [
    int(round(255 * min(lp[col][0] for f in membrane for lp in f.loops))),
    int(round(255 * max(lp[col][0] for f in membrane for lp in f.loops)))]
added += membrane

report["winding_fixed"] = R.agree_winding(bm, added)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["building_triangles"] = len(bm.faces)
report["building_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
report["building_open_above_ground"] = sum(
    1 for e in bm.edges if e.is_boundary and max(v.co.z for v in e.verts) > 1e-4)
bm.to_mesh(body.data)
bm.free()
body.data.update()

# ------------------------------------------------------------------ part 1: the globe
globe = objs[1]
bm = bmesh.new()
bm.from_mesh(globe.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 * U)
Pts = np.asarray([tuple(v.co) for v in bm.verts])
Am = np.c_[2 * Pts, np.ones(len(Pts))]
sol = np.linalg.lstsq(Am, (Pts ** 2).sum(1), rcond=None)[0]
gc = Vector(sol[:3])
gr = math.sqrt(sol[3] + gc.length_squared)
miss = max(abs((v.co - gc).length - gr) for v in bm.verts)
report["globe_radius_units"] = round(gr * B.CELL, 1)
report["globe_sphere_miss_units"] = round(miss * B.CELL, 2)
report["globe_triangles_before"] = len(bm.faces)
res = bmesh.ops.subdivide_edges(bm, edges=list(bm.edges), cuts=1, use_grid_fill=True,
                                use_only_quads=False, use_single_edge=False)
for v in [g for g in res["geom_inner"] if isinstance(g, bmesh.types.BMVert)]:
    v.co = gc + (v.co - gc).normalized() * gr
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["globe_triangles"] = len(bm.faces)
report["globe_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
bm.to_mesh(globe.data)
bm.free()
globe.data.update()

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "HAND_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
