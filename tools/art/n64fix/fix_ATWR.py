"""The Advanced Guard Tower's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_ATWR.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/ATWR_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building, a
box with a striped panel, a grey collar, a windowed cab on top, a mast behind and two
dishes:

  frame   the whole model is tilted back about 7.4 degrees and turned so two box faces
          (south-west and south-east) face the south camera. In that frame every ring of
          the box, collar and cab is a parallelogram: the roof's four corners satisfy
          E + W - S = N to within 2 model units.
  box     only the two south walls are modelled. The box is 0.39 by 0.55 cells, not
          square, so its symmetry is a half turn about its vertical axis, not a diagonal
          mirror: each missing north wall is the half-turn copy of the south wall
          opposite, carrying that wall's UVs and baked colour.
  panel   the striped panel stands proud of the south-west wall with its east side
          modelled; its west side is the mirror of the east side (same UVs), its top a
          cap under the collar.
  collar  the grey band under the cab, and the cab (lower walls and windows), are
          completed by the same half turn. The roof quad is already whole. The small
          window bay on the south-east face is one feature and is not copied. The collar
          and cab are centred south-east of the box's centre, so on the north the box's
          top shows beside the collar (capped flat) and the cab's foot overhangs the
          collar's inner edge (a ledge closes the slit).
  mast    a tapered three-sided spike with its north-west side missing: closed with its
          convex hull. The two antenna prongs (arm and spike each) are closed side by
          side along the cartridge's own diagonals; each arm-to-spike joint stays internal.
  dishes  the two dishes (separate animated parts, pivots kept) are 8-sided bowls with no
          back. In each dish's own frame they are doubled to 16 sides, re-shaded with one
          smooth fit of their painted fan, and given a thin closed back. The big dish's
          one-triangle feed needle becomes a three-sided spike.
"""
import itertools
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402
import repair as R  # noqa: E402

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
B.reset_scene()
doc, objs = B.build_cartridge(src, "ATWR")
body = objs[0]

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")
bm.normal_update()

# parts by cartridge triangle index (extract.py order)
(BAY, WIN_SW, WIN_SE, ROOF, CAB_SW, CAB_SE, COL_SW, COL_SE, WALL_SW, WALL_SE, PANEL,
 PANEL_SIDE, MAST, ARM_A, SPIKE_A, ARM_B, SPIKE_B, BOX_TOP, CAB_LEDGE) = range(1, 20)
