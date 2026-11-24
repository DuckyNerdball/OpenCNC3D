# Light shafts

Dust and mist that know where the shadows are.

## What it does

The Enhanced air volume (`game/atmosphere_glsl.h`, `game/atmosphere_mod.h`) marches twelve
samples per half-resolution pixel through the desert dust or the temperate/snow mist and
integrates extinction and in-scattering. Before this feature every sample was lit by a
fixed air colour, so the medium glowed the same in a building's shadow as in the open.

The rule the feature is built on: **the picture already shows sunlit air.** The ground
colours, the grade and the weather were all tuned with the air between the camera and
the ground taken as lit. So in the open nothing changes, and the feature only adds the
other case, air that the sun does not reach, which only ever gets darker.

- **Every march step asks whether the sun reaches it.** The step's world position is
  projected into the same 4096 sun shadow map, terrain shadow map and cloud deck the light
  pass uses for surfaces. Where the sun is blocked, the sun's share of the air's light
  (`shafts_strength`) is taken away, so a tree, a wall, a cliff or a hill throws its shadow
  through the air as well as onto the ground: dark wedges of dust behind cliffs, the shadow
  volume between a tree clump and its ground shadow, a tree's shadow crossing a sunlit road
  in front of it.
- **The weather (mist, dust)** keeps its own light in the open and loses the sun's share
  of it in shadow.
- **A thin lower haze** carries the shadows where no weather is drawn and fills the gaps
  between wisps where it is. It has no colour and draws no light of its own: its sunlit
  glow is taken to match what lies behind it, so where it is lit it leaves the pixel
  alone, and where it is shadowed only its extinction is left. That darkens by a fraction,
  so hue and saturation stay and the view is never pulled toward a grey. It takes no part
  in local light either: fire, explosion, muzzle and Tiberium light scatter in the weather
  alone, under the weather's own dial, so with the mist off the haze adds no glow.
- **Cloud shadows reach the air only in part** (`shafts_clouds`, a quarter by default). In
  full they darken the air over whole screens at once, offset from their ground shadow by
  the air's height, which reads as patchy dimming rather than as shadow.
- **A Henyey-Greenstein lobe** (`shafts_phase`) makes the air's shadows deeper looking
  toward the sun than away. It scales only the shadowed share, never lit air, and it is
  normalised to its own mean over a 5x5 grid of view rays, so it moves where the shadows
  are deeper, not how dark the view is. The camera's yaw does not turn in play, so a lobe
  on all of the air's light would be one fixed left-to-right ramp on every screen.
- **Deterministic jitter and a wider resolve.** The march start is offset per pixel by
  interleaved gradient noise (a fixed function of the pixel, no time), and the depth-aware
  upsample reads a 4x4 tent of half-resolution texels instead of 2x2, which averages the
  offsets into a smooth result with no banding and no grain.
- Shroud and unseen cells are untouched: the existing visibility field still gates the
  march and the composite.
- Red Alert interiors (`RAINT`) have no sun to make beams with; the feature stands down
  there and the interior is drawn exactly as before. It also stands down when sun shadows
  are off, and at `shafts_strength 0`.

The shafts programs are separate compiles of the same shader sources with
`#define AIR_SHAFTS 1`; when the feature stands down the original programs run, so the
picture is byte-identical to the build without this feature.

The cloud mask is mipmapped, and the march reads it after per-pixel early returns, where
the implicit derivative that picks a mip level is undefined (GLSL 1.20 has no fragment
`texture2DLod`). For the length of the air pass its minification filter is set to
`GL_LINEAR`, so the base level is read and no mip is chosen; the mipmap filter is put back
straight after. The base level is never minified there (512 texels over 20 cells or more,
read into a half-resolution volume).

## Dials (F5, LIGHTING tab, group LIGHT SHAFTS; per theater through the LIGHTING bank)

| key | default | range | meaning |
|---|---|---|---|
| `shafts_on` | 1 | 0/1 | master; 0 draws exactly the air drawn before |
| `shafts_strength` | 0.80 | 0..1 | how much of the air's light goes in shadow; 0 also stands the feature down (identical to off) |
| `shafts_haze` | 1.00 | 0..4 | the lower haze as a multiple of the theater's own; 0 = none |
| `shafts_phase` | 0.20 | 0..0.9 | Henyey-Greenstein g: shadows deeper toward the sun, around the view's average |
| `shafts_clouds` | 0.25 | 0..1 | share of the cloud deck's shadow that reaches the air |

The defaults are the same in every theater on purpose: a settings file that predates these
rows seeds all four LIGHTING banks from the live value (checked: the player's settings file,
which has none of these keys, loaded and re-saved writes `lighting.<theater>.shafts_*` at
these defaults for all four). What differs by theater lives in `shafts_theater_haze()`:

| theater | haze density /cell at the ground | height (cells) |
|---|---|---|
| temperate (TD, RA) | 0.050 | 1.2 |
| winter (TD) | 0.050 | 1.2 |
| snow (RA) | 0.080 | 1.2 |
| desert (TD) | 0.050 | 1.2 |
| interior (RA) | none: the feature stands down | |

