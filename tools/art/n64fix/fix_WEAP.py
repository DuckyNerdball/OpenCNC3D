"""The Weapons Factory's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_WEAP.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/WEAP_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, islands.py) and checked against the 1995 sprite and its
construction and door animations, and the Remastered sprite sheet: a hall with sloped
sides under a roof block with four round fittings, a hazard-striped door facing south
that splits along a jagged seam, a low annex on the east carrying two round smokestacks
with ring bands, all on a concrete slab.

The model is four parts. Part 1 (the east stack) sways and parts 2 and 3 (the upper and
lower door halves) slide up into the roof and down into the ground in the building's
animation, so every part stays its own object with its pack_part and pivot, and every
repair is made in the rest pose.

  hall    a frustum whose west, north and east walls are modelled on the outside only,
          and whose south side is the doorway. While the door is open the camera looks
          into the hall, where the cartridge draws only a floor: the walls get an inner
          lining 16 model units inside them, the doorway jambs and the tops of the walls
          are closed between the two, and the floor gets a riser along its front edge
          down to the slab. Both sprites show the inside dark.
  roof    the roof block has no north face and no underside: both added, the north face
          mapped like the south one and shaded from one fit of the block's painted
          sides. The four roof fittings are five-sided prisms with their two north sides
          left out: closed, doubled to ten sides (the Remastered fittings are round) and
          re-shaded as cylinders from one fit of each fitting's painted sides.
  lintel  the beam over the door has no back and no underside: closed with its hull.
  stacks  (one in part 0, the east one is part 1) six-sided tapered stacks with only
          their three south sides modelled. The six sides are centrally symmetric (the
          opposite corners' midpoints agree to about 1 model unit), so the north half is
          the south half turned 180 degrees about the stack's axis, its texture carried
          on round the stack so the bands meet with no seam; then doubled twice to 24
          sides (both sprites show round stacks) and the tops re-mapped by their own
          planar projection.
  annex   two stacked blocks against the hall's east wall with no north faces and no
          west faces (the west faces run into the hall wall, but the upper block's west
          end reaches through where the lining now is): closed, the new faces mapped
          like the south faces they mirror and shaded like the darker modelled side. The
          upper block's overhang gets an underside. The ramp round its foot gets its
          north end.
  slab    the north skirt was left out: added, mapped the way the cartridge maps its
          skirts and shaded from one fit of the painted skirts.
  door    (parts 2 and 3) two thick panels meeting along a jagged seam, modelled on
          every side but the back, which faces into the hall: each back is the front's
          own triangles carried onto the back corners the cartridge already has, darker.
"""
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402
import repair as R  # noqa: E402

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
B.reset_scene()
doc, objs = B.build_cartridge(src, "WEAP")
report = {"cartridge_triangles": sum(p["ntri"] for p in doc["parts"]),
          "cartridge_drawn": sum(len(o.data.polygons) for o in objs)}
U = 1.0 / B.CELL               # one model unit in Blender cells


# ------------------------------------------------------------------ helpers
def open_bm(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 * U)
    bm.normal_update()
    return bm


def close_bm(bm, ob):
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    bm.normal_update()
    n = len(bm.faces)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return n


def at(bm, x, y, z, tol=4.0):
    """The vertex at a position given in model units (x east, y north, z up)."""
    p = Vector((x, y, z)) * U
    v = min(bm.verts, key=lambda w: (w.co - p).length)
    if (v.co - p).length > tol * U:
        raise SystemExit("no vertex near (%g, %g, %g)" % (x, y, z))
    return v


def at_or_new(bm, p, tol=3.0):
    v = min(bm.verts, key=lambda w: (w.co - p).length)
    return v if (v.co - p).length <= tol * U else bm.verts.new(p)


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def loop_at(f, v):
    return next(lp for lp in f.loops if lp.vert is v)


def mean_colour(bm, faces):
    col = bm.loops.layers.color.get("Col")
    s = [0.0] * 4
    n = 0
    for f in faces:
        for lp in f.loops:
            for i in range(4):
                s[i] += lp[col][i]
            n += 1
    return tuple(x / max(n, 1) for x in s)


