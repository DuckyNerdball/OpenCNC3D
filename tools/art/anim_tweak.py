#!/usr/bin/env python3
"""Hand tweaks to a dumped Unreal animation clip, applied before it is baked.

A tweak file is JSON, one per unit and clip (tools/anim_lab/tweaks/<unit>_<clip>.json).
Every key is optional; an empty object {} changes nothing:

    {
      "speed": 1.0,            playback rate: 1.25 plays the loop in 80% of the time.
                               The stride is untouched, so on a moving unit the legs
                               simply cycle faster for the same ground covered.
      "toward": [              blend some bones part of the way to ANOTHER clip's pose
        {"clip": "Anim_Rifleman_Aim",    a clip in the same folder; its pose averaged
         "amount": 0.4,                  over the clip, so its idle sway does not ride in
         "bones": ["RightArm", "..."]}   0 changes nothing, 1 is that clip's pose; the
      ],                                 clip's own motion is kept on top in between
      "legs": {                THE STANCE, solved by two-bone IK so the feet stay flat and
        "width": 35,           where they are put: ankles this many cm apart side to side,
        "stagger": 20,         the left this many cm ahead of the right (negative: behind),
        "hips_up": 6,          the hips raised this much (which straightens the knees),
        "toe_out": 8,          each foot turned out this many degrees,
        "back": 25,            both feet moved this many cm further back (lying: legs
                               straighter),
        "center": "hips"       and width measured about the hips instead of about the
      },                       feet' own middle. Each is optional; a foot's own motion
                               in the clip (a step) rides on top.
      "carry": {               turn the arms and what they hold RIGIDLY, as one piece,
        "pitch": -8,           about the point between the shoulders: degrees about the
        "pivot": "RightArm"    body's own left-right axis, + tips the weapon up; or about
      },                       one bone ("pivot"), e.g. the shoulder a launcher rests on. Both
                               hands and the weapon keep their places on each other, so
                               a grip cannot open. Applied after everything else.
      "grip": {...},           put the hands ON THE WEAPON by IK (see _grip): per hand a
                               point, finger and palm directions, and an elbow hint, all
                               in the weapon bone's axes. Applied after the carry.
      "bones": {               per bone, in that bone's OWN local axes (Unreal's),
        "Spine": {
          "rot":   [x, y, z],  degrees added to its rotation on every frame
          "move":  [x, y, z],  centimetres added to its position on every frame
          "swing": 1.2,        scales how far it moves AWAY FROM ITS AVERAGE over the
                               loop: 1.2 is a fifth more arm swing, 0.8 a fifth less,
                               0 holds it still at its average pose
          "twist": 1.3,        the same, but only for its turn about its OWN LENGTH
                               (an Unreal bone's X axis runs along it): on the spine,
                               the torso's twist, which carries both arms and whatever
                               they hold together, so a two-handed grip stays closed.
                               A runner's spine turns AGAINST his hips, so on the spine
                               a factor below 1 is what shows MORE shoulder swing
          "sway": [            a smooth back-and-forth ADDED on top: turns about the
            {"axis": "x",      bone's own x, y or z axis, "deg" either way, a whole
             "deg": 12,        number of "cycles" per loop so the loop stays seamless,
             "cycles": 1,      starting "phase" of the way through a cycle (0..1)
             "phase": 0}
          ],
          "steady": 0.4        cancels that share of the HIPS' up-and-down bob at this
                               bone, by moving it against the bob: on the spine it calms
                               the upper body while the legs and feet stay untouched
        }
      }
    }

The order is fixed: toward, then per bone swing, twist, rot, sway, move, steady; the
timing is applied throughout. It works on the bones'
LOCAL transforms, exactly the keys the clip carries, so a tweak to the spine carries the
arms and the head with it as the animator's own spine motion would.

sample_clip() is what both the anim lab (tools/anim_lab/build.py) and the converter use,
so a tweak that looks right in the lab is the tweak the game plays.
"""
import json
import os

import numpy as np


