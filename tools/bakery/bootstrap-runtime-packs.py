#!/usr/bin/env python3
"""Bake scenario-independent runtime packs from this checkout's tracked inputs.

Legacy bakers retain their formats and pixel algorithms. Resolve their old scratch
paths explicitly, direct derived manifests away from source, and seed the build's
own play folder. Never read another installation or the historical dist snapshot.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    a = p.parse_args()
    root = a.root.resolve()
    out = root / 'build/base-runtime-packs'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'chunks').mkdir(exist_ok=True)
    play = root / 'playable'
    assert not play.is_symlink(), 'the private build must own its staging folder'
    play.mkdir(exist_ok=True)
    rom = root / 'data/rom/cnc_eu.z64'
    dos = root / 'data/dosdata'
    assert rom.is_file() and (dos / 'CONQUER.MIX').is_file(), 'tracked original inputs are missing'
    sys.path.insert(0, str(root / 'menu/tools'))

    def module(rel):
        spec = importlib.util.spec_from_file_location('base_' + Path(rel).stem, root / rel)
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        return m

    def run(rel, *args):
        subprocess.run([sys.executable, str(root / rel), *map(str, args)], cwd=root, check=True)

    # Start with the engine's actual building list, then let game/build.sh merge
    # the original DOS shape members by the existing Classic MIX baker.
    run('game/make_opencnc3dmake.py', '--conquer', dos / 'CONQUER.MIX',
        '--bdata', root / 'brain/vanilla/tiberiandawn/bdata.cpp', '--out', out / 'CONQUER.MIX')
    run('game/bake_dosclassic_mix.py', out / 'CONQUER.MIX', dos / 'CONQUER.MIX',
        out / 'CONQUER.classic.MIX')
    shutil.copyfile(out / 'CONQUER.classic.MIX', play / 'CONQUER.MIX')

    for rel, name in [('game/bake_campaign.py', 'campaign.pack'),
                      ('game/bake_dosinfantry.py', 'dosinfantry.pack'),
                      ('game/bake_dossidebar.py', 'dossidebar.pack'),
                      ('game/bake_dosmake.py', 'dosmake.pack')]:
        m = module(rel)
        m.OUT = str(out / name)
        if name == 'dossidebar.pack':
            m.SCRATCH = str(root)
            m.MIX_PATHS = [str(Path('data/dosdata') / n) for n in
                           ('CONQUER.MIX', 'LOCAL.MIX', 'TEMPERAT.MIX', 'DESERT.MIX', 'GENERAL.MIX')]
        if name == 'dosmake.pack':
            m.CONQUER_CANDIDATES = [str(out / 'CONQUER.MIX')]
            m.LOCAL_CANDIDATES = [str(dos / 'LOCAL.MIX')]
        m.main()

    m = module('tools/bakery/sharecopy/eyes/bake_cameos.py')
    m.main(str(out / 'cameos.pack'))
    run('game/bake_efx.py', rom, out / 'efx.pack')
    run('game/bake_verdict.py', rom, out / 'verdict.pack')
    run('game/bake_dostiberium.py', '--rom', rom, '--out', out / 'dostib.pack')
    run('game/bake_smudges.py', out / 'smudge.pack')

    m = module('tools/sidebar_redesign/cameos95.py')
    m.OUT = str(out / 'cameos95.pack')
    m.DOSPACK = str(out / 'dossidebar.pack')
    m.HERE = str(out)  # its derived chunks report, not the authored layout
    m.build()
    m = module('tools/sidebar_redesign/cameos_tdr.py')
    m.OUT = str(out / 'cameos_tdr.pack')
    m.build()

    m = module('game/bake_doscrate.py')
    m.main(str(dos / 'CONQUER.MIX'), str(out / 'dossidebar.pack'), str(out / 'doscrate.pack'))

    records = []
    for pack in sorted(p for p in out.glob('*.pack') if p.name != 'mappreview.pack'):
        assert pack.stat().st_size > 0
        shutil.copyfile(pack, root / 'game' / pack.name)
        shutil.copyfile(pack, play / pack.name)
        records.append({'path': pack.name, 'bytes': pack.stat().st_size,
                        'sha256': hashlib.sha256(pack.read_bytes()).hexdigest()})
    run('tools/bakery/bootstrap-scenario-packs.py', '--root', root)
    # The scenario bake above resolves terrain only for the scenario packs it makes; a map
    # with a preview and no scenario pack of its own (Pitfall Quarter, SCM92EA) is resolved
    # by the preview baker itself. --check then holds the result to the committed manifest.
    run('tools/bake_map_previews.py', '--resolve-terrain', '--out', out / 'mappreview.pack')
    preview = out / 'mappreview.pack'
    run('tools/bake_map_previews.py', '--check', preview)
    for folder in [root / 'game', play]:
        shutil.copyfile(preview, folder / preview.name)
    records.append({'path': preview.name, 'bytes': preview.stat().st_size,
                    'sha256': hashlib.sha256(preview.read_bytes()).hexdigest()})
    (out / 'receipt.json').write_text(json.dumps({'packs': records,
        'source': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        'original_rom_sha256': hashlib.sha256(rom.read_bytes()).hexdigest(),
        'original_conquer_sha256': hashlib.sha256((dos / 'CONQUER.MIX').read_bytes()).hexdigest(),
        'historical_snapshot_used': False}, indent=2) + '\n')
    print('BASE-PACKS|%d current-source packs|PASS' % len(records), flush=True)


if __name__ == '__main__':
    main()
