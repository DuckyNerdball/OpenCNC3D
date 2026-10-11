#!/usr/bin/env python3
"""Convert a skinned, animated FBX character into a version 2 .c3m for the Enhanced art set.

A plain Python 3 script: numpy and Pillow, no Blender, no FBX SDK, no game engine. It
reads the binary FBX container itself, evaluates the node hierarchy, the skin and the
animation curves, and writes what the renderer needs and nothing else.

This is the exact invocation that produced the first 3D Minigunner (E1.c3m is now the
Tempest Rising rifleman, made by ue_c3m.py; this pack is no longer drawn):

    python3 tools/art/fbx_c3m.py --type E1 \\
        --mesh  art/models/lowpoly_soldiers_demo/models/Soldier_demo.FBX \\
        --texdir art/models/lowpoly_soldiers_demo/models/Materials \\
        --clip idle=art/models/lowpoly_soldiers_demo/animation/demo_combat_idle.FBX \\
        --clip run=art/models/lowpoly_soldiers_demo/animation/demo_combat_run.FBX \\
        --clip shoot=art/models/lowpoly_soldiers_demo/animation/demo_combat_shoot.FBX \\
        --height 0.55 --texmax 512 \\
        --out game/models3d/E1.c3m

--preview FILE.png adds a picture of the bind pose from four sides and four frames of
every clip, drawn with the renderer's own skinning arithmetic, so a conversion can be
checked before the game is ever started.

WHY A CONVERTER AND NOT AN FBX LOADER IN THE GAME. FBX is an interchange format, not a
runtime one: a node's world matrix is a chain of eleven transforms (pivots, offsets, pre
and post rotations, six rotation orders, three scale inheritance rules), animation is a
set of per-channel curves that each exporter keys differently, and two exporters disagree
about what a skin cluster's bind matrix even means (see inverse_bind). Resolving all of
that once, offline, where every intermediate can be printed and looked at, leaves the
game a file it can read in one pass and a pose it can compute with a multiply and a lerp.
It is also what keeps the draw deterministic: the game never evaluates a curve.

WHAT THE FILE CARRIES (the other half of this is game/model3d_mod.h):

  The mesh in its BIND POSE, in pack units: x east, y up, z south, 1024 to the map cell,
      facing north (-z) at facing zero.
  Per vertex, up to FOUR bone influences, weights quantised to bytes summing to 255.
      The file's own influences are kept in order of weight and the fifth and later
      dropped and renormalised; the count dropped is reported, never silent.
  Per bone, its INVERSE BIND matrix: the one taking a bind-pose vertex into that bone's
      own space. Only bones that actually move a vertex are bones here. A Biped's nubs,
      footsteps and the root are not, and are folded into the poses of what they carry.
  Per clip, per frame, per bone, the bone's MODEL SPACE pose as a unit quaternion, a
      translation and a uniform scale. The renderer interpolates two frames and
      multiplies by the inverse bind; it never walks a hierarchy.
  Per clip, the STRIDE: how far the ground moves under the feet in one loop, measured
      here from the lowest foot. A locomotion clip played at a fixed rate on a unit whose
      speed the game decides would slide its feet; the renderer instead advances the
      clip by distance travelled over stride, so the feet plant.

WHAT IT DOES NOT DO:

  One skinned mesh per file. A second mesh would need its own skin, and the one
      character this was written for is one mesh with two materials.
  Euler order XYZ only, and no rotation or scaling pivots or offsets. Anything else is
      refused with the node's name rather than approximated.
  No blend shapes, no per-vertex colour, no second UV set.
  No player colour mask unless --mask names one. A character without one wears no
      house colour at all; that is reported when it happens.
"""
import argparse
import os
import struct
import sys
import zlib

import numpy as np
from PIL import Image

MAGIC = b"C3DMODL1"
FORMAT_VERSION = 2
CELL = 1024.0
KTIME = 46186158000.0          # FBX time ticks to the second

MAT_NO_PLAYER = 1 << 1
MAT_CLOTH = 1 << 2          # the surface map's alpha is the cloth mask (a side's cloth colour)
MAT_EMBLEM = 1 << 3         # surface alpha = GDI emblem stencil, normal alpha = Nod's
ROLE_STATIC = 0
MAX_INFLUENCES = 4
MAX_BONES = 64                 # the renderer's palette; checked here so it fails early

CLIP_LOOP = 1 << 0
CLIP_LOCOMOTION = 1 << 1


def fail(msg):
    print("FBX|FAIL|" + msg)
    sys.exit(1)


# ---------------------------------------------------------------------------------- #
#  The binary FBX container
# ---------------------------------------------------------------------------------- #

class Node:
    __slots__ = ("name", "props", "children")

    def __init__(self, name, props, children):
        self.name, self.props, self.children = name, props, children

    def find(self, name):
        for c in self.children:
            if c.name == name:
                return c
        return None

    def findall(self, name):
        return [c for c in self.children if c.name == name]


_ARRAY = {b"f": "<f4", b"d": "<f8", b"l": "<i8", b"i": "<i4", b"b": "?"}


