"""The Hospital's repair plan: the cartridge model, completed.

    blender -b --python tools/art/n64fix/fix_HOSP.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/HOSP_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building: a
cross-shaped main block with a cross on the roof at the crossing, a low L-shaped annex
along the south and east, a pipe out of the south arm down onto the annex roof, and a
small unit on the west arm's roof.

  cross   two boxes with sloped sides crossing at full height (0.471 cells): an east-west
          bar and a north-south bar. The east-west bar's north side and the north-south
          bar's north end were never modelled. Each is its south counterpart mirrored
          north to south, carrying its UVs, onto the corners the end walls already state
          (the north-south bar's north foot sits 36 units further out than a true mirror,
          so the copy snaps to the cartridge's own corners).
  annex   an L at 0.34 cells, open along its courtyard side. Closed with one wall on the
          corners the west end and the arm's inner wall state, mapped at the south wall's
          own texel rate. The arm's north end stays open: it lies wholly inside the
          east-west bar's south wall.
  shade   the walls are painted flat per face as if lit from the south-west (south 169 to
          210, west 150, east 100). A new wall takes one light fitted to every painted
          wall, held inside the painted range, which gives the north faces the east
          faces' 100.
  vent    a regular pentagonal frustum on the west arm (radii 0.090 to 0.099), the
          console's cheap round, with its north side missing. The side one step round
          (the east one) is turned 72 degrees onto the gap, carrying its UVs; then the
          whole vent goes to 10 sides and is re-shaded by one fit round its axis.
  pipe    a two-faced wedge, the top of a round pipe (the arc runs 25 to 155 degrees at
          the north end), with no underside. Each face is split along its length onto the
          circle each end states, so the arc has four facets, shaded across the arc from
          the east face's colour to the west face's; the underside is closed. Both ends
          stay open: they are buried in the south arm's end and in the annex roof.
  sign    the cross on the roof is a cutout plate floating 0.038 cells above the roof,
          seen from the side as a sheet over a gap. Rebuilt as a solid cross: the plate
          trimmed to the cross its texture's alpha draws, and sides down to the roof that
          sample the texture's own grey border.

Nothing the Remastered shows that the cartridge lacks (the outer pipes, the fan, the
terraced sides) is added.
"""
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402
import repair as R  # noqa: E402

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
B.reset_scene()
doc, objs = B.build_cartridge(src, "HOSP")
body = objs[0]
report = {"cartridge_triangles": len(doc["tris"]),
          "cartridge_drawn": sum(len(o.data.polygons) for o in objs)}

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
bm.normal_update()
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

SIGN, WALL, TOP, PIPE, VENT, VENT_TOP, UNDER, RIM = range(1, 9)
BY_TRI = {0: SIGN, 1: SIGN}
for i in range(2, 22):
    BY_TRI[i] = WALL
