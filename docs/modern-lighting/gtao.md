# Ground-truth ambient occlusion (GTAO)

Branch `visual/modern-gtao`, and `visual/modern-gtao-lin` on top of linear light (see
"With linear light" below). Replaces the screen-space hemisphere occlusion (SSAO) in
the Enhanced post chain with horizon-based ground-truth ambient occlusion, after
Jimenez, Wu, Pesce and Jarabo, "Practical Real-Time Strategies for Accurate Indirect
Occlusion" (2016), in the shape Intel's XeGTAO made common.

## What the player sees

- Units, buildings, crates, barrels and rocks sit in the ground: a soft darkening on
  the ground where they meet it, wider and smoother than the thin dark outline the old
  pass drew. Measured on the ground within eight pixels of a modelled object, against
  no occlusion: 1.3% to 5.2% darker per scene, where the old pass managed 0.1% to 1.3%.
- Inside corners darken softly: the floor of a Red Alert interior along its wall bases
  and around its pillars, the deck under the harvester's raised bin, the cracks between
  desert cliff rocks, the pad of a structure against its walls. On the Hand of Nod's
  pad (Tiberian Dawn temperate) the strip under the wall is 74.0 grey against 73.0 with
  the old pass and 76.9 with none: about the same depth, spread as a gradient rather
  than a narrow dark band.
- No grain, no smudges. The pass is blurred along surfaces and never across their
  edges; the old pass's dark blotches and halo discs on building pads are gone.
- Units and buildings keep their brightness. Each class was tuned against the old pass
  on its own (table below), not on the whole frame, so hulls are not duller than today.
- Whole-frame brightness moves by 0.0 to 0.6 levels out of 255 against the old pass.

## How it works

1. **Occlusion pass** (`FX_FS_GTAO_BODY` in `game/fx_post.h`), its own RGBA8 target at
   the scene's resolution. For each pixel: world position from depth; a normal (the
   class target's own normal on authored hulls and canopies, up on the cartridge's tree
   cards, otherwise the plane rebuilt from depth using whichever neighbour on each axis
   is nearer in depth); then `gtao_slices` screen directions, each marched both ways for
   `gtao_steps` depth taps out to `gtao_radius` cells (squared step spacing, a 1.3 pixel
   minimum). Each side keeps its highest horizon; an occluder further than the radius
   fades out over the last `gtao_falloff` of it. The visible arc between the two
   horizons, clipped to the surface's hemisphere, is integrated in closed form (cosine
   weighted).
2. **Sky only** (`gtao_sky`, on). The same visible arc and the arc an open surface
   would see are both cut to the half of each slice above the horizontal plane through
   the pixel, and the slices' sums are divided. So only things that rise above a point
   take its sky: the floor darkens at a wall's foot, the wall's foot is not darkened by
   the floor, a hull's flank is not darkened by the ground beside it, and a flat wall
   top no longer reads its own surface as an occluder along its edge (the faint line
   the first version drew on Red Alert interior wall tops). `gtao_sky 0` is the plain
   hemisphere search.
3. **Noise**: slice rotation from interleaved gradient noise, step offset from a 4x4
   ordered pattern, both functions of the pixel position only. `g_fxGtaoFrame` (C
   global, 0 by default) turns both per frame for a temporal accumulator to average;
   at 0 the pattern stands still and runs are deterministic.
4. **Blur** (`FX_FS_GTAO_BLUR`): separable Gaussian, sigma `gtao_blur` pixels, along
   x then y. Each tap is weighed by how far its depth is from where the centre's own
   surface would be at that tap, so ground seen at a glancing angle still blurs fully
   and silhouettes do not bleed. The target's green and blue carry the eye distance as
   16-bit fixed point over 256 cells for this.