BY_TRI = {}
for ids, p in (((0, 1, 2, 3, 4, 5, 6, 7), BAY), ((8, 9), WIN_SW), ((10, 11), WIN_SE),
               ((12, 13), ROOF), ((39, 40), CAB_SW), ((36, 38), CAB_SE),
               ((19, 21), COL_SW), ((14, 15), COL_SE), ((42, 43), WALL_SW),
               ((37, 41), WALL_SE), ((44, 45), PANEL), ((46, 47), PANEL_SIDE),
               ((20, 29, 30, 31, 32), MAST), ((16, 17, 18, 33, 34), ARM_A),
               ((22, 23), SPIKE_A), ((24, 25, 26, 27), ARM_B), ((28, 35), SPIKE_B)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI[f[tri]]


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


report = {"cartridge_triangles": len(bm.faces)}

# the model's own up: the box's vertical edges (south corner, bottom to top)
sw_v, se_v = set(verts_of(faces_of(WALL_SW))), set(verts_of(faces_of(WALL_SE)))
corner = sorted(sw_v & se_v, key=lambda v: v.co.z)
up = (corner[1].co - corner[0].co).normalized()
report["tilt_deg"] = round(math.degrees(math.acos(max(-1.0, min(1.0, up.z)))), 2)


# the south-west wall's plane, taken before its north twin (same part) exists
wall_pts = [v.co.copy() for v in verts_of(faces_of(WALL_SW))[:3]]


# ------------------------------------------------------------------ half turn
def complete_by_half_turn(sw_parts, se_parts, snap=4.0 / B.CELL):
    """Complete a four-sided part whose two south faces are modelled. The vertices only
    the south-west faces use are its W corners, only the south-east faces its E corners,
    both its S corners; ranked by height along `up` they pair into rings, and each ring's
    missing corner is N = E + W - S (the ring is a parallelogram). The half turn maps
    S to N and swaps E and W, so every south face has a north twin with the same winding,
    UVs and baked colour. A computed N within `snap` of an existing vertex is that vertex
    (the roof's north corner is modelled)."""
    fa, fb = faces_of(*sw_parts), faces_of(*se_parts)
    va, vb = set(verts_of(fa)), set(verts_of(fb))
    h = lambda v: v.co.dot(up)  # noqa: E731
    S = sorted(va & vb, key=h)
    E = sorted(vb - va, key=h)
    W = sorted(va - vb, key=h)
    if not (len(S) == len(E) == len(W)):
        raise SystemExit("half turn: rings do not pair (S %d E %d W %d)" % (len(S), len(E), len(W)))
    everything = verts_of(list(bm.faces))
    vmap = {}
    snapped = 0
    for s, e, w in zip(S, E, W):
        n = e.co + w.co - s.co
        old = min(everything, key=lambda v: (v.co - n).length)
        if (old.co - n).length < snap:
            vmap[s] = old
            snapped += 1
        else:
            vmap[s] = bm.verts.new(n)
        vmap[e], vmap[w] = w, e
    new = []
    for f in fa + fb:
        vs = [vmap[v] for v in f.verts]
        if bm.faces.get(vs) is not None:
            continue
        nf = bm.faces.new(vs)
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        nf[part] = f[part]
        for lo, ln in zip(f.loops, nf.loops):
            ln[uvl].uv = lo[uvl].uv
            ln[col] = lo[col]
        new.append(nf)
    return new, snapped


new_box, _ = complete_by_half_turn([WALL_SW], [WALL_SE])
new_col, _ = complete_by_half_turn([COL_SW], [COL_SE])
new_cab, cab_snapped = complete_by_half_turn([CAB_SW, WIN_SW], [CAB_SE, WIN_SE])
report["half_turn_faces"] = "box %d collar %d cab %d" % (len(new_box), len(new_col), len(new_cab))
# the roof's north corner is modelled: the cab's top ring must land on it
report["cab_corners_snapped"] = cab_snapped

# the box's top: the collar is centred on the cab, not on the box, so on the north the
# box's top edge stands outside the completed collar and the open top shows. Capped
# flat, mapped and shaded like the collar that sits on it.
box_v = verts_of(faces_of(WALL_SW, WALL_SE))
mid_h = sum(v.co.dot(up) for v in box_v) / len(box_v)
top = [v for v in box_v if v.co.dot(up) > mid_h]
tc_ = sum((v.co for v in top), Vector()) / len(top)
ax = (top[0].co - tc_).normalized()
ay = up.cross(ax)
top.sort(key=lambda v: math.atan2((v.co - tc_).dot(ay), (v.co - tc_).dot(ax)))
collar = faces_of(COL_SW, COL_SE)
col_uv, _ = R.fit_uv(collar, uvl, lambda co: (co.x, co.y, co.z, 1.0))
col_mean = [sum(lp[col][k] for f in collar for lp in f.loops) / (3.0 * len(collar)) for k in range(4)]
lid = bm.faces.new(top)
lid.normal_update()
if lid.normal.dot(up) < 0:
    lid.normal_flip()
lid.material_index = collar[0].material_index
lid[tex] = collar[0][tex]
lid[part] = BOX_TOP
for lp in lid.loops:
    lp[uvl].uv = col_uv(lp.vert.co)
    lp[col] = tuple(col_mean)

# the cab's underside: the cab's foot ring overhangs the collar's inner edge (they
# share only the east corner), and from a low eastern view the slit between them shows.
# Closed with a ledge from one ring to the other, mapped and shaded like the cab's
# lower walls in shadow.
cab_low = faces_of(CAB_SW, CAB_SE)
cab_v = verts_of(cab_low)
foot = sorted(cab_v, key=lambda v: v.co.dot(up))[:4]
col_v = verts_of(collar)
edge_in = sorted(col_v, key=lambda v: -v.co.dot(up))[:4]
fc = sum((v.co for v in foot), Vector()) / 4.0
foot.sort(key=lambda v: math.atan2((v.co - fc).dot(ay), (v.co - fc).dot(ax)))
mate_in = {v: min(edge_in, key=lambda w: (w.co - v.co).length) for v in foot}
cab_uv, _ = R.fit_uv(cab_low, uvl, lambda co: (co.x, co.y, co.z, 1.0))
cab_mean = [sum(lp[col][k] for f in cab_low for lp in f.loops) / (3.0 * len(cab_low)) for k in range(4)]
ledge = []
for i in range(4):
    a, b = foot[i], foot[(i + 1) % 4]
    vs = []
    for v in (a, b, mate_in[b], mate_in[a]):
        if v not in vs:
            vs.append(v)
    if len(vs) < 3 or bm.faces.get(vs) is not None:
        continue
    f = bm.faces.new(vs)
    f.material_index = cab_low[0].material_index
    f[tex] = cab_low[0][tex]
    f[part] = CAB_LEDGE
    for lp in f.loops:
        lp[uvl].uv = cab_uv(lp.vert.co)
        lp[col] = (cab_mean[0] * 0.6, cab_mean[1] * 0.6, cab_mean[2] * 0.6, cab_mean[3])
    ledge.append(f)
report["cab_ledge_faces"] = len(ledge)

# ------------------------------------------------------------------ striped panel
# the wall plane it stands on, normal pointing out towards the panel
w0 = wall_pts[0]
wn = (wall_pts[1] - w0).cross(wall_pts[2] - w0).normalized()
front = verts_of(faces_of(PANEL))
if wn.dot(front[0].co - w0) < 0:
    wn = -wn
side = faces_of(PANEL_SIDE)
side_v = set(verts_of(side))
p_east = sorted([v for v in front if v in side_v], key=lambda v: v.co.dot(up))
p_west = sorted([v for v in front if v not in side_v], key=lambda v: v.co.dot(up))
c_back = sorted([v for v in side_v if v not in front], key=lambda v: v.co.dot(up))
feet = [bm.verts.new(v.co - wn * (v.co - w0).dot(wn)) for v in p_west]
report["panel_depth"] = round(sum((v.co - w0).dot(wn) for v in p_west) / 2 * B.CELL, 1)
# the west side mirrors the east side corner for corner: P_east <-> P_west, back <-> feet
mirror = {p_east[0]: p_west[0], p_east[1]: p_west[1], c_back[0]: feet[0], c_back[1]: feet[1]}
panel_new = []
for f in side:
    nf = bm.faces.new([mirror[v] for v in reversed(f.verts)])
    nf.material_index = f.material_index
    nf[tex] = f[tex]
    nf[part] = PANEL_SIDE
    for lo, ln in zip(reversed(f.loops), nf.loops):
        ln[uvl].uv = lo[uvl].uv
        ln[col] = lo[col]
    panel_new.append(nf)
# the top, under the collar: mapped and shaded like the east side's top edge
top_src = {lp.vert: lp for f in side for lp in f.loops}
pf = faces_of(PANEL)[0]
cap = bm.faces.new([p_west[1], p_east[1], c_back[1], feet[1]])
cap.material_index = pf.material_index
cap[tex] = pf[tex]
cap[part] = PANEL_SIDE
cap_uv = {p_west[1]: top_src[p_east[1]], p_east[1]: top_src[p_east[1]],
          c_back[1]: top_src[c_back[1]], feet[1]: top_src[c_back[1]]}
for lp in cap.loops:
    lp[uvl].uv = cap_uv[lp.vert][uvl].uv
    lp[col] = cap_uv[lp.vert][col]
panel_new.append(cap)
report["panel_faces_added"] = len(panel_new)

# ------------------------------------------------------------------ mast and prongs
rms_props = []
new_mast, rms = R.close_convex(bm, faces_of(MAST), skip_below_z=0.43)
for f in new_mast:
    if f.is_valid:
        f[part] = MAST
rms_props.append(round(rms, 4))
def close_prism(faces, far, p):
    """Close a thin open prism (near ring of 3) or pyramid (near ring of 1, the tip) whose
    far end is `far`, a ring of 3 left open because it is a joint with the next piece.
    A side
    quad with one triangle modelled gets the other half along the same diagonal, a side
    with none gets both, and the near end gets its cap. A convex hull does not do here:
    the cartridge's side quads are not planar, the hull splits them along the other
    diagonal and its faces then overlap the modelled ones. Near and far pair up the way
    the modelled faces say (every one must lie on a side quad). New faces are mapped by a
    planar fit of the piece's UVs and shaded like close_convex (mean colour, darker
    facing away from the modelled front). Returns the new faces."""
    near = [v for v in verts_of(faces) if v not in far]
    ring = list(far)
    have = {frozenset(f.verts) for f in faces}
    if len(near) == 3:
        # the pairing under which every modelled face lies on one side quad (the arms
        # taper and twist, so nearest-in-place pairing is not reliable)
        def fits(m):
            sides = [{m[ring[i]], m[ring[(i + 1) % 3]], ring[i], ring[(i + 1) % 3]} for i in range(3)]
            return sum(1 for h in have if any(h <= q for q in sides))
        mate = max((dict(zip(ring, perm)) for perm in itertools.permutations(near)), key=fits)
        if fits(mate) < len(have):
            raise SystemExit("close_prism: the modelled faces fit no side pairing")
    else:
        mate = {b: near[0] for b in ring}
    want = []
    for i in range(3):
        b0, b1 = ring[i], ring[(i + 1) % 3]
        a0, a1 = mate[b0], mate[b1]
        quad = {a0, a1, b0, b1}
        got = [h for h in have if h <= quad]
        if a0 is a1:
            if not got:
                want.append((a0, b0, b1))
        elif len(got) == 1:
            (t,) = got
            diag = {a0, b1} if {a0, b1} <= t else {a1, b0}
            want.append(tuple(quad - (t - diag)))
        elif not got:
            want += [(a0, a1, b1), (a0, b1, b0)]
    if len(near) == 3 and frozenset(near) not in have:
        want.append(tuple(near))
    pred, _ = R.fit_uv(faces, uvl, lambda co: (co.x, co.y, co.z, 1.0))
    front = sum((f.normal * f.calc_area() for f in faces), Vector())
    front = front.normalized() if front.length > 1e-9 else Vector((0, 0, 1))
    mean = [sum(lp[col][k] for f in faces for lp in f.loops) / (3.0 * len(faces)) for k in range(4)]
    new = []
    for vs in want:
        nf = bm.faces.new(vs)
        nf.material_index = faces[0].material_index
        nf[tex] = faces[0][tex]
        nf[part] = p
        new.append(nf)
    R.agree_winding(bm, new)
    for nf in new:
        nf.normal_update()
        shade = 0.55 + 0.45 * max(0.0, nf.normal.dot(front))
        for lp in nf.loops:
            lp[uvl].uv = pred(lp.vert.co)
            lp[col] = (mean[0] * shade, mean[1] * shade, mean[2] * shade, mean[3])
    return new


joints = 0
prong_new = 0
for arm, spike in ((ARM_A, SPIKE_A), (ARM_B, SPIKE_B)):
    joint = set(verts_of(faces_of(arm))) & set(verts_of(faces_of(spike)))
    prong_new += len(close_prism(faces_of(arm), joint, arm))
    prong_new += len(close_prism(faces_of(spike), joint, spike))
    joints += 1
report["prong_faces_added"] = prong_new
report["prong_joints_left_internal"] = joints
report["prop_uv_fit_rms"] = rms_props

report["winding_fixed"] = R.agree_winding(bm, faces_of(WALL_SW, WALL_SE, COL_SW, COL_SE, CAB_SW,
                                                      CAB_SE, WIN_SW, WIN_SE, PANEL_SIDE, MAST,
                                                      ARM_A, SPIKE_A, ARM_B, SPIKE_B, CAB_LEDGE))

bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["body_triangles"] = len(bm.faces)
report["body_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
bm.to_mesh(body.data)
bm.free()
body.data.update()


# ------------------------------------------------------------------ dishes
def dish_frame(faces):
    """The dish's own frame: origin the rim's centre, Z its axis pointing out of the
    bowl (the way its faces face). The rim is the ring farthest from the fan's centre."""
    vs = verts_of(faces)
    hub = max(vs, key=lambda v: len([f for f in v.link_faces if f in faces]))
    rim = sorted(vs, key=lambda v: -(v.co - hub.co).length)[:8]
    P = np.array([tuple(v.co) for v in rim])
    c = P.mean(0)
    _, _, vt = np.linalg.svd(P - c)
    z = Vector(vt[2])
    fn = sum((f.normal * f.calc_area() for f in faces), Vector())
    if z.dot(fn) < 0:
        z = -z
    x = (Vector(tuple(P[0] - c)) - z * Vector(tuple(P[0] - c)).dot(z)).normalized()
    y = z.cross(x)
    rot = Matrix((x, y, z))
    return Vector(tuple(c)), rot, hub


def smooth_disc_shade(faces, centre, rq):
    """One smooth shade for a whole dish, fitted to its painted fan: per ring of equal
    radius (rq bins), colour = a0 + a1 cos(angle) + b1 sin(angle), which is what one
    light across a bowl looks like. Frame: the dish's own, axis Z."""
    rings = {}
    for f in faces:
        for lp in f.loops:
            k = round(R.radius_of(lp.vert.co, centre) / rq)
            rings.setdefault(k, []).append((R.angle_of(lp.vert.co, centre), tuple(lp[col])))
    fits = {}
    for k, s in rings.items():
        A = np.array([[1.0, math.cos(a), math.sin(a)] for a, _ in s])
        Y = np.array([c for _, c in s], float)
        fits[k] = (np.linalg.lstsq(A, Y, rcond=None)[0], Y.min(0), Y.max(0))

    def colour(co):
        k = min(fits, key=lambda q: abs(q - R.radius_of(co, centre) / rq))
        coef, lo, hi = fits[k]
        a = R.angle_of(co, centre)
        return tuple(float(x) for x in np.clip(np.array([1.0, math.cos(a), math.sin(a)]) @ coef, lo, hi))
    return colour


def lens_back(pm, faces, r_rim, depth, shade=0.6):
    """A back for a bowl with none, in the dish's own frame (axis +Z out of the bowl):
    a reversed copy of the bowl whose rim is the bowl's own rim and whose inner rings
    sit `depth` further back. A plain reversed copy (double_side) lies exactly on the
    front and fights it wherever back faces are drawn; this one is a closed shell, thin
    at the rim and `depth` thick at the hub, and costs no more triangles."""
    uvl, col, tex = R.layers(pm)
    rim = {v for f in faces for v in f.verts if R.radius_of(v.co, (0.0, 0.0)) > 0.9 * r_rim}
    vmap = {}
    for f in faces:
        for v in f.verts:
            if v not in vmap:
                vmap[v] = v if v in rim else pm.verts.new(v.co - Vector((0.0, 0.0, depth)))
    new = []
    for f in faces:
        nf = pm.faces.new([vmap[v] for v in reversed(f.verts)])
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo[uvl].uv
            c = lo[col]
            ln[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
        new.append(nf)
    return new


def spike_from_fin(pm, fin, shade=0.6):
    """Turn a one-triangle fin (the feed needle) into a thin three-sided spike: a fourth
    corner behind the base edge, at 0.4 of the base's length, and the three faces that
    close it. UVs and colour come from the fin (the fourth corner takes the base's)."""
    uvl, col, tex = R.layers(pm)
    fin.normal_update()
    ls = list(fin.loops)
    # the tip is the corner farthest from the other two
    tip = max(ls, key=lambda lp: sum((lp.vert.co - o.vert.co).length for o in ls))
    b0, b1 = [lp for lp in ls if lp is not tip]
    base = (b0.vert.co - b1.vert.co).length
    q = pm.verts.new(0.5 * (b0.vert.co + b1.vert.co) - fin.normal * 0.4 * base)
    quv = 0.5 * (Vector(b0[uvl].uv) + Vector(b1[uvl].uv))
    qcol = tuple(0.5 * (b0[col][k] + b1[col][k]) for k in range(4))
    new = []
    for vs in ((b0.vert, q, tip.vert), (q, b1.vert, tip.vert), (b0.vert, b1.vert, q)):
        nf = pm.faces.new(vs)
        nf.material_index = fin.material_index
        nf[tex] = fin[tex]
        new.append(nf)
    R.agree_winding(pm, new)
    src = {b0.vert: b0, b1.vert: b1, tip.vert: tip}
    for nf in new:
        for lp in nf.loops:
            s_ = src.get(lp.vert)
            lp[uvl].uv = s_[uvl].uv if s_ else quv
            c = s_[col] if s_ else qcol
            lp[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
    return new


for ob in objs[1:]:
    pm = bmesh.new()
    pm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(pm, verts=pm.verts, dist=2.0 / B.CELL)
    uvl, col, tex = R.layers(pm)
    ptri = pm.faces.layers.int["pack_tri"]
    ppart = pm.faces.layers.int.new("fix_part")
    pm.normal_update()
    DISH, NEEDLE, BACK = 20, 21, 22
    for f in pm.faces:
        # the big dish's feed needle is one flat triangle (cartridge triangle 66)
        f[ppart] = NEEDLE if f[ptri] == 66 else DISH
    dish = [f for f in pm.faces if f[ppart] == DISH]
    c, rot, hub = dish_frame(dish)
    for v in pm.verts:
        v.co = rot @ (v.co - c)
    pm.normal_update()
    r_rim = max(R.radius_of(v.co, (0.0, 0.0)) for v in verts_of(dish))
    shade = smooth_disc_shade(dish, (0.0, 0.0), r_rim / 6.0)
    sides_before = len(dish)
    R.densify_curve(pm, dish, (0.0, 0.0, r_rim), max_deg=60.0)
    dish = [f for f in pm.faces if f[ppart] == DISH]
    R.apply_colour(dish, col, shade)
    hub_depth = min(v.co.z for v in verts_of(dish))
    # a dish about a twelfth of its radius thick at the hub
    for f in lens_back(pm, dish, r_rim, r_rim / 12.0):
        f[ppart] = BACK
    for fin in [f for f in pm.faces if f[ppart] == NEEDLE]:
        for f in spike_from_fin(pm, fin):
            f[ppart] = NEEDLE
    inv = rot.transposed()
    for v in pm.verts:
        v.co = inv @ v.co + c
    bmesh.ops.triangulate(pm, faces=[f for f in pm.faces if len(f.verts) > 3])
    pm.normal_update()
    report["part%d_dish" % ob["pack_part"]] = "%d->%d front, %d total, rim r %.1f, bowl depth %.1f" % (
        sides_before, len(dish), len(pm.faces), r_rim * B.CELL, -hub_depth * B.CELL)
    pm.to_mesh(ob.data)
    pm.free()
    ob.data.update()

report["fixed_triangles"] = sum(len(o.data.polygons) for o in objs)
report["ratio"] = round(report["fixed_triangles"] / float(sum(p["ntri"] for p in doc["parts"])), 2)

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "ATWR_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
