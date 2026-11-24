# Linear light, high range and a film curve

The foundation of the modern lighting work. Enhanced used to light the picture on
gamma encoded colour in eight bits, so anything brighter than white was cut flat: a
headlight pool on snow and the core of an explosion were flat white discs, and the bloom
and the grade only ever saw clipped data. With `linear_on` the light pass works in linear
radiance, writes a float target with room above white, and the resolve maps that to the
screen through a film curve.

## What a player sees

- Headlight pools on dark ground have a hot centre along the beam that fades out to the
  tuned rim (`lamp_core`). The core is held to dark ground: it fades out between display
  luminance 0.10 and 0.35 of the ground's own colour and never reaches anything that is
  not ground. On mid-tone desert sand it had merged a convoy's two beams into one
  near-white blob, and a hull standing in another vehicle's beam was lifted about 22
  percent and washed out; on snow the pool is the tuned disc it was.
- Fires and explosions have a yellow to white core that grades out to orange and red,
  and dim embers stay saturated red. Where a flame is bright its particles carry their
  own heat (`emissive_hdr`) past white and into the bloom; the dark wreck, ground or
  smoke seen through a thin flame is left as it was.
- Specular glints and anything else that used to clip now roll off. Pure white pixels in
  the world (every channel 255) drop from up to 0.5% of a frame to none on every scene
  in the before/after set except one burning tank (0.01%).
- Everything else looks as it did: burning buildings throw the same orange light on
  their walls, lamps and pale walls bloom exactly as tuned, and every theater keeps the
  brightness and colour it was tuned to (within 0.8% mean luminance and 0.6 levels per
  channel, numbers below).

## How it works

All of it is in `game/fx_post.h`, plus the mist and dust composite in
`game/atmosphere_glsl.h` / `atmosphere_mod.h`.

1. **Targets.** `fx_hdr_want()` now answers `linear_on`. The lit target, the air volume
   targets and the bloom ping-pong become `GL_RGBA16F`; the scene target stays RGBA8,
   because it holds the gamma encoded picture the fixed-function world pass drew and
   keeping it eight bit keeps every world blend exactly as it was. `g_fxLinLive` is set
   per frame once the lit target is made: asked for AND granted float. A driver that
   refuses float gets the gamma chain, not linear values in eight bits.
2. **Decode.** The light pass decodes the scene colour once, right before
   `vec3 vlBase = col;` (after the terrain grade, the structure and vehicle grades and the
   rain's wetting, which are edits to the surface's own colour and stay in the space they
   were tuned in). The sky and shroud early-out writes the decoded colour too.
3. **Tuned light factors** (sun, sky, shadow tint, the one-sun slope, occlusion, the
   relight ratio, the structure glow, the second sun) are converted to radiance with
   `fx_lin_gain(f) = f^linear_light_k`. At the default 2.2 this is exact: a frame lit only
   by those factors is the frame the gamma chain drew, below the curve's shoulder.
   The relight ratio is written as `fx_lin_gain(lnew) / fx_lin_decode(shade)`, i.e. the
   bake shade is decoded like the colour it divides.
4. **Tuned additions** (point lights, lamp pools, rock and gravel glints, specular map
   lobes and sheen, tree sky fill, rain sheen) go through `fx_lin_add`: added to the
   encoded colour, then decoded, with no clip, so they keep their tuned look and gain
   headroom.
