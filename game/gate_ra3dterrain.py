#!/usr/bin/env python3
"""G310's helper: Red Alert's ground in the 3D view under Enhanced (game/ra3d_terrain.h).

    gate_ra3dterrain.py seams <terrainenh_RA_TH.pack> <out.pack>
    gate_ra3dterrain.py check <dir> <terrainenh_RA_SNOW.pack> <terrainseam_RA_SNOW.pack>

`seams` writes a join-blend pack of solid-colour strips (a cliff's magenta for names
starting WC, a feature's cyan for the rest, CLEAR1 none), six texels deep, for every key
of the terrain pack: whatever the shipped strips are, the gate then has strips it knows
are there, to prove the drawing as well as the loading.

`check` reads the runs' logs and pictures in <dir> and prints one line, "1 <claims>" or
"0 <what failed>". The runs and what each one is are in game/gates.sh, G310.
"""
import re
import struct
import sys
from pathlib import Path

from PIL import Image, ImageChops

MAP_BOX = (0, 90, 1030, 720)        # the 3D view left of the panel, below its top strip
PANEL_BOX = (1045, 90, 1280, 650)   # the panel itself


def pack_count(path, magic, count_at):
    d = Path(path).read_bytes()[:count_at + 4]
    if d[:8] != magic:
        return None
    return struct.unpack_from("<I", d, count_at)[0]


def seams(enh, out):
    d = Path(enh).read_bytes()
    assert d[:8] == b"C3DRA144", "not a Red Alert terrain pack"
    n = struct.unpack_from("<I", d, 16)[0]
    cell, margin = 144, 6
    keys = []
    for i in range(n):
        e = d[20 + i * 20: 40 + i * 20]
        name = e[:16].rstrip(b"\0").decode()
        if name == "CLEAR1":
            continue
        icon, var, size = struct.unpack_from("<HBB", e, 16)
        kind = 0 if name.startswith("WC") else 1
        for side in (0, 1):
            keys.append((e[:16], icon, side, kind, var, size))

    def strip(side, kind):
        col = bytes((255, 0, 255) if kind == 0 else (0, 255, 255)) + b"\xff"
        return col * (margin * cell)

    body = {(s, k): strip(s, k) for s in (0, 1) for k in (0, 1)}
    with open(out, "wb") as f:
        f.write(b"C3DRASEM" + struct.pack("<IIII", 1, cell, margin, len(keys)))
        for name, icon, side, kind, var, size in keys:
            f.write(name + struct.pack("<HBBBBH", icon, side, kind, var, size, 0))
        for name, icon, side, kind, var, size in keys:
            f.write(body[(side, kind)])


def log(d, name):
    p = Path(d) / name
    return p.read_text(errors="replace") if p.exists() else ""


def img(d, name):
    return Image.open(Path(d) / name).convert("RGB")


def differ(a, b, box):
    diff = ImageChops.difference(a.crop(box), b.crop(box)).convert("L")
    return sum(1 for v in diff.getdata() if v > 8)


