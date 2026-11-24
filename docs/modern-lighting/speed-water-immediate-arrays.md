# Speed: the shore and the sea drawn from arrays

Key: `water-immediate-arrays`. Branch: `visual/speed-water-immediate-arrays`. No switch:
the pictures are byte-identical, so the old path is not kept.

The beach film, the swash crest and the shore-aware sea surface (`water_shore_draw` and
`water_fx_draw_surface` in `game/water_mod.h`) were drawn in immediate mode: per vertex,
one `glMultiTexCoord` for each texture coordinate set the shader reads, then `glColor4f`
and `glVertex3f`. That is five or six GL calls a vertex. The desert base view sent 273,000
of them a frame from the shore alone. Red Alert's water sent 270,000 on the snow view and
367,000 in a skirmish, which the frame profiler counts under `terrain` because Red Alert
draws its shore from its terrain pass; most of those come from the same beach film, and
only a small part from the sea's own per-corner loop. A driver that charges for every call
spends milliseconds on that while the GPU waits.

## What changed

- The three loops write the very same floats, in the same order, into one interleaved
  array (`g_waterArray`), and each former `glBegin(GL_TRIANGLES)`/`glEnd` pair is one
  `glDrawArrays(GL_TRIANGLES)` from client arrays (`water_array_draw`, just above
  `water_fx_draw_surface`). The rasteriser receives the same triangles with the same
  attributes.
- The array is filled by a plain copy of each vertex's floats into storage that only grows
  (`water_array_put`), with a count of the floats in use, not by a `std::vector` range
  insert. The insert is an out-of-line library call per vertex; on the Mac it cost more
  than the GL calls it replaced (see Frame time, Mac).
- Vertex layouts, in floats: the sea surface 15 (the base's coordinate, read by sets 0 and
  1 from the same pair; the terrain tint on set 2; the swash term on set 6; the colour; the
  position); the beach film 15 (the coast sheet on set 5; the beach lift on set 7; the
  contact lift and coast term on set 6; the colour; the position); the crest 13 (the ground
  height on set 7; the coast term on set 6; the colour; the position).
- A set the immediate loop never sent (the coast sheet's set 5 on a base that is not
  painted) is not in the array either, so the shader keeps reading its current value, as
  before.
- Client state is saved and restored with `glPushClientAttrib`/`glPopClientAttrib`
  through the state cache's own wrappers, so the cache's view of the client state stays
  right. Every texture coordinate set the context has is walked, not only the four
  fixed-function units: the water shaders read sets 5, 6 and 7, and an array left enabled
  on any set by earlier code would otherwise be read where immediate mode ignored it.
- A bound `GL_ARRAY_BUFFER` is unbound for the draw (client pointers would otherwise be
  read as offsets into it). The pop puts the binding back; the state cache, which forgets
  the binding at every pop, is told the restored value, so the change adds no query to the
  driver.
- After an array draw GL leaves the current colour and texture coordinates undefined, where
  `glEnd` left them at the last vertex's values. They are set back to the last vertex's
  values (one call per set plus the colour), so whatever draws next starts from the state
  it always had.

## Identity

Shot with `tools/visual/modern-shots.py` (17 pictures, 15 scenes, 1600x900, `--jobs 2`),
from the parent commit and from this branch, with the same frozen settings files for both.
For the final build (the plain-copy fill):

- All nine masters off: 17/17 byte-identical to the parent in each of two runs, and the
  two runs identical to each other.
- Defaults (every feature on): 17/17 byte-identical to the parent in each of two runs, and
  the two runs identical to each other. A first parent run differed from both in
  `ra_skirmish` (270 pixels, nearly all one level, on ore glints); a second parent run
  matched both branch runs 17/17, and `ra_skirmish` and `ra_ant` shot on their own, four
  times from each build alternating, gave one hash per scene in all eight runs, the
  branch's. So that difference was the parent's own.
- A moving case (`ra_snow` and `td_desert` panned over sea and shore with a shroud reveal
  between shots, `td_coast` panned and zoomed, and a `ra_skirmish` with units moving, 16
  pictures): 16/16 byte-identical to the parent.
- A harder set (41 pictures, 15 scenes: the desert zoomed in and out, the first frames
  after load under shroud and after the reveal, Enhanced, Classic and N64 mode switches,
  units ordered along the desert river, a refinery and a tower destroyed, one map loaded
  after another in both games, winter, Red Alert's snow and interior zoomed, a gunboat on
  the coast): 39/41 byte-identical to the parent. The other two, `a_ra_skirmish_move_1`
  (2 pixels, one level) and `a_td_remission_2` (a patch of sand on the first frame after a
  map reload), change hash from run to run in the parent as well. Shot again twice from
  each build, alternating: the skirmish gave one hash per round, the same from both builds
  and different between rounds; the reload gave the parent two different hashes in its
  three runs, and its usual one in three of the branch's four.
- On Windows (an NVIDIA RTX-class card), a screenshot after each timing run below, all on
  and all masters off, on `td_desert_base`, `td_coast`, `ra_snow` and `ra_skirmish`: 32 of
  32 byte-identical between the parent and the final build, and the same in both rounds.

The shot set carries a small nondeterminism that shows only under load, in the parent as
much as in this branch: one level in a few pixels, now and then many, on ore glints in
`ra_ant` or `ra_skirmish`. Earlier builds of this branch met it in three of six all-on
runs, and the parent in one of five. Its cause is not known.

## GL calls per frame

Counted with a GL interposer over `framebench 60` after 30 warm frames, 1600x900, Mac:

