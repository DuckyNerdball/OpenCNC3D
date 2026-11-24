# Ground materials: snow, wet shore, sand and grass

Open ground used to answer the sun as one flat matte surface. With this feature each kind
of ground answers it the way that material does. The approved terrain textures are not
touched: every term is shading on top of them, in the light pass's terrain branch.

Code: `game/ground_material_glsl.h` (the shader function `gm_ground` and its helpers),
`game/ground_material_mod.h` (the uniforms), one call site in `FX_FS_LIGHT_BODY`
(`game/fx_post.h`, the terrain branch, in place of `col *= fx_lin_gain(slopeLight)`), the dial rows in
`game/fx_state.h` (group `12r GROUND MATERIALS (GLOBAL)`), and one line in
`game/fx_panel_policy.h` that puts the group on the LIGHTING tab.

## What the player sees

- **Red Alert snow** goes blue in shade and under cloud while sunlit snow stays white, at
  the brightness it had. The cool shade reaches every ground pixel on a snow map, the
  generated rock and gravel included, so a snowfield that crosses from one ground class to
  the other stays one surface. A hillside's terminator is softer. A sparse sparkle of
  single glints sits on snow in direct sun (never inside a cast shadow or under cloud).
- **Desert sand**: dune faces are lit as a rough surface (flat ground keeps its brightness
  exactly) and a rare grain catches the sun, drawn as a brighter grain of the same sand,
  not as a white speck.
- **Wet shore**: on Red Alert maps, mid to dark warm soil next to the water is darker
  (about 8 levels of 255 on average where the band lies), with a faint gloss, fading
  inland along an irregular line. Dark paint (crevices, the shaded side of a stone) only
  gets darker; it never picks up the sky's reflection.
- **Grass**: unchanged in practice. See Known gaps.
- Red Alert interior, buildings, units and water: unchanged.

Mean brightness per test scene moves by at most 1.3 levels of 255 (ra_snow_close); every
Tiberian Dawn scene and the Red Alert temperate scenes are within 0.2. The snow scenes
gain 3 to 6 in blue and lose about 1 in red: the cool shade.

## How a pixel knows what it is

The normal target says "ground" and nothing finer, so each material is a weight:

- the theater decides what can exist: snow on `*SNOW*`, sand on `DESERT`/`SAND`, grass on
  `*TEMP*`/`WINTER`; Red Alert's interior matches none;
- the ground's own colour before any light decides how much: snow is bright and nearly
  colourless, sand is warm, grass is green (soft thresholds);
- a slope limit keeps cliff faces out of sand and grass, a softer one out of snow;
- the generated rock, gravel and stone materials arrive with a stone weight: they take
  the snow map's sky and the snow wrap, and nothing else (no sand, grass, wet or glints);
  their own dielectric highlight is added afterwards by the caller, as before;
- the wet band is a band of the sea's shore field (`uTerrainWater` red channel), from the
  swash's inland reach (`0.5 - (water_foam_cross + 0.25) / 32`) inland by
  `ground_wet_width` cells, only on warm, level, mid to dark (luminance under about
  0.40), non-grass, non-snow, non-stone ground.

`OPENCNC3D_GROUND_MAT_VIEW=1` paints the weights (snow blue, sand orange, grass green, wet
magenta) for tuning; `=2` paints the shore field; `=3` paints the art's relief, the
albedo's luminance and the stone weight. Off unless set.

## The shading

- **Snow**: wrapped diffuse normalised against level ground (wrap 0.25 x strength); the sky
  term is tinted cool (a clear-sky blue scaled to luminance 1) on all snow-map ground, and
  the sun term is warmed by `(1 - cool) x sky0 / sun0`, which makes level ground in full
  sun exactly neutral.
