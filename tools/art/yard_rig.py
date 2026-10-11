#!/usr/bin/env python3
"""Rig the Construction Yard: a bone for every part that moves, pivots taken from the
model's own pins, and the game's triangles bound to them.

    python3 tools/art/yard_rig.py                 # writes tools/anim_lab/out/fact_rig.*
    python3 tools/art/yard_rig.py --report        # the rig as text, nothing written

WHAT IT READS. The runtime source blend (art/construction_yard/runtime_v01), through
tools/art/blendread.py, for the 211 named pieces; and game/models3d/FACT.c3m for the
triangles as the game draws them, with their normals, UVs and textures. Every c3m
triangle is matched to the one piece whose polygon holds all three of its corners, so
the rig binds exactly the geometry the game has, and a delivery that stops agreeing with
its blend fails here by name rather than drawing half a crane.

WHAT IT WRITES (generated, not committed; the Anim Lab serves them), and then runs
tools/art/yard_clips.py, whose clips are solved against this rig:
    fact_rig.glb    the yard as a glTF skinned mesh: one joint per bone, every vertex at
                    full weight on one joint (the pieces are rigid), rest pose = the model
                    as delivered. Pack units and axes (x east, y up, z south).
    fact_rig.json   the bones and their CONTROLS: what each slider does, about or along
                    which bone-local axis, its range and its rest value; and the aim
                    constraints that keep the hydraulic rams between their pins (a
                    ram half turns so its local +y points at its partner's pivot).

THE PIECES HAVE NO PIVOTS. Every object in the delivery has its origin at the world
origin with its geometry in place, so nothing in the file says where a hinge is. Each
pivot here is measured from the part that physically is the hinge: a joint's two end
caps give its pin line (their centres are the two ends of the pin), a fan's hub gives
its spin axis, a ram's two pins give its ends. PIECES and BONES below are the whole
decision, readable in one place.

AXES AND SIGNS. Every rotating bone's rest frame puts its hinge on local +x, so a
control is always "turn about x" (or "slide along x"). The sign of x is chosen by a
stated test, not by whichever way the pin was modelled: the boom's + raises the elbow,
the forearm's + swings the wrist away from the crane's turning axis, a finger's + opens
it, a hatch's + lifts its free edge, a roof half's + opens it. The test is run and
printed, so a flipped axis shows up in the report.
"""
import argparse
import fnmatch
import io
import json
import math
import os
import struct
import sys
from collections import Counter, OrderedDict, defaultdict
from itertools import product

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import blendread  # noqa: E402

BLEND = os.path.join(REPO, "art", "construction_yard", "runtime_v01",
                     "ConstructionYard_Runtime01.blend")
C3M = os.path.join(REPO, "game", "models3d", "FACT.c3m")
OUT = os.path.join(REPO, "tools", "anim_lab", "out")
CELL = 1024.0

