# Structure and vehicle showcase video recipe

## Vehicle locations and isolated mission behavior — current27 October2026

Use the20 original mission areas shown in Desktop/OpenCNC3D Campaign Locations.
Select by **base ownership**, retaining each area's actual native theater. The user's
27 October correction explicitly says **use all 20 original locations**. This
supersedes the earlier GDI Temperate / Nod Desert restriction: all ten reviewed
GDI bases and all ten reviewed Nod bases are eligible, with no theater relabeling.

- GDI vehicles open at a random reviewed GDI War Factory and end as Nod at a random reviewed Nod base.
- Nod vehicles open at a random reviewed Nod Airstrip and end as GDI at a random reviewed GDI base.

Carry shuffle history across batches in `output/vehicle-showcase/location-history.json`.
Exhaust each faction's ten-location pool before repeating; avoid the previous
location when a pool refills. A persisted plan reserves its draw once, and a
technical rerender retains that exact pair and music. Do not reset the shuffle
for a new batch or limit it to the former five theater-matching bases.
`tools/video/showcase_locations.py select PLAN SUBJECT:GDI SUBJECT:Nod ...`
persists the seed, original location metadata, policy snapshot and history.
The seven Nod ground films draw seven different previously unused Nod openings
and seven different previously unused GDI endings. Original mission AI isolation
and native scene/encounter verification remain mandatory for every newly used area.

Keep MS-DOS, N64 and Enhanced on the same selected opening map, factory and
faction for that film. Reverse the ending faction for Nod subjects; preserve both original theaters.
Use native production for the opening faction: GDI War Factory doors and a
straight rollout; Nod Airstrip vehicle delivery. Do not disguise an Airstrip as
a War Factory. Preserve the approved follow/camera/amber-wipe/music rules.
Harvester keeps its noncombat harvesting/unloading story and duration exception;
the new faction/theater ordering applies to newly requested Harvester films too.
Previously approved exports remain unchanged until a revision is requested.

**Disable the original mission director in a private filming copy before boot.**
No campaign attack waves, patrols, reinforcements, triggers, production, automatic
base rebuilding, harvesting armies, cutscenes or victory/defeat transitions may
interfere. Clear Triggers, CellTriggers, TeamTypes and Teams, clear the Base build
queue, zero house starting credits and carryover money, and omit the original
mobile actor placements. Clearing triggers alone does not neutralize placed Guard,
Hunt or Harvest units. Preserve the selected original base structures, walls,
foliage, map data and native terrain pack. `tools/video/showcase_locations.py
prepare LOCATION_ID PRIVATE_DIRECTORY` creates this isolated private copy and
records original/private hashes; it never edits installed mission files.

Only introduce the film's explicitly recorded hero, supporting actors, designed
attackers and participating base defenses. Keep original actors omitted unless
reintroduced deliberately as part of the film; no inherited mission orders.
Native movement, targeting, turret motion, projectiles, suspension and effects
remain active for those designed encounters. Create attackers outside the actual
camera frustum, including their bodies and shadows, then make them approach.

Before recording, run a native idle probe for the entire film plus the production
prelude with no designed encounters. Require no unplanned actors, reinforcement,
production, combat, structure/layout/health changes or mission endings. During the
actual capture verify actor counts, births, targets, firing and permitted base
defenses against the saved encounter whitelist. A static INI edit or an idle pass
alone is not proof for a new encounter. Keep logs and source/private hashes in the
film plan. This is capture-only mission isolation; retain ordinary gameplay AI,
the Dock installation, the user's CFG and saves.

Current isolation preflight evidence is retained at output/vehicle-showcase/campaign-ai-isolation-v01/summary.json. Prepared copies with reported blockers are not ready to record.


Current direction: 19 October 2026. This is the current rule set for new structure and vehicle films and their revised capture tools. The historical implementation and original 22-film plans live in the turntable tools of the earlier film branch (`tools/turntable/`); those old binaries and captures do not establish the current build or visual settings.

### Verified original-mission capture isolation —27 October2026

The revised five GDI ground films are delivered for review at1440p60/40 seconds
in `~/Desktop/OpenCNC3D Vehicle Films/GDI-Vehicles-Review-v03`.
Current roots: Humvee `v31-production` (exportv04), Medium Tank `v27-production`,
APC `v28-production`, Mobile Rocket Launcher `v29-production`, MCV `v30-production`
(the latter four exportv03). Exact deliveries, hashes, music, source maps and
retirement totals are in `output/vehicle-showcase/gdi-batch-v03/completion-summary.json`.
All five native raw sequences have retired after hash-verified delivery; authoring,
settings, logs, receipts, useful QA and finals remain. Artistic approval is pending;
previously approved Mammoth and Harvester copies remain unchanged. The first
Humvee export is preserved under the review folder's Superseded directory.


For the MCV, retain the2.5-second driving follow and use native deployment, with
no fictitious vehicle weapon. In the current original GDI-02 layout it reaches
cell13,54 after the follow; deploy at11.75 seconds into Enhanced and move the
final amber theater wipe to31–32 seconds. The total remains40 seconds. The Nod
section deploys at1.5 seconds. Require the newly created Yard to finish native
construction and remain alive before cutting; distinguish it from original
mission Yards by native object identity/cell. Frame the Yard together with the attackers’ approach lane: native attackers can stop at weapon range outside a Yard-centered shot. The current GDI-02 MCV follow lead is3.3 cells east and1 cell south. Verify actual rendered unfolding before the wipe as well as native Guard state.
A failed early deploy can leave
an intact MCV rather than a Yard; a successful command or a pre-existing FACT
alone is not proof. Preserve failed attempt authoring/logs while revising.


Clearing INI teams and triggers alone left automatic helipad aircraft and low-power
building damage active. The private current-main Dawn3D filming engine additionally
requires all three flags `OPENCNC3D_BACKGROUND=1`, `OPENCNC3D_FILM_ISOLATE=1` and
`OPENCNC3D_FILM_PRIVATE_MISSION=1`. It disables autonomous base/director decisions,
free helipad/refinery actors, passive low-power damage and unsolicited superweapons.
Weapon buildings remain passive unless their exact native cells are listed in
`OPENCNC3D_FILM_DEFENSES` (semicolon-separated X,Y pairs). Designed mobile attackers
keep native movement and combat. Never put this engine or these flags in the Dock
launcher. The renderer, assets and full per-theater CFG come from the actual Dock
target; the private engine is built from current main. Record both sources and hashes.

