#!/usr/bin/env python3
"""G384: the pieces of a destroyed building draw the same picture with their fragment stores
resting, and the resting saves the copying (game/mesh_batch.h: mesh_fragment_rest_mode,
mesh_fragment_store_begin, mesh_fragment_rest_note).

  gate_fragmentrest.py script DIR      write DIR/rest.txt and DIR/full.txt, the two scripts
  gate_fragmentrest.py check DIR       read DIR/rest.log, DIR/full.log and the shots:
                                       one line, "1|0 message"

Two hidden runs of Red Alert's first Allied mission (SCG01EA) under Enhanced, the same script,
the camera on the explosive barrels beside the Soviet Power Plant: one with the resting rule
(OPENCNC3D_FRAGMENT_REST=1, the default) and one with every store in use every frame
(OPENCNC3D_FRAGMENT_REST=0, the rule before). The first barrel is destroyed (hurtobj); the
nearby Power Plant (cell 62,58) is killed first to reserve a visible destruction slot, then the blast sets off
the barrels. This preserves the full-fragment fidelity workload under the two-building cap. A shot
is taken before the blast and at four moments after it, and the mesh batch's counters
(meshperfdump) bracket a window of 40 frames, one an engine tick, while the pieces fly, and
then a pause: one frame with the clock standing still, then PAUSE frames more, counted.

CLAIMS, from the engine's and the renderer's own counts (machine independent):
  (1) the scene happened: both runs end with no script failure, and in both the Power Plant
      came apart into at least MIN_PIECES pieces (its EBLD|wreck line);
  (2) the rule ran in the resting run and not in the other: FRAGMENTREST mode=1 with scopes
      rested (skips) and retried (tries) inside the window; mode=0 with no skips;
  (3) both runs drew the same pieces the same way: inside the window the fragment flushes
      and the fragment ranges they replayed are equal and at least MIN_FLUSHES;
  (4) the resting saves the work it is for: the bytes collected as full vertex records inside
      the window are at most COLLECT_RATIO of the other run's;
  (5) the pictures are the same: the shot before the blast is byte for byte the other run's,
      and every shot after it is either byte for byte the other run's or differs in at most
      NOISE_PIXELS pixels by at most NOISE_LEVELS levels, with at least half of them exact.
      (The explosion frames are not perfectly repeatable on every machine: two runs of one
      unchanged renderer were measured to differ in up to five pixels of an explosion shot
      by up to ten levels. A changed vertex moves whole faces, thousands of pixels.)
  (6) a pause gives the stores back at once: on the first frame with the clock standing still
      the scopes still resting tried their stores (FRAGMENTREST still), and over the PAUSE
      frames after it the resting run reused whole streams exactly as often as the other run
      and prepared exactly as many vertices, with the same flushes.
PROVEN TO FAIL on the renderer before the rule: it prints no FRAGMENTREST line (claim 2),
and with OPENCNC3D_FRAGMENT_REST=0 in both runs nothing rests and nothing is saved (claims 2
and 4). On the first version of the rule, which let a scope rest on through a pause, claim 6
fails: 152 of 800 streams reused in the paused frames, and 6.46 million vertices prepared.
Measured: the window's full records were 37% of the other run's (283.9 MB against 762.9 MB);
1517 flushes and 55007 ranges in both; 43 scopes came back on the first still frame, and both
runs reused 800 of 800 streams and prepared no vertices in the 20 paused frames after it; all
five shots exact, and the same bytes as the renderer before the rule drew.
NOT COVERED: the time saved, measured by hand on the 32-bit Windows build (4 to 10 percent off the
frames while the pieces fly and sink, once the first 20 engine ticks, which the pauses of
first use dominate, are past; a pause right after the blast as fast as with every store in
use from its second still frame); how soon a resting scope comes back when its pieces stop
moving while the clock runs on; Tiberian Dawn's destroyed buildings, which share the code;
the scopes of damaged buildings standing still, whose stores keep reusing.
"""
import hashlib
import os
import re
import sys

MIN_PIECES = 50
MIN_FLUSHES = 100
COLLECT_RATIO = 0.5
NOISE_PIXELS = 16
NOISE_LEVELS = 12
WINDOW = 40
PAUSE = 20
SHOTS = [('s000', None), ('s003', 3), ('s009', 9), ('s060', 60), ('s120', 120)]


