#!/bin/sh
# Build the Windows half: opencnc3d-game.exe, cnc_eyes.exe and the brain DLLs beside them.
#
#   tools/win/build-win.sh              -> build/win/
#   tools/win/build-win.sh --no-brain   -> skip the brain (it changes rarely and is slow)
#
# Run tools/win/setup-toolchain.sh once first.
#
#   OPENCNC3D_WIN_JOBS=1 tools/win/build-win.sh   -> the brain compiled one file at a time
#
# 64-BIT ONLY. Every program and DLL this script writes is x86-64 (PE32+), and any other
# compiler host is refused below. The engine's pointers already live at 64-bit addresses
# on macOS. What a 64-bit Windows build adds is one trap, and it is guarded here rather
# than trusted: -fpermissive, which the Remaster header needs, turns GCC's "cast from
# pointer to a smaller type loses precision" from an error into a warning, and at 64 bits
# that cast is a real bug that works until something is allocated above 4 GB. So every
# compiler message is kept (build/win/compile.log, build/win/brain-build.log) and a pointer
# truncation in either stops the build. See BUILDING.md.
#
# WHY NO SOURCE WAS EDITED TO MAKE THIS WORK. Every file except game/cnc_eyes.cpp already
# carried its own `#ifdef __APPLE__` GL guard. The renderer did not, and it is the file
# most likely to be open in a concurrent edit, so compat/win/ supplies the three macOS-only
# headers it asks for by name instead. See compat/win/README.md.
set -e
cd "$(dirname "$0")/../.."
ROOT=$(pwd)

# THE ONE HOST. A 32-bit host would write 32-bit programs into a folder the packager and
# the release refuse, after minutes of compiling, so it is refused here, first.
HOST=${OPENCNC3D_WIN_HOST:-x86_64-w64-mingw32}
case "$HOST" in
    x86_64-w64-mingw32) ;;
    *) echo "OPENCNC3D_WIN_HOST=$HOST is refused: Windows builds are 64-bit only, and the" >&2
       echo "only host is x86_64-w64-mingw32. Unset OPENCNC3D_WIN_HOST to use it." >&2
       exit 1 ;;
esac

python3 "$ROOT/tools/bakery/materialize-grassmask.py"

# THE BUILD NUMBER, regenerated before anything compiles. tools/version.sh reads the
# VERSION file at the repo root and writes game/opencnc3d_build.h; nothing else in the tree
# is allowed to carry a version string, so there is no second copy to forget to bump.
# tools/release.sh has already written the header with the bare release number by
# the time it calls this, and regenerating it here would stamp the binary
# "+sha-dirty" for a build that is about to become the tag.
if [ -z "$OPENCNC3D_SKIP_VERSION_HEADER" ] || [ ! -f "$ROOT/game/opencnc3d_build.h" ]; then
    sh "$ROOT/tools/version.sh" --header "$ROOT/game/opencnc3d_build.h"
fi

SDK="${OPENCNC3D_WINSDK:-$HOME/.opencnc3d-winsdk}"
OUT="$ROOT/build/win"
DO_BRAIN=1
[ "$1" = "--no-brain" ] && DO_BRAIN=0

SDL2_ROOT=$(ls -d "$SDK"/SDL2-*/"$HOST" 2>/dev/null | head -1)
[ -n "$SDL2_ROOT" ] || { echo "no SDL2 for $HOST in $SDK. Run tools/win/setup-toolchain.sh" >&2; exit 1; }
[ -f "$SDL2_ROOT/lib/libSDL2.a" ] || { echo "no static libSDL2.a in $SDL2_ROOT/lib. Run tools/win/setup-toolchain.sh" >&2; exit 1; }
[ -f "$SDK/lib/$HOST/libz.a" ] || { echo "no zlib for $HOST in $SDK. Run tools/win/setup-toolchain.sh" >&2; exit 1; }
command -v $HOST-gcc >/dev/null 2>&1 || { echo "no $HOST-gcc on PATH (brew install mingw-w64)" >&2; exit 1; }

# --no-brain KEEPS THE BRAIN ALREADY IN build/win, so that brain is asked first. A checkout
# that built the 32-bit Windows half still holds 32-bit brain DLLs there, and a 64-bit game
# beside one cannot load it (Windows error 193): the build would report success and hand
# over a game that stops at its first mission. Refused before anything is compiled.
if [ $DO_BRAIN -eq 0 ]; then
    for brain in TiberianDawn.dll RedAlert.dll; do
        if [ ! -f "$OUT/$brain" ]; then
            echo "NOTE: --no-brain and no build/win/$brain: the programs build, but cannot start" >&2
            echo "      a game from build/win until the brain is built (run without --no-brain)." >&2
            continue
        fi
        python3 "$ROOT/tools/win/check-pe-arch.py" "$OUT/$brain" >/dev/null || {
            echo "ERROR: --no-brain would keep build/win/$brain, which is not a 64-bit brain. A" >&2
            echo "       64-bit game cannot load it. Run tools/win/build-win.sh once without" >&2
            echo "       --no-brain to build the brain again for this architecture." >&2
            exit 1; }
    done
