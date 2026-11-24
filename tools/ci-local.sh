#!/usr/bin/env bash
# The project's continuous integration, run on a Mac instead of a hosted service.
#
# Everything that has to hold before main moves, in four parts:
#   guards     tree hygiene over the commits being checked, the brain checkout against
#              upstream plus our patch, and the exports the renderer needs from both brains
#   macos      both brains, the Red Alert bootstrap, mission and lockstep gates, the
#              renderer, the program and the launcher (universal), the launcher checks,
#              and the sanity checks on build marks, the macOS floor and both CPUs
#   windows    the mingw-w64 cross build with its source, data-fingerprint and package
#              gates, the pointer-truncation guard and the binary and dependency checks
#   windows runtime
#              the Red Alert bootstrap and mission gates run natively on Windows over
#              ssh, then the macOS and Windows reports compared for equivalence
#
#   tools/ci-local.sh                      check the commit checked out here
#   tools/ci-local.sh --range A..B         hygiene-check A..B instead of origin/main..HEAD
#   tools/ci-local.sh --no-windows-runtime skip the ssh part
#
# IT CHECKS WHAT IS COMMITTED. Uncommitted changes to tracked files are refused: a green
# run over edits nobody can fetch says nothing about the commit being pushed. Untracked
# and ignored files are left alone, and build output is ignored.
#
# IT BUILDS IN PLACE, in the checkout it is started from, so a second run rebuilds only
# what changed and no extra copy of the tree is made. Run it in a checkout of your own.
#
# THE WINDOWS RUNTIME PART needs a Windows machine with Python 3 (the py launcher) and
# OpenSSH. It tries each ssh destination in $OPENCNC3D_CI_WINDOWS_HOSTS (space separated)
# or, when that is unset, each line of ~/.opencnc3d-ci/windows-hosts, and uses the first
# that answers. When none does, those gates are reported NOT RUN, which is not a pass.
#
# Results go to build/ci-local/: summary.txt, one log per step under logs/, and an empty
# marker <commit>.green or <commit>.red. Exit status 0 means every part that ran passed.

set -uo pipefail

RANGE=""
WINDOWS_RUNTIME=1
while [ $# -gt 0 ]; do
    case "$1" in
        --range) RANGE="$2"; shift 2 ;;
        --no-windows-runtime) WINDOWS_RUNTIME=0; shift ;;
        -h|--help) sed -n '2,36p' "$0"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

ROOT=$(git rev-parse --show-toplevel) || exit 2
cd "$ROOT" || exit 2
SHA=$(git rev-parse HEAD)
SHORT=$(git rev-parse --short HEAD)
OUT="$ROOT/build/ci-local"
rm -rf "$OUT/logs"
mkdir -p "$OUT/logs" "$OUT/bin"

if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "ci-local: this checkout has uncommitted changes to tracked files:" >&2
    git status --short --untracked-files=no | head -20 >&2
    echo "ci-local: commit them (or check a clean checkout) first." >&2
    exit 2
fi

# A full build writes several GB, and a disk that fills stops every program on the
# machine, not only this one. Refuse to start without room.
FREE_GB=$(df -k "$ROOT" | awk 'NR==2 {print int($4 / 1048576)}')
if [ "$FREE_GB" -lt 15 ]; then
    echo "ci-local: only $FREE_GB GB free on this disk; a build needs at least 15 GB. Free space first." >&2
    exit 2
fi

# The bakes game/build.sh runs import Pillow, NumPy and SciPy. Use the first python3 that
# has all three, wherever it is installed, by putting it first on PATH as "python3".
PY=""
for p in "$(command -v python3 || true)" /usr/local/bin/python3 \
         /Library/Frameworks/Python.framework/Versions/Current/bin/python3 /opt/homebrew/bin/python3; do
    [ -n "$p" ] && [ -x "$p" ] || continue
    if "$p" -c 'from PIL import Image; import numpy, scipy, scipy.ndimage' >/dev/null 2>&1; then PY=$p; break; fi
