#!/usr/bin/env python3
"""G457's two halves: the scenes it stages and the claims it checks.

    python3 gate_churchroof.py maps DIR    write the test missions and their scripts
    python3 gate_churchroof.py check DIR   read the runs' logs and shots in DIR and print
                                           "1 message" or "0 why"

WHAT IT HOLDS. The remastered church (V01) has a damaged stage cut on purpose: the spire
snaps off, a ragged hole opens in the south roof slope over the back of the nave with the
roof piece hanging into it on a hinge, a fire burns in the hole, the wall round the back
window and a strip of the eave are blown out onto the ground, the back gable's apex falls,
and the roof round the hole is charred. At the game camera the char, the fire and the
smoke make a big dark patch of the roof, which can be taken for the roof being gone. This
gate holds that the roof stays drawn round the designed hole, in every file that serves
the church: the temperate one and the snow one both games share.

THREE SCENES, one per file the game picks: Tiberian Dawn temperate (ebld/V01.ebld; a copy
of SCM01EA), Tiberian Dawn WINTER (ebld/V01_WIN.ebld; a copy of SCM72EA) and Red Alert's
snow (ra3d/ebld/V01_WIN.ebld; Allied 4, SCG04EA, kept for its snow map and emptied of its
own units, buildings and triggers). Each holds the church, owned by Neutral, and two of the
player's tanks five cells south of it.

THE SCRIPT shoots the church whole from close, and the same frame with the buildings not
drawn (bldshow 0: the bare ground under the same camera), and lists every triangle of the
whole model as that frame projects it (ebldtris); the tanks force-fire on a cell it blocks
until it breaks (tickstr: its strength at half), then are stopped; the damaged stage is
shot one and three seconds later, each with its bare twin of the same moment (the fire and
smoke in front of the church are particles, drawn in both); then the church is killed on
that cell (hurtcell) and the wreck is shot two seconds later, once the explosion's fire
has died down: the measure's negative control. ebldump after every shot, ebldbodies at the
damaged stage, and under Red Alert the ore on the map (raore).

THE ROOF is found by geometry, not by colour: the model's triangles from ebldtris, drawn
into a depth buffer at the shot's own projection, and the pixels where the nearest
triangle is one of the roof's shingled tops (the 20 triangles whose normal stands at the
roof's 31 degrees, |ny| within ROOF_NY, centred between eave and ridge, ROOF_Y in model
units). The walls, the tower, the spire, the door hood, the gutters and the church's cast
shadow on the ground are none of those, and the snow-covered roof of the winter file is
the same triangles as the temperate one's.

THE CLAIMS, per scene:
  (1) the file: EBLDART names the file the theater picks (V01, V01_WIN, V01_WIN) and no
      file failed to read back; under Red Alert no ore lies under the church or round it;
  (2) the stages: INTACT, DAMAGED at the break (EBLD|damaged with 4 pieces), WRECKED at the
      kill; the tickstr reached half strength;
  (3) the designed cut: three seconds into the damaged stage the loose pieces are exactly
      spire_top, roof_edge, wall_chunk and gable_top besides the hit chips (the roof round
      the hole, the `base`, never comes loose), and `roof_in` hangs bent on its hinge
      (EBLDMOVE kind=bend, turned 30 degrees or more);
  (4) the roof mask is the drawn roof: exactly ROOF_TRIS roof triangles, at least MIN_ROOF
      pixels, and at least ALIGN of them differ from the bare twin in the intact shot (the
      projection lands on the church the frame drew, not beside it);
  (5) THE ROOF READS AS ROOF: of the roof's pixels bright enough in the intact shot to be
      judged (SEEN), at least ROOF_KEEP in both damaged shots are drawn (they differ from
      the same moment's bare twin), not fire, and at least LIT of their own intact
      brightness (the hole, the fire and the darkest char are what is left);
  (6) the floor tells a gone roof from a holed one: on the settled wreck, whose roof is
      gone, that share is below ROOF_KEEP itself.
NOT COVERED: a fault that drops only the roof's triangles and shows something as bright
behind them would still read as roof; the cut's own pieces (claim 3) are what rule it out.
Smoke drawn in front of a roof pixel counts for it when it is bright enough; the fire does
not. Classic and the N64 look draw the cartridge's chapel, which has no damaged stage of
its own (G308 claim 7 holds that they read none of this).
"""
import json
import os
import re
import sys

