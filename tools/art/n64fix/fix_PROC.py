"""The Tiberium Refinery's repair plan: the cartridge model, completed and rounded.

    blender -b --python tools/art/n64fix/fix_PROC.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/PROC_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the 1995 sprite and its
construction frames (PROC.SHP, PROCMAKE.SHP), which show a tall house-coloured hall with
the harvester arch in its south face, round Tiberium tanks on the east, a tall mast on
the west, all on a concrete slab. Where they disagree (the sprite has two tanks and a
round pad behind the hall; the cartridge has three tanks and no pad) the cartridge wins.

  tanks   three octagonal tanks with a Tiberium lid. Each lid is whole; of the eight
          sides only the five facing south were modelled. The two smaller tanks are
          regular octagons; the big north one is symmetric only under a half turn (its
          corners sit 0.533 to 0.577 cells from the axis), so no mirror copy of its front
          meets its lid. The missing sides are therefore built straight down from the
          lid's own rim, which states every corner. The side texture is a lit cylinder
          painted across the front half (u runs 0 to 1 from the east corner round the
          south to the west corner), so a new corner takes the u of its mirror image
          across the east-west plane, and the two corners shared with modelled sides keep
          the cartridge's u, so the texture runs on without a seam. Then doubled to 16
          sides, re-shaded as a whole from one smooth fit of the painted sides, and the
          lid rebuilt as a fan from the axis with its own planar mapping.
  hall    the tall block's back is a slope from its roof's north edge down to the ground
          (both side walls state it) and was never modelled: closed with one quad, mapped
          across like the side walls and coloured by the one light that explains the
          hall's four painted faces.
  bar     the cross bar through the hall three quarters of the way up: back and underside
          added, mapped like its front and top, in the shade of its painted ends.
  wing    the low block east of the arch: its back wall (against the tank behind it) was
          open. Closed, keeping the cartridge's extra corner on its east edge.
  ramp    the sloped buttress on the hall's west side: its north-west side and its short
          back wall added; the back wall's inner end stands inside the hall.
  step    the grey block in front of the arch: its west end added, the mirror of its
          east end.
  mast    the tall mast on the west is two side plates and a front strip (part 1, its own
          object with its pivot, kept exactly as the cartridge has it). Its back, in two
          planes like the side plates' back edge, is closed.
  slab    the two north edges of the slab had no skirt; they get one like the other four.

The slab's underside, the tanks' and blocks' bottoms on the ground, the flat decals'
rims, and the joints buried inside the hall stay open: none of them can be seen.
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
doc, objs = B.build_cartridge(src, "PROC")
body = objs[0]

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")
bm.normal_update()

(TANK_A, TANK_B, TANK_C, LID_A, LID_B, LID_C, HALL, BAR, WING, RAMP, STEP, MAST, SLAB,
 DECAL) = range(1, 15)
# parts by cartridge triangle index (extract.py order)
BY_TRI = {}
for ids, p in (((3, 4, 5, 6, 7, 8, 9, 10, 11, 20), TANK_A),
               ((13, 14, 15, 16, 27, 28, 29, 30, 31, 32), TANK_B),
               ((12, 17, 18, 19, 21, 22, 23, 24, 25, 26), TANK_C),
               ((67, 73, 74, 75, 76, 84), LID_A),
               ((77, 78, 79, 80, 82, 83), LID_B),
               ((68, 69, 70, 71, 72, 81), LID_C),
               ((0, 1, 34, 35, 36, 37, 38, 50), HALL),
               (tuple(range(39, 47)), BAR),
               ((33, 47, 48, 49, 51, 52, 53, 54, 55, 56, 57), WING),
               (tuple(range(58, 65)), RAMP),
               (tuple(range(101, 107)), STEP),
               ((85, 86, 87, 88), MAST),
               (tuple(range(89, 101)), SLAB),
               ((2, 65, 66), DECAL)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI[f[tri]]


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def mean_colour(faces):
    a = np.asarray([tuple(lp[col]) for f in faces for lp in f.loops], float)
    return tuple(float(x) for x in a.mean(0))


def vert_at(p, tol=2.5 / B.CELL):
    """The existing vertex at p (within tol), or a new one there."""
    p = Vector(p)
    best = min(bm.verts, key=lambda v: (v.co - p).length)
    return best if (best.co - p).length < tol else bm.verts.new(p)


def grey(c):
    return (c / 255.0, c / 255.0, c / 255.0, 1.0)


def new_face(pts, like, fpart, uvs, colour):
    """A face through the given positions (existing vertices reused), with the texture
    and material slot of `like`, one UV per corner and one colour for all."""
    vs = [vert_at(p) for p in pts]
    f = bm.faces.new(vs)
    f[part] = fpart
    f[tex] = like[tex]
    f.material_index = like.material_index
    for lp, uv in zip(f.loops, uvs):
        lp[uvl].uv = uv
        lp[col] = colour
    return f


report = {"cartridge_triangles": len(bm.faces) + sum(len(o.data.polygons) for o in objs[1:])}
new_faces = []


# ------------------------------------------------------------------ helpers
def cap_fan(ring, centre, z, like_tex, mat, fpart, uv_of, colour):
    """A flat cap over a horizontal ring of vertices, fanned from a new vertex on the
    axis, facing up. Returns the new faces."""
    ring = sorted(ring, key=lambda v: R.angle_of(v.co, centre))
    hub = bm.verts.new((centre[0], centre[1], z))
    new = []
    for a, b in zip(ring, ring[1:] + ring[:1]):
        f = bm.faces.new([hub, a, b])
        f.normal_update()
        if f.normal.z < 0:
            f.normal_flip()
        f[part] = fpart
        f[tex] = like_tex
        f.material_index = mat
        for lp in f.loops:
            lp[uvl].uv = uv_of(lp.vert.co)
            lp[col] = colour
        new.append(f)
    return new


def light_fit(faces):
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
        c = np.clip(np.asarray((1.0, n.x, n.y, n.z)) @ M, 0.0, 1.0)
        return tuple(float(x) for x in c)
    return colour, rms


# ------------------------------------------------------------------ tanks
def wrap(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


def tank(side_p, lid_p, name):
    side, lid = faces_of(side_p), faces_of(lid_p)
    z_top = max(v.co.z for v in verts_of(lid))
    z_bot = min(v.co.z for v in verts_of(side))
    ring = [v for v in verts_of(lid) if abs(v.co.z - z_top) < 1e-3]
    c = R.fit_circle([(v.co.x, v.co.y) for v in ring])
    ring.sort(key=lambda v: R.angle_of(v.co, c))
    radii = [R.radius_of(v.co, c) * B.CELL for v in ring]
    # the painted cylinder: u at each modelled top corner, as a function of the angle
    # from the east corner (u 0), negative round the south to the west corner (u 1)
    u_at = {}
    for f in side:
        for lp in f.loops:
            if abs(lp.vert.co.z - z_top) < 1e-3:
                u_at.setdefault(lp.vert, []).append(lp[uvl].uv.x)
    east = min(u_at, key=lambda v: min(u_at[v]))
    a_e = R.angle_of(east.co, c)
    front = sorted((wrap(R.angle_of(v.co, c) - a_e), sum(u) / len(u)) for v, u in u_at.items()
                   if wrap(R.angle_of(v.co, c) - a_e) <= 1e-6 or max(u) > 0.9)
    front = [(-math.pi if u > 0.9 else d, u) for d, u in front]
    front.sort()
    fd = [d for d, _ in front]
    fu = [u for _, u in front]

    def u_mirror(v):
        d = wrap(R.angle_of(v.co, c) - a_e)
        return float(np.interp(-abs(d), fd, fu))

    shade = R.smooth_ring_shade(side, col, c)
    lid_uv, lid_rms = R.fit_uv(lid, uvl, lambda co: (co.x, co.y, 1.0))
    lid_col = mean_colour(lid)
    lid_tex, lid_mat = lid[0][tex], lid[0].material_index
    like = side[0]
    below = {}
    for f in side:
        for v in f.verts:
            if abs(v.co.z - z_bot) < 1e-3:
                below[(round(v.co.x, 4), round(v.co.y, 4))] = v

    def foot(v):
        k = (round(v.co.x, 4), round(v.co.y, 4))
        if k not in below:
            below[k] = bm.verts.new((v.co.x, v.co.y, z_bot))
        return below[k]

    def u_of(v):
        # a corner a modelled side already maps keeps the cartridge's u
        own = [lp[uvl].uv.x for lp in v.link_loops if lp.face[part] == side_p]
        return own[0] if own else u_mirror(v)

    new = []
    for a, b in zip(ring, ring[1:] + ring[:1]):
        e = bm.edges.get([a, b])
        if e is not None and any(f[part] == side_p for f in e.link_faces):
            continue
        ua, ub = u_of(a), u_of(b)
        fa, fb = foot(a), foot(b)
        nf = bm.faces.new([a, fa, fb, b])
        nf[part] = side_p
        nf[tex] = like[tex]
        nf.material_index = like.material_index
        uv = {a: (ua, 0.999), b: (ub, 0.999), fa: (ua, 0.001), fb: (ub, 0.001)}
        for lp in nf.loops:
            lp[uvl].uv = uv[lp.vert]
        new.append(nf)
    R.agree_winding(bm, new)
    # (vertex references go stale in the densify below: measure first)
    new_u = [round(u_mirror(v), 3) for v in ring if v not in u_at]
    R.apply_colour(faces_of(side_p), col, shade)
    bmesh.ops.delete(bm, geom=faces_of(lid_p), context="FACES_ONLY")
    # an octagon's sides span about 45 degrees: widen the chord test
    R.densify_curve(bm, faces_of(side_p), c, max_deg=60.0)
    R.apply_colour(faces_of(side_p), col, shade)
    top = [v for v in verts_of(faces_of(side_p)) if abs(v.co.z - z_top) < 1e-3]
    cap = cap_fan(top, c, z_top, lid_tex, lid_mat, lid_p, lid_uv, lid_col)
    report[name] = ("axis %.3f %.3f, corner radius %.0f..%.0f, %d sides added, u at new "
                    "corners %s, sides %d, lid uv rms %.4f" % (
                        c[0], c[1], min(radii), max(radii), len(new),
                        new_u, len(top), lid_rms))
    return cap


tank(TANK_A, LID_A, "tank_east")
tank(TANK_B, LID_B, "tank_south")
tank(TANK_C, LID_C, "tank_north")

# ------------------------------------------------------------------ hall back
hall = faces_of(HALL)
hall_light, hall_rms = light_fit(hall)
report["hall_light_rms"] = round(hall_rms * 255, 2)
wall = next(f for f in hall if f[tex] == 14)
# roof's north edge and the ground line both side walls slope down to
back_pts = [(0.023, 0.397, 0.001), (-0.608, 0.629, 0.001), (-0.743, 0.259, 1.052),
            (-0.110, 0.027, 1.052)]
vs = [vert_at(p) for p in back_pts]
if any(v.is_valid and v.co.z < 0.5 and not v.link_faces for v in vs):
    raise SystemExit("hall back: the side walls' ground corners were not found")
pts = [v.co.copy() for v in vs]
nrm = (pts[1] - pts[0]).cross(pts[3] - pts[0]).normalized()
if nrm.y < 0:
    nrm = -nrm
f = new_face(pts, wall, HALL, [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)],
             hall_light(nrm))
new_faces.append(f)
report["hall_back_colour"] = round(hall_light(nrm)[0] * 255, 1)

# ------------------------------------------------------------------ bar
bar = faces_of(BAR)
bar_like = bar[0]
end_col = grey(145)
bl, br = (-0.881, 0.282), (0.025, -0.047)      # the back corners, west and east
lo_z, hi_z = 0.779, 0.899
new_faces.append(new_face([(br[0], br[1], lo_z), (bl[0], bl[1], lo_z), (bl[0], bl[1], hi_z),
                           (br[0], br[1], hi_z)], bar_like, BAR,
                          [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)], end_col))
new_faces.append(new_face([(-0.976, 0.022, lo_z), (-0.068, -0.307, lo_z), (br[0], br[1], lo_z),
                           (bl[0], bl[1], lo_z)], bar_like, BAR,
                          [(1.0, 1.0), (0.0, 1.0), (0.0, 0.0), (1.0, 0.0)],
                          grey(145 * 0.8)))

# ------------------------------------------------------------------ wing back
wing_like = next(f for f in faces_of(WING) if f[tri] == 51)
# the east side maps u 0.109 to 1 along its 0.887 cells, and v 1 to its 0.337 height
k_u = (1.0 - 0.109) / 0.887
wb = [(-0.058, 0.161, 0.0), (0.140, 0.105, 0.0), (0.140, 0.105, 0.102), (0.140, 0.105, 0.337),
      (-0.058, 0.161, 0.337)]
run = Vector((0.198, -0.056, 0.0)).length
new_faces.append(new_face(wb, wing_like, WING,
                          [(0.0, 0.0), (run * k_u, 0.0), (run * k_u, 0.307), (run * k_u, 1.0),
                           (0.0, 1.0)], grey(145)))

# ------------------------------------------------------------------ ramp
ramp_like = next(f for f in faces_of(RAMP) if f[tri] == 58)
k_ramp_u, k_ramp_v = 0.98, 0.316 / 0.179


def ramp_quad(a, b, za, zb):
    """A vertical wall from ground to the given heights under the segment a-b."""
    d = Vector((b[0] - a[0], b[1] - a[1], 0.0)).length * k_ramp_u
    return new_face([(a[0], a[1], 0.0), (b[0], b[1], 0.0), (b[0], b[1], zb), (a[0], a[1], za)],
                    ramp_like, RAMP,
                    [(0.0, 0.0), (d, 0.0), (d, zb * k_ramp_v), (0.0, za * k_ramp_v)], grey(145))


new_faces.append(ramp_quad((-0.860, 0.478), (-1.491, -0.193), 0.570, 0.179))
new_faces.append(ramp_quad((-0.669, 0.394), (-0.860, 0.478), 0.570, 0.570))

# ------------------------------------------------------------------ step
step_like = next(f for f in faces_of(STEP) if f[tri] == 102)
new_faces.append(new_face([(-0.670, 0.086, 0.057), (-0.760, -0.160, 0.057), (-0.760, -0.160, 0.386),
                           (-0.670, 0.086, 0.386)], step_like, STEP,
                          [(0.0, 0.0), (0.0, 1.0), (1.0, 1.0), (1.0, 0.0)], grey(150)))

# ------------------------------------------------------------------ mast back
mast = faces_of(MAST)
mast_uv, mast_rms = R.fit_uv(mast, uvl, lambda co: (co.x, co.y, co.z, 1.0))
report["mast_uv_rms"] = round(mast_rms, 4)
m_col = grey(150)
for pts in (((-1.191, -0.055, -0.018), (-1.298, -0.049, -0.018), (-1.300, -0.097, 0.646),
             (-1.194, -0.103, 0.646)),
            ((-1.194, -0.103, 0.646), (-1.300, -0.097, 0.646), (-1.309, -0.257, 1.350),
             (-1.202, -0.263, 1.350))):
    f = new_face(pts, mast[0], MAST, [mast_uv(Vector(p)) for p in pts], m_col)
    new_faces.append(f)

# ------------------------------------------------------------------ slab skirts
slab = faces_of(SLAB)
skirt_like = next(f for f in slab if f[tri] == 95)
top_uv = {}
for f in slab:
    for lp in f.loops:
        top_uv.setdefault(tuple(round(x, 3) for x in lp.vert.co), lp[uvl].uv.copy())
skirt_col = grey(150)
corners = [(1.099, 0.586), (-0.845, 1.344), (-1.537, -0.038)]
bottoms = {(1.099, 0.586): (1.096, 0.577), (-0.845, 1.344): (-0.845, 1.344),
           (-1.537, -0.038): (-1.541, -0.047)}
for a, b in zip(corners, corners[1:]):
    ta, tb = vert_at((a[0], a[1], 0.009)), vert_at((b[0], b[1], 0.009))
    ua = [lp[uvl].uv.copy() for lp in ta.link_loops if lp.face[part] == SLAB][0]
    ub = [lp[uvl].uv.copy() for lp in tb.link_loops if lp.face[part] == SLAB][0]
    ba, bb = bottoms[a], bottoms[b]
    new_faces.append(new_face([(a[0], a[1], 0.009), (ba[0], ba[1], -0.048), (bb[0], bb[1], -0.048),
                               (b[0], b[1], 0.009)], skirt_like, SLAB, [ua, ua, ub, ub], skirt_col))

report["faces_added_by_hand"] = len(new_faces)
report["winding_fixed"] = R.agree_winding(bm, new_faces)

bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["body_triangles"] = len(bm.faces)
report["body_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
bm.to_mesh(body.data)
bm.free()
body.data.update()

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "PROC_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
