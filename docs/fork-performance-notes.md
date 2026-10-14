# Fork notes: Enhanced-mode performance and skirmish saves (branch `hayden`)

Work on top of v0.8.0 (`63df500`), 2026-10-11 to 2026-10-14. Each change is a
`patch-*.py` script of exact-anchor replacements (refuses if an anchor is not found
exactly once), so the set re-applies deterministically on a fresh snapshot.

## Symptom
Enhanced mode at 3440x1392 on an RTX 4070: 5–10 fps with a small base, GPU 20–40 %
busy. The frame was CPU-bound in `draw_mesh` (game/cnc_eyes.cpp): remastered-building
batches were being rebuilt on the CPU in every pass (picture, cutout, normals, sun
map, mirror) of every frame instead of coming out of the batch cache (game/mesh_batch.h).

## Profiler additions (all behind OPENCNC3D_FRAMEPROF)
- `OPENCNC3D_FRAMEPROF=2`: `glFinish` at every mark, so a section's time includes its
  own GPU work instead of the whole frame's landing on the first call that waits.
- Section cap 40 → 96; per-function marks in the sun, mirror and normals caster passes,
  the sidebar and the pieces.
- On the FRAMEPROF line: `mesh:` hit/upload counters; `cache:` entries, MB, evictions
  and `big:` the meshes holding the most bytes; `up:` why each batch was built
  (stable-key miss, per-frame pose path, split tier, extra-matrix piece, animated,
  touching shroud, empty) and `top:` the worst meshes with the draw path that built
  them; `stab:`/`keydiff:` which key field differed on a stable-path miss.

## Fixes, in the order they were found
1. **Waved flag** (`mesh_wave_hits`): `ebld_wave` marked the whole pack mesh
   `gpuDeformed`, and `draw_mesh` then refused to cache any run of that mesh. Now only
   a range holding a waved triangle counts as deformed; the run mergers draw the
   flag's piece alone so the runs around it stay cached.
2. **Standing pieces that "move"** (`ebld_mx_still`): once a building is held or
   damaged, every piece of its clip (a Weapons Factory's door) is drawn through the
   extra-matrix path, which was cached only for loose bodies asleep. A matrix equal to
   last frame's is now drawn as rest pose; the key already carries the matrix.
3. **Empty builds** (`MeshBatch::finish`): a walk that produced no vertices — a piece's
   run holding no triangle of the pass being drawn — returned before storing anything,
   so the same range was walked, keyed and missed again in every pass of every frame.
   An empty result is now stored as an empty entry. This was the largest single cost.
4. **Eviction**: over a cap the cache dropped the quarter used longest ago, which at a
   full cache in steady state included live entries. It now drops only entries not
   drawn in the last two frames, else the new entry is not kept.
5. **Damage glow flicker**: the glow's alpha scale (two sines) is a byte in the batch
   key, so each distinct value was another cached copy of the glow mesh, up to 256 per
   building (442 MB seen). Quantised to sixteen levels.
6. Smaller: whole-object cull per pass (`cull_objects`), static/dynamic mesh split
   (`mesh_split`), cache caps raised, `shadow_every`, `reflect_trees`,
   `tree3d_normals`, `water_reflect_every`, `wasd_scroll`; a PERFORMANCE TWEAKS section
   on Options → Visuals → Advanced exposing the dials.

Fenced mean on the test base went 222 ms → ~85 ms over the set. What remains is the
genuine cost of the passes (light, mirror, normals), the per-tick text-dump transfer
from the brain, and a collapsing building's live shards until its wreck settles.

## Skirmish save/load (fork only; upstream refuses by design)
Upstream refused saving a skirmish because the slot did not record the game mode and
a reload through the mission path converted the match. The engine's `CNC_Save_Load`
already accepts `GAME_GLYPHX_MULTIPLAYER`. The slot index now records a skirmish flag
(byte 103, previously zero; old indexes still read), the load hands the engine the
recorded mode, and a slot loads only into the kind of game it was saved from: a
skirmish slot from the pause menu of a skirmish running on the same map. Rows list
as `(SKIRMISH)`. Bare `cnc_eyes.exe` needs `--savedir PATH`; the shell supplies one.
Tested: save and load mid-match, AI still playing, sidebar intact.