# measured on the committed files, the same on two runs: the share that reads as roof at 1
# and 3 seconds into the damaged stage 0.61 and 0.46 temperate, 0.91 and 0.85 winter, 0.88
# and 0.82 Red Alert's snow; on the settled wreck 0.26, 0.03 and 0.07. The temperate
# margin is the narrow one: there the charred, fire-lit shingles and the wreck's scorched
# ground are close in colour.
ROOF_KEEP = 0.35
LIT = 0.5
SEEN = 16           # a roof pixel this dark in the intact shot (the north slope in its own
                    # shade on the temperate map) cannot be judged, and is left out
ROOF_NY = (0.845, 0.870)   # cos 31 degrees is 0.857; the nearest other slopes are 0.83, 0.873
ROOF_Y = (430.0, 760.0)    # the shingled tops' centres (449 to 701), model units
ROOF_TRIS = 20
ALIGN = 0.97
DRAWN = 8           # drawn: a shot against its bare twin, any channel
MIN_ROOF = 4000     # roof pixels the intact shot must hold for the measure to mean anything
LOOSE = {"spire_top", "roof_edge", "wall_chunk", "gable_top"}
# the damaged stage's moments after the break, and the wreck's after the kill (ticks)
HELD = (15, 45)
WRECK = 60
TAIL = ["", "[TERRAIN]", "", "[SMUDGE]", "", "[Triggers]", "", "[CellTriggers]", "",
        "[Teams]", "", "[Waypoints]", "", "[Base]", "Count=0", ""]
# (scene, game, source mission, the scenario name it is written as, its pack, the file
# the game should pick)
SCENES = [("tem", "td", "SCM01EA", "SCG78EA", "SCG01EA.pack", "V01"),
          ("win", "td", "SCM72EA", "SCG79EA", "SCM72EA.pack", "V01_WIN"),
          ("ras", "ra", "SCG04EA", "SCG04EA", "ra3d/3d/SCG04EA.pack", "V01_WIN")]


def td_mission(folder, src, name):
    b = open(os.path.join("missions", src + ".BIN"), "rb").read()
    ini = open(os.path.join("missions", src + ".INI"), encoding="latin-1").read()
    mapsec = re.search(r"\[MAP\].*?(?=\n\[)", ini, re.S).group(0).replace("\r", "").strip()
    mx, my = int(re.search(r"\bX=(\d+)", mapsec).group(1)), int(re.search(r"\bY=(\d+)", mapsec).group(1))
    mw = int(re.search(r"\bWidth=(\d+)", mapsec).group(1))
    mh = int(re.search(r"\bHeight=(\d+)", mapsec).group(1))
    clear = lambda x, y: (mx + 1 <= x < mx + mw - 1 and my + 1 <= y < my + mh - 1
                          and b[(y * 64 + x) * 2] in (0, 0xFF))
    # the clear plot nearest the map's middle, with room round it and for the tanks
    cx, cy = mx + mw // 2, my + mh // 2
    best = None
    for y in range(my + 3, my + mh - 9):
        for x in range(mx + 3, mx + mw - 6):
            if all(clear(xx, yy) for xx in range(x - 3, x + 5) for yy in range(y - 3, y + 8)):
                d = (x - cx) ** 2 + (y - cy) ** 2
                if best is None or d < best[0]:
                    best = (d, x, y)
    if best is None:
        sys.exit("gate_churchroof: no room on %s" % src)
    _, x, y = best
    cell = lambda a, c: c * 64 + a
    st = ["000=Neutral,V01,256,%d,0,None" % cell(x, y)]
    un = ["%03d=GoodGuy,MTNK,256,%d,0,Guard,None" % (i, cell(tx, y + 5))
          for i, tx in enumerate((x, x + 1))]
    text = ["[Basic]", "Player=GoodGuy", "Action=x", "Lose=x", "Win=x", "Brief=x",
            "Intro=x", "BuildLevel=98", "Theme=No theme", "Percent=0", "CarryOverCap=-1",
            "CarryOverMoney=0", "", mapsec, "",
            "[GoodGuy]", "Allies=GoodGuy,Neutral", "MaxBuilding=150", "MaxUnit=150",
            "Credits=0", "Edge=South", "", "[Neutral]", "Allies=Neutral,GoodGuy",
            "MaxBuilding=150", "MaxUnit=150", "Credits=0", "Edge=North", "",
            "[BadGuy]", "Allies=BadGuy", "MaxBuilding=150", "MaxUnit=150", "Credits=0",
            "Edge=North", "", "[STRUCTURES]"] + st + ["", "[UNITS]"] + un + \
        ["", "[INFANTRY]", "", "[OVERLAY]"] + TAIL
    with open(os.path.join(folder, name + ".INI"), "w", encoding="latin-1", newline="") as f:
        f.write("\r\n".join(text))
    with open(os.path.join(folder, name + ".BIN"), "wb") as f:
        f.write(b)
    # (the church's footprint is 2 x 2 and it blocks the south row, List0011, in both games)
    return dict(x=x, y=y, aim=[x + 1, y + 1], tank="MTNK")


