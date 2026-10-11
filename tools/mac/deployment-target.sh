#!/bin/sh
# THE OLDEST macOS THIS PROJECT'S BINARIES RUN ON. Sourced, not executed:
#
#     . tools/mac/deployment-target.sh      (from app/build.sh, game/build.sh, ...)
#
# WHY THIS EXISTS (measured, true of every macOS build ever cut).
#
# No build script here ever set a deployment target, so clang stamped LC_BUILD_VERSION
# with whatever SDK it found on the machine that compiled. On a current macOS SDK that is
#
#     minos 26.0    sdk 26.2
#
# on app/opencnc3d, game/cnc_eyes AND the brain, and dyld REFUSES to load a Mach-O whose
# minos is newer than the running system. So every Mac below macOS 26 rejected the
# binaries before main() ran -- which is most Macs, macOS 26 having shipped in late 2025.
# That sat directly on top of the SDL bundling work: bundling SDL makes a release
# playable on a Mac that is not this one, and a 26.0 floor made sure it was not.
#
# WHY 14.0 AND NOT LOWER. The Homebrew SDL libraries we bundle beside the binaries
# (tools/bundle-sdl.sh) themselves declare minos 14.0:
#
#     otool -l playable/libSDL2-2.0.0.dylib | grep -A3 LC_BUILD_VERSION   ->  minos 14.0
#     otool -l playable/libSDL3.dylib       | grep -A3 LC_BUILD_VERSION   ->  minos 14.0
#
# A binary whose floor is below its own dependencies' floor has a floor on paper only:
# dyld would accept our Mach-O on macOS 12 and then refuse the SDL beside it. 14.0 is
# therefore the lowest number reachable without ALSO re-sourcing or rebuilding SDL, which
# changes what ships and is a project decision rather than a flag.
#
# IT IS ALL THREE BINARIES OR NONE. opencnc3d, cnc_eyes and TiberianDawn.dylib all load in
# one process; two of them fixed and the third left at 26.0 still refuses to launch and
# merely looks fixed. game/make-build.sh asserts the floor over the whole play folder
# after it is assembled, so this cannot drift back one binary at a time.
#
# MACOSX_DEPLOYMENT_TARGET is read by clang and by cc directly. CMake reads
# CMAKE_OSX_DEPLOYMENT_TARGET instead, and initialises it from this same variable when
# the cache does not already hold one, which is why tools/mac/build-brain-mac.sh passes
# it explicitly rather than relying on inheritance through an existing build directory.
OPENCNC3D_MACOS_MIN=14.0
export OPENCNC3D_MACOS_MIN
MACOSX_DEPLOYMENT_TARGET="$OPENCNC3D_MACOS_MIN"
export MACOSX_DEPLOYMENT_TARGET

# ------------------------------------------------------------------ both CPUs, one build
# EVERY MAC BINARY IS UNIVERSAL: arm64 for Apple Silicon and x86_64 for Intel, in one
# file, so one Mac package runs natively on either. The build machine's own CPU does not
# decide what ships. That is only true if EVERYTHING in the process carries both slices:
# opencnc3d, cnc_eyes, the brain dylibs and both SDL libraries load together, and a process
# that finds one of them without its slice dies in dyld before main(). game/make-build.sh
# checks every Mach-O in the play folder for both after it is assembled.
#
# OPENCNC3D_ARCH is the flag list for cc / c++ / clang++. OPENCNC3D_CMAKE_ARCHS is the same list
# in CMake's form. Overriding OPENCNC3D_MAC_ARCHS (say, to "arm64") gives a quicker one-slice
# development build; game/make-build.sh then reports the play folder as not universal.
OPENCNC3D_MAC_ARCHS="${OPENCNC3D_MAC_ARCHS:-arm64 x86_64}"
OPENCNC3D_ARCH=""
OPENCNC3D_CMAKE_ARCHS=""
for opencnc3d_a in $OPENCNC3D_MAC_ARCHS; do
    OPENCNC3D_ARCH="$OPENCNC3D_ARCH -arch $opencnc3d_a"
    OPENCNC3D_CMAKE_ARCHS="${OPENCNC3D_CMAKE_ARCHS:+$OPENCNC3D_CMAKE_ARCHS;}$opencnc3d_a"
