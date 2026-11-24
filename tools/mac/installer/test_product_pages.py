#!/usr/bin/env python3
"""The Mac installer's pages show their emblem and say where the game is installed.

Builds tiny inert products with package.py's own welcome(), distribution() and
make_component(), runs the real productbuild over them, expands each product with
pkgutil and reads the pages Installer would show. Nothing is installed and no window
opens.

productbuild copies into a product only the Resources files its Distribution names
(welcome.html and conclusion.html here). A page that drew its emblem from a separate
module.png lost the picture in every released product: Installer drew an empty box
with the alt text where the emblem belongs. So:

  (1) the old arrangement, a page linking to module.png beside it, is built the same
      way and must come out of productbuild WITHOUT module.png, and verify_pages() must
      refuse it (the check can see the fault it exists for);
  (2) each of the three products (empty, Dawn3D Complete, RA3D Complete) carries its
      emblem inside both pages, byte for byte the approved 160x160 art of its game;
  (3) both pages name ~/Applications/OpenCNC3D and say it is not the Mac's main
      Applications folder, and the last page says how to find the app in the Finder;
  (4) the Installer window title is the product's spoken name, not the asset name.

With --product A.pkg [B.pkg ...] it asks built products instead, a released one
included, through the same verify_pages() that package.py runs on every product.
"""
import base64
import importlib.util
from pathlib import Path
import plistlib
import re
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('mac_packages', ROOT / 'tools/mac/installer/package.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)

FAILS = []


def check(cond, what):
    print(('ok   ' if cond else 'FAIL ') + what)
    if not cond:
        FAILS.append(what)


def png_size(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
        return None
    return struct.unpack('>II', data[16:24])


def build(work, name, resources, title, game, complete):
    d = work / name
    payload = d / 'payload'
    payload.mkdir(parents=True)
    (payload / 'INERT.txt').write_text('inert installer-pages fixture\n')
    components = d / 'components.plist'
    components.write_bytes(plistlib.dumps([]))
    core = d / 'core.pkg'
    ident = 'com.slipgate.fixture.pages.' + name
    p.make_component(payload, None, core, ident, '0.9.0', components)
    if resources is None:
        resources = d / 'resources'
        p.welcome(resources, game, title, '0.9.0', complete)
    dist = d / 'Distribution'
    p.distribution(dist, title, '0.9.0', core, {}, {'core': ident})
    out = d / (name + '.pkg')
    subprocess.run(['productbuild', '--distribution', dist, '--resources', resources,
                    '--package-path', d, out], check=True, capture_output=True)
    expanded = d / 'expanded'
    subprocess.run(['pkgutil', '--expand', out, expanded], check=True)
    return expanded


def old_resources(work):
    """The pages as every product up to this check carried them: emblem in a file beside."""
    r = work / 'old-resources'
    r.mkdir()
    (r / 'module.png').write_bytes(p.ART['td'].read_bytes())
    page = ('<!doctype html><html><body><img src="module.png" width="160" height="160" '
            'alt="OpenCNC3D"><p>Installs inside your home folder:<br>'
            '<b>~/Applications/OpenCNC3D</b></p></body></html>')
    (r / 'welcome.html').write_text(page)
    (r / 'conclusion.html').write_text(page)
    return r


def main():
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mac-pages-fixture-', dir=ROOT / 'build') as name:
        work = Path(name)

        # (1) the old arrangement loses its picture, and the check says so
        old = build(work, 'old', old_resources(work), 'OpenCNC3DInstaller', 'td', False)
        held = sorted(x.name for x in (old / 'Resources').iterdir())
        check('module.png' not in held, f'productbuild drops a picture the Distribution does not name (the old product holds {held})')
        try:
            p.verify_pages(old / 'Resources', 'td')
            check(False, 'verify_pages refuses the old pages')
        except ValueError as e:
            check('module.png' in str(e), 'verify_pages refuses the old pages: ' + str(e))

        # (2)-(4) the three products as package.py now builds them
        for title, game, complete in [('OpenCNC3DInstaller', 'td', False),
                                      ('OpenCNC3DComplete', 'td', True),
                                      ('RA3DComplete', 'ra', True)]:
            ex = build(work, title, None, title, game, complete)
            res = ex / 'Resources'
            try:
                p.verify_pages(res, game)
                check(True, f'{title}: verify_pages passes on the built product')
            except ValueError as e:
                check(False, f'{title}: verify_pages: {e}')
            art = p.ART[game].read_bytes()
            for kind in ['welcome', 'conclusion']:
                text = (res / (kind + '.html')).read_text(encoding='utf-8')
                srcs = re.findall(r'<img src="([^"]*)"', text)
                data = b''
                if len(srcs) == 1 and srcs[0].startswith('data:image/png;base64,'):
                    data = base64.b64decode(srcs[0].split(',', 1)[1])
                check(data == art and png_size(data) == (160, 160),
                      f'{title} {kind}: the page itself holds the {game} emblem, {png_size(data)} px, '
                      f'{len(data)} bytes, same as {p.ART[game].name}')
                check('~/Applications/OpenCNC3D' in text and 'Mac’s main Applications folder' in text,
                      f'{title} {kind}: names ~/Applications/OpenCNC3D and says it is not the main Applications folder')
            last = (res / 'conclusion.html').read_text(encoding='utf-8')
            check('Go &gt; Home' in last and 'Spotlight' in last,
                  f'{title}: the last page says how to find the app (Finder Go > Home, Spotlight)')
            check(('Add Game' in last) != complete,
                  f'{title}: the last page says what to do next for this product')
            want = p.DISPLAY[title]
            dist = ET.parse(ex / 'Distribution').getroot()
            check(dist.findtext('title') == want and f'<h1>{want}</h1>' in last,
                  f'{title}: Installer title and page heading read "{want}"')

    if FAILS:
        print(f'MAC-INSTALLER-PAGES|FAIL|{len(FAILS)} checks failed')
        return 1
    print('MAC-INSTALLER-PAGES|PASS|productbuild drops a separate emblem file and the check '
          'refuses it; all three products carry their emblem inside both pages, name '
          '~/Applications/OpenCNC3D against the main Applications folder and say how to find the app')
    return 0


def check_products(paths):
    """Ask built products, a released one included, the same question: --product X.pkg."""
    with tempfile.TemporaryDirectory(prefix='mac-pages-product-') as name:
        for i, pkg in enumerate(paths):
            ex = Path(name) / str(i)
            subprocess.run(['pkgutil', '--expand', pkg, ex], check=True)
            game = 'ra' if Path(pkg).name.startswith('RA3D') else 'td'
            try:
                p.verify_pages(ex / 'Resources', game)
                check(True, f'{Path(pkg).name}: pages carry their emblem and say where the game goes')
            except ValueError as e:
                check(False, f'{Path(pkg).name}: {e}')
    print(('MAC-INSTALLER-PAGES|FAIL|' if FAILS else 'MAC-INSTALLER-PAGES|PASS|') + f'{len(paths)} built products asked')
    return 1 if FAILS else 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--product']:
        sys.exit(check_products(sys.argv[2:]))
    sys.exit(main())
