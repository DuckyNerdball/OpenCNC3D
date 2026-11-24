# Exact static cloth arithmetic

The Windows path stores only the original time-independent envelope and rest
expressions. Every float input is keyed by its bits. Fixed direct-mapped tables occupy
less than 768 KiB together, allocate nothing, hold no model pointers and cannot grow.
A collision recomputes the original expression. Wave time, phase, motion, lighting,
triangles and animation pose caches retain their established behavior.

`proof.cpp` compares independent original expressions with misses, repeated hits,
parameter changes, direct-map replacement and signed zero. Build C++14 with strict
`-O3 -Wall -Wextra -Werror` for native Mac and Windows, plus Rosetta Intel and sanitizers.
The deliberately corrupted negative control must fail. Native `WAVESTATIC` verification,
exact images in both games and paired real-loop timing are separate adoption gates.

`OPENCNC3D_WAVE_STATIC_CACHE=0/1/verify` selects the private comparison. It is enabled by default only on Windows after native precision/image checks and a
paired comparison against the prior source. Mac compiles its original formulas without
the tables or lookup branches because the experiment showed no benefit there.
