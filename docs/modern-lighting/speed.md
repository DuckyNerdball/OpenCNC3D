# Speed: the five speed items, merged

Five speed items were worked on their own branches against the modern lighting stack and
are merged here together. Each has its own page with the full method and evidence; this
page gives what the merged renderer does to the frame on both platforms, and one paragraph
per item on what it changed and what it is worth.

| key | page | renderer change | default | switch back |
|---|---|---|---|---|
| `glow-pose-cache` | [speed-glow-pose-cache.md](speed-glow-pose-cache.md) | yes | on | `OPENCNC3D_MESH_GLOW_KEEP=0` (environment) |
| `water-immediate-arrays` | [speed-water-immediate-arrays.md](speed-water-immediate-arrays.md) | yes | on | none (pictures identical) |
| `aux-pass-state-elision` | [speed-aux-pass-state-elision.md](speed-aux-pass-state-elision.md) | yes | on | `OPENCNC3D_AUX_STATE_ELIDE=0` (environment) |
| `overlay-immediate-arrays` | [speed-overlay-immediate-arrays.md](speed-overlay-immediate-arrays.md) | behind a dial | off | `overlay_arrays` (DEBUG group) switches it on |
| `sun-map-static-reuse` | [speed-sun-map-static-reuse.md](speed-sun-map-static-reuse.md) | none | | notes only |

## The frame, before and after

Three renderers, timed back to back on each view:

- **before**: the renderer the modern lighting stack branched from (c5befcdac), without
  any of the modern features;
- **modern**: the modern lighting stack before these items (a99d6f7e2), every master at
  its default;
- **merged**: the same stack with the five items merged, every master at its default.

Each run loads the view, warms up for 20 frames and then times three windows of
`framebench 60` (each frame finished on the GPU) with `OPENCNC3D_FRAMEPROF=1`. The three
renderers run back to back for each view and size, and the order rotates every round, so
over three rounds each renderer takes each place once: on Windows the same build drifts
by about a millisecond between the first and the middle place of a round, as large as some
of the effects measured. All runs use one frozen copy of the graphics
settings. Cells are medians over the nine windows, in ms.

Windows: a desktop NVIDIA card (RTX class) under OpenGL 4.6, the 32-bit build, run hidden
in the logged-on desktop session. All 72 runs reported the NVIDIA renderer and exited
cleanly, and the two modern builds reported drawing at the requested size (the before build
prints no such line).

| view | size | before | modern | merged | merged - modern |
|---|---|---|---|---|---|
| td_base | 1600x900 | 34.13 | 37.68 | 24.00 | -13.7 |
| td_base | 1920x1080 | 35.44 | 39.83 | 24.44 | -15.4 |
| td_desert_base | 1600x900 | 14.47 | 17.46 | 13.53 | -3.9 |
| td_desert_base | 1920x1080 | 15.36 | 18.68 | 14.29 | -4.4 |
| ra_snow | 1600x900 | 24.29 | 27.13 | 26.50 | -0.6 |
| ra_snow | 1920x1080 | 24.47 | 27.41 | 26.57 | -0.8 |
| ra_skirmish | 1600x900 | 14.01 | 16.34 | 15.78 | -0.6 |
| ra_skirmish | 1920x1080 | 14.35 | 16.63 | 17.21 | +0.6 |

Mac: Apple Silicon, Apple's GL 2.1. The rows marked * were taken while other processes
were loading the machine: their whole-frame medians swing by 3 to 10 ms between rounds,
more than any change measured, so they do not measure those views; the quiet-machine
table that follows and the sections further down do.

| view | size | before | modern | merged | merged - modern |
|---|---|---|---|---|---|
| td_base | 1600x900 | 20.86 | 27.63 | 18.24 | -9.4 |
| td_base | 3008x1692 | 30.33 | 36.30 | 28.58 | -7.7 |
| td_desert_base * | 1600x900 | 10.69 | 8.61 | 8.76 | +0.2 |
| td_desert_base * | 3008x1692 | 13.92 | 24.94 | 25.47 | +0.5 |
| ra_snow * | 1600x900 | 13.65 | 17.10 | 18.49 | +1.4 |
| ra_snow * | 3008x1692 | 17.51 | 26.75 | 21.38 | -5.4 |
| ra_skirmish * | 1600x900 | 10.48 | 11.96 | 14.16 | +2.2 |
| ra_skirmish * | 3008x1692 | 18.93 | 14.54 | 14.66 | +0.1 |

The same three views at 1600x900 on a quiet machine, in ms, with a third build that is the
merged one without the water item:

| view | modern | merged | merged without water |
|---|---|---|---|
| td_desert_base | 8.75 | 8.43 | 8.84 |
| ra_snow | 13.12 | 13.06 | 13.03 |
| ra_skirmish | 9.04 | 9.06 | 9.16 |