def ra_mission(folder, src, name):
    """Allied 4 kept for its snow map, emptied of everything that moves or fights."""
    ini = open(os.path.join("ra3d", "missions", src + ".INI"), "rb").read().decode("latin-1")
    # (a plot the map leaves clear, with no ore under it or round it: claim 1 holds that)
    x, y = 83, 56
    cell = lambda a, c: c * 128 + a
    rows = {"UNITS": ["%d=Greece,2TNK,256,%d,0,Guard,None" % (i, cell(tx, y + 5))
                      for i, tx in enumerate((x, x + 1))],
            "STRUCTURES": ["0=Neutral,V01,256,%d,0,None,0,0" % cell(x, y)],
            "INFANTRY": [], "Trigs": [], "TeamTypes": [], "CellTriggers": [],
            "Base": ["Player=BadGuy", "Count=0"]}
    out_lines, sec = [], None
    for l in ini.replace("\r", "").split("\n"):
        m = re.match(r"\[(.+)\]$", l)
        if m:
            sec = m.group(1)
            out_lines.append(l)
            out_lines += rows.get(sec, [])
        elif sec not in rows:
            out_lines.append(l)
    with open(os.path.join(folder, name + ".INI"), "wb") as f:
        f.write("\r\n".join(out_lines).encode("latin-1"))
    return dict(x=x, y=y, aim=[x + 1, y + 1], tank="2TNK")


def maps(out):
    os.makedirs(out, exist_ok=True)
    layout = {}
    for scene, game, src, name, pack, want in SCENES:
        folder = os.path.join(out, scene)
        os.makedirs(folder, exist_ok=True)
        d = (td_mission if game == "td" else ra_mission)(folder, src, name)
        d.update(game=game, name=name, pack=pack, want=want)
        layout[scene] = d
        shot = lambda tag: "shot %s/%s_%s.png" % (out, scene, tag)
        twin = lambda tag: [shot(tag), "bldshow 0", shot(tag + "_bare"), "bldshow 1"]
        look = "cam %.1f %.1f" % (d["x"] + 1.0, d["y"] + 1.3)
        s = ["tick 3", "zoom max", "dist 900", look, "tick 1"] + twin("intact")
        s += ["ebldtris V01 0", "echo g457 intact", "ebldump"] + (["raore"] if game == "ra" else [])
        s += ["cam %.1f %.1f" % (d["x"] + 1.0, d["y"] + 4.0), "tick 1"]
        for n in (0, 1):
            s += ["lclickobj %s %d" % (d["tank"], n), "actcell %d %d c" % tuple(d["aim"])]
        s += ["tickstr V01 0 49 1500"]
        for n in (0, 1):
            s += ["lclickobj %s %d" % (d["tank"], n), "stop"]
        s += ["deselect", look, "echo g457 broke", "ebldump"]
        now = 0
        for k in HELD:
            s += ["tick %d" % (k - now)] + twin("dmg%d" % k)
            s += ["echo g457 dmg %d" % k, "ebldump", "ebldbodies"]
            now = k
        # (Red Alert's debug hit takes about a hundred points off a church, not all of it)
        kill = ["hurtcell %d %d" % tuple(d["aim"])]
        if game == "ra":
            kill = kill + ["tick 1"] + kill
        s += kill + ["tick %d" % WRECK] + twin("wreck") + ["echo g457 wreck", "ebldump", "quit"]
        with open(os.path.join(out, scene + ".txt"), "w") as f:
            f.write("\n".join(s) + "\n")
    with open(os.path.join(out, "layout.json"), "w") as f:
        json.dump(layout, f, indent=1)


