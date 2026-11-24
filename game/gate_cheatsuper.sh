#!/bin/sh
# G543: the Unlock Superweapons cheat announces each super weapon ready once, not every tick.
# Run by game/gates.sh.
#
# Standalone:  sh game/gate_cheatsuper.sh [play-folder]      (default: ./playable)
# Inside game/gates.sh the blocks run as they are: ok/bad/skip and OUT come from the suite.
# Every run is headless (OPENCNC3D_BACKGROUND=1, a hidden window) and takes under a minute.

if ! command -v ok >/dev/null 2>&1; then
  RUNDIR=${1:-"$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/playable"}
  cd "$RUNDIR" || { echo "FAIL: no play folder at $RUNDIR" >&2; exit 1; }
  OUT=${OUT:-/dev/null}; PASS=0; FAIL=0
  ok()   { echo "PASS  $1"; PASS=$((PASS+1)); }
  bad()  { echo "FAIL  $1"; FAIL=$((FAIL+1)); }
  skip() { echo "SKIP  $1"; }
  OPENCNC3D_BACKGROUND=1; export OPENCNC3D_BACKGROUND
  CS_STANDALONE=1
fi

# =====================================================================================
# G543  RED ALERT'S UNLOCK SUPERWEAPONS CHEAT SAYS EACH WEAPON IS READY ONCE.
#
# THE FAULT. The cheat re-grants the super weapons after every tick, so a spent one is
# ready again at once (cheat_post_advance), and Red Alert's grant export charged each
# weapon on every call. SuperClass::Forced_Charge has the player's own house announce the
# weapon ready every time it runs, ready or not, so with the cheat on the engine asked for
# "atom bomb ready", "chronosphere ready", "iron curtain ready" and "spy plane ready" on
# every tick: about 1200 requests in 300 ticks, heard as one announcement on an endless
# loop. The export now charges only a weapon that is not ready, so each is announced once
# when it is granted and once more each time a spent one comes back.
#
# THE TEST. Allied mission 1 in Red Alert, the cheat turned on at tick 30, 300 ticks
# played with every spoken line logged (--dumpsound). Each of the four ready lines must be
# asked for at least once (the cheat worked) and at most twice, and the whole run must
# ask for fewer than 20 lines. Seen red before the fix: each line asked for about 300
# times, 1201 lines in all.
# =====================================================================================
if [ -f ra3d/RedAlert.dylib ] && [ -f ra3d/3d/SCG01EA.pack ]; then
  rm -rf g543 && mkdir -p g543
  printf 'tick 30\ncheatset super 1\ntick 300\ncheatdump\nquit\n' > g543/super.txt
  ./cnc_eyes --ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dir ra3d/missions/ \
    --dosdata ra3d/audio --scen SCG01EA --pack ra3d/3d/SCG01EA.pack --noshroud \
    --w 640 --h 400 --dumpsound --script g543/super.txt > g543/super.log 2>&1
  csrc=$?
  cat g543/super.log >> "$OUT"
  csmsg=""; csok=1
  for csname in AREADY1 CHRORDY1 IRONRDY1 SPYPLN1; do
    csn=$(grep -c "^SOUND|eva|[0-9]*|$csname|" g543/super.log)
    csmsg="$csmsg $csname=$csn"
    { [ "$csn" -ge 1 ] && [ "$csn" -le 2 ]; } || csok=0
  done
  csall=$(sed -n 's/^SOUND|end|.*|eva=[0-9]*\/\([0-9]*\)$/\1/p' g543/super.log | tail -1)
  cson=$(grep -c '^CHEATDUMP|super|on=1|sent=1|have=1' g543/super.log)
  if [ "$csrc" = 0 ] && [ "$csok" = 1 ] && [ "$cson" = 1 ] && [ -n "$csall" ] && [ "$csall" -lt 20 ]; then
    ok "G543 the Unlock Superweapons cheat says each weapon is ready once: in 300 ticks of Red Alert's Allied mission 1 with the cheat on, ready lines asked for:$csmsg, $csall spoken lines asked for in all (about 300 each and 1201 in all before)"
  else
    bad "G543 the Unlock Superweapons cheat repeats its ready announcement: rc=$csrc cheat-on=$cson(want 1) ready lines asked for:$csmsg (want 1 or 2 each), spoken lines asked for in all=[$csall](want under 20)"
  fi
  rm -rf g543
else
  skip "G543 the Unlock Superweapons cheat's announcements: no Red Alert module in this play folder"
fi

if [ -n "$CS_STANDALONE" ]; then
  echo "----- gate_cheatsuper: $PASS pass, $FAIL fail -----"
  exit "$FAIL"
fi
