#!/usr/bin/env python3
"""Make a CLOTH MASK for a Tempest Rising soldier's colour map: white where the paint is
the uniform's fabric (the camouflage, the plate carrier, pouches and webbing), black
elsewhere (skin, metal, dark plastic, lenses, and the team mask's own plates).

    python3 tools/art/cloth_mask.py \\
        art/models/tempest_rising_gdi_rifleman/textures/T_GDI_Rifleman

reads <stem>_D.png and <stem>_C.png and writes <stem>_Cloth.png at the colour map's size.
ue_c3m.py packs it into the alpha of the surface map, where the renderer reads it as the
area a side's CLOTH colour replaces.

WHY IT IS COMPUTED. Tempest Rising has no such mask: its _C is the team area only, and
the source folder's _MASK is the same picture. The rifleman's fabric is one family of
colour, tan to brown (hue 15 to 62 degrees, saturation above 0.14), and nothing else on
the sheet is: skin is the exception, and it is told apart by being redder (red over green
above 1.2) and by where it is -- the face and ears sit in the sheet's top left corner, and
testing redness everywhere would punch holes in the brown webbing, which is as red. The
result is cleaned of single-texel specks (a 5 texel median at 1024, scaled with the
sheet) and softened by a texel so its edge filters rather than stair-steps.

The rules are this one sheet's. Another soldier's paint needs its numbers checked against
an overlay before this is trusted on it."""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter


def cloth_mask(stem):
    d_img = Image.open(stem + "_D.png").convert("RGB")
    n = d_img.size[0]
    k = max(1, n // 1024)                       # the filter sizes were set at 1024
    d = np.asarray(d_img, np.float32) / 255.0
    team = np.zeros(d.shape[:2], np.float32)
    if os.path.exists(stem + "_C.png"):
        c = Image.open(stem + "_C.png").convert("RGB")
        if c.size != d_img.size:
            c = c.resize(d_img.size, Image.LANCZOS)
        team = np.asarray(c, np.float32).max(axis=2) / 255.0
    hsv = np.asarray(d_img.convert("HSV"), np.float32) / 255.0
    h, s, v = hsv[..., 0] * 360.0, hsv[..., 1], hsv[..., 2]
    rg = d[..., 0] / np.maximum(d[..., 1], 1e-3)

    skin = (rg > 1.2) & (v > 0.35)
    corner = np.zeros_like(skin)
    corner[: int(0.3 * n), : int(0.33 * n)] = True
    skin &= corner
    skin = np.asarray(Image.fromarray((skin * 255).astype(np.uint8))
                      .filter(ImageFilter.MaxFilter(15 * k - (k - 1) % 2))) > 0

    m = (s > 0.14) & (h > 15.0) & (h < 62.0) & (team < 0.5) & ~skin
    img = Image.fromarray((m * 255).astype(np.uint8))
    img = img.filter(ImageFilter.MedianFilter(5 * k - (k - 1) % 2))
    img = img.filter(ImageFilter.GaussianBlur(k))
    return img


def main(argv):
    if len(argv) != 1:
        print(__doc__)
        return 2
    stem = argv[0]
    img = cloth_mask(stem)
    img.save(stem + "_Cloth.png", optimize=True)
    a = np.asarray(img, np.float32) / 255.0
    print("CLOTH|%s_Cloth.png|%dx%d|covers %.1f%% of the sheet" % (
        os.path.basename(stem), img.size[0], img.size[1], 100.0 * a.mean()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
