#!/usr/bin/env python3
"""Verify a selected RA INI wins over the installed mission of the same name."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--brain', type=Path)
    parser.add_argument('--baseline-exe', type=Path)
    parser.add_argument('--fixture-source', type=Path,
                        help='Optional root with the two original G343 mission INIs')
    parser.add_argument('--pack', type=Path, help='Optional original Allied 5 pack')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    runtime, out = args.runtime.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    brain = (args.brain or runtime / 'ra3d/RedAlert.dylib').resolve()
    fixture_source = (args.fixture_source or runtime).resolve()
    pack = (args.pack or runtime / 'ra3d/3d/SCG05EA.pack').resolve()
    env = {**os.environ, 'OPENCNC3D_BACKGROUND': '1'}
    fixture = out / 'fixture'
    subprocess.run([sys.executable, str(ROOT / 'game/gate_ra3dvehicles.py'),
                    'script', str(fixture)], cwd=fixture_source, env=env, check=True)
    isolated = out / 'mission-only'
    isolated.mkdir(exist_ok=True)
    shutil.copy2(fixture / 'mt/SCG05EA.INI', isolated / 'SCG05EA.INI')
    types = ['1TNK:Greece', 'ARTY:Greece', 'APC:USSR', 'HELI:USSR']
    script = out / 'script.txt'
    script.write_text('tick 1\n' + ''.join(f'count {t} 1\n' for t in types) + 'quit\n')
    cases = []
    targets = [('selected-folder', args.exe, fixture / 'mt', True),
               ('shared-rules-fallback', args.exe, isolated, True)]
    if args.baseline_exe:
        targets.insert(0, ('negative-control', args.baseline_exe, fixture / 'mt', False))
    for name, executable, missions, positive in targets:
        dest = out / name
        (dest / 'saves').mkdir(parents=True, exist_ok=True)
        executable = executable.resolve()
        command = [str(executable), '--ra3d', '--dylib', str(brain),
                   '--content', 'ra3d/data/', '--dosdata', 'ra3d/audio/',
                   '--dir', str(missions) + '/', '--scen', 'SCG05EA',
                   '--pack', str(pack), '--nosound', '--noshroud',
                   '--w', '640', '--h', '480', '--savedir', str(dest / 'saves'),
                   '--gfxsave', str(dest / 'session.cfg'),
                   '--visualscfg', str(dest / 'visuals.cfg'), '--script', str(script)]
        with (dest / 'run.log').open('w') as log:
            result = subprocess.run(command, cwd=runtime, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=180)
        text = (dest / 'run.log').read_text(errors='replace')
        expected = [f'COUNT|{t}|want=1|have={1 if positive else 0}|'
                    f'{"PASS" if positive else "FAIL"}' for t in types]
        assert result.returncode == (0 if positive else 1), (name, result.returncode)
        assert all(line in text for line in expected), name
        assert f'6 lines, {0 if positive else 4} failures' in text, name
        cases.append(dict(case=name, passed=True, exit=result.returncode,
                          executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                          assertions=expected))
        print(name, 'passed', flush=True)
    (out / 'results.json').write_text(json.dumps(dict(passed=True, cases=cases), indent=2) + '\n')


if __name__ == '__main__':
    main()
