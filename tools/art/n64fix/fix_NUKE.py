"""The Power Plant's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_NUKE.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/NUKE_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building,
which shows a round cooling tower standing in the notch of a C-shaped block:

  tower   12 sides, only 8 of them modelled: the four facing north were left out
          because the console camera never looks from there. The twelve are unevenly
          spaced but mirror-symmetric east to west, so the gap is filled with a mirror
          copy of the south side, which meets the existing edges exactly. Then doubled
          to 24 sides.
  arc     the C-shaped block is a low drum under a sloped roof, both modelled on the
          outside only, with an open slot under the roof's outer edge. Doubled round the
          curve, then closed: a fascia under the roof edge, an inner wall facing the
          tower and a flat cap at each end.
  props   the pipes and the two pipe supports are single-sided blocks: closed with
          their convex hulls. The three roof vents are plates: given a back.
  slab    the ground slab is already closed (its underside is never seen) and stays.
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
doc, objs = B.build_cartridge(src, "NUKE")
body = objs[0]

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

TOWER, DRUM, DRUMTOP, ROOF, SLAB, PROP, VENT, INNER = 1, 2, 3, 4, 5, 6, 7, 8
for f in bm.faces:
    t, i = f[tex], f[tri]
    f[part] = (TOWER if t == 90 else ROOF if t in (91, 92) else VENT if t == -1
               else DRUM if (t == 89 and i != 46) else DRUMTOP if i in (101, 102, 103, 104, 110)
               else SLAB if t == 13 else PROP)


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


report = {"cartridge_triangles": len(bm.faces)}

# ------------------------------------------------------------------ tower
tw = faces_of(TOWER)
rim = [v.co for v in {v for f in tw for v in f.verts} if abs(v.co.z - 0.833) < 0.01]
tc = R.fit_circle([(p.x, p.y) for p in rim])
report["tower_axis"] = [round(tc[0], 4), round(tc[1], 4), round(tc[2], 4)]


def radial(f, c):
    m = f.calc_center_median()
    r = Vector((m.x - c[0], m.y - c[1], 0.0))
    return r.length > 1e-9 and f.normal.dot(r.normalized()) > 0.3


outer = [f for f in tw if radial(f, tc)]
funnel = [f for f in tw if f not in outer]
uv_outer, rms = R.fit_uv(outer, uvl, lambda co: (co.x, co.y, 1.0))
report["tower_uv_fit_rms"] = round(rms, 4)
# one smooth shade for the whole tower wall, fitted to the cartridge's painted front,
# so the front (flat per face on the cartridge) and the rebuilt back read as one
shade_outer = R.smooth_ring_shade(outer, col, tc)


axes = R.symmetry_axes(list({v for f in tw for v in f.verts}), tc)
# weld at 15 model units: the cartridge's vertices are symmetric only to within about
# 11 (measured at the waist ring), and no two tower vertices are closer than 100
# the mirror copy carries its source face's UVs: the top-down projection was painted
# for the front only, and re-projecting the back samples texels never meant to show
new_tower, axis_deg = R.complete_by_mirror(bm, outer, tc, [a for a, _ in axes],
                                           uv_pred=None, colour=shade_outer,
                                           tol=15.0 / B.CELL)
report["tower_mirror_candidates"] = [(a, round(m, 2)) for a, m in axes]
report["tower_mirror_deg"] = axis_deg
report["tower_faces_added"] = len(new_tower)
for f in new_tower:
    f[part] = TOWER
R.apply_colour([f for f in faces_of(TOWER) if radial(f, tc)], col, shade_outer)
R.densify_curve(bm, faces_of(TOWER), tc)

# ------------------------------------------------------------------ arc: drum and roof
drum = faces_of(DRUM)
foot = [v.co for v in {v for f in drum for v in f.verts} if v.co.z < 0.05]
ac = R.fit_circle([(p.x, p.y) for p in foot])
report["arc_axis"] = [round(ac[0], 4), round(ac[1], 4), round(ac[2], 4)]
R.densify_curve(bm, faces_of(DRUM, DRUMTOP, ROOF), ac)

# ------------------------------------------------------------------ arc: inner wall and end caps
roof = faces_of(ROOF)
rv = list({v for f in roof for v in f.verts})
top_z = max(v.co.z for v in rv)
inner_top = sorted([v for v in rv if abs(v.co.z - top_z) < 0.01],
                   key=lambda v: R.angle_of(v.co, ac) % (2 * math.pi))
# the arc runs through 0 degrees: order it from its west end round to its north-east end
angs = [math.degrees(R.angle_of(v.co, ac)) % 360 for v in inner_top]
inner_top = [v for _, v in sorted(zip([(a - 200) % 360 for a in angs], inner_top),
                                  key=lambda p: p[0])]
base_z = min(v.co.z for f in faces_of(DRUM) for v in f.verts)
tower_r_base = tc[2] + 0.015
concrete = next(f for f in faces_of(SLAB))
k_tex = 2.0                  # the slab repeats its concrete about twice per cell
wall_shade = 0.55


def dark_of(faces):
    s = [0.0] * 4
    n = 0
    for f in faces:
        for lp in f.loops:
            for i in range(4):
                s[i] += lp[col][i]
            n += 1
    return [x / max(n, 1) for x in s]


dcol = dark_of(faces_of(DRUM))
wall_col = (dcol[0] * wall_shade, dcol[1] * wall_shade, dcol[2] * wall_shade, 1.0)


def bottom_for(v):
    """The inner wall's foot under a roof vertex, kept clear of the tower's flared base."""
    p = Vector((v.co.x, v.co.y, base_z))
    d = Vector((p.x - tc[0], p.y - tc[1], 0.0))
    if d.length < tower_r_base:
        d = d.normalized() * tower_r_base
        p = Vector((tc[0] + d.x, tc[1] + d.y, base_z))
    return bm.verts.new(p)


feet = [bottom_for(v) for v in inner_top]
arc_len = [0.0]
for a, b in zip(inner_top, inner_top[1:]):
    arc_len.append(arc_len[-1] + (b.co - a.co).length)
new_inner = []
for i in range(len(inner_top) - 1):
    q = [inner_top[i], feet[i], feet[i + 1], inner_top[i + 1]]
    f = bm.faces.new(q)
    f.normal_update()
    m = f.calc_center_median()
    toward_tower = Vector((tc[0] - m.x, tc[1] - m.y, 0.0))
    if f.normal.dot(toward_tower) < 0:
        f.normal_flip()
    f[part] = INNER
    f[tex] = concrete[tex]
    f.material_index = concrete.material_index
    for lp in f.loops:
        j = inner_top.index(lp.vert) if lp.vert in inner_top else feet.index(lp.vert)
        lp[uvl].uv = (arc_len[j] * k_tex, lp.vert.co.z * k_tex)
        lp[col] = wall_col
    new_inner.append(f)


def end_cap(first):
    """Close one end of the arc in the radial plane: inner wall, roof edge, drum face."""
    it = inner_top[0] if first else inner_top[-1]
    ft = feet[0] if first else feet[-1]
    a_end = R.angle_of(it.co, ac)

    def near_end(vs, z0, z1):
        c = [v for v in vs if z0 <= v.co.z <= z1]
        return min(c, key=lambda v: abs((R.angle_of(v.co, ac) - a_end + math.pi) % (2 * math.pi) - math.pi))

    roof_out = near_end(rv, 0.30, top_z - 0.05)
    dv = list({v for f in faces_of(DRUM) for v in f.verts})
    drum_top = near_end(dv, 0.2, 0.35)
    drum_bot = near_end(dv, -0.01, 0.1)
    # include the fascia's foot at this end, or a sliver stays open beside it. The
    # corners are not coplanar (the drum ledge between the fascia foot and the drum edge
    # is flat), so the cap is fanned from its ground corner by hand rather than left to
    # the triangulator, which folds one triangle over.
    drop = drops[0] if first else drops[-1]
    rim = [drum_bot, drum_top, drop, roof_out, it]
    if (drop.co - drum_top.co).length < 1e-4:
        rim.remove(drop)
    tris = []
    for a, b in zip(rim, rim[1:]):
        t = bm.faces.new([ft, a, b])
        t[part] = INNER
        t[tex] = concrete[tex]
        t.material_index = concrete.material_index
        for lp in t.loops:
            r = R.radius_of(lp.vert.co, ac)
            lp[uvl].uv = (r * k_tex, lp.vert.co.z * k_tex)
            lp[col] = wall_col
        tris.append(t)
    tang = Vector((-math.sin(a_end), math.cos(a_end), 0.0))
    want = -1.0 if first else 1.0
    for t in tris:
        t.normal_update()
        if t.normal.dot(tang) * want < 0:
            t.normal_flip()
    return tris


# the fascia: the roof's outer edge dropped to the drum's top, closing the slot
drum_top_z = max(v.co.z for f in faces_of(DRUM) for v in f.verts)
out_z = min(v.co.z for v in rv)
outer_edge = sorted([v for v in rv if abs(v.co.z - out_z) < 0.03],
                    key=lambda v: (math.degrees(R.angle_of(v.co, ac)) - 200) % 360)
drops = [bm.verts.new((v.co.x, v.co.y, drum_top_z)) for v in outer_edge]
for i in range(len(outer_edge) - 1):
    f = bm.faces.new([outer_edge[i], outer_edge[i + 1], drops[i + 1], drops[i]])
    f.normal_update()
    m = f.calc_center_median()
    if f.normal.dot(Vector((m.x - ac[0], m.y - ac[1], 0.0))) < 0:
        f.normal_flip()
    f[part] = INNER
    f[tex] = concrete[tex]
    f.material_index = concrete.material_index
    for lp in f.loops:
        lp[uvl].uv = (R.angle_of(lp.vert.co, ac) * ac[2] * k_tex, lp.vert.co.z * k_tex)
        lp[col] = wall_col
report["fascia_faces"] = len(outer_edge) - 1

# the drum top is a fan whose inner edge is a straight chord, and at an end the
# fascia's foot can land just outside it: patch the ledge there with one triangle,
# mapped and shaded like the drum top it extends
dt_faces = faces_of(DRUMTOP)
dt_uv, _ = R.fit_uv(dt_faces, uvl, lambda co: (co.x, co.y, 1.0))
dt_col = dark_of(dt_faces)
ledges = 0
patches = []
for first in (True, False):
    drop = drops[0] if first else drops[-1]
    nxt = drops[1] if first else drops[-2]
    dv = list({v for f in faces_of(DRUM) for v in f.verts if 0.2 <= v.co.z <= 0.35})
    a_end = R.angle_of(drop.co, ac)
    corner = min(dv, key=lambda v: abs((R.angle_of(v.co, ac) - a_end + math.pi) % (2 * math.pi) - math.pi))
    probe = 0.5 * (drop.co + corner.co)
    probe.z = corner.co.z
    covered = any(R._covered(probe, [f], 0.004) for f in dt_faces)
    if covered or (corner.co - drop.co).length < 1e-4:
        continue
    t = bm.faces.new([corner, drop, nxt])
    t.normal_update()
    if t.normal.z < 0:
        t.normal_flip()
    t[part] = DRUMTOP
    t[tex] = dt_faces[0][tex]
    t.material_index = dt_faces[0].material_index
    for lp in t.loops:
        lp[uvl].uv = dt_uv(lp.vert.co)
        lp[col] = tuple(dt_col)
    ledges += 1
    patches.append(t)
report["ledge_patches"] = ledges

caps = end_cap(True) + end_cap(False)
report["end_cap_triangles"] = len(caps)
report["winding_fixed"] = R.agree_winding(bm, faces_of(INNER) + patches)

# ------------------------------------------------------------------ props
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


rms_props = []
for g in islands(faces_of(PROP)):
    main = max({f[tex] for f in g}, key=lambda t: sum(1 for f in g if f[tex] == t))
    new, rms = R.close_convex(bm, g, skip_below_z=0.03, uv_faces=[f for f in g if f[tex] == main])
    for f in new:
        if f.is_valid:
            f[part] = PROP
    rms_props.append(round(rms, 4))
report["prop_uv_fit_rms"] = rms_props
for f in R.double_side(bm, faces_of(VENT)):
    f[part] = VENT

bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["fixed_triangles"] = len(bm.faces)
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "NUKE_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
