#!/usr/bin/env python3
"""
make_course.py -- build the Rally course maps from a short description of each.

WHAT IT MAKES
  A Rally course is an ordinary 64x64 Tiberian Dawn multiplayer map (SCMnnEA.INI and an
  8192-byte .BIN, cell = y*64 + x, no [MAP] Version line) with a [Rally] section the
  brain reads: the centre line, the start grid, the finish line and the crate spots, all
  in leptons (256 per cell). Everything else on the map is the engine's own vocabulary:
  sandbag wall overlay along both edges of the track, defences owned by the course's
  hostile house outside the walls, and scenery (trees, Tiberium, civilian buildings, a
  hostile base) outside the track.

CIRCUITS AND POINT-TO-POINT COURSES
  A course is a CIRCUIT (the description says "loop": True; the [Rally] section says
  Loop=1) or a point-to-point run (no Loop key). All three courses here are circuits.
  On a circuit the start line is the finish line and a race is some number of laps of it:
    - the points go once round the lap: P000 is on the start/finish line, the rest
      follow the direction of travel, and the last one is the point before the line,
      about one cell short of P000 (P000 is not repeated);
    - Finish= is the start/finish line: across the track at P000, square to the direction
      of travel there, P000's half width either side of it (so it is the very segment
      the eyes draw as the start line, and stops a margin short of each wall face);
    - the eight grid slots stand behind the line on the lap's final straight, the front
      row three quarters of a cell behind it. The eyes draw the start line three
      quarters of a cell ahead of the front row, so on a circuit it lands on the finish
      line. The final straight must be long enough to hold the grid.
  On a point-to-point course the points run from the start line to the finish, Finish=
  crosses the last point and the grid's front row is a cell and a quarter behind P000.

HOW A COURSE IS DESCRIBED
  As a turtle drive: a start position and heading, then straights and arcs, each able to
  change the half width of the track as it goes. A circuit's drive starts ON the start
  line and must end where it began, on the start heading: its turns add up to one whole
  turn, and two of its straights, one running east or west and one running north or
  south, are FIT straights whose lengths are solved so the drive closes exactly (so a
  bend can be reshaped without redoing the arithmetic by hand). A point-to-point drive
  names its START and FINISH with marks. Defences, crate rows and Tiberium fields are
  placed relative to the course (so many cells along it from the start line, negative
  meaning before it on a circuit, on the left or the right, so far beyond the wall), so
  the description survives the path being reshaped; bases and villages are placed at map
  cells.

FROM DESCRIPTION TO MAP
  1. The centre line is sampled every 1/20 cell. A cell is TRACK when its centre lies
     within the local half width of a sample. That gives clean straight lanes and round
     bends. Lanes far apart along the lap are kept further apart than one shared wall:
     check_course.py refuses two of them with fewer than three cells (wall, ground,
     wall) between their track cells.
  2. A cell is WALL when it is not track and touches a track cell, diagonals included.
     So no path of any kind leaves the track without crossing a wall cell: the barrier
     is closed by construction. Walls are sandbags (SBAG): not crushable by tracks, and
     unlike concrete they do not stop the defences' shells in flight (bullet.cpp explodes
     a low shell on a "high" overlay), which would make every defence fire into its own
     wall.
  3. Objects are placed in order: defences, then buildings, then Tiberium, then trees.
     Each one is searched for in a small window around where the description wants it
     and takes the nearest spot where its whole footprint (occupied cells, overlap cells
     and the bib the engine lays under a bibbed building) is clear ground that nothing
     else claims and that stays off the track and its walls. A defence must also stand
     one to three cells beyond the track edge and have a course point within its
     weapon's range; one that cannot is an error, not a quiet omission.
  4. The [Rally] points are resampled along the centre line about one cell apart. Each
     point's half width is MEASURED, not copied from the description: the distance from
     the point to the nearest blocked cell's square, less a small margin, so it is the
     room a driver really has there.
  5. tools/rally/check_course.py then re-reads the written files and checks every claim
     against the engine source; a course that fails it is reported as failed.

THE TERRAIN
  Every cell of the .BIN is clear ground (template 255). The courses are flat on
  purpose: no template is needed that might lack a cartridge tile in the bake, and the
  brain's vehicle physics and the drawn ground cannot disagree about a slope.

A FORK (point-to-point only; no course has one now)
  A point-to-point description may add a second route ("branch") that leaves the main
  line at one mark and rejoins it further on. The [Rally] centre line follows the SHORT
  route, which the AI drivers take; the long route is listed as an extra the brain
  ignores (Branches=1, B0From, B0To, B0Points, B0Pnnn), and the short branch is kept
  under 8% of the points so a racer on the long route still passes the 90% the finish
  rule asks for.

THE WHOLE PIPELINE, from this file to a course the lobby offers
  python3 tools/rally/make_course.py                   # writes build-native/rally-courses,
                                                        # checks, renders art/rally/courses
  The generator writes to a build directory rather than straight into game/missions
  because the staging step refuses to copy a directory onto itself. Staging is the steps
  of tools/stage-skirmish-maps.sh --from, per course: copy the INI into game/missions
  and the bakery's extracted INI directory and the BIN into game/missions, convert the
  BIN with tools/bin_to_n64map.py, resolve it with n64_terrain.py and bake the pack with
  bake5.py. The script itself refuses a scenario whose INI git already tracks, which the
  three courses are, so for a course being reshaped the steps are run by hand. A
  course's BIN is all clear ground in a theater that does not change, so its .MAP and
  its pack stay byte for byte the same; only the INI moves.
  python3 tools/bake_map_previews.py                    # every map's lobby thumbnail,
                                                        # courses included
  python3 tools/rally/check_course.py SCM70EA SCM71EA SCM72EA   # the staged files, again

Run:  python3 tools/rally/make_course.py [--out DIR] [--png DIR] [--no-check] [SCM70EA ...]
"""
import argparse
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import check_course as cc  # noqa: E402  (the engine facts and the checker)

