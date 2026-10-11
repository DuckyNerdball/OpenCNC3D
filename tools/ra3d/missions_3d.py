#!/usr/bin/env python3
"""
OpenCNC3D RA3D -- every Red Alert map that gets a 3D scene.

    python3 tools/ra3d/missions_3d.py          one scenario name per line

The 42 campaign missions (Allied SCG.., Soviet SCU.., every A/B/C variant the map
screen can choose) and the 24 multiplayer maps (SCM01EA..SCM24EA) of the 1996 disc.

GENERAL.MIX also answers to SCG01EM, SCM01EQ, SCJ01EG and the like, but those are not
missions: Red Alert's MIX file id is a rotate-and-add over four-byte words, so a name
whose letters differ by amounts that cancel after the rotate gets the same id as a real
file (SCG01EM.INI and SCM01EA.INI share one). Every such name reads back an existing
mission's file byte for byte.
"""
ALLIED = ("SCG01EA SCG02EA SCG03EA SCG03EB SCG04EA SCG05EA SCG05EB SCG05EC SCG06EA SCG06EB "
          "SCG07EA SCG08EA SCG08EB SCG09EA SCG09EB SCG10EA SCG10EB SCG11EA SCG11EB SCG12EA "
          "SCG13EA SCG14EA").split()
SOVIET = ("SCU01EA SCU02EA SCU02EB SCU03EA SCU04EA SCU04EB SCU05EA SCU06EA SCU06EB SCU07EA "
          "SCU08EA SCU08EB SCU09EA SCU10EA SCU11EA SCU11EB SCU12EA SCU13EA SCU13EB SCU14EA").split()
MULTI = ["SCM%02dEA" % n for n in range(1, 25)]
ALL = ALLIED + SOVIET + MULTI

if __name__ == "__main__":
    print("\n".join(ALL))
