# G525 to G527: the waterfall rocks, the Refinery's gauge and the match debrief's score
# sum, run by game/gates.sh.
#
# Standalone, from a built play folder (headless, nothing takes the screen):
#   cd playable && sh ../game/gate_terrainhud.sh
# Inside game/gates.sh the block is sourced as it is: it only defines ok/bad/BASE/OUT
# when they are not defined already.
#
#   G525  no river-bed gravel on the waterfall rocks under the Enhanced rock relief, and
#         the falls' rock is the cliffs' rock: relief in the surface pack, no pale facets;
#         and the river above them carries no hard line down its middle
#   G526  the Enhanced Refinery's status lights fill while a harvester unloads, and
#         stand unlit otherwise, from the refinery's own animation stage
#   G527  the match debrief spells out how the score is added up as a line of its own,
#         inside the frame under the result, in Tiberian Dawn's kit

if ! command -v ok >/dev/null 2>&1; then
  PASS=0; FAIL=0
  OUT="${OUT:-gates-terrainhud.log}"
  : > "$OUT"
  ok()  { echo "PASS  $1" | tee -a "$OUT"; PASS=$((PASS+1)); }
  bad() { echo "FAIL  $1" | tee -a "$OUT"; FAIL=$((FAIL+1)); }
  GTH_STANDALONE=1
fi
BASE="${BASE:---cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --dir missions/ --content content/ --dist 3000}"
GTH=gp_terrainhud
rm -rf $GTH && mkdir -p $GTH