5. **Light pass**: a second build of the light program with `FX_GTAO` defined
   (`g_fxPLightGtao`) is bound on frames where the pass ran. It reads the visibility and
   raises it to the exponent

       gtao_strength x ssao_intensity x class weight (ssao_ground / ssao_object / ssao_foliage)
         x on a structure: st_ao x gtao_structure
         x on a vehicle:   vh_ao x gtao_vehicle
         x on a canopy:    gtao_foliage
         x on any other modelled object (props, soldiers, ore, indoor walls): gtao_props

   An exponent of 0 is no occlusion (pow(0, 0) is never evaluated). The structures'
   and vehicles' group dials multiply the exponent here; the old method scaled its
   darkening by them, which at their saved value of 2 turned any visibility under a
   half to black. `object_shadow_fill` still applies after. The multi-bounce fit then
   lifts the occlusion by the surface's own colour (`gtao_bounce`), and the result
   darkens the sky's share of the pixel's light plus `gtao_direct` of the sun's. Each
   lighting branch records the sky's share in a plain `#ifdef FX_GTAO` block (ground,
   relit cartridge meshes, primitive-colour faces, split-light remastered models).
   Where a branch cannot separate sky from sun (the non-unified legacy path) the share
   stays 1 and occlusion darkens all of the light, as the old pass did.

Why two builds of the light program: adding the new terms to one program moved 1 to 7
pixels per picture by 1 or 2 levels with GTAO switched off, because the compiler then
rounded the shared light terms differently. Every GTAO line in the light shader sits
inside `#ifdef FX_GTAO` / `#ifndef FX_GTAO` blocks of plain statements (no
function-like macros, so the base program does not depend on how a driver's GLSL
preprocessor handles macro arguments across lines), and the base build is the old
program token for token.

The GTAO build of the light program is only built when the fragment stage has a
texture unit 15 (`GL_MAX_TEXTURE_IMAGE_UNITS` above `FX_GTAO_UNIT`); otherwise, or if
any GTAO program fails to build, the old occlusion stays in use and nothing else in
the chain is affected. The log line `FX|gtao|pass=1|blur=1|light=1|units=16` says which.

Full resolution, not half: the radius is 0.6 of a cell, 30 to 60 pixels from this
camera. A half-resolution pass would put the contact line under a tank on two-pixel
blocks and need a depth-aware upsample pass to restore it.

## Dials (F5, LIGHTING tab, group "5 AMBIENT OCCLUSION")

The panel shows the rows of the method in use: under GTAO the old `radius`, `bias`,
`contrast` and `samples` rows are hidden, under the old method the `GTAO ...` rows are.
`intensity` and the three class weights serve both.

| key | default | meaning |
|---|---|---|
| `ao_mode` | 1 | 0 = the old hemisphere SSAO, exactly; 1 = GTAO. ONE setting for all theaters (not part of a theater's lighting profile), shown on the LIGHTING tab beside its dials. `ssao_on` still switches occlusion off entirely. |
| `gtao_radius` | 0.60 | how far an occluder counts, world cells |
| `gtao_strength` | 2.25 | exponent on visibility, times the weights above |
| `gtao_slices` | 3 | screen directions per pixel (1 to 4) |
| `gtao_steps` | 4 | depth taps per side per direction (2 to 8) |
| `gtao_falloff` | 0.615 | share of the radius over which a distant occluder fades |
| `gtao_blur` | 2.0 | depth-aware blur sigma in pixels; 0 shows the raw pass |
| `gtao_bounce` | 1.0 | multi-bounce fill from the surface's own colour |
| `gtao_direct` | 0.30 | share of the direct sun the occlusion also takes. 0 is physical; 0.3 keeps contact shading in sunlit corners, which matters most on temperate structures, whose saved shade dial (`st_shade` 1.875) leaves them almost no sky light for occlusion to take |
| `gtao_vehicle` | 0.37 | GTAO's own weight on vehicles |
| `gtao_structure` | 0.67 | GTAO's own weight on structures |
| `gtao_foliage` | 0.23 | GTAO's own weight on canopies |
| `gtao_props` | 0.33 | GTAO's own weight on every other modelled object |
| `gtao_sky` | 1.0 | 1 = only what rises above the pixel's horizon occludes; 0 = whole hemisphere |

Every key except `ao_mode` is saved per theater with the rest of the group. A saved file
without these keys loads them at these defaults (checked: the player's saved file, then
`gfxsave`, writes them at the defaults; a change to `gtao_strength` reaches only the
current theater; `ao_mode 0` is written once, at the top level).

## Tuning

The first version matched the whole-frame mean only, and hulls came out 10 to 25%
darker than with the old pass while temperate structures lost their contact shading:
ground fills most of the frame and hides both. This version was tuned class by class.
Classes come from the light pass's own class flags (a diagnostic build painted them);
each number is the class's mean luminance change against no occlusion at all, median
over the 17 pictures of the scene set, with the player's saved settings:

| class | old pass | first GTAO | this GTAO |
|---|---|---|---|
| vehicles | -5.5% | -19.1% | -6.2% |
| structures | -2.2% | -1.4% | -2.5% |
| canopies | -1.5% | -5.8% | -1.9% |
| props, soldiers, ore, indoor walls | -1.5% | -6.2% | -2.0% |
| ground | -0.1% | -0.8% | -0.6% |

Spots checked against the review's regions (grey levels; none / old pass / first GTAO
/ this): td_forest dark tank in shade 42.4 / 39.1 / 29.9 / 38.8; APC in shade 45.2 /
43.5 / 34.8 / 40.9; sunlit APC roof 105.1 / 103.4 / 93.6 / 100.1; td_base_close
harvester 44.4 / 41.9 / 36.9 / 40.9; Hand of Nod pad strip 76.9 / 73.0 / 76.2 / 74.0.

