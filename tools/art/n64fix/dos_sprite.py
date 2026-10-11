"""Reference frames for one building from the 1995 game's own sprites, for when no
Remastered install is available to refs.py.

    python3 tools/art/n64fix/dos_sprite.py TYPE <out_dir>

Run from the repository root. Reads TYPE.SHP (the building) and TYPEMAKE.SHP (its
construction animation) from the DOS disc data, draws them in the temperate palette at
4x, and writes dos_<NAME>_<frame>.png for a spread of frames plus sheet.png of them all.
Colour 0 is transparent in the game and is drawn slate grey here.
"""
import os
import sys

sys.path.insert(0, "menu/tools")
from mixshp import MixFile, Shape  # noqa: E402
from PIL import Image  # noqa: E402

if len(sys.argv) != 3:
    sys.exit(__doc__)
kind, out = sys.argv[1].upper(), sys.argv[2]
os.makedirs(out, exist_ok=True)

pal6 = MixFile("data/dosdata/LOCAL.MIX").read("TEMPERAT.PAL")
pal = [tuple(min(252, v << 2) for v in pal6[i * 3:i * 3 + 3]) for i in range(256)]

shots = []
for name in (kind, kind + "MAKE"):
    data = None
    for mixn in ("CONQUER.MIX", "GENERAL.MIX", "LOCAL.MIX"):
        try:
            data = MixFile("data/dosdata/" + mixn).read(name + ".SHP")
        except Exception:
            data = None
        if data:
            break
    if not data:
        print(name, "not found")
        continue
    s = Shape(data)
    print(name, mixn, s.frames, s.width, s.height)
    if name == kind:
        # The building's own frames: the healthy states and any idle animation come first.
        pick = list(range(0, min(s.frames, 32), 4))
    else:
        pick = list(range(0, s.frames, max(1, s.frames // 8))) + [s.frames - 1]
    for f in sorted(set(pick)):
        im = Image.new("RGB", (s.width, s.height))
        im.putdata([pal[i] if i else (60, 60, 70) for i in s.frame(f)])
        im = im.resize((s.width * 4, s.height * 4), Image.NEAREST)
        im.save(os.path.join(out, "dos_%s_%02d.png" % (name, f)))
        shots.append(im)

if shots:
    w = max(i.width for i in shots)
    h = max(i.height for i in shots)
    cols = 5
    sheet = Image.new("RGB", (w * cols, h * ((len(shots) + cols - 1) // cols)), (40, 40, 40))
    for k, im in enumerate(shots):
        sheet.paste(im, ((k % cols) * w, (k // cols) * h))
    sheet.save(os.path.join(out, "sheet.png"))