def _quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([aw * bx + ax * bw + ay * bz - az * by,
                     aw * by - ax * bz + ay * bw + az * bx,
                     aw * bz + ax * by - ay * bx + az * bw,
                     aw * bw - ax * bx - ay * by - az * bz])


def _quat_from_euler_deg(r):
    """X then Y then Z, about the bone's own axes."""
    q = np.array([0.0, 0.0, 0.0, 1.0])
    for axis, deg in zip(range(3), r):
        if not deg:
            continue
        h = np.radians(deg) * 0.5
        e = np.zeros(4)
        e[axis] = np.sin(h)
        e[3] = np.cos(h)
        q = _quat_mul(q, e)
    return q


def _quat_inv(q):
    return np.array([-q[0], -q[1], -q[2], q[3]])


def _mat(q):
    x, y, z, w = q / np.linalg.norm(q)
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def _model(bones, fr, j):
    """Model-space rotation matrix and position of bone j in one local frame."""
    chain = []
    while j >= 0:
        chain.append(j)
        j = bones[j]["parent"]
    R, t = np.eye(3), np.zeros(3)
    for k in reversed(chain):
        q, tr, sc = fr[k]
        t = t + R @ tr
        R = R @ (_mat(q) * np.asarray(sc, float)[None, :])
    return R, t


def load_tweak(path):
    if not path or not os.path.exists(path):
        return {}
    return json.load(open(path))


def _sample(keys, t01, is_quat):
    n = len(keys)
    if n == 1:
        return np.asarray(keys[0], np.float64)
    x = min(max(t01, 0.0), 1.0) * (n - 1)
    i0 = int(np.floor(x))
    i1 = min(i0 + 1, n - 1)
    u = x - i0
    a = np.asarray(keys[i0], np.float64)
    b = np.asarray(keys[i1], np.float64)
    if is_quat:
        if a @ b < 0:
            b = -b
        q = a + (b - a) * u
        return q / (np.linalg.norm(q) + 1e-30)
    return a + (b - a) * u


