#!/bin/sh
# Launch from the second-slot app menu, select an original RA jeep, and move it.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
install=${1:-"$root/playable"}
install=$(CDPATH= cd -- "$install" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ra3d-play.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cd "$install"
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --harness 1 --no-movies --nosound \
    --script "$root/tools/ra3d/first-mission.script" \
    --shotdir "$work" > "$work/run.log" 2>&1
grep -Fq 'RA3D|start|SCG01EA|original mission' "$work/run.log"
grep -Fq 'sidebar: DOS pack ra3d/sidebar-snow.pack (100 shapes, 3 fonts)' "$work/run.log"
grep -Eq 'RA2D\|sprites\|ok=1\|records=[1-9][0-9]*\|infantry=[1-9][0-9]*\|vehicles=[1-9][0-9]*' "$work/run.log"
grep -Fq 'EXPECTSEL|want=1|have=1|PASS' "$work/run.log"
grep -Fq 'SEL|UNIT|JEEP|id=0|cell=6460|x=60|y=50' "$work/run.log"
grep -Fq 'MUSIC|play|BIGF226M|theme=0|307s|repeats' "$work/run.log"
grep -Fq 'HARNESS|end|2 round(s), 0 failure(s)' "$work/run.log"
test -s "$work/h_menu1.png"
test -s "$work/h_side1.png"
test -s "$work/h_game1.png"
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --harness 1 --rounds 1 --no-movies --nosound --dumpsound --raside soviet \
    --script "$root/tools/ra3d/soviet-first.script" \
    --shotdir "$work" > "$work/soviet.log" 2>&1
grep -Fq 'RA3D|start|SCU01EA|original mission' "$work/soviet.log"
grep -Fq 'sidebar: DOS pack ra3d/sidebar-snow.pack (100 shapes, 3 fonts)' "$work/soviet.log"
grep -Eq 'RA2D\|sprites\|ok=1\|records=[1-9][0-9]*\|infantry=[1-9][0-9]*\|vehicles=[1-9][0-9]*' "$work/soviet.log"
grep -Fq 'EXPECTSEL|want=1|have=1|PASS' "$work/soviet.log"
grep -Fq 'EXPECTCELL|E1:USSR#0|want=43,80|have=43,80|PASS' "$work/soviet.log"
grep -Fq 'EXPECTCURSOR|want=move|have=move|PASS' "$work/soviet.log"
grep -Eq 'CURSOR\|at=496\.0,456\.0\|shape=move\|frames=4\|rate=4\|hotspot=14,11' "$work/soviet.log"
grep -Fq 'MUSIC|play|CRUS226M|theme=1|222s|repeats' "$work/soviet.log"
grep -Eq 'SOUND\|sfx\|[0-9]+\|YESSIR1\.R[0-9]+.*\|played' "$work/soviet.log"
grep -Eq 'SOUND\|sfx\|[0-9]+\|RITAWAY\.R[0-9]+.*\|played' "$work/soviet.log"
grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$work/soviet.log"
cat > "$work/reselect.script" <<EOF
cam 62 50
tick 1
lclickobj JEEP 0
tick 2
lclickobj JEEP 0
tick 2
quit
EOF
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --harness 1 --rounds 1 --no-movies --nosound --dumpsound \
    --script "$work/reselect.script" --shotdir "$work" > "$work/reselect.log" 2>&1
test "$(grep -Ec 'SOUND\|sfx\|[0-9]+\|(YESSIR1|AWAIT1)\.V[0-9]+.*\|played' "$work/reselect.log")" -eq 2
grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$work/reselect.log"
"$root/tools/ra3d/gate-ra-opening.sh" "$install"
"$root/tools/ra3d/gate-ra-effects.sh" "$install"
sh "$root/tools/ra3d/gate-ra-infantry-death.sh" "$install"
sh "$root/tools/ra3d/gate-ra-interior.sh" "$install"
sh "$root/tools/ra3d/gate-ra-cash.sh" "$install"
echo 'RA3D Allied and Soviet mission moves, marker, explosion, sidebar, cursor, music and responses passed'
