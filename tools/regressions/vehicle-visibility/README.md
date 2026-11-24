# Vehicle visibility submission regression

Large explored battles visit offscreen vehicles in every rendering pass. A vehicle’s
scrolled tracks/flipbook wheels formerly refused the rigid early-replay path, although
UV changes do not move geometry. The opaque sun pass also used a four-cell/sixteen-cell
whole-object slab before posing; its individual parts were rejected much later.

The new path replays the original material runs, colour, normal and exact last textured
UV for an invisible rigid range. It evaluates that UV in the original raw-coordinate,
hook, texture-aspect order. Animated/deformed, tread-byte, shrouded and other unsupported
inputs retain the full original path. Current posed opaque-part boxes give the sun pass
a tighter conservative vehicle bound, including terrain contact, articulation, flight,
rider lift and submarine dive. This uses the current pass matrices/receiver window;
colour, reflection and cutout passes retain their object-level behavior.

`OPENCNC3D_ACTOR_DEPTH_CULL=0/1/verify` is the diagnostic control. `verify` runs the full
original submission and compares the actual range bounds/current attributes. Both
ACTORDEPTHCULL and MESHEARLYCULL mismatch counters must remain zero. Allocation/source
refusal retains the original path; metadata is bounded to4MiB of retained box storage,256 models and4096 pieces/model.
The source cache clears at mission reset. No authored assets or simulation are changed.

Run against a full installed runtime, with a matching pre-change reference executable:

```sh
python3 tools/regressions/vehicle-visibility/probe.py \
  --runtime /absolute/playable --exe /absolute/candidate \
  --baseline-exe /absolute/reference --out /absolute/fresh-results \
  --game both --ticks 3000 --state-checks
```

Every native platform separately runs original, candidate OFF, verified original and
candidate ON. The scripted battle advances240 consecutive ticks. The state sequence
checks pan/return, near/wide zoom, Classic/N64/Enhanced, fog/reveal and mission restart;
all eleven pictures per mode must be exact within that platform. All scripts must pass,
both vehicle-bound and UV paths must have positive coverage, and the proper64-bit
MESHEARLYCULL verifier must report zero errors. Compilation and picture/state proofs
are separate from native paired real-loop timing. Never infer Windows results from Mac.
All runs are hidden. Foreground game checks require specific permission for that run.


`fallback.py` reuses a recent native reference proof with matching engine/pack/settings
hashes and checks source, allocation, metadata-budget and prepared-range refusal.
Every fallback picture must match that reference; the refused path must report actual
refusal while the independent supported path retains positive coverage.

```sh
python3 tools/regressions/vehicle-visibility/fallback.py \
  --runtime /absolute/playable --exe /absolute/candidate \
  --proof /absolute/native-proof/results.json --out /absolute/fresh-fallback-results
```
