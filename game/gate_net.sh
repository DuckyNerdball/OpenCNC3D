#!/bin/sh
# game/gate_net.sh -- G517, the long network match, run by game/gates.sh.
#
# RUN STANDALONE:  sh game/gate_net.sh [play-folder]
#   The play folder defaults to ./playable beside this checkout and must hold cnc_eyes,
#   TiberianDawn.dylib and ra3d/RedAlert.dylib. Every copy of the game it starts is hidden
#   (OPENCNC3D_BACKGROUND), so nothing takes the screen. About four minutes on an idle Mac, eight on a loaded one.
#   Exit status is the number of failed blocks.
#
# Inside game/gates.sh the blocks below run with that suite's RUNDIR, OUT, ok and bad; run
# on their own they define the same three for themselves.

if ! command -v ok >/dev/null 2>&1; then
  GP_HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
  RUNDIR=${1:-"$GP_HERE/../playable"}
  RUNDIR=$(CDPATH= cd -- "$RUNDIR" && pwd) || { echo "FAIL: no play folder at $1" >&2; exit 1; }
  cd "$RUNDIR" || exit 1
  OUT=${OUT:-/dev/null}
  PASS=0; FAIL=0
  ok()  { echo "PASS  $1" | tee -a "$OUT"; PASS=$((PASS+1)); }
  bad() { echo "FAIL  $1" | tee -a "$OUT"; FAIL=$((FAIL+1)); }
  GP_STANDALONE=1
fi

# ---------------------------------------------------------------------------
# G517  A NETWORK MATCH LASTS. The symptom this guards against is a match between two
# machines, over a LAN or the internet, that ends for everybody after ten to fifteen minutes
# and drops every player on the end-of-match screen. A match runs at 15 to 18 turns a second,
# so that is 9000 to 16000 turns, and no other network gate gets past 9000 (the Red Alert
# net gate) or 2000 (G195). This one plays three matches over loopback for 16000 turns each,
# long enough for any counter, ring, timer or slow drift that ends a match at that age:
#
#   A  TIBERIAN DAWN, TWO PEOPLE WHO PLAY: GDI against Nod on SCM01EA, no computers. Each
#      deploys, builds a base, then for forty rounds trains soldiers and vehicles, selects
#      them, sends them across the map, assigns a control group, guards and scatters. The
#      orders cross the wire on every round.
#   B  RED ALERT, the same on SCM10EA, Greece against the USSR.
#   C  RED ALERT WITH TWO COMPUTERS, THROUGH A LOSSY LINK: the joiner reaches the host
#      through a stand-in relay that drops one datagram in twenty and holds every other one
#      for 20 to 50 ms, both ways, so the re-send and the requirement-by-number repair run thousands
#      of times over the match.
#
# Each must end on its own script's last line: both copies exit 0, both reach frame 16000,
# no desync alarm, no hash report differs between the two logs and at least a thousand
# agree, nothing ends the match (no NET|error, no NETLOST verdict, no peer gone silent).
# A and B also require at least a hundred orders received from the other machine, which is
# what makes them more than two worlds idling side by side. C requires the relay to have
# dropped at least five hundred datagrams.
#
# Measured before this gate existed, on the build it was written against: all three legs
# pass, and the live render loop (the `livebench` script verb) held two Tiberian Dawn and
# two Red Alert copies with two computers each in agreement for 1000 seconds of real time
# (18000 turns). So this gate does not reproduce that symptom; it shows that a match of this
# length between two copies on one machine does not produce it, and keeps that true.
# ---------------------------------------------------------------------------
gp_netlong_scripts() {   # gp_netlong_scripts FILE BUILDINGS UNITS LASTTICKS
  f=$1; b=$2; u=$3; last=$4
  printf 'tick 5\nlclickobj MCV:@me\nactclickobj MCV:@me\ntick 60\n' > "$f"
  for x in $b; do printf 'build %s\nready %s 3000\nstartplace %s\nplace auto\ntick 30\n' $x $x $x >> "$f"; done
  r=0
  while [ $r -lt 40 ]; do
    for x in $u; do printf 'build %s\n' $x >> "$f"; done
    printf 'base\n' >> "$f"
    for x in $u; do printf 'lclickobj %s:@me\norder %d %d\n' $x $((20 + r % 30)) $((20 + (r * 7) % 30)) >> "$f"; done
    printf 'group 1 2\nguard\nscatter\ntick 375\n' >> "$f"
    r=$((r + 1))
  done
  printf 'tick %s\nquit\n' "$last" >> "$f"
}

