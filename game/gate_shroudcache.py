#!/usr/bin/env python3
"""G382: the shroud's derived data kept until the shroud changes (game/shroud_mod.h,
OPENCNC3D_SHROUD_CACHE; the fog field in cnc_eyes.cpp and fx_post.h, the air's visibility in
atmosphere_mod.h, the ground patches in mottle_mod.h, the mesh shroud key in cnc_eyes.cpp).

    python3 gate_shroudcache.py script DIR    writes DIR/on.txt, off.txt and verify.txt
    python3 gate_shroudcache.py check DIR     prints "1 <message>" or "0 <what failed>"

One script (a copy per run, differing only in the picture names) is run three times by
gates.sh, all hidden, on the first GDI mission with its shroud live (no reveal): with the
copies kept (on.log, on_NN.png), with OPENCNC3D_SHROUD_CACHE=0 (off.log, off_NN.png) and
with =verify (verify.log, verify_NN.png). It stands still for eight frames, then ticks one
tick at a time with frames between while the construction yard drives into the shroud,
scrolls, reclassifies the shroud as unseen, lifts it, switches the rim off and on, puts the
shroud back, moves a fog dial, draws the cartridge look, restarts the mission and plays on.

Claims:
  (1) a still frame rebuilds nothing: no blanket, no fog field, no upload, no visibility,
      no blanket cell and no ground patch worked out;
  (2) while the world ticks the copies are rebuilt at most once a tick and at least once
      (the shroud did move), and reused on the frames between;
  (3) with the switch at 0 nothing is kept (the arm is what it says it is);
  (4) every picture is byte-identical across the three runs;
  (5) the compared pictures are not one picture: the drive, the reclassified shroud, the
      reveal, the rim, covering again and the restart each changed it;
  (6) verify compared every copy every frame and found no difference;
  (7) a change of the shroud works out only the blanket cells on screen: over the ticks,
      no more cells than the view holds per change, and the view is smaller than the
      blanket's whole range (so the claim could fail).
"""
import os
import re
import sys

SHOTS = 12


def script(d):
    lines = ["tick 10", "cam 54 50", "lclickobj MCV 0", "order 44 42", "shroudstat",
             "shot %s/NAME_01.png", "shroudcache reset", "frames 8", "shroudcache reset"]
    for _ in range(12):
        lines += ["tick 1", "frames 3"]
    lines += ["shroudcache reset", "shroudstat", "shot %s/NAME_02.png",
              "cam 50 46", "frames 2", "shot %s/NAME_03.png",
              "shrouddarktest", "shot %s/NAME_04.png", "tick 1", "shot %s/NAME_05.png",
              "reveal 1", "shot %s/NAME_06.png", "rim 0", "shot %s/NAME_07.png",
              "rim 1", "reveal 0", "shot %s/NAME_08.png",
              "gfx fow_falloff 2", "shot %s/NAME_09.png", "gfx fow_falloff 1",
              "gfxmode 1", "tick 2", "shot %s/NAME_10.png", "gfxmode 2",
              "remission", "tick 8", "shot %s/NAME_11.png", "tick 8", "frames 2", "shot %s/NAME_12.png",
              "shroudcache", "quit"]
    for arm in ("on", "off", "verify"):
        body = "\n".join(l.replace("%s", d).replace("NAME", arm) for l in lines) + "\n"
        open(os.path.join(d, arm + ".txt"), "w").write(body)


def text(path):
    try:
        return open(path, errors="replace").read()
    except OSError:
        return ""


def diff(pa, pb):
    from PIL import Image
    import numpy as np
    a = np.asarray(Image.open(pa).convert("RGB")).astype(int)
    b = np.asarray(Image.open(pb).convert("RGB")).astype(int)
    if a.shape != b.shape:
        return -1
    return int((abs(a - b).max(axis=2) > 0).sum())


STAT = re.compile(r"SHROUDCACHE\|stats\|mode=(\d)\|gen=(\d+)\|field=(\d+)/(\d+)\|blanket=(\d+)/(\d+)\|"
                  r"fog=(\d+)/(\d+)\|fogtex=(\d+)/(\d+)\|vis=(\d+)/(\d+)\|sigreset=(\d+)\|"
                  r"mottle=(\d+)/(\d+)\|checks=(\d+)\|mismatches=(\d+)\|"
                  r"blanketcells=(\d+)\|mottlepatches=(\d+)\|blanketview=(\d+)/(\d+)")
KEYS = ("mode gen fieldB fieldK blanketB blanketK fogB fogK texB texK visB visK sigreset "
        "mottleB mottleK checks bad cells patches view range").split()


def stats(log):
    return [dict(zip(KEYS, map(int, m))) for m in STAT.findall(log)]


