#!/usr/bin/env python3
"""The Construction Yard's first two animations, as keyframes on the rig's controls.

    python3 tools/art/yard_clips.py        # after yard_rig.py (which also runs this)

Reads tools/anim_lab/out/fact_rig.json and writes tools/anim_lab/out/fact_clips.json,
which the lab's rig page plays. Generated, not committed: this script is the source.

  UNFOLD    the build-up, in the order the Remastered Collection's own FACTMAKE sprite
            plays it: the pad first and a hatch opening in it, the walls and the fan
            block, then the crane rising out of the ground folded and swinging out, the
            vaulted roof after it, and the grid garage door last, with the grab hanging
            in front of it.
  DELIVERY  the door slides down, a crate rolls out on its wheels, the crane grabs it,
            lifts it, swings round and sets it on the ground in front of the yard, lets
            go, the door closes and the crane goes back to rest.

A CLIP is a set of TRACKS, one per control ("bone.control", the rig's own names), each a
list of keys [time, value, how]: how is "s" (ease in and out to the next key) or "l"
(straight to the next key). A PROP is something that is not the yard (the crate): its
track is its pose in the world, [time, x, y, z, yaw degrees, wheel turn degrees].

THE CRANE IS SOLVED, NOT GUESSED. Wherever the grab has to be somewhere (over the crate,
on it, over the drop point) the crane's turn, boom and forearm are solved so the point
between the two jaw shoes lands exactly there, and the grab's own tilt is set to cancel
the boom's and forearm's so it always hangs straight (all three hinges share one axis).
A move between two such points is sampled every 1/20 s along an arc about the crane's
turning axis (angle, reach and height each eased), so the load swings the way a slewing
crane swings it rather than cutting a chord through the building.
"""
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
OUT = os.path.join(REPO, "tools", "anim_lab", "out")
UP = np.array([0.0, 1.0, 0.0])
RATE = 20.0                  # samples a second on a solved move


# ---- the rig, the way the lab page poses it ---------------------------------------------
def rot(axis, deg):
    a = np.asarray(axis, float)
    a = a / np.linalg.norm(a)
    t = math.radians(deg)
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + math.sin(t) * K + (1 - math.cos(t)) * K @ K


class Rig:
    def __init__(self, path):
        self.js = json.load(open(path))
        self.bones = self.js["bones"]
        self.W0 = {}
        for n, b in self.bones.items():
            M = np.eye(4)
            M[:3, :3] = np.array(b["rest_frame"])
            M[:3, 3] = b["pivot"]
            self.W0[n] = M
        self.rest = {"%s.%s" % (n, c["id"]): c["rest"] for n, b in self.bones.items()
                     for c in b["controls"]}

    def pose(self, values):
        """World matrix of every bone for a set of control values (missing = rest)."""
        W = {}
        for n, b in self.bones.items():          # parents come first in the file
            C = np.eye(4)
            lift = 0.0
            for c in b["controls"]:
                v = values.get("%s.%s" % (n, c["id"]), c["rest"])
                if c["kind"] == "rise":
                    lift -= (1 - v) * c["depth"]
                elif c["kind"] == "slide":
                    C[:3, 3] += np.array(c["axis"]) * v
                else:
                    R = np.eye(4)
                    R[:3, :3] = rot(c["axis"], v)
                    C = C @ R
            if b["parent"]:
                p = b["parent"]
                M = W[p] @ np.linalg.inv(self.W0[p]) @ self.W0[n] @ C
            else:
                M = self.W0[n] @ C
                M[1, 3] += lift
            W[n] = M
        return W


