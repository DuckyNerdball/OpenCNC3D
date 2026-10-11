# OpenCNC3D pixel UI component recipe

Updated 7 October 2026. Applies to C&C's 2D Enhanced Interface Lab.
Start here for its pixel artwork, alongside `docs/ui-interface-recipe.md`.
RA's accepted floating materials and mesh behavior are separate.

## Current review state

`cnc-pixel-tools-v01` was rejected. Its technical tests did not detect its visual
craft failures. Do not reuse its cropped edges, source-column interpolation,
repeated strips or arbitrary holes. Do not call it an accepted baseline.

The user approved the **v03 empty button plate**: “The buttons are good and
approved. The icons are not.” Freeze that exact housing, including its material,
fastener seats and four state frames. Approved sheet SHA-256:
`ca7a096ef05e9633d953b9f8be725d678e9ea78ef8ef2a2597b3da13331d9d65`.
Normal PNG, exact source and visual evidence are preserved in
`art/interface-lab/cnc3d/pixel-tools-v03-grit-parts/`.

The user subsequently approved the darker **v05 backing plate**: “Great. Back
buttons are approved. Now get the icons in and lets see.” Its exact sheet hash is
`91dc9849eaa5e9f9b7270d6150c5a01e9ed7bc4449b132906129f989d59f344e`. The generator pins both the original
v03 shape/wear and the accepted v05 output; preserve these bytes in icon work.

Current `cnc-pixel-tools-v08-orange-states` places the revised double-ended wrench, dollar and
lightning on that shared plate for review. All icons remain unapproved; putting
them in the HUD is explicitly authorized, not an artistic acceptance. Each
19×19 transparent icon is blitted at (8,8) inside the 35×35 plate, at the same
native texel density. A pressed icon moves down one native pixel with the inset.
Separate source layers are retained; no stretched or preflattened replacement.

## Feedback → cause → rejection rule

| User feedback on v01 | Cause | Rule for the next component |
| --- | --- | --- |
| Hard edges and cutoffs midway along edges | Old side fragments repeated to make taller geometry | Author the complete perimeter for the target size; never repeat a cropped edge. Inspect every straight run and corner at native size and 4×. |
| Random bolts / screw holes | Decorative source pixels survived cropping | Fasteners are separate authored parts with explicit seats. Four matching corner seats for this housing; no fasteners implied by texture. |
| Inconsistent shadows | Patches brought their original local lighting | One upper-left light for face, bevel, fasteners and symbols. Lower/right shadows must agree. States cannot flip the light direction. |
| Many identical vertical lines | One source column profile extended through the face | No column interpolation, modulo sampling, replicated grain or noise hash. Draw sparse, finite wear clusters by hand. |
| v02 too flat / simple | Clean face had too little material depth or wear | Retain the continuous housing; add irregular finite grime/worn-metal clusters, paired lit scuffs and dark undersides. Increase face-to-bevel depth while keeping the outer rails uninterrupted. |
| Wrench has a weird shape | Fork/jaw silhouette collapsed into an ambiguous bulb | Start from recognizable anatomy: two jaws around a visibly open mouth, parallel handle, rounded end. Review the plain silhouette at native size before adding highlights. |
| Overall inconsistency | Three precomposed sheets concealed structure | One shared housing, separate symbols and a shared palette/state contract. Review blank structure first. |

## Ordered pixel workflow

1. Open the original C&C Repair/Sell/Map sheets. Use them for metal color, edge
   weight and lighting reference; do not treat their text, fittings or corners as
   a stretchable material. Preserve them unchanged.
2. Define native bounds, one silhouette, bevel width, icon safe area, fastener
   centers and light direction before drawing. For v03: 35×35 housing, two-pixel
   stepped bevel, corner centers (5,5), (29,5), (5,29), (29,29), 3×3 fittings,
   central 19×19 icon canvas. Native HUD placement remains unchanged from v01.
3. Draw one empty housing. Straight rails stay uninterrupted. Chamfers terminate
   at actual corners. Changes in highlight strength must follow face orientation
   or intentional wear, never a crop seam. Keep the interior quiet.
