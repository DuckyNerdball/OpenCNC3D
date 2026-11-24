#!/usr/bin/env python3
"""Attack-move and G-key fixtures for gates G478 and G479 (game/gates.sh).

  attack_move.py write DIR GAME FIXTURE [GFXMODE]
      Writes DIR/SCG88EA.INI (and DIR/SCG88EA.BIN for Tiberian Dawn) and DIR/script.txt.
      GAME is td or ra. GFXMODE is the `gfxmode` the script starts in (0 Classic, 1 N64,
      2 Enhanced; 1 when left out).
  attack_move.py measure LOG GAME FIXTURE
      Reads the engine's own object dumps out of a run's log and prints one line of
      key=value measurements, then a verdict word, pass or fail.
  attack_move.py digest LOG
      A digest of every unit and soldier line of every dump, the selection flag left out,
      so that runs in different graphics modes can be compared.

The missions are flat 60x60 (Tiberian Dawn) or 100x100 (Red Alert) maps with no shroud
drawn and the campaign rules of a scenario (the player sees an enemy only once it has
been in sight). Run them through cnc_eyes with --scen SCG88EA --dir DIR/.

FIXTURES
  chase-am  Two of the player's tanks attack-move 25 cells east. An enemy scout runs north
            off the map past them; a sleeping, weakened enemy light tank sits to the north,
            out of sight and out of weapon range at the start, and comes into range once the
            tanks have followed the scout a little; a second one sits on the route east.
  chase-g   The same map, the tanks given the G key (guard area round where they stand)
            instead of an order, and watched for longer.
  scatter   Three riflemen on the G key are given an attack-move east (so the order reaches
            soldiers already on guard area) into a parked enemy vehicle on their route and,
            while they stand and fire at it, are told to scatter (the X key, which runs the
            same InfantryClass::Scatter a friendly vehicle driving through their cell does).

Tiberian Dawn's scout is a one-member team ordered to a waypoint on the north edge,
starting three cells east of the tanks. Red Alert's is a Retreat-mission jeep that starts
out of sight to the south and runs to its nearest edge, the north one, past the tanks; the
scenario's own rules override makes it sturdier, quicker and unarmed, because the 1996 jeep
dies in seconds under two tanks and one that is hit turns to fight or dodges on the spot.
The Tiberian Dawn scatter target is an unarmed vehicle, because a light tank woken by the
riflemen drives over them.
"""
import base64, hashlib, math, struct, sys
from pathlib import Path

# Weapon range of the player's tank in cells: the Medium Tank's 105mm (0x4C0 leptons) and
# the Medium Tank's 90mm in Red Alert (Range=4.75 in the 1996 rules).
TANK_RANGE = {'td': 4.75, 'ra': 4.75}
# The guard radius of that tank, twice its weapon range (TechnoClass::Threat_Range(1)).
GUARD_RADIUS = {'td': 9.5, 'ra': 9.5}


def packed(raw):
    """A MapPack section: LCW blocks of one repeated byte each."""
    blocks = bytearray()
    for pos in range(0, len(raw), 8192):
        part = raw[pos:pos + 8192]
        comp = b'\xfe' + struct.pack('<H', len(part)) + part[:1] + b'\x80'
        blocks += struct.pack('<HH', len(comp), len(part)) + comp
    t = base64.b64encode(blocks).decode()
    return '\n'.join(f'{i // 70 + 1}={t[i:i + 70]}' for i in range(0, len(t), 70))


