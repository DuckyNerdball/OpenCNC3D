#!/usr/bin/env python3
"""Build the anim lab's models: for each unit and clip, the loop as the game plays it now
(current) and the same loop through its tweak file (tweaked), as two glTF binaries.

    python3 tools/anim_lab/build.py                 # every unit, the run loop
    python3 tools/anim_lab/build.py --clip aim      # another clip from the units' maps
    tools/anim_lab/serve.sh                         # then open http://localhost:8791

The mesh, skin and poses are computed as tools/art/ue_c3m.py computes them for the game:
pack axes, the mirror taken out on both sides of every bone, a pose per frame per bone in
model space. The glTF skeleton is therefore FLAT (every joint a child of the root), each
joint carrying its model-space pose as its own transform, which is exactly the product
the game's renderer multiplies a vertex by.

Writes tools/anim_lab/out/ (generated, not committed): <unit>_<clip>_current.glb,
<unit>_<clip>_tweaked.glb, and manifest.json, which the page reads. The source art is
Tempest Rising's and never leaves this machine: the page is served locally.
"""
import argparse
import io
import json
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "art"))
import fbx_c3m as F       # noqa: E402
import ue_c3m as U        # noqa: E402
import anim_tweak as AT   # noqa: E402

ART = os.path.join(REPO, "art", "models")
UNITS = {
    "rifleman": {
        "label": "GDI Rifleman, Tempest Rising rifle (old)",
        "mesh": os.path.join(ART, "tempest_rising_gdi_rifleman", "mesh", "SK_GDI_Rifleman"),
        "anims": os.path.join(ART, "tempest_rising_gdi_rifleman", "anims"),
        "clips": os.path.join(ART, "tempest_rising_gdi_rifleman", "clips.txt"),
        "tex": [os.path.join(ART, "tempest_rising_gdi_rifleman", "textures")],
    },
    "rifleman_m16": {
        "label": "GDI Minigunner, M16 (E1 / E1TR)",
        "mesh": os.path.join(ART, "tempest_rising_gdi_rifleman", "mesh", "SK_GDI_Rifleman_M16"),
        "anims": os.path.join(ART, "tempest_rising_gdi_rifleman", "anims"),
        "clips": os.path.join(ART, "tempest_rising_gdi_rifleman", "clips.txt"),
        "tex": [os.path.join(ART, "tempest_rising_gdi_rifleman", "textures")],
        "tweaks": "rifleman",
    },
    "rocketeer": {
        "label": "GDI Rocketeer, Tempest Rising launcher (old)",
        "mesh": os.path.join(ART, "tempest_rising_gdi_rocketeer", "mesh", "SK_GDI_Rocketeer"),
        "anims": os.path.join(ART, "tempest_rising_gdi_rocketeer", "anims"),
        "clips": os.path.join(ART, "tempest_rising_gdi_rocketeer", "clips.txt"),
        "tex": [os.path.join(ART, "tempest_rising_gdi_rocketeer", "textures"),
                os.path.join(ART, "tempest_rising_gdi_rifleman", "textures")],
    },
    "rocketeer_law": {
        "label": "GDI Rocketeer, LAW (E3)",
        "mesh": os.path.join(ART, "tempest_rising_gdi_rocketeer", "mesh", "SK_GDI_Rocketeer_LAW"),
        "anims": os.path.join(ART, "tempest_rising_gdi_rocketeer", "anims"),
        "clips": os.path.join(ART, "tempest_rising_gdi_rocketeer", "clips.txt"),
        "tex": [os.path.join(ART, "tempest_rising_gdi_rocketeer", "textures"),
                os.path.join(ART, "tempest_rising_gdi_rifleman", "textures")],
        "tweaks": "rocketeer_law",
    },
}
HEIGHT = 0.55            # the game's standing height, cells (ue_c3m --height)
FPS = 30.0
TEX = 1024               # the game's own size, so the normal maps read as they do there


def clip_line(path, name):
    for line in open(path):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        n, rest = [x.strip() for x in line.split("=", 1)]
        if n == name:
            parts = rest.split()
            t0 = t1 = None
            for p_ in parts[1:]:
                if p_.startswith("section="):
                    t0, t1 = (float(x) for x in p_[8:].split(":"))
            return parts[0], "once" in parts[1:], t0, t1
    raise SystemExit("%s has no clip %s" % (path, name))


