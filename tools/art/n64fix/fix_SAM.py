"""The SAM Site's repair plan: the cartridge model, completed and softened.

    blender -b --python tools/art/n64fix/fix_SAM.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/SAM_fixed.blend; run.sh then puts it
in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and its animation, and checked against the
Remastered building: a square pad with a bevelled rim, a hazard-striped band round a
square pit with dark walls, two doors over the pit, and a missile box on a post that
rises out of it.

The model is seven animated parts, each kept as its own object with its pack_part and
pivot, geometry in its rest pose:

  0 body   the slab (a box 68 units high, closed but for its underside), the striped
           collar (a raised band, 138 high, top and outer walls only) and the pit floor
           (98). The pit's four inner walls, from the collar's inner edge down to the
           floor, were never modelled: once the doors open the camera looks straight
           through that gap. Added, dark and untextured like the floor, shaded from the
           floor's 19 at the foot to 64 at the lip.
           The slab's and the collar's top outer edges are chamfered (18 and 14 units,
           the Remastered rim is bevelled). A chamfer carries the texture of the top it
           rolls off (the top's own projection, so the stripes run over the edge) and
           shades from the top's colour to the wall's.
  1 post   a closed box (its underside never rises above the pit floor). Its four
           vertical edges are chamfered, 10 units.
  2 pod    the missile box, closed on every side. It turns through 180 degrees in play,
           so every side is seen. Eight of its twelve edges are chamfered, 12 units; the
           missile face keeps its four edges square, because the hatch plates hinge on
           its top and bottom edges and cover it edge to edge (a bevel there opened a
           see-through slot at each hinge and cropped the outer missile cells), so only
           its corners are cut, by the four long bevels running into it. A chamfer
           takes the texture of whichever neighbouring side is mapped more coarsely (the
           missile face repeats its texture 2 by 3 times; the ends are not), so no
           missile tile is squeezed onto a bevel.
  3, 4     the two hatch plates on the missile face: closed thin boxes, unchanged.
  5, 6     the doors: single-sided plates flush with the collar. They swing up through
           90 degrees on their outer edges, so their undersides face the camera while
           they open. Each is given a 12 unit thickness below its top: a reversed copy
           carrying the top's UVs at 0.6 of its colour, and four edge faces.

Every chamfer and rebuilt face takes its UV and colour from a least-squares affine map
of the cartridge side it lies on (in the side's own plane), which reproduces the
cartridge's corners to within the rms reported. Nothing the Remastered shows that the
cartridge lacks (the pedestal legs, bolts, missile tips) is added. The doors still pass
through the collar's top as they slide down into it, as on the cartridge.
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
doc, objs = B.build_cartridge(src, "SAM")
SC = 1.0 / B.CELL
report = {"cartridge_triangles": len(doc["tris"]),
          "cartridge_drawn": sum(len(o.data.polygons) for o in objs)}


# ------------------------------------------------------------------ helpers
def P(x, y, z):
    """A pack position (x east, y up, z south, model units) in Blender cells."""
    return Vector(B.to_blender((x, y, z)))


def open_bm(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1.0 * SC)
    bm.normal_update()
    return bm


def close_bm(bm, ob):
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    bm.normal_update()
    bm.to_mesh(ob.data)
    ob.data.update()
    bm.free()


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


class Side:
    """One flat side of a cartridge part: its plane and the affine maps from a position
    in that plane to UV and to baked colour, fitted over the side's own corners. A point
    off the plane is read at its projection (the in-plane basis ignores the normal)."""

    def __init__(self, bm, faces):
        uvl, col, tex = R.layers(bm)
        mode = bm.faces.layers.int["pack_mode"]
        n = Vector()
        for f in faces:
            n += f.normal * f.calc_area()
        self.n = n.normalized()
        a = Vector((1, 0, 0)) if abs(self.n.x) < 0.9 else Vector((0, 1, 0))
        self.e1 = self.n.cross(a).normalized()
        self.e2 = self.n.cross(self.e1)
        self.o = faces[0].verts[0].co.copy()
        X, U, C = [], [], []
        for f in faces:
            for lp in f.loops:
                X.append(self.feat(lp.vert.co))
                U.append(tuple(lp[uvl].uv))
                C.append(tuple(lp[col]))
        X, U, C = np.asarray(X), np.asarray(U, float), np.asarray(C, float)
        self.Mu = np.linalg.lstsq(X, U, rcond=None)[0]
        self.Mc = np.linalg.lstsq(X, C, rcond=None)[0]
        self.rms = float(np.sqrt(((X @ self.Mu - U) ** 2).sum(1).mean()))
        self.clo, self.chi = C.min(0), C.max(0)
        # texture repeats per cell along the plane: a coarse side maps a bevel better
        self.density = float(np.linalg.norm(self.Mu[:2]))
        self.mat = faces[0].material_index
        self.tex = faces[0][tex]
        self.mode = faces[0][mode]

    def feat(self, co):
        d = co - self.o
        return (d.dot(self.e1), d.dot(self.e2), 1.0)

    def uv(self, co):
        return tuple(float(x) for x in np.asarray(self.feat(co)) @ self.Mu)

    def col(self, co):
        c = np.clip(np.asarray(self.feat(co)) @ self.Mc, self.clo, self.chi)
        return tuple(float(x) for x in c)


def put_face(bm, vs, side, colour_sides=None, facing=None, fpart_tag=None):
    """A new face on `vs` with `side`'s material and texture and UVs from `side`'s map.
    Corner colours come from colour_sides[i] (one Side per corner) or from `side`.
    `facing` is a direction the face's normal must agree with."""
    uvl, col, tex = R.layers(bm)
    mode = bm.faces.layers.int["pack_mode"]
    tri = bm.faces.layers.int["pack_tri"]
    f = bm.faces.new(vs)
    f.material_index = side.mat
    f[tex] = side.tex
    f[mode] = side.mode
    f[tri] = -1
    for i, lp in enumerate(f.loops):
        lp[uvl].uv = side.uv(lp.vert.co)
        cs = colour_sides[vs.index(lp.vert)] if colour_sides else side
        lp[col] = cs.col(lp.vert.co) if isinstance(cs, Side) else cs
    if facing is not None:
        f.normal_update()
        if f.normal.dot(facing) < 0:
            f.normal_flip()
    return f


