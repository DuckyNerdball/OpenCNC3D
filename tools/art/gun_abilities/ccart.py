"""ccart.py -- read the original Tiberian Dawn (1995 DOS) and Red Alert (1996) art for the
Gun Game ability cameos and the mode-picker icons.

Every image here comes out of the original archives in data/dosdata and data/radata,
decoded by the project's own ports of the engine readers:

    menu/tools/mixshp.py   MIX index, LCW, XOR delta and keyframe SHP (Build_Frame)
    tools/ra3d/ramix.py    Red Alert's MIX, including the encrypted 1996 headers

Palettes are widened the way the engines widen them: a 6-bit DAC value v becomes
min(252, v << 2). Index 0 is transparent, index 4 is the shadow ghost (drawn here as a
translucent black), and the house band is remapped with the engines' own tables:
Tiberian Dawn's Remap* arrays in tiberiandawn/const.cpp (band 176..191), Red Alert's
rows of PALETTE.CPS (band 80..95), as tools/art/remaster/infantry/ra_sprites.py reads it.

Each loader records what it read (archive, entry, frame, sha1 of the entry bytes) in
LEDGER (and TOUCHED, which a baker clears per picture), so a baker can write a
SOURCES.json that says exactly which bytes and frames made each picture.
"""
import hashlib
import os
import re
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(REPO, "menu", "tools"))
sys.path.insert(0, os.path.join(REPO, "tools", "ra3d"))
from mixshp import MixFile, Shape, lcw_uncompress  # noqa: E402
from ramix import RaMix                            # noqa: E402

DOS = os.path.join(REPO, "data", "dosdata")
RADATA = os.path.join(REPO, "data", "radata")
AFTERMATH = os.path.join(RADATA, "expansions", "aftermath")
TD_SRC = os.path.join(REPO, "brain", "vanilla", "tiberiandawn")
RA_SRC = os.path.join(REPO, "brain", "vanilla", "redalert")

TD_ARCHIVES = ["CONQUER.MIX", "LOCAL.MIX", "TEMPERAT.MIX", "GENERAL.MIX", "DESERT.MIX"]
# hires first, so a cameo resolves to its 64x48 form; the Aftermath archives last
RA_ARCHIVES = [(RADATA, "HIRES.MIX"), (RADATA, "CONQUER.MIX"), (RADATA, "LORES.MIX"),
               (RADATA, "LOCAL.MIX"), (RADATA, "TEMPERAT.MIX"), (RADATA, "SNOW.MIX"),
               (RADATA, "GENERAL.MIX"), (AFTERMATH, "HIRES1.MIX"), (AFTERMATH, "EXPAND2.MIX"),
               (AFTERMATH, "LORES1.MIX")]

LEDGER = {}
TOUCHED = {}        # key -> frames read since the caller last cleared it
_MIX = {}


def _rel(path):
    return os.path.relpath(path, REPO)


def _record(game, archive, entry, data, frame=None):
    key = "%s:%s:%s" % (game, archive, entry)
    seen = TOUCHED.setdefault(key, [])
    if frame is not None and frame not in seen:
        seen.append(frame)
        seen.sort()
    row = LEDGER.setdefault(key, {"game": game, "archive": archive, "entry": entry,
                                  "bytes": len(data), "sha1": hashlib.sha1(data).hexdigest(),
                                  "frames": []})
    if frame is not None and frame not in row["frames"]:
        row["frames"].append(frame)
        row["frames"].sort()


def _td_mixes():
    if "td" not in _MIX:
        _MIX["td"] = [(n, MixFile(os.path.join(DOS, n))) for n in TD_ARCHIVES
                      if os.path.exists(os.path.join(DOS, n))]
    return _MIX["td"]


def _ra_mixes():
    if "ra" not in _MIX:
        _MIX["ra"] = [(_rel(os.path.join(d, n)), RaMix(os.path.join(d, n))) for d, n in RA_ARCHIVES
                      if os.path.exists(os.path.join(d, n))]
    return _MIX["ra"]


def td_entry(name):
    for rel, mix in _td_mixes():
        if mix.has(name):
            data = mix.read(name)
            return "data/dosdata/" + rel, data
    raise KeyError("Tiberian Dawn entry missing: " + name)


def ra_entry(name, prefer=None):
    mixes = _ra_mixes()
    if prefer:
        mixes = [m for m in mixes if m[0].endswith(prefer)] + [m for m in mixes if not m[0].endswith(prefer)]
    for rel, mix in mixes:
        data = mix.read(name)
        if data is not None:
            return rel, data
    raise KeyError("Red Alert entry missing: " + name)


# ---------------------------------------------------------------------------
# palettes and remaps
# ---------------------------------------------------------------------------

def _widen(pal6):
    return np.minimum(252, np.frombuffer(pal6, np.uint8).astype(int) << 2).reshape(256, 3).astype(np.uint8)


def td_palette():
    rel, data = td_entry("TEMPERAT.PAL")
    _record("td", rel, "TEMPERAT.PAL", data)
    return _widen(data)


