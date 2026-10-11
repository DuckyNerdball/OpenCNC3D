"""The Temple of Nod's repair plan: the cartridge model, completed.

    blender -b --python tools/art/n64fix/fix_TMPL.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/TMPL_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building: a
vaulted hall with a gothic facade at its south end over a crescent forecourt, three
flying-buttress ribs down each side, each with a tall spike at its inner end, and a tower
with claw horns rising from the rear of the roof. The cartridge model is one part with
no animation data, so there is no moving piece to keep posed.

  hall    four plank roof panels and the facade are modelled; the north end of the hall
          is open. Closed with a flat end wall in the plane the roof's rear edge already
          lies in. No reference shows the back, so the wall carries no doorway: it uses
          the facade's own striped band (its texture minus the arch), mirrored at two
          vertical lines so the stripes keep the facade's width.
  tower   a tapered hexagon whose three south faces are modelled; the top and crown are
          whole. The three north faces are built between the existing top corners and
          the existing west and east columns. The two missing foot corners are the front
          feet mirrored front to back, then pulled forward to just inside the end wall,
          because the tower's top stands flush with the hall's rear edge and a mirrored
          foot would hang in the air behind it. Each new face carries the UVs and colour
          of the face it mirrors.
  ribs    six flat one-sided blades (arch plus spike). Each gets a back 64 model units
          behind it that is full thickness along the arch's upper edge and meets the
          blade at its underside, the spike's inner edge and the spike's tip: a wedge
          section, so the top of the arch shows a thick lit band (as the Remastered ribs
          do) for about two thirds of the triangles a full slab would cost.
  horns   six flat two-triangle claws on the tower, modelled facing north. Each is
          closed as a low pyramid: one apex 28 model units behind the claw's middle.
  court   the crescent forecourt is a cutout plate lying on the ground: stays.
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
doc, objs = B.build_cartridge(src, "TMPL")
report = {"cartridge_triangles": sum(1 for t in doc["tris"] if t["mode"] != "shadow")}
body = objs[0]

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
bm.normal_update()
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

FACADE, COURT, ROOF, HORN, TOWER, RIB, END, TOWER_N, RIB_BACK, HORN_BACK = range(1, 11)
# cartridge triangle ranges, read off the extract: facade 0-3, forecourt 4-5, roof 6-13,
# the six horns 22-33, the tower 34-43, everything else in texture 139 is a rib
for f in bm.faces:
    i = f[tri]
    f[part] = (FACADE if i <= 3 else COURT if i <= 5 else ROOF if i <= 13
               else HORN if 22 <= i <= 33 else TOWER if 34 <= i <= 43 else RIB)
TOWER_W, TOWER_F, TOWER_E = (34, 35), (36, 37), (38, 39)


# ------------------------------------------------------------------ helpers
def Q(x, y, z):
    """A position in model units, Blender axes (x east, y north, z up), in cells."""
    return Vector((x, y, z)) / B.CELL


def find(x, y, z, tol=8.0):
    p = Q(x, y, z)
    v = min(bm.verts, key=lambda w: (w.co - p).length)
    if (v.co - p).length > tol / B.CELL:
        raise SystemExit("no vertex near (%g, %g, %g)" % (x, y, z))
    return v


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def corner(faces, v):
    """UV and colour a vertex carries in the given faces (mean over its corners)."""
    ls = [lp for f in faces for lp in f.loops if lp.vert is v]
    if not ls:
        raise SystemExit("vertex not in the source faces")
    uv = sum((Vector(lp[uvl].uv) for lp in ls), Vector((0.0, 0.0))) / len(ls)
    c = tuple(sum(lp[col][k] for lp in ls) / len(ls) for k in range(4))
    return uv, c


def new_face(vs, like, fpart, uvs, cols):
    f = bm.faces.new(vs)
    f[part] = fpart
    f[tex] = like[tex]
    f.material_index = like.material_index
    for lp, uv, c in zip(f.loops, uvs, cols):
        lp[uvl].uv = uv
        lp[col] = c
    return f


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


def boundary_of(faces):
    """{edge: (a, b, loop)} for each open edge of `faces`, a -> b in the face's winding."""
    fs = set(faces)
    out_ = {}
    for f in faces:
        for lp in f.loops:
            if sum(1 for g in lp.edge.link_faces if g in fs) == 1:
                out_[lp.edge] = (lp.vert, lp.link_loop_next.vert, lp)
    return out_


def by_tris(ids):
    return [f for f in bm.faces if f.is_valid and f[tri] in ids]


# ------------------------------------------------------------------ the hall's axis
roof = faces_of(ROOF)
rv = verts_of(roof)
top = sorted([v for v in rv if v.co.z > max(w.co.z for w in rv) - 0.01], key=lambda v: v.co.y)
ridge_f, ridge_r = top[0], top[-1]
axis = Vector((ridge_r.co.x - ridge_f.co.x, ridge_r.co.y - ridge_f.co.y, 0.0)).normalized()
perp = Vector((axis.y, -axis.x, 0.0))
rear_ax = ridge_r.co.dot(axis)
report["hall_axis_deg"] = round(math.degrees(math.atan2(axis.y, axis.x)), 1)

