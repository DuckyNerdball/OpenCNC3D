# Local player destruction limits

Direction, 3 November 2026: show at most 20 corpse families, including each
body's torn pieces, weapons, pools, spray and projected blood decals, in the local
tactical viewport. Keep the existing global 200-body storage/physics budget
separate. Camera movement must not delete saved bodies or their effects. Newest
visible deaths win, with a stable owner key for ties. All rendering passes share
one cohort. The sidebar is outside this viewport.

Building physics separately admits at most two visible destruction families.
Off-screen deaths use particles/scorch only. Leaving the viewport retires an
existing physical family and releases its slot; returning does not replay it.
Direction, 4 November 2026: vehicle, aircraft and vessel destruction now
admits at most two physical families in this same local viewport, in a separate
budget from buildings. Enhanced wrecks and cartridge shatter share those vehicle
slots. Overflow and off-screen deaths retain native explosion particles, shock
pulses and scorch marks; leaving the viewport retires the physical family and
returning does not replay it. Completion, load and restart release ownership.
The infantry storage/physics budget remains unchanged.

Run against matching staged native runtimes on each platform:

```sh
python3 tools/regressions/local-destruction-view/corpses.py \
  --runtime /absolute/runtime --exe /absolute/runtime/cnc_eyes \
  --out /absolute/private/corpse-check
python3 tools/regressions/local-destruction-view/buildings.py \
  --runtime /absolute/runtime --exe /absolute/runtime/cnc_eyes \
  --out /absolute/private/building-check
```

On Windows use the renderer `.exe`; the fixture chooses native engine DLLs.
The corpus creates isolated maps, saves and preferences, and forces Enhanced
mode after loading the preset. It never opens a visible window.

Six corpse cases cover Dawn3D and RA3D, under-cap parity with the diagnostic
limit switch disabled, two separated clusters of 40 deaths, an empty camera
view, and return to the first cluster. Assertions check 80 retained bodies,
20 selected owners per occupied view, disjoint local cohorts, bounded associated
effects, zero drawn corpse effects in the empty view, and exact picture parity
under the cap and on returning to an unchanged scene. The diagnostic
`OPENCNC3D_CORPSE_VIEW_LIMIT=0` disables the rendering limit only for comparisons.

Two building cases check that off-screen deaths reserve no slots, visible pairs
are admitted, panning releases them, and two further pairs in alternating views
can use the released slots. Also run the existing fourteen-case destruction
suite and G384: a camera-budget pass alone does not prove particles, scorch,
vehicle behavior or fragment picture parity. Inspect actual captured scenes.

The vehicle regression uses matching engine modules explicitly:

```sh
python3 tools/regressions/local-destruction-view/vehicles.py \
  --runtime /absolute/runtime --exe /absolute/runtime/cnc_eyes \
  --engine-root /absolute/runtime --out /absolute/private/vehicle-check
c++ -std=c++14 tools/regressions/local-destruction-view/vehicles.cpp \
  -o /absolute/private/vehicle-budget-check
/absolute/private/vehicle-budget-check
```

Sixteen native cases cover both games separately: Enhanced small-event image
identity with the cap enabled/disabled, crowd overflow, off-screen deaths,
viewport retirement and slot reuse, final expiry, mission restart, cartridge
shatter and Classic small-event image identity. RA's older modes without 3D
vehicle physics are recorded as unaffected. The Enhanced crowd also compares
native particle-pool and scorch/crater draw reports against the uncapped control.
Two additional crowd cases leave the policy switch unset and prove the compiled
default has the same viewport behavior and images as explicitly enabling it.
Use `--only-defaults` for those two installation smoke checks.
`OPENCNC3D_VEHICLE_DESTRUCTION_VIEW=0` is a diagnostic comparison only; normal
runtime uses the two-family rule. The ownership unit checks mixed Enhanced and
cartridge owners, independent building slots, recycled engine IDs, duplicate
completion and stale tickets after reset.
