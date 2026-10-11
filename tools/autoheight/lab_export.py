#!/usr/bin/env python3
"""
OpenCNC3D auto heightmap -- the remaster lab's Heightmaps category.

    python3 lab_export.py [--lab CHECKOUT] [--play FOLDER] [SCEN ...]

For every generated heightmap in data/autoheight (or the maps named), writes
<CHECKOUT>/build-native/remaster/heights/<SCEN>/, which the lab serves at /heights/:

    tiles.png        the whole 64x64 map in its own tile art, 16 px a cell (also the
                     3D view's texture)
    relief.png       the generated heights as a lit relief, playable rectangle
    over.png         the tile art lit by the generated heights, playable rectangle
    heights.json     the 65x65 corners and the playable rectangle, for the 3D view
    shot_<k>_flat.png, shot_<k>_gen.png
                     the game itself at two spots, as the cartridge left the map and
                     with the generated heights (only with --play: a play folder that
                     holds <SCEN>.pack baked with the heights and <SCEN>_flat.pack
                     without them; the runs are headless, in the background)
    meta.json        names, rung counts and the playability check's numbers

--lab defaults to this checkout. The lab at :8796 serves the shared checkout's
build-native, so pass that checkout to show the maps there.
"""
import json, os, subprocess, sys
import numpy as np
from PIL import Image
from scipy import ndimage as nd

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import corpus as C
import render as R
import check as K
import generate as G

DATA = os.path.join(C.ROOT, "data", "autoheight")


def spots(m, h, n=2):
    """The n places with the most relief, well inside the playable rectangle."""
    c = (h[:-1, :-1] + h[1:, 1:] + h[:-1, 1:] + h[1:, :-1]) / 4.0
    v = nd.generic_filter(c, np.std, size=(9, 13))
    ok = m.playable().copy()
    ok[:m.Y + 5] = False; ok[m.Y + m.H - 5:] = False
    ok[:, :m.X + 7] = False; ok[:, m.X + m.W - 7:] = False
    v[~ok] = -1
    out = []
    for _ in range(n):
        y, x = np.unravel_index(np.argmax(v), v.shape)
        out.append((int(x), int(y)))
        v[max(0, y - 12):y + 12, max(0, x - 16):x + 16] = -1
    return out


def shoot(play, scen, pack, x, y, out):
    """One headless frame of the game: hidden window, background app, no focus."""
    script = os.path.join(play, "ahshots", f"lab_{scen}.txt")
    os.makedirs(os.path.dirname(script), exist_ok=True)
    tmp = os.path.join("ahshots", f"lab_{scen}.png")
    open(script, "w").write(f"tick 40\ncammode n64\ndist 3800\ncam {x} {y}\nshot {tmp}\nquit\n")
    env = dict(os.environ, OPENCNC3D_BACKGROUND="1")
    subprocess.run(["./cnc_eyes", "--noshroud", "--scen", scen, "--pack", pack,
                    "--cameos", "cameos.pack", "--dospack", "dossidebar.pack",
                    "--dosinf", "dosinfantry.pack", "--dylib", "TiberianDawn.dylib",
                    "--dir", "missions/", "--content", "content/",
                    "--script", os.path.relpath(script, play)],
                   cwd=play, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    im = Image.open(os.path.join(play, tmp)).convert("RGB")
    # the battlefield only: the sidebar is the same in both and would halve the picture
    im.crop((0, 0, min(im.width, 1040), im.height)).save(out)


def main():
    args = sys.argv[1:]
    lab, play = C.ROOT, None
    if "--lab" in args:
        k = args.index("--lab"); lab = os.path.abspath(args[k + 1]); del args[k:k + 2]
    if "--play" in args:
        k = args.index("--play"); play = os.path.abspath(args[k + 1]); del args[k:k + 2]
    ids = args or sorted(f[:-4] for f in os.listdir(DATA) if f.endswith(".HGT"))
    lim, _ = K.cartridge_limits()
    for s in ids:
        m = C.Map(s)
        h = np.frombuffer(open(os.path.join(DATA, s + ".HGT"), "rb").read(), np.uint8) \
            .reshape(65, 65).astype(np.int16)
        out = os.path.join(lab, "build-native", "remaster", "heights", s)
        os.makedirs(out, exist_ok=True)
        R.tiles_image(m, 16).save(os.path.join(out, "tiles.png"))
        R.crop_playable(R.relief_image(h, 8), m, 8).save(os.path.join(out, "relief.png"))
        R.crop_playable(R.overlay_image(m, h, 8), m, 8).save(os.path.join(out, "over.png"))
        json.dump({"corners": h.ravel().tolist(), "rect": [m.X, m.Y, m.W, m.H]},
                  open(os.path.join(out, "heights.json"), "w"))
        pc = m.playable_corners()
        vals, cnt = np.unique(np.round(h[pc] / 64.0).astype(int), return_counts=True)
        sp = spots(m, h.astype(np.float64))
        shots = []
        if play:
            for k, (x, y) in enumerate(sp, 1):
                for v, pk in (("flat", f"{s}_flat.pack"), ("gen", f"{s}.pack")):
                    shoot(play, s, pk, x, y, os.path.join(out, f"shot_{k}_{v}.png"))
                shots.append(k)
        r = K.measure(m, h, lim)
        group = ("Multiplayer" if s in C.MULTIPLAYER else "Covert Ops" if s in C.COVERT_OPS
                 else "Bonus missions" if s in C.BONUS else "Test scenarios" if s in C.TEST
                 else "Dinosaur" if s.startswith("SCJ") else "Other")
        key, rect = G._tiles_key(s), (m.X, m.Y, m.W, m.H)
        twins = [t for t in C.all_ids() if t != s and G._tiles_key(t) == key
                 and (lambda tm: (tm.X, tm.Y, tm.W, tm.H))(C.Map(t)) == rect]
        real_twin = next((t for t in twins if C.Map(t).h is not None), None)
        meta = dict(scen=s, twins=twins, real_twin=real_twin, name=m.name.title() if m.name.isupper() else m.name, group=group,
                    theater=m.theater, rect=[m.X, m.Y, m.W, m.H], starts=m.starts,
                    rungs={int(a): int(b) for a, b in zip(vals, cnt)},
                    corners=int(pc.sum()), spots=sp, shots=shots, check=r, steep_limit=lim)
        json.dump(meta, open(os.path.join(out, "meta.json"), "w"), indent=1)
        print(s, "->", out, f"({len(shots)} shot pairs)")


if __name__ == "__main__":
    main()