5. **Lights.** The point lights' summed add goes through `fx_lin_add` too. Decoding each
   light's add on its own and adding it in linear was the first version and was wrong:
   an add of 0.2 on a surface at 0.3 then raised it by about 0.05, so a burning dome lost
   nine tenths of its flame light (measured 69 to 50 mean luminance over the fire). The
   vehicles' lamps gain a hot core along the beam axis on dark ground,
   `k *= 1 + lamp_core * dark * a^3` with `a` the position across the cone (0 at the rim,
   1 on the axis) and `dark` = 1 - smoothstep(0.10, 0.35, display luminance of the
   surface's colour) on terrain pixels, 0 on everything else.
6. **Haze** (distance fog, Tiberium fog, rain fog and haze, the mist and dust volume) is
   composited in display space with `fx_lin_over` (and the matching encode/decode in the
   air composite). Measured: laying the same tuned opacity over linear values made a
   temperate base's mist read several times thicker (mean luminance 44 to 60).
7. **Bloom** is cut from the picture it was tuned on. The bright pass applies the exposure
   trim, holds the radiance at `bloom_hot_ceiling` (decoded; 1 = white), adds the
   particles' heat, and encodes before the tuned threshold and knee; the resolve adds
   the halo after its own encode, in display space, as the gamma chain did. Cutting the
   bloom from the raw radiance was the first version and was wrong: values the old chain
   clipped at 1 reached several, so a vehicle lamp's halo on the grass beside it went
   from about 3 levels to about 22, and pale structures wore a glow veil.
8. **Emissive heat.** The explosion and muzzle isolation target (red explosion, green
   muzzle coverage) is drawn float whenever `emissive_hdr` is up, so overlapping additive
   particles keep adding. The bright pass and the resolve both scale those pixels by
   `1 + emissive_hdr * (r + g) * smoothstep(emissive_floor, emissive_floor + 0.4, m)`,
   where `m` is the pixel's brightest channel, encoded (`FX_LIN_EMISSIVE_GLSL`). The
   weight is what keeps the heat on the flame: scaling the whole pixel lifted a burnt
   wreck under a semi-transparent fire from 34 to 43 and tinted it brown.
9. **Resolve order:** emissive heat (linear) -> `linear_exposure` (linear) -> encode,
   unclamped -> bloom add (display) -> the colour grade exactly as tuned (display space)
   -> the shadow toe -> decode -> film curve -> encode -> clamp -> VI gamma -> fog of
   war. The grade
   is kept in display space so its tuned sliders keep their meaning; the curve goes after
   it because this grade's gain and contrast push everything above about 0.7 past white,
   so a curve placed before the grade would have its rolloff clipped flat again.
10. **The curve** (`tone_curve 1`) is the Khronos PBR Neutral shoulder: identity below
    `tone_start` linear, then `1 - d^2 / (x + d - start)` with `d = 1 - start`. Khronos
    starts it at 0.76; the default here is 0.88 (0.94 encoded), because graded RA snow
    sits at about 0.8 linear and a shoulder under it squeezed a headlight pool and the
    snow round it into the same few levels. `tone_hue` mixes between
    applying it per channel (0, default) and the Khronos hue-preserving form (1, exactly
    the published curve). The hue-preserving form was the first default and was rejected
    on the pictures: it drew an over-bright fireball's core salmon pink and kept the
    headlight pool on snow the same pink as the snow. Per channel, a hot colour runs to
    yellow and then white as film shows it. `neutral_toe` scales the curve's flare
    removal step (off: this renderer adds no Fresnel floor for it to remove).
    `tone_curve 2` is AgX (minimal fit, default look), `0` the legacy per-channel rolloff
    in display space.

`fx_lin_decode` / `fx_lin_encode` are a pure 2.2 power, not the piecewise sRGB curve,
because only one shared power makes the factor conversion exact; the sRGB toe would have
darkened every multiplied shadow by up to a fifth.

## Dials (F5, Lighting tab, group "12q LINEAR LIGHT AND FILM CURVE")

| key | default | meaning |
|---|---|---|
| `linear_on` | 1 | master. 0 is the old eight bit gamma chain, byte-identical |
| `tone_curve` | 1 | 1 PBR Neutral shoulder, 2 AgX, 0 legacy rolloff |
| `tone_hue` | 0 | 0 per-channel shoulder (fire goes yellow-white), 1 Khronos PBR Neutral exactly |
| `linear_exposure` | 0 | stops, before the curve; per theater (lighting bank) |
| `linear_light_k` | 2.2 | power turning tuned factors into radiance; 1 = read them as linear irradiance (lifts shadows until retuned) |
| `emissive_hdr` | 1.0 | how much of their own light explosion and muzzle particles add |
| `neutral_toe` | 0 | PBR Neutral flare-removal step |
| `tone_start` | 0.88 | where the film curve's shoulder starts, linear (Khronos: 0.76) |
| `emissive_floor` | 0.35 | encoded brightness below which a pixel under a particle takes no heat |
| `lamp_core` | 1.5 | how much brighter a vehicle lamp is on its beam axis than at its rim |
| `bloom_hot_ceiling` | 1.0 | the most, in display values, anything but the particles' heat brings into the bloom; 1 = the old clip |

A settings file without these keys loads the defaults (the player's saved files were
used for every shot below, and none has them). `linear_exposure` joins the per-theater
lighting bank; no theater needed a trim, so the bank seeds 0 for all four.

## Verification

Tools: `tools/visual/modern-shots.py` (17 pictures over TD Temperate, Desert, Winter and
RA Temperate, Snow, Interior, with the player's saved settings) and
`tools/visual/compare-shots.py`.

- **Off is the old picture:** all 17 pictures with `linear_on 0` are byte-identical to
  the unmodified build.
- **Deterministic:** two runs with linear on gave 17/17 identical pictures.
- **Value pass**, linear on vs the unmodified build (mean luminance, signed delta of
  mean R,G,B in levels):

| scene | before | after | delta | dRGB |
|---|---|---|---|---|
| ra_ant | 39.2 | 39.2 | -0.1% | 0.0 0.0 0.0 |
| ra_interior | 99.4 | 99.4 | +0.0% | 0.0 0.0 0.0 |
| ra_skirmish | 153.7 | 153.7 | +0.0% | 0.0 0.1 0.0 |
| ra_snow | 150.9 | 151.1 | +0.1% | 0.2 0.1 0.2 |
| ra_snow_close | 129.1 | 129.1 | -0.0% | -0.1 -0.1 0.0 |
| ra_temperate | 41.1 | 41.2 | +0.4% | 0.2 0.1 0.1 |
| td_base | 43.7 | 43.6 | -0.3% | -0.1 -0.1 0.0 |
| td_base_close | 46.8 | 46.7 | -0.3% | -0.1 -0.2 0.0 |
| td_coast | 45.2 | 45.1 | -0.1% | -0.1 -0.1 -0.1 |
| td_desert | 84.0 | 84.0 | -0.0% | 0.0 0.0 0.0 |
| td_desert_base | 63.5 | 64.0 | +0.8% | 0.6 0.5 0.2 |
| td_forest | 34.7 | 34.9 | +0.5% | 0.2 0.2 0.1 |
| td_nuke_1 | 69.6 | 69.6 | +0.1% | 0.1 0.0 0.1 |
| td_nuke_2 | 71.3 | 71.3 | +0.0% | 0.0 0.0 0.1 |
| td_winter | 24.2 | 24.2 | -0.0% | 0.0 0.0 0.0 |
| td_wreck_1 | 52.3 | 52.6 | +0.6% | 0.4 0.3 0.1 |
| td_wreck_2 | 51.0 | 51.1 | +0.3% | 0.2 0.2 0.1 |

- **Local checks**, mean luminance over the regions an adversarial review measured on
  the first version (before / first version / now): burning dome in the TD base close-up
  69.1 / 50.5 / 68.4; its lit side 31.3 / 23.7 / 31.1; the burning building left of it
  28.0 / 20.3 / 27.9; the burnt tank under the fire in the wreck shot 33.8 / 42.6 / 35.4;
  the grass beside the RA ant mission's vehicle lamp 24.7 / 43.7 / 25.1 and the smoke
  beside it 25.9 / 57.6 / 26.5; the RA skirmish truck's pool minus the snow round it
  31.2 / 23.5 / 32.3 levels, pool maximum 255 (flat) / 249 / 254 (graded).
- **The heat still acts:** with `emissive_hdr 0` against the default, the burning-tank
  shot changes on 1.3% of the frame by more than two levels, up to 57 levels in the
  flame core.

## Cost

`framebench` in one process, alternating `linear_on` 1 and 0, six rounds of 60
GPU-finished frames each, 1600x900, Apple Silicon, on a machine shared with other render
jobs (load average 10 to 12), so only the alternation means anything:

- TD Temperate base close-up with burning structures (SCG08EA): median of the medians
  23.1 ms on, 23.5 ms off; best 20.6 on, 21.3 off.
- RA snow (SCG03EA): median of the medians 17.6 ms on, 17.1 ms off; best 16.0 on, 16.1
  off.

The difference is inside the noise of that machine (at most about 0.5 ms, 3%, on RA
snow). What it adds: the lit target, the
air targets and the half-resolution bloom ping-pong are twice the bytes (RGBA16F),
about 11.5 MB more written and read per pass at 1600x900; the explosion isolation target
is float and is drawn whenever an explosion or muzzle flash is on screen; about a dozen
`pow` calls per pixel in the light pass and three to six in the resolve. On a mid-range
Windows card, where frame rate was won back by heavy optimisation, this needs measuring
on Windows before it ships; `linear_on 0` removes all of it.

## The contract for the features merged on top

- **Where linear starts:** in `FX_FS_LIGHT_BODY`, `col` is linear radiance from
  `col = fx_lin_decode(col);` (just before `vec3 vlBase = col;`) to the end of the pass
  whenever `uLinOn == 1`. `vlBase` is the decoded surface colour (albedo times the
  cartridge's baked shade for meshes, the flat-lit colour for ground). Above that line
  `col` is the gamma encoded scene colour.
- **Helpers** (`FX_LINEAR_GLSL`, concatenated into the light program after its uniforms,
  so any code in the body may call them; with `uLinOn` 0 each returns its input exactly):
  `fx_lin_decode(c)`, `fx_lin_encode(c)`, `fx_lin_gain(f)` (a tuned factor to radiance),
  `fx_lin_add(colLin, addDisplay)` (a tuned addition), `fx_lin_over(colLin, cDisplay, t)`
  (a haze laid over in display space). Uniforms `uLinOn`, `uLinK`.
- **New physically defined light** (ground bounce, flash glints, PBR materials, light
  shafts' in-scatter) is written in linear: irradiance times linear albedo, added to
  `col`, e.g. `col += vlBase * E`. Decode any authored colour with `fx_lin_decode` first.
  Do not wrap it in `fx_lin_gain`; that is only for values tuned against gamma colour.
- **Existing terms you replace:** if a new term replaces a tuned one and should keep its
  look (for example GTAO replacing the SSAO strength), keep the call site's
  `fx_lin_gain(...)`. If it is physically defined (a GTAO visibility applied to the
  ambient term only), apply it unwrapped, in linear.
- **Exposure:** `linear_exposure` is applied AFTER the light pass, the atmosphere and the
  bloom add (resolve) and before the bloom threshold (bright pass). Nothing in the light
  pass should pre-expose.
- **Bloom:** the bright pass input is linear radiance; it is exposed, held at
  `bloom_hot_ceiling`, given the particles' heat and ENCODED before the tuned threshold,
  so the halo is display referred and the resolve adds it after its encode. A pass that
  wants something to bloom harder (a flash glint, a blast) has to carry it past the
  ceiling the way `fx_lin_emissive` does, or the dial must be raised.
- **Any pass that reads `litSrc` / `g_fxLit`** after the light pass reads linear high range
  values when `g_fxLinLive` is 1, in a float target. A pass that allocates its own target
  to write into the chain before the resolve should ask `fx_rt_init(... | fx_hdr_want())`.
- **The air composite** (`ATMOSPHERE_COMPOSITE_BODY`) encodes the scene, composites the
  mist as tuned and decodes the result. Light shafts that rewrite the mist either keep
  that, or move it to linear and retune the densities.
- **Passes after the resolve** (the Unity VFX layer, CRT, HUD) work on the display
  encoded native target exactly as before; nothing changes for them.
- **TAA:** temporal accumulation belongs on a linear float target (the lit target, after
  the atmosphere) or on the resolved display image; if on the lit target, its history
  must be float too and its clamps should work on encoded or tonemapped values to stop
  bright fireballs leaving trails.

## Known gaps

- `linear_light_k 2.2` reproduces the gamma chain's light; it is a conversion of the tuned
  values, not a retune. Physically, sky plus sun should be summed in linear with their own
  linear energies; that would change the shape of every terminator and penumbra and needs
  the per-theater values retuned by eye. The dial is there to start that work.
- The colour grade still runs in display space between the encode and the curve. A grade
  in linear (or a LUT after the curve) would be the next step, at the cost of retuning
  every theater's grade.
- Fog, mist and haze are composited in display space for look parity, not as in-scattered
  radiance.
- Point lights, lamps and glints are added in display space through `fx_lin_add` to keep
  their tuned look. That is not physical light: two overlapping fires add as encoded
  values do. The materials and bounce work replacing them should write radiance.
- The bloom is cut from display values at the old threshold, for parity. So it still
  catches pale surfaces just under white as the tuned chain did; only the particles'
  heat goes past the old clip. Making bloom catch only what is really bright means
  raising `bloom_threshold` above white and `bloom_hot_ceiling` with it, which changes
  the tuned look and is left to the tuning pass.
- `lamp_core` makes the pool's centre brighter on dark ground (by up to 2.5 times the
  tuned add on the beam axis). Pale ground and every object keep the tuned pool.
- The heat weight (`emissive_floor`) is a brightness test, not the particle's own
  colour: the isolation target holds coverage only. A very dark flame frame takes
  little heat.
- With `tone_start` at 0.88 the curve has 0.12 of range above its start, so a value
  about seven times over white reaches 255 again. In the set no world pixel did except
  in one burning tank's core (0.01% of the frame).
- Gates that classify by exact colour (and G258, which expects `FX|hdr|asked=0|live=0` by
  default) move with linear on and need re-baselining at merge; with `linear_on 0` they
  see the old chain.
