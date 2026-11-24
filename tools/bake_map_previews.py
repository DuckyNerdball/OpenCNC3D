#!/usr/bin/env python3
"""
bake_map_previews.py -- one thumbnail per skirmish map, baked into `mappreview.pack`
so the lobby can show you the ground before you commit to it, the way the 1995 and
later Westwood lobbies do.

WHY THIS EXISTS

The skirmish flow currently ends in a plain list of map names. A name is not a map:
"Sand Trap" does not say whether there is water between you and the computer, where
the tiberium is, or how many players the thing was cut for. Every one of those facts
is already in the data. This turns them into a picture.

WHAT IS DRAWN, AND WHERE EACH PART COMES FROM

  ground     terrain_<SCEN>.json gives, per cell, the exact patch of the theater
             atlas the game draws there. The preview reads that patch out of the
             atlas PNG, so a preview is made of the same texels as the tactical view
             and no colour here is invented.

  water      A texel with alpha 0 in the atlas is a WATER HOLE: the terrain art has
             a gap and the renderer supplies the water surface underneath. In the
             tactical view that surface is two scrolling cartridge textures, which is
             not a colour at all, so the preview paints the same flat (28,51,74) the
             in-game radar paints a water cell with. A lobby preview and the radar are
             the same map at the same scale and they should agree.

  tiberium   The INI's [OVERLAY] section names TI1..TI12 per cell. The SHAPE comes
             from the real overlay art in dostib.pack, at the same density frame the
             engine would pick: neighbour count -> {0,1,3,4,6,7,8,10,11}, the engine's
             own Tiberium_Adjust table. So a thin field covers a fifth of its cells'
             texels and a thick one covers half, and the downsample turns that into a
             faint tint or a solid slab exactly in proportion.
             The COLOUR is a legend green, not the art's own. The cartridge's tiberium
             is olive (92,106,67 at full density), which against temperate grass
             (48,92,40) is a change of about six per cent per channel: composited
             faithfully it is invisible at this size, and a preview that hides where
             the money is has failed at the one job a lobby thumbnail has. So the mask
             is the cartridge's and the fill is a legend colour, the way the in-game
             radar paints an object in a GUI ramp colour rather than its own. Green is
             reserved for it: no start marker is allowed a green.

  starts     The INI's [Waypoints]. Waypoints 0..25 that are not -1, COMPACTED in
             ascending order, are the legal start positions: the engine builds that
             compacted array and indexes it with the player's start location without
             a bound check, so the count is per-map and it is not always 8. Waypoints
             26 and 27 are the home cell and the reinforcement cell, not starts, and
             are excluded. Each start gets a diamond in its own colour, and the pack
             also carries the pixel coordinates so a lobby that wants to colour them
             by who actually took the slot can draw its own.

  NOT drawn  [TERRAIN] (trees, rocks), [SMUDGE], [STRUCTURES], [UNITS], [INFANTRY].
             Their art is not part of this bake, and a made-up blob standing in for a
             tree would be an invented fact on a picture whose whole value is that it
             is not invented. Blocking terrain is therefore missing from the preview;
             that is a known gap, not an oversight.

RACE COURSES: A MAP WHOSE INI HAS A [Rally] SECTION

A race course is cut from clear ground: every cell of its .BIN is the same flat clear
cell, so the ground alone is a blank field with a little tiberium on it. The course is
drawn over the ground, in BOTH planes, from the course's own lines and nothing else.
Every key the drawing needs is read the way the engine's race module reads it (rally.cpp
Rally_Read_INI). Where the race refuses a course (no usable Finish, P000 or Grid0, or
any of them off the map) the bake stops too; where the race reads leniently (the grid
ends at the first missing or malformed slot, GridFacing is read like any INI integer)
the bake reads the same way. A run with no scenario named bakes every course in
game/missions after the nine retail maps and the game modes' own maps (MODE_SCENARIOS).

  walls      [OVERLAY] cells naming one of the engine's wall overlays (odata.cpp: SBAG,
             CYCL, BRIK, BARB, WOOD). They are the track's edges. A wall is plotted
             nearest-neighbour: a pixel is a wall pixel when the cell under its centre
             is a wall cell, which puts every wall where the ground under it, the lines
             and the grid say it is. At about two pixels a cell every wall cell owns at
             least one pixel centre, and the bake stops if one does not. That is also
             how the race sees a wall, as a whole impassable square. A wall is a line
             one cell thick, so it is plotted solid rather than composited and averaged:
             an average at this scale would smear it into the ground it has to stand
             out from.
             The COLOUR is a legend, for the reason tiberium's is. The 1995 radar paints
             a wall cell DKGREY (const.cpp Ground[LAND_WALL]), (85,85,85) in the game
             palettes, which is about 8 from the winter ground's mean (79,85,80); and
             the sandbag art's own colour, (113,103,56) averaged over the opaque texels
             of its DOS shape with the shadow left out, is about 25 from the desert
             ground's (133,96,70). So the fill is WALL_RGB, a pale sand in the sandbag
             art's warm family but far lighter. The bake checks, rather than trusting
             this paragraph, that it is an exact palette entry; that it is not green
             (green is tiberium's); that it is a different entry from the tiberium
             legend, every start colour of this pack and the lines' black and white;
             that it is at least START_MIN_SEPARATION from every colour the lobby marks
             a start with (the seat colours, the idle start, the rim and the label ink,
             read out of menu/doslobby.c, the nearest being the light blue seat at
             about 82); and that it clears every theater's mean ground by
             START_GROUND_CLEARANCE. The pack's own start colours are held only to being
             different entries, because on a course none of them is drawn.

  finish     [Rally] Finish=x1,y1,x2,y2, in leptons (256 a cell). A strip of black and
             white squares along the segment, as the race draws it on the ground
             (rally_mod.h rly_checker_strip): squares counted along the line and across
             it, black where the two counts add up to an even number. There it is half a
             cell wide in quarter-cell squares; here it is COURSE_LINE_PX pixels wide in
             one-pixel squares, the least that still reads as checkered at about two
             pixels a cell. The lobby draws the frame at one or two menu pixels per
             frame pixel, where a square stays a square; a panel that samples the frame
             down to fewer pixels than it has breaks the strip into plain bars and drops
             most of the grid.

  start      The start line, built exactly as rly_draw_ground builds it, because the
             course file has no key for it: across the grid's facing (GridFacing, an
             engine DirType, 0 north and clockwise), three quarters of a cell ahead of
             the foremost grid slot, centred on the grid, and as wide as the grid plus
             three quarters of a cell either side or the first course point's half
             width (P000), whichever is wider. The same checkered strip as the finish.

  grid       Grid0..Grid7, the slots the racers line up on, each one pixel of opaque
             black. That is what tells the two checkered lines apart: the start is the
             one with the grid behind it.

  Decided against, by looking at all three courses:
    the centre line (P000..), which is the computer drivers' route. Faint, it lands on
             a different grey on each theater once it is quantised to this palette, and
             it is a stripe the game never paints on the ground, so it reads as a road
             marking that is not there. The walls already draw the corridor.
    defences ([STRUCTURES] of the Hostile= house). The radar would plot each one as its
             footprint in its owner's house colour, which is real data, but the
             footprints are a table this file does not have, and the race's hostile
             house is Nod, whose red is one of the start colours and a seat colour in
             the lobby: a red block beside the track would read as a player. They stay
             under the NOT drawn rule above with everything else on [STRUCTURES]. Crate
             spots are not drawn either; they are objects, not ground.

  starts     The start records are unchanged: Waypoints 0..25, as above, because a
             battle on a course map seats its players there. On a course those
             waypoints are the grid's own cells, eight of them within four cells of each
             other, and diamonds seven pixels across and two pixels apart stamp one blob
             over the start line they mark. So on a course the MARKED plane carries no
             diamonds and is the same picture as the plain one: the grid is the start.
             A lobby still draws its own markers from px,py.

  Walls are drawn on race courses only. On a battle map a wall is one obstacle among
  the trees and rocks this bake leaves out, and the nine retail maps carry none.

THE SIZE, AND WHY IT IS 120x120

The screen this is drawn on is 320x200. A lobby needs a map list, a side plate and
buttons on the same screen, so the preview panel gets at most a third of the width.
Inset 8 pixels and 4 clear of the panel edge, a 120 wide preview leaves 150 columns
for a ten-row list and still 80 rows under the panel for the buttons; --proof draws
that layout at 1:1 rather than asserting it.

It is also enough resolution to be worth having. The nine retail maps measure 56x49
to 62x62 cells, so the frame gives between 1.94 and 2.45 pixels per cell: a tiberium
field of a dozen cells is a 25 pixel blob and the smallest lake still has shape. One
pixel per cell would fit a whole 64 cell map in 64 pixels and lose all of that.

The frame is SQUARE and fixed, so the caller blits one rectangle whatever map is
selected. Maps are not square, so the map is fitted inside it with its aspect kept and
the leftover margin filled with palette index 0, which the DOS shape blitter treats as
transparent (dosbar.h DB_TBLACK). Blit the frame over the dialog and the margin simply
does not paint. The record also carries the sub-rectangle the map really occupies, so
a caller that wants a border tight against the map can draw one.

THE PALETTE

The lobby draws into an 8-bit surface, so the previews are 8-bit indices, not RGB.
The palette is the one the menu runs on: the 256 entries embedded in TITLE.CPS, the
same ones bake_dosmenu.py puts in dosmenu.pack, so a preview blits into a menu plate
with no palette change and no remap. Quantisation is the renderer's own rule
(cnc_eyes.cpp pal_index): nearest entry in RGB, over a 5:5:5 cube, with index 0
excluded because it is the transparent one. No preview pixel is ever index 0 except
the margin, and the self-check asserts that.

INPUTS, and every one of them is either tracked or regenerable by a named command

  game/missions/<SCEN>.INI                          tracked
  tools/bakery/sharecopy/assets/terrain/
      terrain_<SCEN>.json                           n64_terrain.py <SCEN> beside it,
                                                    from the tracked extracted .MAP
                                                    (sh tools/stage-skirmish-maps.sh
                                                    runs it too, and so does this
                                                    file given --resolve-terrain)
      <THEATER>_tiles.png, the atlas it names       tracked
  data/dosdata/LOCAL.MIX  (TITLE.CPS palette)       tracked
  game/dostib.pack or playable/dostib.pack          python3 game/bake_dostiberium.py
      (or the one --tib names)
  menu/doslobby.c  (the lobby's marker colours,     tracked
      read only when a race course is baked)

Missing any of them stops the run with the command that makes it, rather than baking
a grey rectangle and calling it a fallback.

OUTPUT

  game/mappreview.pack            the previews
  game/mappreview.manifest.json   what went into them, for anyone reading rather than
                                  blitting. Both land in game/ because that is where
                                  the standalone bakers write, and the build's pack
                                  refresh scans that directory: the pack reaches the
                                  play folder without any script learning its name.

FORMAT (all little-endian), magic "MAPPREV1":

    char  magic[8]        "MAPPREV1"
    u32   version         1
    u32   count           number of maps in the pack
    u32   frame_w         120
    u32   frame_h         120
    u32   max_starts      26   (slots reserved in EVERY record, so records are fixed
                                size and the n-th one is at a computable offset)
    u8    pal6[768]       TITLE.CPS's palette, 6-bit DAC values, as it ships in the CPS
    u8    pal8[768]       the same widened with v << 2, i.e. what an index resolves to
    count x {
        char  scenario[12]      "SCM01EA", NUL padded
        char  name[24]          the INI [Basic] Name=, NUL padded
        char  theater[12]       "TEMPERATE" / "DESERT", NUL padded
        u16   cell_x, cell_y, cell_w, cell_h    the INI [MAP] playable rect, in cells
        u16   img_x, img_y, img_w, img_h        where the map sits inside the frame
        u16   starts                            legal start positions, 0..max_starts
        u16   tiberium                          cells carrying a tiberium overlay
        max_starts x {
            u16 cell_x, cell_y                  map cell of this start
            u16 px, py                          its centre in frame pixels
            u8  colour                          palette index its diamond is drawn in
            u8  reserved                        0
        }                                       slots past `starts` are all zero
        u8    plain [frame_w*frame_h]           the ground (and on a race course the
                                                course), row major, top row first
        u8    marked[frame_w*frame_h]           the same with the start diamonds on it
                                                (none on a race course; see above)
    }

    Record size is 12+24+12+8+8+4 + max_starts*10 + 2*frame_w*frame_h, fixed.

    Two planes because there are two kinds of caller. A lobby that just wants a
    picture blits `marked` and is done. A lobby that colours each start by who took
    the slot blits `plain` and draws its own markers at the recorded px,py.

THE PROOF THAT THE GROUND IS THE GAME'S GROUND

`--verify` is not an opinion either. n64_terrain.py writes, beside each terrain JSON it
resolves, a terrain_<SCEN>.png: its own 64x64 by 24 pixel render of that map. Four of
those are tracked, from both theaters, and running the map pipeline produces more.
--verify finds every one that is present, rebuilds it with the code in this file, and
compares texel by texel. On a checkout with the nine skirmish maps resolved:

    OPAQUE TEXELS IDENTICAL, 13 scenarios, 29192890 of 29192890 (100.0000%)

The only texels excluded are the water holes, and they are excluded because the two
renders disagree about one unit of green there and nowhere else: this file paints the
(28,51,74) the in-game radar paints a water cell with, n64_terrain.py paints its own
documented placeholder (28,52,74). Neither is sampled from the cartridge's water tiles,
which are an animated textured surface and not a colour at all; the radar's value is
used here because a lobby preview and the radar showing the same map should agree.

USAGE

    python3 tools/bake_map_previews.py                     # the nine retail maps, the
                                                           # game modes' maps and every
                                                           # race course in
                                                           # game/missions, to the pack
                                                           # below
    python3 tools/bake_map_previews.py SCM02EA SCM07EA     # a subset
    python3 tools/bake_map_previews.py --png /tmp/prev     # also plain PNGs, 1:1 and 4:1
    python3 tools/bake_map_previews.py --proof /tmp/sheet.png   # a labelled contact sheet
    python3 tools/bake_map_previews.py --verify            # the fidelity check, alone
    python3 tools/bake_map_previews.py --check playable/mappreview.pack
                                                           # does a built pack carry
                                                           # every map the committed
                                                           # manifest lists?
    python3 tools/bake_map_previews.py --list              # the scenarios a default
                                                           # bake covers
    python3 tools/bake_map_previews.py --resolve-terrain --out X.pack
                                                           # make any missing
                                                           # terrain_<SCEN>.json first

A BUILT PACK IS CHECKED AGAINST THE COMMITTED MANIFEST, because the pack is a build
product and the manifest is not. game/mappreview.manifest.json is written by every bake
and committed with the change that made it, so it names the maps the source says a lobby
has a picture for. A play folder only ever gains the pack somebody baked, and the lobby
does not complain about a map it has no record for: it draws an empty panel. A folder
whose pack was baked before the three race courses existed therefore shipped every
course with no picture and nothing said so. --check reads the pack's own header and
records (magic MAPPREV1, the format above) and refuses any map the manifest lists that
the pack does not carry; game/make-build.sh rebakes on that refusal and fails the build
if the rebake does not cure it, and both packagers ask the same question of what they
are about to zip.
"""