All five eligible original locations passed600 native seconds with no unplanned
actors, shots, structure/health changes or endings; evidence is in
`output/vehicle-showcase/campaign-ai-isolation-v03/summary.json`. This idle evidence
must be followed by each film's rollout and encounter check. Production scripts and
exact location draws for the five revised GDI films are in
`output/vehicle-showcase/gdi-batch-v03/`. Original structure geometry and ownership,
INI terrain/overlay/smudges and native BIN/pack remain authoritative. Any scripted
initial repairs are recorded separately. Do not impose former custom-set wall/cliff
checks on original campaign layouts or silently alter their geometry.

Use native measured paths for per-sample following. Scripted selection and orders
may temporarily center the hidden camera on each actor separately between renders;
restore the analytic film camera before the next shot. Native camera distance clamps
to4200, so an arbitrarily large distance cannot make distant actors selectable.
For factories near the edge of an original map, constrain the pullback inward before recording. Check native source exposures for black map-edge corner polygons before delivery; do not extend original terrain or conceal the boundary with invented scenery.

Validate spawns by actual unlimbo/occupied cells: a failed native spawn may leave a
limbo object at127,127. Check full-body/shadow birth frames, subsequent entry and
incoming fire. Attackers may begin their offscreen approach during the driving
introduction, but no incoming fire may interrupt the required2–3 seconds of driving.

## Camera direction

Begin each new rotating 3D structure shot on the structure's **right side**, preferably its front-right three-quarter view. Move smoothly through the front toward the opposite side. Apply this in both N64 and Enhanced, including revised vehicle showcase camera paths. A vehicle’s heading and a structure’s front are different references: never label a positive world yaw “right” without viewing that asset and shot. Confirm the side against the actual structure's front/door/gate; a yaw sign alone is not sufficient proof. Classic retains its authentic fixed projection.

Use the actual front/door and a saved native view to identify the side. A world yaw sign is not a universal physical-side rule. The generic historical sign reversal was insufficient and is superseded by asset-specific camera references. For the current War Factory, the rejected view started at **+55°** and showed the plain wall; the requested **chimney/right-side** view starts at **−55°**, then sweeps **+1.5°/s toward the front**. Apply this in N64 and Enhanced, including vehicle-production openings. The actual closed/open/rollout comparison is saved in the current film’s `v05-production/factory-side-review/`; use its −55° views as the matching reference. Classic retains its authentic fixed sprite projection.

Aim at the complete footprint center. Maintain continuous position, yaw, pitch and distance within a shot and across a mode-comparison wipe of that shot. Establish the target and distance before entering freecam; drive each pose from absolute film time rather than elapsed capture wall time. Keep the accepted wider framing from the current showcase reference. Never horizontally flip models, insignia or footage to change sides.

## Smooth capture

Capture rotating N64 and Enhanced shots at **native 60 fps**. The engine remains at 15 ticks per second: explicitly enable smooth animations and render all four subframe phases `0, .25, .5, .75`. Update the camera on every output frame. A 30 fps recording merely relabelled 60 fps, duplicated frames, and optical-flow fabrication do not satisfy this rule.

The v04 vehicle films demonstrated that 60 fps metadata and four phases alone do not establish satisfactory motion. For the v05 Enhanced rerenders, use 1.5× internal spatial anti-aliasing and four real native exposure samples per output frame, over a 180° shutter: offsets .3125, .4375, .5625, .6875 within the 60 fps frame, with tick alpha (frame-within-tick + offset)/4. Drive camera and simulation interpolation at every sample; average in linear light before badges, swipes and picture fades. Record this capture-only AA override separately from the accepted F5 lighting defaults. Keep the camera sweep slow and continuous.

Review a contiguous full-resolution rotating section, including the mode wipes. Check camera pose increments, frame timestamps and adjacent frame differences for holds or jumps, not only contact sheets. Ground, model, turret and effects should follow the intended clock. Classic may retain its original sprite stepping. If further temporal sampling is used, record its shutter and sample times.

## Current build, assets and visual tuning

Resolve the actual Dock app and its working directory before each capture batch. Use its current native renderer, engine and installed assets, verified against current private main when runtime changes are required. Freeze executable, engine, model, paired material/specular files and relevant terrain inputs with hashes. Do not reuse an old isolated filming build or replace the user's installed assets for a settings-only rerender.

For every capture or technical rerender, resolve the actual macOS Dock app and
its working directory again. Snapshot the newest user-saved Dawn3D CFG from that
installation, preserving its exact bytes and SHA-256. The user's latest saved
lighting supersedes earlier film settings and dated compiled defaults. Keep the
complete global settings, per-mode rows and separate Temperate/Desert profiles.
Do not flatten one theater over another or reuse an earlier batch's defaults.cfg.

Before full recording and again before delivery, compare the live native loaded
settings against the exact saved profile selected for each theater. Missing
fields and unknown CFG keys are failures, not silently ignored comparisons.
Allow only the documented filming camera, smooth animation, AA and hidden FPS
controls; never override sun, secondary sun, object/terrain lighting, exposure,
bloom, effects or colour grading. Keep installed CFGs unchanged and retain the
previous approved movies. A new lighting export needs its own design-review review.

Use native GDI/Nod material mapping rather than multiplayer RED. Film existing approved models without geometry changes.

## Music selection

Randomize music independently for each new film from the nine approved structure-film tracks:

- We Will Stop Them / Deception (OST Version)
- Die (Remastered)
- ReCon (Remastered)
- Iron Fist (Remastered)
- Depth Charge (Remastered)
- Creeping Upon (Remastered)
- Airstrike (Remastered)
- Fight Win Prevail (Remastered)
- March To Doom (Remastered)

