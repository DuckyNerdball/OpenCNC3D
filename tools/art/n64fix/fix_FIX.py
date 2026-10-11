"""The Repair Bay's repair plan: the cartridge model, rounded and doubled.

    blender -b --python tools/art/n64fix/fix_FIX.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/FIX_fixed.blend; run.sh then puts it
in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building, which
shows an octagonal striped collar round a flat pad floor, with four low plates at the
compass points under the collar's outer edge:

  collar  an octagon 108 to 112 model units wide and 100 high, closed on every side the
          cartridge needs (top, inner wall, the four diagonal outer walls). Nothing was
          missing; what reads as blocky is its square section. Rebuilt on the cartridge's own
          sixteen corners with both top edges rounded, each by a two-segment quarter round
          (31 units on the inner lip, 20 on the outer), mitred at the octagon's corners. The
          Remastered collar shows both lips bevelled. The octagon stays an octagon: every
          reference draws it with eight straight sides.
  plates  the four sloped plates at north, east, south and west, also closed. Their outer
          top edge is rounded the same way (12 units), and their inner edge now meets the
          foot of the collar's outer round, 20 units lower than before, so the plates read
          as a step below the collar as they do in the Remastered frames.
  floor   the cartridge's floor is a square turned 45 degrees, laid under the collar at
          height 5. Its edges cut across the octagon's inner corners, so at each of the
          eight a sliver of ground shows between the floor and the inner wall. Rebuilt as
          the octagon itself, welded to the foot of the inner wall (raised to the floor's
          height), with the square's own planar mapping and a linear fit of its four
          painted corner colours.

Texture: every new corner takes the UV the cartridge's own mapping gives at its place
across the collar or plate (a bilinear blend of the cartridge's corner UVs), and walls keep
the cartridge's dragged mapping (a wall corner carries the UV of the top corner above it).
Colour: the tops keep the cartridge's 255; every vertical wall is shaded by one fit of the
cartridge's painted walls against their facing (lit from the south-west, 100 to 150), and a
round blends from the top's colour to that wall colour across its two segments.

Only the bottoms resting on the ground stay open.
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
doc, objs = B.build_cartridge(src, "FIX")
body = objs[0]
report = {"cartridge_triangles": len(doc["tris"])}

U = 1.0 / B.CELL             # one model unit in Blender cells
R_IN, R_OUT, R_PLATE = 31 * U, 20 * U, 12 * U
K_ARC = 1.0 - 1.0 / math.sqrt(2.0)   # the middle of a quarter round, as a share of its legs
TEX_RING, TEX_PLATE, TEX_FLOOR = 15, 50, 51
RING, PLATE, FLOOR = 1, 2, 3


# ------------------------------------------------------------------ helpers
def face_toward(f, d):
    f.normal_update()
    if f.normal.dot(d) < 0:
        f.normal_flip()


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


def boundary_loops(bm):
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


def offset_corner(p_prev, p, p_next, dist, centre):
    """The mitred corner of a convex polygon's outline offset by `dist` away from
    `centre` (negative: toward it): the meeting point of the two offset sides, in XY."""
    def normal(a, b):
        t = Vector((b.x - a.x, b.y - a.y))
        n = Vector((t.y, -t.x)).normalized()
        m = Vector(((a.x + b.x) / 2 - centre.x, (a.y + b.y) / 2 - centre.y))
        return n if n.dot(m) > 0 else -n
    n0, n1 = normal(p_prev, p), normal(p, p_next)
    A = np.array([[n0.x, n0.y], [n1.x, n1.y]])
    b = np.array([n0.dot(p.xy) + dist, n1.dot(p.xy) + dist])
    x, y = np.linalg.solve(A, b)
    return Vector((x, y))


def quarter_mid(corner, a, c):
    """The middle of the quarter round that replaces the edge at `corner` between legs
    ending at a (down the wall) and c (along the top)."""
    return corner + K_ARC * ((a - corner) + (c - corner))


def wall_shade_fit(faces, col):
    """One linear fit of the cartridge's painted wall colour against the wall's facing:
    colour = k0 + k1 nx + k2 ny, clipped to the range the cartridge painted."""
    X, Y = [], []
    for f in faces:
        n = Vector((f.normal.x, f.normal.y)).normalized()
        for lp in f.loops:
            X.append((1.0, n.x, n.y))
            Y.append(tuple(lp[col]))
    X, Y = np.asarray(X), np.asarray(Y)
    k = np.linalg.lstsq(X, Y, rcond=None)[0]
    lo, hi = Y.min(0), Y.max(0)
    rms = float(np.sqrt(((X @ k - Y)[:, :3] ** 2).mean()) * 255)

    def colour(nx, ny):
        n = Vector((nx, ny))
        n = n.normalized() if n.length > 1e-9 else Vector((0.0, 0.0))
        return tuple(float(v) for v in np.clip(np.array([1.0, n.x, n.y]) @ k, lo, hi))
    return colour, rms


# ------------------------------------------------------------------ the cartridge mesh
bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.5 * U)
uvl, col, tex = R.layers(bm)
part = bm.faces.layers.int.new("fix_part")
bm.normal_update()

ring_f = [f for f in bm.faces if f[tex] == TEX_RING]
plate_f = [f for f in bm.faces if f[tex] == TEX_PLATE]
floor_f = [f for f in bm.faces if f[tex] == TEX_FLOOR]
floor_z = floor_f[0].verts[0].co.z
floor_uv, floor_rms = R.fit_uv(floor_f, uvl, lambda co: (co.x, co.y, 1.0))
report["floor_uv_fit_rms"] = round(floor_rms, 5)
_X = np.asarray([(lp.vert.co.x, lp.vert.co.y, 1.0) for f in floor_f for lp in f.loops])
_Y = np.asarray([tuple(lp[col]) for f in floor_f for lp in f.loops])
_K = np.linalg.lstsq(_X, _Y, rcond=None)[0]


def floor_colour(co):
    return tuple(float(c) for c in np.clip(np.array([co.x, co.y, 1.0]) @ _K, _Y.min(0), _Y.max(0)))


floor_like = (floor_f[0][tex], floor_f[0].material_index)

ring_top = [f for f in ring_f if f.normal.z > 0.9]
ring_top_v = list({v for f in ring_top for v in f.verts})
centre = sum((v.co for v in ring_top_v), Vector()) / len(ring_top_v)
report["centre_units"] = "x %.0f y %.0f" % (centre.x * B.CELL, centre.y * B.CELL)


def ang(p):
    return math.atan2(p.y - centre.y, p.x - centre.x)


def uv_table(faces):
    """Each vertex's UV in the given faces, checked to be one UV per vertex."""
    t, worst = {}, 0.0
    for f in faces:
        for lp in f.loops:
            uv = lp[uvl].uv.copy()
            if lp.vert in t:
                worst = max(worst, (t[lp.vert] - uv).length)
            t[lp.vert] = uv
    return t, worst


