# OpenCNC3D: the brain

The shipped brain is the GPL Tiberian Dawn game logic, built as a platform-agnostic
library exposing the flat `CNC_*` C ABI, **running missions extracted from the Nintendo
64 cartridge**. Phase 2 of RA3D is adding the GPL Red Alert logic as a second bootstrap
target through the same stock remaster ABI. It is not yet the unified multiplayer brain.

Verified: `SCB01EA` (Nod 1) loads with **73/73 objects matching the N64 INI**
exactly (type, owner, cell, health); `SCG01EA` (GDI 1) matches 60/60. 3000 ticks with no
crash, deterministic across runs, and the simulation genuinely evolves (triggers fire,
`[TeamTypes]` reinforcements spawn and walk to `[Waypoints]`).

## Red Alert Phase 2 bootstrap

Phase 2 is **IN PROGRESS**. The current implementation contains:

- a headless Red Alert initialization path with `-HEADLESS`, fixed `-SEED=<number>`
  support, startup and scenario stage messages, asset-free palette and map fallback, and
  safe minimum capacities for the compiled weapon and warhead tables;
- the project-authored `tests/ra3d/RA3DBOOT.mpr` single-player fixture;
- `host/ra_bootstrap.c`, a dependency-free stock-ABI probe that validates API version
  258, loads the fixture with seed 19005, advances 60 ticks and emits a canonical static
  map and mission-timer report;
- `tools/ra3d/run-bootstrap-gate.py`, which runs the probe twice and compares both
  results with `tests/ra3d/expected-report.txt`; and
- Windows and macOS build-driver work that builds `RedAlert` beside `TiberianDawn` while
  keeping the temporary Red Alert bootstrap library out of the shipped module package.

The local 32-bit MinGW Windows build produced `RedAlert.dll` and `ra_bootstrap.exe`, and
the two-run gate passed. Its canonical report records API 258, requested seed input
19005, `RA3DBOOT`,
Temperate theater 2, rectangle `20,20,24,24`, and a live mission timer decreasing from 20
at tick 0 to 16 at tick 60. A macOS runtime result and byte-for-byte comparison of the two
platform reports are still pending, so the Phase 2 exit gate has not passed. The probe
intentionally does not use OpenCNC3D-specific object exports; those belong to Phase 3.

## Layout

```
patches/    our changes to upstream Vanilla Conquer (see "Rebuilding")
host/       cnc_host.c    dependency-free host over the CNC_* API (dlopen / LoadLibrary)
            run1.cpp      the loader used for the verified run above
            ra_bootstrap.c  stock-ABI Red Alert Phase 2 probe
missions/   SCB01EA/SCG01EA .INI (verbatim from the ROM) + converted .BIN terrain
            n64map_to_bin.py  N64 .MAP -> PC .BIN converter
tests/ra3d/ RA3DBOOT.mpr, a synthetic asset-free Red Alert bootstrap scenario
            expected-report.txt, the canonical 60-tick report
stubcontent/ DESERT.MIX, TEMPERAT.MIX  synthetic palettes (see "Game data")
lib/        TiberianDawn.dylib  prebuilt macOS brain
```

## Running

```
cd host && ./run1 ../lib/TiberianDawn.dylib ../missions/ SCB01EA 1 ../stubcontent/ 300
```
Args: `<library> <mission dir> <scenario> <build level> <content dir> <ticks>`.

## Running the Red Alert bootstrap gate

After the platform library and probe have been built, the Windows runtime command is:

```
py -3 tools/ra3d/run-bootstrap-gate.py --host build/win/ra_bootstrap.exe --brain build/win/RedAlert.dll --fixture brain/tests/ra3d --report build/ra3d/ra-bootstrap-windows.txt
```

The native macOS wrapper compiles its probe and invokes the same two-run check:

```
tools/ra3d/gate-ra-bootstrap.sh \
  brain/vanilla/build-native/redalert/RedAlert.dylib \
  build/ra3d/ra-bootstrap-macos.txt
```

When both reports exist, compare them directly:

```
python3 tools/ra3d/compare-bootstrap-reports.py \
  build/ra3d/ra-bootstrap-macos.txt \
  build/ra3d/ra-bootstrap-windows.txt
```

