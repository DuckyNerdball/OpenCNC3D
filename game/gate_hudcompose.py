#!/usr/bin/env python3
"""G383: the HUD and the sidebar are composed only when something they show changed.

  gate_hudcompose.py script DIR   write DIR/{ra,td}_{v,a}.txt
  gate_hudcompose.py check DIR    read DIR/{ra,td}_{v,a}.log and the shots; print one line,
                                  "1 <message>" or "0 <reason>"

The same script is run twice per game: once with OPENCNC3D_HUD_COMPOSE=verify, which takes
exactly the skip and patch decisions the default takes and also composes every skipped or
patched surface from scratch and compares it byte for byte (DIR/<game>_v.log), and once
with OPENCNC3D_HUD_COMPOSE=0, which composes every surface on every frame as the renderer
did before (DIR/<game>_a.log). The scripts walk the states a HUD can change in: a still
window first, then hover and press, a tab switch, production with the clock at several
sub-tick phases, a busy factory, a hold, scrolling, the tools, power forced red and back,
a selection on the unit card, the radar off, on and moving with the camera, the drawer
sliding, the mission timer ticking, and Classic, N64 and back to Enhanced mid-mission.

Claims:
  1. a still frame composes nothing: across ten still frames the composed count does not
     move and the skipped count grows by at least one per surface per frame;
  2. verify reports no difference anywhere in the run, and verified at least 100 times;
  3. Red Alert's bar was patched by cell and by radar at least once each, so the
     partial redraws were exercised and checked;
  4. the compose-always run really composed every frame (mode 0, nothing skipped);
  5. every shot of the verify run is byte-identical to the same shot of the
     compose-always run.
"""
import hashlib
import os
import re
import sys

RA = ['gfxmode 2', 'hud 1', 'tick 30', 'smoothmove 1', 'tick 1', 'tickalpha 0', 'frames 2',
      'meshperfdump', 'frames 10', 'meshperfdump', 'shot {d}/ra_01.png',
      'cursor 1134 306', 'shot {d}/ra_02.png', 'cursor 1141 244', 'shot {d}/ra_03.png',
      'sbhold 1141 244', 'shot {d}/ra_04.png', 'sbrelease',
      'sbcategory infantry', 'cursor 600 400', 'shot {d}/ra_05.png',
      'build E1', 'tick 2', 'shot {d}/ra_06.png',
      'tickalpha 0.5', 'frames 1', 'shot {d}/ra_07.png', 'tickalpha 0',
      'build E3', 'tick 2', 'shot {d}/ra_08.png',
      'hold E1', 'tick 1', 'shot {d}/ra_09.png',
      'tickalpha 0.25', 'frames 1', 'shot {d}/ra_10.png', 'tickalpha 0',
      'build E1', 'tick 3', 'cursor 1134 306', 'shot {d}/ra_11.png',
      'sbcategory structures', 'sbclick 1249 668', 'cursor 600 400', 'shot {d}/ra_12.png',
      'sbhold 1249 668', 'shot {d}/ra_13.png', 'sbrelease',
      'cursor 1027 69', 'shot {d}/ra_14.png', 'sbtool repair', 'shot {d}/ra_15.png', 'sbtool repair',
      'cursor 600 400',
      'powerforce 200 500', 'frames 1', 'shot {d}/ra_16.png', 'powerforce off', 'tick 1', 'shot {d}/ra_17.png',
      'band 0 0 1000 700', 'frames 1', 'shot {d}/ra_18.png',
      'cardclick mini 1', 'frames 1', 'shot {d}/ra_19.png', 'deselect', 'frames 1',
      'minimapoff', 'frames 1', 'shot {d}/ra_20.png', 'minimapon', 'frames 1', 'shot {d}/ra_21.png',
      'cam 60 60', 'frames 1', 'shot {d}/ra_22.png', 'tick 15', 'shot {d}/ra_23.png',
      'sbslide hide', 'frames 3', 'shot {d}/ra_24.png', 'sbslide show', 'frames 40', 'shot {d}/ra_25.png',
      'gfxmode 0', 'frames 1', 'shot {d}/ra_26.png', 'tick 15', 'shot {d}/ra_27.png',
      'gfxmode 1', 'frames 1', 'shot {d}/ra_28.png', 'tick 15', 'shot {d}/ra_29.png',
      'gfxmode 2', 'hud 1', 'frames 1', 'shot {d}/ra_30.png', 'tick 5', 'shot {d}/ra_31.png',
      'meshperfdump', 'quit']

