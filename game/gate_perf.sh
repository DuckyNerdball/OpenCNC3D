# G516: the graphics mode switch and new buildings in play, run by game/gates.sh.
#
# Standalone, from a built play folder (headless, nothing takes the screen):
#   cd playable && sh ../game/gate_perf.sh
# Inside game/gates.sh the block can be pasted as it is: it only defines ok/bad/OUT when
# they are not defined already.
#
# G516  A REMASTERED BUILDING THAT APPEARS IN PLAY IS READY BEFORE THE
# SWITCH TO ENHANCED, AND IS THE SAME ON THE CARD (game/ebld_mod.h ebld_async_run and
# ebld_async_take, game/ebld_prepare.h ebld_async_start and the in-play step, livery_build_one's
# g_liveryPre in game/cnc_eyes.cpp).
# A building type first seen while a mission played in Classic or N64 had its textures read,
# painted for every house colour and put on the card in the frame that first drew it, which
# was the frame that switched to Enhanced: every type since the mission started at once
# (0.7 s on a Red Alert skirmish after two and a half minutes in Classic). That CPU work now
# runs on a worker thread while the game plays, through the same functions, and the frame
# that uploads takes the results.
# Two hidden runs each game, the same script, with the worker on and off
# (OPENCNC3D_EBLD_ASYNC=0, which is the build before):
#   Red Alert, an eight-player skirmish (SCM01EA, seed 4242), in Classic with a player's
#     loading preparation (`gfxprebuild`): 2400 ticks of the AIs building, then the in-play
#     preparation for 400 frames (`ebldprepplay`, the live loop's per-frame call, each after
#     the worker is done, so the run does not hang on the clock), then Enhanced;
#   Tiberian Dawn, GDI 1 (SCG01EA), the same, with a Communications Center, a Guard Tower
#     and a Silo spawned for the player in play (`spawnobj`).
# Claims, per game:
#   (i)   the worker run puts textures up in play (EBLDPREPPLAY added_mb > 0) and, after the
#         switch to Enhanced, no remastered building's textures go on the card for the types
#         it exercised (Red Alert's refinery and barracks, Tiberian Dawn's three); the control
#         run does load them after the switch (so the scenario is the one the claim is about);
#   (ii)  the switch frame's picture is byte for byte the control's (--shot after the frame);
#   (iii) every texture both runs have on the card reads back the same bytes (ebldtexhash:
#         the colour look, the GDI look, the specular map and every house livery), and the
#         common set holds at least one livery copy (Red Alert) and is not empty.
# PROVEN TO FAIL: the control run (the build before) is (i)'s failure: PROC and BARR (Red
# Alert) and HQ and GTWR (Tiberian Dawn) are put on the card after the switch.
# Measured (information, never gated): the first Enhanced frame of the Red Alert script
# 268 ms against 499 ms; in the live loop after 150 s of Classic play, 77 ms against
# 695 to 707 ms; a texture's upload in play about 10 ms against 60 to 260 ms in one frame.
# NOT COVERED: the remastered vehicles' standing textures (a harvester's, 20 ms on the
# switch frame) and a building file first read at the switch (a dome under construction);
# Windows, where the cross build cannot run here.

if ! command -v ok >/dev/null 2>&1; then
  PASS=0; FAIL=0
  OUT="${OUT:-gates-perf.log}"
  : > "$OUT"
  ok()  { echo "PASS  $1" | tee -a "$OUT"; PASS=$((PASS+1)); }
  bad() { echo "FAIL  $1" | tee -a "$OUT"; FAIL=$((FAIL+1)); }
  GP_PERF_STANDALONE=1
fi
GPP_RA="--ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dir ra3d/missions/ --dosdata ra3d/audio --nosound --w 1280 --h 720"
GPP_TD="--cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --dir missions/ --content content/ --dist 3000 --nosound --w 1280 --h 720"
# The claim is about an ordinary player's machine, so no test memory limit or HUD switch
# left behind by an earlier gate may reach these runs.
unset OPENCNC3D_TEST_COMMIT_ROOM_MB OPENCNC3D_TEST_PAGEFILE_GROWTH_MB OPENCNC3D_HUD OPENCNC3D_FRAMEPROF
rm -rf gp_perf && mkdir -p gp_perf

