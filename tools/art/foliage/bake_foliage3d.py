#!/usr/bin/env python3
"""Bake the scattered bushes and grass clumps into foliage3d.pack.

WHAT GOES IN. Five prefabs from the Unity Asset Store pack "Realistic Grass and Bush
Pack1" (Pixel Games): two bushes (Bush, Bush2) and three grass clumps (grass1_7,
grass2, grass3). Their meshes live in Unity's own serialisation, so Unity reads them
and OpenCNC3DFoliageExport.cs (beside this file, in Editor/) writes them out as JSON. This
script reads that JSON and the textures the materials name, and writes a pack in the
leafy trees' own format, TREE3D4, so the renderer reads it with the trees' own reader
and draws it with the trees' own shader: one sun, one wind, one set of shadows.

WHAT IS KEPT AS AUTHORED: the geometry of one LOD per prefab, its UVs and normals, and
the textures. The bushes ship LOD0..LOD2; LOD2 is kept, because a bush here stands about
half a cell tall, which is twenty to thirty screen pixels at the default zoom, and LOD0's
extra leaves cannot be seen at that size but are paid for on every one of hundreds.

WHAT IS ADDED, because the renderer needs it and the asset has no way to carry it: the
wind's sway weight (nothing at the root, the square of the height up to the top), the
occlusion and thickness the tree shader reads (baked by the trees' own ao_bake from a
sample of the model's surface), and a material flag per vertex, bark or foliage.

UNITY'S V RUNS UP, OURS RUNS DOWN. Unity puts UV (0,0) at the bottom of an image and this
renderer uploads an image top row first, so every V is flipped here, once.

Usage:
    python3 tools/art/foliage/bake_foliage3d.py --export <json dir> \
        --project <unity project> --out data/foliage/foliage3d.pack

The JSON comes from running the export (see the header of Editor/OpenCNC3DFoliageExport.cs)
on a throwaway Unity project with the pack imported; <unity project> is that project,
because the JSON names textures by their path inside it.
"""
import argparse, json, os, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'game'))
import bake_tree3d as bt  # noqa: E402  (its ao_bake, surface_cloud and pack writer)

# prefab JSON name -> (code in the pack, which LOD, is it a bush)
# EVERY LOD OF BOTH BUSHES, each with its OWN textures. The pack's LODs are not one
# model at three triangle counts over one atlas: each carries its own leaf atlas, laid
# out differently (LOD2's differs from a downscale of LOD0's by 45 to 55 levels a texel)
# and at a different size, 1024, 512 and 256. The first bake used LOD2 for both bushes
# and scaled its 256 atlas UP to 512, which is exactly the blurred bush with its big
# cards showing that play found. The renderer now picks the level from the F5 bush
# detail dial.
PREFABS = [
    ('Bush',     'BUSH1L0', 0, True),
    ('Bush',     'BUSH1L1', 1, True),
    ('Bush',     'BUSH1L2', 2, True),
    ('Bush2',    'BUSH2L0', 0, True),
    ('Bush2',    'BUSH2L1', 1, True),
    ('Bush2',    'BUSH2L2', 2, True),
    ('grass1_7', 'GRASS1', -1, False),
    ('grass2',   'GRASS2', -1, False),
    ('grass3',   'GRASS3', -1, False),
]

# Sheets are baked at the size the artist made them, capped here. Never scaled up.
LEAF_MAX = 1024
BARK_MAX = 512  # the bark is under the leaves and barely seen

# THE CUT THE RENDERER TESTS AGAINST. The tree shader discards foliage below 0.34 and the
# mip chain holds coverage at 0.35, so each material's own Unity cutoff is carried by
# rescaling its alpha to put that cutoff at 0.34, rather than by a per-model uniform the
# pack format has no field for. Bush2's leaves are cut at 0.7 in Unity; cut at 0.34 they
# came out as fat blobs with their soft fringes showing.
RENDER_CUT = 0.34


def load_rgba(project, rel, px, flip=True):
    im = Image.open(os.path.join(project, rel)).convert('RGBA')
    if im.size != (px, px):
        im = im.resize((px, px), Image.LANCZOS)
    a = np.asarray(im).copy()
    return a


def native_px(project, rel, cap):
    w, h = Image.open(os.path.join(project, rel)).size
    return min(max(w, h), cap)


def pick_renderer(d, lod):
    rs = d['renderers']
    if lod < 0:
        return rs[0]
    for r in rs:
        if r['lod'] == lod:
            return r
    raise SystemExit('%s has no LOD%d' % (d['prefab'], lod))


