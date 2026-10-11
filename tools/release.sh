#!/bin/sh
# Make a build. One command, both platforms, one number, everything pushed.
#
#   tools/release.sh              make the next build and push it
#   tools/release.sh --dry-run    do everything except commit, tag, push and publish
#   tools/release.sh --next       print what the next build number would be
#   tools/release.sh --minor      make the next MINOR build instead (0.7.3 -> 0.8.0); with
#                                 --next or --dry-run it asks the same of that number
#
# THE RULES THIS ENFORCES, so that they are enforced rather than remembered:
#
#  1. Every build has a number, and the number goes up by 0.0.1. It lives in VERSION,
#     it reaches the menu plate through tools/version.sh, and it becomes the git tag.
#  2. Every build is BOTH platforms of the SAME COMMIT. Never a Mac build now and a
#     Windows one later; that is project rule 4 and it is the rule this project has
#     broken most expensively.
#  3. Every build has a changelog entry, written BEFORE the build, in the existing
#     format. This script refuses to run without one, because a build nobody wrote
#     down is a build nobody can tell apart from the last one.
#  4. Everything is pushed. Not just the commit: this refuses to run with an untracked
#     file in the tree, because the project is worked from more than one checkout and
#     anything that exists in only one of them is work that can be lost.
#  5. The branches `windows` and `macos` are RELEASE POINTERS, not development lines.
#     Both are moved to the released commit, so checking out either on either machine
#     always gives the same gate-tested code. Work lands on main and nowhere else.
#  6. The gates must pass before anything is tagged or pushed.
set -e
cd "$(dirname "$0")/.."
ROOT=$(pwd)

# --minor cuts a MILESTONE build (0.5.9 -> 0.6.0) instead of the usual patch. It is a
# flag rather than something the script works out, because whether a round is a milestone
# is a judgement about what shipped and no version string carries that.
DRY=0
MINOR=0
NEXTONLY=0
INTERACTIVE_GATES=0
for a in "$@"; do
    case "$a" in
    --dry-run) DRY=1 ;;
    --minor)   MINOR=1 ;;
    --next)    NEXTONLY=1 ;;
    --interactive-gates) INTERACTIVE_GATES=1 ;;
    esac
done

# THE PUBLIC BUILDS CARRY NO RED ALERT. OPENCNC3D_NO_RA=1 is passed to both halves and both
# packagers: the Mac build stages nothing under ra3d/, neither package carries ra3d/ or the
# Red Alert brain, and the zips are searched for either before anything is published. The
# Red Alert gates may then skip (RA_GATE_LIST, at the gates). 0 puts the module back in.
NO_RA=1

VERB=--next
[ "$MINOR" = "1" ] && VERB=--next-minor
if [ "$NEXTONLY" = "1" ]; then tools/version.sh "$VERB"; exit 0; fi

NEXT=$(tools/version.sh "$VERB")
TAG="v$NEXT"
say() { printf '\n== %s\n' "$*"; }
die() { printf '\nRELEASE REFUSED: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- 1. the tree is sane
say "checks"
BR=$(git rev-parse --abbrev-ref HEAD)
[ "$BR" = "main" ] || die "you are on '$BR'. Work lands on main; windows and macos are
       release pointers this script moves for you, and committing on them by hand is how
       the checkouts drift apart."

git diff --quiet || die "there are uncommitted changes. A build is made from a commit, so
       commit the work first: the build number on the menu would otherwise say -dirty and
       the tag would point at something is not in hand."
git diff --cached --quiet || die "there are staged but uncommitted changes."

UNTRACKED=$(git ls-files --others --exclude-standard)
[ -z "$UNTRACKED" ] || die "untracked files present, and rule 4 says everything is pushed:
$(echo "$UNTRACKED" | sed 's/^/       /')
       Commit them, or add them to .gitignore if a command in this repo rebuilds them."

git fetch -q origin
LOCAL=$(git rev-parse HEAD); REMOTE=$(git rev-parse origin/main)
BASE=$(git merge-base HEAD origin/main)
[ "$LOCAL" = "$REMOTE" ] || { [ "$REMOTE" = "$BASE" ] || die "origin/main has commits you do
       not have. Rebase and re-run the gates on the combined tree before releasing."; }

git rev-parse -q --verify "refs/tags/$TAG" >/dev/null && die "tag $TAG already exists."

# ---------------------------------------------------------------- 2. the changelog
if ! grep -q "^## .*v$NEXT" docs/CHANGELOG.md; then
    die "docs/CHANGELOG.md has no entry for v$NEXT.

       Every build gets one, in the format the file already uses, and SHORT is the rule:
       a heading, category headings, bullets, nothing else.

           ## OpenCNC3D v$NEXT \"Some Name\" ($(date '+%Y-%m-%d'))

           ### New features

           - one line per item, saying WHAT changed, not why or how

           ### Bugs fixed

           - ...

       Categories in order, only the ones that apply: New features, Improvements,
       Performance, Platforms and builds, Bugs fixed. No prose paragraphs, no
       'Covers up to', no known-issues section; gaps are recorded separately, not here.

       Write it first. It is the only record of what changed between two builds, and it
       is what announces the build."
fi
say "changelog entry for v$NEXT found"

# THE OTHER HALF OF THE SAME RULE: an EARLIER build whose entry went into the changelog and
# whose release never appeared on GitHub. Cutting a new build on top of that buries it, and
# the changelog goes on naming versions nobody can download. The check passes while v$NEXT
# itself is written-but-not-yet-cut, which is the state this script requires two checks
# above; it fails only on a version older than the one in VERSION.
tools/changelog-check.sh || die "changelog and GitHub have drifted (above). A build updates
       the changelog AND the builds on GitHub, so the older gap gets closed before a new
       build is stacked on top of it."

# ------------------------------------------------- 2b. the install wizard and its downloads
# THE WINDOWS INSTALLER IS PART OF THE WINDOWS HALF, not an extra. It is how a Windows
# player is asked for the movies and the music, so a release without it is half a Windows
# build, and the tool that makes it is asked for here, before anything is built, rather
# than found missing twenty minutes in. The dry run asks too: a rehearsal that passes
# where the real run would stop is not a rehearsal.
command -v makensis >/dev/null 2>&1 || die "makensis is not installed, so the Windows installer
       (OpenCNC3D-Setup) cannot be built, and a release is both halves with the Windows
       install wizard in it. brew install makensis, then run this again. Nothing was built."

# EVERY MOVIE AND EVERY MUSIC FILE CAN STILL BE DOWNLOADED, asked of the mirrors themselves
# before anything is built or tagged. No package carries them, so a dead mirror is a box
# on the install wizard that fails for every player who leaves it ticked, and nothing in
# the build itself would notice. tools/media/probe-sources.py reads the first and last
# 64 KB of every file from every mirror and checks them against the recorded windows, and
# checks that the copy compiled into the launcher is the manifest that was probed. It
# takes about two minutes and needs the network; -I so nothing in the working directory
# can stand in for the standard library.
say "probing the movies and music mirrors"
command -v python3 >/dev/null 2>&1 || die "python3 is not installed, and the media probe needs it."
PROBELOG=$(mktemp)
_probe=0
python3 -I tools/media/probe-sources.py > "$PROBELOG" 2>&1 || _probe=$?
sed 's/^/   /' "$PROBELOG"
rm -f "$PROBELOG"
case "$_probe" in
0) ;;
1) die "the media probe found a movie or music file that NO mirror serves correctly (the
       NO GOOD MIRROR lines above). A player ticking that box would get a failure.
       Fix tools/media/media-sources.txt and regenerate the manifest
       (tools/media/README.md) before cutting a build. Nothing was built." ;;
2) die "the media manifest, its windows file and the copy compiled into the launcher
       (launcher/lmedia_manifest.c) are missing or disagree (above). The launcher would
       download by a list nobody probed. Regenerate with
       tools/media/make-media-manifest.py. Nothing was built." ;;
3) die "the media probe reached no mirror at all, so whether the movies and the music can
       be downloaded is unknown. Check the network and run this again. Nothing was built." ;;
*) die "the media probe ended with exit status $_probe (its output is above), so whether
       the movies and the music can be downloaded is unknown. Nothing was built." ;;
esac
say "every movie and music file has a mirror serving the recorded bytes"

# ---------------------------------------------------------------- 3. stamp the number
say "stamping $TAG"
echo "$NEXT" > VERSION
# --release, so the binaries carry the bare number rather than a sha: the very next thing
# this script does on success is commit and tag exactly this tree.
BUILDSTR=$(tools/version.sh --release)
tools/version.sh --release-header game/opencnc3d_build.h
[ "$BUILDSTR" = "v$NEXT" ] || { git checkout -q -- VERSION
    die "VERSION reads back as $BUILDSTR after stamping, not v$NEXT."; }