def sample_clip(path, bones, fps, once, t0=None, t1=None, tweak=None):
    """The clip's LOCAL (quat, translation, scale) per bone per frame at fps, tweaked.
    Returns (dump, nframe, local) with local[f][b] = (q, t, s)."""
    tweak = tweak or {}
    d = json.load(open(path))
    by = {t["bone"]: t for t in d["tracks"]}
    length = float(d["length"])
    t0 = 0.0 if t0 is None else t0
    t1 = length if t1 is None else min(t1, length)
    speed = float(tweak.get("speed", 1.0)) or 1.0
    span = (t1 - t0) / speed                     # seconds the tweaked clip lasts
    nframe = max(1, int(round(span * fps)) + (1 if once else 0))
    frames = []
    for f in range(nframe):
        src_t = t0 + (f / fps) * speed
        t01 = src_t / length if length > 0 else 0.0
        row = []
        for b in bones:
            tr = by.get(b["name"])
            q = _sample(tr["q"], t01, True) if tr and tr["q"] else np.asarray(b["r"], float)
            t = _sample(tr["p"], t01, False) if tr and tr["p"] else np.asarray(b["t"], float)
            s = _sample(tr["sc"], t01, False) if tr and tr["sc"] else np.asarray(b["s"], float)
            row.append([q / np.linalg.norm(q), t, s])
        frames.append(row)
    names = [b["name"] for b in bones]
    for tow in (tweak.get("toward") or []):
        other = os.path.join(os.path.dirname(path), tow["clip"] + ".json")
        if not os.path.exists(other):
            # a clip borrowed from another unit's folder (the rocketeer walks on the
            # rifleman's walk) still blends toward its OWN unit's pose: the name is looked
            # for in every unit's anims folder, and must be found in exactly one
            import glob
            hits = sorted(glob.glob(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
                os.path.abspath(path)))), "*", "anims", tow["clip"] + ".json")))
            if len(hits) != 1:
                raise SystemExit("toward clip %s: found %d times" % (tow["clip"], len(hits)))
            other = hits[0]
        od = json.load(open(other))
        oby = {t["bone"]: t for t in od["tracks"]}
        amt = float(tow.get("amount", 0.0))
        for bname in tow["bones"]:
            if bname not in names:
                raise SystemExit("toward names bone %s, which this rig does not have" % bname)
            j = names.index(bname)
            tr = oby.get(bname)
            if not tr:
                continue
            if tr["q"]:
                qs = np.array(tr["q"], float)
                qs = np.where((qs @ qs[0])[:, None] < 0, -qs, qs)
                qa = qs.mean(0)
                qa /= np.linalg.norm(qa)
            else:
                qa = np.asarray(bones[j]["r"], float)
            ta = np.mean(np.array(tr["p"], float), axis=0) if tr["p"] else np.asarray(bones[j]["t"], float)
            for fr in frames:
                q = fr[j][0]
                qb = qa if q @ qa >= 0 else -qa
                qq = q + (qb - q) * amt
                fr[j][0] = qq / np.linalg.norm(qq)
                fr[j][1] = fr[j][1] + (ta - fr[j][1]) * amt
    for bname, tw in (tweak.get("bones") or {}).items():
        if bname not in names:
            raise SystemExit("tweak names bone %s, which this rig does not have" % bname)
        j = names.index(bname)
        swing = tw.get("swing")
        if swing is not None and swing != 1.0 and len(frames) > 1:
            # the loop's average pose, then every frame's departure from it scaled
            qs = np.array([fr[j][0] for fr in frames])
            ref = qs[0]
            qs = np.where((qs @ ref)[:, None] < 0, -qs, qs)
            qm = qs.mean(0)
            qm /= np.linalg.norm(qm)
            tm = np.mean([fr[j][1] for fr in frames], axis=0)
            qm_inv = np.array([-qm[0], -qm[1], -qm[2], qm[3]])
            for fr in frames:
                q = fr[j][0]
                if q @ qm < 0:
                    q = -q
                dq = _quat_mul(qm_inv, q)              # departure, in the mean's frame
                ang = 2 * np.arccos(np.clip(dq[3], -1, 1))
                axis = dq[:3] / (np.linalg.norm(dq[:3]) + 1e-12)
                h = ang * swing * 0.5
                dq2 = np.append(axis * np.sin(h), np.cos(h))
                fr[j][0] = _quat_mul(qm, dq2)
                fr[j][1] = tm + (fr[j][1] - tm) * swing
        twist = tw.get("twist")
        if twist is not None and twist != 1.0 and len(frames) > 1:
            qs = np.array([fr[j][0] for fr in frames])
            qs = np.where((qs @ qs[0])[:, None] < 0, -qs, qs)
            qm = qs.mean(0)
            qm /= np.linalg.norm(qm)
            for fr in frames:
                q = fr[j][0]
                if q @ qm < 0:
                    q = -q
                dq = _quat_mul(_quat_inv(qm), q)
                # swing-twist about the bone's own X axis
                tq = np.array([dq[0], 0.0, 0.0, dq[3]])
                tn = np.linalg.norm(tq)
                if tn < 1e-9:
                    continue
                tq /= tn
                swing_q = _quat_mul(dq, _quat_inv(tq))
                ang = 2 * np.arctan2(tq[0], tq[3]) * twist
                tq2 = np.array([np.sin(ang / 2), 0.0, 0.0, np.cos(ang / 2)])
                fr[j][0] = _quat_mul(qm, _quat_mul(swing_q, tq2))
        if tw.get("rot"):
            dq = _quat_from_euler_deg(tw["rot"])
            for fr in frames:
                fr[j][0] = _quat_mul(fr[j][0], dq)
        for sw in (tw.get("sway") or []):
            ax = "xyz".index(sw.get("axis", "x"))
            cyc = int(sw.get("cycles", 1)) or 1
            n = len(frames) - (1 if once else 0)
            for f, fr in enumerate(frames):
                ang = float(sw.get("deg", 0.0)) * np.sin(
                    2 * np.pi * (cyc * f / max(1, n) + float(sw.get("phase", 0.0))))
                r = [0.0, 0.0, 0.0]
                r[ax] = ang
                fr[j][0] = _quat_mul(fr[j][0], _quat_from_euler_deg(r))
        if tw.get("move"):
            for fr in frames:
                fr[j][1] = fr[j][1] + np.asarray(tw["move"], float)
        steady = tw.get("steady")
        if steady and "Hips" in names and len(frames) > 1:
            h = names.index("Hips")
            hz = np.array([_model(bones, fr, h)[1][2] for fr in frames])  # Unreal up is +Z
            mz = hz.mean()
            p = bones[j]["parent"]
            for fr, z in zip(frames, hz):
                Rp, _ = _model(bones, fr, p) if p >= 0 else (np.eye(3), None)
                want = np.array([0.0, 0.0, -steady * (z - mz)])      # model space
                fr[j][1] = fr[j][1] + np.linalg.solve(Rp, want)
    legs = tweak.get("legs")
    if legs:
        _legs(bones, names, frames, legs)
    carry = tweak.get("carry")
    if carry and (carry.get("pitch") or carry.get("yaw")):
        _carry(bones, names, frames, carry)
    if tweak.get("grip"):
        _grip(bones, names, frames, tweak["grip"])
    return d, nframe, frames


