#!/usr/bin/env python3
"""Put a weapon from a static FBX into a Tempest Rising soldier's hands: read it, cut it
down to a low-poly version for a soldier a few dozen pixels tall, fit it to the weapon
bone, and write a new mesh dump that ue_c3m.py converts as usual.

    python3 tools/art/weapon_graft.py \\
        --base   art/models/tempest_rising_gdi_rifleman/mesh/SK_GDI_Rifleman \\
        --drop   M_GDI_Weapon \\
        --fbx    "art/weapons/m16a1/Base mesh.fbx" \\
        --tex    art/weapons/m16a1/textures --stem T_M16A1 \\
        --muzzle +z --up +y --length 99 --grip -21,-9 --at -15,0,-12 \\
        --tris 300 --texsize 256 \\
        --out    art/models/tempest_rising_gdi_rifleman/mesh/SK_GDI_Rifleman_M16 \\
        --texout art/models/tempest_rising_gdi_rifleman/textures

(the numbers the shipped models were made with are in each model folder's README).

THE FBX. Every mesh in the file, each through its node's full static transform: FBX's
translation, rotation offset and pivot, pre-rotation, rotation, post-rotation, scaling
offset and pivot, scale, and the geometric transform that applies to the mesh alone. A
file whose parts are placed by pivots (a gun modelled part by part is) comes out
assembled. Materials are named per triangle.

THE WEAPON BONE'S SPACE. Measured on the Tempest Rising rifle: centimetres, muzzle along
+x, up +z, the right hand's grip near the origin. --muzzle and --up name the FBX's own
axes that point that way; --length scales the weapon to its real length along the
muzzle axis, in centimetres; --grip is the (along, up) point on the weapon, measured from
its rear end and its bore line, that goes to --at in the bone's space.

LOW POLY. tools/art/mesh_lod.py's collapse, run on the whole weapon welded by position,
down to --tris triangles. Every material keeps its own UVs; the textures are cut to
--texsize.

TEXTURES, turned into what ue_c3m.py reads (<stem>_<mat>_D/_N/_RMOE.png):
  Unity metallic: BaseColor, Metallic (r metal, a smoothness), AO, Normal (OpenGL style)
  Unity legacy:   diffuse, normal (OpenGL style), specular (a gloss when it has alpha)
Roughness is 1 - smoothness; the normal map is turned DirectX style because ue_c3m.py
takes Unreal's maps and turns them back.
"""
import argparse
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fbx_c3m as F                         # noqa: E402  the FBX record reader
import mesh_lod                             # noqa: E402
from ue_weapon_swap import REC, bind, load  # noqa: E402


def fail(msg):
    print("GRAFT|error|%s" % msg)
    sys.exit(1)


# ---------------------------------------------------------------------------------- #
#  The FBX, statically, with pivots
# ---------------------------------------------------------------------------------- #

def _v(p, k, d):
    return np.array([float(x) for x in p[k]]) if k in p else np.array(d, float)


def _t(v):
    m = np.eye(4)
    m[:3, 3] = v
    return m


def _r(deg):
    m = np.eye(4)
    m[:3, :3] = F.euler_xyz(deg)
    return m


def _s(v):
    return np.diag([v[0], v[1], v[2], 1.0])


def local(sc, oid):
    """FBX's own composition, static:
       T * Roff * Rp * Rpre * R * Rpost^-1 * Rp^-1 * Soff * Sp * S * Sp^-1"""
    p = F.p70(sc.models[oid])
    if int(p.get("RotationOrder", [0])[0]) != 0:
        fail("node %s is not XYZ euler order" % sc.name(oid))
    Rp, Sp = _v(p, "RotationPivot", [0, 0, 0]), _v(p, "ScalingPivot", [0, 0, 0])
    return (_t(_v(p, "Lcl Translation", [0, 0, 0])) @ _t(_v(p, "RotationOffset", [0, 0, 0]))
            @ _t(Rp) @ _r(_v(p, "PreRotation", [0, 0, 0])) @ _r(_v(p, "Lcl Rotation", [0, 0, 0]))
            @ np.linalg.inv(_r(_v(p, "PostRotation", [0, 0, 0]))) @ _t(-Rp)
            @ _t(_v(p, "ScalingOffset", [0, 0, 0])) @ _t(Sp)
            @ _s(_v(p, "Lcl Scaling", [1, 1, 1])) @ _t(-Sp))