4. Use a compact shared palette. Metal shading is deliberate pixel clusters and
   restrained value bands, with sparse finite strokes. No blur, automatic noise,
   repeated stripe patterns, photographic texture, or resampled geometry.
5. Draw the symbol on transparent pixels in its own file, with a legible silhouette
   and the same upper-left light. Icon shadow belongs to that layer, never baked
   into the blank housing. Do not invent lettering with a generated image.
6. Derive states from the same geometry. Hover increases restrained face/edge
   light. Pressed state changes the inset depth while the outer rim remains lit
   from above-left. Pressed and active states tint the entire housing orange, retaining its wear, bevel shading and fasteners. A thin amber outline is insufficient. Fastener seats never move.
7. Preflight straight-edge continuity, equal corner fittings, lack of long
   high-contrast face stripes, palette/alpha integrity and layer separation.
   Automated checks prevent specific regressions; they do not judge attractiveness.
8. Inspect actual native pixels and nearest 4× views, then the full HUD. Display
   blank housings and separate icons in the Lab component review. Retain the
   vacated row under the minimap. Test interactions without changing game code.
9. Save the version, source hashes, screenshots and user feedback. Explicitly
   record acceptance or rejection; a passed test or old approval is not art
   approval. Adjust the affected rule when new feedback exposes a failure.

## Reproducible implementation

`tools/cnc_interface_lab/build_tool_buttons.py` holds the silhouette, compact
palette, explicit four fastener seats, finite stroke clusters and separate icon
masks. `public/buttons/manifest.json` records the version and layer contract.
No whole-button tiling or copied side-rail fragments are used in v03.
The browser loads the single shared plate for all three tool seats and displays
icons only in the parts review. Integer nearest sampling remains mandatory.

This recipe records the current feedback and trial, not a claim that an unseen icon is
beautiful or approved. It is a local browser study, not game integration.

## v03 material amendment

Quiet does not mean sterile. The face now uses a stronger six-value depth
ramp, eighteen individually drawn wear clusters and six finite scuff pairs.
A wear cluster combines dark recess pixels with a small upper-left exposed-metal
lip. Keep high-contrast details finite; do not copy them into full-height grain.
Chips stay on the inner bevel; outer straight rails remain continuous.
The fasteners and icon layers retain their common light and separate geometry.
The user subsequently approved this exact v03 plate finish; its hash is pinned above.

## Icon-only revision after plate approval

Keep the approved housing pinned by hash during symbol work. Icon canvases stay
19×19 with transparent backgrounds. The v04 wrench has distinct paired jaws,
a readable open mouth, a narrower parallel shaft and rounded heel with a small
hanging hole. It uses the same upper-left light and one-pixel lower-right shadow.
Silhouette recognition comes before texture. Reject an ambiguous blob, bent or
uneven shaft, unequal horn weights, or a shadow that visually closes the jaw.
No approval of the housing transfers to an icon or assembled button.

## v05 tone match against the actual HUD

Compare the new material in place beside the minimap, not only against a dark
component-review backdrop. Inspect the exact original chassis top and lower
radar rails. Record sample regions and encoded RGB brightness distributions;
these are practical pixel comparisons, not physical luminance measurements.

The previous face median was 82.6 versus the surrounding top rail's 64.0.
The v05 face median is about 56, with neutral R/B one code value above G, matching
the original chrome's balance. Compress mids and restrained highlights while
preserving the ordered scuff/grime values; keep the same shape, alpha and light
source. `public/buttons/tone-comparison.json` records exact regions/results.
Do not transfer the plate's tonal change into unapproved icon files or silently
change geometry. The new tone still requires visual review.

## Shared texel density - 7 October 2026

User direction: every element must share the same texel density so components
belong together. The authored basis is **one texture pixel per native HUD unit**.
A 35×35 plate occupies 35×35 units; a 19×19 icon occupies 19×19 units, not a
resized 35×35 surface. Original chrome and full cameos also use this native grid.
Bevels, fasteners, scuffs, outlines and icon highlights are authored on that grid.
A different asset footprint is not a different texel density.

