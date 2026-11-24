# Blood-splat FIFO equivalence

Windows reuses oldest ground-splat slots during `ir_drops_step`, then rotates the
vector once before returning. The renderer and other consumers see the identical
oldest-first array, existing 12,000 global cap and local 20-corpse/effect selection.
There is no new allocation, change to randomness, physics or visible effects.
Mac retains its original measured default. `OPENCNC3D_BLOOD_SPLAT_FIFO=0` restores
the repeated front-erasure path; `=1` selects the FIFO path.

Run `probe.py` with absolute `--runtime`, `--exe`, `--baseline`, `--helper`
(the top20 probe) and a fresh `--out`. Both games independently create 240 deaths
and photograph six physics/effect ages. Original, disabled and enabled pictures
must match exactly. Native disabled/enabled diagnostics hash every explicit field
of ground splats, drops and object splats in draw order; all six age witnesses
must match. Positive overwrite/rotation counters prove the full-cap path ran.
Struct padding is excluded. The hash runs only for diagnostics.

Compile, hidden native correctness and native performance are separate checks.
Use paired real-loop timing; these correctness checks do not assert a speedup.