fi

# ONE ARCHITECTURE PER FOLDER. build/win outlives every build, and nothing used to empty
# it, so a checkout that built the 32-bit Windows half still holds 32-bit programs and
# DLLs there, including names no script writes any more. The packager refuses such a
# folder, and a program from it left beside a fresh one is exactly the mix error 193 is
# made of. So the host that filled the folder is written to build/win/.host, and when it
# is missing or names another host, the folder's programs, its DLLs (except a brain
# --no-brain has just accepted) and both object folders go before anything is compiled.
# tools/win/build-launcher-win.sh asks the same file the same question.
if [ "$(cat "$OUT/.host" 2>/dev/null)" != "$HOST" ]; then
    echo "== build/win was not built for $HOST: removing its programs, DLLs and objects"
    for f in "$OUT"/*.exe "$OUT"/*.dll; do
        [ -f "$f" ] || continue
        if [ $DO_BRAIN -eq 0 ]; then
            case "${f##*/}" in TiberianDawn.dll|RedAlert.dll) continue ;; esac
        fi
        echo "   removed ${f##*/}"
        rm -f "$f"
    done
    rm -rf "$OUT/obj" "$OUT/obj-launcher"
fi
mkdir -p "$OUT"
printf '%s\n' "$HOST" > "$OUT/.host"

# NO SDL2.dll IN THE FOLDER. SDL2 is linked into every program (see LIBS), so a copy here
# is a leftover from an older build, and nothing may package it.
if [ -f "$OUT/SDL2.dll" ]; then
    rm -f "$OUT/SDL2.dll"
    echo "   removed build/win/SDL2.dll: SDL2 is linked into the programs, no DLL ships"
fi

# EVERY COMPILER MESSAGE IS KEPT, AND A POINTER TRUNCATION STOPS THE BUILD.
#
# Every compile and link below goes through run_cc. Its messages are appended to
# build/win/compile.log and shown, the compiler's own exit status decides whether the
# build goes on, and then the whole log is searched for the three ways GCC reports a
# pointer cut to a narrower integer: C++ under -fpermissive ("loses precision", a warning
# there and an error everywhere else), and C ("cast from pointer to integer of different
# size", "cast to pointer from integer of different size"). One is enough to stop the
# build. C compiles also carry -Werror for those diagnostics and for int-conversion, so
# in C the compiler stops first and the search is the second line.
#
# NO PIPE between a compiler and the shell, ever. This is sh with set -e and no pipefail
# (CI runs it under dash), so "gcc ... | tee log" returns tee's status: a compile that
# FAILED would report success, and the object from the previous run, still in obj/, would
# be linked into the new program. The redirection keeps the compiler's own status.
#
# LC_ALL=C on every compile the search reads, because GCC translates its messages and the
# patterns are the English ones; a German or French build machine would otherwise find
# nothing and pass.
LOG="$OUT/compile.log"
: > "$LOG"
TRUNCATION='loses precision|cast from pointer to integer of different size|cast to pointer from integer of different size'
CWIDTH="-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=int-conversion"
truncation_guard() {   # truncation_guard <log>: stop the build on a pointer truncation in it
    if grep -E "$TRUNCATION" "$1" >/dev/null; then
        echo "ERROR: a pointer is cut to a narrower integer (from $1):" >&2
        grep -nE "$TRUNCATION" "$1" >&2
        echo "       In a 64-bit program that loses the top half of an address. Cast through" >&2
        echo "       intptr_t or uintptr_t instead. This build stops here on purpose." >&2
        exit 1
    fi
}
run_cc() {   # run_cc <compiler command...>
    _from=$(( $(wc -c < "$LOG") + 1 ))
    if LC_ALL=C "$@" 2>>"$LOG"; then _rc=0; else _rc=$?; fi
    tail -c +"$_from" "$LOG" >&2
    if [ "$_rc" -ne 0 ]; then
        echo "ERROR: the compiler failed (exit $_rc); its messages are in $LOG" >&2
        exit "$_rc"
    fi
    truncation_guard "$LOG"
}

# The anti-drift guard, first, before anything is compiled. See tools/win/sources.sh.
tools/win/check-sources.sh

. tools/win/sources.sh

HDR=""
for c in brain/vanilla/tiberiandawn; do
    [ -f "$c/dllinterface.h" ] && HDR="$c" && break
done
[ -n "$HDR" ] || { echo "cannot find dllinterface.h (clone Vanilla Conquer into brain/vanilla)" >&2; exit 1; }

CC=$HOST-gcc
CXX=$HOST-g++
OBJ="$OUT/obj"
mkdir -p "$OBJ"
# Every program and DLL this script writes, for the read-back at the end.
PRODUCED=""

# compat/win FIRST: it answers <OpenGL/gl.h> and <dlfcn.h> for the renderer. Then game/,
# for the same reason app/build.sh puts it first -- game/dosbar.c is a strict superset of
# the sidebar copy and dosmenu.c must see the wider DB_State.
INC="-I$ROOT/compat/win -I$ROOT/game -I$ROOT/menu -I$ROOT/video -I$ROOT/audio \
     -I$SDL2_ROOT/include/SDL2 -I$SDK/include"
