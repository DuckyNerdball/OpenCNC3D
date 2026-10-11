#!/usr/bin/env python3
"""Put the soldier colours saved in the anim lab into the game.

    python3 tools/anim_lab/push_colours.py            # write them into game/cnc_eyes.cpp
    python3 tools/anim_lab/push_colours.py --check    # say whether the two agree, change nothing

The lab saves to tools/anim_lab/colours.json:

    {"gdi": {"team": [r, g, b], "cloth": [r, g, b], "amount": a}, "nod": {...}}

linear values, exactly what the model shader takes. This rewrites the block between
BEGIN SOLDIER PALETTE and END SOLDIER PALETTE in game/cnc_eyes.cpp and nothing else. The
same parser is what the lab's server reads the game's current values with, so the page
can say whether what it shows is in the game yet.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
EYES = os.path.join(REPO, "game", "cnc_eyes.cpp")
MODEL = os.path.join(REPO, "game", "model3d_mod.h")
SAVED = os.path.join(HERE, "colours.json")
BEGIN, END = "/* BEGIN SOLDIER PALETTE */\n", "/* END SOLDIER PALETTE */\n"
SIDES = ("gdi", "nod")
FLOAT = r"(-?[0-9.]+)f"


def _block(src):
    i, j = src.index(BEGIN), src.index(END)
    return i + len(BEGIN), j


def read_game(src=None):
    """The palette as the game has it, in the saved file's shape."""
    if src is None:
        src = open(EYES, encoding="latin-1").read()
    i, j = _block(src)
    b = src[i:j]

    def rows(name, n):
        body = b[b.index(name):]
        body = body[:body.index(";")]
        vals = [float(x) for x in re.findall(FLOAT, body)]
        return [vals[k * n:(k + 1) * n] for k in range(2)]
    team, cloth = rows("SOLDIER_TEAM", 3), rows("SOLDIER_CLOTH[", 3)
    amt = rows("SOLDIER_CLOTH_AMT", 1)
    return {s: {"team": team[k], "cloth": cloth[k], "amount": amt[k][0]}
            for k, s in enumerate(SIDES)}


def cloth_ref():
    src = open(MODEL, encoding="latin-1").read()
    return float(re.search(r"#define M3_CLOTH_REF " + FLOAT, src).group(1))


def logo_colour():
    src = open(MODEL, encoding="latin-1").read()
    return [float(re.search(r"#define M3_LOGO_%s " % c + FLOAT, src).group(1)) for c in "RGB"]


def render(p):
    f = lambda v: "%.4ff" % v          # noqa: E731
    trip = lambda v: "{ %s, %s, %s }" % tuple(f(x) for x in v)   # noqa: E731
    return ("static const float SOLDIER_TEAM[2][3]   = { %s,\n"
            "                                            %s };\n"
            "static const float SOLDIER_CLOTH[2][3]  = { %s,\n"
            "                                            %s };\n"
            "static const float SOLDIER_CLOTH_AMT[2] = { %s, %s };\n") % (
        trip(p["gdi"]["team"]), trip(p["nod"]["team"]),
        trip(p["gdi"]["cloth"]), trip(p["nod"]["cloth"]),
        f(p["gdi"]["amount"]), f(p["nod"]["amount"]))


def validate(p):
    """Refuse anything that is not two sides of three numbers in 0..4 and an amount in 0..1:
    the file is written by a web page."""
    for s in SIDES:
        e = p[s]
        for k in ("team", "cloth"):
            if len(e[k]) != 3 or not all(0.0 <= float(x) <= 4.0 for x in e[k]):
                raise ValueError("%s %s out of range" % (s, k))
        if not 0.0 <= float(e["amount"]) <= 1.0:
            raise ValueError("%s amount out of range" % s)
    return {s: {"team": [round(float(x), 4) for x in p[s]["team"]],
                "cloth": [round(float(x), 4) for x in p[s]["cloth"]],
                "amount": round(float(p[s]["amount"]), 4)} for s in SIDES}


def main(argv):
    if not os.path.exists(SAVED):
        print("PALETTE|nothing saved in the lab yet (%s)" % os.path.relpath(SAVED, REPO))
        return 1
    saved = validate(json.load(open(SAVED)))
    src = open(EYES, encoding="latin-1").read()
    same = validate(read_game(src)) == saved
    if "--check" in argv:
        print("PALETTE|%s" % ("the game has the saved colours" if same
                              else "the saved colours are NOT in the game yet"))
        return 0 if same else 1
    if same:
        print("PALETTE|already in the game, nothing to do")
        return 0
    i, j = _block(src)
    src = src[:i] + render(saved) + src[j:]
    open(EYES, "w", encoding="latin-1").write(src)
    for s in SIDES:
        print("PALETTE|%s|team %s|cloth %s|amount %.2f" % (
            s, saved[s]["team"], saved[s]["cloth"], saved[s]["amount"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
