#!/usr/bin/env python3
"""bake_mode_icons.py -- the game-mode tiles for the Multiplayer/Skirmish mode picker.

    /usr/local/bin/python3 tools/art/gun_abilities/bake_mode_icons.py

Writes art/interface/game-mode-icons-v01/:
    td-mode-ffa.png  td-mode-gungame.png  td-mode-rally.png
    ra-mode-ffa.png  ra-mode-gungame.png  ra-mode-rally.png
    SOURCES.json  REVIEW.png

Every tile is 136x120 RGBA, the size of the Dawn menu's faction plates
(art/interface/enhanced-menu-v06/faction-GDI.png and faction-Nod.png), and each game's
tile is dressed in that game's own menu chassis:

  Dawn       the brushed gunmetal of the Dawn menu kit's faction plate, taken from the
             border band of faction-Nod.png (the emblem is covered by the well), with a
             dark recessed well and four bolts lifted from the corner of the kit's
             slots-frame.png.
  Red Alert  the plate the Red Alert Enhanced menu kit builds for its side tiles
             (art/interface/ra-enhanced-menu-v01/mockup/compose.py, plate()): steel cut
             from Red Alert's own Win95 dialog rail DD-TOP.SHP in the title palette,
             darkened, with a dark inset and a one-pixel bevel; the four corners carry the
             brass rivet from the dialog corner piece DD-CRNR.SHP.

The pictures in the wells come from the original unit sprites and sidebar cameos:

  Free for All  four Medium Tanks in four multiplayer house colours, one from each corner,
                all facing the centre (Dawn: MTNK with the engine's RemapGold, RemapRed,
                RemapLtBlue and RemapGreen tables; Red Alert: 2TNK with PALETTE.CPS rows
                gold, red, light blue and green).
  Gun Game      a rank ladder of three sidebar cameos climbing from bottom left to top
                right (Dawn: E1ICON, MTNKICON, HTNKICON, doubled with Scale2x to 64x48;
                Red Alert: E1ICON, 3TNKICON, 4TNKICON at their native 64x48), each cut to
                its central 56x40 (which drops Red Alert's caption strip) so the three
                overlap only at their corners, with a gold chevron at each step.
  Rally         two vehicles racing up and to the right for a chequered finish line drawn
                across the top of the well, the game's own waving flag (FLAGFLY.SHP, the
                capture-the-flag flag) on its pole beside it with its cloth chequered in
                its own shading (Dawn: a Nod Buggy leading a Hum-vee, BGGY and JEEP body
                and turret frames facing north-east, remapped red and gold; Red Alert: a
                Ranger leading a Light Tank, JEEP and 1TNK, the same way), all doubled
                with Scale2x.
"""
import io
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ccart as C  # noqa: E402
import bake_cameos as B  # noqa: E402

REPO = C.REPO
OUT = os.path.join(REPO, "art", "interface", "game-mode-icons-v01")
DAWN_KIT = os.path.join(REPO, "art", "interface", "enhanced-menu-v06")
TW, TH = 136, 120
KIT_FILES = {}


def kit_image(name):
    path = os.path.join(DAWN_KIT, name)
    import hashlib
    data = open(path, "rb").read()
    KIT_FILES[os.path.relpath(path, REPO)] = hashlib.sha1(data).hexdigest()
    return np.array(Image.open(io.BytesIO(data)).convert("RGBA"))


def over(dst, img, x, y, alpha=1.0):
    x, y = int(round(x)), int(round(y))
    h, w = img.shape[:2]
    sx0, sy0 = max(0, -x), max(0, -y)
    dx0, dy0 = max(0, x), max(0, y)
    dx1, dy1 = min(dst.shape[1], x + w), min(dst.shape[0], y + h)
    if dx1 <= dx0 or dy1 <= dy0:
        return
    src = img[sy0:sy0 + dy1 - dy0, sx0:sx0 + dx1 - dx0].astype(np.float32)
    a = src[..., 3:4] / 255.0 * alpha
    d = dst[dy0:dy1, dx0:dx1, :3].astype(np.float32)
    dst[dy0:dy1, dx0:dx1, :3] = (d * (1 - a) + src[..., :3] * a).astype(np.uint8)


def clip_over(dst, img, x, y, box, alpha=1.0):
    """over(), but only inside box = (x0, y0, x1, y1)."""
    x0, y0, x1, y1 = box
    sub = dst[y0:y1, x0:x1].copy()
    over(sub, img, x - x0, y - y0, alpha)
    dst[y0:y1, x0:x1] = sub


