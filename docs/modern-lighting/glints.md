# Flash glints and blast shadows

Part of the modern lighting work. Code: `game/glint_glsl.h` (GLSL helpers and the uniform
upload), one marked block in the light pass of `game/fx_post.h` right after the point-light
loop, three fields on `FxLight` (`blast`, `lift`, `flame`) filled by `fx_gather_lights` in
`game/cnc_eyes.cpp`, one field on `EbldLight` (`state`, the emitter's stage) in
`game/ebld_mod.h`, and ten dials in `game/fx_state.h`.

## What the player sees

- An explosion, a muzzle flash, a burning wreck or a damaged building's fire now puts a
  highlight on what it lights, not only a warm tint. Metal in a specular map (a tank's
  steel, a building's pipes and rims) takes a small hot reflection of the flame; paint,
  wood and stone take a quarter of that; grass and dirt only a faint broad sheen, so the
  ground is not washed out.
- A standing building's own working lights (a refinery's molten pots) do not glint, so an
  undamaged building looks exactly as its art was approved. Once it is damaged or
  destroyed, its fires and sparks glint like any other fire.
- The biggest explosions cast shadows. A tank beside an exploding tank throws a shadow
  away from the blast, a building stops the blast's light on the ground behind it, and
  the wreck's own pieces throw short ones.
- With the master dial off the frame is the one the game drew before, to the byte.

## Dials (F5, LIGHTING tab, group 6 DYNAMIC LIGHT, saved per theater)

| key | default | meaning |
| --- | --- | --- |
| `light_spec` | 1 | master. 0 = the light pass as it was, byte for byte |
| `light_spec_strength` | 1.0 | highlight strength (0..4); 0 keeps the shadows and drops the glints |
| `blast_shadows` | 3 | how many of the brightest explosions at once cast shadows (0..4) |
| `blast_shadow_strength` | 0.75 | how much of a blast's light a shadowed surface loses (0..1) |
| `light_spec_works` | 0 | 1 lets a standing building's working lights glint too |
| `light_spec_paint` | 0.25 | share of the highlight a surface with no metal in a specular map takes (0..1) |
| `light_spec_size` | 0.1 | flame radius as a share of its light's reach (0.02..0.3); capped at 0.15 cells for everything but a blast |
| `blast_shadow_steps` | 12 | march samples toward the blast (4..16) |
| `blast_shadow_near` | 1.0 | cells short of the fireball's heart the march stops (0.25..2) |
| `blast_shadow_thick` | 0.2 | cells a hull, a soldier or a piece of wreck is taken to be deep behind its visible face |
| `blast_shadow_wall` | 0.5 | the same for a building, a wall, a sprite or the ground |

A settings file without these keys loads them at the defaults above, in every theater
bank.

## How it works

- **Highlight.** For each of the 24 point lights that has a flame, a GGX lobe (Smith
  visibility, Schlick Fresnel) on the pixel's own normal, with the material the sun's
  highlight already uses on that class: a specular map's tightness (its Blinn-Phong
  exponent converted to the matching roughness) and weight, averaged over the pixel and
  its four neighbours for this highlight only so a fine pattern does not turn to glitter;
  the relight's roughness and `relight_spec` on a relit cartridge mesh; rock's and
  gravel's own roughness; a fixed material elsewhere (authored hull 0.45 rough, foliage
  0.8 at 0.3 weight, plain ground 0.9 at 0.15 weight, sprites and walls without a normal
  0.6 with the camera-facing card fade the rain uses). Every surface without metal in a
  map takes `light_spec_paint` of the gain. The flame is a sphere
  (representative-point sphere light, energy kept), so it shows in a hull as a blob of its
  own size. Water and self-lit pixels are skipped; nothing shines on the shroud's black.
  The highlight is scaled by `GLINT_GAIN` (16): a light's power is set for the warm spill
  it adds to a surface's colour, the flame itself is drawn near white, and the highlight
  is the flame seen in the surface.
- **Which lights.** `FxLight.flame` is the flame's radius, and 0 puts no highlight: the
  remastered buildings' lights from an emitter of the standing stage (`EbldLight.state`
  0) get 0 unless `light_spec_works` is on. Without this, each refinery's molten pots
  turned a shaded brown panel beside them flat khaki in every theater, all the time.
- **Where the flame is.** An engine anim has no height, so its light sits a quarter cell
  over the ground. The highlight and the shadow come from the heart of the flame instead:
  `FxLight.lift` raises a blast 0.75 cells, a fire 0.2, a muzzle 0.15; the remastered
  buildings' own lights and the Obelisk's beam already have real heights and are not
  raised. The diffuse spill is unchanged and still measured from the light's own point.
- **Shadow.** The first `blast_shadows` explosions in the light list (it is ranked by
  power) march `blast_shadow_steps` steps through the depth buffer from each surface
  turned toward them, as the headlights do, stopping `blast_shadow_near` short of the
  fireball's heart so the wreck or wall the blast sits on does not shadow it. A depth
  buffer holds surfaces, not solids, so each step reads the class at its pixel and takes
  the thing there as `blast_shadow_thick` (hulls, soldiers, wreckage) or
  `blast_shadow_wall` (the rest) deep. A step `e` behind the visible face of a thing `T`
  deep blocks the cone toward the fireball by `min(e, T - e) / w`, `w` being the cone's
  half width at that step: a blocker thinner than the cone is wide never blocks all of
  it, which is why flying debris near a fireball the size of a tank casts a faint shadow
  and a building beside the ground a full one. The shadow comes in over the first few
  degrees a surface turns toward the blast, which keeps a dome's rim from speckling. It
  takes back `blast_shadow_strength` of that light's diffuse (mirrored exactly from the
  point-light loop by `glint_point_diffuse`) and of its highlight. The per-pixel march
  offset is interleaved gradient noise on the pixel position: deterministic, no clock.

