"""The Guard Tower's repair plan: the cartridge model, completed.

    blender -b --python tools/art/n64fix/fix_GTWR.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/GTWR_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building: a
square lookout on four splayed square legs, turned 45 degrees so its corners point to
the compass, under a low pyramid roof.

  lean    the whole model is sheared, not upright: the cabin's top square sits 47 model
          units north of its bottom square (the roof apex carries on the same slope) and
          the east and west legs' feet sit 69 units south of their tops. Every mirror and
          copy below is taken level by level about that leaning axis, so a completed side
          leans with the rest.
  cabin   an open lookout: a parapet from the floor up to 430, a post at each corner up
          to the roof, and a window between each pair of posts. The walls are single
          surfaces on one square. The console camera sees the south-west and south-east
          walls from outside and, through their windows, the north-east and north-west
          walls from inside, so those two were modelled facing inward, and the north post
          was never modelled at all. Repaired as thin walls, a card 16 units thick: the
          outer north-east and north-west walls are the south-east and south-west walls
          mirrored north to south (carrying their UVs and colours); the inward-facing
          cartridge walls move 16 units in to become the inner side; the north post's
          inner strips copy the east post's; the inner side of the south walls is the
          inner side of the north walls mirrored; every window gets a sill and two jambs
          16 units deep. A ceiling under the roof, a floor raised 6 units off the slab's
          underside, and the underside itself close the box.
  legs    each leg is a square frustum with a corner on the cabin's corner (the cartridge
          states the two outer faces of the south leg at a right angle, and one face of the
          east and west legs). The missing outer faces are mirrors across the leg's own
          centre line, the inner faces are the outer faces turned half round the leg's
          axis, and the north leg, never modelled, is the south leg turned half round the
          tower, set down on the lean the east and west legs state. Shaded by facing: the
          cartridge paints west-facing faces light and east-facing faces dark, and nothing
          states a north-south difference, so a face takes the colour of its east or west
          counterpart.

There are no round parts, so nothing is densified: every added triangle closes something
the cartridge left open.
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
doc, objs = B.build_cartridge(src, "GTWR")
report = {"cartridge_triangles": sum(p["ntri"] for p in doc["parts"])}
body = objs[0]

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
bm.normal_update()
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

(ROOF, FLOOR, LEG, OUT, IN, CEIL, UNDER, RIM) = range(1, 9)
BY_TRI = {}
for ids, p in (((0, 1, 2, 3), ROOF), ((4, 5), FLOOR),
               ((6, 7, 8, 9, 10, 11, 12, 15), LEG),
               ((13, 14, 16, 17, 18, 19, 20, 28, 29, 30, 31, 32), OUT),
               ((21, 22, 23, 24, 25, 26, 27), IN)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI[f[tri]]


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


# ------------------------------------------------------------------ the pack frame
# Positions below are written in the pack's frame (x east, y up, z south, 1024 to the
# cell), where the cartridge's numbers can be read directly.
SC = 1.0 / B.CELL
SHEAR = 47.0 / 267.0      # the cabin's lean: z per unit of height, measured top to bottom
GAP = 16.0                # wall thickness of the completed cabin
Y_FLOOR = 310.0           # the floor, raised off the underside at 304
R2 = math.sqrt(2.0)


def P(x, y, z):
    return Vector((x * SC, -z * SC, y * SC))


def K(co):
    return (co.x * B.CELL, co.z * B.CELL, -co.y * B.CELL)


def DIR(dx, dz, dy=0.0):
    return Vector((dx, -dz, dy))


def cz(y):
    """The cabin's centre line: z of the tower axis at height y."""
    return -54.5 - SHEAR * (y - 304.0)


def ns_mirror(co):
    """Mirror north to south across the leaning east-west plane through the axis."""
    x, y, z = K(co)
    return P(x, y, 2.0 * cz(y) - z)


def x_mirror(co):
    x, y, z = K(co)
    return P(-x, y, z)


def diag_mirror(co):
    """Mirror across the plane through the axis and the middle of the north-east wall:
    it swaps the east and north corners and keeps the north-east wall in place."""
    x, y, z = K(co)
    a, b = x, z - cz(y)
    return P(-b, y, cz(y) - a)


def inward(co, g=GAP):
    """A point of the outer wall square moved `g` into the cabin: along the wall's
    inward normal, or along the diagonal at a corner (where both walls move in)."""
    x, y, z = K(co)
    a, b = x, z - cz(y)
    if abs(a) < 10.0:
        dx, dz = 0.0, (-g * R2 if b > 0 else g * R2)
    elif abs(b) < 10.0:
        dx, dz = (-g * R2 if a > 0 else g * R2), 0.0
    else:
        dx, dz = -math.copysign(g / R2, a), -math.copysign(g / R2, b)
    return P(x + dx, y, z + dz)


