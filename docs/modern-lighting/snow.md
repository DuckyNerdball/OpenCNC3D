# Snowfall

Feature key: `snow`. Master dial: `snow_fx`. Branch: `visual/modern-snow`. Code:
`game/snow_mod.h` (its header comment is the design), dials in `game/fx_state.h` (group
`3d2 SNOWFALL`), drawn from `draw_frame` in `game/cnc_eyes.cpp` beside the rain.

## What the player sees

On Red Alert's snow maps, and only there, snow falls over the battlefield: small soft
white flakes, a few pixels across, falling slowly, each one fluttering from side to side on
its own rhythm while the whole fall drifts on a light wind. Flakes near the camera are
larger than far ones, and the far ones are fainter. It is clearly snowing without being a
blizzard. Against open sunlit snow a flake is nearly invisible, as real snowfall is; it
shows against water, rock, roads, shadows, trees and units. A hill or a building hides the
snow behind it, the shroud's black covers it over unexplored ground (a faint flake can show
through the soft rim of the Enhanced fog, as a raindrop can), and no flake is seen over the
black beyond the edge of the map. The flakes stay where they are in the world while the
view pans and while it zooms, and about the same number are on screen at every zoom.

A flake that reaches the ground comes to rest there: it lies still on the real ground
where it landed (on top of a hill, not inside it; in a valley, not above it), with no
more drift or flutter, and fades out smoothly over three seconds (`snow_settle`) before
it falls again from the top. On open water it is gone within a third of a second. The
fall is the same length over high ground as over low, because it starts the volume's
height above the ground it will land on.

The number of flakes IN THE AIR is what the amount dial gave before flakes could rest,
but more of the snow is SEEN than in the version the amount was tuned on, for three
reasons: a falling flake is no longer hidden inside a hill, it no longer fades over the
last tenth of its fall, and the resting flakes come on top. Measured at the same ticks
against the snow off, at the defaults: about twice the flake dots on screen and 1.6 to 1.8
times the white (613 dots a frame against 305 at the nearest zoom, 653 against 373 at the
standard view); the falling flakes alone are 12 to 15 percent more dots. It still reads as
light snow. If it should look as it was tuned, the amount's default is the dial to lower;
that has not been changed.
Being a uniform volume seen in perspective, the snow is denser toward the horizon than in
the foreground; that is by design.

It never falls in Tiberian Dawn (its winter maps included), on Red Alert's temperate or
interior maps, or on the editor's SNOW theater. The gate is `snow_theater()`: the Red Alert
module loaded and the pack's theater `RASNOW`.

## The dials

F5 panel, WEATHER tab, group `3d2 SNOWFALL` (right after the rain's place in the table;
the rain itself has no panel rows). One setting for every theater, not a lighting row, so
`fx_lighting_param` keeps no per-theater copy and no saved theater slot moves. A settings
file that lacks these keys loads them at their defaults.

| key            | default | range       | meaning |
|----------------|---------|-------------|---------|
| `snow_fx`      | 1       | 0 or 1      | the master; 0 draws exactly what drew before |
| `snow_amount`  | 0.75    | 0..1        | how many flakes in the air: the density of 6000 times the square of this (0.75 is 3375) in a lattice cut tight to the view; 1 is the heaviest, a steady moderate snowfall, not a blizzard. Flakes lying on the ground come on top of these |
| `snow_size`    | 0.09    | 0.01..0.30  | cells across a flake at the standard zoom; follows the zoom so a flake keeps its screen size |
| `snow_speed`   | 1.4     | 0.2..6      | cells a second a flake falls at the standard zoom (each flake one of four speeds, 0.75 to 1.5 times this); the fall keeps its speed on screen as the view zooms |
| `snow_sway`    | 0.18    | 0..1        | cells of sideways flutter |
| `snow_flutter` | 1       | 0.25..3     | how quickly a flake flutters |
| `snow_wind`    | 120     | 0..360      | the bearing the snow drifts toward, read like the cloud bearing |
| `snow_drift`   | 0.35    | 0..2        | the wind's strength: sideways cells per cell of fall |
| `snow_height`  | 6       | 1..12       | cells above the ground the fall starts at the standard zoom |
| `snow_opacity` | 0.85    | 0..1        | how solid and bright a flake is |
| `snow_settle`  | 3       | 0..10       | seconds a landed flake lies still where it landed, fading out smoothly, before it falls again (a third of a second at most on open water); 0 fades it over the last tenth of its fall instead, as before flakes could rest. Shortened only where the lattice cannot hold that many resting flakes on top of the falling ones: at the default amount and speed never; at `snow_amount 1` it holds at most 4.2 s, at `snow_speed 6` with amount 1 about 1 s (see Clock) |

