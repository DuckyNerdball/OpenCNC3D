#!/usr/bin/env python3
"""Turn a cross-layout sky photograph into sky.pack: a cube map plus its lighting.

    python3 game/bake_sky.py SKY_CROSS.png -o game/sky.pack

WHAT THIS IS FOR. The authored-model shader evaluates a real microfacet BRDF, and half of
that equation is the light arriving from every direction that is not the sun. Before this
existed that half was a guess: a sky colour from above and a bounce colour from below,
blended on the surface normal. It is a reasonable guess for a diffuse surface and a bad
one for a metal, because a metal has no diffuse term at all and reflects the surroundings
or it reflects nothing. A shipped roughness floor existed only to stop polished texels
reading as black dots, which is the guess admitting it was one.

WHAT COMES OUT, and why there are two different convolutions of the same picture:

  THE ROUGHNESS CHAIN is the specular half. A mirror reflects one direction; a rough
  surface reflects a cone, and the rougher it is the wider the cone. Each mip level is
  the sky pre-blurred by the cone of one roughness, so the shader picks a level instead
  of integrating a lobe per pixel. Level 0 is the sky untouched, the last level is nearly
  uniform. This is the standard split-sum prefilter and the shader half of it is in
  model3d_mod.h.

  THE IRRADIANCE MAP is the diffuse half: for every direction, the cosine-weighted
  average of everything the surface can see from there. Tiny, because it is smooth by
  construction; 32 a side is more than a diffuse term can show.

DONE HERE AND NOT AT LOAD TIME, for the same reason the tiberium crystals and the trees
are baked: it makes the result a file that can be looked at, diffed and shipped, rather
than a few hundred milliseconds every player pays on every boot and nobody can inspect.
It also keeps the renderer free of a render-to-cubemap path, which is a good deal more GL
surface than reading a file.

THE SUN IS NOT IN HERE, and that is deliberate rather than a limitation. A photograph is
eight bits: its sun is clipped flat at white, so using it as a key light would give a
washed-out smear where a sun should be. The renderer already has a real directional sun
with a real shadow map, so this file supplies the ambient and the reflections and the sun
stays analytic. That is also how a modern engine splits the same job.
"""
import argparse
import os
import struct
import sys

import numpy as np
from PIL import Image

MAGIC = b"CNC3DSKY"
VERSION = 1

# GL's own face order, so the loader can upload them in a loop from
# GL_TEXTURE_CUBE_MAP_POSITIVE_X without a lookup table.
FACES = ["posX", "negX", "posY", "negY", "posZ", "negZ"]

# Where each face sits in a horizontal cross. Column then row, in cells:
#
#         +Y
#    -X   +Z   +X   -Z
#         -Y
#
# This is the layout a panorama tool writes when asked for a "horizontal cross", and it
# is the one the supplied sky uses. --layout names others if a second sky ever arrives
# in a different shape.
CROSS_H = {"posY": (1, 0), "negX": (0, 1), "posZ": (1, 1),
           "posX": (2, 1), "negZ": (3, 1), "negY": (1, 2)}


def load_cross(path, layout):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    cols, rows = (4, 3) if layout == "cross" else (3, 4)
    if w % cols or h % rows or w // cols != h // rows:
        sys.exit("sky: %s is %dx%d, which is not a %dx%d cross of square faces"
                 % (path, w, h, cols, rows))
    n = w // cols
    a = np.asarray(im).astype(np.float32) / 255.0
    out = {}
    for name, (c, r) in CROSS_H.items():
        out[name] = a[r * n:(r + 1) * n, c * n:(c + 1) * n]
    return out, n


def srgb_to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c):
    c = np.clip(c, 0.0, None)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1.0 / 2.4) - 0.055)


def face_dirs(face, n):
    """Unit directions for every texel of one cube face, in GL's own orientation.

    The signs are GL's, not a choice: a cube map is addressed as if the viewer stands
    inside the cube, which flips v against the usual image convention and is why every
    row here carries a minus somewhere. Getting one wrong gives a sky that is subtly
    mirrored and looks almost right, so they are written out rather than derived.
    """
    t = (np.arange(n, dtype=np.float32) + 0.5) / n * 2.0 - 1.0
    u, v = np.meshgrid(t, t)
    o = np.ones_like(u)
    if   face == "posX": d = np.stack([ o, -v, -u], -1)
    elif face == "negX": d = np.stack([-o, -v,  u], -1)
    elif face == "posY": d = np.stack([ u,  o,  v], -1)
    elif face == "negY": d = np.stack([ u, -o, -v], -1)
    elif face == "posZ": d = np.stack([ u, -v,  o], -1)
    else:                d = np.stack([-u, -v, -o], -1)
    return d / np.linalg.norm(d, axis=-1, keepdims=True)


