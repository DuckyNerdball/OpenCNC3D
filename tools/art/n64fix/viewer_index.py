#!/usr/bin/env python3
"""Write the viewer's list: every structure and civilian building on the cartridge, its
1995 display name, and whether its cartridge model and its fixed model exist yet.

    python3 tools/art/n64fix/viewer_index.py [pack]

Names come from the model gallery's catalogue (the GPL source's own strings). The page
re-reads this file every couple of seconds, so a model appears in the list as soon as
it is written. Run by survey_all.sh and run.sh after each model.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "model-gallery"))
sys.path.insert(0, os.path.join(ROOT, "tools", "bakery"))
import catalog  # noqa: E402
import packinspect as PI  # noqa: E402

GROUPS = (("Structures", "Structures"), ("Civilian", "Civilian buildings"))


def main():
    pack = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "tools", "bakery", "game", "SCG01EA.pack")
    txt, gpl, km = catalog.build()
    p = PI.read(pack)
    models = os.path.join(HERE, "viewer", "models")
    os.makedirs(models, exist_ok=True)
    seen, rows = set(), []
    for code, mi, _ in p["types"]:
        if code in seen:
            continue
        seen.add(code)
        name, cat = catalog.resolve(code, txt, gpl, km)[:2]
        group = dict(GROUPS).get(cat)
        if not group:
            continue
        d = os.path.join(models, code)
        row = dict(type=code, name=name, group=group,
                   cartridge_tris=p["meshes"][mi]["ntris"],
                   anim_frames=(p["meshes"][mi]["anim"] or {}).get("frames", 0),
                   old=os.path.exists(os.path.join(d, "old.json")),
                   new=os.path.exists(os.path.join(d, "new.json")),
                   # a WORKING file in the model's folder marks the one being fixed now
                   working=os.path.exists(os.path.join(d, "WORKING")))
        for side in ("old", "new"):
            j = os.path.join(d, side + ".json")
            if os.path.exists(j):
                row[side + "_tris"] = json.load(open(j))["triangles"]
        rows.append(row)
    order = {g: i for i, (_, g) in enumerate(GROUPS)}
    rows.sort(key=lambda r: (order[r["group"]], r["name"].lower()))
    tmp = os.path.join(models, "index.json.%d.tmp" % os.getpid())
    with open(tmp, "w") as f:
        json.dump(rows, f, indent=0)
    os.replace(tmp, os.path.join(models, "index.json"))
    print("viewer index: %d models, %d surveyed, %d fixed"
          % (len(rows), sum(r["old"] for r in rows), sum(r["new"] for r in rows)))


if __name__ == "__main__":
    main()
