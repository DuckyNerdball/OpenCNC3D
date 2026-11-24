#!/usr/bin/env python3
"""
check_course.py -- prove that a Rally course map holds up every claim it makes, and draw it.

WHAT A RALLY COURSE IS
  An ordinary 64x64 Tiberian Dawn multiplayer map (SCMnnEA.INI + an 8192-byte .BIN, cell
  = y*64 + x) with a [Rally] section: the centre line with its driveable half width at
  every point, the start grid, the finish line and the crate spots. The track's edges
  are sandbag wall overlay, the enemy defences are structures owned by the course's
  hostile house, and the scenery (trees, Tiberium, civilian and hostile buildings) sits
  outside the walls.

  A course is a CIRCUIT when [Rally] says Loop=1: the points go once round the lap from
  P000, which is on the start/finish line, to the point before it; Finish= is that line,
  across the track at P000; the grid stands behind it on the lap's final straight. A
  course without the key is a point-to-point run: the points go from the start line to
  the finish and Finish= crosses the last point.

WHY A SEPARATE CHECKER
  tools/rally/make_course.py writes the courses. This file re-reads what it wrote, from the
  files alone, and checks it against the engine's own data: the land type of every
  template, which land types a tracked and a wheeled vehicle can enter, the footprint of
  every tree and building, the range of every weapon and the power of every power plant
  are all parsed out of the engine source in this repository (cdata.cpp, const.cpp,
  tdata.cpp, odata.cpp, bdata.cpp, building.cpp), never typed in. A course that passes
  here is a course the engine will load the way it was drawn.

WHAT IS ASSERTED, per course
  1. Every point's half width is at least 1.75 cells (no pinch narrower than 3.5 cells),
     and it is MEASURED: within a lepton or two of the distance from the point to the
     nearest blocked cell's square, less the generator's 16-lepton margin. Every course
     point's cell, and every cell whose centre lies within the point's half width of it,
     is passable for both TRACK and WHEEL locomotion and carries no wall, building, tree
     or overlay other than a crate. The cross-section from -hw to +hw along the point's
     normal touches no blocked cell either, and nor does the centre line between one
     point and the next (on a circuit, the last point back to P000 too).
  2. The walls are a closed barrier: an 8-connected flood from the grid over every
     enterable cell of the map, diagonal squeezes between two blocked cells included,
     never gets next to a tree, a building or Tiberium (all of which stand outside the
     walls) and never strays more than 10 cells from the course points, grid slots and
     finish ends. A fork's second route is part of the track, so the flood covers it.
     A point-to-point centre line is 250 to 350 cells long; a lap of a circuit 150 to
     230, and its last point is about a cell short of P000 (0.5 to 1.5 cells).
  3. The flood reaches the finish line.
  4. Every structure owned by the hostile house is outside the track; every one with a
     weapon has its fire centre within that weapon's range of some course point, by the
     engine's own Distance(), and stands 1 to 3 cells beyond the track edge.
  5. Grid slots: eight of them, each on a track cell, each in a cell of its own, at least
     1.5 cells from every other slot and clear of every blocked cell by a vehicle's radius.
     Waypoints 0..7 are the grid cells, with no gap, and nothing else below 26 is set.
     On a circuit every slot is behind the start/finish line by more than a vehicle's
     radius, its nearest course point is one of the lap's last eight, and GridFacing is
     the direction of travel at P000.
  6. Crate spots are on track cells and clear of the walls.
  7. Power: the hostile house's power plants cover the drain of everything it owns.
  8. The finish segment crosses the centre line at the last point (on a circuit at P000),
     square to the direction of travel there, both its ends lie on the track's edge with a
     blocked cell just beyond each, and it crosses no other part of the course. On a
     circuit it is centred on P000 and reaches P000's half width either side, from the
     left of the direction of travel to the right: the segment the eyes draw as the start
     line, so the two strips coincide.
  9. Every [MultiN] house is allied only with itself, and there are eight of them.
 10. Lanes far apart along the course keep apart on the map. Every track cell belongs to
     its nearest course point; two track cells whose points are more than 12 apart along
     the course (counted round the lap on a circuit; the brain's search for a racer's
     place spans 12 points) are at least 4 cells apart, so between them lie two walls and
     at least one cell of ground. No wall is shared by two such lanes, and a racer on one
     cannot be put down on the other.

THE PICTURE
  --png DIR draws each course top down, 12 pixels per cell: ground, track, walls, trees,
  Tiberium, buildings by owner, every defence with its weapon range as a circle, the
  centre line, the grid, the finish and the crates. Look at it before trusting a course.

Run:  python3 tools/rally/check_course.py [--png DIR] SCM70EA [SCM71EA ...]
      (reads game/missions/<SCEN>.INI and .BIN unless --dir names another directory)
"""
import argparse
import math
import os
import re
import runpy
import sys
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
VANILLA = os.path.join(ROOT, "brain", "vanilla", "tiberiandawn")
MISSIONS = os.path.join(ROOT, "game", "missions")

MAP_W = 64                 # the legacy INI cell stride; a .BIN of 8192 bytes is 64x64
LEPTON = 256               # leptons per cell
GRID_SLOTS = 8
GRID_MIN_APART = 384       # 1.5 cells between slot centres
VEHICLE_RADIUS = 96        # clearance a grid slot or crate needs from any blocked square
DEFENCE_GAP = (1, 3)       # cells of ground between the track edge and a defence
MIN_HALF_WIDTH = 448       # 1.75 cells: no point leaves a racer less than 3.5 cells of room
LEAK_RADIUS = 10           # cells: no track cell lies further than this from a lane point,
                           # a grid slot or a finish end (run-offs and the grid's back end
                           # are the furthest legitimate ones, at about 9)
