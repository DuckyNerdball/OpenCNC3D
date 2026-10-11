"""The Construction Yard's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_FACT.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/FACT_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building: a
barrel-roofed hangar lying south-west to north-east on a raised platform, a panelled
door at its south-west end, a crane on a round base to the west, a twin-fan box at the
north-west corner, all on one slab.

The model is ten parts. Parts 1 to 8 move in the building's animation (the crane turns,
its boom and hoist swing, the door slides down into the ground, the crate drives out of
the hangar), so every part stays its own object with its pack_part and pivot, and every
repair is made in the rest pose.

  vault   five roof strips on a sheared barrel: the south-west and north-east arches lie
          in planes leaning 22 and 30 degrees from upright, and the north-east arch is
          about 85 model units lower. Each arch is fitted with a circle in its own
          plane (miss at most 14 units) and every strip cut in three with the new
          vertices on those circles: 15 strips. The north-east end was never modelled: it gets a gable. While the
          door is down the inside of the vault shows through the doorway, and the
          cartridge draws the roof one-sided, so the vault gets a lining 12 units inside
          it, a lip across the doorway's arch and an inner gable. The cartridge's own
          dark interior walls below the springline stay as they are.
  door    part 6, the arch-topped panel that closes the doorway: its arch follows the
          vault's new vertices, so door and roof still meet with no gap.
  base    the crane's round base is a regular hexagonal prism with its east side left
          out (it runs into the platform's tip). The east side is the west side
          mirrored, then the prism is doubled twice to 24 sides (the Remastered base is
          round) and re-shaded from one smooth fit of the cartridge's painted sides.
  platform  the raised platform round the hangar has no north-east outer wall: built
          from the platform's top edge down to its foot, leaning out as the two outer
          walls the cartridge draws do.
  slab    two skirts, both facing north, were left out: added, mapped the way the
          cartridge maps its skirts.
  fans    part 9, the fan box: each of its six side triangles is stored twice in the
          pack with the same winding; the copies are dropped. Its north-east end is
          the south-west end mirrored across the box's middle, shaded as its back.
  crane   the boom (part 2) and the hoist (part 3) tilt as they swing, so their
          undersides can turn to the camera: each gets its missing bottom.
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
doc, objs = B.build_cartridge(src, "FACT")
report = {"cartridge_triangles": sum(p["ntri"] for p in doc["parts"]),
          "cartridge_drawn": sum(len(o.data.polygons) for o in objs)}


# ------------------------------------------------------------------ helpers
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


def at(bm, x, y, z, tol=5.0 / B.CELL):
    """The vertex at a position in Blender cells, to within `tol`."""
    p = Vector((x, y, z))
    v = min(bm.verts, key=lambda w: (w.co - p).length)
    if (v.co - p).length > tol:
        raise SystemExit("no vertex near (%g, %g, %g)" % (x, y, z))
    return v


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def loop_at(f, v):
    return next(lp for lp in f.loops if lp.vert is v)


def mean_colour(bm, faces):
    col = bm.loops.layers.color.get("Col")
    s = [0.0] * 4
    n = 0
    for f in faces:
        for lp in f.loops:
            for i in range(4):
                s[i] += lp[col][i]
            n += 1
    return tuple(x / max(n, 1) for x in s)


def scaled(c, k):
    return (c[0] * k, c[1] * k, c[2] * k, c[3])


def new_face(bm, vs, like, uv_of, colour, fpart=None, partl=None):
    """A face through `vs`, textured like `like` (a face, or (pack_tex, material
    slot)); uv_of(vert) and colour(vert) (or one
    colour for all corners) give its corners."""
    uvl, col, tex = R.layers(bm)
    f = bm.faces.new(vs)
    if isinstance(like, tuple):
        f[tex], f.material_index = like
    else:
        f[tex] = like[tex]
        f.material_index = like.material_index
    if partl is not None:
        f[partl] = fpart
    for lp in f.loops:
        lp[uvl].uv = uv_of(lp.vert)
        lp[col] = colour(lp.vert) if callable(colour) else colour
    return f


def face_toward(f, d):
    """Turn a face so its normal points along d."""
    f.normal_update()
    if f.normal.dot(d) < 0:
        f.normal_flip()


def plane_mirror(point, normal):
    n = Vector(normal).normalized()
    p0 = Vector(point)

    def m(co):
        return co - 2.0 * (co - p0).dot(n) * n
    return m


def mirror_snap(bm, faces, mirror, candidates, snap):
    """Mirror copy of `faces` through the position map `mirror`, reversed so it faces
    outward again. Each mirrored corner lands on the nearest of `candidates` within
    `snap`, otherwise on a new vertex. UVs and colours are carried from the source
    corner. Returns (new faces, worst snap distance). As in fix_PYLE.py."""
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


def boundary_loops(bm):
    """Open-edge loops as ordered vertex lists."""
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


class ArchFit:
    """A circle fitted to an arch that lies in a tilted plane: the plane by least
    squares, the circle in it. point(p0, p1, t) is the point a fraction t of the way
    round the fitted arc from p0 to p1, keeping the small off-plane part of each end,
    so an arch cut this way stays on the arch the cartridge drew."""

    def __init__(self, pts):
        X = np.asarray([tuple(p) for p in pts], float)
        c = X.mean(0)
        _, sv, vt = np.linalg.svd(X - c)
        self.e1, self.e2, self.n = (Vector(vt[0]), Vector(vt[1]), Vector(vt[2]))
        self.c0 = Vector(c)
        uv = np.c_[(X - c) @ vt[0], (X - c) @ vt[1]]
        A = np.c_[2 * uv, np.ones(len(uv))]
        cx, cy, k = np.linalg.lstsq(A, (uv ** 2).sum(1), rcond=None)[0]
        self.r = math.sqrt(k + cx * cx + cy * cy)
        self.centre = self.c0 + self.e1 * cx + self.e2 * cy
        self.miss = float(np.abs(np.hypot(uv[:, 0] - cx, uv[:, 1] - cy) - self.r).max())
        self.flat = float(sv[2])

    def split(self, p):
        d = p - self.centre
        off = d.dot(self.n)
        return d - self.n * off, off

    def point(self, p0, p1, t):
        d0, o0 = self.split(p0)
        d1, o1 = self.split(p1)
        ang = d0.angle(d1)
        axis = self.n if d0.cross(d1).dot(self.n) > 0 else -self.n
        rot = Matrix.Rotation(ang * t, 3, axis)
        d = rot @ d0.normalized() * (d0.length * (1 - t) + d1.length * t)
        return self.centre + d + self.n * (o0 * (1 - t) + o1 * t)

    def inward(self, p):
        d, _ = self.split(p)
        return -d.normalized()


def catmull(vals, i, t):
    """Catmull-Rom between vals[i] and vals[i + 1] (ends held), per component."""
    n = len(vals)
    p0, p1 = vals[max(i - 1, 0)], vals[i]
    p2, p3 = vals[min(i + 1, n - 1)], vals[min(i + 2, n - 1)]
    out = []
    for k in range(len(p1)):
        a, b, c, d = p0[k], p1[k], p2[k], p3[k]
        out.append(0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t +
                          (-a + 3 * b - 3 * c + d) * t * t * t))
    return out


# ------------------------------------------------------------------ body (part 0)
body = objs[0]
bm = open_bm(body)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

(INWALL, OUTWALL, PLATTOP, HEXW, HEXCAP, SLAB, SKIRT, SLABBOT, ROOF, NEWALL, GABLE,
 LINING, LIP, GABLE_IN) = range(1, 15)
# cartridge triangle ranges, read off the extract
BY_TRI = {}
for ids, p in (((0, 1, 8, 9, 18, 19), INWALL), ((2, 3, 4, 5, 6, 7, 20, 21), OUTWALL),
               (range(10, 18), PLATTOP), (range(22, 32), HEXW), (range(32, 36), HEXCAP),
               ((36, 49, 50, 51, 53, 54), SLAB), ((37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 52), SKIRT),
               ((48,), SLABBOT), (range(55, 65), ROOF)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI[f[tri]]


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


# ------------------------------------------------------------------ crane base
cap_v = verts_of(faces_of(HEXCAP))
hc = R.fit_circle([(v.co.x, v.co.y) for v in cap_v])
report["base_axis"] = "x %.1f y %.1f r %.1f" % (hc[0] * B.CELL, hc[1] * B.CELL, hc[2] * B.CELL)
walls = faces_of(HEXW)
shade_base = R.smooth_ring_shade(walls, col, hc)
cap_uv, rms = R.fit_uv(faces_of(HEXCAP), uvl, lambda co: (co.x, co.y, 1.0))
report["base_cap_uv_fit_rms"] = round(rms, 4)
# the west side, mirrored across the north-south plane through the axis, is the east
west = [f for f in walls if f.normal.x < -0.9]
east, worst = mirror_snap(bm, west, plane_mirror((hc[0], hc[1], 0.0), (1.0, 0.0, 0.0)),
                          verts_of(walls), 4.0 / B.CELL)
for f in east:
    f[part] = HEXW
report["base_east_faces"] = len(east)
report["base_east_snap_units"] = round(worst * B.CELL, 2)
report["base_east_winding_fixed"] = R.agree_winding(bm, east)
sides_before = len(faces_of(HEXW)) // 2
# six sides span 60 degrees each, past densify_curve's default limit
R.densify_curve(bm, faces_of(HEXW, HEXCAP), hc, max_deg=75.0)
R.densify_curve(bm, faces_of(HEXW, HEXCAP), hc, max_deg=45.0)
R.apply_colour(faces_of(HEXW), col, shade_base)
for f in faces_of(HEXCAP):
    for lp in f.loops:
        lp[uvl].uv = cap_uv(lp.vert.co)
report["base_sides"] = "%d->%d" % (sides_before, len({round(math.degrees(R.angle_of(v.co, hc))) % 360
                                                       for v in verts_of(faces_of(HEXCAP))}))

# ------------------------------------------------------------------ slab skirts
skirts = faces_of(SKIRT)
skirt_uv, rms = R.fit_uv(skirts, uvl, lambda co: (co.x, co.y, 1.0))
report["skirt_uv_fit_rms"] = round(rms, 4)
skirt_col = mean_colour(bm, skirts)
slab_top = [at(bm, 1.296, 0.778, 0.064), at(bm, 0.396, 1.321, 0.064), at(bm, -0.369, 1.292, 0.064)]
slab_bot = [at(bm, 1.296, 0.778, 0.0), bm.verts.new((slab_top[1].co.x, slab_top[1].co.y, 0.0)),
            at(bm, -0.369, 1.292, 0.0)]
slab_c = sum((v.co for v in verts_of(faces_of(SLAB))), Vector()) / len(verts_of(faces_of(SLAB)))
new_skirts = []
for i in range(2):
    f = new_face(bm, [slab_top[i], slab_top[i + 1], slab_bot[i + 1], slab_bot[i]], skirts[0],
                 lambda v: skirt_uv(v.co), skirt_col, SKIRT, part)
    m = f.calc_center_median()
    face_toward(f, Vector((m.x - slab_c.x, m.y - slab_c.y, 0.0)))
    new_skirts.append(f)
report["skirt_winding_fixed"] = R.agree_winding(bm, new_skirts)

# ------------------------------------------------------------------ platform, north-east wall
tops = [at(bm, 1.309, 0.428, 0.270), at(bm, 1.154, 0.800, 0.270), at(bm, 0.743, 1.019, 0.270),
        at(bm, 0.268, 1.031, 0.270)]
foot_z = at(bm, 1.371, 0.405, 0.052).co.z


def batter(top_a, top_b, foot_a):
    """How far a wall's foot stands out from its top edge, horizontally."""
    d = (top_b.co - top_a.co)
    d.z = 0.0
    n = Vector((d.y, -d.x, 0.0)).normalized()
    return abs((foot_a.co - top_a.co).dot(n))


