#!/usr/bin/env python3
"""
OpenCNC3D RA3D -- a Red Alert mission's terrain, read from the 1996 disc, for the 3D pipeline.

    python3 tools/ra3d/ra_map.py SCG01EA [--bin OUT.BIN] [--ini OUT.INI] [--as RAG01EA]

Reads the mission INI out of data/radata/GENERAL.MIX and decodes its [MapPack] exactly as
MapClass::Read_Binary does (map.cpp): the section's lines joined and base64-decoded, then
LCW blocks each behind a {u16 compressed, u16 uncompressed} header (lcwstraw.cpp), giving
128 x 128 u16 template numbers and then 128 x 128 u8 icons (NewINIFormat 3).

Every Red Alert template is named by its own TemplateTypeClass in cdata.cpp ("D01",
"S01", "CLEAR1" ...), the same INI names Tiberian Dawn uses. The 3D pipeline draws a snow
map on Tiberian Dawn's winter template set (Red Alert's snow art, re-indexed by name:
tools/snow_to_n64bank.py), so each cell is translated by NAME into Tiberian Dawn's
template number. A template with no Tiberian Dawn namesake, or an icon past the size of
its namesake, is reported and drawn as clear ground; nothing is substituted silently.

--bin writes the translated grid as a dense 128 x 128 Tiberian Dawn .BIN (32768 bytes,
{u8 template, u8 icon} per cell), the form tools/bin_to_n64map.py converts; --ini
writes the scenario INI that pipeline reads its theater and rectangle from.
"""
import base64, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "menu", "tools"))
sys.path.insert(0, os.path.join(ROOT, "tools", "autoheight"))
import ramix                       # noqa: E402
from mixshp import lcw_uncompress  # noqa: E402

RADEF = os.path.join(ROOT, "brain", "vanilla", "redalert", "defines.h")
RACDATA = os.path.join(ROOT, "brain", "vanilla", "redalert", "cdata.cpp")
W = H = 128


def ra_template_enum():
    """[enum name] in TemplateType order (sequential from TEMPLATE_CLEAR1 = 0)."""
    src = open(RADEF, "rb").read().decode("cp437")
    a = src.index("typedef enum TemplateType")
    body = src[a:src.index("}", a)]
    names = []
    for line in body.split("\n"):
        line = line.split("//")[0].strip().rstrip(",")
        m = re.match(r"TEMPLATE_(\w+)$", line)
        if m and m.group(1) not in ("COUNT", "NONE", "FIRST"):
            names.append(m.group(1))
    return names


def ra_template_ini():
    """{enum name: (ini name, snow theater?)} from cdata.cpp's TemplateTypeClass list."""
    return {k: (v[0], "SNOW" in v[1]) for k, v in ra_template_theaters().items()}


def ra_template_theaters():
    """{enum name: (ini name, {"SNOW", "TEMPERATE", "INTERIOR"} it is drawn in)}."""
    src = open(RACDATA, "rb").read().decode("cp437")
    out = {}
    for m in re.finditer(r'TemplateTypeClass\s+const\s+\w+\(\s*TEMPLATE_(\w+)\s*,\s*([^,]+?)\s*,\s*"([^"]+)"', src):
        th = {t for t in ("SNOW", "TEMPERATE", "INTERIOR") if "THEATERF_" + t in m.group(2)}
        out[m.group(1)] = (m.group(3), th)
    return out


def mission_ini(scen):
    mix = ramix.Mix(os.path.join(ROOT, "data", "radata", "GENERAL.MIX")) if hasattr(ramix, "Mix") else None
    if mix is not None:
        return mix.read(scen + ".INI").decode("cp437", "replace")
    import subprocess, tempfile
    d = tempfile.mkdtemp()
    subprocess.run([sys.executable, os.path.join(HERE, "ramix.py"), "extract",
                    os.path.join(ROOT, "data", "radata", "GENERAL.MIX"), d, scen + ".INI"],
                   check=True, stdout=subprocess.DEVNULL)
    return open(os.path.join(d, scen + ".INI"), "rb").read().decode("cp437", "replace")


def ini_sections(text):
    sec, out = None, {}
    for line in text.split("\n"):
        s = line.strip()
        if s.startswith("[") and "]" in s:
            sec = s[1:s.index("]")]
            out.setdefault(sec, [])
        elif "=" in s and sec is not None and not s.startswith(";"):
            k, v = s.split("=", 1)
            out[sec].append((k.strip(), v.strip()))
    return out


