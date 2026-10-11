#!/usr/bin/env python3
"""Bake ground mottle sheets into mottle.pack.

A MOTTLE is a patch of ground variation laid over plain terrain: a blended quad on a
cell's own four corners, so it follows the ground and anything standing on that ground
covers it. It is NOT one of the cartridge's decals. That word already means the scorch
marks, craters and building aprons this renderer draws from the cartridge's own art, and
a second meaning would make every later search ambiguous.

INPUT is a folder of four-map source art per patch, of which this reads two:

    <name>_Albedo.jpg     the colour
    <name>_Mask.jpg       the blend shape, a soft organic blob with feathered edges

OUTPUT is one RGBA sheet per patch, albedo in RGB and the mask in ALPHA, which is the
layout the other art packs in this tree already use. The normal and roughness maps that
come with such art are deliberately NOT read: a mottle writes no depth, so the terrain's
own normal triangle still wins the depth test at every mottle pixel and the lighting chain
lights the mottle exactly as it lights the ground beside it. Submitting a normal would
instead REPLACE the ground's normal inside the mask, which is a design question rather
than a saving, and it would have to survive being stored in a lossy format first.

EVERY PATCH CARRIES ITS OWN MIP CHAIN, and that is a Tier 1 decision rather than a
convenience. The Voodoo 2 cannot hold a texture larger than 256 on a side at all, so a
single large page with the patches laid out on it is a dead end there; and a chain built
over such a page bleeds one patch into its neighbours at every level below the cell size.
Chains are built per patch, and a loader is free to assemble each level into a page of
its own.

THE CANONICAL BAKE, and every number in it was arrived at by measuring rather than by
taste. The ground carries 264 texels to a cell, so a patch spanning two to four of them
needs 528 to 1056 to match: 1024 covers that. Half the source width fills 1024 at the
photograph's own resolution, with nothing upscaled. Six tenths of the way to the ground's
colour is as far as a patch can be moved before it stops being soil or gravel or leaves.

    python3 bake_mottle.py --src <folder> -o <out>.pack --side 1024 --grain 2         --toward art/generated/temperate_grass/v3_color_matched --toward-amount 0.60

    python3 bake_mottle.py --src <folder> -o <out>.pack --only GDM_04 --report
"""

import argparse
import os
import struct
import sys
import zlib

MAGIC = b"CNC3DMOT"

# The version is its own field rather than a digit inside the magic. A reader that spells
# the check as a comparison over the magic can only ever accept ONE version, and then a
# newer pack is reported as "not a mottle pack", which is both wrong and hard to act on.
VERSION = 1

# The smallest level worth storing. Below this a patch is a couple of texels and the
# chain is all overhead.
MIN_SIDE = 4


def load_rgba(albedo_path, mask_path, side, grain):
    """Albedo in RGB, mask in alpha, square, at `side`.

    GRAIN IS WHY THESE READ AS GROUND AND NOT AS A SMUDGE, and it is a question of scale
    rather than of resolution. The source photographs capture on the order of a couple of
    metres of real ground. A cell here is several times that, so laying one photograph
    across one cell stretches its gravel and its blades until the surface has almost no
    high frequency left, and it lands soft beside terrain art made for exactly this size.
    Raising the stored resolution does not help: the screen samples whichever level
    matches it, and the missing detail is missing from the CONTENT, not from the file.
    Measured at 4K and close range, 256 and 512 came out within a delta of 31 on a 765
    scale, which is invisible.

    So the albedo is TILED `grain` times across the patch while the mask is laid over it
    ONCE. The blob keeps its single soft shape and the surface inside it repeats near the
    ground's own frequency. The seams the tiling introduces sit under a mask that is
    fading out towards its edges."""
    from PIL import Image
    with Image.open(albedo_path) as a:
        src = a.convert("RGB")
        if grain > 1:
            # A CROP, NOT A REPEAT. Tiling the photograph was tried first and is worse
            # than the softness it cures: these captures are not seamless, so repeating
            # one lays a visible lattice across the patch, which reads as a printing
            # fault rather than as ground. Taking a SMALLER PIECE of the same photograph
            # at its own resolution gets the same effect on scale with nothing to repeat.
            # The piece is taken from the middle, where a ground capture is squarest.
            w, h = src.size
            keep = max(8, int(min(w, h) / float(grain)))
            left, top = (w - keep) // 2, (h - keep) // 2
            alb = src.crop((left, top, left + keep, top + keep)).resize((side, side),
                                                                       Image.LANCZOS)
        else:
            alb = src.resize((side, side), Image.LANCZOS)
    with Image.open(mask_path) as m:
        msk = m.convert("L").resize((side, side), Image.LANCZOS)
    out = alb.convert("RGBA")
    out.putalpha(msk)
    return out


