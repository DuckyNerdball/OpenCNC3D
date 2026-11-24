# Native map importer

> **Current v0.9.4 delivery — 3 November 2026:** private release commit
> `729e0b4859fc9cb660d6b4b6c177aaee84147e73` is published as binaries and delivered to both Mac Dock
> installations. All eight fresh installed importer routes pass, including browser,
> preview, progress and completion; both games' approved HUD/fog checks pass.
> ZIP/RAR/RAR5/7Z use the built-in pinned libarchive 3.8.10 reader on Mac and
> Windows, preserving old launcher filename contracts and existing artwork.
> Custom RA mission INIs take priority over shared missions, with shared rules
> retained as a fallback; strict old-build negative controls pass on both platforms.
> Artifact/program hashes and preservation evidence are recorded in
> [release verification](verification/2026-11-02-v094-release/README.md).
> This supersedes the dated private-build delivery statements below. Public source
> refs remain unchanged and source publication remains a manual user action.

### Current verification notes

Relocating QA outputs must preserve the `--importmap-ui` input argument. One
7Z fixture was inside the old results folder and was mistakenly relocated with
the outputs; the corrected eight-route repeat asserts source existence, source
hash preservation and the actual menu controller. Player-facing menus remain
Enhanced in every mission graphics mode; `--classic` does not select the old
menu. The two additional legacy-controller QA cases explicitly set
`OPENCNC3D_MENU_1995=1`, separate from their private graphics preset.

The RA approved-HUD load message precedes the scripted Enhanced-mode switch.
Check the approved pack/layout fields and actual post-switch mode separately,
then inspect the native capture and fog/cache results. Runtime source and
accepted artwork did not change for these QA corrections. The current RA
lookup order is content archive, selected mission directory, then shared rules
as a fallback; the negative baseline and both corrected cases pass separately.


> **Historical 2 November combined delivery:** clean private main `8e8bedc76` reconciles the
> v0.9.3 release source with the importer and community fixes. Build0.9.3+8e8bedc
> is deployed to the actual pinned Dock installation and canonical runtime.
> All70 imported maps,8 hidden menu/import flows,110 backend and16 archive-edge
> checks pass; ZIP/RAR/7Z also import natively on Windows. User assets/settings/
> saves/launchers are unchanged. See `docs/verification/2026-11-02-top20-bugs/`.
> This supersedes the earlier importer-only build's source-preservation limitation.



## Archive and actual Dock verification — 1 November 2026

The current importer accepts `.zip`, `.rar` (including RAR5) and `.7z` bundles as
well as extracted maps. Inspecting a bundle counts each map by game and SP/MP
kind, previews its first terrain, and makes clear that Import adds every map.
The worker extracts only bounded map inputs into a private temporary workspace,
validates every candidate before import, and generates/preserves each heightmap.
Bundled replacement menus, shared rules, executable files and renderer packs
cannot replace installed assets. Sources remain read only. Each map commits
through its existing INI-last transaction; an I/O failure identifies the number
already committed. See `tools/map-archive/README.md` for pinned reader sources,
licenses, build/staging contracts, limits and archive errors.

A TD BIN without its INI can use a matching Remastered JSON terrain sidecar when
it supplies a native rectangle, theater and distinct multiplayer starts. This is
explicitly **terrain-only recovery**, not reconstruction of unavailable mission
objects/resources/scripts. The warning precedes bundle explanatory text so it
fits in the existing two-line status seat before Import is clicked. An invalid
or incomplete singleplayer source is not guessed into a mission.

The actual pinned Dock URL is `DOCK_CHECKOUT/playable/OpenCNC3D.app`. The original
v01 check mistakenly verified `MAIN_CHECKOUT/playable` only. Build runtime source from
current private main in `MAIN_CHECKOUT`, deliver its binaries/dependencies to the actual
pinned runtime as well as the canonical installation, and preserve both launchers,
settings, saves, maps and accepted assets. `tools/mac/gate-dock-launchers.py
RUNTIME --actual-dock` must match the pinned target before declaring delivery.
The hidden importer probe now clicks Main Menu → User Maps → Import Map through
the real controller before testing archive preview/progress/completion.

The Desktop CNC Maps and RA Maps corpus contains 70 distinct importable map
entries: 60 TD singleplayer difficulty variants, 5 TD multiplayer and 5 RA
multiplayer. All 70 passed cold simulation in the actual Dock's preserved mode
and in explicit Enhanced mode, all 10 multiplayer maps passed loopback tests,
and both games passed imported singleplayer save/load witnesses. Back 2 Back is
the one labelled recovery. Full results and limits are in
`docs/verification/2026-11-01-map-importer-fan-maps/`; the v02 native/browser
review is under `tools/art/remaster/viewer/map-import-v02/`. Earlier v01 evidence
remains historical. Native Windows execution and full human mission completion
are separate from this Mac import/start/short-simulation coverage.

## Initial implementation — 1 November 2026

The design review requested an **Import Map** button under User Maps, a file browser inside
the game menu, a map preview and progress while importing, Auto Heightmap, and
automatic singleplayer/multiplayer routing. This first version is the foundation
for testing the user's fan-map collection. Format validation is explicit; a
successful import is not a claim that every fan-made mission has been playtested.

### Input and preservation contract

