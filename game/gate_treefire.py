#!/usr/bin/env python3
"""G312: a remastered tree burns and burns out under Enhanced.

    python3 gate_treefire.py maps DIR        write the test mission and its scripts
    python3 gate_treefire.py check DIR       read the runs' logs and shots in DIR and print
                                             "1 message" or "0 why"

THE SCENE: SCG10EA's temperate map and pack, on G307's block of clear cells, with nothing on
it but one spruce T01 and a Nod flame tank two cells south of the cell the tree blocks (T01
blocks the cell under its origin, tdata.cpp _List0010). The player is Nod. The tank is
ordered to fire on the tree's cell (a force-fire on the ground, as a player burns a tree)
and goes on firing until the tree is gone, then is stopped.

THE ENGINE'S STORY (terrain.cpp): the flame catches the tree (Catch_Fire attaches BURN-L
and BURN-M); the flames take its strength; with no strength left and the fire out it
crumbles. In the current headless brain it remains in the heap. THE REMASTER'S: the burning
model while the fire is on it, the engine's two fire anims on it stood down; the charred
snag from the crumble, smoking out by 30 seconds. If the engine deletes the tree, its
orphan snag must survive, settle and go. A run retaining the engine tree does not cover
that orphan path; the verdict says so explicitly.

THE RUNS: the Enhanced run (`TF`), the same run again (`TFr`: two runs must be one picture),
and the Enhanced run with --notreefire (`TFn`: the old drawing, the A/B for the pixels).
Every second: `eterrdump` (ETREE, ETREESTAGE, ETREEDUMP lines) and `efxdump`.

THE CLAIMS:
  1  the tree caught fire: an ETREE|stage line 0 -> 1, with the engine's fire seen on it;
  2  while it burned, the burning model was drawn (T01~BURN_TEM drawn > 0 in a dump that
     says the tree is burning), and the engine's BURN-L and BURN-M on it stood down
     (ETREEDUMP stood >= 2) and are gone from the effects dump while it burns;
  3  it burnt out: ETREE|stage 1 -> 2 with no strength left, and the snag model was drawn
     while the engine still had the tree (T01~SNAG_TEM drawn > 0 with orphan=0);
  4  the snag emits smoke/embers and stops emitting after 30 seconds. If the engine keeps
     the dead tree, its snag remains drawn. If the engine deletes it (ETREE|orphan), the
     snag must draw independently, sink and go between 30 and 34 seconds;
  5  pixels: the burning frame differs from the --notreefire run's at the tree, the snag
     frame shows something standing at the tree's place where --notreefire shows nothing
     (the cartridge draws no tree once it is deleted), and the gone frame matches the
     --notreefire run's at the tree's place within the smoke's noise;
  6  the two Enhanced runs are one picture at every shot.
"""
import json
import os
import re
import sys

TAIL = ["", "[SMUDGE]", "", "[Triggers]", "", "[CellTriggers]", "", "[Teams]", "",
        "[Waypoints]", "", "[Base]", "Count=0", ""]
SRC, AT = "SCG10EA", (26, 45)
NAME = "SCG88EA"
SECONDS = 100           # how long the scene runs after the order
SHOT_EVERY = 3          # seconds between shots


def cell(x, y):
    return y * 64 + x


def layout():
    x0, y0 = AT
    tree = (x0 + 7, y0 + 2)               # the origin cell; it blocks the one below
    target = (tree[0], tree[1] + 1)
    tank = (tree[0], tree[1] + 3)
    cam = (tree[0] + 0.5, tree[1] + 1.7)
    return tree, target, tank, cam


def script(out, tag, stop_at):
    tree, target, tank, cam = layout()
    s = ["tick 3", "cam %.1f %.1f" % cam, "zoom max", "tick 1",
         "shot %s/%s_a.png" % (out, tag), "eterrdump",
         "lclickobj FTNK 0", "order %d %d c" % target]
    for k in range(1, SECONDS + 1):
        s.append("tick 15")
        if k == stop_at:
            s += ["lclickobj FTNK 0", "stop"]
        s += ["eterrdump", "efxdump"]
        if k % SHOT_EVERY == 0:
            s.append("shot %s/%s_s%03d.png" % (out, tag, k))
            s.append("eterrdump")
    s.append("quit")
    return s