import argparse
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(ROOT, "menu", "tools"))

MISSIONS = os.path.join(ROOT, "game", "missions")
TERRAIN = os.path.join(ROOT, "tools", "bakery", "sharecopy", "assets", "terrain")
DOSDATA = os.path.join(ROOT, "data", "dosdata")
OUT = os.path.join(ROOT, "game", "mappreview.pack")

MANIFEST = os.path.join(ROOT, "game", "mappreview.manifest.json")
N64_TERRAIN = os.path.join(TERRAIN, "n64_terrain.py")

MAGIC = b"MAPPREV1"
VERSION = 1
FRAME_W = 120
FRAME_H = 120
MAX_STARTS = 26          # the size of the engine's own compacted waypoint array

CELL_PX = 24             # the atlas is 24 texels per cell, display.h ICON_PIXEL_W

# The nine retail multiplayer scenarios off the 1995 disc, in disc order.
RETAIL_SCENARIOS = ["SCM%02dEA" % n for n in range(1, 10)]

# cnc_eyes.cpp: the flat colour the in-game radar paints a water cell with, and the
# fallback mean colour it gives a cell that is nothing but hole. Reused here so the
# lobby preview and the radar are the same picture of the same map.
WATER_RGB = (28, 51, 74)

# cell.cpp Tiberium_Adjust: OverlayData (the frame index) from the number of the eight
# neighbouring cells that also carry tiberium.
TIB_ADJ = (0, 1, 3, 4, 6, 7, 8, 10, 11)

# The legend colour tiberium is painted in. It is palette entry 4 exactly, so an
# undiluted tiberium texel quantises back to itself with no error and only the
# downsample's blend with the ground moves it.
TIB_RGB = (84, 252, 84)

# How the start colours are chosen out of the menu palette. TITLE.CPS is the title
# screen's own palette: fire, sky and desert, so it has plenty of reds, oranges,
# yellows and warm greys, exactly one cyan, one strong green ramp, and NO blue and no
# magenta at all. Asking it for "the six Westwood house colours" gets three washed-out
# greys, so the set is derived from what is actually there instead of imposed.
#
#   seeds        the four the palette does well, in the order a player expects
#   candidates   bright enough to read against dark water; never green-dominant,
#                because green is the tiberium legend and a green diamond in the
#                middle of a green field is not a marker; and never within
#                START_GROUND_CLEARANCE of a theater's mean ground colour, measured
#                off the theater atlases themselves. Without that last rule the
#                selection happily hands out (148,112,96), which is 34 from the desert
#                atlas's mean (133,96,70): a marker the colour of the ground it stands
#                on is not a marker.
#   the rest     greedy farthest-point: each new colour is the candidate whose
#                nearest already-chosen colour is furthest away
START_SEEDS = [(5, "yellow"), (127, "red"), (15, "white"), (2, "cyan")]
START_COUNT = 10               # SCM07EA has ten legal start positions
START_MIN_LUMA = 70
START_MIN_SEPARATION = 75      # euclidean in RGB; the bake stops if the set is closer
START_GROUND_CLEARANCE = 90    # euclidean in RGB from each theater's mean ground

# The start diamond: |dx|+|dy| <= 3 painted in opaque black, then |dx|+|dy| <= 2 in the
# slot colour. 7 pixels across, about three and a half cells, which is the smallest
# marker that still reads as a deliberate mark rather than a speck of terrain.
MARK_RIM = 3
MARK_FILL = 2

# ---- race courses (see RACE COURSES at the top) ----
# odata.cpp: the engine's five wall overlays, by their INI names.
WALL_TYPES = ("SBAG", "CYCL", "BRIK", "BARB", "WOOD")
# The wall legend. It is palette entry 48 exactly, a pale sand, so a wall pixel is this
# index with no quantisation in between.
WALL_RGB = (244, 212, 148)
# The lobby that draws these previews puts its own start markers and labels on them,
# in colours it keeps in menu/doslobby.c rather than in this pack: each start in its
# holder's seat colour (SK_PLAYER_COLOUR), an unheld one in SK_START_IDLE, every one
# rimmed in SK_START_RIM, and its number in the palette's whitest entry. The wall legend
# is checked against those, read out of that file, so a seat colour changed there is a
# bake that stops here rather than a wall that quietly looks like a player.
LOBBY_SOURCE = os.path.join(ROOT, "menu", "doslobby.c")
# The checkered start and finish lines: this many pixels across, in one-pixel squares.
COURSE_LINE_PX = 2
# rly_draw_ground: the start line stands this far ahead of the foremost grid slot, and
# reaches this far past the grid on either side, both in cells.
START_LINE_AHEAD = 0.75
START_LINE_MARGIN = 0.75
# rally_internal.h MAX_GRID: Grid0..Grid7, read until the first one missing or malformed.
MAX_GRID = 8
LEPTONS_PER_CELL = 256
# rally.cpp LEPTON_LIMIT: Read_Ints refuses a number outside -limit..limit.
LEPTON_LIMIT = 1 << 20


