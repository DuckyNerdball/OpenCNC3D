#!/usr/bin/env python3
"""G318: read the HUDQ lines of the production queue strip and judge them.

    gate_hudq.py MAIN.log MORE.log CLASSIC.log

MAIN is gate_hudq.txt under the Enhanced HUD at 1280x720 (the HUD's zoom is 1 there, so
screen pixels are native units). MORE is gate_hudq_more.txt at 854x480. CLASSIC is
gate_hudq.txt with the 1995 sidebar, which must draw no strip at all.

Prints one G318| line per finding and a final verdict line; exits 0 only when every
finding holds.
"""
import re
import sys

TOP, PITCH, SETTLED_X, HIDE = 275, 32, 1280 - 172 - 73 + 21, 40  # bar origin + EHQ_X
CARD = re.compile(r"\|([^:|]+):(\d+):(\d+):([BHWM]):x=(-?\d+):y=(-?\d+):out=([0-9.]+)(:leaving)?")


def frames(text):
    """Every HUDQ frame line, split into stretches by the ECHO markers before it."""
    out, mark = [], "start"
    for line in text.splitlines():
        if line.startswith("ECHO|HUDQ-"):
            mark = line[len("ECHO|"):]
        elif line.startswith("HUDQ|t="):
            cards = [dict(name=m[0], count=int(m[1]), pct=int(m[2]), state=m[3],
                          x=int(m[4]), y=int(m[5]), out=float(m[6]), leaving=bool(m[7]))
                     for m in CARD.findall(line)]
            out.append((mark, cards))
    return out


def eased(values):
    """A move that starts slow, is fastest in its middle third and ends slow, and never
    reverses. values are the positions frame by frame, from rest to rest."""
    d = [abs(b - a) for a, b in zip(values, values[1:])]
    moving = [i for i, v in enumerate(d) if v > 0]
    if len(moving) < 6:
        return False, "only %d moving frames" % len(moving)
    sign = [b - a for a, b in zip(values, values[1:]) if b != a]
    if any((s > 0) != (sign[0] > 0) for s in sign):
        return False, "reverses"
    d = d[moving[0]:moving[-1] + 1]
    # thirds of the move by frame count: the middle one carries the most distance. At a
    # zoom of 1 a frame moves whole pixels, so single deltas tie; thirds do not.
    n = len(d)
    a, b = n // 3, n - n // 3
    first, middle, end = sum(d[:a]) / max(1, a), sum(d[a:b]) / max(1, b - a), sum(d[b:]) / max(1, n - b)
    ok = middle > first and middle > end
    return ok, "frames=%d mean px/frame by third %.2f/%.2f/%.2f" % (n, first, middle, end)