def geometric(sc, oid):
    p = F.p70(sc.models[oid])
    return (_t(_v(p, "GeometricTranslation", [0, 0, 0])) @ _r(_v(p, "GeometricRotation", [0, 0, 0]))
            @ _s(_v(p, "GeometricScaling", [1, 1, 1])))


def world(sc):
    W = {}

    def walk(oid, parent):
        w = parent @ local(sc, oid)
        W[oid] = w
        for c in sc.children.get(oid, []):
            walk(c, w)
    for c in sc.children.get(0, []):
        walk(c, np.eye(4))
    return W


def read_weapon(path, min_part=0.0, skip=()):
    """Every mesh, assembled: positions, normals, uvs per triangle corner, and a material
    name per triangle. A part whose largest side is under min_part (the file's units) is
    left out: pins and screws are nothing at a soldier's size."""
    sc = F.Scene(path)
    W = world(sc)
    P, N, UV, M, PART = [], [], [], [], []
    for g in sc.obj.values():
        if g.name != "Geometry" or sc.kind(g.props[0]) != "Mesh":
            continue
        gid = g.props[0]
        owners = [c[2] for c in sc.conn if c[0] == "OO" and c[1] == gid and c[2] in sc.models]
        if not owners:
            continue
        mid = owners[0]
        if any(k in sc.name(mid) for k in skip):
            continue
        mats = [sc.name(c[1]) for c in sc.conn
                if c[0] == "OO" and c[2] == mid and sc.obj.get(c[1]) is not None
                and sc.obj[c[1]].name == "Material"]
        X = W[mid] @ geometric(sc, mid)
        R = np.linalg.inv(X[:3, :3]).T
        v, tris = F.corners(sc, g)
        pw = (X[:3, :3] @ v.T).T
        if (pw.max(0) - pw.min(0)).max() < min_part:
            continue
        for tri, m in tris:
            for cp, n, uv in tri:
                P.append(X[:3, :3] @ v[cp] + X[:3, 3])
                nn = R @ n
                N.append(nn / (np.linalg.norm(nn) + 1e-12))
                UV.append(uv)
            M.append(mats[m] if m < len(mats) else (mats[0] if mats else "default"))
            PART.append(mid)
    return (np.array(P).reshape(-1, 3, 3), np.array(N).reshape(-1, 3, 3),
            np.array(UV).reshape(-1, 3, 2), M, float(sc.globals.get("UnitScaleFactor", [1])[0]),
            np.array(PART))


# ---------------------------------------------------------------------------------- #
#  Textures
# ---------------------------------------------------------------------------------- #

def _find(texdir, mat, keys):
    """The map named <material>_<key>..., spaces and case aside. Exactly that: one
    material's name is often the start of another's (Mat_5_56 and Mat_5_56_1)."""
    low = mat.lower().replace(" ", "_")
    for k in keys:
        for n in sorted(os.listdir(texdir)):
            nl = n.lower().replace(" ", "_")
            if nl.startswith("._"):
                continue
            stem = nl.rsplit(".", 1)[0]
            for pre in ("", "base_mesh_"):
                if stem.startswith(pre + low + "_" + k):
                    return os.path.join(texdir, n)
    return None