def die(msg):
    raise SystemExit("bake_map_previews: " + msg)


# ---------------------------------------------------------------------------
# the menu palette
# ---------------------------------------------------------------------------

def load_menu_palette():
    """The 256 entries embedded in TITLE.CPS, the palette the whole DOS menu runs on.

    Read the same way bake_dosmenu.py reads it, from the same file, so a preview and
    the plate it is drawn on cannot disagree about what an index means."""
    try:
        from mixshp import MixSet
    except ImportError as exc:
        die("cannot import menu/tools/mixshp.py (%s)" % exc)
    local = os.path.join(DOSDATA, "LOCAL.MIX")
    if not os.path.exists(local):
        die("no LOCAL.MIX at %s. It carries TITLE.CPS, whose embedded palette is the "
            "one the menu draws in; there is no substitute for it." % local)
    mixes = MixSet(DOSDATA, ["LOCAL.MIX"])
    data = mixes.read("TITLE.CPS")
    if not data:
        die("TITLE.CPS is not in %s" % local)
    _size, _comp, _uncomp, palsize = struct.unpack_from("<HHIH", data, 0)
    if palsize != 768:
        die("TITLE.CPS carries a %d byte palette, expected 768" % palsize)
    pal6 = bytes(data[10:10 + 768])
    if max(pal6) > 63:
        die("TITLE.CPS palette is not 6-bit (max %d)" % max(pal6))
    # video_sdl2.cpp:653 -- the engine widens a 6-bit DAC value with v << 2.
    pal8 = bytes(min(252, v << 2) for v in pal6)
    return pal6, pal8


def build_pal_lut(pal8):
    """cnc_eyes.cpp build_pal_lut, verbatim: nearest palette entry for every cell of a
    5:5:5 cube, with index 0 excluded because it is the transparent one."""
    import numpy as np
    idx = np.arange(32768)
    r = ((idx >> 10) & 31) * 255 // 31
    g = ((idx >> 5) & 31) * 255 // 31
    b = (idx & 31) * 255 // 31
    pal = np.frombuffer(pal8, dtype=np.uint8).reshape(256, 3).astype(np.int32)
    best = np.zeros(32768, dtype=np.uint8)
    bestd = np.full(32768, 1 << 30, dtype=np.int64)
    for c in range(1, 256):
        d = ((r - pal[c, 0]) ** 2 + (g - pal[c, 1]) ** 2 + (b - pal[c, 2]) ** 2)
        hit = d < bestd
        bestd = np.where(hit, d, bestd)
        best = np.where(hit, np.uint8(c), best)
    return best


def quantise(rgb, lut):
    """RGB array (h, w, 3) -> palette indices (h, w), by cnc_eyes.cpp pal_index."""
    import numpy as np
    a = rgb.astype(np.uint16)
    key = ((a[:, :, 0] >> 3).astype(np.int32) << 10) \
        | ((a[:, :, 1] >> 3).astype(np.int32) << 5) \
        | (a[:, :, 2] >> 3).astype(np.int32)
    return lut[key]


def nearest_index(rgb, lut):
    r, g, b = rgb
    return int(lut[((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)])


def ground_colours(atlases):
    """The mean opaque colour of the theater atlases used by this bake. This is
    what "the ground" is, measured rather than guessed, and it is what a start marker
    has to stand out from."""
    import numpy as np
    from PIL import Image
    out = {}
    for f in sorted(os.listdir(TERRAIN)):
        if f not in atlases:
            continue
        a = np.asarray(Image.open(os.path.join(TERRAIN, f)).convert("RGBA"))
        op = a[a[:, :, 3] >= 128][:, :3].astype(float)
        if not len(op):
            die("%s has no opaque texels at all" % f)
        out[f[:-len("_tiles.png")]] = [int(round(v)) for v in op.mean(axis=0)]
    if len(out) != len(atlases):
        die("missing theater atlas in %s: %s" % (TERRAIN, sorted(atlases)))
    return out


def pick_start_colours(pal8, grounds, count):
    """The start-marker colours, derived from the palette rather than wished for.

    Seeded farthest-point selection over the entries that are bright enough to read on
    dark water and are not green (green belongs to tiberium). Returns the seeds first,
    in their fixed order, so start 0 is always yellow and start 1 always red however
    many colours the rest of the run needs.

    `count` is how many the maps in this run actually need, never fewer than
    START_COUNT. Colours are never cycled: two starts a player can pick and cannot tell
    apart is the failure this whole selection exists to avoid, so if the palette cannot
    supply that many at the separation floor, the bake stops."""
    def rgb(i):
        return (pal8[i * 3], pal8[i * 3 + 1], pal8[i * 3 + 2])

    def luma(c):
        return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]

    def d2(a, b):
        return sum((a[k] - b[k]) ** 2 for k in range(3))

    def green_dominant(c):
        r, g, b = c
        return g > r + 24 and g > b + 24

    def clear_of_ground(c):
        return all(d2(c, g) ** 0.5 >= START_GROUND_CLEARANCE for g in grounds.values())

    chosen = [i for i, _ in START_SEEDS]
    labels = {i: lab for i, lab in START_SEEDS}
    cand = [i for i in range(1, 256)
            if luma(rgb(i)) >= START_MIN_LUMA and not green_dominant(rgb(i))
            and clear_of_ground(rgb(i))]
    for i in chosen:
        if green_dominant(rgb(i)):
            die("seed colour %d %r is green, which is reserved for tiberium"
                % (i, rgb(i)))
        if not clear_of_ground(rgb(i)):
            die("seed colour %d %r is within %d of a theater's mean ground %r"
                % (i, rgb(i), START_GROUND_CLEARANCE, grounds))
    while len(chosen) < count:
        best, bestd = None, -1
        for i in cand:
            if i in chosen:
                continue
            m = min(d2(rgb(i), rgb(j)) for j in chosen)
            if m > bestd:
                bestd, best = m, i
        if best is None:
            die("the palette has %d usable start colours and this run needs %d"
                % (len(chosen), count))
        chosen.append(best)
    sep = min(d2(rgb(a), rgb(b)) ** 0.5
              for n, a in enumerate(chosen) for b in chosen[n + 1:])
    if sep < START_MIN_SEPARATION:
        die("the closest two start colours are %.0f apart in RGB, under the %d floor. "
            "Two starts a player cannot tell apart is worse than no colour at all."
            % (sep, START_MIN_SEPARATION))
    return [{"slot": n, "index": i, "rgb": list(rgb(i)),
             "label": labels.get(i, "derived"),
             "clearance_from_ground": {k: int(round(d2(rgb(i), g) ** 0.5))
                                       for k, g in grounds.items()}}
            for n, i in enumerate(chosen)], sep


# ---------------------------------------------------------------------------
# the scenario INI
# ---------------------------------------------------------------------------

def ini_sections(text):
    out = {}
    cur = None
    for line in text.splitlines():
        line = line.rstrip()
        m = re.match(r"^\[([^\]]+)\]", line.strip())
        if m:
            cur = m.group(1).upper()
            out.setdefault(cur, [])
            continue
        if cur is None or not line.strip() or line.strip().startswith(";"):
            continue
        out[cur].append(line.strip())
    return out


def course_scenarios():
    """Every scenario in game/missions whose INI has a [Rally] section, sorted. These
    are the race courses the skirmish list offers beside the retail maps, so a bake run
    with no scenario named covers them too: a course added to game/missions gets its
    preview from the same command, and a release bake cannot leave one out."""
    found = []
    for fn in sorted(os.listdir(MISSIONS)):
        stem, ext = os.path.splitext(fn)
        if ext != ".INI":
            continue
        with open(os.path.join(MISSIONS, fn), errors="replace") as fh:
            if "RALLY" in ini_sections(fh.read()):
                found.append(stem)
    return found


# Maps a game mode adds to the skirmish list that are neither retail nor race courses:
# Pitfall Quarter, the Gun Game arena.
MODE_SCENARIOS = ["SCM92EA"]


def default_scenarios():
    """The run with no scenario named: the nine retail maps, the game modes' own maps,
    then every race course."""
    fixed = RETAIL_SCENARIOS + MODE_SCENARIOS
    return fixed + [s for s in course_scenarios() if s not in fixed]


def kv(lines):
    out = {}
    for line in lines:
        if "=" in line:
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def read_ini(scen):
    path = os.path.join(MISSIONS, scen + ".INI")
    if not os.path.exists(path):
        die("no %s. The nine retail skirmish INIs are staged out of GENERAL.MIX by\n"
            "       sh tools/stage-skirmish-maps.sh" % path)
    secs = ini_sections(open(path, errors="replace").read())
    for need in ("MAP", "WAYPOINTS"):
        if need not in secs:
            die("%s.INI has no [%s] section" % (scen, need))

    mp = kv(secs["MAP"])
    try:
        rect = (int(mp["X"]), int(mp["Y"]), int(mp["Width"]), int(mp["Height"]))
    except KeyError as exc:
        die("%s.INI [MAP] has no %s" % (scen, exc))
    theater = mp.get("Theater", "").upper()
    if not theater:
        die("%s.INI [MAP] has no Theater" % scen)

    name = kv(secs.get("BASIC", [])).get("Name", "").strip()
    if not name:
        die("%s.INI [Basic] has no Name; the lobby row and the preview would disagree"
            % scen)

    if not (0 <= rect[0] and 0 <= rect[1] and rect[0] + rect[2] <= 64
            and rect[1] + rect[3] <= 64 and rect[2] > 0 and rect[3] > 0):
        die("%s.INI [MAP] rect %r does not fit the 64x64 cell grid" % (scen, rect))

    wp = {}
    for line in secs["WAYPOINTS"]:
        m = re.match(r"^(\d+)\s*=\s*(-?\d+)\s*$", line)
        if not m:
            die("%s.INI [Waypoints] line %r is not <number>=<cell>" % (scen, line))
        wp[int(m.group(1))] = int(m.group(2))
    # Waypoints 0..25 that are not -1, compacted in ascending order. That compacted
    # array IS the start table the engine indexes; 26 (home) and 27 (reinforcement)
    # are not starts.
    starts = [wp[i] for i in sorted(k for k in wp if 0 <= k < 26) if wp[i] >= 0]
    if not starts:
        die("%s.INI has no valid waypoint below 26, so it has no start positions at all"
            % scen)

    # [OVERLAY] carries walls and crates as well as tiberium; only TI1..TI12 is
    # tiberium and only tiberium is drawn. Anything else is left alone silently
    # because it is not a preview's business, not because it failed to parse.
    tib = {}
    for line in secs.get("OVERLAY", []):
        m = re.match(r"^(\d+)\s*=\s*(\S+)\s*$", line)
        if not m:
            die("%s.INI [OVERLAY] line %r is not <cell>=<name>" % (scen, line))
        o = re.match(r"^TI(\d+)$", m.group(2).upper())
        if o:
            kind = int(o.group(1)) - 1               # TI1 -> kind 0
            if not 0 <= kind < 12:
                die("%s.INI [OVERLAY] names TI%s; the engine has twelve"
                    % (scen, o.group(1)))
            tib[int(m.group(1))] = kind
    return {"scenario": scen, "name": name, "theater": theater, "rect": rect,
            "starts": starts, "tiberium": tib, "rally": read_rally(scen, secs)}


