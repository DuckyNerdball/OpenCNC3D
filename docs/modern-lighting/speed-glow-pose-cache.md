# Speed: the ember glow keeps its posed pieces

Key: `glow-pose-cache`. Branch: `visual/speed-glow-pose-cache`. Switch:
`OPENCNC3D_MESH_GLOW_KEEP=0` (environment) puts the old per-frame path back.

## What changed

The ember glow on damaged remastered buildings (`ebld_draw_glow` in `game/ebld_mod.h`)
is drawn additively over the building's standing pieces, at an opacity
that flickers on two sines of the engine clock. That opacity reaches `draw_mesh` as
`g_meshAlphaScale`, which multiplies every corner's own alpha. Because it changes every
frame, the glow sets `g_meshTransientColor`, and that switch turns off every cache in
`draw_mesh`: each standing piece was posed from its triangles, packed and uploaded with
`glBufferData` again for every piece in every frame.

Now a glow draw that is still in every other respect (the same conditions as a still
remastered piece: no waved cloth, no animation, no moving or sinking matrix, no flash, a
shroud signature) is kept:

- `draw_mesh` computes `glowKeep` beside `stable`, both from one shared list of what makes
  a remastered piece still (`stillPiece`), so a new per-frame input added to that list
  turns off both. Such a draw is keyed with the still-piece key less the opacity, and
  looked up in a store of its own (`g_meshGlowCache`, `game/mesh_batch.h`).
- The first time, the per-corner loop runs exactly as before but with the opacity at 255,
  so each corner keeps its own alpha. `mesh_glow_keep` uploads the finished vertices once
  (`GL_STATIC_DRAW`) and a second buffer that holds only the RGBA bytes
  (`GL_DYNAMIC_DRAW`).
- Each later draw only checks the opacity. If it differs from the one the colour buffer
  holds, the alpha bytes are worked out again from the kept alphas with the expression the
  loop uses, `(alpha * opacity) / 255` in integers, and the colour buffer is rewritten with
  one `glBufferSubData`. Then `mesh_batch_draw` draws the same runs, in the same order,
  with the colour pointer on that buffer.
- The last vertex's colour, which `mesh_batch_draw` leaves current the way the immediate
  path did, is the recoloured one.
- An entry not used for 60 frames is released (a building changing stage, being sold, a
  piece falling or the shroud moving all give new keys). The store is aged at the start
  of every frame (`draw_frame`), so it also empties while no glow is drawn: the camera
  away from the burning base, or every damaged building repaired, sold or destroyed.
  The store is bounded at 4096 entries and 64 MB; past either bound a draw takes the
  per-frame path, as before.

Untouched: the flash, the moving and sinking standing pieces (`ebld_draw_moving`), waved
cloth (`ebld_wave_body`), the cut pieces and the wreck's shards (they go through the
fragment collector, so a wrecked building's glow, which is drawn only on its shards, keeps
the per-frame path), and every other pass (shadow, normals, bake). The vehicle wrecks' glow
(`evhc_mod.h`) also sets `g_meshTransientColor` but is not a remastered building, so it
keeps the per-frame path; it is the obvious next step if a view full of burning vehicles
shows up in a profile.

## Why not the existing still-piece cache (step 0)

`OPENCNC3D_MESH_GLOW_DYNAMIC=0` lets the glow use the ordinary persistent cache with the
opacity in the key. That is correct but no use in play: the opacity changes every frame,
so every frame is a new key per piece, a full rebuild plus a persistent upload, and the
shared LRU fills with copies of the same geometry at old opacities, pushing out the still
pieces of everything else. It only helps a paused frame. The colour-only refresh keeps one
copy per piece and pays four bytes per corner per frame instead of the whole vertex.

## Identity

Pictures are byte-identical (cmp), parent `origin/visual/modern-lighting` 9edaaa7f0 against
this branch, the same play folder:

- `tools/visual/modern-shots.py`, all 17 pictures with every master on: identical, two
  runs of the branch.
- The same 17 with all nine masters off: identical, two runs.
- A moving case on SCG08EA (td_base) at 1600x900: seven pictures at five engine times and
  three sub-tick phases, so the flicker differs in each; two buildings hurt part way
  through so they change stage under a kept glow; then the camera pans and zooms out with
  40 more ticks. All seven identical to the parent, and each build's two runs identical.
  The run's counters: `MESHGLOW|hits=17561|builds=76|recolours=317|refused=0|entries=55`.
