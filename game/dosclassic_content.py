#!/usr/bin/env python3
"""Make sure a play folder's theater archives carry the 1995 aprons, craters and scorches.

DOS Classic draws the ground layer from the engine's own map export, and the engine
exports a smudge cell only when its shape loaded: SmudgeTypeClass::Init reads BIB1.TEM,
CR1.DES and so on from content/TEMPERAT.MIX and content/DESERT.MIX, and Cell_Class_Draw_It
skips a smudge whose image data is NULL. A theater archive repacked without those members
starts, runs and plays normally, and Classic simply draws grass or sand where every
building apron, crater and scorch mark should be. Nothing else notices.

  dosclassic_content.py <content dir>                       check only
  dosclassic_content.py <content dir> --merge <dosdata dir> add what is missing, then check

--merge copies only the missing members from the original 1995 archive beside them
(bake_dosclassic_mix.py, which asserts every existing member stays byte-identical) and
leaves an archive that already has them untouched. With --original, the check also asks
that every required member is byte-identical to the one in the original archive.

Prints one line per theater, DOS2D|content|<theater>|members=N|smudges=15/15, and exits 1
naming every missing or altered member.
"""
import argparse
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'menu' / 'tools'))
sys.path.insert(0, str(HERE))
from mixshp import MixFile, mix_id

# The smudge shapes Tiberian Dawn loads per theater (sdata.cpp): three building aprons,
# six craters and six scorch marks. Theater archives carry them under the theater's own
# extension. WINTER is not listed: no 1995 winter archive with these shapes is held.
SMUDGES = ['BIB1', 'BIB2', 'BIB3'] + ['CR%d' % i for i in range(1, 7)] + \
          ['SC%d' % i for i in range(1, 7)]
THEATERS = (('TEMPERAT', 'TEM'), ('DESERT', 'DES'))


def check(content, original_dir=None):
    problems = []
    for theater, ext in THEATERS:
        path = content / (theater + '.MIX')
        if not path.is_file():
            problems.append('%s is missing' % path)
            continue
        mix = MixFile(str(path))
        original = None
        if original_dir is not None:
            opath = original_dir / (theater + '.MIX')
            if not opath.is_file():
                # Asked to compare byte for byte: a missing original is a failure, not a
                # quiet fall back to checking membership only.
                problems.append('the 1995 archive %s is missing, so %s cannot be compared'
                                % (opath, path))
            else:
                original = MixFile(str(opath))
        found, absent, altered = 0, [], []
        for name in SMUDGES:
            member = name + '.' + ext
            key = mix_id(member)
            if key not in mix.index:
                absent.append(member)
            elif original is not None and key not in original.index:
                altered.append(member + ' (not in the 1995 archive)')
            elif original is not None and mix.read_id(key) != original.read_id(key):
                altered.append(member)
            else:
                found += 1
        if absent:
            problems.append('%s has no %s' % (path, ' '.join(absent)))
        if altered:
            problems.append('%s differs from the 1995 archive in %s' % (path, ' '.join(altered)))
        print('DOS2D|content|%s|members=%d|smudges=%d/%d' %
              (theater, len(mix.index), found, len(SMUDGES)))
    return problems


def merge(content, dosdata):
    from bake_dosclassic_mix import merge as merge_mix
    for theater, ext in THEATERS:
        target = content / (theater + '.MIX')
        original = dosdata / (theater + '.MIX')
        if not original.is_file():
            raise SystemExit('the 1995 archive %s is missing, so %s cannot be completed'
                             % (original, target))
        if not target.is_file():
            continue          # check() names it; seeding a folder is the packager's job
        mix = MixFile(str(target))
        if all(mix_id(n + '.' + ext) in mix.index for n in SMUDGES):
            continue          # complete already: leave the bytes exactly as they are
        tmp = target.with_name(target.name + '.classic.tmp')
        merge_mix(target, original, tmp, ext)
        tmp.replace(target)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('content', type=pathlib.Path)
    ap.add_argument('--merge', type=pathlib.Path, metavar='DOSDATA',
                    help='add the missing members from the 1995 archives in this folder')
    ap.add_argument('--original', type=pathlib.Path, metavar='DOSDATA',
                    help='also require each member to match the 1995 archive byte for byte')
    a = ap.parse_args()
    if a.merge is not None:
        merge(a.content, a.merge)
    problems = check(a.content, a.original or a.merge)
    if problems:
        for p in problems:
            print('DOS2D|content|MISSING|' + p, file=sys.stderr)
        print('DOS Classic would draw no building aprons, craters or scorch marks from '
              'this folder. Run game/dosclassic_content.py %s --merge data/dosdata.'
              % a.content, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