def coarser(*sides):
    return min(sides, key=lambda s: s.density)


def box_sides(bm, faces, lo, hi, tol=1.5 * SC):
    """The six sides of an axis-aligned box part, keyed (axis, +1/-1), found by
    position (every corner on that extreme), not by the cartridge's winding."""
    out = {}
    for a in range(3):
        for s, ext in ((-1, lo[a]), (1, hi[a])):
            fs = [f for f in faces if all(abs(v.co[a] - ext) < tol for v in f.verts)]
            if fs:
                out[(a, s)] = Side(bm, fs)
    return out


def chamfer_box(bm, lo, hi, c, sides, sharp=None):
    """Rebuild an axis-aligned box with its edges chamfered by `c`: six inset sides,
    twelve edge strips and eight corner triangles. Each vertex lies on exactly one of
    the old sides and takes its colour from it; a strip or corner takes its UVs from the
    most coarsely mapped side it touches. `sharp` = (axis, +1/-1) names one side whose
    four edges stay square: it keeps its full width and height, only its corners are
    cut by the strips of the four edges that run into it (an octagon), and nothing at
    that end has a corner triangle. Returns the new faces."""
    made = {}

    def is_sharp(a, sa):
        return sharp is not None and (a, sa) == tuple(sharp)

    def V(s, a):
        if (s, a) not in made:
            p = Vector()
            for i in range(3):
                ext = hi[i] if s[i] > 0 else lo[i]
                square = i == a or is_sharp(i, s[i])
                p[i] = ext if square else ext - s[i] * c
            made[(s, a)] = bm.verts.new(p)
        return made[(s, a)]

    new = []
    signs = (-1, 1)
    corners = ((-1, -1), (1, -1), (1, 1), (-1, 1))
    # the six sides
    for a in range(3):
        b, k = [i for i in range(3) if i != a]
        for sa in signs:
            ring = []
            for ci, (sb, sk) in enumerate(corners):
                s = [0, 0, 0]
                s[a], s[b], s[k] = sa, sb, sk
                s = tuple(s)
                if not is_sharp(a, sa):
                    ring.append(V(s, a))
                    continue
                # an octagon: at each corner, the vertex on the side arrived along,
                # then the one on the side left along
                prev, nxt = corners[ci - 1], corners[(ci + 1) % 4]
                arrive = b if prev[0] == sb else k
                leave = b if nxt[0] == sb else k
                ring += [V(s, arrive), V(s, leave)]
            d = Vector()
            d[a] = sa
            new.append(put_face(bm, ring, sides[(a, sa)], facing=d))
    # the twelve edges, less the four round a sharp side
    for k in range(3):
        a, b = [i for i in range(3) if i != k]
        for sa in signs:
            for sb in signs:
                if is_sharp(a, sa) or is_sharp(b, sb):
                    continue
                s0, s1 = [0, 0, 0], [0, 0, 0]
                s0[a] = s1[a] = sa
                s0[b] = s1[b] = sb
                s0[k], s1[k] = -1, 1
                s0, s1 = tuple(s0), tuple(s1)
                q = [V(s0, a), V(s1, a), V(s1, b), V(s0, b)]
                A, Bs = sides[(a, sa)], sides[(b, sb)]
                d = Vector()
                d[a], d[b] = sa, sb
                new.append(put_face(bm, q, coarser(A, Bs), [A, A, Bs, Bs], facing=d))
    # the eight corners, less the four at a sharp side
    for sx in signs:
        for sy in signs:
            for sz in signs:
                s = (sx, sy, sz)
                if any(is_sharp(i, s[i]) for i in range(3)):
                    continue
                t = [V(s, 0), V(s, 1), V(s, 2)]
                ss = [sides[(i, s[i])] for i in range(3)]
                new.append(put_face(bm, t, coarser(*ss), ss, facing=Vector(s)))
    return new


