# G507 to G515: unit targeting and combat, both games, run by game/gates.sh.
#
# Every block reads the engine's own object
# dumps through tools/regressions/combat.py, which writes one small flat mission per
# fixture (SCG88EA in a scratch folder) and its script.
#
# STANDALONE: run from inside a play folder that has TiberianDawn.dylib, SCG01EA.pack,
# ra3d/RedAlert.dylib and ra3d/3d/SCG01EA.pack staged:
#
#   cd playable && sh ../game/gate_combat.sh
#
# It defines ok/bad/skip/gbegin/grun itself when game/gates.sh has not, prints one PASS
# or FAIL line per block and exits non-zero if any block failed. Every run is headless
# (--script); nothing raises a window. The whole file takes about three minutes.
#
# WHAT THESE BLOCKS ARE. None of them proves a fix: every behaviour measured here was
# found to be the 1995 or 1996 engine's own, or already right in v0.9.11, when the
# reports were investigated. They pin that behaviour so that a later change cannot
# quietly break it, and three INFO lines print, without failing, the measurements of two
# engine faults that are original and await a decision (a Red Alert attack order lost
# by a vehicle that has just been given a move, and Tiberian Dawn's defeated-house
# explosion that destroys the winner's vehicles beside it), and of artillery against a
# moving target.

if ! command -v ok >/dev/null 2>&1; then
  OUT=${OUT:-/dev/null}
  PASS=0; FAIL=0; SKIPPED=0
  ok()  { echo "PASS  $1"; PASS=$((PASS+1)); }
  bad() { echo "FAIL  $1"; FAIL=$((FAIL+1)); }
  skip() { echo "SKIP  $1"; SKIPPED=$((SKIPPED+1)); }
  gbegin() { GRC=0; }
  grun() {
    ge_log="$1"; shift
    ./cnc_eyes "$@" >"$ge_log" 2>&1; ge_rc=$?
    [ "$ge_rc" = "0" ] || GRC="$ge_rc"
    return "$ge_rc"
  }
  CMB_STANDALONE=1
  GATEDIR="$(cd "$(dirname "$0")" && pwd)"
fi

CMB_PY="$GATEDIR/../tools/regressions/combat.py"
CMB_TD="--cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --content content/ --nosound --w 1280 --h 720 --scen SCG88EA --pack SCG01EA.pack --noshroud --reskin"
CMB_RA="--ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dosdata ra3d/audio --nosound --w 1280 --h 720 --scen SCG88EA --pack ra3d/3d/SCG01EA.pack --noshroud --seed 7"

# cmb_run GAME FIXTURE [GFXMODE] [extra flags]: write the fixture, run it, leave the log at
# gcmb/GAME-FIXTURE.log and the measurement line in CMB_M (its last word is pass or fail).
cmb_run() {
  cmb_g=$1; cmb_f=$2; cmb_m=${3:-1}; shift 3 2>/dev/null
  cmb_d="gcmb/$cmb_g-$cmb_f"
  rm -rf "$cmb_d"
  python3 "$CMB_PY" write "$cmb_d" "$cmb_g" "$cmb_f" "$cmb_m"
  if [ "$cmb_g" = td ]; then cmb_a="$CMB_TD"; else cmb_a="$CMB_RA"; fi
  grun "gcmb/$cmb_g-$cmb_f.log" $cmb_a --savedir "$cmb_d" --gfxsave "$cmb_d/private.cfg" \
       --dir "$cmb_d/" --script "$cmb_d/script.txt" "$@"
  CMB_M=$(python3 "$CMB_PY" measure "gcmb/$cmb_g-$cmb_f.log" "$cmb_g" "$cmb_f" 2>&1)
}
# cmb_all GAME:FIXTURE... : run each, collect "game fixture [measurement]" into CMB_V and
# clear CMB_OK if any measurement failed.
cmb_all() {
  CMB_OK=1; CMB_V=""
  for cmb_x in "$@"; do
    cmb_run "${cmb_x%%:*}" "${cmb_x#*:}" 1
    CMB_V="$CMB_V; ${cmb_x%%:*} ${cmb_x#*:} [$CMB_M]"
    [ "${CMB_M##* }" = "pass" ] || CMB_OK=0
  done
}

if [ ! -f "$CMB_PY" ] || [ ! -f TiberianDawn.dylib ] || [ ! -f SCG01EA.pack ] \
   || [ ! -f ra3d/RedAlert.dylib ] || [ ! -f ra3d/3d/SCG01EA.pack ]; then
  skip "G507-G515 combat fixtures: tools/regressions/combat.py, SCG01EA.pack or Red Alert's ra3d/3d/SCG01EA.pack is not staged here"
else
mkdir -p gcmb

