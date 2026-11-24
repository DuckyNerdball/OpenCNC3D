#!/usr/bin/env python3
"""Hidden native rigid-cull proof: original, witnessed, fast; both games separately.

Requires the same full installed content/engines/settings for both executables.
Writes only under --out. No visible window, real input, or player preference edits.
Timing is deliberately not a pass criterion; use a separate real-loop benchmark.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--baseline-exe', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--game', choices=('td', 'ra', 'both'), default='both')
    parser.add_argument('--ticks', type=int, default=3000)
    parser.add_argument('--growth', type=Path, default=Path(__file__).resolve().parents[2] / 'perfprobe/make-growth-probe.py')
    args = parser.parse_args()
    runtime, output = args.runtime.resolve(), args.out.resolve()
    candidate, baseline = args.exe.resolve(), args.baseline_exe.resolve()
    assert 0 < args.ticks <= 21600
    assert not (output / 'results.json').exists(), 'Use a fresh output directory'
    output.mkdir(parents=True, exist_ok=True)
    spec = importlib.util.spec_from_file_location('growth', args.growth)
    growth = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(growth)
    windows = os.name == 'nt'
    rows = []
    games = ['td', 'ra'] if args.game == 'both' else [args.game]
    for game in games:
        ra = game == 'ra'
        pictures = []
        for mode in ['original', 'witness', 'fast']:
            folder = output / game / mode
            folder.mkdir(parents=True, exist_ok=True)
            body = growth.make_script(str(folder), samples=3, game='ra' if ra else 'dawn').split('echo STEP|burst')[0]
            lines = ['gfxmode 2', 'frames 3', body, 'look FACT:@me', 'dist 4200',
                     'tick ' + str(args.ticks), 'frames 20', 'gfx show_stats 0', 'echo CULL_MOVING']
            for _ in range(240):
                lines.extend(['tick 1', 'frames 1'])
            shot = folder / 'proof.png'
            lines.extend(['meshperfdump', 'shot ' + str(shot), 'quit'])
            script = folder / 'script.txt'
            script.write_text('\n'.join(lines) + '\n')
            gfx = runtime / ('ra3d-fx.cfg' if ra else 'opencnc3d-fx.cfg')
            diagnostic_gfx = runtime / ('performance-ra.cfg' if ra else 'performance-td.cfg')
            if windows and diagnostic_gfx.exists():
                gfx = diagnostic_gfx
            engine = runtime / (('ra3d/RedAlert' if ra else 'TiberianDawn') + ('.dll' if windows else '.dylib'))
            exe = baseline if mode == 'original' else candidate
            command = [str(exe), '--scen', 'SCM01EA', '--pack', 'ra3d/3d/SCM01EA.pack' if ra else 'SCM01EA.pack',
                       '--dylib', str(engine), '--dir', 'ra3d/missions/' if ra else 'missions/',
                       '--content', 'ra3d/data/' if ra else 'content/', '--nosound', '--enhanced',
                       '--w', '1920', '--h', '1080', '--vsync', '0', '--gfx', str(gfx),
                       '--gfxsave', str(folder / 'session.cfg'), '--visualscfg', str(folder / 'visuals.cfg'),
                       '--savedir', str(folder / 'saves'), '--script', str(script), '--skirmish',
                       '--side', 'allies' if ra else 'gdi', '--ai', '3', '--seed', '4242',
                       '--credits', '99999', '--build', '98']
            if ra:
                command.extend(['--ra3d', '--dosdata', 'ra3d/audio/'])
            env = {**os.environ, 'OPENCNC3D_BACKGROUND': '1', 'OPENCNC3D_MESH_EARLY_CULL': '0' if mode == 'original' else '1',
                   'OPENCNC3D_MESH_EARLY_CULL_VERIFY': '1' if mode == 'witness' else '0'}
            with (folder / 'run.log').open('w') as log:
                run = subprocess.run(command, cwd=runtime, env=env, stdout=log,
                                     stderr=subprocess.STDOUT, timeout=600)
            text = (folder / 'run.log').read_text(errors='replace')
            assert run.returncode == 0 and re.search(r'SCRIPT\|end .*0 failures', text), (game, mode, text[-1500:])
            ticks = re.findall(r'^TICK\|1\|objects=\d+\|frame=(\d+)', text.split('ECHO|CULL_MOVING', 1)[1], re.M)
            assert len(ticks) == 240 and all(int(b) == int(a) + 1 for a, b in zip(ticks, ticks[1:])), 'Scene did not advance'
            counters = re.findall(r'^MESHEARLYCULL\|tests=(\d+)\|rejected=(\d+)\|verified=(\d+)\|bad=(\d+)', text, re.M)
            if mode != 'original':
                assert counters, 'No native diagnostic evidence'
                tested, rejected, verified, bad = map(int, counters[-1])
                assert tested > 100 and rejected > 100 and bad == 0, counters[-1]
                assert verified > 100 if mode == 'witness' else verified == 0, counters[-1]
            pictures.append(sha(shot))
            row = dict(game=game, mode=mode, passed=True, counters=counters[-1:] or [],
                       advancing_frames=len(ticks), exe_sha256=sha(exe), engine_sha256=sha(engine),
                       gfx_sha256=sha(gfx), script_sha256=sha(script), run_sha256=sha(folder / 'run.log'),
                       picture_sha256=pictures[-1], command=command)
            rows.append(row)
            print(json.dumps({k:row[k] for k in ['game', 'mode', 'passed', 'counters', 'picture_sha256']}), flush=True)
        assert len(set(pictures)) == 1, (game, 'Pictures differ', pictures)
        (output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')


if __name__ == '__main__':
    main()