def rd(path):
    try:
        with open(path, errors="ignore") as f:
            return f.read().splitlines()
    except OSError:
        return []


def section(L, name):
    out, on = [], False
    for l in L:
        if l.startswith("ECHO|"):
            on = (l == "ECHO|g457 " + name)
            continue
        if on:
            out.append(l)
    return out


def field(line, key):
    m = re.search(r"\|%s=([^|]*)" % re.escape(key), line)
    return m.group(1) if m else None


def roof_mask(L, w, h):
    """The intact roof's pixels: the model's triangles (EBLDTRI) through a depth buffer,
    kept where the nearest one is a shingled top. Returns the mask and the roof's
    triangle count."""
    import numpy as np
    zb = np.full((h, w), np.inf)
    top = np.zeros((h, w), bool)
    nroof = 0
    for l in L:
        if not l.startswith("EBLDTRI|"):
            continue
        p = l.split("|")
        ny = abs(float(p[2][3:]))
        cy = float(p[3][2:].split(",")[1])
        roof = ROOF_NY[0] <= ny <= ROOF_NY[1] and ROOF_Y[0] <= cy <= ROOF_Y[1]
        nroof += roof
        P = np.array([[float(v) for v in q.split(",")] for q in p[4][4:].split(";")])
        x0, x1 = max(0, int(np.floor(P[:, 0].min()))), min(w - 1, int(np.ceil(P[:, 0].max())))
        y0, y1 = max(0, int(np.floor(P[:, 1].min()))), min(h - 1, int(np.ceil(P[:, 1].max())))
        if x1 < x0 or y1 < y0:
            continue
        (ax, ay, az), (bx, by, bz), (qx, qy, qz) = P
        den = (by - qy) * (ax - qx) + (qx - bx) * (ay - qy)
        if abs(den) < 1e-9:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        w0 = ((by - qy) * (xs - qx) + (qx - bx) * (ys - qy)) / den
        w1 = ((qy - ay) * (xs - qx) + (ax - qx) * (ys - qy)) / den
        w2 = 1.0 - w0 - w1
        z = w0 * az + w1 * bz + w2 * qz
        zs = zb[y0:y1 + 1, x0:x1 + 1]
        near = (w0 >= 0) & (w1 >= 0) & (w2 >= 0) & (z < zs)
        zs[near] = z[near]
        top[y0:y1 + 1, x0:x1 + 1][near] = roof
    return top, nroof


def pixels(d, scene, L):
    """The roof's triangle count, its pixel count, the share of it the intact shot draws
    (claim 4) and, per damaged shot and the wreck, the share that reads as roof (5, 6)."""
    import numpy as np
    from PIL import Image

    def load(tag):
        return np.asarray(Image.open(os.path.join(d, "%s_%s.png" % (scene, tag))).convert("RGB")).astype(int)
    lum = lambda a: a @ np.array([0.299, 0.587, 0.114])
    I, B = load("intact"), load("intact_bare")
    roof, nroof = roof_mask(L, I.shape[1], I.shape[0])
    if not roof.any():
        return nroof, 0, 0.0, {}
    align = float((roof & (np.abs(I - B).max(axis=2) > DRAWN)).sum()) / roof.sum()
    li = lum(I)
    roof &= li >= SEEN
    n = int(roof.sum())
    if n == 0:
        return nroof, 0, align, {}
    out = {}
    for tag in ["dmg%d" % k for k in HELD] + ["wreck"]:
        X, XB = load(tag), load(tag + "_bare")
        drawn = np.abs(X - XB).max(axis=2) > DRAWN
        fire = (X[..., 0] > 170) & (X[..., 0] - X[..., 2] > 90)
        reads = roof & drawn & ~fire & (lum(X) >= LIT * li)
        out[tag] = reads.sum() / n
    return nroof, n, align, out


