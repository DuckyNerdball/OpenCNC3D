# Sun map: keeping the still casters (not done, and why)

The proposal: split the sun map's caster loop (`fx_draw_shadow_casters` in
`game/cnc_eyes.cpp`, called from `fx_render_shadow_map` in `game/fx_post.h`) into still
casters and moving ones. The still ones (buildings at rest, walls, wrecks at rest, and trees
and foliage when they do not sway in the sun's pass) would be drawn into a second depth
target, kept under a key of the sun matrix, the map size, the terrain caster serial and a
revision of the still set. While the key held, that depth would be copied into the sun map
with a depth blit and only the moving casters drawn over it. A depth-only pass with
`GL_LEQUAL` keeps the least depth per texel whatever the draw order, so the result would be
exact.

The renderer is **unchanged** on this branch. Measured on both platforms, every caster that
could be still during play is worth at most about 1.1 to 1.4 ms of sun map on Windows on a
damaged base (td_base) and 0.3 to 0.5 ms on the Mac, and 0.1 to 0.6 ms on the other scenes
measured. Most of that is `ebld_draw`, the remastered buildings drawn piece by piece, and
almost none of it is the part of `ebld_draw` a cache could keep: its standing runs. The
expensive casters are the swaying trees, which take a new pose on every presented frame, so
no cache keyed on stillness can keep them. This page records the measurements so the idea is
not taken up again on the strength of the frame benchmark alone, and points at where the sun
map's time really goes.

## Where the caster loop's time goes

The object loop was timed by kind with the frame profiler's own marks in a scratch build
(one run each, 1600x900, Mac, `framebench 60` after 30 warm frames; the per-object marks add
some overhead of their own). CPU milliseconds per frame, opaque pass + cut-out pass:

| caster | td_base | ra_snow |
|---|---|---|
| trees (`K_TERRAIN`, remastered trees and clumps) | 0.50 + 0.80 | 1.46 + 2.71 |
| remastered buildings' pieces (`ebld_draw`)\* | 0.23 to 0.29 | about 0 |
| vehicle wrecks (`evhc_draw_wrecks`) | 0.45 + 0.25 | 0.01 + 0.01 |
| soldiers' cards and corpses | 0.55 | 0.41 |
| infantry objects in the mesh loop | 0.37 | 0.38 |
| vehicles (`K_UNIT`) | 0.19 | 0.05 |
| buildings (`K_BUILDING`) | 0.04 + 0.02 | 0.42 + 0.07 |
| walls, fields, crates, Red Alert's ore and interiors, bushes | under 0.1 | 0.15 |

\* `ebld_draw` draws every remastered building that is not simply whole: held and damaged
buildings with their loose pieces, wrecked ones, and tree snags. It had no mark in the
scratch build, so its row is the drop in `sun shadow map` when it alone is left out of the
sun's pass (the ceiling runs below, at 1600x900 and 3008x1692), not a mark of its own. On
Windows the same drop is 1.0 to 1.3 ms on td_base and td_base_close, which makes it the
largest still caster on a base scene there.

So the trees are most of the loop: about 1.3 ms of it on td_base and 4.2 ms on ra_snow.

## The ceiling, measured on both platforms

Four scratch variants of the parent renderer bound what any version of this cache could
save, because each simply leaves casters out of the sun's pass (which changes the picture, so
none is a candidate itself):

- **still out**: every still candidate left out of the sun's pass: `K_BUILDING` objects,
  `ebld_draw` (all of it), wrecks, walls, fields, crates, interiors and ore. Trees, bushes,
  units and soldiers kept. A cache can only save less than this, since it still pays for its
  blit, its key and its moving casters, and `ebld_draw` includes moving pieces, flags and
  liquids that no cache could keep.
- **trees out**: the same, and the trees and bushes left out too.
- **ebld out**: `ebld_draw` alone left out.
- **standing out**: only the standing runs inside `ebld_draw` left out: the pieces
  `ebld_draw_entry` already sorts as still with `ebld_stand_moves` and merges into runs with
  `ebld_draw_standing`. That is exactly what a still layer for this group could keep.

The first measurement of this ceiling left `ebld_draw` in the sun's pass under "still out",
and so put the still casters at 0.1 to 0.75 ms on Windows and nothing measurable on the Mac.
On a base scene `ebld_draw` is the largest still caster, so those figures were several times
too low; the tables below replace them.

Run with the same frozen graphics settings for every variant, `framebench 60` after 20 or 30
warm frames, variants alternated in back-to-back runs with the order reversed between rounds.
Frame time medians in ms; the sections are CPU ms per frame.

**Windows** (RTX 3070, i5-12400F, 32-bit build, 1600x900, two rounds of three windows;
every run reported the NVIDIA renderer):

