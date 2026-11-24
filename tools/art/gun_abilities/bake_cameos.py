#!/usr/bin/env python3
"""bake_cameos.py -- the twenty Gun Game ability cameos for Dawn3D.

    /usr/local/bin/python3 tools/art/gun_abilities/bake_cameos.py

Writes:
    art/interface/gun-game-abilities-v01/GA01.png .. GA20.png    64x48 RGBA
    art/interface/gun-game-abilities-v01/SOURCES.json            what each was made from
    art/interface/gun-game-abilities-v01/REVIEW.png              1x and 3x, named
    game/gun_abilities.pack

Each cameo is laid out like a Tiberian Dawn sidebar cameo: the empty bevelled cell the
1995 sidebar draws (PUMPICON.SHP, which is byte for byte that blank cell) at twice size,
and on its flat grey field a picture composed from the original sprites, cameos and
effect frames of Tiberian Dawn and Red Alert, doubled with Scale2x so every cameo has the
pixel density of Tiberian Dawn's own 32x24 cameos drawn at 64x48. There is no text on a
cameo. The only marks drawn here rather than taken from the art are the arrows, arcs,
chevrons, music notes, the fuse, the reticle and the mines' ground patches that say what
the ability does. Every colour in the finished cameo is a Tiberian Dawn palette entry.

Pack format, little endian:
    char magic[8]  "GABIL01\\0"
    u32  count     20
    count x { char name[16] ("GA01".., zero padded); u32 w; u32 h; u8 rgba[w*h*4] }
"""
import json
import math
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ccart as C  # noqa: E402

REPO = C.REPO
OUT = os.path.join(REPO, "art", "interface", "gun-game-abilities-v01")
PACK = os.path.join(REPO, "game", "gun_abilities.pack")
W, H = 64, 48
X0, Y0, X1, Y1 = 2, 2, 62, 46          # the field inside the doubled bevel

PAL = C.td_palette()
REMAP = C.td_remaps()
# Quantise to the palette, leaving out index 0 (transparent), 4 (the shadow ghost) and
# the colour-cycling water entries 32..38, which never hold still in the game.
_QIDX = np.array([i for i in range(1, 256) if i != 4 and not 32 <= i <= 38])
_QPAL = PAL[_QIDX].astype(np.int32)


def col(i):
    return tuple(int(v) for v in PAL[i])


# named palette entries used by the drawn marks
BLACK, WHITE = col(12), col(15)
YELLOW, GOLD = col(5), col(149)
CYAN, CYAN_D = col(161), col(219)
CYAN_M = col(9)
GREEN, GREEN_D = col(166), col(160)
RED, RED_D = col(127), col(123)
ORANGE = col(148)
VIOLET, VIOLET_D = col(1), col(212)
FIELD = col(196)


# ---------------------------------------------------------------------------
# layers
# ---------------------------------------------------------------------------

class Cameo(object):
    def __init__(self):
        cell = C.Art("td", "PUMPICON.SHP").indices(0)
        self.cell = np.repeat(np.repeat(cell, 2, 0), 2, 1)
        self.rgb = PAL[self.cell].astype(np.float32)

    def over(self, img, x, y, alpha=1.0):
        """Alpha-composite an RGBA uint8 array with its top-left at (x, y)."""
        x, y = int(round(x)), int(round(y))
        h, w = img.shape[:2]
        sx0, sy0 = max(0, -x), max(0, -y)
        dx0, dy0 = max(0, x), max(0, y)
        dx1, dy1 = min(W, x + w), min(H, y + h)
        if dx1 <= dx0 or dy1 <= dy0:
            return
        src = img[sy0:sy0 + dy1 - dy0, sx0:sx0 + dx1 - dx0].astype(np.float32)
        a = src[..., 3:4] / 255.0 * alpha
        d = self.rgb[dy0:dy1, dx0:dx1]
        self.rgb[dy0:dy1, dx0:dx1] = d * (1 - a) + src[..., :3] * a

    def place(self, img, cx, by, alpha=1.0):
        """Centre on cx, bottom row on by."""
        self.over(img, cx - img.shape[1] / 2.0, by - img.shape[0], alpha)

    def centre(self, img, cx, cy, alpha=1.0):
        self.over(img, cx - img.shape[1] / 2.0, cy - img.shape[0] / 2.0, alpha)

    def draw(self, fn):
        """Draw marks with PIL (no antialiasing) on a transparent layer, then composite."""
        layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        fn(ImageDraw.Draw(layer))
        self.over(np.array(layer), 0, 0)

    def finish(self):
        rgb = np.clip(self.rgb, 0, 255).astype(np.int32)
        flat = rgb.reshape(-1, 3)
        d = ((flat[:, None, :] - _QPAL[None, :, :]) ** 2).sum(-1)
        q = _QPAL[d.argmin(1)].reshape(H, W, 3)
        border = np.ones((H, W), bool)
        border[Y0:Y1, X0:X1] = False
        q[border] = PAL[self.cell][border]
        out = np.zeros((H, W, 4), np.uint8)
        out[..., :3] = q
        out[..., 3] = 255
        return out


