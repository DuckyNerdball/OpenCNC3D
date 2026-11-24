#!/usr/bin/env python3
"""G380: the remastered trees' wind posed by the card (game/mesh_wind_gpu.h).

  gate_treewindgpu.py script DIR    write the run scripts into DIR
  gate_treewindgpu.py check DIR     read the runs' logs and pictures; first line is
                                    "1 message" or "0 message"

A remastered tree's wind pose used to be worked out on the CPU, corner by corner, and sent
to the card in every frame. Each pose is now one transform feedback draw that writes the
world positions into the pose store every pass reads; OPENCNC3D_MESH_WIND_GPU=0 keeps the
CPU pose and =verify works out every pose both ways. Runs, each in steady state on a
swaying sub-tick (the wind's phase moves with tickalpha, so 0.25 and 0.75, not 0):

  td_on, td_off, td_verify   GDI 8 (SCG08EA) Enhanced with the switch at 1, 0 and verify; the
                             1 and 0 runs then shoot the same view in Classic and N64
  td_auto                    the same with the switch unset: the driver's own default
  td_fault_<what>            the switch at 1 with one way back to the CPU forced (load, link,
                             source, store, check: OPENCNC3D_MESH_WIND_GPU_<WHAT>_OK=0)
  td_still_on, td_still_off  the same view with --notreewind, on and off
  ra_on, ra_off, ra_verify   Red Alert's snow (SCG03EA) Enhanced, on, 0 and verify

Claims:
  (1) THE CARD POSES THE TREES: with the switch at 1 every wind pose of both games is made by
      the card (MESHWINDGPU poses > 0, no fallback, nothing unavailable) and none by the
      CPU's direct or depth paths; with it at 0 the card makes none and the CPU's paths do.
  (2) THE SAME POSES: every verify pose was checked (read back and compared with the CPU's
      pose of the same draw), no corner of either lay outside the box the culling was
      given, and the largest difference is at most four units in the last place of a
      coordinate on a 128-cell map, 3.1e-5 of a cell (MAX_ABS_E9).
  (3) LESS SENT: the frame uploads fall by exactly the 12 bytes a corner the card posed,
      and the sources sent once in their place weigh under a twentieth of that.
  (4) THE SAME PICTURE: the Enhanced frames with the switch at 1 are those with it at 0 to
      within PIXELS pixels each off by at most LEVELS levels; the Classic and N64 frames
      and the --notreewind frames are pixel for pixel the same, and --notreewind poses
      nothing on the card.
  (5) THE DEFAULT: unset, the switch poses on the card only on NVIDIA's driver off macOS
      (the one driver it was measured and checked on); in a build for macOS, whatever the
      GPU, and on any other driver it keeps the CPU's pose and draws the frame the switch at
      0 draws, byte for byte.
  (6) EVERY WAY BACK: with the switch at 1 and each fault forced in turn, a context without
      transform feedback, a program the driver refuses to link, a source store and a pose
      store the driver did not make, and a first pose that disagrees with the CPU's, the
      card makes no pose, the fault is counted where it happened, the CPU makes exactly the
      poses and uploads of the switch at 0, and the frame is that frame byte for byte.
"""
import os
import re
import sys

import numpy as np
from PIL import Image

MAX_ABS_E9 = 31000       # four units in the last place of a coordinate on a 128-cell map, in billionths of a cell
PIXELS, LEVELS = 64, 6   # see the measurement in the gate's header in gates.sh

# What each forced fault must leave in the MESHWINDGPU line (and in the log).
FAULTS = {
    "load": (dict(ready=0, poses=0, sources=0, fallbacks=0), "unavailable", "MESHWINDGPU|unavailable|no transform feedback"),
    "link": (dict(ready=0, poses=0, sources=0, fallbacks=0), "unavailable", "MESHWINDGPU|link|failed"),
    "source": (dict(ready=1, poses=0, sources=0, source_entries=0), "fallbacks", "MESHWINDGPU|ready|"),
    "store": (dict(ready=1, poses=0), "fallbacks", "MESHWINDGPU|ready|"),
    "check": (dict(ready=0, poses=0, check_failures=1, verified=1, fallbacks=1), "unavailable",
              "MESHWINDGPU|unavailable|the first pose did not match the CPU's"),
}

TD_STEPS = ["tick 60", "reveal 1", "cam 47 48", "dist 3000", "smoothmove 1", "tick 1"]
RA_STEPS = ["tick 60", "reveal 1", "cam 80 62", "dist 3400", "smoothmove 1", "tick 1"]