COURSE_LENGTH = (250, 350) # cells of centre line, a race of about 60 to 90 seconds
LAP_LENGTH = (150, 230)    # cells round a circuit, a lap of about 25 to 35 seconds
CLOSING_GAP = (128, 384)   # leptons from a circuit's last point back to P000
HW_MARGIN = 16             # leptons the generator keeps back from the measured clearance
LANE_WINDOW = 12           # course points: lanes further apart than this along the course
LANE_APART = 4             # must have track cells at least this many cells apart
GRID_TAIL = 8              # a circuit's grid projects onto the lap's last this many points

# The engine's movement-speed columns in Ground[] (defines.h SpeedType order).
SPEED_TRACK = 1
SPEED_WHEEL = 3


# ================================================================== engine facts ==

def _src(name):
    return open(os.path.join(VANILLA, name), errors="replace").read()


def _offsets(expr_list, mcw="MAP_CELL_W"):
    """A footprint offset list ("0, MAP_CELL_W + 1, REFRESH_EOL") as (dx, dy) pairs.

    The offsets are cell numbers relative to the object's top-left cell on a MAP_CELL_W
    wide grid. They are evaluated on the 64-wide grid and split back into x and y,
    rounding so that a negative row (the SAM's overlap is one row up) decomposes too."""
    out = []
    for e in expr_list.split(","):
        e = e.strip()
        if not e or "REFRESH_EOL" in e:
            continue
        v = eval(e.replace(mcw, "64"), {"__builtins__": {}}, {})
        dy = int(round(v / 64.0))
        out.append((v - dy * 64, dy))
    return out


class Engine(object):
    """Everything this checker needs to know about the engine, read from its source."""

    def __init__(self):
        skirmish = runpy.run_path(os.path.join(ROOT, "game", "missions", "make_skirmish_map.py"))
        self.lands = skirmish["load_template_lands"]()
        self._land_type = skirmish["land_type"]
        if len(self.lands) != skirmish["template_count"]():
            raise SystemExit("template land table is stale: %d entries, enum has %d"
                             % (len(self.lands), skirmish["template_count"]()))
        self.ground = self._ground()
        self.terrain = self._terrain()
        self.overlay = self._overlay()
        self.weapons = self._weapons()
        self.buildings = self._buildings()

    # const.cpp Ground[]: per land type, the movement cost for each locomotion; S1 (0)
    # means the locomotion cannot enter that land at all.
    def _ground(self):
        src = _src("const.cpp")
        defs = dict((k, int(v, 16)) for k, v in re.findall(r"#define (S\d) (0x[0-9A-Fa-f]+)", src))
        body = src[src.index("GroundType const Ground[LAND_COUNT]"):]
        body = body[:body.index("};")]
        out = {}
        for speeds, land in re.findall(r"\{\s*\w+,\s*\{([^}]*)\},\s*\w+\},\s*//\s*(LAND_\w+)", body):
            out[land] = [defs[s.strip()] for s in speeds.split(",")]
        if len(out) != 7:
            raise SystemExit("const.cpp Ground[] parse found %d land types, expected 7" % len(out))
        return out

    def land_passable(self, land):
        speeds = self.ground[land]
        return speeds[SPEED_TRACK] != 0 and speeds[SPEED_WHEEL] != 0

    def land_type(self, template, icon):
        return self._land_type(self.lands, template, icon)

    # tdata.cpp: every terrain object's INI name, theaters and occupy/overlap lists.
    def _terrain(self):
        src = _src("tdata.cpp")
        lists = dict(re.findall(r"static short const (_List\d+)\[\]\s*=\s*\{([^}]+)\}", src))
        out = {}
        for m in re.finditer(r"static TerrainTypeClass const\s+\w+\((.*?)\);", src, re.S):
            body = m.group(1)
            name = re.search(r'"([A-Z0-9]+)"', body).group(1)
            theaters = set(t.replace("THEATERF_", "")
                           for t in re.findall(r"THEATERF_\w+", body))
            refs = re.findall(r"\(short const\s*\*\)\s*(_List\d+)", body)
            occupy = _offsets(lists[refs[0]])
            overlap = _offsets(lists[refs[1]]) if len(refs) > 1 and refs[1] in lists else []
            spawns = re.search(r"(true|false),\s*//\s*Spawns Tiberium", body).group(1) == "true"
            out[name] = {"theaters": theaters, "occupy": occupy, "overlap": overlap,
                         "spawns_tiberium": spawns}
        return out

    # odata.cpp: every overlay's INI name, land type, and whether it is a wall or a crate.
    def _overlay(self):
        src = _src("odata.cpp")
        out = {}
        for m in re.finditer(r"static OverlayTypeClass const\s+\w+\((.*?)\n\s*\);", src, re.S):
            body = m.group(1)
            name = re.search(r'"([A-Z0-9]+)"', body).group(1)
            land = re.search(r"(LAND_\w+)", body).group(1)
            wall = re.search(r"(true|false),?\s*//\s*Is this a wall type", body).group(1) == "true"
            crate = re.search(r"(true|false),?\s*//\s*Is this a crate", body).group(1) == "true"
            tib = re.search(r"(true|false),?\s*//\s*Is this harvestable Tiberium", body).group(1) == "true"
            out[name] = {"land": land, "wall": wall, "crate": crate, "tiberium": tib}
        return out

    # const.cpp Weapons[], in defines.h WeaponType order: range in leptons.
    def _weapons(self):
        defs = _src("defines.h")
        head = defs.index("WEAPON_NONE = -1")
        names = re.findall(r"(WEAPON_\w+)", defs[head:defs.index("WEAPON_COUNT", head)])[1:]
        src = _src("const.cpp")
        body = src[src.index("WeaponTypeClass const Weapons[WEAPON_COUNT]"):]
        body = body[:body.index("};")]
        rows = re.findall(r"\{\s*BULLET_\w+,\s*\d+,\s*\d+,\s*(0x[0-9A-Fa-f]+),", body)
        if len(rows) != len(names):
            raise SystemExit("const.cpp Weapons[] has %d rows, defines.h %d weapons"
                             % (len(rows), len(names)))
        return dict((n, int(r, 16)) for n, r in zip(names, rows))

    # bdata.cpp: every structure's INI name, weapon, power, drain, size and footprints;
    # building.cpp: the fire centre offset for each size.
    def _buildings(self):
        src = _src("bdata.cpp")
        lists = {}
        for n, v in re.findall(r"static short const (\w+)\[\]\s*=\s*\{([^}]+)\}", src):
            if "XYCELL" not in v:
                lists[n] = _offsets(v, "MCW")
        defs = _src("defines.h")
        head = defs.index("BSIZE_NONE = -1")
        sizes = re.findall(r"(BSIZE_\w+)", defs[head:defs.index("BSIZE_COUNT", head)])[1:]
        bsrc = _src("building.cpp")
        table = bsrc[bsrc.index("BuildingClass::CenterOffset[BSIZE_COUNT]"):]
        table = table[:table.index("};")]
        centres = [int(v, 16) for v in re.findall(r"(0x[0-9A-Fa-f]+)L", table)]
        if len(centres) != len(sizes):
            raise SystemExit("building.cpp CenterOffset has %d entries, BSIZE %d"
                             % (len(centres), len(sizes)))
        body_all = src[:src.index("BuildingTypeClass const* const BuildingTypeClass::Pointers")]
        out = {}
        for b in body_all.split("static BuildingTypeClass const")[1:]:
            name = re.search(r'"([A-Z0-9]+)"', b).group(1)
            weapon = re.findall(r"^\s*(WEAPON_\w+)\s*,", b, re.M)[0]
            size = re.search(r"(BSIZE_\w+)", b).group(1)
            occ = re.search(r"\(short const\s*\*\)\s*(\w+)\s*,\s*//\s*OCCUPYLIST", b).group(1)
            ovl = re.search(r"\(short const\s*\*\)\s*(\w+)\s*[,)]?\s*//\s*OVERLAPLIST", b).group(1)
            w, h = int(size[6]), int(size[7])
            c = centres[sizes.index(size)]
            out[name] = {
                "bib": re.search(r"(true|false),\s*//\s*Requires a bib", b).group(1) == "true",
                "weapon": weapon,
                "range": self.weapons.get(weapon, 0),
                "power": int(re.search(r"(\d+),\s*//\s*POWER", b).group(1)),
                "drain": int(re.search(r"(\d+),\s*//\s*DRAIN", b).group(1)),
                "size": (w, h),
                "occupy": lists.get(occ, []),
                "overlap": lists.get(ovl, []) if ovl != "NULL" else [],
                "centre": (c & 0xFFFF, (c >> 16) & 0xFFFF),     # COORDINATE: y high, x low
            }
        return out

    def building_cells(self, name, x, y):
        """(occupy, overlap, bib) cell sets of a structure whose top-left cell is (x, y).

        The bib is the smudge the engine lays under a bibbed building: its width, two
        rows tall, starting on the building's bottom row (bdata.cpp Bib_And_Offset)."""
        b = self.buildings[name]
        occ = set((x + dx, y + dy) for dx, dy in b["occupy"])
        ovl = set((x + dx, y + dy) for dx, dy in b["overlap"])
        bib = set()
        if b["bib"] and b["size"][0] in (2, 3, 4):
            w, h = b["size"]
            bib = set((x + i, y + h - 1 + j) for i in range(w) for j in range(2))
        return occ, ovl, bib

    def terrain_cells(self, name, x, y):
        t = self.terrain[name]
        return (set((x + dx, y + dy) for dx, dy in t["occupy"]),
                set((x + dx, y + dy) for dx, dy in t["overlap"]))


