#!/usr/bin/env python3
"""G296's two helpers (game/gates.sh).

    python3 gate_evhc.py missions SRC DST    the private scenario copies the gate plays
    python3 gate_evhc.py check               the claims, read from the runs' logs and shots

THE SCENARIOS. SRC's INI and BIN files copied to DST/all and DST/air, SCG90EA's INI edited:
  all  every remastered ground vehicle and boat placed twice, GDI and Nod side by side in
       two rows in the middle of the map, a hovercraft and a Nod gun boat on the water in
       the south, three GDI riflemen beside the APC, and the two houses made allies so
       nothing fights unless the script says so.
  air  a GDI helipad by the Nod base, whose free Orca the script flies in the SAM site's
       reach and brings down in flight.
Line endings and the latin-1 text are kept.

THE CLAIMS, each printed with the numbers it rests on; the last line is OK or FAIL.
"""
import filecmp
import glob
import os
import re
import shutil
import sys

TYPES = ("HTNK", "MTNK", "LTNK", "FTNK", "STNK", "APC", "ARTY", "MSAM", "HARV", "JEEP",
         "BGGY", "BIKE", "MCV", "MHQ", "BOAT", "LST", "HELI", "ORCA", "TRAN", "A10", "C17")
GROUND = ("HTNK", "MTNK", "LTNK", "FTNK", "STNK", "APC", "ARTY", "MSAM", "HARV", "JEEP",
          "BGGY", "BIKE", "MCV", "MHQ", "BOAT", "LST")
ROW = ("HTNK", "MTNK", "LTNK", "FTNK", "STNK", "APC", "ARTY", "MSAM", "JEEP", "BGGY", "BIKE",
       "MCV", "MHQ", "HARV")


def cell(x, y):
    return y * 64 + x


def add_rows(t, section, rows):
    i = t.index('[%s]' % section)
    j = t.index('\n\n', i)
    n = len(t[i:j].split('\n')) - 1
    return t[:j] + '\n' + '\n'.join('%03d=%s' % (n + k, r) for k, r in enumerate(rows)) + t[j:]