# =====================================================================================
# G525 THE WATERFALL'S ROCKS CARRY NO RIVER-BED GRAVEL.
#
# Under Enhanced the river's floor is the shader's, except on the authored waterfall
# cells, which keep the cartridge's two-triangle floor of pale gravel under the cut in
# their tiles. The geometric rock relief draws those same cells as a subdivided surface,
# and wherever its relief is zero that surface lies in the floor's own plane: two
# tessellations of one plane, so the depth test chose between them pixel by pixel and the
# gravel showed on the rocks in yellow-green patches with striped edges, which crawled as
# the camera moved. The floor is now pushed behind the ground by the bias the cartridge
# path's floor already takes (water_bed.h).
#
# Three framings of temperate waterfalls (SCG05EA at two camera positions a tenth of a
# cell apart, and SCG15EC), Enhanced at the 144-texel terrain with the relief on; in the
# box round each fall, the pixels of the gravel's colour (green at least 110, red within
# 30 of it, blue 25 below it) are counted. Measured before the fix: 1784 and 1098 on
# SCG05EA, 1759 on SCG15EC; after: 10, 4 and 18. The run must also say the relief surface
# was built and the waterfall cells kept their native floor, or nothing here was tested.
#
# AND THE FALLS' ROCK IS THE CLIFFS' ROCK. The falls were the only rock pieces of the
# Temperate set without the cliffs' physical treatment: their first painting was a quilt of
# pale pyramid and cube facets with its light baked in, flat on the ground, beside the grey
# relief of the cliffs either side. They are now repainted the cliffs' way and carry
# daylight assets of their own (art/remaster/textures/unified_daylight_v01/falls_v01).
# Two more claims, on the same runs: the play folder's terrainsurface_TEMPERAT.pack raises
# every cell of FALLS1 (9) and FALLS2 (6), each template to at least half the relief's
# full height somewhere; and in the same boxes the pale, warm rock of the old painting
# (red at least 150, red 20 or more above blue, red not below green) stays at 600 pixels or
# fewer at each fall. Measured with the old falls: no falls cells in the surface pack, and
# 2058 and 4273 such pixels at SCG05EA's and SCG15EC's falls; with the new ones: all 15
# cells, and the counts in the PASS line.
#
# AND NO SEAM DOWN THE RIVER ABOVE THE FALLS. The light pass marked every water pixel as an
# overlay (the planar water's normal pass wrote the water surface's depth after the depth
# copy the overlay mark compares against), so the water was lit from its depth's own slope.
# A narrow river's native corners rise to its banks, its surface is a V, and the sun lit
# the two faces differently: on SCG05EA, above FALLS2, one half of the river went black
# beside the other along a straight line. On run a, in the river's rows just above the
# falls, the brightness step across that crease (columns 700-708 against 712-720, rows
# 232-305, relative to their mean) is held to 0.30, beside the step between two strips of
# the same water left of it (688-696 against 700-708), and the left strip must not be the
# black band (mean above 12). Measured with the mark: step 0.61, the same water 0.05, left
# strip 7.3; without it: 0.17, 0.24 and 23.3.
# =====================================================================================
g525_run() {  # $1 scen, $2 cam x, $3 cam y, $4 tag
  printf 'tick 20\nzoom max\ncam %s %s\ntick 2\nshot %s/g525_%s.png\nquit\n' "$2" "$3" "$GTH" "$4" > $GTH/g525_$4.txt
  ./cnc_eyes --noshroud --scen "$1" --pack "$1.pack" $BASE --nosound --w 1280 --h 720 \
    --enhanced --texset 3 --script $GTH/g525_$4.txt > $GTH/g525_$4.log 2>&1
  G525RC="$G525RC $?"
  cat $GTH/g525_$4.log >> "$OUT"
}
G525RC=""
g525_run SCG05EA 46 39 a
g525_run SCG05EA 46.1 39 b
g525_run SCG15EC 42 38 c
G525SURF=$(cat $GTH/g525_a.log $GTH/g525_b.log $GTH/g525_c.log | grep -c '^TERRAINSURFACE|built|TEMPERAT|')
G525FALLS=$(cat $GTH/g525_a.log $GTH/g525_c.log | sed -n 's/^WATER|plane|.*|native_falls_cells=\([0-9]*\)|.*/\1/p' | awk '$1 > 0 {n++} END {print n+0}')
G525N=$(python3 - $GTH/g525_a.png $GTH/g525_b.png $GTH/g525_c.png <<'PY'
import sys
import numpy as np
from PIL import Image
boxes = [(520, 260, 820, 460), (520, 260, 820, 460), (440, 220, 840, 500)]
out = []
for path, (x0, y0, x1, y1) in zip(sys.argv[1:], boxes):
    try:
        a = np.asarray(Image.open(path).convert("RGB")).astype(int)[y0:y1, x0:x1]
    except Exception:
        out.append("x"); continue
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    m = (g >= 110) & (abs(r - g) <= 30) & (b < g - 25) & (g >= r - 5)
    out.append(str(int(m.sum())))
print(" ".join(out))
PY
)
set -- $G525N
G525A="${1:-x}"; G525B="${2:-x}"; G525C="${3:-x}"
G525ROCK=$(python3 - terrainsurface_TEMPERAT.pack $GTH/g525_a.png $GTH/g525_c.png <<'PY'
import struct, sys
import numpy as np
from PIL import Image
cells = {163: 0, 164: 0}
top = {163: 0, 164: 0}
try:
    b = open(sys.argv[1], 'rb').read()
    assert b[:8] == b'C3DSURF\0'
    _, hc, n = struct.unpack_from('<III', b, 8)
    start = 20 + n * 24
    for i in range(n):
        tid, icon, var, game = struct.unpack_from('<HHBB', b, 20 + i * 24 + 16)
        if game == 0 and var == 0 and tid in cells:
            cells[tid] += 1
            top[tid] = max(top[tid], max(b[start + i * hc * hc:start + (i + 1) * hc * hc]))
except Exception:
    pass
out = ["%d" % (cells[163] + cells[164]), "%d" % min(top[163], top[164])]
for path, (x0, y0, x1, y1) in zip(sys.argv[2:], [(520, 260, 820, 460), (440, 220, 840, 500)]):
    try:
        a = np.asarray(Image.open(path).convert("RGB")).astype(int)[y0:y1, x0:x1]
    except Exception:
        out.append("x"); continue
    r, g, bl = a[..., 0], a[..., 1], a[..., 2]
    out.append(str(int(((r >= 150) & (r - bl >= 20) & (r >= g)).sum())))
print(" ".join(out))
PY
)
set -- $G525ROCK
G525CELLS="${1:-0}"; G525TOP="${2:-0}"; G525PA="${3:-x}"; G525PC="${4:-x}"
G525SEAM=$(python3 - $GTH/g525_a.png <<'PY'
import sys
import numpy as np
from PIL import Image
try:
    a = np.asarray(Image.open(sys.argv[1]).convert("L")).astype(float)
    rows = slice(232, 306)
    left, right, same = a[rows, 700:709].mean(1), a[rows, 712:721].mean(1), a[rows, 688:697].mean(1)
    step = np.mean(np.abs(right - left) / np.maximum((right + left) / 2, 1))
    ctrl = np.mean(np.abs(left - same) / np.maximum((left + same) / 2, 1))
    seam = int(step <= 0.30 and left.mean() > 12)
    print("%d %.2f %.2f %.1f" % (seam, step, ctrl, left.mean()))
except Exception:
    print("0 x x x")
PY
)
set -- $G525SEAM
G525SOK="${1:-0}"; G525STEP="${2:-x}"; G525SCTL="${3:-x}"; G525SLEFT="${4:-x}"
if [ "$(echo $G525RC)" != "0 0 0" ]; then
  bad "G525 waterfall rocks: a run failed (exit codes $G525RC)"
