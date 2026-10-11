#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <RedAlert.dylib> <report-path>" >&2
    exit 2
fi

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BRAIN=$1
REPORT=$2
WORK=$(mktemp -d "${TMPDIR:-/tmp}/ra3d-bootstrap.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

DL_LIBS=-ldl
if [ "$(uname -s)" = "Darwin" ]; then
    DL_LIBS=
fi

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -O2 \
    "$ROOT/brain/host/ra_bootstrap.c" $DL_LIBS -o "$WORK/ra_bootstrap"

python3 "$ROOT/tools/ra3d/run-bootstrap-gate.py" \
    --host "$WORK/ra_bootstrap" \
    --brain "$BRAIN" \
    --fixture "$ROOT/brain/tests/ra3d" \
    --report "$REPORT"
