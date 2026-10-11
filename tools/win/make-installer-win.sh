#!/bin/sh
# Build OpenCNC3D Setup, the Windows web installer, on the Mac.
#
#   tools/win/make-installer-win.sh             build the launcher here, then wrap it
#   tools/win/make-installer-win.sh <folder>    wrap the launcher taken out of a staged
#                                               Windows package (what tools/release.sh
#                                               passes, so Setup installs the package's
#                                               own launcher, byte for byte)
#
# Output: ~/Desktop/OpenCNC3D-Setup-vX.Y.Z.exe, or the folder OPENCNC3D_WIN_ZIP names,
# which is the same variable that moves make-build-win.sh's zips.
#
# It needs NSIS, which cross builds happily from macOS, and python3:
#
#   brew install makensis
#
# SETUP CARRIES THE LAUNCHER AND NOTHING OF THE GAME. It installs OpenCNC3D.exe, SDL2.dll,
# the icon, the notes and the licence, and the launcher it has just installed downloads
# the newest Windows package and the movies and music the player ticked, on Setup's own
# progress page (the long note at the top of installer/opencnc3d.nsi has why). So this
# needs no staged game and no data folder, and the result is a couple of megabytes. The
# game does not fit in an NSIS installer anyway: one holds at most 2 GB.
#
# WHY AN INSTALLER AT ALL, when the zip works. Three things the zip cannot do, and all
# three are things players hit:
#
#   1. A Start Menu entry and a desktop icon. A folder in Downloads is not an installed
#      game, and "where did it go" is a real support question.
#   2. An entry in Add/Remove Programs, so uninstalling is the ordinary thing rather than
#      deleting a folder and hoping.
#   3. A place to put it that is WRITABLE. The launcher updates the game in place; the
#      installer's per-user location makes that work with no UAC prompt at all.
#
# The zip does not go away. It stays for anyone who wants the folder, it is what Setup's
# launcher downloads, and the launcher's binary-only update is a zip unpacked over exactly
# that folder.
#
# OPENCNC3D_MEDIA_MANIFEST=<file> reads the sizes on the components page from that
# manifest (plain text, or a C file defining LMEDIA_MANIFEST) instead of
# launcher/lmedia_manifest.c. Either way the launcher being wrapped must carry the same
# manifest, byte for byte, or nothing is built.
set -e
cd "$(dirname "$0")/../.."
ROOT=$(pwd)

die() { echo "$*" >&2; exit 1; }

command -v makensis >/dev/null 2>&1 || {
    echo "makensis is not installed, so the Windows installer cannot be built." >&2
    echo "  brew install makensis" >&2
    exit 1
}
command -v python3 >/dev/null 2>&1 || die "python3 is needed to read the media manifest."

VERSION=$(tr -d '[:space:]' < VERSION)
TAG="v$VERSION"
NSI="$ROOT/tools/win/installer/opencnc3d.nsi"

# ---- BOTH DOWNLOADS START TICKED --------------------------------------------------
# A section carrying /o starts unticked, and the movies and the music both start ticked,
# so neither section may carry it. Asked of the script first, before anything is built,
# because it is the one word in it a tidy-up could change without anything else noticing.
for s in SecMovies SecMusic; do
    line=$(grep -E "^[[:space:]]*Section[[:space:]].*[[:space:]]$s[[:space:]]*$" "$NSI" || true)
    [ "$(printf '%s\n' "$line" | grep -c .)" = 1 ] ||
        die "installer/opencnc3d.nsi must declare exactly one section $s; it declares: ${line:-none}"
    case "$line" in
        *"/o"*) die "the $s section carries /o, so its box would start unticked: $line" ;;
    esac
done
echo "== the Movies and Music sections carry no /o, so both start ticked"

# ---- the files Setup installs -------------------------------------------------------
SETUP_STAGE="$ROOT/build/win-setup"
PAYLOAD="$SETUP_STAGE/payload"
rm -rf "$SETUP_STAGE"
mkdir -p "$PAYLOAD"