TD = ['gfxmode 2', 'hud 1', 'hudqstep 0.05', 'tick 30', 'reveal 1', 'cam 47 48', 'smoothmove 1', 'tick 1', 'tickalpha 0',
      'frames 2', 'meshperfdump', 'frames 10', 'meshperfdump', 'shot {d}/td_01.png',
      'cursor 1169 246', 'shot {d}/td_02.png', 'sbhold 1169 246', 'shot {d}/td_03.png', 'sbrelease',
      'sbcategory infantry', 'cursor 1156 298', 'shot {d}/td_04.png', 'cursor 600 400',
      'build E1', 'tick 2', 'shot {d}/td_05.png',
      'tickalpha 0.5', 'frames 1', 'shot {d}/td_06.png', 'tickalpha 0',
      'queue E1 3', 'tick 1', 'shot {d}/td_07.png',
      'build E2', 'tick 1', 'shot {d}/td_08.png',
      'hold E1', 'tick 1', 'shot {d}/td_09.png', 'build E1', 'tick 1',
      'frames 5', 'shot {d}/td_10.png',
      'cursor 1071 69', 'shot {d}/td_11.png', 'sbtool repair', 'shot {d}/td_12.png', 'sbtool repair',
      'cursor 600 400',
      'powerforce 200 500', 'frames 1', 'shot {d}/td_13.png', 'powerforce off', 'tick 1', 'shot {d}/td_14.png',
      'band 0 0 1000 700', 'frames 1', 'shot {d}/td_15.png', 'deselect', 'frames 1',
      'minimapoff', 'frames 1', 'shot {d}/td_16.png', 'minimapon', 'frames 1', 'shot {d}/td_17.png',
      'cam 40 40', 'frames 1', 'shot {d}/td_18.png', 'tick 15', 'shot {d}/td_19.png',
      'gfxmode 0', 'frames 1', 'shot {d}/td_20.png', 'tick 10', 'shot {d}/td_21.png',
      'gfxmode 1', 'frames 1', 'shot {d}/td_22.png', 'tick 10', 'shot {d}/td_23.png',
      'gfxmode 2', 'hud 1', 'frames 1', 'shot {d}/td_24.png',
      'meshperfdump', 'quit']

# Surfaces a still Enhanced frame shows at least: Red Alert's bar, wing, three header
# plates and the mission timer; Tiberian Dawn's bar and its header plates. One skip each
# per frame is the floor claimed. The scripts open in Enhanced (gfxmode 2, hud 1): a bare
# run starts on the DOS bar, which is one surface.
MIN_SURFACES = {'ra': 5, 'td': 4}


def write_scripts(d):
    """<game>_v.txt shoots into <game>/, <game>_a.txt the same into <game>_a/."""
    for game, lines in (('ra', RA), ('td', TD)):
        for arm, sub in (('v', game), ('a', game + '_a')):
            os.makedirs(os.path.join(d, sub), exist_ok=True)
            with open(os.path.join(d, '%s_%s.txt' % (game, arm)), 'w') as f:
                f.write('\n'.join(l.format(d=os.path.join(d, sub)) for l in lines) + '\n')


def compose_lines(path):
    out = []
    for l in open(path, errors='replace').read().splitlines():
        if l.startswith('HUDCOMPOSE|'):
            out.append(dict(kv.split('=', 1) for kv in l.split('|')[1:] if '=' in kv))
    return out


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()


def check(d):
    notes = []
    for game in ('ra', 'td'):
        v, a = os.path.join(d, game + '_v.log'), os.path.join(d, game + '_a.log')
        if not os.path.exists(v) or not os.path.exists(a):
            return '0 %s: a log is missing' % game
        lv, la = compose_lines(v), compose_lines(a)
        if len(lv) < 3 or len(la) < 3:
            return '0 %s: want three HUDCOMPOSE lines per run, got %d and %d (a renderer without the counters?)' % (game, len(lv), len(la))
        s0, s1, end = lv[0], lv[1], lv[-1]
        frames = 10
        composed = int(s1['composed']) - int(s0['composed'])
        skipped = int(s1['skipped']) - int(s0['skipped'])
        if end.get('mode') != 'verify':
            return '0 %s: the verify run reports mode=%s' % (game, end.get('mode'))
        if composed != 0:
            return '0 %s: %d surfaces were composed during ten still frames (want 0)' % (game, composed)
        if skipped < frames * MIN_SURFACES[game]:
            return '0 %s: only %d composes skipped during ten still frames (want at least %d)' % (
                game, skipped, frames * MIN_SURFACES[game])
        if int(end['mismatch']) != 0:
            return '0 %s: verify found %s surfaces that differ from a compose from scratch (HUDCOMPOSEVERIFY lines in the log)' % (
                game, end['mismatch'])
        if int(end['verified']) < 100:
            return '0 %s: verify compared only %s surfaces (want at least 100)' % (game, end['verified'])
        if game == 'ra' and (int(end['cell_patches']) < 1 or int(end['radar_patches']) < 1):
            return '0 ra: the partial redraws were not exercised: cell_patches=%s radar_patches=%s' % (
                end['cell_patches'], end['radar_patches'])
        ae = la[-1]
        if ae.get('mode') != '0' or int(ae['skipped']) != 0:
            return '0 %s: the compose-always run reports mode=%s skipped=%s (want 0 and 0)' % (game, ae.get('mode'), ae['skipped'])
        sv, sa = os.path.join(d, game), os.path.join(d, game + '_a')
        shots = sorted(f for f in os.listdir(sv) if f.endswith('.png'))
        want = len([l for l in (RA if game == 'ra' else TD) if l.startswith('shot ')])
        if len(shots) != want:
            return '0 %s: %d of %d shots written by the verify run' % (game, len(shots), want)
        diff = [s for s in shots if not os.path.exists(os.path.join(sa, s)) or sha(os.path.join(sv, s)) != sha(os.path.join(sa, s))]
        if diff:
            return '0 %s: %d of %d shots differ from the compose-always run: %s' % (game, len(diff), len(shots), ' '.join(diff[:6]))
        notes.append('%s %d shots identical, still frames composed 0 and skipped %d, verified %s with 0 mismatches%s' % (
            game, len(shots), skipped, end['verified'],
            ', %s cell and %s radar patches' % (end['cell_patches'], end['radar_patches']) if game == 'ra' else ''))
    return '1 ' + '; '.join(notes)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'script':
        write_scripts(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == 'check':
        print(check(sys.argv[2]))
    else:
        sys.exit(__doc__)