def sample_cube(cube, n, dirs):
    """Nearest-texel lookup of a cube map by direction. dirs is (..., 3)."""
    d = dirs.reshape(-1, 3)
    ax = np.abs(d)
    major = np.argmax(ax, axis=1)
    m = ax[np.arange(len(d)), major]
    m = np.maximum(m, 1e-8)
    x, y, z = d[:, 0], d[:, 1], d[:, 2]
    sc = np.empty(len(d), np.float32)
    tc = np.empty(len(d), np.float32)
    idx = np.empty(len(d), np.int32)
    pos = d[np.arange(len(d)), major] > 0
    for axis, (pf, nf) in enumerate([("posX", "negX"), ("posY", "negY"), ("posZ", "negZ")]):
        for sign, name in ((True, pf), (False, nf)):
            sel = (major == axis) & (pos == sign)
            if not sel.any():
                continue
            idx[sel] = FACES.index(name)
            if   name == "posX": sc[sel], tc[sel] = -z[sel], -y[sel]
            elif name == "negX": sc[sel], tc[sel] =  z[sel], -y[sel]
            elif name == "posY": sc[sel], tc[sel] =  x[sel],  z[sel]
            elif name == "negY": sc[sel], tc[sel] =  x[sel], -z[sel]
            elif name == "posZ": sc[sel], tc[sel] =  x[sel], -y[sel]
            else:                sc[sel], tc[sel] = -x[sel], -y[sel]
    su = np.clip(((sc / m) * 0.5 + 0.5) * n, 0, n - 1e-3).astype(np.int32)
    sv = np.clip(((tc / m) * 0.5 + 0.5) * n, 0, n - 1e-3).astype(np.int32)
    flat = cube.reshape(6, n, n, 3)
    return flat[idx, sv, su].reshape(dirs.shape[:-1] + (3,))


def downsample(cube, n, to):
    """Reduce every face of a cube to `to` a side, in whatever space it is already in.

    A box reduction when the sizes divide, which is exact and is the mip case; a Lanczos
    resample otherwise, because a photographic sky does not arrive at a power of two and
    refusing it over that would be a silly place to stop. The resample runs per channel
    on floats, so it never round-trips through eight bits and never leaves linear.
    """
    if n == to:
        return cube
    if n % to == 0:
        k = n // to
        return cube.reshape(6, to, k, to, k, 3).mean(axis=(2, 4))
    out = np.empty((6, to, to, 3), np.float32)
    for f in range(6):
        for c in range(3):
            im = Image.fromarray(cube[f, :, :, c].astype(np.float32), mode="F")
            out[f, :, :, c] = np.asarray(im.resize((to, to), Image.LANCZOS), np.float32)
    return out


def hammersley(count):
    i = np.arange(count, dtype=np.float64)
    bits = i.astype(np.uint64)
    b = bits.copy()
    b = ((b << np.uint64(16)) | (b >> np.uint64(16))) & np.uint64(0xFFFFFFFF)
    b = (((b & np.uint64(0x55555555)) << np.uint64(1)) | ((b & np.uint64(0xAAAAAAAA)) >> np.uint64(1))) & np.uint64(0xFFFFFFFF)
    b = (((b & np.uint64(0x33333333)) << np.uint64(2)) | ((b & np.uint64(0xCCCCCCCC)) >> np.uint64(2))) & np.uint64(0xFFFFFFFF)
    b = (((b & np.uint64(0x0F0F0F0F)) << np.uint64(4)) | ((b & np.uint64(0xF0F0F0F0)) >> np.uint64(4))) & np.uint64(0xFFFFFFFF)
    b = (((b & np.uint64(0x00FF00FF)) << np.uint64(8)) | ((b & np.uint64(0xFF00FF00)) >> np.uint64(8))) & np.uint64(0xFFFFFFFF)
    return np.stack([i / count, b.astype(np.float64) * 2.3283064365386963e-10], -1)


