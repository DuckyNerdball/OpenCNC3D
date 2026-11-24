#!/usr/bin/env python3
"""The 3D views' Stealth Tank fade: the scene a gate stages and the claims it checks.

    python3 gate_cloak3d.py maps DIR                write the test mission (SCG99EA, the
                                                    same one gate_cloak.py makes) and the
                                                    run's script, DIR/cloak3d.txt
    python3 gate_cloak3d.py check MODE LOG SHOTDIR  MODE n64 or enh: read one run's log and
                                                    shots (c3_*.png) and print "1 message"
                                                    or "0 why" (G466, the fade)
    python3 gate_cloak3d.py ghost MODE LOG SHOTDIR  the same, for the player's own fully
                                                    cloaked tank's look (G472)
    python3 gate_cloak3d.py shimmer MODE LOG SHOTDIR the same, for the 1995 shimmer, drawn
                                                    under Enhanced and not under N64 (G473)

WHAT IS UNDER TEST. Under N64 and Enhanced a Stealth Tank fades out as it cloaks and back
in as it uncloaks (cloak_alpha, cloak_draw_ghosts, CloakScreen in cnc_eyes.cpp), the
player's own fully cloaked tank staying a dark see-through ghost at 0.40 of itself, and
under Enhanced the 1995 shimmer is drawn over a cloaking tank (fx_cloak_shimmer_apply in
fx_post.h, cloak_shimmer_mask in cnc_eyes.cpp). The ways the fade showed something other
than the share of the tank its cloak leaves:

  N64       the cartridge's ground plate under the tank (the PKD shadow quad) stayed at
            full strength while the tank faded, and under the player's ghost; and the
            moment cloaking began the tank went darker and lost its top plate, its far
            side drawn over its near side;
  ENHANCED  at the faintest steps the tank read as a dark silhouette rather than as the
            grass behind it, because its see-through pixels were lit by the light pass
            as a vehicle's; and it lost half its presence the moment cloaking began;
  BOTH      the player's own tank fell to nothing as it cloaked, jumped to the ghost when
            cloaked, and dropped to nothing again on the first frame of uncloaking; and
            the ghost itself was the tank at 0.80, a little bluer, its shadow under it:
            players read it as a tank that had not cloaked at all.

THE SCENE. SCG99EA: one Nod Stealth Tank and one of the player's (GDI) on open grass,
both cloaking on their own from frame 2 and cloaked by frame 40. At frame CLOAKED a
soldier of the other side is placed beside each tank, the player's tank is ordered to
attack its one, and both fire, so both uncloak.

THE SCRIPT. Every frame from 1 to FRAMES is shot twice, as it is and with every vehicle
left out of the picture (`vhshow 0`, which hides them from every pass, plates included),
so what was drawn for each tank is the difference of two pictures of one frame.
`cloakprobe` after each first shot prints each tank's cloak state and the share drawn.

THE CLAIMS, per tank and frame, over the rectangle the solid tank changes on frame 1 (with
a margin for the plate). Presence is the sum of |with - without| over it, darkening the
sum of how much darker the picture is with the tank, each against the solid tank's:
  (1) ENGINE TRUTH: both tanks run solid, cloaking, cloaked, uncloaking and back to solid;
  (2) THE FADE TAKES EVERYTHING (the Nod tank): at a share of 0.15 or less its darkening
      is at most 0.35, at 0.08 or less at most 0.20; fully cloaked, the picture is the
      picture without it. Under Enhanced the tank is a screen door on its share of the
      pixels (sixteen steps), and its faint sun shadow and the blend's averaging leave
      about twice the share; before, the faintest steps darkened 0.6 to 1.5. Under
      Enhanced the shimmer moves the grass round under the faint tank, which darkens some
      pixels as much as it lightens others, so there the darkening is the net one (the
      sum of how much darker, less how much lighter), which a dark silhouette still
      fills and a shimmer does not;
  (3) NO POP AS IT CLOAKS: the Nod tank's presence moves by at most 0.15 between two
      frames from solid to fully cloaked (the cloak's start was 0.3 to 0.5);
  (4) THE PLAYER'S TANK NEVER POPS: its probed share moves by no more than one stage's
      step ((1 - 0.40)/38) between two frames and never under the ghost's 0.40, and its
      presence by at most 0.15 between any two frames of the cycle.

G472, THE PLAYER'S OWN GHOST (ghost), over the pixels the player's solid tank covers on
frame 1, on every frame it is fully cloaked:
  (5) DARK: the picture there is at most 0.85 of the brightness of the picture without the
      tank, and at least 0.25 (a ghost, not a hole);
  (6) SEE-THROUGH: it differs from the picture without the tank by at most 0.40 of what
      the solid tank does (before: 0.80 share, a tank brighter than the grass at 1.4 to
      1.5 of its brightness and 0.45 to 0.55 of the solid tank's difference). Both
      are over the pixels the solid tank changes, its shadow among them, so a shadow
      left at its old strength fails them too.

THE OUTLINE. Under Enhanced the player's own tank wears a pale, flickering outline along
its edge from the first frame it cloaks to the last frame it uncloaks
(fx_cloak_shimmer_apply in fx_post.h), on purpose, so the player can find it on the
ground; G524 holds that outline. It lies inside the pixels scored above and is bright,
flickering and switched on and off at once by design, so under Enhanced (4), (5) and (6)
leave it out: on each frame the player's tank is not solid, the pixels it lights there
(bluer than red by 12 levels and greener by 6, and changed by more than 8 from the
picture without the tank), grown by one pixel; (4) compares two frames over the pixels
neither frame's outline lights. The yellow solid tank has 4 such pixels. Measured: with
the outline in, the ghost reads 1.2 to 1.7 of the ground's brightness and the picture
jumps by 0.33 when the outline lights; with it out, the ghost's body is 0.55 to 0.61 of
the ground's brightness, differs by 0.17, and the picture moves at most 0.11 a frame,
as it did before the outline.

G473, THE SHIMMER (shimmer), over the same pixels, between each two fully cloaked frames
in a row (the tank stands still):
  (7) the probe says the shimmer's pass reached a tank on every frame either tank was
      cloaking or cloaked under Enhanced, and on none under N64;
  (8) UNDER ENHANCED THE GROUND THROUGH THE GHOST MOVES: the picture there changes from one
      engine frame to the next by at least 1.5 levels more than the picture without the
      tank does (the clouds' shadows move both); UNDER N64 it does not (at most 0.3 more):
      the cartridge's own fade, with no shimmer.
"""
import os
import re
import sys

