#!/bin/sh
# Real RA win event, then the Allied and Soviet two-mission screen flow.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
install="$root/playable"
work=$(mktemp -d "${TMPDIR:-/tmp}/ra3d-campaign.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/verdict.script" <<EOF
tick 40
shot $work/before-verdict.png
cheats
optclick Instant Win
tick 30
shot $work/verdict.png
quit
EOF
cd "$install"
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --harness 1 --rounds 1 --no-movies --nosound \
    --script "$work/verdict.script" --shotdir "$work" > "$work/verdict.log" 2>&1
grep -Fq 'CHEAT|verdict|win|flagged=1|' "$work/verdict.log"
grep -Fq 'GAMEOVER|WIN|movie=SNOWBOMB|score=' "$work/verdict.log"
grep -Fq 'GAMEOVER|movies|first=SNOWBOMB|second=|third=|fourth=|after-score=' "$work/verdict.log"
grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$work/verdict.log"
python3 - "$work/before-verdict.png" "$work/verdict.png" <<'PY'
from PIL import Image
import sys

before = Image.open(sys.argv[1]).convert("RGB")
image = Image.open(sys.argv[2]).convert("RGB")
assert image.size == (1280, 720), image.size
red_ink = new_pale_pixels = 0
for y in range(175, 218):
    for x in range(280, 770):
        r, g, b = image.getpixel((x, y))
        red_ink += r > 130 and r > 2 * g and r > 2 * b
        br, bg, bb = before.getpixel((x, y))
        new_pale_pixels += (175 <= r <= 220 and abs(r - g) < 8 and abs(g - b) < 8
                            and abs(r - br) + abs(g - bg) + abs(b - bb) > 80)
assert red_ink > 300, "mission verdict text did not render"
assert new_pale_pixels < 100, "mission verdict font painted opaque glyph boxes"
PY
for side in allied soviet; do
    case "$side" in
        allied) first=SCG01EA; second=SCG02EA; reel=MSAB; label=ALLIED; fame_side=0 ;;
        soviet) first=SCU01EA; second=SCU02EA; reel=MSSB; label=SOVIET; fame_side=1 ;;
    esac
    OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
        --flowtest 1 --flowrounds 2 --flowside "$side" \
        --no-movies --nosound > "$work/$side.log" 2>&1
    grep -Fq "RA3D|start|$first|original mission" "$work/$side.log"
    grep -Fq "CAMPAIGN|ra-score|scen=1|side=$label|" "$work/$side.log"
    grep -Fq "CAMPAIGN|ra-fame|rank=1|name=OPENCNC3D|score=1234|level=1|side=$fame_side" "$work/$side.log"
    grep -Fq "CAMPAIGN|ra-map|scen-won=1|reel=$reel|picked=0|dir=E|var=A" "$work/$side.log"
    grep -Fq "RA3D|start|$second|original mission" "$work/$side.log"
    if grep -Fq 'CAMPAIGN|movie|CONSYARD|missing' "$work/$side.log"; then
        echo 'Red Alert flow requested a Tiberian Dawn win movie' >&2
        exit 1
    fi
    grep -Fq 'FLOWTEST|complete|2 missions booted and returned through the full flow' "$work/$side.log"
    python3 - "$install/flow_ra_score.png" <<'PY'
from PIL import Image
import sys

image = Image.open(sys.argv[1]).convert("RGB")
assert image.size == (1280, 720), image.size
green_ink = pale_background = 0
for y in range(90, 114):
    for x in range(650, 865):
        r, g, b = image.getpixel((x, y))
        green_ink += g > 90 and g > 2 * r and g > 2 * b
        pale_background += min(r, g, b) >= 175 and max(r, g, b) - min(r, g, b) <= 25
assert green_ink > 100, "Win95 score label did not render"
assert pale_background < 25, "Win95 score font painted pale character backgrounds"
PY
    test -s "$install/flow_ra_fame.png"
