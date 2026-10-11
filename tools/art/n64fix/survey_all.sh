#!/bin/sh
# Put every structure's cartridge model and thumbnail into the viewer, one at a time,
# refreshing the list after each so they appear as they are done.
#   tools/art/n64fix/survey_all.sh
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
PACK=${N64FIX_PACK:-$ROOT/tools/bakery/game/SCG01EA.pack}
BLENDER=${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}
python3 "$HERE/viewer_index.py" "$PACK"
for T in $(python3 -c "import json;print(' '.join(r['type'] for r in json.load(open('$HERE/viewer/models/index.json'))))"); do
    W=$ROOT/build-native/n64fix/$T
    # a model being worked on reads its own src folder; leave it alone
    [ -e "$HERE/viewer/models/$T/WORKING" ] && continue
    python3 "$HERE/extract.py" "$PACK" "$T" -o "$W/src" >/dev/null
    "$BLENDER" -b --python "$HERE/export_old.py" -- "$W/src" "$T" "$HERE/viewer/models/$T" 2>&1 | grep -E "^VIEW|Error|Traceback"
    python3 "$HERE/viewer_index.py" "$PACK" >/dev/null
done
python3 "$HERE/viewer_index.py" "$PACK"
