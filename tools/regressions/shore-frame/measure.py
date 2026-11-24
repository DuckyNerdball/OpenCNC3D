#!/usr/bin/env python3
"""Native immutable-data performance comparison; tick-advancing replay and live play."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, re, subprocess, time
p = argparse.ArgumentParser()
p.add_argument('--runtime', required=True, type=Path)
p.add_argument('--exe', required=True, type=Path)
p.add_argument('--out', required=True, type=Path)
p.add_argument('--growth', required=True, type=Path)
p.add_argument('--game', choices=['td', 'ra'], default='ra')
p.add_argument('--ticks', type=int, default=21600)
p.add_argument('--baseline-exe', type=Path)
p.add_argument('--single', action='store_true')
p.add_argument('--verify', action='store_true')
p.add_argument('--live', type=int, default=20)
a = p.parse_args()
r = a.runtime.resolve()
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=True)
ra = a.game == 'ra'
win = os.name == 'nt'
spec = importlib.util.spec_from_file_location('growth', a.growth.resolve())
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)
rows = []
for i, enabled in enumerate([True] if a.single else [False, True, True, False]):
    d = out / (str(i) + ('-on' if enabled else '-off'))
    d.mkdir(exist_ok=True)
    body = g.make_script(str(d), samples=3, game='ra' if ra else 'dawn').split('echo STEP|burst')[0]
    lines = ['gfxmode 2', 'frames 3'] + body.splitlines() + ['look FACT:@me', 'dist 4200', 'echo FAST_FORWARD', f'tick {a.ticks}', 'frames 20', 'gfx show_stats 0', 'echo MOVING_BENCH', 'fprofreport']
    for sample in range(240):
        lines += ['tick 1', 'framebench 1']
    lines += ['fprofreport', 'm3dump', 'meshperfdump', 'shorecachedump', 'shot ' + str(d / 'proof.png'), 'echo LIVE_PHASE']
    if a.live:
        lines += ['livebench ' + str(a.live) + ' play', 'fprofreport', 'm3dump', 'meshperfdump', 'shorecachedump']
    lines += ['quit']
    if a.baseline_exe and (not enabled):
        lines = [line for line in lines if line != 'shorecachedump']
    script = d / 'script.txt'
    script.write_text('\n'.join(lines) + '\n')
    engine = ('ra3d/RedAlert' if ra else 'TiberianDawn') + ('.dll' if win else '.dylib')
    preset = r / (('performance-ra.cfg' if ra else 'performance-td.cfg') if win else 'ra3d-fx.cfg' if ra else 'opencnc3d-fx.cfg')
    exe = (a.baseline_exe if a.baseline_exe and (not enabled) else a.exe).resolve()
    cmd = [str(exe), '--scen', 'SCM01EA', '--pack', ('ra3d/3d/' if ra else '') + 'SCM01EA.pack', '--dylib', engine, '--dir', 'ra3d/missions/' if ra else 'missions/', '--content', 'ra3d/data/' if ra else 'content/', '--nosound', '--enhanced', '--w', '1920', '--h', '1080', '--vsync', '0', '--gfx', str(preset), '--gfxsave', str(d / 'session.cfg'), '--visualscfg', str(d / 'visuals.cfg'), '--savedir', str(d / 'saves'), '--script', str(script), '--skirmish', '--side', 'allies' if ra else 'gdi', '--ai', '3', '--seed', '4242', '--credits', '99999', '--build', '98']
    if ra:
        cmd += ['--ra3d', '--dosdata', 'ra3d/audio/']
    env = {**os.environ, 'OPENCNC3D_BACKGROUND': '1', 'OPENCNC3D_FRAMEPROF': '1', 'OPENCNC3D_PERFDETAIL': '1', 'OPENCNC3D_SHORE_FILM_CACHE': '1', 'OPENCNC3D_MESH_RIGID_ATTRIBUTES': '1', 'OPENCNC3D_WATER_ARRAY_CACHE': '1', 'OPENCNC3D_SHORE_FRAME_CACHE': ('verify' if a.verify else '1') if enabled else '0'}
    env.pop('OPENCNC3D_GPUPROF', None)
    print('START', a.game, i, enabled, flush=True)
    start = time.monotonic()
    with (d / 'run.log').open('w') as f:
        c = subprocess.run(cmd, cwd=r, env=env, stdout=f, stderr=subprocess.STDOUT, timeout=600)
    s = (d / 'run.log').read_text(errors='replace')
    moving = s.split('ECHO|MOVING_BENCH', 1)[-1].split('ECHO|LIVE_PHASE', 1)[0]
    samples = [float(x) for x in re.findall('^FRAMEBENCH\\|n=1\\|median=([\\d.]+)', moving, re.M)]
    m = [(str(__import__('statistics').median(samples)), str(sorted(samples)[int(len(samples) * 0.9)]))] if len(samples) == 240 else []
    u = re.findall('^' + 'SHOREFRAMECACHE' + '\\|offers=(\\d+)\\|hits=(\\d+)\\|verified=(\\d+)\\|bad=(\\d+)', s, re.M)
    profile = re.findall('^FRAMEPROF\\|frames=240.*$', s, re.M)
    if a.baseline_exe and (not enabled):
        u = [('0', '0', '0', '0')]
    row = dict(game=a.game, arm=i, enabled=enabled, exit=c.returncode, script_ok=bool(re.search('SCRIPT\\|end .*0 failures', s)), seconds=time.monotonic() - start, median_ms=float(m[-1][0]) if m else None, p90_ms=float(m[-1][1]) if m else None, cache=u[-1:] if u else [], profiles=profile, live_profiles=re.findall('^FRAMEPROF\\|frames=.*$', s.split('ECHO|LIVE_PHASE', 1)[-1], re.M), samples=samples, diagnostic_verify=a.verify, body_states=re.findall('^INFDEATH\\|.*$', s, re.M), view_states=re.findall('^CORPSEVIEW\\|limit=.*$', s, re.M), command=cmd, exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(), script_sha256=hashlib.sha256(script.read_bytes()).hexdigest())
    rows.append(row)
    (out / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(json.dumps({k: row[k] for k in ['arm', 'enabled', 'exit', 'script_ok', 'seconds', 'median_ms', 'p90_ms', 'cache']}), flush=True)
    assert c.returncode == 0 and row['script_ok'] and m and u, (s[-1500:], row)
    assert (int(u[-1][0]) > 0) == enabled and int(u[-1][3]) == 0, u[-1]
    if enabled and (not a.verify):
        assert int(u[-1][1]) > 100, u[-1]
    if a.verify and enabled:
        assert int(u[-1][2]) > 100, u[-1]
