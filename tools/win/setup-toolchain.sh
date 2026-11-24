#!/bin/sh
# One-time setup of the Windows cross-compile toolchain on macOS.
#
# WHY THIS EXISTS AS A SCRIPT AND NOT AS INSTRUCTIONS IN A DOC.
# The Windows half of every build has to be as reproducible as the Mac half, and a doc
# that says "download SDL2 somewhere" produces a different toolchain on every machine.
# This puts the two third-party SDKs mingw-w64 does not ship (SDL2 and zlib) in one
# known place, pinned to one version, so build-win.sh can just use them.
#
# It installs OUTSIDE the repo on purpose: the SDK is binaries, project rule 2 keeps
# binaries out of git, and there can be more than one worktree of this repo checked out.
#
#   ./setup-toolchain.sh          -> ~/.opencnc3d-winsdk
#   OPENCNC3D_WINSDK=/some/where ./setup-toolchain.sh
#
# Safe to re-run: anything already unpacked is left alone.
set -e

SDK="${OPENCNC3D_WINSDK:-$HOME/.opencnc3d-winsdk}"
# SDL2 IS LINKED INTO EVERY WINDOWS PROGRAM, from the static libSDL2.a in the package below,
# so no SDL2.dll ships and there is no DLL to pin. That is deliberate. A DLL beside the
# programs has to match their width, and the launchers already on players' machines keep
# whatever SDL2.dll sits beside them: a game update that replaced it would leave them
# beside a DLL they cannot load. With SDL inside each program, every program carries the
# SDL it was built with, and a new SDL2_VER reaches players inside the programs built
# from it. The archive itself is pinned by content below (SDL2_SHA256).
SDL2_VER=2.32.8
ZLIB_VER=1.3.1

# Pinned by content, not just by version. A download that silently returns something
# other than the archive is the failure this catches: zlib.net answered a CI runner with
# an 11,962-byte HTML page instead of the tarball, and the only symptom was
# "gzip: stdin: not in gzip format" three lines later, which says nothing about why.
SDL2_SHA256=f590b3707689562483ded05bf15eaf0f5d33eb97f49d0fd3b894225fe17cc52b
ZLIB_SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23

# sha256sum on Linux, shasum -a 256 on macOS. Both exist in CI and on a current macOS SDK.
sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d" " -f1
    else shasum -a 256 "$1" | cut -d" " -f1; fi
}
fetch() {   # fetch <url> <file> <expected-sha256>
    [ -f "$2" ] || curl -fL --retry 3 -o "$2" "$1"
    got=$(sha256_of "$2")
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

# THE WINDOWS BUILD IS 64-BIT ONLY (x86_64, PE32+). 32-bit Windows is not supported.
# OPENCNC3D_WIN_HOSTS=i686-w64-mingw32 still fetches the 32-bit SDK, which only the
# historical diagnostics under tools/diagnostics/windows-performance read.
HOSTS="${OPENCNC3D_WIN_HOSTS:-x86_64-w64-mingw32}"

for h in $HOSTS; do
    command -v $h-gcc >/dev/null 2>&1 || {
        echo "no $h-gcc on PATH. Install it with:  brew install mingw-w64" >&2
        exit 1
    }
done

mkdir -p "$SDK/src"
cd "$SDK/src"

# ---- SDL2 -----------------------------------------------------------------------
# The mingw development package. Its x86_64-w64-mingw32 half holds the headers and the
# static libSDL2.a every Windows program links. The package also carries an i686 half and
# an SDL2.dll for each width; they are unpacked with the rest and no build uses them.
if [ ! -d "$SDK/SDL2-$SDL2_VER" ]; then
    echo "fetching SDL2 $SDL2_VER (mingw devel)"
    fetch "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-devel-$SDL2_VER-mingw.tar.gz" \
          "SDL2-devel-$SDL2_VER-mingw.tar.gz" "$SDL2_SHA256"
    tar xf "SDL2-devel-$SDL2_VER-mingw.tar.gz" -C "$SDK"
fi

# ---- zlib -----------------------------------------------------------------------
# mingw-w64 does not ship zlib and the renderer links -lz for the PNG writer. Built
# from source as a static library so the .exe carries it and there is one less DLL
# beside the binary on Windows.
for HOST in $HOSTS; do
    [ -f "$SDK/lib/$HOST/libz.a" ] && continue
    echo "building zlib $ZLIB_VER for $HOST"
    # madler/zlib on GitHub, NOT zlib.net. zlib.net answered the CI runner with an HTML
    # page rather than the tarball, and the release asset is the copy that is actually
    # reachable from everywhere.
    fetch "https://github.com/madler/zlib/releases/download/v$ZLIB_VER/zlib-$ZLIB_VER.tar.gz" \
          "zlib-$ZLIB_VER.tar.gz" "$ZLIB_SHA256"
    rm -rf "zlib-$ZLIB_VER"
    tar xf "zlib-$ZLIB_VER.tar.gz"
    cd "zlib-$ZLIB_VER"
    # zlib's ./configure does not cross-compile. Its own win32 makefile does, and the
    # PREFIX variable is the documented way to point it at a cross toolchain. Only the
    # static library is asked for, so the .exe carries zlib and there is one less DLL to
    # ship beside the executable.
    make -f win32/Makefile.gcc libz.a PREFIX=$HOST- -j4
    mkdir -p "$SDK/lib/$HOST" "$SDK/include"
    cp libz.a "$SDK/lib/$HOST/"
    cp zlib.h zconf.h "$SDK/include/"
    cd ..
done

echo
echo "Windows SDK ready in $SDK"
for HOST in $HOSTS; do
    echo "  $HOST"
    echo "    SDL2   $SDK/SDL2-$SDL2_VER/$HOST"
    echo "    zlib   $SDK/lib/$HOST/libz.a"
done
