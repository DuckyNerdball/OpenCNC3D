#!/usr/bin/env python3
"""Read G268's three engine dumps: thrown rocks must sleep and remain still."""
import sys


def check(path):
    groups = []
    for line in open(path, encoding="latin-1"):
        if line.startswith("FOLDUMP|"):
            groups.append({})
        elif line.startswith("ROCKDYN|") and groups:
            fields = line.strip().split("|")
            groups[-1][fields[1]] = dict(p.split("=", 1) for p in fields[2:])
    if len(groups) != 3 or any(len(g) != 4 for g in groups):
        return "0 expected three dumps of the four thrown rocks"
    moving, rest, later = groups
    if rest.keys() != moving.keys() or later.keys() != rest.keys():
        return "0 the thrown rock identities changed"
    if not any(d["sleep"] == "0" for d in moving.values()):
        return "0 no rock was moving after the pulse"
    for key, d in rest.items():
        e = later[key]
        if d["sleep"] != "1" or e["sleep"] != "1":
            return "0 rock %s never settled or woke without another hit" % key
        if d["gone"] != "0" or e["gone"] != "0":
            return "0 rock %s disappeared instead of settling" % key
        if d["at"] != e["at"] or d["v"] != "0.000,0.000,0.000":
            return "0 rock %s drifted after settling" % key
    return "1 four thrown rocks asleep by 31 seconds, still present at 41 seconds, " \
           "zero velocity and unchanged positions at the dump's 0.001-cell precision"


if __name__ == "__main__":
    print(check(sys.argv[1]))