# ---- the grab ---------------------------------------------------------------------------
def grab_geometry(rig):
    """The jaw shoes' midpoint in the grab's rest frame, and the finger angle at which the
    shoes' inner faces stand a given distance apart. Measured on the delivered geometry."""
    import blendread
    import yard_rig as Y
    b = blendread.Blend(Y.BLEND)
    G = {o["name"]: Y.P(b.mesh(o["data"])[1]) for o in b.objects() if o["type"] == 1}
    shoes = {s: G["LP2_CR_Jaw_replaceable_shoe_" + s] for s in ("left", "right")}
    fingers = {s: np.vstack([G["LP_CY_Grab_finger_" + s], shoes[s]]) for s in shoes}
    mid = 0.5 * (shoes["left"].mean(0) + shoes["right"].mean(0))
    grip_local = np.linalg.inv(rig.W0["grab"]) @ np.append(mid, 1.0)
    kl, kr = rig.W0["finger_left"][:3, 3], rig.W0["finger_right"][:3, 3]
    d = kr - kl
    d[1] = 0
    d /= np.linalg.norm(d)
    shoe_y = (shoes["left"][:, 1].min(), shoes["left"][:, 1].max())

    def gap(deg):
        out = {}
        for s in ("left", "right"):
            W = rig.W0["finger_" + s]
            R = W[:3, :3] @ rot([1, 0, 0], deg) @ W[:3, :3].T
            p = (fingers[s] - W[:3, 3]) @ R.T + W[:3, 3]
            band = p[(p[:, 1] >= shoe_y[0]) & (p[:, 1] <= shoe_y[1])]
            out[s] = band @ d
        return out["right"].min() - out["left"].max()

    return grip_local[:3], gap, mid[1] - rig.W0["grab"][1, 3]


def angle_for_gap(gap, want):
    lo, hi = -10.0, 40.0
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        if gap(mid) < want:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


class Crane:
    def __init__(self, rig, grip_local):
        self.rig, self.grip_local = rig, grip_local
        self.yaw_c = rig.W0["crane_yaw"][:3, 3]

    def arm(self, yaw, boom, fore):
        return {"crane_yaw.yaw": yaw, "boom.pitch": boom, "forearm.pitch": fore,
                "grab.pitch": -(boom + fore)}

    def grip(self, q):
        W = self.rig.pose(self.arm(*q))
        return (W["grab"] @ np.append(self.grip_local, 1.0))[:3]

    def solve(self, target, seed):
        """(yaw, boom, forearm) putting the grip on target; Gauss-Newton on the rig's own
        forward kinematics, from a seed so a sampled path stays on one branch."""
        q = np.array(seed, float)
        for _ in range(60):
            e = self.grip(q) - target
            if np.linalg.norm(e) < 0.05:
                break
            J = np.zeros((3, 3))
            for i in range(3):
                dq = np.zeros(3)
                dq[i] = 1e-3
                J[:, i] = (self.grip(q + dq) - self.grip(q - dq)) / 2e-3
            q -= np.linalg.lstsq(J, e, rcond=None)[0]
        err = float(np.linalg.norm(self.grip(q) - target))
        if err > 0.5:
            raise SystemExit("crane cannot reach %s (%.1f units short)" % (np.round(target, 1), err))
        for i, (lo, hi, name) in enumerate(((-180, 180, "turn"), (-30, 30, "boom"), (-140, 60, "forearm"))):
            if not lo - 1e-6 <= q[i] <= hi + 1e-6:
                raise SystemExit("reaching %s needs %s %.1f, outside the rig's %g..%g" % (
                    np.round(target, 1), name, q[i], lo, hi))
        return q

    def cyl(self, p):
        v = p - self.yaw_c
        return math.atan2(v[2], v[0]), math.hypot(v[0], v[2]), p[1]

    def uncyl(self, a, r, y):
        return self.yaw_c + np.array([r * math.cos(a), 0.0, r * math.sin(a)]) + np.array([0, y - self.yaw_c[1], 0])


def smooth(u):
    return u * u * (3 - 2 * u)


class Clip:
    def __init__(self, name, label):
        self.name, self.label = name, label
        self.tracks = {}
        self.props = {}
        self.end = 0.0

    def key(self, ctl, t, v, how="s"):
        tr = self.tracks.setdefault(ctl, [])
        if tr and t < tr[-1][0] - 1e-9:
            raise SystemExit("%s: key at %.2f before %.2f" % (ctl, t, tr[-1][0]))
        if tr and abs(tr[-1][0] - t) < 1e-9:
            tr[-1] = [round(t, 3), round(float(v), 3), how]
        else:
            tr.append([round(t, 3), round(float(v), 3), how])
        self.end = max(self.end, t)

    def move(self, ctl, t0, t1, v0, v1):
        """Hold v0 until t0, ease to v1 by t1."""
        self.key(ctl, t0, v0)
        self.key(ctl, t1, v1)

    def json(self):
        return {"label": self.label, "duration": round(self.end + 0.4, 2),
                "tracks": self.tracks, "props": self.props}