The equivalent gate passed locally against a 32-bit MinGW Windows build. The macOS command
and final comparison are wired into CI but have not yet supplied the two-platform Phase 2
exit proof.

## Rebuilding

**Do not clone anything.** `vanilla/` is committed source, not a working copy someone is
expected to fetch. It was gitignored until recently on the theory that the patch beside
it recorded every change we make to the brain; the checkout had quietly drifted ahead of
that patch, and CI spent a day red proving it. Cloning over it now is what broke CI a
second time, because git stops at "destination path already exists".

On macOS, run the script rather than the commands. It is what CI runs and what the
release is built from, so there is one recipe. During Phase 2 it builds both brains,
copies only the shipped Tiberian Dawn brain to the requested destinations, and leaves the
Red Alert bootstrap library in `brain/vanilla/build-native/redalert/RedAlert.dylib`:

```
tools/mac/build-brain-mac.sh              # -> game/TiberianDawn.dylib
tools/mac/build-brain-mac.sh game playable
```

What it adds to the raw cmake below is `CMAKE_OSX_DEPLOYMENT_TARGET`, taken from
`tools/mac/deployment-target.sh`, and an `otool` assertion that the finished library
really carries it. Without it clang stamps `LC_BUILD_VERSION minos` with whatever SDK the
build machine has, and dyld refuses a Mach-O whose floor is newer than the running
system: until recently the brain, `opencnc3d` and `cnc_eyes` all said 26.0, so every Mac
below macOS 26 rejected the release before `main()` ran. All three load in ONE process,
so a brain left at 26.0 beside two fixed binaries still fails and merely looks fixed.

The raw commands, for reference and for platforms with no script:

```
cd vanilla
cmake -S . -B build-native -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
      -DBUILD_REMASTERTD=ON -DBUILD_VANILLATD=OFF \
      -DBUILD_VANILLARA=OFF -DBUILD_REMASTERRA=ON -DSDL2=OFF -DOPENAL=OFF -DNETWORKING=OFF
cmake --build build-native --target TiberianDawn RedAlert -j8
```

The normal Windows driver likewise builds both DLLs unless `--no-brain` is supplied:

```
tools/win/build-win.sh
```

That is cross-compilation only. A successful build does not substitute for executing the
Red Alert probe under Windows and comparing its canonical report with the macOS result.

`CMAKE_OSX_DEPLOYMENT_TARGET` has to be passed on the command line, not left to the
`MACOSX_DEPLOYMENT_TARGET` environment variable: cmake only reads the variable when it
INITIALISES a cache, so an existing `build-native/` configured earlier keeps
its empty value and silently rebuilds at the old floor.

`patches/vanilla-opencnc3d.patch` is still the readable record of what we changed and it must
stay true: `tools/check-brain-checkout.sh` fetches upstream at the commit pinned in
`patches/UPSTREAM.txt`, applies the patch to it, and demands the result equal `vanilla/`
byte for byte. CI runs it on every push. So if you edit the checkout, regenerate the patch
in the same commit; if you move upstream, re-pin `UPSTREAM.txt` in the same commit.

## The four portability patches (and why each is Win98-safe)

The Win98 + Voodoo2 target is the reason the brain must stay dependency-free. Every patch
is additive and disappears or still applies on a 32-bit Windows build.

1. **`wwkeyboard_null.cpp`**: `COMMONR_SRC` was already headless (`soundio_null`,
   `video_null`) but hardcoded `wwkeyboard_win32.cpp`, the sole reason the target refused
   to build off Windows. All input arrives via `CNC_Handle_Input()`, so a null backend is
   correct. Deliberately NOT `wwkeyboard_sdl2.cpp`: that would bind the game logic to SDL.
2. **`-fms-extensions -fdeclspec`**: `dllinterface.h` is MSVC-flavoured (`__int64`,
   `__declspec`, types declared in anonymous unions). Compiler flags, not source edits.