def _prop(buf, o):
    t = buf[o:o + 1]
    o += 1
    if t == b"Y":
        return struct.unpack_from("<h", buf, o)[0], o + 2
    if t == b"C":
        return bool(buf[o]), o + 1
    if t == b"I":
        return struct.unpack_from("<i", buf, o)[0], o + 4
    if t == b"F":
        return struct.unpack_from("<f", buf, o)[0], o + 4
    if t == b"D":
        return struct.unpack_from("<d", buf, o)[0], o + 8
    if t == b"L":
        return struct.unpack_from("<q", buf, o)[0], o + 8
    if t in (b"S", b"R"):
        n = struct.unpack_from("<I", buf, o)[0]
        o += 4
        raw = buf[o:o + n]
        return (raw.decode("utf-8", "replace") if t == b"S" else raw), o + n
    if t in _ARRAY:
        n, enc, clen = struct.unpack_from("<III", buf, o)
        o += 12
        data = buf[o:o + clen]
        o += clen
        if enc == 1:
            data = zlib.decompress(data)
        return np.frombuffer(data, dtype=_ARRAY[t], count=n).copy(), o
    raise ValueError("unknown FBX property type %r at byte %d" % (t, o - 1))


def _node(buf, o, wide):
    if wide:
        end, nprops, _ = struct.unpack_from("<QQQ", buf, o)
        o += 24
    else:
        end, nprops, _ = struct.unpack_from("<III", buf, o)
        o += 12
    nlen = buf[o]
    o += 1
    if end == 0:
        return None, o
    name = buf[o:o + nlen].decode("ascii", "replace")
    o += nlen
    props = []
    for _ in range(nprops):
        p, o = _prop(buf, o)
        props.append(p)
    children = []
    null = 25 if wide else 13
    while o < end and end - o != null:
        c, o = _node(buf, o, wide)
        if c is None:
            break
        children.append(c)
    return Node(name, props, children), end


def read_fbx(path):
    buf = open(path, "rb").read()
    if buf[:20] != b"Kaydara FBX Binary  ":
        fail("%s is not a binary FBX (an ASCII FBX has to be re-saved as binary)" % path)
    ver = struct.unpack_from("<I", buf, 23)[0]
    o, top = 27, []
    while o < len(buf):
        n, o2 = _node(buf, o, ver >= 7500)
        if n is None:
            break
        top.append(n)
        o = o2
    return Node("root", [ver], top)


def p70(node):
    out = {}
    p = node.find("Properties70") if node is not None else None
    if p:
        for c in p.children:
            out[c.props[0]] = c.props[4:]
    return out


# ---------------------------------------------------------------------------------- #
#  The scene: hierarchy, local transforms, curves
# ---------------------------------------------------------------------------------- #

def euler_xyz(deg):
    x, y, z = np.radians(np.asarray(deg, dtype=np.float64))
    cx, sx, cy, sy, cz, sz = np.cos(x), np.sin(x), np.cos(y), np.sin(y), np.cos(z), np.sin(z)
    rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return rz @ ry @ rx


class Scene:
    def __init__(self, path):
        self.path = path
        self.root = read_fbx(path)
        objs = self.root.find("Objects")
        self.obj = {o.props[0]: o for o in objs.children}
        self.conn = [c.props for c in self.root.find("Connections").children]
        self.models = {o.props[0]: o for o in objs.findall("Model")}
        self.children = {}
        for c in self.conn:
            if c[0] == "OO" and c[1] in self.models:
                self.children.setdefault(c[2], []).append(c[1])
        self.by_name = {self.name(i): i for i in self.models}
        self.globals = p70(self.root.find("GlobalSettings"))

    def name(self, oid):
        return self.obj[oid].props[1].split("\x00")[0]

    def kind(self, oid):
        o = self.obj[oid]
        return o.props[2] if len(o.props) > 2 else ""

    def local(self, oid):
        p = p70(self.models[oid])

        def vec(k, d):
            return np.array([float(x) for x in p[k]]) if k in p else np.array(d, float)

        for k in ("RotationPivot", "ScalingPivot", "RotationOffset", "ScalingOffset",
                  "GeometricTranslation", "GeometricRotation", "GeometricScaling"):
            if k in p:
                v = vec(k, [0, 0, 0])
                ident = [1, 1, 1] if "Scaling" in k and "Geometric" in k else [0, 0, 0]
                if np.abs(v - ident).max() > 1e-6:
                    fail("%s: node %s has %s %s, which this converter does not evaluate"
                         % (self.path, self.name(oid), k, v))
        if int(p.get("RotationOrder", [0])[0]) != 0:
            fail("%s: node %s is not XYZ euler order" % (self.path, self.name(oid)))
        return dict(T=vec("Lcl Translation", [0, 0, 0]), R=vec("Lcl Rotation", [0, 0, 0]),
                    S=vec("Lcl Scaling", [1, 1, 1]), pre=vec("PreRotation", [0, 0, 0]),
                    post=vec("PostRotation", [0, 0, 0]))

    def curves(self):
        """{model id: {property: [(times, values) or constant] for x, y, z}}."""
        nodes = {}
        for c in self.conn:
            if c[0] == "OP" and c[1] in self.obj and self.obj[c[1]].name == "AnimationCurveNode" \
                    and c[2] in self.models:
                nodes[c[1]] = (c[2], c[3])
        out = {}
        for cn, (mid, prop) in nodes.items():
            d = p70(self.obj[cn])
            ch = [float(d["d|" + a][0]) if ("d|" + a) in d else 0.0 for a in "XYZ"]
            for c in self.conn:
                if c[0] == "OP" and c[2] == cn and c[1] in self.obj \
                        and self.obj[c[1]].name == "AnimationCurve":
                    cv = self.obj[c[1]]
                    kt = cv.find("KeyTime").props[0].astype(np.float64) / KTIME
                    kv = cv.find("KeyValueFloat").props[0].astype(np.float64)
                    ch["XYZ".index(c[3][2])] = (kt, kv)
            out.setdefault(mid, {})[prop] = ch
        return out

    def duration(self):
        st = self.root.find("Objects").findall("AnimationStack")
        if not st:
            return 0.0
        return float(p70(st[0])["LocalStop"][0]) / KTIME

    def fps(self):
        """The file's own frame rate, from GlobalSettings.TimeMode."""
        rates = {1: 120, 2: 100, 3: 60, 4: 50, 5: 48, 6: 30, 7: 30, 8: 29.97, 9: 29.97,
                 10: 25, 11: 24, 13: 96, 14: 72, 15: 59.94}
        mode = int(self.globals.get("TimeMode", [0])[0])
        if mode == 12:
            return float(self.globals.get("CustomFrameRate", [30])[0])
        return float(rates.get(mode, 30))