Use `tools/video/select_showcase_music.py PLAN.json SUBJECT...` to create and save a random batch seed and draw **without replacement** within the batch. Existing saved plans retain their selection; `--seed` reproduces a new plan deterministically. Mammoth and Harvester must receive different tracks. If a batch exceeds the pool, reshuffle only after exhausting it and avoid an immediate repeat. An explicit track request for a particular film overrides the random draw for that film only. In Trouble was requested for the trailer; do not hardcode it into every showcase.

Save the chosen title/version, source URL, file hash, seed and approved pool in each film's plan. Keep a chosen track fixed for reproducible technical rerenders unless the design-review requests new music. Use approved source recordings, including the requested official Frank Klepacki Topic uploads where available.

## Vehicle story — current general rule

All vehicle showcase films **except Harvester** begin with GDI and repeat a real War Factory opening in each comparison mode:

1. **MS-DOS:** the War Factory opens its doors and the featured vehicle drives out.
2. **Amber left-to-right swipe.**
3. **N64:** repeat the same GDI factory-door rollout.
4. **Amber left-to-right swipe.**
5. **Enhanced:** repeat the GDI rollout. Once the vehicle is clear, ease the camera wider with both an eased start and stop. Follow the vehicle driving for **2–3 seconds**, then have enemies attack it and show it defending itself.
6. **Amber left-to-right swipe.**
7. Continue the defense in the other theater as **Nod**, using Nod’s native faction materials rather than multiplayer RED. Keep the vehicle alive until the end of the final section.

Use native production, door animation, movement, incoming fire and projectiles. Measure the native rollout before timing the edit: a Mammoth needs roughly seven seconds to clear this factory. Do not interrupt its tethered exit track by ordering it through a wall. For Mammoth, turn its turret between several vehicles and include both cannon and Tusk rocket salvos. The v05 reference uses a 40-second edit to give the rollout, 2.5-second follow and multi-target fight room; it supersedes the older stationary-fight opening.

### Straight exits, arriving enemies and dressed bases — 19 October revision

The featured vehicle must drive **straight out through the center of the War Factory door**, with its hull pointing along its direction of travel. Keep that straight line until the complete vehicle has cleared the doorway and slab; only then turn toward the follow route. Do not let the original curved exit track make the vehicle slide diagonally through the jamb, and do not issue a movement order while its factory track is still tethered. Validate the actual centerline and hull heading in the rendered rollout, including N64 and Enhanced, before full capture. Preserve the right/chimney camera side.

**No enemy may materialize inside the filmed frame.** Create attackers beyond the actual camera frustum, with clearance for their whole model and shadow, then let them drive toward and attack the featured vehicle using native movement, targeting and weapons. Check the camera pose at the exact creation frame, including pullback and orbit; a map-cell distance alone is not evidence of being off camera. Record creation positions, projected screen positions, subsequent native movement, first visible entry and incoming fire. Do not use stationary in-frame spawns or merely hide them under an impact effect. Keep the initial 2–3-second hero follow before the fight begins.

Stage factory and refinery shots as **small believable bases**, rather than isolated structures on empty ground. Include a few appropriate supporting structures, short perimeter wall sections and open vehicle entrances. Add a few trees, foliage clusters and natural edge details that belong to the theater; use sparse desert trees/cacti in Desert rather than Temperate vegetation. Preserve a clear view of the hero and door, the straight factory exit lane, enemy approaches and the Harvester's resource/refinery route. Dress the same set consistently across the mode comparison. Use existing approved native assets; this staging does not authorize model redesigns.

The v06 production controls use explicit private filming environment flags for a measured native straight factory rail and enemy attack orders; ordinary launches retain the original simulation. Save the exact flags, binaries and scripts in the capture manifest. Do not make this a user setting or silently change normal or multiplayer production rules.

**Harvester is the exception:** retain the approved harvesting and refinery-unloading story, different theaters and maximum 30-second duration. Do not add a combat or War Factory opening. Its current revised edit remains 29 seconds. Correct the actual N64/Enhanced camera paths, rather than only updating this document. The v04 rejected GDI start was normalized −107° and Nod +45°; v05 physically reverses those starts to approximately +107° and −45°, then sweeps slowly at 1.5°/s. Do not flip footage or markings.

For a moving follow shot, record the deterministic native unit positions first, then interpolate that measured path at each exposure sample. Keep the pullback and target handoff continuous. Save the actual start views with each film so later captures can match the camera side from visible evidence.

## Story, transitions and audio

Retain the approved per-film story and timings, subject to the current vehicle opening above. Structure films show Classic placement, N64 placement, Enhanced placement, attack, damaged stage and destruction. Preserve a pause for the Enhanced placement's impact and dust. Use amber left-to-right swipes throughout, the existing platform badges and OpenCNC3D branding. Fade the entire picture and all audio to/from black/silence. Keep game action and effects audible under the music, and keep the whoosh subtle.

The native WAV tap records 66 ms for each 15 Hz tick: measured recorded time is approximately .99 times simulation time. Production/build-ready preludes advance the simulation too: read the actual engine frame from native TICKALPHA logs rather than counting only explicit tick commands. Map source samples from the actual tick clock and measured recorder ratio, including every subframe phase. Do not reuse the historical fixed-start-shortfall approximation. Check encoded true peak and fades.

## Background capture and delivery

Use hidden `--script` capture with `OPENCNC3D_BACKGROUND=1`, private missions, private settings/visuals/saves paths and no visible or focused game window. Preserve the user's settings and unrelated assets. Do not replace the Dock installation with a filming executable.

Verify the final export's actual dimensions, native frame cadence, frame count, duration, audio streams, full decode, black end frames, framing and movement. Classic's usable tactical viewport is narrower than the capture window; widening may require verified adjacent scene views, rather than cropping into black margins. Preserve original sprite animation cadence when taking those views. Deliver hash-verified versioned files to the existing Desktop film folder and retain prior versions.

A recipe update does not itself rerender the historical 22 structure films. New captures and requested rerenders must use these current rules.

## Verified v05 capture notes

The current factory fixture must be discovered by a friendly unit for the engine to expose its build choices. A private scout reveals it and moves off camera before production. `build HTNK; ready HTNK 3000` then reaches the native door sequence. Keep filming helpers, tech/immunity staging, settings and saves private. The Mammoth capture uses a private invulnerability switch until the final native destruction cue; this is film staging, not a delivered gameplay change.

