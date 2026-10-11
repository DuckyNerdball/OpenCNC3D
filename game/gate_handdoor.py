"""G323's claims, read off the dumps and pictures of gate_handdoor.txt's three runs.

    python3 gate_handdoor.py SHOTDIR

SHOTDIR holds g323a.log and g323b.log (Enhanced, run twice), g323c.log (the 1995 look) and
the pictures g323_a_*.png, g323_b_*.png. Prints one line, "1 <what passed>" or "0 <what
failed>", then the evidence.

1. THE GATE HAS AN OPENING. Every sample's EBLDDOOR line has gate=1: the renderer found the
   door clip's leaves on the Hand of Nod's remastered model.
2. IT OPENS FOR EACH SOLDIER AND SHUTS AGAIN, intact and damaged: open is 0 before he leaves
   training, reaches 1 by 116 ticks, and is 0 again at 150; the second soldier's samples are
   the DAMAGED stage's.
3. THE DAMAGED BUILDING'S LEAVES MOVE. Its cut has the leaves as pieces of their own,
   gate_upper and gate_lower, posed by the clip (EBLDMOVE kind=clip): at rest before the
   soldier, the upper tipped back at least 80 degrees and the lower sunk at least 200 pack
   units (0.2 cells) while the gate is open, at rest again at 150.
4. THE SOLDIER COMES OUT THROUGH THE OPENING, intact and damaged. On the first sample he is
   out of limbo he is drawn within 0.2 cells of the opening while the engine has him further
   from it than he is drawn; by 125 ticks he is drawn nearer the engine's position than at
   116, and by 135 exactly there. The engine's own position is never touched: it is the same
   in the 1995 look's run, where nothing moves him.
5. THE PICTURES AGREE. Round the gate of the damaged building at least 300 pixels change
   between the frame before the soldier and the open gate, and at most 100 once it has shut.
6. IT IS DETERMINISTIC. The two Enhanced runs print the same samples, and the same open-gate
   picture.
7. WITHOUT THE REMASTERED DOOR NOTHING MOVES HIM. In the 1995 look every sample draws him
   exactly where the engine has him.
"""
import hashlib, math, os, re, sys

D = sys.argv[1]
fails, notes = [], []


def need(ok, msg):
    if not ok:
        fails.append(msg)


def samples(path):
    """{(stage, tick): {...}} from a log's echoed samples."""
    out, cur = {}, None
    try:
        lines = open(path, errors="replace").read().split("\n")
    except OSError:
        return None
    for l in lines:
        m = re.match(r"ECHO\|g323 (intact|damaged) t=(\d+)$", l)
        if m:
            cur = out.setdefault((m.group(1), int(m.group(2))), {"move": {}, "raw": []})
            continue
        if cur is None:
            continue
        m = re.match(r"DRAWPOS\|E1\|\d+\|ex=([\d.]+)\|ez=([\d.]+)\|wx=([\d.]+)\|wz=([\d.]+)", l)
        if m:
            cur["pos"] = tuple(float(v) for v in m.groups())
            cur["raw"].append(l)
        m = re.match(r"EBLDDOOR\|HAND\|key=\d+\|id=\d+\|stage=(\w+)\|open=([\d.]+)\|gate=(\d)\|x=([\d.]+)\|z=([\d.]+)", l)
        if m:
            cur["door"] = (m.group(1), float(m.group(2)), int(m.group(3)), float(m.group(4)), float(m.group(5)))
            cur["raw"].append(l)
        m = re.match(r"EBLDMOVE\|HAND\|key=\d+\|piece=(gate_\w+)\|kind=(\w+)\|deg=([\d.]+)\|moved=([\d.]+)", l)
        if m:
            cur["move"][m.group(1)] = (m.group(2), float(m.group(3)), float(m.group(4)))
            cur["raw"].append(l)
        if l.startswith("ECHO|") or l.startswith("SCRIPT|end"):
            cur = None
    return out


A, B, C = (samples(os.path.join(D, "g323%s.log" % r)) for r in "abc")
if not A:
    print("0 the Enhanced run printed no samples (see its log)")
    sys.exit(0)
TICKS = sorted({t for _, t in A})

# 1, 2
for st in ("intact", "damaged"):
    S = [A.get((st, t), {}) for t in TICKS]
    need(all(s.get("door") and s["door"][2] == 1 for s in S),
         "%s: a sample without EBLDDOOR gate=1 (the renderer found no gate on the model)" % st)
    if not all(s.get("door") for s in S):
        continue
    op = {t: A[(st, t)]["door"][1] for t in TICKS}
    need(op[104] == 0.0 and op[116] == 1.0 and op[150] == 0.0,
         "%s: the gate's open was %.3f, %.3f, %.3f at 104, 116, 150 ticks, want 0, 1, 0"
         % (st, op[104], op[116], op[150]))
    notes.append("%s gate open by tick: %s" % (st, " ".join("%d:%.2f" % (t, op[t]) for t in TICKS)))
need(all(A[("damaged", t)].get("door", ("",))[0] == "DAMAGED" for t in TICKS),
     "the second soldier's samples are not the DAMAGED stage's")

