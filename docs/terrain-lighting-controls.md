# Enhanced terrain controls

## Latest saved lighting - 27 October 2026, 20:36

The newest Dock save supersedes the 14:49 preset below. Its SHA256 is
`48c5ef0003befc6d9e9297209b5bf4b39fc53f78ccefa7a7ca01e24d5375a1bf`.
The v04 includes import all884 named theater values and528 shared visual
candidates, preserving local display, input, audio and graphics-mode choices.
The saved Temperate and Desert exposure, contrast, clouds and tree fill are
retained; Desert also retains the revised second-sun energy and infantry light.
The active saved theater is not promoted to a global default.

No panel layout, labels, artwork or controls changed. The running 14:49 release
candidate is superseded; its results cannot certify this newer preset. Both games
require fresh default comparisons and Dock verification before delivery, and the
release candidate must be rebuilt from the newly integrated private main.
Public publication still requires final approval of the concrete build/changelog;
public source and refs remain manual USER actions only.


## Final saved lighting for the v0.9.2 candidate - 27 October 2026, 14:49

The newest actual Dock F5 save is now the default authority for both games, SHA256
`8414eb87526c9121eee05cdf4ad736cd5e90ae57176f455675e9a900ec1bf8f1`.
The dated v03 includes import all884 named values across four theaters and the
saved shared visual rows. They supersede the earlier27October layers by key.
The user's saved category checkbox states are now defaults too: Structures is
on at1.232143x in Temperate and1.526786x in Desert; Desert Vehicles is on at0.75x.
Other categories/theaters retain their exact saved values. This explicit latest
save supersedes the feature's original all-off initial defaults. Display, input,
audio, graphics-mode selection, saves and unrelated settings remain local.

No F5 label, artwork, fitting, layout or hit-testing changes in this tuning pass.
The existing348px panel and20character/120px slider label remain authoritative.
Both games' CPU gates compare all884 saved authority values and keep category
neutrality, independence and save/load checks separate from tuned defaults.
The private v0.9.2 candidate is built and gated through the normal release script;
release binaries are cooked while Dock testing keeps the developer F5 controls.
Compilation, package gates, native Mac checks, native Windows visual verification,
Dock delivery and final publication approval remain separate. Native Windows
verification is not implied by a successful cross compile. Current results and
hashes belong to the final release-v092-2026-10-27 verification report.

The concrete candidate/changelog must receive final design-review approval before public
binary or website publication. Public source snapshots remain manual USER actions
only. Preparing this candidate does not authorize any public repository ref write.



## Category second-sun power and revised defaults - 27 October 2026, 14:06

The newest actual Dock F5 save supersedes the earlier 13:29 tuning as the visual
authority: SHA256
`2eb67647ccf22cf3efa2222d0fb11dff75ec5eb105d825f9a46f2fe3a7b2f8d1`.
The v02 dated includes import its shared visual values and all860 named lighting
values across four theaters, including the user's enabled second sun in Temperate
and Desert. Display, input, audio and graphics-mode selection remain local.
The older dated includes remain historical layers; v02 wins by schema key.

F5 > Lighting exposes Second Sun Power checkboxes in STRUCTURES, VEHICLES and
SOLDIERS. Each is followed by second sun power (x), a0-3 multiplier in0.01 steps.
All three checkboxes default off, meaning the category follows the global second
sun exactly. A checked box enables only its paired slider;1 preserves current
brightness,0 removes that category's secondary contribution,3 triples it. Turning
the box off retains its slider value for later, while restoring global behavior.
The slider stays visible but dim and cannot be clicked or dragged while unchecked.
The master Second sun checkbox/energy must still enable the source light. These
controls do not add a sun, change the primary sun, or create any shadow pass.

Reuse the accepted348px panel,15px rows,160px label well and existing checkbox/
slider fittings. The longest added label is20characters/120px, within the label
well. The shared enablement policy drives native hit testing and drawing. All six
keys save independently per theater and migrate from older presets as off/1.
Structures/walls and vehicles/aircraft use their existing pixel classes; authored
PBR vehicles/buildings and3D infantry receive the multiplier in their own direct
light uniforms. Trees, terrain, sky fill, emissive parts and existing primary-light
shares retain their previous behavior. Infantry follows the existing3D Soldiers
group; this does not invent directional lighting for the original flat sprite art.

CPU tests cover both games'860 authority values, four profiles, independent
checkbox/slider persistence, disabled neutrality, actual row enablement, legacy
migration and label fitting. Hidden Mac checks separately cover category brightness
and other-category cores, exact disabled/1x images, primary shadows/depth/normals/
fog, and actual SDL checkbox and slider clicks with saved-value assertions.
Unallocated optional targets are recorded as absent, never claimed as exact data.
F5 on/off screenshots are inspected directly; this is a schema/control extension
using existing artwork, not a new artistic variation or approval.

The dated second-sun-power-2026-10-27 report records current-main build identity,
Windows compilation, native Mac results and both installed Dock folders separately.
Native Windows graphics are not verified for this change. Delivery preserves the
Dock wrappers, local preferences, saves and unrelated assets and merges only the
requested visual CFG rows. Private builds only; the release remains paused.


Private Dock delivery completed as v0.9.1+b4ab12e, runtime source
`b4ab12e5839cf14c200ca791f5c35e4ec4a81bfd`. Both universal Mac frontends,
standalone renderer and launcher aliases are refreshed in canonical and actual
Dock play folders. Both games pass two hidden menu/mission/return rounds in each
installation. Installed category tests show exact prior-build default images,
exact disabled/1x and master-off images, separate brightness response with zero
changed pixels in the other categories' cores, unchanged available shadow/depth/
normals/fog targets, and actual checkbox/disabled-slider behavior. The separate
terrain shadow target is not allocated in these views, so no exact-data claim is
made for that absent target. The native F5 on/off screenshots were inspected.

At1280x720 the tested checkbox/slider click centers are structures212/227,
vehicles338/353 (bottom scroll clamp), and infantry287/302; x180 toggles a box
and x280 moves an enabled slider. Disabled clicks retain1, enabled clicks save2.79.
Shared layout governs other resolutions; these coordinates are test evidence,
not fixed product assumptions. The preservation audit covers11077 canonical and
11182 actual-Dock protected files with zero changed and zero unexpected additions.
All unrelated CFG rows are preserved and all860 named values match in each of
four installed CFGs. The same-commit Windows update ZIP is on Desktop; native
Windows graphics remain untested. Exact hashes, private CFG backups and receipts
are in the local the engineering reports report. No public
publication occurred and the release task remains paused.

## Second sun and saved visual defaults - 27 October 2026

The user's 13:29 Dock F5 save is the current shared visual and four-theater
lighting authority, SHA256
`18af297657db3c5cdb1b20ae81a98ee6a2813a0cfd7521a966332821b74491cb`.
The dated Enhanced include imports visual keys by schema name; the dated lighting
include imports all 848 saved values across Temperate, Winter, Snow and Desert.
Display, mode selection, input and audio preferences stay local. Personal CFGs
are never committed or shipped. Installed CFG delivery merges only those visual
keys and named profiles, preserving unrelated rows and player files.

