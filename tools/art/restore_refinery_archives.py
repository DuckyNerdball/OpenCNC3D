"""Reassemble oversized Refinery .blend checkpoints from committed parts.

Run from any directory: python3 tools/art/restore_refinery_archives.py
Existing correct files are left untouched; incorrect files raise an error.
"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / 'art/refinery/ARCHIVE_PARTS.json'


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def restore():
    entries = json.loads(MANIFEST.read_text())['files']
    for entry in entries:
        destination = ROOT / entry['path']
        if destination.exists():
            assert destination.stat().st_size == entry['size'] and sha256(destination) == entry['sha256'], destination
            print('Already valid:', entry['path'])
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_name(destination.name + '.restoring')
        try:
            digest = hashlib.sha256()
            size = 0
            with temporary.open('wb') as output:
                for piece in entry['parts']:
                    source = ROOT / piece['path']
                    assert source.stat().st_size == piece['size'] and sha256(source) == piece['sha256'], source
                    with source.open('rb') as stream:
                        for block in iter(lambda: stream.read(1024 * 1024), b''):
                            output.write(block)
                            digest.update(block)
                            size += len(block)
            assert size == entry['size'] and digest.hexdigest() == entry['sha256'], entry['path']
            temporary.replace(destination)
        finally:
            temporary.unlink(missing_ok=True)
        print('Restored:', entry['path'])


if __name__ == '__main__':
    restore()
