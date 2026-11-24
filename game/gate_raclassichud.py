#!/usr/bin/env python3
"""Red Alert's mission timer on the sidebar, and the build clock's darkening in Classic.

  gate_raclassichud.py script DIR   write the four run scripts into DIR
  gate_raclassichud.py check DIR    read DIR/<scen>.log and the shots and the staged packs
                                    (ra3d/sidebar-*.pack, from the play folder) and print
                                    '1 <summary>' or '0 <what failed>'

THE RUNS, 1280 x 720, hidden, from the play folder, Classic unless said:
  SCG08EA  Allied 8 starts a 45-minute timer and owns a Barracks. Build a Rifle Infantry,
           shoot, then ask for the timer, the sidebar and the Rocket Soldier's rectangle;
           then the same moment under Enhanced with its HUD, shot and asked again.
  SCG47EA  Negotiations starts a 5-minute timer: run it under two minutes, the lit plate.
  SCG41EA  In the Nick of Time starts a 90-minute timer: the form with hours.
  SCG01EA  Allied 1 has no timer: the control.
A run whose scenario is not staged is left out and said so.

THE CLAIMS:
  packs    every staged ra3d/sidebar-*.pack: the clock lookup claims only GREEN; the fade
           leaves 0 and 241..255 alone and sends 1..240 into the grey ramp 240..254
           (Conquer_Build_Fading_Table); composed over every cameo, CLOCK frame 0 brightens
           no pixel by more than 32 levels of luma.
  SCG08EA  Classic: the plate's rectangle, directly left of the credits, is at least 60
           percent grey metal; the Rocket Soldier is busy (its factory builds the Rifle
           Infantry) and the inside of its cameo has at most 0.5 percent of pixels brighter
           than 150 luma. SBTIMER: the engine's seconds are 2640..2700, the frame drew
           exactly that, in the "Time:MM:SS" form, unlit, on the Classic panel.
           Enhanced: the frame drew the same seconds on the HUD's own panel, and the header
           plate left of the credits header is filled and carries its white text.
  SCG47EA  1..119 seconds, drawn and lit; the plate at least 50 percent red.
  SCG41EA  the text is "Time:01:MM:SS".
  SCG01EA  the engine reports -1 and nothing was drawn.
The pictures are judged whether or not the run could say what it drew, so a build without
the timer fails on the plate itself and not only on a missing line.
"""
import math
import os
import re
import struct
import sys

RUNS = {
    'SCG08EA': ['gfxmode 0', 'tick 30', 'build E1', 'tick 40', 'frames 2', 'shot {d}/SCG08EA.png',
                'timerdump', 'sidebar raclassic', 'sbitem E3', 'sbitem E1',
                'gfxmode 2', 'hud 1', 'frames 2', 'shot {d}/SCG08EA_enh.png', 'timerdump', 'quit'],
    'SCG47EA': ['gfxmode 0', 'tick 2730', 'frames 2', 'shot {d}/SCG47EA.png', 'timerdump', 'quit'],
    'SCG41EA': ['gfxmode 0', 'tick 45', 'frames 2', 'shot {d}/SCG41EA.png', 'timerdump', 'quit'],
    'SCG01EA': ['gfxmode 0', 'tick 45', 'frames 2', 'shot {d}/SCG01EA.png', 'timerdump', 'quit'],
}


def write_scripts(d):
    os.makedirs(d, exist_ok=True)
    for scen, lines in RUNS.items():
        with open(os.path.join(d, scen + '.txt'), 'w') as f:
            f.write('\n'.join(l.format(d=d) for l in lines) + '\n')