def decode_mappack(sec):
    blob = base64.b64decode("".join(v for k, v in sorted(sec, key=lambda kv: int(kv[0]))))
    out, p = bytearray(), 0
    while p + 4 <= len(blob):
        comp, uncomp = struct.unpack_from("<HH", blob, p)
        p += 4
        data, _ = lcw_uncompress(blob, p, uncomp)
        out += bytes(data[:uncomp])
        p += comp
    need = W * H * 3
    if len(out) < need:
        raise SystemExit("ra_map: MapPack decoded to %d bytes, want %d" % (len(out), need))
    tmpl = struct.unpack_from("<%dH" % (W * H), out, 0)
    icon = out[W * H * 2:W * H * 3]
    return list(tmpl), list(icon)


def translate(scen):
    """-> dict with the Red Alert grid, the Tiberian Dawn grid and the report."""
    import corpus as TD                      # tools/autoheight: Tiberian Dawn's template table
    ini = ini_sections(mission_ini(scen))
    mp = dict(ini.get("Map", []))
    rect = tuple(int(mp.get(k, d)) for k, d in (("X", 0), ("Y", 0), ("Width", W), ("Height", H)))
    enum = ra_template_enum()
    named = ra_template_ini()
    td_id = {t[0]: i for i, t in enumerate(TD.TT)}
    ra_t, ra_i = decode_mappack(ini["MapPack"])
    td_t, td_i = [255] * (W * H), [0] * (W * H)
    missing = {}
    for c in range(W * H):
        t, i = ra_t[c], ra_i[c]
        if t == 0xFFFF or t == 0xFF or t >= len(enum):
            continue                                       # TEMPLATE_NONE: plain ground
        nm, snow = named.get(enum[t], (enum[t], False))
        k = td_id.get(nm)
        if t == 0:                                         # CLEAR1 keeps its own icon
            td_t[c], td_i[c] = 0, i
            continue
        if k is None or i >= TD.TT[k][4] * TD.TT[k][5]:
            missing.setdefault(nm, []).append(c)
            continue
        td_t[c], td_i[c] = k, i
    return dict(scen=scen, theater=mp.get("Theater", "SNOW").upper(), rect=rect, ini=ini,
                ra_t=ra_t, ra_i=ra_i, td_t=td_t, td_i=td_i, missing=missing, enum=enum, named=named)


def main():
    args = sys.argv[1:]
    def opt(name):
        if name in args:
            k = args.index(name); v = args[k + 1]; del args[k:k + 2]; return v
        return None
    bin_out, ini_out, alias = opt("--bin"), opt("--ini"), opt("--as")
    scen = args[0] if args else "SCG01EA"
    r = translate(scen)
    X, Y, w, h = r["rect"]
    inside = lambda c: X <= c % W < X + w and Y <= c // W < Y + h
    used = {}
    for c in range(W * H):
        t = r["ra_t"][c]
        if t in (0xFFFF, 0xFF) or t >= len(r["enum"]):
            continue
        used[r["named"].get(r["enum"][t], (r["enum"][t],))[0]] = used.get(r["named"].get(r["enum"][t], (r["enum"][t],))[0], 0) + inside(c)
    print(f"{scen}: theater {r['theater']}, playable {X},{Y} {w}x{h}, "
          f"{sum(1 for c in range(W * H) if r['td_t'][c] != 255)} templated cells, "
          f"{len(used)} templates")
    lost = {k: sum(inside(c) for c in v) for k, v in r["missing"].items()}
    if lost:
        print("  no Tiberian Dawn namesake (drawn as clear ground):")
        for k, v in sorted(lost.items(), key=lambda kv: -kv[1]):
            print(f"    {k:10s} {len(r['missing'][k]):4d} cells, {v:4d} inside the playable rectangle")
    else:
        print("  every template has a Tiberian Dawn namesake")
    if bin_out:
        open(bin_out, "wb").write(bytes(b for c in range(W * H) for b in (r["td_t"][c], r["td_i"][c])))
        print("  wrote", bin_out)
    if ini_out:
        name = alias or scen
        lines = ["; " + name + ": Red Alert " + scen + "'s terrain, translated by tools/ra3d/ra_map.py",
                 "[Basic]", "Name=" + scen, "",
                 "[MAP]", "Theater=WINTER", "OpenCNC3DTheater=" + r["theater"],
                 "X=%d" % X, "Y=%d" % Y, "Width=%d" % w, "Height=%d" % h, ""]
        open(ini_out, "w").write("\r\n".join(lines) + "\r\n")
        print("  wrote", ini_out)


if __name__ == "__main__":
    main()
