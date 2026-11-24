#!/bin/sh
# game/gate_debrief.sh -- G541 and G542, the match debrief's pages and the match timeline,
# run by game/gates.sh.
#
# RUN STANDALONE:  sh game/gate_debrief.sh [play-folder]
#   The play folder defaults to ./playable beside this checkout. Every copy of the game it
#   starts is hidden (OPENCNC3D_BACKGROUND), so nothing takes the screen. About three
#   minutes. Exit status is the number of failed gates.
#
#   G541  the debrief's six pages and its graph, drawn by the application: Summary, Score,
#         Units, Structures, Economy and the Graph with each of its six series, in
#         Tiberian Dawn's kit from --matchshot's eight commanders, every page a different
#         picture, every commander's line on the graph, this machine's the thickest; and a
#         real Tiberian Dawn skirmish against computers ending on the same pages with a
#         timeline of its own
#   G542  the match is sampled over time, every player, by the renderer: a skirmish of
#         several minutes against three computers gives one sample every 75 engine ticks, the
#         harvest never falls, the table's last samples agree with the final figures, the
#         computers are seen to build, a small table halves and doubles its step instead
#         of growing, and two runs of one seed record the same timeline

if ! command -v ok >/dev/null 2>&1; then
  GD_HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
  RUNDIR=${1:-"$GD_HERE/../playable"}
  RUNDIR=$(CDPATH= cd -- "$RUNDIR" && pwd) || { echo "FAIL: no play folder at $1" >&2; exit 1; }
  cd "$RUNDIR" || exit 1
  OUT=${OUT:-/dev/null}
  PASS=0; FAIL=0
  ok()  { echo "PASS  $1" | tee -a "$OUT"; PASS=$((PASS+1)); }
  bad() { echo "FAIL  $1" | tee -a "$OUT"; FAIL=$((FAIL+1)); }
  GD_STANDALONE=1
  OPENCNC3D_BACKGROUND=1; export OPENCNC3D_BACKGROUND
fi
GDR=gp_debrief
rm -rf $GDR && mkdir -p $GDR
GD_PY="${GATEDIR:-.}/gate_debrief.py"
[ -f "$GD_PY" ] || GD_PY=./gate_debrief.py
GD_SK="--cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --dir missions/ --content content/"

# =====================================================================================
# G541 THE DEBRIEF HAS PAGES: SUMMARY, SCORE, UNITS, STRUCTURES, ECONOMY AND A GRAPH.
#
# The TOP SCORES board alone left out most of what the old match plate and the 1995 and
# 1996 score screens carried. Under its title it now has a row of tabs, and every page is
# a different table: each commander's score as a bar with the two ratings, what they built,
# destroyed and lost of units and of structures, the money they started with, harvested,
# spent and held, and a graph of the match over time, one line per commander in their
# colour, with a button for each of six series. (G325 holds each page's figures and bars
# pixel by pixel in both games' kits; this gate holds the application's route to them.)
#
#   (a) --matchshot's eight commanders with OPENCNC3D_DEBRIEF_PAGES=1: the log says six
#       pages and the sample's 48 samples 75 ticks apart, and eleven pictures are written
#       (five pages and the graph's six series); all eleven differ in the dialog's content
#       area; on the graph every commander's colour draws a line, this machine's (gold)
#       the most pixels.
#   (b) a real skirmish against computers through the lobby (--lobbyplay 3000): it ends
#       on the same pages, its log names a timeline of at least 30 samples, its graph
#       draws at least two commanders' lines, and no commander is credited with more than
#       one battle won for the one match (the computers are all named COMPUTER).
# PROVEN TO FAIL on the build before: the debrief had no pages and wrote no page pictures,
# and --lobbyplay ended without a debrief.
# =====================================================================================
G541A=""; G541AP=""; G541B=""; G541BP=""
if [ -x ./opencnc3d ] && [ -s enhanced-menu.pack ]; then
  mkdir -p $GDR/a $GDR/b
  OPENCNC3D_DEBRIEF_PAGES=1 ./opencnc3d --matchshot --w 1280 --h 800 --dylib ./TiberianDawn.dylib \
    --dir ./missions/ --shotdir $GDR/a > $GDR/a.log 2>&1
  cat $GDR/a.log >> "$OUT"
  G541A=$(grep -a '^APP|debrief|pages=' $GDR/a.log | head -1)
  g541btn=$(sed -n 's/^DEBRIEF|shown|once|continue=\([-0-9,]*\)$/\1/p' $GDR/a.log | head -1)
  [ -n "$g541btn" ] && G541AP=$(python3 "$GD_PY" pages $GDR/a "$g541btn")
  OPENCNC3D_DEBRIEF_PAGES=1 ./opencnc3d --lobbyplay 3000 --dylib ./TiberianDawn.dylib --dir ./missions/ \
    --content ./content/ --cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack \
    --shotdir $GDR/b > $GDR/b.log 2>&1 &
  g541p=$!; g541w=0
  while kill -0 $g541p 2>/dev/null && [ $g541w -lt 300 ]; do sleep 1; g541w=$((g541w+1)); done
  kill $g541p 2>/dev/null; wait $g541p 2>/dev/null
  cat $GDR/b.log >> "$OUT"
  G541B=$(grep -a '^APP|debrief|pages=' $GDR/b.log | head -1)
  g541btn=$(sed -n 's/^DEBRIEF|shown|once|continue=\([-0-9,]*\)$/\1/p' $GDR/b.log | head -1)
  [ -n "$g541btn" ] && G541BP=$(python3 "$GD_PY" pages $GDR/b "$g541btn")
