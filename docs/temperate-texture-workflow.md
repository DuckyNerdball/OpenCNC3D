# Temperate texture production workflow

This is the approved approach for the Experimental Art Temperate terrain set.
The runtime cells are **256×256**. Keep the original generated high-resolution
images as the color-detail source; do not regenerate the approved art or enlarge
the old 128px cells as replacement RGB textures. The 128px approved cells still
define cliff/road footprints, grass and gravel boundaries, and a few edge
pixels. `tools/bakery/make_temperate_256.py` makes the 256px cells, and
`tools/bakery/pack_experimental_terrain.py` makes the runtime packs. The latter
runs from `game/build.sh` and `game/make-build.sh`.

## Sources and cell treatment

- Grass: `art/generated/temperate_grass/v1/source_gpt_image.png` is one
  continuous 4×4 field. Grade and resize it as a whole, make only the **outer
  edge of the complete field** repeat with a three-pixel correction, then split
  into 16 cells. Do not blend each cell to the 128px guide: that eight-pixel
  blend caused the blurry grid lines seen in-game.
- Cliffs: the original `source_gpt_image.png` under each
  `art/generated/temperate_cliffs/approved_style_v1/Sxx/` template supplies
  stone detail. S03 uses `art/generated/temperate_cliffs/s03/v5/`.
  Resize the full template, apply the approved footprint, fill open areas with
  approved grass, and limit color edge correction to three pixels. Derive
  256px height and normal cells from the result and the approved height guide.
- Roads: use each `art/generated/temperate_roads/approved_style_v1/Dxx/`
  high-resolution source when present. The script uses the approved D15 or
  D02 high-resolution source for templates without one. Process a whole road
  template before splitting it into cells. Apply the approved road footprint,
  grade gravel to the common material, and match road exits to a periodic
  high-resolution D15 gravel strip. Limit the **RGB** edge correction to three
  pixels; the former wide RGB blend appeared as blurry cross-tile lines. Join
  internal color, height, and roughness boundaries, then derive the normals
  from the continuous template. Height still has its separate broad edge ramp
  to avoid a geometry step; do not use that ramp for RGB.
- Shores: each high-resolution source under
  `art/generated/temperate_shores/remastered_style_v1/SHxx/` was edited from
  the matching **Remastered Temperate** assembled template. The Remastered
  coastline, beach width, rock positions, and material layout lead the art;
  no old N64 colors or geometry are used as the design source. Baked trees were
  removed from SH12–SH16. Resize each whole source before splitting into
  256px cells. Match grass to the approved grass material and use a pure dry
  sand area of SH1 for a periodic high-resolution sand material; sampling the
  wet beach in that reference caused repeating dark rectangular bands. Keep
  source grit and wet/dry variation inside cells. After cutting, match the
  edges of shore cells that actually meet in the shipped maps, by material so
  water pixels cannot enter grass or sand. Preserve grass-facing exits from
  those matches. The engine retains its existing cartridge alpha
  solely to cut out open water. That alpha identifies 131 visible shore cells;
  28 fully transparent shore cells have no visible land art and are deferred
  to the water pass. Nine templates with rocks supply 56 visible height and
  normal cells through `make_temperate_shores_256.py`.
- Bridges: the two high-resolution sheets under
  `art/generated/temperate_bridges/remastered_style_v1/` follow their matching
  Remastered Temperate bridge layouts. Bake the complete BRIDGE1 and BRIDGE2
  sheets with `make_temperate_bridges_256.py`, then split into 35 cells. Their
  gravel approaches use the approved D15 road source, graded to the D02 road
  material, so they meet the earlier road set. Preserve the source's exposed
  stone paving and worn tracks across the bridge deck: extend a light gravel
  blend onto the deck, while keeping the paving readable through its middle.
  A full gravel overlay hides the stone. The paving and parapets have raised height
  and normal detail; the approach gravel has recessed ruts, its own normal
  response, and a damp roughness response. Join shared RGB,
  height, normal, and roughness edges before packing. The cartridge alpha still
  cuts out open water.

Keep the road border's transition from the base terrain normal to the authored
normal narrow. Applying the road normal across grass pixels previously made
dark slots. Keep all new art behind **F5 → Experimental Art** and the relief
effects behind **Terrain Displacement**. The existing depth slider changes
road, cliff, shore-rock, and bridge relief in-game. The 3D grass controls live in the
same Experimental tab.

## Rebuild and check

1. Run `python3 tools/bakery/pack_experimental_terrain.py` from the repo root,
   or build the game with `game/build.sh` / `game/make-build.sh`.
2. Confirm the generated cells remain 256×256 and the count remains 16 grass,
   157 cliffs, 206 roads, 131 visible shore cells, and 35 bridge cells. Generated 256px intermediate files and packed
   runtime files are reproducible build products. Keep the original source art
   and scripts in Git so another machine can rebuild them.
3. Inspect adjacent grass cells, connected cliff edges, road exits and junctions
   at game scale. Use `game/examples/USER92` for roads and `SCG01EA` for
   shoreline beside grass and cliffs; use `SCG05EA` and `SCG06EA` for the two
   bridge sheets and their road approaches. Enable Enhanced mode, Experimental Art,
   and Terrain Displacement; compare depth zero and 0.25. Watch for seams,
   blurred cell borders, dark road slots, and changes to grass detail.

For an apparent broad shore seam, compare the same frozen frame with relief,
cloud shadows, and one-sun Lambert lighting toggled independently. Cloud
strength 0.473 made one beach section noticeably darker, so its default is
0.16; retain the F5 control. A second, fixed vertical band on the left of
SCG01EA remained with clouds off. The relief shore tiles there had no pixels
in the ground-normal target, so one-sun lighting reconstructed a different
normal at the tile boundary. The normal pass was sending unscaled UVs into
the padded terrain atlas; it now uses the same usable-area scale as the color
pass. `fx_draw_terrain_normals` also draws the terrain normal beneath every
relief tile, which the relief normal overwrites wherever its mapped surface
draws. At depth 0.25, compare both parallax and geometry modes against the
depth-off beach. Do not repaint shore art to compensate for a lighting-buffer
gap.

`docs/temperate-texture-cell-status.csv` is the per-slot inventory. Its
`new_color_path` points to approved guide or high-resolution source art; it is
not a list of the generated 256px runtime payloads. Count `status = new art` to
get the completed cells. As of 2026-09-28: **660 of 758** Temperate atlas slots
are redone (16 grass, 157 cliffs, 206 roads, 131 visible shores, 35 bridges,
115 rivers); 98 remain on the Remastered fallback, including 28 fully transparent
shore cells. Height,
normal, and roughness channels belong to these same cells and are not
additional cell counts.

River cells RV01–RV13 use versioned high-resolution source sheets generated
against the Remastered layouts. `tools/bakery/make_temperate_rivers_256.py`
resamples complete sheets to the exact cell grid, matches the exposed grass to
the approved CLEAR material, and joins grass-facing exits. It preserves each
sheet's internal water and bank joins; applying the shore cross-template edge
matcher to these mixed water/land edges caused dark rectangular water strips.
The 115 RGB cells and their stone-masked height and normal maps are packed by
`tools/bakery/pack_experimental_terrain.py`. Experimental Art loads them only
with the toggle on. Rock relief uses the same F5 depth control as the earlier
cliffs, roads, shores, and bridges; water and wet banks remain near datum.
