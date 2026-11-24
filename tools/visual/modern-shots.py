#!/usr/bin/env python3
"""Shoot the fixed before/after scene set for the modern lighting work.

Every scene is a hidden --script run of cnc_eyes from a play folder (the data is the
play folder's; the binary is whichever one is named), at a fixed camera, seed and tick
count, so two binaries shot with the same arguments differ only by what they draw.

    modern-shots.py --binary <cnc_eyes> --play <play folder> --out <dir>
                    [--scenes td_coast,ra_snow,td_rain] [--gfx "key value" ...] [--jobs 4]
                    [--w 1600 --h 900] [--td-cfg F] [--ra-cfg F]

The graphics settings are COPIED out of the play folder (opencnc3d-fx.cfg for Tiberian
Dawn, ra3d-fx.cfg for Red Alert) into the output folder before anything runs, and the
runs save only into the output folder, so the play folder's own settings are never
written. --gfx lines are applied after the saved settings load, which is how one
feature is switched off for an A/B.

Writes <out>/<scene>.png (and <scene>_<n>.png for scenes with several moments), one log
per scene and <out>/shots.json with each run's exit code and command line.
"""
import argparse, json, os, shutil, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

TD = dict(dylib='TiberianDawn.dylib', content='content/', dir='missions/', dosdata='dosdata', ra=False)
RA = dict(dylib='ra3d/RedAlert.dylib', content='ra3d/data', dir='ra3d/missions/', dosdata='ra3d/audio', ra=True)

# scene -> (game, scenario, pack, extra arguments, script steps). A step 'SHOT' writes the
# next numbered picture; a scene with one SHOT writes <scene>.png.
SCENES = {
    'td_coast':      (TD, 'SCG01EA', 'SCG01EA.pack', [], ['tick 60', 'reveal 1', 'cam 47 53', 'dist 3000', 'SHOT']),
    'td_forest':     (TD, 'SCG05EA', 'SCG05EA.pack', [], ['tick 60', 'reveal 1', 'cam 20 29', 'dist 3200', 'SHOT']),
    'td_base':       (TD, 'SCG08EA', 'SCG08EA.pack', [], ['tick 60', 'reveal 1', 'cam 47 48', 'dist 3000', 'SHOT']),
    'td_base_close': (TD, 'SCG08EA', 'SCG08EA.pack', [], ['tick 60', 'reveal 1', 'cam 47 48', 'dist 1500', 'SHOT']),
    'td_desert':     (TD, 'SCB01EA', 'SCB01EA.pack', [], ['tick 60', 'reveal 1', 'cam 39 26', 'dist 3200', 'SHOT']),
    'td_desert_base':(TD, 'SCB05EA', 'SCB05EA.pack', [], ['tick 60', 'reveal 1', 'cam 21 18', 'dist 3000', 'SHOT']),
    'td_winter':     (TD, 'SCW01EA', 'SCW01EA.pack', [], ['tick 60', 'reveal 1', 'cam 32 32', 'dist 3400', 'SHOT']),
    'td_nuke':       (TD, 'SCB20EA', 'SCB20EA.pack', ['--noshroud'],
                      ['tick 30', 'cam 50 12', 'dist 3000', 'hurtobj NUK2', 'tick 6', 'SHOT', 'tick 40', 'SHOT']),
    'td_wreck':      (TD, 'SCG90EA', 'SCG01EA.pack', ['--noshroud'],
                      ['tick 30', 'cam 54 50', 'dist 1800', 'hurtobj MTNK 0', 'tick 1', 'hurtobj MTNK 0',
                       'tick 20', 'SHOT', 'tick 60', 'SHOT']),
    'ra_ant':        (RA, 'SCA01EA', 'ra3d/3d/SCA01EA.pack', [], ['tick 60', 'reveal 1', 'cam 63 56', 'dist 3600', 'SHOT']),
    'ra_temperate':  (RA, 'SCG05EA', 'ra3d/3d/SCG05EA.pack', [], ['tick 60', 'reveal 1', 'cam 40 83', 'dist 3400', 'SHOT']),
    'ra_snow':       (RA, 'SCG03EA', 'ra3d/3d/SCG03EA.pack', [], ['tick 60', 'reveal 1', 'cam 80 62', 'dist 3400', 'SHOT']),
    'ra_snow_close': (RA, 'SCG03EA', 'ra3d/3d/SCG03EA.pack', [], ['tick 60', 'reveal 1', 'cam 80 62', 'dist 1600', 'SHOT']),
    'ra_interior':   (RA, 'SCA04EA', 'ra3d/3d/SCA04EA.pack', [], ['tick 60', 'reveal 1', 'cam 40 55', 'dist 2600', 'SHOT']),
    'ra_skirmish':   (RA, 'SCM01EA', 'ra3d/3d/SCM01EA.pack', ['--skirmish', '--ai', '7', '--noshroud'],
                      ['tick 200', 'SHOT']),
}

