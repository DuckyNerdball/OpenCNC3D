#!/bin/sh
# G535: a save never goes over another one unless the player picks it, both games.
# Run by game/gates.sh.
#
# Standalone:  sh game/gate_saves.sh [play-folder]      (default: ./playable)
# Inside game/gates.sh the blocks run as they are: ok/bad/skip and OUT come from the suite.
# Every run is headless (OPENCNC3D_BACKGROUND=1, a hidden window) and takes about a minute.

if ! command -v ok >/dev/null 2>&1; then
  RUNDIR=${1:-"$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/playable"}
  cd "$RUNDIR" || { echo "FAIL: no play folder at $RUNDIR" >&2; exit 1; }
  OUT=${OUT:-/dev/null}; PASS=0; FAIL=0
  ok()   { echo "PASS  $1"; PASS=$((PASS+1)); }
  bad()  { echo "FAIL  $1"; FAIL=$((FAIL+1)); }
  skip() { echo "SKIP  $1"; }
  OPENCNC3D_BACKGROUND=1; export OPENCNC3D_BACKGROUND
  GS_STANDALONE=1
fi

# =====================================================================================
# G535  EVERY SAVE GOES TO A SLOT OF ITS OWN UNTIL THE PLAYER PICKS ONE TO REPLACE.
#
# THE FAULT. The slot index held sixteen records. With sixteen saves made, the Save
# Mission dialog had no [EMPTY SLOT] row left and opened on the newest save, so the next
# Save went over the last save without the player choosing it, and one save per mission
# could not be kept. 1995 sets no such limit (loaddlg.cpp:612-680 lists every SAVEGAME.*
# and numbers the empty slot past them).
#
# THE TEST, per game (Tiberian Dawn's GDI 1, and Red Alert's Allied 1 when the module is
# staged), through the pause dialog's own buttons:
#   a  seventeen saves in a row, each with Save Mission and Save and nothing else, land in
#      seventeen different slots, 0 to 16, and each dialog opened on [EMPTY SLOT];
#   b  with every slot of the index taken (one real save copied into all of them), Save
#      Mission opens with no row chosen and Save grey; pressing Save writes nothing; a
#      picked row is written over, and only that one. The Enhanced dialog is pictured in
#      that state (g535/<game>_full.png) with its line saying every slot is in use.
# On the build with sixteen records, leg a's seventeenth save goes to slot 15 again.
# =====================================================================================
GSBASE="--cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --dir missions/ --content content/ --dist 3000"
GSTD="--scen SCG01EA --pack SCG01EA.pack $GSBASE --nosound --w 1280 --h 720"
GSRA="--ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dir ra3d/missions/ --dosdata ra3d/audio --nosound --w 1280 --h 720 --scen SCG01EA --pack ra3d/3d/SCG01EA.pack"
rm -rf g535 && mkdir -p g535

# gs_fill DIR: every record of the index a copy of slot 0, each with its own number and
# description, and the payload copied beside it; prints how many records it wrote
gs_fill() {
  python3 - "$1" <<'PY'
import os, shutil, struct, sys, time
d = sys.argv[1]
idx = open(os.path.join(d, 'OPENCNC.IDX'), 'rb').read()
rec = bytearray(idx[0:128])
assert struct.unpack('<I', rec[:4])[0] == 0x53443343, 'slot 0 is not a record'
src = os.path.join(d, 'savegame.000')
n = 100
out = bytearray()
now = time.time()
for s in range(n):
    r = bytearray(rec)
    r[8:12] = struct.pack('<I', s)
    r[16:60] = (b'kept %d' % s).ljust(44, b'\0')
    out += r
    if s:
        shutil.copyfile(src, os.path.join(d, 'savegame.%03d' % s))
    p = os.path.join(d, 'savegame.%03d' % s)
    os.utime(p, (now - 1000 + s, now - 1000 + s))   # slot 99 newest
open(os.path.join(d, 'OPENCNC.IDX'), 'wb').write(bytes(out))
print('G535FILL|records=%d' % n)
PY
}