MAP_W = 64
RECT = (1, 1, 62, 62)          # the playable window: cells 1..62 both ways
LEPTON = 256
STEP = 0.05                    # centre-line sampling, cells
WALL = "SBAG"
HOSTILE = "BadGuy"
HW_MARGIN = 16                 # leptons kept back from the measured clearance
SHORT_BRANCH_MAX = 0.08        # the fork's short branch, as a share of all points
GRID_FRONT_RUN = 1.25          # cells from the start line back to the grid's front row
GRID_FRONT_LOOP = 0.75         # the same on a circuit, where the start line is the finish
GRID_RUNWAY = 1.5              # straight kept behind the grid's back row on a circuit

# Trees that exist in each theater, from tdata.cpp; the blossom trees are left out
# because they spawn Tiberium, which would grow up to the walls.
TREES = {
    "TEMPERATE": ["T01", "T02", "T03", "T05", "T06", "T07", "T08", "T10", "T11", "T12",
                  "T13", "T14", "T15", "T16", "T17"],
    "WINTER": ["T01", "T02", "T03", "T05", "T06", "T07", "T08", "T10", "T11", "T12",
               "T13", "T14", "T15", "T16", "T17"],
    "DESERT": ["T04", "T08", "T09", "T18"],
}
CLUMPS = {
    "TEMPERATE": ["TC01", "TC02", "TC03", "TC04", "TC05"],
    "WINTER": ["TC01", "TC02", "TC03", "TC04", "TC05"],
    "DESERT": ["ROCK1", "ROCK2", "ROCK3", "ROCK4", "ROCK5", "ROCK6", "ROCK7"],
}


# ======================================================================= geometry ==

class Drive(object):
    """A turtle that drives the centre line and remembers where it has been.

    x grows east and y grows south (the engine's axes), headings are degrees with 0 east
    and 90 south, so a positive arc turns right as seen on the map."""

    def __init__(self, x, y, heading, hw, fit=None):
        self.x, self.y, self.h, self.hw = float(x), float(y), float(heading), float(hw)
        self.s = 0.0
        self.samples = [(self.x, self.y, self.h, self.hw, 0.0)]
        self.marks = {}
        self.fit = fit or {}        # FIT straight name -> solved length
        self.fit_heading = {}       # FIT straight name -> its heading, degrees

    def _emit(self):
        self.samples.append((self.x, self.y, self.h, self.hw, self.s))

    def straight(self, length, hw=None):
        n = max(1, int(round(length / STEP)))
        hw0, hw1 = self.hw, (self.hw if hw is None else float(hw))
        dx, dy = math.cos(math.radians(self.h)), math.sin(math.radians(self.h))
        for i in range(1, n + 1):
            f = i / float(n)
            self.x += dx * length / n
            self.y += dy * length / n
            self.s += length / n
            self.hw = hw0 + (hw1 - hw0) * f
            self._emit()
        return self

    def arc(self, radius, degrees, hw=None):
        length = abs(math.radians(degrees)) * radius
        n = max(1, int(round(length / STEP)))
        hw0, hw1 = self.hw, (self.hw if hw is None else float(hw))
        side = 1.0 if degrees > 0 else -1.0
        cxp = self.x + radius * math.cos(math.radians(self.h + 90 * side))
        cyp = self.y + radius * math.sin(math.radians(self.h + 90 * side))
        a0 = math.degrees(math.atan2(self.y - cyp, self.x - cxp))
        h0 = self.h
        for i in range(1, n + 1):
            f = i / float(n)
            a = math.radians(a0 + degrees * f)
            self.x = cxp + radius * math.cos(a)
            self.y = cyp + radius * math.sin(a)
            self.h = h0 + degrees * f
            self.s += length / n
            self.hw = hw0 + (hw1 - hw0) * f
            self._emit()
        return self

    def mark(self, name):
        self.marks[name] = len(self.samples) - 1
        return self

    def run(self, steps):
        for st in steps:
            kind = st[0]
            if kind == "S":
                self.straight(*st[1:])
            elif kind == "F":
                # A FIT straight: ("F", name[, hw]). Its length is solved by close_loop.
                name = st[1]
                if name in self.fit_heading:
                    raise SystemExit("FIT straight %r appears twice" % name)
                self.fit_heading[name] = self.h
                length = self.fit.get(name, 0.0)
                if length > 0:
                    self.straight(length, *st[2:])
                elif len(st) > 2:
                    self.hw = float(st[2])
            elif kind == "A":
                self.arc(*st[1:])
            elif kind == "M":
                self.mark(st[1])
            else:
                raise SystemExit("unknown drive step %r" % (st,))
        return self


