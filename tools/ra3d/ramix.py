#!/usr/bin/env python3
"""Read Red Alert MIX archives, including the encrypted ones on the 1996 discs.

    ramix.py list    ARCHIVE.MIX [--names FILE]
    ramix.py extract ARCHIVE.MIX OUTDIR NAME [NAME ...]

A Red Alert MIX starts with two little-endian u16s. If the first is zero the archive is
the extended format and the second is a flag word: 0x0001 means a SHA-1 digest follows
the data, 0x0002 means the header is encrypted. Otherwise the file is the Tiberian Dawn
layout (u16 count, u32 size, then the index).

An encrypted header is an 80-byte RSA block holding a 56-byte Blowfish key, followed by
the Blowfish-encrypted count, size and index, padded to whole 8-byte blocks. The file
bodies are never encrypted. Both the public key and the Blowfish tables are read from the
Red Alert source in brain/vanilla (redalert/const.cpp and common/blowfish.cpp), so this
tool carries no key material of its own and agrees with the engine by construction.

Entries are found by the engine's name hash, so listing an archive prints hashes; pass
--names with a file of candidate names to see which are present.
"""
import base64
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
VANILLA = os.path.join(ROOT, "brain", "vanilla")

sys.path.insert(0, os.path.join(ROOT, "menu", "tools"))
from mixlib import mix_id  # noqa: E402


def _source(rel):
    with open(os.path.join(VANILLA, rel), "rb") as f:
        return f.read().decode("latin-1")


def _public_key():
    """The modulus from const.cpp's [PublicKey]; the exponent is always 65537."""
    src = _source("redalert/const.cpp")
    m = re.search(r'char const Keys\[\] = "\[PublicKey\]\\n"\s*"1=([A-Za-z0-9+/=]+)\\n"', src)
    if not m:
        raise SystemExit("redalert/const.cpp: [PublicKey] not found")
    der = base64.b64decode(m.group(1))
    if der[0] != 0x02:
        raise SystemExit("public key is not a DER integer")
    n = der[1]
    if n & 0x80:
        k = n & 0x7F
        n = int.from_bytes(der[2:2 + k], "big")
        body = der[2 + k:2 + k + n]
    else:
        body = der[2:2 + n]
    return int.from_bytes(body, "big"), 65537


def _blowfish_tables():
    src = _source("common/blowfish.cpp")
    p = src[src.index("P_Init[BlowfishEngine::ROUNDS + 2]"):]
    p = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{8})U?", p.split("};")[0])]
    s = src[src.index("S_Init[4][UCHAR_MAX + 1]"):]
    s = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{8})U?", s.split("};\n")[0])]
    if len(p) != 18 or len(s) != 1024:
        raise SystemExit("blowfish tables: %d P and %d S entries" % (len(p), len(s)))
    return p, [s[i * 256:(i + 1) * 256] for i in range(4)]


class Blowfish(object):
    """Standard Blowfish, big-endian words, as common/blowfish.cpp implements it."""

    def __init__(self, key):
        p0, s0 = _blowfish_tables()
        self.S = [list(b) for b in s0]
        self.P = list(p0)
        j = 0
        for i in range(18):
            d = 0
            for _ in range(4):
                d = (d << 8) | key[j % len(key)]
                j += 1
            self.P[i] ^= d
        left = right = 0
        for i in range(0, 18, 2):
            left, right = self._enc(left, right)
            self.P[i], self.P[i + 1] = left, right
        for b in range(4):
            for i in range(0, 256, 2):
                left, right = self._enc(left, right)
                self.S[b][i], self.S[b][i + 1] = left, right

    def _f(self, x):
        S = self.S
        return ((((S[0][x >> 24] + S[1][(x >> 16) & 255]) & 0xFFFFFFFF)
                 ^ S[2][(x >> 8) & 255]) + S[3][x & 255]) & 0xFFFFFFFF

    def _enc(self, l, r):
        for i in range(16):
            l ^= self.P[i]
            r ^= self._f(l)
            l, r = r, l
        l, r = r, l
        r ^= self.P[16]
        l ^= self.P[17]
        return l, r

    def _dec(self, l, r):
        for i in range(17, 1, -1):
            l ^= self.P[i]
            r ^= self._f(l)
            l, r = r, l
        l, r = r, l
        r ^= self.P[1]
        l ^= self.P[0]
        return l, r

    def decrypt(self, data):
        out = bytearray()
        for i in range(0, len(data) - len(data) % 8, 8):
            l, r = struct.unpack_from(">II", data, i)
            out += struct.pack(">II", *self._dec(l, r))
        return bytes(out)


