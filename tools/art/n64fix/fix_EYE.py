"""The Advanced Com. Center's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_EYE.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/EYE_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
and checked against the Remastered building, which shows two crossing long blocks, a
rectangular radar screen on a round pedestal, and two square lattice towers carrying
round dishes:

  body    the house-coloured block (NE to SW) has vertical long walls and sloped ends;
          its north-west long wall was never modelled. The grey block (NW to SE) has
          sloped long sides and a vertical end at each end; its north-west end was never
          modelled. Both are completed with a mirror copy of the opposite face, which
          carries that face's UVs and colour. The roof strip and ramp are whole. The
          radar mast is a 4-sided spike under a round pedestal: 8 sides, re-shaded
          as one.
  radar   the screen (part 1, it turns on its pivot) is a bent slab with no underside,
          and its feed horn has no bottom face: both closed.
  towers  (parts 2 and 4) square tapered lattices with only their south-west and
          south-east sides. The north-west and north-east sides are mirrors across the
          tower's west-east diagonal. The lattice is see-through, so every lattice face
          gets an inner side, set in slightly so the two never fight for depth.
  dishes  (parts 3, 5 and 6) round, 8 sides, single-sided, facing south. The rim goes to
          16 sides on the fitted circle of the dish's own tilted plane, and the dish gets
          a back shell that shares the rim and bulges a little behind the front.
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
doc, objs = B.build_cartridge(src, "EYE")
report = {"cartridge_triangles": sum(p["ntri"] for p in doc["parts"])}


# ------------------------------------------------------------------ helpers
def open_bm(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
    bm.normal_update()
    return bm


def close_bm(bm, ob):
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    bm.normal_update()
    n = len(bm.faces)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return n


def plane_mirror(point, normal):
    n = Vector(normal).normalized()
    p0 = Vector(point)

    def m(co):
        return co - 2.0 * (co - p0).dot(n) * n
    return m


def mirror_snap(bm, faces, mirror, candidates, snap):
    """Mirror copy of `faces` through the position map `mirror`, reversed so it faces
    outward again. Each mirrored corner lands on the nearest vertex of `candidates`
    within `snap` (so the copy welds onto the edges the cartridge left open, even where
    the cartridge is symmetric only to a few model units), otherwise on a new vertex.
    UVs and colours are carried from the source corner. Returns (new faces, worst snap
    distance)."""
    uvl, col, tex = R.layers(bm)
    made = {}
    worst = 0.0

    def vert_for(v):
        nonlocal worst
        if v in made:
            return made[v]
        p = mirror(v.co)
        best = min(candidates, key=lambda w: (w.co - p).length)
        d = (best.co - p).length
        if d <= snap:
            worst = max(worst, d)
            made[v] = best
        else:
            made[v] = bm.verts.new(p)
        return made[v]

    new = []
    for f in faces:
        vs = [vert_for(v) for v in reversed(f.verts)]
        nf = bm.faces.new(vs)
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo[uvl].uv
            ln[col] = lo[col]
        new.append(nf)
    return new, worst


def back_shell(bm, faces, move, shade=0.6, colour=None):
    """An inner or back side for a one-sided surface: a reversed copy of `faces` whose
    corners go through `move(vert) -> position or None` (None keeps the original vertex,
    so a back can share a rim with its front and leave no gap). UVs carried, colour the
    source corner times `shade`, or colour(vert) when given. Returns the new faces."""
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
            c = colour(lo.vert) if colour else lo[col]
            ln[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
        new.append(nf)
    return new


def mean_colour(loops_col):
    a = np.asarray(loops_col, float)
    return tuple(float(x) for x in a.mean(0))


# ------------------------------------------------------------------ body
body = objs[0]
bm = open_bm(body)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")
HOUSE, GREY, STRIP, MAST = 1, 2, 3, 4
for f in bm.faces:
    t, i = f[tex], f[tri]
    f[part] = (HOUSE if t == 38 else GREY if (t == 39 or i in (14, 17))
               else MAST if i in (15, 16, 18, 19) else STRIP)


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(*parts):
    return list({v for f in faces_of(*parts) for v in f.verts})


# the house-coloured block: its long walls are vertical, so the mirror plane is the
# vertical plane down the middle of its flat roof
house = faces_of(HOUSE)
roof_v = [v for v in verts_of(HOUSE) if v.co.z > 0.2]
c = sum((v.co for v in roof_v), Vector()) / len(roof_v)
xy = np.asarray([(v.co.x - c.x, v.co.y - c.y) for v in roof_v])
ax = np.linalg.svd(xy)[2][0]                    # the long axis
n_house = Vector((-ax[1], ax[0], 0.0))
side = [f for f in house if abs(f.normal.z) < 0.2 and abs(f.normal.dot(n_house)) > 0.9]
report["house_long_wall_faces"] = len(side)
new_wall, miss = mirror_snap(bm, side, plane_mirror(c, n_house), verts_of(HOUSE), 15.0 / B.CELL)
for f in new_wall:
    f[part] = HOUSE
report["house_wall_added"] = len(new_wall)
report["house_wall_snap_units"] = round(miss * B.CELL, 1)

# the grey block: its ends are vertical trapezoids; the mirror plane is the vertical
# plane across the block halfway between them
grey_roof = [v for v in verts_of(GREY) if v.co.z > 0.2]
g = sum((v.co for v in grey_roof), Vector()) / len(grey_roof)
xy = np.asarray([(v.co.x - g.x, v.co.y - g.y) for v in grey_roof])
u = np.linalg.svd(xy)[2][0]
u_grey = Vector((u[0], u[1], 0.0))
along = [(v.co - g).dot(u_grey) for v in grey_roof]
mid = g + u_grey * (0.5 * (min(along) + max(along)))
end = [f for f in faces_of(GREY) if abs(f.normal.dot(u_grey)) > 0.9]
report["grey_end_faces"] = len(end)
new_end, miss = mirror_snap(bm, end, plane_mirror(mid, u_grey), verts_of(GREY), 15.0 / B.CELL)
for f in new_end:
    f[part] = GREY
report["grey_end_added"] = len(new_end)
report["grey_end_snap_units"] = round(miss * B.CELL, 1)
report["body_winding_fixed"] = R.agree_winding(bm, new_wall + new_end)

# the radar mast: round in every reference, a 4-sided spike on the cartridge. One smooth
# shade fitted to its painted sides first, then 8 sides, then the shade applied to all
mast = faces_of(MAST)
foot = [v.co for v in verts_of(MAST) if v.co.z < 0.3]
mc = R.fit_circle([(p.x, p.y) for p in foot])
shade_mast = R.smooth_ring_shade(mast, col, mc)
R.densify_curve(bm, mast, mc, max_deg=100.0)
R.apply_colour(faces_of(MAST), col, shade_mast)
report["mast_sides"] = len(faces_of(MAST))

report["body_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
report["body_triangles"] = close_bm(bm, body)


# ------------------------------------------------------------------ radar screen
radar = objs[1]
bm = open_bm(radar)
uvl, col, tex = R.layers(bm)
def boundary_loops(bm):
    """The open-edge loops of a mesh, each as a set of vertices."""
    seen, loops = set(), []
    for e in bm.edges:
        if not e.is_boundary or e in seen:
            continue
        st, vs = [e], set()
        seen.add(e)
        while st:
            x = st.pop()
            vs.update(x.verts)
            for v in x.verts:
                for y in v.link_edges:
                    if y.is_boundary and y not in seen:
                        seen.add(y)
                        st.append(y)
        loops.append(vs)
    return loops


def uv_from_neighbour(v, exclude):
    lp = next(l for l in v.link_loops if l.face not in exclude)
    return lp[uvl].uv.copy()


loops = boundary_loops(bm)
# the feed horn's bottom: the only 3-edge hole, closed with one triangle shaded like
# the horn's darkest side
horn = next((list(vs) for vs in loops if len(vs) == 3), [])
added = []
if horn:
    t = bm.faces.new(horn)
    ref = next(f for f in horn[0].link_faces if f is not t)
    t.material_index = ref.material_index
    t[tex] = ref[tex]
    side_cols = [tuple(lp[col]) for f in bm.faces if f is not t
                 and sum(1 for v in f.verts if v in horn) >= 2 for lp in f.loops]
    dark = min(side_cols, key=lambda c: c[0])
    for lp in t.loops:
        lp[uvl].uv = uv_from_neighbour(lp.vert, {t})
        lp[col] = dark
    added.append(t)
# the screen's underside: its front and back bottom edges are two chains a slab's
# thickness apart. Each vertex pairs with its nearest open neighbour across the slab
slab = list(max(loops, key=len))
partner = {}
for v in slab:
    partner[v] = min((w for w in slab if w is not v), key=lambda w: (w.co - v.co).length)
all_cols = [tuple(lp[col]) for f in bm.faces for lp in f.loops if f not in added]
under_col = tuple(x * 0.6 for x in mean_colour(all_cols)[:3]) + (1.0,)
front = sorted([v for v in slab if partner[v].co.y < v.co.y], key=lambda v: v.co.x)
under = []
for a, b in zip(front, front[1:]):
    if not any(e.is_boundary and b in e.verts for e in a.link_edges):
        continue
    q = bm.faces.new([a, b, partner[b], partner[a]])
    ref = next(f for f in a.link_faces if f is not q)
    q.material_index = ref.material_index
    q[tex] = ref[tex]
    for lp in q.loops:
        lp[uvl].uv = uv_from_neighbour(lp.vert, {q})
        lp[col] = under_col
    under.append(q)
report["radar_underside_quads"] = len(under)
report["radar_horn_closed"] = len(added)
report["radar_winding_fixed"] = R.agree_winding(bm, under + added)
report["radar_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
report["radar_triangles"] = close_bm(bm, radar)


# ------------------------------------------------------------------ lattice towers
def fix_tower(ob, key):
    bm = open_bm(ob)
    uvl, col, tex = R.layers(bm)
    sides = [f for f in bm.faces if abs(f.normal.z) < 0.9]
    caps = [f for f in bm.faces if abs(f.normal.z) >= 0.9]
    base = [v for v in bm.verts if v.co.z < 0.05]
    pairs = [(a, b) for i, a in enumerate(base) for b in base[i + 1:]]
    w, e = max(pairs, key=lambda p: (p[0].co - p[1].co).length)   # the diagonal
    d = (e.co - w.co)
    n = Vector((-d.y, d.x, 0.0)).normalized()
    new, miss = mirror_snap(bm, sides, plane_mirror(w.co, n), list(bm.verts), 30.0 / B.CELL)
    report[key + "_sides_added"] = len(new)
    report[key + "_snap_units"] = round(miss * B.CELL, 1)
    report[key + "_winding_fixed"] = R.agree_winding(bm, new)
    # the inner side of the lattice, set in 6 model units from every face
    lat = [f for f in bm.faces]
    corners = [v for v in bm.verts if v.co.z < 0.05]
    ctr = sum((v.co for v in corners), Vector()) / len(corners)
    top_z = max(v.co.z for v in bm.verts)
    inset = 6.0 / B.CELL

    def move(v):
        r = Vector((ctr.x - v.co.x, ctr.y - v.co.y, 0.0))
        p = v.co + r.normalized() * inset * math.sqrt(2.0)
        if v.co.z > top_z - 1e-4:
            p.z -= inset
        return p
    inner = back_shell(bm, lat, move)
    report[key + "_inner_faces"] = len(inner)
    report[key + "_open_edges"] = sum(1 for x in bm.edges if x.is_boundary)
    report[key + "_triangles"] = close_bm(bm, ob)


fix_tower(objs[2], "tower_east")
fix_tower(objs[4], "tower_west")


# ------------------------------------------------------------------ dishes
def fix_dish(ob, key):
    bm = open_bm(ob)
    uvl, col, tex = R.layers(bm)
    hub = max(bm.verts, key=lambda v: len(v.link_faces))
    hub_co = hub.co.copy()
    rim_e = [e for e in bm.edges if e.is_boundary]
    rim = list({v for e in rim_e for v in e.verts})
    P = np.asarray([tuple(v.co) for v in rim])
    c = Vector(P.mean(0))
    axis = Vector(np.linalg.svd(P - P.mean(0))[2][2])
    if axis.dot(c - hub.co) < 0:
        axis = -axis                     # points out of the dish's open face

    def split(co):
        rel = co - c
        a = rel.dot(axis)
        return a, rel - axis * a

    # 16 sides: cut each rim edge and put the cut on the circle through its two ends,
    # in the dish's own (tilted) plane
    ends = {}
    for e in rim_e:
        p0, p1 = e.verts[0].co.copy(), e.verts[1].co.copy()
        ends[tuple(round(x, 6) for x in 0.5 * (p0 + p1))] = (p0, p1)
    res = bmesh.ops.subdivide_edges(bm, edges=rim_e, cuts=1, use_grid_fill=False)
    moved = 0
    for v in [g for g in res["geom_inner"] if isinstance(g, bmesh.types.BMVert)]:
        k = min(ends, key=lambda q: (Vector(q) - v.co).length)
        p0, p1 = ends[k]
        a0, r0 = split(p0)
        a1, r1 = split(p1)
        dirn = (r0.normalized() + r1.normalized()).normalized()
        v.co = c + axis * (0.5 * (a0 + a1)) + dirn * (0.5 * (r0.length + r1.length))
        moved += 1
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    bm.normal_update()
    report[key + "_rim_sides"] = len([e for e in bm.edges if e.is_boundary])
    hub = min(bm.verts, key=lambda v: (v.co - hub_co).length)

    # the back: shares the rim, bulges behind the front by up to 10 model units at the
    # hub. Shaded from the front's own painting at 0.6, except the hub, whose black is
    # the shadow of the feed on the front and means nothing on the back
    front = list(bm.faces)
    rim = {v for e in bm.edges if e.is_boundary for v in e.verts}
    R_max = max(split(v.co)[1].length for v in rim)
    depth = 10.0 / B.CELL
    ring_cols = [tuple(lp[col]) for v in hub.link_edges for w in [v.other_vert(hub)]
                 for lp in w.link_loops]
    hub_col = mean_colour(ring_cols)

    def move(v):
        if v in rim:
            return None
        return v.co - axis * depth * (1.0 - split(v.co)[1].length / R_max)

    front_col = {v: mean_colour([tuple(lp[col]) for lp in v.link_loops]) for v in bm.verts}
    front_col[hub] = hub_col

    def colour(v):
        return front_col[v]
    back = back_shell(bm, front, move, shade=0.6, colour=colour)
    report[key + "_back_faces"] = len(back)
    report[key + "_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
    report[key + "_triangles"] = close_bm(bm, ob)


fix_dish(objs[3], "dish_east")
fix_dish(objs[5], "dish_west_high")
fix_dish(objs[6], "dish_west_low")

report["fixed_triangles"] = sum(len(o.data.polygons) for o in objs)
report["ratio"] = round(report["fixed_triangles"] / report["cartridge_triangles"], 2)

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "EYE_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