def has_clip(path, name):
    try:
        clip_line(path, name)
        return True
    except SystemExit:
        return False


def load_mesh(prefix):
    meta = json.load(open(prefix + ".json"))
    nv, ni = meta["nverts"], meta["nidx"]
    raw = open(prefix + ".bin", "rb").read()
    o = 0
    pos = np.frombuffer(raw, np.float32, nv * 3, o).reshape(nv, 3).astype(float); o += nv * 12
    nrm = np.frombuffer(raw, np.float32, nv * 3, o).reshape(nv, 3).astype(float); o += nv * 12
    uv = np.frombuffer(raw, np.float32, nv * 2, o).reshape(nv, 2).astype(float); o += nv * 8
    inf = np.frombuffer(raw, np.dtype([("b", "<u2", 8), ("w", "<f4", 8)]), nv, o); o += nv * 48
    idx = np.frombuffer(raw, np.uint32, ni, o).astype(np.int64)
    return meta, pos, nrm, uv, inf, idx


_MAPS = {}


def _png(img):
    b = io.BytesIO()
    img.save(b, "PNG")
    return b.getvalue()


def material_maps(unit, mat):
    """Every map the game's model shader reads, built exactly as ue_c3m builds them, at the
    lab's size: the colour map (team mask in alpha, the paint under it neutralised), the
    normal map (turned to OpenGL style, as the game's is), the surface map as glTF wants
    it (occlusion, roughness, metal in r, g, b: the game's own layout), and the cloth mask.
    The cloth mask and the two emblem stencils go in as the EMISSIVE map (r, g, b), with no
    emissive colour, so it lights nothing; the page reads them from there. It is not the surface map's alpha, as it is in
    the game, because a browser may decode an image premultiplied, and every texel with no
    cloth would lose its occlusion, roughness and metal.
    Returns {"base", "normal", "orm", "cloth": png bytes or None, "team": bool}."""
    stem = os.path.basename((mat["path"] or mat["slot"]).split(".")[0])
    tex_stem = mat.get("tex") or ("T_" + stem[2:] if stem.startswith("M_") else stem)
    key = (tuple(unit["tex"]), tex_stem)
    if key in _MAPS:
        return _MAPS[key]
    out = {"base": None, "normal": None, "orm": None, "cloth": None, "team": False,
           "emblem": False}
    for td in unit["tex"]:
        for cand in (tex_stem, tex_stem.replace("_Weapon", "_Rifleman_Rifle")):
            p = os.path.join(td, cand + "_D.png")
            if not os.path.exists(p):
                continue
            mp = os.path.join(td, cand + "_C.png")
            mask = mp if os.path.exists(mp) else None
            bl = os.path.join(td, cand + "_Blank.png")
            lg = os.path.join(td, cand + "_LogoGDI.png")
            ln = os.path.join(td, cand + "_LogoNod.png")
            emblem = os.path.exists(lg) and os.path.exists(ln)
            if emblem and os.path.exists(bl):
                p = bl
            out["base"] = _png(U.base_with_mask(p, mask, TEX))
            out["team"] = mask is not None
            np_ = os.path.join(td, cand + "_N.png")
            if os.path.exists(np_):
                w, h, data = U.load_normal_dx(np_, TEX)
                a = np.frombuffer(data, np.uint8).reshape(h, w, 4)[::-1, :, :3]
                out["normal"] = _png(Image.fromarray(np.ascontiguousarray(a), "RGB"))
            rp = os.path.join(td, cand + "_RMOE.png")
            if os.path.exists(rp):
                w, h, data = U.load_rmoe(rp, TEX)
                a = np.frombuffer(data, np.uint8).reshape(h, w, 4)[::-1, :, :3]
                out["orm"] = _png(Image.fromarray(np.ascontiguousarray(a), "RGB"))
            # the extra masks, one per channel: r cloth, g the GDI emblem, b Nod's
            cp = os.path.join(td, cand + "_Cloth.png")
            chans = [cp if os.path.exists(cp) else None,
                     lg if emblem else None, ln if emblem else None]
            if any(chans) and out["orm"]:
                side = min(TEX, Image.open(p).size[0])
                rgb = np.zeros((side, side, 3), np.uint8)
                for i, cpath in enumerate(chans):
                    if cpath:
                        c = Image.open(cpath).convert("L")
                        if c.size != (side, side):
                            c = c.resize((side, side), Image.LANCZOS)
                        rgb[..., i] = np.asarray(c, np.uint8)
                out["cloth"] = _png(Image.fromarray(rgb, "RGB"))
                out["emblem"] = emblem
            _MAPS[key] = out
            return out
    _MAPS[key] = out
    return out