# gs_game NAME ARGS: legs a and b for one game; sets GSOK and GSM
gs_game() {
  gn="$1"; shift
  mkdir -p "g535/$gn/a" "g535/$gn/b"
  { printf 'tick 20\n'
    i=0; while [ $i -lt 17 ]; do printf 'options\noptclick Save Mission\noptclick Save\ntick 2\n'; i=$((i+1)); done
    printf 'slots\nquit\n'; } > "g535/$gn/a.txt"
  ./cnc_eyes "$@" --savedir "g535/$gn/a" --script "g535/$gn/a.txt" > "g535/$gn/a.log" 2>&1
  gsrc=$?
  cat "g535/$gn/a.log" >> "$OUT"
  gsslots=$(sed -n 's/^SAVE|slot=\([0-9]*\)|.*/\1/p' "g535/$gn/a.log" | paste -sd' ' -)
  gsdistinct=$(sed -n 's/^SAVE|slot=\([0-9]*\)|.*/\1/p' "g535/$gn/a.log" | sort -un | wc -l | tr -d ' ')
  gsempty=$(grep -c '^SLOTS|row|0|slot=[0-9]*|\[EMPTY SLOT\]$' "g535/$gn/a.log")
  # leg b: the index full
  cp "g535/$gn/a/savegame.000" "g535/$gn/b/" && cp "g535/$gn/a/OPENCNC.IDX" "g535/$gn/b/" \
    && gs_fill "g535/$gn/b" >> "$OUT" 2>&1
  printf 'tick 10\noptions\noptclick Save Mission\noptclick Save\noptrow 3\noptclick Save\nslots\nquit\n' > "g535/$gn/b.txt"
  ./cnc_eyes "$@" --savedir "g535/$gn/b" --script "g535/$gn/b.txt" > "g535/$gn/b.log" 2>&1
  gsrcb=$?
  cat "g535/$gn/b.log" >> "$OUT"
  gsfullopen=$(grep -c '^SLOTS|open|mode=1|rows=100|sel=-1|top=0|.*|ok-disabled=1$' "g535/$gn/b.log")
  gsfullempty=$(grep -c '\[EMPTY SLOT\]' "g535/$gn/b.log")
  gspick=$(grep -c '^OPTIONS|script|row|3|sel=3|descr=kept 96$' "g535/$gn/b.log")
  gssaves=$(grep '^SAVE|slot=' "g535/$gn/b.log" | sed 's/|bytes=.*//' | paste -sd' ' -)
  gsafter=$(grep -c '^SLOT|' "g535/$gn/b.log")
  # the Enhanced dialog in the full state, for the eye
  printf 'tick 10\noptions\nemclick Save Mission\ntick 2\nshot g535/%s_full.png\nquit\n' "$gn" > "g535/$gn/c.txt"
  ( unset OPENCNC3D_MENU_1995; ./cnc_eyes "$@" --savedir "g535/$gn/b" --script "g535/$gn/c.txt" ) > "g535/$gn/c.log" 2>&1
  cat "g535/$gn/c.log" >> "$OUT"
  gsshot=0; [ -s "g535/${gn}_full.png" ] && gsshot=1
  GSOK=0
  if [ "$gsrc" = 0 ] && [ "$gsrcb" = 0 ] \
     && [ "$gsslots" = "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16" ] && [ "$gsdistinct" = 17 ] \
     && [ "$gsempty" = 17 ] && [ "$gsfullopen" = 2 ] && [ "$gsfullempty" = 0 ] \
     && [ "$gspick" = 1 ] && [ "$gssaves" = "SAVE|slot=96|kept 96" ] && [ "$gsafter" = 100 ] \
     && [ "$gsshot" = 1 ]; then
    GSOK=1
    GSM="$gn: seventeen saves in slots 0-16; a full index opened with no row and Save grey, wrote nothing until row 3 was picked, then only slot 96"
  else
    GSM="$gn: rc=$gsrc/$gsrcb slots=[$gsslots](want 0..16) distinct=$gsdistinct(want 17) opened-on-empty=$gsempty(want 17) full-open=$gsfullopen(want 2, on opening and after the grey Save: rows=100 sel=-1 Save grey) empty-row-when-full=$gsfullempty(want 0) pick=$gspick saves=[$gssaves](want only SAVE|slot=96|kept 96) slots-after=$gsafter(want 100) picture=$gsshot"
  fi
}

gs_game td $GSTD
G535TD=$GSOK; G535M="$GSM"
G535RA=1
if [ -f ra3d/RedAlert.dylib ] && [ -d ra3d/missions ] && [ -f ra3d/3d/SCG01EA.pack ]; then
  gs_game ra $GSRA
  G535RA=$GSOK; G535M="$G535M; $GSM"
else
  G535M="$G535M; Red Alert: no module with Allied 1 staged in this folder, not run"
fi
if [ "$G535TD" = 1 ] && [ "$G535RA" = 1 ]; then
  ok "G535 a save goes to a slot of its own until the player picks one to replace: $G535M"
else
  bad "G535 save slots: $G535M"
fi

if [ -n "$GS_STANDALONE" ]; then
  echo "----- gate_saves: $PASS pass, $FAIL fail -----"
  exit "$FAIL"
fi