# =====================================================================================
# G507 A GROUP'S ATTACK-MOVE THROUGH A FIELD OF ENEMIES (both games).
# Four tanks attack-move 38 cells through riflemen, a rocket soldier and light tanks on
# guard either side of the route. Wanted: every enemy that came within three cells of a
# tank (inside its sight, so the player had seen it) is dead at the end, and all four
# tanks end within three cells of the clicked cell on Area Guard. Enemies farther off the
# route that no tank ever saw are left alone in both games, by the engine's own rule:
# a player's unit only takes as a target an object the player has seen
# (TechnoClass::Evaluate_Object), and a Tiberian Dawn Medium Tank sees 3 cells and
# shoots 4.75, so an unseen enemy at its maximum range is not engaged until something
# reveals it. That is the "not at max range" half of the attack-move report.
# =====================================================================================
gbegin
cmb_all td:amove-field ra:amove-field
if [ "$GRC" != "0" ]; then bad "G507 attack-move through a field: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G507 attack-move fights every enemy it meets and ends on the click$CMB_V"
else bad "G507 attack-move through a field$CMB_V"; fi

# =====================================================================================
# G508 FOUR TANKS ORDERED AS ONE SELECTION TO ATTACK ONE ENEMY (both games).
# Wanted: all four hold the target on the tick after the order and still five dumps
# (25 ticks) later, and the enemy dies. Both games are also run with the tanks given a
# move order first (group-attack-moving).
# The Red Alert moving case is the fix: the attack event queues MISSION_ATTACK and clears
# the destination, the vehicle's next DriveClass::Start_Of_Move sees no destination while
# still on MISSION_MOVE and calls UnitClass::Enter_Idle_Mode, which in the 1996 engine
# queued MISSION_GUARD over the waiting attack, and the guard scan dropped the target out
# of range. Enter_Idle_Mode now keeps a queued attack with a live target, as Tiberian
# Dawn's does. PROVEN TO FAIL on the engine before the fix: ra group-attack-moving held
# 3 of 4 (still_holding_after_25_ticks=3).
# =====================================================================================
gbegin
cmb_all td:group-attack ra:group-attack td:group-attack-moving ra:group-attack-moving
if [ "$GRC" != "0" ]; then bad "G508 group attack: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G508 every tank of a selection takes the attack order, also given while moving$CMB_V"
else bad "G508 group attack$CMB_V"; fi

# =====================================================================================
# G509 A TANK FORCE-MOVED ONTO AN ENEMY RIFLEMAN DRIVES OVER HIM (both games).
# The force-move (alt) click onto the soldier: wanted, he is gone and was never shot.
# A plain click on a visible enemy soldier is an attack order and the player's tank
# shoots him, as in 1995 and 1996: only the computer's vehicles drive at infantry they
# attack (DriveClass::Approach_Target's !House->IsHuman arm in Tiberian Dawn, Red Alert's
# PlayerAutoCrush rule, off by default).
# =====================================================================================
gbegin
cmb_all td:crush-force ra:crush-force
if [ "$GRC" != "0" ]; then bad "G509: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G509 a force-moved tank crushes the enemy soldier$CMB_V"
else bad "G509 a force-moved tank does not crush$CMB_V"; fi

# =====================================================================================
# G510 NOTHING THAT CANNOT HIT AN AIRCRAFT AIMS AT ONE (both games, both sides).
# air-retaliate: the player's aircraft strikes a computer tank beside a rifleman, a rocket
# soldier and an artillery piece. air-defend: the computer's aircraft hunts the player's
# tank, artillery (and V2 launcher), rifleman and rocket soldier. Wanted: no tank,
# artillery, launcher or rifleman ever has an aircraft as its target, and the rocket
# soldiers do (the control: the measurement can see an aircraft target at all).
# =====================================================================================
gbegin
cmb_all td:air-retaliate ra:air-retaliate td:air-defend ra:air-defend
if [ "$GRC" != "0" ]; then bad "G510: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G510 only anti-aircraft weapons take an aircraft as a target$CMB_V"
else bad "G510 a unit that cannot hit aircraft aims at one$CMB_V"; fi

# =====================================================================================
# G511 EVA STOPS SAYING LOW POWER ONCE POWER IS RESTORED (both games).
# A base with no power plant runs short for about two and a half minutes, then the player
# switches its three consumers off with the power button. Wanted: low power was said
# while short, never after, over about four and a half more minutes, and the final
# reading is not short. --dumpsound prints every EVA line the engine raises.
# =====================================================================================
gbegin
CMB_OK=1; CMB_V=""
for cmb_g in td ra; do
  cmb_run $cmb_g lowpower 1 --dumpsound
  CMB_V="$CMB_V; $cmb_g lowpower [$CMB_M]"
  [ "${CMB_M##* }" = "pass" ] || CMB_OK=0
done
if [ "$GRC" != "0" ]; then bad "G511: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G511 low power is said while short and never after power is restored$CMB_V"
else bad "G511$CMB_V"; fi