fi
g541_v() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
G541AL=$(g541_v "$G541AP" lines); G541BL=$(g541_v "$G541BP" lines)
G541ALOCAL=$(printf '%s' "$G541AL" | cut -d, -f1)
G541AOK=1
for g541c in $(printf '%s' "$G541AL" | tr ',' ' '); do
  [ "$g541c" -ge 150 ] 2>/dev/null || G541AOK=0
  [ "$g541c" -le "${G541ALOCAL:-0}" ] 2>/dev/null || G541AOK=0
done
[ -n "$G541AL" ] || G541AOK=0
G541BLINES=0
for g541c in $(printf '%s' "$G541BL" | tr ',' ' '); do [ "$g541c" -ge 150 ] 2>/dev/null && G541BLINES=$((G541BLINES+1)); done
G541BS=$(printf '%s' "$G541B" | sed -n 's/.*|samples=\([0-9]*\)|.*/\1/p')
# ONE MATCH WINS A COMMANDER ONE BATTLE AT MOST: every computer is called COMPUTER, and
# rows told apart by name and side alone made all of one side's computers a single row
# that won once for each of them (1, 2, 3 ... down the table).
G541BWINS=$(grep -a '^APP|debrief|row=' $GDR/b.log 2>/dev/null | sed -n 's/.*|wins=\([0-9]*\)|.*/\1/p' | sort -n | tail -1)
if [ "$G541A" = "APP|debrief|pages=6|samples=48|interval=75" ] && [ "$(g541_v "$G541AP" pages)" = "11" ] \
   && [ "$(g541_v "$G541AP" distinct)" = "11" ] && [ "$G541AOK" = "1" ] \
   && [ "$(g541_v "$G541BP" pages)" = "11" ] && [ "${G541BS:-0}" -ge 30 ] && [ "$G541BLINES" -ge 2 ] \
   && [ "${G541BWINS:-9}" -le 1 ]; then
  ok "G541 the debrief has pages: eleven pictures from --matchshot's eight commanders (five pages and six graph series), all different, every commander's line on the graph (pixels $G541AL, this machine's first and most); a real skirmish against computers ends on the same pages with $G541BS samples and $G541BLINES commanders' lines (pixels $G541BL), the most battles any commander won in the one match: $G541BWINS"
else
  bad "G541 the debrief's pages: sample=[$G541A] [$G541AP] (want pages=6 samples=48 interval=75, 11 pictures all distinct, every line >=150 px and the first the most) real=[$G541B] [$G541BP] (want 11 pictures, >=30 samples, >=2 lines) most-battles-won-in-one-match=$G541BWINS(want <=1)"
fi

