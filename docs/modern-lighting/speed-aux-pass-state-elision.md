# Speed: the ground-normal and bake passes stop resetting state they already have

Key: `aux-pass-state-elision`. Branch: `visual/speed-aux-pass-state-elision`. Default on:
the pictures are byte-identical. `OPENCNC3D_AUX_STATE_ELIDE=0` sends every one of the
calls again, as before, for an A/B or a suspect driver.

Under Enhanced, two extra world passes draw the visible world again, object by object,
through the colour picture's own draw functions: the ground-normal target
(`fx_draw_terrain_normals`, through `ebld_draw_normals`, `evhc_draw_normals`,
`m3_draw_object_normals`, `eterr_draw_normals` and the foliage) and the bake target
(`fx_draw_mesh_bake`). Each draw sets its state as if nothing were set yet. The state cache
(`game/gl_state_cache.h`) already skipped a bind, an enable or a client switch of the value
GL holds; this item takes the three kinds of repeated call it could not see.

## What changed

- **A uniform set to the value its program already holds is not sent**, inside the two
  passes only. Every uniform call goes through `fx_gl.h`'s entry points; those now ask
  `gls_uniform_held` first, which remembers the bytes each (bound program, location) was
  last given, by call kind. The bound program is the one the existing program shadow
  knows. A link or a delete of any program, a call the table cannot follow (an array of
  more than one element, a transposed matrix), and the start and end of each pass forget
  every value. Outside the passes the wrappers send every call, unchanged. The values sit in
  a fixed open-addressed table of 1,024 slots: no allocation, and forgetting is one counter
  step. Bytes, not values, are compared, so -0 after 0 is sent again.
- **A texture bound on a second unit is not wrapped in two unit switches when the unit
  already holds it** (`gls_bind_texture_on`, used by the stone material's normal bind,
  `stone_mesh_normal_bind`). The bind in the middle was already skipped by the cache; the
  switch to unit 1 and back to unit 0 around it changed nothing. This was the largest
  single source of repeated calls: 1,338 `glActiveTexture` a frame on the Tiberian Dawn base.
- **The array switches survive a `glPopClientAttrib` inside the passes.** A pop used to
  make the cache forget every client array switch, although the pop puts back exactly the
  ones the matching push saw, so the next draw re-sent each enable. A push of the vertex
  array bit made inside one of the passes now records the switches as the cache knew them,
  and its pop restores that knowledge. Outside the passes a pop forgets, as before.

Draws, programs, draw order and every value GL holds at every draw are untouched: only a
call that would set a value GL already holds is left out.

## GL calls

Counted with a GL interposer that hooks every entry point the renderer imports, around
the two pass functions only, on the Tiberian Dawn base view (SCG08EA, cam 47 48, dist 3000,
1600x900), one frame:

| Pass | Draws | Calls before | Calls after | Cut |
|---|---|---|---|---|
| Ground normals | 731 | 13,521 | 11,295 | -2,226 (16%) |
| Bake target | 354 | 6,296 | 5,872 | -424 (7%) |

On Red Alert's snow view (SCG03EA, cam 80 62, dist 3400): ground normals 10,328 to 9,295
(-1,033) for 342 draws, bake target 3,572 to 3,372 (-200) for 168 draws.

By entry point, before / after:

- Ground normals: `glActiveTexture` 2,102 / 764, `glUniform1f` 770 / 201,
  `glUniform1i` 214 / 23, `glUniform2f` 40 / 3, `glUniformMatrix4fv` 38 / 19,
  `glEnableClientState` 295 / 251, `glDisableClientState` 92 / 64. Unchanged:
  `glBindTexture` 1,317, `glEnable` 930, `glDisable` 934, `glUniform3f` 144.
- Bake target: `glUniform1f` 885 / 545, `glUniform2f` 39 / 20, `glUniformMatrix4fv`
  38 / 19, `glEnableClientState` and `glDisableClientState` 85 / 62 each. Unchanged:
  `glActiveTexture` 804, `glBindTexture` 774, `glUniform3f` 398.

