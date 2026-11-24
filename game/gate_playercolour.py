#!/usr/bin/env python3
"""The square a seat picks is the colour its army wears, in both games (the player colour gate).

  gate_playercolour.py script DIR           writes DIR/a.txt and DIR/b.txt, the Classic pair,
                                            from the probe run's DIR/probe.log
  gate_playercolour.py check DIR REPO [TD]  every claim; the first line printed is
                                            "1 <summary>" or "0 <the failures>"

REPO is the source tree (the gate passes "$GATEDIR/.."): the claims read the tables the game
was built from, so a table that drifts from another one is caught without a picture. TD is
"td" to check the Tiberian Dawn claims only (no Red Alert module staged).

Claims:
  td-squares  every Enhanced lobby square (menu/enhanced_menu.cpp colours[]) is
              LIVERY_BAND[c][2] of its own colour (game/cnc_eyes.cpp), and every 1995-style
              lobby square (menu/doslobby.c SK_PLAYER_COLOUR, in dosmenu.pack's palette) is
              within 40 of it
  td-seats    eight seats on a derangement of the eight colours: each seat was handed its
              colour, the engine gave it back (TEAMCOLOUR|paint), the body printed there is
              LIVERY_BAND[c][2], and that is the Enhanced square for c
  td-roster   the player list inks every seat in its own house's ink, orange in 26
  td-classic  two computers' yards in Classic, their colours swapped between two runs: each
              yard gains the palette entries only its new colour has and loses the old's
  ra-seats    a Red Alert match with Ukraine on the grey row and France on the blue row:
              every object of each seat's house exports the row its seat picked
  ra-classic  Soviet 1's French jeep and the French village round it, in Classic, wear the
              blue row's own entries, and more of them than the grey row's
  ra-roster   the ra-seats match's player list (Classic, 640x400): each row names the row
              its house's objects export, is inked in that row's 1996 name colour, and
              each of those colours is drawn in the list; in N64 (the 320x200 bar) every
              screen row carrying the grey row's ink, the Name:/Kills: header's among them,
              is that ink on black between its first and last ink pixel, not on frame art
"""
import re
import struct
import sys
from pathlib import Path

ROSTER_INK = [157, 203, 123, 159, 26, 135, 194, 209]
DERANGEMENT = [3, 6, 0, 5, 2, 7, 4, 1]
CLASSIC_A = [0, 4, 5, 6]          # seats 1 and 2 orange and blue in run A ...
CLASSIC_B = [0, 5, 4, 6]          # ... and blue and orange in run B
RA_ROWS = {4: 5, 6: 6}            # Ukraine on the grey row, France on the blue row
# Red Alert's name inks: Color, column 4 of each PALETTE.CPS row, in the 1996 row order
# (redalert/init.cpp Init_Color_Remaps; Fancy_Text_Print sets the font's entry 1 to it).
RA_ROSTER_INK = [84, 164, 233, 146, 212, 115, 226, 203]
RA_ROSTER_MIN = 20                # pixels of each listed row's ink in the list
# Measured on the build that paints by the seat: each yard gains 6576 and 6736 pixels of the
# entries only its new colour has (0 on a build that ignores --colours, where the two runs
# are the same picture); the French in Soviet 1 show 2790 blue-row pixels against 764
# grey-row, and 32 against 3522 on the build that painted them grey.
CLASSIC_MIN = 2000
FRENCH_MIN = 1000


