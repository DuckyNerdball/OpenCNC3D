"""List the connected pieces of a cartridge model after welding, with their texture and
extent, and render each piece in its own colour so they can be named.
    blender -b --python tools/art/n64fix/islands.py -- <src_dir> <TYPE> <out_prefix>"""
import colorsys
import os
import sys

import bpy
import bmesh

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402

src, code, out = sys.argv[sys.argv.index("--") + 1:][:3]
B.reset_scene()
doc, objs = B.build_cartridge(src, code)
ob = objs[0]
bm = bmesh.new()
bm.from_mesh(ob.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2.0 / B.CELL)
tex = bm.faces.layers.int["pack_tex"]
isl = {}
seen = set()
k = 0
for f in bm.faces:
    if f in seen:
        continue
    st = [f]
    seen.add(f)
    grp = []
    while st:
        x = st.pop()
        grp.append(x)
        for e in x.edges:
            for y in e.link_faces:
                if y not in seen:
                    seen.add(y)
                    st.append(y)
    for x in grp:
        isl[x.index] = k
    vs = {v for x in grp for v in x.verts}
    lo = [min(v.co[i] for v in vs) for i in range(3)]
    hi = [max(v.co[i] for v in vs) for i in range(3)]
    ts = sorted({x[tex] for x in grp})
    ob_open = sum(1 for e in {e for x in grp for e in x.edges} if e.is_boundary)
    print("ISLAND %2d faces %3d open %2d tex %s  x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f" %
          (k, len(grp), ob_open, ts, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
    k += 1
bm.to_mesh(ob.data)
bm.free()
me = ob.data
ca = me.color_attributes["Col"]
n = max(k, 1)
for p in me.polygons:
    h = (isl[p.index] * 0.61803) % 1.0
    c = colorsys.hsv_to_rgb(h, 0.8, 1.0)
    for li in p.loop_indices:
        ca.data[li].color = (*c, 1.0)
for m in me.materials:
    tx = [x for x in m.node_tree.nodes if x.type == "TEX_IMAGE"]
    for x in tx:
        m.node_tree.nodes.remove(x)
    mul = next(x for x in m.node_tree.nodes if x.type == "MIX")
    mul.inputs[7].default_value = (1, 1, 1, 1)
B.review_renders(objs, out)
