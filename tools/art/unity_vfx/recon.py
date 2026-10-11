"""Rebuilds a Unity particle renderer's geometry from recorded particle state, the way
the game's renderer does it, so the construction can be checked against Unity's own
(the probe files) before it is trusted in C++. Positions only; UVs, normals and streams
are compared too where the mode defines them."""
import numpy as np

def system_scale(P, s):
    """What the system's transform scale does to a recorded particle, as fitted against
    Unity's own baked geometry for the x2 and x0.5 variants (every vertex within 2 cm):
    the SIZE is multiplied by it -- except a mesh particle aligned to the system, whose
    size it leaves alone -- and the position and velocity are used as recorded, in either
    simulation space. Returns a scaled copy."""
    L = np.array(s['l2w']).reshape(4, 4).T
    k = float(np.linalg.norm(L[:3, 0]))
    P = P.copy()
    if not (s.get('renderMode') == 'Mesh' and s.get('alignment') == 'Local'):
        P[:, 7:10] *= k
    return P

def euler_zxy(deg):
    """Unity's Euler convention: rotate about Z, then X, then Y (extrinsic), i.e. the
    matrix Ry * Rx * Rz applied to a column vector."""
    x, y, z = np.radians(deg)
    cx, sx, cy, sy, cz, sz = np.cos(x), np.sin(x), np.cos(y), np.sin(y), np.cos(z), np.sin(z)
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return Ry @ Rx @ Rz

QUAD = np.array([[-0.5, 0.5], [0.5, 0.5], [0.5, -0.5], [-0.5, -0.5]])
QUAD_UV = np.array([[0, 1], [1, 1], [1, 0], [0, 0]], float)

def rot2(a):
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, -s], [s, c]])

def billboard(p, cam):
    """View-aligned quad in the camera's (right, up, forward) basis, turned by the
    particle's Euler rotation with the ROLL NEGATED (measured: a pure rotation.z turns
    the quad clockwise on screen, a pure rotation.y swings its left edge away from the
    camera)."""
    B = np.stack([np.array(cam['right']), np.array(cam['up']), np.array(cam['forward'])], 1)
    E = euler_zxy(np.array([p[4], p[5], -p[6]]))
    c = np.concatenate([QUAD * p[7:9], np.zeros((4, 1))], 1)
    return p[1:4] + (B @ E @ c.T).T

def horizontal(p, cam):
    """World XZ quad, rotated by -rotation.z, and 1/sqrt(2) of the nominal size (measured
    from Unity's own baked geometry)."""
    o = (QUAD @ rot2(-np.radians(p[6])).T) * p[7:9] / np.sqrt(2.0)
    return p[1:4] + np.stack([o[:, 0], np.zeros(4), o[:, 1]], 1)

def stretched(p, cam, s):
    """Head at the particle, tail back along the velocity; width across it, facing the
    camera. Length = size.y * lengthScale + speed * velocityScale."""
    v = p[14:17]; sp = np.linalg.norm(v)
    d = v / max(sp, 1e-6)
    F = np.array(cam['forward']) if s.get('_useF', 1) else (p[1:4] - np.array(cam['position']))
    w = np.cross(d, F); w /= max(np.linalg.norm(w), 1e-9)
    L = p[8] * s['lengthScale'] + sp * s['velocityScale']
    W = p[7]
    head, tail = p[1:4], p[1:4] - d * L
    return np.array([head + w * W / 2, tail + w * W / 2, tail - w * W / 2, head - w * W / 2])

def mesh_xform(p, cam, s):
    """Model matrix of one mesh particle: basis (camera or local), then Euler rotation,
    then per-axis size."""
    Rp = euler_zxy(p[4:7])
    S = np.diag(p[7:10])
    if s['alignment'] == 'View':
        B = np.stack([np.array(cam['right']), np.array(cam['up']), np.array(cam['forward'])], 1)
    else:
        B = np.eye(3)
    return B @ Rp @ S
