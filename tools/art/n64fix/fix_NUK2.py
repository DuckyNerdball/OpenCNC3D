"""The Advanced Power Plant's repair plan: the cartridge model, completed and doubled.

    blender -b --python tools/art/n64fix/fix_NUK2.py -- <src_dir> <out_dir>

<src_dir> is what extract.py wrote. Writes <out_dir>/NUK2_fixed.blend; run.sh then puts
it in the viewer (export_new.py). The building is the Power Plant's bigger sibling, and
the repair follows fix_NUKE.py part for part. Every decision below was read off the
cartridge mesh (extract.py, audit.py, inspect_view.py) and checked against the Remastered
building: two round cooling towers, a C-shaped block wrapped round the north-west one, a
tank, and pipes.

  towers  two, north-west and south-east, 12 sides each, only the 8 south-facing sides
          modelled (the funnel inside has all 12, so every side angle is stated). Each is
          completed with a mirror copy of its south side, which meets the existing edges,
          then doubled to 24 sides. The lower and upper wall bands are re-shaded each
          from one smooth fit of the cartridge's own colours, so the rebuilt back does
          not read as a second half, and the crease at the waist stays.
  arc     a low drum under a sloped roof, both modelled on the outside only. The drum is
          closed where it is cut off at the tower: a flat end at each end of the C, in
          the vertical plane its ends already lie in. Above the drum the block gets an
          inner wall facing the tower, an end cap at each end and a wall under the
          roof's outer edge, set back so the one post the cartridge models there stays
          in front of it (the Remastered building shows that slot as a recess with posts).
  tank    an open vessel on the cartridge: its south walls face out, its north walls
          face in, over a dark floor. Kept open; every wall is turned to face out and
          given an inner skin 8 model units inside it, with a rim between, so it reads
          as a vessel from every side. The floor's one missing triangle is filled. It
          keeps its 8 sides: at its size a side is as long as a tower's side at 24.
  pipes   the four green pipes are two faces each: closed with their convex hulls.
  post    the one post under the roof edge is a plate: given a thin back.
  slab    already closed (its underside is never seen) and stays.
  plates  the two coolant plates are separate animated parts and stay as they are.
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402
import repair as R  # noqa: E402

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
B.reset_scene()
doc, objs = B.build_cartridge(src, "NUK2")
body = objs[0]

bm = bmesh.new()
bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
uvl, col, tex = R.layers(bm)
tri = bm.faces.layers.int["pack_tri"]
part = bm.faces.layers.int.new("fix_part")

(TOWER_NW, TOWER_SE, DRUM, DRUMTOP, ROOF, SLAB, PIPE, POST, INNER, TANK, FLOOR,
 TANK_IN) = range(1, 13)
# cartridge triangle ranges, read off the extract: the north-west tower is 51-94, the
# south-east one 95-138; the tank's floor is five texture-89 triangles, the drum top is
# five texture-13 triangles fanned from the drum's west end
for f in bm.faces:
    t, i = f[tex], f[tri]
    if t == 90:
        f[part] = TOWER_NW if i <= 94 else TOWER_SE
    elif t in (91, 92):
        f[part] = ROOF
    elif t == -1:
        f[part] = POST
    elif t == 89:
        f[part] = FLOOR if i in (34, 46, 47, 49, 50) else DRUM
    elif t == 13:
        f[part] = DRUMTOP if i in (145, 148, 150, 151, 161) else SLAB
    elif t == 11:
        f[part] = PIPE
    elif t == 4:
        f[part] = TANK


def faces_of(*parts):
    return [f for f in bm.faces if f.is_valid and f[part] in parts]


def verts_of(faces):
    return list({v for f in faces for v in f.verts})


def radial(f, c, k=0.3):
    m = f.calc_center_median()
    r = Vector((m.x - c[0], m.y - c[1], 0.0))
    return r.length > 1e-9 and f.normal.dot(r.normalized()) > k


def mean_colour(faces):
    s = [0.0] * 4
    n = 0
    for f in faces:
        for lp in f.loops:
            for i in range(4):
                s[i] += lp[col][i]
            n += 1
    return [x / max(n, 1) for x in s]


def along_arc(verts, c):
    """Vertices of an open arc in order round `c`, starting after the arc's widest gap
    in angle (so an arc that runs through 0 degrees is not cut in two)."""
    a = sorted(((R.angle_of(v.co, c) % (2 * math.pi), v) for v in verts), key=lambda p: p[0])
    n = len(a)
    gaps = [((a[(i + 1) % n][0] - a[i][0]) % (2 * math.pi), i) for i in range(n)]
    k = max(gaps, key=lambda g: g[0])[1]
    return [v for _, v in a[k + 1:] + a[:k + 1]]


def surface_z(faces):
    """z of the given faces straight under (x, y), or None where there is none."""
    vs, idx, polys = [], {}, []
    for f in faces:
        p = []
        for v in f.verts:
            if v not in idx:
                idx[v] = len(vs)
                vs.append(v.co.copy())
            p.append(idx[v])
        polys.append(p)
    tree = BVHTree.FromPolygons(vs, polys)

    def z_at(x, y):
        hit = tree.ray_cast(Vector((x, y, 5.0)), Vector((0.0, 0.0, -1.0)))
        return hit[0].z if hit[0] is not None else None
    return z_at


def new_face(vs, like, fpart, uv_of, colour):
    f = bm.faces.new(vs)
    f[part] = fpart
    f[tex] = like[tex]
    f.material_index = like.material_index
    for lp in f.loops:
        lp[uvl].uv = uv_of(lp.vert.co)
        lp[col] = colour
    return f


report = {"cartridge_triangles": len(bm.faces) + sum(len(o.data.polygons) for o in objs[1:])}


# ------------------------------------------------------------------ towers
def tower(tp, name):
    tw = faces_of(tp)
    top = max(v.co.z for v in verts_of(tw))
    rim = [v.co for v in verts_of(tw) if abs(v.co.z - top) < 0.01]
    tc = R.fit_circle([(p.x, p.y) for p in rim])
    report[name + "_axis"] = [round(x, 4) for x in tc]
    outer = [f for f in tw if radial(f, tc)]
    # the wall is two bands (flared foot to waist, waist to rim), painted separately on
    # the cartridge: one smooth fit per band keeps the crease at the waist
    zs = sorted({round(v.co.z, 2) for f in outer for v in f.verts})
    waist = [z for z in zs if zs[0] + 0.05 < z < zs[-1] - 0.05]
    waist_z = sum(waist) / len(waist)

    def band(f):
        return f.calc_center_median().z > waist_z

    shade = {b: R.smooth_ring_shade([f for f in outer if band(f) == b], col, tc) for b in (False, True)}
    axes = R.symmetry_axes(verts_of(tw), tc)
    # weld at 15 model units as on the Power Plant: the cartridge's side angles are
    # symmetric only to about 11, and no two tower vertices are closer than 100.
    # The mirror copy carries its source face's UVs (the top-down projection was
    # painted for the front only)
    new, axis_deg = R.complete_by_mirror(bm, outer, tc, [a for a, _ in axes], uv_pred=None,
                                         colour=None, tol=15.0 / B.CELL)
    for f in new:
        f[part] = tp
    report[name + "_mirror_candidates"] = [(a, round(m, 2)) for a, m in axes]
    report[name + "_mirror_deg"] = axis_deg
    report[name + "_faces_added"] = len(new)
    R.densify_curve(bm, faces_of(tp), tc)
    wall = [f for f in faces_of(tp) if radial(f, tc)]
    for b in (False, True):
        R.apply_colour([f for f in wall if band(f) == b], col, shade[b])
    return tc


tc_nw = tower(TOWER_NW, "tower_nw")
tc_se = tower(TOWER_SE, "tower_se")

# ------------------------------------------------------------------ tank
tk = faces_of(TANK)
tk_c = R.fit_circle([(v.co.x, v.co.y) for v in verts_of(tk)])
report["tank_axis"] = [round(x, 4) for x in tk_c]
tk_out = [f for f in tk if radial(f, tk_c, 0.0)]
tk_in = [f for f in tk if f not in tk_out]
shade_tk_out = R.smooth_ring_shade(tk_out, col, tk_c)
shade_tk_in = R.smooth_ring_shade(tk_in, col, tk_c)
report["tank_walls_out_in"] = (len(tk_out), len(tk_in))
# the north walls face in (the cartridge shows their inside over the south rim): turn
# them to face out, so the whole ring is one outer skin
for f in tk_in:
    f.normal_flip()
# the floor is an octagon fanned from its west corner with the southern triangle left
# out; fill it, mapped and shaded like the rest of the floor
fl = faces_of(FLOOR)
fl_verts = verts_of(fl)
floor_added = 0
tank_bottom = verts_of(tk)
bottom_ring = sorted([v for v in tank_bottom if v.co.z < 0.15],
                     key=lambda v: R.angle_of(v.co, tk_c))
for a, b in zip(bottom_ring, bottom_ring[1:] + bottom_ring[:1]):
    e = bm.edges.get([a, b])
    if e is not None and any(g[part] == FLOOR for g in e.link_faces):
        continue
    # a side of the octagon with no floor under it: close it to the fan's corner
    others = [v for v in fl_verts if v not in (a, b)]
    for c in others:
        if bm.edges.get([a, c]) and bm.edges.get([b, c]):
            t = bm.faces.new([a, b, c])
            t.normal_update()
            if t.normal.z < 0:
                t.normal_flip()
            t[part] = FLOOR
            t[tex] = fl[0][tex]
            t.material_index = fl[0].material_index
            for lp in t.loops:
                lp[uvl].uv = fl[0].loops[0][uvl].uv
                lp[col] = tuple(fl[0].loops[0][col])
            floor_added += 1
            break
report["tank_floor_triangles_added"] = floor_added
# the floor belongs to the vessel's INNER surface (it faces up into the cavity), so it
# is cut loose from the outer walls' feet; the inner skin added below stands on it
bmesh.ops.split(bm, geom=faces_of(FLOOR))
# the tank stays at 8 sides: at its radius (about 148 model units) a side is as long as
# a side of the towers at 24, and the smooth re-shade below rounds it

# ------------------------------------------------------------------ arc: drum and roof
drum = faces_of(DRUM)
foot = [v.co for v in verts_of(drum) if v.co.z < 0.05]
ac = R.fit_circle([(p.x, p.y) for p in foot])
report["arc_axis"] = [round(x, 4) for x in ac]
dt_uv, dt_rms = R.fit_uv(faces_of(DRUMTOP), uvl, lambda co: (co.x, co.y, 1.0))
dt_col = tuple(mean_colour(faces_of(DRUMTOP)))
# the roof's outer edge as the cartridge states it (7 vertices): the recess wall stands
# on these, not on the doubled edge, since it is set back in shadow under the overhang
rv0 = verts_of(faces_of(ROOF))
out_z0 = min(v.co.z for v in rv0)
# (kept as positions: the densify below rebuilds the roof's vertices)
outer_cart = [v.co.copy() for v in rv0 if abs(v.co.z - out_z0) < 0.01]
R.densify_curve(bm, faces_of(DRUM, DRUMTOP, ROOF), ac)

roof = faces_of(ROOF)
rv = verts_of(roof)
top_z = max(v.co.z for v in rv)
out_z = min(v.co.z for v in rv)
inner_top = along_arc([v for v in rv if abs(v.co.z - top_z) < 0.01], ac)
outer_edge = along_arc([v for v in rv if abs(v.co.z - out_z) < 0.01], ac)
dv = verts_of(faces_of(DRUM))
drum_ground = along_arc([v for v in dv if v.co.z < 0.05], ac)
dtv = verts_of(faces_of(DRUMTOP))
drum_top_ring = along_arc([v for v in dtv if v.co.z > 0.2], ac)
d_w, d_e = drum_top_ring[0], drum_top_ring[-1]
g_w, g_e = drum_ground[0], drum_ground[-1]
chord = bm.edges.get([d_w, d_e])
if chord is None:
    raise SystemExit("the drum top's straight inner edge was not found")
report["arc_ends"] = [[round(c, 3) for c in v.co] for v in (d_w, d_e)]

concrete = next(f for f in faces_of(SLAB))
k_tex = 2.0                  # the slab repeats its concrete about twice per cell
wall_shade = 0.55
dcol = mean_colour(faces_of(DRUM))
wall_col = (dcol[0] * wall_shade, dcol[1] * wall_shade, dcol[2] * wall_shade, 1.0)
cdir = Vector((d_e.co.x - d_w.co.x, d_e.co.y - d_w.co.y, 0.0))
clen = cdir.length
cdir.normalize()


def on_chord(p):
    """The point of the drum top's straight inner edge nearest to p (xy), with its z."""
    t = (Vector((p.x - d_w.co.x, p.y - d_w.co.y, 0.0))).dot(cdir) / clen
    return t, d_w.co.lerp(d_e.co, t)