def local_matrix(L, anim=None, t=0.0):
    vals = {"Lcl Translation": L["T"].copy(), "Lcl Rotation": L["R"].copy(),
            "Lcl Scaling": L["S"].copy()}
    for prop, arr in vals.items():
        if anim and prop in anim:
            for i, ch in enumerate(anim[prop]):
                arr[i] = np.interp(t, ch[0], ch[1]) if isinstance(ch, tuple) else ch
    m = np.eye(4)
    m[:3, :3] = euler_xyz(L["pre"]) @ euler_xyz(vals["Lcl Rotation"]) \
        @ euler_xyz(L["post"]).T @ np.diag(vals["Lcl Scaling"])
    m[:3, 3] = vals["Lcl Translation"]
    return m


def world_matrices(mesh_sc, anim_sc=None, anim_curves=None, t=0.0):
    """Every node's world matrix. A bone the animation file carries takes ALL its local
    values from there (the static channels too: an exporter writes the pose it sampled
    from, which need not be the mesh file's); a bone it lacks keeps the mesh file's."""
    W = {}

    def walk(oid, parent):
        nm = mesh_sc.name(oid)
        if anim_sc is not None and nm in anim_sc.by_name:
            aid = anim_sc.by_name[nm]
            m = local_matrix(anim_sc.local(aid), anim_curves.get(aid), t)
        else:
            m = local_matrix(mesh_sc.local(oid))
        w = parent @ m
        W[nm] = w
        for c in mesh_sc.children.get(oid, []):
            walk(c, w)

    for c in mesh_sc.children.get(0, []):
        walk(c, np.eye(4))
    return W


# ---------------------------------------------------------------------------------- #
#  The mesh and its skin
# ---------------------------------------------------------------------------------- #

def the_mesh(sc):
    geoms = [o for o in sc.obj.values() if o.name == "Geometry" and sc.kind(o.props[0]) == "Mesh"]
    if len(geoms) != 1:
        fail("%s has %d meshes; one skinned mesh per file" % (sc.path, len(geoms)))
    g = geoms[0]
    gid = g.props[0]
    model = [c[2] for c in sc.conn if c[0] == "OO" and c[1] == gid and c[2] in sc.models][0]
    return g, model


def layer(g, name, data, index):
    le = g.find(name)
    if le is None:
        return None, None
    mapping = le.find("MappingInformationType").props[0]
    ref = le.find("ReferenceInformationType").props[0]
    arr = le.find(data).props[0]
    idx = le.find(index).props[0] if (ref == "IndexToDirect" and le.find(index)) else None
    return (mapping, arr, idx), ref