# ---- which piece rides which bone ---------------------------------------------------------
# Every mesh object in the blend must match exactly one line (fnmatch patterns). The
# ROOF halves are the one exception to "a piece is one bone": the shell and both arched
# ribs are single meshes spanning the crown, and are cut along it (SPLIT below).
PIECES = OrderedDict([
    # -- the ground the yard stands on
    ("slab", ["LP_CY_Foundation_original_footprint"]),
    ("apron", ["LP__SH_Apron_gasket_*", "LP_SH_Apron_hinge_*"]),
    ("apron_flap_inner", ["LP__SH_Apron_cover_inner", "LP__SH_Apron_hinge_leaf_*_1"]),
    ("apron_flap_outer", ["LP__SH_Apron_cover_outer", "LP__SH_Apron_hinge_leaf_*_0"]),
    # -- the workshop
    ("deck", ["LP_INT_Continuous_deck_base", "LP_INT_Deck_*", "LP_INT_Front_deck_sill",
              "LP_INT_Front_landing", "LP_INT_Rear_plain_deck", "LP_CY_Door_threshold"]),
    ("wall_west", ["LP_CY_Buttress_west", "LP_CY_Sill_west", "LP_INT_Inner_shell_liner_L",
                   "LP_INT_Inner_shell_rib_L_*"]),
    ("wall_east", ["LP_CY_Buttress_east", "LP_CY_Sill_east", "LP_INT_Inner_shell_liner_R",
                   "LP_INT_Inner_shell_rib_R_*"]),
    ("drive_west", ["LP_INT_Shell_rail_L", "LP_INT_Side_cylinder_L_*",
                    "LP_INT_Side_chrome_rod_L_*", "LP_INT_Cylinder_saddle_L_*",
                    "LP_INT_Guide_coupling_L_*", "LP_INT_Protected_hose_L"]),
    ("drive_east", ["LP_INT_Shell_rail_R", "LP_INT_Side_cylinder_R_*",
                    "LP_INT_Side_chrome_rod_R_*", "LP_INT_Cylinder_saddle_R_*",
                    "LP_INT_Guide_coupling_R_*", "LP_INT_Protected_hose_R"]),
    ("power_unit", ["LP_INT_Power_housing", "LP_INT_Power_perimeter_hose"]),
    ("rear_wall", ["LP_CY_Workshop_rear_wall", "LP3_Rear_vertical_stiffener_*",
                   "LP_EXT_Inner_rear_rib_*", "LP_CY_Rear_service_door",
                   "LP_CY_Rear_service_panel", "LP_EXT_Interior_hatch_*"]),
    ("door", ["LP_CY_Workshop_inset_front_door"]),
    ("roof", []),                                    # carries its halves; see SPLIT
    ("roof_west", ["LP3_Roof_standing_seam_*_1", "LP3_Roof_crown_cap_*",
                   "LP3_Shell_rain_lip_west_*", "LP3_Shell_vent_frame_west_*"]),
    ("roof_east", ["LP3_Roof_standing_seam_*_0",
                   "LP3_Shell_rain_lip_east_*", "LP3_Shell_vent_frame_east_*"]),
    # -- the cooling block and its two fans
    ("fan_housing", ["LP_CY_Rear_fan_housing", "LP_CY_Fan_raised_roof", "LP_CY_Fan_dark_well_*",
                     "LP_CY_Fan_rim_*", "LP_CY_Fan_guard_*", "LP3_Fan_wire_guard_*",
                     "LP3_Cooling_*", "LP_FN_Coolant_return_*", "LP2_FN_Flanged_coupling_*"]),
    ("fan_0", ["LP_CY_Fan_hub_0", "LP_FN_Swept_blade_0_*"]),
    ("fan_1", ["LP_CY_Fan_hub_1", "LP_FN_Swept_blade_1_*"]),
    # -- the crane
    ("crane", ["LP_CY_Crane_plinth"]),
    ("crane_yaw", ["LP_CY_Crane_turntable", "LP_CY_Crane_turntable_rim", "LP_CY_Crane_lower_*",
                   "LP2_CR_Lower_cheek_cover_*", "LP2_CR_Protected_service_hose_*",
                   "LP_CY_Crane_operator_*", "LP2_CR_Cab_front_bezel_*", "LP_CY_Cab_mounting_shelf",
                   "LP_CY_Lift_*", "LP_EXT_Lift_collar_*_tip",
                   "LP_CY_Crane_hinge_shoulder", "LP_CY_Crane_pin_shoulder",
                   "LP_EXT_Joint_cap_shoulder_*"]),
    ("boom", ["LP_CY_Crane_upper_*", "LP2_CR_Boom_*", "LP3_Boom_*",
              "LP2_CR_Bearing_carrier_shoulder_*",
              "LP_CY_Crane_hinge_elbow", "LP_CY_Crane_pin_elbow", "LP_EXT_Joint_cap_elbow_*",
              "LP2_CR_Bearing_carrier_elbow_*"]),
    ("forearm", ["LP_CY_Crane_forearm", "LP2_CR_Forearm_cover_*", "LP_CY_Crane_hinge_wrist",
                 "LP_CY_Crane_pin_wrist", "LP_EXT_Joint_cap_wrist_*", "LP_CY_Forearm_ram*",
                 "LP_EXT_Forearm_collar_tip"]),
    ("grab", ["LP_CY_Grab_upper_*", "LP_CY_Grab_knuckle_*"]),
    ("finger_left", ["LP_CY_Grab_finger_left", "LP2_CR_Jaw_replaceable_shoe_left"]),
    ("finger_right", ["LP_CY_Grab_finger_right", "LP2_CR_Jaw_replaceable_shoe_right"]),
])

# The shell and the two arched ribs are cut along the roof's crown: a triangle whose
# centroid lies west of the crown line goes to roof_west, the rest to roof_east.
SPLIT = ["LP_CY_Vault_shell", "LP_CY_Vault_structural_rib_front",
         "LP_CY_Vault_structural_rib_rear"]

