#!/bin/sh
# Fix and improve one cartridge model, start to finish, and put it in the viewer.
#
#   tools/art/n64fix/run.sh NUKE [work_dir]
#
# Needs tools/art/n64fix/fix_<TYPE>.py, the model's own repair plan (see README.md for
# how one is written). Without one it stops after the survey, which is step 2 of the
# recipe: the survey is what the plan is written from.
set -e
TYPE=$1
[ -n "$TYPE" ] || { echo "usage: $0 TYPE [work_dir]"; exit 2; }
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
WORK=${2:-$ROOT/build-native/n64fix/$TYPE}
PACK=${N64FIX_PACK:-$ROOT/tools/bakery/game/SCG01EA.pack}
BLENDER=${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}
VIEW=$HERE/viewer/models/$TYPE
mkdir -p "$WORK/look" "$VIEW"

# an APPROVED model is locked: its fixed side is what the director signed off
if python3 -c "import json,sys;sys.exit(0 if json.load(open('$HERE/reviews.json')).get('$TYPE',{}).get('state')=='approved' else 1)" 2>/dev/null \
        && [ -z "$N64FIX_FORCE" ]; then
    echo "$TYPE is approved and locked; reopen it in the viewer (or set N64FIX_FORCE=1) to change it"
    exit 1
fi

blend() { "$BLENDER" -b "$@" 2>&1 | grep -E "^(AUDIT|LOOP|FLIP|FIX|VIEW|built)|Error|Traceback" || true; }

echo "== 1. extract $TYPE from $(basename "$PACK")"
python3 "$HERE/extract.py" "$PACK" "$TYPE" -o "$WORK/src"

echo "== 2. survey: open edges, pieces, eight bearings with holes in cyan"
blend --python "$HERE/audit.py" -- "$WORK/src" "$TYPE" "$WORK/look/cull"
blend --python "$HERE/islands.py" -- "$WORK/src" "$TYPE" "$WORK/look/isl"
blend --python "$HERE/export_old.py" -- "$WORK/src" "$TYPE" "$VIEW"
python3 "$HERE/viewer_index.py" "$PACK" >/dev/null

if [ ! -f "$HERE/fix_$TYPE.py" ]; then
    echo "no repair plan yet: write $HERE/fix_$TYPE.py from the survey in $WORK/look"
    exit 0
fi

echo "== 3. textures at 4x, both house palettes"
python3 "$HERE/upscale.py" --dir "$WORK/src"

echo "== 4. repair plan"
blend --python "$HERE/fix_$TYPE.py" -- "$WORK/src" "$WORK/fixed"

blend "$WORK/fixed/${TYPE}_fixed.blend" --python "$HERE/export_new.py" -- "$WORK/src" "$TYPE"
python3 "$HERE/viewer_index.py" "$PACK" >/dev/null

echo "== 5. audit the result"
blend "$WORK/fixed/${TYPE}_fixed.blend" --python "$HERE/audit_fixed.py" -- "$WORK/look/fix"
# ready for review: it leaves the queue
python3 - "$HERE/queue.txt" "$TYPE" <<'PY'
import sys
p, t = sys.argv[1], sys.argv[2]
ls = open(p).read().splitlines(True)
open(p, "w").writelines(l for l in ls if l.split()[:1] != [t])
PY
echo "review: $WORK/look/fix_*.png, and the viewer (preview config n64fix, port 8795)"
