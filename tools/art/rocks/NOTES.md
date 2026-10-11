# The small scattered rocks: notes

The stones the game lays at the foot of rock faces under Enhanced (F5 ROCKS, with rock
physics; `game/foliage3d_mod.h`, THE ROCKS and ROCK PHYSICS). Two sets exist:

- **The painted stones** (`build_stones.py`, not committed, rebuilt by that command): the
  fifteen stones as angular faceted rocks in the hand-painted method of the approved
  desert rocks ROCK1 to ROCK7 (`tools/art/remaster/build_ROCKS.py`, `pass_ROCKS.py`), one
  pack per theater. **What a build ships.**
- **`rock3d.pack`** (`data/rocks/`, committed, `bake_rock3d.py`): a photographed asset-store
  pack, rounded stones with scanned textures, the stones before. Still shipped, and drawn
  only on a theater with no painted pack (none today) or when the flag is turned back.

**State: approved, on every theater, and the default.** The requirement was angular stones in the remaster set's hand-painted style in
place of the rounded scanned ones, and a version per theater that blends into that
theater's Enhanced ground; the desert stones follow the small pale ochre rocks of the
Enhanced desert ground (sunlit top near cream, warm brown sides, low contrast with the
reddish ground, nothing grey).

## Switching

One flag, in `game/make-build.sh`: `ROCK_STONES_DEFAULT=painted` (the default) or `unity`
(the stones before). Both builds hand it to `tools/art/rocks/stones.sh`, which with
`painted` bakes `rock3d_DESERT.pack`, `rock3d_TEMPERAT.pack` and `rock3d_WINTER.pack`
(about a minute, cached in `build-native/rocks`, stamped with the script and the pack
writer, until either changes) and copies them into the folder, and with `unity` removes
them, so a folder never mixes the two. The Mac build runs it on the play folder; the
Windows package (`tools/win/make-build-win.sh`) runs it on the package after copying the
play folder's packs, so it carries what the flag says even when the play folder holds an
earlier build's rock packs. `ROCK_STONES=unity` in the environment makes one folder with
the stones before, for a comparison.

## Decisions

1. **One pack per theater, chosen at load.** The game reads `rock3d_<THEATER>.pack` in
   place of `rock3d.pack` when the play folder has it (the snow theater takes WINTER), and
   `rock3d.pack` otherwise; `--rocks <file>` still names one pack for every map. Sitting
   beside the three colourings in one pack would have tripled what every map loads, and
   replacing the colourings would have lost the F5 shares. So each theater pack keeps the
   format, the codes and the three colourings exactly, and only the stones' colours differ
   between packs: the pick code (`rock_pick`) is unchanged.
2. **The three colourings are three stones of the theater's palette**, in the slots the
   F5 shares name: A the darker stone (was dark grey), B the pale lichened one (as before),
   C the warm one (was red-brown). In the desert all three are sandstone: warm tan, pale
   cream with lichen, pale sandy ochre. The F5 help texts say so.
3. **Exposure.** The rock exposure dial (`rock_gain`, 0.25 by default and in saved
   settings) was tuned on the scanned pack, whose albedo is about half as bright as this
   paint and drew the old stones near black. The painted packs carry a code `RKPAINT`
   that names no stone; the game reads it and draws them at the dial times 1.7
   (`ROCK_PAINTED_EXPOSURE`), measured on the Enhanced desert so a sandstone boulder's
   sunlit face matches the ground's own pebbles. One dial still moves every rock, and
   saved settings need no change.
4. **Heights 0.7 to 1.0 of the longest side.** The game buries 0.41 of a rock's size
   (`rock_sink`) and stands it on its lowest vertex, so a stone flatter than that would
   vanish into the ground; what shows is the top of the stone. Visible size is close to the
   old stones'.
5. **The painted light is the form from straight above**, not the set's key light: a loose
   stone lies at any yaw, so a light baked from one side would point anywhere. A face
   turned up shows its tone, an upright one about three quarters of it; the game's sun does the rest.
   No cast shadow or large occlusion is painted.