def scaled(c, k):
    return (c[0] * k, c[1] * k, c[2] * k, c[3])


def new_face(bm, vs, like, uv_of, colour, fpart=None, partl=None):
    """A face through `vs`, textured like `like` (a face, or (pack_tex, material slot));
    uv_of(vert) and colour(vert) (or one colour for all corners) give its corners."""
    uvl, col, tex = R.layers(bm)
    f = bm.faces.new(vs)
    if isinstance(like, tuple):
        f[tex], f.material_index = like
    else:
        f[tex] = like[tex]
        f.material_index = like.material_index
    if partl is not None:
        f[partl] = fpart
    for lp in f.loops:
        lp[uvl].uv = uv_of(lp.vert)
        lp[col] = colour(lp.vert) if callable(colour) else colour
    return f


def face_toward(f, d):
    f.normal_update()
    if f.normal.dot(d) < 0:
        f.normal_flip()


def light_fit(bm, faces):
    """Least-squares colour = a + b.normal over flat-painted faces: the one light that
    best explains a box's painted sides. Returns colour(normal), rms. As in fix_BIO.py."""
    col = bm.loops.layers.color.get("Col")
    X, Y = [], []
    for f in faces:
        for lp in f.loops:
            X.append((1.0, f.normal.x, f.normal.y, f.normal.z))
            Y.append(tuple(lp[col]))
    X, Y = np.asarray(X), np.asarray(Y, float)
    M = np.linalg.lstsq(X, Y, rcond=None)[0]
    rms = float(np.sqrt(((X @ M - Y) ** 2).sum(1).mean()))

    def colour(n):
        c = np.clip(np.asarray((1.0, n.x, n.y, n.z)) @ M, 0.0, 1.0)
        return tuple(float(x) for x in c[:3]) + (1.0,)
    return colour, rms


def islands(faces):
    left = set(faces)
    groups = []
    while left:
        st = [left.pop()]
        g = []
        while st:
            f = st.pop()
            g.append(f)
            for e in f.edges:
                for n in e.link_faces:
                    if n in left:
                        left.remove(n)
                        st.append(n)
        groups.append(g)
    return groups


def winding_errors(bm):
    bad = 0
    for e in bm.edges:
        if len(e.link_faces) != 2:
            continue
        f0, f1 = e.link_faces
        d0 = [lp for lp in e.link_loops if lp.face is f0][0].vert
        d1 = [lp for lp in e.link_loops if lp.face is f1][0].vert
        bad += d0 is d1
    return bad


