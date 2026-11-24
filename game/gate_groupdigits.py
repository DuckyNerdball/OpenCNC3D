#!/usr/bin/env python3
"""Red Alert's control-group digit on a selected unit, in every graphics mode.

  gate_groupdigits.py script DIR   write DIR/ra.txt (Allied 1) and DIR/td.txt (GDI's test map)
  gate_groupdigits.py check DIR    read DIR/ra.log, DIR/td.log and the six shots, print
                                   '1 <summary>' or '0 <what failed>'

THE RUNS, hidden, 1280 x 720, the map revealed:
  ra   Allied 1. A Ranger selected in no group; put in group 1 and deselected; selected
       again and shot in N64, Enhanced and Classic, each frame twice, with the digits on and
       with `groupdigits 0`; then a second Ranger, in no group, selected alone.
  td   Tiberian Dawn's test map: a Medium Tank put in group 1 and selected.

THE CLAIMS:
  engine   the Ranger in no group draws no digit; in group 1 and not selected, none; selected
           again, exactly one, digit 1, at the DOS bar's size (zoom 2.0 at 720 rows) in N64
           and Enhanced, and in Classic a box twice the frame's 34 x 8; the other Ranger
           draws none; the script ran without a failure.
  pixels   in each mode, inside the glyph's own box (the frame's columns 11..16 and rows
           1..7, scaled by the zoom), the shot with the digits on differs from the same
           frame without them, and pixels that were not white turn white (the selection
           bracket's own white corner crosses that box in the 3D modes and is not counted):
           the digit was drawn, not only counted.
  td       the tank is grouped and selected, and Tiberian Dawn draws no digit and loads no
           digit art, because 1995's Tiberian Dawn never drew a group number.
"""
import os
import re
import sys

RA = """tick 30
gfxmode 1
tick 5
cam 62 50
tick 1
lclickobj JEEP 0
expectsel 1
echo GRP-0
groupdump
group 1 2
tick 1
deselect
echo GRP-1
groupdump
lclickobj JEEP 0
expectsel 1
echo GRP-N64
groupdump
shot {d}/n64_on.png
groupdigits 0
shot {d}/n64_off.png
groupdigits 1
gfxmode 2
tick 1
echo GRP-ENH
groupdump
shot {d}/enh_on.png
groupdigits 0
shot {d}/enh_off.png
groupdigits 1
gfxmode 0
tick 1
shot {d}/cls_on.png
echo GRP-CLS
groupdump
groupdigits 0
shot {d}/cls_off.png
groupdigits 1
lclickobj JEEP 1
shot {d}/cls_j1.png
echo GRP-2
groupdump
quit
"""

TD = """tick 20
cam 54 51
zoom min
lclickobj MTNK 0
group 1 2
tick 1
groupdump
quit
"""

# Measured on the build that draws the digit, inside one glyph box: N64 93 pixels changed
# and 30 turned white, Enhanced 112 and 30, Classic 120 and 32. With the digit drawn nowhere
# the on and off frames are the same picture, 0 and 0 in all three.
MIN_DIFF = 40
MIN_WHITE = 12
MODES = (('GRP-N64', 'n64', '2.0'), ('GRP-ENH', 'enh', '2.0'), ('GRP-CLS', 'cls', None))


def write_scripts(d):
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'ra.txt'), 'w') as f:
        f.write(RA.format(d=d))
    with open(os.path.join(d, 'td.txt'), 'w') as f:
        f.write(TD)


def segment(log, tag):
    """The lines after ECHO|tag, up to the next ECHO line or the end of the log."""
    m = re.search(r'^ECHO\|' + re.escape(tag) + r'$(.*?)(?=^ECHO\||\Z)', log, re.S | re.M)
    return m.group(1) if m else ''


