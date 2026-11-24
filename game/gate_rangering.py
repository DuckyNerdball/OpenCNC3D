#!/usr/bin/env python3
"""G493: a selected launcher or firing defence shows its owner the reach of its weapon, as
a dashed circle whose dashes travel slowly round it.

    gate_rangering.py script DIR            write DIR/td.txt and DIR/ra.txt
    gate_rangering.py check DIR [RECDIR]    read DIR/td.log, DIR/ra.log and the shots; print
                                            "1 msg" or "0 msg" on the first line. With
                                            RECDIR, the rings' records are read from
                                            RECDIR's logs and the pictures from DIR's
                                            shots: how this build's rings are looked for
                                            in the pictures of a build before it.

The ring (draw_range_rings, dos2d_range_rings) is drawn only round the player's own
selected long-range launcher vehicles and structures with a weapon, never round infantry
or tanks. Its reach is the brain's (OpenCNC3D_Weapon_Range): the range In_Range compares
against plus the muzzle distance it takes off first, measured from the weapon's firing
centre. It is a circle of that radius, drawn as dashes whose angle is a pure function of
the world's clock (one turn in the record's `turn` seconds).

Each game runs in Enhanced, N64 and Classic, and in each mode selects in turn:

  launcher  Tiberian Dawn's Rocket Launcher (MSAM) / Red Alert's V2 Rocket Launcher (V2RL),
            spawned for the player: one ring, round it;
  same      still the launcher, after four more frames drawn with the engine standing
            still: the same clock, the same ring;
  later     still the launcher, a few engine ticks on: the dashes have moved round;
  tower     a Guard Tower (GTWR) / a Tesla Coil (TSLA), spawned for the player: one ring;
  rifle     the player's first Rifleman (E1), armed (the brain gives his range) yet with no
            ring;
  off       nothing selected: no ring;
  all       all three together: exactly two rings, the launcher's and the tower's.

Claims, per game and mode:
  (1) the export is there (export=1);
  (2) the rings drawn are exactly those listed above, each in that mode's pass;
  (3) each ring's reach is the brain's range plus muzzle, centred on the firing centre
      the brain gives (the drawn position of a vehicle, the engine's centre of a building,
      moved by the brain's offset), and the ranges are the 1995 and 1996 values: MSAM 1536
      leptons (WEAPON_MLRS, 6 cells), GTWR 1024 plus a 64 muzzle (WEAPON_CHAIN_GUN), V2RL
      2560 (RULES.INI SCUD, 10 cells), TSLA 2176 (TeslaZap, 8.5 cells);
  (4) IT IS A CIRCLE: every point the pass drew lies at the reach from the centre (in the
      3D modes within 0.3 per cent; in Classic, whose points are whole 1995 pixels, within
      three quarters of one of them), including the drawn point nearest each of the eight
      compass directions, which is never more than one dash's period off it; and the
      sixteen sampled dash middles, in world cells, lie at the reach and fill all eight
      45 degree sectors. The engine's octagon is 0.943 of the reach on the diagonals and
      0.894 of it (1/sqrt(1.25)) 26.6 degrees off each axis, so every dash middle measures
      between R and 1.118 R by the engine's own Distance(); the range seen is reported;
  (5) IT TURNS ON THE WORLD'S CLOCK: the dashes' angle is the clock's share of a turn;
      `same` has the launcher leg's clock and angle exactly, and in N64 and Classic draws
      the same pixels round every sampled point (Enhanced's temporal antialiasing settles
      the whole picture further over four frames, so there `same` is held to claim (6)
      against its own record like any other leg); `later` is ahead by the ticks played over the tick rate, its angle
      ahead by that share of a turn;
  (6) THE DASHES ARE ON THE SCREEN WHERE THE RECORD SAYS: of the sampled points inside
      the tactical view, at least three quarters of the dash middles are lit (a pixel
      within two of the point at least 60 brighter than in the deselected shot) and at
      most a quarter of the gap middles; and in the `later` leg, the dash middles that the
      launcher leg's angle puts squarely in a gap are dark in the launcher leg's picture
      and those it puts squarely on a dash are lit, so the picture moved as the record did.
"""
import math
import os
import sys