def shadowed(img, off=2, a=150):
    """The image with a soft dark drop shadow down and to the right."""
    h, w = img.shape[:2]
    out = np.zeros((h + off, w + off, 4), np.uint8)
    m = img[..., 3] > 0
    out[off:off + h, off:off + w][m] = (0, 0, 0, a)
    over(out, img, 0, 0)
    out[:h, :w, 3] = np.maximum(out[:h, :w, 3], img[..., 3])
    return out


# ---------------------------------------------------------------------------
# chassis
# ---------------------------------------------------------------------------

DAWN_WELL = (10, 11, 126, 109)          # covers the Nod emblem, leaves the plate's band
RA_WELL = (7, 7, TW - 7, TH - 7)


def dawn_plate():
    plate = kit_image("faction-Nod.png").copy()
    x0, y0, x1, y1 = DAWN_WELL
    rng = np.random.RandomState(7)
    well = np.full((y1 - y0, x1 - x0, 3), 15, np.float32)
    well += rng.normal(0, 2.2, well.shape[:2])[..., None]
    plate[y0:y1, x0:x1, :3] = np.clip(well, 0, 255).astype(np.uint8)
    # recessed bevel: shadow on the top and left, catch-light on the bottom and right
    plate[y0, x0:x1, :3] = (6, 6, 6)
    plate[y0:y1, x0, :3] = (6, 6, 6)
    plate[y0 + 1, x0 + 1:x1 - 1, :3] = (10, 10, 10)
    plate[y0 + 1:y1 - 1, x0 + 1, :3] = (10, 10, 10)
    plate[y1 - 1, x0:x1, :3] = (96, 96, 94)
    plate[y0:y1, x1 - 1, :3] = (96, 96, 94)
    plate[y1 - 2, x0 + 1:x1 - 1, :3] = (44, 44, 43)
    plate[y0 + 1:y1 - 1, x1 - 2, :3] = (44, 44, 43)
    # the bolt from the top right corner of the Dawn kit's slots frame
    frame = kit_image("slots-frame.png")
    bolt = frame[3:12, 657:666].copy()
    lum = bolt[..., :3].astype(int).mean(-1)
    bolt[..., 3] = np.where(lum > 30, 255, 0)
    for bx, by in ((1, 1), (TW - 10, 1), (1, TH - 10), (TW - 10, TH - 10)):
        over(plate, bolt, bx, by)
    plate[..., 3] = 255
    return plate


def ra_palette():
    rel, pcx = C.ra_entry("TITLE.PCX", prefer="HIRES.MIX")
    C._record("ra", rel, "TITLE.PCX", pcx)
    im = Image.open(io.BytesIO(pcx))
    return np.array(im.getpalette()[:768], np.uint8).reshape(256, 3)


def ra_piece(name, frame, pal):
    rel, data = C.ra_entry(name + ".SHP", prefer="HIRES.MIX")
    C._record("ra", rel, name + ".SHP", data, frame)
    s = C.Shape(data)
    idx = np.frombuffer(bytes(s.frame(frame)), np.uint8).reshape(s.height, s.width)
    rgba = np.zeros((s.height, s.width, 4), np.uint8)
    rgba[..., :3] = pal[idx]
    rgba[..., 3] = np.where(idx == 0, 0, 255)
    return rgba, idx


def ra_plate():
    pal = ra_palette()
    top, _ = ra_piece("DD-TOP", 0, pal)
    steel = top[0:4, :, :3]
    tile = np.zeros((TH, TW, 3), np.uint8)
    for y in range(TH):
        tile[y] = np.resize(np.roll(steel[y % 4], y * 29, axis=0), (TW, 3))
    plate = np.zeros((TH, TW, 4), np.uint8)
    plate[..., :3] = (tile * 0.55).astype(np.uint8)
    plate[..., 3] = 255
    x0, y0, x1, y1 = RA_WELL
    well = plate[y0:y1, x0:x1, :3].astype(np.float32)
    plate[y0:y1, x0:x1, :3] = (well * (1 - 200 / 255.0) + np.array((8, 6, 6)) * (200 / 255.0)).astype(np.uint8)
    # fine dark scan rows in the field, as the side tiles' wells carry
    plate[y0:y1:2, x0:x1, :3] = (plate[y0:y1:2, x0:x1, :3] * 0.8).astype(np.uint8)
    plate[0, :, :3] = (190, 190, 190)
    plate[:, 0, :3] = (190, 190, 190)
    plate[TH - 1, :, :3] = (30, 30, 30)
    plate[:, TW - 1, :3] = (30, 30, 30)
    # brass rivets out of the dialog's corner pieces (frames: top left, top right,
    # bottom left, bottom right), each cut to its round head
    for frame, (bx, by) in zip(range(4), ((0, 0), (TW - 14, 0), (0, TH - 14), (TW - 14, TH - 14))):
        crn, idx = ra_piece("DD-CRNR", frame, pal)
        cx, cy = (8, 8) if frame in (0, 2) else (crn.shape[1] - 9, 8)
        if frame in (2, 3):
            cy = crn.shape[0] - 9
        head = crn[cy - 7:cy + 7, cx - 7:cx + 7].copy()
        yy, xx = np.mgrid[0:14, 0:14]
        head[..., 3] = np.where((yy - 6.5) ** 2 + (xx - 6.5) ** 2 <= 6.6 ** 2, 255, 0)
        over(plate, head, bx, by)
    return plate