def textures(texdir, mat, size, out_stem, smooth_scale=1.0):
    """Write out_stem_D/_N/_RMOE.png for one material; returns False without a colour map."""
    d = _find(texdir, mat, ("basecolor", "diffuse", "albedo"))
    if not d:
        return False
    def sq(p, mode):
        im = Image.open(p).convert(mode)
        return im.resize((size, size), Image.LANCZOS) if im.size != (size, size) else im
    sq(d, "RGB").save(out_stem + "_D.png")
    n = _find(texdir, mat, ("normal",))
    if n:
        a = np.asarray(sq(n, "RGB"), np.uint8).copy()
        a[..., 1] = 255 - a[..., 1]                       # OpenGL style -> DirectX style
        Image.fromarray(a).save(out_stem + "_N.png")
    rough = np.full((size, size), 0.6)
    metal = np.zeros((size, size))
    ao = np.ones((size, size))
    mt = _find(texdir, mat, ("metallic",))
    if mt:
        # Unity: metal in r, smoothness in a times the material's _GlossMapScale; a map
        # with no alpha reads as a = 1
        a = np.asarray(sq(mt, "RGBA"), np.float64) / 255.0
        metal, rough = a[..., 0], 1.0 - a[..., 3] * smooth_scale
    sp = _find(texdir, mat, ("specular",))
    if sp and not mt:
        im = Image.open(sp)
        if "A" in im.getbands():
            rough = 1.0 - np.asarray(sq(sp, "RGBA"), np.float64)[..., 3] / 255.0
    o = _find(texdir, mat, ("ao", "occlusion"))
    if o:
        ao = np.asarray(sq(o, "L"), np.float64) / 255.0
    rmoe = np.stack([rough, metal, ao, np.zeros_like(ao)], axis=2)
    Image.fromarray(np.clip(rmoe * 255 + 0.5, 0, 255).astype(np.uint8), "RGBA").save(out_stem + "_RMOE.png")
    return True


# ---------------------------------------------------------------------------------- #

