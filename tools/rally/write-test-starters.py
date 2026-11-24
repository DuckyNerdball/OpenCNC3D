#!/usr/bin/env python3
"""Write double-click starters that drop straight into a Rally race.

A course is raced whenever it is played as a skirmish or a network match, so the menus
start a race too (Rally under the lobby's Game Mode lists the courses); these starters skip the
menus with a double-click: one per course, each a race of LAPS laps against seven computer
drivers on a fresh random seed. Beside them goes
RALLY-READ-ME.txt, which says how to start a race from the menus and over a network.

    write-test-starters.py --mac <play folder>          START-RALLY-<n>-<course>.command
    write-test-starters.py --windows <staged folder>    START-RALLY-<n>-<course>.bat

The Windows starters go into the folder tools/win/make-build-win.sh --no-zip stages,
before it is zipped: the package's last entries are fixed (an installed launcher writes
its install record from them), so files are never appended to a finished zip.

The Mac starters run the play folder's own cnc_eyes and TiberianDawn.dylib (named
explicitly, because a play folder inside a checkout otherwise probes for the checkout's
engine first). The Windows starters run cnc_eyes.exe beside them, which loads the
TiberianDawn.dll beside it.
"""
import argparse
import os
import stat
import sys

COURSES = [
    ("SCM70EA", "Tiberium-Run", "Tiberium Run"),
    ("SCM71EA", "Nod-Gauntlet", "Nod Gauntlet"),
    ("SCM72EA", "Frostbite-Pass", "Frostbite Pass"),
]
OPPONENTS = 7
LAPS = 3

README = """RALLY

Rally is a kart race with the game's own vehicles on three circuits: Tiberium Run,
Nod Gauntlet and Frostbite Pass. A race is three laps unless the room says otherwise;
the start line is the finish line, and the first across it on the last lap wins.

ON YOUR OWN
  Double-click a START-RALLY file to race three laps against seven computer drivers.
  Or start the game, choose Skirmish, then Rally under Game Mode at the top right:
  the map list holds the courses, and Match Rules sets the laps (1 to 9).

OVER A NETWORK
  1. Everyone plays this same build.
  2. One player starts the game, chooses Multiplayer, then Host Game. Tick Internet
     Game to play over the internet; leave it off on one network.
  3. In the room, the host chooses Rally under Game Mode at the top right, then the
     course and, under Match Rules, the laps. Empty
     seats can be given to computer drivers.
  4. The others choose Multiplayer, then Join Game: on one network the game is in the
     LAN list, named "<course> / Rally"; over the internet, type the room code the
     host's room shows.
  5. The host starts the game. Everyone picks a vehicle, then the countdown starts.
  Your own vehicle answers the keys at once, however far away the others are.

AFTER A RACE
  The results stay up until the host (on your own, you) clicks RACE AGAIN, which goes
  back to the vehicle choice on the same course and laps with last race's vehicle kept
  unless another is chosen, or END, which ends the match. Everyone else sees who they
  are waiting for. On your own, Enter races again and Esc ends; in a network race Enter
  still opens chat and Esc the menu, so use the buttons.

CONTROLS
  W or Up               accelerate
  S or Down             brake, then reverse
  A, D or Left, Right   steer
  Shift (hold)          drift; let go after a long drift for a speed boost
  Mouse                 aim; the left button fires
  Space or right button use the item from a crate
  R                     back to the last checkpoint
  Esc                   pause menu, where the race can be left
"""


def write_readme(folder, newline):
    path = os.path.join(folder, "RALLY-READ-ME.txt")
    with open(path, "w", newline="") as out:
        out.write(README.replace("\n", newline))
    print(f"wrote {path}")


def flags(scen, brain):
    return (f"--scen {scen} --pack {scen}.pack --cameos cameos.pack --dospack dossidebar.pack "
            f"--dosinf dosinfantry.pack --dylib {brain} --dir missions/ --content content/ "
            f"--skirmish --rally --laps {LAPS} --side gdi --ai {OPPONENTS}")


def mac(folder):
    for f in ("cnc_eyes", "TiberianDawn.dylib", "cameos.pack", "missions"):
        if not os.path.exists(os.path.join(folder, f)):
            sys.exit(f"write-test-starters: {folder} has no {f}; build the play folder first")
    for n, (scen, slug, name) in enumerate(COURSES, 1):
        if not os.path.exists(os.path.join(folder, f"{scen}.pack")):
            sys.exit(f"write-test-starters: {folder} has no {scen}.pack")
        path = os.path.join(folder, f"START-RALLY-{n}-{slug}.command")
        with open(path, "w") as out:
            out.write("#!/bin/sh\n")
            out.write(f"# A Rally race of {LAPS} laps on {name} against {OPPONENTS} computer drivers.\n")
            out.write("# Esc opens the pause menu, where the race can be left.\n")
            out.write('cd "$(dirname "$0")"\n')
            out.write(f"exec ./cnc_eyes {flags(scen, './TiberianDawn.dylib')}\n")
        os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        print(f"wrote {path}")
    write_readme(folder, "\n")


def windows(folder):
    for f in ("cnc_eyes.exe", "TiberianDawn.dll", "cameos.pack", "missions/SCM70EA.INI"):
        if not os.path.exists(os.path.join(folder, f)):
            sys.exit(f"write-test-starters: {folder} has no {f}; stage the Windows folder first")
    for n, (scen, slug, name) in enumerate(COURSES, 1):
        if not os.path.exists(os.path.join(folder, f"{scen}.pack")):
            sys.exit(f"write-test-starters: {folder} has no {scen}.pack")
        path = os.path.join(folder, f"START-RALLY-{n}-{slug}.bat")
        with open(path, "w", newline="") as out:
            out.write("@echo off\r\n")
            out.write(f"rem A Rally race of {LAPS} laps on {name} against {OPPONENTS} computer drivers.\r\n")
            out.write("rem Esc opens the pause menu, where the race can be left.\r\n")
            out.write('cd /d "%~dp0"\r\n')
            out.write(f"cnc_eyes.exe {flags(scen, 'TiberianDawn.dll')}\r\n")
        print(f"wrote {path}")
    write_readme(folder, "\r\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mac")
    ap.add_argument("--windows")
    a = ap.parse_args()
    if not a.mac and not a.windows:
        ap.error("give --mac <play folder> and/or --windows <staged folder>")
    if a.mac:
        mac(a.mac)
    if a.windows:
        windows(a.windows)


if __name__ == "__main__":
    main()
