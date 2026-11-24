#!/bin/sh
# The data fingerprint moves when the data does, and only then.
#
#   tools/launcher/gate-data-id.sh
#
# tools/launcher/data-id.sh decides whether an installed game may take the small
# binary-only update. A change it misses is not a larger download, it is a player
# left on an old file with nothing to say so, which happened for real while the
# fingerprint was path and size: a mission pack is the same length whatever is in
# it. So the first thing asked here is a change of the same size. Fixtures only;
# nothing is built and no game runs.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
id="$repo/tools/launcher/data-id.sh"
work=$(mktemp -d "${TMPDIR:-/tmp}/opencnc3d-data-id.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
fail() { echo "gate-data-id: $*" >&2; exit 1; }

pkg="$work/OpenCNC3D-windows-v0.0.0"
mkdir -p "$pkg/missions" "$pkg/ra3d" "$pkg/dosdata/movies" "$pkg/Logs"
printf 'mission pack, first bake\n' > "$pkg/SCG01EA.pack"
printf 'sidebar pack, first bake\n' > "$pkg/ra3d/sidebar-temperat.pack"
printf '[Basic]\n'                  > "$pkg/missions/SCG01EA.INI"
printf 'game binary\n'              > "$pkg/opencnc3d-game.exe"
printf 'net probe\n'                > "$pkg/netcheck.exe"
printf 'v0.0.0  windows\n'          > "$pkg/BUILD-ID.txt"
printf 'build v0.0.0\n'             > "$pkg/READ-ME-WINDOWS.txt"
mkdir -p "$pkg/OpenCNC3D.app/Contents/_CodeSignature"
printf 'seal 0\n'                   > "$pkg/OpenCNC3D.app/Contents/_CodeSignature/CodeResources"
printf 'movie\n'                    > "$pkg/dosdata/movies/GDI1.VQA"
printf 'version 0.0.0\n'            > "$pkg/opencnc3d-install.txt"
base=$("$id" "$pkg")
printf '%s' "$base" | grep -Eq '^[0-9a-f]{64}$' || fail "no fingerprint: '$base'"

# The same length, other bytes: the case the size-only fingerprint could not see.
printf 'mission pack, NEW!! bake\n' > "$pkg/SCG01EA.pack"
same=$("$id" "$pkg")
[ "$same" != "$base" ] || fail "a mission pack changed at the same size and the fingerprint did not move"
printf 'sidebar pack, NEW!! bake\n' > "$pkg/ra3d/sidebar-temperat.pack"
side=$("$id" "$pkg")
[ "$side" != "$same" ] || fail "a sidebar pack changed at the same size and the fingerprint did not move"

# What the small update carries, what the launcher writes, the media no package
# carries, and the files every release restamps at the same length: none of them is
# data, so none of them may move it. If one did, every update would be the full one.
printf 'game binary, rebuilt\n' > "$pkg/opencnc3d-game.exe"
printf 'net probe, rebuilt\n'   > "$pkg/netcheck.exe"
printf 'v0.0.1  windows\n'      > "$pkg/BUILD-ID.txt"
printf 'build v0.0.1\n'         > "$pkg/READ-ME-WINDOWS.txt"
printf 'seal 1\n'               > "$pkg/OpenCNC3D.app/Contents/_CodeSignature/CodeResources"
printf 'version 0.0.1\n'        > "$pkg/opencnc3d-install.txt"
printf 'another movie\n'        > "$pkg/dosdata/movies/GDI1.VQA"
printf 'a session log\n'        > "$pkg/Logs/run.txt"
[ "$("$id" "$pkg")" = "$side" ] || fail "a binary, a release stamp, the install record, a movie or a log moved the fingerprint"

# A renamed file is a different data set even at the same content.
mv "$pkg/missions/SCG01EA.INI" "$pkg/missions/SCG01EB.INI"
[ "$("$id" "$pkg")" != "$side" ] || fail "a renamed file did not move the fingerprint"
mv "$pkg/missions/SCG01EB.INI" "$pkg/missions/SCG01EA.INI"

# One number from every hasher this script may pick: the Mac packager, a Linux CI
# runner and Git Bash on Windows must all agree on the same folder.
if command -v sha256sum >/dev/null 2>&1 && command -v shasum >/dev/null 2>&1; then
    bin="$work/bin"
    mkdir -p "$bin"
    for t in sh find sort xargs awk cut sed grep shasum dirname perl; do
        p=$(command -v "$t") && ln -s "$p" "$bin/$t"
    done
    alt=$(PATH="$bin" "$id" "$pkg")
    [ "$alt" = "$side" ] || fail "sha256sum and shasum gave different fingerprints for one folder"
fi

# A path no two hashers print alike is refused rather than given a number.
printf 'x\n' > "$pkg/back\\slash.pack"
if "$id" "$pkg" >/dev/null 2>&1; then fail "a path with a backslash was given a fingerprint"; fi
rm "$pkg/back\\slash.pack"

echo "gate-data-id: same-size changes move the fingerprint; binaries, records, media and logs do not"