done
unset opencnc3d_a
export OPENCNC3D_MAC_ARCHS OPENCNC3D_ARCH OPENCNC3D_CMAKE_ARCHS

# THE SDL THE MAC BUILD LINKS is the universal one tools/mac/setup-sdl-mac.sh builds, not
# Homebrew's: a Homebrew bottle carries only the build machine's CPU. Its sdl2-config is put
# first on PATH here, so every `$(sdl2-config ...)` in the build scripts means it.
OPENCNC3D_MACSDK="${OPENCNC3D_MACSDK:-$HOME/.opencnc3d-macsdk}"
export OPENCNC3D_MACSDK
case ":$PATH:" in
    *":$OPENCNC3D_MACSDK/bin:"*) ;;
    *) [ -d "$OPENCNC3D_MACSDK/bin" ] && PATH="$OPENCNC3D_MACSDK/bin:$PATH" && export PATH ;;
esac

# Called by the scripts that link SDL, before the first compile: refuse early, with the
# fix, rather than let the linker say "ignoring file ... found architecture 'x86_64',
# required architecture 'arm64'" forty lines into a build.
opencnc3d_need_mac_sdl() {
    opencnc3d_sdl="$(sdl2-config --prefix 2>/dev/null)/lib/libSDL2-2.0.0.dylib"
    # shellcheck disable=SC2086
    if [ ! -f "$opencnc3d_sdl" ] || ! lipo "$opencnc3d_sdl" -verify_arch $OPENCNC3D_MAC_ARCHS 2>/dev/null; then
        echo "FATAL: no SDL for $OPENCNC3D_MAC_ARCHS (sdl2-config found: ${opencnc3d_sdl#/lib/libSDL2-2.0.0.dylib})." >&2
        echo "       Run tools/mac/setup-sdl-mac.sh once on this machine; it builds a universal" >&2
        echo "       SDL into $OPENCNC3D_MACSDK, which the build then uses by itself." >&2
        exit 1
    fi
}

# A CMAKE BUILD DIRECTORY REMEMBERS THE TOOLS THAT CONFIGURED IT, by absolute path. One
# configured by another cmake (an Intel Homebrew's, on a build machine since moved to
# Apple Silicon) names a cmake and compilers that may no longer run, and one configured before
# the build went universal keeps its old architectures on objects it thinks are up to
# date. Either way the directory is thrown away and configured fresh; it holds nothing
# that is not rebuilt from source.
opencnc3d_cmake_fresh() {   # opencnc3d_cmake_fresh <build dir> [the cmake cache key that must hold $OPENCNC3D_CMAKE_ARCHS]
    opencnc3d_cache="$1/CMakeCache.txt"
    [ -f "$opencnc3d_cache" ] || return 0
    opencnc3d_was=$(sed -n 's/^CMAKE_COMMAND:INTERNAL=//p' "$opencnc3d_cache")
    opencnc3d_now=$(command -v cmake)
    opencnc3d_why=""
    [ "$opencnc3d_was" = "$opencnc3d_now" ] || opencnc3d_why="configured by $opencnc3d_was, not $opencnc3d_now"
    if [ -z "$opencnc3d_why" ] && [ -n "${2:-}" ]; then
        grep -qx "$2:STRING=$OPENCNC3D_CMAKE_ARCHS" "$opencnc3d_cache" \
            || opencnc3d_why="configured for other architectures than $OPENCNC3D_CMAKE_ARCHS"
    fi
    if [ -n "$opencnc3d_why" ]; then
        echo "   $1 was $opencnc3d_why: configuring it fresh"
        rm -rf "$1"
    fi
}

# Every slice or refuse: opencnc3d_verify_archs <file> ...
opencnc3d_verify_archs() {
    for opencnc3d_f in "$@"; do
        # shellcheck disable=SC2086
        lipo "$opencnc3d_f" -verify_arch $OPENCNC3D_MAC_ARCHS 2>/dev/null || {
            echo "FATAL: $opencnc3d_f is $(lipo -archs "$opencnc3d_f" 2>/dev/null || echo 'not a Mach-O'), not $OPENCNC3D_MAC_ARCHS" >&2
            exit 1
        }
    done
}
