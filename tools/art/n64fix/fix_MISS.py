"""The Technology Center's repair plan: the cartridge model, completed and rounded.

    blender -b --python tools/art/n64fix/fix_MISS.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/MISS_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building,
which shows a low three-lobed slab carrying three geodesic glass domes (big north-west,
medium east, small south-west), each on a round collar, the collars joined pairwise by
three pipes.

The cartridge draws 156 triangles (plus 2 shadow triangles that the game draws from the
pack and that are not part of this model), in four kinds of piece:

  slab    a flat 14-sided top at height 245 over a wall down to the ground. The outline
          is three lobes, each drawn as four or five straight chords round the dome it
          carries. Two of the fourteen wall sides, the two facing north, were left out
          because the console camera never looks from there. The outline is rounded:
          each chord is cut into segments of at most 20 degrees round its lobe's fitted
          circle, top and foot alike, and the missing north walls are built with the
          rest. The top keeps the cartridge's own triangles, each one only extended out
          to its new rounded edge; its texture is a top-down projection (planar to a
          fraction of a texel) and so is the wall's, so every new corner is mapped by
          that same projection. The wall is painted flat per side (100 on the north-east
          to 220 on the west); each side's colour is kept at its middle and the rounded
          wall shades smoothly between them along the outline, the two missing sides
          taking their colour from their neighbours.
  collars the three olive dome collars are 6-sided sloped bands with a flat top, 5 sides
          modelled each: the north side is missing. It is added between the existing
          top and foot corners, then every collar is doubled to 12 sides round its
          hub. Like the wall, the sides are painted flat per side, so each side's colour
          is kept at its middle and the collar shades smoothly round the ring.
  pipes   the three pipes are two-faced tents (a semicircle's section drawn as two
          slopes, the texture's shading doing the rest). Each slope is split along a
          new ridge line pushed out onto the half-ellipse through the pipe's own two
          foot corners and ridge, at both ends, so the section is four faces; the end
          of the pipe that meets the small dome's collar stood open above it, so every
          end is closed with a flat cap. UVs and colours of the new ridge lines are the
          exact midpoints of the cartridge's own.
  domes   the three domes are complete (20 facets each, an icosahedral cap) and are the
          faceted glass the reference shows; they are left as the cartridge has them.

Left open, and unseeable: the slab's underside on the ground, the collars' feet (below the
slab's top), the domes' lower rims (inside the collars), the pipes' undersides (below the
slab's top).
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
doc, objs = B.build_cartridge(src, "MISS")
body = objs[0]
report = {"cartridge_triangles": len(doc["tris"]),
          "cartridge_drawn": sum(len(o.data.polygons) for o in objs)}

MAX_SEG_DEG = 20.0      # the slab outline is cut into segments of at most this
CAP_SHADE = 0.7         # a pipe end cap against the pipe's mean colour

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

TOP, WALL, DOME, PIPE = 1, 2, 3, 4
SIDE = {0: 10, 1: 11, 2: 12}          # collar sides, per collar
CTOP = {0: 20, 1: 21, 2: 22}          # collar tops, per collar
for f in bm.faces:
    t, i = f[tex], f[tri]
    f[part] = (TOP if t == 85 else WALL if t == 86 else PIPE if t == 4
               else SIDE[i // 10] if i < 30 else DOME if i >= 48 else 0)


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


# ------------------------------------------------------------------ helpers
def wrap(a):
    """An angle difference folded into -pi..pi."""
    return (a + math.pi) % (2 * math.pi) - math.pi


def polar_lerp(p0, p1, c, t):
    """The point a fraction t of the way from p0 to p1 round the vertical axis through
    c = (x, y): angle and radius interpolate, as does height. A chord between two points
    on one circle becomes an arc of that circle."""
    a0, a1 = R.angle_of(p0, c), R.angle_of(p1, c)
    a = a0 + wrap(a1 - a0) * t
    r = R.radius_of(p0, c) * (1 - t) + R.radius_of(p1, c) * t
    return Vector((c[0] + r * math.cos(a), c[1] + r * math.sin(a), p0.z * (1 - t) + p1.z * t))


def cyclic_keys(keys, period):
    """Piecewise-linear interpolation through (parameter, colour) keys on a cycle of
    length `period`. Used to keep a flat-painted round part's colour at the middle of
    each cartridge side and shade smoothly between; a gap with no key (a side the
    cartridge left out) takes its colour from the keys either side of it."""
    keys = sorted(((p % period, Vector(c)) for p, c in keys), key=lambda k: k[0])

    def colour(p):
        p %= period
        for (p0, c0), (p1, c1) in zip(keys, keys[1:] + [(keys[0][0] + period, keys[0][1])]):
            q = p if p >= p0 else p + period
            if p0 <= q <= p1:
                s = (q - p0) / max(p1 - p0, 1e-9)
                return tuple(c0.lerp(c1, s))
        return tuple(keys[0][1])
    return colour


def mean_colour(faces):
    s = Vector((0.0, 0.0, 0.0, 0.0))
    n = 0
    for f in faces:
        for lp in f.loops:
            s += Vector(lp[col])
            n += 1
    return s / max(n, 1)


def pair_sides(faces):
    """The cartridge's flat sides: its triangles grouped by shared edges between faces
    lying in one plane (the triangle indices of one side are not always consecutive)."""
    left = set(faces)
    groups = []
    while left:
        st = [left.pop()]
        g = []
        while st:
            f = st.pop()
            g.append(f)
            for e in f.edges:
                for nb in e.link_faces:
                    if nb in left and nb.normal.dot(f.normal) > 0.995:
                        left.remove(nb)
                        st.append(nb)
        groups.append(g)
    return groups


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


# ------------------------------------------------------------------ collars
# the flat tops are the cartridge's triangles 30 to 47, six to a collar; each goes with
# the collar whose sides lie nearest
hubs = {}
for k in (0, 1, 2):
    sv = verts_of(faces_of(SIDE[k]))
    hubs[k] = R.fit_circle([(v.co.x, v.co.y) for v in sv if v.co.z > max(w.co.z for w in sv) - 0.01])
for f in bm.faces:
    if 30 <= f[tri] < 48:
        m = f.calc_center_median()
        f[part] = CTOP[min(hubs, key=lambda k: math.hypot(m.x - hubs[k][0], m.y - hubs[k][1]))]

collar_info = []
for k in (0, 1, 2):
    c = hubs[k]
    sides = faces_of(SIDE[k])
    # one colour key per cartridge side, at the side's middle angle
    keys = []
    for pair in pair_sides(sides):
        m = sum((f.calc_center_median() for f in pair), Vector()) / len(pair)
        keys.append((R.angle_of(m, c), mean_colour(pair)))
    shade = cyclic_keys(keys, 2 * math.pi)
    # the missing side: its top edge is the one open edge on the top rim, and its foot
    # corners hang off that edge's ends along the neighbouring sides' open edges
    z_top = max(v.co.z for v in verts_of(sides))
    top_edge = [e for e in bm.edges if e.is_boundary
                and all(abs(v.co.z - z_top) < 1e-3 for v in e.verts)
                and any(f[part] in (SIDE[k], CTOP[k]) for f in e.link_faces)]
    assert len(top_edge) == 1, "collar %d: %d open top edges" % (k, len(top_edge))

    def foot(t):
        return next(e.other_vert(t) for e in t.link_edges
                    if e.is_boundary and e.other_vert(t).co.z < z_top - 0.05)
    # run the new side against its top edge the other way from the top face
    lp = [l for l in top_edge[0].link_loops][0]
    a, b = lp.vert, lp.link_loop_next.vert
    fa = bm.faces.new([b, a, foot(a), foot(b)])
    fa[part] = SIDE[k]
    fa[tex] = sides[0][tex]
    fa.material_index = sides[0].material_index
    for lpp in fa.loops:
        lpp[uvl].uv = (0.0, 0.0)
    # 12 sides: every side (60 degrees) is cut in two and pushed out onto the circle
    R.densify_curve(bm, faces_of(SIDE[k], CTOP[k]), c, max_deg=75.0)
    for f in faces_of(SIDE[k]):
        for lpp in f.loops:
            lpp[col] = shade(R.angle_of(lpp.vert.co, c))
    collar_info.append("%d:hub(%.0f,%.0f)r%.0f" % (k, c[0] * B.CELL, c[1] * B.CELL, c[2] * B.CELL))
report["collars"] = ";".join(collar_info)
report["collar_sides"] = [len(faces_of(SIDE[k])) // 2 for k in (0, 1, 2)]

# ------------------------------------------------------------------ slab: outline
top_faces = faces_of(TOP)
wall_faces = faces_of(WALL)
top_uv, rms_top = R.fit_uv(top_faces, uvl, lambda co: (co.x, co.y, 1.0))
wall_uv, rms_wall = R.fit_uv(wall_faces, uvl, lambda co: (co.x, co.y, co.z, 1.0))
report["slab_top_uv_fit_rms"] = round(rms_top, 5)
report["slab_wall_uv_fit_rms"] = round(rms_wall, 5)

# the outline, in the order the top faces run round it (anticlockwise from above)
out_loops = {}
for f in top_faces:
    for lp in f.loops:
        if sum(1 for g in lp.edge.link_faces if g[part] == TOP) == 1:
            out_loops[lp.vert] = lp.link_loop_next.vert
ring = [next(iter(out_loops))]
while out_loops[ring[-1]] is not ring[0]:
    ring.append(out_loops[ring[-1]])
report["slab_outline_corners"] = len(ring)

# each top corner's foot: the wall vertex straight below it
wall_verts = verts_of(wall_faces)
feet = {}
for v in ring:
    low = [w for w in wall_verts if w.co.z < 0.05]
    feet[v] = min(low, key=lambda w: (w.co.xy - v.co.xy).length)

# each chord goes with the lobe (the collar hub) it lies nearest; each lobe's circle is
# fitted to the outline corners of its own chords
edge_lobe = []
for i, v in enumerate(ring):
    w = ring[(i + 1) % len(ring)]
    m = 0.5 * (v.co + w.co)
    edge_lobe.append(min(hubs, key=lambda k: math.hypot(m.x - hubs[k][0], m.y - hubs[k][1])))
lobes = {}
for k in (0, 1, 2):
    pts = []
    for i, v in enumerate(ring):
        if edge_lobe[i] == k:
            pts += [v.co, ring[(i + 1) % len(ring)].co]
    lobes[k] = R.fit_circle([(p.x, p.y) for p in pts])
report["slab_lobes"] = ";".join("%d:(%.0f,%.0f)r%.0f" % (k, c[0] * B.CELL, c[1] * B.CELL, c[2] * B.CELL)
                                for k, c in lobes.items())

# wall colours: one key per cartridge wall side, at its middle along the outline
s_at = {ring[0]: 0.0}
for i in range(1, len(ring)):
    s_at[ring[i]] = s_at[ring[i - 1]] + (ring[i].co.xy - ring[i - 1].co.xy).length
perimeter = s_at[ring[-1]] + (ring[0].co.xy - ring[-1].co.xy).length
wall_keys = []
for pair in pair_sides(wall_faces):
    tops = [v for f in pair for v in f.verts if v in s_at]
    tops = list(dict.fromkeys(tops))
    if len(tops) != 2:
        continue
    sa, sb = s_at[tops[0]], s_at[tops[1]]
    if abs(sa - sb) > perimeter / 2:          # the side that closes the ring
        mid = (max(sa, sb) + (min(sa, sb) + perimeter)) / 2
    else:
        mid = (sa + sb) / 2
    wall_keys.append((mid, mean_colour(pair)))
report["slab_wall_sides_modelled"] = len(wall_keys)
wall_shade = cyclic_keys(wall_keys, perimeter)

# the new corners along every chord, top and foot, shared by the top and the wall
chains = []           # per chord: (top chain, foot chain, outline parameter per step)
seg_total = 0
for i, v in enumerate(ring):
    w = ring[(i + 1) % len(ring)]
    c = lobes[edge_lobe[i]]
    span = abs(wrap(R.angle_of(w.co, c) - R.angle_of(v.co, c)))
    n = max(1, int(math.ceil(math.degrees(span) / MAX_SEG_DEG - 1e-6)))
    seg_total += n
    tc, fc = [v], [feet[v]]
    for j in range(1, n):
        t = j / float(n)
        tc.append(bm.verts.new(polar_lerp(v.co, w.co, c, t)))
        fc.append(bm.verts.new(polar_lerp(feet[v].co, feet[w].co, c, t)))
    tc.append(w)
    fc.append(feet[w])
    s0 = s_at[v]
    s1 = s_at[w] if i + 1 < len(ring) else perimeter
    chains.append((tc, fc, [s0 + (s1 - s0) * j / float(n) for j in range(n + 1)]))
report["slab_outline_segments"] = seg_total

# the top: every cartridge triangle on the outline is extended out to its rounded edge,
# its old corners keeping their UV and colour, the new ones mapped by the top's own
# projection and coloured along the chord they came from
chain_of = {(ch[0][0], ch[0][-1]): ch for ch in chains}
like_top = (top_faces[0][tex], top_faces[0].material_index)
new_top = 0
for f in list(top_faces):
    corners = [(lp.vert, Vector(lp[uvl].uv), Vector(lp[col])) for lp in f.loops]
    if not any((a, b) in chain_of for (a, _, _), (b, _, _) in zip(corners, corners[1:] + corners[:1])):
        continue
    poly = []
    for (a, ua, ca), (b, ub, cb) in zip(corners, corners[1:] + corners[:1]):
        poly.append((a, ua, ca))
        ch = chain_of.get((a, b))
        if ch:
            n = len(ch[0]) - 1
            for j in range(1, n):
                poly.append((ch[0][j], Vector(top_uv(ch[0][j].co)), ca.lerp(cb, j / float(n))))
    bm.faces.remove(f)
    nf = bm.faces.new([p[0] for p in poly])
    nf[tex], nf.material_index = like_top
    nf[part] = TOP
    for lp, (_, u, cc) in zip(nf.loops, poly):
        lp[uvl].uv = u
        lp[col] = tuple(cc)
    res = bmesh.ops.triangulate(bm, faces=[nf], quad_method="BEAUTY", ngon_method="BEAUTY")
    new_top += len(res["faces"])
report["slab_top_rebuilt_triangles"] = new_top

# the wall: rebuilt whole, every side (the two missing north ones too) as a strip of
# quads under its rounded top edge, run against the top's direction so the two agree
like_wall = (wall_faces[0][tex], wall_faces[0].material_index)
for f in list(wall_faces):
    bm.faces.remove(f)
for tc, fc, ss in chains:
    for j in range(len(tc) - 1):
        vs = [tc[j + 1], tc[j], fc[j], fc[j + 1]]
        par = {tc[j + 1]: ss[j + 1], tc[j]: ss[j], fc[j]: ss[j], fc[j + 1]: ss[j + 1]}
        nf = bm.faces.new(vs)
        nf[tex], nf.material_index = like_wall
        nf[part] = WALL
        for lp in nf.loops:
            lp[uvl].uv = wall_uv(lp.vert.co)
            lp[col] = wall_shade(par[lp.vert])

# ------------------------------------------------------------------ pipes
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
                for nb in e.link_faces:
                    if nb in left:
                        left.remove(nb)
                        st.append(nb)
        groups.append(g)
    return groups


pipe_report = []
for g in islands(faces_of(PIPE)):
    like = (g[0][tex], g[0].material_index)
    data = {}
    for f in g:
        for lp in f.loops:
            data[lp.vert] = (Vector(lp[uvl].uv), Vector(lp[col]))
    vs = list(data)
    assert len(vs) == 6, "a pipe of %d corners" % len(vs)
    # the ridge is the pipe's two highest corners; the ends split along it
    ridge2 = sorted(vs, key=lambda v: v.co.z)[-2:]
    axis = ridge2[1].co - ridge2[0].co
    axis.z = 0.0
    axis.normalize()
    order = sorted(vs, key=lambda v: v.co.dot(axis))
    ends = [order[:3], order[3:]]
    sections = []
    for e3 in ends:
        ridge = max(e3, key=lambda v: v.co.z)
        lo = [v for v in e3 if v is not ridge]
        side = Vector((-axis.y, axis.x, 0.0))
        left_v, right_v = sorted(lo, key=lambda v: v.co.dot(side))
        cc = 0.5 * (left_v.co + right_v.co)
        a_ax, b_ax = right_v.co - cc, ridge.co - cc
        h = math.sqrt(0.5)
        pl = bm.verts.new(cc - a_ax * h + b_ax * h)
        pr = bm.verts.new(cc + a_ax * h + b_ax * h)
        for p, (x, y) in ((pl, (left_v, ridge)), (pr, (ridge, right_v))):
            data[p] = (data[x][0].lerp(data[y][0], 0.5), data[x][1].lerp(data[y][1], 0.5))
        sections.append([left_v, pl, ridge, pr, right_v])
    for f in g:
        bm.faces.remove(f)
    s0, s1 = sections
    mid_axis = sum((v.co for v in vs), Vector()) / 6.0
    along = (s1[2].co - s0[2].co).normalized()
    made = 0
    for j in range(4):
        q = [s0[j], s0[j + 1], s1[j + 1], s1[j]]
        nf = bm.faces.new(q)
        nf.normal_update()
        m = nf.calc_center_median()
        radial = m - mid_axis
        radial -= along * radial.dot(along)
        if nf.normal.dot(radial) < 0:
            nf.normal_flip()
        nf[tex], nf.material_index = like
        nf[part] = PIPE
        for lp in nf.loops:
            lp[uvl].uv = data[lp.vert][0]
            lp[col] = tuple(data[lp.vert][1])
        made += 2
    # the end caps, flat, fanned from the left foot corner, facing out of the pipe
    for sec, sign in ((s0, -1.0), (s1, 1.0)):
        mean = sum((data[v][1] for v in sec), Vector((0.0, 0.0, 0.0, 0.0))) / len(sec)
        cap_col = (mean[0] * CAP_SHADE, mean[1] * CAP_SHADE, mean[2] * CAP_SHADE, mean[3])
        width = (sec[4].co - sec[0].co).length
        for j in range(1, 4):
            t = bm.faces.new([sec[0], sec[j], sec[j + 1]])
            t.normal_update()
            if t.normal.dot(along) * sign < 0:
                t.normal_flip()
            t[tex], t.material_index = like
            t[part] = PIPE
            for lp in t.loops:
                u = (lp.vert.co - sec[0].co).dot((sec[4].co - sec[0].co).normalized()) / width
                v0 = data[sec[0]][0]
                lp[uvl].uv = (data[sec[0]][0].x + (data[sec[4]][0].x - v0.x) * u, v0.y)
                lp[col] = cap_col
            made += 1
    pipe_report.append(made)
report["pipe_triangles"] = pipe_report

# ------------------------------------------------------------------ checks
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.normal_update()
report["winding_errors"] = winding_errors(bm)
loops = boundary_loops(bm)
report["open_loops"] = ";".join(
    "%d edges z %.0f..%.0f" % (len(L), min(v.co.z for v in L) * B.CELL, max(v.co.z for v in L) * B.CELL)
    for L in sorted(loops, key=len, reverse=True))
report["by_part"] = {n: len(faces_of(*p)) for n, p in
                     (("slab_top", (TOP,)), ("wall", (WALL,)), ("collars", tuple(SIDE.values()) + tuple(CTOP.values())),
                      ("domes", (DOME,)), ("pipes", (PIPE,)))}
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "MISS_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
