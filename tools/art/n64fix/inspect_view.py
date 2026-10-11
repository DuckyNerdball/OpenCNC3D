"""One close review render: back faces cyan, edges drawn in black.
    blender -b --python tools/art/n64fix/inspect_view.py -- <src_dir> <TYPE> <out.png> <azimuth> <elevation> [size]"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blend_common as B  # noqa: E402

a = sys.argv[sys.argv.index("--") + 1:]
src, code, out, az, el = a[0], a[1], a[2], float(a[3]), float(a[4])
size = int(a[5]) if len(a) > 5 else 900
B.reset_scene()
doc, objs = B.build_cartridge(src, code)
B.mark_backfaces()
B.add_wire_overlay(objs)
B.review_renders(objs, out[:-4], size=size, views=1, elev=el, az0=az)
os.replace(out[:-4] + "_0.png", out)