def chamfer_prism_top(bm, top, bot, c, top_side, wall_sides, inner=None):
    """Chamfer the top outer edge of a straight-walled prism by `c`. `top` and `bot`
    are its corner positions, counter-clockwise from above; wall_sides[i] is the Side of
    the wall from corner i to i+1. The top becomes the ring inset by `c` (or, given the
    `inner` ring, the band between the two), each wall's top drops by `c` along its own
    corner edges, and a strip joins them: the top's projection carries on over it (its
    texture rolls over the edge) and it shades from the top's colour to the wall's.
    Returns (top faces, strips, walls)."""
    n = len(top)
    z0 = top[0].z
    inset = []
    for i in range(n):
        p, q, r = top[i - 1], top[i], top[(i + 1) % n]
        d0 = (q - p)
        d0.z = 0
        d1 = (r - q)
        d1.z = 0
        n0 = Vector((-d0.y, d0.x, 0)).normalized()
        n1 = Vector((-d1.y, d1.x, 0)).normalized()
        m = (n0 + n1) / (1.0 + n0.dot(n1))
        inset.append(bm.verts.new(q + m * c))
    low = [bm.verts.new(t + (b - t) * (c / (t.z - b.z))) for t, b in zip(top, bot)]
    bots = [bm.verts.new(b) for b in bot]
    up = Vector((0, 0, 1))
    tops = []
    if inner is None:
        tops.append(put_face(bm, inset, top_side, facing=up))
    else:
        inn = [bm.verts.new(p) for p in inner]
        for i in range(n):
            j = (i + 1) % n
            tops.append(put_face(bm, [inset[i], inset[j], inn[j], inn[i]], top_side, facing=up))
    strips, walls = [], []
    for i in range(n):
        j = (i + 1) % n
        w = wall_sides[i]
        d = top[j] - top[i]
        out_d = Vector((d.y, -d.x, 0.0)).normalized()     # outward for a CCW ring
        strips.append(put_face(bm, [inset[i], inset[j], low[j], low[i]], top_side,
                               [top_side, top_side, w, w], facing=out_d + up))
        walls.append(put_face(bm, [low[i], low[j], bots[j], bots[i]], w, facing=out_d))
    return tops, strips, walls


