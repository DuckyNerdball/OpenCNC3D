"""The Obelisk of Light's repair plan: the cartridge model, completed and smoothed.

    blender -b --python tools/art/n64fix/fix_OBLI.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/OBLI_fixed.blend; run.sh then puts
it in the viewer (export_new.py). Every decision below was read off the cartridge mesh
(extract.py, audit.py, inspect_view.py) and checked against the Remastered building,
which shows a leaning blade: a lower block whose front is one flat slope carrying the
lights, with straight side walls, and above it a shaft that curves forward to its tip.

The cartridge draws 18 triangles on 13 vertices (plus 2 shadow triangles that the game
draws from the pack and that are not part of this model), in three sections:

  block   the lower block, base (four corners at height 8) up to a waist ring at about
          height 820: the front slope, the west and the east walls are modelled, all
          planar. Its north wall was left out because the console camera never looks
          from there, so from the north the inside of the front slope showed through.
          The wall is added between the base and the waist, mapped the way the side
          walls are mapped (the same texel density, v from 0 at the base to 1 at the
          waist) and shaded to agree with every face it meets at the shared corners.
          The block stays flat: straight in the cartridge and in the reference.
  shaft   four walls from the waist up to a shoulder ring (height 1233 at the front,
          1336 at the back).
  tip     four triangles from the shoulder ring to one point, which leans south-west.

  The shaft and the tip together are the curved blade, drawn as two straight pieces
  with a kink of about 18 degrees at the shoulder on every corner. Each corner's three
  cartridge points (waist, shoulder, tip) are joined by one parabola in chord length,
  and each of the two pieces is cut into three along it, so the blade bends in six
  steps of about 6 degrees instead of one of 18. Every cartridge vertex stays where
  it was. Each wall keeps its own texture mapping and its baked colour: the new
  corners interpolate both along the cartridge face they cut, which is exact, since
  every cartridge face is mapped and painted linearly.

The base rests on the ground and stays open, as the cartridge has it.
"""
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
doc, objs = B.build_cartridge(src, "OBLI")
body = objs[0]
report = {"cartridge_triangles": len(doc["tris"]),
          "cartridge_drawn": sum(len(o.data.polygons) for o in objs)}

CUTS = 3            # each straight piece of the blade becomes this many


# ------------------------------------------------------------------ helpers
def P(x, y, z):
    """A pack position (x east, y up, z south, model units) in Blender cells."""
    return Vector(B.to_blender((x, y, z)))


def at(bm, p, tol=2.0 / B.CELL):
    """The vertex at a position, to within `tol`."""
    v = min(bm.verts, key=lambda w: (w.co - p).length)
    if (v.co - p).length > tol:
        raise SystemExit("no vertex near %s" % (tuple(round(c * B.CELL) for c in p),))
    return v


def new_face(bm, vs, like, uv_of, colour):
    """A face through `vs` textured like `like`, a (pack_tex, material slot) tuple so
    that no reference to a deleted face is kept; uv_of(vert) and colour(vert) give its
    corners."""
    uvl, col, tex = R.layers(bm)
    f = bm.faces.new(vs)
    f[tex], f.material_index = like
    for lp in f.loops:
        lp[uvl].uv = uv_of(lp.vert)
        lp[col] = colour(lp.vert)
    return f


def corner_data(bm, faces):
    """{vertex: (uv, colour)} over a set of cartridge faces that are mapped and painted
    as one (the two triangles of one wall), and that wall's (pack_tex, material)."""
    uvl, col, tex = R.layers(bm)
    data = {}
    for f in faces:
        for lp in f.loops:
            data[lp.vert] = (Vector(lp[uvl].uv), Vector(lp[col]))
    return data, (faces[0][tex], faces[0].material_index)


def parabola(p0, p1, p2):
    """The quadratic through three points in chord-length parameter, t = 0 at p0 and
    1 at p2. Returns (curve(t), t at p1). Rounds a two-piece polyline along its own
    bend without moving any of its three points."""
    d0, d1 = (p1 - p0).length, (p2 - p1).length
    t1 = d0 / (d0 + d1)

    def curve(t):
        l0 = (t - t1) * (t - 1.0) / t1
        l1 = t * (t - 1.0) / (t1 * (t1 - 1.0))
        l2 = t * (t - t1) / (1.0 - t1)
        return p0 * l0 + p1 * l1 + p2 * l2
    return curve, t1


def boundary_loops(bm):
    """Open-edge loops as ordered vertex lists (as in fix_FACT.py)."""
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


# ------------------------------------------------------------------ the cartridge mesh
bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
part = bm.faces.layers.int.new("fix_part")
BLOCK, SHAFT, TIP = 1, 2, 3

# the thirteen cartridge vertices, by their pack positions
base_sw, base_se = at(bm, P(-406, 8, 175)), at(bm, P(-29, 8, 438))
base_ne, base_nw = at(bm, P(294, 8, -24)), at(bm, P(-81, 8, -287))
waist = {"fw": at(bm, P(-100, 825, -60)), "fe": at(bm, P(86, 825, 70)),
         "be": at(bm, P(199, 837, -90)), "bw": at(bm, P(12, 837, -221))}
shoulder = {"fw": at(bm, P(-102, 1233, -33)), "fe": at(bm, P(54, 1233, 76)),
            "be": at(bm, P(113, 1336, -8)), "bw": at(bm, P(-43, 1335, -117))}