def team_colours():
    """The colours the game multiplies a model's team mask by, read out of the renderer's
    own source so the lab cannot disagree with it: house_colour's campaign sides and the
    skirmish liveries' step 2 (the entry house_colour takes)."""
    import re
    src = open(os.path.join(REPO, "game", "cnc_eyes.cpp"), encoding="latin-1").read()
    out = []
    fn = src[src.index("static void house_colour("):]
    fn = fn[:fn.index("\n}\n")]
    for side, label in ((0, "GDI (campaign)"), (1, "Nod (campaign)")):
        m = re.search(r"if \(side == %d\) \{ \*r = ([0-9.]+)f; \*g = ([0-9.]+)f; \*b = ([0-9.]+)f;" % side, fn)
        out.append({"label": label, "rgb": [float(m.group(i)) for i in (1, 2, 3)]})
    names = re.search(r"LIVERY_NAME\[LIVERY_COUNT\] = \{([^}]*)\}", src).group(1)
    names = re.findall(r'"([A-Z]+)"', names)
    band = src[src.index("static const unsigned char LIVERY_BAND"):]
    band = band[band.index("{") + 1:]
    rows = re.findall(r"\{((?:\s*\{\s*\d+,\s*\d+,\s*\d+\},?){16})\s*\}", band)[:len(names)]
    for name, row in zip(names, rows):
        trip = re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+)\}", row)
        out.append({"label": "Skirmish: " + name.title().replace("Ltblue", "Light Blue"),
                    "rgb": [int(x) / 255.0 for x in trip[2]]})
    return out


class Glb:
    """A minimal glTF 2.0 binary writer."""
    def __init__(self):
        self.bin = bytearray()
        self.views, self.acc = [], []

    def data(self, arr, ctype, typ, target=None, minmax=False):
        arr = np.ascontiguousarray(arr)
        while len(self.bin) % 4:
            self.bin.append(0)
        off = len(self.bin)
        self.bin += arr.tobytes()
        v = {"buffer": 0, "byteOffset": off, "byteLength": arr.nbytes}
        if target:
            v["target"] = target
        self.views.append(v)
        count = arr.shape[0]
        a = {"bufferView": len(self.views) - 1, "componentType": ctype, "count": count,
             "type": typ}
        if minmax:
            flat = arr.reshape(count, -1)
            a["min"] = flat.min(0).tolist()
            a["max"] = flat.max(0).tolist()
        self.acc.append(a)
        return len(self.acc) - 1

    def image(self, png):
        while len(self.bin) % 4:
            self.bin.append(0)
        off = len(self.bin)
        self.bin += png
        self.views.append({"buffer": 0, "byteOffset": off, "byteLength": len(png)})
        return len(self.views) - 1

    def write(self, path, js):
        js["buffers"] = [{"byteLength": len(self.bin)}]
        js["bufferViews"] = self.views
        js["accessors"] = self.acc
        j = json.dumps(js).encode()
        j += b" " * ((4 - len(j) % 4) % 4)
        while len(self.bin) % 4:
            self.bin.append(0)
        out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(self.bin))
        out += struct.pack("<II", len(j), 0x4E4F534A) + j
        out += struct.pack("<II", len(self.bin), 0x004E4942) + bytes(self.bin)
        open(path, "wb").write(out)


