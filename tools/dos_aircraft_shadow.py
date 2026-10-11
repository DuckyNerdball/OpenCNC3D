#!/usr/bin/env python3
"""
dos_aircraft_shadow.py -- the blend Classic uses for an aircraft's shadow, derived from
the 1995 table it stands in for.

What the 1995 engine does. AircraftClass::Draw_It (tiberiandawn/aircraft.cpp) draws the
aircraft's own frame a second time at its ground position with SHAPE_PREDATOR |
SHAPE_FADING, and a flying helicopter's rotor takes the same pair. CC_Draw_Shape
(tiberiandawn/conquer.cpp) turns that pair into a ghost draw through Map.SpecialGhost,
whose lookup half maps every source index to 0 and whose table half is
Conquer_Build_Fading_Table(GamePalette, ..., BLACK, 100) (display.cpp). So each pixel
under the silhouette becomes the BACKGROUND pixel faded 100/256 toward black and then
matched to the nearest of the palette's last 16 entries (common/fading.cpp), which in
every Tiberian Dawn theater palette is a dark shade ramp: olive in Temperate, brown in
Desert. The shadow is a dark, tinted, translucent smudge, never a copy of the aircraft.

Why a fit. Classic draws with fixed-function blending so that it stays drawable on a
Voodoo 2, and one blend cannot repeat a 16-colour remap. The renderer draws the
silhouette as  ground * (1 - a) + C * a,  and this script finds the a and C that come
closest to the real table over the theater's clear-ground tile CLEAR1, which is most
of what any shadow falls on. It prints the table rows for game/dosclassic_mod.h and how
far the fit and a plain black blend each sit from the true remap.

Usage:
    python3 tools/dos_aircraft_shadow.py [WINTER.MIX]

TEMPERAT and DESERT are read from data/dosdata. The Winter theater archive is not part
of that disc set; pass a Tiberian Dawn WINTER.MIX to add its row. Its WINTER.PAL and
CLEAR1.WIN are byte-identical to TEMPERAT.PAL and CLEAR1.TEM, so the row comes out the
same as Temperate's.
"""

import os
import struct
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "menu", "tools"))
import mixshp  # noqa: E402

BLACK = 12          # ColorType BLACK, common/wwstd.h
SHADE_FRAC = 100    # display.cpp: Conquer_Build_Fading_Table(..., &SpecialGhost[256], BLACK, 100)


def fading_table(pal6, color, frac):
    """common/fading.cpp Conquer_Build_Fading_Table, over the 6-bit palette as the
    engine holds it. Index 0 stays 0 and indices 240..255 are left as themselves."""
    frac = min(frac, 255)
    fraction = frac >> 1
    tr, tg, tb = pal6[color * 3:color * 3 + 3]
    out = list(range(256))
    out[0] = 0
    for i in range(1, 240):
        ideal = []
        for c, t in zip(pal6[i * 3:i * 3 + 3], (tr, tg, tb)):
            o = c if c < 128 else c - 256                      # signed char
            tmp = ((o - t) * fraction) << 1
            tmp = ((tmp + 32768) & 0xFFFF) - 32768             # signed short
            ideal.append((o - (tmp >> 8)) & 0xFF)
        best, bestv = color, None
        for j in range(16):
            idx = 240 + j
            v = 0
            for k in range(3):
                d = ((pal6[idx * 3 + k] - ideal[k] + 128) & 0xFF) - 128
                v += d * d
            if bestv is None or v < bestv:
                bestv, best = v, idx
            if v == 0:
                break
        out[i] = best
    return out


def fit_blend(src, dst):
    """Least squares  dst ~= k * src + b  (k shared, b per channel), then b held at 0
    on any channel the free fit drove negative, since a blend colour cannot be."""
    fixed = [False, False, False]
    for _ in range(3):
        cols = [src.reshape(-1)]
        for c in range(3):
            if not fixed[c]:
                col = np.zeros(src.size)
                col[c::3] = 1
                cols.append(col)
        sol, *_ = np.linalg.lstsq(np.stack(cols, 1), dst.reshape(-1), rcond=None)
        k, b, n = sol[0], np.zeros(3), 1
        for c in range(3):
            if not fixed[c]:
                b[c] = sol[n]
                n += 1
        neg = [c for c in range(3) if not fixed[c] and b[c] < 0]
        if not neg:
            return k, b
        for c in neg:
            fixed[c] = True
    return k, np.maximum(b, 0)


def theater_row(mix, root, ext):
    for name in (root + ".PAL", "CLEAR1." + ext):
        if not mix.has(name):
            raise SystemExit("no %s in the %s archive" % (name, root))
    pal6 = list(mix.read(root + ".PAL"))
    if len(pal6) != 768 or max(pal6) > 63:
        raise SystemExit("%s.PAL is not a 6-bit VGA palette" % root)
    pal = np.array([(v << 2) | (v >> 4) for v in pal6], float).reshape(256, 3)
    table = np.array(fading_table(pal6, BLACK, SHADE_FRAC))
    data = mix.read("CLEAR1." + ext)
    w, h, count, _a, _size, icons = struct.unpack_from("<4h2i", data, 0)
    px = np.frombuffer(data[icons:icons + w * h * count], np.uint8).astype(int)
    px = px[px > 0]
    src, dst = pal[px], pal[table[px]]
    k, b = fit_blend(src, dst)
    a = 1.0 - k
    colour = np.clip(np.round(b / a), 0, 255).astype(int)
    alpha = int(round(a * 255))
    rms = lambda est: float(np.sqrt(((np.clip(est, 0, 255) - dst) ** 2).mean()))
    blended = src * (1 - alpha / 255.0) + colour * (alpha / 255.0)
    black = src * (1 - SHADE_FRAC / 256.0)
    lum = np.array([0.299, 0.587, 0.114])
    print("%-8s { \"%s\", %3d, %3d, %3d, %3d },  /* fit rms %.1f, black %d/256 rms %.1f;"
          " ground luminance x%.2f (table x%.2f) */"
          % (root, root, colour[0], colour[1], colour[2], alpha, rms(blended),
             SHADE_FRAC, rms(black), (blended @ lum).mean() / (src @ lum).mean(),
             (dst @ lum).mean() / (src @ lum).mean()))


def main():
    for root, ext in (("TEMPERAT", "TEM"), ("DESERT", "DES")):
        theater_row(mixshp.MixFile(os.path.join(ROOT, "data", "dosdata", root + ".MIX")),
                    root, ext)
    if len(sys.argv) > 1:
        theater_row(mixshp.MixFile(sys.argv[1]), "WINTER", "WIN")


if __name__ == "__main__":
    main()