All HUD artwork uses one shared positive integer zoom from the game layout and
nearest sampling, with smoothing disabled. No element-specific enlargement,
fractional CSS transforms, blur or filtered resizing. Sidebar animation snaps
its shared displacement to whole native units; renderer destinations snap to
whole screen pixels. Keep children aligned rather than snapping each to a
different motion grid. Compose source clock masks and miniature content into
their native raster seats before applying the shared HUD zoom.

The manifest declares `nativeSize`, `pixelsPerUnit: 1` and `sampling: nearest`
for every new plate/icon. Browser initialization rejects mismatches. The renderer
rejects fractional art scales. Source images are unmodified; density work has
not changed plate or icon bytes. Review images all use the same shared 4× zoom,
with native-size views alongside them. Review enlargement never becomes the
shipped texture resolution. Runtime and asset checks cover common scales and
reject individually doubled or stretched assets.

## Double-ended wrench correction - 7 October 2026

The user noted that the wrench had its fork at only one end. The required symbol
is a double-ended spanner: two visible open fork heads, connected by a slim shaft.
The previous rounded heel/hanging hole was the wrong design. v07 replaces that
end with a second open jaw while preserving the 19×19 transparent canvas, common
native texel density, light direction and plate seat. Plate, dollar and power
image hashes are unchanged. Icon acceptance remains pending visual review.

## Full orange down state - 7 October 2026

User direction: the Down state must be fully orange, not merely a thin edge.
Both transient pointer-down (frame 2) and latched selected/active (frame 3) use
an orange palette across the entire plate, including face, bevels and fittings.
Map existing shade values into warm orange/brown highlights and shadows rather
than applying a flat fill. Keep icon layers intact and readable above the plate.
Normal and hover pixels remain byte-identical to the approved v05 base.

Pressed state takes precedence over active state so an already selected tool
still visibly depresses by one native pixel when pressed. Release restores the
latched active or normal/hover appearance. Existing tool-mode logic is unchanged.
The generator reconstructs the historical v05 sheet for its hash pin while the
v08 pressed/active frames implement the user's authorized colour amendment.

## Accepted button recipe and category tabs - 7 October 2026

The user accepted the v08 full-orange down state with “Great. Remember how you
did these buttons in your recipe”. Pin the complete current plate sheet SHA-256
`718107317af1eb287e4a253d231bcae180f273ddf9ccbff441b287b48e1692f9`.
Use `tools/cnc_interface_lab/build_tool_buttons.py` to reproduce the housing;
retain the v05 idle/hover pixels, paired upper-left scuff lighting, finite irregular
wear, four intentional corner fasteners, continuous stepped rails and double-ended
spanner. Keep 19×19 symbols separate from their 35×35 housings. Down and active
are fully orange shaded metal; a down symbol shifts exactly one native pixel.
Do not crop source chrome into buttons, tile grain, stretch plates or change
individual component texel density.

The new four category tabs reuse that exact plate with separate native symbols
(structure, soldier, tank, aircraft); new symbols remain subject to visual review.
Seats: x3/43/83/123, y185, 35×35. Fixed two-column, eight-row grid starts y232;
sidebar is 160×621 native units. All categories and factions use these same seats
and fixed power-meter height. Smaller categories retain empty original wells.
No scroll arrows or embedded arrow housings remain. Viewports shorter than621
use a scrollable minimum-height canvas; never compress texels to fit.

Grayscale only the copied unavailable cameo art; preserve source pixels, chrome
and category plates. Hover/focus names missing buildings and tech level; locked
entries cannot queue. Build rules are exported from pinned local TD type tables,
using house.cpp prerequisite equivalences plus each production factory. The Lab
uses multiplayer tech levels and an editable owned-building fixture, not campaign
mission gates or live engine state. Placing a completed structure adds it to the
owned roster and unlocks dependent cameos. Category switching never resets queues.
`core/tabbed.mjs` composes the revised layout independently of the original
compositor, whose eight compiled C golden comparisons must continue to pass.
Actual browser evidence and versioned sources: `art/interface-lab/cnc3d/pixel-tabs-v01/`.

