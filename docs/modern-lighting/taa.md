# Temporal anti-aliasing and full Retina resolution

Feature key: `taa`. Master dial: `aa_mode`. Branch: `visual/modern-taa`, and
`visual/modern-taa-lin` for the version merged onto linear light (`linear.md`).

## What the player sees

- Hull outlines, cliff edges, sandbag walls and tree trunks lose their stair steps. A still
  edge settles on the average of eight sub-pixel samples, which is close to what eight
  times supersampling would draw, at the cost of two full-screen passes.
- Textures (cliff striping, grass, roofs) stop shimmering while the view scrolls.
- With `hidpi` on (off by default), a Retina Mac draws the game at the display's real pixel
  count, two pixels a point each way, instead of a half-resolution picture the system
  magnifies. The HUD, the F5 panel and the pointer keep the physical size they had. It is
  one setting for both games (see the shipped app below).
- The pointer, the selection brackets, the health bars and the placement marks look
  exactly as they do with anti-aliasing off: crisp, still, and whole while they or the
  view move. Fire, explosions, smoke and rain do not smear.

## The dials

All in the F5 panel, ENHANCED tab, group `2b ANTI-ALIASING` (right under SUPERSAMPLING).
They are not lighting rows: edge quality belongs to the display, not to a theater, so
`fx_lighting_param` keeps no per-theater copy of them and no saved theater slot moves.
A settings file that lacks them loads them at their defaults.

| key            | default | range       | meaning |
|----------------|---------|-------------|---------|
| `aa_mode`      | 1       | 0 or 1      | 0 none (the picture is the one the chain drew, byte for byte); 1 temporal |
| `taa_feedback` | 0.90    | 0.50..0.97  | how much of the previous frames each new frame keeps (about ten frames at 0.90) |
| `taa_sharpen`  | 0.25    | 0..1        | unsharp mask on the blended picture; never fed back into the history |
| `taa_reactive` | 0.85    | 0..1        | how far fire, explosions, smoke and rain take the current frame instead of the blend (0 blends them like everything else) |
| `hidpi`        | 0       | 0 or 1      | ask for the display's real backing; read when the window is created, so a change applies at the next start. One setting for both games, kept in Tiberian Dawn's `opencnc3d-fx.cfg`. Off by default because it is the most expensive single setting: a borderless window on a 3008x1692-point display is drawn at 6016x3384 with it on, and measured there with every master on Tiberian Dawn's base ran at about 15 fps against 29 off, Red Alert's snow mission 18 against 45, a Red Alert skirmish 23 against 55 (scripted runs that wait for the blend, so low; with the blend off, 18 against 68, 22 against 89, 28 against 112) |

Command line: `--hidpi` forces the real backing on, `--nohidpi` forces it off, over the dial.
An automated run (a script, a shot, any hidden window) keeps the 1:1 window whatever the
dial says, unless it asks with `--hidpi` or a `gfx hidpi 1` line in its own script.

## How it works

### Temporal anti-aliasing (`game/fx_post.h`, block "TEMPORAL ANTI-ALIASING")

1. **Jitter.** `set_camera` calls `fx_taa_jitter_projection()` right after it loads the
   identity on the projection stack. While the chain runs and `aa_mode` is 1, that adds a
   translate of a Halton (2,3) offset (eight positions, half a native pixel either way).
   A translate placed before the frustum or ortho moves the clip position by (dx, dy)
   times w, which is a shift in normalised device units for the perspective and the
   orthographic camera alike. The index is a count of frames the blend has run, so the
   sequence is the same on every run of a script.
2. **Matrices.** `fx_capture_camera` reads the jittered view-projection back as before (so
   every screen-space pass reconstructs world positions from the depth it was drawn
   with) and hands it to `fx_taa_capture`, which removes the jitter with one row
   operation and keeps this frame's and last frame's un-jittered matrices.
