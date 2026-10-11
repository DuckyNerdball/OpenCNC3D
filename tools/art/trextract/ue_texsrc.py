#!/usr/bin/env python3
"""Decode an uncooked UE5 Texture2D's source art into a PNG.

    python3 ue_texsrc.py <liboodle> <texture.uasset> <out.png> [<texture.uasset> <out.png> ...]

An editor texture keeps its source image as an FCompressedBuffer: a 64 byte big-endian
header (magic 0xb7756362, CRC, method, compressor, level, block size exponent, block count,
raw size, compressed size, raw hash), a big-endian table of compressed block sizes, then
the blocks. A block whose compressed size equals its raw size is stored raw; every other
block is Oodle-compressed. For a texture whose source CompressionFormat is TSCF_PNG the
decompressed payload is a PNG file.

<liboodle> is an Oodle shared library exporting OodleLZ_Decompress. Oodle is not open
source and is not in this repository. An Unreal Engine source installation carries its
source under Engine/Source/Runtime/OodleDataCompression/Sdks/<version>/src/; compiled with
-DOODLE_BUILDING_LIB (the core and base .cpp files, the one AVX2 file with -mavx2 -mbmi2)
and linked as a dynamic library, it is what this was run with.

THE CHANNELS ARE SWAPPED BACK. A TSF_BGRA8 source is written into the PNG byte for byte,
so the PNG's red holds blue and its blue holds red; read as it is, a normal map comes out
orange instead of blue. Only the first mip of the first slice is written; these are all
single-mip sources.
"""
import ctypes
import io
import struct
import sys

from PIL import Image


def main(argv):
    lib = ctypes.CDLL(argv[0])
    dec = lib.OodleLZ_Decompress
    dec.restype = ctypes.c_ssize_t
    dec.argtypes = [ctypes.c_void_p, ctypes.c_ssize_t, ctypes.c_void_p, ctypes.c_ssize_t,
                    ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_ssize_t,
                    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ssize_t,
                    ctypes.c_int]
    for src, out in zip(argv[1::2], argv[2::2]):
        b = open(src, "rb").read()
        i = b.find(struct.pack(">I", 0xB7756362))
        if i < 0:
            sys.exit("%s: no compressed buffer" % src)
        _, _, meth, _, _, bexp, bcount, raw, _ = struct.unpack(">IIBBBBIQQ", b[i:i + 32])
        sizes = struct.unpack(">%dI" % bcount, b[i + 64:i + 64 + 4 * bcount])
        o = i + 64 + 4 * bcount
        bs = 1 << bexp
        parts = []
        for k, cs in enumerate(sizes):
            rs = min(bs, raw - k * bs)
            blk = b[o:o + cs]
            o += cs
            if meth == 0 or cs == rs:
                parts.append(blk)
                continue
            dst = ctypes.create_string_buffer(rs)
            # fuzz safe, no CRC check, quiet, no dictionary, no callback, own memory,
            # both thread phases
            n = dec(blk, cs, dst, rs, 1, 0, 0, None, 0, None, None, None, 0, 3)
            if n != rs:
                sys.exit("%s: block %d decoded %d of %d bytes" % (src, k, n, rs))
            parts.append(dst.raw)
        im = Image.open(io.BytesIO(b"".join(parts)))
        im.load()
        if im.mode == "RGBA":
            r, g, bl, a = im.split()
            im = Image.merge("RGBA", (bl, g, r, a))
        im.save(out)
        print("%s -> %s %dx%d %s (%d blocks)" % (src.rsplit("/", 1)[-1], out, im.size[0],
                                                  im.size[1], im.mode, bcount))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
