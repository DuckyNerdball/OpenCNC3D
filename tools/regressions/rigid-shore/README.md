# Immutable vehicle attributes and shoreline records

Run against a full private installed runtime, a candidate renderer and the qualified
pre-change renderer. All arms use that runtime's same engines, content and settings.
The window stays hidden, settings/saves go under a fresh `--out`, and no real input
or desktop preferences are changed. The probe is portable to native Windows Python.

```
python3 tools/regressions/rigid-shore/probe.py --runtime /absolute/runtime --exe /absolute/candidate --baseline-exe /absolute/original --out /absolute/new/vehicle-proof
python3 tools/regressions/rigid-shore/probe.py --runtime /absolute/runtime --exe /absolute/candidate --baseline-exe /absolute/original --out /absolute/new/coast-proof --campaign --state-checks
```

Each game runs original, witnessed and fast arms with seed 4242. The vehicle scene
constructs a real 20-building base with three active AIs. Both cases advance 240
actual consecutive engine frames. The coast case uses native SCG01EA, not an
inland battle view. State checks additionally compare camera departure/return,
Classic/Enhanced, fog/reveal and a same-process mission restart. Every corresponding
PNG must be byte-identical. A missing fixed campaign seed changes Red Alert's
ambient aircraft/unit poses; do not attribute such an uncontrolled comparison to
the renderer. The reference witnesses rebuild original CPU data, read actual GPU
bytes and check exact attributes/material ordering and bounds. They render the
original path; their runtime is not throughput evidence.

Each cache is independently controllable with `0`, `1` or `verify`:

- `OPENCNC3D_MESH_RIGID_ATTRIBUTES`: immutable UV/colour streams; current positions
  remain the existing CPU-generated shared GPU pose. Varying colour/UV, deformation,
  animation, normals and partial fog retain their original paths.
- `OPENCNC3D_SHORE_FILM_CACHE`: exact film records per coast cell, including current
  light/fog values. Geometry follows the existing source-generation invalidation.
- `OPENCNC3D_WATER_ARRAY_CACHE`: exact retained GPU bytes for film and crest. Shader
  time stays live; current GL attributes and bindings are restored after each draw.

Native paired measurements enable these by default on Windows only. Mac defaults
retain the original paths because its total live frame time did not improve.
The editor retains the original per-frame paths on both platforms.

For each refusal branch, run the coast case into another fresh output with
`--fault array-load`, `array-alloc`, `array-store`, `array-check`, `array-budget`,
`film-alloc` or `film-budget`. The probe requires an actual refusal, zero mismatches,
and unchanged pictures. It checks the 32 MiB film and combined 16 MiB GPU-stream
CPU-capacity ceilings. GPU retained payload cannot exceed that stream capacity;
transactional replacement can temporarily retain both old and new stores.
Mission shutdown releases the film records and both stream stores.

Compilation, these correctness witnesses, native visual checks and real-loop
performance are separate evidence. Measure performance with ordinary pipelined
TAA in `livebench ... play`, with verification/readback disabled. A synchronized
screenshot/frame benchmark is not evidence for live FPS. Keep the broad performance
reports open while the user's 60 FPS target remains unmet. Do not open a visible
window without specific permission for that run.

After the normal coast/state proof passes, `fallbacks.py` can reuse its qualified
original scripts and pictures for 14 fast refusal replays, avoiding repeated
original/reference arms. Supply `--runtime`, `--exe`, a fresh `--out` and
`--baseline-proof /absolute/coast-proof/results.json`. It verifies reference script,
engine and image hashes, requires real refusals, compares every state picture
exactly, and checks retained byte ceilings. These refusal replays do not claim
reference readbacks: the separate normal witness arms perform those checks.