LIBDIRS="-L$SDL2_ROOT/lib -L$SDK/lib/$HOST"
CDEFS="-DGL_SILENCE_DEPRECATION"
# A COOKED BUILD has no F5 panel (see app/build.sh). $CDEFS already reaches every object
# below, so this one line is the whole of the Windows half.
[ -n "$OPENCNC3D_COOKED" ] && CDEFS="$CDEFS -DOPENCNC3D_COOKED=1"
# NO BUILD MACHINE'S FOLDERS IN THE BINARIES. -g records the directory every object was
# compiled in, and __FILE__ in the engine's log lines spells out the same path, so a
# shipped .exe or .dll would otherwise carry the absolute path of whichever checkout and
# SDK folder built it, user name included. Mapped to "." for the tree and "winsdk" for
# the SDK, the debug information still names every source file, relative to the repo
# root, and two checkouts of one commit compile the same paths into their binaries.
PATHMAP="-ffile-prefix-map=$ROOT=. -ffile-prefix-map=$SDK=winsdk"
CDEFS="$CDEFS $PATHMAP"
OPENCNC3D_WIN_HOST="$HOST" python3 tools/map-archive/build.py windows
INC="$INC -I$ROOT/build-native/map-archive/cache/libarchive-3.8.10/libarchive"
CDEFS="$CDEFS -DOPENCNC3D_MAP_ARCHIVE_STATIC=1 -DLIBARCHIVE_STATIC"
ARCHIVE_LIBS="$ROOT/build-native/map-archive/windows/libmaparchive.a $ROOT/build-native/map-archive/windows/libmaplzma.a"

# NO CPU FLAGS, AND THAT IS THE 64-BIT DEFAULT DOING THE WORK. The 32-bit build needed
# -msse2 -mfpmath=sse to keep its floats off the old x87 unit, whose rounding-mode
# switches roughly doubled Enhanced frame times, and -mstackrealign because a 32-bit
# Windows callback arrives with the stack aligned to 4 bytes. x86-64 has neither problem:
# SSE2 is its floating point unit and its ABI keeps the stack 16-byte aligned. With
# -msse2 or -mfpmath=sse this compiler writes byte-identical code. The variable stays as
# the one place a CPU target would go (-march=x86-64-v2, say), which would be a measured
# decision of its own. The brain DLL is built separately and keeps its own flags.
WIN_CPU=""

echo "== C sources ($HOST)"
# THE C DIALECT SPLIT IS THE MAC BUILD'S, NOT A NEW ONE.
#
# game/build.sh holds the sidebar rasteriser, the 640x480 HUD, the Options dialog and the
# audio engine to -std=c89 on purpose: they carry no GL and no SDL and stay portable C.
# The split is kept here too, which also means this build is the first thing that would
# notice if someone broke it.
#
# The rest gets gnu99. Apple clang quietly accepts C99 declarations under gnu89 as an
# extension and GCC does not, so app/campaign.c (declarations inside `for`) is the one
# file where the Mac flag would not have crossed. That is a compiler difference, not a
# portability problem in the code.
COBJ=""
compile_c() {
    std=$1; shift
    for f in "$@"; do
        o="$OBJ/$(basename "$f" .c).o"
        run_cc $CC -std=$std -O2 -g $WIN_CPU -Wall $CWIDTH $CDEFS $INC -c "$f" -o "$o"
        COBJ="$COBJ $o"
    done
}
AUDIO_C89=$(echo $WIN_AUDIO_C | tr ' ' '\n' | grep -v audio_sdl.c | tr '\n' ' ')
compile_c c89   $WIN_GAME_C $AUDIO_C89
compile_c gnu99 audio/audio_sdl.c $WIN_MENU_C $WIN_VIDEO_C $WIN_APP_C
# The scheduler is strict C89; the socket file needs gnu89 for the winsock headers.
#
# NAMED ONE BY ONE, and that is a trap this build has already fallen into. Every other
# group above is compiled from its $WIN_* variable, so adding a file to tools/win/sources.sh
# is enough for it -- but these four carry three different C standards between them, so the
# variable is only ever READ by check-sources.sh and never compiled from. netbeacon.c was
# added to WIN_NET_C, passed the drift check on the strength of that, and then failed at
# the LINK with a dozen undefined nb_* symbols, because nothing here compiled it. If you
# add a net source, it needs a line here as well as a name there.
compile_c c89   net/lockstep.c
compile_c gnu89 net/net_udp.c
compile_c gnu89 net/netmatch.c
compile_c gnu89 net/netbeacon.c
# The room code is a relayed host's address, so it is part of the game and not a tool.
compile_c gnu89 net/roomcode.c

# The HTTP client, for the internet game list. gnu99 because it is written in the same
# style as the launcher that already builds it, and it needs a line here for the reason
# spelled out above: a name in sources.sh is read by the drift check and compiled by
# nobody.
compile_c gnu99 $WIN_HTTP_C