def script(d):
    for arm in ('rest', 'full'):
        lines = ['gfxmode 2', 'gfx show_stats 0', 'tick 30', 'reveal 1', 'cam 60 57', 'dist 3000', 'smoothmove 1', 'tick 1',
                 'tickalpha 0.5', 'frames 3', 'shot %s/%s_s000.png' % (d, arm), *(['hurtcell 62 58'] * 8), 'hurtobj BARL 0']
        t = 0
        for name, at in SHOTS[1:3]:
            lines += ['tick %d' % (at - t), 'tickalpha 0.5', 'frames 2', 'shot %s/%s_%s.png' % (d, arm, name)]
            t = at
        lines += ['label WINDOW', 'meshperfdump']
        for _ in range(WINDOW):
            lines += ['tick 1', 'frames 1']
        t += WINDOW
        lines += ['meshperfdump', 'label END', 'frames 1', 'label PAUSE', 'meshperfdump']
        lines += ['frames 1'] * PAUSE
        lines += ['meshperfdump', 'label PAUSEEND']
        for name, at in SHOTS[3:]:
            lines += ['tick %d' % (at - t), 'tickalpha 0.5', 'frames 2', 'shot %s/%s_%s.png' % (d, arm, name)]
            t = at
        lines += ['quit']
        with open(os.path.join(d, arm + '.txt'), 'w') as f:
            f.write('\n'.join(lines) + '\n')


def fields(line):
    return dict((k, int(v)) for k, v in re.findall(r'(\w+)=(-?\d+)', line))


def dumps(text, start, end):
    """the counters at each meshperfdump between LABEL|start and LABEL|end"""
    part = text.split('LABEL|' + start, 1)[-1].split('LABEL|' + end, 1)[0]
    out = []
    for tag in ('MESHPERF|', 'FRAGMENTREST|', 'FRAGMENTREUSE|', 'FRAGMENTPREP|'):
        out.append([fields(l) for l in part.splitlines() if l.startswith(tag)])
    if any(len(x) != 2 for x in out):
        return None
    return [dict(m, **{'rest_' + k: v for k, v in r.items()}, **{'reuse_' + k: v for k, v in u.items()},
                 **{'prep_' + k: v for k, v in q.items()}) for m, r, u, q in zip(*out)]


def window(text, start='WINDOW', end='END'):
    """counter deltas between the two meshperfdumps of a window"""
    a = dumps(text, start, end)
    if not a:
        return None
    d = {k: a[1][k] - a[0].get(k, 0) for k in a[1]}
    d['mode'] = a[1]['rest_mode']
    return d


def still(text):
    """the first frame with the clock standing still: from the window's last dump to the pause's first"""
    a, b = dumps(text, 'WINDOW', 'END'), dumps(text, 'PAUSE', 'PAUSEEND')
    if not a or not b:
        return None
    return {k: b[0][k] - a[1].get(k, 0) for k in b[0]}


def picdiff(a, b):
    from PIL import Image
    A = Image.open(a).convert('RGB')
    B = Image.open(b).convert('RGB')
    if A.size != B.size:
        return (10 ** 9, 255)
    pa, pb = A.tobytes(), B.tobytes()
    n, worst = 0, 0
    for i in range(0, len(pa), 3):
        if pa[i:i + 3] != pb[i:i + 3]:
            n += 1
            worst = max(worst, abs(pa[i] - pb[i]), abs(pa[i + 1] - pb[i + 1]), abs(pa[i + 2] - pb[i + 2]))
    return n, worst