## How it works

- **One lattice, built once, one draw call.** 65536 quads hold only seeds (three in the
  vertex beside the flake's index, two more beside the corner in the texture coordinate),
  put into a static vertex buffer once (8 MiB; the buffer loaders are the grass bake's).
  The first 32768 carry the seeds the earlier, smaller lattice had.
  The amount dial and the zoom choose how many of them are drawn; nothing is uploaded or
  rebuilt per frame. A driver without buffer objects keeps the client array, which draws
  the same picture and sends the lattice each frame. A vertex shader turns a seed and the
  clock into a flake.
- **Where a flake is.** Its landing point sits on a periodic lattice and the shader takes
  the copy nearest the middle of the view, so panning never drags it; the copy changes
  only half a period from the middle, off screen. Its height is a phase of its fall, above
  the ground under the point it will come to rest on. The
  wind carries it sideways in proportion to the height still to fall, so the drift is
  steady and it still lands on its point. Its flutter is a small sideways ellipse with a
  whole number of turns per fall, its own axis, size and starting point, so it lands
  exactly where the ellipse started; that point, the lattice point plus the flutter's
  starting offset, is where it rests.
- **At rest.** After its fall a flake holds its landed position (the fall's progress is
  held at its end, so the wind's slant and the flutter both stop where they were) for
  `snow_settle` seconds while its alpha eases from full to nothing (a smoothstep over the
  whole time), then starts its next fall from the top. Where the water field says open
  water (the same field and threshold the rain's crowns use, asked at the resting point)
  the rest lasts at most 0.3 s. The field is the map's coastline built from the water
  cut, whichever sea draws the water, and the light pass keeps it current with or without
  the Enhanced sea, so the test holds with `water_fx 0` too. A flake is lifted by its own
  radius and, near the ground, its depth is taken up to a flake's width toward the eye:
  the sprite is flat across the view, so on a slope rising toward the camera the ground
  in front of its lower half would otherwise be nearer than its centre and cut it in
  half. The pull eases in over the last two flake widths of the fall and is whole at
  rest; a flake higher up keeps its own depth, so it never draws in front of the edge of
  a unit or a building it is just behind. A hill or a building in front of a resting
  flake still hides it.
- **The ground under the snow.** `terrain_y`, the renderer's own ground height (corner
  heights, building pads, the Enhanced relief surface, the water's surface over water; the
  function that stands units, props and trees on the map), is sampled four times a cell
  over the play rectangle into a 16-bit luminance texture (297 by 153 texels on Soviet 1),
  rebuilt only when one of its inputs moves: a new pack or ground generation, the pad
  epoch, the relief surface's generation, the water plane's epoch, the terrain base, or
  the play rectangle. A building pad redoes only the cells it moved: the ground's revision
  stamps (`g_groundRev`, which `ground_rev_sync` issues on the frames the pad epoch moves)
  say which cells could have changed, those texels are sampled again and only their rows
  are sent with `glTexSubImage2D`. Anything else, a stamp that says "everything", a new
  height outside the range the 16 bits cover, or a relief move on a frame the pads did not
  move (the stamps owe those until the next pad change, so the snow watches for them every
  frame, drawn or not), rebuilds the whole texture. `OPENCNC3D_GROUND_PATCH_VERIFY=1`
  samples every texel again after a patch and logs how many differ. The vertex shader
  reads the texture with `texture2DLod` at the resting point, so the whole fall stands on
  that ground: it is the dial's length over a hill as over a valley, and it ends on the
  ground. `GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS` is asked before the program is linked, and
  the reads the GL cannot do are left out of the shader (`SNOW_GROUND` and `SNOW_WATER`):
  with no units the snow still draws and ends at the terrain's zero, with one it has the
  ground but no water test. The texture is on unit 1 and the water field on unit 2,
  unbound again after the draw.
- **Fixed under a pan.** Everything that follows the zoom is read from the eye's
  distance to the ground it looks at, never the visible rectangle. The camera rides the
  terrain, so that rectangle changes by a cell or so as the view crosses a rise; sized
  from it (as the rain is), every flake jumped at every step of a pan.
- **Fixed under a zoom.** The lattice period is fixed in cells, sized for the farthest
  view the camera rig allows (with a tenth to spare), and the phase of every fall runs
  against the height dial, not the zoomed height, with the clock folded by that dial. A
  zoom step therefore moves a flake only by the change in its drawn height and in the
  wind's slant over it, a small fraction of a cell. Sizing the period and the phase from the zoom (the
  first version) moved every flake to a new place at every zoom step, and more so the
  longer the session had run. What a zoom does change: the size and the drawn height
  follow it against the standard view (13.3 cells from the eye), so a flake is the same
  size on screen at every zoom and the volume never reaches past 0.45 of the eye's
  distance; and the drawn count follows it, because zoomed in the fixed period holds more
  lattice than the view needs, so more of it is drawn (up to 4.4 times the far view's
  count at the nearest zoom) to keep the density. The drawn range ends in a 64-flake
  ramp of alpha, so a zoom fills or thins the snow one faint flake at a time.
- **Round on screen, with a floor.** The sprite is built in clip space around the
  projected centre, sized from the projection of the flake's world size. Under a pixel and
  a half it is drawn at that floor with its alpha reduced by the area it lost, so a far
  flake stays a faint dot instead of flickering as the anti-aliasing jitter moves it; a
  ceiling of nine window points of radius (eighteen pixels on a Retina backing) stops a
  flake at the lens filling the screen. The floor counts pixels of the scene target,
  because it exists for the anti-aliasing's pixel grid; the ceiling counts points,
  because it is a size on the display. The fragment is a solid core easing to nothing at
  the rim.
- **The map's edge.** Beyond the play rectangle is the clear colour, and nothing covers
  it. A flake is faded by the nearest to the edge of three ground points: its landing
  point, its own column, and the ground seen behind it on screen, where the eye's ray
  through the flake meets the ground plane. The last one matters at the far edge: a
  flake a few cells up projects several cells further up the screen than its landing
  point, which near the top of the map is over the void.
- **Clock.** A flake of speed class c (0.75, 1, 1.25 or 1.5 times the speed dial) falls
  for `H / speed` seconds, H the height dial, then rests `snow_settle` seconds: its cycle.
  Each class gets `g_fxTime` folded by its own cycle, so every phase is exactly the one
  the unfolded clock would give and no phase ever coarsens. One cycle per class, rather
  than one common cycle the faster classes would sit out invisible for part of, keeps
  every drawn flake falling or fading all the time, so fewer have to be drawn (a common
  cycle of the slowest fall plus the rest needs 2.14 times the old count at the
  defaults, this needs 1.77), and `snow_settle 0` is the motion exactly as it was before
  flakes could rest. The drawn count is divided by the share of a cycle a flake spends in
  the air, averaged over the four classes (0.566 at the defaults), so the number FALLING
  is the number the amount dial gave before; the flakes lying on the ground are extra.
  That division must leave the count inside the 65536 of the lattice, or the cap would
  take flakes out of the air as well. So the rest is worked out against the count the
  rig's NEAREST zoom needs (where the most of the lattice is drawn) and, when the dial's
  rest would pass the lattice there, shortened to the longest that fits (found by
  halving; logged as `SNOW|settle|dial=..|held=..`). It depends only on the dials and the
  window's shape, never on the current zoom, so a zoom cannot change a cycle and move a
  flake. At 1600x900: the default amount holds the whole dial (10 s needs 62,604 of
  65,536); `snow_amount 1` holds 4.19 s; `snow_amount 1` with `snow_speed 6` holds 0.98 s.
  Only a camera nearer than the rig's nearest zoom can still reach the cap.
  The seeds are a fixed LCG. Two runs of a script are
  one picture. Being the engine's clock, it stops while the game is paused, runs faster at
  a faster game speed, and steps once a tick when smooth animation is off, as the rain and
  every other effect on the world does; that is accepted.

## Where it is drawn, and why

Inside the world pass, straight after the rain's streaks and splashes: after every object
and effect, before the shroud blanket (whose black covers the snow over unexplored ground;
the Enhanced fog's soft rim is a mask the snow does not read, so a faint flake can show
through it there, as rain does),
depth tested with depth writes off, and between the two scene copies the anti-aliasing
takes around the effects (`FX_TAA_SNAP_FX0` and `FX_TAA_SNAP_FX1`). The reactive mask built
from those copies therefore counts every flake as an effect: the blend takes the current
frame where a flake is or was, so a falling flake stays a crisp dot instead of smearing
into a trail or fading out.

The light pass then shades a flake's pixels as the ground behind it, as it does the rain.
That was chosen over the two alternatives, a draw after the light pass (outside both the
shroud and the reactive mask, so it would need its own copy of each) and a flag in the
scene's alpha for the light pass to undo (which reaches into every lighting term of that
shader, and the light program already uses all sixteen texture units). On a snow map the
effect is mild: the ground behind most flakes is snow, which the light pass keeps at its
own brightness in the sun and cools in shade, so a flake over a cast shadow takes the
shadow's tone and stays as much brighter than the shadow as it is brighter than open snow,
which reads as a flake falling through the shade. The flake colour (0.90, 0.92, 0.95) is
set a little below white, so the lift a sunward slope gets can carry a flake's core to
white but not into the bloom's halo.

## Cost

`framebench` on Soviet 1's `ra_snow` camera, snow off, the default and `snow_amount 1`
alternating in one process, six rounds of 40 frames each, medians, at the standard view
(dist 3400) and at the nearest zoom (dist 1920, where the most of the lattice is drawn):

| window    | view     | snow off | default  | `snow_amount 1` | paired default - off | paired full - off |
|-----------|----------|----------|----------|-----------------|----------------------|-------------------|
| 1600x900  | standard | 17.13 ms | 17.95 ms | 17.65 ms        | +1.03 ms             | +0.84 ms          |
| 1600x900  | nearest  | 16.78 ms | 16.90 ms | 16.84 ms        | +0.10 ms             | +0.02 ms          |
| 3008x1692 | standard | 29.79 ms | 29.38 ms | 30.07 ms        | +0.13 ms             | +0.38 ms          |
| 3008x1692 | nearest  | 28.02 ms | 27.23 ms | 27.86 ms        | -0.36 ms             | +0.08 ms          |

The nearest view draws about 4.4 times the flakes of the standard one and reads no
slower, and the larger window reads no slower than the smaller; the one row near 1 ms is
inside the spread between rounds of the same setting on a machine doing other work. A
GPU-serialized frame profile of the section that holds the snow and the shroud put the
snow's own cost at about 0.05 to 0.1 ms with the client-array lattice; the static buffer
removes the per-frame copy that version made.

The lattice sits in a static vertex buffer; nothing is sent per frame but the uniforms.
Zoomed in, up to 4.4 times as many flakes are vertex-shaded (most of them off screen and
discarded as degenerate quads), which is vertex work only; the fill, a few pixels for each
of a few hundred visible flakes, is the same at every zoom.

With flakes at rest: the same camera, `snow_settle 3`, `snow_settle 0` and `snow_fx 0`
alternating in one process, six rounds of 40 frames, medians, while other renderer jobs
shared the machine:

| window    | view     | settle 3 | settle 0 | snow off | paired 3 - 0 | paired 3 - off |
|-----------|----------|----------|----------|----------|--------------|----------------|
| 1600x900  | standard | 17.34 ms | 17.05 ms | 17.53 ms | +0.27 ms     | -0.22 ms       |
| 1600x900  | nearest  | 14.59 ms | 13.80 ms | 14.74 ms | +1.04 ms     | +0.35 ms       |
| 3008x1692 | standard | 32.30 ms | 34.61 ms | 32.97 ms | -2.52 ms     | -1.55 ms       |
| 3008x1692 | nearest  | 26.32 ms | 27.10 ms | 27.16 ms | -0.78 ms     | -1.09 ms       |

Every difference is inside the spread between rounds of one setting (settle 0 reads
faster than the snow off in one row and slower in another), so the rest costs nothing
this method can see. What it adds is vertex work: 1.77 times the flakes at the defaults
(about 10,700 drawn at the standard view, 31,500 at the nearest, against 6,100 and
17,900), each vertex with two texture reads, and the ground texture's build, under a
millisecond on Soviet 1 and 2 ms on the 96 by 96 skirmish map, once per map. A building
pad now patches only its cells, 0.08 to 0.24 ms on the skirmish map.

After the review fixes, the same camera, snow on and off alternating, six rounds of 40
frames at 1600x900, medians, the fixed build and the build before it each in one process:
snow on minus snow off, paired, -0.62 ms at the standard view and +0.35 ms at the
nearest (fixed build), -0.08 and -0.20 ms (build before). Again all inside the spread
between rounds; the fixes
add a few arithmetic operations a vertex and, only when the rest is shortened, a halving
of 24 steps on the CPU each frame.

## Verification

Review fixes (rest bounded by the lattice, partial ground updates, water test without the
Enhanced sea, depth pull near the ground only, shader variants without vertex textures),
checked against the build before flakes could rest and the build before these fixes:

- `snow_fx 0`: 17 of 17 pictures byte-identical to the build before flakes could rest.
  Defaults: the 14 that are not Red Alert snow byte-identical to it; two runs of the full
  set 17 of 17 identical. At the defaults `ra_snow_close` differs from the build before
  these fixes by no pixel more than 20 of 255 (the rest is not shortened there, and the
  depth pull only moves a falling flake's depth).
- `SNOW|settle` at `ra_snow_close` (1600x900): defaults held 3.00 (31,556 drawn at the
  nearest zoom); `snow_settle 10` held 10 (62,604); `snow_amount 1, snow_settle 10` held
  4.19 (65,504, inside the lattice, where the build before capped at 65,536 and lost
  about 40 percent of the falling flakes); with `snow_speed 6` as well, 0.98.
- The ground patch on the 96 by 96 skirmish map with seven computer players, 2,700 ticks
  drawn every third: the whole build once (2.1 ms), then six pad changes patched in 0.08
  to 0.24 ms each (96 to 416 cells, 2,400 to 10,400 texels), where each used to cost
  about 2 ms. With `OPENCNC3D_GROUND_PATCH_VERIFY=1` every texel was sampled again after
  each patch: 0 of 148,225 differed, every time.
- Water with `water_fx 0`, `snow_amount 1, snow_settle 10`, `ra_snow_close`, against the
  same view with the snow off: dots over the river fell from 102 to 48 (what is left are
  flakes still falling across it), with land almost unchanged (907 and 867); enlarged,
  the river carries no resting dots, where the build before had them all along it. The
  log shows `SNOW|water|field` with the sea off.

- `snow_fx 0`: all 17 standard pictures byte-identical to the build without the snow.
- Defaults (`snow_fx 1`): the 14 pictures that are not Red Alert snow are byte-identical;
  `ra_snow`, `ra_snow_close` and `ra_skirmish` show the snowfall. The log line
  `SNOW|on|theater=RASNOW` appears only in those three; every other scene logs
  `SNOW|off` with its theater (`WINTER`, `TEMPERAT`, `DESERT`, `RATEMP`, `RAINT`).
- Two runs of the full set: 17 of 17 identical.
- Motion: `modern-shots.py --scenes ra_snow_motion` shoots `ra_snow_close` twice one engine
  tick apart; each flake moves a few pixels, round and whole. With `taawarm 0` and frames
  drawn at quarter-tick steps (the blend mid-motion, as a 60 frames a second player sees
  it), the flakes are as crisp as in a settled still, with no trails.
- Panning: one cell of pan with the clock stopped. Of 179 flakes in the first picture, 140
  have a partner in the second along the pan's direction at the ground's shift or more
  (the higher a flake, the larger its parallax); a window of the same size in the opposite
  direction finds 36, and none sits still on screen.
- Enlarged crops over the river, over rock and over shaded snow: the flakes are soft white
  dots, not grey and not glowing; over sunlit snow they nearly vanish.
- Zoom: with the clock stopped, four zoom steps of 20 leptons (0.8 percent) from
  2400: 85 to 90 percent of the flakes fit one smooth zoom of the picture to within 2.5
  pixels (median 1.1), and the same at an engine tick of 2000 as at 60, so a long
  session does not make a zoom worse. The first version matched at chance level (2
  percent within 2 pixels), every flake re-placed at every step.
- The map's edge: the north edge of Soviet 1 at `cam 72 50`, and the mission's opening
  view, show no flake over the black beyond the map (327 pixels before the fix, with
  flakes up to 285 pixels above the edge).
- Retina (`gfx hidpi 1`, a 3200x1800 backing for a 1600x900 window): the largest flake
  is 8.1 points of radius against 8.0 at 1x, and the 99th percentile of flake area is
  173 square points against 170 (before the fix, 4.1 points and 53).
- Windows: `i686-w64-mingw32-g++ -fsyntax-only` of `cnc_eyes.cpp` with the build's flags,
  no errors.

Flakes at rest (`snow_settle`), checked against the build before it:

- `snow_fx 0`: 17 of 17 pictures byte-identical to the build before flakes could rest,
  shot with `snow_fx 0` as well. Defaults: the 14 pictures that are not Red Alert snow
  byte-identical to it; two runs of the full set 17 of 17 identical. The three snow
  scenes log `SNOW|ground` once: 297 by 153 texels on Soviet 1 in 0.6 ms, 385 by 385 on
  the skirmish map in 2.0 ms, 16 bits.
- A flake landing and fading: `ra_snow_close`'s camera, 60 settled frames three engine
  ticks (0.2 s) apart, shot once with the snow and once without at the same ticks, so the
  difference is the flakes alone (575 a frame). Script `dist 1600` and `dist 1200` are
  both held to the rig's nearest, 1920, so that is the zoom shown. Followed frame to
  frame, flakes come down, stop, and hold the same pixel for 13 or 14 frames above a
  difference of 12 of 255, their peak easing down smoothly (one by the bridge's
  parapet: 214 215 216 216 210 192 173 146 122 100 78 47 26), and are gone after 3 s.
  The same flake rests on the same pixel again one cycle later (29 frames, 5.8 s: the
  fastest class's 2.86 s fall plus the 3 s rest). Enlarged 2x, the flake drops in, sits
  still as a round dot and fades; there is no drift or flutter at rest.
- Hills: `ra_snow_close`'s camera with `snow_amount 1` and `snow_settle 10`, enlarged over
  the rock ridge at the bottom of the view: resting flakes dot the tops and faces of the
  raised rock, where the build before ended every fall at the terrain's zero, inside the
  ridge, and shows only the flakes still in the air.
- The ground texture against `terrain_y` over 200,000 random points of Soviet 1: mean
  difference 0.0014 cells; the drawn ground above the texture by more than 0.02 cells at
  0.9 percent of points, more than 0.05 at 0.06 percent, at most 0.12 (sharp rock).

## Known gaps

- Flakes are lit as the ground behind them (above). A flake far above a cast shadow is
  darkened as if it were in it.
- The ground texture is linear between samples a quarter cell apart. Over 200,000 random
  points of Soviet 1 the drawn ground stood above it by more than 0.02 cells at 0.9
  percent of them, by more than 0.05 at 0.06 percent and by at most 0.12 cells, all on
  sharp rock; there a resting flake's lift (half its size, 0.025 to 0.07 cells at the
  standard zoom) and depth pull may not cover it and part of the flake is hidden.
- A flake falls straight along its own column, standing on the ground at its resting
  point. A hill between the top of its fall and its landing point (the wind carries it
  about two cells sideways over a full fall) can hide part of the fall, as the depth
  test would hide any object inside a hill.
- A relief change that is not a building pad (a relief setting, the coast atlas) still
  rebuilds the whole ground texture, 0.6 ms on Soviet 1 and 2 ms on the 96 by 96
  skirmish map. The editor moves the ground generation every frame it draws the ground,
  so over a Red Alert snow map open in the editor the texture would be rebuilt every
  frame; that case was not tried.
- A heavy snow left to lie long, or a fast one, rests for less than the dial says
  (`snow_amount 1` at most 4.2 s, with `snow_speed 6` about 1 s), because the lattice
  cannot hold more resting flakes on top of the falling ones at the nearest zoom. A
  camera nearer than the rig's nearest zoom can still pass the lattice, and there the
  draw is capped.
- More snow is seen than in the version the amount was tuned on (What the player sees):
  about twice the dots and 1.6 to 1.8 times the white at the defaults. The default amount
  is unchanged.
- A resting flake one or two pixels across sparkles slightly under the anti-aliasing:
  being in the reactive mask, it is taken from the current frame, and the jitter changes
  its shape from tick to tick (median 2 to 4 percent of its light, at most about 18
  percent on a small flake on rock). A falling flake hides this because it moves. Letting
  the history steady a resting flake would need it drawn outside the reactive span, a
  second draw at the call site, and a check that a still flake fading out leaves no
  ghost; not done.
- On by default, unlike the rain: a gate that compares pictures on a Red Alert snow map
  sees the snow, so it must pin `snow_fx 0`. G351, G359, G362 and G364 do (their scripts,
  and the placement preset G364 loads); G364 passes on this branch, the other three have
  not been run on it. Any new
  gate on a snow map needs the same line.
- The Enhanced fog's soft rim is a mask the snow does not read, so a faint grey flake can
  show through it over ground at the edge of the explored area, as rain does. Reading that
  mask would mean a texture in the snow's program and the fog field a frame late (it is
  built after the world pass); the shroud blanket itself does cover the snow.
- The foreground is sparser than the horizon, because the snow is a uniform volume seen in
  perspective and the nearest flakes are capped at nine points and fade near the eye.
  Whether the near half of the view should carry more snow is a matter of taste, not a
  fault, and is left as it is.
- The engine clock drives the fall, so the snow stops while the game is paused and steps
  once a tick with smooth animation off (about 20 pixels a step for a near flake zoomed
  in), as the rain does.
- The heaviest setting, `snow_amount 1`, is a steady moderate snowfall, not a blizzard.
- Tier 2 only. There is no Win98 answer: CLASSIC and the 640x480 Glide build draw no snow.
