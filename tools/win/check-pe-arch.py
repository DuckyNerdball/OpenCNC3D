#!/usr/bin/env python3
"""Refuse any Windows program that is not 64-bit x86 (PE32+, AMD64), or that is laid out
so that a pointer cut to 32 bits could keep working.

Windows builds are 64-bit only. One 32-bit file in a 64-bit package is enough to stop
the program that loads it: a 32-bit SDL2.dll beside a 64-bit game, or a 32-bit brain
DLL that LoadLibrary refuses with error 193. Every .exe and .dll is read by content,
in a folder, inside a zip, or named on its own (an installer's payload is checked as
the folder it was made from), and every wrong one is named.

Each one must also let Windows load it anywhere in the 64-bit address space
(DYNAMIC_BASE and HIGH_ENTROPY_VA in DllCharacteristics) and prefer an image base above
4 GB. With both, a pointer truncated to 32 bits points at nothing the first time the
program uses an address inside one of its own images, instead of working until a
process happens to grow past 4 GB. A link flag that took either away would otherwise
go unnoticed, because nothing fails at once.

And none may import SDL2.dll. SDL is linked into every program and no build or package
carries the DLL, so a program whose link fell back to the import library would start
here and stop on every machine without an SDL2.dll beside it. The import table is read
by content as well, so the check needs no binutils on the machine that runs it.

And none may import, by name, a function or a DLL that Windows 7 does not have. Windows 7
and 8.1 are not supported, but the Windows loader refuses a whole program for one import
it cannot resolve, before any code of ours runs, so a single call that is newer than
Windows 7 turns "untested there" into "cannot start there". That happened once: the map
archive reader was built for Windows 10 by default and imported KERNEL32!CreateFile2
(Windows 8), and the game and the diagnostics program stopped loading on Windows 7 while
the launcher still started. WIN8_FUNCTIONS and WIN8_DLLS list what is refused. A newer
call that a program really needs is looked up at run time with GetProcAddress, with a
fallback when it is missing (SDL and winpthreads do that for SetThreadDescription,
GetDpiForWindow and GetSystemTimePreciseAsFileTime, which is why those appear in their
strings but not in any import table).

    check-pe-arch.py [--installed] <folder-or-zip-or-file>...

--installed is for a folder Setup installed into, and only for that. Setup writes its own
uninstall.exe there, and a Setup program may be kept beside it; both are 32-bit NSIS
programs on purpose, so that on 32-bit Windows Setup can start and refuse with a
sentence. With --installed those two names are listed as skipped instead of checked.
Build folders and packages are never asked with it: Setup is not part of either.
"""
import fnmatch
import struct
import sys
import zipfile
from pathlib import Path

AMD64, PE32PLUS = 0x8664, 0x20b
HIGH_ENTROPY_VA, DYNAMIC_BASE = 0x0020, 0x0040
FOUR_GB = 1 << 32
NAMES = {0x14c: 'i386 (32-bit)', 0xaa64: 'ARM64', AMD64: 'x86-64'}
SETUP_STUBS = ('uninstall.exe', 'opencnc3d-setup*.exe')
HEAD = 4096
NO_IMPORT = ('sdl2.dll',)
MAX_IMPORTS = 4096
MAX_THUNKS = 65536
ORDINAL_FLAG = 1 << 63
# Functions whose oldest Windows is 8 or later, by the minimum supported client in each
# one's documentation. Matched by name whatever DLL imports them, because the same name
# can come through KERNEL32, KERNELBASE or an API set.
WIN8_FUNCTIONS = frozenset("""
    CreateFile2 CopyFile2 CreateFileMappingFromApp MapViewOfFileFromApp
    VirtualAllocFromApp GetSystemTimePreciseAsFileTime GetCurrentThreadStackLimits
    WaitOnAddress WakeByAddressSingle WakeByAddressAll PrefetchVirtualMemory
    DiscardVirtualMemory OfferVirtualMemory ReclaimVirtualMemory GetOverlappedResultEx
    InitializeSynchronizationBarrier EnterSynchronizationBarrier
    DeleteSynchronizationBarrier GetProcessMitigationPolicy SetProcessMitigationPolicy
    GetProcessInformation SetProcessInformation GetThreadInformation SetThreadInformation
    GetFirmwareType GetCurrentPackageId GetCurrentPackageFullName GetPackageFullName
    SetThreadDescription GetThreadDescription IsWow64Process2 VirtualAlloc2
    MapViewOfFile3 CreateFileMapping2 QueryUnbiasedInterruptTimePrecise
    GetSystemCpuSetInformation SetThreadSelectedCpuSets CreatePseudoConsole
    ResizePseudoConsole ClosePseudoConsole GetTempPath2W GetTempPath2A
    SetProcessDpiAwarenessContext SetThreadDpiAwarenessContext
    GetThreadDpiAwarenessContext GetWindowDpiAwarenessContext
    AreDpiAwarenessContextsEqual GetAwarenessFromDpiAwarenessContext GetDpiForWindow
    GetDpiForSystem AdjustWindowRectExForDpi GetSystemMetricsForDpi
    SystemParametersInfoForDpi EnableNonClientDpiScaling
    LogicalToPhysicalPointForPerMonitorDPI PhysicalToLogicalPointForPerMonitorDPI
    GetPointerInfo GetPointerType GetPointerFrameInfo GetPointerDevices
    EnableMouseInPointer RegisterPointerInputTarget InjectTouchInput
    GetDisplayAutoRotationPreferences SetDisplayAutoRotationPreferences
    IsImmersiveProcess GetDpiForMonitor SetProcessDpiAwareness GetProcessDpiAwareness
""".split())
# DLLs that Windows 7 does not ship, or API sets it does not resolve (a name ending in
# '-' is a prefix).
WIN8_DLLS = ('shcore.dll', 'combase.dll', 'd3d12.dll', 'api-ms-win-core-synch-l1-2-0.dll',
             'api-ms-win-core-path-l1-1-0.dll', 'api-ms-win-core-winrt-')