# ---------------------------------------------------------------------------
# art helpers
# ---------------------------------------------------------------------------

def up(a, times=1):
    for _ in range(times):
        a = C.scale2x(a)
    return a


def sprite(game, name, frame, remap=None, turret=None, shadow=True, times=1, prefer=None, flip=False):
    art = C.Art(game, name + ".SHP", prefer)
    img = art.rgba(frame, remap, shadow)
    if turret is not None:
        t = art.rgba(turret, remap, False)
        a = t[..., 3:4] / 255.0
        img = img.astype(np.float32)
        img[..., :3] = img[..., :3] * (1 - a) + t[..., :3] * a
        img[..., 3] = np.maximum(img[..., 3], t[..., 3])
        img = img.astype(np.uint8)
    img = C.trim(img)
    if flip:
        img = img[:, ::-1]
    return up(img, times)


def solid(img):
    """Drop the half-black shadow pixels."""
    img = img.copy()
    img[img[..., 3] < 255] = 0
    return img


def tint(img, rgb, k):
    out = img.astype(np.float32)
    out[..., :3] = out[..., :3] * (1 - k) + np.array(rgb, np.float32) * k
    return out.astype(np.uint8)


def ramp(img, colours):
    """Recolour by brightness onto a ramp of palette entries, dark to light."""
    out = img.copy()
    lum = img[..., :3].astype(np.float32) @ np.array([0.3, 0.59, 0.11], np.float32)
    lo, hi = np.percentile(lum[img[..., 3] == 255], [3, 97]) if (img[..., 3] == 255).any() else (0, 255)
    t = np.clip((lum - lo) / max(1.0, hi - lo), 0, 1)
    k = np.minimum(len(colours) - 1, (t * len(colours)).astype(int))
    pal = np.array(colours, np.uint8)
    out[..., :3] = pal[k]
    return out


def halo(img, rgb, width=2, alpha=200):
    """A solid outline of `width` pixels around the shape, behind it."""
    m = img[..., 3] > 0
    h, w = m.shape
    pad = width
    big = np.zeros((h + 2 * pad, w + 2 * pad), bool)
    for dy in range(-pad, pad + 1):
        for dx in range(-pad, pad + 1):
            if dx * dx + dy * dy <= pad * pad + 1:
                big[pad + dy:pad + dy + h, pad + dx:pad + dx + w] |= m
    out = np.zeros((h + 2 * pad, w + 2 * pad, 4), np.uint8)
    out[big] = tuple(rgb) + (alpha,)
    src = np.zeros_like(out)
    src[pad:pad + h, pad:pad + w] = img
    a = src[..., 3:4] / 255.0
    out = (out * (1 - a) + src * a).astype(np.uint8)
    out[..., 3] = np.maximum(out[..., 3], src[..., 3])
    return out


def outline_only(img, rgb):
    """Just the edge pixels of a shape, in one colour: a ghost's outline."""
    m = img[..., 3] > 128
    e = m & ~(np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1))
    out = np.zeros_like(img)
    out[e] = tuple(rgb) + (255,)
    return out


def twirl(img, strength):
    """Rotate each pixel about the centre by an angle that falls off with radius."""
    h, w = img.shape[:2]
    cy, cx = (h - 1) / 2.0, (w - 1) / 2.0
    r0 = min(cx, cy)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    dx, dy = xx - cx, yy - cy
    r = np.sqrt(dx * dx + dy * dy)
    ang = np.arctan2(dy, dx) - strength * np.clip(1 - r / r0, 0, 1)
    sx = np.clip(np.round(cx + r * np.cos(ang)).astype(int), 0, w - 1)
    sy = np.clip(np.round(cy + r * np.sin(ang)).astype(int), 0, h - 1)
    out = img[sy, sx]
    out[r > r0] = 0
    return out