| view | parent | this branch | in the changed section |
|---|---|---|---|
| td_base | 177,299 | 121,586 | shore 44,595 to 153 |
| td_desert_base | 367,427 | 77,228 | shore 273,393 to 153 |
| ra_snow | 341,656 | 73,603 | terrain 269,709 to 1,656 |
| ra_skirmish | 410,316 | 44,091 | terrain 367,496 to 1,271 |

Immediate-mode vertices per frame fall by the same amount (55,176 on the desert base,
53,730 on the snow view, 73,038 in the skirmish). Draw counts are unchanged.

On Red Alert views the saving is mainly Red Alert's beach film, the same loop as Tiberian
Dawn's shore: with `OPENCNC3D_WATER_SHORE_AUDIT=1` the snow view reports 43,134 film
vertices of its 53,730, about 216,000 of its 270,000 calls. The rest, about 52,000 calls,
is the swash crest (about 5,600 vertices) and the sea's per-corner loop (about 5,000). The
desert base reports 52,596 film vertices, which matches its count above.

## Frame time, Windows

An NVIDIA RTX-class card on Windows, the parent against the final build, each with every
feature on and with all nine masters off, 2 rounds of 3 windows of `framebench 60`, order
reversed between rounds, every run on the NVIDIA renderer (32 of 32). Frame medians in ms,
and the CPU section this change touches:

| view | parent | final | section | masters off: parent | final |
|---|---|---|---|---|---|
| td_desert_base | 16.42 | 13.33 | shore 4.24 to 1.33 | 14.88 | 11.14 |
| td_coast | 18.01 | 15.14 | shore 3.44 to 1.00 | 16.08 | 13.34 |
| ra_snow | 26.27 | 25.27 | terrain 3.15 to 2.45 | 23.34 | 21.78 |
| ra_skirmish | 16.91 | 18.46 | terrain 3.38 to 2.64 | 14.29 | 13.54 |

The Tiberian Dawn coast views gain, all of it from the shore: the desert base 2.7 to 4 ms
a frame (16 to 24 percent) over three separate runs of this comparison, and `td_coast` 2.2
to 2.9 ms (12 to 16 percent) over two. The parent's own shore section ranges from 3.8 to
4.7 ms on the desert from run to run, which is most of that spread. The final build and
the first version differ little here, since the range insert is cheap next to the GL calls
on this driver.

On Red Alert views the gain is CPU submission time only. The terrain section falls by
about 0.7 ms, but with every feature on these views are bound by the GPU, and the time
saved reappears as waiting further on in the frame, mostly in `effects`, where the
temporal anti-aliasing snapshot is taken (0.61 to 1.60 ms in the skirmish). Timed with
every pass finished on the GPU before the next (`OPENCNC3D_FRAMEPROF_SYNC=1`), Red Alert's
terrain pass (first version) costs only 0.16 ms less in CPU and GPU time together. So
`ra_snow` is about 1 ms faster, inside its run-to-run spread, and the skirmish is not
faster but slightly slower: re-timed on its own over 4 rounds it gave 16.52 ms for the
parent against 17.18, and over the six rounds of the two runs the final build was slower
in every one, by 0.04 to 2.2 ms. Why a GPU-bound frame should lose time to the same
triangles sent from a client array is not known; a buffer object filled in place, or a
switch back to immediate mode for Red Alert's film, would be the things to try. With all
masters off, where the frame is less held by the GPU, both views are 0.7 to 1.6 ms faster.

An earlier run of the first version at 1600x900 and 1920x1080 found the same pattern:
the desert base 18.1 to 14.1 and 18.9 to 14.3 ms (the top of the range above), and
`td_base` and `ra_temperate` within noise.

## Frame time, Mac

Three builds alternated back to back, the order reversed each round, 4 rounds, 1600x900,
three windows of `framebench 60` after 20 warm frames with the frame profiler on: the
parent, this change's first version (the array filled by a `std::vector` range insert per
vertex) and the final build (the plain copy). The CPU section this change touches, median
ms (lowest to highest round in brackets):

| view | section | parent | range insert | final |
|---|---|---|---|---|
| td_desert_base | shore | 0.91 (0.89 to 0.98) | 0.86 (0.83 to 0.90) | 0.52 (0.51 to 0.55) |
| td_coast | shore | 0.70 (0.68 to 0.82) | 0.59 (0.57 to 0.63) | 0.38 (0.35 to 0.44) |
| ra_snow | terrain | 1.23 (1.12 to 1.42) | 1.36 (1.28 to 1.56) | 1.11 (0.96 to 1.14) |
| ra_skirmish | terrain | 1.29 (1.27 to 1.35) | 1.46 (1.41 to 1.57) | 1.06 (1.02 to 1.23) |

Apple's legacy context makes immediate mode cheap, so the GL calls removed are worth
little there, and the cost of filling the array decides the result. The range insert made
Red Alert's views slower than the parent in every round (0.13 and 0.17 ms in the medians)
and used up most of the Tiberian Dawn saving: sampling the snow view's process for 8 seconds found 90 samples
in the library's insert routine itself, inside the shore film's loop. The plain copy has
none there. With it the shore costs about 45 percent less than the parent's on both
Tiberian Dawn coast views, and Red Alert's terrain section is 0.12 and 0.23 ms cheaper, no
round above the parent's. The whole-frame medians moved by up to several ms between rounds on a
machine running other work, so no frame-level change can be read from them.

## Not done here

- The shore's remaining 1.0 to 1.3 ms on Windows on the coast views is the per-vertex work
  of the loop itself (the coast cache, the terrain corner lookups), not GL.
- The remaining immediate-mode vertices on these views (35,000 a frame on td_base) belong
  to the shroud, the mottle and the tiberium ground, which are a separate item.
