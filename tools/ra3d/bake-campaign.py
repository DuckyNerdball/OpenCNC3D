#!/usr/bin/env python3
"""Pack Red Alert's original Allied/Soviet score and campaign map reels.

The LCW-compressed WSA frame deltas and their palettes remain exactly as they
appear in the disc archives. app/campaign.c decodes the shared CNC3DCPN v2
container at runtime; no Tiberian Dawn campaign artwork enters this pack.
"""

from __future__ import annotations

import argparse
import io
from pathlib import Path
import struct
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "game"))
from bake_campaign import wsa_parse, wsa_prove  # noqa: E402
from ramix import RaMix  # noqa: E402
sys.path.insert(0, str(ROOT / "menu" / "tools"))
from mixshp import Shape  # noqa: E402


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    general = RaMix(args.data / "GENERAL.MIX")
    conquer = RaMix(args.data / "CONQUER.MIX")
    entries = []
    for name in ("ALI-TRAN.WSA", "SOV-TRAN.WSA"):
        data = conquer.read(name)
        if data is None:
            raise RuntimeError(f"CONQUER.MIX is missing {name}")
        nf, w, h, x, y, pal, chunks = wsa_parse(data, name)
        wsa_prove(w, h, chunks)
        entries.append((name[:-4], nf, w, h, x, y, pal, chunks))
    for side in "AS":
        for scenario in range(1, 14):
            name = f"MS{side}{chr(ord('A') + scenario)}.WSA"
            data = general.read(name)
            if data is None:
                raise RuntimeError(f"GENERAL.MIX is missing {name}")
            nf, w, h, x, y, pal, chunks = wsa_parse(data, name)
            if (w, h) != (320, 200):
                raise RuntimeError(f"{name} is {w}x{h}, expected 320x200")
            wsa_prove(w, h, chunks)
            entries.append((name[:-4], nf, w, h, x, y, pal, chunks))
    # The Win95 score screen replaces the low resolution transition frame with
    # one of these original 640x400 PCX plates (score.cpp Presentation).
    for name in ("ALIBACKH.PCX", "SOVBACKH.PCX"):
        data = conquer.read(name)
        if data is None:
            raise RuntimeError(f"CONQUER.MIX is missing {name}")
        image = Image.open(io.BytesIO(data))
        if image.mode != "P" or image.size != (640, 400):
            raise RuntimeError(f"{name} is not an indexed 640x400 PCX")
        pal8 = image.getpalette()
        if pal8 is None or len(pal8) < 768:
            raise RuntimeError(f"{name} has no usable palette")
        pal6 = bytes(channel >> 2 for channel in pal8[:768])
        entries.append((name[:-4], 1, 640, 400, 0, 0, pal6,
                        [image.tobytes()]))
    # ScoreTimeClass and the two hall-of-fame ornaments use the high resolution
    # shapes in the Win95 path. Keep their transparent index intact.
    for name, tag in (("TIMEHR.SHP", "TIMEHR"),
                      ("HISC1-HR.SHP", "HISC1HR"),
                      ("HISC2-HR.SHP", "HISC2HR"),
                      ("BAR3BHR.SHP", "BAR3BHR"),
                      ("BAR3RHR.SHP", "BAR3RHR"),
                      ("CREDSAHR.SHP", "CREDSAHR"),
                      ("CREDSUHR.SHP", "CREDSUHR")):
        data = conquer.read(name)
        if data is None:
            raise RuntimeError(f"CONQUER.MIX is missing {name}")
        shape = Shape(data)
        frames = [bytes(shape.frame(i)) for i in range(shape.frames)]
        entries.append((tag, shape.frames, shape.width, shape.height,
                        0, 0, bytes(768), frames))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("wb") as out:
        out.write(b"CNC3DCPN")
        out.write(struct.pack("<II", 2, len(entries)))
        for tag, nf, w, h, x, y, pal, chunks in entries:
            name = tag.encode("ascii")
            out.write(name + bytes(16 - len(name)))
            kind = 0 if tag.startswith(("ALI-TRAN", "SOV-TRAN", "MS")) else (
                1 if tag.endswith("BACKH") else 2)
            out.write(struct.pack("<6I", kind, nf, w, h, x, y))
            out.write(pal)
            if kind == 0:
                out.write(struct.pack(f"<{nf}I", *(len(chunk) for chunk in chunks)))
            for chunk in chunks:
                out.write(chunk)
    print(f"Red Alert campaign pack: 28 WSA reels, two Win95 score plates, "
          f"seven score shapes, {args.out}")


if __name__ == "__main__":
    main()
