# Ground bounce

The undersides and shaded sides of units, buildings, trees and soldiers take their fill
light from the ground beneath and in front of them. Sand throws a warm orange light up
onto hulls and walls, snow lights everything from below, an indoor floor lifts the walls
standing on it, and water, dark ground or an indoor pit bounce little or nothing. Over
Tiberian Dawn's temperate grass the level stays exactly where it was tuned; only its hue
leans faintly toward the grass.

Before this, the lower half of every ambient was one fixed colour whatever the ground:
a grey a fifth of the sky's level for the relit models and buildings
(`0.6 + 0.4 n.y` in the light pass), a fixed olive for the foliage (`uGndCol` in
`TREE_FS`) and a fixed warm grey, or the sky cube's own lower half, for the 3D soldiers
(`M3_FS`).

## How it works

`game/bounce_mod.h` builds one texture over the whole map, two texels a cell (one on a
map wider than 128 cells). Each cell's colour is the mean of the terrain art actually
drawn there, the one the radar already paints with (`PackCell::avg` for the set on the
ground, Red Alert's own sheet through `ra3d_radar_cell`). Every cell inside the map is
taken as painted, black ones included: an indoor mission's pits are cells whose painting
is black, and they throw back nothing. Only the margin outside the map's rectangle takes
the mean of the painted cells, so the edge does not darken toward nothing. The ground's
colour grade from the light pass (brightness, contrast, snow saturation, biome balance)
is applied, the colours are decoded to linear light, softened over `bounce_spread`
cells, and stored sRGB-encoded again. A copy stays on the CPU for the soldiers. The map
is rebuilt only when the cells' colours, the map size or a dial that colours the ground
changes, found by hashing those once a frame.

What the ground throws back is its colour times the light falling on it (sun on level
ground plus sky on level ground, over the sky part alone, with the cloud over the pixel
taken as the cloud over the ground), divided by the same product for a reference ground,
Tiberian Dawn's temperate grass. Call that T: about 1 on grass, 3.4 to 3.6 on sand
(orange) and snow, under 1 on water and dark ground, 0 in a pit. Every reader then adds
the same light below its horizon:

    lower half = old lower half + strength x 0.2 x grey(sky) x (T - 1)

weighted by how far a surface faces down (all of it facing down, half on a wall, none
facing up). Readers:

- the light pass (`BOUNCE_GLSL` and two call sites in `game/fx_post.h`): the relight of
  cartridge meshes, remastered buildings, vehicles and trees, and the untextured faces;
  per pixel. It reads the map one cell out along the face's level normal, so a wall
  sees the ground in front of it rather than the ground at its own foot (read at the
  foot, the face of a ledge over an indoor pit took half its light from the floor
  behind it).
- the foliage (`TREE_FS` in `game/cnc_eyes.cpp`, set in `game/foliage3d_mod.h`): per
  pixel, under the pixel.
- the 3D soldiers and authored models (`game/model3d_mod.h`): one sample per draw at the
  model's anchor, on the CPU. A model drawn outside the battlefield (the database's 3D
  viewport, `M3Draw::offworld`) takes none.

The light pass's bounce is a SECOND BUILD of the pass (a third with linear light, below),
not a branch in it: everything the bounce adds sits between `#ifdef BOUNCE_LIGHT` and
`#endif`, and `bounce_light_select` builds the pass again with that defined the first
time the bounce is wanted, then points `g_fxPLight` at whichever build the frame draws
with. With the switch off the pass
runs the program `fx_boot` built, whose source after the preprocessor is the pass
exactly as it was, so the off picture cannot move on any driver. The foliage and soldier
shaders keep a uniform branch; their bounce is a separate assignment that shares no
arithmetic with the code beside it, so a branch not taken leaves those values exactly as
they were.

## Dials

On the LIGHTING tab, group "5b GROUND BOUNCE", kept per theater like the sun and sky.

| key | default | meaning |
| --- | --- | --- |
| `bounce_on` | 1 | the switch. 0 draws the picture exactly as before. Needs unified daylight. |
| `bounce_strength` | 0.75 | how much light the ground adds, measured against grass. 0 is the old fixed grey; 0.75 is what linear light gives a shaded wall (below). |
| `bounce_spread` | 0.75 | cells the ground's colour is softened over. Wider washes narrow features (a pit, a road) into the cells around them. |

A settings file without these keys loads them at their defaults (the per-theater banks
seed missing keys from the defaults).

