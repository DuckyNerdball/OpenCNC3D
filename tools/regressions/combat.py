#!/usr/bin/env python3
"""Combat fixtures: small flat missions that stage one fight each and read the engine's
own object dumps back. Written in the shape of attack_move.py, whose map writer it shares.

  combat.py write DIR GAME FIXTURE [GFXMODE]
      Writes DIR/SCG88EA.INI (and DIR/SCG88EA.BIN for Tiberian Dawn) and DIR/script.txt.
      GAME is td or ra. GFXMODE is the `gfxmode` the script starts in (0 Classic, 1 N64,
      2 Enhanced; 1 when left out).
  combat.py measure LOG GAME FIXTURE
      Prints one line of key=value measurements, then a verdict word, pass or fail.

Run a fixture through cnc_eyes with --scen SCG88EA --dir DIR/ (see gates/pending/combat.sh).

FIXTURES (each one's comment in layout() says what it stages)
  amove-field, amove-short, amove-long      attack-move
  group-attack, group-attack-moving         one attack order for a selection of four tanks
  crush, crush-force                        a tank ordered onto an enemy rifleman
  air-retaliate, air-defend                 aircraft against ground units, both sides
  arty-moving, arty-still                   artillery against a jeep (Tiberian Dawn only)
  neighbours                                enemies on guard side by side
  lowpower                                  EVA's low power warning and its end
  capture-apc, capture-apc-weap,            a captured vehicle factory made primary
  capture-apc-nod, capture-apc-ra
  civilians                                 allied civilians
  blowup                                    a destroyed house's collapse beside an enemy tank
"""
import math, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from attack_move import packed, dumps, mobile, pos  # noqa: E402

# Weapon range of the player's tank in cells (Medium Tank 105mm 0x4C0 leptons in Tiberian
# Dawn, the Medium Tank's 90mm Range=4.75 in Red Alert).
TANK_RANGE = 4.75


def mine(o):
    return o['house'] in ('GoodGuy', 'Greece')


