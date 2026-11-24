#!/usr/bin/env python3
"""Compare two folders of modern-shots.py pictures: numbers and side-by-side sheets.

    compare-shots.py --before <dir> --after <dir> --out <dir> [--label-a Before --label-b After]

For every PNG present in both folders it writes <out>/<name>_pair.png (the two pictures
side by side, the second scaled to the first's size if they differ) and records, per
picture, the mean colour, the mean and percentiles of luminance and the share of pixels
at or above 250 (clipped white) and at or below 5 (crushed black). Region means come
from a 4x4 grid so a change confined to part of the frame is visible in the numbers.
The HUD strip (the top 24 rows and the sidebar on the right) is excluded from the
statistics; it is not lit by the world and would dilute every comparison.

Needs Pillow and NumPy.
"""
import argparse, json, os
import numpy as np
from PIL import Image, ImageDraw


def stats(img):
    a = np.asarray(img.convert('RGB'), dtype=np.float32)
    h, w, _ = a.shape
    a = a[int(h * 0.03):, :int(w * 0.86)]
    lum = 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]
    gh, gw = lum.shape[0] // 4, lum.shape[1] // 4
    grid = [[round(float(lum[y * gh:(y + 1) * gh, x * gw:(x + 1) * gw].mean()), 1) for x in range(4)] for y in range(4)]
    return dict(mean_rgb=[round(float(a[..., c].mean()), 1) for c in range(3)],
                mean_lum=round(float(lum.mean()), 2),
                p05=round(float(np.percentile(lum, 5)), 1), p50=round(float(np.percentile(lum, 50)), 1),
                p95=round(float(np.percentile(lum, 95)), 1),
                clipped=round(float((lum >= 250).mean()), 4), crushed=round(float((lum <= 5).mean()), 4),
                grid=grid)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--before', required=True)
    p.add_argument('--after', required=True)
    p.add_argument('--out', required=True)
    p.add_argument('--label-a', default='BEFORE')
    p.add_argument('--label-b', default='AFTER')
    a = p.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rows = {}
    for n in sorted(os.listdir(a.before)):
        if not n.endswith('.png') or not os.path.exists(os.path.join(a.after, n)):
            continue
        A = Image.open(os.path.join(a.before, n)).convert('RGB')
        B = Image.open(os.path.join(a.after, n)).convert('RGB')
        if B.size != A.size:
            B = B.resize(A.size, Image.LANCZOS)
        sa, sb = stats(A), stats(B)
        rows[n[:-4]] = dict(before=sa, after=sb, d_mean_lum=round(sb['mean_lum'] - sa['mean_lum'], 2),
                            d_mean_rgb=[round(sb['mean_rgb'][i] - sa['mean_rgb'][i], 1) for i in range(3)])
        W, H = A.size
        sheet = Image.new('RGB', (W * 2 + 8, H), (0, 0, 0))
        sheet.paste(A, (0, 0))
        sheet.paste(B, (W + 8, 0))
        d = ImageDraw.Draw(sheet)
        for x, t in ((10, a.label_a), (W + 18, a.label_b)):
            d.rectangle([x - 4, H - 34, x + 8 * len(t) + 14, H - 8], fill=(0, 0, 0))
            d.text((x + 4, H - 28), t, fill=(255, 255, 255))
        sheet.save(os.path.join(a.out, n[:-4] + '_pair.png'))
    with open(os.path.join(a.out, 'compare.json'), 'w') as f:
        json.dump(rows, f, indent=1)
    print('%-18s %8s %8s %7s  %s' % ('scene', 'lumA', 'lumB', 'dLum', 'dRGB'))
    for k, r in rows.items():
        print('%-18s %8.1f %8.1f %+7.1f  %s  clipA=%.3f clipB=%.3f' % (
            k, r['before']['mean_lum'], r['after']['mean_lum'], r['d_mean_lum'], r['d_mean_rgb'],
            r['before']['clipped'], r['after']['clipped']))


if __name__ == '__main__':
    main()
