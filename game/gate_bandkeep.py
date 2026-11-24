#!/usr/bin/env python3
"""G459: a drag box that catches nothing it can select keeps the selection.

    gate_bandkeep.py script DIR     write DIR/td.txt and DIR/ra.txt
    gate_bandkeep.py check DIR      read DIR/td.log and DIR/ra.log; print "1 msg" or "0 msg"

1995 and 1996 clear the selection before a band sweeps (Select_These calls Unselect_All
first), so a box dragged over bare ground, often an order click that drifted, dropped
whatever was selected. Here a fresh band whose candidates the engine would all refuse
(OpenCNC3D_Band_Would_Take, asked without selecting) leaves the selection alone; a band
that catches something still replaces it, and the right button still deselects.

Each game runs in N64, Enhanced and Classic, every leg in each mode:

  Tiberian Dawn, GDI's test map (SCG90EA), the first Medium Tank selected:
    empty    a 30 px left drag from bare ground (cell 56,49): a band, kept;
    shift    the same drag with SHIFT: unchanged;
    foe      a box round a Nod Light Tank alone (Tiberian Dawn's band never takes another
             side's units): kept;
    replace  a box round the second Medium Tank: the selection is that tank;
    cancel   the first tank again, then the right button on the map: nothing selected.

  Red Alert, G345's private copy of Allied 5 (an Allied and a Soviet row of vehicles),
  the Allied V2 (id 19) selected:
    empty, shift  as above, from cell 25,66;
    loaner   a box round the mission's own arriving Transport alone: the renderer draws it
             and counts it, the engine refuses it (a reinforcement loaner), so kept. This
             is the leg that proves the answer is the engine's and not a copy of its tests (in
             Classic taken 45 ticks later, once the Transport is on the page);
    replace  a box round the Soviet V2 (id 26): the selection is that V2, as 1996 shows
             another side's units boxed alone;
    cancel   as above.

The boxes round objects come from `bandobj`, centred on the object's drawn silhouette, so
they hold in every mode and at every camera.
"""
import os
import re
import sys

MODES = [(1, 'N64'), (2, 'Enhanced'), (0, 'Classic')]

# The Transport starts its run in off the map's west edge. The 3D cameras see past the
# edge, but Classic's page stops at it: at the first two legs' moment the Transport's
# centre is 24 px left of the page, where no box a player can draw reaches (a box is held
# to the page). So Classic's loaner leg waits 45 ticks first, by which time the Transport
# is sailing in on the page (centre about 140 px from its left edge) and the engine still
# refuses it; it lands and becomes selectable only after about 105 ticks. The other modes
# take the leg at once, as before.
RA_LOANER_WAIT = {1: '', 2: '', 0: 'tick 45\n'}

TD_MODE = """gfxmode {m}
tick 1
echo G459-{m}
lclickobj MTNK 0
echo G459-{m}-empty
lgesture cell 56 49 30 30
select
echo G459-{m}-shift
lgesture cell 56 49 30 30 s
select
echo G459-{m}-foe
bandobj LTNK 0 14
select
echo G459-{m}-replace
bandobj MTNK 1 12
select
echo G459-{m}-cancel
lclickobj MTNK 0
rbutton 400 600
select
bandoff
"""

RA_MODE = """gfxmode {m}
tick 1
cam 24 66
echo G459-{m}
bandobj V2RL#19 0 12
echo G459-{m}-empty
lgesture cell 25 66 30 30
select
echo G459-{m}-shift
lgesture cell 25 66 30 30 s
select
bandobj V2RL#19 0 12
cam 16 50
{wait}echo G459-{m}-loaner
bandobj LST 0 16
select
cam 24 66
echo G459-{m}-replace
bandobj V2RL#26 0 12
select
echo G459-{m}-cancel
bandobj V2RL#19 0 12
rbutton 400 600
select
bandoff
"""


def script(d):
    os.makedirs(d, exist_ok=True)
    td = 'tick 20\ncam 54 51\nzoom min\n' + ''.join(TD_MODE.format(m=m) for m, _ in MODES)
    ra = 'tick 2\nreveal 1\ndist 1700\n' + ''.join(RA_MODE.format(m=m, wait=RA_LOANER_WAIT[m])
                                                   for m, _ in MODES)
    open(os.path.join(d, 'td.txt'), 'w').write(td + 'quit\n')
    open(os.path.join(d, 'ra.txt'), 'w').write(ra + 'quit\n')