def close_loop(code, start, steps, min_fit=1.0):
    """The drive of a circuit, with its two FIT straights solved so it ends where it began.

    A straight only moves everything after it, so the end of the drive is the end with
    both FIT straights at length zero plus each FIT length times its own heading. Two
    straights that are not parallel make that two equations in two lengths."""
    x, y, h, hw = start
    turn = sum(st[2] for st in steps if st[0] == "A")
    if abs(abs(turn) - 360.0) > 1e-6:
        raise SystemExit("%s: a circuit's turns must add up to one whole turn; they add up "
                         "to %.3f degrees" % (code, turn))
    probe = Drive(x, y, h, hw).run(steps)
    names = sorted(probe.fit_heading)
    if len(names) != 2:
        raise SystemExit("%s: a circuit needs exactly two FIT straights, it has %d" % (code, len(names)))
    ex, ey = x - probe.x, y - probe.y
    (a, b) = names
    ua = (math.cos(math.radians(probe.fit_heading[a])), math.sin(math.radians(probe.fit_heading[a])))
    ub = (math.cos(math.radians(probe.fit_heading[b])), math.sin(math.radians(probe.fit_heading[b])))
    det = ua[0] * ub[1] - ua[1] * ub[0]
    if abs(det) < 0.5:
        raise SystemExit("%s: the two FIT straights are nearly parallel; one must run east or "
                         "west and the other north or south" % code)
    la = (ex * ub[1] - ey * ub[0]) / det
    lb = (ua[0] * ey - ua[1] * ex) / det
    for name, length in ((a, la), (b, lb)):
        if length < min_fit:
            raise SystemExit("%s: FIT straight %r would be %.2f cells; reshape the drive so it "
                             "is at least %.1f" % (code, name, length, min_fit))
    d = Drive(x, y, h, hw, fit={a: la, b: lb}).run(steps)
    miss = math.hypot(d.x - x, d.y - y)
    hmiss = abs(((d.h - h) + 180.0) % 360.0 - 180.0)
    if miss > 1e-6 or hmiss > 1e-6:
        raise SystemExit("%s: the circuit does not close: %.6f cells and %.6f degrees off"
                         % (code, miss, hmiss))
    return d, {a: la, b: lb}


def resample(samples, i0, i1, spacing=1.0):
    """Points about `spacing` cells apart from sample i0 to sample i1, both included."""
    s0, s1 = samples[i0][4], samples[i1][4]
    n = max(1, int(round((s1 - s0) / spacing)))
    out = []
    j = i0
    for k in range(n + 1):
        target = s0 + (s1 - s0) * k / float(n)
        while j < i1 and samples[j + 1][4] < target:
            j += 1
        a = samples[j]
        b = samples[min(j + 1, i1)]
        if b[4] > a[4]:
            f = (target - a[4]) / (b[4] - a[4])
        else:
            f = 0.0
        f = max(0.0, min(1.0, f))
        out.append((a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f,
                    a[2] + (b[2] - a[2]) * f, a[3] + (b[3] - a[3]) * f))
    return out


def chebyshev_field(cells, limit=8):
    """Chebyshev distance from every map cell to the nearest cell of `cells` (capped)."""
    dist = {}
    for y in range(MAP_W):
        for x in range(MAP_W):
            dist[(x, y)] = limit + 1
    for (x, y) in cells:
        for yy in range(max(0, y - limit), min(MAP_W, y + limit + 1)):
            for xx in range(max(0, x - limit), min(MAP_W, x + limit + 1)):
                d = max(abs(xx - x), abs(yy - y))
                if d < dist[(xx, yy)]:
                    dist[(xx, yy)] = d
    return dist


def in_rect(c, inset=0):
    x0, y0, w, h = RECT
    return x0 + inset <= c[0] < x0 + w - inset and y0 + inset <= c[1] < y0 + h - inset


# ====================================================================== the build ==