def split_chord_at(p):
    """Split the drum top's inner edge where p projects onto it; returns the new vertex.
    The drum-top faces on the edge get the drum top's own planar mapping there."""
    t, q = on_chord(p)
    # find the piece of the (possibly already split) chord that holds t
    for e in [e for e in bm.edges if e.is_valid and all(abs(on_chord(v.co)[1].z - v.co.z) < 1e-4
                                                          and (on_chord(v.co)[1] - v.co).length < 1e-4
                                                          for v in e.verts)]:
        ta, tb = sorted(on_chord(v.co)[0] for v in e.verts)
        if ta < t < tb:
            a = min(e.verts, key=lambda v: on_chord(v.co)[0])
            fac = (t - ta) / (tb - ta)
            _, nv = bmesh.utils.edge_split(e, a, fac)
            nv.co = q
            for lp in nv.link_loops:
                lp[uvl].uv = dt_uv(nv.co)
                lp[col] = dt_col
            return nv
    raise SystemExit("could not split the drum top's inner edge")


def split_roof_end(o, i, dist):
    """Split the roof's end edge from outer vertex o toward inner vertex i, `dist` in from
    o (horizontally); the roof's own UV and colour are interpolated there."""
    e = bm.edges.get([o, i])
    if e is None:
        raise SystemExit("roof end edge not found")
    span = Vector((i.co.x - o.co.x, i.co.y - o.co.y, 0.0)).length
    fac = min(dist / span, 0.9)
    before = {f: ({lp.vert: (lp[uvl].uv.copy(), tuple(lp[col])) for lp in f.loops})
              for f in e.link_faces}
    _, nv = bmesh.utils.edge_split(e, o, fac)
    nv.co = o.co.lerp(i.co, fac)
    for lp in nv.link_loops:
        d = before.get(lp.face)
        if d and o in d and i in d:
            lp[uvl].uv = d[o][0].lerp(d[i][0], fac)
            lp[col] = tuple(d[o][1][k] * (1 - fac) + d[i][1][k] * fac for k in range(4))
    return nv