def ints(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    return [int(v, 0) for v in re.findall(r'0x[0-9a-fA-F]+|\d+', text)]


def livery_band(repo):
    s = (repo / 'game/cnc_eyes.cpp').read_text(errors='replace')
    a = s.index('static const unsigned char LIVERY_BAND[LIVERY_COUNT][16][3] = {')
    a = s.index('{', a + 60)
    b = s.index('};', a)
    v = ints(s[a:b])
    assert len(v) == 8 * 16 * 3, len(v)
    return [[tuple(v[(c * 16 + k) * 3:(c * 16 + k) * 3 + 3]) for k in range(16)] for c in range(8)]


def enhanced_squares(repo):
    s = (repo / 'menu/enhanced_menu.cpp').read_text(errors='replace')
    m = re.search(r'const unsigned int colours\[\]=\{([^}]*)\}', s)
    v = ints(m.group(1))
    assert len(v) == 8, v
    return [((x >> 16) & 255, (x >> 8) & 255, x & 255) for x in v]


def dos_squares(repo, run):
    s = (repo / 'menu/doslobby.c').read_text(errors='replace')
    m = re.search(r'SK_PLAYER_COLOUR\[8\] = \{(.*?)\};', s, re.S)
    idx = ints(m.group(1))
    assert len(idx) == 8, idx
    b = (run / 'dosmenu.pack').read_bytes()
    assert b[:8] == b'DOSBAR01'
    pal = b[24 + 768:24 + 1536]
    return [tuple(pal[i * 3:i * 3 + 3]) for i in idx]


def dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


def unique_entries(band, c):
    others = set()
    for o in range(8):
        if o != c:
            others.update(band[o])
    return set(band[c]) - others


def count(png, colours, half=200, tactical=False):
    """Pixels of exactly these colours in a square of side 2*half round the frame's centre,
    which is where `cam` put the object (less what the map edge clamps away); or, with
    tactical, in the whole view left of the Classic sidebar (the right quarter of the frame),
    for a camera the map's edge has pushed off its object."""
    from PIL import Image
    im = Image.open(png).convert('RGB')
    w, h = im.size
    if tactical:
        box = (0, 0, w * 3 // 4, h)
    else:
        box = (max(0, w // 2 - half), max(0, h // 2 - half), min(w, w // 2 + half), min(h, h // 2 + half))
    data = im.crop(box).tobytes()
    return sum(1 for k in range(0, len(data), 3) if (data[k], data[k + 1], data[k + 2]) in colours)


def probe_positions(log):
    t = log.read_text(errors='replace')
    out = []
    for s in (1, 2):
        # (the seat's house, from the line that names it; a build without the paint line
        # still names it on its TEAMCOLOUR|seat line)
        m = re.search(r'^TEAMCOLOUR\|(?:paint\|)?seat=%d\|house=([^|]+)\|' % s, t, re.M)
        if not m:
            return None
        house = m.group(1)
        p = None
        for line in t.splitlines():
            if line.startswith('P|2|') and line.split('|')[3] == house and '|127|127|' not in line:
                q = re.search(r'\|wx=([-0-9.]+)\|wz=([-0-9.]+)\|', line)
                if q:
                    p = (q.group(1), q.group(2))
                    break
        if not p:
            return None
        out.append(p)
    return out


def script(d):
    pos = probe_positions(d / 'probe.log')
    if not pos:
        print('0 the probe run named no standing building for seat 1 or 2')
        return 1
    for tag in ('a', 'b'):
        lines = ['tick 900', 'gfxmode 0']
        for i, (x, z) in enumerate(pos, 1):
            lines += ['cam %s %s' % (x, z), 'shot %s/%s_%d.png' % (d, tag, i)]
        lines.append('quit')
        (d / ('%s.txt' % tag)).write_text('\n'.join(lines) + '\n')
    print('1 %s' % ' '.join('%s,%s' % p for p in pos))
    return 0


def check(d, repo, td_only):
    run = Path('.')
    fails, says = [], []
    band = livery_band(repo)
    enh = enhanced_squares(repo)

    # td-squares
    bad = [c for c in range(8) if enh[c] != band[c][2]]
    if bad:
        fails.append('td-squares: Enhanced squares %s are not their paint (%s against %s)' % (
            bad, [enh[c] for c in bad], [band[c][2] for c in bad]))
    dos = dos_squares(repo, run)
    far = [(c, round(dist(dos[c], band[c][2]), 1)) for c in range(8) if dist(dos[c], band[c][2]) > 40]
    if far:
        fails.append('td-squares: 1995-style squares far from their paint %s' % far)
    if not bad and not far:
        says.append('all 16 squares are their paint (Enhanced exact, 1995-style within %.1f)'
                    % max(dist(dos[c], band[c][2]) for c in range(8)))

    # td-seats and td-roster
    t = (d / 'seats.log').read_text(errors='replace')
    houses = set()
    seat_bad = []
    for s, c in enumerate(DERANGEMENT):
        handed = re.search(r'^skirmish: seat=%d\|.*\|colour=(\d+)\|' % s, t, re.M)
        paint = re.search(r'^TEAMCOLOUR\|paint\|seat=%d\|house=([^|]+)\|colour=(\d+)\|body=(\d+),(\d+),(\d+)$'
                          % s, t, re.M)
        if not handed or int(handed.group(1)) != c:
            seat_bad.append('seat %d handed %s, want %d' % (s, handed.group(1) if handed else 'nothing', c))
            continue
        if not paint:
            seat_bad.append('seat %d: no TEAMCOLOUR|paint line' % s)
            continue
        houses.add(paint.group(1))
        body = tuple(int(paint.group(k)) for k in (3, 4, 5))
        if int(paint.group(2)) != c or body != band[c][2] or body != enh[c]:
            seat_bad.append('seat %d wears %s body %s, square %s' % (s, paint.group(2), body, enh[c]))
    if seat_bad or len(houses) != 8:
        fails.append('td-seats: %s; %d distinct houses (want 8)' % ('; '.join(seat_bad), len(houses)))
    else:
        says.append('eight seats each wear the square they were handed')
    rows = re.findall(r'^SBROW\|\d+\|name=[^|]*\|house=\d+\|colour=(\d+)\|ink=(\d+)\|', t, re.M)
    ink_bad = [(int(c), int(i)) for c, i in rows if int(i) != ROSTER_INK[int(c)]]
    if len(rows) != 8 or ink_bad:
        fails.append('td-roster: %d rows (want 8), wrong inks (colour, ink) %s' % (len(rows), ink_bad))
    else:
        says.append('the player list inks every seat in its house ink (orange 26)')

    # td-classic
    pal_ok = True
    deltas = []
    for i, (ca, cb) in enumerate(zip(CLASSIC_A[1:3], CLASSIC_B[1:3]), 1):
        a, b = d / ('a_%d.png' % i), d / ('b_%d.png' % i)
        if not a.exists() or not b.exists():
            fails.append('td-classic: shot %s or %s missing' % (a, b))
            pal_ok = False
            continue
        ua, ub = unique_entries(band, ca), unique_entries(band, cb)
        # (the yards stand near the map's top edge, where the camera clamps and leaves
        # them off centre, so the whole view is counted)
        gain = count(a, ua, tactical=True) - count(b, ua, tactical=True)   # the colour run A gave
        lose = count(b, ub, tactical=True) - count(a, ub, tactical=True)   # the colour run B gave
        deltas.append((i, gain, lose))
        if gain < CLASSIC_MIN or lose < CLASSIC_MIN:
            fails.append('td-classic: yard %d gained %d of colour %d and %d of colour %d (want >=%d each)'
                         % (i, gain, ca, lose, cb, CLASSIC_MIN))
    if pal_ok and not any(f.startswith('td-classic') for f in fails):
        says.append('Classic yards repaint with their seat (%s)' % deltas)

    if not td_only:
        # ra-seats
        r = (d / 'ra.log').read_text(errors='replace')
        stats = re.findall(r'^MATCHSTATS\|\d+\|.*\|colour=(\d+)\|house=(\d+)\|.*\|country=(\d+)$', r, re.M)
        dumps = r.split('OBJDUMP-BEGIN')
        last = dumps[-1] if len(dumps) > 1 else ''
        ra_bad = []
        seen = 0
        for colour, house, country in stats:
            colour, house, country = int(colour), int(house), int(country)
            if country not in RA_ROWS:
                continue
            seen += 1
            if colour != RA_ROWS[country]:
                ra_bad.append('country %d recorded colour %d, want %d' % (country, colour, RA_ROWS[country]))
            owner = 'Multi%d' % (house - 11)          # HOUSE_MULTI1 is 12 in Red Alert
            remaps = set(re.findall(r'^OBJ\|[A-Z]+\|[^|]+\|%s\|.*\|remap=(-?\d+)\|' % owner, last, re.M))
            if remaps != {str(RA_ROWS[country])}:
                ra_bad.append('%s (country %d) exports remap %s, want {%d}'
                              % (owner, country, sorted(remaps), RA_ROWS[country]))
        if seen != 2 or ra_bad:
            fails.append('ra-seats: %d of 2 seats found; %s' % (seen, '; '.join(ra_bad)))
        else:
            says.append('Red Alert grey and blue seats export the rows they picked')

        # ra-roster
        rows = re.findall(r'^SBROW\|\d+\|name=[^|]*\|house=(\d+)\|colour=(\d+)\|ink=(\d+)\|', r, re.M)
        rr_bad, inks = [], {}
        for house, colour, ink in rows:
            house, colour, ink = int(house), int(colour), int(ink)
            owner = 'Multi%d' % (house - 11)
            remaps = set(re.findall(r'^OBJ\|[A-Z]+\|[^|]+\|%s\|.*\|remap=(-?\d+)\|' % owner, last, re.M))
            if remaps != {str(colour)}:
                rr_bad.append('%s listed as row %d but wears %s' % (owner, colour, sorted(remaps)))
            elif ink != RA_ROSTER_INK[colour]:
                rr_bad.append('%s (row %d) inked %d, want %d' % (owner, colour, ink, RA_ROSTER_INK[colour]))
            else:
                inks[colour] = ink
        shot = d / 'ra_list.png'
        if len(rows) != 4 or rr_bad or not shot.exists():
            fails.append('ra-roster: %d rows (want 4), shot %s; %s'
                         % (len(rows), 'present' if shot.exists() else 'missing', '; '.join(rr_bad)))
        else:
            from PIL import Image
            sb = (run / 'ra3d/sidebar-temperat-win95.pack').read_bytes()
            assert sb[:8] == b'DOSBAR01'
            wide = sb[8 + 16 + 768:8 + 16 + 768 + 768]
            im = Image.open(shot).convert('RGB')
            box = im.crop((im.width - 160, 0, im.width, 170))
            hist = {}
            for px in box.getdata():
                hist[px] = hist.get(px, 0) + 1
            drawn = {c: hist.get(tuple(wide[i * 3:i * 3 + 3]), 0) for c, i in inks.items()}
            thin = {c: n for c, n in drawn.items() if n < RA_ROSTER_MIN}
            if thin:
                fails.append('ra-roster: rows %s drawn in too few pixels of their ink %s (want >=%d)'
                             % (sorted(thin), thin, RA_ROSTER_MIN))
            else:
                says.append('the Red Alert player list inks each row in its own 1996 colour (%s px)'
                            % ', '.join('row %d %d' % (c, n) for c, n in sorted(drawn.items())))
            n64 = d / 'ra_list_n64.png'
            if not n64.exists():
                fails.append('ra-roster: no N64 shot of the list')
            else:
                grey = tuple(wide[RA_ROSTER_INK[5] * 3:RA_ROSTER_INK[5] * 3 + 3])
                im = Image.open(n64).convert('RGB')
                x0 = im.width - im.width // 4
                lines = frame = 0
                for y in range(im.height // 2):
                    row = [im.getpixel((x, y)) for x in range(x0, im.width)]
                    at = [i for i, px in enumerate(row) if px == grey]
                    if len(at) < 3:
                        continue
                    lines += 1
                    frame += sum(1 for px in row[at[0]:at[-1] + 1] if px != grey and max(px) > 8)
                if lines < 6 or frame > lines:
                    fails.append('ra-roster: in N64, %d screen rows carry the grey ink and %d pixels of '
                                 'something other than black lie between their inks (want >=6 rows, '
                                 'at most one such pixel a row): the list is not in the radar hole'
                                 % (lines, frame))
                else:
                    says.append('in N64 the list stands in the radar hole (%d ink rows, %d stray px)'
                                % (lines, frame))

        # ra-classic
        b = (run / 'ra3d/dosclassic.pack').read_bytes()
        assert b[:8] == b'DOS2D005'
        off = 12 + 16
        rows5 = b[off + 16 * 5:off + 16 * 6]
        rows6 = b[off + 16 * 6:off + 16 * 7]
        snow = b[off + 128:off + 128 + 768]
        rgb = lambda i: tuple(snow[i * 3:i * 3 + 3])
        blue = {rgb(i) for i in rows6} - {rgb(i) for i in rows5}
        grey = {rgb(i) for i in rows5} - {rgb(i) for i in rows6}
        fr = d / 'fr.png'
        if not fr.exists():
            fails.append('ra-classic: no shot of the French jeep')
        else:
            # The jeep sits three cells from the map's top edge, so the camera clamps and the
            # jeep is off centre: a wider square, which also holds the French village round
            # it. Snow ground and barrels hold a few entries of both rows, so the blue row
            # must also outnumber the grey.
            nb, ng = count(fr, blue, 300), count(fr, grey, 300)
            if nb < FRENCH_MIN or nb <= ng:
                fails.append('ra-classic: the French jeep and village show %d blue-row pixels against %d '
                             'grey-row (want >=%d, and more than the grey)' % (nb, ng, FRENCH_MIN))
            else:
                says.append('the French jeep and village wear the blue row in Classic (%d px, grey row %d)'
                            % (nb, ng))

    if fails:
        print('0 ' + ' | '.join(fails))
        return 1
    print('1 ' + '; '.join(says))
    return 0


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == 'script':
        return script(Path(sys.argv[2]))
    if len(sys.argv) >= 4 and sys.argv[1] == 'check':
        return check(Path(sys.argv[2]), Path(sys.argv[3]), len(sys.argv) > 4 and sys.argv[4] == 'td')
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())
