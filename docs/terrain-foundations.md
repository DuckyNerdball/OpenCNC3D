# Smooth, connected building foundations

On 3 November 2026 the design review reported square raised ground with sharp vertical
edges and explicitly chose **“Yes—adjust heights where needed”** to prioritize
smooth connected terrain. This supersedes the 2 November choice of fixed
independent building elevations and stepped foundations.

Touching building footprints, including footprints sharing a corner, form one
platform. Its level is the highest original map corner beneath those footprints.
Every building on that platform has a flat, unburied base. Adding or removing a
connecting building may adjust the other buildings’ elevations; this is now an
explicitly accepted consequence. Separate platforms retain their own source levels.

All neighbouring terrain cells use the same shared corner heights. A three-cell
radial skirt blends raised platforms into the surrounding ground; the apron
follows that slope instead of becoming a separate flat shelf. The blend uses a
smooth falloff and integer accumulation, so overlapping skirts do not depend on
construction/object order. Terrain is raised rather than excavated. No vertical
foundation faces remain in colour, normal, sun/depth or reflection passes.

The original raw heightmap, baked terrain artwork, authored meshes and textures
remain unchanged. Ground queries, overlays, picking, physics support and rendered
terrain read the same shared height field. Existing ground-revision and pad-epoch
invalidation refresh geometry, shadow and terrain caches when platforms change.

No new save fields are required. Older saves adopt the current foundation policy
when rendered. Campaign save/load checks rebuild the saved platform graph after
an extra structure is built in an abandoned timeline. Skirmish save support
remains a separate existing limitation.

## Verification

- `tools/regressions/foundations/foundations.cpp`: 2,506 cases with an independent
  rectangle-connectivity reference, random insertions, removal/rebuild, apron
  transitions, order independence, map bounds, unchanged raw data, flat building
  support, and exact shared heights on every adjoining edge.
- `tools/regressions/foundations/probe.py --expect-connected --save-load --capture`:
  legal sidebar placements on both GDI Special Ops1 variants and RA Allied8,
  flat/unburied buildings, zero vertical faces in every pass, stability after
  300 ticks, and save/load after an extra construction in an abandoned timeline.
- G239 retains native-base, live placement and `--noflatpads` negative controls.
  The current expectation is shared platform levels, replacing the 2 November
  fixed-height/vertical-seam assertions.
- Native water-contact/ownership checks, gameplay, Mac deployment and Windows
  verification remain separate evidence. A compiler pass alone proves none of these.

The 2 November independent-height results remain historical evidence in
[the earlier follow-up](verification/2026-11-02-foundation-followup/README.md).
No authored 3D assets or HUD artwork are introduced by this renderer change.
