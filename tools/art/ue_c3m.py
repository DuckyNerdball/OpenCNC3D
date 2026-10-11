#!/usr/bin/env python3
"""Convert an Unreal skeletal mesh and its animations, dumped by trextract, into a version 2 .c3m.

The sibling of fbx_c3m.py for characters whose only source is an Unreal Engine project:
an editor .uasset has no FBX beside it, so tools/art/trextract (a small CUE4Parse program)
dumps what the asset holds into plain files first, and this script turns those into the
same file fbx_c3m.py writes. Plain Python 3: numpy and Pillow.

This is the exact invocation that produced the Tempest Rising GDI rifleman (E1.c3m, the
Minigunner, is the same command with --type E1 and --out game/models3d/E1.c3m):

    python3 tools/art/ue_c3m.py --type E1TR \\
        --mesh art/models/tempest_rising_gdi_rifleman/mesh/SK_GDI_Rifleman_M16 \\
        --anims art/models/tempest_rising_gdi_rifleman/anims \\
        --clipmap art/models/tempest_rising_gdi_rifleman/clips.txt \\
        --texdir art/models/tempest_rising_gdi_rifleman/textures \\
        --height 0.55 --texmax 1024 --lods 0.5@0.1,0.25@0.06 \\
        --tweaks tools/anim_lab/tweaks/rifleman \\
        --preview /tmp/e1tr.png --out game/models3d/E1TR.c3m

(SK_GDI_Rifleman_M16 is the rifleman dump with its rifle replaced by an M16 by
tools/art/weapon_graft.py; the command is in art/weapons/m16a1/README.md.)

WHAT THE DUMP IS (trextract writes it; see its README):

  <mesh>.json   the reference skeleton (per bone: name, parent, LOCAL rotation, translation,
                scale, as Unreal's FTransform), the material slots, the sections.
  <mesh>.bin    every position, every normal, every uv, then per vertex eight 16-bit
                bone indices and eight float weights; then the 32-bit index buffer. Unreal centimetres, Unreal axes.
  <anim>.json   per bone track: every key of rotation, translation and scale, in the
                bone's LOCAL space, spaced evenly over the clip's length.

UNREAL'S AXES ARE LEFT HANDED (x forward, y right, z up). The pack is right handed (x east,
y up, z south). So the axis change is a MIRROR, and a mirror cannot be a bone's rotation.
It is absorbed on both sides of every bone instead: the pose written is P W Q and the
inverse bind is inv(P W_bind Q), with Q the inverse of the mirror. Their product, which is
all the renderer ever multiplies a vertex by, is P W inv(W_bind) inv(P), exactly the
change of axes applied to Unreal's own skinning; each factor on its own is a proper
rotation, a translation and a uniform scale, which is what the file can carry.

A mirror also reverses the winding of every triangle as seen from outside, which is what
turns Unreal's clockwise front faces into the counter-clockwise ones GL culls by. That
is checked, not assumed: the face normals are compared against the vertex normals and
the script refuses a mesh that comes out inside out.

CLIPS are named by a map file, one line each: `clip = AnimName [once] [section=T0:T1]`,
the last taking only those seconds of the clip, as an Unreal montage section does. A clip whose keys
are denser than --fps (some Unreal clips are keyed at 1920 a second) is resampled to
--fps; quaternions are normalised-lerped between keys, translations lerped.
"""
import argparse
import json
import os
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fbx_c3m as F  # noqa: E402  the writer's constants, quaternion maths and preview

CELL = F.CELL


def fail(msg):
    print("UE|error|%s" % msg)
    sys.exit(1)


# ---------------------------------------------------------------------------------- #
#  Unreal transforms
# ---------------------------------------------------------------------------------- #

def trs(q, t, s):
    """Unreal FTransform (quat xyzw, translation, scale) as a column-vector 4x4. Unreal
    multiplies row vectors, but a quaternion rotates a vector the same way in either
    notation, so R(q) here is the same rotation FQuat::RotateVector applies."""
    q = np.asarray(q, np.float64)
    q = q / (np.linalg.norm(q) + 1e-30)
    m = np.eye(4)
    m[:3, :3] = F.quat_to_mat(q) * np.asarray(s, np.float64)[None, :]
    m[:3, 3] = t
    return m