Choose an extracted original Tiberian Dawn `.INI`/`.BIN` pair (either file can be
selected), or an original Red Alert scenario `.INI`, `.MPR` or `.MAP` containing
MapPack. Paired filenames are resolved without regard to case. The first version
handles native TD 64-cell dense terrain and Version=1 sparse 128-cell terrain,
and RA's original MapPack layouts. Archive bundles are supported as described
above. Later-game isometric maps, custom theaters and unsupported grid formats produce an actionable error.

Sources are read only. Imports receive a native-safe eight-character content ID
and go into `missions/user_maps/` or `ra3d/missions/user_maps/`, according to the
detected game. Identical imports reuse their existing entry; collisions preserve
existing maps. Exact original scenario/terrain/supplied-height bytes are retained
as `.source.*` files beside the working map. Scripts and original rule sections
stay intact; the working scenario adds owned kind/import metadata. A source BOM
is retained in the original and removed from the engine-facing copy. A receipt
and lossless terrain overview accompany the import. The catalog-visible INI is
published last; failed writes remove this operation's partial files.

Type detection uses explicit map-kind/SoloMission/multiplayer metadata before
player-house, scripting, original scenario naming and start-position evidence.
Mission waypoints must not alone move a scripted singleplayer map into skirmish.
Multiplayer requires at least two valid distinct starts. Existing editor-created
USER maps retain their earlier seat-based catalog policy.

### Height and runtime contract

Keep a valid original heightmap. Otherwise run the same native solver as the map
editor, using the playable rectangle, native building footprints and multiplayer
starts. RA uses the existing `ra_heights.py` template translation frozen into a
generated C++ table with input hashes. Interior floors remain flat, and RA open
water follows its existing flattening policy. Python is not needed at runtime.

TD reuses the established borrowed-theater/reskin path, reading the imported BIN
and height sidecar. RA reads its actual packed terrain through the engine and
loads its own 129×129 height corners into a 128-cell world before selecting the
theater lighting. Donor ground, tint and holes are discarded. Its playable-rect
median determines the ground datum. Imported RA singleplayer starts through the
custom-map engine entry, preserving its own Player and mission scripting.
Imported faction HUD/music identity comes from Player rather than the opaque ID.
Cold RA starts also mount the shared missions folder before native initialization,
so RULES.INI, AFTRMATH.INI and TUTORIAL.INI remain available when the selected
scenario lives in user_maps. Native HEADLESS mode tolerates missing rules and
otherwise leaves health/sight defaults unusable; a menu-only launch check cannot
certify this. The native checks therefore include 120 simulation ticks, sight,
Skirmish world creation and full two-peer loopback ticks without premature defeat.

The working INI removes a stale original Digest section after metadata changes;
the exact source retains it. Original SCM naming precedes generic leftover campaign
briefings when classifying maps. Native multiplayer slot order (including duplicate
waypoints) is preserved, while distinct valid starting locations are required.

Singleplayer entries appear under User Maps. Multiplayer entries appear in the
User Maps tab in Skirmish and Multiplayer. Network fingerprints resolve the
managed map folder on host and joiner. This does not add automatic map transfer;
peers still need the matching map installed.

### UI construction and review

The current accepted TD v06 and RA red/blue Enhanced kits supply the unchanged
housing, bitmap font, button art and feedback. The importer uses the existing
1016×636 visuals housing: Home/Up and status at y86, location/Go at y138,
600×304 file well at (38,194), 320×304 preview well at (658,194), and the fixed
footer at y550. Nine 26-pixel file rows share rendering and hit identities.
Long paths, filenames and titles are clipped by native glyph metrics; previews
use integer scaling with their aspect ratio preserved. Classic has a separate
320×200 layout driven by the same state and actions.

Folder navigation, typed paths, Home/Up, mouse selection/double-click and keyboard
navigation operate inside the menu. Map inspection and import run on a worker;
the menu continues drawing progress from actual preparation/generation/save
stages. Closing during a write waits for that operation to finish. Errors stay
in the dialog, and Import remains disabled until inspection succeeds.

The empty User Maps screen is reachable and offers Import Map in both games.
Special Ops/Mission Disks retain their own capability and button identities.
The first pixel review caught a combined game/type label that clipped; separate
rows fix it. Native menu preflight checks browser, location, inspect, ready,
pressed, progress, complete and error states at 720p/1440p, their seats and long
labels. Browser review and deployed UI/controller checks are recorded separately.
Technical evidence does not confer new artistic approval.

### Verification and delivery

Evidence is recorded under `docs/verification/2026-11-01-map-importer/`.
`before.json` hashes the actual Dock bundle/launcher, player settings/saves/maps,
current engines and critical HUD/fog assets before integration. Complete
source/build/test/deployment hashes and limitations belong in the final receipt.
All native checks must be hidden and silent. Build the combined current main
source and refresh both TD and RA executables before declaring this ready to test.
No public source, release or website publication is part of this task.

Hidden verification entry points:

- `--importmap FILE`: run the real backend and exit without a game window.
- `--importmap-ui FILE --importmap-shotdir DIR`: drive the actual hidden browser,
  preview, import and completion through the normal worker/controller.
- `--importmap-td-dir DIR --importmap-ra-dir DIR`: private test destinations.
- `--importmap-catalog`: report both catalogs and imported multiplayer fingerprints.
- `--harness 1 --rounds 1 --harnessusermap ID`: launch through User Maps and return.

Keep backend format/transaction tests, native compilation, hidden game checks,
browser/pixel review and native Windows verification as separate results.