def script(d):
    os.makedirs(os.path.join(d, "saves"), exist_ok=True)
    def write(name, lines):
        with open(os.path.join(d, name + ".txt"), "w") as f:
            f.write("\n".join(["gfx show_stats 0"] + lines + ["quit"]) + "\n")
    for arm in ("on", "off", "verify"):
        modes = [] if arm == "verify" else [
            "gfxmode 0", "frames 2", "shot %s/td_%s_classic.png" % (d, arm),
            "gfxmode 1", "frames 2", "shot %s/td_%s_n64.png" % (d, arm)]
        write("td_" + arm, TD_STEPS + ["tickalpha 0.25", "frames 2", "shot %s/td_%s.png" % (d, arm),
                                       "meshperfdump"] + modes)
        write("ra_" + arm, RA_STEPS + ["tickalpha 0.75", "frames 2", "shot %s/ra_%s.png" % (d, arm),
                                       "meshperfdump"])
    write("td_auto", TD_STEPS + ["tickalpha 0.25", "frames 2", "shot %s/td_auto.png" % d, "meshperfdump"])
    for what in FAULTS:
        write("td_fault_" + what, TD_STEPS + ["tickalpha 0.25", "frames 2",
                                              "shot %s/td_fault_%s.png" % (d, what), "meshperfdump"])
    for arm in ("on", "off"):
        write("td_still_" + arm, TD_STEPS + ["tickalpha 0.25", "frames 2",
                                             "shot %s/td_still_%s.png" % (d, arm), "meshperfdump"])


def rows(d, name, bad):
    path = os.path.join(d, name + ".log")
    if not os.path.exists(path):
        bad.append(name + ": no log")
        return {}
    text = open(path, errors="replace").read()
    if not re.search(r"^SCRIPT\|end .+: \d+ lines, 0 failures$", text, re.M):
        bad.append(name + ": the script did not complete without failures")
    out = {}
    for line in text.splitlines():
        head = line.split("|", 1)[0]
        if head in ("MESHWINDGPU", "MESHWINDDIRECT", "MESHWINDDEPTH", "MESHPERF") and "=" in line:
            out[head] = dict((k, int(v)) for k, v in re.findall(r"(\w+)=(-?\d+)", line))
    for head in ("MESHWINDGPU", "MESHWINDDIRECT", "MESHWINDDEPTH", "MESHPERF"):
        if head not in out:
            bad.append(name + ": no " + head + " line")
            out[head] = {}
    return out


def pixels(d, a, b):
    pa, pb = os.path.join(d, a + ".png"), os.path.join(d, b + ".png")
    if not (os.path.exists(pa) and os.path.exists(pb)):
        return None
    x = np.asarray(Image.open(pa).convert("RGB")).astype(np.int32)
    y = np.asarray(Image.open(pb).convert("RGB")).astype(np.int32)
    if x.shape != y.shape:
        return (x.size, 255)
    diff = np.abs(x - y).max(2)
    return int((diff > 0).sum()), int(diff.max())