The current renderer writes PNG with zlib level 6, which unnecessarily slows multi-sample capture. A capture-only `png_transport.dylib` interposes compression effort at level 0; it changes lossless PNG storage, not pixels, rendering or simulation. Freeze and hash that helper beside the unchanged current-main renderer/engine. Four source PNGs are integrated into one lossless exposure before compositing. No Dock installation or user setting is changed by these filming controls.

Leave enough map around the entire pullback and orbit. A factory placed near the original campaign-map edge passed timing/weapon tests but exposed a black void in the wide shots. The v05 final Mammoth set was moved into the interior of the full Temperate map before delivery. Review source starts, widest midpoints and ends for edge-connected black regions as well as ordinary Classic viewport margins; framing checks must cover N64 and Enhanced too.

Make repeated captures independent of saved private visual settings. A prior capture’s remembered Perspective flag could change the free camera’s frozen starting pitch on the next run. Start with a fresh private visuals profile each time and explicitly enable Enhanced perspective **before** entering freecam. v05’s final Enhanced shots use the current F5 53° tilt; N64 keeps its native 44.69° tilt. Keep the recorded yaw path and footage framing stable. For Harvester’s interior field, translate the complete set (units, structures, terrain, Tiberium/other overlays and ground tiles) together; moving only units and buildings removes the harvestable resource from the shot. Validate actual `harvesting=1` before full capture.

## Current v05 delivery reference

The 19 October v05 technical revision contains a 40-second Mammoth and 29-second Harvester at 2560×1440/60 fps. Production scripts, frozen runtime/input manifests, private missions, actual camera-side references and full QC evidence are kept outside the repository in the vehicle-showcase output's `v05-production/` folder; final film/QC folders are its siblings `mammoth-v05/` and `harvester-v05/`, and the versioned films are delivered beside them. Technical checks pass; artistic acceptance of the new films remains with the user. This filming revision changes the recipe and capture controls, not the game runtime or Dock installation.

## Current v06 staging and delivery reference

The 19 October v06 revision applies the straight-exit, arriving-enemy, small-base and foliage requirements to the 40-second Mammoth and 29-second Harvester films. Both remain 2560×1440 at native 60 fps, with amber swipes, current accepted F5 lighting, four real Enhanced exposure samples and the existing different approved music selections. Harvester keeps its mining/unloading exception.

Native rollout QA compares 119 non-limbo samples against the pre-change engine: normal launches match exactly. The explicit private filming rail has a maximum cross-track error of 0.00204 cells and a constant moving hull heading of 135/256 turns. This is the measured Mammoth/WEAP reference, not permission to assume every future vehicle has the same door clearance. All six enemies are created outside the actual filmed image and move across native cells under Attack missions aimed at the hero. Validate whole-model/shadow clearance, not just an anchor; the saved native counterfactual birth-frame comparison shows no in-frame enemy body or shadow. Diffuse illumination from an off-screen headlamp may reach the frame naturally before its vehicle arrives. GDI incoming fire begins after the measured 2.5-second follow. Both Harvester sets still show native mining and refinery lift/unloading.

The small sets use existing native buildings, perimeter wall sections, open travel lanes and theater-specific trees/cacti. Match set locations across mode comparisons. Freeze private INI/BIN, capture scripts, flags, camera/clock traces and asset/runtime hashes with the film. v06 integrates source samples to lossless exposures and records their hashes before removing new temporary PNGs; previous film sources and deliveries remain intact.

Production and QA: the vehicle-showcase output's `v06-production/` folder, outside the repository. Final media/QC: its siblings `mammoth-v06/` and `harvester-v06/`, with the versioned films delivered beside them. The opt-in filming controls were built from canonical main and delivered to the Dock installation, with refreshed TD and separate RA executables; hidden installed-runtime HUD, fog and RA selection/movement checks passed. Ordinary launches retain their original simulation. Technical verification does not imply artistic acceptance of the new films.

## Base layout, closer framing and terrain — 19 October v07 direction

The user's v06 screenshots reject wall T junctions and isolated wall segments. Use a coherent perimeter with deliberate entrances aligned to roads. Remove inherited wall overlays before placing the new perimeter. Validate its native cell graph: no degree-three/four junctions, no isolated pieces, one connected boundary path, with endpoints only at the intended entrance. Check the native N64 and Enhanced views too; a cell-graph check alone does not establish that a junction looks right.

Every supporting structure, including the Repair Bay and Power Plant, must fit inside the perimeter. Measure complete native building/apron footprints from current source; guessed 2×2 bounds missed several larger aprons. Keep all footprints separate and inside the boundary, and preserve the straight factory exit, travel lanes and refinery approach.

Frame all vehicle sections **15% closer than v06**. Apply a consistent scale change: Classic/N64 crop 1.472 instead of 1.28; Enhanced requests a lens narrowed by tan(old_fov/2)/1.15. The native F5 lens rounds to whole degrees: the v07 request of 35.1247° becomes 35°, approximately 15.4% closer than the former 40° lens. Record the actual saved value rather than claiming exact sub-degree precision. Preserve the approved camera side, slow orbit, eased pullback and motion blur.

Author a few coherent native road routes connecting entrances and working areas. Give the terrain visible relief, with raised terrain and cliffs beyond the usable base ground, and coherent road routes through deliberate entrances. Cliff art must carry matching non-flat elevation; painted cliff tiles on a flat heightmap fail this rule. Private showcase maps must include their own complete 65×65 `.HGT`, passed through `--reskin`, and the native log must confirm it was applied. Without that file the renderer deliberately flattens the borrowed pack. Verify actual height differences and native views, keep building aprons level, and keep ramps/passability compatible with unit routes. Use theater-correct cliff and road templates.

**No fence or wall may run over a cliff face or hang across an elevation break.** Design the perimeter and terrain together. Keep the entire perimeter on level, buildable ground, with a clear cell between it and cliff art; place raised terrain outside it. Validate every wall cell against the native cliff tiles and all four heightmap corners, and inspect the actual orbit views. A connected cell graph does not establish safe terrain placement. Keep complete building aprons level too.

