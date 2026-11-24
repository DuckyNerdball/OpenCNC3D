#!/bin/sh
# G536: an attack ordered on an aircraft standing on its helipad goes to the aircraft.
# Run by game/gates.sh.
#
# Standalone:  sh game/gate_landedair.sh [play-folder]      (default: ./playable)
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
  GL_STANDALONE=1
fi

# =====================================================================================
# G536  A CLICK ON A HELICOPTER PARKED ON ITS PAD ATTACKS THE HELICOPTER, NOT THE PAD.
#
# THE FAULT. The renderer aimed every order at the centre of the picked object's cell, and
# the engine resolves an order with MapClass::Close_Object, which counts a building as
# standing at the centre of each of its cells. A helicopter parks on the corner its pad's
# four cells share, so the pad was always nearer: the order became an attack on the pad.
# A shell's splash still reached the helicopter, but a bite, a bullet or a gore did not,
# so in the last Funpark mission no dinosaur could hurt a Nod Apache on its pad. A landed
# aircraft is now aimed at its own centre (engine_pixels_for_object), where a click on its
# sprite lands in 1995.
#
# THE TEST. Funpark 5 (SCJ05EA): a T-Rex is put on each of the two cells just north of the
# pad at 51,44 and ordered, through the click path, to attack the Apache standing on it.
# The T-Rex's target must be the Apache's own cell, the Apache must lose strength while
# it is still on the ground, and the pad must not. On the build that aimed at the cell
# centre the T-Rex's target was the pad and its bite took 116 off the pad and nothing off
# the Apache.
#
# WHAT IS 1995 AND KEPT (not tested here, and not changed): a bite is no anti-aircraft
# weapon, so once the Apache lifts off no dinosaur can touch it (TechnoClass::Can_Fire,
# td techno.cpp), and a computer Apache with ammunition lifts off as soon as it picks a
# target (AircraftClass::Mission_Guard, td aircraft.cpp). A T-Rex beside the pad but not
# north of it stays out of reach of the Apache in the middle of the pad: its bite reaches
# 1.5 cells (WEAPON_TREX, td const.cpp) and FootClass::Approach_Target finds no cell in
# range to walk to.
# =====================================================================================
GLBASE="--cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --dir missions/ --content content/ --dist 3000"
rm -rf g536 && mkdir -p g536
G536N=0; G536M=""
for gl in "52 43" "51 43"; do
  gn=$(echo "$gl" | tr ' ' '_')
  { printf 'tick 5\nspawnobj TREX %s\ntick 1\nlook HELI:BadGuy 2\ntick 1\nlclickobj TREX:Multi4 1\nselect\nactclickobj HELI:BadGuy 2\n' "$gl"
    i=0; while [ $i -lt 40 ]; do printf 'tick 1\nobjdump\n'; i=$((i+1)); done
    printf 'quit\n'; } > "g536/$gn.txt"
  ./cnc_eyes --scen SCJ05EA --pack SCG01EA.pack $GLBASE --reskin --noshroud --nosound --w 640 --h 400 \
    --script "g536/$gn.txt" > "g536/$gn.log" 2>&1
  glrc=$?
  cat "g536/$gn.log" >> "$OUT"
  # one line per dump: the Apache's cell, strength and height, the pad's strength, the T-Rex's target
  awk -F'|' '
    function f(name,   i) { for (i = 1; i <= NF; i++) if (index($i, name "=") == 1) return substr($i, length(name) + 2); return "" }
    /^OBJ\|AIRCRAFT\|HELI\|BadGuy\|/ && f("id") == "2" { hc = f("newcell"); hs = f("str"); ha = f("alt") }
    /^OBJ\|BUILDING\|HPAD\|BadGuy\|/ && f("x") == "51" && f("y") == "44" { ps = f("str") }
    /^OBJ\|UNIT\|TREX\|Multi4\|/ && f("id") == "44" { tt = f("tar") }
    /^OBJDUMP-END/ { print hc, hs, ha, ps, tt }' "g536/$gn.log" > "g536/$gn.rows"
  glorder=$(grep -c '^ORDER|.*|target=HELI|probe=5|ATTACK$' "g536/$gn.log")
  glsel=$(grep -c '^SELECT|1 selected: TREX#44@' "g536/$gn.log")
  # the T-Rex aims at the Apache's own cell on every dump after the order
  gltar=$(awk '$5 != $1 && $5 != "" { bad++ } END { print bad + 0 }' "g536/$gn.rows")
  # the Apache hurt while still on the ground
  glhit=$(awk '$2 < 125 && $3 == 0 { n++ } END { print n + 0 }' "g536/$gn.rows")
  glpad=$(awk '$4 != 800 { n++ } END { print n + 0 }' "g536/$gn.rows")
  gllast=$(tail -1 "g536/$gn.rows")
  if [ "$glrc" = 0 ] && [ "$glorder" = 1 ] && [ "$glsel" = 1 ] && [ "$gltar" = 0 ] \
     && [ "$glhit" -ge 1 ] && [ "$glpad" = 0 ]; then
    G536N=$((G536N + 1))
    G536M="$G536M T-Rex at $gl: Apache hurt on the ground on $glhit dumps, pad untouched (last: apache-cell,str,alt,pad,trex-target = $gllast);"
  else
    G536M="$G536M T-Rex at $gl: rc=$glrc order=$glorder(want 1 ATTACK on HELI) selected=$glsel(want 1) dumps-aimed-elsewhere=$gltar(want 0) hurt-on-ground=$glhit(want >=1) pad-hurt=$glpad(want 0) last=[$gllast];"
  fi
done
if [ "$G536N" = 2 ]; then
  ok "G536 an attack on a helicopter standing on its pad goes to the helicopter, Funpark 5:$G536M"
else
  bad "G536 landed helicopter on its pad: $G536N of 2 runs right:$G536M"
fi

if [ -n "$GL_STANDALONE" ]; then
  echo "----- gate_landedair: $PASS pass, $FAIL fail -----"
  exit "$FAIL"
fi