def toward_ground(img, ref_stats, amount):
    """Pull a patch part of the way to the ground it will lie on.

    A patch is meant to be a DIFFERENT material -- soil, gravel, dry leaves -- so grading
    it all the way to the ground would just make more ground. But the source photographs
    were lit and exposed somewhere else entirely, and dropped on this terrain unaltered
    they arrive far lighter and far warmer than everything around them. That reads as
    something pasted on rather than as the ground varying, which is the whole purpose.

    So each channel is moved a fraction of the way from the patch's own mean and spread to
    the ground's, which keeps the patch's texture and its identity while taking away the
    difference that has no reason to be there. `amount` 0 leaves the art alone and 1 makes
    it agree with the ground completely; the useful settings are in between."""
    import numpy as np
    if amount <= 0.0:
        return img
    a = np.asarray(img).astype(np.float32)
    rgb = a[..., :3]
    m, s = rgb.mean(axis=(0, 1)), rgb.std(axis=(0, 1))
    tm, ts = ref_stats
    nm = m + (tm - m) * amount
    ns = s + (ts - s) * amount
    scale = np.where(s > 1e-3, ns / np.maximum(s, 1e-3), 1.0)
    out = (rgb - m) * scale + nm
    a[..., :3] = np.clip(out, 0, 255)
    from PIL import Image
    return Image.fromarray(a.astype("uint8"), "RGBA")


def ground_stats(folder):
    """Mean and spread of the ground the patches will lie on, over its own art."""
    import glob
    import numpy as np
    from PIL import Image
    files = sorted(glob.glob(os.path.join(folder, "*.png")))
    if not files:
        raise SystemExit("no ground art in %s" % folder)
    px = np.concatenate([np.asarray(Image.open(f).convert("RGB")).astype(np.float32).reshape(-1, 3)
                         for f in files], axis=0)
    return px.mean(axis=0), px.std(axis=0)


def chain(img):
    """The mip chain for one patch, largest first, halving to MIN_SIDE."""
    from PIL import Image
    levels = [img]
    cur = img
    while cur.width > MIN_SIDE:
        cur = cur.resize((cur.width // 2, cur.height // 2), Image.LANCZOS)
        levels.append(cur)
    return levels


def find_patches(src, only):
    """Every patch in `src` that has BOTH maps. One without a mask is not a mottle: the
    mask is the blend shape, and without it the patch would land as a hard-edged square
    of photograph on the ground. The source art this was built from ships one such patch,
    which is why this refuses by pairing rather than by listing albedos."""
    names = {}
    for f in sorted(os.listdir(src)):
        for suffix in ("_Albedo.jpg", "_Mask.jpg"):
            if f.endswith(suffix):
                stem = f[: -len(suffix)]
                names.setdefault(stem, {})[suffix] = os.path.join(src, f)
    out, skipped = [], []
    for stem in sorted(names):
        if only and only not in stem:
            continue
        got = names[stem]
        if "_Albedo.jpg" in got and "_Mask.jpg" in got:
            out.append((stem, got["_Albedo.jpg"], got["_Mask.jpg"]))
        else:
            skipped.append(stem)
    return out, skipped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="folder of <name>_Albedo.jpg and _Mask.jpg")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--side", type=int, default=256, help="top level size, power of two")
    ap.add_argument("--only", default="", help="substring filter over patch names")
    ap.add_argument("--grain", type=int, default=3,
                    help="tile the albedo this many times across a patch so its detail "
                         "sits at the ground scale; 1 stretches one photograph over the cell")
    ap.add_argument("--toward", default="",
                    help="folder of the ground art these will lie on; grades the patches "
                         "part of the way to it")
    ap.add_argument("--toward-amount", type=float, default=0.0,
                    help="0 leaves the art alone, 1 makes it agree with the ground")
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    if args.side & (args.side - 1):
        raise SystemExit("--side must be a power of two")

    patches, skipped = find_patches(args.src, args.only)
    if not patches:
        raise SystemExit("no patch in %s has both an albedo and a mask" % args.src)
    for stem in skipped:
        print("skipped %s: it has no mask, so it has no blend shape" % stem)

    blob = bytearray()
    blob += MAGIC
    blob += struct.pack("<II", VERSION, len(patches))

    ref = ground_stats(args.toward) if args.toward and args.toward_amount > 0 else None
    if ref is not None:
        print('ground reference: mean=[%.1f, %.1f, %.1f] spread=[%.1f, %.1f, %.1f]'
              % (tuple(ref[0]) + tuple(ref[1])))
    total = 0
    for stem, alb, msk in patches:
        img = load_rgba(alb, msk, args.side, args.grain)
        if ref is not None:
            img = toward_ground(img, ref, args.toward_amount)
        levels = chain(img)
        name = stem.encode("ascii")[:31]
        blob += struct.pack("<I", len(name)) + name
        blob += struct.pack("<I", len(levels))
        for lv in levels:
            raw = lv.tobytes()
            comp = zlib.compress(raw, 9)
            blob += struct.pack("<IIII", lv.width, lv.height, len(raw), len(comp))
            blob += comp
            total += len(comp)
        if args.report:
            print("%-34s %d levels, %d..%d" % (stem, len(levels), levels[0].width,
                                               levels[-1].width))

    outdir = os.path.dirname(os.path.abspath(args.out))
    if outdir and not os.path.isdir(outdir):
        os.makedirs(outdir)
    with open(args.out, "wb") as fh:
        fh.write(blob)
    print("%s: %d patches, %d bytes (%.1f MB compressed image data)"
          % (args.out, len(patches), len(blob), total / 1048576.0))


if __name__ == "__main__":
    main()