def read_rally(scen, secs):
    """The [Rally] lines the preview draws, or None for a map that is not a race course.

    Read with the engine's own rules (rally.cpp Rally_Read_INI and Read_Ints): keys are
    case-insensitive; a value is comma-separated integers, each within LEPTON_LIMIT, with
    anything after the last one ignored; a course point is P000 or P0. Where the engine
    refuses the course (Finish or P000 missing or malformed, anything off the map, no
    grid slot at all) this stops too, rather than drawing a guess. Where the engine is
    lenient this is lenient the same way: the grid is Grid0 upwards and ends at the
    first slot that is missing or malformed, and GridFacing is INIClass::Get_Int's
    reading of whatever is there."""
    if "RALLY" not in secs:
        return None
    keys = {}
    for line in secs["RALLY"]:
        if "=" in line:
            k, v = line.split("=", 1)
            keys[k.strip().upper()] = v.split(";", 1)[0].strip()

    def ints(key, count, alt=None):
        """The engine's Read_Ints: None when the key is absent or empty, "malformed"
        when it does not hold `count` integers within LEPTON_LIMIT, else the list."""
        raw = keys.get(key.upper(), "")
        if not raw and alt:
            raw = keys.get(alt.upper(), "")
        if not raw:
            return None
        m = re.match(r"^[ \t]*" + r"[ \t]*,[ \t]*".join([r"([-+]?\d+)"] * count), raw)
        if not m:
            return "malformed"
        v = [int(g) for g in m.groups()]
        if any(abs(n) > LEPTON_LIMIT for n in v):
            return "malformed"
        return v

    def needed(key, count, what, alt=None):
        v = ints(key, count, alt)
        if v is None or v == "malformed":
            why = "is missing" if v is None else \
                "is not %d comma-separated integers within %d" % (count, LEPTON_LIMIT)
            die("%s.INI [Rally] %s %s; the race refuses this course" % (scen, what, why))
        return v

    def in_map(x, y, what):
        if not (0 <= x < 64 * LEPTONS_PER_CELL and 0 <= y < 64 * LEPTONS_PER_CELL):
            die("%s.INI [Rally] %s (%d,%d) is off the map; the race refuses this course"
                % (scen, what, x, y))

    finish = needed("Finish", 4, "Finish")
    in_map(finish[0], finish[1], "Finish")
    in_map(finish[2], finish[3], "Finish")

    grid = []
    for i in range(MAX_GRID):
        g = ints("Grid%d" % i, 2)
        if g is None or g == "malformed":
            break
        in_map(g[0], g[1], "Grid%d" % i)
        grid.append(tuple(g))
    if not grid:
        die("%s.INI [Rally] has no usable Grid0; the race refuses this course" % scen)

    p0 = needed("P000", 3, "P000, the first course point,", alt="P0")
    in_map(p0[0], p0[1], "P000")
    if not 0 < p0[2] <= 64 * LEPTONS_PER_CELL:
        die("%s.INI [Rally] P000's half width %d is out of range; the race refuses this "
            "course" % (scen, p0[2]))

    walls = set()
    for line in secs.get("OVERLAY", []):
        m = re.match(r"^(\d+)\s*=\s*(\S+)\s*$", line)
        if m and m.group(2).upper() in WALL_TYPES:
            walls.add(int(m.group(1)))
    return {"finish": tuple(finish), "grid": grid,
            "facing": ini_get_int(keys.get("GRIDFACING", ""), 0) & 255,
            "first_half_width": p0[2], "walls": sorted(walls)}


def ini_get_int(raw, default):
    """INIClass::Get_Int on one value: "$1F" and "1Fh" are hexadecimal, anything else is
    atoi's reading (leading blanks, a sign, the digits up to the first non-digit, and 0
    when there are none); an absent or empty value is the default."""
    raw = raw.strip()
    if not raw:
        return default
    if raw.startswith("$"):
        m = re.match(r"^\$([0-9A-Fa-f]+)", raw)
        return int(m.group(1), 16) if m else default
    if raw[-1] in "hH":
        m = re.match(r"^[ \t]*([0-9A-Fa-f]+)", raw)
        return int(m.group(1), 16) if m else default
    m = re.match(r"^[ \t]*([-+]?\d+)", raw)
    return int(m.group(1)) if m else 0


# ---------------------------------------------------------------------------
# the terrain
# ---------------------------------------------------------------------------

def read_terrain(scen):
    path = os.path.join(TERRAIN, "terrain_%s.json" % scen)
    if not os.path.exists(path):
        if os.path.exists(os.path.join(TERRAIN, "..", "extracted", "MAP", scen + ".MAP")):
            tool = os.path.relpath(os.path.join(TERRAIN, "n64_terrain.py"), ROOT)
            how = "python3 %s %s" % (tool, scen)
        else:
            how = "sh tools/stage-skirmish-maps.sh %s" % scen
        die("no %s.\n"
            "       That file is the resolved terrain, and it is a regenerated\n"
            "       intermediate rather than tracked data. Make it with:\n"
            "           %s" % (path, how))
    with open(path) as fh:
        t = json.load(fh)
    if t.get("tileSize") != CELL_PX:
        die("%s says tileSize=%s; this bake assumes %d texels per cell"
            % (path, t.get("tileSize"), CELL_PX))
    st = t.get("stats", {})
    if st.get("unmapped"):
        die("%s reports %d unmapped cells. Those draw as clear ground, so a preview "
            "made from it would quietly lie about the map." % (path, st["unmapped"]))
    atlas = os.path.join(TERRAIN, t["atlas"])
    if not os.path.exists(atlas):
        die("no theater atlas at %s" % atlas)
    return t, atlas


def load_tiberium(explicit=None):
    """The tiberium overlay art out of dostib.pack, as 12 kinds x 12 stages of 24x24
    RGBA. Format is bake_dostiberium.py's own, read here with a parser that knows only
    what that file's header comment documents. `explicit` (--tib) names the pack to
    read, which a build passes so the pictures are made from the pack it ships."""
    import numpy as np
    if explicit:
        if not os.path.exists(explicit):
            die("no dostib.pack at %s (--tib)" % explicit)
        cands = [explicit]
    else:
        cands = [os.path.join(ROOT, "game", "dostib.pack"),
                 os.path.join(ROOT, "playable", "dostib.pack")]
    path = next((p for p in cands if os.path.exists(p)), None)
    if path is None:
        die("no dostib.pack in game/ or playable/. The preview composites the REAL\n"
            "       tiberium art rather than a green rectangle, so it needs that pack:\n"
            "           python3 game/bake_dostiberium.py")
    blob = open(path, "rb").read()
    if blob[:8] != b"DOSTIB1\x00":
        die("%s does not start with the DOSTIB1 magic" % path)
    ver, _src, nsets, fw, fh, types, frames = struct.unpack_from("<7I", blob, 8)
    if ver != 1 or (fw, fh) != (CELL_PX, CELL_PX):
        die("%s is version %d with %dx%d frames; expected version 1, %dx%d"
            % (path, ver, fw, fh, CELL_PX, CELL_PX))
    if nsets < 1:
        die("%s carries no sets" % path)
    o = 8 + 28 + 12                                   # header + the first set's name
    nsheets, texw, texh, _cols, _rows = struct.unpack_from("<5I", blob, o)
    o += 20
    slots = []
    for _ in range(types * frames):
        slots.append(struct.unpack_from("<3H", blob, o))
        o += 6
    sheets = []
    for _ in range(nsheets):
        sheets.append(np.frombuffer(blob, dtype=np.uint8, count=texw * texh * 4,
                                    offset=o).reshape(texh, texw, 4))
        o += texw * texh * 4
    art = {}
    for kind in range(types):
        for stage in range(frames):
            sh, x0, y0 = slots[kind * frames + stage]
            art[(kind, stage)] = sheets[sh][y0:y0 + fh, x0:x0 + fw, :]
    return art, os.path.relpath(os.path.abspath(path), ROOT), types, frames


