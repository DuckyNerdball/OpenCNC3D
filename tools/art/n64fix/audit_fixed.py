"""Open-edge loops of a fixed model, and a cyan-backface review sheet.
    blender -b <fixed.blend> --python tools/art/n64fix/audit_fixed.py -- <out_prefix>"""
import os
import sys

import bpy
import bmesh

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402

out = sys.argv[sys.argv.index("--") + 1]
objs = [o for o in bpy.context.scene.objects if o.type == "MESH" and not o.name.startswith("wire")]
body = objs[0]
bm = bmesh.new()
bm.from_mesh(body.data)
part = bm.faces.layers.int.get("fix_part")
seen = set()
for e in bm.edges:
    if not e.is_boundary or e in seen:
        continue
    st, grp = [e], []
    seen.add(e)
    while st:
        x = st.pop()
        grp.append(x)
        for v in x.verts:
            for y in v.link_edges:
                if y.is_boundary and y not in seen:
                    seen.add(y)
                    st.append(y)
    vs = {v for x in grp for v in x.verts}
    parts = sorted({f[part] for x in grp for f in x.link_faces}) if part else []
    lo = [min(v.co[i] for v in vs) for i in range(3)]
    hi = [max(v.co[i] for v in vs) for i in range(3)]
    print("LOOP %3d edges parts %s x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f" %
          (len(grp), parts, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
bad = []
for e in bm.edges:
    if len(e.link_faces) != 2:
        continue
    f0, f1 = e.link_faces
    d0 = [l for l in e.link_loops if l.face is f0][0].vert
    d1 = [l for l in e.link_loops if l.face is f1][0].vert
    if d0 is d1:
        bad.append((f0, f1))
for f0, f1 in bad:
    c = (f0.calc_center_median() + f1.calc_center_median()) / 2
    print("FLIP parts %d/%d at %.2f %.2f %.2f" % (f0[part], f1[part], c.x, c.y, c.z))
print("AUDIT inconsistent-winding edges: %d" % len(bad))
bm.free()
B.mark_backfaces()
B.add_wire_overlay(objs)
B.review_renders(objs, out, size=640)