class Build(object):
    def __init__(self, eng, code, desc):
        self.eng = eng
        self.code = code
        self.d = desc
        self.theater = desc["theater"]
        self.problems = []
        self.claimed = {}        # cell -> what claims it (footprints of placed objects)
        self.structures = []     # (house, type, x, y)
        self.trees = []          # (type, x, y)
        self.tiberium = {}       # cell -> TIn
        self.rng = random.Random(desc.get("seed", 1))

    # --------------------------------------------------------------- the course --
    def lay_track(self):
        d = self.d
        x, y, h, hw = d["start"]
        self.loop = bool(d.get("loop"))
        if self.loop:
            # The drive starts on the start line and comes back to it.
            self.main, self.fit = close_loop(self.code, d["start"], d["drive"])
            self.main.marks["START"] = 0
            self.lap = self.main.samples[-1][4]
            if "branch" in d:
                raise SystemExit("%s: a circuit has no fork" % self.code)
        else:
            self.main = Drive(x, y, h, hw).run(d["drive"])
            for need in ("START", "FINISH"):
                if need not in self.main.marks:
                    raise SystemExit("%s: the drive has no %s mark" % (self.code, need))
        self.branch = None
        if "branch" in d:
            m = self.main.samples[self.main.marks[d["branch"]["from"]]]
            self.branch = Drive(m[0], m[1], m[2], m[3]).run(d["branch"]["drive"])
        lanes = list(self.main.samples)
        if self.branch:
            lanes += self.branch.samples
        self.lanes = lanes
        track = set()
        for (sx, sy, sh, shw, ss) in lanes:
            r = int(math.ceil(shw)) + 1
            cx, cy = int(math.floor(sx)), int(math.floor(sy))
            for yy in range(cy - r, cy + r + 1):
                for xx in range(cx - r, cx + r + 1):
                    if (xx + 0.5 - sx) ** 2 + (yy + 0.5 - sy) ** 2 <= shw * shw + 1e-9:
                        track.add((xx, yy))
        self.track = track
        walls = set()
        for (x, y) in track:
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (x + dx, y + dy)
                    if n not in track:
                        walls.add(n)
        self.walls = walls
        bad = [c for c in track | walls if not in_rect(c)]
        if bad:
            self.problems.append("the track or its walls leave the map at %d cells, first %s"
                                 % (len(bad), sorted(bad)[0]))
        self.near = chebyshev_field(track, 8)      # cells from the track
        for c in walls:
            self.claimed[c] = WALL

    def course_points(self):
        """The main line about one cell apart: from START to FINISH, or on a circuit once
        round from the start line to the point before it (the last resampled point is the
        start line again and is dropped)."""
        m = self.main
        if self.loop:
            return resample(m.samples, 0, len(m.samples) - 1, 1.0)[:-1]
        return resample(m.samples, m.marks["START"], m.marks["FINISH"], 1.0)

    def at(self, where):
        """(x, y, heading, hw) on the course: a mark name, or cells along it from START
        (on a circuit any number, taken round the lap, so -5 is five cells before the
        line)."""
        m = self.main
        if isinstance(where, str):
            s = m.samples[m.marks[where]]
            return s[0], s[1], s[2], s[3]
        s0 = m.samples[m.marks["START"]][4]
        if self.loop:
            where = where % self.lap
        target = s0 + where
        for s in m.samples:
            if s[4] >= target:
                return s[0], s[1], s[2], s[3]
        s = m.samples[-1]
        return s[0], s[1], s[2], s[3]

    # -------------------------------------------------------------- footprints --
    def footprint(self, kind, name, x, y):
        if kind == "building":
            occ, ovl, bib = self.eng.building_cells(name, x, y)
            return occ, occ | ovl | bib
        occ, ovl = self.eng.terrain_cells(name, x, y)
        return occ, occ | ovl

    def fits(self, kind, name, x, y, min_gap=2, max_gap=None):
        occ, allc = self.footprint(kind, name, x, y)
        for c in allc:
            if not in_rect(c) or c in self.track or c in self.claimed or c in self.tiberium:
                return False
            if self.near.get(c, 99) < min_gap:
                return False
        if max_gap is not None and min(self.near[c] for c in occ) > max_gap:
            return False
        return True

    def claim(self, kind, name, x, y, house=None):
        occ, allc = self.footprint(kind, name, x, y)
        for c in allc:
            self.claimed[c] = name
        if kind == "building":
            self.structures.append((house, name, x, y))
        else:
            self.trees.append((name, x, y))

    def search(self, kind, name, tx, ty, radius, ok):
        """The nearest top-left cell to (tx, ty) within `radius` that passes ok(x, y)."""
        best = None
        for y in range(int(ty) - radius, int(ty) + radius + 1):
            for x in range(int(tx) - radius, int(tx) + radius + 1):
                if ok(x, y):
                    dd = (x - tx) ** 2 + (y - ty) ** 2
                    if best is None or dd < best[0]:
                        best = (dd, x, y)
        return best

    def edge_point(self, where, side, beyond):
        """The point `beyond` cells past the wall, on `side` (+1 right, -1 left) of the
        course at `where`."""
        x, y, h, hw = self.at(where)
        nx, ny = math.cos(math.radians(h + 90 * side)), math.sin(math.radians(h + 90 * side))
        t = 0.0
        while (int(math.floor(x + nx * t)), int(math.floor(y + ny * t))) in self.track:
            t += 0.1
        t += 1.0 + beyond      # the wall cell, then the gap
        return x + nx * t, y + ny * t

    # ----------------------------------------------------------------- defences --
    def place_defences(self, pts):
        for spec in self.d.get("defences", []):
            name, where, side, gap = spec
            b = self.eng.buildings[name]
            tx, ty = self.edge_point(where, side, gap)
            w, h = b["size"]
            tx -= w / 2.0
            ty -= h / 2.0

            def ok(x, y, name=name, b=b):
                if not self.fits("building", name, x, y, min_gap=2, max_gap=4):
                    return False
                occ, _ = self.footprint("building", name, x, y)
                if min(self.near[c] for c in occ) < 2:
                    return False
                if b["range"]:
                    fx, fy = x * LEPTON + b["centre"][0], y * LEPTON + b["centre"][1]
                    best = min(cc.engine_distance(fx, fy, p[0] * LEPTON, p[1] * LEPTON) for p in pts)
                    if best > b["range"] - 24:
                        return False
                return True
            got = self.search("building", name, tx, ty, 4, ok)
            if not got:
                self.problems.append("no spot for %s at %r side %+d gap %d" % (name, where, side, gap))
                continue
            self.claim("building", name, got[1], got[2], HOSTILE)

    def place_buildings(self, key, house):
        for spec in self.d.get(key, []):
            name, tx, ty = spec[:3]
            radius = spec[3] if len(spec) > 3 else 3

            def ok(x, y, name=name):
                return self.fits("building", name, x, y, min_gap=2)
            got = self.search("building", name, tx, ty, radius, ok)
            if not got:
                self.problems.append("no spot for %s %s near %s" % (house, name, (tx, ty)))
                continue
            self.claim("building", name, got[1], got[2], house)

    # ----------------------------------------------------------------- scenery --
    def place_tiberium(self):
        for spec in self.d.get("tiberium", []):
            if spec[0] == "at":
                _, cx, cy, rx, ry = spec
            else:
                _, where, side, beyond, rx, ry = spec
                cx, cy = self.edge_point(where, side, beyond)
            for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
                for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                    c = (x, y)
                    e = ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2
                    # a ragged edge, so a field does not read as a drawn ellipse
                    edge = 1.0 + 0.35 * (((x * 73856093) ^ (y * 19349663)) % 100) / 100.0 - 0.17
                    if e > edge:
                        continue
                    if not in_rect(c) or c in self.track or c in self.claimed or self.near[c] < 2:
                        continue
                    self.tiberium[c] = "TI%d" % (1 + (x * 7 + y * 3) % 12)

    def place_trees(self):
        names = TREES[self.theater]
        clumps = CLUMPS[self.theater]
        for spec in self.d.get("groves", []):
            cx, cy, r, n = spec[:4]
            pool = spec[4] if len(spec) > 4 else names + clumps
            tries = 0
            placed = 0
            while placed < n and tries < n * 40:
                tries += 1
                a = self.rng.uniform(0, 2 * math.pi)
                rr = r * math.sqrt(self.rng.random())
                x, y = int(cx + rr * math.cos(a)), int(cy + rr * math.sin(a))
                name = self.rng.choice(pool)
                if self.fits("terrain", name, x, y, min_gap=2):
                    self.claim("terrain", name, x, y)
                    placed += 1
        # A scatter over the rest of the map, thinner, so open ground is never bare.
        scatter = self.d.get("scatter", 0)
        tries = 0
        placed = 0
        while placed < scatter and tries < scatter * 60:
            tries += 1
            x, y = self.rng.randint(1, 61), self.rng.randint(1, 61)
            name = self.rng.choice(names)
            if self.near.get((x, y), 0) < 3:
                continue
            if self.fits("terrain", name, x, y, min_gap=3):
                self.claim("terrain", name, x, y)
                placed += 1

    # -------------------------------------------------------------- the [Rally] --
    def measure_hw(self, x, y):
        """Leptons from (x, y) cells to the nearest blocked square, less the margin."""
        lx, ly = x * LEPTON, y * LEPTON
        cxc, cyc = int(math.floor(x)), int(math.floor(y))
        best = 1 << 30
        for yy in range(cyc - 8, cyc + 9):
            for xx in range(cxc - 8, cxc + 9):
                c = (xx, yy)
                if c in self.track:
                    continue
                ddx = max(xx * LEPTON - lx, 0, lx - (xx + 1) * LEPTON)
                ddy = max(yy * LEPTON - ly, 0, ly - (yy + 1) * LEPTON)
                best = min(best, math.hypot(ddx, ddy))
        return int(best) - HW_MARGIN

    def rally_section(self, pts):
        d = self.d
        L = []
        L.append("Name=%s" % d["name"])
        L.append("Hostile=%s" % HOSTILE)
        if self.loop:
            L.append("Loop=1")
        L.append("Points=%d" % len(pts))
        for i, (x, y, h, hw) in enumerate(pts):
            mhw = self.measure_hw(x, y)
            L.append("P%03d=%d,%d,%d" % (i, round(x * LEPTON), round(y * LEPTON), mhw))
        # The grid: two columns, four rows, 1.5 cells apart, behind the start line. Rows
        # sit at quarter-cell offsets so no slot lands on a cell boundary and every slot
        # has a cell of its own. On a circuit the front row is three quarters of a cell
        # behind the line, where the eyes' start line (drawn that far ahead of the front
        # row) lands on the start/finish line; the grid stands on the lap's final
        # straight, which must hold all of it.
        sx, sy, sh, shw = self.at("START")
        fx, fy = math.cos(math.radians(sh)), math.sin(math.radians(sh))
        nx, ny = -fy, fx
        facing = int(round((sh + 90.0) / 360.0 * 256)) % 256
        L.append("GridFacing=%d" % facing)
        front = GRID_FRONT_LOOP if self.loop else GRID_FRONT_RUN
        if self.loop:
            need = front + 1.5 * 3 + GRID_RUNWAY
            for s in self.main.samples:
                if s[4] >= self.lap - need and abs(((s[2] - sh) + 180.0) % 360.0 - 180.0) > 1e-6:
                    self.problems.append("the lap's final straight is shorter than the %.2f cells "
                                         "the grid needs" % need)
                    break
        self.grid = []
        for row in range(4):
            back = front + 1.5 * row
            for col in (-1, 1):
                gx = sx - fx * back + nx * 0.75 * col
                gy = sy - fy * back + ny * 0.75 * col
                self.grid.append((gx, gy))
        for i, (gx, gy) in enumerate(self.grid):
            L.append("Grid%d=%d,%d" % (i, round(gx * LEPTON), round(gy * LEPTON)))
        # The finish: across the track at the last point, from wall face to wall face. On
        # a circuit it is the start line too, at P000, and it is exactly the segment the
        # eyes draw as the start line (rly_draw_ground: centred on the grid, P000's half
        # width either side, from the left of the direction of travel to the right), so
        # the two checkered strips lie square on square. That stops a margin short of
        # each wall face, closer than any racer's centre can get.
        ex, ey, eh, ehw = pts[0] if self.loop else pts[-1]
        nxe, nye = -math.sin(math.radians(eh)), math.cos(math.radians(eh))
        ends = []
        for sgn in (-1, 1):
            if self.loop:
                t = self.measure_hw(ex, ey) / float(LEPTON)
            else:
                t = 0.0
                while (int(math.floor(ex + nxe * sgn * (t + 0.05))),
                       int(math.floor(ey + nye * sgn * (t + 0.05)))) in self.track:
                    t += 0.05
            ends.append((ex + nxe * sgn * t, ey + nye * sgn * t))
        self.finish = ends
        L.append("Finish=%d,%d,%d,%d" % (round(ends[0][0] * LEPTON), round(ends[0][1] * LEPTON),
                                         round(ends[1][0] * LEPTON), round(ends[1][1] * LEPTON)))
        # Crate rows, across the track.
        self.crates = []
        for where, n in d.get("crates", []):
            x, y, h, hw = self.at(where)
            # usable half width measured here, less room for the crate itself
            room = self.measure_hw(x, y) / float(LEPTON) - 0.55
            nxc, nyc = -math.sin(math.radians(h)), math.cos(math.radians(h))
            for k in range(n):
                off = -room + 2 * room * k / float(n - 1)
                self.crates.append((x + nxc * off, y + nyc * off))
        L.append("Crates=%d" % len(self.crates))
        for i, (x, y) in enumerate(self.crates):
            L.append("C%02d=%d,%d" % (i, round(x * LEPTON), round(y * LEPTON)))
        # The fork's long route, as an extra the brain may use.
        if self.branch:
            bp = resample(self.branch.samples, 0, len(self.branch.samples) - 1, 1.0)

            def nearest(px, py):
                return min(range(len(pts)), key=lambda i: (pts[i][0] - px) ** 2 + (pts[i][1] - py) ** 2)
            b_from = nearest(bp[0][0], bp[0][1])
            b_to = nearest(bp[-1][0], bp[-1][1])
            self.branch_span = (b_from, b_to, len(bp))
            L.append("Branches=1")
            L.append("B0From=%d" % b_from)
            L.append("B0To=%d" % b_to)
            L.append("B0Points=%d" % len(bp))
            for i, (x, y, h, hw) in enumerate(bp):
                L.append("B0P%03d=%d,%d,%d" % (i, round(x * LEPTON), round(y * LEPTON),
                                               self.measure_hw(x, y)))
        return L

    # -------------------------------------------------------------------- write --
    def write(self, outdir, pts, rally):
        def cell(c):
            return c[1] * MAP_W + c[0]

        def section(name, lines):
            return "[%s]\n%s\n" % (name, "\n".join(lines))

        def house(name, allies):
            return section(name, ["Allies=%s" % allies, "MaxBuilding=150", "MaxUnit=150",
                                  "Edge=North", "Credits=0"])
        out = []
        out.append("; %s, a Rally course: written by tools/rally/make_course.py.\n"
                   "; Cells are y*64 + x. The [Rally] section is read by the brain's Rally\n"
                   "; module; every position in it is in leptons, 256 to a cell.\n" % self.d["name"])
        out.append(section("Basic", [
            "Name=%s" % self.d["name"],
            "Player=Multi1",
            "BuildLevel=98",
            "Theme=No theme",
            "Percent=0",
            "CarryOverCap=-1",
            "CarryOverMoney=0",
            "Intro=x", "Brief=x", "Action=x", "Win=x", "Lose=x",
        ]))
        out.append(section("MAP", ["Theater=%s" % self.theater, "X=%d" % RECT[0], "Y=%d" % RECT[1],
                                   "Width=%d" % RECT[2], "Height=%d" % RECT[3]]))
        out.append(section("Rally", rally))
        wp = {}
        for i, (gx, gy) in enumerate(self.grid):
            wp[i] = cell((int(math.floor(gx)), int(math.floor(gy))))
        wp[26] = wp[0]
        out.append(section("Waypoints", ["%d=%d" % (i, wp.get(i, -1)) for i in range(31, -1, -1)]))
        # The hostile house does not shoot the civilian scenery; racers are nobody's ally.
        out.append(house("Neutral", "Neutral,BadGuy"))
        out.append(house("Special", "Special"))
        out.append(house("GoodGuy", "GoodGuy"))
        out.append(house("BadGuy", "BadGuy,Neutral"))
        for n in range(1, 9):
            out.append(house("Multi%d" % n, "Multi%d" % n))
        out.append(section("Base", ["Count=0"]))
        out.append(section("TERRAIN", ["%d=%s,None" % (cell((x, y)), name)
                                       for name, x, y in sorted(self.trees, key=lambda t: cell(t[1:]))]))
        overlay = dict((c, WALL) for c in self.walls)
        overlay.update(self.tiberium)
        out.append(section("OVERLAY", ["%d=%s" % (cell(c), v)
                                       for c, v in sorted(overlay.items(), key=lambda kv: cell(kv[0]))]))
        out.append(section("SMUDGE", []))
        out.append(section("INFANTRY", []))
        out.append(section("UNITS", []))
        out.append(section("STRUCTURES", ["%03d=%s,%s,256,%d,0,None" % (i, h, n, cell((x, y)))
                                          for i, (h, n, x, y) in enumerate(self.structures)]))
        text = "\n".join(out)
        with open(os.path.join(outdir, self.code + ".INI"), "w") as fh:
            fh.write(text)
        with open(os.path.join(outdir, self.code + ".BIN"), "wb") as fh:
            fh.write(bytes([0xFF, 0x00]) * (MAP_W * MAP_W))

    # ---------------------------------------------------------------------- all --
    def make(self, outdir):
        self.lay_track()
        pts = self.course_points()
        if len(pts) > 512:
            self.problems.append("%d points; the brain takes at most 512" % len(pts))
        self.place_defences(pts)
        self.place_buildings("base", HOSTILE)
        self.place_buildings("village", "Neutral")
        self.place_tiberium()
        self.place_trees()
        rally = self.rally_section(pts)
        if self.branch:
            b_from, b_to, k = self.branch_span
            share = (b_to - b_from) / float(len(pts))
            if share > SHORT_BRANCH_MAX:
                self.problems.append("the fork's short branch is %.1f%% of the course; a racer on "
                                     "the long route could not reach 90%% of the points"
                                     % (100 * share))
        self.write(outdir, pts, rally)
        return pts


