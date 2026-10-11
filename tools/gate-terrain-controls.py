#!/usr/bin/env python3
"""Hidden runtime checks for terrain colour, independent shadows and F5 persistence.
Requires a built playable folder, Pillow and NumPy. Outputs actual runtime captures.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--rundir', type=Path, default=ROOT / 'playable')
p.add_argument('--output', type=Path, default=ROOT / 'tmp/terrain-controls-gate')
a = p.parse_args()
run, out = a.rundir.resolve(), a.output.resolve()
out.mkdir(parents=True, exist_ok=True)

def shot(name):
    return f'shot {out / (name + ".png")}\n'

def render(name, body, scenario='USER60', shroud=False):
    script = out / (name + '.txt')
    script.write_text(body + 'quit\n')
    targets = re.findall(r'^shot (.+)$', body, re.M)
    for target in targets:
        Path(target).unlink(missing_ok=True)
    cmd = [str(run/'cnc_eyes'), '--scen', scenario, '--pack', 'SCG01EA.pack',
           '--cameos', 'cameos.pack', '--dospack', 'dossidebar.pack', '--dosinf', 'dosinfantry.pack',
           '--dylib', 'TiberianDawn.dylib', '--content', 'content/', '--nosound',
           '--w', '1000', '--h', '650', '--script', str(script)]
    if scenario.startswith('USER'):
        cmd += ['--dir', 'missions/user_maps/', '--reskin']
    else:
        cmd += ['--dir', 'missions/']
    if not shroud:
        cmd += ['--noshroud']
    result = subprocess.run(cmd, cwd=run, env={**os.environ, 'OPENCNC3D_BACKGROUND':'1'},
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
    log = result.stdout.decode(errors='replace')
    (out/(name+'.log')).write_text(log)
    assert result.returncode == 0 and re.search(r'^SCRIPT\|end .+: \d+ lines, 0 failures$', log, re.M), log[-4000:]
    assert not re.search(r'FX\|(?:shader|link|rt)\|.*(?:fail|incomplete)', log, re.I), log[-4000:]
    for target in targets:
        assert Path(target).is_file(), target

def pixels(name):
    return np.asarray(Image.open(out/(name+'.png')).convert('RGB')).astype(int)

def changed(x,y):
    # Ignore the animated credit counter at the top and the framebuffer rim.
    # The playable world and static sidebar must still match pixel for pixel.
    return int(np.any(pixels(x)[24:-24] != pixels(y)[24:-24], axis=2).sum())

setup = ('gfxmode 2\ntick 300\ngfx show_stats 0\ngfx ss_scale 1\ngfx cloud_on 0\ngfx fog_fx 0\n'
         'gfx sand_fx 0\ngfx dfog_on 0\ngfx bloom_on 0\n')
# Ground casting off must match terrain strength zero while object shadows remain.
render('shadows', setup + 'gfx contact_on 0\ngfx sun_lambert 0\n' +
       'gfx terrain_shadow_strength 1\n' + shot('shadow-full') +
       'gfx terrain_shadow_strength 0.5\n' + shot('shadow-half') +
       'gfx terrain_shadow_strength 0\n' + shot('shadow-zero') +
       'gfx shadow_terrain 0\n' + shot('ground-casts-off') +
       'gfx shadow_on 0\n' + shot('all-shadows-off'), scenario='SCB05EA')
assert changed('shadow-zero','ground-casts-off') == 0
assert changed('shadow-zero','all-shadows-off') > 500
assert changed('shadow-full','shadow-half') > 500
assert changed('shadow-half','shadow-zero') > 500
print('PASS: terrain strength changes hill shadows; zero keeps object shadows', flush=True)

# The reported USER60 cliff stays black if only its cast map follows the dial.
# Disable terrain casting to measure the cliff's own face and slope shading.
render('cliff-faces', setup + 'cam 27 7\nzoom min\ngfx shadow_terrain 0\ngfx contact_on 0\n'
       'gfx terrain_normals 1\ngfx sun_lambert 1\ngfx terrain_brightness 1\ngfx terrain_contrast 1\n' +
       'gfx terrain_shadow_strength 1\n' + shot('cliff-full') +
       'gfx terrain_shadow_strength 0.5\n' + shot('cliff-half') +
       'gfx terrain_shadow_strength 0\n' + shot('cliff-zero') +
       'gfx terrain_shadow_strength 1\n' + shot('cliff-return'))
face = [pixels(name)[290:440,20:730].mean(axis=2) for name in
        ['cliff-full','cliff-half','cliff-zero','cliff-return']]
lifted = face[2] > face[0] + 8
assert lifted.sum() > 1000, 'terrain dial must visibly lift the dark cliff faces'
# Colour-grade clipping can leave a handful of texels black at half strength.
# Require monotonic response at every lifted texel and visible progression across
# both steps, rather than demanding sub-byte changes through that clip.
assert np.all(face[1][lifted] >= face[0][lifted])
assert np.all(face[2][lifted] >= face[1][lifted])
assert (face[1][lifted] > face[0][lifted]).sum() > 1000
assert (face[2][lifted] > face[1][lifted]).sum() > 1000
assert np.array_equal(face[0],face[3]), 'neutral cliff appearance must return exactly'
# These are actual painted dark rock faces that the saved colour grade clipped
# black. A cast/slope-only implementation leaves both points black at zero.
for y,x in [(360,130),(350,280)]:
    assert pixels('cliff-zero')[y,x].mean() > pixels('cliff-full')[y,x].mean()+12
render('cliff-objects', setup + 'cam 17 16\nzoom min\ngfx shadow_terrain 0\ngfx contact_on 0\n' +
       'gfx terrain_shadow_strength 1\n' + shot('objects-full') +
       'gfx terrain_shadow_strength 0\n' + shot('objects-zero'))
# Opaque Construction Yard roof and tank hull, away from animated effects/edges.
for ys,xs in [(slice(170,186),slice(232,256)),(slice(406,421),slice(360,374))]:
    assert np.array_equal(pixels('objects-full')[ys,xs],pixels('objects-zero')[ys,xs])
print('PASS: USER60 dark cliff faces respond progressively; buildings and tank keep their shading', flush=True)

render('colour', setup + 'cam 17 16\nzoom min\n' + shot('neutral') +
       'gfx terrain_brightness 1.5\n' + shot('bright') +
       'gfx terrain_brightness 1\ngfx terrain_contrast 0.5\n' + shot('contrast') +
       'gfx terrain_contrast 1\n' + shot('neutral-return') +
       'gfx terrain_normals 0\n' + shot('flat-neutral') +
       'gfx terrain_brightness 1.5\n' + shot('flat-bright'))
assert changed('neutral','bright') > 5000
assert changed('neutral','contrast') > 5000
assert changed('neutral','neutral-return') == 0
assert changed('flat-neutral','flat-bright') > 5000
# Compare the opaque sidebar interior; the rounded outside edge exposes terrain.
assert np.array_equal(pixels('neutral')[24:-24,870:980],pixels('bright')[24:-24,870:980])
assert np.array_equal(pixels('neutral')[24:-24,870:980],pixels('contrast')[24:-24,870:980])
print('PASS: both terrain colour controls respond, return to neutral and preserve HUD', flush=True)

cfg = out/'saved.cfg'
render('panel-save', setup + 'cam 17 16\n' +
       'gfxpanel 1 terrain_shadow_strength\n' + shot('panel') +
       'gfxpanel 1 terrain_brightness\ngfxpanel 1 terrain_contrast\ngfxpanel 0\n' +
       'gfx terrain_shadow_strength 0.35\ngfx terrain_brightness 1.25\ngfx terrain_contrast 0.75\n' +
       f'gfxsave {cfg}\n' + shot('saved') +
       'gfx terrain_shadow_strength 1\ngfx terrain_brightness 1\ngfx terrain_contrast 1\n' +
       f'gfxload {cfg}\n' + shot('loaded'))
assert changed('saved','loaded') == 0
for key,value in [('terrain_shadow_strength',0.35),('terrain_brightness',1.25),('terrain_contrast',0.75)]:
    m = re.search(r'^'+key+r'\s+([\d.]+)',cfg.read_text(),re.M)
    assert m and abs(float(m[1])-value)<0.001,key
print('PASS: all F5 rows reachable and saved settings reproduce the picture', flush=True)

render('hidden', setup + 'shrouddarktest\n' + shot('hidden-neutral') +
       'gfx terrain_brightness 2\ngfx terrain_contrast 0\nshrouddarktest\n' + shot('hidden-grade'), shroud=True)
# Water updates between script captures even without ticks. Compare against the
# same frame/command sequence with neutral controls, not its earlier water frame.
render('hidden-control', setup + 'shrouddarktest\n' + shot('hidden-control-first') +
       'gfx terrain_brightness 1\ngfx terrain_contrast 1\nshrouddarktest\n' +
       shot('hidden-control-second'), shroud=True)
assert changed('hidden-neutral','hidden-control-first') == 0
assert changed('hidden-grade','hidden-control-second') == 0
# Isolate the new source-art softener from pre-existing sun-map/face changes.
# A matching frame reference accounts for the water animation.
baked_hidden = (setup + 'gfx shadow_terrain 0\ngfx sun_lambert 0\ngfx terrain_normals 0\n'
                'gfx terrain_brightness 1.01\ngfx terrain_contrast 1\n')
for name,strength in [('hidden-baked',0),('hidden-baked-control',1)]:
    render(name, baked_hidden + 'gfx terrain_shadow_strength 1\nshrouddarktest\n' + shot(name+'-first') +
           f'gfx terrain_shadow_strength {strength}\nshrouddarktest\n' + shot(name+'-second'), shroud=True)
assert changed('hidden-baked-second','hidden-baked-control-second') == 0
render('classic', 'gfxmode 0\ntick 300\ngfx show_stats 0\n' + shot('classic-neutral') +
       'gfx terrain_brightness 2\ngfx terrain_contrast 0\ngfx terrain_shadow_strength 0\n' + shot('classic-dials'))
assert changed('classic-neutral','classic-dials') == 0
print('PASS: black shroud stays black; Classic is unchanged', flush=True)
