"""Write a fixed model into the side-by-side viewer, with its thumbnail.
    blender -b <work>/fixed/<TYPE>_fixed.blend --python tools/art/n64fix/export_new.py -- <src_dir> <TYPE>

The fixed .blend's images point at the 4x GDI textures; the Nod ones are found beside
them in <src_dir>/x4 by the extract's own names, so the viewer can switch palettes."""
import json
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402

src, code = sys.argv[sys.argv.index("--") + 1:][:2]
doc = json.load(open(os.path.join(src, code + ".json")))
houses = {}
for t, info in doc["textures"].items():
    houses[info["gdi"]] = {h: os.path.join(src, "x4", info[h]) for h in ("gdi", "nod")}
objs = [o for o in bpy.context.scene.objects if o.type == "MESH" and not o.name.startswith("wire")]
view = os.path.join(os.path.dirname(os.path.abspath(__file__)), "viewer", "models", code)
n = B.export_view_json(objs, view, "new", houses, doc.get("anim"))
B.thumbnail(objs, os.path.join(view, "thumb_new.png"))
print("VIEW new: %d triangles -> %s" % (n, view))