def _ik2(S, E, W, Wt, pole_hint):
    """Two-bone IK: shoulder S fixed, elbow E and wrist W moved so the wrist lands on Wt,
    the elbow bending toward pole_hint. Returns the new elbow and wrist."""
    L1, L2 = np.linalg.norm(E - S), np.linalg.norm(W - E)
    d = min(np.linalg.norm(Wt - S), (L1 + L2) * 0.9999)
    u = (Wt - S) / np.linalg.norm(Wt - S)
    pole = pole_hint - (pole_hint @ u) * u
    pole /= np.linalg.norm(pole) + 1e-12
    a = (L1 * L1 - L2 * L2 + d * d) / (2 * d)
    h = np.sqrt(max(L1 * L1 - a * a, 0.0))
    return S + a * u + h * pole, S + u * d


def _carry(bones, names, frames, carry):
    """Turn the arms and what they hold. Without "pivot": both arm roots rigidly about
    the point between the shoulders. With a pivot bone (the shoulder a launcher rests
    on): that bone's whole arm and weapon turn rigidly about its own joint, and the OTHER
    arm follows by two-bone IK so its hand keeps its grip while its shoulder stays put.
    Unreal model space: X the body's left-right, Y forward, Z up. pitch + tips the weapon
    up, yaw + turns it toward the body's left; both measured, not assumed."""
    p_ = np.radians(float(carry.get("pitch", 0.0)))
    y_ = np.radians(float(carry.get("yaw", 0.0)))
    Rx = np.array([[1, 0, 0], [0, np.cos(p_), -np.sin(p_)], [0, np.sin(p_), np.cos(p_)]])
    Rz = np.array([[np.cos(y_), -np.sin(y_), 0], [np.sin(y_), np.cos(y_), 0], [0, 0, 1]])
    R = Rx @ Rz
    pivot = carry.get("pivot")
    if not pivot:
        roots = [names.index(n) for n in carry.get("roots", ["LeftArm", "RightArm"])]
        for fr in frames:
            piv = np.mean([_model(bones, fr, r)[1] for r in roots], axis=0)
            for r in roots:
                Rm, tm = _model(bones, fr, r)
                Rp, tp = _model(bones, fr, bones[r]["parent"])
                fr[r][0] = _mat_to_quat(np.linalg.solve(Rp, R @ Rm) / np.asarray(fr[r][2], float)[None, :])
                fr[r][1] = np.linalg.solve(Rp, piv + R @ (tm - piv) - tp)
        return
    pv = names.index(pivot)
    side = "Left" if pivot.startswith("Right") else "Right"
    sA, sF, sH = (names.index(side + n) for n in ("Arm", "ForeArm", "Hand"))
    for fr in frames:
        Rm, piv = _model(bones, fr, pv)
        Rp, _ = _model(bones, fr, bones[pv]["parent"])
        # the other hand's place and turn, before anything moves, carried by the same turn
        RH, WH = _model(bones, fr, sH)
        RE, E = _model(bones, fr, sF)
        RS, S = _model(bones, fr, sA)
        Wt = piv + R @ (WH - piv)
        RHt = R @ RH
        # 1. the pivot arm (and the weapon under it) turns rigidly about its own joint
        fr[pv][0] = _mat_to_quat(np.linalg.solve(Rp, R @ Rm) / np.asarray(fr[pv][2], float)[None, :])
        # 2. the other arm reaches for its grip
        pole_hint = (E - S) - ((E - S) @ ((WH - S) / np.linalg.norm(WH - S))) * ((WH - S) / np.linalg.norm(WH - S))
        Et, Wt2 = _ik2(S, E, WH, Wt, R @ pole_hint)
        R1 = _between(E - S, Et - S)
        RS2 = R1 @ RS
        R2 = _between(R1 @ (WH - E), Wt2 - Et)
        RE2 = R2 @ R1 @ RE
        RSp, _ = _model(bones, fr, bones[sA]["parent"])
        fr[sA][0] = _mat_to_quat(RSp.T @ RS2)
        fr[sF][0] = _mat_to_quat(RS2.T @ RE2)
        fr[sH][0] = _mat_to_quat(RE2.T @ RHt)