| scene | frame: parent / still out / trees out / ebld out / standing out | sun shadow map, same order |
|---|---|---|
| td_base | 39.23 / 36.76 / 35.64 / 37.12 / 37.87 | 4.47 / 2.53 / 0.53 / 2.67 / 3.58 |
| td_base_close | 34.20 / 33.30 / 32.80 / 33.49 / 35.04 | 3.35 / 2.20 / 0.50 / 2.32 / 3.23 |
| ra_snow | 26.00 / 25.66 / 20.98 / 26.67 / 26.55 | 8.12 / 7.53 / 0.42 / 8.43 / 8.49 |

The parent's first three windows on td_base ran slow (4.9 to 5.4 ms of sun map against 3.7
to 4.0 after), so its median there is high: read the parent as about 3.8 ms. A control
session that ran five arms of the same parent renderer measured 3.77 to 3.81 ms on td_base,
3.25 to 3.41 on td_base_close and 8.00 to 8.43 on ra_snow, which is the noise between arms.

Where the trees' cost goes when they leave the sun's pass (parent / still out / trees out):

| scene | objects (opaque) | trees + foliage |
|---|---|---|
| td_base | 1.61 / 1.56 / 2.13 | 0.17 / 0.17 / 1.19 |
| td_base_close | 1.27 / 1.14 / 1.60 | 0.17 / 0.16 / 1.08 |
| ra_snow | 0.93 / 0.96 / 1.89 | 0.24 / 0.24 / 2.79 |

Split by group in separate sessions on td_base (Windows, 1600x900, `sun shadow map` against
a parent of 3.74 to 3.93 ms): `K_BUILDING` objects alone -0.09, wrecks alone +0.08, walls
alone -0.34, `ebld_draw` alone -1.26, everything still except `K_BUILDING` objects -1.38.
The other scenes, from the first of those sessions: still out -0.29 on ra_snow, -0.31 on
ra_interior and -0.14 on td_desert_base.

**Mac** (shared machine, so whole-frame medians moved by several ms between rounds and only
the sections are worth reading; three rounds of three windows on td_base and td_base_close,
two on the others):

| scene, size | sun shadow map: parent / still out / trees out / ebld out / standing out | objects (opaque): parent / still out / trees out | trees + foliage, same |
|---|---|---|---|
| td_base 1600x900 | 1.89 / 1.56 / 0.30 / 1.60 / 1.89 | 0.90 / 0.90 / 1.10 | 0.49 / 0.47 / 0.91 |
| td_base_close 1600x900 | 1.64 / 1.24 / 0.27 / 1.27 / 1.59 | 0.68 / 0.71 / 0.88 | 0.40 / 0.39 / 0.77 |
| td_base 3008x1692 | 1.89 / 1.43 / 0.28 / 1.66 / 1.81 | 0.89 / 0.93 / 1.12 | 0.48 / 0.48 / 0.94 |
| ra_snow 1600x900 | 3.60 / 3.23 / 0.24 / 3.63 / 3.56 | 0.59 / 0.63 / 0.98 | 0.27 / 0.27 / 1.30 |

What that says:

- **The still casters are worth about 1.1 to 1.4 ms of sun map on Windows on a damaged
  base**, and 1 to 2.5 ms of frame there across four sessions and both base views (frame
  medians on Windows move by about 1 ms between windows of the same arm). That is an upper
  bound, since it still includes `ebld_draw`'s moving pieces, flags and liquids. On the Mac
  they are worth 0.3 to 0.5 ms of sun map. On scenes with few damaged buildings they are small
  on both platforms: 0.1 to 0.6 ms of sun map on Windows (ra_snow, ra_interior,
  td_desert_base), 0.4 ms on the Mac's ra_snow.
- **Most of it is `ebld_draw`, and not its standing runs.** Leaving out the standing runs
  alone moves the sun map by 0.1 to 0.4 ms on Windows' base views (nothing measurable on
  ra_snow) and by 0.0 to 0.08 ms on the Mac. What `ebld_draw` spends in the sun's pass is in
  its other parts: the pieces that are off a building (chips and loose parts, each drawn with
  its own transform), the standing pieces that move, the liquids, and the flags' wave pose.
  Unlike the trees, leaving it out of the sun's pass moved no measurable cost to the colour
  pass.
- **The trees are the sun map.** Counted alone (still out minus trees out) they are
  2.0 to 2.3 ms of td_base's sun map and 7.1 to 7.5 ms of ra_snow's on Windows (over this
  run and an earlier one), and 1.0 to 1.3 ms (td_base) and 3.0 ms (ra_snow) on the Mac.
  Taken out, part of their cost moves to the colour pass instead of going away (objects and
  trees + foliage rise by 1.5 to 1.6 ms on td_base and 3.5 to 3.6 ms on ra_snow on Windows),
  and what is left of the trees alone is a net 3.7 to 4.7 ms a frame on ra_snow on Windows
  and about 1.6 ms of sections on the Mac. That remainder is the trees that stand only in the
  sun's box and the sun pass's own submission of them. The box is about twice the view
  across, and the geometry uploads count those trees: on ra_snow 140 a frame with the trees in the sun's pass
  and 78 without, so 62 (about 44 %) are trees posed only for the sun; on td_base 151 and
  147, almost none.