# Scenes shot only when named with --scenes. td_rain is td_base under heavy rain, the one
# view that reaches the rain's air, puddles and overcast in the light pass: the scene set
# above has no rain, so an identity check over it alone cannot see those paths.
EXTRA_SCENES = {
    'td_rain':       (TD, 'SCG08EA', 'SCG08EA.pack', [],
                      ['gfx rain_fx 1', 'gfx rain_amount 1', 'gfx rain_haze 1', 'gfx rain_overcast 0.6',
                       'gfx rain_puddle 1', 'gfx rain_wet 1',
                       'tick 60', 'reveal 1', 'cam 47 48', 'dist 3000', 'SHOT']),
    # ra_snow_close twice, one engine tick apart: the snowfall's motion between two
    # consecutive settled frames (each flake should have moved a few pixels, no trails).
    'ra_snow_motion':(RA, 'SCG03EA', 'ra3d/3d/SCG03EA.pack', [],
                      ['tick 60', 'reveal 1', 'cam 80 62', 'dist 1600', 'SHOT', 'SHOT']),
}
ALL_SCENES = dict(SCENES, **EXTRA_SCENES)


def run_scene(name, a, out):
    game, scen, pack, extra, steps = ALL_SCENES[name]
    d = os.path.join(out, 'run', name)
    os.makedirs(os.path.join(d, 'saves'), exist_ok=True)
    cfg = os.path.join(out, 'ra.cfg' if game['ra'] else 'td.cfg')
    nshots = steps.count('SHOT')
    lines = ['gfxload ' + cfg, 'gfx show_stats 0'] + ['gfx ' + g for g in a.gfx]
    k = 0
    for s in steps:
        if s == 'SHOT':
            k += 1
            png = os.path.join(out, name + ('.png' if nshots == 1 else '_%d.png' % k))
            if os.path.exists(png):
                os.remove(png)
            lines += ['smoothmove 1', 'tick 1', 'tickalpha 0', 'shot ' + png]
        else:
            lines.append(s)
    lines.append('quit')
    script = os.path.join(d, 'script.txt')
    with open(script, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    args = [os.path.abspath(a.binary), '--scen', scen, '--pack', pack, '--dylib', game['dylib'],
            '--content', game['content'], '--dir', game['dir'], '--dosdata', game['dosdata'],
            '--build', '98', '--seed', '12345', '--enhanced', '--nosound',
            '--w', str(a.w), '--h', str(a.h), '--gfx', cfg,
            '--gfxsave', os.path.join(d, 'private.cfg'), '--savedir', os.path.join(d, 'saves'),
            '--script', script] + extra
    if game['ra']:
        args.append('--ra3d')
    else:
        args += ['--cameos', 'cameos.pack', '--dospack', 'dossidebar.pack', '--dosinf', 'dosinfantry.pack',
                 '--texset', '3']
    log = os.path.join(out, name + '.log')
    with open(log, 'w') as f:
        rc = subprocess.call(args, cwd=a.play, stdout=f, stderr=subprocess.STDOUT, timeout=900)
    return dict(scene=name, rc=rc, log=log, command=args)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--binary', required=True)
    p.add_argument('--play', required=True)
    p.add_argument('--out', required=True)
    p.add_argument('--scenes', default=','.join(SCENES))
    p.add_argument('--gfx', action='append', default=[])
    p.add_argument('--jobs', type=int, default=3)
    p.add_argument('--w', type=int, default=1600)
    p.add_argument('--h', type=int, default=900)
    p.add_argument('--td-cfg')
    p.add_argument('--ra-cfg')
    a = p.parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    shutil.copyfile(a.td_cfg or os.path.join(a.play, 'opencnc3d-fx.cfg'), os.path.join(out, 'td.cfg'))
    shutil.copyfile(a.ra_cfg or os.path.join(a.play, 'ra3d-fx.cfg'), os.path.join(out, 'ra.cfg'))
    names = [s for s in a.scenes.split(',') if s]
    bad = [s for s in names if s not in ALL_SCENES]
    if bad:
        sys.exit('unknown scene(s): ' + ', '.join(bad))
    with ThreadPoolExecutor(a.jobs) as ex:
        res = list(ex.map(lambda n: run_scene(n, a, out), names))
    with open(os.path.join(out, 'shots.json'), 'w') as f:
        json.dump(res, f, indent=1)
    for r in res:
        print('%-15s rc=%d' % (r['scene'], r['rc']))
    sys.exit(1 if any(r['rc'] for r in res) else 0)


if __name__ == '__main__':
    main()
