"""The Helicopter Pad's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_HPAD.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/HPAD_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building,
which shows a round landing disc on a sloped apron, with a block carrying the green
landing light standing on the apron at the north-east:

  pad     a near-regular octagon, 717 model units from its centre to a corner (corners
          at +-24, +-66, +-114 and +-156 degrees), flat at height 154. Rounded to 16
          sides: each side is cut in two and the new corner pushed out onto the circle.
          The texture is a top-down projection (planar to a fraction of a texel), so
          every corner is re-mapped by that one projection and the marking stays put.
  apron   all eight slopes are modelled, but their top sits 2 units below the pad and
          misses its corners by up to 5 units, so the two never shared an edge. The top
          is welded to the pad's rim; every slope is cut in two, its foot rounded with
          the four-point rule (the foot is an irregular blob, wider to the south and
          east, so it is smoothed along its own shape rather than onto a circle), and the
          whole apron re-shaded by one fit of its painted colours (flat per slope on the
          cartridge, 220 at the south-west top down to 120 at the north-east).
  box     the landing-light block is drawn as its tilted top and its south-east end
          only. The north-west end and the outer (north-east) wall are added, both down
          to the ground as the south-east end is, mapped the way the cartridge mapped
          that end (the top's edge texels dragged down) and shaded like it (110).

Under the box the pad's north-east side and the apron slope below it are cut but not
pushed out: the box hides them, and a rounded pad would poke up through the box's top
(which starts 1 unit below the pad). The apron's foot and the box's underside rest on
the ground and stay open. The two shadow triangles under the box are drawn by the game
from the pack and are not part of this model. The Remastered vents and barrels are not
on the cartridge, so none are added.
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402
import repair as R  # noqa: E402

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
B.reset_scene()
doc, objs = B.build_cartridge(src, "HPAD")
body = objs[0]
report = {"cartridge_triangles": len(doc["tris"]),
          "cartridge_drawn": sum(len(o.data.polygons) for o in objs)}


# ------------------------------------------------------------------ helpers
def P(x, y, z):
    """A pack position (x east, y up, z south, model units) in Blender cells."""
    return Vector(B.to_blender((x, y, z)))


def at(bm, p, tol=2.0 / B.CELL):
    """The vertex at a position, to within `tol`."""
    v = min(bm.verts, key=lambda w: (w.co - p).length)
    if (v.co - p).length > tol:
        raise SystemExit("no vertex near %s" % (tuple(round(c * B.CELL) for c in p),))
    return v


def face_toward(f, d):
    f.normal_update()
    if f.normal.dot(d) < 0:
        f.normal_flip()


def new_face(bm, vs, like, uv_of, colour):
    """A face through `vs` textured like `like` (a face, or (pack_tex, material slot));
    uv_of(vert) and colour(vert) (or one colour for all corners) give its corners."""
    uvl, col, tex = R.layers(bm)
    f = bm.faces.new(vs)
    if isinstance(like, tuple):
        f[tex], f.material_index = like
    else:
        f[tex] = like[tex]
        f.material_index = like.material_index
    for lp in f.loops:
        lp[uvl].uv = uv_of(lp.vert)
        lp[col] = colour(lp.vert) if callable(colour) else colour
    return f


def four_point(p0, p1, p2, p3):
    """The interpolatory four-point rule: the point between p1 and p2 on the smooth
    closed curve through p0..p3. Rounds an irregular outline along its own shape."""
    return (p1 + p2) * (9.0 / 16.0) - (p0 + p3) * (1.0 / 16.0)


def boundary_loops(bm):
    """Open-edge loops as ordered vertex lists (as in fix_FACT.py)."""
    left = {e for e in bm.edges if e.is_boundary}
    loops = []
    while left:
        e = left.pop()
        chain = [e.verts[0], e.verts[1]]
        while True:
            nxt = [g for g in chain[-1].link_edges if g in left]
            if not nxt:
                break
            g = nxt[0]
            left.discard(g)
            w = g.other_vert(chain[-1])
            if w is chain[0]:
                break
            chain.append(w)
        loops.append(chain)
    return loops


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


# ------------------------------------------------------------------ the cartridge mesh
bm = bmesh.new()
bm.from_mesh(body.data)
# weld exact duplicates only: the box's inner corner is 1 unit from the apron's top
# corner and must stay a separate vertex
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.5 / B.CELL)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
bm.normal_update()

PAD_T, BOX_T = set(range(0, 6)), set(range(6, 10))
pad_f = [f for f in bm.faces if f[tri] in PAD_T]
box_f = [f for f in bm.faces if f[tri] in BOX_T]
apron_f = [f for f in bm.faces if f[tri] >= 10]

# ------------------------------------------------------------------ pad: the circle
pad_v = list({v for f in pad_f for v in f.verts})
pc = R.fit_circle([(v.co.x, v.co.y) for v in pad_v])
report["pad_axis"] = "x %.0f y %.0f r %.0f" % tuple(c * B.CELL for c in pc)
pad_z = pad_v[0].co.z
pad_uv, pad_rms = R.fit_uv(pad_f, uvl, lambda co: (co.x, co.y, 1.0))
report["pad_uv_fit_rms"] = round(pad_rms, 5)
pad_col = tuple(pad_f[0].loops[0][col])
pad_like = (pad_f[0][tex], pad_f[0].material_index)
corners = sorted(pad_v, key=lambda v: R.angle_of(v.co, pc))

# ------------------------------------------------------------------ apron: sides
# the apron's slopes pair up by triangle (each pair a quad with consistent UVs); per
# side keep its top corners (as pad corners) and its foot, with the UV each carried
shade = R.smooth_ring_shade(apron_f, col, pc)
apron_like = (apron_f[0][tex], apron_f[0].material_index)
PAIRS = ((10, 20), (22, 23), (18, 19), (16, 17), (14, 15), (11, 21), (12, 13), (24, 25))
by_tri = {f[tri]: f for f in apron_f}
sides = []
snap_worst = 0.0
for pair in PAIRS:
    fs = [by_tri[i] for i in pair]
    uv_at = {}
    for f in fs:
        for lp in f.loops:
            uv_at[lp.vert] = lp[uvl].uv.copy()
    top, foot = [], []
    for v, uv in uv_at.items():
        if v.co.z > 0.1:
            c = min(corners, key=lambda w: (w.co.xy - v.co.xy).length)
            snap_worst = max(snap_worst, (c.co - v.co).length)
            top.append((c, uv))
        else:
            foot.append((v, uv))
    # order both edges by angle, the short way round
    a0 = R.angle_of(top[0][0].co, pc)
    rel = lambda v: (R.angle_of(v.co, pc) - a0 + math.pi) % (2 * math.pi) - math.pi  # noqa: E731
    top.sort(key=lambda t: rel(t[0]))
    foot.sort(key=lambda t: rel(t[0]))
    sides.append({"top": top, "foot": foot})
report["apron_weld_worst_units"] = round(snap_worst * B.CELL, 1)
sides.sort(key=lambda s: R.angle_of(0.5 * (s["top"][0][0].co + s["top"][1][0].co), pc))

# the side the box stands on: its top edge runs under the box's inner edge
box_mid = sum((v.co for f in box_f for v in f.verts), Vector()) / sum(len(f.verts) for f in box_f)
hidden = min(range(len(sides)), key=lambda i: (
    0.5 * (sides[i]["top"][0][0].co + sides[i]["top"][1][0].co) - box_mid).xy.length)

# ------------------------------------------------------------------ new corners
n = len(sides)
feet = [s["foot"][0][0] for s in sides]          # foot corners in ring order
for i, s in enumerate(sides):
    (a, _), (b, _) = s["top"]
    ra, rb = R.radius_of(a.co, pc), R.radius_of(b.co, pc)
    aa = R.angle_of(a.co, pc)
    am = aa + ((R.angle_of(b.co, pc) - aa + math.pi) % (2 * math.pi) - math.pi) * 0.5
    if i == hidden:
        m = 0.5 * (a.co + b.co)
    else:
        r = 0.5 * (ra + rb)
        m = Vector((pc[0] + r * math.cos(am), pc[1] + r * math.sin(am), pad_z))
    s["mid_top"] = bm.verts.new(m)
    c, d = s["foot"][0][0], s["foot"][1][0]
    if i == hidden:
        q = 0.5 * (c.co + d.co)
    else:
        q = four_point(feet[(i - 1) % n].co, c.co, d.co, feet[(i + 2) % n].co)
        q.z = 0.5 * (c.co.z + d.co.z)
    s["mid_foot"] = bm.verts.new(q)
report["pad_bulge_units"] = round((pc[2] - pc[2] * math.cos(math.pi / 8)) * B.CELL)

# ------------------------------------------------------------------ rebuild pad and apron
old_apron_top = {v for f in apron_f for v in f.verts if v.co.z > 0.1}
bmesh.ops.delete(bm, geom=pad_f + apron_f, context="FACES_ONLY")
for v in old_apron_top:
    if v.is_valid and not v.link_faces:
        bm.verts.remove(v)

rim = []
for s in sides:
    rim += [s["top"][0][0], s["mid_top"]]
pad = new_face(bm, rim, pad_like, lambda v: pad_uv(v.co), pad_col)
face_toward(pad, Vector((0, 0, 1)))
bmesh.ops.triangulate(bm, faces=[pad], quad_method="BEAUTY", ngon_method="BEAUTY")

new_apron = []
for s in sides:
    (a, ua), (b, ub) = s["top"]
    (c, uc), (d, ud) = s["foot"]
    m, q = s["mid_top"], s["mid_foot"]
    uv = {a: ua, b: ub, c: uc, d: ud, m: 0.5 * (ua + ub), q: 0.5 * (uc + ud)}
    for quad in ((a, m, q, c), (m, b, d, q)):
        f = new_face(bm, list(quad), apron_like, lambda v: uv[v], lambda v: shade(v.co))
        cen = f.calc_center_median()
        face_toward(f, Vector((cen.x - pc[0], cen.y - pc[1], 0.0)))
        new_apron.append(f)
bmesh.ops.triangulate(bm, faces=new_apron, quad_method="BEAUTY")

# ------------------------------------------------------------------ box: the two missing walls
t_in_nw, t_out_nw = at(bm, P(330, 153, -966)), at(bm, P(478, 190, -1116))
t_in_se, t_out_se = at(bm, P(654, 153, -669)), at(bm, P(798, 190, -831))
g_out_se = at(bm, P(798, 3, -831))
g_in_nw = bm.verts.new(P(330, 3, -966))
g_out_nw = bm.verts.new(P(478, 3, -1116))
top_f = next(f for f in box_f if f[tri] == 6)
side_f = next(f for f in box_f if f[tri] == 7)
side_col = tuple(side_f.loops[0][col])
box_uv = {}
for lp in top_f.loops:
    box_uv[lp.vert] = lp[uvl].uv.copy()
for f in box_f:
    for lp in f.loops:
        box_uv.setdefault(lp.vert, lp[uvl].uv.copy())
# the south-east end drags the top's south-east edge down (every corner at the top's
# v there); the north-west end does the same with the north-west edge, and the outer
# wall with the outer edge
u_in, u_out = box_uv[t_in_nw].x, box_uv[t_out_nw].x
v_nw, v_se = box_uv[t_in_nw].y, box_uv[t_in_se].y
box_uv[g_in_nw] = Vector((u_in, v_nw))
box_uv[g_out_nw] = Vector((u_out, v_nw))
outer_uv = {t_out_nw: Vector((u_out, v_nw)), g_out_nw: Vector((u_out, v_nw)),
            t_out_se: Vector((u_out, v_se)), g_out_se: Vector((u_out, v_se))}
along = (t_in_se.co - t_in_nw.co).normalized()
out_dir = (t_out_nw.co - t_in_nw.co)
out_dir.z = 0.0
out_dir.normalize()
nw_end = new_face(bm, [t_in_nw, t_out_nw, g_out_nw, g_in_nw], side_f,
                  lambda v: box_uv[v], side_col)
face_toward(nw_end, -along)
outer = new_face(bm, [t_out_nw, t_out_se, g_out_se, g_out_nw], side_f,
                 lambda v: outer_uv[v], side_col)
face_toward(outer, out_dir)
# the inner wall stands inside the apron under the pad's rim and is never seen; it is
# closed anyway so the block's only opening is its footprint on the ground
g_in_se = at(bm, P(654, 3, -669))
inner_uv = {t_in_nw: Vector((u_in, v_nw)), g_in_nw: Vector((u_in, v_nw)),
            t_in_se: Vector((u_in, v_se)), g_in_se: Vector((u_in, v_se))}
inner = new_face(bm, [t_in_nw, g_in_nw, g_in_se, t_in_se], side_f,
                 lambda v: inner_uv[v], side_col)
face_toward(inner, -out_dir)
bmesh.ops.triangulate(bm, faces=[nw_end, outer, inner])

# ------------------------------------------------------------------ checks
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bm.normal_update()
report["winding_errors"] = winding_errors(bm)
loops = boundary_loops(bm)
report["open_loops"] = ";".join(
    "%d edges z %.0f..%.0f" % (len(L), min(v.co.z for v in L) * B.CELL, max(v.co.z for v in L) * B.CELL)
    for L in loops)
report["fixed_triangles"] = len(bm.faces)
report["ratio"] = round(len(bm.faces) / float(report["cartridge_triangles"]), 2)
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "HPAD_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
