#!/usr/bin/env python3
"""Make a saved F5 foliage tuning the built-in default for experimental art.

The F5 panel's SAVE writes every dial to opencnc3d-fx.cfg beside the game, and the game reads
that file back at start, so a saved tuning already survives a restart in that folder. It
does not survive into the next build: a fresh folder has no opencnc3d-fx.cfg and starts from
the compiled defaults. This copies the BUSHES AND FOLIAGE dials (every key beginning
fol_) from a saved file into the block between the FOLIAGE DEFAULTS markers in
fx_defaults (game/fx_state.h), so every later build starts from them.

Only that block is touched, and only keys already in it, so a saved file cannot move
any other default by accident. A fol_ key in the file that the block does not carry is
reported rather than invented.

Usage:
    python3 tools/fol_defaults.py path/to/opencnc3d-fx.cfg          # show what would change
    python3 tools/fol_defaults.py path/to/opencnc3d-fx.cfg --write  # change it
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
STATE = os.path.join(HERE, '..', 'game', 'fx_state.h')
BEGIN = '/* FOLIAGE DEFAULTS BEGIN */'
END = '/* FOLIAGE DEFAULTS END */'


def read_cfg(path):
    vals = {}
    for line in open(path, errors='replace'):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        f = line.split()
        if len(f) >= 2 and f[0].startswith('fol_'):
            vals[f[0]] = f[1]
    return vals


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    write = '--write' in sys.argv
    if len(args) != 1:
        sys.exit(__doc__)
    saved = read_cfg(args[0])
    if not saved:
        sys.exit('%s holds no fol_ keys; was it saved from a build with the foliage '
                 'group?' % args[0])
    src = open(STATE).read()
    a, b = src.find(BEGIN), src.find(END)
    if a < 0 or b < 0 or b < a:
        sys.exit('the FOLIAGE DEFAULTS markers are missing from game/fx_state.h')
    block = src[a:b]
    changed, seen = [], set()

    def repl(m):
        key, old = m.group(2), m.group(4)
        seen.add(key)
        if key not in saved:
            return m.group(0)
        v = saved[key]
        if old.endswith('f'):
            new = ('%.5f' % float(v)).rstrip('0')
            if new.endswith('.'):
                new += '0'
            new += 'f'
        else:
            new = str(int(float(v)))
        if new != old and float(new.rstrip('f')) != float(old.rstrip('f')):
            changed.append((key, old, new))
            return m.group(1) + m.group(2) + m.group(3) + new + ';'
        return m.group(0)

    block2 = re.sub(r'(\s*s->)(fol_[a-z0-9_]+)(\s*=\s*)([-0-9.]+f?);', repl, block)
    for key in sorted(set(saved) - seen):
        print('not in the defaults block, left alone: %s %s' % (key, saved[key]))
    if not changed:
        print('the saved foliage dials already match the built-in defaults')
        return
    for key, old, new in changed:
        print('%-18s %-10s -> %s' % (key, old, new))
    if write:
        open(STATE, 'w').write(src[:a] + block2 + src[b:])
        print('wrote %d default(s) into game/fx_state.h' % len(changed))
    else:
        print('(dry run: pass --write to change game/fx_state.h)')


if __name__ == '__main__':
    main()
