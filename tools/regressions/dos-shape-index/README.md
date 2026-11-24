# Exact original sprite-name lookup

The fixed 8,216-byte index retains the first matching pack record, case-sensitive
names and missing results. It stores source indices only and clears before every
pack load/free. An unexpected container identity, size, invalid name or excessive
pack count rebuilds or falls back to the original scan. No texture, palette, pose
or graphical state is cached.

`proof.cpp` independently compares 134,460 queries with the original linear scan:
12 source generations, duplicates, missing/empty/long names, pointer changes,
invalid records and the 1,024-record boundary. Build with C++14, `-O3 -Wall -Wextra
-Werror`, then run natively on Mac and Windows; also run ASan/UBSan and Intel Mac.
`OPENCNC3D_SHAPE_INDEX=verify` checks every native lookup against the original
pointer. Native images, camera/fog/modes/destruction/restart and FPS are separate
checks. `OPENCNC3D_SHAPE_INDEX_BUILD_OK=0` exercises the original fallback.

Native precision and image checks pass on both platforms. Paired Windows timing and
the combined Mac bundle qualify its default-on path; no public release is implied.
