#!/usr/bin/env python3
"""Run the Red Alert bootstrap twice and persist its canonical report."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

BOOTSTRAP_TIMEOUT_SECONDS = 60


def normalize_report(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n")


def run_once(host: Path, brain: Path, fixture: Path, run_number: int) -> tuple[bytes, bytes]:
    environment = os.environ.copy()
    environment["_NO_DEBUG_HEAP"] = "1"
    try:
        process = subprocess.run(
            [str(host), str(brain), str(fixture), "60"],
            cwd=fixture,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            timeout=BOOTSTRAP_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired as error:
        if error.stderr:
            sys.stderr.buffer.write(error.stderr)
        if error.stdout:
            sys.stderr.write(f"partial report from run {run_number}:\n")
            sys.stderr.buffer.write(normalize_report(error.stdout))
        raise RuntimeError(
            f"bootstrap run {run_number} timed out after {BOOTSTRAP_TIMEOUT_SECONDS} seconds"
        ) from error
    if process.returncode != 0:
        sys.stderr.buffer.write(process.stderr)
        if process.stdout:
            sys.stderr.write(f"partial report from run {run_number}:\n")
            sys.stderr.buffer.write(normalize_report(process.stdout))
        raise RuntimeError(f"bootstrap run {run_number} exited with status {process.returncode}")
    return normalize_report(process.stdout), normalize_report(process.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--brain", required=True, type=Path)
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()

    host = args.host.resolve()
    brain = args.brain.resolve()
    fixture = args.fixture.resolve()
    expected_path = fixture / "expected-report.txt"
    if not fixture.is_dir():
        raise FileNotFoundError(f"fixture directory: {fixture}")
    for required in (host, brain, fixture / "RA3DBOOT.mpr", expected_path):
        if not required.is_file():
            raise FileNotFoundError(required)
    if os.name != "nt" and not os.access(host, os.X_OK):
        raise RuntimeError(f"bootstrap host is not executable: {host}")

    first, first_diagnostics = run_once(host, brain, fixture, 1)
    second, second_diagnostics = run_once(host, brain, fixture, 2)
    if first != second:
        sys.stderr.buffer.write(first_diagnostics)
        sys.stderr.buffer.write(second_diagnostics)
        sys.stderr.write("first report:\n")
        sys.stderr.buffer.write(first)
        sys.stderr.write("second report:\n")
        sys.stderr.buffer.write(second)
        raise RuntimeError("two identical bootstrap runs produced different reports")

    expected = normalize_report(expected_path.read_bytes())
    if first != expected:
        sys.stderr.buffer.write(first_diagnostics)
        sys.stderr.write("canonical report differs from expected-report.txt\n")
        sys.stderr.write("actual:\n")
        sys.stderr.buffer.write(first)
        raise RuntimeError("unexpected Red Alert bootstrap state")

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_bytes(first)
    sys.stdout.write(f"Red Alert bootstrap gate passed twice: {args.report}\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        sys.stderr.write(f"ra3d bootstrap gate: {error}\n")
        raise SystemExit(1)