Preserve the existing Amber-only transitions, 1440p60 format, per-film approved music, native faction materials, backgrounds and foliage. Capture and deliver the revised films as new versions, retaining prior exports.

## Verified v07 scene revision

The 19 October v07 films retain 1440p60, the 40-second Mammoth and 29-second Harvester stories, real Enhanced shutter sampling and different persisted approved tracks. Every initial perimeter is one connected path with a deliberate road entrance; all full native/apron footprints fit inside. Road cell networks are connected. Wall cells and their four height corners are flat, with at least one clear cell to native cliff art. Every cliff cell has real corner-height relief, and native logs confirm the private HGT was loaded. GDI comparison INI/BIN/HGT files match across all three platforms. Combat may damage walls after the initial set is established.

Mammoth's six attackers are created outside the actual closer camera view, then approach and fire under native AI. GDI spawn-frame paired renders show zero changed pixels; Nod has a small diffuse headlight spill at the bottom edge, with no vehicle body or shadow in frame. Full decode, frame cadence, source camera phases, native behavior, black ends, audio peak and Desktop copy hashes pass. Production evidence is `output/vehicle-showcase/v07-production/` in the filming workspace; delivered files are versioned v07 in the existing Desktop film folder. The user responded “Better” to Harvester v07, then requested the suspension improvement below.

## Harvester suspension — 19 October v08 amendment

The Enhanced Harvester must have visible weight while driving: a sprung chassis above grounded running gear, with a damped response to road input, acceleration/braking and turns. It settles when stopped; avoid an idle jiggle. Keep the approved geometry, faction materials, claw/ram motion and native refinery dock rig. This is runtime animation, not a post-production camera effect or an asset redesign. Classic and N64 retain their native animation.

The runtime implementation takes contact points and the body pivot from the installed HARV wheel records, not an older model's guessed wheelbase. The render-only spring advances on native tick edges and interpolates at each real exposure sample; static hull, collector and lights share its body transform, while wheel/tyre parts keep their contact pose. The normal/material passes use the same posed geometry. Limit compression and reset history for limbo/teleports. The refinery dock clip takes over normally.

Native paired on/off renders show body movement above the wheels, continuous subframe poses and zero collector ram gap. The stopped native preview settles below 0.0001 cells/radians. An installed-runtime N64 frame matches v07 pixel-for-pixel. Current-main TD and separate RA app aliases are refreshed together, with hidden installed-runtime HUD, shroud/superweapon and RA selection/movement checks passing. The mesh/material files remain unchanged. New Enhanced Harvester filming is tracked separately under `output/vehicle-showcase/v08-production/`; preserve the accepted v07 film while that technical rerender completes.

## Factory movement and latest-profile amendment — 19 October v10

A factory exit rail must end at a native cell centre before normal path driving. The prior filming rail stopped at subcell (28.6875,29.0); the next order recentered it to the stock path, causing a 0.574-cell native jump. The opt-in straight rail now runs from the unchanged doorway spawn to centre (28.5,29.5), 32 evenly spaced records with matching heading 137. Actual per-tick measurements around the seven-second order show maximum 0.043-cell command-window movement; maximum throughout rollout is 0.065 cells. Normal launches without the filming flag match the prior engine's native positions exactly. Never conceal a native handoff error with interpolation.

Freeze the user's latest saved F5 CFG and compare every render dial against compiled defaults before capturing. The 21:34 profile supplies the current grass saturation, terrain brightness, shadows, contact light, fog, structures and vehicle brightness; preserve machine resolution/display preferences and hide stats in films. The latest bloom instruction explicitly forbids reducing overall bloom. Keep overall intensity at the user’s 0.92857, with threshold, radius, passes and clamp unchanged. The new explosion_bloom dial is 0.30 and affects only the bright-pass halo beneath a depth-tested mask of luminous particles classified at birth as explosion emitters. Smoke, muzzle flashes, headlights, materials, the source particle art and scene exposure retain their settings. Lingering explosion emitters keep their classification after the parent anim is gone. A gain of 1 bypasses isolation and retains the previous picture. These films use the current Enhanced particle effects path; a change solely to the legacy experimental Unity pass has no effect on them.

Recordings must use current canonical-main renderer/engine and current matching model, specular, grass and terrain files. Check actual file hashes, including the Dock installation: an up-to-date executable alongside old models is not current delivery. Refresh the required current-main assets and both TD/RA executables without altering the launcher or user profiles. Harvester retains the sprung chassis, outside turn lean and normal refinery rig. v10 replaces both films with current inputs; the undelivered partial v09 overall-bloom attempt is superseded and its logs/hashes/selected QA samples are retained. v07 delivered exports remain preserved. Production/QC is tracked in the filming workspace at output/vehicle-showcase/v10-production; final delivery is recorded only after capture and media checks.

The particle-only scope check uses the actually deployed Dock renderer. Matched frames with scene highlights and a native cannon muzzle are pixel-identical at explosion_bloom 1 and 0.30; an actual explosion is also pixel-identical when bloom is disabled. Ordinary bloom still changes the control scene when its own switch is changed. Three same-tick combat comparisons show reduced explosion halos while the full latest user profile retains its overall bloom value. Hidden TD HUD/fog/superweapon and separate RA input checks pass; Harvester’s paired sprung-body check retains its grounded running gear and zero collector ram gap.

For long shutter captures, use lossless PNG transport with bounded integration backlog. The private transport can wait after GPU readback while the reducer writes already captured exposures; waiting must never advance an engine tick, particle/RNG state, camera or input event. Check native clock/audio ratios after capture. Preserve the exact source/exposure hashes and final decode checks.


## Film storage lifecycle — 19 October 2026

The retention rule replaces historical instructions to retain every raw
frame/version. Keep delivered MP4s, art/music, code, native scenes, settings,
hash manifests, receipts and QA contact sheets; delete superseded raw PNGs.
Before pruning, resolve actual symlinks and script imports. Materialize every
active input in the current production, verify copies, then register only those
current frame roots in `storage-retention.json`. Do not traverse symlinks.