def highest(rig, values, root):
    """The highest point of root's whole subtree in a pose, on the delivered geometry."""
    import blendread
    import yard_rig as Y
    b = blendread.Blend(Y.BLEND)
    pieces = {o["name"]: Y.P(b.mesh(o["data"])[1]) for o in b.objects() if o["type"] == 1}
    Y.OWNER = owner = Y.assign({n: None for n in pieces})
    sub = {root}
    for n, bn in rig.bones.items():
        if bn["parent"] in sub:
            sub.add(n)
    W = rig.pose(values)
    top = -1e9
    for n, p in pieces.items():
        bone = owner[n]
        if bone in sub:
            M = W[bone] @ np.linalg.inv(rig.W0[bone])
            top = max(top, float((p @ M[:3, :3].T + M[:3, 3])[:, 1].max()))
    return top


# ---- UNFOLD -----------------------------------------------------------------------------
def unfold(rig):
    c = Clip("unfold", "Unfold")
    rise = lambda b, t0, t1: c.move(b + ".rise", t0, t1, 0, 1)
    # everything starts below the ground; the roof halves start folded open, the crane
    # folded compact (forearm tucked up along the boom), as the Remastered one rises
    FOLD = {"crane_yaw.yaw": -40, "boom.pitch": 30, "forearm.pitch": -135, "grab.pitch": 105}
    for k, v in FOLD.items():
        c.key(k, 0, v)
    c.key("roof_west.open", 0, 140)
    c.key("roof_east.open", 0, 140)

    rise("slab", 0.0, 0.7)
    rise("apron", 0.4, 1.1)
    c.move("apron_flap_inner.open", 1.0, 1.8, 0, 105)
    c.move("apron_flap_outer.open", 1.1, 1.9, 0, 105)
    rise("deck", 1.5, 2.4)
    rise("wall_west", 1.7, 2.7)
    rise("wall_east", 1.8, 2.8)
    rise("drive_west", 2.1, 2.9)
    rise("drive_east", 2.2, 3.0)
    rise("power_unit", 2.4, 3.1)
    rise("fan_housing", 2.5, 3.5)
    rise("rear_wall", 2.8, 3.8)
    # the fans come up to speed once their block is up, and keep turning
    spin_from, end = 3.4, 10.0
    for f, phase in (("fan_0", 0), ("fan_1", 40)):
        c.key(f + ".spin", spin_from, phase, "l")
        c.key(f + ".spin", spin_from + 1.0, phase + 180, "l")      # half speed while spooling
        c.key(f + ".spin", end, phase + 180 + 540 * (end - spin_from - 1.0), "l")

    # the crane rises folded, then unfolds: out, up and over, past its rest, and settles.
    # Folded, the boom stands higher than at rest, so the crane starts deeper than the
    # rig's own depth (a rise below 0) by exactly what the fold adds.
    depth = next(k for k in rig.bones["crane"]["controls"] if k["id"] == "rise")["depth"]
    over = highest(rig, dict(FOLD, **{"crane.rise": 0.0}), "crane")
    start = -max(0.0, over + 5.0) / depth
    c.move("crane.rise", 3.0, 4.6, start, 1)
    c.key("crane_yaw.yaw", 4.6, -40)
    c.key("crane_yaw.yaw", 6.0, 18)
    c.key("crane_yaw.yaw", 6.8, -4)
    c.key("crane_yaw.yaw", 7.4, 0)
    c.key("boom.pitch", 4.6, 30)
    c.key("boom.pitch", 5.6, 22)
    c.key("boom.pitch", 6.6, -6)
    c.key("boom.pitch", 7.4, 0)
    c.key("forearm.pitch", 4.4, -135)
    c.key("forearm.pitch", 5.8, 40)
    c.key("forearm.pitch", 6.8, -8)
    c.key("forearm.pitch", 7.4, 0)
    c.key("grab.pitch", 4.4, 105)
    c.key("grab.pitch", 5.8, -50)
    c.key("grab.pitch", 6.8, 10)
    c.key("grab.pitch", 7.6, 0)
    for f in ("finger_left", "finger_right"):
        c.key(f + ".open", 6.6, 0)
        c.key(f + ".open", 7.0, 30)
        c.key(f + ".open", 7.6, 30)
        c.key(f + ".open", 8.0, 0)

    # the roof comes up open, then closes over the workshop
    rise("roof", 5.0, 6.8)
    c.move("roof_west.open", 6.7, 7.9, 140, 0)
    c.move("roof_east.open", 6.8, 8.0, 140, 0)
    c.move("apron_flap_inner.open", 7.8, 8.7, 105, 0)
    c.move("apron_flap_outer.open", 7.9, 8.8, 105, 0)
    # and the garage door last
    rise("door", 8.4, 9.6)
    c.end = end - 0.4
    return c