def render_full(ini, terr, atlas_path, tib_art):
    """The playable rect at 24 pixels per cell, RGB, water resolved and tiberium
    composited. This is the honest full-resolution picture; the fit to the frame is a
    downsample of it."""
    import numpy as np
    from PIL import Image

    x0, y0, w, h = ini["rect"]
    atlas = np.asarray(Image.open(atlas_path).convert("RGBA"))
    aw, ah = terr["atlasWidth"], terr["atlasHeight"]
    if atlas.shape[1] != aw or atlas.shape[0] != ah:
        die("%s is %dx%d but terrain_%s.json says %dx%d"
            % (atlas_path, atlas.shape[1], atlas.shape[0], ini["scenario"], aw, ah))

    cells = {}
    for c in terr["cells"]:
        cells[(c["x"], c["y"])] = c

    tib = ini["tiberium"]
    tset = set(tib)

    out = np.zeros((h * CELL_PX, w * CELL_PX, 3), dtype=np.uint8)
    holes = 0
    for cy in range(y0, y0 + h):
        for cx in range(x0, x0 + w):
            c = cells.get((cx, cy))
            if c is None:
                die("terrain_%s.json has no cell (%d,%d) but [MAP] says it is inside "
                    "the playable rect" % (ini["scenario"], cx, cy))
            px, py = c["px"], c["py"]
            patch = atlas[py:py + CELL_PX, px:px + CELL_PX, :].astype(np.int32)
            rgb = patch[:, :, :3].copy()
            # A texel with alpha 0 is a water hole: the art has a gap and the renderer
            # paints water under it. Same colour the renderer uses.
            mask = patch[:, :, 3] < 128
            if mask.any():
                holes += 1
                rgb[mask] = WATER_RGB

            cell_no = cy * 64 + cx
            if cell_no in tib:
                n = 0
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        nx, ny = cx + dx, cy + dy
                        if 0 <= nx < 64 and 0 <= ny < 64 and (ny * 64 + nx) in tset:
                            n += 1
                stage = TIB_ADJ[n]
                frame = tib_art.get((tib[cell_no], stage))
                if frame is None:
                    die("dostib.pack has no frame for kind %d stage %d"
                        % (tib[cell_no], stage))
                # The cartridge's own mask, the legend's colour. See the note at the
                # top of this file for why the art's own olive is not used.
                rgb[frame[:, :, 3] >= 128] = TIB_RGB

            oy = (cy - y0) * CELL_PX
            ox = (cx - x0) * CELL_PX
            out[oy:oy + CELL_PX, ox:ox + CELL_PX, :] = rgb.astype(np.uint8)
    return out, holes


def fit_frame(full, cell_w, cell_h):
    """Fit the full-resolution render into the fixed square frame, aspect kept and
    centred. BOX is an exact area average, which is what a downsample of ground cover
    wants: every source texel contributes to the pixel it lands in and to no other."""
    import numpy as np
    from PIL import Image
    scale = min(FRAME_W / float(cell_w), FRAME_H / float(cell_h))
    iw = max(1, min(FRAME_W, int(round(cell_w * scale))))
    ih = max(1, min(FRAME_H, int(round(cell_h * scale))))
    small = Image.fromarray(full).resize((iw, ih), Image.BOX)
    ix = (FRAME_W - iw) // 2
    iy = (FRAME_H - ih) // 2
    canvas = np.zeros((FRAME_H, FRAME_W, 3), dtype=np.uint8)
    canvas[iy:iy + ih, ix:ix + iw, :] = np.asarray(small)
    return canvas, (ix, iy, iw, ih)


def stamp_start(plane, px, py, colour, black):
    """The diamond, drawn straight into the index plane. Rim first, fill second."""
    for dy in range(-MARK_RIM, MARK_RIM + 1):
        for dx in range(-MARK_RIM, MARK_RIM + 1):
            d = abs(dx) + abs(dy)
            if d > MARK_RIM:
                continue
            x, y = px + dx, py + dy
            if x < 0 or y < 0 or x >= FRAME_W or y >= FRAME_H:
                continue
            plane[y][x] = colour if d <= MARK_FILL else black


def checker_strip(plane, img, a, b, width, black, white):
    """A strip of one-pixel black and white squares `width` pixels across, centred on the
    segment a-b (frame pixels, fractional). A pixel belongs to the strip when its centre
    does. Its square is counted from the segment's start along it and from one edge
    across it, the way rly_checker_strip counts its squares, and is black when the two
    counts add up to an even number and white when odd, as there. Clipped to the map's
    own rectangle, so the margin stays index 0."""
    import math
    ix, iy, iw, ih = img
    ax, ay = a
    dx, dy = b[0] - ax, b[1] - ay
    length = math.hypot(dx, dy)
    if length < 0.5:
        return 0
    ux, uy = dx / length, dy / length
    nx, ny = -uy, ux
    half = width / 2.0
    painted = 0
    x_lo = max(ix, int(math.floor(min(ax, b[0]) - width)))
    x_hi = min(ix + iw - 1, int(math.ceil(max(ax, b[0]) + width)))
    y_lo = max(iy, int(math.floor(min(ay, b[1]) - width)))
    y_hi = min(iy + ih - 1, int(math.ceil(max(ay, b[1]) + width)))
    for y in range(y_lo, y_hi + 1):
        for x in range(x_lo, x_hi + 1):
            px, py = x + 0.5 - ax, y + 0.5 - ay
            along = px * ux + py * uy
            across = px * nx + py * ny
            if 0.0 <= along < length and -half <= across < half:
                odd = (int(math.floor(along)) + int(math.floor(across + half))) & 1
                plane[y, x] = white if odd else black
                painted += 1
    return painted


def start_line(course):
    """The start line's two ends in cells, built as rly_draw_ground builds it: across the
    grid's facing, START_LINE_AHEAD ahead of the foremost slot, centred on the grid, as
    wide as the grid plus START_LINE_MARGIN a side or the first course point's half
    width, whichever is wider. DirType 0 is north and the map's y grows southwards."""
    import math
    ang = course["facing"] * 2.0 * math.pi / 256.0
    fx, fy = math.sin(ang), -math.cos(ang)           # the way the grid faces
    sx, sy = -fy, fx                                 # across it
    grid = [(gx / float(LEPTONS_PER_CELL), gy / float(LEPTONS_PER_CELL))
            for gx, gy in course["grid"]]
    front = max(gx * fx + gy * fy for gx, gy in grid)
    lo = min(gx * sx + gy * sy for gx, gy in grid)
    hi = max(gx * sx + gy * sy for gx, gy in grid)
    mx = sum(g[0] for g in grid) / len(grid)
    my = sum(g[1] for g in grid) / len(grid)
    hw = max(0.5 * (hi - lo) + START_LINE_MARGIN,
             course["first_half_width"] / float(LEPTONS_PER_CELL))
    along = front + START_LINE_AHEAD - (mx * fx + my * fy)
    cx, cy = mx + fx * along, my + fy * along
    return (cx - sx * hw, cy - sy * hw), (cx + sx * hw, cy + sy * hw)


def draw_course(plane, ini, img, wall, black, white):
    """The race course over the quantised ground, in place: walls, then the two
    checkered lines over them, then the grid. Returns what was drawn, for the
    manifest."""
    course = ini["rally"]
    x0, y0, cw, ch = ini["rect"]
    ix, iy, iw, ih = img

    def frame_xy(cell_x, cell_y):
        return (ix + (cell_x - x0) * iw / float(cw), iy + (cell_y - y0) * ih / float(ch))

    # The walls: a pixel is a wall pixel when the cell under its centre is a wall cell.
    # Integer arithmetic, so the cell is exact: pixel p's centre is p + 1/2, which is
    # cell x0 + (2p + 1) * cw / (2 * iw), rounded down.
    import numpy as np
    is_wall = np.zeros((64, 64), dtype=bool)
    shown = 0
    for cell in course["walls"]:
        cx, cy = cell % 64, cell // 64
        if x0 <= cx < x0 + cw and y0 <= cy < y0 + ch:
            is_wall[cy, cx] = True
            shown += 1
    col = x0 + ((2 * np.arange(iw) + 1) * cw) // (2 * iw)
    row = y0 + ((2 * np.arange(ih) + 1) * ch) // (2 * ih)
    mask = is_wall[row[:, None], col[None, :]]
    # Every wall cell must own at least one pixel centre, or a wall would vanish. At
    # this frame size a cell is about two pixels, so this only trips if the frame or
    # the map size changes.
    owned = set(zip(row[np.nonzero(mask)[0]].tolist(), col[np.nonzero(mask)[1]].tolist()))
    if len(owned) != shown:
        die("%s: %d wall cells but only %d of them own a pixel at this scale"
            % (ini["scenario"], shown, len(owned)))
    plane[iy:iy + ih, ix:ix + iw][mask] = wall

    f = course["finish"]
    L = float(LEPTONS_PER_CELL)
    fin = checker_strip(plane, img, frame_xy(f[0] / L, f[1] / L),
                        frame_xy(f[2] / L, f[3] / L), COURSE_LINE_PX, black, white)
    sa, sb = start_line(course)
    sta = checker_strip(plane, img, frame_xy(*sa), frame_xy(*sb), COURSE_LINE_PX,
                        black, white)
    if not fin or not sta:
        die("%s: the %s line drew no pixel" % (ini["scenario"],
                                               "finish" if not fin else "start"))

    grid_px = []
    for gx, gy in course["grid"]:
        px, py = frame_xy(gx / L, gy / L)
        px, py = int(px), int(py)
        if ix <= px < ix + iw and iy <= py < iy + ih:
            plane[py, px] = black
            grid_px.append([px, py])
    return {"wall_cells": len(course["walls"]), "wall_cells_shown": shown,
            "finish_leptons": list(f), "finish_pixels": fin,
            "start_line_cells": [round(v, 3) for v in sa + sb], "start_pixels": sta,
            "grid_facing": course["facing"], "grid_slots": len(course["grid"]),
            "grid_pixels": grid_px}


def lobby_marker_colours(pal8):
    """The palette entries the lobby draws its own start markers and labels in, read
    out of menu/doslobby.c: the eight seat colours, the idle start, the marker rim, and
    the label ink, which sk_start_ink finds as the entry whose darkest channel is the
    brightest. Returned as {name: index}."""
    try:
        src = open(LOBBY_SOURCE, errors="replace").read()
    except OSError as exc:
        die("cannot read %s, where the lobby keeps its marker colours (%s)"
            % (LOBBY_SOURCE, exc))
    m = re.search(r"SK_PLAYER_COLOUR\[(\d+)\]\s*=\s*\{([^}]*)\}", src)
    if not m:
        die("%s has no SK_PLAYER_COLOUR table; the wall legend cannot be checked "
            "against the lobby's seat colours" % LOBBY_SOURCE)
    # The table's own comments carry RGB triples; only what is outside them counts.
    body = re.sub(r"/\*.*?\*/", " ", m.group(2), flags=re.S)
    seats = [int(v) for v in re.findall(r"\d+", body)]
    if len(seats) != int(m.group(1)) or not all(0 < v < 256 for v in seats):
        die("%s SK_PLAYER_COLOUR does not read as %s palette indices: %r"
            % (LOBBY_SOURCE, m.group(1), seats))
    out = {"seat%d" % n: v for n, v in enumerate(seats)}
    for name in ("SK_START_IDLE", "SK_START_RIM"):
        d = re.search(r"^#define\s+%s\s+(\d+)\b" % name, src, flags=re.M)
        if not d:
            die("%s has no #define %s" % (LOBBY_SOURCE, name))
        out[name] = int(d.group(1))
    out["start_ink"] = max(range(256), key=lambda i: (min(pal8[i * 3:i * 3 + 3]), -i))
    return out


