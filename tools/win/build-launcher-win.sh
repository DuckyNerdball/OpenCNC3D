#!/bin/sh
# Cross compile the Windows launcher, from the Mac, with the toolchain the game
# already uses.
#
#   tools/win/build-launcher-win.sh          -> build/win/OpenCNC3D.exe
#
# It reads launcher/sources.sh, the SAME list launcher/build.sh reads, so a file
# added on one platform cannot be forgotten on the other. That is repository rules rule 4
# expressed by construction rather than by a guard script: there is one list.
#
# THREE THINGS ARE DIFFERENT FROM THE GAME'S OWN WINDOWS BUILD, each for a reason:
#
#  1. -lwininet instead of libcurl. WinINet is a system DLL, so the launcher needs
#     no bundled SSL stack, trusts the certificate store Windows already trusts,
#     and adds nothing for the player to install. lnet.c holds both backends.
#  2. -mwindows, so double-clicking the launcher does NOT open a black console
#     window behind the dialog. The game is built console-subsystem on purpose
#     (its diagnostics and the whole gate suite talk on stdout); a launcher has no
#     such traffic in the case that matters, and launcher.c attaches to the parent
#     console when there is one, so --check from a cmd prompt still prints.
#  3. The icon and version resource are compiled in from the same tools/win/opencnc3d.rc
#     template the game uses, so the file Explorer shows carries the eagle and the
#     build number.
set -e
cd "$(dirname "$0")/../.."
ROOT=$(pwd)

# THE ONE HOST, refused otherwise, for the reason tools/win/build-win.sh gives: Windows
# builds are 64-bit only, and a 32-bit launcher would be refused by the packager anyway.
HOST=${OPENCNC3D_WIN_HOST:-x86_64-w64-mingw32}
case "$HOST" in
    x86_64-w64-mingw32) ;;
    *) echo "OPENCNC3D_WIN_HOST=$HOST is refused: Windows builds are 64-bit only, and the" >&2
       echo "only host is x86_64-w64-mingw32. Unset OPENCNC3D_WIN_HOST to use it." >&2
       exit 1 ;;
esac
SDK="${OPENCNC3D_WINSDK:-$HOME/.opencnc3d-winsdk}"
OUT="$ROOT/build/win"
OBJ="$OUT/obj-launcher"

SDL2_ROOT=$(ls -d "$SDK"/SDL2-*/"$HOST" 2>/dev/null | head -1)
[ -n "$SDL2_ROOT" ] || { echo "no SDL2 for $HOST in $SDK. Run tools/win/setup-toolchain.sh" >&2; exit 1; }
[ -f "$SDL2_ROOT/lib/libSDL2.a" ] || { echo "no static libSDL2.a in $SDL2_ROOT/lib. Run tools/win/setup-toolchain.sh" >&2; exit 1; }
[ -f "$SDK/lib/$HOST/libz.a" ] || { echo "no zlib for $HOST in $SDK. Run tools/win/setup-toolchain.sh" >&2; exit 1; }
command -v $HOST-gcc >/dev/null 2>&1 || { echo "no $HOST-gcc on PATH (brew install mingw-w64)" >&2; exit 1; }

# ONE ARCHITECTURE PER FOLDER, the same question tools/win/build-win.sh asks of the same
# file, build/win/.host. This script may be the first to run in a checkout whose build/win
# still holds 32-bit programs, so it empties them the same way, and the game build that
# follows finds the folder already marked and keeps the launcher this script wrote.
if [ "$(cat "$OUT/.host" 2>/dev/null)" != "$HOST" ]; then
    echo "== build/win was not built for $HOST: removing its programs, DLLs and objects"
    for f in "$OUT"/*.exe "$OUT"/*.dll; do
        [ -f "$f" ] || continue
        echo "   removed ${f##*/}"
        rm -f "$f"
    done
    rm -rf "$OUT/obj" "$OUT/obj-launcher"