for ids, p in (((25, 26, 27, 28, 31, 40, 41, 42), TOP), ((22, 23, 24, 44), PIPE),
               ((29, 30, 34), VENT_TOP), ((32, 33, 35, 36, 37, 38, 39, 43), VENT)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI[f[tri]]
SC = 1.0 / B.CELL


# ------------------------------------------------------------------ helpers
def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def faces_tri(*ids):
    return [f for f in bm.faces if f.is_valid and f[tri] in ids]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def at(p, pool=None, tol=3.0):
    """The vertex at a position (Blender cells), to within `tol` model units."""
    pool = pool if pool is not None else bm.verts
    v = min(pool, key=lambda w: (w.co - p).length)
    if (v.co - p).length > tol * SC:
        raise SystemExit("no vertex near %s" % (tuple(round(c, 3) for c in p),))
    return v


def mean_colour(faces):
    s = np.zeros(4)
    n = 0
    for f in faces:
        for lp in f.loops:
            s += np.asarray(tuple(lp[col]))
            n += 1
    return tuple(float(x) for x in s / max(n, 1))


def new_face(vs, like, prt, uvs=None, colour=None, facing=None):
    """A face on `vs` with `like`'s material and texture (a face, or a (material slot,
    pack texture) pair). `uvs` one per corner, `colour` one value for all corners,
    `facing` a direction its normal must agree with."""
    f = bm.faces.new(vs)
    f.material_index, f[tex] = like if isinstance(like, tuple) else (like.material_index, like[tex])
    f[part] = prt
    f[tri] = -1
    for i, lp in enumerate(f.loops):
        if uvs is not None:
            lp[uvl].uv = uvs[i]
        if colour is not None:
            lp[col] = colour
    if facing is not None:
        f.normal_update()
        if f.normal.dot(facing) < 0:
            f.normal_flip()
    return f


def copy_snap(faces, fmap, candidates, snap, reverse):
    """Copy `faces` through the position map `fmap` (reversed in winding when the map
    is a mirror). Each mapped corner lands on the nearest of `candidates` within `snap`
    model units, otherwise on a new vertex. UVs and colours are carried from the source
    corner. Returns (new faces, worst snap in model units, vertices created)."""
    made, pool = {}, list(candidates)
    worst, created = 0.0, 0

    def vert_for(v):
        nonlocal worst, created
        if v not in made:
            p = fmap(v.co)
            best = min(pool, key=lambda w: (w.co - p).length) if pool else None
            if best is not None and (best.co - p).length <= snap * SC:
                worst = max(worst, (best.co - p).length * B.CELL)
                made[v] = best
            else:
                made[v] = bm.verts.new(p)
                pool.append(made[v])
                created += 1
        return made[v]

    new = []
    for f in faces:
        loops = list(reversed(f.loops)) if reverse else list(f.loops)
        nf = bm.faces.new([vert_for(lp.vert) for lp in loops])
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        nf[part] = f[part]
        nf[tri] = -1
        for lo, ln in zip(loops, nf.loops):
            ln[uvl].uv = lo[uvl].uv
            ln[col] = lo[col]
        new.append(nf)
    return new, worst, created


def light_fit(faces):
    """Least-squares colour = a + b.normal over flat-painted faces: the one light that
    best explains them. Returns colour(normal) held inside the painted range, and rms."""
    X, Y = [], []
    for f in faces:
        for lp in f.loops:
            X.append((1.0, f.normal.x, f.normal.y, f.normal.z))
            Y.append(tuple(lp[col]))
    X, Y = np.asarray(X), np.asarray(Y, float)
    M = np.linalg.lstsq(X, Y, rcond=None)[0]
    rms = float(np.sqrt(((X @ M - Y) ** 2).sum(1).mean()))
    lo, hi = Y.min(0), Y.max(0)

    def colour(n):
        c = np.clip(np.asarray((1.0, n.x, n.y, n.z)) @ M, lo, hi)
        return tuple(float(x) for x in c)
    return colour, rms


def circumcentre(a, b, c):
    """Centre and radius of the circle through three points in space."""
    ab, ac = b - a, c - a
    n = ab.cross(ac)
    o = a + (n.cross(ab) * ac.length_squared + ac.cross(n) * ab.length_squared) \
        / (2.0 * n.length_squared)
    return o, (a - o).length


def on_arc(p, q, o, r):
    """The point halfway between p and q on the circle (o, r) they both lie on."""
    m = 0.5 * (p + q)
    return o + (m - o).normalized() * r


def winding_errors():
    bad = 0
    for e in bm.edges:
        if len(e.link_faces) != 2:
            continue
        f0, f1 = e.link_faces
        d0 = [lp for lp in e.link_loops if lp.face is f0][0].vert
        d1 = [lp for lp in e.link_loops if lp.face is f1][0].vert
        bad += d0 is d1
    return bad


walls = faces_of(WALL)
wall_like = (walls[0].material_index, walls[0][tex])
wall_light, wall_rms = light_fit(walls)
report["wall_light_fit_rms"] = round(wall_rms * 255, 1)
report["north_wall_grey"] = round(wall_light(Vector((0.0, 1.0, 0.0)))[0] * 255, 1)
added = []

# ------------------------------------------------------------------ cross: north faces
# the east-west bar's north side: its south side (tris 20, 21) mirrored about the bar's
# own centre line, which the top states
ew_top = verts_of(faces_tri(25, 28))
y_ew = 0.5 * (min(v.co.y for v in ew_top) + max(v.co.y for v in ew_top))
new, worst, made = copy_snap(faces_tri(20, 21), lambda co: Vector((co.x, 2 * y_ew - co.y, co.z)),
                             verts_of(walls), 12.0, True)
report["ew_north_snap_units"] = round(worst, 1)
report["ew_north_new_verts"] = made
added += new
# the north-south bar's north end: its south end (tris 15, 16) mirrored about the top's
# centre line; the north foot is 36 units further out than a true mirror, so the snap is
# wide (no two of the bar's corners are closer than 280 units)
ns_top = verts_of(faces_tri(27, 31))
y_ns = 0.5 * (min(v.co.y for v in ns_top) + max(v.co.y for v in ns_top))
new, worst, made = copy_snap(faces_tri(15, 16), lambda co: Vector((co.x, 2 * y_ns - co.y, co.z)),
                             verts_of(walls), 48.0, True)
report["ns_north_snap_units"] = round(worst, 1)
report["ns_north_new_verts"] = made
added += new
for f in added:
    f.normal_update()
    c = wall_light(f.normal)
    for lp in f.loops:
        lp[col] = c

# ------------------------------------------------------------------ annex: courtyard wall
a_tw = at(Vector((-0.608, -0.730, 0.340)))
a_te = at(Vector((0.343, -0.730, 0.340)))
a_fe = at(Vector((0.298, -0.729, 0.003)))
a_fw = at(Vector((-0.696, -0.767, 0.003)))
# the south wall (tris 5, 7) runs u from its west end to its east end along the top and
# along the foot; the courtyard wall takes the same u at the same x, edge by edge
south = faces_tri(5, 7)
sv = {}
for f in south:
    for lp in f.loops:
        sv[lp.vert] = tuple(lp[uvl].uv)
s_top = sorted([v for v in sv if v.co.z > 0.2], key=lambda v: v.co.x)
s_foot = sorted([v for v in sv if v.co.z < 0.2], key=lambda v: v.co.x)


def u_along(edge, x):
    a, b = edge[0], edge[-1]
    t = (x - a.co.x) / (b.co.x - a.co.x)
    return sv[a][0] + t * (sv[b][0] - sv[a][0])


cw_uv = [(u_along(s_top, a_tw.co.x), sv[s_top[0]][1]), (u_along(s_foot, a_fw.co.x), sv[s_foot[0]][1]),
         (u_along(s_foot, a_fe.co.x), sv[s_foot[0]][1]), (u_along(s_top, a_te.co.x), sv[s_top[0]][1])]
cw = new_face([a_tw, a_fw, a_fe, a_te], wall_like, WALL, uvs=cw_uv, facing=Vector((0, 1, 0.2)))
cw.normal_update()
for lp in cw.loops:
    lp[col] = wall_light(cw.normal)
added.append(cw)

# ------------------------------------------------------------------ vent: north side, 10 sides
vent = faces_of(VENT)
base = [v for v in verts_of(vent) if v.co.z < 0.47]
vc = R.fit_circle([(v.co.x, v.co.y) for v in base])
report["vent_axis"] = [round(x, 4) for x in vc]
report["vent_base_radii_units"] = sorted(round(R.radius_of(v.co, vc) * B.CELL) for v in base)
vent_shade = R.smooth_ring_shade(vent, col, vc)
rot = Matrix.Rotation(math.radians(72.0), 3, "Z")
axis = Vector((vc[0], vc[1], 0.0))
east = faces_tri(38, 39)
new, worst, made = copy_snap(east, lambda co: rot @ (co - axis) + axis,
                             verts_of(faces_of(VENT, VENT_TOP)), 20.0, False)
report["vent_north_snap_units"] = round(worst, 1)
report["vent_north_new_verts"] = made
added += new
R.densify_curve(bm, faces_of(VENT, VENT_TOP), vc, max_deg=90.0)
R.apply_colour(faces_of(VENT), col, vent_shade)
report["vent_sides"] = len({v for f in faces_of(VENT_TOP) for v in f.verts})

# ------------------------------------------------------------------ pipe: four facets and an underside
pv = verts_of(faces_of(PIPE))
r_n = max(pv, key=lambda v: v.co.z)
r_s = min(pv, key=lambda v: v.co.y)
north_end = sorted([v for v in pv if v.co.y > -0.7 and v is not r_n], key=lambda v: v.co.x)
south_end = sorted([v for v in pv if v.co.y < -0.8 and v is not r_s], key=lambda v: v.co.x)
w_n, e_n = north_end
w_s, e_s = south_end
pipe_old = faces_of(PIPE)
e_face = [f for f in pipe_old if e_n in f.verts or e_s in f.verts]
w_face = [f for f in pipe_old if w_n in f.verts or w_s in f.verts]
c_e = np.asarray(mean_colour(e_face))
c_w = np.asarray(mean_colour(w_face))
n_e = sum((f.normal for f in e_face), Vector()).normalized()
n_w = sum((f.normal for f in w_face), Vector()).normalized()
puv = {}
for f in pipe_old:
    for lp in f.loops:
        puv[lp.vert] = Vector(tuple(lp[uvl].uv))
pipe_like = (pipe_old[0].material_index, pipe_old[0][tex])
o_n, rad_n = circumcentre(e_n.co, r_n.co, w_n.co)
o_s, rad_s = circumcentre(e_s.co, r_s.co, w_s.co)
report["pipe_radius_units"] = [round(rad_n * B.CELL), round(rad_s * B.CELL)]
ends = {}
for key, (e, r, w, o, rad) in (("n", (e_n, r_n, w_n, o_n, rad_n)), ("s", (e_s, r_s, w_s, o_s, rad_s))):
    me = bm.verts.new(on_arc(e.co, r.co, o, rad))
    mw = bm.verts.new(on_arc(r.co, w.co, o, rad))
    puv[me] = 0.5 * (puv[e] + puv[r])
    puv[mw] = 0.5 * (puv[r] + puv[w])
    ends[key] = [e, me, r, mw, w]
bmesh.ops.delete(bm, geom=pipe_old, context="FACES_ONLY")


def pipe_colour(t):
    """Across the arc, east foot 0 to west foot 1: the east face's colour over the east
    quarter, the west face's over the west quarter, graded between them."""
    k = min(max((t - 0.25) / 0.5, 0.0), 1.0)
    return tuple(float(x) for x in c_e * (1 - k) + c_w * k)


pipe_new = []
for i in range(4):
    a_s, b_s = ends["s"][i], ends["s"][i + 1]
    a_n, b_n = ends["n"][i], ends["n"][i + 1]
    f = new_face([a_s, b_s, b_n, a_n], pipe_like, PIPE, uvs=[puv[v] for v in (a_s, b_s, b_n, a_n)],
                 facing=n_e if i < 2 else n_w)
    for lp in f.loops:
        j = (ends["s"] if lp.vert in ends["s"] else ends["n"]).index(lp.vert)
        lp[col] = pipe_colour(j / 4.0)
    pipe_new.append(f)
pipe_uv, pipe_rms = R.fit_uv(pipe_new, uvl, lambda co: (co.x, co.y, 1.0))
dark = tuple(float(x) for x in 0.55 * (0.5 * (c_e + c_w)))
under = new_face([w_n, e_n, e_s, w_s], pipe_like, UNDER, uvs=[pipe_uv(v.co) for v in (w_n, e_n, e_s, w_s)],
                 colour=dark[:3] + (1.0,), facing=Vector((0, 0, -1)))
report["pipe_underside_uv_rms"] = round(pipe_rms, 4)
added += pipe_new + [under]

# ------------------------------------------------------------------ sign: a solid cross
plate = faces_of(SIGN)
plate_like = (plate[0].material_index, plate[0][tex])
z_sign = plate[0].verts[0].co.z
z_roof = max(v.co.z for f in faces_of(TOP) for v in f.verts)
plate_col = mean_colour(plate)
# the plate's planar map, and its inverse (UV to position)
X = np.asarray([(lp.vert.co.x, lp.vert.co.y, 1.0) for f in plate for lp in f.loops])
Y = np.asarray([tuple(lp[uvl].uv) for f in plate for lp in f.loops])
to_uv = np.linalg.lstsq(X, Y, rcond=None)[0]
to_xy = np.linalg.lstsq(np.c_[Y, np.ones(len(Y))], X[:, :2], rcond=None)[0]
report["sign_uv_fit_rms"] = round(float(np.sqrt(((X @ to_uv - Y) ** 2).sum(1).mean())), 5)
# the cross the alpha draws: texels 5 to 10 of 16 in each direction, full width. The
# outline sits a tenth of a texel inside the alpha edge so the alpha test never trims it.
TX = 1.0 / 16.0
eps = 0.1 * TX
lo, hi, e0, e1 = 5 * TX + eps, 11 * TX - eps, eps, 1.0 - eps
outline_uv = [(lo, e0), (hi, e0), (hi, lo), (e1, lo), (e1, hi), (hi, hi),
              (hi, e1), (lo, e1), (lo, hi), (e0, hi), (e0, lo), (lo, lo)]


def xy_of(uv):
    x, y = np.asarray((uv[0], uv[1], 1.0)) @ to_xy
    return float(x), float(y)


top_v = {uv: bm.verts.new((*xy_of(uv), z_sign)) for uv in outline_uv}
bot_v = {uv: bm.verts.new((*xy_of(uv), z_roof)) for uv in outline_uv}
up = Vector((0, 0, 1))
sign_new = []
for quad in (((lo, lo), (hi, lo), (hi, hi), (lo, hi)),
             ((lo, e0), (hi, e0), (hi, lo), (lo, lo)),
             ((hi, lo), (e1, lo), (e1, hi), (hi, hi)),
             ((lo, hi), (hi, hi), (hi, e1), (lo, e1)),
             ((e0, lo), (lo, lo), (lo, hi), (e0, hi))):
    sign_new.append(new_face([top_v[q] for q in quad], plate_like, SIGN, uvs=list(quad),
                             colour=plate_col, facing=up))


def in_cross(u, v):
    return (lo <= u <= hi and e0 <= v <= e1) or (lo <= v <= hi and e0 <= u <= e1)


for i, a in enumerate(outline_uv):
    b = outline_uv[(i + 1) % len(outline_uv)]
    du, dv = b[0] - a[0], b[1] - a[1]
    ln = math.hypot(du, dv)
    nu, nv = dv / ln, -du / ln          # one of the two normals in the texture's plane
    mu, mv = 0.5 * (a[0] + b[0]), 0.5 * (a[1] + b[1])
    if in_cross(mu + 0.01 * nu, mv + 0.01 * nv):
        nu, nv = -nu, -nv               # now pointing out of the cross
    # the side samples the grey border texel just inside its own edge
    ua = (a[0] - 0.5 * TX * nu, a[1] - 0.5 * TX * nv)
    ub = (b[0] - 0.5 * TX * nu, b[1] - 0.5 * TX * nv)
    ox, oy = xy_of((mu + nu, mv + nv))
    cx, cy = xy_of((mu, mv))
    out_dir = Vector((ox - cx, oy - cy, 0.0)).normalized()
    f = new_face([top_v[a], top_v[b], bot_v[b], bot_v[a]], plate_like, RIM,
                 uvs=[ua, ub, ub, ua], facing=out_dir)
    # shaded by facing, with the walls' own light
    c = wall_light(out_dir)
    for lp in f.loops:
        lp[col] = c
    sign_new.append(f)
plate_verts = verts_of(plate)
bmesh.ops.delete(bm, geom=plate, context="FACES_ONLY")
bmesh.ops.delete(bm, geom=[v for v in plate_verts if v.is_valid and not v.link_faces], context="VERTS")
added += sign_new
report["sign_height_units"] = round((z_sign - z_roof) * B.CELL, 1)

# ------------------------------------------------------------------ finish
report["winding_fixed"] = R.agree_winding(bm, [f for f in added if f.is_valid])
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["inconsistent_winding_edges"] = winding_errors()
report["fixed_triangles"] = len(bm.faces)
report["ratio"] = round(len(bm.faces) / report["cartridge_drawn"], 2)
report["open_edges_after"] = sum(1 for e in bm.edges if e.is_boundary)
bm.to_mesh(body.data)
bm.free()
body.data.update()

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "HOSP_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