def missions(src, dst):
    for kind in ("all", "air"):
        d = os.path.join(dst, kind)
        os.makedirs(d, exist_ok=True)
        for f in glob.glob(os.path.join(src, '*.INI')) + glob.glob(os.path.join(src, '*.BIN')):
            shutil.copy(f, d)
        p = os.path.join(d, 'SCG90EA.INI')
        t = open(p, 'rb').read().decode('latin-1')
        if kind == "all":
            t = t.replace('Allies=GoodGuy,Neutral', 'Allies=GoodGuy,Neutral,BadGuy')
            t = t.replace('Allies=BadGuy\n', 'Allies=BadGuy,GoodGuy\n')
            rows = []
            for i, ty in enumerate(ROW):
                x, y = 39 + (i % 7) * 3, 40 + (i // 7) * 3
                rows.append('GoodGuy,%s,256,%d,96,Guard,None' % (ty, cell(x, y)))
                rows.append('BadGuy,%s,256,%d,160,Guard,None' % (ty, cell(x + 1, y + 1)))
            rows.append('GoodGuy,LST,256,%d,64,Guard,None' % cell(49, 59))
            rows.append('BadGuy,BOAT,256,%d,192,Guard,None' % cell(45, 60))
            t = add_rows(t, 'UNITS', rows)
            t = add_rows(t, 'INFANTRY', ['GoodGuy,E1,256,%d,%d,Guard,0,None' % (cell(53, 39), s)
                                         for s in (1, 2, 3)])
        else:
            t = add_rows(t, 'STRUCTURES', ['GoodGuy,HPAD,256,%d,0,None' % cell(44, 37)])
        open(p, 'wb').write(t.encode('latin-1'))
    print("missions written to", dst)


def log(name):
    try:
        return open('shots/g296_%s.log' % name, encoding='latin-1').read().splitlines()
    except OSError:
        return []


def dumps(lines, prefix):
    """The lines of each evhcdump in order: a list of lists."""
    out, cur = [], None
    for l in lines:
        if l.startswith('EVHC|loaded='):
            cur = []
            out.append(cur)
        if cur is not None and l.startswith(prefix):
            cur.append(l)
    return out


def fields(l):
    return dict(kv.split('=', 1) for kv in l.split('|') if '=' in kv)


def check():
    bad, say = [], []
    A = log('a')
    head = [l for l in A if l.startswith('EVHC|loaded=')]
    # 1. every model loads and attaches, the switch and the picture on
    if not head or head[0] != 'EVHC|loaded=21|attached=21|on=1|enhanced=1':
        bad.append('load(%s)' % (head[0] if head else '-'))
    arts = dumps(A, 'EVHCART|')
    if not arts:
        bad.append('no-dump')
        arts = [[]]
    first = {l.split('|')[1]: fields(l) for l in arts[0]}
    far = []
    for ty in TYPES:
        f = first.get(ty)
        if not f:
            bad.append('noart-' + ty)
            continue
        x0, x1 = map(float, f['x'].split(',')); cx0, cx1 = map(float, f['cartx'].split(','))
        z0, z1 = map(float, f['z'].split(',')); cz0, cz1 = map(float, f['cartz'].split(','))
        off = max(abs(x0 - cx0), abs(x1 - cx1), abs(z0 - cz0), abs(z1 - cz1))
        if off > 164.0:             # 0.16 of a cell: the lab keeps the cartridge's plan
            far.append('%s:%.0f' % (ty, off))
        if ty in GROUND and int(f['drawn']) <= 0:
            bad.append('notdrawn-' + ty)
        if int(f['wreck']) <= 0:
            bad.append('nowreck-' + ty)
    if far:
        bad.append('plan(' + ','.join(far) + ')')
    say.append('21 models load and attach, the 16 placed draw, every plan within 0.16 cells of '
               'the cartridge model it stands in for')
    objs = [l for l in A if l.startswith('EVHCOBJ|')]

    def best(ty, key):
        vals = [fields(l) for l in objs if l.split('|')[1] == ty]
        return vals
    # 2. a turret turns: the GDI Mammoth force-fired north of it
    tur = [int(f['tur']) for f in best('HTNK', 'tur') if f['key'] == '1000013']
    if not any(t != 0 for t in tur):
        bad.append('turret(%s)' % tur)
    # 3. tracks: a Medium Tank's two sides travel and its wheel flipbook steps
    mt = [f for f in best('MTNK', 'trav') if f['key'] == '1000015']
    tv = [tuple(map(float, f['trav'].split(','))) for f in mt]
    fr = set(f['frame'] for f in mt)
    if not tv or min(abs(tv[-1][0]), abs(tv[-1][1])) < 0.3 or len(fr) < 2:
        bad.append('tracks(%s,%s)' % (tv[-1] if tv else '-', sorted(fr)))
    # 4. wheels: the Hum-vee drives, rolls and steers
    je = [f for f in best('JEEP', 'trav') if f['key'] == '1000029']
    jt = [tuple(map(float, f['trav'].split(','))) for f in je]
    steer = max([abs(float(f['turn'])) for f in je] or [0.0])
    if not jt or min(abs(jt[-1][0]), abs(jt[-1][1])) < 0.3 or steer < 0.02:
        bad.append('wheels(%s,steer %.3f)' % (jt[-1] if jt else '-', steer))
    # 5. recoil: the Mammoth's shots kick both barrels in turn
    fires = [fields(l) for l in A if l.startswith('EVHC|fire|') and '|type=HTNK|' in l]
    barrels = set(f['barrel'] for f in fires if f['key'] == '1000013')
    kick = max([float(f['kick']) for f in best('HTNK', 'kick') if f['key'] == '1000013'] or [0.0])
    if barrels != {'0', '1'} or kick <= 0.0:
        bad.append('recoil(%s,%.3f)' % (sorted(barrels), kick))
    # 6. the APC's rear door opens for the riflemen
    door = max([float(f['door']) for f in best('APC', 'door') if f['key'] == '1000023'] or [0.0])
    if door < 0.5:
        bad.append('door(%.2f)' % door)
    # 7. a death: the Nod Mammoth breaks into its wreck, the old shatter does not spawn,
    #    and the wreck sinks and goes
    dth = [fields(l) for l in A if l.startswith('EVHC|death|') and '|type=HTNK|' in l]
    wreckn = first.get('HTNK', {}).get('wreck')
    sink = [l for l in A if l.startswith('EVHC|sink|') and '|type=HTNK|' in l]
    gone = [l for l in A if l.startswith('EVHC|gone|') and '|type=HTNK|' in l]
    old = [l for l in A if l.startswith('SHATTERUNIT|HTNK|')]
    if len(dth) != 1 or dth[0]['air'] != '0' or dth[0]['pieces'] != wreckn or not sink or not gone \
            or old or fields(sink[0]).get('wet') != '0':
        bad.append('death(%d deaths, pieces %s of %s, sink %d, gone %d, old %d)'
                   % (len(dth), dth[0]['pieces'] if dth else '-', wreckn, len(sink), len(gone),
                      len(old)))
    say.append('a turret turns (%s), a Medium Tank\'s tracks travel %s with frames %d, a Hum-vee '
               'rolls %s and steers %.2f, the Mammoth kicks barrels %s (%.2f), the APC door '
               'opens %.2f, a Mammoth breaks into %s pieces that sink and go with no old shatter'
               % (sorted(set(tur)), tv[-1] if tv else '-', len(fr), jt[-1] if jt else '-', steer,
                  sorted(barrels), kick, door, wreckn))
    # 8. the air: the Orca's rotors idle on the pad and spin up, the disc comes up in the air
    #    and the ducts tilt; shot down at flight level it falls whole and breaks on landing
    B = log('b')
    orca = [fields(l) for l in B if l.startswith('EVHCOBJ|ORCA|')]
    disc0 = [float(f['disc']) for f in orca[:1]]
    discs = [float(f['disc']) for f in orca]
    ducts = [float(f['duct']) for f in orca]
    rot = [float(f['rotor']) for f in orca]
    bd = [fields(l) for l in B if l.startswith('EVHC|death|') and '|type=ORCA|' in l]
    imp = [fields(l) for l in B if l.startswith('EVHC|impact|') and '|type=ORCA|' in l]
    ok_air = (disc0 and disc0[0] == 0.0 and max(discs or [0]) >= 0.9 and max(ducts or [0]) > 5.0
              and len(rot) > 2 and rot[-1] > rot[0] and len(bd) == 1 and bd[0]['air'] == '1'
              and float(bd[0]['lift']) > 0.5 and len(imp) == 1
              and 3 <= int(imp[0]['frame']) - int(bd[0]['frame']) <= 40)
    if not ok_air:
        bad.append('air(disc %s..%.2f duct %.1f rotor %s deaths %d impacts %d)'
                   % (disc0, max(discs or [0]), max(ducts or [0]), rot[:1] + rot[-1:], len(bd),
                      len(imp)))
    else:
        say.append('the Orca idles, spins up (disc %.2f, ducts %.0f degrees), is shot down at '
                   'lift %s and lands broken %d ticks later'
                   % (max(discs), max(ducts), bd[0]['lift'],
                      int(imp[0]['frame']) - int(bd[0]['frame'])))
    # 8b. water: a gunboat killed on the sea goes down within a second of its death, and
    #     its wreck knows it is on water (the Mammoth on land above sank dry)
    Wl = log('w')
    wd = [fields(l) for l in Wl if l.startswith('EVHC|death|') and '|type=BOAT|' in l]
    ws = [fields(l) for l in Wl if l.startswith('EVHC|sink|') and '|type=BOAT|' in l]
    wg = [l for l in Wl if l.startswith('EVHC|gone|') and '|type=BOAT|' in l]
    if len(wd) != 1 or len(ws) != 1 or ws[0].get('wet') != '1' or not wg \
            or not 1 <= int(ws[0]['frame']) - int(wd[0]['frame']) <= 15:
        bad.append('water(deaths %d sinks %s gone %d)' % (len(wd), ws[:1], len(wg)))
    else:
        say.append('a gunboat sunk on the sea goes down %d ticks after it dies'
                   % (int(ws[0]['frame']) - int(wd[0]['frame'])))
    # 9. the rigs: the MCV unfolds, the harvester empties in the dock. Under Enhanced the
    #    remastered Construction Yard's placement (ebld_place.h) IS the deploy: it draws the
    #    remastered MCV as its companion and unfolds the yard out of it, and the cartridge's
    #    deploy rig stands down for it (mcvrig_for), so in run c the MCV is drawn by the
    #    placement and never by the rig. Run c2 (--noeplc, a yard with no placement) is the
    #    rig's own case, where evhc_deploy_draw puts the remastered MCV in the rig.
    def rigs(L):
        rig = {}
        for l in L:
            if l.startswith('EVHCART|'):
                f = fields(l)
                rig[l.split('|')[1]] = max(rig.get(l.split('|')[1], 0), int(f['rigdrawn']))
        return rig
    C = log('c')
    rig, rig2 = rigs(C), rigs(log('c2'))
    fact = [fields(l) for l in C if l.startswith('EPLCMODEL|FACT|')]
    started = [l for l in C if l.startswith('EPLC|start|') and '|type=FACT|' in l]
    placed = bool(fact) and int(fact[-1].get('companion', '0')) > 0 \
        and int(fact[-1].get('drawn', '0')) > 0 and bool(started)
    clips = [float(fields(l).get('clip', '-1')) for l in C
             if l.startswith('EVHCOBJ|HARV|') and '|drawn=1|' in l]
    if not placed or rig.get('MCV', 0) != 0 or rig2.get('MCV', 0) <= 0 or rig.get('HARV', 0) <= 0:
        bad.append('rigs(placement %s%s, MCV in the rig %d (want 0 beside the placement), '
                   'MCV in the rig with --noeplc %d, HARV %d)'
                   % ('played' if started else 'not started',
                      (' companion %s drawn %s' % (fact[-1].get('companion'), fact[-1].get('drawn')))
                      if fact else ' no eplcdump', rig.get('MCV', 0), rig2.get('MCV', 0),
                      rig.get('HARV', 0)))
    elif not clips or max(clips) < 20.0 or min(clips) != 0.0:
        bad.append('arms(%s)' % sorted(set(clips)))
    else:
        say.append('the harvester works its arms (clip to %.0f); the MCV unfolds in the '
                   'Construction Yard\'s placement (companion of %s runs) and, with no placement, '
                   'in its deploy rig; the harvester is drawn in the refinery dock'
                   % (max(clips), fact[-1].get('companion')))
    # 10. switched off and in Classic: nothing of it draws or claims, and the old shatter
    #     is back for the same death
    for name, what in (('n', '--noevhc'), ('cl', 'Classic')):
        L = log(name)
        h = [l for l in L if l.startswith('EVHC|loaded=')]
        drawn = sum(int(fields(l)['drawn']) for l in L if l.startswith('EVHCART|'))
        ev = [l for l in L if l.startswith(('EVHC|fire', 'EVHC|death'))]
        oldl = [l for l in L if l.startswith('SHATTERUNIT|HTNK|')]
        if not h or '|on=0|' not in h[0] or drawn or ev or len(oldl) != 1:
            bad.append('%s(%s drawn %d events %d old %d)' % (what, h[0] if h else '-', drawn,
                                                              len(ev), len(oldl)))
    say.append('with --noevhc and in Classic nothing draws or claims and the old shatter takes '
               'the Mammoth')
    # 11. two runs are one picture
    for s in ('1', '2', '3'):
        a, b = 'shots/g296_a_%s.png' % s, 'shots/g296_a2_%s.png' % s
        if not (os.path.exists(a) and os.path.exists(b) and filecmp.cmp(a, b, shallow=False)):
            bad.append('nondeterministic' + s)
    print('\n'.join(say))
    print(('OK ' + '; '.join(say[:1])) if not bad else 'FAIL ' + ' '.join(bad))


if __name__ == '__main__':
    if len(sys.argv) >= 4 and sys.argv[1] == 'missions':
        missions(sys.argv[2], sys.argv[3])
    elif len(sys.argv) >= 2 and sys.argv[1] == 'check':
        check()
    else:
        raise SystemExit(__doc__)