NOD = (12, 28)
GDI = (20, 34)
CAM = (18, 31)
CLOAKED = 46          # frames shot while the tanks cloak; soldiers arrive after this one
FRAMES = 92           # and the rest while they uncloak and stand solid again
GHOST = 0.40          # the player's own fully cloaked tank's share (CLOAK_OURS_ALPHA)
TAIL = ["", "[TERRAIN]", "", "[SMUDGE]", "", "[Triggers]", "", "[CellTriggers]", "",
        "[Teams]", "", "[Waypoints]", "", "[Base]", "Count=0", ""]


def cell(x, y):
    return y * 64 + x


def maps(out):
    src = os.path.join("missions", "SCM01EA")
    b = open(src + ".BIN", "rb").read()
    ini = open(src + ".INI", encoding="latin-1").read()
    mapsec = re.search(r"\[MAP\].*?(?=\n\[)", ini, re.S).group(0).replace("\r", "").strip()
    os.makedirs(out, exist_ok=True)
    un = ["000=BadGuy,STNK,256,%d,0,Guard,None" % cell(*NOD),
          "001=GoodGuy,STNK,256,%d,0,Guard,None" % cell(*GDI)]
    text = ["[Basic]", "Player=GoodGuy", "Action=x", "Lose=x", "Win=x", "Brief=x", "Intro=x",
            "BuildLevel=98", "Theme=No theme", "Percent=0", "CarryOverCap=-1",
            "CarryOverMoney=0", "", mapsec, "",
            "[GoodGuy]", "Allies=GoodGuy", "MaxBuilding=150", "MaxUnit=150",
            "Credits=0", "Edge=South", "", "[BadGuy]", "Allies=BadGuy", "MaxBuilding=150",
            "MaxUnit=150", "Credits=0", "Edge=North", "",
            "[STRUCTURES]", "", "[UNITS]"] + un + TAIL
    open(os.path.join(out, "SCG99EA.INI"), "w", encoding="latin-1",
         newline="").write("\r\n".join(text))
    open(os.path.join(out, "SCG99EA.BIN"), "wb").write(b)
    s = ["cam %d %d" % CAM]
    for f in range(1, FRAMES + 1):
        s += ["tick 1", "shot shots/c3_f%02d_on.png" % f, "cloakprobe",
              "vhshow 0", "shot shots/c3_f%02d_off.png" % f, "vhshow 1"]
        if f == CLOAKED:
            # the player's tank is ordered onto its soldier, as a player would: on guard
            # it would not open fire by itself
            s += ["spawnobj E1 %d %d" % (NOD[0] + 2, NOD[1] + 1),
                  "spawnobj E1 %d %d enemy" % (GDI[0] + 2, GDI[1] + 1), "tick 1",
                  "lclickobj STNK:GoodGuy 0", "actclickobj E1:BadGuy 0", "deselect"]
    s += ["quit"]
    open(os.path.join(out, "cloak3d.txt"), "w").write("\n".join(s) + "\n")


