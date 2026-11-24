#!/bin/sh
# THE MENUS ARE ENHANCED IN EVERY GRAPHICS MODE, IN BOTH GAMES. Classic and N64 change the
# mission's picture and its HUD and nothing else: the main menu is the same picture under
# Classic, N64 and Enhanced, the menu after a mission is the same picture as before it,
# the pause dialog in a mission is the Enhanced one, and the Skirmish lobby, Load Mission
# and Multiplayer open their screens under Classic with no notice. Red Alert's three once
# answered under Classic with an operating-system box ("This Red Alert menu route is still
# being connected"), because its Classic menus were the 1996 ones and those routes had
# been connected for Enhanced only. Hidden throughout.
#
#   game/gate_menus_every_mode.sh [RUN_FOLDER]
#
# Red Alert's legs need its module in the run folder; without one they say so and
# Tiberian Dawn's still run.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/.." && pwd)
install=${1:-"$root/playable"}
install=$(CDPATH= cd -- "$install" && pwd)
# the 1995 menus are what this gate refuses, so the switch that keeps them must be off
unset OPENCNC3D_MENU_1995
work=$(mktemp -d "${TMPDIR:-/tmp}/menus-every-mode.XXXXXX")
keep=${MENUS_KEEP:-}
trap '[ -n "$keep" ] || rm -rf "$work"' EXIT HUP INT TERM
fail() { echo "FAIL: $1"; [ -n "$keep" ] || tail -20 "$2"; exit 1; }
# Five minutes a run: a screen that waited on a dialog would otherwise hold the suite.
run() { OPENCNC3D_BACKGROUND=1 perl -e 'alarm 300; exec @ARGV' "$@"; }
# The app says which menu it put on screen each time the main menu opens; every one of
# them must be the Enhanced one. The kit loading proves nothing by itself: it loads in
# every mode whichever menu is drawn.
enhanced() {
    grep -Fq 'APP|menu|style=enhanced' "$1" || fail "$2: the main menu was never the Enhanced one" "$1"
    if grep -Fq 'APP|menu|style=1995' "$1"; then fail "$2: the 1995 menu was drawn" "$1"; fi
    if grep -Fq 'APP|menu|ra-route-notice' "$1"; then fail "$2 answered with a notice" "$1"; fi
}
# The three modes: the shipped look with its mode switched. Classic is dos_classic,
# N64 is the 3D view with the post chain off, Enhanced is the post chain on.
for m in classic n64 enhanced; do
    case $m in classic) e=0; d=1 ;; n64) e=0; d=0 ;; enhanced) e=1; d=0 ;; esac
    sed -e "s/^enabled .*/enabled            $e/" -e "s/^dos_classic .*/dos_classic        $d/" \
        "$root/docs/tuning/opencnc3d-fx.cfg" > "$work/$m.cfg"
done
cd "$install"
ra=1
if [ ! -f ra3d/RedAlert.dylib ] || [ ! -f ra3d/enhanced-menu.pack ]; then
    ra=0
    echo "SKIP: no Red Alert module with its Enhanced kit is staged in $install; Red Alert's legs do not run"
fi

# 1. THE MAIN MENU, AND THE PAUSE DIALOG, IN EVERY MODE. One harness round per game and
#    mode: the menu, a mission, the menu again. Red Alert's round is the campaign's first
#    mission and its script opens the pause dialog and presses its Enhanced Resume
#    Mission seat, which only exists when the Enhanced dialog is the one open.
printf 'tick 2\noptions\nemclick Resume Mission\ntick 1\nquit\n' > "$work/pause.script"
games=td
[ "$ra" = 1 ] && games="td ra"
for g in $games; do
    digests=""
    for m in classic n64 enhanced; do
        o="$work/$g-$m"
        mkdir -p "$o"
        if [ "$g" = ra ]; then
            run ./opencnc3d-launcher --dir . --module ra3d --play -- --gfx "$work/$m.cfg" \
                --harness 1 --rounds 1 --no-movies --nosound --script "$work/pause.script" \
                --shotdir "$o" > "$o/run.log" 2>&1 || true
        else
            run ./opencnc3d --gfx "$work/$m.cfg" --harness 1 --rounds 1 --no-movies --nosound \
                --shotdir "$o" > "$o/run.log" 2>&1 || true
        fi
        enhanced "$o/run.log" "$g $m main menu"
        grep -Fq 'HARNESS|end|1 round(s), 0 failure(s)' "$o/run.log" \
            || fail "$g $m: the harness did not end cleanly (the menu after the mission differs from the one before it, or a step failed)" "$o/run.log"
        if [ "$g" = ra ]; then
            grep -Fq 'OPTIONS|script|emclick|Resume Mission|act=1|' "$o/run.log" \
                || fail "ra $m: the pause dialog is not the Enhanced one" "$o/run.log"
        fi
        d=$(sed -n 's/^HARNESS|menu0|.*digest=\([0-9a-f]*\).*/\1/p' "$o/run.log" | head -1)
        [ -n "$d" ] || fail "$g $m: no menu digest" "$o/run.log"
        digests="$digests $m=$d"
    done
    n=$(echo "$digests" | tr ' ' '\n' | sed -n 's/.*=//p' | sort -u | wc -l | tr -d ' ')
    [ "$n" = 1 ] || fail "$g: the main menu is not the same picture in every mode:$digests" "$work/$g-classic/run.log"
    echo "MENUS|$g|menu|$digests"