fi
mkdir -p "$OUT"
printf '%s\n' "$HOST" > "$OUT/.host"
# No SDL2.dll in the folder: SDL2 is linked into the launcher (see the link below).
if [ -f "$OUT/SDL2.dll" ]; then
    rm -f "$OUT/SDL2.dll"
    echo "   removed build/win/SDL2.dll: SDL2 is linked into the programs, no DLL ships"
fi

# The update host, stamped in, exactly as the Mac build does it.
tools/launcher/make-config.sh

. launcher/sources.sh

mkdir -p "$OBJ"
CC=$HOST-gcc

INC="-I$ROOT/launcher -I$ROOT/game -I$ROOT/menu -I$ROOT/video \
     -I$SDL2_ROOT/include/SDL2 -I$SDK/include"
LIBDIRS="-L$SDL2_ROOT/lib -L$SDK/lib/$HOST"
# A POINTER CUT TO A NARROWER INTEGER IS AN ERROR, the same three -Werror switches every
# C compile in tools/win/build-win.sh carries, and the same search of the log behind
# them (run_cc below).
CWIDTH="-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=int-conversion"
CFLAGS="-O2 -Wall -Wextra $CWIDTH -std=gnu89 $INC"

# EVERY COMPILER MESSAGE IS KEPT, in build/win/launcher-compile.log, and searched for a
# pointer truncation; tools/win/build-win.sh explains each part of this at its run_cc.
# In short: the compiler's own exit status decides (no pipe, because this is sh with
# set -e and no pipefail, and behind a pipe a failed compile reports success), and
# LC_ALL=C because the patterns are GCC's English messages.
LOG="$OUT/launcher-compile.log"
: > "$LOG"
TRUNCATION='loses precision|cast from pointer to integer of different size|cast to pointer from integer of different size'
run_cc() {   # run_cc <compiler command...>
    _from=$(( $(wc -c < "$LOG") + 1 ))
    if LC_ALL=C "$@" 2>>"$LOG"; then _rc=0; else _rc=$?; fi
    tail -c +"$_from" "$LOG" >&2
    if [ "$_rc" -ne 0 ]; then
        echo "ERROR: the compiler failed (exit $_rc); its messages are in $LOG" >&2
        exit "$_rc"
    fi
    if grep -E "$TRUNCATION" "$LOG" >/dev/null; then
        echo "ERROR: a pointer is cut to a narrower integer (from $LOG):" >&2
        grep -nE "$TRUNCATION" "$LOG" >&2
        echo "       In a 64-bit program that loses the top half of an address. Cast through" >&2
        echo "       intptr_t or uintptr_t instead. This build stops here on purpose." >&2
        exit 1
    fi
}

echo "== compile"
# From inside launcher/, because the shared source list is written relative to it
# (../game/dosbar.c and the rest). Objects go to absolute paths so the build tree
# stays where build-win.sh puts everything else.
OBJS=""
for src in $LAUNCHER_SOURCES; do
    o="$OBJ/$(echo "$src" | sed 's|[./]|_|g').o"
    ( cd "$ROOT/launcher" && run_cc $CC $CFLAGS -c "$src" -o "$o" )
    OBJS="$OBJS $o"
done

echo "== resources"
# Same template, same substitution, same single source of truth for the number as
# tools/win/build-win.sh. An unsubstituted @VER@ reaching windres is a syntax
# error rather than a silent blank, which is why they are checked.
RCOBJ=""
if [ -f tools/launchers/opencnc3d.ico ]; then
    VER_COMMA=$(sh "$ROOT/tools/version.sh" --rcversion)
    VER_STRING=$(tr -d '[:space:]' < "$ROOT/VERSION")
    sed -e "s/@VER_COMMA@/$VER_COMMA/g" -e "s/@VER_STRING@/$VER_STRING/g" \
        -e "s/@EXE_NAME@/OpenCNC3D.exe/g" \
        tools/win/opencnc3d.rc > "$OBJ/opencnc3d.rc"
    # A whole placeholder, @NAME@, not the string "@VER": opencnc3d.rc's own header
    # comment contains the phrase "the two @VER placeholders", so a loose match fails
    # every build on a comment that is doing its job.
    grep -qE '@[A-Z][A-Z_]*@' "$OBJ/opencnc3d.rc" \
        && { echo "a placeholder in tools/win/opencnc3d.rc was not substituted" >&2; exit 1; }
    cp tools/launchers/opencnc3d.ico "$OBJ/"
    if $HOST-windres "$OBJ/opencnc3d.rc" -O coff -o "$OBJ/res.o" 2>"$OBJ/windres.err"; then
        RCOBJ="$OBJ/res.o"
        echo "   icon + version $VER_STRING compiled in"
    else
        echo "WARNING: windres failed; the launcher will ship without its icon" >&2
        sed 's/^/  windres: /' "$OBJ/windres.err" >&2
    fi
