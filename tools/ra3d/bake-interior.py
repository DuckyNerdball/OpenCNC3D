#!/usr/bin/env python3
"""Red Alert's Interior walls for the game: each indoor mission's walls, built from its own
map by the approved kit (tools/art/remaster/ra/build_INT_WALLS.py) and painted by its
approved paint (paint_INT_WALLS.py), baked for Enhanced.

    python3 tools/ra3d/bake-interior.py [OUT]      default OUT: game/einw_ra

Writes, per mission set indoors (interior.MISSIONS: three on the 1996 discs, four on the
expansion discs), OUT/<SCEN>.einw, and once OUT/walls_atlas.rgbz (the atlas: "RAINTA01",
u32 width and height, then its RGBA rows from the top, zlib-compressed) and OUT/manifest.json
(the sources' hashes and the lab approval the bake was made under); the atlas as a PNG for
looking at goes to build-native/remaster-ra/civil/INT_WALLS/game/. Refuses while the lab's walls
row (civil/INT_WALLS in tools/art/remaster/reviews-ra.json) is not approved, and refuses a
mission whose walls open a seam, but for the one known join SCG21EA carries (KNOWN_OPEN).
Each wall cell's faces take its own of the 1996 walls' four faces (interior.FACES) once the
lab's face variants row (civil/INT_FACES) is approved, and the one face until then.

THE FILE (little-endian):
    char magic[8]  "RAINTW01"
    u32  cells, verts
    cells x { u16 x, u16 y, u32 first, u32 count }   each wall cell's vertices, in map
                                                      order (y, then x)
    verts x { f32 x, y, z, s, t, nx, ny, nz }         a triangle list: x east and y south
                                                      in cells from the 128 x 128 map's
                                                      corner, z up in cells (the floor at
                                                      0), the atlas coordinates (t from
                                                      the image's top), the triangle's
                                                      normal (x east, y NORTH, z up)
A cell is drawn only once the player has revealed it, so the walls come out of the shroud
with the room; the floor and the void are the game's own ground.
"""
import hashlib
import json
import zlib
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RA = os.path.join(ROOT, "tools", "art", "remaster", "ra")
sys.path.insert(0, RA)
import interior as I                         # noqa: E402
import build_INT_WALLS as K                  # noqa: E402

# the engine's map export: the playable rectangle and a ring of one cell round it, which
# the game's ground draws too, so its walls stand there as well
RING = 1
import paint_INT_WALLS as PW                 # noqa: E402

import numpy as np                           # noqa: E402
from PIL import Image                        # noqa: E402

REVIEWS = os.path.join(ROOT, "tools", "art", "remaster", "reviews-ra.json")
# SCG21EA's one open join: a brick wall beside a cut corner's cut side (the brick walls'
# own kit closes it); any other open edge stops the bake
KNOWN_OPEN = {"SCG21EA": 10}


class Stub(object):
    """What the kit needs of a proxy: the plan frame, and somewhere to put its parts."""
    def __init__(self, w, h):
        self.w, self.h = w, h

    def xy(self, u, v):
        return (u - self.w / 2.0, self.h / 2.0 - v)

    def _add(self, *a, **k):
        pass


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def main(out):
    reviews = json.load(open(REVIEWS))
    row = reviews.get("civil/INT_WALLS", {})
    if row.get("state") != "approved" or not row.get("remaster_approved"):
        sys.exit("bake-interior: the lab's walls row is %s, not approved: nothing baked" % row.get("state"))
    os.makedirs(out, exist_ok=True)
    frow = reviews.get("civil/INT_FACES", {})
    faces = frow.get("state") == "approved"
    A = PW.build_atlas([], faces)
    atlas = np.clip(A.image(), 0, 255).astype(np.uint8)
    rgba = np.concatenate([atlas, np.full(atlas.shape[:2] + (1,), 255, np.uint8)], axis=2)
    with open(os.path.join(out, "walls_atlas.rgbz"), "wb") as f:
        f.write(b"RAINTA01" + struct.pack("<II", rgba.shape[1], rgba.shape[0]) + zlib.compress(rgba.tobytes(), 9))
    look = os.path.join(ROOT, "build-native", "remaster-ra", "civil", "INT_WALLS", "game")
    os.makedirs(look, exist_ok=True)
    Image.fromarray(atlas).save(os.path.join(look, "walls_atlas.png"))
    missions = {}
    for scen in I.MISSIONS:
        m = I.mission(scen, ring=RING)
        g = m["grid"]
        kit = K.Kit(Stub(len(g[0]), len(g)), g)
        kit.origin, kit.faces = (m["rect"][0], m["rect"][1]), faces
        kit.build(K.floor_colour)
        bad = len(K.seams(kit))
        if bad != KNOWN_OPEN.get(scen, 0):
            sys.exit("bake-interior: %s opens %d wall edges (%d known): the kit changed, nothing written"
                     % (scen, bad, KNOWN_OPEN.get(scen, 0)))
        cells = K.game_mesh(kit, A, m["rect"])
        keys = sorted(cells, key=lambda c: (c[1], c[0]))
        head, verts, first = [], [], 0
        for c in keys:
            vs = cells[c]
            head.append(struct.pack("<HHII", c[0], c[1], first, len(vs)))
            verts.extend(vs)
            first += len(vs)
        blob = b"RAINTW01" + struct.pack("<II", len(keys), len(verts)) + b"".join(head)
        blob += np.asarray(verts, np.float32).tobytes()
        path = os.path.join(out, scen + ".einw")
        with open(path, "wb") as f:
            f.write(blob)
        missions[scen] = dict(cells=len(keys), triangles=len(verts) // 3, open_edges=bad, bytes=len(blob),
                              sha256=hashlib.sha256(blob).hexdigest(), rect=list(m["rect"]))
        print("EINW|%s|%d wall cells|%d triangles|%d open (known)|%s" % (scen, len(keys), len(verts) // 3, bad, path))
    srcs = {os.path.relpath(p, ROOT): sha(p) for p in (
        os.path.join(RA, "interior.py"), os.path.join(RA, "build_INT_WALLS.py"), os.path.join(RA, "paint_INT_WALLS.py"),
        os.path.join(ROOT, "data", "radata", "INTERIOR.MIX"))}
    man = dict(made_by="tools/ra3d/bake-interior.py", baked=time.strftime("%Y-%m-%d %H:%M"),
               approval=dict(row="civil/INT_WALLS", remaster_approved=row["remaster_approved"],
                             **(dict(faces_row="civil/INT_FACES", faces_approved=frow.get("at")) if faces else {})),
               wall_height=K.H, atlas=dict(file="walls_atlas.rgbz", size=list(atlas.shape[1::-1]),
                                           sha256=sha(os.path.join(out, "walls_atlas.rgbz"))),
               sources=srcs, missions=missions)
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(man, f, indent=1, sort_keys=True)
    print("EINW|wrote|%d missions|%s" % (len(missions), out))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "game", "einw_ra"))