# RGBA menu art remains GL 1.1/nearest; compile the shared native renderer once.
for f in $WIN_MENU_CPP; do
    o="$OBJ/$(basename "$f" .cpp).o"
    run_cc $CXX -std=c++14 -O2 -g $CDEFS $INC -c "$f" -o "$o"
    COBJ="$COBJ $o"
done

echo "== renderer"
# -fpermissive for the same reason the brain build needs it, and only for that reason:
# these translation units include the Remaster's dllinterface.h, whose MS-style anonymous
# unions GCC rejects and clang accepts. Nothing in our own code relies on it. It would
# also let a pointer-to-int truncation through as a warning, which in a 64-bit program is
# a real bug; run_cc's search of the log is what stops the build on one.
# -DOPENCNC3D_NO_MAIN compiles cnc_eyes.cpp as a library for opencnc3d-game.exe, exactly as the Mac
# app build does. It is compiled twice because cnc_eyes.exe (what the gates drive) needs
# the main() and opencnc3d-game.exe must not have two. Both take the build header, so
# cnc_eyes.exe carries the same build mark the Mac renderer does.
run_cc $CXX -std=c++14 -O3 -g $WIN_CPU -fms-extensions -fpermissive -DOPENCNC3D_NO_MAIN \
     $CDEFS -include "$ROOT/game/opencnc3d_build.h" $INC -I"$HDR" -c "$WIN_EYES_CPP" -o "$OBJ/cnc_eyes_lib.o"
run_cc $CXX -std=c++14 -O3 -g $WIN_CPU -fms-extensions -fpermissive \
     $CDEFS -include "$ROOT/game/opencnc3d_build.h" $INC -I"$HDR" -c "$WIN_EYES_CPP" -o "$OBJ/cnc_eyes_main.o"
# The app list can contain more than one translation unit. Compile every
# listed source independently for both entry points; source-list parity alone
# cannot catch a quoted list passed as one nonexistent filename.
APP_OBJECTS=""
APP_DIAGNOSTIC_OBJECTS=""
for f in $WIN_APP_CPP; do
    app_object="$OBJ/$(basename "$f" .cpp).o"
    diagnostic_object="$OBJ/$(basename "$f" .cpp)_diagnostics.o"
    run_cc $CXX -std=c++14 -O2 -g $WIN_CPU -fms-extensions -fpermissive \
         $CDEFS $INC -I"$HDR" -c "$f" -o "$app_object"
    run_cc $CXX -std=c++14 -O2 -g $WIN_CPU -fms-extensions -fpermissive \
         -DOPENCNC3D_DIAGNOSTIC_APP=1 $CDEFS $INC -I"$HDR" -c "$f" -o "$diagnostic_object"
    APP_OBJECTS="$APP_OBJECTS $app_object"
    APP_DIAGNOSTIC_OBJECTS="$APP_DIAGNOSTIC_OBJECTS $diagnostic_object"
done

# EVERYTHING IS LINKED INTO THE PROGRAM, SDL2 INCLUDED, so no DLL travels beside the .exe.
#
# -static is given to the g++ driver, and that is what folds the GCC runtime, the C++
# runtime and winpthread in: -static-libgcc and -static-libstdc++ alone leave
# libwinpthread-1.dll as a dependency, which a stock Windows 11 does not have, and naming
# -lwinpthread with -Bstatic does not work because the driver appends its own dynamic
# -lwinpthread after everything we pass.
#
# SDL2 comes from the SDK's static libSDL2.a. Inside a DLL, SDL resolves the Windows
# multimedia, input and device calls it makes against its own imports; linked into the
# program, those imports become ours, so SDL's private libraries follow it on this line
# (they are the Libs.private of the SDK's sdl2.pc). A -static link without them fails on
# the first of those calls, waveOutGetErrorTextW. With SDL inside the program, a folder
# can no longer pair a 64-bit game with a 32-bit SDL2.dll an older install left behind.
#
# Console subsystem on purpose: the gates and every diagnostic in this project talk on
# stdout and stderr, and -mwindows would throw all of it away.
# -lws2_32 is winsock, for net/net_udp.c. It goes AFTER the objects that reference it,
# which is what the rest of this line already assumes. -lwininet is the HTTP client's
# transport, beside -lopengl32 for the same reason: this linker resolves left to right,
# so a library named before the objects that need it resolves nothing. That slot is where
# the launcher's own working link line puts it.
LIBS="-static -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lwininet -lz -lws2_32 -lm \
      -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 \
      -lshell32 -lsetupapi -lversion -luuid"
# LOADED ANYWHERE IN THE 64-BIT SPACE: a relocatable image (--dynamicbase), randomised
# across the whole 64-bit range (--high-entropy-va), no executable data (--nxcompat).
# These are the toolchain's defaults for x86-64 today and are named so that a change of
# default cannot quietly take them away. With them, and the image base above 4 GB that
# goes with them, a pointer cut to 32 bits faults the first time it is used instead of
# working until a process grows past 4 GB. tools/win/check-pe-arch.py asks every program
# for them at the end of this script.
LDSEC="-Wl,--dynamicbase -Wl,--high-entropy-va -Wl,--nxcompat"

