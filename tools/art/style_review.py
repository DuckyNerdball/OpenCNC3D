"""Capture the fixed Mammoth/Yard style comparison in a staged OpenCNC3D game.

python3 tools/art/style_review.py --runtime playable \
  --out art/style/enhanced_v1_engine_comparison.png
The mission bytes are restored even if a capture fails.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def display_path(path):
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    output = args.out.resolve()
    # The headless --scen command reads directly from --dir, unlike the User Maps UI.
    mission_dir = runtime / 'missions'
    original_files = {extension: (mission_dir / f'USER92.{extension}') for extension in ('INI', 'BIN', 'HGT')}
    prior_bytes = {extension: (path.read_bytes() if path.exists() else None)
                   for extension, path in original_files.items()}
    original = (ROOT / 'game/examples/USER92.INI').read_bytes()
    old = b'GoodGuy,FACT,256,2147'
    assert original.count(old) == 1, 'Unexpected USER92 Yard owner or layout'
    program = runtime / 'cnc_eyes'
    library = 'TiberianDawn.dylib'
    if not program.is_file():
        program = runtime / 'cnc_eyes.exe'
        library = 'TiberianDawn.dll'
    assert program.is_file() and (runtime / library).is_file()
    shots = runtime / 'shots/style'
    shots.mkdir(parents=True, exist_ok=True)
    results = {}
    try:
        for extension, path in original_files.items():
            path.write_bytes((ROOT / f'game/examples/USER92.{extension}').read_bytes())
        for name, data in [('gdi', original), ('nod', original.replace(old, b'BadGuy,FACT,256,2147'))]:
            original_files['INI'].write_bytes(data)
            image_path = shots / f'{name}.png'
            script = shots / f'{name}.txt'
            script.write_text(f'gfx experimental_textures 1\ncam 34 33\nzoom max\ntick 6\nshot shots/style/{name}.png\nm3dump\nquit\n')
            if image_path.exists():
                image_path.unlink()
            cmd = [str(program), '--scen', 'USER92', '--pack', 'SCG01EA.pack',
                   '--dylib', library, '--dir', 'missions/', '--content', 'content/',
                   '--cameos', 'cameos.pack', '--dospack', 'dossidebar.pack',
                   '--dosinf', 'dosinfantry.pack', '--noshroud', '--w', '1600',
                   '--h', '1000', '--script', str(script), '--enhanced']
            log = shots / f'{name}.log'
            with log.open('w') as handle:
                run = subprocess.run(cmd, cwd=runtime, stdout=handle,
                                     stderr=subprocess.STDOUT, timeout=180)
            text = log.read_text(errors='replace')
            assert run.returncode == 0, text[-3000:]
            assert re.search(r'SCRIPT.*0 failures', text), text[-3000:]
            summary = re.search(r'M3DUMP\|[^\n]*\|drawn=(\d+)\|tris=(\d+)\|', text)
            assert summary and int(summary.group(1)) >= 2 and int(summary.group(2)) >= 19810, text[-3000:]
            assert '|cell=35,33|model=1|visible=1|construct=0|damaged=0|' in text, text[-3000:]
            assert image_path.stat().st_size > 10000
            results[name] = {'screenshot': str(image_path), 'sha256': sha(image_path),
                             'engine_triangles_total': int(summary.group(2))}
    finally:
        for extension, path in original_files.items():
            if prior_bytes[extension] is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(prior_bytes[extension])

    left = Image.open(results['gdi']['screenshot']).convert('RGB')
    right = Image.open(results['nod']['screenshot']).convert('RGB')
    assert left.size == right.size == (1600, 1000)
    crop = (480, 300, 1380, 750)
    sheet = Image.new('RGB', (1800, 500), '#151515')
    sheet.paste(left.crop(crop), (0, 50))
    sheet.paste(right.crop(crop), (900, 50))
    draw = ImageDraw.Draw(sheet)
    draw.text((20, 18), 'GDI Yard + Mammoth | current build | USER92', fill='white')
    draw.text((920, 18), 'Nod Yard + Mammoth | current build | USER92', fill='white')
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    report_captures = {name: {**capture, 'screenshot': display_path(Path(capture['screenshot']))}
                       for name, capture in results.items()}
    report = {'style_version': 'enhanced-v1', 'mission': 'USER92',
              'camera_cell': [34, 33], 'zoom': 'max', 'resolution': [1600, 1000],
              'contact_sheet_crop': list(crop),
              'enhanced': True, 'experimental_art': True,
              'models': {'HTNK.c3m': sha(runtime / 'models3d/HTNK.c3m'),
                         'FACT.c3m': sha(runtime / 'models3d/FACT.c3m')},
              'captures': report_captures, 'contact_sheet': display_path(output)}
    output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