Where the balance still differs from the old pass: structures in the desert scenes
(td_desert_base, td_nuke) darken 4 to 5% against the old 2%, because the desert's
saved structure shade dial gives them a strong sky for occlusion to take; vehicles in
td_base and ra_snow darken 7.5% and 5.3% against 1.9% and 2.0%, while the vehicles of
td_wreck darken 3.2 to 3.5% against 5.1 to 5.5%. `gtao_structure` and `gtao_vehicle` are per theater
for exactly this.

Whole-frame mean luminance, old pass to this GTAO, sidebar included: ra_ant -0.3,
ra_interior -0.2, ra_skirmish -0.2, ra_snow -0.6, ra_snow_close -0.3, ra_temperate -0.3,
td_base -0.3, td_base_close -0.3, td_coast -0.2, td_desert -0.3, td_desert_base -0.4,
td_forest -0.3, td_nuke -0.4 and -0.5, td_winter 0.0, td_wreck -0.3 and -0.3.

## Verification

All scenes of `tools/visual/modern-shots.py`, 1600x900, the player's saved settings.

- `ao_mode 0`: 17 of 17 pictures byte identical to the unmodified build.
- GTAO on, two runs: 17 of 17 pictures byte identical to each other.
- No NaN: no pixel turns black that was more than one level above black before (142
  pixels across five pictures, all at the fog-of-war edge, go from level 1 to 0; the
  first version did the same).
- The Red Alert interior wall-top line: on the review's sample columns (x 420 and 520,
  the top row of the wall top) the first version took the edge from 202 to 187 and from
  160 to 146 grey; this version leaves both at 202 and 160, the old pass's values, and
  the diff map against the old pass no longer shows the line along the wall tops or the
  diagonal wall.
- F5: `gfxpanel 1 ssao_radius` finds no row under GTAO and `gfxpanel 1 gtao_radius` none
  under the old method; the method row is on the LIGHTING tab in both.
- Looked at, before against after: the forest tanks and APCs, the harvester, the
  construction yard and Hand of Nod pads, a td_base truck, desert silos and the
  construction yard, desert cliffs, Red Alert snow barrels and fences, Red Alert
  interior pillars, room floor and wall tops.

## Cost

`framebench 60`, 1600x900, GTAO / old SSAO / none in rotation six times in one process
per scene, on a machine with other renders running (load average 6 to 9):

| scene | GTAO | old SSAO | none | GTAO minus SSAO, paired median |
|---|---|---|---|---|
| td_base_close | 19.5 ms | 18.7 ms | 18.3 ms | +0.7 ms (+0.1 to +1.3) |
| td_forest | 14.6 ms | 13.3 ms | 12.6 ms | +1.3 ms (-1.2 to +3.5) |
| ra_snow_close | 13.1 ms | 12.5 ms | 12.1 ms | +0.8 ms (+0.2 to +1.3) |

So about 1 ms over no occlusion and under 1 ms over the old pass at this size.
Compiling the light program twice adds roughly 10 to 15 ms at load. 4 slices x 8 steps
costs roughly another millisecond. These should be confirmed on an idle machine.

## With linear light (`linear_on`)

GTAO sits on the linear light pass (`linear.md`) as follows. With `linear_on 0` the
FX_GTAO build computes exactly what it did on its own; with `linear_on 1` it draws the
same occlusion, at the same strength, in radiance.