# ---- the bones -------------------------------------------------------------------------
# parent, then the controls it carries. RISE is an unfold element: 1 = in place, 0 = its
# whole subtree below the ground (the depth is measured, not typed).
PARENT = {
    "slab": None, "apron": None, "deck": None, "wall_west": None, "wall_east": None,
    "drive_west": None, "drive_east": None, "power_unit": None, "rear_wall": None,
    "door": None, "roof": None, "fan_housing": None, "crane": None,
    "apron_flap_inner": "apron", "apron_flap_outer": "apron",
    "roof_west": "roof", "roof_east": "roof",
    "fan_0": "fan_housing", "fan_1": "fan_housing",
    "crane_yaw": "crane", "boom": "crane_yaw", "forearm": "boom", "grab": "forearm",
    "finger_left": "grab", "finger_right": "grab",
}
ELEMENT_LABEL = OrderedDict([
    ("slab", "Foundation slab"), ("apron", "Front apron and hatches"), ("deck", "Workshop floor"),
    ("wall_west", "West wall"), ("wall_east", "East wall"),
    ("drive_west", "West roof rails and rams"), ("drive_east", "East roof rails and rams"),
    ("power_unit", "Power unit"), ("rear_wall", "Rear wall"), ("door", "Garage door"),
    ("roof", "Roof"), ("fan_housing", "Cooling block"), ("crane", "Crane"),
])


def P(v):
    """Blender (cells, z up) -> pack (units, x east, y up, z south)."""
    v = np.asarray(v, float)
    return np.stack([v[..., 0] * CELL, v[..., 2] * CELL, -v[..., 1] * CELL], -1)


def D(v):
    """A Blender direction in pack axes, unit length."""
    d = P(v) / CELL
    return d / np.linalg.norm(d)


UP = np.array([0.0, 1.0, 0.0])


def frame(x, up=UP):
    """Rotation whose columns are the bone's local x (the joint axis), y, z."""
    x = x / np.linalg.norm(x)
    y = up - x * (up @ x)
    if np.linalg.norm(y) < 1e-6:
        y = np.array([0.0, 0.0, -1.0]) - x * (-x[2])
    y /= np.linalg.norm(y)
    return np.column_stack([x, y, np.cross(x, y)])


def rot(axis, deg):
    a = axis / np.linalg.norm(axis)
    t = math.radians(deg)
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + math.sin(t) * K + (1 - math.cos(t)) * K @ K


# ---- reading --------------------------------------------------------------------------------
def read_c3m(path):
    data = open(path, "rb").read()
    if data[:8] != b"C3DMODL1":
        raise SystemExit("%s: not a c3m" % path)
    version, nv, ni, npart, ns, nm, nt, nbone = struct.unpack_from("<8I", data, 8)
    if version != 1:
        raise SystemExit("%s: version %d; this rigs the static delivery (version 1)" % (path, version))
    off = 72
    vert = np.frombuffer(data, "<f4", nv * 8, off).reshape(nv, 8).astype(np.float64)
    off += nv * 32
    idx = np.frombuffer(data, "<u4", ni, off).astype(np.int64)
    off += ni * 4 + npart * 32
    subs = np.frombuffer(data, "<u4", ns * 4, off).reshape(ns, 4)
    off += ns * 16
    mats = np.frombuffer(data, "<i4", nm * 4, off).reshape(nm, 4)
    off += nm * 16
    tex = []
    for _ in range(nt):
        w, h, size = struct.unpack_from("<3I", data, off)
        off += 12
        tex.append(np.frombuffer(data, "u1", size, off).reshape(h, w, 4))
        off += size
    return vert, idx, subs, mats, tex


def read_pieces():
    b = blendread.Blend(BLEND)
    out = OrderedDict()
    for o in b.objects():
        if o["type"] != 1 or not o["data"]:
            continue
        name, pos, polys, _ = b.mesh(o["data"])
        W = o["world"]
        wpos = pos @ W[:3, :3].T + W[:3, 3]
        out[o["name"]] = {"pos": wpos, "polys": polys}
    return out


def assign(pieces):
    """piece name -> bone, every piece exactly once."""
    owner = {}
    for bone, pats in PIECES.items():
        for pat in pats:
            hit = [n for n in pieces if fnmatch.fnmatchcase(n, pat)]
            if not hit:
                raise SystemExit("rig: pattern %r (%s) matches no piece" % (pat, bone))
            for n in hit:
                if n in owner and owner[n] != bone:
                    raise SystemExit("rig: %s claimed by both %s and %s" % (n, owner[n], bone))
                owner[n] = bone
    for n in SPLIT:
        if n not in pieces:
            raise SystemExit("rig: split piece %s is missing" % n)
        if n in owner:
            raise SystemExit("rig: split piece %s also claimed by %s" % (n, owner[n]))
        owner[n] = "<split>"
    loose = [n for n in pieces if n not in owner]
    if loose:
        raise SystemExit("rig: %d pieces have no bone: %s" % (len(loose), ", ".join(loose)))
    return owner


