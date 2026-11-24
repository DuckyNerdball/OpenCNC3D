#!/usr/bin/env python3
"""Native refusal checks against an existing exact reference proof; hidden only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--exe', type=Path, required=True)
    p.add_argument('--proof', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    runtime, exe, out = a.runtime.resolve(), a.exe.resolve(), a.out.resolve()
    references = [v for v in json.loads(a.proof.read_text()) if v['mode'] == 'original']
    assert {v['game'] for v in references} == {'td', 'ra'}
    assert not out.exists()
    out.mkdir(parents=True)
    results = []
    controls = {
        'actor-source': 'OPENCNC3D_ACTOR_DEPTH_SOURCE_OK',
        'actor-allocation': 'OPENCNC3D_ACTOR_DEPTH_ALLOC_OK',
        'actor-budget': 'OPENCNC3D_ACTOR_DEPTH_BUDGET_OK',
        'range-allocation': 'OPENCNC3D_MESH_PREPARED_ALLOC_OK',
        'range-budget': 'OPENCNC3D_MESH_PREPARED_BUDGET_FULL',
    }
    for reference in references:
        original = reference['command']
        engine = Path(original[original.index('--dylib') + 1])
        pack = runtime / original[original.index('--pack') + 1]
        gfx = Path(original[original.index('--gfx') + 1])
        assert sha(engine) == reference['engine_sha256']
        assert sha(pack) == reference['pack_sha256']
        assert sha(gfx) == reference['gfx_sha256']
        script = Path(original[original.index('--script') + 1])
        assert sha(script) == reference['script_sha256']
        old = script.parent
        for label, control in controls.items():
            folder = out / reference['game'] / label
            folder.mkdir(parents=True)
            new_script = folder / 'script.txt'
            new_script.write_text(script.read_text().replace(str(old), str(folder)))
            command = [str(exe)] + [v.replace(str(old), str(folder)) for v in original[1:]]
            command[command.index('--script') + 1] = str(new_script)
            env = {**os.environ, 'OPENCNC3D_BACKGROUND': '1',
                   'OPENCNC3D_ACTOR_DEPTH_CULL': '1', control: '0'}
            for setting in ['MESH_RIGID_ATTRIBUTES', 'SHORE_FILM_CACHE', 'WATER_ARRAY_CACHE', 'SHORE_FRAME_CACHE']:
                env['OPENCNC3D_' + setting] = '1' if os.name == 'nt' else '0'
            with (folder / 'run.log').open('w') as log:
                run = subprocess.run(command, cwd=runtime, env=env, stdout=log,
                                     stderr=subprocess.STDOUT, timeout=600)
            text = (folder / 'run.log').read_text(errors='replace')
            assert run.returncode == 0 and re.search(r'SCRIPT\|end .*0 failures', text), text[-1500:]
            sun = re.findall(r'^ACTORDEPTHCULL\|mode=\d+\|offers=(\d+)\|rejected=(\d+)\|verified=(\d+)\|bad=(\d+)\|refused=(\d+)', text, re.M)
            uv = re.findall(r'^ACTORUVCULL\|offers=(\d+)\|rejected=(\d+)\|verified=(\d+)\|bad=(\d+)', text, re.M)
            early = re.findall(r'^MESHEARLYCULL\|tests=(\d+)\|rejected=(\d+)\|verified=(\d+)\|bad=(\d+)', text, re.M)
            assert sun and uv and early
            assert all(int(v[3]) == 0 for v in sun + uv + early)
            if label.startswith('actor-'):
                assert int(sun[-1][0]) > 100 and int(sun[-1][1]) == 0 and int(sun[-1][4]) > 0, sun[-1]
                assert int(uv[-1][1]) > 20, uv[-1]
            else:
                assert int(sun[-1][1]) > 20, sun[-1]
                assert int(uv[-1][0]) > 100 and int(uv[-1][1]) == 0, uv[-1]
            pictures = [sha(folder / 'proof.png')]
            if len(reference['pictures']) > 1:
                names = ['pan', 'return', 'near', 'wide', 'classic', 'n64', 'enhanced', 'fog', 'reveal', 'restart']
                pictures.extend(sha(folder / (n + '.png')) for n in names)
            assert pictures == reference['pictures'], (reference['game'], label, 'Pictures differ')
            row = dict(game=reference['game'], refusal=label, passed=True, pictures=pictures,
                       sun=sun[-1], uv=uv[-1], early=early[-1], command=command,
                       exe_sha256=sha(exe), engine_sha256=sha(engine), pack_sha256=sha(pack),
                       gfx_sha256=sha(gfx), script_sha256=sha(new_script), run_sha256=sha(folder / 'run.log'))
            results.append(row)
            (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            print(json.dumps({k: row[k] for k in ['game', 'refusal', 'passed', 'sun', 'uv']}), flush=True)


if __name__ == '__main__':
    main()
