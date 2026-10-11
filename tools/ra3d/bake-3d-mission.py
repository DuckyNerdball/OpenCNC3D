#!/usr/bin/env python3
"""
OpenCNC3D RA3D -- a Red Alert mission's pack for the 3D modes (N64 and Enhanced).

    python3 tools/ra3d/bake-3d-mission.py OUTDIR [SCEN ...]      default: SCG01EA

For each mission writes OUTDIR/<SCEN>.pack, the renderer's 3D scene for it:

  1. its terrain, Red Alert's own 128 x 128 cells on Red Alert's own snow bank
     (bake_ra_terrain3d.py terrain; the bank itself is `bake_ra_terrain3d.py bank`);
  2. its heights, the approved auto heightmap in data/ra3d/autoheight/<NAME>.HGT
     (tools/ra3d/ra_heights.py made it; a mission with none bakes flat and says so);
  3. bake5.py's build of the two, under a name of its own (SCG01EA becomes RAG01EA) so
     nothing keyed by scenario name can ever reach the Tiberian Dawn mission of the
     same name on the cartridge; its tint is passed in as the cartridge's flat one for
     the same reason (bake5 derives the tint's file name from the scenario's).
  4. OpenCNC3D's shared cursor rigs and flipbooks, so the same remastered 3D cursor art
     attaches with its animation and the shared cursor size settings.

The pack is written straight to OUTDIR, not to the bakery's own output folder, because
every Tiberian Dawn build copies that folder's packs into the play folder's root.
"""
import importlib.util, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))


def ra_name(scen):
    return "RA" + scen[2:]


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__); sys.exit(2)
    out = os.path.abspath(args[0])
    scens = args[1:] or ["SCG01EA"]
    os.makedirs(out, exist_ok=True)
    if len(scens) > 1:
        # One process per mission, a few at a time: each bake is a minute of one core, and
        # every one writes only its own terrain JSON and its own pack.
        from concurrent.futures import ThreadPoolExecutor
        def one(scen):
            r = subprocess.run([sys.executable, os.path.abspath(__file__), out, scen],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            return scen, r.returncode, r.stdout
        failed = []
        with ThreadPoolExecutor(max(1, min(6, (os.cpu_count() or 2) // 2))) as pool:
            for scen, rc, text in pool.map(one, scens):
                print(text.strip().splitlines()[-1] if text.strip() else scen)
                if rc:
                    print(text); failed.append(scen)
        if failed:
            sys.exit("bake-3d-mission: failed: " + " ".join(failed))
        return
    bakery = os.path.join(ROOT, "tools", "bakery")
    sys.path.insert(0, bakery)
    spec = importlib.util.spec_from_file_location("bake5", os.path.join(bakery, "bake5.py"))
    bake5 = importlib.util.module_from_spec(spec)
    sys.argv = [spec.origin]
    spec.loader.exec_module(bake5)
    for scen in scens:
        name = ra_name(scen)
        subprocess.run([sys.executable, os.path.join(HERE, "bake_ra_terrain3d.py"), "terrain", scen, name],
                       check=True)
        hgt = os.path.join(ROOT, "data", "ra3d", "autoheight", name + ".HGT")
        heights = None
        if os.path.exists(hgt):
            heights = list(open(hgt, "rb").read())
            print("%s: heights from %s" % (scen, os.path.relpath(hgt, ROOT)))
        else:
            heights = [0] * (129 * 129)
            print("%s: NO auto heightmap in data/ra3d/autoheight; baked flat" % scen)
        # THE TINT IS THE CARTRIDGE'S FLAT ONE, passed in. bake5 names a scenario's tint map
        # by overwriting the first two letters of its name with CM, so RAG01EA would read
        # GDI 1's CMG01EA.IMG off the cartridge; a name the cartridge has no tint for
        # answers CMFLAT, sized to this grid.
        _src, cm = bake5.bake_cm("ZZZ00ZZ", 129, 129)
        dst = os.path.join(out, scen + ".pack")
        bake5.build(name, dst, heights=heights, cmvals=cm, lean=True, cursors=True)
        print("%s -> %s (%.1f MB)" % (scen, dst, os.path.getsize(dst) / 1e6))


if __name__ == "__main__":
    main()