b_se = batter(at(bm, 0.705, -0.696, 0.270), tops[0], at(bm, 0.719, -0.801, 0.052))
b_nw = batter(at(bm, -0.387, -0.086, 0.270), tops[3], at(bm, -0.471, -0.128, 0.052))
lean = 0.5 * (b_se + b_nw)
report["platform_lean_units"] = "SE %.1f NW %.1f, used %.1f" % (b_se * B.CELL, b_nw * B.CELL, lean * B.CELL)
plat_c = sum((v.co for v in verts_of(faces_of(PLATTOP))), Vector()) / len(verts_of(faces_of(PLATTOP)))


def out_normal(a, b):
    d = b.co - a.co
    n = Vector((d.y, -d.x, 0.0)).normalized()
    m = 0.5 * (a.co + b.co)
    return n if n.dot(m - plat_c) > 0 else -n


feet = [at(bm, 1.371, 0.405, 0.052)]
for i in (1, 2):
    n0, n1 = out_normal(tops[i - 1], tops[i]), out_normal(tops[i], tops[i + 1])
    bis = (n0 + n1).normalized()
    p = tops[i].co + bis * (lean / max(bis.dot(n0), 0.5))
    feet.append(bm.verts.new((p.x, p.y, foot_z)))
feet.append(at(bm, 0.226, 1.064, 0.052))
# the feet must stay on the slab: distance from each new foot to the slab's north rim
rim = [(v.co.x, v.co.y) for v in slab_top]


