"""Write the untouched cartridge model for the side-by-side viewer.
    blender -b --python tools/art/n64fix/export_old.py -- <src_dir> <TYPE> <viewer_model_dir>"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402

src, code, out = sys.argv[sys.argv.index("--") + 1:][:3]
B.reset_scene()
doc, objs = B.build_cartridge(src, code)
houses = {}
for t, info in doc["textures"].items():
    houses[info["gdi"]] = {"gdi": os.path.join(src, info["gdi"]), "nod": os.path.join(src, info["nod"])}
n = B.export_view_json(objs, out, "old", houses, doc.get("anim"))
B.thumbnail(objs, os.path.join(out, "thumb_old.png"))
print("VIEW old: %d triangles -> %s" % (n, out))
