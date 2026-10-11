#!/usr/bin/env python3
"""
OpenCNC3D RA3D -- the auto heightmap for a Red Alert mission.

    python3 tools/ra3d/ra_heights.py SCG01EA OUT.HGT [--noswells]

Runs the auto heightmap generator (game/autoheight_mod.h, the C++ port of
tools/autoheight/generate.py that the map editor uses, built here through
tools/autoheight/port_check.cpp) on the mission's own 128 x 128 terrain, playable
rectangle and pre-placed buildings, and writes the 129 x 129 corner heights bake5.py
takes as --heights.

The generator learned its rules from the 55 hand-made heightmaps of the N64 cartridge, so
it reads Tiberian Dawn template numbers. Each Red Alert cell is handed to it as:

  - the Tiberian Dawn icon whose 1995 picture it is (bake_ra_terrain3d.picture_match, the
    same match Enhanced's paintings are carried by);
  - else its Tiberian Dawn namesake, where both games have the template, the icon exists
    in both and the two templates are the same picture give or take a palette (median
    icon difference under NAMESAKE_CLOSE): the cliffs (S01..S38), roads, rivers, fords
    and falls. Names alone are not trusted: Red Alert's shores SH01..SH56 are different
    pieces from Tiberian Dawn's SH1..SH63, and read as one another they put a learned
    shore slope where there is none;
  - otherwise a stand-in chosen by the land type RED ALERT ITSELF gives that icon
    (TemplateTypeClass::Land_Type: the template file's control map through the same
    sixteen-entry table, cdata.cpp): water as open water (W1), rock as a rocky shore (SH3,
    Tiberian Dawn's one-cell rock-and-water piece), anything else as plain ground. That is
    how the shore cliffs (WC01..WC38) and Red Alert's own SH31 are read: water where Red
    Alert says water, rock where it says rock.

Buildings stand on level ground: their footprints come from Red Alert's own occupy lists
(bdata.cpp, List22 and the rest, MAP_CELL_W = 128).

INTERIOR MISSIONS ARE FLAT. They are floors inside buildings; the generator learned
outdoor ground from the cartridge, and its swells would roll a hill across a room. A
mission in the interior theater gets an all-zero heightmap and says so.
"""
import os, re, struct, subprocess, sys, tempfile, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "autoheight"))
import ra_map                      # noqa: E402
import bake_ra_terrain3d as B      # noqa: E402
from ramix import RaMix            # noqa: E402
import corpus as TD                # noqa: E402

BDATA = os.path.join(ROOT, "brain", "vanilla", "redalert", "bdata.cpp")
W = H = ra_map.W
# TemplateTypeClass::Land_Type's table, by control code
RA_LAND = ["CLEAR"] * 6 + ["BEACH", "CLEAR", "ROCK", "ROAD", "WATER", "RIVER", "CLEAR", "CLEAR", "ROUGH", "CLEAR"]
NAMESAKE_CLOSE = 15.0


def ra_land_codes(bank):
    """{(Red Alert template, icon): land name} from each template's control map."""
    mixname, ext = B.THEATERS[bank][:2]
    mix = RaMix(os.path.join(B.DATA, mixname))
    out = {}
    for tid, name in B.bank_templates(bank):
        raw = mix.read(name + "." + ext)
        if raw is None:
            continue
        f = struct.unpack_from("<6h7i", raw)
        mw, mh, color_map = f[4], f[5], f[11]
        n = max(1, mw * mh)
        for icon in range(f[2]):
            code = raw[color_map + icon % n] if color_map + icon % n < len(raw) else 0
            out[(tid, icon)] = RA_LAND[code & 15]
    return out