done
if [ -z "$PY" ]; then
    echo "ci-local: no python3 with pillow, numpy and scipy (python3 -m pip install pillow numpy scipy)" >&2
    exit 2
fi
ln -sf "$PY" "$OUT/bin/python3"
export PATH="$OUT/bin:$PATH"
# A python.org install has no CA certificates of its own until its "Install Certificates"
# step has been run, and the Windows build downloads with urllib. Lend it the system's.
if [ -z "${SSL_CERT_FILE:-}" ] && [ -f /etc/ssl/cert.pem ]; then export SSL_CERT_FILE=/etc/ssl/cert.pem; fi

FAILED=0
NOTRUN=0
SECTION=""
SECTION_FAILED=0
STEPNO=0
: > "$OUT/summary.txt"

result() { printf '%s\n' "$1" | tee -a "$OUT/summary.txt" >/dev/null; }
section() { SECTION=$1; SECTION_FAILED=0; printf '\n### %s\n' "$SECTION"; }

# step NAME COMMAND...: run one check, its output in its own log. A failure stops the
# rest of its part, as a failing step stopped its job, and the other parts still run.
step() {
    local name=$1; shift
    STEPNO=$((STEPNO + 1))
    if [ "$SECTION_FAILED" = 1 ]; then
        result "SKIP     $SECTION: $name (an earlier step in this part failed)"
        return
    fi
    local slug log t0=$SECONDS
    slug=$(printf '%s' "$name" | tr -c 'A-Za-z0-9' '-' | tr -s '-' | cut -c1-60)
    log="$OUT/logs/$(printf '%02d' "$STEPNO")-$slug.log"
    printf '  %-74s ' "$name"
    # pipefail stays off inside a step: in "producer | grep -q" grep exits at its first
    # match, the producer then dies of SIGPIPE, and pipefail would call a found symbol missing.
    if ( set +o pipefail; "$@" ) >"$log" 2>&1; then
        printf 'ok (%ss)\n' $((SECONDS - t0))
        result "PASS     $SECTION: $name ($((SECONDS - t0))s)"
    else
        printf 'FAILED (%ss)\n' $((SECONDS - t0))
        tail -30 "$log" | sed 's/^/      | /'
        result "FAIL     $SECTION: $name, log ${log#"$ROOT"/}"
        SECTION_FAILED=1
        FAILED=1
    fi
}

notrun() { result "NOT RUN  $SECTION: $1"; NOTRUN=1; printf '  %-74s NOT RUN\n' "$1"; }

# --------------------------------------------------------------------------------------
# guards
# --------------------------------------------------------------------------------------

hygiene() {
    local range=$RANGE
    if [ -z "$range" ]; then
        git fetch --quiet origin main 2>/dev/null || true
        if git merge-base --is-ancestor HEAD origin/main 2>/dev/null; then
            range="$(git rev-parse HEAD~1 2>/dev/null || git rev-parse HEAD)..HEAD"
        else
            range="origin/main..HEAD"
        fi
    fi
    echo "checking $range"
    tools/check-tree-hygiene.sh --range "$range"
}

brain_exports() {
    local field
    for field in dimw makecnt dostage sel nav tar act; do
        grep -aq "|$field=%d" brain/vanilla/tiberiandawn/dllinterface.cpp \
            || { echo "brain checkout is stale: no $field= in the OBJ dump"; return 1; }
    done
    for field in dimw status flash pips maxpips cloak cstage repairing wrench poweroff harv \
                 rotating navset driving primary flashme act; do
        grep -aq "|$field=%d" brain/vanilla/redalert/dllinterface.cpp \
            || { echo "Red Alert brain checkout is stale: no $field= in the OBJ dump"; return 1; }
    done
    echo "brain exports present"
}