# ---- DELIVERY ---------------------------------------------------------------------------
CRATE = {"width": 140.0, "depth": 140.0, "height": 110.0, "wheel": 15.0, "clear": 26.0}
DECK_Y, APRON_Y, GROUND_Y = 134.1, 74.8, 0.0     # tops of the workshop floor, the apron, the ground


def delivery(rig):
    c = Clip("delivery", "Crate delivery")
    grip_local, gap, _ = grab_geometry(rig)
    crane = Crane(rig, grip_local)
    along = np.array([0.5, 0.0, -0.8660254])          # the building's axis, front to back
    across = np.cross(along, UP)
    cell = rig.js["cell"]
    at = lambda s, t, y: cell * (s * along + t * across) + np.array([0, y, 0])

    # the crate sits at the workshop's front, on the door's centre line; it stops where the
    # crane, turned to its rest bearing, can reach it
    rest_grip = crane.grip((0, 0, 0))
    stop_s = float(rest_grip @ along) / cell
    start = at(0.30, 0.40, DECK_Y)
    stop = at(stop_s, 0.40, APRON_Y)
    drop = np.array([-470.0, GROUND_Y, 930.0])        # on the ground, in front of the yard
    body_h = CRATE["clear"] + CRATE["height"]
    grip_h = CRATE["clear"] + 0.62 * CRATE["height"]  # the jaws take it above its middle
    hold = angle_for_gap(gap, CRATE["width"] - 2.0)   # a touch of squeeze
    wide = hold + 20.0
    c.props["crate"] = dict(CRATE, grip_angle=round(hold, 2))

    def crate_key(t, p, yaw=0.0, wheel=0.0, how="s"):
        c.props.setdefault("crate_track", []).append(
            [round(t, 3), *[round(float(x), 2) for x in p], round(yaw, 2), round(wheel, 1), how])

    q = np.zeros(3)

    def arm_keys(t, q, how="l"):
        for k, v in crane.arm(*q).items():
            c.key(k, t, v, how)

    def travel(t0, t1, p0, p1, q):
        """The grip from p0 to p1 between t0 and t1, along an arc about the turning axis."""
        a0, r0, y0 = crane.cyl(p0)
        a1, r1, y1 = crane.cyl(p1)
        if a1 - a0 > math.pi:
            a1 -= 2 * math.pi
        if a0 - a1 > math.pi:
            a1 += 2 * math.pi
        n = max(2, int(round((t1 - t0) * RATE)))
        for i in range(n + 1):
            u = smooth(i / n)
            p = crane.uncyl(a0 + (a1 - a0) * u, r0 + (r1 - r0) * u, y0 + (y1 - y0) * u)
            q = crane.solve(p, q)
            arm_keys(t0 + (t1 - t0) * i / n, q)
        return q

    fingers = lambda t, v, how="s": [c.key(f + ".open", t, v, how) for f in ("finger_left", "finger_right")]

    # 0.4 - 2.0  the door slides down; the crane lifts its grab clear and opens it
    c.move("door.open", 0.4, 2.0, 0, rig.bones["door"]["controls"][1]["max"])
    arm_keys(0.0, q, "s")
    hover = stop + np.array([0, grip_h + 230.0, 0])
    q = travel(0.6, 2.2, rest_grip, hover, q)
    fingers(0.6, 0)
    fingers(1.8, wide)

    # 2.0 - 4.3  the crate rolls out, over the sill and down onto the apron
    crate_key(0.0, start)
    crate_key(2.0, start)
    sill, drop_end = at(-0.30, 0.40, DECK_Y), at(-0.40, 0.40, APRON_Y)
    turn = lambda a, b: math.degrees(np.linalg.norm((b - a) * [1, 0, 1]) / CRATE["wheel"])
    w = turn(start, sill)
    crate_key(3.2, sill, 0, w, "l")
    w += turn(sill, drop_end)
    crate_key(3.5, drop_end, 0, w)
    w += turn(drop_end, stop)
    crate_key(4.3, stop, 0, w)

    # 4.3 - 5.1  down onto it, 5.1 - 5.5 the jaws close
    on_crate = stop + np.array([0, grip_h, 0])
    arm_keys(4.3, q)
    q = travel(4.3, 5.1, hover, on_crate, q)
    fingers(5.1, wide)
    fingers(5.5, hold)

    # 5.6 - 10.0  lift, swing round, set it down on the ground in front of the yard. The
    # crate is carried: its pose is the grab's, sampled at the same rate as the arm.
    lifted = on_crate + np.array([0, 420.0, 0])
    over_drop = drop + np.array([0, grip_h + 420.0, 0])
    on_ground = drop + np.array([0, grip_h, 0])
    arm_keys(5.6, q)
    for (t0, t1, p0, p1) in ((5.6, 6.8, on_crate, lifted), (6.8, 9.0, lifted, over_drop),
                             (9.0, 10.0, over_drop, on_ground)):
        q = travel(t0, t1, p0, p1, q)
    # the crate follows the grab through exactly the keys the arm got
    arm = {k: v for k, v in c.tracks.items() if k in ("crane_yaw.yaw", "boom.pitch", "forearm.pitch")}
    keys = sorted({k[0] for k in arm["crane_yaw.yaw"] if 5.6 <= k[0] <= 10.0 + 1e-9})
    W_hold = rig.pose(crane.arm(*[_at(arm["crane_yaw.yaw"], 5.6), _at(arm["boom.pitch"], 5.6),
                                  _at(arm["forearm.pitch"], 5.6)]))["grab"]
    offset = np.linalg.inv(W_hold) @ np.append(stop, 1.0)
    yaw0 = _at(arm["crane_yaw.yaw"], 5.6)
    for t in keys:
        qq = [_at(arm["crane_yaw.yaw"], t), _at(arm["boom.pitch"], t), _at(arm["forearm.pitch"], t)]
        W = rig.pose(crane.arm(*qq))["grab"]
        crate_key(t, (W @ offset)[:3], qq[0] - yaw0, w, "l")
    landed = c.props["crate_track"][-1]

    # 10.0 - 10.5 let go; lift clear; the door closes; the crane goes home
    fingers(10.0, hold)
    fingers(10.5, wide)
    arm_keys(10.4, q)
    q = travel(10.4, 11.4, on_ground, on_ground + np.array([0, 260.0, 0]), q)
    c.move("door.open", 10.8, 12.4, rig.bones["door"]["controls"][1]["max"], 0)
    arm_keys(11.6, q, "s")
    arm_keys(13.4, np.zeros(3), "s")
    fingers(11.6, wide)
    fingers(13.2, 0)
    crate_key(13.8, landed[1:4], landed[4], landed[5])
    c.end = 13.8
    return c