def check_wall_legend(pal8, lut, grounds, start_idxs, tib_index, black, white):
    """The wall legend's claims, checked: an exact palette entry; not green; not the same
    entry as the tiberium legend, a start colour, or the lines' black or white; at least
    START_MIN_SEPARATION from every colour the lobby marks a start with; and clear of
    every theater's mean ground in this run by the start markers' own clearance.

    The bake's own start colours need only be different entries: a course's marked
    plane carries no diamonds, so no start colour of this pack is ever drawn beside a
    wall. The colours that are drawn beside one are the lobby's, which is why those are
    held to the distance."""
    import math
    wall = nearest_index(WALL_RGB, lut)
    got = tuple(pal8[wall * 3:wall * 3 + 3])
    if got != WALL_RGB:
        die("WALL_RGB %r is not a palette entry (nearest is %d, %r)"
            % (WALL_RGB, wall, got))
    r, g, b = WALL_RGB
    if g > r + 24 and g > b + 24:
        die("the wall legend %r is green, which is reserved for tiberium" % (WALL_RGB,))
    if wall in start_idxs or wall in (tib_index, black, white, 0):
        die("the wall legend resolves to index %d, which is already taken" % wall)
    lobby = lobby_marker_colours(pal8)
    lobby["tiberium"] = tib_index
    apart = {k: round(math.dist(WALL_RGB, pal8[v * 3:v * 3 + 3]), 1)
             for k, v in lobby.items()}
    near = min(apart, key=apart.get)
    if apart[near] < START_MIN_SEPARATION:
        die("the wall legend %r is %.1f from the lobby's %s (index %d), closer than %d"
            % (WALL_RGB, apart[near], near, lobby[near], START_MIN_SEPARATION))
    clear = {k: int(round(sum((WALL_RGB[n] - v[n]) ** 2 for n in range(3)) ** 0.5))
             for k, v in grounds.items()}
    if min(clear.values()) < START_GROUND_CLEARANCE:
        die("the wall legend %r is within %d of a theater's mean ground: %r"
            % (WALL_RGB, START_GROUND_CLEARANCE, clear))
    return wall, clear, apart


# ---------------------------------------------------------------------------
# the pack
# ---------------------------------------------------------------------------

def pack_name(s, n):
    try:
        b = s.encode("ascii")
    except UnicodeEncodeError:
        die("%r is not ASCII; the 8-bit surface has no way to print it" % s)
    if len(b) > n:
        die("%r does not fit in %d bytes" % (s, n))
    return b + b"\x00" * (n - len(b))


RECORD_BYTES = 12 + 24 + 12 + 8 + 8 + 4 + MAX_STARTS * 10 + 2 * FRAME_W * FRAME_H
HEADER_BYTES = 8 + 5 * 4 + 768 + 768


def write_pack(path, pal6, pal8, maps):
    with open(path, "wb") as fh:
        fh.write(MAGIC)
        fh.write(struct.pack("<5I", VERSION, len(maps), FRAME_W, FRAME_H, MAX_STARTS))
        fh.write(pal6)
        fh.write(pal8)
        for m in maps:
            if len(m["starts"]) > MAX_STARTS:
                die("%s has %d start positions and a record reserves %d. Raise "
                    "MAX_STARTS rather than dropping starts a player can pick."
                    % (m["scenario"], len(m["starts"]), MAX_STARTS))
            if len(m["plain"]) != FRAME_W * FRAME_H or \
                    len(m["marked"]) != FRAME_W * FRAME_H:
                die("%s: a plane is not %d bytes" % (m["scenario"], FRAME_W * FRAME_H))
            fh.write(pack_name(m["scenario"], 12))
            fh.write(pack_name(m["name"], 24))
            fh.write(pack_name(m["theater"], 12))
            fh.write(struct.pack("<4H", *m["rect"]))
            fh.write(struct.pack("<4H", *m["img"]))
            fh.write(struct.pack("<2H", len(m["starts"]), m["tiberium"]))
            for i in range(MAX_STARTS):
                if i < len(m["starts"]):
                    cx, cy, px, py, col = m["starts"][i]
                    fh.write(struct.pack("<4H2B", cx, cy, px, py, col, 0))
                else:
                    fh.write(b"\x00" * 10)
            fh.write(m["plain"])
            fh.write(m["marked"])
    return os.path.getsize(path)


def readback(path, maps):
    """Re-read the pack with a parser that knows ONLY the format comment at the top of
    this file, and compare every field and every pixel with what was baked. Three
    separate claims are checked, not one: that the bytes round-trip, that the record
    stride really is fixed (the n-th record is found by arithmetic, not by walking),
    and that the reader ends exactly at the last byte with nothing left over."""
    blob = open(path, "rb").read()
    if blob[:8] != MAGIC:
        die("readback: bad magic")
    ver, count, fw, fh, ms = struct.unpack_from("<5I", blob, 8)
    if (ver, fw, fh, ms) != (VERSION, FRAME_W, FRAME_H, MAX_STARTS):
        die("readback: header is %r" % ((ver, fw, fh, ms),))
    if count != len(maps):
        die("readback: %d records, baked %d" % (count, len(maps)))
    if len(blob) != HEADER_BYTES + count * RECORD_BYTES:
        die("readback: file is %d bytes, format says %d"
            % (len(blob), HEADER_BYTES + count * RECORD_BYTES))
    plane = fw * fh
    for i, m in enumerate(maps):
        o = HEADER_BYTES + i * RECORD_BYTES        # by arithmetic, on purpose
        scen = blob[o:o + 12].split(b"\x00")[0].decode()
        name = blob[o + 12:o + 36].split(b"\x00")[0].decode()
        thr = blob[o + 36:o + 48].split(b"\x00")[0].decode()
        if (scen, name, thr) != (m["scenario"], m["name"], m["theater"]):
            die("readback: record %d is %r, baked %r"
                % (i, (scen, name, thr), (m["scenario"], m["name"], m["theater"])))
        o += 48
        rect = struct.unpack_from("<4H", blob, o); o += 8
        img = struct.unpack_from("<4H", blob, o); o += 8
        ns, ntib = struct.unpack_from("<2H", blob, o); o += 4
        if rect != tuple(m["rect"]) or img != tuple(m["img"]):
            die("readback: %s geometry %r %r" % (scen, rect, img))
        if ns != len(m["starts"]) or ntib != m["tiberium"]:
            die("readback: %s counts %d %d" % (scen, ns, ntib))
        for j in range(ms):
            rec = struct.unpack_from("<4H2B", blob, o); o += 10
            if j < ns:
                if rec[:5] != m["starts"][j] or rec[5] != 0:
                    die("readback: %s start %d is %r" % (scen, j, rec))
            elif any(rec):
                die("readback: %s unused start slot %d is not zero" % (scen, j))
        if blob[o:o + plane] != m["plain"]:
            die("readback: %s plain plane differs" % scen)
        o += plane
        if blob[o:o + plane] != m["marked"]:
            die("readback: %s marked plane differs" % scen)
        o += plane
        if o != HEADER_BYTES + (i + 1) * RECORD_BYTES:
            die("readback: %s record stride is wrong" % scen)
    return count


# ---------------------------------------------------------------------------
# verification
# ---------------------------------------------------------------------------

# n64_terrain.py's own placeholder for a water hole in the proof renders it writes. One
# unit of green away from the radar's colour, which is the whole reason the water texels
# are compared separately instead of being allowed to fail the check silently.
TERRAIN_PROOF_WATER = (28, 52, 74)


def verify():
    """Rebuild every tracked terrain_<SCEN>.png with this file's own atlas reader and
    compare texel by texel. Any scenario with both a tracked JSON and a tracked proof
    render is used; there is no list to go stale."""
    import numpy as np
    from PIL import Image

    scens = sorted(f[len("terrain_"):-len(".json")]
                   for f in os.listdir(TERRAIN)
                   if f.startswith("terrain_") and f.endswith(".json")
                   and os.path.exists(os.path.join(
                       TERRAIN, f[:-len(".json")] + ".png")))
    if not scens:
        die("no terrain_<SCEN>.json with a matching terrain_<SCEN>.png in %s, so there "
            "is nothing to check this render against." % TERRAIN)

    print("verifying the ground against n64_terrain.py's own proof renders")
    total = same = holes = holes_expected = 0
    failed = []
    for scen in scens:
        with open(os.path.join(TERRAIN, "terrain_%s.json" % scen)) as fh:
            t = json.load(fh)
        atlas = np.asarray(Image.open(os.path.join(TERRAIN, t["atlas"]))
                           .convert("RGBA")).astype(np.int32)
        ref = np.asarray(Image.open(os.path.join(TERRAIN, "terrain_%s.png" % scen))
                         .convert("RGB")).astype(np.int32)
        w, h = t["width"], t["height"]
        if ref.shape[:2] != (h * CELL_PX, w * CELL_PX):
            die("terrain_%s.png is %dx%d, expected %dx%d"
                % (scen, ref.shape[1], ref.shape[0], w * CELL_PX, h * CELL_PX))
        mine = np.zeros((h * CELL_PX, w * CELL_PX, 3), dtype=np.int32)
        alpha = np.zeros((h * CELL_PX, w * CELL_PX), dtype=np.int32)
        for c in t["cells"]:
            p = atlas[c["py"]:c["py"] + CELL_PX, c["px"]:c["px"] + CELL_PX]
            y, x = c["y"] * CELL_PX, c["x"] * CELL_PX
            mine[y:y + CELL_PX, x:x + CELL_PX] = p[:, :, :3]
            alpha[y:y + CELL_PX, x:x + CELL_PX] = p[:, :, 3]
        opaque = alpha >= 128
        agree = (mine == ref).all(axis=2)
        n_op = int(opaque.sum())
        n_ok = int((agree & opaque).sum())
        n_hole = int((~opaque).sum())
        n_hole_ok = int((~opaque & (ref == TERRAIN_PROOF_WATER).all(axis=2)).sum())
        total += n_op
        same += n_ok
        holes += n_hole
        holes_expected += n_hole_ok
        flag = "OK  " if n_ok == n_op else "FAIL"
        if n_ok != n_op:
            failed.append(scen)
        print("  %s %-8s %-8s opaque %8d/%8d   holes %7d, %7d of them the "
              "placeholder %s"
              % (flag, scen, t["theater"], n_ok, n_op, n_hole, n_hole_ok,
                 TERRAIN_PROOF_WATER))
    print("  OPAQUE TEXELS IDENTICAL, %d scenarios, %d of %d (%.4f%%)"
          % (len(scens), same, total, 100.0 * same / total if total else 0.0))
    print("  water holes are excluded and accounted for: %d of %d carry n64_terrain.py's"
          % (holes_expected, holes))
    print("  placeholder %s, against the radar's %s that this bake paints."
          % (TERRAIN_PROOF_WATER, WATER_RGB))
    if failed:
        die("the ground does not match on: %s" % ", ".join(failed))
    print("  VERDICT: PASS")


