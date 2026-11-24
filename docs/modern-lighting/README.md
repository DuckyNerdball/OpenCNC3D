# Modern lighting

The Enhanced renderer draws the world forward into a scene target, then relights it in a
screen-space light pass (`FX_FS_LIGHT` / `FX_FS_LIGHT_BODY` in `game/fx_post.h`), then
lays on the atmosphere, bloom, the resolve, CRT and HUD. The features below sit in that
chain. Each one has a master dial in `FX_PARAMS` (`game/fx_state.h`); every master is on in
`fx_defaults`, and a settings file without the new keys loads them at those defaults.
With every master at 0 the picture is byte for byte the one the renderer drew before any
of them existed.

## The features

**Linear light** (`linear_on`, [linear.md](linear.md)). The foundation the others are
written against. The light pass decodes the scene colour once, lights it as radiance into
a float target, and the resolve maps it to the screen through a film curve, so fire cores,
lamps and highlights roll off instead of clipping. Every tuned light factor goes through
`fx_lin_gain`, which keeps the look it was tuned for. Its section "The contract for the
features merged on top" is the rule every other feature follows.

**Ground-truth ambient occlusion** (`ao_mode`, [gtao.md](gtao.md)). GTAO replaces the
hemisphere SSAO: its own pass and a depth-aware blur, read by a build of the light program
compiled with `FX_GTAO`. Wall feet, unit contact shadows and creases darken, and pale
surfaces fill their own creases back in (the multibounce term).

**Building and vehicle materials** (`pbr_on`, [pbr.md](pbr.md)). Relit buildings and
vehicles become GGX materials whose roughness and metalness come from the specular maps:
metal mirrors the sky cube, paint keeps a satin highlight, and the fill takes the sky's
colour.

**Ground bounce** (`bounce_on`, [bounce.md](bounce.md)). The ground's own colour and
daylight replace the fixed grey that lit the undersides of buildings, vehicles, trees and
soldiers, so sand and snow throw warm or cool light up onto what stands on them. Built
into the light program with `BOUNCE_LIGHT` (and `BOUNCE_LINEAR` under linear light).

**Flash glints and blast shadows** (`light_spec`, [glints.md](glints.md)). Explosions,
muzzle flashes and fires put a highlight on hulls and metal, and the biggest blasts are
stopped by what stands between them and a surface.

**Ground materials** (`ground_mat_on`, [ground.md](ground.md)). Snow, the wet shore, dry
sand and grass each answer the sun as their own material in the light pass's terrain
branch.

**Light shafts** (`shafts_on`, [shafts.md](shafts.md)). The sun's shadow map is marched
through the desert dust, the mist and a thin low haze, so cliffs and buildings cut dark
wedges into the air. Its own two air programs; it adds nothing to the light pass.

**Lit soft smoke** (`smoke_lit`, [smoke.md](smoke.md)). Smoke and dust particles are shaded
by the sun as lumpy balls and fade where they meet the ground. Drawn in the forward pass,
before the light pass.

**Temporal anti-aliasing** (`aa_mode`, plus `hidpi`, [taa.md](taa.md)). A jittered blend
on the resolved display image, with a float history. Full Retina resolution for the
player's own game is an opt-in (`hidpi`, off by default). It is the most expensive single
setting: a borderless window on a 3008x1692-point display is drawn at 6016x3384 with it on,
and measured there Tiberian Dawn's base ran at about 15 fps against 29 off, Red Alert's
snow mission 18 against 45 and a Red Alert skirmish 23 against 55 (scripted runs that wait
for the blend, so low). It is one setting for both games, kept in Tiberian Dawn's
`opencnc3d-fx.cfg`: Red Alert reads it from there whenever that file exists, and saving the
row from Red Alert's F5 panel rewrites that one line of it. It applies at the next start.
The menus, fullscreen and mode switching at 2x have not been played.

