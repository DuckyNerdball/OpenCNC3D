# Phase 0: the cross architecture determinism gate

`docs/design-multiplayer.md` section 10, Phase 0. Lockstep multiplayer needs the macOS
arm64 build and the 64 bit (x86_64) Windows build of the SAME commit to simulate bit for
bit alike. The proof below was first made against the 32 bit (i686) Windows build, before
Windows went 64 bit only, and was made again for x86_64 (see the last section).
This folder holds the tool that turns a per tick engine dump into one hash per tick, and
the captured hashes from each machine, named by platform, commit, scenario and tick count.

## The recipe, identical on either platform

```
OPENCNC3D_TICKDUMP=1 <cncbrain> content/ missions/ SCB01EA 20000 > capture.log
python3 tools/phase0/tickhash.py capture.log > <platform>-<commit>-SCB01EA-20000.sha256
```

`cncbrain` is `brain/host/cnc_host.c`. On the Mac it is one compile with no build rule of
its own, `cc -std=gnu99 -O2 -g -Wall brain/host/cnc_host.c -o cncbrain`, run from the
repository root so it finds `brain/vanilla/build-native/tiberiandawn/TiberianDawn.dylib`
(`OPENCNC3D_BRAIN` names another); `tools/win/build-win.sh` builds `cncbrain.exe` for
Windows, which loads `TiberianDawn.dll` from the folder it is run in. `content/` is the two theatre
MIX stubs the shipped game carries (TEMPERAT.MIX sha1 44ffd85aaade, DESERT.MIX
441f8bdbf2b6), `missions/` is `game/missions/`. Default build level 7. The brain must be
built from the commit named in the file, or the comparison is meaningless.

## What the tool hashes, and the two things it deliberately ignores

Each output line is `<tick> <sha256>` over that tick's ENGINE lines only: from `TICK|n` to
the engine's own `OBJDUMP-END` receipt, keeping lines that start with an upper case tag
and `|` (`OBJ|`, `HOUSE|`, `TIB|`, `WALL|`, `VIEW|`, ...) or with `OBJDUMP-`. The last line
`ALL <sha256> ticks=N` chains every per tick hash into one number. Compare `ALL` first; if
it differs, `diff` the two files and the first differing line is the first divergent tick.

It ignores carriage returns, because Windows writes a redirected stdout as CRLF. And it
ignores everything the HOST prints, because the host's state readout after the first and
last tick carries the platform's pointer width in a buffer size, and its `[event]` lines
land wherever the host's stdout buffer happens to flush on Windows, which is not where they
land on the Mac. Both were found the expensive way: the first comparison had
all 20,000 ticks differing, and the engine was identical underneath.

## The result, 3 Sep 2026: PHASE 0 PASSES

    ALL eace4a6ee574b329f54a56e1824977a2c462fcfbf3bb7fbd541b217c231c3307 ticks=20000

Both files above carry that line and are byte identical, so `diff` closes the gate with no
argument. macOS arm64 and 32 bit (i686) Windows, built from commit 0372033, simulate
20,000 ticks of SCB01EA identically at every single tick.

**Lockstep is viable.** The assumption every later phase rests on is now measured rather
than hoped, and the design's own words for it were that this is the one test that can kill
the recommendation. It did not.

### Three false alarms on the way, all worth knowing

The first comparison had every tick differing from tick 1, which is the shape of a
systematic difference rather than a drifting simulation. None of the three causes was the
engine.

1. **Line endings in the capture.** Windows opens stdout in text mode, so a redirected
   capture is CRLF and the macOS one is LF. Hashing raw bytes made every tick disagree.
2. **The host's chatter inside the block.** The host and the DLL do not share one stdout
   buffer on Windows, so the host's own state readout surfaced inside the blocks of ticks
   2 to 10. Only engine lines are hashed now.
3. **A buffer size that carries the platform's pointer width.** The host prints a state
   readout whose buffer is 4080118 bytes on arm64 and 4014574 on i686. Bounding each block
   at the engine's own `OBJDUMP-END` receipt keeps the host's opinion of itself out.

And one ruled out cheaply rather than assumed: **char signedness**, unsigned by default on
arm64 and signed on x86, which is the classic cross platform divergence. A Mac brain
rebuilt with `-fsigned-char` produces a byte identical capture.

Line endings bite twice: these two `.sha256` files are themselves written by Python on each
platform, so the Windows one arrived CRLF and a plain `diff` called all 20,001 lines
different while every hash matched. Both are stored LF.

## What is captured

| File | Built from | Self identical? | Hashed with |
|---|---|---|---|
| `windows-i686-0372033-SCB01EA-20000.sha256` | 32 bit Windows, MSYS2 i686 gcc 16.2, 3 Sep 2026 | yes, two independent runs | the tool as committed now |
| `mac-arm64-0372033-SCB01EA-20000.sha256` | macOS arm64, Apple clang, 3 Sep 2026 | yes, two independent runs | the tool as committed now |
| `mac-arm64-0372033-SCB01EA-tick1.txt` | the Mac's tick 1 block, raw | | (byte identical to the Windows tick 1 engine lines, checked by hand) |
| `mac-arm64-b6d340b92-SCB01EA-20000.sha256` | macOS arm64, Apple clang, the brain as it stands after the 64-bit Windows changes | identical to the capture of the brain before them, so those changes do not move the simulation | the tool as committed now |
| `mac-arm64-a5540f0-SCB01EA-20000.sha256` | macOS arm64, Apple clang, the brain of the combined 64-bit tree (`a5540f046`) | yes, two runs | the tool as committed now |
| `windows-x86_64-38ed6d5ff-SCB01EA-20000.sha256` | 64 bit Windows, `cncbrain.exe` and `TiberianDawn.dll` cross built by `tools/win/build-win.sh` (x86_64-w64-mingw32), run on a Windows 11 PC | one run | the tool as committed now, run on Windows, line endings LF |

The two `0372033` `.sha256` files are byte identical, which is the gate. To re-run it after a brain
change, capture on both platforms from the same commit with the recipe above and `diff`.

## The 64 bit Windows build: PHASE 0 PASSES

Windows builds are now x86_64 only, and the brain's floating point moved from x87 to SSE2
with that switch. The simulation uses none, and the capture says so:
`windows-x86_64-38ed6d5ff-SCB01EA-20000.sha256` (20,000 ticks of SCB01EA, `ALL
0efc7828...`) is byte identical to `mac-arm64-a5540f0-SCB01EA-20000.sha256`. That pair is
the gate now, and the 0372033 files stay as the record of the 32 bit build.

The two halves name different commits of one brain: `git diff a5540f046 38ed6d5ff --
brain/` is empty, so both captures ran the same engine source, compiled by Apple clang for
arm64 and by mingw-w64 gcc for x86_64. Both are also byte identical to
`mac-arm64-b6d340b92-SCB01EA-20000.sha256`, although the Tiberian Dawn engine code
changed between b6d340b92 and a5540f046 (the merge of the player-reported fixes): those
changes do not reach this scenario's 20,000 ticks. The Windows run used the content stubs
of its own play folder (`TEMPERAT.MIX` sha1 8a024d47b83c, `DESERT.MIX` 7869fb9f0420), not
the ones named in the recipe above, and still matched exactly.

After the next change to the engine code, capture both halves again from one commit.
Compare the new Windows capture with
a Mac capture of its own brain, not with the 0372033 files: the engine dump has gained fields since 0372033 (`poweroff`, `primary`, `group` and
`flashme` on `OBJ|`, and `EFX|` lines), so no current brain reproduces those hashes on any
platform.