def model_space(bones, local):
    """Local 4x4s -> model space, walking parents (they precede children in Unreal)."""
    out = [None] * len(bones)
    for i, b in enumerate(bones):
        p = b["parent"]
        out[i] = local[i] if p < 0 else out[p] @ local[i]
    return out


def axis_basis(forward):
    """3x3 taking Unreal axes to pack axes with the character's front turned north.
    Pack rows: the character's right (east), up, and backward (south). In Unreal's left
    handed frame the right of a forward (fx, fy) on the ground is (-fy, fx)."""
    sign = -1.0 if forward.startswith("-") else 1.0
    f = np.zeros(3)
    f["xyz".index(forward[-1])] = sign
    if abs(f[2]) > 0.5:
        fail("--forward %s lies along Unreal's up axis" % forward)
    right = np.array([-f[1], f[0], 0.0])
    up = np.array([0.0, 0.0, 1.0])
    return np.stack([right, up, -f])


# ---------------------------------------------------------------------------------- #
#  Clips
# ---------------------------------------------------------------------------------- #

def sample_track(keys, t01, lerp_quat):
    """keys spaced evenly over [0, 1]; the value at t01."""
    n = len(keys)
    if n == 1:
        return np.asarray(keys[0], np.float64)
    x = min(max(t01, 0.0), 1.0) * (n - 1)
    i0 = int(np.floor(x))
    i1 = min(i0 + 1, n - 1)
    u = x - i0
    a = np.asarray(keys[i0], np.float64)
    b = np.asarray(keys[i1], np.float64)
    if lerp_quat:
        if a @ b < 0:
            b = -b
        q = a + (b - a) * u
        return q / (np.linalg.norm(q) + 1e-30)
    return a + (b - a) * u


def load_clip(path, bones, fps, once, t0=None, t1=None):
    """The clip's poses at fps, over [t0, t1] seconds of it when given (a montage
    section), else the whole of it."""
    d = json.load(open(path))
    by = {t["bone"]: t for t in d["tracks"]}
    length = float(d["length"])
    t0 = 0.0 if t0 is None else t0
    t1 = length if t1 is None else min(t1, length)
    nframe = max(1, int(round((t1 - t0) * fps)) + (1 if once else 0))
    missing = [b["name"] for b in bones if b["name"] not in by]
    frames = []
    for f in range(nframe):
        t01 = (t0 + f / fps) / length if length > 0 else 0.0
        local = []
        for b in bones:
            tr = by.get(b["name"])
            q = sample_track(tr["q"], t01, True) if tr and tr["q"] else b["r"]
            t = sample_track(tr["p"], t01, False) if tr and tr["p"] else b["t"]
            s = sample_track(tr["sc"], t01, False) if tr and tr["sc"] else b["s"]
            local.append(trs(q, t, s))
        frames.append(model_space(bones, local))
    return d, nframe, frames, missing


# ---------------------------------------------------------------------------------- #