# ===================================================================== the courses ==
#
# Drive steps: ("S", length[, hw]) straight, ("A", radius, degrees[, hw]) arc (positive
# turns right on the map), ("F", name[, hw]) a circuit's FIT straight (its length is
# solved so the lap closes; a circuit has two, one running east or west and one north or
# south), ("M", name) mark. hw is the half width in cells the step ends at. Placement
# "where" is a mark name or cells along the course from the start line (negative is
# before it on a circuit); side +1 is the right of the direction of travel, -1 the left.

COURSES = {}

COURSES["SCM70EA"] = {
    # The gentle first course: a lap round the outside of the map, anticlockwise. The
    # start/finish straight runs east along the south edge between Tiberium fields, a
    # chicane jinks up the east side between two guard towers, S-bends wind west along
    # the north edge past the village, and the long run down the west side brings the
    # lap home. The Nod base stands in the middle of the infield.
    "name": "Tiberium Run",
    "theater": "TEMPERATE",
    "seed": 70,
    "loop": True,
    "start": (24.0, 57.0, 0.0, 3.0),
    "drive": [
        # The four corners are a radius of six: tight enough that every vehicle's computer
        # driver drifts them (a drift is worth it when the course turns about 65 to 90
        # degrees within a second of driving, depending on the vehicle), where a radius
        # of eight left the quickest-turning vehicles nothing to drift on this course.
        ("S", 24.0),                                    # the start/finish straight
        ("A", 6.0, -90.0),                              # up the east side
        ("S", 4.0),
        # The chicane: two cells across and back (5 * 2 * (1 - cos 36.87) = 2), so its
        # narrow middle lies on whole cells and keeps all four of its cells' width.
        ("A", 5.0, -36.87, 2.5), ("A", 5.0, 36.87, 2.25),
        ("S", 3.0),
        ("A", 5.0, 36.87, 2.5), ("A", 5.0, -36.87, 3.0),
        ("S", 16.0),
        ("A", 6.0, -90.0),                              # west along the north edge
        ("S", 2.0),
        ("A", 5.0, -45.0), ("A", 5.0, 90.0), ("A", 5.0, -90.0), ("A", 5.0, 90.0),
        ("A", 5.0, -45.0),                              # the S-bends
        ("F", "north"),
        ("A", 6.0, -90.0),                              # down the west side
        ("F", "west"),
        ("A", 6.0, -90.0),                              # onto the start/finish straight
        ("S", 12.0),
    ],
    "crates": [(14.0, 4), (66.0, 3), (106.0, 3), (146.0, 4)],
    "defences": [
        ("GUN", 16.0, 1, 1),
        ("GTWR", 46.5, 1, 1), ("GTWR", 48.5, -1, 1),    # either side of the chicane
        ("GUN", 98.0, -1, 2),
        ("GUN", 150.0, -1, 1),
    ],
    "base": [("NUKE", 26, 30), ("NUKE", 26, 35), ("HAND", 31, 32)],
    "village": [("V01", 21, 18), ("V03", 26, 17), ("V05", 31, 18), ("V08", 36, 17),
                ("V11", 24, 22), ("V09", 39, 21)],
    "tiberium": [("course", 8.0, 1, 1.5, 6, 1.5), ("course", 2.0, -1, 2, 5, 1.5),
                 ("at", 42.0, 40.0, 4, 3), ("at", 18.0, 44.0, 3, 2),
                 ("at", 58.0, 58.0, 2, 2), ("at", 4.0, 34.0, 1.5, 4)],
    "groves": [(18, 30, 3, 5), (44, 28, 3, 5), (36, 46, 3, 4), (59, 4, 3, 3), (4, 4, 3, 3),
               (60, 60, 2, 2), (59, 30, 2, 3)],
    "scatter": 28,
}