Temperate and winter were 0.120. At that density the shadowed haze laid broad dark bands
over ground that was itself in direct sun, with no caster nearby: on td_base_close a band
across the sunlit tiberium field went 5.4 levels darker on average and up to 37, and among
the brightest 40 percent of each picture thousands of pixels lost over a quarter of their
brightness (9,890 on td_base_close, 5,606 on td_wreck_1). It read as wrong ground shadows,
not air, and it was the largest share of the all-on build's overall darkening (about one
level a scene). Every outdoor theater now carries the desert's thin haze; snow keeps
0.080 because its maps showed no phantom bands. A beam still shows where there is weather
to carry it: the mist and the dust lose the sun's share in shadow whatever the haze is. Where there is weather the
haze lives inside the weather's slab, so the weather is marched over the same span in the
same twelve steps as without shafts.

## Verification

Tools: `tools/visual/modern-shots.py` (17 fixed pictures, the player's saved settings,
which have the mist and the dust on), `tools/visual/compare-shots.py`.

- **Off is identical:** all 17 pictures shot with `--gfx "shafts_on 0"` are byte-identical
  to the base build's pictures (17 of 17). `shafts_strength 0` is byte-identical too
  (checked on td_coast, td_desert, ra_snow_close).
- **On is deterministic:** all 17 pictures shot twice are byte-identical run to run.
- **Brightness, colour and contrast kept** (play area, 0..255, before -> after):

| scene | mean before | after | change | saturation | local contrast (6 px) |
|---|---|---|---|---|---|
| td_forest | 34.7 | 34.2 | -0.5 | -0.4% | -0.7% |
| td_base | 43.7 | 42.7 | -1.0 | 0.0% | -1.4% |
| td_base_close | 46.8 | 45.7 | -1.1 | +0.1% | -1.5% |
| td_coast | 45.2 | 44.2 | -1.0 | -0.1% | -1.3% |
| td_desert | 84.0 | 82.4 | -1.6 | +0.2% | -1.1% |
| td_desert_base | 63.5 | 62.6 | -0.9 | -0.1% | -0.6% |
| td_winter | 24.2 | 23.7 | -0.5 | +0.6% | -1.7% |
| td_nuke 1 / 2 | 69.6 / 71.3 | 68.9 / 70.4 | -0.6 / -0.9 | 0.0% | -0.5 / -0.8% |
| td_wreck 1 / 2 | 52.3 / 51.0 | 51.0 / 49.8 | -1.3 / -1.2 | -0.3 / -0.4% | -1.6 / -1.5% |
| ra_temperate | 41.1 | 39.9 | -1.2 | -1.1% | -1.8% |
| ra_ant | 39.2 | 38.3 | -0.9 | -0.8% | -1.5% |
| ra_snow | 150.9 | 149.6 | -1.3 | +0.5% | -0.8% |
| ra_snow_close | 129.1 | 128.0 | -1.1 | +0.6% | -0.7% |
| ra_skirmish | 153.7 | 152.8 | -0.9 | +0.5% | -1.2% |
| ra_interior | 99.4 | 99.4 | identical | | |

  The small contrast losses are the darkening itself (a region made a few percent darker
  has a few percent less local contrast), not a veil: the 5th-percentile brightness holds
  (ra_snow_close 19 -> 19, ra_snow 34 -> 33), and fewer than 1% of pixels anywhere get
  brighter by more than 3 levels (the different march pattern under fire and Tiberium
  light). Half of each view's pixels change by less than 1%.
- **The lobe's ramp:** pictures with the default lobe minus `shafts_phase 0`, mean
  difference in six vertical strips across the play area, stay within 0.3 levels end to
  end (td_coast -0.13 .. +0.06, ra_snow_close -0.32 .. +0.09), and the mean within 0.05.
- **Mist off** (`fog_fx 0`, `sand_fx 0`, the haze alone): nothing gets brighter; the
  Tiberium field in td_base_close reads 26.7/46.4/10.0 against 29.3/50.2/10.8 before (darker
  only where shadows cross it, no green wash). The off path with the mist off is again
  byte-identical to the base build.
- **Looked at:** the desert cliffs throw clean dark wedges into the dust with no grain at 3x;
  Red Alert temperate tree clumps throw their shadow over the road beside them; a tree's
  shadow crosses the sunlit road at the top of td_base_close; snow keeps its colour, its
  barrel and fence shadows keep their depth, and the red soldiers keep their saturation.

## Cost

`framebench` in one process, shafts on and off alternated six times at 1600x900 (wall
clock on a busy machine, so noisy; on-minus-off median of the pairs):

- td_desert_base (dust already marched): +0.48 ms (pairs -2.4 .. +1.4).
- td_forest with the mist on: +0.28 ms (pairs -0.4 .. +0.9).
- td_forest with the mist off (the haze makes the pass run where it did not before):
  +0.69 ms (pairs -4.8 .. +1.3).

So roughly half a millisecond a frame at this size.

## Known gaps

