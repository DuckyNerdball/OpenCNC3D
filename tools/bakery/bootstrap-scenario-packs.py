#!/usr/bin/env python3
"""Bake the explicit runtime scenario set from tracked cartridge terrain inputs."""
import argparse
import builtins
import concurrent.futures
import importlib.util
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def worker(root, codes, report):
    # Only the legacy derived role report is redirected; inputs and bake math stay unchanged.
    original_open = builtins.open
    role_path = root / 'tools/bakery/parts_roles.json'
    def output_open(path, *args, **kwargs):
        mode = args[0] if args else kwargs.get('mode', 'r')
        if isinstance(path, (str, Path)) and Path(path).resolve() == role_path and any(c in mode for c in 'wax'):
            path = report
        return original_open(path, *args, **kwargs)
    report.parent.mkdir(parents=True, exist_ok=True)
    builtins.open = output_open
    spec = importlib.util.spec_from_file_location('runtime_bake5', root / 'tools/bakery/bake5.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    for entry in codes:
        code, source = alias(entry)
        module.build(source, str(root / 'tools/bakery/game' / (code + '.pack')),
                     label=code if source != code else None)


def alias(entry):
    """A manifest line is CODE, or TARGET=SOURCE: the pack TARGET.pack baked from the
    cartridge scene SOURCE, for a mission installed under a code other than its own."""
    code, _, source = entry.partition('=')
    return code, source or code


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    p.add_argument('--jobs', type=int, default=min(4, os.cpu_count() or 1))
    p.add_argument('--worker', type=int, help=argparse.SUPPRESS)
    a = p.parse_args()
    assert 1 <= a.jobs <= 16
    root = a.root.resolve()
    manifest = Path(__file__).with_name('runtime-scenarios.txt')
    scenarios = [line for line in manifest.read_text().splitlines() if line and not line.startswith('#')]
    codes = [alias(entry)[0] for entry in scenarios]
    assert len(codes) == len(set(codes)) == 95
    groups = [scenarios[i::a.jobs] for i in range(a.jobs)]
    if a.worker is not None:
        assert 0 <= a.worker < a.jobs
        worker(root, groups[a.worker], root / ('build/base-runtime-packs/worker-%d/parts_roles.json' % a.worker))
        return
    bakery = root / 'tools/bakery'
    extracted = bakery / 'sharecopy/assets/extracted'
    terrain = bakery / 'sharecopy/assets/terrain'
    for code, source in map(alias, scenarios):
        for path in [extracted / 'MAP' / (source + '.MAP'), extracted / 'INI' / (source + '.INI'),
                     root / 'game/missions' / (code + '.INI')]:
            assert path.is_file(), 'missing source scenario input: ' + str(path)
            subprocess.run(['git', 'ls-files', '--error-unmatch', str(path.relative_to(root))],
                           cwd=root, check=True, stdout=subprocess.DEVNULL)
    links = [(terrain / 'share/OpenCNC3D/assets/extracted', '../../../../extracted'),
             (terrain / 'share/OpenCNC3D/assets/terrain', '../../../../terrain'),
             (bakery / 'support/share/OpenCNC3D', '../../sharecopy')]
    for link, target in links:
        if link.is_symlink():
            assert os.readlink(link) == target, 'refusing to retarget an unrelated scaffold'
        else:
            assert not link.exists(), 'scaffold path is not a symlink'
            link.parent.mkdir(parents=True, exist_ok=True)
            link.symlink_to(target)
    output = bakery / 'game'
    output.mkdir(exist_ok=True)
    sources = list(dict.fromkeys(alias(entry)[1] for entry in scenarios))
    subprocess.run([sys.executable, str(terrain / 'n64_terrain.py'), *sources], cwd=root, check=True)
    def batch(index):
        log = root / ('build/base-runtime-packs/scenarios-worker-%d.log' % index)
        log.parent.mkdir(parents=True, exist_ok=True)
        with log.open('w') as stream:
            subprocess.run([sys.executable, str(Path(__file__).resolve()), '--root', str(root),
                            '--jobs', str(a.jobs), '--worker', str(index)], cwd=root, check=True,
                           stdout=stream, stderr=subprocess.STDOUT)
        return index
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for completed in concurrent.futures.as_completed([pool.submit(batch, i) for i in range(a.jobs)]):
            index = completed.result()
            print('SCENARIO-BATCH|%d|%d packs|PASS' % (index, len(groups[index])), flush=True)
    records = []
    for code in codes:
        pack = output / (code + '.pack')
        assert pack.is_file() and pack.stat().st_size > 0
        records.append(dict(scenario=code, bytes=pack.stat().st_size,
                            sha256=hashlib.sha256(pack.read_bytes()).hexdigest()))
    receipt = root / 'build/base-runtime-packs/scenarios.json'
    receipt.parent.mkdir(parents=True, exist_ok=True)
    receipt.write_text(json.dumps(dict(source=subprocess.check_output(['git', 'rev-parse', 'HEAD'],
        cwd=root, text=True).strip(), scenarios=records, count=len(records), installed_inputs_used=False),
        indent=2) + '\n')
    print('SCENARIO-PACKS|%d current-source scenarios|PASS' % len(records))


if __name__ == '__main__':
    main()
