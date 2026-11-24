#!/usr/bin/env python3
"""The oldest macOS the Mac release runs on, read from its one source.

tools/mac/deployment-target.sh sets OPENCNC3D_MACOS_MIN, which every compile reads as
its deployment target. The Mac installer packages, the release notes and the check of
the shipped binaries all read the same line through this module, so the floor a package
states, the floor it enforces and the floor the binaries carry cannot drift apart.

Command line:
    os_floor.py                  print the floor, e.g. 14.0
    os_floor.py --label          print e.g. "macOS 14 (Sonoma)"
    os_floor.py --requirement    print the one-line requirement players read
    os_floor.py --check PATH...  refuse any Mach-O under PATH whose floor is newer
"""
from __future__ import annotations
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'tools/mac/deployment-target.sh'

# Marketing names, so a refusal names the release a player recognises. A floor with no
# name here is refused by macos_label() rather than printed without one.
NAMES = {11: 'Big Sur', 12: 'Monterey', 13: 'Ventura', 14: 'Sonoma', 15: 'Sequoia', 26: 'Tahoe'}

LC_BUILD_VERSION = 0x32
LC_VERSION_MIN_MACOSX = 0x24
PLATFORM_MACOS = 1


def macos_min(source: Path = SOURCE) -> str:
    """The OPENCNC3D_MACOS_MIN= line of deployment-target.sh, as written there."""
    found = re.findall(r'^OPENCNC3D_MACOS_MIN=([0-9]+(?:\.[0-9]+){0,2})\s*$',
                       Path(source).read_text(encoding='utf-8'), re.M)
    if len(found) != 1:
        raise ValueError(f'{source} must set OPENCNC3D_MACOS_MIN exactly once, as a plain version')
    return found[0]


def version_tuple(text: str) -> tuple[int, int, int]:
    parts = [int(x) for x in text.split('.')]
    if not 1 <= len(parts) <= 3:
        raise ValueError('not a macOS version: ' + text)
    return tuple(parts + [0] * (3 - len(parts)))  # type: ignore[return-value]


def macos_label(floor: str | None = None, strict: bool | None = None) -> str:
    """'macOS 14 (Sonoma)' for 14.0, 'macOS 14.2 (Sonoma)' for 14.2.

    The project's own floor must have a name (strict); a floor passed in for a test may
    not, and is then printed by number alone."""
    if strict is None:
        strict = floor is None or floor == macos_min()
    floor = floor or macos_min()
    major, minor, patch = version_tuple(floor)
    shown = str(major) if (minor, patch) == (0, 0) else '.'.join(str(x) for x in (major, minor, patch)[:3 if patch else 2])
    name = NAMES.get(major)
    if name is None:
        if strict:
            raise ValueError(f'macOS {major} has no release name in tools/mac/os_floor.py NAMES')
        return 'macOS ' + shown
    return f'macOS {shown} ({name})'


def requirement(floor: str | None = None) -> str:
    return f'Requires {macos_label(floor)} or newer, on Apple Silicon or Intel Macs.'


def shown(v: tuple[int, int, int]) -> str:
    """(15, 0, 0) -> '15.0', (14, 2, 1) -> '14.2.1', as vtool prints them."""
    return '.'.join(str(x) for x in (v if v[2] else v[:2]))


def _decode(v: int) -> tuple[int, int, int]:
    return (v >> 16, (v >> 8) & 0xff, v & 0xff)


def _thin_floor(data: bytes, off: int, size: int) -> tuple[int, int, int] | None:
    magic = data[off:off + 4]
    if magic == b'\xcf\xfa\xed\xfe':
        header = 32
    elif magic == b'\xce\xfa\xed\xfe':
        header = 28
    else:
        raise ValueError('not a little-endian Mach-O slice')
    ncmds, sizeofcmds = struct.unpack_from('<II', data, off + 16)
    pos = off + header
    end = min(pos + sizeofcmds, off + size, len(data))
    found = None
    for _ in range(ncmds):
        if pos + 8 > end:
            raise ValueError('truncated load commands')
        cmd, cmdsize = struct.unpack_from('<II', data, pos)
        if cmdsize < 8:
            raise ValueError('malformed load command')
        if cmd == LC_BUILD_VERSION:
            platform, minos = struct.unpack_from('<II', data, pos + 8)
            if platform == PLATFORM_MACOS:
                found = _decode(minos)
        elif cmd == LC_VERSION_MIN_MACOSX:
            found = _decode(struct.unpack_from('<I', data, pos + 8)[0])
        pos += cmdsize
    return found


def is_macho(head: bytes) -> bool:
    return head[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf', b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe')


def macho_floors(data: bytes) -> list[tuple[int, int, int] | None]:
    """The macOS floor of every slice, None for a slice that declares none."""
    magic = data[:4]
    if magic in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
        wide = magic == b'\xca\xfe\xba\xbf'
        (count,) = struct.unpack_from('>I', data, 4)
        out = []
        for i in range(count):
            if wide:
                _, _, off, size, _, _ = struct.unpack_from('>IIQQII', data, 8 + 32 * i)
            else:
                _, _, off, size, _ = struct.unpack_from('>IIIII', data, 8 + 20 * i)
            out.append(_thin_floor(data, off, size))
        return out
    return [_thin_floor(data, 0, len(data))]


def check_bytes(name: str, data: bytes, floor: str | None = None) -> None:
    """Refuse a Mach-O any of whose slices declares no floor or a newer one."""
    limit = version_tuple(floor or macos_min())
    for slice_floor in macho_floors(data):
        if slice_floor is None:
            raise ValueError(f'{name} declares no macOS floor at all (no LC_BUILD_VERSION)')
        if slice_floor > limit:
            raise ValueError(f'{name} needs macOS {shown(slice_floor)}, newer than the {shown(limit)} '
                             'floor the package states; dyld would refuse it on the older Macs the package accepts')


def check_tree(root: Path, floor: str | None = None) -> int:
    """Check every Mach-O under root (or root itself). Returns how many were read."""
    root = Path(root)
    files = [root] if root.is_file() else sorted(p for p in root.rglob('*') if p.is_file() and not p.is_symlink())
    count = 0
    for p in files:
        with p.open('rb') as f:
            if not is_macho(f.read(4)):
                continue
        check_bytes(str(p), p.read_bytes(), floor)
        count += 1
    return count


def main(argv: list[str]) -> int:
    if argv[:1] == ['--label']:
        print(macos_label())
    elif argv[:1] == ['--requirement']:
        print(requirement())
    elif argv[:1] == ['--check'] and len(argv) > 1:
        try:
            n = sum(check_tree(Path(x)) for x in argv[1:])
        except ValueError as e:
            print('MACOS-FLOOR|FAIL|' + str(e), file=sys.stderr)
            return 1
        print(f'MACOS-FLOOR|PASS|{n} Mach-O files, none newer than macOS {macos_min()}')
    elif not argv:
        print(macos_min())
    else:
        print(__doc__, file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
