"""The Barracks' repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_PYLE.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/PYLE_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building:
two parallel round-roofed huts on a slab, a flag on a pole standing on a round base.

  huts    each a half cylinder of 8 roof strips on a horizontal axis running
          south-south-west to north-north-east, with a flat end wall carrying a centred
          door on the south end only. The north end was never modelled (the console
          camera never looks from there). The hut is mirror-symmetric about the plane
          across its middle, so the north wall is the mirror of the south wall, welded
          onto the roof's north ring and carrying the south wall's UVs and colours. No
          reference shows a north end; the cartridge's own symmetry is the evidence.
          Then the roof is doubled to 16 strips in the hut's own frame (new vertices on
          the fitted circle, both end walls following) and re-shaded from one smooth fit
          of the cartridge's painted strips, which were flat per strip and banded.
  base    the flag pole's base is a regular pentagonal frustum with its north and
          north-west sides left out. Completed from the four corners that are there (the
          missing foot corner is the missing top corner scaled out by the measured
          top-to-foot ratio), doubled to 10 sides and re-shaded as one.
  pole    a three-sided spike, doubled to six sides round its own axis.
  slab    its skirt was modelled on the south and east only: the three sides facing
          west and north get one, mapped the way the cartridge maps its skirts (the rim
          texel stretched down) and shaded from a fit of the existing skirts' colour
          against facing. The rounded south-west end is doubled on its fitted circle.
  flag    three segments (parts 1 to 3, each waves on its own pivot), already two-sided,
          but the two sides lie on top of each other and fight for depth wherever back
          faces are drawn: the back is set 3 model units behind the front and the rim
          between them closed, a thin card per segment. Geometry stays in the rest pose.
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
doc, objs = B.build_cartridge(src, "PYLE")
report = {"cartridge_triangles": sum(p["ntri"] for p in doc["parts"])}
body = objs[0]

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
bm.normal_update()
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

# parts by cartridge triangle index (extract.py order)
(POLE, BASE_TOP, BASE_SIDE, ROOF_W, ROOF_E, END_W, END_E, SLAB_TOP, SKIRT) = range(1, 10)
BY_TRI = {}
for ids, p in (((0, 1, 2), POLE), ((26, 35, 36), BASE_TOP),
               ((54, 62, 63, 64, 65, 66), BASE_SIDE),
               ((5, 6, 7, 8, 9, 10, 11, 12, 13, 21, 22, 23, 24, 25, 27, 34), ROOF_W),
               ((3, 4, 14, 15, 16, 17, 18, 19, 20, 28, 29, 30, 31, 32, 33, 37), ROOF_E),
               (range(45, 52), END_W), (range(38, 45), END_E),
               (range(67, 74), SLAB_TOP),
               ((52, 53, 55, 56, 57, 58, 59, 60, 61, 74, 75, 76), SKIRT)):
    for i in ids:
        BY_TRI[i] = p
for f in bm.faces:
    f[part] = BY_TRI[f[tri]]


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


# ------------------------------------------------------------------ helpers
def plane_mirror(point, normal):
    n = Vector(normal).normalized()
    p0 = Vector(point)

    def m(co):
        return co - 2.0 * (co - p0).dot(n) * n
    return m


def mirror_snap(faces, mirror, candidates, snap):
    """Mirror copy of `faces` through the position map `mirror`, reversed so it faces
    outward again. Each mirrored corner lands on the nearest of `candidates` within
    `snap`, otherwise on a new vertex. UVs and colours are carried from the source
    corner. Returns (new faces, worst snap distance)."""
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
        nf[part] = f[part]
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo[uvl].uv
            ln[col] = lo[col]
        new.append(nf)
    return new, worst


class AxisFrame:
    """A frame for a round part lying on its side: local X across the part, local Y
    world up, local Z along its horizontal axis. In this frame the part's profile is a
    circle in XY and repair.densify_curve / smooth_ring_shade work unchanged."""

    def __init__(self, origin, axis):
        self.o = Vector((origin.x, origin.y, 0.0))
        self.d = Vector((axis.x, axis.y, 0.0)).normalized()
        self.a = Vector((-self.d.y, self.d.x, 0.0))      # up x d: a proper rotation

    def to_local(self, co):
        r = co - self.o
        return Vector((r.dot(self.a), co.z, r.dot(self.d)))

    def to_world(self, lo):
        return self.o + self.a * lo.x + Vector((0.0, 0.0, lo.y)) + self.d * lo.z


def edge_shade_fit(faces):
    """Colour against facing for a vertical band of flat-painted faces (the slab's
    skirt): colour = a0 + a1 cos(t) + b1 sin(t), t the face normal's bearing. Returns
    colour(normal), clipped to the range the cartridge painted."""
    A, Y = [], []
    for f in faces:
        t = math.atan2(f.normal.y, f.normal.x)
        for lp in f.loops:
            A.append([1.0, math.cos(t), math.sin(t)])
            Y.append(tuple(lp[col]))
    A, Y = np.asarray(A), np.asarray(Y, float)
    coef = np.linalg.lstsq(A, Y, rcond=None)[0]
    lo, hi = Y.min(0), Y.max(0)
    rms = float(np.sqrt(((A @ coef - Y) ** 2).sum(1).mean()))

    def colour(n):
        t = math.atan2(n.y, n.x)
        c = np.clip(np.asarray([1.0, math.cos(t), math.sin(t)]) @ coef, lo, hi)
        return tuple(float(x) for x in c)
    return colour, rms


# ------------------------------------------------------------------ huts
def fix_hut(roof_part, end_part, key):
    roof = faces_of(roof_part)
    rv = verts_of(roof)
    # the hut's axis: the long direction of its roof, pointing north
    P = np.asarray([(v.co.x, v.co.y) for v in rv])
    m = P.mean(0)
    _, _, vt = np.linalg.svd(P - m)
    d = Vector((vt[0][0], vt[0][1], 0.0))
    if d.y < 0:
        d = -d
    proj = {v: (v.co - Vector((m[0], m[1], 0.0))).dot(d) for v in rv}
    mid = 0.5 * (min(proj.values()) + max(proj.values()))
    south = [v for v in rv if proj[v] < mid]
    north = [v for v in rv if proj[v] >= mid]
    sc = sum((v.co for v in south), Vector()) / len(south)
    ncen = sum((v.co for v in north), Vector()) / len(north)
    report[key + "_length"] = round((ncen - sc).length * B.CELL, 1)

    # the north end wall: the south wall mirrored across the hut's middle
    centre = 0.5 * (sc + ncen)
    wall_s = faces_of(end_part)
    new_wall, worst = mirror_snap(wall_s, plane_mirror(centre, d), north, 12.0 / B.CELL)
    report[key + "_north_wall_faces"] = len(new_wall)
    report[key + "_north_wall_snap_units"] = round(worst * B.CELL, 1)
    report[key + "_north_wall_winding_fixed"] = R.agree_winding(bm, new_wall)

    # into the hut's own frame; the profile circle is fitted to the south ring
    fr = AxisFrame(sc, d)
    hut_v = verts_of(faces_of(roof_part, end_part))
    for v in hut_v:
        v.co = fr.to_local(v.co)
    ring = [(v.co.x, v.co.y) for v in south]
    cx, cy, r = R.fit_circle(ring)
    report[key + "_profile"] = "r %.1f, centre %.1f above the foot, miss %.1f" % (
        r * B.CELL, (cy - min(p[1] for p in ring)) * B.CELL,
        max(abs(math.hypot(x - cx, y - cy) - r) for x, y in ring) * B.CELL)
    roof = faces_of(roof_part)
    shade = R.smooth_ring_shade(roof, col, (cx, cy))
    _, rms_r = R.fit_uv(roof, uvl, lambda co: (co.x, co.y, co.z, 1.0))
    end_uv, rms_e = R.fit_uv(faces_of(end_part), uvl, lambda co: (co.x, co.y, 1.0))
    report[key + "_uv_fit_rms"] = "roof %.4f end %.4f" % (rms_r, rms_e)
    before = set(hut_v)
    strips = len(roof) // 2
    R.densify_curve(bm, roof, (cx, cy, r))
    roof = faces_of(roof_part)
    ends = faces_of(end_part)
    R.apply_colour(roof, col, shade)
    # a new roof vertex keeps the UV the cut interpolated: the texel the cartridge drew
    # at that point of the chord. The roof's mapping is not planar (fit above), so a
    # re-projection would shift it; the end walls' is planar, so their cuts follow it
    for f in ends:
        for lp in f.loops:
            if lp.vert not in before:
                lp[uvl].uv = end_uv(lp.vert.co)
    report[key + "_strips"] = "%d->%d" % (strips, len(roof) // 2)
    for v in verts_of(faces_of(roof_part, end_part)):
        v.co = fr.to_world(v.co)


fix_hut(ROOF_W, END_W, "hut_west")
fix_hut(ROOF_E, END_E, "hut_east")

# ------------------------------------------------------------------ flag pole base
sides = faces_of(BASE_SIDE)
top = verts_of(faces_of(BASE_TOP))
bc = R.fit_circle([(v.co.x, v.co.y) for v in top])
side_v = verts_of(sides)
feet = [v for v in side_v if v.co.z < 0.1]
top.sort(key=lambda v: R.angle_of(v.co, bc))


def foot_of(v):
    a = R.angle_of(v.co, bc)
    return min(feet, key=lambda w: abs((R.angle_of(w.co, bc) - a + math.pi) % (2 * math.pi) - math.pi))


pairs = {v: foot_of(v) for v in top if v in side_v}
k_out = sum(R.radius_of(pairs[v].co, bc) / R.radius_of(v.co, bc) for v in pairs) / len(pairs)
z_foot = sum(w.co.z for w in feet) / len(feet)
report["base_top_to_foot_ratio"] = round(k_out, 3)
shade_base = R.smooth_ring_shade(sides, col, bc)
like = sides[0]
for v in top:
    if v not in pairs:
        p = Vector((bc[0] + (v.co.x - bc[0]) * k_out, bc[1] + (v.co.y - bc[1]) * k_out, z_foot))
        pairs[v] = bm.verts.new(p)
new_sides = []
for i in range(len(top)):
    a, b = top[i], top[(i + 1) % len(top)]
    if any(a in f.verts and b in f.verts for f in sides):
        continue
    f = bm.faces.new([a, b, pairs[b], pairs[a]])
    f.material_index = like.material_index
    f[tex] = like[tex]
    f[part] = BASE_SIDE
    for lp in f.loops:
        lp[uvl].uv = like.loops[0][uvl].uv
        lp[col] = like.loops[0][col]
    new_sides.append(f)
report["base_sides_added"] = len(new_sides)
report["base_winding_fixed"] = R.agree_winding(bm, new_sides)
R.densify_curve(bm, faces_of(BASE_TOP, BASE_SIDE), (bc[0], bc[1], bc[2]), max_deg=80.0)
R.apply_colour(faces_of(BASE_SIDE), col, shade_base)
report["base_sides"] = "5->%d" % (sum(1 for e in bm.edges
                                      if all(abs(v.co.z - z_foot) < 1e-4 for v in e.verts)
                                      and e.is_boundary))

# ------------------------------------------------------------------ pole
pole = faces_of(POLE)
pb = [v for v in verts_of(pole) if v.co.z < 0.2]
pc = R.fit_circle([(v.co.x, v.co.y) for v in pb])
R.densify_curve(bm, pole, pc, max_deg=130.0)
report["pole_sides"] = "3->%d" % len(faces_of(POLE))

# ------------------------------------------------------------------ slab skirt
top_faces = faces_of(SLAB_TOP)
skirt = faces_of(SKIRT)
z_bot = min(v.co.z for v in verts_of(skirt))
skirt_colour, rms_sk = edge_shade_fit(skirt)
report["skirt_shade_fit_rms"] = round(rms_sk * 255.0, 1)
slab_uv, rms_top = R.fit_uv(top_faces, uvl, lambda co: (co.x, co.y, 1.0))
report["slab_top_uv_fit_rms"] = round(rms_top, 4)
top_set = set(top_faces)
all_v = list(bm.verts)


def below(v):
    """The skirt's foot under a rim vertex: the cartridge's own where it has one."""
    p = Vector((v.co.x, v.co.y, z_bot))
    for w in all_v:
        if w.is_valid and (w.co - p).length < 1e-4:
            return w
    w = bm.verts.new(p)
    all_v.append(w)
    return w


def rim_uv(v):
    """A rim vertex's UV as the slab top states it (the skirt stretches that texel down)."""
    for lp in v.link_loops:
        if lp.face in top_set or lp.face[part] == SLAB_TOP:
            return lp[uvl].uv.copy()
    return Vector(slab_uv(v.co))