## Why the trees cannot be kept

A remastered tree's wind clock is `engine_time() / 15` plus a per-cell phase
(`ebld_draw_object`, `g_meshSwayT`), and `engine_time()` is the engine tick plus the frame's
interpolation alpha. In live play the alpha advances on every presented frame, so every
swaying tree has a new pose every frame, in the sun's pass as in the colour pass. A tree is
still only when the wind is off.

**The 140 buffer uploads a frame on ra_snow are that wind pose**, not a colour rebuild like
the ember glow's. The sun's pass is the first pass in the frame to pose each swaying tree,
and its positions-only pose is the one the later passes share. Taking the trees out of the
sun's pass shows it: the cost moves to the colour pass instead of going away (the tables
above).

**The frame benchmark hides this.** `framebench` draws the same frame N times with the engine
standing still, so `engine_time()` does not move and every tree is still. A key that included
the engine clock would hit on every benchmark frame and in a paused game, and report a saving
of 2 to 8 ms that live play never sees.

## Why the rest is not cleanly still either

- **Wrecks sink.** `evhc_draw_wrecks` places each wreck from `evhc_sink(E, V, engine_time())`,
  so a wreck moves on the interpolated clock until it settles, and V2 rockets in flight are
  drawn from the same call (`evhc_draw_flights`).
- **Buildings animate on the same clock.** Flags wave on `engine_time() / 15`
  (`ebld_wave`), the Construction Yard's fans turn on the engine frame, clips run from
  `object_anim_t`, and a building going up, being sold, docking a harvester or breaking up
  is drawn by other paths (`eplc_draw_object`, `evhc_dock_draw`, `shatter_owns`). For the
  whole `K_BUILDING` objects each of those is decided inside functions the caster loop only
  calls.
- **`ebld_draw` has its classifier, but it is not complete for a cache.** `ebld_draw_entry`
  already splits the standing pieces that move from the ones that do not with
  `ebld_stand_moves`, so for this group the stillness rule is in one place. It does not cover
  the flags: `ebld_wave_c` poses a flag's cloth triangles in place inside the standing runs on
  every frame, so a layer keyed on `ebld_stand_moves` alone would freeze every flag's shadow.
  A layer for these runs would also have to leave out the waved triangles
  (`gpuWaveTris`) and be keyed per building on its stage, its pieces still standing and its
  place, and the measurement above says what it would buy for all that: 0.1 to 0.4 ms of
  Windows' sun map at most, and nothing measurable on the Mac.

A classification at the caster loop would have to restate each of those rules from outside
the code that owns them, and one missed rule is a shadow that freezes while the camera is
still. The fixed picture set would not catch it, because its shots are taken one frame after
the camera is placed.

## If it is taken up again

- The saving is in the trees, so the work is in the wind pose, not in the sun map. Posing each
  swaying tree once a frame is already shared between passes; what remains is the CPU pose in
  `draw_mesh`'s wind path and one buffer upload per tree per frame, which Windows' driver
  charges for heavily. Swaying the trees in a vertex program from their rest pose would remove
  both, but its floating point differs from the CPU pose's, so it changes pixels and would
  need its own dial and review.
- The trees that stand only in the sun's box cost the sun pass alone (about 44 % of the tree
  uploads on ra_snow). A tighter box per tree (whether its shadow can reach anything the light
  pass reads) changes which texels are written, and the light shafts read the map along the
  view rays, away from the ground, so it is not a byte-identical change either.
- The second target is `ebld_draw` on a damaged base, 1.0 to 1.3 ms of Windows' sun map on
  td_base, but not through a still layer: its standing runs are worth 0.1 to 0.4 ms on
  Windows and nothing measurable on the Mac, before the layer pays for a 4096 by 4096 depth
  copy each frame and its key. What it spends
  is in its loose pieces, moving pieces, liquids and flag pose, and a profile of those parts
  inside the sun's pass is where to start. A still layer for the whole `K_BUILDING` objects,
  wrecks and walls together is worth at most about 0.4 ms of Windows' sun map and is not
  worth its frozen-shadow risk.
- Any version needs a verify mode that draws the full pass into a scratch target on each hit
  frame and counts differing texels, run over scenes with play running and the camera still
  (units moving, a building placed, one destroyed, a wreck sinking, a shroud reveal, a flag
  waving), because the picture set alone cannot see a frozen shadow.
