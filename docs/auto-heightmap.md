# The auto heightmap

A heightmap for a map that ships none, generated from its 2D tiles alone, in the style
of the 55 hand-made heightmaps on the cartridge.

**Which maps need it.** Every mission the console can put you in has a hand-made
heightmap: the 51 campaign missions and the four N64 Special Ops (SCG30EA, SCG90EA,
SCB21EA, SCB22EB). Nothing else does. The thirteen Covert Operations missions, the
bonus and debug scenarios, the 16 DOS multiplayer maps and the five dinosaur missions are
flat, on the console as well as here; the 1995 map format has no height field at all.
`docs/terrain-elevation-grammar.md` section 0 has the evidence.

**Where it lives.** `tools/autoheight/`:

| tool | what it does |
|---|---|
| `corpus.py` | every cartridge map as grids: template, icon, the 1995 land type of each icon, family, heights, buildings |
| `analyse.py` | the measurements below; every number in this document is its output |
| `generate.py` | the generator; `python3 generate.py SCG22EA --out DIR` writes `SCG22EA.HGT` |
| `score.py` | the scoreboard: regenerate each hand-made map from its tiles and compare |
| `check.py` | the playability check against the cartridge's own worst cases |
| `compare.py`, `render.py` | pictures: tiles, real relief, generated relief side by side |
| `ablate.py` | the scoreboard with any generator setting overridden |

`docs/terrain-elevation-grammar.md` measured the vocabulary one tile at a time: the
five-rung ladder (0, 64, 128, 191, 255; one rung is one cell of height), which families
carry relief, the 38-way cliff compass. This document measures the grammar: how tiles
behave in relation to the patch of map around them, which is what a generator needs.

---

## 1. What the cartridge's designers did

Run `python3 tools/autoheight/analyse.py` to reproduce every number here.

**A. Cliff art does not enclose its plateaus.** Cut the open ground apart wherever there
is cliff, water or rock art, and only 45% of the resulting 217 patches are level; 60.7% of
open cells sit within 8 bytes of their patch's median. Most patches span two or more
levels, joined where a cliff line has a gap. The designer drew cliff fragments and then
closed each plateau across the gaps with plain ground. On SCB22EB the cliff fragments are
short and scattered, and the plateaus they belong to are large, continuous shapes.

**B. A cliff placement is one rung between its two sides.** Two cells beyond its high side
and two beyond its low side, 73% of 4094 cliff placements differ by 58 to 70 bytes, median
exactly 64. Which cells carry the climb varies (the cell beside the art often carries part
of it); what a placement guarantees is the rung.

**C. Waterfalls are cliffs with a fixed direction.** FALLS1 is high in the east (16
placements, median rise 59), FALLS2 high in the north (29, median 46).

**D. Water sits under the lowest terrace that touches it.** Over 120 bodies of 40 or more
cells, the surface is a median 12.5 bytes under its median bank and up to 24 under its
lowest. By the terrace it sits in: banks on 64, surface 52.8; banks on 128, surface 108.4;
banks on 191, surface 171.0. A body is carved into one terrace; it never floats between two.

**E. Shores are grooves, not cliffs.** 278 shore placements drop a median 19.6 bytes from
land to water (90th percentile 31.2). Where a plateau meets the sea at full height, there is
cliff art saying so.

**F. Beaches are on the base terrace 60% of the time** and one rung up the other 40% (a
highland lake has highland beaches). The tiles cannot tell the two apart, so the generator
does not try.

**G. The main ground is rung 1** (64) on 52 of 55 maps, leaving room below for carved water
and the occasional basin.

**H. Pre-placed buildings stand on levelled ground**: 1404 of 1831 (77%) on corners within
one byte of each other.

**I. Walkable ground is gentle.** Over 104069 walkable cells (not cliff, water or rock), the
corner span is 0 at the median, 13 at the 90th percentile and 33 at the 99th.

**J. A ramp without cliff art is three to four cells long.** Of 1098 one-rung climbs over
walkable ground only, 26% take three cells, 27% four, 12% five, 10% two.

**K. Bridge decks sit at bank height**: median 0 bytes from their banks over 73 bridges.

**L. What the tiles cannot say.** 9.3% of open cells sit more than 8 bytes off a rung:
ramps, plus swells and basins with no art under them. The cartridge ships SCG05EA and
SCG05WA with identical tiles and heightmaps differing in 2259 of 4225 corners. A
generator working from tiles therefore has a ceiling well short of an exact copy.

---

## 2. The generator

Everything it knows it measures from the 55 maps when it starts (`generate.train`). Three
stages.

**1. Levels.** Each cliff and waterfall placement is one equation: the ground two cells
beyond its high side is one rung above the ground two cells beyond its low side (B, C).
Each pair of neighbouring open cells is a weaker equation saying "same level", and each
pre-placed building's cells are tied to one level (H). Every cell also leans, very
weakly, toward the base level. The fit is robust (iteratively reweighted toward a
least-absolute-error fit), which is what makes it behave like the designers did:

- **Gaps close with the shortest boundary.** Where a cliff line has gaps, the cheapest
  way to satisfy every placement is a short straight edge across each gap, not a long
  sag over the whole plateau (A).
- **Contradictions are out-voted.** Where two pieces of art disagree, the minority loses
  instead of tearing the map.

The solve covers the playable rectangle only. Plateaus run off its edge as they do on the
cartridge; a graph that went on outside it charged a plateau for closing in the border,
and a single long cliff raised a thin ridge instead of everything on its high side.