Counted again from this build's final code, on both views: the same totals in every frame
window.

### Why not half

It was expected that about half of these calls set a value GL already held. Tracing every
state call in the two passes to its call site showed that most of what remains is real
change, not repetition:

- the foliage binds the four textures of a tree set on units 3 to 0 whenever the set
  changes from one batch to the next, which on that view is nearly every batch (141 a
  frame per unit, per pass): each bind is a different texture;
- a remastered building's broken faces turn polygon offset on for their block and off
  after it (`ebld_gl_broken`, 602 enables and 602 disables a frame): each is a change;
- the model batcher binds each piece's own texture, about 620 binds a frame that each
  change it (the cache already skips the ones that repeat);
- per-piece uniforms (placement, wind, gain) differ from piece to piece.

Removing those would need the draws reordered by state, or a state change deferred until
the next draw needs it; both change the order of work and are outside this item, which
leaves the order alone.

## Pictures

- The 17-picture scene set (`tools/visual/modern-shots.py`, frozen settings files), all
  features on: 17 of 17 byte-identical to the parent build (`cmp`).
- The same set with the nine masters off: 17 of 17 byte-identical.
- A second run of both sets: 16 of 17 and 17 of 17 identical to the first. The one
  difference was `ra_ant`, 2 pixels one level apart, in a run made while the machine was
  heavily loaded. Three further runs of that scene from this build and three from the
  parent were all identical to the parent's reference picture.
- A moving case, shot from the parent and this build after the same script: the Tiberian
  Dawn base with two buildings damaged mid-script (their stages change), the camera
  panned three times and the zoom changed, units moving over 125 ticks, four pictures: all
  four byte-identical. A Red Alert skirmish script was tried as well and dropped as a test:
  the parent itself does not repeat it (three runs of the parent gave three different
  pictures at two of its three moments).