3. **The pass.** `fx_taa_apply()` runs in `fx_world_end` after the resolve and after the
   layer the host lays over it (`g_fxAfterResolve`), before the CRT and the HUD, on
   `g_fxNativeA` in place:
   - reprojection from this frame's NDC to last frame's (both without jitter), using the
     nearest depth in the 3x3 neighbourhood so an edge takes the motion of what is in
     front;
   - history fetched with a five-tap Catmull-Rom (exact at a texel centre, so a still
     camera reads its history back unchanged);
   - the history is clipped toward the centre of a variance box in YCoCg (mean plus or
     minus 1.25 standard deviations, inside the neighbourhood's min/max) so a colour that
     is no longer near the pixel cannot linger as a ghost;
   - blended with weight `taa_feedback`, reduced by up to a quarter when the view moves two
     pixels a frame or more, because a history that is resampled every frame softens;
   - written to a 16-bit float history target (falls back to eight bit, once, if a driver
     refuses), then copied through the sharpen into a second view target that is swapped
     in for `g_fxNativeA`, the way the Unity explosion's composite already does, so no
     pass writes a target an earlier pass of the frame is still reading.
4. **The reactive mask.** The world pass copies its scene target three times
   (`fx_taa_snapshot`, `glCopyTexSubImage2D`, OpenGL 1.1): before the effects, after the
   effects and the rain, and before the overlays (the order marks, the rally marks, the
   selection, the 3D pointer, the health bars, the pips, the wrenches, the power feedback
   and the placement ghost). A small pass at the start of `fx_taa_apply` turns the
   differences into a mask at scene resolution: R where the overlays changed the picture
   at all, G how solidly, B how strongly the effects did. The blend widens it by a pixel
   and takes the current frame:
   - fully where an overlay covers solidly (G);
   - fully wherever an overlay or an effect arrived or left since last frame, found by
     comparing this frame's coverage with last frame's, which the history keeps in its
     alpha and the reprojection carries like its colour;
   - by `taa_reactive` where an effect lit the pixel (B).
   The overlays between `fx_taa_overlays_begin` and `fx_taa_overlays_end` are drawn with the
   jitter's translate taken back off the projection, so taking them from a single frame
   shows them exactly as still as they are with the blend off. They stay depth tested
   against the jittered world; the difference is under a pixel.
5. **Resets.** The history starts over on the first frame of a mission
   (`fx_shutdown` frees it), on a resize, on any frame drawn without the blend (chain off,
   `aa_mode` 0), and per pixel wherever the reprojection falls off last frame's screen.

Why on the finished view: its values are display referred, between 0 and 1, which is
where the neighbourhood clip behaves without brightness weighting; everything the world
draws is in it; the HUD is drawn after it and is never jittered or blended; and the hook
is a single call outside the resolve shader, so it does not collide with changes to the
resolve.

**Picking is never jittered.** Clicks, the edge scroll and every world-to-screen question
go through the renderer's hand-written `project_point` / `screen_to_world`, which never
read the GL matrix. The only readers of the GL matrix during the world pass are the
chain's captures (consistent with the jittered depth), `gls_clip` culling (a sub-pixel
difference) and `bibenh_rect`, which pads its box by two pixels.

**Scripted stills.** A script's `shot` verb and `--shot` draw `fx_taa_warm_frames()`
frames first (38 at the default weight, enough that the first frame's share is under two
percent), with the engine not ticking, so a still is the settled picture a player sees
whenever the camera stops. With `aa_mode` 0 the count is 0 and exactly one frame is drawn,
as before. `fx_taa_settle_begin` starts each still from an empty history and jitter index
0, so a still depends only on its own frames and not on how many the run drew before it,
and while the settle frames run (`g_fxTaaSettling`) the per-frame housekeeping waits:
the mode switch's hidden warm-up and the background ground build (`gmw_frame`) and the FPS
counter's clock (`fxp_draw_stats`). Without that, a `--shot` (which is not on the
automated list) ran the hidden warm-up during the settle frames, came back through
Enhanced's own mode rows and kept a frame with the 640x480 HUD, the Enhanced terrain art
and bilinear filtering that the command line never asked for (G36b went red), and a still
with the counter on showed whatever the counter measured during the settle. The script verb `taawarm 0` skips the settling, so a script can draw frames
while moving the camera or stepping `tickalpha` and shoot the blend mid-motion.

### Full resolution (`game/cnc_eyes.cpp`, `app/opencnc3d.cpp`)

- `cnc_eyes`: a player's run (a visible window, no script, no shot) gets
  `SDL_WINDOW_ALLOW_HIGHDPI` when the Enhanced chain is configured (a `--gfx` preset) and
  the dial says so; the dial is also read from the `gfxload FILE` lines of a script, in
  order, because the backing is fixed when the window is created. An automated run keeps
  its 1:1 window, as the app's automated runs do, so a gate's picture never changes size
  with the display the machine happens to drive; it gets 2x only with `--hidpi` or a
  `gfx hidpi 1` line in its own script. A bare renderer run with no preset keeps 1:1.
  The run prints `HIDPI|asked=..|window WxH|drawable WxH` once.
