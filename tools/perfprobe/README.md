# Growth and elapsed-time performance probes

Generate a Windows probe for either installed game:

```sh
python3 tools/perfprobe/make-growth-probe.py Dawn-GrowthProbe.bat --game dawn --live-minutes 20
python3 tools/perfprobe/make-growth-probe.py RA-GrowthProbe.bat --game ra --live-minutes 20
```

Place the selected batch beside `cnc_eyes.exe` in a complete private installation.
The RA probe also requires that installation's separate `ra3d` module. Keep each
run's `performance-growth-results` folder before starting another run.

The probe constructs 20 buildings through normal sidebar requests and build
times, with three AI opponents and seed 4242. Diagnostic immunity preserves the
human base. After revealing the map it keeps the camera on the human yard, so
the constructed base remains visible on both 64-cell and 128-cell maps. It loads the existing graphics preset, saves its
session settings,
visual preferences and saves under the results folder, and runs hidden and silent.
It does not lower rendering quality. `framebench` samples finish every frame on
the GPU and advance simulation ticks explicitly: they measure render throughput,
not elapsed play time. The optional `livebench` soak uses the real tick/present
loop and checks the requested wall duration, successful return and surviving
buildings before reporting success.

`livebench SECONDS [play|capture]` defaults to `play`. It temporarily uses the
player's pipelined TAA path (`taa_sync=0`), while `capture` retains the extra GPU
waits used for repeatable screenshot comparison. Both restore the prior mode
after the loop. These waits previously affected even the real-loop probe; old
timings must be labeled accordingly. Neither mode shows or focuses a window.
Invalid durations, including NaN and infinity, fail without entering the loop.

Record binary and preset hashes, GPU/driver, actual drawable size, mode, wall
duration, base counts, frame-profile windows and cache counters with each result.
A hidden macOS probe does not establish normal Windows GPU performance. Exact
image comparisons require `capture`; performance differences require paired
workloads, settings and modes. A passing soak is evidence for its workload, not
proof that the reported community slowdown is fixed.

`python3 tools/perfprobe/test-livebench.py` checks the production command's mode
selection, duration bounds and state restoration with a controlled loop. Native
gameplay and long-duration runs remain separate checks.