`tools/clean-showcase.py` removes frame/screenshot/exposure PNGs only inside
explicitly retired version folders. `--retire VERSION --apply` requires a
hash-verified Desktop delivery receipt and refuses a current dependency.
Delivery must invoke it immediately after its export copy hash passes. The
scheduled cleanup is a fallback for interrupted delivery cleanup, not a reason
to retain delivered frames. Current v12 has local fixture/helper/art/music,
old movement-control baseline, scene metadata and reused Classic/N64 frames;
it no longer imports frame sequences from v04-v11 or old subject versions.
The fully verified Harvester v10 export was visually reviewed and delivered;
its PNGs were then retired. Older metadata provenance can retain historical
paths without implying that those frame sequences still exist.

Completed disposable session temp folders use `.cnc3d-cleanup.json` receipts.
`tools/clean-session-temp.py` requires 72 idle hours, no live cwd/open-file/command
reference, and no Git checkout or authoring/project files. Release staging also
requires a published receipt. Unrecognized and active temp paths are kept.
Both helpers are included in `tools/clean-builds.sh`; daily follow-up runs the
same narrow helpers without invoking broad worktree cleanup.

## Muzzle adjustment — 19 October 2026

The design review rejected the first scoped reduction (.30 source/.15 light/.08 halo) as too
faint. The brighter middle setting is .60 source, .35 muzzle-only light spill,
.25 muzzle-only bloom; explosion-particle bloom remains .30. Ordinary scene
bloom stays at the user's .92857 and the full saved rendering profile remains
unchanged. Only actual GUNFIRE/MINIGUN/SAMFIRE particle ancestry and corresponding
local muzzle lights receive the additional muzzle controls; smoke, flame/chem,
other emitters and ordinary highlights retain their previous treatment.
Matched actual native-tick/camera shots were inspected; the stronger core is
visible without the former broad blown-out halo. Final Mammoth recapture and
installed-runtime verification are recorded separately from these QA previews.


### Verified final delivery and cleanup — 19 October 2026, 22:44

Mammoth v12: 2560×1440, 60 fps, 2400 frames / 40 s, SHA256
`7f814004eb54849f6549448e967bf12b1200651e76305670aa04bbd0afdc625d`.
Harvester v10: 2560×1440, 60 fps, 1740 frames / 29 s, SHA256
`e3871f59834d4819a4ece783b7ead74b273968a0c092197ea146f17acb58270b`.
Both exports are hash-verified in Desktop/OpenCNC3D Vehicle Films. Actual decoded
contact sheets and matched muzzle comparisons were inspected. Full decode,
constant native cadence, source yaw/shutter, black ends, audio, current F5
profile, correct native drive handoff, initial wall/road/elevation constraints
and offscreen births pass. Actual latest installed output is pixel-identical
to retained Classic/N64 and Harvester controls under matched draw history;
72 native/art/terrain inputs are unchanged. Both TD/RA installed aliases match
current-main builds, with hidden HUD/fog and separate RA input checks passing.

Historical superseded frame deletion reclaimed 428.05 decimal GB (398.65 GiB).
All 32 pre-existing final movie hashes remain unchanged. After the new Mammoth
export was copied and verified, delivery immediately deleted another 14.87 GiB
of its temporary raw/exposure/per-frame QA images. No raw film sequences or
active frame dependencies remain. Final media, contact sheets, scene/settings
files, recipes, scripts, immutable inputs, masks/art/music and receipts remain.
Future versions must capture fresh raw frames; do not assume a delivered
version still contains reusable sequences. Materialize needed inputs locally
before retiring a still-live source. Daily narrow cleanup runs at 09:00, and
completed disposable temp/staging folders require 72 idle hours and activity /
authoring / Git safety guards. The three old Oct15 temp staging paths were
already absent and are not included in our reclaimed-byte total.


## Bib freshness correction — 19 October 2026

The design review found pink/purple trails on the Enhanced building aprons and asked for the
approved RA3D treatment. The Dock install had BIBENH3 while the current canonical
baker and the separate latest-build folder had BIBENH4. Version 3 omits the ground
calibration and silently uses the older multiply fallback, which tints roads pink.
This is a stale delivery artifact, not a need to repaint the approved gravel.

Fresh source bake SHA256:
`632e9039774b6b287b83900aa4caa29260e68bea46007a70ff0978c04f634a4f`.
It is byte-identical to the latest-build pack, using the approved g3s art shared
with RA3D. All six apron RGBA payloads, footprints and colour gains match the old
pack exactly; only the missing calibration records and pack format are restored.
Matched actual game views show the road strips becoming neutral dirt/gravel.
The temperate probe's pink fraction falls from 32093/1033769 (3.10 percent) to
2872/1039430 (0.28 percent). Bare ground is pixel-identical; pixels away from bibs
are unchanged. Desert calibration was also checked. F5 values stay unchanged.
Evidence: filming workspace output/bib-fix-2026-10-19/review/.

A pair of identical captures can share a stale input. For future films, validate
asset freshness against a fresh canonical-source bake as well as matching capture
hashes. In particular, require a calibrated version-4 bib pack rather than merely
matching an older installed copy. Both Mac game/app builds now bake and stage this
required asset even in code-only mode. App staging rejects a pre-v4 pack before
replacing executables. Publication is atomic. The staging regression proves stale
pack rejection and delivery of a fresh bake to the TD/RA install. Native builds
also freeze their build-header identity so parallel game/app builds cannot mix
version stamps between translation units.

Both v13 films recapture all eight native sections, using local non-frame inputs
and the current installed assets; no retired sequences are restored. Preserve
camera/zoom, eased follow, actual suspension/aiming, amber wipes, persisted distinct
approved music and the .60/.35/.25 muzzle and .30 explosion bloom controls. Overall
bloom stays .92857. Required native scene/height/perimeter/action checks and actual
runtime asset hashes pass. Delivery status and final hashes are recorded after
media verification; delivery immediately retires raw sequences and per-frame QA.


### Verified bib-corrected final delivery — 19 October 2026