6. **Texel density.** Painted at the set's 232 texels a cell in grain (every pattern's size
   in cells at the stone's reference size: the default 0.30 cells for a boulder, 0.45 of it
   for a detail stone), stored at 300 to 420 texels a cell (boulders on 256 sheets, detail
   stones on 128), so the size dial and the size variation can enlarge a stone without
   blur. The normal sheets are flat: the facets are geometry.

## The stones

| | kind | triangles | | kind | triangles |
|---|---|---|---|---|---|
| RK01 | block | 48 | RK09 | slab | 70 |
| RK02 | slab | 60 | RK10 | split boulder | 109 |
| RK03 | split boulder | 110 | RK11 | flat pebble | 66 |
| RK04 | wedge | 50 | RK12 | chunk | 66 |
| RK05 | shard | 52 | RK13 | wedge | 50 |
| RK06 | chunk | 70 | RK14 | chip | 42 |
| RK07 | slab with a block on it | 78 | RK15 | block | 50 |
| RK08 | block | 50 | | | |

971 triangles for the fifteen (the scanned pack: 3,200). Each is a convex block (or two):
the hull of a few points scattered over a rounded box, then one or two flat splits; a split
boulder is one block cut down a joint with its halves leaning apart; RK07's upper block
stands on a flat bed cut into the slab. Convex, so the physics' 26 support points are the
stone's true corners.

## The paint

`pass_ROCKS.py`'s stone, layer for layer, every pattern from the 3D position at four
sub-texel samples: beds of close tones with a few faint partings, flakes each a shade of
its own, mottling at two scales, a grain two set texels across, a few long cracks with a
lit lip, desert varnish down steep faces, lichen crusts, then the theater's dressing on the
faces turned up (desert sand, temperate moss, winter snow patches), a band at the foot,
the set's wash (0.28, lighter than ROCK1 to ROCK7's 0.38: a loose stone is cleaner) and a
soft lift along every ridge. The patterns are computed once a stone and shared by the nine
palettes. Palettes: desert from the approved desert rocks and the Enhanced desert ground's
pebbles; temperate from the Enhanced temperate cliffs (warm grey-beige); winter from the
Enhanced winter cliffs (cool grey, snow on the tops).

## Checks

- Enhanced game shots, `--texset 3`, desert (SCB01EA), temperate (SCG07EA) and winter, the
  old stones beside the painted at one camera and both zooms. No winter mission has rock
  faces, so the winter shots are SCG07EA's terrain re-skinned on the winter theater
  (`--reskin` with the winter donor pack, heights flattened).
- A play folder without the theater packs draws a picture byte-identical to the build
  before the change (desert, nearest zoom).
- G268 passes with the painted stones and with the old (18 rocks on SCG01EA, the pulse
  moves 4, two runs one picture), and the picking and sidebar gates pass.
- Physics, old against painted, three pulses on the SCB01EA hillside, 31 bodies: the same
  number asleep after 200 and 600 ticks (1 and 2) and the same creep (about half a cell
  over 400 ticks). Few bodies ever sleep on a slope with either set; that is the contact
  model's, not the stones'.
- Two builds of the packs are byte-identical.

## The switch to the painted stones

With the flag at `painted`, every Enhanced gate that could see a rock was run in two folders
of the same binary, one carrying the painted packs and one without: G222, G242 to G245,
G254, G258, G267, G268, G273 to G275 (and the G276 to G292 run inside it), G294 to G296,
G298, G300, G303 to G310. Every verdict is the same in both, and every result line is the
same word for word but G268's sampled pixel count after the pulse (7,494 old, 7,506
painted; its floor is 20). No gate pins a number the stones move, so none was changed. The
gates that fail there fail identically with both stones (a run folder without the newest
buildings and models), so they say nothing about the rocks. The picking test and the
sidebar gate pass with the painted packs.