So on the Mac the merged renderer is level with the modern stack on both Red Alert views
and about 0.3 ms faster on the desert. Modern against merged alone, alternating so each is
first in half the rounds, taken earlier while the machine was shared with other work:
td_desert_base 8.91 against 8.57 (six rounds), ra_skirmish 12.61 against 12.12 (six
rounds), ra_snow 13.93 against 16.09 (ten rounds; see "Red Alert's snow map on the Mac"
below).

td_base in play, with the flicker of the ember glow moving every frame (60 frames drawn one
at a time, the sub-tick phase stepped before each), Mac 1600x900, three rounds: before
22.04, modern 27.79, merged 22.17.

The table's 9.4 ms on td_base at 1600x900 holds for frames finished on the GPU with the
clock paused. In conditions closer to play, with the flicker moving, the sub-tick phase
stepped and the frames allowed to run ahead of the GPU (each timed to its swap,
`OPENCNC3D_FRAMEPROF_END=swap`), the gain is about 4.5 ms: modern 28.21 against merged
23.73 ms over four alternating rounds. The merged frame becomes GPU-bound there, so only
part of the glow's CPU saving reaches it.

### Where it comes from

The sections the items change, modern against merged, in ms (Windows at 1600x900; the
1920x1080 figures are within 0.2 ms of these):

| view | section | item | Mac 1600x900 | Mac 3008x1692 | Windows |
|---|---|---|---|---|---|
| td_base | cutout + translucent | glow | 7.23 to 0.95 | 7.38 to 1.08 | 14.67 to 2.49 |
| td_base, in play | cutout + translucent | glow | 7.36 to 1.93 | | |
| td_base | shore | water | 0.24 to 0.18 | 0.24 to 0.21 | 0.93 to 0.44 |
| td_desert_base | shore | water | 0.84 to 0.50 | 1.09 to 0.69 | 4.48 to 1.34 |
| ra_snow | terrain | water | 1.09 to 0.94 | 1.17 to 1.00 | 3.23 to 2.46 |
| ra_skirmish | terrain | water | 1.24 to 1.02 | 1.21 to 1.03 | 3.42 to 2.48 |

The sections of the other items hardly move. On Windows "post: ground normals" (one of the
two passes the state elision works in) is 0.1 to 0.2 ms cheaper on td_base in five rounds
of six and level on the other views, "post: specular" (the other) within 0.1 ms
everywhere, and "rain + shroud draw" (the overlay arrays, off) within 0.1 ms. On the Mac
these sections move by less than their spread between rounds, in both directions. The sun
shadow map, which no item changes, moves by up to 0.35 ms either way on both platforms,
which is its noise. On Windows the merged renderer is now faster than the renderer
before the modern lighting on both Tiberian Dawn views (td_base by about 10 ms, the desert
by about 1 ms), while the Red Alert views keep most of the modern stack's cost. There the
water item takes 0.7 to 0.9 ms of CPU off the terrain section. On ra_snow most of it
reaches the frame (0.6 to 0.8 ms). On ra_skirmish it is absorbed: terrain is GPU-bound on
that view, so the CPU saving comes back as a longer wait for the GPU, which a scripted run
shows in "effects", where the TAA takes its snapshot (0.39 to 1.16 ms at 1600x900, 0.55 to
1.46 at 1920x1080), and the card's work is unchanged. The frame is level at both sizes. The
table's 0.6 ms either way is inside the millisecond a build drifts between places in a
round, and the loss at 1920x1080 that the water item recorded on its own branch did not
reproduce when measured again here: modern 17.34 against merged 17.32 ms, 17.18 against
17.14 with GPU timestamps, and a GPU-timeline total of 14.79 against 14.84 ms.

### Red Alert's snow map on the Mac

In ten alternating rounds of modern against merged, the merged renderer was level or
faster in the first six and 0.6 to 3.4 ms slower in the last four. Every CPU section is
the same or cheaper in every round (terrain 0.15 ms cheaper); the whole difference is in
the GPU wait at the end of the frame ("post chain" and "effects"). With the passes
serialised (`OPENCNC3D_FRAMEPROF_SYNC=1`, each pass's CPU and GPU time together, four
rounds) every pass costs the same in both builds and terrain is 0.1 ms cheaper in the
merged one (frame 21.13 against 20.95), so the merged renderer does not give the GPU more
work. A third test ran four builds in rotation for eight rounds (modern, modern with the
glow item, modern with the glow and water items, merged): there the merged renderer was
the fastest of the four (14.23, 13.60, 15.12 and 13.16 ms), and each build had slow rounds
of 16 to 24 ms. So the snow map's frame on the Mac follows how busy the shared GPU is, and
no build is measurably slower there; only its terrain section, 0.15 ms cheaper with the
water item, changes for certain. On a quiet machine the two are level: 13.12 against
13.06 ms (the quiet-machine table above).

### Pictures

