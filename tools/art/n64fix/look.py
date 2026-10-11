"""Render the untouched cartridge model from eight bearings, for a first look.
    blender -b --python tools/art/n64fix/look.py -- <src_dir> <TYPE> <out_prefix>"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402

src, code, out = sys.argv[sys.argv.index("--") + 1:][:3]
B.reset_scene()
doc, objs = B.build_cartridge(src, code)
print("built", [(o.name, len(o.data.polygons)) for o in objs])
B.review_renders(objs, out)