def footprints():
    """{building INI name: [(dx, dy)]} from bdata.cpp's occupy lists."""
    src = re.sub(r"//[^\n]*", "", open(BDATA, "rb").read().decode("cp437"))
    lists = {}
    for m in re.finditer(r"static\s+short\s+const\s+(\w+)\s*\[\]\s*=\s*\{([^}]*)\}", src):
        offs = []
        for e in m.group(2).split(","):
            e = e.split("//")[0].strip()
            if not e or "REFRESH_EOL" in e:
                break
            try:
                v = eval(e.replace("MAP_CELL_W", "128").replace("MCW", "128"), {"__builtins__": {}})
            except Exception:
                continue
            offs.append(((v + 64 * 128) % 128 if v < 0 else v % 128, v // 128))
        lists[m.group(1)] = offs
    out = {}
    for m in re.finditer(r'BuildingTypeClass\s+const\s+\w+\s*\(\s*STRUCT_\w+\s*,[^"]*"(\w+)".*?BSIZE_\w+\s*,[^,]*,\s*\(short\s+const\s*\*\)\s*(\w+)', src, re.S):
        out.setdefault(m.group(1).upper(), lists.get(m.group(2), [(0, 0)]))
    return out


def close_namesakes(bank, td):
    """{Red Alert template: Tiberian Dawn template} for namesakes that are the same picture
    give or take a palette: median icon difference under NAMESAKE_CLOSE."""
    import numpy as np
    from PIL import Image
    _m, _e, palname, _f, td_bank, td_atlas = B.THEATERS[bank]
    slot = {(t, i): s for t, i, s in B.td_slots(td_bank)}
    atl = np.asarray(Image.open(os.path.join(B.TERRAIN, td_atlas)).convert("RGB"), np.float32)
    pal = B.palette(palname).astype(np.float32)
    names = dict(B.bank_templates(bank))
    diffs = {}
    for t, i, px in B.bank_slots(bank):
        nm = names.get(t, "")
        k = td.get(nm)
        if k is None and len(nm) > 2 and nm[-2] == "0" and nm[:-2].isalpha():
            k = td.get(nm[:-2] + nm[-1])
        if k is None or t == 0:
            continue
        s = slot.get((k, i))
        if s is None:
            diffs.setdefault(t, (k, []))[1].append(999.0)
            continue
        y0, x0 = (s // B.COLS) * B.PITCH + B.GUTTER, (s % B.COLS) * B.PITCH + B.GUTTER
        ra = pal[np.frombuffer(px, np.uint8)].reshape(B.TS, B.TS, 3)
        diffs.setdefault(t, (k, []))[1].append(float(np.abs(atl[y0:y0 + B.TS, x0:x0 + B.TS] - ra).mean()))
    return {t: k for t, (k, d) in diffs.items() if sorted(d)[len(d) // 2] < NAMESAKE_CLOSE}


def generator_input(scen, swells=True):
    r = ra_map.translate(scen)
    bank = B.BANK_OF[r["theater"]]
    td = {t[0]: i for i, t in enumerate(TD.TT)}
    land = ra_land_codes(bank)
    pic = B.picture_match(bank)
    same = close_namesakes(bank, td)
    tm, ic = bytearray(W * H), bytearray(W * H)
    stand = {"picture": 0, "namesake": 0, "water": 0, "rock": 0, "ground": 0}
    for c in range(W * H):
        t, i = r["ra_t"][c], r["ra_i"][c]
        if t in (0xFFFF, 0xFF) or t >= len(r["enum"]):
            tm[c], ic[c] = 255, 0
            continue
        k = same.get(t)
        if t == 0:
            tm[c], ic[c] = 255, 0
        elif (t, i) in pic:
            tm[c], ic[c] = pic[(t, i)]
            if tm[c] == 0:                     # Tiberian Dawn's CLEAR1 is plain ground too
                tm[c], ic[c] = 255, 0
            stand["picture"] += 1
        elif k is not None and i < TD.TT[k][4] * TD.TT[k][5]:
            tm[c], ic[c] = k, i
            stand["namesake"] += 1
        else:
            l = land.get((t, i), "CLEAR")
            if l in ("WATER", "RIVER"):
                tm[c], ic[c] = td["W1"], 0; stand["water"] += 1
            elif l == "ROCK":
                tm[c], ic[c] = td["SH3"], 0; stand["rock"] += 1
            else:
                tm[c], ic[c] = 255, 0; stand["ground"] += 1
    fp = footprints()
    foot = []
    for k, v in r["ini"].get("STRUCTURES", []):
        f = v.split(",")
        if len(f) >= 4 and f[3].strip().isdigit():
            cell = int(f[3]); cx, cy = cell % W, cell // W
            foot.append([(cx + dx, cy + dy) for dx, dy in fp.get(f[1].strip().upper(), [(0, 0)])])
    X, Y, w, h = r["rect"]
    p = struct.pack("<8i", W, H, X, Y, w, h, zlib.crc32(scen.encode()) & 0x7fffffff, int(swells))
    p += bytes(tm) + bytes(ic)
    p += struct.pack("<i", len(foot))
    for fc in foot:
        p += struct.pack("<i", len(fc)) + b"".join(struct.pack("<2i", x, y) for x, y in fc)
    p += struct.pack("<i", 0)
    return p, stand, len(foot), r


def flatten_water(corners, payload):
    """OPEN WATER IS ONE LEVEL. The generator gives every icon of a water template its own
    learned offset, which on the cartridge's maps is the floor under a separate sea
    surface; Red Alert's water is drawn as the ground itself, so those offsets would show
    as a chequered sea. Every corner with water on all four sides takes the lowest such
    corner of its connected body; a corner touching land keeps its slope down to it."""
    tm = payload[32:32 + W * H]
    water = [tm[c] in (1, 2) for c in range(W * H)]       # W1, W2 (and the water stand-in)
    CW = W + 1
    wet = [False] * (CW * (H + 1))
    for gy in range(H + 1):
        for gx in range(W + 1):
            cs = [(min(max(gy + dy, 0), H - 1)) * W + min(max(gx + dx, 0), W - 1)
                  for dy in (-1, 0) for dx in (-1, 0)]
            wet[gy * CW + gx] = all(water[c] for c in cs)
    seen = [False] * len(wet)
    for start in range(len(wet)):
        if not wet[start] or seen[start]:
            continue
        body, stack = [], [start]
        seen[start] = True
        while stack:
            i = stack.pop(); body.append(i)
            x, y = i % CW, i // CW
            for j in ((i - 1) if x > 0 else -1, (i + 1) if x < W else -1,
                      (i - CW) if y > 0 else -1, (i + CW) if y < H else -1):
                if j >= 0 and wet[j] and not seen[j]:
                    seen[j] = True; stack.append(j)
        lo = min(corners[i] for i in body)
        for i in body:
            corners[i] = lo
    return corners


def build_generator():
    """The generator, compiled once per source: missions run in parallel share it."""
    src = os.path.join(ROOT, "tools", "autoheight", "port_check.cpp")
    deps = [src, os.path.join(ROOT, "game", "autoheight_mod.h")]
    stamp = zlib.crc32(b"".join(open(f, "rb").read() for f in deps if os.path.exists(f)))
    exe = os.path.join(tempfile.gettempdir(), "ra_autoheight_%08x" % stamp)
    if not os.path.exists(exe):
        tmp = exe + ".%d" % os.getpid()
        subprocess.run(["c++", "-std=c++17", "-O2", "-o", tmp, src], check=True)
        os.replace(tmp, exe)
    return exe


def main():
    args = sys.argv[1:]
    swells = "--noswells" not in args
    args = [a for a in args if a != "--noswells"]
    scen, out = args[0], args[1]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    if B.BANK_OF[ra_map.translate(scen)["theater"]] == "RAINT":
        open(out, "wb").write(bytes((W + 1) * (H + 1)))
        print(f"{scen}: interior, a floor: flat heightmap -> {out}")
        return
    payload, stand, nfoot, r = generator_input(scen, swells)
    exe = build_generator()
    with tempfile.TemporaryDirectory() as d:
        a, b = os.path.join(d, "in.bin"), os.path.join(d, "out.bin")
        open(a, "wb").write(payload)
        subprocess.run([exe, a, b], check=True)
        raw = open(b, "rb").read()
    n = (W + 1) * (H + 1)
    corners = raw[:n]
    places, outvoted, clamped, orphans = struct.unpack("<4i", raw[n + W * H:n + W * H + 16])
    corners = flatten_water(bytearray(corners), payload)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    open(out, "wb").write(bytes(corners))
    X, Y, w, h = r["rect"]
    inner = [corners[y * (W + 1) + x] for y in range(Y, Y + h + 1) for x in range(X, X + w + 1)]
    print(f"{scen}: cells read as {stand}; {nfoot} buildings levelled; {places} cliff placements, "
          f"{outvoted} out-voted, {clamped} clamped, {orphans} orphan cells; playable heights "
          f"{min(inner)}..{max(inner)} -> {out} ({len(corners)} corners)")


if __name__ == "__main__":
    main()