F5 > Lighting > SUN AND SHADOWS now places Second Sun bearing, Second sun and
Second sun energy immediately below sun bearing. Reuse the existing 348px panel,
15px rows, 160px label well and native font. The longest new label is 18 characters
(108px); no new artwork or layout variation is introduced. Bearing spans 0-360
degrees. Energy spans 0-3 in 0.01 steps and is independent of primary sun energy.
The second sun shares the primary height, starts disabled at bearing225/energy1,
and adds diffuse/specular lighting only in Enhanced unified daylight. Its three
keys save independently in each theater, including through old-preset migration,
Save/Load and Defaults. No second shadow pass, map, contact ray or cloud shadow
is created; the existing split-light targets retain only the primary direct share
so primary shadows cannot darken the secondary contribution.

CPU gates pass separately for Dawn3D and RA3D: all848 saved authority values,
four profiles, toggle/bearing/energy roundtrip, legacy-off defaults, global fog,
Classic edits and the existing public Visuals switches. Hidden native macOS
checks pass separately on Dawn GDI1 and RA Ant1: independent energy with primary
sun/sky at zero, bearing response, energy-zero/disabled exact pixels and exact
primary/terrain/infantry shadow maps, scene depth, normals and fog coverage.
Fully hidden fog pixels remain unchanged in both games. Disabled output versus
the prior shader differs at one pixel by at most2 channel levels per game;
this is the explicit shader rounding tolerance, not an exact full-image claim.
The new F5 panel was captured and inspected with all three labels fitting.

Build, Windows compilation, actual Dock delivery and installed checks are distinct
receipts in the local second-sun-2026-10-27 report. Native Windows graphics have
not been tested for this change. The actual Dock entry remains
CNC3D-latest/playable/OpenCNC3D.app; both it and the canonical CNC3D/playable
installation receive matching current-main binaries. Preserve their wrappers,
saves, settings and unrelated assets. This is private testing delivery; the
release remains paused and no publication is authorized.


## Final v0.9.1 settings and fog corrections — 27 October 2026

The ordinary Visuals switches now commit changes to the selected Enhanced theater
profile, including edits made while Classic is active. Other theater profiles
retain their own values. Automatic theater selection at mission startup adopts
the loaded state without treating it as a user preference edit: opening a page
and leaving without changes writes no settings file. Visuals Reset restores the
current theater's compiled defaults while preserving other theater banks, input,
display and other graphics modes. Presets record `lighting.context` so a saved
named bank wins over the duplicated flat rows when loaded; default annotations
compare against the same theater. Legacy presets still migrate as before.

Final Fog of War coverage starts with the existing native-height shroud geometry
in an independent color target. It temporarily shares scene depth read-only, then
detaches it before reconstructing solid surfaces. The old geometry mask left holes
where upright units won the depth test; the solid pass fills those silhouettes.
Transparent water does not write scene depth, so clear-depth pixels retain the
native geometry coverage rather than being mapped to a far off-map position.
No flat sea level is invented. Scene depth is never owned, cleared or written. Smooth/fan/hard coverage follows the same engine visibility
field, and the five fog controls remain global. The GL2.1 shader uses the existing
inverse camera transform and a normalized 16-bit visibility texture. Clear pixels
receive no correction; hidden pixels stay black. F5 layout and artwork are unchanged.

The unit gate covers both games, all four preset contexts, exact save/load/save,
424 shared authority values, global fog isolation, five public switches and
Classic edits. Hidden native checks cover both games' public switches, five
preference scenarios (writes 1/1/0/1/0), full Reset with exact frame equality under
a fixed simulation seed, and actual F5 theater switching. Native fog probes show
256 coverage levels, black cores, unchanged depth and unchanged revealed pixels;
the formerly brighter hidden infantry now agrees with its dark surrounding ground.

Older visual regression fixtures now freeze their lighting explicitly. Approved
damage/placement reference images remain unchanged and use the exact original
calibration. Rain animation measures newly moving pixel locations rather than
subtracting two whole-screen counts; the disabled-animation negative control
still fails the original floor. Structure color scope uses the existing eight
pixel RGB-footprint tolerance in both art modes: four actual object pixels happen
to match their exposed background RGB. This is an explicit measurement tolerance
change from zero in the older-art arm, not proof that every pixel is separable by
color. Snow keeps its shipped quality caps plus a paired bright-sun on/off control.
No product lighting or artwork was changed to satisfy these fixture corrections.

The unchanged ground/sea gates catch the depth-free-water case: the defective
depth-only candidate had a black cove and only1940 strip pixels; the corrected
mask draws2474 strip pixels and75% water motion against7% in its control. Both
games retain256 coverage levels, black cores, neutral revealed pixels and exact
scene depth. Hidden infantry stays at0.0 luminance on surrounding ground at0.8.
See `HYBRID-FOG-WATER.json`; fresh-main rebuild/delivery follows these diagnostics.

Verification receipts are in `docs/verification/2026-10-27-v091-release/`.
Current runtime integration, final package verification and Dock delivery remain
pending until the final release receipt records their exact source and hashes.
These technical checks do not authorize public source publication or replace
final approval of the concrete binaries and changelog.


## Global Fog of War and promoted theater tuning —26 October2026, late evening

Current design direction: Fog of War controls are **global across theaters**. F5 →
Lighting opens with **FOG OF WAR (GLOBAL)** above the sun group. Its five rows are
smooth edges, edge falloff, unseen darkness, final darkness and banding noise.
Their `fow_*` keys never enter a named theater profile; switching the theater
selector leaves them unchanged. They save once in the ordinary shared schema.
The existing panel artwork, dimensions, tabs and dropdown fittings are retained.
Lighting's Defaults button still resets only the selected lighting profile and
leaves the global fog settings alone.

Enhanced fog can use a continuous bilinear coverage field with quarter-cell
triangles instead of the old centre fan. A native-height coverage target refined from solid-surface depth reapplies
adjustable darkness after the light/grade/shadow-detail toe; its stationary grain
only touches partially covered pixels. Fully covered pixels stay black, and
fully clear pixels receive no final fog correction. The geometry seed temporarily shares scene depth read-only and detaches it before
the solid pass samples it; the mask never clears, writes, owns or deletes scene depth. Legacy N64/DOS rendering
and engine visibility/culling remain authoritative. The controls do not reveal
units or change exploration rules. Disabling smooth edges and setting final
darkness/noise to zero retains the previous soft-shroud presentation at falloff1
and unseen darkness0.45.

The user's actual Dock Dawn save from23:27, SHA256
`18937f5e865622de80e84bdb84211b013a0109833b3e8e056f3560d8931517b6`,
is the new **Temperate and Desert lighting** authority for both games.
`game/lighting_defaults_20261026.inc` imports exactly212 visual values for each
of those two profiles. Winter/Snow retain their earlier defaults; local display,
input, graphics mode and other global preferences are excluded. Existing named
profiles still load as saved, and legacy presets still seed their four profiles
from their own values. Delivery merges only the requested two named profiles
into existing installed presets, preserving every unrelated row. No personal
CFG is committed or shipped.

