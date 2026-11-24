#!/usr/bin/env python3
"""Extract the campaign's cinematics from the 1995 MS-DOS CD's MOVIES.MIX.

WHY THIS EXISTS. The campaign asks for 87 distinct .VQA files by name: 77 from the
Intro / Brief / Action / Win / Win2 / Win3 / Win4 / Lose keys in the [BASIC] section of
every campaign mission file in game/missions (see campaign_scenarios), and 10 more that the engine names itself for the
two endings (ENGINE_MOVIES below). A movie that is not in `data/dosdata/movies/`
reports

    CAMPAIGN|movie|GDI7|missing

on the real campaign path. The flow does NOT break on a missing movie -- camp_movie
returns 1 for anything but a user quit -- so the campaign is playable end to end without
them; it is simply silent where the briefings belong.

They are not recoverable from the cartridge. The N64 version has NO FMV AT ALL: there is
not one .VQA in the 33 MB ROM. It replaces the movies with a text briefing engine
(`../src/briefing_engine.c`, ROM 0x1B6820, a 25-verb table at ROM 0x1DE758 driving 39
plain-text .BRF scripts). The PC presentation wants the PC movies, and those live only
on the original 1995 CDs, which are not redistributed with this repository.

WHAT TO RUN, once the CD or its ISO is mounted:

    python3 tools/extract_movies.py /Volumes/<CD>/MOVIES.MIX data/dosdata/movies

Both C&C discs carry a MOVIES.MIX and the two are NOT the same file -- disc 1 has the
GDI set, disc 2 the Nod set. Run it once per disc into the same output directory; the
second run fills in what the first could not find. Pass --dry-run to see what a disc
holds without writing anything.

Westwood MIX archives store a HASH of each filename, never the name, so extraction is
by computed id: this asks the archive for the exact names the campaign needs and
reports, by name, the ones that disc did not have.

Pass --list to print the wanted names and where each is asked for, one per line, and
nothing else. game/gates.sh uses that to check data/dosdata/movies and the download
manifest against the same list this tool extracts.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.realpath(__file__))
REPO = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(REPO, "menu", "tools"))
import mixlib  # noqa: E402


def campaign_scenarios(missiondir):
    """Every campaign mission file in the missions folder: SCG and SCB, 02 to 15, every
    direction and variant whose .INI is there, and each side's mission 1 'EA'.

    That is every campaign INI present, not a walk of which ones the map can select: a
    file the map never offers would still have its movies asked for. Over-asking only
    costs disc space; under-asking leaves a briefing silent, which is the failure this
    list exists to prevent.

    It is NOT the list game/gate_campaign.sh runs. That gate loads 36 missions for their
    terrain and keeps its own fixed list (only the 'A' variant of most Nod missions), so
    it does not boot SCB04EB, SCB07EC or SCB10EB, whose briefings this list does carry.

    Derived from the folder rather than kept as a list, because the list this used to be
    named only the 'A' variant of most Nod missions, and so missed the briefings of
    Nod 4B, 7C and 10B that the Nod map does offer. Mission 1 is the exception: Choose_Side
    starts every campaign on its 'EA' variant, and the other mission 1 files on the disc
    (SCB01EC, SCG01EB, SCG01EC) are never reached.
    """
    out = []
    for f in sorted(os.listdir(missiondir)):
        m = re.match(r"^(SC[GB](\d\d)[EW][ABC])\.INI$", f, re.I)
        if not m:
            continue
        num = int(m.group(2))
        name = m.group(1).upper()
        if 1 <= num <= 15 and (num != 1 or name.endswith("EA")):
            out.append(name)
    return out


# Movies the ENGINE names directly, which no scenario file mentions. The list that
# used to be derived purely from the .INI files missed these, and the Nod campaign walk
# caught it: every one of Nod's 35 cues played and NOD1PRE alone reported missing,
# because Choose_Side plays the side's opening briefing itself rather than reading it
# out of scenario 1 (app/opencnc3d.cpp: camp_movie(win, au, side ? "NOD1PRE" : "GDI1"),
# mirroring 1995's intro.cpp). GDI1 is also SCG01EA's Brief=, so only NOD1PRE was
# actually absent -- but deriving the list from the scenario files alone is the kind of
# completeness bug that hides until someone plays the other faction.
#
# The endings are the same kind of name. SCG15E* and SCB13E* carry Win=x; the engine
# names the ending movies itself when GDI wins mission 15 or Nod wins mission 13
# (brain/vanilla/tiberiandawn/dllinterface.cpp On_Game_Over, after ending.cpp GDI_Ending
# and Nod_Ending), and the shell plays the rest of 1995's ending sequence around them.
# TRAILER, the Red Alert teaser 1995 played only "if available", is on neither disc and
# is not wanted here.
ENGINE_MOVIES = {
    "GDI1": ["app/opencnc3d.cpp:Choose_Side (GDI opening briefing)"],
    "NOD1PRE": ["app/opencnc3d.cpp:Choose_Side (Nod opening briefing)"],
    "GDIFINA": ["dllinterface.cpp On_Game_Over (GDI 15 won, Temple not ion-cannoned)"],
    "GDIFINB": ["dllinterface.cpp On_Game_Over (GDI 15 won, Temple ion-cannoned)"],
    "GDIEND1": ["dllinterface.cpp On_Game_Over (after the GDI 15 score)"],
    "GDIEND2": ["dllinterface.cpp On_Game_Over (after the GDI 15 score, Temple ion-cannoned)"],
    "NODFINAL": ["dllinterface.cpp On_Game_Over (after the Nod 13 score)"],
    "NODEND1": ["app/campaign.c camp_nod_target (target 1)"],
    "NODEND2": ["app/campaign.c camp_nod_target (target 2)"],
    "NODEND3": ["app/campaign.c camp_nod_target (target 3)"],
    "NODEND4": ["app/campaign.c camp_nod_target (target 4)"],
    "CC2TEASE": ["app/opencnc3d.cpp (the last movie of both endings)"],
}

# Names a scenario file asks for that are not a movie on any disc. SCG08EB's Win= reads
# HELLVLY; the disc's movie is HELLVALY.VQA, so in 1995 that win movie never played
# either. Kept as the 1995 game had it, and kept out of the wanted list.
INI_TYPOS = {
    "HELLVLY": "SCG08EB Win=, a misspelling of HELLVALY in the 1995 scenario file",
}


def wanted_movies(missiondir):
    """Every movie name the campaign asks for, and which scenarios ask for it.

    ONLY the [BASIC] section. A scenario's [TRIGGERS] section also carries Win= and
    Lose= words -- "DESTROYED,WIN,0,NONE,NONE,0" -- and a regex that scans the whole
    file happily reports those as missing movies called `DESTROYED,WIN,0,...`.
    """
    want = {}
    for scen in campaign_scenarios(missiondir):
        path = os.path.join(missiondir, scen + ".INI")
        if not os.path.isfile(path):
            print("  no .INI for %s (looked in %s)" % (scen, missiondir))
            continue
        text = open(path, encoding="latin-1").read()
        m = re.search(r"(?ims)^\[BASIC\](.*?)(?=^\[|\Z)", text)
        if not m:
            continue
        for key in ("Intro", "Brief", "Action", "Win", "Win2", "Win3", "Win4", "Lose"):
            k = re.search(r"(?im)^%s=(.+)$" % key, m.group(1))
            if not k:
                continue
            name = k.group(1).strip().upper()
            # `x` is the scenario files' own "no movie here" marker, and camp_movie in
            # app/opencnc3d.cpp treats it as a no-op rather than a missing file.
            if name and name not in ("X", "NONE", "<NONE>") and name not in INI_TYPOS:
                want.setdefault(name, []).append("%s:%s" % (scen, key))
    for name, why in ENGINE_MOVIES.items():
        want.setdefault(name, []).extend(why)
    return want


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    dry = "--dry-run" in sys.argv
    if "--list" in sys.argv:
        for name, why in sorted(wanted_movies(os.path.join(REPO, "game", "missions")).items()):
            print("%s %s" % (name, ",".join(why)))
        return 0
    if len(args) < 1:
        print(__doc__)
        return 2
    mixpath = args[0]
    outdir = args[1] if len(args) > 1 else os.path.join(REPO, "data", "dosdata", "movies")
    missiondir = os.path.join(REPO, "game", "missions")

    want = wanted_movies(missiondir)
    if not want:
        print("no movies wanted -- is %s populated?" % missiondir)
        return 1

    have = set()
    if os.path.isdir(outdir):
        have = {f.upper()[:-4] for f in os.listdir(outdir) if f.upper().endswith(".VQA")}

    print("campaign wants %d distinct movies; %d of them already in %s"
          % (len(want), len(set(want) & have), outdir))

    if not os.path.isfile(mixpath):
        print("MIX not found: %s" % mixpath)
        print("Mount the C&C CD (or its ISO) and pass the path to its MOVIES.MIX.")
        return 1

    mix = mixlib.Mix(mixpath)
    print("%s: %d records" % (os.path.basename(mixpath), mix.count))

    if not dry:
        os.makedirs(outdir, exist_ok=True)

    got, already, absent = [], [], []
    for name in sorted(want):
        if name in have:
            already.append(name)
            continue
        if not mix.has(name + ".VQA"):
            absent.append(name)
            continue
        data = mix.get(name + ".VQA")
        got.append(name)
        if not dry:
            with open(os.path.join(outdir, name + ".VQA"), "wb") as f:
                f.write(data)

    print("%s %d, already had %d, NOT on this disc %d"
          % ("would extract" if dry else "extracted", len(got), len(already), len(absent)))
    if got:
        print("  new: " + " ".join(got))
    if absent:
        print("  still missing (try the other disc): " + " ".join(absent))
        for name in absent:
            print("      %-10s wanted by %s" % (name, ", ".join(want[name])))
    return 0 if not absent else 3


if __name__ == "__main__":
    raise SystemExit(main())
