#!/bin/sh
# game/gate_controls.sh -- G530 and G531, run by game/gates.sh.
#
# RUN STANDALONE:  sh game/gate_controls.sh [play-folder]
#   The play folder defaults to ./playable beside this checkout and must hold cnc_eyes,
#   TiberianDawn.dylib and ra3d/RedAlert.dylib. Every copy of the game it starts is hidden,
#   so nothing takes the screen. About a minute. Exit status is the number of failed blocks.
#
# Inside game/gates.sh the blocks below run with that suite's RUNDIR, OUT, ok and bad; run
# on their own they define the same three for themselves.

if ! command -v ok >/dev/null 2>&1; then
  GC_HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
  RUNDIR=${1:-"$GC_HERE/../playable"}
  RUNDIR=$(CDPATH= cd -- "$RUNDIR" && pwd) || { echo "FAIL: no play folder at $1" >&2; exit 1; }
  cd "$RUNDIR" || exit 1
  OUT=${OUT:-/dev/null}
  PASS=0; FAIL=0
  ok()  { echo "PASS  $1" | tee -a "$OUT"; PASS=$((PASS+1)); }
  bad() { echo "FAIL  $1" | tee -a "$OUT"; FAIL=$((FAIL+1)); }
  GC_STANDALONE=1
fi
GC_TD="--cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --dir missions/ --content content/ --dist 3000"
GC_RA="--ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dir ra3d/missions/ --dosdata ra3d/audio"
GC_DIR=${TMPDIR:-/tmp}/opencnc3d-gate-controls.$$
rm -rf "$GC_DIR"; mkdir -p "$GC_DIR"