def wall_side_of(bm, faces, a, b, tol=1.5 * SC):
    """The Side made of the faces that contain both points' vertical edges (a wall
    whose top edge runs from a to b), found by position."""
    d = (b - a)
    d.z = 0
    nrm = Vector((d.y, -d.x, 0)).normalized()          # outward for a CCW ring
    fs = [f for f in faces if abs(f.normal.z) < 0.5 and
          all(abs((v.co - a).dot(nrm)) < tol * 3 for v in f.verts)]
    return Side(bm, fs)


# ------------------------------------------------------------------ part 0: body
body = objs[0]
bm = open_bm(body)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
FLOOR, COLLAR_W, COLLAR_T, SLAB = (0, 1), range(2, 10), range(10, 18), range(18, 28)
floor = [f for f in bm.faces if f[tri] in FLOOR]
collar_w = [f for f in bm.faces if f[tri] in COLLAR_W]
collar_t = [f for f in bm.faces if f[tri] in COLLAR_T]
slab = [f for f in bm.faces if f[tri] in SLAB]

# the corners, counter-clockwise from above (Blender x east, y north)
SLAB_T = [P(-462, 68, 463), P(474, 68, 463), P(474, 68, -265), P(-462, 68, -265)]
SLAB_B = [P(-462, 0, 463), P(472, 0, 463), P(472, 0, -265), P(-462, 0, -265)]
COL_T = [P(-412, 138, 420), P(418, 138, 420), P(418, 138, -223), P(-412, 138, -223)]
COL_B = [P(-412, 64, 420), P(418, 64, 420), P(418, 64, -223), P(-412, 64, -223)]
PIT_T = [P(-347, 138, 354), P(353, 138, 354), P(353, 138, -158), P(-347, 138, -158)]
PIT_B = [P(-347, 98, 354), P(353, 98, 354), P(353, 98, -158), P(-347, 98, -158)]
for ring in (SLAB_T, SLAB_B, COL_T, COL_B, PIT_T, PIT_B):
    for p in ring:
        if min((v.co - p).length for v in bm.verts) > 1.5 * SC:
            raise SystemExit("no cartridge vertex at %s" % (tuple(round(x * B.CELL) for x in p),))

slab_top = Side(bm, [f for f in slab if f.normal.z > 0.9])
slab_walls = [wall_side_of(bm, slab, SLAB_T[i], SLAB_T[(i + 1) % 4]) for i in range(4)]
col_top = Side(bm, collar_t)
col_walls = [wall_side_of(bm, collar_w, COL_T[i], COL_T[(i + 1) % 4]) for i in range(4)]
floor_side = Side(bm, floor)
report["collar_top_uv_fit_rms"] = round(col_top.rms, 4)
report["slab_uv_fit_rms"] = round(max([slab_top.rms] + [s.rms for s in slab_walls]), 4)
report["collar_wall_uv_fit_rms"] = round(max(s.rms for s in col_walls), 4)

bmesh.ops.delete(bm, geom=collar_w + collar_t + slab, context="FACES")
CH_SLAB, CH_COLLAR = 18.0 * SC, 14.0 * SC
t0, s0, w0 = chamfer_prism_top(bm, SLAB_T, SLAB_B, CH_SLAB, slab_top, slab_walls)
t1, s1, w1 = chamfer_prism_top(bm, COL_T, COL_B, CH_COLLAR, col_top, col_walls, inner=PIT_T)
report["slab_chamfer_units"] = 18
report["collar_chamfer_units"] = 14