# WHAT A RELEASE BINARY IS, read off the file rather than the flags that made it. Every
# binary built against game/opencnc3d_build.h carries its build mark (tools/version.sh
# writes it; game/build.sh force-includes it into cnc_eyes): one string naming the build,
# ending "|cooked" when OPENCNC3D_COOKED reached that compile. A release binary carries
# exactly MARK: this number, and no F5 panel. A binary left from another build fails it,
# and so does a cook that was asked for and not compiled in.
MARK="OPENCNC3D|build|$BUILDSTR|cooked"
HEADERLINE="#define OPENCNC3D_BUILD \"$BUILDSTR\""
has_mark() {   # has_mark <file>
    strings -a "$1" 2>/dev/null | grep -qxF "$MARK"
}
marks_of() {   # what a file says it is instead, for the refusal
    strings -a "$1" 2>/dev/null | grep '^OPENCNC3D|build|' | sort -u | tr '\n' ' '
}
header_is_release() {
    grep -qxF "$HEADERLINE" game/opencnc3d_build.h
}

# NO PACKAGE CARRIES THE MOVIES OR THE MUSIC. They come from the freeware disc images at
# install time, through the Windows installer or the launcher's Setup screen, and only for
# a player who leaves those boxes ticked. Both packagers leave them out; this asks the
# folders and the zips themselves, because the question is what a player is handed.
#
# The paths are tools/media/media-paths.txt, the one list both packagers and the data
# fingerprint read: one path per line relative to the package root, "/dosdata/movies/" a
# folder, "/dosdata/SCORES.MIX" a file, '#' a comment. The leading "/" is optional, as it
# is for every other reader of the list, so a line written either way means the same. The
# Red Alert module's movies and score are on it and are refused like the rest. The copy
# below is the same list, for a checkout without that file. Each path may sit under the
# one folder a full zip wraps everything in, and on top of the list, anything that is a
# movie or a score by its name, and the record and partial files a media download leaves
# in an install, are refused wherever they are.
media_paths() {   # media_paths: the list, one path per line, with no leading "/"
    if [ -f tools/media/media-paths.txt ]; then
        sed -e 's/#.*//' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' \
            -e 's|^\./||' -e 's|^/||' tools/media/media-paths.txt | grep -v '^$' || true
    else
        printf '%s\n' dosdata/movies/ dosdata/music/ dosdata/SCORES.MIX \
            dosdata/TRANSIT.MIX dosdata/transit/TRANSIT.MIX movies/ \
            ra3d/movies/ ra3d/audio/SCORES.MIX ra3d/data/SCORES.MIX
    fi
}
# Each path as a literal: every character a regular expression would read as an operator
# is escaped, and a file (no trailing "/") must end the name.
MEDIA_ERE=$(media_paths | awk '
    {
        e = ""
        for (i = 1; i <= length($0); i++) {
            c = substr($0, i, 1)
            if (index(".[]()*+?{}|^$\\", c)) e = e "\\" c
            else e = e c
        }
        if (substr($0, length($0), 1) != "/") e = e "$"
        printf "%s^([^/]+/)?%s", (n++ ? "|" : ""), e
    }')
[ -n "$MEDIA_ERE" ] || { git checkout -q -- VERSION; tools/version.sh --header game/opencnc3d_build.h
    die "tools/media/media-paths.txt names no path, so nothing would keep the movies and
       the music out of a package. Nothing is built."; }
MEDIA_ERE="$MEDIA_ERE|\.vqa$|(^|/)(scores|transit)\.mix$|(^|/)opencnc3d-media\.txt$|\.ocpart$"
media_names() {   # media_names: the media among the paths on stdin, at most eight of them
    grep -i -E "$MEDIA_ERE" | head -8
}

# AND THE PANEL'S SWITCH OPENS NOTHING, asked of the program as well as its bytes. cnc_eyes
# reads its switches before it opens a window or loads anything: on a developer build
# --gfxpanel opens the tuning panel and prints FX|panel|open, and the unknown switch after
# it ends the run with the usage text. The usage text is what proves the run happened, so
# a missing "open" is a measurement, not a binary that never started. No window, no brain,
# no data, and nothing is written: it runs in an empty folder of its own.
panel_refused() {   # panel_refused <absolute path to cnc_eyes>
    _pd=$(mktemp -d)
    _po=$(cd "$_pd" && "$1" --gfxpanel --release-probe-ends-here 2>&1 </dev/null || true)
    rm -rf "$_pd"
    printf '%s\n' "$_po" | grep -q '^usage: cnc_eyes' || return 2
    ! printf '%s\n' "$_po" | grep -q '^FX|panel|open'
}

# THE DEVELOPER BUILD BACK INTO playable/. Step 4 stages the COOKED packages there (no
# F5 panel), and playable/ is the folder this machine plays and tunes from: the desktop
# shortcut opens playable/OpenCNC3D.app. Every path out of this script therefore ends here,
# the dry run and the failure paths included. The dry run used to skip it, and since a
# session ends with a dry run far more often than with a release, the developer's own
# game lost its F5 panel after nearly every session and stayed that way until somebody
# happened to run game/build.sh. Three minutes of rebuild is the price, on every path.
dev_build_back() {
    say "putting the developer build back into playable/ (uncooked, with F5)"
    if (cd game && sh build.sh >/dev/null 2>&1) && (cd app && sh build.sh >/dev/null 2>&1); then
        if strings -a playable/opencnc3d playable/cnc_eyes 2>/dev/null | grep -q '^OPENCNC3D|build|.*|cooked$'; then
            echo "NOTE: playable/ STILL holds a cooked binary after the rebuild; F5 is" >&2
            echo "      missing. Run game/build.sh and app/build.sh by hand and look." >&2
        else
            echo "playable/ holds the developer build again (F5 is back)"
        fi
    else
        echo "NOTE: the developer rebuild failed; playable/ still holds the COOKED binaries" >&2
        echo "      (no F5). Run game/build.sh and app/build.sh by hand." >&2
    fi
}

restore() {
    git checkout -q -- VERSION 2>/dev/null || true
    tools/version.sh --header game/opencnc3d_build.h
    dev_build_back
}

# ---------------------------------------------------------------- 4. build both halves
say "building macOS"
# OPENCNC3D_COOKED=1: THE PACKAGES ARE COOKED. The F5 tuning panel's player doors (the key
# and --gfxpanel) are compiled out of the binaries this cuts, and both packagers refuse a
# cooked package whose binaries do not carry the refusal. This is the only place in the
# tree that knows a build is a release, so it is the only place the flag is set: a
# developer's own build never sees it. The gates below run on these cooked binaries --
# the ones that ship are the ones proved -- and step 7b puts the developer build back
# into playable/ afterwards, uncooked, so this machine keeps F5.
OPENCNC3D_COOKED=1 OPENCNC3D_SKIP_VERSION_HEADER=1 OPENCNC3D_BUILD_ID="$TAG" OPENCNC3D_NO_RA=$NO_RA \
    game/make-build.sh "$ROOT/playable" || { restore; die "the macOS build failed."; }

# THE macOS BINARIES ARE THIS RELEASE, COOKED. The header first, because every binary is
# compiled against it, then each binary's own mark, then the panel's switch run for real.
header_is_release || { restore; die "game/opencnc3d_build.h does not say $BUILDSTR after the
       macOS build, so the binaries were stamped with something else. Nothing is tagged."; }
for b in cnc_eyes opencnc3d; do
    has_mark "playable/$b" || { _m=$(marks_of "playable/$b"); restore; die "playable/$b is not
       a cooked $BUILDSTR: it should carry $MARK and carries ${_m:-no build mark at all}.
       Without |cooked it still has the F5 panel. Nothing is tagged."; }
done
panel_refused "$ROOT/playable/cnc_eyes" || { _r=$?; restore
    [ "$_r" = "2" ] && die "playable/cnc_eyes did not run far enough to read its switches (no
       usage text), so whether its F5 panel opens is unknown. Nothing is tagged."
    die "playable/cnc_eyes opened the F5 panel when asked with --gfxpanel. A cooked build
       has no player's door to it. Nothing is tagged."; }
say "macOS: cnc_eyes and opencnc3d are $BUILDSTR, cooked, and --gfxpanel opens nothing"

say "building Windows"
# A dry run still COMPILES the Windows half, because "does it build" is the whole point;
# it just does not spend minutes zipping 845 MB of game data nobody is going to send.
#
# OPENCNC3D_PLAYABLE names the folder the macOS half was just built into. The Windows
# packager takes its game data from a play folder, and without this it takes the one at
# its own default path, which is a different folder whenever this runs from any other
# checkout: two halves of one release built from two sets of data.
WINARG=""
[ "$DRY" = "1" ] && WINARG="--no-zip"
# Where the Windows packager leaves its zips: its own default, or OPENCNC3D_WIN_ZIP when the
# operator has set it, read here the same way so the two cannot look in different places.
WINDIST="${OPENCNC3D_WIN_ZIP:-$HOME/Desktop}"
OPENCNC3D_COOKED=1 OPENCNC3D_SKIP_VERSION_HEADER=1 OPENCNC3D_BUILD_ID="$TAG" OPENCNC3D_NO_RA=$NO_RA \
    OPENCNC3D_PLAYABLE="$ROOT/playable" tools/win/make-build-win.sh $WINARG || { restore; die "the Windows build failed.
       Both platforms come from one commit, so a Windows failure stops the release rather
       than shipping half of it."; }

# THE WINDOWS BINARIES, THE SAME QUESTION. They cannot be run here, so this is their bytes
# only, asked of the staged folder both the zip and the installer are made from. The header
# again too: the Windows build regenerates it unless told not to, and a header rewritten
# between the halves stamps the two platforms differently.
WINSTAGE="$ROOT/build/win-dist/OpenCNC3D-windows-$TAG"
header_is_release || { restore; die "game/opencnc3d_build.h no longer says $BUILDSTR after the
       Windows build. Nothing is tagged."; }
for b in opencnc3d-game.exe cnc_eyes.exe; do
    has_mark "$WINSTAGE/$b" || { _m=$(marks_of "$WINSTAGE/$b"); restore; die "$WINSTAGE/$b is
       not a cooked $BUILDSTR: it should carry $MARK and carries ${_m:-no build mark at
       all}. cnc_eyes.exe carries it only when tools/win/build-win.sh force-includes
       game/opencnc3d_build.h, as game/build.sh does. Nothing is tagged."; }
done
if [ "$NO_RA" = "1" ]; then
    [ ! -e "$WINSTAGE/ra3d" ] && [ -z "$(find "$WINSTAGE" -iname 'redalert*' | head -1)" ] \
        || { restore; die "the staged Windows folder carries Red Alert ($WINSTAGE), and this
       release leaves it out. Nothing is tagged."; }
fi
# The staged folder is what the Windows zip is made from, and a dry run, which zips
# nothing, still asks it.
_wm=$(cd "$WINSTAGE" && find . -print | sed 's|^\./||' | media_names)
[ -z "$_wm" ] || { restore; die "the staged Windows folder carries movies or music
       ($WINSTAGE), and no package does:
$(echo "$_wm" | sed 's/^/           /')
       The Windows packager has to leave out every path on the media list. Nothing is
       tagged."; }
say "Windows: opencnc3d-game.exe and cnc_eyes.exe are $BUILDSTR, cooked, and the folder carries no movies or music"

# AND NOTHING THAT SHIPS TELLS A PLAYER TO PRESS F5. A cooked build has no F5 panel, so a
# key list naming it describes a key that does nothing. Asked of the two read-me files a
# player is handed: the macOS one is copied from its tracked master, the Windows ones are
# written into the staged folder by the Windows packager.
for f in tools/launchers/READ-ME.txt "$WINSTAGE"/READ-ME*.txt; do
    [ -f "$f" ] || continue
    _f5=$(grep -n -w 'F5' "$f" || true)
    [ -z "$_f5" ] || { restore; die "$f, which ships in this release, still names F5:
$(echo "$_f5" | sed 's/^/           /')
       The released build has no F5 panel. Nothing is tagged."; }
done

# THE WINDOWS ZIPS END IN A FIXED ORDER, asked of the files that will be published, the
# full one before anything is tagged. Every installed launcher up to v0.6.11 unpacks in
# zip order, writes the install record whenever the zip reaches it, and stops at
# SDL2.dll, which it has loaded. So the full package ends CHANGELOG.txt,
# opencnc3d-install.txt, BUILD-ID.txt, SDL2.dll: such a launcher stopped anywhere earlier has
# not yet claimed the new version, and one that reaches SDL2.dll has written everything
# else. The binary-only zip carries no record, ends BUILD-ID.txt, SDL2.dll, and is FLAT,
# because those launchers unpack it without stripping a folder. make-build-win.sh makes
# both shapes and refuses a zip without them; this is the same question where it ships.
win_zip_tail() {   # win_zip_tail <zip> <prefix> <name>...: its last entries, in this order
    _z=$1
    _p=$2
    shift 2
    [ "$(unzip -Z1 "$_z" 2>/dev/null | tail -n $#)" = "$(for _f in "$@"; do printf '%s%s\n' "$_p" "$_f"; done)" ]
}

# AND SDL2.DLL IS THE PINNED ONE. Listed last it is the one file an old launcher fails to
# write, which is harmless exactly while the shipped DLL is byte identical to the one
# installed. A release whose SDL2.dll differs leaves every player updating with such a
# launcher on the old DLL under a record naming the new version. The DLL is pinned by
# content beside SDL2_VER in tools/win/setup-toolchain.sh, whose note says when moving the
# pin is safe; OPENCNC3D_SDL2_CHANGE=1 releases anyway. Asked of the staged folder too, so a
# dry run, which zips nothing, still asks it.
SDL2_DLL_PIN=$(sed -n 's/^SDL2_DLL_SHA256=//p' tools/win/setup-toolchain.sh)
win_sdl_pinned() {   # win_sdl_pinned <sha256 of the SDL2.dll about to ship>
    [ "$OPENCNC3D_SDL2_CHANGE" = "1" ] && return 0
    [ -n "$SDL2_DLL_PIN" ] && [ "$1" = "$SDL2_DLL_PIN" ]
}
SDL2_NOT_PINNED="is not the pinned one (SDL2_DLL_SHA256 in
       tools/win/setup-toolchain.sh). Launchers up to v0.6.11 cannot replace SDL2.dll, so a
       player updating with one would keep the old DLL under a record naming $TAG. If the
       change is meant, read the note at the pin and re-run with OPENCNC3D_SDL2_CHANGE=1."
WINSDL="build/win-dist/OpenCNC3D-windows-$TAG/SDL2.dll"
[ -f "$WINSDL" ] && win_sdl_pinned "$(shasum -a 256 "$WINSDL" | cut -d' ' -f1)" \
    || { restore; die "the staged Windows SDL2.dll ($WINSDL) $SDL2_NOT_PINNED Nothing is tagged."; }
if [ "$DRY" != "1" ]; then
    WINFULL="$WINDIST/OpenCNC3D-windows-$TAG.zip"
    win_zip_tail "$WINFULL" "OpenCNC3D-windows-$TAG/" CHANGELOG.txt opencnc3d-install.txt BUILD-ID.txt SDL2.dll \
        || { restore; die "the Windows package does not end CHANGELOG.txt, opencnc3d-install.txt,
       BUILD-ID.txt, SDL2.dll ($WINFULL). Every installed launcher up to v0.6.11 writes the
       record where the zip lists it and stops at SDL2.dll, so a player updating with one
       could be left claiming $TAG over files it never wrote. Nothing is tagged."; }
    win_sdl_pinned "$(unzip -p "$WINFULL" "OpenCNC3D-windows-$TAG/SDL2.dll" | shasum -a 256 | cut -d' ' -f1)" \
        || { restore; die "the SDL2.dll in $WINFULL $SDL2_NOT_PINNED Nothing is tagged."; }
    _wm=$(unzip -Z1 "$WINFULL" 2>/dev/null | media_names)
    [ -z "$_wm" ] || { restore; die "the Windows package carries movies or music ($WINFULL):
$(echo "$_wm" | sed 's/^/           /')
       No package does. Nothing is tagged."; }
fi

# THE WINDOWS INSTALLER, BUILT BEFORE ANYTHING IS TAGGED. It is the Windows half of the
# movies-and-music wizard: a small web installer that installs the launcher, then has it
# download the newest Windows package and the movies and music the player ticked, on one
# progress page. It is REQUIRED: makensis was asked for in step 2b, and an installer that
# fails to build stops the release here, before the tag, rather than after it with the
# tag pushed and nothing published. It is built from the same staged folder the zip was
# made from, so it installs that package's own launcher.
#
# A dry run builds it too, into a folder of its own that is thrown away, because "does it
# build" is the question a dry run is for. tools/win/make-installer-win.sh writes
# OpenCNC3D-Setup-$TAG.exe into the folder OPENCNC3D_WIN_ZIP names.
say "building the Windows installer"
if [ "$DRY" = "1" ]; then SETUPDIR=$(mktemp -d); else SETUPDIR="$WINDIST"; fi
mkdir -p "$SETUPDIR"
OPENCNC3D_WIN_ZIP="$SETUPDIR" tools/win/make-installer-win.sh "$WINSTAGE" \
    || { restore; die "the Windows installer failed to build (its output is above). The
       installer is part of the Windows half, and a release is both halves. Nothing is
       tagged."; }
WINSETUP="$SETUPDIR/OpenCNC3D-Setup-$TAG.exe"
[ -f "$WINSETUP" ] || { restore; die "tools/win/make-installer-win.sh succeeded and left no
       $WINSETUP. Nothing is tagged."; }
# SMALL, because it carries the launcher and not the game: the game is downloaded by
# the launcher it installs. A Setup.exe the size of the package is the old full
# installer, which can no longer hold the game (an NSIS installer stops at 2 GB) and
# would carry the movies and music besides.
_ss=$(wc -c < "$WINSETUP" | tr -d ' ')
[ "$_ss" -le 67108864 ] || { restore; die "the Windows installer is $((_ss / 1048576)) MB
       ($WINSETUP). The web installer carries the launcher only and is a few MB; one this
       large is carrying the game. Nothing is tagged."; }
echo "   $(basename "$WINSETUP"): $((_ss / 1024)) KB"
if [ "$DRY" = "1" ]; then rm -rf "$SETUPDIR"; WINSETUP=""; fi

# ---------------------------------------------------------------- 5. the gates
say "gates"
GLOG=$(mktemp)

# THE PARKED GATES, deferred by decision so that v0.6.3 could ship, to be fixed in the
# rounds after it.
#
# THIS IS A DELIBERATE WEAKENING OF A RELEASE GATE and it is written as a NAMED list
# rather than a count so that it can only ever excuse the gates named on it. Any other
# gate that goes red still stops the release, and so does a suite that dies before it
# finishes. The list is meant to shrink, not to grow by habit.
#
#   G21   hovercraft riders draw off the deck (jeep=0, deckpx=0, outside=1066)
#   G36g  bilinear fringe leaves 13 key-colour pixels behind
#   G90   death shed: a batched tick steps over the 8-tick death window, so
#         nothing is observed dying and the pixel arm is vacuous
#   G104  mission reset: the same batched-tick skip, same vacuous result
#   G44   a dynamic light fading when its source ends
#   G50   the flamethrower's tongue
#   G133  a dying vehicle's tap on the ground
#         These three share one cause: the engine reads the 1995 anim art in every
#         mode, which gives those anims their 1995 timing. The behaviour is kept and
#         the effect timing is decided later.
#   G135  the Enhanced HUD's multiplayer player list (the Map button's third state),
#         deferred until after this release.
#
# ALL FIVE ARE PRE-EXISTING, and that is measured rather than assumed: a peer session
# built a pre-work binary at 71f59f1 in an isolated run folder and reproduced all five
# identically, same numbers, same assertions. Nothing in 0.6.3 caused any of them.
# G90 and G104 are ONE bug with a diagnosed cause -- the sim-advance loop observes only
# the last tick of a batched `tick N` -- and the fix belongs there, not in the scripts.
#
# G81 WAS PARKED HERE AND IS NOT NOW, because its red was never the game. It blamed a
# smoothing bug, "one unit in three keeps smoothing through a teleport", and the gate was
# in fact sampling a rifleman who was never aboard the hovercraft. Fixed in the gate: it
# samples the deck's own three riders, requires every sample to have smoothing on, and
# passes on the real build. The other four above are unchanged.
#
# G130 WAS PARKED FOR v0.6.4 AND IS NOT PARKED NOW, because the waiver asked for one of two
# things and both were done: the intermittency is explained AND the gate is hardened.
#
# The signature the waiver recorded was the two peers disagreeing on order count and turn
# over real UDP, passing standalone and going red inside the suite, trigger unknown. The
# trigger was in the TEST, not the scheduler. Its turn loop sent and drained in the same
# breath, so a datagram still in flight when the drain ran was covered by the redundancy in
# the next turn's packet; on the last turn there is no next packet, so one late datagram
# left the peers a turn apart and failed three legs at once. Under the suite's scheduling
# pressure that becomes reachable; standalone it almost never is.
#
# Reproduced rather than reasoned about: skipping seat 0's drain on the final turn alone
# gives exactly the recorded signature, 13 passed and 3 failed, seat 0 on 74 orders against
# seat 1's 75. The gate now settles both peers before it asserts, and the same injected
# fault passes at 17 of 17, as do forty concurrent runs under eight CPU burners.
#
# WHAT IS NOT PROVEN: this diagnosis was established on Linux, and the red was seen on
# macOS inside the suite. The mechanism is platform independent and the reproduction is
# exact, but no suite run has confirmed it. If G130 goes red again the diagnosis was
# incomplete rather than wrong, and the next reading to take is whether the two peers are
# still a turn apart or something else has changed.
# G194 IS UNPARKED AS OF 4 SEP 2026 AND MUST NOT GO BACK ON THIS LIST. It was parked for
# v0.6.5 only, reporting two instances of the engine diverging after a SELL order in one
# process with no networking involved, which is the one defect class lockstep cannot
# tolerate. The condition written here was that any release CLAIMING a networking feature
# had to take it off this list and go green first.
#
# It is off the list because it is FIXED, not because the condition was waived.
# BuildingClass::Sell_Back took the flashing house from PlayerPtr behind an
# Is_Owned_By_Player() guard, which is `House == PlayerPtr`, so the seller's machine set
# FlashCount to 7 and every other peer left it 0. Under lockstep the house now comes from
# the object rather than from the machine. Two sibling sites went with it, in both brains.
# The fix was mutation tested by putting the defect back and watching the gate report the
# original signature again.
#
# THE STANDING RULE SURVIVES THE FIX and is what to read if this ever comes up again: a
# release with a networking feature in its changelog cannot park G194, because a lockstep
# that can disagree with itself about a sell is not a lockstep.
PARKED_GATES="G21 G36g G90 G104 G44 G50 G133 G135"

# THE INTERACTIVE GATES ARE OFF UNLESS THE OPERATOR SAYS OTHERWISE, AT THE TIME. A handful
# of gates cannot run in the background because what they measure IS a focused window and
# a real pointer (G124 is the list today): they raise the game over whatever the operator
# is doing and move the mouse. A release run happens while somebody works at the machine,
# so taking the desk is never the default, not even for a release. --interactive-gates
# runs them, and is for a moment when the operator has said the machine is free.
#
# Without it, those gates are the ONLY skips allowed, and the run says so out loud below:
# the edge-scroll gesture through the live loop is then not covered by this release.
INTERACTIVE_GATE_LIST="G124"
# THE RED ALERT GATES run against a Red Alert module staged in the play folder, and skip
# when there is none. A release that leaves Red Alert out (NO_RA, at the top) does not
# stage one, so on a play folder without it these two skip, and that is the only case in
# which they are allowed to. Where a module IS staged they run, and a red one still stops
# the release: the renderer code they exercise ships either way.
RA_GATE_LIST="G310 G311"
OPENCNC3D_GATES_INTERACTIVE=$INTERACTIVE_GATES RUNDIR="$ROOT/playable" sh playable/gates.sh "$TAG" > "$GLOG" 2>&1 || true
TAIL=$(tail -1 "$GLOG")
echo "$TAIL"

# THE SUITE MUST HAVE FINISHED, and its own summary line is the proof. This used to be
# read off gates.sh's EXIT CODE, which is non-zero whenever any gate fails -- so a suite
# that ran perfectly well and reported "120 pass, 5 fail" was announced as "the gate
# suite did not finish", sending the reader after a crash that never happened.
# AND IT MUST NOT HAVE SKIPPED ANYTHING ELSE. With --interactive-gates every gate was asked
# to run, so any skip means one refused. Without it, the interactive gates are skipped on
# purpose and are named here every time; any OTHER skip still stops the release, because a
# release with unrun gates it did not choose to leave out has unknown coverage.
ALLOWED_SKIPS=""
[ "$INTERACTIVE_GATES" = "1" ] || ALLOWED_SKIPS="$INTERACTIVE_GATE_LIST"
[ "$NO_RA" = "1" ] && ALLOWED_SKIPS="$ALLOWED_SKIPS $RA_GATE_LIST"
UNEXPECTED_SKIPS=""
RA_SKIPPED=""
for g in $(grep '^SKIP' "$GLOG" | awk '{print $2}'); do
    case " $ALLOWED_SKIPS " in
        *" $g "*) case " $RA_GATE_LIST " in *" $g "*) RA_SKIPPED="$RA_SKIPPED $g" ;; esac ;;
        *) UNEXPECTED_SKIPS="$UNEXPECTED_SKIPS $g" ;;
    esac
done
if [ -n "$UNEXPECTED_SKIPS" ]; then
    echo "the gate suite SKIPPED gates it was expected to run:$UNEXPECTED_SKIPS" >&2
    echo "  $TAIL" >&2
    echo "a release cannot be cut on a suite that did not run every gate." >&2
    exit 1
fi
if [ "$INTERACTIVE_GATES" != "1" ]; then
    echo "NOTE: the interactive gates ($INTERACTIVE_GATE_LIST) were NOT run, so the desk was not"
    echo "      taken; the edge-scroll gesture through the live loop is not covered by this"
    echo "      release. tools/release.sh --interactive-gates runs them when the machine is free."
fi
if [ -n "$RA_SKIPPED" ]; then
    echo "NOTE: the Red Alert gates$RA_SKIPPED skipped: no Red Alert module is staged, and this"
    echo "      release ships none."
fi

case "$TAIL" in
*" pass, "*" fail"*) : ;;
*) tail -40 "$GLOG"; restore; die "the gate suite did not finish -- it never printed a
       summary line, so it died part way. Log: $GLOG" ;;
esac

# EVERY FAILING GATE MUST BE ON THE PARKED LIST. One that is not stops the release.
UNPARKED=""
for g in $(grep '^FAIL' "$GLOG" | awk '{print $2}'); do
    case " $PARKED_GATES " in
        *" $g "*) : ;;
        *) UNPARKED="$UNPARKED $g" ;;
    esac
done
if [ -n "$UNPARKED" ]; then
    tail -40 "$GLOG"; restore; die "gates failed that are NOT parked:$UNPARKED
       $TAIL
       The parked list is $PARKED_GATES and nothing else may be red. Nothing is tagged
       or pushed. Full log: $GLOG"
fi
case "$TAIL" in
*" 0 fail"*) : ;;
*) say "PARKED gates are red and were allowed through: $PARKED_GATES
   $TAIL
   These are known, pre-existing and deferred. Any OTHER red would have
   stopped this release." ;;
esac
# The Windows binaries are cross compiled here and cannot be run, so this suite is the
# MAC half only. Said out loud rather than implied, because "the gates passed" has meant
# two different things on this project before.
say "gates green on macOS. The Windows binaries are cross compiled and UNTESTED here:
   they have to be run on Windows (the Windows build notes)."

# ------------------------------------------------------------- 5b. G136, the editor
# THE EDITOR BUTTON IS ONLY WORTH ENABLING IF THE BUILD BEHIND IT HAS AN EDITOR IN IT.
# That button was drawn grey for as long as the editor existed in the tree and not in
# anything a player could be handed, and the danger in turning it on is a build that
# forgets the editor and ships a button which fails at the moment it is pressed. This
# runs BEFORE the tag, so such a build is never tagged. The Windows half of the same
# check lives in the Windows packager and runs earlier still.
#
# LEG 1: THE BUTTON IS DRAWN ENABLED, and the evidence is pixels rather than a reading
# of the source. The launcher renders one frame to a PNG with no window and no GL,
# which is what makes this cheap enough to run every release.
#
# IT ASKS THE LAUNCHER WHERE THE BUTTON IS. This check used to carry its own copy of the
# row's geometry and reach for the button by index, and both went stale the day a fourth
# button joined the row: the four were narrowed, every index moved along, and a sample
# aimed at Editor landed on the label of the button beside it and read a correct build as
# broken. The launcher now prints a rect per row button beside the picture, so the layout
# lives in one place. The fill is taken as the commonest colour inside the border rather
# than a fixed block, because that is the button's own colour whatever its label reads
# and however long it is. Measured in both states: the enabled fill is (48,84,44) and the
# disabled fill this button carried until it went live is (84,84,84). The test compares
# Editor's fill with PLAY's rather than with a literal colour, so restyling the buttons
# cannot turn it red on its own, and it reads the drawn disabled flag as well, so a
# button greyed by a state this shot did not reach still fails here.
#
# python3 with PIL is not a new dependency here: the gate suite that just ran above is
# built on it, so any machine that reaches this line already has it.
say "G136 the editor"
G136DIR=$(mktemp -d)
playable/opencnc3d-launcher --dir playable --shot "$G136DIR/launcher.png" --scale 1 \
    >"$G136DIR/buttons.txt" 2>/dev/null || { restore; die "the launcher could not draw
       its own screen, so the EDITOR button cannot be checked. Nothing is tagged."; }
python3 - "$G136DIR/launcher.png" "$G136DIR/buttons.txt" <<'G136PY' || { restore; die "the launcher's EDITOR
       button is drawn DISABLED. Enabling it is one line in the launcher, and a release
       that ships it grey ships a front door with the editor locked behind it. Nothing
       is tagged."; }
import sys
from collections import Counter
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
row = {}
for line in open(sys.argv[2]):
    if line.startswith("LBTN|"):
        f = dict(p.split("=", 1) for p in line.rstrip("\n").split("|")[1:])
        row[f["label"]] = f
for want in ("Play", "Editor"):
    if want not in row:
        print("   G136 the button row has no %s in it, only: %s"
              % (want, ", ".join(sorted(row)) or "nothing"))
        sys.exit(1)
def fill(b):
    x, y, w, h = (int(b["x"]), int(b["y"]), int(b["w"]), int(b["h"]))
    inside = [im.getpixel((x + dx, y + dy))
              for dx in range(2, w - 2) for dy in range(1, h - 1)]
    return Counter(inside).most_common(1)[0][0]
play, editor = fill(row["Play"]), fill(row["Editor"])
print("   G136 button fill: play=%s editor=%s (Editor is button %s at x=%s, drawn "
      "disabled=%s)" % (play, editor, row["Editor"]["i"], row["Editor"]["x"],
                        row["Editor"]["disabled"]))
sys.exit(1 if row["Editor"]["disabled"] != "0" or editor != play else 0)
G136PY
rm -rf "$G136DIR"

# LEG 2: WHAT THE EDITOR OPENS WITH IS IN THE FOLDER THE PACKAGE IS BUILT FROM. The
# macOS package is an allow-list over playable/, so anything absent here is absent
# there, and finding that out before the tag is the reason this is checked here and not
# only after the package is assembled.
#
# The list is short because most of the editor is not on disk at all: its font, its
# emblem and its legality tables are compiled into the binary as generated headers.
# These three were measured by removal rather than guessed: the editor opens with no
# cameos.pack, no dosinfantry.pack and no CONQUER.MIX, and refuses to open without any
# one of them.
for f in cnc_eyes dossidebar.pack content; do
    [ -e "playable/$f" ] || { restore; die "playable/$f is missing and the editor
       cannot open without it. The macOS package is an allow-list over that folder, so
       the package would carry a live EDITOR button with nothing behind it. Nothing is
       tagged."; }
done

# FIVE MAPS, NOT ONE, and they are not examples. SCG01EA is what the button passes to
# --scen, and the other four are the editor's donors: a map made in the editor has no
# baked terrain of its own and borrows the pack of a donor scenario in its THEATER, one
# each for desert, winter, snow and sand. Ship four of the five and MAP > New Map opens
# a blank grey map in whichever theater was dropped, with nothing on screen to say why.
# Both halves are needed because a pack draws the ground and the INI is what is read.
for f in SCG01EA SCB01EA SCW01EA SCS01EA SCA01EA; do
    [ -f "playable/$f.pack" ] && [ -f "playable/missions/$f.INI" ] || { restore
        die "the editor's $f donor is incomplete in playable/: it needs BOTH $f.pack and
       missions/$f.INI. Every map made in that theater borrows this pack, so a build
       without it opens MAP > New Map on blank ground. Nothing is tagged."; }
done
strings -a playable/cnc_eyes 2>/dev/null | grep -q 'edit: world grid' \
    || { restore; die "playable/cnc_eyes carries no editor: the editor-only diagnostic
       string is not in it, and the EDITOR button runs that binary with --edit. Nothing
       is tagged."; }
say "G136 the EDITOR button is drawn live and the editor it opens is in the build"

# ------------------------------------------------- 5c. the macOS package, before the tag
# WHAT GETS ATTACHED: THE PLAYABLE BUILDS, DATA AND ALL, EXCEPT THE MOVIES AND THE MUSIC.
# (a project rule, after v0.5.1 first went out with binaries only.) A release that cannot
# be played is not a build, it is a source archive with extra steps. Both platforms ship
# the whole folder: unzip it and double-click, with nothing to bake. The one thing left
# out is the 1995 movies and score, which no package may carry: the Windows installer and
# the launcher's Setup screen download them from the disc images on archive.org for the
# player who wants them, and the game plays cleanly without them.
#
# THE MAC PACKAGE IS ASSEMBLED HERE, BEFORE THE TAG, and that is the reason for its place.
# It used to be made after the tag was pushed, so every refusal the packager can make (a
# piece of the allow-list missing, a movie or the score in the folder) left a pushed tag
# with nothing published. Now each of them stops the release with nothing committed. The
# Windows package was staged, asked and zipped in step 4 for the same reason. A dry run
# assembles and asks this one too, and zips nothing, as the Windows half does.
#
# THE MAC PACKAGE IS BUILT FROM AN ALLOW-LIST, in tools/mac/make-package-mac.sh, which
# sits beside the Windows script it is modelled on so the two read the same way. Until
# this step was
#
#     cp -RL playable "$STAGEDIR/$MACFULLNAME"
#     rm -f  "$STAGEDIR/$MACFULLNAME"/BUILD-HISTORY.txt
#
# which is the entire developer working folder minus exactly one file. So every release
# also carried the gate suite, 433 proof screenshots (255 MB in shots/), three audio
# captures, a save/load test directory, ~40 loose proof PNGs and the hand-tuned
# presentation preset. The published macOS zip was 748 MB against Windows' 511 MB, and
# the 237 MB difference was almost exactly shots/. Subtraction cannot fix that: the next
# new kind of artefact is not on the exclude list and nobody finds out.
#
# WHAT cp -RL WAS FOR IS NOT LOST. playable/dosdata is a SYMLINK into the repo so that a
# local test folder does not carry a second 527 MB copy of the 1995 discs. Archive that
# link AS a link and it expands on the other machine as a dangling pointer, after which
# the game boots, runs, and is silent, with one line on stderr to say so. The packaging
# script rsyncs -L and then ASSERTS that dosdata arrived as a real directory holding
# SOUNDS.MIX and SPEECH.MIX, and holding no movie and no score, so the dereferencing and
# the exclusion are both checked rather than merely intended.
say "packaging the macOS build: the game and its data, without the movies and music"
BINDIR=$(mktemp -d)
STAGEDIR=$(mktemp -d)
cleanup_pkg() { rm -rf "$STAGEDIR"; }
# Before the tag every refusal also takes the zips folder away and puts the developer
# build back, as every other refusal before the tag does.
refuse_pkg() { cleanup_pkg; rm -rf "$BINDIR"; restore; die "$@"; }

MACFULLNAME="OpenCNC3D-macos-$TAG"
OPENCNC3D_NO_RA=$NO_RA tools/mac/make-package-mac.sh "$STAGEDIR/$MACFULLNAME" || refuse_pkg "the macOS
       package could not be assembled. It is an allow-list and it refuses rather than
       shipping a folder with a piece of the game missing, or one holding a movie or the
       score; the message above names what. Nothing is tagged."
# The packaged pair is a copy of the pair proved above; asked again of the copy, because
# the copy is what ships.
for b in cnc_eyes opencnc3d; do
    has_mark "$STAGEDIR/$MACFULLNAME/$b" || refuse_pkg "the packaged $b is not a cooked
       $BUILDSTR ($MARK). Nothing is tagged."
done
# NO MOVIE AND NO SCORE, asked of the assembled folder with the same list the zips are
# asked with. The packager refuses them itself; this is the release's own question of
# what it is about to hand out, and it is asked before the tag.
_mm=$(cd "$STAGEDIR/$MACFULLNAME" && find . -print | sed 's|^\./||' | media_names)
[ -z "$_mm" ] || refuse_pkg "the assembled macOS package carries movies or music, and no
       package does:
$(echo "$_mm" | sed 's/^/           /')
       Nothing is tagged."

# G136, THE OTHER HALF: THE EDITOR OPENS FROM THE ASSEMBLED PACKAGE. The legs above were
# about playable/, which is the folder the allow-list reads FROM. This is the folder a
# player unzips, and it is the only place the allow-list itself can be caught leaving the
# editor behind. It runs the exact command the EDITOR button issues, from the working
# directory the button gives it, and asks the editor to draw one frame and quit. Nothing
# short of running it proves the package can.
#
# The window is small and the shot is written OUTSIDE the package, so the check cannot
# leave a file in the folder that is about to be zipped.
say "G136 opening the editor from the package"
G136DIR=$(mktemp -d)
printf 'shot %s\nquit\n' "$G136DIR/editor.png" > "$G136DIR/editor.script"
( cd "$STAGEDIR/$MACFULLNAME" && ./cnc_eyes --edit --scen SCG01EA --nosound \
      --w 800 --h 600 --script "$G136DIR/editor.script" ) > "$G136DIR/editor.log" 2>&1 || {
    tail -20 "$G136DIR/editor.log"
    refuse_pkg "the editor did not open from the assembled macOS package. The EDITOR button
       runs exactly that command from exactly that folder, so this package would ship a
       button that fails the moment it is pressed. The tail of the run is above, and it
       names the missing piece. Nothing is tagged."
}
grep -q '^edit: ' "$G136DIR/editor.log" || refuse_pkg "the package's cnc_eyes ran and said
       none of the things the editor says, so it opened the renderer rather than the
       editor. Nothing is tagged."
[ -s "$G136DIR/editor.png" ] || refuse_pkg "the editor opened from the package and drew no
       frame. Nothing is tagged."
echo "   G136 the editor opened SCG01EA from the package and drew a frame"
rm -rf "$G136DIR"

MACZIP=""
if [ "$DRY" != "1" ]; then
    MACZIP="$BINDIR/$MACFULLNAME.zip"
    ( cd "$STAGEDIR" && zip -qrX "$MACZIP" "$MACFULLNAME" -x '*.DS_Store' ) \
        || refuse_pkg "zip failed while packing the macOS package. Nothing is tagged."
    _mm=$(unzip -Z1 "$MACZIP" 2>/dev/null | media_names)
    [ -z "$_mm" ] || refuse_pkg "the macOS zip carries movies or music ($MACZIP):
$(echo "$_mm" | sed 's/^/           /')
       No package does. Nothing is tagged."
fi
cleanup_pkg
say "macOS: the package is assembled$([ -n "$MACZIP" ] && echo ' and zipped'), and carries no movies or music"

# ---------------------------------------------------------------- 6. commit, tag, push
if [ "$DRY" = "1" ]; then
    say "dry run: not committing, tagging, pushing or publishing"
    rm -rf "$BINDIR"
    restore
    exit 0
fi

say "committing and tagging $TAG"
git add VERSION docs/CHANGELOG.md
git commit -q -m "Build $TAG

Both platforms from this commit. The gates were run on the macOS half before the tag
went on; the Windows half is cross compiled and has to be run on Windows."
git tag -a "$TAG" -m "OpenCNC3D $TAG"

# CAPTURE THE RELEASED COMMIT NOW, and never read HEAD again. On v0.5.3 the parallel
# push landed while this script was running and main moved underneath it, so the
# closing line reported a commit that was not the one released. Nothing was actually
# mispublished -- the tag and both pointers were already correct -- but a message that
# names the wrong commit is exactly the kind of thing someone later takes as evidence.
# HEAD is a moving target in a repo two agents share; RELCOMMIT is not.
RELCOMMIT=$(git rev-parse HEAD)

say "pushing main, the tag, and the two release pointers"
git push -q origin main
git push -q origin "$TAG"

# THE macos AND windows POINTER BRANCHES ARE NO LONGER PUSHED, by decision, after they
# stopped v0.6.3 between the tag and the build.
#
# WHAT HAPPENED. They are set, not merged, so each release force-moves them from wherever
# they were to the release commit -- by 0.6.3 that was a jump of 350 commits. The
# tree-hygiene pre-push hook rescans the whole range it is asked to push, and 350 commits
# back there are Win98 commits carrying Co-Authored-By trailers. Those commits are ALREADY
# PUBLISHED on origin/main (checked with merge-base --is-ancestor, all three), so the hook
# was refusing to re-point two branches at content it had already let through. The hook is
# not wrong to scan; the pointers are wrong to be 350 commits behind.
#
# WHY DROPPING THEM IS SAFE. Nothing consumes them. A player gets builds from the GitHub
# release, and opencnc3d.com's /api/builds reads that release's assets. The tag is the
# durable marker of what shipped and it is still pushed, one line above.
#
# THE COST, so nobody is surprised: `git checkout macos` no longer lands on the newest
# release. Use the tag. They are left on the remote at whatever they last pointed to
# rather than deleted, because deleting a branch somebody may have cloned is a bigger
# decision than not moving it.

# ---------------------------------------------------------------- 7. publish the builds
# The macOS full package was assembled, asked and zipped in step 5c, before the tag, and
# the Windows full package by make-build-win.sh in step 4. What is built from here on is
# the two binary-only zips and the manifest.
#
# The small binary-only zips go up ALONGSIDE the full ones, because they are what you want
# when you already have the folder: 13 MB instead of 450, unzipped over the top, data
# untouched.

WINZIP=$(ls -t "$WINDIST"/OpenCNC3D-windows-"$TAG".zip 2>/dev/null | head -1)

# THE WINDOWS INSTALLER WAS BUILT IN STEP 4, before the tag, from the same staged folder
# the zip was made from (the reason is written there), and WINSETUP names it. It is
# published beside the zips rather than instead of them: the zip is still what anyone who
# wants the plain folder takes, and it is what the launcher's own binary-only update
# unpacks over.

say "packaging the binary-only updates"
MACBINS="$BINDIR/OpenCNC3D-macos-$TAG-bins.zip"
# THE SDL LIBRARIES ARE PART OF "THE BINARIES", and leaving them out of this zip
# reintroduces the bug the bundling fixed. From the binaries load SDL from
# @executable_path (tools/bundle-sdl.sh has the account); unzipped over a folder from an
# older release, which has no SDL beside it, they die in dyld before main() exactly as
# before, with "Library not loaded: @executable_path/libSDL2-2.0.0.dylib".
#
# THE GLOB IS EXPANDED HERE, BY THE SHELL, AND WHAT COMES OUT IS CHECKED. The first
# attempt at this handed zip the pattern in single quotes:
#
#     zip -qX "$MACBINS" cnc_eyes opencnc3d TiberianDawn.dylib gates.sh 'libSDL*.dylib'
#
# and the finished zip held exactly four entries and no SDL at all. Three separate
# things have to be true for a quoted glob to work and none of them is: the shell does
# not expand a quoted word, Apple's zip 3.0 does not expand an argument either (it is
# not tar), and it treats an unmatched name as a WARNING, printing "zip warning: name
# not matched: libSDL*.dylib" and exiting 0. -q silenced the warning, 2>/dev/null threw
# away what was left, and the `|| MACBINS=` guard never fired because there was no
# failure to catch. So the zip written to close the dyld bug shipped precisely the
# binaries that still had it, and said nothing.
#
# Still globbed rather than named, because a machine that links a REAL SDL2 instead of
# sdl2-compat has no libSDL3.dylib to carry and must still be able to cut a release. But
# at least one libSDL*.dylib is demanded, because zero of them is the defect above and
# not a configuration.
# gate_optlayout rides with gates.sh, and the pairing is the point. G57 runs that binary,
# so a bins update that carried the script without it would put a gates.sh on the folder
# whose G57 goes RED for a missing binary on a folder that is perfectly fine. A false red
# costs the same as a false green here: the next person stops trusting the suite.
#
# gate_lockstep and gate_netloop ride with it for that same reason and are the same
# pairing: G130 runs both, and reports a missing binary as a FAILURE rather than a skip,
# which is right in the gate and would be a false red here.
#
# netcheck rides with them because it is not a gate and is still one of "the binaries":
# it is what a player runs when a match will not start, and a folder taking the small
# update would otherwise keep whatever copy of it the last full package left behind.
# The menu pack rides with them too: a repainted title screen keeps the pack's size, and
# the data fingerprint counts sizes, so the small update is the only way it arrives.
# THE LAUNCHER IS ONE OF "THE BINARIES". Leave it out and a player who takes the
# small update gets a new game under an old launcher, which is the exact drift the
# launcher exists to stop: it would go on reporting the version it was built with
# while the folder underneath it moved. It is named by its path at the package
# root rather than by the bundle, because OpenCNC3D.app is a tracked script wrapper
# and this file is the only binary in the pair (see the note in game/make-build.sh).
MACBIN_LIST="cnc_eyes opencnc3d opencnc3d-launcher TiberianDawn.dylib dosclassic.pack dosmenu.pack CONQUER.MIX gates.sh gate_optlayout gate_lockstep gate_netloop netcheck"
MACBIN_SDL=$( (cd playable && ls libSDL*.dylib) 2>/dev/null || true )
[ -n "$MACBIN_SDL" ] || die "playable/ holds no libSDL*.dylib, so the binary-only update
       would carry binaries that load SDL from @executable_path with no SDL beside them.
       Unzipped over an older release folder they die in dyld before main() runs, which
       is the very bug the bundling closed. Run game/make-build.sh (it calls
       tools/bundle-sdl.sh) and re-run the publish step."
MACBIN_LIST="$MACBIN_LIST $MACBIN_SDL"
MACBIN_MISSING=""
for f in $MACBIN_LIST; do
    [ -f "playable/$f" ] || MACBIN_MISSING="$MACBIN_MISSING $f"
done
[ -z "$MACBIN_MISSING" ] || die "the binary-only update is missing:$MACBIN_MISSING
       Named files are checked here rather than left to zip, which warns about a name it
       cannot match and still exits 0, so a short zip is otherwise silent."
# NOT -q, and stderr NOT discarded. A warning from zip is the one signal that told us the
# glob had failed, and the point of this block is that it can never be swallowed again.
( cd playable && zip -X "$MACBINS" $MACBIN_LIST ) \
    || die "zip failed while packing the macOS binary-only update."
# And COUNT what landed rather than trusting the exit status, because the failure this
# replaces produced both a zip and an exit status of 0.
MACBIN_WANT=$(printf '%s\n' $MACBIN_LIST | wc -l | tr -d ' ')
MACBIN_GOT=$(unzip -Z1 "$MACBINS" 2>/dev/null | wc -l | tr -d ' ')
[ "$MACBIN_GOT" = "$MACBIN_WANT" ] || die "the macOS binary-only zip has $MACBIN_GOT
       entries and should have $MACBIN_WANT ($MACBIN_LIST). A short bins zip unzipped
       over an older release folder is how a Mac ends up with new binaries and no SDL."
echo "   bins zip: $MACBIN_GOT entries ($MACBIN_LIST)"
# NOT OPTIONAL. The packager refuses a zip of the wrong shape by deleting it and failing,
# and this used to discard that failure and go on to publish one asset short, so the
# checks below could only ever have caught a stale file.
WBLOG=$(mktemp)
# THE SAME SWITCHES AS THE FULL BUILD, because --bins-only compiles the Windows binaries
# again: the release header is kept rather than regenerated, Red Alert stays out, and the
# data comes from the same play folder.
OPENCNC3D_COOKED=1 OPENCNC3D_SKIP_VERSION_HEADER=1 OPENCNC3D_BUILD_ID="$TAG" OPENCNC3D_NO_RA=$NO_RA \
    OPENCNC3D_PLAYABLE="$ROOT/playable" tools/win/make-build-win.sh --bins-only >"$WBLOG" 2>&1 \
    || { tail -15 "$WBLOG" >&2; die "the Windows binary-only packager failed (its last lines
       are above). The tag and branches are pushed and nothing is published; fix it and
       re-run the publish step."; }
rm -f "$WBLOG"
WINBINS=$(ls -t "$WINDIST"/OpenCNC3D-windows-"$TAG"-bins.zip 2>/dev/null | head -1)
[ -n "$WINBINS" ] || die "the Windows binary-only packager succeeded and left no
       $WINDIST/OpenCNC3D-windows-$TAG-bins.zip. The tag and branches are pushed and nothing
       is published; re-run the publish step once it does."
win_zip_tail "$WINBINS" "" BUILD-ID.txt SDL2.dll && ! unzip -Z1 "$WINBINS" | grep -q / \
    || die "the Windows binary-only zip is not flat with BUILD-ID.txt and SDL2.dll last
       ($WINBINS). Launchers up to v0.6.11 unpack it without stripping a folder, so a
       wrapped one lands in a subfolder and records $TAG over the old binaries. The tag and
       branches are pushed and nothing is published; fix tools/win/make-build-win.sh and
       re-run the publish step."
win_sdl_pinned "$(unzip -p "$WINBINS" SDL2.dll | shasum -a 256 | cut -d' ' -f1)" \
    || die "the SDL2.dll in $WINBINS $SDL2_NOT_PINNED The tag and branches are pushed and
       nothing is published."

# WHAT IS ABOUT TO BE PUBLISHED, asked of the zips themselves. Every game binary in every
# zip is this release, cooked: the Windows binary-only zip was compiled again just above,
# after the tag, and nothing else looks at it. And with Red Alert left out, no zip holds a
# path naming it. Each binary is extracted on its own into a folder of its own; unzip
# seeks the central directory, so this costs seconds even on the full packages.
say "checking the zips"
ZCHK=$(mktemp -d)
for zb in "$MACZIP|$MACFULLNAME/cnc_eyes" "$MACZIP|$MACFULLNAME/opencnc3d" \
          "$MACBINS|cnc_eyes" "$MACBINS|opencnc3d" \
          "$WINZIP|OpenCNC3D-windows-$TAG/opencnc3d-game.exe" \
          "$WINZIP|OpenCNC3D-windows-$TAG/cnc_eyes.exe" \
          "$WINBINS|opencnc3d-game.exe" "$WINBINS|cnc_eyes.exe"; do
    z=${zb%%|*}; e=${zb#*|}
    # A zip that was not produced is refused by name further down.
    [ -n "$z" ] && [ -f "$z" ] || continue
    rm -f "$ZCHK/bin"
    unzip -p "$z" "$e" > "$ZCHK/bin" 2>/dev/null && has_mark "$ZCHK/bin" || {
        _m=$(marks_of "$ZCHK/bin"); rm -rf "$ZCHK"; die "$e in $(basename "$z") is not a
       cooked $BUILDSTR: it should carry $MARK and carries ${_m:-no build mark, or is not
       in the zip}. The tag and branches are pushed and nothing is published."; }
done
rm -rf "$ZCHK"
if [ "$NO_RA" = "1" ]; then
    for z in "$MACZIP" "$MACBINS" "$WINZIP" "$WINBINS"; do
        [ -n "$z" ] && [ -f "$z" ] || continue
        _ra=$(unzip -Z1 "$z" 2>/dev/null | grep -i -E '(^|/)(ra3d|redalert)[^/]*(/|$)' | head -3)
        [ -z "$_ra" ] || die "$(basename "$z") carries Red Alert, and this release leaves it out:
$(echo "$_ra" | sed 's/^/           /')
       The tag and branches are pushed and nothing is published."
    done
fi
# AND NO ZIP CARRIES A MOVIE OR THE SCORE, asked of all four by their own listing. The
# packagers refuse to stage one and both full zips were asked before the tag; this is the
# same question of every file about to be published, the two binary-only zips included.
for z in "$MACZIP" "$MACBINS" "$WINZIP" "$WINBINS"; do
    [ -n "$z" ] && [ -f "$z" ] || continue
    _zm=$(unzip -Z1 "$z" 2>/dev/null | media_names)
    [ -z "$_zm" ] || die "$(basename "$z") carries movies or music, and no package does:
$(echo "$_zm" | sed 's/^/           /')
       The tag and branches are pushed and nothing is published."
done
echo "   every game binary in every zip is $BUILDSTR, cooked$([ "$NO_RA" = "1" ] && echo ', and no zip carries Red Alert'); no zip carries a movie or the score"

# THE MANIFEST ASSET. One small text file listing a SHA-256 for every zip and
# the data fingerprint each package carries. opencnc3d.com's /api/builds lists
# every asset of the newest release, so publishing it here is the whole of
# publishing it: the launcher finds it in that list, and with it can check a
# download against a hash instead of a length, and take the 15 MB update instead
# of the 500 MB one when the player's data already matches.
#
# NOT FATAL. A release without it still updates every launcher; it just downloads
# more and checks less, and the launcher says which. Refusing to ship two playable
# builds over a missing text file would be the tail wagging the dog.
MANIFEST=""
if [ -n "$MACZIP" ] && [ -n "$WINZIP" ]; then
    say "manifest"
    MPATH="$BINDIR/OpenCNC3D-$TAG-manifest.txt"
    if tools/launcher/make-manifest.sh "$TAG" "$MPATH" \
           "$MACZIP" "$WINZIP" ${MACBINS:+"$MACBINS"} ${WINBINS:+"$WINBINS"}; then
        MANIFEST="$MPATH"
    else
        echo "WARNING: the manifest could not be written, so launchers will fall back" >&2
        echo "         to the full package and a length check. The release is fine." >&2
    fi
fi

for v in MACZIP WINZIP MACBINS WINBINS WINSETUP MANIFEST; do
    eval "f=\$$v"
    [ -n "$f" ] && [ -f "$f" ] || eval "$v=''"
done
[ -n "$MACZIP" ] || die "the macOS playable package was not produced, and a release has to
       be playable. Nothing has been published; the tag and the branches are already
       pushed, so fix the packaging and re-run just the publish step."
[ -n "$WINZIP" ] || die "the Windows playable package was not produced (expected
       $WINDIST/OpenCNC3D-windows-$TAG.zip). A release is both platforms, playable."
# The installer was built and checked before the tag, and it has to still be there to
# publish, because it is how a Windows player asks for the movies and music.
[ -n "$WINSETUP" ] || die "the Windows installer built before the
       tag is gone (expected $WINDIST/OpenCNC3D-Setup-$TAG.exe). The tag and branches are
       pushed and nothing is published; rebuild it with tools/win/make-installer-win.sh
       and re-run the publish step."

# THE PUBLISH IS NOT OPTIONAL, AND ITS FAILURE IS NOT A FOOTNOTE. (a project rule
# 2026: a new build updates the changelog AND the builds on GitHub, and neither half goes
# without the other.) This block used to be wrapped in `if command -v gh`, and its failure
# branch echoed one line and let the script carry on to print "$TAG is out." So there were
# three separate ways to finish a release round with the changelog naming a version nobody
# can download -- gh missing, the create failing, or a create that silently dropped an
# asset -- and all three reported success. That is the state found,
# reading the changelog, seeing v0.5.8 and finding v0.5.7 as the newest download. All
# three are hard failures now.
command -v gh >/dev/null 2>&1 || die "gh is not installed, so the builds cannot be
       published, and a changelog entry with no build on GitHub is exactly the drift this
       rule exists to stop. The tag and both branches ARE pushed, so nothing is lost:
       install gh, authenticate, and re-run the publish step."
if true; then
    say "publishing the GitHub release"
    NOTES=$(mktemp)
    # No \b here: it is a GNU regex extension that POSIX awk does not have, and the first
    # release cut with this script published EMPTY notes because of it. Plain index()
    # instead, with a guard so v0.5.1 does not also match v0.5.10.
    awk -v v="v$NEXT" '
        substr($0, 1, 3) == "## " {
            if (on) exit
            n = index($0, v)
            if (n > 0) {
                c = substr($0, n + length(v), 1)
                if (c !~ /[0-9.]/) { on = 1; buf[++nb] = $0 }
            }
            next
        }
        on { buf[++nb] = $0 }
        # The trailing blank lines and --- belong to the FILE, where they divide one entry
        # from the next. On a release page they are a stray horizontal rule under the last
        # bullet, so they are trimmed here rather than left for someone to notice. exit
        # still runs END, which is what makes the buffering safe.
        END {
            while (nb > 0 && (buf[nb] == "" || buf[nb] == "---")) nb--
            for (i = 1; i <= nb; i++) print buf[i]
        }
    ' docs/CHANGELOG.md > "$NOTES"
    [ -s "$NOTES" ] || { echo "the changelog section for v$NEXT came out empty"; exit 1; }

    # THE SOURCE OFFER, ON EVERY RELEASE. These archives carry the game logic, which is
    # GPL v3, so whoever receives the binary is entitled to the corresponding source.
    # A link in the notes is the simplest way to satisfy that, and it has to be automatic:
    # a source offer that depends on somebody remembering to paste it is not an offer.
    cat >> "$NOTES" <<'SRC'

---

**Source.** OpenCNC3D is free software under the GNU General Public License v3. The
complete corresponding source for this build, including the modified Tiberian Dawn
game logic, is at https://github.com/freschism3000/OpenCNC3D
SRC
    WANT=0
    for z in "$MACZIP" "$WINZIP" "$MACBINS" "$WINBINS" "$WINSETUP" "$MANIFEST"; do
        [ -n "$z" ] && WANT=$((WANT + 1))
    done
    gh release create "$TAG" --title "OpenCNC3D $TAG" --notes-file "$NOTES" \
        ${MACZIP:+"$MACZIP"} ${WINZIP:+"$WINZIP"} \
        ${MACBINS:+"$MACBINS"} ${WINBINS:+"$WINBINS"} \
        ${WINSETUP:+"$WINSETUP"} ${MANIFEST:+"$MANIFEST"} >/dev/null \
        || die "gh release create failed for $TAG. The tag and both branches are pushed,
       so the code is safe, but the changelog now names a build nobody can download. That
       is the one state this rule forbids: finish the publish before doing anything else."

    # COUNT WHAT LANDED. A create that uploads three of four zips exits 0, exactly the way
    # the short bins zip did, and the release page then looks finished while missing the
    # half someone needs. Read it back from GitHub rather than trusting the exit status.
    GOT=$(gh release view "$TAG" --json assets -q '.assets | length' 2>/dev/null || echo 0)
    [ "$GOT" = "$WANT" ] || die "the $TAG release page carries $GOT assets and should carry
       $WANT. Attach the missing ones with 'gh release upload $TAG <file>'; a release with
       a partial set of builds is a release that lies about what it is."
    echo "published $TAG: $GOT assets, both playable builds plus the binary-only updates$([ -n "$WINSETUP" ] && echo ' and the Windows installer')"
fi

# ---------------------------------------------------------------- 7b. the developer build back
# THE COOKED BINARIES ARE STILL IN playable/, staged there by the packager in step 4, and
# this machine plays and tunes from playable/. Now that the packages are proved and
# published, put the developer build (with F5) back. This step used to be promised by a
# comment and done by nobody, and playable/ stayed cooked after every successful release
# until the next developer build happened to run; the dry run and the failure paths
# reach the same function through restore().
dev_build_back

# ---------------------------------------------------------------- 8. the launchers
# THERE IS NO EIGHTH STEP ANY MORE, and that is the point of reading the site.
# opencnc3d.com's Builds section is generated from this repo's GitHub releases,
# and the launcher reads the same three routes the website does. So the release
# that was just published IS the thing every launcher will offer, within one
# check, with nothing else to upload and no second place to forget.
#
# This used to be a private keyed host with its own upload step. It was replaced
# because the site already had a Builds section: two
# publishing paths for one build is two things to keep in step, and the launcher
# and the website disagreeing about what the newest build is would be a bug
# nobody could see from either side.

say "$TAG is out.
   the tag points at $(git rev-parse --short "$RELCOMMIT"), and it is the only thing that
   does: the macos and windows branches are deliberately left where they were, for the
   reason set out where they used to be moved. Check out the tag, not a branch.
   main may already be ahead of the tag, which is the arrangement working rather than a
   problem: the tag names the last GATED build, not the newest commit.
   The menu plate says: OpenCNC3D $BUILDSTR"
