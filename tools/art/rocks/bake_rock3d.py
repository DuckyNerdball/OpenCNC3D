#!/usr/bin/env python3
"""Bake the experimental art's rocks into rock3d.pack.

WHAT GOES IN. The Unity Asset Store package "Rocks and Stones" (MSGDI): fifteen stones,
ten boulders (Stone1..Stone10) and five small detail stones (Stone11..Stone15), each in
three colourings (Prefabs/TextureSet1..3). Only the full prefabs are used; the "A" and
"B" prefabs beside each are the same stone at lower detail over a shared low-resolution
texture, and a rock here is small enough on screen that the full mesh (50 to 300
triangles) costs nothing.

Unity reads the prefabs, as it does for the foliage: tools/art/foliage/Editor/
OpenCNC3DFoliageExport.cs, run once per TextureSet folder (see data/rocks/README.md). This
script reads that JSON and writes the leafy trees' own TREE3D4 format, so the renderer
loads it with the trees' reader and lights it with the trees' shader, as BARK: an opaque
surface with a colour sheet and a normal sheet, no alpha cut and no sway.

WHAT IS BAKED:
  - the mesh centred on its bounding box and scaled so its longest side is one, which
    is what the renderer's size dial and its physics both measure
  - the albedo multiplied by the pack's own ambient occlusion map, at 256 texels
  - the normal map at 256 texels
  - a code per model, RK<stone><A|B|C>, the letter being the colouring

UNITY'S V RUNS UP AND ITS HAND IS LEFT; both are flipped exactly as the foliage baker
flips them.

THE PAINTED STONES (tools/art/rocks/build_stones.py) are the angular hand-painted
replacement, one pack per theater, which the game reads in place of this pack when the
play folder has them, and which every build carries by default (ROCK_STONES_DEFAULT in
game/make-build.sh). This pack stays committed and shipped, for a theater without a
painted pack and for the flag turned back to unity.

Usage:
    python3 tools/art/rocks/bake_rock3d.py --export <json dir 1> <json dir 2> <json dir 3> \
        --project <unity project> --out data/rocks/rock3d.pack [--preview x.png]
"""
import argparse, json, os, re, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'game'))
import bake_tree3d as bt  # noqa: E402

SHEET = 256


def load(project, rel, px, mode='RGB'):
    im = Image.open(os.path.join(project, rel)).convert(mode)
    if im.size != (px, px):
        im = im.resize((px, px), Image.LANCZOS)
    return np.asarray(im).astype(np.float32)