def _at(track, t):
    """A track's value at t, as the page evaluates it."""
    if t <= track[0][0]:
        return track[0][1]
    for (t0, v0, how), (t1, v1, _) in zip(track, track[1:]):
        if t0 <= t <= t1:
            u = 0.0 if t1 == t0 else (t - t0) / (t1 - t0)
            return v0 + (v1 - v0) * (smooth(u) if how == "s" else u)
    return track[-1][1]


def main(argv):
    rig = Rig(os.path.join(OUT, "fact_rig.json"))
    clips = [unfold(rig), delivery(rig)]
    out = {"rig": "fact_rig.json", "clips": {c.name: c.json() for c in clips}}
    json.dump(out, open(os.path.join(OUT, "fact_clips.json"), "w"), indent=None, separators=(",", ":"))
    for c in clips:
        print("CLIP|%s|%.1f s|%d tracks|%d keys" % (c.name, c.end, len(c.tracks),
                                                    sum(len(v) for v in c.tracks.values())))
    cr = out["clips"]["delivery"]["props"]
    print("CLIP|delivery|crate %gx%gx%g|grip angle %.1f deg|lands at %s" % (
        CRATE["width"], CRATE["depth"], CRATE["height"], cr["crate"]["grip_angle"],
        cr["crate_track"][-1][1:4]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