for GPP_K in 1 0; do
  { echo "gfxmode 0"; echo "gfxprebuild"; echo "reveal 1"; echo "tick 2400"; echo "ebldprepplay 400"
    echo "echo GPPERF|switch"; echo "gfxmode 2"; echo "frames 1"; echo "fprofreport"
    echo "shot gp_perf/ra_$GPP_K.png"
    echo "ebldtexhash PROC"; echo "ebldtexhash BARR"; echo "quit"; } > gp_perf/ra_$GPP_K.txt
  { echo "gfxmode 0"; echo "gfxprebuild"; echo "tick 3"
    echo "spawnobj HQ 50 51"; echo "spawnobj GTWR 52 53"; echo "spawnobj SILO 48 53"; echo "tick 3"
    echo "ebldprepplay 200"
    echo "echo GPPERF|switch"; echo "gfxmode 2"; echo "frames 1"; echo "fprofreport"
    echo "shot gp_perf/td_$GPP_K.png"
    echo "ebldtexhash HQ"; echo "ebldtexhash GTWR"; echo "ebldtexhash SILO"; echo "quit"; } > gp_perf/td_$GPP_K.txt
  OPENCNC3D_EBLD_ASYNC=$GPP_K OPENCNC3D_FRAMEPROF=1 ./cnc_eyes $GPP_RA --scen SCM01EA \
    --pack ra3d/3d/SCM01EA.pack --skirmish --seed 4242 --ai 7 --script gp_perf/ra_$GPP_K.txt \
    > gp_perf/ra_$GPP_K.log 2>&1
  echo "rc=$?" >> gp_perf/ra_$GPP_K.log
  OPENCNC3D_EBLD_ASYNC=$GPP_K OPENCNC3D_FRAMEPROF=1 ./cnc_eyes $GPP_TD --seed 7 --scen SCG01EA \
    --pack SCG01EA.pack --script gp_perf/td_$GPP_K.txt > gp_perf/td_$GPP_K.log 2>&1
  echo "rc=$?" >> gp_perf/td_$GPP_K.log
done
GPP_V=$(python3 - <<'GPPEOF' 2>&1
import re, os
msgs, bad = [], []
for game, types in (('ra', ('PROC', 'BARR')), ('td', ('HQ', 'GTWR'))):
    logs = {}
    for k in ('1', '0'):
        p = 'gp_perf/%s_%s.log' % (game, k)
        logs[k] = open(p, errors='replace').read().splitlines() if os.path.exists(p) else []
    def after(L):
        i = next((n for n, l in enumerate(L) if l.startswith('ECHO|GPPERF|switch')), None)
        return None if i is None else L[i:]
    A, C = after(logs['1']), after(logs['0'])
    if A is None or C is None or 'rc=0' not in logs['1'] or 'rc=0' not in logs['0']:
        bad.append('%s: a run failed or never reached the switch' % game); continue
    added = [float(m.group(1)) for l in logs['1'] for m in [re.search(r'^EBLDPREPPLAY\|.*added_mb=([\d.]+)\|async=1', l)] if m]
    if not added or added[0] <= 0.0:
        bad.append('%s: the worker run put nothing on the card in play (%s)' % (game, added)); continue
    rx = re.compile(r'^ebld: (%s) textures on the card' % '|'.join(types))
    late = [l for l in A if rx.search(l)]
    ctl = [l for l in C if rx.search(l)]
    if late:
        bad.append('%s: %d texture loads after the switch with the worker (first: %s)' % (game, len(late), late[0][:90]))
    if not ctl:
        bad.append('%s: the control run loaded nothing after the switch either: the scenario did not exercise the claim' % game)
    a, c = 'gp_perf/%s_1.png' % game, 'gp_perf/%s_0.png' % game
    if not (os.path.exists(a) and os.path.exists(c)) or open(a, 'rb').read() != open(c, 'rb').read():
        bad.append('%s: the picture after the switch differs from the control (or was not written)' % game)
    def hashes(L):
        h = {}
        for l in L:
            m = re.match(r'^EBLDTEXHASH\|(\w+)\|(\d+)\|(\w+)\|(\d+x\d+)\|([0-9a-f]{16})$', l)
            if m: h[m.group(1, 2, 3, 4)] = m.group(5)
        return h
    ha, hc = hashes(logs['1']), hashes(logs['0'])
    common = sorted(set(ha) & set(hc))
    diff = [k for k in common if ha[k] != hc[k]]
    liveries = [k for k in common if k[2].startswith('extra')]
    if not common: bad.append('%s: no texture on the card in both runs to compare' % game)
    elif diff: bad.append('%s: %d of %d textures read back differently (first %s)' % (game, len(diff), len(common), '|'.join(diff[0])))
    elif game == 'ra' and not liveries: bad.append('ra: no house livery among the textures compared')
    first = {k: [float(m.group(1)) for l in logs[k] for m in [re.search(r'^FRAMEPROF\|frames=1\|mean=([\d.]+)', l)] if m] for k in ('1', '0')}
    msgs.append('%s: %.1f MB up in play, none of %s loaded after the switch (control: %d loads), picture identical, %d textures the same bytes (%d liveries); first Enhanced frame %s ms against %s' % (
        game, added[0], '/'.join(types), len(ctl), len(common), len(liveries),
        '%.0f' % first['1'][0] if first['1'] else '?', '%.0f' % first['0'][0] if first['0'] else '?'))
print(('0 ' + '; '.join(bad)) if bad else ('1 ' + '; '.join(msgs)))
GPPEOF
)
case "$GPP_V" in
  1\ *) ok "G516 a building that appears in play is ready before the switch to Enhanced, and the same on the card: ${GPP_V#1 }" ;;
  *)    bad "G516 ${GPP_V#0 }" ;;
esac

if [ -n "$GP_PERF_STANDALONE" ]; then
  echo "perf gates: $PASS passed, $FAIL failed"
  [ "$FAIL" = "0" ]
fi