tip = at(bm, P(-107, 1777, 148))
for f in bm.faces:
    zs = [v.co.z for v in f.verts]
    f[part] = BLOCK if max(zs) < 0.9 else TIP if tip in f.verts else SHAFT

# each wall of the blade, named by the two corners along its foot in the order the
# cartridge winds it; the two triangles of a shaft wall are mapped as one
WALLS = [("fw", "fe"), ("be", "bw"), ("fe", "be"), ("bw", "fw")]


def faces_with(vs, prt):
    return [f for f in bm.faces if f[part] == prt and all(v in f.verts for v in vs)]


shaft_walls, tip_walls = {}, {}
for a, b in WALLS:
    shaft_walls[(a, b)] = corner_data(bm, faces_with([waist[a], waist[b]], SHAFT)
                                      + faces_with([shoulder[a], shoulder[b]], SHAFT))
    tip_walls[(a, b)] = corner_data(bm, faces_with([shoulder[a], shoulder[b]], TIP))
    assert len(shaft_walls[(a, b)][0]) == 4 and len(tip_walls[(a, b)][0]) == 3

# ------------------------------------------------------------------ block: the north wall
# mapped like the side walls: u advances 1.11 per cell along the foot, v from 0.006 at
# the base to 0.986 at the waist (the west wall runs 0.004..0.992, the east 0.004..0.994)
side = [f for f in bm.faces if f[part] == BLOCK and base_ne in f.verts][0]
along = (base_ne.co - base_nw.co)
along.z = 0.0
along.normalize()
z0, z1 = base_nw.co.z, waist["bw"].co.z


def north_uv(v):
    return Vector((0.009 + 1.11 * (v.co - base_nw.co).dot(along),
                   0.006 + 0.98 * (v.co.z - z0) / (z1 - z0)))


# shaded to agree with each face it meets: the base corners as the west (153) and east
# (100) walls have them, the waist corners as the shaft's back wall (255 west, 156 east)
back_data = shaft_walls[("be", "bw")][0]
north_col = {base_nw: corner_data(bm, [f for f in bm.faces if f[part] == BLOCK
                                       and base_nw in f.verts])[0][base_nw][1],
             base_ne: corner_data(bm, [side])[0][base_ne][1],
             waist["bw"]: back_data[waist["bw"]][1], waist["be"]: back_data[waist["be"]][1]}
north = new_face(bm, [waist["be"], base_ne, base_nw, waist["bw"]],
                 (side[tex], side.material_index), north_uv, lambda v: north_col[v])
north[part] = BLOCK
report["north_wall_colours"] = "/".join(str(round(north_col[v][0] * 255)) for v in north.verts)

# ------------------------------------------------------------------ blade: shaft and tip
# per corner: waist, shoulder and tip on one parabola, each piece cut into CUTS
chains, bulge = {}, 0.0
for c in waist:
    curve, t1 = parabola(waist[c].co.copy(), shoulder[c].co.copy(), tip.co.copy())
    ends = [waist[c], shoulder[c], tip]
    chain = []
    for piece in range(2):
        t_lo, t_hi = (0.0, t1) if piece == 0 else (t1, 1.0)
        lo, hi = ends[piece], ends[piece + 1]
        chain.append(lo)
        for j in range(1, CUTS):
            s = j / float(CUTS)
            v = bm.verts.new(curve(t_lo + (t_hi - t_lo) * s))
            bulge = max(bulge, (v.co - lo.co.lerp(hi.co, s)).length)
            chain.append(v)
    chain.append(tip)
    chains[c] = chain
report["largest_bulge_units"] = round(bulge * B.CELL, 1)

for f in [f for f in bm.faces if f[part] in (SHAFT, TIP)]:
    bm.faces.remove(f)

new_blade = 0
for a, b in WALLS:
    for j in range(2 * CUTS):
        piece = SHAFT if j < CUTS else TIP
        data, like = (shaft_walls if piece == SHAFT else tip_walls)[(a, b)]
        base = 0 if piece == SHAFT else CUTS
        va0, vb0, va1, vb1 = chains[a][j], chains[b][j], chains[a][j + 1], chains[b][j + 1]

        def at_frac(ch, k):
            """UV and colour of chain vertex k, interpolated along the cartridge wall
            edge it lies on (foot corner to head corner) by its fraction of the piece."""
            lo, hi = data[ch[base]], data[ch[base + CUTS]]
            s = (k - base) / float(CUTS)
            return lo[0].lerp(hi[0], s), lo[1].lerp(hi[1], s)
        uvc = {va0: at_frac(chains[a], j), vb0: at_frac(chains[b], j),
               va1: at_frac(chains[a], j + 1), vb1: at_frac(chains[b], j + 1)}
        if va1 is vb1:          # the last band of the tip closes on the point
            tris = [[va0, vb0, va1]]
        else:                   # the cartridge's own diagonal, foot a to head b
            tris = [[va0, vb0, vb1], [vb1, va1, va0]]
        for t in tris:
            f = new_face(bm, t, like, lambda v: uvc[v][0], lambda v: uvc[v][1])
            f[part] = piece
            new_blade += 1
report["blade_triangles"] = new_blade

# ------------------------------------------------------------------ checks
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "OBLI_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