def corners(sc, g):
    """Per-triangle corners: control point, normal, uv, material."""
    v = g.find("Vertices").props[0].reshape(-1, 3).astype(np.float64)
    pvi = g.find("PolygonVertexIndex").props[0]
    nrm, _ = layer(g, "LayerElementNormal", "Normals", "NormalsIndex")
    uvl, _ = layer(g, "LayerElementUV", "UV", "UVIndex")
    mat, _ = layer(g, "LayerElementMaterial", "Materials", None)
    if nrm is None or uvl is None:
        fail("%s: the mesh needs normals and a UV set" % sc.path)

    def per_corner(lay, width, ci, cp, poly):
        mapping, arr, idx = lay
        key = {"ByPolygonVertex": ci, "ByVertice": cp, "ByVertex": cp,
               "ByPolygon": poly, "AllSame": 0}[mapping]
        k = idx[key] if idx is not None else key
        return arr[k * width:(k + 1) * width]

    out = []
    start = poly = 0
    for k in range(len(pvi)):
        if pvi[k] >= 0:
            continue
        ring = list(range(start, k + 1))
        pts = [int(pvi[c]) if pvi[c] >= 0 else int(~pvi[c]) for c in ring]
        m = int(per_corner(mat, 1, 0, 0, poly)[0]) if mat is not None else 0
        for j in range(1, len(ring) - 1):
            tri = []
            for c, cp in ((ring[0], pts[0]), (ring[j], pts[j]), (ring[j + 1], pts[j + 1])):
                tri.append((cp, per_corner(nrm, 3, c, cp, poly).astype(np.float64),
                            per_corner(uvl, 2, c, cp, poly).astype(np.float64)))
            out.append((tri, m))
        start = k + 1
        poly += 1
    return v, out


def clusters(sc):
    out = []
    for oid, o in sc.obj.items():
        if o.name != "Deformer" or sc.kind(oid) != "Cluster":
            continue
        bone = [c[1] for c in sc.conn if c[0] == "OO" and c[2] == oid and c[1] in sc.models]
        idx, w = o.find("Indexes"), o.find("Weights")
        if not bone or idx is None or w is None or len(idx.props[0]) == 0:
            continue
        out.append(dict(bone=sc.name(bone[0]), idx=idx.props[0].astype(np.int64),
                        w=w.props[0].astype(np.float64),
                        T=o.find("Transform").props[0].reshape(4, 4).T.astype(np.float64),
                        TL=o.find("TransformLink").props[0].reshape(4, 4).T.astype(np.float64)))
    return out


def inverse_bind(cls, mesh_world):
    """The matrix taking a bind-pose vertex (mesh LOCAL coordinates, as the file stores
    them) into each bone's own space.

    The FBX SDK's meaning is Transform = the MESH's global matrix at bind and
    TransformLink = the BONE's, so the answer is inv(TL) @ T. Some 3ds Max exports
    write Transform already composed, as inv(TL) @ mesh global; for those TL @ T is the
    mesh's own matrix and T is the answer as it stands. Both occur in delivered assets,
    so the file is tested rather than assumed, and a file that is neither is refused."""
    composed = all(np.allclose(c["TL"] @ c["T"], mesh_world, atol=1e-3) for c in cls)
    standard = all(np.allclose(c["T"], mesh_world, atol=1e-3) for c in cls)
    if not composed and not standard:
        fail("the skin's bind matrices match neither FBX convention")
    for c in cls:
        c["IB"] = c["T"] if composed else np.linalg.inv(c["TL"]) @ c["T"]
    return "composed (3ds Max)" if composed else "standard"


# ---------------------------------------------------------------------------------- #
#  Axes
# ---------------------------------------------------------------------------------- #

def pack_basis(sc, forward):
    """3x3 taking the FBX scene's world into pack axes (x east, y up, z south), with the
    character's front turned to face north (-z), which is facing zero.

    The world's up axis comes from the file's GlobalSettings. Which way the CHARACTER
    faces is not something FBX records, so --forward states it, and --preview is how
    that statement is checked: the preview draws the model from the south."""
    up_i = int(sc.globals.get("UpAxis", [1])[0])
    up_s = int(sc.globals.get("UpAxisSign", [1])[0])
    up = np.zeros(3)
    up[up_i] = up_s
    sign = -1.0 if forward.startswith("-") else 1.0
    f = np.zeros(3)
    f["xyz".index(forward[-1])] = sign
    if abs(f @ up) > 0.5:
        fail("--forward %s lies along the file's up axis" % forward)
    right = np.cross(f, up)          # the character's right hand, in the file's world
    # pack: forward -> north (0,0,-1), up -> (0,1,0), right -> east (1,0,0)
    src = np.stack([right, up, f])
    dst = np.array([[1.0, 0, 0], [0, 1.0, 0], [0, 0, -1.0]])
    return dst.T @ src


# ---------------------------------------------------------------------------------- #
#  Poses
# ---------------------------------------------------------------------------------- #

def mat_to_quat(m):
    """Unit quaternion (x, y, z, w) of a rotation matrix."""
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