- The shipped app (`app/opencnc3d.cpp`) asks `game_hidpi_wanted(display_cfg, shared, &from)`
  (`cnc_game.h`) before `SDL_CreateWindow` and applies the answer in a player's run only;
  automated app runs keep the 1:1 window their pictures were measured on. Every run prints
  `DISPLAY|hidpi|wanted=W|applied=A|from=FILE`, so a hidden run shows the decision. It
  reads the dial alone and not the mode: the app's one window serves Classic, N64 and
  Enhanced, which the player switches between live, so whether the chain is on when the
  window is made says nothing about the rest of the run. `cnc_eyes`'s own test for a run
  with no chain is about measuring tools, which count pixels on a 1:1 window.
- One setting for both games. Red Alert's window is made from Tiberian Dawn's
  `opencnc3d-fx.cfg` whenever that file exists (`display_cfg`, as for the display mode and
  size), even in a run that names Red Alert's own file with `--visualscfg`; without that
  file Red Alert reads its own `ra3d-fx.cfg`. `game_visuals_default_ra` then puts the same
  value on Red Alert's live row over whatever its own file holds, and says so
  (`RA3D|hidpi|live=N|from=FILE`). When Red Alert saves its own remembered file with the
  row changed in that run, and Tiberian Dawn's file does not already give the new value
  (its line, or the default without one), that one line of Tiberian Dawn's file is
  rewritten, or appended, keeping every other byte and the line endings
  (`vis_shared_hidpi_follow`, through a hook every `fx_save` calls;
  `VISCFG|hidpi|wrote|FILE|N`). Tiberian Dawn's own saves need nothing: its file is the
  shared one.
- What the rewrite leaves alone: a run that remembers no file, any save to a path other
  than Red Alert's own remembered `ra3d-fx.cfg` (a `gfxsave` elsewhere, a `--visualscfg`
  naming another file, a performance trace), and a save that leaves the row as it was in
  that run, so a value Tiberian Dawn saved meanwhile from a game running beside it is not
  written back over. It never creates the file. It writes a temporary file and moves it
  over the old one (`MoveFileEx` on Windows, where `rename` does not replace), following a
  link to its file and keeping the file's permissions on the Mac. It refuses, with
  `VISCFG|hidpi|failed|...`, and tries again at the next save, when the file cannot be
  written (read-only, as `fx_save` refuses it), is empty or lacks the `enh_textures` line
  every save writes (how it looks while another game is part way through saving it), or is
  no longer the bytes it read when the move is due.
- Settings saved while the default was on carry a bare `hidpi 1`, the old default written
  out rather than a choice (a row moved off its default carries `# moved`). `fx_load` and
  the reading before the window both take such a line as today's default, as they already
  do for an old bare `vsync 1`, so those files start at 1x; a row turned on now is saved
  with the mark and stays on.
- G251's Retina leg holds, in hidden runs: the window's decision with and without a line,
  with a marked and with an unmarked `hidpi 1`; Red Alert following Tiberian Dawn's file in
  both directions (0 over its own 1, and 1 over its own 0), on the window line, the live
  row and a `gfxsave` read back; the rewrite appending an "on" to an LF file and replacing
  it with "off" in place in a CRLF file; and nothing written to Tiberian Dawn's file by a
  save to another `--visualscfg` file or by a run that remembers nothing. Not gated: the
  refusals and the rule for a value changed by another game during the run, which need
  two games or a file changing at a chosen instant.
- What keeps its size: `g_fxHidpi` (whole drawable pixels per point) is set each frame
  beside the pointer scale (`hidpi_note_backing`). The Enhanced HUD's whole-number zoom
  (`cnc_sidebar.h`) and the F5 panel's scale (`fxp_scale` in `fx_panel.h`) are chosen in
  points and multiplied by it, so a 2x window shows them at the 1x physical size and not,
  through the rounding of the larger number, one step larger. The sprite pointer follows
  the HUD zoom. Health bars, the 3D cursor and the selection marks are world space.
- What was already right: the live loop converts pointer points to drawable pixels
  (`mscaleX/Y`) for the editor, and the edge scroll, the F5 panel and the menus
  (`dosmenu_shell.c`, `enhanced_menu.cpp`) use those scales.
- Windows: SDL ignores `ALLOW_HIGHDPI` unless the process declares itself DPI aware, which
  this build does not, so the dial changes nothing there.

## Verification

Shots: `tools/visual/modern-shots.py`, all 15 scenes (17 pictures), the player's saved
settings, headless, 1600x900. A scripted run is now 1:1 unless asked, so the default set
is the blend at 1x; `--gfx "hidpi 1"` shoots the Retina set (3200x1800).