def check(d, enh, seam):
    fails, claims = [], []
    P = pack_count(enh, b"C3DRA144", 16)
    SEAMS = pack_count(seam, b"C3DRASEM", 20)
    a = log(d, "a.log")
    # (1) the sheet, from the shipped pack, for the mission's theater
    m = re.search(r"RA3D\|enh\|sheet\|SCG01EA\|SNOW\|enhanced\|\d+x\d+\|cell=144\|slots=(\d+)\|"
                  r"pack=(\d+)\|variants=\d+\|tiles=(\d+)\|missing=(\d+)", a)
    if not m:
        fails.append("no Enhanced SNOW sheet line for SCG01EA")
    elif P is None or int(m.group(2)) != P or int(m.group(4)) != 0:
        fails.append(f"the sheet took {m.group(2)} cells from a pack of {P}, {m.group(4)} missing")
    else:
        claims.append(f"SNOW sheet: all {P} cells of the pack, {m.group(3)} from the 1995 tiles, none missing")
    if "RA3D|scene|ground=red alert|theater=SNOW|rect=48,44 32x38|set=enhanced" not in a:
        fails.append("the scene line does not name Red Alert's SNOW ground on SCG01EA's rectangle")
    cells = [int(c) for c in re.findall(r"RA3D\|frame\|SCG01EA\|cells=(\d+)\|", a)]
    if not cells or max(cells) < 100:
        fails.append("the frame laid fewer than 100 Red Alert cells")
    sm = re.search(r"RA3D\|seams\|[^|]*terrainseam_RA_SNOW\.pack\|(\d+) left\|(\d+) top\|", a)
    if not sm or SEAMS is None or int(sm.group(1)) + int(sm.group(2)) != SEAMS:
        fails.append(f"the shipped seam pack's {SEAMS} strips were not all loaded")
    else:
        claims.append(f"its {SEAMS} shipped strips loaded")
    # (2) never the placeholder: no Tiberian Dawn sheet or sea, and the picture is snow
    # A sea field is Red Alert's own only when water_mod.h says it built it for Red Alert's
    # map (WATER|ra|field, one per field); any other is the placeholder's.
    if "enhanced terrain:" in a or "terrain seams:" in a \
            or a.count("WATER|field|") != a.count("WATER|ra|field|"):
        fails.append("the Tiberian Dawn placeholder's terrain or sea was built")
    A = img(d, "a.png")
    px = list(A.crop(MAP_BOX).getdata())
    snow = sum(1 for r, g, b in px if min(r, g, b) > 120 and max(r, g, b) - min(r, g, b) < 60) / len(px)
    green = sum(1 for r, g, b in px if g > r + 12 and g > b + 12) / len(px)
    if snow < 0.35 or green > 0.25:
        fails.append(f"the frame is not snow ground ({snow:.0%} snow-white, {green:.0%} green)")
    else:
        claims.append(f"the frame is snow ({snow:.0%} snow-white, {green:.1%} green)")
    if differ(A, img(d, "a2.png"), (0, 0, 1280, 720)):
        fails.append("two Enhanced runs drew different pictures")
    # (3) the strips drawn, by the shared code, and switched off
    b = log(d, "b.log")
    strips = [int(c) for c in re.findall(r"RA3D\|frame\|SCG01EA\|cells=\d+\|strips=(\d+)", b)]
    B, C, D = img(d, "b.png"), img(d, "c.png"), img(d, "d.png")
    nb = differ(B, D, MAP_BOX)
    if not strips or max(strips) == 0 or nb < 2000:
        fails.append(f"the gate's strips were not drawn ({max(strips) if strips else '?'} strips, {nb} pixels)")
    else:
        claims.append(f"{max(strips)} of the gate's strips drawn ({nb} pixels)")
    if differ(B, D, PANEL_BOX):
        fails.append("the strips reached the panel")
    if differ(C, D, (0, 0, 1280, 720)) or "RA3D|seams|off" not in log(d, "c.log"):
        fails.append("OPENCNC3D_TERRAINSEAM=off did not take the strips off")
    # (3b) what stands on the ground is still drawn, the same with either set under it
    def drawn(text):
        m = re.findall(r"draw: (\d+) meshes, (\d+) sprites, (\d+) fallback boxes", text)
        return tuple(int(x) for x in m[-1]) if m else None
    da, dd = drawn(a), drawn(log(d, "a1995.log"))
    if not da or sum(da) == 0 or da != dd:
        fails.append(f"the objects on the ground were not drawn alike over both sets ({da} against {dd})")
    else:
        claims.append(f"the objects still drawn on it ({da[0]} meshes, {da[1]} sprites, {da[2]} boxes, the same over the 1995 set)")
    if "RA3D|scene|ground=red alert|theater=SNOW|rect=48,44 32x38|set=1995" not in log(d, "a1995.log"):
        fails.append("texset 1 did not draw the 1995 tiles")
    # (4) the temperate theater, and interior from its 1995 tiles
    if "RA3D|scene|ground=red alert|theater=INTERIOR|" not in log(d, "h.log") or \
            "set=1995" not in log(d, "h.log"):
        fails.append("SCG13EA did not draw its interior from the 1995 tiles")
    else:
        claims.append("SCG13EA's interior from its 1995 tiles")
    if "RA3D|scene|ground=red alert|theater=TEMPERAT|rect=11,47 62x66|set=enhanced" not in log(d, "e.log"):
        fails.append("SCG05EA did not draw Red Alert's TEMPERAT ground")
    else:
        claims.append("SCG05EA on TEMPERAT")
    # (5) Classic untouched: it reads none of this, and draws the same either way
    f = log(d, "f.log")
    if "RA3D|enh" in f or "RA3D|scene" in f:
        fails.append("Classic built or drew the 3D ground")
    if differ(img(d, "f.png"), img(d, "f2.png"), (0, 0, 1280, 720)):
        fails.append("Classic's picture changed with the Enhanced packs off")
    else:
        claims.append("Classic reads none of it and draws the same with the packs off")
    # (6) the Visuals dialog reaches it
    if "RA3D|scene|ground=red alert|theater=SNOW" not in log(d, "g.log"):
        fails.append("choosing Enhanced in the Visuals dialog did not reach the 3D ground")
    else:
        claims.append("reached from the Visuals dialog")
    if fails:
        print("0 " + "; ".join(fails))
    else:
        print("1 " + "; ".join(claims))


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "seams":
        seams(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 5 and sys.argv[1] == "check":
        check(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        print(__doc__)
        sys.exit(2)