- **Glints** (snow, and sand at a narrower cone): one hashed mirror per world-space cell,
  tilted up to about 80 degrees, firing when it reflects the sun along the camera's axis.
  The axis and the cell size are per frame, not per pixel: the axis is the ray through
  the middle of the screen, the cell is four times one pixel's footprint at the ground
  plane on that ray, rounded up to a power of two. Both are unchanged by a scroll, so the
  pattern is fixed to the ground and only re-deals when the zoom crosses a step. The gate
  is the sun's disc: `1 - max(sh, cl)` taken without the shadow and cloud strength dials,
  because those set how dark a shadow paints, not how much sun reaches it.
- **Sand**: Oren-Nayar (qualitative form, sigma^2 = 0.3 x strength^2) divided by the same
  response on a level normal under the same eye, times the level Lambert. Grain glints add
  the sand's own decoded albedo, so a glint is a brighter grain.
- **Wet**: albedo `mix(a, a^1.3 x 0.82, 0.75 k)` on the decoded albedo; GGX sun highlight,
  roughness 0.22, F0 0.02, on a normal bent by the art's own bounded relief; the sky's
  Fresnel reflection, `F(NoV) x sky`, where sky is the same sky term the diffuse gets.
  Both reflections are multiplied by a specular occlusion from the painting's darkness
  (`smoothstep(0.015, 0.08, linear luminance)`), and the highlight is gated by the sun's
  disc as the glints are.
- **Grass**: Charlie sheen (roughness 0.45) on the smooth normal, minus the same lobe on
  a level normal (so level fields keep their brightness); translucency
  `albedo x (0.9, 1.25, 0.45) x 0.3 x max(0, -V.L)^2`.

## Dials (FX_PARAMS, LIGHTING tab, defaults)

| key | default | meaning |
|---|---|---|
| `ground_mat_on` | 1 | master; 0 draws exactly the picture without the feature |
| `ground_snow` | 0.5 | snow wrap and cool shade (0..2), snow maps only |
| `ground_sparkle` | 1.0 | snow glints (0..3), snow maps only |
| `ground_wet` | 1.0 | wet shore darkening and gloss (0..2), Red Alert maps with water |
| `ground_wet_width` | 1.0 | wet band width in cells (0.1..4) |
| `ground_sand` | 1.0 | desert sand rough diffuse and its grain glints (0..2), desert maps only |
| `ground_grass` | 1.0 | grass sheen and translucency (0..3), temperate and winter maps |

The group is global, not one of the per-theater lighting banks, and its label says so.
Each dial reaches one material; `ground_wet` and `ground_wet_width` reach every Red Alert
theater with water. A saved settings file without these keys loads them at the defaults
above.

## Verification

- `ground_mat_on 0`: all 17 fixed scene pictures byte-identical to the base build (`cmp`).
- Two runs of the full scene set with the feature on: all 17 byte-identical.
- Snow class seam: on the ra_snow_close cliff top, column x=420, the blue-minus-red change
  is +29 to +34 levels from y 680 to 730 with no step (it used to drop from +33 to 0 at the
  class edge at y 711).
- Wet band, measured as feature on minus `ground_wet 0` on the pixels the band touches:
  ra_snow_close -7.6 luminance (dRGB -8.6, -7.5, -5.6), ra_ant -9.1, ra_snow -8.1,
  ra_skirmish -4.1; 97 percent of touched pixels get darker on the first three. No pixel
  darker than 25 in the base picture rises by more than 20 (there were 1224 on
  ra_snow_close). The ra_temperate band is 61 pixels.
- Glints in shadow, measured against a run with shadows and clouds off: 1 percent of glint
  pixels on ra_snow and ra_snow_close (edge pixels of the penumbra), 0 on td_desert.
- Desert grain glints: on td_desert 10 pixels brighten by more than 60 levels (was 344),
  36 small clusters per frame, warm in colour.
- Glint persistence over a one-cell scroll, each tile aligned on the ground art and then
  allowed the best small slide (alignment on smooth sand and snow is only good to a few
  pixels): desert 100 percent of glint pixels still there (47 percent before this
  change), Red Alert snow 67 percent (45 percent before).