def skirt_quad(a, b, like):
    ab, bb = below(a), below(b)
    f = bm.faces.new([a, b, bb, ab])
    f.material_index, f[tex] = like
    f[part] = SKIRT
    uva, uvb = rim_uv(a), rim_uv(b)
    for lp in f.loops:
        lp[uvl].uv = uva if lp.vert in (a, ab) else uvb
    return f


new_skirt = []
for e in list(bm.edges):
    lf = e.link_faces
    if len(lf) == 1 and lf[0][part] == SLAB_TOP:
        new_skirt.append(skirt_quad(e.verts[0], e.verts[1], (skirt[0].material_index, skirt[0][tex])))
report["skirt_sides_added"] = len(new_skirt)

# the rounded south-west end: each rim edge on the fitted circle is cut at its middle,
# the cut pushed out onto the circle; the top gains the sliver, the skirt is rebuilt
arc_pts = [(-0.863, -0.181), (-0.719, -0.478), (-0.457, -0.696), (-0.092, -0.769),
           (0.218, -0.678), (0.441, -0.468)]
rim_top = [v for v in verts_of(faces_of(SLAB_TOP)) if abs(v.co.z - z_bot) > 0.03]
arc = [min(rim_top, key=lambda v: math.hypot(v.co.x - x, v.co.y - y)) for x, y in arc_pts]
ac = R.fit_circle([(v.co.x, v.co.y) for v in arc])
report["slab_arc"] = "r %.1f, miss %.1f" % (ac[2] * B.CELL, max(
    abs(R.radius_of(v.co, ac) - ac[2]) for v in arc) * B.CELL)