section guards
step "Tree hygiene over the commits being checked" hygiene
step "The committed brain checkout is upstream plus our patch, exactly" tools/check-brain-checkout.sh
step "The brains carry the exports the renderer reads" brain_exports

# --------------------------------------------------------------------------------------
# macos
# --------------------------------------------------------------------------------------

# The universal SDL lives outside the repo. It is rebuilt when the script that pins its
# versions, or the deployment target, changes; an install made before this script kept
# a key is taken as current, and the architecture and floor checks below catch it if not.
mac_sdk() {
    local dir="$HOME/.opencnc3d-macsdk" key
    key=$(cat tools/mac/setup-sdl-mac.sh tools/mac/deployment-target.sh | shasum | cut -c1-40)
    if [ -d "$dir" ] && [ ! -f "$dir/.ci-local-key" ]; then
        echo "$key" > "$dir/.ci-local-key"; echo "existing SDL install adopted"; return 0
    fi
    if [ -d "$dir" ] && [ "$(cat "$dir/.ci-local-key")" = "$key" ]; then
        echo "SDL install is current"; return 0
    fi
    command -v ninja >/dev/null || brew install ninja
    tools/mac/setup-sdl-mac.sh && echo "$key" > "$dir/.ci-local-key"
}

ra_gates_mac() {
    mkdir -p build/ra3d
    tools/ra3d/gate-ra-bootstrap.sh brain/vanilla/build-native/redalert/RedAlert.dylib build/ra3d/ra-bootstrap-macos.txt \
        && tools/ra3d/gate-ra-mission.sh brain/vanilla/build-native/redalert/RedAlert.dylib build/ra3d/ra-mission-macos.txt \
        && tools/ra3d/gate-ra-lockstep.sh brain/vanilla/build-native/redalert/RedAlert.dylib build/ra3d/ra-lockstep-macos.txt
}

usage_check() {
    ./game/cnc_eyes --help | head -5
    ./app/opencnc3d --help | head -5 || true
    ./launcher/opencnc3d-launcher --help | head -5
}

build_marks() {
    local f marks
    for f in game/cnc_eyes app/opencnc3d; do
        marks=$(strings -a "$f" | grep '^OPENCNC3D|build|v' | sort -u)
        echo "$f: $marks"
        [ "$(printf '%s\n' "$marks" | grep -c .)" = "1" ] || { echo "$f should carry exactly one build mark"; return 1; }
        case "$marks" in *"|cooked") echo "$f is cooked in a developer build"; return 1 ;; esac
    done
}

macos_floor() {
    . tools/mac/deployment-target.sh
    local rc=0 f min
    for f in game/cnc_eyes app/opencnc3d game/TiberianDawn.dylib brain/vanilla/build-native/redalert/RedAlert.dylib; do
        min=$(otool -l "$f" | awk '/LC_BUILD_VERSION/{f=1} f&&/^ *minos/{print $2; exit}')
        echo "$f minos ${min:-none}"
        [ "$min" = "$OPENCNC3D_MACOS_MIN" ] || rc=1
    done
    [ "$rc" = 0 ] || { echo "expected minos $OPENCNC3D_MACOS_MIN on all four"; return 1; }
}

both_cpus() {
    . tools/mac/deployment-target.sh
    local rc=0 f
    for f in game/cnc_eyes app/opencnc3d launcher/opencnc3d-launcher launcher/opencnc3d-launch \
             game/TiberianDawn.dylib game/libSDL2-2.0.0.dylib game/libSDL3.dylib; do
        echo "$f: $(lipo -archs "$f")"
        lipo "$f" -verify_arch $OPENCNC3D_MAC_ARCHS || rc=1
    done
    [ "$rc" = 0 ] || { echo "expected $OPENCNC3D_MAC_ARCHS in every file"; return 1; }
}

