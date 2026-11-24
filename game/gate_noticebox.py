#!/usr/bin/env python3
"""gate_noticebox.py PICTURE X0,Y0,X1,Y1 -- G406: is a notice box drawn in that rectangle?

The picture passes (first word BOX) when the rectangle the NOTICE line names holds a drawn
box: at least 5 % of its pixels lit and at least four distinct colours, so a black frame,
a flat fill or a lone frame line does not pass. The 1995 dialogs draw the notice over
black inside that rectangle; the Enhanced menus draw their own larger box over the menu's
art, so the rectangle (the 1995 layout's) holds its text and button but the frame around
it is not black, and the lit share outside is printed, not judged. Measured on the Mac:
the 1995 notices light 10 to 12 % of the rectangle with 8 to 18 colours, the Enhanced
ones 31 to 50 % with over 60, a black frame 0 % with one. Any other outcome prints NOBOX or
NOPICTURE with the numbers, for the gate's message.
"""
import sys


def main():
    try:
        from PIL import Image
        im = Image.open(sys.argv[1]).convert('RGB')
        x0, y0, x1, y1 = (int(v) for v in sys.argv[2].split(','))
    except Exception as e:  # a missing picture or rectangle is the gate's answer, not a crash
        print('NOPICTURE %s' % type(e).__name__)
        return
    w, h = im.size
    data = im.load()
    inside = lit = outside = olit = 0
    colours = set()
    for y in range(h):
        for x in range(w):
            r, g, b = data[x, y]
            on = r + g + b > 60
            if x0 <= x <= x1 and y0 <= y <= y1:
                inside += 1
                lit += on
                colours.add((r >> 4, g >> 4, b >> 4))
            else:
                outside += 1
                olit += on
    fi = lit / float(inside or 1)
    fo = olit / float(outside or 1)
    verdict = 'BOX' if inside and fi >= 0.05 and len(colours) >= 4 else 'NOBOX'
    print('%s lit-inside=%.3f colours-inside=%d lit-outside=%.4f size=%dx%d'
          % (verdict, fi, len(colours), fo, w, h))


if __name__ == '__main__':
    main()