ring_uv, ring_uv_split = uv_table(ring_top)
report["collar_uv_split"] = round(ring_uv_split, 4)
ring_col = tuple(ring_top[0].loops[0][col])

# the sixteen collar corners: eight on the inner edge, eight on the outer, paired by angle
rad = {v: (v.co.xy - centre.xy).length for v in ring_top_v}
mid_r = 0.5 * (min(rad.values()) + max(rad.values()))
inner = sorted([v for v in ring_top_v if rad[v] < mid_r], key=lambda v: ang(v.co))
outer = sorted([v for v in ring_top_v if rad[v] >= mid_r], key=lambda v: ang(v.co))
assert len(inner) == 8 and len(outer) == 8, (len(inner), len(outer))
pair_err = max(abs((ang(a.co) - ang(b.co) + math.pi) % (2 * math.pi) - math.pi)
               for a, b in zip(inner, outer))
report["collar_pairing_worst_deg"] = round(math.degrees(pair_err), 1)
widths = []
N = 8
for k in range(N):
    a, b = inner[k].co, inner[(k + 1) % N].co
    t = (b - a).xy.normalized()
    n = Vector((t.y, -t.x))
    widths.append(abs((outer[k].co.xy - a.xy).dot(n)))
report["collar_width_units"] = "%.0f..%.0f" % (min(widths) * B.CELL, max(widths) * B.CELL)
ground_z = min(v.co.z for f in ring_f for v in f.verts)