def read_head(stream):
    """The bytes a PE header can live in: the first 4 KB, more if the header starts later."""
    data = stream.read(HEAD)
    if len(data) >= 64 and data[:2] == b'MZ':
        need = struct.unpack_from('<I', data, 60)[0] + 512
        if need > len(data):
            data += stream.read(need - len(data))
    return data


def problem_of(data):
    """What is wrong with this image as a 64-bit Windows program, or None if nothing is."""
    if len(data) < 64 or data[:2] != b'MZ':
        return 'not a PE image'
    off = struct.unpack_from('<I', data, 60)[0]
    if off + 24 > len(data) or data[off:off + 4] != b'PE\0\0':
        return 'not a PE image'
    machine = struct.unpack_from('<H', data, off + 4)[0]
    if machine != AMD64:
        return NAMES.get(machine, hex(machine))
    optsize = struct.unpack_from('<H', data, off + 20)[0]
    opt = off + 24
    if optsize < 72 or opt + 72 > len(data):
        return 'x86-64 with a truncated optional header'
    if struct.unpack_from('<H', data, opt)[0] != PE32PLUS:
        return 'x86-64 with a 32-bit (PE32) optional header'
    wrong = []
    flags = struct.unpack_from('<H', data, opt + 70)[0]
    lost = [n for bit, n in ((DYNAMIC_BASE, 'DYNAMIC_BASE'), (HIGH_ENTROPY_VA, 'HIGH_ENTROPY_VA'))
            if not flags & bit]
    if lost:
        wrong.append('without ' + ' and '.join(lost))
    base = struct.unpack_from('<Q', data, opt + 24)[0]
    if base < FOUR_GB:
        wrong.append(f'with its image base at 0x{base:x}, below 4 GB')
    return ('x86-64 PE32+ but ' + ', '.join(wrong)) if wrong else None


def read_at(stream, offset, n):
    stream.seek(offset)
    return stream.read(n)


def imported_dlls(stream, data):
    """The DLL names in the import directory of an image problem_of accepted."""
    return [dll for dll, _ in imports_of(stream, data)]


def imports_of(stream, data):
    """(DLL name, [function names]) for each entry in the import directory of an image
    problem_of accepted, read through stream. Functions imported by ordinal carry no name
    and are left out. Raises ValueError for a directory that is named but cannot be read."""
    off = struct.unpack_from('<I', data, 60)[0]
    nsect = struct.unpack_from('<H', data, off + 6)[0]
    optsize = struct.unpack_from('<H', data, off + 20)[0]
    opt = off + 24
    # PE32+: NumberOfRvaAndSizes at 108, the data directories from 112, imports second.
    if optsize < 112 + 16:
        return []
    head = read_at(stream, opt, optsize)
    if len(head) < 112 + 16 or struct.unpack_from('<I', head, 108)[0] < 2:
        return []
    rva = struct.unpack_from('<I', head, 112 + 8)[0]
    if rva == 0:
        return []
    table = read_at(stream, opt + optsize, 40 * nsect)
    if len(table) < 40 * nsect:
        raise ValueError('its section table is cut short')
    sections, cache = [], {}
    for i in range(nsect):
        vsize, va, rawsize, rawptr = struct.unpack_from('<IIII', table, 40 * i + 8)
        sections.append((va, max(vsize, rawsize), rawsize, rawptr))

    def at(addr, n):
        """n bytes at a virtual address, out of the one section that holds it (read once)."""
        for i, (va, span, rawsize, rawptr) in enumerate(sections):
            if va <= addr < va + span:
                if i not in cache:
                    cache[i] = read_at(stream, rawptr, rawsize)
                return cache[i][addr - va:addr - va + n]
        raise ValueError(f'its imports name address 0x{addr:x}, which no section holds')

    names = []
    for k in range(MAX_IMPORTS):
        desc = at(rva + 20 * k, 20)
        if len(desc) < 20:
            raise ValueError('its import table runs off the end of its section')
        if desc == bytes(20):
            return names
        name = at(struct.unpack_from('<I', desc, 12)[0], 256).split(b'\0', 1)[0]
        # The lookup table (OriginalFirstThunk) names each function; a linker that left it
        # out has the same 8-byte PE32+ thunks in the address table (FirstThunk) on disk.
        thunk = struct.unpack_from('<I', desc, 0)[0] or struct.unpack_from('<I', desc, 16)[0]
        funcs = []
        for t in range(MAX_THUNKS if thunk else 0):
            entry = at(thunk + 8 * t, 8)
            if len(entry) < 8:
                raise ValueError('an import lookup table runs off the end of its section')
            value = struct.unpack_from('<Q', entry)[0]
            if value == 0:
                break
            if not value & ORDINAL_FLAG:
                func = at((value & 0x7fffffff) + 2, 256).split(b'\0', 1)[0]
                funcs.append(func.decode('ascii', 'replace'))
        else:
            if thunk:
                raise ValueError(f'an import lookup table has no end within {MAX_THUNKS} entries')
        names.append((name.decode('ascii', 'replace'), funcs))
    raise ValueError(f'its import table has no end within {MAX_IMPORTS} entries')