def ggx_prefilter(src, srcn, out_n, roughness, samples):
    """One level of the roughness chain: the sky blurred by the cone of one roughness.

    Importance sampled against the GGX normal distribution with N = V = R, which is the
    approximation the split-sum method is built on: it throws away the stretch a
    reflection has at a grazing angle in exchange for a table that depends on roughness
    alone. Every real-time engine makes the same trade.

    Deterministic by construction. The sample set is a Hammersley sequence, not a random
    one, so this file is a pure function of its input and two bakes are byte identical.
    """
    a = max(roughness * roughness, 1e-4)
    xi = hammersley(samples)
    phi = 2.0 * np.pi * xi[:, 0]
    cos_t = np.sqrt((1.0 - xi[:, 1]) / (1.0 + (a * a - 1.0) * xi[:, 1]))
    sin_t = np.sqrt(np.maximum(0.0, 1.0 - cos_t * cos_t))
    hl = np.stack([sin_t * np.cos(phi), sin_t * np.sin(phi), cos_t], -1).astype(np.float32)

    out = np.zeros((6, out_n, out_n, 3), np.float32)
    for fi, face in enumerate(FACES):
        n = face_dirs(face, out_n).reshape(-1, 3)
        # An orthonormal basis about each normal, dodging the pole.
        up = np.where(np.abs(n[:, 2:3]) < 0.999,
                      np.array([0, 0, 1], np.float32), np.array([1, 0, 0], np.float32))
        tx = np.cross(up, n); tx /= np.maximum(np.linalg.norm(tx, axis=1, keepdims=True), 1e-8)
        ty = np.cross(n, tx)
        acc = np.zeros((len(n), 3), np.float32)
        wsum = np.zeros((len(n), 1), np.float32)
        for s in range(samples):
            h = (tx * hl[s, 0] + ty * hl[s, 1] + n * hl[s, 2])
            l = 2.0 * np.sum(n * h, axis=1, keepdims=True) * h - n
            ndl = np.sum(n * l, axis=1, keepdims=True)
            w = np.maximum(ndl, 0.0)
            acc += sample_cube(src, srcn, l) * w
            wsum += w
        out[fi] = (acc / np.maximum(wsum, 1e-6)).reshape(out_n, out_n, 3)
    return out


def irradiance(src, srcn, out_n):
    """The diffuse half: the cosine-weighted average of everything a normal can see.

    Done as a dense weighted sum against a small source rather than by sampling, because
    at this size the exact answer is cheaper than an approximation of it: a 16-a-side
    source is 1536 directions, and the whole convolution is one matrix multiply per face.
    """
    sdir = np.concatenate([face_dirs(f, srcn).reshape(-1, 3) for f in FACES])
    scol = src.reshape(-1, 3)
    # Solid angle of each source texel, so the poles of a face do not outvote its middle.
    t = (np.arange(srcn, dtype=np.float32) + 0.5) / srcn * 2.0 - 1.0
    u, v = np.meshgrid(t, t)
    sa = (4.0 / ((1.0 + u * u + v * v) ** 1.5) / (srcn * srcn)).reshape(-1)
    sa = np.tile(sa, 6).astype(np.float32)

    out = np.zeros((6, out_n, out_n, 3), np.float32)
    for fi, face in enumerate(FACES):
        nd = face_dirs(face, out_n).reshape(-1, 3)
        w = np.maximum(nd @ sdir.T, 0.0) * sa            # (out, src)
        acc = (w @ scol) / np.pi
        norm = w.sum(axis=1, keepdims=True) / np.pi
        out[fi] = (acc / np.maximum(norm, 1e-6)).reshape(out_n, out_n, 3)
    return out


