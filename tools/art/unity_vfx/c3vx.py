"""Reader for the recordings OpenCNC3DVfxBake.cs writes (seed<k>.bin, mesh_*.bin, meta.json)."""
import json, os, struct
import numpy as np

FIELDS = ['seed', 'px', 'py', 'pz', 'rx', 'ry', 'rz', 'sx', 'sy', 'sz', 'r', 'g', 'b', 'a',
          'vx', 'vy', 'vz', 'life', 'start', 'c1x', 'c1y', 'c1z', 'c1w', 'c2x', 'c2y', 'c2z',
          'c2w', 'tu', 'tv', 'mesh']

def load_meta(d):
    return json.load(open(os.path.join(d, 'meta.json')))

def load_seed(path):
    """-> (fps, frames) where frames[f][s] is an (n, 30) float32 array."""
    buf = open(path, 'rb').read()
    assert buf[:4] == b'C3VX', path
    ver, nsys, nfr, fps, nf = struct.unpack_from('<5I', buf, 4)
    off = 24
    frames = []
    for f in range(nfr):
        (ns,) = struct.unpack_from('<I', buf, off); off += 4
        assert ns == nsys
        sy = []
        for s in range(ns):
            (n,) = struct.unpack_from('<I', buf, off); off += 4
            a = np.frombuffer(buf, '<f4', n * nf, off).reshape(n, nf); off += 4 * n * nf
            sy.append(a)
        frames.append(sy)
    assert off == len(buf)
    return fps, frames

def load_mesh(path):
    buf = open(path, 'rb').read()
    assert buf[:4] == b'C3VM'
    ver, vc, ic = struct.unpack_from('<3I', buf, 4)
    v = np.frombuffer(buf, '<f4', vc * 16, 16).reshape(vc, 16)
    i = np.frombuffer(buf, '<u4', ic, 16 + vc * 64)
    return v, i

def load_probe(path):
    buf = open(path, 'rb').read()
    assert buf[:4] == b'C3VP'
    vc, ic = struct.unpack_from('<2I', buf, 4)
    v = np.frombuffer(buf, '<f4', vc * 26, 12).reshape(vc, 26)
    i = np.frombuffer(buf, '<u4', ic, 12 + vc * 104)
    return v, i