# the plates: each is one sloped quad, two corners on the collar's outer edge, two outside
plate_top = [f for f in plate_f if f.normal.z > 0.3]
plate_uv, plate_uv_split = uv_table(plate_top)
report["plate_uv_split"] = round(plate_uv_split, 4)
plate_col = tuple(plate_top[0].loops[0][col])
plate_top_v = list({v for f in plate_top for v in f.verts})
plates = []
for k in range(N):
    # a plate sits on collar side k when both of that side's outer corners are its corners
    ends = []
    for j in (k, (k + 1) % N):
        hit = [v for v in plate_top_v if (v.co.xy - outer[j].co.xy).length < 2 * U]
        ends.append(hit[0] if hit else None)
    # every outer corner is some plate's corner, so it is a plate side only when one
    # plate triangle holds both of them
    if None in ends or not any(set(ends) <= set(f.verts) for f in plate_top):
        continue
    # the plate's own quad: its outer corners are the two nearest its collar corners
    quad_faces = [f for f in plate_top if set(f.verts) & set(ends)]
    quad_v = {v for f in quad_faces for v in f.verts} - set(ends)
    o = [min(quad_v, key=lambda v: (v.co.xy - e.co.xy).length) for e in ends]
    plates.append({"side": k, "ring": [k, (k + 1) % N], "rv": ends, "ov": o})
report["plates"] = len(plates)
plate_sides = {p["side"] for p in plates}

# one wall shade for the whole building, from every painted vertical wall
walls = [f for f in ring_f + plate_f if abs(f.normal.z) < 0.1]
wall_colour, wall_rms = wall_shade_fit(walls, col)
report["wall_shade_fit_rms"] = round(wall_rms, 1)

# remember everything needed, then clear the lot
I = [v.co.copy() for v in inner]
O = [v.co.copy() for v in outer]
UI = [ring_uv[v].copy() for v in inner]
UO = [ring_uv[v].copy() for v in outer]
for p in plates:
    p["R"] = [v.co.copy() for v in p["rv"]]
    p["O"] = [v.co.copy() for v in p["ov"]]
    p["UR"] = [plate_uv[v].copy() for v in p["rv"]]
    p["UO"] = [plate_uv[v].copy() for v in p["ov"]]
ring_like = (ring_top[0][tex], ring_top[0].material_index)
plate_like = (plate_top[0][tex], plate_top[0].material_index)
bmesh.ops.delete(bm, geom=ring_f + plate_f + floor_f, context="FACES_ONLY")
for v in [v for v in bm.verts if not v.link_faces]:
    bm.verts.remove(v)

# ------------------------------------------------------------------ collar
# per corner k, the section from the inner wall's foot round to the outer wall's foot;
# each point carries (position, share across the collar for its UV, weight toward the
# wall colour for its shade)
section = []
for k in range(N):
    pk, nk = (k - 1) % N, (k + 1) % N
    zi, zo = I[k].z, O[k].z
    ci = offset_corner(I[pk], I[k], I[nk], R_IN, centre)       # inset along the top
    co = offset_corner(O[pk], O[k], O[nk], -R_OUT, centre)
    w_k = (O[k].xy - I[k].xy).length
    ti = (ci - I[k].xy).length / w_k                            # share across, from inside
    to = 1.0 - (co - O[k].xy).length / w_k
    Ai = Vector((I[k].x, I[k].y, zi - R_IN))
    Ci = Vector((ci.x, ci.y, zi + (zo - zi) * ti))
    Ao = Vector((O[k].x, O[k].y, zo - R_OUT))
    Co = Vector((co.x, co.y, zi + (zo - zi) * to))
    pts = [(Vector((I[k].x, I[k].y, floor_z)), 0.0, 1.0),
           (Ai, 0.0, 1.0),
           (quarter_mid(I[k], Ai, Ci), K_ARC * ti, 0.5),
           (Ci, ti, 0.0),
           (Co, to, 0.0),
           (quarter_mid(O[k], Ao, Co), 1.0 - K_ARC * (1.0 - to), 0.5),
           (Ao, 1.0, 1.0),
           (Vector((O[k].x, O[k].y, ground_z)), 1.0, 1.0)]
    section.append([(bm.verts.new(p), s, w) for p, s, w in pts])

up = Vector((0, 0, 1))


def shade_face(f, top_colour, weight):
    """Top colour on the flat tops, the fitted wall colour on the walls, and a blend by
    each corner's weight on the rounds between them."""
    f.normal_update()
    n = f.normal
    wall = wall_colour(n.x, n.y)
    for lp in f.loops:
        if abs(n.z) < 0.1:
            w = 1.0
        elif n.z > 0.95:
            w = 0.0
        else:
            w = weight[lp.vert]
        lp[col] = tuple(top_colour[i] * (1 - w) + wall[i] * w for i in range(4))


def new_face(vs, like, uv, top_colour, weight, d, which):
    f = bm.faces.new(vs)
    f[tex], f.material_index = like
    f[part] = which
    face_toward(f, d)
    for lp in f.loops:
        lp[uvl].uv = uv[lp.vert]
    shade_face(f, top_colour, weight)
    return f