def quat_to_mat(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def decompose(m):
    """Model-space bone pose -> (quat, translation, uniform scale). A bone that is not
    uniformly scaled, or is mirrored, cannot be carried and is refused."""
    a = m[:3, :3]
    s = np.cbrt(np.linalg.det(a))
    if s <= 0:
        fail("a bone's pose is mirrored")
    r = a / s
    if np.abs(r @ r.T - np.eye(3)).max() > 1e-3:
        fail("a bone's pose carries non-uniform scale or shear")
    return mat_to_quat(r), m[:3, 3].copy(), s


# ---------------------------------------------------------------------------------- #
#  Textures
# ---------------------------------------------------------------------------------- #

def load_texture(path, size, mask_path=None):
    """RGBA8, BOTTOM row first, which is the order glTexImage2D takes with v increasing
    upward. Box filtered when the source is an exact multiple of the target side."""
    img = Image.open(path).convert("RGB")
    w, h = img.size
    side = min(size, w, h)
    a = np.asarray(img, dtype=np.float32) / 255.0
    if w % side == 0 and h % side == 0:
        a = a.reshape(side, h // side, side, w // side, 3).mean(axis=(1, 3))
    else:
        a = np.asarray(img.resize((side, side), Image.LANCZOS), dtype=np.float32) / 255.0
    out = np.zeros((side, side, 4), dtype=np.uint8)
    out[:, :, :3] = np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if mask_path:
        m = Image.open(mask_path).convert("L").resize((side, side), Image.LANCZOS)
        out[:, :, 3] = np.asarray(m, dtype=np.uint8)
    return side, side, out[::-1].tobytes()


def material_textures(sc, mesh_model, texdir):
    """The mesh's materials in slot order, each with its diffuse texture file."""
    mats = [c[1] for c in sc.conn if c[0] == "OO" and c[2] == mesh_model
            and c[1] in sc.obj and sc.obj[c[1]].name == "Material"]
    out = []
    for mid in mats:
        tex = [c[1] for c in sc.conn if c[0] == "OP" and c[2] == mid and c[3] == "DiffuseColor"
               and sc.obj[c[1]].name == "Texture"]
        path = None
        if tex:
            fn = sc.obj[tex[0]].find("FileName") or sc.obj[tex[0]].find("RelativeFilename")
            base = fn.props[0].replace("\\", "/").rsplit("/", 1)[-1]
            cand = os.path.join(texdir, base)
            if os.path.exists(cand):
                path = cand
        out.append((sc.name(mid), path))
    return out


# ---------------------------------------------------------------------------------- #
#  Preview: a software rasteriser, so a conversion can be looked at without the game
# ---------------------------------------------------------------------------------- #

def raster(pos, tris, uv, tmat, texs, size, yaw_deg, scale, centre):
    """Orthographic, from the south and a little above, turned by yaw. pos is pack units."""
    y = np.radians(yaw_deg)
    ry = np.array([[np.cos(y), 0, np.sin(y)], [0, 1, 0], [-np.sin(y), 0, np.cos(y)]])
    p = pos @ ry.T
    tilt = np.radians(15)
    rx = np.array([[1, 0, 0], [0, np.cos(tilt), -np.sin(tilt)], [0, np.sin(tilt), np.cos(tilt)]])
    p = p @ rx.T
    sx = (p[:, 0] - centre[0]) * scale + size / 2
    sy = -(p[:, 1] - centre[1]) * scale + size * 0.55
    sz = -p[:, 2]                                   # camera sits south (+z), looking north
    img = np.zeros((size, size, 3), np.float32)
    img[:] = (0.16, 0.17, 0.2)
    zb = np.full((size, size), np.inf, np.float32)
    light = np.array([-0.4, 0.7, 0.6])
    light /= np.linalg.norm(light)
    fn = np.cross(p[tris[:, 1]] - p[tris[:, 0]], p[tris[:, 2]] - p[tris[:, 0]])
    fn /= np.linalg.norm(fn, axis=1, keepdims=True) + 1e-12
    for t in range(len(tris)):
        a, b, c = tris[t]
        xs, ys, zs = sx[[a, b, c]], sy[[a, b, c]], sz[[a, b, c]]
        x0, x1 = int(max(0, np.floor(xs.min()))), int(min(size - 1, np.ceil(xs.max())))
        y0, y1 = int(max(0, np.floor(ys.min()))), int(min(size - 1, np.ceil(ys.max())))
        den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if x0 > x1 or y0 > y1 or abs(den) < 1e-9:
            continue
        X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        l0 = ((ys[1] - ys[2]) * (X - xs[2]) + (xs[2] - xs[1]) * (Y - ys[2])) / den
        l1 = ((ys[2] - ys[0]) * (X - xs[2]) + (xs[0] - xs[2]) * (Y - ys[2])) / den
        l2 = 1 - l0 - l1
        z = l0 * zs[0] + l1 * zs[1] + l2 * zs[2]
        sub = zb[y0:y1 + 1, x0:x1 + 1]
        m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0) & (z < sub)
        if not m.any():
            continue
        U = l0 * uv[a, 0] + l1 * uv[b, 0] + l2 * uv[c, 0]
        V = l0 * uv[a, 1] + l1 * uv[b, 1] + l2 * uv[c, 1]
        tex = texs[tmat[t]]
        th, tw = tex.shape[:2]
        tx = (np.mod(U, 1.0) * tw).astype(int).clip(0, tw - 1)
        ty = (np.mod(V, 1.0) * th).astype(int).clip(0, th - 1)   # rows are bottom first
        n = fn[t] if fn[t][2] > 0 else -fn[t]
        shade = 0.45 + 0.65 * max(0.0, float(n @ light))
        sub[m] = z[m]
        img[y0:y1 + 1, x0:x1 + 1][m] = tex[ty, tx, :3][m] * shade
    return Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))


