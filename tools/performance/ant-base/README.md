# Damaged-base performance regression

This suite measures the shared Dawn3D/RA3D renderer with the player's unchanged
graphics settings and assets. It uses hidden background windows and isolated
settings/save destinations. Run sequentially; do not overlap GPU benchmarks.
Python needs Pillow for pixel comparison.

```sh
python3 tools/performance/ant-base/verify.py \
  --binary /absolute/path/to/cnc_eyes --runtime /absolute/path/to/playable \
  --output /private/tmp/ant-base-visual --suite visual
python3 tools/performance/ant-base/compare-installed.py \
  --evidence /private/tmp/ant-base-visual --runtime /absolute/path/to/old-playable \
  --output /private/tmp/ant-base-installed-reference
python3 tools/performance/ant-base/verify.py \
  --binary /absolute/path/to/cnc_eyes --runtime /absolute/path/to/playable \
  --output /private/tmp/ant-base-timing --suite performance
python3 tools/performance/ant-base/test-bounds.py \
  --repo /absolute/path/to/repo --out /private/tmp/ant-base-bounds
```

The installed-reference comparison must run before replacing that executable.
Both builds receive `--seed 12345`: Red Alert otherwise reseeds its simulation
from the clock, producing different idle unit directions, ore growth and shadows.
This is a test input only; ordinary game seeds remain unchanged.

Visual checks compare OFF/ON/OFF in the same process, at the same simulation tick
and interpolation alpha. Final pixels, full floating-point render targets and
gameplay/physics dumps must match exactly. The repeated OFF also detects unstable
reference output. The older installed renderer is compared separately for final
pixels and gameplay state. Tests cover the ant base, first Dawn mission, damaged
Dawn base, snowy Red Alert mission and existing destruction fixture, with wet/dry
lighting, advancing animation, camera/zoom changes and damage/settling.

`antperf -1|0|1` is a script diagnostic: -1 restores launch controls, 0 selects
the previous paths, 1 enables this patch. Interactive play always starts at -1.
The six launch controls default on:

- `OPENCNC3D_DAMAGED_FRAGMENT_BATCH`
- `OPENCNC3D_DAMAGED_ARRAY_SCOPE`
- `OPENCNC3D_BODY_PASS_CULL`
- `OPENCNC3D_WAVE_RANGE_SKIP`
- `OPENCNC3D_AUX_ARRAY_SCOPE`
- `OPENCNC3D_FRAGMENT_ACTIVE_REUSE`

Performance uses separate OFF/ON/ON/OFF launches, 20 warm frames and 100 samples
per launch, each preceded by an engine tick. Render size is 3008 x 1692. These
GPU-finished timings include renderer/driver work but exclude the preceding
separately executed simulation tick, and do not predict another platform's FPS.
`--case NAME` restricts cases; `--enable NAME` isolates launch controls for timing.
`--env OPENCNC3D_MESH_FRAGMENT_BUDGET_FULL=0` forces the existing retained-cache
fallback, for example. Retention remains capped at 64 MiB; the latest exact
instance/pass stream replaces its predecessor, including during active ticks.

The native bounds test extracts the actual source helper and quaternion code,
checks transformed vertices independently against the conservative body sphere,
tests unsupported-input fallbacks and verifies that an undersized-radius mutation
is detected. `--suite live` runs OFF/ON/ON/OFF in the real tick/present loop for five seconds
per launch, discarding its first 60 recorded frames. Run the existing
`active-fragments` lifecycle/matrix suites as well
when changing retained-stream lifetime or batching fallback behavior.
