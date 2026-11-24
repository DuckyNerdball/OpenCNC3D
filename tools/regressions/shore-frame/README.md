# Whole shoreline-film reuse

Hidden native checks for both games, against the previously delivered 0668f30 renderer.
Use the same installed content, engine and settings for both executables.

`probe.py --runtime PATH --exe CANDIDATE --baseline-exe BASELINE --out FRESH --campaign --state-checks`
compares original, full CPU/GPU witness and fast modes. Each arm advances 240 simulation
frames, then tests camera away/return, Classic/Enhanced, fog reveal and mission restart.
All eight pictures must match exactly. It verifies the predicted input key against the
complete rebuilt film and reads back actual GPU bytes. Normal tests require positive
hits/witnesses; allocation/load refusal tests require the appropriate fallback.

Add `--fault frame-alloc`, `array-load`, `array-alloc`, `array-store`, `array-check`,
`array-budget`, `film-alloc` or `film-budget` to test a refused optimization. Each cache
has a checked byte ceiling (2 MiB frame metadata, 16 MiB GPU-array CPU snapshots, 32 MiB
per-cell film). The frame metadata has at most one retained and one temporary input
vector; it does not accumulate frames. Mission shutdown releases all retained data.

`measure.py --runtime PATH --exe CANDIDATE --baseline-exe BASELINE --out FRESH --growth
 tools/perfprobe/make-growth-probe.py --game ra --ticks 21600 --live 20` runs an ABBA
comparison with all prior delivered optimizations held enabled. Only whole-film reuse
changes. Live-loop means, completed-frame samples and witness runs are separate evidence.
These tests never authorize or open a visible window. Mac keeps this optimization off
by default pending a measured native gain. No public release is implied.