# the one post under the roof's outer edge: how far in from the edge it reaches, so the
# wall under the edge can stand behind it
post_v = verts_of(faces_of(POST))


def outer_r_at(a):
    best = min(outer_edge, key=lambda v: abs((R.angle_of(v.co, ac) - a + math.pi) % (2 * math.pi) - math.pi))
    return R.radius_of(best.co, ac)


post_depth = max(outer_r_at(R.angle_of(v.co, ac)) - R.radius_of(v.co, ac) for v in post_v)
inset = max(0.06, post_depth + 0.03)
report["recess_inset"] = round(inset, 3)

roof_z = surface_z(faces_of(ROOF))
top_z_at = surface_z(faces_of(DRUMTOP))

# the ends: the drum top's inner edge is split where the roof's inner end and the
# recess wall's foot meet it, so the new walls share its vertices
recess_cols = [v for v in outer_edge if min((v.co - p).length for p in outer_cart) < 1e-5]
fascia_top = []
for k, o in enumerate(recess_cols):
    if k in (0, len(recess_cols) - 1):
        it = inner_top[0] if k == 0 else inner_top[-1]
        fascia_top.append(split_roof_end(o, it, inset))
    else:
        d = Vector((ac[0] - o.co.x, ac[1] - o.co.y, 0.0)).normalized() * inset
        p = Vector((o.co.x + d.x, o.co.y + d.y, 0.0))
        z = roof_z(p.x, p.y)
        p.z = (z if z is not None else o.co.z) - 0.002
        fascia_top.append(bm.verts.new(p))