# the pit walls: the collar's inner edge down to the floor, facing the pit's centre,
# untextured like the floor, 19 at the foot (the floor's own) to 64 at the lip
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1.0 * SC)
pit_centre = sum(PIT_B, Vector()) / 4
foot_g = floor_side.col(PIT_B[0])[0]
lip_g = 64.0 / 255.0
pit = []


def vnear(p):
    return min(bm.verts, key=lambda v: (v.co - p).length)


for i in range(4):
    j = (i + 1) % 4
    q = [vnear(PIT_T[i]), vnear(PIT_B[i]), vnear(PIT_B[j]), vnear(PIT_T[j])]
    cols = [(g, g, g, 1.0) for g in (lip_g, foot_g, foot_g, lip_g)]
    m = sum((v.co for v in q), Vector()) / 4
    inward = pit_centre - m
    inward.z = 0
    pit.append(put_face(bm, q, floor_side, cols, facing=inward))
report["pit_wall_faces"] = len(pit)
report["body_winding_fixed"] = R.agree_winding(bm, pit + t1 + s1 + w1 + t0 + s0 + w0)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.5 * SC)
report["body_winding_errors"] = winding_errors(bm)
report["body_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
close_bm(bm, body)


# ------------------------------------------------------------------ parts 1, 2: post and pod
def box_extent(bm):
    xs = [v.co for v in bm.verts]
    lo = Vector((min(p.x for p in xs), min(p.y for p in xs), min(p.z for p in xs)))
    hi = Vector((max(p.x for p in xs), max(p.y for p in xs), max(p.z for p in xs)))
    return lo, hi


# the pod: every edge chamfered
pod = objs[2]
bm = open_bm(pod)
lo, hi = box_extent(bm)
sides = box_sides(bm, list(bm.faces), lo, hi)
if len(sides) != 6:
    raise SystemExit("the pod is not a six-sided box: %s" % sorted(sides))
if sides[(1, 1)].density <= max(s_.density for k_, s_ in sides.items() if k_ != (1, 1)):
    raise SystemExit("the missile face is not the pod's north side")
report["pod_uv_fit_rms"] = round(max(s.rms for s in sides.values()), 4)
old = list(bm.faces)
bmesh.ops.delete(bm, geom=old, context="FACES")
# the missile face (north at rest, Blender +y) keeps square edges: the two hatch plates
# hinge on its top and bottom edges and cover it edge to edge, so a bevel there opens a
# see-through slot at each hinge and crops the outer missile cells
new = chamfer_box(bm, lo, hi, 12.0 * SC, sides, sharp=(1, 1))
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.5 * SC)
report["pod_winding_errors"] = winding_errors(bm)
report["pod_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
close_bm(bm, pod)

# the post: its four vertical edges chamfered, the underside left open (it never rises
# above the pit floor)
post = objs[1]
bm = open_bm(post)
lo, hi = box_extent(bm)
sides = box_sides(bm, list(bm.faces), lo, hi)
bmesh.ops.delete(bm, geom=list(bm.faces), context="FACES")
c = 10.0 * SC
# the octagon, counter-clockwise from above: two corners per box corner, each on one side
ring = []
for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
    x = hi.x if sx > 0 else lo.x
    y = hi.y if sy > 0 else lo.y
    # entering the corner along one side, leaving along the other
    first = (1, sy) if (sx, sy) in ((1, -1), (-1, 1)) else (0, sx)
    second = (0, sx) if first[0] == 1 else (1, sy)
    for a, s in (first, second):
        p = Vector((x, y, 0.0))
        other = 1 - a
        p[other] -= (sx if other == 0 else sy) * c
        ring.append((p, sides[(a, s)]))
tv = [bm.verts.new((p.x, p.y, hi.z)) for p, _ in ring]
bv = [bm.verts.new((p.x, p.y, lo.z)) for p, _ in ring]
put_face(bm, tv, sides[(2, 1)], facing=Vector((0, 0, 1)))
for i in range(8):
    j = (i + 1) % 8
    si, sj = ring[i][1], ring[j][1]
    m = 0.5 * (ring[i][0] + ring[j][0]) - 0.5 * (lo + hi)
    m.z = 0
    put_face(bm, [bv[i], bv[j], tv[j], tv[i]], coarser(si, sj), [si, sj, sj, si], facing=m)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.5 * SC)