Mammoth v13 (40 s, 1440p60), SHA256
`0cade1ac213a880e8ba902deb85306d9a0989420f076940e962448ca7124eb4e`;
Harvester v13 (29 s, 1440p60), SHA256
`65913577c8eb8f65acf468fbf737f50d1d59e2f76cb3dc2164a5653850e5cc58`.
Both are hash-verified in Desktop/OpenCNC3D Vehicle Films. Actual decoded contacts
were inspected and show the corrected neutral dirt/gravel bibs. Full decode,
native cadence/yaw/shutter, black fades, audio and source action checks pass.
All eight sections were captured afresh; only bibenh.pack changed among 72
native/art/terrain inputs, and all live inputs still matched their freeze hashes
at capture end. The Dock and separate RA executable aliases are refreshed and
hidden runtime checks pass. Existing running games must relaunch to load the pack.

Delivery immediately removed 24.69 GiB of native/exposure source PNGs plus the
two exports' redundant individual QA frame dumps. No raw sequences or active frame
dependencies remain. Prior delivered exports are preserved. Contact sheets and
source/scene/settings/input evidence remain for review and reproducibility.


## Vehicle showcase review and approved collection — 26 October 2026

Vehicle showcases are the current scope; structure showcases will be made later.
The design review explicitly approved Harvester v13 (SHA256
`65913577c8eb8f65acf468fbf737f50d1d59e2f76cb3dc2164a5653850e5cc58`).
Its unchanged, hash-verified movie is in Desktop/Approved Showcase Videos/Dawn3D.
The matching RA3D subfolder is ready for future approved vehicle films. Add only
the exact approved version; do not infer acceptance of historical structure
films, trailer source footage or a technically verified Mammoth export.

Mammoth v14 is a new 40-second 2560x1440/native60fps capture and remains pending
design-review artistic review. SHA256 `056579505acbdedb14c76a43f9a01f0e0c486fd7947cd28b223c72d48ebb1bc6`. It retains real GDI
War Factory rollouts in DOS, N64 and Enhanced, chimney/right-side starts, amber
left-to-right wipes, eased follow, native multi-target cannon/Tusk fire and the
Nod Desert finale. The requested base revision adds two GDI guard towers and two
Nod gun turrets inside square wall returns at the entrances. Native campaign wall
placement references are recorded. Each boundary remains one connected open path,
with no T junctions or isolated walls; full aprons fit inside the perimeter, roads
use the deliberate gate, and walls stay level and clear of raised cliffs.

Defenses change target acquisition: the GDI attackers now spawn offscreen only
after the 2.5-second drive follow, then enter through the clear lane. Nod attackers
use an unobstructed exterior route; its combat framing eases toward the approach
so all three are visible. Verify actual offscreen model/shadow clearance as well
as anchors. Matched birth frames show only native headlamp spill at the edge.
Keep the saved random music selection for this technical rerender.

The batch freezes the current installed renderer/TD engine and verifies current
model, paired materials and terrain hashes. A concurrent main renderer update
was checked separately: matched native frames in all four modes are pixel-identical
on macOS. This does not establish Windows verification. Fresh compiled F5 values
are used with the previously requested muzzle-only .60/.35/.25 and explosion .30
controls; global scene bloom is unchanged. Native60fps, four Enhanced shutter
samples, camera phases/yaw, black ends, complete decode, stereo audio and -1.8dBFS
encoded true peak pass. Native scenes, settings, exact scripts, hashes, preserved
runtimes, comparison evidence and QA contacts live in output/vehicle-showcase/
v14-production and mammoth-v14. The hash-verified review is delivered to the
existing Desktop/OpenCNC3D Vehicle Films folder; retire its disposable frames
immediately after delivery, keeping all final media and unique authoring work.


### Colour audit correction — 26 October 2026

The user reports Mammoth v14 colours as overblown, especially Desert. The current
installed renderer/assets are verified. A fresh hidden native Dawn3D comparison
with the actual current profile reproduces the bright orange Desert and strong
light halos before video export. The shared tone/desert colour values match the
current Dawn3D and RA3D profiles. V14 nevertheless carried historical muzzle-only
overrides (.60/.35/.25) instead of current .30/.15/.08. This is a recording-profile
mismatch: future recordings must use the actual current profile/defaults and must
not silently reapply those historical constants. Explicit user tuning supersedes
these old capture notes. Removing the overrides alone does not resolve the
overall Desert appearance in matched native frames. Do not claim it does.

RA3D checked separately: a hidden current-engine Temperate mission was rendered
and inspected; the Desert-specific orange appearance does not reproduce there.
There is no equivalent native RA Desert theater in that check. Verification is
macOS native; no Windows native result is inferred. The audit changes no game
settings or runtime, and v14 remains pending design-review review. Exact current profiles,
scripts, native before/after images and observations are retained under
output/vehicle-showcase/colour-audit-20261026.


## Actual Dock target and per-theater CFG correction — 26 October 2026

The user's OpenCNC3D Dock icon currently resolves from com.apple.dock.plist to
the Dock-resolved `playable/OpenCNC3D.app`. V14 used the
separate CNC3D/playable CFG. Its binaries could match while its personal lighting
settings did not. The previous colour audit's claim that v14 reproduced this
user's Dock appearance is superseded and incorrect; its data is retained and
marked SUPERSEDED_WRONG_INSTALLATION_PROFILE.

Before every film batch, resolve the actual Dock shortcut, freeze that runtime,
assets and complete saved CFG, then verify the engine's effective live values.
Do not infer the shortcut's target from an old path convention or a matching
binary hash. This filming correction does not change canonical-main runtime
build/delivery authority and does not authorize changing the Dock or game settings.
The actual CFG contains `lighting.temperate.*` and `lighting.desert.*` records:
compare the active theater layer as well as top-level values. Temperate sun
0.66964 / light0.60714 / effects exposure -0.85714 and Desert sun0.64286 /
light0.21429 / effects brightness0.625 are verified in v15. No historical muzzle
overrides or added export colour grade are applied. Exact source CFG SHA256:
`18937f5e865622de80e84bdb84211b013a0109833b3e8e056f3560d8931517b6`.

