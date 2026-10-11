#!/usr/bin/env python3
"""
G319: an aircraft's shadow in Classic is a dark smudge on the ground, and a flying
helicopter's rotor is a dark blur, as the 1995 engine draws them.

AircraftClass::Draw_It draws the aircraft's own frame a second time at its ground
position with SHAPE_PREDATOR | SHAPE_FADING, and a flying rotor uses the same pair;
CC_Draw_Shape turns it into Map.SpecialGhost, which darkens whatever is underneath. Classic
used to honour that only for Red Alert, so in Tiberian Dawn every Orca, Apache and A-10
was drawn twice in full colour, and a flying Apache's rotor came out as palette index 4
raw, which is light green.

    python3 gate_airshadow.py SHADOW_LOG ROTOR_LOG SHOTS_DIR

The shadows are picked by the ENGINE's draw flags, and the control frame leaves out
exactly those layers, so a renderer that stops treating them as shadows fails on its
pixels, not merely on a label.

Leg 1 (SHADOW_LOG, game/gate_airshadow.txt): two frames of one tick, shadows on and off.
For each A-10 shadow on screen the pixels that differ are the shadow's own, and
  * nothing outside the shadows' rectangles differs (the two frames are comparable);
  * the shadow covers a real area, not a stray pixel;
  * every pixel over ground brighter than the shade ramp's floor is darker than that
    ground, and the ground keeps 50 to 80% of its luminance (the 1995 table keeps 67%
    on Temperate grass);
  * it is far darker than its aircraft (the same silhouette, moved by the rectangles'
    offset) and does not repeat its colours.
Leg 2 (ROTOR_LOG, game/gate_airshadow_rotor.txt): a flying Apache's rotor is drawn as a
shadow, and not one pixel of its rectangle is palette index 4's colour.

Prints one line per check and a verdict line; exits 0 only if every check passed.
"""

import os
import re
import sys

import numpy as np
from PIL import Image

LUM = np.array([0.299, 0.587, 0.114])
# The 1995 table maps every colour onto the palette's 16-entry shade ramp, whose darkest
# Temperate entry is (28,24,8), luminance 23.4: ground darker than that comes out a hair
# LIGHTER under a 1995 shadow, and under Classic's fitted blend (36,24,0 at 164/255)
# likewise below about 25. Ground at or above this floor must always darken.
FLOOR = 30.0
bad = []


def check(ok, msg):
    print(("ok   " if ok else "BAD  ") + msg)
    if not ok:
        bad.append(msg)
    return ok


def fadepred(r):
    """The engine drew this sub-object with SHAPE_FADING | SHAPE_PREDATOR (0x0300)."""
    return r["sub"] != "0" and (int(r["flags"]) & 0x0300) == 0x0300


def probes(path):
    """[(frame header dict, [record dicts])] for each dosairprobe in the log."""
    out = []
    for line in open(path, errors="replace"):
        line = line.strip()
        if not line.startswith("DOSAIR|"):
            continue
        f = line.split("|")
        if f[1] == "frame":
            out.append((dict(kv.split("=", 1) for kv in f[2:]), []))
        elif out:
            r = dict(kv.split("=", 1) for kv in f[2:])
            r["asset"] = f[1]
            r["rect"] = [int(round(float(v))) for v in r["rect"].split(",")]
            out[-1][1].append(r)
    return out


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.int32)