def near(p, pool, tol):
    best = min(pool, key=lambda v: (v.co - p).length)
    return best if (best.co - p).length <= tol * SC else None


def new_face(vs, like, prt, uvs=None, colour=None, facing=None):
    """A face on `vs` with `like`'s material and texture. `uvs` one per corner, colour
    one value for all corners, `facing` a direction its normal must agree with."""
    f = bm.faces.new(vs)
    f.material_index = like.material_index
    f[tex] = like[tex]
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


def grey(v):
    return (v / 255.0, v / 255.0, v / 255.0, 1.0)


def mirror_snap(faces, mirror, candidates, snap, prt=None):
    """Mirror copy of `faces` through the position map `mirror`, reversed so it faces
    the mirrored way. Each mirrored corner lands on the nearest of `candidates` within
    `snap` model units, otherwise on a new vertex (which later copies may snap to). UVs
    and colours are carried from the source corner. Returns (new faces, worst snap)."""
    made = {}
    pool = list(candidates)
    worst = 0.0

    def vert_for(v):
        nonlocal worst
        if v in made:
            return made[v]
        p = mirror(v.co)
        best = near(p, pool, snap) if pool else None
        if best is not None:
            worst = max(worst, (best.co - p).length * B.CELL)
            made[v] = best
        else:
            made[v] = bm.verts.new(p)
            pool.append(made[v])
        return made[v]

    new = []
    for f in faces:
        vs = [vert_for(v) for v in reversed(f.verts)]
        nf = bm.faces.new(vs)
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        nf[part] = f[part] if prt is None else prt
        nf[tri] = -1
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo[uvl].uv
            ln[col] = lo[col]
        new.append(nf)
    return new, worst


def plane_uv(face, feats):
    """The affine texture map one cartridge face states, as a function of position."""
    X = np.asarray([feats(lp.vert.co) for lp in face.loops], float)
    Y = np.asarray([tuple(lp[uvl].uv) for lp in face.loops], float)
    M = np.linalg.solve(X, Y)
    return lambda co: Vector(tuple(np.asarray(feats(co), float) @ M))


def ne_coords(co):
    """(along the north-east wall, height, 1): a face frame for that wall."""
    x, y, z = K(co)
    return (x + (z - cz(y)), y, 1.0)


conc = faces_of(OUT)[0]            # a concrete face: material and texture to copy
floor_like = faces_of(FLOOR)[0]

# ------------------------------------------------------------------ floor, recorded
floor_uv, rms_floor = R.fit_uv(faces_of(FLOOR), uvl, lambda co: (co.x, co.y, 1.0))
floor_col = tuple(faces_of(FLOOR)[0].loops[0][col])
report["floor_uv_fit_rms"] = round(rms_floor, 4)
floor_mat = (floor_like.material_index, floor_like[tex])
bmesh.ops.delete(bm, geom=faces_of(FLOOR), context="FACES_ONLY")

# ------------------------------------------------------------------ inner walls, north
# the cartridge's north-east and north-west walls face into the cabin: they become the
# inner side, moved GAP in, with their bottom edge on the raised floor. Their original
# positions stay behind as snap targets for the outer walls built next.
inner_src = faces_of(IN)
copy = {}
for f in inner_src:
    vs = []
    for v in f.verts:
        if v not in copy:
            copy[v] = bm.verts.new(v.co)
        vs.append(copy[v])
    nf = bm.faces.new(vs)
    nf.material_index = f.material_index
    nf[tex] = f[tex]
    nf[part] = IN
    nf[tri] = f[tri]
    for lo, ln in zip(f.loops, nf.loops):
        ln[uvl].uv = lo[uvl].uv
        ln[col] = lo[col]
bmesh.ops.delete(bm, geom=inner_src, context="FACES_ONLY")
for v in copy.values():
    p = inward(v.co)
    x, y, z = K(p)
    v.co = P(x, max(y, Y_FLOOR), z)
inner_verts = set(copy.values())

# ------------------------------------------------------------------ outer walls, north
outer_pool = [v for v in bm.verts if v not in inner_verts]
new_out, worst = mirror_snap(faces_of(OUT), ns_mirror, outer_pool, 12.0)
report["outer_north_faces"] = len(new_out)
report["outer_north_snap_units"] = round(worst, 1)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")