COURSES["SCM71EA"] = {
    # Clockwise. The start/finish straight runs east along the north edge under a
    # turret. Down the east side, then west through the middle of a Nod base (the
    # gauntlet, narrowing between its turrets), south past its outworks, west along the
    # south edge under an obelisk, and up the west side through S-bends. One obelisk
    # only, standing back from the middle of the straight rather than in the corner: a
    # racer passes it every lap, and closer, or with a second one, it destroyed a racer
    # on most laps.
    "name": "Nod Gauntlet",
    "theater": "DESERT",
    "seed": 71,
    "loop": True,
    "start": (22.0, 8.0, 0.0, 3.0),
    "drive": [
        ("S", 27.0),                                    # under the turret
        ("A", 6.0, 90.0),                               # down the east side
        ("S", 12.0),
        ("A", 5.0, 90.0),                               # west, into the base
        ("S", 2.0), ("S", 2.0, 2.5), ("S", 9.0), ("S", 2.0, 3.0),    # the gauntlet
        ("A", 5.0, -90.0),                              # south past the outworks
        ("S", 16.0),
        ("A", 5.0, 90.0),                               # the obelisk straight
        ("F", "home"),
        ("A", 6.0, 90.0),                               # up the west side
        ("S", 3.0),
        ("A", 5.0, 40.0), ("A", 5.0, -80.0), ("A", 5.0, 80.0), ("A", 5.0, -80.0),
        ("A", 5.0, 40.0),                               # the S-bends
        ("F", "west"),
        ("A", 6.0, 90.0),                               # onto the start/finish straight
        ("S", 9.0),
    ],
    "crates": [(18.0, 4), (48.0, 3), (88.0, 3), (140.0, 3)],
    "defences": [
        ("GUN", 12.0, -1, 1),                           # the turret over the straight
        ("GUN", 61.0, 1, 1), ("GUN", 66.0, -1, 1),      # the gauntlet
        ("GUN", 90.0, -1, 1),                           # the outworks
        ("OBLI", 108.0, 1, 2),                          # the obelisk straight
    ],
    "base": [
        ("AFLD", 34, 23), ("PROC", 39, 22), ("HAND", 45, 21),
        ("NUK2", 15, 15), ("NUK2", 18, 15), ("NUK2", 15, 20), ("NUKE", 18, 20),
        ("SILO", 22, 16), ("SILO", 22, 19), ("SAM", 26, 16), ("SAM", 29, 22),
        ("NUKE", 37, 38), ("SILO", 41, 38), ("SAM", 45, 38),
    ],
    "village": [("V20", 57, 44), ("V24", 57, 52), ("V27", 56, 48)],
    "tiberium": [("at", 20.0, 36.0, 3, 3), ("at", 46.0, 50.0, 4, 2), ("at", 4.0, 4.0, 2.5, 2.5),
                 ("at", 59.0, 30.0, 1.5, 4)],
    "groves": [(22, 46, 3, 3), (4, 30, 2, 3), (59, 4, 3, 4), (40, 58, 3, 2), (59, 59, 2, 2),
               (36, 48, 2, 2), (24, 28, 2, 2)],
    "scatter": 16,
}

