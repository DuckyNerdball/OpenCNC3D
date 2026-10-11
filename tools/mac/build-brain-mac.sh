#!/bin/sh
# Build the bootstrap brains for macOS: TiberianDawn.dylib and RedAlert.dylib.
#
#   tools/mac/build-brain-mac.sh [DEST ...]
#
# With no argument it writes game/TiberianDawn.dylib, the copy the play folder is
# assembled from; each extra argument is another folder to drop that finished library in.
# RedAlert.dylib remains in the CMake build directory until RA3D has a module package, so
# this bootstrap cannot silently add a second, temporary brain to the OpenCNC3D release.
#
# WHY THIS SCRIPT EXISTS. The Windows half of the brain has had a driver since the cross
# build was written (tools/win/build-win.sh, the DO_BRAIN block), but the macOS half was
# only ever a set of commands in brain/README.md and a copy of the same commands in the
# CI workflow, run by hand and then hand copied into game/ and playable/. Two things
# followed from that. Nothing could set a compiler flag for the shipped library, and the
# library that shipped was whatever was last copied rather than whatever the checkout
# says. The first one is what this script was written for.
#
# WHAT IT SETS AND WHY: CMAKE_OSX_DEPLOYMENT_TARGET, from the one number in
# tools/mac/deployment-target.sh. Without it the brain was stamped LC_BUILD_VERSION
# minos 26.0 exactly like the two binaries beside it, and all three load in ONE process:
# a opencnc3d fixed to 14.0 that dlopens a 26.0 TiberianDawn.dylib still fails on any Mac
# below macOS 26, it just fails a few milliseconds later. Both programs and both brains
# must share the same floor.
#
# Passed explicitly rather than left to the MACOSX_DEPLOYMENT_TARGET environment
# variable, because CMake only reads that variable when it INITIALISES the cache. A
# build-native directory configured earlier already holds an empty
# CMAKE_OSX_DEPLOYMENT_TARGET, and an inherited environment would not have overridden it.
# That is the silent-half-fix this whole item is about.
set -e
cd "$(dirname "$0")/../.."
ROOT=$(pwd)

. tools/mac/deployment-target.sh

[ -f brain/vanilla/CMakeLists.txt ] || {
    echo "no brain checkout at brain/vanilla. It is COMMITTED SOURCE (a project rule;" >&2
    echo "brain/README.md), not something to clone, so a missing one means a broken" >&2
    echo "checkout rather than a missing step." >&2
    exit 1
}

BUILD=brain/vanilla/build-native
echo "== brains (TiberianDawn.dylib + RedAlert.dylib), deployment target $OPENCNC3D_MACOS_MIN, $OPENCNC3D_MAC_ARCHS"
opencnc3d_cmake_fresh "$BUILD" CMAKE_OSX_ARCHITECTURES
cmake -S brain/vanilla -B "$BUILD" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="$OPENCNC3D_MACOS_MIN" \
      -DCMAKE_OSX_ARCHITECTURES="$OPENCNC3D_CMAKE_ARCHS" \
      -DBUILD_REMASTERTD=ON -DBUILD_VANILLATD=OFF -DBUILD_VANILLARA=OFF \
      -DBUILD_REMASTERRA=ON -DSDL2=OFF -DOPENAL=OFF -DNETWORKING=OFF >/dev/null
cmake --build "$BUILD" --target TiberianDawn RedAlert \
      -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)" >/dev/null

TD_LIB="$BUILD/tiberiandawn/TiberianDawn.dylib"
RA_LIB="$BUILD/redalert/RedAlert.dylib"
for lib in "$TD_LIB" "$RA_LIB"; do
    [ -f "$lib" ] || {
        echo "cmake reported success but $lib is not there." >&2
        echo "Vanilla Conquer's feature summary can disagree with its target list;" >&2
        echo "inspect the CMake configure output and generated targets." >&2
        exit 1
    }
done

# PROVE THE FLOOR, do not assume it. Changing CMAKE_OSX_DEPLOYMENT_TARGET in an existing
# cache is exactly the case where a stale object file can survive a rebuild, and a brain
# that is still 26.0 fails in dyld on the machine that downloads it rather than here.
for lib in "$TD_LIB" "$RA_LIB"; do
    MIN=$(otool -l "$lib" | awk '/LC_BUILD_VERSION/{f=1} f&&/^ *minos/{print $2; exit}')
    [ "$MIN" = "$OPENCNC3D_MACOS_MIN" ] || {
        echo "$(basename "$lib") came out with minos ${MIN:-none}, not $OPENCNC3D_MACOS_MIN." >&2
        echo "Delete $BUILD and run this again: a cache configured without a deployment" >&2
        echo "target keeps the old compile flags on objects it thinks are up to date." >&2
        exit 1
    }
    echo "   $(basename "$lib"): minos $MIN"
    opencnc3d_verify_archs "$lib"
    echo "   $(basename "$lib"): $(lipo -archs "$lib")"
done

# game/ IS ALWAYS WRITTEN, and each argument is an ADDITIONAL folder. That is what the
# header above has always promised -- "with no argument it writes game/TiberianDawn.dylib
# ... each extra argument is another folder to drop the finished library in" -- but the
# code used to read "$#" > 0 and REPLACE the list, so passing any destination silently
# skipped game/. On 24 Aug 2026 that shipped a freshly built brain to playable/ and app/
# while game/ kept a copy four hours old, and G30 caught it as a stale binary. Nobody
# calling this wants game/ left behind; the code was the bug, not the comment.
DESTS="game $*"
for d in $DESTS; do
    [ -d "$d" ] || { echo "no such folder: $d" >&2; exit 1; }
    cp "$TD_LIB" "$d/TiberianDawn.dylib"
    echo "   -> $d/TiberianDawn.dylib"
done

echo "   RA bootstrap -> $RA_LIB"