The expanded compiled profile gate checks all424 authority values for both
frontends, global fog save/load and theater switching, legacy migration and
Classic-mode preservation. Hidden native checks exercise the actual dropdown,
pixel response, black core, depth preservation and clear-region neutrality.
Clean source `d5c9d3744` is delivered as private v0.9.1+d5c9d37 to both play
folders. `docs/verification/2026-10-26-global-fog/DELIVERY.json` binds the actual
Dock target and preserved files. Both games pass hidden native Mac/RTX3070 fog
checks, exact legacy-off images, unchanged depth and revealed-ground neutrality. Technical verification
does not confer artistic approval or public publication authority.


## v0.9.1 private development delivery — 26 October 2026

Final verified delivery: private **v0.9.1+d01f248**, runtime source commit
`d01f24886`. See `docs/verification/2026-10-26-v091-final/README.md` and its
hash-bound receipts. Actual launcher/menu screenshots verify Mission Disks,
Counterstrike/Aftermath and the Shift-speaker Ant list. All Ant chapters and
four private Dawn User Maps launch through their native lists. The four-tab F5
layout and theater selector are verified on Mac and native Windows; no artistic
approval is inferred from these technical checks. Both Dock play folders receive
the clean binaries and engines while preserving their launcher and personal state.
The original tree mesh class is 0.247 (outside emissive bands); RA fallback cards
reuse their exact theater-prefixed sprite coverage and painter order. Existing
Enhanced tree normals retain their original foliage class.


RA's title button and mission picker are named **Mission Disks**; Dawn retains
Special Ops. The existing accepted RA menu kit is reused. Hold Shift and click
the title backdrop's top-right speaker to open the four Ant missions. Both classic
and Enhanced layouts use the same native-art hotspot, scaled with the backdrop.
The original Spain player and Ant rules are enabled; mission 2 inherits the actual
surviving objects exported by mission 1. Direct selection of chapter 2 seeds the
original Discovery base rather than losing immediately with an empty player.
All 38 add-on maps have their own freshly generated, verified height corners.

F5 tabs are Enhanced, Weather, **Lighting**, Experimental. The four 80 px tabs
fit the existing 348 px panel. Lighting's sticky selector occupies y78–98;
rows begin at y108 on that tab and y80 elsewhere. Shared layout/hit testing
splits mixed groups by parameter, keeping geometry and motion on their existing
tabs. Temperate, Winter, Snow and Desert save independently by named keys.
Old files seed all four from their existing values. Loading a map chooses its
theater; the selector previews another profile immediately. Lighting's Defaults
resets only the selected theater. Classic mode keeps its effect switches off.

Four personal maps named Effects - Temperate/Winter/Snow/Desert are installed in
Dawn's user_maps through tools/install-effects-lab.py. Snow uses the existing
Snow renderer with the original engine's Winter rules. Their own flat 65x65
heightfields accompany clear ground; 51 production effect recipes loop on labeled
ground stations. These are private tuning maps, gated by private_developer and
excluded by existing public user_maps package filters. No map changes engine rules.

The fallback tree mesh path now writes its foliage class before the remastered
normal pass. This connects original trees to the same tree shadow fill/object fill
and foliage occlusion rules as the Enhanced trees, including cutout coverage.
Native checks and raw buffer comparisons remain separate from artistic approval.

The earlier delivery reached the canonical `CNC3D/playable` folder. The actual Dock points to
`CNC3D-latest/playable/OpenCNC3D.app`. Final delivery receipts must identify the
latter and preserve its launcher, saves, settings, current HUD/fog/art assets.
All candidates remain private; no public source or binary publication is authorized.


## Latest shared visual preset —26 October2026

The latest user-saved RA3D picture, SHA256
`b0f67027ba5d9703dec4dc05a7d451fec9df9ff004ac6f3b5fcaa37a8486b0e0`,
now defines the shared Enhanced visual defaults for Dawn3D and RA3D. The dated
`game/enhanced_defaults_20261026.inc` follows the retained22/25October layers
before mode defaults are captured. Exactly22 visual values change: clear-terrain
saturation; water alignment, bump, highlight strength, reflection bearing,
refraction, natural shading and darkness; tree gain/shadow fill; grass gain,
green/tint, root fade/alpha, AO/tip and five grass-terrain colour values.
The reflection offset is explicitly **90°** independently of sun rotation.
This replaces the older zero-default contract below; zero still gives the
unrotated reflection direction.

No shader, native height/depth, wave/shore solver, mesh, terrain pack or artwork
changes. Twelve local fields remain untouched: `show_stats`, `vsync`,
`display_mode`, `res_w`, `res_h`, `ui_scale`, `swap_buttons`,
`right_drag_scroll`, `cash_tick`, `enabled`, `dos_classic` and `smooth_anim`.
Classic/N64 saved rows and migration markers are not imported, and no personal
CFG is shipped. The full saved input is retained only as independent local
verification authority, with Enhanced rows taking precedence over live copies.

`docs/verification/2026-10-26-shared-defaults/COMPILED-DEFAULTS-RESULTS.json`
records the independent compiled comparison: all519 visual keys match the
approved save within its five-decimal precision, exactly22 fields change,
all remaining default bytes and inactive mode rows stay exact, and old/new
complete loaded states are byte-identical with that same frozen preset.
Current-main native and Dock verification remain pending and require a final
receipt in the same directory. No gate threshold or historical fixture changes;
the earlier dated material below remains provenance.

## Shared25October visual preset

The approved shared save at SHA256
`1e5ddb3c9dbd99d743598f309c71abab80e5b19a5a7740593e2e443e2caf448c`
supersedes the22October visual tuning for both games. A dated84-assignment layer,
`game/enhanced_defaults_20261025.inc`, follows the retained22October base before
mode defaults are captured. Only the measured visual differences are imported;
the user's personal CFG, display resolution/mode, input, debug and selected
graphics mode are not imported into public defaults.

The layer includes12 water values from that save, together with its lighting,
terrain, effects/infantry brightness and other visual grades. It changes no shader,
geometry, wave/shore solver, source artwork or depth ownership. Reflection rotation
continues to inherit zero when the older preset omits it. The original base and
dated historical measurements below remain available as provenance.

Independent compiled TD/RA checks pass all visual-key comparisons to the exact
approved saved input, within its five-decimal serialization precision. They prove
exact84-field coverage, preservation of every other default-state byte/mode row,
and byte-identical complete loaded states before/after this amendment with the
approved full preset. Public fresh-default versus approved-preset native scenes
and final release delivery remain separate pending checks. No gate threshold or
historical fixture is changed by this defaults amendment.

## Independent water reflection bearing —25 October2026

The Water group exposes `water_reflection_yaw` as **Reflection rotation**,
from-180to180degrees clockwise on the map. It is an offset from the current sun
bearing and does not change the sun's azimuth or elevation. Its default is zero,
including when loading an older preset. The shared F5 schema saves and loads the
offset in both Dawn3D and RA3D.