def build(unit_key, clip, outdir, tweak):
    u = UNITS[unit_key]
    meta, pos, nrm, uv, inf, idx = load_mesh(u["mesh"])
    bones = meta["bones"]
    nv = len(pos)
    # ---- ue_c3m's pack transform, exactly ------------------------------------------------
    B = U.axis_basis("+y")
    pb = pos @ B.T
    ground = pb[:, 1].min()
    k = HEIGHT * F.CELL / (pb[:, 1].max() - ground)
    cx = 0.5 * (pb[:, 0].min() + pb[:, 0].max())
    cz = 0.5 * (pb[:, 2].min() + pb[:, 2].max())
    P = np.eye(4)
    P[:3, :3] = B * k
    P[:3, 3] = -np.array([cx, ground, cz]) * k
    Q = np.eye(4)
    Q[:3, :3] = np.linalg.inv(B)
    ppos = (P @ np.c_[pos, np.ones(nv)].T).T[:, :3]
    pnrm = nrm @ B.T
    pnrm /= np.linalg.norm(pnrm, axis=1, keepdims=True) + 1e-12
    tris = idx.reshape(-1, 3)
    fn = np.cross(ppos[tris[:, 1]] - ppos[tris[:, 0]], ppos[tris[:, 2]] - ppos[tris[:, 0]])
    if float((np.einsum("ij,ij->i", fn, pnrm[tris].sum(1)) > 0).mean()) < 0.5:
        tris = tris[:, [0, 2, 1]]
    joints = np.zeros((nv, 4), np.uint8)
    weights = np.zeros((nv, 4), np.float32)
    for v in range(nv):
        lst = sorted(((float(inf["w"][v][j]), int(inf["b"][v][j])) for j in range(8)
                      if inf["w"][v][j] > 0), reverse=True)[:4]
        tot = sum(w for w, _ in lst)
        for j, (w, b) in enumerate(lst):
            joints[v, j], weights[v, j] = b, w / tot
    bind = U.model_space(bones, [U.trs(b["r"], b["t"], b["s"]) for b in bones])
    ib = [np.linalg.inv(P @ W @ Q) for W in bind]

    anim, once, t0, t1 = clip_line(u["clips"], clip)
    apath = os.path.join(u["anims"], anim + ".json")
    results = {}
    for which, tw in (("current", {}), ("tweaked", tweak)):
        d, nframe, local = AT.sample_clip(apath, bones, FPS, once, t0, t1, tw)
        Wf = AT.model_space_frames(bones, local, U.trs, U.model_space)
        poses = [[F.decompose(P @ Wb @ Q) for Wb in fr] for fr in Wf]
        # stride, as ue_c3m measures it (the lowest foot's travel over a loop)
        stride = 0.0
        feet = [{b["name"]: (P @ Wb)[:3, 3] for b, Wb in zip(bones, fr)
                 if "foot" in b["name"].lower()} for fr in Wf]
        if feet and feet[0] and not once and clip in U.LOCOMOTION:
            for f in range(nframe):
                a0, a1 = feet[f], feet[(f + 1) % nframe]
                low = min(a0, key=lambda b: a0[b][1])
                stride += abs(a1[low][2] - a0[low][2])

        g = Glb()
        a_pos = g.data(ppos.astype(np.float32), 5126, "VEC3", 34962, True)
        a_nrm = g.data(pnrm.astype(np.float32), 5126, "VEC3", 34962)
        a_uv = g.data(uv.astype(np.float32), 5126, "VEC2", 34962)
        a_j = g.data(joints, 5121, "VEC4", 34962)
        a_w = g.data(weights, 5126, "VEC4", 34962)
        prims, mats, texs, imgs = [], [], [], []
        for s in meta["sections"]:
            t = tris.reshape(-1)[s["first"]:s["first"] + s["faces"] * 3].astype(np.uint32)
            a_i = g.data(t, 5125, "SCALAR", 34963)
            mat = meta["materials"][s["mat"]]
            mp = material_maps(u, mat)
            m = {"pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 0.8},
                 "doubleSided": False,
                 "extras": {"teamMask": mp["team"],
                            "clothMask": bool(mp["cloth"]) and not mp["emblem"],
                            "emblem": mp["emblem"]}}

            def tex(png):
                imgs.append({"bufferView": g.image(png), "mimeType": "image/png"})
                texs.append({"source": len(imgs) - 1})
                return {"index": len(texs) - 1}
            if mp["base"]:
                m["pbrMetallicRoughness"]["baseColorTexture"] = tex(mp["base"])
            if mp["normal"]:
                m["normalTexture"] = tex(mp["normal"])
            if mp["orm"]:
                t_ = tex(mp["orm"])
                m["pbrMetallicRoughness"].update(metallicFactor=1.0, roughnessFactor=1.0,
                                                 metallicRoughnessTexture=t_)
                m["occlusionTexture"] = dict(t_)
            if mp["cloth"]:
                m["emissiveTexture"] = tex(mp["cloth"])
                m["emissiveFactor"] = [0.0, 0.0, 0.0]
            mats.append(m)
            prims.append({"attributes": {"POSITION": a_pos, "NORMAL": a_nrm,
                                         "TEXCOORD_0": a_uv, "JOINTS_0": a_j,
                                         "WEIGHTS_0": a_w},
                          "indices": a_i, "material": len(mats) - 1})
        ibm = np.array([m_.T.reshape(-1) for m_ in ib], np.float32)      # column-major
        a_ib = g.data(ibm, 5126, "MAT4")
        nb = len(bones)
        nodes = [{"name": "root", "children": list(range(1, nb + 1))}]
        for j, b in enumerate(bones):
            q, t, sc = poses[0][j]
            nodes.append({"name": b["name"], "translation": [float(x) for x in t],
                          "rotation": [float(x) for x in q], "scale": [float(sc)] * 3})
        nodes.append({"name": "body", "mesh": 0, "skin": 0})
        times = (np.arange(nframe) / FPS).astype(np.float32)
        a_t = g.data(times, 5126, "SCALAR", None, True)
        samplers, channels = [], []
        for j in range(nb):
            tr = np.array([poses[f][j][1] for f in range(nframe)], np.float32)
            qs = np.array([poses[f][j][0] for f in range(nframe)], np.float32)
            for f in range(1, nframe):
                if qs[f] @ qs[f - 1] < 0:
                    qs[f] = -qs[f]
            sc = np.array([[poses[f][j][2]] * 3 for f in range(nframe)], np.float32)
            for path, arr, typ in (("translation", tr, "VEC3"), ("rotation", qs, "VEC4"),
                                   ("scale", sc, "VEC3")):
                samplers.append({"input": a_t, "output": g.data(arr, 5126, typ),
                                 "interpolation": "LINEAR"})
                channels.append({"sampler": len(samplers) - 1,
                                 "target": {"node": j + 1, "path": path}})
        js = {"asset": {"version": "2.0", "generator": "tools/anim_lab/build.py"},
              "scene": 0, "scenes": [{"nodes": [0, nb + 1]}], "nodes": nodes,
              "meshes": [{"primitives": prims}],
              "skins": [{"joints": list(range(1, nb + 1)), "inverseBindMatrices": a_ib,
                         "skeleton": 0}],
              "materials": mats, "textures": texs, "images": imgs,
              "samplers": [{"magFilter": 9729, "minFilter": 9987}],
              "animations": [{"name": clip, "samplers": samplers, "channels": channels}]}
        for t_ in texs:
            t_["sampler"] = 0
        fname = "%s_%s_%s.glb" % (unit_key, clip, which)
        g.write(os.path.join(outdir, fname), js)
        results[which] = {"file": fname, "frames": nframe, "seconds": nframe / FPS,
                          "stride": stride, "loop": not once}
        print("LAB|%s|%s|%s|%d frames|%.2f s|stride %.0f units" % (
            unit_key, clip, which, nframe, nframe / FPS, stride))
    results["anim"] = anim
    return results


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--clip", action="append", default=[],
                    help="a clip name from the units' maps; may repeat (default: run)")
    ap.add_argument("--unit", action="append", default=[])
    a = ap.parse_args(argv)
    outdir = os.path.join(HERE, "out")
    os.makedirs(outdir, exist_ok=True)
    mpath = os.path.join(outdir, "manifest.json")
    manifest = json.load(open(mpath)) if os.path.exists(mpath) else {"units": {}}
    for key in (a.unit or list(UNITS)):
        for clip in (a.clip or ["run"]):
            if not has_clip(UNITS[key]["clips"], clip):
                print("LAB|%s|%s|not in this unit's clip map, skipped" % (key, clip))
                continue
            tpath = os.path.join(HERE, "tweaks", "%s_%s.json" % (UNITS[key].get("tweaks", key), clip))
            tweak = AT.load_tweak(tpath)
            r = build(key, clip, outdir, tweak)
            r["tweak"] = tweak
            manifest["units"].setdefault(key, {"label": UNITS[key]["label"], "clips": {}})
            manifest["units"][key]["clips"][clip] = r
    manifest["cell"] = F.CELL
    manifest["teams"] = team_colours()
    json.dump(manifest, open(mpath, "w"), indent=1)
    print("LAB|wrote|%s" % mpath)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
