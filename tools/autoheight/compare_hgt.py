#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- one editor-written .HGT against the generator, for gate G190.

    python3 compare_hgt.py SCEN FILE.HGT

Prints one line:

    AHCOMPARE|SCEN|mean=M|within2=P|max=X|water=W/WG|steep=S|lift=L

    mean, within2, max   the file against tools/autoheight/generate.py with the swells
                         OFF: mean absolute difference per playable corner (bytes),
                         the share of corners within 2 bytes, the largest difference
    water W/WG           open-water cells whose corners slope more than 8 bytes, in the
                         file and in the generator's own answer
    steep                walkable cells steeper than the cartridge's 99th percentile
    lift                 the highest the file stands above the swell-less answer: the
                         swells, when the file was written with them on
"""
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import corpus as C
import generate as G
import check as K


def main():
    scen, path = sys.argv[1], sys.argv[2]
    m = C.Map(scen)
    h = np.frombuffer(open(path, "rb").read(), np.uint8).reshape(65, 65).astype(int)
    G.SWELLS = False
    g = G.generate(m, G.train(C.training())).astype(int)
    pc = m.playable_corners()
    d = np.abs(h - g)[pc]
    lim, _ = K.cartridge_limits()
    a, b = K.measure(m, h, lim), K.measure(m, g, lim)
    print(f"AHCOMPARE|{scen}|mean={d.mean():.3f}|within2={100 * (d <= 2).mean():.1f}|max={d.max()}"
          f"|water={a['water']}/{b['water']}|steep={a['steep']}|lift={int((h - g)[pc].max())}")


if __name__ == "__main__":
    main()