def sh_irradiance(src, srcn):
    """Project the sky onto nine spherical-harmonic coefficients, pre-convolved.

    The diffuse half of the lighting equation again, but as nine numbers instead of a
    texture. That is not a saving for its own sake: an irradiance map has cube seams at
    the size it wants to be, costs a texture unit and a sampler, and stores something
    that is smooth by construction at a resolution that cannot represent anything else.
    Nine coefficients reconstruct the same function to within a percent and the shader
    that evaluates them is nine multiply-adds.

    The cosine-lobe convolution (the A-hat factors) is folded in HERE rather than in the
    shader, so what ships is irradiance and not radiance and the renderer cannot get the
    convolution wrong. Ramamoorthi and Hanrahan's own formulation.
    """
    dirs = np.concatenate([face_dirs(f, srcn).reshape(-1, 3) for f in FACES])
    col = src.reshape(-1, 3)
    t = (np.arange(srcn, dtype=np.float32) + 0.5) / srcn * 2.0 - 1.0
    u, v = np.meshgrid(t, t)
    # A cube texel's solid angle: it shrinks toward the corners of a face, and ignoring
    # that weights the corners of the cube six times too heavily.
    sa = np.tile((4.0 / ((1.0 + u * u + v * v) ** 1.5) / (srcn * srcn)).reshape(-1), 6)

    x, y, z = dirs[:, 0], dirs[:, 1], dirs[:, 2]
    Y = np.stack([
        np.full_like(x, 0.282095),
        0.488603 * y, 0.488603 * z, 0.488603 * x,
        1.092548 * x * y, 1.092548 * y * z,
        0.315392 * (3.0 * z * z - 1.0),
        1.092548 * x * z, 0.546274 * (x * x - y * y),
    ], axis=1)
    coef = (Y * sa[:, None]).T @ col                      # (9, 3)
    ahat = np.array([3.141593, 2.094395, 2.094395, 2.094395,
                     0.785398, 0.785398, 0.785398, 0.785398, 0.785398], np.float32)
    return (coef * ahat[:, None]).astype(np.float32)


def to_bytes(faces_linear):
    """Linear float faces back to 8-bit sRGB, which is what the loader uploads."""
    return (np.clip(linear_to_srgb(faces_linear) * 255.0 + 0.5, 0, 255)
            .astype(np.uint8).tobytes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", help="the cross-layout sky image")
    ap.add_argument("-o", "--out", default="game/sky.pack")
    ap.add_argument("--layout", default="cross", choices=["cross"])
    ap.add_argument("--face", type=int, default=256, help="base cube face, a side")
    ap.add_argument("--samples", type=int, default=128, help="GGX samples per texel")
    ap.add_argument("--preview", default="", help="write the levels out as PNGs here too")
    args = ap.parse_args()

    cross, n = load_cross(args.source, args.layout)
    print("sky: %s, %d faces of %dx%d" % (args.source, len(cross), n, n))

    # LINEAR FROM HERE TO THE LAST LINE. Averaging sRGB values is averaging the wrong
    # numbers: two texels at 0.5 do not make light worth 0.5, and a convolution that
    # ignores that comes out visibly too dark in the shadows and too flat in the sky.
    base = np.stack([srgb_to_linear(cross[f]) for f in FACES]).astype(np.float32)
    if n != args.face:
        base = downsample(base, n, args.face)
        print("sky: resampled to %d a side" % args.face)

    mips = 1
    while (args.face >> mips) >= 4:
        mips += 1
    print("sky: %d roughness levels, %d..%d a side" % (mips, args.face, args.face >> (mips - 1)))

    # The source every convolution reads. Small on purpose: a blurred result cannot carry
    # detail the blur is about to destroy, and the cost is quadratic in it.
    src64 = downsample(base, args.face, 64) if args.face >= 64 else base
    src16 = downsample(base, args.face, 16) if args.face >= 16 else base

    levels = [base]
    for m in range(1, mips):
        size = args.face >> m
        rough = m / float(mips - 1)
        lvl = ggx_prefilter(src64, 64, size, rough, args.samples)
        levels.append(lvl)
        print("sky:   level %d  %3dx%-3d roughness %.2f" % (m, size, size, rough))

    sh = sh_irradiance(src16, 16)
    print("sky: irradiance as 9 SH coefficients, ambient %.3f %.3f %.3f"
          % (sh[0][0], sh[0][1], sh[0][2]))

    blob = bytearray()
    blob += MAGIC
    blob += struct.pack("<8I", VERSION, args.face, mips, 9, 0, 0, 0, 0)
    for c in sh:
        blob += struct.pack("<4f", float(c[0]), float(c[1]), float(c[2]), 0.0)
    for lvl in levels:
        blob += to_bytes(lvl)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(bytes(blob))
    print("sky: wrote %s, %.2f MB" % (args.out, len(blob) / 1048576.0))

    if args.preview:
        os.makedirs(args.preview, exist_ok=True)
        for m, lvl in enumerate(levels):
            for fi, face in enumerate(FACES):
                Image.fromarray((np.clip(linear_to_srgb(lvl[fi]), 0, 1) * 255).astype(np.uint8)) \
                     .save(os.path.join(args.preview, "mip%d_%s.png" % (m, face)))
        print("sky: previews in %s" % args.preview)
    return 0


if __name__ == "__main__":
    sys.exit(main())
