# OpenCNC3D 3D model pipeline

Living reference, started 2026-09-22 from the Mammoth Tank modeling session.
This document establishes a repeatable quality bar. Update it with measured
results, user-approved design decisions, and verified tool behavior as the
pipeline develops. Asset-specific measurements are examples, not universal sizes.

**Approved reusable baseline:** the approved Mammoth multi-angle direction, selected v03
red-tip amendment and later approved darker/grittier representative-pod treatment
have been propagated across **Lookdev05: 2888 triangles, 56 low meshes**. Full
physical normal/AO and paint/PBR/player-mask bakes, actual multi-view/changing-light
and player-colour inspection, and a fresh save/reopen check are complete. The
packed authoring file is `art/mammoth_lowpoly/Mammoth_TD_OpenCNC3D_Lookdev05.blend`;
earlier checkpoints remain preserved. The user approved this completed full-asset
result on **2026-09-22** as the reusable authoring baseline. See the hash-bound
[Lookdev05 approval record](../art/mammoth_lowpoly/LOOKDEV05_APPROVAL.md).
This does not claim reference-quality parity. Runtime texture-budget acceptance,
export, pack import and an in-game War Factory exit remain pending.

**Lookdev05 authorization history:** after reviewing the actual textures, the user
requested a stronger hybrid AI-assisted/art-authored treatment and authorized a
**3000-triangle ceiling** for the next Mammoth revision. The measured footprint
and selected v03 design remain authoritative, with the later approved darker,
grittier pigment amendment. After the representative section corrections, the
user explicitly authorized applying all learnings across the tank and then
approved the result. Authorization, technical checks and artistic acceptance are
separate recorded events; runtime integration is still a separate gate.

**Current Mammoth stage, 2026-09-23:** the user accepted actual **HP10** and selected
the corrected **GPT Image FinishDirection02** after the Ideogram comparison,
then explicitly directed final-low work. The [approval record](../art/mammoth_lowpoly/highpoly_v10/HP10_FINISH_APPROVAL.md)
binds the decision to both hashes. Its then-current historical ceiling was
**10,000 evaluated triangles** for both vehicles and structures. The separate
`final_low_v01` work follows accepted HP10 geometry and the baZGeo-guided finish;
HP09 and Lookdev05 remain preserved historical approvals. FinalLow01 subsequently
received explicit approval to proceed with texture work; its separate
[approval record](../art/mammoth_lowpoly/final_low_v01/FINAL_LOW_APPROVAL.md) binds
the accepted file. Mapped-material and runtime acceptance remain separate.

**Approved FinalLow01:** the separate [actual mesh review](../art/mammoth_lowpoly/final_low_v01/FINAL_LOW_REVIEW.md)
now records **9,870 evaluated triangles**, unchanged complete XY envelope and
restored Z=0 track contact. Saved-file topology and complete low-track versus
low-hull/armor checks pass; all 2,148 high sources have named bake receivers.
The [Texture01 UV checkpoint](../art/mammoth_lowpoly/texture_v01/UV_HANDOFF.md)
preserves the approved geometry and shading basis. The separate
[MaterialTrial06 review](../art/mammoth_lowpoly/texture_v01/MATERIAL_REVIEW.md)
was approved, with 47 receivers covering the relevant material and construction
cases and 159 gray context meshes. The hash-bound
[approval record](../art/mammoth_lowpoly/texture_v01/MATERIAL_TRIAL_APPROVAL.md)
authorizes whole-asset propagation. Runtime integration remains pending.

**Start new work with [the detailed recipe v1.1](3d-model-recipe.md).** It is the
ordered runbook, including exact successful Mammoth settings, prompts, material
logic and replay limitations. This pipeline supplies general rules and gates;
section 9 preserves historical experiments. Counts and pending statuses inside
those historical checkpoints describe that stage, not the current approved asset.

## 1. Research, initial low-poly, then approved generated views

**Proxy-first amendment, 2026-09-24:** use suitable meshes from
`art/enhanced_proxy_meshes/` as the starting initial low. Preserve the source, make
only necessary reference-led adjustments in a versioned copy, and derive the high
from that adjusted model after generated-design approval. Do not rebuild either
stage from scratch when the proxy can be reused. Original OpenCNC3D measurement and
all approval/quality gates remain required; see recipe section 3.

The user's final production-order instruction supersedes the earlier
concepts-before-any-mesh rule. For every new model, **research the subject and
material references online, look up and measure the original asset in the OpenCNC3D
source, then build an initial low-poly design mesh preserving that footprint**. Render
that actual mesh from multiple angles. Give GPT Image those renders and the
ArtStation finish reference to generate coherent gritty multi-angle views.
Obtain approval of the generated design sheet; then create and obtain approval
of the **high-poly**. Run its actual approved views through **GPT Image again
for high-quality finish direction**, with the asset-selected ArtStation finish
reference. Review and select that finish before constructing a **separate final
optimized low-poly**, then obtain final-mesh approval before final UVs, physical
bakes, textures and validation. Good initial topology may be reused, but these
are two deliberate low-poly stages with different purposes, not one rough mesh
declared final.

**Mandatory user approval stops for every asset:** first low-poly before GPT Image;
first generated design views before high-poly; high-poly before the second GPT
Image finish pass; that generated finish direction before final optimized low-poly;
final mesh before final UVs/bakes/textures. The added finish review preserves all
four original approval gates. Present each specific version, stop
and wait for explicit approval. General authorization to start does not waive
these stops; requested revisions require renewed approval at the affected gate.
The later representative-material and finished-asset gates remain in force.