def field(l, k):
    m = re.search(r"\|%s=([^|]*)" % k, l)
    return m.group(1) if m else None


def probes(lines):
    """one entry per cloakprobe: per tank (ours 0 or 1) its state and share"""
    out, cur = [], None
    for l in lines:
        if l.startswith("CLOAK3D|frame|"):
            cur = {"shimmer": int(field(l, "shimmer") or -1)}
            out.append(cur)
        elif l.startswith("CLOAK3D|STNK|") and cur is not None:
            cur[int(field(l, "ours"))] = {
                "cloak": int(field(l, "cloak")), "cstage": int(field(l, "cstage")),
                "alpha": float(field(l, "alpha")),
                "shimmer": float(field(l, "shimmer") or 0)}
    return out


def check(mode, log, shots):
    from PIL import Image
    import numpy as np
    why = []

    def need(c, m):
        if not c:
            why.append(m)
        return c

    try:
        P = probes(open(log, errors="ignore").read().splitlines())
    except OSError:
        P = []
    if not need(len(P) == FRAMES, "%d probes in the log, want %d" % (len(P), FRAMES)):
        return "0 " + "; ".join(why)

    def img(f, which):
        p = os.path.join(shots, "c3_f%02d_%s.png" % (f, which))
        if not os.path.exists(p):
            return None
        return np.asarray(Image.open(p).convert("RGB")).astype(np.float64)

    def lum(a):
        return a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114

    # THE RECTANGLE EACH TANK IS SCORED OVER, from the pictures rather than from the
    # probe: what changes on frame 1 when the vehicles are left out, split in two at the
    # widest gap across the screen (the Nod tank stands left of the player's), each part
    # grown by half its size for the edges' filtering. Neither tank moves while cloaking.
    on1, off1 = img(1, "on"), img(1, "off")
    if not need(on1 is not None and off1 is not None, "frame 1: a shot is missing"):
        return "0 " + "; ".join(why)
    ys, xs = np.nonzero(np.abs(on1 - off1).max(axis=2) > 24)
    if not need(len(xs) > 200, "frame 1: the solid tanks change only %d pixels" % len(xs)):
        return "0 " + "; ".join(why)
    cols = np.unique(xs)
    gap = int(np.argmax(np.diff(cols)))
    split = (cols[gap] + cols[gap + 1]) / 2.0
    box = {}
    for ours, sel in ((0, xs < split), (1, xs >= split)):
        x0, x1, y0, y1 = xs[sel].min(), xs[sel].max(), ys[sel].min(), ys[sel].max()
        mx, my = (x1 - x0) / 2.0, (y1 - y0) / 2.0
        box[ours] = (int(y0 - my), int(y1 + my) + 1, int(x0 - mx), int(x1 + mx) + 1)

    dark, net, pres = {0: [], 1: []}, {0: [], 1: []}, {0: [], 1: []}
    own_d, own_line = [], []      # the player's tank per frame, for (4) under Enhanced
    for f in range(1, FRAMES + 1):
        on, off = img(f, "on"), img(f, "off")
        if not need(on is not None and off is not None, "frame %d: a shot is missing" % f):
            return "0 " + "; ".join(why)
        for ours in (0, 1):
            y0, y1, x0, x1 = box[ours]
            h, w = on.shape[:2]
            a = on[max(0, y0):min(h, y1), max(0, x0):min(w, x1)]
            b = off[max(0, y0):min(h, y1), max(0, x0):min(w, x1)]
            d = lum(b) - lum(a)
            dark[ours].append(float(np.clip(d, 0, None).sum()))
            # (under Enhanced the net darkening: the shimmer moves the grass both ways)
            net[ours].append(float(max(d.sum(), 0.0)))
            pres[ours].append(float(np.abs(a - b).sum()))
            if ours:
                own_d.append(np.abs(a - b).sum(axis=2))
                st = P[f - 1].get(1, {}).get("cloak", 0)
                own_line.append(outline(a, b) if mode == "enh" and st in (1, 2, 3)
                                else np.zeros(a.shape[:2], bool))

    # (1) ENGINE TRUTH
    seq = {o: [p.get(o, {}).get("cloak", -1) for p in P] for o in (0, 1)}
    for o in (0, 1):
        s = seq[o]
        runs = [s[0]] + [s[i] for i in range(1, len(s)) if s[i] != s[i - 1]]
        need(runs == [0, 1, 2, 3, 0], "the %s tank's cloak states run %s, want "
             "solid, cloaking, cloaked, uncloaking, solid" % ("player's" if o else "Nod", runs))
    solid = {o: dark[o][0] for o in (0, 1)}
    solidp = {o: pres[o][0] for o in (0, 1)}
    need(solid[0] > 0 and solid[1] > 0, "a solid tank leaves no darkening: %s" % solid)

    # (2) THE FADE TAKES EVERYTHING (the other side's tank)
    faint = []
    for i, p in enumerate(P):
        t = p.get(0)
        if not t:
            continue
        if t["alpha"] < 0:
            need(pres[0][i] == 0, "frame %d: the Nod tank is fully cloaked yet the picture "
                 "differs by %.0f round it" % (i + 1, pres[0][i]))
        elif t["alpha"] <= 0.15:
            r = (net if mode == "enh" else dark)[0][i] / solid[0] if solid[0] else 9
            bar = 0.20 if t["alpha"] <= 0.08 else 0.35
            faint.append(r)
            need(r <= bar, "frame %d (%s, share %.2f): the Nod tank darkens the ground by "
                 "%.2f of the solid tank's darkening, want %.2f or less"
                 % (i + 1, "cloaking" if t["cloak"] == 1 else "uncloaking", t["alpha"], r,
                    bar))
    need(len(faint) >= 6, "only %d faint frames of the Nod tank were scored" % len(faint))

    # (3) NO POP AS IT CLOAKS (the Nod tank, solid to fully cloaked: it moves afterwards)
    nj = [abs(pres[0][i] - pres[0][i - 1]) / solidp[0] for i in range(1, FRAMES)
          if seq[0][i] in (1, 2) and seq[0][i - 1] in (0, 1)]
    need(nj and max(nj) <= 0.15, "the picture of the Nod tank changes by %.2f of its solid "
         "presence between two frames as it cloaks, want 0.15 or less"
         % (max(nj) if nj else 9))

    # (4) THE PLAYER'S TANK NEVER POPS
    al = [p.get(1, {}).get("alpha", -9) for p in P]
    steps = [abs(al[i] - al[i - 1]) for i in range(1, len(al))]
    need(max(steps) <= (1.0 - GHOST) / 38 + 0.002, "the player's tank's share jumps by %.3f "
         "between two frames: %s" % (max(steps), ["%.2f" % a for a in al]))
    need(min(al) >= GHOST - 0.001, "the player's tank's share falls to %.2f, under its "
         "ghost's %.2f" % (min(al), GHOST))
    # (each two frames over the pixels neither frame's outline lights: see THE OUTLINE)
    pj = []
    for i in range(1, FRAMES):
        if seq[1][i] == 0 and seq[1][i - 1] == 0:
            continue
        k = ~(own_line[i] | own_line[i - 1])
        pj.append(abs(own_d[i][k].sum() - own_d[i - 1][k].sum()) / max(own_d[0][k].sum(), 1.0))
    need(max(pj) <= 0.15, "the picture of the player's tank changes by %.2f of its solid "
         "presence between two frames, want 0.15 or less" % max(pj))

    if why:
        return "0 %s: %s" % (mode, "; ".join(why[:6]) + (" (and %d more)" % (len(why) - 6)
                                                     if len(why) > 6 else ""))
    return ("1 %s: the Nod tank at its %d faintest frames darkens the ground by at most "
            "%.2f of its solid self, fully cloaked leaves the picture untouched and moves "
            "at most %.2f a frame as it cloaks; the player's tank's share moves at most "
            "%.3f a frame and never under %.2f, its picture at most %.2f"
            % (mode, len(faint), max(faint) if faint else 0, max(nj), max(steps), GHOST,
               max(pj)))