section macos
# A one-CPU development build is not what ships, so this part always builds universal.
unset OPENCNC3D_MAC_ARCHS
step "Universal SDL (SDL3 + sdl2-compat) is installed and current" mac_sdk
step "Build the brains (TiberianDawn and RedAlert libraries)" tools/mac/build-brain-mac.sh
step "Red Alert bootstrap, mission and lockstep gates on macOS" ra_gates_mac
step "Build the renderer (game/build.sh)" sh -c 'cd game && ./build.sh'
step "Build the program (app/build.sh)" sh -c 'cd app && ./build.sh'
step "Build the launcher (launcher/build.sh)" sh -c 'cd launcher && ./build.sh'
step "Launcher: every argument after -- reaches the game" tools/launcher/forward-check.sh launcher/opencnc3d-launcher
step "Launcher: Red Alert's test asks for nothing a package leaves out" tools/launcher/module-check.sh launcher/opencnc3d-launcher
step "Launcher: Red Alert's movies and score download only with its module" tools/launcher/media-module-check.sh launcher/opencnc3d-launcher
step "Sanity: the programs start and report usage" usage_check
step "Sanity: both programs carry one build mark, uncooked" build_marks
step "Sanity: programs and brains declare the macOS floor" macos_floor
step "Sanity: every Mac binary carries both CPUs" both_cpus
MAC_OK=$((1 - SECTION_FAILED))

# --------------------------------------------------------------------------------------
# windows
# --------------------------------------------------------------------------------------