# gp_netlong_leg LEG PORT HOSTSCRIPT JOINSCRIPT JOINPORT FLAGS... -> sets GP_WHY ("" = pass).
# GP_HX and GP_JX are flags for the host alone and the joiner alone (each picks its own side).
gp_netlong_leg() {
  leg=$1 port=$2 hs=$3 js=$4 jport=$5; shift 5
  rm -f "$GPW/$leg-h.log" "$GPW/$leg-j.log"
  OPENCNC3D_BACKGROUND=1 ./cnc_eyes "$@" $GP_HX --host "$port" --script "$hs" > "$GPW/$leg-h.log" 2>&1 &
  hp=$!
  sleep 2
  OPENCNC3D_BACKGROUND=1 ./cnc_eyes "$@" $GP_JX --join 127.0.0.1 "$jport" --script "$js" > "$GPW/$leg-j.log" 2>&1 &
  jp=$!
  hrc=0; jrc=0
  wait $hp || hrc=$?
  wait $jp || jrc=$?
  GP_WHY=""
  for s in h j; do
    l="$GPW/$leg-$s.log"
    last=$(sed -n 's/^NETSYNC|frame=\([0-9]*\)|.*/\1/p' "$l" | tail -1)
    [ "${last:-0}" -ge 16000 ] || GP_WHY="$GP_WHY $s:last-sync-frame=${last:-none}(want>=16000)"
    n=$(grep -c '^NETDESYNC|' "$l"); [ "$n" = "0" ] || GP_WHY="$GP_WHY $s:desync-alarms=$n"
    n=$(grep -c -E '^NET\|error|^NET\|peer-silent|^NET\|self-stalled|^GAMEOVER\|NETLOST' "$l")
    [ "$n" = "0" ] || GP_WHY="$GP_WHY $s:match-broken=[$(grep -E '^NET\|error|^NET\|peer-silent|^NET\|self-stalled|^GAMEOVER\|NETLOST' "$l" | head -1)]"
  done
  # A SCRIPT THAT ENDS IS THE PASS, NOT ITS EXIT STATUS: a click on a unit that has already
  # died counts as a script failure and exits 1, which says nothing about the match. A
  # crash or a hang does not reach SCRIPT|end, and a signal exits above 1.
  for s in h j; do
    grep -q '^SCRIPT|end ' "$GPW/$leg-$s.log" || GP_WHY="$GP_WHY $s:script-never-ended"
  done
  [ "$hrc" -le 1 ] && [ "$jrc" -le 1 ] || GP_WHY="$GP_WHY exit=$hrc/$jrc"
  grep '^NETSYNC|' "$GPW/$leg-h.log" | sort -u > "$GPW/$leg-hs"
  grep '^NETSYNC|' "$GPW/$leg-j.log" | sort -u > "$GPW/$leg-js"
  GP_AGREE=$(comm -12 "$GPW/$leg-hs" "$GPW/$leg-js" | grep -c .)
  sort -t'|' -k2,2 "$GPW/$leg-hs" > "$GPW/$leg-hs2"
  sort -t'|' -k2,2 "$GPW/$leg-js" > "$GPW/$leg-js2"
  differ=$(join -t'|' -j2 "$GPW/$leg-hs2" "$GPW/$leg-js2" | awk -F'|' '$3 != $5' | grep -c .)
  [ "$differ" = "0" ] || GP_WHY="$GP_WHY differing-hash-reports=$differ"
  [ "$GP_AGREE" -ge 1000 ] || GP_WHY="$GP_WHY agreeing-hash-reports=$GP_AGREE(want>=1000)"
  GP_ORD_H=$(sed -n 's/^NET|leaving|.*|orders-in=\([0-9]*\)|.*/\1/p' "$GPW/$leg-h.log" | tail -1)
  GP_ORD_J=$(sed -n 's/^NET|leaving|.*|orders-in=\([0-9]*\)|.*/\1/p' "$GPW/$leg-j.log" | tail -1)
}