def own_pixels(shots):
    """the pixels the player's solid tank changes on frame 1 (its shadow among them), and
    the loader for one frame's two pictures; None when frame 1 is missing"""
    from PIL import Image
    import numpy as np

    def img(f, which):
        p = os.path.join(shots, "c3_f%02d_%s.png" % (f, which))
        if not os.path.exists(p):
            return None
        return np.asarray(Image.open(p).convert("RGB")).astype(np.float64)
    on1, off1 = img(1, "on"), img(1, "off")
    if on1 is None or off1 is None:
        return None, img
    ch = np.abs(on1 - off1).max(axis=2) > 24
    ys, xs = np.nonzero(ch)
    if len(xs) < 200:
        return None, img
    cols = np.unique(xs)
    gap = int(np.argmax(np.diff(cols)))
    split = (cols[gap] + cols[gap + 1]) / 2.0
    own = ch & (np.arange(ch.shape[1])[None, :] >= split)
    return own, img


def lum(a):
    return a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114


def outline(on, off):
    """the pixels the player's ghost's outline lights in one frame's pictures (or a part
    of them), grown by one pixel; see THE OUTLINE"""
    import numpy as np
    lit = ((on[..., 2] > on[..., 0] + 12) & (on[..., 1] > on[..., 0] + 6)
           & (np.abs(on - off).max(axis=2) > 8))
    grown = lit.copy()
    grown[1:, :] |= lit[:-1, :]
    grown[:-1, :] |= lit[1:, :]
    grown[:, 1:] |= lit[:, :-1]
    grown[:, :-1] |= lit[:, 1:]
    return grown


