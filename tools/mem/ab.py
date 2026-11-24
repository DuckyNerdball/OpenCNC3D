#!/usr/bin/env python3
"""Run memory scenes with one or two renderers and compare every picture byte for byte.

    tools/mem/ab.py --play PLAY --bin A=path/cnc_eyes [--bin B=path/cnc_eyes]
                    [--env A:KEY=VALUE ...] [--env B:KEY=VALUE ...] [--flags B:'--switch']
                    [--scenes S1,S3] [--modes enh,cls,n64] [--runs 2] [--jobs 2]
                    [--ref DIR] --out DIR

A scene is a file tools/mem/scenes/<ID>.scene (see the README there): which game and
mission, which graphics modes it may run in, extra flags, and the script, in which
{OUT} is the run's own folder and {CFG} the frozen graphics settings of the mode.

PLAY is a play folder the runs start in. Each binary is copied into it once, as
cnc_eyes.<label>, so the engine library, the packs and SDL beside it are the folder's
and a binary is found there by the same paths a player's is. Every run is headless
(OPENCNC3D_BACKGROUND=1, a hidden window); nothing takes the screen.

For each scene, mode, binary and run it writes OUT/<label>/<scene>/<mode>/r<n>/: the
script, out.log, the shots, and result.json (exit status, the script's own failure
count, wall time, each shot's SHA-1, the MEM|ledger and TEXHASH lines). Then it compares:

  - with two binaries: A's shots against B's, shot by shot;
  - with --ref DIR: every binary's shots against an earlier OUT's shots.txt (the value most
    of its runs gave; a reference shot with more than one value prints a REFFLAKE| line);
  - with --runs 2 or more: every run of a binary against its others (determinism; a
    shot that takes more than one value is printed as a FLAKE| line).

When a scene and mode differ anywhere, it is run --settle more times per binary, one
game at a time, and every shot is judged on the value most of its runs give (see
SETTLING in the code for why). It prints one SHOT| line per comparison, FLAKE| lines,
and one AB|summary line. OUT/shots.txt lists "<label> <scene> <mode> r<n> <shot> <sha1>"
for every shot, for use as a --ref later. Exit status 0 only when every run finished
cleanly and every comparison was equal.

Runs go one at a time by default (--jobs 1): two games drawing at once were measured to
flip pixels. Each run records how many other game processes were running (others=),
because another agent's game on the same GPU can do the same.

A binary that has no `memledger` verb (any build older than the ledger) gets the
scene's memledger lines removed from its script; the verb draws nothing, so the
pictures are comparable either way.
"""
import argparse, collections, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
SCENES = os.path.join(HERE, 'scenes')
EXTRA = ''        # --scenedir: a second folder of scenes, looked in after this one

RA = ('--ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dir ra3d/missions/ '
      '--dosdata ra3d/audio --scen {scen} --pack ra3d/3d/{scen}.pack')
TD = ('--scen {scen} --pack {scen}.pack --dylib TiberianDawn.dylib --content content/ '
      '--dir missions/ --dosdata dosdata --cameos cameos.pack --dospack dossidebar.pack '
      '--dosinf dosinfantry.pack')
MODES = {
    # graphics mode -> (flags, cfg file per game). Enhanced is what a player gets with the
    # Enhanced art; Classic and N64 are cold starts, the way a player starts in them.
    'enh': ('--enhanced --texset 3', {'ra': 'ra.cfg', 'td': 'td.cfg'}),
    'cls': ('', {'ra': 'ra-classic.cfg', 'td': 'td-classic.cfg'}),
    'n64': ('', {'ra': 'ra-n64.cfg', 'td': 'td-n64.cfg'}),
}
COMMON = '--nosound --w 1600 --h 900'


def read_scene(sid, seen=()):
    path = os.path.join(SCENES, sid + '.scene')
    if not os.path.exists(path) and EXTRA and os.path.exists(os.path.join(EXTRA, sid + '.scene')):
        path = os.path.join(EXTRA, sid + '.scene')
    head, body, inbody = {}, [], False
    for line in open(path, encoding='utf-8'):
        line = line.rstrip('\n')
        if inbody:
            body.append(line)
        elif line.strip() == '---':
            inbody = True
        elif line.strip() and not line.lstrip().startswith('#'):
            k, _, v = line.partition(':')
            head[k.strip()] = v.strip()
    if 'like' in head:
        if head['like'] in seen:
            raise SystemExit('scene %s: like-loop' % sid)
        base = read_scene(head['like'], seen + (sid,))
        merged = dict(base['head'])
        merged.update({k: v for k, v in head.items() if k != 'like'})
        if not body:
            body = base['body']
        head = merged
    return {'id': sid, 'head': head, 'body': body}


