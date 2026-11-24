#!/usr/bin/env python3
"""Red Alert's remastered vehicles and buildings as materials under Enhanced: the scripts a
gate runs and the claims it checks.

    python3 gate_ramaterial.py scripts DIR   write DIR/base.txt and DIR/lamps.txt
    python3 gate_ramaterial.py check DIR     read the runs' pictures and target dumps and
                                             print "1 message" or "0 why"

WHAT IS UNDER TEST. The light pass lights a vehicle's or a building's pixel as a material
(its specular map's roughness and metal, the sky's reflection, the screen's reflections,
the sun's and the lamps' highlights) only where the console's baked shade has been drawn
into the bake target. Red Alert's vehicles and buildings are drawn by their remastered
models directly, never through a cartridge mesh, and the bake pass reached models only
through a cartridge mesh, so not one of their pixels was in it: the screen reflections and
a headlight's glint on metal, which Tiberian Dawn's show, changed nothing in Red Alert.

THE RUNS (all Red Alert, Enhanced, the 1280x720 test size, no shroud):
  base    SCU01EA, the Soviet base the mission starts on (a radar dome, two power plants,
          two airfields, a vehicle): one picture and its targets (renderdump);
  basex   the same with the screen reflections off (CNC3D_SSR_OFF=1): one picture;
  lamps   SCG01EA at its start, a Light Tank and a Medium Tank placed nose to nose with two
          of the Rangers, so each stands in another's headlights: the picture with the
          lamps on and off, under the material model and again without it
          (`gfx pbr_on 0`, the F5 "materials" switch), and the targets.

THE CLAIMS:
  (1) every vehicle and building pixel is in the bake target: of the pixels the normal
      target names a remastered vehicle (0.335..0.345) or building (0.365..0.395), at
      least 98 per cent carry the bake target's mark (alpha over 0.9), in both scenes;
  (2) THE REFLECTIONS REACH THEM: on the base's building and vehicle pixels, the picture
      with the reflections on differs from the one with them off on at least 1000 pixels
      (by more than 6 levels of the three channels together);
  (3) THE GLINT REACHES THEM: on the lamp scene's vehicle pixels, what the lamps add
      under the material model exceeds what they add without it by more than 30 levels on
      at least 150 pixels.
Before the fix: (1) none of them, (2) 0 pixels, (3) 0 pixels.
"""
import gzip
import os
import struct
import sys

VEH = (0.335, 0.345)
BLD = (0.365, 0.395)


def scripts(d):
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "base.txt"), "w").write(
        "tick 3\nzoom max\nshot %s/base.png\nrenderdump %s/base\nquit\n" % (d, d))
    open(os.path.join(d, "basex.txt"), "w").write(
        "tick 3\nzoom max\nshot %s/basex.png\nquit\n" % d)
    open(os.path.join(d, "lamps.txt"), "w").write("\n".join([
        "tick 1", "spawnobj 2TNK 64 51", "spawnobj 1TNK 62 51", "tick 3", "zoom max",
        "cam 63 51", "shot %s/l_on.png" % d, "renderdump %s/lamps" % d,
        "evhclamps 0", "shot %s/l_off.png" % d, "evhclamps 1",
        "gfx pbr_on 0", "shot %s/l_on_nomat.png" % d,
        "evhclamps 0", "shot %s/l_off_nomat.png" % d, "evhclamps 1", "gfx pbr_on 1",
        "quit"]) + "\n")


def load(p):
    import numpy as np
    b = gzip.open(p).read()
    w, h, k = struct.unpack("3i", b[:12])
    a = np.frombuffer(b[12:], dtype=np.float32)
    return a.reshape(h, w, 4 if k == 2 else 1)[::-1]


def check(d):
    import numpy as np
    from PIL import Image
    why = []

    def need(c, m):
        if not c:
            why.append(m)
        return c

    def img(n):
        p = os.path.join(d, n)
        return np.asarray(Image.open(p).convert("RGB")).astype(np.int64) if os.path.exists(p) else None

    msg = []
    masks = {}
    for scene in ("base", "lamps"):
        try:
            nm = load(os.path.join(d, scene + "-normals.bin.gz"))[..., 3]
            bk = load(os.path.join(d, scene + "-bake.bin.gz"))[..., 3]
        except (OSError, ValueError) as e:
            need(False, "%s: the targets were not dumped (%s)" % (scene, e))
            continue
        veh = (nm > VEH[0]) & (nm < VEH[1])
        bld = (nm > BLD[0]) & (nm < BLD[1])
        masks[scene] = (veh, bld)
        body = veh | bld
        share = float((bk[body] > 0.9).mean()) if body.any() else 0.0
        need(body.sum() > 2000, "%s: only %d vehicle and building pixels in view"
             % (scene, body.sum()))
        need(share >= 0.98, "%s: %.0f per cent of the %d vehicle and building pixels are in "
             "the bake target, want 98 or more" % (scene, share * 100, body.sum()))
        msg.append("%s %.0f%% of %d" % (scene, share * 100, body.sum()))

    a, b = img("base.png"), img("basex.png")
    refl = 0
    if need(a is not None and b is not None and "base" in masks, "the base pictures are missing"):
        veh, bld = masks["base"]
        ch = np.abs(a - b).sum(axis=2) > 6
        refl = int((ch & (veh | bld)).sum())
        need(refl >= 1000, "the reflections change %d building and vehicle pixels, want 1000 "
             "or more" % refl)

    on, off = img("l_on.png"), img("l_off.png")
    onn, offn = img("l_on_nomat.png"), img("l_off_nomat.png")
    glint = 0
    if need(all(x is not None for x in (on, off, onn, offn)) and "lamps" in masks,
            "the lamp pictures are missing"):
        veh, _ = masks["lamps"]
        extra = (on - off).max(axis=2) - (onn - offn).max(axis=2)
        glint = int(((extra > 30) & veh).sum())
        need(glint >= 150, "the lamps' highlight under the material model reaches %d vehicle "
             "pixels, want 150 or more" % glint)

    if why:
        return "0 " + "; ".join(why)
    return ("1 Red Alert's vehicles and buildings are lit as materials: bake target %s; the "
            "screen reflections change %d of their pixels; the headlights' glint lifts %d "
            "vehicle pixels by more than 30 levels" % (", ".join(msg), refl, glint))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "scripts":
        scripts(sys.argv[2])
    elif len(sys.argv) >= 3 and sys.argv[1] == "check":
        print(check(sys.argv[2]))
    else:
        sys.exit(__doc__)