def thick_line(d, pts, colour, width=2, outline=True):
    if outline:
        d.line(pts, fill=BLACK + (255,), width=width + 2, joint="curve")
    d.line(pts, fill=colour + (255,), width=width, joint="curve")


def arrow_head(d, tip, direction, colour, size=5, outline=True):
    dx, dy = direction
    n = math.hypot(dx, dy) or 1.0
    dx, dy = dx / n, dy / n
    px, py = -dy, dx
    bx, by = tip[0] - dx * size, tip[1] - dy * size
    tri = [tip, (bx + px * size * 0.8, by + py * size * 0.8), (bx - px * size * 0.8, by - py * size * 0.8)]
    if outline:
        big = [(tip[0] + dx * 1.5, tip[1] + dy * 1.5),
               (bx + px * (size * 0.8 + 1.5) - dx, by + py * (size * 0.8 + 1.5) - dy),
               (bx - px * (size * 0.8 + 1.5) - dx, by - py * (size * 0.8 + 1.5) - dy)]
        d.polygon(big, fill=BLACK + (255,))
    d.polygon(tri, fill=colour + (255,))


def arc_points(cx, cy, rx, ry, a0, a1, n=24):
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def curved_arrow(d, pts, colour, width=2, head=5):
    thick_line(d, pts, colour, width)
    tip = pts[-1]
    arrow_head(d, tip, (tip[0] - pts[-3][0], tip[1] - pts[-3][1]), colour, head)
    # repaint the shaft over the head's outline where they meet
    d.line(pts[:-2], fill=colour + (255,), width=width, joint="curve")


def dotted(d, pts, colour, every=3, size=2):
    """Dots along a polyline, each a size x size square with a dark corner."""
    acc, out = 0.0, []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        seg = math.hypot(x1 - x0, y1 - y0)
        t = 0.0
        while acc + seg - t >= every:
            t += every - acc
            acc = 0.0
            out.append((x0 + (x1 - x0) * t / seg, y0 + (y1 - y0) * t / seg))
        acc += seg - t
    for x, y in out:
        d.rectangle([x - 1, y - 1, x - 1 + size, y - 1 + size], fill=BLACK + (255,))
        d.rectangle([x - 1, y - 1, x - 2 + size, y - 2 + size], fill=colour + (255,))
    return out


# An eighth note and a pair of beamed notes, as 1-bit glyphs drawn at 2x.
NOTE = ["..##", "..#.#", "..#..#", "..#", "..#", "###", "###"]
NOTES2 = ["..#####", "..#...#", "..#...#", "..#...#", "###.###", "###.###"]


def glyph(rows, colour, scale=2, outline=True):
    h, w = len(rows), max(len(r) for r in rows)
    m = np.zeros((h, w), bool)
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            m[y, x] = c == "#"
    m = np.repeat(np.repeat(m, scale, 0), scale, 1)
    img = np.zeros(m.shape + (4,), np.uint8)
    img[m] = tuple(colour) + (255,)
    return halo(img, BLACK, 1, 255) if outline else img


