# Hidden rigid-mesh cull regression

The renderer previously prepared/re-uploaded invisible rigid meshes before its
final cull. The early path uses a conservative transformed source box, but keeps
all ordered texture/material runs and final current colour, normal and UV.
Visible meshes and varying/deformed sources retain their existing vertex path.
Source bounds belong to the mesh allocation, reset on reload, and reject no
non-finite result. Animated UVs, wind, deformation, part spin, per-vertex ground,
colour changes, neutral rocks and fragment collection retain their old paths.

`OPENCNC3D_MESH_EARLY_CULL=0` disables the early path.
`OPENCNC3D_MESH_EARLY_CULL_VERIFY=1` keeps the original draw, including cache hits,
and checks its actual final cull, material runs and consumed current attributes
against each proposed replay. Read `MESHEARLYCULL` from `meshperfdump`;
zero mismatches without positive rejected/verified coverage is insufficient.

Run separately on native macOS and Windows with full installed content. Example:

```sh
python3 tools/regressions/rigid-cull/probe.py \
  --runtime /path/to/playable --exe /path/to/current/cnc_eyes \
  --baseline-exe /path/to/previous/cnc_eyes --out /path/to/new/results
```

The probe constructs an isolated skirmish base through normal controls, advances
3,000 ticks, then checks 240 consecutive moving frames in original, witnessed and
fast configurations for each game. Saves/settings go under the output folder.
All three final PNGs must match byte-for-byte and the witness must check more
than 100 proposed rejections without mismatch. `--ticks 21600` exercises a later
battle; `--game td` and `--game ra` allow separate runs. `--growth` overrides the
tracked base-building helper when copying this driver to another host.

This proves correctness and useful coverage. Timing and real sustained-play
performance are separate measurements; a pass does not close a performance bug.