def layout(game, name):
    ra = game == 'ra'
    tank = '2TNK' if ra else 'MTNK'
    light = '1TNK' if ra else 'LTNK'
    f = {'size': 100 if ra else 60, 'units': [], 'infantry': [], 'aircraft': [],
         'structures': [], 'extra': '', 'waypoints': {}}
    script = []
    if name == 'amove-field':
        # Four tanks attack-move 38 cells east through a field of enemies standing on
        # guard on both sides of the route, some of them at the very edge of a tank's
        # weapon range from the nearest lane.
        x, y = (30, 30) if ra else (8, 26)
        f['dest'] = (x + 38, y + 2)
        f['units'] = [('g', tank, x, y + i, 'Guard', 64, 256) for i in range(4)]
        f['units'] += [('b', light, x + 14, y + 8, 'Guard', 192, 256),
                       ('b', light, x + 26, y - 4, 'Guard', 192, 256)]
        f['infantry'] = [('b', 'E1', x + 9, y - 4, 'Guard', 0), ('b', 'E1', x + 10, y - 4, 'Guard', 0),
                         ('b', 'E1', x + 20, y + 7, 'Guard', 0), ('b', 'E3', x + 31, y - 3, 'Guard', 0),
                         ('b', 'E1', x + 34, y + 2, 'Guard', 0)]
        script += [f'cam {x} {y + 1}', f'lclickobj {tank}#0', f'dbltapobj {tank}#0', 'tick 1', 'select',
                   f'cam {f["dest"][0]} {f["dest"][1]}', 'amove', f'actcell {f["dest"][0]} {f["dest"][1]}',
                   'tick 1', 'seldump']
        script += ['tick 15', 'objdump'] * 200
    elif name in ('group-attack', 'group-attack-moving'):
        # Four tanks in a line are ordered, as one selection, to attack one enemy tank.
        x, y = (30, 30) if ra else (10, 26)
        f['units'] = [('g', tank, x, y + i, 'Guard', 64, 256) for i in range(4)]
        f['units'] += [('b', '4TNK' if ra else 'HTNK', x + 9, y + 1, 'Sleep', 192, 256)]
        # A soldier of the player's beside the enemy, so the enemy is in sight from the start.
        f['infantry'] = [('g', 'E1', x + 11, y - 1, 'Sleep', 0)]
        script += [f'cam {x} {y + 1}', f'lclickobj {tank}#0', f'dbltapobj {tank}#0', 'tick 1', 'select']
        if name == 'group-attack-moving':
            # Already driving north-east when the attack is given.
            script += [f'cam {x + 4} {y - 6}', f'actcell {x + 4} {y - 6}', 'tick 12']
        script += [f'cam {x + 9} {y + 1}', f'actclickobj {"4TNK" if ra else "HTNK"} 0', 'tick 1', 'seldump']
        script += ['tick 5', 'objdump'] * 120
    elif name in ('crush', 'crush-force'):
        # A tank (asleep, so it does not open fire first) is ordered onto an enemy
        # rifleman's cell: a plain click on the cell (crush), or the force-move click, alt
        # (crush-force). A soldier of the player's stands beside the rifleman so it is in sight.
        x, y = (30, 30) if ra else (10, 26)
        f['units'] = [('g', tank, x, y, 'Sleep', 64, 256)]
        f['infantry'] = [('b', 'E1', x + 6, y, 'Sleep', 0), ('g', 'E1', x + 6, y - 2, 'Sleep', 0)]
        mods = ' a' if name == 'crush-force' else ''
        script += [f'cam {x} {y}', f'lclickobj {tank}#0', 'tick 1', f'cam {x + 6} {y}',
                   f'actcell {x + 6} {y}{mods}', 'tick 1', 'seldump']
        script += ['tick 5', 'objdump'] * 60
    elif name == 'air-retaliate':
        # The player's attack aircraft strikes an enemy tank standing on guard with a
        # rifleman, a rocket soldier and an artillery piece beside it. Only weapons that can
        # reach a craft in flight should ever take it as a target.
        x, y = (30, 30) if ra else (20, 26)
        air = 'HIND' if ra else 'ORCA'
        if ra:
            f['units'] = [('b', '1TNK', x + 8, y, 'Guard', 192, 256), ('b', 'ARTY', x + 9, y + 2, 'Guard', 192, 256)]
            f['infantry'] = [('b', 'E1', x + 8, y + 2, 'Guard', 0), ('b', 'E3', x + 10, y, 'Guard', 0)]
            f['aircraft'] = [('g', 'HELI', x, y, 'Guard', 64)]
            air = 'HELI'
        else:
            f['units'] = [('b', 'LTNK', x + 8, y, 'Guard', 192, 256), ('b', 'ARTY', x + 9, y + 2, 'Guard', 192, 256)]
            f['infantry'] = [('b', 'E1', x + 8, y + 2, 'Guard', 0), ('b', 'E3', x + 10, y, 'Guard', 0)]
            f['aircraft'] = [('g', air, x, y, 'Guard', 64)]
        # Something of the player's beside the enemy so it is in sight from the start.
        f['infantry'] += [('g', 'E1', x + 8, y - 3, 'Sleep', 0)]
        script += [f'cam {x} {y}', f'lclickobj {air} 0', 'tick 1', 'select',
                   f'cam {x + 8} {y}', f'actclickobj {"1TNK" if ra else "LTNK"} 0', 'tick 1', 'seldump']
        script += ['tick 3', 'objdump'] * 200
    elif name == 'air-defend':
        # The PLAYER is the second house here. The first house's attack aircraft hunts the
        # player's tank, rifleman, artillery and rocket soldier, which stand on guard.
        x, y = (30, 30) if ra else (20, 26)
        f['player'] = 'b'
        if ra:
            f['units'] = [('b', '3TNK', x, y, 'Guard', 64, 256), ('b', 'V2RL', x + 2, y + 2, 'Guard', 64, 256)]
            f['infantry'] = [('b', 'E1', x + 1, y + 2, 'Guard', 0), ('b', 'E3', x + 2, y, 'Guard', 0)]
            f['aircraft'] = [('g', 'HELI', x + 14, y, 'Hunt', 192)]
            f['structures'] = [('g', 'HPAD', x + 14, y + 4)]
        else:
            f['units'] = [('b', 'LTNK', x, y, 'Guard', 64, 256), ('b', 'ARTY', x + 2, y + 2, 'Guard', 64, 256)]
            f['infantry'] = [('b', 'E1', x + 1, y + 2, 'Guard', 0), ('b', 'E3', x + 2, y, 'Guard', 0)]
            f['aircraft'] = [('g', 'ORCA', x + 14, y, 'Hunt', 192)]
            f['structures'] = [('g', 'HPAD', x + 14, y + 4)]
        script += [f'cam {x} {y}']
        script += ['tick 3', 'objdump'] * 250
    elif name in ('arty-moving', 'arty-still'):
        # The player's artillery on guard; an enemy vehicle drives past it at three to four
        # cells (arty-moving), or stands still at the same distance (arty-still).
        x, y = (30, 30) if ra else (20, 26)
        f['player'] = 'b'
        if ra:
            f['units'] = [('b', 'ARTY', x, y, 'Guard', 64, 256)]
            mover = 'JEEP'
        else:
            f['units'] = [('b', 'ARTY', x, y, 'Guard', 64, 256)]
            mover = 'JEEP'
        if name == 'arty-moving':
            f['units'].append(('g', mover, x - 12, y - 3, 'Guard', 64, 256))
            f['waypoints'] = {1: (x + 14, y - 3)}
            f['extra'] = ('[TeamTypes]\nrun=' + ('Greece' if ra else 'GoodGuy') + ',0,0,0,0,0,7,1,0,0,1,'
                          + mover + ':1,1,Move:1,0,0\n'
                          '[Triggers]\ngo=Time,Create Team,0,' + ('Greece' if ra else 'GoodGuy') + ',run,0\n')
        else:
            f['units'].append(('g', mover, x + 1, y - 3, 'Sleep', 64, 256))
        script += [f'cam {x} {y}']
        script += ['tick 2', 'objdump'] * 200
    elif name == 'neighbours':
        # Soldiers and vehicles of both sides placed on guard within a few cells of each
        # other, nobody given an order. Everyone in weapon range of an enemy it can see
        # should open fire on its own.
        x, y = (30, 30) if ra else (20, 26)
        tank, light = ('2TNK', '3TNK') if ra else ('MTNK', 'LTNK')
        f['infantry'] = [('g', 'E1', x, y, 'Guard', 0), ('b', 'E1', x + 1, y, 'Guard', 0),
                         ('g', 'E1', x, y + 4, 'Guard', 0), ('b', 'E1', x + 2, y + 4, 'Guard', 0),
                         ('g', 'E1', x + 8, y, 'Sleep', 0), ('b', 'E1', x + 8, y + 1, 'Guard', 0)]
        f['units'] = [('g', tank, x, y + 8, 'Guard', 64, 256), ('b', light, x + 3, y + 8, 'Guard', 192, 256)]
        script += [f'cam {x} {y}']
        script += ['tick 2', 'objdump'] * 150
    elif name == 'lowpower':
        # The player's base runs short of power: a construction yard, a refinery, a
        # factory and a radar, no power plant. EVA's low power warning is due, and is due
        # again every two minutes while it lasts. After about two and a half minutes the
        # player switches the three consumers off with the sidebar's power button, power
        # is no longer short, and the run goes on for about four and a half minutes more,
        # long enough for two more warnings if anything still thought power was low.
        ra = game == 'ra'
        sz = f['size']
        # Beside the construction yard, with soldiers next to them: a campaign building
        # only counts towards its owner's power once its owner has seen it.
        f['structures'] = [('g', 'PROC', 10, sz - 8), ('g', 'WEAP', 15, sz - 8), ('g', 'DOME' if ra else 'HQ', 20, sz - 8)]
        f['infantry'] = [('g', 'E1', 12, sz - 9, 'Guard', 0), ('g', 'E1', 17, sz - 9, 'Guard', 0),
                         ('g', 'E1', 21, sz - 9, 'Guard', 0), ('g', 'E1', 8, sz - 6, 'Guard', 0)]
        script += [f'cam 15 {sz - 8}', 'tick 30', 'powerdump']
        script += ['tick 300', 'powerdump'] * 7
        script += ['echo LOWPOWER-RESTORE', 'deselect', 'powermode 1', f'lclickobj PROC 0', 'tick 1',
                   f'lclickobj WEAP 0', 'tick 1', f'lclickobj {"DOME" if ra else "HQ"} 0', 'tick 1',
                   'powermode 0', 'tick 5', 'powerdump']
        script += ['tick 300', 'powerdump'] * 14
    elif name in ('capture-apc', 'capture-apc-weap'):
        # Tiberian Dawn: the player (GDI) owns a Weapons Factory and captures a Nod Airstrip
        # with an engineer, makes the airstrip the primary vehicle factory, and builds an
        # APC (a vehicle both sides may own). The APC should be delivered to the airstrip.
        sz = f['size']
        f['structures'] = [('g', 'NUKE', 9, sz - 6), ('g', 'NUKE', 11, sz - 6), ('g', 'NUKE', 13, sz - 6),
                           ('g', 'PYLE', 9, sz - 10), ('g', 'WEAP', 13, sz - 10), ('g', 'PROC', 17, sz - 6),
                           ('b', 'AFLD', 20, sz - 12)]
        f['infantry'] = [('g', 'E6', 19, sz - 13, 'Guard', 0), ('g', 'E1', 8, sz - 7, 'Guard', 0),
                         ('g', 'E1', 12, sz - 11, 'Guard', 0), ('g', 'E1', 18, sz - 7, 'Guard', 0)]
        script += [f'cam 20 {sz - 12}', 'tick 10', 'lclickobj E6 0', 'tick 1', 'actclickobj AFLD 0', 'tick 250', 'objdump']
        prim = 'AFLD' if name == 'capture-apc' else 'WEAP'
        script += ['deselect', f'lclickobj {prim} 0', 'tick 1', f'actclickobj {prim} 0', 'tick 2', 'primarydump', 'objdump']
        script += ['deselect', 'build APC', 'ready APC 6000', 'tick 5', 'objdump']
        script += ['tick 30', 'objdump'] * 40
    elif name in ('capture-apc-nod', 'capture-apc-ra'):
        # The player captures the OTHER side's vehicle factory with an engineer (Nod a GDI
        # Weapons Factory in Tiberian Dawn, the Soviets an Allied War Factory in Red Alert),
        # makes it the primary vehicle factory and builds an APC there. In Tiberian Dawn Nod
        # also owns an Airstrip, and the APC is a vehicle Nod may build; in Red Alert the APC
        # is an Allied vehicle only the captured factory can make.
        ra = game == 'ra'
        sz = f['size']
        f['player'] = 'b'
        if ra:
            f['structures'] = [('b', 'POWR', 9, sz - 6), ('b', 'POWR', 11, sz - 6), ('b', 'POWR', 13, sz - 6),
                               ('b', 'BARR', 9, sz - 10), ('b', 'WEAP', 13, sz - 10), ('b', 'PROC', 17, sz - 6),
                               ('g', 'WEAP', 20, sz - 12, 40)]
            # Red Alert's engineer only captures a building already in the red.
            eng = 'E6'
        else:
            f['structures'] = [('b', 'NUKE', 9, sz - 6), ('b', 'NUKE', 11, sz - 6), ('b', 'NUKE', 13, sz - 6),
                               ('b', 'HAND', 9, sz - 10), ('b', 'AFLD', 13, sz - 10), ('b', 'PROC', 17, sz - 6),
                               ('g', 'WEAP', 20, sz - 12)]
            eng = 'E6'
        f['infantry'] = [('b', eng, 19, sz - 13, 'Guard', 0), ('b', 'E1', 8, sz - 7, 'Guard', 0),
                         ('b', 'E1', 12, sz - 11, 'Guard', 0), ('b', 'E1', 18, sz - 7, 'Guard', 0)]
        # The captured factory is the ninth building in the INI list (two construction yards
        # come first), so its heap id is 8.
        script += [f'cam 20 {sz - 12}', 'tick 10', f'lclickobj {eng} 0', 'tick 1', 'actclickobj WEAP#8 0',
                   'tick 250', 'objdump', 'deselect', 'lclickobj WEAP#8 0', 'tick 1', 'actclickobj WEAP#8 0',
                   'tick 2', 'primarydump', 'objdump', 'deselect']
        # Red Alert's APC also needs the Allied barracks, which the Soviet player does not
        # have; the Allied light tank needs only the factory.
        built = '1TNK' if ra else 'APC'
        script += [f'build {built}', f'ready {built} 6000', 'tick 5', 'objdump']
        script += ['tick 30', 'objdump'] * 40
    elif name == 'civilians':
        # Civilians of the Neutral house allied to the player, as a campaign mission sets
        # up a village the player is protecting. The player clicks one, then clicks the
        # ground beside it (a move order if it were the player's), then gives a rifleman a
        # plain click on another civilian, then a force-fire (ctrl) on it.
        x, y = (30, 30) if ra else (20, 26)
        f['neutral_allies'] = True
        f['infantry'] = [('n', 'C1', x + 2, y, 'Guard', 0), ('n', 'C2', x + 3, y + 1, 'Guard', 0),
                         ('g', 'E1', x, y + 2, 'Guard', 0), ('g', 'E1', x + 2, y + 1, 'Sleep', 0)]
        script += [f'cam {x} {y}', 'tick 5', 'objdump', 'lclickobj C1 0', 'tick 1', 'select', 'seldump',
                   f'actcell {x + 6} {y}', 'tick 30', 'objdump', 'deselect',
                   'lclickobj E1 0', 'tick 1', 'select', 'actclickobj C2 0', 'tick 1', 'seldump', 'tick 30', 'objdump',
                   'deselect', 'lclickobj E1 0', 'tick 1', 'actclickobj C2 0 c', 'tick 1', 'seldump', 'tick 60', 'objdump']
    elif name == 'blowup':
        # A house is destroyed outright (the way a defeated or departed player's house is,
        # HouseClass::Blowup_All) while an enemy light tank stands among its soldiers, the
        # moment a tank driving over one of them would be in. The tank belongs to a house
        # that was not defeated and should come through.
        x, y = (30, 30) if ra else (20, 26)
        light = '3TNK' if ra else 'LTNK'
        f['infantry'] = [('g', 'E1', x, y, 'Sleep', 0), ('g', 'E1', x + 2, y, 'Sleep', 0), ('g', 'E1', x + 1, y + 1, 'Sleep', 0)]
        f['units'] = [('b', light, x + 1, y, 'Sleep', 192, 256)]
        script += [f'cam {x} {y}', 'tick 5', 'objdump', 'blowup 0', 'tick 1', 'objdump']
        script += ['tick 10', 'objdump'] * 20
    elif name in ('amove-short', 'amove-long'):
        # One tank attack-moves a short way (7 cells, less than its guard radius of twice
        # its weapon range) or a long way (16 cells). An enemy tank sits three cells off the
        # route, two cells along it, in sight and in weapon range from the start. A soldier of the player's
        # stands beside it so it is in sight. The tank should fight it until one of them dies.
        x, y = (30, 30) if ra else (14, 26)
        tank, light = ('2TNK', '3TNK') if ra else ('MTNK', 'LTNK')
        dist = 7 if name == 'amove-short' else 16
        f['dest'] = (x + dist, y)
        # The player's tank starts asleep so it has no target before the order is given.
        f['units'] = [('g', tank, x, y, 'Sleep', 64, 256), ('b', light, x + 2, y - 3, 'Sleep', 0, 256)]
        f['infantry'] = [('g', 'E1', x, y - 4, 'Sleep', 0)]
        script += [f'cam {x} {y}', f'lclickobj {tank} 0', 'tick 1', f'cam {f["dest"][0]} {y}', 'amove',
                   f'actcell {f["dest"][0]} {y}', 'tick 1', 'seldump']
        script += ['tick 5', 'objdump'] * 150
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
    who = lambda c: good if c == 'g' else (bad if c == 'b' else 'Neutral')
    U = [f'{i}={who(u[0])},{u[1]},{u[6]},{cell(u[2], u[3])},{u[5]},{u[4]},None'
         for i, u in enumerate(f['units'])]
    I = [f'{i}={who(u[0])},{u[1]},256,{cell(u[2], u[3])},{u[5]},{u[4]},0,None'
         for i, u in enumerate(f['infantry'])]
    A = [f'{i}={who(u[0])},{u[1]},256,{cell(u[2], u[3])},{u[5]},{u[4]}'
         for i, u in enumerate(f['aircraft'])]
    B = [f'0={good},FACT,256,{cell(5, size - 5)},0,None', f'1={bad},FACT,256,{cell(size - 5, size - 5)},0,None']
    B += [f'{i + 2}={who(s[0])},{s[1]},{s[4] if len(s) > 4 else 256},{cell(s[2], s[3])},0,None'
          for i, s in enumerate(f['structures'])]
    wp = {0: cell(size // 2, size // 2)}
    for k, (x, y) in f['waypoints'].items():
        wp[k] = cell(x, y)
    houses = ''
    hl = ((good, 10000), (bad, 1000)) + ((('Neutral', 0),) if f.get('neutral_allies') else ())
    for h, credits in hl:
        allies = h
        if f.get('neutral_allies') and h in (good, 'Neutral'):
            allies = f'{good},Neutral'
        houses += (f'[{h}]\nCredits={credits}\nMaxBuilding=150\nMaxUnit=150\nMaxInfantry=150\n'
                   f'MaxAircraft=150\nTechLevel=10\nAllies={allies}\n')
    player = bad if f.get('player') == 'b' else good
    text = (f'[Basic]\nName=combat fixture\nPlayer={player}\nBuildLevel=98\nNewINIFormat=3\n'
            f'[Map]\nTheater=TEMPERATE\nX=1\nY=1\nWidth={size}\nHeight={size}\n' + houses
            + '[Waypoints]\n' + '\n'.join(f'{k}={v}' for k, v in sorted(wp.items()))
            + '\n[Structures]\n' + '\n'.join(B) + '\n[Units]\n' + '\n'.join(U)
            + '\n[Infantry]\n' + '\n'.join(I) + '\n'
            + ('[Aircraft]\n' + '\n'.join(A) + '\n' if A else '') + f['extra'])
    if ra:
        text += '[MapPack]\n' + packed(b'\xff' * (128 * 128 * 2) + b'\0' * (128 * 128)) + '\n'
    else:
        (d / 'SCG88EA.BIN').write_bytes(b'\0\0' * (64 * 64))
    (d / 'SCG88EA.INI').write_text(text)
    lines = [f'gfxmode {gfx}', 'frames 1', 'tick 5', 'zoom min'] + body + ['quit']
    (d / 'script.txt').write_text('\n'.join(lines) + '\n')


def live(objs, kind=('UNIT', 'INFANTRY')):
    return [o for o in objs if o['kind'] in kind and o.get('limbo') != '1' and int(o['str']) > 0]


def ocell(o):
    return int(o['y']) * 128 + int(o['x'])


def first(objs, typ, house=None, oid=None):
    for o in objs:
        if o['type'] == typ and (house is None or o['house'] == house) and (oid is None or o['id'] == str(oid)):
            return o
    return None


def lines(log, prefix):
    return [l.rstrip('\n') for l in open(log, errors='replace') if l.startswith(prefix)]


def measure(log, game, name):
    """One line of measurements and a verdict. Each fixture asserts the behaviour the
    engine has to show; where the fixture only documents a 1995 or 1996 rule, that is
    said in the line and the verdict is about the rule holding, not about it changing."""
    ra = game == 'ra'
    D, _ = dumps(log)
    if not D and name != 'lowpower':
        print('dumps=0 fail')
        return False
    f, _ = layout(game, name)
    good, bad = ('Greece', 'USSR') if ra else ('GoodGuy', 'BadGuy')
    me = bad if f.get('player') == 'b' else good
    res, ok = [], True
    last = D[-1][1] if D else []
    if name == 'amove-field':
        tank = '2TNK' if ra else 'MTNK'
        seen = set()
        for fr, objs in D:
            tanks = [o for o in live(objs) if o['house'] == me and o['type'] == tank]
            for e in live(objs):
                if e['house'] == me:
                    continue
                if any(math.hypot(pos(t)[0] - pos(e)[0], pos(t)[1] - pos(e)[1]) <= 3.0 for t in tanks):
                    seen.add(e['type'] + '#' + e['id'])
        alive = {o['type'] + '#' + o['id'] for o in live(last) if o['house'] != me}
        missed = sorted(seen & alive)
        ends = [o for o in live(last) if o['house'] == me and o['type'] == tank]
        far = [round(math.hypot(pos(o)[0] - f['dest'][0] - 0.5, pos(o)[1] - f['dest'][1] - 0.5), 1) for o in ends]
        res.append(f'met={len(seen)} left_alive={",".join(missed) or "none"} tanks={len(ends)} end_to_click={far}')
        ok = len(seen) >= 2 and not missed and len(ends) == 4 and all(d <= 3.0 for d in far) \
            and all(o['mission'] == 'Area Guard' for o in ends)
    elif name in ('group-attack', 'group-attack-moving'):
        tank, heavy = ('2TNK', '4TNK') if ra else ('MTNK', 'HTNK')
        sel = [l for l in lines(log, 'SEL|UNIT|' + tank)]
        target = None
        e0 = first(D[0][1], heavy)
        if e0:
            target = ocell(e0)
        ordered = sum(1 for l in sel if l.endswith(f'tarcell={target}'))
        # Five dumps (25 engine ticks) after the order: does every tank still hold it?
        held = sum(1 for o in live(D[min(4, len(D) - 1)][1]) if o['type'] == tank and o['tar'] == str(target))
        dead = first(live(last), heavy) is None
        res.append(f'ordered={ordered} still_holding_after_25_ticks={held} enemy_destroyed={int(dead)}')
        ok = ordered == 4 and held == 4 and dead
    elif name in ('crush', 'crush-force'):
        vic = [first(live(objs), 'E1', bad if me == good else good) for _, objs in D]
        present = [v for v in vic if v]
        shot = any(int(v['str']) < 50 for v in present)
        gone = vic[-1] is None
        res.append(f'rifleman_gone={int(gone)} rifleman_was_shot={int(shot)}')
        ok = gone and not shot if name == 'crush-force' else True
    elif name in ('air-retaliate', 'air-defend'):
        # A target cell names a craft in flight when it lies within two cells of one (the
        # engine aims at the craft's drawn position, lifted by its altitude) and no ground
        # object stands on it.
        noaa = {'td': ('LTNK', 'MTNK', 'ARTY', 'E1'), 'ra': ('1TNK', '3TNK', 'ARTY', 'V2RL', 'E1')}[game]
        wrong, right = set(), 0
        for fr, objs in D:
            air = [(int(o['x']), int(o['y'])) for o in objs if o['kind'] == 'AIRCRAFT' and o.get('limbo') != '1']
            ground = {ocell(o) for o in live(objs)}
            for o in live(objs):
                t = int(o['tar'])
                if t < 0 or t in ground:
                    continue
                tx, ty = t % 128, t // 128
                if any(math.hypot(tx - ax, ty - ay) <= 2.0 for ax, ay in air):
                    if o['type'] in noaa:
                        wrong.add(o['type'])
                    elif o['type'] == 'E3':
                        right += 1
        res.append(f'non_antiair_aiming_at_aircraft={",".join(sorted(wrong)) or "none"} rocket_soldier_dumps_on_aircraft={right}')
        ok = not wrong and right > 0
    elif name == 'lowpower':
        before = after = 0
        restored = False
        for l in open(log, errors='replace'):
            if l.startswith('ECHO|') and 'LOWPOWER-RESTORE' in l:
                restored = True
            elif l.startswith('SOUND|eva|') and 'LOPOWER' in l:
                if restored:
                    after += 1
                else:
                    before += 1
        pw = lines(log, 'POWER|watts=')
        final = pw[-1].split('|')[1] if pw else 'none'
        res.append(f'low_power_lines_before={before} after_power_restored={after} final_{final}')
        p, d = (int(v) for v in final.split('=')[1].split('/')) if pw else (0, 1)
        ok = before >= 1 and after == 0 and p >= d
    elif name in ('capture-apc', 'capture-apc-weap', 'capture-apc-nod', 'capture-apc-ra'):
        built = '1TNK' if ra else 'APC'
        fac = {'capture-apc': ('AFLD', '8'), 'capture-apc-weap': ('WEAP', '6'),
               'capture-apc-nod': ('WEAP', '8'), 'capture-apc-ra': ('WEAP', '8')}[name]
        b = first(last, fac[0], me, fac[1])
        u = first(live(last), built, me)
        prim = any(l.startswith(f'PRIMARY-FLAG|{fac[0]}|{me}|id={fac[1]}|primary=1') for l in open(log, errors='replace'))
        if b and u:
            bx, by = int(b['x']) + 1, int(b['y']) + 1
            d = math.hypot(int(u['x']) - bx, int(u['y']) - by)
        else:
            d = 99
        res.append(f'factory_owned={int(b is not None)} primary={int(prim)} {built}_to_factory={d:.1f}')
        # A vehicle leaves a factory from its door; one delivered to an airstrip is set down
        # beside it. Either way it ends within four cells of the factory's centre.
        ok = b is not None and prim and d <= 4.0
    elif name == 'neighbours':
        # The adjacent riflemen of the first pair: both must have been hit.
        x, y = (30, 30) if ra else (20, 26)
        a0 = first(D[0][1], 'E1', good, 0)
        b0 = first(D[0][1], 'E1', bad, 1)
        hurt = lambda oid, h: any((first(objs, 'E1', h, oid) is None) or int(first(objs, 'E1', h, oid)['str']) < 50
                                   for _, objs in D)
        ha, hb = hurt(0, good), hurt(1, bad)
        res.append(f'adjacent_pair_fought={int(ha and hb)}')
        ok = ha and hb and a0 is not None and b0 is not None
    elif name == 'civilians':
        # A click on an allied civilian selects it (it can be inspected, as in 1995 and
        # 1996) and never makes the player's rifleman fire at it; only a force-fire does.
        sel = [l for l in open(log, errors='replace') if l.startswith('SELECT|1 selected: C1')]
        c2 = [first(objs, 'C2') for _, objs in D]
        e1 = [first(objs, 'E1', me, 2) for _, objs in D]
        plain_tar = e1[2]['tar'] if len(e1) > 2 and e1[2] else '?'
        alive = len(c2) > 2 and c2[2] is not None and int(c2[2]['str']) > 0
        res.append(f'civilian_selectable={int(bool(sel))} rifleman_target_after_plain_click={plain_tar} '
                   f'civilian_alive_after_plain_click={int(alive)}')
        ok = bool(sel) and plain_tar == '-1' and alive
    elif name in ('arty-moving', 'arty-still'):
        # Tiberian Dawn only (the moving target is a scripted team). Documents the 1995
        # rule rather than a fix: a turretless gun fires only when not turning and its body
        # is within 8 of 256 facing steps of the target (TurretClass::Can_Fire), and an
        # arcing shell flies to where the target was. Wanted: the still jeep is hit.
        j = [first(objs, 'JEEP') for _, objs in D]
        hit = any(o is None or int(o['str']) < int(o['max']) for o in j)
        aimed = sum(1 for _, objs in D for o in live(objs) if o['type'] == 'ARTY' and o['tar'] != '-1')
        res.append(f'jeep_hit={int(hit)} dumps_with_artillery_aiming={aimed}')
        ok = hit if name == 'arty-still' else True
    elif name == 'blowup':
        light = '3TNK' if ra else 'LTNK'
        alive = first(live(last), light) is not None
        res.append(f'other_house_tank_survives={int(alive)}')
        ok = alive
    else:
        res.append('no measurement for this fixture')
        ok = False
    print(' '.join(res) + (' pass' if ok else ' fail'))
    return ok


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'write':
        write(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5] if len(sys.argv) > 5 else '1')
    elif cmd == 'measure':
        sys.exit(0 if measure(sys.argv[2], sys.argv[3], sys.argv[4]) else 1)
    else:
        raise SystemExit(__doc__)
