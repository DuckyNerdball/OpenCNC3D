# Two concurrent building destructions

Direction, 3 November 2026: at most two full building destruction sequences
in the local tactical viewport. Off-screen families release their slots; extra deaths keep particles and ground scorch, with no physical debris.
Vehicles keep their existing destruction. The shared budget covers Enhanced
building wrecks and legacy building shatter; a slot lasts through settling and
fade while visible, until those pieces are retired. Overflow is never queued for later physics.

Run the ownership/reset test:

```sh
c++ -std=c++14 tools/regressions/building-destruction/budget.cpp -o /tmp/destruction-budget
/tmp/destruction-budget
```

The exact resource census is separately checked with `python3
tools/regressions/building-destruction/test-census.py --out /tmp/destruction-census`;
it verifies overflow only requests its scorch atlas and preserves ordinary/shared
texture ownership.

Run hidden native tests against a fully staged matching runtime, separately on
macOS and Windows (Python 3):

```sh
python3 tools/regressions/building-destruction/probe.py \
  --runtime /absolute/path/to/playable --exe /absolute/path/to/cnc_eyes \
  --out /absolute/path/to/private/output
```

Fourteen cases destroy power plants, refineries, construction yards, medium and
heavy tanks, twelve tanks together, and twelve power plants together, in both
games. They assert actual engine removal after the death animation, native
renderer death events, and real frame-loop completion. Burst checks require
exactly two full wrecks, ten effects-only wrecks, twelve particle recipes, both
slots released, and another building admitted later. Vehicle bursts require all
twelve native vehicle deaths and no building admissions. Captures provide visual
particle/scorch evidence; log assertions alone do not prove appearance. Inputs,
commands, executable hashes and timings are saved alongside each run.

The RA engine's debug damage is 100 per hit, versus Dawn's 1000; enough hits must
be sent before advancing the simulation to kill the largest tested buildings.
Explicitly set `gfxmode 2`: saved presets can override `--enhanced` at startup.
No normal saves/settings or visible windows are used.

G384 kills the power plant before the neighboring barrels, reserving its full
wreck slot. Its existing pixel parity and fragment-reuse requirements remain.

Camera ownership and the 20-corpse/effect cohort are covered separately by
`tools/regressions/local-destruction-view/README.md`.