Only the surface shader's specular half-vector uses the rotated direction.
`uSunDir` and its diffuse/painted-water relief remain unchanged; the independent
`uReflectionDir` preserves the light's elevation and length. Zero copies the
previous components exactly before upload. The same shader/upload applies to
TD unpainted water and TD/RA painted water. Shore shaders, the virtual bed,
absorption, native height, normals, waves and shared lighting are untouched.
This control does not reintroduce the retired mirror-scene reflection pass.

`tools/tests/test_water_reflection_rotation.py` compiles the actual state and
direction helper for exact neutral components, rotation direction, elevation,
TD/RA defaults, legacy inheritance, label fit, panel visibility and preset
roundtrip. Native validation must separately check zero/rotated/restored water,
unchanged terrain/sun and each frontend's saved preset. Current-main combined
build and verified Dock delivery remain pending.

The hidden actual-production-GLSL probe passes the unpainted TD, painted TD and
painted RA variants. It compares the old half-vector against the new zero value
and restores zero after90degrees: both comparisons have zero pixel error. The
rotation changes32,704/32,758/32,758of32,768pixels respectively, with exact alpha;
specular-disabled body pixels remain exact, and every active shared-sun uniform
keeps its original vector. Constant fixture textures isolate shader behavior;
this is separate from full-game scene and installed frontend proof. The local
`water-reflection/NATIVE-RESULTS.json` binds source, executable and SDL closure.

## Pale-slope headroom under unified daylight — 25 October 2026

Red Alert's pale-ground highlight rolloff now reads the final unified sky and
direct-sun factor. Previously that factor replaced the already bounded legacy
slope factor, bypassing the rolloff while unified daylight was enabled. The
existing knee and white-input limit now bound the unified boost before it
multiplies the painted ground. The legacy branch, darker slopes and disabled
rolloff retain their prior arithmetic. Saved lighting and colour values are
unchanged. Source-extracted CPU controls reproduce the overwritten bound and
check legacy equality, disabled-rolloff equality, retained hue and highlight
headroom; native slope captures remain a separate runtime check.

## Submerged banks and continuous Snow relief — 22 October 2026

Enhanced water keeps the existing body plane and low-bank easing. Its opaque
bed closes the terrain cut and slopes into a flat basin three cells below water
(one and a half for narrow river channels). Shared sixteenth-cell bank vertices
carry interpolated surface normals, so exposed closures no longer receive a
separate flat normal for each triangle. The short bank lip eases over0.18cells.

Above-water banks continue the actual adjacent terrain atlas, sampled through a
per-map-cell UV lookup and the nearest dry foot. Cliff banks continue that local
rock into the shallow water; ordinary banks transition to the native BOTTOM dirt
material with depth. The source material extends inward as the bank drops,
without repeating another complete cliff silhouette. The nearest dry colour is
the fallback when the atlas has no valid source coverage.

Swash runs up only onto banks within0.035cells of the water, fading out by0.09.
Raised banks instead receive a narrow cellular foam rim at their physical water
contact, using bank depth and slope. They do not receive beach run-up on top.
Swash opacity still controls this rim; disabling both swash and foam removes it.
The shared clock, Hold timing and river swash exclusion remain unchanged.

Narrow connected lakes require authored river/ford/falls/bridge terrain before
the width rule can classify them as rivers. Lakefront Clash (TD SCM06EA) has two
lakes; both now retain ocean pan. Its right lake was previously classified as a
river solely because its maximum radius was below the configured river width.

RA Enhanced WC/RC cliff materials remove blue surf and its attached thin white
fringe only near the immutable native water cut. Repair works on each complete
template and variant, with dry source RGB as the boundary condition. Matching
join-strip RGB is repaired where the source edge was repaired; all blend alpha
is preserved. Snow and Temperate banks are audited across84template/variant
groups (352icons). The original atlas and strips remain available when water FX
are off and in Classic rendering; source terrain packs are not rewritten.

Absorption uses vertical water depth: texture contrast drops continuously and
is exactly zero beyond2.7cells. The post normal pass writes the actual visible
water surface depth, so lighting and fog reconstruct the plane rather than the
deep floor. Opaque projected terrain and later object normals keep priority.
Authored waterfalls and unsupported border water retain their native floor.
Classic water, the body solver, shoreward chart, swash clock, Hold, opacity and
the exact13:51defaults remain unchanged. Installed user CFG files are preserved.

Snow relief now feathers in the shared world lattice before the authoritative
water cut. Its zero-height neighbours carry matching edge normals; its base
normal interpolation uses the native SW–NE triangles without renormalizing the
flat supports. This removes template rectangles around Snow rock feet while
preserving the source albedo and saved feature mattes. Building pads remain
flat and wet supports retain zero relief. No model, HUD or F5 control changes.

Validation targets include SCM01EA, Coastal Influence, the user's first RA
skirmish map, at native and low cameras. Isolated production-GLSL absorption,
actual bed supports, water depth/material ownership, Snow boundary curvature
and both installed game regressions provide separate technical evidence.
`gate-water-bank-material.py` renders the production bank GLSL against an
independent interpolated-normal field and two distinct local atlas cells.
`gate-water-bank-contact.py` renders all three shoreline variants across32cycle
samples: low beach, raised beach, raised water contact and disabled foam.
`gate-submerged-terrain.py` uses the optional authoritative `C3DPLN1` dump to
intersect camera rays with the per-cell surface before selecting expected wet
pixels. Bed seen beyond a map edge is not evidence of a water hole. Above-water
bank occlusion is evaluated from bed geometry. Foreground sprites and native
cliffs are confirmed by an above-water depth point on dry or partially covered native coast, or the
authoritative native relief. Selection never uses RGB; missing material, including
a zero normal buffer, and incorrect surface depth remain failures.

## Foam build-up and backwash — 22 October 2026, 13:51 save

The foam material now follows the reversible swash lip rather than the ocean's
always-forward accumulated travel. It builds a dense, granular bubble rim as the
front reaches maximum run-up. The rim broadens during early backwash; a separate
shore-anchored residue spreads towards the sea and breaks apart as the water
retreats. Both layers fade before the next approach, so their periodic reset is
invisible. The held phase still pins peak run-up for exactly the requested Hold;
zero Hold reverses immediately. Opacity scales film and both foam layers together.
The shore-only band, projected contact coverage and ordinary-ocean fade remain.
The residue begins only after the incoming lip reaches maximum run-up; it cannot
appear ahead of the water during approach. No panel rows, label geometry or artwork change.

The exact latest F5 save is `docs/tuning/opencnc3d-fx-2026-10-22-1351.cfg`, SHA256
`a5945491a3934f0ab794c1271651770a8cf0d2592de78251dafdd4376a142805`.
The canonical generator updates the shared Enhanced include for TD and RA:
Hold0sec, opacity0.48214, reach0, blend3.31518cells, legacy foam width0.67857,
water bump0.41518, specular0.53571 and refraction0.02196. Earlier snapshots and
measurements below are dated history. Display preferences, saved Classic/N64
rows and existing installation CFGs remain user-local.

