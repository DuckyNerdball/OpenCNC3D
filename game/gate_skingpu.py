"""Whole soldiers are skinned on the card, and the switch puts every pose back on the CPU.

usage: gate_skingpu.py script DIR          writes the runs' scripts into DIR
       gate_skingpu.py check DIR           prints "1 message" or "0 reasons"

Two scenes, each drawn with the soldiers skinned on the card as shipped ("on") and with
OPENCNC3D_SKIN_GPU=0 ("off"), the anti-aliasing off so each picture is one frame:
  td  SCG08EA's base close up, its riflemen and engineers standing and walking, then the
      same moment in Classic and in N64;
  ra  Red Alert's snow mission with the six soldiers that have models of their own (Medic,
      Spy, Thief, Tanya, General Stavros and the Attack Dog) staged beside its own.
The base is drawn a third time under OPENCNC3D_SKIN_GPU=verify, which also checks every
palette and the packed bones and weights against the CPU's skin of the same pose, and a
fourth time with the switch unset and OPENCNC3D_SKIN_GPU_VENDOR naming a driver the card
path has not been run on ("vendor").

Claims:
  (1) LIVING WHOLE BODIES ARE SKINNED ON THE CARD: as shipped no pose is skinned on the
      CPU or sent to the card as vertices, and the draws skinned on the card, one palette
      each, are as many as the posed draws the CPU path makes; in Red Alert all six of its
      own soldier types are among them.
  (2) THE PALETTE PATH IS THE CPU SKIN: under verify every checked vertex is within a
      thousandth of a model unit of the CPU's, and the picture is the shipped one exactly.
  (3) THE PICTURES: each scene as the CPU skin draws it, to within PIXELS pixels each off
      by at most LEVELS levels.
  (4) THE SWITCH PUTS EVERYTHING BACK: with it off nothing is skinned on the card and every
      posed draw is served by the frame's poses from the card as before (a still frame's
      poses come back from the cross-frame rest cache as hits, so it may skin none), and
      Classic and N64, which draw no 3D soldier, are the same pictures to the byte either
      way.
  (5) THE PALETTES DO NOT EMPTY THE UNIFORM CACHE: inside the ground-normal and bake passes
      no array set forgot every kept value, and the palettes' sets forgot only their own
      locations.
  (6) UNSET, THE CARD SKINS ONLY ON A DRIVER IT HAS BEEN RUN ON: as shipped the switch was
      decided from the build and the vendor string (Apple's OpenGL on a macOS build, NVIDIA's
      driver on any other), and with another vendor's string in its place every pose is
      skinned on the CPU and the picture is the switch at 0's to the byte.
"""
import os
import re
import sys

import numpy as np
from PIL import Image

# The tolerance G332 allows a soldier drawn from the card against system memory. Measured
# on macOS at 1280x800 with the anti-aliasing off: both scenes identical to the byte.
PIXELS, LEVELS = 64, 12
RA_TYPES = ("MEDI", "SPY", "THF", "E7", "GNRL", "DOG")


