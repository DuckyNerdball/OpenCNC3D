#!/bin/sh
# Frame cost of a built-up base, for measuring the cartridge model draw (draw_mesh).
#
#   sh tools/perf/mesh-bench.sh [playable folder] [scenario] [width] [height]
#
# Defaults: the playable/ folder of this checkout (what game/make-build.sh fills), SCG30EA, 1600x960. Runs the game's own script harness under
# Enhanced with the experimental art on, lets the mission play 600 ticks so the base is
# standing, then prints FRAMEBENCH lines (median and 90th percentile of draw_frame plus
# glFinish, in ms) zoomed in and zoomed out, with the shroud on and off. The shroud run
# matters: the per-vertex shroud lookup is part of what draw_mesh pays.
#
# The clock is the wall clock and vsync does not apply (framebench calls draw_frame
# directly, never the swap), so compare only runs made one after the other on an idle
# machine, and the screen must be unlocked or the window never gets a GL context.
set -eu
REPO=$(cd "$(dirname "$0")/../.." && pwd)
DIR=${1:-$REPO/playable}
SCEN=${2:-SCG30EA}
W=${3:-1600}
H=${4:-960}
cd "$DIR"
S=$(mktemp -t meshbench)
cat > "$S" <<SCRIPT
gfx experimental_textures 1
tick 600
zoom min
framebench 120
zoom max
framebench 120
SCRIPT
BASE="--cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --dir missions/ --content content/"
for SH in "" "--noshroud"; do
    echo "== $SCEN ${W}x${H} ${SH:-shroud on}"
    sh -c "./cnc_eyes --scen $SCEN --pack $SCEN.pack $BASE --nosound --w $W --h $H \
        --script $S --enhanced $SH" 2>/dev/null | grep FRAMEBENCH
done
rm -f "$S"