## Unavailable opacity and matching vent footer - 7 October 2026

Unavailable cameo art is grayscale at 50% opacity (8-bit alpha128 from255),
composited over its original dark well. Apply this only to the copied cameo
art; its surrounding frame and bolts remain fully opaque. The cameo raster
path must honor source alpha rather than replacing it with255. Unlocked
cameos retain their original source alpha and colour.

Close the fixed sidebar with the exact original minimap vent housing: source
chassis x0..159, y151..180, moved without scaling to y619..648. Keep its
original lighting, slats, fitted end bolts and lower corner outline together,
without piecing bolts into arbitrary positions or repeating texture stripes.
The fixed eight-row grid and power-meter geometry remain unchanged; only the
end housing extends the sidebar to160×649 native units. Every category uses
this same footer. This replaces the v01 two-pixel bottom rail.

## Gameplay behind open HUD areas - 7 October 2026

Leave the entire y181..230 category seat transparent; each 35×35 plate supplies
its own opaque housing. The battlefield must show through the gaps, including
between tabs and between HUD sections. Do not draw a full-height sidebar
background rectangle: the fixed sidebar ends at the vent footer, and gameplay
continues underneath it. Keep deliberate dark cameo wells and the minimap's
own raster unchanged. The Lab demonstrates compositing over its tactical field;
eventual runtime integration must preserve these open areas and draw gameplay
behind them. No runtime integration is implied by this browser revision.

## Framed tab and cameo housing - 7 October 2026

User now requests a minimap-style top-and-side frame covering the tabs and
cameo section. This supersedes the fully unframed tab strip; the middle stays
open and gameplay still shows outside the housing and below its vent.

Use the complete original minimap top member (chassis y18..37), including its
fitted corner bolts, at y181..200. Mask the six-pixel clipped upper corners so
source-pack black outside the outline is transparent. Continue seven-pixel
side members from y201 to250 using one contiguous source run y38..87; retain
the existing grid chassis below them. No arbitrary bolt placement, repeated
texture strip or stretched bevel. Four unchanged35×35 tab plates sit at
x7/44/81/118, y205, with two-pixel gaps. Each plate fits between the side rails
without overlap. Move the grid and meter down20 native pixels as a section:
first cameo y252, footer y639, fixed total height669. Every category keeps this
same height and layout; texel density and approved button pixels are unchanged.

## Continuous housing, meter enclosure and tab backing - 7 October 2026

Historical v05, rejected by the user for broken corners and joins. The v06
construction below supersedes this geometry and placement.

User's vent-join screenshot is authoritative for the lower outline: vertical
rails flow into the original angled vent corners. Extend the left rail12
native units outward (x-12..-1), so the power meter atx0..17 stays fully inside
the enclosure. The right edge and its dark shadow run atx153..159 through the
entire grid to the lower vent. Preserve the source vent center and lower corner
fittings; move only the left corner assembly to the expanded outer boundary.
Never end either side abruptly at the first cameo row.

Add a dark, gritty gunmetal backing under the tab buttons. Use finite irregular
wear clusters and continuous stepped lighting profiles at native density.
`build_housing.py` reproduces the separate172×488 overlay at(-12,181), verified
with its own source/asset hashes. Composite it over the grid without covering
the meter or cameo images. The fixed full height remains669; grid, tab plate
pixels and native hit seats remain unchanged. Its six-pixel clipped outer tips
are transparent, and gameplay remains visible outside/below the frame.
Sidebar collapse must cover172 units now that its left edge extends outward.

## Source-preserved corners, aligned minimap and continuous vents - 7 October 2026

The user rejected all four v05 corners: synthetic rail profiles and diagonal
cutoff masks clipped the source bevels and produced seams. Treat this as a
construction failure, not something to hide with extra shading. Build corners
and their adjacent joins together from the original minimap chassis. Preserve
each corner's visible material pixels, bolt seat, chamfer and shadow; clear only
the known inner map aperture. Never paint a guessed diagonal over the source.