def dist_to_rim(p):
    best = 9.0
    for (ax, ay), (bx, by) in zip(rim, rim[1:]):
        d = Vector((bx - ax, by - ay, 0.0))
        t = max(0.0, min(1.0, Vector((p.x - ax, p.y - ay, 0.0)).dot(d) / d.length_squared))
        q = Vector((ax, ay, 0.0)) + d * t
        best = min(best, (Vector((p.x, p.y, 0.0)) - q).length)
    return best


report["platform_feet_inside_slab_units"] = [round(dist_to_rim(v.co) * B.CELL, 1) for v in feet[1:3]]
top_faces = faces_of(PLATTOP)
top_uv, rms = R.fit_uv(top_faces, uvl, lambda co: (co.x, co.y, 1.0))
report["platform_top_uv_fit_rms"] = round(rms, 4)


def top_corner_uv(v):
    f = next(f for f in v.link_faces if f.is_valid and f[part] == PLATTOP)
    return loop_at(f, v)[uvl].uv.copy()


foot_of = {}
for t_, f_ in zip(tops, feet):
    foot_of[f_] = t_
uv_top = {v: top_corner_uv(v) for v in tops}


def ne_uv(v):
    if v in uv_top:
        return uv_top[v]
    t_ = foot_of[v]
    return uv_top[t_] + Vector(top_uv(v.co)) - Vector(top_uv(t_.co))


