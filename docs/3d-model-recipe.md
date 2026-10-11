# 3D model recipe v1.2 — approved Mammoth-quality method

**Status: production-ready for a fresh asset session as of 2026-09-24.** The
procedure is complete when used with its linked pipeline, current repository
instructions and user approval gates. New measured lessons should be appended;
historical asset values must not silently replace current budgets or code behavior.

Approved by the user on **2026-09-22**, following the actual Mammoth Lookdev05
result: “This looks great. I think we have a receipe.” The user then specified
the production order below. This recipe replaces the earlier rule requiring
generated concepts before any mesh. Read it alongside [the pipeline](3d-model-pipeline.md).
The user's subsequent amendments add mandatory approval stops at both low-poly
stages, generated views and high-poly. The current repository ceilings are
**10,000 triangles for vehicles and 10,000 for structures/buildings**, reaffirmed
by the user on 2026-09-24. These supersede the erroneous 5,000/8,000 limits.
Mammoth's later 9,990-triangle RuntimeRig01 fits these category ceilings.
The preserved approved Lookdev05 Mammoth checkpoint remains
unchanged at 2,888 triangles.

This is the repeatable authoring method, not a claim that the existing scripts
are a portable asset factory. It is proven here for a rigid hard-surface vehicle.
Adapt moving-part, organic, building or skeletal requirements explicitly rather
than copying the Mammoth's topology, wheel counts, dimensions or budgets.

**All-asset quality amendment, 2026-09-23:** the reference inventory, designed
detail, bake-transfer and material checks below are mandatory for every future
asset and revision using this recipe, including assets that reuse approved or
default materials. They apply to the asset's relevant assemblies and material
classes; they do not require every asset to contain tank parts or exposed metal.
These are evidence requirements within the existing high-poly, final-mesh,
representative-material and finished-asset gates, not additional approval stops.
The [Mammoth audit](../art/mammoth_lowpoly/reviews/20260923_gpt_comparison/MATERIAL_HIGH_POLY_AUDIT.md)
supplies the measured lessons; its numeric settings remain case history.

**Post-high-poly finish amendment, 2026-09-23:** after explicit high-poly approval,
run the approved actual mesh views through GPT Image again with the asset-selected
ArtStation finish reference. This second image stage proposes the finished
texturing/material treatment on the now-established geometry. Present it for
finish-direction review and selection before final-low work. It supplements the
original design-image stage and all existing mesh/material approval gates.

## 0. Fresh-session execution contract

A new session can use this file as its primary authoring procedure, but it must
also read [3d-model-pipeline.md](3d-model-pipeline.md),
[art-return-contract.md](art-return-contract.md) before runtime delivery, and the
repository's current `repository rules`. Current user direction and current code override
historical examples. Do not use the Mammoth scripts as an automatic asset factory;
reuse their measured methods, evidence format and quality bar.

At the start, create `art/<asset>/ASSET_DOSSIER.md` from the requirement in section 12.
Fill every known field before modeling and append evidence as work advances. Record
unknowns explicitly. If the original OpenCNC3D asset cannot be resolved and measured,
stop before inventing a footprint and ask for the intended reference or dimensions.

Run the stages in section 1 in order. At each approval stop, present the required
actual or generated evidence, record the exact approved version/hash/date/quote and
wait. A new session must not silently infer approval from an earlier asset, from a
general “continue”, or from a generated image. After requested revisions, return to
the affected gate.

“Equal quality to the Mammoth” means the new asset passes the same kinds of evidence,
not that it copies Mammoth geometry, noise, palette or its historical triangle
override. Do not claim that quality until all applicable conditions below are true:

- online subject research, original-game measurement and reference roles are saved;
- the approved design inventory accounts for every characteristic assembly and
  records deliberate adaptations or omissions;
- the high-poly shows readable construction, fitted attachments, real seats/joins,
  controlled chamfer hierarchy and no visible clipping in matched multiviews;
- the final low preserves the approved silhouette, negative spaces, ground contact,
  footprint, pivots and moving clearances within the current category ceiling;
- every required detail is assigned to retained geometry, a named bake source and
  receiver, or an intentional material-only representation;
- delivered-resolution physical-bake comparisons prove bevels, recesses, seams and
  attachments under multiple grazing-light directions and at gameplay size;
- the PBR set has coherent material classes, coupled deposits, spatial roughness,
  verified OpenGL normals and a readable player-colour mask at actual game zoom;
- the clean delivery scene, manifests, hashes, cross-format counts and fresh-reopen
  tests pass, and runtime capability is described without claiming unsupported
  controls;
- the user has explicitly approved the first low, first generated sheet, high,
  post-high finish direction, final low, representative material and finished asset.

Technical checks cannot grant artistic approval, and artistic approval cannot prove
export or runtime behavior. Preserve those states separately throughout the dossier.

## 1. The mandatory order

```text
Online design/material research
  -> look up the original in OpenCNC3D source and measure its footprint
  -> initial low-poly design mesh in Blender
  -> STOP: user approval of the first low-poly
  -> actual multi-angle renders of that mesh
  -> GPT Image gritty multi-angle concepts using those renders + ArtStation finish reference
  -> STOP: user approval of the first consistent, versioned generated views
  -> detailed high-poly based on that approved sheet
  -> STOP: user approval of the high-poly
  -> actual approved high-poly views + selected ArtStation finish reference
  -> GPT Image high-quality finish-direction renders, preserving that geometry
  -> STOP: user review and approval of the generated finish direction
  -> final optimized low-poly derived/refined against the high-poly
  -> STOP: user approval of the final mesh
  -> final shading basis, triangulation and UVs
  -> representative-section normal/AO/material trial and approval
  -> complete matched bakes and textures
  -> actual-model visual/technical QA, save/reopen and full-asset approval
  -> separately authorized and verified export/runtime integration
```

**There are two low-poly stages.** The first establishes the real shape, proportions,
footprint and part layout that GPT Image must respect. The second is the final
budgeted mesh supporting the high-poly result and the exact tangent-space bake.
The second may reuse good topology from the first, but must be deliberately
revisited after the high-poly; do not simply declare the rough source final.

### Mandatory user approval stops — every asset

| Gate | Present for approval | Do not begin until approved |
| --- | --- | --- |
| First low-poly | Actual clay multiviews, evaluated count and source-footprint comparison | GPT Image concepts |
| First GPT Image generations | Clearly labeled, coherent generated multiviews and saved input/prompt provenance | High-poly |
| High-poly | Actual source views showing the approved forms and physical bake details | Post-high GPT Image finish pass and final optimized low-poly |
| Post-high GPT Image finish direction | Clearly labeled finish concepts from the approved high renders, provenance and geometry-deviation review | Final optimized low-poly |
| Final mesh | Actual final-low views, evaluated triangle count, footprint/clearances and shading checks | Final UVs, bakes and texture production |

**Stop the turn and wait for explicit approval of the presented version at every
gate.** Approval of an earlier stage, another asset, or a general “go ahead” to
start is not approval of an unseen later result. Record the user's decision,
version and date. Requested changes return to that gate for approval. Later
representative-material and finished-asset approvals remain additional gates.
The post-high finish review adds a direction checkpoint; it does not replace any
of the four original approval stops or reopen unrelated historical approvals.
Honor a later explicit user instruction to revise the high and then regenerate
the finish in one preview sequence: perform both requested actions and present
the new actual high plus generated finish together for renewed acceptance before
final-low work. General permission to start/continue is not that instruction.

### Category triangle ceilings

- **Structures/buildings: maximum 10,000 triangles.**
- **Vehicles: maximum 10,000 triangles.**

These apply to the complete evaluated final runtime mesh, including all rigid or
animated parts, backs/hidden runtime faces and modifier output. Exclude only
explicit non-export high-poly, cages, references and QA objects. Keep the first
low-poly within the relevant ceiling as a practical design preview; the high-poly
bake source is not restricted by the runtime budget. A ceiling is not a target.
An explicit asset-specific override must be recorded; do not automatically inflate
an existing approved model. Mammoth's 3,000 Lookdev05 ceiling and later 10,000
RuntimeRig01 budget describe historical approved checkpoints; the latter also
matches the current general vehicle limit. No historical instruction waives final-low approval before
final UVs, bakes and textures.

Generated concept images are art direction. Actual mesh renders are evidence of
the mesh. Neither a generated view nor a triangle-count label inside an image
validates geometry, UVs, materials, dimensions or game integration.

## 2. Reference pack and appearance authority