`build_housing.py` v06 uses a 184×488 lower frame at (-12,205), with full
12-pixel side rails enclosing the power meter. The four protected source crops
are (0,18,24,42), (136,18,160,42), (0,149,24,181), and (136,149,160,181).
Keep 16 original pixels of straight rail beside each corner, and 16 original
pixels of the horizontal members beside them. Extended middle rails use the
measured source lighting profile and finite native wear. Composite transparent
source apertures over the backing; do not erase backing with an RGBA paste.

The minimap frame is 184×187 at (-12,18), matching both outside edges of the
lower frame. Its map window is 160×144. Keep source corner geometry at native
density rather than stretching the entire minimap. Tabs sit at y229, first
cameos y276, lower vent y663; fixed total height693. Collapse travel is184.
All categories retain eight rows, with gameplay visible outside and below.

The user also rejected a vertical material strip interrupting the bottom vent.
Extend the vent slats with their continuous per-row dark grooves and lit lips;
never insert a plain metal post into the groove run. Keep the original corner
assemblies and their adjacent material. The user's “This is great” image is the
approved vent finish/shape reference: gritty metal, slat depth and corner joins.
It approves that reference, not an unseen complete housing revision.
Saved as `art/interface-lab/cnc3d/pixel-tabs-v06-source-corners/approved-vent-reference.png`,
640×128, SHA256 `d37572cc94dd903965221f75a876ec79ae45ebd0ecf458f876e817a4a2384cb3`.

**Mandatory frame QA before presenting future changes:** inspect all four corners
and at least16 native pixels of every adjoining rail at 1× and nearest 4×,
side by side with the original minimap. Check bevel continuity, outside alpha,
bolt placement, the complete right shadow, power-meter enclosure, continuous
vent grooves and matching minimap/sidebar edges. Inspect the rendered Lab too;
asset-only inspection is insufficient. Save actual browser evidence and the
source/raster comparison in the versioned archive. `test_housing.py` checks
protected visible corner pixels, adjacent source runs, shared width/density and
uninterrupted vent grooves. Automated checks supplement visual inspection.

## Shaped alpha and original cameo metal backing - 7 October 2026