arc_new = 0
for a, b in zip(arc, arc[1:]):
    ab, bb = below(a), below(b)
    olds = [f for f in faces_of(SKIRT) if set(f.verts) <= {a, b, ab, bb}]
    like = (olds[0].material_index, olds[0][tex])
    bmesh.ops.delete(bm, geom=olds, context="FACES_ONLY")
    e = bm.edges.get((a, b))
    _, mv = bmesh.utils.edge_split(e, a, 0.5)
    rr = 0.5 * (R.radius_of(a.co, ac) + R.radius_of(b.co, ac))
    a0, a1 = R.angle_of(a.co, ac), R.angle_of(b.co, ac)
    t = a0 + ((a1 - a0 + math.pi) % (2 * math.pi) - math.pi) * 0.5
    # the slab top's mapping is close to planar (fit above): the cut keeps its
    # interpolated UV plus the planar map's change over the push onto the circle
    mid = mv.co.copy()
    mv.co = Vector((ac[0] + rr * math.cos(t), ac[1] + rr * math.sin(t), a.co.z))
    shift = Vector(slab_uv(mv.co)) - Vector(slab_uv(mid))
    for lp in mv.link_loops:
        lp[uvl].uv = lp[uvl].uv + shift
    new_skirt.append(skirt_quad(a, mv, like))
    new_skirt.append(skirt_quad(mv, b, like))
    arc_new += 1
