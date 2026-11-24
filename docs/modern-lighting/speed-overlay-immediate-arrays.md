# Speed: the shroud, the mottle and the tiberium ground drawn from arrays

Key: `overlay-immediate-arrays`. Branch: `visual/speed-overlay-immediate-arrays`.

**Behind a dial, off by default.** `overlay_arrays` (the DEBUG group, hidden from the
panel; `gfx overlay_arrays 1` in a script) switches the array path on. Off, which is the
default, every overlay is drawn in immediate mode with the same GL calls in the same
order as the parent. The array path draws the same pictures, but it made no frame faster
on either platform (see Verdict), so it is kept only as a workshop control. The 1 ms loss
on the Windows desert base reported earlier did not survive a test that balances run
position (see The dial round).

Three ground overlays were drawn in immediate mode, one GL call per vertex attribute:

- the shroud (`shroud_draw`, the hard squares, and `shroud_draw_soft`, the soft black core
  and its feathered edge, in `game/shroud_mod.h`), which the frame profiler counts under
  `rain + shroud draw`: a `glColor4f` and a `glVertex3f` per vertex, 21,000 calls a frame
  on the base view and 41,000 on the desert base;
- the ground mottle (`mottle_draw`, `game/mottle_mod.h`): a `glColor4f`, a `glTexCoord2f`
  and a `glVertex3f` per vertex, 29,000 calls a frame on the base view, in 29
  `glBegin`/`glEnd` blocks, one per patch, each with its own texture;
- the tiberium ground (`draw_tiberium_ground`, the glowing cracks, and
  `draw_tiberium_contact`, the dark contact patch under every mound, in
  `game/cnc_eyes.cpp`), counted under `tiberium ground`: a `glTexCoord2f` and a `glVertex3f`
  per vertex, 13,700 calls a frame on the base view. The contact patches are 5,760 of that
  section's 6,738 vertices there, so both functions are converted, although only the
  cracks were named when this item was planned.

## What changed

- `game/overlay_arrays.h` (new) holds `OverlayArrays`, which every one of these loops now
  writes through. With the dial off (the default) each of its calls is forwarded to GL as
  it comes: `bind_texture()`, `begin()`, `color()`, `texcoord()`, `vertex()` and `end()`
  are `glBindTexture`, `glBegin`, `glColor4f`, `glTexCoord2f`, `glVertex3f` and `glEnd`,
  and the state changes around them are where they were in the parent, so the default
  draw is the parent's call stream. With the dial on it is a batch that keeps immediate
  mode's own rules so each loop converts line for line: `color()` and `texcoord()` set a
  current value, and `vertex()` appends a position together with whatever is current, as
  `glVertex3f` does; `flush()` then draws what was collected. The loops write the same floats in the same order, and every former
  `glBegin`/`glEnd` block becomes one `glDrawArrays` of the same primitive over the same
  vertices: `GL_QUADS` for the shroud's squares and core, `GL_TRIANGLES` for the feather
  (both the classic four-triangle fan and the smooth 4 x 4 sub-grid), the mottle and the
  tiberium ground.
- A batch carries only the streams its loop varied. The shroud's squares and core, and
  the contact patches, set one colour before the block, so their batches are positions
  (and texture coordinates) only and the colour stays the current one, exactly as before.
  The feather carries a colour per vertex; the cracks carry each crack's colour on its six
  vertices, where the loop set it once before them.
- In the array path each block is drawn at the `flush()` that follows it, under the state
  set before its loop: the shroud's darkened squares under blending, then the black ones
  without (one flush per pass, so two client-array setups where the first version of this
  branch used one); the core, then the feather; the mottle's patches collected with their
  textures and each bound and drawn in turn at one flush after the loop. A mottle patch
  with no visible cell is still bound, as before, but drawn with nothing. None of these
  loops touched GL state, so drawing after them changes nothing GL sees.
