#!/bin/sh
# The DATA FINGERPRINT of a staged package: one line on stdout.
#
#   tools/launcher/data-id.sh <staged-package-folder>
#
# ONE IMPLEMENTATION, CALLED FROM BOTH ENDS. The packagers write this number into
# the folder's opencnc3d-install.txt, and tools/launcher/make-manifest.sh reads it
# back out of the same file rather than recomputing it. That is deliberate: the
# launcher only takes the small 15 MB update when its own number matches the
# manifest's, so two implementations that disagreed by one file would silently
# turn every update back into a 500 MB download and nothing would report it.
#
# WHAT IT COVERS: every file in the package that the binary-only update does NOT
# carry. So it changes when the DATA changes and not when the code does, which is
# exactly the question the launcher is asking.
#
# WHAT IT NEVER COVERS: the movies and the music. No package carries them; the Windows
# installer and the launcher's Setup download them into the install afterwards, for the
# player who wants them. They are left out here by name anyway, so that a media file
# that strays into a staged folder cannot give one platform, or one package, a
# different number from the other. The same goes for the two files a media download
# leaves behind: its record, opencnc3d-media.txt, and its partial files, *.ocpart.
#
# WHAT IT HASHES: every file's path and the SHA-256 of its CONTENT. It used to be
# path and size alone, on the theory that a pack that changed changed size. That
# does not hold. A mission pack is 12,415,300 bytes whatever is baked into it, and
# re-baked texture and sidebar packs come out exactly as long as before. Measured
# across published packages: the mission packs, the map previews and a river
# texture pack have all changed content at the same size, and twice a release went
# out where nothing else moved the number.
#
# A MISSED CHANGE DOES NOT FAIL SAFE. An unchanged number sends the launcher to the
# small update, which does not carry the file, so the player keeps the old one and
# nothing reports it. Only a number that moves is safe: the full package's extractor
# compares every file by CRC, so one full download also replaces whatever an
# earlier small update left stale.
#
# THE COST: 4 to 5 seconds for a 4.4 GB package with sha256sum and 10 to 12 with
# the Perl shasum, warm cache. The size-only probe it replaced started a shell per
# file and took 16.
set -e
DIR="$1"
[ -d "$DIR" ] || { echo "usage: data-id.sh <package-folder>" >&2; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd)

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum | cut -d" " -f1
    else shasum -a 256 | cut -d" " -f1; fi
}

# THE MEDIA PATHS, from the one list the packagers read (tools/media/media-paths.txt):
# one path per line relative to the package root, "/dosdata/movies/" a folder,
# "/dosdata/SCORES.MIX" a file, '#' a comment, and the leading "/" optional, read the same
# way by every reader of the list. Red Alert's movies and score are on it. The copy here
# is the same list, for a checkout that does not have that file.
MEDIA_LIST="$HERE/../media/media-paths.txt"
if [ -f "$MEDIA_LIST" ]; then
    MEDIA=$(sed -e 's/#.*//' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' \
                -e 's|^\./||' -e 's|^/||' "$MEDIA_LIST" | grep -v '^$' || true)
else
    MEDIA="dosdata/movies/
dosdata/music/
dosdata/SCORES.MIX
dosdata/TRANSIT.MIX
dosdata/transit/TRANSIT.MIX
movies/
ra3d/movies/
ra3d/audio/SCORES.MIX
ra3d/data/SCORES.MIX"
fi

# The exclusions ARE the binary-only zip's file list, on both platforms, plus the
# files the launcher itself writes into an install, plus the media above. Keep this
# in step with the OpenCNC3DBinaries zips (tools/win/make-release-packages-win.py,
# tools/mac/installer/package.py). Leaving out a file that zip does NOT carry is how
# a player ends up on stale data, so the list errs the other way: the few data packs
# the Windows zip also carries still count here, and cost a full download if they
# change.
#
# The one exception is READ-ME-WINDOWS.txt. No binary-only zip carries it, and it
# differs from release to release only by the version printed in it, which under a
# content hash would turn every Windows update into the full package. netcheck and
# BUILD-ID.txt are rebuilt or restamped every release too, but the zip carries them.
set -- \
    ! -name 'opencnc3d'      ! -name 'opencnc3d-game.exe' \
    ! -name 'cnc_eyes'   ! -name 'cnc_eyes.exe' \
    ! -name 'OpenCNC3D-Diagnostics.exe' \
    ! -name 'OpenCNC3D.exe'  ! -name 'opencnc3d-launcher' \
    ! -name 'TiberianDawn.dylib' ! -name 'TiberianDawn.dll' \
    ! -name 'SDL2.dll'   ! -name 'libSDL*.dylib' \
    ! -name 'netcheck'   ! -name 'netcheck.exe' \
    ! -name 'BUILD-ID.txt' ! -name 'READ-ME-WINDOWS.txt' \
    ! -name 'gates.sh'   ! -name 'gate_optlayout' \
    ! -name 'opencnc3d-install.txt' ! -name 'CHANGELOG.txt' \
    ! -name 'opencnc3d-log.txt'     ! -name '.DS_Store' \
    ! -name 'opencnc3d-media.txt'   ! -name '*.ocpart' \
    ! -ipath './Logs/*' \
    ! -path './OpenCNC3D.app/*'
# Split on lines, and never glob: a list entry is a find pattern, not a shell word.
set -f
_ifs=$IFS
IFS='
'
for p in $MEDIA; do
    case "$p" in
        */) set -- "$@" ! -path "./${p}*" ;;   # a folder: everything under it
        *)  set -- "$@" ! -path "./${p}" ;;    # one file
    esac
done
IFS=$_ifs
set +f

# One "<sha256> *./path" line per file, in byte order of the path. -b on both tools:
# binary reads everywhere, and the same line from sha256sum on Linux and Git Bash,
# macOS's own sha256sum and the Perl shasum, so every machine writes one number.
# -r: no file, no hasher run (GNU xargs would otherwise hash its own empty stdin).
if command -v sha256sum >/dev/null 2>&1; then HASH="sha256sum -b"
else HASH="shasum -a 256 -b"; fi
RECORDS=$(cd "$DIR" && find . -type f "$@" -print0 \
          | LC_ALL=C sort -z | xargs -0 -r $HASH) \
    || { echo "data-id.sh: could not hash every file in $DIR" >&2; exit 1; }

# Any other line shape means a tool printed something this script has not seen, or
# a path held a backslash or a newline (GNU escapes those, the others do not). Either
# would give one platform a number the other cannot reproduce, so it stops here.
[ -z "$RECORDS" ] || printf '%s\n' "$RECORDS" | awk '
    length($0) < 68 || substr($0, 1, 64) !~ /^[0-9a-f]+$/ || substr($0, 65, 4) != " *./" \
    || index($0, "\\") {
        print "data-id.sh: unexpected hash line: " $0 > "/dev/stderr"; exit 1 }' \
    || exit 1

{ [ -z "$RECORDS" ] || printf '%s\n' "$RECORDS"; } | sha256_of