def maps(out):
    os.makedirs(out, exist_ok=True)
    src = os.path.join("missions", SRC)
    b = open(src + ".BIN", "rb").read()
    ini = open(src + ".INI", encoding="latin-1").read()
    mapsec = re.search(r"\[MAP\].*?(?=\n\[)", ini, re.S).group(0).replace("\r", "").strip()
    tree, target, tank, cam = layout()
    text = ["[Basic]", "Player=BadGuy", "Action=x", "Lose=x", "Win=x", "Brief=x",
            "Intro=x", "BuildLevel=98", "Theme=No theme", "Percent=0",
            "CarryOverCap=-1", "CarryOverMoney=0", "", mapsec, "",
            "[GoodGuy]", "Allies=GoodGuy", "MaxBuilding=150", "MaxUnit=150",
            "Credits=0", "Edge=South", "", "[BadGuy]", "Allies=BadGuy,Neutral",
            "MaxBuilding=150", "MaxUnit=150", "Credits=0", "Edge=North", "",
            "[STRUCTURES]", "", "[UNITS]",
            "000=BadGuy,FTNK,256,%d,0,Guard,None" % cell(*tank),
            "", "[INFANTRY]", "", "[OVERLAY]", "",
            "[TERRAIN]", "%d=T01,None" % cell(*tree)] + TAIL
    open(os.path.join(out, NAME + ".INI"), "w", encoding="latin-1",
         newline="").write("\r\n".join(text))
    open(os.path.join(out, NAME + ".BIN"), "wb").write(b)
    stop_at = int(os.environ.get("TF_STOP", "25"))
    for tag in ("TF", "TFr", "TFn"):
        open(os.path.join(out, tag + ".txt"), "w").write(
            "\n".join(script(out, tag, stop_at)) + "\n")
    json.dump(dict(tree=tree, target=target, tank=tank, cam=cam, pack=SRC, stop=stop_at),
              open(os.path.join(out, "plan.json"), "w"), indent=1)


def kv(line):
    return dict(p.split("=", 1) for p in line.split("|") if "=" in p)


def dumps(log, tag):
    out, cur = [], None
    for l in open(log, encoding="latin-1"):
        l = l.rstrip("\n")
        if l.startswith(tag):
            cur = [l]
            out.append(cur)
        elif cur is not None:
            cur.append(l)
    return out


def box_diff(a, b, box):
    from PIL import Image, ImageChops
    try:
        A, B = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    except OSError:
        return -1
    d = ImageChops.difference(A, B).convert("L").point(lambda v: 255 if v > 24 else 0)
    return d.crop(box).histogram()[255]


def same_png(a, b):
    from PIL import Image, ImageChops
    try:
        return ImageChops.difference(Image.open(a).convert("RGB"),
                                     Image.open(b).convert("RGB")).getbbox() is None
    except OSError:
        return False