**2. Rungs.** Levels are rounded onto the ladder, the lowest substantial land on rung 1
(G). Each body of water then takes the lowest terrace that makes up a real share of its
bank (D). Outside the playable rectangle each cell copies the nearest playable cell.

**3. Shape.** Every cell's four corners come from what its icon looked like on the
cartridge:

- **Cliff and waterfall icons** sit at their measured fraction of the way from the low
  rung to the high one.
- **Every other icon** sits at its measured offset from its terrace: zero for grass, the
  groove depth for a river or shore (D, E).
- **Shared corners** take the confidence-weighted mean of what each cell predicts.
- **Bridges** are a level deck at the median height of their banks (K).
- **Pre-placed buildings** are levelled to the highest terrace they stand on, the same
  raise-never-bury rule the renderer's building pads use (H).
- **Steep walkable cells** are spread into a ramp. Any walkable cell spanning more than 20
  bytes has its free corners relaxed toward their neighbours until none does, which gives
  the three-to-four-cell climb of J. Cliff corners, pads, bridges and open water do not
  move.

**4. Swells (a deliberate addition, not the cartridge's).** The cartridge's designers left
most open ground level. Where a map has a large stretch with no cliff, water, shore,
bridge, ramp or starting building, the generator gives it broad, rounded rises:

- **Taller with more room.** A rise may begin 1.5 cells from anything that must stay
  where it is and reaches full height (96 bytes, one and a half rungs) only 11 cells from
  it, so only the biggest open patches get the tallest crowns.
- **Its shape is a broad smooth random field**, the same on every run (seeded by the
  scenario's name), leaning upward where the ground is most open, with its tops
  flattened: crowns are rounded, never pointed.
- **Gentle everywhere, locally.** A corner may stand at most 9 bytes above a neighbour,
  so no cell climbs more than about 18; where the field asks for more, the high side is
  cut back and the cut rounded, rather than the whole map's swells being scaled down.

On the Covert Ops maps this lifts up to 96 bytes on Infiltrated! and 66 on Under Siege,
and next to nothing on maps packed with cliffs (Ground Zero, Twist of Fate). It adds no
steep cells and no pointed tops (see Playability).

### What each stage is worth

`python3 tools/autoheight/score.py flat levels_only generate`. Each map is regenerated by
a model trained without its own mission (and so without its sister variants, which share
most of their ground). Measured inside the playable rectangle:

| generator | mean error | after best offset | open ground on the right rung | relief in the right places (F1) |
|---|---:|---:|---:|---:|
| flat | 38.7 | 27.4 | 60.4% | 0.00 |
| levels only | 24.6 | 15.3 | 87.2% | 0.31 |
| full generator, without swells | 16.3 | 11.0 | 87.0% | 0.72 |
| full generator | 17.1 | 11.7 | 86.6% | 0.72 |

The swells cost 0.6 bytes of resemblance: by design, since the originals leave open
ground level. Bytes, 64 to a rung. "After best offset" forgives a map that is right in shape but sits a
whole rung higher or lower than the original. Half the maps come out within about 6.5
bytes of the hand-made original. The worst (SCB11EB, SCG15EC, SCB22EB) are the maps where
the designer raised or sank whole regions with no art under them (L).

Choices the scoreboard did not make. These are look and feel, and the numbers above show
what each costs:

- **Ramp limit 20 bytes.** It costs 0.16 bytes of score against a 33-byte limit. It is
  taken because it gives the three-to-four-cell ramps of J.
- **Ramp spreading at all.** It costs about 0.25 bytes against none. It is taken because
  without it the gaps in cliff lines become walls of walkable ground.

### Playability

`python3 tools/autoheight/check.py`. The 1995 engine has no height: passability,
pathing, line of sight and weapon ranges are decided on the flat grid, so no heightmap can
make an area unreachable or a base unbuildable. What a heightmap can do is look wrong for
play, and the check measures that against the cartridge's worst cases:

| defect | test | Covert Ops result |
|---|---|---|
| steep walkable cells | span beyond 33 bytes, the cartridge's 99th percentile | 0 to 1.6% of cells per map |
| tilted bridge decks | span over 8 bytes | none |
| sloping open water | span over 8 bytes | in the cartridge's own range (e.g. SCG07EA: 8 generated, 9 real) |
| spikes | a corner a rung away from its neighbours | at most 1 per map |
| pointed tops | a walkable corner more than 6 bytes above its neighbours' mean | 0 to 9 per map, none from the swells (the cartridge's own maps: median 13) |

The steep walkable cells that remain are pinned between fixed corners (a cliff's profile
on one side and a building pad on the other) and cannot be spread.

---

## 3. Where it runs

- **The game.** `data/autoheight/<SCEN>.HGT` holds the approved heightmap of every map
  the cartridge left flat that the game offers: the thirteen Covert Operations missions,
  the nine multiplayer maps, the five bonus missions and six test scenarios. The bake
  reads it for any scenario without a cartridge heightmap, and the build re-bakes those
  packs and refuses if one does not carry its heights (`check_packs.py`). A map that
  shares another's tiles and playable rectangle takes that map's heights, the
  cartridge's own where it has them (SCB70EA is SCB12EA's ground).
- **The map editor's AUTO HEIGHTMAP.** `game/autoheight_mod.h` is the generator in C++,
  with everything it learned and every tuning number in `game/autoheight_tables.h`,
  which `export_tables.py` writes (`--check` says whether it is current).
  `port_check.py` holds the C++ against the Python map by map; with the swells off they
  agree to a few hundredths of a byte a corner. The swells' rolling field comes from a
  different random generator in each, so they roll in different places. On a
  multiplayer map every start is a level base site five cells round.