outer = [f for f in faces_of(OUTWALL) if f[tri] in (4, 5, 6, 7)]
wall_col = mean_colour(bm, outer)
ne_faces = []
for i in range(3):
    f = new_face(bm, [tops[i], tops[i + 1], feet[i + 1], feet[i]], outer[0], ne_uv, wall_col,
                 NEWALL, part)
    f.normal_update()
    face_toward(f, out_normal(tops[i], tops[i + 1]))
    ne_faces.append(f)
report["platform_ne_winding_fixed"] = R.agree_winding(bm, ne_faces)

# ------------------------------------------------------------------ vault
SW = [(0.593, -0.542, 0.269), (0.519, -0.394, 0.502), (0.354, -0.229, 0.610),
      (0.170, -0.134, 0.615), (-0.002, -0.093, 0.516), (-0.193, -0.081, 0.275)]
NE = [(1.163, 0.477, 0.269), (1.096, 0.622, 0.438), (0.969, 0.765, 0.525),
      (0.763, 0.883, 0.531), (0.575, 0.923, 0.452), (0.396, 0.925, 0.275)]
roof = faces_of(ROOF)
sw_v = [at(bm, *p) for p in SW]
ne_v = [at(bm, *p) for p in NE]


def roof_corner(v):
    f = next(f for f in v.link_faces if f.is_valid and f[part] == ROOF)
    lp = loop_at(f, v)
    return lp[uvl].uv.copy(), tuple(lp[col])


sw_uvc = [roof_corner(v) for v in sw_v]
ne_uvc = [roof_corner(v) for v in ne_v]
roof_like = (roof[0][tex], roof[0].material_index)
fit_sw, fit_ne = ArchFit([v.co for v in sw_v]), ArchFit([v.co for v in ne_v])
report["vault_arch_fit"] = "SW r %.1f miss %.1f, NE r %.1f miss %.1f" % (
    fit_sw.r * B.CELL, fit_sw.miss * B.CELL, fit_ne.r * B.CELL, fit_ne.miss * B.CELL)