- The effect is deliberately restrained: at the default zoom the camera looks down through
  only a few cells of air, so beams read as shadows that deepen and continue past what
  cast them rather than as bright shafts. `shafts_haze 2` or more shows them more; it
  deepens shadows under tall casters by the same token, but never greys the view.
- The view is about one level darker on average (0.5 to 1.6), all of it in air that is in
  shadow.
- A tree's shadow volume can cross a sunlit surface in front of it as a darker band (the
  strongest is about 30% on the road at the top of td_base_close). That is what a shadow
  through air looks like, and it moves with the view, but in a still picture it can be
  taken for a ground shadow.
- One nearest shadow tap per march step; the soldiers' own shadow map is not read, so 3D
  soldiers cast no shafts (too small to show), and vehicle lamp spots do not light the air
  (as before).
- The per-theater haze densities are constants in `atmosphere_mod.h`, scaled by the
  per-theater `shafts_haze` dial, not dials of their own.
- The cloud uniform arithmetic is repeated from the light pass in `shafts_uniforms()` to
  keep the change self-contained.
- Not run on Windows: no new GL entry points are used (`glActiveTexture`, `glBindTexture`,
  `glTexParameteri` and the uniform setters the air pass already used), and the GLSL is
  `#version 120`.

## Under linear light

Merged onto the linear light pass (`linear_on`, see `linear.md`) as a faithful port: the
feature looks the same with linear light on as it does in the gamma chain, and with
`linear_on 0` it is the reviewed feature to the bit.

- **The composite stays in display space.** The linear branch's air composite encodes the
  linear scene, lays the air over it as tuned and decodes the result (the first of the two
  routes the linear contract offers the shafts). The shafts composite is the same program
  text with `AIR_SHAFTS` defined, so it inherits that encode and decode, and `uLinOn` is
  set on whichever composite runs. The haze's extinction and the weather's shadowed glow
  therefore act on display values exactly as they were tuned, and the per-theater haze
  densities, `shafts_strength` and the other defaults are unchanged.
- **No colour is decoded.** The haze has none, and `uAirColour` is a display-space value in
  a display-space composite; decoding it there would be wrong.
- **The volume is float with linear on** (`g_airVolume` and `g_airResult` already asked
  `fx_hdr_want()`), so a faint haze's transmittance is no longer rounded to 1/255. The faint
  concentric bands in the depth of td_desert's dust shadow, visible in the gamma
  version's difference image, are gone.
- **Nothing pre-exposes:** the air runs before the resolve's `linear_exposure`, as before.

Verification (1600x900, the 17 picture scene set, `cmp`):

- `linear_on 0`, `shafts_on 0` against the base build: 17 of 17 identical.
- `shafts_on 0` (linear on) against the linear branch's ON set: 17 of 17 identical.
- `linear_on 0` (shafts on) against the shafts branch's ON set: 16 of 17 identical. The
  one difference is a single pixel in td_wreck_1, green 12 against 11, in a dark tree
  mass. It comes from the linear branch's light pass with `uLinOn 0`: swapping the base
  build's light pass shader into this tree makes all 17 identical, while swapping the
  base air composite or resolve does not and bloom off keeps it. The linear light pass at
  `uLinOn 0` is the same arithmetic compiled differently, and the shafts' darkening
  moves one pixel across a rounding boundary that the base picture hides.
- Defaults, run twice: 17 of 17 identical.
- The feature's own delta, linear (defaults against the linear ON set) against gamma (the
  shafts ON set against the base build), mean luminance in levels: td_desert -1.44 /
  -1.45, ra_snow -1.19 / -1.18, td_wreck_1 -1.18 / -1.17, td_coast -0.89 / -0.89,
  td_forest -0.39 / -0.40, ra_interior 0 / 0; every scene within 0.02 levels. Per-pixel
  correlation of the two delta maps 0.77 (td_forest, where the delta is mostly rounding)
  to 0.98, saturation change within 0.2%, pixels brightened by more than 3 levels under
  0.8%. Against the base build the views move by the linear branch's own change plus the
  shafts' (within 0.5 levels of the shafts branch's ON in every scene).

Texture units: the shafts add nothing to the light program. `air-volume-shafts` reads 6
samplers (uDepth 1, uVisibility 2, uShaftShadow 4, uShaftTerrainShadow 5, uShaftCloud 6,
uTibField 7); `air-composite-shafts` reads 4 (uScene 0, uDepth 1, uVisibility 2,
uVolume 3).

Left for later, if the air moves to linear radiance (the contract's second route):

- decode `uAirColour` with the sRGB transfer function on the CPU before upload;
- the same `hazeKeep` applied to linear values darkens on screen by that factor to the
  power 1/2.2, so matching today's look would take about 2.2 times the haze density;
- the weather's sun share should take the sun's radiance (`fx_sun_energy()`) and the rest
  the sky's (`fx_sky_energy()`), instead of one fixed colour per medium.

When temporal anti-aliasing lands, add a per-frame offset to `shaftJitter()` (frame index
times the golden ratio, wrapped) so the history integrates the march; the 4x4 resolve can
then shrink back toward 2x2.