def check(out):
    plan = json.load(open(os.path.join(out, "plan.json")))
    log = os.path.join(out, "TF.log")
    if not os.path.exists(log):
        return "0 no log"
    L = [l.rstrip("\n") for l in open(log, encoding="latin-1")]
    why, facts = [], []
    st = [l for l in L if l.startswith("ETREE|stage|T01|")]
    lit = [l for l in st if "from=0|to=1" in l]
    out_ = [l for l in st if "from=1|to=2" in l]
    if not lit:
        why.append("the tree never caught fire (%d stage lines)" % len(st))
    if not out_:
        why.append("the tree never burnt out")
    elif kv(out_[0]).get("str") != "0":
        why.append("it burnt out with strength left: %s" % out_[0])
    orphan = [i for i, l in enumerate(L) if l.startswith("ETREE|orphan|T01|")]
    gone = [l for l in L if l.startswith("ETREE|gone|T01|")]
    # the dumps, each the lines from one ETREE... up to its ETREEDUMP
    burn_drawn = snag_live = snag_orphan = 0
    stood = 0
    burn_anims_while = 0
    sink_seen = 0.0
    emitted_snag = 0
    i0 = orphan[0] if orphan else len(L)
    blk, burning, orph = [], False, False
    for i, l in enumerate(L):
        if l.startswith("ETREE|tree|"):
            d = kv(l)
            burning = d.get("stage") == "burning"
            orph = d.get("orphan") == "1"
            if orph:
                sink_seen = max(sink_seen, float(d.get("sink", "0")))
            if d.get("stage") == "snag":
                emitted_snag = max(emitted_snag, int(d.get("emitted", "0")))
        elif l.startswith("ETREESTAGE|T01~BURN_TEM|") and burning:
            burn_drawn += int(kv(l).get("drawn", "0"))
        elif l.startswith("ETREESTAGE|T01~SNAG_TEM|"):
            n = int(kv(l).get("drawn", "0"))
            if i > i0:
                snag_orphan += n
            elif not burning:
                snag_live += n
        elif l.startswith("ETREEDUMP|"):
            stood = max(stood, int(kv(l).get("stood", "0")))
            burning = orph = False
    # the engine's fire anims while the tree burns: none of BURN-L / BURN-M left in g_efxAnims
    # EFXSEEN is cumulative pre-filter history, not the live draw list.
    for l in L:
        if re.match(r"^EFXDUMP\|ANIM\|(BURN-L|BURN-M)\|", l):
            burn_anims_while += 1
    if lit and burn_drawn <= 0:
        why.append("the burning model was never drawn while the tree burned")
    if lit and stood < 2:
        why.append("the engine's fire anims on the tree were not stood down (stood=%d)" % stood)
    if lit and burn_anims_while:
        why.append("%d effect-dump lines still carry BURN-L/BURN-M" % burn_anims_while)
    if out_ and snag_live <= 0:
        why.append("the snag was not drawn while the engine crumbled the tree")
    # THIS ENGINE NEVER DELETES A CRUMBLING TREE (its theater shapes are not loaded when the
    # terrain types are initialised, so the crumble has no last frame to reach): the tree
    # stays in the dump with no strength, and its snag stands as long as it does. Where the
    # engine does delete it, the snag must go on on its own entry, smoke out, sink and go.
    last_snag = [l for l in L if l.startswith("ETREE|tree|T01|") and "stage=snag" in l]
    if emitted_snag <= 0 and out_:
        why.append("the snag gave off no smoke or embers")
    if out_ and not orphan:
        # still the engine's: drawn at the end, and its smoke out after 30 seconds
        tail = [l for l in L if l.startswith("ETREESTAGE|T01~SNAG_TEM|")][-3:]
        if not tail or all(int(kv(l).get("drawn", "0")) <= 0 for l in tail):
            why.append("the snag was not drawn at the end while the engine kept the tree")
        em = [int(kv(l).get("emitted", "0")) for l in last_snag
              if int(kv(l).get("since", "0")) >= 31 * 15]
        if len(em) < 2:
            why.append("fewer than two snag emission samples after 31 seconds")
        elif em[-1] != em[0]:
            why.append("the snag still smoked after 31 seconds (%d -> %d)" % (em[0], em[-1]))
        if last_snag and int(kv(last_snag[-1]).get("since", "0")) < 31 * 15:
            why.append("the run ended before the snag's smoke was out")
        facts.append("the engine kept the crumbling tree to the end, its snag with it")
    elif orphan and snag_orphan <= 0:
        why.append("the snag was not drawn on its own entry after the deletion")
    if out_ and orphan and not gone:
        why.append("the snag never went (no ETREE|gone)")
    elif out_ and orphan and gone:
        t = int(kv(gone[0]).get("frame", "0")) - int(kv(out_[0]).get("frame", "0"))
        if not (30 * 15 <= t <= 34 * 15):
            why.append("the snag went %d ticks after it began, wanted 450..510" % t)
        if sink_seen <= 0.0:
            why.append("the snag never sank")
    # pixels: the shots of the burning, the snag and after, against --notreefire
    if lit and out_:
        f_lit = int(kv(lit[0])["frame"])
        f_out = int(kv(out_[0])["frame"])
        f_gone = int(kv(gone[0])["frame"]) if gone else 10 ** 9
        f0 = None
        for l in L:
            if l.startswith("ORDERCELL|cell="):
                break
        # the frame of shot k: 3 + 1 + k*15 ticks from the start (tick 3, tick 1, then 15s)
        def shot_at(k):
            return 4 + 15 * k
        ks = [k for k in range(SHOT_EVERY, SECONDS + 1, SHOT_EVERY)]
        kb = [k for k in ks if f_lit + 30 <= shot_at(k) < f_out]
        kso = [k for k in ks if f_out + 45 <= shot_at(k) < f_out + 15 * 20]
        kg = [k for k in ks if shot_at(k) > f_gone + 15] if gone else None
        box = (520, 160, 760, 460)
        for name, sel, need in (("burning", kb, "more"), ("snag", kso, "more"),
                                ("gone", kg, "less")):
            if sel is None:
                continue
            if not sel:
                why.append("no shot falls in the %s window" % name)
                continue
            k = sel[len(sel) // 2]
            a = os.path.join(out, "TF_s%03d.png" % k)
            b = os.path.join(out, "TFn_s%03d.png" % k)
            n = box_diff(a, b, box)
            facts.append("%s s%03d differs from --notreefire on %d px" % (name, k, n))
            if need == "more" and n < 1500:
                why.append("the %s shot s%03d hardly differs from --notreefire (%d px)"
                           % (name, k, n))
            if need == "less" and n > 6000:
                why.append("after the snag went, s%03d still differs from --notreefire "
                           "on %d px" % (k, n))
        # two runs, one picture
        bad = [k for k in ks if not same_png(os.path.join(out, "TF_s%03d.png" % k),
                                             os.path.join(out, "TFr_s%03d.png" % k))]
        if bad:
            why.append("the two Enhanced runs differ at %d shots (first s%03d)"
                       % (len(bad), bad[0]))
    if why:
        return "0 " + "; ".join(why)
    lifetime = ("orphan snag drawn %d times, sank and went" % snag_orphan if orphan else
                "engine retained the dead tree; snag smoke stopped; orphan path not covered")
    return "1 the tree caught fire at frame %s, drew its burning model (%d draws, %d engine " \
           "fire anims stood down), burnt out at %s into the snag (%d draws, %d particles); " \
           "%s; %s" % (kv(lit[0])["frame"], burn_drawn, stood, kv(out_[0])["frame"],
                        snag_live, emitted_snag, lifetime, "; ".join(facts))


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("maps", "check"):
        sys.exit(__doc__)
    if sys.argv[1] == "maps":
        maps(sys.argv[2])
    else:
        print(check(sys.argv[2]))