def ghost(mode, log, shots):
    """G472: the player's own fully cloaked tank is a dark see-through ghost"""
    import numpy as np
    try:
        P = probes(open(log, errors="ignore").read().splitlines())
    except OSError:
        P = []
    if len(P) != FRAMES:
        return "0 %s: %d probes in the log, want %d" % (mode, len(P), FRAMES)
    own, img = own_pixels(shots)
    if own is None:
        return "0 %s: frame 1 is missing or its solid tanks change too little" % mode
    on1, off1 = img(1, "on"), img(1, "off")
    solid = float(np.abs(on1 - off1).max(axis=2)[own].mean())
    why, lr, pr = [], [], []
    for i, p in enumerate(P):
        if p.get(1, {}).get("cloak") != 2:
            continue
        on, off = img(i + 1, "on"), img(i + 1, "off")
        if on is None or off is None:
            why.append("frame %d: a shot is missing" % (i + 1))
            continue
        # (under Enhanced the ghost's body, without its outline: see THE OUTLINE)
        body = own & ~outline(on, off) if mode == "enh" else own
        if body.sum() < 200:
            why.append("frame %d: the outline covers all but %d of the tank's pixels"
                       % (i + 1, body.sum()))
            continue
        r = float(lum(on)[body].mean() / max(lum(off)[body].mean(), 1e-6))
        q = float(np.abs(on - off).max(axis=2)[body].mean()) / solid
        lr.append(r)
        pr.append(q)
        if not 0.25 <= r <= 0.85:
            why.append("frame %d: the ghost is %.2f of the ground's brightness, want 0.25 to "
                       "0.85" % (i + 1, r))
        if q > 0.40:
            why.append("frame %d: the ghost differs from the ground by %.2f of the solid "
                       "tank, want 0.40 or less" % (i + 1, q))
    if len(lr) < 5:
        why.append("only %d fully cloaked frames of the player's tank" % len(lr))
    if why:
        return "0 %s: %s" % (mode, "; ".join(why[:6]))
    return ("1 %s: on its %d fully cloaked frames the player's tank is %.2f to %.2f of the "
            "ground's brightness and differs from the ground by %.2f to %.2f of the solid "
            "tank" % (mode, len(lr), min(lr), max(lr), min(pr), max(pr)))


