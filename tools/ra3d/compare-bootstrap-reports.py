#!/usr/bin/env python3
"""Compare canonical Red Alert bootstrap reports from two platforms."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


def normalized(path: Path) -> bytes:
    return path.read_bytes().replace(b"\r\n", b"\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    args = parser.parse_args()
    left = normalized(args.left)
    right = normalized(args.right)
    if not left or not right:
        sys.stderr.write("bootstrap reports must not be empty\n")
        return 1
    if left == right:
        print("Red Alert bootstrap reports are equivalent.")
        return 0
    left_lines = left.decode("ascii").splitlines(keepends=True)
    right_lines = right.decode("ascii").splitlines(keepends=True)
    limit = max(len(left_lines), len(right_lines))
    for index in range(limit):
        left_line = left_lines[index] if index < len(left_lines) else "<missing>"
        right_line = right_lines[index] if index < len(right_lines) else "<missing>"
        if left_line != right_line:
            sys.stderr.write(f"first difference at report line {index + 1}\n")
            sys.stderr.write(f"left : {left_line!r}\n")
            sys.stderr.write(f"right: {right_line!r}\n")
            break
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