echo "== link"
# THE ICON AND THE VERSION BLOCK, compiled into opencnc3d-game.exe as resources, so the game
# carries the same face and the same build number as the launcher in front of it.
# tools/launchers/make_icon.py writes opencnc3d.ico beside the .icns from ONE source image, so
# the Windows and macOS icons cannot drift apart.
#
# tools/win/opencnc3d.rc is a TEMPLATE and is substituted here rather than compiled as it
# stands. The version it carries comes from tools/version.sh, which reads the VERSION file
# at the repo root, so Explorer's Properties tab and the menu plate cannot disagree and
# there is still only one place the number is written down.
#
# Of the binaries this script links, only opencnc3d-game.exe gets any of this (the launcher,
# OpenCNC3D.exe, gets the same template from tools/win/build-launcher-win.sh). cnc_eyes.exe
# is the verification binary the gate suite drives on Windows, not something anybody
# double-clicks, and giving it the game's face would undo the point of the exercise.
RCOBJ=""
if [ -f "$ROOT/tools/win/opencnc3d.rc" ] && [ -f "$ROOT/tools/launchers/opencnc3d.ico" ]; then
    cp "$ROOT/tools/launchers/opencnc3d.ico" "$OBJ/opencnc3d.ico"
    VER_COMMA=$(sh "$ROOT/tools/version.sh" --rcversion)
    VER_STRING=$(cat "$ROOT/VERSION" | tr -d '[:space:]')
    sed -e "s/@VER_COMMA@/$VER_COMMA/g" -e "s/@VER_STRING@/$VER_STRING/g" \
        -e "s/@EXE_NAME@/opencnc3d-game.exe/g" \
        "$ROOT/tools/win/opencnc3d.rc" > "$OBJ/opencnc3d.rc"
    # An unsubstituted placeholder would reach windres and be reported as a syntax error
    # on a line number in a generated file, which is a long way from the cause. Say it here
    # instead. This is also the test that fails loudly if the template gains a placeholder
    # the substitution above does not know about: any @NAME@ left over, not only the ones
    # known today. The template's own comment mentions "@VER placeholders", which this
    # pattern does not match, because a placeholder is closed by a second @.
    if grep -qE '@[A-Z][A-Z_]*@' "$OBJ/opencnc3d.rc"; then
        echo "ERROR: tools/win/opencnc3d.rc has a placeholder build-win.sh does not substitute:" >&2
        grep -nE '@[A-Z][A-Z_]*@' "$OBJ/opencnc3d.rc" >&2
        exit 1
    fi
    # windres errors used to go to /dev/null, so a resource script that stopped compiling
    # reported itself as one warning line about a missing icon and the real message was
    # destroyed. The version block makes that worse, because a silent failure now costs the
    # version as well as the face. The output is kept and shown.
    if $HOST-windres "$OBJ/opencnc3d.rc" -O coff -o "$OBJ/opencnc3d_res.o" 2>"$OBJ/windres.err"; then
        RCOBJ="$OBJ/opencnc3d_res.o"
        echo "== resources: icon + version $VER_STRING compiled into opencnc3d-game.exe"
    else
        echo "WARNING: windres failed; opencnc3d-game.exe will ship without its icon and version" >&2
        sed 's/^/  windres: /' "$OBJ/windres.err" >&2
    fi
else
    echo "WARNING: no opencnc3d.ico (run tools/launchers/make_icon.py); shipping without an icon" >&2
fi

# THE UTF-8 MANIFEST, on both executables. The per-user directory SDL_GetPrefPath answers
# is UTF-8, and it is handed to the C runtime's fopen for the log, the saves and the
# world dump. Windows reads that path in the process code page, so a user name the code
# page cannot spell (a Polish, Cyrillic or Japanese one on most machines) gave the game a
# path it could not open: no per-user log, no saves, and an empty world where the dump
# was. The launcher has carried this manifest since it was written; the game did not.
# It makes the process code page UTF-8 on Windows 10 version 1903 and later, and earlier
# Windows ignores the element. Resource 1 of type 24 (RT_MANIFEST) is the one Windows
# reads when it starts a process. Compiled into its own object rather than folded into
# opencnc3d.rc, because cnc_eyes.exe carries no icon and no version block and needs this.
cat > "$OBJ/game.manifest" <<'XML'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
    </windowsSettings>
  </application>
</assembly>
XML
printf '1 24 "game.manifest"\n' > "$OBJ/game_manifest.rc"
MANOBJ=""
if ( cd "$OBJ" && $HOST-windres game_manifest.rc -O coff -o game_manifest.o 2>"$OBJ/manifest.err" ); then
    MANOBJ="$OBJ/game_manifest.o"
    echo "== manifest: UTF-8 code page declared for opencnc3d-game.exe and cnc_eyes.exe"
