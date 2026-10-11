#!/usr/bin/env python3
"""Give one trextract mesh dump another's weapon: drop the base mesh's weapon material,
graft the donor's weapon on, and write a new dump that ue_c3m.py converts as usual.

    python3 tools/art/ue_weapon_swap.py \\
        --base  art/models/tempest_rising_gdi_rifleman/mesh/SK_GDI_Rifleman \\
        --drop  M_GDI_Weapon \\
        --donor art/models/tempest_rising_gdi_rocketeer/mesh/SK_CH_DYN_RocketSoldier_01 \\
        --donor-tex art/models/tempest_rising_gdi_rocketeer/donor_textures/T_DYN_RocketSoldier \\
        --bone Weapon \\
        --out art/models/tempest_rising_gdi_rocketeer/mesh/SK_GDI_Rocketeer \\
        --texout art/models/tempest_rising_gdi_rocketeer/textures

WHICH PART IS THE WEAPON. Every donor vertex carried entirely by --bone (or a bone under
it), and every donor triangle whose three corners are such vertices. A triangle with one
corner on the weapon and one on the body would be a seam this cannot cut, and it is
refused with a count rather than torn.

WHERE IT GOES. Both meshes are on Tempest Rising's shared infantry skeleton, so both have
the bone, and an animation poses it the same way on either. A weapon vertex is carried
from the donor's bind pose into that bone's own space (the inverse of the bone's
model-space bind) and out again through the base mesh's bind of the same bone. Played
with the donor's own clips, the weapon therefore sits in the hand exactly where it sat on
the donor. Its skin becomes that one bone, fully weighted, which is what it was.

ITS TEXTURE. The donor's colour, normal and surface maps are one atlas for the whole
character, of which the weapon uses a corner. That corner is cut out (the UV box of the
weapon's vertices, plus a margin for mip filtering, rounded out to whole pixels) and the
weapon's UVs are remapped into it, so the model does not carry a 2048 atlas that is
mostly somebody else's body. Written as <texout>/<stem>_D/_N/_RMOE.png (and _C, the team mask, when the donor has
one), and the grafted
material's "tex" names that stem for the converter.
"""
import argparse
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_c3m import model_space, trs  # noqa: E402

REC = np.dtype([("b", "<u2", 8), ("w", "<f4", 8)])
MARGIN = 8            # texels kept around the weapon's UV box, for the mip chain


def fail(msg):
    print("SWAP|error|%s" % msg)
    sys.exit(1)


def load(prefix):
    meta = json.load(open(prefix + ".json"))
    nv, ni = meta["nverts"], meta["nidx"]
    raw = open(prefix + ".bin", "rb").read()
    o = 0
    pos = np.frombuffer(raw, np.float32, nv * 3, o).reshape(nv, 3).copy(); o += nv * 12
    nrm = np.frombuffer(raw, np.float32, nv * 3, o).reshape(nv, 3).copy(); o += nv * 12
    uv = np.frombuffer(raw, np.float32, nv * 2, o).reshape(nv, 2).copy(); o += nv * 8
    inf = np.frombuffer(raw, REC, nv, o).copy(); o += nv * 48
    idx = np.frombuffer(raw, np.uint32, ni, o).astype(np.int64); o += ni * 4
    if o != len(raw):
        fail("%s.bin does not match its header" % prefix)
    return meta, pos, nrm, uv, inf, idx