def leg_shadow(log, shots):
    p = probes(log)
    if not check(len(p) == 2, "leg 1: two probes in the log (found %d)" % len(p)):
        return
    (h_on, on_recs), (h_off, off_recs) = p
    check(h_on.get("shadows") == "1" and h_off.get("shadows") == "0",
          "leg 1: the first frame has its shadows, the second does not")
    on, off = load(os.path.join(shots, "g319_on.png")), load(os.path.join(shots, "g319_off.png"))
    H, W = on.shape[:2]
    fbw, fbh = (int(v) for v in h_on["fb"].split("x"))
    if not check((W, H) == (fbw, fbh), "leg 1: the shot is the framebuffer (%dx%d, fb %dx%d)"
                 % (W, H, fbw, fbh)):
        return
    key = lambda r: (r["asset"], r["id"], r["sub"])
    check(sorted((key(r), r["rect"]) for r in on_recs) == sorted((key(r), r["rect"]) for r in off_recs),
          "leg 1: every aircraft layer is in the same place in both frames")
    shadows = [r for r in on_recs if r["asset"] == "A10" and fadepred(r)]
    check(len(shadows) >= 1 and all(r["shadow"] == "1" for r in shadows),
          "leg 1: every A-10 layer the engine flags FADING|PREDATOR was drawn as a shadow"
          " (%d of %d)" % (sum(r["shadow"] == "1" for r in shadows), len(shadows)))
    bodies = {r["id"]: r for r in on_recs if r["asset"] == "A10" and r["sub"] == "0"}
    diff = np.any(on != off, axis=2)
    inside = np.zeros_like(diff)
    for r in shadows:
        x0, y0, x1, y1 = r["rect"]
        inside[max(0, y0):max(0, y1), max(0, x0):max(0, x1)] = True
    check(int((diff & ~inside).sum()) == 0,
          "leg 1: no pixel outside the shadows' rectangles differs (%d do)" % int((diff & ~inside).sum()))
    seen = 0
    for r in shadows:
        x0, y0, x1, y1 = r["rect"]
        if x0 < 0 or y0 < 0 or x1 > W or y1 > H or r["id"] not in bodies:
            continue
        b = bodies[r["id"]]
        dx, dy = b["rect"][0] - x0, b["rect"][1] - y0
        m = np.zeros_like(diff)
        m[y0:y1, x0:x1] = diff[y0:y1, x0:x1]
        ys, xs = np.nonzero(m)
        tag = "leg 1: A-10 %s shadow" % r["id"]
        if not check(len(ys) >= 400, "%s covers %d pixels (want >= 400)" % (tag, len(ys))):
            continue
        seen += 1
        l_on, l_off = on[ys, xs] @ LUM, off[ys, xs] @ LUM
        lit = l_off >= FLOOR
        darker = int((l_on[lit] < l_off[lit]).sum())
        ratio = float(l_on.mean() / max(l_off.mean(), 1e-6))
        check(lit.sum() >= 200 and darker == int(lit.sum()),
              "%s: %d of the %d pixels over ground brighter than the shade floor are darker"
              " (want all, and >= 200)" % (tag, darker, int(lit.sum())))
        check(0.50 <= ratio <= 0.80, "%s keeps %.2f of the ground's luminance (want 0.50..0.80)"
              % (tag, ratio))
        by, bx = ys + dy, xs + dx
        okb = (by >= 0) & (by < H) & (bx >= 0) & (bx < W)
        body = on[by[okb], bx[okb]]
        l_body = float((body @ LUM).mean())
        check(l_on.mean() < 0.5 * l_body, "%s luminance %.1f, its aircraft's %.1f (want under half)"
              % (tag, l_on.mean(), l_body))
        apart = float(np.abs(on[ys[okb], xs[okb]] - body).sum(1).mean())
        check(apart > 60, "%s differs from its aircraft by %.0f RGB levels a pixel (want > 60; "
              "a full-colour copy is 0)" % (tag, apart))
    check(seen >= 1, "leg 1: at least one A-10 shadow wholly on screen was measured (%d)" % seen)


def leg_rotor(log, shots):
    p = probes(log)
    if not check(len(p) == 1, "leg 2: one probe in the log (found %d)" % len(p)):
        return
    head, recs = p[0]
    rotors = [r for r in recs if r["asset"] in ("RROTOR", "LROTOR") and fadepred(r)]
    if not check(len(rotors) >= 1, "leg 2: the engine flags a flying rotor FADING|PREDATOR (%d)"
                 % len(rotors)):
        return
    check(all(r["shadow"] == "1" for r in rotors), "leg 2: every flying rotor was drawn as a shadow"
          " (%d of %d)" % (sum(r["shadow"] == "1" for r in rotors), len(rotors)))
    img = load(os.path.join(shots, "g319_rotor.png"))
    H, W = img.shape[:2]
    green = np.array([int(v) for v in head["ltgreen"].split(",")])
    for r in rotors:
        x0, y0, x1, y1 = r["rect"]
        box = img[max(0, y0):min(H, y1), max(0, x0):min(W, x1)]
        n = int((np.abs(box - green).max(2) <= 8).sum())
        check(box.size > 0 and n == 0, "leg 2: %s %s rectangle holds %d pixels of palette index 4 "
              "(%s) (want 0)" % (r["asset"], r["id"], n, head["ltgreen"]))


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    leg_shadow(sys.argv[1], sys.argv[3])
    leg_rotor(sys.argv[2], sys.argv[3])
    print("verdict: %s" % ("PASS" if not bad else "FAIL, %d check(s)" % len(bad)))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