# ------------------------------------------------------------------ hall: the north end wall
rear = [v for v in rv if abs(v.co.dot(axis) - rear_ax) < 12.0 / B.CELL]
rear.sort(key=lambda v: (v.co - ridge_r.co).dot(perp))        # west to east
if len(rear) != 5:
    raise SystemExit("expected 5 corners on the roof's rear edge, found %d" % len(rear))
w_eave, w_mid, _, e_mid, e_eave = rear
foot_z = 0.5 * (w_eave.co.z + e_eave.co.z)
feet = {v: bm.verts.new((v.co.x, v.co.y, foot_z)) for v in (w_mid, ridge_r, e_mid)}
facade = faces_of(FACADE)
# the facade's v runs with height; its u runs from the arch at the centre line (0) out to
# the stripes at the eaves (0.98). The end wall takes the stripe band only, 0.5 to 0.98,
# running in to the mid ridge line and back out, so no arch appears on it.
vfit, _ = R.fit_uv(facade, uvl, lambda co: (co.z, 1.0))
s_mid = 0.5 * (abs((w_mid.co - ridge_r.co).dot(perp)) + abs((e_mid.co - ridge_r.co).dot(perp)))
s_eave = 0.5 * (abs((w_eave.co - ridge_r.co).dot(perp)) + abs((e_eave.co - ridge_r.co).dot(perp)))


def end_uv(v):
    a = abs((v.co - ridge_r.co).dot(perp))
    if a <= s_mid:
        u = 0.98 - 0.48 * a / s_mid
    else:
        u = 0.5 + 0.48 * min(1.0, (a - s_mid) / (s_eave - s_mid))
    return Vector((u, vfit(v.co)[1]))


# painted like the facade's shaded (west) half: the cartridge lights this building from
# the west, so a north face is no brighter than the facade's darker half
end_col = min((tuple(lp[col]) for f in facade for lp in f.loops), key=lambda c: c[0])
end_faces = []
for vs in ([w_eave, feet[w_mid], w_mid], [feet[w_mid], feet[ridge_r], ridge_r, w_mid],
           [feet[ridge_r], feet[e_mid], e_mid, ridge_r], [feet[e_mid], e_eave, e_mid]):
    f = new_face(vs, facade[0], END, [end_uv(v) for v in vs], [end_col] * len(vs))
    f.normal_update()
    if f.normal.dot(axis) < 0:
        f.normal_flip()
    end_faces.append(f)
report["end_wall_faces"] = len(end_faces)

# ------------------------------------------------------------------ tower: the north faces
W_b, FW_b, FE_b, E_b = (find(-117, 588, 621), find(31, 298, 747), find(194, 259, 747),
                        find(439, 456, 629))
FW_t, FE_t, W_t, E_t = (find(96, 393, 1955), find(174, 370, 1955), find(22, 533, 2107),
                        find(290, 467, 2107))
BW_t, BE_t = find(133, 582, 1955), find(216, 562, 1955)
# the front-to-back mirror plane that maps the top's front corners onto its back corners
mn = ((BW_t.co - FW_t.co) + (BE_t.co - FE_t.co))
mn.z = 0.0
mn.normalize()
mp = 0.25 * (BW_t.co + FW_t.co + BE_t.co + FE_t.co)


def mirror(co):
    return co - 2.0 * (co - mp).dot(mn) * mn


report["tower_top_mirror_miss_units"] = round(max((mirror(FW_t.co) - BW_t.co).length,
                                                  (mirror(FE_t.co) - BE_t.co).length) * B.CELL, 1)
inside = rear_ax - 4.0 / B.CELL


def tower_foot(front):
    p = mirror(front.co)
    over = p.dot(axis) - inside
    if over > 0:
        p = p - axis * over
    return bm.verts.new(p), over * B.CELL


BW_b, pull_w = tower_foot(FW_b)
BE_b, pull_e = tower_foot(FE_b)
report["tower_feet_pulled_in_units"] = [round(pull_w, 1), round(pull_e, 1)]
tw_like = by_tris(TOWER_F)[0]
tower_new = []
# (new corner, source face group, source vertex) round each new face
for spec in (((W_b, TOWER_W, W_b), (BW_b, TOWER_W, FW_b), (BW_t, TOWER_W, FW_t), (W_t, TOWER_W, W_t)),
             ((BW_b, TOWER_F, FW_b), (BE_b, TOWER_F, FE_b), (BE_t, TOWER_F, FE_t), (BW_t, TOWER_F, FW_t)),
             ((BE_b, TOWER_E, FE_b), (E_b, TOWER_E, E_b), (E_t, TOWER_E, E_t), (BE_t, TOWER_E, FE_t))):
    vs, uvs, cols = [], [], []
    for v, grp, s in spec:
        uv, c = corner(by_tris(grp), s)
        vs.append(v)
        uvs.append(uv)
        cols.append(c)
    tower_new.append(new_face(vs, tw_like, TOWER_N, uvs, cols))