def _hand_axes(bones, names, side):
    """The hand's finger and palm directions in its OWN axes, from the bind pose: the
    fingers continue the forearm (this rig has no finger bones), and a bind pose's palms
    face down."""
    bind = [[np.asarray(b["r"], float), np.asarray(b["t"], float), np.asarray(b["s"], float)]
            for b in bones]
    RH, WH = _model(bones, bind, names.index(side + "Hand"))
    _, E = _model(bones, bind, names.index(side + "ForeArm"))
    f = (WH - E) / np.linalg.norm(WH - E)
    p = np.array([0.0, 0.0, -1.0])
    p = p - (p @ f) * f
    p /= np.linalg.norm(p)
    Rn = RH / np.linalg.norm(RH, axis=0)[None, :]
    return Rn.T @ f, Rn.T @ p


def _grip(bones, names, frames, grip):
    """HANDS ON THE WEAPON, wherever the weapon is. Applied last: the weapon keeps the
    place every other tweak gave it, and each hand named reaches a point ON THE WEAPON by
    two-bone IK, turned so its fingers and palm point where it says. Everything is in the
    weapon bone's own axes (for the Tempest Rising rig: x along the bore to the muzzle,
    z up, y to the right of the bore), so one grip serves every clip:

        "grip": {"right": {"at": [x, y, z],        where the wrist goes, cm
                           "fingers": [x, y, z],   the way the fingers point
                           "palm": [x, y, z],      the way the palm faces
                           "elbow": [x, y, z],     the way the elbow bends out
                           "roll": 45,             the hand turned about its length
                           "straight": true},      or: no bend at the wrist at all
                 "left": {...}}

    The weapon hangs from the right hand in this rig, so it is put back where it was
    after the right arm moves."""
    w = names.index(grip.get("bone", "Weapon"))
    axes = {s: _hand_axes(bones, names, s) for s in ("Right", "Left")}
    for fr in frames:
        Rw, tw = _model(bones, fr, w)
        Rwn = Rw / np.linalg.norm(Rw, axis=0)[None, :]
        for side in ("Right", "Left"):
            g = grip.get(side.lower())
            if not g:
                continue
            sA, sF, sH = (names.index(side + n) for n in ("Arm", "ForeArm", "Hand"))
            RS, S = _model(bones, fr, sA)
            RE, E = _model(bones, fr, sF)
            RH, WH = _model(bones, fr, sH)
            Wt = tw + Rwn @ np.asarray(g["at"], float)
            pole = Rwn @ np.asarray(g.get("elbow", [0, 0, -1]), float)
            Et, Wt2 = _ik2(S, E, WH, Wt, pole)
            R1 = _between(E - S, Et - S)
            RS2 = R1 @ RS
            R2 = _between(R1 @ (WH - E), Wt2 - Et)
            RE2 = R2 @ R1 @ RE
            if g.get("straight"):
                # NO BEND AT THE WRIST: the hand keeps the turn it has on the forearm in
                # the bind pose, so it runs straight on from it
                bnd = [[np.asarray(b["r"], float), np.asarray(b["t"], float),
                        np.asarray(b["s"], float)] for b in bones]
                RFb, _ = _model(bones, bnd, sF)
                RHb, _ = _model(bones, bnd, sH)
                RFb = RFb / np.linalg.norm(RFb, axis=0)[None, :]
                RHb = RHb / np.linalg.norm(RHb, axis=0)[None, :]
                RS2n = RS2 / np.linalg.norm(RS2, axis=0)[None, :]
                RE2n = RE2 / np.linalg.norm(RE2, axis=0)[None, :]
                RSp, _ = _model(bones, fr, bones[sA]["parent"])
                fr[sA][0] = _mat_to_quat(np.linalg.solve(RSp, RS2n))
                fr[sF][0] = _mat_to_quat(RS2n.T @ RE2n)
                fr[sH][0] = _mat_to_quat(RFb.T @ RHb)
                continue
            # the hand: its own finger and palm axes onto the ones asked for
            fl, pl = axes[side]
            F = Rwn @ np.asarray(g["fingers"], float)
            F /= np.linalg.norm(F)
            P = Rwn @ np.asarray(g["palm"], float)
            P = P - (P @ F) * F
            P /= np.linalg.norm(P)
            # "roll": degrees turned about the hand's own length, + turning the palm toward
            # fingers x palm (for a right hand on a tube's right side: down over the top)
            rl = np.radians(float(g.get("roll", 0.0)))
            P = np.cos(rl) * P + np.sin(rl) * np.cross(F, P)
            Rhand = np.stack([F, P, np.cross(F, P)], 1) @ np.stack([fl, pl, np.cross(fl, pl)], 1).T
            RSp, _ = _model(bones, fr, bones[sA]["parent"])
            sc = lambda j: np.asarray(fr[j][2], float)[None, :]   # noqa: E731
            fr[sA][0] = _mat_to_quat(np.linalg.solve(RSp, RS2 / np.linalg.norm(RS2, axis=0)[None, :] * sc(sA)) / sc(sA))
            RS2n = RS2 / np.linalg.norm(RS2, axis=0)[None, :]
            RE2n = RE2 / np.linalg.norm(RE2, axis=0)[None, :]
            fr[sF][0] = _mat_to_quat(RS2n.T @ RE2n)
            fr[sH][0] = _mat_to_quat(RE2n.T @ Rhand)
        # the weapon back where it was, under the right hand's new place
        pw = bones[w]["parent"]
        Rp, tp = _model(bones, fr, pw)
        fr[w][0] = _mat_to_quat(np.linalg.solve(Rp, Rw) / np.asarray(fr[w][2], float)[None, :])
        fr[w][1] = np.linalg.solve(Rp, tw - tp)