- Seven harder scripts, 67 pictures a run: units ordered across a base, tanks driving
  through a Red Alert base, buildings destroyed and sold, a building placed from the
  sidebar while under construction, the nearest and farthest zoom, the Red Alert interior
  and snow, a mode round trip, and a mission loaded after another in each game. At least
  eight runs of each script from each build, alternated, under heavy load from other work
  on the machine: every picture matched the parent's first run except for two kinds of
  difference that the parent produces too. A mission loaded after another one gives one
  of three pictures for its first frames in the parent itself (around the plants at the
  map's left edge), and this build gave only pictures the parent also gives. And a single
  picture of a run sometimes differs by 3 or 4 pixels of 1 to 4 levels: the parent once,
  this build twice, never the same picture twice. The sidebar script run on its own, six
  times from each build, repeated exactly.

## Frame time, Mac

No measurable change. Parent and this build alternated per scene and size over two rounds,
the order reversed in the second, two 60-frame windows a run, frozen settings, frame
profiler on; medians of four windows, in ms. "Normals" is the profiler's
`post: ground normals` section (the ground-normal pass), "bake" its `post: specular`
section (the bake target plus the specular target). The machine was shared with other
work throughout, and the frame medians swung by up to 50% between windows of the same
build, so only the two sections are worth reading.

| Scene | Size | Frame parent | Frame new | Normals parent | Normals new | Bake parent | Bake new |
|---|---|---|---|---|---|---|---|
| td_base | 1600x900 | 32.57 | 36.78 | 2.26 | 2.49 | 1.53 | 1.71 |
| td_base | 3008x1692 | 47.55 | 43.86 | 2.54 | 2.38 | 1.71 | 1.59 |
| td_desert_base | 1600x900 | 29.02 | 15.30 | 1.00 | 0.87 | 0.69 | 0.61 |
| td_desert_base | 3008x1692 | 32.95 | 31.65 | 0.99 | 1.01 | 0.68 | 0.71 |
| ra_snow | 1600x900 | 20.92 | 23.16 | 0.92 | 0.93 | 0.78 | 0.82 |
| ra_snow | 3008x1692 | 39.92 | 51.58 | 1.04 | 0.94 | 0.92 | 0.85 |
| ra_skirmish | 1600x900 | 16.54 | 14.56 | 0.44 | 0.37 | 0.62 | 0.49 |
| ra_skirmish | 3008x1692 | 28.09 | 29.97 | 0.46 | 0.42 | 0.64 | 0.56 |

The section differences run both ways, from -0.15 to +0.23 ms, inside the run-to-run
spread. Timed through the interposer on the base view, two runs of the same binary with
the elision on and off (`OPENCNC3D_AUX_STATE_ELIDE=0`) gave 2.13 and 3.01 ms on, 2.24 and
2.84 ms off for the ground-normal pass: the same spread again. That fits how Apple's legacy
GL behaves, where the frame time hardly moves with the number of calls, and 2,650 calls out
of about 20,000 is too small a share to show there.

## Frame time, Windows

No measurable change either. The parent and this build, cross compiled the way the
shipped Windows build is, timed on a desktop PC with an NVIDIA GeForce RTX 3070 (driver
610.60, OpenGL 4.6) and an Intel i5-12400F, inside the logged-on desktop session, hidden
window, frozen settings. Two rounds, the order reversed in the second, three 60-frame
windows a run after a 20-frame warm-up; medians of six windows, in ms. Every run reported
the NVIDIA renderer and exited cleanly; none was rejected.

| Scene | Size | Frame parent | Frame new | Normals parent | Normals new | Bake parent | Bake new |
|---|---|---|---|---|---|---|---|
| td_base | 1600x900 | 36.28 | 37.87 | 2.93 | 3.08 | 2.30 | 2.42 |
| td_base | 1920x1080 | 40.14 | 40.51 | 3.15 | 3.10 | 2.38 | 2.40 |
| td_desert_base | 1600x900 | 18.22 | 18.05 | 0.85 | 0.88 | 0.63 | 0.64 |
| td_desert_base | 1920x1080 | 18.45 | 18.91 | 0.87 | 0.90 | 0.66 | 0.69 |
| ra_snow | 1600x900 | 25.86 | 26.41 | 1.19 | 1.22 | 1.31 | 1.30 |
| ra_snow | 1920x1080 | 26.95 | 27.88 | 1.23 | 1.25 | 1.35 | 1.33 |

The frame medians of single windows ranged over 4 to 5 ms within one cell (td_base at
1600x900: 34.3 to 39.8 for the parent), so the frame columns cannot separate the two
builds. The two sections can: across the six cells the ground-normal pass moved by +0.03 ms
on average (-0.05 to +0.15) and the bake section by +0.03 ms (-0.02 to +0.12). That is no
saving, and at most a few hundredths of a millisecond of cost.

## Verdict

The pictures are identical and the calls are gone, but the frame does not get faster on
either platform. On this NVIDIA driver a state call that changes nothing is cheap: the
history that made the Windows frame call-bound was hundreds of thousands of immediate-mode
vertex calls a frame, not a few thousand state calls, and 2,650 calls are about 1.5% of the
Tiberian Dawn base's 177,000. It was expected that about half of the two passes' calls
would be repeats, and that skipping them would save 0.8 to 1.5 ms; a sixth were, and they
cost almost nothing. The
change is safe to keep for the call count alone, but it is not a frame-time item, and the
two passes' real cost lies in the draws and the real state changes listed under
"Why not half".

## Files

- `game/gl_state_cache.h`: the pass scope (`GlsAuxScope`), the uniform table, the
  second-unit bind, the client-switch knowledge across a push and pop.
- `game/fx_gl.h`: the uniform entry points ask the table first.
- `game/stone_materials.h`: the stone normal bind uses the second-unit bind.
- `game/cnc_eyes.cpp`: one scope line at the top of each pass.