# =====================================================================================
# G512 A CAPTURED ENEMY VEHICLE FACTORY MADE PRIMARY DELIVERS THE VEHICLE.
# Tiberian Dawn: GDI captures a Nod Airstrip with an engineer and makes it primary, and an
# APC is flown in by the cargo plane and set down beside it (capture-apc); GDI makes its
# own Weapons Factory primary instead and the APC leaves that (capture-apc-weap); Nod
# captures a GDI Weapons Factory, makes it primary and the APC leaves it (capture-apc-nod).
# Red Alert: the Soviets capture an Allied War Factory (an engineer captures only a
# building in the red, so it starts damaged), make it primary and an Allied light tank
# leaves it (capture-apc-ra; the Allied APC also needs the Allied barracks).
# The primary gesture (a second click on a selected factory) only exists with two
# factories of a kind (BuildingClass::What_Action), and a captured factory counts only
# once the engineer is inside, so the script waits for the capture before clicking.
# =====================================================================================
gbegin
cmb_all td:capture-apc td:capture-apc-weap td:capture-apc-nod ra:capture-apc-ra
if [ "$GRC" != "0" ]; then bad "G512: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G512 a vehicle comes out of the primary factory, captured or not$CMB_V"
else bad "G512$CMB_V"; fi

# =====================================================================================
# G513 ENEMY SOLDIERS SIDE BY SIDE FIGHT WITHOUT ORDERS (both games).
# Riflemen of both sides on guard in neighbouring cells: wanted, both are hit. A unit on
# guard only engages what is inside its weapon range (Mission_Guard scans THREAT_RANGE),
# so in Tiberian Dawn two riflemen exactly two cells apart do not: the M16 reaches 0x200
# leptons, measured from the firing point, which is just short of the 512 between them.
# =====================================================================================
gbegin
cmb_all td:neighbours ra:neighbours
if [ "$GRC" != "0" ]; then bad "G513: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G513 neighbouring enemy soldiers fight on their own$CMB_V"
else bad "G513$CMB_V"; fi

# =====================================================================================
# G514 ALLIED CIVILIANS ARE NOT THE PLAYER'S TO COMMAND (both games).
# Neutral civilians allied to the player: a click selects one (it can be inspected, as
# any object can in 1995 and 1996) and a click on the ground does not move it; a plain
# click on one with a rifleman selected never makes the rifleman fire. A force-fire
# (ctrl) still can, as it can on anything in both original games.
# =====================================================================================
gbegin
cmb_all td:civilians ra:civilians
if [ "$GRC" != "0" ]; then bad "G514: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G514 allied civilians are not attacked by a plain click$CMB_V"
else bad "G514$CMB_V"; fi

# =====================================================================================
# G515 A DESTROYED HOUSE'S COLLAPSE (Red Alert asserted, Tiberian Dawn INFO).
# The player's house is blown up (blowup 0, the path a defeat or a departure takes) with
# an enemy tank standing among its soldiers. Red Alert's HouseClass::Blowup_All damages
# each object directly and the tank survives: asserted. Tiberian Dawn's 1995 Blowup_All
# drops a 0x7fff Explosion_Damage on every soldier's own coordinate ("like dropping a big
# bomb right on the object; it will also damage anything around it"), which destroys the
# other house's tank: printed as INFO, original behaviour awaiting a decision.
# =====================================================================================
gbegin
cmb_all ra:blowup
if [ "$GRC" != "0" ]; then bad "G515: a run exited $GRC$CMB_V"
elif [ "$CMB_OK" = "1" ]; then ok "G515 Red Alert: a defeated house's collapse spares the other house's tank$CMB_V"
else bad "G515$CMB_V"; fi
cmb_run td blowup 1
echo "INFO  G515 (Tiberian Dawn) Tiberian Dawn defeated-house explosion (original 1995 behaviour, kept): [$CMB_M]"

# INFO, NOT A GATE: NOD ARTILLERY AGAINST A MOVING TARGET (Tiberian Dawn).
# The same artillery piece on guard hits a jeep standing three cells away and does not
# hit one driving past at three cells: it keeps turning its whole body after the jeep and
# TurretClass::Can_Fire refuses a turretless gun that is turning or more than 8 of 256
# facing steps off, and the arcing shell goes where the target was. 1995 behaviour.
cmb_run td arty-still 1;  CMB_S="$CMB_M"
cmb_run td arty-moving 1
echo "INFO  G507 (Tiberian Dawn artillery) artillery: still jeep [$CMB_S], moving jeep [$CMB_M] (original 1995 behaviour)"

fi

if [ -n "$CMB_STANDALONE" ]; then
  echo "combat.sh: $PASS passed, $FAIL failed, $SKIPPED skipped"
  [ "$FAIL" = "0" ]
fi