def build(d, lod, is_bush, project):
    r = pick_renderer(d, lod)
    P = np.array(r['pos'], dtype=np.float64).reshape(-1, 3)
    N = np.array(r['nrm'], dtype=np.float64).reshape(-1, 3)
    U = np.array(r['uv'], dtype=np.float64).reshape(-1, 2)
    # THE PREFAB ROOT'S OWN SCALE. The export writes each mesh in the root's space, and
    # Bush2's root is scaled to about a third, so without this it would stand three times
    # the size its author set. Only the ratio matters here, because every model is
    # normalised to its own height below, but the scale also turns the normals.
    rs = np.array(d['rootScale'], dtype=np.float64)
    P = P * rs
    # A UNITY PREFAB IS LEFT-HANDED. The game's world is right-handed about the same up
    # axis, so one horizontal axis is mirrored; without it every model is its own mirror
    # image, which a bush survives but a UV that reads text would not.
    P[:, 2] = -P[:, 2]
    N[:, 2] = -N[:, 2]
    U = U.copy()
    U[:, 1] = 1.0 - U[:, 1]

    subs = r['sub']
    tris, mats = [], []
    for s in subs:
        idx = np.array(s['idx'], dtype=np.int64).reshape(-1, 3)
        # the mirror above reverses every winding; put it back
        idx = idx[:, [0, 2, 1]]
        leaf = 1.0 if (not is_bush or s.get('mat', '') == 'leaf') else 0.0
        tris.append(idx)
        mats.append((idx, leaf, s))
    T = np.concatenate(tris, axis=0)
    MAT = np.zeros(len(P))
    for idx, leaf, _ in mats:
        MAT[idx.reshape(-1)] = leaf

    # STAND IT ON ITS OWN ORIGIN, ONE UNIT TALL. The trunk (or the clump's root) is the
    # horizontal centre of the lowest tenth of the model, so a bush whose crown leans is
    # still planted where its stem is and not where its bounding box is.
    y0 = P[:, 1].min()
    h = P[:, 1].max() - y0
    low = P[:, 1] < y0 + 0.1 * h
    cx, cz = P[low, 0].mean(), P[low, 2].mean()
    P[:, 0] -= cx; P[:, 2] -= cz; P[:, 1] -= max(y0, 0.0) if is_bush else 0.0
    P /= h
    radius = float(np.sqrt((P[:, 0] ** 2 + P[:, 2] ** 2).max()))

    Nn = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    dense = bt.surface_cloud(P, T, target=120000)
    ao_n = Nn.copy()    # the occlusion bake marches along the AUTHORED normals
    AO, TH = bt.ao_bake(P, ao_n, dense, res=64, ndirs=24, steps=18, reach=0.45)
    # A GRASS CLUMP IS A HANDFUL OF BLADES, not a canopy, and the voxel march reads the
    # tight bundle at its root as solid: it came out near black at the bottom. The clump
    # keeps a milder version of the same shape.
    if not is_bush:
        AO = 0.60 + 0.40 * AO
    ht = np.clip(P[:, 1], 0.0, 1.0)
    sway = ht ** 2

    # THE SHADING NORMALS ARE BENT, the standard foliage trick, and it is measured
    # rather than taste: drawn with their authored normals the grass clumps came out as
    # dark holes in a lit field, because a card's own normal points sideways and this
    # camera looks down on it under a high sun, so a clump got a fraction of the light
    # the ground around it did. A clump's normals lean most of the way to straight up,
    # so it is lit like the field it grows from; a bush's lean toward a dome over its
    # own crown, so it shades as one rounded mass rather than as a heap of cards. The
    # normal maps still add the leaf detail on top, in the shader.
    if is_bush:
        c = np.array([0.0, 0.45, 0.0])
        d = P - c
        d /= np.maximum(np.linalg.norm(d, axis=1, keepdims=True), 1e-9)
        Nn = 0.35 * Nn + 0.65 * d
    else:
        Nn = 0.25 * Nn + 0.75 * np.array([0.0, 1.0, 0.0])
    Nn /= np.maximum(np.linalg.norm(Nn, axis=1, keepdims=True), 1e-9)

    # the textures this model's materials name
    leafsub = next((s for _, l, s in mats if l > 0.5), None)
    barksub = next((s for _, l, s in mats if l < 0.5), None)
    lt = leafsub['allTex']
    leaf_tex = lt.get('_MainTex')
    leaf_nrm = lt.get('_BumpMap') or lt.get('_BumpSpecMap') or lt.get('_NormalMap')
    LEAF_PX = native_px(project, leaf_tex, LEAF_MAX)
    atlas = load_rgba(project, leaf_tex, LEAF_PX)
    cut = leafsub.get('cutoff')
    if cut:
        a = atlas[..., 3].astype(np.float32) * (RENDER_CUT / float(cut))
        atlas[..., 3] = np.clip(a + 0.5, 0, 255).astype(np.uint8)
    atlas_n = np.zeros_like(atlas)
    if leaf_nrm:
        atlas_n[..., :3] = load_rgba(project, leaf_nrm, LEAF_PX)[..., :3]
    else:
        atlas_n[..., :3] = (128, 128, 255)
    atlas_n[..., 3] = 110
    BARK_PX = native_px(project, barksub['allTex']['_MainTex'], BARK_MAX) if barksub else 64
    bark = np.zeros((BARK_PX, BARK_PX, 4), dtype=np.uint8)
    bark_n = np.zeros_like(bark)
    if barksub:
        bt_ = barksub['allTex']
        bark[..., :3] = load_rgba(project, bt_['_MainTex'], BARK_PX)[..., :3]
        bn = bt_.get('_BumpMap') or bt_.get('_BumpSpecMap')
        bark_n[..., :3] = load_rgba(project, bn, BARK_PX)[..., :3] if bn else (128, 128, 255)
    else:
        bark[..., :3] = (90, 74, 58)
        bark_n[..., :3] = (128, 128, 255)
    bark[..., 3] = 255
    bark_n[..., 3] = 205
    st = dict(atlas=atlas, atlas_n=atlas_n, bark=bark, bark_n=bark_n,
              aw=LEAF_PX, ah=LEAF_PX, bp=BARK_PX)
    model = dict(P=P.astype('<f4'), N=Nn.astype('<f4'), U=U.astype('<f4'),
                 T=T.astype('<u4'), sway=sway, AO=AO, TH=TH, MAT=MAT, radius=radius)
    return model, st, h


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--export', required=True, help='directory of OpenCNC3DFoliageExport JSON')
    ap.add_argument('--project', required=True, help='the Unity project the JSON came from')
    ap.add_argument('--out', required=True)
    ap.add_argument('--preview', help='write a contact sheet of every model to this PNG')
    a = ap.parse_args()

    models, sets, mapping = [], [], {}
    for name, code, lod, is_bush in PREFABS:
        d = json.load(open(os.path.join(a.export, name + '.json')))
        m, st, h = build(d, lod, is_bush, a.project)
        m['set'] = len(sets)
        sets.append(st)
        mapping[code] = len(models)
        models.append(m)
        print('%-8s %-7s %5d verts %5d tris  authored height %.2f  radius %.2f  '
              'ao %.2f..%.2f  sheets %d/%d' % (name, code, len(m['P']), len(m['T']), h,
                                            m['radius'], m['AO'].min(), m['AO'].max(),
                                            st['aw'], st['bp']))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    bt.write_pack3(a.out, models, sets, mapping)
    if a.preview:
        preview(models, sets, a.preview)