The default finish reference for this approved hard-surface recipe is
[Repulsor Executioner Blood Angel — Enclave Interactive](https://www.artstation.com/artwork/J9Z64d).
Its [artist-authored repost](https://www.zbrushcentral.com/t/repulsor-executioner-blood-angel/412274)
also records the work. Both pages were checked on 2026-09-22; ArtStation's text
extraction is limited, so retain the project provenance rather than guessing
unavailable technical material settings from the page.

The user also supplied [EXAR - VARGHULM](https://www.artstation.com/artwork/6LX5K5)
as an earlier quality reference; **J9Z64d is the preferred hybrid hand-painted/PBR
finish reference**, not an interchangeable secondary choice.

Use the references with distinct roles:

| Input | Authority | Must not be inferred from it |
| --- | --- | --- |
| Online subject research and user design references | Identity, construction logic, distinctive parts, unseen-surface research | Exact game dimensions from perspective pixels |
| Original asset looked up in OpenCNC3D source / explicit brief | Mandatory footprint, axes, ground contact, pivots, role constraints | A demand to copy every old geometric detail or a guessed generic one-cell size |
| Initial low-poly and its actual renders | Proposed 3D shape, part placement and proportions for GPT Image | A completed final bake mesh |
| ArtStation J9Z64d | Layered painterly finish, gritty midtones, local abrasion and material separation | Red faction palette, insignia, geometry, copied textures or baked lighting |
| New Mammoth RuntimeRig01 actual material screenshots | Shared GDI material/texture style: tan paint, dark steel, edge wear, grime distribution and texture scale | Mammoth geometry, weapon count, footprint, or permission to reuse rendered pixels as textures |
| Approved generated sheet | Coherent high-poly/final-low/texture art direction | Actual mesh, true orthographic calibration, UV atlas or valid normal map |
| Approved actual high-poly and its saved views | Geometry, construction, part layout and camera authority for the second GPT Image pass | Completed runtime mesh, final materials or engine validation |
| Selected post-high generated finish sheet | Target paint/material separation, metal response, roughness variation and weathering on the approved forms | Permission to change approved geometry, measured PBR values, bakes or actual Blender/runtime proof |
| Latest explicit user amendment / approved actual model | Current artistic decision | Export, engine or target-resolution acceptance not explicitly tested |

When the user selects a replacement design reference, explicitly refresh the
feature inventory instead of accumulating its details over incompatible old
forms. Mark which prior layout, coverage and construction choices it supersedes.
Reconcile primary silhouette and assembly counts first, then fit secondary
plates and hardware to those forms. An earlier clearance pass applies only to
the unchanged geometry it actually checked; replacing armor coverage requires
fresh checks of the complete running gear and all relevant receiving surfaces.

For every asset, make a **reference feature inventory** before the initial mesh.
Compare online design references with the complete original OpenCNC3D asset and the
user's brief. Record identifiable assemblies, characteristic construction,
functional openings, moving interfaces and material regions, with source views.
Identify features present in a reference but missing or simplified in the original,
and features that the proposed replacement would omit. Give each an explicit
retain/reinterpret/omit decision and reason; preserve measured footprint authority.
Unseen or ambiguous features remain labeled interpretations. Resolve important
omissions through the existing design approvals rather than silently losing them.
After generated-view approval, update the inventory with accepted additions and
amendments and carry it through the high, final low and mapped-material reviews.
Finish-only references supply craftsmanship and material behavior, not a list of
foreign components to transplant.

For the Mammoth, the source design is the
[Tiberian Dawn render](https://cdn.cnc-comm.com/gallery/command-and-conquer/Renders/MAMOTH.png).
The selected concept is
[v03](../art/mammoth_lowpoly/references/Mammoth_MultiAngle_TextureDirection_v03.png),
with fixed red rocket tips. Its paler pigment was superseded by the approved
[DarkGritty v02 panel](../art/mammoth_lowpoly/textures/lookdev04/source/ArmorPanel_Paint_AI_v02_DarkGritty.png)
and finally the approved actual Lookdev05 result. Do not reset that result to
the older pale sheet merely because it is the concept reference.

**Mammoth finish amendment, 2026-09-23:** the user now requires the style,
materials, edges and metallic response of
[Gears 5 - UIR Tank — Andres Munar](https://www.artstation.com/artwork/baZGeo),
alongside the supplied Mammoth generated sheet. This is the current revision's
finish/quality target; Lookdev05 remains a preserved historical approval, not
proof of parity with this target. The page shows both an untextured high-res
model and finished views, with textures credited to Nicole Plum. Study designed
secondary forms, bevel highlights, layered coatings, localized metal exposure
and directional weathering. Do not copy its tank geometry, markings, palette or
image pixels into the asset, or infer exact shader values from its lit images.
This direction amendment does not approve any revised high, low or material trial.

**Mammoth HP09 approval and finish-pass choice, 2026-09-23:** the user explicitly
approved [Highpoly09](../art/mammoth_lowpoly/highpoly_v09/HIGH_POLY_REVIEW.md),
SHA-256 `369f620a52b482fe081e45f847079275b9eee9725b3a448916df7afddcdfe29d`,
then requested the new post-high GPT Image stage using **baZGeo** for texturing,
materials, metals and grittiness. Its approved Hobma-derived geometry remains
fixed. Use baZGeo for this asset's finish pass rather than silently substituting
the general J9Z64d baseline; approval of HP09 does not approve an unseen generated
finish or the final-low/material stages.

**Current Mammoth acceptance, 2026-09-23:** after the actual HP10 chamfer revision
and GPT/Ideogram comparison, the user selected the corrected GPT finish and
explicitly directed final-low work. The accepted actual high is SHA-256
`69ccef556e5460d654aeb7557af68a088e7d83d70e9f42359098671006b09615`; the selected
finish is `Mammoth_HP10_FinishDirection02.png`, SHA-256
`d486312784445eba4bb651436590a0f6d17c5b7c40752c71711d452dae0ee9b0`.
See the [hash-bound decision](../art/mammoth_lowpoly/highpoly_v10/HP10_FINISH_APPROVAL.md).
Ideogram remains an unselected experiment. Construct the separate final low
under Mammoth's then-approved historical 10,000-triangle override, then present it
for approval before final UVs/bakes; the selected image is not approval of real
mapped materials.

### Comparing generated finish methods

Keep geometry fidelity and finish quality as separate acceptance tests. In the
[Mammoth comparison](../art/mammoth_lowpoly/references/ideogram4_highpoly10/README.md),
the combined geometry/finish input produced major design drift in the first
Ideogram result. A geometry-only input at weight 90 retained more structure but
largely retained the clay appearance; weight 70 allowed more sand paint but still
altered details and gave weaker paint/metal separation than the corrected GPT
candidate. Treat this as observed evidence of a geometry-versus-transformation
tradeoff in those requests, not a universal ranking or a universal weight setting.
Input layouts, reference presentation, prompts, resolutions and iteration counts
differed. Record these differences and inspect actual outputs, including part
counts, camera views, material separation and localized wear. The user retained
GPT Image as this recipe's chosen workflow. A future alternative is an explicit
experiment, not a silent replacement of the selected finish or mesh authority.

Keep exact URLs, titles/attribution, selected image filenames, date, source roles,
generation/edit prompts, output hashes and approval history. Existing ArtStation
reference provenance is in
[third_party/README](../art/mammoth_lowpoly/references/third_party/README.md).
Reference artwork is not automatically licensed for texture reuse or shipping.
Do not copy its image into an atlas or include it in a runtime pack. Do not feed
visible watermark/text contamination into a material-generation pass.

## 3. Research and make the first low-poly

### Proxy-first authoring amendment — 2026-09-24

When a suitable mesh is available in `art/enhanced_proxy_meshes/`, use it as the
starting design mesh. The user explicitly selected `TD_VE_MTNK.obj` for the GDI
Medium Tank. Preserve the supplied proxy and work on a separate versioned copy.
Research online references, measure the original OpenCNC3D asset as usual, then make
only the simple necessary proportion, silhouette, placement and missing-feature
adjustments. Preserve useful topology and assemblies; do not rebuild the initial
low from scratch. Record source hashes and a concise retained/changed inventory.

Present that adjusted actual mesh at the first-low approval gate, then use its
actual views for GPT Image. After generated-design approval, duplicate and refine
the adjusted model into the high-poly instead of rebuilding from scratch. Replace
individual parts only where the approved direction or demonstrated topology/shading
defects require it. This reuse is intended to save time and tokens; avoid unnecessary
detail or wholesale reconstruction. All footprint, triangle, approval, final-low,
bake and material checks still apply. The proxy is a geometry starting point, not
the authority for original-game dimensions or proof of production-ready textures.

1. Research the subject online: original concept/render, multiple views, real
   construction references where useful, and the selected finish reference.
   Distinguish verified shapes from interpretation. Record uncertainty about
   hidden surfaces instead of letting an image generator silently decide them.
2. Resolve the requirement: triangle ceiling, target camera/platform, intended texture
   budget, required moving assemblies, fixed accents and player-colour regions.
3. **Look up the original asset in the OpenCNC3D source before modeling.** Resolve its
   game/type code and complete part set, then extract/inspect its rest-posed
   geometry using the project's own readers. The original footprint must be kept;
   online art and generated images are not its dimensional authority. Record
   source identity, origin-relative bounds, hull/contact footprint separately
   from overhang, ground plane, axes, scale and pivots. Inspect relevant factory
   or environment constraints. Do not rely on an incomplete legacy OBJ.
4. Create an **initial low-poly design mesh** with recognizable massing, functional
   part placement and openings. Give it meaningful silhouettes, not just generic
   boxes. It may remain untextured/clay; no final unwrap or bake is required yet.
5. Fit footprint without blindly matching old height or repeatedly stretching
   an already stretched mesh. Preserve round gun sections, proportion balance
   and moving-part clearances. Save a versioned source snapshot.
6. Inspect front, rear, side, top and three-quarter views. Resolve major clipping,
   impossible supports or incorrect counts before making this mesh an AI input.
7. **STOP for user approval of the first low-poly.** Save its actual views, count
   and original-footprint comparison. Do not generate GPT Image views yet.

Construction Yard first-low review verified why true side elevations matter:
near-identical pitched front/hero views hid a short rear-wall return and small
floating supports. Check component-to-component contact as well as each mesh's
closed topology. A manifold part can still float above its support. Match the
ground-contact outline/corners separately from the complete overhang envelope;
containment alone does not prove the original contact footprint is preserved.

The first low-poly is the user's required geometry anchor for image generation.
Do not reverse this order by generating a whole-asset concept from text alone
and treating a later mesh as though it had constrained that concept.

### Mandatory OpenCNC3D original-footprint lookup

Prefer [tools/art/fbxout.py](../tools/art/fbxout.py) with the relevant baked pack
and type code; keep its FBX and JSON part/pivot contract as reference, not as files
to overwrite. If no suitable pack is available, use the existing project source
readers: `tools/bakery/objgraph2.py` for parts/rest transforms and
`tools/romdump/model.py` for display-list triangles. Cross-check the part counts
against `tools/bakery/parts_survey.json`. The Mammoth example is
[extract_opencnc3d_reference.py](../art/mammoth_lowpoly/extract_opencnc3d_reference.py),
which is HTNK-specific and must not be run unchanged for another unit.

Measure only triangle-referenced vertices after applying each part's rest pose.
Exclude explicitly identified shadow/decal-only geometry from physical bounds;
record the exclusion rather than deleting source evidence. `FACT` demonstrates
why: eight shadow faces would enlarge the original physical footprint. Preserve
asymmetric contact/overhang outline guides when a rectangle loses important shape.
Save the source pack/ROM identity and hash, type/part identifiers, units/axis
conversion, origin-relative horizontal min/max bounds and ground plane in a fit
report. Preserve both the hull/contact footprint and the documented overall
horizontal envelope including overhang; matching width/length while shifting
the model away from its original origin is not sufficient. Check pivots and any
approved overrides separately. A gameplay 1x1 occupancy cell is not the visible
model's footprint.

Keep a non-export footprint guide in Blender. Compare the initial low-poly, final
low-poly and export copy against the same source measurements, and recheck after
proportion changes, bevels or other silhouette edits. Record numeric deltas and
an explicit small conversion/rounding tolerance; do not quietly enlarge the
envelope to accommodate a new detail. If source identity is missing/ambiguous, or
the requirement has no original counterpart, request an explicit reference/envelope
before proceeding instead of inventing a size.

This preserves **overall footprint, not exact legacy geometry**. The user permits
artistic freedom in proportions/detail and height, subject to explicit clearance
constraints. Keeping the footprint does not itself prove a moving factory exit
or turret sweep fits; those remain separately tested. See the pipeline's measured
HTNK case for the original and approved result.

## 4. Generate the gritty views from the real mesh

Enter this stage only after explicit first-low-poly approval.

Render the initial mesh from front three-quarter, rear three-quarter, side and
top, with consistent pose/scale, clear visibility and neutral lighting. Add a
front or opposite-side view when needed. Include complete extremities. Save the
actual renders and camera matrices separately from generated images.

Use the available built-in GPT Image generator. Supply the actual mesh renders
as the **geometry/proportion inputs** and the ArtStation image as **finish only**.
For the Medium Tank and matching GDI vehicles, also supply the new Mammoth
screenshots listed in section 9 as the primary **in-project material/style match**.
Match that finished asset's paint tone, quiet panels, dark gunmetal, broken silver
edge wear and localized lower grime; ArtStation remains a craftsmanship reference.
Include these actual screenshots in both image passes, not merely a text mention.
Use the current image-generation instructions/tool; do not assert an exact model
version unless the tool exposes it. For this baseline, it did not.

Adapt and save this prompt scaffold rather than copying Mammoth-specific counts:

```text
Use case: stylized-concept
Asset type: consistent multi-angle game-asset material/design reference sheet.
Inputs 1..N: actual Blender renders of [INITIAL LOW-POLY VERSION]; these establish
the shape, footprint, part count, placement and proportions.
Finish input: Enclave Interactive's Repulsor Executioner, J9Z64d, FINISH ONLY.
Shared-style inputs: new Mammoth RuntimeRig01 assembly, rear and gun screenshots;
match its material/texture language while preserving this asset's own geometry.
Show the SAME [ASSET] in front 3/4, rear 3/4, side and top views, in the same pose.
Preserve [IDENTITY, PART COUNTS, OPENINGS, MOVING PARTS, CONSTRAINED SILHOUETTE].
Develop a gritty hand-painted/PBR treatment: darker readable painted midtones,
broad deliberate color masses, varied brushwork, selective broken edge chips,
layered primer/steel, contact scuffs, localized dust/handling grime, controlled
metal highlights and rough rubber. Keep quiet areas; avoid indiscriminate noise.
Retain [ASSET PALETTE], [FIXED ACCENTS] and [PLAYER-PAINT LOCATIONS].
Do not borrow the finish reference's geometry, red palette, insignia or lettering.
Do not add inconsistent hardware between views or reverse openings on the rear.
Neutral comparable illumination, full subject visible, simple background.
This is a concept sheet, not a UV atlas or proof of mesh/triangle count.
```

Inspect part counts, front/rear orientation, shared wear/paint placement, silhouette,
player regions and fixed accents across views. Correct a versioned sheet if it
contradicts itself. If a useful AI-proposed structural detail is accepted, record
it in a detail plan for real 3D implementation. Resolve conflicts with measured
dimensions/budget before proceeding. Obtain **user approval of the selected sheet**.
Stop here until that approval is explicit; no high-poly work in advance.

Historical exact prompts: [initial sheet](../art/mammoth_lowpoly/references/Mammoth_MultiAngle_TextureDirection_v01.md),
[rear correction](../art/mammoth_lowpoly/references/Mammoth_MultiAngle_TextureDirection_v02.md),
[red tips](../art/mammoth_lowpoly/references/Mammoth_MultiAngle_TextureDirection_v03.md).

## 5. High-poly, generated finish direction, then final low-poly

### High-poly from the approved gritty result

- Preserve the initial mesh and build a named, separate high-poly source from it,
  revising it to embody the approved generated views. Keep the design sheet and
  measured dimensional constraints visible during authoring.
- Model genuinely rounded barrels/collars, readable chamfers, panel thickness,
  joins, vents, bolts and recesses intended to affect light. A panel painted by
  GPT Image does not appear in a normal bake until a physical/controlled-height
  source represents it.
- For texture-driven tracks, model recessed/perforated wheel dishes, rubber rims,
  hubs/bolts, drive sprocket teeth, idlers, return rollers and side-link/pin relief.
  Fit front and rear layouts separately. Inspect with armor installed: hidden
  end supports do not solve unsupported-looking belt bends.
- Use one shared measured layout for source relief and later paint/PBR selections:
  centers, extents, seam widths, bolt radii, recess depth and wheel positions.
- Check high-source shading. For long cylinders, cap/bevel averaging must not bend
  the wall normal into an axial bulge; use suitable split/analytic source normals.
  High-poly geometry is hidden/non-export and outside the runtime triangle count.

Before high-poly approval, every asset must have a **per-assembly detail plan**
linked to its reference feature inventory. Record the reference crop, primary
form, secondary construction, tertiary relief, named physical source and intended
low-geometry/bake/material allocation. Cover all exposed sides and relevant moving
or interior interfaces. For hard surfaces, construction can include plate
thickness, layered covers, seated hardware, joints and vent depth; choose features
from this asset's design. Adapt the same hierarchy to other forms instead of
adding a generic mechanical-detail kit.

Audit **how parts are joined**, not only whether the parts exist. Every exposed
interface must have a plausible construction treatment appropriate to the asset:
a fitted seat and flange, a welded seam, bolted bracket, bearing, gasket, socket
or deliberate clearance. For hard surfaces, distinguish permanent welded joints
from removable covers and moving pivots. Fit the attachment to the actual receiving
surface before adding weld toes or fasteners; a bead over a visibly intersecting
slab does not repair the joint. Keep intentional buried overlap within the seat
and check visible contact edges in close-up and under grazing light. Use real
high-source geometry for weld/seat relief that must bake, with a restrained scale
and variation consistent with the chosen finish reference.

Apply a **visibility and bake-contribution filter before adding detail**. Classify
each proposed surface as visible from the intended asset views, contributing
useful relief/occlusion to a visible bake receiver, or fully concealed. Build the
first two as needed; omit fully concealed wheel mechanisms, internal suspension
and similar assemblies that neither appear nor contribute to the intended bake.
Mechanical completeness is not a quality target. A diagnostic cutaway does not
make an otherwise hidden feature worth modeling.

Inspect undersides and inward-facing surfaces only where the intended views or
visible bake result require them. Exposed gun assemblies may need supported
under-barrels and fitted joints; exposed wheel backs or open gaps may need local
construction. Adapt that scope to the reference and bake receivers rather than
adding an entire internal mechanism. Keep the reference's designed coverage:
for example, armor skirts covering roughly half the wheels must retain that
coverage. Resolve cap clipping by fitting wheel depth and local housing clearance,
not by raising the skirt to reveal more wheels. Use close-ups of the actual
assembled model for the required visible joints and openings.

Check evaluated assembly clearance as well as individual mesh validity. Wheels,
hub caps, belts and moving hardware need actual space inside surrounding armor;
model the wheel well or housing cavity and fit its lip around the required exposed
parts. Review assembled front, side and low-angle views; use a labeled diagnostic
cutaway only when needed to resolve a specific visible defect. Use
bounding boxes to find candidate collisions, then test the actual surfaces and
containment where the boxes overlap. Document specific intentional mounting
contacts separately. A valid outer footprint and a clean beauty view do not prove
visible assembly clearance; inherited geometry must pass this check after
revision too. Track paths must remain beneath their front/rear fenders in the
assembled mesh. Check this before sharing approval images. Covered static
overlap and intentionally unmodeled interiors must be scoped explicitly instead
of triggering a blanket demand for invisible mechanical detail.

At curved or angled assemblies, test the complete evaluated piece, including
grousers, pins, fasteners and bevels. A link's center can fit a track path while
its corners cross a fender. Fit the whole visible assembly to its housing and
recheck actual triangle intersections/containment; do not declare it clear from
centerline or overall-footprint checks alone.

Collision scope must include every receiving surface visible at the interface.
Do not exclude an entire hull or parent assembly because some of its overlap is
concealed: Mammoth Highpoly06's pod-only check omitted the main hull and missed
22 front tread/hull contact pairs. Name the checked components and region limits
in the report, and show the formerly failing interface from both sides. A scoped
pass must never be described as clearance for the whole asset.

Review what each part **visually communicates**, not just its object name or
topology. A lamp needs a readable housing, bezel and optic; a hatch needs a fitted
perimeter and recognizable opening hardware; a mounted tool needs a supported
mount rather than a stack of generic blocks. Use silhouettes and construction
appropriate to the design, and replace inherited low-poly proxies when they fail
the high-source review. Do not add telescoping collars, exposed piston rods or
other visual motion cues to a component the requirement specifies as fixed. Preserve
reference-defined fixed shoulders, jackets and support collars: a fixed barrel
does not imply a uniform cylinder. Correct the misleading interface while
retaining the recognizable silhouette and construction in the user's reference.

Check proportions separately from the presence of the correct parts. A shaft,
muzzle jacket and collar can all be recognizable yet collectively too bulky.
Compare exposed length-to-diameter ratios and the thickness of neighboring
supports in true side and three-quarter views. When reducing bulk, adjust the
related rims, bores, clamps and secondary tubes coherently; do not leave an
oversized connector that defeats the slimmer silhouette. Preserve the agreed
source envelope while refining these proportions, unless the user amends it.

Derive attached covers and their fasteners from the receiving component's actual
local frame. A tilted housing must carry its side panel, recess and hinge in that
same frame; a world-aligned detail can be flush yet still visibly wrong. Check
level or upright reference features in true orthographic elevations. Preserve
intentional local chamfers without introducing a global lean into the assembly.

Measure the actual receiving perimeter before assuming it is rectangular. An
angled or sheared side can have two edge families that are not perpendicular;
a frame made from an adjacent face normal will then misalign its cover. Fit
the cover outline to the actual side edges, including shear/taper, and measure
parallelism and inset distances in a face-on view. Check both support interfaces
(support-to-parent and support-to-mounted-part), including inherited supports;
replacing a cover does not repair a block buried underneath it.

Declare where detail is concentrated and which broad surfaces stay quiet. Both
must be designed and visible in review: empty required construction is not a quiet
area, and uniform bolts, grooves or noise are not a rich surface hierarchy. More
radial segments, source polygons or repeated objects do not satisfy this plan.

Treat edge readability as modeled form, with a hierarchy suited to the selected
finish reference and viewing distance. Broad armor may need a wide planar chamfer
and a smaller softened shoulder; thin covers and tiny hardware need proportionate
edge treatment. Preserve broad planar faces and intended part thickness. Do not
apply one large bevel width to every edge, or mistake a generated bright edge for
physical high-source relief.

Measure the **evaluated chamfer**, not only its modifier width. Thin stock,
neighboring topology and overlap clamping can prevent a larger setting from making
a visibly wider edge. Inspect cross-sections or actual surface setbacks. Where
needed, revise plate thickness or build explicit planar edge strips, then refit
seats, covers and fasteners to the changed receiving surface. Removing redundant
decorative edge strips can give a clearer thick-plate highlight than stacking
several narrow rims.

Check evaluated local contour and component bounds as well as manifold topology
and the overall footprint. A fitted non-planar cap can produce long bevel-miter
spikes while remaining closed and inside the whole asset's bounds. Inspect such
joints in grazing and low-angle views; reconstruct stable planar chamfer strips
when a modifier cannot preserve the intended local outline. Recheck full assembly
clearance after all edge revisions, including unchanged moving parts against the
revised housing.

For an edge/readability revision, present matched before/after actual renders with
the same cameras, framing, lights, materials and exposure. Include both close-ups
and an equally reduced whole-asset view; zoomed highlights alone do not prove
distance readability. A reduced Blender render is useful comparison evidence,
not an in-game camera test. These checks were verified in the
[Mammoth HP10 review](../art/mammoth_lowpoly/highpoly_v10/INDEPENDENT_REVIEW.md);
the asset's renewed artistic approval remains separate.

Review matched actual high-poly views against the appropriate design and finish
references in neutral clay and with a grazing/moving key that reveals bevels and
recesses. Check inventory coverage, construction depth, attachments, detail scale
and rich-versus-quiet areas at the intended review distance. Resolve missing or
unreadable required detail, or record a proposed deliberate adaptation for the
existing high-poly approval. Passing counts/topology alone cannot pass that review.
Plan feature widths against intended texel density, but do not start final UVs or
bakes before final-low approval. Deep openings, silhouettes and parallax-critical
layers need a viable final geometry allocation; a detailed high alone cannot
preserve them.

**STOP for high-poly approval.** Present actual source renders and the physical
detail plan, not only the generated concept. Do not begin the second GPT Image
pass or final optimized low-poly until the user approves this high-poly version.
The explicit combined-revision exception in section 1 permits only the requested
preview sequence; it does not approve the revised high or authorize final-low.

### GPT Image finish direction from the approved high-poly

This is a required second image stage for future assets using the recipe. The
first generated sheet develops design from the initial low. This pass develops
high-quality finish direction from the approved detailed high. It is not a
texture bake, an actual Blender material preview or a new geometry proposal.

1. Bind the input to the approved high version, source-file hash and approval
   record. Use its actual front/rear three-quarter, side and top renders, with
   complete extremities; include close-ups when needed for material decisions.
   Preserve the actual source images, pose and camera matrices independently.
2. Supply those actual renders to GPT Image as geometry/camera authority and
   the currently selected ArtStation images as finish-only references. Use the
   asset's latest explicit finish choice; otherwise use the recipe default.
   For matching GDI vehicles, include the new Mammoth actual screenshots from
   section 9 again as the shared material/style authority.
   Keep any separate geometry reference's role explicit. State the approved
   palette, fixed accents, marking direction and planned player-colour regions.
3. Ask for polished, coherent finish renders: painted midtones and coating
   layers, localized chips/primer/bare metal, believable metal highlights,
   distinct roughness behavior, readable rubber/glass and intentional grit,
   dust, scuffs and contact grime. Preserve the approved silhouette, part count,
   dimensions, openings, joins, poses and exact view directions/composition.
   Do not let finish-reference geometry or newly invented hardware replace them.
4. Compare every generated view with its actual high input. Record missing,
   invented, moved or distorted geometry and inconsistent material assignments.
   Reject/correct material directions that rely on those deviations. If the
   user chooses a structural change, revise the real high and return to its
   affected approval gate before using that change downstream.
5. Save the exact prompt, input filenames/hashes, reference URLs/attribution and
   roles, generation date/tool provenance, output filenames/hashes, high-source
   hash, camera/view mapping, deviation notes and selection/approval history in
   a versioned companion record. Keep generated outputs separate from actual
   mesh renders. Record a model version only if the tool exposes it.
6. Present the result as **GPT Image generated finish concept — not a Blender
   render, texture set or physical PBR proof**. Return for direction review and
   explicit selection/approval before final-low construction. Keep actual high
   views available beside it so the accepted finish does not conceal drift.

Adapt and save this second-pass prompt scaffold:

```text
Use case: approved-geometry finish concept.
Inputs 1..N are actual views of approved [HIGH VERSION / HASH]. Preserve their
exact geometry, part counts, silhouette, openings, pose, camera and composition.
Finish inputs: [ASSET-SELECTED ARTSTATION REFERENCE / SOURCE ROLES], finish only.
Render the same approved asset with a high-quality [APPROVED PALETTE / MARKINGS]
material treatment: layered coatings, restrained exposed metal and edge wear,
distinct roughness for paint/metal/rubber/glass, localized grime and coherent wear
across all views. Preserve [FIXED ACCENTS] and [PLAYER-PAINT REGION DIRECTION].
Do not add, remove, reshape or relocate hardware. Do not transfer the finish
reference's geometry. Keep all extremities visible in the supplied views.
Output is generated finish art direction, not a UV atlas, measured PBR map set,
actual Blender material validation or proof of geometry/bake/runtime correctness.
```

Carry the selected finish sheet and its material-region notes into final-low
feature allocation and the later mapped trial. Do not infer precise metallic or
roughness values from a lit generated image; implement and validate real maps on
the approved final mesh under the existing UV/bake/material gates.

### Final low-poly after high and finish-direction approval

- Derive/retopologize or deliberately refine the initial low-poly against the new
  high-poly, carrying the selected post-high finish direction as the material
  target. Spend triangles on silhouette, negative spaces, bevels and useful
  openings, not tiny bolts that can bake. Keep editable sources separately.
- Preserve the OpenCNC3D original's measured footprint: compare origin-relative
  horizontal bounds with the saved source report and record deltas. Preserve
  ground contact, hierarchy and moving pivots. Check
  rails/launcher clearance and other adjacent parts, not only global bounds.
- Retain actual silhouette geometry where required; normals cannot make a polygonal
  silhouette round or create a real hole. Use separate closed track volume and
  continuous belt surface, not hundreds of animated link meshes.
- Carry broad form-defining and silhouette-critical high-source chamfers into
  actual low geometry, including exposed corners and edge bands that change the
  visible cross-section. Preserve their hierarchy, broad panel planes, local
  contours and fitted attachment seats within the measured horizontal envelope.
  Do not erase the approved edge treatment merely to reach a smaller count, or
  copy the high's segment density where fewer segments preserve the same form.
- Allocate fine rounded shoulders, small hardware bevels and shallow relief to
  named bake sources/receivers when they do not require silhouette or parallax.
  Compare matched high/low views and estimate feature widths at the intended
  on-screen scale and planned texel density. Nominal world-space bevel values or
  a modifier slider alone do not prove fidelity. After final-low approval, verify
  the physical transfer at delivered map resolution in the representative bake
  trial; broaden or reallocate unreadable features and repeat affected approval
  if geometry must change. Do not start production bakes to bypass mesh review.
- Set deliberate planar-panel and smooth-cylinder/chamfer corner normals. Count
  evaluated triangles on every intended model part, including backs/hidden faces.
  Exclude only explicitly non-runtime high/cage/guide/QA objects.
- Check degenerates, doubled faces, unintended holes, normals and visible clipping.
  Intentional attachment overlap is not an excuse for exposed intersections.
- Audit simplified thin fitted layers after triangulation and reopening. Collapse
  can merge nearby top/bottom surfaces into non-manifold edges even when the
  preview looks closed. Rebuild a clean fitted support when hidden cavities are
  unnecessary; preserve the visible receiver, seat and silhouette, and repeat
  topology/clearance checks. Mammoth FinalLow01's first fan-deck simplification
  exposed this failure and was replaced before review.
- Resolve archived rest transforms in their own evaluated scene or explicitly
  compose their parent/basis transforms. An excluded archival object's cached
  `matrix_world` may be stale; do not replace a recorded pivot with that value.
- Freeze the exact bake-target triangles **before the final unwrap/audit**. Preserve
  the same triangles, UVs and corner normals through bake, QA, save and export.

Reconcile the detail plan with the actual final low before its approval. Identify
each required feature retained as geometry, assigned to a named high/height source
and visible bake receiver, or intentionally represented only in material channels.
Show silhouette-critical layers and openings in the actual mesh. Explain relief
that is absent from the untextured low because it is allocated to a later bake;
do not silently discard it or begin bakes to decorate the mesh approval preview.

**STOP for final-mesh approval** after geometry/shading/count/footprint checks and
before final UVs, bakes and textures. Present actual model views and the measured
report. If later topology changes are necessary, return for affected mesh approval
and refresh the tangent-basis bake rather than silently replacing the approved mesh.

## 6. UVs and physical bakes

Use a unique `BakeUV` atlas for armor and static detail. Split UVs at actual hard
corner-normal discontinuities, give bevels sufficient pixels and leave gutters
for dilation plus filtering. A zero sampled-overlap count is not an exact/subpixel
or mip-bleed proof. Re-unwrap final triangles around concave plates/circular holes;
inspect small folds and add local seams if required, rather than lowering checks.

Create a separate `TreadUV` for moving belts where applicable: U across width,
V proportional to actual travel arc length, a hidden seam and a deliberate integer
repeat count. Sidewall/wheel artwork remains static. Assign separate L/R belt
materials sharing the same periodic images. Verify face-role masks, UV direction,
closure, physical pitch and sampling; do not run a topology test written for a
different number of segments.

Bake matched components, using only the intended high sources for each low target.
Prevent neighboring guns, launchers or moving assemblies contaminating one another.
Tune cage/ray distances to the actual model. Bake **tangent OpenGL +Y normal** and
component-local AO; record the scope rather than permanently baking a turret's
shadow into a hull that it rotates above. Retain coverage/miss diagnostics.

### Bevel-normal baking

Use bevel baking to preserve a controlled rounded edge highlight on the optimized
mesh when the edge does not need to change the silhouette. It is a transfer of
authored edge shape into the tangent normal map, not a substitute for modeling.
Silhouette corners, plate thickness, deep joints, welded seats, moving clearances,
openings and other mechanically readable construction remain geometry. A normal
map cannot round the outline, repair clipping, create a connection or supply
parallax.

Classify every important edge before baking:

- retain broad, silhouette-critical and gameplay-readable chamfers in the final
  low geometry;
- bake smaller visible shoulders and secondary hardware bevels from the approved
  high source;
- where no dedicated high source is warranted, make a temporary **bevel bake
  proxy** from an exact copy of the frozen low and author the bevel on that copy;
- leave intentionally sharp manufactured edges sharp.

The proxy method is the preferred controlled “bevel bake without a separate
sculpted high-poly” workflow. Keep the approved final low unchanged. Copy its
world transform and complete geometry into an enabled bake collection, apply
scale before evaluating world-space widths, and add bevel geometry only to the
declared edge set. Use bevel weights, marked edges or a reviewed angle selection
rather than beveling every edge indiscriminately. Give the source enough segments
to describe the intended round highlight, use stable outer/inner miters and check
overlap clamping, thin-stock collapse and long corner spikes. Smooth the rounded
source while preserving the broad planar faces. The proxy is a non-export source
and must be named and recorded like every other high source.

Size the bevel from the approved edge hierarchy and the final presentation scale,
then test it at the **delivered** texture resolution. A bevel should normally span
at least three to four real sampled texels across its rounded shoulder after
filtering; a one-pixel bright line is unstable and a high-resolution working bake
does not prove the delivery map. Broaden the authored source, allocate more UV
area, retain low geometry or deliberately omit the bevel when it cannot survive.
Do not enlarge every bevel simply to satisfy a texel count: preserve plate
thickness, quiet planes and the established wide/medium/fine edge hierarchy.

Freeze and triangulate the receiver before the bake. Set its production corner
normals first, and split UVs at hard corner-normal discontinuities. In the normal
bake, the source supplies the rounded shape while the receiver tangent basis
supplies the frame; changing receiver triangulation, UVs or corner normals later
invalidates the result. Keep gutters and dilation sufficient for the target mip
levels. Apply the same matched-component isolation and cage policy as other
physical bakes so nearby panels, brackets and parallel barrels cannot project
onto the bevel.

Use the approved physical high bevel when it already exists. Do not stack a
second proxy or shader bevel over it, and do not add a live runtime bevel effect
on top of a baked bevel normal. A procedural bevel/rounded-corner shader may be
used as a source only when the chosen Blender/Cycles version can bake it
deterministically into the same tangent OpenGL +Y normal convention. Compare that
result against a geometry-proxy control before adopting it; record renderer,
version, radius/width, segment or sample settings and any unsupported GPU path.
The exported material must not depend on that authoring-only node unless the
target runtime explicitly implements the same effect.

Validate bevel transfer with matched source, unbaked-low and baked-low views under
at least two grazing-key directions. Inspect both close-up and intended gameplay
size. Reject gradients across faces that should remain planar, waviness, skewed
corners, dark seams, projection misses, mirrored tangent errors, collapsed thin
parts and highlights that change width unpredictably. Rotate the light: painted
edge wear or a bright albedo stripe must not be mistaken for a normal response.
Also inspect the silhouette in neutral clay, because a convincing interior
highlight can conceal an unchanged polygonal outline.

Record per bevel group: source type and object, receiver, intended physical width,
selection rule, evaluated source segments, delivered texel width, UV island and
cage/ray settings, normal convention, matched-view evidence and the decision to
model, bake or omit. Curvature and edge-wear masks may be derived from the verified
bevel result afterward; they cannot serve as proof that the physical normal baked
correctly.

Evaluate high sources in an enabled dependency graph. In Blender 5.2, evaluating
a source while its collection is disabled can silently return its base mesh
without bevel modifiers. Copy the complete source and modifier stack into a
visible temporary bake scene, then compare evaluated triangle counts with the
approved high-source report before projection. Preserve the original sources.

Validate normal-only with albedo/AO/paint height disabled. Change light direction
and inspect close-up and gameplay size. A successful ray count is not visual
quality; more samples cannot recover subpixel bevels, incorrect source normals
or mismatched panel boundaries. If triangles, UVs, corner normals or high relief
change, rebake the affected normal/AO basis before using the textures.

For every asset, prove transfer on the representative assemblies after final-low
approval: compare actual high and baked-low crops with matched cameras, scale and
neutral material under at least two key directions. Label the source and receiver
views explicitly. Check close-up and intended presentation/gameplay sizes at the
planned delivered texture density, including reduced-resolution trials when the
working bake is larger. Record feature widths in texels and inspect seams, bevels,
recesses and attachments named in the detail plan. A high-resolution working bake
cannot certify a smaller delivery. Missing relief, subpixel features and flattened
openings must be resolved in the responsible source, receiver, UV allocation or
projection; roughness or normal-strength changes cannot substitute for this proof.
Include the comparison in the existing representative-material review and repeat
affected checks after whole-asset propagation.

Nonblack normal pixels can still conceal partially missed corner rays. Check
actual covered texel-center rays and edge samples where sharp low corners extend
past rounded high surfaces; an explicit temporary cage can correct those rays
without changing the approved low. Use the identical cage for physical and PBR
channels. Preserve sharp-edge flags as well as polygon boundaries and custom
corner normals when joining temporary evaluated sources.

When composing per-receiver images, derive ownership from that receiver's UV
coverage and verified dilation, not from nonblack normal pixels. A flat tangent
normal is valid data, and a reused bake buffer may contain unrelated islands.
Keep receiver ownership distinct from source-hit diagnostics, reject padding that
reaches another receiver, and retain the unmodified per-pair buffers for review.

Do not retain temporary evaluated datablock references in a persistent joined
source. Resolve evaluated material slots back to their original material IDs,
and construct diagnostic control meshes from copied geometry rather than keeping
an evaluated mesh copy after its owner is released. Mammoth Texture01 exposed a
Blender 5.2 dependency-graph crash through those temporary references; the corrected
join passed fresh physical and material bake controls without changing sources.

Review bake ownership in the assembled model. Construction Yard's near shoulder
ring and central bolt initially belonged to a cap receiver behind a separate
visible pin; valid UV coverage and one-to-one ownership did not prevent the pin
from hiding the detail. Compare matched high/low assembly close-ups, assign the
detail to the actually visible receiver, and rebake all coupled channels.

A hit on the expected object can still reach the wrong surface. Check the hit's
side and facing against the intended visible feature, especially near holes,
thin plates and collapsed low-poly chamfers. Mammoth's fan-deck rays could reach
the high plate's underside and create dark wedges despite broad coverage checks.
The visible rim also belonged to a separate genuine high part. Include such a
part in each receiver that actually represents it, record intentional shared
source membership, and use receiver UV ownership to validate the association.
Where one cage cannot serve adjoining faces, use declared face-specific cages;
retain original receiver topology for each normal pass and composite by recorded
polygon UV ownership. Replay the same ray policy for AO, identity and all material
channels. Verify the actual plain-low, normal-only and full-material views.

For a suspect boundary, trace visible receiver pixels back to their projected
high points and test those points against the assembled high from the matched
camera. A valid, front-facing hit can still describe a hidden interior wall.
Mammoth's barrel-seat ring captured 37 occluded high points among 38 visible
receiver samples. A temporary front-face cage redirected those samples to the
visible annular seat without changing approved geometry. Retain the before/after
source-visibility evidence and check the coupled rebake under both light directions.
Trace any remaining rendered artifact to its actual receiver and UV samples
before changing the same part again. The remaining Mammoth seat ring belonged to
the adjacent barrel's outer root, whose rays captured an axial end-cap normal;
it was not evidence that the corrected mantlet front still failed.

For faster material-only emission bakes, isolated target/source groups can be
joined into batches with shared projection settings. Keep explicit-cage and
implicit-cage targets separate, translate targets/sources/cages together, and
subtract each translation from shader world-position inputs. Retain polygon
boundaries, sharp flags and custom corner normals: procedural BOX projection
also depends on the source normals. First compare decoded covered pixels with
fresh sequential bakes using the same cages, and record bounded differences
rather than claiming bit-identical output. Construction Yard's three-target
comparison covered 382476 pixels, with maximum linear RGB difference .005562
and none above .01; the evidence is in its material-trial manifest.

Material-emission batching does not validate tangent-normal batching. A temporary
receiver with duplicated triangle corners can preserve UV positions and cage
rays while changing the tangent basis on complex hard-surface parts. In the
Texture01 controls, source-ID projection agreed while collar tangent normals
did not. Retain the approved receiver topology for physical tangent baking unless
the proposed alternative passes isolated normal comparisons, including curved
forms and hard corners. Do not use matching source IDs as a substitute for that
test or compensate for a basis mismatch with normal strength.

Corner-normal arrays alone do not prove that a baker uses the same shading basis
as the renderer. In Blender 5.2, a Mammoth private receiver with flat polygon flags
and custom corner normals produced a false tilt on a planar collar. A controlled
copy using smooth flags and explicitly restored original corner vectors removed
that tilt. Treat this as a version-specific, measured bake preparation workaround,
not an instruction to smooth the runtime mesh. Preserve its topology and UV
derivatives, measure any custom-normal re-encoding error, and inspect the result
on the unchanged runtime mesh under matched lights before accepting the bake.

## 7. Representative material trial and coherent PBR

Every asset must pass a representative mapped-material trial before whole-asset
propagation, including when it inherits a default or previously approved treatment.
Choose one section or a small set that covers its relevant material classes,
challenging surfaces, construction relief, exposed edges, material boundaries and
player regions. Include curved and flat surfaces, wear, deposits and fixed materials
where they exist; do not invent irrelevant classes to fill a checklist. A single
easy flat swatch cannot stand in for an untested assembly or finish. Compare
alternatives under identical geometry, physical maps, lights, exposure and camera.
Obtain approval of the actual mapped result at the existing representative-material
gate, then apply the treatment to the rest.

Diagnose each mismatch through **geometry -> bake transfer -> material -> lighting**.
Inspect the actual high and low with plain materials; inspect the physical normal
alone with albedo/AO/micro-height disabled; inspect decoded channel maps and their
live shader connections; then compare the full material under controlled lighting.
A shallow-looking cavity may be a lighting/renderer issue even when real geometry
exists. A connected map may still contain weak or inappropriate authored detail.
Record the responsible layer and evidence before changing it; do not use dark AO,
global gloss, stronger normals or exposure to conceal another layer's defect.

Inspect BaseColor, Roughness and Metallic separately on the actual model/UVs.
Measure decoded distributions by **authored material class and representative
component**, excluding padding/background. Report sample counts, useful percentiles
and the mask definitions; atlas-wide min/max alone cannot establish quality. Keep
class masks distinct from component UV masks: a component may mix coatings, metal,
rubber and hidden surfaces, and a metallic threshold is not semantic class proof.
Record original bit depth, numeric normalization, color-space treatment, UV pairing,
coverage weighting and exclusions. Do not reduce 16-bit scalar maps to 8-bit for
measurement. If boundary erosion is used, retain the un-eroded comparison and
sample-retention fraction, since narrow wear islands can disappear. UV-area results
are not visible-screen-area or world-surface-area percentages. Verify that packed
images match the measured external files and that those maps drive the live shader;
connected input defaults are not the active material values. Statistics diagnose
distribution and transfer; they do not replace visual acceptance.

Account for filtering when checking a shared material-selection equation after
baking. The filtered product of coverage weights need not equal the product of
their filtered averages at a transition. Keep the original strict comparison,
locate every outlier by source and component, and inspect local mask variation
before attributing it to an incorrect material stack. Do not silently widen a
global tolerance or discard failures. Mammoth Texture01 retained its strict
one-texel cassette finding and a separate local filtering diagnosis; that finding
does not justify accepting unrelated interior errors or unknown source identity.

Metallic describes **material identity and coverage**; roughness describes the
surface's scattering response. Opaque paint, primer, rubber, rust and dust are
dielectric; clean exposed metal is metallic. Intermediate metallic values must
describe intentional coverage/filtering, not serve as a general shininess control.
Neither pale chips nor dark pigment determine metalness. Use authored material
selections, and verify that wear and deposits change all affected channels coherently.

Author roughness independently of albedo luminance and at deliberate spatial scales.
Give broad coating variation, local contact polish/handling or grease, deposits,
abrasion direction and fine manufacturing texture appropriate placement and scale
where relevant. Preserve quiet areas and distinct responses between material classes;
uniform noise or one global gloss value is not a substitute. Correlate actual
coating/exposure boundaries across channels without forcing pigment and roughness
to share identical noise. A broad or narrow numeric range is not inherently good:
judge its spatial pattern and moving highlights on the model.

Treat numeric roughness ranges as trial parameters, not values recoverable from
a reference render or universal settings copied from Mammoth. Use fixed exposure,
camera and a documented neutral/reflection-readable lighting setup, then cross-check
the same candidate under a second environment without retuning exposure to hide a
problem. Review actual albedo-only, roughness-only, metallic-only and full-material
views, alongside the structural normal-only check. Highlights must move with light;
pale edges must read as selective material wear rather than permanent outlines.

Keep micro-surface relief separate from structural normals. Author its material
source, scale, physical or nominal bump amplitude and composition order explicitly;
verify it alone and combined so it does not double existing bevels or emboss paint
color. Add it only where the material calls for it and the delivered resolution can
represent it. A constant neutral height map contributes no micro-relief, and a
separate shader bump is not evidence that it exists in the delivered normal image.

### GPT Image contributes paint, not arbitrary normal maps

Use actual orthographic surface guides and approved gritty views as input for
panel-specific **unlit BaseColor pigment**. Ask for brushwork, darker color masses,
local variation and restrained grit with exact silhouette/detail invariants.
No shadows, AO, directional highlights, invented panel positions or new fasteners.
Retain exact prompts and immutable generated sources. For broad continuation,
generate hardware-free pigment consistent with the approved representative panel.

Project/clean the selected pigment onto measured surfaces. Replace any generated
bolt/plate/bevel-lighting pixels with aligned authored layers. A generated promise
of seamless tiling is not evidence: verify repetition or keep all sampling inside
one source sheet. Inspect face boundaries and scale consistency on the actual mesh.

Exact accepted source prompts:
[dark panel v02](../art/mammoth_lowpoly/textures/lookdev04/source/ArmorPanel_Paint_AI_v02_DarkGritty.md)
and [whole-asset pigment](../art/mammoth_lowpoly/lookdev05_image_prompt.md).
Darkening is a pigment decision under fixed lights, not an exposure/AO trick.
Do not apply an old pale-source darkening gain again to an already dark source.

### One authored material selection stack

Maintain explicit named selections for paint, player paint, primer, exposed steel,
hardware, seams/vents, rubber, dust and grime. Reuse the **same boundaries** in
BaseColor, Roughness, Metallic and player exclusions. Accepted structural relief
comes from matched high-poly or a controlled structural height source. Mammoth's
optional SurfaceHeight represents only intentionally physical shallow paint loss.
Other authored micro-relief needs its own named source and declared composition;
camouflage/grime brightness is not embossing.

Derive exposed-edge wear from the actual chamfers/bevels and geometry-aware
selections, then break it up with restrained localized abrasion. Bake the aligned
wear into BaseColor and its corresponding roughness, metallic, paint-loss and
player-mask exclusions. Preserve the editable selection, but do not substitute a
procedural mask alone or an AI-painted highlight for the delivered edge treatment.

Make wear selective and layered: intact coating, primer where present, exposed
substrate and deposited dirt must have intentional boundaries and material response.
Vary placement with exposed corners, contact, handling and recess accumulation,
while leaving protected and quiet surfaces intact. Do not trace every edge with
the same bright band or assume every pale edge is bare metal. Check wear in
albedo-only, material-class, roughness and metallic views; keep its height, if any,
shallow and separately authored. Carry the same exclusions into player masks.

| Class | Metalness | Roughness approach | Player mask |
| --- | --- | --- | --- |
| Intact/player paint | 0 | Broad matte/satin response | Authored paint region only |
| Primer | 0 | Rougher than rubbed steel | Excluded |
| Bare steel/contact wear | 1 before dirt coverage | Generally smoother than paint; varied by abrasion | Excluded |
| Bare fasteners/hardware | Metallic where appropriate | Controlled highlight | Excluded |
| Rubber, dirt, dust | 0 | Rough | Excluded |
| Fixed accents/glass/lenses | Deliberate material class | Role-specific | Excluded unless explicitly intended |

Use separate master images: BaseColor (sRGB), Normal, AO, Roughness, Metallic,
PlayerColorMask and optional SurfaceHeight (all other maps Non-Color). Keep
lighting/AO out of clean BaseColor. The approved Mammoth display multiplies
`0.72 + 0.28 * AO` once in the shader after player tint. A legacy AO-colored
derivative must be separately named and must not receive AO a second time.

Test a known-value PNG round trip before exporting bake channels. Blender 5.2
can save a Non-Color float buffer without applying the requested Standard view
transform. For linear BaseColor buffers, explicitly encode linear to sRGB once
when required and verify the decoded values after inverse-sRGB conversion.
Scalar maps and tangent normals remain raw data. A color-space label alone does
not prove the stored pixels have the intended encoding.

### Mandatory player-colour pairing

Every albedo has a separate grayscale linear/Non-Color mask at identical dimensions
and UVs. Black 0 preserves albedo, white 1 enables tint, fractional values blend.
Author neutral tonal paint under intended player regions. Preview in linear space:

```text
tinted = mix(BaseColor, BaseColor * PlayerTint, PlayerColorMask)
display_base = tinted * (0.72 + 0.28 * AO)  # approved Mammoth policy, once
```

Do not infer masks from diffuse hue, hide them in alpha, or recolor dirt/metal/rubber.
Intentional fixed-color albedos still get a documented all-black paired mask. Pack
and deliver the images; list pairing, dimensions, UV, color space and tint formula
in the manifest. Render at least two distinct player colours and inspect boundaries
and fixed accents; inspect filtered/mip behavior at the chosen runtime resolution
separately. Mammoth's twelve red tips always remain red.

## 8. Bake paint, decode delivery images, inspect and save

1. Bake the approved color/PBR/player/optional-height channels to the final UVs.
   Retain editable semantic graphs/layout data and separate source images.
2. Save PNGs, load each as a **fresh decoded image**, set sRGB or Non-Color,
   reload after assigning that space, pack, and connect the decoded image.
   Do not preview an obsolete float authoring buffer instead of the actual file.
3. Check dimensions, grayscale scalar RGB, [0,1] ranges, normal convention,
   mask exclusions and all-black exceptions. Compare packed payload hashes with
   the standalone PNGs. Record source/output hashes and map pairings.
4. Render actual front/rear/side/top/hero views, normal-only key/reversed-key
   views, complete-material reversed-light views, two player colours and a
   gameplay-size proxy. Inspect end supports with armor on. Test belt controls
   independently; phase-offset stills are not proof of animation or signed travel.
5. Hold exposure constant. Use scene lights/world in Eevee Rendered mode, not
   unrelated Material Preview studio lighting. Separate camera changes from
   asset/export orientation. Verify metal reflections actually change with light.
6. Save a new normal `.blend` with low visible, high sources hidden/non-export,
   useful camera/lights and packed maps. Allow disk room for a full temporary save
   plus backup. A library checkpoint alone may reopen into an empty default scene.
7. Reopen in a separate call and activate/update the owning scene before reading
   parented world matrices. Verify counts/bounds/pivot, frozen mesh/UV/corner-normal
   state, image hashes, active scene and engine. **Verification must not rewrite
   the file it is verifying.** Keep the previous source unchanged/recoverable.
8. Obtain full-asset user approval and record its date, exact scope, source hash
   and preview/manifest identity. Preserve pre-approval QA as historical evidence;
   append an approval record rather than retroactively changing its findings.

## 9. Mammoth benchmarks — examples, not global constants

**Shared GDI finish amendment, 2026-09-24:** the user requires the updated Medium
Tank to use subtle broad desert camouflage inspired by its ModDB reference and
much larger physical chamfers with layered edge wear inspired by Gears 5 baZGeo.
This later amendment overrides the previous Medium concept's thin edge treatment
and uniform tan paint: retain the new Mammoth's material/palette family, add
low-contrast sand/ochre/olive-brown camouflage, and carry broad readable chamfers
into actual high and final-low geometry. Use the saved ModDB and baZGeo images
as explicit pattern and edge-craft inputs respectively; preserve Medium identity,
footprint, barrel proportions and part counts. Generated approval does not certify
the later actual chamfer geometry or material transfer.

The updated Medium Tank must match the new finished Mammoth's material family. Use these
actual RuntimeRig01 screenshots, not the old Lookdev05 or generated Mammoth art:

- [Whole tank](../art/mammoth_lowpoly/export_v01/previews/RuntimeRig01_assembly.png)
- [Rear material detail](../art/mammoth_lowpoly/export_v01/previews/RuntimeRig01_rear.png)
- [Gun and armor detail](../art/mammoth_lowpoly/export_v01/previews/RuntimeRig01_gun.png)

Carry their warm muted tan paint, broad restrained panels, charcoal gunmetal,
broken exposed-metal edges, dark recesses and concentrated lower grime into both
GPT Image stages and actual material work. Match wear/texture scale relative to
the game-sized vehicle, not image resolution. Preserve each vehicle's geometry.
Compare the actual mapped Medium Tank and Mammoth under matched lighting and at
gameplay scale during material review. Generated similarity alone is insufficient.
Save exact screenshot inputs and hashes with each generation record. A different
future user-selected finish can override this shared-style default explicitly.

**Camouflage paint rule, verified on Mammoth Camo01:** author pattern masks as
paint shapes with hard, irregular borders and only a narrow antialias band. Apply
the pattern from one shared object/world-space projection and bake that result into
the UV atlas so paint crosses chamfers and UV seams continuously. Atlas-space noise
or per-island pattern generation is not acceptable for continuous painted armor.
Apply each colour as a pigment tint/multiply over the approved BaseColor instead of
blending toward a flat replacement colour; the baked grain, scratches, stains and
panel variation must remain visible within every patch. Layer grime/oil/soot above
the camouflage and exclude player-colour regions. Validate both a close material
view and the real engine/gameplay camera. If the close view reads as blurred
smudges, sharpen the mask; if gameplay reads as one colour, adjust patch scale or
tone separation without flattening the underlying texture.

### Reusable tank material and texture method

Use this stack for every tank unless its approved direction explicitly replaces a
layer:

1. Finish UVs, rest transforms and the approved runtime assembly before projecting
   camouflage. Use one stable projection anchor shared by hull, turret, skirts,
   hatches and other painted armor. Pattern scale is measured against the vehicle,
   not the atlas or individual UV islands.
2. Bake a separate object-space camouflage classification mask into the final UV
   atlas. Give paint borders a narrow raster antialias only. Inspect every visible
   chamfer, welded transition and adjacent armor face for continuity. UV padding
   may extend a colour outside an island, but it must not choose that colour.
3. Keep the approved pre-camouflage BaseColor recoverable. Apply camouflage with
   per-tone linear pigment multipliers/tints so all existing grain, scratches,
   stains, edge wear and panel variation survive inside each patch. Do not mix
   toward flat constant colours or use blurred opacity fields as the pattern.
4. Use this layer order: substrate and baked panel variation; base paint;
   camouflage pigment; chips/exposed edges; dust, grime, oil and soot. Weathering
   crosses camouflage borders naturally because it is accumulated after painting.
5. Restrict camouflage with authored semantic masks. Exclude bare metal, barrels,
   optics, lamps, rubber, tracks, rocket faces and other non-painted materials.
   Keep the player-colour mask separate and prevent camouflage from contaminating
   recolour regions unless the asset brief explicitly makes them the same paint.
6. Preserve channel meaning. Camouflage changes BaseColor and may add only a small,
   authored roughness response for different paint batches. It does not change
   Metallic, AO or Normal by itself. Chips become metallic only where the material
   mask identifies exposed metal; colour brightness never decides metalness.
7. Retain a full-resolution working atlas and export the measured runtime sizes.
   For the Mammoth benchmark this is a 2048 atlas working master, a 1024 C3M atlas,
   and 256 auxiliary rocket/tread maps; those dimensions are evidence, not automatic
   constants for every vehicle. Hash the pre-camouflage source, classification mask,
   final maps and runtime export.
8. Validate under held neutral lighting with a whole vehicle view, a close material
   view centered on a multi-face armor transition, and the actual engine/gameplay
   camera. Reject visible UV cuts, flattened underlying detail, airbrushed smudges,
   unreadable gameplay patches, recolour bleed and camouflage on excluded parts.

The replayable Mammoth implementation is
[`bake_object_space_camo_v02.py`](../art/mammoth_lowpoly/texture_v01/bake_object_space_camo_v02.py)
followed by
[`apply_subtle_camo_v01.py`](../art/mammoth_lowpoly/texture_v01/apply_subtle_camo_v01.py).
Reuse the method and parameterize it for the new tank's bounds, masks, palette and
runtime texture budget; do not copy Mammoth UV coordinates or fixed pattern scale.

The final Mammoth-quality reference is the clean
[`Mammoth_TD_RuntimeRig01.blend`](../art/mammoth_lowpoly/export_v01/Mammoth_TD_RuntimeRig01.blend),
its [runtime manifest](../art/mammoth_lowpoly/export_v01/Mammoth_TD_RuntimeRig01.manifest.json),
[integration handoff](../art/mammoth_lowpoly/export_v01/INTEGRATION_INSTRUCTIONS.md),
approved tuned values in section 13 and the HP10/Texture01 evidence recorded in the
pipeline. It has 218 runtime meshes, one 30-bone rigid armature and 9,990 triangles
within the current vehicle ceiling; a new vehicle must achieve comparable visual
craft within the current 10,000-triangle ceiling unless the user grants a recorded
asset-specific override. Its 2K atlas is a working master and its measured desktop
C3M target is 1K with 256 tread/rocket maps.

Lookdev05 below remains the first completely approved, hash-bound reusable material
and bake benchmark. Use its numeric values to understand the proven technique, then
apply the later high-poly, chamfer, tuned-material and C3M lessons elsewhere in this
recipe. Do not mistake this earlier 2,888-triangle checkpoint for the final Mammoth
geometry or current runtime delivery.

The accepted file is
[Mammoth_TD_OpenCNC3D_Lookdev05.blend](../art/mammoth_lowpoly/Mammoth_TD_OpenCNC3D_Lookdev05.blend).
[Approval](../art/mammoth_lowpoly/LOOKDEV05_APPROVAL.md),
[manifest](../art/mammoth_lowpoly/textures/lookdev05/manifest.json),
[README](../art/mammoth_lowpoly/LOOKDEV05_README.md) and
[actual gold preview](../art/mammoth_lowpoly/textures/lookdev05/qa/Mammoth_Lookdev05_player_gold.png)
are the reference bundle. This approval does not assert parity with the ArtStation
asset or adopt its much larger geometry budget.

| Parameter | Verified Lookdev05 value |
| --- | --- |
| Low / ceiling | 2888 / 3000 triangles, 56 meshes |
| High source | 178 meshes; 64 radial gun segments, 48 collar/launcher segments |
| Track shells | 140 triangles each; 24 belt segments / 48 belt triangles |
| Per track cap source | Three roads, sprocket, idler, two return rollers; front/rear fitted separately |
| Blender convention | One unit = one map cell; +Z up; Mammoth forward -Y |
| Overall bounds | X [-.345703125,.3505859375], Y [-.6052048206,.5205078125], Z [0,.4394695461] |
| Turret pivot | (.003441035049,.034287344664,.217492580414) |
| Static atlas | 4096 BakeUV, .0045 fraction pack margin, 8px EXTEND bake padding; about 54.9% sampled coverage |
| Belts | 512 TreadUV, seven arc-length repeats, distinct L/R materials, `(U,V-phase)` |
| Physical bake | Cycles CPU 16 samples, tangent +X/+Y/+Z; normal strength 1 |
| Projection | Track extrusion .020 / distance .045 cell; other parts .012 / .030 |
| AO source | Component-local; AO node distance .035 cell, 16 samples |
| Paint bake | EMIT, four samples; BaseColorB/Roughness/Metallic/PlayerColorMask/SurfaceHeight |
| Full pigment | World/rest scale .85, offset (.5,.5,.15), gain 1; measured nonwrapping projection, not verified seamless |
| Paint / primer roughness | .50–.65 / .68 |
| Exposed steel / hardware roughness | .22–.35 / .31 before dust/grime |
| Dust / grime roughness | .87 / .82 on armor; both reduce exposed-metal contribution |
| Paint height | `.5-.006*primer-.012*steel`; observed [.487999,.500008] |
| Height composition | Bump strength 1, distance .0005 cell, physical normal fed into bump normal; separate map, not baked into delivered normal PNG |
| Authoring textures | Seven 4K atlas + six 512 belt images; explicit player mask for each set |
| QA | 1600×1200, Cycles 40 samples + denoise; AgX, exposure 0, gamma 1 |
| Example lights | Key/fill/rim 85/25/75 W with sizes 1.0/1.1/.75 cell in `lookdev05_qa.py`; world strength .22; poses in QA report |
| Small view | 320×240 Blender gameplay proxy, not engine capture |

These values are a known successful starting point **at this scale**, not physical
laws or mandatory dimensions for other assets. Calculate new ray distances, detail
texel widths, UV gutters, belt pitch and light sizes for each brief. Normal maps
cannot preserve subpixel features simply by increasing their strength.

### Verified full-atlas delivery rules — Mammoth Texture01

These rules apply to future assets as well as the Mammoth:

- Bake the shipped paint channels at the intended delivery resolution when a
  larger atlas would only be discarded. Keep higher-resolution physical or
  representative studies as authoring evidence; do not call them the runtime
  texture budget.
- When disjoint bake batches share an atlas, compose covered pixels first and
  regenerate one gutter around the complete atlas. Never merge each batch's
  independently expanded padding: two valid margins can overwrite another
  batch when the delivered UV gap is smaller than their combined width.
- Downsample tangent normals as vectors and renormalize them. Numeric scalar
  channels use linear averaging; categorical source/material IDs use an exact
  categorical sample and remain audit-only.
- A high source that covers only detail on a larger low receiver needs an
  explicit material-matched backing for the source-absent receiver faces. A
  flat default normal can conceal a miss, so require covered SourceID and
  receiver-ownership checks in addition to a normal-map black-pixel test.
- Preserve and inspect disagreements caused by overlapping tiny sources. Count
  them, identify their receiver/source pairs, and judge them in actual material
  and gameplay-scale views; do not convert a bounded filtering/source-order
  discrepancy into a silent pass.

## 10. Reusing the implementation without overwriting the approved asset

Use [lookdev05_geometry.py](../art/mammoth_lowpoly/lookdev05_geometry.py) for the
geometry/layout examples, [lookdev05_materials.py](../art/mammoth_lowpoly/lookdev05_materials.py)
for semantic material layers, [lookdev05_build.py](../art/mammoth_lowpoly/lookdev05_build.py)
for matched bakes, and [lookdev05_qa.py](../art/mammoth_lowpoly/lookdev05_qa.py) /
[lookdev05_uv_audit.py](../art/mammoth_lowpoly/lookdev05_uv_audit.py) for checks.
Inspect their imports and parameterize asset-specific names, paths, source layouts,
counts, role/mask mappings and limits before adapting them. Do not execute them
unchanged on a new model or automatically rerun them over approved files.

Historical Mammoth replay sequence (reference only, not a clean-room replay claim):

```text
preserved Lookdev04 source, original Scene evaluated
 -> run_lookdev05_prepare.py -> run_lookdev05_physical.py -> run_lookdev05_paint.py
 -> run_lookdev05_qa.py -> inspect initial render -> run_lookdev05_qa_suite.py
 -> run_lookdev05_material_checks.py -> inspect -> finish_lookdev05.py
 -> normal authoring/UI save -> separate-call reopen -> verify_lookdev05_reopen.py
 -> manifest generation -> separate user approval record
```

Important dependencies and guards:

- A new asset must follow sections 1–8, including **high-poly before final low-poly**.
  The historical Mammoth scripts evolved from a pre-existing asset in another
  order; that is recovery history, not the future production order.
- Full Mammoth replay starts from preserved Lookdev04, **not** compact final
  Lookdev05, which omits the original `Scene` and legacy collections.
- Geometry/material helpers depend on original chamfer meshes, prior running-gear
  sources and `rebuild04_geometry`, `lookdev04_track_layout`, `lookdev04_high_detail`,
  `pass03_running_gear`, `lookdev04_materials`, `pbr_pass03_materials`,
  `texture_pass_materials`, `pbr_pass02_materials`, `lookdev04_benchmark`,
  `rebuild03_textures` and `audit_texture_uv`. Preserve the source tree.
- The six belt maps are reused from `textures/rebuild03/`; the whole-tank paint
  pass does not regenerate that periodic source. Revalidate before changing it.
- Prepare refuses existing generated scene identities. Assign fresh revision
  names/outputs; do not remove that guard to force an overwrite.
- QA suite, material checks and finish need the live state made by QA setup.
  Reopening clears that state. Explicitly unhide the intended high-source
  collection before any intentional rebake; final delivery hides it.
- `finish` writes/opens a compact library checkpoint; perform a normal source/UI
  save before fresh verification. The historical D: disk workaround is not a
  requirement to use D: on future machines.
- `record_lookdev05_review.py` is a historical event writer with old review and
  disk-recovery statements, **not a generic verifier**; do not rerun it as a new
  approval or replay it over this later approval record.
- The manifest generator's pre-approval status describes its original handoff.
  Read the later hash-bound approval record for the accepted result. Do not turn
  numeric validation into automatic artistic approval.

## 11. Handoff and separately gated runtime work

Deliver the initial low, approved design sheet, approved high source, selected
post-high generated finish sheet with its provenance/deviation review, final low/source
file, physical bakes, clean/PBR/player maps, generation prompts/provenance,
editable recipes, manifest, fit/UV/map/reopen reports, actual preview images and
dated approval record. Preserve earlier failures as labeled history, not current
defaults. Maintain repository-relative links and note anything not shared/versioned.

Only start export/runtime work when it is in scope. Read
[art-return-contract.md](art-return-contract.md) and inspect the actual importer
and renderer then; a Blender PBR material does not establish current engine
support. Build a separate export copy, preserve the tangent basis, validate rigid
roles/pivots/axes/scale and the complete material adapter, then test in an isolated
pack before changing canonical assets. Validate several hull/turret headings,
muzzle effects, terrain contact, player colours, signed independent belt travel,
filters/mips and the actual moving War Factory exit. Authoring/art approval,
export validation and runtime acceptance are three different states.

## 12. Brief to fill at the start of every new asset

```text
Asset / type code / revision:
User brief and scope (authoring, export, integration):
Online research URLs, attribution and interpretation notes:
OpenCNC3D original type/parts, source pack/ROM hash, extraction tool and fit-report path:
Design references / ArtStation finish reference / palette:
Reference feature inventory, original/proposed omissions and retain/reinterpret/omit decisions:
Measured footprint, overhang, height constraints, axes/units, ground and pivots:
Original-to-initial/final/export footprint deltas and numeric tolerance:
Triangle ceiling / authoring texture sizes / separately approved runtime budget:
Desktop exporter/runtime version, target format and current material/part capabilities:
First-low approval version/date/quote:
Initial low-poly file and actual render/camera inputs:
GPT Image prompt, source roles, output hash, selected views and approval:
Per-assembly primary/secondary/tertiary detail plan, rich/quiet areas and shared layout:
Reference-to-high clay/grazing-light evidence and intentional adaptations:
High-poly approval version/date/quote:
Post-high GPT Image actual source views/cameras and approved-high file hash:
Selected finish-reference URLs/roles and asset-specific overrides:
Post-high finish prompt, input/output hashes, geometry deviations and direction approval:
Final low-poly file, count, hierarchy and exact tangent-basis identity:
Feature allocation to retained geometry, named bake source/receiver or material only:
Final-mesh approval version/date/quote:
BakeUV / auxiliary UVs, padding, density, matched sources and projection settings:
High-to-baked-low comparisons, feature texel widths and delivered-resolution checks:
PBR classes, physical normal/AO scope, optional height amplitude, AO policy:
Spatial roughness/wear plan, micro-relief sources and composition:
Decoded class/component statistics, masks, bit depth, weighting/exclusions and file hashes:
Albedo/player-mask pairings, tint rule, fixed colors and black-mask exceptions:
Player-mask coverage and two gameplay-size faction previews:
Representative trial coverage, diagnostic channel views, fixed/second lighting evidence:
Representative-section approval and any later artistic amendments:
Full-asset QA views, technical reports, saved-file/hash verification:
Full-asset artistic approval quote/date/hash:
Clean delivery scene/datablock counts and authoring-archive path:
Working-to-runtime texture resample, C3M slot packing and byte-size comparison:
Active, renderer-reachable and future-facing articulated controls:
Unverified export/runtime stages and next authorized step:
```

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
High-poly v01 now has a separate actual mesh; its own approval gate remains.
See [deployment research](../art/construction_yard/references/deployment/RESEARCH.md)
and [generated review](../art/construction_yard/generated/REVIEW.md).

### High-poly detail and attachment checks — Construction Yard (2026-09-22)

Preserve an exact footprint/contact foundation when adding high-poly detail;
bevel other parts inward instead of rounding away required source corners.
Rebuild true cylinders parametrically. An eight-sided box and a low cylinder
can share the same vertex/face counts, so topology counts alone cannot identify
which components should become round. Keep named part roles authoritative.

Evaluate small bevels: a width reaching half a thin cover's thickness produced
collapsed faces despite overlap clamping. Limiting that cover's bevel to 40%
of its thinnest dimension removed the degeneracy. Closed per-object topology
still does not prove attachment: actual interior cutaways exposed a threshold
gap and disconnected inner ribs. Seat the deck/landing on supports and attach
ribs to real liner surfaces before approval. Back deck-panel seams with real
support and join coplanar tops at their edges; overlapping landing/sill tops
produced a black render artifact despite clean per-object topology.
The corrected high source preserves
exact contact corners and passes evaluated topology plus 641 static clearance
pairs; see the [high-poly review](../art/construction_yard/HIGH_POLY_REVIEW.md).

### Readable detail density — Construction Yard v02 (2026-09-22)

The user found high-poly v01 insufficiently detailed despite its 234524 evaluated
source triangles. Polygon count is not a measure of visible construction detail:
much of that count served curved hardware and interior perforations. v02 uses
reference-led secondary forms that read at review distance: layered reinforcement,
framed bolted covers, segmented housings, real louvers and embossed door panels.
Keep broad roof sheets readable and the briefly exposed deployment interior sparse.

New detail must repeat the footprint and mechanical checks. An inspection-port
rim exceeded the source polygon despite unchanged axis-aligned bounds. Reduce and
seat it against the curved jacket; do not relax the footprint tolerance. A new
elbow carrier touched an existing piston pin, requiring a shorter carrier. Tiny
coplanar retaining rings produced jagged dark arcs in a real render even with
closed, positive-volume meshes; separate exposed faces and seat screw washers.
The corrected v02 passes zero envelope/contact deltas, solid-topology checks and
26449 static clearance pairs, including all 641 prior pairs. It still requires
[high-poly approval](../art/construction_yard/HIGH_POLY_V02_REVIEW.md) before final low.

### Final-low derivation and review — Construction Yard (2026-09-22)

High-poly v02 received explicit approval: “Great. Approved”. The separate final
low v01 contains 7544 evaluated triangles, including the briefly exposed interior.
Keep large covers and openings physical; transfer shallow bolts, shutter tiles,
vents and backed grating relief from named high sources after final-mesh approval.
An untextured low preview will lack that surface relief: explain the allocation
instead of silently starting bakes to make the approval view look finished.

After creating positioned assembly parents, update dependency-graph transforms
before preserving child world matrices. A stale new parent matrix doubled pivot
offsets in a diagnostic build; fresh-reopen footprint checks caught it. The
corrected source retains exact contact/envelope bounds and 1595 static separation
pairs, including all 271 inherited first-low pairs. Keep original source pivot
guides separate from authored pivots and do not imply deployment compatibility.

Define final-low corner normals deliberately: straight lathed walls need radial
normals independent of adjacent short shoulder slopes; pitched fan tops should
not average with their thin edge faces. Freeze evaluated chamfers, triangulation
and these normals together before subsequent unwrapping. Review render overrides
must restore both material slots and polygon material indices, because clearing
slots can reset the latter. See [final-low review](../art/construction_yard/FINAL_LOW_V01_REVIEW.md).

## 13. Adjustable lookdev, approved-value capture and rigid runtime delivery

The Mammoth Gears-finish revision verified a reusable material workflow after the
ordinary texture bake. Treat a finish reference as evidence for spatial behavior:
broad paint fade, directional runoff, seam oxidation, polished contact edges,
oily recesses and roughness breakup. Do not turn those observations into uniform
noise or force every listed deposit onto the asset. The user ultimately set the
Mammoth rust amount to zero while increasing dust, grime, oil and soot; reference
analysis proposes layers, while the approved live values decide the asset.

Bake durable, noninteractive color decisions into a new versioned Base Color:
palette-preserving grade, broad fade and restrained large-scale stain history.
Keep material response and local deposits adjustable through authored semantic
masks. At minimum expose paint value, exposed-edge response, rust, grime, dust,
oil/grease, soot, roughness strength and offset, metallic strength, baked-normal
strength and micro-normal strength. Deposit sliders must affect all physically
related channels together: dry deposits generally raise roughness, oil lowers it,
and rust/grime/dust/oil/soot suppress metallic response where they coat a metal.
Never infer these masks from final hue. Preserve the clean prior checkpoint and
store controls as named scene/material properties that an artist can read back.

Open the control panel in the actual Blender session and verify that a scripted
property change updates the driven shader before handing it to the user. When the
user finishes, capture the live values immediately to JSON and a new `.blend`.
Freeze the approved deposits into delivery Base Color/Roughness/Metallic maps, but
do **not** bake the player tint into Base Color. Deliver the existing linear
player-colour mask beside the albedo and let the preview shader apply a reversible
default tint. Preserve physical normal/AO masters; record normal and height
strengths when the target shader must reproduce them separately.

### Approved Mammoth Gears-look preset — 2026-09-24

The user accepted the adjustable result (“Fantastic! This looks great!”), tuned
the live controls, then explicitly requested that the values be saved for future
assets. Use this as the default **starting preset for gritty military vehicles
using the same finish direction**, followed by an actual-model material review;
it is not a substitute for asset-specific masks or approval.

| Control | Approved value |
| --- | ---: |
| Paint Value | 0.960000 |
| Exposed Edge Highlight | 0.220000 |
| Rust | 0.000000 |
| Grime | 0.720909 |
| Dust | 0.677273 |
| Oil / Grease | 0.445455 |
| Soot | 0.830000 |
| Roughness Strength | 0.893636 |
| Roughness Offset | 0.000000 |
| Metallic Strength | 0.459091 |
| Baked Normal Strength | 1.212121 |
| Paint Micro Normal | 1.066667 |

The exact machine-readable record is
[`Mammoth_GearsLookdev02_UserValues.json`](../art/mammoth_lowpoly/texture_v01/Mammoth_GearsLookdev02_UserValues.json).
The tuned checkpoint is `Mammoth_TD_OpenCNC3D_Texture01_UserTuned03.blend`.

### Plan rigid animation and destruction before final UVs

For future vehicles, resolve the runtime articulation list during the initial
brief, before the final mesh and bake: turret yaw, recoil groups, launcher pitch,
individually fired projectiles, scrolling belts and destruction chunks. Reserve
triangle and texture budget for hidden projectile bodies. The Mammoth reached
9,870 triangles before this requirement and could add only twelve five-sided,
ten-triangle rocket bodies while staying inside its recorded historical 10,000
vehicle override; the delivery is 9,990 triangles. Do not rely on such a narrow
margin in future assets.

Use a rigid skeleton when the target supports it: one bone with 100% weights per
mesh, no deformation, explicit local motion axes/ranges, and bind poses at the
real mechanical pivots. Keep functional parts and destruction groups compatible:
turret children remain under turret yaw; recoil barrels under turret; launcher
and individual rocket bones under turret; hull pods, engine deck and tread units
remain detachable hull chunks. Keep each projectile a complete independent
assembly rather than a loose visible nose. Preserve a separate mesh/chunk manifest
even when many authoring objects share one destruction bone.

Scrolling belts require their own repeating textures and `TreadUV`; use separate
left/right materials even when their initial pixels are identical. Drive phase
from signed distance travelled, independently per side, and sample the Mammoth
unwrap as `V - phase`. A stopped vehicle must stop, reverse travel must reverse,
and turning in place must produce opposite/unequal side phases. Static sidewalls,
wheels and armor remain on the non-scrolling atlas.

Re-read [art-return-contract.md](art-return-contract.md) and the current desktop
exporter/runtime before export. Deliver a clean `.blend` as the authoritative C3M
source, plus a rigged FBX and clearly named two-part compatibility FBX as independent
cross-checks. Do not flatten away the authoritative rig or claim controls that the
current C3M part record and game state cannot drive. Recoil, destruction groups and
signed left/right belt motion are reachable through renderer/C3M work; launcher
pitch and per-tube depletion are future-facing because unchanged Tiberian Dawn logic
exposes neither elevation nor a Mammoth magazine/tube index.

Set delivery image paths relative to the delivery `.blend`'s final directory. A
`//textures/...` path made relative to the authoring checkpoint can silently point
back to the source directory and render magenta after reopening. Fresh-open the
clean delivery blend, confirm that no dormant authoring datablocks remain, export
C3M, import each FBX into an empty scene, and verify exact triangles, bones, names,
materials, UV sets and sample motion deltas. Parse the compatibility FBX with the
repository reader and compare its hull/turret counts with the bone-derived C3M split.
Export validation remains separate from in-game acceptance.

## 14. OpenCNC3D desktop C3M delivery contract

The Mammoth's first live integration corrected the assumed target. Modern desktop
assets enter the game through `tools/art/blender_c3m.py` from a Blender scene and
are read by `game/model3d_mod.h`. Mission-pack replacement through `fbxin.py` is
the Classic Visuals/legacy path, not the PBR desktop delivery. Inspect the current
implementation again for every asset because format capabilities may advance.

### Deliver a clean Blender scene

The desktop exporter reads Blender objects. Supply a dedicated delivery `.blend`
containing exactly the runtime scene: runtime mesh objects, required armature,
runtime materials and their canonical image datablocks. Move high-poly sources,
lookdev cameras, review copies, draft bakes and other history into a separately
named authoring archive. Scene membership alone is not enough when dormant data
contains hundreds of thousands of triangles; an exporter that scans datablocks can
silently include it.

Record and validate total Blender datablocks as well as scene objects. Remove
decorated duplicate images such as `Map.001`, point every texture node at the one
canonical image, purge unused materials/images/meshes and reopen the file before
delivery. The Mammoth correction reduced its runtime `.blend` from 2,657 meshes and
974,062 stored triangles to 218 meshes and 9,990 triangles without changing the
rendered asset.

Keep the rigged FBX as a hierarchy/weights cross-check and a two-object compatibility
FBX as a second statement of the hull/turret split. Compare counts rather than merely
opening both files. For Mammoth, both paths resolve to 4,890 hull and 5,100 turret
triangles; that agreement proved that bone ancestry was interpreted correctly.

### Author six maps; expect three C3M slots

Use the suffixes `BaseColor`, `Normal`, `AO`, `Roughness`, `Metallic` and
`PlayerColorMask` under a stable material stem. The exporter packs them as:

| C3M slot | Channels |
| --- | --- |
| BASE | sRGB BaseColor RGB, linear PlayerColorMask A |
| NORM | tangent-space OpenGL +Y normal RGB |
| ORM | AO R, Roughness G, Metallic B |

There is no current fourth slot. `SurfaceHeight` and emissive do not reach the
runtime. Keep height masters when they are useful for authoring or deriving a normal,
but do not call them runtime maps or spend final delivery work on emissive/parallax
without an approved format extension. C3M stores RGBA8, so preserve high-bit-depth
working masters separately and deliver 8-bit inputs unless a later exporter proves
otherwise.

### Use a measured runtime resolution, not the working-bake size

C3M currently stores three raw RGBA8 images per material, costing exactly
`3 × width × height × 4` bytes. Working at 2K remains valid, but the Mammoth
maximum-zoom comparison established a 1,024 main atlas and 256 tread/rocket maps as
the current runtime target: 15.6 MB versus 58.1 MB at 2K, with only 0.25% of the
1.6 million comparison pixels differing. Record both working and runtime sizes,
resample with a declared filter and compare them at maximum intended game zoom.
Revisit the budget when the camera, compression or C3M format changes.

### Design player colour for recognition at gameplay distance

A technically valid mask can still fail its purpose. Mammoth's integrated mask
covered only 0.744% of the atlas and left both tread and rocket masks black, so GDI
and Nod remained hard to distinguish. During the mapped trial, measure mask coverage
and preview it at actual gameplay size. When quick faction recognition is required,
allocate coherent larger panels such as a turret cheek, hull flank or pod face while
keeping dirt, rust, bare metal, rubber and tread outside the recolour mask.

### Separate authored future controls from reachable runtime controls

The current C3M part has role, pivot and name but no parent, rest-local transform or
visibility channel. Preserve the rigid hierarchy because bone ancestry produces the
correct present hull/turret split and prepares a later format extension. State the
current capability precisely:

- turret yaw and full PBR are active;
- barrel recoil, destruction groups and signed independent tread motion are
  reachable with renderer/C3M changes and existing game state;
- launcher pitch has no authoritative elevation value in unchanged Tiberian Dawn;
- per-tube rocket visibility has no authoritative state because Mammoth ammunition
  is unlimited and exposes neither tube index nor remaining magazine;
- cannon versus rocket-weapon fire is distinguishable and may drive separate visual
  effects.

Do not ask an integration agent to invent absent simulation state. Keep such authored
controls labeled future-facing, and record the exact engine signal needed before a
future asset spends triangles or texture space supporting it.

### Construction Yard high v03 — measured refinement lesson (2026-09-24)

The new high refines the preserved existing high rather than reconstructing the
asset. A read-only comparison verifies all 1,310 inherited mesh vertex/face arrays
and local poses unchanged, six declared bevel changes, and 388 detail additions.
Actual evaluated broad-face midpoint-to-boundary setbacks measure .005 cell on
buttresses, .006 on cheeks, .007 on the boom and .0055 on the forearm. Match both
face plane and normal when selecting the remaining planar face for this test;
modifier width alone would not demonstrate overlap-clamp behavior. This sample
method does not certify every corner/miter and complements actual multiviews.
Original footprint/contact checks and all 26,449 inherited static clearance pairs
were repeated within a 31,825-pair scoped audit. No topology defects were reported.
See art/construction_yard/HIGH_POLY_V03_REVIEW.md and its hash-bound evidence.
The revised high still requires renewed artistic approval; previous final-low or
material approvals do not authorize new bakes or certify this new source detail.

### Foundation side-face planarity — Construction Yard v04 (2026-09-24)

Exact bounds and manifold topology did not catch the Yard foundation's two twisted
side quads. Directly connecting different contact and top-overhang polygons made
triangulation form long wedge faces; maximum face-normal projection spread was
.058261685 cell. The user flagged the pinched corners after the high and generated
finish review. Replacing those ramps with a constant-depth vertical upper fascia,
horizontal soffit and original contact plinth removed the defect while preserving
both exact polygons. Corrected face spread is below 7.08e-8 cell, and all 1,697
other high meshes retain geometry/pose/modifier/material signatures. Add explicit
face planarity and actual low-elevation perimeter checks for slabs joining unlike
outlines; do not rely on closed topology or a beauty view. See the Yard v04 review.
Renewed high approval remains required before finish regeneration.

### Yard v05 follow-up: remove the actual notch (2026-09-24)

The user rejected v04's remaining underside recess. Planar fascia alone did not
solve the corner's lifted appearance. V05 fills the recess down to Z=0 inside the
unchanged overall envelope; ground contact now follows that full eight-corner
outline instead of the old seven-corner contact polygon. Record this explicitly
as a proposed asset-specific contact-area amendment, not an unchanged source fit.
All 1,697 other meshes are unchanged. Inspect the exact user-marked crop before
claiming a visual repair; numeric planarity alone is insufficient. See the v05 review.


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

### Medium Tank receiving-mesh repair lesson — 2026-09-24
Enhanced proxy parts may contain open ends and overlapping edge topology even when renders look sound. Check actual final triangulated receiving meshes before UVs. Medium FinalLow01 required simple end caps and an inward hull backing constrained to original bounds, followed by removal of a dangling triangle. Fresh-file checks found zero boundary/nonmanifold edges or degenerate/duplicate faces at 9,948 triangles. Do not infer topology health from shading, and recount after frozen triangulation and repairs. Matched pre-bake views document silhouette retention; fine-detail transfer remains unverified until physical baking.


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

### Moving highlight diagnosis on nominally planar hard-surface parts — 2026-09-24

Do not label a moving bright patch as a broken normal until the channels and the
evaluated geometry have been isolated. Test the shipped/runtime asset with the
tangent normal disabled, metallic forced to zero, roughness forced to one and an
unlit BaseColor view. Then compare each candidate polygon's geometric normal,
corner normals and vertex distance from the intended plane. A plausible beauty
render or a flat-looking silhouette does not prove coplanarity.

The Mammoth rear-deck report reproduced two white triangular flashes with the
normal map disabled and with metallic black. A global roughness-one diagnostic
suppressed the specular component, but the final cause was geometric: 18 vertices
on the nominally flat rear receiver occupied several nearby Z levels. Flattening
those vertices to the measured 0.209645-cell plane and hardening the upper faces
removed the flashes with the normal map both on and off, without changing the
9,990-triangle count. Keep bevels and fitted wells intact; flatten only faces whose
design intent and neighboring coplanar boundary establish one plane. Re-export and
verify under the same gameplay camera, sun and post-processing rather than relying
on Blender alone. Store the channel tests and the final normal-on/off capture.

### Medium Tank chamfer correction during texture work — 2026-09-24

An approved finish image and a promising paint trial can still expose edges that
remain too sharp in the actual mesh. Revise the high source and final low as
versioned geometry, then invalidate UVs, bakes and edge-wear maps derived from the
earlier low. Measure evaluated bevel faces: on the Medium Tank's middle side-skirt
segment, the maximum measured cross-plate chamfer span grew from 0.000599 to
0.003995 cell. A larger bevel modifier setting alone did not guarantee this; the
plate needed extra inward depth. Keep first/last skirt backing shallower where
the end road wheels occupy it, and verify wheel clearance after the change.

**User correction:** the repeated side-panel treatment above was an experiment,
not the selected Medium Tank design. The large Gears 5 inspired chamfers belong
on the core hull and turret armor; return the side-panel details to restrained
edges. Highpoly04 directly spreads the turret's existing top/side shoulder loops,
rather than increasing a clamped generic bevel. The measured source cross-section
grew from 0.003278 to 0.033675 cell. The front hull lip grew from 0.004001 to
0.012987 cell. Both are actual geometry carried into provisional FinalLow04;
neither changed the side-panel contours. No core face flipped or degenerated.

**Close-up correction after Highpoly04:** global topology and wheel-clearance
checks did not detect the nonplanar front skirt seat bridges cutting through the
hull. In Highpoly05, remove those bridges and seat each front side plate with a
continuous contour sampled from the actual deck. Check clearance throughout the
sloped top span, on both sides, rather than at only the endpoints. The corrected
plates have at least 0.000828 cell sampled clearance and no nonmanifold or
degenerate base faces. Planar turret rail covers also need deliberate flat face
normals; inherited smooth shading made a false dark gradient across their tops.
Compare before/after actual meshes under matched Workbench lighting. FinalLow05
preserves these repairs at 9,940 evaluated triangles. Do not treat v04's earlier
topology and running-gear checks as evidence that its panel/hull contact was
clean. The dark lit patch under the turret is contact shading in these views;
the evaluated turret and hull have zero triangle intersections. See the v05
high/low reviews and `CLEANUP_AUDIT.json`.

For a fitted front plate, a raw BVH overlap count includes intentional buried
mounting/contact faces; the corrected Medium high and its matching low both
retain such contacts. Inspect where the intersecting triangles lie, compare
the approved high with the low, and review the exposed contour under matched
lighting. Do not report a zero-overlap requirement for every fitted assembly,
but do reject visible protruding slivers or discontinuous external shading.

Do not fund armor chamfers by making prominent road wheels visibly faceted.
Preserve their radial resolution and simplify small rear lamps, tow eyes or other
subpixel hardware first. Before resuming texturing, obtain renewed geometry and
finish-direction approvals for the revised mesh. Prior paint trials remain
reference experiments, not transferable validated bakes.

**Texture02 trial after FinalLow05 approval:** rebuild UVs on the frozen new
low; the older edge selection and normal atlas cannot be reused because their
coordinates changed. The 4K atlas passed zero exact/sampled overlap and all
hard-normal seam checks, but fine triangles without interior texel centers
remain a delivered-resolution limitation. In a selected-to-active 2K normal
trial, projecting the high turret and front skirts onto broad chamfers already
present in the low created false stair-stepped highlights. Exclude those
duplicated broad-form projections and keep their physical low geometry; retain
small-detail normal transfer on matched receivers. Compare normal-only under
both key directions before applying paint. On this Windows/Blender 5.2 setup,
the first 2K bake crashed during buffer allocation with automatic thread count;
four fixed CPU threads completed both 1K and 2K trials. This is a measured
local workaround, not a blanket resolution reduction. See the Medium
Texture02 material review and saved source/receiver mapping.


### Yard full-bake diagnostics and edge scale — 2026-09-24

Keep broad approved chamfers in the final mesh and measure both their UV sampling
and their projected width through the actual review camera. Yard UV02 has about
834.93 texels per model unit on its crane: .005–.007-unit chamfers span roughly
4.17–5.84 texels at 4K. A .000333-unit bolt bevel is only .278 texel; do not claim
that microscopic edge transfers identically in an extreme close-up. Judge the
actual normal-only low beside the high under matched opposing lights.

Coverage and SourceID parity alone do not prove tangent-normal parity. Yard's
rejected compact per-chart repack retained source identities but changed normal
channels by up to .072. Keep the tested rectangular integer pixel-window method
separate from that rejected experiment. CPU/CUDA controls retained covered
SourceID exactly and bounded normal differences to 1.49e-7; AO remains stochastic.
See art/construction_yard/textures/full_v01/DIAGNOSTICS.md and its control files.


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


### Yard material identity and reflection diagnosis — 2026-09-24

Trial04/04b corrected the curved roof from opaque gray coating to bare metal and both side supports to explicitly requested weathered concrete, with separate metal fittings. Audit complete requested assemblies and small caps by high-source identity; a partial receiver set can retain contradictory old materials. A pale rendered paint face is not evidence of paint loss: the Yard top-cover channel audit retained about 83% strongly intact coating and only 3.6–4.1% strongly exposed steel. Its broad glare required roughness adjustment, not invented wear or altered geometry. Keep source-class masks and physical bakes fixed for a response-only refinement, and verify actual changing-light and alternate-player views. See the [Trial04b review](../art/construction_yard/MATERIAL_TRIAL_V04B_REVIEW.md).


### Yard roof dust appearance correction — 2026-09-24

The user rejected Trial04b's hard-edged light dust islands on bare roof metal as white pecks. A deposit mask can be numerically coherent yet read as unintended paint flakes. Localized opaque deposits used to protect player recoloring are not a universal bare-metal weathering recipe. Trial04c removes this known final dust layer from roof-owned linear color, roughness and metallic together, clears its semantic dust selection, and preserves other grime, surface variation and physical bakes. An unchanged-light before/after confirms the broad white flecks are gone. See `art/construction_yard/refine_material_trial_v04c_roof.py` and its retained numeric recovery diagnostics.


### Medium Tank failed material transfer and front-arch fit — 2026-09-24

Medium MaterialTrial06 passed several map/UV checks but the user rejected its
visual match to both approved FinishDirection02 and the finalized Mammoth. Only
15 meshes had material maps, and the structural normal transferred eight
receivers; most wheel dishes/hubs/bolts and all tread shoes were absent. A
connected normal texture node is not evidence of complete high-to-low coverage.
Audit every receiver against named high sources, then verify wheel and tread
close-ups in normal-only and material views before showing a finish trial.

Applying Mammoth source graphs and numeric tuning to a larger Medium sample in
Trial07 still did not match the selected generated finish: the same object-space
camo scale produced oversized patches, and gray context hid an incomplete asset.
Reuse Mammoth's semantic construction and tuned *method*, then parameterize
pattern scale and grade for the Medium footprint and the Gears 5 reference;
compare under matched lighting. Do not advance a technically baked yet visually
weak trial.

The front wheel-arch spikes were separately traced with matched clay and
per-object diagnostic colours to the low hull protruding through the fitted
skirt, not a tangent-normal problem. Correct the actual mesh on a versioned low,
preserve measured bounds and triangle ceiling, and return to the affected mesh
approval gate before changing UVs or bakes. See the [FinalLow06 review](../art/medium_tank/final_low_v06/REVIEW.md).


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

### 2026-09-24 Medium Tank front/rear geometry correction
The NormalBake06 close-ups were insufficient: subsequent grazing views exposed
folded front lip caps and rear corner defects. The inherited high hull had open
and multiply connected edges; thin Solidify backing and bounds clamping in the
low build made topology statistics look closed without making valid surfaces.
Do not use global hole filling, backing or clamping as proof of geometric repair.
Inspect normal-off front/rear grazing views, then repair local fitted surfaces and
verify the paired source. Manifold counts alone do not detect folded or overlapping
faces. MeshRepair07 is a renewed mesh review, not a new bake or texture approval.
Its hull UVs and prior body normal are obsolete after the topology change.

### 2026-09-25 CUR00 source inspection convention

The standard cursor FBX inspection file uses Blender `(x,-z,y)*0.01`, hence
10.24 Blender units per cell. Its root is zero, but its arrow is intentionally
offset from the root. Preserve that offset when replacing it; do not recenter
geometry or silently treat this inspection scene as the one-unit-per-cell convention.
The complete source has 20 triangles: 19 visible arrow faces and one fully
transparent ground-marker triangle. The latter is reference-only for physical
footprint measurement. InitialLow01 independently compared the complete pack
vertex set with the ROM reader before building. See
`art/cursor/initial_low_v01/verification.json`; first-mesh approval is recorded in
`art/cursor/initial_low_v01/APPROVAL.md` on 2026-09-25.


### 2026-10-06 Runtime stage verification

For a staged runtime asset, distinguish cumulative effects history from the live draw
list. OpenCNC3D's `EFXSEEN` records fires observed before filtering; `EFXDUMP|ANIM` describes
live effects after filtering. G312 must use the latter to reject duplicate engine fires
beside the approved burning-tree mesh. Verify stage pixels against a control and repeat
runs, and report engine-retained versus orphan lifetimes separately. A conditional code
path is not covered merely because the retained-tree case passes. Preserve source/export
hashes and artistic approval separately from these runtime checks. Runtime coverage is
checked by `game/gate_treefire.py`.

### Verified RA runtime import boundary, 2026-10-06

Export approval and later-stage acceptance remain separate. The first RA import uses the
approved intact exports, with original engine art for damage, construction and sale. Keep
RA files in their module namespace even when their names match C&C. Lock all sampled
inputs before baking, preserve the engine footprint and approved plot turn, and verify
house masks, subobject suppression, shadows and Classic fallback in a hidden game run.
The [RA runtime notes](ra3d-structures-runtime.md) record the implementation and checks.

The subsequent runtime review required standing models to survive health loss. Keep the
approved intact geometry while damaged until the separate damage gate is accepted; this
does not accept unseen damage art. Suppress both the main engine draw record (its own
type/ID) and child records (base type/ID). Test live attacks and inspect snow views, since
a healthy-only fixture missed the first-hit fallback and grass multiply gains tint snow.