def build(d, project):
    r = d['renderers'][0]
    P = np.array(r['pos'], dtype=np.float64).reshape(-1, 3) * np.array(d['rootScale'])
    N = np.array(r['nrm'], dtype=np.float64).reshape(-1, 3)
    U = np.array(r['uv'], dtype=np.float64).reshape(-1, 2).copy()
    P[:, 2] = -P[:, 2]
    N[:, 2] = -N[:, 2]
    U[:, 1] = 1.0 - U[:, 1]
    T = np.concatenate([np.array(s['idx'], dtype=np.int64).reshape(-1, 3)[:, [0, 2, 1]]
                        for s in r['sub']], axis=0)
    lo, hi = P.min(0), P.max(0)
    P -= 0.5 * (lo + hi)
    ext = float((hi - lo).max())
    P /= ext
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    tex = r['sub'][0]['allTex']
    alb = load(project, tex['_MainTex'], SHEET, 'RGB')
    if '_OcclusionMap' in tex:
        ao = load(project, tex['_OcclusionMap'], SHEET, 'L')[..., None] / 255.0
        alb = alb * (0.35 + 0.65 * ao)
    nrm = load(project, tex['_BumpMap'], SHEET, 'RGB') if '_BumpMap' in tex else \
        np.zeros((SHEET, SHEET, 3), np.float32) + (128, 128, 255)
    bark = np.zeros((SHEET, SHEET, 4), np.uint8)
    bark[..., :3] = np.clip(alb + 0.5, 0, 255).astype(np.uint8)
    bark[..., 3] = 255
    bark_n = np.zeros((SHEET, SHEET, 4), np.uint8)
    bark_n[..., :3] = np.clip(nrm + 0.5, 0, 255).astype(np.uint8)
    bark_n[..., 3] = 200      # rough: stone takes almost no specular
    dummy = np.zeros((4, 4, 4), np.uint8) + 128
    st = dict(atlas=dummy, atlas_n=dummy, bark=bark, bark_n=bark_n, aw=4, ah=4, bp=SHEET)
    n = len(P)
    model = dict(P=P.astype('<f4'), N=N.astype('<f4'), U=U.astype('<f4'),
                 T=T.astype('<u4'), sway=np.zeros(n), AO=np.ones(n), TH=np.ones(n),
                 MAT=np.zeros(n), radius=float(np.sqrt((P[:, 0] ** 2 + P[:, 2] ** 2).max())))
    return model, st, ext


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--export', nargs='+', required=True)
    ap.add_argument('--project', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--preview')
    a = ap.parse_args()
    models, sets, mapping = [], [], {}
    for ti, folder in enumerate(a.export):
        for f in sorted(os.listdir(folder)):
            m = re.match(r'^Stone(\d+)(?:Detail)?Texture(\d)\.json$', f)
            if not m:
                continue            # the A and B prefabs: lower detail, shared texture
            stone, tset = int(m.group(1)), int(m.group(2))
            d = json.load(open(os.path.join(folder, f)))
            model, st, ext = build(d, a.project)
            code = 'RK%02d%s' % (stone, 'ABC'[tset - 1])
            model['set'] = len(sets)
            sets.append(st)
            mapping[code] = len(models)
            models.append(model)
            print('%-7s %4d verts %4d tris  authored size %.2f' %
                  (code, len(model['P']), len(model['T']), ext))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    bt.write_pack3(a.out, models, sets, mapping)
    if a.preview:
        S = 160
        cols = 15
        rows = (len(models) + cols - 1) // cols
        sheet = Image.new('RGB', (S * cols, S * rows), (110, 120, 140))
        L = np.array([0.4, 0.8, 0.45]); L /= np.linalg.norm(L)
        for i, (m, st) in enumerate(zip(models, sets)):
            img = np.zeros((S, S, 3), np.float32) + (110, 120, 140)
            zb = np.full((S, S), -1e9)
            P, T, U, Nn = m['P'], m['T'], m['U'], m['N']
            for tri in T:
                p = P[tri]
                xs = (p[:, 0] * 0.9 + 0.5) * S
                ys = (0.5 - p[:, 1] * 0.9) * S
                x0, x1 = int(max(xs.min(), 0)), int(min(xs.max() + 1, S))
                y0, y1 = int(max(ys.min(), 0)), int(min(ys.max() + 1, S))
                (ax, ay), (bx, by), (cx, cy) = zip(xs, ys)
                den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
                if x1 <= x0 or y1 <= y0 or abs(den) < 1e-9:
                    continue
                gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
                w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
                w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
                w2 = 1 - w0 - w1
                ins = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
                z = w0 * p[0, 2] + w1 * p[1, 2] + w2 * p[2, 2]
                sub = zb[y0:y1, x0:x1]
                ok = ins & (z > sub)
                if not ok.any():
                    continue
                uv = w0[..., None] * U[tri[0]] + w1[..., None] * U[tri[1]] + w2[..., None] * U[tri[2]]
                tw = st['bark'].shape[0]
                c = st['bark'][(np.mod(uv[..., 1], 1) * (tw - 1)).astype(int),
                               (np.mod(uv[..., 0], 1) * (tw - 1)).astype(int)][..., :3]
                nn = Nn[tri].mean(0)
                sh = 0.35 + 0.65 * max(float(np.dot(nn, L)), 0.0)
                sub[ok] = z[ok]
                img[y0:y1, x0:x1][ok] = c[ok] * sh
            sheet.paste(Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)),
                        ((i % cols) * S, (i // cols) * S))
        sheet.save(a.preview)
        print('preview:', a.preview)


if __name__ == '__main__':
    main()
