# Vehicle wreck glow: one colour pass

On Windows, a vehicle wreck’s ember glow bypasses shared pose storage. Its
geometry has no depth/normal sibling that consumes that storage. The complete
original vertex walk, alpha arithmetic, materials, frame cache, culling and
animation remain in use; this removes CPU corner bookkeeping and a separate
position-buffer upload. Mac retains its measured original default.

`OPENCNC3D_VEHICLE_GLOW_SINGLE_PASS=0` restores the shared-pose path; `=1`
selects the single-pass path. `meshperfdump` reports positive skipped requests
and the restored outer sharing state. No new allocation or artwork is involved.

Run `probe.py` with absolute `--runtime`, `--exe`, `--baseline`, `--helper`
(the top20 probe) and a fresh `--out`. Each game is independent. Twelve heavy
vehicles are destroyed, then photographed through burst, flight, settling,
sinking and expiry, including a camera-away/return. Original, disabled, enabled
and native driver-state verification must match all nine pictures exactly.
The diagnostic must show positive skipped requests and restored sharing after
every frame. Keep any failed captures; do not weaken the image comparison.

Compilation and these hidden checks are separate from native visible verification.
A correctness pass does not establish a performance gain; use paired native timing.