elif [ "$G525SURF" != "3" ] || [ "$G525FALLS" != "2" ]; then
  bad "G525 waterfall rocks: the scene is not the one this gate measures: relief surface built in $G525SURF of 3 runs, waterfall cells with their native floor in $G525FALLS of 2 maps"
elif [ "$G525CELLS" != "15" ] || [ "$G525TOP" -lt 128 ] 2>/dev/null; then
  bad "G525 waterfall rocks: the falls have no relief of their own: $G525CELLS of 15 FALLS1/FALLS2 cells in terrainsurface_TEMPERAT.pack, lowest template peak $G525TOP of 255 (want all 15, at least 128): the falls are not in the daylight assets"
elif ! { [ "$G525PA" -le 600 ] 2>/dev/null && [ "$G525PC" -le 600 ] 2>/dev/null; }; then
  bad "G525 waterfall rocks: pale faceted rock at the falls, SCG05EA=$G525PA SCG15EC=$G525PC pixels (want 600 or fewer each; the old falls painting measured 2058 and 4273)"
elif [ "$G525SOK" != "1" ]; then
  bad "G525 waterfall rocks: a hard line down the river above SCG05EA's falls: brightness step $G525STEP across the crease against $G525SCTL in the same water beside it, left strip $G525SLEFT (want the step 0.30 or less and the strip above 12; with the water marked as an overlay: 0.61, 0.05 and 7.3)"
elif [ "$G525A" -le 100 ] 2>/dev/null && [ "$G525B" -le 100 ] && [ "$G525C" -le 100 ]; then
  ok "G525 waterfall rocks: no river-bed gravel shows on the waterfalls' rocks under the Enhanced rock relief: $G525A and $G525B gravel-coloured pixels at SCG05EA's falls from two camera positions a tenth of a cell apart, $G525C at SCG15EC's (were 1784, 1098 and 1759 before the floor was biased behind the ground); and the falls' rock is the cliffs': all $G525CELLS falls cells raised in the surface pack (lowest template peak $G525TOP of 255), $G525PA and $G525PC pale-rock pixels at the two falls (the old painting: 2058 and 4273); and no seam down the river above SCG05EA's falls: step $G525STEP across the crease, $G525SCTL in the same water beside it, left strip $G525SLEFT (with the water marked as an overlay: 0.61, 0.05, 7.3)"