def check(d):
    layout = json.load(open(os.path.join(d, "layout.json")))
    why, said = [], []
    for scene, game, src, name, pack, want in SCENES:
        L = rd(os.path.join(d, scene + ".log"))
        if not L:
            why.append("%s: no log" % scene)
            continue
        # (1) the file
        arts = sorted(set(l.split("|")[1] for l in L if l.startswith("EBLDART|V01")))
        if arts != [want]:
            why.append("%s: the church drew from %s, not %s" % (scene, arts or "nothing", want))
        bad = [l for l in L if "could not be read back" in l]
        if bad:
            why.append("%s: %s" % (scene, bad[0][:120]))
        # (2) the stages
        def stage(sec):
            st = [field(l, "stage") for l in section(L, sec) if l.startswith("EBLDOBJ|V01|")]
            return st[0] if st else None
        got = [stage("intact"), stage("broke"), stage("dmg 45"), stage("wreck")]
        if got != ["INTACT", "DAMAGED", "DAMAGED", "WRECKED"]:
            why.append("%s: stages %s" % (scene, got))
        if not any(l.startswith("TICKSTR|V01") and l.endswith("|reached") for l in L):
            why.append("%s: the church never broke" % scene)
        dm = [l for l in L if l.startswith("EBLD|damaged|")]
        if len(dm) != 1 or field(dm[0], "pieces") != "4":
            why.append("%s: the break said %s" % (scene, dm))
        # (3) the designed cut
        sec = section(L, "dmg 45")
        loose = set(field(l, "piece") for l in sec
                    if l.startswith("EBLDBODY|") and field(l, "set") == "cut")
        named = set(p for p in loose if not p.startswith("chip_"))
        if named != LOOSE:
            why.append("%s: loose pieces %s, not %s" % (scene, sorted(named), sorted(LOOSE)))
        bend = [l for l in sec if l.startswith("EBLDMOVE|V01|") and field(l, "piece") == "roof_in"]
        if not bend or field(bend[0], "kind") != "bend" or float(field(bend[0], "deg") or 0) < 30.0:
            why.append("%s: roof_in does not hang on its hinge (%s)" % (scene, bend[:1]))
        if game == "ra":
            ore = set()
            for l in L:
                m = re.match(r"RA3DORE\|cell\|[^|]*\|cell=(\d+),(\d+)", l)
                if m:
                    ore.add((int(m.group(1)), int(m.group(2))))
            x, y = layout[scene]["x"], layout[scene]["y"]
            under = sorted(c for c in ore if x - 1 <= c[0] <= x + 2 and y - 1 <= c[1] <= y + 2)
            if not ore or under:
                why.append("%s: ore under or round the church %s (of %d ore cells)" % (scene, under, len(ore)))
        # (4) to (6) the pixels
        try:
            nroof, n, align, px = pixels(d, scene, L)
        except (OSError, ValueError, IndexError) as e:
            why.append("%s: the shots or the triangles could not be read (%s)" % (scene, e))
            continue
        if nroof != ROOF_TRIS or n < MIN_ROOF or align < ALIGN:
            why.append("%s: the roof mask is not the drawn roof (%d roof triangles, %d px, %.3f drawn)"
                       % (scene, nroof, n, align))
            continue
        for k in HELD:
            if px["dmg%d" % k] < ROOF_KEEP:
                why.append("%s: at %d ticks only %.3f of the roof reads as roof" % (scene, k, px["dmg%d" % k]))
        if px["wreck"] >= ROOF_KEEP:
            why.append("%s: the wreck still reads %.3f roof; the measure tells nothing" % (scene, px["wreck"]))
        said.append("%s %s roof %d px (%.3f on the church), reads %.2f/%.2f, wreck %.2f" % (
            scene, want, n, align, px["dmg15"], px["dmg45"], px["wreck"]))
    if why:
        return "0 " + "; ".join(why)
    return "1 " + "; ".join(said)


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "maps":
        maps(sys.argv[2])
        return 0
    if len(sys.argv) >= 3 and sys.argv[1] == "check":
        print(check(sys.argv[2]))
        return 0
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main())
