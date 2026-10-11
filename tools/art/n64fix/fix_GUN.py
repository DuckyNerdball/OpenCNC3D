"""The Gun Turret's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_GUN.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/GUN_fixed.blend; run.sh then puts it
in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building, which
shows a square plinth with all four corners cut off, a wedge-shaped turret with a sloped
front, a gun housing at the root of the barrel and a round barrel:

  plinth  (part 0) a square turned 45 degrees, its corners at the four compass points.
          The east, south and west corners are cut off 0.27 cells from the centre; the
          north corner, under the barrel at rest, was left sharp at 0.31. Cut off to match
          the other three: the two side walls and the top are trimmed, and the new north
          face carries the south face's UVs (a mirror copy across the east-west axis) and
          a colour from one fit of the six painted walls round the plinth.
  turret  (part 1, it turns on its own pivot) closed on every side but its underside:
          the barrel points north at rest, so the back the camera sees at rest and the
          front it sees once the turret turns are both modelled. The underside and the
          gun housing's underside are closed.
  barrel  (part 1) a tapered rectangular box, 0.073 by 0.042 cells at the root and 0.058
          by 0.028 at the muzzle, with no underside. Rebuilt round, 16 sides, as an
          elliptical tube of exactly the box's width and height at both ends, so its
          outline from above and from the side is the cartridge's own. The cartridge's
          top-down UV projection is kept (the sides were mapped by it too), and the whole
          tube is shaded by one fit of its painted colours (top 60, sides 40). Its root
          end stays open: it lies wholly inside the gun housing.

Nothing is relit and no geometry the references do not show is added: the Remastered
turret ring is not on the cartridge (the turret floats 0.017 cells above the plinth), so
none is made.
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
doc, objs = B.build_cartridge(src, "GUN")
report = {"cartridge_triangles": sum(len(o.data.polygons) for o in objs)}


# ------------------------------------------------------------------ helpers
def face_uv_map(f, uvl):
    """The affine map from position to UV that a flat face's own corners state. Exact
    at the corners, so a point moved along the face's plane keeps the face's mapping."""
    pred, _ = R.fit_uv([f], uvl, lambda co: (co.x, co.y, co.z, 1.0))
    return pred


def rebuild_face(bm, f, verts, uv_at, col_at):
    """A new face on `verts`, carrying f's material and texture, UV and colour from the
    given functions of the corner position. Returns the new face."""
    uvl, col, tex = R.layers(bm)
    nf = bm.faces.new(verts)
    nf.material_index = f.material_index
    nf[tex] = f[tex]
    for lp in nf.loops:
        lp[uvl].uv = uv_at(lp.vert.co)
        lp[col] = col_at(lp.vert.co)
    return nf


def loop_of(edges):
    """Order the vertices of one closed chain of edges."""
    adj = {}
    for e in edges:
        a, b = e.verts
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    start = next(iter(adj))
    ring, prev = [start], None
    while True:
        nxt = [w for w in adj[ring[-1]] if w is not prev]
        if not nxt or nxt[0] is start:
            break
        prev = ring[-1]
        ring.append(nxt[0])
    return ring


def orient(f, want):
    f.normal_update()
    if f.normal.dot(want) < 0:
        f.normal_flip()


# ------------------------------------------------------------------ plinth (part 0)
base = objs[0]
bm = bmesh.new()
bm.from_mesh(base.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
bm.normal_update()

top_z = max(v.co.z for v in bm.verts)
bot_z = min(v.co.z for v in bm.verts)
tops = [v for v in bm.verts if abs(v.co.z - top_z) < 0.01]
n_t = max(tops, key=lambda v: v.co.y)
n_b = min((v for v in bm.verts if abs(v.co.z - bot_z) < 0.01),
          key=lambda v: (v.co.xy - n_t.co.xy).length)
# the three cut corners: their flat walls' distance from the plinth's centre
east = [v for v in tops if v.co.x > 0.2]
west = [v for v in tops if v.co.x < -0.2]
south = [v for v in tops if v.co.y < -0.2]
x_e = sum(v.co.x for v in east) / len(east)
x_w = sum(v.co.x for v in west) / len(west)
y_s = sum(v.co.y for v in south) / len(south)
cut = 0.5 * (x_e - x_w)
cy = y_s + cut
y_n = cy + cut
report["plinth_cut_from_centre"] = round(cut, 4)
report["north_corner_was"] = round(n_t.co.y - cy, 4)

# the two walls meeting at the sharp corner, and the top face over it
old = list(n_t.link_faces) + list(n_b.link_faces)
old = list(dict.fromkeys(old))
top_faces = [f for f in bm.faces if f.normal.z > 0.9]
top_uv, top_rms = R.fit_uv(top_faces, uvl, lambda co: (co.x, co.y, 1.0))
report["plinth_top_uv_fit_rms"] = round(top_rms, 5)
# the neighbour corners along each wall, where the new corners are cut in
ne = next(v for f in old if f.normal.x > 0.3 for v in f.verts
          if v not in (n_t, n_b) and abs(v.co.z - top_z) < 0.01)
nw = next(v for f in old if f.normal.x < -0.3 for v in f.verts
          if v not in (n_t, n_b) and abs(v.co.z - top_z) < 0.01)


def on_wall(a, z):
    t = (y_n - a.co.y) / (n_t.co.y - a.co.y)
    p = a.co.lerp(n_t.co, t)
    return bm.verts.new((p.x, p.y, z))


a_t, a_b = on_wall(ne, top_z), on_wall(ne, bot_z)
b_t, b_b = on_wall(nw, top_z), on_wall(nw, bot_z)
sub = {True: {n_t: a_t, n_b: a_b}, False: {n_t: b_t, n_b: b_b}}
new_base = []
for f in old:
    c0 = tuple(f.loops[0][col])
    if f.normal.z > 0.9:
        # the sharp corner becomes two, in the fan's own order: the east one follows
        # the east neighbour, the west one the west neighbour
        vs = []
        fv = list(f.verts)
        for i, v in enumerate(fv):
            if v is n_t:
                prev = fv[i - 1]
                vs += [a_t, b_t] if prev.co.x > v.co.x else [b_t, a_t]
            else:
                vs.append(v)
        nf = rebuild_face(bm, f, vs, top_uv, lambda co, c=c0: c)
    else:
        m = sub[f.normal.x > 0]
        pred = face_uv_map(f, uvl)
        nf = rebuild_face(bm, f, [m.get(v, v) for v in f.verts], pred, lambda co, c=c0: c)
    new_base.append(nf)
bmesh.ops.delete(bm, geom=old, context="FACES_ONLY")
bmesh.ops.delete(bm, geom=[n_t, n_b], context="VERTS")

# the new north wall: the south wall mirrored across the east-west axis carries its UVs,
# its west end to the west end
sw = [f for f in bm.faces if f.normal.y < -0.9]
s_uv = {}
for f in sw:
    for lp in f.loops:
        s_uv[(lp.vert.co.x > 0, lp.vert.co.z > 0.5 * (top_z + bot_z))] = tuple(lp[uvl].uv)
# one fit of the painted walls round the plinth: colour = a0 + a1 cos + b1 sin of the
# wall's facing (they are painted as if lit from the south-west)
walls = [f for f in bm.faces if abs(f.normal.z) < 0.1]
A = np.asarray([[1.0, f.normal.x, f.normal.y] for f in walls])
Y = np.asarray([tuple(f.loops[0][col]) for f in walls], float)
coef = np.linalg.lstsq(A, Y, rcond=None)[0]
n_col = tuple(float(x) for x in np.clip(np.asarray([1.0, 0.0, 1.0]) @ coef, Y.min(0), Y.max(0)))
report["north_wall_grey"] = int(round(n_col[0] * 255))
nf = bm.faces.new([a_t, a_b, b_b, b_t])
nf.material_index = sw[0].material_index
nf[tex] = sw[0][tex]
for lp in nf.loops:
    lp[uvl].uv = s_uv[(lp.vert.co.x > 0, lp.vert.co.z > 0.5 * (top_z + bot_z))]
    lp[col] = n_col
orient(nf, Vector((0, 1, 0)))
new_base.append(nf)
report["plinth_winding_fixed"] = R.agree_winding(bm, new_base)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["plinth_triangles"] = len(bm.faces)
report["plinth_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
bm.to_mesh(base.data)
bm.free()
base.data.update()

# ------------------------------------------------------------------ turret (part 1)
tur = objs[1]
bm = bmesh.new()
bm.from_mesh(tur.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
bm.normal_update()
BARREL = set(range(19, 27))
HOUSING = {29, 30, 32, 33, 36, 37, 43, 50}
barrel = [f for f in bm.faces if f[tri] in BARREL]
housing = [f for f in bm.faces if f[tri] in HOUSING]
body = [f for f in bm.faces if f[tri] not in BARREL | HOUSING]


def dark(faces, k):
    s = np.mean([tuple(lp[col]) for f in faces for lp in f.loops], 0)
    return (s[0] * k, s[1] * k, s[2] * k, 1.0)


# ---- the body's underside: one flat face on its open bottom edge
ring = loop_of(R.open_edges(bm, body))
body_top = [f for f in body if f.normal.z > 0.9]
body_uv, rms = R.fit_uv(body_top, uvl, lambda co: (co.x, co.y, 1.0))
report["turret_top_uv_fit_rms"] = round(rms, 4)
under_col = dark([f for f in body if abs(f.normal.z) < 0.5], 0.6)
bottom = rebuild_face(bm, body[0], ring, body_uv, lambda co: under_col)
orient(bottom, Vector((0, 0, -1)))
report["turret_underside_corners"] = len(ring)

# ---- the gun housing: its underside, and its back (inside the turret's sloped front)
ring = loop_of(R.open_edges(bm, housing))
h_uv, _ = R.fit_uv([f for f in housing if f.normal.z > 0.9], uvl,
                   lambda co: (co.x, co.y, 1.0))
h_col = dark(housing, 0.6)
lo = sorted(ring, key=lambda v: v.co.z)[:4]          # the four bottom corners
hi = [v for v in ring if v not in lo]                  # the two rear top corners
rear_lo = sorted(lo, key=lambda v: v.co.y)[:2]


hb = rebuild_face(bm, housing[0], sorted(lo, key=lambda v: math.atan2(
    v.co.y - sum(w.co.y for w in lo) / 4, v.co.x - sum(w.co.x for w in lo) / 4)),
    h_uv, lambda co: h_col)
orient(hb, Vector((0, 0, -1)))
back = rear_lo + hi
bc = sum((v.co for v in back), Vector()) / 4
hr = rebuild_face(bm, housing[0], sorted(back, key=lambda v: math.atan2(
    v.co.z - bc.z, v.co.x - bc.x)), h_uv, lambda co: h_col)
orient(hr, Vector((0, -1, 0)))
report["housing_closed_triangles"] = 4

# ---- the barrel, round
bv = list({v for f in barrel for v in f.verts})
y_root = min(v.co.y for v in bv)
y_muz = max(v.co.y for v in bv)
top = [f for f in barrel if f.normal.z > 0.9]


def end(y):
    vs = [v for v in bv if abs(v.co.y - y) < 0.01]
    x0, x1 = min(v.co.x for v in vs), max(v.co.x for v in vs)
    z0, z1 = min(v.co.z for v in vs), max(v.co.z for v in vs)
    uvs = [(lp.vert.co.x, tuple(lp[uvl].uv)) for f in top for lp in f.loops
           if abs(lp.vert.co.y - y) < 0.01]
    u0 = min(uvs)[1][0]
    u1 = max(uvs)[1][0]
    return dict(y=y, cx=0.5 * (x0 + x1), cz=0.5 * (z0 + z1), hw=0.5 * (x1 - x0),
                hh=0.5 * (z1 - z0), u0=u0, u1=u1, v=uvs[0][1][1])


ends = [end(y_root), end(y_muz)]
report["barrel_root_w_h"] = [round(2 * ends[0]["hw"], 4), round(2 * ends[0]["hh"], 4)]
report["barrel_muzzle_w_h"] = [round(2 * ends[1]["hw"], 4), round(2 * ends[1]["hh"], 4)]
side_grey = min(tuple(f.loops[0][col])[0] for f in barrel)
top_grey = max(tuple(f.loops[0][col])[0] for f in barrel)
tmpl = barrel[0]
SIDES = 16


def barrel_colour(phi):
    # top 60, sides 40 on the cartridge: one cosine round the tube, darker underneath
    g = side_grey + (top_grey - side_grey) * math.cos(phi)
    return (g, g, g, 1.0)


rings = []
for e in ends:
    r = []
    for k in range(SIDES):
        phi = 2 * math.pi * k / SIDES
        r.append((bm.verts.new((e["cx"] + e["hw"] * math.sin(phi), e["y"],
                                e["cz"] + e["hh"] * math.cos(phi))), phi))
    rings.append(r)
new_barrel = []
for k in range(SIDES):
    q = [rings[0][k], rings[0][(k + 1) % SIDES], rings[1][(k + 1) % SIDES], rings[1][k]]
    f = bm.faces.new([v for v, _ in q])
    f.material_index = tmpl.material_index
    f[tex] = tmpl[tex]
    for lp, (v, phi) in zip(f.loops, q):
        e = ends[0] if v in [w for w, _ in rings[0]] else ends[1]
        s = math.sin(phi)
        lp[uvl].uv = (0.5 * (e["u0"] + e["u1"]) + 0.5 * (e["u1"] - e["u0"]) * s, e["v"])
        lp[col] = barrel_colour(phi)
    m = f.calc_center_median()
    orient(f, Vector((m.x - ends[0]["cx"], 0.0, m.z - ends[0]["cz"])))
    new_barrel.append(f)
muzzle_grey = side_grey
cap = bm.faces.new([v for v, _ in rings[1]])
cap.material_index = tmpl.material_index
cap[tex] = tmpl[tex]
e1 = ends[1]
for lp in cap.loops:
    s = (lp.vert.co.x - e1["cx"]) / e1["hw"]
    lp[uvl].uv = (0.5 * (e1["u0"] + e1["u1"]) + 0.5 * (e1["u1"] - e1["u0"]) * s, e1["v"])
    lp[col] = (muzzle_grey, muzzle_grey, muzzle_grey, 1.0)
orient(cap, Vector((0, 1, 0)))
new_barrel.append(cap)
bmesh.ops.delete(bm, geom=barrel, context="FACES")
report["barrel_sides"] = SIDES
report["turret_winding_fixed"] = R.agree_winding(bm, [bottom, hb, hr] + new_barrel)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["turret_triangles"] = len(bm.faces)
# the only open loop left should be the barrel's root, inside the gun housing
report["turret_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
bm.to_mesh(tur.data)
bm.free()
tur.data.update()

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "GUN_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