def preview(models, sets, path):
    """Each model from the side, flat-shaded by its baked occlusion over its own texture,
    so the bake can be checked without the game."""
    S = 320
    sheet = Image.new('RGB', (S * len(models), S), (120, 130, 150))
    for i, (m, st) in enumerate(zip(models, sets)):
        img = np.zeros((S, S, 3), dtype=np.float32) + (120, 130, 150)
        zb = np.full((S, S), -1e9)
        P, T, U = m['P'], m['T'], m['U']
        for tri in T:
            p = P[tri]
            xs = (p[:, 0] / 1.2 * 0.5 + 0.5) * S
            ys = (1.0 - (p[:, 1] * 0.9 + 0.05)) * S
            x0, x1 = int(max(xs.min(), 0)), int(min(xs.max() + 1, S))
            y0, y1 = int(max(ys.min(), 0)), int(min(ys.max() + 1, S))
            if x1 <= x0 or y1 <= y0:
                continue
            (ax, ay), (bx, by), (cx, cy) = zip(xs, ys)
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-9:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
            w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
            w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
            w2 = 1 - w0 - w1
            ins = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not ins.any():
                continue
            uv = w0[..., None] * U[tri[0]] + w1[..., None] * U[tri[1]] + w2[..., None] * U[tri[2]]
            leaf = m['MAT'][tri[0]] > 0.5
            tex = st['atlas'] if leaf else st['bark']
            tw = tex.shape[0]
            tu = (np.mod(uv[..., 0], 1.0) * (tw - 1)).astype(int)
            tv = (np.mod(uv[..., 1], 1.0) * (tw - 1)).astype(int)
            c = tex[tv, tu].astype(np.float32)
            ok = ins & ((c[..., 3] > 0.35 * 255) | (not leaf))
            ao = (w0 * m['AO'][tri[0]] + w1 * m['AO'][tri[1]] + w2 * m['AO'][tri[2]])
            z = w0 * p[0, 2] + w1 * p[1, 2] + w2 * p[2, 2]
            sub = zb[y0:y1, x0:x1]
            ok &= z > sub
            sub[ok] = z[ok]
            img[y0:y1, x0:x1][ok] = c[..., :3][ok] * (0.35 + 0.65 * ao[ok, None])
        sheet.paste(Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)), (i * S, 0))
    sheet.save(path)
    print('preview:', path)


if __name__ == '__main__':
    main()