Mammoth v15 freshly recaptures all four native sections at1440p60/40seconds,
retaining the revised defenses, square gate returns, camera path, straight native
rollouts, multi-target cannon/rocket fight, amber wipes, fades and saved random
music. Native per-theater CFG, scene, movement/arrival, camera/shutter/cadence,
full decode and audio checks pass. Actual decoded views were reviewed. SHA256:
`1821094ff0f434ed65732422ab04296092d1a3efbfcf7325b06f645f6e0483de`. Hash-verified Desktop delivery is complete; user
artistic acceptance remains pending. Harvester v13 stays explicitly approved and
unchanged in the Dawn3D approved folder. V14 is superseded for colour mismatch.

RA3D was checked separately through the actual Dock app's module/harness route: its
original SCG05EA mission script completes with zero failures and native saved-file
inheritance is logged. A whole-harness return-menu comparison fails after the probe
changes Classic to Enhanced; do not present that harness as a full pass. Evidence
and this limitation are retained in v15-production/review/actual-dock-ra-check.
No Windows native result is inferred. Keep source profiles, private native scenes,
settings/scripts, frozen runtimes, input hashes, final media and useful QA contacts;
retire disposable v15 frames immediately after verified delivery.


## Vehicle action and latest lighting — 27 October 2026, revision 4

Standing design direction: every technical rerender must re-resolve the actual Dock
installation and snapshot its newest executable, assets and saved lighting CFG.
Retain the reviewed location draw and approved random track selection for revisions.
Do not restore an earlier lighting profile or override exposure, bloom, sun or colours.

Suppress only the Construction Complete announcement in private film capture,
including visual-mode transitions; preserve weapons, impacts and other game audio.
The original mission director remains disabled. Explicitly allow the participating
native base defenses: Obelisks must have real power, acquire enemies and fire;
gun turrets must turn their actual barrels toward targets and fire. An empty defense
whitelist fails action review. Do not fake firing through effects or static poses.

Humvee: stage infantry as the GDI Enhanced encounter targets. In both Humvee and APC
ending encounters, enemy Humvees must use the existing native muzzle flashes and spent
casing effects. Check their positions on the posed gun, not just point-light glows.
APC: a small friendly infantry squad boards the newly produced vehicle after its
straight War Factory rollout and before it drives away in the Enhanced opening.
Use actual native boarding and cargo evidence; allow time for the transport doors
and loading queue. The Classic/N64 views may establish this same waiting squad.
Do not fake hidden passengers.
MCV: peaceful production, driving and deployment. No designed attackers or incoming
combat; keep the camera centered on the actual MCV and the Construction Yard it
becomes. Preserve enough time to see native deployment complete.

Verify native actor state, defense target/weapon events, boarding count and final
visual footage before delivery. Record Dawn3D and RA3D bug results separately.
Keep approved Mammoth and Harvester unchanged. New films remain review candidates
until the accepts them. Hash-deliver and retire disposable raw frames after
each completed film under the existing guarded retention policy.

Verified action-check lessons: let an armed vehicle reach weapon range before
stopping its approach; stopping behind its defenses can leave all combat to the
towers. Count hero weapon events by native heap ID and player house, separately
from identical enemy vehicle types. Use house-qualified script selectors so a
surviving enemy Humvee cannot be mistaken for the hero after the finale. Validate
purpose-specific native action in the short preflight before full capture.


27 October final review additions: MCV v04 is explicitly approved
(SHA256 e55b2044011221fc2ed858673e8d970aa8eb8f507f25147198f533353340dc11),
as is Mobile Rocket Launcher / MLRS v04
(SHA256 a4c369d0bd7a21a03300209bc54d7abccb7c239d8d859101bf54784ccc5210a6).
Their hash-verified copies join Mammoth v15 and Harvester v13 in
Desktop/Approved Showcase Videos/Dawn3D. Other revised vehicles remain pending.

APC is armed: native infantry boarding does not exempt its opening from the
standing 2.5-second driving follow and subsequent defensive encounter. Allow
native loading time rather than skipping combat to fit the schedule; the current
three-soldier production uses a52-second film. Check the owned hero's actual
weapon event separately from base and enemy weapon events.

Restore filming camera range after every temporary actor-selection/boarding
interaction, including N64. A boarding command's temporary4200-lepton selection
range must not become the shot's persistent camera distance. APC v05 exposed
the original mission map edge because it omitted that reset; v06 restores the
intended N64 range. Check every cropped N64 source frame for black map corners,
then inspect decoded final views. Keep raw inputs protected until the final
visual review and hash-verified Desktop delivery are both complete. Native
checks do not replace this visual review. Preserve superseded final videos and
their receipts as history; retire only their declared disposable frames.


## Full location rotation and Nod ground batch — 27 October 2026

The design review approved Humvee v06, Medium Tank v05 and APC v07 with the latest Dock CFG, then required all twenty original reviewed locations. Approved copies and SHA-256 receipts are in Desktop/Approved Showcase Videos/Dawn3D and output/vehicle-showcase/gdi-batch-v05/approval-receipt.json; older approved copies remain intact.

Location policy v2 carries history across batches, exhausts all ten bases per faction, retains native theaters and keeps technical revisions on their saved draw. Two selector regressions verify cross-batch exhaustion and no immediate repeat at refill. The seven next films are Buggy, Recon Bike, Light Tank, Artillery, Flame Tank, Stealth Tank and SSM Launcher. Their persisted draws in output/vehicle-showcase/nod-batch-v01/location-selection.json use seven distinct previously unused Nod opening bases and seven distinct previously unused GDI endings. New films remain unapproved review candidates until the accepts each version.

Use native Nod C17 delivery, not a substituted War Factory. Measure each selected Airstrip arrival and start the comparison three seconds before actual vehicle release; confirm the primary Airstrip in bases with multiple production buildings. Run full native idle and encounter preflights for every exact scene. Original Special/Neutral scenery may need capture-only mutual alliances to prevent it becoming an unintended hero target; preserve original ownership, geometry and normal installed gameplay. Invalid native spawn cells and failed preflights are blockers, never evidence of verified isolation. Retain their authoring/logs while correcting paths.