else
    echo "WARNING: windres could not compile the manifest; both executables will ship without the UTF-8 code page" >&2
    sed 's/^/  windres: /' "$OBJ/manifest.err" >&2
fi

# No large-address-aware flag any more: the x86-64 linker refuses it, and a 64-bit program
# has the whole address space without asking. What a 64-bit build has to keep instead is
# LDSEC above, and the read-back at the end of this script asks for it.
#
# EVERY THREAD'S STACK HAS 512 KB COMMITTED FROM THE START (the reserve stays the default
# 2 MB). A Windows stack otherwise commits one page at a time as it grows, and under a
# commit limit (a job object's memory cap, or a machine whose commit is spent) the next page
# can be refused, which Windows reports as a stack overflow however little of the stack is
# in use: the game died that way, with no line, at a 4 GB cap. Committed up front, the pages
# are there. It costs 512 KB of commit per thread (about ten in a game), and it is the
# default for every thread made with no size of its own, the C runtime's, SDL's and the
# graphics driver's included. game/fatal_guard.h prints a line if an overflow happens anyway.
# The linker's option takes "reserve,commit" as one argument, so it is passed with -Xlinker:
# -Wl splits at every comma and would hand the commit size to the linker as a file name.
LDSTACK="-Xlinker --stack -Xlinker 2097152,524288"
run_cc $CXX $LDSEC $LDSTACK -o "$OUT/cnc_eyes.exe" "$OBJ/cnc_eyes_main.o" $MANOBJ $COBJ $LIBDIRS $LIBS
run_cc $CXX $LDSEC $LDSTACK -o "$OUT/opencnc3d-game.exe"    $APP_OBJECTS "$OBJ/cnc_eyes_lib.o" $RCOBJ $MANOBJ $COBJ $LIBDIRS $ARCHIVE_LIBS $LIBS -lbcrypt -lxmllite -ladvapi32 -lcomdlg32 -lwinspool

run_cc $CXX -mwindows $LDSEC $LDSTACK -o "$OUT/OpenCNC3D-Diagnostics.exe" $APP_DIAGNOSTIC_OBJECTS "$OBJ/cnc_eyes_lib.o" $RCOBJ $MANOBJ $COBJ $LIBDIRS $ARCHIVE_LIBS $LIBS -lbcrypt -lxmllite -ladvapi32 -lcomdlg32 -lwinspool
PRODUCED="$PRODUCED $OUT/cnc_eyes.exe $OUT/opencnc3d-game.exe $OUT/OpenCNC3D-Diagnostics.exe"
python3 tools/map-archive/stage.py build-native/map-archive/windows "$OUT"
PRODUCED="$PRODUCED $OUT/archive.dll"

# THE CONNECTIVITY TOOL SHIPS WITH THE WINDOWS BUILD, and it has to, because the pairing
# it is most needed for is this machine against the Mac one. It links only the two net
# modules and needs no SDL and no engine, so it is a few kilobytes and cannot be affected
# by anything else in the build going wrong. Standalone, exactly like the gates: nothing
# links it into the game, and tools/win/check-sources.sh excludes it by name.
#
# A 16 MB STACK, because an LsState is a megabyte (LS_HISTORY x LS_MAX_SEATS turns of
# LS_TURN_BYTES) and netcheck keeps them as locals in main. macOS gives the main thread
# 8 MB and never noticed; Windows gives 2 MB, and the v0.6.4 netcheck.exe died on launch
# with STATUS_STACK_OVERFLOW (0xC00000FD) before printing a byte. Found on the Windows
# box, 3 Sep 2026. The lasting fix is to stop putting the scheduler state on the stack
# at all, which is the net/ author's call; this makes the shipped tool run today.
#
# ITS SOURCE LIST IS ITS OWN AND IS NOT CHECKED BY ANYTHING. The drift guard compares the
# two builds' GAME sources; netcheck is a standalone tool, so a file the Mac links into it
# and this line does not is caught by nothing until the link fails. That is how the room
# code broke this build: net/roomcode.c was added to the game's list on both platforms
# correctly, and netcheck, which prints and reads those codes, was linked without it here
# and kept linking on the Mac. A new net source that netcheck calls needs adding HERE as
# well as to sources.sh.
echo "== netcheck.exe"
run_cc $CC -std=gnu89 -O2 -g -Wall $CWIDTH $PATHMAP $LDSEC -Wl,--stack,16777216 -o "$OUT/netcheck.exe" \
    "$ROOT/net/netcheck.c" "$ROOT/net/lockstep.c" "$ROOT/net/net_udp.c" \
    "$ROOT/net/roomcode.c" -lws2_32
PRODUCED="$PRODUCED $OUT/netcheck.exe"

