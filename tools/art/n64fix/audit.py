"""Measure a cartridge model's holes: weld, count open edges, group them into loops,
and render with back faces culled so a missing face shows as background.
    blender -b --python tools/art/n64fix/audit.py -- <src_dir> <TYPE> <out_prefix>"""
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
open_e = [e for e in bm.edges if e.is_boundary]
nonman = [e for e in bm.edges if len(e.link_faces) > 2]
print("AUDIT verts %d faces %d open edges %d nonmanifold %d" %
      (len(bm.verts), len(bm.faces), len(open_e), len(nonman)))
# group open edges into connected loops
seen = set()
loops = []
for e in open_e:
    if e in seen:
        continue
    stack, grp = [e], []
    seen.add(e)
    while stack:
        x = stack.pop()
        grp.append(x)
        for v in x.verts:
            for y in v.link_edges:
                if y.is_boundary and y not in seen:
                    seen.add(y)
                    stack.append(y)
    vs = {v for x in grp for v in x.verts}
    lo = [min(v.co[i] for v in vs) for i in range(3)]
    hi = [max(v.co[i] for v in vs) for i in range(3)]
    loops.append((len(grp), lo, hi))
for n, lo, hi in sorted(loops, key=lambda l: -l[0]):
    print("LOOP %3d edges  x %.2f..%.2f  y %.2f..%.2f  z %.2f..%.2f" %
          (n, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
bm.free()
B.mark_backfaces()
B.review_renders(objs, out)