if [ -x ./cnc_eyes ] && [ -f TiberianDawn.dylib ] && [ -f ra3d/RedAlert.dylib ] && [ -s SCM01EA.pack ] \
   && [ -s ra3d/3d/SCM10EA.pack ] && command -v python3 >/dev/null 2>&1; then
  GPW=$(mktemp -d "${TMPDIR:-/tmp}/gp-netlong.XXXXXX")
  GP_HX=""; GP_JX=""
  TDF="--scen SCM01EA --pack SCM01EA.pack --cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --dir missions/ --content content/ --nosound --w 640 --h 400 --skirmish --credits 30000 --seed 4243"
  RAF="--ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dir ra3d/missions/ --dosdata ra3d/audio --nosound --w 640 --h 400 --scen SCM10EA --pack ra3d/3d/SCM10EA.pack --skirmish --credits 30000 --seed 4243"

  # A: Tiberian Dawn, GDI hosts and Nod joins
  # THE JOINER'S SCRIPT IS THE SHORTER ONE, so the joiner leaves first and the host plays
  # on alone for a moment. The other way round the joiner is left waiting for a turn the
  # departed host will never send and ends, correctly, on THE HOST LEFT THE GAME.
  gp_netlong_scripts "$GPW/a-h.txt" "NUKE NUKE PROC PYLE WEAP" "E1 MTNK E1 JEEP" 2030
  gp_netlong_scripts "$GPW/a-j.txt" "NUKE NUKE PROC HAND AFLD" "E1 LTNK E1 BGGY" 2000
  # The side is each machine's own choice, as in G195.
  GP_HX="--side gdi"; GP_JX="--side nod"
  gp_netlong_leg a 17661 "$GPW/a-h.txt" "$GPW/a-j.txt" 17661 $TDF --ai 0
  GP_HX=""; GP_JX=""
  WHY_A=$GP_WHY; AGREE_A=$GP_AGREE; ORD_A="$GP_ORD_H/$GP_ORD_J"
  [ "${GP_ORD_H:-0}" -ge 100 ] && [ "${GP_ORD_J:-0}" -ge 100 ] || WHY_A="$WHY_A orders-in=$ORD_A(want>=100 each)"

  # B: Red Alert, the same two players
  gp_netlong_scripts "$GPW/b-h.txt" "POWR PROC TENT WEAP POWR" "E1 1TNK E1 JEEP" 2030
  gp_netlong_scripts "$GPW/b-j.txt" "POWR PROC BARR WEAP POWR" "E1 3TNK E1 E2" 2000
  gp_netlong_leg b 17663 "$GPW/b-h.txt" "$GPW/b-j.txt" 17663 $RAF --ai 0
  WHY_B=$GP_WHY; AGREE_B=$GP_AGREE; ORD_B="$GP_ORD_H/$GP_ORD_J"
  [ "${GP_ORD_H:-0}" -ge 100 ] && [ "${GP_ORD_J:-0}" -ge 100 ] || WHY_B="$WHY_B orders-in=$ORD_B(want>=100 each)"

  # C: Red Alert with two computers, the joiner through a lossy stand-in relay
  cat > "$GPW/lossy.py" <<'PYEOF'
