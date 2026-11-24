#!/usr/bin/env python3
"""Stage current Mac app aliases and report the installed binary's build identity."""
import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import tempfile

runtime, binary = (Path(p).resolve() for p in sys.argv[1:3])
# Refuse a stale calibration-less pack before replacing any executable aliases.
bib = Path(__file__).resolve().parents[2] / 'game/bibenh.pack'
if not bib.is_file() or bib.read_bytes()[:12] != b'BIBENH4\0\x04\0\0\0':
    raise SystemExit('Rebuild current calibrated bibenh.pack before staging the app')
marks = subprocess.check_output(['strings', str(binary)], text=True)
versions = set(re.findall(r'(?:OPENCNC3D|CNC3D)\|build\|v([^|\s]+)', marks))
if len(versions) != 1:
    raise SystemExit('Cannot identify one build version in the app binary')
version = versions.pop()
for name in ['opencnc3d-update.journal', 'cnc3d-update.journal']:
    if (runtime / name).exists():
        raise SystemExit('Finish the existing update before staging a local build')
targets = ['opencnc3d']
targets += [p for p in ['cnc3d'] if (runtime / p).exists()]
if os.environ.get('OPENCNC3D_NO_RA') != '1':
    targets += [p for p in ['ra3d/opencnc3d', 'ra3d/cnc3d'] if (runtime / p).exists()]
for name in targets:
    target = runtime / name
    if target.is_symlink():
        raise SystemExit('Runtime executable must be a regular file: ' + str(target))
    shutil.copy2(binary, target)
# A map importer is incomplete without its paired archive reader. Stage only
# these verified dependencies here; no broad terrain/art rebake is involved.
if b'MAPIMPORT|item|' in binary.read_bytes():
    import hashlib
    import json
    dependency = binary.parent / 'libmaparchive.dylib'
    manifest = binary.parent / 'map-archive.json'
    notices = binary.parent / 'map-archive-LICENSE.txt'
    assert dependency.is_file() and manifest.is_file() and notices.is_file(), 'Missing paired map archive reader/notices'
    record = json.loads(manifest.read_text())
    assert hashlib.sha256(dependency.read_bytes()).hexdigest() == record['sha256'], 'Map archive dependency hash mismatch'
    for folder in [runtime] + ([runtime / 'ra3d'] if (runtime / 'ra3d').is_dir() else []):
        for source in [dependency, manifest, notices]:
            target = folder / source.name
            assert not target.is_symlink(), target
            shutil.copy2(source, target)
# Code-only integration preserves the installation's independently authored artwork.
if os.environ.get('OPENCNC3D_CODE_ONLY') != '1':
    # A normal launch must find the ground used by preview and verification runs.
    for asset in ['bibenh.pack','clearblend_TEMPERAT.pack','clearblend_DESERT.pack','groundmask_DESERT.pack','clearblend_RA_SNOW.pack','groundmask_RA_SNOW.pack','grassmask_TEMPERAT.pack','waterbase_RA_TEMPERAT.pack','coastmask_TEMPERAT.pack','coastmask_DESERT.pack','coastmask_WINTER.pack','coastmask_SNOW.pack','waterbase_WINTER.pack','waterbase_SNOW.pack']:
        ground = Path(__file__).resolve().parents[2] / 'game' / asset
        if ground.is_file():
            with tempfile.NamedTemporaryFile(dir=runtime, prefix='.clear-ground-', delete=False) as f:
                temporary = Path(f.name)
            shutil.copy2(ground, temporary)
            os.replace(temporary, runtime / ground.name)
    # Keep source-placed scenery and its removal mattes inseparable on a code-only update.
    mineral_installed = (runtime / 'stone-materials/v01/MANIFEST.json').is_file() or (runtime / 'stonematerials.pack').is_file()
    subprocess.run([sys.executable,str(Path(__file__).resolve().parents[2]/'tools/art/terrain/stage_desert_details.py'),str(runtime)],check=True)
    # Scenery staging restores raw rock albedo packs. Existing mapped installations
    # must receive their paired mineral normals/specular data after that final copy.
    if mineral_installed:
        subprocess.run([sys.executable,str(Path(__file__).resolve().parents[2]/'tools/art/rocks/stage_stone_materials.py'),str(runtime)],check=True)
    # Experimental Minigunner pixels are a runtime asset, not a browser dependency.
    sprites = Path(__file__).resolve().parents[2] / 'game/sprites'
    if sprites.is_dir():
        (runtime / 'sprites').mkdir(exist_ok=True)
        for name in ('e1-v15.pack', 'e1-v15-player-mask.png', 'manifest.json'):
            shutil.copy2(sprites / name, runtime / 'sprites' / name)
# Approved native skinned Ant and its separate mask/provenance package.
ant = Path(__file__).resolve().parents[2] / 'game/ant_ra'
if ant.is_dir() and (runtime / 'ra3d').is_dir():
    destination = runtime / 'ra3d/3d/ant'
    destination.mkdir(parents=True, exist_ok=True)
    for source in ant.iterdir():
        if source.is_file():
            target = destination / source.name
            if target.is_symlink():
                raise SystemExit('Refusing Ant asset symlink: ' + str(target))
            shutil.copy2(source, target)
record = '# Local source build; no verified release asset data ID.\nversion ' + version + '\ndata_id unknown\n'
if not os.environ.get('OPENCNC3D_COOKED'):
    record += 'private_developer 1\n'
for name, text in [('cnc3d-install.txt', record), ('opencnc3d-install.txt', record),
                   ('BUILD-ID', 'v' + version + ' macOS\n'), ('BUILD-ID.txt', 'v' + version + ' macOS\n')]:
    target = runtime / name
    if target.is_symlink() or (target.exists() and not target.is_file()):
        raise SystemExit('Runtime metadata must be a regular file: ' + str(target))
    with tempfile.NamedTemporaryFile(mode='w', dir=runtime, prefix='.build-version-', delete=False) as f:
        f.write(text)
        temporary = f.name
    os.replace(temporary, target)
print('staged', len(targets), 'app aliases; installed version v' + version)
# The Dock check needs the launcher and a Dock bundle beside the game. A play folder being
# built for the first time has neither yet: game/make-build.sh copies them in after this step
# and runs the same check last, so here it runs only for a folder that already carries them.
if (runtime / 'opencnc3d-launcher').exists() and any((runtime / b).exists() for b in ('C&C3D.app', 'OpenCNC3D.app', 'Editor.app')):
    runpy.run_path(str(Path(__file__).with_name('gate-dock-launchers.py')))['check'](runtime,repair=True)
