"""The Communications Center's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_HQ.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/HQ_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
and checked against the Remastered building: a long hall running north-west to south-
east, crossed by a house-coloured block running south-west to north-east, low grey wings
with sloped sides along the hall, a lattice tower at the north-west end, a big dish on a
round pedestal at the front and a small curved radar screen on a post behind the
crossing. Pack coordinates in the comments are x east, y up, z south, 1024 units to the
cell.

  block   the house-coloured block has sloped ends and vertical long sides; its
          north-west long side was never modelled. It is the mirror copy of the south-
          east side across the block's long centre plane, carrying that side's UVs and
          colour, and it meets the four corners the ends already state.
  hall    the long hall (concrete, 238 units high) has its south-west side, its roof and
          its sloped south-east end. Its cross-section is a 307-unit rectangle, so the
          north-east side is the mirror copy of the south-west side. Its vertical north-
          west end stands 9 units inside the lattice tower and shows through the lattice:
          closed with a flat end mapped and shaded like the south-east end.
  wings   the grey sloped wings are two blocks, south-east and north-west, that overlap
          under the hall. The south-east wing is open at its north-west end, the north-
          west wing is two loose sloped sides with no ends and no top. Each open end is
          closed with a flat end mapped and shaded like the south-east wing's own end;
          the north-west wing's top (inside the hall) is closed like the south-east
          wing's top.
  tower   the lattice tower is a thin tapered slab with a rectangular cross-section
          (84 by 445 units at the foot), modelled on its south-west and south-east faces
          and its top. The north-east and north-west faces are the half-turn copies of
          the opposite faces (every ring of the slab is a rectangle, so N = E + W - S),
          carrying their UVs and colour. The lattice is see-through, so every face also
          gets an inner side 5 units inside it.
  posts   the dish's pedestal and the small radar's post are 4-sided pyramids: 8 sides
          each (round in every reference), re-shaded as one.
  dish    (part 1, it turns on its pivot) an 8-sided bowl with a recessed hub and no
          back, and four one-triangle feed struts. In the dish's own frame the bowl goes
          to 16 sides, is re-shaded with one smooth fit of its painted rings and gets a
          closed back that shares the rim. The strut feet follow the rounded rim, and
          each strut becomes a thin three-sided spike.
  radar   (part 2, it turns on its pivot) a bent three-panel plate with a front and side
          and top rims but no back and no bottom rim: both built from the back rim the
          cartridge does state, as on the Airstrip's radar.
"""
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
doc, objs = B.build_cartridge(src, "HQ")
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


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def mesh_check(bm):
    """(open edges, edges whose two faces run the same way round)."""
    opened = sum(1 for e in bm.edges if e.is_boundary)
    bad = 0
    for e in bm.edges:
        if len(e.link_faces) != 2:
            continue
        f0, f1 = e.link_faces
        d0 = [lp for lp in e.link_loops if lp.face is f0][0].vert
        d1 = [lp for lp in e.link_loops if lp.face is f1][0].vert
        bad += d0 is d1
    return opened, bad


def plane_mirror(point, normal):
    n = Vector(normal).normalized()
    p0 = Vector(point)

    def m(co):
        return co - 2.0 * (co - p0).dot(n) * n
    return m


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