# ---------------------------------------------------------------------------
# proof
# ---------------------------------------------------------------------------

def to_rgb_image(plane, pal8):
    import numpy as np
    from PIL import Image
    a = np.frombuffer(plane, dtype=np.uint8).reshape(FRAME_H, FRAME_W)
    pal = np.frombuffer(pal8, dtype=np.uint8).reshape(256, 3)
    return Image.fromarray(pal[a])


def write_pngs(directory, maps, pal8):
    from PIL import Image
    os.makedirs(directory, exist_ok=True)
    written = []
    for m in maps:
        for suffix, key in (("", "marked"), ("-plain", "plain")):
            p = os.path.join(directory, "%s%s.png" % (m["scenario"], suffix))
            to_rgb_image(m[key], pal8).save(p)
            written.append(p)
        p = os.path.join(directory, "%s@4x.png" % m["scenario"])
        to_rgb_image(m["marked"], pal8).resize(
            (FRAME_W * 4, FRAME_H * 4), Image.NEAREST).save(p)
        written.append(p)
    return written


def footprint_panel(m, pal8, scale=3):
    """The size argument, drawn rather than asserted: one preview at 1:1 inside an
    otherwise empty 320x200 screen, so the space left for the list and the buttons is
    the space you can see."""
    from PIL import Image, ImageDraw
    screen = Image.new("RGB", (320, 200), (24, 28, 24))
    d = ImageDraw.Draw(screen)
    d.rectangle([8, 8, 311, 191], outline=(96, 104, 96))
    px, py = 320 - 8 - 4 - FRAME_W, 8 + 4
    # Blitted the way the DOS shape blitter blits: index 0 does not paint, so the
    # letterbox is the dialog and the border is drawn tight to the map's own rect.
    mask = Image.frombytes("L", (FRAME_W, FRAME_H),
                           bytes(255 if v else 0 for v in m["marked"]))
    screen.paste(to_rgb_image(m["marked"], pal8), (px, py), mask)
    ix, iy, iw, ih = m["img"]
    d.rectangle([px + ix - 1, py + iy - 1, px + ix + iw, py + iy + ih],
                outline=(168, 168, 168))
    for row in range(10):
        y = 16 + row * 12
        d.rectangle([16, y, 16 + 150, y + 8], fill=(40, 48, 40))
    return screen.resize((320 * scale, 200 * scale), Image.NEAREST)