The source-extracted lifecycle gate and negative controls reject constant-width,
unfading, forward-only and sliding-residue variants. An isolated hidden GPU
transect pins phase and uses black ground/water plus white foam input to measure
the production material envelope without lighting/noise confounds. Coverage grows
while the foam spreads, then drains; that growth must not be mistaken for a fade
failure. Full-game hidden GDI1/Allied1 loops check the real procedural bubbles.
Source checks, isolated probes, visible game captures and clean-main Dock delivery
are separate evidence; their results do not imply design-review artistic acceptance.


## Adjustable swash hold and opacity — 22 October 2026

F5 > Water adds **swash hold (sec)** (`water_swash_hold`, 0–10 seconds,
default 0) and **swash opacity** (`water_swash_opacity`, 0–1, default 1).
Hold inserts seconds at maximum inland run-up, keeping foam age and shoreward
chart presence at their shared peak. Approach and retreat durations retain the
saved crest-speed timing. Thus the 10:24 baseline has a roughly 10.69-second
cycle plus the selected hold. Crest speed zero still freezes the cycle.

The 13:04 user test supersedes the original quintic envelope: its nearly flat
endpoints hid 2–3 seconds of apparent holding even at zero. Quadratic run-up
and backwash now reverse promptly at zero hold; only the chosen interval parks
the crest. The cycle integrates these envelopes analytically, including the
peak interval. Its mean-normalized forward travel remains continuous across
cycle boundaries; it never multiplies elapsed time by changing instantaneous
speed. Both water and beach film use the same held clock, and retreat still
fades the aligned chart into the ordinary ocean. Opacity scales the combined
film/foam alpha after deriving its unpremultiplied colour, avoiding darkening
or changing foam colour. It affects both the exact beach mesh and incoming
plane foam, independently of legacy foam, beach-fade and ocean controls.
The swash crest starts at signed shore distance zero and travels inland. Its
shore band ends through a 0.04-cell antialias feather, removing the previous
offshore foam row while retaining foam over the projected lower shore lip.

The exact saved 10:24 CFG and its 523 supported values remain unchanged.
Existing presets inherit zero hold/full opacity until edited; ordinary
Save/Load/reset stores the two new plain keys. Source/state, hidden GPU hold
and opacity, F5 capture and clean-main Dock checks have distinct receipts.
See the UI notes for fitting and verification scope.

The current default is **unified daylight**, shared by Tiberian Dawn and Red Alert.
F5 > Enhanced > Sun and Shadows has unified daylight, sun energy, sky fill,
terrain sky fill, sun bearing/height and the existing shadow quality controls.
The latest 22 October 10:24 F5 save supplies the shared Enhanced visual defaults
for both games. The exact snapshot is
`docs/tuning/opencnc3d-fx-2026-10-22-1024.cfg` and the generated include is
`game/enhanced_defaults_20261022.inc`. Enhanced mode rows override live copies;
Classic/N64 rows and machine/display state are excluded. The compiled defaults
retain the supported saved visual/mode values in TD and RA. Earlier dated
snapshots remain preserved. The new save keeps shore reach0, blend2.53437,
flow0.25, flow rate0.02, crest speed0.19643 and beach fade width0.93036.
Shadows remove direct sunlight once; ambient sky fill survives them. Zero sun
and zero sky darken non-emissive surfaces. Dynamic lights and emissions remain
independent. Turning off sun shadows does not restore cartridge shadow plates.

F5 > Terrain has geometric rock relief and its maximum height (default 0.32
world cells, range 0–0.6), brightness and contrast. The saved physical masks
produce one bounded mesh used by colour, normals, depth, reflections, placement,
picking and the terrain sun map. The native map heightfield and building pads
remain authoritative. Missing packs or refused GPU buffers retain native terrain.
Fine-grid and geometry-memory caps bound large-map allocations.

Terrain shadow strength now controls actual terrain cast-shadow opacity.
There are independent Desert, Sand and Snow groups; Snow covers both games.
Other theaters use the generic Terrain value. Old CFG files inherit their final
saved generic value for missing new keys, regardless of key order. Explicit
values stay independent through Save/Load. Object shadows retain their setting.
Desert brightness/contrast/saturation and Snow brightness/contrast/saturation
retain their existing controls. All values use the normal saved CFG schema.

Enhanced physical cliff/boulder/shore albedos no longer contain the broad fixed
cast-shadow skirts; saved common-ground shade is neutral. The source deltas and
explicit black player-colour masks are in
`art/remaster/textures/unified_daylight_v01/`. Sand derives matching Enhanced
Desert template detail with its measured native pale palette. Native and DOS
Classic sources are unchanged. Fine source crevices and the approximate source-led
relief remain asset fidelity limits; this is not a claim of full PBR replacement
of every historical sprite and translucent effect.

Brightness/contrast affect ground before lighting. Visibility is carried by the
same smooth shroud field; actual water coverage excludes water from ground grade.
The older painted-shade correction and shade-floor/one-sun controls remain only
for the legacy daylight path. They must not be reintroduced as modern shadows.

Run `python3 tools/gate-unified-daylight.py --playable <installation> --cfg <cfg>
--output <private-directory> --native-fixtures` for hidden runtime checks.
The gate never opens/focuses a visible window and uses private settings/saves.
It covers both games, eight theater paths, opposing sun, independent sun/sky,
zero-energy material classes, physical cliff cells, F5 reachability, timing,
Native/Classic isolation and preservation of saved user CFGs. The older
`gate-terrain-controls.py` describes legacy shade-softening semantics.

See the engineering reports for the diagnosis, iterations and
runtime evidence. Deliver from current main to the existing Dock installation,
including both frontends and all required masks/surface packs; keep launchers,
saves, user tuning and unrelated approved assets intact.


## Shadow detail and Temperate surfaces — 20 October 2026

Sun and Shadows adds **shadow detail floor** (`shadow_black_limit`, 0–0.15,
default 0.035). It softens the colour grade's black clipping with a smooth toe;
it is a display response, not an extra light or a minimum brightness painted into
albedo. Sky fill scales it. Zero incoming radiance remains black, including
unexplored fog and a zero-sun/zero-sky scene. Set 0 for the previous hard clipping.
Source foliage AO settings remain separate from this display curve.

The separate **object shadow fill** (`object_shadow_fill`, 0–1, default 0.30)
raises sun self-shadowing and ambient occlusion on structures and trees. It uses
the structure class mask even when the other Structures dials are neutral. It does
not weaken the same object's cast shadow on terrain. The 3D tree material shader
uses it for canopy and bark occlusion as well.

Temperate cliffs/boulders use the same explicit mineral mask with a darker,
neutral-grey albedo grade. Fine grit and fissures remain; grass stays unchanged.
The authored RGB/height/normal cliff packs are shared with RA3D; its pack loader
also checks the parent `playable/` directory when launched from `playable/ra3d`,
so the separate frontend resolves the same OpenCNC3D cliffs. Both TD and RA
Temperate banks receive the neutral surface grade.

The normal Enhanced RA bank now shares all 42 matching TD cliff templates and
variants by exact name, variant, occupied cells and dimensions. The compiler
remaps only template IDs: colour, physical height, ground/mineral masks and the
complete external feather strips come from the same TD source. RA's former
cliff strips are removed before the shared strips are written. The legacy RGB
pack lookup alone does not provide this native-bank contract.

