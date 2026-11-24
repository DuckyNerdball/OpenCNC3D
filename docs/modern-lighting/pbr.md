# Building and vehicle materials (pbr)

## What it does

Remastered buildings and vehicles (and cartridge ones the structure and vehicle groups
mark) are relit in the light pass with the material model the 3D soldiers already use,
instead of one fixed roughness, Lambert, a white hemisphere fill and the specular map's
two lobes on top:

- **Sun:** Cook-Torrance, GGX distribution, Smith height-correlated visibility, Schlick
  Fresnel, with f90 following f0 down below 0.04 (Filament's `clamp(50 f0)`), in the
  sun's warm white (the colour the old highlight used), scaled by `pbr_sun_strength`
  and the specular maps' `vspec_sun`.
- **Sky, diffuse:** the fill keeps the relight's own brightness (`0.6 + 0.4 n.y` times
  the fill dials) and takes the hue of the sky pack's nine-coefficient irradiance in the
  surface's direction, at `pbr_sky_tint` strength.
- **Sky, specular:** the sky pack's prefiltered cube, sampled at roughness times the
  top mip, with Karis's analytic split-sum environment BRDF, held out of creases by
  Lagarde's specular occlusion from the screen-space AO term, scaled by
  `pbr_ibl_strength` and `vspec_sky`. On a dielectric it is added only in the measure
  the surface is smooth (`pbr_sheen_weight`: full at glass's roughness 0.2, none from
  0.45 up): a rough painted surface's even sky veil is already in its painted colour,
  and adding it again laid a milky film over every dark texel. What is added comes off
  the diffuse (energy conservation). Metals always reflect in full.
- **Painted cavity:** reflections and the sun lobe fade to nothing on texels painted near
  black (soot, grime, seams), except smooth ones (dark glass does mirror the sky).
- **Metal:** loses its diffuse; its reflectance is its painted (display) value, so a
  mid-grey steel lands at 0.5 to 0.6, where real steel, zinc and aluminium sit.
- **No map:** a pixel no specular map covers (a cartridge mesh, a wall, a model without
  a `.spec`) keeps today's light whole, the old lobe and `relight_spec` with it, and only
  takes the sky's colour in its fill.
- **Indoors** (Red Alert missions with 3D interior walls) the cube and its hue are not
  used: reflections see a white hemisphere at the fill's level.

Which pixels: the normal target's class bands for remastered buildings (0.365 to 0.395)
and remastered vehicles (0.335 to 0.355), plus the structure and vehicle groups'
cartridge marks when those groups are in use. Not the crates, haystack fields, crashed
Orca wreck, ore and Red Alert's indoor walls (class 0.36), foliage, ground, water,
self-lit (emissive) texels or soldiers; they keep the old relight exactly. Team colours
are never metal (saturated texels), and nothing outside the unified light model changes.

## The material, derived per texel

No hull carries authored roughness or metalness yet. The specular target
(`evhc_draw_spec`, maps baked by `tools/art/remaster/bake_spec.py` from `spec_map.py`)
holds red = strength (metals 0.4 to 1, glass 1, paint 0.3 to 0.42, masonry and rubber
below 0.2), green = tightness (rubber 0.15, concrete 0.2, paint 0.45, steel 0.65,
polished and glass above 0.9). `pbr_material` in `game/pbr_glsl.h` maps them:

| output | formula |
|---|---|
| metal class | `smoothstep(0.555, 0.60, green)`: steel 0.65, pipe 0.60, bronze, polished, glass; NOT plate, trim, galvanised, tower or paint (0.55 and looser) |
| metalness | `smoothstep(0.45, 0.85, red) * metalClass * (1 - smoothstep(0.25, 0.55, saturation)) * smoothstep(0.015, 0.06, linear luminance) * pbr_metal_scale` |
| paint on steel | a texel of a metal-class part below glass's tightness (green under 0.8) that is not metal takes paint's tightness (0.45) and f0 0.04 |
| roughness | `mix(0.95, 0.15, green) * vspec_gloss^-0.25 * pbr_rough_scale`, clamped 0.06..1 |
| dielectric f0 | `0.04 * clamp(red / 0.3, 0.2, 1.5)` (paint 0.04, concrete and rubber less, glass 0.06) |

Why metal is read from green and not red: the baker gives every part one tightness by
its class, but scales the strength by how light each texel is against its part. The
barracks' camouflaged roof is plate (0.55 tight) and its light grey patches reach steel's
strength; as metal they mirrored the sky as light blue speckles on a desert base.

Pixels with no map take `relight_rough` and the old lobe, and no metal. Authored maps
replace `pbr_material` and nothing else.

## Dials (F5, LIGHTING tab, group "6b BUILDING AND VEHICLE MATERIALS", per theater)

