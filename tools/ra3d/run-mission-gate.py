#!/usr/bin/env python3
"""Load an original Red Alert mission twice and verify real object state."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from ramix import RaMix

TIMEOUT_SECONDS = 60


def normalized(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n")


def run_once(host: Path, brain: Path, scenario_dir: Path, run_number: int, mode: str) -> bytes:
    environment = os.environ.copy()
    environment["_NO_DEBUG_HEAP"] = "1"
    try:
        process = subprocess.run(
            [str(host), str(brain), str(scenario_dir), "60", mode],
            cwd=scenario_dir,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            timeout=TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired as error:
        if error.stderr:
            sys.stderr.buffer.write(error.stderr)
        raise RuntimeError(f"{mode} run {run_number} timed out") from error
    if process.returncode:
        sys.stderr.buffer.write(process.stderr)
        sys.stderr.buffer.write(normalized(process.stdout))
        raise RuntimeError(f"{mode} run {run_number} exited with status {process.returncode}")
    return normalized(process.stdout)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--brain", required=True, type=Path)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--expected", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()

    host = args.host.resolve()
    brain = args.brain.resolve()
    data = args.data.resolve()
    abi_expected = args.expected.with_name("expected-abi-report.txt")
    for required in (host, brain, data / "GENERAL.MIX", data / "LOCAL.MIX", args.expected, abi_expected):
        if not required.is_file():
            raise FileNotFoundError(required)

    with tempfile.TemporaryDirectory(prefix="ra3d-mission-") as temporary:
        staging = Path(temporary)
        for archive_name, entry in (
            ("GENERAL.MIX", "SCG01EA.INI"),
            ("GENERAL.MIX", "TUTORIAL.INI"),
            ("LOCAL.MIX", "RULES.INI"),
        ):
            content = RaMix(data / archive_name).read(entry)
            if content is None:
                raise RuntimeError(f"{entry} missing from {archive_name}")
            (staging / entry).write_bytes(content)
        first = run_once(host, brain, staging, 1, "mission")
        second = run_once(host, brain, staging, 2, "mission")
        abi_first = run_once(host, brain, staging, 1, "mission-abi")
        abi_second = run_once(host, brain, staging, 2, "mission-abi")

    if first != second:
        raise RuntimeError("two fixed-seed mission runs produced different object reports")
    if abi_first != abi_second:
        raise RuntimeError("two fixed-seed mission ABI runs produced different reports")
    expected = normalized(args.expected.read_bytes())
    if first != expected:
        actual = args.report.with_suffix(".actual.txt")
        actual.parent.mkdir(parents=True, exist_ok=True)
        actual.write_bytes(first)
        raise RuntimeError(f"disc mission object state differs from expected report: {actual}")
    if abi_first != normalized(abi_expected.read_bytes()):
        actual = args.report.with_suffix(".abi-actual.txt")
        actual.parent.mkdir(parents=True, exist_ok=True)
        actual.write_bytes(abi_first)
        raise RuntimeError(f"disc mission renderer ABI differs from expected report: {actual}")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_bytes(first)
    print(f"Red Alert disc mission and renderer ABI gates passed twice: {args.report}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"ra3d mission gate: {error}", file=sys.stderr)
        raise SystemExit(1)