Desert and Sand ground blending fades out over the physical foot of raised
rock. A neighbouring clear cell cannot spread sand up a cliff face merely
because it crosses a cell boundary. Ground-level transitions retain the smooth
authored matte. The sandstone's material chroma is reduced by 35 percent while
retaining its luminance and grain; saved theater tint/RGB values remain intact.

The complete original Desert cliff footprint is restored for all 56 S-template
versions. A previous colour-seeded graph cut wrongly discarded shaded lower rock
faces. The explicit original outline now determines material and physical relief;
all original rock texels remain stone and the 3.5-texel sand feather lies outside.
Broad source illumination is divided out independently of coverage. Run
`tools/gate-desert-footprint.py --playable <installation>` to verify the saved
outline and the installed Desert/Sand ground and physical surface packs.

`python3 tools/gate-shared-cliffs.py --playable <installation>` verifies every
native TD/RA cliff cell, height tile, ground mask and external feather strip,
and rejects residual warm mineral streaks in the explicit rock interior.
Normal updates of an existing RA module now also refresh its native terrain
packs and RA infantry exports; these must not be confined to first-time module
installation. Existing non-cliff seam kind/reserved bytes remain intact.

Shore wet/dry soil is material colour, not cliff illumination. The compiler keeps
the continuous accepted shoreline source and its complete external blend strips.
The previous cliff normalization changed some cells independently and threw away
external margins; that caused the rectangular seams. Water cuts, native topology
and lighting geometry remain shared with the sun/depth passes.

Unified daylight now uses the approved authored gravel bib composition in both
games, as the RA snow path already did. The same g3s/version 4 pack is required;
this replaces TD's old substrate multiply, which hid the gravel detail and tinted
it with the ground. Strength and the authored feathered alpha control the blend.
The legacy daylight path retains its calibrated cover/multiply controls.


## Independent terrain colours

F5 > Enhanced has Grass, Desert, Sand and Snow groups. Each adds warm/cool and
green/magenta tint (−1..1, neutral 0), plus red, green and blue balance (0..2,
neutral 1). Keys are `grass_terrain_*`, `desert_terrain_*`, `sand_terrain_*` and
`snow_terrain_*`, ending in`warmth`, `tint`, `red`, `green`, `blue`. These 20 sliders
are independent of the existing 3D grass blade tint, and they use normal Save/Load.
Snow colours also cover Winter. RA Interior keeps its own source palette.

The controls grade ground and cliffs before sunlight, shadows and lights. Source
material classes exclude objects; the coast field excludes open water and fades
the grade smoothly through 0.75 cell at the inland wash in both games. This removes
the remaining brown/pale shoreline contour; only Desert had that fade previously.
The relief colour pass also retains its parent's feathered alpha blend.

All new colours default neutral. The latest RA3D CFG supplies the shared compiled
Enhanced defaults, while local display values and existing per-mode choices remain
user-local. New shadow controls keep their own compiled baseline unless present in
the CFG.

Hidden checks:
`python3 tools/gate-terrain-colours.py --playable <install> --cfg <private-cfg>
--output <private-output>` exercises every colour control, inactive biomes, exact
object isolation, F5 and all 20 saved values over eight paths. Editor comparisons
exclude the animated palette icon and use actual painted cliff fixtures.
`tools/gate-shadow-detail.py` accepts the same arguments and checks the display
toe, zero radiance, F5, saved roundtrip and native modes in both games.
See the engineering reports for verified delivery.


## Earlier Dock delivery — before the water-plane correction

Clean pushed-main build `0.8.7+9a11a66` is installed in both existing Dock
locations, including TD and separate RA executables. It includes the bank/crown
coverage repairs, paired stone normals/specular, native mineral sampling and the
latest saved Enhanced defaults. Approved shore grass-only mattes are preserved;
physical ground overlays are clipped by the actual saved coast alpha.
Eight daylight paths, native/Classic isolation, zero energy, F5, six RA HUD and
four RA first-frame cases pass hidden installed checks. All8,520delivered hashes
match, and70settings/save/bundle files retain their bytes and modes.
The earlier5,204-file dirty-source receipt is superseded and retained as dated evidence.
The current receipt and combined-source hashes are in
`docs/verification/2026-10-21-shoreward-water/` (the earlier coverage receipt remains historical).
The20October world-detail and terrain-polish receipts remain dated history.

The shared animated run-up/backwash investigation is saved in
`art/studies/shoreline_swash_v08/`. Its four actual engine loops use a separate
prototype renderer. The animated swash is not installed or enabled as a default;
the study does not modify the user's saved water settings.
Version08 retains the physical shore mesh and cellular foam detail of version07,
then aligns the water, ripples and prototype foam motion with the local coast.


## Earlier steady shoreward motion — superseded by the pulse below

Enhanced water uses the shared continuous coast field in both games. Offshore
water retains the saved current bearing. Across a 1.5–3.5-cell approach band
(derived from the existing foam-width control), colour, both ripple normal layers,
broad waves and shore film blend into coastal coordinates. The inner0.35cell of
sea excludes the offshore motion; river banks use a0.10–0.65-cell band and keep
the channel's downstream flow where a bank normal is undefined.

Coastal texture V is `-signedDistance - uniformSpeed*time`. Its U uses two spatially
blended world-axis charts, weighted to suppress the singular projection. Every
chart advances toward smaller water distance regardless of texture orientation
or offshore bearing; periodic texture sampling supplies the loop. Normals decode
through the same chart gradients. A spatially varying `direction*time` would
accumulate shear, and temporal phase resets can make correlated texture features
oscillate, so neither is used for the coastal branch. Field-gradient and coastal
texture reads are skipped outside the approach band. Existing F5 values/defaults
are unchanged; zero pan and flow still intentionally stop texture motion.

Environment-only `OPENCNC3D_COAST_FLOW_AUDIT` modes expose the ungraded distance/
coastal weight/river field(1), colour UV markers(2), ripple layers(3/4), or broad
wave UVs(5). `OPENCNC3D_COAST_FLOW_ROTATION` rotates diagnostic marker artwork in
degrees before sampling. These are test overrides, never saved UI controls. Use
raw scene field captures, fixed cameras, camera shake and battle lights off, and
exclude occluded/shore-film/pyramid-border pixels when measuring image motion.
The animated swash/foam artwork remains an isolated review study.


## Water plane, shoreline coverage and pulsed swash — 21 October 2026

Enhanced uses one horizontal surface per connected water body. Source wet masks
connect across edges and diagonals; opaque TD bridge footprints join existing
wet banks without turning the deck into water. Authored FALLS1/2 retain their
native drop and remain barriers. Body elevation comes from modal native pure-water
corners inside the playable map rectangle, with a weighted native fallback. The
outer pyramid border cannot vote a playable body to a different elevation. Dry
land, bridge support and object placement retain their native support; actual
wet support uses the same exact body plane as the water draw.