def check(d):
    fails, claims = [], []
    j = lambda n: os.path.join(d, n)
    logs = {arm: text(j(arm + ".log")) for arm in ("on", "off", "verify")}
    st = {arm: stats(logs[arm]) for arm in logs}
    for arm in logs:
        if len(st[arm]) != 4:
            fails.append(f"the {arm} run printed {len(st[arm])} of 4 SHROUDCACHE lines")
    if fails:
        return fails, claims
    on, off, ver = st["on"], st["off"], st["verify"]

    # 1. eight still frames
    s = on[1]
    if (s["fieldB"] or s["blanketB"] or s["fogB"] or s["texB"] or s["visB"] or s["mottleB"]
            or s["cells"] or s["patches"]):
        fails.append(f"a still frame rebuilt something: {s}")
    elif s["blanketK"] < 16 or s["fogK"] < 8 or s["texK"] < 8 or s["visK"] < 8:
        fails.append(f"eight still frames did not reuse the blanket twice and the fog field once each: {s}")
    else:
        claims.append(f"8 still frames: 0 rebuilt and 0 cells worked out, blanket reused "
                      f"{s['blanketK']}x, fog field {s['fogK']}x and never sent")

    # 2. twelve ticks with three frames after each
    t = on[2]
    sh = re.findall(r"SHROUDSTAT\|[^\n]*\|clear=(\d+)", logs["on"])
    if len(sh) < 2 or int(sh[1]) <= int(sh[0]):
        fails.append("the construction yard uncovered nothing, so the shroud never moved")
    elif t["fieldB"] < 1 or t["blanketB"] < 1:
        fails.append(f"the shroud moved and nothing was rebuilt: {t}")
    elif t["fieldB"] > 12 or t["blanketB"] > 12 or t["texB"] > 12:
        fails.append(f"more rebuilds than ticks: {t}")
    elif t["blanketK"] < 2 * 36 - 2 * t["blanketB"] or t["texK"] < 36 - t["texB"]:
        fails.append(f"the frames between ticks did not reuse the copies: {t}")
    else:
        claims.append(f"12 ticks, 36 frames: field rebuilt {t['fieldB']}x (the cells uncovered "
                      f"{sh[0]} to {sh[1]}), fog field sent {t['texB']}x, kept {t['texK']}x")

    # 3. the switch at 0
    o = off[1]
    if o["mode"] != 0 or o["blanketK"] or o["fogK"] or o["texK"] or o["blanketB"] < 16 or o["texB"] < 8:
        fails.append(f"OPENCNC3D_SHROUD_CACHE=0 kept something or rebuilt too little: {o}")

    # 4. the pictures, byte for byte
    bad = []
    for k in range(1, SHOTS + 1):
        names = [j("%s_%02d.png" % (arm, k)) for arm in ("on", "off", "verify")]
        if not all(os.path.exists(n) for n in names):
            bad.append("%02d missing" % k)
            continue
        raw = [open(n, "rb").read() for n in names]
        if raw[0] != raw[1] or raw[0] != raw[2]:
            bad.append("%02d (%d, %d px)" % (k, diff(names[0], names[1]), diff(names[0], names[2])))
    if bad:
        fails.append("pictures differ between kept, rebuilt and verified: " + ", ".join(bad))
    else:
        claims.append(f"{SHOTS} pictures byte-identical kept, rebuilt every frame and verified")

    # 5. the steps changed the picture
    if not bad:
        steps = [("01", "02", "the drive"), ("03", "04", "the reclassified shroud"),
                 ("05", "06", "the reveal"), ("06", "07", "the rim"), ("07", "08", "covering again"),
                 ("02", "11", "the restart")]
        flat = [w for a, b, w in steps if diff(j("on_%s.png" % a), j("on_%s.png" % b)) < 500]
        if flat:
            fails.append("these steps did not change the picture, so identity proves nothing: " + ", ".join(flat))

    # 7. a change works out the cells on screen, not the whole range
    if t["view"] <= 0 or t["view"] >= t["range"]:
        fails.append(f"the view held {t['view']} of the blanket's {t['range']} cells, so the "
                     f"check below cannot tell a view from the whole range")
    elif t["cells"] < 1 or t["cells"] > t["blanketB"] * t["view"]:
        fails.append(f"{t['blanketB']} changes of the shroud worked out {t['cells']} blanket cells, "
                     f"more than the {t['view']} on screen each")
    else:
        claims.append(f"each change worked out at most the {t['view']} cells on screen of "
                      f"{t['range']} ({t['cells']} over {t['blanketB']} changes)")

    # 6. verify, summed over the run (each "reset" line starts a new count)
    checks, mism = sum(x["checks"] for x in ver), sum(x["bad"] for x in ver)
    if any(x["mode"] != 2 for x in ver) or mism or "SHROUDCACHE|verify|mismatch" in logs["verify"]:
        m = re.findall(r"SHROUDCACHE\|verify\|mismatch\|[^\n]*", logs["verify"])
        fails.append(f"verify found a stale copy ({mism}): {m[:3]}")
    elif checks < 500:
        fails.append(f"verify compared too little ({checks} comparisons)")
    else:
        claims.append(f"verify: {checks} comparisons, 0 differences")
    return fails, claims


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("script", "check"):
        print("0 usage: gate_shroudcache.py script|check DIR")
        sys.exit(0)
    if sys.argv[1] == "script":
        script(sys.argv[2])
        sys.exit(0)
    fails, claims = check(sys.argv[2])
    print(("0 " + "; ".join(fails)) if fails else ("1 " + "; ".join(claims)))
