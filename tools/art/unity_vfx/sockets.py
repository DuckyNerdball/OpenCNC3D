#!/usr/bin/env python3
"""Measures the gun sockets of the authored models: where a muzzle flash plays and where a
projectile's trail starts, as points on the model itself.

    python3 tools/art/unity_vfx/sockets.py [--models game/models3d] [--out game/vfx3d/sockets.txt]

Nothing here is typed in by hand. Every socket is found on the model's own vertices, so a
re-exported model moves its sockets with it when this is run again:

  A SOLDIER'S WEAPON is the set of vertices skinned wholly to the "Weapon" bone. In that
  bone's space the weapon lies along x (the graft puts it there), and which END is the
  muzzle is read off the "aim" clip: the end that is furthest forward (model -z, the way
  a soldier faces) when he aims. The socket is the middle of the last 1.5 cm of that end,
  pointing out of it. The rocket soldier also gets the other end ("back"), where a
  launcher's backblast leaves.

  A VEHICLE'S BARRELS are the turret part's vertices within 3 units of its furthest
  forward point (+z, the way the hulls are authored), split by side; each barrel's socket
  is the middle of its muzzle ring. The Mammoth's missile pods are the turret's outboard
  boxes (|x| > 230, above y 280), and each pod's two tubes are the two clusters of its
  front face.

Output, one socket per line:

    MODEL SET INDEX SPACE px py pz fx fy fz ux uy uz

SPACE is bone:<name> (the point is in that bone's space) or part:<name> (model units, and
the part's own turn applies). f is the direction a shot leaves in, u the socket's up. The
renderer (game/unityvfx_mod.h) picks the socket of a set by the engine's barrel: index
0 is the LEFT barrel, the one a two-shooter fires first (TurretClass::Fire_Coord moves a
first shot to the facing's west, a second to its east).
"""
import argparse, os, struct, sys
import numpy as np


def read_c3m(path):
    d = open(path, 'rb').read()
    if d[:8] != b'C3DMODL1':
        raise SystemExit('%s: not a c3m' % path)
    ver, nv, ni, npart, ns, nm, nt, nbone = struct.unpack_from('<8I', d, 8)
    off = 72
    vert = np.frombuffer(d, '<f4', nv * 8, off).reshape(nv, 8).astype(np.float64)
    off += nv * 32
    idx = np.frombuffer(d, '<u4', ni, off)
    off += ni * 4
    parts = []
    for _ in range(npart):
        role = struct.unpack_from('<I', d, off)[0]
        parts.append((role, d[off + 16:off + 32].split(b'\0')[0].decode()))
        off += 32
    subs = np.frombuffer(d, '<u4', ns * 4, off).reshape(ns, 4)
    off += ns * 16 + nm * 16
    for _ in range(nt):
        w, h, sz = struct.unpack_from('<3I', d, off)
        off += 12 + sz
    m = dict(vert=vert, idx=idx, parts=parts, subs=subs, bones=[], clips={})
    if ver >= 2 and nbone:
        nclip = struct.unpack_from('<I', d, off)[0]
        off += 16
        m['skin'] = np.frombuffer(d, 'u1', nv * 8, off).reshape(nv, 8)
        off += nv * 8
        for _ in range(nbone):
            name = d[off:off + 32].split(b'\0')[0].decode()
            m['bones'].append((name, np.frombuffer(d, '<f4', 12, off + 32).reshape(3, 4).astype(np.float64)))
            off += 80
        for _ in range(nclip):
            name = d[off:off + 16].split(b'\0')[0].decode()
            nf = struct.unpack_from('<I', d, off + 16)[0]
            off += 32
            m['clips'][name] = np.frombuffer(d, '<f4', nf * nbone * 8, off).reshape(nf, nbone, 8)
            off += nf * nbone * 32
    return m


def quat_mat(q):
    x, y, z, w = q / np.linalg.norm(q)
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def fmt(v):
    return ' '.join('%.3f' % x for x in v)


