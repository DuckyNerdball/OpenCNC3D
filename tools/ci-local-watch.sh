#!/bin/bash
# One tick of the automatic CI: if the newest commit on origin/main has not been checked
# yet, check it with tools/ci-local.sh in a checkout kept for nothing else.
#
#   tools/ci-local-watch.sh [REPO]
#
# tools/ci-local-schedule.sh installs a launchd job that runs this every ten minutes, as it
# stands on origin/main. Commits pushed while a run is going are covered by the next run,
# which checks the newest one and hygiene-checks everything since the last commit checked.
#
# A red run opens the GitHub issue "CI is red on main" (label ci-red), or comments on it
# if it is open, and the next green run closes it: a red run verifies nothing, so it has
# to be seen somewhere that is not a log. Issues cost nothing; hosted runners did.
#
# The tick does nothing while another run is going, while the Mac runs on battery, or when
# the newest commit was already checked. A run that cannot start (a disk under 15 GB, say)
# is retried on the next tick. The checkout is ~/opencnc3d-ci; the last commit checked,
# last-summary.txt and last-run.log are in ~/.opencnc3d-ci/.
set -u

STATE="$HOME/.opencnc3d-ci"
CI="$HOME/opencnc3d-ci"
LOCK="$STATE/run.lock"
mkdir -p "$STATE"

if [ -n "${1:-}" ]; then
    REPO=$(cd "$1" && pwd)
else
    REPO=$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")
fi
stamp() { date '+%Y-%m-%d %H:%M'; }

# One run at a time. A lock left by a run that died is taken over.
if ! mkdir "$LOCK" 2>/dev/null; then
    pid=$(cat "$LOCK/pid" 2>/dev/null || true)
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then exit 0; fi
    rm -rf "$LOCK"
    mkdir "$LOCK" 2>/dev/null || exit 0
fi
echo $$ > "$LOCK/pid"
trap 'rm -rf "$LOCK"' EXIT

# A 40-minute build on battery is not worth the charge; the next tick on power runs it.
if pmset -g batt 2>/dev/null | head -1 | grep -q "Battery Power"; then exit 0; fi

git -C "$REPO" fetch -q origin main 2>/dev/null || { echo "$(stamp) fetch failed, skipped"; exit 0; }
HEAD=$(git -C "$REPO" rev-parse origin/main)
LAST=$(cat "$STATE/last-checked" 2>/dev/null || true)
[ "$HEAD" = "$LAST" ] && exit 0
SHORT=$(git -C "$REPO" rev-parse --short "$HEAD")

echo "==== $(stamp) checking origin/main $SHORT"
if git -C "$CI" rev-parse --git-dir >/dev/null 2>&1; then
    git -C "$CI" checkout -q --detach --force "$HEAD" || { echo "checkout failed, skipped"; exit 0; }
else
    rm -rf "$CI"
    git -C "$REPO" worktree prune
    git -C "$REPO" worktree add -q --detach "$CI" "$HEAD" || { echo "worktree add failed, skipped"; exit 0; }
fi

RANGE=""
if [ -n "$LAST" ] && git -C "$CI" merge-base --is-ancestor "$LAST" "$HEAD" 2>/dev/null; then
    RANGE="$LAST..$HEAD"
fi

cd "$CI" || exit 0
rc=0
if [ -n "$RANGE" ]; then
    tools/ci-local.sh --range "$RANGE" > "$STATE/last-run.log" 2>&1 || rc=$?
else
    tools/ci-local.sh > "$STATE/last-run.log" 2>&1 || rc=$?
fi
tail -1 "$STATE/last-run.log"
if [ "$rc" = 2 ]; then
    echo "$(stamp) the run could not start; retried on the next tick"
    exit 0
fi
echo "$HEAD" > "$STATE/last-checked"
cp build/ci-local/summary.txt "$STATE/last-summary.txt" 2>/dev/null || true

SLUG=$(git -C "$REPO" remote get-url origin | sed -E 's#^.*github\.com[:/]+##; s#\.git$##; s#/+$##')
command -v gh >/dev/null || { echo "gh not found: the ci-red issue was not updated"; exit 0; }
existing=$(gh issue list --repo "$SLUG" --state open --label ci-red --json number -q '.[0].number' 2>/dev/null || true)

if [ "$rc" != 0 ]; then
    body=$(printf 'The build is failing on main.\n\nCommit: %s\nChecked by tools/ci-local.sh on the Mac (%s).\n\n```\n%s\n```\n\nLogs: build/ci-local/logs/ in ~/.opencnc3d-ci/checkout on that Mac. This issue closes itself when main next checks green. A red run verifies nothing, including the guards in it, so fixing it comes before new work.' \
        "$HEAD" "$(stamp)" "$(grep -E '^(FAIL|NOT RUN|NOTE)' build/ci-local/summary.txt)")
    if [ -n "$existing" ]; then
        gh issue comment "$existing" --repo "$SLUG" --body "$body" >/dev/null && echo "commented on issue #$existing"
    else
        gh label create ci-red --repo "$SLUG" --color b60205 --description "The build is failing on main" >/dev/null 2>&1 || true
        gh issue create --repo "$SLUG" --title "CI is red on main" --label ci-red --body "$body" && echo "opened the ci-red issue"
    fi
elif [ -n "$existing" ]; then
    gh issue close "$existing" --repo "$SLUG" --comment "main checks green again as of $HEAD (tools/ci-local.sh, $(stamp))." >/dev/null \
        && echo "closed issue #$existing"
fi
exit 0