**Snowfall** (`snow_fx`, [snow.md](snow.md)). Weather rather than light, kept here because
it is drawn into the same chain and checked with the same shots. Soft white flakes fall,
flutter and drift on the wind over Red Alert's snow maps only (never Tiberian Dawn, its
winter maps included, and never Red Alert's temperate or interior maps), placed by a
vertex shader from a lattice of seeds built once into a static buffer, in one draw call; a
zoom changes how many are drawn and how large, never where they are. They are drawn inside
the world pass between the anti-aliasing's two effect copies, so the reactive mask keeps
them crisp, and the light pass shades them as the ground behind them, as it does the rain.
Its dials are in the F5 panel's WEATHER tab (`3d2 SNOWFALL`), one setting for every theater.

## Where the switches live

The nine masters are global: one choice for every theater. `linear_on`, `ao_mode`,
`ground_mat_on` and `aa_mode` sit in groups that keep no per-theater copy, and
`shafts_on`, `bounce_on`, `pbr_on`, `light_spec` and `smoke_lit` are excluded by name in
`fx_lighting_param`, as `ao_mode` is in its group. So a feature switched off in F5 stays
off on the next map, and a script's `gfx smoke_lit 0` is not undone when a theater's bank
loads. Their tuning dials (strengths, radii, haze, `linear_exposure`) stay per theater with
the rest of their groups. A saved file whose banks still carry rows for those five masters
loads cleanly: banks are read by key name, and a row that is no longer banked is ignored.

## The light pass's texture units

Apple's legacy GL 2.1 gives the fragment stage sixteen texture units, 0 to 15. Every
sampler any build of the light program declares has its own unit, fixed in
`FX_LIGHT_UNITS` in `game/fx_post.h`:

| unit | sampler | what it is |
|---|---|---|
| 0 | uColor | the scene colour |
| 1 | uDepth | the scene depth |
| 2 | uShadow | the sun's shadow map |
| 3 | uCloudTex | the cloud mask |
| 4 | uGNorm | normals and material class |
| 5 | uBake | the cartridge's baked shade |
| 6 | uInfShadow | the 3D soldiers' own sun map |
| 7 | uTibField | the Tiberium's glow and height field |
| 8 | uTerrainShadow | the terrain's own shadow |
| 9 | uTerrainVisibility | the shroud's visibility field |
| 10 | uTerrainWater | the shore field |
| 11 | uBounceMap | the ground bounce map (bounce builds only) |
| 12 | uRainWater | the sea's rain field |
| 13 | uVhCards, or uGtaoTex | the vehicle cards' mask; in the GTAO builds, GTAO's result |
| 14 | uSpecMap | the specular maps |
| 15 | uPbrEnv | the materials' sky cube (a samplerCube) |

Unit 13 is the one shared unit. The vehicle cards' mask is one bit a pixel, its alpha, and
GTAO's result used its alpha for nothing, so on a frame that runs GTAO the blur's second
axis copies the mask into that alpha (`uVhPack` in `FX_FS_GTAO_BLUR`) and the `FX_GTAO`
builds read both from one texture and declare no `uVhCards`. Glints, ground materials,
linear light and the rest add no light-pass sampler. The shafts, smoke and TAA programs are
separate programs with units of their own.

The light program is built once per combination of the features compiled into it rather
than branched on (a uniform branch moved pixels of the build without the feature), and
`fx_light_variant` keeps each build for the process:

| build | samplers |
|---|---|
| `light` | 15 |
| `light+bounce`, `light+bounce+linear` | 16 |
| `light-gtao` | 15 |
| `light-gtao+bounce`, `light-gtao+bounce+linear` | 16 |

`fx_light_units_audit` checks this against the driver: when a build links it logs
`FX|light|units|<build>|built|samplers=N|of=16|fits|...`, and on the first frame the build
draws it reads every sampler's unit back from the program and logs the same line with
`drawn`. A build over the units, a sampler missing from the table or two samplers on one
unit says `FAULT` instead of `fits`.

## How the features compose

- **GTAO and the materials.** On a pixel lit as a material, GTAO's sky share is the
  material's own fill over its own diffuse light (`lAmb / lDiff`), so the occlusion
  darkens the material's image-based fill, and the sky reflection is held out of creases by
  the material's specular occlusion read from the same visibility. The sun's highlight and
  the sun's diffuse share stay outside the occlusion except for `gtao_direct`.
- **GTAO and the bounce.** The bounce is indirect light, the same kind as the sky's fill,
  so the occlusion takes it with the sky's share (`gtao_share_bounce`): a crease loses its
  bounce as it loses its fill.