AXES = {"+x": [1, 0, 0], "-x": [-1, 0, 0], "+y": [0, 1, 0], "-y": [0, -1, 0],
        "+z": [0, 0, 1], "-z": [0, 0, -1]}


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", required=True)
    ap.add_argument("--drop", required=True)
    ap.add_argument("--fbx", required=True)
    ap.add_argument("--tex", required=True)
    ap.add_argument("--stem", required=True)
    ap.add_argument("--muzzle", required=True, choices=list(AXES))
    ap.add_argument("--up", required=True, choices=list(AXES))
    ap.add_argument("--length", type=float, required=True, help="cm along the muzzle axis")
    ap.add_argument("--grip", required=True, help="along,up: cm from the rear end and the bore")
    ap.add_argument("--at", required=True, help="x,y,z in the weapon bone's space, cm")
    ap.add_argument("--roll", type=float, default=0.0, help="degrees about the bore")
    ap.add_argument("--tris", type=int, default=300)
    ap.add_argument("--skip", action="append", default=[],
                    help="leave out parts whose node name contains this; may repeat")
    ap.add_argument("--texname", help="the texture files' name for every material, when "
                    "the FBX's materials are unnamed (a 'default' material)")
    ap.add_argument("--smoothness", type=float, default=1.0,
                    help="the Unity material's _GlossMapScale (Mat_Rifle_Caliber_5_56: 0.67)")
    ap.add_argument("--min-part", type=float, default=0.0,
                    help="leave out parts whose largest side is under this, file units")
    ap.add_argument("--cross-seams", action="store_true",
                    help="weld texture seams so the cut can cross them (each welded point "
                         "keeps one of its UVs)")
    ap.add_argument("--texsize", type=int, default=256)
    ap.add_argument("--bone", default="Weapon")
    ap.add_argument("--out", required=True)
    ap.add_argument("--texout", required=True)
    ap.add_argument("--report", action="store_true", help="print the weapon's profile and stop")
    a = ap.parse_args(argv)

    P, N, UV, M, unit, PART = read_weapon(a.fbx, a.min_part, a.skip)
    if a.texname:
        M = [a.texname] * len(M)
    # ---- into the bone's axes: muzzle +x, up +z, and y their cross
    fx, fz = np.array(AXES[a.muzzle], float), np.array(AXES[a.up], float)
    if abs(fx @ fz) > 0.5:
        fail("--muzzle and --up are the same axis")
    fy = np.cross(fz, fx)
    B = np.stack([fx, fy, fz])                            # rows: new axes in FBX terms
    p = P.reshape(-1, 3) @ B.T
    n = N.reshape(-1, 3) @ B.T
    if abs(np.linalg.det(B) + 1) < 1e-6:
        fail("the axes make a mirror")
    lo, hi = p.min(0), p.max(0)
    k = a.length / (hi[0] - lo[0])
    # the bore line: the median height and width of the front fifth (the barrel)
    front = p[:, 0] > hi[0] - 0.2 * (hi[0] - lo[0])
    bore = np.array([lo[0], np.median(p[front, 1]), np.median(p[front, 2])])
    p = (p - bore) * k
    if a.report:
        print("GRAFT|fbx|%d triangles, %d materials %s, unit %.3f cm, length %.1f -> %.1f cm"
              % (len(P), len(set(M)), sorted(set(M)), unit, (hi[0] - lo[0]), a.length))
        for x0 in np.arange(0, a.length, a.length / 12):
            s = (p[:, 0] >= x0) & (p[:, 0] < x0 + a.length / 12)
            if s.any():
                print("GRAFT|profile|%5.1f..%5.1f cm|up %6.1f..%6.1f|side %5.1f..%5.1f" % (
                    x0, x0 + a.length / 12, p[s, 2].min(), p[s, 2].max(), p[s, 1].min(), p[s, 1].max()))
        return 0
    ga, gu = (float(x) for x in a.grip.split(","))
    at = np.array([float(x) for x in a.at.split(",")])
    r = np.radians(a.roll)
    Rr = np.array([[1, 0, 0], [0, np.cos(r), -np.sin(r)], [0, np.sin(r), np.cos(r)]])
    p = (p - np.array([ga, 0.0, gu])) @ Rr.T + at
    n = n @ Rr.T

    # ---- weld by position and cut down
    tri_pos = p.reshape(-1, 3, 3)
    key = np.round(p / 0.01).astype(np.int64)
    # attribute vertices: position + uv + material, so seams stay seams
    mats = sorted(set(M))
    mid = np.repeat([mats.index(m) for m in M], 3)
    uvkey = np.round(UV.reshape(-1, 2) * 4096).astype(np.int64)
    if a.cross_seams:
        uvkey[:] = 0
    attr = np.c_[key, uvkey, mid]
    _, first, inv = np.unique(attr, axis=0, return_index=True, return_inverse=True)
    inv = inv.reshape(-1)
    vpos, vnrm, vuv, vmat = p[first], n[first], UV.reshape(-1, 2)[first], mid[first]
    tris = inv.reshape(-1, 3)
    good = (tris[:, 0] != tris[:, 1]) & (tris[:, 1] != tris[:, 2]) & (tris[:, 0] != tris[:, 2])
    tris = tris[good]
    tmat = np.array([mats.index(m) for m in M])[good]
    tpart = PART[good]
    nv = len(vpos)
    sb = np.zeros((nv, 4), np.uint8)
    sw = np.zeros((nv, 4), np.uint8)
    sw[:, 0] = 255
    # PART BY PART, each on its share of the budget and never under a floor: cut as one
    # mesh, the long thin parts (the barrel, the handguard) cost the least to collapse
    # and went first, entirely.
    frac = min(1.0, a.tris / max(1, len(tris)))
    if frac < 1.0:
        out_t, out_m = [], []
        for pid in sorted(set(tpart.tolist())):
            sel = tpart == pid
            pt, pm = tris[sel], tmat[sel]
            want = max(min(len(pt), 24), int(round(len(pt) * frac)))
            if want < len(pt):
                (lt, ids), = mesh_lod.simplify_chain(vpos, vnrm, pt, sb, sw, [want / len(pt)],
                                                     log=lambda *x: None)
                pt, pm = lt, pm[ids]
            out_t.append(pt)
            out_m.append(pm)
        tris, tmat = np.concatenate(out_t), np.concatenate(out_m)
    print("GRAFT|weapon|%d triangles from %d (target %d)" % (len(tris), len(P), a.tris))

    # ---- textures per material
    os.makedirs(a.texout, exist_ok=True)
    stems = {}
    for i, m in enumerate(mats):
        st = "%s_%d" % (a.stem, i)
        if textures(a.tex, m, a.texsize, os.path.join(a.texout, st), a.smoothness):
            stems[i] = st
        else:
            print("GRAFT|warning|no colour map found for material %s" % m)

    # ---- the base, less its weapon, plus this one
    bm, bpos, bnrm, buv, binf, bidx = load(a.base)
    drop = [i for i, m_ in enumerate(bm["materials"]) if a.drop in ((m_["path"] or "") + m_["slot"])]
    if not drop:
        fail("no base material matches %s" % a.drop)
    keep_tris, keep_mat = [], []
    for s in bm["sections"]:
        if s["mat"] in drop:
            continue
        keep_tris.append(bidx[s["first"]:s["first"] + s["faces"] * 3].reshape(-1, 3))
        keep_mat.append(s["mat"])
    used = np.unique(np.concatenate(keep_tris))
    bmap = -np.ones(len(bpos), np.int64)
    bmap[used] = np.arange(len(used))
    mat_new, materials = {}, []
    for i, m_ in enumerate(bm["materials"]):
        if i not in drop:
            mat_new[i] = len(materials)
            materials.append(m_)

    # WINDING. The dump is Unreal's, left handed; an FBX from Unity is right handed, so
    # the same corner order faces the other way. Whichever way the base winds against its
    # own normals, the weapon is wound to match, or it is culled from the outside.
    def facing(pp, nn, tt):
        fn = np.cross(pp[tt[:, 1]] - pp[tt[:, 0]], pp[tt[:, 2]] - pp[tt[:, 0]])
        return (np.einsum("ij,ij->i", fn, nn[tt].sum(1)) > 0).mean()
    base_out = facing(bpos, bnrm, np.concatenate(keep_tris)) > 0.5
    weap_out = facing(vpos, vnrm, tris) > 0.5
    if base_out != weap_out:
        tris = tris[:, [0, 2, 1]]
    print("GRAFT|winding|base %s, weapon %s%s" % (
        "outward" if base_out else "inward", "outward" if weap_out else "inward",
        ", reversed to match" if base_out != weap_out else ""))

    Wb, bbone = bind(bm, a.bone)
    wused = np.unique(tris)
    wmap = -np.ones(nv, np.int64)
    wmap[wused] = len(used) + np.arange(len(wused))
    wp = (Wb @ np.c_[vpos[wused], np.ones(len(wused))].T).T[:, :3]
    R = Wb[:3, :3] / np.cbrt(np.linalg.det(Wb[:3, :3]))
    wn = vnrm[wused] @ R.T
    wn /= np.linalg.norm(wn, axis=1, keepdims=True) + 1e-12
    # Unity's v runs up the image, Unreal's (which the dump carries) down it
    wuv = np.c_[vuv[wused, 0], 1.0 - vuv[wused, 1]]

    pos = np.vstack([bpos[used], wp]).astype(np.float32)
    nrm = np.vstack([bnrm[used], wn]).astype(np.float32)
    uv = np.vstack([buv[used], wuv]).astype(np.float32)
    inf = np.zeros(len(pos), REC)
    inf[:len(used)] = binf[used]
    inf["b"][len(used):, 0] = bbone
    inf["w"][len(used):, 0] = 1.0
    sections, idx = [], []
    for t, m_ in zip(keep_tris, keep_mat):
        sections.append({"mat": mat_new[m_], "first": len(idx), "faces": len(t)})
        idx.extend(bmap[t].reshape(-1).tolist())
    for i in sorted(set(tmat.tolist())):
        t = tris[tmat == i]
        materials.append({"slot": "Weapon%d" % i, "imported": mats[i], "path": None,
                          "tex": stems.get(i, a.stem)})
        sections.append({"mat": len(materials) - 1, "first": len(idx), "faces": len(t)})
        idx.extend(wmap[t].reshape(-1).tolist())
    idx = np.asarray(idx, np.int64)
    meta = {"bones": bm["bones"], "materials": materials, "sections": sections,
            "nverts": len(pos), "nidx": len(idx), "lods": 1,
            "made_by": "weapon_graft.py: %s less %s, plus %s" % (
                os.path.basename(a.base), a.drop, os.path.basename(a.fbx))}
    json.dump(meta, open(a.out + ".json", "w"), indent=2)
    with open(a.out + ".bin", "wb") as f:
        f.write(pos.tobytes()); f.write(nrm.tobytes()); f.write(uv.tobytes())
        f.write(inf.tobytes()); f.write(idx.astype("<u4").tobytes())
    print("GRAFT|wrote|%s|%d materials on the weapon" % (a.out, len(set(tmat.tolist()))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
