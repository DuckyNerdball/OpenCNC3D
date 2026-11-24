# Ordered particle-pool cleanup

The helper visits each input record exactly once and keeps survivors in their original
order. Removed tails are compacted once, without allocation. Disabled mode is the
original erase-and-revisit loop. The visitor must not resize or read this same pool's
intermediate ordering/size; callers were audited for that condition. Emission into
other pools, lifetime updates, RNG/hash counters, physics and all capacities stay intact.

`proof.cpp` independently compares the old erase loop, helper-disabled loop and compact
loop across 512 cases / 3,072 ordered state and emission comparisons, including empty,
all/none/alternating removals, changing side-effect counters and 16,000-record bursts.
Build with C++14, `-O3 -Wall -Wextra -Werror`; run native Mac, native Windows, Intel Mac
and ASan/UBSan. A deliberately unstable replacement must fail this comparison.

`OPENCNC3D_BLOOD_STABLE_COMPACT=0/1` is the launch-time comparison. `m3dump` reports
`IRPOOLSTATE`: an explicit-field, ordered hash of blood/drop/object-splat/casing data,
body splat counters and emission counters. It excludes padding and the implementation's
move counts. Compare OFF/ON states at each flight/settling/expiry/restart sample, separately
in both games and platforms, alongside exact native images and real-loop FPS. The
existing blood FIFO hash retains its earlier meaning.

Native ordered-state and image checks pass on both platforms. The compact loop is
enabled on Mac, where paired timing qualifies it; Windows retains the original loop
because its measured difference remains below drift. All physics/storage limits stay intact.
