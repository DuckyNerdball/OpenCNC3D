#!/usr/bin/env python3
"""Replay qualified original coast scripts with each optional-cache refusal."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

FAULTS = {
    'array-load': 'OPENCNC3D_WATER_ARRAY_LOAD_OK',
    'array-alloc': 'OPENCNC3D_WATER_ARRAY_ALLOC_OK',
    'array-store': 'OPENCNC3D_WATER_ARRAY_STORE_OK',
    'array-check': 'OPENCNC3D_WATER_ARRAY_CHECK_OK',
    'array-budget': 'OPENCNC3D_WATER_ARRAY_BUDGET_FULL',
    'film-alloc': 'OPENCNC3D_SHORE_FILM_ALLOC_OK',
    'film-budget': 'OPENCNC3D_SHORE_FILM_BUDGET_FULL',
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', required=True, type=Path)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--baseline-proof', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    runtime, exe, out = args.runtime.resolve(), args.exe.resolve(), args.out.resolve()
    qualified = json.loads(args.baseline_proof.read_text())
    assert len(qualified) == 6 and all(row['passed'] for row in qualified)
    assert not out.exists(), 'Use a fresh output directory'
    out.mkdir(parents=True)
    rows = []
    for reference in qualified:
        if reference['mode'] != 'original':
            continue
        original = reference['command']
        source_script = Path(original[original.index('--script') + 1])
        source_folder = source_script.parent
        assert sha(source_script) == reference['script_sha256']
        assert sha(original[original.index('--dylib') + 1]) == reference['engine_sha256']
        picture_names = [line[5:] for line in source_script.read_text().splitlines() if line.startswith('shot ')]
        assert [sha(path) for path in picture_names] == reference['pictures']
        for label, variable in FAULTS.items():
            folder = out / reference['game'] / label
            folder.mkdir(parents=True)
            script = folder / 'script.txt'
            script.write_text(source_script.read_text().replace(str(source_folder), str(folder)).replace('\nquit\n', '\nshorecachedump\nquit\n'))
            command = [str(exe)] + [value.replace(str(source_folder), str(folder)) for value in original[1:]]
            command[command.index('--script') + 1] = str(script)
            env = {**os.environ, 'OPENCNC3D_BACKGROUND': '1',
                   'OPENCNC3D_MESH_RIGID_ATTRIBUTES': '1',
                   'OPENCNC3D_SHORE_FILM_CACHE': '1', 'OPENCNC3D_WATER_ARRAY_CACHE': '1', variable: '0'}
            with (folder / 'run.log').open('w') as log:
                run = subprocess.run(command, cwd=runtime, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=600)
            text = (folder / 'run.log').read_text(errors='replace')
            assert run.returncode == 0 and re.search(r'SCRIPT\|end .*0 failures', text), (label, text[-1500:])
            counters = {}
            for name, cap in [('SHOREFILMCACHE', 32 * 1024 * 1024), ('WATERARRAYCACHE', 16 * 1024 * 1024)]:
                matches = re.findall(r'^' + name + r'\|offers=(\d+)\|hits=(\d+)\|verified=(\d+)\|bad=(\d+)\|fallbacks=(\d+)\|bytes=(\d+)', text, re.M)
                assert matches and all(int(row[3]) == 0 and int(row[5]) <= cap for row in matches)
                counters[name] = list(map(int, matches[-1]))
            selected = counters['WATERARRAYCACHE' if label.startswith('array-') else 'SHOREFILMCACHE']
            assert selected[0] > 100 and selected[4] > 100, selected
            pictures = [sha(path.replace(str(source_folder), str(folder))) for path in picture_names]
            assert pictures == reference['pictures'], (reference['game'], label, 'Pictures differ')
            row = dict(game=reference['game'], fault=label, passed=True, counters=counters,
                       pictures=pictures, exe_sha256=sha(exe), script_sha256=sha(script), run_sha256=sha(folder / 'run.log'))
            rows.append(row)
            (out / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
            print(json.dumps(dict(game=row['game'], fault=label, passed=True)), flush=True)


if __name__ == '__main__':
    main()
