# Fix and improve: the cartridge models, completed and doubled

A recipe for taking one Nintendo 64 model out of the baked pack, repairing what the
console left out, roughly doubling its triangles where that reads as smoother, raising
its textures 4x, and reviewing the result beside the original. It never edits the pack
and never changes the game: the output is a Blender file and a pair of models in the
side-by-side viewer.

## What goes wrong on the cartridge, and what the recipe does about it

| Symptom | Why the cartridge did it | Repair |
|---|---|---|
| A round part is only half built (the Power Plant's tower has 8 of its 12 sides) | the console camera looks from the south and never turns, so north-facing sides were never drawn | **mirror** the built half across the part's symmetry plane (`complete_by_mirror`) |
| A block is modelled on its outside only (the Power Plant's arc has no inner wall and no ends) | same | **close** it: inner wall, fascia, end caps, built from the existing edges |
| Pipes, brackets, crates have no backs | same | **convex hull** of the prop, keeping every original face (`close_convex`) |
| Round things are visibly polygons | a 12-sided tower was the budget | **densify**: cut every edge that runs round a curve and push the new vertex out onto the fitted circle (`densify_curve`) |
| Shading is flat per face and bands | the console bakes colour per vertex, often painted per face | **re-shade the whole part** from one smooth fit of the cartridge's own colours (`smooth_ring_shade`), never just the new half, or the part reads as two halves |
| Textures are 8 to 64 texels | the console's texture memory | **4x upscale**, tile-aware (`upscale.py`, hybrid Scale2x + Lanczos) |

Every repair keeps the cartridge's UVs and baked colour where the cartridge stated them.
New faces borrow: a mirror copy carries its source face's UVs (the texture was painted
for the side that was shown), a hull face gets a planar fit of the prop's own UVs, a new
wall gets the concrete of the slab it stands on at the slab's texel density.

## The steps, per model

```
tools/art/n64fix/run.sh <TYPE>
```

runs 2 to 6 below. Step 1 and the plan in step 3 are the parts that take judgement.

1. **References first.** Before touching the mesh, know what the building is supposed
   to look like from every side.
   - `python3 tools/art/n64fix/refs.py <TYPE> <work>/refs` pulls the Remastered
     Collection's finished frame and construction animation from the player's own
     install. The construction animation is the best evidence for hidden sides: it
     builds up from the foundation and shows inner walls and end faces.
   - Search online for console captures (they only ever show the front), strategy-guide
     renders, the 1995 sprite. Write down, per part: is it round, a full ring or an arc,
     does it have an inner wall, what is on the side the console never showed. Invent
     nothing that no reference supports; where they disagree, the cartridge's own shape
     wins over the Remastered one.
2. **Extract and survey.** `extract.py` (triangles, both house palettes), `audit.py`
   (open-edge loops, holes rendered cyan on magenta from eight bearings), `islands.py`
   (the connected pieces, each in its own colour), `inspect_view.py` for close views.
   Name every piece and say which of the symptoms above it has.
3. **Write the plan**, `fix_<TYPE>.py`, using the operations in `repair.py`. The Power
   Plant's (`fix_NUKE.py`) is the worked example. Tag each face with its part
   (`fix_part`) so later steps can find it after the mesh has been cut up.
4. **Textures.** `upscale.py --dir <work>/src` (needs the system Python: Blender's has
   no PIL).
5. **Run the plan**, then **audit** with `audit_fixed.py`: every remaining open loop must
   be one that cannot be seen (a bottom resting on the slab, the slab's underside, a
   vent plate's rim), and the inconsistent-winding count must be **zero**. Then look at
   the eight-bearing sheet and close-ups: cyan anywhere is a hole or a flipped face.
6. **Review in the viewer**, left the cartridge, right the fix, both palettes, the
   holes toggle on. `run.sh` writes both sides (`export_old.py`, `export_new.py`) and
   refreshes the list (`viewer_index.py`); a plan only has to repair and save its
   `.blend`. While a model is being worked on, a `WORKING` file in its viewer folder
   shows it as "fixing" in the list.

## Traps met on the Power Plant

- **The cartridge's round parts are unevenly spaced but symmetric.** Sides at 0, 28, 58,
  90, 122 ... degrees. A rotated copy misses the existing edges by a degree or two; a
  mirror copy meets them. Judge symmetry on the set of side ANGLES (a complete rim ring
  states every side), not on vertices, because the missing half has none.
- **A round part can be symmetric about two planes.** Pick the one that fills the most
  missing area, not the lowest error: the east-west mirror of the tower fits best and
  fills nothing.
- **"Symmetric" means to about 11 model units.** Weld mirror copies at 15; no two
  vertices on a part that size are closer than 100.
- **Test "already covered" against the surface, not triangle centres.** A mirrored quad
  splits along the other diagonal, so its triangles never share a centre with the
  original's.
- **Non-planar caps fold.** Fan them from a corner by hand, then make every new face
  agree in winding with the cartridge faces it meets (`agree_winding`), rather than
  guessing an outward direction.
- **A fan's straight chord can leave a notch.** Where a new wall's foot lands outside an
  old fan-shaped face, patch the ledge with one triangle mapped like the fan.
- **The baked colour is mostly painted, not lit.** One sun direction explains only part
  of it; do not relight the model from a fitted light.

## Budget and Win98

The Power Plant goes from 132 to 317 triangles (2.4x) and its textures from at most
32x32 to at most 128x128 (the pipe strip 64x256). Everything stays inside the Voodoo 2's
256-texel limit. Nothing here is in the game yet; which path a fixed model takes in
(the pack, drawn by every renderer, or the Enhanced art set, Tier 2 only) is an open
decision, and it decides the Win98 answer.

## Files

| | |
|---|---|
| `run.sh` | steps 2 to 6 for one type |
| `refs.py` | Remastered reference frames from the player's install |
| `extract.py` | the model out of the pack, both palettes |
| `blend_common.py` | rebuild in Blender, review renders, viewer export |
| `repair.py` | the generic repairs |
| `fix_<TYPE>.py` | one model's plan |
| `upscale.py` | the textures, 4x |
| `audit.py`, `islands.py`, `inspect_view.py`, `audit_fixed.py` | survey and checks |
| `export_old.py`, `export_new.py`, `viewer_index.py` | the viewer's two sides and its list |
| `survey_all.sh` | every structure's cartridge model into the viewer |
| `viewer/` | the side-by-side page; launch config `n64fix`, port 8795 |
