#!/usr/bin/env python3
"""Stage RA's encrypted audio archives for OpenCNC3D's existing AUD mixer.

The disc archives remain authoritative. This writes plain MIX headers around their
unchanged data blocks, since audio/mixfile.c deliberately reads plain MIX only.
LOCAL.MIX becomes RA_LOCAL.MIX so the audio bank can open its menu and map themes.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct

from ramix import RaMix

AUDIO_ARCHIVES = ("SOUNDS.MIX", "SPEECH.MIX", "SCORES.MIX",
                  "ALLIES.MIX", "RUSSIAN.MIX")


def stage_one(source_path: Path, destination_path: Path) -> None:
    if source_path.resolve() == destination_path.resolve():
        raise ValueError("the staged archive must not replace the original disc archive")
    source = RaMix(source_path)
    if source.count != len(source.index):
        raise ValueError(f"{source_path.name}: duplicate archive IDs")
    header = bytearray(struct.pack("<HI", source.count, source.size))
    for entry_id, (absolute_offset, size) in source.index.items():
        header += struct.pack("<III", entry_id, absolute_offset - source.start, size)
    payload = source.data[source.start : source.start + source.size]
    plain = bytes(header) + payload
    if len(payload) != source.size:
        raise ValueError(f"{source_path.name}: truncated data block")

    # Check the same plain layout that the C audio bank reads, for every entry.
    data_start = 6 + source.count * 12
    for index, (entry_id, (original, size)) in enumerate(source.index.items()):
        found_id, offset, found_size = struct.unpack_from("<III", plain, 6 + index * 12)
        if (found_id, found_size) != (entry_id, size):
            raise ValueError(f"{source_path.name}: entry {index} changed")
        if plain[data_start + offset : data_start + offset + size] != source.data[original : original + size]:
            raise ValueError(f"{source_path.name}: entry {index} payload changed")

    destination_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination_path.with_suffix(".tmp")
    temporary.write_bytes(plain)
    temporary.replace(destination_path)
    print(f"{source_path.name}: {source.count} verified entries, {len(plain)} bytes")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    for name in AUDIO_ARCHIVES:
        source = args.data / name
        if not source.is_file():
            parser.error(f"missing {source}")
        stage_one(source, args.out / name)
    allies = RaMix(args.data / "ALLIES.MIX")
    russian = RaMix(args.data / "RUSSIAN.MIX")
    if allies.read("ACKNO.V00") is None or russian.read("ACKNO.R00") is None:
        raise ValueError("original Allied and Soviet voice archives are incomplete")
    local_path = args.data / "LOCAL.MIX"
    if not local_path.is_file():
        parser.error(f"missing {local_path}")
    local = RaMix(local_path)
    for name in ("INTRO.AUD", "MAP.AUD"):
        if not local.read(name):
            raise ValueError(f"{local_path.name} has no {name}")
    stage_one(local_path, args.out / "RA_LOCAL.MIX")


if __name__ == "__main__":
    main()