else
    echo "WARNING: tools/launchers/opencnc3d.ico is missing; no icon" >&2
fi

echo "== manifest"
# THE LAUNCHER'S NARROW STRINGS ARE UTF-8. Every path it handles goes through a "char"
# API: GetModuleFileNameA for its own folder, CreateFileA for the install lock, and the
# C runtime's fopen, rename, remove and stat for the update. Windows reads all of them in
# the process code page, and the installer's default folder is inside the player's own
# user folder, so a user name that code page cannot spell (a Polish name on a Western
# European machine, a Cyrillic or Japanese one on most) gave the launcher a path it could
# not open again, and an update then failed as "could not lock the game folder". This
# manifest makes the process code page UTF-8 on Windows 10 version 1903 and later, which
# every one of those calls follows with no change to the code. Earlier Windows ignores
# the element and keeps the old limit. It declares nothing else, so the rest of how
# Windows treats the program (compatibility mode, DPI, file virtualization) is what it
# was without a manifest.
cat > "$OBJ/launcher.manifest" <<'XML'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
    </windowsSettings>
  </application>
</assembly>
XML
# Resource 1 of type 24 (RT_MANIFEST) is the one Windows reads when it starts a process.
printf '1 24 "launcher.manifest"\n' > "$OBJ/manifest.rc"
( cd "$OBJ" && $HOST-windres manifest.rc -O coff -o manifest.o )

echo "== link"
# EVERYTHING IS LINKED IN, SDL2 included, the same line tools/win/build-win.sh explains
# at length: -static to the driver folds in the GCC runtime and winpthread, and the static
# libSDL2.a brings its own private Windows libraries with it (the SDK's sdl2.pc lists
# them). So the launcher is one file. That matters more for the launcher than for the
# game: it is the program that replaces the others, and it runs from a folder where an
# older install may have left a 32-bit SDL2.dll that a 64-bit launcher could not load.
LIBS="-static -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lwininet -lz -lm \
      -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 \
      -lshell32 -lsetupapi -lversion -luuid"
# Loaded anywhere in the 64-bit space; tools/win/build-win.sh says why at LDSEC.
LDSEC="-Wl,--dynamicbase -Wl,--high-entropy-va -Wl,--nxcompat"

run_cc $CC -mwindows $LDSEC -o "$OUT/OpenCNC3D.exe" $OBJS $RCOBJ "$OBJ/manifest.o" $LIBDIRS $LIBS

# Asked of the binary, not of the build: a launcher without it still links and runs,
# and fails only on the machines the manifest is for.
grep -aq '>UTF-8</activeCodePage>' "$OUT/OpenCNC3D.exe" || {
    echo "the linked launcher carries no UTF-8 code page manifest" >&2
    exit 1
}

# AND IT IS A 64-BIT PROGRAM, laid out to load anywhere, that loads no SDL2.dll:
# check-pe-arch.py reads its import table too. Only the file this script wrote is asked;
# the rest of build/win is tools/win/build-win.sh's.
python3 "$ROOT/tools/win/check-pe-arch.py" "$OUT/OpenCNC3D.exe" || {
    echo "the linked launcher is not a 64-bit Windows program laid out as required, or imports SDL2.dll" >&2
    exit 1
}

ls -la "$OUT/OpenCNC3D.exe"
echo "built $OUT/OpenCNC3D.exe"