# THE HEADLESS BRAIN HOST AND THE TWO-BRAIN GATE, for the Windows half of Phase 0
# (docs/design-multiplayer.md section 10). Both are dependency-free C over LoadLibrary,
# compiled unedited from brain/host/, and both need the DLL built below beside them to
# be useful: the cross architecture determinism run needs both halves. gnu99 for loop
# variables in for statements and _Static_assert on the struct mirrors.
# Standalone, like netcheck: nothing links them into the game, and brain/host/* is
# excluded from tools/win/check-sources.sh by directory.
echo "== cncbrain.exe, cnc_twobrain.exe"
run_cc $CC -std=gnu99 -O2 -g -Wall $CWIDTH $LDSEC -I"$ROOT/brain/host" -o "$OUT/cncbrain.exe" "$ROOT/brain/host/cnc_host.c"
run_cc $CC -std=gnu99 -O2 -g -Wall $CWIDTH $LDSEC -I"$ROOT/brain/host" -o "$OUT/cnc_twobrain.exe" "$ROOT/brain/host/cnc_twobrain.c"
PRODUCED="$PRODUCED $OUT/cncbrain.exe $OUT/cnc_twobrain.exe"

if [ $DO_BRAIN -eq 1 ]; then
    echo "== brains (TiberianDawn.dll + RedAlert.dll)"
    # -fpermissive: GCC rejects the Remaster header's MS-style anonymous unions with
    # named struct types, and one extra qualification on a constructor. Both are ISO
    # conformance complaints about name lookup with no effect on generated code, and
    # clang accepts them silently on the Mac side. The same flag also turns the one
    # diagnostic that matters at 64 bits, a pointer cast to a 32-bit integer ("loses
    # precision"), into a warning. So the brain's compiler output is kept in
    # build/win/brain-build.log, not thrown away, and truncation_guard reads it below.
    tc="$ROOT/brain/vanilla/cmake/$(echo $HOST | cut -d- -f1)-mingw-w64-toolchain.cmake"
    # THE BRAIN CACHE IS PER ARCHITECTURE: brain/vanilla/build-win64. A CMake cache keeps
    # the compiler that configured it, and the 32-bit builds used brain/vanilla/build-win,
    # so a 64-bit build pointed at that folder would quietly compile 32-bit brains again.
    # A 32-bit cache there is removed once, so nothing can pick it up by hand either.
    if [ -f brain/vanilla/build-win/CMakeCache.txt ] && \
       grep -q 'i686-w64-mingw32' brain/vanilla/build-win/CMakeCache.txt; then
        echo "   removing brain/vanilla/build-win: a 32-bit brain cache from an older build"
        rm -rf brain/vanilla/build-win
    fi
    BRAINDIR=brain/vanilla/build-win64
    # A cache remembers the cmake and the mingw tools that configured it by absolute path.
    # After the build machine's toolchain moves (an Intel Homebrew in /usr/local replaced
    # by a native one in /opt/homebrew) the cache names programs that no longer run, and
    # every configure fails. It holds nothing that is not rebuilt from source, so a cache
    # from another cmake, or one whose compiler is not this host's, is thrown away.
    #
    # The compiler is asked through two entries the cache really carries. The toolchain
    # file sets CMAKE_CXX_COMPILER as a plain variable, so that name never reaches
    # CMakeCache.txt, and a test of it matched nothing and threw every cache away. The
    # toolchain file's own name says the architecture (x86_64-mingw-w64-toolchain.cmake),
    # and the archiver CMake found beside the compiler says which mingw it was and where
    # it lives: this host's prefix, at a path that still runs.
    if [ -f "$BRAINDIR/CMakeCache.txt" ]; then
        cached_tc=$(sed -n 's/^CMAKE_TOOLCHAIN_FILE:FILEPATH=//p' "$BRAINDIR/CMakeCache.txt")
        cached_ar=$(sed -n 's/^CMAKE_CXX_COMPILER_AR:FILEPATH=//p' "$BRAINDIR/CMakeCache.txt")
        case "$cached_ar" in
            */$HOST-gcc-ar|*/$HOST-gcc-ar.exe) ar_ok=1 ;;
            *) ar_ok=0 ;;
        esac
        if [ "$(sed -n 's/^CMAKE_COMMAND:INTERNAL=//p' "$BRAINDIR/CMakeCache.txt")" != "$(command -v cmake)" ]; then
            echo "   $BRAINDIR was configured by another cmake: configuring it fresh"
            rm -rf "$BRAINDIR"
        elif [ "${cached_tc##*/}" != "${tc##*/}" ] || [ $ar_ok -eq 0 ]; then
            echo "   $BRAINDIR was configured for another compiler: configuring it fresh"
            rm -rf "$BRAINDIR"
        elif [ ! -x "$cached_ar" ]; then
            echo "   $BRAINDIR names mingw tools that are no longer there: configuring it fresh"
            rm -rf "$BRAINDIR"
        fi
    fi
    # As many compile jobs as the machine has cores, unless OPENCNC3D_WIN_JOBS says
    # otherwise: a machine busy measuring something else can build one file at a time.
    JOBS=${OPENCNC3D_WIN_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}
    case "$JOBS" in
        ''|*[!0-9]*|0) echo "OPENCNC3D_WIN_JOBS=$JOBS is not a number of jobs (1 or more)" >&2; exit 1 ;;
    esac
    BRAINLOG="$OUT/brain-build.log"
    : > "$BRAINLOG"
    mkdir -p "$BRAINDIR"
    BRAIN_BUILT=1
    (cd "$BRAINDIR" && \
     LC_ALL=C cmake .. -DCMAKE_TOOLCHAIN_FILE="$tc" \
              -DCMAKE_CXX_FLAGS="-fms-extensions -fpermissive -flifetime-dse=1 $PATHMAP" \
              -DCMAKE_C_FLAGS="$PATHMAP" \
              -DCMAKE_SHARED_LINKER_FLAGS="-static $LDSEC" \
              -DBUILD_REMASTERTD=ON -DBUILD_VANILLATD=OFF -DBUILD_VANILLARA=OFF \
              -DBUILD_REMASTERRA=ON -DSDL2=OFF -DOPENAL=OFF -DNETWORKING=OFF >>"$BRAINLOG" 2>&1 && \
     LC_ALL=C cmake --build . --target TiberianDawn RedAlert -j"$JOBS" >>"$BRAINLOG" 2>&1) \
        || BRAIN_BUILT=0
    # A TRUNCATION IN THE BRAIN ALSO EMPTIES ITS CACHE, whether or not the rest built. CMake
    # compiles only what changed, so the next run would not compile the offending file
    # again, print nothing, and pass this search with the very object that carries the
    # truncation. Thrown away, the next build compiles every brain file and asks again.
    if grep -E "$TRUNCATION" "$BRAINLOG" >/dev/null; then
        echo "   removing $BRAINDIR so that the next build compiles every brain file again"
        rm -rf "$BRAINDIR"
    fi
    if [ $BRAIN_BUILT -eq 0 ]; then
        echo "ERROR: the Windows brain did not build. The end of $BRAINLOG:" >&2
        tail -n 40 "$BRAINLOG" >&2
        echo "       A cache generated under a different path refuses every later" >&2
        echo "       configure; deleting $BRAINDIR and trying again clears that." >&2
        exit 1
    fi
    truncation_guard "$BRAINLOG"
    # THE COPY IS CONDITIONAL, and it used to be unconditional.
    #
    # Both cmake steps used to send their output to /dev/null inside a subshell, so a
    # configure that refused left no message; the cp beneath it then copied whatever
    # TiberianDawn.dll happened to be in the build directory from a previous run, and the
    # script printed the file under "built:" as though it had just made it. A working copy
    # cloned from another path hits exactly that, because a CMake cache records the
    # directory it was generated in and refuses to be reused from anywhere else. The
    # result is a Windows package carrying a renderer from this commit and a brain from
    # some earlier one, which is the precise failure the both-platforms rule exists to
    # prevent, reported as success.
    for brain in TiberianDawn.dll RedAlert.dll; do
        [ -f "$BRAINDIR/$brain" ] \
            || { echo "ERROR: cmake reported success but $brain was not produced" >&2; exit 1; }
        cp "$BRAINDIR/$brain" "$OUT/"
    done