COURSES["SCM72EA"] = {
    # Clockwise. Two long sweeping arcs round one centre, joined by tight U-turns. The
    # start/finish straight runs north up the west edge into the outer sweep, a single
    # bend to the north-east; a U-turn there turns into the inner sweep, which runs back
    # inside it and narrows to a pass between two advanced guard towers; a U-turn in the
    # south-west corner brings the lap home. The first bend after the start is the long
    # sweep, not a U-turn, so the field does not pile into a hairpin off the grid. A
    # frozen village and a GDI outpost fill the middle.
    "name": "Frostbite Pass",
    "theater": "WINTER",
    "seed": 72,
    "loop": True,
    # The sweeps share a centre (42, 42) and lie 12 cells apart, the U-turns' diameter.
    "start": (6.0, 44.0, -90.0, 3.0),
    "drive": [
        ("S", 2.0),
        ("A", 36.0, 90.0),                              # the long sweep to the north-east
        ("S", 10.0),
        ("A", 6.0, 180.0),                              # U-turn
        ("F", "top"),
        ("A", 24.0, -10.0, 2.5), ("A", 24.0, -14.0),    # the pass, narrowing between towers
        ("A", 24.0, -8.0, 3.0), ("A", 24.0, -58.0),     # the sweep back
        ("F", "down"),
        ("A", 6.0, 180.0),                              # U-turn
        ("S", 8.0),                                     # the start/finish straight
    ],
    "crates": [(30.0, 4), (64.0, 3), (125.0, 4)],
    "defences": [
        ("ATWR", 20.0, -1, 1), ("ATWR", 45.0, -1, 1),   # outside the long sweep
        ("ATWR", 103.0, 1, 1), ("ATWR", 107.0, -1, 1),  # the pass
        ("ATWR", 128.0, -1, 1),
        ("ATWR", 150.0, 1, 1),
    ],
    "base": [("PYLE", 4, 4), ("HQ", 8, 3), ("NUKE", 4, 9), ("NUKE", 12, 4), ("NUKE", 4, 14),
             ("WEAP", 46, 46), ("NUKE", 52, 48), ("SILO", 52, 53)],
    "village": [("V01", 30, 36), ("V02", 34, 33), ("V04", 32, 41), ("V06", 37, 38), ("V10", 38, 30)],
    "tiberium": [("at", 56.0, 34.0, 3, 2), ("at", 28.0, 52.0, 3, 2)],
    "groves": [(26, 26, 3, 6), (44, 24, 3, 6), (24, 44, 3, 6), (42, 54, 3, 4), (58, 22, 2, 3),
               (58, 58, 2, 3), (14, 58, 2, 2)],
    "scatter": 30,
}