def soldier(name, m, out):
    bones = [b[0] for b in m['bones']]
    if 'Weapon' not in bones or 'aim' not in m['clips']:
        print('%s: no Weapon bone or no aim clip, no sockets' % name)
        return
    wb = bones.index('Weapon')
    sk = m['skin']
    sel = (sk[:, 0] == wb) & (sk[:, 4] == 255)
    ib = m['bones'][wb][1]
    B = m['vert'][sel, :3] @ ib[:, :3].T + ib[:, 3]
    # which end points forward when he aims: model -z is his front
    p = m['clips']['aim'][0, wb]
    R, T, s = quat_mat(p[:4]), p[4:7], p[7]
    lo = B[:, 0].argmin()
    hi = B[:, 0].argmax()
    zlo = (R @ (B[lo] * s) + T)[2]
    zhi = (R @ (B[hi] * s) + T)[2]
    front = -1.0 if zlo < zhi else 1.0
    for sset, sgn in (('muzzle', front), ('back', -front)):
        if sset == 'back' and name not in ('E3',):
            continue
        end = B[:, 0].max() if sgn > 0 else B[:, 0].min()
        tip = B[np.abs(B[:, 0] - end) < 1.5]
        c = (tip.min(0) + tip.max(0)) * 0.5
        c[0] = end
        out.append('%s %s 0 bone:Weapon %s %s 0 1 0' % (name, sset, fmt(c), fmt((sgn, 0, 0))))
        print('%s %s: bone space %s, %d tip vertices' % (name, sset, fmt(c), len(tip)))


def vehicle(name, m, out):
    tp = [i for i, (role, _) in enumerate(m['parts']) if role == 1]
    if not tp:
        print('%s: no turret part, no sockets' % name)
        return
    pname = m['parts'][tp[0]][1]
    ids = set()
    for part, mat, i0, n in m['subs']:
        if part == tp[0]:
            ids.update(m['idx'][i0:i0 + n].tolist())
    T = m['vert'][sorted(ids), :3]
    zmax = T[:, 2].max()
    ring = T[T[:, 2] > zmax - 3]
    for i, side in enumerate((ring[ring[:, 0] > 0], ring[ring[:, 0] < 0])):   # left first
        if not len(side):
            continue
        c = (side.min(0) + side.max(0)) * 0.5
        c[2] = zmax
        out.append('%s cannon %d part:%s %s 0 0 1 0 1 0' % (name, i, pname, fmt(c)))
        print('%s cannon %d: %s' % (name, i, fmt(c)))
    if name != 'HTNK':
        return
    k = 0
    for sgn in (1, -1):                          # left pod (+x) first
        pod = T[(T[:, 0] * sgn > 230) & (T[:, 1] > 280)]
        face = pod[pod[:, 2] > pod[:, 2].max() - 8]
        # the two tubes: split the face at the middle of its x range
        mid = 0.5 * (face[:, 0].min() + face[:, 0].max())
        tubes = [face[face[:, 0] * sgn < mid * sgn], face[face[:, 0] * sgn >= mid * sgn]]
        for tube in tubes:
            c = (tube.min(0) + tube.max(0)) * 0.5
            c[2] = pod[:, 2].max()
            out.append('%s tusk %d part:%s %s 0 0 1 0 1 0' % (name, k, pname, fmt(c)))
            print('%s tusk %d: %s (%d face vertices)' % (name, k, fmt(c), len(tube)))
            k += 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--models', default='game/models3d')
    ap.add_argument('--out', default='game/vfx3d/sockets.txt')
    a = ap.parse_args()
    out = ['# written by tools/art/unity_vfx/sockets.py from the models themselves; do not edit',
           '# MODEL SET INDEX SPACE px py pz fx fy fz ux uy uz']
    for name in ('E1', 'E1TR', 'E3', 'HTNK'):
        path = os.path.join(a.models, name + '.c3m')
        if not os.path.exists(path):
            continue
        m = read_c3m(path)
        (soldier if m['bones'] else vehicle)(name, m, out)
    open(a.out, 'w').write('\n'.join(out) + '\n')
    print('%d sockets -> %s' % (len(out) - 2, a.out))


if __name__ == '__main__':
    main()
