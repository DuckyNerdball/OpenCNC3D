#!/bin/bash
# Run the CI on this Mac automatically: every ten minutes, check whether origin/main has a
# commit that has not been checked yet, and if so run tools/ci-local.sh on it.
#
#   tools/ci-local-schedule.sh               install (or update) the job, from any checkout
#   tools/ci-local-schedule.sh --uninstall   remove it
#   tools/ci-local-schedule.sh --status      show the job, the last result and its log
#
# Each tick fetches origin and runs tools/ci-local-watch.sh as it stands on origin/main,
# so a change to how the CI runs reaches the job without reinstalling. The watch script
# keeps its own checkout in ~/opencnc3d-ci (a full worktree plus its build,
# about 45 GB), runs one check at a time, skips while on battery, and opens or closes the
# "CI is red on main" issue. The job runs at nice 10. Its log is
# ~/Library/Logs/opencnc3d-ci-local.log.
set -eu

LABEL=com.opencnc3d.ci-local
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
STATE="$HOME/.opencnc3d-ci"
RUNNER="$STATE/job.sh"
LOG="$HOME/Library/Logs/opencnc3d-ci-local.log"

case "${1:-}" in
    --uninstall)
        launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
        rm -f "$PLIST" "$RUNNER"
        echo "ci-local: automatic job removed (the checkout in $HOME/opencnc3d-ci is left; remove it with git worktree remove)"
        exit 0 ;;
    --status)
        launchctl print "gui/$(id -u)/$LABEL" 2>/dev/null | grep -E 'state =|last exit|run interval' || echo "ci-local: not installed"
        [ -f "$STATE/last-checked" ] && echo "last commit checked: $(cat "$STATE/last-checked")"
        [ -f "$STATE/last-summary.txt" ] && grep -E '^(FAIL|NOT RUN|NOTE|commit|checked)' "$STATE/last-summary.txt"
        [ -f "$LOG" ] && tail -10 "$LOG"
        exit 0 ;;
esac

# The primary checkout of the repository this is run from, even from a worktree.
REPO=$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")

mkdir -p "$STATE" "$(dirname "$PLIST")" "$(dirname "$LOG")"
{
    echo '#!/bin/bash'
    echo '# Written by tools/ci-local-schedule.sh; run by launchd every ten minutes.'
    echo 'export PATH=/usr/local/bin:/opt/homebrew/bin:$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin'
    printf 'REPO=%q\n' "$REPO"
    cat <<'EOF'
git -C "$REPO" fetch -q origin main 2>/dev/null || exit 0
script=$(mktemp "${TMPDIR:-/tmp}/ci-local-watch.XXXXXX")
if git -C "$REPO" show origin/main:tools/ci-local-watch.sh > "$script"; then
    bash "$script" "$REPO"
fi
rm -f "$script"
EOF
} > "$RUNNER"
chmod +x "$RUNNER"

cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>$LABEL</string>
    <key>ProgramArguments</key><array><string>/bin/bash</string><string>$RUNNER</string></array>
    <key>StartInterval</key><integer>600</integer>
    <key>RunAtLoad</key><true/>
    <key>ProcessType</key><string>Standard</string>
    <key>Nice</key><integer>10</integer>
    <key>StandardOutPath</key><string>$LOG</string>
    <key>StandardErrorPath</key><string>$LOG</string>
</dict>
</plist>
EOF

launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
launchctl bootstrap "gui/$(id -u)" "$PLIST"
echo "ci-local: automatic job installed for $REPO (every ten minutes)"
echo "          log: $LOG"
