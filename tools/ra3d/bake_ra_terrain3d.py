#!/usr/bin/env python3
"""
OpenCNC3D RA3D -- Red Alert's terrain for the 3D modes (N64 and Enhanced).

    python3 tools/ra3d/bake_ra_terrain3d.py bank               the three banks: atlases + slot tables
    python3 tools/ra3d/bake_ra_terrain3d.py enh BANK PACK OUT   terrainenh_<BANK>.pack from PACK
    python3 tools/ra3d/bake_ra_terrain3d.py ra144 BANK PACK OUT the same paintings as the Red
                                                                Alert ground's pack (C3DRA144)
    python3 tools/ra3d/bake_ra_terrain3d.py raseam BANK SEAMPACK OUT  its join strips (C3DRASEM)
    python3 tools/ra3d/bake_ra_terrain3d.py frombank BANK ENH SEAM OUTENH OUTSEAM
                                        Red Alert's OWN set, packed by the standard tools
                                        against the bank, as the Red Alert ground's packs
    python3 tools/ra3d/bake_ra_terrain3d.py terrain SCEN NAME   terrain_<NAME>.json for bake5

WHY A BANK OF ITS OWN. Tiberian Dawn's SNOW theater is Red Alert's snow LOOK on Tiberian
Dawn's 140 winter templates, keyed by Tiberian Dawn's template numbers. A Red Alert map is
drawn from Red Alert's 259 snow templates, about eighty of which Tiberian Dawn never had
(the shore cliffs WC01..WC38, the ice-age bridges, RF and RC river pieces, F01..F06), and
those do not fit into the single-byte template numbers the Tiberian Dawn banks use. So
Red Alert terrain is keyed by RED ALERT'S OWN template numbers (TemplateType, defines.h),
in a bank per theater: RASNOW, RATEMP and RAINT, for the theater a mission's [Map] names.

  bank     every icon of every template the theater draws, out of the 1996 disc's theater
           MIX through its own IconControlType header and the theater's palette, packed
           exactly as n64_terrain packs a theater atlas (32 columns, 26-texel pitch, a texel
           of replicated gutter), into tools/bakery/sharecopy/assets/terrain/<BANK>_tiles.png,
           with the slot tables the renderer builds Enhanced's sheet from written to
           game/ra_terrain_tiles.h.
  enh      Enhanced's 144-texel paintings carried from Tiberian Dawn's template numbers to
           Red Alert's BY PICTURE: a Red Alert icon takes the painting of the Tiberian Dawn
           icon its 1995 pixels match (see MATCH below). Names are not trusted: Red Alert
           reused names such as SH01 for shore pieces that are different pictures from
           Tiberian Dawn's SH1. An icon with no match draws its 1995 art, scaled, in
           Enhanced (the pack's DOS atlas is this same 1995 art). Interior has no Tiberian
           Dawn counterpart and so no paintings at all.
  terrain  one mission's 128 x 128 cells, in the form bake5.py reads (terrain JSON).

Reads data/radata (the 1996 discs) and brain/vanilla/redalert. Writes nothing else.
"""
import importlib.util, json, math, os, struct, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import ra_map            # noqa: E402
from ramix import RaMix  # noqa: E402

_spec = importlib.util.spec_from_file_location("ra_classic_terrain", os.path.join(HERE, "bake-classic-terrain.py"))
_ct = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_ct)

DATA = os.path.join(ROOT, "data", "radata")
TERRAIN = os.path.join(ROOT, "tools", "bakery", "sharecopy", "assets", "terrain")
HEADER = os.path.join(ROOT, "game", "ra_terrain_tiles.h")
# bank: (1996 disc MIX, file extension, palette, Red Alert theater flag, Tiberian Dawn bank
# whose paintings it can borrow, the 1995 atlas those paintings were painted over)
THEATERS = {"RASNOW": ("SNOW.MIX", "SNO", "SNOW.PAL", "SNOW", "SNOW", "SNOW_tiles.png"),
            "RATEMP": ("TEMPERAT.MIX", "TEM", "TEMPERAT.PAL", "TEMPERATE", "TEMPERAT",
                       "TEMPERAT_tiles_dos.png"),
            "RAINT": ("INTERIOR.MIX", "INT", "INTERIOR.PAL", "INTERIOR", None, None)}
