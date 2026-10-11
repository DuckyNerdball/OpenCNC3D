#!/usr/bin/env python3
"""Lay out data/radata from the two 1996 Red Alert discs.

    extract_ra_disc.py ALLIED_ROOT SOVIET_ROOT [OUTDIR]

ALLIED_ROOT and SOVIET_ROOT are the top folders of the two discs (a mounted ISO or a copy
of the CD). The discs used are the Allied and Soviet images of the 1996 release that
Electronic Arts made freeware in 2008:

    redalert_allied.iso   653725696 bytes  md5 13e213c788890e5172c766ed0059ea22
    redalert_soviets.iso  677222400 bytes  md5 b6eddcf8daefcd633ea804f9813e25b7

Each disc carries MAIN.MIX at its root and INSTALL/REDALERT.MIX. Both are encrypted
archives of further archives. Every inner archive is written out byte for byte, still
encrypted, because that is the form the engine's own MixFileClass reads. The two movie
archives (MOVIES1.MIX on the Allied disc, MOVIES2.MIX on the Soviet one) are too large to
commit whole, so they are split into their .VQA and .VQP files under movies/, the way
data/dosdata holds Tiberian Dawn's.

Names come from the Red Alert source: the inner archive names it opens, VQName[] in
const.cpp and the theme table in theme.cpp. An entry no known name matches is still
written, as _XXXXXXXX.MIX or _XXXXXXXX.BIN after its name hash, so nothing on the disc is
dropped silently. MANIFEST.txt lists every file with its size, md5 and where it came from.

An inner archive found on both discs must be identical on both, or the tool stops: the
layout has one copy of each, and two different copies would need a decision.
"""
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "menu", "tools"))
import ramix  # noqa: E402
from mixlib import mix_id  # noqa: E402

MOVIE_ARCHIVES = ("MOVIES1.MIX", "MOVIES2.MIX")


def _src(rel):
    with open(os.path.join(ROOT, "brain", "vanilla", "redalert", rel), "rb") as f:
        return f.read().decode("latin-1")


def known_names():
    names = set()
    for rel in os.listdir(os.path.join(ROOT, "brain", "vanilla", "redalert")):
        if rel.lower().endswith((".cpp", ".h")):
            names |= set(n.upper() for n in re.findall(r'"([A-Za-z0-9_]{1,8}\.[A-Za-z0-9]{3})"', _src(rel)))
    const = _src("const.cpp")
    vq = re.findall(r'"([A-Z0-9_]+)"', const[const.index("VQName[VQ_COUNT]"):].split("};")[0])
    names |= {v + ext for v in vq for ext in (".VQA", ".VQP")}
    themes = re.findall(r'\{\s*"([A-Z0-9_]+)"', _src("theme.cpp"))
    names |= {t + ext for t in themes for ext in (".AUD", ".VAR")}
    return {mix_id(n): n for n in names}


def entries(mix):
    for fid, (off, size) in sorted(mix.index.items(), key=lambda kv: kv[1][0]):
        yield fid, mix.data[off:off + size]


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    discs = (("allied", argv[0]), ("soviet", argv[1]))
    out = argv[2] if len(argv) > 2 else os.path.join(ROOT, "data", "radata")
    names = known_names()
    written = {}   # relative path -> (md5, size, source)

    def put(rel, blob, source):
        md5 = hashlib.md5(blob).hexdigest()
        if rel in written:
            if written[rel][0] != md5:
                raise SystemExit("%s differs between discs (%s and %s)"
                                 % (rel, written[rel][2], source))
            return
        path = os.path.join(out, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(blob)
        written[rel] = (md5, len(blob), source)

    for disc, top in discs:
        for outer in ("MAIN.MIX", os.path.join("INSTALL", "REDALERT.MIX")):
            mix = ramix.RaMix(os.path.join(top, outer))
            for fid, blob in entries(mix):
                name = names.get(fid)
                source = "%s:%s" % (disc, outer.replace(os.sep, "/"))
                if name in MOVIE_ARCHIVES:
                    inner = ramix.RaMix(data=blob, name=name)
                    for mid, movie in entries(inner):
                        mname = names.get(mid, "_%08X.BIN" % mid)
                        put(os.path.join("movies", mname), movie, source + ":" + name)
                    continue
                put(name or "_%08X.MIX" % fid, blob, source)

    with open(os.path.join(out, "MANIFEST.txt"), "w") as f:
        f.write("# data/radata, written by tools/ra3d/extract_ra_disc.py\n")
        f.write("# path  size  md5  source(disc:archive[:inner archive])\n")
        for rel in sorted(written):
            md5, size, source = written[rel]
            f.write("%s  %d  %s  %s\n" % (rel.replace(os.sep, "/"), size, md5, source))
    total = sum(v[1] for v in written.values())
    print("%d files, %.1f MB, into %s" % (len(written), total / 1048576.0, out))
    unnamed = sorted(r for r in written if os.path.basename(r).startswith("_"))
    if unnamed:
        print("unnamed entries kept by hash: " + ", ".join(unnamed))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
