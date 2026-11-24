#!/usr/bin/env python3
"""Hidden native immutable-data proof: original, witnessed, fast; both games separately.

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
    parser.add_argument('--campaign', action='store_true', help='Positive native coast fixtures instead of a vehicle battle')
    parser.add_argument('--state-checks', action='store_true', help='Also exercise camera, fog, graphics modes and mission restart')
    parser.add_argument('--fault', choices=['array-load','array-alloc','array-store','array-check','array-budget','film-alloc','film-budget','frame-alloc'])
    parser.add_argument('--ticks', type=int, default=3000)
    parser.add_argument('--growth', type=Path, default=Path(__file__).resolve().parents[2] / 'perfprobe/make-growth-probe.py')
    args = parser.parse_args()
    runtime, output = args.runtime.resolve(), args.out.resolve()
    candidate, baseline = args.exe.resolve(), args.baseline_exe.resolve()
    assert 0 < args.ticks <= 21600
    assert not output.exists(), 'Use a fresh output directory'
    output.mkdir(parents=True, exist_ok=True)
    spec = importlib.util.spec_from_file_location('growth', args.growth)
    growth = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(growth)
    windows = os.name == 'nt'
    rows = []
    games = ['td', 'ra'] if args.game == 'both' else [args.game]
    for game in games:
        scene = 'SCG01EA' if args.campaign else 'SCM01EA'
        ra = game == 'ra'
        picture_sets = []
        for mode in ['original', 'witness', 'fast']:
            folder = output / game / mode
            folder.mkdir(parents=True, exist_ok=True)
            body = growth.make_script(str(folder), samples=3, game='ra' if ra else 'dawn').split('echo STEP|burst')[0]
            lines = ['gfxmode 2', 'frames 3', body, 'look FACT:@me', 'dist 4200',
                     'tick ' + str(args.ticks), 'frames 20', 'gfx show_stats 0', 'echo CULL_MOVING']
            if args.campaign:
                lines = ['gfxmode 2', 'gfx show_stats 0', 'reveal 1', 'tick 1', 'cam 57 56' if ra else 'cam 50 57', 'dist 3000', 'frames 20', 'echo CULL_MOVING']
            for _ in range(240):
                lines.extend(['tick 1', 'frames 1'])
            shot = folder / 'proof.png'
            lines.extend(['echo STATE_CHECKS', 'meshperfdump', 'shorecachedump', 'shot ' + str(shot)])
            shots = [shot]
            if args.state_checks:
                assert args.campaign, 'State transitions require a campaign fixture'
                camera = 'cam 57 56' if ra else 'cam 50 57'
                states = [('pan', ['cam 59 56' if ra else 'cam 52 57']),
                          ('return', [camera]), ('classic', ['gfxmode 0']),
                          ('enhanced', ['gfxmode 2']), ('fog', ['reveal 0']),
                          ('reveal', ['reveal 1']),
                          ('restart', ['remission', 'gfxmode 2', 'reveal 1', 'tick 1', camera, 'dist 3000'])]
                for label, commands in states:
                    picture = folder / (label + '.png')
                    lines.extend(commands + ['frames 20', 'shot ' + str(picture)])
                    shots.append(picture)
                lines.append('shorecachedump')
            lines.append('quit')
            if mode == 'original': lines = [line for line in lines if line != 'shorecachedump']
            script = folder / 'script.txt'
            script.write_text('\n'.join(lines) + '\n')
            gfx = runtime / ('ra3d-fx.cfg' if ra else 'opencnc3d-fx.cfg')
            diagnostic_gfx = runtime / ('performance-ra.cfg' if ra else 'performance-td.cfg')
            if windows and diagnostic_gfx.exists():
                gfx = diagnostic_gfx
            engine = runtime / (('ra3d/RedAlert' if ra else 'TiberianDawn') + ('.dll' if windows else '.dylib'))
            exe = baseline if mode == 'original' else candidate
            command = [str(exe), '--scen', scene, '--pack', ('ra3d/3d/' if ra else '') + scene + '.pack',
                       '--dylib', str(engine), '--dir', 'ra3d/missions/' if ra else 'missions/',
                       '--content', 'ra3d/data/' if ra else 'content/', '--nosound', '--enhanced',
                       '--w', '1920', '--h', '1080', '--vsync', '0', '--gfx', str(gfx),
                       '--gfxsave', str(folder / 'session.cfg'), '--visualscfg', str(folder / 'visuals.cfg'),
                       '--savedir', str(folder / 'saves'), '--script', str(script), '--skirmish',
                       '--side', 'allies' if ra else 'gdi', '--ai', '3', '--seed', '4242',
                       '--credits', '99999', '--build', '98']
            if args.campaign: command = command[:command.index('--skirmish')] + ['--seed', '4242']
            if ra:
                command.extend(['--ra3d', '--dosdata', 'ra3d/audio/'])
            env = {**os.environ, 'OPENCNC3D_BACKGROUND': '1', 'OPENCNC3D_SHORE_FILM_CACHE': 'verify' if mode == 'witness' else '0' if mode == 'original' else '1', 'OPENCNC3D_WATER_ARRAY_CACHE': 'verify' if mode == 'witness' else '0' if mode == 'original' else '1'}
            if args.fault:
                assert args.campaign
                fault = {'array-load':'OPENCNC3D_WATER_ARRAY_LOAD_OK', 'array-alloc':'OPENCNC3D_WATER_ARRAY_ALLOC_OK',
                         'array-store':'OPENCNC3D_WATER_ARRAY_STORE_OK', 'array-check':'OPENCNC3D_WATER_ARRAY_CHECK_OK',
                         'array-budget':'OPENCNC3D_WATER_ARRAY_BUDGET_FULL', 'film-alloc':'OPENCNC3D_SHORE_FILM_ALLOC_OK',
                         'film-budget':'OPENCNC3D_SHORE_FILM_BUDGET_FULL', 'frame-alloc':'OPENCNC3D_SHORE_FRAME_ALLOC_OK'}[args.fault]
                env[fault] = '0'
            for setting in ['OPENCNC3D_MESH_RIGID_ATTRIBUTES','OPENCNC3D_SHORE_FILM_CACHE','OPENCNC3D_WATER_ARRAY_CACHE']:
                env[setting]=('1' if windows else '0') if mode=='original' else 'verify' if mode=='witness' else '1'
            env['OPENCNC3D_SHORE_FRAME_CACHE']='0' if mode=='original' else 'verify' if mode=='witness' else '1'
            with (folder / 'run.log').open('w') as log:
                run = subprocess.run(command, cwd=runtime, env=env, stdout=log,
                                     stderr=subprocess.STDOUT, timeout=600)
            text = (folder / 'run.log').read_text(errors='replace')
            assert run.returncode == 0 and re.search(r'SCRIPT\|end .*0 failures', text), (game, mode, text[-1500:])
            ticks = re.findall(r'^TICK\|1\|objects=\d+\|frame=(\d+)', text.split('ECHO|CULL_MOVING', 1)[1].split('ECHO|STATE_CHECKS',1)[0], re.M)
            assert len(ticks) == 240 and all(int(b) == int(a) + 1 for a, b in zip(ticks, ticks[1:])), 'Scene did not advance'
            prefix = 'SHOREFRAMECACHE'
            counters = re.findall(r'^' + prefix + r'\|offers=(\d+)\|hits=(\d+)\|verified=(\d+)\|bad=(\d+)', text, re.M)
            if mode != 'original':
                assert counters, 'No native diagnostic evidence'
                tested, rejected, verified, bad = map(int, counters[-1])
                assert bad == 0 and (tested > 100 or args.fault=='array-load'), counters[-1]
                if not (args.fault and (args.fault.startswith('array-') or args.fault=='frame-alloc')):
                    if mode=='fast': assert rejected>100,counters[-1]
                    assert verified > 100 if mode == 'witness' else verified == 0, counters[-1]
            film = re.findall(r'^SHOREFILMCACHE\|offers=(\d+)\|hits=(\d+)\|verified=(\d+)\|bad=(\d+)',text,re.M)
            if mode != 'original' and args.campaign: assert film and int(film[-1][3]) == 0, film[-1:]
            for label, cap in [('SHOREFILMCACHE',32*1024*1024),('WATERARRAYCACHE',16*1024*1024),('SHOREFRAMECACHE',2*1024*1024)]:
                metrics = re.findall(r'^'+label+r'\|offers=(\d+)\|hits=(\d+)\|verified=(\d+)\|bad=(\d+)\|fallbacks=(\d+)\|bytes=(\d+)',text,re.M)
                if mode!='original':
                    assert metrics and all(int(v[5])<=cap and int(v[3])==0 for v in metrics),metrics
                    if args.fault and ((label=='WATERARRAYCACHE' and args.fault.startswith('array-')) or (label=='SHOREFILMCACHE' and args.fault.startswith('film-')) or (label=='SHOREFRAMECACHE' and args.fault=='frame-alloc')):
                        assert int(metrics[-1][4])>0,metrics[-1]
            picture_sets.append([sha(path) for path in shots])
            row = dict(game=game, mode=mode, passed=True, counters=counters[-1:] or [],
                       advancing_frames=len(ticks), exe_sha256=sha(exe), engine_sha256=sha(engine),
                       gfx_sha256=sha(gfx), script_sha256=sha(script), run_sha256=sha(folder / 'run.log'),
                       picture_sha256=picture_sets[-1][0], pictures=picture_sets[-1], fault=args.fault, command=command)
            rows.append(row)
            print(json.dumps({k:row[k] for k in ['game', 'mode', 'passed', 'counters', 'picture_sha256']}), flush=True)
        assert picture_sets[0] == picture_sets[1] == picture_sets[2], (game, 'Pictures differ', picture_sets)
        (output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')


if __name__ == '__main__':
    main()