- **Off switch** (`--gfx "aa_mode 0"`): 17 of 17 pictures byte-identical to the
  unmodified build.
- **Determinism**: two runs of the full set with everything on, 17 of 17 identical. Under
  stress (one scene run eight to twelve times at once on one machine) `ra_skirmish` still
  disagreed in about one run in ten, by one level in a few pixels (see the known gaps).
- **Brightness**: the blend moves mean luminance by -0.1 to -0.3 of 255 in every scene
  (`td_coast` -1.2, the camera settling below). No clipping or crushing change.
- **Edges**: enlarged crops of the MCV in `td_base_close`, the snow shore in
  `ra_snow_close` and the soldiers in `td_wreck`: the stair steps are gone at 1x, and at
  2x the edges are those of a picture with four times the pixels.
- **The pointer**: `td_wreck`, the pointer moved 8 px a frame for ten frames, shot
  mid-motion with `taawarm 0`; then the view scrolled 0.1 cell a frame under a still
  pointer; then a 60 px a frame sweep: in all three the pointer is whole and matches the
  picture with anti-aliasing off (it used to lose its outline and tail, and the sweep
  left faint dots). Selection brackets and health bars stay white and crisp while the
  view scrolls 0.25 cell a frame.
- **Effects**: `td_nuke`, the collapse shot mid-motion: the fire keeps its hot core, and
  the loss of edge contrast in the debris box is 16 to 22 percent, about what the
  anti-aliasing itself takes from a still edge (it was 28 to 33 percent).
- **Picking**: unchanged by this round (the pick path never reads the GL matrix); gate
  G1's `--picktest` passed against the earlier binary at 1x and 2x.

Two scenes, `td_coast` and `td_desert`, also change when the unmodified build simply draws
more frames before its shot (the camera's look-at height settles over the first frames
of those missions; the eye height in the log goes from 11.4 to 11.7 cells on `td_coast`).
The settling shot draws 38 frames, so those two AFTER pictures show a slightly different
camera from the BEFORE ones. A fair BEFORE for them is the unmodified picture after the
same 38 frames.

## Cost

`framebench 60`, `td_base` at dist 3000, a 1600x900 drawable, the blend switched in one
process, measured while the machine carried other renderer test runs (load average about
13): `aa_mode` 1 gave 25.0 and 27.8 ms, `aa_mode` 0 gave 26.9 and 24.9 ms. The blend, its
three scene copies and the mask pass are inside that noise at 1x. A scripted run also
finishes each blend on the card (below), which a player's run does not.

An earlier measurement of full resolution, on a more heavily loaded machine: about 31 to
33 ms at 1600x900 and 41 to 49 ms at 3200x1800, about 40% more frame time, because every
pass of the post chain and every fragment of the world runs on four times the pixels.
Repeat both on an idle machine before quoting them.

## Known gaps

- **Scripted stills are not yet bit-exact under heavy parallel load.** An automated run
  also finishes each snapshot copy on the card (`fx_taa_snapshot`). That did not close
  it: six whole shot runs of ra_skirmish and td_base at once, two games each (twelve
  processes), gave td_base identical in all six and ra_skirmish identical in five, the
  sixth off by one or two levels in 10 pixels at ore nugget edges. Gates that compare two
  runs bit for bit while other runs share the card should shoot with `aa_mode 0` until
  the cause is found. The history of this gap: with the jitter on,
  the same `ra_skirmish` script run eight to twelve times at once disagreed in about one
  run in ten, by one level in a few pixels at the ore field's nugget edges. Traced: with
  the jitter switched off, so every frame fed the blend the same scene, the variation was
  still there (four runs of eight differed), and a GPU finish after the blend removed it
  (eight of eight agreed). An automated run therefore finishes each blend on the card
  (`g_fxTaaSync`); that brought the jittered case from about one run in three to about one
  in ten, and a finish before the blend as well did not lower it further, so the rest
  comes from earlier in the frame. Two whole-set runs side by side agreed on all 17
  pictures. The likely cause is the driver letting work on targets read across frames
  overlap; it was not found. Writing the sharpened view to a swapped target instead of
  back into the one the blend reads did not change the rate on its own.
- A tank shell, a box about two pixels wide, thins under the blend: G245 measures 1 x 2
  against 2 x 5 without it, although bullets are drawn inside the effects window. Why the
  reactive mask does not hold it was not found; G245 runs with aa_mode 0.
- No per-object motion vectors. Units are held by the neighbourhood clip, the overlays
  and the effects by the reactive mask. Building-collapse debris is drawn among the
  objects, not the effects, so it is held only by the clip and is a little softer while
  it flies.