def engine_distance(ax, ay, bx, by):
    """The engine's Distance() between two lepton coordinates: the larger axis plus half
    the smaller, the octagonal estimate every range test in Tiberian Dawn uses."""
    dx, dy = abs(ax - bx), abs(ay - by)
    return dx + dy // 2 if dx > dy else dy + dx // 2


# ===================================================================== the files ==

def ini_sections(text):
    secs = {}
    cur = None
    for raw in text.replace("\r", "").split("\n"):
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        m = re.match(r"^\[(.+)\]$", line)
        if m:
            cur = m.group(1)
            secs.setdefault(cur, [])
            continue
        if cur is not None and "=" in line:
            k, v = line.split("=", 1)
            secs[cur].append((k.strip(), v.strip()))
    return secs


def read_course(directory, scen):
    ini_path = os.path.join(directory, scen + ".INI")
    bin_path = os.path.join(directory, scen + ".BIN")
    secs = ini_sections(open(ini_path, errors="replace").read())
    data = open(bin_path, "rb").read()
    if len(data) != MAP_W * MAP_W * 2:
        raise SystemExit("%s is %d bytes; a course is a 64x64 map, 8192 bytes" % (bin_path, len(data)))
    tiles = [[(data[(y * MAP_W + x) * 2], data[(y * MAP_W + x) * 2 + 1])
              for x in range(MAP_W)] for y in range(MAP_W)]
    return secs, tiles


def ints(v):
    return [int(t) for t in v.split(",")]


def parse_rally(secs):
    r = dict(secs.get("Rally", []))
    out = {"name": r.get("Name", ""), "hostile": r.get("Hostile", "")}
    m = re.match(r"\s*[-+]?\d+", r.get("Loop", ""))
    out["loop"] = bool(m and int(m.group(0)) != 0)
    n = int(r.get("Points", "0"))
    out["points"] = [ints(r["P%03d" % i]) for i in range(n)]
    out["facing"] = int(r.get("GridFacing", "-1"))
    out["grid"] = [ints(r["Grid%d" % i]) for i in range(GRID_SLOTS) if ("Grid%d" % i) in r]
    out["finish"] = ints(r["Finish"]) if "Finish" in r else None
    c = int(r.get("Crates", "0"))
    out["crates"] = [ints(r["C%02d" % i]) for i in range(c)]
    out["branches"] = []
    for b in range(int(r.get("Branches", "0"))):
        k = int(r["B%dPoints" % b])
        out["branches"].append({
            "from": int(r["B%dFrom" % b]), "to": int(r["B%dTo" % b]),
            "points": [ints(r["B%dP%03d" % (b, i)]) for i in range(k)]})
    return out