3. **`opencnc3d_compat.h`**: `timeGetTime`, `GetTickCount`, `itoa`,
   `UNREFERENCED_PARAMETER`, `HINSTANCE`, `DWORD`, `MB_*`, `ProgramInstance`,
   `GetModuleFileNameA`, `MessageBoxA`. Red Alert uses the shared off-Windows shim too.
   It is entirely inside `#if !defined(_WIN32)`, so it vanishes on the Windows build.
4. **`dllinterfaceeditor.cpp` excluded off Windows**: the map-editor API is COM
   (`SAFEARRAY`/`HRESULT`/`VT_UI1`); neither headless bootstrap brain uses it.

## Game data: you do NOT need the original PC release

For the shipped Tiberian Dawn brain, the only hard requirement is a theater palette:
`<THEATER>.MIX` containing `<THEATER>.PAL`, or `Init_Theater` memcpys from NULL and
segfaults. `stubcontent/` holds synthetic ones and that is enough for a full load.
Original unit/building art is only needed if you want `GAME_STATE_LAYERS`, which we
don't: we have the N64 art.

The Phase 2 Red Alert probe instead uses explicit headless mode and the synthetic
`RA3DBOOT.mpr` fixture, so it can test initialization and ticking without original Red
Alert art, palettes or a `MapPack`. That is a bootstrap test only; it does not claim that
campaigns, player-facing rendering or a complete RA3D module are asset-free.

## Terrain: the cartridge ships its own conversion table

N64 `.MAP` = 64×64 `u16` **big-endian direct tile IDs**, clear = `0xFFFF`.
PC `.BIN` = 64×64 cells of `(template, icon)`, clear = `0xFF 0x00`.

The bridge is `<THEATER>.TL4`/`.TL8` in the extracted assets: **arrays of PC
(template, icon) pairs indexed by N64 tile ID**. (They were originally mis-catalogued as
palettes; TL = tile lookup.) Validated across all 77 usable maps: 93,126 cells,
24,216 template placements, **zero unmapped IDs, zero wrong-theater templates**.

The `.BIN` is optional: without it `Read_Binary_File` returns false and the engine falls
back to a legacy `[TEMPLATE]` INI section the N64 maps don't have, so the map loads clear.

## Gotchas that cost real time

- **`CNC_Start_Custom_Instance` concatenates `"%s%s.INI"` with no separator**: the mission
  directory argument must end in `/`.
- **Do NOT pass `-CD<path>` to `CNC_Init`.** `Parse_Command_Line` uppercases every argument
  and returns false on an unrecognised switch, silently skipping `Main_Game` entirely.
- **`GAME_STATE_LAYERS` is a rendering API, not a state API.** `Get_Layer_State` harvests
  objects as a side effect of `object->Draw_It()`, so with no SHP art it collects nothing
  and returns false *by design*. Enumerate the heaps instead.
- **`Get_Occupier_State` stride bug in EA's own code** (`dllinterface.cpp:5628`): it does
  `occupier + 1` after already advancing past the n objects, so the real per-cell stride is
  `4 + 8*n + 8`, while `memory_needed` is computed as `4 + 8*n`.
- **MEGAMAPS**: the DLL is built with it, so the engine map is 128×128 and cell numbers are
  not the INI's. Inverse transform: `ini_cell = y*64 + x` (`function.h:871 Confine_Old_Cell`).
- Westwood sources contain **Latin-1 bytes**: patch them binary-safe; a UTF-8 read of
  `startup.cpp` throws.
- Vanilla Conquer's `add_feature_info(RemasterTD BUILD_REMASTERTD, ...)` has a stray comma,
  so cmake **falsely reports the target as disabled**. Check the target list instead.
- `SCG71EB.MAP` / `SCG74EA.MAP` were previously 992 bytes (dumped raw before method `0x11`
  was reversed). **Fixed**: re-extracted with `tools/romdump/decomp11.py`, both now decode
  to exactly 8192 bytes. They are *genuinely all-clear* maps, which is not a decode bug:
  this codec's maximum match length is 18 bytes, so filling 8192 identical bytes needs ~455
  match commands (455x2 + flags + header ~= 975 vs the actual 992). They are almost
  certainly placeholders for the dormant, unplayable missions left in the ROM.