def import_problem(stream, data):
    """What is wrong with what this image imports, or None if nothing is."""
    try:
        table = imports_of(stream, data)
    except (ValueError, struct.error) as e:
        return f'x86-64 PE32+ whose imports cannot be read: {e}'
    banned = [d for d, _ in table if d.lower() in NO_IMPORT]
    if banned:
        return ('x86-64 PE32+ but imports ' + ', '.join(banned)
                + ', which no Windows build ships (SDL is linked into every program)')
    newer = [d for d, _ in table if d.lower().startswith(WIN8_DLLS)]
    newer += [f'{d}!{f}' for d, funcs in table for f in funcs if f in WIN8_FUNCTIONS]
    if newer:
        return ('x86-64 PE32+ but imports ' + ', '.join(newer) + ', which Windows 7 does'
                ' not have, so Windows 7 refuses to load the program at all')
    return None


def is_program(name):
    return name.lower().endswith(('.exe', '.dll'))


def is_setup_stub(name):
    base = name.replace('\\', '/').rsplit('/', 1)[-1].lower()
    return any(fnmatch.fnmatchcase(base, pattern) for pattern in SETUP_STUBS)


def entries(target):
    """(label, file name, header bytes, open stream) for every .exe and .dll in a folder, a
    zip or a single file; the stream stays open until the next one is asked for. The bytes
    are None for something that is none of those, False for a target that does not exist,
    and the stream is then None."""
    path = Path(target)
    if path.is_dir():
        for p in sorted(path.rglob('*')):
            if p.is_file() and is_program(p.name):
                with p.open('rb') as f:
                    yield f'{target}: {p.relative_to(path).as_posix()}', p.name, read_head(f), f
    elif path.is_file() and is_program(path.name):
        with path.open('rb') as f:
            yield target, path.name, read_head(f), f
    elif path.is_file() and zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            for name in z.namelist():
                if is_program(name):
                    with z.open(name) as f:
                        yield f'{target}: {name}', name, read_head(f), f
    else:
        yield target, path.name, (None if path.exists() else False), None


def main(argv):
    installed = '--installed' in argv
    targets = [a for a in argv if a != '--installed']
    if not targets or any(a.startswith('--') for a in targets):
        print(__doc__.strip(), file=sys.stderr)
        return 2
    bad, skipped, seen = [], [], 0
    for target in targets:
        for label, name, data, stream in entries(target):
            if data is False:
                bad.append(f'{label} does not exist')
            elif data is None:
                bad.append(f'{label} is not a folder, a zip, or an .exe or .dll')
            elif installed and is_setup_stub(name):
                skipped.append(label)
            else:
                seen += 1
                what = problem_of(data) or import_problem(stream, data)
                if what:
                    bad.append(f'{label} is {what}')
    for label in skipped:
        print(f'pe-arch: skipped {label} (a 32-bit Setup program, asked with --installed)')
    if bad:
        print('Windows programs are 64-bit only, must load anywhere in the 64-bit address'
              ' space, must not import SDL2.dll and must import nothing newer than'
              ' Windows 7, and these do not hold:', file=sys.stderr)
        for line in bad:
            print('  ' + line, file=sys.stderr)
        return 1
    if not seen:
        print('no .exe or .dll found in ' + ', '.join(targets), file=sys.stderr)
        return 1
    print(f'pe-arch: {seen} Windows program file(s), every one x86-64 PE32+, high-entropy'
          ' relocatable, image base above 4 GB, importing no SDL2.dll and nothing newer'
          ' than Windows 7')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