# ===================================================================== the check ==

class Course(object):
    """A course read back from its files, with every cell classified."""

    def __init__(self, eng, directory, scen):
        self.scen = scen
        self.eng = eng
        self.secs, self.tiles = read_course(directory, scen)
        self.rally = parse_rally(self.secs)
        mp = dict(self.secs.get("MAP", []))
        self.theater = mp.get("Theater", "")
        self.rect = (int(mp["X"]), int(mp["Y"]), int(mp["Width"]), int(mp["Height"]))
        self.problems = []
        self.notes = []

        # Ground passability from the templates.
        self.land = [[eng.land_type(*self.tiles[y][x]) for x in range(MAP_W)] for y in range(MAP_W)]
        # Objects by cell.
        self.walls = set()
        self.overlay = {}
        for k, v in self.secs.get("OVERLAY", []):
            c = int(k)
            self.overlay[(c % MAP_W, c // MAP_W)] = v
            if v not in eng.overlay:
                self.problems.append("overlay %s at cell %s is not an engine overlay" % (v, k))
            elif eng.overlay[v]["wall"]:
                self.walls.add((c % MAP_W, c // MAP_W))
        self.trees = []          # (name, x, y, occupy, overlap)
        for k, v in self.secs.get("TERRAIN", []):
            c = int(k)
            name = v.split(",")[0]
            if name not in eng.terrain:
                self.problems.append("terrain %s at cell %s is not an engine terrain type" % (name, k))
                continue
            occ, ovl = eng.terrain_cells(name, c % MAP_W, c // MAP_W)
            self.trees.append((name, c % MAP_W, c // MAP_W, occ, ovl))
        self.structures = []     # (house, name, x, y, occupy, overlap, bib)
        for k, v in self.secs.get("STRUCTURES", []):
            f = v.split(",")
            house, name, cell = f[0], f[1], int(f[3])
            if name not in eng.buildings:
                self.problems.append("structure %s is not an engine building type" % name)
                continue
            occ, ovl, bib = eng.building_cells(name, cell % MAP_W, cell // MAP_W)
            self.structures.append((house, name, cell % MAP_W, cell // MAP_W, occ, ovl, bib))
        self.units = [v for k, v in self.secs.get("UNITS", [])]
        self.infantry = [v for k, v in self.secs.get("INFANTRY", [])]

        self.blocked = set()     # what a racer cannot drive into
        for y in range(MAP_W):
            for x in range(MAP_W):
                if not eng.land_passable(self.land[y][x]):
                    self.blocked.add((x, y))
        self.blocked |= self.walls
        for t in self.trees:
            self.blocked |= t[3]
        for s in self.structures:
            self.blocked |= s[4]
        x0, y0, w, h = self.rect
        for y in range(MAP_W):
            for x in range(MAP_W):
                if not (x0 <= x < x0 + w and y0 <= y < y0 + h):
                    self.blocked.add((x, y))
        # Overlay that is not a wall and not a crate does not block, but it may not be on
        # the course either (tiberium slows a racer and is the wrong thing to drive on).
        self.dirty = set(c for c, v in self.overlay.items()
                         if v in eng.overlay and not eng.overlay[v]["wall"]
                         and not eng.overlay[v]["crate"])

    # -------------------------------------------------------------------- helpers --
    def cell_of(self, lx, ly):
        return (int(lx) // LEPTON, int(ly) // LEPTON)

    def clearance(self, lx, ly):
        """Distance in leptons from a point to the nearest blocked cell's square."""
        cx, cy = self.cell_of(lx, ly)
        best = 1 << 30
        for y in range(max(0, cy - 8), min(MAP_W, cy + 9)):
            for x in range(max(0, cx - 8), min(MAP_W, cx + 9)):
                if (x, y) in self.blocked:
                    ddx = max(x * LEPTON - lx, 0, lx - (x + 1) * LEPTON)
                    ddy = max(y * LEPTON - ly, 0, ly - (y + 1) * LEPTON)
                    best = min(best, math.hypot(ddx, ddy))
        return best

    def enterable(self, c):
        return c not in self.blocked

    def direction(self, i):
        """The direction of travel at point i in leptons (not normalised): the chord from
        the point before to the point after, round the lap on a circuit."""
        pts = self.rally["points"]
        n = len(pts)
        if self.rally["loop"]:
            a, b = pts[(i - 1) % n], pts[(i + 1) % n]
        else:
            a, b = pts[max(i - 1, 0)], pts[min(i + 1, n - 1)]
        return b[0] - a[0], b[1] - a[1]

    def lap_apart(self, a, b):
        """Course points between point a and point b: round the lap on a circuit."""
        d = abs(a - b)
        if self.rally["loop"]:
            d = min(d, len(self.rally["points"]) - d)
        return d

    # ---------------------------------------------------------------------- check --
    def check(self):
        P = self.problems
        r = self.rally
        pts = r["points"]
        if not r["name"]:
            P.append("[Rally] has no Name")
        basic = dict(self.secs.get("Basic", []))
        if basic.get("Name") != r["name"]:
            P.append("[Basic] Name=%r differs from [Rally] Name=%r" % (basic.get("Name"), r["name"]))
        if r["hostile"] != "BadGuy":
            P.append("[Rally] Hostile is %r, expected BadGuy" % r["hostile"])
        if not (2 <= len(pts) <= 512):
            P.append("[Rally] has %d points; 2..512 allowed" % len(pts))
        if self.rect != (1, 1, 62, 62):
            P.append("[MAP] rect is %r, expected the full 62x62 at 1,1" % (self.rect,))
        if any(k == "Version" for k, v in self.secs.get("MAP", [])):
            P.append("[MAP] carries a Version line; a 64x64 course must not")

        # 1. every point and its width.
        loop = r["loop"]
        length = 0.0
        min_hw = min((p[2] for p in pts), default=0)
        n = len(pts)
        for i, (x, y, hw) in enumerate(pts):
            if i:
                length += math.hypot(x - pts[i - 1][0], y - pts[i - 1][1]) / LEPTON
            c = self.cell_of(x, y)
            measured = self.clearance(x, y) - HW_MARGIN
            if abs(hw - measured) > 2:
                P.append("P%03d half width %d is not the measured %d" % (i, hw, measured))
            # the centre line on to the next point
            if i + 1 < n or loop:
                nx2, ny2 = pts[(i + 1) % n][0], pts[(i + 1) % n][1]
                for k in range(1, 16):
                    sc = self.cell_of(x + (nx2 - x) * k / 16.0, y + (ny2 - y) * k / 16.0)
                    if not self.enterable(sc) or sc in self.dirty:
                        P.append("the centre line from P%03d to P%03d crosses blocked cell %s"
                                 % (i, (i + 1) % n, sc))
                        break
            if not self.enterable(c) or c in self.dirty:
                P.append("P%03d cell %s is not clean driveable ground" % (i, c))
            if hw < MIN_HALF_WIDTH:
                P.append("P%03d half width %d is under %d: the track is pinched there"
                         % (i, hw, MIN_HALF_WIDTH))
            cx, cy = c
            reach = hw // LEPTON + 2
            for yy in range(cy - reach, cy + reach + 1):
                for xx in range(cx - reach, cx + reach + 1):
                    if math.hypot((xx + 0.5) * LEPTON - x, (yy + 0.5) * LEPTON - y) <= hw:
                        if not self.enterable((xx, yy)) or (xx, yy) in self.dirty:
                            P.append("P%03d: cell %s within the half width %d is blocked or dirty"
                                     % (i, (xx, yy), hw))
            # the cross-section along the normal
            ax, ay = self.direction(i)
            al = math.hypot(ax, ay) or 1.0
            nx, ny = -ay / al, ax / al
            for k in range(-32, 33):
                px, py = x + nx * hw * k / 32.0, y + ny * hw * k / 32.0
                cc = self.cell_of(px, py)
                if not self.enterable(cc) or cc in self.dirty:
                    P.append("P%03d: the cross-section touches blocked cell %s" % (i, cc))
                    break
        if loop and n >= 2:
            closing = math.hypot(pts[0][0] - pts[-1][0], pts[0][1] - pts[-1][1])
            length += closing / LEPTON
            if not (CLOSING_GAP[0] <= closing <= CLOSING_GAP[1]):
                P.append("the lap's last point is %d leptons from P000; a circuit closes in %d to %d"
                         % (closing, CLOSING_GAP[0], CLOSING_GAP[1]))
        self.length = length
        self.min_hw = min_hw
        span = LAP_LENGTH if loop else COURSE_LENGTH
        if not (span[0] <= length <= span[1]):
            P.append("the centre line is %.1f cells; %s is %d to %d"
                     % (length, "a lap" if loop else "a course", span[0], span[1]))
        if loop and r["branches"]:
            P.append("a circuit has a fork; forks belong to point-to-point courses only")
        for b in r["branches"]:
            for i, (x, y, hw) in enumerate(b["points"]):
                c = self.cell_of(x, y)
                if not self.enterable(c) or c in self.dirty:
                    P.append("branch point %d cell %s is not clean driveable ground" % (i, c))
                if self.clearance(x, y) < hw - 1:
                    P.append("branch point %d: half width %d exceeds its clearance %d"
                             % (i, hw, self.clearance(x, y)))
            if not (0 <= b["from"] < b["to"] < len(pts)):
                P.append("branch joins at points %d..%d, outside the course" % (b["from"], b["to"]))

        # 5. grid
        grid = r["grid"]
        if len(grid) != GRID_SLOTS:
            P.append("%d grid slots, expected %d" % (len(grid), GRID_SLOTS))
        if not (0 <= r["facing"] <= 255):
            P.append("GridFacing %d is not a DirType" % r["facing"])
        gcells = [self.cell_of(x, y) for x, y in grid]
        if len(set(gcells)) != len(gcells):
            P.append("two grid slots share a cell: %s" % gcells)
        for i, (x, y) in enumerate(grid):
            c = gcells[i]
            if not self.enterable(c) or c in self.dirty:
                P.append("Grid%d cell %s is not clean driveable ground" % (i, c))
            if self.clearance(x, y) < VEHICLE_RADIUS:
                P.append("Grid%d is only %d leptons from a blocked cell" % (i, self.clearance(x, y)))
            for j in range(i):
                d = math.hypot(x - grid[j][0], y - grid[j][1])
                if d < GRID_MIN_APART - 1:
                    P.append("Grid%d and Grid%d are only %d leptons apart" % (i, j, d))
        if loop and n >= 2:
            dx0, dy0 = self.direction(0)
            dl = math.hypot(dx0, dy0) or 1.0
            ux, uy = dx0 / dl, dy0 / dl
            want = int(round(math.degrees(math.atan2(ux, -uy)) / 360.0 * 256)) % 256
            if min((r["facing"] - want) % 256, (want - r["facing"]) % 256) > 1:
                P.append("GridFacing %d is not the direction of travel at P000 (%d)" % (r["facing"], want))
            for i, (x, y) in enumerate(grid):
                ahead = ((x - pts[0][0]) * ux + (y - pts[0][1]) * uy)
                if ahead > -VEHICLE_RADIUS:
                    P.append("Grid%d is not behind the start/finish line (%d leptons ahead of it)"
                             % (i, ahead))
                j = min(range(n), key=lambda k: (pts[k][0] - x) ** 2 + (pts[k][1] - y) ** 2)
                if not (n - GRID_TAIL <= j < n or j == 0):
                    P.append("Grid%d is nearest course point P%03d, not one of the lap's last %d"
                             % (i, j, GRID_TAIL))
        wp = dict((int(k), int(v)) for k, v in self.secs.get("Waypoints", []))
        for i in range(26):
            want = gcells[i][1] * MAP_W + gcells[i][0] if i < len(gcells) else -1
            if wp.get(i, -1) != want:
                P.append("waypoint %d is %s, expected %d" % (i, wp.get(i), want))

        # 2 + 3. the flood
        start = gcells[0] if gcells else None
        reach = set()
        if start and self.enterable(start):
            q = deque([start])
            reach.add(start)
            while q:
                x, y = q.popleft()
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nb = (x + dx, y + dy)
                        if (dx or dy) and nb not in reach and 0 <= nb[0] < MAP_W and 0 <= nb[1] < MAP_W \
                                and self.enterable(nb):
                            reach.add(nb)
                            q.append(nb)
        self.reach = reach
        # The track is everything the flood reaches. It must hold every course point and
        # nothing the walls are supposed to keep out. Everything outside the walls stands
        # at least two cells from the track, so a flood that gets next to a tree, a
        # building or a Tiberium cell has got out; so has one that wanders further from
        # every lane, grid slot and finish end than the run-off and the grid's back end
        # can account for.
        lanes = list(pts) + [p for b in r["branches"] for p in b["points"]]
        anchors = [(p[0], p[1]) for p in lanes] + [(g[0], g[1]) for g in r["grid"]]
        if r["finish"]:
            anchors += [(r["finish"][0], r["finish"][1]), (r["finish"][2], r["finish"][3])]
        outside = set(self.dirty)
        for t in self.trees:
            outside |= t[3]
        for s in self.structures:
            outside |= s[4] | s[5] | s[6]
        leaks = []
        for c in reach:
            if any((c[0] + dx, c[1] + dy) in outside for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                leaks.append(c)
        if leaks:
            P.append("the flood from the grid reaches %d cells beside scenery outside the walls "
                     "(first %s): a gap in the walls" % (len(leaks), sorted(leaks)[0]))
        far = []
        for c in reach:
            lx, ly = (c[0] + 0.5) * LEPTON, (c[1] + 0.5) * LEPTON
            d = min(math.hypot(lx - ax, ly - ay) for ax, ay in anchors)
            if d > LEAK_RADIUS * LEPTON:
                far.append(c)
        if far:
            P.append("the flood from the grid reaches %d cells more than %d cells from the "
                     "course (first %s): the walls have a gap" % (len(far), LEAK_RADIUS, far[0]))
        self.leak_margin = max(min(math.hypot((c[0] + 0.5) * LEPTON - ax, (c[1] + 0.5) * LEPTON - ay)
                                   for ax, ay in anchors) for c in reach) / LEPTON if reach else 0
        if r["finish"]:
            x1, y1, x2, y2 = r["finish"]
            fc = [self.cell_of(x1 + (x2 - x1) * k / 16.0, y1 + (y2 - y1) * k / 16.0) for k in range(17)]
            if not any(c in reach for c in fc):
                P.append("the flood from the grid never reaches the finish line")
            # 8. the finish crosses the last point (on a circuit P000, the start line)
            fi = 0 if loop else len(pts) - 1
            lx, ly = pts[fi][0], pts[fi][1]
            seg = math.hypot(x2 - x1, y2 - y1)
            off = abs((x2 - x1) * (y1 - ly) - (x1 - lx) * (y2 - y1)) / max(seg, 1.0)
            if off > 32:
                P.append("the finish line misses P%03d by %d leptons" % (fi, off))
            dx0, dy0 = self.direction(fi)
            dl = math.hypot(dx0, dy0) or 1.0
            if seg > 0 and abs((x2 - x1) * dx0 + (y2 - y1) * dy0) / (seg * dl) > 0.05:
                P.append("the finish line is not square to the direction of travel at P%03d" % fi)
            for ex, ey in ((x1, y1), (x2, y2)):
                c = self.cell_of(ex - (ex - lx) * 0.02, ey - (ey - ly) * 0.02)
                if c not in reach:
                    P.append("a finish line end %s is not on the track" % ((ex, ey),))
                el = math.hypot(ex - lx, ey - ly) or 1.0
                beyond = self.cell_of(ex + (ex - lx) / el * 32, ey + (ey - ly) / el * 32)
                if self.enterable(beyond):
                    P.append("the finish line stops short of the wall at %s" % ((ex, ey),))
            if loop:
                hw0 = pts[0][2]
                nx0, ny0 = -dy0 / dl, dx0 / dl
                want = (lx - nx0 * hw0, ly - ny0 * hw0, lx + nx0 * hw0, ly + ny0 * hw0)
                if max(abs(a - b) for a, b in zip((x1, y1, x2, y2), want)) > 3:
                    P.append("the start/finish line is not P000 plus and minus its half width, "
                             "left to right: %s, expected about %s"
                             % ((x1, y1, x2, y2), tuple(int(round(v)) for v in want)))
            for k in range(33):
                fx, fy = x1 + (x2 - x1) * k / 32.0, y1 + (y2 - y1) * k / 32.0
                j = min(range(n), key=lambda q: (pts[q][0] - fx) ** 2 + (pts[q][1] - fy) ** 2)
                if self.lap_apart(j, fi) > LANE_WINDOW:
                    P.append("the finish line passes over P%03d, another part of the course" % j)
                    break
        else:
            P.append("[Rally] has no Finish")
        for i, (x, y, hw) in enumerate(pts):
            if self.cell_of(x, y) not in reach:
                P.append("P%03d is not reachable from the grid" % i)
                break

        # 10. lanes far apart along the course keep apart on the map
        owner = {}
        for c in reach:
            cx, cy = (c[0] + 0.5) * LEPTON, (c[1] + 0.5) * LEPTON
            owner[c] = min(range(n), key=lambda q: (pts[q][0] - cx) ** 2 + (pts[q][1] - cy) ** 2)
        close = []
        for c, oc in owner.items():
            for dy in range(-(LANE_APART - 1), LANE_APART):
                for dx in range(-(LANE_APART - 1), LANE_APART):
                    d = (c[0] + dx, c[1] + dy)
                    if d in owner and d > c and self.lap_apart(owner[d], oc) > LANE_WINDOW:
                        close.append((c, d, oc, owner[d]))
        self.lane_pairs = close
        if close:
            c, d, a, b = close[0]
            P.append("%d pairs of track cells under %d cells apart belong to parts of the course "
                     "more than %d points apart (first %s at P%03d and %s at P%03d): lanes too close"
                     % (len(close), LANE_APART, LANE_WINDOW, c, a, d, b))

        # 6. crates
        if not (9 <= len(r["crates"]) <= 64):
            P.append("%d crate spots; a course has three or four rows of three or four" % len(r["crates"]))
        for i, (x, y) in enumerate(r["crates"]):
            c = self.cell_of(x, y)
            if c not in reach or c in self.dirty:
                P.append("C%02d at cell %s is not on the track" % (i, c))
            if self.clearance(x, y) < VEHICLE_RADIUS:
                P.append("C%02d is only %d leptons from a blocked cell" % (i, self.clearance(x, y)))

        # 4 + 7. structures
        hostile = r["hostile"]
        power = drain = 0
        self.defences = []
        occupied = {}
        for house, name, x, y, occ, ovl, bib in self.structures:
            b = self.eng.buildings[name]
            for c in occ | ovl | bib:
                if c in reach:
                    P.append("%s %s at %s: footprint cell %s is on the track" % (house, name, (x, y), c))
                    break
                if c in self.walls:
                    P.append("%s %s at %s: footprint cell %s is on a wall" % (house, name, (x, y), c))
                    break
            for c in occ:
                if self.land[c[1]][c[0]] not in ("LAND_CLEAR", "LAND_ROAD"):
                    P.append("%s %s at %s stands on %s" % (house, name, (x, y), self.land[c[1]][c[0]]))
                if c in occupied:
                    P.append("%s %s at %s overlaps %s at %s" % (house, name, (x, y), occupied[c], c))
                occupied[c] = name
                rx0, ry0, rw, rh = self.rect
                if not (rx0 <= c[0] < rx0 + rw and ry0 <= c[1] < ry0 + rh):
                    P.append("%s %s at %s is outside the map" % (house, name, (x, y)))
            if house == hostile:
                power += b["power"]
                drain += b["drain"]
            if house == hostile and b["range"] > 0 and name != "SAM":
                fx, fy = x * LEPTON + b["centre"][0], y * LEPTON + b["centre"][1]
                best = min(engine_distance(fx, fy, p[0], p[1]) for p in pts)
                gap = min(max(abs(a[0] - t[0]), abs(a[1] - t[1])) for a in occ
                          for t in reach if abs(a[0] - t[0]) <= 6 and abs(a[1] - t[1]) <= 6) \
                    if any(abs(a[0] - t[0]) <= 6 and abs(a[1] - t[1]) <= 6 for a in occ for t in reach) else 99
                self.defences.append((name, x, y, fx, fy, b["range"], best, gap - 1))
                if best > b["range"]:
                    P.append("%s at %s: nearest course point is %d leptons away, range %d"
                             % (name, (x, y), best, b["range"]))
                if not (DEFENCE_GAP[0] <= gap - 1 <= DEFENCE_GAP[1]):
                    P.append("%s at %s stands %d cells beyond the track edge, wanted %d..%d"
                             % (name, (x, y), gap - 1, DEFENCE_GAP[0], DEFENCE_GAP[1]))
        self.power, self.drain = power, drain
        if power < drain:
            P.append("the hostile house has %d power for a drain of %d" % (power, drain))
        if hostile and not any(s[0] == hostile and self.eng.buildings[s[1]]["range"] > 0
                               for s in self.structures):
            P.append("the hostile house owns no defence")
        for v in self.units + self.infantry:
            P.append("[UNITS]/[INFANTRY] entry %r: courses carry none" % v)

        # trees and overlay off the track, and only this theater's trees
        for name, x, y, occ, ovl in self.trees:
            if self.theater not in self.eng.terrain[name]["theaters"]:
                P.append("tree %s at %s does not exist in %s" % (name, (x, y), self.theater))
            if self.eng.terrain[name]["spawns_tiberium"]:
                P.append("tree %s at %s spawns Tiberium, which would grow onto the track" % (name, (x, y)))
            for c in occ | ovl:
                if c in reach or c in self.walls:
                    P.append("tree %s at %s reaches the track or a wall at %s" % (name, (x, y), c))
                    break
            for c in occ:
                if c in occupied:
                    P.append("tree %s at %s overlaps %s" % (name, (x, y), occupied[c]))
                occupied[c] = name
        for c, v in self.overlay.items():
            if c in occupied:
                P.append("overlay %s at %s is under %s" % (v, c, occupied[c]))

        # 9. houses
        for n in range(1, 9):
            h = dict(self.secs.get("Multi%d" % n, []))
            if not h:
                P.append("no [Multi%d] section" % n)
            elif h.get("Allies") != "Multi%d" % n:
                P.append("[Multi%d] Allies=%r, expected itself only" % (n, h.get("Allies")))
        for name in ("Neutral", "Special", "GoodGuy", "BadGuy"):
            if name not in self.secs:
                P.append("no [%s] section" % name)
        if dict(self.secs.get("Base", [])).get("Count") != "0":
            P.append("[Base] Count is not 0")
        th = dict(self.secs.get("MAP", [])).get("Theater")
        if not th or "Theater" in dict(self.secs.get("Basic", [])):
            P.append("Theater must be in [MAP] and only there")
        return P

    def summary(self):
        r = self.rally
        tib = sum(1 for v in self.overlay.values() if v.startswith("TI"))
        return ("%s %-16s %-9s %s points %3d  length %5.1f cells  min hw %.2f cells  track cells %4d  "
                "furthest track cell %.1f cells from the course  walls %4d  defences %2d  "
                "power %d/%d  crates %d  trees %d  tiberium %d  structures %d"
                % (self.scen, r["name"], self.theater, "circuit" if r["loop"] else "run", len(r["points"]), self.length,
                   self.min_hw / 256.0, len(self.reach), self.leak_margin, len(self.walls),
                   len(self.defences), self.power, self.drain, len(r["crates"]), len(self.trees),
                   tib, len(self.structures)))


# ===================================================================== the picture ==

GROUND = {"TEMPERATE": (74, 104, 52), "DESERT": (176, 140, 92), "WINTER": (206, 212, 220)}
TRACK = {"TEMPERATE": (150, 140, 110), "DESERT": (214, 190, 140), "WINTER": (150, 156, 168)}


def render(course, path, scale=12):
    from PIL import Image, ImageDraw
    th = course.theater
    head = 28
    img = Image.new("RGB", (MAP_W * scale, MAP_W * scale + head), (20, 20, 20))
    d = ImageDraw.Draw(img)
    ground = GROUND.get(th, (90, 90, 90))
    track = TRACK.get(th, (150, 150, 150))

    def box(c, col, inset=0):
        x, y = c
        d.rectangle([x * scale + inset, head + y * scale + inset,
                     (x + 1) * scale - 1 - inset, head + (y + 1) * scale - 1 - inset], fill=col)

    def pt(lx, ly):
        return (lx * scale / LEPTON, head + ly * scale / LEPTON)

    x0, y0, w, h = course.rect
    for y in range(MAP_W):
        for x in range(MAP_W):
            inside = x0 <= x < x0 + w and y0 <= y < y0 + h
            land = course.land[y][x]
            col = ground if inside else (40, 40, 40)
            if inside and land == "LAND_WATER":
                col = (40, 70, 120)
            elif inside and land == "LAND_ROCK":
                col = (100, 90, 80)
            elif inside and land == "LAND_ROAD":
                col = (120, 110, 90)
            box((x, y), col)
    for c in course.reach:
        box(c, track)
    for c, v in course.overlay.items():
        if v.startswith("TI"):
            box(c, (60, 220, 60), 1)
    for c in course.walls:
        box(c, (230, 220, 200) if th != "WINTER" else (90, 80, 70))
        box(c, (130, 115, 90) if th != "WINTER" else (60, 50, 45), 2)
    for name, x, y, occ, ovl in course.trees:
        for c in occ:
            box(c, (20, 70, 20) if th != "DESERT" else (90, 70, 40), 1)
    for house, name, x, y, occ, ovl, bib in course.structures:
        col = {"BadGuy": (200, 40, 40), "Neutral": (160, 160, 170)}.get(house, (200, 200, 60))
        for c in bib:
            box(c, (110, 100, 90))
        for c in ovl:
            box(c, tuple(int(v * 0.7) for v in col))
        for c in occ:
            box(c, col)
        d.text((x * scale + 1, head + y * scale), name, fill=(255, 255, 255))
    # range circles
    for name, x, y, fx, fy, rng, best, gap in course.defences:
        cx, cy = pt(fx, fy)
        rr = rng * scale / LEPTON
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], outline=(255, 90, 90))
    # centre line and branch
    pts = course.rally["points"]
    line = list(pts) + ([pts[0]] if course.rally["loop"] else [])
    d.line([pt(p[0], p[1]) for p in line], fill=(255, 255, 0), width=2)
    for b in course.rally["branches"]:
        d.line([pt(p[0], p[1]) for p in b["points"]], fill=(255, 170, 0), width=2)
    for i in range(0, len(pts), 25):
        px, py = pt(pts[i][0], pts[i][1])
        d.ellipse([px - 2, py - 2, px + 2, py + 2], fill=(255, 255, 255))
        d.text((px + 3, py - 10), str(i), fill=(255, 255, 255))
    # grid
    for i, (gx, gy) in enumerate(course.rally["grid"]):
        px, py = pt(gx, gy)
        d.ellipse([px - 4, py - 4, px + 4, py + 4], fill=(40, 120, 255), outline=(255, 255, 255))
        d.text((px - 3, py - 5), str(i), fill=(255, 255, 255))
    # finish, checkered
    if course.rally["finish"]:
        x1, y1, x2, y2 = course.rally["finish"]
        n = 8
        for k in range(n):
            a = pt(x1 + (x2 - x1) * k / n, y1 + (y2 - y1) * k / n)
            b = pt(x1 + (x2 - x1) * (k + 1) / n, y1 + (y2 - y1) * (k + 1) / n)
            d.line([a, b], fill=(255, 255, 255) if k % 2 == 0 else (0, 0, 0), width=4)
    for (cx, cy) in course.rally["crates"]:
        px, py = pt(cx, cy)
        d.rectangle([px - 3, py - 3, px + 3, py + 3], fill=(255, 150, 30), outline=(60, 30, 0))
    title = "%s  %s  (%s)  %s, %d points, %.0f cells" % (
        course.scen, course.rally["name"], th, "circuit" if course.rally["loop"] else "point to point",
        len(pts), course.length)
    d.text((6, 8), title, fill=(255, 255, 255))
    img.save(path)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("scenarios", nargs="+")
    ap.add_argument("--dir", default=MISSIONS, help="directory holding <SCEN>.INI and .BIN")
    ap.add_argument("--png", help="write <SCEN>.png top-down renders into this directory")
    a = ap.parse_args(argv)
    eng = Engine()
    failed = 0
    for scen in a.scenarios:
        c = Course(eng, a.dir, scen)
        problems = c.check()
        print(c.summary())
        for name, x, y, fx, fy, rng, best, gap in c.defences:
            print("    defence %-4s at (%2d,%2d)  range %4d  nearest point %4d  %d cell(s) beyond the edge"
                  % (name, x, y, rng, best, gap))
        if a.png:
            os.makedirs(a.png, exist_ok=True)
            render(c, os.path.join(a.png, scen + ".png"))
        if problems:
            failed += 1
            print("  FAILED, %d problem(s):" % len(problems))
            for p in problems[:40]:
                print("    - " + p)
            if len(problems) > 40:
                print("    ... and %d more" % (len(problems) - 40))
        else:
            print("  OK: every assertion holds")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
