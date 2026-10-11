"""The Bio-Research Laboratory's repair plan: the cartridge model, completed and rounded.

    blender -b --python tools/art/n64fix/fix_BIO.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/BIO_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building and
its construction frames, which show a cross of sloped-walled blocks under a round dome
with the radiation trefoil on its flat top, round stacks behind, and a block on the west:

  body     the cross (four arms, sloped walls on every side, flat roof), the west block
           with its raised top, and the wedge on the east arm's end are all whole on the
           cartridge. The wedge's open back is buried in the arm's sloped end wall (23
           model units in at its top, 90 at its foot). They stay as they are.
  stack    the square stack at the north-east has its north face missing: the one hole
           the console left. Added across the two corners the cartridge already states.
           It stays square (the Remastered stacks are round; the cartridge's own shape
           wins). Its four painted faces fit one light exactly (colour = a + b.normal),
           and the north face takes what that light gives.
  dome     a regular 8-sided frustum, every side and the top modelled, painted flat per
           side. Doubled twice to 32 sides on its fitted circles, re-shaded as a whole
           from one smooth fit of its own painted sides, and every side corner remapped
           by the cartridge's own projection (u linear in x, v linear in height), so the
           new corners pushed out onto the circle carry the texel they sit on. The top is
           rebuilt as a fan from the axis over the 32-sided rim, with the top's own planar
           mapping (the half trefoil mirrored across u = 1) and its flat colour.
  vent     the regular pentagonal cone on the west arm's roof: doubled to 10 sides,
           re-shaded as a whole, its top rebuilt as a fan like the dome's.
The dome's and the vent's open bottoms sit 2 and 5 model units under the roof they
stand on, and the cross's and west block's bottoms rest on the ground: never seen.
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
doc, objs = B.build_cartridge(src, "BIO")
body = objs[0]

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")
bm.normal_update()

BODY, STACK, DOME, DOME_TOP, VENT, VENT_TOP = range(1, 7)
# parts by cartridge triangle index (extract.py order)
BY_TRI = {}
for ids, p in (((7, 18, 30, 31, 34, 35, 36, 37), STACK),
               ((8, 9, 10, 11, 12, 13, 14, 15, 16, 19, 20, 21, 22, 23, 32, 33), DOME),
               ((24, 25, 26, 27, 28, 29), DOME_TOP),
               ((38, 39, 40, 86, 87, 88, 91, 92, 93, 94), VENT),
               ((95, 96, 97), VENT_TOP)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI.get(f[tri], BODY)


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def mean_colour(faces):
    s = [0.0] * 4
    n = 0
    for f in faces:
        for lp in f.loops:
            for i in range(4):
                s[i] += lp[col][i]
            n += 1
    return tuple(x / max(n, 1) for x in s)


report = {"cartridge_triangles": sum(p["ntri"] for p in doc["parts"]),
          "cartridge_drawn": len(bm.faces)}


# ------------------------------------------------------------------ stack
def light_fit(faces):
    """Least-squares colour = a + b.normal over the given flat-painted faces: the one
    light that best explains a box's painted sides. Returns colour(normal), rms."""
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


stack = faces_of(STACK)
sv = verts_of(stack)
y_n = max(v.co.y for v in sv)
north = [v for v in sv if abs(v.co.y - y_n) < 1e-4]
nc = sum((v.co for v in north), Vector()) / max(len(north), 1)
north.sort(key=lambda v: math.atan2(v.co.z - nc.z, v.co.x - nc.x))
if len(north) != 4 or bm.faces.get(north) is not None:
    raise SystemExit("stack: expected its four north corners and no north face")
stack_light, stack_rms = light_fit(stack)
nf = bm.faces.new(north)
nf.normal_update()
if nf.normal.y < 0:
    nf.normal_flip()
like = stack[0]
nf[part] = STACK
nf[tex] = like[tex]
nf.material_index = like.material_index
north_col = stack_light(Vector((0.0, 1.0, 0.0)))
for lp in nf.loops:
    lp[uvl].uv = like.loops[0][uvl].uv          # the stack samples one texel throughout
    lp[col] = north_col