import heapq, random, select, socket, sys, time
lp, hp, loss, delay, jitter, seed, until = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5]), int(sys.argv[6]), time.time() + float(sys.argv[7])
rng = random.Random(seed)
front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); front.bind(("127.0.0.1", lp))
back = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); back.bind(("127.0.0.1", 0))
host = ("127.0.0.1", hp); client = None; q = []; seen = 0; dropped = 0; seq = 0
while time.time() < until:
    now = time.time()
    r, _, _ = select.select([front, back], [], [], min(max(0.0, q[0][0] - now) if q else 0.05, 0.05))
    for s in r:
        data, addr = s.recvfrom(65536)
        if s is front: client = addr; out = (back, host)
        elif client is None: continue
        else: out = (front, client)
        seen += 1
        if rng.random() < loss: dropped += 1; continue
        seq += 1
        heapq.heappush(q, (time.time() + (delay + rng.random() * jitter) / 1000.0, seq, out[0], out[1], data))
    now = time.time()
    while q and q[0][0] <= now:
        _, _, s, a, data = heapq.heappop(q); s.sendto(data, a)
    if r:
        with open(sys.argv[8], "w") as f: f.write("LOSSY|seen=%d|dropped=%d\n" % (seen, dropped))
PYEOF
  printf 'tick 5\nlclickobj MCV:@me\nactclickobj MCV:@me\ntick 16030\nquit\n' > "$GPW/c-h.txt"
  printf 'tick 5\nlclickobj MCV:@me\nactclickobj MCV:@me\ntick 16000\nquit\n' > "$GPW/c-j.txt"
  python3 -I "$GPW/lossy.py" 17666 17665 0.05 20 30 7 900 "$GPW/c-proxy.txt" &
  GPP=$!
  sleep 1
  # The host's seed is the match's, and on this one the two computers have not yet overrun
  # two players who only deployed by turn 16000. On a seed where they have, the match ends
  # on an ordinary verdict before the length this gate is about.
  gp_netlong_leg c 17665 "$GPW/c-h.txt" "$GPW/c-j.txt" 17666 $RAF --ai 2 --credits 10000 --seed 4242
  kill $GPP 2>/dev/null; wait $GPP 2>/dev/null
  WHY_C=$GP_WHY; AGREE_C=$GP_AGREE
  DROP_C=$(sed -n 's/^LOSSY|seen=[0-9]*|dropped=\([0-9]*\)$/\1/p' "$GPW/c-proxy.txt" 2>/dev/null)
  [ "${DROP_C:-0}" -ge 500 ] || WHY_C="$WHY_C relay-dropped=${DROP_C:-none}(want>=500)"
  [ -n "${GP_KEEP:-}" ] && echo "G517: logs kept in $GPW" || rm -rf "$GPW"
else
  WHY_A=" no play folder (cnc_eyes, both brains, SCM01EA.pack, ra3d/3d/SCM10EA.pack and python3 are needed)"
  WHY_B="$WHY_A"; WHY_C="$WHY_A"; AGREE_A=0; AGREE_B=0; AGREE_C=0; ORD_A=0; ORD_B=0; DROP_C=0
fi
if [ -z "$WHY_A$WHY_B$WHY_C" ]; then
  ok "G517 a network match lasts: three loopback matches of 16000 turns, past the ten to fifteen minutes at which a match must not end for everybody, each ended on its own last line with both copies agreeing to the end. A Tiberian Dawn GDI against Nod with orders crossing every round ($AGREE_A hash reports agree, orders received $ORD_A); B Red Alert the same ($AGREE_B agree, orders $ORD_B); C Red Alert with two computers through a relay dropping one datagram in twenty and delaying the rest 20 to 50 ms ($AGREE_C agree, $DROP_C datagrams dropped). No desync alarm, no differing report, no NET|error, no silent peer, no NETLOST verdict"
else
  bad "G517 a network match lasts: A[$WHY_A ] B[$WHY_B ] C[$WHY_C ] (GP_KEEP=1 keeps the logs)"
fi

if [ -n "${GP_STANDALONE:-}" ]; then
  echo "game/gate_net.sh: $PASS passed, $FAIL failed"
  exit "$FAIL"
fi
