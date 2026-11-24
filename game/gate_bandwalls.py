#!/usr/bin/env python3
"""Band walls against the band's own tested rectangle, on rendered pixels.

usage: gate_bandwalls.py PLAIN.png BAND.png C0 R0 C1 R1 XMAX

PLAIN and BAND are two shots of one frame, the second with the band up and nothing else
changed. Every pixel that differs by more than a small threshold is the band's. The walls
stand on the ground round the tested screen rectangle and rise a little above it, and
under perspective a wall standing at a side edge leans outwards as it rises, so a wall
pixel is allowed within TOP pixels above the rectangle, SIDE pixels either side of it and
BOTTOM pixels below it; anything further out was drawn round ground the selection never
tests. Columns at or right of XMAX (the sidebar) are not counted. Prints
    BANDWALLS|changed=N|outside=M|far=F
where far counts pixels more than 100 px from the rectangle.
"""
import sys
from PIL import Image, ImageChops

TOP, SIDE, BOTTOM, THRESH = 40, 20, 6, 24

def main():
    a = Image.open(sys.argv[1]).convert('RGB')
    b = Image.open(sys.argv[2]).convert('RGB')
    c0, r0, c1, r1, xmax = [int(float(v)) for v in sys.argv[3:8]]
    if c1 < c0: c0, c1 = c1, c0
    if r1 < r0: r0, r1 = r1, r0
    d = ImageChops.difference(a, b).convert('L')
    px = d.load()
    w, h = d.size
    changed = outside = far = 0
    for y in range(h):
        for x in range(min(w, xmax)):
            if px[x, y] <= THRESH:
                continue
            changed += 1
            if c0 - SIDE <= x <= c1 + SIDE and r0 - TOP <= y <= r1 + BOTTOM:
                continue
            outside += 1
            if not (c0 - 100 <= x <= c1 + 100 and r0 - 100 <= y <= r1 + 100):
                far += 1
    print('BANDWALLS|changed=%d|outside=%d|far=%d' % (changed, outside, far))

if __name__ == '__main__':
    main()
