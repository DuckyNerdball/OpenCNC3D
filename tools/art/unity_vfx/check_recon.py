"""Checks recon.py against Unity's own baked geometry: python3 check_recon.py <bakedir>.
Every render mode the pack uses must rebuild Unity's vertices, UVs and custom streams."""
import sys, json, numpy as np, c3vx, recon, glob, os
O = sys.argv[1]
m = c3vx.load_meta(O); fps, fr = c3vx.load_seed(O + '/seed0.bin')
meshes = {}
def mesh(n):
    if n not in meshes: meshes[n] = c3vx.load_mesh(f'{O}/mesh_{n}.bin')
    return meshes[n]
worst = 0.0
for camf in sorted(glob.glob(O + '/probe/f*_camera.json')):
    f = int(os.path.basename(camf)[1:5]); cam = json.load(open(camf))
    for s in m['systems']:
        pf = f'{O}/probe/f{f:04d}_s{s["index"]:02d}.bin'
        if not os.path.exists(pf): continue
        v, ix = c3vx.load_probe(pf); P = recon.system_scale(fr[f][s['index']], s)
        if not len(P): continue
        mode = s['renderMode']
        counts = [4] * len(P)
        if mode == 'Mesh':
            out = []; counts = []
            for p in P:
                counts.append(len(mesh(s['meshes'][int(p[29])])[0]))
                mv, mi = mesh(s['meshes'][int(p[29])])
                out.append(p[1:4] + (recon.mesh_xform(p, cam, s) @ mv[:, :3].T).T)
            out = np.concatenate(out)
        elif mode == 'Billboard':
            out = np.concatenate([recon.billboard(p, cam) for p in P])
        elif mode == 'HorizontalBillboard':
            out = np.concatenate([recon.horizontal(p, cam) for p in P])
        elif mode == 'Stretch':
            out = np.concatenate([recon.stretched(p, cam, s) for p in P])
        if out.shape != v[:, :3].shape:
            print(f'f{f} {s["name"]}: {len(out)} vertices rebuilt, Unity has {len(v)}'); worst = 9e9; continue
        err = np.abs(out - v[:, :3]).max()
        # the streams: uv0.zw = custom1.xy, uv1.xy = custom1.zw, uv1.zw = custom2.xy
        per = counts
        c1 = np.repeat(P[:, 19:23], per, 0)
        serr = 0.0
        if 'Custom1XYZW' in s['streams']:
            serr = max(np.abs(v[:, 12:14] - c1[:, 0:2]).max(), np.abs(v[:, 14:16] - c1[:, 2:4]).max())
        # colour: linear of the recorded (gamma) colour, alpha untouched
        col = np.repeat(P[:, 10:14], per, 0)
        lin = np.where(col[:, :3] <= 0.04045, col[:, :3] / 12.92, ((col[:, :3] + 0.055) / 1.055) ** 2.4)
        # Unity hands the vertex colour over as 8 bits AFTER linearising it
        lin = np.round(lin * 255.0) / 255.0
        cerr = max(np.abs(v[:, 6:9] - lin).max(), np.abs(v[:, 9] - col[:, 3]).max())
        print(f'f{f:4d} {s["name"]:26s} {mode:20s} n={len(P):3d} pos {err:.5f}  streams {serr:.5f}  colour {cerr:.4f}')
        worst = max(worst, err)
print('WORST position error (m):', worst)