def main():
    main_t, more_t, classic_t = (open(p, errors="replace").read() for p in sys.argv[1:4])
    f = frames(main_t)
    found, bad = [], 0

    def say(ok, what):
        nonlocal bad
        found.append(("ok" if ok else "BAD") + " " + what)
        bad += 0 if ok else 1

    by = lambda mark: [c for m, c in f if m == mark]
    out_frames, settled = by("start") + by("HUDQ-out"), by("HUDQ-out")
    # 1. what is on the strip once it has settled, and in what order
    last = settled[-1] if settled else []
    got = [(c["name"], c["count"], c["state"]) for c in last]
    want = [("NUKE", 1, "B"), ("E1", 10, "B"), ("E2", 1, "W"), ("E3", 1, "W"),
            ("MTNK", 1, "B"), ("JEEP", 1, "W")]
    say(got == want, "settled cards %s (want %s)" % (got, want))
    say(all(c["x"] == SETTLED_X and c["y"] == TOP + i * PITCH and c["out"] == 1.0
            for i, c in enumerate(last)),
        "settled places %s (want x=%d, y=%d+%d*i)" % ([(c["x"], c["y"]) for c in last],
                                                    SETTLED_X, TOP, PITCH))
    # 2. the entrance: every card slides out from under the panel, eased, to its place
    for name in ("NUKE", "JEEP"):
        xs = [c["x"] for cards in out_frames for c in cards if c["name"] == name]
        ok, why = eased(xs)
        say(ok and xs and xs[0] == SETTLED_X + HIDE and xs[-1] == SETTLED_X,
            "%s slides out %d->%d %s" % (name, xs[0] if xs else -1, xs[-1] if xs else -1, why))
    # 3. progress moves on, and the corner count counts down as Minigunners come out
    mid = by("HUDQ-mid")[-1] if by("HUDQ-mid") else []
    pm = {c["name"]: c for c in mid}
    ps = {c["name"]: c for c in last}
    say(all(n in pm and n in ps and pm[n]["pct"] > ps[n]["pct"] for n in ("NUKE", "MTNK")),
        "progress NUKE %s->%s MTNK %s->%s (want both up)" % (
            ps.get("NUKE", {}).get("pct"), pm.get("NUKE", {}).get("pct"),
            ps.get("MTNK", {}).get("pct"), pm.get("MTNK", {}).get("pct")))
    # 4. the exit: the finished power plant slides back under the panel at 100%, then the
    #    cards below it rise one place, eased, to close the gap
    leave = by("HUDQ-leave")
    nx = [c["x"] for cards in leave for c in cards if c["name"] == "NUKE"]
    full = all(c["pct"] == 100 and c["leaving"] for cards in leave for c in cards if c["name"] == "NUKE")
    ok, why = eased([SETTLED_X] + nx + [SETTLED_X + HIDE])
    say(full and ok and nx and nx[-1] > SETTLED_X, "NUKE leaves at 100%% %s" % why)
    gone = next((i for i, cards in enumerate(leave) if all(c["name"] != "NUKE" for c in cards)), None)
    held = gone is not None and all(c["y"] == TOP + PITCH for cards in leave[:gone]
                                    for c in cards if c["name"] == "E1")
    ys = [c["y"] for cards in leave for c in cards if c["name"] == "E1"]
    ok, why = eased(ys)
    say(held and ok and ys and ys[-1] == TOP, "E1 waits for the space, then rises %s" % why)
    left = leave[-1] if leave else []
    say([c["name"] for c in left] == ["E1", "E2", "E3", "MTNK", "JEEP"] and
        all(c["y"] == TOP + i * PITCH for i, c in enumerate(left)),
        "after the exit %s" % [(c["name"], c["y"]) for c in left])
    # 5. on hold, and cancelled: the tank card leaves and the jeep starts in its place
    hold = by("HUDQ-hold")[-1] if by("HUDQ-hold") else []
    say(any(c["name"] == "MTNK" and c["state"] == "H" for c in hold), "MTNK on hold reads H")
    canc = by("HUDQ-cancelled")[-1] if by("HUDQ-cancelled") else []
    say([c["name"] for c in canc][-2:] == ["E3", "JEEP"] and canc[-1]["state"] == "B",
        "after the cancel %s" % [(c["name"], c["state"]) for c in canc])
    # 6. never over the panel: no settled card reaches the panel's left edge
    edge = 1280 - 172 - 12
    say(all(c["x"] + 38 <= edge for cards in by("HUDQ-mid") for c in cards), "clear of the panel edge %d" % edge)
    # 7. overflow: the strip holds what fits and its last place counts the rest
    mf = [c for m, c in frames(more_t) if m == "start"]
    ml = mf[-1] if mf else []
    say([(c["name"], c["count"], c["state"]) for c in ml] ==
        [("NUKE", 1, "B"), ("E1", 1, "B"), ("E2", 1, "W"), ("+", 6, "M")],
        "overflow %s (want NUKE E1 E2 then +6)" % [(c["name"], c["count"]) for c in ml])
    m = re.search(r"^HUDQ\|t=[^|]*\|cap=(\d+)", more_t, re.M)
    say(m is not None and m.group(1) == "4", "overflow capacity %s (want 4)" % (m.group(1) if m else None))
    # 8. the 1995 sidebar draws no strip
    say(not re.search(r"^HUDQ\|t=", classic_t, re.M) and "HUDQLOG|1" in classic_t,
        "classic sidebar draws no strip")
    for line in found:
        print("G318|" + line)
    print("G318|verdict|bad=%d" % bad)
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