def ra_palette():
    rel, data = ra_entry("TEMPERAT.PAL", prefer="LOCAL.MIX")
    _record("ra", rel, "TEMPERAT.PAL", data)
    return _widen(data)


def td_remaps():
    """{'GOLD': lut, 'RED': lut, ...} from tiberiandawn/const.cpp's Remap tables."""
    text = open(os.path.join(TD_SRC, "const.cpp"), encoding="latin-1").read()
    out = {}
    for m in re.finditer(r"unsigned char const Remap(\w+)\[256\]\s*=\s*\{(.*?)\};", text, re.S):
        body = re.sub(r"//[^\n]*", "", m.group(2))
        vals = [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+|\d+", body)]
        if len(vals) == 256:
            out[m.group(1).upper()] = np.array(vals, np.uint8)
    return out


def load_cps(data):
    """A Westwood CPS picture (u16 size, u16 compression 4 = LCW, u32 size, u16 palette)."""
    import struct
    size, comp, usize, palsize = struct.unpack_from("<HHIH", data, 0)
    pos = 10 + palsize
    if comp == 4:
        buf, _ = lcw_uncompress(data, pos, usize)
        return bytes(buf)
    return bytes(data[pos:pos + usize])


def ra_remaps():
    """Red Alert's eight multiplayer colours as full 256 luts, in PCOLOR order: gold,
    light blue, red, green, orange, grey, blue, brown. PALETTE.CPS row 0 is the band a
    sprite is drawn in, row n the indices that side's colour maps it to."""
    rel, data = ra_entry("PALETTE.CPS", prefer="LOCAL.MIX")
    _record("ra", rel, "PALETTE.CPS", data)
    cps = load_cps(data)
    base = np.frombuffer(cps[:16], np.uint8)
    names = ["GOLD", "LTBLUE", "RED", "GREEN", "ORANGE", "GREY", "BLUE", "BROWN"]
    out = {}
    for r, n in enumerate(names):
        row = np.frombuffer(cps[r * 320:r * 320 + 16], np.uint8)
        lut = np.arange(256, dtype=np.uint8)
        lut[base] = row
        out[n] = lut
    return out


# ---------------------------------------------------------------------------
# shapes
# ---------------------------------------------------------------------------

class Art(object):
    """One SHP from one game. .rgba(frame, remap=None) gives an RGBA numpy image: index 0
    transparent, index 4 (the shadow ghost) half black."""

    def __init__(self, game, name, prefer=None):
        self.game, self.name = game, name
        if game == "td":
            self.archive, data = td_entry(name)
            self.pal = td_palette()
        else:
            self.archive, data = ra_entry(name, prefer)
            self.pal = ra_palette()
        self.data = data
        self.shp = Shape(data)
        self.w, self.h, self.frames = self.shp.width, self.shp.height, self.shp.frames
        _record(game, self.archive, name, data)

    def indices(self, n):
        _record(self.game, self.archive, self.name, self.data, n)
        return np.frombuffer(bytes(self.shp.frame(n)), np.uint8).reshape(self.h, self.w)

    def rgba(self, n, remap=None, shadow=True):
        idx = self.indices(n)
        if remap is not None:
            idx = remap[idx]
        out = np.zeros((self.h, self.w, 4), np.uint8)
        solid = (idx != 0) & (idx != 4)
        out[solid, :3] = self.pal[idx[solid]]
        out[solid, 3] = 255
        if shadow:
            out[idx == 4] = (0, 0, 0, 110)
        return out


def td(name, frame=0, remap=None, shadow=True):
    return Art("td", name).rgba(frame, remap, shadow)


def ra(name, frame=0, remap=None, shadow=True, prefer=None):
    return Art("ra", name, prefer).rgba(frame, remap, shadow)


def to_image(a):
    return Image.fromarray(np.ascontiguousarray(a), "RGBA")


def scale2x(a):
    """Scale2x (EPX): doubles pixel art without inventing colours, keeping diagonals."""
    h, w = a.shape[:2]
    p = np.pad(a, ((1, 1), (1, 1), (0, 0)), mode="edge")
    B, D, F, H, E = p[:-2, 1:-1], p[1:-1, :-2], p[1:-1, 2:], p[2:, 1:-1], p[1:-1, 1:-1]
    eq = lambda x, y: np.all(x == y, axis=-1)
    e0 = np.where((eq(D, B) & ~eq(B, F) & ~eq(D, H))[..., None], D, E)
    e1 = np.where((eq(B, F) & ~eq(B, D) & ~eq(F, H))[..., None], F, E)
    e2 = np.where((eq(D, H) & ~eq(D, B) & ~eq(H, F))[..., None], D, E)
    e3 = np.where((eq(H, F) & ~eq(D, H) & ~eq(B, F))[..., None], F, E)
    out = np.zeros((h * 2, w * 2, a.shape[2]), a.dtype)
    out[0::2, 0::2], out[0::2, 1::2], out[1::2, 0::2], out[1::2, 1::2] = e0, e1, e2, e3
    return out


def trim(a):
    ys, xs = np.nonzero(a[..., 3] > 0)
    if len(xs) == 0:
        return a
    return a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