MODES = [(2, 'Enhanced'), (1, 'N64'), (0, 'Classic')]
GAMES = {
    'td': {'name': 'Tiberian Dawn', 'launcher': 'MSAM', 'tower': 'GTWR', 'cam': '58 51',
           'setup': 'tick 3\nreveal 1\ncheatset invuln 1\nspawnobj MSAM 60 49\nspawnobj GTWR 57 50\ntick 6\n',
           'reach': {'MSAM': (1536, 0), 'GTWR': (1024, 64)},
           'later': 6},
    'ra': {'name': 'Red Alert', 'launcher': 'V2RL', 'tower': 'TSLA', 'cam': '74 50',
           'setup': 'tick 20\nreveal 1\ncheatset invuln 1\nspawnobj POWR 71 45\ntick 3\n'
                    'spawnobj V2RL 72 51\nspawnobj TSLA 74 48\ntick 30\n',
           'reach': {'V2RL': (2560, 0), 'TSLA': (2176, 0)},
           'later': 3},
}
LEGS = ['launcher', 'same', 'later', 'tower', 'rifle', 'off', 'all']
SINGLE = ('launcher', 'same', 'later', 'tower')   # one ring, looked for in the picture


def script(d):
    for g, cfg in GAMES.items():
        out = [cfg['setup']]
        for m, _ in MODES:
            out.append('gfxmode %d\ncam %s\ntick 3\n' % (m, cfg['cam']))
            out.append('lclickobj %s:@me 0\ntick 1\nshot %s/%s_launcher%d.png\nrangeringdump\n' % (cfg['launcher'], d, g, m))
            out.append('frames 4\nshot %s/%s_same%d.png\nrangeringdump\n' % (d, g, m))
            out.append('tick %d\nshot %s/%s_later%d.png\nrangeringdump\n' % (cfg['later'], d, g, m))
            out.append('lclickobj %s:@me 0\ntick 1\nshot %s/%s_tower%d.png\nrangeringdump\n' % (cfg['tower'], d, g, m))
            out.append('lclickobj E1:@me 0\ntick 1\nshot %s/%s_rifle%d.png\nrangeringdump\n' % (d, g, m))
            out.append('deselect\ntick 1\nshot %s/%s_off%d.png\nrangeringdump\n' % (d, g, m))
            out.append('lclickobj %s:@me 0\nlclickobj %s:@me 0 shift\nlclickobj E1:@me 0 shift\n'
                       'tick 1\nshot %s/%s_all%d.png\nrangeringdump\n' % (cfg['launcher'], cfg['tower'], d, g, m))
            out.append('deselect\ntick 1\n')
        out.append('quit\n')
        with open(os.path.join(d, g + '.txt'), 'w') as f:
            f.write(''.join(out))


def kv(fields):
    return dict(x.split('=', 1) for x in fields if '=' in x)


def pts_of(s):
    return [tuple(float(v) for v in q.split(',')) for q in s.split(';')] if s else []


def dumps_of(path):
    """Every rangeringdump in the log, in order: head, objects, rings."""
    res, cur = [], None
    with open(path, encoding='latin-1') as f:
        for line in f:
            line = line.rstrip('\n')
            if line.startswith('RANGERING-BEGIN'):
                cur = {'head': kv(line.split()[1:]), 'objs': [], 'rings': []}
            elif cur is not None and line.startswith('RANGERING-OBJ|'):
                p = line.split('|')
                o = kv(p[3:]); o['type'] = p[1]; o['house'] = p[2]
                cur['objs'].append(o)
            elif cur is not None and line.startswith('RANGERING|'):
                p = line.split('|')
                r = kv(p[2:]); r['type'] = p[1]
                for k in ('pts', 'gpts', 'wpts'):
                    r[k] = pts_of(r.get(k, ''))
                for k in ('rdir', 'adir'):
                    r[k] = [float(v) for v in r[k].split(';')] if r.get(k) else []
                cur['rings'].append(r)
            elif cur is not None and line.startswith('RANGERING-END'):
                res.append(cur); cur = None
    return res


