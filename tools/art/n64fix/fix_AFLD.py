"""The Airstrip's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_AFLD.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/AFLD_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
and checked against the Remastered building: a long runway strip running east whose
west end is a raised landing pad with a rounded end, a control tower on a sloped
pedestal behind the pad's north-west corner, a radar behind the pad, and posts and a
pipe along the pad's south foot. Pack coordinates in the comments are x east, y up,
z south, 1024 units to the cell.

  pad     the landing pad (top at 182 units, walls sloping out about 54 units at the
          foot) and the runway ramp east of it have their south walls only. The north
          walls are mirror copies of the south ones across the strip's long axis,
          carrying their UVs and colour; the new feet keep the south walls' flare.
          The pad's west end is a half octagon (a flat west face and two chamfers).
          Each of its three edges is cut into six along one smooth curve through the
          cartridge's own corners, tangent to the straight long sides, so the end reads
          as the rounded end every reference shows; the walls under it are rebuilt as
          strips on the same curve, mapped as the walls they replace.
  arrow   the grey chevron floats 37 units above the pad top with nothing under its
          edges: a skirt down to the pad closes it.
  tower   the cabin and its flared lip are whole. The sloped pedestal under them has
          only its two south faces, and it leans: its top is centred 61 units north of
          its foot. The north faces are the south ones mirrored through the leaning
          centre plane, which meets the existing edges to within 4 units.
  radar   the pyramid mount behind the pad has only its two south faces: mirrored
          across its own centre plane. The screen on it (part 1, it turns on its pivot)
          is a bent three-panel plate with a front and side and top rims but no back
          and no bottom rim: both built from the back rim the cartridge does state.
          Then each panel is cut in two on a smooth curve through the columns, so the
          bend reads as a curve.
  props   the four posts and the pipe along the south foot are open at the back where
          they lean on the pad wall: closed with their convex hulls.
  lights  the two red plates at the east end (parts 2 and 3) are flat, face up and are
          animated; they stay as they are.
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
doc, objs = B.build_cartridge(src, "AFLD")
report = {"cartridge_triangles": sum(p["ntri"] for p in doc["parts"])}


# ------------------------------------------------------------------ helpers
def P(x, y, z):
    """A pack position (x east, y up, z south) in Blender cells."""
    return Vector(B.to_blender((x, y, z)))


def find(bm, x, y, z, tol=8.0):
    """The vertex at a pack position, to within `tol` model units."""
    p = P(x, y, z)
    v = min(bm.verts, key=lambda w: (w.co - p).length)
    if (v.co - p).length > tol / B.CELL:
        raise SystemExit("no vertex near (%g, %g, %g)" % (x, y, z))
    return v


def open_bm(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
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


def open_edge_count(bm):
    return sum(1 for e in bm.edges if e.is_boundary)


def mirror_snap(bm, faces, mirror, candidates, snap):
    """Mirror copy of `faces` through the position map `mirror`, reversed so it faces
    outward again. Each mirrored corner lands on the nearest vertex of `candidates`
    within `snap`, otherwise on a new vertex. UVs and colours are carried from the
    source corner. Returns (new faces, worst snap distance). As in fix_EYE.py."""
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


def hermite(p0, p1, t0, t1, s):
    """Cubic Hermite point at parameter s between p0 and p1 with end tangents t0, t1."""
    h00 = 2 * s ** 3 - 3 * s ** 2 + 1
    h10 = s ** 3 - 2 * s ** 2 + s
    h01 = -2 * s ** 3 + 3 * s ** 2
    h11 = s ** 3 - s ** 2
    return p0 * h00 + t0 * h10 + p1 * h01 + t1 * h11


def curve_through(chain, lead_in, lead_out, cuts):
    """Points along a smooth curve through `chain` (a list of positions), `cuts` new
    points inside each segment. The curve leaves chain[0] in the direction lead_in and
    arrives at chain[-1] in the direction lead_out (unit vectors, so it continues a
    straight side tangentially); inside it is Catmull-Rom, so every chain point stays
    where it was. Returns one list of new points per segment."""
    n = len(chain)
    tang = []
    for i in range(n):
        if i == 0:
            tang.append(lead_in * (chain[1] - chain[0]).length)
        elif i == n - 1:
            tang.append(lead_out * (chain[-1] - chain[-2]).length)
        else:
            tang.append((chain[i + 1] - chain[i - 1]) * 0.5)
    segs = []
    for i in range(n - 1):
        segs.append([hermite(chain[i], chain[i + 1], tang[i], tang[i + 1], k / (cuts + 1.0))
                     for k in range(1, cuts + 1)])
    return segs


def catmull_mid(chain, i):
    """The Catmull-Rom midpoint of segment i of an open chain (ends reflected)."""
    p1, p2 = chain[i], chain[i + 1]
    p0 = chain[i - 1] if i > 0 else 2 * p1 - p2
    p3 = chain[i + 2] if i + 2 < len(chain) else 2 * p2 - p1
    return (-p0 + 9 * p1 + 9 * p2 - p3) / 16.0


# ------------------------------------------------------------------ body
body = objs[0]
bm = open_bm(body)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")
FLOOR, ARROW, PROP, TPED, TOWER, RPED, RAMP, PADTOP, PADWALL, NEWWALL, SKIRT = range(1, 12)
for f in bm.faces:
    i = f[tri]
    f[part] = (FLOOR if i in (0, 1) else ARROW if i in (2, 3) else PROP if 4 <= i <= 63
               else TPED if i in (64, 65, 82, 83) else TOWER if 66 <= i <= 91 and i not in range(84, 90)
               else RPED if 84 <= i <= 89 else RAMP if i in (92, 93)
               else PADTOP if i in (95, 96, 97, 98, 99, 101) else PADWALL)
by_tri = {f[tri]: f for f in bm.faces}


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(*parts):
    return list({v for f in faces_of(*parts) for v in f.verts})


# ------------------------------------------------------------------ pad and ramp: north walls
# the strip's long axis, from the three pairs of top corners the south and north edges
# both state (the pad's chamfer corners, the pad's mid corners, the pad's east corners)
pairs = [((-1533, 182, 849), (-1528, 182, -258)), ((-956, 182, 929), (-956, 182, -358)),
         ((-381, 182, 935), (-380, 182, -375))]
axis_y = sum((find(bm, *s).co.y + find(bm, *n).co.y) * 0.5 for s, n in pairs) / len(pairs)
report["strip_axis_z_units"] = round(-axis_y * B.CELL, 1)
S0, S0b = find(bm, -956, 182, 929), find(bm, -1009, 0, 952)
SE, SEb = find(bm, -381, 182, 935), find(bm, -381, 0, 953)
N0, NE = find(bm, -956, 182, -358), find(bm, -380, 182, -375)


def flip(d):
    return Vector((d.x, -d.y, d.z))


# the two new feet keep the flare of the south feet they mirror, measured from the top
# corner the mirror lands on, so the north walls slope out exactly as the south ones do
feet = {S0b: N0.co + flip(S0b.co - S0.co), SEb: NE.co + flip(SEb.co - SE.co)}


def strip_mirror(co):
    for v, p in feet.items():
        if (v.co - co).length < 1e-6:
            return p.copy()
    return Vector((co.x, 2 * axis_y - co.y, co.z))


south = [by_tri[i] for i in (103, 104, 100, 110, 102)]
cand = verts_of(PADTOP, PADWALL, RAMP)
north, miss = mirror_snap(bm, south, strip_mirror, cand, 60.0 / B.CELL)
for f in north:
    f[part] = NEWWALL
report["north_wall_faces"] = len(north)
report["north_wall_snap_units"] = round(miss * B.CELL, 1)
N0b = min(bm.verts, key=lambda v: (v.co - feet[S0b]).length)

# ------------------------------------------------------------------ pad: the rounded west end
NW, W1, W2, SW = (find(bm, -1528, 182, -258), find(bm, -1930, 182, 65),
                  find(bm, -1929, 182, 500), find(bm, -1533, 182, 849))
NWb, W1b, W2b, SWb = (find(bm, -1582, 0, -258), find(bm, -1984, 0, 65),
                      find(bm, -1983, 0, 500), find(bm, -1587, 0, 872))
CUTS = 5
top_chain = [NW, W1, W2, SW]
bot_chain = [NWb, W1b, W2b, SWb]


def lead(a, b):
    d = b.co - a.co
    d.z = 0.0
    return d.normalized()


top_pts = curve_through([v.co.copy() for v in top_chain], lead(N0, NW), lead(SW, S0), CUTS)
bot_pts = curve_through([v.co.copy() for v in bot_chain], lead(N0b, NWb), lead(SWb, S0b), CUTS)
report["pad_end_bulge_units"] = round(max(
    (top_pts[1][CUTS // 2] - (W1.co + W2.co) * 0.5).length,
    (top_pts[0][CUTS // 2] - (NW.co + W1.co) * 0.5).length) * B.CELL, 1)


def corner_uv(f, v):
    return next(lp[uvl].uv.copy() for lp in f.loops if lp.vert is v)


def corner_col(f, v):
    return tuple(next(lp[col] for lp in f.loops if lp.vert is v))


def face_with(faces, *vs):
    return next(f for f in faces if all(v in f.verts for v in vs))


pad_top = faces_of(PADTOP)
# the three end walls, north-west, west and south-west, two triangles each
wall_pairs = [[by_tri[i] for i in pr] for pr in ((105, 106), (94, 107), (108, 109))]
old_walls = [f for pr in wall_pairs for f in pr]
wall_info = []
for i in range(3):
    a, b, a2, b2 = top_chain[i], top_chain[i + 1], bot_chain[i], bot_chain[i + 1]
    ftop = face_with(pad_top, a, b)
    fwa = face_with(wall_pairs[i], a, a2)
    fwb = face_with(wall_pairs[i], b, b2)
    wall_info.append(dict(
        top=(ftop, corner_uv(ftop, a), corner_uv(ftop, b), corner_col(ftop, a), corner_col(ftop, b)),
        wall=(fwa, corner_uv(fwa, a), corner_uv(fwb, b), corner_uv(fwa, a2), corner_uv(fwb, b2),
              corner_col(fwa, a), corner_col(fwb, b), corner_col(fwa, a2), corner_col(fwb, b2))))
wall_mat = (old_walls[0].material_index, old_walls[0][tex])
bmesh.ops.delete(bm, geom=old_walls, context="FACES_ONLY")


def lerp(p, q, t):
    return p * (1 - t) + q * t


def lerpc(p, q, t):
    return tuple(p[k] * (1 - t) + q[k] * t for k in range(4))


# a point inside the pad's west end, to tell a wall's outside from its inside
pad_c = sum((v.co for v in top_chain), Vector()) / 4.0
pad_c.x = 0.5 * (pad_c.x + N0.co.x)
end_faces = []
for i in range(3):
    a, b, a2, b2 = top_chain[i], top_chain[i + 1], bot_chain[i], bot_chain[i + 1]
    tv = [a] + [bm.verts.new(p) for p in top_pts[i]] + [b]
    bv = [a2] + [bm.verts.new(p) for p in bot_pts[i]] + [b2]
    ts = [k / (CUTS + 1.0) for k in range(CUTS + 2)]
    ftop, uva, uvb, ca, cb = wall_info[i]["top"]
    # the sliver between the old chord and the curve: a fan from the segment's start,
    # mapped and coloured as the chord it grows from
    for k in range(1, CUTS + 1):
        f = bm.faces.new([a, tv[k], tv[k + 1]])
        f[part] = PADTOP
        f[tex] = ftop[tex]
        f.material_index = ftop.material_index
        for lp in f.loops:
            t = ts[tv.index(lp.vert)]
            lp[uvl].uv = lerp(uva, uvb, t)
            lp[col] = lerpc(ca, cb, t)
        f.normal_update()
        if f.normal.z < 0:
            f.normal_flip()
        end_faces.append(f)
    fw, ua, ub, ua2, ub2, cwa, cwb, cwa2, cwb2 = wall_info[i]["wall"]
    for k in range(CUTS + 1):
        q = bm.faces.new([tv[k], tv[k + 1], bv[k + 1], bv[k]])
        q[part] = PADWALL
        q.material_index, q[tex] = wall_mat
        for lp in q.loops:
            if lp.vert in tv:
                t = ts[tv.index(lp.vert)]
                lp[uvl].uv = lerp(ua, ub, t)
                lp[col] = lerpc(cwa, cwb, t)
            else:
                t = ts[bv.index(lp.vert)]
                lp[uvl].uv = lerp(ua2, ub2, t)
                lp[col] = lerpc(cwa2, cwb2, t)
        q.normal_update()
        m = q.calc_center_median()
        if q.normal.dot(Vector((m.x - pad_c.x, m.y - pad_c.y, 0.0))) < 0:
            q.normal_flip()
        end_faces.append(q)
report["pad_end_segments"] = 3 * (CUTS + 1)

# ------------------------------------------------------------------ arrow: a skirt to the pad
arrow = faces_of(ARROW)
pad_z = NW.co.z
arrow_edges = [e for e in bm.edges if e.is_boundary and all(f[part] == ARROW for f in e.link_faces)]
drop = {}
for e in arrow_edges:
    for v in e.verts:
        if v not in drop:
            drop[v] = bm.verts.new((v.co.x, v.co.y, pad_z))
acol = [lp[col] for f in arrow for lp in f.loops][0]
skirt_col = (acol[0] * 0.6, acol[1] * 0.6, acol[2] * 0.6, acol[3])
skirt = []
for e in arrow_edges:
    a, b = e.verts
    q = bm.faces.new([a, b, drop[b], drop[a]])
    q[part] = SKIRT
    q.material_index, q[tex] = arrow[0].material_index, arrow[0][tex]
    for lp in q.loops:
        lp[col] = skirt_col
    # run the shared edge the opposite way to the arrow face above it
    above = next(f for f in e.link_faces if f is not q)
    la = next(lp for lp in above.loops if lp.edge is e)
    lq = next(lp for lp in q.loops if lp.edge is e)
    if la.vert is lq.vert:
        q.normal_flip()
    skirt.append(q)
report["arrow_skirt_faces"] = len(skirt)

# ------------------------------------------------------------------ tower pedestal
# its top is centred north of its foot, so the mirror plane leans with it: a point at
# height h is reflected across the centre line at that height
Sb, Wb, Eb = find(bm, -768, 0, -361), find(bm, -1002, 0, -597), find(bm, -534, 0, -593)
St, Wt, Et, Nt = (find(bm, -768, 469, -468), find(bm, -956, 469, -658),
                  find(bm, -580, 469, -654), find(bm, -768, 469, -844))
c_foot = 0.5 * (Wb.co.y + Eb.co.y)
c_top = 0.25 * (Wt.co.y + Et.co.y + St.co.y + Nt.co.y)
h_top = St.co.z
report["tower_lean_units"] = round((c_top - c_foot) * B.CELL, 1)


def lean_mirror(co):
    c = c_foot + (c_top - c_foot) * co.z / h_top
    return Vector((co.x, 2 * c - co.y, co.z))


ped = faces_of(TPED)
new_ped, miss = mirror_snap(bm, ped, lean_mirror, verts_of(TPED, TOWER), 15.0 / B.CELL)
for f in new_ped:
    f[part] = TPED
report["tower_pedestal_added"] = len(new_ped)
report["tower_pedestal_snap_units"] = round(miss * B.CELL, 1)

# ------------------------------------------------------------------ radar mount
rv = verts_of(RPED)
top = [v for v in rv if v.co.z > 0.3]
c_r = 0.5 * (min(v.co.y for v in top) + max(v.co.y for v in top))
rped = faces_of(RPED)
sides = [f for f in rped if abs(f.normal.z) < 0.9]
new_r, miss = mirror_snap(bm, sides, lambda co: Vector((co.x, 2 * c_r - co.y, co.z)), rv, 15.0 / B.CELL)
for f in new_r:
    f[part] = RPED
report["radar_mount_added"] = len(new_r)
report["radar_mount_snap_units"] = round(miss * B.CELL, 1)
report["body_winding_fixed"] = R.agree_winding(bm, north + end_faces + skirt + new_ped + new_r)


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


prop_new = 0
for g in islands(faces_of(PROP)):
    new, rms = R.close_convex(bm, g, skip_below_z=0.03)
    for f in new:
        if f.is_valid:
            f[part] = PROP
            prop_new += 1
report["prop_hull_faces"] = prop_new
report["body_open_edges"] = open_edge_count(bm)
report["body_triangles"] = close_bm(bm, body)


# ------------------------------------------------------------------ radar screen (part 1)
screen = objs[1]
bm = open_bm(screen)
uvl, col, tex = R.layers(bm)
Fb = [find(bm, *p) for p in ((-140, 441, -440), (-63, 413, -496), (102, 415, -559), (191, 443, -563))]
Ft = [find(bm, *p) for p in ((-149, 547, -472), (-72, 567, -521), (93, 566, -584), (181, 543, -591))]
Bt = [find(bm, *p) for p in ((-154, 544, -485), (-78, 564, -537), (88, 564, -597), (175, 542, -607))]
Bb = [find(bm, -147, 437, -459), None, None, find(bm, 187, 440, -574)]
# the back's two missing bottom corners: the front corner moved by the same thickness
# the column's top corner is moved by
for i in (1, 2):
    Bb[i] = bm.verts.new(Fb[i].co + (Bt[i].co - Ft[i].co))
ref = next(iter(bm.faces))


def vcol(v):
    return tuple(next(iter(v.link_loops))[col]) if v.link_loops else None


front_col = {Fb[i]: vcol(Fb[i]) for i in range(4)}
front_col.update({Ft[i]: vcol(Ft[i]) for i in range(4)})
made = []
for i in range(3):
    # the back, shaded from the front's own painted columns at 0.6
    q = bm.faces.new([Bb[i], Bb[i + 1], Bt[i + 1], Bt[i]])
    for lp in q.loops:
        j = (Bb if lp.vert in Bb else Bt).index(lp.vert)
        c = front_col[(Fb if lp.vert in Bb else Ft)[j]]
        lp[col] = (c[0] * 0.6, c[1] * 0.6, c[2] * 0.6, c[3])
    made.append(q)
    # the bottom rim, coloured per column like the top rim the cartridge draws
    q = bm.faces.new([Fb[i], Fb[i + 1], Bb[i + 1], Bb[i]])
    for lp in q.loops:
        j = (Fb if lp.vert in Fb else Bb).index(lp.vert)
        lp[col] = front_col[Fb[j]]
    made.append(q)
for q in made:
    q.material_index = ref.material_index
    q[tex] = ref[tex]
report["screen_closing_faces"] = len(made)
# the cartridge winds the screen's rims inconsistently with its front (16 edges
# disagree once it is closed); closed, it has an inside, so every face is turned out
bm.normal_update()
before = [f.normal.copy() for f in bm.faces]
bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
bm.normal_update()
report["screen_faces_turned_out"] = sum(1 for f, n in zip(bm.faces, before) if f.normal.dot(n) < 0)
report["screen_open_edges_closed"] = open_edge_count(bm)

# the bend: every edge between two neighbouring columns is cut, and the cut goes onto
# the Catmull-Rom curve through that row's four corners
targets = {}
edges = []
for chain in (Fb, Ft, Bt, Bb):
    pts = [v.co.copy() for v in chain]
    for i in range(3):
        e = next(e for e in chain[i].link_edges if e.other_vert(chain[i]) is chain[i + 1])
        edges.append(e)
        targets[tuple(round(x, 6) for x in 0.5 * (pts[i] + pts[i + 1]))] = catmull_mid(pts, i)
res = bmesh.ops.subdivide_edges(bm, edges=edges, cuts=1, use_grid_fill=False)
moved = 0
for v in [g for g in res["geom_inner"] if isinstance(g, bmesh.types.BMVert)]:
    k = min(targets, key=lambda q: (Vector(q) - v.co).length)
    if (Vector(k) - v.co).length < 1e-5:
        v.co = targets[k]
        moved += 1
report["screen_columns_added"] = moved
report["screen_open_edges"] = open_edge_count(bm)
report["screen_triangles"] = close_bm(bm, screen)

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "AFLD_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