# ---------------------------------------------------------------------------
# G530  THE MODERN MOUSE SCHEME'S LEFT BUTTON SELECTS AND NEVER ORDERS, in every graphics
# mode and both games. The symptom: with the Modern scheme both buttons gave orders, so a
# selection could not be dropped. The scheme is meant to be the StarCraft one: left click
# selects (and on empty ground deselects), left drag is the band, right click orders.
#
# Tiberian Dawn, SCG90EA, once each in Classic, N64 and Enhanced (gfxmode 0, 1, 2):
#   1  Modern left click on the player's tank (57,50) selects it.
#   2  Modern left click on empty ground (56,52) with the tank selected drops the
#      selection, gives no order, and sixty ticks later the tank is still on 57,50.
#   3  the tank selected again, a Modern right click on 52,50 orders it, and it gets there.
#   4  with the tank selected, a Modern left click on an enemy light tank (57,38) is not an
#      attack order (the 1995 left button would attack).
#   5  a Modern left click on the radar with the tank selected moves the camera and is not
#      a radar order; the tank stays selected.
#   6  CLASSIC IS UNCHANGED: the 1995 left click on the same ground orders the tank.
# Red Alert, Allied mission 1, in Classic and Enhanced (it has no N64 mode): 1, 2, 3 with a
# jeep (63,50; ground 60,53; order to 59,52), and 6 with another jeep.
# The verbs name the scheme (`m`); the SDL handler hands the setting to the same functions.
# ---------------------------------------------------------------------------
gc_td_script() {   # gc_td_script FILE MODE
  cat > "$1" <<EOF
tick 20
gfxmode $2
tick 2
cam 54 51
zoom min
deselect
echo G530-1
lgesture cell 57 50 0 0 m
expectsel 1
echo G530-2
lgesture cell 56 52 0 0 m
expectsel 0
tick 60
echo G530-3
lgesture cell 57 50 0 0 m
expectsel 1
seldump
rdown cell 52 50 m
rup
expectsel 1
tick 300
seldump
echo G530-4
cam 56 41
lgesture cell 57 38 0 0 m
tick 2
echo G530-5
cam 54 51
deselect
lgesture cell 52 50 0 0 m
expectsel 1
mmclick 48 44 m
expectsel 1
echo G530-6
cam 54 51
lgesture cell 56 52 0 0
expectsel 1
echo G530-7
quit
EOF
}
gc_ra_script() {   # gc_ra_script FILE MODE
  cat > "$1" <<EOF
tick 30
gfxmode $2
tick 2
cam 62 51
tick 1
deselect
echo G530-1
lgesture cell 63 50 0 0 m
expectsel 1
echo G530-2
lgesture cell 60 53 0 0 m
expectsel 0
tick 40
echo G530-3
lgesture cell 63 50 0 0 m
expectsel 1
seldump
rdown cell 59 52 m
rup
expectsel 1
tick 200
seldump
echo G530-6
deselect
cam 62 51
lgesture cell 64 50 0 0
lgesture cell 61 53 0 0
expectsel 1
echo G530-7
quit
EOF
}
gc_leg() { echo "$1" | sed -n "/^ECHO|G530-$2\$/,/^ECHO|G530-$3\$/p"; }
G530OK=1; G530WHY=""
for gcm in 0 1 2; do
  gc_td_script "$GC_DIR/td$gcm.txt" $gcm
  GCL=$(./cnc_eyes --scen SCG90EA --pack SCG01EA.pack $GC_TD --noshroud --nosound --w 1280 --h 800 \
          --forceradar --script "$GC_DIR/td$gcm.txt" 2>&1); GCRC=$?
  echo "$GCL" | grep -E '^(ECHO|MOUSE|CLICK|SELECT|EXPECTSEL|SEL\|UNIT|GFXMODE|MINIMAP|CAM\|mmclick|SCRIPT\|end)' \
    | sed "s/^/G530 td$gcm /" >> "$OUT"
  L1=$(gc_leg "$GCL" 1 2); L2=$(gc_leg "$GCL" 2 3); L3=$(gc_leg "$GCL" 3 4)
  L4=$(gc_leg "$GCL" 4 5); L5=$(gc_leg "$GCL" 5 6); L6=$(gc_leg "$GCL" 6 7)
  a1=$(echo "$L1" | grep -c '^SELECT|.*|type=MTNK|house=GoodGuy|.*|selected$')
  a2=$(echo "$L2" | grep -c '^SELECT|.*|hit=none|cleared$'); a2o=$(echo "$L2" | grep -c '^CLICK|.*|ORDER$')
  a3s=$(echo "$L3" | grep '^SEL|UNIT|MTNK|' | head -1 | grep -c '|x=57|y=50|')
  a3o=$(echo "$L3" | grep -c '^MOUSE|modern|right|order|')
  a3e=$(echo "$L3" | grep '^SEL|UNIT|MTNK|' | tail -1 | grep -c '|x=52|y=50|')
  a4h=$(echo "$L4" | grep -c '^SELECT|.*|type=LTNK|house=BadGuy|'); a4o=$(echo "$L4" | grep -c '^CLICK|.*|ORDER$')
  a5c=$(echo "$L5" | grep -c '^CAM|mmclick|'); a5o=$(echo "$L5" | grep -c '^MINIMAP|order|')
  a5s=$(echo "$L5" | grep -c '^EXPECTSEL|want=1|have=1|PASS')
  a6=$(echo "$L6" | grep -c '^CLICK|.*|cell=56,52|.*|ORDER$')
  clean=$(echo "$GCL" | grep -c "SCRIPT|end $GC_DIR/td$gcm.txt: .*, 0 failures")
  got="rc=$GCRC sel=$a1 clear=$a2 order=$a2o stayed=$a3s rorder=$a3o arrived=$a3e enemyhit=$a4h enemyorder=$a4o radarcam=$a5c radarorder=$a5o kept=$a5s classic=$a6 clean=$clean"
  [ "$got" = "rc=0 sel=1 clear=1 order=0 stayed=1 rorder=1 arrived=1 enemyhit=1 enemyorder=0 radarcam=1 radarorder=0 kept=2 classic=1 clean=1" ] \
    || { G530OK=0; G530WHY="$G530WHY [Tiberian Dawn gfxmode $gcm: $got]"; }