def glyph(d, mode, rect, zoom):
    from PIL import Image
    a = Image.open(os.path.join(d, mode + '_on.png')).convert('RGB')
    b = Image.open(os.path.join(d, mode + '_off.png')).convert('RGB')
    pa, pb = a.load(), b.load()
    x0, y0 = rect[0], rect[1]
    diff = white = 0
    for y in range(int(y0 + 1 * zoom), int(y0 + 8 * zoom)):
        for x in range(int(x0 + 11 * zoom), int(x0 + 17 * zoom)):
            if not (0 <= x < a.width and 0 <= y < a.height):
                continue
            ca, cb = pa[x, y], pb[x, y]
            if max(abs(ca[k] - cb[k]) for k in range(3)) > 8:
                diff += 1
            if min(ca) >= 200 and min(cb) < 200:
                white += 1
    return diff, white


def check(d):
    bad, said = [], []
    try:
        ra = open(os.path.join(d, 'ra.log'), errors='replace').read()
        td = open(os.path.join(d, 'td.log'), errors='replace').read()
    except OSError as e:
        return '0 a run left no log (%s)' % e
    begin = r'^GROUPDUMP-BEGIN mode=%s atlas=1 frame=34x8 count=%d$'
    if not re.search(begin % ('3d', 0), segment(ra, 'GRP-0'), re.M):
        bad.append('a Ranger in no group draws a digit')
    s1 = segment(ra, 'GRP-1')
    if not re.search(begin % ('3d', 0), s1, re.M) or not re.search(r'^GROUP-OBJ\|JEEP\|.*\|group=0\|sel=0$', s1, re.M):
        bad.append('the Ranger was not put in group 1, or draws a digit while not selected')
    for tag, mode, zoom in MODES:
        seg = segment(ra, tag)
        if not re.search(begin % ('classic' if zoom is None else '3d', 1), seg, re.M):
            bad.append('%s: not exactly one digit' % mode)
            continue
        m = re.search(r'^GROUP-DIGIT\|JEEP\|id=\d+\|group=0\|digit=1\|(?:zoom=([0-9.]+)\|)?'
                      r'rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)$', seg, re.M)
        if not m:
            bad.append('%s: no digit 1 on the Ranger' % mode)
            continue
        rect = [int(v) for v in m.groups()[1:]]
        z = (rect[2] - rect[0]) / 34.0
        if zoom is not None and m.group(1) != zoom:
            bad.append('%s: the digit is drawn at zoom %s (want %s, the DOS bar\'s size)' % (mode, m.group(1), zoom))
        if abs(z - 2.0) > 0.01 or rect[3] - rect[1] != 16:
            bad.append('%s: the digit\'s box is %dx%d (want 68x16)' % (mode, rect[2] - rect[0], rect[3] - rect[1]))
        diff, white = glyph(d, mode, rect, z)
        said.append('%s diff %d white %d' % (mode, diff, white))
        if diff < MIN_DIFF or white < MIN_WHITE:
            bad.append('%s: the glyph box differs in %d pixels (want >=%d) and holds %d white (want >=%d)'
                       % (mode, diff, MIN_DIFF, white, MIN_WHITE))
    if not re.search(begin % ('classic', 0), segment(ra, 'GRP-2'), re.M):
        bad.append('another Ranger in no group draws a digit')
    if not re.search(r'^SCRIPT\|end .*ra\.txt: \d+ lines, 0 failures', ra, re.M):
        bad.append('the Red Alert script did not run clean')
    if not re.search(r'^GROUPDUMP-BEGIN mode=3d atlas=0 frame=0x0 count=0$', td, re.M) \
            or not re.search(r'^GROUP-OBJ\|MTNK\|.*\|group=0\|sel=1$', td, re.M):
        bad.append('Tiberian Dawn: the tank is not grouped and selected, or a digit is drawn')
    if bad:
        return '0 ' + '; '.join(bad)
    return ('1 a Ranger put in group 1 and selected again wears a 1 at the DOS bar\'s size in N64, '
            'Enhanced and Classic (%s); none in no group, none unselected, none on another Ranger, '
            'none in Tiberian Dawn' % ', '.join(said))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'script':
        write_scripts(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == 'check':
        print(check(sys.argv[2]))
    else:
        sys.exit(__doc__)