def build(eng, code, outdir):
    b = Build(eng, code, COURSES[code])
    pts = b.make(outdir)
    return b, pts


def main(argv=None):
    ap = argparse.ArgumentParser(description="Build the Rally course maps.")
    ap.add_argument("codes", nargs="*")
    ap.add_argument("--out", default=os.path.join(ROOT, "build-native", "rally-courses"))
    ap.add_argument("--png", default=os.path.join(ROOT, "art", "rally", "courses"))
    ap.add_argument("--no-check", action="store_true")
    a = ap.parse_args(argv)
    codes = a.codes or sorted(COURSES)
    eng = cc.Engine()
    os.makedirs(a.out, exist_ok=True)
    failed = 0
    for code in codes:
        b, pts = build(eng, code, a.out)
        length = sum(math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1])
                     for i in range(1, len(pts)))
        if b.loop:
            length += math.hypot(pts[0][0] - pts[-1][0], pts[0][1] - pts[-1][1])
        print("%s %s: %s, %d points, %.1f cells, %d track cells, %d wall cells, %d structures, "
              "%d trees, %d tiberium%s" % (
                  code, b.d["name"], "a circuit" if b.loop else "point to point", len(pts), length,
                  len(b.track), len(b.walls), len(b.structures), len(b.trees), len(b.tiberium),
                  "" if not b.loop else ", FIT straights " + ", ".join(
                      "%s %.2f" % kv for kv in sorted(b.fit.items()))))
        for p in b.problems:
            print("  BUILD PROBLEM: " + p)
        if b.problems:
            failed += 1
        if not a.no_check:
            failed += cc.main(["--dir", a.out, "--png", a.png, code])
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
