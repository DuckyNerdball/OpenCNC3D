#!/usr/bin/env python3
"""Step 1 of the recipe: pull the local references for one building out of the player's
own Remastered Collection install, as PNGs and one contact sheet.

    OPENCNC3D_REMASTER_DIR=<install> python3 tools/art/n64fix/refs.py NUKE <work_dir>/refs

Writes <TYPE>.ZIP's first frame (the finished building) and every eighth frame of
<TYPE>MAKE.ZIP (the construction animation, which builds the building up from its
foundation and so shows sides and walls the finished view hides), plus sheet.png.
Nothing is written into the repository: EA's art stays in the install.

The online half of the reference step (console screenshots, strategy guide renders) is
a search, not a script; the README says what to look for.
"""
import glob
import io
import os
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "remaster"))
import meg  # noqa: E402
from PIL import Image  # noqa: E402

TEX = "DATA\\ART\\TEXTURES\\SRGB\\TIBERIAN_DAWN\\STRUCTURES\\%s.ZIP"


def find_meg():
    roots = [os.environ.get("OPENCNC3D_REMASTER_DIR", "")]
    roots += glob.glob("/Volumes/*/Steam/steamapps/common/Command & Conquer Remastered")
    roots += glob.glob(os.path.expanduser("~/Library/Application Support/Steam/steamapps/common/"
                                          "Command & Conquer Remastered"))
    for r in roots:
        for p in glob.glob(os.path.join(r, "**", "TEXTURES_TD_SRGB.MEG"), recursive=True) if r else []:
            return p
    return None


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    code, out = sys.argv[1], sys.argv[2]
    path = find_meg()
    if not path:
        print("no Remastered install found: set OPENCNC3D_REMASTER_DIR")
        return 1
    m = meg.Meg(path)
    os.makedirs(out, exist_ok=True)
    shots = []
    for name, step in ((code, 999), (code + "MAKE", 8)):
        blob = m.read(TEX % name)
        if blob is None:
            print("%s: not in the install" % name)
            continue
        z = zipfile.ZipFile(io.BytesIO(blob))
        frames = sorted(n for n in z.namelist() if n.lower().endswith(".tga"))
        pick = frames[:1] if step == 999 else frames[::step] + frames[-1:]
        for n in pick:
            im = Image.open(io.BytesIO(z.read(n))).convert("RGBA")
            dst = os.path.join(out, "rm_" + os.path.basename(n)[:-4] + ".png")
            im.save(dst)
            shots.append(im)
    if shots:
        w = max(i.width for i in shots)
        h = max(i.height for i in shots)
        sheet = Image.new("RGBA", (w * len(shots), h), (40, 40, 40, 255))
        for k, im in enumerate(shots):
            sheet.alpha_composite(im, (k * w, 0))
        sheet.save(os.path.join(out, "sheet.png"))
    print("%s: %d reference frames -> %s" % (code, len(shots), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
