#!/usr/bin/env python3
"""
G321: four things Classic (the 1995 MS-DOS look) drew wrongly, checked on rendered pixels.

    python3 gate_classic_ui.py UI_LOG DESERT_LOG SHOTS_DIR PACK

UI_LOG and DESERT_LOG are the runs of game/gate_classic_ui.txt (a skirmish) and
game/gate_classic_desert.txt (SCG41EA); PACK is the run folder's dosclassic.pack, whose
TEMPERAT.PAL and DESERT.PAL are the colours every check below is measured in.

  selection  A skirmish player is a Multi house. Selecting the MCV must add the white
             corner brackets: the frame with it selected differs from the one without
             by at least 100 pixels, 50 of them WHITE (palette index 15).
  band       The rubber band is DisplayClass::Draw_It's Draw_Rect in WHITE, one 1995
             pixel wide: every pixel the band adds is WHITE, its left and right edges are
             equally thick and more than one screen pixel thick (one game pixel at the
             window's integer scale), and it spans the dragged rectangle.
  desert     Every sprite on a Desert map decodes through DESERT.PAL: no pixel of the
             tactical view is a colour only Temperate's palette has (the Temperate
             palette turned Desert art blue and lavender), and Desert's own colours are
             there (at least 10000 pixels of colours only DESERT.PAL has).
  gunfire    A GUNFIRE muzzle flash goes through TranslucentTable, which makes its white
             rim a faint shade of the ground: inside every flash on screen, no pixel is
             WHITE, and the flame itself is drawn (at least 30 pixels).

Prints one line per check and a verdict line; exits 0 only if every check passed.
"""

import os
import sys

import numpy as np
from PIL import Image

bad = []


def check(ok, msg):
    print(("ok   " if ok else "BAD  ") + msg)
    if not ok:
        bad.append(msg)
    return ok


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.int64)


def key(a):
    return a[..., 0] * 65536 + a[..., 1] * 256 + a[..., 2]


def palettes(pack):
    data = open(pack, "rb").read()
    if data[:8] != b"DOS2D003":
        return None
    off = 8 + 4 + 8 * 16
    return [np.frombuffer(data[off + i * 768:off + (i + 1) * 768], np.uint8)
            .astype(np.int64).reshape(256, 3) << 2 for i in range(2)]


def leg_selection_band(shots, white):
    sel, nosel = load(os.path.join(shots, "g321_sel.png")), load(os.path.join(shots, "g321_nosel.png"))
    d = np.any(sel != nosel, axis=2)
    n_white = int((d & (key(sel) == white)).sum())
    check(d.sum() >= 100 and n_white >= 50,
          "selection: selecting the skirmish MCV adds %d pixels, %d of them WHITE"
          " (want >= 100 and >= 50)" % (int(d.sum()), n_white))
    band, noband = load(os.path.join(shots, "g321_band.png")), load(os.path.join(shots, "g321_noband.png"))
    d = np.any(band != noband, axis=2)
    n = int(d.sum())
    n_white = int((d & (key(band) == white)).sum())
    if not check(n >= 1000 and n_white == n,
                 "band: it adds %d pixels, %d of them WHITE (want >= 1000, all)" % (n, n_white)):
        return
    ys, xs = np.nonzero(d)
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    mid = (y0 + y1) // 2
    row = d[mid, x0:x1 + 1]
    left = int(np.argmin(row)) if not row.all() else 0
    right = int(np.argmin(row[::-1])) if not row.all() else 0
    check(left >= 2 and left == right,
          "band: its left and right edges are %d and %d screen pixels thick"
          " (want equal and >= 2, one 1995 pixel)" % (left, right))
    check(abs(x0 - 200) <= 8 and abs(y0 - 200) <= 8 and abs(x1 - 900) <= 8 and abs(y1 - 700) <= 8,
          "band: it spans %d,%d..%d,%d for a drag over 200,200..900,700 (want within"
          " one 1995 pixel)" % (x0, y0, x1, y1))


def probes(path):
    """[(shot index, [GUNFIRE rects])] for each dosanimprobe in the log, in order."""
    out = []
    for line in open(path, errors="replace"):
        if line.startswith("DOSANIM|frame|"):
            out.append([])
        elif line.startswith("DOSANIM|GUNFIRE|") and out:
            f = dict(kv.split("=", 1) for kv in line.strip().split("|")[2:])
            out[-1].append([int(round(float(v))) for v in f["rect"].split(",")])
    return out


def leg_desert(desert_log, shots, T, D):
    img = load(os.path.join(shots, "g321_desert.png"))
    view = img[:, :img.shape[1] * 3 // 4]          # the tactical view, not the sidebar
    k = key(view)
    t_only = np.array(sorted(set(key(T)) - set(key(D))))
    d_only = np.array(sorted(set(key(D)) - set(key(T))))
    n_t, n_d = int(np.isin(k, t_only).sum()), int(np.isin(k, d_only).sum())
    check(n_t == 0, "desert: %d pixels of the Nod base are colours only TEMPERAT.PAL has"
          " (want 0)" % n_t)
    check(n_d >= 10000, "desert: %d pixels are colours only DESERT.PAL has (want >= 10000)" % n_d)

    white = int(key(D[15]))
    flame = np.array(sorted(set(key(D[17:32]))))   # the flash's own orange ramp
    blocks = probes(desert_log)
    seen = 0
    for i, rects in enumerate(blocks):
        path = os.path.join(shots, "g321_gf_%02d.png" % (i + 1))
        if not rects or not os.path.exists(path):
            continue
        shot = load(path)
        H, W = shot.shape[:2]
        for x0, y0, x1, y1 in rects:
            if x0 < 0 or y0 < 0 or x1 > W * 3 // 4 or y1 > H:
                continue
            box = key(shot[y0:y1, x0:x1])
            n_white, n_flame = int((box == white).sum()), int(np.isin(box, flame).sum())
            seen += 1
            check(n_white == 0 and n_flame >= 30,
                  "gunfire: the flash in shot %02d at %d,%d holds %d WHITE pixels and %d of"
                  " its flame (want 0 and >= 30)" % (i + 1, x0, y0, n_white, n_flame))
    check(seen >= 1, "gunfire: at least one GUNFIRE flash wholly on screen was measured (%d)" % seen)


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    pals = palettes(sys.argv[4])
    if not check(pals is not None, "the pack carries the theater palettes (DOS2D003)"):
        print("verdict: FAIL, 1 check(s)")
        sys.exit(1)
    T, D = pals
    leg_selection_band(sys.argv[3], int(key(T[15])))
    leg_desert(sys.argv[2], sys.argv[3], T, D)
    print("verdict: %s" % ("PASS" if not bad else "FAIL, %d check(s)" % len(bad)))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
