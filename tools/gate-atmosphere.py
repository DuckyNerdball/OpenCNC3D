#!/usr/bin/env python3
"""Render checks for Enhanced fog/dust, local scattering, presets and shroud.
Requires an existing runnable developer build, Pillow and NumPy. Never opens a
visible window. Pass --desert-pack for an actual DESERT/SAND mission pack.
"""
import argparse
import hashlib
import re
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rundir', type=Path, default=ROOT / 'playable')
parser.add_argument('--output', type=Path, default=ROOT / 'tmp/atmosphere-gate')
parser.add_argument('--desert-pack', type=Path, required=True)
parser.add_argument('--desert-scenario', default='SCB01EA')
args = parser.parse_args()
run = args.rundir.resolve()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)


def shot(name):
    return f'shot {out / (name + ".png")}\n'


def render(name, script, scenario='SCG01EB', pack=None, shroud=False):
    script_path = out / (name + '.txt')
    script_path.write_text(script + 'quit\n')
    targets = re.findall(r'^shot (.+)$', script, re.M)
    for target in targets:
        Path(target).unlink(missing_ok=True)
    command = [str(run / 'cnc_eyes'), '--scen', scenario, '--pack', str(pack or run/'SCG01EA.pack'),
               '--cameos', 'cameos.pack', '--dospack', 'dossidebar.pack', '--dosinf', 'dosinfantry.pack',
               '--dylib', 'TiberianDawn.dylib', '--dir', 'missions/', '--content', 'content/',
               '--w', '1000', '--h', '650', '--script', str(script_path)]
    if not shroud:
        command += ['--noshroud']
    result = subprocess.run(command, cwd=run, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
    log = result.stdout.decode(errors='replace')
    (out / (name+'.log')).write_text(log)
    assert result.returncode == 0 and re.search(r'^SCRIPT\|end .+: \d+ lines, 0 failures$', log, re.M), log[-4000:]
    for target in targets:
        assert Path(target).is_file(), f'No fresh render: {target}'


def changed(a, b):
    x = np.array(Image.open(out/(a+'.png')))
    y = np.array(Image.open(out/(b+'.png')))
    assert x.shape == y.shape
    return int(np.any(x != y, axis=2).sum())


setup = 'tick 30\ncam 56 55\nzoom min\ngfx enabled 1\ngfx ss_scale 1\ngfx cloud_on 0\n'
render('mist', setup + 'gfx fog_fx 0\n' + shot('mist-off') +
       'gfx fog_fx 1\n' + shot('mist-on') + 'gfx fog_speed 0\n' + shot('mist-parked') +
       'gfx fog_speed 3\n' + shot('mist-drift') + 'gfx fog_density 0\n' + shot('mist-zero'))
assert changed('mist-off', 'mist-on') > 2000
assert changed('mist-parked', 'mist-drift') > 2000
assert changed('mist-off', 'mist-zero') == 0
print('PASS: mist extinction, simulation drift and zero-density identity', flush=True)

# Each pair uses the same live source and unchanged surface-light settings.
for effect, anim in [('fire', 30), ('explosion', 2), ('muzzle', 34)]:
    render(effect, setup + f'gfx fog_density 0.2\ngfx light_intensity 2\ngfx light_radius 6\nefxanim {anim} 54 54\ntick 1\n' +
           'gfx fog_light 0\n' + shot(effect+'-unlit') + 'gfx fog_light 8\n' + shot(effect+'-lit'))
    count = changed(effect+'-unlit',effect+'-lit')
    assert count > 500, f'{effect}: no scattering response ({count} pixels)'
    print(f'PASS: {effect} colours the volume ({count} changed pixels)', flush=True)

render('tiberium', setup + 'cam 30 50\ngfx fog_density 0.15\ngfx light_tiberium 1\ngfx tib3d_glow 1\n' +
       'gfx fog_light 0\n' + shot('tiberium-unlit') + 'gfx fog_light 8\n' + shot('tiberium-lit'), scenario='SCG05EA', pack=run/'SCG05EA.pack')
assert changed('tiberium-unlit','tiberium-lit') > 500
print('PASS: tiberium colours the volume', flush=True)

pack = args.desert_pack.resolve()
with pack.open('rb') as file:
    header = file.read(52)
assert header[36:52].split(b'\0')[0] in (b'DESERT',b'SAND'), 'The dust gate requires an actual desert pack'
render('sand', 'tick 30\ngfx enabled 1\ngfx ss_scale 1\ngfx cloud_on 0\ngfx sand_fx 0\n' + shot('sand-off') +
       'gfx sand_fx 1\n' + shot('sand-on') + 'gfx fog_fx 0\n' + shot('sand-fog-off') +
       'gfx sand_speed 0\n' + shot('sand-parked') + 'gfx sand_speed 3\n' + shot('sand-drift'),
       scenario=args.desert_scenario, pack=pack)
assert changed('sand-off','sand-on') > 2000
assert changed('sand-on','sand-fog-off') == 0
assert changed('sand-parked','sand-drift') > 2000
print('PASS: desert-only dust and drift', flush=True)

# Keep an actual reveal perimeter in the review outputs: an entirely dark map
# cannot catch a hard per-cell boundary around clear terrain.
render('soft-edge', setup + 'gfx fog_density 0.2\ngfx fog_fx 0\n' + shot('soft-edge-off') +
       'gfx fog_fx 1\n' + shot('soft-edge-on'), shroud=True)
assert changed('soft-edge-off','soft-edge-on') > 2000
print('PASS: mist renders at the soft reveal perimeter (review edge captures)', flush=True)

render('shroud', setup + 'shrouddarktest\ngfx fog_fx 0\n' + shot('dark-off') +
       'gfx fog_fx 1\n' + shot('dark-on'), shroud=True)
assert changed('dark-off','dark-on') == 0
render('classic', setup + 'gfx enabled 0\ngfx fog_fx 0\ngfx sand_fx 0\n' + shot('classic-off') +
       'gfx fog_fx 1\ngfx sand_fx 1\n' + shot('classic-on'))
assert changed('classic-off','classic-on') == 0
print('PASS: dark shroud and Classic unchanged', flush=True)

render('panel', setup + 'gfxpanel 1 fog_fx\n' + shot('f5-temperate') +
       'gfxpanel 1 sand_fx\n' + shot('f5-desert') + 'gfxpanel 0\n' +
       f'gfxsave {out / "a.cfg"}\ngfxload {out / "a.cfg"}\ngfxsave {out / "b.cfg"}\n')
assert (out/'a.cfg').read_bytes() == (out/'b.cfg').read_bytes()
print('PASS: both F5 profiles reachable; preset round trip byte exact', flush=True)

hashes = []
for repeat in range(5):
    render('determinism-'+str(repeat), setup + shot('determinism-'+str(repeat)))
    hashes.append(hashlib.sha256((out/f'determinism-{repeat}.png').read_bytes()).hexdigest())
assert len(set(hashes)) == 1, hashes
# An orthographic ray must also produce extinction in front of visible geometry.
render('orthographic', setup + 'cammode old\ngfx fog_fx 0\n' + shot('ortho-off') +
       'gfx fog_fx 1\n' + shot('ortho-on'))
assert changed('ortho-off','ortho-on') > 2000
print('PASS: five-run determinism and orthographic view rays', flush=True)
print(f'All atmosphere checks passed. Review renders in {out}', flush=True)