- Client state is saved and restored with `glPushClientAttrib`/`glPopClientAttrib`
  through the state cache's own wrappers. For the draw, every texture coordinate array but
  unit 0's, the normal array, the colour index array and the edge flag array are switched
  off (the same set `imm_draw_triangles` and the terrain cache switch off), and a bound
  `GL_ARRAY_BUFFER` is unbound, so nothing an earlier pass left enabled is read; the cache is told the restored
  buffer binding, so no query is added.
- After an array draw the current colour and texture coordinate are undefined, where
  `glEnd` left them at the last vertex's values. They are set back to those values, so
  whatever draws next starts from the state it always had.
- The arrays are filled by a plain copy of each vertex's floats into storage that only
  grows, with a count of the vertices in use. The first version appended with `push_back`
  and a range insert per vertex; on the Mac that was slower than the immediate-mode calls
  it replaced (see Frame time, Mac).

Left as it was: the DOS health bar at the end of `shroud_mod.h` (12 vertices a bar, not
part of this item).

## Identity

Shot with `tools/visual/modern-shots.py` (17 pictures, 15 scenes, 1600x900, `--jobs 2`),
with the frozen settings files, from the parent commit (`6a36316a0`) and from this branch.
The play folder's data was rewritten while the first comparison ran, which changed the
pictures shot during the copy (whole buildings drawn from other models), whichever build
shot them. Every comparison below was therefore shot from an APFS clone of the play
folder taken once the copy had finished; a parent set shot from the clone matched the
parent set shot before the copy, 17 of 17.

- Defaults (every feature on): 17/17 byte-identical to the parent in each of two runs of
  the final build, and the two runs identical to each other. The first version (vector
  fill) gave the same, also twice.
- All nine masters off: 17/17 byte-identical to the parent in each of two runs, for both
  versions.
- A moving case, all on and all masters off (14 pictures each): the first GDI mission
  under full shroud with the smooth feather, the classic fan and the hard squares in turn,
  units ordered into the black with a selection band on screen and the camera panning
  after them; the base view with its mottle and tiberium cracks, then panned along a
  reveal edge; the desert base with mottle under the soft and the hard shroud; Red Alert's
  snow map with the classic and the smooth feather; Red Alert's temperate map after 60
  more ticks of play. 14/14 and 14/14 byte-identical to the parent, for both versions.

## GL calls per frame