def shimmer(mode, log, shots):
    """G473: the 1995 shimmer runs through the ghost under Enhanced, and not under N64"""
    import numpy as np
    try:
        P = probes(open(log, errors="ignore").read().splitlines())
    except OSError:
        P = []
    if len(P) != FRAMES:
        return "0 %s: %d probes in the log, want %d" % (mode, len(P), FRAMES)
    own, img = own_pixels(shots)
    if own is None:
        return "0 %s: frame 1 is missing or its solid tanks change too little" % mode
    why = []
    # (a build whose probe has no shimmer count draws no shimmer; claim 7 cannot be asked
    # of it, and the pictures are still measured below, so its failure says what they show)
    counted = not any(p["shimmer"] < 0 for p in P)
    if not counted:
        why.append("the probe carries no shimmer count: a build that draws no shimmer")
    # (7) the pass reached every tank the probe gives a shimmer, and only under Enhanced
    live = 0
    for i, p in enumerate(P if counted else []):
        want = sum(1 for o in (0, 1) if p.get(o, {}).get("shimmer", 0) > 0)
        if mode == "n64" and (want or p["shimmer"] != 0):
            why.append("frame %d: a shimmer under N64 (%d drawn)" % (i + 1, p["shimmer"]))
        if mode == "enh":
            if p.get(1, {}).get("cloak") == 2 and want == 0:
                why.append("frame %d: the player's cloaked tank has no shimmer" % (i + 1))
            if p["shimmer"] != want:
                why.append("frame %d: the shimmer's pass reached %d tanks, the probe gives "
                           "%d" % (i + 1, p["shimmer"], want))
            live += 1 if want else 0
    if counted and mode == "enh" and live < 30:
        why.append("the shimmer ran on only %d frames" % live)
    # (8) the ground through the ghost moves from one engine frame to the next
    moves = []
    prev = None
    for i, p in enumerate(P):
        if p.get(1, {}).get("cloak") != 2:
            prev = None
            continue
        on, off = img(i + 1, "on"), img(i + 1, "off")
        if on is None or off is None:
            why.append("frame %d: a shot is missing" % (i + 1))
            prev = None
            continue
        if prev is not None:
            mon = float(np.abs(on - prev[0]).max(axis=2)[own].mean())
            moff = float(np.abs(off - prev[1]).max(axis=2)[own].mean())
            moves.append(mon - moff)
        prev = (on, off)
    if len(moves) < 4:
        why.append("only %d pairs of fully cloaked frames" % len(moves))
    else:
        m = float(np.mean(moves))
        if mode == "enh" and m < 1.5:
            why.append("the ground through the ghost moves %.2f levels a frame more than "
                       "without the tank, want 1.5 or more" % m)
        if mode == "n64" and m > 0.3:
            why.append("under N64 the ghost changes %.2f levels a frame more than the ground "
                       "without it, want 0.3 or less (no shimmer)" % m)
    if why:
        return "0 %s: %s" % (mode, "; ".join(why[:6]))
    return ("1 %s: the shimmer's pass ran on %d frames and the ground through the player's "
            "ghost moves %.2f levels a frame more than without the tank"
            % (mode, live, float(np.mean(moves))))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "maps":
        maps(sys.argv[2])
    elif len(sys.argv) >= 5 and sys.argv[1] in ("check", "ghost", "shimmer"):
        fn = {"check": check, "ghost": ghost, "shimmer": shimmer}[sys.argv[1]]
        print(fn(sys.argv[2], sys.argv[3], sys.argv[4]))
    else:
        sys.exit(__doc__)