def legs(lines):
    """{ 'M-leg': [lines] } from the echo markers."""
    out, cur = {}, None
    for l in lines:
        m = re.match(r'^ECHO\|G459-(\d)(?:-([a-z]+))?$', l)
        if m:
            cur = m.group(1) + ('-' + m.group(2) if m.group(2) else '')
            out[cur] = []
        elif cur is not None:
            out[cur].append(l)
    return out


def sel(ls):
    """The last `select` dump in a leg, as a list of TYPE#id."""
    got = None
    for l in ls:
        if re.match(r'^SELECT\|\d+ selected:', l):
            got = re.findall(r' ([A-Z0-9]+#\d+)@', l)
    return got


def band(ls):
    for l in ls:
        if l.startswith('BAND|'):
            return l
    return None


def check_game(name, lines, first, kept_leg, kept_note, replaced, fails, notes):
    lg = legs(lines)
    for m, mode in MODES:
        k = lambda leg: lg.get(f'{m}-{leg}', [])
        bad = []
        # the drift past the threshold is a band, it caught nothing, the selection stays
        b = band(k('empty'))
        if not any(l.startswith('LGESTURE|') and l.endswith('|band=1') for l in k('empty')) \
           or not b or not re.search(r'\|empty\|candidates=0\|kept=1$', b) or sel(k('empty')) != [first]:
            bad.append(f'empty ground: {b} -> {sel(k("empty"))}')
        b = band(k('shift'))
        if not b or not re.search(r'\|took=0\|selection=1$', b) or sel(k('shift')) != [first]:
            bad.append(f'SHIFT over empty ground: {b} -> {sel(k("shift"))}')
        b = band(k(kept_leg))
        want = r'\|empty\|candidates=0\|kept=1$' if kept_leg == 'foe' \
            else r'\|empty\|candidates=[1-9]\d*\|kept=1$'
        if not b or not re.search(want, b) or sel(k(kept_leg)) != [first]:
            bad.append(f'{kept_note}: {b} -> {sel(k(kept_leg))}')
        b = band(k('replace'))
        if not b or not re.search(r'\|took=[1-9]\d*\|selection=1$', b) or sel(k('replace')) != [replaced]:
            bad.append(f'a box round {replaced}: {b} -> {sel(k("replace"))}')
        if not any(l.startswith('CANCEL|deselect|was=1|now=0') for l in k('cancel')) \
           or sel(k('cancel')) != []:
            bad.append(f'the right button: {sel(k("cancel"))}')
        if bad:
            fails.append(f'{name} {mode}: ' + '; '.join(bad))
    if not [f for f in fails if f.startswith(name)]:
        notes.append(f'{name}, in N64, Enhanced and Classic: {first} stays selected after a '
                     f'box over bare ground, with SHIFT, and over {kept_note}; a box round '
                     f'{replaced} takes it; the right button still deselects')


def check(d):
    fails, notes = [], []
    for name, log, first, kept_leg, kept_note, replaced in (
            ('Tiberian Dawn', 'td.log', 'MTNK#3', 'foe', 'a Nod Light Tank alone', 'MTNK#4'),
            ('Red Alert', 'ra.log', 'V2RL#19', 'loaner',
             'the arriving Transport the engine refuses', 'V2RL#26')):
        p = os.path.join(d, log)
        if not os.path.exists(p):
            fails.append(f'{name}: no log')
            continue
        lines = open(p, errors='replace').read().splitlines()
        if not any(re.match(r'^SCRIPT\|end .*, 0 failures', l) for l in lines):
            fails.append(f'{name}: the script reported failures (a missing verb or object)')
        check_game(name, lines, first, kept_leg, kept_note, replaced, fails, notes)
    if fails:
        print('0 ' + '; '.join(fails))
    else:
        print('1 a drag box that catches nothing it can select keeps the selection: '
              + '; '.join(notes))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'script':
        script(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == 'check':
        check(sys.argv[2])
    else:
        sys.exit(__doc__)