report["stack_light_fit_rms"] = round(stack_rms * 255, 2)
report["stack_north_colour"] = round(north_col[0] * 255, 1)


# ------------------------------------------------------------------ round parts
def cap_fan(ring, centre, z, t, mat, fpart, uv_of, colour):
    """A flat cap over a horizontal ring of vertices, fanned from a new vertex on the
    axis, facing up, with pack texture t and material slot mat. Returns the new faces."""
    ring = sorted(ring, key=lambda v: R.angle_of(v.co, centre))
    hub = bm.verts.new((centre[0], centre[1], z))
    new = []
    for a, b in zip(ring, ring[1:] + ring[:1]):
        f = bm.faces.new([hub, a, b])
        f.normal_update()
        if f.normal.z < 0:
            f.normal_flip()
        f[part] = fpart
        f[tex] = t
        f.material_index = mat
        for lp in f.loops:
            lp[uvl].uv = uv_of(lp.vert.co)
            lp[col] = colour
        new.append(f)
    return new


def round_off(side_p, top_p, doublings, max_deg, remap_side_uv):
    """Double a frustum's sides `doublings` times onto its fitted circles, re-shade the
    sides from one smooth fit of their own painted colours, and rebuild the top as an
    axis fan with the top's own planar mapping and mean colour. Heights, not vertex
    references, are carried across the densify: it rebuilds the faces it cuts."""
    side, top = faces_of(side_p), faces_of(top_p)
    zs = [v.co.z for v in verts_of(side)]
    z_top, z_bot = max(zs), min(zs)
    foot = [v.co for v in verts_of(side) if abs(v.co.z - z_bot) < 1e-3]
    c = R.fit_circle([(p.x, p.y) for p in foot])
    shade = R.smooth_ring_shade(side, col, c)
    top_uv, top_rms = R.fit_uv(top, uvl, lambda co: (co.x, co.y, 1.0))
    side_uv, side_rms = R.fit_uv(side, uvl, lambda co: (co.x, co.z, 1.0))
    top_col = mean_colour(top)
    t, mat = top[0][tex], top[0].material_index
    n_before = len(side)
    bmesh.ops.delete(bm, geom=top, context="FACES_ONLY")
    for _ in range(doublings):
        R.densify_curve(bm, faces_of(side_p), c, max_deg=max_deg)
    side = faces_of(side_p)
    R.apply_colour(side, col, shade)
    if remap_side_uv:
        for f in side:
            for lp in f.loops:
                lp[uvl].uv = side_uv(lp.vert.co)
    ring = [v for v in verts_of(side) if abs(v.co.z - z_top) < 1e-3]
    cap = cap_fan(ring, c, z_top, t, mat, top_p, top_uv, top_col)
    return dict(axis=[round(x, 4) for x in c], sides=len(ring), side_tris=n_before,
                side_tris_after=len(side), top_uv_rms=round(top_rms, 4),
                side_uv_rms=round(side_rms, 4), top_tris=len(cap))


# the dome: 8 regular sides (45 degrees each, so the chord test needs a wider limit
# than densify_curve's default), 32 after two doublings. Its sides are mapped by one
# projection (u linear in x, v in height), so every corner is remapped by it.
report["dome"] = round_off(DOME, DOME_TOP, 2, 60.0, True)
# the vent: 5 regular sides (72 degrees each), 10 after one doubling. Each of its sides
# samples its own small patch of the concrete, so its corners interpolate instead.
report["vent"] = round_off(VENT, VENT_TOP, 1, 90.0, False)
report["winding_fixed"] = R.agree_winding(bm, faces_of(STACK, DOME, DOME_TOP, VENT, VENT_TOP))

bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["fixed_triangles"] = len(bm.faces)
report["ratio"] = round(report["fixed_triangles"] / float(report["cartridge_triangles"]), 2)
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "BIO_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
