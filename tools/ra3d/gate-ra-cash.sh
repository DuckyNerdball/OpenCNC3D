#!/bin/sh
# Red Alert uses its own credit counter sounds from SOUNDS.MIX.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
install=${1:-"$root/playable"}
install=$(CDPATH= cd -- "$install" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ra3d-cash.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/cash.script" <<EOF
tick 100
creditsdump
cam 69 48
sbclickitem POWR
ready POWR 2000
startplace POWR
hover 69 48
lclickcell 69 48
tick 20
creditsdump
quit
EOF
cd "$install"
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --flowtest 1 --flowrounds 1 --raflowstart 2 --flowside soviet \
    --no-movies --nosound --dumpsound --script "$work/cash.script" > "$work/run.log" 2>&1
grep -Fq 'SOUND|cash|CASHUP1|from=0|to=143|step=+143|gain=500|played' "$work/run.log"
grep -Eq 'SOUND\|cash\|CASHDN1\|from=[0-9]+\|to=[0-9]+\|step=-[0-9]+\|gain=500\|played' "$work/run.log"
grep -Fq 'SCRIPT|end ' "$work/run.log"
grep -Fq '0 failures' "$work/run.log"
if grep -Eq 'SOUND\|cash\|(TONE15|TONE16)|AUDIO\|missing\|(CASHUP1|CASHDN1)' "$work/run.log"; then
    echo 'Red Alert requested the wrong credit cue or missed the original clip' >&2
    exit 1
fi
echo 'RA3D original rising and falling credit sounds passed'