out_v = verts_of(faces_of(OUT))


def ov(x, y, z):
    v = near(P(x, y, z), out_v, 3.0)
    assert v is not None, (x, y, z)
    return v


# the windows' outer corners: (post A bottom, post A top, post B bottom, post B top); on
# the north walls post A is the north post
WIN_SW = [ov(-37, 430, 164), ov(-37, 571, 141), ov(-243, 430, -42), ov(-243, 571, -65)]
WIN_SE = [ov(37, 430, 165), ov(37, 571, 141), ov(242, 430, -39), ov(241, 571, -60)]
WIN_NE = [near(ns_mirror(v.co), out_v, 12.0) for v in WIN_SE]
WIN_NW = [near(ns_mirror(v.co), out_v, 12.0) for v in WIN_SW]
assert all(WIN_NE) and all(WIN_NW)

# ------------------------------------------------------------------ inner walls: posts
in_v = verts_of(faces_of(IN))


def iv_of(outer_vert, pool=None):
    p = inward(outer_vert.co)
    pool = in_v if pool is None else pool
    v = near(p, pool, 2.0) if pool else None
    if v is None:
        v = bm.verts.new(p)
        in_v.append(v)
    return v


t21 = [f for f in faces_of(IN) if f[tri] == 21][0]
t22 = [f for f in faces_of(IN) if f[tri] == 22][0]
t24 = [f for f in faces_of(IN) if f[tri] == 24][0]
map21 = plane_uv(t21, ne_coords)
NE_IN = DIR(-1.0, 1.0)
NW_IN = DIR(1.0, 1.0)
post_col = tuple(t21.loops[0][col])

# the east post on the north-east wall: the cartridge drew one triangle of its inner
# strip; the other continues the same texture map
e_in_top = near(inward(ov(288, 571, -104).co), in_v, 3.0)
ewin_in_bot = iv_of(WIN_NE[2])
ewin_in_top = iv_of(WIN_NE[3])
fill = new_face([e_in_top, ewin_in_top, ewin_in_bot], t21, IN,
                uvs=[map21(v.co) for v in (e_in_top, ewin_in_top, ewin_in_bot)],
                colour=post_col, facing=NE_IN)


def split_at(face, a, b, target):
    """Cut the edge a-b of `face` where `target` projects onto it, and move the cut to
    `target`. The loops' UV and colour interpolate at the cut."""
    e = bm.edges.get((a, b))
    d = b.co - a.co
    fac = max(0.05, min(0.95, (target - a.co).dot(d) / d.length_squared))
    _, nv = bmesh.utils.edge_split(e, a, fac)
    nv.co = target
    return nv


# the north post, never modelled: on the north-east wall its inner strip stands on the
# parapet (which the cartridge ran up to the north corner) and copies the east post's
# strip across the wall's middle
n_in_mid = near(inward(P(0, 430, -361)), in_v, 3.0)
assert n_in_mid is not None
n_in_top = iv_of(ov(0, 571, -384))
nwin_ne_bot = split_at(t22, ewin_in_bot, n_in_mid, inward(WIN_NE[0].co))
nwin_ne_top = iv_of(WIN_NE[1])
in_v.append(nwin_ne_bot)
strip_vs = [n_in_mid, n_in_top, nwin_ne_top, nwin_ne_bot]
strip_ne = new_face(strip_vs, t21, IN, uvs=[map21(diag_mirror(v.co)) for v in strip_vs],
                    colour=post_col, facing=NE_IN)
# on the north-west wall, the same strip mirrored east to west
wwin_in_bot = near(inward(WIN_NW[2].co), in_v, 12.0)
nwin_nw_bot = split_at(t24, n_in_mid, wwin_in_bot, inward(WIN_NW[0].co))
in_v.append(nwin_nw_bot)
strip_nw, _ = mirror_snap([strip_ne], x_mirror, in_v, 3.0, IN)
report["north_inner_faces_added"] = 2 + len(strip_nw)  # east post fill, north post strips

# ------------------------------------------------------------------ inner walls, south
# the inner side of the south-east and south-west walls is the north walls' inner side
# mirrored north to south; its window corners land GAP inside the outer ones
targets = [iv_of(v, pool=[]) for v in WIN_SE + WIN_SW]
pool = [v for v in verts_of(faces_of(IN))] + targets
new_in, worst = mirror_snap(faces_of(IN), ns_mirror, pool, 12.0)
report["inner_south_faces"] = len(new_in)
report["inner_south_snap_units"] = round(worst, 1)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")