def fmt(t):
    if t >= 3600:
        return 'Time:%02d:%02d:%02d' % (t // 3600, t // 60 % 60, t % 60)
    return 'Time:%02d:%02d' % (t // 60 % 60, t % 60)


def plate_rect(w, h):
    """sb_layout's Classic Red Alert arithmetic, then the timer strip at g_dbX0 - 320 ui."""
    unit = max(1, min(w // 640, h // 352))
    ui = max(1.0, min(h / 480.0, float(unit)))
    rows = min(int(math.floor(h / ui + 0.01)), 400 + 48 * 32)
    x0 = w - 160.0 * ui
    y0 = math.floor((h - rows * ui) * 0.5)
    return int(round(x0 - 320 * ui)), int(y0), int(round(x0 - 160 * ui)), int(round(y0 + 14 * ui)), ui


def pixels(png):
    from PIL import Image
    im = Image.open(png).convert('RGB')
    return im.size, im.load()


def fractions(png):
    """Grey metal and red in the timer plate's rectangle, two pixels inside its edge."""
    (w, h), px = pixels(png)
    x0, y0, x1, y1, _ = plate_rect(w, h)
    grey = red = n = 0
    for y in range(y0 + 2, y1 - 2):
        for x in range(x0 + 2, x1 - 2):
            r, g, b = px[x, y]
            n += 1
            if max(r, g, b) - min(r, g, b) <= 24 and 40 <= (r + g + b) / 3 <= 235:
                grey += 1
            if r >= 90 and r >= g + 50 and r >= b + 50:
                red += 1
    return grey / max(n, 1), red / max(n, 1)


def bright_inside(png, rect):
    """Pixels brighter than 150 luma in the middle of a cameo: 8 native pixels in from
    every side, clear of CLOCK frame 0's grey rim and of the 4 native pixels by which
    sbitem's Red Alert rectangle (the 1995 TD column x) sits left of the drawn cameo."""
    (w, h), px = pixels(png)
    _, _, _, _, ui = plate_rect(w, h)
    x0, y0, x1, y1 = rect
    m = int(math.ceil(8 * ui))
    n = b = 0
    for y in range(int(y0) + m, int(y1) - m):
        for x in range(int(x0) + m, int(x1) - m):
            r, g, bb = px[x, y]
            n += 1
            if 0.299 * r + 0.587 * g + 0.114 * bb > 150:
                b += 1
    return b / max(n, 1)


def timer_lines(log):
    """Every SBTIMER line, in order: (engine, drawn, light, text, panel)."""
    return [(int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4), m.group(5))
            for m in re.finditer(r'^SBTIMER\|engine=(-?\d+)\|drawn=(-?\d+)\|light=(\d)\|text=([^|]*)\|panel=(\w+)',
                                 log, re.M)]


# The Enhanced HUD's header plate left of the credits header, at 1280 x 720 (its plates are
# 158 x 15 from the top row; this is the plate's inside, clear of its rim).
ENH_PLATE = (754, 3, 894, 14)


def enhanced_plate(png):
    """The share of the plate's inside that is drawn (not the black under the strip), and its
    white text pixels."""
    (w, h), px = pixels(png)
    x0, y0, x1, y1 = ENH_PLATE
    n = filled = white = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b = px[x, y]
            n += 1
            filled += (r + g + b) / 3 > 15
            white += min(r, g, b) >= 200
    return filled / max(n, 1), white


def load_pack(path):
    b = open(path, 'rb').read()
    if b[:8] != b'DOSBAR01':
        raise ValueError('not a DOSBAR01 pack')
    ns = struct.unpack_from('<I', b, 12)[0]
    pal8, clk, p, shapes = b[24 + 768:24 + 1536], b[24 + 1536:24 + 2048], 24 + 2048, {}
    for _ in range(ns):
        name = b[p:p + 12].split(b'\0')[0].decode()
        fr, w, h = struct.unpack_from('<III', b, p + 12)
        p += 24
        shapes[name] = (fr, w, h, b[p:p + fr * w * h])
        p += fr * w * h
    return pal8, clk, shapes


def check_packs(paths):
    worst, bad = 0.0, []
    for path in paths:
        pal8, clk, shapes = load_pack(path)
        look, fade = clk[:256], clk[256:]
        name = os.path.basename(path)
        if [i for i in range(256) if look[i] != 0xFF] != [3] or look[3] != 0:
            bad.append(name + ': the clock lookup claims more than GREEN')
        if fade[0] != 0 or any(fade[i] != i for i in range(241, 256)):
            bad.append(name + ': the fade moves 0 or 241..255')
        out = [i for i in range(1, 241) if not 240 <= fade[i] <= 254]
        if out:
            bad.append('%s: %d fade entries outside the grey ramp (%d -> %d)' % (name, len(out), out[0], fade[out[0]]))
        _, w, h, clock = shapes['CLOCK']
        luma = [0.299 * pal8[c * 3] + 0.587 * pal8[c * 3 + 1] + 0.114 * pal8[c * 3 + 2] for c in range(256)]
        for key, (_, cw, ch, px) in shapes.items():
            if key == 'CLOCK' or (cw, ch) != (w, h):
                continue
            for i in range(w * h):
                s = clock[i]
                if s and look[s] != 0xFF and px[i]:
                    worst = max(worst, luma[fade[px[i] + look[s] * 256]] - luma[px[i]])
    if worst > 32:
        bad.append('a darkened cameo pixel brightens by %.0f' % worst)
    return worst, bad


def check(d):
    import glob
    bad, said = [], []
    packs = sorted(glob.glob('ra3d/sidebar-*.pack'))
    if not packs:
        return '0 no ra3d/sidebar-*.pack is staged'
    worst, pbad = check_packs(packs)
    bad += pbad
    said.append('%d packs, brightest darkened pixel %+.0f' % (len(packs), worst))
    logs = {}
    for scen in RUNS:
        path = os.path.join(d, scen + '.log')
        if os.path.exists(path):
            logs[scen] = open(path, errors='replace').read()
    if 'SCG08EA' not in logs:
        return '0 SCG08EA did not run'
    for scen, log in logs.items():
        lines = timer_lines(log)
        t = lines[0] if lines else None
        if t is None:
            bad.append(scen + ': no SBTIMER line')
        if scen == 'SCG01EA':
            if t and (t[0] != -1 or t[1] != -1):
                bad.append('SCG01EA: a timer where none runs (engine %d, drawn %d)' % (t[0], t[1]))
            continue
        if t:
            engine, drawn, light, text, panel = t
            if panel != 'classic95':
                bad.append('%s: the %s panel drew, not Classic' % (scen, panel))
            if drawn != engine or text != fmt(engine):
                bad.append('%s: engine %d, drawn %d as %r' % (scen, engine, drawn, text))
        if scen == 'SCG08EA':
            grey, _ = fractions(os.path.join(d, 'SCG08EA.png'))
            said.append('SCG08EA %s, plate %.0f%% metal' % (t[3] if t else '-', 100 * grey))
            if t and (not 2640 <= t[0] <= 2700 or t[2]):
                bad.append('SCG08EA: %d seconds, lit %d' % (t[0], t[2]))
            if grey < 0.6:
                bad.append('SCG08EA: the timer plate is %.0f percent metal' % (100 * grey))
            busy = re.search(r'^SBITEM\|\d+\|col=\d\|E3\|.*\|busy=(\d)', log, re.M)
            rect = re.search(r'^SBRECT\|E3\|\d+\|(-?\d+),(-?\d+)\.\.(-?\d+),(-?\d+)', log, re.M)
            if not busy or busy.group(1) != '1' or not rect:
                bad.append('SCG08EA: the Rocket Soldier is not shown busy')
            else:
                frac = bright_inside(os.path.join(d, 'SCG08EA.png'), [float(v) for v in rect.groups()])
                said.append('darkened cameo %.1f%% bright' % (100 * frac))
                if frac > 0.005:
                    bad.append('SCG08EA: the darkened Rocket Soldier has %.1f percent bright pixels' % (100 * frac))
            filled, white = enhanced_plate(os.path.join(d, 'SCG08EA_enh.png'))
            said.append('Enhanced %s, plate %.0f%% drawn, %d white' % (
                lines[1][3] if len(lines) > 1 else '-', 100 * filled, white))
            if filled < 0.7 or white < 40:
                bad.append('SCG08EA Enhanced: the header plate left of the credits is %.0f percent drawn with '
                           '%d white pixels (want >=70 and >=40)' % (100 * filled, white))
            if t and (len(lines) < 2 or lines[1][4] != 'hud' or lines[1][1] != lines[1][0]
                      or abs(lines[1][0] - t[0]) > 2):
                bad.append('SCG08EA Enhanced: the HUD did not draw the timer (%s)' % (lines[1:2] or 'no line'))
        elif scen == 'SCG47EA':
            _, red = fractions(os.path.join(d, 'SCG47EA.png'))
            said.append('SCG47EA %s lit %d, plate %.0f%% red' % (t[3] if t else '-', t[2] if t else 0, 100 * red))
            if red < 0.5 or (t and (not 1 <= t[0] <= 119 or not t[2])):
                bad.append('SCG47EA: %s seconds, lit %s, plate %.0f percent red' % (
                    t[0] if t else '-', t[2] if t else '-', 100 * red))
        elif scen == 'SCG41EA' and t:
            said.append('SCG41EA ' + t[3])
            if not t[3].startswith('Time:01:'):
                bad.append('SCG41EA: %r has no hours' % t[3])
    missing = [s for s in RUNS if s not in logs]
    if missing:
        said.append('not staged: ' + ' '.join(missing))
    return ('0 ' + '; '.join(bad)) if bad else ('1 ' + '; '.join(said))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'script':
        write_scripts(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == 'check':
        print(check(sys.argv[2]))
    else:
        sys.exit(__doc__)