## Verified

- Master off (`light_spec 0`): 17 of 17 pictures of the fixed scene set byte-identical to
  the base build's. 22 extra blast moments (below) are byte-identical to the base build's
  too.
- On: 17 of 17 fixed-set pictures and 22 of 22 blast moments identical across two runs.
- On vs before, fixed set: 13 of 17 pictures are unchanged, because no light with a flame
  is in view at their shot moments (the refineries' pots no longer glint). The other four
  (td_base, td_base_close, td_coast, ra_ant) change only near burning wrecks and damaged
  buildings; whole-frame mean luminance moves by at most +0.2 of 255.
- The fixed set catches no explosion at its shot moments (a remastered building's death
  stands down the engine's own blast anims and plays its own effects, which carry no
  point light), so glints and shadows were checked on extra moments in SCG90EA (a Medium
  Tank destroyed beside another tank and beside the Barracks, 1 to 6 ticks after the
  kill) and SCG08EA. On the tank kill, the area the shadows darken by more than 10 levels
  per channel fell by roughly half against the first version (10,472 pixels against
  20,337 six ticks after the kill), while the tank beside it keeps about nine tenths of
  its shadow one tick after the kill.
- The renderer compiles with the Windows cross compiler (syntax check of
  `game/cnc_eyes.cpp`); the GLSL uses only constructs the light pass already uses and has
  been compiled by one driver only.
- Cost, alternating on and off in one process, six pairs of 60 GPU-finished frames at the
  big-fireball moment, on a machine busy with other work: at 1600x900 the pairwise
  differences ran from -0.3 to +1.5 ms, median about +0.4 ms; three tanks destroyed at
  once, the same. At 2560x1440 they ran from -0.5 to +2.7 ms, median about +0.5, mean
  about +0.9. The march and the spec-map averaging scale with the pixels a blast lights,
  so full Retina costs more, roughly 1 to 2 ms during a large blast.

## Known gaps

- Screen-space shadows only know what the camera sees: a march that passes behind
  something nearer the camera can take it for a blocker (darkening some ground beyond a
  wreck, in soft wedges behind its pieces), and a blocker off
  screen casts nothing. The thickness dials trade missed hulls and walls against these
  false hits; flying debris and a tank sitting on the ground look alike to the march.
- The shadow edge carries a fine grain from the per-pixel march offset. The offset is the
  same every frame, so it does not shimmer, but it is fixed to the screen and a temporal
  resolve cannot average it away as it stands (see below).
- A specular map with a fine pattern (the Barracks' camouflage net) still shows its
  pattern as a dappled sheen under a blast behind it, much softer than the pixel glitter
  it was.
- Building deaths in Tiberian Dawn light the scene through the remastered buildings' fire
  and spark lights, which are not explosion class, so they glint but do not cast.
- With linear light off, the point lights' colours are treated as linear for the highlight,
  as it was tuned; with it on they are decoded (below).

## With linear light (linear_on)

The block follows the contract in `linear.md`. With `linear_on 0` it is computed exactly
as before, and with `linear_on 1` it changes in four places:

- The highlight is added to the linear radiance as it is (`col += gSpec`), not through
  `glint_decode` / `glint_encode`, which only the display-colour pass still uses.
- The light's colour is decoded before it is reflected (`fx_lin_decode`), because it is
  a display value: the flame's colour on screen. The glint now shows the flame's own
  orange where the display-colour version showed a paler, whiter tint of it.
- The blast shadow takes the lost light off the display colour and decodes again,
  because the point lights are still summed onto display colour (`fx_lin_add`) and
  `glint_point_diffuse` mirrors that sum unchanged. On the destroyed tank's blast
  moments, where blasts cast, the light the shadows take away is 0.97 to 1.00 of the
  display-colour version's.
- `GLINT_LIN_GAIN` (1.3) raises the highlight in linear only. The decoded colour and the
  film curve's shoulder took about a fifth off it; at 1.3 the summed brightness the
  glints add is 0.98 to 1.02 of the display-colour version on every blast moment of a
  destroyed tank beside a Barracks, tanks and silos, and 0.94 to 1.04 on the standard
  set. More pixels change by a level or two, because a faint lobe on dark ground adds
  more display value in linear.

The pass adds no sampler. The light program declares 14 (units 0 to 10 and 12 to 14):
`uColor` 0, `uDepth` 1, `uShadow` 2, `uCloudTex` 3, `uGNorm` 4, `uBake` 5,
`uInfShadow` 6, `uTibField` 7, `uTerrainShadow` 8, `uTerrainVisibility` 9,
`uTerrainWater` 10, `uRainWater` 12, `uVhCards` 13, `uSpecMap` 14. The glints read
`uDepth`, `uGNorm` and `uSpecMap`.

Verified on the 17-picture set: both masters off is byte-identical to the base build
(17/17); `linear_on 0` is byte-identical to this feature's reviewed shots (17/17), and on
the 22 blast moments 21/22 (the odd one, a single pixel two levels apart on a nuke frame
with no point light, is the linear foundation's own `linear_on 0` picture and shows with
`light_spec 0` too); `light_spec 0` with linear on is byte-identical to the linear
foundation (17/17); two runs with both on are identical (17/17). With both on, every
scene's mean brightness against the base build stays within 0.7% of the display-colour
feature's.

Still open at integration with a temporal resolve: rotate the march offset per frame on
the resolve's own frame index (`gJit = fract(IGN + frame * 0.618034)`), so the history
averages the grain away; keep it a frame index, not a clock, so scripted shots stay
deterministic. The highlight's directions are divided by floored lengths, so a
degenerate case gives a dark pixel rather than a NaN that a float target would carry
into the bloom.