def _rsa_key(block):
    """The 56-byte Blowfish key out of the 80-byte RSA block (PKStraw, PKey::Decrypt)."""
    n, e = _public_key()
    precision = n.bit_length() - 1
    plain = (precision - 1) // 8
    crypt = plain + 1
    out = b""
    for i in range(0, len(block) - len(block) % crypt, crypt):
        c = int.from_bytes(block[i:i + crypt], "little")
        out += pow(c, e, n).to_bytes(64, "little")[:plain]
    return out[:56], crypt * ((56 - 1) // plain + 1)


class RaMix(object):
    def __init__(self, path=None, data=None, name=None):
        self.name = name or os.path.basename(path)
        self.data = data if data is not None else open(path, "rb").read()
        d = self.data
        first, second = struct.unpack_from("<hH", d, 0)
        self.encrypted = self.digest = False
        if first == 0:
            self.digest = bool(second & 1)
            self.encrypted = bool(second & 2)
            p = 4
            if self.encrypted:
                key, keylen = _rsa_key(d[p:p + 80])
                p += keylen
                bf = Blowfish(key)
                head = bf.decrypt(d[p:p + 8])
                count, size = struct.unpack_from("<HI", head, 0)
                hlen = 6 + 12 * count
                padded = (hlen + 7) // 8 * 8
                head = bf.decrypt(d[p:p + padded])
                p += padded
                index = head[6:hlen]
            else:
                count, size = struct.unpack_from("<HI", d, p)
                index = d[p + 6:p + 6 + 12 * count]
                p += 6 + 12 * count
        else:
            count, size = struct.unpack_from("<HI", d, 0)
            index = d[6:6 + 12 * count]
            p = 6 + 12 * count
        self.count, self.size, self.start = count, size, p
        expect = p + size + (20 if self.digest else 0)
        if expect != len(d):
            raise SystemExit("%s: header says %d bytes, file is %d (count %d)"
                             % (self.name, expect, len(d), count))
        self.index = {}
        for i in range(count):
            fid, off, sz = struct.unpack_from("<III", index, 12 * i)
            if off + sz > size:
                raise SystemExit("%s: entry %08X runs past the data" % (self.name, fid))
            self.index[fid] = (p + off, sz)

    def has(self, name):
        return mix_id(name) in self.index

    def read(self, name):
        e = self.index.get(mix_id(name))
        return None if e is None else self.data[e[0]:e[0] + e[1]]


def _names(path):
    if not path:
        return []
    with open(path) as f:
        return [ln.strip() for ln in f if ln.strip() and not ln.startswith("#")]


def main(argv):
    if len(argv) >= 2 and argv[0] == "list":
        m = RaMix(argv[1])
        names = _names(argv[3]) if len(argv) > 3 and argv[2] == "--names" else []
        known = {mix_id(n): n for n in names}
        print("%s: %d entries, %d bytes of data, encrypted=%s digest=%s"
              % (m.name, m.count, m.size, m.encrypted, m.digest))
        for fid, (off, sz) in sorted(m.index.items(), key=lambda kv: kv[1][0]):
            print("  %08X %10d  %s" % (fid, sz, known.get(fid, "")))
        return 0
    if len(argv) >= 4 and argv[0] == "extract":
        m = RaMix(argv[1])
        os.makedirs(argv[2], exist_ok=True)
        missing = 0
        for n in argv[3:]:
            b = m.read(n)
            if b is None:
                print("not in %s: %s" % (m.name, n), file=sys.stderr)
                missing += 1
                continue
            with open(os.path.join(argv[2], n.upper()), "wb") as f:
                f.write(b)
        return 1 if missing else 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