def bind(meta, name):
    bones = meta["bones"]
    W = model_space(bones, [trs(b["r"], b["t"], b["s"]) for b in bones])
    names = [b["name"] for b in bones]
    if name not in names:
        fail("no bone %s" % name)
    return W[names.index(name)], names.index(name)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", required=True)
    ap.add_argument("--drop", required=True, help="the base material to remove, by name")
    ap.add_argument("--donor", required=True)
    ap.add_argument("--donor-tex", required=True, help="<stem>: the donor's _D/_N/_RMOE PNGs")
    ap.add_argument("--bone", default="Weapon")
    ap.add_argument("--out", required=True)
    ap.add_argument("--texout", required=True)
    a = ap.parse_args(argv)

    bm, bpos, bnrm, buv, binf, bidx = load(a.base)
    dm, dpos, dnrm, duv, dinf, didx = load(a.donor)

    # ---- the base, less its weapon -------------------------------------------------------
    drop = [i for i, m in enumerate(bm["materials"]) if a.drop in (m["path"] or m["slot"])]
    if not drop:
        fail("no base material matches %s" % a.drop)
    keep_tris, keep_mat = [], []
    for s in bm["sections"]:
        if s["mat"] in drop:
            continue
        t = bidx[s["first"]:s["first"] + s["faces"] * 3].reshape(-1, 3)
        keep_tris.append(t)
        keep_mat.append(s["mat"])
    used = np.unique(np.concatenate(keep_tris))
    bmap = -np.ones(len(bpos), np.int64)
    bmap[used] = np.arange(len(used))
    mat_new = {}
    materials = []
    for i, m in enumerate(bm["materials"]):
        if i not in drop:
            mat_new[i] = len(materials)
            materials.append(m)

    # ---- the donor's weapon ------------------------------------------------------------------
    dnames = [b["name"] for b in dm["bones"]]
    _, wbone = bind(dm, a.bone)
    under = {wbone}
    for i, b in enumerate(dm["bones"]):
        if b["parent"] in under:
            under.add(i)
    onw = np.zeros(len(dpos))
    for j in range(8):
        onw += np.where(np.isin(dinf["b"][:, j], list(under)), dinf["w"][:, j], 0.0)
    wv = onw > 0.999
    dt = didx.reshape(-1, 3)
    cut = wv[dt].any(1) & ~wv[dt].all(1)
    if cut.any():
        fail("%d donor triangles join the weapon to the body" % int(cut.sum()))
    wt = dt[wv[dt].all(1)]
    wused = np.unique(wt)
    dmap = -np.ones(len(dpos), np.int64)
    dmap[wused] = len(used) + np.arange(len(wused))

    Wd, _ = bind(dm, a.bone)
    Wb, bbone = bind(bm, a.bone)
    M = Wb @ np.linalg.inv(Wd)
    wp = (M @ np.c_[dpos[wused], np.ones(len(wused))].T).T[:, :3]
    R = M[:3, :3] / np.cbrt(np.linalg.det(M[:3, :3]))
    wn = dnrm[wused] @ R.T
    wn /= np.linalg.norm(wn, axis=1, keepdims=True) + 1e-12

    # ---- its corner of the atlas -------------------------------------------------------------
    wuv = duv[wused].astype(np.float64)
    src = {k: Image.open("%s_%s.png" % (a.donor_tex, k)) for k in ("D", "N", "RMOE", "C")
           if os.path.exists("%s_%s.png" % (a.donor_tex, k))}     # C: the team mask
    W, H = src["D"].size
    x0 = max(0, int(np.floor(wuv[:, 0].min() * W)) - MARGIN)
    x1 = min(W, int(np.ceil(wuv[:, 0].max() * W)) + MARGIN)
    y0 = max(0, int(np.floor(wuv[:, 1].min() * H)) - MARGIN)
    y1 = min(H, int(np.ceil(wuv[:, 1].max() * H)) + MARGIN)
    stem = os.path.basename(a.donor_tex) + "_Weapon"
    os.makedirs(a.texout, exist_ok=True)
    for k, im in src.items():
        im.crop((x0, y0, x1, y1)).save(os.path.join(a.texout, "%s_%s.png" % (stem, k)))
    # Unreal UVs, v down the image; the crop keeps that sense
    nuv = np.c_[(wuv[:, 0] * W - x0) / (x1 - x0), (wuv[:, 1] * H - y0) / (y1 - y0)]
    materials.append({"slot": "Weapon", "imported": "Weapon", "path": None, "tex": stem})

    # ---- write ---------------------------------------------------------------------------------
    pos = np.vstack([bpos[used], wp]).astype(np.float32)
    nrm = np.vstack([bnrm[used], wn]).astype(np.float32)
    uv = np.vstack([buv[used], nuv]).astype(np.float32)
    inf = np.zeros(len(pos), REC)
    inf[:len(used)] = binf[used]
    inf["b"][len(used):, 0] = bbone
    inf["w"][len(used):, 0] = 1.0
    sections, idx = [], []
    for t, m in zip(keep_tris, keep_mat):
        sections.append({"mat": mat_new[m], "first": len(idx) * 3, "faces": len(t)})
        idx.extend(bmap[t].tolist())
    sections.append({"mat": len(materials) - 1, "first": len(idx) * 3, "faces": len(wt)})
    idx.extend(dmap[wt].tolist())
    idx = np.asarray(idx, np.int64).reshape(-1)
    meta = {"bones": bm["bones"], "materials": materials, "sections": sections,
            "nverts": len(pos), "nidx": len(idx), "lods": 1,
            "made_by": "ue_weapon_swap.py: %s less %s, plus the %s of %s" % (
                os.path.basename(a.base), a.drop, a.bone, os.path.basename(a.donor))}
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    json.dump(meta, open(a.out + ".json", "w"), indent=2)
    with open(a.out + ".bin", "wb") as f:
        f.write(pos.tobytes()); f.write(nrm.tobytes()); f.write(uv.tobytes())
        f.write(inf.tobytes()); f.write(idx.astype("<u4").tobytes())
    print("SWAP|base|%d vertices, %d triangles kept (dropped %s)" % (
        len(used), sum(len(t) for t in keep_tris), a.drop))
    print("SWAP|weapon|%d vertices, %d triangles from %s, on %s" % (
        len(wused), len(wt), os.path.basename(a.donor), a.bone))
    print("SWAP|atlas|%dx%d of %dx%d cut at %d,%d" % (x1 - x0, y1 - y0, W, H, x0, y0))
    print("SWAP|wrote|%s" % a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
