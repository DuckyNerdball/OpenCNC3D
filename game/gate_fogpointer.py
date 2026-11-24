#!/usr/bin/env python3
"""The 3D pointer is not under the fog of war under Enhanced.

  gate_fogpointer.py script DIR       write DIR/td.txt and DIR/ra.txt
  gate_fogpointer.py check DIR [td]   read DIR/<leg>.log and the leg's four shots, print
                                      '1 <summary>' or '0 <what failed>'; "td" checks the
                                      Tiberian Dawn leg only (no Red Alert module staged)

THE RUNS, hidden, 1280 x 720, Enhanced, one per game, the pointer put on one HIDDEN cell
and shot four times: with the fog on, with the pointer and without it; then with the fog
switched off (fow_final 0, fow_noise 0: no mask at all), the same two.
  td   GDI's test map SCG90EA, cell 56,38 (unseen at tick 20), camera on 54,46
  ra   Allied 1, cell 60,75 (far south of the Allied start), camera on 60,72

THE CLAIMS, per leg, in a box round the pointer (110 px either side, 130 above, 90 below):
  the cell is HIDDEN (otherwise the leg tests nothing); with no fog the pointer lights at
  least MIN_LIT pixels by more than 40 levels; with the fog on it lights at least RATIO of
  that number (the glow fringe stays fogged by design); and outside the box the pictures
  with and without the pointer under the fog agree (the hole cut for the pointer clears
  nothing else).
"""
import os
import re
import sys

LEGS = {
    'td': ('56 38', '54 46'),
    'ra': ('60 75', '60 72'),
}
# Measured on the build that cuts the pointer out of the mask: Tiberian Dawn 20 pixels lit
# under the fog against 25 with none (0.800), Red Alert 70 against 95 (0.737), outside the
# box 1.0000 alike; on the build before, 0 lit under the fog in both games.
MIN_LIT = 15
RATIO = 0.5
SAME_OUTSIDE = 0.995


def write_scripts(d):
    os.makedirs(d, exist_ok=True)
    for leg, (cell, cam) in LEGS.items():
        p = os.path.join(d, leg)
        lines = ['gfx fow_final 1', 'gfx fow_noise 0.5', 'gfx fog_fx 0', 'tick 20',
                 'cam ' + cam, 'zoom min', 'shroudat ' + cell, 'cursorcell ' + cell,
                 'shot %s_on.png' % p, 'cursor -1 -1', 'shot %s_off.png' % p,
                 'gfx fow_final 0', 'gfx fow_noise 0', 'cursorcell ' + cell,
                 'shot %s_non.png' % p, 'cursor -1 -1', 'shot %s_noff.png' % p, 'quit']
        with open(p + '.txt', 'w') as f:
            f.write('\n'.join(lines) + '\n')


def measure(d, leg):
    import numpy as np
    from PIL import Image
    cell = LEGS[leg][0].replace(' ', ',')
    log = open(os.path.join(d, leg + '.log'), errors='replace').read()
    hidden = re.search(r'^SHROUDAT\|%s\|HIDDEN$' % cell, log, re.M) is not None
    m = re.search(r'^CURSORCELL\|cell=%s\|at=([0-9.]+),([0-9.]+)\|' % cell, log, re.M)
    if not m:
        return hidden, None
    cx, cy = int(float(m.group(1))), int(float(m.group(2)))
    L = lambda n: np.asarray(Image.open(os.path.join(d, '%s_%s.png' % (leg, n))).convert('RGB')).astype(int).max(axis=2)
    a, b, c, e = L('on'), L('off'), L('non'), L('noff')
    H, W = a.shape
    box = (slice(max(0, cy - 130), min(H, cy + 90)), slice(max(0, cx - 110), min(W, cx + 110)))
    lit_fog = int(((a[box] - b[box]) > 40).sum())
    lit_nofog = int(((c[box] - e[box]) > 40).sum())
    out = np.ones_like(a, bool)
    out[box] = False
    same = float((np.abs(a - b)[out] <= 2).mean())
    return hidden, (lit_fog, lit_nofog, same)


def check(d, td_only):
    bad, said = [], []
    for leg in ('td',) if td_only else ('td', 'ra'):
        name = 'Tiberian Dawn' if leg == 'td' else 'Red Alert'
        try:
            hidden, m = measure(d, leg)
        except OSError as e:
            bad.append('%s: %s' % (name, e))
            continue
        if not hidden:
            bad.append('%s: the cell is not HIDDEN, so nothing was tested' % name)
        if m is None:
            bad.append('%s: no CURSORCELL line, so the pointer was not placed' % name)
            continue
        lf, ln, same = m
        ratio = lf / max(1, ln)
        said.append('%s %d lit under the fog against %d with none (%.3f), outside %.4f'
                    % (name, lf, ln, ratio, same))
        if ln < MIN_LIT:
            bad.append('%s: with no fog the pointer lights %d pixels (want >=%d), so there is no pointer to judge'
                       % (name, ln, MIN_LIT))
        if ratio < RATIO:
            bad.append('%s: under the fog the pointer lights %d pixels against %d with none (%.3f, want >=%.2f;'
                       ' about 0 is the fog mask multiplying the pointer to black)' % (name, lf, ln, ratio, RATIO))
        if same < SAME_OUTSIDE:
            bad.append('%s: outside the pointer\'s box only %.4f of the picture is unchanged (want >=%.3f)'
                       % (name, same, SAME_OUTSIDE))
    if td_only:
        said.append('Red Alert not staged')
    return ('0 ' + '; '.join(bad)) if bad else ('1 ' + '; '.join(said))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'script':
        write_scripts(sys.argv[2])
    elif len(sys.argv) in (3, 4) and sys.argv[1] == 'check':
        print(check(sys.argv[2], len(sys.argv) == 4 and sys.argv[3] == 'td'))
    else:
        sys.exit(__doc__)