pointer_guard() {
    local log
    for log in build/win/compile.log build/win/brain-build.log; do
        [ -s "$log" ] || { echo "$log is missing or empty, so the truncation search has nothing to read"; return 1; }
    done
    if grep -nE 'loses precision|cast from pointer to integer of different size|cast to pointer from integer of different size' build/win/*.log; then
        echo "a pointer is cut to a narrower integer (above). Cast through intptr_t or uintptr_t."
        return 1
    fi
    echo "no pointer truncation in $(ls build/win/*.log | wc -l | tr -d ' ') compiler logs"
}

windows_binaries() {
    local sym f binary extra
    file build/win/opencnc3d-game.exe | grep -q "PE32+ executable" || { echo "not a PE32+"; return 1; }
    file build/win/opencnc3d-game.exe | grep -q "x86-64" || { echo "not 64-bit"; return 1; }
    for sym in CNC_Advance_Instance OpenCNC3D_Dump_Objects OpenCNC3D_Sel_Info; do
        x86_64-w64-mingw32-objdump -p build/win/TiberianDawn.dll | grep -q "$sym" || { echo "brain is missing $sym"; return 1; }
    done
    for f in build/win/RedAlert.dll build/win/ra_bootstrap.exe; do
        file "$f" | grep -q "PE32+" || { echo "$f is not a PE32+ binary"; return 1; }
        file "$f" | grep -q "x86-64" || { echo "$f is not 64-bit"; return 1; }
    done
    for sym in CNC_Version CNC_Init CNC_Config CNC_Add_Mod_Path CNC_Start_Custom_Instance \
               CNC_Advance_Instance CNC_Get_Game_State CNC_Start_Mission_Timer; do
        x86_64-w64-mingw32-objdump -p build/win/RedAlert.dll | grep -q "$sym" || { echo "Red Alert brain is missing $sym"; return 1; }
    done
    # Only DLLs every Windows ships may be imported: the C runtime, the core system
    # libraries, OpenGL, winsock, WinINet and BCrypt. Anything else would have to ship
    # beside the program, and SDL2 is linked in rather than shipped.
    for binary in build/win/opencnc3d-game.exe build/win/cnc_eyes.exe build/win/netcheck.exe \
                  build/win/OpenCNC3D-Diagnostics.exe build/win/ra_bootstrap.exe \
                  build/win/TiberianDawn.dll build/win/RedAlert.dll; do
        extra=$(x86_64-w64-mingw32-objdump -p "$binary" | grep "DLL Name" \
                | grep -viE "api-ms-win-crt|msvcrt|KERNEL32|USER32|SHELL32|OPENGL32|WINMM|OLEAUT32|ADVAPI32|GDI32|WS2_32|WININET|IMM32|SETUPAPI|VERSION|OLE32|BCRYPT" || true)
        [ -z "$extra" ] || { echo "$binary needs a DLL that Windows 11 does not ship:"; echo "$extra"; return 1; }
    done
    python3 tools/win/check-diagnostics.py build/win
    python3 tools/win/check-pe-arch.py build/win
    echo "PE32+ x86-64, brain exports present, SDL2 linked in, no DLL beyond what Windows ships"
}

section windows
step "Cross toolchain SDKs (SDL2 and zlib for mingw)" tools/win/setup-toolchain.sh
step "The Windows build compiles the same sources as the Mac build" tools/win/check-sources.sh
step "Gate: the data fingerprint sees a same-size change" sh tools/launcher/gate-data-id.sh
step "Gate: full and binaries-only Windows diagnostic packages" \
    python3 tools/win/test-diagnostic-packages.py --repo . --output build/windows-package-tests
step "Build for Windows (tools/win/build-win.sh)" tools/win/build-win.sh
step "No pointer is cut to 32 bits anywhere in the Windows build" pointer_guard
step "Sanity: the right kind of binary, with the right dependencies" windows_binaries
WIN_OK=$((1 - SECTION_FAILED))

# --------------------------------------------------------------------------------------
# windows runtime
# --------------------------------------------------------------------------------------

windows_host() {
    local hosts="${OPENCNC3D_CI_WINDOWS_HOSTS:-}" h
    if [ -z "$hosts" ] && [ -f "$HOME/.opencnc3d-ci/windows-hosts" ]; then
        hosts=$(grep -v '^[[:space:]]*#' "$HOME/.opencnc3d-ci/windows-hosts" | tr '\n' ' ')
    fi
    for h in $hosts; do
        if ssh -o BatchMode=yes -o ConnectTimeout=8 "$h" exit </dev/null >/dev/null 2>&1; then
            echo "$h"; return 0
        fi
    done
    return 1
}

# Runs a PowerShell script on the Windows host, whatever its ssh shell is (cmd or PowerShell).
win_ps() {
    local host=$1 script=$2 enc
    enc=$(printf '%s' "$script" | iconv -f UTF-8 -t UTF-16LE | base64 | tr -d '\n')
    ssh -o BatchMode=yes "$host" "powershell -NoProfile -NonInteractive -EncodedCommand $enc" </dev/null
}

# The gates read the gate scripts, the MIX reader under menu/tools, their fixtures under
# brain/tests, two archives off the Red Alert disc, and the decryption key in the engine's
# own source, so the committed brain/ goes along whole (about 60 MB) with the two binaries.
windows_runtime() {
    local host=$1 stage name rc
    stage=$(mktemp -d)
    name="opencnc3d-ci-$SHORT"
    mkdir -p "$stage/ci/build/win"
    git archive HEAD tools/ra3d menu/tools brain data/radata/GENERAL.MIX data/radata/LOCAL.MIX | tar -x -C "$stage/ci"
    cp build/win/RedAlert.dll build/win/ra_bootstrap.exe "$stage/ci/build/win/"
    tar -czf "$stage/$name.tgz" -C "$stage" ci
    scp -q -o BatchMode=yes "$stage/$name.tgz" "$host:$name.tgz" || { rm -rf "$stage"; return 1; }
    rm -rf "$stage"
    win_ps "$host" '
$ErrorActionPreference = "Stop"
$name = "'"$name"'"
$work = Join-Path $env:TEMP $name
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory $work | Out-Null
tar -xzf (Join-Path $HOME "$name.tgz") -C $work
Remove-Item (Join-Path $HOME "$name.tgz")
Set-Location (Join-Path $work "ci")
New-Item -ItemType Directory -Force build/ra3d | Out-Null
$rc = 0
py -3 tools/ra3d/run-bootstrap-gate.py --host build/win/ra_bootstrap.exe --brain build/win/RedAlert.dll --fixture brain/tests/ra3d --report build/ra3d/ra-bootstrap-windows.txt
if ($LASTEXITCODE -ne 0) { $rc = 1 }
if ($rc -eq 0) {
  py -3 tools/ra3d/run-mission-gate.py --host build/win/ra_bootstrap.exe --brain build/win/RedAlert.dll --data data/radata --expected brain/tests/ra3d/expected-mission-report.txt --report build/ra3d/ra-mission-windows.txt
  if ($LASTEXITCODE -ne 0) { $rc = 1 }
}
Get-ChildItem build/ra3d/*.txt | ForEach-Object { Copy-Item $_.FullName (Join-Path $HOME ("$name-" + $_.Name)) }
Set-Location $HOME
Remove-Item -Recurse -Force $work
exit $rc
'
    rc=$?
    local f
    for f in ra-bootstrap-windows.txt ra-mission-windows.txt; do
        scp -q -o BatchMode=yes "$host:$name-$f" "build/ra3d/$f" 2>/dev/null
    done
    win_ps "$host" "Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path \$HOME '$name-*.txt')" >/dev/null 2>&1
    return $rc
}

equivalence() {
    python3 tools/ra3d/compare-bootstrap-reports.py build/ra3d/ra-bootstrap-macos.txt build/ra3d/ra-bootstrap-windows.txt \
        && python3 tools/ra3d/compare-bootstrap-reports.py build/ra3d/ra-mission-macos.txt build/ra3d/ra-mission-windows.txt
}

section "windows runtime"
rm -f build/ra3d/ra-bootstrap-windows.txt build/ra3d/ra-mission-windows.txt
if [ "$WINDOWS_RUNTIME" = 0 ]; then
    notrun "Red Alert gates on Windows and macOS/Windows equivalence (--no-windows-runtime)"
elif [ "$WIN_OK" = 0 ] || [ "$MAC_OK" = 0 ]; then
    result "SKIP     $SECTION: Red Alert gates on Windows and equivalence (a build above failed)"
elif ! HOST=$(windows_host); then
    notrun "Red Alert gates on Windows and macOS/Windows equivalence (no Windows host answered)"
else
    echo "  (Windows host: $HOST)"
    step "Red Alert bootstrap and mission gates on Windows" windows_runtime "$HOST"
    step "macOS and Windows Red Alert reports are equivalent" equivalence
fi

# --------------------------------------------------------------------------------------

# The checkout was clean when the run started, so a tracked file changed now was changed
# by the build itself (a bake that writes a receipt of its outputs, say). Put it back so the
# next run, and the next commit, start clean, and say which files it was.
REWRITTEN=$(git diff --name-only)
if [ -n "$REWRITTEN" ]; then
    git diff --name-only -z | xargs -0 git checkout --
    result "NOTE     the build rewrote tracked files, restored: $(printf '%s' "$REWRITTEN" | tr '\n' ' ')"
fi

rm -f "$OUT"/*.green "$OUT"/*.red
{
    echo "commit $SHA"
    echo "checked $(date '+%Y-%m-%d %H:%M')"
} >> "$OUT/summary.txt"
echo
if [ "$FAILED" = 1 ]; then
    : > "$OUT/$SHA.red"
    echo "ci-local: RED at $SHORT. Do not push this to main. Summary: build/ci-local/summary.txt"
    exit 1
fi
: > "$OUT/$SHA.green"
if [ "$NOTRUN" = 1 ]; then
    echo "ci-local: GREEN at $SHORT, except the parts marked NOT RUN in build/ci-local/summary.txt"
else
    echo "ci-local: GREEN at $SHORT"
fi
exit 0
