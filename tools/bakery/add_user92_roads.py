#!/usr/bin/env python3
"""Paint a connected road network onto the existing raised USER92 test plateau.

Only the road template/icon bytes in USER92.BIN are changed. The original height
map, cliff rim, mission settings and Mammoth tank are kept as authored.
"""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
MAP = ROOT / "game/examples/USER92.BIN"
TABLE = ROOT / "game/terrain_tiles.h"
TEMPLATES = ROOT / "game/edit_tables.h"


def road_templates():
    found = {}
    for line in TEMPLATES.read_text().splitlines():
        match = re.search(r'\{ "(D\d\d)",.*?, (\d+), (\d+) \},\s*/\* (\d+) \*/', line)
        if match:
            found[match[1]] = tuple(map(int, match.groups()[1:]))
    return found


def used_icons():
    raw = TABLE.read_text().split("static const TtSlot TT_TEMPERAT[TT_TEMPERAT_N] = {", 1)[1].split("};", 1)[0]
    icons = {}
    for tid, icon in re.findall(r'\{\s*(\d+),\s*(\d+),\s*\d+,\s*\d+\s*\}', raw):
        icons.setdefault(int(tid), set()).add(int(icon))
    return icons


def main():
    data = bytearray(MAP.read_bytes())
    if len(data) != 64 * 64 * 2:
        raise ValueError("USER92.BIN is not a 64x64 template/icon map")
    templates, icons = road_templates(), used_icons()
    # D03/D29 cap the north and south ends; D15 joins the east spur to the
    # vertical track. D11 is a straight east-west segment; D28 caps its end.
    # All road exits are paired with an entry on the next template.
    placements = [
        ("D03", 28, 26),
        ("D15", 28, 28),
        ("D06", 28, 31),
        ("D06", 28, 34),
        ("D29", 28, 37),
        ("D11", 31, 27),
        ("D11", 33, 27),
        ("D11", 35, 27),
        ("D28", 37, 28),
    ]
    painted = set()
    for name, left, top in placements:
        width, height, tid = templates[name]
        for icon in sorted(icons[tid]):
            x, y = left + icon % width, top + icon // width
            if not (26 <= x <= 39 and 26 <= y <= 39):
                raise ValueError(f"{name}/{icon} leaves plateau at {x},{y}")
            if (x, y) in painted:
                raise ValueError(f"road overlap at {x},{y}")
            pos = 2 * (y * 64 + x)
            old_tid = data[pos]
            if old_tid not in (255, tid):
                raise ValueError(f"{name}/{icon} would overwrite template {old_tid} at {x},{y}")
            data[pos:pos + 2] = bytes((tid, icon))
            painted.add((x, y))
    MAP.write_bytes(data)
    print(f"USER92: {len(painted)} road cells across {len(placements)} connected templates")


if __name__ == "__main__":
    main()