else
  bad "G525 waterfall rocks: gravel-coloured pixels at the falls SCG05EA=$G525A, $G525B (camera moved a tenth of a cell) SCG15EC=$G525C, want 100 or fewer each: the floor under the waterfall cells is fighting the relief surface for depth"
fi

# =====================================================================================
# G526 THE REFINERY'S GAUGE FILLS WHILE A HARVESTER UNLOADS.
#
# The 1995 Refinery's column of lights fills from the bottom over the five frames of its
# siphoning animation (bdata.cpp, BSTATE_AUX1 {19, 5}) and stands unlit otherwise; the
# cartridge's strip book does the same (texbook_state_slot). The Enhanced model's
# self-lit blocks used to shine at every moment. Now ebld_gauge_prepare finds them (five, the
# lowest inside the tower's foot, as the 1995 column has five bands) and the draw lights
# stage - 18 of them while siphoning and none otherwise.
#
# The Test Map (SCG90EA) under Enhanced, the refinery in view, sixteen steps of 15 ticks
# from tick 930, which take its harvester from the approach through docking into the
# siphoning loop. Asserted:
#   (a) the model's gauge was found: five blocks;
#   (b) every step's GAUGE line (printed after its picture) lights what the stage asks:
#       0 outside 19..23 and 1 to 5 at stages 19 to 23; at least two idle steps and four
#       siphoning steps seen;
#   (c) the PICTURE: bright green pixels on the tower are fewer than 5 at every idle step,
#       and at stage 23 at least 40 and more than at stage 20.
# Measured: 0 at every idle step, 9, 27, 47 and 68 at stages 20 to 23. PROVEN TO FAIL on
# the build before: it has no gaugedump verb, so the run fails, and its pictures show every
# block lit at every step (68 bright green pixels at stage 10 as at stage 23).
# =====================================================================================
{ printf 'tick 30\nlook PROC 0\ndist 1000\ntick 900\n'
  for i in $(seq 0 15); do printf 'tick 15\nanims\nshot %s/g526_%02d.png\ngaugedump\n' "$GTH" "$i"; done
  printf 'quit\n'; } > $GTH/g526.txt
./cnc_eyes --scen SCG90EA --pack SCG01EA.pack $BASE --noshroud --nosound --w 1280 --h 720 \
  --enhanced --texset 3 --script $GTH/g526.txt > $GTH/g526.log 2>&1
G526RC=$?
cat $GTH/g526.log >> "$OUT"
G526BLK=$(sed -n 's/^ebld: PROC gauge: piece [0-9]*, \([0-9]*\) blocks.*/\1/p' $GTH/g526.log | head -1)
G526R=$(python3 - $GTH/g526.log $GTH <<'PY'
import sys
import numpy as np
from PIL import Image
log, d = sys.argv[1], sys.argv[2]
stages, gauges = [], []
for line in open(log, errors="replace"):
    if line.startswith("ANIM|PROC|"):
        f = dict(x.split("=", 1) for x in line.strip().split("|")[2:] if "=" in x)
        stages.append(int(f.get("stage", -1)))
    elif line.startswith("GAUGE|"):
        if line.startswith("GAUGE|none"):
            gauges.append(None); continue
        f = dict(x.split("=", 1) for x in line.strip().split("|")[2:] if "=" in x)
        gauges.append((int(f["stage"]), int(f["lit"]), int(f["of"])))
want = {19: 1, 20: 2, 21: 3, 22: 4, 23: 5}
wrong, idle, siph = 0, 0, 0
greens = []
for i, s in enumerate(stages):
    g = gauges[i] if i < len(gauges) else None
    exp = want.get(s, 0)
    if g is None or g[1] != exp:
        wrong += 1
    if exp == 0: idle += 1
    else: siph += 1
    try:
        a = np.asarray(Image.open("%s/g526_%02d.png" % (d, i)).convert("RGB")).astype(int)[255:375, 470:530]
        m = (a[..., 1] > 150) & (a[..., 1] > a[..., 0] + 40) & (a[..., 1] > a[..., 2] + 10)
        greens.append(int(m.sum()))
    except Exception:
        greens.append(-1)
idle_max = max([greens[i] for i, s in enumerate(stages) if s not in want] or [-1])
full = [greens[i] for i, s in enumerate(stages) if s == 23]
low = [greens[i] for i, s in enumerate(stages) if s == 20]
print(len(stages), wrong, idle, siph, idle_max, min(full) if full else -1, max(low) if low else -1)
PY
)
set -- $G526R
G526N="${1:-0}"; G526WRONG="${2:-x}"; G526IDLE="${3:-0}"; G526SIPH="${4:-0}"
G526IG="${5:-x}"; G526FULL="${6:--1}"; G526LOW="${7:--1}"
if [ "$G526RC" != "0" ]; then
  bad "G526 refinery gauge: the run failed (exit $G526RC)"
