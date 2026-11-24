#!/usr/bin/env python3
"""Red Alert's mission map shows the attack pointer over a target.

  gate_ramappointer.py check DIR PACK   read DIR/flow.log, DIR/flow_ra_map.png and
                                        DIR/flow_ra_map_attack.png and the cursor pack the
                                        map screen draws from; print '1 <summary>' or
                                        '0 <what failed>'

THE RUN is the hands-off campaign flow on the Allied side (--flowtest): after Allied 1 is
won the map screen comes up, the autopilot parks the pointer, shoots the screen
(flow_ra_map.png), moves the pointer onto the next mission's first target, waits ten ticks,
shoots again (flow_ra_map_attack.png) and logs where, which MOUSE.SHP frame and which hot
spot it drew.

THE CLAIMS:
  log      the map screen switched the pointer to the attack shape over target 0, and the
           shot names an attack frame (MOUSE_CAN_ATTACK, frames 21..28) with its hot spot
           14,11 (redalert/mapsel.cpp and Set_Mouse_Cursor);
  pixels   the plate is 320 x 200 at whole-number scale 3 in a 1280 x 720 window, offset
           160,60. At the logged place, less the hot spot, at least half of that frame's
           opaque pixels differ between the two shots (an outline pixel over ground of the
           same index cannot), and at least 98 percent of the frame's clear pixels do not;
           and at least half of the arrow's opaque pixels at its parking place (80,100,
           frame 0) differ, so the arrow is gone rather than drawn beside the new shape.
"""
import os
import re
import struct
import sys

# Measured on the build that draws the attack pointer: frame 24 at 178,116, hot spot 14,11;
# 230 of the frame's 231 opaque pixels changed and all of its clear pixels unchanged; 69 of
# the arrow's 89 opaque pixels changed (the rest are its outline over ground of the same
# colour). The build before logs no pointer line and shoots no attack picture.
MIN_OPAQUE = 50


def check(d, pack):
    from PIL import Image
    try:
        log = open(os.path.join(d, 'flow.log'), errors='replace').read()
    except OSError as e:
        return '0 the flow left no log (%s)' % e
    if not re.search(r'^CAMPAIGN\|ra-map\|pointer=attack\|spot=0$', log, re.M):
        return '0 the map screen never turned the pointer to the attack shape over target 0'
    m = re.search(r'^CAMPAIGN\|ra-map\|shot=flow_ra_map_attack\.png\|at=(\d+),(\d+)\|frame=(\d+)'
                  r'\|hotspot=(\d+),(\d+)$', log, re.M)
    if not m:
        return '0 the map screen logged no shot with the pointer over the target'
    ax, ay, frame, hx, hy = map(int, m.groups())
    if not 21 <= frame <= 28 or (hx, hy) != (14, 11):
        return '0 the pointer over the target is frame %d with hot spot %d,%d (want 21..28 and 14,11)' % (frame, hx, hy)
    data = open(pack, 'rb').read()
    if data[:8] != b'DOSBAR01':
        return '0 %s is not a DOSBAR01 pack' % pack
    off = 8 + 16 + 768 + 768 + 512
    frames, w, h = struct.unpack_from('<III', data, off + 12)
    if data[off:off + 5] != b'MOUSE' or frames != 222 or (w, h) != (30, 24):
        return '0 %s does not hold the 222 frames of MOUSE.SHP first' % pack
    px = data[off + 24:]
    try:
        P = Image.open(os.path.join(d, 'flow_ra_map.png')).convert('RGB')
        A = Image.open(os.path.join(d, 'flow_ra_map_attack.png')).convert('RGB')
    except OSError as e:
        return '0 a map screen shot is missing (%s)' % e
    if P.size != (1280, 720) or A.size != (1280, 720):
        return '0 the map screen shots are %s and %s (want 1280x720)' % (P.size, A.size)
    win = lambda x, y: (160 + 3 * x + 1, 60 + 3 * y + 1)

    def compare(f, ox, oy):
        on = on_diff = off_n = off_same = 0
        for y in range(h):
            for x in range(w):
                X, Y = ox + x, oy + y
                if not (0 <= X < 320 and 0 <= Y < 200):
                    continue
                differs = P.getpixel(win(X, Y)) != A.getpixel(win(X, Y))
                if px[f * w * h + y * w + x]:
                    on += 1
                    on_diff += differs
                else:
                    off_n += 1
                    off_same += not differs
        return on, on_diff, off_n, off_same

    on, dif, clear, same = compare(frame, ax - hx, ay - hy)
    on0, dif0, _, _ = compare(0, 80, 100)
    bad = []
    if on < MIN_OPAQUE or dif < 0.5 * on:
        bad.append('the attack frame is not drawn there (%d of its %d opaque pixels changed)' % (dif, on))
    if same < 0.98 * clear:
        bad.append('pixels changed outside the attack frame\'s shape (%d of %d clear pixels unchanged)' % (same, clear))
    if on0 < 20 or dif0 < 0.5 * on0:
        bad.append('the arrow is still at its parking place (%d of its %d opaque pixels changed)' % (dif0, on0))
    if bad:
        return '0 ' + '; '.join(bad)
    return ('1 over target 0 the pointer is MOUSE.SHP frame %d at %d,%d with hot spot %d,%d: %d of its %d '
            'opaque pixels drawn, %.1f%% of its clear pixels unchanged, and the parked arrow gone '
            '(%d of %d changed)' % (frame, ax, ay, hx, hy, dif, on, 100.0 * same / max(1, clear), dif0, on0))


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == 'check':
        print(check(sys.argv[2], sys.argv[3]))
    else:
        sys.exit(__doc__)