done
G530RA="Red Alert not staged"
if [ -f ra3d/RedAlert.dylib ] && [ -f ra3d/3d/SCG01EA.pack ]; then
  G530RA="Red Alert:"
  for gcm in 0 2; do
    gc_ra_script "$GC_DIR/ra$gcm.txt" $gcm
    GCL=$(./cnc_eyes $GC_RA --nosound --w 1280 --h 720 --noshroud --scen SCG01EA \
            --pack ra3d/3d/SCG01EA.pack --script "$GC_DIR/ra$gcm.txt" 2>&1); GCRC=$?
    echo "$GCL" | grep -E '^(ECHO|MOUSE|CLICK|SELECT|EXPECTSEL|SEL\|UNIT|GFXMODE|SCRIPT\|end)' \
      | sed "s/^/G530 ra$gcm /" >> "$OUT"
    L1=$(gc_leg "$GCL" 1 2); L2=$(gc_leg "$GCL" 2 3); L3=$(gc_leg "$GCL" 3 6); L6=$(gc_leg "$GCL" 6 7)
    a1=$(echo "$L1" | grep -c '^SELECT|.*|type=JEEP|house=Greece|.*|selected$')
    a2=$(echo "$L2" | grep -c '^SELECT|.*|hit=none|cleared$'); a2o=$(echo "$L2" | grep -c '^CLICK|.*|ORDER$')
    a3s=$(echo "$L3" | grep '^SEL|UNIT|JEEP|' | head -1 | grep -c '|x=63|y=50|')
    a3o=$(echo "$L3" | grep -c '^MOUSE|modern|right|order|')
    a3e=$(echo "$L3" | grep '^SEL|UNIT|JEEP|' | tail -1 | grep -vc '|x=63|y=50|')
    a6=$(echo "$L6" | grep -c '^CLICK|.*|cell=61,53|.*|ORDER$')
    clean=$(echo "$GCL" | grep -c "SCRIPT|end $GC_DIR/ra$gcm.txt: .*, 0 failures")
    got="rc=$GCRC sel=$a1 clear=$a2 order=$a2o stayed=$a3s rorder=$a3o moved=$a3e classic=$a6 clean=$clean"
    G530RA="$G530RA gfxmode $gcm ok;"
    [ "$got" = "rc=0 sel=1 clear=1 order=0 stayed=1 rorder=1 moved=1 classic=1 clean=1" ] \
      || { G530OK=0; G530WHY="$G530WHY [Red Alert gfxmode $gcm: $got]"; }
  done
fi
if [ "$G530OK" = "1" ]; then
  ok "G530 Modern mouse scheme: in Tiberian Dawn's Classic, N64 and Enhanced modes the left button selects the tank, a left click on the ground drops the selection with no order (the tank stays on 57,50), a left click on an enemy is no attack, a left click on the radar moves the camera only, the right button orders the tank to 52,50, and Classic's left click still orders; $G530RA"
else
  bad "G530 Modern mouse scheme:$G530WHY"
fi