- Close crops looked at: the snow cliff top, the snow river banks, the barrels' shadows,
  the ra_ant river and boulders, desert grain, temperate grass.

## Cost

`framebench 60` at 1600x900, feature on then off, twice, in one process (median ms):
ra_snow_close 18.56 / 18.31 and 18.31 / 17.43; td_desert 16.31 / 15.05 and 17.13 / 17.51;
td_forest 13.19 / 13.18 and 14.23 / 14.16. No difference above the run-to-run noise (about
+-1 ms on a busy machine). The extra work is one function on ground pixels (a per-pixel
branch on the ground class, not a uniform one): a few BRDF terms, one shore-field fetch
on Red Alert maps with water, and, only where a glint is possible, three reconstructions
for the frame's axis and two hashed glint evaluations.

## Under linear light

Ported onto the linear light foundation (`linear_on`, `docs/modern-lighting/linear.md`),
following its contract for the features merged on top. `gm_ground` now takes the pass's
colour in whichever space the pass is in and returns it in that space:

- **Linear light on.** The albedo arrives linear (it is the pass's `vlBase` at this
  point). The step is `albMod * fx_lin_gain(slopeLight) * (light / base) + add`, in
  radiance, with no clip. `slopeLight` is the tuned ground light, so it keeps the
  `fx_lin_gain` conversion the plain branch gives it; the material ratio
  `light / base` is physically defined and multiplies unconverted; the glints, the wet
  highlight and sky reflection, the grass sheen and translucency are radiance and are
  added as they are. Nothing pre-exposes: a glint above white is rolled off by the film
  curve at the end.
- **Linear light off.** Exactly the reviewed gamma step: the ratio applied as its 1/2.2
  power to the gamma colour, the added terms through `gm_dec` and `gm_enc`.
- The colour masks (`lum`, `sat`, `warm`, `green`, the wet band's luminance cap) were
  tuned on gamma values, so they read the gamma-encoded albedo in both modes
  (`fx_lin_encode`, an identity with linear light off). The wet darkening and the
  specular occlusion read the linear albedo, as they did.
- The `OPENCNC3D_GROUND_MAT_VIEW` instruments paint display colours, decoded under linear
  light so they read the same on screen.
- The pale-ground rolloff (`uSlopeWhite`) is kept by the foundation and still arrives
  inside `slopeLight`. No new texture, no new sampler and no render target: the pass
  reads only what it read before.

Verified on the fixed 17-picture scene set, byte compared with `cmp`:

- `linear_on 0`, `ground_mat_on 0`: 17 of 17 identical to the base build.
- `linear_on 0`, feature on: 17 of 17 identical to the reviewed gamma feature.
- `ground_mat_on 0`, linear on: 17 of 17 identical to the linear foundation alone.
- Both on (the defaults), two runs: 17 of 17 identical.

The feature's own change under linear light (both on minus linear alone) against its
change in gamma (gamma feature minus base), in mean luminance of 255: every Tiberian Dawn
scene, ra_ant, ra_temperate and ra_interior move by the same amount to 0.01 level. The
snow scenes are slightly brighter under linear light: ra_snow_close +1.66 against +1.31,
ra_snow +0.66 against +0.46, ra_skirmish +0.80 against +0.53, with the same blue shift
(blue minus red +6.80 against +6.82 on ra_snow_close). The difference is sunlit snow:
the gamma chain clipped the warmed sun's red at the eight bit target, and with a float
target that red is kept. On pixels brighter than 200 before (ra_skirmish, ra_snow,
ra_snow_close) the change is -0.13, -0.80 and +1.23 levels under linear light, against
-0.54, -1.14 and +0.54 in gamma.
Per pixel the two changes agree (correlation 0.98 to 0.99 on the snow scenes, mean
absolute change 0.98 to 1.06 times the gamma one). Glints are the same count and
strength (ra_snow_close 3258 pixels brighter by more than 30 levels, against 3316; mean
excess 41 against 38.5; td_desert 90 against 90), so the float target did not make them
read brighter and no default was retuned. The wet band darkens 2708 pixels on ra_ant by
more than 5 levels, against 2859. Against the base build, every scene's mean luminance
with both on lies within 0.8 percent of what the gamma feature alone did against it, and
that 0.8 is the linear foundation's own change on td_desert_base.

Cost under linear light, `framebench 60` at 1600x900 on ra_snow_close, alternating in
one process (median ms, on / off): 18.89 / 17.55, 16.87 / 17.30, 18.11 / 18.97. No
difference above the noise of a shared machine.

## Known gaps

- **The snow's shade is a look decision still open.** At `ground_snow` 1 the cool sky
  turned shaded snow on every Red Alert snow map from today's warm grey to lilac (ra_snow
  groove RGB 104,95,98 to 108,105,122), cast shadows of barrels and fences went lilac, and
  the wrap lifted the grooves so the lit-to-groove difference fell by about two fifths
  (19.6 to 11.5 levels). The cause is the art: the snow texture is pink-warm, and a blue
  sky multiplied over pink gives lilac, not cold blue. The default is 0.5 as an interim,
  which halves both the tint and the wrap. The fuller answer is to pull the albedo's warm
  cast toward a neutral blue in shadow instead of multiplying it by the sky, and to keep
  the wrap off grooves that are already shaded; that is a choice of look, not a fix.