- A second moving case on the same view: the barracks and the headquarters are
  destroyed, the camera moves, and six pictures are taken over 140 ticks at six sub-tick
  phases while the wrecks burn out and the other damaged buildings keep glowing. All six
  identical to the parent.

- After the store was made to age every frame and the still-piece conditions were shared:
  the 17 pictures, all masters on and all off, identical to the parent in two runs each
  (and a third run of the on set from the committed build); both moving cases identical
  again, two runs each; `OPENCNC3D_MESH_GLOW_KEEP=0` identical. A run that looks at the
  burning base, turns away for 140 frames, then comes back gives the same four pictures
  as the parent. While the camera is away the store now empties (`entries=0|bytes=0`)
  where before it held its 76 pieces, 16.7 MB, until the next glow; on return the 76
  pieces are built again.

## Numbers

Both platforms were measured parent against branch, back to back, the order reversed
every round, with `OPENCNC3D_FRAMEPROF=1`. Each run times two windows after 20 warm-up
frames:

- **paused**: `framebench 60` with the engine clock standing still, so the flicker does not
  move and the branch rewrites no colours;
- **play**: 60 frames drawn one at a time, the sub-tick phase stepped before each
  (`tickalpha`), so the engine clock and the flicker move every frame as they do in live
  play and the branch rewrites every glowing piece's colours every frame. Each of these
  frames is finished on its own, so the frame times are a little higher than the paused
  window's.

The section is "cutout + translucent", where the glow is drawn; it was almost all glow
before (the cut-out and translucent pieces and the translucent object loop cost about
0.4 ms of it).

### Mac (Apple Silicon, Apple's GL 2.1), shared with other work

Two rounds; the frame medians move by up to 3 ms between rounds on views the change does
not touch, so the section times are the reliable figure.

| view | size | frame paused, parent / branch | frame play, parent / branch | section paused | section play |
|---|---|---|---|---|---|
| td_base | 1600x900 | 29.8 / 22.6 | 32.4 / 27.9 | 7.06 / 1.08 | 7.76 / 2.04 |
| td_base | 3008x1692 | 35.4 / 28.0 | 40.8 / 32.0 | 6.91 / 1.06 | 7.67 / 2.03 |
| td_desert_base | 1600x900 | 11.4 / 9.5 | 13.8 / 10.6 | 0.05 / 0.04 | 0.05 / 0.05 |
| td_desert_base | 3008x1692 | 23.4 / 21.8 | 25.2 / 21.1 | 0.05 / 0.05 | 0.05 / 0.06 |
| ra_snow | 1600x900 | 18.0 / 18.1 | 18.4 / 18.6 | 0.09 / 0.09 | 0.10 / 0.10 |
| ra_snow | 3008x1692 | 28.5 / 29.6 | 31.5 / 30.9 | 0.10 / 0.10 | 0.10 / 0.10 |
| ra_skirmish | 1600x900 | 13.5 / 13.0 | 13.0 / 12.2 | 0.03 / 0.02 | 0.02 / 0.02 |
| ra_skirmish | 3008x1692 | 26.8 / 23.7 | 27.0 / 26.7 | 0.02 / 0.02 | 0.02 / 0.02 |

- td_base: the glow's CPU time falls from about 7 ms to 1.1 ms paused and 2.0 ms in play,
  at either size: it is CPU work, so the resolution does not matter. The frame gains 4.5
  to 9 ms.
- The desert and the Red Alert views have no damaged remastered building in sight
  (`MESHGLOW` counts no entry), so the change does nothing there; their frame
  differences are the shared machine's noise.
- What the branch does instead on td_base: 76 kept pieces, 370 thousand corners, 16.7 MB
  of kept vertices on the card and in memory. The draws are the same 257 thousand over a
  run. In play, about 73 colour rewrites a frame, 1.5 MB in all, replace the rebuild and
  re-upload of the same pieces (about 13 MB a frame at 36 bytes a corner, and 367
  thousand corners posed on the CPU); paused, nothing is uploaded.
