#!/usr/bin/env python3
"""Pull one cartridge model out of a baked pack into a folder Blender can rebuild it from.

    python3 tools/art/n64fix/extract.py tools/bakery/game/SCG01EA.pack NUKE -o <dir>

Writes <dir>/<TYPE>.json (every triangle: texture, draw mode, wrap, and per corner the
position, uv and baked vertex colour, plus the parts and construction sections) and one
PNG per texture in BOTH house palettes, <TYPE>_tNNN_nod.png and <TYPE>_tNNN_gdi.png.
The two palettes are the only record of which texels are house colour: a texel that
differs between them is paint.

This is the first stage of the fix-and-improve recipe (tools/art/n64fix/README.md). It
reads the pack with the repo's own reader and changes nothing.
"""
import json
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "bakery"))
import packinspect as PI  # noqa: E402

TRI_STRIDE = 4 + 2 + 3 * (5 * 4 + 4)
TRI_FMT = "<i2B" + "5f4B" * 3
MODE = {0: "opaque", 1: "cutout", 2: "shadow", 3: "xlu"}


def png(path, w, h, rgba):
    raw = b"".join(b"\0" + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    a = sys.argv[1:]
    out = "."
    if "-o" in a:
        i = a.index("-o")
        out = a[i + 1]
        del a[i:i + 2]
    if len(a) != 2:
        print(__doc__)
        return 2
    pack = PI.read(a[0])
    code = a[1]
    idx = [m for c, m, f in pack["types"] if c == code]
    if not idx:
        print("no type %s in %s" % (code, a[0]))
        return 1
    mesh = pack["meshes"][idx[0]]
    os.makedirs(out, exist_ok=True)
    tris, used = [], []
    for i in range(mesh["ntris"]):
        f = struct.unpack_from(TRI_FMT, mesh["tris"], i * TRI_STRIDE)
        t, mode, wrap = f[:3]
        corners = []
        for k in range(3):
            v = f[3 + k * 9:12 + k * 9]
            corners.append(dict(p=list(v[0:3]), uv=list(v[3:5]), c=list(v[5:9])))
        tris.append(dict(tex=t, mode=MODE.get(mode, str(mode)), wrap=wrap, v=corners))
        if t >= 0 and t not in used:
            used.append(t)
    texs = {}
    for t in used:
        tx = pack["tex"][t]
        w, h = tx["w"], tx["h"]
        nod = "%s_t%03d_nod.png" % (code, t)
        gdi = "%s_t%03d_gdi.png" % (code, t)
        png(os.path.join(out, nod), w, h, tx["data"])
        png(os.path.join(out, gdi), w, h, tx["gdi"] or tx["data"])
        texs[str(t)] = dict(w=w, h=h, used=[tx["uw"], tx["uh"]], nod=nod, gdi=gdi,
                            house=bool(tx["gdi"]) and tx["gdi"] != tx["data"])
    parts = [dict(tri0=p[0], ntri=p[1], role=p[2], pivot=list(p[3])) for p in mesh["parts"]]
    # PKB node animation: per frame, per part, a row-major 3x4 applied ON TOP of the
    # already-posed triangles (v' = M v + t, pack units), and a visibility byte
    anim = None
    pa = mesh["anim"]
    if pa and pa["frames"]:
        n = pa["frames"] * len(parts) * 12
        anim = dict(frames=pa["frames"], ticks_per_frame=pa["ticks_per_frame"],
                    clips=[dict(c) for c in pa["clips"]],
                    mat=[round(x, 5) for x in struct.unpack("<%df" % n, pa["mat"])],
                    vis=list(pa["vis"]) if pa["vis"] else [1] * (pa["frames"] * len(parts)))
    doc = dict(type=code, pack=os.path.basename(a[0]), cartridge_mesh=mesh["name"],
               axes="x east, y up, z south; 1024 units to the cell",
               parts=parts, sections=list(mesh["sections"] or []), textures=texs, tris=tris,
               anim=anim)
    with open(os.path.join(out, code + ".json"), "w") as f:
        json.dump(doc, f, indent=0)
    print("%s: %d triangles, %d textures (%d carry house colour), %d parts, %d animation frames -> %s"
          % (code, len(tris), len(texs), sum(1 for v in texs.values() if v["house"]),
             len(parts), anim["frames"] if anim else 0, out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