Use the selected design sheet, post-high finish sheet and explicit later
amendments throughout the relevant high/low/texturing stages. The first generated
views develop the initial design mesh; the second pass treats the approved high
geometry and saved camera views as fixed. Neither pass may silently replace the
approved silhouette, measured envelope or part layout.
Use [the recipe's reference roles and prompt scaffold](3d-model-recipe.md) to keep
the design, ArtStation finish and runtime constraints distinct.

Collect these brief inputs before the initial mesh and generated sheet:

- Game and unit/type code, reference art, and which interpretation is intended.
- Hard triangle ceiling, target platform/art tier, texture stage, and output type.
- Required silhouette features and what can be simplified at gameplay distance.
- Required footprint, ground plane, forward/up axes, and moving-part roles.
- Required player-colour regions and explicit mask/albedo pairing for every asset.
- Which measurements are hard constraints and which proportions remain artistic.
- Whether this task is geometry only, delivery preparation, or actual integration.

Use the references deliberately: identify which establish shape and which provide
only a texture/finish target. Render the initial mesh, then generate matching
front three-quarter, rear
three-quarter, side and top views; add front, opposite side or underside views
when important forms remain ambiguous. Use consistent scale, neutral lighting,
orientation and background. All views must depict the same assemblies, silhouette,
paint layout, fixed-colour accents and player-colour zones. Preserve original
design identity rather than importing another asset's geometry, insignia or palette
when it was supplied only as a finish reference.

Before approval, compare views for invented or missing components, mirrored
features, reversed weapons, inconsistent wheel/pod counts, changing proportions
and impossible joints. Correct contradictions in a versioned revision and record
any intentionally interpreted unseen surfaces. A plausible image is not proof of
cross-view consistency. Do not silently select a conflicting view while modeling.

Alongside the selected sheet, plan which visible features are:

- **Silhouette/functional geometry:** budgeted low-poly shapes, openings, moving
  parts and clearances that must remain visible at gameplay distance.
- **Bake-only geometry:** rounded source barrels/bevels, recessed wheel dishes,
  hubs, bolts and links that can project onto the approved runtime surfaces.
- **Texture-only detail:** paint variation, scuffs, dirt and other marks that
  need no geometric relief. Do not expect colour alone to supply a normal bake.
- **Player colour versus fixed colour:** explicitly identify neutral player-paint
  regions and exclusions before UVs/materials; plan the mandatory mask pairings.

Preserve the initial low-poly file and its actual input renders. Save each
generated sheet under the asset's `references/` directory with a versioned name
and companion Markdown record. Preserve source URLs/attribution, input image
filenames, the exact generation/edit prompts, date, generator/tool provenance,
output filename and hash, and the approval/change-request history. Record the
actual model version only if exposed; do not infer a version from the user's
requested model name. Mark one selected version, retain superseded versions and
make the selected sheet available to every modeling/baking/texturing pass.
Third-party artwork remains reference, not an automatically licensed texture asset.

**Concept approval is an artistic gate, not dimensional or runtime authority.**
Generated views are not guaranteed orthographic drawings, UV atlases, Blender
renders or evidence of a triangle count. Keep measured runtime footprint, axes,
pivots and environment clearance authoritative. If an approved concept conflicts
with that envelope or budget, resolve and document the adaptation before moving
on; do not guess a new footprint from generated pixels. A later artistic change
needs an explicit selected-sheet revision, not divergent low/high/texture targets.

Read [the art return contract](art-return-contract.md), but verify implementation
when making export promises. The discrepancies in section 8 below were found by
reading the current importer, not by assuming the older document was exact.

An approved stylized/high-poly model is a deliberate artistic departure from the
cartridge presentation. Do not silently alter the project's original art set or
overwrite canonical packs while making a replacement.

### Second GPT Image pass after approved high-poly

The 2026-09-23 workflow amendment adds a reusable finish-direction stage between
high approval and final-low construction. Feed the exact approved high's actual
multi-angle renders to GPT Image, with the asset-selected ArtStation reference
for texturing, materials, metal response, roughness and grittiness. Preserve the
approved part count, shape, proportions, openings, pose and camera composition.
Use the latest explicit asset-specific finish reference; the general J9Z64d
default does not override a later choice such as Mammoth's baZGeo.

Keep the high file, approval hash, actual source renders and camera data intact.
Save the exact prompt, input/output filenames and hashes, reference URLs and
attribution, source roles, tool/date provenance, view mapping and versioned
selection record. Compare each generated view to the actual high and document
invented, missing, distorted or moved geometry and cross-view finish deviations.
Correct/reject drift; any chosen structural change must be made in the actual
high and return to its affected mesh approval gate.

Label these outputs **GPT Image generated finish concepts**, separate from actual
Blender renders. They communicate material/art direction, not measured metallic
or roughness values, a texture set, physical bakes or Blender/runtime validation.
Present the new direction for user review and explicit selection/approval before
final-low work. Preserve the four original approvals and the later mapped-trial
and full-asset gates. The detailed prompt and inspection sequence are in
[recipe section 5](3d-model-recipe.md#5-high-poly-generated-finish-direction-then-final-low-poly).

When the user's latest instruction explicitly requests a high revision followed
by image regeneration, complete that combined preview sequence without inserting
an unrequested approval interruption. Present both the revised actual high and
the regenerated finish for renewed acceptance before final-low. General
permission to start/continue does not authorize this exception.

## 2. Reference and measurement

**Mandatory: the original asset must be looked up in the OpenCNC3D source and its
footprint preserved.** Resolve the correct game/type and complete rest-posed part
set before the initial low-poly. Record source pack/ROM identity and hash, readers
used and the measured original bounds; keep them as the dimensional reference
through high-poly, final low-poly and export. Preserve origin-relative horizontal
min/max bounds, not just width/length around an arbitrary new center. Treat the
hull/contact footprint and complete overhang envelope separately. Generated
views, online art, a generic gameplay cell and a partial OBJ cannot replace this
lookup. Missing or ambiguous originals require an explicit user-selected
reference/footprint, not a guessed size.

Keep a non-export footprint guide and a measured original-versus-new comparison
with numeric deltas and a documented conversion/rounding tolerance. Recheck after
every silhouette/proportion change and on the final low-poly/export copy. This is
not a demand to copy the old triangles or lock artistic height to the old model;
height and detail remain flexible within user/environment constraints. Never
silently expand the footprint to make new geometry fit.

Research original references and measure the actual runtime asset before the
initial design low-poly. After generated-view approval, use the selected sheet
to develop the high-poly and final low-poly within those measured constraints. Record
source URLs and provenance. One image does not specify an unseen rear/underside:
mark those as interpretations, even when a generator has filled them in.

The authoritative original vehicles are not ordinary standalone model files.
The production path is ROM display lists and scene-graph rest transforms, through
the baker, into mission pack triangle lists:

```text
ROM -> display lists + rest pose -> tools/bakery/bake5.py -> mission .pack -> renderer
```

Prefer `tools/art/fbxout.py` with the relevant pack and type code. Retain the JSON
manifest, including roles and pivots. An OBJ may be useful visually but cannot
carry the complete rigid-part/pivot contract and may contain only part of a unit.

When a baked pack is unavailable, the project's own ROM readers can reconstruct
the reference. Use `tools/bakery/objgraph2.py` for parts/rest transforms and
`tools/romdump/model.py` for display-list triangles. Apply the node's rest pose
before measuring; compare extracted counts with `tools/bakery/parts_survey.json`.
Compute bounds from vertices actually referenced by triangles, not unused buffers.
Do not guess dimensions from a screenshot or an incomplete legacy OBJ.

Inspect source render modes before measuring: **shadow planes are not physical
footprint geometry**. Construction Yard `FACT` contains 163 original triangles,
but eight are shadow faces; including them incorrectly enlarges its footprint.
Its physical rest-pose geometry has an asymmetric seven-point contact outline
and eight-point horizontal envelope. Preserve measured guides instead of assuming
the gameplay 3x2 occupancy is a rectangular model outline. The pack's `static`
part label also does not imply no animation: FACT crane parts have active node
channels. Retain component editability and inspect animation separately.

Record separately:

- Hull/track contact footprint, including origin-relative min/max positions.
- Overall rest-pose envelope, including barrel overhang and antennas.
- Turret/rotor pivot and role, ground contact, and moving-part clearance.
- Relevant environment clearance; a dimension alone does not prove an animation fits.

## 3. Authoring units, axes, and scene organization

Pack coordinates use X horizontal, Y up, Z horizontal; **1024 model units = one
map cell**. The renderer applies this scale; it does not automatically fit new art.
The Mammoth's model forward is +Z in pack space. Check other types independently.

For new fitted Blender assets, use this explicit working convention:

| Meaning | Blender authoring | Pack/runtime |
| --- | --- | --- |
| Width | X | X |
| Forward for Mammoth | -Y | +Z |
| Up | +Z | +Y |
| One map cell | 1 Blender unit | 1024 model units |
| Position conversion | `(x, y, z)` | `(1024*x, 1024*z, -1024*y)` |

The older artistic Mammoth files use arbitrary larger units. Do not apply this
conversion to an arbitrary source file without checking its metadata and bounds.

Keep an editable source assembly: one model collection, one root, named mesh
components, and one parent per movable assembly. Parent cannons, launchers,
hatches, and side rails to the turret if they must turn with it. Keep cameras,
reference geometry, and footprint guides outside the model collection.

Do not rely on an unapplied root scale to make an asset fit. Bake placement into
mesh coordinates, keep object rotations/scales controlled, and explicitly handle
parent transforms. Record the pivot rather than trusting visual alignment.

Editable authoring objects and runtime parts are different things. The Mammoth
started with 64 editable mesh objects, had 44 after an earlier running-gear cleanup,
and has **56 low meshes in approved Lookdev05**. Its delivery contract still calls
for **two rigid runtime parts**; recheck the importer when preparing that export.
Make a separate export copy; do not destroy editability to satisfy the importer.

## 4. Shape and polygon quality

**Current category ceilings: structures/buildings 10,000 triangles; vehicles
10,000 triangles.** The user reaffirmed these limits on 2026-09-24, superseding
the erroneous 5,000/8,000 limits. Mammoth RuntimeRig01 remains at 9,990 triangles.
Count the whole evaluated runtime asset, including every rigid/moving part and
modifier result. Exclude only designated non-export high-poly, cages, reference
and QA objects. These are maxima, not targets. Record explicit per-asset overrides.
Older Mammoth budgets in section 9 describe their historical stages; the new
ceiling does not require changing or adding geometry to approved Lookdev05.

Spend the budget in this order:

1. Recognizable silhouette, stance, major massing, and important negative spaces.
2. Distinctive functional shapes: Mammoth twin guns, four track pods, side launchers.
3. Readable bevels/chamfers, open bores, launcher recesses, and supporting structure.
4. Details that survive the game camera: hatch, headlights, side rails.

There is no reward for using every available triangle. Leave headroom when extra
triangles would not improve the result. Six- or eight-sided cylinders were useful
in the earliest tight-budget stages; approved Lookdev05 uses 16-sided low-poly gun
walls where the larger silhouette budget matters. Choose by actual camera tests,
not a fixed cylinder rule. A few true recessed sockets can read better than many
small surface decorations. Keep round gun cross sections round when reshaping.

Keep broad hard-surface panels planar while allowing appropriate smooth shading
across rounded chamfers and cylindrical sides; a blanket flat or smooth setting
is not the goal. Recalculate and inspect corner normals after procedural mesh
creation. Inspect the result in neutral clay without textures before relying on
materials to hide topology or silhouette problems. Changing low-poly loop normals
after a tangent bake requires a matching rebake; an old map may compensate for a
different shading basis and is not proof that the new normals are correct.

For a late chamfer pass under a tight budget, test selected edge loops on temporary
BMeshes before changing the asset. One-segment bevels on prominent armor rims can
add useful highlights at low cost; beveling every edge or every missile socket is
usually wasteful. Count the actual result: a rectangular launcher face can have
subdivided perimeter edges, so its bevel cost is not necessarily that of a box.
Validate footprint extrema and neighboring-part gaps again after beveling.

Finish proportion fitting before this topology-changing pass. Preserve the approved
unbeveled source and use it for repeatable chamfer regeneration. Vertex-index-based
fit snapshots cannot safely be applied to the altered bevel topology.

For the high source, choose an edge-width hierarchy from the finish reference and
intended viewing distance. Inspect actual evaluated setbacks: thin plate stock or
overlap clamping can make a larger nominal bevel ineffective. Use appropriate
thickness and explicit planar edge strips where necessary, keeping broad faces
planar and small hardware proportionate. Refit attachment seats and hardware when
the receiving chamfer changes; inspect clearance again after integration.

Closed topology and a correct global envelope do not rule out bevel artifacts.
Check local component contour/bounds and grazing views, especially at fitted
non-planar caps. Mammoth HP10's mantlet test exposed long miter spikes despite
manifold topology; explicit planar strips and local-outline assertions corrected
them. For revision review, pair matched actual before/after close-ups with equally
reduced whole-asset renders under the same lighting. Label reduced renders as
distance comparisons, not runtime tests. See the
[recipe's high-source checks](3d-model-recipe.md#high-poly-from-the-approved-gritty-result)
and [HP10 evidence](../art/mammoth_lowpoly/highpoly_v10/INDEPENDENT_REVIEW.md).

Retain silhouette-critical and broad form-defining chamfers as geometry when
deriving the final low. Preserve their visible cross-sections and fitted seats
within the original measured footprint; simplify segment density rather than
discarding the approved edge hierarchy. Fine bevels may transfer through named
high-source/low-receiver bakes when they do not need silhouette or parallax.
Judge the allocation with matched high/low views, projected on-screen widths and
planned texel density. Once the final low is approved, verify actual bake fidelity
at delivered resolution; a nominal bevel setting is not proof of a readable edge.
Any later topology repair returns to affected mesh approval and matching rebakes.

### Initial low, high, generated finish direction, then final low for baking

The initial low-poly and approved mesh-guided generated views establish the
high-poly direction. Preserve that initial file and its measured footprint,
pivot and part roles. Duplicate or rebuild components in a separately named,
non-export high-poly collection; hide it outside source inspection and baking.
Model the approved detailed forms: smoother cylinders, small
multi-segment bevels, shallow panels/bolts and running-gear relief. A normal bake
cannot create a detail that is absent from the physical source or source shader.
Do not expect a bake to change the runtime silhouette or create real openings.

After high-poly approval and review/selection of its GPT Image finish direction,
derive/refine the final optimized low-poly against the approved actual high.
Carry the selected finish's material-region requirements into feature allocation.
Preserve required silhouette and openings in real budgeted geometry;
use physical bakes for relief that can project onto those final surfaces. Reusing
initial topology is allowed, but do not freeze final topology/UVs before this
second low-poly stage. Recheck footprint, pivots, gaps and evaluated triangles,
then freeze the final shading basis and triangulation before the final unwrap.

The high-poly source is not part of the game triangle budget. Keep it, bake cages,
triangulated bake targets and QA scenes out of the delivery-part selection. Use
matched component pairs so a launcher, turret or track does not capture a nearby
part's normals. Match source visibility to the intended AO scope: the current
Mammoth AO is component-local, not a permanent shadow between moving assemblies.

Prepare a unique `BakeUV` atlas for the armor/static detail and a separate repeated
UV layer for tread travel. Preserve exact triangulation, UVs and loop normals from
the tangent-space bake target through any delivery that uses the normal map;
retriangulating editable ngons later can change the tangent basis. Keep the
editable assembly separately. A Blender tangent normal map is useful for authoring
QA but is not currently consumed by the vehicle renderer; `--shade normals` means
geometric-normal-derived vertex shading, not normal-map support.

Check the bake itself, not only a zero-missed-ray count. The first Mammoth 1024
working atlas showed sawtooth bevels, dotted fine details and local barrel shading
artifacts despite having no black covered normal pixels. The corrective rules are:

- Give fine bevels enough texels; raising render samples cannot recover subpixel
  geometry in a normal image. Inspect a representative armor panel and gun before
  repeating a whole-asset bake.
- Split UVs at hard-normal boundaries. A broad Smart Project angle can keep flat
  faces and their chamfer transitions in one island, leaving no gutter for the
  discontinuous tangent normals. Coplanar faces may remain joined.
- Freeze the intended triangles before the final unwrap/audit, especially around
  circular holes cut into concave plate ngons. In Lookdev05, unwrapping before
  final tessellation produced 394 sampled overlapping pixels around the launchers.
  Re-unwrapping the frozen triangles reduced this to 15; explicit seam splits at
  the remaining shallow bevel folds removed them. A 3D shared-edge/plane check
  confirmed these were UV folds, not intersecting geometry. Preserve the verified
  final triangulation, UVs and corner normals together through saving/reopening.
- Measure island spacing against bake dilation: leave at least twice the padding
  plus filtering allowance. Padding from sequential component bakes must not
  overwrite neighboring island interiors.
- Keep normal/mask data non-color, use an explicit tangent convention and the same
  triangulated mesh in QA. Investigate projection/cage directions separately if
  artifacts remain after resolution and UV fixes.
- Inspect source corner normals on long cylinders. Averaging end-bevel normals
  into one long wall can interpolate a false axial bulge across the whole gun,
  even when target triangulation and UVs match perfectly. Correct source wall
  normals analytically or harden the bevel transition; extra pixels or ray length
  do not fix incorrect source shading. Weighted normals alone did not resolve
  this Mammoth case.

The earlier 2K Mammoth correction added 196 hard-edge UV splits, repacked `BakeUV`
with a 0.008 fractional island margin, and retained `BakeUV_Draft1024` as a backup.
That historical working rebake used 2048 pixels and 4-pixel dilation. Approved
Lookdev05 instead uses 4096 pixels, 0.0045 fractional packing margin and 8-pixel
`EXTEND` dilation after frozen-triangle seam correction. These remain authoring
settings, not approval of a runtime texture budget; verify actual texel gutters.

### Hybrid paint must agree with physical detail and material data

For AI-assisted or art-authored paint, use the actual model's orthographic guides,
face coverage and planned detail positions as placement authority. Generated
images may develop the surface treatment; they must not silently move seams,
bolts, wheel centers or UV boundaries. Project and clean accepted paint onto the
model, inspect seams, and bake it into the intended UV layout. Retain generated
source images, exact prompts and projection settings separately from final maps.

A generated texture's request for seamless borders is not a verified repeat
contract. Test opposite edges and repeated presentation before tiling it. For
non-repeating armor pigment, a measured projection wholly inside one source sheet
is also valid: Lookdev05 uses world/rest scale 0.85 and offset (0.5, 0.5, 0.15),
with every armor/olive sample inside 0..1. This is not permission to treat that
source as a seamless tread or terrain tile. Do not apply the old pale-source
darkening multiplier again to a newly generated source that is already dark.

For legacy terrain replacements, keep **material-layout evidence** separate from
**terrain geometry** and **material/style authority**. The OpenCNC3D renderer maps one
texture rectangle onto each terrain cell's real heightfield corners; the scenario's
65×65 heightmap supplies the macro slope, elevation and projected cliff direction.
The albedo is therefore a surface material in cell UV space, not a second physical
cliff object. It may organize broad face/recess/toe values to preserve the cliff
read at gameplay distance, but those painted values do not establish physical
planes, silhouette, collision, cast shadows or height-map authority.

The DOS cliff pixels contain baked lighting. Dark regions can be cast shadow,
surface colour, crevice paint or grass under shadow; they are not automatically
geometry, rock coverage, boulders, contour rings, cracks or planes. A palette-based
"rock topology" mask is a heuristic material mask when the source cell is fully
opaque, not exact geometry authority. World-cardinal heightmap gradients may prove
which side is higher, but an arrow in texture-image space does not prove the local
face layout or perspective.

Keep grass and neutral rock pigment free of baked directional cast shadows. The
scenario heightfield and runtime lighting create the real macro slope, but they do
not necessarily create the DOS art's apparent near-vertical wall. In a matched
Enhanced SCG10EA North-core capture, the PK9 section falls about 0.81 cell over
two cells (roughly a 22-degree ramp); a flat granite swatch read as a gray band,
while the DOS stamps still read as vertical buttresses. A replacement therefore
needs an art-directed, connected face-value composition at gameplay scale, or a
separately engineered real escarpment mesh if geometric wall depth is required.
Do not describe painted face values as decoded source geometry or let them drive
displacement. Texture normal and height maps add only coherent small-scale stone
relief; they must not reproduce scenario elevation or emboss DOS shadows. Preserve
source icon order, absent cells and verified edge continuity, then validate at
target cell size and in connected in-engine assemblies with the actual heightfield
and camera. The 2026-09-23 flat-swatch core v03 is a useful failed diagnostic,
not accepted cliff artwork.

The 2026-09-22 S08 image remains a user-liked **rock material mood reference only**.
Its drawn structure is not geometry authority for S08 or any other template. S01
trials proved that literal tone bands, heuristic masks, broad-mass guides and even a
cap/face/toe gradient can all survive exact clipping while still inventing raised
geometry; masking, grading, normals and displacement cannot repair that category
error.

**Accepted structural details need aligned relief.** A panel join, overlapping
plate edge, bolt, recessed fitting or comparable feature accepted from generated
or hand-authored art must also have matching physical high-poly geometry or a
controlled, explicitly authored height source for its normal and AO treatment.
BaseColor alone is insufficient. If a proposed detail cannot be reproduced
coherently, remove it from the accepted paint or retain it only as an explicitly
identified pending design proposal. Do not imply that an old normal/AO bake
contains details subsequently invented in a new paint image.

Camouflage, color variation and ordinary dirt stains are not embossing. Do not
convert overall albedo luminance into height or normals: that would emboss paint
patches, highlights and shadows. Physical grime buildup or a chipped paint edge
may have deliberately authored relief, but its height and scale need an explicit
decision. Keep structural relief separate from optional fine paint height, avoid
double-counting existing baked bevels, and regenerate matching AO where the
accepted physical structure changes. Document the AO source and scope.

Use one named semantic mask stack in the same UV/coordinate basis: intact paint,
player paint, primer/undercoat, exposed steel, rubber, dust, seams and hardware.
The same chip, primer and dust boundaries must drive BaseColor, Roughness,
Metallic and player-paint exclusions; where a layer actually has physical
thickness, align its controlled height/normal contribution to those boundaries.
Do not independently generate attractive-looking maps or classify materials by
the brightness of generated RGB. Preserve the editable selections as well as the
baked outputs so later paint changes can update all affected channels together.

Material response must remain coherent across that stack:

- Intact paint, including player paint, is dielectric, even over a steel body.
- Exposed steel and worn contact edges are metallic and generally smoother than
  the surrounding paint; tune actual abrasion, oxidation and dirt deliberately.
- Primer/undercoat is dielectric. A pale or bright exposed undercoat is not bare
  metal merely because its color resembles a highlight.
- Rubber and dust are rough dielectrics. Dust over steel reduces its exposed-metal
  contribution as well as changing color and roughness.
- Bare chips, primer, rubber and dirt remain outside player recolouring. These
  exclusions use the same boundaries as their visible surface treatment.

These are recipe requirements, not proof that a generated source image already
has matching normal/AO or material maps. Validate the actual model in a neutral
normal-only material, with albedo, AO and fine color-driven effects disabled, to
confirm that the accepted joins and fasteners exist and shade correctly. Then
inspect the complete material while changing light direction at fixed exposure:
specular highlights must respond to the light, rather than remain painted into
clean BaseColor. Check normal-only and complete-material views at close-up and
representative gameplay screen sizes before adopting the treatment as a recipe.

### Validate the saved image, not only its authoring buffer

During the Mammoth first pass, the packed float RGB authoring-image path produced
a washed-out material preview relative to the saved PNG. The correction loads
each saved PNG as a fresh image, explicitly marks base colour as sRGB and normal,
roughness, metallic and mask maps as Non-Color, then packs and connects that decoded
image in the inspection material. Preserve procedural/float buffers as recipes,
but treat the decoded delivery image and its recorded colour space as the preview
reference. Do not assume that a successful save/pack proves the shader is consuming
the same data as an external texture file. Record paths and hashes in the manifest.

Record how AO is composed. If a displayed or legacy derivative already contains AO, do
not multiply it a second time in the material. Mammoth Texture Pass 01 used 34%
AO influence once in the atlas base colour; preserve it as a historical fallback,
not the PBR master defined below.

### Metal/roughness PBR master and reflection inspection

The chosen master authoring workflow is **metal/roughness PBR**. Keep BaseColor,
Normal, Roughness, Metallic, AO and the explicit PlayerColorMask as separately
identified images. BaseColor is sRGB; normal, roughness, metallic, AO and the mask
are linear/Non-Color data. Record normal convention and UV/dimension pairings in
the manifest. A target-specific packed texture may be produced later by a verified
adapter; it does not replace the editable, explicit master images.

Keep AO and lighting out of the clean PBR BaseColor master. Use the separate AO
image through a documented shader/engine policy; do not silently alter the master
or apply AO twice. The user may approve an art-directed AO-composited appearance,
including for a PBR material, or a legacy diffuse-only delivery may need it. Bake
that appearance into a separately named derivative such as `BaseColorAO`, record
the strength and which image the displayed material consumes, and apply no second
AO multiplier. Keep the clean master and source AO alongside the derivative so
the choice remains reversible. A request to preserve AO's aesthetic takes
precedence over removing it from the displayed material.

Separate material classes before tuning reflection strength. Opaque painted
armor is dielectric even when the substrate is steel: its metallic value stays
zero, while its roughness controls broad paint highlights. Exposed steel, worn
tread contact surfaces and bare-metal chips receive their own metallic response
and roughness. Rubber, dust and dirt remain nonmetallic. Do not make all armor
metallic merely to obtain visible specular highlights, or use low roughness
everywhere to conceal weak material separation. Player paint remains dielectric;
exposed chips, dirt and rubber remain outside its mask.

Inspect a PBR pass under both neutral lighting and a reflection-readable setup
with broad area lights or a structured environment. Flat ambient illumination
can hide specular response; lighting that is excessively bright can wash it out.
Check broad paint highlights, tighter exposed-metal highlights, normal detail,
roughness variation and dark rubber separately while preserving the base colour.
Use consistent exposure and views for material comparisons. A prettier studio
render is not proof of readability at the game camera or under engine lighting.

Match the reference's pigment value, tonal hierarchy and weathering in BaseColor
under held neutral lights and exposure. If the paint is too pale or clean, revise
the pigment and deliberately placed grit; do not fake the correction by darkening
the lights/exposure or multiplying AO into the clean master. Avoid unintended
creamy highlights across the whole coat and uniformly distributed grunge. Preserve
quiet paint masses between local abrasion, handling marks and accumulated dirt.
Pigment variation is not automatically height or bare metal: keep physical relief
and semantic material selections authoritative. Accept this treatment on the actual
representative model before extending it to the whole asset.

For painted camouflage, separate **shape**, **pigment** and **weathering**. Build
coherent masks with decisive paint borders and only a narrow texel antialias band;
do not obtain the pattern by blurring colour noise or varying opacity across the
whole patch. Generate or project the pattern in one shared object/world coordinate
frame, then bake it into the final UV atlas; independently generated atlas-space
fields create false cuts wherever adjacent armor faces occupy separate UV islands.
Tint or multiply the approved underlying BaseColor so its panel grain,
scratches, staining and local value variation remain visible inside every colour
field. Composite dirt, soot and oil above that pigment layer. Protect explicit
player-colour regions. Check a close neutral render for retained surface detail and
an actual gameplay-size render for readable patch boundaries; passing only one of
those views is insufficient.

For vehicle delivery, retain the unmodified pre-camouflage BaseColor and the baked
object-space classification mask as reproducible sources. Record their hashes with
the final BaseColor/Roughness and runtime model. Camouflage alone must not alter
Metallic, AO or Normal; any roughness change must describe a deliberate paint-batch
response and preserve the underlying authored variation. Include one close seam
continuity proof and one real engine/gameplay proof in the validation record.

When evaluating scene lights in Blender, verify that the viewport actually uses
scene lights/world rather than an unrelated Material Preview environment. Keep
authoring light/view changes separate from model orientation and export axes.
Record case-specific light settings rather than treating one energy value as a
universal default for differently scaled scenes or light types.

PBR authoring is distinct from runtime support. The current OpenCNC3D vehicle path
does not consume this full PBR set or a traditional authored specular/normal set.
Either requires a separately verified importer/renderer adapter, or a documented
appearance conversion for the existing renderer. Do not call a Blender PBR
preview integrated or change runtime shading as an incidental art-file edit.

### Mandatory player-colour mask for every asset

Every asset must have an explicit player-colour mask image as part of its texture
deliverables. This is a user-required pipeline rule, not an optional Mammoth-only
effect. Author the mask directly from intended recolour regions; never derive it
by guessing which diffuse hues look like a player colour.

The authoring contract is a grayscale PNG sampled as linear **Non-Color** data:
black/0 leaves the corresponding albedo unchanged; white/1 fully enables player
colour; intermediate values represent fractional coverage. Each mask must have
the same dimensions and UV layout as its paired albedo. Do not silently use the
albedo alpha channel, a spare material channel, or another runtime encoding in
place of this explicit image. Any engine-specific packing is a documented adapter
step that must be verified separately.

Keep dirt, exposed bare metal, rubber, track links and other intentionally fixed
materials outside the recolour regions. Under player-colour paint, use a neutral
tonal base so applying a tint preserves highlights, shadow/AO variation and surface
detail instead of multiplying a saturated fixed paint colour by the player tint.
Document the actual blend/tint rule with the asset. For example, an authoring
preview can blend unchanged base colour with `neutral_tonal_base * player_tint`
using the mask, in linear colour space; the runtime adapter must reproduce the
intended appearance rather than assuming a shader convention.

An asset-wide all-black mask is allowed only when the asset is explicitly intended
to be non-recolourable, such as a fixed-colour prop; record that decision in the
manifest. Do not substitute a black mask for missing player-colour authoring on
a recolourable unit. Auxiliary albedo tiles for intentionally fixed materials,
such as the Mammoth tread/rubber tile, still receive a documented all-black paired
mask while the unit's armor atlas supplies its actual player-colour regions.

Pack the mask into the editable Blender source and include its standalone image
in the delivered texture bundle. The file manifest must identify the mask filename,
paired albedo, UV layer/layout, dimensions, Non-Color interpretation, black/white
semantics, tint rule, and any intentional all-black exception. A mask that exists
only in a procedural node or an unlisted local file is not a complete deliverable.

Before texture acceptance, render at least two clearly different player colours
using the same mask. Check that regions retain shading and wear, black-mask areas
stay unchanged, and filtering/mips do not bleed colour into neighboring UV islands,
rubber, bare metal or dirt. Include those previews with the handoff. Then verify
the chosen importer/pack/shader adapter independently in the engine; authoring
previews and the mask requirement do not establish that a particular import path
already wires the image to player colour. Do not omit the mask while that adapter
is pending, or claim integration from Blender previews alone.

### Required edge-wear treatment for the Mammoth texture pass

The user explicitly requested that the new chamfers drive **baked texture edge
wear**. The first texture pass implemented this in base colour; the following
attribute details describe that preserved early checkpoint. Approved Lookdev05
extends geometry-aware wear through shared BaseColor, roughness, metallic,
paint-loss height and player-mask exclusions; use the current recipe for that
complete treatment. Runtime-resolution acceptance remains separate.

The chamfered Blender file includes an `edge_wear_chamfer` mesh attribute
(`FLOAT`, `FACE` domain): 1 on the new bevel faces, 0 elsewhere on those eight
meshes. There are 56 marked faces. Objects without this attribute contribute zero
to this particular selection mask. The root also stores the texturing requirement.

During texturing, preserve that mask through UV preparation and delivery-copy
creation, then bake it into UV space. Combine the selected chamfers with evaluated
curvature/exposed-edge information for existing armor bevels, and introduce restrained
wear variation appropriate to the art direction. Check that the wear follows the
geometry, survives the delivered atlas resolution, and does not bleed across UV
islands or close the visual missile openings. Bake the resulting wear into the
delivered base-colour texture; a Blender-only procedural effect or mesh attribute
is not a substitute for a game texture. Keep the editable mask and bake settings
in the source file. A working `EdgeWearMask` UV image has now been baked from the
56 selected faces. That image remains a selection mask, distinct from the actual
first-pass wear now composed into `Mammoth_BaseColor_2048.png`. The pass breaks up
selected chamfers with restrained wear variation and excludes exposed chips from
player recolouring. Preserve both the editable selection and the resulting paint;
a mask alone is not a substitute for the composed base-colour treatment.

### Texture-driven running gear

The user rejected the exposed hexagonal wheel pieces and requested wheel/sidewall
and tread-link detail as texture work, with animated tread textures. Preserve the
track shell's volume, ground contact and silhouette, but omit separate decorative
wheel and raised tread-bar meshes from the runtime asset. High-poly wheel/link
relief may exist solely as a bake source. Do not spend the freed runtime triangles
automatically.

Mechanically plausible running gear needs an intentional drive sprocket and
idler as well as road wheels. For texture-driven assemblies, model their relief
in the actual high-poly source and bake it; painted circles or source-object
names alone do not meet this requirement. Drive teeth, idler rims, wheel centers
and material selections must share the same authored layout, and changed source
positions require aligned paint masks and fresh normal/AO bakes.

Validate the assembled result with the armor skirt installed, in side and oblique
views, at close-up and representative gameplay scale. Check that the sprocket
and idler sit plausibly within the belt's end bends, the belt path and ground
contact agree with the visible silhouette, and road-wheel spacing leaves sensible
clearances without intersecting neighboring wheels, end supports or track links.
Keep intended recesses and partial occlusion, but ensure exposed mechanical
structure remains legible rather than leaving unsupported-looking belt bends.
The existence of a sprocket/idler in the high-poly collection, atlas or an
uncovered preview is not proof that it is visible or convincing in the assembled
asset. Inspect normal-only and complete-material views before acceptance; this
does not require adding separate runtime wheel geometry or changing the footprint.

The four closed track shells carry two `FLOAT`/`FACE` masks:
`track_sidewall_mask` selects the two longitudinal side caps for painted running
gear; `tread_scroll_mask` selects the wrapped belt surface for a separate animated
strip. Early `Track_*` prisms had eight belt faces and 28 total triangles each;
approved Lookdev05 uses **140 triangles per shell**, including 48 belt triangles
over 24 segments. Do not reuse an old eight-quad topology validator. The sidewall/wheel artwork must
not scroll along with the belt, nor should the armor atlas be used as a tread strip.
These tags are authoring data only. Lookdev05 assigns `Mammoth_Lookdev05_atlas` to
static sides and `Mammoth_Lookdev05_belt_L` / `Mammoth_Lookdev05_belt_R` to belts.
The earlier source used `Mammoth_RunningGear_Static` and `Mammoth_Tread_L` /
`Mammoth_Tread_R`. In the inspected importer, `fbxin.py --tread` selects one material name, not a
mesh attribute; these two authoring belt slots do not yet map to independently
animated runtime materials.

`TreadUV` follows actual arc length around each of the four closed loops, with its
seam at the top/rear corner under the armor. Each loop closes at seven repeats;
the physical high-poly source has six links per repeat. U runs across +X on both
sides; increasing V runs forward across the top, down the nose, rearward along
the bottom and up the rear. Sample `V - phase` for forward travel and verify the
sign visually. Keep static wheel images and the unique armor atlas out of this
scrolling sampler; use a separate repeatable tread tile.

For procedural tile colour, make the recipe periodic along the travel axis (for
example, noise evaluated on a sine/cosine circle). Do not require first/last pixel
rows to be identical: their pixel centers are distinct samples on either side of
the wrap. Compare the wrap gradient with ordinary adjacent-row gradients, then
inspect repeated, filtered and moving samples. A normal amount of texture grain
at the boundary is not by itself a visible seam; a periodic recipe alone does not
certify all mip levels or the final runtime sampler.

The historical target repeat pitch was 160/1024 = 0.15625 cell, with integer
closure giving 0.1574844665 cell on front loops and 0.1524510291 on rear loops of
that earlier geometry. Do not reuse those measured pitches after reshaping the
belt. Measure each final loop's circumference and divide by its repeat count. Correct travel is
`phase = signed_travel / effective_pitch`. Matching all four loops therefore
requires their actual pitches or a deliberately accepted fixed-pitch approximation.
Left/right phase parameters are prepared in Blender, but they have no animation
driver or runtime implementation. Bare elapsed time is not a valid vehicle tread
driver: a stopped tank must stop its belts, reverse travel must reverse phase, and
turning in place requires different left/right signed travel.

Code limitations recorded during the 2026-09-22 inspection follow. Recheck the
current implementation during actual tread integration; these are not promises
about future importer/runtime behavior:

- `project_tread_uv()` in `tools/art/fbxin.py` projects V from longitudinal pack Z,
  not distance around the track circumference. Constant-Z end faces collapse in V;
  proper motion around the front/rear bends needs verified UV handling, potentially
  preservation of a custom belt unwrap or an importer change. Do not promise that
  the current automatic projection produces a continuous wrapped belt.
- The importer accepts only one `--tread` material name and overwrites the chosen
  material's delivered UVs. Preserving `TreadUV` and independent L/R motion needs
  verified importer/runtime work, not just different Blender material names.
- `tread_advance()` in `game/cnc_eyes.cpp` accumulates unsigned vehicle travel
  distance. It does not rotate wheel images, reverse belt phase when reversing,
  or provide separate left/right motion during turning in place. Wheel rotation
  would be additional work, not a capability supplied by the current strip system.
- The importer uses `--treadpitch * --scale`, while runtime `TREAD_PITCH` is 160
  pack units. For an export still using one-cell coordinates with `--scale 1024`,
  `--treadpitch 0.15625` yields 160; the default 160 would be scaled again. Verify
  the actual output scale and UV repeat against runtime movement before acceptance.

Count **triangles, not polygons**: evaluate modifiers, call `calc_loop_triangles()`,
and sum every model mesh, including hidden/back faces and small details. Free
temporary evaluated meshes afterward. Compare against the requirement's current limit,
not an obsolete hard-coded limit from an earlier script.

Check for degenerate triangles, doubled faces, inconsistent normals, unintended
holes and visible intersections. Intentional component overlap at a bracket or
armor attachment is acceptable for game art; this is not necessarily a fused,
watertight, printable solid. Do not let that exception excuse visible clipping.

## 5. Fit the footprint without sacrificing the design

**Footprint compatibility is not a demand to copy the old model's geometry or
height.** Establish which dimensions are constrained before scaling every axis.
The user explicitly retained artistic freedom for this Mammoth.

Recommended sequence:

1. Preserve an approved artistic source and snapshot its coordinates.
2. Fit hull width/length and origin-relative front/rear extents in plan view.
3. Handle barrel overhang separately from hull length and turret massing.
4. Preserve or deliberately revise height based on appearance and real clearance.
5. Reposition assemblies with their actual pivots; record any runtime pivot override.
6. Review three-quarter, front, side, top, and rear views at consistent camera settings.
7. Recheck all component clearances and the triangle count after every proportion pass.

Do not run repeated nonuniform scaling against already-scaled geometry. Regenerate
from a source snapshot or a preserved source file so iterations do not compound
distortion. Save named variants instead of overwriting the approved starting point.

The first Mammoth fit matched the old overall height as well as the footprint.
Although numerically valid, it flattened the stance and made the guns read too
short. The user preferred an intermediate design: keep the accepted XY footprint,
restore half the lost vertical proportion, and move the turret slightly rearward
to recover barrel length while holding the muzzle tips at the same forward extent.
This is a reusable lesson: **bounds checks and artistic approval are separate gates**.

Resizing also exposed interference between missile pods and side railings. The
correction moved both pods outward, extended their supporting mounts, and checked
a positive gap to the rails while keeping the pods within the hull width. Check
assembly neighbors, not just each part against the hull or the overall bounding box.

## 6. Automation and reproducibility

For this session Blender was controlled through the installed MCP add-on's local
bridge at `127.0.0.1:9876`, using a small JSON socket client. The relevant messages
are `get_scene_info` and `execute_code` with a `code` field. Inspect the live scene
and current file before mutation; do not assume the last session is still open.

The add-on was enabled in Blender preferences. Connection availability is a live
state, not a durable guarantee: verify it rather than reinstalling automatically.
Keep the execution bridge local; it permits arbitrary Blender Python execution.

Procedural edits should be repeatable and narrowly scoped:

For an existing asset, open its normal saved `.blend` directly after checking the
live file and dirty state. Construction Yard review on 2026-09-22 verified that
`open_mainfile` preserves the working MCP connection without a factory reset.
Use a separate subsequent call for the owning-scene/evaluated-geometry audit;
write a hash-bound sidecar report without resaving the file being verified.

- Target the named asset collection/objects; never clear an unrelated scene.
- Upsert mesh data by name or regenerate only the known generated collection.
- Preserve source transforms or intentionally bake them, never mix the two silently.
- Save source vertex snapshots when later fitting will need repeatable interpolation.
- Assert triangle limits, bounds, and expected topology before declaring completion.
- Save a new `.blend`, a compact numeric report, and an inspection render.
- Keep scripts and paths portable when promoting a session helper into a shared tool.

The session helpers under `art/mammoth_lowpoly/` are working assets, not yet a
generalized cross-platform pipeline. Several use a Windows absolute output path.
The original verifier has a 1000-triangle limit and original-scale camera positions;
do not run it unchanged against the fitted 1500-budget revision.

## 7. Quality gates and delivery bundle

### Mandatory approval sequence

1. **First low-poly:** actual multiviews, evaluated count and original-footprint
   comparison; stop for approval before GPT Image generation.
2. **First GPT Image generations:** consistent labeled generated views and input
   provenance; stop for approval before high-poly.
3. **High-poly:** actual source views with physical panels/bolts/wheels and other
   required relief; stop for approval before final low-poly.
4. **Final mesh:** actual optimized mesh views, count, footprint and shading
   checks; stop for approval before final UVs/bakes/textures.

Every asset must pass each stop explicitly. Record version/date/user decision;
never infer unseen-stage approval from a general instruction to continue. Later
material/full-asset/export/runtime gates below are not replaced by these four.

### 0. Research, initial mesh and generated-view approval, before detailed high-poly

- Online research and the requirement distinguish geometry, finish and runtime limits.
- The original asset is identified in OpenCNC3D source, its complete posed footprint
  is measured, and a source-linked dimensional report/guide is preserved.
- An initial low-poly design mesh establishes proportions, footprint and part layout.
- Its actual multiview renders and the ArtStation finish reference are supplied to
  GPT Image; mesh file, rendered inputs and exact generation prompt are preserved.
- A consistent, versioned gritty multi-angle sheet is user-approved before the
  high-poly, final optimized low-poly and texture production.
- Cross-view part counts, orientation, proportions, paint and player regions agree.
- Exact prompts, input/output paths, provenance and approval amendments are saved.
- Silhouette, bake-only and texture-only features have an explicit allocation.
- The same selected sheet guides high-poly, final low-poly and textures; measured runtime constraints remain
  separate, with any needed concept adaptations documented.

### A. Geometry approval

- The detailed high-poly follows the approved views; the final optimized low-poly
  is deliberately derived/refined afterward, even when it reuses initial topology.
- Correct reference interpretation and user-approved silhouette/proportions.
- Evaluated triangle total at or below the requested ceiling.
- Ground contact and explicit, recorded axis/unit convention.
- Required original-versus-new footprint/overhang checks with origin-relative
  min/max deltas and documented numeric tolerance, independent from height decisions.
- No degenerate triangles or unintended visible intersections.
- Tracked running gear has plausible sprocket/idler support, belt-path contact
  and wheel clearances, inspected in assembled side/oblique views with armor on.
- Guns retain readable openings; launcher recesses and rails remain unobstructed.
- Turret/rotor hierarchy and pivot are sensible; test their motion separately.
- Neutral front, side, top, rear, and three-quarter inspection images.

### A1. Representative-section look-development approval

- Before repeating an unproven treatment across a whole asset, finish one section
  containing its important paint, relief, material transitions and player paint.
- Compare controlled art-authored/procedural paint against the AI-assisted paint
  on the same geometry, physical normal/AO maps, camera, exposure and lighting.
  Record geometry or lighting changes separately so they do not confound the A/B.
- Accepted panel joins, bolts and other structural details align across paint,
  physical high/height sources and normal/AO maps. Camouflage and ordinary dirt
  are not accidentally embossed.
- Derive source geometry and material masks from the same authored layout
  parameters. Measure visible relief in texels: important bevels should span
  several samples at the delivered resolution, not only in a larger working bake.
  Raising bake resolution alone does not fix a layout mismatch or ensure that
  subpixel relief survives the final delivered texture. Increasing normal
  strength is not a substitute for correctly positioned, adequately sampled relief.
- Shared semantic masks produce coherent paint, primer, steel, rubber and dust
  behavior in BaseColor/Roughness/Metallic and player-mask exclusions.
- BaseColor pigment value, tonal hierarchy and grit match the requested finish
  under held neutral lights/exposure; darker lighting or AO in the clean master
  must not substitute for the requested paint correction.
- Actual-model neutral normal-only and changing-light inspections pass, alongside
  close-up, gameplay-scale and at least two distinct player-colour comparisons.
- Obtain artistic approval of the representative result before propagating the
  recipe. Save rejected attempts and measured limitations as well as the winner.
- A generated source, successful bake or passed numeric check alone does not pass
  this visual gate or imply runtime integration.

### B. Export approval

- Separate triangulated delivery mesh per runtime rigid part, correct names/order.
- Evaluated geometry baked to the intended coordinates; no hidden parent transforms.
- Correct normals, UVs, material assignments, textures, and shading strategy.
- Clean PBR BaseColor and separate AO master; any AO-composited display/legacy derivative
  named and documented independently, with no double application of AO.
- Dielectric paint, bare metal and rubber remain distinguishable under neutral
  and reflection-readable inspection lighting at the chosen texture resolution.
- Player-colour mask for every asset/albedo pairing, packed and delivered with its
  required manifest fields; intentional all-black exceptions documented.
- At least two distinct player-colour previews preserve tonal detail with no mask
  bleed into fixed-colour materials or neighboring UV islands.
- If a tangent normal map is delivered, exact bake-target triangulation, UVs and
  loop normals retained; high-poly/cage/QA objects excluded from runtime parts.
- Actual exported FBX axes and scale verified, not merely Blender scene settings.
- Geometry report agrees with intended dimensions, role count, and triangle count.
- Texture, pivot, and pack checks completed beyond the limited `--report` path.

### C. Runtime approval

- Replacement draws at several hull headings and independent turret angles.
- Ground contact, clipping, shadows, muzzle effects, firing, and tread movement checked.
- House colours and viewing at normal game distance checked.
- Player-colour mask import/packing and tint behavior verified through the chosen
  runtime adapter, independently from the authoring previews.
- Factory emergence tested with the moving door and actual exit route.
- Original art remains recoverable; integration uses a separate verified pack first.

A handoff should include the editable `.blend`, delivery copy when ready, generation
or edit scripts, references/provenance, stats and fit report, preview images, and a
short README stating what has and has not been validated. Commit source/documentation
deliberately; avoid sweeping unrelated working changes or generated game packs into
an art commit. A local file or untracked helper is not a shared repository asset.
The bundle must include the explicit player-colour mask images, their albedo/UV
manifest pairings, and the two-or-more player-colour inspection previews.

## 8. Runtime/import facts and current limitations

Verified from current repository code on 2026-09-22. Recheck when tools change.

| Requirement or footgun | Implementation reference |
| --- | --- |
| Runtime scale is 1/1024; heading corrections are type-dependent | `game/cnc_eyes.cpp`: `MODEL_SCALE`, `g_meshForward`, `model_face_bias`, `draw_mesh` |
| FBX plus manifest preserves the reference contract better than OBJ | `tools/art/fbxout.py` |
| `--axis` is manual; importer does not infer unit scale or resolve arbitrary FBX axis settings | `tools/art/fbxin.py`: `axis_map`, `main` |
| `fbxout.py` writes Y-up; an unchanged direct roundtrip needs `--axis yup`; a Blender-exported file must be checked separately | `tools/art/fbxout.py`: `GlobalSettings` |
| Only each mesh node's own translations are applied; arbitrary parent transform chains are not evaluated | `tools/art/fbxin.py`: `node_offset`, `placed` |
| Nonidentity rotations/scales are refused; delivery must contain triangles | `tools/art/fbxin.py`: `node_offset`, `triangles` |
| Roles come from name substrings; parts are sorted by role/name; old pivots are reused by sorted part index, not the Blender origin | `tools/art/fbxin.py`: `role_of`, `main` |
| `--pivot PART=X,Y,Z` explicitly overrides a pivot in pack units | `tools/art/fbxin.py`: pivot assignment loop |
| `--fit` averages width and length scale ratios; it does not precisely fit both dimensions, recenter, or control height | `tools/art/fbxin.py`: `if a.fit` |
| `--report` returns before texture lookup, tread validation and pivot assignment; it is not a full acceptance test | `tools/art/fbxin.py`: `if a.report` |
| Every material needs its PNG or an explicit `--tex` mapping; Blender object display colours are not game appearance | `tools/art/fbxin.py`: texture import loop |
| Missing UVs silently become `(0,0)`; unsupported/missing normals can yield white shading | `tools/art/fbxin.py`: `triangles`, `shade` |
| Current importer regenerates vertex colour with `shade(normal, mode)`; it does not preserve authored FBX colours despite older documentation | `tools/art/fbxin.py`: `triangles`, `have_colour`, emitted `verts` |
| Vehicle shading does not consume tangent normal maps; `--shade normals` derives vertex shading from geometric normals | `tools/art/fbxin.py`: `shade`; `game/cnc_eyes.cpp`: vehicle drawing |
| Tier 1 textures require power-of-two dimensions, max 256 per side, and max 8:1 aspect; importer does not enforce the aspect limit | `docs/art-return-contract.md`; `tools/art/fbxin.py`: `load_texture` |
| A single `--tread` material replaces delivered track UVs with a repeating projection; an automatically generated strip is placeholder art | `tools/art/fbxin.py`: argument parser, `project_tread_uv`, texture import loop |
| House-colour variants are not supplied automatically; import drops node clips and writes one construction section | `tools/art/fbxin.py`: `mesh_record`; `docs/art-return-contract.md` |

For Mammoth delivery, consolidate into `HTNK_p0_static` and `HTNK_p1_turret`.
Do not export all independent authoring meshes and expect two runtime parts. Flatten
the export copy to world-rest geometry with identity rotation/scale; then assign
the correct explicit runtime pivot. A default Blender FBX exporter may introduce
axis transforms: inspect the output before asserting compatibility.

Conceptual geometry-report invocation, only after a real delivery FBX exists:

```text
python tools/art/fbxin.py <source.pack> HTNK <delivery.fbx> -o <separate-output.pack> --axis zup --scale 1024 --report
```

The `zup` and `1024` choices apply only if the exported coordinates still follow
this document's one-cell Blender convention. They are not universal FBX settings.
No delivery FBX or imported Mammoth pack has been validated in this session.

The `debug.pack` preview path can help integration, but has caveats: replacement
selection compares triangle data, so pivot-only changes may not activate a swap;
`muzzle_geom()` still reads/caches the original `g_pack`. A successful debug render
therefore does not prove the final replacement's muzzle effects are aligned.
See `load_debug_pack`, `draw_mesh`, and `muzzle_geom` in `game/cnc_eyes.cpp`.

Do not generalize this rigid-vehicle process into a finished building or skeletal
pipeline. Building construction sections and node animations need separate work.

## 9. Mammoth case study and measured contract

The progression below is a chronological record, not the order to repeat for a
new asset. Historical pending statuses and old budgets remain attached to their
checkpoints. **Lookdev05 is the user-approved authoring baseline**; use the
[current recipe](3d-model-recipe.md) and its two distinct low-poly stages for new work.

### Reference and progression

The design reference was the original Tiberian Dawn Mammoth render:
[Westwood render via CNC Communications](https://cdn.cnc-comm.com/gallery/command-and-conquer/Renders/MAMOTH.png).

The user selected
[Repulsor Executioner Blood Angel by Enclave Interactive](https://www.artstation.com/artwork/J9Z64d)
as the preferred hand-painted/PBR **finish** reference, after also supplying
[EXAR - VARGHULM by Maxim Buyanov](https://www.artstation.com/artwork/6LX5K5).
The finish reference does not replace the Mammoth's geometry, desert palette or
player-colour layout. The generated sheet uses this project's actual front,
side, top and rear renders as shape anchors; historical gray views contain
obsolete wheel protrusions that must not be reintroduced.

On 2026-09-22 the user approved
`references/Mammoth_MultiAngle_TextureDirection_v02.png` as exactly the desired
visual direction, then required red rocket tips. The selected amended sheet is
`references/Mammoth_MultiAngle_TextureDirection_v03.png`: it has been saved and
visually checked for recessed red tips at visible **forward-facing** launcher
mouths, solid rear covers, four track pods, twin guns and retained desert finish.
The companion v01/v02/v03 Markdown records retain the exact prompts, input images,
style attribution and revision history. The built-in generator's exact model
version is not exposed; it is not recorded as a verified version number.

Use v03 as the shared artistic target for model, high-poly and texture revisions.
The desired finish combines large readable sand/khaki paint masses, deliberate
hand-painted tonal variation, selective broken chips, dusty lower running gear,
dark machined gunmetal and controlled material-specific highlights. Preserve
ochre player panels as separate mask-driven regions. **Red rocket tips are a
fixed non-player-colour accent**, seated inside the existing six forward tubes
per launcher (two columns by three rows), surrounded by dark tube interiors;
they do not belong on the rear covers or across the whole launcher face. Exclude
them from the player mask and verify they remain red in both player-colour tests.
That sheet established approved concept direction, not evidence of implementation.
The later Lookdev05 actual-model approval is recorded separately below and does
not establish engine integration. The measured footprint,
pivot and envelope below remain the dimensional constraints. The historical
1500-triangle ceiling was subsequently raised by the user to **3000 triangles**
for the hybrid look-development revision; historical counts below are unchanged.

| Revision | Evaluated triangles | Purpose/status |
| --- | ---: | --- |
| Original low-poly | 984 / 1000 | Initial untextured silhouette |
| Detailed artistic source | 1384 / 1500 | Recessed hex launchers, octagonal guns/collars, driver's hatch, lights, rails |
| Initial OpenCNC3D fit | 1384 / 1500 | Correct XY footprint, but height looked too compressed |
| Balanced OpenCNC3D fit | 1384 / 1500 | User preferred intermediate proportions; pod/railing clearance subsequently corrected |
| Selectively chamfered OpenCNC3D model | 1496 / 1500 | Armor rims, front gun mounts, launcher face rims; footprint/pivot/gaps unchanged; edge-wear mask stored for later baking |
| Texture-driven track preparation | 1296 / 1500 | Removed eight wheel meshes and twelve raised tread bars; retained closed track shells with static-side/animated-belt face masks |
| High-poly source and working bakes | 1296 / 1500 runtime; 94,292 bake-source triangles | Unchanged low geometry, separate detailed source, UV/material roles and working normal/AO/mask bakes; final paint/export pending |
| Texture Pass 01 | 1296 / 1500, geometry/placement unchanged | 2K atlas and 512 tread paint, corrected gun normals, baked chamfer wear, explicit player masks and gold/red/blue previews; ready for user review, not runtime approval |
| Texture Pass 02 | 1296 / 1500, geometry/placement and UV hashes unchanged | Metal/roughness PBR refinement, clean BaseColor and separate AO plus user-approved BaseColorAO display, clearer paint/metal separation, reflection-lit review and explicit masks; runtime approval pending |
| Pass 03 shading preparation | 1296 / 1500, geometry/placement and UV hashes unchanged | New low corner normals, rounded armor bake sources and matched targets; normal rebake and new textures still pending |
| Generated reference v02 / selected v03 | No geometry count implied | User-approved texture direction / saved red-tip amendment; shared artistic target, not completed Blender art |
| ReferenceRebuild physical stage | 1440 / 1500 across 56 low meshes | Twelve inset rocket tips, replacement physical running-gear/service sources, 56 matched normal/AO pairs completed; textures, visual QA and final source-save recovery pending |
| Hybrid look-development authorization | Ceiling 3000; new evaluated count pending | User authorized the higher budget and hybrid workflow; representative front-pod A/B, aligned physical relief and actual-material acceptance required before whole-tank propagation |
| Approved Lookdev05 authoring baseline | 2888 / 3000 across 56 low meshes | Full physical/PBR/player-mask treatment, actual-model QA and fresh save/reopen verified; full-asset artistic approval on 2026-09-22, not export/runtime acceptance |

`Mammoth_TD_OpenCNC3D_TexturePass02.blend` is the preserved painted baseline, not the
approved final appearance. `Mammoth_TD_OpenCNC3D_TexturePass03.blend` is the preserved
incomplete shading-preparation file. The subsequent ReferenceRebuild physical
checkpoint is documented below. Pass 01, the bake-source file and earlier
geometry variants are preserved. Approval of the generated sheet alone did not
approve final Blender art; the user subsequently approved actual Lookdev05 in the
[identified approval record](../art/mammoth_lowpoly/LOOKDEV05_APPROVAL.md).
Runtime integration remains unapproved and unverified.

Original HTNK is model slot 38: hull display list `0xD7610`, turret `0xD6A18`.
The extracted/rest-posed count is 62 hull + 76 turret = 138 triangles. A legacy
`tools/bakery/sharecopy/assets/models_full/HTNK.obj` contains only the 62-triangle
hull and is not an authoritative complete-tank footprint reference.

Original measurements in pack units:

| Property | Value |
| --- | --- |
| Hull width | 713 |
| Hull length | 1020 |
| Hull X extents | -354 to 359 |
| Hull forward-Z extents | -533 to 487 |
| Overall forward-Z extents, including guns | -533 to 619.729759991169 |
| Overall height above ground | 389.072006225586 |
| Original turret pivot `(X,Y,Z)` | `(3.523619890213, 130.072006225586, 0.729759991169)` |

Balanced authored result, including the pod clearance correction:

- Hull footprint: 713 wide by 1020 long; overall footprint: 713 by 1152.72974.
- Blender bounds: X `[-0.345703125, 0.3505859375]`,
  Y `[-0.60520482, 0.5205078125]`, Z `[0, 0.43946955]`.
- Height about 450.01682 pack units: halfway between the initial fitted height and
  the artistic source's height at the matched width. Height is not locked to 389.
- Hull length relative to width increased about 15.33% from the artistic source.
- Turret moved rearward 0.035 cell relative to the first fit; guns gained visible
  length without changing the forward envelope.
- Revised pivot in pack units: approximately
  `(3.52361989, 222.71240234, -35.11024094)`.
  **Use this override for delivery, not the original default pivot.** Re-read the
  final fit report if another edit moves the turret.
- Each missile pod moved outward 0.035 cell; mounts extended at their outer ends.
  Minimum lateral gap from the side rails is about 0.00985976 cell on each side.
  Pods remain within hull width. Counts remain 1384 with no degenerate triangles.

The subsequent chamfer pass adds 112 triangles: 48 on the four outer track-armor
profile rims, 24 on the two front mantlet rims, and 40 on the two launcher face-plate
perimeters. Widths are respectively 0.006, 0.0045, and 0.0022 cell, all one segment.
Total evaluated geometry is **1496 triangles**. The exact overall/hull bounds,
turret pivot and 0.00985976-cell pod-to-railing gaps remain unchanged. All eight
edited component meshes are manifold and no degenerate triangles were found.
Three-quarter, front, side, top, and rear inspection renders were generated.

The running-gear revision removes 128 wheel triangles and 72 tread-bar triangles,
bringing the current model to **1296 triangles across 44 mesh objects**. All four
track shells remain closed/manifold. The overall footprint, height, turret pivot,
armor chamfers and 56 edge-wear mask faces are preserved. The old wheel/rib version
is recoverable from the saved pre-cleanup file. Front-three-quarter and side clay
previews were inspected after removal; texture animation is still pending.

The preserved `Mammoth_TD_OpenCNC3D_BakeSource.blend` retains those **1296 triangles and
44 low-poly meshes** without coordinate/topology changes. Its high-poly collection
contains **94,292 triangles across 101 meshes**: 32-sided barrels/collars,
microbevels, shallow panels and bolts, 12 wheel and 12 hub reliefs, plus 168 links
and 168 grousers around the four loops. Link/grouser counts are source details,
not additional game meshes. The main scene opens on the low-poly assembly;
high-poly sources and triangulated bake targets are hidden/non-export, and normal
inspection uses a separate `Mammoth | normal-bake QA` scene.

The initial 1024 normal/AO/mask outputs are drafts. The completed 2048 working
rebake uses refined hard-edge-split `BakeUV`; `bake_maps_report.json` records all
44 completed pairs and zero black covered normal pixels. Outputs are normal, AO,
edge-wear selection, detail selection and coverage, not final paint. The separate
512-pixel repeatable tread normal/AO tile also passed its source/coverage/seam
checks: normal first/last-row error is zero; AO maximum row error is about 0.007813.
These tile maps must use `TreadUV`/the repeat sampler, not the unique tank atlas.
Numerical bake checks do not replace visual approval at the delivered resolution.
The initial refined preview showed gun/collar waviness and dotted fine wheel
outlines. The weighted-normal trial did not resolve the gun defect. Read-only
corner-normal inspection found that cap/bevel averaging tilted long wall normals
from about -0.138 to +0.122 on a tapered gun whose correct axial component was
about -0.008615. The bake targets and QA copies had matching triangulation, UVs
and corner normals; that mismatch was not the cause.

`fix_cylinder_source_normals.py` corrects the four high-source gun/collar wall
normals analytically, including their shared smooth bevel boundaries, and rebakes
only those components into a new pass image. The report records 88,064 changed
pixels, maximum analytic normal error below 0.000074, no black interior projection
samples, unchanged other atlas pixels, and unchanged original working image/file.
Both low and high geometry are unchanged; the original bake-source file remains
preserved. The resulting gun highlights read straight in the first-pass previews.
Normal maps still cannot round the actual eight-sided silhouette.

Wheel edge relief remains smaller than a pixel: at about 540 texels per cell, a
0.0006-cell bevel occupies about 0.324 texel. Flat wheel caps parallel to the shell
correctly bake nearly neutral normals; only the thin rim carries relief. The first
paint pass therefore adds static painted hubs/rubber and AO separation without
boosting normal strength to compensate. Inspect this detail again at the intended
delivery resolution. `bake_source_verification.json` retains the verified
94,292-triangle source and unchanged 1296-triangle game-geometry contract.

### First texture pass, ready for user review

The first pass contains base colour, roughness, metallic and player-colour mask
images for the 2048-pixel `BakeUV` atlas and separate 512-pixel `TreadUV` tile.
It uses the corrected 2048 normal image and existing 512 tread normal/AO bakes.
Atlas AO is already composed into base colour at 34% once; the preview does not
multiply it again. Geometry-derived chamfer wear is baked into base colour, and
exposed chips are removed from the player mask.

The armor/static mask is `textures/pass01/Mammoth_PlayerColorMask_2048.png`;
the fixed tread tile has an intentionally all-black
`textures/pass01/Mammoth_Tread_PlayerColorMask_512.png`. Player regions are readable
upper pod bands, side service panels and the commander hatch, with a neutral
tonal base beneath them. The report counts 27,231 white mask pixels; tested fixed
parts have maximum mask sample 0. Gold, red and blue previews plus a side view
were generated from the decoded PNG materials. Red/blue comparison showed no
visible stray recolour or bleeding into guns, rails, running gear or camouflage
at that preview scale. This is authoring QA, not verification of the runtime
player-colour adapter or all mip levels.

The built-in image-generation tool supplied only a generic desert camouflage
paint surface. Blender projected/baked it with geometry-aware details, wear and
the existing normal/AO source; it did not replace the physical high-poly bake.
`textures/source/generation-notes.md` preserves the exact prompt and provenance,
and `Mammoth_DesertPaint_Source_v01.png` preserves the material input. Requested
source coverage/seamlessness is an artistic aim, not a mathematical guarantee;
the final atlas is uniquely unwrapped and the tread tile is checked separately.

`verify_texture_pass01.py` connected and packed 10 freshly decoded PNG images,
rendered the hero/red/blue/side views and wrote `textures/pass01/texture_manifest.json`.
The manifest records both texture sets, UV/dimension pairings, tint convention,
AO policy, intentional black tread mask and file hashes. The numeric UV audit
passes for 44 meshes/1296 triangles: zero sampled overlapping triangle interiors,
valid hard-normal UV splits, and four correctly separated static/belt material
roles. Its 2048-pixel center-sample test does not prove absence of subpixel overlap
or padding/filter bleed. `texture_uv_audit_report.json` and
`texture_uv_layout_2048.png` retain the audit and layout. Final art acceptance,
delivery-resolution QA, export and runtime integration remain pending.

### PBR texture pass 02, preserved work-in-progress baseline

The user selected metal/roughness PBR, then explicitly retained the baked-AO
aesthetic. `textures/pass02/` therefore contains **14 PNG images**: two complete
sets of BaseColor, BaseColorAO, Normal, Roughness, Metallic, AO and PlayerColorMask.
The atlas uses 2048-pixel `BakeUV`; the tread tile uses 512-pixel `TreadUV`.
BaseColor is the clean master. The displayed material uses BaseColorAO with 0.34
atlas / 0.42 tread AO influence baked once and no second shader AO multiplier.
Both colour variants and the separate AO remain available and packed.

`texture_pass02_report.json` verifies the unchanged 1296-triangle low assembly,
geometry/placement and UV hashes, and preservation of the Pass 01 file/images.
All seven maps per set are freshly decoded, correctly tagged and packed. Atlas
paint samples have metallic 0 and roughness about 0.47; gun samples have metallic
1 and roughness about 0.29. Rubber remains rough/nonmetallic, while exposed steel
and worn tread regions have a distinct reflection response. Existing corrected
normal maps and geometry-derived chamfer wear remain in use.

The explicit player mask pairs with both colour variants on identical UVs and
dimensions. The atlas has 26,859 pixels above 0.5; sampled fixed-colour components
remain zero, and the tread mask remains intentionally all black. Gold, red and
blue views were rendered. Red/blue inspection found no visible stray tint on
guns, rails, rubber, camouflage or exposed chamfers at preview scale. This does
not certify all mip levels or the pending engine player-colour adapter.

`Mammoth | PBR Pass 02` supplies reflection-readable inspection, with broad area
lights and Blender's packed `studio.exr` environment. The bundled environment is
credited to Greg Zaal / Poly Haven under CC0; it is inspection lighting, not a
runtime asset texture. The final authoring scene uses key/fill/rim strengths of
38/16/44, environment strength 0.18 and Sun strength 0.35; these are scene-specific,
not pipeline defaults. Hero, side and main-scene Eevee previews accompany the
red/blue checks. The gun reflections read straight and round, with tighter metal
highlights than the painted armor. The eight-sided geometry silhouette is unchanged.

`pbr_pass02_materials.py`, `texture_mammoth_pass02.py` and `finish_pbr_pass02.py`
retain the recipes, baking, decoded-map checks, lighting and preview setup.
`textures/pass02/texture_manifest.json` records all 14 paths/hashes, colour spaces,
normal convention, AO policy and explicit player-mask pairings. A fresh numeric
UV audit in `textures/pass02/qa/` passes for 44 meshes / 1296 triangles with zero
sampled overlapping interiors, errors or warnings; the audit leaves persistent
data/UI unchanged. This is still a pixel-center test, not exact/subpixel overlap
or padding/filter proof. The tread colour recipe is mathematically periodic.
Its clean linear-colour wrap mean/max differences are about 0.01372/0.06448,
versus ordinary adjacent-row median mean/max differences of 0.01262/0.07178.
The wrap lies at about the 78th percentile by mean and 17th by maximum: no
exceptional numerical boundary gradient was found; the AO derivative is similar.
Roughness, metallic and player-mask edge rows match exactly. Repeated motion and
mip/filter appearance still need normal delivery/runtime acceptance checks.
Final artistic approval, production-resolution checks and runtime integration
remain pending.

### Preserved Pass 03 preparation

The user found the armor edges too hard, the running gear unconvincing and the
material detail insufficient. The newer real-wheel photo and approved generated
sheet supersede any earlier assertion that painted wheel circles were adequate.
Use readable rubber tires, recessed/perforated wheel dishes, hubs/bolts, sprocket
and idler structure and chain detail in the high-poly/texture design. Preserve
static side artwork versus the independently scrolling belt; do not restore
decorative low-poly wheel discs merely because a reference shows physical wheels.

`prepare_pass03_shading.py` and `pass03_shading_preparation.json` record the work
actually completed: selective custom low corner normals on eight chamfered armor
components, radial collar-wall normals, rounded high-source armor and matched
triangulated bake targets. Low coordinates, topology, world placement and UV hashes
remain unchanged at 1296 triangles. Broad panels remain planar while bevels and
round walls receive suitable shading. The existing Pass 02 file and user lighting
were preserved. **A matching normal rebake is still required:** old image normals
must not be accepted against the changed low-poly shading basis.

At this preserved checkpoint, `pass03_running_gear.py` and
`pbr_pass03_materials.py` were prepared recipes, not evidence of completed sources
or maps. The later ReferenceRebuild below executes the physical source/bake work;
it does not retroactively make the older Pass 03 file a completed texture pass.
Do not report the generated concept or source preparation as a finished asset.

### ReferenceRebuild: verified geometry and physical-bake checkpoint

The user's request to redo the actual model from selected v03 starts a separate
`Mammoth_TD_OpenCNC3D_ReferenceRebuild.blend` variant. At this checkpoint,
`rebuild03_geometry_report.json` records **1440 triangles across 56 low meshes**:
the existing 1296-triangle assembly plus twelve closed, 12-triangle rocket-tip
cones. Six tips sit inside each existing forward launcher in two columns and
three rows, with the nose about 0.002302 cell behind the mouth. They inherit the
turret hierarchy and are tagged as fixed red, excluded from player paint; the
image-mask and two-colour visual checks remain to be completed.

The report verifies unchanged vertices, topology, UVs and corner normals on the
original 44 meshes, and exactly unchanged overall world bounds:
X `[-0.345703125, 0.3505859375]`, Y `[-0.6052048206, 0.5205078125]`,
Z `[0, 0.4394695461]`. The new tips intentionally share one reserved 128-pixel
atlas region at `(763, 1401)` with 8-pixel padding in the 2048 atlas; the original
44 UV layouts remain unchanged. This is a documented identical-part UV-sharing
exception, not permission to ignore accidental overlaps in an atlas audit.

`rebuild03_running_gear_report.json` records **76 new bake-only source meshes /
352,896 source triangles**. Both caps of the four shells have physical rubber
tires, perforated recessed wheel dishes, hubs and bolts, toothed sprockets,
idlers, return rollers and segmented chain-side plates/pins. Its counts include
inner as well as outer caps: 24 road-wheel reliefs, 144 wheel holes, 144 wheel
bolts, 8 sprockets, 8 idlers, 16 return rollers, 336 side plates and 672 pins.
These are not runtime wheel meshes or evidence of animated wheels. The low
shells, static-side artwork and independent scrolling-belt design remain intact.
The geometry detail recipe also supplies bake-only rear/deck louvers, service
panel frames/bolts and hatch relief; its separate source total is 14,416 triangles.

`rebuild03_physical_bake_report.json` verifies **all 56 matched component pairs**
using 166 selected physical source objects. The 2048 normal/AO/coverage bake took
about 404 seconds, found **0 black covered normal pixels** and 270,816 covered
pixels with varied normal data. AO is component-local, not permanent shadows
between independently moving assemblies. The normal convention is tangent
`+X/+Y/+Z`, Non-Color, against the exact `BAKE03__` triangulation, UVs and corner
normals. The resulting files are under `textures/rebuild03/`, including
`Mammoth_Normal_Physical_2048.png`, `Mammoth_Normal_2048.png`,
`Mammoth_AO_2048.png` and `Mammoth_Coverage_2048.png`. Zero black samples is a
projection diagnostic, not proof that the relief looks good or matches v03.

Verified implementation lessons from this stage:

- Physical dish holes and recessed backing must exist in the source if those
  recesses are expected in a normal/AO bake. A flat covering cap can occlude the
  relief. This source uses a 0.010-cell backing inset; paired track projection
  uses cage extrusion 0.020 and maximum ray distance 0.045. These are measured
  case settings, not universal bake defaults.
- Account for both inner and outer source caps when reporting wheel/link counts;
  do not mistake doubled bake-side detail for runtime geometry or silhouette.
- Add a late repeated detail through a measured free UV region or a deliberate
  shared layout, retain existing UVs where possible, and record the exception
  for subsequent overlap/player-mask audits. Preserve exact normal-bake targets
  rather than retriangulating the editable source for the preview.
- A completed bake/report and successful PNG saves do not prove a successful
  `.blend` checkpoint. The backup/rename save step in this run failed and left a
  `.blend@` temporary beside the earlier preserved `.blend`. Do not delete the
  previous file or treat the temporary's existence as verification of recovery;
  recover and validate a normal `.blend` save separately.

**Still pending at this checkpoint:** decoded paint/roughness/metallic/mask
outputs, any optional SurfaceHeight contribution, material and multi-view visual
QA, fixed-red-tip/player-colour comparisons, final source-save recovery, artistic
approval, delivery-resolution checks and runtime integration. The proposed
isolated QA scene must use its own lights/world/material copies and preserve the
user's authoring controls. No completed texture bundle or visual match is implied
by the physical-bake statistics above.

### Hybrid revision: authorized experiment, not a completed recipe

The user subsequently found the actual model and atlas substantially simpler than
the ArtStation finish reference and the approved generated sheet. The next
authorized approach combines up to 3000 runtime triangles with a representative
front-pod paint experiment. Extra geometry is for useful silhouettes, bevels,
openings and plate thickness, not automatic restoration of decorative wheel discs
or hundreds of independent tread links. Keep the prior revision recoverable and
preserve the measured footprint and moving-part clearances.

`art/mammoth_lowpoly/rebuild04_lookdev_plan.md` defines the bounded comparison:
controlled geometry-aware paint versus AI-assisted armor paint on the same pod,
using the same physical normals/AO and lighting. The approved v03 sheet guides
the finish, while actual geometry guides the projected paint placement. Accepted
joins and bolts must be reproduced in aligned high-poly or controlled height/AO
sources; new albedo art does not certify that this has happened. Shared named
material masks must control paint/primer/steel/rubber/dust response and player
exclusions. Neutral normal-only, changing-light, close-up, gameplay-scale and
two-player-colour checks remain required. These statements record the current
requirements and planned validation, not completed outputs or artistic approval.

#### Verified first normal-only study failure

The isolated front-pod study contains 56 armor triangles plus 140 track triangles
(196 total); its 56-mesh, 1488-triangle main source was left unchanged. These are
study/baseline counts at this checkpoint, not a completed whole-tank 3000-triangle
revision. `textures/lookdev04/geometry_benchmark.json` records the candidate
scope. The initial 1024-pixel physical bake used 26 matched high-source objects
and reported zero black covered normal pixels, but its actual neutral normal-only
render revealed a fragmented service-panel outline and tiny, poorly resolved bolts.
The rejected bake and render are preserved under
`art/mammoth_lowpoly/textures/lookdev04/pre_detail_alignment/`, including
`physical_bake.json` and `Section_04_NormalOnly_Key.png`.

Read-only measurement found about 1210 pixels per cell on the outer armor face.
The old physical service panel had half-size `(0.0675, 0.018)` cell, while the
paint selection used `(0.0695, 0.020)`: a 0.002-cell boundary mismatch on each side.
The source panel's 0.0005-cell bevel occupied about 0.61 texel, and the bolt's
0.00025-cell bevel about 0.30 texel. This verifies both an alignment defect and
under-sampled relief, despite the successful projection statistic. Source-layout
and relief-width corrections are in progress; no corrected visual result or
artistic acceptance is claimed at this checkpoint.

The reusable lesson is to share one authored layout between physical sources and
material masks, then measure bevel widths against the actual delivered texel
density. Require the normal-only render as a gate: neither zero missed/black rays,
more bake samples, a larger working image alone, nor stronger normal shading
proves that small structural details are aligned and readable.

#### Corrected-source and resolution-control follow-up

The next study pass replaced only the private high-source service panel and four
bolts in the bake selection, retaining the five previous sources and the main
asset. `textures/lookdev04/high_detail_alignment.json` records the matched panel
half-size `(0.0695, 0.020)` cell, wider 0.0018-cell panel rounding and 0.0009-cell
bolt rounding. The corrected 1024 comparison is preserved in `aligned_1024/`.
A subsequent 2048 working bake used the same 26-source paired setup and reported
zero black covered normal pixels (about 41 seconds in the current
`textures/lookdev04/physical_bake.json`). Actual inspection of
`textures/lookdev04/qa/Section_04_NormalOnly_Key.png` shows a substantially more
continuous, readable panel and bolts, with reduced wheel-rim speckling relative
to the 1024 control. This verifies a useful source-alignment and working-sampling
improvement, not final artistic approval. The 2048 image covers only the study
section: full-tank texel density, delivered texture budget, reduced-resolution
readability, paint/PBR acceptance and runtime integration remain pending.

#### Initial hybrid paint/PBR trial: tested section, further art refinement

The first actual Cycles paint trial used freshly decoded, packed maps on the
196-triangle section. `textures/lookdev04/qa_report.json` records the A/B control:
geometry, physical normal/AO, roughness, metallic, player mask, lights and camera
are identical; only armor paint changes. `textures/lookdev04/manifest.json`
records image paths/hashes, color spaces, `BakeUV` pairings and the explicit
player mask, plus the separate 512-pixel `TreadUV` set with its intentionally
all-black mask. Inspected initial A/B, reverse-light, red/blue and 240x140 section
views confirmed that pins, bolts and rims respond to changed lighting and player
color remains on the service plate without visibly tinting its fixed hardware at
that preview scale. These are actual Blender checks, not runtime validation.

The source-paint A/B difference is visually modest; this experiment does not
establish that AI paint is inherently superior or that either version meets the
reference. The initial wear was too pale/soft and is retained with its maps,
`paint_bake.json` and two actual A/B renders under `paint_trial_v01/`. A follow-up
with crisper broken wear, darker undercoat, more selective exposed contact steel
and inset player-paint loss was subsequently rebaked and re-rendered. Inspection
of the refined normal-only, complete-material, reverse-light, red/blue and small
section views confirms readable plate/bolt relief, changing hardware/contact
reflections and player-paint exclusions at those preview scales. The small dark
steel spots change value with the light; they are not missing normal samples.
The refinement is still not reference-quality artistic approval: the coat remains
pale, metal wear is rather dotted/busy, wheels are too clean/graphic, and selected
long scratches look schematic. Those are explicit next art targets, not reasons
to silently promote the study across the tank. Artistic acceptance, final
full-tank texel density and texture budget remain pending: the working 2048 map
pair covers only this section.
The main 1488-triangle assembly is unchanged; the authorized 3000-triangle
whole-tank revision is not a completed or integrated delivery.

`Mammoth_TD_OpenCNC3D_Lookdev04.blend` was saved and reopened. The read-back audit in
`textures/lookdev04/reopen_verification.json` checks all 56 original meshes and
world matrices against the pre-study snapshot, the unchanged replacement envelope,
original light powers/render visibility, packed map hashes/dimensions and grayscale
player masks. The 512-pixel tread mask remains intentionally all black. Editable
semantic recipe materials are retained with fake users. The study's normal PNG is
the physical structural bake; separate `SurfaceHeight` adds only shallow selected
paint loss in the preview shader. A single combined runtime normal/export material
has not been delivered or validated.

Read-back lesson: evaluate an object's owning scene before auditing its world
matrix. Immediately after reopening into the isolated study, dormant parented tank
objects initially reported unevaluated matrices. Activating/updating the original
scene restored correct evaluated values, which matched the snapshot exactly; no
transform edits were needed. Never "fix" geometry from a stale dependency-graph
read. Keep camera guide matrices explicit for the same reason.

#### End-support visibility correction: representative section

The next assembled-study review exposed three road wheels but no readable end
supports. The physical source did contain them: the sprocket center was at
Z 0.106 cell with radius 0.034 (bottom 0.072), and the idler at Z 0.103 with radius
0.035 (bottom 0.068). The armor skirt's bottom around Z 0.061-0.068 hid these
details in the assembled view. Their presence in the earlier source/atlas did
not satisfy the new assembled-running-gear gate.

The isolated Lookdev04 section now uses the shared, measured layout in
`art/mammoth_lowpoly/lookdev04_track_layout.py`. Its three road wheels remain
unchanged; the sprocket moves to Y/Z (-0.4235, 0.054), body radius 0.029 cell,
and the idler to (-0.060, 0.054), radius 0.0335. Both caps receive matched physical
sources and paint/roughness/metallic selections. Twelve separately retained,
beveled source teeth make the sprocket distinct; the former Boolean result had
lost its intended tooth extensions. The minimum main-wheel gap is 0.001435 cell.
End-wheel envelopes meet the measured inner chain-side band without leaving the
track shell. This is a conservative lower-bend fit, not a simulated drivetrain.

Fresh normal/AO and six material channels were baked for the unchanged 196-triangle
study mesh and its existing UVs. Actual armor-on side and oblique renders show
three road wheels plus readable sprocket/idler; normal-only companions confirm
physical relief. Red/blue player previews keep running gear fixed-color. Inspect
`Section_04_B_EndSupports_Side.png`, `Section_04_NormalOnly_EndSupports_Side.png`
and `Section_04_B_AI_Paint.png` under `textures/lookdev04/qa/`. Numerical layout,
source cleanup and save/reopen reports accompany the maps. A scale-induced
collapsed Boolean quad diagonal was repaired with explicit source triangulation,
then rebaked; do not lower the degenerate-face gate to hide the problem.

The previous study is recoverable as
`Mammoth_TD_OpenCNC3D_Lookdev04_before_end_support_fix.blend` and the matching
`textures/lookdev04/before_end_support_fix/` maps/renders. No low-poly wheel
objects were added. The original full tank and footprint remain unchanged.
Rear-pod shapes still require their own fit and assembled-view checks when this
treatment is propagated; do not blindly reuse front-pod coordinates. Whole-tank
propagation, final texture quality, artistic approval and runtime acceptance
remain pending.

#### User finish amendment: darker, grittier pigment before propagation

After the end-support correction, the user again identified the base color as
too pale and clean compared with the ArtStation `J9Z64d` finish reference. The
current direction is to darken and enrich the actual paint and localized grit
on the representative section **before** extending the recipe across the tank.
Retain the desert/khaki palette, explicit player masks and fixed red rocket tips;
the reference supplies finish craftsmanship, not a new red armor scheme.

`textures/lookdev04/source/ArmorPanel_Paint_AI_v02_DarkGritty.png` and its companion
Markdown record preserve a generated pigment-only source and exact prompt. It is
approved as the darker/grittier pigment direction: the user responded "Much
better!" to that source image. This amendment takes precedence over the paler
paint values in the earlier multi-angle concept; retain that sheet's approved
design, camouflage family and red-tip layout. Use the darker panel source as the
finish anchor when generating the remaining Mammoth surfaces, not as a generic
darkness multiplier for unrelated assets. This does not approve an actual Blender mapped
result, normal/AO map, completed atlas or whole tank.

The material-only section has now been baked and saved/reopened with geometry,
corrected running gear, physical normal/AO, QA lights and exposure held fixed.
The AI side pigment uses gain 1.0. Controlled top/nose continuation uses a 0.38
linear multiplier (previously 0.92); a 0.60 trial left those surfaces too pale
relative to the revised side. Restrained authored lower/handling grime shares
base-color, roughness and metallic/player exclusion selections, not height.
The unchanged normal/AO file hashes and current packed-map/player-mask checks
are recorded in `dark_gritty_paint_revision.json` and `reopen_verification.json`.

UV-selected albedo measurements confirm a pigment change, not a lighting trick:
mean linear luminance on side paint changes from 0.218 to 0.084, and top/nose
paint from 0.249 to 0.107. These are linear texture measurements, not percentages
of perceived render brightness. See `dark_gritty_albedo_metrics.json`. Actual
same-light, reverse-light, armor-on side and red/blue previews were reviewed;
the bright studio setup still produces lighter illuminated top planes. This is
not a claim of matching the reference's entire lighting or finish quality.
The prior end-support checkpoint is `Mammoth_TD_OpenCNC3D_Lookdev04_EndSupports.blend`,
with pre-darkening maps/renders in `textures/lookdev04/before_dark_gritty_paint/`.
The user subsequently authorized whole-tank propagation (Lookdev05) with
"Go ahead and apply all of these learnings to the rest of the tank." This
authorizes applying the recipe; it is not advance approval of the final full
asset, exported files or an in-game result.

### Lookdev05: user-approved full-tank authoring baseline

Following the user's whole-tank authorization, the separate Lookdev05 variant
contains **2888 triangles across 56 low meshes**, below the 3000 ceiling. The
1488-triangle source remains preserved. `textures/lookdev05/geometry.json`
records unchanged bounds X `[-0.345703125, 0.3505859375]`,
Y `[-0.6052048206, 0.5205078125]`, Z `[0, 0.4394695461]`, the unchanged turret
pivot `(0.003441035049, 0.034287344664, 0.217492580414)` and retained
0.00985976-cell pod/railing gaps. The final authoring source is
`Mammoth_TD_OpenCNC3D_Lookdev05.blend`, saved with Eevee, three scene lights and a
GDI-gold player-colour preview. The `_Working.blend` is a geometry checkpoint.

The **178 matched high-poly source meshes** include physical wheel dishes,
sprockets/idlers, chain detail and aligned service plates/bolts. Each track cap
has three road wheels, a sprocket, an idler and two return rollers; rear end
supports are fitted to the shorter rear shell rather than copied blindly from
the front. Round-source guns use 64 radial segments, collars and launcher tubes
48. These are bake-source counts, not additional runtime wheels or weapon
segments. The runtime track volume remains a closed shell with separate static
sidewall and scrolling-belt face roles.

The final low triangles were frozen before correcting the launcher/atlas unwrap
and baking. The 4096-pixel atlas audit in
`textures/lookdev05/uv/texture_uv_audit_report.json` records **zero sampled
overlapping interiors** and approximately 54.9% sampled coverage. This does not
prove subpixel separation or filtering/dilation safety. Its legacy tread check
is not a pass: prefixed identities, 48 belt triangles per shell and temporary
shared bake materials require the separate `lookdev05_uv_audit.py` check after
final distinct L/R materials are assigned. Do not reuse an eight-quad topology
test for the new 24-segment belt. The final and reopened audits under `uv_final/`
and `uv_reopened/` now pass the proper triangle-aware atlas/tread checks: zero
sampled overlap, unchanged hard-corner seams, affine arc-length belt UVs, seven
repeats, complementary static/belt faces and distinct consistent L/R materials.

`textures/lookdev05/physical_bake.json` records all **56 matched component pairs**
completed at 4096, with **zero black covered normal pixels**. Normal and
component-local AO are real physical bakes against the frozen tangent basis.
The early background hero/side renders and `qa_early/early_normal_report.json`
show smooth major gun highlights, rounded chamfers, service relief and visible
end supports under the armor. Fine plate outlines and chain pins remain slightly
grainy at that full-asset density. A larger whole-tank atlas does not automatically
retain the earlier section-only texel density; zero black samples is not visual
approval. The background inspection left the working file hash unchanged.

The completed paint recipe uses AI images **only for pigment**, with independently
authored shared semantic masks for paint, primer, exposed steel, rubber, dust,
hardware and player exclusions. Delivered outputs are clean `BaseColorB`, separate
Normal/AO/Roughness/Metallic/PlayerColorMask and bounded paint-loss `SurfaceHeight`;
the preview applies AO once at 28%. The physical normal image does not contain
the shallow paint-loss height: the authoring shader combines that separate map
using 0.0005-cell bump distance. Do not claim that a combined runtime normal was
delivered. All scalar RGB channels decode to grayscale, the height stays within
`[0.487999, 0.500008]`, and near-white player-mask pixels have metallic values
below 0.01. These correlations do not prove every filtered boundary.

All **13 texture images** are packed and retained externally: seven 4K atlas
channels paired to `BakeUV`, and six 512 belt channels paired to `TreadUV`.
The belt's explicit player mask is intentionally all-black. All 12 red tips
remain outside recolouring. `textures/lookdev05/manifest.json` records actual
file hashes, colour spaces, mask/albedo pairings, sources, prompts and evidence.
`reopen_verification.json` confirms every packed payload matches its external PNG,
exact saved mesh/UV/corner-normal/material state, 178 hidden high sources, the
unchanged pivot/bounds and the unchanged previous Lookdev04 file hash.

Actual hero/rear/side/top/front, reversed-key, normal-only, red/blue/gold,
independent-belt-offset and gameplay-proxy renders are under `textures/lookdev05/qa/`.
Changing light changes highlights; intended markings recolour without obvious
spill into inspected wheels/rubber/metal/tips. Opposite L/R phase stills demonstrate
separate controls, not continuous animation or correct runtime travel. Fine plate
edges/pins remain grainy, some chamfer wear reads chalky, and lit roofs/barrels
remain cleaner/brighter than the aspirational finish reference. `visual_review.json`
records these nonblocking art-review notes. Technical QA is not user approval.
The user subsequently approved the completed result on 2026-09-22: “This looks
great. I think we have a receipe.” The separate
[approval record](../art/mammoth_lowpoly/LOOKDEV05_APPROVAL.md) binds that decision
to the exact saved file, manifest and gold preview. The original manifest and
QA/delivery/reopen reports retain their pre-approval status as immutable historical
evidence; the later record supersedes only their artistic-approval status for
those identified bytes, not their tests or outstanding runtime gates.

Save/recovery lesson: leave enough disk space for Blender's temporary save beside
the existing file. The final normal/UI save hit a nearly full C: drive; it was
saved on D:, hash-checked and copied back with a preserved checkpoint, then the
project-folder file was freshly reopened and verified. Do not delete unrelated
files or treat an operator's vague error as successful persistence. Verification
must not rewrite the very file it is verifying. The 4K atlas is still an
authoring resolution, not an approved runtime budget; no export, integration or
factory-exit acceptance is implied.

### Factory clearance is a separate check

Gameplay uses a 1x1 footprint for non-buildings; that does not define visible mesh
width/length (`brain/vanilla/tiberiandawn/dllinterface.cpp`, footprint reporting).

The War Factory waits for its door and drives the produced unit along the scripted
`OUT_OF_WEAPON_FACTORY` route/Track13, not a triangle-mesh collision test. See
`building.cpp` factory exit logic and `drive.cpp` Track13. The concern is visible
clipping, not art physically blocking the route.

A read-only measurement of the source factory found a front opening span of about
1054 model units near height 798; this suggests room for the balanced 450-high tank.
It is not a proof of clearance along the diagonal route or through moving doors.
**The final in-game exit test remains outstanding.**

### Working session outputs

These are local files under `art/mammoth_lowpoly/`; verify they exist and are
versioned/shared before depending on them on another machine. Documentation edits
do not themselves publish the Blender asset bundle. The documentation work is on
`a development branch`; branch presence is not evidence that local changes or
large asset files have been committed or published. Rows for older passes describe
their preserved checkpoint state, not new work required on approved Lookdev05.

| File | Purpose |
| --- | --- |
| `Mammoth_TD_Lowpoly.blend` | Preserved 984-triangle source |
| `Mammoth_TD_1500.blend` | Preserved detailed artistic source |
| `Mammoth_TD_OpenCNC3D.blend` | Initial, flatter footprint-fit variant |
| `Mammoth_TD_OpenCNC3D_Balanced.blend` | Preserved approved proportions before chamfering |
| `Mammoth_TD_OpenCNC3D_Chamfered.blend` | Preserved 1496-triangle model before wheel/tread-rib cleanup |
| `Mammoth_TD_OpenCNC3D_BeforeTrackCleanup.blend` | Exact pre-removal recovery copy |
| `Mammoth_TD_OpenCNC3D_TrackShells.blend` | Preserved 1296-triangle geometry-only source with edge-wear/track masks |
| `Mammoth_TD_OpenCNC3D_BakeSource.blend` | Preserved low/high source, UV/material roles, hidden bake targets and separate normal QA scene |
| `Mammoth_TD_OpenCNC3D_TexturePass01.blend` | Preserved first painted pass, unchanged game geometry, packed decoded PNG materials and separate texture-pass scene |
| `Mammoth_TD_OpenCNC3D_TexturePass02.blend` | Preserved PBR baseline with 14 packed texture maps; further art refinement and runtime integration pending |
| `Mammoth_TD_OpenCNC3D_TexturePass03.blend` | Incomplete shading/source preparation; matching normal rebake and new textures pending |
| `Mammoth_TD_OpenCNC3D_ReferenceRebuild.blend` | New v03-driven revision; 1440-triangle physical stage verified, final bake-state save recovery and texture/visual QA pending |
| `rebuild03_geometry_report.json` / `rebuild03_running_gear_report.json` | Twelve recessed low rocket tips and detailed non-export running gear; unchanged original geometry/UVs and measured envelope |
| `rebuild03_physical_bake_report.json` / `textures/rebuild03/Mammoth_{Normal_Physical,Normal,AO,Coverage}_2048.png` | Completed 56-pair physical bake; zero black covered normal pixels, visual acceptance still pending |
| `references/Mammoth_MultiAngle_TextureDirection_v01.md` | Initial generation prompt, actual-model input renders and finish-reference provenance |
| `references/Mammoth_MultiAngle_TextureDirection_v02.png` / `.md` | User-approved visual direction and rear-launcher correction prompt; preserved |
| `references/Mammoth_MultiAngle_TextureDirection_v03.png` / `.md` | Selected, visually checked red-tip amendment and exact prompt; common artistic target, not a Blender render |
| `prepare_pass03_shading.py` / `pass03_shading_preparation.json` | Verified unchanged geometry/UVs, updated corner normals and rounded sources; new normal bake required |
| `pass03_running_gear.py` / `pbr_pass03_materials.py` | Prepared recipes; replacement-source execution and texture baking not yet verified |
| `blender_bridge.py` | Local add-on socket client |
| `lookdev05_geometry.py` / `lookdev05_build.py` | Full 2888-triangle variant, matched high sources, frozen-basis UV and physical/paint bakes |
| `lookdev05_materials.py` / `lookdev05_qa.py` / `lookdev05_uv_audit.py` | Shared material selections, actual multiview/PBR/mask checks and triangle-aware atlas/belt validation |
| `LOOKDEV05_README.md` / `lookdev05_manifest.py` / `textures/lookdev05/manifest.json` | Authoring contract, hash-backed 13-file pairing/provenance manifest; not runtime delivery |
| `LOOKDEV05_APPROVAL.md` | Dated user artistic approval bound to exact Lookdev05 file/manifest/preview hashes; preserves earlier pre-approval reports |
| `textures/lookdev05/reopen_verification.json` / `visual_review.json` | Exact reopened low state and packed payload checks; technical visual review and remaining art limitations |
| `build_mammoth.py` / `upgrade_mammoth_1500.py` | Initial geometry / detail generation |
| `extract_opencnc3d_reference.py` | Original posed HTNK extraction through project readers |
| `references/opencnc3d_htnk_geometry.json` | Original posed geometry, bounds and precise pivots |
| `fit_mammoth_opencnc3d.py` | Repeatable fit from original coordinates, balanced proportions and pod gap checks |
| `opencnc3d_balanced_report.json` | Current counts, bounds, pivot override and clearances |
| `mammoth_opencnc3d_balanced_preview.png` | Preserved pre-chamfer clay render |
| `chamfer_mammoth.py` | Repeatable selective beveling, preserved source meshes, clearance/count checks, wear mask |
| `opencnc3d_chamfer_report.json` | Chamfer costs, final bounds, triangle count and wear-mask metadata |
| `mammoth_opencnc3d_chamfered_*.png` | Chamfered inspection renders |
| `prepare_textured_tracks.py` | Narrow wheel/rib removal, closed track shell checks and texture-role tagging |
| `opencnc3d_track_shells_report.json` | Current geometry counts, footprint and texture-preparation status |
| `mammoth_opencnc3d_track_shells_*.png` | Current cleaned-up track previews |
| `build_bake_source.py` / `bake_source_report.json` | Initial high-poly/UV/material preparation and source-preservation checks |
| `add_highpoly_treads.py` / `highpoly_treads_report.json` | Physical link/grouser bake source; authoritative 94,292 high-poly total |
| `tread_uv_layout.py` | Arc-length belt UVs, sidewall/L/R material roles, measured repeat pitches |
| `refine_bake_uv.py` / `bake_uv_refinement_report.json` | 196 hard-edge UV splits, 0.008 atlas margin, draft-UV backup |
| `bake_mammoth_maps.py` / `bake_maps_report.json` | Paired normal/AO/selection-mask baking and working-map checks; inspect report resolution |
| `bakes/Mammoth_*_1024.png` | Initial working drafts; not production textures |
| `bakes/Mammoth_{Normal,AO,EdgeWearMask,DetailMask,Coverage}_2048.png` | Completed refined working atlas; not production textures |
| `bake_tread_tile.py` / `tread_tile_bake_report.json` | Separate periodic tread source and verified 512-pixel normal/AO bake; script leaves main-file saving to the caller |
| `bakes/Mammoth_Tread_{Normal,AO}_512.png` | Completed repeating working tile; source/coverage/seam checks passed, runtime integration pending |
| `verify_bake_source.py` / `finalize_bake_preparation.py` / `bake_source_verification.json` | Low geometry/UV-role checks, isolated normal QA, and final 94,292-high/1296-low verification |
| `refine_cylinder_bake_normals.py` | Earlier weighted-normal trial; superseded by the analytic cylinder fix for Texture Pass 01 |
| `fix_cylinder_source_normals.py` / `textures/pass01/cylinder_normals_report.json` | Analytic high-source wall normals, isolated four-part rebake and source/image preservation checks |
| `mammoth_bake_high_preview.png` / `mammoth_baked_normal_qa.png` | High-source and baked-shading inspection renders; check timestamp against current bake |
| `texture_mammoth_pass01.py` / `texture_pass_materials.py` / `texture_pass01_report.json` | Geometry-aware first paint, wear/mask composition, material setup and first-pass measurements |
| `textures/source/generation-notes.md` / `Mammoth_DesertPaint_Source_v01.png` | Exact image-generation prompt/provenance and generic material input, not a runtime UV atlas |
| `textures/pass01/Mammoth_{BaseColor,Roughness,Metallic,PlayerColorMask,Normal}_2048.png` | First-pass atlas images; player mask uses `BakeUV` like its paired albedo |
| `textures/pass01/Mammoth_Tread_{BaseColor,Roughness,Metallic,PlayerColorMask}_512.png` | Separate painted repeat tile and intentional all-black tread mask; existing tread normal/AO remain under `bakes/` |
| `verify_texture_pass01.py` / `textures/pass01/texture_manifest.json` | Fresh decoded PNG material verification, packed maps, texture contracts/file hashes and final pass preview generation |
| `audit_texture_uv.py` / `texture_uv_audit_report.json` / `texture_uv_layout_2048.png` | Non-mutating numeric UV/material-role audit and atlas inspection image |
| `mammoth_texture_pass01_{hero,player_red,player_blue,side}.png` | First-pass review renders using decoded texture PNGs; not runtime screenshots |
| `pbr_pass02_materials.py` / `texture_mammoth_pass02.py` / `finish_pbr_pass02.py` | PBR recipes, separate clean/AO colour baking, decoded-map/material verification and review lighting |
| `texture_pass02_report.json` / `textures/pass02/texture_manifest.json` | Unchanged geometry/UV/prior-pass checks, map measurements, explicit colour/AO/mask contracts and 14 file hashes |
| `textures/pass02/qa/texture_uv_audit_report.json` / `texture_uv_layout_2048.png` | Fresh Pass 02 sampled UV audit and layout; 44 meshes/1296 triangles, zero sampled overlap/errors/warnings |
| `textures/pass02/Mammoth_{BaseColor,BaseColorAO,Normal,Roughness,Metallic,AO,PlayerColorMask}_2048.png` | Current 2K PBR atlas; BaseColorAO is displayed, clean BaseColor/AO masters retained |
| `textures/pass02/Mammoth_Tread_{BaseColor,BaseColorAO,Normal,Roughness,Metallic,AO,PlayerColorMask}_512.png` | Current separate PBR tread tile and intentional all-black player mask |
| `mammoth_texture_pass02_{hero,player_red,player_blue,side,authoring}.png` | PBR review renders including the actual authoring scene in Eevee; not runtime screenshots |

### Terrain texture test: Temperate CLEAR1 grass v02

The first GPT Image grass attempt produced bright connected filaments and was rejected
as slime/noise rather than grass. The accepted second direction uses the user's supplied
grass reference more literally: separate short blades, dark depth between them, subdued
forest/moss greens and diffuse top-down lighting. The approved generated source is under
`art/terrain_textures/grass_test_v02/` with its exact prompt and hashes in
`texture_manifest.json`.

One 480x480 master was cropped deterministically into sixteen 120x120 CLEAR1 images in
the established `icon = (x & 3) | ((y & 3) << 2)` order. This makes every internal join
in the displayed 4x4 pattern an exact continuation of the same image. It does **not**
prove that the outer edge of the complete 4x4 macro-pattern repeats seamlessly. GPT
Image's request for seamless wrapping did not make the opposite source edges numerically
or visibly interchangeable, and a narrow deterministic edge-average trial introduced a
visible band. Preserve the approved raw appearance and treat outer-wrap repair and a
2x2 repeated-macro inspection as a separate pending gate rather than silently filtering
the borders.

Terrain is intentionally non-recolourable in this test. Every 120x120 albedo therefore
has an explicit all-black linear/Non-Color player-colour mask, and the 480x480 master has
a corresponding all-black mask. The manifest records those pairings. This is an
authoring/reference test only at the approval stage; no canonical terrain atlas or
installed runtime path was changed.

An isolated runtime comparison was subsequently captured from `GRASS01`, an all-clear
128x128 Temperate test map with six GDI structures, five vehicles and two infantry. The
same mission, object state, N64 camera (target 38,39; distance 2900), single simulation
tick and 1280x960 framebuffer were used for both shots. Both runs used `--enhanced` and
reported the enhanced shader/FBO path ready, bilinear filtering on and soft decal edges.
The installed runtime and canonical packs remained untouched: separate test packs replace
texture bank 0 and remap every cell to the appropriate CLEAR1 icon layout. Pack inspection
verified 339 textures, 264 meshes, 16,384 cells, a valid tail, and no mesh differences;
both capture scripts ended with zero failures. The result validates this isolated
comparison only, not production importer integration. It also makes the pending outer
4x4 macro-wrap defect visible as repeated horizontal boundaries in the new-grass shot,
so edge repair remains required before production use. Build/capture inputs, hashes and
logs are recorded under `art/terrain_textures/grass_test_v02/ingame/`.

The v03 seam repair keeps v02 intact as the failed-wrap baseline. It circularly offsets
the approved 480x480 master by half a macro tile, moving both bad outer joins to a central
cross while turning ordinary continuous interior pixels into the new outer boundary. GPT
Image repairs that central cross; a broad feather merges the repair while preserving the
continuous source border. The result is split back into sixteen 120x120 tiles with new
all-black masks and tested on the same `GRASS01` scene under the same enhanced settings.
Both v03 capture scripts ended with zero failures, the enhanced shader/FBO path reported
ready, and the hard horizontal four-cell boundaries visible in v02 are absent in the v03
runtime capture. This verifies the isolated grass wrap and presentation, not canonical
production-pack/importer integration. Sources, masks, manifests, repeated-macro review and
runtime evidence are under `art/terrain_textures/grass_test_v03/`.

The accepted grass set is v04 under `art/terrain_textures/grass_test_v04/`. The remaining
distance-visible bands were not black gutter leakage, a half-texel error or bad mipmap
generation. The v03 tile-average luminance ranged from 47.77 to 59.04 (11.27 levels), so
level-0 bilinear minification exposed its darker second row and brighter fourth row. A
periodic 32px-radius low-frequency exposure normalization reduced the sixteen-tile mean
range to 2.16 while retaining the approved high-frequency blade detail and global
exposure. The grass-only runtime atlas also uses four-pixel toroidal cross-neighbour
gutters: each CLEAR1 tile samples the actual adjacent tile rather than clamping its own
edge. This rule is valid for the connected 4x4 CLEAR1 macro and must not be applied to
unrelated atlas tiles.

The same `GRASS01` scene was recaptured at N64 camera distances 2900 and 3800 with
`--enhanced`; all DOS, new and far-distance scripts ended with zero failures and the
enhanced shader/FBO path reported ready. The user accepted the result with “Looks
perfect” and “We have a winner!” on 2026-09-22. That is artistic and isolated runtime-test
approval for v04, while canonical production-pack/importer integration remains a separate
pending step. Preserve v02 and v03 as the seam and tonal-banding diagnostic history.

The promoted Step 01 material now also carries a 16-bit linear height source, RGB8
tangent-space OpenGL (+Y) normals and explicit all-black player-colour masks for every
one of the sixteen tiles. Those maps are generated on the complete periodic 480x480
master before cropping. `tools/validate_enhanced_terrain_material.py` verifies all four
channel pairings, true height bit depth, exact master crops, hashes, mask range and the
normal reconstructed from the declared 0.0125-cell height amplitude. The accepted set
passes with mean height/normal cosine 0.999995 and no validation errors; its report is
`art/terrain_textures/enhanced_textures/temperate/01_grass/material_validation.json`.

This is authoring validation, not a claim about the existing screenshots. The current
terrain pack and renderer consume albedo only; their ground-normal target is derived from
the map corner heightfield. Keep macro cliff elevation, silhouettes, traversal and shadow
casters on that authoritative heightfield. Use the texture height source only for bounded
surface-scale relief, and integrate authored normals first through the Enhanced lighting
path. A cliff height map must be authored from rock strata, ledges, cracks and soil depth,
not inferred from painted albedo luminance, because doing so bakes the old light direction
into the new normal. Never describe a material as integrated until changing-light runtime
captures demonstrate its authored normal and height response.

### Temperate cliff material direction and displacement contract

The Step 02 topology audit resolves all `SLOPE1` through `SLOPE38` templates into 157
real cells and seven deliberately absent source positions. Its canonical sheets and
machine-readable mapping are under
`art/terrain_textures/enhanced_textures/temperate/02_cliffs/reference/`. Never invent the
seven missing cells, change row-major icon order, or treat a multi-cell template as
independent unconnected squares.

Early generated concepts were rejected for turning cliffs into freestanding boulder piles,
bright loop-like grass and small circular/nodular noise. A later layered pass was still too
busy, with many thin shelves stacked on top of one another. Two user-supplied real-cliff
photographs established the correction: a continuous grass-capped wall, very large jointed
bedrock masses, broad quiet planes and only a few deep vertical/diagonal fractures. The
selected reference is
`concepts/Cliff_S04_MaterialConcept_v04_MassiveJointedBedrock.png`; the user approved that
direction on 2026-09-22 with “Now THAT's good! Thats the direction.” The exact prompt,
hash and reference roles are recorded beside it.

That approval applies to macro geology only. Close inspection later showed that the
selected concept's albedo also contains dense loop-, cell- and pseudo-glyph markings.
The two photographic geology references carry visible stock-photo watermark swirls and
lettering across their rock faces; feeding them directly to an image model caused those
marks to be reinterpreted and propagated as stone detail. Never use a watermarked image,
or a generated albedo already containing a rejected motif, as material pixels in a later
edit. Build a sanitized material-neutral silhouette/form guide, exclude every contaminated
RGB source, and regenerate bare rock on a uniform chroma background. Require a close-zoom
zero-detail gate with calm continuous planes before adding any grain, lichen or contact
vegetation.

The zero-detail gate itself is not a finished-cliff review asset. Presenting a cropped
bare-rock diagnostic without the full grass context caused a separate rejection because
it was understandably read as the deliverable and looked far too simplistic. The
successful S04 v07 correction used a positive medium-detail budget instead of merely
adding more prohibitions: about six dominant masses, broad plane changes, four deep
clefts, a few structurally terminating secondary joints and large spall/ledge forms,
while leaving most face area calm. The user approved that generated rock direction with
“Perfect” on 2026-09-22. Do not sharpen it or add another noise layer; its current detail
is already near the upper useful limit at 240x240.

Always show this gate as the complete template with accepted grass on both high and low
ground. `Cliff_S04_ExactGrassProof_v07.png` composites the accepted periodic v04 grass
at 120 pixels per cell, preserves every fully exposed grass pixel byte-for-byte and has
zero chroma leakage inside solid rock. The remaining production contact task is limited
to restrained 1–2-pixel grounding occlusion and sparse 2–5-pixel overlaps sampled from
the accepted grass; never generate a new grass fringe.

The same approval explicitly requires normals and a real displacement map. For cliffs,
“real” means an independently authored 16-bit height source for coherent surface-scale
rock relief: small ledges, recessed joints and the soil interface. The scenario PK9
owns macro elevation and silhouette. Do not infer height from DOS darkness or albedo
luminance. Derive the OpenGL (+Y) tangent normal from the same height source, declare
its physical amplitude, and use the approved grass height/normal behavior wherever a
cliff cell exposes grass. Concept grass regenerated by GPT Image is presentation
context only and is never production art.

The `S03 | S04 | S05` cleanup established the corresponding contact rule. Discard
all grass produced by an image model and composite the accepted periodic grass master
at the exact runtime cell scale instead. Literal DOS pixels are evidence for the
composition, not an exact rock/grass or geometry mask: shadows can masquerade as rock.
Draw a natural contact against verified neighboring cells and confine any blend to a
small antialias band. Validate exposed grass against the approved master byte-for-byte.
This prevents invented loop-like vegetation and enlarged DOS stair steps without
promoting DOS shadows to geometry.

The historical v08/v09 `S03 | S04 | S05` proofs established the byte-exact grass and
dark, contrasty granite *material mood*. Their DOS-derived structure guides, semantic
masks and v02 staged height/normal maps were later revoked: baked source shadows and
texture-space arrows had been mistaken for physical cliff geometry. All 38 templates
must be rebuilt from literal DOS placement/color context plus independent scenario PK9
heights, without reusing those masks or height sources. The v09 RGB is a mood benchmark,
never a layout stencil or an automatic grade for every cliff.

The rejected bulk pass and matched SCG10EA v03 captures show that exact cropping,
seam continuity and a coherent micro-normal/height sidecar do not guarantee a cliff
read: a neutral stone surface still looked like a paved ramp. Review complete connected
families at 120 pixels per cell and in the actual Enhanced camera *before* cropping or
propagating a design. Each DOS template must retain its own recognizably placed large
faces, recesses, foot and joins as art composition; those painted cues remain separate
from physical PK9 shape and independently authored micro-relief. `production_v01/REJECTED.md`
and `02_cliffs/ingame_core_v03/REVIEW_FINDINGS.md` record the failed approaches.

The 2026-09-23 North `S03–S05` v04c review accepted its rock direction but requested
grass flowing subtly over the crest and grounding the foot. A GPT Image contact edit
changed distant rock pixels and regenerated grass at the wrong scale/phase, so it is
visual guidance only. The isolated v05c diagnostic instead samples the approved v04
grass in the exact world phase and changes just 1,221 pixels in narrow crest/foot
bands; all 70,755 originally exposed grass pixels remain byte-exact. Independent
pixel checks and matched Enhanced captures show no continuous dark contact line.
But the user rejected v05c: its contiguous, blended green edge read as jelly/jam,
not individual grass straws over rock. Thus exact phasing, narrow changed-pixel
counts and absence of a dark line are necessary but not an art pass. The revised
contact method must use separate rooted, tapered blades at crest and toe, with
irregular clusters and clear stone gaps; review both native and 4× detail plus
the actual Enhanced camera before rebasing height/normal maps. Keep contact
selection, neighboring joins, physical relief and runtime integration as
separate gates. The v05c albedo and its v01–v03 physical-map diagnostics are
quarantined from production until a corrected contact is selected and rebased.
On 2026-09-23 the user selected the v05e **TuftAccent** North contact for
continuation. That gate approves the discrete BaseColor blade placement only:
the v04c rock outside the strand masks and all 70,755 exposed v04 grass pixels
remain exact. Rebase physical maps from the independently authored angular
stone field, and derive blade micro-normal/height from the strand geometry—not
from green RGB. The isolated v01 physical rebase preserves 70,755 exposed-grass
pixels and 79,716 angular rock-interior height/normal pixels exactly; only the
2,313 selected rooted-strand pixels receive blade microheight. Sixteen matched
Enhanced captures cover no sidecar, normal-only, POM 0.5 and POM 1.0 under both
sun directions at near/far range. Internal QA found no visible tile seam, raised
contact strip, blade spike or UV tearing and recommends conservative POM 0.5,
but this is not user approval or production integration. Normal/POM review
remains a separate gate, and PK9 remains the true elevation/collision/shadow
authority while POM supplies bounded UV relief only.

The isolated East `S12–S11–S10` v02 test preserved the winding DOS footprint and
exact v04 grass phase, but matched Enhanced captures still read its uniform
diagonal stone grooves as a narrow rocky slope, not a cliff. The original
SCG10EA PK9 is low across the core x56–57 cells and rises sharply just east
at x58–59; do not infer a continuous wall volume across the bitmap or promote
dark DOS pixels into recess geometry. A later East pass needs a broad cap/face/
toe hierarchy specific to each template, while preserving the narrow S10 neck
and detached fragments. Its north/south ends join still-unapproved S13 and are
not proven seamless. Normal/POM maps cannot fix a silhouette or macro-height
misread; keep this East diagnostic out of production. Four later full-strip
ImageGen attempts retained the footprint but repeated the same category error as
towers, a deep central canyon/stair and a raised S10 bridge, while adding cellular
rock noise. They are rejected negative evidence. For this family, author the
per-template broad-face topology deterministically and use generated imagery only
as pigment/material source; never import its cracks, recesses or implied height.
The later bounded-template tests also isolated a provenance risk: including a
watermarked stock cliff photograph in an image edit strongly amplified tiny
loop/glyph motifs in the stone. Keep such a photograph as human geology guidance
only; exclude it from generator inputs and quarantine every contaminated output.
Clean-source iterative edits can still develop related swirly artifacts, so do
not assume that removing the watermark makes an edit chain safe. Prefer fresh
bounded generations, short prompts and native-scale review over repeated edits.
The resulting clean-source East v04g BaseColor candidate reconstructs the three
templates separately, then links them with bounded deterministic seam edits. It
reads S12 as a broad attached shoulder, S11 as a connected buttress, and S10 as
a narrow neck/gap with a terminal foot instead of a path, bridge, boulder pile
or shadow-canyon. Its rock-only value transfer matches the selected North mood
(East rock luma P5/P50/P95 26.93/82.89/144.82 versus North
24.0/82.64/141.91) while preserving 90,006 exposed v04 grass pixels and all 999
rooted-straw pixels byte-exact. Both internal joins pass native and 4x review;
the watermarked photograph and contaminated edit chains contribute no pixels.
This is an unapproved BaseColor-direction candidate only. S13 end joins,
16-bit height, +Y tangent normals, bounded POM and Enhanced runtime captures are
still separate work after explicit art approval; canonical and installed packs
remain unchanged.
The user accepted that East macro direction on 2026-09-23 but correctly flagged
excessive internal noise. Measurement found its micro-detail energy 31% above
the selected North material and short strong-edge fragments 45% higher. Do not
promote dense generated 1-4 px ridges, speckle or equal-weight short scratches
into texture identity or later physical relief. The v04i review candidate keeps
the exact DOS footprint, primary long/deep fractures, all 90,006 exposed grass
pixels, all 999 rooted-straw pixels, both connection bands and alpha, while
selectively calming repeated medium-scale ridge activity inside solid rock.
Independent review passes v04i for BaseColor-direction review only and warns
that upper S12 and lower S10 are at the maximum acceptable smoothness: do not
apply another global blur. If approved, derive height/normals from the retained
large planes and primary fractures plus restrained granular roughness, never
from the rejected albedo chatter.
The next user correction supersedes the v04i cleanup choice: the **v04g
Original** rock detail is preferred. The actual defect is the East strip's
high-side read. Source SCG10EA PK9 has low x56-57 ground near height byte 64;
the major rise is x57→58 to approximately 109-125, followed by higher x59
ground near 129-130. Thus image right/east is the top/crest and image left/west
the lower toe. The earlier phrase suggesting the principal rise begins at
x58-59 was imprecise. v04g paints the right contact as a long dark return and
therefore suggests a trench or isolated rock spine. Restore the v04g detail
language while correcting the connected top/face/toe hierarchy: grassy high
plateau and sparse separate blades at the east crest, west-facing broad rock
face descending to a grounded western toe. Do not derive physical height or
normals from v04i or from the misleading dark east strip. Show the correction
against literal DOS and PK9, then in the actual Enhanced camera, before the
physical maps resume.

The user accepted S10 v04r as the high-east/right crest direction ("This last
one gets the idea and is correct"), not as blanket approval of the East family.
S12 v04r demonstrated why matching mean rock value is insufficient: generated
pigment made a pale smooth ribbon and lost about 28% of the original local
granite grain. The S12 v04s correction reused S10's bounded, in-place v04g
grain treatment and exact approved grass. Assembled S12/S11/S10 then exposed
two artificial dark horizontal bands: freezing each tile's original two port
rows preserved the old false return even though individual pieces looked right.
The connected v04t study changed only 1,240 high-east cap pixels in the two
21-row join bands, retained the full 90,006 exposed grass pixels, 999 original
straw pixels and 160 new discrete blades byte-exact, and removed the visible
troughs at native size. Matched Enhanced DOS/new captures with unchanged SCG10EA
PK9, cell records, camera per pair, lighting and placed units technically
validated the isolated test; no material sidecar was used. **The user then
rejected its continuous gray rock "top" as an artificial shelf that does not
look like a cliff.** Native 1x and seam checks had missed how the 30–40 px cap
becomes a long roof in perspective. Thus v04t and its broad-cap source variants
are not an art direction to propagate. Return to the v04g original cliff face;
let the high grass plateau reach an irregular stone crest, with individual
overhanging blades and at most a narrow, nonuniform shadow correction rather
than a separate painted stone plane. S13 ends remain DOS. Height, +Y normal
and bounded displacement/POM remain held pending new BaseColor review.

The v04u no-shelf rollback proved the gray roof was the immediate regression,
but 186 new exact-grass blade pixels did not register at gameplay distance and
the original east contact still read as a dark outline. A modest S11 shadow lift
was likewise not a connected solution. Read-only DOS comparison found a second
cause for the narrow-ribbon read: a conservative bright/warm rock classifier
identified 5,258 S11 west-half pixels in the literal DOS tile outside v04g's
rock coverage. This is visual shape evidence, **not** permission to convert
every dark DOS pixel into rock; the palette/shadow audit still applies. Two
source-confirmed isolated-patch extensions looked like loose boulder nubs and
were rejected. A fuller source-traced S11 west contour guide is useful, with
its 259 ambiguous dark connector pixels kept inside the shape as recesses, but
the first deterministic granite fill looked horizontally smeared and was also
rejected. A fresh built-in GPT Image extension invented chunky rock and bright
grass and supplied no pixels. No corrected East BaseColor is approved. The next
art pass must make broad geology continuous across each DOS-connected template
and its neighbors, with the actual high grass meeting a broken rock crest;
verify that structure in Enhanced before authoring physical relief.

## 10. Next work and how to keep this document useful

Construction Yard `FACT` first-low v06 (4260 triangles / 110 meshes) and generated
exterior v02 + deployment interior v03 are approved. Its separate high-poly v01
is now at the **high-poly user approval gate**. Exact original XY/contact bounds
are retained, including the new interior. See
[`HIGH_POLY_REVIEW.md`](../art/construction_yard/HIGH_POLY_REVIEW.md).
Final optimized low, UV/bake/texture and runtime stages remain unstarted.

**Lookdev05 is artistically approved and the detailed recipe is finalized as
v1.1.** The representative-pod gate, whole-asset propagation, actual
normal/changing-light and player-colour views, packed-file save/reopen checks and
full-asset user approval are recorded separately. Preserve the approved bytes and
previous checkpoints. Any later requested revision gets a new version/review;
the remaining technical art-review notes are not a mandate to change approved art.

Still pending: runtime shading integration/conversion and
verification at the delivered texture resolution, including fine wheel-detail
sampling, tread colour repeat continuity, filtering and player-mask bleed. The
earlier PBR bakes, explicit AO policy, chamfer wear, masks, player previews and UV
audit remain useful baselines; neither those nor artistic approval establish
runtime approval. Also pending:
reusable/path-independent tooling, full turret-motion QA,
export-copy generation, two-part FBX report, separate pack import, and runtime
acceptance including factory exit. Resolve importer UV/colour behavior and signed
independent tread travel, and verify the player-colour adapter before relying on
those features.

For every future model, follow [the recipe](3d-model-recipe.md): **online research
→ OpenCNC3D original lookup/footprint measurement → initial low-poly within that footprint
→ first-low approval → actual mesh renders plus ArtStation-guided GPT Image views
→ generated-design approval → high-poly → high-poly approval → actual approved-high
views plus selected ArtStation finish reference → GPT Image finish direction
→ finish-direction review/approval → final optimized low-poly → final-mesh approval → UVs, bakes,
textures and validation**. Add a short case record: brief, source, initial mesh
and actual renders, exact generation prompts and selected
version, approval amendments, budget, dimensions, pivot/roles, detail allocation,
important artistic decisions, rejected approaches, verification results, file
locations and open issues. Promote repeated lessons into the general sections.
Keep measurements separate from assumptions and user approval separate from tests.
Never mark a pending stage complete just because the preceding preview looks good.

### Verified mechanical-clearance lesson — Construction Yard v06 (2026-09-22)

After a proportion pass, test internal mechanical pairs as well as the footprint.
Construction Yard v05 passed global bounds and per-object topology checks while
its lift pistons intersected the crane cheeks/cab and its forearm piston lay
inside the beam. v06 places actuator axes on exterior member-parallel planes,
moves the supported cab, and connects actuators through explicit pin mounts.
Inspect actual close-ups from both sides and above. Pairwise convex separating
axis checks on evaluated geometry detect overlap/containment that bounding-box
fit or surface-only intersection tests can miss. Whitelist only specific intended
pin/eye/mount contacts, never an entire crane assembly; include mounting pins in
cab checks. Repeat after reopening. Rest-pose clearance does not prove animated
clearance. v06 passed 271 checked pairs and received first-low approval with a
requirement to include interiors in the next generated-direction/modeling stages.

### Construction Yard generated-direction gate — 2026-09-22

First-low v06 received explicit user approval with an interior requirement. The
built-in GPT Image tool produced exterior v02 and proposed workshop/cab interior
v01 from actual v06 mesh views and the finish-only ArtStation reference. Exterior
v01 added incorrect cab side/rear windows; a targeted correction restored the
single approved front window. Interior cutaways are proposed design, not modeled
geometry or door-motion proof. The approved mesh hash stayed unchanged. See
[generated review](../art/construction_yard/generated/REVIEW.md) for exact prompts,
input/output hashes and selected views. The later sparse deployment interior v03
replaces the furnished interior proposal. **Exterior v02 + interior v03 received
explicit approval on 2026-09-22: “Perfect. Approved”.** High-poly is authorized;
its separate approval is still required before final low-poly.


### Briefly visible interiors — Construction Yard correction (2026-09-22)

Research the asset's actual function and exposure before dressing an interior.
The user rejected a furnished Construction Yard workshop: its interior is seen
briefly as the MCV unfolds. Direct inspection of local CONSYARD.VQA and the
OpenCNC3D source changed the direction to a shallow gridded deck, compact perimeter
mechanisms and panel-attached inner structure. Keep small surface detail in the
later bake/texture plan and avoid props that will never read at game distance.
FACT's deployment substitutes MCVANIM slot 18, so adding detail only to completed
FACT cannot establish a correct deployment reveal. Coordinate the eventual parts
with that rig and test packed-state and motion clearances separately. The user
approved generated exterior v02 + interior v03 on 2026-09-22 (“Perfect. Approved”).
The resulting high-poly is a separate approval gate, not deployment validation.
See [deployment research](../art/construction_yard/references/deployment/RESEARCH.md)
and [generated review](../art/construction_yard/generated/REVIEW.md).

### Construction Yard high-poly verification — 2026-09-22

High-poly v01 preserves the approved first-low and adds physical vault seams,
rounded crane/fan hardware, louvers, five perforated deck panels and low perimeter
machinery. Its 324 non-export sources evaluate to 234524 triangles; the later
runtime mesh was capped at 8000 including the interior, a historical asset budget
below the current 10,000-triangle structures ceiling. Fresh-reopen checks
pass exact source envelope/contact corners, closed coherent positive-volume
solids, no degenerate faces, and 641 static crane/cab/actuator clearance pairs.
The high-poly approval remains pending; final low, UVs and textures have not begun.

High-detail separation can reuse an earlier separating axis as a candidate, but
must project the new evaluated vertices onto it. A positive gap proves separation;
failure to find one after bounded work is unresolved, never a pass. Curved detail
can multiply edge-cross axes, so use cheap witnesses first. Inspect real renders
for attached supports and threshold continuity as well as individual manifoldness.
See [review and verification](../art/construction_yard/HIGH_POLY_REVIEW.md).

### Construction Yard high-poly detail revision — 2026-09-22

The user requested more detail than high-poly v01 and the generated concepts,
closer to the Remastered reference. v02 adds functional secondary forms across
the crane, rotary base, cooling unit, shutter, shell supports and apron. It keeps
the sparse deployment interior and broad roof. Its 1310 non-export sources
evaluate to 563264 triangles; the complete runtime building's ceiling was 8000,
a historical asset budget below the current 10,000-triangle structures ceiling.
This count describes the source and is not a detail target.

Fresh reopen verification passes exact source envelope and contact corners,
coherent positive-volume solids and 26449 static mechanical separation pairs,
including every inherited v01 pair. Added relief exposed three distinct checks:
a port can exceed the polygon without changing global bounds; a bearing carrier
can clip a previously clear pin; and coplanar surface layers can render poorly
despite passing topology. Fix geometry, preserve narrow explicit attachment
exceptions, and inspect close-ups after numeric QA. See the
[v02 review](../art/construction_yard/HIGH_POLY_V02_REVIEW.md). High-poly approval
is pending; final mesh, UVs, bakes and textures remain unstarted.

### Construction Yard final optimized mesh gate — 2026-09-22

High-poly v02 was approved with “Great. Approved”; its file is preserved unchanged.
The separate final-low v01 candidate has 177 editable meshes and 7544 evaluated
triangles, including all interior/runtime parts. Exact source footprint/contact
corners and 1595 static clearance pairs pass, with all 271 inherited first-low
pairs mapped and checked. All 1310 high source objects have explicit future bake
target ownership. Closed topology, positive volumes, corner normals and parent
transforms are verified after reopening. No final UVs, bakes or texture images.

Keep silhouette/negative spaces and broad cover layers in geometry; reserve small
surface relief for the approved high-to-low bake stage. Update new parent world
matrices before reparenting and repeat footprint checks afterward. A lathed low
wall may need explicit radial corner normals to avoid shoulder-induced bulging;
fan blade edges need separate normals from the pitched faces. Preserve per-face
material indices when applying temporary wire/QA material overrides. Final mesh
approval subsequently arrived as “Great. Continue”; see the
[approval record](../art/construction_yard/final_low_v01_approval_20260922.json).

### Construction Yard UV checkpoint — 2026-09-22

The new UV_v01 checkpoint uses two unique 4096 BakeUV atlases: Structure for the
broad shell/foundation/interior and Mechanisms for crane/plinth/fans. Fresh reopen
checks preserve all 177 meshes, 7544 triangles, geometry, corner normals and
hierarchy. Both atlases have zero sampled overlap, minimum boundary separation
over 21 pixels, and over 10 pixels to the atlas edge. These support the planned
8-pixel bake dilation; they are not proof against every mip/filter condition.
See [UV evidence](../art/construction_yard/uv_v01/README.md).

Measure actual packed gaps rather than trusting a requested Blender margin:
FRACTION packing initially produced roughly 45-pixel gaps and wasted density.
Local seams resolved a small sampled fold without changing approved geometry.
Before baking, verify high-source modifier evaluation in an enabled temporary
scene: the disabled original collection otherwise returned un-beveled base
geometry. Compare each evaluated count with the approved high report.

### Construction Yard representative material gate — 2026-09-22

MaterialTrial_v01 is saved and independently reopened with zero QA errors.
The trial has 18 matched physical normal/AO and PBR receivers; two protected
hoses have fixed rubber only, pending dedicated cages. The entire approved low
retains 177 meshes, 7544 triangles, source footprint, UVs, corner normals and
hierarchy. Packed images match their external PNGs, and the separate player
mask excludes fixed hardware/rubber and does not bleed onto unbaked charts.
Gold/red/blue, reversed lighting, normal-only, rear, context and small-size
actual renders were reviewed. Thin ring lips and small fastener bevels remain
sampling-limited at extreme magnification. See the
[material review](../art/construction_yard/MATERIAL_TRIAL_V01_REVIEW.md).

Ten shoulder detail sources were reassigned from the occluded cap to the
visible pin without changing low geometry; all 1310 high sources still have
exactly one owner. The 14 explicit cages and four implicit projections are
shared by physical and material passes. Material-only batching was checked
against fresh sequential output before use; shader position compensation and
source sharp/corner-normal preservation are essential. These lessons are
recorded in the recipe. Whole-building finish propagation awaits mapped-trial
approval; the sparse deployment interior and runtime/MCVANIM validation remain
later work. Approved Construction Yard checkpoints and Mammoth are unchanged.

### Mammoth material/high-source audit and revised finish target — 2026-09-23

The user selected [Gears 5 - UIR Tank](https://www.artstation.com/artwork/baZGeo)
as the current Mammoth revision's style/material/edge/metallic quality target.
Preserve Lookdev05's historical approval; it does not certify a match to this
new target. This is a direction amendment, not approval of a revised asset.
See the [audit](../art/mammoth_lowpoly/reviews/20260923_gpt_comparison/MATERIAL_HIGH_POLY_AUDIT.md)
and the expanded high-source/material criteria in the recipe.

Live inspection found the atlas and belt PBR maps connected, data maps Non-Color,
normal strength 1 and all 13 packed image payloads identical to their external
PNGs. Correct wiring and numeric validity do not establish artistic quality.
Lookdev05 armor starts at .50–.65 roughness, with primer .68 and exposed steel
.22–.35 before deposits. Barrel machining varies color/roughness while its
optional SurfaceHeight stays .5; that layer contributes no barrel micro-relief.
The physical normal remains connected and must be judged separately. Broad pale
edge wear, smooth guns and sparse secondary construction remain visible issues.

A high source exists, but much of its complexity is repeated running gear or
rounding. Require a per-assembly reference/detail plan and actual clay/grazing-light
approval views, not a source polygon/object count. After final-low approval,
compare high versus baked-low relief at the intended density and review decoded
roughness/metallic distributions by region. Distinguish model construction,
bake transfer, coating masks, micro-surface response and lighting; one cannot
be used to certify the others. Retain actual geometry for required silhouettes
and deep openings. No mesh, texture, rebake or new artistic approval resulted
from this audit.

The user explicitly required these lessons to apply to **every future asset**.
The recipe now makes the reference-feature inventory, per-assembly detail plan,
high-to-baked-low comparison and complete mapped-material diagnosis mandatory
evidence within the existing approval gates, including reused/default materials.
Adapt the evidence to the asset's actual assemblies and material classes; tank
equipment and Mammoth numeric ranges remain examples. Record missing original
features and deliberate omissions before high approval. Inspect full-bit-depth
decoded channels with declared class/component masks and sample scope, and judge
selective wear plus multiscale roughness separately from pigment and structural
relief. No additional gate type or universal numeric material preset is introduced.

The follow-up construction requirement applies to every asset too: fit exposed
attachments to their receiving surfaces and distinguish welded, bolted, sealed
and moving interfaces. Inspect underside and inward-facing construction wherever
the design leaves it visible. The recipe now requires actual joint/interior
close-ups at high-poly review; surface noise, buried intersecting slabs or dark
backing blocks cannot certify those connections. Mammoth's under-barrel supports
and visible running gear are specific applications of that general rule.

The user then corrected an overextended high-source pass: preserve the reference's
skirts covering roughly the upper half of the wheels, and omit concealed wheel
mechanisms that will not be seen or contribute to the bake. The recipe now
requires a visibility/bake-contribution filter before authoring. Restore coverage
and fit the caps behind the skirt instead of exposing more wheels to solve
clipping. Check tread/fender clearance in the assembled front, side and low-angle
views before presenting renders. Earlier raised-skirt/cutaway experiments are
superseded diagnostics, not accepted direction or evidence of a finished asset.

The corrected [Mammoth Highpoly06 review](../art/mammoth_lowpoly/highpoly_v06/HIGH_POLY_REVIEW.md)
is saved and open in Blender, with original skirt coverage, inset visible wheel
caps, fitted under-barrels/mounts and the restored classic equipment. Hidden
suspension and inward guide teeth are omitted. Evaluated visible wheel/tread
surfaces pass the pod-armor collision/containment audit; complete link corners,
grousers and hinge pins had to be checked at the sloped front and rear turns.
Original Lookdev05 geometry/images and the measured envelope are preserved.
This is a high-poly proposal awaiting user approval, with neutral review materials;
no final-low, UV/bake/texture or runtime-integration approval follows from it.

### Mammoth Highpoly06 rejection and revised construction brief — 2026-09-23

The user rejected Highpoly06: rear housings looked intersected into the turret,
launcher side panels did not follow the boxes' tilt, front treads still clipped,
lights/hatches were unreadable proxies, barrels looked retractable despite being
fixed, the machinegun used generic boxes, antennas were too short/thin, and wheel
armor leaned forward. This supersedes any description of Highpoly06 as corrected
or artistically accepted. The renewed high-poly gate remains pending.

Independent investigation found 22 front tread/Upper_Hull contact pairs in the
region excluded by the earlier pod-only clearance audit. Its scoped pass did not
prove assembled front clearance. Future checks must include every relevant
visible receiving surface and state exclusions precisely. The recipe now also
requires readable part function, attachment-local orientation, appropriate fixed
versus moving construction cues, and orthographic checks of upright/level forms.
[Highpoly07](../art/mammoth_lowpoly/highpoly_v07/HIGH_POLY_REVIEW.md) is the separate
revised proposal, saved and open in Blender. It rebuilds the listed attachments,
uses fixed continuous main guns and rigid under-barrels, and replaces the leaning
skirts. Fresh reopen verifies preserved original archives and no new geometry
faults. Expanded clearance checks all 922 track sources against pod armor and both
hulls: zero contacts or unresolved cases across 508 exact checks. Actual assembled
front/rear/side/top and close-up review remains separate from that numeric result.
Longer antennas are the explicitly requested vertical amendment; measured X/Y
bounds remain unchanged. Original Lookdev05 and rejected Highpoly06 remain
preserved. Highpoly07 still awaits user approval; no final-low or bake work begins.

### Mammoth launcher and barrel correction — 2026-09-23

The user requested three further Highpoly07 corrections: fitted launcher mounts,
side covers aligned to the boxes, and the original stepped barrel design shown
in a newly supplied classic render. The former launcher side is a sheared
parallelogram: long edges are at approximately -13.935 degrees in Y/Z, but the
orthogonal frame derived from its rear face placed the cover at -23.049 degrees.
The short edges matched, which concealed the 9.114-degree long-edge error in
oblique review. Direct side-perimeter measurements supersede that frame.

The barrel image shows a slender shaft, one enlarged forward muzzle section and
a squared support collar. A fixed-barrel requirement did not justify deleting
those design features into a uniform tube. Highpoly08 restores that silhouette
as fixed construction and fits visible launcher supports to both receiving
surfaces. These lessons are in the general recipe; Highpoly07's technical PASS
is preserved as historical evidence, not approval of these rejected details.

The user clarified that the twin cannon barrels still appeared too stubby and
thick. The revised Highpoly08 narrows the shafts by 27–29 percent and muzzle
sections by about 27 percent relative to its first test, with matching slimmer
collars and under-barrels. The muzzle endpoint and agreed horizontal envelope
are preserved; the exposed gun length to maximum diameter increases from 5.27
to 7.14. The recipe now explicitly separates part presence from proportion
review and requires connected forms to be adjusted together. The GunTest files
are intermediate diagnostics, not an approved high-poly version.

The combined [Highpoly08 proposal](../art/mammoth_lowpoly/highpoly_v08/HIGH_POLY_REVIEW.md)
is saved and open in Blender with actual four-angle and attachment-detail views.
Fresh reopen verifies 1453 high meshes / 559096 evaluated triangles, no new
geometry or envelope faults, and unchanged original archives/maps. Both covers'
evaluated edge errors are below .000005 degree. Exact preservation of all 922
track and 172 pod/hull checked sources carries forward Highpoly07's clearance
evidence; this was not a repeated collision run. Fitted launcher seats and
rail/cradle clearances have separate scoped evidence. These checks support the
proposal, not artistic acceptance: renewed high-poly approval remains pending.

### Mammoth Hobma design replacement — 2026-09-23

The user requested duplicating everything possible from
[Erik Hobma's GDI Mammoth Tank](https://erikh.artstation.com/projects/zdBeQ),
specifically including the side armor. This is an explicit geometry/construction
amendment, superseding incompatible HP08 layouts: deep segmented skirts replace
the former half-wheel coverage; twin roof hatches and a central optic replace
the single hatch/MG; the turret has integral rear shoulders/notch; the engine
deck has two circular fan grilles; cannons use two shared supports and capped
lower tubes. The [HP09 inventory](../art/mammoth_lowpoly/highpoly_v09/REFERENCE_FEATURE_INVENTORY.md)
records the full observed feature set from all seven artist images. Original
measured horizontal bounds remain authoritative, and reference imagery stays
outside runtime textures. HP08 remains a preserved, unapproved checkpoint.

HP09 is a high-poly revision and must return to that gate. New deep skirts and
sloped fenders require the complete tread path and projecting pins to be fitted
again; the prior unchanged-geometry clearance proof cannot apply after those
changes. Updated modules declare all replaced/modified source objects and fresh
clearance must include the hull as well as pod armor. No final-low or bake stage
is authorized by the change request itself.

The integrated [Highpoly09 proposal](../art/mammoth_lowpoly/highpoly_v09/HIGH_POLY_REVIEW.md)
contains 2,204 high-source meshes / 790,542 evaluated triangles, with twelve actual
mesh views and an attributed front/rear reference comparison. Independent fresh
reopen checks pass declared edits, new geometry, launcher cover alignment,
original archive/map preservation and exact original X/Y bounds. A fresh complete
static running-gear check covers 1,052 gear meshes against 795 pod/hull meshes:
354,972 eligible pairs, 172 exact candidate checks, no intersections or containment
findings and no contact waivers. Both reports bind to candidate SHA-256
`369f620a52b482fe081e45f847079275b9eee9725b3a448916df7afddcdfe29d`.
The high-source tread bevel leaves a 0.000519-cell ground gap; final-low work must
explicitly verify runtime ground contact. The recessed rear ladder treatment
preserves the original width. Materials remain neutral review materials.
At this technical delivery HP09 awaited renewed high-poly approval; the checks
alone did not authorize final-low, bakes, textures or runtime integration. The
subsequent explicit user decision is recorded below, without rewriting either
QA report or the saved .blend file.

### Mammoth HP09 approved; post-high generated finish stage added — 2026-09-23

The user explicitly approved the presented HP09 revision:

> Approved - Now add another step after the highpoly - Run it through GPT image again, high quality render, using the Artstation references (bazGeo), for the style of texturing, materials, metals, grittyness etc.

Approval binds to `Mammoth_TD_OpenCNC3D_Highpoly09.blend`, SHA-256
`369f620a52b482fe081e45f847079275b9eee9725b3a448916df7afddcdfe29d`, and the
actual mesh views in its [review record](../art/mammoth_lowpoly/highpoly_v09/HIGH_POLY_REVIEW.md).
The saved source and earlier QA remain immutable historical evidence. This is
high-poly artistic approval, not a mapped-material, final-low or runtime approval.

The newly requested pass uses those approved high views as geometry/camera
authority and [Gears 5 - UIR Tank, Andres Munar — baZGeo](https://www.artstation.com/artwork/baZGeo)
as the finish reference. Hobma remains the design/construction authority.
The baZGeo choice overrides the general J9Z64d default for this asset's pass.
Its prompt, generated outputs, provenance and drift assessment are recorded
separately by the generation step; no output or finish approval is asserted by
this workflow update. Return the generated finish direction for review/selection
before final-low work, then continue through the unchanged final-mesh, mapped
trial and full-asset approval gates.

The user then added an explicit combined revision instruction:

> Great - If you look at the Artstation reference, you can see that almost every edge is heavily chamfered, to make it more readable from a distance. Do a pass on this, on the Mammoth high, and then redo the image generation afterwards.

This authorizes the chamfer/readability pass on a separate HP10 source and image
regeneration afterwards, preserving approved HP09 unchanged. Complete that
requested preview sequence, then present the revised actual high and generated
finish together for renewed acceptance before final-low. The prior HP09 approval
does not automatically approve the edge revision. The subsequently verified
modeling/QA result is recorded below; renewed artistic acceptance remains pending.

### Mammoth HP10 chamfer/readability revision — 2026-09-23

The separate [HP10 source](../art/mammoth_lowpoly/highpoly_v10/Mammoth_TD_OpenCNC3D_Highpoly10.blend),
SHA-256 `69ccef556e5460d654aeb7557af68a088e7d83d70e9f42359098671006b09615`,
contains 2148 high-source meshes and 754950 evaluated triangles. It broadens
physical armor, turret, launcher and support edges and incorporates the requested
10% main-gun diameter increase with matching bores and supports. It removes 56
thin decorative skirt-edge strips in favor of coherent chamfered plate edges.
High-source count is not a runtime triangle budget.

The independent [geometry/preservation audit](../art/mammoth_lowpoly/highpoly_v10/verify_report.json)
verifies 92 removed, 36 created and 186 modified sources against the exact
declarations; all 1926 remaining sources, including all 1052 Track_* meshes,
retain their geometry and poses. The original XY envelope, approved HP09 file,
original Lookdev05 archives, images and 13 external maps are preserved. Launcher
cover/gasket edge alignment also passes against the evaluated housing perimeter.
Eight inherited wheel-topology warnings remain identified historical defects;
two gun attachment AABB warnings were reviewed in actual assembled views and do
not represent detached guns.

The fresh [running-gear audit](../art/mammoth_lowpoly/highpoly_v10/runninggear10_clearance.json)
checks every Track_* source against its pod and all hull sources: 1052 gear meshes,
739 armor/hull meshes, 340220 eligible pairs and 172 exact pairs, with no surface
intersections, containment findings, unresolved cases or contact waivers. This
is the reported running-gear scope, not blanket collision certification.

All twelve actual integrated views and the requested matched
[before/after sheet](../art/mammoth_lowpoly/highpoly_v10/Highpoly10_Before_After.png),
[edge close-ups](../art/mammoth_lowpoly/highpoly_v10/Highpoly10_Edge_Details_Before_After.png)
and [smaller on-screen comparison](../art/mammoth_lowpoly/highpoly_v10/Highpoly10_Distance_Readability.png)
were independently inspected. Broader edge bands read more clearly without new
visible fit defects; the smaller-view improvement is visible but modest. The
amount of stylization still requires user acceptance. The measured roof setbacks
and corrected non-planar mantlet miters support the reusable evaluated-width,
local-outline and matched-view checks added to section 4 and the recipe.

The [independent review](../art/mammoth_lowpoly/highpoly_v10/INDEPENDENT_REVIEW.md)
records scope and limits. HP10 and its regenerated GPT Image finish return together
for renewed review/selection. No final optimized low, UVs, bakes, shipped textures
or runtime integration are authorized by these technical checks.

### Mammoth HP10/GPT selection and global 10k ceilings — 2026-09-23

After the additional GPT Image/Ideogram comparison, the user agreed to keep GPT
and instructed: “let's move on to the Bakes and Lowpoly,” with **10k maximum
triangles for vehicles and 10k for structures**. The full instruction and exact
accepted artifacts are in the [new approval record](../art/mammoth_lowpoly/highpoly_v10/HP10_FINISH_APPROVAL.md).
Actual HP10 SHA-256 is
`69ccef556e5460d654aeb7557af68a088e7d83d70e9f42359098671006b09615`; selected
corrected GPT FinishDirection02 SHA-256 is
`d486312784445eba4bb651436590a0f6d17c5b7c40752c71711d452dae0ee9b0`.
These decisions supersede their earlier pending status; historical QA and
generation records are not rewritten. The new ceiling applies to the complete
evaluated runtime asset, including all parts and modifiers, and is not a target.

The [comparison case](../art/mammoth_lowpoly/references/ideogram4_highpoly10/README.md)
records three completed Ideogram experiments. The first combined-reference input
lost major geometry; geometry-only weight 90 mostly preserved the clay look;
weight 70 allowed more sand paint but still altered detail and separated paint
from metal less clearly than the corrected GPT result. These observed requests
demonstrate a source-preservation/material-transformation tradeoff in this case.
Different input layouts, visual/text references, prompts, resolutions and
iterations prevent a universal model ranking or universal weight recommendation.
Review geometry fidelity separately from finish quality. The user retained GPT
Image for the recipe; Ideogram is an unselected experiment.

The current authorized stage is a separate optimized low in
`art/mammoth_lowpoly/final_low_v01`. Carry broad silhouette/form-defining chamfers
into actual low geometry, keep the original footprint and fitted seats, and assign
fine edge relief to named bake sources/receivers. Compare projected readability
before low approval and real transfer at delivery resolution in the later mapped
trial. **Final-low approval before final UVs, physical bakes and textures remains
mandatory**; “GO” authorizes progress through the workflow, not an unseen mesh or
a waiver of the remaining gates.

### Mammoth final-low approval and Texture01 basis — 2026-09-23

The user approved the presented final low and requested texture work using the
latest selected GPT render. The [approval record](../art/mammoth_lowpoly/final_low_v01/FINAL_LOW_APPROVAL.md)
binds FinalLow01 SHA-256
`be5f5e63cc06948c013e7cb73e663988dd6e2c7dc9c3f8cd34abfba9b8625a8f`.
This authorizes final UVs, physical bakes and a representative mapped-material
trial; whole-asset material propagation still follows that trial's approval.

The separate [Texture01 UV checkpoint](../art/mammoth_lowpoly/texture_v01/UV_HANDOFF.md)
preserves all 206 meshes / 9,870 triangles, frozen corner normals, rest pivots,
footprint and track face masks. Fresh reopen verifies the 4096 planning atlas:
no positive-area overlap or missing hard-normal UV split; minimum island gap
20.014 pixels and border 10.240 pixels for 8-pixel bake dilation. Its source hash
is `1566f47a07f6bb7a9f6bad018cadce1df1845e2db113d55fa5c99c8d9c8bf997`.
The recorded 501 very small/slender triangles without a 4K texel-center sample
cover about 0.071% of occupied UV area; passing atlas layout does not prove their
detail survives transfer. The continuous tread UV retains 43 front / 42 rear
shoe periods and a separate static sidewall mapping.

The [trial plan](../art/mammoth_lowpoly/texture_v01/TEXTURE_TRIAL_PLAN.md) selects
47 receivers spanning armor, guns, launcher, optics, headlamp and rear cooling
details. Generated tan pigment supplies unlit color only. Explicit source roles
and shared wear/deposit selections determine physical material channels and the
separate player mask. Saved actual mapped views and subsequent approval, not
the generated pigment or this basis record, determine material acceptance.

### Mammoth Texture01 representative material review — 2026-09-23

[MaterialTrial06](../art/mammoth_lowpoly/texture_v01/MATERIAL_REVIEW.md) is the
current mapped review checkpoint, SHA-256
`c9c874382e7ac36bf82821f334943be8579956fb340e7c35e37e15b3e0d8d367`.
It combines `paint_final03`, `physical_v05` and the periodic `tread` maps; older
trial versions remain diagnostic history. The selected GPT FinishDirection02
guides the tan pigment, restrained broad paint, broken chamfer wear, lower grime
and weathered gun steel. Explicit source roles and shared coating/deposit masks
control roughness, metalness and player colour independently from pigment RGB.

Fresh read-only reopen verifies all 206 meshes / 9,870 triangles, exact approved
geometry/shading/UV/pivot basis, all 13 packed maps and actual shader connections.
All 29 actual mapped views were inspected, including changed light, isolated
channels, red/blue player colour and reduced texture density. Projection fixes
removed fan-deck underside capture, hidden mantlet-wall transfer and an adjacent
barrel-root end-cap normal artifact. These lessons are recorded in recipe section
6; every coupled physical/material channel uses the same per-face ray policy.

The material audit retains **62/63 strict checks passing**, with the remaining
single-texel filtered-product discrepancy and its measured explanation preserved.
Fine fan wires, the approved polygonal muzzle/aperture and five inherited tiny
muzzle-ray backface samples remain explicit limits. The 4K trial atlas does not
certify a runtime texture budget. The [actual review sheet](../art/mammoth_lowpoly/texture_v01/MaterialTrial06_Review.png)
and [player previews](../art/mammoth_lowpoly/texture_v01/MaterialTrial06_PlayerColours.png)
are Blender evidence, not generated views. The user approved this representative
result on **2026-09-23** with “I can approve this so far.” Whole-asset propagation
is now authorized; final actual-model approval and runtime integration remain
separate and pending.

### Mammoth Texture01 full 2K candidate — 2026-09-24

The approved representative material has been propagated to all 206 runtime
meshes. [FullMaterial01](../art/mammoth_lowpoly/texture_v01/FULL_MATERIAL_REVIEW.md)
retains 9,870 evaluated triangles and uses a 2048 static atlas with a regenerated
four-pixel whole-atlas gutter plus the existing 512 repeating tread set. Paint
was baked natively at the delivery resolution in two disjoint 103-receiver
batches. Batch padding was discarded before composition and regenerated only
after the complete coverage union, avoiding cross-batch margin contamination.

The complete 4K physical transfer exposed one source-absent shell: the fan-deck
high source described only detail over part of its larger low receiver. An
explicit paint-class shell backing now fills those faces while leaving the high
fan detail as the first ray hit. The corrected receiver reports zero covered
SourceID holes and zero black covered normal pixels. This establishes a general
quality gate: coverage plus a plausible flat normal is insufficient without an
independent receiver/source identity raster.

Fresh read-only reopen of the saved 2K candidate passes with 206 meshes, 9,870
triangles, exact frozen geometry/UV/corner-normal/pivot state and byte-identical
packed/standalone maps. The saved map audit passes 63 of 67 strict checks. Its
retained exceptions are 10 coverage samples beyond the one-pixel boundary rule
and 201 semantic/source-class comparison hits on overlapping tiny details; all
are recorded and actual PBR, isolated-channel and gameplay-scale renders show no
visible hole. Full mapped artistic approval remains pending. Runtime return and
in-game validation have not begun.

### Mammoth tuned material and RuntimeRig01 export — 2026-09-24

The user approved the adjustable Gears-finish result, tuned the live controls and
requested that they become the future gritty-vehicle starting preset. Exact values
and the reusable control/coupling rules are recorded in recipe section 13 and in
`art/mammoth_lowpoly/texture_v01/Mammoth_GearsLookdev02_UserValues.json`.
The tuned checkpoint preserves the 206-mesh / 9,870-triangle approved source.

Delivery textures bake the approved deposit/color response into a 2048 atlas and
two separate 512 left/right tread sets while retaining a separate player-colour
mask and physical normal/AO. Default player tint is a preview operation, not baked
albedo. Twelve complete hidden rocket bodies add 120 triangles; the exported
historical Mammoth total is **9,990**, within its recorded 10,000 per-asset override.

`Mammoth_TD_RuntimeRig01.fbx` contains 218 mesh nodes and a 30-bone rigid skeleton.
Fresh FBX import verifies exact triangles, four material identities, both `BakeUV`
and `TreadUV`, required bones and nonzero motion for turret yaw, left barrel recoil,
left launcher pitch and one individual rocket. The hierarchy also groups hull,
four pods, rear engine deck, four tread units and turret body as detachable chunks.
`Mammoth_TD_OpenCNC3D_Compat01.fbx` parses through `tools/art/fbxin.py` as exactly two
nodes: 4,890 static triangles and 5,100 turret triangles.

At this checkpoint this was **export validation, not runtime integration**. The
later C3M integration below supersedes the assumed mission-pack target and records
which authored controls the current desktop format and game state can actually use.

### Mammoth C3M integration feedback — 2026-09-24

The game integration agent reports the Mammoth integrated and drawing through the
desktop C3M path. `tools/art/blender_c3m.py` exports from the Blender scene and
`game/model3d_mod.h` renders the resulting `.c3m` with Cook-Torrance GGX, a
directional sun/shadow map and sky-cubemap ambient. The modern desktop target uses
BaseColor, Metallic, Roughness, OpenGL tangent Normal, AO and PlayerColorMask;
mission-pack replacement remains the Classic Visuals/legacy path. The C3M exporter
and resulting artifact live in that integration task and are not present in this
art worktree, so this entry records the integrating agent's measured results rather
than claiming a second local game run.

The exporter packs exactly three RGBA8 slots: BASE contains sRGB BaseColor RGB and
linear PlayerColorMask A; NORM contains tangent OpenGL +Y normal RGB; ORM contains
AO R, Roughness G and Metallic B. `SurfaceHeight` and emissive have no runtime slot.
The six canonical suffixes were discovered correctly on first export. Six duplicate
left-tread image datablocks ending in `.001` made lookup order-dependent; the clean
delivery now contains one canonical datablock for each of 24 consumed images.

C3M stores the three maps as raw RGBA8. The 2K atlas build occupied 58.1 MB. A
maximum-zoom comparison with four Mammoths established the shipped 1K atlas plus
256 tread/rocket maps: 15.6 MB, with 4,037 of 1,600,000 pixels (0.25%) differing
from 2K. The 512 atlas trial occupied 5.0 MB but differed at 7,099 pixels (0.44%)
with a larger maximum error. The 2048 atlas is therefore a working master, not the
runtime budget. Final runtime inputs need only eight bits because C3M discards the
delivered 16-bit depth.

Player colour is technically correct but visually weak. The integrated atlas mask
covers 0.744% and both tread/rocket masks are black, so GDI and Nod are difficult to
distinguish at gameplay distance. A later art revision should assign larger coherent
armor panels while continuing to exclude deposits, exposed metal, rubber and tread.

The current C3M part record stores role, pivot and name, with no parent, local rest
transform or visibility. Bone ancestry therefore flattens the rig into two drawn
parts. Geometry, full PBR and turret yaw work; the explicit pivot landed exactly.
Barrel recoil, destruction chunks and signed left/right tread motion are reachable
with renderer/C3M work. Launcher pitch is not authoritative because unchanged
Tiberian Dawn exposes no elevation state. Individual rocket visibility is likewise
not authoritative because the Mammoth has unlimited ammunition and exposes no tube
or magazine state. The logic does distinguish cannon fire (`WEAPON_120MM`) from
rocket-weapon fire (`WEAPON_MAMMOTH_TUSK`). Keep the future-facing rig but do not
represent absent simulation state as pending routine integration.

Axes, 1024-unit scale, explicit turret pivot and the x -354..359 / z -533..619.7
footprint matched exactly. The two independent split descriptions also agreed:
bone ancestry and the compatibility FBX both produced 4,890 hull and 5,100 turret
triangles. This remains a required cross-check for later vehicles.

The original runtime `.blend` retained four scenes, 2,657 meshes and 974,062 stored
triangles of authoring history although its active scene correctly referenced only
218 meshes / 9,990 triangles. It has been replaced in the delivery folder with a
clean 541 KB source containing one scene, 218 meshes, one armature, four materials,
24 canonical images and 9,990 triangles. The 197.9 MB history file is retained as
`Mammoth_TD_RuntimeRig01_AuthoringHistory.blend` under `texture_v01` and must not be
used as exporter input. Fresh reopen reports all external texture paths present.

### Construction Yard high v03 — current revision (2026-09-24)

The user reopened high-poly authoring and requested substantially more detail on
the existing model, especially from Remastered references. The versioned v03 high
retains all 1,310 prior mesh vertex/face arrays and local poses, adds 388 detail
sources, and changes six declared bevel widths. It contains 1,698 non-export high
meshes / 693,080 evaluated triangles. The final runtime ceiling is currently
10,000; the Yard's historical 8,000 statements are not the current category rule.
Exact footprint/contact checks, evaluated topology, preservation comparisons and
31,825 scoped static crane/cab/actuator checks pass, including all 26,449 prior
pairs. Measured planar setbacks confirm the chosen chamfers evaluate as intended;
this is not blanket collision, motion, bake-transfer or game validation.
Current review: art/construction_yard/HIGH_POLY_V03_REVIEW.md. Actual high approval
is pending before the post-high generated finish direction and its approval gate.
The earlier final low, UVs and partial material trial remain preserved history.

### Construction Yard foundation correction — 2026-09-24

The user identified triangular/pinched slab corners inherited by High v03 and its
generated finish. High v04 changes only the foundation: vertical upper fascia,
horizontal underside step, preserved original ground-contact and envelope polygons.
Two old nonplanar side quads (.058261685-cell maximum projection spread) are removed;
corrected face spread is under 7.08e-8 cell. All 1,697 other high meshes are unchanged.
Footprint, topology and prior scoped mechanical checks pass. This exposes a missing
quality check: manifold surfaces can still contain twisted quads whose triangulation
creates visible corner wedges. Review side-face planarity and low-angle perimeter
views. See art/construction_yard/HIGH_POLY_V04_REVIEW.md. Renewed high approval is
pending; no regenerated finish or updated final low is claimed.

### Yard foundation v05 supersedes the v04 proposal (2026-09-24)

The user identified v04's surviving underside notch. V05 extends the slab's full
outer outline to ground, removing the recess while retaining its overall envelope.
Ground contact intentionally expands to that outline; the old contact guide remains
historical evidence. The revised contact-area treatment awaits mesh approval.
All other high geometry is unchanged. See art/construction_yard/HIGH_POLY_V05_REVIEW.md.


### Yard FinalLow v02: preserve approved high chamfers in the frozen low (2026-09-24)

After High v05 and FinishDirection02 approval, the revised low contains 9,820
triangles across 211 runtime candidates. It preserves 170 prior low meshes and
updates the grounded slab plus six broad chamfers, adding 34 contour/opening
receivers. Broad planar setback measurement on triangulated lows must use the
union boundary of coplanar triangles; measuring one triangle includes internal
diagonals and gives misleading distances. The resulting .005-.007-cell median
setbacks match the approved high hierarchy within 1e-5 cell. Their estimated
width at 300-pixel whole-asset height remains subpixel; a correct geometric
measurement does not certify delivery-resolution normal/material readability.
See art/construction_yard/final_low_v02_preservation.json and FINAL_LOW_V02_REVIEW.md.
Final-low approval and later physical transfer/material review are still required.

### Verified Medium Tank final-low check — 2026-09-24
Reopen the saved, triangulated low before recording counts/topology; proxy closure repairs can change evaluated totals and bounds. Medium FinalLow01: 9,948 triangles, zero checked topology findings, original XY envelope and ground retained. See art/medium_tank/final_low_v01/VERIFY.json and REVIEW.md for exact scope. This geometry result does not certify bake transfer, animation or runtime integration.


### Yard High05 / UV02 physical transfer — 2026-09-24

After FinalLow v02 approval, two unique 4K-planned BakeUV atlases retain all 211
meshes / 9,820 triangles, corner normals and transforms. One initial Mechanisms
unwrap had 78 sampled overlap pixels; splitting those boundaries and re-unwrapping
removed them. Final measured gaps are 21.7075 pixels and borders exceed 10.8539
pixels at 4K; a 2K trial uses the same UVs with four-pixel dilation and half those
measured gaps. Geometry and UV audits pass after fresh reopen.

The 26-receiver High05 physical trial at 2K has zero covered normal misses, zero
source-identity holes, valid normal vectors and no cross-pair padding/interior
collisions. A separate chosen-cage audit measures 8,176 near-boundary/interior
rays with zero misses and backfaces. Preserve the explicit distinction between
receiver coverage and projected SourceID; neither sampling nor a plausible flat
normal alone certifies complete detail transfer. In this run, joining evaluated
source copies with retained identity material slots removed a severe slowdown
in the many-object SourceID pass. The original high and approved low are unchanged.
See art/construction_yard/textures/material_trial_v02/physical2k/physical_bake.json.
Actual mapped material review and full-asset propagation remain separate gates.


### Yard semantic player exclusion and albedo pairing — 2026-09-24

Verified on the High05/UV02 material trial: tightening player eligibility from authored primer/steel/deposit/hardware/rubber channels can reveal a neutral recolour base as grey patches. Do not solve that by allowing dirt or bare metal to recolour. With the existing `mix(base, base*tint, mask)` shader and strictly positive default tint, rebake the natural intended colour and derive the paired albedo as `natural / (1 + mask*(defaultTint-1))`. Check finite in-range values and decoded PNG reconstruction; a zero denominator or clipping requires a different representation. This Yard trial uses default linear tint (1,.68,.17), with measured maximum decoded reconstruction error .00226104. Preserve raw masks/diagnostics, explicitly version the eligible mask and paired albedo, hash/pack/reopen both, and inspect at least two alternate colours. The current Yard trial has zero strong-mask conflicts with exposed steel, hardware and rubber. This is representative Blender evidence; it does not establish runtime shader equivalence or whole-asset approval.

### Runtime moving-highlight gate — 2026-09-24

For a light-dependent bright patch on a surface intended to be planar, run a
channel-isolation matrix before editing maps: normal off, metallic zero,
roughness one and unlit BaseColor. Audit evaluated vertex planarity and compare
geometric face normals with exported corner normals. If the artifact survives
normal-off and metallic-zero tests, do not hide it by weakening the normal map or
painting the albedo darker. Correct unintended micro-slopes, preserve deliberate
bevels and fitted seats, then rebuild the runtime package and repeat the same
camera/light capture with normal mapping on and off.

Mammoth RuntimeRig01 is the verified case: 18 rear-receiver vertices were
flattened to one 0.209645-cell plane, upper faces were hardened, triangle count
remained 9,990, and the two gameplay-scale white wedges disappeared in both
normal states. The proof is the C3M engine A/B, not the Blender viewport alone.


### Whole-yard projection diagnostics — 2026-09-24

A sampled cage audit is necessary but does not replace covered-pixel validation.
Yard Full01's chosen temporary cages passed 68,740 sampled rays with zero misses
or backfaces. Four explicitly identified plain curves use the approved retained
low surface as their shading source; do not report their differing high curve
tessellation as transferred. Preserve source/receiver geometry and record these
exceptions in the physical and material manifests.

A short baked normal vector is not automatically a missed source ray. Retain raw
float maps, independent coverage and source identity, then investigate its local
directions. Yard's 274 grate/hose boundary vectors were hash-bound to positive
convex mixtures of nearby valid unit directions (maximum residual .00019).
This scoped classification does not waive source holes, black normals, or the
actual rendered transfer review. Normalization must preserve nonzero direction;
never invent a direction for missing data. The original numerical diagnostics
and per-pixel mixture evidence remain in the asset's Full01 physical records.


### Original-position exceptions for exploded material baking — 2026-09-24

Validate evaluated corner normals after assembling any exploded source batch.
Yard Full01 exposed 37 changed corner normals when large float coordinates
collapsed tiny bevel vertices on rear service panels and two shutter tiles.
Projecting the three affected receivers separately at their original coordinates
preserved the existing .001 normal-delta bound without editing approved geometry.
Also compare independent SourceID against the physical bake: one coincident
bezel/gasket boundary changed 71 pixels after translation, and original-position
projection restored exact agreement. Apply a chosen projection correction to every
material channel, not only BaseColor or the diagnostic ID image. Keep residual
source-boundary differences explicit; the verified Yard preflights retained
32 Mechanisms and 15 Structure samples, not universal pixel-perfect identity.
Evidence: art/construction_yard/textures/full_v01/DIAGNOSTICS.md.


### Yard Full01 completed review evidence — 2026-09-24

The finished 211-part / 9,820-triangle Yard passes fresh-reopen geometry/UV/corner-normal
fingerprints and packed-image checks. Two 4K sets provide 14 packed display maps;
explicit paired player masks have zero strong-mask/fixed-material conflicts and
maximum decoded default-colour reconstruction error .002265. Actual red/blue views
confirm that conservatively excluded weathered patches retain their original ochre,
most visibly on the plinth base; this is visible artistic direction, not hidden
under a numeric mask pass. Matched High05/normal-only comparisons retain broad
chamfer highlights but expose softer/stair-stepped microscopic bearing/bolt edges.
The actual-camera chamfer report supersedes simple height/ortho pixel estimates.
See art/construction_yard/TEXTURED_V01_REVIEW.md and its hashed manifest.
Full-asset artistic approval and runtime validation remain separate.


### Yard Full01 finish-match correction - 2026-09-24

The user rejected the visual match of Yard Full01 despite accepting its technical quality. Direct comparison with the approved High05 FinishDirection02 shows insufficient broad staining/runoff, pale hardware and weak coating/metal separation. The generated finish remains the asset-specific artistic target; Mammoth supplies material construction methodology, not a numeric preset that supersedes that image. Representative approval does not excuse a visibly divergent whole-asset result. Compare actual mapped material studies against the selected generated image explicitly, including palette/value relationships, weathering scale/placement and material separation. Keep an unchanged-light A/B alongside any second presentation studio; do not conceal material drift with exposure or stronger AO. Bare-metal identity must not be globally reduced to an intermediate value as a shininess control. Full01 remains preserved and unapproved while a versioned representative correction is developed.


### Yard player-deposit coverage lesson - 2026-09-24

Trial03b passed numeric player-mask exclusion checks but actual red/blue previews exposed broad fixed ochre streaks: weak translucent deposits excluded mostly intact player paint. Numeric non-overlap is not sufficient visual validation. Trial03c confines deposits to localized substantially covered patches and recomposes BaseColor, Roughness, Metallic and player coverage with the same authored selections. For this specific known linear stack, lossless predeposit colour/roughness can be recovered with bounded denominators; use the separately authored predeposit paint coverage rather than dividing filtered products (37 samples exceeded one slightly in the diagnostic inversion). Actual alternate-colour renders verify the broad streak correction; fine warm wear transitions remain fixed. This is revised representative material evidence, still pending artistic approval and full propagation.


### Yard response-compositor ownership check — 2026-09-24

The Trial04b source-ID-selected response refinement exposed overlapping receiver crop rectangles: assigning zeros to an entire crop background erased earlier parts' valid selections. Write only the receiver's verified Padding/coverage pixels, even when the high-source projection already passed. The corrected compositor asserts greater than 98% selection of the turntable's owned pixels and records source-ID uncertainty separately; ambiguous IDs are left unchanged. Preserve raw projection/class/deposit masks and hash-check the resulting PNGs. A stale pre-correction PNG was rejected by the existing float-to-decoded comparison; superseded unfinished outputs were archived before fresh encoding. This is a post-composition ownership/cache lesson, not a new geometry or ray-projection defect. See `art/construction_yard/refine_material_trial_v04b.py` and its hash-bound raw basis.


### Yard roof dust appearance correction — 2026-09-24

The user rejected Trial04b's hard-edged light dust islands on bare roof metal as white pecks. A deposit mask can be numerically coherent yet read as unintended paint flakes. Localized opaque deposits used to protect player recoloring are not a universal bare-metal weathering recipe. Trial04c removes this known final dust layer from roof-owned linear color, roughness and metallic together, clears its semantic dust selection, and preserves other grime, surface variation and physical bakes. An unchanged-light before/after confirms the broad white flecks are gone. See `art/construction_yard/refine_material_trial_v04c_roof.py` and its retained numeric recovery diagnostics.


### Yard concrete-only response delta — 2026-09-24

Trial04d selects concrete by high-source ID and receiver ownership, excluding attached metal fittings. When broadening existing deposits in UV space, normalize the filtered mask by the same material ownership so neighbouring atlas materials cannot seed stains. Verify all pixels outside the selected material remain bit identical, couple added grime to roughness, and retain explicit raw-basis provenance when delivering a decoded-PNG delta rather than a fresh bake. The matched-camera review and derivation are in `art/construction_yard/MATERIAL_TRIAL_V04D_REVIEW.md`; artistic acceptance remains pending.


Trial04d was subsequently rejected because its concrete darkening did not read clearly to the user. A numeric albedo reduction is not visual acceptance. Trial04e uses a stronger concrete-only response and a matched-light crop comparison against the rejected 04d; keep the affected surfaces explicit in the review. See `art/construction_yard/MATERIAL_TRIAL_V04E_REVIEW.md`; renewed artistic review is pending.


### Yard concrete relief reference lesson - 2026-09-24

For reference-led concrete depth, author aggregate/pore/spall height independently of pigment and photographed lighting. Trial04f samples continuous world-space detail into approved UVs, adds a separate 16-bit Non-Color height plus explicit concrete-only mask, and composes it after the retained physical normal and coating bump. Inspect matched close-ups and reversed lights; preserve silhouette chamfers and verify metal fittings and non-concrete channels are unaffected. Shader relief is not actual displacement, and its runtime transfer remains a separate integration check. See `art/construction_yard/MATERIAL_TRIAL_V04F_REVIEW.md`; artistic acceptance remains pending.


### Medium Tank normal-source occlusion and atlas density — 2026-09-24

A full tyre endcap hid the recessed high-poly wheel dish during projection.
Removing the full disk alone exposed a narrow gap at the rim. On temporary bake
copies, retain an annular tyre backing seated behind the dish lip, remove the
occluding central disk and overlapping dish endcaps, and preserve the approved
source file. The corrected Medium wheel sources pass all 8,928 sampled rays;
actual normal-only wheel renders show dishes, hubs and bolt rings. Record such
source repairs explicitly rather than silently treating holes as neutral normals.

The previous shared atlas gave thin wheel rims insufficient pixels. Dedicated
WheelUV and TrackUV normal layouts provide more density without changing the
approved mesh or its paint UVs. Explicitly connect the matching UV map to both
the image texture and tangent Normal Map node; pack the physical maps. The 4K
layouts have zero overlaps at texel-center coverage sampling. This is an
authoring choice, not acceptance of a runtime texture budget or multi-UV importer.

Evaluating and joining the translated high-source copies before a grouped bake
resolved the observed repeated-buffer memory failure. Compare reproduced corner
normals against the actual receiver; body parts that fail the .001 delta check
must use original-position projection instead of relaxing the tolerance.
See art/medium_tank/texture_v06 for physical bake records and rendered evidence.


Medium NormalBake06 additionally exposed reversed faces that plain two-sided
clay shading concealed. An exterior multi-angle ray audit identified ten low
hull faces and twenty evaluated high-source faces. Preserve vertex positions and
per-corner UV correspondence when correcting winding, retain a precise face
list, and rebake affected receivers. A cage-distance change alone did not cure
this defect. Low corrections belong in a separately saved inspection mesh;
high-source corrections stay explicit in temporary bake copies. See
`HULL_FACE_ORIENTATION06.json`, `ORIENTATION_DELTA_VERIFIED06.json` and
`HIGH_EXPOSED_BACKFACES06.json` in the Medium texture_v06 directory.


The twenty high-source exterior backface candidates above are diagnostic findings,
not accepted repairs. Flipping all twenty or flattening broad source shading
normals introduced regressions and was rejected. Only the verified ten low-hull
orientation corrections and the wheel source cap/annulus repair are retained in
NormalBake06. Preserve these distinctions when resuming material work.


## 2026-09-24 Construction Yard runtime transfer lesson

Validate the approved material beside the current Mammoth in the actual game under
the same lighting before calling the transfer complete. Yard Trial04f initially
exported at roughly four times the Mammoth paint reflectance and looked washed out.
The user requested darker matching values and less shine. Record the explicit
asset-map correction and preserve the shared light rig, geometry, bevels and masks.
The portable runtime recipe and engine evidence are in
`art/construction_yard/runtime_v01/README.md` on `a development branch`.

C3M v1 has no height input. Bake the complete high-normal/coating/concrete bump
chain into tangent normals before export. Flatten each atlas on a temporary joined
receiver for performance, while preserving original low objects, UVs and corner
normals. Keep separate linear grayscale player masks and verify actual GDI/Nod views.
A Blender render is not engine proof. Building lifecycle tests also need the brain's
CONQUER.MIX buildup data; a logged sell click alone does not prove a sale occurred.


The retained front-lip correction is eight planar low faces with planar corner
normals, followed by a new physical hull bake. Original corner normals tilted
strongly across a thin UV strip, demanding an under-resolved inverse tangent
correction: one reconstructed normal pointed behind the front plate. Source
flattening, source winding changes and cage-only tests did not fix that cause.
After the low planar-normal correction, reconstructed sample normals agree with
the high front face and the black wedge disappears in the actual close-up.
Vertex positions, polygon vertex sets, UV correspondence and 9,940 triangles
are unchanged. See `PLANAR_LIP_NORMALS06.json` and `NormalBake06_Track.png`.

2026-09-24 MeshRepair07 correction: NormalBake06 did not establish clean geometry.
Normal-off grazing close-ups exposed front lip and rear corner defects hidden in
wider views. Require actual local surface inspection in addition to manifold and
triangle counts; Solidify can conceal broken topology with a folded closed shell.
Changed hull topology invalidates its previous UV/bake and requires renewed mesh
approval. See art/medium_tank/mesh_repair_v07/README.md and VERIFICATION.json.

### CUR00 original bounds and first mesh review — 2026-09-25

Measured original visible-arrow pack bounds: X 15..445, Y(up) 100..157,
Z 7..914; root/pivot (0,0,0). The additional fully transparent marker expands
complete source bounds to X -107..445, Y 0..157, Z -92..914 and must not be
mistaken for physical arrow geometry. The original inspection Blender scene
uses `(pack_x,-pack_z,pack_y)*0.01`, not the later fitted-vehicle convention.
CUR00 InitialLow01 preserves the original root and physical horizontal bounds
to 0.001 pack unit, with an artistically raised face at Y=178. The new closed
mesh has 94 evaluated triangles; saved-file reopen confirms zero non-manifold
edges. Colored/clay views are first-mesh review evidence, with provisional
materials/emission and no final UVs, bakes or texture maps. Initial mesh approved
2026-09-25 (see `art/cursor/initial_low_v01/APPROVAL.md`);
this is not a final asset or runtime integration. Exact source/reference/file
hashes and comparisons are in `art/cursor/initial_low_v01/verification.json`.


### 2026-10-06 Runtime stage verification

For a staged runtime asset, distinguish cumulative effects history from the live draw
list. OpenCNC3D's `EFXSEEN` records fires observed before filtering; `EFXDUMP|ANIM` describes
live effects after filtering. G312 must use the latter to reject duplicate engine fires
beside the approved burning-tree mesh. Verify stage pixels against a control and repeat
runs, and report engine-retained versus orphan lifetimes separately. A conditional code
path is not covered merely because the retained-tree case passes. Preserve source/export
hashes and artistic approval separately from these runtime checks. Runtime coverage is
checked by `game/gate_treefire.py`.

## RA intact runtime import, 2026-10-06

The approved intact export and later lab stages have separate runtime boundaries. An
intact-only EBLD may omit the scorch texture when the original game owns all later states.
Do not borrow another game's scorch or include unapproved damage merely to satisfy an
importer assumption. Use the correct game's engine footprint and plot-turn metadata;
rotated flag waves must turn their origin and both local horizontal axes with the mesh.
See [RA structure runtime](ra3d-structures-runtime.md) for the version/hash locks and gate.

The RA regression gate now checks actual root/child sprite suppression, infantry damage
without a model-to-sprite switch, approved defense exports and turret facing. Snow aprons
reuse the approved painting at shared strength with painted coverage, bypassing the
grass-to-brown gain. Fog on/off pixel comparisons must change the far world while leaving
near ground and the sidebar unchanged. Keep these runtime checks separate from approval
of authored damage, placement, recoil and destruction.
