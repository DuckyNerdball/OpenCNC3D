#!/usr/bin/env python3
"""The Mac packages explain the macOS floor instead of refusing every location.

Builds tiny inert products with package.py's own welcome(), distribution() and
make_component(), and only QUERIES macOS Installer about them (-volinfo,
-showChoicesXML). Nothing is installed and no window opens.

On a macOS older than the floor, allowed-os-versions alone disables the startup disk
with an empty reason; the home folder is the only location these packages allow, so
Installer shows its generic "cannot install in this location" line for every choice.
An older macOS is stood in for by building the same product with the floor raised to
99.0, above any macOS that exists:

  (1) 99.0 without the installation check: Installer disables the disk with an empty
      reason (the refusal players saw);
  (2) 99.0 with it: Installer says which macOS this Mac runs and which one is needed;
  (3) the real floor: the disk is offered and the choices come back, no error;
  (4) the welcome page and the Distribution state the one floor read from
      tools/mac/deployment-target.sh;
  (5) a Mach-O asking for a newer macOS than the floor, in either slice, is refused,
      by the parser and by bootstrap(), and the parser agrees with vtool.

installer exits 0 even when it refuses, so every verdict is read from its text.
"""
import importlib.util
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('mac_packages', ROOT / 'tools/mac/installer/package.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
F = p.os_floor

FAILS = []


def check(cond, what):
    print(('ok   ' if cond else 'FAIL ') + what)
    if not cond:
        FAILS.append(what)


def query(*args):
    r = subprocess.run(['installer', *map(str, args)], capture_output=True)
    return r.returncode, r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')


def product(work, name, floor, with_check=True):
    d = work / name
    payload = d / 'payload'
    payload.mkdir(parents=True)
    (payload / 'INERT.txt').write_text('inert os-floor fixture\n')
    components = d / 'components.plist'
    components.write_bytes(plistlib.dumps([]))
    core = d / 'core.pkg'
    ident = 'com.slipgate.fixture.osfloor.' + name
    p.make_component(payload, None, core, ident, '0.9.0', components)
    resources = d / 'resources'
    p.welcome(resources, 'td', 'OpenCNC3DComplete', '0.9.0', True, floor)
    dist = d / 'Distribution'
    p.distribution(dist, 'OpenCNC3DComplete', '0.9.0', core, {}, {'core': ident}, floor)
    if not with_check:
        root = ET.parse(dist).getroot()
        for tag in ['installation-check', 'script']:
            for el in root.findall(tag):
                root.remove(el)
        ET.ElementTree(root).write(dist, encoding='utf-8', xml_declaration=True)
    out = d / (name + '.pkg')
    subprocess.run(['productbuild', '--distribution', dist, '--resources', resources,
                    '--package-path', d, out], check=True, capture_output=True)
    return out, resources


def compile_inert(work, name, floor, archs=('arm64', 'x86_64'), text=''):
    src = work / (name + '.c')
    src.write_text('#include <stdio.h>\nint main(int n,char **v){if(n==241)puts("%s");return 0;}\n' % text)
    out = work / name
    cmd = ['cc', '-mmacosx-version-min=' + floor, str(src), '-o', str(out)]
    for a in archs:
        cmd[1:1] = ['-arch', a]
    subprocess.run(cmd, check=True)
    return out


def vtool_floors(path):
    out = subprocess.run(['vtool', '-show-build', str(path)], capture_output=True, text=True).stdout
    return sorted(F.version_tuple(x) for x in re.findall(r'^\s*minos\s+([0-9.]+)', out, re.M))


def main():
    floor = F.macos_min()
    label = F.macos_label()
    running = subprocess.run(['sw_vers', '-productVersion'], capture_output=True, text=True).stdout.strip()
    print(f'floor {floor} ({label}) read from tools/mac/deployment-target.sh; this Mac runs macOS {running}')
    check(re.search(r'^OPENCNC3D_MACOS_MIN=' + re.escape(floor) + r'$',
                    (ROOT / 'tools/mac/deployment-target.sh').read_text(), re.M) is not None,
          'the floor is the OPENCNC3D_MACOS_MIN line of deployment-target.sh')
    check(F.version_tuple(running) >= F.version_tuple(floor), f'this Mac ({running}) is at or above the floor, so (3) means something')
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mac-os-floor-fixture-', dir=ROOT / 'build') as name:
        work = Path(name)

        # (1) the refusal as shipped up to now: allowed-os-versions alone
        old, _ = product(work, 'before', '99.0', with_check=False)
        _, vol = query('-volinfo', '-pkg', old)
        check(vol.strip() == '', 'before, at 99.0: -volinfo offers no disk (got %r)' % vol.strip())
        _, text = query('-showChoicesXML', '-pkg', old, '-target', 'CurrentUserHomeDirectory')
        print('     installer said: ' + ' | '.join(text.strip().splitlines()[:3]))
        check('because it is disabled' in text and 'OpenCNC3D runs on' not in text,
              'before, at 99.0: the home folder is refused with no reason given')

        # (2) the same product with the installation check
        new, _ = product(work, 'after', '99.0')
        _, text = query('-showChoicesXML', '-pkg', new, '-target', 'CurrentUserHomeDirectory')
        print('     installer said: ' + ' | '.join(text.strip().splitlines()[:3]))
        want = f'This Mac runs macOS {running}. OpenCNC3D runs on macOS 99 or newer, on Apple Silicon and Intel Macs.'
        check(want in text, 'after, at 99.0: Installer names this Mac\'s macOS and the one needed')
        check('<plist' not in text, 'after, at 99.0: no choices are offered')

        # (3) the real floor
        real, resources = product(work, 'real', None)
        _, vol = query('-volinfo', '-pkg', real)
        check('/' in vol.split(), 'real floor: -volinfo offers the startup disk (got %r)' % vol.strip())
        rc, text = query('-showChoicesXML', '-pkg', real, '-target', 'CurrentUserHomeDirectory')
        choices = []
        if '<plist' in text:
            choices = plistlib.loads(text[text.index('<?xml'):].encode())
        def idents(rows):
            for c in rows:
                yield c.get('choiceIdentifier')
                yield from idents(c.get('childItems', []))
        check(rc == 0 and 'Error -' not in text and 'core' in idents(choices),
              'real floor: the choices come back with core and no error')

        # (4) one floor, stated where the player reads it
        welcome = (resources / 'welcome.html').read_text()
        check(F.requirement() in welcome and f'Requires {label} or newer' in welcome,
              'the welcome page says: ' + F.requirement())
        check(F.requirement() not in (resources / 'conclusion.html').read_text(),
              'the conclusion page does not repeat it')
        exp = work / 'real-expanded'
        subprocess.run(['pkgutil', '--expand', real, exp], check=True)
        dist = ET.parse(exp / 'Distribution').getroot()
        try:
            p.verify_os_floor(dist)
            verified = True
        except ValueError as e:
            print('     ' + str(e))
            verified = False
        check(verified, 'the built Distribution carries the installation check and os-version min ' + floor)
        script = dist.findtext('script') or ''
        check(f"compareVersions(system.version.ProductVersion, '{floor}') == -1" in script and '<' not in script,
              'the check compares against ' + floor + ' and has no "<" for XML to escape')
        check(f"OpenCNC3D runs on {label} or newer, on Apple Silicon and Intel Macs." in script
              and f"my.result.title = 'OpenCNC3D needs {label} or newer';" in script,
              'on an older Mac the real product would say: OpenCNC3D runs on ' + label + ' or newer')
        check(f'Requires {label} or newer' in (ROOT / 'tools/launchers/READ-ME.txt').read_text(),
              'READ-ME.txt states the same floor')

        # (5) binaries newer than the floor are refused
        bins = work / 'bins'
        bins.mkdir()
        good = compile_inert(bins, 'good', floor)
        newer = compile_inert(bins, 'newer', '15.0' if F.version_tuple(floor) < (15, 0, 0) else '26.0')
        arm = compile_inert(bins, 'arm', floor, ('arm64',))
        x86 = compile_inert(bins, 'x86', '15.0' if F.version_tuple(floor) < (15, 0, 0) else '26.0', ('x86_64',))
        mixed = bins / 'mixed'
        subprocess.run(['lipo', '-create', arm, x86, '-output', mixed], check=True)
        for b in [good, newer, mixed]:
            check(sorted(F.macho_floors(b.read_bytes())) == vtool_floors(b), f'parser agrees with vtool on {b.name}')

        def refused(b):
            try:
                F.check_bytes(b.name, b.read_bytes())
                return False
            except ValueError:
                return True
        check(not refused(good), f'a binary at {floor} passes')
        check(refused(newer), 'a binary asking for a newer macOS is refused')
        check(refused(mixed), 'a universal binary whose second slice asks for a newer macOS is refused')

        # bootstrap(), the step every native product goes through, refuses one too
        version = '0.9.0'
        marks = '/api/builds --module-file --media-game --media-add-only ' + version
        launcher_ok = compile_inert(bins, 'launcher-ok', floor, text=marks)
        launcher_new = compile_inert(bins, 'launcher-new', '15.0' if F.version_tuple(floor) < (15, 0, 0) else '26.0', text=marks)

        def base_with(launcher, tag):
            base = work / ('base-' + tag)
            for rel in p.BOOTSTRAP:
                q = base / rel
                q.parent.mkdir(parents=True, exist_ok=True)
                if rel == 'opencnc3d-launcher' or rel.endswith('.dylib'):
                    shutil.copy2(launcher if rel == 'opencnc3d-launcher' else good, q)
                else:
                    q.write_bytes(b'INERT\n')
            app = base / 'OpenCNC3D.app/Contents/MacOS'
            app.mkdir(parents=True)
            shutil.copy2(good, app / 'opencnc3d-launch')
            return base

        def bootstrap_refuses(launcher, tag):
            dest = work / ('dest-' + tag)
            try:
                p.bootstrap(base_with(launcher, tag), dest, version, 'OpenCNC3DComplete')
                return None
            except ValueError as e:
                return str(e)
        check(bootstrap_refuses(launcher_ok, 'ok') is None, 'bootstrap() accepts a payload at the floor')
        why = bootstrap_refuses(launcher_new, 'new') or ''
        print('     bootstrap said: ' + why)
        check('opencnc3d-launcher needs macOS' in why, 'bootstrap() refuses a launcher asking for a newer macOS, by name')

    if FAILS:
        print(f'MAC-OS-FLOOR|FAIL|{len(FAILS)} checks failed: ' + '; '.join(FAILS))
        return 1
    print(f'MAC-OS-FLOOR|PASS|floor {floor}: an older macOS is told "{want}" instead of the generic location refusal; '
          'the real floor offers the disk and its choices; welcome page, Distribution and READ-ME agree; newer binaries refused')
    return 0


if __name__ == '__main__':
    sys.exit(main())