def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--type", required=True, help="the engine type this model draws")
    ap.add_argument("--mesh", required=True, help="the dump prefix: <prefix>.json and .bin")
    ap.add_argument("--anims", required=True, help="the directory of dumped clips")
    ap.add_argument("--clipmap", required=True, help="clip = AnimName [once], one a line")
    ap.add_argument("--texdir", action="append", default=[],
                    help="where <stem>_D/_N/_RMOE PNGs are; may be given more than once")
    ap.add_argument("--forward", default="+y",
                    help="the Unreal axis the character faces in its bind pose (default +y)")
    ap.add_argument("--height", type=float, required=True,
                    help="the character's standing height in map cells")
    ap.add_argument("--fps", type=float, default=30.0)
    ap.add_argument("--texmax", type=int, default=1024)
    ap.add_argument("--preview", help="write a PNG of the bind pose and every clip")
    ap.add_argument("--lods", default="",
                    help="LEVELS OF DETAIL as FRACTION@SCREEN,...: each keeps that fraction of "
                         "the triangles and is drawn below that Unreal ScreenSize, e.g. "
                         "0.5@0.1,0.25@0.06 (the Tempest Rising GDF rifleman's own)")
    ap.add_argument("--tweaks", default=None,
                    help="PREFIX: each clip NAME is tweaked by PREFIX_NAME.json when it "
                         "exists (tools/art/anim_tweak.py; the anim lab writes these)")
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)

    meta = json.load(open(a.mesh + ".json"))
    bones = meta["bones"]
    if len(bones) > F.MAX_BONES:
        fail("%d bones, the renderer's palette holds %d" % (len(bones), F.MAX_BONES))
    nv, ni = meta["nverts"], meta["nidx"]
    raw = open(a.mesh + ".bin", "rb").read()
    o = 0
    pos = np.frombuffer(raw, np.float32, nv * 3, o).reshape(nv, 3).astype(np.float64); o += nv * 12
    nrm = np.frombuffer(raw, np.float32, nv * 3, o).reshape(nv, 3).astype(np.float64); o += nv * 12
    uv = np.frombuffer(raw, np.float32, nv * 2, o).reshape(nv, 2).astype(np.float64); o += nv * 8
    inf = np.frombuffer(raw, np.dtype([("b", "<u2", 8), ("w", "<f4", 8)]), nv, o); o += nv * 48
    ib, iw = inf["b"], inf["w"]                # interleaved per vertex: 8 bones, 8 weights
    idx = np.frombuffer(raw, np.uint32, ni, o).astype(np.int64); o += ni * 4
    if o != len(raw):
        fail("%s.bin is %d bytes and the header describes %d" % (a.mesh, len(raw), o))

    # ---- axes, scale and seat: Unreal component space -> pack units --------------------
    B = axis_basis(a.forward)
    pb = pos @ B.T
    ground = pb[:, 1].min()
    tall = pb[:, 1].max() - ground
    k = a.height * CELL / tall
    cx = 0.5 * (pb[:, 0].min() + pb[:, 0].max())
    cz = 0.5 * (pb[:, 2].min() + pb[:, 2].max())
    P = np.eye(4)
    P[:3, :3] = B * k
    P[:3, 3] = -np.array([cx, ground, cz]) * k
    Q = np.eye(4)
    Q[:3, :3] = np.linalg.inv(B)            # the mirror, taken back out in bone space
    pack_pts = (P @ np.c_[pos, np.ones(nv)].T).T[:, :3]
    pack_nrm = nrm @ B.T
    pack_nrm /= np.linalg.norm(pack_nrm, axis=1, keepdims=True) + 1e-12
    uv_gl = np.c_[uv[:, 0], 1.0 - uv[:, 1]]  # Unreal v runs down the image; GL's runs up

    # ---- winding: the mirror should have made Unreal's fronts GL's fronts ---------------
    tris = idx.reshape(-1, 3)
    fn = np.cross(pack_pts[tris[:, 1]] - pack_pts[tris[:, 0]],
                  pack_pts[tris[:, 2]] - pack_pts[tris[:, 0]])
    agree = np.einsum("ij,ij->i", fn, pack_nrm[tris].sum(axis=1))
    outward = float((agree > 0).mean())
    if outward < 0.5:
        tris = tris[:, [0, 2, 1]]
        outward = 1.0 - outward
        print("UE|note|winding reversed to face outward")
    if outward < 0.9:
        fail("only %.0f%% of faces agree with their vertex normals; the mesh is not "
             "consistently wound" % (outward * 100))

    # ---- influences: the dump's eight, heaviest four kept ------------------------------
    skin_b = np.zeros((nv, 4), np.uint8)
    skin_w = np.zeros((nv, 4), np.uint8)
    dropped = 0
    for v in range(nv):
        lst = sorted(((float(iw[v, j]), int(ib[v, j])) for j in range(8) if iw[v, j] > 0),
                     reverse=True)
        if not lst:
            fail("vertex %d is bound to no bone" % v)
        dropped += max(0, len(lst) - F.MAX_INFLUENCES)
        lst = lst[:F.MAX_INFLUENCES]
        tot = sum(w for w, _ in lst)
        qw = [int(round(255.0 * w / tot)) for w, _ in lst]
        qw[0] += 255 - sum(qw)
        for j, ((_, bi), w8) in enumerate(zip(lst, qw)):
            skin_b[v, j], skin_w[v, j] = bi, w8

    # ---- bones: the bind in model space, and its inverse in pack space ----------------
    bind_local = [trs(b["r"], b["t"], b["s"]) for b in bones]
    bind = model_space(bones, bind_local)
    Pinv = np.linalg.inv(P)
    bone_ib = [np.linalg.inv(P @ W @ Q) for W in bind]
    # proof the bind is the pose the vertices were saved in: the skeleton sits inside them
    head = (P @ bind[[b["name"] for b in bones].index("Head")])[:3, 3] if any(
        b["name"] == "Head" for b in bones) else None

    # ---- materials and textures ----------------------------------------------------------
    texs = [(4, 4, bytes([128, 128, 255, 255]) * 16),
            (4, 4, bytes([255, int(round(255 * 0.8)), 0, 255]) * 16)]
    FLAT_NORMAL, NEUTRAL_ORM = 0, 1
    mats, mat_report = [], []
    for m in meta["materials"]:
        stem = os.path.basename((m["path"] or m["slot"]).split(".")[0])   # e.g. M_GDI_Weapon
        tex_stem = "T_" + stem[2:] if stem.startswith("M_") else stem
        if m.get("tex"):
            tex_stem = m["tex"]              # named outright (ue_weapon_swap.py writes this)
        base = norm = orm = mask = cloth = logo_g = logo_n = None
        for td in a.texdir:
            for cand in (tex_stem, tex_stem.replace("_Weapon", "_Rifleman_Rifle")):
                p = os.path.join(td, cand + "_D.png")
                if os.path.exists(p):
                    base = p
                    np_ = os.path.join(td, cand + "_N.png")
                    rp = os.path.join(td, cand + "_RMOE.png")
                    cp = os.path.join(td, cand + "_C.png")
                    norm = np_ if os.path.exists(np_) else None
                    orm = rp if os.path.exists(rp) else None
                    mask = cp if os.path.exists(cp) else None
                    clp = os.path.join(td, cand + "_Cloth.png")
                    cloth = clp if os.path.exists(clp) else None
                    # THE SHOULDER EMBLEM (tools/art/emblem_masks.py): the colour map with
                    # the eagle painted out, and a stencil of each side's emblem
                    lg = os.path.join(td, cand + "_LogoGDI.png")
                    ln = os.path.join(td, cand + "_LogoNod.png")
                    if os.path.exists(lg) and os.path.exists(ln):
                        logo_g, logo_n = lg, ln
                        bl = os.path.join(td, cand + "_Blank.png")
                        if os.path.exists(bl):
                            base = bl
                    break
            if base:
                break
        if base:
            img = base_with_mask(base, mask, a.texmax)
            w, h = img.size
            texs.append((w, h, np.asarray(img, np.uint8)[::-1].tobytes()))
            bi_ = len(texs) - 1
        else:
            # NO COLOUR MAP: a flat mid grey, loudly. Never a silent substitute.
            texs.append((4, 4, bytes([150, 150, 150, 0]) * 16))
            bi_ = len(texs) - 1
        ni_, oi_ = FLAT_NORMAL, NEUTRAL_ORM
        if norm:
            w, h, data = load_normal_dx(norm, a.texmax, logo_n if orm else None)
            texs.append((w, h, data))
            ni_ = len(texs) - 1
        if orm:
            w, h, data = load_rmoe(orm, a.texmax, cloth or logo_g)
            texs.append((w, h, data))
            oi_ = len(texs) - 1
        flags = 0 if mask else F.MAT_NO_PLAYER
        if cloth and orm:
            flags |= F.MAT_CLOTH
        elif logo_g and orm and norm:
            flags |= F.MAT_EMBLEM
        mats.append((bi_, ni_, oi_, flags, m["slot"]))
        mat_report.append((m["slot"], stem, base, norm, orm, mask,
                           cloth if (cloth and orm) else ("emblems" if flags & F.MAT_EMBLEM else None)))

    # submeshes from the sections, each one material, all in part 0
    subs = []
    for s in meta["sections"]:
        subs.append((0, s["mat"], s["first"], s["faces"] * 3))
    flat_idx = tris.reshape(-1)

    # ---- levels of detail (tools/art/mesh_lod.py) -----------------------------------------
    # Each threshold is written as Unreal's ScreenSize, unconverted: the renderer measures
    # the same thing (the bind pose's bounding sphere, projected diameter over screen
    # height), so a switch lands where it would in Tempest Rising.
    lods = []
    if a.lods:
        import mesh_lod
        specs = [tuple(float(x) for x in sp.split("@")) for sp in a.lods.split(",")]
        tri_mat = np.zeros(len(tris), np.int64)
        for sct in meta["sections"]:
            tri_mat[sct["first"] // 3:sct["first"] // 3 + sct["faces"]] = sct["mat"]
        levels = mesh_lod.simplify_chain(pack_pts, pack_nrm, tris, skin_b, skin_w,
                                         [fr for fr, _ in specs],
                                         log=lambda m: print("UE|" + m))
        # VERTEX ORDER: the coarsest level's vertices first, then each finer level's
        # additions, then the rest. The levels are nested, so every level uses a prefix
        # of the array and the renderer skins only that prefix.
        rank = np.zeros(nv, np.int64)
        for k, (lt, _) in enumerate(levels):
            used = np.zeros(nv, bool)
            used[lt.reshape(-1)] = True
            if k and np.any(used & (rank < k)):
                fail("level %d uses a vertex level %d dropped; the levels are not nested"
                     % (k + 1, k))
            rank[used] = k + 1
        order = np.lexsort((np.arange(nv), -rank))
        remap = np.empty(nv, np.int64)
        remap[order] = np.arange(nv)
        pack_pts, pack_nrm, uv_gl = pack_pts[order], pack_nrm[order], uv_gl[order]
        skin_b, skin_w = skin_b[order], skin_w[order]
        tris = remap[tris]
        flat_idx = tris.reshape(-1)
        for k, ((lt, ids), (fr, screen)) in enumerate(zip(levels, specs)):
            lt = remap[lt]
            mt = tri_mat[ids]
            srt = np.argsort(mt, kind="stable")
            lt, mt = lt[srt], mt[srt]
            lsubs = []
            for m_ in np.unique(mt):
                r = np.nonzero(mt == m_)[0]
                lsubs.append((0, int(m_), int(r[0]) * 3, len(r) * 3))
            nvp = int((rank >= k + 1).sum())
            if lt.max() >= nvp:
                fail("level %d reaches past its vertex prefix" % (k + 1))
            lods.append((screen, nvp, lt.reshape(-1), lsubs, fr, screen))
            print("UE|lod|%d|%d triangles|%d vertices|below Unreal ScreenSize %g"
                  % (k + 1, len(lt), nvp, screen))

    # ---- clips -------------------------------------------------------------------------
    clips = []
    for line in open(a.clipmap):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        name, rest = [x.strip() for x in line.split("=", 1)]
        parts = rest.split()
        anim, once = parts[0], "once" in parts[1:]
        t0 = t1 = None
        for p_ in parts[1:]:
            if p_.startswith("section="):
                t0, t1 = (float(x) for x in p_[8:].split(":"))
        path = os.path.join(a.anims, anim + ".json")
        if not os.path.exists(path):
            fail("clip %s: %s is not in %s" % (name, anim, a.anims))
        if a.tweaks:
            import anim_tweak as AT
            tpath = "%s_%s.json" % (a.tweaks, name)
            tweak = AT.load_tweak(tpath)
            d, nframe, local = AT.sample_clip(path, bones, a.fps, once, t0, t1, tweak)
            W = AT.model_space_frames(bones, local, trs, model_space)
            missing = [b["name"] for b in bones if b["name"] not in {t["bone"] for t in d["tracks"]}]
            if tweak:
                print("UE|tweak|%s|%s" % (name, os.path.relpath(tpath)))
        else:
            d, nframe, W, missing = load_clip(path, bones, a.fps, once, t0, t1)
        if missing:
            print("UE|note|clip %s (%s) does not animate %s; they hold the bind pose"
                  % (name, anim, ", ".join(missing)))
        frames, feet = [], []
        for Wf in W:
            pose = [F.decompose(P @ Wb @ Q) for Wb in Wf]
            if frames:
                for j in range(len(pose)):
                    if np.dot(pose[j][0], frames[-1][j][0]) < 0:
                        pose[j] = (-pose[j][0], pose[j][1], pose[j][2])
            frames.append(pose)
            feet.append({b["name"]: (P @ Wb)[:3, 3] for b, Wb in zip(bones, Wf)
                         if "foot" in b["name"].lower()})
        stride = 0.0
        if feet and feet[0]:
            for f in range(nframe - 1 if once else nframe):
                a0, a1 = feet[f], feet[(f + 1) % nframe]
                low = min(a0, key=lambda b: a0[b][1])
                stride += abs(a1[low][2] - a0[low][2])
        flags = 0 if once else F.CLIP_LOOP
        if not once and stride > 0.25 * a.height * CELL and name in LOCOMOTION:
            flags |= F.CLIP_LOCOMOTION
        else:
            stride = 0.0
        clips.append((name, a.fps, stride, flags, frames))
        print("UE|clip|%s|%s|%d frames at %g fps|%.2f s|%s%s|keys %d over %.3f s" % (
            name, anim, len(frames), a.fps, len(frames) / a.fps,
            "loop" if flags & F.CLIP_LOOP else "once",
            ("|stride %.0f units (%.3f cells)" % (stride, stride / CELL))
            if flags & F.CLIP_LOCOMOTION else "",
            max(len(t["q"]) for t in d["tracks"]), float(d["length"])))

    # ---- write (the same layout fbx_c3m.py writes) --------------------------------------
    bb = list(pack_pts.min(0)) + list(pack_pts.max(0))
    out = bytearray()
    out += F.MAGIC
    out += struct.pack("<8I", 3 if lods else F.FORMAT_VERSION, nv, len(flat_idx), 1, len(subs), len(mats),
                       len(texs), len(bones))
    tn = a.type.encode("ascii")[:8]
    out += tn + b"\0" * (8 - len(tn))
    out += struct.pack("<6f", *bb)
    for v in range(nv):
        p, n, t = pack_pts[v], pack_nrm[v], uv_gl[v]
        out += struct.pack("<8f", p[0], p[1], p[2], n[0], n[1], n[2], t[0], t[1])
    out += flat_idx.astype("<u4").tobytes()
    out += struct.pack("<I3f", F.ROLE_STATIC, 0.0, 0.0, 0.0) + b"body" + b"\0" * 12
    for s in subs:
        out += struct.pack("<4I", *s)
    for base, norm, orm, flags, _ in mats:
        out += struct.pack("<3iI", base, norm, orm, flags)
    for w, h, data in texs:
        out += struct.pack("<3I", w, h, len(data)) + data
    out += struct.pack("<4I", len(clips), 0, 0, 0)
    for v in range(nv):
        out += bytes(skin_b[v]) + bytes(skin_w[v])
    for b, ibm in zip(bones, bone_ib):
        nb = b["name"].encode("ascii", "replace")[:31]
        out += nb + b"\0" * (32 - len(nb))
        out += struct.pack("<12f", *ibm[:3, :].reshape(-1))
    for name, fps, stride, flags, frames in clips:
        nn = name.encode("ascii")[:15]
        out += nn + b"\0" * (16 - len(nn))
        out += struct.pack("<I2fI", len(frames), fps, stride, flags)
        for pose in frames:
            for q, t, s in pose:
                out += struct.pack("<8f", q[0], q[1], q[2], q[3], t[0], t[1], t[2], s)
    # ---- the version 3 tail: coarser levels, each a vertex prefix and its own indices ----
    if lods:
        out += struct.pack("<I", len(lods))
        for screen, nvp, lidx, lsubs, _, _ in lods:
            out += struct.pack("<f3I", screen, nvp, len(lidx), len(lsubs))
            for sb in lsubs:
                out += struct.pack("<4I", *sb)
            out += lidx.astype("<u4").tobytes()
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "wb") as f:
        f.write(bytes(out))

    # ---- report -------------------------------------------------------------------------
    print("UE|type|%s" % a.type)
    print("UE|skin|%d bones|%d influences dropped past %d|%.1f%% of faces outward"
          % (len(bones), dropped, F.MAX_INFLUENCES, outward * 100))
    print("UE|verts|%d|tris|%d|mats|%d" % (nv, len(flat_idx) // 3, len(mats)))
    for slot, stem, base, norm, orm, mask, cloth in mat_report:
        print("UE|mat|%s|%s|colour %s|normal %s|surface %s|team mask %s|cloth or emblem %s" % (
            slot, stem, os.path.basename(base) if base else "MISSING (flat grey)",
            os.path.basename(norm) if norm else "flat", os.path.basename(orm) if orm else "neutral",
            os.path.basename(mask) if mask else "none", os.path.basename(cloth) if cloth else "none"))
    if head is not None:
        print("UE|head bone at|%.0f %.0f %.0f|model top %.0f" % (head[0], head[1], head[2], bb[4]))
    print("UE|bbox|x %.1f..%.1f|y %.1f..%.1f|z %.1f..%.1f" % tuple(bb[i] for i in (0, 3, 1, 4, 2, 5)))
    print("UE|cells|w %.3f|h %.3f|d %.3f" % ((bb[3] - bb[0]) / CELL, (bb[4] - bb[1]) / CELL,
                                            (bb[5] - bb[2]) / CELL))
    if not any(r[5] for r in mat_report):
        print("UE|note|no player-colour mask: every house draws this model identically")
    print("UE|wrote|%s|%d bytes" % (a.out, len(out)))

    if a.preview:
        verts = [(v, pack_nrm[v], uv_gl[v]) for v in range(nv)]
        F.preview(a.preview, pack_pts, verts, list(flat_idx), subs, mats, texs,
                  skin_b, skin_w, bone_ib, clips)
        print("UE|preview|%s" % a.preview)
        if lods:
            lp = a.preview[:-4] + "_lods.png" if a.preview.endswith(".png") else a.preview + "_lods.png"
            lod_preview(lp, pack_pts, uv_gl, texs, mats, flat_idx, subs, lods)
            print("UE|preview|%s" % lp)
    return 0


def lod_preview(path, pts, uv, texs, mats, idx0, subs0, lods):
    """Every level from the front and the side, textured, then untextured so the
    silhouette and the shading of the simplified surface can be judged on their own."""
    texarr = [np.frombuffer(d, np.uint8).reshape(h, w, 4).astype(np.float32) / 255.0
              for w, h, d in texs]
    grey = [np.full((4, 4, 4), 0.7, np.float32)]
    size = 320
    hgt = pts[:, 1].max()
    scale = 0.85 * size / hgt
    centre = (0.0, hgt * 0.5)
    levels = [(idx0, subs0)] + [(li, ls) for _, _, li, ls, _, _ in lods]
    rows = []
    for tex in (True, False):
        row = []
        for li, ls in levels:
            tris = np.asarray(li, np.int64).reshape(-1, 3)
            tmat = np.zeros(len(tris), np.int64)
            for _, m_, i0, n_ in ls:
                tmat[i0 // 3:(i0 + n_) // 3] = mats[m_][0] if tex else 0
            for yaw in (180, 270):
                row.append(F.raster(pts, tris, uv, tmat, texarr if tex else grey, size, yaw,
                                    scale, centre))
        rows.append(row)
    sheet = Image.new("RGB", (size * len(rows[0]), size * len(rows)))
    for r, row in enumerate(rows):
        for c, im in enumerate(row):
            sheet.paste(im, (c * size, r * size))
    sheet.save(path)


# Clips the renderer advances by ground covered rather than by time. Any other clip with
# feet that happen to move (a death, a lie-down) plays on the clock.
LOCOMOTION = ("run", "trot", "walk", "sprint", "crawl")


def _square(path, size):
    """The map at most size a side, square. Read as RGB: Tempest Rising's normal and surface
    maps carry an alpha (the surface map's is emissive, zero almost everywhere), and Pillow
    resamples RGBA premultiplied, so a zero alpha would come back as zero colour -- every
    roughness, metal and occlusion texel lost. Nothing downstream reads their alpha."""
    img = Image.open(path).convert("RGB")
    w, h = img.size
    side = min(size, w, h)
    if (w, h) != (side, side):
        img = img.resize((side, side), Image.LANCZOS)
    return side, img


def base_with_mask(base_path, mask_path, size):
    """The colour map as RGBA, top row first, at most size a side. Alpha is the TEAM MASK
    (the renderer's player colour multiplies the albedo where it is set), 0 without one.

    Tempest Rising's _C masks carry the team area in one channel or all three, so the
    STRONGEST channel is the mask. Under it the paint is NEUTRALISED: replaced by its own
    brightness, grey, lifted so the masked area averages 0.8, and blended by the mask's
    own softness. The paint there is one faction's colour already (this rifleman's plates
    are GDI tan), and a multiply over it would give every other faction a muddy mix of
    two colours; over grey the house colour comes through as itself, with every scratch
    and streak of the original kept."""
    img = Image.open(base_path).convert("RGB")
    w0, h0 = img.size
    side = min(size, w0, h0)
    # scaled exactly as fbx_c3m.load_texture scales: a box average for a whole multiple
    a0 = np.asarray(img, np.float32) / 255.0
    if w0 % side == 0 and h0 % side == 0:
        rgb = a0.reshape(side, h0 // side, side, w0 // side, 3).mean(axis=(1, 3))
    else:
        rgb = np.asarray(img.resize((side, side), Image.LANCZOS), np.float32) / 255.0
    alpha = np.zeros((side, side), np.float32)
    if mask_path:
        m = Image.open(mask_path).convert("RGB")
        if m.size != (side, side):
            m = m.resize((side, side), Image.LANCZOS)
        alpha = np.asarray(m, np.float32).max(axis=2) / 255.0
        lum = rgb @ np.array([0.2126, 0.7152, 0.0722], np.float32)
        sel = alpha > 0.5
        mean = float(lum[sel].mean()) if sel.any() else 0.5
        grey = np.clip(lum * (0.8 / max(mean, 1e-3)), 0.0, 1.0)[..., None]
        rgb = rgb + (grey - rgb) * alpha[..., None]
    out = np.zeros((side, side, 4), np.uint8)
    out[..., :3] = np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    out[..., 3] = np.clip(alpha * 255.0 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def _alpha(path, side):
    c = Image.open(path).convert("L")
    if c.size != (side, side):
        c = c.resize((side, side), Image.LANCZOS)
    return np.asarray(c, np.uint8)


def load_normal_dx(path, size, alpha=None):
    """Unreal normal maps are DirectX style, green pointing down the image; the renderer's
    tangent frame is built with v running up, so green is flipped. Bottom row first. Alpha
    is the Nod emblem's stencil on an emblem material (MAT_EMBLEM), 255 otherwise."""
    side, img = _square(path, size)
    a = np.asarray(img.convert("RGB"), np.uint8).copy()
    a[:, :, 1] = 255 - a[:, :, 1]
    out = np.zeros((side, side, 4), np.uint8)
    out[:, :, :3] = a
    out[:, :, 3] = _alpha(alpha, side) if alpha else 255
    return side, side, out[::-1].tobytes()


def load_rmoe(path, size, alpha=None):
    """Tempest Rising packs roughness, metal, occlusion, emissive; the renderer reads
    occlusion, roughness, metal from r, g, b. Alpha is the CLOTH MASK when there is one
    (tools/art/cloth_mask.py makes it; the material is then flagged MAT_CLOTH, which is
    what tells the renderer the alpha means anything), or the GDI emblem's stencil on an
    emblem material (MAT_EMBLEM), 255 otherwise."""
    side, img = _square(path, size)
    a = np.asarray(img, np.uint8)
    out = np.zeros((side, side, 4), np.uint8)
    out[:, :, 0] = a[:, :, 2]
    out[:, :, 1] = a[:, :, 0]
    out[:, :, 2] = a[:, :, 1]
    out[:, :, 3] = 255
    if alpha:
        out[:, :, 3] = _alpha(alpha, side)
    return side, side, out[::-1].tobytes()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