# ---------------------------------------------------------------------------
# G531  THE TABBED HUD FILLS A WINDOW BETWEEN ONE AND TWO HOUSINGS TALL, AND THE UI SCALE
# ROW CHANGES IT THERE. The symptom: in a window (or on a 1080-row screen) the tabbed HUD
# stood at its 1x zoom, 693 rows tall, over the top half or two thirds of the window with
# the rest empty, and its UI scale row seemed stuck, because -2x of 1x is still 1x. It
# grows into spare height with build rows, so the next zoom is taken when the 1x housing
# covers less than three quarters of the height and four build rows still fit.
#   a  Tiberian Dawn, Enhanced, at 1920x1017 (a window on a 1080 screen), 1920x1080 and
#      2560x1377 (a window on a 1440 screen): zoom 2, covering at least 0.9 of the height;
#      with the UI scale at -2x the zoom is 1, so the row now changes something.
#   b  the sizes that already filled are unchanged: 1280x720 zoom 1, 1600x900 zoom 1,
#      2560x1440 zoom 2 and 3840x2160 zoom 3, all with the full eight rows.
#   c  N64 with the New HUD turned on in the Settings dialog, 1920x1080: zoom 2. N64's
#      own DOS bar (New HUD off) keeps its zoom of 5.
#   d  Red Alert, Enhanced, 1920x1080: zoom 2, covering at least 0.9 of the height;
#      1280x720 stays at 1.
# Read off the `hudscale` verb, which reports sb_layout's own answer.
# ---------------------------------------------------------------------------
printf 'tick 5\ngfxmode 2\ntick 1\nhudscale\ngfx ui_scale 1\ntick 1\nhudscale\nquit\n' > "$GC_DIR/h.txt"
printf 'tick 5\ngfxmode 1\ntick 1\nhudscale\noptions\noptclick Visuals\nemclick Interface\nemclick New HUD\nemclick OK\nemclick Resume Mission\ntick 2\nhudscale\nquit\n' > "$GC_DIR/hn.txt"
gc_hud() {   # gc_hud W H SCRIPT [ra] -> the HUDSCALE lines, one per line
  if [ "$4" = "ra" ]; then
    ./cnc_eyes $GC_RA --nosound --w "$1" --h "$2" --noshroud --scen SCG01EA \
      --pack ra3d/3d/SCG01EA.pack --script "$3" 2>&1 | grep '^HUDSCALE|'
  else
    ./cnc_eyes --scen SCG01EA --pack SCG01EA.pack $GC_TD --nosound --w "$1" --h "$2" \
      --script "$3" 2>&1 | grep '^HUDSCALE|'
  fi
}
gc_f() { echo "$1" | sed -n "$2p" | sed -n "s/.*|$3=\([0-9.]*\).*/\1/p"; }
G531OK=1; G531WHY=""
for gcs in 1920x1017 1920x1080 2560x1377; do
  H=$(gc_hud ${gcs%x*} ${gcs#*x} "$GC_DIR/h.txt"); echo "$H" | sed "s/^/G531 td $gcs /" >> "$OUT"
  s1=$(gc_f "$H" 1 scale); c1=$(gc_f "$H" 1 cover); e1=$(gc_f "$H" 1 eh); s2=$(gc_f "$H" 2 scale)
  if [ "$e1" != "1" ] || [ "$s1" != "2" ] || [ "$s2" != "1" ] \
     || [ "$(echo "${c1:-0} >= 0.9" | bc)" != "1" ]; then
    G531OK=0; G531WHY="$G531WHY [TD $gcs: tabbed=$e1 zoom=$s1 cover=$c1 (want 1, 2, >=0.9); at -2x zoom=$s2 (want 1)]"
  fi
done
for gcs in 1280x720:1 1600x900:1 2560x1440:2 3840x2160:3; do
  sz=${gcs%:*}; want=${gcs#*:}
  H=$(gc_hud ${sz%x*} ${sz#*x} "$GC_DIR/h.txt"); echo "$H" | sed "s/^/G531 td $sz /" >> "$OUT"
  s1=$(gc_f "$H" 1 scale); r1=$(gc_f "$H" 1 rows)
  [ "$s1" = "$want" ] && [ "$r1" = "8" ] \
    || { G531OK=0; G531WHY="$G531WHY [TD $sz: zoom=$s1 rows=$r1 (want $want and 8, as before)]"; }
done
H=$(gc_hud 1920 1080 "$GC_DIR/hn.txt"); echo "$H" | sed "s/^/G531 n64 1920x1080 /" >> "$OUT"
n0e=$(gc_f "$H" 1 eh); n0d=$(gc_f "$H" 1 dbscale); n1e=$(gc_f "$H" 2 eh); n1s=$(gc_f "$H" 2 scale)
[ "$n0e" = "0" ] && [ "$n0d" = "5" ] && [ "$n1e" = "1" ] && [ "$n1s" = "2" ] \
  || { G531OK=0; G531WHY="$G531WHY [N64 1920x1080: DOS bar tabbed=$n0e zoom=$n0d (want 0, 5); New HUD tabbed=$n1e zoom=$n1s (want 1, 2)]"; }
G531RA="Red Alert not staged"
if [ -f ra3d/RedAlert.dylib ] && [ -f ra3d/3d/SCG01EA.pack ]; then
  H=$(gc_hud 1920 1080 "$GC_DIR/h.txt" ra); echo "$H" | sed "s/^/G531 ra 1920x1080 /" >> "$OUT"
  s1=$(gc_f "$H" 1 scale); c1=$(gc_f "$H" 1 cover); s2=$(gc_f "$H" 2 scale)
  H7=$(gc_hud 1280 720 "$GC_DIR/h.txt" ra); echo "$H7" | sed "s/^/G531 ra 1280x720 /" >> "$OUT"
  s7=$(gc_f "$H7" 1 scale)
  G531RA="Red Alert 1920x1080 zoom $s1 covering $c1, -2x zoom $s2, 1280x720 zoom $s7"
  [ "$s1" = "2" ] && [ "$s2" = "1" ] && [ "$s7" = "1" ] && [ "$(echo "${c1:-0} >= 0.9" | bc)" = "1" ] \
    || { G531OK=0; G531WHY="$G531WHY [$G531RA (want 2, >=0.9, 1, 1)]"; }
fi
if [ "$G531OK" = "1" ]; then
  ok "G531 tabbed HUD zoom: Tiberian Dawn at 1920x1017, 1920x1080 and 2560x1377 draws at zoom 2 and fills at least 0.9 of the height, and -2x takes it to 1; 720, 900, 1440 and 2160 rows keep zoom 1, 1, 2, 3 with eight rows; N64's New HUD follows and its DOS bar keeps zoom 5; $G531RA"
else
  bad "G531 tabbed HUD zoom:$G531WHY"
fi

rm -rf "$GC_DIR"
if [ -n "$GC_STANDALONE" ]; then
  echo "----- gate_controls: $PASS pass, $FAIL fail -----"
  exit $FAIL
fi