done
# Tiberian Dawn's harness round plays no script, so its pause dialog is opened in a mission
# run of the renderer, in the two modes that used to draw the 1995 dialog.
for m in classic n64; do
    run ./cnc_eyes --scen SCG01EA --nosound --gfx "$work/$m.cfg" --script "$work/pause.script" \
        > "$work/td-$m-pause.log" 2>&1 || true
    grep -Fq 'OPTIONS|script|emclick|Resume Mission|act=1|' "$work/td-$m-pause.log" \
        || fail "td $m: the pause dialog is not the Enhanced one" "$work/td-$m-pause.log"
done
echo "MENUS|pause|Enhanced in Classic and N64 (td$( [ "$ra" = 1 ] && echo ', ra'))"

# 2. THE ROUTES UNDER CLASSIC. The Skirmish lobby pressed through to Play, in both games.
run ./opencnc3d --gfx "$work/classic.cfg" --lobbyplay 30 --no-movies --nosound \
    --shotdir "$work/td-classic" > "$work/td-skirmish.log" 2>&1 || true
enhanced "$work/td-skirmish.log" "td Skirmish"
grep -Fq 'LOBBY|click|Play|' "$work/td-skirmish.log" || fail "td: the lobby did not open, or its Play was not pressed" "$work/td-skirmish.log"
grep -Fq 'GAMELOOP|end|ticks=30|' "$work/td-skirmish.log" || fail "td: the skirmish did not play its 30 ticks" "$work/td-skirmish.log"
if [ "$ra" = 1 ]; then
    run ./opencnc3d-launcher --dir . --module ra3d --play -- --gfx "$work/classic.cfg" \
        --lobbyplay 30 --no-movies --nosound --shotdir "$work/ra-classic" > "$work/ra-skirmish.log" 2>&1 || true
    enhanced "$work/ra-skirmish.log" "ra Skirmish"
    grep -Fq 'LOBBY|click|Play|' "$work/ra-skirmish.log" || fail "ra: the lobby did not open, or its Play was not pressed" "$work/ra-skirmish.log"
    grep -Fq 'APP|raskirmish|start|SCM01EA|' "$work/ra-skirmish.log" || fail "ra: the lobby's Play did not start a Red Alert skirmish" "$work/ra-skirmish.log"
    grep -Fq 'GAMELOOP|end|ticks=30|' "$work/ra-skirmish.log" || fail "ra: the skirmish did not play its 30 ticks" "$work/ra-skirmish.log"

    # Load Mission: a Red Alert save of Allied 1, the button pressed, the newest slot taken
    mkdir -p "$work/save" "$work/load"
    printf 'tick 120\nsave 0 menu load\nquit\n' > "$work/save.txt"
    run ./cnc_eyes --ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dir ra3d/missions/ \
        --dosdata ra3d/audio --nosound --w 640 --h 400 --scen SCG01EA --pack ra3d/3d/SCG01EA.pack \
        --gfx "$work/classic.cfg" --savedir "$work/save" --script "$work/save.txt" > "$work/save.log" 2>&1 || true
    grep -Eq '^SAVE\|slot=0\|menu load\|.*\|frame=120\|scen=SCG01EA\|' "$work/save.log" || fail "ra: the save for Load Mission was not written" "$work/save.log"
    run ./opencnc3d-launcher --dir . --module ra3d --play -- --gfx "$work/classic.cfg" \
        --harness 60 --rounds 1 --harnessload --savedir "$work/save" \
        --no-movies --nosound --shotdir "$work/load" > "$work/load.log" 2>&1 || true
    enhanced "$work/load.log" "ra Load Mission"
    grep -Fq 'HARNESS|click|LOAD MISSION' "$work/load.log" || fail "ra: Load Mission was not pressed" "$work/load.log"
    grep -Fq 'APP|load|slot=0|scen=SCG01EA|pack=ra3d/3d/SCG01EA.pack|' "$work/load.log" || fail "ra: Load Mission did not take Allied 1's slot" "$work/load.log"
    grep -Eq '^LOAD\|slot=0\|frame=120\|scen=SCG01EA\|' "$work/load.log" || fail "ra: the slot did not load at frame 120" "$work/load.log"

    # Multiplayer: the room screen opens
    mkdir -p "$work/mp"
    run ./opencnc3d-launcher --dir . --module ra3d --play -- --gfx "$work/classic.cfg" \
        --mpplay 3000 --no-movies --nosound --shotdir "$work/mp" > "$work/mp.log" 2>&1 || true
    enhanced "$work/mp.log" "ra Multiplayer"
    grep -Fq 'MPPLAY|click|Multiplayer' "$work/mp.log" || fail "ra: Multiplayer was not pressed" "$work/mp.log"
    grep -Fq 'MPPLAY|left|' "$work/mp.log" || fail "ra: the multiplayer screen did not open" "$work/mp.log"
    echo "MENUS|routes|Classic: Skirmish's lobby and its match in both games; Red Alert's Load Mission and Multiplayer"
else
    echo "MENUS|routes|Classic: Skirmish's lobby and its match in Tiberian Dawn"
fi
echo "Menus in every graphics mode: the same Enhanced main menu in Classic, N64 and Enhanced, the Enhanced pause dialog, the routes under Classic: PASS"
