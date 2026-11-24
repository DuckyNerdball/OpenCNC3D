# Base runtime pack preparation

The release pipeline runs `tools/bakery/bootstrap-runtime-packs.py` before
compilation in its isolated clean-main checkout. It regenerates scenario-independent
packs from this checkout's tracked ROM, original DOS archives, cameo artwork and
engine building table. It never copies packs from an installed game or the
historical dist snapshot. Existing scene/terrain/Enhanced bakers still run normally.

The helper uses the established pixel/format algorithms. It supplies the legacy
sidebar and buildup bakers with explicit current-source paths and directs derived
manifests into `build/base-runtime-packs/`, preserving authored manifests. The C&C95
cameo baker now initializes its repository override outside its docstring so it
also runs correctly on its own. The derived C&C95 report is redirected outside
the authored layout folder when invoked by the release preflight.

The minimal CONQUER.MIX comes from the actual engine building list and original
DOS buildups, then the existing Classic MIX baker adds the original shape members.
The resulting archive, plus the twelve original-art base packs (including legacy crate/wrench sprites), match the previously
verified Dock assets byte-for-byte. Repeat baking preserves those hashes. Map previews are generated separately after terrain resolution, for the nine retail maps and every race course in `game/missions` (a mission INI with a `[Rally]` section). Only the theaters used by those maps enter marker-colour clearance checks, so unrelated Snow/Sand atlases cannot change or reject them; adding the Winter course's theater leaves the nine retail previews byte-identical. This
is build preparation, not an artwork revision or a visual approval.

Run only in an isolated release checkout with tracked original inputs:

```
python3 tools/bakery/bootstrap-runtime-packs.py --root .
```

The receipt under `build/base-runtime-packs/receipt.json` records source identity,
original-input hashes, output sizes and hashes. Any missing input or failed baker
stops the release. Normal full packaging, native gates and per-game module checks
remain mandatory after this preparation; existing installed assets do not replace
them. The latest final candidate results belong to the dated release verification
report. Public source publication remains manual USER action only.

## Scenario packs

The same preflight also invokes `bootstrap-scenario-packs.py`. Its explicit `runtime-scenarios.txt` lists 95 shipped campaign, Special Ops, skirmish, editor-donor and multiplayer-fixture scenarios, the three Rally race courses (SCM70EA to SCM72EA) among them. Both source MAP/INI files and the engine INI must be tracked. A line may read `TARGET=SOURCE`, which bakes the cartridge scene SOURCE into `TARGET.pack` and writes TARGET into the pack header. One line uses it: `SCG99EA=SCG90EA`. The cartridge's SCG90EA is GDI's second N64 Special Ops mission, and in `game/missions` the code SCG90EA belongs to the renderer's own Test Map, which plays on SCG01EA.pack, so the cartridge mission is installed as SCG99EA. `game/make-build.sh` bakes an alias pack that a development folder is missing, since such a folder never runs this full bootstrap.

The helper creates only the existing relative bakery scaffolds, resolves terrain with `n64_terrain.py`, then runs the unchanged `bake5.py` for every listed scenario. It writes the packs to `tools/bakery/game`, where both platform packagers already collect them, and records hashes in `build/base-runtime-packs/scenarios.json`. No installed or historical scenario pack seeds the build. This also supplies all five required editor donors before Windows packaging checks them.

Independent scenario batches bake in four worker processes by default. Each worker redirects only the legacy generated role report to its own derived output directory. Terrain resolution finishes before these read-only source bakes start. A complete serial bake and the parallel bake produced identical hashes for all 91 packs of the list before the race courses joined it, without changing the tracked role report. The three course packs baked through the same `bake5.build` call are byte-identical to the ones the course staging step bakes.

Comparison with the installed packs found identical geometry, textures, terrain, heights and gameplay data. The only numerical difference was CUR06 cursor-animation rotation rounding, at most `5.820766091346741e-11` per matrix element. No pixel or format algorithm was changed.

## Clean-candidate omissions and fixture isolation

The full clean release run exposed two missing derived packs: `doscrate.pack` and `mappreview.pack`. Both are now regenerated and required by the platform packager. The former is byte-identical to the installed 2,972-byte pack; hidden fallback checks draw both crate kinds in Dawn3D and RA3D. The latter preserves all nine retail maps, palette, metadata and start markers, and now also carries the three race courses. Its current terrain-hole inputs change 59 pixels across the plain previews (and the same 59 in their marked copies) relative to the older installed pack; the gameplay renderer and its assets are unchanged. The output is read back and checked by the established baker. The complete base receipt contains 13 packs.

The preview bake runs with `--resolve-terrain`, so a map with a preview and no scenario pack of its own (Pitfall Quarter, SCM92EA, whose extracted `.MAP` and `.INI` are tracked) gets its terrain made by the baker, and the result is held to the committed `game/mappreview.manifest.json` by `--check`. `game/make-build.sh` asks the same question of every play folder it builds and rebakes a stale pack; both packagers refuse one; gate G463 holds it.

Two regression fixtures also assumed older visual defaults. G245 now measures a radius of eight around the shell projection, retaining every original geometry and centring assertion but excluding a background pixel nine rows away. G305 explicitly disables the independent per-structure second-sun override before checking the neutral path of the eight original dials. This leaves the saved default override enabled for players.

The historical G360/G364 presets now explicitly disable the second sun and keep the later-added effects grade at its original neutral values. The placement differences were early construction debris inheriting the new effects exposure, not building geometry: turning the performance controls off left the same pixels. With the historical controls pinned, all nine Dawn3D and eight Temple placement frames differ by zero interior pixels, and the eight RA3D frames by at most one, under the unchanged 24-pixel limit. The original reference PNGs and every checker threshold remain unchanged. This affects only regression fixtures, not shipped visual defaults.
