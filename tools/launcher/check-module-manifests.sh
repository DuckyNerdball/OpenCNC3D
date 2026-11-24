#!/bin/sh
# Read every game module ZIP's file list through the launcher's own parser
# (check-module-manifest.c says why) and refuse if the launcher would refuse any of them.
#
#   tools/launcher/check-module-manifests.sh VERSION MODULE.zip...
#
# The module (td or ra) is read from the ZIP's own list name, opencnc3d-module-<m>.txt.
set -eu
[ $# -ge 2 ] || { echo "usage: $0 VERSION MODULE.zip..." >&2; exit 2; }
version=$1; shift
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/opencnc3d-module-names.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cd "$repo"
. tools/mac/deployment-target.sh
. launcher/sources.sh
OPENCNC3D_BUILD_ID=$(cat VERSION) sh tools/launcher/make-config.sh >/dev/null
# every launcher source but the program itself and the module file this checker includes
sources=$(printf '%s' "$LAUNCHER_SOURCES" | tr ' ' '\n' | awk 'NF && $0 != "launcher.c" && $0 != "lmodule.c"' | tr '\n' ' ')
( cd launcher && cc -O1 -std=gnu89 -w -I. -I../game -I../menu -I../video -DGL_SILENCE_DEPRECATION \
    -o "$work/check" ../tools/launcher/check-module-manifest.c $sources \
    $(sdl2-config --cflags --libs) -lcurl -lz -framework OpenGL )
sh tools/bundle-sdl.sh "$work" check >/dev/null
rc=0
n=0
for zip in "$@"; do
    list=$(unzip -Z1 "$zip" | grep -E '(^|/)opencnc3d-module-(td|ra)\.txt$' | head -1)
    if [ -z "$list" ]; then
        echo "MANIFEST|$zip|no opencnc3d-module-td.txt or -ra.txt inside" >&2
        rc=1
        continue
    fi
    module=$(printf '%s' "$list" | sed -E 's#.*opencnc3d-module-(td|ra)\.txt$#\1#')
    n=$((n + 1))
    unzip -p "$zip" "$list" > "$work/list-$n.txt"
    if out=$("$work/check" "$module" "$version" "$work/list-$n.txt"); then st=0; else st=1; rc=1; fi
    printf '%s\n' "$out" | sed "s#$work/list-$n.txt#$(basename "$zip")#"
    [ $st -eq 0 ] || echo "MANIFEST|$(basename "$zip")|the launcher would refuse this module" >&2
done
exit $rc