BANK_OF = {"SNOW": "RASNOW", "TEMPERATE": "RATEMP", "INTERIOR": "RAINT"}
# MATCH: a Red Alert icon borrows a painting only when its 1995 pixels are the same picture
# as the Tiberian Dawn icon's: mean channel difference under MATCH_MEAN (the two games'
# palettes differ by a few steps) and under MATCH_FAR of its pixels off by more than 40.
MATCH_MEAN, MATCH_FAR = 6.0, 0.03
COLS, TS, GUTTER, PITCH = 32, 24, 1, 26


def palette(name):
    raw = RaMix(os.path.join(DATA, "LOCAL.MIX")).read(name)
    return np.array([(v << 2) | (v >> 4) for v in raw], np.uint8).reshape(256, 3)


def bank_templates(theater):
    """[(Red Alert template number, INI name)] for every template the bank's theater
    draws, in enum order."""
    enum, named = ra_map.ra_template_enum(), ra_map.ra_template_theaters()
    flag = THEATERS[theater][3]
    return [(i, named[e][0]) for i, e in enumerate(enum) if e in named and flag in named[e][1]]


_SLOTS = {}


def bank_slots(theater):
    """[(template, icon, 24x24 indexed pixels)] in slot order."""
    if theater in _SLOTS:
        return _SLOTS[theater]
    mixname, ext = THEATERS[theater][:2]
    mix = RaMix(os.path.join(DATA, mixname))
    out = []
    _SLOTS[theater] = out
    for tid, name in bank_templates(theater):
        raw = mix.read("%s.%s" % (name, ext))
        if raw is None:
            continue
        for icon, px in sorted(_ct.icons(raw, name).items()):
            out.append((tid, icon, px))
    return out