# ------------------------------------------------------------------ window rims
in_v = verts_of(faces_of(IN))
out_faces = faces_of(OUT)


def out_loop(v, other):
    """The loop at outer corner `v` in the outer face that holds the edge v-other."""
    for f in out_faces:
        if v in f.verts and other in f.verts:
            return [lp for lp in f.loops if lp.vert is v][0]
    return [lp for lp in v.link_loops if lp.face[part] == OUT][0]


rims = []
for key, (a0, a1, b0, b1) in (("sw", WIN_SW), ("se", WIN_SE), ("ne", WIN_NE), ("nw", WIN_NW)):
    ia0, ia1, ib0, ib1 = (near(inward(v.co), in_v, 3.0) for v in (a0, a1, b0, b1))
    assert None not in (ia0, ia1, ib0, ib1), key
    # the sill stretches the parapet's top texel inward, in the parapet's colour
    la, lb = out_loop(a0, b0), out_loop(b0, a0)
    rims.append(new_face([a0, b0, ib0, ia0], la.face, RIM,
                         uvs=[la[uvl].uv, lb[uvl].uv, lb[uvl].uv, la[uvl].uv],
                         colour=tuple(la[col]), facing=Vector((0, 0, 1))))
    # the jambs stretch the post's edge texel; lit like a post facing the same way
    for (p0, p1, i0, i1, other) in ((a0, a1, ia0, ia1, b0), (b0, b1, ib0, ib1, a0)):
        l0, l1 = out_loop(p0, p1), out_loop(p1, p0)
        facing = other.co - p0.co
        facing.z = 0.0
        shade = grey(240 if facing.x < 0 else 90)
        rims.append(new_face([p0, p1, i1, i0], l0.face, RIM,
                             uvs=[l0[uvl].uv, l1[uvl].uv, l1[uvl].uv, l0[uvl].uv],
                             colour=shade, facing=facing))
report["rim_faces"] = len(rims)

# ------------------------------------------------------------------ floor, underside, ceiling
corner = {}
for name, (x, z) in (("s", (0, 228)), ("e", (281, -54)), ("n", (0, -337)), ("w", (-283, -54))):
    corner[name] = ov(x, 304, z)
top = {}
for name, (x, z) in (("s", (0, 181)), ("e", (288, -104)), ("n", (0, -384)), ("w", (-283, -101))):
    top[name] = near(P(x, 571, z), verts_of(faces_of(ROOF)), 3.0)


def floor_pt(v):
    x, _, z = K(inward(v.co))
    return P(x, Y_FLOOR, z)


fl = [near(floor_pt(corner[n]), in_v, 3.0) for n in "senw"]
assert None not in fl


def conc_uv(co):
    """Concrete for faces the cartridge never mapped: the wall texture laid flat at
    about the walls' own texel density, centred under the tower."""
    x, y, z = K(co)
    return Vector((0.5 + x / 700.0, 0.5 - (z + 101.0) / 700.0))


floor = new_face(fl, conc, FLOOR, uvs=[Vector(floor_uv(v.co)) for v in fl],
                 colour=floor_col, facing=Vector((0, 0, 1)))
floor.material_index, floor[tex] = floor_mat
dark = grey(90)
under = new_face([corner[n] for n in "senw"], conc, UNDER,
                 uvs=[conc_uv(corner[n].co) for n in "senw"], colour=dark,
                 facing=Vector((0, 0, -1)))
ceil = new_face([top[n] for n in "senw"], conc, CEIL,
                uvs=[conc_uv(top[n].co) for n in "senw"], colour=dark,
                facing=Vector((0, 0, -1)))

