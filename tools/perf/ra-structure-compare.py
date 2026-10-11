#!/usr/bin/env python3
"""Hidden RA structure comparison; use an isolated RASTRUCT fixture without infantry.

Keep the original attacking infantry in gate-ra-structures.sh for the damage check.
Its own idle pose is outside this structure image comparison's scope.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

import numpy as np
from PIL import Image

VIEWS = [(51, 66, 3000), (58, 80, 3000), (44, 79, 1200),
         (80, 76, 1200), (58, 80, 4200), (48, 59, 1200), (62, 64, 1800)]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'runtime', 'gfx', 'output'):
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument("--pose-check", action="store_true", help="Require the animated buffer pool and compare it disabled")
    a = ap.parse_args()
    a.output = a.output.resolve()
    a.output.mkdir(parents=True, exist_ok=True)
    fixture = a.runtime / 'ra3d/missions/RASTRUCT.INI'
    assert fixture.is_file()
    infantry = re.search(r'\[Infantry\]\s*([^\[]*)', fixture.read_text())
    assert infantry and not re.search(r'^\d+=', infantry[1], re.M), \
        'Use the canonical fixture with its Infantry section empty for structure pixels'
    results = {}
    legs = [('old', a.baseline, [], {}), ('new', a.candidate, [], {}),
            ('no-cull', a.candidate, [], {'OPENCNC3D_MESH_CULL': '0'}),
            ('all-off', a.candidate, [], {'OPENCNC3D_MESH_GPU': '0',
                                         'OPENCNC3D_STRUCTURE_CLASS_SKIP': '0'}),
            ('normal-old', a.baseline, ['--noebldnormals'], {}),
            ('normal-new', a.candidate, ['--noebldnormals'], {})]
    if a.pose_check:
        legs.append(("pose-off", a.candidate, [], {"OPENCNC3D_MESH_POSE": "0"}))
    for label, exe, flags, env in legs:
        lines = ['tick 5', 'gfx show_stats 0']
        for n, (x, z, dist) in enumerate(VIEWS):
            lines += [f'cam {x} {z}', f'dist {dist}', 'tick 4',
                      f'shot {a.output / (label + "-" + str(n) + ".png")}',
                      'frameperf 5', 'framebench 90']
        lines += ['ebldump']
        if label != 'old' and label != 'normal-old':
            lines.append('meshperfdump')
        lines.append('quit')
        script = a.output / (label + '.txt')
        script.write_text('\n'.join(lines) + '\n')
        args = [str(exe.resolve()), '--ra3d', '--dylib', 'ra3d/RedAlert.dylib',
                '--content', 'ra3d/data', '--dir', 'ra3d/missions/', '--scen', 'RASTRUCT',
                '--pack', 'ra3d/3d/SCG05EA.pack', '--dosdata', 'ra3d/audio', '--nosound',
                '--w', '2560', '--h', '1440', '--enhanced', '--noshroud',
                '--gfx', str(a.gfx.resolve()), *flags, '--script', str(script)]
        log = a.output / (label + '.log')
        with log.open('w') as out:
            subprocess.run(args, cwd=a.runtime, env={**os.environ, **env}, stdout=out,
                           stderr=subprocess.STDOUT, check=True, timeout=180)
        text = log.read_text()
        assert re.search(r'SCRIPT\|end .*: \d+ lines, 0 failures', text), label
        assert len(re.findall(r'^RASTRUCT\|.*\|draw=model$', text, re.M)) == 33, label
        stats = re.search(r'MESHPERF\|([^\n]+)', text)
        results[label] = {'frames': re.findall(r'FRAMEBENCH\|[^\n]+', text),
                          'stats': dict(kv.split('=') for kv in stats[1].split('|')) if stats else {}}
    pairs = [('old', 'new'), ('new', 'no-cull'), ('old', 'all-off'),
             ('normal-old', 'normal-new')]
    if a.pose_check:
        pairs.append(('new', 'pose-off'))
        # This RA scene has no repeated compatible dynamic draw in one frame.
        # It still exercises the separate stores used by the shared backend.
        assert int(results['new']['stats']['pose_entries']) > 0
        assert int(results['pose-off']['stats']['pose_hits']) == 0
        assert int(results['pose-off']['stats']['pose_entries']) == 0
    for old, new in pairs:
        rows = []
        for n in range(len(VIEWS)):
            aa = np.asarray(Image.open(a.output / f'{old}-{n}.png').convert('RGB'), dtype=np.int16)
            bb = np.asarray(Image.open(a.output / f'{new}-{n}.png').convert('RGB'), dtype=np.int16)
            delta = np.abs(aa - bb).max(axis=2)
            count, maximum = int(np.count_nonzero(delta)), int(delta.max())
            assert maximum <= 1 and count <= 1000, (old, new, n, count, maximum)
            rows.append({'view': n, 'changed_pixels': count, 'max_channel_delta': maximum})
        results[old + '/' + new] = rows
    for k in ('hits', 'uploads', 'culled', 'draws'):
        assert int(results['new']['stats'][k]) > 0, k
    assert int(results['no-cull']['stats']['culled']) == 0
    assert int(results['normal-new']['stats']['class_skip']) == 0
    (a.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('PASS: 33 RA models in seven views, GPU reuse/culling, normal fallback and original path; results.json')


if __name__ == '__main__':
    main()