def check(d):
    logs = {}
    for arm in ('rest', 'full'):
        p = os.path.join(d, arm + '.log')
        if not os.path.exists(p):
            return '0 the %s run left no log' % arm
        logs[arm] = open(p, errors='replace').read()
    for arm, text in logs.items():
        if not re.search(r'SCRIPT\|end .*: \d+ lines, 0 failures', text):
            return '0 the %s run did not end cleanly (a script line failed)' % arm
        pieces = [int(x) for x in re.findall(r'EBLD\|wreck\|key=\d+\|frame=\d+\|pieces=(\d+)', text)]
        if not pieces or max(pieces) < MIN_PIECES:
            return '0 the %s run: no building came apart into %d pieces (largest %s)' % (arm, MIN_PIECES, max(pieces) if pieces else 'none')
    w = {arm: window(text) for arm, text in logs.items()}
    if not w['rest'] or not w['full']:
        return '0 no FRAGMENTREST counters around the window (a renderer without the resting rule)'
    r, f = w['rest'], w['full']
    if r['mode'] != 1 or f['mode'] != 0:
        return '0 the runs were not one with the rule and one without (modes %d and %d)' % (r['mode'], f['mode'])
    if r['rest_skips'] < 1 or r['rest_tries'] < 1:
        return '0 nothing rested in the window (skips %d, tries %d)' % (r['rest_skips'], r['rest_tries'])
    if f['rest_skips'] != 0:
        return '0 the run without the rule rested %d scopes' % f['rest_skips']
    if r['fragment_flushes'] != f['fragment_flushes'] or r['fragment_ranges'] != f['fragment_ranges'] \
            or r['fragment_flushes'] < MIN_FLUSHES:
        return '0 the two runs did not draw the same pieces (flushes %d and %d, ranges %d and %d)' % (
            r['fragment_flushes'], f['fragment_flushes'], r['fragment_ranges'], f['fragment_ranges'])
    if f['rest_collected_bytes'] <= 0 or r['rest_collected_bytes'] > COLLECT_RATIO * f['rest_collected_bytes']:
        return '0 the resting saved too little: %d bytes of full records against %d' % (
            r['rest_collected_bytes'], f['rest_collected_bytes'])
    p = {arm: window(text, 'PAUSE', 'PAUSEEND') for arm, text in logs.items()}
    s1 = {arm: still(text) for arm, text in logs.items()}
    if not p['rest'] or not p['full'] or not s1['rest']:
        return '0 no counters around the pause'
    pr, pf = p['rest'], p['full']
    if pr['fragment_flushes'] != pf['fragment_flushes'] or pr['fragment_flushes'] < PAUSE:
        return '0 the two runs did not draw the same pieces in the pause (flushes %d and %d)' % (
            pr['fragment_flushes'], pf['fragment_flushes'])
    if pr['reuse_streams'] != pf['reuse_streams'] or pr['prep_vertices'] != pf['prep_vertices']:
        return '0 in the %d paused frames the resting run reused %d whole streams and prepared %d vertices, the other run %d and %d' % (
            PAUSE, pr['reuse_streams'], pr['prep_vertices'], pf['reuse_streams'], pf['prep_vertices'])
    if s1['rest'].get('rest_still', 0) < 1:
        return '0 no resting scope tried its store on the first frame with the clock standing still'
    exact, near, notes = 0, 0, []
    for name, at in SHOTS:
        a = os.path.join(d, 'rest_%s.png' % name)
        b = os.path.join(d, 'full_%s.png' % name)
        if not (os.path.exists(a) and os.path.exists(b)):
            return '0 the shot %s is missing' % name
        if hashlib.sha256(open(a, 'rb').read()).digest() == hashlib.sha256(open(b, 'rb').read()).digest():
            exact += 1
            continue
        if at is None:
            return '0 the shot before the blast differs between the runs'
        n, worst = picdiff(a, b)
        if n > NOISE_PIXELS or worst > NOISE_LEVELS:
            return '0 the shot %s differs in %d pixels by up to %d levels' % (name, n, worst)
        near += 1
        notes.append('%s %d px by %d' % (name, n, worst))
    if exact * 2 < len(SHOTS):
        return '0 only %d of %d shots are byte for byte the same' % (exact, len(SHOTS))
    return '1 a destroyed building\'s pieces draw the same with their stores resting: %d shots exact%s; in %d frames of flying pieces %d scopes rested and %d retried, the same %d flushes and %d ranges, %d bytes of full records against %d (%.0f%%); paused, %d scopes came back on the first still frame and the next %d frames reused %d of %d streams in both runs' % (
        exact, (', %d within the explosion noise (%s)' % (near, '; '.join(notes))) if near else '',
        WINDOW, r['rest_skips'], r['rest_tries'], r['fragment_flushes'], r['fragment_ranges'],
        r['rest_collected_bytes'], f['rest_collected_bytes'], 100.0 * r['rest_collected_bytes'] / f['rest_collected_bytes'],
        s1['rest']['rest_still'], PAUSE, pr['reuse_streams'], pr['fragment_flushes'])


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'script':
        script(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == 'check':
        print(check(sys.argv[2]))
    else:
        sys.exit(__doc__)