axis = ((sum((v.co for v in ne_v), Vector()) - sum((v.co for v in sw_v), Vector())) / 6.0)
axis.z = 0.0
axis.normalize()
axis_pt = 0.5 * (fit_sw.centre + fit_ne.centre)

CUTS = 3                 # each cartridge strip becomes three


def arch(verts, uvc, fit):
    """The arch with CUTS - 1 new points in each span: [(vert, uv, colour)]."""
    cols = [c for _, c in uvc]
    outp = []
    for i in range(len(verts)):
        outp.append((verts[i], uvc[i][0], uvc[i][1]))
        if i == len(verts) - 1:
            break
        for k in range(1, CUTS):
            t = k / CUTS
            p = fit.point(verts[i].co, verts[i + 1].co, t)
            uv = uvc[i][0].lerp(uvc[i + 1][0], t)
            c = tuple(max(0.0, min(1.0, x)) for x in catmull(cols, i, t))
            outp.append((bm.verts.new(p), uv, c))
    return outp


A = arch(sw_v, sw_uvc, fit_sw)
Bq = arch(ne_v, ne_uvc, fit_ne)
for f in roof:
    bm.faces.remove(f)
N = len(A)
uv_at = {}
col_at = {}
for (v, uv, c) in A + Bq:
    uv_at[v] = uv
    col_at[v] = c


def away_from_axis(p):
    d = p - axis_pt
    d = d - axis * d.dot(axis)
    return d.normalized()


new_roof = []
for j in range(N - 1):
    q = [A[j][0], A[j + 1][0], Bq[j + 1][0], Bq[j][0]]
    f = new_face(bm, q, roof_like, lambda v: uv_at[v], lambda v: col_at[v], ROOF, part)
    face_toward(f, away_from_axis(f.calc_center_median()))
    new_roof.append(f)
report["vault_strips"] = "5->%d" % len(new_roof)

# the lining: T inside the roof, its springs on the roof's own springs so it meets the
# interior walls below them; pulled a few units in from each end so it never lies in
# the door's plane or the gable's
T = 12.0 / B.CELL
A_in, B_in = [], []
for j in range(N):
    for src_, dst, fit, along in ((A, A_in, fit_sw, 3.0 / B.CELL), (Bq, B_in, fit_ne, -6.0 / B.CELL)):
        v = src_[j][0]
        if j in (0, N - 1):
            dst.append(v)
        else:
            dst.append(bm.verts.new(v.co + fit.inward(v.co) * T + axis * along))
in_of = {}
for j in range(N):
    in_of[A_in[j]] = A[j][0]
    in_of[B_in[j]] = Bq[j][0]
lining = []
for j in range(N - 1):
    q = [A_in[j], A_in[j + 1], B_in[j + 1], B_in[j]]
    f = new_face(bm, q, roof_like, lambda v: uv_at[in_of[v]],
                 lambda v: scaled(col_at[in_of[v]], 0.55), LINING, part)
    face_toward(f, -away_from_axis(f.calc_center_median()))
    lining.append(f)
# the lip across the doorway's arch, between roof and lining
lip = []
front = -axis
for j in range(N - 1):
    q = [A[j][0], A[j + 1][0], A_in[j + 1], A_in[j]]
    q = [v for k, v in enumerate(q) if v not in q[:k]]
    f = new_face(bm, q, roof_like, lambda v: uv_at[in_of.get(v, v)],
                 lambda v: scaled(col_at[in_of.get(v, v)], 0.8), LIP, part)
    face_toward(f, front)
    lip.append(f)

# the north-east gable: fanned from the middle of its springline, which is cut there so
# the platform top and the interior wall under it share the new vertex
chord = next(e for e in Bq[0][0].link_edges if e.other_vert(Bq[0][0]) is Bq[-1][0])
_, M = bmesh.utils.edge_split(chord, Bq[0][0], 0.5)
s0, s1 = Bq[0][0].co, Bq[-1][0].co
span = (s1 - s0)
span.z = 0.0
u0, u1 = uv_at[Bq[0][0]].x, uv_at[Bq[-1][0]].x
v_ne = uv_at[Bq[0][0]].y


