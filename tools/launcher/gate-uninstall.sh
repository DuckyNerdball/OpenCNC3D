#!/bin/sh
# The uninstaller's launcher half (--uninstall) on disposable folders, no network and
# no window: tools/launcher/gate-uninstall.c says what each case proves.
#   gate-uninstall.sh [<launcher binary>]
# With a launcher binary, one case runs that program the way the Windows uninstaller
# does. Also runs tools/launcher/gate-module-removal.c, the single game Remove's own
# checks, which share the removal transaction.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
launcher_bin=${1:-}
cd "$repo"
. tools/mac/deployment-target.sh
. launcher/sources.sh
work=$(mktemp -d "${TMPDIR:-/tmp}/opencnc3d-uninstall-gate.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
sources=$(printf '%s' "$LAUNCHER_SOURCES" | sed 's/^launcher.c //')
cd launcher
# shellcheck disable=SC2086
cc -O1 -Wall -Wextra -std=gnu89 -I. -I../game -I../menu -I../video -DGL_SILENCE_DEPRECATION -o "$work/gate" ../tools/launcher/gate-uninstall.c $sources $(sdl2-config --cflags --libs) -lcurl -lz -framework OpenGL
# shellcheck disable=SC2086
cc -O1 -Wall -Wextra -std=gnu89 -I. -I../game -I../menu -I../video -DGL_SILENCE_DEPRECATION -o "$work/removal" ../tools/launcher/gate-module-removal.c $sources $(sdl2-config --cflags --libs) -lcurl -lz -framework OpenGL
cd ..
sh tools/bundle-sdl.sh "$work" gate removal >/dev/null
if [ -n "$launcher_bin" ]; then
    "$work/gate" "$work/fixtures" "$launcher_bin"
else
    "$work/gate" "$work/fixtures"
fi
"$work/removal" "$work/removal-fixture"