def back_shell(bm, faces, move, shade=0.6):
    """A reversed copy of `faces` whose corners go through move(vert) -> position (None
    keeps the vertex). UVs carried, colour the source's times `shade`. As in fix_EYE.py;
    a moved corner that lands on an existing vertex is welded by the final weld."""
    uvl, col, tex = R.layers(bm)
    made = {}
    new = []
    for f in faces:
        vs = []
        for v in reversed(f.verts):
            if v not in made:
                p = move(v)
                made[v] = v if p is None else bm.verts.new(p)
            vs.append(made[v])
        nf = bm.faces.new(vs)
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo[uvl].uv
            c = lo[col]
            ln[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
        new.append(nf)
    return new, made


def half_turn(bm, faces, centre, snap=5.0, du=0.0, extra=()):
    """Complete a centrally symmetric round part whose south half is modelled: each face
    turned 180 degrees about the vertical axis through `centre` (a rotation, so winding
    is kept). A turned corner within `snap` model units of an existing vertex is that
    vertex (the candidates are the part's own vertices and `extra`). UVs are carried with `du` added to u, so a texture wrapped round the front
    half carries on round the back instead of restarting; colours are carried."""
    uvl, col, tex = R.layers(bm)
    made = {}
    worst = 0.0
    cands = verts_of(faces) + list(extra)

    def vert_for(v):
        nonlocal worst
        if v not in made:
            p = Vector((2 * centre[0] - v.co.x, 2 * centre[1] - v.co.y, v.co.z))
            best = min(cands, key=lambda w: (w.co - p).length)
            d = (best.co - p).length
            if d <= snap * U:
                worst = max(worst, d)
                made[v] = best
            else:
                made[v] = bm.verts.new(p)
        return made[v]

    new = []
    for f in faces:
        nf = bm.faces.new([vert_for(v) for v in f.verts])
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        for lo, ln in zip(f.loops, nf.loops):
            ln[uvl].uv = (lo[uvl].uv.x + du, lo[uvl].uv.y)
            ln[col] = lo[col]
        new.append(nf)
    return new, worst / U


def solve_offset(v, planes, T):
    """The horizontal move (dx, dy) of vertex v that puts it T inside each plane
    (outward unit normal n, move . n = -T), or keeps it in a plane (n, 0)."""
    A = np.asarray([[n.x, n.y] for n, _ in planes])
    b = np.asarray([t for _, t in planes])
    dx, dy = np.linalg.solve(A, b)
    return Vector((v.co.x + dx, v.co.y + dy, v.co.z))


def to_plane(p0, d, n, dist):
    """The point p0 + t d on the plane n . p = dist."""
    t = (dist - n.dot(p0)) / n.dot(d)
    return p0 + d * t


# ------------------------------------------------------------------ body (part 0)
body = objs[0]
bm = open_bm(body)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

(WALL, BOLT, ROOF, ANNEX_UP, STACK, STACK_TOP, RAMP, FLOOR, LINTEL, SLAB, SKIRT, ANNEX_LO,
 XLU, LINING, JAMB, CAP, RISER, SKIRT_N, ROOF_N, ROOF_UNDER, ANNEX_NEW) = range(1, 22)
BY_TRI = {}
for ids, p in (((0, 1, 2, 3, 16, 17), WALL), (tuple(range(4, 16)) + tuple(range(18, 42)), BOLT),
               (tuple(range(44, 56)) + (58, 59), ROOF), ((42, 43, 63, 64, 67, 68), ANNEX_UP),
               ((56, 57, 60, 61), STACK_TOP), ((62, 65, 66, 69, 70, 71), STACK),
               (range(72, 76), RAMP), ((76, 77), FLOOR), (range(78, 86), LINTEL),
               ((86, 96, 97, 98), SLAB), ((87, 88, 89, 90, 91, 92, 93, 94, 95, 102), SKIRT),
               ((99, 100, 101, 103), ANNEX_LO), ((106, 107, 108), XLU)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI[f[tri]]


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


# ------------------------------------------------------------------ roof fittings
bolt_report = []
for g in islands(faces_of(BOLT)):
    gv = verts_of(g)
    z_top = max(v.co.z for v in gv)
    z_bot = min(v.co.z for v in gv)
    light, rms = light_fit(bm, g)
    top_ring = [v for v in gv if abs(v.co.z - z_top) < 1e-4]
    c = R.fit_circle([(v.co.x, v.co.y) for v in top_ring])
    top_col = mean_colour(bm, [f for f in g if f.normal.z > 0.9])
    gset = set(g)
    new = []
    for e in {e for f in g for e in f.edges}:
        a, b = e.verts
        if not (abs(a.co.z - z_top) < 1e-4 and abs(b.co.z - z_top) < 1e-4):
            continue
        if sum(1 for f in e.link_faces if f in gset) != 1:
            continue
        a0 = at_or_new(bm, Vector((a.co.x, a.co.y, z_bot)))
        b0 = at_or_new(bm, Vector((b.co.x, b.co.y, z_bot)))
        f = new_face(bm, [a, b, b0, a0], g[0], lambda v: (0.0, 0.0), (0, 0, 0, 1), BOLT, part)
        m = f.calc_center_median()
        face_toward(f, Vector((m.x - c[0], m.y - c[1], 0.0)))
        new.append(f)
    R.agree_winding(bm, new)
    # five sides of 72 degrees: past densify_curve's default limit
    zc = 0.5 * (z_top + z_bot)
    near = [f for f in faces_of(BOLT) if (f.calc_center_median() - Vector((c[0], c[1], zc))).length < 2 * c[2]]
    R.densify_curve(bm, near, c, max_deg=90.0)
    after = [f for f in faces_of(BOLT)
             if (f.calc_center_median() - Vector((c[0], c[1], zc))).length < 2 * c[2]]
    for f in after:
        for lp in f.loops:
            if f.normal.z > 0.9:
                lp[col] = top_col
            else:
                d = Vector((lp.vert.co.x - c[0], lp.vert.co.y - c[1], 0.0)).normalized()
                lp[col] = light(d)
    bolt_report.append("%d sides added, light rms %.1f" % (len(new), rms * 255))
report["roof_fittings"] = bolt_report

# ------------------------------------------------------------------ stacks
def fix_stack(pm, side, top, stack_p, top_p, partl):
    """Half-turn the south half of a six-sided stack onto its north, then double it twice
    to 24 sides and re-map its top by its own planar projection."""
    puvl, pcol, ptex = R.layers(pm)
    tv = verts_of(top)
    cx = sum(v.co.x for v in tv) / len(tv)
    cy = sum(v.co.y for v in tv) / len(tv)
    # central symmetry check: every top corner's opposite, measured
    miss = max(min((Vector((2 * cx - v.co.x, 2 * cy - v.co.y, v.co.z)) - w.co).length for w in tv)
               for v in tv) / U
    us = [lp[puvl].uv.x for f in side for lp in f.loops]
    du = max(us) - min(us)
    top_uv, top_rms = R.fit_uv(top, puvl, lambda co: (co.x, co.y, 1.0))
    # the foot ring is symmetric only to about 11 model units (the top to about 3); its
    # corners are 140 or more apart, so the turned copy is welded at 15
    new, worst = half_turn(pm, side, (cx, cy), snap=15.0, du=du, extra=tv)
    for f in new:
        f[partl] = stack_p
    R.agree_winding(pm, new)
    bmesh.ops.remove_doubles(pm, verts=pm.verts, dist=1e-5)
    # the top corners differ in height by up to 1 model unit
    z_top = max(v.co.z for v in tv)
    ring = [v for v in verts_of([f for f in pm.faces if f[partl] == stack_p])
            if abs(v.co.z - z_top) < 3.0 * U]
    c = R.fit_circle([(v.co.x, v.co.y) for v in ring])
    for md in (75.0, 45.0):
        R.densify_curve(pm, [f for f in pm.faces if f.is_valid and f[partl] in (stack_p, top_p)],
                        c, max_deg=md)
    sides = len({round(math.degrees(R.angle_of(v.co, c))) % 360 for v in pm.verts
                 if abs(v.co.z - z_top) < 3.0 * U and R.radius_of(v.co, c) > 0.5 * c[2]})
    for f in pm.faces:
        if f.is_valid and f[partl] == top_p:
            for lp in f.loops:
                lp[puvl].uv = top_uv(lp.vert.co)
    return "6->%d sides, top symmetry miss %.1f, weld snap at most %.1f, u carried +%.1f, top uv rms %.4f" % (
        sides, miss, worst, du, top_rms)


report["stack_west"] = fix_stack(bm, faces_of(STACK), faces_of(STACK_TOP), STACK, STACK_TOP, part)

# ------------------------------------------------------------------ hall: inner lining
SW, NW, NE, SE = at(bm, -1271, -671, 75), at(bm, -1067, 381, 73), at(bm, 216, 166, 73), at(bm, 16, -885, 73)
SWt, NWt, NEt, SEt = (at(bm, -1118, -628, 965), at(bm, -941, 292, 965), at(bm, 67, 123, 965),
                      at(bm, -109, -796, 965))
walls = faces_of(WALL)


def wall_normal(pred):
    n = Vector()
    for f in walls:
        if pred(f.normal):
            n += f.normal * f.calc_area()
    return n.normalized()


nW = wall_normal(lambda n: n.x < -0.8)
nN = wall_normal(lambda n: n.y > 0.8)
nE = wall_normal(lambda n: n.x > 0.8)
door_n = (SE.co - SW.co).cross(SWt.co - SW.co).normalized()
if door_n.y > 0:
    door_n = -door_n
report["hall_doorway_normal"] = [round(x, 3) for x in door_n]
T = 16.0 * U
# the move of each corner, in its own horizontal plane: T inside both walls it joins,
# or T inside its wall and kept in the doorway's plane
inner_pos = {}
for v, planes in ((SW, ((nW, -T), (door_n, 0.0))),
                  (SWt, ((nW, -T), (door_n, 0.0))),
                  (NW, ((nW, -T), (nN, -T))), (NWt, ((nW, -T), (nN, -T))),
                  (NE, ((nN, -T), (nE, -T))), (NEt, ((nN, -T), (nE, -T))),
                  (SE, ((nE, -T), (door_n, 0.0))), (SEt, ((nE, -T), (door_n, 0.0)))):
    inner_pos[v] = solve_offset(v, planes, T)
wall_col = mean_colour(bm, walls)
lining, inner = back_shell(bm, walls, lambda v: inner_pos[v], shade=0.8)
for f in lining:
    f[part] = LINING
report["hall_lining_faces"] = len(lining)
jamb_col = scaled(wall_col, 0.9)
added = []
for a, b in ((SW, SWt), (SEt, SE)):
    f = new_face(bm, [a, b, inner[b], inner[a]], walls[0], lambda v: (0.0, 0.0), jamb_col, JAMB, part)
    face_toward(f, -door_n)
    added.append(f)
for a, b in ((SWt, NWt), (NWt, NEt), (NEt, SEt)):
    f = new_face(bm, [a, b, inner[b], inner[a]], walls[0], lambda v: (0.0, 0.0), wall_col, CAP, part)
    face_toward(f, Vector((0.0, 0.0, 1.0)))
    added.append(f)
report["hall_jambs_and_caps"] = len(added)

# the floor's front edge stands 48 units above the slab: a riser down to the slab
floor = faces_of(FLOOR)
fl_a, fl_b = at(bm, -1222, -606, 125), at(bm, 23, -814, 125)
slab_z = 77.0 * U
fa_uv = loop_at(next(f for f in fl_a.link_faces if f[part] == FLOOR), fl_a)[uvl].uv.copy()
fb_uv = loop_at(next(f for f in fl_b.link_faces if f[part] == FLOOR), fl_b)[uvl].uv.copy()
fl_col = scaled(mean_colour(bm, floor), 0.6)
ra = bm.verts.new((fl_a.co.x, fl_a.co.y, slab_z))
rb = bm.verts.new((fl_b.co.x, fl_b.co.y, slab_z))
r_uv = {fl_a: fa_uv, fl_b: fb_uv, ra: fa_uv - Vector((0.0, 0.05)), rb: fb_uv - Vector((0.0, 0.05))}
f = new_face(bm, [fl_a, fl_b, rb, ra], floor[0], lambda v: r_uv[v], fl_col, RISER, part)
face_toward(f, door_n)
added.append(f)

# ------------------------------------------------------------------ roof block
roof = faces_of(ROOF)
roof_light, rms = light_fit(bm, roof)
report["roof_light_fit_rms"] = round(rms * 255, 1)
nb0, nb1, nt1, nt0 = at(bm, -989, 249, 925), at(bm, 58, 80, 925), at(bm, 15, 87, 1130), at(bm, -945, 242, 1130)
# the north face carries the south strip's corner UVs, west to west and east to east
south_strip = [f for f in roof if f[tri] in (58, 59)]


def strip_uv(x, y, z):
    v = at(bm, x, y, z)
    return loop_at(next(f for f in v.link_faces if f in south_strip), v)[uvl].uv.copy()


n_uv = {nb0: strip_uv(-1154, -676, 925), nb1: strip_uv(-106, -845, 925),
        nt1: strip_uv(-125, -818, 1053), nt0: strip_uv(-1125, -656, 1053)}
fq = new_face(bm, [nb0, nb1, nt1, nt0], south_strip[0], lambda v: n_uv[v], (0, 0, 0, 1), ROOF_N, part)
face_toward(fq, Vector((0.2, 1.0, 0.0)))
nc = roof_light(fq.normal)
for lp in fq.loops:
    lp[col] = nc
report["roof_north_colour"] = round(nc[0] * 255)
added.append(fq)
ring = [at(bm, *p) for p in ((-1154, -676, 925), (-106, -845, 925), (-77, -694, 925),
                             (58, 80, 925), (-989, 249, 925), (-1125, -525, 925))]
top_faces = [f for f in roof if f.normal.z > 0.99]
roof_top_uv, _ = R.fit_uv(top_faces, uvl, lambda co: (co.x, co.y, 1.0))
fq = new_face(bm, ring, top_faces[0], lambda v: roof_top_uv(v.co), scaled(wall_col, 0.8), ROOF_UNDER, part)
face_toward(fq, Vector((0.0, 0.0, -1.0)))
added.append(fq)

# ------------------------------------------------------------------ lintel
lin = faces_of(LINTEL)
new, rms = R.close_convex(bm, lin)
for f in new:
    if f.is_valid:
        f[part] = LINTEL
report["lintel_faces_added"] = len(new)


# ------------------------------------------------------------------ annex
# the hall's east wall, and the plane half way into it (between the wall and its
# lining), where the annex's new north faces stop
dE = nE.dot(NE.co)
mid_d = dE - 0.5 * T
up_front = [f for f in faces_of(ANNEX_UP) if f[tri] in (42, 43)]
up_east = [f for f in faces_of(ANNEX_UP) if f[tri] in (63, 64)]
up_top = [f for f in faces_of(ANNEX_UP) if f[tri] in (67, 68)]
a_fsw, a_fse, a_tse, a_tsw = at(bm, 47, -405, 397), at(bm, 776, -526, 397), at(bm, 784, -479, 541), at(bm, 0, -347, 546)
a_ne_b, a_ne_t, a_nw_t = at(bm, 829, 13, 397), at(bm, 829, 13, 541), at(bm, 91, 147, 546)
# The block's west end reaches up to 43 model units through the hall's east wall, past
# where the lining now is. Its two top west corners (inside the wall, never seen) move
# along their top edges to the plane half way between wall and lining, their UVs by the
# faces' own planar fits, and the new north-west foot is put on that plane too.
front_uv_fit, rms_f = R.fit_uv(up_front, uvl, lambda co: (co.x, co.y, co.z, 1.0))
top_uv_fit, rms_t = R.fit_uv(up_top, uvl, lambda co: (co.x, co.y, 1.0))
report["annex_uv_fit_rms"] = [round(rms_f, 4), round(rms_t, 4)]
moved = []
for v, towards in ((a_tsw, a_tse), (a_nw_t, a_ne_t)):
    p = to_plane(v.co, towards.co - v.co, nE, mid_d)
    moved.append(round((p - v.co).length / U, 1))
    v.co = p
    for f in v.link_faces:
        lp = loop_at(f, v)
        if f in up_front:
            lp[uvl].uv = front_uv_fit(v.co)
        elif f in up_top:
            lp[uvl].uv = top_uv_fit(v.co)
report["annex_upper_west_corners_moved"] = moved
a_nw_b = bm.verts.new(to_plane(a_ne_b.co, a_fsw.co - a_fse.co, nE, mid_d))
report["annex_upper_nw_foot"] = [round(x / U) for x in a_nw_b.co]


def uv_of_face(fs, v):
    return loop_at(next(f for f in v.link_faces if f in fs), v)[uvl].uv.copy()


dark_up = min((mean_colour(bm, up_east), mean_colour(bm, up_front)), key=lambda c: sum(c[:3]))
n_uv = {a_ne_b: uv_of_face(up_front, a_fsw), a_nw_b: uv_of_face(up_front, a_fse),
        a_nw_t: uv_of_face(up_front, a_tse), a_ne_t: uv_of_face(up_front, a_tsw)}
annex_new = []
f = new_face(bm, [a_ne_b, a_nw_b, a_nw_t, a_ne_t], up_front[0], lambda v: n_uv[v], dark_up, ANNEX_NEW, part)
face_toward(f, Vector((0.18, 1.0, 0.0)))
annex_new.append(f)
e_uv = uv_of_face(up_east, a_ne_t)
f = new_face(bm, [a_fsw, a_tsw, a_nw_t, a_nw_b], up_east[0], lambda v: e_uv, dark_up, ANNEX_NEW, part)
face_toward(f, Vector((-1.0, 0.18, 0.0)))
annex_new.append(f)
f = new_face(bm, [a_fsw, a_fse, a_ne_b, a_nw_b], up_top[0], lambda v: top_uv_fit(v.co),
             scaled(mean_colour(bm, up_top), 0.35), ANNEX_NEW, part)
face_toward(f, Vector((0.0, 0.0, -1.0)))
annex_new.append(f)

lo = faces_of(ANNEX_LO)
lo_east = [f for f in lo if f[tri] in (99, 100)]
lo_south = [f for f in lo if f[tri] in (101, 103)]
l_sw_b, l_se_b, l_se_t, l_sw_t = at(bm, 107, -382, 69), at(bm, 743, -489, 69), at(bm, 743, -489, 406), at(bm, 68, -376, 406)
l_ne_b, l_ne_t = at(bm, 814, -8, 69), at(bm, 814, -8, 406)
l_nw_b = bm.verts.new(to_plane(l_ne_b.co, l_sw_b.co - l_se_b.co, nE, mid_d))
l_nw_t = bm.verts.new(to_plane(l_ne_t.co, l_sw_t.co - l_se_t.co, nE, mid_d))
report["annex_lower_nw"] = [[round(x / U) for x in l_nw_b.co], [round(x / U) for x in l_nw_t.co]]
dark_lo = min((mean_colour(bm, lo_east), mean_colour(bm, lo_south)), key=lambda c: sum(c[:3]))
ln_uv = {l_ne_b: uv_of_face(lo_south, l_sw_b), l_nw_b: uv_of_face(lo_south, l_se_b),
         l_nw_t: uv_of_face(lo_south, l_se_t), l_ne_t: uv_of_face(lo_south, l_sw_t)}
f = new_face(bm, [l_ne_b, l_nw_b, l_nw_t, l_ne_t], lo_south[0], lambda v: ln_uv[v], dark_lo, ANNEX_NEW, part)
face_toward(f, Vector((0.18, 1.0, 0.0)))
annex_new.append(f)
lw_uv = {l_sw_b: ln_uv[l_nw_b], l_nw_b: ln_uv[l_ne_b], l_nw_t: ln_uv[l_ne_t], l_sw_t: ln_uv[l_nw_t]}
f = new_face(bm, [l_sw_b, l_sw_t, l_nw_t, l_nw_b], lo_south[0], lambda v: lw_uv[v], dark_lo, ANNEX_NEW, part)
face_toward(f, Vector((-1.0, 0.18, 0.0)))
annex_new.append(f)

# the ramp round the annex's foot: its north end
ramp = faces_of(RAMP)
r_out, r_in = at(bm, 859, 12, 72), at(bm, 818, 19, 108)
r_foot = bm.verts.new((r_in.co.x, r_in.co.y, r_out.co.z))
ro_uv = uv_of_face(ramp, r_out)
ri_uv = uv_of_face(ramp, r_in)
rr = {r_out: ro_uv, r_in: ri_uv, r_foot: ro_uv}
f = new_face(bm, [r_out, r_in, r_foot], ramp[0], lambda v: rr[v], scaled(mean_colour(bm, ramp), 0.8),
             ANNEX_NEW, part)
face_toward(f, Vector((0.0, 1.0, 0.0)))
annex_new.append(f)
report["annex_faces_added"] = len(annex_new)

# ------------------------------------------------------------------ slab: north skirt
skirts = faces_of(SKIRT)
skirt_uv, rms = R.fit_uv(skirts, uvl, lambda co: (co.x, co.y, 1.0))
report["skirt_uv_fit_rms"] = round(rms, 4)
skirt_light, srms = light_fit(bm, skirts)
s_e_t, s_w_t = at(bm, 943, 203, 77), at(bm, -1072, 540, 77)
s_e_b, s_w_b = at(bm, 943, 203, -9), at(bm, -1072, 540, -9)
f = new_face(bm, [s_e_t, s_w_t, s_w_b, s_e_b], skirts[0], lambda v: skirt_uv(v.co), (0, 0, 0, 1), SKIRT_N, part)
face_toward(f, Vector((0.16, 1.0, 0.0)))
sc = skirt_light(f.normal)
for lp in f.loops:
    lp[col] = sc
report["skirt_north"] = "colour %d, light rms %.1f" % (round(sc[0] * 255), srms * 255)
added.append(f)

report["body_winding_fixed"] = R.agree_winding(bm, faces_of(LINING, JAMB, CAP, RISER, ROOF_N, ROOF_UNDER,
                                                           ANNEX_NEW, SKIRT_N))
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["body_winding_errors"] = winding_errors(bm)
report["body_triangles"] = "%d->%d" % (doc["parts"][0]["ntri"] - 2, len(bm.faces))
bm.to_mesh(body.data)
bm.free()
body.data.update()

# ------------------------------------------------------------------ east stack (part 1)
pm = open_bm(objs[1])
pl = pm.faces.layers.int.new("fix_part")
for f in pm.faces:
    f[pl] = STACK_TOP if f.normal.z > 0.9 else STACK
report["stack_east"] = fix_stack(pm, [f for f in pm.faces if f[pl] == STACK],
                                 [f for f in pm.faces if f[pl] == STACK_TOP], STACK, STACK_TOP, pl)
report["stack_east_triangles"] = "10->%d" % close_bm(pm, objs[1])


# ------------------------------------------------------------------ door halves (parts 2, 3)
def door_back(ob):
    """The back of a door panel: the front's own triangles carried onto the back corners
    the cartridge already modelled (each front corner's partner lies the panel's
    thickness behind it, at the same height), reversed and darker."""
    pm = open_bm(ob)
    puvl, pcol, ptex = R.layers(pm)
    n0 = Vector()
    for f in pm.faces:
        if f.normal.y < -0.9:
            n0 += f.normal * f.calc_area()
    n0.normalize()
    depth = [v.co.dot(n0) for v in pm.verts]
    lo_d, hi_d = min(depth), max(depth)
    mid = 0.5 * (lo_d + hi_d)
    front_v = {v for v in pm.verts if v.co.dot(n0) > mid}
    back_v = [v for v in pm.verts if v not in front_v]
    front = [f for f in pm.faces if f.normal.dot(n0) > 0.9 and all(v in front_v for v in f.verts)]
    # a face lying in the back plane but facing forward, inside the panel, never seen:
    # it would lie on the new back face and fight it for depth
    inner = [f for f in pm.faces if f.normal.dot(n0) > 0.9 and not any(v in front_v for v in f.verts)]
    for f in inner:
        pm.faces.remove(f)
    worst = 0.0
    partner = {}
    for v in verts_of(front):
        same = [w for w in back_v if abs(w.co.z - v.co.z) < 3.0 * U]
        w = min(same, key=lambda w: ((w.co - v.co) - (-n0) * (hi_d - lo_d)).length)
        worst = max(worst, ((w.co - v.co) - (-n0) * (hi_d - lo_d)).length)
        partner[v] = w
    new, _ = back_shell(pm, front, lambda v: partner[v].co.copy(), shade=0.45)
    bmesh.ops.remove_doubles(pm, verts=pm.verts, dist=1e-5)
    fixed = R.agree_winding(pm, [f for f in new if f.is_valid])
    n_before = len(pm.faces) - len(new)
    open_left = sum(1 for e in pm.edges if e.is_boundary)
    n = close_bm(pm, ob)
    return "%d->%d, back %d faces, %d hidden inner face dropped, thickness %.0f, pairing miss %.1f, flipped %d, open edges %d" % (
        n_before + len(inner), n, len(new), len(inner), (hi_d - lo_d) / U, worst / U, fixed, open_left)


report["door_upper"] = door_back(objs[2])
report["door_lower"] = door_back(objs[3])

# ------------------------------------------------------------------ every part checked
for ob in objs:
    pm = bmesh.new()
    pm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(pm, verts=pm.verts, dist=1e-5)
    report["part%d" % ob["pack_part"]] = "%d tris, open %d, winding errors %d" % (
        len(pm.faces), sum(1 for e in pm.edges if e.is_boundary), winding_errors(pm))
    pm.free()
report["fixed_triangles"] = sum(len(o.data.polygons) for o in objs)
report["ratio"] = round(report["fixed_triangles"] / float(report["cartridge_drawn"]), 2)

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "WEAP_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