WHY 0.75. A shaded wall sees half sky and half ground, so on linear light its irradiance
is `0.5 + 0.5 x albedo x daylight` in units of the level sky. With the maps' own decoded
ground (the log line below prints both numbers), that wall is about 1.75 times as bright
over Red Alert snow and 1.67 times over Tiberian Dawn sand as over temperate grass, which
on screen (the 2.2 power) is 29% and 26% brighter. The rule above gives a wall those same
lifts at a strength of 0.74 on snow and 0.72 on sand.

The rebuild log line, for checking a map:

    FX|bounce|ground map rebuilt: 64x64 cells (pack cells), 4096 painted, ... mean albedo
    0.808 0.278 0.083 linear, luminance 0.609 sRGB, daylight 2.23 of the sky, T 3.40, ...

## Verified

- Off: all 17 pictures of the fixed before/after set shot with `bounce_on 0` are
  byte-identical to the base build, and the bounce program is not built in those runs.
  An earlier version with a uniform branch in the light pass needed care to stay
  identical (sharing a product with the relight let the compiler regroup it, moving one
  pixel by two levels in each of two explosion shots); the second build removes that
  dependence on the driver.
- On with strength 0: byte-identical to off on td_desert_base and ra_interior, so the
  second build itself moves nothing.
- On: two runs of the whole set are byte-identical to each other.
- Looked at, before against after: the power plant towers and weapons factory front on
  the desert base and the nuke mission turn warm on their shaded faces; snow barrels,
  fence posts and pines lift on their shaded sides; the indoor pillars lift off the light
  floor; the rock face of the ledge over the indoor pit stays where it was (+0.2 of 255
  on a base of 34, against +2.1 before the pits were counted as black and the map was
  read in front of the face). Grass maps are unchanged in level.
- Whole-picture mean luminance moves by at most 0.9 of 255 (ra_interior), 0.7 on the
  desert nuke shots, 0.4 on the desert base, 0.0 to 0.2 elsewhere. On the pixels that
  change, the mean lift is +3.3 to +4.5 levels on desert, snow and indoors, with the
  99th percentile at +10 to +15.
- The ground beside a brightened building changes by a level or two in the full picture.
  That is the bloom spreading the brighter wall: with bloom off, the ground there is
  identical with the bounce on and off.
- The map dump (`OPENCNC3D_BOUNCE_DUMP=<file.png>` writes it on every rebuild) lines up
  with the ground: the desert's rock outcrops, Red Alert snow's rivers and cliffs, the
  indoor mission's pits black.

## Cost

Measured with `framebench 90`, alternating on and off six times in one process at
1600x900, while other heavy work ran on the same machine, so the noise is large:

- td_desert_base: 11.2 ms on, 11.5 ms off (mean of six medians; pairwise differences
  from -2.9 to +0.9 ms).
- ra_snow_close: 18.7 ms on, 18.7 ms off (pairwise from -2.8 to +2.3 ms).

No difference beyond the noise. The GPU work is one bilinear texture read and a few
multiplies per object pixel and per foliage pixel. The CPU work is a hash over the map's
cell colours each frame (49 KB on a 128x128 map), not measured separately, and the
second build of the light pass once per process, logged at 16 to 111 ms.

## Known gaps

- The concrete under buildings (pads and bibs), tiberium, scorch marks and other
  overlays are not in the cell colours, so they do not tint the bounce.
- The ground's light ignores the shadows of the objects standing on it: the ground in a
  building's own shadow bounces as if it were in the sun.
- Soldiers and foliage use the deck's mean cloud cover rather than the cloud over them,
  and read the ground under them rather than in front of each face.
- Grass reads only faintly green, because the temperate art is an olive grey and the
  rule holds its level where it was tuned.
- From the game's high camera, true undersides are rarely in view; the effect shows
  mostly on shaded walls and sides.
- Texture units: the light pass reads the map on unit 11, the foliage program on unit 5.
  A sibling change that also claims unit 11 in the light pass must move one of them.
  The full count is under "With linear light" below.
- A change that edits the light pass edits all its builds at once (they are one source);
  a change that adds a uniform set only once rather than every frame would reach only
  the program current at that moment. Every uniform the pass has today is set every
  frame.

## With linear light

With `linear_on 1` the light pass decodes the scene and lights radiance
(`docs/modern-lighting/linear.md`). The bounce follows that pass's contract:

- THE LIGHT PASS HAS A THIRD BUILD. `BOUNCE_LIGHT` alone is the bounce above, on the
  gamma pass; `BOUNCE_LIGHT` with `BOUNCE_LINEAR` is the version written in radiance.
  `bounce_light_select` picks by `g_fxLinLive`, after the frame has settled it. One
  bounce build with uniform branches on `uLinOn` was tried first: with linear light
  off it moved one pixel by one or two levels in six of the seventeen shots, so the
  linear statements are compiled only into the build that runs them.
- THE GROUND'S LIGHT IS RADIANCE. The map sample is decoded with the exact inverse of
  the sRGB curve it was stored with, and the lower half becomes
  `strength x (decoded albedo x fx_lin_gain(daylight) - reference) x (how far the face
  looks down)`, in units of the sky's radiance. The caller scales it by the sky
  converted with `fx_lin_gain`, as the pass converts every tuned factor, and adds it
  unwrapped: on the relight it joins the linear ratio over the decoded baked shade, on
  the untextured faces it is `col += vlBase x E`.
- THE DAYLIGHT IS CONVERTED, NOT A PLAIN RATIO. The linear pass lights level ground with
  a tuned sky-plus-sun sum raised to `linear_light_k`, so sunlit sand on screen is the
  daylight ratio to that power brighter than the same sand under the sky alone. The
  bounce comes off the ground the player sees, so it uses the same power. Taken as a
  plain ratio, the bounce came out at a third to a half of the gamma build's on the
  desert and indoor shots. At `linear_light_k 1` this is the plain physical form.
- THE REFERENCE STAYS GRASS. The reference is the temperate grass's decoded albedo
  luminance (`BOUNCE_REF_LIN_ALBEDO`, 0.0363, measured over the base mission's map) times
  its daylight of 2 under the same power. So grass maps keep the lower half they were
  tuned with, as in the gamma build, and nothing on them needs retuning.
- THE DEFAULTS DO NOT CHANGE. `bounce_strength` 0.75 gives about the gamma build's
  lift. Over the deepest shade on snow it is a little stronger, and on sunlit faces
  weaker, because in radiance a small addition to a bright face barely shows. That is
  the physical behaviour, and it also drops the faint chroma speckle the gamma ratio
  put on the indoor pillars.
- THE FOLIAGE AND THE SOLDIERS ARE UNCHANGED. `TREE_FS` and `M3_FS` light in their own
  forward shaders and write display colour into the scene target in both modes, which
  the linear pass decodes and returns unchanged. Their share of the bounce is
  therefore the same with linear light on or off.

### Verified with linear light

Over the fixed set of 17 pictures, compared with `cmp`:

- `linear_on 0` and `bounce_on 0`: 17 of 17 byte-identical to the base build.
- `linear_on 0`, bounce on: 17 of 17 byte-identical to the reviewed gamma bounce.
- `bounce_on 0`, linear on: 17 of 17 byte-identical to linear light alone.
- Both on, two runs: 17 of 17 identical to each other.

Both on, against linear light alone, mean luminance in levels of 255 (the gamma
bounce's own change against the base build in brackets): td_nuke_1 +0.56 (+0.64),
td_nuke_2 +0.53 (+0.61), ra_interior +0.49 (+0.76), td_desert_base +0.28 (+0.37),
ra_snow +0.14 (+0.15), td_desert +0.12 (+0.12), ra_snow_close +0.10 (+0.10),
ra_skirmish +0.08 (+0.07). The grass, temperate and winter shots move by less than 0.01.
Against the base build, every scene's mean sits within 0.53 percentage points of
where the gamma bounce put it (largest on td_desert_base: +1.14% against +0.60%, most of
it linear light's own change there).

Texture units, for fitting the lighting features into the 16 that Apple's GL 2.1
reports:

- light pass, base build (`fx_boot`): 14 samplers. 0 uColor, 1 uDepth, 2 uShadow,
  3 uCloudTex, 4 uGNorm, 5 uBake, 6 uInfShadow, 7 uTibField, 8 uTerrainShadow,
  9 uTerrainVisibility, 10 uTerrainWater, 12 uRainWater, 13 uVhCards, 14 uSpecMap.
  Unit 15 is free.
- light pass, either bounce build: those 14 plus 11 uBounceMap, 15 samplers. Linear
  light adds none.
- foliage (`TREE_FS` and its vertex shader): 0 to 3 the sheets, 4 uCrush, 5 uBounceMap,
  6 in all.
- `M3_FS`: the bounce adds no sampler (one uniform colour per draw).
