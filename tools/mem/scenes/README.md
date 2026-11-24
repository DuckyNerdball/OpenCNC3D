# Memory scenes

Framed, scripted scenes for proving that a memory change leaves every picture
byte-identical. `tools/mem/ab.py` runs them with one or two renderers and compares every
shot by its SHA-1; see the top of that script for how.

A memory change can only be judged by shots that show what it touches. So every scene
lifts the shroud (`reveal 1`, which changes nothing the engine knows) and points the
camera (`cam X Z`, in cells) at the remastered buildings, the coast and the water, the
ground the mottle and grass are laid on, and damage: buildings hit past half strength
(their damage pictures, fire and smoke), some destroyed (wrecks and craters).

| scene | game, mission | modes | what is framed |
|---|---|---|---|
| S1 | Red Alert SCG01EA (snow) | Enhanced | the Soviet base west with the lake, east, the lake and coast; t1, t300; then every building kind damaged, t320 and t420 |
| S2 | Tiberian Dawn SCG01EA (temperate) | Enhanced | beach and sea with the boat, the Nod gun line, the meadow; t1, t300; the guns destroyed, t320, t420 |
| S3 | Red Alert skirmish SCM01EA, seed 4242, 7 computer players | all three | the start at t1; three computer bases and the coast at t600 |
| S4 | Red Alert SCG01EA, Enhanced then Classic before tick 1 | one | S1's base and lake in Classic, t1 and t300 |
| S5, S6 | S1 started cold in Classic, in N64 | one each | as S1 |
| S7, S8 | S2 started cold in Classic, in N64 | one each | as S2 |
| S9 | Red Alert SCG05EA, the first temperate Allied mission | all three | the base, the yard and airfields, a field by the river; damage |
| S10 | Red Alert SCG10EA (snow) | all three | the yard, the missile silos, the south; damage |
| S11 | Tiberian Dawn SCB01EA (desert) | all three | the village and the Nod start; two houses destroyed |
| S12 | Red Alert SCG10EB (indoors) | all three | the start, the tank hall, the power plants; barrels blown up |
| S13 | Red Alert SCG01EA | all three | a barracks captured by an engineer: before, during, after |
| S14 | Red Alert SCG01EA, started cold in Classic | one | soldiers killed in Classic, then the switch to Enhanced |
| S15 | S1 with the remastered soldiers off (`--noeinf`, `gfx einf_on 0`) | Enhanced | as S1 |
| ground-bridge | Red Alert SCU02EA (temperate) | Enhanced, N64 | the bridge at tick 30, saved; both spans blown (two views of the gap); the save loaded and the bridge standing again: two mid-mission rebuilds of the ground |
| ground-cycle | Red Alert SCG01EA (snow) | all three | the base and the lake in the starting mode, then in N64, Classic and Enhanced in turn in one session, and N64 again after 40 ticks: the ground built again for each set and the bilinear toggle switched each time |
| ground-bridge-cycle | Red Alert SCU02EA (temperate) | Enhanced, N64 | as ground-bridge, with a switch to N64 and back to Enhanced after the blast and after the load |

## The file format

    # comment lines
    game: ra | td
    scen: SCG01EA
    modes: enh,cls,n64     (enh = --enhanced --texset 3; cls and n64 are cold starts)
    seed: 12345            (default)
    build: 98              (default; none leaves --build off, as a skirmish does)
    flags: --noeinf        (anything else for the command line)
    env: KEY=VALUE ...     (environment for every run of the scene)
    extra: LINE[; LINE]    (script lines put right after the gfxload of the frozen settings)
    like: S1               (take every field and the script of S1, then override)
    ---
    the script, with {OUT} for the run's folder and {CFG} for the mode's settings

`memledger LABEL` lines print the memory ledger (game/memledger.h); a renderer that has no
such verb gets them removed from its script, since the verb draws nothing.

## The frozen settings (cfg/)

Each mode runs with a fixed graphics settings file, so a shot does not change when the
defaults do: `ra.cfg` and `td.cfg` are the Enhanced settings the game's memory was first
measured with (sha256 c3a40919... and 48c5ef00...); the Classic and N64 files are the
same with `enabled 0` and, for Classic, `dos_classic 1`.

## One game at a time

Two game processes drawing at once on one Mac were measured to flip one to a few
hundred pixels by one or two levels in a small share of shots (18 of 110 comparisons in
six parallel runs of S1, S9 and S13); the same scenes run one at a time gave the same
bytes in every run (0 of 70). ab.py therefore runs one game at a time, records how many
other game processes were running beside each run, and settles any difference by
rerunning the scene before it reports one.