roof_edges = [e for e in bm.edges if any(f[part] == ROOF for f in e.link_faces)]


def roof_crossings(a, b):
    """Where the segment a-b (seen from above) crosses the roof's own edges, as new
    vertices a hair under the roof. A wall top that only met the roof at its ends would
    dip under the roof's triangles in places and stand through them in others (a line
    across the roof); breaking it wherever it crosses a roof edge keeps each piece
    inside one flat roof triangle, so the top follows the roof. Under, not over: the
    roof's overhang hides the join from every camera above the horizon."""
    from mathutils.geometry import intersect_line_line_2d
    pts = []
    for e in roof_edges:
        if a in e.verts or b in e.verts:
            continue
        x = intersect_line_line_2d(a.co.xy, b.co.xy, e.verts[0].co.xy, e.verts[1].co.xy)
        if x is None:
            continue
        t = (x - a.co.xy).length / max((b.co.xy - a.co.xy).length, 1e-9)
        if 1e-3 < t < 1 - 1e-3:
            z = roof_z(x.x, x.y)
            pts.append((t, Vector((x.x, x.y, (z if z is not None else a.co.z) - 0.002))))
    pts.sort(key=lambda p: p[0])
    out = []
    for t, p in pts:
        if out and (p - out[-1].co).length < 1e-4:
            continue
        out.append(bm.verts.new(p))
    return out