report["post_winding_errors"] = winding_errors(bm)
report["post_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
close_bm(bm, post)


# ------------------------------------------------------------------ parts 5, 6: doors
def thicken_plate(ob, depth, shade=0.6):
    """A flat one-sided plate given a back `depth` below it (along -normal): a reversed
    copy carrying the top's UVs at `shade` of its colour, and one face per open edge
    that drags the edge's own texels down, at the edge's colour."""
    bm = open_bm(ob)
    uvl, col, tex = R.layers(bm)
    mode = bm.faces.layers.int["pack_mode"]
    tri = bm.faces.layers.int["pack_tri"]
    faces = list(bm.faces)
    n = Vector()
    for f in faces:
        n += f.normal * f.calc_area()
    n.normalize()
    vmap = {v: bm.verts.new(v.co - n * depth) for v in list(bm.verts)}
    new = []
    for f in faces:
        nf = bm.faces.new([vmap[v] for v in reversed(f.verts)])
        nf.material_index, nf[tex], nf[mode], nf[tri] = f.material_index, f[tex], f[mode], -1
        for lo_, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo_[uvl].uv
            c_ = lo_[col]
            ln[col] = (c_[0] * shade, c_[1] * shade, c_[2] * shade, c_[3])
        new.append(nf)
    for e in R.open_edges(bm, faces):
        f = [g for g in e.link_faces if g in faces][0]
        lp = [l_ for l_ in e.link_loops if l_.face is f][0]
        a, b = lp.vert, lp.link_loop_next.vert
        q = bm.faces.new([b, a, vmap[a], vmap[b]])
        q.material_index, q[tex], q[mode], q[tri] = f.material_index, f[tex], f[mode], -1
        uva, uvb = lp[uvl].uv.copy(), lp.link_loop_next[uvl].uv.copy()
        ca, cb = tuple(lp[col]), tuple(lp.link_loop_next[col])
        for l2 in q.loops:
            first = l2.vert in (a, vmap[a])
            l2[uvl].uv = uva if first else uvb
            l2[col] = ca if first else cb
        new.append(q)
    fixed = R.agree_winding(bm, new)
    errs = winding_errors(bm)
    opn = sum(1 for e in bm.edges if e.is_boundary)
    close_bm(bm, ob)
    return fixed, errs, opn


for k in (5, 6):
    fx, er, op = thicken_plate(objs[k], 12.0 * SC)
    report["door%d_winding_errors" % k] = er
    report["door%d_open_edges" % k] = op

# ------------------------------------------------------------------ parts 3, 4: hatches
for k in (3, 4):
    bm = open_bm(objs[k])
    report["hatch%d_winding_errors" % k] = winding_errors(bm)
    report["hatch%d_open_edges" % k] = sum(1 for e in bm.edges if e.is_boundary)
    bm.free()

per_part = [len(o.data.polygons) for o in objs]
report["triangles_per_part"] = per_part
report["fixed_triangles"] = sum(per_part)
report["ratio"] = round(report["fixed_triangles"] / float(report["cartridge_triangles"]), 2)
for o in objs:
    report["pivot_p%d" % o["pack_part"]] = [round(x, 1) for x in o["pack_pivot"]]

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "SAM_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