def gable_uv(v):
    """Flat across the end: u runs across the gable as the roof's u runs from spring to
    spring, so the roof's vertical banding carries on down the end."""
    p = in_of.get(v, v).co if v is not M_in else M.co
    d = p - s0
    d.z = 0.0
    s = max(0.0, min(1.0, d.dot(span) / span.length_squared))
    return Vector((u0 + (u1 - u0) * s, v_ne))


roof_mean = mean_colour(bm, new_roof)
gable_col = scaled(roof_mean, 0.6)
M_in = bm.verts.new(M.co - axis * 6.0 / B.CELL)
gable = []
for j in range(N - 1):
    f = new_face(bm, [M, Bq[j][0], Bq[j + 1][0]], roof_like, gable_uv, gable_col, GABLE, part)
    face_toward(f, axis)
    gable.append(f)
gable_in = []
inner_col = scaled(roof_mean, 0.45)
for j in range(N - 1):
    f = new_face(bm, [M_in, B_in[j], B_in[j + 1]], roof_like, gable_uv, inner_col, GABLE_IN, part)
    face_toward(f, -axis)
    gable_in.append(f)
# two slivers under the inner gable close the step between it and the springline
for a_, b_ in ((Bq[0][0], M), (M, Bq[-1][0])):
    f = new_face(bm, [a_, b_, M_in], roof_like, gable_uv, inner_col, GABLE_IN, part)
    face_toward(f, Vector((0.0, 0.0, -1.0)))
    gable_in.append(f)
report["vault_added"] = "lining %d, lip %d, gable %d, inner gable %d" % (
    len(lining), len(lip), len(gable), len(gable_in))

# the door is rebuilt on the same arch
arch_sw = [(p[0].co.copy(), p[1].copy(), p[2]) for p in A]
arch_new = [k % CUTS != 0 for k in range(N)]
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["body_winding_errors"] = winding_errors(bm)
report["body_triangles"] = len(bm.faces)
bm.to_mesh(body.data)
bm.free()
body.data.update()

# ------------------------------------------------------------------ door (part 6)
door = objs[6]
dm = open_bm(door)
duvl, dcol, dtex = R.layers(dm)
dfaces = list(dm.faces)
like = (dfaces[0][dtex], dfaces[0].material_index)
d_orig = [at(dm, *p, tol=6.0 / B.CELL) for p in SW]
d_uvc = []
for v in d_orig:
    f = next(iter(v.link_faces))
    lp = loop_at(f, v)
    d_uvc.append((lp[duvl].uv.copy(), tuple(lp[dcol])))
# v runs with height on the door (the panels repeat upward): a fit on z
Z = np.asarray([[lp.vert.co.z, 1.0] for f in dfaces for lp in f.loops])
V = np.asarray([lp[duvl].uv.y for f in dfaces for lp in f.loops])
vz = np.linalg.lstsq(Z, V, rcond=None)[0]
foot_se = at(dm, 0.531, -0.582, 0.053)
foot_nw = at(dm, -0.197, -0.141, 0.053)
foot_uvc = {}
for v in (foot_se, foot_nw):
    lp = loop_at(next(iter(v.link_faces)), v)
    foot_uvc[v] = (lp[duvl].uv.copy(), tuple(lp[dcol]))
door_n = Vector()
for f in dfaces:
    door_n += f.normal * f.calc_area()
for f in dfaces:
    dm.faces.remove(f)