GAME_KB=0
PKG="$1"
if [ -n "$PKG" ]; then
    # ABSOLUTE, ALWAYS. makensis resolves a relative path against the directory of the
    # .nsi FILE, not against the working directory, so every path handed to it is made
    # absolute.
    [ -d "$PKG" ] || die "no staged Windows folder at: $PKG"
    PKG=$(cd "$PKG" && pwd)
    for f in OpenCNC3D.exe SDL2.dll READ-ME-WINDOWS.txt LICENSE NOTICE.md; do
        [ -s "$PKG/$f" ] || die "the staged package $PKG has no $f to put in Setup."
        cp "$PKG/$f" "$PAYLOAD/"
    done
    # The space line on the directory page counts the game Setup is about to download.
    # The package being wrapped is that game, as near as can be known here: the build the
    # site offers when Setup runs is this one or a later one.
    GAME_KB=$(du -sk "$PKG" | cut -f1)
    echo "== launcher taken from the staged package $(basename "$PKG")"
else
    echo "== compiling the Windows launcher"
    tools/win/build-launcher-win.sh
    cp "$ROOT/build/win/OpenCNC3D.exe" "$ROOT/build/win/SDL2.dll" "$PAYLOAD/"
    GITDESC=$(git describe --always --dirty 2>/dev/null || echo untracked)
    [ -n "$OPENCNC3D_BUILD_ID" ] && GITDESC="$OPENCNC3D_BUILD_ID"
    OPENCNC3D_COOKED="$OPENCNC3D_COOKED" sh "$ROOT/tools/win/installer/readme-windows.sh" \
        "$PAYLOAD/READ-ME-WINDOWS.txt" "$GITDESC"
    # The licence and the notice travel with any binary built from this tree, and Setup
    # is a binary package of the launcher.
    for f in LICENSE NOTICE.md; do
        [ -f "$ROOT/$f" ] || die "$f is missing from the repo root; a GPL binary package has to carry it"
        cp "$ROOT/$f" "$PAYLOAD/"
    done
    echo "   no staged package was given, so the directory page's space line counts only"
    echo "   the launcher and the media, and the welcome page gives no game size"
fi
# The icon the shortcuts and the Add/Remove Programs entry show, as a file of its own, so
# they keep the eagle even in a launcher whose icon resource did not compile.
ICON="$ROOT/tools/launchers/opencnc3d.ico"
[ -f "$ICON" ] || die "no $ICON for Setup and its shortcuts"
cp "$ICON" "$PAYLOAD/opencnc3d.ico"

# NOTHING ELSE, by name. Setup is small because it carries no game, and a payload that
# grows one data file is a payload that will grow the rest.
EXTRA=$(cd "$PAYLOAD" && find . -type f | sed 's|^\./||' | sort |
        grep -vxE 'OpenCNC3D\.exe|SDL2\.dll|opencnc3d\.ico|READ-ME-WINDOWS\.txt|LICENSE|NOTICE\.md' || true)
[ -z "$EXTRA" ] || die "Setup's payload holds files it must not carry:
$(echo "$EXTRA" | sed 's/^/  /')"

# The picture down the left of the welcome and finish pages: the Enhanced game, a GDI base
# in the desert (mission SCB05EA), 164 by 314 as the wizard draws it.
WIZARD="$ROOT/tools/win/installer/wizard.bmp"
[ -f "$WIZARD" ] || die "no $WIZARD for Setup's welcome and finish pages"

# ---- the launcher must be one that can install the game -----------------------------
EXE="$PAYLOAD/OpenCNC3D.exe"
# The LAUNCHER, not the engine wearing its name: every shortcut points at this file.
LC_ALL=C grep -aqF '/api/builds' "$EXE" ||
    die "OpenCNC3D.exe is not the launcher: it has no update route in it."
# And a launcher with the install mode Setup drives. An older one would start, refuse the
# arguments, and leave the player with a launcher and no game.
for s in install-game media-install progress-file cancel-file; do
    LC_ALL=C grep -aqF -- "$s" "$EXE" || die "OpenCNC3D.exe has no '$s' in it, so it cannot do the
