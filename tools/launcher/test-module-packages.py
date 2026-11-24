#!/usr/bin/env python3
"""Disposable package fixtures: real ZIP manifests, corruption and empty NSIS setup.

No game is compiled or run. NSIS wraps tiny inert fixture files; the Windows
installer still needs its separate native first-launch acceptance check.
"""
import importlib.util
import json
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import uuid
import zipfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('module_packages', ROOT / 'tools/launcher/package-modules.py')
packages = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packages)


def pe(tail=b'', machine=0x8664):
    """A bare PE header (x86-64 PE32+ unless told otherwise) with inert bytes after it, so
    the packagers' architecture check reads a program file where a real one would be."""
    wide = machine == 0x8664
    data = bytearray(512)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 128)
    data[128:132] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 132, machine, 1, 0, 0, 0, 240 if wide else 224, 0x22)
    struct.pack_into('<H', data, 152, 0x20b if wide else 0x10b)
    if wide:
        struct.pack_into('<Q', data, 176, 0x140000000)
        struct.pack_into('<H', data, 222, 0x160)
    struct.pack_into('<H', data, 220, 2)
    return bytes(data) + tail


class Modules(unittest.TestCase):
    def test_builtin_archive_keeps_legacy_module_paths_and_notices(self):
        for platform in ('macos', 'windows'):
            with self.subTest(platform=platform):
                folder = self.base / platform
                folder.mkdir()
                app = folder / ('opencnc3d' if platform == 'macos' else 'opencnc3d-game.exe')
                app.write_bytes(b'MAPARCHIVE|builtin|3.8.10')
                brain = folder / ('RedAlert.dylib' if platform == 'macos' else 'RedAlert.dll')
                brain.write_bytes(b'inert brain')
                notice = b'OpenCNC3D map archive reader\nlibarchive and xz copyright fixture\n'
                (folder/'map-archive-LICENSE.txt').write_bytes(notice)
                destination = folder/'ra3d'
                def fake_baker(args, **kwargs):
                    if str(args[1]).endswith('ra3d/stage-runtime.py'):
                        destination.mkdir()
                        shutil.copy2(app, destination/app.name)
                        shutil.copy2(brain, destination/brain.name)
                        for name in ['archive.dll', 'libmaparchive.dylib', 'map-archive.json', 'map-archive-LICENSE.txt']:
                            (destination/name).write_bytes(b'unneeded paired dependency')
                with patch.object(packages.subprocess, 'run', side_effect=fake_baker):
                    packages.stage_ra(folder, destination, app, brain)
                for name in ['archive.dll', 'libmaparchive.dylib', 'map-archive.json', 'map-archive-LICENSE.txt']:
                    self.assertFalse((destination/name).exists())
                self.assertIn(notice, (destination/'NOTICE.md').read_bytes())
                files = packages.collect(destination, 'ra', platform, prefix='ra3d/')
                self.assertIn('ra3d/NOTICE.md', files)
                path = folder/'module.zip'
                packages.write_archive(files, path, 'ra', platform, '0.9.4')
                packages.verify_archive(path, 'ra', platform, '0.9.4')

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cnc3d-module-package-fixture-')
        self.base = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def fixture(self, name, data=b'fixture payload', mode=0o644):
        p = self.base / 'inputs' / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
        p.chmod(mode)
        return p

    def archive(self, module, platform, version='0.9.0'):
        names = (['ra3d/opencnc3d' if platform == 'macos' else 'ra3d/opencnc3d-game.exe',
                  'ra3d/ebld/BARL.ebld', 'ra3d/einf/E7.c3m', 'efx.pack', 'vfx3d/claims.txt']
                 if module == 'ra' else
                 ['opencnc3d' if platform == 'macos' else 'opencnc3d-game.exe',
                  'CONQUER.MIX', 'missions/SCG01EA.INI', 'SCG01EA.pack'])
        files = {n: self.fixture(n, pe(n.encode()) if platform == 'windows' and n.endswith('.exe')
                                 else n.encode(), 0o755 if 'opencnc3d' in n else 0o644) for n in names}
        out = self.base / f'OpenCNC3D-module-{module}-{platform}-v{version}.zip'
        return packages.write_archive(files, out, module, platform, version)

    def test_roundtrip_complete_deterministic_manifest_and_executable_mode(self):
        record = self.archive('ra', 'macos')
        path = self.base / record['asset']
        first = path.read_bytes()
        self.assertEqual(record, self.archive('ra', 'macos'))
        self.assertEqual(first, path.read_bytes())
        packages.verify_archive(path, 'ra', 'macos', '0.9.0')
        with zipfile.ZipFile(path) as z:
            self.assertFalse(any(n.endswith('/') for n in z.namelist()))
            self.assertEqual(z.getinfo('ra3d/opencnc3d').external_attr >> 16 & 0o777, 0o755)
            text = z.read('opencnc3d-module-ra.txt').decode('ascii')
            self.assertIn('platform macos\n', text)
            self.assertEqual(len(text.splitlines()) - 4, len(record['files']))
        # Identity is binding, even when all payload bytes are intact.
        with self.assertRaises(ValueError):
            packages.verify_archive(path, 'ra', 'windows', '0.9.0')
        # Extra bytes/files may not bypass the declared ownership inventory.
        with zipfile.ZipFile(path, 'a') as z:
            z.writestr('ra3d/missions/unlisted.INI', b'unlisted')
        with self.assertRaises(ValueError):
            packages.verify_archive(path, 'ra', 'macos', '0.9.0')

    def test_reject_player_state_media_symlinks_and_foreign_ownership(self):
        bad = ['../savegame.sav', '/etc/passwd', 'ra3d/../LICENSE', 'ra3d-fx.cfg',
               'ra3d/missions/user_maps/custom.INI', 'ra3d/movies/INTRO.VQA',
               'ra3d/data/SCORES.MIX', 'ra3d/audio/TRANSIT.MIX',
               'ra3d/settings/custom.txt', 'ra3d/unknown/asset.pack', 'ra3d/ant/ANT.c3m',
               'ra3d/Logs/game.txt', 'ra3d/missions/quicksave.INI',
               'SCG01EA.pack', 'TiberianDawn.dylib', 'opencnc3d-launcher']
        for rel in bad:
            with self.subTest(rel=rel):
                self.assertFalse(packages.allowed(rel, 'ra', 'macos'))
                with self.assertRaises(ValueError):
                    packages.write_archive({rel: self.fixture('good')}, self.base / 'bad.zip',
                                           'ra', 'macos', '0.9.0')
        target = self.fixture('real.c3m')
        link = target.with_name('linked.c3m')
        link.symlink_to(target)
        with self.assertRaises(ValueError):
            packages.write_archive({'ra3d/einf/E7.c3m': link}, self.base / 'link.zip',
                                   'ra', 'macos', '0.9.0')
        with self.assertRaises(ValueError):
            packages.collect(target.parent, 'td', 'macos')
        # A pair safe on a case-sensitive source disk must not collide on Windows.
        with self.assertRaises(ValueError):
            packages.write_archive({'ra3d/data/LOCAL.MIX': target, 'ra3d/data/local.mix': target},
                                   self.base / 'case.zip', 'ra', 'windows', '0.9.0')
        for rel in packages.RA_ROOT_PACKS:
            self.assertTrue(packages.allowed(rel, 'ra', 'macos'))
            self.assertEqual(packages.allowed(rel, 'td', 'macos'), rel == packages.TD_SHARED_WATER)
        self.assertFalse(packages.allowed('groundmask_RA_UNKNOWN.pack', 'ra', 'macos'))

    def test_ra_infantry_dedup_keeps_local_distinct_and_fallback_models(self):
        source = {
            rel: self.fixture(rel, data) for rel, data in (
                ('einf/E1.c3m', b'same'), ('ra3d/einf/E1.c3m', b'same'),
                ('einf/E2.c3m', b'shared'), ('ra3d/einf/E2.c3m', b'local'),
                ('einf/E3.c3m', b'fallback'), ('ra3d/3d/ant/ANT.c3m', b'ant'),
            )
        }
        files = dict(source)
        packages.remove_duplicate_ra_infantry(files)
        self.assertEqual(files, {rel: path for rel, path in source.items() if rel != 'einf/E1.c3m'})

    def test_catalog_requires_both_modules_platforms_and_exact_current_zip_hashes(self):
        records = [self.archive(m, p) for m in ('td', 'ra') for p in ('macos', 'windows')]
        catalog = self.base / 'OpenCNC3D-v0.9.0-modules.txt'
        packages.write_catalog(records, catalog, '0.9.0')
        self.assertEqual(len(catalog.read_text().splitlines()), 6)
        with self.assertRaises(ValueError):
            packages.write_catalog(records[:-1], catalog, '0.9.0')
        with self.assertRaises(ValueError):
            packages.write_catalog(records, catalog, '0.9.1')
        (self.base / records[0]['asset']).write_bytes(b'replaced archive')
        with self.assertRaises(ValueError):
            packages.write_catalog(records, catalog, '0.9.0')

    def test_expansion_catalogs_survive_staging_archive_and_native_picker(self):
        stage_spec = importlib.util.spec_from_file_location('ra_stage', ROOT / 'tools/ra3d/stage-runtime.py')
        stage = importlib.util.module_from_spec(stage_spec)
        stage_spec.loader.exec_module(stage)
        source_text = (ROOT / 'brain/vanilla/redalert/expand.cpp').read_text(encoding='cp437')
        source_text = source_text.split('const char* ExpandNames[] = {', 1)[1].split('NULL};', 1)[0]
        codes = re.findall(r'"(SC[GU][0-9]{2}EA)"', source_text)
        self.assertEqual(len(codes), 34)
        roster = [(code, 'counterstrike' if i < 16 else 'aftermath') for i, code in enumerate(codes)]
        roster += [(f'SCA{i:02}EA', 'ants') for i in range(1, 5)]
        missions = []
        for code, family in roster:
            ini = self.fixture(f'ra3d/missions/{code}.INI', ('original ' + code).encode())
            scene = self.fixture(f'ra3d/3d/{code}.pack', ('scene ' + code).encode())
            missions.append(dict(code=code, family=family, sha256=packages.digest(ini),
                                 scene_sha256=packages.digest(scene)))
        for name, ants in [('expansions.txt', False), ('ants.txt', True)]:
            self.fixture('ra3d/' + name, ''.join(f'{family} {code} Mission {code}\n'
                         for code, family in roster if (family == 'ants') == ants).encode())
        self.fixture('ra3d/expansions-manifest.json', json.dumps({'missions': missions}).encode())
        original = self.base / 'inputs/ra3d'
        staged = self.base / 'release-stage'
        staged.mkdir()
        stage.stage_catalogs(original, staged)
        for name in ['missions', '3d']:
            shutil.copytree(original / name, staged / name)
        for platform in ['macos', 'windows']:
            files = packages.collect(staged, 'ra', platform, prefix='ra3d/')
            packages.validate_ra_catalogs(files)
            archive = self.base / (platform + '.zip')
            packages.write_archive(files, archive, 'ra', platform, '0.9.1')
            unpacked = self.base / platform
            with zipfile.ZipFile(archive) as z:
                for name in ['expansions.txt', 'ants.txt', 'expansions-manifest.json']:
                    self.assertEqual(z.read('ra3d/' + name), (original / name).read_bytes())
                    self.assertFalse(packages.allowed('ra3d/' + name, 'td', platform))
                z.extractall(unpacked)
            if shutil.which('clang++'):
                cpp = self.base / 'picker.cpp'
                cpp.write_text('''#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cassert>
#include <unistd.h>
struct SpecOp {std::string scen,name,pack;int header=0,group=0;};
struct DO_Mission {const char *scen,*name;int nod,header;};
static std::vector<SpecOp> g_specops;static std::vector<DO_Mission> g_specopsRows;
#include "''' + str(ROOT / 'app/ra_expansions.h') + '''"
int main(int argc,char**argv){assert(argc==2&&!chdir(argv[1]));ra_expansions_scan();
int count=0;for(auto&r:g_specops)count+=!r.header;assert(count==34&&ra_ants_installed());
ra_ants_scan();assert(g_specopsRows.size()==4);puts("MISSION-CATALOG|missions=34|ants=4|PASS");}
''')
                exe = self.base / 'picker'
                subprocess.run(['clang++', '-std=c++14', str(cpp), '-o', str(exe)], check=True)
                native = subprocess.check_output([str(exe), str(unpacked)], text=True)
                self.assertIn('MISSION-CATALOG|missions=34|ants=4|PASS', native)
            for missing in ['ra3d/expansions.txt', 'ra3d/ants.txt', 'ra3d/3d/SCA01EA.pack']:
                with self.subTest(platform=platform, missing=missing), self.assertRaises(ValueError):
                    packages.validate_ra_catalogs({k: v for k, v in files.items() if k != missing})
            ini = files['ra3d/missions/SCA01EA.INI']
            saved = ini.read_bytes()
            ini.write_bytes(b'wrong mission')
            with self.assertRaises(ValueError):
                packages.validate_ra_catalogs(files)
            ini.write_bytes(saved)

    def test_whole_install_manifest_rejects_module_zip_instead_of_misclassifying_it(self):
        zips = []
        for platform in ('macos', 'windows'):
            path = self.base / f'OpenCNC3D-{platform}-v0.9.0.zip'
            with zipfile.ZipFile(path, 'w') as z:
                z.writestr(f'OpenCNC3D-{platform}-v0.9.0/opencnc3d-install.txt',
                           'version 0.9.0\ndata_id ' + 'a' * 64 + '\n')
            zips.append(path)
        output = self.base / 'main-manifest.txt'
        command = ['sh', str(ROOT / 'tools/launcher/make-manifest.sh'), 'v0.9.0', str(output)]
        subprocess.run(command + list(map(str, zips)), check=True, capture_output=True)
        before = output.read_bytes()
        module = self.archive('ra', 'macos')
        result = subprocess.run(command + list(map(str, zips)) + [str(self.base / module['asset'])],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('separately verified module catalog', result.stderr)
        self.assertEqual(before, output.read_bytes())

    def test_real_nsis_bootstrap_contains_art_but_no_game_or_network_wizard(self):
        if not shutil.which('makensis'):
            self.skipTest('NSIS compiler unavailable; native installer build still required')
        stage = self.base / 'windows-package'
        fixture_files = ['OpenCNC3D.exe', 'READ-ME-WINDOWS.txt', 'LICENSE',
                         'NOTICE.md', 'dosmenu.pack', 'enhanced-menu.pack', 'ecur/ecur.bin']
        for rel in fixture_files:
            p = stage / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes((pe if rel.endswith('.exe') else bytes)(b'/api/builds inert bootstrap fixture\n'))
        # A main full package can contain game files; Setup must not copy them.
        (stage / 'TiberianDawn.dll').write_bytes(b'do not embed game')
        private_stage = ROOT / 'build' / ('fixture-empty-setup-' + uuid.uuid4().hex)
        env = dict(os.environ, OPENCNC3D_SETUP_STAGE=str(private_stage),
                   OPENCNC3D_WIN_ZIP=str(self.base / 'setup-output'))
        try:
            result = subprocess.run(['sh', str(ROOT / 'tools/win/make-installer-win.sh'), str(stage)],
                                    env=env, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('SETUP|empty_launcher=1|automatic_download=0|files=8|', result.stdout)
            actual = {p.relative_to(private_stage / 'payload').as_posix()
                      for p in (private_stage / 'payload').rglob('*') if p.is_file()}
            self.assertEqual(actual, set(fixture_files) | {'opencnc3d.ico'})
            outputs = list((self.base / 'setup-output').glob('OpenCNC3DInstaller-windows-*.exe'))
            self.assertEqual(len(outputs), 1)
            self.assertGreater(outputs[0].stat().st_size, 0)
            script = (ROOT / 'tools/win/installer/opencnc3d.nsi').read_text()
            self.assertNotIn('--install-game', script)
            self.assertIn('opencnc3d-module-removal.txt', script)
            self.assertIn('!ifdef MODULE\n  Page custom', script)
            self.assertNotIn('RMDir /r', script)
        finally:
            shutil.rmtree(private_stage, ignore_errors=True)

    def test_complete_nsis_variants_embed_verified_game_and_media_choices(self):
        if not shutil.which('makensis'):
            self.skipTest('NSIS compiler unavailable')
        stage = self.base / 'complete-fixture'
        for rel in ('OpenCNC3D.exe', 'READ-ME-WINDOWS.txt', 'LICENSE',
                    'NOTICE.md', 'dosmenu.pack', 'enhanced-menu.pack', 'ecur/ecur.bin'):
            p = stage / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes((pe if rel.endswith('.exe') else bytes)(b'/api/builds inert fixture\n'))
        version = (ROOT / 'VERSION').read_text().strip()
        env = dict(os.environ, OPENCNC3D_WIN_ZIP=str(self.base / 'complete-output'))
        # The project's art bakers use Pillow. The Mac has its maintained art
        # interpreter even when a newer shell Python has no art dependencies.
        if Path('/usr/local/bin/python3.12').is_file():
            env['OPENCNC3D_ART_PYTHON'] = '/usr/local/bin/python3.12'
        for module, prefix in (('td', 'OpenCNC3DComplete'), ('ra', 'RA3DComplete')):
            record = self.archive(module, 'windows', version)
            private_stage = ROOT / 'build' / ('fixture-complete-setup-' + uuid.uuid4().hex)
            env['OPENCNC3D_SETUP_STAGE'] = str(private_stage)
            try:
                result = subprocess.run(['sh', str(ROOT / 'tools/win/make-installer-win.sh'),
                                         str(stage), module, str(self.base / record['asset'])],
                                        env=env, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(f'SETUP|module={module}|embedded_verified=1|media_checkboxes=2|files=9|', result.stdout)
                self.assertIn('--media-game ${MEDIA_GAME} --media-add-only',
                              (ROOT / 'tools/win/installer/opencnc3d.nsi').read_text())
                self.assertIn('--media-game ${MEDIA_GAME} --media-add-only',
                              (ROOT / 'tools/win/installer/media-page.nsh').read_text())
                output = self.base / 'complete-output' / f'{prefix}-windows-v{version}.exe'
                self.assertGreater(output.stat().st_size, 0)
                if module == 'ra':
                    art = json.loads((private_stage / 'ra-wizard.json').read_text())
                    self.assertEqual(art['dimensions'], [164, 314])
                    self.assertEqual(len(art['sources']), 2)
            finally:
                shutil.rmtree(private_stage, ignore_errors=True)

    def test_windows_modules_carry_no_sdl2(self):
        # SDL is linked into every Windows program. A module that carried SDL2.dll would
        # write it over the one a 32-bit launcher from v0.9.0 to v0.9.2 still loads.
        for rel, module in (('SDL2.dll', 'td'), ('ra3d/SDL2.dll', 'ra')):
            with self.subTest(rel=rel):
                self.assertFalse(packages.allowed(rel, module, 'windows'))
                with self.assertRaises(ValueError):
                    packages.write_archive({rel: self.fixture('sdl', pe())}, self.base / 'sdl.zip',
                                           module, 'windows', '0.9.0')
        self.assertTrue(packages.allowed('libSDL2-2.0.0.dylib', 'td', 'macos'))
        stage = self.base / 'stale-stage'
        (stage / 'ra3d').mkdir(parents=True)
        (stage / 'ra3d' / 'Sdl2.DLL').write_bytes(pe())
        (stage / 'opencnc3d-game.exe').write_bytes(pe())
        self.assertEqual(packages.sdl_dlls(stage), ['ra3d/Sdl2.DLL'])
        (stage / 'ra3d' / 'Sdl2.DLL').unlink()
        self.assertEqual(packages.sdl_dlls(stage), [])

    def test_setup_refuses_sdl2_and_programs_that_are_not_64_bit(self):
        if not shutil.which('makensis'):
            self.skipTest('NSIS compiler unavailable')
        def stage(name, launcher, extra=()):
            folder = self.base / name
            for rel in ('OpenCNC3D.exe', 'READ-ME-WINDOWS.txt', 'LICENSE', 'NOTICE.md',
                        'dosmenu.pack', 'enhanced-menu.pack', 'ecur/ecur.bin') + tuple(extra):
                p = folder / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(launcher if rel == 'OpenCNC3D.exe' else pe() if rel.endswith('.dll')
                              else b'inert fixture\n')
            return folder
        good = pe(b'/api/builds inert fixture\n')
        cases = [('sdl2', [str(stage('with-sdl2', good, ['ra3d/SDL2.dll']))], 'carries SDL2.dll'),
                 ('32-bit launcher', [str(stage('with-i386', pe(b'/api/builds\n', machine=0x14c)))],
                  'OpenCNC3D.exe is i386 (32-bit)')]
        version = (ROOT / 'VERSION').read_text().strip()
        record = packages.write_archive(
            {'opencnc3d-game.exe': self.fixture('i386-game.exe', pe(b'game', machine=0x14c)),
             'CONQUER.MIX': self.fixture('CONQUER.MIX')},
            self.base / f'OpenCNC3D-module-td-windows-v{version}.zip', 'td', 'windows', version)
        cases.append(('32-bit game in the module', [str(stage('complete', good)), 'td',
                      str(self.base / record['asset'])], 'opencnc3d-game.exe is i386 (32-bit)'))
        for label, args, said in cases:
            with self.subTest(case=label):
                output = self.base / ('refused-' + uuid.uuid4().hex)
                private_stage = ROOT / 'build' / ('fixture-refused-setup-' + uuid.uuid4().hex)
                env = dict(os.environ, OPENCNC3D_SETUP_STAGE=str(private_stage),
                           OPENCNC3D_WIN_ZIP=str(output))
                try:
                    result = subprocess.run(['sh', str(ROOT / 'tools/win/make-installer-win.sh')] + args,
                                            env=env, text=True, capture_output=True)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn(said, result.stdout + result.stderr)
                    self.assertFalse(list(output.glob('*.exe')), 'a refused Setup left an installer')
                finally:
                    shutil.rmtree(private_stage, ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