def write_proof(path, maps, pal8, scale=3):
    """LOOK AT IT: every preview at 3:1 nearest, over a checkerboard so the margin's
    index 0 shows up as the transparent thing it is, each labelled with what the record
    says about it, and one panel showing the footprint at 1:1 on a 320x200 screen."""
    from PIL import Image, ImageDraw
    cols = 3
    rows = (len(maps) + cols - 1) // cols + 1
    cw, ch = FRAME_W * scale + 16, FRAME_H * scale + 40
    sheet = Image.new("RGB", (cols * cw, rows * ch), (16, 16, 16))
    d = ImageDraw.Draw(sheet)
    fp = footprint_panel(maps[0], pal8, scale=2)
    sheet.paste(fp, (8, (rows - 1) * ch + 8))
    d.text((8, (rows - 1) * ch + 8 + fp.size[1] + 4),
           "footprint: one 120x120 preview at 1:1 on the whole 320x200 screen "
           "(shown at 2:1), with room left for the list", fill=(230, 230, 230))
    for i, m in enumerate(maps):
        cx, cy = (i % cols) * cw, (i // cols) * ch
        bg = Image.new("RGB", (FRAME_W * scale, FRAME_H * scale))
        for y in range(FRAME_H * scale):
            for x in range(FRAME_W * scale):
                bg.putpixel((x, y), (60, 40, 60) if ((x // 12 + y // 12) % 2 == 0)
                            else (40, 30, 40))
        img = to_rgb_image(m["marked"], pal8).resize(
            (FRAME_W * scale, FRAME_H * scale), Image.NEAREST)
        mask = Image.frombytes("L", (FRAME_W, FRAME_H),
                               bytes(255 if v else 0 for v in m["marked"])).resize(
            (FRAME_W * scale, FRAME_H * scale), Image.NEAREST)
        bg.paste(img, (0, 0), mask)
        sheet.paste(bg, (cx + 8, cy + 8))
        d.text((cx + 8, cy + FRAME_H * scale + 12),
               "%s  %s" % (m["scenario"], m["name"]), fill=(230, 230, 230))
        d.text((cx + 8, cy + FRAME_H * scale + 24),
               "%s  %dx%d cells  %d starts  %d tiberium cells%s"
               % (m["theater"], m["rect"][2], m["rect"][3],
                  len(m["starts"]), m["tiberium"],
                  "  race course, %d wall cells" % m["course"]["wall_cells_shown"]
                  if m["course"] else ""), fill=(150, 150, 150))
    sheet.save(path)
    return sheet.size


# ---------------------------------------------------------------------------

def pack_scenarios(path):
    """The scenario of every record in a built pack, in record order, read from the pack's
    own header with nothing but the format comment at the top of this file. A file that is
    not a MAPPREV1 pack of this version, or whose length disagrees with its own header,
    is refused rather than half read."""
    try:
        blob = open(path, "rb").read()
    except OSError as exc:
        die("cannot read %s (%s)" % (path, exc.strerror))
    if len(blob) < 8 + 5 * 4 or blob[:8] != MAGIC:
        die("%s is not a %s pack" % (path, MAGIC.decode()))
    ver, count, fw, fh, ms = struct.unpack_from("<5I", blob, 8)
    if ver != VERSION:
        die("%s is format version %d; this reader knows %d" % (path, ver, VERSION))
    record = 12 + 24 + 12 + 8 + 8 + 4 + ms * 10 + 2 * fw * fh
    if len(blob) != HEADER_BYTES + count * record:
        die("%s is %d bytes; its header (%d records of %d) says %d"
            % (path, len(blob), count, record, HEADER_BYTES + count * record))
    return [blob[HEADER_BYTES + i * record:HEADER_BYTES + i * record + 12]
            .split(b"\x00")[0].decode("ascii", "replace") for i in range(count)]


def manifest_scenarios(path):
    try:
        with open(path) as fh:
            man = json.load(fh)
    except (OSError, ValueError) as exc:
        die("cannot read the manifest %s (%s)" % (path, exc))
    scens = [m.get("scenario") for m in man.get("maps", [])]
    if not scens or not all(scens):
        die("%s lists no maps" % path)
    return scens


def check_pack(path, manifest):
    """--check: every map the committed manifest lists must have a record in the pack.
    Extra records are allowed (a course added in a working copy before its manifest is
    committed); a missing one is the empty lobby panel this exists to stop."""
    want = manifest_scenarios(manifest)
    have = pack_scenarios(path)
    missing = [s for s in want if s not in have]
    if missing:
        print("MAPPREVIEW|STALE|%s carries %d previews and lacks %d of the %d the "
              "manifest lists: %s" % (path, len(have), len(missing), len(want),
                                      " ".join(missing)))
        print("  rebake it:  python3 tools/bake_map_previews.py --resolve-terrain "
              "--out <pack>   (game/make-build.sh does this on its own)")
        return 1
    print("MAPPREVIEW|OK|%s carries all %d previews the manifest lists (%d records)"
          % (path, len(want), len(have)))
    return 0


def resolve_terrain(scenarios):
    """--resolve-terrain: make terrain_<SCEN>.json for every scenario in the run that has
    none, with n64_terrain.py, from the tracked extracted .MAP and .INI. A scenario with
    no .MAP is left for read_terrain to refuse with its own message."""
    todo = [s for s in scenarios
            if not os.path.exists(os.path.join(TERRAIN, "terrain_%s.json" % s))
            and os.path.exists(os.path.join(TERRAIN, "..", "extracted", "MAP", s + ".MAP"))]
    if not todo:
        return
    import subprocess
    print("resolving terrain for %s" % " ".join(todo))
    r = subprocess.run([sys.executable, N64_TERRAIN] + todo, cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout)
        die("n64_terrain.py %s failed" % " ".join(todo))


def main():
    ap = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    ap.add_argument("scenarios", nargs="*", default=None,
                    help="scenario codes; default is the nine retail skirmish maps, "
                         "the game modes' own maps and every race course in game/missions")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--png", metavar="DIR", help="also write plain PNGs, 1:1 and 4:1")
    ap.add_argument("--proof", metavar="FILE", help="write a labelled contact sheet")
    ap.add_argument("--verify", action="store_true",
                    help="check the ground against n64_terrain.py's proof renders")
    ap.add_argument("--check", metavar="PACK",
                    help="refuse a built pack that lacks a map the manifest lists")
    ap.add_argument("--manifest", default=MANIFEST,
                    help="the manifest --check reads (default: the committed one)")
    ap.add_argument("--list", action="store_true",
                    help="print the scenarios a bake with none named covers")
    ap.add_argument("--resolve-terrain", action="store_true",
                    help="make a missing terrain_<SCEN>.json with n64_terrain.py first")
    ap.add_argument("--tib", metavar="PACK",
                    help="the dostib.pack to read (default game/, then playable/)")
    a = ap.parse_args()

    # The two read-only questions answer and stop, and need neither numpy nor Pillow.
    if a.check:
        if a.scenarios or a.png or a.proof or a.verify or a.list:
            die("--check reads a built pack and stops; run it on its own")
        sys.exit(check_pack(a.check, a.manifest))
    if a.list:
        print("\n".join(a.scenarios or default_scenarios()))
        return

    try:
        import numpy  # noqa: F401
        from PIL import Image  # noqa: F401
    except ImportError as exc:
        die("needs numpy and Pillow (%s)" % exc)

    if a.verify:
        # A check and a bake are two different jobs; silently doing only one of them
        # while the flags asked for both is how a green light comes to mean nothing.
        if a.scenarios or a.png or a.proof:
            die("--verify checks the ground and stops. Run it on its own, then run the "
                "bake.")
        verify()
        return

    import numpy as np

    scenarios = a.scenarios or default_scenarios()
    if a.resolve_terrain:
        resolve_terrain(scenarios)

    pal6, pal8 = load_menu_palette()
    lut = build_pal_lut(pal8)
    black = nearest_index((0, 0, 0), lut)

    # Every INI first, so the colour selection knows how many starts this run has to
    # colour before it picks a single one.
    inis = [read_ini(scen) for scen in scenarios]
    need = max(max(len(i["starts"]) for i in inis), START_COUNT)

    grounds = ground_colours({os.path.basename(read_terrain(s)[1]) for s in scenarios})
    colours, separation = pick_start_colours(pal8, grounds, need)
    idxs = [c["index"] for c in colours]
    if black in idxs:
        die("a start colour resolved to the same index as the diamond's rim (%d)" % black)
    tib_index = nearest_index(TIB_RGB, lut)
    if tib_index in idxs:
        die("a start colour is the tiberium legend colour (index %d)" % tib_index)

    tib_art, tib_path, tib_types, tib_frames = load_tiberium(a.tib)

    # The course legend is checked only when a course is in the run: a run of battle maps
    # draws no course and has no business failing over a legend it never uses.
    white = nearest_index((252, 252, 252), lut)
    courses = [i["scenario"] for i in inis if i["rally"]]
    wall_index = wall_clear = wall_apart = None
    if courses:
        wall_index, wall_clear, wall_apart = check_wall_legend(pal8, lut, grounds, idxs,
                                                               tib_index, black, white)

    maps = []
    for ini in inis:
        scen = ini["scenario"]
        terr, atlas_path = read_terrain(scen)
        if terr["theater"] not in ini["theater"] and \
                ini["theater"] not in terr["theater"]:
            die("%s: the INI says theater %s, terrain_%s.json says %s. Nothing else "
                "cross-checks these two, which is exactly why this does."
                % (scen, ini["theater"], scen, terr["theater"]))

        full, hole_cells = render_full(ini, terr, atlas_path, tib_art)
        rgbframe, img = fit_frame(full, ini["rect"][2], ini["rect"][3])
        plane = quantise(rgbframe, lut)

        ix, iy, iw, ih = img
        # The margin is index 0, which the DOS shape blitter skips. Nothing inside the
        # map may be 0, or a hole would appear in the middle of the picture.
        plane[:iy, :] = 0
        plane[iy + ih:, :] = 0
        plane[:, :ix] = 0
        plane[:, ix + iw:] = 0
        inner = plane[iy:iy + ih, ix:ix + iw]
        if (inner == 0).any():
            die("%s: %d pixels inside the map quantised to index 0, the transparent "
                "one" % (scen, int((inner == 0).sum())))

        course = None
        if ini["rally"]:
            course = draw_course(plane, ini, img, wall_index, black, white)
            inner = plane[iy:iy + ih, ix:ix + iw]
            if (inner == 0).any() or plane[:iy, :].any() or plane[iy + ih:, :].any() \
                    or plane[:, :ix].any() or plane[:, ix + iw:].any():
                die("%s: the course drew index 0 inside the map or paint outside it"
                    % scen)

        plain = plane.tolist()
        marked = [row[:] for row in plain]

        x0, y0, cw, chh = ini["rect"]
        starts = []
        for n, cell in enumerate(ini["starts"]):
            cx, cy = cell % 64, cell // 64
            if not (x0 <= cx < x0 + cw and y0 <= cy < y0 + chh):
                die("%s: start %d is cell (%d,%d), outside the [MAP] rect %r"
                    % (scen, n, cx, cy, ini["rect"]))
            px = ix + int((cx - x0 + 0.5) * iw / float(cw))
            py = iy + int((cy - y0 + 0.5) * ih / float(chh))
            col = colours[n]["index"]
            # On a course the grid is the start marker; see RACE COURSES at the top.
            if course is None:
                stamp_start(marked, px, py, col, black)
            starts.append((cx, cy, px, py, col))

        maps.append({
            "scenario": scen, "name": ini["name"], "theater": ini["theater"],
            "rect": list(ini["rect"]), "img": list(img), "starts": starts,
            "tiberium": len(ini["tiberium"]), "hole_cells": hole_cells,
            "course": course,
            "plain": np.asarray(plain, dtype=np.uint8).tobytes(),
            "marked": np.asarray(marked, dtype=np.uint8).tobytes(),
        })
        print("  %-8s %-18s %-9s  rect %2d,%2d %2dx%2d  fit %3dx%3d at %2d,%2d  "
              "%2d starts  %4d tiberium  %4d water cells"
              % (scen, ini["name"][:18], ini["theater"], x0, y0, cw, chh,
                 iw, ih, ix, iy, len(starts), len(ini["tiberium"]), hole_cells))
        if course:
            print("           race course: %d wall cells, finish %d px, "
                  "start line %d px, %d grid slots facing %d"
                  % (course["wall_cells_shown"], course["finish_pixels"],
                     course["start_pixels"], course["grid_slots"],
                     course["grid_facing"]))

    size = write_pack(a.out, pal6, pal8, maps)
    n = readback(a.out, maps)

    manifest = {
        "pack": os.path.basename(a.out),
        "magic": MAGIC.decode(),
        "version": VERSION,
        "bytes": size,
        "count": n,
        "frame": [FRAME_W, FRAME_H],
        "max_starts": MAX_STARTS,
        "header_bytes": HEADER_BYTES,
        "record_bytes": RECORD_BYTES,
        "palette": "embedded in TITLE.CPS (data/dosdata/LOCAL.MIX), the menu's own",
        "palette_widening": "v << 2",
        "quantiser": "cnc_eyes.cpp pal_index: nearest in RGB over a 5:5:5 cube, "
                     "index 0 excluded",
        "margin_index": 0,
        "water_rgb": list(WATER_RGB),
        "tiberium_mask_source": tib_path,
        "tiberium_shapes": tib_types,
        "tiberium_frames": tib_frames,
        "tiberium_legend_rgb": list(TIB_RGB),
        "tiberium_legend_index": tib_index,
        "start_colours": colours,
        "start_colour_min_separation": round(separation, 1),
        "start_colour_ground_clearance": START_GROUND_CLEARANCE,
        "theater_mean_ground": grounds,
        "marker": {"shape": "diamond", "rim_radius": MARK_RIM,
                   "fill_radius": MARK_FILL, "rim_index": black},
        "not_drawn": ["TERRAIN", "SMUDGE", "STRUCTURES", "UNITS", "INFANTRY"],
        "maps": [dict({"scenario": m["scenario"], "name": m["name"],
                       "theater": m["theater"], "cell_rect": m["rect"],
                       "image_rect": m["img"], "starts": len(m["starts"]),
                       "start_cells": [[s[0], s[1]] for s in m["starts"]],
                       "tiberium_cells": m["tiberium"],
                       "cells_touching_water": m["hole_cells"]},
                      **({"race_course": m["course"]} if m["course"] else {}))
                 for m in maps],
    }
    if courses:
        manifest["race_course_legend"] = {
            "courses": courses,
            "wall_overlays": list(WALL_TYPES),
            "wall_rgb": list(WALL_RGB), "wall_index": wall_index,
            "wall_clearance_from_ground": wall_clear,
            "wall_distance_from_lobby_markers": wall_apart,
            "lines": "checkered, %d px across in 1 px squares, black index %d and white "
                     "index %d" % (COURSE_LINE_PX, black, white),
            "start_line": "rly_draw_ground's construction: %.2f cells ahead of the "
                          "grid, %.2f cells past it a side, or P000's half width"
                          % (START_LINE_AHEAD, START_LINE_MARGIN),
            "grid_slot": "one pixel, index %d" % black,
            "marked_plane": "the same as plain: no start diamonds on a course",
            "not_drawn": ["course centre line", "defences", "crate spots"],
        }
    mpath = os.path.splitext(a.out)[0] + ".manifest.json"
    with open(mpath, "w") as fh:
        json.dump(manifest, fh, indent=1, sort_keys=True)

    if a.png:
        w = write_pngs(a.png, maps, pal8)
        print("wrote %d PNGs into %s" % (len(w), a.png))
    if a.proof:
        sz = write_proof(a.proof, maps, pal8)
        print("wrote proof sheet %s (%dx%d)" % (a.proof, sz[0], sz[1]))

    print("wrote %s: %d previews, %d bytes, readback clean" % (a.out, n, size))
    print("wrote %s" % mpath)


if __name__ == "__main__":
    main()
