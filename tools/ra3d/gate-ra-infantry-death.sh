#!/bin/sh
# Prove a real Red Alert infantry death selects frames from the RA SHP table.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
install=${1:-"$root/playable"}
install=$(CDPATH= cd -- "$install" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ra3d-infantry-death.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/death.script" <<EOF
cam 64 49
dosdrawprobe
shot $work/before.png
hurtunit E1:Greece 0 1000
dosdrawprobe
tick 1
dosdrawprobe
shot $work/dying.png
tick 2
dosdrawprobe
quit
EOF
cd "$install"
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --harness 1 --rounds 1 --no-movies --nosound \
    --script "$work/death.script" --shotdir "$work" > "$work/run.log" 2>&1
grep -Fq 'HURTUNIT|E1:Greece#0|id=13|kind=0|alt=0|str-before=50/50|hit' "$work/run.log"
grep -Eq 'DOSDRAW\|layer\|E1\|asset=E1\|frame=30[4-9]\|.*\|id=13\|' "$work/run.log"
if grep -Eq 'DOSDRAW\|layer\|E1\|asset=E1\|frame=39[8-9]\|.*\|id=13\|' "$work/run.log"; then
    echo 'Red Alert death used the Tiberian Dawn virtual frame table' >&2
    exit 1
fi
grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$work/run.log"
python3 - "$work/before.png" "$work/dying.png" <<'PY'
from PIL import Image, ImageChops
import sys

before, dying = (Image.open(path).convert("RGB") for path in sys.argv[1:])
assert before.size == dying.size == (1280, 720)
box = (510, 175, 570, 230)
diff = ImageChops.difference(before.crop(box), dying.crop(box))
changed = sum(max(diff.getpixel((x, y))) > 35
              for y in range(diff.height) for x in range(diff.width))
assert changed > 25, f"infantry death animation changed only {changed} pixels"
print(f"RA3D infantry death: {changed} changed pixels in the original sprite region")
PY