def atlas_of(slots, pal):
    rows = (len(slots) + COLS - 1) // COLS
    img = np.zeros((rows * PITCH, COLS * PITCH, 4), np.uint8)
    for s, (_t, _i, px) in enumerate(slots):
        x0, y0 = (s % COLS) * PITCH + GUTTER, (s // COLS) * PITCH + GUTTER
        rgb = pal[np.frombuffer(px, np.uint8)].reshape(TS, TS, 3)
        img[y0:y0 + TS, x0:x0 + TS, :3] = rgb
        img[y0:y0 + TS, x0:x0 + TS, 3] = 255
        img[y0 - 1, x0:x0 + TS] = img[y0, x0:x0 + TS]
        img[y0 + TS, x0:x0 + TS] = img[y0 + TS - 1, x0:x0 + TS]
        img[y0 - 1:y0 + TS + 1, x0 - 1] = img[y0 - 1:y0 + TS + 1, x0]
        img[y0 - 1:y0 + TS + 1, x0 + TS] = img[y0 - 1:y0 + TS + 1, x0 + TS - 1]
    return img


def cmd_bank():
    L = ["/* ====================================================================================",
         " *  ra_terrain_tiles.h -- GENERATED by tools/ra3d/bake_ra_terrain3d.py bank. Do not edit.",
         " *",
         " *  Red Alert's terrain banks for the 3D modes, keyed by Red Alert's OWN template numbers",
         " *  (TemplateType, brain/vanilla/redalert/defines.h): (template, icon) -> atlas slot, the",
         " *  same packing as the Tiberian Dawn banks in terrain_tiles.h, which includes this file.",
         " * ==================================================================================== */"]
    for theater in THEATERS:
        slots = bank_slots(theater)
        pal = palette(THEATERS[theater][2])
        img = atlas_of(slots, pal)
        png = os.path.join(TERRAIN, theater + "_tiles.png")
        Image.fromarray(img, "RGBA").save(png)
        names = dict(bank_templates(theater))
        L += ["",
              "/* %s: %d tiles, %d templates, from the 1996 disc's %s. */"
              % (theater, len(slots), len({t for t, _i, _p in slots}), THEATERS[theater][0]),
              "#define TT_%s_N %d" % (theater, len(slots)),
              "#define TT_%s_SLOTS %d" % (theater, len(slots)),
              "static const TtSlot TT_%s[TT_%s_N] = {" % (theater, theater)]
        for s, (t, i, _p) in enumerate(slots):
            L.append("    { %d, %d, %d, 0 },   /* %s */" % (t, i, s, names.get(t, "?")) if i == 0 else
                     "    { %d, %d, %d, 0 }," % (t, i, s))
        L.append("};")
        print("bank %s: %d tiles from %d templates -> %s (%dx%d)"
              % (theater, len(slots), len({t for t, _i, _p in slots}), os.path.relpath(png, ROOT),
                 img.shape[1], img.shape[0]))
    open(HEADER, "w").write("\n".join(L) + "\n")
    print("slot tables ->", os.path.relpath(HEADER, ROOT))


def td_slots(td_bank):
    """[(Tiberian Dawn template, icon, slot)] of a Tiberian Dawn bank, from terrain_tiles.h."""
    import re
    src = open(os.path.join(ROOT, "game", "terrain_tiles.h")).read()
    a = src.index("static const TtSlot TT_%s[" % td_bank)
    body = src[a:src.index("};", a)]
    return [(int(t), int(i), int(s)) for t, i, s in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+),", body)]


def picture_match(theater):
    """{(Red Alert template, icon): (Tiberian Dawn template, icon)} for every Red Alert icon
    whose 1995 pixels are the same picture as a Tiberian Dawn icon's (MATCH above)."""
    _mix, _ext, palname, _flag, td_bank, td_atlas = THEATERS[theater]
    if td_bank is None:
        return {}
    tds = td_slots(td_bank)
    atl = np.asarray(Image.open(os.path.join(TERRAIN, td_atlas)).convert("RGB"), np.float32)
    tdv = np.stack([atl[(s // COLS) * PITCH + GUTTER:(s // COLS) * PITCH + GUTTER + TS,
                        (s % COLS) * PITCH + GUTTER:(s % COLS) * PITCH + GUTTER + TS].reshape(-1)
                    for _t, _i, s in tds])
    pal = palette(palname).astype(np.float32)
    slots = bank_slots(theater)
    out = {}
    for k in range(0, len(slots), 16):
        chunk = slots[k:k + 16]
        R = np.stack([pal[np.frombuffer(px, np.uint8)].reshape(-1) for _t, _i, px in chunk])
        d = np.abs(R[:, None, :] - tdv[None, :, :])
        mean = d.mean(2)
        for n, (t, i, _px) in enumerate(chunk):
            j = int(mean[n].argmin())
            if mean[n, j] < MATCH_MEAN and (d[n, j] > 40).mean() < MATCH_FAR:
                out[(t, i)] = tds[j][:2]
    return out


def cmd_enh(theater, src, dst):
    blob = open(src, "rb").read()
    magic, ver, cell, n = blob[:8], *struct.unpack_from("<III", blob, 8)
    keys = [struct.unpack_from("<HH", blob, 20 + i * 4) for i in range(n)]
    px = 20 + n * 4
    size = cell * cell * 3
    painted = {k: j for j, k in enumerate(keys)}
    match = picture_match(theater)
    out_keys, out_px = [], []
    for ra_key, td_key in sorted(match.items()):
        j = painted.get(td_key)
        if j is None:
            continue
        out_keys.append(ra_key)
        out_px.append(blob[px + j * size:px + (j + 1) * size])
    with open(dst, "wb") as f:
        f.write(magic + struct.pack("<III", ver, cell, len(out_keys)))
        for t, i in out_keys:
            f.write(struct.pack("<HH", t, i))
        for p in out_px:
            f.write(p)
    print("enh %s: %d of %d Red Alert icons painted (%d match a Tiberian Dawn icon's picture); "
          "the rest draw their 1995 art -> %s"
          % (theater, len(out_keys), len(bank_slots(theater)), len(match), dst))


def template_sizes(theater):
    """{INI name: (width, height) in cells} for every template of the bank's theater, off
    each template file's own IconControlType header."""
    mixname, ext = THEATERS[theater][:2]
    mix = RaMix(os.path.join(DATA, mixname))
    out = {}
    for _tid, name in bank_templates(theater):
        raw = mix.read("%s.%s" % (name, ext))
        if raw is None:
            continue
        w, h = struct.unpack_from("<6h", raw)[4:6]
        out[name] = (max(1, w), max(1, h))
    return out


def cmd_ra144(theater, src, dst):
    """THE RED ALERT GROUND'S PACK (game/ra3d_terrain.h, C3DRA144 version 1): one 144-texel
    RGB painting per (template INI name, icon, variant 0), with the template's size, for
    every Red Alert icon picture_match carries a Tiberian Dawn painting to. The ground
    draws the 1995 tile, enlarged, for every icon not in it. Paintings made for Red
    Alert's own templates take this pack's place when they exist."""
    blob = open(src, "rb").read()
    _magic, _ver, cell, n = blob[:8], *struct.unpack_from("<III", blob, 8)
    keys = [struct.unpack_from("<HH", blob, 20 + i * 4) for i in range(n)]
    px = 20 + n * 4
    size = cell * cell * 3
    painted = {k: j for j, k in enumerate(keys)}
    names = dict(bank_templates(theater))
    dims = template_sizes(theater)
    entries = []
    for (t, i), td_key in sorted(picture_match(theater).items()):
        j = painted.get(td_key)
        nm = names.get(t)
        if j is None or nm is None:
            continue
        w, h = dims.get(nm, (1, 1))
        entries.append((nm, i, ((w - 1) & 15) | (((h - 1) & 15) << 4),
                        blob[px + j * size:px + (j + 1) * size]))
    with open(dst, "wb") as f:
        f.write(b"C3DRA144" + struct.pack("<III", 1, cell, len(entries)))
        for nm, i, sz, _p in entries:
            f.write(nm.encode("ascii")[:16].ljust(16, b"\0") + struct.pack("<HBB", i, 0, sz))
        for *_k, p in entries:
            f.write(p)
    print("ra144 %s: %d Red Alert icons painted from %s -> %s"
          % (theater, len(entries), os.path.basename(src), dst))


def cmd_raseam(theater, src, dst):
    """THE RED ALERT GROUND'S JOIN STRIPS (game/ra3d_terrain.h, C3DRASEM version 1): for
    every Red Alert icon picture_match carries a painting to, the strips Tiberian Dawn's
    seam pack (C3DSEAM) holds for that painting, variant 0 only (a Tiberian Dawn variant's
    icon field carries bits above the icon's eight, and those are skipped)."""
    blob = open(src, "rb").read()
    if blob[:8] != b"C3DSEAM\0":
        raise SystemExit("raseam: %s is not a Tiberian Dawn seam pack" % src)
    _ver, cell, margin, n = struct.unpack_from("<4I", blob, 8)
    strip = cell * margin * 4
    body = 24 + n * 8
    strips = {}
    for k in range(n):
        t, i, side, kind = struct.unpack_from("<2H2B", blob, 24 + k * 8)
        if i < 256:
            strips[(t, i, side)] = (kind, blob[body + k * strip:body + (k + 1) * strip])
    names = dict(bank_templates(theater))
    dims = template_sizes(theater)
    entries = []
    for (t, i), (tt, ti) in sorted(picture_match(theater).items()):
        nm = names.get(t)
        if nm is None:
            continue
        w, h = dims.get(nm, (1, 1))
        sz = ((w - 1) & 15) | (((h - 1) & 15) << 4)
        for side in (0, 1):
            got = strips.get((tt, ti, side))
            if got:
                entries.append((nm, i, side, got[0], sz, got[1]))
    with open(dst, "wb") as f:
        f.write(b"C3DRASEM" + struct.pack("<4I", 1, cell, margin, len(entries)))
        for nm, i, side, kind, sz, _p in entries:
            f.write(nm.encode("ascii")[:16].ljust(16, b"\0") + struct.pack("<HBBBBH", i, side, kind, 0, sz, 0))
        for *_k, p in entries:
            f.write(p)
    print("raseam %s: %d strips from %s -> %s" % (theater, len(entries), os.path.basename(src), dst))


def _bank_field(field, t, names, dims):
    """A standard pack's icon field (tools/bakery/pack_remaster_terrain.py: the icon's eight
    bits, a variant in bits 8-9 with the template's size less one in bits 10-12 and 13-15)
    -> (INI name, icon, variant, size byte as ra3d_terrain.h reads it), or None."""
    nm = names.get(t)
    if nm is None:
        return None
    icon, var = field & 255, (field >> 8) & 3
    if var:
        w, h = ((field >> 10) & 7) + 1, ((field >> 13) & 7) + 1
    else:
        w, h = dims.get(nm, (1, 1))
    return nm, icon, var, ((w - 1) & 15) | (((h - 1) & 15) << 4)


def cmd_frombank(theater, enh, seam, out_enh, out_seam):
    """RED ALERT'S OWN 144 SET into the Red Alert ground's formats. The set is painted on
    the bank's templates (art/remaster/textures/<BANK>, keyed by Red Alert's template
    numbers like ra_terrain_tiles.h) and packed by the standard tools (C3DT30 cells,
    C3DSEAM strips); game/ra3d_terrain.h reads cells and strips by template NAME
    (C3DRA144, C3DRASEM). Same pixels, variants and strips kept, keys renamed."""
    names = dict(bank_templates(theater))
    dims = template_sizes(theater)
    blob = open(enh, "rb").read()
    if blob[:8] != b"C3DT30\0\0":
        raise SystemExit("frombank: %s is not a standard terrain pack" % enh)
    _ver, cell, n = struct.unpack_from("<3I", blob, 8)
    size = cell * cell * 3
    px = 20 + n * 4
    entries, skipped = [], 0
    for k in range(n):
        t, field = struct.unpack_from("<HH", blob, 20 + k * 4)
        key = _bank_field(field, t, names, dims)
        if key is None:
            skipped += 1
            continue
        entries.append(key + (blob[px + k * size:px + (k + 1) * size],))
    with open(out_enh, "wb") as f:
        f.write(b"C3DRA144" + struct.pack("<III", 1, cell, len(entries)))
        for nm, icon, var, sz, _p in entries:
            f.write(nm.encode("ascii")[:16].ljust(16, b"\0") + struct.pack("<HBB", icon, var, sz))
        for *_k, p in entries:
            f.write(p)
    print("frombank %s: %d cells (%d variants) -> %s%s" % (
        theater, len(entries), sum(1 for e in entries if e[2]), out_enh,
        "; %d keys name no template of the bank" % skipped if skipped else ""))
    blob = open(seam, "rb").read()
    if blob[:8] != b"C3DSEAM\0":
        raise SystemExit("frombank: %s is not a standard seam pack" % seam)
    _ver, cell, margin, n = struct.unpack_from("<4I", blob, 8)
    strip = cell * margin * 4
    body = 24 + n * 8
    strips = []
    for k in range(n):
        t, field, side, kind = struct.unpack_from("<2H2B", blob, 24 + k * 8)
        key = _bank_field(field, t, names, dims)
        if key is not None:
            strips.append((key[0], key[1], side, kind, key[2], key[3],
                           blob[body + k * strip:body + (k + 1) * strip]))
    with open(out_seam, "wb") as f:
        f.write(b"C3DRASEM" + struct.pack("<4I", 1, cell, margin, len(strips)))
        for nm, icon, side, kind, var, sz, _p in strips:
            f.write(nm.encode("ascii")[:16].ljust(16, b"\0") + struct.pack("<HBBBBH", icon, side, kind, var, sz, 0))
        for *_k, p in strips:
            f.write(p)
    print("frombank %s: %d strips -> %s" % (theater, len(strips), out_seam))


def cmd_terrain(scen, name):
    r = ra_map.translate(scen)
    theater = BANK_OF[r["theater"]]
    slots = bank_slots(theater)
    slot = {(t, i): s for s, (t, i, _p) in enumerate(slots)}
    names = dict(bank_templates(theater))
    clear = 0                                        # CLEAR1 is template 0
    rows = (len(slots) + COLS - 1) // COLS
    AW, AH = COLS * PITCH, rows * PITCH
    W = H = ra_map.W
    cells, st = [], dict(clear=0, bank8=0, unmapped=0, holes=0)
    for c in range(W * H):
        x, y = c % W, c // W
        t, i = r["ra_t"][c], r["ra_i"][c]
        s = slot.get((t, i))
        if t in (0xFFFF, 0xFF) or s is None:
            if t not in (0xFFFF, 0xFF):
                st["unmapped"] += 1
            t, i = clear, (x & 3) | ((y & 3) << 2)
            s = slot.get((t, i), slot.get((t, 0)))
            st["clear"] += 1
        else:
            st["bank8"] += 1
        u0, v0 = (s % COLS) * PITCH + GUTTER, (s // COLS) * PITCH + GUTTER
        cells.append(dict(x=x, y=y, tileId=s, drawTile=s, template=t, icon=i,
                          templateName=names.get(t, "CLEAR1"), bank=8, hasHoles=False,
                          u0=u0 / AW, v0=v0 / AH, u1=(u0 + TS) / AW, v1=(v0 + TS) / AH,
                          px=u0, py=v0))
    doc = dict(scenario=name, theater=theater, width=W, height=H, tileSize=TS,
               atlas=theater + "_tiles.png", atlasPath="assets/terrain/%s_tiles.png" % theater,
               atlasDos=theater + "_tiles.png", atlasWidth=AW, atlasHeight=AH,
               note="Red Alert %s's own terrain, keyed by Red Alert's template numbers "
                    "(tools/ra3d/bake_ra_terrain3d.py)" % scen,
               source=scen, rect=list(r["rect"]), stats=st, cells=cells)
    out = os.path.join(TERRAIN, "terrain_%s.json" % name)
    json.dump(doc, open(out, "w"))
    print("terrain %s (%s, %s): %d x %d, %s -> %s"
          % (name, scen, theater, W, H, st, os.path.relpath(out, ROOT)))
    return theater


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__); sys.exit(2)
    if a[0] == "bank":
        cmd_bank()
    elif a[0] == "enh":
        cmd_enh(a[1], a[2], a[3])
    elif a[0] == "ra144":
        cmd_ra144(a[1], a[2], a[3])
    elif a[0] == "raseam":
        cmd_raseam(a[1], a[2], a[3])
    elif a[0] == "frombank":
        cmd_frombank(a[1], a[2], a[3], a[4], a[5])
    elif a[0] == "terrain":
        cmd_terrain(a[1], a[2])
    else:
        print(__doc__); sys.exit(2)


if __name__ == "__main__":
    main()