# =====================================================================================
# G542 THE MATCH IS SAMPLED OVER TIME, EVERY PLAYER, BY EVERY MACHINE ITSELF.
#
# The graph needs every commander's numbers through the match, and nothing in the engine
# keeps them, so the renderer records them (match_timeline_tick): every 75 engine ticks a
# sample of each commander's score, army and structures standing, harvest, money held and
# kills, read from the sidebar state and the world it already parses each tick; and every
# tick what each commander newly fields, for the built figures. It only reads.
#
#   (a) a skirmish of up to five minutes (4500 ticks) against three computers, which beat
#       the idle commander a little before that: one sample per 75 ticks of the match's
#       own length (MATCHSTATS seconds), four rows of six series each as long; every
#       row's harvest never falls and its last sample is at most the final harvest in the
#       table; every row's kills likewise; the computers' army series reaches above zero
#       and no army sample passes that row's peak; and the computers built units
#       (MATCHSTATS|n|more units_built > 0).
#   (b) the same match with the table capped at 16 samples (OPENCNC3D_MATCHTL_CAP=16):
#       MATCHTL|halved is logged twice or more, at most 16 samples are kept, the interval
#       is 300 ticks or more and the samples still reach to within two steps of the end.
#   (c) the same match again: the timeline and the extra table lines are byte for byte the
#       first run's.
# PROVEN TO FAIL on the build before: no MATCHTL line and no MATCHSTATS|n|more line at all.
# =====================================================================================
G542A=""; G542BAD=""
if [ -x ./cnc_eyes ] && [ -s SCM01EA.pack ] && [ -s missions/SCM01EA.INI ]; then
  printf 'tick 4500\nquit\n' > $GDR/long.txt
  for g542k in a b c; do
    g542cap=""; [ $g542k = b ] && g542cap=16
    OPENCNC3D_MATCHTL_CAP=$g542cap ./cnc_eyes --scen SCM01EA --pack SCM01EA.pack $GD_SK --skirmish \
      --side gdi --ai 3 --seed 4242 --script $GDR/long.txt > $GDR/tl_$g542k.log 2>&1
    cat $GDR/tl_$g542k.log >> "$OUT"
  done
  G542A=$(python3 - $GDR/tl_a.log $GDR/tl_b.log $GDR/tl_c.log <<'PY'
import re, sys
def read(p):
    t = open(p, errors="replace").read()
    head = re.findall(r"^MATCHTL\|samples=(\d+)\|interval=(\d+)\|first=(-?\d+)$", t, re.M)
    series = {}
    for r, s, vals in re.findall(r"^MATCHTL\|(\d+)\|series=(\d+)\|(.*)$", t, re.M):
        series[(int(r), int(s))] = [int(v) for v in vals.split(",") if v]
    stats = {}
    for r, rest in re.findall(r"^MATCHSTATS\|(\d+)\|(name=.*)$", t, re.M):
        stats[int(r)] = dict(kv.split("=", 1) for kv in rest.split("|") if "=" in kv)
    for r, rest in re.findall(r"^MATCHSTATS\|(\d+)\|more\|(.*)$", t, re.M):
        stats.setdefault(int(r), {}).update(dict(kv.split("=", 1) for kv in rest.split("|") if "=" in kv))
    keep = [l for l in t.splitlines() if l.startswith("MATCHTL|") or "|more|" in l]
    return head, series, stats, "\n".join(keep), t.count("MATCHTL|halved")
bad = []
ha, sa, ta, ka, _ = read(sys.argv[1])
hb, sb, tb, kb, halved = read(sys.argv[2])
hc, sc, tc, kc, _ = read(sys.argv[3])
if not ha:
    print("NO-TIMELINE"); sys.exit()
n, interval = int(ha[-1][0]), int(ha[-1][1])
rows = sorted(ta)
# The match runs until the script quits or a side falls; its length is the table's own.
secs = re.findall(r"^MATCHSTATS\|rows=\d+\|win=\d+\|seconds=(\d+)\|", open(sys.argv[1], errors="replace").read(), re.M)
ticks = int(secs[-1]) * 15 if secs else 0
if ticks < 3000 or interval != 75 or n < ticks // 75 - 1 or n > ticks // 75 + 2:
    bad.append("samples=%d interval=%d over %d ticks" % (n, interval, ticks))
if len(rows) != 4: bad.append("rows=%d" % len(rows))
built = 0
for r in rows:
    for s in range(6):
        if len(sa.get((r, s), [])) != n: bad.append("row %d series %d has %d samples" % (r, s, len(sa.get((r, s), []))))
    h = sa.get((r, 3), [0]); k = sa.get((r, 5), [0]); a = sa.get((r, 1), [0])
    if any(h[i] > h[i + 1] for i in range(len(h) - 1)): bad.append("row %d harvest falls" % r)
    if h[-1] > int(ta[r].get("harvested", 0)): bad.append("row %d harvest %d past final %s" % (r, h[-1], ta[r].get("harvested")))
    if any(k[i] > k[i + 1] for i in range(len(k) - 1)): bad.append("row %d kills fall" % r)
    if k[-1] > int(ta[r].get("killed", 0)) + int(ta[r].get("razed", 0)): bad.append("row %d kills past final" % r)
    if max(a) > int(ta[r].get("army_peak", 0)): bad.append("row %d army %d past peak %s" % (r, max(a), ta[r].get("army_peak")))
    if r > 0:
        if max(a) <= 0: bad.append("computer %d army never seen" % r)
        built += int(ta[r].get("units_built", 0))
if built <= 0: bad.append("the computers built nothing")
nb, ib = (int(hb[-1][0]), int(hb[-1][1])) if hb else (0, 0)
if not hb or halved < 2 or nb > 16 or ib < 300 or (nb - 1) * ib < ticks - 2 * ib:
    bad.append("capped: samples=%s interval=%s halved=%d" % (hb[-1][0] if hb else "-", hb[-1][1] if hb else "-", halved))
if ka != kc: bad.append("a second run recorded another timeline")
print("OK samples=%d interval=%d over %d ticks, rows=%d computers-built=%d capped=%d@%d halved=%d" % (n, interval, ticks, len(rows), built, nb, ib, halved) if not bad else "BAD " + "; ".join(bad[:6]))
PY
)
fi
case "$G542A" in
  OK*) ok "G542 the match is sampled over time: a skirmish against three computers recorded by the renderer, ${G542A#OK }; the harvest and kills never fall and end within the final table, no army sample passes its peak, and a second run of the seed recorded the same timeline" ;;
  *) bad "G542 the match timeline: ${G542A:-not run (no cnc_eyes or no SCM01EA here)}" ;;
esac

if [ -n "$GD_STANDALONE" ]; then
  echo "debrief gates: $PASS passed, $FAIL failed"
  exit $FAIL
fi