fi
# The brains in the folder are part of what is asked at the end, whether this run built
# them or --no-brain kept them: they are what the programs beside them will load.
for brain in TiberianDawn.dll RedAlert.dll; do
    if [ -f "$OUT/$brain" ]; then PRODUCED="$PRODUCED $OUT/$brain"; fi
done

# The RA bootstrap probe is a build/test tool, not a shipped game binary. Keep it in the
# cross-build output so CI can execute it under its Windows runtime and compare its report
# with macOS, while tools/win/make-build-win.sh continues to package only named products.
# C11 is required for the ABI static assertions, warnings are errors because a bad mirror
# invalidates the gate, and a static link keeps the native runner independent of MinGW DLLs.
if [ -f "$ROOT/brain/host/ra_bootstrap.c" ]; then
    echo "== ra_bootstrap.exe"
    run_cc $CC -std=c11 -O2 -g -Wall -Wextra -Werror $CWIDTH -static $LDSEC -I"$ROOT/brain/host" \
        -o "$OUT/ra_bootstrap.exe" "$ROOT/brain/host/ra_bootstrap.c"
    PRODUCED="$PRODUCED $OUT/ra_bootstrap.exe"
fi

# AND READ BACK, every file this script wrote and every brain it kept, by content.
# check-pe-arch.py refuses anything that is not x86-64 PE32+, that has lost the
# high-entropy relocatable layout LDSEC asks for, or whose import table names SDL2.dll:
# a link line that fell back to the import library would still build and run here, and
# would ship a program that stops on any machine without the DLL beside it. It reads the
# files itself, so this step needs no objdump, which MSYS2's MINGW64 installs unprefixed.
# It runs here, at the end, on exactly these files: run earlier, over the whole folder,
# it would trip on programs the rest of this script is about to rewrite.
echo "== read back"
python3 "$ROOT/tools/win/check-pe-arch.py" $PRODUCED || {
    echo "ERROR: a program this build wrote is not a 64-bit Windows program laid out as required" >&2
    exit 1; }

echo
echo "built:"
for f in "$OUT"/*.exe "$OUT"/*.dll; do [ -f "$f" ] && echo "  $f"; done
