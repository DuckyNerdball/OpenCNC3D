# Lit, soft smoke (`smoke_lit`)

## What it does

The remastered Enhanced smoke and dust particles used to be flat cards: one colour
across a whole puff, unlit, and a hard straight edge wherever a card standing upright
cut into the ground or a building behind it. They are now drawn with the same art,
colours, alpha law, sizes and order, through a small shader (`effects_mod.h`, block
`LIT SOFT SMOKE`) that adds two things:

1. **Lit by the sun, lump by lump.** Each card is shaded as the ball it depicts: a sphere
   normal is rebuilt from where a fragment lies on the card (the billboard basis the pass
   already draws with) and the sun lights it through a wrapped diffuse term over the
   sky's ambient. On top of that, each fragment compares the art's density a short way
   toward the sun with the density the same distance away from it (two taps each way):
   the sun side of every lump comes out lit and its lee falls into shade, so a puff reads
   as a cluster of lumps lit from one side rather than a flat stamp. A second sun, where
   the theater has one, lights the ball in its share of the direct light.
2. **Soft against the scene.** Each puff is treated as a ball in depth as well. At every
   fragment the ball spans a chord along the view, and alpha is scaled by the part of
   that chord in front of the scene (read from a copy of the scene's depth). Where a card
   meets the ground or a wall the puff is at half strength and fades smoothly either side,
   instead of ending on a straight line with the far half missing. To let the buried half
   draw, the card's depth (not its place on screen) is moved to the front of its ball.

**The brightness is kept, not re-decided.** The light pass that follows relights every
pixel of the scene target by the surface behind it, smoke included, so smoke over sunlit
ground already carries the sun's level. The shader applies its shading as a *ratio*
against the mean shading of a puff for the current suns and camera (worked out on the
CPU each frame over the ball, weighted by the ball's depth, then raised by a measured
6 per cent because the art is denser in its sun-facing middle than an even ball). Light
moves from the far side and the lee of each puff to its sun side; the average stays.

**Which particles.** Standing particles of the smoke system (Burn_Smoke, sys 6) and of
every Enhanced sheet that is smoke (`SMOKE`, `LZSMOKE`). The Intensity system (sys 0)
carries smoke puffs, sparks and water drops on one white blob, so its particles are lit
only when their colour is a grey no brighter than 200 and they carry no flash flag.
Particles lying flat on the ground (Red Alert's water foam rings, which wear the
`LZSMOKE` sheet) are decals and keep the flat draw, as do fire, explosions, muzzle
flashes, sparks, embers, water splash and spray, and debris trails.

**Fire light.** The light pass already adds every dynamic point light (burning wrecks,
explosions) to every pixel, smoke included, by where the surface behind it lies. An
earlier version of this shader added a second "fire glow" of its own on top; that lit
smoke beside a fire twice and was removed, along with its dial.

**Why not the experimental lit smoke.** The lit, depth-faded smoke behind the
Experimental switch (`unityvfx_mod.h`) is a different thing: a replay of a Unity
explosion pack that replaces whole explosions with Unity's own particles, drawn into a
float layer with Unity's bloom and tonemap, and only for the anims it claims. It does
not touch the approved smoke and dust art that every burning building, wreck and shell
uses. This feature lights that approved art in place.

## Dials (F5, LIGHTING tab, group "6 DYNAMIC LIGHT")

All three are lighting dials, so they are saved per theater like the rest of that tab.

| key | default | range | meaning |
| --- | --- | --- | --- |
| `smoke_lit` | 1 | 0/1 | master switch. 0 draws the flat cards exactly as before (byte-identical) |
| `smoke_soft` | 1 | 0 to 2 | the ball's depth in puff radii (1 = round). 0 = the flat card and its hard cut |
| `smoke_light_strength` | 1.5 | 0 to 2 | how far the sun and lump shading departs from the flat card. 0 = flat, 1 = the modelled ball, 1.5 = the look chosen on the before/after scenes |

Fixed constants beside the shader, each with its reason in a comment: the ball's size on
the card (0.85), the self-shadow reach (0.36 of a card half-width) and strength (1.8),
the diffuse wrap (0.25) and the reference trim (1.06).

A settings file without these keys (every file saved before this change) loads them at
their defaults, in the live values and in all four theater banks: checked by loading the
player's saved file and writing it back with `gfxsave`.

`efxdump` prints `EFXSMOKELIT|on=|program=|drawn=|share=|sun2=|ref=|selfshadow=` (cards
drawn lit this mission, and the last frame's sky share, second-sun share, reference and
self-shadow weight); stderr says `SMOKELIT|program|ready` once the shader is built.

## How it was verified

All pictures from `tools/visual/modern-shots.py`, 1600x900, the player's saved settings.
This section and the cost below were measured before linear light existed, so they hold
with `linear_on 0`; the linear light section after them repeats the checks with it on.

- **Off is the base build:** all 17 pictures shot with `--gfx "smoke_lit 0"` are
  byte-identical to the base build's pictures (17 of 17, `cmp`).
- **Neutral shader is the flat draw:** with `smoke_light_strength 0` and `smoke_soft 0`
  the lit program reproduces the flat draw exactly: 8 of 8 smoke pictures (td_wreck,
  td_nuke, td_coast, td_base, td_base_close, ra_ant) byte-identical to the base build.
  So everything on screen comes from the shading and the ball, not from the program swap
  or the draw order.
- **Deterministic:** two full runs with the feature on, 17 of 17 pictures byte-identical.
- **Water left alone:** three Red Alert water shells (efxanim 70, 71, 72 in SCA01EA)
  shot 5, 14 and 25 ticks after firing, feature on against off: 3 of 3 pictures byte-identical, with
  and without the soft ball. The foam rings and the spray drops keep their flat look.
- **Brightness kept:** whole-picture mean luminance moves by less than 0.02 of a level in
  every scene. Over the pixels the feature changes, the mean moves between -0.3 and +0.6
  of a level out of 255, while the mean size of the change there is 1.4 to 4.5 levels
  (that is the shape being drawn).
- **Visible:** pixels changed by more than 8 levels: 6340 in td_wreck_1, 1225 in
  td_coast, 774 in td_wreck_2, 476 in td_base_close, 358 in td_nuke_2. Largest single
  changes about 50 levels, at puffs that used to be cut off by a building.
- **Looked at:** 3x before/after crops of every scene with smoke, and close-ups of the
  burning wreck (camera distance 800) and the nuclear plant's smoke column (1400). Grey
  smoke and dust puffs show lit tops and shaded lower lumps; the column no longer ends on
  the straight silhouette of the cooling tower behind it but passes over it softly.
- Pictures unchanged with the feature on: td_winter, ra_snow, ra_snow_close, ra_interior,
  ra_skirmish (no lit smoke in shot).

## Cost

`framebench 60` in one process, the master dial switched on and off four times, on the
td_nuke smoke column (about 550 lit cards) and the td_wreck fire: td_nuke medians
12.9, 12.4, 13.4, 15.5 ms on against 12.4, 13.8, 14.5, 15.8 off; td_wreck 16.4, 15.4,
13.7, 15.4 on against 15.7, 14.2, 14.1, 16.6 off. No consistent sign: the cost is below
the noise of about 1 ms at 1600x900. The work added is one copy of the scene's depth per
frame (only when a lit particle draws), five texture reads per smoke fragment instead of
one, and a few dozen arithmetic operations. At a Retina resolution the depth copy is
four times larger; that was not measured.

## On linear light (`linear_on`, see `linear.md`)

**What changed: nothing in the smoke.** The smoke draws into the forward scene target
before the light pass, and with linear light that target stays eight bit and display
encoded; the light pass decodes it once at its start. So the shader decodes the art
times vertex colour with `objectDecode` (the sRGB transfer function the effects grade
compiles in), shades in linear light, encodes back with `objectEncode` and applies the
effects grade last, exactly as before, with linear light on or off. It adds no light of
its own, pre-exposes nothing and reads neither the lit target nor any float target, so
the contract's decode, `fx_lin_gain` and `fx_hdr_want()` rules have nothing to act on
here. The light pass's `fx_lin_gain` scales the decoded smoke exactly as the tuned factor
scaled the encoded smoke, so the shading reaches the screen at the same size below the
film curve's shoulder. The defaults are unchanged.

**Verified** (17 pictures each, `cmp`):

- `linear_on 0`, `smoke_lit 0` against the base build: 17 of 17 identical.
- `linear_on 0`, smoke on, against this feature's reviewed pictures without linear
  light: 17 of 17 identical.
- `smoke_lit 0`, linear on, against linear light's own pictures: 17 of 17 identical.
- Both on, two runs: 17 of 17 identical. Against linear light alone, the same 12 scenes
  change as without linear light (td_winter, ra_snow, ra_snow_close, ra_interior,
  ra_skirmish have no lit smoke in shot).
- **Same character, same size.** Per scene, the luminance change this feature makes on
  top of linear light against the change it makes without it: correlation 0.94 to 0.99
  over the pixels, and a size of 91 to 101 per cent (td_wreck_1 101, td_coast 99,
  td_nuke_2 96, td_base_close 96). Pixels changed by more than 8 levels: td_wreck_1 8077
  (8402 without linear light), td_coast 1453 (1615), td_wreck_2 907 (1066), td_base_close
  667 (749), td_nuke_2 463 (467). Whole-picture mean luminance still moves by under 0.02
  of a level. The small loss is the film curve's shoulder and rounding.
- Both on against the base build: every scene's mean luminance is within 0.7 per cent of
  what the feature alone gave against the base build; all of that gap is linear light's
  own change, which the smoke leaves as it was.
- Looked at: before, smoke alone, linear alone, and both, at 3x, over the td_wreck_1
  fire, the td_coast beach dust, the td_nuke_2 column and the td_base_close fire. The lit
  tops, shaded lees and the soft edge of the column over the cooling tower look the same
  with linear light as without.

**Cost with linear light on**: td_wreck, `framebench 60`, smoke on and off four times in
one process: 14.8, 14.3, 14.0, 15.2 ms on against 14.3, 14.4, 15.5, 15.7 off. No
consistent sign, inside the noise of a shared machine.

**Texture units.** The light program gains nothing from this feature. The smoke's own
program declares two samplers: `uTex` on unit 0 (the particle sheet) and `uDepth` on
unit 1 (the copy of the scene's depth, bound only while `smoke_soft` is above 0 and
unbound at the end of the particle pass). The effects grade it compiles in declares no
sampler.

If the forward target ever becomes linear (float), make the final `objectEncode` the
identity and keep `objectDecode` on the art. If the effects grade moves into linear, move
it the same way here.

## Known gaps

- **Black smoke stays black.** A ratio of near-black is near-black (the nuclear plant's
  column sits at about 5/255). Lifting the sun's side of black smoke to a dark grey was
  tried: the light pass then printed the shading of the cooling towers behind the column
  onto it, because it relights each smoke pixel as the surface behind it. That relight is
  also why a straight edge stays inside the column in td_nuke_2 (about x 627, y 393 to
  487): it lines up with the left edge of the ruined plant's concrete pad below it, it
  is there with the soft ball at 0 and at 1, and it is most likely where the surface
  behind the smoke changes. Both need
  the light pass to know where smoke is (a smoke coverage mask), which this feature does
  not add.
- **No shadow at the puff itself.** The sun's shadow map is rendered later in the chain
  than the effects draw, so a puff is darkened by the shadow of the surface behind it
  (the light pass does that), not by its own position.
- **The soft ball changes what covers what.** A big puff whose card used to be hidden by
  a building just behind it now shows its front over that building, softly. That is the
  right order for smoke rising in front of a building (the td_nuke column over the
  cooling towers) but it means smoke can also creep over a wall it hugs.
- **Intensity system puffs are chosen by colour** (a grey up to 200, no flash flag);
  launcher and gun puffs spawned with the muzzle flag stay flat.
- With the feature on, a slot that holds both lit and unlit particles draws the unlit
  ones first, for example the Intensity slot, or the `LZSMOKE` slot during a Red Alert
  water shell.
  The neutral check above shows no picture changed by it.
- Not run on Windows. It uses only entry points the Windows build already resolves
  (`glActiveTexture`, the `fx_gl*` shader calls) and GL 1.1 calls (`glCopyTexSubImage2D`,
  `glNormal3f`, a `GL_DEPTH_COMPONENT24` texture).
- The first lit-smoke commit message on this branch said the fire glow default was
  raised in it; it was not (it was already on). The glow is gone now in any case.
