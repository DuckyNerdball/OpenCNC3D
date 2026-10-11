#!/bin/sh
# Exercise original Red Alert interior art in Allied mission 13.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
install=${1:-"$root/playable"}
install=$(CDPATH= cd -- "$install" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ra3d-interior.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/interior.script" <<EOF
state
tick 1
dosdrawprobe
shot $work/interior.png
quit
EOF
cd "$install"
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --flowtest 1 --flowrounds 1 --raflowstart 13 --flowside allied \
    --no-movies --nosound --script "$work/interior.script" > "$work/run.log" 2>&1
grep -Fq 'RA3D|start|SCG13EA|original mission' "$work/run.log"
grep -Fq 'RA2D|map SCG13EA theater=-1 bounds=' "$work/run.log"
grep -Fq 'RA2D|loaded 3531 original terrain icons' "$work/run.log"
grep -Fq 'DOS2D|loaded 379 original Red Alert SHP shapes' "$work/run.log"
grep -Fq 'sidebar: Red Alert Win95 pack ra3d/sidebar-interior-win95.pack' "$work/run.log"
grep -Fq 'sidebar: DOS pack ra3d/sidebar-interior.pack' "$work/run.log"
grep -Fq 'FLOWTEST|complete|1 missions booted and returned through the full flow' "$work/run.log"
grep -Fq '0 failures' "$work/run.log"
if grep -Fq 'DOS2D|missing original SHP' "$work/run.log"; then
    echo 'interior mission requested an unstaged original sprite' >&2
    exit 1
fi
python3 - "$install/ra3d/dosclassic.pack" "$work/interior.png" <<'PY'
from pathlib import Path
from PIL import Image
import sys

pack = Path(sys.argv[1]).read_bytes()
assert pack[:8] == b"DOS2D005"
for name in (b"I/BOXES04", b"I/BOXES08", b"I/BOXES09"):
    assert name in pack, f"missing interior sprite {name!r}"
image = Image.open(sys.argv[2]).convert("RGB")
assert image.size == (1280, 720)
visible = sum(max(image.getpixel((x, y))) > 40
              for y in range(150, 550) for x in range(20, 320))
assert visible > 15000, f"interior mission rendered only {visible} visible pixels"
print(f"RA3D interior: {visible} visible battlefield pixels and original BOXES art")
PY