def script(d):
    os.makedirs(d, exist_ok=True)
    for arm in ("on", "off", "verify"):
        with open(os.path.join(d, "td_%s.txt" % arm), "w") as f:
            f.write("gfx show_stats 0\ngfx aa_mode 0\ntick 60\nreveal 1\ncam 47 48\ndist 1500\n"
                    "smoothmove 1\ntick 1\ntickalpha 0\nframes 2\nshot %s/td_%s.png\nm3dump\n" % (d, arm))
            f.write("gfxmode 0\ntick 1\ntickalpha 0\nshot %s/classic_%s.png\n" % (d, arm))
            f.write("gfxmode 1\ntick 1\ntickalpha 0\nshot %s/n64_%s.png\nquit\n" % (d, arm))
    with open(os.path.join(d, "td_vendor.txt"), "w") as f:
        f.write("gfx show_stats 0\ngfx aa_mode 0\ntick 60\nreveal 1\ncam 47 48\ndist 1500\n"
                "smoothmove 1\ntick 1\ntickalpha 0\nframes 2\nshot %s/td_vendor.png\nm3dump\nquit\n" % d)
    for arm in ("on", "off"):
        with open(os.path.join(d, "ra_%s.txt" % arm), "w") as f:
            f.write("gfx show_stats 0\ngfx aa_mode 0\ntick 30\nreveal 1\n")
            for i, t in enumerate(RA_TYPES):
                f.write("spawnobj %s %d %d\n" % (t, 80 + i % 3, 62 + i // 3))
            f.write("tick 30\ncam 81 62.5\ndist 700\nsmoothmove 1\ntick 1\ntickalpha 0\nframes 2\n"
                    "shot %s/ra_%s.png\nm3dump\nquit\n" % (d, arm))


def last(path, tag):
    r = [dict(re.findall(r"(\w+)=([-\w.+]+)", l)) for l in open(path, errors="replace")
         if l.startswith(tag + "|")]
    return r[-1] if r else None


def picture(pa, pb):
    x = np.asarray(Image.open(pa).convert("RGB")).astype(np.int32)
    y = np.asarray(Image.open(pb).convert("RGB")).astype(np.int32)
    diff = np.abs(x - y).max(2)
    return int((diff > 0).sum()), int(diff.max())


def same_bytes(pa, pb):
    return open(pa, "rb").read() == open(pb, "rb").read()


def check(d):
    bad, notes = [], []
    runs = {}
    for name in ("td_on", "td_off", "td_verify", "ra_on", "ra_off", "td_vendor"):
        p = os.path.join(d, name + ".log")
        g = last(p, "M3GPUSKIN") if os.path.exists(p) else None
        pose = last(p, "M3POSE") if os.path.exists(p) else None
        dump = last(p, "M3DUMP") if os.path.exists(p) else None
        if not g or not pose or not dump or "gpuskin" not in pose:
            bad.append("%s: no M3GPUSKIN, M3POSE with gpuskin, or M3DUMP line (an older binary, or the run failed)"
                       % name)
            continue
        runs[name] = (g, {k: int(v) for k, v in pose.items()}, dump, p)
    if len(runs) < 6:
        return "0 " + "; ".join(bad)
    # (1) living whole bodies on the card, and (4) the switch off
    for sc in ("td", "ra"):
        g, on, dump, _ = runs[sc + "_on"]
        go, off, _, _ = runs[sc + "_off"]
        inf, posed = int(dump.get("inf", "0")), off["skins"] + off["hits"]
        if g.get("mode") != "1" or g.get("programs") != "1":
            bad.append("%s: the switch reads %s and the programs %s (want 1 and 1; unset, the card skins only on "
                       "Apple's OpenGL on Apple silicon or NVIDIA's driver outside macOS)"
                       % (sc, g.get("mode"), g.get("programs")))
        if on["skins"] or on["uploads"] or on["gpudraws"] or on["gpuskin"] == 0 \
                or on["gpuskin"] != on["palettes"] or on["gpuskin"] != posed:
            bad.append("%s: as shipped %d CPU skins, %d uploads, %d draws of them; %d draws skinned on the card "
                       "from %d palettes, against %d posed draws with it off (want 0, 0, 0 and every posed draw "
                       "on the card, one palette each)" % (sc, on["skins"], on["uploads"], on["gpudraws"],
                                                            on["gpuskin"], on["palettes"], posed))
        if inf < 6:
            bad.append("%s: %d soldiers drawn (want 6 or more)" % (sc, inf))
        if go.get("mode") != "0" or off["gpuskin"] or off["palettes"] or posed == 0 \
                or off["gpudraws"] != posed or off["uploads"] > off["skins"]:
            bad.append("%s: with the switch off it reads %s, %d draws on the card from %d palettes, %d skins + %d hits, "
                       "%d drawn from the card, %d uploads (want 0, 0, 0, posed draws all from the card and at most "
                       "one upload a skin)" % (sc, go.get("mode"), off["gpuskin"], off["palettes"],
                                               off["skins"], off["hits"], off["gpudraws"], off["uploads"]))
        notes.append("%s %d soldiers: %d draws skinned on the card, none on the CPU (off: %d skins, %d hits)"
                     % (sc, inf, on["gpuskin"], off["skins"], off["hits"]))
    types = set(re.findall(r"^M3INF2\|id=\d+\|type=(\w+)", open(runs["ra_on"][3], errors="replace").read(), re.M))
    missing = [t for t in RA_TYPES if t not in types]
    if missing:
        bad.append("ra: %s not among the soldiers" % ", ".join(missing))
    # (2) the palette path
    gv, v, _, _ = runs["td_verify"]
    checked, vbad, worst = int(gv.get("checked", "0")), int(gv.get("bad", "-1")), float(gv.get("worst", "1e9"))
    if gv.get("mode") != "2" or checked == 0 or vbad != 0 or not worst <= 1e-3 or v["gpuskin"] == 0:
        bad.append("verify: mode %s, %d vertices checked, %d off, worst %.3g (want 2, some, 0 and 0.001 or less)"
                   % (gv.get("mode"), checked, vbad, worst))
    notes.append("verify %d vertices, %d off, worst %.2g" % (checked, vbad, worst))
    shots = {k: os.path.join(d, k + ".png") for k in ("td_on", "td_off", "td_verify", "ra_on", "ra_off",
                                                      "classic_on", "classic_off", "n64_on", "n64_off",
                                                      "td_vendor")}
    gone = [k for k, p in shots.items() if not os.path.exists(p)]
    if gone:
        bad.append("not shot: " + ", ".join(gone))
        return "0 " + "; ".join(bad)
    if not same_bytes(shots["td_on"], shots["td_verify"]):
        bad.append("the verify run's picture is not the shipped one")
    # (3) the pictures
    for sc in ("td", "ra"):
        n, m = picture(shots[sc + "_on"], shots[sc + "_off"])
        if n > PIXELS or m > LEVELS:
            bad.append("%s: %d pixels differ from the CPU skin, by up to %d levels (want %d by %d at most)"
                       % (sc, n, m, PIXELS, LEVELS))
        notes.append("%s as the CPU skin draws it to within %d pixels by %d" % (sc, n, m))
    # (4) Classic and N64
    for k in ("classic", "n64"):
        if not same_bytes(shots[k + "_on"], shots[k + "_off"]):
            n, m = picture(shots[k + "_on"], shots[k + "_off"])
            bad.append("%s differs with the switch off: %d pixels by up to %d levels" % (k, n, m))
    notes.append("Classic and N64 identical either way")
    # (5) the uniform cache
    for name in ("td_on", "td_off", "ra_on", "ra_off"):
        g = runs[name][0]
        if int(g.get("auxfull", "1")) != 0:
            bad.append("%s: %s array sets forgot every kept uniform inside the aux passes (want 0)"
                       % (name, g.get("auxfull")))
    for sc in ("td", "ra"):
        ron, roff = int(runs[sc + "_on"][0].get("auxrange", "0")), int(runs[sc + "_off"][0].get("auxrange", "0"))
        if ron <= roff:
            bad.append("%s: %d ranged forgets as shipped, %d with the switch off (want the palettes' own on top)"
                       % (sc, ron, roff))
    notes.append("aux passes: 0 full forgets, %s ranged (off %s)"
                 % (runs["td_on"][0].get("auxrange"), runs["td_off"][0].get("auxrange")))
    # (6) the unset switch follows the build and the driver
    g = runs["td_on"][0]
    apple, applegl, nvidia = g.get("apple"), g.get("applegl"), g.get("nvidia")
    want = applegl if apple == "1" else nvidia
    if g.get("auto") != "1" or apple not in ("0", "1") or want not in ("0", "1") or g.get("mode") != want:
        bad.append("default: the unset switch read %s from auto=%s apple=%s applegl=%s nvidia=%s (want auto 1 "
                   "and the card only for Apple's OpenGL on macOS or NVIDIA's driver elsewhere)"
                   % (g.get("mode"), g.get("auto"), apple, applegl, nvidia))
    gv, pv = runs["td_vendor"][0], runs["td_vendor"][1]
    if gv.get("auto") != "1" or gv.get("applegl") != "0" or gv.get("nvidia") != "0" or gv.get("mode") != "0" \
            or pv["gpuskin"] or pv["palettes"] or pv["skins"] + pv["hits"] == 0:
        bad.append("another vendor: the unset switch read %s (auto=%s applegl=%s nvidia=%s), %d draws skinned on "
                   "the card from %d palettes, %d CPU skins + %d hits (want 0, 1, 0, 0, no draw on the card and "
                   "every pose on the CPU)" % (gv.get("mode"), gv.get("auto"), gv.get("applegl"), gv.get("nvidia"),
                                               pv["gpuskin"], pv["palettes"], pv["skins"], pv["hits"]))
    elif not same_bytes(shots["td_vendor"], shots["td_off"]):
        n, m = picture(shots["td_vendor"], shots["td_off"])
        bad.append("another vendor: the picture is not the switch at 0's (%d pixels by up to %d levels)" % (n, m))
    notes.append("unset: %s on this driver (apple=%s applegl=%s nvidia=%s), the CPU's skin under another vendor"
                 % ("the card" if g.get("mode") == "1" else "the CPU", apple, applegl, nvidia))
    return ("0 " + "; ".join(bad)) if bad else ("1 " + "; ".join(notes))


if __name__ == "__main__":
    if sys.argv[1] == "script":
        script(sys.argv[2])
    else:
        print(check(sys.argv[2]))
