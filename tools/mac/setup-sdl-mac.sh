#!/bin/sh
# One-time setup of the SDL libraries the macOS build links and ships, built UNIVERSAL
# (arm64 + x86_64) so one Mac package runs on Apple Silicon and on Intel.
#
# WHY THIS EXISTS. The Mac build used to link whatever `sdl2-config` Homebrew put on the
# PATH. Homebrew builds for the machine it runs on only, so an Intel build machine could
# only ever make an Intel game, and an Apple Silicon one only an Apple Silicon game.
# Building SDL here from its release source, for both architectures in one pass, removes
# the build machine's CPU from the question.
#
# WHAT IT BUILDS, and why these two:
#   SDL3        the real library. Ships as libSDL3.dylib beside the binaries.
#   sdl2-compat the SDL2 API on top of SDL3, which is what the game is written against.
#               Ships as libSDL2-2.0.0.dylib. It loads SDL3 at run time by name, which is
#               why tools/bundle-sdl.sh has to put SDL3 beside it.
# The versions are the ones the earlier Homebrew-linked builds shipped, so the move to a
# universal build does not also change which SDL the game runs on.
#
# It installs OUTSIDE the repo on purpose (binaries stay out of git, and there can be
# more than one worktree):
#
#   tools/mac/setup-sdl-mac.sh            -> ~/.opencnc3d-macsdk
#   OPENCNC3D_MACSDK=/some/where tools/mac/setup-sdl-mac.sh
#
# Needs cmake and ninja on the PATH (brew install cmake ninja). Safe to re-run: a
# finished install is left alone.
set -e

SDK="${OPENCNC3D_MACSDK:-$HOME/.opencnc3d-macsdk}"
SDL3_VER=3.4.14
SDL2C_VER=2.32.70
SDL3_SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
SDL2C_SHA256=998fa62557eb46ffe7e5c3e2c123bc332f7df9d9f593b3ceed88ed1158428a44
ARCHS="arm64;x86_64"

. "$(cd "$(dirname "$0")" && pwd)/deployment-target.sh"

fetch() {   # fetch <url> <file> <expected-sha256>
    [ -f "$2" ] || curl -fL --retry 3 -o "$2" "$1"
    got=$(shasum -a 256 "$2" | cut -d" " -f1)
    if [ "$got" != "$3" ]; then
        echo "DOWNLOAD IS NOT WHAT IT SHOULD BE: $2" >&2
        echo "  from     $1" >&2
        echo "  expected $3" >&2
        echo "  got      $got  ($(wc -c < "$2" | tr -d " ") bytes)" >&2
        echo "  If this is a genuine upstream version bump, update the SHA above." >&2
        rm -f "$2"
        exit 1
    fi
}

# Done already, and for both architectures? A half-finished earlier run, or an install
# from before this script built universal, is rebuilt rather than trusted.
universal() { lipo "$1" -verify_arch arm64 x86_64 2>/dev/null; }
if universal "$SDK/lib/libSDL3.0.dylib" && universal "$SDK/lib/libSDL2-2.0.0.dylib" \
   && codesign -v "$SDK/lib/libSDL3.0.dylib" 2>/dev/null \
   && codesign -v "$SDK/lib/libSDL2-2.0.0.dylib" 2>/dev/null \
   && [ -x "$SDK/bin/sdl2-config" ]; then
    echo "setup-sdl-mac: $SDK already holds universal SDL $SDL3_VER + sdl2-compat $SDL2C_VER"
    exit 0
fi

for t in cmake ninja; do
    command -v $t >/dev/null 2>&1 || { echo "no $t on PATH. Install it with:  brew install cmake ninja" >&2; exit 1; }
done

mkdir -p "$SDK/src"
cd "$SDK/src"
fetch "https://github.com/libsdl-org/SDL/releases/download/release-$SDL3_VER/SDL3-$SDL3_VER.tar.gz" \
      "SDL3-$SDL3_VER.tar.gz" "$SDL3_SHA256"
fetch "https://github.com/libsdl-org/sdl2-compat/releases/download/release-$SDL2C_VER/sdl2-compat-$SDL2C_VER.tar.gz" \
      "sdl2-compat-$SDL2C_VER.tar.gz" "$SDL2C_SHA256"
rm -rf "SDL3-$SDL3_VER" "sdl2-compat-$SDL2C_VER" build-sdl3 build-sdl2compat
tar xf "SDL3-$SDL3_VER.tar.gz"
tar xf "sdl2-compat-$SDL2C_VER.tar.gz"

# The deployment target is the project's (deployment-target.sh), so the libraries never
# ask for a newer macOS than the binaries beside them.
cmake -S "SDL3-$SDL3_VER" -B build-sdl3 -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$OPENCNC3D_MACOS_MIN" \
    -DCMAKE_INSTALL_PREFIX="$SDK" \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
cmake --build build-sdl3
cmake --install build-sdl3

cmake -S "sdl2-compat-$SDL2C_VER" -B build-sdl2compat -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$OPENCNC3D_MACOS_MIN" \
    -DCMAKE_INSTALL_PREFIX="$SDK" \
    -DCMAKE_PREFIX_PATH="$SDK" \
    -DSDL2COMPAT_TESTS=OFF
cmake --build build-sdl2compat
cmake --install build-sdl2compat

# SIGNED, AD HOC. Apple Silicon refuses to load arm64 code that carries no signature at
# all, and cmake's install step rewrites the install names after the linker signed them,
# which leaves SDL3 with none: it loads on an Intel Mac and is killed on Apple Silicon. This
# is the same ad-hoc signature the linker puts on every arm64 binary, not a distribution
# signature, which needs a certificate. tools/bundle-sdl.sh copies SDL3 untouched, so what
# is signed here is what ships.
for f in "$SDK/lib/libSDL3.0.dylib" "$SDK/lib/libSDL2-2.0.0.dylib"; do
    universal "$f" || { echo "setup-sdl-mac: $f is not arm64 + x86_64" >&2; exit 1; }
    codesign -f -s - "$f"
    codesign -v "$f" || { echo "setup-sdl-mac: $f does not verify after signing" >&2; exit 1; }
done
echo "setup-sdl-mac: universal SDL $SDL3_VER + sdl2-compat $SDL2C_VER in $SDK"
echo "  the Mac build finds it there by itself (tools/mac/deployment-target.sh puts $SDK/bin first on PATH)"