# 3
for pc, key, want in (("gate_upper", 1, 80.0), ("gate_lower", 2, 200.0)):
    mv = {t: A[("damaged", t)]["move"].get(pc) for t in TICKS}
    if not all(mv.values()):
        need(False, "damaged: no EBLDMOVE line for the cut piece %s at every sample (the leaves are "
                    "not pieces the clip poses)" % pc)
        continue
    need(all(m[0] == "clip" for m in mv.values()), "damaged: %s is not moved by the clip" % pc)
    need(mv[104][key] == 0.0 and mv[116][key] >= want and mv[150][key] == 0.0,
         "damaged: %s's %s was %.2f, %.2f, %.2f at 104, 116, 150 ticks, want 0, >=%g, 0"
         % (pc, "deg" if key == 1 else "moved", mv[104][key], mv[116][key], mv[150][key], want))
    notes.append("damaged %s %s by tick: %s" % (pc, "deg" if key == 1 else "moved",
                 " ".join("%d:%.1f" % (t, mv[t][key]) for t in TICKS)))

# 4
for st in ("intact", "damaged"):
    out = [t for t in TICKS if A[(st, t)].get("pos") and A[(st, t)]["pos"][0] < 200.0]
    if not out or not A[(st, out[0])].get("door"):
        need(False, "%s: the soldier never left training in the samples" % st)
        continue
    t0 = out[0]
    ex, ez, wx, wz = A[(st, t0)]["pos"]
    gx, gz = A[(st, t0)]["door"][3:5]
    dw, de = math.hypot(wx - gx, wz - gz), math.hypot(ex - gx, ez - gz)
    need(dw <= 0.2 and de > dw,
         "%s: on his first tick out (%d) he is drawn %.3f cells from the opening (want <= 0.2) and "
         "the engine has him %.3f from it" % (st, t0, dw, de))
    gap = {t: math.hypot(A[(st, t)]["pos"][0] - A[(st, t)]["pos"][2], A[(st, t)]["pos"][1] - A[(st, t)]["pos"][3])
           for t in (116, 125, 135)}
    need(gap[125] < gap[116] and gap[135] < 1e-3,
         "%s: drawn-to-engine gap %.3f, %.3f, %.3f at 116, 125, 135 ticks, want shrinking to 0"
         % (st, gap[116], gap[125], gap[135]))
    notes.append("%s: out at tick %d, drawn %.3f from the opening (%.3f,%.3f), the engine %.3f; gap 116:%.3f "
                 "125:%.3f 135:%.3f" % (st, t0, dw, gx, gz, de, gap[116], gap[125], gap[135]))
    if C:
        ce = [(t, C.get((st, t), {}).get("pos")) for t in TICKS]
        need(all(p and p[0] == A[(st, t)]["pos"][0] and p[1] == A[(st, t)]["pos"][1] for t, p in ce),
             "%s: the engine's position differs between the Enhanced and the 1995 look's runs" % st)

# 5
try:
    import numpy as np
    from PIL import Image
    box = (610, 340, 700, 400)

    def im(n):
        return np.asarray(Image.open(os.path.join(D, "g323_a_%s.png" % n)).convert("RGB").crop(box), np.int16)
    d0 = im("d106")
    ch_open = int((np.abs(im("d116") - d0).max(2) > 24).sum())
    ch_shut = int((np.abs(im("d150") - d0).max(2) > 24).sum())
    need(ch_open >= 300 and ch_shut <= 100,
         "the damaged gate's pictures: %d pixels change round the gate when open (want >= 300), %d once "
         "shut (want <= 100)" % (ch_open, ch_shut))
    notes.append("pixels round the damaged gate: %d changed open, %d shut" % (ch_open, ch_shut))
except Exception as e:                                   # (a missing picture is a failure)
    need(False, "the pictures could not be read: %s" % e)

# 6
if B:
    need(all(A[k]["raw"] == B.get(k, {}).get("raw") for k in A), "the two Enhanced runs printed different samples")


def sha(n):
    try:
        return hashlib.sha256(open(os.path.join(D, n), "rb").read()).hexdigest()
    except OSError:
        return None


need(sha("g323_a_d116.png") and sha("g323_a_d116.png") == sha("g323_b_d116.png"),
     "the two Enhanced runs drew different open-gate pictures")
need(B is not None, "the second Enhanced run printed nothing")

# 7
if C is None or not C:
    need(False, "the 1995 look's run printed no samples")
else:
    need(all(p["pos"][0] == p["pos"][2] and p["pos"][1] == p["pos"][3] for p in C.values() if p.get("pos")),
         "the 1995 look moved where a soldier is drawn")

if fails:
    print("0 " + "; ".join(fails))
else:
    print("1 the Hand of Nod's gate opens and shuts for each soldier trained, intact and damaged (its damaged "
          "leaves, cut pieces of their own, ride the door clip), the soldier is drawn coming out through the "
          "opening and eased onto the engine's position, the same in two runs, and the 1995 look leaves him "
          "where the engine puts him")
for n in notes:
    print(n)