User correction after v06: the corner cuts were still opaque; the flat backing
did not match the cameo metal and crossed into bolt corners. The source chassis
sheet itself is opaque outside its diagonal outline. Copying source RGB/A is
therefore insufficient to extract a usable UI frame. v07 separately masks those
exterior packing pixels using measured corner cuts (TL7/TR10/BL9/BR10 native
pixels, including the source's asymmetric dark edge). Preserve RGB in retained
bevels and shadows. Both minimap and lower frame use the same extraction.

The old rectangular aperture also removed metal under the top corner bolts.
The source aperture is x12..146/y31..148, with three-pixel top inner corner cuts.
Protect those seats and the right inner edge at source x147. Apply backing below
the extracted frame, never over the bolts or bevels. Alpha-composite protected
frame material last. Do not equate unchanged source pixels with a correct mask.

Replace the synthetic backing fade/scuff layer with native metal-face patches
from existing cameo junctions: source rectangles x79..88, seven pixels high,
starting at y254/302/349/397/445. Select overlapping3×3 native subpatches with a
fixed seed, excluding empty wells and bright bevel lines. Preserve source
colours and density without repeating a source edge strip or stretching.
This gives the tab backing the same metal palette and patina as the grid.

Added checks verify exterior cut alpha in BOTH frame assets and the fully
composed HUD, retained bolt-seat pixels, and backing pixels drawn from those
source material patches. Inspect masked corners over the tactical field, and
compare tabs directly with the adjoining cameo frame. v07 evidence and source
are in `art/interface-lab/cnc3d/pixel-tabs-v07-masked-corners/`.
17 Python tests and49 raster/state checks pass, including8 pixel-exact original
C composites. Original vent reference approval remains the finish baseline;
this correction does not change the vent material or button artwork.

## Aligned header, complete rail lip and centered tool enclosure - 7 October 2026

User requests: widen the Sidebar header to the new frame, mask Credits corners,
remove the mismatched top right rail section, vertically center Repair/Sell/Power
against the minimap, and enclose those tools in matching chrome tucked underneath
its left edge. These are Lab-only pixel-art changes.

Sidebar header is184×17, aligned with both184-wide frames at logicalx-12. Its
original caps and whole central lettering retain native pixels; add12 native
pixels of blank source metal on each side of the central label. The control seat
uses the actual184-pixel width. Move Credits left12 pixels so it meets the header
without overlap. Extract four-pixel stepped outside corner cuts in all three
Credits and Sidebar states. Preserve the original compositor as the C oracle;
`drawLabTopTab` applies these changes only to the Lab artwork.

The shaped aperture preserves source x147 at the upper right corner, but v07's
straight rail began at x148. This omitted inner dark lip caused the mismatched
piece just below the cap. Extend the complete13-pixel source rail x147..159,
including its original corner-adjacent runs, throughout both frames. Inspect the
inner lip as well as the outer edge/shadow; test the complete adjoining profile.

Tool plates remain35×35 with42-pixel stride. Their seats are x-57 at y52/94/136.
The stack spans y52..171 against the minimap's y18..205:34 native pixels above
and below. New tool enclosure is64×183 at(-69,20), using original corner/bolt
assemblies, the same source metal backing and continuous rails. Compact
horizontal members use four-pixel source join guards; broad frames retain16.
Draw the tool enclosure BEFORE the minimap, which covers its rightmost7 pixels:
this makes the enclosure protrude on the left while tucking under the minimap.
Button/icon texel density and approved artwork are unchanged. Full sidebar
collapse travel is now241 native pixels to include the protruding tool frame.

Actual browser overview and Credits-corner proof are archived at
`art/interface-lab/cnc3d/pixel-tabs-v08-header-tools/`.53 raster/state checks and
17 Python tests pass, including the8 pixel-exact original C composites. Checks
cover equal top/bottom stack margins, retained centered header lettering, masks
in all header states, complete source rail joins and full collapse travel.

## Open-sided tool extension and centered seats - 7 October 2026

User correction to v08: the entire right edge of the tool enclosure must be
hidden/cut off by the minimap's left edge. Show only its left, top and bottom
members. The tool section must have no vent grooves, and buttons must be
centered inside the visible box rather than the full partly obscured frame.

v09 builds the tool shell81 pixels wide, placing the full source right cap
starting at localx57. Crop the shell to57 pixels before compositing at(-69,20),
so its visible right boundary is exactly the minimap's x-12. No part of the
right rail or either right corner bolt remains in the tool asset. Keep it drawn
behind the minimap. Retain the left cut corners, bolt seats and outer rail.
The panel remains183 high and ends at y203. Replace the former lower groove
field (localx18..56/y151..175) with native source metal-face patches; preserve
the fitted left bolt, lower bevel and perimeter. This solid-bottom rule applies
to this tool extension only; approved minimap and main sidebar vents stay intact.

The visible opening is localx12..56, between the inner left rail and minimap
boundary. Put35-pixel plates at globalx-52: five pixels clear of each side.
Their y52/94/136 seats retain equal32-pixel top/bottom panel margins, and equal
34-pixel margins against the minimap. Do not center against an occluded full
four-sided shell, or count a vent strip as usable interior. Tool glyphs, header,
minimap, cameo metal and texel density are unchanged. Collapse travel remains241.

18 Python tests and53 raster/state checks pass; the original8 C composites
remain pixel-exact. Added checks cover omitted right cap, solid footer material,
centering in the visible opening and equal vertical margins. Actual Lab imagery
and button seat measurements are saved in
`art/interface-lab/cnc3d/pixel-tabs-v09-open-tools/`. Browser inspection confirms
only three tool-frame sides and two left bolts remain visible.

## Rejected joins, true centering, compact frame and dynamic labels - 7 October 2026

The user rejected v09 with close-up evidence of a detached top strip and a
rectangular footer patch/remaining ledge. The button centering checks measured
an invented inner opening, not the panel's visible outline. v09 is rejected;
its automated pass is not visual acceptance. Initial v10 corrected the joins,
but user feedback requires a visibly defined bottom member and tighter spacing.
That intermediate is retained as history, not an approved frame.

Current v11 tool wing: origin(-73,28), native artwork85×167; visible width61,
plus24 pixels of plain metal underneath the minimap. No right rail/right bolts.
Both horizontal members extend into the underlap, so they meet the minimap's
vertical left edge without a truncated stripe. The source left top cap and
fitted lower bolt remain. Use one continuous metal surface, with isolated
bolt/seat artwork; never paste a rectangular differently textured footer face
or retain the original inner map ledge. The bottom is an actual13-pixel steel
member with a lit upper lip and dark underside, using the top member's source
lighting/material profile, a stepped lower-left cut and a fitted bolt. No vents.

Visible panel spans x-73..-12 and y28..195.35×35 buttons sit atx-60 and
 y52/94/136, yielding equal13-pixel left/right margins against the visible
outline, and equal24-pixel top/bottom margins. Their overall stack still has
34-pixel margins against the minimap. Header/minimap/cameo dimensions and tool
plates/icons are unchanged. Full collapse travel is245 pixels including the
wing. Do not align the wing edges with the minimap edges against current user
instructions, or claim centering by excluding only one side's bevel.

The user's stray mark in Sidebar was a copied fragment of its baked-in S glyph.
Header extension strips must use `tab_plate.png`, never `tab_sidebar.png` or
any asset containing text. The184×17 header sheet is now blank in all states.
`drawLabTopTab` centers caller-supplied labels with the native Enhanced
`font_big` sprite font and handles pressed tone/offset. It supports arbitrary
wide labels independently of their text. Options/Database labels also render
from strings; Credits remains a rendered numeric value. Source lettering is
not part of the new background PNG. No automatic localization is implied.

19 Python tests and54 raster/state checks pass, including8 pixel-exact original
C composites. Tests verify blank source-backed header pixels, changed label
rendering, no ink outside the centered label bounds, actual bottom rail
lighting, visible-outline centering and opaque composite attachment joins.
Browser proof in `art/interface-lab/cnc3d/pixel-tabs-v11-solid-rail-dynamic-header/`
includes actual upper/lower joins and the full header at enlarged pixel scale.
Future QA must inspect internal attachments, footer lighting and label-free
backgrounds as well as four outer corners. Do not infer visual approval from
source-pixel checks or correct numeric dimensions alone.

## Equal inner clearances and minimap contact shadows - 7 October 2026

The user requested spacing measured to the start of the frame edges, rather
than the exterior outline used in v11. Current v12 seats move six native pixels
right, to x-54; y52/94/136 and the approved35×35 plates/icons stay unchanged.
Bring the top/bottom members inward by four pixels each: wing85×159 at(-73,32),
61 visible pixels and24 underneath the minimap. Its inner opening is
x-61..-12, y45..178. The button stack has exactly7 native pixels of clearance
on its left, right, top and bottom. Collapse travel remains245.

Paint contact shadows into both13-pixel horizontal members where they tuck
beneath the minimap's left rail. The seven visible native columns before that
edge darken with factors .95/.87/.76/.62/.47/.33/.22. Carry the darkest shade
under the occluder; preserve material alpha and the minimap's original rail.
No detached shadow rectangle, new frame seam, vents, or alteration of the
approved button art. Inspect both joins enlarged in the assembled Lab.

19 Python checks and54 raster/state checks pass, including8 pixel-exact source
C composites. No browser warnings/errors. Actual browser captures and source
snapshots are saved in `art/interface-lab/cnc3d/pixel-tabs-v12-inner-spacing-contact-shadows/`.
This records the implemented revision and verification, pending user feedback.

## Native hover lighting and faction clock - 7 October 2026

The user requested a much more visible hover on Repair/Sell/Power and all four
category tabs, plus a sand GDI/red Nod clock with soft color variation and a
thicker animated sweep. A subsequent amendment requires the remaining clock
area to be much darker. `public/core/effects.mjs` applies these effects at native
resolution, preserving source artwork, alpha and integer nearest sampling.

`drawLitButton` lights the complete worn metal face, icon and bevel on pointer
hover or keyboard focus, including selected orange tabs/modes. Normal/pressed
approved source sheets remain intact. Preserve retained DOM hit targets through
animated redraws: never hide every control at render start, as that fires
pointerleave and cancels hover. Hide only targets absent after reconciliation.

`composeLabCameo` preserves locked grayscale/50% alpha and source status marks.
Use a continuous clockwise radial wedge instead of the109 discrete source
frames. The remaining mask is dark sand RGB91/68/32 for GDI, deep red96/16/12
for Nod. Broad overlapping low-frequency color variation softly moves inside
that wedge, with78–86% coverage. Busy idle cameos use48% coverage, without a
sweep. The moving edge has a roughly3-native-pixel bright core and a broader
soft halo; modulate light along its length in time. The fixed noon seam does
not emit. Construction advances at30 Hz, avoiding the previous100 ms jumps.
The original compositor stays the pixel-exact reference; Lab effects are separate.

This revision is implemented for review, not recorded as aesthetic approval.
Validation:19 Python tests,57 raster/state checks,8 unchanged original native
composites. Versioned source and browser proof live in
`art/interface-lab/cnc3d/pixel-effects-v13-faction-clock-hover/`.


## Approved v13 and game integration

The user approved v13 and authorized replacing the Enhanced in-game HUD on
7 October 2026. The subsequent Power decision explicitly adds building on/off
switching. The approved pixel plates, separate symbols, wing spacing, corner
masks, source-metal continuity, contact shadows and dark faction clocks are the
art authority. Preserve this recipe when extending the system.

Runtime source: `game/hud_enhanced_impl.h` is a portable C89 compositor included
by `hud640.c`; `cnc_sidebar_enhanced.h` binds real engine state. The twelve
hash-recorded source PNGs under `tools/sidebar_redesign/chunks/enhanced/` are
packed by `bake_pack.py`. New packs validate their exact dimensions before the
Enhanced path activates. Old HUD and unit-card composites remain unchanged.

Every texture uses native pixels and integer nearest sampling. At 720p the
panel is 693 pixels tall, eight rows; short displays keep three or more whole
rows and the native lower cap. Do not stretch edges, corner cuts, vents, bolts
or symbols to fit a new viewport. The 245-pixel collapse includes the tool wing.
At 640 pixels wide, shorten only the middle of Options and Database plates to
148 pixels, retaining their bolt caps and runtime text. Minimap and main frame
share the184-pixel outside width. Frame alpha also determines pointer ownership
at cut corners, so gameplay behind transparent corners remains interactive.

Hover pixels must reproduce JavaScript Uint8ClampedArray round-to-even behavior.
A normal round-up conversion differed by one level on 81 red-channel samples;
the independent golden test caught it. Keep the button-specific conversion.
Do not rewrite expected PNGs from the runtime to turn a mismatch into a pass.

Four categories retain engine cameo IDs for orders and queue state. Catalog
entries without an engine entry are grayscale with50% alpha and never send a
build order. Their tooltip uses owned prerequisites plus mission/tech-level
restrictions. Captured and extra engine entries still appear. Mouse wheel scroll
handles compact displays. All categories share the same visible row count and
frame height. Real engine progress drives clocks; interpolation eases between
confirmed updates without predicting ahead or progressing while a factory sleeps.

Power mode uses the native bolt symbol as its cursor and marks switched-off own
buildings. Switching preserves ownership, storage and prerequisites; disables
supply/drain, weapons, production, radar and relevant service/charge activity.
The engine retains state in saves and queues one synchronized POWER_SWITCH event
in lockstep games. Event ABI and network protocol are updated together. Walls,
enemy structures and buildings being constructed or sold cannot be switched.
Repair/Sell retain their engine actions and cursor behavior.

Validation must include the independent frame, six clock and fourteen hover
fixtures; original eight HUD/card composites; hidden screenshots at720p and
640x480; real queue/category and tooltip interaction; power output/drain,
production pause/resume, save/load and two independent engine instances.
Visual inspection of all caps and wing joins remains mandatory even with green
numeric tests. The compositor has no shader or bloom dependency; Tier1 keeps
CPU pixel effects and nearest alpha-composited artwork.