- Where `taa_reactive` takes an effect's pixel from the current frame, the ground showing
  through thin smoke is not anti-aliased there and moves with the jitter.
- The shadow, occlusion and Poisson patterns use fixed orientations, so their grain is the
  same every frame and the blend can only average the jittered samples, not rotate the
  pattern. A per-frame rotation keyed to the TAA frame index would let the blend smooth it.
- Full resolution in the app (`app/opencnc3d.cpp`, off by default, the `hidpi` row turns it
  on) is decided and printed in hidden runs (G251), but a player's visible window at 2x has
  not been opened: every check so far is headless. Still
  to check by hand: the Classic and DOS presentations and the menus, which choose their
  whole-number zoom from drawable pixels and can come out one step larger on a Retina
  window; fullscreen and display-mode switching on a 2x backing; and the frame rate, since
  the render scale cannot go below 1.0 to buy back the four times the pixels. `hidpi 0` in
  either game's F5 panel (next start) turns it off.
- Effects measured in pixels (the occlusion radius, grass and detail density) look
  different at 2x. Automated runs are now 1x by default, so the sibling branches' shots
  are unaffected; a player who turns `hidpi` on sees the 2x version.
- Gates that shoot with a `--gfx` preset now draw 38 settling frames before a still and
  blend it; their baselines need re-taking when this merges. Their size does not change.

## Linear lighting

The port onto linear light (`linear.md`) changed no code in this feature: the merge was
clean and the blend is the one reviewed here. It already sits where the contract allows
temporal accumulation, on the resolved display image: after the film curve, the encode,
the clamp, the grade and the fog of war, and after the layer the host lays over the view.
So it blends values between 0 and 1 that the curve has already rolled off, and needs
neither brightness weighting nor `fx_hdr_want()`. It reads no linear target: its inputs
are the native view (eight bit in both chains), the scene depth, and copies of the scene
target, which stays eight bit and gamma encoded with linear on, so the reactive mask's
thresholds keep their meaning. Its history is float in both chains, as before.

Checked on the merged build, 17 pictures each, byte for byte:

- `linear_on 0`, `aa_mode 0`: 17 of 17 identical to the unmodified build.
- `aa_mode 0` (linear on): 17 of 17 identical to linear light's own pictures.
- `linear_on 0` (anti-aliasing on): 15 of 17 identical to this feature's own pictures.
  `ra_snow` differs in 2 pixels by 1 level and `ra_snow_close` in 6 pixels by up to 4,
  in snow-laden tree foliage, the same on every run. The cause is in the linear branch's
  eight bit path, not in this pass: with anti-aliasing off, the linear branch's own build
  and the unmodified build also disagree on one pixel of `ra_skirmish` at 1280x720 (by 2
  levels), the same on every run, although they agree on all 17 pictures at 1600x900.
  The settling frames sample many sub-pixel positions, so they meet such a rounding
  difference more often than one still frame does.
- Both on: anti-aliasing changes mean luminance by -0.13 to -0.32 of 255 against linear
  light alone (`td_coast` -1.13, its camera settling), against -0.13 to -0.32 (and -1.13)
  in the gamma chain; scene by scene the two agree within 0.01. The mean per-pixel change
  is 5 to 10 percent smaller in linear (1.41 to 3.37 levels against 1.56 to 3.53), because
  the film curve has already softened the brightest edges. Enlarged crops of the MCV and
  the cables in `td_base_close`, the snow trees, the Tiberium field in `td_wreck_1` and
  the desert trees look the same as in the gamma chain: the stair steps go, the fire
  keeps its hot core. The Retina shot (`gfx hidpi 1`, 3200x1800) runs with float targets
  granted and matches the gamma Retina shot in character.
- Determinism: two runs of the set with both on, 17 of 17 identical.

No dial was retuned.

Two things for whoever merges or tunes this:

- With linear on, the supersampling cap in `fx_world_begin` falls from 40 to 20
  megapixels. Full resolution makes the drawable four times larger, so a Retina player
  who raises `ss_scale` reaches the cap sooner (at 3200x1800, 1.75 instead of 2.5; at
  5120x2880, 1.0 instead of 1.5), and the log says so (`FX|ss_cap`). At the default
  `ss_scale` 1 nothing is capped. The cap also does not count this feature's targets
  (three scene copies and the mask, 16 bytes per scene pixel, and the float history).
- If the blend were ever moved before the resolve, onto the linear lit image, it would need
  brightness weighting (weight each sample by 1/(1+luma)) to stop hot pixels flickering
  through the clip, and its history would have to stay float.