# ---------------------------------------------------------------------------
# contents
# ---------------------------------------------------------------------------
# Vehicle frames: 0 is north and they count anticlockwise (8 west, 16 south, 24 east),
# so the four diagonals are NW 4, SW 12, SE 20, NE 28.
NW, SW, SE, NE = 4, 12, 20, 28


def tank(game, name, facing, lut, times=1):
    return B.sprite(game, name, facing, remap=lut, turret=32 + facing, times=times)


def ffa(plate, well, game, name, luts):
    x0, y0, x1, y1 = well
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    corners = [(SE, -1, -1), (SW, 1, -1), (NE, -1, 1), (NW, 1, 1)]
    blast = C.trim(C.Art("td", "FBALL1.SHP").rgba(5, shadow=False)) if game == "td" else \
        C.trim(C.Art("ra", "FBALL1.SHP").rgba(5, shadow=False))
    clip_over(plate, blast, cx - blast.shape[1] / 2, cy - blast.shape[0] / 2, well)
    for (facing, sx, sy), lut in zip(corners, luts):
        t = tank(game, name, facing, lut)
        tx = cx + sx * 29 - t.shape[1] / 2
        ty = cy + sy * 25 - t.shape[0] / 2
        clip_over(plate, t, tx, ty, well)
    return plate


def card(cam, box):
    """A cameo cut to the window box = (x0, y0, x1, y1), closed with a one-pixel bevel in
    the Tiberian Dawn cameo cell's own light and dark greys."""
    x0, y0, x1, y1 = box
    c = cam[y0:y1, x0:x1].copy()
    c[0, :, :3] = B.col(194)
    c[:, 0, :3] = B.col(194)
    c[-1, :, :3] = B.col(197)
    c[:, -1, :3] = B.col(197)
    c[..., 3] = 255
    return c


def ladder(plate, well, cards):
    x0, y0, x1, y1 = well
    w, h = cards[0].shape[1], cards[0].shape[0]
    n = len(cards)
    dx = (x1 - x0 - 6 - w) / float(n - 1)
    dy = (y1 - y0 - 6 - h) / float(n - 1)
    pos = [(x0 + 3 + i * dx, y1 - 3 - h - i * dy) for i in range(n)]
    for (x, y), cam in zip(pos, cards):
        clip_over(plate, shadowed(cam, 3, 170), x, y, well)
    chev = B.chevron(B.col(149), 12, 5, 3)[::-1]          # pointing up
    for i in range(n - 1):
        xa, ya = pos[i]
        xb, yb = pos[i + 1]
        # in the step's corner: right of the lower card, under the upper one
        clip_over(plate, chev, xa + w + (dx - chev.shape[1]) / 2.0 + 1, ya + h - chev.shape[0] - 1, well)
    return plate