Counted with a GL interposer over `framebench 60` after 30 warm frames, 1600x900, Mac,
the array path (the build before the dial; with the dial off the counts are the
parent's):

| view | parent | this branch | in the changed sections |
|---|---|---|---|
| td_base | 177,299 | 113,554 | shroud 21,100 to 177; mottle 29,326 to 91; tiberium ground 13,660 to 69 |
| td_desert_base | 367,427 | 313,258 | shroud 41,100 to 177; mottle 13,318 to 72; tiberium ground 21 to 17 |
| ra_snow | 341,656 | 306,895 | shroud 35,000 to 237 |
| ra_skirmish | 410,316 | 386,164 | shroud 24,392 to 239 |

Immediate-mode vertices in the three sections fall to 4 a frame (the rain's), from 27,510
on the base view, 25,844 on the desert base, 17,956 on the snow view and 12,724 in the
skirmish. Draw counts are unchanged, apart from empty mottle patches and an empty crack
batch, which issued a `glBegin`/`glEnd` with nothing in it and now issue no draw. Red
Alert has no mottle or tiberium ground, so only its shroud changes. The remaining calls on
the desert and Red Alert views are mostly the shore and the sea, which the
`water-immediate-arrays` item converts.

## Frame time, Mac

The parent and the final build back to back, the order reversed each round, three windows
of `framebench 60` after 30 warm frames, with the frame profiler on; 4 rounds at 1600x900
and 2 at 3008x1692, on a machine shared with other work. Medians in ms; "sections" is the
sum of `mottle`, `tiberium ground` and `rain + shroud draw`:

| view | size | frame: parent | final | sections: parent | final |
|---|---|---|---|---|---|
| td_base | 1600x900 | 30.21 | 29.23 | 1.13 | 1.07 |
| td_desert_base | 1600x900 | 14.99 | 12.76 | 0.99 | 0.88 |
| ra_snow | 1600x900 | 23.96 | 23.16 | 1.14 | 1.01 |
| ra_skirmish | 1600x900 | 14.91 | 15.76 | 0.85 | 0.79 |
| td_base | 3008x1692 | 36.44 | 40.34 | 1.16 | 1.23 |
| td_desert_base | 3008x1692 | 24.96 | 24.15 | 1.03 | 0.94 |
| ra_snow | 3008x1692 | 29.07 | 36.30 | 0.96 | 1.30 |
| ra_skirmish | 3008x1692 | 29.96 | 23.12 | 1.10 | 1.04 |

The whole-frame figures are noise: the same build moved by as much as 17 ms from one round
to the next (`ra_snow` at 3008x1692), in both directions. The sections are 0.06 to 0.13 ms cheaper at 1600x900, inside
their own round-to-round spread of about 0.3 ms, and mixed at 3008x1692. So on the Mac this
change is neutral: the 64,000 calls it removes from the base view were cheap on Apple's GL,
and the sections cost what they did. The planned saving of 0.3 to 0.6 ms did not appear.

The fill matters here. The first version, which appended each vertex with `push_back` and
a range insert, was slower than immediate mode on the Mac. Three alternating rounds of all
three builds, sections in ms:

| view | parent | first version | final |
|---|---|---|---|
| td_base | 1.06 | 1.29 | 1.10 |
| td_desert_base | 0.94 | 1.24 | 0.97 |

## Frame time, Windows

An NVIDIA RTX-class card on Windows (32-bit build, GL 4.6 compatibility profile), the
parent against the final build with every feature on, 2 rounds of 3 windows of
`framebench 60`, order reversed between rounds, every run on the NVIDIA renderer (24 of
24). Frame medians and the CPU sections this change touches, ms:

| view | size | frame: parent | final | mottle | tiberium ground | rain + shroud draw |
|---|---|---|---|---|---|---|
| td_base | 1600x900 | 36.90 | 38.06 | 0.28 to 0.23 | 0.21 to 0.17 | 1.40 to 1.49 |
| td_base | 1920x1080 | 39.50 | 39.02 | 0.29 to 0.22 | 0.22 to 0.17 | 1.41 to 1.48 |
| td_desert_base | 1600x900 | 17.54 | 18.82 | 0.17 to 0.15 | under 0.1 | 2.16 to 2.16 |
| td_desert_base | 1920x1080 | 18.87 | 20.55 | 0.17 to 0.20 | under 0.1 | 2.20 to 2.51 |
| ra_snow | 1600x900 | 26.96 | 26.61 | none | none | 2.06 to 2.12 |
| ra_snow | 1920x1080 | 27.80 | 27.70 | none | none | 2.08 to 2.08 |

Timed again with every pass finished on the GPU before the next
(`OPENCNC3D_FRAMEPROF_SYNC=1`, 1600x900, 2 rounds), which gives each section its CPU and
GPU cost together: on `td_base` the mottle 0.41 to 0.30 ms, the tiberium ground 0.29 to
0.23 and the shroud 1.50 to 1.54; on the desert base the mottle 0.24 to 0.23 and the
shroud 2.31 to 2.35.

So on this driver the shroud's 21,000 to 41,000 calls a frame cost about what the array
draw costs: NVIDIA's immediate mode is fast for plain vertex calls, and the section's 1.4
to 2.3 ms is spent elsewhere in it. Whatever made the calls removed in v0.8.1 expensive
does not apply to these. The mottle and the tiberium ground gain 0.05 to 0.11 ms on the
base view. None of this moves `td_base` or `ra_snow` by more than their noise.

The desert base is the exception: it came out slower in both rounds and at both sizes, by
1.3 and 1.7 ms. Timed again on its own, 3 rounds alternating at 1600x900, it was 16.90 ms
for the parent against 18.71 for the final build, slower in every round (15.04 against
18.70, 16.90 against 18.97, 16.95 against 17.56), and a fourth comparison gave 16.72
against 18.41. The extra time is not in the sections this change touches, which cost the
same, but in the sections that wait for the GPU: `effects` (0.87 to 1.71 ms, where the
temporal anti-aliasing snapshot waits), `markers + bars` and `post chain`. With every pass
finished on the GPU before the next, the same view's passes each cost what they did, so
the loss comes from how the frame overlaps with the GPU, not from more work in any one
pass. The cause is not known. The water conversion met the same kind of loss on Red
Alert's skirmish view.

A trial that streamed each batch through one buffer object (orphaned with `glBufferData`
and filled with `glBufferSubData`, instead of client memory) was byte-identical on the
Mac (17/17 all masters off; 16/17 all on, the odd one a single pixel off by three levels
in `ra_ant` that a reshoot of that scene did not repeat) and timed on Windows against the
parent and the final build: the desert base 16.72, 18.41 and 17.87 ms, `td_base` 37.98,
37.92 and 37.46. It moved part of the loss into the next section (`smudges`, 0.04 to 0.21
ms on the desert and 0.07 to 0.51 on the base view) and did not remove it, so it is not
kept.

## Review: balanced timing and split builds

An independent review timed the parent and this branch's first array build (691155b3b) on
the same card with every run on the NVIDIA renderer and a balanced order (each round
parent, branch, branch, parent, then reversed; 4 rounds of 3 windows), so the position of
a run in its round cannot explain a difference:

- `td_desert_base`: parent 17.66 ms, branch 18.70; the branch slower than the parent runs
  beside it in 7 of 8 cases. The extra time again sat in the passes that wait for the
  card (`effects` 1.12 to 1.78, `markers + bars` 0.22 to 0.42, `post chain` 3.73 to 3.99),
  not in the converted sections (`mottle` 0.17 to 0.15, `rain + shroud draw` 2.12 to 2.23).
- The first GDI mission under shroud, not revealed: 13.98 to 14.27. A separate 3-round
  run: `td_base` 37.57 to 37.67, the desert under shroud 11.49 to 11.66.

It also built each half of the conversion on its own over the parent (4 rounds, order
reversed each round):

| view | parent | mottle + tiberium ground only | shroud only | both |
|---|---|---|---|---|
| td_desert_base | 17.09 | 18.34 | 18.59 | 18.20 |
| td_base | 37.52 | 37.63 | 38.84 | 37.48 |

Either half alone carries the whole desert loss, so the loss is not the shroud's in
particular, and no subset of the conversion is a measured win on Windows. (Both designs
left the parent at the start and end of every round, where the same build runs faster
than in the middle; see The dial round, which finds no loss once positions are balanced.) On `td_base`
the mottle and tiberium sections still get cheaper (0.30 to 0.23 and 0.23 to 0.17 ms) and
the frame does not.

The same review found the pictures identical on Windows as well (parent twice, branch
twice, `td_base`, `td_desert_base` and five play scenes: 48/48 identical across the four
runs), and no leak over 2,050 frames on the Mac.

## The dial round

After the review the array path went behind the `overlay_arrays` dial, off by default,
and the four functions went back to the parent's order of state changes around each
`glBegin`/`glEnd`, so that the default draw is the parent's call stream. Measured from
commit `0a843e7c8` (the same code as the final commit, whose changes after it are comments
and these notes).

Pictures, Mac, frozen settings, `--jobs 2`, from the play folder (unchanged since the
parent sets were shot):

- Dial off (the default): all on 17/17 and all nine masters off 17/17 byte-identical to
  the parent sets, in a second run of both. In the first run's all-off set `ra_skirmish`
  had two pixels two levels off on the ore field's glints; two reshoots of that scene from
  this build and two from the parent all matched the parent set, so it is the known glint
  flicker.
- Dial on (`gfx overlay_arrays 1`, confirmed in each log): all on 17/17 and all off 17/17
  byte-identical to the parent sets, twice, and the two runs identical.

Mac timing, the parent, dial off and dial on, 3 rounds with the order rotated, 3 windows
of `framebench 60` each, 1600x900, frame median and the sum of the three sections, ms:

| view | parent | dial off | dial on | sections: parent | dial off | dial on |
|---|---|---|---|---|---|---|
| td_base | 25.54 | 25.29 | 25.65 | 0.91 | 0.95 | 0.97 |
| td_desert_base | 10.26 | 9.50 | 9.77 | 0.84 | 0.77 | 0.83 |

Neutral, as before.

Windows, the RTX 3070 rig, every run on `NVIDIA GeForce RTX 3070/PCIe/SSE2`, frozen
settings, 1600x900. First the parent, dial off twice and dial on, in the order parent,
off, on, off, parent and reversed, 4 rounds (40 runs), frame medians in ms:

| view | parent (first) | dial off | dial on | dial off | parent (last) |
|---|---|---|---|---|---|
| td_desert_base | 17.54 | 17.83 | 18.29 | 18.16 | 17.00 |
| td_base | 38.40 | 38.19 | 38.11 | 38.61 | 38.17 |

The desert numbers rise towards the middle of the round whatever is running there: the
dial-off build issues the parent's GL calls, yet sits 0.3 to 1.2 ms above the parent
runs beside it. So the same executable (the parent) was run in all five places, 4 rounds,
order reversed each round: 17.45 ms in the first place, 18.02, 18.48, 17.98 and 17.84 in
the last. One build, 1.0 ms slower in the middle of a round than at its start. The
earlier balanced design (parent, branch, branch, parent) and the split builds put the
parent at the ends of every round too, so this drift alone is the size of the loss they
measured.

Then the parent and the dial-on build alone, alternating so that each runs first in half
the rounds, 6 rounds (12 runs), on the desert base: parent 17.83 ms, dial on 17.93; the
per-round difference (dial on minus parent) -0.16, -0.71, -0.21, -0.27, +0.17, +0.48, a
median of -0.18. The `mottle` section 0.17 to 0.15, `rain + shroud draw` 2.17 to 2.14,
`effects` 1.00 to 1.28, `post chain` 3.53 to 3.47. No loss, and no gain.

## Verdict

The calls go (55,000 to 64,000 a frame on the Tiberian Dawn views, 24,000 to 35,000 on
Red Alert's) and every picture is unchanged, but no view runs faster on either platform.
On the Mac the sections move by a tenth of a millisecond, within their noise. On Windows
with NVIDIA the mottle and tiberium sections get a few hundredths of a millisecond
cheaper and the frame does not: these vertex calls are cheap on NVIDIA's driver, so the
expected saving, which rested on them costing what the calls removed in v0.8.1 cost, is
not there. The desert-base loss reported before (about 1 ms, with either half of the
conversion) came from runs in which the parent always took the first and last place of
its round; the same build drifts by that much between those places, and with the
positions balanced the array path is level with the parent (17.93 against 17.83 ms, a
median paired difference of -0.18). Neither the loss nor a subset worth keeping is
established: no subset of the conversion is a measured win on Windows.

So the array path is not a speed change and is not switched on. It stays behind the
`overlay_arrays` dial, off by default, where the default draw is the parent's immediate
mode call for call; the dial exists so the array path can be measured again on another
driver without rebuilding it. Any later Windows comparison on that rig should give each
build every place in the round (alternate pairs, or rotate the order), because the drift
between places is as large as the effects being measured.