The water draw extends underneath partial-alpha banks, with its opaque bed kept
0.04 cell below the surface. Fully opaque land writes depth first; feathered
shore colour blends without writing false raised depth. Colour, physical normals
and coast masks share the accepted atlas alpha. Snow whole-template water seeds,
occupied-icon context and wet-side RGB extension remove painted static water and
opaque tile fringes while retaining actual rock, snow and native palette pixels.
Physical relief tapers to zero at wet source pixels. Cache keys include terrain,
hole, pad, relief, map rectangle, scene and water-enabled generations as relevant.

The normal pass marks visible water as class0.28 with a planar upward normal.
A per-frame screen-space mask redraws the production opaque terrain prepass at
the land's actual projected height, with the same alpha>0.999 threshold and
read-only scene depth. This protects dry cliffs below a nearby river plane as
well as equal-height land. Sampling coast alpha at the lower plane cannot replace
this ownership test: elevated, partially transparent banks project elsewhere.
The mask has a separate colour attachment and detaches borrowed depth before
the water shader samples it, avoiding framebuffer feedback. Strict depth testing
and disabled polygon offset preserve other foreground occluders. Water
receives sky fill and direct sun visibility, excludes ground colour grading and
opaque surface ambient occlusion, and preserves real terrain/object occluders.
Blended shoreline pixels cannot inherit the old opaque-object black outline.
`tools/gate-water-normal-ownership.py` checks actual mask/depth/normal readbacks
against an opaque-terrain reference and independently sampled wet/dry source
support. The SCB01 dry-cliff regression rejects the earlier helper's 186,586
false-water pixels while requiring the fixed 400,000-pixel dry region to retain
its original normals and material class exactly. A freshly redrawn water-disabled
reference allows six float32 depth units of roundoff (measured maximum five,
less than 0.000015 world-cell height); earlier stale-frame comparisons are not
independent reference proof.

F5 > Water Shader adds independent **shoreline swash** (`water_swash`). Enhanced
defaults on; Classic/N64 default off. Existing presets inherit that new control;
explicit opt-outs survive Save/Load. Legacy `water_foam 0` and every other saved
value remain unchanged. With swash enabled its cellular bubble foam replaces the
legacy smooth rim. Physical shore film advances and retreats over a tide-dependent
low-bank region; rock relief and high cliffs stay out of the film.

Water colour, both ripple layers, broad waves and shore film use one phase clock.
A six-second cycle at crest speed0.35 accelerates with beach run-up, peaks
at phase0.28 and gradually slows during retreat. Speed ranges from0.20 to1.8667
times the mean, with no stopped interval. The exact integral of the quintic surge
keeps travel forward and continuous through the loop; multiplying elapsed time
by the changing speed would instead cause reversals and jumps. Crest speed0
holds the beach cycle and retains steady water drift.

F5 **shore reach (cells)** (`water_align_length`,0–8, default0) is the fully
aligned distance from shore. **shore blend (cells)** (`water_align_blend`,0.05–8,
default2.53437) is the additional transition back to the offshore current. Their sum
is the outer limit: the latest saved default is2.53437cells, and the maximum is16cells, within
the precise field range. This fades sampled colours and normals together without
stretching ocean UVs. Both controls save/load normally; older CFGs inherit the
defaults and existing saved values remain unchanged.
As the swash settles, the same surge value fades aligned colour, both ripple
normal octaves and broad reflection slopes back to their ordinary ocean samples.
At zero surge the ocean chart is exact; the next run-up smoothly restores shore
alignment. A frozen crest clock or zero flow/pan also removes the aligned chart.
The separate beach foam remains on its physical swash cycle. This is a temporal
sample crossfade; it does not rotate or deform UV coordinates.

River bank steering retains its0.10–0.65cell band. Continuous dual chart weights
avoid branch cuts where opposing shore normals cancel.

The actual GPU distance field is RGBA16 over ±16cells (step32/65535cell). Flow and
coverage bytes retain exact byte×257 promotion. The legacy range4 diagnostic stays
compatible. `OPENCNC3D_WATER_FIELD16_DUMP` records actual GPU readback; audit mode6
shows phase, run-up and positive motion; audit mode7 shows the actual speed gain
divided by2. Audit mode8 records temporal presence, spatial band and their combined influence. See the versioned verification helpers and
receipt in `docs/verification/2026-10-21-water-plane-and-swash/`. Source contracts,
GPU checks, visual review and installed Dock delivery are separate evidence.

Desert FORD1/BRIDGE3/BRIDGE4 now use complete saved rock-bank regions, shared cliff
texel density and UV-paired height/normal/specular data. Native crossing tracks,
bridge masonry and accepted coast alpha remain exact. The reproducible source
and53-cell gate are in `desert_crossings_v05/` plus the reversible `desert_crossings_v06/` approach
repair and
`tools/gate-desert-crossings.py`. Destroyed bridge IDs170/172 still use native
broken-deck fallback art; no new destroyed-bridge model or artwork is implied.


## Runtime mineral staging order

`stage_desert_details.py` refreshes raw theater scatter packs as part of coupled
scenery delivery. The mineral normal/specular stage must run after that refresh.
The app-alias stager now reapplies mineral maps last when the installation already
contains mineral data; fresh full builds retain their existing final material
stage. `tools/gate-installed-stone-materials.py` verifies actual deployed packs,
paired images/masks, physical keys and preserved scatter geometry against the
active cache, with an optional mirrored-install check. Validating only authoring
cache maps cannot detect a later raw-pack overwrite. The regression includes a
negative raw-scatter reset and preserves exact source albedo/UV/geometry.

The small Temperate coast gate retains its former green-hue count for diagnosis,
but evaluates actual water material and depth ownership. Current natural-colour
water can satisfy that old hue threshold: the diagnosed4354green pixels were all
water class0.28 in an exactly matching depth/normal capture. The older steady-flow
renderer fails the new ownership gate (25,800nonwater pixels), so the regression
still rejects the fault without treating accepted water tint as grass.


## Projected shoreline contact and swash approach — 22 October 2026

The raised beach and the sea have different projected edges. The previous late
surface first protected actual opaque terrain, then cut water a second time at
the flat map-space signed-distance boundary. That left a strip of static painted
water below the raised beach. The normal pass already classified this strip as
planar water. Coverage now follows the projected opaque-terrain ownership mask,
with a short 0.08-cell feather limited to 12 screen pixels. Colour sampling uses
linear filtering only during those passes; exact pixel-centre ownership and the
normal/depth mask retain their existing nearest filtering.

Swash uses two supports with the same clock, field, crest and cellular texture:
the exact native beach mesh for wet sand, and the existing flat water body for
approaching foam. The beach pass no longer snaps a subset of its lattice vertices
to sea level, which bent mixed triangles below the ground and exposed hard joins.
Its coverage follows the actual ground alpha; the fallback without a painted base
uses the source wet mask. The water pass carries foam without a second sand wash
and rejects projected opaque land. Crests begin 0.40 cell offshore, cross the
contact edge and reach the saved inland span. Both ends of the envelope fade
before the geometry/shader cull, avoiding a hard line at the previous cutoff.
Flow pacing, reach/blend sliders, swash timing and all saved CFG values are unchanged.
Existing coast-flow debug views isolate their water data by suppressing this new
foam-only plane; production swash checks use ordinary final-colour captures.