x_w = split_chord_at(inner_top[0].co)
f_w = split_chord_at(fascia_top[0].co)
x_e = split_chord_at(inner_top[-1].co)
f_e = split_chord_at(fascia_top[-1].co)


def foot_of(p):
    z = top_z_at(p.x, p.y)
    return bm.verts.new((p.x, p.y, (z if z is not None else d_w.co.z) - 0.003))


fascia_foot = [f_w] + [foot_of(v.co) for v in fascia_top[1:-1]] + [f_e]
inner_foot = [x_w] + [foot_of(v.co) for v in inner_top[1:-1]] + [x_e]


def strip(tops, feet, facing_centre, mids=None):
    """Vertical walls between two vertex rows, concrete, UV by run length and height.
    mids(a, b) may give extra vertices along the top between two columns."""
    run = [0.0]
    for a, b in zip(tops, tops[1:]):
        run.append(run[-1] + (b.co.xy - a.co.xy).length)
    new = []
    for i in range(len(tops) - 1):
        extra = mids(tops[i], tops[i + 1]) if mids else []
        q = [tops[i], feet[i], feet[i + 1], tops[i + 1]] + list(reversed(extra))
        uv = {tops[i]: run[i], feet[i]: run[i], feet[i + 1]: run[i + 1], tops[i + 1]: run[i + 1]}
        for v in extra:
            uv[v] = run[i] + (v.co.xy - tops[i].co.xy).length
        f = new_face(q, concrete, INNER, lambda co: (0.0, 0.0), wall_col)
        for lp in f.loops:
            lp[uvl].uv = (uv[lp.vert] * k_tex, lp.vert.co.z * k_tex)
        f.normal_update()
        m = f.calc_center_median()
        toward = Vector((ac[0] - m.x, ac[1] - m.y, 0.0))
        if (f.normal.dot(toward) < 0) == facing_centre:
            f.normal_flip()
        new.append(f)
    return new


inner_wall = strip(inner_top, inner_foot, True)
recess = strip(fascia_top, fascia_foot, False, mids=roof_crossings)
report["inner_wall_faces"] = len(inner_wall)
report["recess_wall_faces"] = len(recess)


def chord_uv(co):
    return (Vector((co.x - d_w.co.x, co.y - d_w.co.y, 0.0)).dot(cdir) * k_tex, co.z * k_tex)


def cap(vs, outward, colour):
    """A flat end in the plane the arc is cut off in, fanned from its first corner."""
    tris = []
    for a, b in zip(vs[1:], vs[2:]):
        t = new_face([vs[0], a, b], concrete, INNER, chord_uv, colour)
        t.normal_update()
        if t.normal.dot(outward) < 0:
            t.normal_flip()
        tris.append(t)
    return tris