done
cmp "$root/data/radata/movies/_C1FB1EE5.BIN" ra3d/movies/ALLIEND.VQA
"$root/video/playvqa" ra3d/movies/ALLIEND.VQA --scan > "$work/ending-scan.log"
grep -Fq 'decoded 2620 frames' "$work/ending-scan.log"
python3 "$root/tools/ra3d/audit-campaign-media.py" --module ra3d
for side in allied soviet; do
    case "$side" in
        allied) rounds=16; last=SCG14EA; label=ALLIED; expected=15 ;;
        soviet) rounds=15; last=SCU14EA; label=SOVIET; expected=14 ;;
    esac
    OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
        --flowtest 1 --flowrounds "$rounds" --flowside "$side" \
        --no-movies --nosound > "$work/$side-full.log" 2>&1
    grep -Fq "RA3D|start|$last|original mission" "$work/$side-full.log"
    grep -Fq "CAMPAIGN|complete|$label" "$work/$side-full.log"
    grep -Fq "FLOWTEST|campaign-ended|$expected missions booted and final result handled" \
        "$work/$side-full.log"
done
OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
    --flowtest 1 --flowrounds 2 --raflowstart 10 --flowside allied \
    --no-movies --nosound > "$work/allied-10.log" 2>&1
grep -Fq 'RA3D|start|SCG10EA|original mission' "$work/allied-10.log"
grep -Fq 'CAMPAIGN|continue|SCG10EA|part=B' "$work/allied-10.log"
grep -Fq 'RA3D|start|SCG10EB|original mission' "$work/allied-10.log"
if grep -Eq 'CAMPAIGN\|ra-(score|map)\|scen=10|CAMPAIGN\|ra-map\|scen-won=10' "$work/allied-10.log"; then
    echo 'Allied 10A incorrectly showed a score or map screen' >&2
    exit 1
fi
for final in 'SCG14EA ALLIEND' 'SCU14EA SOVFINAL'; do
    set -- $final
    scen=$1 ending=$2
    OPENCNC3D_BACKGROUND=1 ./cnc_eyes --ra3d --dylib ra3d/RedAlert.dylib \
        --content ra3d/data --dir ra3d/missions/ --scen "$scen" \
        --pack ra3d/SCG01EA.pack --dosdata ra3d/audio --nosound \
        --script "$work/verdict.script" > "$work/$scen.log" 2>&1
    grep -Fq 'CHEAT|verdict|win|flagged=1|' "$work/$scen.log"
    grep -Fq "GAMEOVER|movies|first=|second=|third=|fourth=|after-score=$ending" "$work/$scen.log"
    grep -Fq '0 failures' "$work/$scen.log"
done
for side in allied soviet; do
    case "$side" in
        allied) ending=ALLIEND; label=ALLIED ;;
        soviet) ending=SOVFINAL; label=SOVIET ;;
    esac
    OPENCNC3D_BACKGROUND=1 ./opencnc3d-launcher --dir . --module ra3d --play -- \
        --flowtest 1 --flowrounds 2 --raflowstart 14 --raflowrealwin \
        --flowside "$side" --nosound > "$work/$side-real-final.log" 2>&1
    grep -Fq 'FLOWTEST|real-verdict|flagged at tick' "$work/$side-real-final.log"
    grep -Fq "GAMEOVER|movies|first=|second=|third=|fourth=|after-score=$ending" \
        "$work/$side-real-final.log"
    grep -Fq "CAMPAIGN|movie|$ending|played" "$work/$side-real-final.log"
    grep -Fq "CAMPAIGN|complete|$label" "$work/$side-real-final.log"
    grep -Fq 'FLOWTEST|campaign-ended|1 missions booted and final result handled' \
        "$work/$side-real-final.log"
done
test -s flow_ra_score.png
test -s flow_ra_map.png
echo 'RA3D real verdicts, both complete synthetic campaign routes, and original ending media passed'