def layout(game, name):
    """Everything a fixture places and the script that drives it."""
    ra = game == 'ra'
    tank = '2TNK' if ra else 'MTNK'
    light = '1TNK' if ra else 'LTNK'
    f = {'size': 60, 'units': [], 'infantry': [], 'extra': '', 'waypoints': {}, 'dest': None, 'home': None}
    script = []
    if name in ('chase-am', 'chase-g'):
        x, y = (47, 33) if ra else (20, 40)
        f['size'] = 100 if ra else 60
        f['home'] = (x, y + 1)
        north = (x + 1, y - 7) if ra else (x + 2, y - 5)
        f['north'] = north
        f['route'] = (x + 16, y + 1)
        f['y0'] = y
        f['units'] = [('g', tank, x, y, 'Guard', 64, 256), ('g', tank, x, y + 2, 'Guard', 64, 256),
                      ('b', light, north[0], north[1], 'Sleep', 0, 48),
                      ('b', light, f['route'][0], f['route'][1], 'Sleep', 0, 48)]
        if ra:
            f['units'].append(('b', 'JEEP', x + 3, y + 7, 'Retreat', 0, 256))
            f['extra'] = '[JEEP]\nStrength=900\nSpeed=12\nPrimary=none\n'
        else:
            f['units'].append(('b', 'BGGY', x + 3, y + 1, 'Guard', 0, 256))
            f['waypoints'] = {1: (x + 3, 1)}
            f['extra'] = ('[TeamTypes]\nrun=BadGuy,0,0,0,0,0,7,1,0,0,1,BGGY:1,1,Move:1,0,0\n'
                          '[Triggers]\ngo=Time,Create Team,0,BadGuy,run,0\n')
        # A second click on one tank selects every tank of its type on the screen, which is
        # both of them in every graphics mode; a band box covers a different area of the
        # map in each. The tick lets the renderer read the selection back from the engine
        # before the order is given.
        script += [f'cam {x} {y + 1}', f'lclickobj {tank}#0', f'dbltapobj {tank}#0', 'tick 1', 'select']
        if name == 'chase-am':
            f['dest'] = (x + 25, y + 1)
            script += [f'cam {f["dest"][0]} {f["dest"][1]}', 'amove', f'actcell {f["dest"][0]} {f["dest"][1]}']
        else:
            script += ['guard']
        script += ['tick 1', 'seldump']
        # The G key runs longer: in Tiberian Dawn the original routine follows the scout to
        # the north edge before it walks home.
        script += ['tick 15', 'objdump'] * (170 if name == 'chase-g' else 100)
    elif name == 'scatter':
        f['infantry'] = [('g', 'E1', 24, 40, 'Guard', 0), ('g', 'E1', 24, 41, 'Guard', 0), ('g', 'E1', 24, 42, 'Guard', 0)]
        f['units'] = [('b', '1TNK', 31, 41, 'Sleep', 0, 96)] if ra else [('b', 'MHQ', 31, 41, 'Sleep', 0, 128)]
        f['dest'] = (44, 41)
        # On the G key first, so the attack-move reaches soldiers already on guard area,
        # whose mission does not restart: it must still be read as an attack-move.
        script += ['cam 24 41', 'lclickobj E1#0', 'dbltapobj E1#0', 'tick 1', 'select',
                   'guard', 'tick 20', 'cam 44 41', 'amove', 'actcell 44 41', 'tick 1', 'seldump']
        script += ['tick 15', 'objdump'] * 20
        script += ['echo SCATTER-NOW', 'objdump', 'scatter', 'tick 1']
        script += ['tick 15', 'objdump'] * 80
    else:
        raise SystemExit(f'unknown fixture {name}')
    return f, script