report["slab_arc_edges_doubled"] = arc_new
report["skirt_winding_fixed"] = R.agree_winding(bm, new_skirt)
bm.normal_update()
for f in faces_of(SKIRT):
    c = skirt_colour(f.normal)
    for lp in f.loops:
        lp[col] = c

bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
report["winding_fixed_final"] = R.agree_winding(bm, faces_of(ROOF_W, ROOF_E, END_W, END_E,
                                                             BASE_SIDE, BASE_TOP, SKIRT, SLAB_TOP))
bm.normal_update()
report["body_triangles"] = len(bm.faces)
report["body_open_edges"] = sum(1 for e in bm.edges if e.is_boundary)
bm.to_mesh(body.data)
bm.free()
body.data.update()

# ------------------------------------------------------------------ flag
# each segment's two sides lie within about two model units of one plane and of each
# other, so they fight for depth wherever back faces are drawn. The back is rebuilt as
# the front moved 3 model units behind it (keeping the back's own UVs and colours), and
# the rim between the two is closed, so the segment is a thin closed card that can wave
# on its own pivot without showing a slit.
gap = 3.0 / B.CELL
flag_added = 0
for ob in objs[1:]:
    fm = bmesh.new()
    fm.from_mesh(ob.data)
    fuv, fcol, ftex = R.layers(fm)
    fm.normal_update()
    fm.faces.ensure_lookup_table()
    n = fm.faces[0].normal.copy()
    p0 = fm.faces[0].verts[0].co.copy()
    front = [f for f in fm.faces if f.normal.dot(n) > 0]
    back = [f for f in fm.faces if f.normal.dot(n) <= 0]
    for v in {v for f in front for v in f.verts}:
        v.co = v.co - n * (v.co - p0).dot(n)
    bmesh.ops.remove_doubles(fm, verts=list({v for f in front for v in f.verts}), dist=1e-5)
    fv = list({v for f in front for v in f.verts})
    for v in {v for f in back for v in f.verts}:
        on = v.co - n * (v.co - p0).dot(n)
        v.co = min(fv, key=lambda w: (w.co - on).length).co - n * gap
    bmesh.ops.remove_doubles(fm, verts=list({v for f in back for v in f.verts}), dist=1e-5)
    behind = {}
    for v in fv:
        behind[v] = min((w for f in back for w in f.verts),
                        key=lambda w: (w.co - (v.co - n * gap)).length)
    rim = []
    fs = set(front)
    for e in R.open_edges(fm, front):
        f = [g for g in e.link_faces if g in fs][0]
        lp = [l for l in e.link_loops if l.face is f][0]
        a, b = lp.vert, lp.link_loop_next.vert
        q = fm.faces.new([b, a, behind[a], behind[b]])
        q.material_index = f.material_index
        q[ftex] = f[ftex]
        la, lb = lp, lp.link_loop_next
        for l2 in q.loops:
            src_ = la if l2.vert in (a, behind[a]) else lb
            l2[fuv].uv = src_[fuv].uv
            l2[fcol] = src_[fcol]
        rim.append(q)
    R.agree_winding(fm, rim)
    bmesh.ops.triangulate(fm, faces=[f for f in fm.faces if len(f.verts) > 3])
    fm.normal_update()
    flag_added += len(fm.faces) - (len(front) + len(back))
    report["part%d_flag_open_edges" % ob["pack_part"]] = sum(1 for e in fm.edges if e.is_boundary)
    fm.to_mesh(ob.data)
    fm.free()
    ob.data.update()
report["flag_rim_triangles_added"] = flag_added

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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "PYLE_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