def check(d, games=("td", "ra")):
    bad, notes = [], []
    r = {name: rows(d, name, bad) for name in
         ["td_still_on", "td_still_off", "td_auto"] + ["td_fault_" + what for what in FAULTS]
         + [g + arm for g in games for arm in ("_on", "_off", "_verify")]}
    for game in games:
        on, off, ver = r[game + "_on"], r[game + "_off"], r[game + "_verify"]
        g, go, gv = on["MESHWINDGPU"], off["MESHWINDGPU"], ver["MESHWINDGPU"]
        # (1)
        if not (g.get("ready") == 1 and g.get("poses", 0) > 0 and g.get("fallbacks", 1) == 0
                and g.get("unavailable", 1) == 0):
            bad.append("%s: the card did not make every pose (%s)" % (game, g))
        if on["MESHWINDDIRECT"].get("draws", 1) or on["MESHWINDDEPTH"].get("draws", 1):
            bad.append(game + ": the CPU still posed trees with the switch on")
        if go.get("poses", 1) != 0 or (off["MESHWINDDIRECT"].get("draws", 0) + off["MESHWINDDEPTH"].get("draws", 0)) <= 0:
            bad.append(game + ": with the switch at 0 the CPU did not pose the trees")
        # (2)
        if not (gv.get("verified", 0) > 0 and gv.get("verified") == gv.get("poses") and gv.get("outside", 1) == 0
                and gv.get("read_failures", 1) == 0 and gv.get("check_failures", 1) == 0
                and gv.get("fallbacks", 1) == 0):
            bad.append("%s verify: not every pose was checked inside its box (%s)" % (game, gv))
        if gv.get("max_abs_e9", MAX_ABS_E9 + 1) > MAX_ABS_E9:
            bad.append("%s verify: the card's pose is %.2g of a cell from the CPU's (want at most %.0e)"
                       % (game, gv.get("max_abs_e9", 0) * 1e-9, MAX_ABS_E9 * 1e-9))
        # (3)
        drop = off["MESHPERF"].get("upload_bytes", 0) - on["MESHPERF"].get("upload_bytes", 0)
        want = 12 * g.get("vertices", 0)
        if drop != want or want <= 0:
            bad.append("%s: uploads fell by %d bytes, want exactly %d (12 a card-posed corner)" % (game, drop, want))
        if g.get("source_upload_bytes", want) * 20 >= max(want, 1):
            bad.append("%s: the card's sources (%d bytes) are not under a twentieth of the drop" %
                       (game, g.get("source_upload_bytes", 0)))
        # (4)
        pd = pixels(d, game + "_on", game + "_off")
        if pd is None:
            bad.append(game + ": the on and off frames were not both shot")
        elif pd[0] > PIXELS or pd[1] > LEVELS:
            bad.append("%s: %d pixels differ between the card's pose and the CPU's, by up to %d levels" %
                       (game, pd[0], pd[1]))
        notes.append("%s %d poses, %.1f MB less sent (sources %.2f MB), verify max %.2g cell over %d poses, "
                     "on/off %s px" % (game, g.get("poses", 0), drop / 1e6, g.get("source_upload_bytes", 0) / 1e6,
                                       gv.get("max_abs_e9", 0) * 1e-9, gv.get("verified", 0),
                                       "%d by %d" % pd if pd else "?"))
    for a, b, what in (("td_on_classic", "td_off_classic", "Classic"), ("td_on_n64", "td_off_n64", "N64"),
                       ("td_still_on", "td_still_off", "--notreewind")):
        pd = pixels(d, a, b)
        if pd is None:
            bad.append(what + ": the frames were not both shot")
        elif pd != (0, 0):
            bad.append("%s: the switch changed %d pixels by up to %d levels" % (what, pd[0], pd[1]))
    if r["td_still_on"]["MESHWINDGPU"].get("poses", 1) != 0:
        bad.append("--notreewind: the card still posed trees")
    notes.append("Classic, N64 and --notreewind identical on and off")
    # (5)
    ga = r["td_auto"]["MESHWINDGPU"]
    apple, nvidia = ga.get("apple", -1), ga.get("nvidia", -1)
    card = apple == 0 and nvidia == 1
    where = "macOS" if apple == 1 else "NVIDIA's driver" if nvidia == 1 else "this driver"
    if ga.get("auto") != 1 or apple not in (0, 1) or nvidia not in (0, 1) or ga.get("mode") != (1 if card else 0):
        bad.append("default: the unset switch did not follow the platform and driver (%s)" % ga)
    elif not card:
        pd = pixels(d, "td_auto", "td_off")
        if ga.get("poses", 1) != 0 or pd != (0, 0):
            bad.append("default on %s: the card posed %s trees, %s pixels from the switch at 0"
                       % (where, ga.get("poses"), pd))
        else:
            notes.append("unset on %s: the CPU's pose, the frame of the switch at 0" % where)
    elif ga.get("poses", 0) <= 0:
        bad.append("default: the unset switch did not pose on the card on NVIDIA's driver")
    else:
        notes.append("unset on NVIDIA's driver: the card's pose")
    # (6)
    off = r["td_off"]
    cpu = lambda run: (run["MESHWINDDIRECT"].get("draws", -1), run["MESHWINDDIRECT"].get("vertices", -1),
                       run["MESHWINDDEPTH"].get("draws", -1), run["MESHWINDDEPTH"].get("vertices", -1))
    held = []
    for what, (want, counted, said) in FAULTS.items():
        name = "td_fault_" + what
        run, g = r[name], r[name]["MESHWINDGPU"]
        wrong = dict((k, g.get(k)) for k, v in want.items() if g.get(k) != v)
        if wrong or g.get(counted, 0) <= 0:
            bad.append("fault %s: the card's counts are wrong (%s, want %s and %s > 0)" % (what, wrong, want, counted))
            continue
        log = open(os.path.join(d, name + ".log"), errors="replace").read() if os.path.exists(
            os.path.join(d, name + ".log")) else ""
        if said not in log:
            bad.append("fault %s: the log does not say %r" % (what, said))
            continue
        if cpu(run) != cpu(off) or cpu(off)[0] + cpu(off)[2] <= 0:
            bad.append("fault %s: the CPU's poses %s are not those of the switch at 0 %s" % (what, cpu(run), cpu(off)))
            continue
        if run["MESHPERF"].get("upload_bytes", -1) != off["MESHPERF"].get("upload_bytes", -2):
            bad.append("fault %s: %s bytes sent, the switch at 0 sent %s" % (
                what, run["MESHPERF"].get("upload_bytes"), off["MESHPERF"].get("upload_bytes")))
            continue
        pd = pixels(d, name, "td_off")
        if pd != (0, 0):
            bad.append("fault %s: the frame is %s pixels from the switch at 0" % (what, pd))
            continue
        held.append(what)
    if len(held) == len(FAULTS):
        notes.append("every way back (%s) gave the CPU's poses and the frame of the switch at 0" % ", ".join(held))
    return ("0 " + "; ".join(bad)) if bad else ("1 " + "; ".join(notes))


if __name__ == "__main__":
    if sys.argv[1] == "script":
        script(sys.argv[2])
    else:
        print(check(sys.argv[2], ("td",) if sys.argv[3:4] == ["td"] else ("td", "ra")))