The controlled GDI1 candidate comparison selected the reported contact strip by
actual GPU water material, projected source field and opaque-bank mask, without
hue classification. All 1,142 eligible strip pixels move and receive foam during
the 91-frame cycle (the previous build moved 69.7%). All 48,963 sampled approaching
water pixels receive foam. The matched no-shore control changes zero offshore
or far-land pixels. `tools/analyze-water-contact.py` records and replays this
measure; actual installed repeats and delivery receipts accompany the local
verification archive. Allied1, Desert and RA Temperate candidate captures also
pass. These are technical checks, not universal artistic approval.

## Coastal swash scope and Desert contact repairs — 22 October 2026

Beach swash and its pulsed shoreward water chart now exclude rivers. The saved
width classification is combined with native river, ford, falls and bridge
identities, including channels attached to a wider ocean. Dry bank vertices
inherit the nearest wet sample's class; both shore passes and the water material
use the same interpolated permission. Ordinary river flow, water geometry and the
shared phase clock are unchanged. The F5 swash help states that rivers are
excluded. No controls, ranges or saved defaults change; the exact 13:51 CFG remains
the Enhanced factory snapshot for both games.

Desert P01 and P03 now use explicit rock keeper mattes with an eight-source-pixel
soil feather. The original paintings, rock faces, relief and native footprints
are preserved. Separately painted Desert road exits share existing D10 earth
detail over twelve source pixels at external template ports. Internal road art
and all coverage/geometry remain exact. Saved sources, hashes, all-black paired
player masks and reproducible exporters live in
`art/remaster/textures/unified_daylight_v01/desert_feature_edges_v01/` and
`desert_road_exits_v01/`. Source tests exercise actual permission sampling, bank
extension, conserved rock pixels and matching road-port overlap texels. Hidden
candidate and installed comparisons, shader checks and delivery receipts are
recorded in the dated local verification archive. Technical validation does not
confer general artistic acceptance.

### Nod 1 road-rock identification follow-up

The map confirmation identifies the orange two-rock cluster as a detached
painted feature in D29 (template121, icon2), rather than P03. The dedicated
`desert_feature_edges_v02` matte feathers D29's two detached rock/soil components
with explicit keeper polygons. Its road component, original painting and geometry
remain exact. The earlier P01/P03 correction remains independently versioned.
`desert_road_exits_v02` restricts road-exit material to each template's connected
road body, preserving detached rock artwork even when it touches a cell edge.
Nod1 captures cover the actual D29/D20/D22 location and its reported seam. This
follow-up changes Enhanced assets and compilation only; water shaders, controls,
saved defaults and timing are unchanged from the preceding verified build.

## Effects and 3D infantry grades — 24 October 2026

F5 > Enhanced exposes two independent colour grades under Effects and Soldiers.
`efx_` controls affect effects; `inf_` controls affect 3D infantry. Brightness,
contrast, gamma, saturation and RGB balance use neutral1; exposure, warm/cool and
green/magenta use neutral0. Exposure ranges−3..+3EV in linear-light stops.
Contrast ranges0.25..3 and fixes black/white; gamma0.4..2.5 adjusts midtones.
Saturation and RGB balance range0..2; colour balance ranges−1..+1. Existing
particle brightness keeps0.25..2; soldier brightness ranges0..2.

Keys in each group: `bright`, `contrast`, `exposure`, `gamma`, `sat`, `warmth`,
`tint`, `red`, `green`, `blue`, prefixed `efx_` or `inf_`. The existing
`efx_bright`, `efx_contrast`, `efx_sat` stay compatible with earlier presets.
Additions inherit neutral defaults from old presets and persist through ordinary
F5 Save/Load. Scene exposure/colour and other object groups remain separate.

The current soldiers' `m3inf_sun`, `m3inf_fill`, `m3inf_paint`, `m3inf_shadow`
and `m3inf_runrate` rows now live in Soldiers, reachable with experimental terrain
off. Their keys, ranges and previous values remain. The grade operates after
model lighting and before the existing display-space shroud multiplier; it does
not brighten hidden infantry. Non-soldier models, normal/depth targets and their
shadows retain their paths.

Effects grading retains source alpha and the neutral bypass. Unity effects grade
only their layer's radiance, leaving scene transmittance/refraction separate;
positive exposure/brightness retain intensity through the intermediate linear
composite. A uniform zero-preserving transform avoids a hard branch at faint
smoke edges. Native effect meshes mirror their texture combiner and only leave
batching when a grade differs from neutral. Bloom radiance comes from the graded
scene; its explosion/muzzle classification mask stays ungraded. Terrain/water/depth
settings remain unchanged.

RA Allied uniforms use source light blue(sRGB104,116,160) and the current
material's explicit ORM-alpha weighted linear albedo reference. This replaces a
single stale reference for the Enhanced Allied blue material. The user's
Soviet1 correction also maps campaign infantry in blue slot6 to Allied blue1;
multiplayer/skirmish owner colours keep their chosen source remaps. E6 keeps
its uncoloured uniform; other cloth remaps retain their calibration. No hue classification or new asset bake is used.
RA owner paint uses its source remap palette rather than TD house colours. The
Codex and pose-sheet fixtures select actual RA remaps as well.

`tools/gate-object-grades.cpp` checks both modules' visible schema, label fitting,
neutral defaults and modified-value roundtrip. The compiled production helpers in
`tools/tests/test_ra_infantry_colour.py` check remaps, empty masks and differing
atlas brightness. Actual hidden GPU/F5, installed frontends/HUD/fog/startup and
preservation proof belong to the dated delivery receipt, separate from CPU checks.

### Desert impact decal blending — 24 October 2026

Enhanced Desert craters retain their original RGB and irregular support. Their
body now mixes at up to60% coverage with a gradual inward30/144-cell edge fade;
soot uses35% of its original density around the existing neutral multiply. This
keeps the sand grain visible through the depression instead of layering an
opaque black bed over an already dark multiply. No new pixels extend the mark.
Tracks, other theaters, Classic/N64 and water retain their current rendering.
`tools/gate-desert-decal-blend.cpp` checks the actual delivered pack as well as
independent RGB/support/ground-grain fixtures. Hidden old/new/ground-only shots
cover all six crater types and scorch marks; the dated delivery retains proof.

## Windows opaque-land alpha — 26 October 2026

The RTX 3070 native reproduction showed empty opaque-land ownership targets in both games. `GL_GREATER, 0.999` can quantize the reference to 1 and reject opaque alpha. Both native terrain depth/ownership prepasses now use `GL_GEQUAL, 1.0`; colour feathers retain their existing threshold. This removes RA Soviet 1's rectangular snow shading and restores the proper land/water lighting classes and land shadows. The same shared failure was checked in Dawn's coastal view. Standalone GPU proof, before/after raw ownership and normal readbacks, inspected screenshots, all four faction build checks and both surrender regressions are retained in `docs/verification/2026-10-26-v091-development/`. Current-main Mac Dock aliases and hidden renderer were rebuilt and verified; Windows tests ran invisibly on the RTX 3070. The earlier camera and texture-unit candidates were removed.
