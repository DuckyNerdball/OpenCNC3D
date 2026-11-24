#!/usr/bin/env python3
"""Run real headless TD/RA simulations for fair, per-seat AI regression checks."""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]

# The idle seat's fixed base on SCM01EA for the long growth jobs: a Construction Yard,
# four power plants and nine guard towers in the far corner from the computer's start.
# Without it the computer finds the seat's lone MCV and the match ends long before the
# economy can be judged.
TD_FORTRESS = ("FACT:47:39;NUKE:44:43;NUKE:47:43;NUKE:50:43;NUKE:51:39;GTWR:43:37;GTWR:45:36;"
               "GTWR:47:36;GTWR:49:36;GTWR:42:40;GTWR:42:42;GTWR:53:36;GTWR:54:41;GTWR:41:45")
VEHICLES = {"BGGY", "JEEP", "BIKE", "APC", "ARTY", "FTNK", "LTNK", "MTNK", "HTNK", "MLRS",
            "MSAM", "STNK"}


def fields(line):
    return dict(part.split("=", 1) for part in line.split("|") if "=" in part)


def census(folder, snapshots):
    """Per computer house, the most of each thing seen in any snapshot."""
    seen = {}
    attacks = {}
    for line in (folder / "probe.log").read_text().splitlines():
        if line.startswith("FAIRAI|"):
            row = fields(line)
            attacks[int(row["house"])] = int(row["attacks"])
    for snapshot in snapshots:
        world = (folder / f"world-{int(snapshot['tick']):06d}.txt").read_text().splitlines()
        humans = {line.split("|")[1] for line in world if line.startswith("HOUSE|") and "|human=1|" in line}
        rows = [line.split("|") for line in world if line.startswith("OBJ|")]
        houses = {row[3] for row in rows if row[3].startswith("Multi") and row[3] not in humans}
        for house in houses:
            mine = [row for row in rows if row[3] == house]
            info = [dict(part.split("=", 1) for part in row[4:] if "=" in part) for row in mine]
            placed = [(row, f) for row, f in zip(mine, info) if f.get("limbo") == "0"]
            yard = [(int(f["x"]), int(f["y"])) for row, f in placed if row[1] == "BUILDING" and row[2] == "FACT"]
            buildings = [row[2] for row, f in placed if row[1] == "BUILDING"]
            # A harvester inside its refinery is in limbo but still the house's.
            harvesters = sum(1 for row, f in zip(mine, info) if row[1] == "UNIT" and row[2] == "HARV" and f.get("x") != "127")
            out = 0
            if yard:
                out = sum(1 for row, f in placed if row[1] == "UNIT" and row[2] in VEHICLES
                          and abs(int(f["x"]) - yard[0][0]) + abs(int(f["y"]) - yard[0][1]) > 15)
            entry = seen.setdefault(house, dict(refineries=0, harvesters=0, buildings=0, vehicles=0,
                                                vehicles_out=0, types=[], attacks=0))
            entry["refineries"] = max(entry["refineries"], buildings.count("PROC"))
            entry["harvesters"] = max(entry["harvesters"], harvesters)
            entry["buildings"] = max(entry["buildings"], len(buildings))
            entry["vehicles"] = max(entry["vehicles"], sum(1 for row, f in placed if row[1] == "UNIT" and row[2] in VEHICLES))
            entry["vehicles_out"] = max(entry["vehicles_out"], out)
            entry["types"] = sorted(set(entry["types"]) | set(buildings))
    # FAIRAI rows name houses by number; Multi1 is house 4 in Tiberian Dawn.
    for house, entry in seen.items():
        entry["attacks"] = attacks.get(3 + int(house[5:]), 0)
    return seen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", type=Path, default=ROOT / "playable")
    parser.add_argument("--td", type=Path)
    parser.add_argument("--ra", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--ticks", type=int, default=12000)
    parser.add_argument("--only", default="",
                        help="comma separated job-name prefixes; the cross-job comparisons need the full matrix")
    args = parser.parse_args()
    install, out = args.install.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    wire = out / "gate-ai-difficulty"
    subprocess.run(["cc", "-std=gnu89", "-O2", "-o", str(wire),
                    *[str(ROOT / "net" / name) for name in
                      ("gate_ai_difficulty.c", "lockstep.c", "net_udp.c", "roomcode.c")]], check=True)
    subprocess.run([str(wire)], check=True)
    binaries = {}
    for game in ("td", "ra"):
        binary = out / ("probe-" + game)
        command = ["clang++", "-std=c++14", "-O1", "-fms-extensions", "-fdeclspec"]
        if game == "ra":
            command += ["-DFAIR_AI_RED_ALERT"]
        command += ["-o", str(binary), str(ROOT / "brain/host/fair_ai_probe.cpp")]
        subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
        binaries[game] = binary
    libs = {"td": (args.td or install / "TiberianDawn.dylib").resolve(),
            "ra": (args.ra or install / "ra3d/RedAlert.dylib").resolve()}
    jobs = []
    only = [x for x in args.only.split(",") if x]
    for game in ("td", "ra"):
        scenario = "SCM01EA" if game == "td" else "SCM08EA"
        for level in range(4):
            jobs.append(dict(name=f"{game}-level-{level}", game=game, level=level,
                             scenario=scenario, save=3000))
        for suffix, options in [
                ("repeat", {}), ("hidden-money", {"perturb": 1}),
                ("context", {"context": 1}), ("same-country", {"perturb": 2}),
                ("mixed", {"level": -1, "bots": 4, "scenario": "SCM01EA", "save": 3000}),
                ("other-seed", {"seed": 4243})]:
            job = dict(name=f"{game}-{suffix}", game=game, level=1,
                       scenario=scenario, save=0)
            job.update(options)
            jobs.append(job)
        jobs.append(dict(name=f"{game}-lowtech", game=game, level=3,
                         scenario=scenario, save=3000, tech=1))
        if game == "td":
            # Long Expert games on the lobby's 5000 credits against an idle fortified
            # seat, once with the computer on each side. Then a corner start: on SCM04EA
            # the same fortress stands beside one computer's corner, whose yard used to
            # deploy against the map edge, see too little ground for a refinery and
            # never build again.
            for side, perturb in (("nod", 0), ("gdi", 2)):
                jobs.append(dict(name=f"td-growth-{side}", game=game, level=3, scenario=scenario,
                                 save=0, perturb=perturb, credits=5000, ticks=18000, every=900,
                                 spawn=TD_FORTRESS, growth=True))
            jobs.append(dict(name="td-corner", game=game, level=3, scenario="SCM04EA", save=0,
                             bots=2, credits=5000, ticks=9000, every=900, spawn=TD_FORTRESS,
                             corner=True))
        if game == "ra":
            jobs.append(dict(name="ra-regrow", game=game, level=3,
                             scenario=scenario, save=3000, perturb=4))

    if only:
        jobs = [job for job in jobs if any(job["name"].startswith(prefix) for prefix in only)]

    def run(job):
        game, folder = job["game"], out / job["name"]
        folder.mkdir(exist_ok=True)
        module = install if game == "td" else install / "ra3d"
        if game == "td":
            for name in ("CONQUER.MIX", "content"):
                link = folder / name
                if not link.exists():
                    link.symlink_to(module / name)
        command = [str(binaries[game]), str(libs[game]), str(module / "missions") + "/",
                   str(module / ("content" if game == "td" else "data")) + "/",
                   job["scenario"], str(job["level"]), str(job.get("ticks", args.ticks)),
                   str(job.get("seed", 4242)), str(job.get("bots", 1)),
                   str(job.get("save", 0)), str(job.get("perturb", 0))]
        env = dict(os.environ, OPENCNC3D_BACKGROUND="1")
        if job.get("tech"):
            env["FAIR_AI_TECH"] = str(job["tech"])
        if job.get("context"):
            env["FAIR_AI_CONTEXT"] = str(job["context"])
        for key, name in (("credits", "FAIR_AI_CREDITS"), ("spawn", "FAIR_AI_SPAWN"), ("every", "FAIR_AI_EVERY")):
            if job.get(key):
                env[name] = str(job[key])
        with (folder / "probe.log").open("w") as log:
            process = subprocess.run(command, cwd=folder, env=env, stdout=log,
                                     stderr=subprocess.STDOUT, timeout=120)
        text = (folder / "probe.log").read_text()
        assert process.returncode == 0 and "PROBE|pass=1|" in text, (job["name"], text[-1800:])
        if job.get("save"):
            assert "PROBE|roundtrip=pass|" in text, job["name"]
        snapshots = [fields(line) for line in text.splitlines() if line.startswith("PROBE|tick=")]
        diagnostics = [fields(line) for line in text.splitlines() if line.startswith("FAIRAI|")]
        rules = [fields(line) for line in text.splitlines() if line.startswith("FAIRRULE|")]
        assert diagnostics and rules, (job["name"], "missing real controller/rule telemetry")
        if job["level"] >= 0:
            assert all(int(row["level"]) == job["level"] for row in diagnostics), job["name"]
            assert int(diagnostics[-1].get("think", diagnostics[-1].get("thinks", 0))) > 0
            assert int(diagnostics[-1]["orders"]) > 0, (job["name"], "no orders")
            assert int(diagnostics[-1]["scouts"]) > 0, (job["name"], "no scouting")
        else:
            assert {int(row["level"]) for row in diagnostics} == {0, 1, 2, 3}, job["name"]
        assert all(int(row["difficulty"]) == 1 for row in rules), (job["name"], "non-neutral handicap")
        if job.get("perturb") == 2:
            human = next(row for row in rules if row["human"] == "1")
            computer = next(row for row in rules if row["human"] == "0")
            for key in ("fire", "ground", "air", "armor", "rof", "cost", "build"):
                assert human[key] == computer[key], (job["name"], key, human, computer)
        # Census deployed structures, deliberately excluding pending/limbo objects.
        maximum = 0
        max_refineries = 0
        for snapshot in snapshots:
            world = (folder / f"world-{int(snapshot['tick']):06d}.txt").read_text()
            humans = {line.split("|")[1] for line in world.splitlines()
                      if line.startswith("HOUSE|") and "|human=1|" in line}
            buildings = [line for line in world.splitlines() if line.startswith("OBJ|BUILDING|")
                         and line.split("|")[3] not in humans and "|limbo=0|" in line]
            maximum = max(maximum, len(buildings))
            max_refineries = max(max_refineries, sum(line.split("|")[2] == "PROC" for line in buildings))
        if job.get("tech") == 1:
            assert max_refineries >= 2, (job["name"], "unavailable factory blocked legal expansion", max_refineries)
            assert int(diagnostics[-1]["attacks"]) > 0, (job["name"], "no low-tech attacks")
        assert maximum >= 4, (job["name"], "economy failed to develop", maximum)
        growth = {}
        if job.get("growth") or job.get("corner"):
            growth = census(folder, snapshots)
            print("GROWTH", job["name"], json.dumps(growth, sort_keys=True), flush=True)
            # Every computer seat must be in the census; an empty one would pass the
            # checks below without judging anything.
            assert len(growth) == job.get("bots", 1), (job["name"], "census found the wrong computer houses", sorted(growth))
        if job.get("growth"):
            # The computer keeps expanding and fights with vehicles, not only infantry.
            for house, seen in growth.items():
                assert seen["refineries"] >= 2, (job["name"], house, "never built a second refinery", seen)
                assert seen["harvesters"] >= 2, (job["name"], house, "never ran two harvesters", seen)
                assert "HQ" in seen["types"], (job["name"], house, "never built a Comm Center", seen)
                assert seen["vehicles_out"] >= 1, (job["name"], house, "no vehicle ever left the base", seen)
                assert seen["attacks"] > 0, (job["name"], house, "never attacked", seen)
        if job.get("corner"):
            # A start against the map edge still gets a refinery and a base.
            for house, seen in growth.items():
                assert seen["refineries"] >= 1 and seen["buildings"] >= 5, (job["name"], house, "base never grew", seen)
        result = dict(job, snapshots=snapshots, diagnostics=diagnostics[-job.get("bots", 1):],
                      deployed_buildings_max=maximum, refineries_max=max_refineries, rules=rules[-2:],
                      growth=growth)
        print("PASS", job["name"], "deployed", maximum, "ticks", snapshots[-1]["tick"], flush=True)
        return job["name"], result

    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        for name, result in executor.map(run, jobs):
            results[name] = result
    for game in ("td", "ra"):
        if f"{game}-level-1" not in results or f"{game}-hidden-money" not in results:
            continue
        base = results[f"{game}-level-1"]["snapshots"]
        for suffix in ("repeat", "context"):
            assert base == results[f"{game}-{suffix}"]["snapshots"], (game, suffix, "world or private AI state changed")
        hidden = results[f"{game}-hidden-money"]["snapshots"]
        assert [(x["tick"], x["state"]) for x in base] == [(x["tick"], x["state"]) for x in hidden], (game, "hidden enemy money changed decisions")
        assert base != hidden, (game, "negative control: hidden money perturbation did not change the world")
        if game == "ra" and f"{game}-other-seed" in results:
            assert base != results[f"{game}-other-seed"]["snapshots"]
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    if only:
        print("PASS selected jobs", ",".join(sorted(results)))
        return
    print("PASS all profiles, per-seat assignment, rule parity, hidden-information independence, save/resume, repeatability and local contexts")


if __name__ == "__main__":
    main()