- Step 0, td_base 1600x900, two more rounds of three variants: parent 25.7 ms paused and
  28.5 ms in play (section 6.97 / 7.47); `OPENCNC3D_MESH_GLOW_DYNAMIC=0` 18.4 / 26.1
  (section 0.83 / 4.66); branch 19.5 / 22.6 (section 1.00 / 1.92). The ordinary cache is
  as good as the branch only while the game is paused.

### Windows

The 32-bit Windows build (cross compiled from this branch; the compile is clean) on a
desktop NVIDIA card under OpenGL 4.6, run hidden in the desktop session. Every run
reported the NVIDIA renderer and exited 0. Medians over six windows of 60 frames (two
rounds of three), ms.

| view | size | frame paused, parent / branch | frame play, parent / branch | section paused | section play |
|---|---|---|---|---|---|
| td_base | 1600x900 | 36.5 / 24.2 | 47.3 / 37.8 | 14.07 / 2.44 | 16.53 / 5.42 |
| td_base | 1920x1080 | 39.6 / 26.2 | 52.1 / 40.0 | 14.88 / 2.58 | 17.23 / 5.44 |
| td_desert_base | 1600x900 | | 21.1 / 21.4 | | 0.20 / 0.20 |
| td_desert_base | 1920x1080 | | 21.6 / 21.2 | | 0.20 / 0.20 |
| ra_snow | 1600x900 | | 27.1 / 26.8 | | 0.31 / 0.31 |
| ra_snow | 1920x1080 | | 28.0 / 27.6 | | 0.31 / 0.32 |

- td_base: the frame gains 9.5 to 13.4 ms, a fifth to a third of it. The glow's section
  falls by 11 to 12 ms, about twice what it saves on the Mac: the frame there is bound by
  the CPU and the driver, and the per-frame rebuild cost twice as much.
- The views without a damaged remastered building are unchanged within their spread.
- In play the section costs about 3 ms more than paused, and 2.5 ms of that is there in
  the parent as well (14.07 to 16.53), whose glow does the same work paused or not: most
  likely the waved cloth and the cut pieces, which move with the clock and are untouched
  here (not broken down further). The colours rewritten every frame cost the rest, about
  0.5 ms (about 0.3 ms on the Mac).
- Moving the clock also costs the parent, on Windows, about 4 ms in the sun shadow map
  (3.6 to 7.6), 1.7 ms in the ground normals and 1.1 ms in the specular target. This
  branch does not touch those passes; they cost the same in both builds.

### Frames running ahead of the card

Every bench above finishes each frame (the wait is its own section), and the modern
build's scripted runs also finish the anti-aliasing blend, so the colour store is never
still being read when it is rewritten. In live play the frames run ahead of the card,
where a rewrite of a store the card has yet to read could stall. Measured that way too
(`frames 1` with `OPENCNC3D_FRAMEPROF_END=swap`, `aa_mode 0`, the flicker moving every
frame, td_base):

- Mac, 1600x900, four rounds: frame 20.0 to 16.7 ms, section 7.44 to 1.94, the same
  section time as with finished frames, so no stall. The branch's frame is now bound by
  the card (the wait at the swap goes from 1.35 to 3.52 ms).
- Mac, every pass finished on its own (`OPENCNC3D_FRAMEPROF_SYNC=1`, so each section is
  its CPU and card time together): section 8.16 to 2.65 ms, every other section within
  0.1 ms. The card's time for the glow is unchanged (about 0.7 ms in both).
- Windows, two rounds: 1600x900 47.9 to 37.8 ms, 1920x1080 50.9 to 39.5 ms, section 16.84
  to 5.53 and 17.17 to 5.60, the same as with finished frames: no stall on this driver
  either.

### After the review fixes

The every-frame ageing and the shared still-piece conditions cost nothing measurable.
Mac, td_base, parent / the earlier commit / this one, alternating, two rounds: section
6.76 / 0.99 / 1.00 ms paused and 7.57 / 1.96 / 1.92 ms in play at 1600x900, and 6.79 /
1.01 / 1.02 and 7.60 / 1.97 / 2.01 at 3008x1692. Windows, td_base 1600x900 in play, two
rounds of three windows: frame 47.70 to 38.06 ms, section 16.39 to 5.31.