ring_uv_of, ring_w = {}, {}
for k in range(N):
    for v, s, w in section[k]:
        ring_uv_of[v] = UI[k].lerp(UO[k], s)
        ring_w[v] = w

new_ring = []
for k in range(N):
    a, b = section[k], section[(k + 1) % N]
    mid = 0.5 * (I[k] + I[(k + 1) % N])
    inward = Vector((centre.x - mid.x, centre.y - mid.y, 0.0)).normalized()
    last = 7 if k not in plate_sides else 6       # the outer wall only where no plate is
    for j in range(last):
        d = [inward, inward + up, inward + up, up, -inward + up, -inward + up, -inward][j]
        q = [a[j][0], b[j][0], b[j + 1][0], a[j + 1][0]]
        new_ring.append(new_face(q, ring_like, ring_uv_of, ring_col, ring_w, d, RING))
report["collar_faces"] = len(new_ring)

# the floor: the inner wall's feet, as one flat octagon
fl = bm.faces.new([section[k][0][0] for k in range(N)])
fl[tex], fl.material_index = floor_like
fl[part] = FLOOR
face_toward(fl, up)
for lp in fl.loops:
    lp[uvl].uv = floor_uv(lp.vert.co)
    lp[col] = floor_colour(lp.vert.co)

# ------------------------------------------------------------------ plates
new_plate = []
for p in plates:
    k0, k1 = p["ring"]
    Rv = [section[k0][6][0], section[k1][6][0]]      # the collar's outer round foot
    Gv = [section[k0][7][0], section[k1][7][0]]      # the collar's outer wall foot
    Oc = p["O"]
    edge = (Oc[1] - Oc[0]).xy.normalized()
    nrm = Vector((edge.y, -edge.x))
    pc = 0.5 * (Oc[0] + Oc[1])
    outward = Vector((pc.x - centre.x, pc.y - centre.y))
    if nrm.dot(outward) < 0:
        nrm = -nrm
    out3 = Vector((nrm.x, nrm.y, 0.0))
    uv, wt, Cv, Bv, Av, GOv = {}, {}, [], [], [], []
    for j in (0, 1):
        o, r = Oc[j], Rv[j].co
        depth = (o.xy - r.xy).dot(nrm)                  # how far the collar corner lies in
        t = R_PLATE / depth
        c = o.lerp(r, t)
        a = Vector((o.x, o.y, o.z - R_PLATE))
        bmid = quarter_mid(o, a, c)
        cv, bv, av = bm.verts.new(c), bm.verts.new(bmid), bm.verts.new(a)
        gv = bm.verts.new((o.x, o.y, ground_z))
        uo, ur = p["UO"][j], p["UR"][j]
        uv.update({Rv[j]: ur, Gv[j]: ur, cv: uo.lerp(ur, t), bv: uo.lerp(ur, K_ARC * t),
                   av: uo, gv: uo})
        wt.update({Rv[j]: 0.0, Gv[j]: 1.0, cv: 0.0, bv: 0.5, av: 1.0, gv: 1.0})
        Cv.append(cv)
        Bv.append(bv)
        Av.append(av)
        GOv.append(gv)
    fs = [new_face([Rv[0], Rv[1], Cv[1], Cv[0]], plate_like, uv, plate_col, wt, up, PLATE),
          new_face([Cv[0], Cv[1], Bv[1], Bv[0]], plate_like, uv, plate_col, wt, up + out3, PLATE),
          new_face([Bv[0], Bv[1], Av[1], Av[0]], plate_like, uv, plate_col, wt, up + out3, PLATE),
          new_face([Av[0], Av[1], GOv[1], GOv[0]], plate_like, uv, plate_col, wt, out3, PLATE)]
    for j in (0, 1):
        end_dir = (Oc[j] - Oc[1 - j]).xy.normalized()
        along = (Rv[j].co - Oc[j]).xy.normalized()
        side = Vector((along.y, -along.x))
        if side.dot(end_dir) < 0:
            side = -side
        fs.append(new_face([Gv[j], Rv[j], Cv[j], Bv[j], Av[j], GOv[j]], plate_like, uv,
                           plate_col, wt, Vector((side.x, side.y, 0.0)), PLATE))
    new_plate += fs
report["plate_faces"] = len(new_plate)

# ------------------------------------------------------------------ checks
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3],
                      quad_method="BEAUTY", ngon_method="BEAUTY")
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "FIX_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