# ------------------------------------------------------------------ legs
# corners per leg, pack frame: O outer, L and Rt left and right seen from outside,
# I inner; top square at the cabin's underside, foot square on the ground
LEGS = {
    "s": {"top": {"O": (0, 304, 228), "L": (-109, 307, 118), "Rt": (108, 307, 119),
                  "I": (0, 304, 9)},
          "foot": {"O": (0, 1, 374), "L": (-67, 1, 306), "Rt": (67, 1, 306), "I": (0, 1, 238)}},
    "e": {"top": {"O": (281, 304, -54), "L": (174, 307, -160), "Rt": (174, 307, 52),
                  "I": (67, 304, -54)},
          "foot": {"O": (372, 1, 15), "L": (305, 1, -51), "Rt": (305, 1, 81), "I": (238, 1, 15)}},
    "w": {"top": {"O": (-283, 304, -54), "L": (-173, 307, 54), "Rt": (-173, 307, -162),
                  "I": (-63, 304, -54)},
          "foot": {"O": (-373, 1, 15), "L": (-305, 1, 82), "Rt": (-305, 1, -52), "I": (-237, 1, 15)}},
}
# the north leg: the south leg turned half round the axis at the top; at the foot the
# lean the east and west legs state (69 south over the leg's height) plus the mean of the
# outward reach of the south leg (118.5) and of the east and west legs (131.5)
lean, reach = 69.0, 0.5 * (118.5 + 131.5)
n_top_c = 2.0 * cz(304) - 118.5
n_foot_c = n_top_c - reach + lean
LEGS["n"] = {"top": {"O": (0, 304, -337), "L": (108.5, 307, n_top_c), "Rt": (-108.5, 307, n_top_c),
                     "I": (0, 304, n_top_c + 109.5)},
             "foot": {"O": (0, 1, n_foot_c - 67.5), "L": (67.5, 1, n_foot_c),
                      "Rt": (-67.5, 1, n_foot_c), "I": (0, 1, n_foot_c + 67.5)}}
report["north_leg_foot_centre_z"] = round(n_foot_c, 1)

all_v = list(bm.verts)
cv = {}
for lg, lv in LEGS.items():
    for lvl, pts in lv.items():
        for role, p in pts.items():
            v = near(P(*p), all_v, 3.0)
            if v is None:
                v = bm.verts.new(P(*p))
                all_v.append(v)
            cv[(lg, lvl, role)] = v
role_of = {v: k for k, v in cv.items()}

# the cartridge's leg faces, by leg and side
leg_faces = {}
for f in faces_of(LEG):
    keys = [role_of[v] for v in f.verts]
    lg = keys[0][0]
    side = "OL" if any(k[2] == "L" for k in keys) else "OR"
    leg_faces.setdefault((lg, side), []).append(f)


def copy_leg(src_faces, dst_leg, rmap, reverse):
    new = []
    for f in src_faces:
        loops = list(reversed(f.loops)) if reverse else list(f.loops)
        vs = []
        for lp in loops:
            _, lvl, role = role_of[lp.vert]
            vs.append(cv[(dst_leg, lvl, rmap[role])])
        nf = bm.faces.new(vs)
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        nf[part] = LEG
        nf[tri] = -1
        for lo, ln in zip(loops, nf.loops):
            ln[uvl].uv = lo[uvl].uv
            ln[col] = lo[col]
        new.append(nf)
    return new


ID = {"O": "O", "L": "L", "Rt": "Rt", "I": "I"}
SWAP_LR = {"O": "O", "L": "Rt", "Rt": "L", "I": "I"}
HALF_TURN = {"O": "I", "I": "O", "L": "Rt", "Rt": "L"}
new_legs = []
# missing outer faces: the leg's other outer face mirrored across its centre line, or
# for the north leg the south leg's own faces turned half round the tower
new_legs += copy_leg(leg_faces[("e", "OR")], "e", SWAP_LR, True)
new_legs += copy_leg(leg_faces[("w", "OL")], "w", SWAP_LR, True)
new_legs += copy_leg(leg_faces[("s", "OL")], "n", ID, False)
new_legs += copy_leg(leg_faces[("s", "OR")], "n", ID, False)
leg_faces[("e", "OL")] = new_legs[0:2]
leg_faces[("w", "OR")] = new_legs[2:4]
leg_faces[("n", "OL")] = new_legs[4:6]
leg_faces[("n", "OR")] = new_legs[6:8]
# inner faces: each outer face turned half round the leg's own axis
for lg in "senw":
    new_legs += copy_leg(leg_faces[(lg, "OL")] + leg_faces[(lg, "OR")], lg, HALF_TURN, False)
report["leg_faces_added"] = len(new_legs)
# shade by facing: west-facing takes the leg's (or its mirror twin's) west colour
WEST = {"s": 240, "n": 240, "w": 220, "e": 220}
EAST = {"s": 135, "n": 135, "w": 135, "e": 135}
bm.normal_update()
for f in faces_of(LEG):
    lg = role_of[f.verts[0]][0]
    c = grey(WEST[lg] if f.normal.x < 0 else EAST[lg])
    for lp in f.loops:
        lp[col] = c

# ------------------------------------------------------------------ tidy and count
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
report["winding_fixed_final"] = R.agree_winding(bm, [f for f in bm.faces if f[tri] < 0])
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "GTWR_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