def checker_strip(w, h, cell, light, dark):
    """A chequered band, w x h, of cell-sized squares in the two colours given, opaque."""
    out = np.zeros((h, w, 4), np.uint8)
    yy, xx = np.mgrid[0:h, 0:w]
    on = ((xx // cell) + (yy // cell)) % 2 == 0
    out[..., :3] = np.where(on[..., None], np.array(light, np.uint8), np.array(dark, np.uint8))
    out[..., 3] = 255
    return out


def checkered_flag(game):
    """The game's own waving flag (FLAGFLY.SHP frame 0), its cloth (the house remap range
    and its shade ramp) chequered black and white in the cloth's own shading, doubled."""
    a = C.Art(game, "FLAGFLY.SHP")
    idx = a.indices(0)
    img = a.rgba(0, shadow=False).astype(np.float32)
    cloth = (idx >= 176) & (idx <= 191)
    cloth |= ((idx >= 115) & (idx <= 119)) if game == "td" else ((idx >= 82) & (idx <= 89))
    lum = img[..., :3].mean(-1)
    ref = lum[cloth].max() if cloth.any() else 255.0
    shade = np.clip(lum / max(ref, 1.0), 0.45, 1.0)
    yy, xx = np.mgrid[0:a.h, 0:a.w]
    on = ((xx // 3) + (yy // 3)) % 2 == 0
    base = np.where(on, 236.0, 22.0)
    for c in range(3):
        img[..., c] = np.where(cloth, base * shade, img[..., c])
    img = C.trim(img.astype(np.uint8))
    return B.up(img)


def racers(plate, well, game, lead, chase, luts):
    """Two vehicles racing up and to the right for a chequered line across the top of the
    well, with the chequered flag on its pole beside the line."""
    x0, y0, x1, y1 = well
    light, dark = (232, 232, 226), (18, 18, 18)
    # THE LINE: two rows of 6 pixel squares across the well, a dark rule either side
    band = checker_strip(x1 - x0, 12, 6, light, dark)
    by = y0 + 34
    clip_over(plate, band, x0, by, well)
    plate[by - 1, x0:x1, :3] = (8, 8, 8)
    plate[by + 12, x0:x1, :3] = (8, 8, 8)
    # THE FLAG on its pole, standing at the left end of the line
    flag = checkered_flag(game)
    clip_over(plate, shadowed(flag, 2, 140), x0 + 3, by + 8 - flag.shape[0], well)
    # THE RACE: the leader crossing the line, the chaser low and left behind it
    t2 = tank(game, chase, NE, luts[1], times=1)
    clip_over(plate, t2, x0 + 10, y1 - t2.shape[0] - 2, well)
    t1 = tank(game, lead, NE, luts[0], times=1)
    clip_over(plate, t1, x1 - t1.shape[1] - 8, by + 6 - t1.shape[0] // 2, well)
    return plate


def td_cameo(name):
    """A Tiberian Dawn cameo doubled with Scale2x, cut to its central 56x40."""
    a = C.Art("td", name + "ICON.SHP").rgba(0, shadow=False)
    return card(B.up(a), (4, 4, 60, 44))


def ra_cameo(name, prefer="HIRES.MIX"):
    """A Red Alert hires cameo at its own size, cut to its central 56x39, which leaves
    the caption strip behind."""
    a = C.Art("ra", name + "ICON.SHP", prefer).rgba(0, shadow=False)
    return card(a, (4, 0, 60, 39))


def bake():
    os.makedirs(OUT, exist_ok=True)
    tiles = {}
    td_luts = [C.td_remaps()[k] for k in ("GOLD", "RED", "LTBLUE", "GREEN")]
    ra_all = C.ra_remaps()
    ra_luts = [ra_all[k] for k in ("GOLD", "RED", "LTBLUE", "GREEN")]

    tiles["td-mode-ffa"] = ffa(dawn_plate(), DAWN_WELL, "td", "MTNK", td_luts)
    tiles["td-mode-gungame"] = ladder(dawn_plate(), DAWN_WELL, [td_cameo(n) for n in ("E1", "MTNK", "HTNK")])
    tiles["ra-mode-ffa"] = ffa(ra_plate(), RA_WELL, "ra", "2TNK", ra_luts)
    tiles["ra-mode-gungame"] = ladder(ra_plate(), RA_WELL, [ra_cameo(n) for n in ("E1", "3TNK", "4TNK")])
    tiles["td-mode-rally"] = racers(dawn_plate(), DAWN_WELL, "td", "BGGY", "JEEP",
                                    [C.td_remaps()["RED"], C.td_remaps()["GOLD"]])
    tiles["ra-mode-rally"] = racers(ra_plate(), RA_WELL, "ra", "JEEP", "1TNK",
                                    [ra_all["RED"], ra_all["GOLD"]])
    for k, img in tiles.items():
        assert img.shape == (TH, TW, 4)
        Image.fromarray(img, "RGBA").save(os.path.join(OUT, k + ".png"))

    meta = {
        "what": "Game-mode picker tiles, 136x120 RGBA, one per mode per game",
        "baker": "tools/art/gun_abilities/bake_mode_icons.py",
        "tiles": {
            "td-mode-ffa.png": "Dawn Free for All: TD MTNK.SHP body+turret frames 20/52, 12/44, "
                               "28/60, 4/36 with RemapGold, RemapRed, RemapLtBlue, RemapGreen "
                               "(tiberiandawn/const.cpp), Scale2x, converging on TD FBALL1.SHP "
                               "frame 5 at its own size; Dawn gunmetal plate",
            "td-mode-gungame.png": "Dawn Gun Game: TD E1ICON.SHP, MTNKICON.SHP, HTNKICON.SHP, "
                                   "Scale2x and cut to their central 56x40, climbing from bottom "
                                   "left to top right, drawn gold chevrons between; Dawn gunmetal plate",
            "ra-mode-ffa.png": "Red Alert Free for All: RA 2TNK.SHP body+turret frames 20/52, "
                               "12/44, 28/60, 4/36 remapped through PALETTE.CPS rows gold, red, "
                               "light blue, green, Scale2x, converging on RA FBALL1.SHP frame 5 at its own "
                               "size; Red Alert dialog-steel plate",
            "ra-mode-gungame.png": "Red Alert Gun Game: RA E1ICON.SHP, 3TNKICON.SHP, "
                                   "4TNKICON.SHP from HIRES.MIX at native size, cut to their central "
                                   "56x39 (which drops the caption strip), climbing; Red Alert dialog-steel plate",
            "td-mode-rally.png": "Dawn Rally: TD BGGY.SHP (RemapRed) leading JEEP.SHP (RemapGold), "
                                 "body+turret frames 28/60 (north-east), Scale2x, racing for a drawn "
                                 "chequered line of 6px squares; TD FLAGFLY.SHP frame 0 with its cloth "
                                 "(indices 176..191 and 115..119) chequered in its own shading, Scale2x; "
                                 "Dawn gunmetal plate",
            "ra-mode-rally.png": "Red Alert Rally: RA JEEP.SHP (PALETTE.CPS red row) leading 1TNK.SHP "
                                 "(gold row), body+turret frames 28/60, Scale2x, racing for a drawn "
                                 "chequered line; RA FLAGFLY.SHP frame 0 with its cloth (indices "
                                 "176..191 and 82..89) chequered in its own shading, Scale2x; Red Alert "
                                 "dialog-steel plate",
        },
        "chassis": {
            "dawn": "faction-Nod.png border band (gunmetal) with its emblem covered by a "
                    "recessed well; corner bolt cut from slots-frame.png (x 657..665, y 3..11)",
            "red_alert": "RA HIRES.MIX DD-TOP.SHP rows 0..3 in the HIRES.MIX TITLE.PCX "
                         "palette, darkened to 55% (the RA menu kit's plate()), dark inset, "
                         "1px bevel; brass rivet heads cut from DD-CRNR.SHP frames 0..3",
        },
        "kit_files_sha1": KIT_FILES,
        "source_entries": [dict(v) for k, v in sorted(C.LEDGER.items())],
    }
    with open(os.path.join(OUT, "SOURCES.json"), "w") as f:
        json.dump(meta, f, indent=2)
    review(tiles, os.path.join(OUT, "REVIEW.png"))
    print("wrote", ", ".join(sorted(tiles)))


def review(tiles, path):
    font = ImageFont.load_default()
    order = [("td-mode-ffa", "Dawn: Free for All"), ("td-mode-gungame", "Dawn: Gun Game"),
             ("td-mode-rally", "Dawn: Rally"),
             ("ra-mode-ffa", "Red Alert: Free for All"), ("ra-mode-gungame", "Red Alert: Gun Game"),
             ("ra-mode-rally", "Red Alert: Rally")]
    cw = TW + 8 + TW * 2 + 24
    ch = TH * 2 + 40
    cols = 3
    sheet = Image.new("RGB", (cw * cols + 8, ch * 2 + 30), (22, 22, 22))
    d = ImageDraw.Draw(sheet)
    d.text((8, 8), "Game-mode tiles v01: 136x120 at 1x and 2x (nearest), one row a game", fill=(235, 235, 235), font=font)
    for i, (k, label) in enumerate(order):
        x, y = 8 + (i % cols) * cw, 30 + (i // cols) * ch
        im = Image.fromarray(tiles[k], "RGBA")
        sheet.paste(im, (x, y))
        sheet.paste(im.resize((TW * 2, TH * 2), Image.NEAREST), (x + TW + 8, y))
        d.text((x, y + TH + 6), label, fill=(255, 220, 90), font=font)
        d.text((x, y + TH + 20), k + ".png", fill=(170, 170, 170), font=font)
    sheet.save(path)


if __name__ == "__main__":
    bake()
