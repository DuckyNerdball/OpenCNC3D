#!/usr/bin/env python3
"""Upscale cartridge textures for the fixed model, tile-aware, no machine learning.

    python3 tools/art/n64fix/upscale.py <in.png> <out.png> [--factor 4] [--method hybrid]
    python3 tools/art/n64fix/upscale.py --dir <src_dir>     every texture extract.py wrote,
                                                            into <src_dir>/x4/

The cartridge's textures are 8 to 64 texels a side and mostly repeat or wrap, so every
filter here runs on a 3x3 tiled copy and crops the middle: a seam that was invisible at
16x16 stays invisible at 64x64.

Methods:
  nearest   the texels made bigger. The reference, not a result.
  lanczos   smooth. Right for noise (concrete, dirt), soft on drawn lines.
  epx       Scale2x applied until the factor is reached: keeps drawn lines and rivets
            crisp and diagonal edges stepped at the new resolution, not the old one.
  hybrid    epx for the shapes, lanczos for the shading inside them, then a light
            sharpen. The default, chosen by comparing all four on the Power Plant.

Alpha is scaled the same way. The factor must be a power of two; the result stays a
power of two, and 256 a side is the Win98 / Voodoo 2 ceiling the pack path observes.
"""
import sys

import numpy as np
from PIL import Image, ImageFilter


def epx2(a):
    """Scale2x on an HxWxC array, edges wrapped."""
    B = np.roll(a, 1, 0)
    D = np.roll(a, 1, 1)
    F = np.roll(a, -1, 1)
    H = np.roll(a, -1, 0)
    eq = lambda x, y: np.all(x == y, axis=-1, keepdims=True)
    P = a
    c1 = np.where(eq(D, B) & ~eq(D, H) & ~eq(B, F), B, P)
    c2 = np.where(eq(B, F) & ~eq(B, D) & ~eq(F, H), F, P)
    c3 = np.where(eq(D, H) & ~eq(D, B) & ~eq(H, F), D, P)
    c4 = np.where(eq(H, F) & ~eq(D, H) & ~eq(B, F), F, P)
    h, w, c = a.shape
    out = np.empty((h * 2, w * 2, c), a.dtype)
    out[0::2, 0::2] = c1
    out[0::2, 1::2] = c2
    out[1::2, 0::2] = c3
    out[1::2, 1::2] = c4
    return out


def quantise(a, step=6):
    """EPX compares texels for equality; cartridge textures are palettised, so equal
    colours are exactly equal, but a small snap keeps near-identical shades together."""
    return (a // step) * step


def upscale(im, factor=4, method="hybrid"):
    im = im.convert("RGBA")
    w, h = im.size
    tiled = Image.new("RGBA", (w * 3, h * 3))
    for y in range(3):
        for x in range(3):
            tiled.paste(im, (x * w, y * h))
    W, H = w * factor, h * factor

    def crop(big):
        return big.crop((W, H, 2 * W, 2 * H))

    if method == "nearest":
        return crop(tiled.resize((w * 3 * factor, h * 3 * factor), Image.NEAREST))
    lz = tiled.resize((w * 3 * factor, h * 3 * factor), Image.LANCZOS)
    if method == "lanczos":
        return crop(lz)
    a = np.asarray(tiled).astype(np.int16)
    e = a.copy()
    q = quantise(a)
    f = 1
    while f < factor:
        stack = np.concatenate([e, q], axis=-1)
        stack = epx2(stack)
        e, q = stack[..., :4], stack[..., 4:]
        f *= 2
    ep = Image.fromarray(np.clip(e, 0, 255).astype(np.uint8), "RGBA")
    if method == "epx":
        return crop(ep)
    # hybrid: EPX structure, softened by a sub-texel blur, blended with Lanczos shading
    soft = ep.filter(ImageFilter.GaussianBlur(radius=factor * 0.28))
    mix = Image.blend(soft, lz, 0.35)
    mix = mix.filter(ImageFilter.UnsharpMask(radius=factor * 0.5, percent=60, threshold=2))
    return crop(mix)


def main():
    a = sys.argv[1:]
    if a and a[0] == "--dir":
        import glob
        import os
        d = a[1]
        os.makedirs(os.path.join(d, "x4"), exist_ok=True)
        n = 0
        for p in sorted(glob.glob(os.path.join(d, "*_t*_*.png"))):
            upscale(Image.open(p)).save(os.path.join(d, "x4", os.path.basename(p)))
            n += 1
        print("upscaled %d textures into %s" % (n, os.path.join(d, "x4")))
        return
    factor, method = 4, "hybrid"
    if "--factor" in a:
        i = a.index("--factor")
        factor = int(a[i + 1])
        del a[i:i + 2]
    if "--method" in a:
        i = a.index("--method")
        method = a[i + 1]
        del a[i:i + 2]
    src, dst = a
    out = upscale(Image.open(src), factor, method)
    if max(out.size) > 256:
        print("warning: %s is %dx%d, over the 256 texel Voodoo 2 limit" % ((dst,) + out.size))
    out.save(dst)


if __name__ == "__main__":
    main()