elif [ "$G526BLK" != "5" ]; then
  bad "G526 refinery gauge: the Enhanced Refinery's gauge was not found (blocks=${G526BLK:-none}, want 5)"
elif [ "$G526N" = "16" ] && [ "$G526WRONG" = "0" ] && [ "$G526IDLE" -ge 2 ] && [ "$G526SIPH" -ge 4 ] \
     && [ "$G526IG" -lt 5 ] && [ "$G526FULL" -ge 40 ] && [ "$G526FULL" -gt "$G526LOW" ]; then
  ok "G526 refinery gauge: the Enhanced Refinery's five status lights follow its animation stage, unlit over $G526IDLE idle steps (at most $G526IG bright green pixels on the tower) and filling over $G526SIPH siphoning steps (all five at stage 23, $G526FULL pixels, against $G526LOW with two at stage 20)"
else
  bad "G526 refinery gauge: steps=$G526N(want 16) wrong-lit=$G526WRONG(want 0) idle-steps=$G526IDLE(>=2) siphon-steps=$G526SIPH(>=4) idle-green-max=$G526IG(want <5) full-green=$G526FULL(want >=40 and more than stage 20's $G526LOW)"
fi

# =====================================================================================
# G527 THE MATCH DEBRIEF'S SCORE SUM IS A LINE OF THE SCREEN, INSIDE ITS FRAME.
#
# The line saying how a score is added up used to stand outside Tiberian Dawn's match plate
# in the screen's bottom left corner beside CONTINUE, where it read as a stray debugging
# line; moving it onto that plate's floor was not taken. Tiberian Dawn's match now ends on
# Red Alert's TOP SCORES layout in its own kit, and the sum goes where that screen puts it:
# one line in the muted ink under the result line and this machine's numbers, inside the
# frame, spelt for this game, "Score = units x2 + structures x5 + tiberium/10", whenever
# there is a score of this machine's to explain, however many commanders the table holds.
#
# Three --matchshot boards, asserted off the line the log says was drawn and off the
# photograph (gate_debrief.py places the dialog by the CONTINUE button the log reports):
#   (a) two commanders: formula=1, and the muted ink on the line under the result, starting
#       at the dialog's left text margin (x 38) and ending well inside the frame (by 760);
#   (b) eight commanders: the same line, drawn the same way: a full room keeps it;
#   (c) eight commanders with no row this machine's: formula=0 and no ink on that line.
# PROVEN TO FAIL on the build before: --matchshot drew the older plate, which logged its
# note on CAMPAIGN|matchscore|note= lines and no APP|debrief|result line at all.
# =====================================================================================
G527A=""; G527B=""; G527C=""; G527PA=""; G527PB=""; G527PC=""
if [ -x ./opencnc3d ] && [ -s enhanced-menu.pack ]; then
  for g527k in a b c; do
    case $g527k in a) g527r=2; g527l="";; b) g527r=8; g527l="";; c) g527r=8; g527l=none;; esac
    rm -f $GTH/debrief.png
    OPENCNC3D_MATCHSHOT_ROWS=$g527r OPENCNC3D_MATCHSHOT_LOCAL=$g527l ./opencnc3d --matchshot \
      --w 1280 --h 800 --dylib ./TiberianDawn.dylib --dir ./missions/ --shotdir $GTH \
      > $GTH/g527_$g527k.log 2>&1
    cat $GTH/g527_$g527k.log >> "$OUT"
    g527line=$(grep -a '^APP|debrief|result=' $GTH/g527_$g527k.log | head -1)
    g527rows=$(grep -a '^APP|debrief|.*|rows=' $GTH/g527_$g527k.log | head -1 | sed -n 's/.*|rows=\([0-9]*\)$/\1/p')
    g527btn=$(sed -n 's/^DEBRIEF|shown|once|continue=\([-0-9,]*\)$/\1/p' $GTH/g527_$g527k.log | head -1)
    g527pic=""
    if [ -s $GTH/debrief.png ] && [ -n "$g527btn" ]; then
      mv $GTH/debrief.png $GTH/g527_$g527k.png
      g527pic=$(python3 "${GATEDIR:-.}/gate_debrief.py" $GTH/g527_$g527k.png "$g527btn")
    fi
    case $g527k in
      a) G527A="rows=$g527rows ${g527line#APP|debrief|}"; G527PA="$g527pic";;
      b) G527B="rows=$g527rows ${g527line#APP|debrief|}"; G527PB="$g527pic";;
      c) G527C="rows=$g527rows ${g527line#APP|debrief|}"; G527PC="$g527pic";;
    esac
  done