# the C is cut off by ONE vertical plane (both roof ends and the drum top's straight
# inner edge lie in it), so both ends face the same way: across the plane, away from
# the drum and toward the tower it wraps
across = Vector((-cdir.y, cdir.x, 0.0))
mid = outer_edge[len(outer_edge) // 2].co
if across.dot(Vector((mid.x - d_w.co.x, mid.y - d_w.co.y, 0.0))) > 0:
    across = -across
caps = []
for first in (True, False):
    g = g_w if first else g_e
    d = d_w if first else d_e
    fx, xx = (f_w, x_w) if first else (f_e, x_e)
    ft = fascia_top[0] if first else fascia_top[-1]
    it = inner_top[0] if first else inner_top[-1]
    outward = across
    # an end is out in the open, not in the recess or facing the tower: it takes the
    # colour of the drum wall it finishes, a little darker, rather than the wall shade
    ec = mean_colour([f for f in faces_of(DRUM) if g in f.verts or d in f.verts])
    end_col = (ec[0] * 0.8, ec[1] * 0.8, ec[2] * 0.8, 1.0)
    # the ground under the inner end, in the same plane
    xg = bm.verts.new((xx.co.x, xx.co.y, g.co.z))
    # the drum's cut end: ground, drum top corner, along the inner edge to the tower
    caps += cap([g, d, fx, xx, xg], outward, end_col)
    # the block above it: recess wall foot, up the recess wall, roof, inner wall
    caps += cap([xx, fx, ft, it], outward, end_col)
report["end_cap_triangles"] = len(caps)
report["winding_fixed"] = R.agree_winding(bm, inner_wall + recess + caps)


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


rms_pipes = []
for g in islands(faces_of(PIPE)):
    new, rms = R.close_convex(bm, g, skip_below_z=0.03)
    for f in new:
        if f.is_valid:
            f[part] = PIPE
    rms_pipes.append(round(rms, 4))
report["pipe_uv_fit_rms"] = rms_pipes


def thicken(faces, depth, fpart, shade=0.6, inward_of=None):
    """Give a one-sided surface a second skin `depth` behind it, reversed, and close the
    open border between the two. With inward_of=(cx, cy) the skin is offset toward that
    vertical axis (a round wall); otherwise along the surface's mean normal (a plate).
    The skin carries its source's UVs; colour is the source's times `shade`. Returns
    (skin faces, border faces)."""
    faces = [f for f in faces if f.is_valid]
    n = Vector()
    for f in faces:
        n += f.normal * f.calc_area()
    n.normalize()
    vmap = {}
    for v in verts_of(faces):
        if inward_of is not None:
            d = Vector((inward_of[0] - v.co.x, inward_of[1] - v.co.y, 0.0)).normalized() * depth
        else:
            d = -n * depth
        vmap[v] = bm.verts.new(v.co + d)
    skin = []
    for f in faces:
        nf = bm.faces.new([vmap[v] for v in reversed(f.verts)])
        nf[part] = fpart
        nf[tex] = f[tex]
        nf.material_index = f.material_index
        for lo, ln in zip(reversed(f.loops), nf.loops):
            ln[uvl].uv = lo[uvl].uv
            c = lo[col]
            ln[col] = (c[0] * shade, c[1] * shade, c[2] * shade, c[3])
        skin.append(nf)
    border = []
    fs = set(faces)
    for e in R.open_edges(bm, faces):
        f = [g for g in e.link_faces if g in fs][0]
        lp = [l for l in e.link_loops if l.face is f][0]
        a, b = lp.vert, lp.link_loop_next.vert
        if inward_of is not None and not (a.co.z > 0.2 and b.co.z > 0.2):
            continue            # a wall's foot stands on the floor: only its rim is closed
        q = bm.faces.new([b, a, vmap[a], vmap[b]])
        q[part] = fpart
        q[tex] = f[tex]
        q.material_index = f.material_index
        uva, uvb = lp[uvl].uv.copy(), lp.link_loop_next[uvl].uv.copy()
        for l2 in q.loops:
            l2[uvl].uv = uva if l2.vert in (a, vmap[a]) else uvb
            l2[col] = tuple(lp[col])
        border.append(q)
    return skin, border


# the tank: one outer skin now; an inner skin 8 model units inside it, and a rim
tank_wall = faces_of(TANK)
skin, rim = thicken(tank_wall, 8.0 / B.CELL, TANK_IN, inward_of=tk_c)
# the floor (cut loose above) moves in to the inner skin's foot, so the final weld
# joins it to the inner skin: floor and inner skin are one surface facing the cavity
for v in verts_of(faces_of(FLOOR)):
    d = Vector((tk_c[0] - v.co.x, tk_c[1] - v.co.y, 0.0)).normalized() * (8.0 / B.CELL)
    v.co += d
R.apply_colour(faces_of(TANK), col, shade_tk_out)
R.apply_colour(skin, col, shade_tk_in)
R.apply_colour(rim, col, shade_tk_out)
report["tank_inner_faces"] = len(skin)
report["tank_rim_faces"] = len(rim)

# the post: a thin back behind the plate
pskin, pborder = thicken(faces_of(POST), 6.0 / B.CELL, POST)
report["post_faces_added"] = len(pskin) + len(pborder)

bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
report["winding_fixed_final"] = R.agree_winding(bm, faces_of(INNER, TANK, TANK_IN, FLOOR, POST))
bm.normal_update()
report["body_triangles"] = len(bm.faces)
report["fixed_triangles"] = len(bm.faces) + sum(len(o.data.polygons) for o in objs[1:])
report["ratio"] = round(report["fixed_triangles"] / report["cartridge_triangles"], 2)
report["open_edges_after"] = sum(1 for e in bm.edges if e.is_boundary)
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
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "NUK2_fixed.blend"))
print("FIX|" + "|".join("%s=%s" % kv for kv in report.items()))