ring = [foot_se]
d_uv = dict(foot_uvc)
for k in range(N):
    if not arch_new[k]:
        v = d_orig[k // CUTS]
        d_uv[v] = d_uvc[k // CUTS]
    else:
        i, t = k // CUTS, (k % CUTS) / CUTS
        v = dm.verts.new(arch_sw[k][0])
        uv = d_uvc[i][0].lerp(d_uvc[i + 1][0], t)
        uv.y = float(vz[0] * v.co.z + vz[1])
        c = tuple(a * (1 - t) + b * t for a, b in zip(d_uvc[i][1], d_uvc[i + 1][1]))
        d_uv[v] = (uv, c)
    ring.append(v)
ring.append(foot_nw)
nf = dm.faces.new(ring)
nf[dtex], nf.material_index = like
for lp in nf.loops:
    lp[duvl].uv, lp[dcol] = d_uv[lp.vert]
face_toward(nf, door_n)
bmesh.ops.triangulate(dm, faces=[nf], quad_method="BEAUTY", ngon_method="BEAUTY")
report["door_triangles"] = "6->%d" % close_bm(dm, door)

# ------------------------------------------------------------------ fan box (part 9)
fans = objs[9]
n_pack = len(fans.data.polygons)
fm = open_bm(fans)
fuvl, fcol, ftex = R.layers(fm)
seen = {}
dup = 0
for f in list(fm.faces):
    key = frozenset(f.verts)
    if key in seen:
        fm.faces.remove(f)
        dup += 1
    else:
        seen[key] = f
# the weld already folds most copies together; count what the pack stored twice
report["fans_duplicates_dropped"] = n_pack - len(fm.faces)
fm.normal_update()
sw_end = [f for f in fm.faces if f[ftex] == 43 and f.normal.dot(Vector((-0.54, -0.84, 0.0))) > 0.8]
box_axis = (at(fm, -0.254, 1.283, 0.050).co + at(fm, 0.101, 1.050, 0.050).co -
            at(fm, -0.750, 0.522, 0.050).co - at(fm, -0.380, 0.274, 0.050).co) * 0.5
mid = (at(fm, -0.254, 1.283, 0.050).co + at(fm, 0.101, 1.050, 0.050).co +
       at(fm, -0.750, 0.522, 0.050).co + at(fm, -0.380, 0.274, 0.050).co) * 0.25
# the box is symmetric only to about 20 model units (its north-east end is that much
# narrower at the top); its corners are 150 or more apart, so snap at 24
ne_end, worst = mirror_snap(fm, sw_end, plane_mirror(mid, box_axis), list(fm.verts), 24.0 / B.CELL)
back = [f for f in fm.faces if f.normal.dot(Vector((-0.84, 0.54, 0.0))) > 0.8]
back_col = mean_colour(fm, back)
for f in ne_end:
    for lp in f.loops:
        lp[fcol] = back_col
report["fans_ne_end"] = "%d faces, snap %.1f units, shaded as the back" % (len(ne_end), worst * B.CELL)
report["fans_winding_fixed"] = R.agree_winding(fm, ne_end)
report["fans_open_edges"] = sum(1 for e in fm.edges if e.is_boundary)
report["fans_winding_errors"] = winding_errors(fm)
report["fans_triangles"] = "14->%d" % close_bm(fm, fans)


# ------------------------------------------------------------------ boom and hoist bottoms
def cap_bottom(ob):
    """Close the open loop at the bottom of a box-like part with a face looking down,
    mapped like the part's top and shaded like its darkest side."""
    pm = open_bm(ob)
    puvl, pcol, ptex = R.layers(pm)
    loops = boundary_loops(pm)
    lo = min(loops, key=lambda L: sum(v.co.z for v in L) / len(L))
    top = max(pm.faces, key=lambda f: f.normal.z)
    top_v = verts_of([f for f in pm.faces if f.normal.z > 0.99])
    sides = [f for f in pm.faces if abs(f.normal.z) < 0.5]
    dark = min((mean_colour(pm, [f]) for f in sides), key=lambda c: c[0] + c[1] + c[2])

    def uv_of(v):
        w = min(top_v, key=lambda t: Vector((t.co.x - v.co.x, t.co.y - v.co.y)).length)
        f = next(f for f in w.link_faces if f.normal.z > 0.99)
        return loop_at(f, w)[puvl].uv.copy()
    f = new_face(pm, lo, top, uv_of, dark)
    face_toward(f, Vector((0.0, 0.0, -1.0)))
    n0 = len(pm.faces) - 1
    return "%d->%d" % (n0, close_bm(pm, ob))


report["boom_bottom"] = cap_bottom(objs[2])
report["hoist_bottom"] = cap_bottom(objs[3])

# ------------------------------------------------------------------ every part checked
for ob in objs:
    pm = bmesh.new()
    pm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(pm, verts=pm.verts, dist=1e-5)
    report["part%d" % ob["pack_part"]] = "%d tris, open %d, winding errors %d" % (
        len(pm.faces), sum(1 for e in pm.edges if e.is_boundary), winding_errors(pm))
    pm.free()
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "FACT_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
