#!/bin/sh
# Check the original Allied opening through the installed Red Alert module.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
install=${1:-"$root/playable"}
install=$(CDPATH= cd -- "$install" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ra3d-opening.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/opening.script" <<EOF
state
count POWR 7
count TSLA 3
dosdrawprobe
shot $work/opening.png
tick 300
count TRAN 1
count E7 1
obj E7 0
dosdrawprobe
shot $work/tanya.png
cam 0 0
state
edge U 500
state
pan 0 -5000
state
cam 999 999
state
edge D 500
state
pan 0 5000
state
quit
EOF
cd "$install"
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --harness 1 --rounds 1 --no-movies --nosound \
    --script "$work/opening.script" --shotdir "$work" > "$work/run.log" 2>&1
grep -Fq 'CAM|state|x=63.500|z=52.500|' "$work/run.log"
test "$(grep -Fc 'CAM|state|x=59.833|z=52.500|' "$work/run.log")" -eq 3
test "$(grep -Fc 'CAM|state|x=68.167|z=73.500|' "$work/run.log")" -eq 3
grep -Fq 'EDGE|U|frames=500|' "$work/run.log"
grep -Fq 'EDGE|D|frames=500|' "$work/run.log"
grep -Fq 'COUNT|POWR|want=7|have=7|PASS' "$work/run.log"
grep -Fq 'COUNT|TSLA|want=3|have=3|PASS' "$work/run.log"
grep -Eq 'RA2D\|sprites\|ok=1\|records=[0-9]+\|infantry=[2-9][0-9]*\|vehicles=[0-9]+\|buildings=26\|' "$work/run.log"
for shape in POWR TSLA STEK BARR DOME; do
    grep -Fq "DOSDRAW|layer|$shape|asset=$shape|" "$work/run.log"
done
grep -Fq 'COUNT|TRAN|want=1|have=1|PASS' "$work/run.log"
grep -Fq 'COUNT|E7|want=1|have=1|PASS' "$work/run.log"
grep -Fq 'OBJ|E7#0|INFANTRY|Greece|cell=63,48|' "$work/run.log"
grep -Eq 'DOSDRAW\|layer\|TRAN\|asset=TRAN\|.*\|id=0\|sub=0\|' "$work/run.log"
grep -Eq 'DOSDRAW\|layer\|TRAN\|asset=TRAN\|.*\|id=0\|sub=1\|' "$work/run.log"
grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$work/run.log"
test -s "$work/tanya.png"
test -s "$work/opening.png"
cat > "$work/large-edge.script" <<EOF
cam 0 0
edge U 500
pan 0 -5000
state
cam 999 999
edge D 500
pan 0 5000
state
quit
EOF
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --harness 1 --rounds 1 --no-movies --nosound --w 1920 --h 1280 \
    --script "$work/large-edge.script" > "$work/large-edge.log" 2>&1
grep -Fq 'CAM|state|x=59.354|z=53.875|' "$work/large-edge.log"
grep -Fq 'CAM|state|x=68.646|z=72.125|' "$work/large-edge.log"
grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$work/large-edge.log"
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --harness 1 --rounds 1 --no-movies --nosound --w 4572 --h 3038 \
    --script "$work/large-edge.script" > "$work/screenshot-edge.log" 2>&1
grep -Fq 'CAM|state|x=59.583|z=54.042|' "$work/screenshot-edge.log"
grep -Fq 'CAM|state|x=68.417|z=71.958|' "$work/screenshot-edge.log"
grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$work/screenshot-edge.log"
echo 'RA3D Allied opening camera, original infantry and structures, Tanya and one transport passed'