class Shot:
    def __init__(self, path):
        from PIL import Image
        self.im = Image.open(path).convert('RGB')
        self.px = self.im.load()
        self.w, self.h = self.im.size

    def window(self, x, y):
        return [(int(x) + dx, int(y) + dy) for dy in range(-2, 3) for dx in range(-2, 3)
                if 0 <= int(x) + dx < self.w and 0 <= int(y) + dy < self.h]


def lit(sel, off, x, y):
    return any(min(sel.px[p]) - min(off.px[p]) >= 60 for p in sel.window(x, y))


def in_view(view, x, y):
    return view[0] + 3 <= x <= view[2] - 3 and view[1] + 3 <= y <= view[3] - 3


def frac(v):
    return v - math.floor(v)


def check_circle(tag, r, fails):
    """Claim (4), from the record alone."""
    R = float(r['r'])
    need = ('rmin', 'rmax', 'phase', 'dashes', 'clock')
    if any(k not in r for k in need) or len(r['rdir']) != 8 or len(r['wpts']) != 16:
        fails.append('%s: %s has no circle record (rmin/rmax/rdir/wpts/phase)' % (tag, r['type']))
        return False
    tol = 0.003 * R if r['classic'] == '0' else 0.75 / 24.0
    n = int(r['dashes'])
    if abs(float(r['rmin']) - R) > tol or abs(float(r['rmax']) - R) > tol:
        fails.append('%s: %s drawn between %s and %s cells from the centre, want %.4f' % (
            tag, r['type'], r['rmin'], r['rmax'], R))
    for k in range(8):
        if abs(r['rdir'][k] - R) > tol or r['adir'][k] > 360.0 / n + 0.01:
            fails.append('%s: %s at %d degrees drawn at %.4f cells (%.2f degrees off), want %.4f' % (
                tag, r['type'], 45 * k, r['rdir'][k], r['adir'][k], R))
    cx, cz = float(r['cx']), float(r['cz'])
    sectors = set()
    for (x, z) in r['wpts']:
        if abs(math.hypot(x - cx, z - cz) - R) > 0.003 * R:
            fails.append('%s: %s dash middle %.3f,%.3f is %.4f cells out, want %.4f' % (
                tag, r['type'], x, z, math.hypot(x - cx, z - cz), R))
            break
        sectors.add(int((math.degrees(math.atan2(z - cz, x - cx)) % 360.0) // 45.0))
    if len(sectors) != 8:
        fails.append('%s: %s dash middles fill %d of the 8 sectors' % (tag, r['type'], len(sectors)))
    # How far beyond the engine's own reach the circle lies: Distance() is the larger axis
    # distance plus half the smaller, so a point of the circle measures between R (on an
    # axis) and sqrt(1.25) R (26.6 degrees off one) to the engine.
    for (x, z) in r['wpts']:
        a, b = abs(x - cx), abs(z - cz)
        e = (max(a, b) + min(a, b) / 2.0) / R
        if e < 1.0 - 0.003 or e > math.sqrt(1.25) + 0.003:
            fails.append('%s: %s dash middle measures %.4f R to the engine, outside 1 to 1.118' % (
                tag, r['type'], e))
            break
        ENGINE_RATIO[0] = min(ENGINE_RATIO[0], e)
        ENGINE_RATIO[1] = max(ENGINE_RATIO[1], e)
    return True


ENGINE_RATIO = [9.0, 0.0]   # the least and the most engine Distance() of a dash middle, over R


def check(d, recdir=None):
    recdir = recdir or d
    fails, notes = [], []
    for g, cfg in GAMES.items():
        path = os.path.join(recdir, g + '.log')
        if not os.path.exists(path):
            fails.append('%s: no log' % g); continue
        ds = dumps_of(path)
        if len(ds) != len(MODES) * len(LEGS):
            fails.append('%s: %d dumps, want %d' % (g, len(ds), len(MODES) * len(LEGS))); continue
        for mi, (m, mname) in enumerate(MODES):
            tag = '%s %s' % (cfg['name'], mname)
            seen = []
            byleg = {}
            for li, leg in enumerate(LEGS):
                dump = ds[mi * len(LEGS) + li]
                byleg[leg] = dump
                if dump['head'].get('export') != '1':
                    fails.append('%s %s: no OpenCNC3D_Weapon_Range export' % (tag, leg)); continue
                want = {'launcher': [cfg['launcher']], 'same': [cfg['launcher']],
                        'later': [cfg['launcher']], 'tower': [cfg['tower']], 'rifle': [],
                        'off': [], 'all': sorted([cfg['launcher'], cfg['tower']])}[leg]
                got = sorted(r['type'] for r in dump['rings'])
                if got != want:
                    fails.append('%s %s: rings %s, want %s' % (tag, leg, got, want)); continue
                if leg == 'rifle':
                    rifle = [o for o in dump['objs'] if o['type'] == 'E1' and o.get('sel') == '1']
                    if not rifle or rifle[0].get('mine') != '1' or int(rifle[0].get('range0', '-1')) <= 0:
                        fails.append('%s: the selected Rifleman is not the player\'s armed one (%s)' % (tag, rifle[:1]))
                for r in dump['rings']:
                    if int(r['classic']) != (1 if m == 0 else 0):
                        fails.append('%s %s: %s drawn by the wrong pass' % (tag, leg, r['type']))
                    obj = [o for o in dump['objs'] if o['type'] == r['type'] and o['id'] == r['id']]
                    if not obj:
                        fails.append('%s %s: %s not in the dump' % (tag, leg, r['type'])); continue
                    o = obj[0]
                    rng, mz = int(o['range0']), int(o['muzzle0'])
                    if (rng, mz) != cfg['reach'][r['type']] or int(r['range']) != rng or int(r['muzzle']) != mz:
                        fails.append('%s %s: %s range %s+%s, brain %d+%d, want %d+%d' % (
                            (tag, leg, r['type'], r['range'], r['muzzle'], rng, mz) + cfg['reach'][r['type']]))
                    if abs(float(r['r']) - (rng + mz) / 256.0) > 1e-3:
                        fails.append('%s %s: %s reach %s cells, want %.4f' % (tag, leg, r['type'], r['r'], (rng + mz) / 256.0))
                    building = o['kind'] == '2'
                    bx = float(o['ex'] if building else o['wx']) + int(o['fdx0']) / 256.0
                    bz = float(o['ez'] if building else o['wz']) + int(o['fdy0']) / 256.0
                    if abs(float(r['cx']) - bx) > 0.01 or abs(float(r['cz']) - bz) > 0.01:
                        fails.append('%s %s: %s centred %s,%s, want %.3f,%.3f' % (tag, leg, r['type'], r['cx'], r['cz'], bx, bz))
                    if not check_circle('%s %s' % (tag, leg), r, fails):
                        continue
                    # (5) the angle is the clock's share of a turn
                    turn = float(dump['head'].get('turn', '0') or 0)
                    if turn <= 0:
                        fails.append('%s %s: no turn length in the dump' % (tag, leg)); continue
                    wantph = frac(float(r['clock']) / turn) * 2 * math.pi
                    dph = abs(float(r['phase']) - wantph)
                    if min(dph, 2 * math.pi - dph) > 1e-4:
                        fails.append('%s %s: %s dashes at %s rad, the clock %s puts them at %.6f' % (
                            tag, leg, r['type'], r['phase'], r['clock'], wantph))
                    if leg in SINGLE:
                        view = [float(v) for v in dump['head'].get('view', '0,0,0,0').split(',')]
                        try:
                            sel = Shot(os.path.join(d, '%s_%s%d.png' % (g, leg, m)))
                            off = Shot(os.path.join(d, '%s_off%d.png' % (g, m)))
                        except (OSError, IOError) as e:
                            fails.append('%s %s: %s' % (tag, leg, e)); continue
                        dv = [p for p in r['pts'] if in_view(view, *p)]
                        gv = [p for p in r['gpts'] if in_view(view, *p)]
                        nd = sum(1 for p in dv if lit(sel, off, *p))
                        ng = sum(1 for p in gv if lit(sel, off, *p))
                        seen.append('%s %s dashes %d/%d gaps %d/%d' % (leg, r['type'], nd, len(dv), ng, len(gv)))
                        if len(dv) < 4 or nd * 4 < len(dv) * 3:
                            fails.append('%s %s: %s dashes lit at %d of the %d in view, want 3/4 and 4 or more' % (
                                tag, leg, r['type'], nd, len(dv)))
                        if ng * 4 > len(gv):
                            fails.append('%s %s: %s gaps lit at %d of the %d in view, want 1/4 at most' % (
                                tag, leg, r['type'], ng, len(gv)))
            # (5) and (6) across the launcher's three legs
            try:
                ra_, rs_, rl_ = (byleg[k]['rings'][0] for k in ('launcher', 'same', 'later'))
                hz = float(byleg['later']['head']['hz'])
                turn = float(byleg['later']['head']['turn'])
                ca, cs, cl = float(ra_['clock']), float(rs_['clock']), float(rl_['clock'])
                pa, ps, pl = float(ra_['phase']), float(rs_['phase']), float(rl_['phase'])
            except (KeyError, IndexError, ValueError):
                fails.append('%s: the launcher legs carry no clock to compare' % tag); notes.append(tag); continue
            if cs != ca or ps != pa:
                fails.append('%s: four frames with the engine still moved the clock %.5f -> %.5f, angle %.6f -> %.6f' % (
                    tag, ca, cs, pa, ps))
            sa = Shot(os.path.join(d, '%s_launcher%d.png' % (g, m)))
            ss = Shot(os.path.join(d, '%s_same%d.png' % (g, m)))
            if m != 2:
                diff = sum(1 for p in ra_['pts'] + ra_['gpts'] for q in sa.window(*p) if sa.px[q] != ss.px[q])
                if diff:
                    fails.append('%s: the same clock drew %d different pixels round the ring' % (tag, diff))
            want_dt = cfg['later'] / hz
            if abs((cl - ca) - want_dt) > 1e-3:
                fails.append('%s: %d ticks moved the clock %.5f s, want %.5f' % (tag, cfg['later'], cl - ca, want_dt))
            want_dp = (cl - ca) / turn * 2 * math.pi
            got_dp = (pl - pa) % (2 * math.pi)
            if abs(got_dp - want_dp) > 1e-4 or want_dp <= 0:
                fails.append('%s: the dashes moved %.6f rad, want %.6f' % (tag, got_dp, want_dp))
            # the later leg's dash middles in the launcher leg's picture
            view = [float(v) for v in byleg['launcher']['head']['view'].split(',')]
            n = int(ra_['dashes'])
            cx, cz = float(ra_['cx']), float(ra_['cz'])
            off = Shot(os.path.join(d, '%s_off%d.png' % (g, m)))
            sl = Shot(os.path.join(d, '%s_later%d.png' % (g, m)))
            on_ok = on_n = gap_ok = gap_n = 0
            for (wx, wz), (sx, sy) in zip(rl_['wpts'], rl_['pts']):
                if not in_view(view, sx, sy):
                    continue
                f = frac((math.atan2(wz - cz, wx - cx) - pa) * n / (2 * math.pi))
                if 0.1 <= f <= 0.45:
                    on_n += 1; on_ok += lit(sa, off, sx, sy)
                elif 0.65 <= f <= 0.9:
                    gap_n += 1; gap_ok += not lit(sa, off, sx, sy)
            if on_n + gap_n < 4 or (on_ok + gap_ok) * 4 < (on_n + gap_n) * 3:
                fails.append('%s: the later dashes, looked for in the earlier picture, matched its angle at %d of %d points' % (
                    tag, on_ok + gap_ok, on_n + gap_n))
            seen.append('turned %.2f deg in %.3f s (%d/%d points moved as predicted)' % (
                math.degrees(got_dp), cl - ca, on_ok + gap_ok, on_n + gap_n))
            notes.append('%s %s' % (tag, ', '.join(seen)))
    if fails:
        print('0 ' + '; '.join(fails[:8]) + ('' if len(fails) <= 8 else '; and %d more' % (len(fails) - 8)))
    else:
        print('1 ' + '; '.join(notes) + '; dash middles measure %.3f to %.3f R to the engine' % tuple(ENGINE_RATIO))


if __name__ == '__main__':
    if len(sys.argv) not in (3, 4) or sys.argv[1] not in ('script', 'check'):
        sys.exit(__doc__)
    if sys.argv[1] == 'script':
        script(sys.argv[2])
    else:
        check(sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else None)