def back_shell(bm, faces, move, shade=0.6):
    """An inner side for a one-sided surface: a reversed copy of `faces` whose corners
    go through `move(vert) -> position`. UVs carried, colour the source corner times
    `shade`. Returns the new faces. As in fix_EYE.py."""
    uvl, col, tex = R.layers(bm)
    made = {}
    new = []
    for f in faces:
        vs = []
        for v in reversed(f.verts):
            if v not in made:
                made[v] = bm.verts.new(move(v))
            vs.append(made[v])
        nf = bm.faces.new(vs)
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo[uvl].uv
            c = lo[col]
            ln[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
        new.append(nf)
    return new


def cap_like(bm, corners, like):
    """A flat face through `corners`, each corner mapped and shaded like its counterpart
    on an existing face: `like` is a list of (face, vertex) in the same order. The
    counterpart is the same corner of a matching end elsewhere on the part, so a closed
    end reads as the ends the cartridge does draw."""
    uvl, col, tex = R.layers(bm)
    f0 = like[0][0]
    nf = bm.faces.new(corners)
    nf.material_index = f0.material_index
    nf[tex] = f0[tex]
    src = {}
    for v, (lf, lv) in zip(corners, like):
        lp = next(lp for lp in lf.loops if lp.vert is lv)
        src[v] = (lp[uvl].uv.copy(), tuple(lp[col]))
    for lp in nf.loops:
        lp[uvl].uv, lp[col] = src[lp.vert]
    return nf


def orient_out(f, centre):
    """Turn a new face so it faces away from `centre` (a point inside its solid)."""
    f.normal_update()
    if f.normal.dot(f.calc_center_median() - centre) < 0:
        f.normal_flip()


# ------------------------------------------------------------------ body
body = objs[0]
bm = open_bm(body)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")
SPIKE, PED, TOWER, BLOCK, HALL, WING_A, WING_B, TOWER_IN = range(1, 9)
for f in bm.faces:
    i = f[tri]
    f[part] = (SPIKE if i <= 3 else PED if i <= 7 else TOWER if i <= 13 else BLOCK if i <= 21
               else WING_B if i in (23, 24, 25, 26) else WING_A if i <= 33 else HALL)
by_tri = {f[tri]: f for f in bm.faces}


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def centre_of(faces):
    vs = verts_of(faces)
    return sum((v.co for v in vs), Vector()) / len(vs)


new_body = []

# the house-coloured block: its long sides are vertical, so the mirror plane is the
# vertical plane halfway between them. Its south-west end's two foot corners lie one on
# each long side, so their midpoint is on that plane
se_side = [by_tri[15], by_tri[20]]
n_block = se_side[0].normal.copy()
n_block.z = 0.0
mid = 0.5 * (find(bm, -486, -6, 839).co + find(bm, -811, -6, 451).co)
nw_side, miss = mirror_snap(bm, se_side, plane_mirror(mid, n_block), verts_of(faces_of(BLOCK)),
                            15.0 / B.CELL)
for f in nw_side:
    f[part] = BLOCK
report["block_side_added"] = len(nw_side)
report["block_side_snap_units"] = round(miss * B.CELL, 1)
new_body += nw_side

# the hall: the north-east side is the south-west side mirrored across the hall's long
# centre plane (the south-east end's two foot corners lie one on each side)
sw_side = [by_tri[37], by_tri[39]]
n_hall = sw_side[0].normal.copy()
n_hall.z = 0.0
se_foot_sw, se_foot_ne = find(bm, 593, 0, 1000), find(bm, 829, 0, 803)
mid = 0.5 * (se_foot_sw.co + se_foot_ne.co)
hall_mirror = plane_mirror(mid, n_hall)
ne_side, miss = mirror_snap(bm, sw_side, hall_mirror, verts_of(faces_of(HALL)), 15.0 / B.CELL)
for f in ne_side:
    f[part] = HALL
report["hall_side_added"] = len(ne_side)
report["hall_side_snap_units"] = round(miss * B.CELL, 1)
new_body += ne_side
# the north-west end, flat, mapped and shaded corner for corner like the south-east end
nw_sw_b, nw_sw_t, nw_ne_t = find(bm, -596, 0, -418), find(bm, -591, 238, -411), find(bm, -354, 238, -607)
p = hall_mirror(nw_sw_b.co)
nw_ne_b = min(bm.verts, key=lambda v: (v.co - p).length)
se_end = [by_tri[34], by_tri[35]]
se_sw_t, se_ne_t = find(bm, 427, 238, 802), find(bm, 663, 238, 605)
hall_end = cap_like(bm, [nw_sw_b, nw_sw_t, nw_ne_t, nw_ne_b],
                    [(se_end[0], se_foot_sw), (se_end[1], se_sw_t), (se_end[0], se_ne_t),
                     (se_end[0], se_foot_ne)])
hall_end[part] = HALL
orient_out(hall_end, centre_of(faces_of(HALL)))
new_body.append(hall_end)

# the wings: every open end is a flat end mapped and shaded corner for corner like the
# south-east wing's own south-east end (a vertical trapezoid: foot and top, south-west
# and north-east corners)
wa_end = [by_tri[29], by_tri[32]]
wa_sw_b, wa_sw_t, wa_ne_t, wa_ne_b = (find(bm, 81, -2, 985), find(bm, 270, 196, 827),
                                      find(bm, 605, 196, 546), find(bm, 932, -2, 272))
end_like = [(wa_end[0], wa_sw_b), (wa_end[0], wa_sw_t), (wa_end[0], wa_ne_t), (wa_end[1], wa_ne_b)]
wing_ends = [
    (WING_A, [(-392, -2, 420), (-203, 196, 261), (130, 196, -18), (457, -2, -293)]),
    (WING_B, [(-866, 0, -127), (-559, 193, -385), (-334, 193, -574), (-48, 0, -814)]),
    (WING_B, [(-358, 0, 478), (-58, 193, 226), (173, 193, 31), (459, 0, -208)]),
]
for wp, pts in wing_ends:
    f = cap_like(bm, [find(bm, *q) for q in pts], end_like)
    f[part] = wp
    new_body.append(f)
# the north-west wing's top, inside the hall, like the south-east wing's top
wa_top = [by_tri[27], by_tri[28]]
wa_top_nw_sw, wa_top_nw_ne = find(bm, -203, 196, 261), find(bm, 130, 196, -18)
f = cap_like(bm, [find(bm, -58, 193, 226), find(bm, 173, 193, 31), find(bm, -334, 193, -574),
                  find(bm, -559, 193, -385)],
             [(wa_top[0], wa_sw_t), (wa_top[0], wa_ne_t), (wa_top[1], wa_top_nw_ne),
              (wa_top[1], wa_top_nw_sw)])
f[part] = WING_B
new_body.append(f)
for wp in (WING_A, WING_B):
    c = centre_of(faces_of(wp))
    for f in new_body:
        if f[part] == wp:
            orient_out(f, c)
report["wing_faces_added"] = len(wing_ends) + 1


# ------------------------------------------------------------------ lattice tower
def complete_by_half_turn(fa, fb, snap):
    """Complete a four-sided part whose two south faces are modelled (fix_ATWR.py's
    helper with height as the ranking axis). The vertices only `fa` uses are its W
    corners, only `fb` its E corners, both its S corners; ranked by height they pair
    into rings, and each ring's missing corner is N = E + W - S. The half turn maps S to
    N and swaps E and W, so every south face has a north twin with the same winding, UVs
    and baked colour. A computed N within `snap` of an existing vertex is that vertex."""
    va, vb = set(verts_of(fa)), set(verts_of(fb))
    S = sorted(va & vb, key=lambda v: v.co.z)
    E = sorted(vb - va, key=lambda v: v.co.z)
    W = sorted(va - vb, key=lambda v: v.co.z)
    if not (len(S) == len(E) == len(W)):
        raise SystemExit("half turn: rings do not pair (S %d E %d W %d)" % (len(S), len(E), len(W)))
    everything = list(bm.verts)
    vmap, snapped, rings = {}, 0, []
    for s, e, w in zip(S, E, W):
        n = e.co + w.co - s.co
        old = min(everything, key=lambda v: (v.co - n).length)
        if (old.co - n).length < snap:
            vmap[s] = old
            snapped += 1
        else:
            vmap[s] = bm.verts.new(n)
        vmap[e], vmap[w] = w, e
        rings.append((s, e, w, vmap[s]))
    new = []
    for f in fa + fb:
        vs = [vmap[v] for v in f.verts]
        if bm.faces.get(vs) is not None:
            continue
        nf = bm.faces.new(vs)
        nf.material_index = f.material_index
        nf[tex] = f[tex]
        for lo, ln in zip(f.loops, nf.loops):
            ln[uvl].uv = lo[uvl].uv
            ln[col] = lo[col]
        new.append(nf)
    return new, snapped, rings


narrow, wide = [by_tri[10], by_tri[11]], [by_tri[12], by_tri[13]]
tw_new, tw_snapped, rings = complete_by_half_turn(narrow, wide, 6.0 / B.CELL)
for f in tw_new:
    f[part] = TOWER
report["tower_faces_added"] = len(tw_new)
report["tower_corners_snapped"] = tw_snapped
# the inner side: in the slab's own axes (the wide face's run and the narrow face's run
# are square to each other to within half a degree), every corner moves 5 model units
# in along both, about the slab's leaning centre line; the top comes down 5
(s0, e0, w0, n0), (s1, e1, w1, n1) = rings
ax_a = (e0.co - s0.co)
ax_a.z = 0.0
ax_a.normalize()
ax_b = (w0.co - s0.co)
ax_b.z = 0.0
ax_b.normalize()
report["tower_axes_square_deg"] = round(90.0 - math.degrees(ax_a.angle(ax_b)), 2)
c_foot = 0.25 * (s0.co + e0.co + w0.co + n0.co)
c_top = 0.25 * (s1.co + e1.co + w1.co + n1.co)
inset = 5.0 / B.CELL


def tower_in(v):
    t = (v.co.z - c_foot.z) / (c_top.z - c_foot.z)
    c = c_foot.lerp(c_top, t)
    d = v.co - c
    da, db = d.dot(ax_a), d.dot(ax_b)
    da -= math.copysign(inset, da)
    db -= math.copysign(inset, db)
    p = c + ax_a * da + ax_b * db
    p.z = v.co.z - (inset if t > 0.5 else 0.0)
    return p


tw_in = back_shell(bm, faces_of(TOWER), tower_in)
for f in tw_in:
    f[part] = TOWER_IN
report["tower_inner_faces"] = len(tw_in)
new_body += tw_new


# ------------------------------------------------------------------ posts: 8 sides
def round_post(pp, key):
    fs = faces_of(pp)
    vs = verts_of(fs)
    zb = min(v.co.z for v in vs)
    foot = [v.co for v in vs if v.co.z < zb + 0.01]
    c = R.fit_circle([(q.x, q.y) for q in foot])
    shade = R.smooth_ring_shade(fs, col, c)
    R.densify_curve(bm, fs, c, max_deg=100.0)
    R.apply_colour(faces_of(pp), col, shade)
    report[key + "_sides"] = len(faces_of(pp))
    report[key + "_radius_units"] = round(c[2] * B.CELL, 1)


round_post(PED, "pedestal")
round_post(SPIKE, "radar_post")

report["body_winding_fixed"] = R.agree_winding(bm, new_body + tw_in)
report["body_open_edges"], report["body_bad_winding"] = mesh_check(bm)
report["body_triangles"] = close_bm(bm, body)


# ------------------------------------------------------------------ dish (part 1)
def dish_frame(pm, faces):
    """The dish's own frame: origin the rim's centre, Z its axis pointing out of the
    bowl (the way its faces face). The rim is the ring farthest from the fan's centre.
    As in fix_ATWR.py."""
    vs = verts_of(faces)
    hub = max(vs, key=lambda v: len([f for f in v.link_faces if f in faces]))
    rim = sorted(vs, key=lambda v: -(v.co - hub.co).length)[:8]
    Pm = np.array([tuple(v.co) for v in rim])
    c = Pm.mean(0)
    _, _, vt = np.linalg.svd(Pm - c)
    z = Vector(vt[2])
    fn = sum((f.normal * f.calc_area() for f in faces), Vector())
    if z.dot(fn) < 0:
        z = -z
    x = (Vector(tuple(Pm[0] - c)) - z * Vector(tuple(Pm[0] - c)).dot(z)).normalized()
    y = z.cross(x)
    return Vector(tuple(c)), Matrix((x, y, z))


def smooth_disc_shade(faces, col, rq):
    """One smooth shade for a whole dish, fitted to its painted fan: per ring of equal
    radius (rq bins), colour = a0 + a1 cos(angle) + b1 sin(angle). Frame: the dish's
    own, axis Z. As in fix_ATWR.py."""
    rings = {}
    for f in faces:
        for lp in f.loops:
            k = round(R.radius_of(lp.vert.co, (0.0, 0.0)) / rq)
            rings.setdefault(k, []).append((R.angle_of(lp.vert.co, (0.0, 0.0)), tuple(lp[col])))
    fits = {}
    for k, s in rings.items():
        A = np.array([[1.0, math.cos(a), math.sin(a)] for a, _ in s])
        Y = np.array([c for _, c in s], float)
        fits[k] = (np.linalg.lstsq(A, Y, rcond=None)[0], Y.min(0), Y.max(0))

    def colour(co):
        k = min(fits, key=lambda q: abs(q - R.radius_of(co, (0.0, 0.0)) / rq))
        coef, lo, hi = fits[k]
        a = R.angle_of(co, (0.0, 0.0))
        return tuple(float(x) for x in np.clip(np.array([1.0, math.cos(a), math.sin(a)]) @ coef, lo, hi))
    return colour


def lens_back(pm, faces, r_rim, depth, shade=0.6):
    """A back for a bowl with none, in the dish's own frame (axis +Z out of the bowl):
    a reversed copy whose rim is the bowl's own rim and whose inner rings sit `depth`
    further back, so it is a closed shell. As in fix_ATWR.py."""
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
    """Turn a one-triangle fin into a thin three-sided spike: a fourth corner behind the
    base edge, at 0.4 of the base's length, and the three faces that close it. As in
    fix_ATWR.py."""
    uvl, col, tex = R.layers(pm)
    fin.normal_update()
    ls = list(fin.loops)
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
    src_ = {b0.vert: b0, b1.vert: b1, tip.vert: tip}
    for nf in new:
        for lp in nf.loops:
            s_ = src_.get(lp.vert)
            lp[uvl].uv = s_[uvl].uv if s_ else quv
            c = s_[col] if s_ else qcol
            lp[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
    return new


def onto_polygon(p, ring):
    """Move p (dish frame, rim plane) radially onto the closed polygon `ring` (2D points
    ordered by angle): where the ray from the axis through p crosses the ring."""
    a = math.atan2(p.y, p.x)
    n = len(ring)
    for i in range(n):
        q0, q1 = ring[i], ring[(i + 1) % n]
        a0, a1 = math.atan2(q0[1], q0[0]), math.atan2(q1[1], q1[0])
        span = (a1 - a0) % (2 * math.pi)
        if (a - a0) % (2 * math.pi) <= span:
            d = Vector((math.cos(a), math.sin(a)))
            e = Vector(q1) - Vector(q0)
            # solve t*d = q0 + s*e
            det = d.x * (-e.y) - d.y * (-e.x)
            if abs(det) < 1e-12:
                return p
            t = (q0[0] * (-e.y) - q0[1] * (-e.x)) / det
            return Vector((d.x * t, d.y * t, p.z))
    return p


dish_ob = objs[1]
pm = open_bm(dish_ob)
uvl, col, tex = R.layers(pm)
ptri = pm.faces.layers.int["pack_tri"]
ppart = pm.faces.layers.int.new("fix_part")
DISH, STRUT, BACK = 20, 21, 22
for f in pm.faces:
    f[ppart] = STRUT if f[ptri] <= 49 else DISH
dish = [f for f in pm.faces if f[ppart] == DISH]
c, rot = dish_frame(pm, dish)
for v in pm.verts:
    v.co = rot @ (v.co - c)
pm.normal_update()
r_rim = max(R.radius_of(v.co, (0.0, 0.0)) for v in verts_of(dish))
struts = [f for f in pm.faces if f[ppart] == STRUT]
tip = max(verts_of(struts), key=lambda v: sum(1 for f in v.link_faces))
# kept as positions: densify_curve rebuilds the mesh's vertex references
feet_at = [v.co.copy() for v in verts_of(struts) if v is not tip]
shade = smooth_disc_shade(dish, col, r_rim / 6.0)
sides_before = sum(1 for e in pm.edges if e.is_boundary and all(f[ppart] == DISH for f in e.link_faces))
R.densify_curve(pm, dish, (0.0, 0.0, r_rim), max_deg=60.0)
dish = [f for f in pm.faces if f[ppart] == DISH]
R.apply_colour(dish, col, shade)
rim_ring = sorted([v for v in verts_of(dish) if R.radius_of(v.co, (0.0, 0.0)) > 0.9 * r_rim],
                  key=lambda v: R.angle_of(v.co, (0.0, 0.0)))
ring2d = [(v.co.x, v.co.y) for v in rim_ring]
moved = 0.0
feet = [min(pm.verts, key=lambda w: (w.co - q).length) for q in feet_at]
for v in feet:
    q = onto_polygon(v.co, ring2d)
    moved = max(moved, (q - v.co).length)
    v.co = q
report["strut_feet_moved_units"] = round(moved * B.CELL, 1)
hub_depth = -min(v.co.z for v in verts_of(dish))
# a dish about a twelfth of its radius thick at the hub, as on the Advanced Guard Tower
for f in lens_back(pm, dish, r_rim, r_rim / 12.0):
    f[ppart] = BACK
spikes = 0
for fin in [f for f in pm.faces if f[ppart] == STRUT]:
    for f in spike_from_fin(pm, fin):
        f[ppart] = STRUT
        spikes += 1
inv = rot.transposed()
for v in pm.verts:
    v.co = inv @ v.co + c
report["dish"] = "%d->%d rim sides, rim r %.1f, bowl depth %.1f, strut faces added %d" % (
    sides_before, len(rim_ring), r_rim * B.CELL, hub_depth * B.CELL, spikes)
report["dish_open_edges"], report["dish_bad_winding"] = mesh_check(pm)
report["dish_triangles"] = close_bm(pm, dish_ob)


# ------------------------------------------------------------------ radar screen (part 2)
screen = objs[2]
sm = open_bm(screen)
uvl, col, tex = R.layers(sm)
Fb = [find(sm, *q) for q in ((-414, 291, -340), (-357, 288, -316), (-273, 288, -235), (-250, 292, -179))]
Ft = [find(sm, *q) for q in ((-394, 423, -362), (-340, 421, -333), (-257, 421, -252), (-232, 422, -200))]
Bt = [find(sm, *q) for q in ((-385, 419, -370), (-332, 418, -342), (-248, 418, -261), (-222, 420, -210))]
Bb = [find(sm, -401, 288, -354), None, None, find(sm, -241, 290, -188)]
# the back's two missing bottom corners: the front corner moved by the same thickness
# the column's top corner is moved by
for i in (1, 2):
    Bb[i] = sm.verts.new(Fb[i].co + (Bt[i].co - Ft[i].co))
ref = next(iter(sm.faces))


def panel_colour(i):
    """The painted colour of front panel i (flat per panel on the cartridge)."""
    f = next(f for f in Fb[i].link_faces if Fb[i + 1] in f.verts)
    return tuple(f.loops[0][col])


made = []
for i in range(3):
    pc = panel_colour(i)
    # the back, shaded as its panel at 0.6
    q = sm.faces.new([Bb[i], Bb[i + 1], Bt[i + 1], Bt[i]])
    for lp in q.loops:
        lp[col] = (pc[0] * 0.6, pc[1] * 0.6, pc[2] * 0.6, pc[3])
    made.append(q)
    # the bottom rim, shaded as the panel above it at 0.8, like the side rims' feet
    q = sm.faces.new([Fb[i], Fb[i + 1], Bb[i + 1], Bb[i]])
    for lp in q.loops:
        lp[col] = (pc[0] * 0.8, pc[1] * 0.8, pc[2] * 0.8, pc[3])
    made.append(q)
for q in made:
    q.material_index = ref.material_index
    q[tex] = ref[tex]
report["screen_closing_faces"] = len(made)
# closed, the screen has an inside, so every face is turned out (the cartridge's rims
# are wound inconsistently with its front, as on the Airstrip's radar)
sm.normal_update()
before = [f.normal.copy() for f in sm.faces]
bmesh.ops.recalc_face_normals(sm, faces=list(sm.faces))
sm.normal_update()
report["screen_faces_turned_out"] = sum(1 for f, n in zip(sm.faces, before) if f.normal.dot(n) < 0)
report["screen_open_edges"], report["screen_bad_winding"] = mesh_check(sm)
report["screen_triangles"] = close_bm(sm, screen)

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "HQ_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