# ---------------------------------------------------------------------------------- #

def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--type", required=True, help="the engine type this replaces, e.g. E1")
    ap.add_argument("--mesh", required=True, help="the FBX holding the skinned mesh")
    ap.add_argument("--texdir", required=True, help="where the diffuse textures are")
    ap.add_argument("--clip", action="append", default=[],
                    help="name=file.fbx[:once]. Clips loop unless marked once.")
    ap.add_argument("--forward", default="+z",
                    help="the axis the character faces in the file's world (default +z)")
    ap.add_argument("--height", type=float, required=True,
                    help="the character's standing height in map cells")
    ap.add_argument("--texmax", type=int, default=512)
    ap.add_argument("--roughness", type=float, default=0.8,
                    help="the roughness the generated surface map carries (default 0.8)")
    ap.add_argument("--mask", action="append", default=[],
                    help="material=mask.png: the player-colour mask for that material")
    ap.add_argument("--preview", help="write a PNG of the bind pose and every clip")
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)

    sc = Scene(a.mesh)
    g, mesh_model = the_mesh(sc)
    W0 = world_matrices(sc)
    mesh_world = W0[sc.name(mesh_model)]
    cls = clusters(sc)
    if not cls:
        fail("%s has no skin; a rigid model goes through blender_c3m.py" % a.mesh)
    if len(cls) > MAX_BONES:
        fail("%d bones, the renderer's palette holds %d" % (len(cls), MAX_BONES))
    convention = inverse_bind(cls, mesh_world)
    bone_names = [c["bone"] for c in cls]

    # ---- axes and scale: FBX world -> pack units ---------------------------------
    B = pack_basis(sc, a.forward)
    ctl, tri_list = corners(sc, g)
    bind_world = (mesh_world @ np.c_[ctl, np.ones(len(ctl))].T).T[:, :3] @ B.T
    ground = bind_world[:, 1].min()
    tall = bind_world[:, 1].max() - ground
    k = a.height * CELL / tall
    # the feet stand on y = 0 and the model turns about the point between them
    cx = 0.5 * (bind_world[:, 0].min() + bind_world[:, 0].max())
    cz = 0.5 * (bind_world[:, 2].min() + bind_world[:, 2].max())
    P = np.eye(4)
    P[:3, :3] = B * k
    P[:3, 3] = -np.array([cx, ground, cz]) * k
    # pack <- FBX world. A bone's pose becomes P @ W (its own space to pack) and its
    # inverse bind IB @ inv(mesh_world) @ inv(P) (pack to its own space), so the pair
    # multiplies back to P @ W @ IB on the file's own local vertex.
    Pinv = np.linalg.inv(P)

    # ---- influences per control point ------------------------------------------------
    infl = [[] for _ in range(len(ctl))]
    for bi, c in enumerate(cls):
        for vi, w in zip(c["idx"], c["w"]):
            if w > 0:
                infl[vi].append((w, bi))
    dropped = 0
    skin_b = np.zeros((len(ctl), 4), np.uint8)
    skin_w = np.zeros((len(ctl), 4), np.uint8)
    for vi, lst in enumerate(infl):
        if not lst:
            fail("control point %d is bound to no bone" % vi)
        lst.sort(reverse=True)
        dropped += max(0, len(lst) - MAX_INFLUENCES)
        lst = lst[:MAX_INFLUENCES]
        tot = sum(w for w, _ in lst)
        q = [int(round(255.0 * w / tot)) for w, _ in lst]
        q[0] += 255 - sum(q)                        # the heaviest takes the rounding
        for j, ((_, bi), qw) in enumerate(zip(lst, q)):
            skin_b[vi, j], skin_w[vi, j] = bi, qw

    # ---- vertices: welded on (point, normal, uv) -----------------------------------
    nB = B / np.linalg.norm(B, axis=1, keepdims=True)
    nrm_mat = nB @ mesh_world[:3, :3]
    verts, vkey, idx_by_mat = [], {}, {}
    for tri, m in tri_list:
        ids = []
        for cp, n, uv in tri:
            n = nrm_mat @ n
            n = n / (np.linalg.norm(n) + 1e-12)
            key = (cp, tuple(np.round(n, 4)), tuple(np.round(uv, 5)))
            if key not in vkey:
                vkey[key] = len(verts)
                verts.append((cp, n, uv))
            ids.append(vkey[key])
        idx_by_mat.setdefault(m, []).extend(ids)
    # positions: the bind pose in the FBX world, then scaled and seated in pack units
    world_pts = (mesh_world @ np.c_[ctl, np.ones(len(ctl))].T).T[:, :3]
    pack_pts = (P @ np.c_[world_pts, np.ones(len(ctl))].T).T[:, :3]

    # ---- materials and textures ------------------------------------------------------
    mt = material_textures(sc, mesh_model, a.texdir)
    masks = dict(s.split("=", 1) for s in a.mask)
    # NEUTRAL SURFACE MAPS, because the renderer's shader samples a normal map and an
    # occlusion/roughness/metal map for every material and has no "absent" branch. A
    # delivery that is a colour map only gets a flat normal (the geometric one) and a
    # matte non-metal with no baked occlusion, shared by every material: 4x4 each, so
    # the cost is 128 bytes rather than a second atlas.
    texs = [(4, 4, bytes([128, 128, 255, 255]) * 16),
            (4, 4, bytes([255, int(round(255 * a.roughness)), 0, 255]) * 16)]
    FLAT_NORMAL, NEUTRAL_ORM = 0, 1
    mats = []
    for name, path in mt:
        if path is None:
            fail("material %s: its diffuse texture was not found in %s" % (name, a.texdir))
        w, h, data = load_texture(path, a.texmax, masks.get(name))
        texs.append((w, h, data))
        mats.append((len(texs) - 1, FLAT_NORMAL, NEUTRAL_ORM,
                     0 if name in masks else MAT_NO_PLAYER, name))

    # submeshes, one per material, all in part 0
    idx, subs = [], []
    for m in sorted(idx_by_mat):
        subs.append((0, m, len(idx), len(idx_by_mat[m])))
        idx.extend(idx_by_mat[m])

    # ---- bones: inverse bind in pack space -------------------------------------------
    inv_mw = np.linalg.inv(mesh_world)
    bone_ib = [c["IB"] @ inv_mw @ Pinv for c in cls]

    # ---- clips -----------------------------------------------------------------------
    clips = []
    for spec in a.clip:
        name, rest = spec.split("=", 1)
        once = rest.endswith(":once")
        path = rest[:-5] if once else rest
        an = Scene(path)
        missing = [b for b in bone_names if b not in an.by_name]
        if missing:
            print("FBX|note|clip %s does not animate %s; they hold the mesh file's pose"
                  % (name, ", ".join(missing)))
        cur = an.curves()
        fps = an.fps()
        dur = an.duration()
        nframe = max(1, int(round(dur * fps)) + (1 if once else 0))
        frames = []
        feet = []
        for f in range(nframe):
            W = world_matrices(sc, an, cur, f / fps)
            pose = []
            for b in bone_names:
                # bone space -> pack: the bone's FBX world, then the axes, scale and seat.
                # The inverse bind carries the opposite trip, pack -> bone space.
                q, t, s = decompose(P @ W[b])
                pose.append((q, t, s))
            # keep quaternions in one hemisphere across frames, so a lerp never flips
            if frames:
                for j in range(len(pose)):
                    if np.dot(pose[j][0], frames[-1][j][0]) < 0:
                        pose[j] = (-pose[j][0], pose[j][1], pose[j][2])
            frames.append(pose)
            # Skin bones only: a Biped carries a "Footsteps" helper that sits still on the
            # ground, is always the lowest node named foot, and measured a stride of zero.
            feet.append({b: (P @ W[b])[:3, 3] for b in bone_names if "foot" in b.lower()})
        # STRIDE: ground passing under the planted (lowest) foot, per loop
        stride = 0.0
        if feet and feet[0]:
            for f in range(nframe - 1 if once else nframe):
                a0, a1 = feet[f], feet[(f + 1) % nframe]
                low = min(a0, key=lambda b: a0[b][1])
                stride += abs(a1[low][2] - a0[low][2])
        flags = (0 if once else CLIP_LOOP)
        if stride > 0.25 * a.height * CELL:
            flags |= CLIP_LOCOMOTION
        else:
            stride = 0.0
        clips.append((name, fps, stride, flags, frames))

    # ---- write ------------------------------------------------------------------------
    bb = list(pack_pts.min(0)) + list(pack_pts.max(0))
    out = bytearray()
    out += MAGIC
    out += struct.pack("<8I", FORMAT_VERSION, len(verts), len(idx), 1, len(subs), len(mats),
                       len(texs), len(bone_names))
    tn = a.type.encode("ascii")[:8]
    out += tn + b"\0" * (8 - len(tn))
    out += struct.pack("<6f", *bb)
    for cp, n, uv in verts:
        p = pack_pts[cp]
        out += struct.pack("<8f", p[0], p[1], p[2], n[0], n[1], n[2], uv[0], uv[1])
    for i in idx:
        out += struct.pack("<I", i)
    out += struct.pack("<I3f", ROLE_STATIC, 0.0, 0.0, 0.0) + b"body" + b"\0" * 12
    for s in subs:
        out += struct.pack("<4I", *s)
    for base, norm, orm, flags, _ in mats:
        out += struct.pack("<3iI", base, norm, orm, flags)
    for w, h, data in texs:
        out += struct.pack("<3I", w, h, len(data)) + data
    # ---- the version 2 tail -------------------------------------------------------------
    out += struct.pack("<4I", len(clips), 0, 0, 0)
    for cp, n, uv in verts:
        out += bytes(skin_b[cp]) + bytes(skin_w[cp])
    for b, ib in zip(bone_names, bone_ib):
        nb = b.encode("ascii", "replace")[:31]
        out += nb + b"\0" * (32 - len(nb))
        out += struct.pack("<12f", *ib[:3, :].reshape(-1))
    for name, fps, stride, flags, frames in clips:
        nn = name.encode("ascii")[:15]
        out += nn + b"\0" * (16 - len(nn))
        out += struct.pack("<I2fI", len(frames), fps, stride, flags)
        for pose in frames:
            for q, t, s in pose:
                out += struct.pack("<8f", q[0], q[1], q[2], q[3], t[0], t[1], t[2], s)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "wb") as f:
        f.write(bytes(out))

    # ---- report -------------------------------------------------------------------------
    print("FBX|type|%s" % a.type)
    print("FBX|skin|%s bind convention|%d bones|%d influences dropped past %d"
          % (convention, len(bone_names), dropped, MAX_INFLUENCES))
    print("FBX|verts|%d|tris|%d|mats|%d" % (len(verts), len(idx) // 3, len(mats)))
    print("FBX|maps|colour only delivered: flat normal and a matte surface (roughness %.2f)"
          " generated" % a.roughness)
    for (name, path), (w, h, _) in zip(mt, texs[2:]):
        print("FBX|mat|%s|%s|%dx%d|%s" % (name, os.path.basename(path), w, h,
                                         "player mask" if name in masks else "no player colour"))
    for name, fps, stride, flags, frames in clips:
        print("FBX|clip|%s|%d frames at %g fps|%.2f s|%s%s" % (
            name, len(frames), fps, len(frames) / fps, "loop" if flags & CLIP_LOOP else "once",
            ("|stride %.0f units per loop (%.3f cells)" % (stride, stride / CELL))
            if flags & CLIP_LOCOMOTION else ""))
    print("FBX|bbox|x %.1f..%.1f|y %.1f..%.1f|z %.1f..%.1f" % tuple(
        bb[i] for i in (0, 3, 1, 4, 2, 5)))
    print("FBX|cells|w %.3f|h %.3f|d %.3f" % ((bb[3] - bb[0]) / CELL, (bb[4] - bb[1]) / CELL,
                                             (bb[5] - bb[2]) / CELL))
    if not masks:
        print("FBX|note|no player-colour mask: every house draws this model identically")
    print("FBX|wrote|%s|%d bytes" % (a.out, len(out)))

    if a.preview:
        preview(a.preview, pack_pts, verts, idx, subs, mats, texs, skin_b, skin_w, bone_ib, clips)
        print("FBX|preview|%s" % a.preview)
    return 0


def skin_positions(pack_pts, skin_b, skin_w, bone_ib, pose):
    """The renderer's arithmetic, repeated here so the preview shows what the game will."""
    M = []
    for (q, t, s), ib in zip(pose, bone_ib):
        h = np.eye(4)
        h[:3, :3] = quat_to_mat(q) * s
        h[:3, 3] = t
        ibm = np.eye(4)
        ibm[:3, :] = ib[:3, :]
        M.append(h @ ibm)
    M = np.array(M)
    ph = np.c_[pack_pts, np.ones(len(pack_pts))]
    out = np.zeros((len(pack_pts), 3))
    for j in range(4):
        w = skin_w[:, j].astype(np.float64) / 255.0
        out += w[:, None] * np.einsum("nij,nj->ni", M[skin_b[:, j]], ph)[:, :3]
    return out


def preview(path, pack_pts, verts, idx, subs, mats, texs, skin_b, skin_w, bone_ib, clips):
    tris = np.array(idx, dtype=np.int64).reshape(-1, 3)
    cps = np.array([v[0] for v in verts])
    uv = np.array([v[2] for v in verts])
    tmat = np.zeros(len(tris), np.int64)
    for part, m, i0, n in subs:
        tmat[i0 // 3:(i0 + n) // 3] = mats[m][0]      # the material's base map
    texarr = [np.frombuffer(d, np.uint8).reshape(h, w, 4).astype(np.float32) / 255.0
              for w, h, d in texs]
    size = 220
    hgt = pack_pts[:, 1].max()
    scale = 0.8 * size / hgt
    centre = (0.0, hgt * 0.5)
    rows = [[raster(pack_pts[cps], tris, uv, tmat, texarr, size, yaw, scale, centre)
             for yaw in (180, 270, 0, 90)]]
    for name, fps, stride, flags, frames in clips:
        pick = [frames[int(i * len(frames) / 4)] for i in range(4)]
        row = []
        for pose in pick:
            p = skin_positions(pack_pts, skin_b, skin_w, bone_ib, pose)
            row.append(raster(p[cps], tris, uv, tmat, texarr, size, 90, scale, centre))
        rows.append(row)
    sheet = Image.new("RGB", (size * 4, size * len(rows)))
    for r, row in enumerate(rows):
        for c, im in enumerate(row):
            sheet.paste(im, (c * size, r * size))
    sheet.save(path)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