def _between(a, b):
    """The smallest rotation taking direction a to direction b."""
    a = a / np.linalg.norm(a)
    b = b / np.linalg.norm(b)
    v = np.cross(a, b)
    c = float(a @ b)
    if np.linalg.norm(v) < 1e-9:
        return np.eye(3)
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx * (1.0 / (1.0 + c))


def _legs(bones, names, frames, legs):
    """Unreal model space: X the body's left-right, Y its forward, Z up."""
    need = ["Hips", "LeftUpLeg", "LeftLeg", "LeftFoot", "RightUpLeg", "RightLeg", "RightFoot"]
    if any(n not in names for n in need):
        raise SystemExit("legs needs %s" % ", ".join(need))
    hi = names.index("Hips")
    side = {"L": [names.index(n) for n in ("LeftUpLeg", "LeftLeg", "LeftFoot")],
            "R": [names.index(n) for n in ("RightUpLeg", "RightLeg", "RightFoot")]}
    # the ankles as the clip has them, and their average over the loop
    A0 = {k: [_model(bones, fr, v[2])[1] for fr in frames] for k, v in side.items()}
    m = {k: np.mean(v, axis=0) for k, v in A0.items()}
    c = 0.5 * (m["L"] + m["R"])
    if legs.get("center") == "hips":
        c[0] = float(np.mean([_model(bones, fr, hi)[1][0] for fr in frames]))
    c[1] -= float(legs.get("back", 0.0))           # Unreal +Y is the body's forward
    tgt = {k: m[k].copy() for k in m}
    if "back" in legs:
        tgt["L"][1] -= float(legs["back"]); tgt["R"][1] -= float(legs["back"])
    if "width" in legs:
        sx = 1.0 if m["L"][0] >= m["R"][0] else -1.0
        tgt["L"][0] = c[0] + sx * legs["width"] * 0.5
        tgt["R"][0] = c[0] - sx * legs["width"] * 0.5
    if "stagger" in legs:
        tgt["L"][1] = c[1] + legs["stagger"] * 0.5
        tgt["R"][1] = c[1] - legs["stagger"] * 0.5
    shift = {k: tgt[k] - m[k] for k in m}
    up = float(legs.get("hips_up", 0.0))
    toe = float(legs.get("toe_out", 0.0))
    for f, fr in enumerate(frames):
        if up:
            Rp, _ = _model(bones, fr, bones[hi]["parent"]) if bones[hi]["parent"] >= 0 else (np.eye(3), None)
            fr[hi][1] = fr[hi][1] + np.linalg.solve(Rp, np.array([0.0, 0.0, up]))
        Rh, _ = _model(bones, fr, hi)
        for k, (ju, jl, jf) in side.items():
            Ru, H = _model(bones, fr, ju)
            Rl, K = _model(bones, fr, jl)
            Rf, A = _model(bones, fr, jf)
            At = A0[k][f] + shift[k]
            L1, L2 = np.linalg.norm(K - H), np.linalg.norm(A - K)
            d = np.linalg.norm(At - H)
            d = min(d, (L1 + L2) * 0.9999)
            u = (At - H) / np.linalg.norm(At - H)
            # the knee keeps pointing the way it pointed
            ua = (A - H) / np.linalg.norm(A - H)
            pole = (K - H) - ((K - H) @ ua) * ua
            pole = pole - (pole @ u) * u
            pole /= np.linalg.norm(pole) + 1e-12
            a = (L1 * L1 - L2 * L2 + d * d) / (2 * d)
            h = np.sqrt(max(L1 * L1 - a * a, 0.0))
            Kt = H + a * u + h * pole
            At = H + u * d
            R1 = _between(K - H, Kt - H)
            Ru2 = R1 @ Ru
            R2 = _between(R1 @ (A - K), At - Kt)
            Rl2 = R2 @ R1 @ Rl
            ang = np.radians(toe if k == "L" else -toe)
            cz, sz = np.cos(ang), np.sin(ang)
            Rf2 = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]) @ Rf
            fr[ju][0] = _mat_to_quat(Rh.T @ Ru2)
            fr[jl][0] = _mat_to_quat(Ru2.T @ Rl2)
            fr[jf][0] = _mat_to_quat(Rl2.T @ Rf2)


def _mat_to_quat(m):
    t = np.trace(m)
    if t > 0:
        s = np.sqrt(t + 1.0) * 2
        q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        q = [0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s, (m[2, 1] - m[1, 2]) / s]
    elif m[1, 1] > m[2, 2]:
        s = np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        q = [(m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s, (m[0, 2] - m[2, 0]) / s]
    else:
        s = np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        q = [(m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s, (m[1, 0] - m[0, 1]) / s]
    q = np.array(q)
    return q / np.linalg.norm(q)


def model_space_frames(bones, frames, trs, model_space):
    """local (q, t, s) frames -> model-space 4x4s, via ue_c3m's own trs/model_space."""
    return [model_space(bones, [trs(q, t, s) for q, t, s in fr]) for fr in frames]