fi
g527_v() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
g527_drawn() {  # $1 the log summary, $2 the picture: the sum logged and inked inside the frame
  case "$1" in *"|formula=1") ;; *) return 1;; esac
  [ "$(g527_v "$2" formula)" -ge 40 ] 2>/dev/null && [ "$(g527_v "$2" formula_x0)" -ge 36 ] \
    && [ "$(g527_v "$2" formula_x0)" -le 42 ] && [ "$(g527_v "$2" formula_x1)" -gt 300 ] \
    && [ "$(g527_v "$2" formula_x1)" -le 760 ]
}
if [ -z "$G527PA" ] || [ -z "$G527PB" ] || [ -z "$G527PC" ]; then
  bad "G527 match debrief score sum: no debrief picture or no DEBRIEF line (two [$G527A] [$G527PA], eight [$G527B] [$G527PB], eight with no local row [$G527C] [$G527PC])"
elif g527_drawn "$G527A" "$G527PA" && g527_drawn "$G527B" "$G527PB" \
     && case "$G527A" in "rows=2 "*) true;; *) false;; esac \
     && case "$G527B" in "rows=8 "*) true;; *) false;; esac \
     && case "$G527C" in *"|formula=0") true;; *) false;; esac && [ "$(g527_v "$G527PC" formula)" = "0" ]; then
  ok "G527 match debrief score sum: Tiberian Dawn's debrief spells out how the score is added up as its own muted line under the result, inside the frame (dialog x $(g527_v "$G527PA" formula_x0) to $(g527_v "$G527PA" formula_x1), $(g527_v "$G527PA" formula) pixels with two commanders, x $(g527_v "$G527PB" formula_x0) to $(g527_v "$G527PB" formula_x1) with eight), and leaves it out when no row is this machine's"
else
  bad "G527 match debrief score sum: two=[$G527A] [$G527PA] eight=[$G527B] [$G527PB] (want formula=1, formula ink >=40 from x 36..42 to x 301..760); eight-no-local=[$G527C] [$G527PC] (want formula=0 and formula ink 0)"
fi

if [ -n "$GTH_STANDALONE" ]; then
  echo "terrain and HUD gates: $PASS passed, $FAIL failed (log $OUT)"
  [ "$FAIL" = "0" ]
fi
