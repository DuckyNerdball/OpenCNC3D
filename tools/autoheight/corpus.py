#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- the corpus: every cartridge map as numpy grids.

One loader for the whole auto-heightmap toolchain (analyse.py, generate.py,
score.py), so the three can never read a map three different ways.

A map is:
    tmpl  [64,64]  template id per cell (defines.h order; 255 = unresolved)
    icon  [64,64]  icon number inside that template
    land  [64,64]  the 1995 land type of that icon (CLEAR, ROAD, WATER, ROCK,
                   WALL, TIBERIUM, BEACH), out of the editor's own template table
    fam   [64,64]  template family code (FAM below)
    h     [65,65]  corner heights, bytes, or None for a map that ships none
    X,Y,W,H        the playable rectangle as the INI declares it
    structs, terrain, overlay   {cell: type} out of the INI

The 55 maps that ship <SCEN>.IMG are the training set: the 51 campaign missions
plus the four N64 Special Ops. Everything else on the cartridge falls back to
FLAT.IMG and is a target, not an example.

Reads the extracted tree (tools/bakery/sharecopy/assets/extracted),
brain/vanilla/tiberiandawn/defines.h and game/edit_tables.h. Read-only.
"""
import os, re, struct
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
EX = os.path.join(ROOT, "tools", "bakery", "sharecopy", "assets", "extracted")
DEFINES = os.path.join(ROOT, "brain", "vanilla", "tiberiandawn", "defines.h")
TABLES = os.path.join(ROOT, "game", "edit_tables.h")

LADDER = (0, 64, 128, 191, 255)
STEP = 64

LAND_NAMES = ("CLEAR", "ROAD", "WATER", "ROCK", "WALL", "TIBERIUM", "BEACH")
L_CLEAR, L_ROAD, L_WATER, L_ROCK, L_WALL, L_TIB, L_BEACH = range(7)

FAMILIES = ("CLEAR", "WATER", "SHORE", "SLOPE", "BRUSH", "PATCH", "BOULDER",
            "ROAD", "RIVER", "FORD", "FALLS", "BRIDGE")
FAM = {f: i for i, f in enumerate(FAMILIES)}

# The thirteen Covert Operations missions: the titled scenarios in 20..59 that
# expand.cpp lists (see docs/terrain-elevation-grammar.md section 0).
MULTIPLAYER = ("SCM01EA", "SCM02EA", "SCM03EA", "SCM04EA", "SCM05EA", "SCM06EA",
               "SCM07EA", "SCM08EA", "SCM09EA")
# The rest of what the game's Special Ops menu offers that ships no heightmap: the five
# C&C95 bonus missions, and the cartridge's development scenarios it files as tests.
BONUS = ("SCB60EA", "SCB61EA", "SCG60EA", "SCG61EA", "SCG62EA")
TEST = ("SCB70EA", "SCG32EA", "SCG71EB", "SCG72EA", "SCG73EA", "SCG74EA")
COVERT_OPS = ("SCB20EA", "SCB31EA", "SCB32EA", "SCB33EA", "SCB35EA", "SCB37EA",
              "SCG22EA", "SCG23EA", "SCG36EA", "SCG38EA", "SCG40EA", "SCG41EA",
              "SCG50EA")


def _template_names():
    names, on = [], False
    for line in open(DEFINES, errors="ignore"):
        s = line.split("//")[0].strip()
        if s.startswith("TEMPLATE_CLEAR1"):
            on = True
        if on:
            if s.startswith("TEMPLATE_COUNT"):
                break
            if s.startswith("TEMPLATE_"):
                names.append(s.rstrip(",").split()[0].rstrip(",").replace("TEMPLATE_", ""))
    return names


def _template_table():
    """(name, land, altland, alt list, w, h) per template id, from edit_tables.h."""
    out, on = [], False
    pat = re.compile(r'\{\s*"([^"]*)",\s*EDIT_LAND_(\w+),\s*EDIT_LAND_(\w+),\s*(\d+),'
                     r'\s*\{([^}]*)\},\s*(\d+),\s*(\d+)\s*\}')
    for line in open(TABLES, errors="ignore"):
        if "EDIT_TEMPLATES[EDIT_TEMPLATE_COUNT]" in line:
            on = True
            continue
        if not on:
            continue
        m = pat.search(line)
        if m:
            nm, la, alt_la, n, alts, w, h = m.groups()
            alts = [int(v) for v in alts.split(",")][:int(n)]
            out.append((nm, LAND_NAMES.index(la), LAND_NAMES.index(alt_la), alts,
                        int(w), int(h)))
        if line.strip().startswith("};"):
            break
    return out


def _footprints():
    """{type: [(dx, dy), ...]} the cells each building occupies, out of the editor's
    own item table (EDIT_ITEMS: name, label, kind, cost, count, xs[12], ys[12])."""
    out = {}
    pat = re.compile(r'\{\s*"([^"]+)",\s*"[^"]*",\s*EDIT_KIND_BUILDING,\s*\d+,\s*(\d+),'
                     r'\s*\{([^}]*)\},\s*\{([^}]*)\}')
    for line in open(TABLES, errors="ignore"):
        m = pat.search(line)
        if m:
            n = int(m.group(2))
            xs = [int(v) for v in m.group(3).split(",")][:n]
            ys = [int(v) for v in m.group(4).split(",")][:n]
            out[m.group(1).upper()] = list(zip(xs, ys))
    return out


TN = _template_names()
FOOTPRINT = _footprints()
TT = _template_table()
N_TEMPLATES = len(TN)


def family_of(t):
    if t >= len(TN):
        return FAM["CLEAR"]
    for f in FAMILIES:
        if TN[t].startswith(f):
            return FAM[f]
    return FAM["CLEAR"]


FAMILY_OF = np.array([family_of(t) for t in range(256)], dtype=np.int8)


def land_of(t, icon):
    if t >= len(TT):
        return L_CLEAR
    nm, la, alt_la, alts, w, h = TT[t]
    return alt_la if icon in alts else la


def is_slope(t):
    return 13 <= t <= 50          # TEMPLATE_SLOPE1 .. SLOPE38


def ini(scen):
    d, sec = {}, None
    for line in open(os.path.join(EX, "INI", scen + ".INI"), errors="ignore"):
        s = line.split(";")[0].strip()
        if s.startswith("["):
            sec = s.strip("[]").upper()
            d.setdefault(sec, [])
        elif "=" in s and sec:
            k, v = s.split("=", 1)
            d[sec].append((k.strip().upper(), v.strip()))
    return d


_th = {}


def theater(root):
    if root not in _th:
        rd = lambda p: open(os.path.join(EX, p), "rb").read()
        _th[root] = dict(n4=os.path.getsize(os.path.join(EX, f"DA4/{root}.DA4")) // 288,
                         n8=os.path.getsize(os.path.join(EX, f"DA8/{root}.DA8")) // 576,
                         tl4=rd(f"TL4/{root}.TL4"), tl8=rd(f"TL8/{root}.TL8"))
    return _th[root]


class Map:
    def __init__(self, scen):
        self.id = scen
        d = ini(scen)
        kv = lambda sec: dict(d.get(sec, []))
        mp = kv("MAP")
        self.name = kv("BASIC").get("NAME", scen)
        th = mp.get("THEATER", "TEMPERATE").upper()
        self.theater = th
        self.root = {"DESERT": "DESERT", "TEMPERATE": "TEMPERAT"}.get(th, "TEMPERAT")
        self.X = int(mp.get("X", 0)); self.Y = int(mp.get("Y", 0))
        self.W = int(mp.get("WIDTH", 64)); self.H = int(mp.get("HEIGHT", 64))
        t = theater(self.root)
        raw = open(os.path.join(EX, "MAP", scen + ".MAP"), "rb").read()
        ids = struct.unpack(">4096H", raw[:8192])
        self.n4 = t["n4"]
        self.tid = np.array(ids, np.int32).reshape(64, 64)
        tm = np.full(4096, 255, np.uint8)
        ic = np.zeros(4096, np.uint8)
        for i, tid in enumerate(ids):
            if tid < t["n4"]:
                tm[i] = t["tl4"][2 * tid]; ic[i] = t["tl4"][2 * tid + 1]
            elif 1024 <= tid < 1024 + t["n8"]:
                k = tid - 1024
                tm[i] = t["tl8"][2 * k]; ic[i] = t["tl8"][2 * k + 1]
        self.tmpl = tm.reshape(64, 64)
        self.icon = ic.reshape(64, 64)
        self.fam = FAMILY_OF[self.tmpl]
        self.land = np.array([land_of(int(a), int(b)) for a, b in zip(tm, ic)],
                             np.int8).reshape(64, 64)
        img = os.path.join(EX, "IMG", scen + ".IMG")
        self.h = None
        if os.path.exists(img):
            blob = open(img, "rb").read()
            hdr, _b, w, h = struct.unpack_from(">IIHH", blob, 0)
            assert (hdr, w, h) == (0x10, 65, 65), scen
            self.h = np.frombuffer(blob[16:16 + 4225], np.uint8).reshape(65, 65).astype(np.int16)
        cells = lambda sec, idx: {int(k) if k.isdigit() else int(v.split(",")[idx]):
                                  v for k, v in d.get(sec, [])}
        # STRUCTURES rows are  n=house,type,strength,cell,facing,trigger
        self.structs = {}
        for k, v in d.get("STRUCTURES", []):
            f = v.split(",")
            if len(f) >= 4 and f[3].strip().isdigit():
                self.structs[int(f[3])] = f[1].strip().upper()
        # TERRAIN and OVERLAY rows are  cell=type
        self.terrain = {int(k): v.split(",")[0].strip().upper()
                        for k, v in d.get("TERRAIN", []) if k.isdigit()}
        self.overlay = {int(k): v.strip().upper()
                        for k, v in d.get("OVERLAY", []) if k.isdigit()}
        # MULTIPLAYER START POSITIONS: waypoints 0..7 of a multiplayer map (house
        # prefix M) are where the players' bases go; everywhere else those waypoints
        # are mission script points and mean nothing to the ground
        wp = {int(k): int(v) for k, v in d.get("WAYPOINTS", []) if k.isdigit()}
        self.starts = [(wp[i] % 64, wp[i] // 64) for i in range(8)
                       if scen.startswith("SCM") and wp.get(i, -1) >= 0]

    # -- helpers ---------------------------------------------------------
    def playable(self):
        """[64,64] bool: inside the declared playable rectangle."""
        m = np.zeros((64, 64), bool)
        m[self.Y:min(64, self.Y + self.H), self.X:min(64, self.X + self.W)] = True
        return m

    def playable_corners(self):
        m = np.zeros((65, 65), bool)
        m[self.Y:min(65, self.Y + self.H + 1), self.X:min(65, self.X + self.W + 1)] = True
        return m

    def footprints(self):
        """[(type, [(x, y), ...])] every pre-placed building and the cells it stands on."""
        out = []
        for cell, typ in self.structs.items():
            cx, cy = cell % 64, cell // 64
            fp = FOOTPRINT.get(typ, [(0, 0)])
            out.append((typ, [(cx + dx, cy + dy) for dx, dy in fp
                              if 0 <= cx + dx < 64 and 0 <= cy + dy < 64]))
        return out

    def base_sites(self, r):
        """[64,64] bool: a disc of radius r cells round every multiplayer start."""
        yy, xx = np.mgrid[0:64, 0:64]
        m = np.zeros((64, 64), bool)
        for x, y in self.starts:
            m |= (xx - x) ** 2 + (yy - y) ** 2 <= r * r
        return m

    def cell_corners(self):
        """[64,64,4] the four corner heights of each cell (nw, ne, sw, se)."""
        h = self.h
        return np.stack([h[:-1, :-1], h[:-1, 1:], h[1:, :-1], h[1:, 1:]], -1)

    def placements(self):
        """Every stamped template: (t, ox, oy, w, h, [(x, y, icon), ...]).

        A cell carrying icon i of a w-wide template was stamped by a placement whose
        origin is (x - i % w, y - i // w); cells that agree on template and origin are
        one placement. Recovering from ANY cell rather than requiring icon 0 at the
        origin matters: several templates (the bridges among them) leave their first
        icon empty, and an origin-only search silently found none of them."""
        groups = {}
        for cy in range(64):
            for cx in range(64):
                t = int(self.tmpl[cy, cx])
                if t >= len(TT) or t == 0:
                    continue
                nm, la, alt_la, alts, w, h = TT[t]
                i = int(self.icon[cy, cx])
                if i >= w * h:
                    continue
                key = (t, cx - i % w, cy - i // w)
                groups.setdefault(key, []).append((cx, cy, i))
        out = []
        for (t, ox, oy), cells in sorted(groups.items(), key=lambda kv: (kv[0][2], kv[0][1], kv[0][0])):
            nm, la, alt_la, alts, w, h = TT[t]
            out.append((t, ox, oy, w, h, cells))
        return out


def all_ids():
    return sorted(f[:-4] for f in os.listdir(os.path.join(EX, "MAP")) if f.endswith(".MAP"))


def training():
    """The 55 maps that ship a hand-made heightmap."""
    return [Map(s) for s in all_ids() if os.path.exists(os.path.join(EX, "IMG", s + ".IMG"))]


def covert_ops():
    return [Map(s) for s in COVERT_OPS]


if __name__ == "__main__":
    tr = training()
    print(f"{len(tr)} maps ship a heightmap; {len(TN)} templates; {len(TT)} in the editor table")
    for m in tr[:3] + covert_ops()[:2]:
        print(m.id, m.theater, m.name, (m.X, m.Y, m.W, m.H), "structs", len(m.structs),
              "terrain", len(m.terrain), "overlay", len(m.overlay),
              "h" if m.h is not None else "flat")
