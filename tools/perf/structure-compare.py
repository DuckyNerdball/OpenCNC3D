#!/usr/bin/env python3
"""Compare structure submission against a trusted renderer, using hidden scripts.

Same runtime/assets/settings for both binaries; no focus or pointer access.
Requires Pillow and numpy, like the repository's existing pixel gates.
One-level rounding is reported and narrowly bounded, never hidden as byte identity.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

import numpy as np
from PIL import Image

VIEWS = [(50, 54, 3000), (52, 12, 4200), (32, 32, 4200),
         (2, 2, 1200), (60, 60, 1200), (50, 54, 1200), (50, 54, 4200)]


def run(exe, runtime, gfx, output, label, flags, env, observer=False):
    script = output / (label + '.txt')
    lines = ['tick 30', 'gfx show_stats 0']
    if label.startswith('fog-'):
        lines.append('shrouddarktest')
    views = [(53,52,1200), *VIEWS[1:]] if label.startswith("fog-") else VIEWS
    if label.startswith('pulse-'):
        views = [(50,54,3000)] * len(VIEWS)
    if label.startswith('wave-'):
        views = [(58,54.6,1500)] * len(VIEWS)
    for n, (x, z, distance) in enumerate(views):
        if label.startswith(('wave-', 'pulse-')):
            lines.append('tick ' + str([1,4,30,4,60,4,30][n]))
        lines += [f'cam {x} {z}' , f'dist {distance}', 'frameperf 2',
                  f'shot {output / (label + "-" + str(n) + ".png")}']
    if observer:
        lines.append('meshperfdump')
    lines.append('quit')
    script.write_text('\n'.join(lines) + '\n')
    args = [str(exe), '--scen', 'SCB20EA', '--pack', 'SCB20EA.pack',
            '--dylib', 'TiberianDawn.dylib', '--dir', 'missions/',
            '--content', 'content/', '--nosound', '--w', '2560', '--h', '1440',
            '--enhanced', '--gfx', str(gfx), *flags, '--script', str(script)]
    with (output / (label + '.log')).open('w') as log:
        subprocess.run(args, cwd=runtime, env={**os.environ, **env},
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    text = (output / (label + '.log')).read_text()
    if not re.search(r'SCRIPT\|end .*: \d+ lines, 0 failures', text):
        raise RuntimeError(label + ': script did not finish successfully')
    stats = re.search(r'MESHPERF\|([^\n]+)', text)
    return dict(kv.split('=') for kv in stats[1].split('|')) if stats else {}


def compare(output, a, b):
    counts = []
    for n in range(len(VIEWS)):
        aa = np.asarray(Image.open(output / f'{a}-{n}.png').convert('RGB'), dtype=np.int16)
        bb = np.asarray(Image.open(output / f'{b}-{n}.png').convert('RGB'), dtype=np.int16)
        delta = np.abs(aa - bb).max(axis=2)
        count, maximum = int(np.count_nonzero(delta)), int(delta.max())
        if maximum > 1 or count > 1000:
            raise AssertionError(f'{a}/{b} view {n}: {count} differing pixels, maximum {maximum}')
        counts.append({'view': n, 'changed_pixels': count, 'max_channel_delta': maximum})
    return counts


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'runtime', 'gfx', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument("--damage-dir", type=Path, help="Isolated six-damaged-structure mission directory")
    p.add_argument("--original-baseline", type=Path, help="Trusted pre-optimization binary for the original-path assertion")
    p.add_argument("--pose-check", action="store_true", help="Compare per-frame pose reuse on/off and require reuse counters")
    a = p.parse_args()
    a.output = a.output.resolve()
    a.output.mkdir(parents=True, exist_ok=True)
    results = {}
    for case, flags in [('clear', ['--noshroud']), ('fog', ['--scen', 'SCG02EA', '--pack', 'SCG02EA.pack']),
                        ('normal-off', ['--noshroud', '--noebldnormals'])]:
        old, new = case + '-old', case + '-new'
        run(a.baseline.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output, old, flags, {})
        stats = run(a.candidate.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output,
                    new, flags, {}, True)
        results[case] = {'pixels': compare(a.output, old, new), 'stats': stats}
        if case == 'clear':
            for k in ('hits', 'uploads', 'culled', 'draws', 'class_skip'):
                assert int(stats[k]) > 0, f'{k}: optimization was not exercised'
        if case == 'fog':
            assert int(stats['uploads']) > 0 and int(stats['draws']) > 0, 'fog test drew no remasters'
        if case == 'normal-off':
            assert int(stats['class_skip']) == 0, 'class fallback was removed without normals'
    wave_flags = ['--scen', 'SCG90EA', '--pack', 'SCG01EA.pack', '--noshroud']
    for label, exe in [('wave-old', a.baseline), ('wave-new', a.candidate)]:
        run(exe.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output,
            label, wave_flags, {}, label.endswith('new'))
    results['wave'] = compare(a.output, 'wave-old', 'wave-new')
    first = np.asarray(Image.open(a.output / 'wave-old-0.png'))
    last = np.asarray(Image.open(a.output / 'wave-old-6.png'))
    assert np.count_nonzero(first != last) > 100, 'flag scene did not visibly animate'
    stats = run(a.candidate.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output,
                'no-cull' , ['--noshroud'], {'OPENCNC3D_MESH_CULL': '0'}, True)
    assert int(stats['culled']) == 0
    results['culling'] = compare(a.output, 'clear-new', 'no-cull')
    run(a.candidate.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output,
        'all-off', ['--noshroud'], {'OPENCNC3D_MESH_GPU': '0', 'OPENCNC3D_STRUCTURE_CLASS_SKIP': '0'}, True)
    original = 'clear-old'
    if a.original_baseline:
        original = 'original-old'
        run(a.original_baseline.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output,
            original, ['--noshroud'], {}, False)
    results['original-path'] = compare(a.output, original, 'all-off')
    assert not any(v['changed_pixels'] for v in results['original-path'])
    if a.pose_check:
        stats = run(a.candidate.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output,
                    'pose-off', ['--noshroud'], {'OPENCNC3D_MESH_POSE': '0'}, True)
        assert int(stats['pose_hits']) == 0 and int(stats['pose_entries']) == 0
        assert int(results['clear']['stats']['pose_hits']) > 0
        results['pose-reuse'] = compare(a.output, 'clear-new', 'pose-off')
        assert not any(v['changed_pixels'] for v in results['pose-reuse'])
        for label, exe, env in [('pulse-old', a.baseline, {}),
                                ('pulse-new', a.candidate, {}),
                                ('pulse-no-orphan', a.candidate, {'OPENCNC3D_GLOW_ORPHAN': '0'})]:
            run(exe.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output,
                label, ['--noshroud'], env, label != 'pulse-old')
        results['animated-glow'] = compare(a.output, 'pulse-old', 'pulse-new')
        results['glow-upload'] = compare(a.output, 'pulse-new', 'pulse-no-orphan')
        assert not any(v['changed_pixels'] for v in results['glow-upload'])
        first = np.asarray(Image.open(a.output / 'pulse-old-0.png'))
        last = np.asarray(Image.open(a.output / 'pulse-old-6.png'))
        assert np.count_nonzero(first != last) > 100, 'pulse scene did not animate'
    if a.damage_dir:
        flags = ['--noshroud', '--dir', str(a.damage_dir.resolve()) + '/']
        for label, exe in [('damage-old', a.baseline), ('damage-new', a.candidate)]:
            run(exe.resolve(), a.runtime.resolve(), a.gfx.resolve(), a.output,
                label, flags, {}, label == 'damage-new')
        results['damaged-structures'] = compare(a.output, 'damage-old', 'damage-new')
    (a.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('PASS: clear/fog/normal fallback, animated flag, original path and pass-specific culling; see results.json')


if __name__ == '__main__':
    main()