def write(d, game, name, gfx='1'):
    ra = game == 'ra'
    f, body = layout(game, name)
    d = Path(d)
    d.mkdir(parents=True, exist_ok=True)
    good, bad = ('Greece', 'USSR') if ra else ('GoodGuy', 'BadGuy')
    stride = 128 if ra else 64
    cell = lambda x, y: y * stride + x
    size = f['size']
    U = [f'{i}={good if u[0] == "g" else bad},{u[1]},{u[6]},{cell(u[2], u[3])},{u[5]},{u[4]},None'
         for i, u in enumerate(f['units'])]
    I = [f'{i}={good if u[0] == "g" else bad},{u[1]},256,{cell(u[2], u[3])},{u[5]},{u[4]},0,None'
         for i, u in enumerate(f['infantry'])]
    B = [f'0={good},FACT,256,{cell(5, size - 5)},0,None', f'1={bad},FACT,256,{cell(size - 5, size - 5)},0,None']
    wp = {0: cell(size // 2, size // 2)}
    for k, (x, y) in f['waypoints'].items():
        wp[k] = cell(x, y)
    houses = ''
    for h, credits in ((good, 10000), (bad, 1000)):
        houses += (f'[{h}]\nCredits={credits}\nMaxBuilding=150\nMaxUnit=150\nMaxInfantry=150\n'
                   f'MaxAircraft=150\nTechLevel=10\nAllies={h}\n')
    text = (f'[Basic]\nName=attack-move fixture\nPlayer={good}\nBuildLevel=98\nNewINIFormat=3\n'
            f'[Map]\nTheater=TEMPERATE\nX=1\nY=1\nWidth={size}\nHeight={size}\n' + houses
            + '[Waypoints]\n' + '\n'.join(f'{k}={v}' for k, v in sorted(wp.items()))
            + '\n[Structures]\n' + '\n'.join(B) + '\n[Units]\n' + '\n'.join(U)
            + '\n[Infantry]\n' + '\n'.join(I) + '\n' + f['extra'])
    if ra:
        text += '[MapPack]\n' + packed(b'\xff' * (128 * 128 * 2) + b'\0' * (128 * 128)) + '\n'
    else:
        (d / 'SCG88EA.BIN').write_bytes(b'\0\0' * (64 * 64))
    (d / 'SCG88EA.INI').write_text(text)
    lines = [f'gfxmode {gfx}', 'frames 1', 'tick 5', 'zoom min'] + body + ['quit']
    (d / 'script.txt').write_text('\n'.join(lines) + '\n')


def dumps(log):
    """[(frame, [object dict, ...]), ...] for every objdump in the log, plus the index of
    the first dump after an `echo SCATTER-NOW` (or None)."""
    out, cur, mark = [], [], None
    for line in open(log, errors='replace'):
        if line.startswith('OBJ|') and '|lx=' in line:
            p = line.rstrip('\n').split('|')
            o = dict(x.split('=', 1) for x in p if '=' in x)
            o['kind'], o['type'], o['house'] = p[1], p[2], p[3]
            cur.append(o)
        elif line.startswith('OBJDUMP-END'):
            out.append((int(line.split('frame=')[1].split()[0]), cur))
            cur = []
        elif line.startswith('ECHO|') and 'SCATTER-NOW' in line and mark is None:
            mark = len(out)
    return out, mark


def mobile(objs):
    return [o for o in objs if o['kind'] in ('UNIT', 'INFANTRY') and o.get('limbo') != '1']


def mine(o):
    return o['house'] in ('GoodGuy', 'Greece')


def pos(o):
    """Centre of the object in cells (leptons / 256)."""
    return int(o['lx']) / 256.0, int(o['ly']) / 256.0


def cdist(a, cellxy):
    return math.hypot(a[0] - cellxy[0] - 0.5, a[1] - cellxy[1] - 0.5)


def tar_cell(o):
    t = int(o['tar'])
    return None if t < 0 else (t % 128, t // 128)


def measure(log, game, name):
    f, _ = layout(game, name)
    D, mark = dumps(log)
    if not D:
        print('dumps=0 fail')
        return False
    last = D[-1][1]
    res, ok = [], True
    if name in ('chase-am', 'chase-g'):
        tank = '2TNK' if game == 'ra' else 'MTNK'
        scout = 'JEEP' if game == 'ra' else 'BGGY'
        light = '1TNK' if game == 'ra' else 'LTNK'
        tanks = {}
        for fr, objs in D:
            for o in mobile(objs):
                if mine(o) and o['type'] == tank:
                    t = tanks.setdefault(o['id'], {'north': 0.0, 'far': 0.0, 'end': None})
                    p = pos(o)
                    t['north'] = max(t['north'], f['y0'] - p[1] + 0.5)
                    t['far'] = max(t['far'], cdist(p, f['home']))
                    t['end'] = (p, o['mission'])
        # The two sleepers, by the id each has in the first dump on its starting cell.
        ids = {(int(o['x']), int(o['y'])): o['id'] for o in mobile(D[0][1]) if not mine(o) and o['type'] == light}
        alive = lambda objs, cellxy: any(not mine(o) and o['type'] == light and o['id'] == ids.get(cellxy)
                                         for o in mobile(objs))
        north_gone = f['north'] in ids and not alive(last, f['north'])
        route_gone = f['route'] in ids and not alive(last, f['route'])
        if name != 'chase-g':
            # At the first dump in which a tank aims at the north sleeper: is the scout still
            # on the map and out of that tank's weapon range, and the sleeper inside it?
            switch = 'never'
            for fr, objs in D:
                ms = mobile(objs)
                sc = [o for o in ms if not mine(o) and o['type'] == scout]
                hit = [o for o in ms if mine(o) and o['type'] == tank and tar_cell(o) == f['north']]
                if hit:
                    t = hit[0]
                    if sc:
                        ds = math.hypot(pos(t)[0] - pos(sc[0])[0], pos(t)[1] - pos(sc[0])[1])
                        dn = cdist(pos(t), f['north'])
                        switch = f'f{fr}:scout={ds:.1f}:north={dn:.1f}'
                        sw_ok = ds > TANK_RANGE[game] and dn <= TANK_RANGE[game] + 0.5
                    else:
                        switch = f'f{fr}:scout-gone'
                        sw_ok = False
                    break
            else:
                sw_ok = False
            res.append(f'switch={switch}')
            ok &= sw_ok
            for i, t in sorted(tanks.items()):
                d = cdist(t['end'][0], f['dest'])
                res.append(f'{tank}#{i}:north={t["north"]:.1f}:end_to_click={d:.1f}:{t["end"][1].replace(" ", "")}')
                ok &= t['north'] <= GUARD_RADIUS[game] + 1.0 and d <= 2.0 and t['end'][1] == 'Area Guard'
            res.append(f'north_sleeper_gone={int(north_gone)} route_sleeper_gone={int(route_gone)}')
            ok &= north_gone and route_gone
            ok &= len(tanks) == 2
        else:
            for i, t in sorted(tanks.items()):
                d = cdist(t['end'][0], f['home'])
                res.append(f'{tank}#{i}:farthest_from_home={t["far"]:.1f}:end_to_home={d:.1f}:{t["end"][1].replace(" ", "")}')
                # It gives chase (the G key engaged) and ends at home on Area Guard. How far
                # the chase goes is each game's original routine, measured on a brain with the
                # attack-move branch switched off: Red Alert's leash brings the tanks back
                # within 8 to 10 cells; Tiberian Dawn's leash waits for the chase to have no
                # destination, which a target that keeps running never gives it, so its tanks
                # follow the scout 31 to 32 cells to the north edge and only then walk home.
                # An attack-move rule leaking into the G key caps the Tiberian Dawn chase
                # near the guard radius instead, and that fails here.
                far_ok = t['far'] <= 12.0 if game == 'ra' else t['far'] >= GUARD_RADIUS[game] + 10.0
                ok &= 3.0 <= t['far'] and far_ok and d <= 1.5 and t['end'][1] == 'Area Guard'
            ok &= len(tanks) == 2
    elif name == 'scatter':
        firing = 0
        if mark is not None and mark < len(D):
            firing = sum(1 for o in mobile(D[mark][1]) if mine(o) and o['type'] == 'E1'
                         and int(o['tar']) >= 0 and int(o['nav']) < 0)
        res.append(f'standing_and_firing_at_scatter={firing}')
        ends = [o for o in mobile(last) if mine(o) and o['type'] == 'E1']
        for o in sorted(ends, key=lambda o: int(o['id'])):
            d = cdist(pos(o), f['dest'])
            res.append(f'E1#{o["id"]}:end_to_click={d:.1f}:{o["mission"].replace(" ", "")}')
            ok &= d <= 1.5 and o['mission'] == 'Area Guard'
        ok &= firing >= 1 and len(ends) >= 2
        res.append(f'alive={len(ends)}')
    print(' '.join(res) + (' pass' if ok else ' fail'))
    return ok


def digest(log):
    h = hashlib.sha1()
    for line in open(log, errors='replace'):
        if line.startswith('OBJ|UNIT|') or line.startswith('OBJ|INFANTRY|'):
            h.update('|'.join(x for x in line.split('|') if not x.startswith('sel=')).encode())
    print(h.hexdigest()[:16])


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'write':
        write(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5] if len(sys.argv) > 5 else '1')
    elif cmd == 'measure':
        sys.exit(0 if measure(sys.argv[2], sys.argv[3], sys.argv[4]) else 1)
    elif cmd == 'digest':
        digest(sys.argv[2])
    else:
        raise SystemExit(__doc__)