- **The factor is applied as it is.** GTAO's factor was always linear (visibility, the
  multi-bounce lift, the sky's share). In the gamma chain it is applied to the encoded
  colour as `pow(factor, 1/2.2)`; with `uLinOn` 1, `col` is radiance at that point, so it
  is `col *= factor`. It is not wrapped in `fx_lin_gain`, because it is not a value tuned
  against gamma colour. The old hemisphere occlusion (the `#else` side) keeps the
  foundation's `col *= fx_lin_gain(...)`, because its strength was tuned in gamma.
- **The bounce albedo is `vlBase`, clamped to 0..1**, which the light pass has already
  decoded. In the gamma chain it is still `gtao_decode(vlBase)` (the sRGB curve). The
  two differ by the 2.2 power against the piecewise curve, at most 0.009 of albedo.
- **The sky's share needed no change.** Each `gtaoAmb = gtao_share(...)` line is a
  ratio of the pass's own tuned light terms, and the foundation converts the finished
  sum of those terms with `fx_lin_gain` rather than changing them. Where the foundation
  folded the second sun into the primitive-colour branch's gain, the share line moved to
  after that branch's `if (uLinOn == 1) ... else { ... }`, so both paths set it.
- Nothing else moved: the GTAO pass and its blur read only depth and the normal target,
  never the lit image, so their two RGBA8 targets do not ask `fx_hdr_want()`, and nothing
  in the feature pre-exposes.

Measured, 17 pictures at 1600x900, the player's saved settings, `cmp` per file:

| check | result |
|---|---|
| `linear_on 0`, `ao_mode 0` against the unmodified build | 17 of 17 identical |
| `linear_on 0`, GTAO on, against GTAO's own reviewed shots | 17 of 17 identical |
| `ao_mode 0`, linear on, against linear light's own shots | 17 of 17 identical |
| both on, two runs | 17 of 17 identical |

GTAO's own change (both on, against linear light alone) next to its change in the gamma
chain (GTAO against the unmodified build), mean luminance: the ratio of the two is 0.96
to 1.02 in every scene that has occlusion to show (td_winter moves 0.03 of a level
either way), and the per-pixel difference maps correlate 0.85 to 0.98. 0.03% of pixels
or fewer differ by more than 4 levels between the two maps, except td_wreck (0.1%),
where the barrel of a tank under a fire sits behind the flame's linear heat. Whole-frame
mean luminance of both on against the unmodified build stays within 0.7% of what GTAO
alone achieved against it (td_desert_base +0.7%, td_wreck +0.5%; both are linear
light's own change). The dials keep their gamma-chain defaults.

Still true after the port:

- **Texture units.** The GTAO build of the light program declares 15 samplers: uColor 0,
  uDepth 1, uShadow 2, uCloudTex 3, uGNorm 4, uBake 5, uInfShadow 6, uTibField 7,
  uTerrainShadow 8, uTerrainVisibility 9, uTerrainWater 10, uRainWater 12, uVhCards 13,
  uSpecMap 14 and uGtaoTex 15. The base build declares the first 14. Linear light adds
  none to this program. Apple's GL 2.1 reports 16 image units, so unit 11 is the only
  one left in the GTAO build. If that build stops linking it falls back to the old
  occlusion without breaking anything else, but GTAO is then silently off: check the
  `FX|gtao|pass=1|blur=1|light=1|units=16` log line after any merge.
- The light program is built twice. Any edit to the light shader is carried into both
  builds; only lines inside `#ifdef FX_GTAO` / `#ifndef FX_GTAO` differ.
- For TAA: set `g_fxGtaoFrame` to the frame index (taken modulo 64) and lower
  `gtao_blur`, letting the accumulator do the averaging.

## Known gaps

- Canopies still carry some per-card variation in their occlusion; `gtao_foliage` and
  the object shadow fill keep it down.
- The packed eye distance spans 256 cells; beyond that the blur stops being depth
  aware. No current camera gets near it.
- Specular occlusion of the vehicle spec maps' sky sheen is the plain visibility, not
  a cone-based term.
- The non-unified legacy lighting path cannot separate sky from sun, so there GTAO
  darkens all of the light. The shipped settings use unified lighting.
- `gtao_direct 0.3` is an artistic choice, not physics: a sunlit crease loses a little
  sunlight. Setting it to 0 makes occlusion strictly sky only, and temperate structures
  then show almost none (their sky light is set very low by their own shade dial).
