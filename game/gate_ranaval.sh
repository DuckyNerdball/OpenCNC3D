#!/bin/sh
# G498 to G501: Red Alert naval yard placement, Yak/MiG airfield limit, and two 1996 rules
# kept as they were (where a new submarine leaves its pen, Tanya for the Soviets in skirmish).
# G498 to G501, run by game/gates.sh.
#
# Standalone:  sh game/gate_ranaval.sh [play-folder]      (default: ./playable)
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
  GP_STANDALONE=1
fi

GPRA="--ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dir ra3d/missions/ --dosdata ra3d/audio --nosound --w 640 --h 400 --skirmish --seed 4242 --credits 10000 --ai 1 --countries ussr,greece"
# deploy the MCV, then build and place each named building; the Construction Yard is in
# radio contact with a building while it unfolds and refuses the next placement until it
# has, so 160 ticks lie between them
gp_base() {
  printf 'cheatset instant 1\ncheatset money 1\ntick 5\nlclickobj MCV:@me\nactclickobj MCV:@me\ntick 250\n'
  for b in "$@"; do printf 'build %s\nready %s 3000\nstartplace %s\nplace auto\ntick 160\n' "$b" "$b" "$b"; done
}

if [ ! -f ra3d/RedAlert.dylib ] || [ ! -f ra3d/3d/SCM11EA.pack ]; then
  skip "G498-G501: no Red Alert module or no SCM11EA pack in this folder"
else
rm -rf gpranaval && mkdir -p gpranaval

# =====================================================================================
# G498  THE GREEN WATER A SUB PEN IS OFFERED IS WATER IT MAY BE PUT ON.
#
# THE FAULT. A Sub Pen showed green, legal water that refused it when clicked. The
# placement overlay paints the brain's per-cell PassesProximityCheck, which
# DLLExportClass::Passes_Proximity_Check builds as a square ring of Adjacent+1 cells
# around every base building. A placement is judged by DisplayClass::Passes_Proximity_
# Check (HouseClass::Place_Object), and for a building with Adjacent above 1 (Naval Yard
# and Sub Pen, RULES.INI Adjacent=8) that is a different rule: the distance from the
# foundation's top left cell to each base building's centre, less half its size. Measured
# on Island Hoppers as the USSR: 112 of 132 green origins refused, the first at 26,37.
#
# THE TEST. placemap compares the overlay with OpenCNC3D_Proximity_Ok (the routine the
# placement answers) on every origin and prints over= (offered, refused), under= and
# legalover= (whole legal origins refused). All must be 0, 26,37 must read illegal, and
# "place auto" must put the pen down (a SPEN in the dump, out of limbo).
#
# G499  1996 RULE KEPT: a new submarine leaves at the cell left of the pen's
# bottom row. BuildingTypeClass ExitSub (bdata.cpp) lists (0,2) (2,2) (-1,2) (1,2) (3,2)
# from the pen's top left; the three inside the 3x3 foundation are always taken, so the
# first free one is (-1,2), even when that cell is a pocket of water closed off by the
# shore. A new submarine left in a pocket between the pen and the shore is this rule.
# =====================================================================================
{ gp_base POWR
  printf 'build SPEN\nready SPEN 3000\ntick 5\nstartplace SPEN\ntick 3\nplacemap\nhover 26 37\nplace auto\ntick 200\n'
  printf 'build SS\nready SS 3000\ntick 30\nobjdump\nquit\n'; } > gpranaval/subpen.txt
./cnc_eyes $GPRA --scen SCM11EA --pack ra3d/3d/SCM11EA.pack --script gpranaval/subpen.txt > gpranaval/subpen.log 2>&1
GPENG=$(grep -a '^PLACEMAP|engine|SPEN|' gpranaval/subpen.log | head -1)
GPPEN=$(grep -a '^OBJ|BUILDING|SPEN|Multi1|' gpranaval/subpen.log | grep -a '|limbo=0|' | head -1)
if [ -z "$GPENG" ]; then
  bad "G498: the Sub Pen never reached placement mode, or this renderer has no PLACEMAP|engine line"
elif ! echo "$GPENG" | grep -q '|over=0|under=0|legalover=0|'; then
  bad "G498: the overlay and the placement disagree on the Sub Pen: $GPENG"
elif ! grep -aq '^SCRIPT|hover|26,37|legal=0|' gpranaval/subpen.log; then
  bad "G498: 26,37 (refused by the placement) is still offered as legal"
elif [ -z "$GPPEN" ]; then
  bad "G498: place auto did not put a Sub Pen down"
else
  ok "G498: the Sub Pen's green water is exactly the water the placement takes ($GPENG)"
  # x,y of the pen and of the submarine, from the dump
  GPPX=$(echo "$GPPEN" | sed 's/.*|x=\([0-9]*\)|y=\([0-9]*\)|.*/\1/'); GPPY=$(echo "$GPPEN" | sed 's/.*|x=\([0-9]*\)|y=\([0-9]*\)|.*/\2/')
  GPSS=$(grep -a '^OBJ|VESSEL|SS|Multi1|' gpranaval/subpen.log | grep -a '|limbo=0|' | head -1)
  if [ -z "$GPSS" ]; then
    bad "G499: no submarine came out of the pen"
  elif echo "$GPSS" | grep -q "|x=$((GPPX - 1))|y=$((GPPY + 2))|"; then
    ok "G499: the submarine left at the 1996 exit cell, left of the pen's bottom row ($((GPPX - 1)),$((GPPY + 2)))"
  else
    bad "G499: the submarine is not at the 1996 exit cell $((GPPX - 1)),$((GPPY + 2)): $GPSS"
  fi
