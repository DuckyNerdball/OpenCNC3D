#!/usr/bin/env python3
"""G415: a drag box takes the player's own units, without harvesters and MCVs.

    gate_bandtidy.py script DIR     write DIR/ra.txt and DIR/td.txt
    gate_bandtidy.py check DIR      read DIR/ra.log and DIR/td.log; print "1 msg" or "0 msg"

The renderer picks a band against the drawn models and selects what it caught one object at
a time, then asks the engine for the end of its own band select (OpenCNC3D_Band_Done, the
end of DisplayClass::Select_These): of a mix of the player's units and others only the
player's stay, and a box that is not added to the selection drops harvesters and MCVs
unless they are all it caught.

Red Alert runs on G345's private copy of Allied 5, where an Allied row and a Soviet row of
vehicles (an Ore Truck in each) stand side by side: one box over both rows keeps the Allied
row less its Ore Truck; a box round the Allied Ore Truck alone takes it; a SHIFT box adds it
to what is selected; and a box over the Soviet row alone takes it, as in 1996, where boxing
another side's units shows them. Tiberian Dawn runs on GDI 3, whose opening view holds the
player's MCV among his units: a box over the whole view leaves the MCV out.

The pixel boxes are the camera's view of those cells at 1280x720 (cam 24 66, dist 1700); a
camera that moves them fails the gate rather than passing it on an empty box.
"""
import os
import re
import sys

RA = """tick 2
reveal 1
dist 1700
cam 24 66
ra3dprobe
band 0 0 1279 719
select
bandoff
band 640 180 740 280
select
bandoff
band 400 180 560 280
shiftband 640 180 740 280
select
bandoff
band 350 460 780 640
select
bandoff
"""

TD = """tick 2
reveal 1
band 0 0 1279 719
select
bandoff
"""


def selections(lines):
    """Each `select` dump as a list of (type, id)."""
    out = []
    for l in lines:
        if re.match(r'^SELECT\|\d+ selected:', l):
            out.append([(m.group(1), int(m.group(2)))
                        for m in re.finditer(r' ([A-Z0-9]+)#(\d+)@', l)])
    return out


def tidies(lines):
    return [tuple(int(x) for x in re.findall(r'=(\d+)', l)) for l in lines
            if l.startswith('BANDTIDY|')]


def houses(lines):
    h = {}
    for l in lines:
        m = re.match(r'^RA3DPROBE\|vehicle\|([A-Z0-9]+)\|id=(\d+)\|.*\|house=([A-Za-z]+)\|', l)
        if m:
            h.setdefault((m.group(1), int(m.group(2))), m.group(3))
    return h


def check(d):
    fails, notes = [], []
    ra = open(os.path.join(d, 'ra.log'), errors='replace').read().splitlines()
    td = open(os.path.join(d, 'td.log'), errors='replace').read().splitlines()
    sel, tid, own = selections(ra), tidies(ra), houses(ra)
    if len(sel) != 4 or len(tid) < 5:
        fails.append(f'Red Alert: {len(sel)} selection dumps (4), {len(tid)} band ends (5); '
                     'an engine without OpenCNC3D_Band_Done prints none')
    else:
        rows, alone, added, foe = sel
        side = lambda s: {own.get(u, '?') for u in s}
        caught, kept = tid[0]
        if side(rows) != {'Greece'} or any(t == 'HARV' for t, i in rows) or len(rows) < 4 \
           or caught <= kept:
            fails.append(f'Red Alert, one box over both rows: kept {rows} of {caught} caught')
        else:
            notes.append(f'Red Alert: a box over both rows keeps the {len(rows)} Allied units of '
                         f'{caught} caught, no Soviet unit and no Ore Truck')
        if [t for t, i in alone] != ['HARV'] or side(alone) != {'Greece'}:
            fails.append(f'Red Alert, the Ore Truck boxed alone: {alone}')
        if sorted(t for t, i in added) != ['HARV', 'V2RL'] or side(added) != {'Greece'}:
            fails.append(f'Red Alert, the Ore Truck added with SHIFT: {added}')
        if not foe or side(foe) != {'USSR'}:
            fails.append(f'Red Alert, the Soviet row boxed alone: {foe}')
        if not [f for f in fails if f.startswith('Red Alert, the')]:
            notes.append('the Ore Truck boxed alone is taken, added with SHIFT is kept, and '
                         'the Soviet row boxed alone is taken')
    sel, tid = selections(td), tidies(td)
    if len(sel) != 1 or len(tid) != 1:
        fails.append(f'Tiberian Dawn: {len(sel)} selection dumps, {len(tid)} band ends')
    else:
        caught, kept = tid[0]
        types = [t for t, i in sel[0]]
        if 'MCV' in types or caught != kept + 1 or kept < 4:
            fails.append(f'Tiberian Dawn, GDI 3\'s opening view boxed: {kept} kept of {caught} '
                         f'caught: {types}')
        else:
            notes.append(f'Tiberian Dawn: GDI 3\'s opening view boxed keeps {kept} of {caught}, '
                         'the MCV left out')
    if fails:
        print('0 ' + '; '.join(fails))
    else:
        print('1 a drag box takes the player\'s own units without Ore Trucks and MCVs: '
              + '; '.join(notes))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'script':
        os.makedirs(sys.argv[2], exist_ok=True)
        open(os.path.join(sys.argv[2], 'ra.txt'), 'w').write(RA)
        open(os.path.join(sys.argv[2], 'td.txt'), 'w').write(TD)
    elif len(sys.argv) == 3 and sys.argv[1] == 'check':
        check(sys.argv[2])
    else:
        sys.exit(__doc__)
