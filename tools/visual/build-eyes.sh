#!/bin/sh
# Rebuild ONLY the renderer from a source tree, into a folder of its own, for testing.
#
#   tools/visual/build-eyes.sh <game source dir> <out dir> <full checkout>
#
# <full checkout> is a checkout where game/build.sh (or game/make-build.sh) has already
# run: its game/*.o objects, its build header, its engine headers and its SDL setup are
# reused, so only cnc_eyes.cpp and the headers it includes are compiled (about a minute).
# <game source dir> may be a sparse checkout holding only the game/ sources. Nothing is
# staged into any play folder: run the result with its working directory set to one,
# as tools/visual/modern-shots.py does. arm64 only, for speed; the shipped build is made
# by game/make-build.sh as usual.
set -e
SRC=$(cd "$1" && pwd)
OUT=$2
FULL=$(cd "$3" && pwd)
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
. "$FULL/tools/mac/deployment-target.sh"
opencnc3d_need_mac_sdl
HDR="$FULL/brain/vanilla/tiberiandawn"
[ -f "$HDR/dllinterface.h" ] || { echo "no engine headers under $HDR" >&2; exit 1; }
G="$FULL/game"
OBJS=""
for o in dosbar hud640 dosopt dossave dosmenu dosops doslobby dosmp enhanced_menu pngwrite logo3d \
         lockstep net_udp netmatch netbeacon roomcode; do
    [ -f "$G/$o.o" ] || { echo "missing $G/$o.o: run game/build.sh in $FULL first" >&2; exit 1; }
    OBJS="$OBJS $G/$o.o"
done
for o in "$G"/aud_*.o "$G"/mix*.o; do [ -f "$o" ] && OBJS="$OBJS $o"; done
clang++ -arch arm64 -std=c++14 -O2 -g \
    -fms-extensions -fdeclspec -D__int64="long long" -include "$G/opencnc3d_build.h" \
    -o "$OUT/cnc_eyes" "$SRC/cnc_eyes.cpp" $OBJS \
    -I"$HDR" -I"$SRC" -I"$FULL/audio" -I"$FULL/menu" \
    $(sdl2-config --cflags) $(sdl2-config --libs) \
    -Wl,-headerpad_max_install_names \
    -framework OpenGL -lz
sh "$FULL/tools/bundle-sdl.sh" "$OUT" cnc_eyes >/dev/null
echo "built $OUT/cnc_eyes"