With every master at its default and with all nine masters off, the merged renderer's 17
pictures (`tools/visual/modern-shots.py`, frozen settings) are byte-identical to the
modern stack's, in each of two runs, and the two runs are identical to each other.
Built after each merge (glow, then water, then the state elision, then the overlay
arrays), the renderer compiled cleanly each time; the Windows cross build of the merged
tree has the same 20 warnings as the stack before it. On Windows a screenshot from each
of 32 runs (td_base, td_desert_base, ra_snow and ra_skirmish at 1600x900, every master at
its default and all nine off, two rounds, every run on the NVIDIA renderer) has the same
SHA-256 from the merged renderer as from the modern stack, and the same in both rounds.
Two states are not stable from run to run in any build of this stack, before or after
these items, so a comparison that reaches them needs care: on the Mac a Red Alert play
state with the 3D ore can differ by a few pixels at the ore nuggets, and the first view
after a mission switch inside the process varies at the left edge; compare them with
`tib3d 0`, or by the majority picture over three or more runs.

## The items

**The ember glow keeps its posed pieces** (`glow-pose-cache`). The ember glow on damaged
remastered buildings is drawn additively over the building's standing pieces at an opacity
that flickers every frame, and that per-frame opacity used to switch off every cache in
`draw_mesh`, so each glowing piece was posed from its triangles, packed and uploaded again
in every frame. Now a glow draw that is still in every other respect keeps its posed pieces
in a store of its own (`g_meshGlowCache`, `game/mesh_batch.h`) and only its colours are
rewritten when the flicker moves; the still-piece cache and the glow store test one shared
list of what keeps a piece still, and the store is aged every frame, so it empties while no
glow is in view. This is the largest single saving of the five: the glow's section
("cutout + translucent") on the damaged Tiberian Dawn base falls from about 7 ms to 1 ms on
the Mac and from about 15 ms to 2.5 ms on Windows with the clock still; with the flicker
moving every frame, as in play, it falls to about 2 ms on the Mac, and on the item's own
branch it fell from 16.4 to 5.3 ms on Windows. Views without a damaged remastered building
are untouched, and a wrecked building's shard glow and a vehicle wreck's glow still rebuild
every frame.

**The shore and the sea drawn from arrays** (`water-immediate-arrays`). The beach film,
the swash crest and the shore-aware sea surface (`game/water_mod.h`) were drawn in
immediate mode, five or six GL calls a vertex. They now write the same floats in the same
order into one interleaved array, filled by a plain copy into storage that only grows, and
draw each former `glBegin`/`glEnd` pair with one `glDrawArrays`. That removes about
290,000 GL calls a frame on the desert base and about 270,000 on Red Alert's snow map. On
Windows the Tiberian Dawn coast views gain 2 to 4 ms; on Red Alert's views the terrain
section gets cheaper on the CPU, but on the skirmish view, where terrain is GPU-bound, the
saving is absorbed as GPU wait later in the frame and the frame is level at both sizes
measured; the slight loss that view showed with every master on, on the item's own branch,
did not reproduce here. On the Mac the shore and terrain sections get 0.15 to 0.35 ms
cheaper. There is no switch: the pictures are byte-identical and the old path is
not kept.

**The ground-normal and bake passes stop resetting state** (`aux-pass-state-elision`).
The two extra Enhanced world passes draw the world again through the colour picture's own
draw functions, each draw setting its state as if nothing were set. Inside those passes a
uniform set to the value its program already holds, the stone texture already bound on its
unit and a client array switch after a restore are now skipped (`game/gl_state_cache.h`,
`game/fx_gl.h`): 2,650 fewer GL calls a frame on the Tiberian Dawn base. The calls go and
the pictures are identical, but no frame got measurably faster on either platform: a state
call that changes nothing is cheap on both drivers. It is merged because it is harmless and
keeps the call count down; `OPENCNC3D_AUX_STATE_ELIDE=0` sends every call again.

**The shroud, the mottle and the tiberium ground from arrays** (`overlay-immediate-arrays`).
The shroud, the ground mottle and the tiberium cracks and contact patches can be drawn from
vertex arrays instead of one GL call per vertex attribute, 55,000 to 64,000 fewer calls a
frame on the Tiberian Dawn views. The array path made no frame faster on either platform,
so it sits behind the `overlay_arrays` dial (DEBUG group, hidden from the panel), off by
default; with the dial off the overlays are drawn in immediate mode with the parent's GL
calls in the parent's order. It is merged as a workshop control, so the array path can be
measured again on another driver without rebuilding it. A settings file saved by this
build carries an `overlay_arrays 0` line.

**The sun map's still casters** (`sun-map-static-reuse`). Notes only; the renderer is
unchanged. Keeping the sun map's still casters in a depth layer of their own was measured
first: every caster that could be still in play is worth at most about 1.1 to 1.4 ms of
sun map on Windows on a damaged base and 0.3 to 0.5 ms on the Mac, and almost none of it
is the part a still layer could keep. Most of the sun map's cost is the swaying trees,
which take a new pose every frame (7.1 to 7.5 ms of Red Alert's snow map's 8 ms on
Windows). The page records where the time goes so the idea is not taken up again on the
strength of the frame benchmark alone; the work it points at is the trees' wind pose.