def scene_ids():
    ids = [f[:-6] for f in os.listdir(SCENES) if f.endswith('.scene')]
    return sorted(ids, key=lambda s: (len(s), s))


def has_memledger(binpath):
    data = open(binpath, 'rb').read()
    return b'memledger_scene' in data or b'MEM|ledger|' in data


def sha1(path):
    h = hashlib.sha1()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def count_games():
    """game processes drawing on this machine right now, other than this script's own runs
    in flight (each run sees the others' runs when --jobs is above 1)"""
    try:
        ps = subprocess.check_output(['ps', '-axo', 'comm'], text=True)
    except Exception:
        return -1
    n = 0
    for line in ps.splitlines():
        base = os.path.basename(line.strip())
        if base.startswith('cnc_eyes') or base.startswith('opencnc3d'):
            n += 1
    return max(0, n - 1)


def one_run(job):
    (label, exe, envs, xflags, scene, mode, run, out, play, cfgdir, keep_ledger) = job
    rd = os.path.join(out, label, scene['id'], mode, 'r%d' % run)
    if os.path.isdir(rd):
        shutil.rmtree(rd)
    os.makedirs(os.path.join(rd, 'saves'))
    head = scene['head']
    game = head.get('game', 'ra')
    flags, cfgs = MODES[mode]
    cfg = os.path.join(cfgdir, cfgs[game])
    lines = []
    extra = [x.strip() for x in head.get('extra', '').split(';') if x.strip()]
    for l in scene['body']:
        if not keep_ledger and re.match(r'\s*memledger\b', l):
            continue
        lines.append(l.replace('{OUT}', rd).replace('{CFG}', cfg))
        if extra and re.match(r'\s*gfxload\b', l):
            lines += extra          # a scene's own settings go after the frozen ones
            extra = []
    script = os.path.join(rd, 'script.txt')
    with open(script, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    base = (RA if game == 'ra' else TD).format(scen=head['scen'])
    seed = head.get('seed', '12345')
    build = head.get('build', '98')
    build = '' if build == 'none' else '--build %s' % build
    cmd = ('./%s %s %s %s --seed %s %s %s --gfx %s --gfxsave %s/private.cfg --savedir %s/saves --script %s'
           % (exe, base, build, COMMON, seed, flags, (head.get('flags', '') + ' ' + xflags).strip(), cfg, rd, rd, script))
    env = dict(os.environ)
    env['OPENCNC3D_BACKGROUND'] = '1'
    for k, v in envs:
        env[k] = v
    for k, v in (kv.split('=', 1) for kv in head.get('env', '').split() if '=' in kv):
        env[k] = v
    with open(os.path.join(rd, 'cmd.txt'), 'w') as f:
        f.write(cmd + '\n' + '\n'.join('%s=%s' % kv for kv in envs) + '\n')
    others = count_games()
    t0 = time.time()
    with open(os.path.join(rd, 'out.log'), 'w') as log:
        rc = subprocess.call(['nice', '-n', '10', 'perl', '-e', 'alarm 1500; exec @ARGV', 'sh', '-c', cmd],
                             cwd=play, env=env, stdout=log, stderr=subprocess.STDOUT)
    wall = time.time() - t0
    others = max(others, count_games())
    text = open(os.path.join(rd, 'out.log'), encoding='latin-1').read()
    m = re.search(r'^SCRIPT\|end .*: (\d+) lines, (\d+) failures', text, re.M)
    shots = {}
    for f in sorted(os.listdir(rd)):
        if f.endswith('.png'):
            shots[f] = sha1(os.path.join(rd, f))
    want = [re.search(r'\{OUT\}/(\S+\.png)', l).group(1) for l in scene['body']
            if re.match(r'\s*shot\s', l) and '{OUT}/' in l]
    res = {
        'label': label, 'scene': scene['id'], 'mode': mode, 'run': run, 'rc': rc,
        'script_failures': int(m.group(2)) if m else None, 'wall_s': round(wall, 1),
        'shots': shots, 'missing_shots': [w for w in want if w not in shots],
        'ledger': re.findall(r'^MEM\|ledger\|.*$', text, re.M),
        'texhash': re.findall(r'^TEXHASH\|.*$', text, re.M),
        'fatal': re.findall(r'^FATAL.*$', text, re.M),
        'others': others,
    }
    res['clean'] = (rc == 0 and res['script_failures'] == 0 and not res['missing_shots']
                    and not res['fatal'])
    with open(os.path.join(rd, 'result.json'), 'w') as f:
        json.dump(res, f, indent=1)
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--play', required=True)
    ap.add_argument('--bin', action='append', required=True, help='LABEL=path')
    ap.add_argument('--env', action='append', default=[], help='LABEL:KEY=VALUE')
    ap.add_argument('--flags', action='append', default=[], help="LABEL:'extra command-line flags'")
    ap.add_argument('--scenes', default='')
    ap.add_argument('--modes', default='', help='only these modes (each scene keeps to its own list)')
    ap.add_argument('--runs', type=int, default=1)
    ap.add_argument('--jobs', type=int, default=1,
                    help='games at once; more than 1 was measured to flip pixels on a Mac (see SETTLING)')
    ap.add_argument('--settle', type=int, default=2, help='reruns per binary of a group that differs')
    ap.add_argument('--ref', default='')
    ap.add_argument('--cfgdir', default=os.path.join(SCENES, 'cfg'))
    ap.add_argument('--scenedir', default='', help='a second folder of .scene files')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    global EXTRA
    EXTRA = os.path.abspath(a.scenedir) if a.scenedir else ''
    play = os.path.abspath(a.play)
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    bins = []
    for b in a.bin:
        label, _, path = b.partition('=')
        exe = 'cnc_eyes.%s' % label
        dst = os.path.join(play, exe)
        if os.path.exists(dst):
            os.remove(dst)
        subprocess.check_call(['cp', '-c', os.path.abspath(path), dst])
        envs = [tuple(e.split(':', 1)[1].split('=', 1)) for e in a.env if e.split(':', 1)[0] == label]
        xflags = ' '.join(f.split(':', 1)[1] for f in a.flags if f.split(':', 1)[0] == label)
        bins.append((label, exe, envs, xflags, has_memledger(dst), sha1(dst)))
    ids = [s for s in a.scenes.split(',') if s] or scene_ids()
    only = [m for m in a.modes.split(',') if m]
    jobs = []
    for sid in ids:
        sc = read_scene(sid)
        modes = [m for m in sc['head'].get('modes', 'enh').split(',') if m]
        if only:
            modes = [m for m in modes if m in only]
        for mode in modes:
            for run in range(1, a.runs + 1):
                for (label, exe, envs, xflags, ledger, _) in bins:
                    jobs.append((label, exe, envs, xflags, sc, mode, run, out, play, os.path.abspath(a.cfgdir), ledger))
    with open(os.path.join(out, 'bins.txt'), 'w') as f:
        for (label, exe, envs, xflags, ledger, digest) in bins:
            f.write('%s %s memledger=%d env=%s flags=%s\n' % (label, digest, ledger, ','.join('%s=%s' % e for e in envs), xflags))
    t0 = time.time()
    results = []

    def run_all(joblist, workers):
        done = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, workers)) as pool:
            for res in pool.map(one_run, joblist):
                done.append(res)
                print('RUN|%s|%s|%s|r%d|rc=%s|fail=%s|shots=%d|missing=%s|others=%d|%.0fs|%s' % (
                    res['label'], res['scene'], res['mode'], res['run'], res['rc'], res['script_failures'],
                    len(res['shots']), ','.join(res['missing_shots']) or '-', res['others'], res['wall_s'],
                    'clean' if res['clean'] else 'NOT CLEAN'), flush=True)
        return done

    results += run_all(jobs, a.jobs)
    labels = [b[0] for b in bins]
    ref = {}
    if a.ref:
        # the reference value of a shot is the one most of the reference's runs gave
        votes = {}
        for line in open(os.path.join(a.ref, 'shots.txt')):
            lab, sid, mode, run, shot, digest = line.split()
            votes.setdefault((sid, mode, shot), collections.Counter())[digest] += 1
        for (sid, mode, shot), c in votes.items():
            (best, n), = c.most_common(1)
            if len(c) > 1:
                print('REFFLAKE|%s|%s|%s|%s' % (sid, mode, shot, ','.join('%s*%d' % (k[:12], m) for k, m in c.most_common())))
            ref.setdefault((sid, mode), {})[shot] = best

    def runs_of(label, sid, mode):
        return [r for r in results if (r['label'], r['scene'], r['mode']) == (label, sid, mode)]

    def raw_diffs():
        """(sid, mode) groups where some comparison differs on its first runs"""
        bad = set()
        for r in results:
            key = (r['scene'], r['mode'])
            first = next((x for x in runs_of(r['label'], *key) if x['run'] == 1), None)
            if first and r['shots'] != first['shots']:
                bad.add(key)
            if r['run'] == 1 and len(labels) >= 2 and r['label'] != labels[0]:
                b0 = next((x for x in runs_of(labels[0], *key) if x['run'] == 1), None)
                if b0 and b0['shots'] != r['shots']:
                    bad.add(key)
            if r['run'] == 1 and a.ref and ref.get(key, {}) != r['shots']:
                bad.add(key)
        return bad

    # SETTLING. Two game processes drawing at once on one Mac's GPU have been measured to
    # flip one to a few hundred pixels by one or two levels in a small share of shots, at
    # random; one process at a time has not. A group with any difference is therefore run
    # `settle` more times per binary, serially, and a shot is judged on the value most of
    # its runs give. Every such rerun is printed, so a settled verdict is never silent.
    settled = sorted(raw_diffs())
    if settled and a.settle > 0:
        print('AB|settle|groups=%s|reruns=%d' % (','.join('%s/%s' % k for k in settled), a.settle), flush=True)
        more = []
        for (sid, mode) in settled:
            sc = read_scene(sid)
            for (label, exe, envs, xflags, ledger, _) in bins:
                have = max(x['run'] for x in runs_of(label, sid, mode))
                for k in range(1, a.settle + 1):
                    more.append((label, exe, envs, xflags, sc, mode, have + k, out, play, os.path.abspath(a.cfgdir), ledger))
        results += run_all(more, 1)

    with open(os.path.join(out, 'shots.txt'), 'w') as f:
        for r in sorted(results, key=lambda r: (r['label'], r['scene'], r['mode'], r['run'])):
            for shot, digest in sorted(r['shots'].items()):
                f.write('%s %s %s r%d %s %s\n' % (r['label'], r['scene'], r['mode'], r['run'], shot, digest))

    def majority(label, sid, mode, shot):
        c = collections.Counter(r['shots'].get(shot) for r in runs_of(label, sid, mode))
        c.pop(None, None)
        return (c.most_common(1)[0][0] if c else None), c

    equal = differ = missing = flaky = 0
    groups = sorted(set((r['scene'], r['mode']) for r in results))
    for (sid, mode) in groups:
        shots = set()
        for r in results:
            if (r['scene'], r['mode']) == (sid, mode):
                shots |= set(r['shots'])
        shots |= set(ref.get((sid, mode), {}))
        for shot in sorted(shots):
            maj, counts = {}, {}
            for label in labels:
                v, c = majority(label, sid, mode, shot)
                maj[label] = v
                counts[label] = c
                if len(c) > 1:
                    flaky += 1
                    print('FLAKE|%s|%s|%s|%s|%s' % (label, sid, mode, shot,
                          ','.join('%s*%d' % (k[:12], n) for k, n in c.most_common())))
            pairs = []
            if len(labels) >= 2:
                pairs += [('ab', labels[0], maj[labels[0]], l, maj[l]) for l in labels[1:]]
            if a.ref:
                pairs += [('ref', 'ref', ref.get((sid, mode), {}).get(shot), l, maj[l]) for l in labels]
            if not pairs and a.runs >= 2:
                # one binary run several times: every run must give the same bytes
                for label in labels:
                    c = counts[label]
                    v = 'same' if len(c) == 1 else 'DIFF'
                    if v == 'same': equal += 1
                    else: differ += 1
                    print('SHOT|determinism|%s/%s|%s|%s|values=%d|%s' % (sid, mode, shot, label, len(c), v))
            for (tag, x, p, y, q) in pairs:
                if p is None or q is None:
                    missing += 1; verdict = 'MISSING'
                elif p == q:
                    equal += 1; verdict = 'same'
                else:
                    differ += 1; verdict = 'DIFF'
                print('SHOT|%s|%s/%s|%s|%s=%s|%s=%s|%s' % (tag, sid, mode, shot, x, (p or '-')[:12],
                                                          y, (q or '-')[:12], verdict))
    unclean = [r for r in results if not r['clean']]
    print('AB|summary|runs=%d|unclean=%d|compared_equal=%d|differ=%d|missing=%d|flaky_shots=%d|settled_groups=%d|wall=%.0fs' % (
        len(results), len(unclean), equal, differ, missing, flaky, len(settled), time.time() - t0))
    with open(os.path.join(out, 'summary.json'), 'w') as f:
        json.dump({'runs': len(results), 'unclean': [(r['label'], r['scene'], r['mode'], r['run']) for r in unclean],
                   'equal': equal, 'differ': differ, 'missing': missing, 'flaky_shots': flaky,
                   'settled_groups': ['%s/%s' % k for k in settled],
                   'wall': {'%s/%s/%s/r%d' % (r['label'], r['scene'], r['mode'], r['run']): r['wall_s'] for r in results}},
                  f, indent=1)
    return 0 if not unclean and not differ and not missing else 1


if __name__ == '__main__':
    sys.exit(main())