- **The bounce and the materials.** The bounce replaces the lower half of the hemisphere
  the materials' fill uses (the old fixed grey ground), so on a material it joins the
  diffuse once, in the units the plain relight gives it, and is never counted twice. It
  takes none of the sky's hue: the hue's share is measured against the light with the
  bounce in it.
- **Glints and the materials.** Where a pixel is lit as a material from a specular map, a
  flash's highlight uses the material's roughness (with `pbr_rough_scale` in it) and its
  metalness: metal keeps the map's metal weight, paint takes the paint share and paint's
  reflectance. Elsewhere the glint reads the map as before.
- **Ground materials, bounce and shafts** read the same shadow and cloud terms: the light
  pass's `sh` and `cl` go into both `gm_ground` and `bounce_lower`, and the shafts march the
  same sun shadow map and terrain shadow with the same bias, and the cloud deck with the
  same drift, scale and cover.
- **TAA** blends the resolved display image, after the film curve and the encode, so its
  neighbourhood clamp works on encoded values and a fireball leaves no trail; its history
  is a float target. Under TAA, GTAO's noise pattern turns with the blend's frame count so
  the history averages it out (scripted shots stay deterministic: a frame count, not a
  clock).
- **Smoke** is drawn in the forward pass into the display-encoded scene target, so it stays
  gamma and the light pass decodes it with everything else.

## Verification

With every master at 0 the 17 before/after shots are byte-identical to the build without
any of these features, and so is `td_rain` (td_base under heavy rain, shot only when named:
`modern-shots.py --scenes td_rain`), the one view that reaches the rain's air, puddles and
overcast in the light pass. A scripted still's settle frames (taa.md) hold the mode
switch's hidden warm-up, the background ground build and the frame counter's clock, so a
`--shot` with the chain on keeps the mode rows the command line asked for; G36, which runs
the chain at `fx_defaults` and so with every master on, is the merge gate for this work. With linear light on and exactly one other master on, each feature
reproduces the shots it was verified with on its own branch, byte for byte. With all of
them on, two runs of the same script give identical pictures, and every scene's mean
brightness equals the base plus the sum of each feature's own change to within 0.04 of a
level, so the features neither cancel nor double one another. Two states are not stable
from run to run in any build of this stack, so a comparison that reaches them needs care:
on the Mac a Red Alert play state with the 3D ore can differ by a few pixels at the ore
nuggets, and the first view after a mission switch inside the process varies at the left
edge; compare them with `tib3d 0`, or by the majority picture over three or more runs.

Three gates measure what was tuned or approved under the lighting without this stack, and
pin its nine masters to 0 so they keep testing what they were written for: G305 (the F5
STRUCTURES group) in the scripts `gate_structs.py` writes, G360 (the damage overlay creeps
in without a seam) in the damage preset it loads, and G364 (placements draw as approved) in
the placement preset it loads, beside its `snow_fx 0`. With the masters at their defaults
G305 found its structure dials changing pixels outside the structures, against a structure
mask about twice main's; G360 found the damaged Construction Yard at a mean difference of
2.85 from its approved reference, on edges and a slight overall lift, with the seam and
soot claims passing; and G364 found the Red Alert Construction Yard on SCM03EA brighter
than its approved reference, with lit smoke and glints. Pinned, all three pass with main's
own numbers: G305's structure mask is 71,980 pixels and every dial's count is the one
main's renderer gives on the same scene, G360's damaged yard differs from its reference by
a mean of 0.78, and G364's worst shots differ from the references by 0, 1 and 0 pixels, as
with main's renderer. G36g (one key-colour
pixel left by the bilinear fix) is red on main as well and is not this work's.

## Speed

Five speed items were measured against this stack and merged into it: the ember glow on
damaged buildings keeps its posed pieces, the shore and the sea are drawn from arrays, the
ground-normal and bake passes skip state the card already holds, an array path for the
shroud, mottle and tiberium ground waits behind the `overlay_arrays` dial (off), and the
sun map's still casters were measured and left alone. [speed.md](speed.md) gives what the
merged renderer does to the frame on both platforms, with one paragraph and a page for
each item.