def match_triangles(pieces, vert, idx):
    """For each c3m triangle, the piece whose polygon holds all three corners."""
    grid = defaultdict(list)
    names = list(pieces)
    polyset = []
    for pi, n in enumerate(names):
        pk = P(pieces[n]["pos"])
        vpolys = defaultdict(set)
        for fi, poly in enumerate(pieces[n]["polys"]):
            for v in poly:
                vpolys[v].add(fi)
        for v, p in enumerate(pk):
            grid[tuple(np.floor(p).astype(int))].append((p, pi, v))
        polyset.append(vpolys)
    near = []
    for p in vert[:, :3]:
        c = np.floor(p).astype(int)
        hits = set()
        for d in product((-1, 0, 1), repeat=3):
            for q, pi, v in grid.get((c[0] + d[0], c[1] + d[1], c[2] + d[2]), ()):
                if np.abs(q - p).max() <= 0.5:
                    hits.add((pi, v))
        near.append(hits)
    tri_piece = np.empty(len(idx) // 3, np.int64)
    for t in range(len(idx) // 3):
        a, b_, c = (near[i] for i in idx[3 * t:3 * t + 3])
        cands = set()
        for pa, va in a:
            for pb, vb in b_:
                if pb != pa:
                    continue
                for pc, vc in c:
                    if pc != pa:
                        continue
                    if polyset[pa][va] & polyset[pa][vb] & polyset[pa][vc]:
                        cands.add(pa)
        if len(cands) != 1:
            raise SystemExit("rig: c3m triangle %d matches %d pieces (%s)" % (
                t, len(cands), ", ".join(names[i] for i in cands) or "none"))
        tri_piece[t] = cands.pop()
    return names, tri_piece


# ---- the rig --------------------------------------------------------------------------------
def build_rig(pieces):
    G = lambda n: pieces[n]["pos"]
    ctr = lambda n: P(0.5 * (G(n).min(0) + G(n).max(0)))
    mean = lambda n: P(G(n).mean(0))
    bones = OrderedDict()

    def bone(name, pivot, R=np.eye(3), controls=(), aim=None, note=""):
        bones[name] = {"parent": PARENT[name], "pivot": np.asarray(pivot, float), "R": R,
                       "controls": list(controls), "aim": aim, "note": note}

    # the building's long axis, front rib to rear rib, and the crown line
    rib_f, rib_r = ctr("LP_CY_Vault_structural_rib_front"), ctr("LP_CY_Vault_structural_rib_rear")
    along = rib_r - rib_f
    along[1] = 0
    along /= np.linalg.norm(along)
    shell = P(G("LP_CY_Vault_shell"))
    across = np.cross(along, UP)            # points east
    s_across = shell @ across
    crown = 0.5 * (s_across.min() + s_across.max())
    eave_y = shell[:, 1].min()
    roof_mid = 0.5 * (shell @ along).min() + 0.5 * (shell @ along).max()

    def on_line(a, b):                      # pivot and axis of a pin through two end pieces
        pa, pb = mean(a), mean(b)
        return 0.5 * (pa + pb), (pb - pa) / np.linalg.norm(pb - pa)

    # elements: pivot at the middle of their footprint on the ground
    def ground_pivot(names):
        pts = np.vstack([P(G(n)) for n in names])
        c = 0.5 * (pts.min(0) + pts.max(0))
        c[1] = 0.0
        return c

    rise = lambda label: {"id": "rise", "kind": "rise", "label": label,
                          "min": 0.0, "max": 1.0, "rest": 1.0}

    def elem(name, extra=(), R=np.eye(3), pivot=None, note=""):
        names = [n for n, b in OWNER.items() if b == name]
        if name == "roof":
            names = SPLIT
        bone(name, ground_pivot(names) if pivot is None else pivot, R,
             [rise(ELEMENT_LABEL[name])] + list(extra), note=note)

    for e in ("slab", "deck", "wall_west", "wall_east", "drive_west", "drive_east",
              "power_unit", "rear_wall", "fan_housing"):
        elem(e)

    # the apron's two hatches hinge on one line through its two hinge knuckles
    hp, hx = on_line("LP_SH_Apron_hinge_0", "LP_SH_Apron_hinge_1")
    elem("apron")
    bone("apron_flap_inner", hp, frame(hx), [
        {"id": "open", "kind": "rotate", "label": "Inner hatch open", "min": 0, "max": 110, "rest": 0,
         "unit": "deg", "test": ("up", "LP__SH_Apron_cover_inner")}])
    bone("apron_flap_outer", hp, frame(hx), [
        {"id": "open", "kind": "rotate", "label": "Outer hatch open", "min": 0, "max": 110, "rest": 0,
         "unit": "deg", "test": ("up", "LP__SH_Apron_cover_outer")}])

    # the garage door: rise and "open" are the same travel, downward into its sill
    door = P(G("LP_CY_Workshop_inset_front_door"))
    bone("door", ground_pivot(["LP_CY_Workshop_inset_front_door"]), np.eye(3), [
        rise(ELEMENT_LABEL["door"]),
        {"id": "open", "kind": "slide", "label": "Garage door slides down", "axis": [0, -1, 0],
         "min": 0, "max": round(float(door[:, 1].max() - P(G("LP_CY_Door_threshold"))[:, 1].max()), 1),
         "rest": 0, "unit": "units"}],
        note="open slides it straight down until its top meets the sill")

    # the roof: one element that rises and slides along the building on its rails, and two
    # halves that hinge open at the eaves
    roof_pivot = along * roof_mid + across * crown
    roof_pivot[1] = eave_y
    length = float((shell @ along).max() - (shell @ along).min())
    elem("roof", [{"id": "slide", "kind": "slide", "label": "Roof slides back", "axis": [1, 0, 0],
                   "min": -length, "max": length, "rest": 0, "unit": "units"}],
         R=frame(along), pivot=roof_pivot,
         note="local x runs front to back along the building; slide + moves it back")
    half = float(s_across.max() - crown)
    for side, sgn in (("west", -1), ("east", 1)):
        p = roof_pivot + across * (sgn * half)
        bone("roof_" + side, p, frame(along), [
            {"id": "open", "kind": "rotate", "label": "Roof %s half opens" % side, "min": 0, "max": 150,
             "rest": 0, "unit": "deg", "test": ("outward_roof", sgn)}],
             note="hinged at its eave")

    # fans spin about a vertical line through their hubs
    for i in (0, 1):
        bone("fan_%d" % i, mean("LP_CY_Fan_hub_%d" % i), frame(UP), [
            {"id": "spin", "kind": "spin", "label": "Fan %d spin" % (i + 1), "min": 0, "max": 360,
             "rest": 0, "unit": "deg"}])

    # ---- the crane ------------------------------------------------------------------
    rim = P(G("LP_CY_Crane_turntable_rim"))
    yaw_c = 0.5 * (rim.min(0) + rim.max(0))
    yaw_c[1] = rim[:, 1].min()
    elem("crane")
    bone("crane_yaw", yaw_c, frame(UP), [
        {"id": "yaw", "kind": "rotate", "label": "Crane turns", "min": -180, "max": 180, "rest": 0,
         "unit": "deg"}], note="local x is up; + turns anticlockwise seen from above")
    sp, sx = on_line("LP_EXT_Joint_cap_shoulder_-1", "LP_EXT_Joint_cap_shoulder_1")
    ep, ex = on_line("LP_EXT_Joint_cap_elbow_-1", "LP_EXT_Joint_cap_elbow_1")
    wp, wx = on_line("LP_EXT_Joint_cap_wrist_-1", "LP_EXT_Joint_cap_wrist_1")
    bone("boom", sp, frame(sx), [
        {"id": "pitch", "kind": "rotate", "label": "Boom raise", "min": -30, "max": 30, "rest": 0,
         "unit": "deg", "test": ("up_point", ep)}])
    bone("forearm", ep, frame(ex), [
        {"id": "pitch", "kind": "rotate", "label": "Forearm swing out", "min": -140, "max": 60, "rest": 0,
         "unit": "deg", "test": ("out_point", wp, yaw_c)}])
    kl, kr = mean("LP_CY_Grab_knuckle_left"), mean("LP_CY_Grab_knuckle_right")
    tips = 0.5 * (ctr("LP_CY_Grab_finger_left") + ctr("LP_CY_Grab_finger_right"))
    bone("grab", wp, frame(wx), [
        {"id": "pitch", "kind": "rotate", "label": "Grab tilt out", "min": -60, "max": 120, "rest": 0,
         "unit": "deg", "test": ("out_point", tips, yaw_c)}])
    for side, k, other in (("left", kl, kr), ("right", kr, kl)):
        fing = P(G("LP_CY_Grab_finger_" + side))
        tip = fing[np.argmin(fing[:, 1])]
        bone("finger_" + side, k, frame(wx), [
            {"id": "open", "kind": "rotate", "label": "Finger %s open" % side, "min": -10, "max": 40,
             "rest": 0, "unit": "deg", "test": ("away_point", tip, other)}])

    # THE RAMS ARE RIGID. Both lift rams and the forearm ram are modelled at full stroke
    # (rod and barrel meet end to end, 0 to 2 units of overlap) with their line through
    # the pin of the joint they sit beside, so any movement of that joint lengthens them
    # and pulls the rod out of its barrel. They ride with the part they are bolted to.
    # Making them work needs the model changed (eyes moved off the joint line, rods drawn
    # at mid-stroke); the aim constraint this rig and the lab both support is then one
    # bone(..., aim=partner) per ram half.

    geo = {"along": along, "across": across, "crown": crown, "eave_y": eave_y,
           "yaw_centre": yaw_c}
    return bones, geo


def world_rest(bones):
    W = {}
    for n, b in bones.items():
        M = np.eye(4)
        M[:3, :3] = b["R"]
        M[:3, 3] = b["pivot"]
        W[n] = M
    return W


def fix_signs(bones, tri_bone, pts_by_bone, geo):
    """Flip a control's axis where the stated test says + goes the wrong way. Returns
    lines for the report."""
    lines = []
    for n, b in bones.items():
        for c in b["controls"]:
            t = c.pop("test", None)
            if c["kind"] not in ("rotate",) or t is None:
                continue
            x = b["R"][:, 0]
            piv = b["pivot"]
            if t[0] == "up":
                probe = pts_by_bone[n][np.argmax(np.linalg.norm(pts_by_bone[n] - piv, axis=1))]
                want = UP
            elif t[0] == "outward_roof":
                side = t[1]
                pts = pts_by_bone[n]
                probe = pts[np.argmax(pts[:, 1])]
                want = geo["across"] * side
            elif t[0] == "up_point":
                probe, want = t[1], UP
            elif t[0] == "out_point":
                probe = t[1]
                want = probe - t[2]
                want[1] = 0
            elif t[0] == "away_point":
                probe = t[1]
                want = probe - t[2]
                want[1] = 0
            v = rot(x, 5) @ (probe - piv) - (probe - piv)
            ok = float(v @ want) > 0
            if not ok:
                b["R"] = b["R"] @ np.diag([-1.0, 1.0, -1.0])     # x -> -x, y kept, still a rotation
            lines.append("SIGN|%s.%s|%s|%s" % (n, c["id"], t[0], "kept" if ok else "flipped"))
    return lines


# ---- writing ----------------------------------------------------------------------------------
class Glb:
    def __init__(self):
        self.bin = bytearray()
        self.views, self.acc = [], []

    def data(self, arr, ctype, typ, target=None, minmax=False):
        arr = np.ascontiguousarray(arr)
        while len(self.bin) % 4:
            self.bin.append(0)
        off = len(self.bin)
        self.bin += arr.tobytes()
        v = {"buffer": 0, "byteOffset": off, "byteLength": arr.nbytes}
        if target:
            v["target"] = target
        self.views.append(v)
        a = {"bufferView": len(self.views) - 1, "componentType": ctype, "count": arr.shape[0],
             "type": typ}
        if minmax:
            flat = arr.reshape(arr.shape[0], -1)
            a["min"], a["max"] = flat.min(0).tolist(), flat.max(0).tolist()
        self.acc.append(a)
        return len(self.acc) - 1

    def image(self, png):
        while len(self.bin) % 4:
            self.bin.append(0)
        off = len(self.bin)
        self.bin += png
        self.views.append({"buffer": 0, "byteOffset": off, "byteLength": len(png)})
        return len(self.views) - 1

    def write(self, path, js):
        js["buffers"] = [{"byteLength": len(self.bin)}]
        js["bufferViews"], js["accessors"] = self.views, self.acc
        j = json.dumps(js).encode()
        j += b" " * ((4 - len(j) % 4) % 4)
        while len(self.bin) % 4:
            self.bin.append(0)
        out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(self.bin))
        out += struct.pack("<II", len(j), 0x4E4F534A) + j
        out += struct.pack("<II", len(self.bin), 0x004E4942) + bytes(self.bin)
        open(path, "wb").write(out)


def quat(R):
    """xyzw of a rotation matrix."""
    m = R
    t = np.trace(m)
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        return [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    i = int(np.argmax(np.diag(m)))
    j, k = (i + 1) % 3, (i + 2) % 3
    s = math.sqrt(1.0 + m[i, i] - m[j, j] - m[k, k]) * 2
    q = [0.0] * 4
    q[i] = 0.25 * s
    q[j] = (m[j, i] + m[i, j]) / s
    q[k] = (m[k, i] + m[i, k]) / s
    q[3] = (m[k, j] - m[j, k]) / s
    return q


def png(arr, mode):
    b = io.BytesIO()
    Image.fromarray(np.ascontiguousarray(arr), mode).save(b, "PNG", compress_level=6)
    return b.getvalue()


def write_glb(path, bones, W, vert, idx, subs, mats, tex, tri_bone):
    names = list(bones)
    bi = {n: i for i, n in enumerate(names)}
    # vertices, split wherever two bones share one (the cut along the roof's crown)
    remap = {}
    vsrc, vbone = [], []
    new_idx = np.empty_like(idx)
    for t in range(len(idx) // 3):
        b = bi[tri_bone[t]]
        for k in range(3):
            v = int(idx[3 * t + k])
            key = (v, b)
            if key not in remap:
                remap[key] = len(vsrc)
                vsrc.append(v)
                vbone.append(b)
            new_idx[3 * t + k] = remap[key]
    vsrc = np.array(vsrc)
    pos = vert[vsrc, 0:3].astype(np.float32)
    nrm = vert[vsrc, 3:6]
    nrm = (nrm / (np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12)).astype(np.float32)
    uv = vert[vsrc, 6:8].astype(np.float32)
    joints = np.zeros((len(vsrc), 4), np.uint8)
    joints[:, 0] = vbone
    weights = np.zeros((len(vsrc), 4), np.float32)
    weights[:, 0] = 1.0

    g = Glb()
    a_pos = g.data(pos, 5126, "VEC3", 34962, True)
    a_nrm = g.data(nrm, 5126, "VEC3", 34962)
    a_uv = g.data(uv, 5126, "VEC2", 34962)
    a_j = g.data(joints, 5121, "VEC4", 34962)
    a_w = g.data(weights, 5126, "VEC4", 34962)
    images, textures = [], []

    def tex_index(i, mode):
        im = tex[i]
        if mode == "RGB":
            im = im[:, :, :3]
        images.append({"bufferView": g.image(png(im, mode)), "mimeType": "image/png"})
        textures.append({"source": len(images) - 1, "sampler": 0})
        return len(textures) - 1

    materials, prims = [], []
    for part, mat, i0, n in subs:
        base, norm, orm, flags = mats[mat]
        m = {"name": "mat%d" % mat,
             "pbrMetallicRoughness": {"baseColorTexture": {"index": tex_index(base, "RGBA")},
                                      "metallicRoughnessTexture": {"index": tex_index(orm, "RGB")}},
             "normalTexture": {"index": tex_index(norm, "RGB")},
             "occlusionTexture": {"index": len(textures) - 2},
             "extras": {"teamMask": not (flags & 2)}}
        materials.append(m)
        a_i = g.data(new_idx[i0:i0 + n].astype(np.uint32), 5125, "SCALAR", 34963)
        prims.append({"attributes": {"POSITION": a_pos, "NORMAL": a_nrm, "TEXCOORD_0": a_uv,
                                     "JOINTS_0": a_j, "WEIGHTS_0": a_w},
                      "indices": a_i, "material": len(materials) - 1})

    nodes = []
    for n in names:
        b = bones[n]
        Wp = W[b["parent"]] if b["parent"] else np.eye(4)
        L = np.linalg.inv(Wp) @ W[n]
        node = {"name": n, "translation": [float(x) for x in L[:3, 3]],
                "rotation": [float(x) for x in quat(L[:3, :3])],
                "extras": {"controls": b["controls"], "aim": b["aim"] or "", "note": b["note"]}}
        kids = [bi[k] for k in names if bones[k]["parent"] == n]
        if kids:
            node["children"] = kids
        nodes.append(node)
    roots = [bi[n] for n in names if bones[n]["parent"] is None]
    nodes.append({"name": "FACT_rig", "children": roots})
    nodes.append({"name": "FACT", "mesh": 0, "skin": 0})
    ibm = np.array([np.linalg.inv(W[n]).T.reshape(-1) for n in names], np.float32)
    js = {"asset": {"version": "2.0", "generator": "tools/art/yard_rig.py"},
          "scene": 0, "scenes": [{"nodes": [len(nodes) - 2, len(nodes) - 1]}],
          "nodes": nodes, "meshes": [{"name": "FACT", "primitives": prims}],
          "skins": [{"joints": list(range(len(names))), "inverseBindMatrices": g.data(ibm, 5126, "MAT4"),
                     "skeleton": len(nodes) - 2}],
          "materials": materials, "textures": textures, "images": images,
          "samplers": [{"magFilter": 9729, "minFilter": 9987}]}
    g.write(path, js)
    return len(vsrc)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--report", action="store_true", help="print the rig, write nothing")
    a = ap.parse_args(argv)

    pieces = read_pieces()
    global OWNER
    OWNER = assign(pieces)
    vert, idx, subs, mats, tex = read_c3m(C3M)
    names, tri_piece = match_triangles(pieces, vert, idx)
    bones, geo = build_rig(pieces)
    for n in PIECES:
        if n not in bones:
            raise SystemExit("rig: %s has pieces but no bone" % n)

    # which bone draws each triangle
    tri_bone = []
    for t, pi in enumerate(tri_piece):
        n = names[pi]
        if OWNER[n] == "<split>":
            c = vert[idx[3 * t:3 * t + 3], :3].mean(0)
            tri_bone.append("roof_west" if c @ geo["across"] < geo["crown"] else "roof_east")
        else:
            tri_bone.append(OWNER[n])
    pts_by_bone = defaultdict(list)
    for t, b in enumerate(tri_bone):
        pts_by_bone[b].append(vert[idx[3 * t:3 * t + 3], :3])
    pts_by_bone = {k: np.vstack(v) for k, v in pts_by_bone.items()}

    sign_lines = fix_signs(bones, tri_bone, pts_by_bone, geo)

    # the depth an element sinks: the top of everything it carries, in the pose as built
    kids = defaultdict(list)
    for n, b in bones.items():
        if b["parent"]:
            kids[b["parent"]].append(n)

    def subtree(n):
        out = [n]
        for k in kids[n]:
            out += subtree(k)
        return out

    for n, b in bones.items():
        for c in b["controls"]:
            if c["kind"] == "rise":
                tops = [pts_by_bone[k][:, 1].max() for k in subtree(n) if k in pts_by_bone]
                c["depth"] = round(float(max(tops)) + 1.0, 1)
            if c["kind"] in ("slide",):
                c["axis"] = [float(x) for x in c["axis"]]
            if c["kind"] in ("rotate", "spin"):
                c["axis"] = [1.0, 0.0, 0.0]

    W = world_rest(bones)
    tri_count = Counter(tri_bone)
    lines = ["RIG|%d bones|%d pieces|%d triangles" % (len(bones), len(pieces), len(tri_piece))]
    for n, b in bones.items():
        ctl = ", ".join("%s %s %g..%g" % (c["id"], c["kind"], c["min"], c["max"]) +
                        (" depth %g" % c["depth"] if "depth" in c else "") for c in b["controls"])
        lines.append("BONE|%-16s|parent %-12s|%5d tris|pivot %7.1f %7.1f %7.1f|x %6.3f %6.3f %6.3f|%s%s" % (
            n, b["parent"] or "-", tri_count.get(n, 0), *b["pivot"], *b["R"][:, 0],
            ctl or "-", ("|aim " + b["aim"]) if b["aim"] else ""))
    lines += sign_lines
    print("\n".join(lines))
    if a.report:
        return 0

    os.makedirs(OUT, exist_ok=True)
    nv = write_glb(os.path.join(OUT, "fact_rig.glb"), bones, W, vert, idx, subs, mats, tex, tri_bone)
    sys.path.insert(0, os.path.join(REPO, "tools", "anim_lab"))
    import build as lab                                  # the game's own team colours
    meta = {"model": "FACT", "cell": CELL, "teams": lab.team_colours(), "vertices": nv, "triangles": len(tri_piece),
            "elements": [{"bone": k, "label": v} for k, v in ELEMENT_LABEL.items()],
            "bones": OrderedDict((n, {"parent": b["parent"], "controls": b["controls"],
                                      "aim": b["aim"], "aim_axis": [0, 1, 0] if b["aim"] else None,
                                      "note": b["note"],
                                      "triangles": tri_count.get(n, 0),
                                      "pivot": [float(x) for x in b["pivot"]],
                                      "rest_frame": [[float(x) for x in r] for r in b["R"]]})
                                 for n, b in bones.items())}
    json.dump(meta, open(os.path.join(OUT, "fact_rig.json"), "w"), indent=1)
    print("RIG|wrote|%s|%d vertices" % (os.path.join(OUT, "fact_rig.glb"), nv))
    import yard_clips                                     # the clips are keyed to this rig
    return yard_clips.main([])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
