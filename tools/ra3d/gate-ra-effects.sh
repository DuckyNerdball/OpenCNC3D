#!/bin/sh
# Prove the original RA animation stream and its Classic SHPs in a real mission.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
install=${1:-"$root/playable"}
install=$(CDPATH= cd -- "$install" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ra3d-effects.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/move.script" <<EOF
cam 62 50
tick 1
sbhit 1080 580
lclickobj JEEP 0
expectsel 1
shot $work/move-before.png
order 60 50
tick 1
dosdrawprobe
shot $work/move-after.png
quit
EOF
cat > "$work/death.script" <<EOF
cam 62 50
tick 1
hurtunit JEEP 0 1000
tick 1
efxdump
dosdrawprobe
quit
EOF
cat > "$work/blast.script" <<EOF
cam 62 50
tick 1
shot $work/blast-before.png
efxanim 2 60.5 50.5
tick 3
dosdrawprobe
shot $work/blast-after.png
quit
EOF
cat > "$work/flash.script" <<EOF
cam 63 50
tick 120
lclickobj JEEP 0
expectsel 1
cam 63 58
orderobj POWR:USSR 2
tick 1
tick 1
tick 1
dosdrawprobe
shot $work/flash-on.png
tick 1
dosdrawprobe
shot $work/flash-off.png
quit
EOF

cd "$install"
for case_name in move death blast flash; do
    set -- --nosound
    if [ "$case_name" = death ]; then
        set -- --dumpsound
    fi
    OPENCNC3D_BACKGROUND=1 SDL_AUDIODRIVER=dummy \
    ./opencnc3d-launcher --dir . --module ra3d --play -- \
        --harness 1 --rounds 1 --no-movies "$@" \
        --script "$work/$case_name.script" --shotdir "$work" \
        > "$work/$case_name.log" 2>&1
    grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$work/$case_name.log"
done
grep -Fq 'DOSDRAW|anim|MOVEFLSH|frame=0|stage=0|type=51' "$work/move.log"
grep -Fq 'SBHIT|1080,580|item|column=0|slot=4|rows=5' "$work/move.log"
grep -Fq 'HURTUNIT|JEEP#0|' "$work/death.log"
grep -Fq '|hit' "$work/death.log"
grep -Fq 'EFXSEEN|FRAG1|1' "$work/death.log"
grep -Fq 'DOSDRAW|anim|FRAG1|frame=0|stage=0|type=2' "$work/death.log"
grep -Fq 'SOUND|sfx|76|KABOOM30|' "$work/death.log"
grep -Fq 'EFXANIM|type=2|at=60.50,50.50|ok' "$work/blast.log"
grep -Fq 'DOSDRAW|anim|FRAG1|frame=2|stage=2|type=2' "$work/blast.log"
grep -Fq 'ORDER|at=' "$work/flash.log"
grep -Fq '|target=POWR|probe=5|ATTACK' "$work/flash.log"
grep -Fq 'DOSDRAW|flash|POWR|POWR|id=3|blush=1|flags=2|player=Greece' "$work/flash.log"
grep -Fq 'DOSDRAW|flash|POWR|POWR|id=3|blush=0|flags=2|player=Greece' "$work/flash.log"

python3 - "$work" <<'PY'
from pathlib import Path
from PIL import Image
import re
import sys

work = Path(sys.argv[1])
log = (work / "move.log").read_text()
match = re.search(r"ORDERCELL\|cell=60,50\|screen=([0-9.]+),([0-9.]+)", log)
if match is None:
    raise SystemExit("move order has no projected destination")
cx, cy = (int(float(v)) for v in match.groups())

# A 1280x720 window keeps the map at 2x nearest-pixel scale while the
# narrower Win95 sidebar fits five interactive build rows.
frame = Image.open(work / "move-before.png").convert("RGB")
if frame.size != (1280, 720) or frame.getpixel((1270, 5)) == (0, 0, 0):
    raise SystemExit("Win95 sidebar does not reach the right window edge")
if frame.getpixel((1040, 10)) == (0, 0, 0):
    raise SystemExit("Win95 sidebar is not anchored at x=1040")
for name, minimum in (("move", 15), ("blast", 120)):
    before = Image.open(work / f"{name}-before.png").convert("RGB")
    after = Image.open(work / f"{name}-after.png").convert("RGB")
    if before.size != after.size:
        raise SystemExit(f"{name}: screenshot dimensions changed")
    changed = sum(
        max(abs(a - b) for a, b in zip(before.getpixel((x, y)), after.getpixel((x, y)))) > 32
        for y in range(max(0, cy - 55), min(before.height, cy + 55))
        for x in range(max(0, cx - 55), min(before.width, cx + 55))
    )
    if changed < minimum:
        raise SystemExit(f"{name}: only {changed} pixels changed at the effect cell")
    print(f"RA3D {name}: {changed} changed pixels at the original effect cell")

on = Image.open(work / "flash-on.png").convert("RGB")
off = Image.open(work / "flash-off.png").convert("RGB")
flash_log = (work / "flash.log").read_text()
match = re.search(r"ORDER\|at=([0-9.]+),([0-9.]+).*\|target=POWR", flash_log)
if match is None:
    raise SystemExit("attack order has no projected target")
fx, fy = (int(float(v)) for v in match.groups())
def changed(rect):
    count = 0
    bright = 0
    for y in range(rect[1], rect[3]):
        for x in range(rect[0], rect[2]):
            a, b = on.getpixel((x, y)), off.getpixel((x, y))
            if max(abs(c - d) for c, d in zip(a, b)) > 30:
                count += 1
            bright += sum(a) - sum(b)
    return count, bright
target, target_bright = changed((fx - 60, fy - 60, fx + 60, fy + 60))
control, _ = changed((fx - 236, fy - 60, fx - 116, fy + 60))
if target < 700 or target_bright < 100000 or target < control * 10:
    raise SystemExit(f"target flash missing or not localized: {target} target pixels, "
                     f"brightness {target_bright}, {control} control pixels")
print(f"RA3D attack target flash: {target} changed pixels, {control} at control")
PY
echo 'RA3D move marker, explosion, sound and target flash passed in Classic mode'