- **Glints are counted per area of ground, at the 900 lines they were tuned at.** The cell
  and the spot size were set in the picture's own pixels, so a 2x picture of the same view
  (the Retina path, or a 3200x1800 window) dealt about four times as many glints (642 to
  2,530 on ra_snow_close). They are now scaled by `gm_res()` = max(height / 900, 1): the
  same view keeps the same cell on the ground and the same dot size at any height above
  900 lines; at 900 lines and below nothing moved. The spot is still sized from the
  pixel's footprint across, so it is a short dash on the ground rather than a round spot,
  and at 1x the glints are as dense as tuned.

- **Grass is unchanged in practice.** Seen from this camera's height with the sun at its
  back, grass is close to matte, and with the sheen normalised to level ground the field
  keeps its brightness: mean luminance on green pixels moves by 0.3 or less on every test
  scene, and 98 percent of green pixels move by 3 levels or less. The translucency needs a low sun
  in front of the camera; with the sun turned round to the front (`sun_az` + 180) it adds
  0.2 levels. The dial is kept so the term can be raised, but nothing visible is claimed.
- Tiberian Dawn's Enhanced temperate coast puts the whole beach on the water side of the
  painted cut: the sea's own pass draws that sand and the first ground inland is grass, so
  the light pass never sees the beach. The wet band is therefore off wherever Tiberian
  Dawn uses the painted cut (`water_painted_cut_td()`), and no Tiberian Dawn shot in the
  test set shows wet ground. Wet sand there belongs in the sea's land-under-water shading
  (water_mod.h). Wet ground is only seen on Red Alert maps.
- The wet band's gloss is faint with the shipped sun, which sits behind the camera: the
  sun highlight only shows when the sun is in front, and the sky's Fresnel reflection at
  this viewing angle is a few percent. The band reads as darker soil more than as a shine.
- The shaded faces of boulders painted along a bank cannot be told from soil by colour or
  by the painting's relief, so they take the wet darkening (not the reflection). Where the
  shore field and the art disagree (a road or bridge deck at a Red Alert river) the road
  counts as wet soil.
- Red Alert snow glints are less fixed to the ground than desert ones: about a third of
  them change after a one-cell scroll. The frame's axis and cell size were checked
  constant over the scroll, and the world position the pass rebuilds from depth matches
  to about a pixel; what moves the rest was not found. Glints are also denser on the far
  half of the screen, because the cell is sized from the middle of the view.
- Only under unified daylight (`unified_lighting 1`, the shipped setting); the legacy path
  is untouched.
- The snow and sand masks are colour based: a pale grey concrete pad on a snow map can pick
  up some snow response.
