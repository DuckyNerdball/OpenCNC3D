#!/usr/bin/env python3
"""The match debrief's eyes (G480, G527): read the Enhanced TOP SCORES screen off its picture.

usage: gate_debrief.py PICTURE X,Y,W,H
       gate_debrief.py pages FOLDER X,Y,W,H     (every page; see pages() below)

X,Y,W,H is where the CONTINUE button landed in the window, as the screen logs it
(DEBRIEF|shown|...|continue=X,Y,W,H). The button stands at (758, 550) of the dialog and is
220 dialog pixels wide (em_match_debrief), so it places the dialog in the picture whatever
the window's size and scale. Prints one line of key=value pairs:

  scale      window pixels per dialog pixel
  rowfill    the colour this machine's row is filled with, at dialog (240, 231), inside the
             COMMANDER well's first row and clear of its text
  field      the frame's own field just inside its left rail, at dialog (31, 70): green-grey
             in Tiberian Dawn's kit, red in Red Alert's title kit
  result     lit pixels of the bright body ink (Tiberian Dawn e6e5da) on the result line,
             dialog y 536..553, x 38..739
  formula    pixels of the muted ink (aeb5ac) on the line under it, dialog y 588..605, where
             the sum of the score is spelt out when there is one
  formula_x0 formula_x1   that ink's first and last column, in dialog pixels (-1 when none);
             the frame's own inner well ends past dialog x 960, so a line ending
             before it stands inside the frame
"""
import sys
from PIL import Image
import numpy as np

ox = oy = s = 0.0
im = None


def px(x, y):
    return im[int(round(oy + (y + 0.5) * s)), int(round(ox + (x + 0.5) * s))]


def box(x0, y0, x1, y1):
    return im[int(round(oy + y0 * s)):int(round(oy + y1 * s)),
              int(round(ox + x0 * s)):int(round(ox + x1 * s)), :]


def near(a, col, tol=28):
    c = np.array([(col >> 16) & 255, (col >> 8) & 255, col & 255])
    return (np.abs(a - c).max(axis=2) <= tol)


def hexof(p):
    return "%02x%02x%02x" % (p[0], p[1], p[2])


def pages(folder, btn, base="debrief"):
    """`pages` mode: every page picture the debrief wrote beside `base` (OPENCNC3D_DEBRIEF_PAGES),
    how many differ from one another in the dialog's content area (x 38..978, y 130..518),
    and on the graph's first series the pixels of each Tiberian Dawn player colour in the
    plot (x 129..952, y 230..462), the local commander's first."""
    import os
    global ox, oy, s
    names = ["summary", "score", "units", "structures", "economy"] + ["graph-%d" % k for k in range(6)]
    got = []
    for n in names:
        p = os.path.join(folder, "%s-%s.png" % (base, n))
        if not os.path.exists(p):
            print("missing=%s" % n)
            return
        got.append(np.asarray(Image.open(p).convert("RGB")).astype(int))
    bx, by, bw, bh = (int(v) for v in btn.split(","))
    s = bw / 220.0
    ox, oy = bx - 758 * s, by - 550 * s

    def area(a, x0, y0, x1, y1):
        return a[int(round(oy + y0 * s)):int(round(oy + y1 * s)), int(round(ox + x0 * s)):int(round(ox + x1 * s)), :]

    content = [area(a, 38, 130, 978, 518) for a in got]
    distinct = 0
    for i in range(len(content)):
        if all(np.abs(content[i] - content[j]).max() > 0 for j in range(len(content)) if j != i):
            distinct += 1
    plot = area(got[5], 129, 230, 953, 463)
    td = [0xc4a85c, 0xc0c0d0, 0xc42814, 0xa0e01c, 0xd47810, 0x648088, 0xa0a0a0, 0xa8704c]
    counts = []
    for c in td:
        col = np.array([(c >> 16) & 255, (c >> 8) & 255, c & 255])
        counts.append(int((np.abs(plot - col).max(axis=2) <= 6).sum() / (s * s)))
    print("pages=%d distinct=%d lines=%s" % (len(got), distinct, ",".join(str(c) for c in counts)))


def one():
    global im, ox, oy, s
    im = np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(int)
    bx, by, bw, bh = (int(v) for v in sys.argv[2].split(","))
    s = bw / 220.0
    ox, oy = bx - 758 * s, by - 550 * s
    res = near(box(38, 536, 740, 554), 0xe6e5da)
    form = near(box(38, 588, 740, 606), 0xaeb5ac)
    cols = np.where(form.any(axis=0))[0]
    fx0 = int(cols[0] / s) + 38 if len(cols) else -1
    fx1 = int(cols[-1] / s) + 38 if len(cols) else -1
    area = s * s
    print("scale=%.3f rowfill=%s field=%s result=%d formula=%d formula_x0=%d formula_x1=%d" % (
        s, hexof(px(240, 231)), hexof(px(31, 70)), int(res.sum() / area), int(form.sum() / area),
        fx0, fx1))


if len(sys.argv) > 3 and sys.argv[1] == "pages":
    pages(sys.argv[2], sys.argv[3])
else:
    one()