install Setup asks of it. Build it from a tree that has the launcher's install mode."
done

# ---- the sizes beside the two boxes, from the manifest the launcher carries -----------
MANIFEST="${OPENCNC3D_MEDIA_MANIFEST:-$ROOT/launcher/lmedia_manifest.c}"
[ -f "$MANIFEST" ] || die "no media manifest at $MANIFEST. The sizes on Setup's components page
come from the same manifest the launcher downloads by."
SIZES=$(python3 -I "$ROOT/tools/win/installer/media-sizes.py" "$MANIFEST" --exe "$EXE") ||
    die "the media manifest could not be used (above), so no installer was built."
eval "$SIZES"
echo "   movies: $MOVIES_FILES files, $MOVIES_SIZE    music: $MUSIC_FILES files, $MUSIC_SIZE"

if [ "$GAME_KB" -gt 0 ]; then
    GAME_SIZE=$(awk -v k="$GAME_KB" 'BEGIN { if (k >= 1048576) printf "%.1f GB", k/1048576; else printf "%.1f MB", k/1024 }')
    echo "   game: $GAME_SIZE installed, counted on the directory page"
fi

OUTDIR="${OPENCNC3D_WIN_ZIP:-$HOME/Desktop}"
mkdir -p "$OUTDIR"
OUTFILE="$(cd "$OUTDIR" && pwd)/OpenCNC3D-Setup-$TAG.exe"
rm -f "$OUTFILE"

echo "== makensis"
LOG=$(mktemp)
set -- -V4 \
    "-DPAYLOAD=$PAYLOAD" \
    "-DVERSION=$VERSION" \
    "-DOUTFILE=$OUTFILE" \
    "-DICONFILE=$ICON" \
    "-DWIZARDBMP=$WIZARD" \
    "-DMOVIES_SIZE=$MOVIES_SIZE" "-DMOVIES_KB=$MOVIES_KB" \
    "-DMUSIC_SIZE=$MUSIC_SIZE" "-DMUSIC_KB=$MUSIC_KB"
[ "$GAME_KB" -gt 0 ] && set -- "$@" "-DGAME_KB=$GAME_KB" "-DGAME_SIZE=$GAME_SIZE"
makensis "$@" "$NSI" > "$LOG" 2>&1 \
    || { sed 's/^/  /' "$LOG"; rm -f "$LOG"; die "makensis failed"; }

[ -f "$OUTFILE" ] || { rm -f "$LOG"; die "makensis reported success and produced no file"; }

# COUNT WHAT LANDED, rather than trusting the exit status. This project has been bitten
# twice by packagers that exited 0 having quietly shipped a short set, and an installer
# that is missing the launcher's DLL installs a launcher that will not start. makensis -V4
# prints one "File: ..." line per file it embeds, IN EITHER OF TWO FORMS:
#
#     File: "OpenCNC3D.exe" 387500 bytes            when the file is stored
#     File: "LICENSE" 11920/35149 bytes             when it went through the compressor
#
# and it also prints `File: Descending to: <dir>` / `File: Returning to: <dir>` while it
# recurses, which must not be counted. Both mistakes have been made here before: one
# counted the recursion lines, the other matched only the stored form.
WANT=$(find "$PAYLOAD" -type f | wc -l | tr -d ' ')
GOT=$(grep -cE '^File: "[^"]+" [0-9]+(/[0-9]+)? bytes' "$LOG" || true)
sed -n 's/^Total size:/   total size:/p' "$LOG"
rm -f "$LOG"
if [ "$GOT" != "$WANT" ]; then
    rm -f "$OUTFILE"
    die "the installer embeds $GOT files and the payload has $WANT. The installer has been removed."
fi
echo "   $GOT files embedded, which is every file in the payload"

BYTES=$(wc -c < "$OUTFILE" | tr -d ' ')
echo "   Setup is $BYTES bytes ($(awk -v b="$BYTES" 'BEGIN { printf "%.1f MB", b/1048576 }'))"
echo "built $OUTFILE"