| key | default | meaning |
|---|---|---|
| `pbr_on` | 1 | master; 0 is the old relight, byte for byte |
| `pbr_rough_scale` | 1.0 | multiplies every roughness |
| `pbr_metal_scale` | 1.0 | multiplies every metalness; 0 lights everything as paint |
| `pbr_ibl_strength` | 1.0 | sky reflection strength |
| `pbr_sky_tint` | 0.12 | how far the fill and the reflection take the sky's colour |
| `pbr_sun_strength` | 0.7 | the sun's highlight on every material; 1 is the physical strength |

The specular maps' dials (group 6, `vspec_*`) keep working on material pixels:
`vspec_sun` multiplies the sun lobe, `vspec_sky` the sky reflection, `vspec_gloss`
tightens every highlight (roughness by its fourth root, as the old Blinn exponent was
multiplied by it), and the map's blue (upright sheen, Red Alert's drums) widens the sun
lobe to at least 0.6 roughness and raises its weight up to four times. `relight_spec`
and `relight_rough` act on pixels without a map, as before.

A settings file without these keys loads the defaults (verified: the saved player
settings lack them and load with the feature on).

## Linear space

The feature runs in both light passes, and `linear_on` picks which.

**`linear_on 0` (the gamma chain).** The diffuse is today's ratio
`col * clamp(light / bakedShade, 0, relight_clamp)` without the old lobe, decoded with
`pbr_to_linear` (pow 2.2, a product rule, so `decode(texture x shade) =
decode(texture) x decode(shade)`); the lobes are added in linear and the sum encoded
back with `pbr_to_display`. Energies (`pbr_irradiance`) are the relight's gamma-domain
sums decoded: the total diffuse light is decoded as a sum (so a rough dielectric keeps
today's brightness) and split by share; the reflection level is the sky's share of the
open-air light (`skyUp / (skyUp + sun)`, from the scene's sky and sun, not the structure
and vehicle shade dials) carried as a ratio onto the decoded open-air light. This is the
reviewed feature, unchanged pixel for pixel.

**`linear_on 1` (the linear light pass, `linear.md`).** `col` is already radiance when
the relight runs, and the material follows the same conversion the plain relight uses:

1. The diffuse is `col * clamp(fx_lin_gain(lDiff) / fx_lin_decode(shade), 0,
   fx_lin_gain(relight_clamp))`: the converted light over the decoded bake shade, with
   no decode of its own.
2. `pbr_irradiance` converts its energies with `fx_lin_gain` (`linear_light_k`) instead
   of a fixed pow 2.2, so they stay in step with that diffuse when the dial moves. The
   sky-share ratio carry stays: the linear pass converts the tuned sky and sun rather
   than summing them with energies of their own, so the dials' one to three is still the
   art direction.
3. The lobes are added straight to the radiance (`col += ...`), with no decode before
   and no encode after. They are new physical light, so they are not wrapped in
   `fx_lin_gain` and nothing pre-exposes them.
4. What the material reads as a display value (`albG` for the metal estimate, the
   cavity and a metal's f0, and the luminance gate under the shroud) is
   `fx_lin_encode(col)` over the shade, the numbers they were tuned on.

At the default `linear_light_k 2.2` each of those is the gamma figure decoded, so the
materials keep their character and strength. What differs is downstream and shared with
every other surface: the float lit target keeps a highlight above white instead of
clipping it, and the film curve rolls it off. The feature adds no pass and no target of
its own, so it asks nothing of `fx_hdr_want()`.

Texture units of the light program in this build (one program, every sampler always
declared): uColor 0, uDepth 1, uShadow 2, uCloudTex 3, uGNorm 4, uBake 5, uInfShadow 6,
uTibField 7, uTerrainShadow 8, uTerrainVisibility 9, uTerrainWater 10, uRainWater 12,
uVhCards 13, uSpecMap 14, and this feature's sky cube uPbrEnv (samplerCube) on 15
(`PBR_ENV_UNIT`, pbr_mod.h). Fifteen of sixteen; unit 11 is free. Linear light adds no
sampler. A sibling that also binds unit 15 cannot share it: a 2D and a cube sampler on
one unit fail at draw time.

## Files

- `game/pbr_glsl.h` (new): the GLSL helpers, uniforms, extension line.
- `game/pbr_mod.h` (new): host uniforms, cube bind on unit 15, indoor switch.
- `game/fx_post.h`: program assembly (extension after `#version`, helpers before the
  body), one branch in the relight, one branch at the specular-map step, the hooks.
- `game/fx_state.h`: five dials, defaults, the group's lighting-tab line.
- `game/cnc_eyes.cpp`: `#include "pbr_mod.h"` after `sky3d_mod.h`.

## Verification

- 17 of 17 scene pictures byte-identical to the base build with `pbr_on 0`.
- Two runs with the feature on: 17 of 17 pictures identical (deterministic). A run with
  no `--gfx` at all (the saved settings, which lack these keys) equals the `pbr_on 1` run.
- Measured on the pixels that change (more than 2 of 255), not the whole frame, where
  grass and ground dilute it. Object luminance, before to after: td_base -3.0%,
  td_base_close -3.7%, td_desert_base -1.8%, td_nuke -1.5% / -1.7%, td_wreck -1.0% /
  -1.4%, td_desert +0.5%, td_forest +3.7%, Red Alert scenes within 0.6%. Pixels darker
  than 30 of 255 stay within 3.4 levels (no lift). House colours (saturation over 0.5):
  GDI yellow 0.758 to 0.738 (td_base), 0.841 to 0.814 (td_desert_base), 0.825 to 0.789
  (td_wreck); Nod red in td_forest 0.809 to 0.695, where the power plants' dome panels
  face the sun and take a satin sheen. Mean colour shift over changed pixels at most
  -5 red / +4 blue of 255.
- The first value pass of this feature measured +10 to +34% object luminance, dark
  texels lifted from 13 to 26, house colours a fifth less saturated and a blue cast on
  desert bases; the reflection on rough paint and on unmapped meshes caused it, and the
  sheen weight, the no-map rule and the lower tint remove it.
- Looked at, side by side: the desert barracks' camouflage roof stays olive and grey
  (no blue speckle); the refinery's steel tanks and pipes read as steel with the sky in
  them; yellow paint keeps its colour with a satin highlight that moves with the
  camera; sandbags, walls and Nod turrets with no map keep their old look.

- On the linear foundation (`visual/modern-pbr-lin`), 17 pictures each, checked with
  cmp: both masters off (`linear_on 0`, `pbr_on 0`) equals the base build 17 of 17;
  `linear_on 0` with the feature on equals this feature's reviewed gamma pictures 17 of
  17; `pbr_on 0` with linear on equals the linear foundation's own pictures 17 of 17.
  Two runs at the defaults (both on) are identical, 17 of 17.
- The feature's change with linear on (both on against linear alone) matches its change
  in gamma (on against the base build) pixel for pixel: correlation of the luminance
  change 0.99 to 1.00 on every scene where buildings or vehicles show (0.95 on
  td_desert, 2,191 pixels). Over the changed pixels, object luminance in gamma and in
  linear: td_base -3.8% / -3.6%, td_base_close -4.6% / -4.5%, td_desert +0.6% / +0.9%,
  td_desert_base -4.0% / -4.1%, td_forest +4.2% / +4.2%, td_nuke -4.0% / -4.2% and
  -4.5% / -4.5%, td_wreck -1.3% / -1.1% and -1.7% / -1.6% (threshold 2 of 255 on
  luminance, so these differ a little from the figures above); saturation changes agree
  to 0.002. Whole-frame means move by the same -0.25% to +0.18% in both. On two
  close-ups (the desert refinery's tanks and pipes, a power plant's dome) the linear
  change is 0.985 times the gamma one, the shortfall being the film curve on the
  brightest rims. No default was retuned.

## Cost

framebench 60, 1600x900, Apple Silicon, with other renders running on the same machine:
td_base_close on 21.6 / 23.9 ms median against off 22.7 / 22.4 / 24.2 (a first warm-up
sample of 29.8 left out); a power-plant close-up filling the frame on 15.4 / 14.1 / 14.4
against off 14.9 / 15.6 / 14.1. The difference is below the noise of a shared machine;
the added work on building and vehicle pixels is one cube fetch, nine SH terms, one GGX
evaluation (the old lobe's is still computed, and used only where no map covers the
pixel) and one more split-sum fit.

## Known gaps

- Material is derived from the specular maps, not authored. Metal is decided by the
  map's tightness (steel and pipe yes, plate no), so painted plate that is really bare
  sheet metal reads as paint, and a light grey texel on a steel part reads as steel.
- The satin highlight on paint is white-warm, not the paint's colour, so a saturated
  panel facing the sun's mirror angle loses some saturation there (Nod red domes in
  td_forest: 0.81 to 0.70 over those panels). `pbr_sun_strength` sets it.
- Objects end up 1 to 4 per cent darker in most base scenes: the old highlight was
  warm and tight and the new one is broad; rough paint no longer takes the old pale
  sky sheen at grazing angles, so dark creases are up to 3 levels darker.
- The sky is one photograph (a canyon rim) for every theater; its colour is held to
  12 per cent, which leaves a slight cool shift in the shade (at most -5 red, +4 blue
  of 255 over changed pixels).
- Red Alert's remastered buildings in the test scenes do not take the relight at all
  (before or after this change), so materials show there only on what the light pass
  relights; only a faint tint change on a few hundred pixels.
- Only the main sun casts a lobe; the second sun lights the diffuse only.