report["tower_faces_added"] = len(tower_new)

# ------------------------------------------------------------------ ribs: a wedge back each
RIB_DEPTH = 64.0 / B.CELL
BACK_SHADE = 0.8
GROUND = 5.0 / B.CELL
rib_back = []
rib_rims = []
ribs = islands(faces_of(RIB))
report["ribs"] = len(ribs)
for g in ribs:
    vs = verts_of(g)
    tip = max(vs, key=lambda v: v.co.z)
    spike = [f for f in g if tip in f.verts]
    if len(spike) != 1:
        raise SystemExit("rib tip in %d faces" % len(spike))
    s_base, s_mid = sorted([v for v in spike[0].verts if v is not tip], key=lambda v: v.co.z)
    bd = boundary_of(g)
    nbr = {}
    for e in bd:
        a, b = e.verts
        nbr.setdefault(a, []).append(b)
        nbr.setdefault(b, []).append(a)
    # the upper edge of the arch: from the spike's joint away from the tip, down to the
    # outer foot on the ground
    moved = [s_mid]
    prev, cur = tip, s_mid
    while cur.co.z > GROUND:
        nxt = [w for w in nbr[cur] if w is not prev][0]
        prev, cur = cur, nxt
        moved.append(cur)
    # the arch's root on the roof, beside the spike's foot
    root = [w for w in nbr[s_base] if w is not tip][0]
    moved.append(root)
    moved = set(moved)
    for f in g:
        if not any(v in moved for v in f.verts):
            raise SystemExit("a rib face has no corner on the thick side")
    n = sum((f.normal * f.calc_area() for f in g), Vector()).normalized()
    vmap = {v: (bm.verts.new(v.co - n * RIB_DEPTH) if v in moved else v) for v in vs}
    for f in g:
        nvs = [vmap[v] for v in reversed(f.verts)]
        ls = list(reversed(f.loops))
        rib_back.append(new_face(nvs, f, RIB_BACK, [lp[uvl].uv.copy() for lp in ls],
                                 [tuple(lp[col][k] * (BACK_SHADE if k < 3 else 1.0) for k in range(4))
                                  for lp in ls]))
    for e, (a, b, lp) in bd.items():
        if a not in moved and b not in moved:
            continue                        # the thin side: back and front meet here
        if a.co.z < GROUND and b.co.z < GROUND:
            continue                        # the outer foot, standing on the ground
        if {a, b} == {root, s_base}:
            continue                        # the arch's root, inside the roof
        quad = [b, a, vmap[a], vmap[b]]
        uva, uvb = lp[uvl].uv.copy(), lp.link_loop_next[uvl].uv.copy()
        ca, cb = tuple(lp[col]), tuple(lp.link_loop_next[col])
        uvs = [uvb, uva, uva, uvb]
        cols = [cb, ca, ca, cb]
        keep = [i for i in range(4) if i < 2 or quad[i] not in quad[:2]]
        rib_rims.append(new_face([quad[i] for i in keep], lp.face, RIB_BACK,
                                 [uvs[i] for i in keep], [cols[i] for i in keep]))
report["rib_faces_added"] = len(rib_back) + len(rib_rims)

# ------------------------------------------------------------------ horns: a low pyramid each
HORN_DEPTH = 28.0 / B.CELL
horn_new = []
horns = islands(faces_of(HORN))
report["horns"] = len(horns)
for g in horns:
    vs = verts_of(g)
    n = sum((f.normal * f.calc_area() for f in g), Vector()).normalized()
    apex = bm.verts.new(sum((v.co for v in vs), Vector()) / len(vs) - n * HORN_DEPTH)
    ls = [lp for f in g for lp in f.loops]
    auv = sum((Vector(lp[uvl].uv) for lp in ls), Vector((0.0, 0.0))) / len(ls)
    acol = tuple(sum(lp[col][k] for lp in ls) / len(ls) * (BACK_SHADE if k < 3 else 1.0) for k in range(4))
    for e, (a, b, lp) in boundary_of(g).items():
        ca, cb = lp[col], lp.link_loop_next[col]
        horn_new.append(new_face([b, a, apex], lp.face, HORN_BACK,
                                 [lp.link_loop_next[uvl].uv.copy(), lp[uvl].uv.copy(), auv],
                                 [tuple(cb[k] * (BACK_SHADE if k < 3 else 1.0) for k in range(4)),
                                  tuple(ca[k] * (BACK_SHADE if k < 3 else 1.0) for k in range(4)),
                                  acol]))
report["horn_faces_added"] = len(horn_new)

# ------------------------------------------------------------------ finish
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
report["winding_fixed"] = R.agree_winding(bm, end_faces + tower_new + rib_back + rib_rims + horn_new)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["fixed_triangles"] = len(bm.faces)
report["ratio"] = round(len(bm.faces) / float(report["cartridge_triangles"]), 2)
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "TMPL_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