def chevron(colour, w=14, h=8, t=3):
    """A rank chevron pointing down."""
    img = Image.new("RGBA", (w + 2, h + t + 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    outer = [(1, 1), (w // 2 + 1, h + 1), (w + 1, 1), (w + 1, 1 + t), (w // 2 + 1, h + 1 + t), (1, 1 + t)]
    d.polygon(outer, fill=colour + (255,))
    return halo(np.array(img), BLACK, 1, 255)


# ---------------------------------------------------------------------------
# the twenty
# ---------------------------------------------------------------------------
# TD vehicles carry 32 body facings then 32 turret facings; frame 0 is north and they
# count anticlockwise, so 8 faces west, 16 south, 24 east. Infantry stand frames are
# N NW W SW S SE E NE; the gestures are E1's GESTURE/SALUTE strips facing south.
E, W_, SW, SE, S = 24, 8, 12, 20, 16

NOD = REMAP["RED"]


def ga01_stealth():
    c = Cameo()
    tank = sprite("td", "STNK", SW, times=1)
    h, w = tank.shape[:2]
    # dissolve the tank from left to right on a 4x4 Bayer pattern, as a cloak closes
    bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
    sweep = np.linspace(-0.3, 1.2, w)[None, :].repeat(h, 0)
    fading = tank.copy()
    fading[sweep > np.tile(bayer, (h // 4 + 1, w // 4 + 1))[:h, :w]] = 0
    globe = up(C.trim(C.ra("STEALTH2.SHP", 10, shadow=False)))
    c.centre(globe, 32, 24, 0.22)
    c.centre(outline_only(tank, CYAN), 32, 28, 0.9)
    c.centre(fading, 32, 28)
    return c, ["TD STNK.SHP body 12: the Stealth Tank, dissolving from left to right on an "
               "ordered dither inside its own cyan outline", "RA STEALTH2.SHP frame 10: the "
               "stealth crate's green field globe, faint, around it"]


def ga02_chrono_hop():
    c = Cameo()
    tank = sprite("td", "LTNK", E, turret=32 + E, times=1)
    ghost = tint(solid(tank), CYAN, 0.8)
    c.place(ghost, 19, 45, 0.3)
    c.place(outline_only(tank, CYAN), 19, 45)
    c.place(tank, 45, 46)
    flash = up(C.trim(C.ra("TWINKLE1.SHP", 2, shadow=False)))

    def marks(d):
        pts = arc_points(32, 26, 14, 14, 190, 340)
        dotted(d, pts, CYAN, every=4)
        arrow_head(d, pts[-1], (pts[-1][0] - pts[-3][0], pts[-1][1] - pts[-3][1]), CYAN, 4)
    c.draw(marks)
    c.centre(ramp(flash, [CYAN_D, CYAN_M, CYAN, WHITE]), 13, 16)
    return c, ["TD LTNK.SHP body 24 + turret 56: the Light Tank facing east, solid where it "
               "lands and as a cyan ghost where it left", "RA TWINKLE1.SHP frame 2: "
               "the departure spark, in chrono cyan", "drawn: dotted jump arc and head"]


def ga03_chrono_swap():
    c = Cameo()
    gdi = sprite("td", "E1", 6, times=1)            # facing east
    nod = sprite("td", "E1", 2, remap=NOD, times=1)  # facing west
    c.place(gdi, 11, 45)
    c.place(nod, 53, 45)

    def marks(d):
        curved_arrow(d, arc_points(32, 30, 13, 10, 200, 340), CYAN, 2, 4)
        curved_arrow(d, arc_points(32, 26, 13, 10, 20, 160), CYAN, 2, 4)
    c.draw(marks)
    return c, ["TD E1.SHP frame 6: a GDI Minigunner facing east",
               "TD E1.SHP frame 2, RemapRed: a Nod Minigunner facing west",
               "drawn: two curved swap arrows in chrono cyan"]


def ga04_iron_curtain():
    c = Cameo()
    tank = sprite("td", "MTNK", SW, turret=32 + SW, shadow=False, times=1)
    red = ramp(tank, [col(i) for i in (47, 43, 121, 122, 123, 124, 125, 126, 127, 218, 217)])
    glow = halo(red, col(126), 2, 140)
    glow = halo(glow, col(122), 1, 110)
    c.centre(glow, 32, 25)
    return c, ["TD MTNK.SHP body 12 + turret 44: the Medium Tank, recoloured onto the "
               "palette's red ramp (120..127) as an Iron Curtain glow, with a red halo "
               "around it"]


def ga05_overdrive():
    c = Cameo()
    jeep = sprite("td", "JEEP", E, turret=32 + E, times=1)
    bolt = up(C.trim(C.ra("SPEED.SHP", 2, shadow=False)))

    def marks(d):
        for y, x0, x1, colour in ((28, 3, 16, WHITE), (33, 6, 18, col(14)), (38, 2, 17, WHITE),
                                  (43, 8, 19, col(14))):
            d.line([(x0, y), (x1, y)], fill=BLACK + (255,), width=4)
            d.line([(x0 + 1, y), (x1 - 1, y)], fill=colour + (255,), width=2)
    c.draw(marks)
    c.place(jeep, 39, 47)
    c.centre(halo(bolt, BLACK, 1, 255), 12, 14)
    return c, ["TD JEEP.SHP body 24 + turret 56: the Hum-vee facing east",
               "RA SPEED.SHP frame 2: the speed crate's lightning bolt",
               "drawn: four motion streaks"]


def ga06_vortex():
    c = Cameo()
    core = up(C.td("EMPULSE.SHP", 1, shadow=False))
    core = ramp(core, [col(i) for i in (153, 206, 212, 211, 109, 236, 170, 161, 15)])
    swirl = twirl(core, 5.0)

    def marks(d):
        for k in range(3):
            a0 = k * 120
            pts = [(32 + (20 - t * 0.55) * math.cos(math.radians(a0 + t * 9)),
                    24 + (16 - t * 0.45) * math.sin(math.radians(a0 + t * 9))) for t in range(0, 26)]
            thick_line(d, pts, col(170), 2)
            d.line(pts[:8], fill=CYAN + (255,), width=2)
    c.draw(marks)
    c.centre(swirl, 32, 24)
    return c, ["TD EMPULSE.SHP frame 1: the EMP crate's cracked sphere, recoloured onto the "
               "palette's violet-to-cyan entries and twirled about its centre",
               "drawn: three spiral arms"]


def ga07_minefield():
    c = Cameo()
    near = halo(up(C.trim(C.td("MINE.SHP", 1, shadow=False))), BLACK, 1, 255)
    far = halo(up(C.trim(C.td("MINE.SHP", 0, shadow=False))), BLACK, 1, 255)
    spots = ((16, 26, far), (48, 26, far), (32, 46, near))

    def ground(d):
        for x, y, m in spots:
            rx = m.shape[1] // 2
            d.ellipse([x - rx, y - 4, x + rx, y + 1], fill=col(198) + (150,))
    c.draw(ground)
    for x, y, m in spots:
        c.place(m, x, y)
    return c, ["TD MINE.SHP frames 0 and 1: the mine crate's spiked mine, edged in black, "
               "one near and two far, each on a dark patch of ground"]


def ga08_hunters_eye():
    c = Cameo()
    target = sprite("td", "MTNK", SW, turret=32 + SW, remap=NOD, times=1)
    c.centre(target, 32, 26)

    def marks(d):
        g = col(166)
        d.ellipse([12, 5, 52, 45], outline=BLACK + (255,), width=4)
        d.ellipse([13, 6, 51, 44], outline=g + (255,), width=2)
        for (x0, y0, x1, y1) in ((32, 1, 32, 12), (32, 38, 32, 47), (6, 25, 19, 25), (45, 25, 58, 25)):
            d.line([(x0, y0), (x1, y1)], fill=BLACK + (255,), width=4)
        for (x0, y0, x1, y1) in ((32, 2, 32, 11), (32, 39, 32, 46), (7, 25, 18, 25), (46, 25, 57, 25)):
            d.line([(x0, y0), (x1, y1)], fill=g + (255,), width=2)
    c.draw(marks)
    return c, ["TD MTNK.SHP body 12 + turret 44, RemapRed: a Nod Medium Tank as the quarry",
               "drawn: a green targeting reticle"]


def ga09_humiliation():
    c = Cameo()
    man = sprite("td", "E1", 4, remap=NOD, times=1)
    dart = up(C.trim(C.td("DEVIATOR.SHP", 3, shadow=False)))[::-1, ::-1]
    c.place(man, 43, 46)
    c.centre(dart, 18, 27)
    ch = chevron(col(126), 14, 6, 3)
    c.centre(ch, 43, 7)
    c.centre(ch, 43, 13)
    return c, ["TD E1.SHP frame 4, RemapRed: a Nod Minigunner facing the viewer",
               "TD DEVIATOR.SHP frame 3, turned to fly at him: the deviator crate's dart",
               "drawn: two red down-rank chevrons over him"]


def ga10_chain_shock():
    c = Cameo()
    men = [sprite("td", "E1", f, remap=NOD, times=1) for f in (3, 4, 5)]
    for x, by, m in ((10, 38, men[0]), (32, 46, men[1]), (54, 38, men[2])):
        c.place(m, x, by)
    blue = [col(i) for i in (220, 219, 9, 161, 15)]
    b0 = up(ramp(C.trim(C.ra("LITNING.SHP", 0, shadow=False)), blue))
    b1 = up(ramp(C.trim(C.ra("LITNING.SHP", 1, shadow=False)), blue))
    c.centre(b0, 21, 19)
    c.centre(b1[:, ::-1], 43, 19)
    return c, ["TD E1.SHP frames 3, 4, 5, RemapRed: three Nod Minigunners",
               "RA LITNING.SHP frames 0 and 1: the Tesla Coil's lightning, brightened on the "
               "palette's cyan entries, jumping from man to man"]


def ga11_extra_life():
    c = Cameo()
    man = sprite("td", "E1", 4, times=1)
    hosp = C.Art("td", "HOSPICON.SHP").indices(0)
    cross = hosp[6:17, 8:23]
    disc = np.zeros(cross.shape + (4,), np.uint8)
    keep = ~np.isin(cross, (176, 177, 178, 179, 180, 181, 182, 184, 185, 186, 187, 188, 196, 12))
    disc[keep, :3] = PAL[cross[keep]]
    disc[keep, 3] = 255
    disc = halo(up(C.trim(disc)), BLACK, 1, 255)
    c.place(man, 18, 46)
    c.centre(disc, 43, 17)

    def marks(d):
        thick_line(d, [(43, 45), (43, 35)], col(166), 2)
        arrow_head(d, (43, 31), (0, -1), col(166), 5)
    c.draw(marks)
    return c, ["TD E1.SHP frame 4: a GDI Minigunner",
               "TD HOSPICON.SHP: the red cross on its white disc, lifted off the Hospital "
               "cameo's roof", "drawn: a green up arrow"]


def ga12_smoke():
    c = Cameo()
    tank = sprite("td", "MTNK", SW, turret=32 + SW, times=1)
    c.centre(tank, 32, 24)
    grey = [col(i) for i in (195, 194, 193, 168, 192, 15)]
    cloud = ramp(C.trim(C.td("ATOMSFX.SHP", 19, shadow=False)), grey)
    fade = np.linspace(0.5, 1.0, cloud.shape[0])[:, None]
    cloud[..., 3] = (cloud[..., 3] * fade).astype(np.uint8)
    c.over(cloud, 32 - cloud.shape[1] // 2, 8, 0.93)
    return c, ["TD MTNK.SHP body 12 + turret 44: the Medium Tank",
               "TD ATOMSFX.SHP frame 19: the grey billow of a blast cloud, set on the "
               "palette's light greys and thinning towards the top, as a smoke bank the tank "
               "shows through"]


def ga13_decoys():
    c = Cameo()
    man = sprite("td", "E2", 4, times=1)
    ghost = tint(solid(man), CYAN, 0.35)
    for x in (13, 51):
        c.place(ghost, x, 41, 0.5)
        c.place(outline_only(man, CYAN), x, 41, 0.8)
    c.place(man, 32, 46)
    return c, ["TD E2.SHP frame 4: a GDI Grenadier facing the viewer, once solid and twice "
               "as faded cyan copies beside him"]


def ga14_giant():
    c = Cameo()
    big = sprite("td", "HTNK", SW, turret=32 + SW, times=1)
    small = sprite("td", "HTNK", SW, turret=32 + SW, times=0)
    c.place(big, 39, 52)
    c.place(small, 11, 45)

    def marks(d):
        thick_line(d, [(9, 24), (9, 13)], YELLOW, 2)
        arrow_head(d, (9, 8), (0, -1), YELLOW, 4)
    c.draw(marks)
    return c, ["TD HTNK.SHP body 12 + turret 44: the Mammoth Tank at the cameos' size, "
               "breaking out of the frame, beside the same tank at half that size",
               "drawn: a yellow grow arrow"]


def ga15_last_laugh():
    c = Cameo()
    boom = up(C.trim(C.td("ATOMSFX.SHP", 11, shadow=False)))
    c.place(boom, 32, 56)
    skull = up(C.trim(C.td("RAPID.SHP", 2, shadow=False)))
    c.centre(halo(skull, BLACK, 1, 255), 32, 31)
    return c, ["TD ATOMSFX.SHP frame 11: the nuclear blast's mushroom cloud",
               "TD RAPID.SHP frame 2: the rapid-fire crate's grinning red skull, in its stem"]


def ga16_hot_potato():
    c = Cameo()
    ball = C.td("EMPULSE.SHP", 0, shadow=False)
    ball = ramp(C.trim(ball), [col(i) for i in (112, 113, 153, 206, 205, 125, 148, 149, 5)])
    ball = up(ball)
    c.centre(halo(ball, col(126), 1, 200), 27, 29)

    def marks(d):
        pts = [(36, 13), (39, 9), (43, 7), (47, 8)]
        thick_line(d, pts, col(179), 2)
        for (x0, y0, x1, y1) in ((51, 15, 55, 19), (54, 9, 59, 9), (51, 3, 54, 1)):
            d.line([(x0, y0), (x1, y1)], fill=ORANGE + (255,), width=2)
    c.draw(marks)
    spark = up(C.trim(C.ra("TWINKLE1.SHP", 2, shadow=False)))
    c.centre(ramp(spark, [col(i) for i in (148, 149, 5, 15)]), 49, 9)
    return c, ["TD EMPULSE.SHP frame 0: the EMP crate's sphere, recoloured into a black "
               "bomb glowing red hot", "RA TWINKLE1.SHP frame 2: the spark at the fuse's end",
               "drawn: the fuse and three flying sparks"]


def ga17_trapped_crate():
    c = Cameo()
    crate = up(C.trim(C.td("WCRATE.SHP", 0, shadow=False)), 2)
    mine = halo(up(C.trim(C.ra("MINV.SHP", 0))), BLACK, 1, 255)
    blast = up(C.trim(C.td("FBALL1.SHP", 8, shadow=False)))
    c.place(blast, 44, 30, 0.95)
    c.place(crate, 23, 46)
    c.place(mine, 46, 47)
    return c, ["TD WCRATE.SHP: the wooden bonus crate, Scale2x twice",
               "RA MINV.SHP: the anti-tank mine, red light lit, beside it",
               "TD FBALL1.SHP frame 8: the fireball bursting out behind it"]


def ga18_wormhole():
    c = Cameo()
    ring = C.trim(C.td("MOVEFLSH.SHP", 2, shadow=False))
    ring = up(ramp(ring, [col(i) for i in (1, 109, 238, 239)]))
    ring = halo(ring, VIOLET_D, 1, 255)
    tank = sprite("td", "LTNK", E, turret=32 + E, times=0)

    def marks(d):
        pts = arc_points(32, 28, 17, 14, 195, 345)
        dotted(d, pts, col(238), every=4)
        arrow_head(d, pts[-1], (pts[-1][0] - pts[-3][0], pts[-1][1] - pts[-3][1]), col(238), 4)
    c.draw(marks)
    c.centre(ring, 15, 32)
    c.centre(ring, 49, 32)
    c.centre(tank, 15, 32, 0.45)
    c.centre(tank, 49, 32)
    return c, ["TD MOVEFLSH.SHP frame 2: the move-order ring, recoloured violet, once at "
               "each mouth", "TD LTNK.SHP body 24 + turret 56 at its own size: fading in one "
               "ring, out of the other", "drawn: the dotted link between them"]


def ga19_dance_off():
    c = Cameo()
    left = sprite("td", "E1", 449, times=1)
    right = sprite("td", "E1", 473, remap=NOD, times=1, flip=True)
    c.place(left, 18, 46)
    c.place(right, 46, 46)
    c.centre(glyph(NOTE, YELLOW, 1), 32, 12)
    c.centre(glyph(NOTES2, YELLOW, 1), 50, 8)
    c.centre(glyph(NOTE, YELLOW, 1), 12, 8)
    c.centre(glyph(NOTE, YELLOW, 1), 32, 26)
    return c, ["TD E1.SHP frame 449 (GESTURE1, arms up, facing south): a GDI Minigunner",
               "TD E1.SHP frame 473 (SALUTE1), RemapRed, mirrored: a Nod Minigunner",
               "drawn: music notes"]


def ga20_mirror():
    c = Cameo()
    man = sprite("td", "RMBO", 6, times=1)          # facing east
    c.place(man, 18, 40)
    c.place(man[:, ::-1], 46, 40)

    def marks(d):
        for y in range(4, 38, 5):
            d.rectangle([31, y, 32, y + 2], fill=col(161) + (255,))
        thick_line(d, [(14, 43), (50, 43)], YELLOW, 2)
        arrow_head(d, (8, 43), (-1, 0), YELLOW, 4)
        arrow_head(d, (56, 43), (1, 0), YELLOW, 4)
    c.draw(marks)
    return c, ["TD RMBO.SHP frame 6: the Commando facing east, and his mirror image",
               "drawn: the dashed mirror line and a two-headed arrow"]


ABILITIES = [
    ("GA01", "Stealth Field", ga01_stealth),
    ("GA02", "Chrono Hop", ga02_chrono_hop),
    ("GA03", "Chrono Swap", ga03_chrono_swap),
    ("GA04", "Personal Iron Curtain", ga04_iron_curtain),
    ("GA05", "Overdrive", ga05_overdrive),
    ("GA06", "Chrono Vortex", ga06_vortex),
    ("GA07", "Minefield", ga07_minefield),
    ("GA08", "Hunter's Eye", ga08_hunters_eye),
    ("GA09", "Humiliation Dart", ga09_humiliation),
    ("GA10", "Chain Shock", ga10_chain_shock),
    ("GA11", "Extra Life", ga11_extra_life),
    ("GA12", "Smoke Screen", ga12_smoke),
    ("GA13", "Decoys", ga13_decoys),
    ("GA14", "Giant Mode", ga14_giant),
    ("GA15", "Last Laugh", ga15_last_laugh),
    ("GA16", "Hot Potato", ga16_hot_potato),
    ("GA17", "Booby-Trapped Crate", ga17_trapped_crate),
    ("GA18", "Wormhole", ga18_wormhole),
    ("GA19", "Dance Off", ga19_dance_off),
    ("GA20", "Mirror Madness", ga20_mirror),
]


def review(images, path):
    font = ImageFont.load_default()
    cw, ch = 64 + 8 + 192 + 16, 144 + 22
    cols = 4
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cw + 8, rows * ch + 30), (22, 22, 22))
    d = ImageDraw.Draw(sheet)
    d.text((8, 8), "Gun Game ability cameos v01: 64x48 at 1x and 3x (nearest)", fill=(235, 235, 235), font=font)
    for i, (key, name, img) in enumerate(images):
        x, y = 8 + (i % cols) * cw, 30 + (i // cols) * ch
        im = Image.fromarray(img, "RGBA")
        sheet.paste(im, (x, y + 4))
        sheet.paste(im.resize((192, 144), Image.NEAREST), (x + 72, y))
        d.text((x, y + 148), "%s  %s" % (key, name), fill=(255, 220, 90), font=font)
    sheet.save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    images, sources = [], []
    for key, name, fn in ABILITIES:
        C.TOUCHED.clear()
        cam, how = fn()
        img = cam.finish()
        Image.fromarray(img, "RGBA").save(os.path.join(OUT, key + ".png"))
        used = [{"entry": k, "frames": v} if v else {"entry": k} for k, v in sorted(C.TOUCHED.items())]
        images.append((key, name, img))
        sources.append({"id": key, "name": name, "file": key + ".png", "size": [W, H],
                        "composition": how, "reads": used})
    # every cameo also reads the blank cell and the palette, loaded once up front
    for s in sources:
        names = [r["entry"] for r in s["reads"]]
        for k in ("td:data/dosdata/CONQUER.MIX:PUMPICON.SHP", "td:data/dosdata/LOCAL.MIX:TEMPERAT.PAL"):
            if k in C.LEDGER and k not in names:
                s["reads"].append({"entry": k})
        s["reads"].sort(key=lambda r: r["entry"])
    with open(PACK, "wb") as f:
        f.write(b"GABIL01\0" + struct.pack("<I", len(images)))
        for key, name, img in images:
            f.write(struct.pack("<16sII", key.encode(), W, H) + img.tobytes())
    ledger = []
    for k in sorted(C.LEDGER):
        row = dict(C.LEDGER[k])
        ledger.append(row)
    meta = {
        "what": "Gun Game ability cameos for Dawn3D, 64x48 RGBA, Tiberian Dawn cameo style",
        "baker": "tools/art/gun_abilities/bake_cameos.py",
        "cell": "TD PUMPICON.SHP (the empty bevelled sidebar cell), doubled",
        "scaling": "Scale2x (EPX) on original pixels; no resampling filters",
        "palette": "every pixel quantised to TD TEMPERAT.PAL widened min(252, v << 2), "
                   "omitting indices 0, 4 and 32..38",
        "house_remap": "TD RemapRed from brain/vanilla/tiberiandawn/const.cpp for Nod units",
        "pack": {"path": "game/gun_abilities.pack", "magic": "GABIL01\\0", "count": len(images),
                 "record": "char name[16], u32 w, u32 h, u8 rgba[w*h*4]"},
        "abilities": sources,
        "source_entries": ledger,
    }
    with open(os.path.join(OUT, "SOURCES.json"), "w") as f:
        json.dump(meta, f, indent=2)
    review(images, os.path.join(OUT, "REVIEW.png"))
    print("wrote %d cameos, %s (%d bytes)" % (len(images), os.path.relpath(PACK, REPO), os.path.getsize(PACK)))


if __name__ == "__main__":
    main()