fi

# =====================================================================================
# G500  NO YAK IS PAID FOR THAT HAS NO AIRFIELD TO LAND ON.
#
# THE FAULT. With one Airfield, a second Yak was built, paid for, and never appeared: it
# sat finished in the factory (limbo=1), because a fixed-wing plane leaves only onto a free
# Airfield (ObjectTypeClass::Who_Can_Build_Me, HouseClass::Place_Object). The 1996 sidebar
# never let it start: it shows a Yak or MiG busy, and refuses it, while every Airfield has
# its plane (HouseClass::Is_Hack_Prevented / Is_No_YakMig). The DLL's sidebar path the
# renderer drives had no such test, and the build queue then started the second Yak by
# itself. The stuck Yak also held the factory, so a Hind queued after it never came.
#
# THE TEST. As the USSR with one Airfield and one Helipad, queue three Yaks and a Hind.
# The engine must build ONE Yak, the Hind must be built past the waiting Yaks, and no
# Yak may sit in limbo. Then a second Airfield goes up and exactly one more Yak must come
# out onto it, with the last still waiting in the queue at no cost.
# =====================================================================================
{ gp_base POWR PROC DOME AFLD HPAD
  printf 'cheatset instant 0\ntick 5\nqueue YAK 3\nqueue HIND 1\ntick 4000\nqueuedump\nobjdump\n'
  printf 'cheatset instant 1\nbuild AFLD\nready AFLD 3000\nstartplace AFLD\nplace auto\ntick 5\ncheatset instant 0\ntick 3000\nqueuedump\nobjdump\nquit\n'; } > gpranaval/yak.txt
./cnc_eyes $GPRA --scen SCM11EA --pack ra3d/3d/SCM11EA.pack --script gpranaval/yak.txt > gpranaval/yak.log 2>&1
# per dump: player Yaks out, player Yaks in limbo, player Hinds out; and per queuedump the entries
GPAIR=$(awk -F'|' '/^OBJDUMP-BEGIN/{n++} /^OBJ\|AIRCRAFT\|/ && $4=="Multi1" {
          l = ($0 ~ /\|limbo=1\|/); if ($3=="YAK") { if (l) yl[n]++; else yo[n]++ } else if ($3=="HIND" && !l) h[n]++ }
        END { for (k = 1; k <= n; k++) printf "%d,%d,%d ", yo[k]+0, yl[k]+0, h[k]+0 }' gpranaval/yak.log)
GPQ=$(grep -a '^SBQUEUEDUMP|' gpranaval/yak.log | sed 's/.*entries=//' | tr '\n' ' ')
if [ "$(grep -ac '^OBJDUMP-END' gpranaval/yak.log)" != "2" ]; then
  bad "G500: the run did not reach both dumps"
elif [ "$GPAIR" != "1,0,2 2,0,2 " ] || [ "$GPQ" != "2 1 " ]; then
  bad "G500: Yaks out,in limbo,Hinds per dump are '$GPAIR' (want '1,0,2 2,0,2 ': the free Hind and the queued one) and the queue holds '$GPQ' (want '2 1 ')"
else
  ok "G500: one Yak per Airfield and none paid for in limbo; the Hind was built past the waiting Yaks, and a new Airfield let exactly one more out"
fi

# =====================================================================================
# G501  1996 RULE KEPT: in skirmish and multiplayer the Soviets may
# train Tanya. RULES.INI [E7] has Owner=allies,soviet and Prerequisite=atek, and
# HouseClass::Can_Build counts either Tech Center as the other outside the campaign
# (Session.Type != GAME_NORMAL). In a Soviet campaign mission no Allied Tech Center is
# owned, so she is not offered there. As the USSR with Barracks and a Soviet Tech Center
# she must be offered.
# =====================================================================================
{ gp_base POWR PROC BARR DOME WEAP APWR STEK; printf 'tick 5\nsbdump\nquit\n'; } > gpranaval/tanya.txt
./cnc_eyes $GPRA --scen SCM11EA --pack ra3d/3d/SCM11EA.pack --script gpranaval/tanya.txt > gpranaval/tanya.log 2>&1
if ! grep -aq '^SBENT|.*|name=STEK|' gpranaval/tanya.log; then
  bad "G501: the Soviet Tech Center never reached the sidebar, so the test did not happen"
elif grep -aq '^SBENT|.*|name=E7|' gpranaval/tanya.log; then
  ok "G501: Tanya is offered to the USSR with a Soviet Tech Center in skirmish, as in 1996 multiplayer"
else
  bad "G501: Tanya is not offered to the USSR with a Soviet Tech Center in skirmish"
fi
fi

if [ -n "${GP_STANDALONE:-}" ]; then
  echo "ranaval: $PASS passed, $FAIL failed"
  [ "$FAIL" = 0 ]
fi
