"""Read a .blend file without Blender: objects, their transforms and their mesh geometry.

    python3 tools/art/blendread.py FILE.blend            # one line per object

Plain Python and numpy. It understands the SDNA the file carries about itself, so it does
not hard-code struct layouts; it only knows the field NAMES it asks for. Both header forms
are read: the classic 12-byte header ("BLENDER-v402", 24-byte block heads on 64-bit) and
the Blender 5 one ("BLENDER17-01v0502", 32-byte block heads with 64-bit lengths). A
zstd-compressed file is unpacked with the `zstd` command.

What it returns is deliberately small: per object its name, parent, local and world
matrices, and for a mesh its vertex positions (object space) and its polygons as vertex
index loops. That is enough to say which piece of a model is which and where it sits,
which is the question a rig asks. It does not read modifiers, custom normals, UVs or
materials; an exporter that needs those runs inside Blender.
"""
import os
import struct
import subprocess
import sys

import numpy as np


class Blend:
    def __init__(self, path):
        raw = open(path, "rb").read()
        if raw[:4] == b"\x28\xb5\x2f\xfd":
            raw = subprocess.run(["zstd", "-d", "-q", "-c", path], check=True,
                                 capture_output=True).stdout
        if not raw.startswith(b"BLENDER"):
            raise SystemExit("%s: not a .blend file" % path)
        if raw[7:9].isdigit():                       # Blender 5: BLENDER17-01v0502
            hsize = int(raw[7:9])
            self.version = raw[hsize - 4:hsize].decode()
            self.bhead = 32
        else:                                        # BLENDER-v402
            hsize = 12
            if raw[7:8] != b"-" or raw[8:9] != b"v":
                raise SystemExit("%s: only 64-bit little-endian files are read" % path)
            self.version = raw[9:12].decode()
            self.bhead = 24
        self.raw = raw
        self.blocks = []
        self.by_addr = {}
        # Blender 5 numbers a data block's address per ID, so two meshes can both have a
        # vertex array at the same "address". A pointer is looked up among the blocks that
        # follow its own ID block first, and only then file-wide.
        self.scoped = {}
        scope = None
        o = hsize
        while o < len(raw):
            code = raw[o:o + 4]
            if self.bhead == 32:
                sdna, old, ln, nr = struct.unpack_from("<iQqq", raw, o + 4)
            else:
                ln, old, sdna, nr = struct.unpack_from("<iQii", raw, o + 4)
            data = o + self.bhead
            blk = (code, sdna, old, ln, nr, data)
            self.blocks.append(blk)
            if code[2:] == b"\0\0" and code[:2].isalpha() and code[:2].isupper():
                scope = self.scoped.setdefault(old, {})
            if old:
                self.by_addr.setdefault(old, blk)
                if scope is not None:
                    scope.setdefault(old, blk)
            if code == b"DNA1":
                self._sdna(data)
            if code == b"ENDB":
                break
            o = data + ln

    # ---- SDNA ----------------------------------------------------------------------------
    def _sdna(self, o):
        r = self.raw
        assert r[o:o + 4] == b"SDNA"
        start = o
        o += 4

        def align(o):
            # aligned to four from the start of the SDNA, not of the file
            return start + ((o - start + 3) & ~3)

        def strings(o, n):
            out = []
            for _ in range(n):
                e = r.index(b"\0", o)
                out.append(r[o:e].decode("latin-1"))
                o = e + 1
            return out, o

        assert r[o:o + 4] == b"NAME"
        n = struct.unpack_from("<i", r, o + 4)[0]
        self.names, o = strings(o + 8, n)
        o = align(o)
        assert r[o:o + 4] == b"TYPE"
        n = struct.unpack_from("<i", r, o + 4)[0]
        self.types, o = strings(o + 8, n)
        o = align(o)
        assert r[o:o + 4] == b"TLEN"
        self.tlen = list(struct.unpack_from("<%dh" % n, r, o + 4))
        o = align(o + 4 + 2 * n)
        assert r[o:o + 4] == b"STRC"
        n = struct.unpack_from("<i", r, o + 4)[0]
        o += 8
        self.structs = []
        self.struct_of = {}
        for i in range(n):
            t, nf = struct.unpack_from("<hh", r, o)
            o += 4
            fields = []
            off = 0
            for _ in range(nf):
                ft, fname = struct.unpack_from("<hh", r, o)
                o += 4
                name = self.names[fname]
                size = self._field_size(ft, name)
                fields.append((self.types[ft], name, off, size))
                off += size
            self.structs.append((self.types[t], fields))
            self.struct_of[self.types[t]] = i

    def _field_size(self, ft, name):
        mult = 1
        base = name
        if "[" in name:
            base = name[:name.index("[")]
            for part in name[name.index("["):].split("]")[:-1]:
                mult *= int(part.strip("["))
        if base.startswith("*") or base.startswith("(*"):
            return 8 * mult
        return self.tlen[ft] * mult

    def field(self, sname, fname):
        """(type, offset, size, full name) of a field; fname is the bare name, no * or []."""
        for t, n, off, size in self.structs[self.struct_of[sname]][1]:
            bare = n.lstrip("*").split("[")[0]
            if bare == fname:
                return t, off, size, n
        raise KeyError("%s.%s" % (sname, fname))

    def has(self, sname, fname):
        try:
            self.field(sname, fname)
            return True
        except KeyError:
            return False

    def get(self, base, sname, fname, fmt=None):
        t, off, size, n = self.field(sname, fname)
        if n.startswith("*"):
            return struct.unpack_from("<Q", self.raw, base + off)[0]
        if fmt:
            return struct.unpack_from("<" + fmt, self.raw, base + off)
        if t == "char":
            s = self.raw[base + off:base + off + size]
            return s.split(b"\0", 1)[0].decode("utf-8", "replace")
        code = {"int": "i", "short": "h", "float": "f", "double": "d", "int64_t": "q",
                "uint64_t": "Q", "char": "b", "uchar": "B", "int8_t": "b",
                "uint8_t": "B", "int32_t": "i", "uint32_t": "I", "int16_t": "h",
                "uint16_t": "H"}[t]
        cnt = size // struct.calcsize(code)
        v = struct.unpack_from("<%d%s" % (cnt, code), self.raw, base + off)
        return v[0] if cnt == 1 else v

    def block(self, addr):
        if self._scope is not None and addr in self._scope:
            return self._scope[addr]
        return self.by_addr.get(addr)

    _scope = None

    def within(self, id_addr):
        """Resolve pointers inside the data of the ID block at id_addr."""
        self._scope = self.scoped.get(id_addr)

    def struct_name(self, blk):
        return self.structs[blk[1]][0]

    # ---- objects and meshes --------------------------------------------------------------
    def id_name(self, base, sname):
        return self.get(base + self.field(sname, "id")[1], "ID", "name")[2:]

    def objects(self):
        """Every object, with its world matrix computed the way Blender does it (the file
        keeps only the channels: location, rotation, scale, their deltas, the parent and
        the parent inverse)."""
        out = []
        for blk in self.blocks:
            if blk[0] != b"OB\0\0":
                continue
            b = blk[5]
            g = lambda f, fmt=None: self.get(b, "Object", f, fmt)
            ob = {"addr": blk[2], "name": self.id_name(b, "Object"), "type": g("type"),
                  "parent": g("parent"), "partype": g("partype"), "data": g("data"),
                  "parentinv": np.array(g("parentinv", "16f"), float).reshape(4, 4).T}
            loc = np.add(g("loc", "3f"), g("dloc", "3f"))
            size = np.multiply(g("size", "3f"), g("dscale", "3f"))
            mode = g("rotmode")
            if mode == 0:
                R = quat_mat(g("quat", "4f")) @ quat_mat(g("dquat", "4f"))
            elif mode == -1:
                R = axis_angle_mat(g("rotAxis", "3f"), g("rotAngle"))
                R = R @ axis_angle_mat(g("drotAxis", "3f"), g("drotAngle"))
            else:
                R = euler_mat(g("rot", "3f"), mode) @ euler_mat(g("drot", "3f"), mode)
            L = np.eye(4)
            L[:3, :3] = R * np.asarray(size)[None, :]
            L[:3, 3] = loc
            ob["local"] = L
            ob["loc"], ob["size"], ob["rotmode"] = tuple(loc), tuple(size), mode
            out.append(ob)
        by = {o["addr"]: o for o in out}

        def world(o):
            if "world" not in o:
                p = by.get(o["parent"])
                if p is None:
                    o["world"] = o["local"]
                else:
                    if o["partype"] not in (0, 1):   # PAROBJECT; bones and vertices are not read
                        raise SystemExit("%s: parent type %d not read" % (o["name"], o["partype"]))
                    o["world"] = world(p) @ o["parentinv"] @ o["local"]
            return o["world"]

        for o in out:
            world(o)
        for o in out:
            p = by.get(o["parent"])
            o["parent"] = p["name"] if p else None
        return out

    def _array(self, addr):
        blk = self.block(addr)
        if not blk:
            return None
        return self.raw[blk[5]:blk[5] + blk[3]]

    def mesh(self, addr):
        """(name, positions Nx3, polygons as lists of vertex indices, material index per
        polygon or None)."""
        self.within(None)
        blk = self.block(addr)
        self.within(addr)
        b = blk[5]
        name = self.id_name(b, "Mesh")
        nv = self.get(b, "Mesh", "totvert")
        npoly = self.get(b, "Mesh", "totpoly")
        nloop = self.get(b, "Mesh", "totloop")
        attrs = self._attributes(b, "vdata", "Mesh")
        pos = attrs.get("position")
        if pos is None:
            raise SystemExit("mesh %s: no position attribute found" % name)
        pos = np.frombuffer(pos, np.float32, nv * 3).reshape(nv, 3).astype(float)
        cattrs = self._attributes(b, "ldata", "Mesh")
        cv = np.frombuffer(cattrs[".corner_vert"], np.int32, nloop)
        offs = self._array(self.get(b, "Mesh", "poly_offset_indices"))
        offs = np.frombuffer(offs, np.int32, npoly + 1)
        polys = [cv[offs[i]:offs[i + 1]].tolist() for i in range(npoly)]
        fattrs = self._attributes(b, "pdata", "Mesh")
        mat = fattrs.get("material_index")
        if mat is not None:
            mat = np.frombuffer(mat, np.int32, npoly)
        return name, pos, polys, mat

    def _attributes(self, base, fname, sname):
        """Named data arrays of one domain. Blender 5 keeps generic attributes in an
        AttributeStorage; older files and the internal arrays (".corner_vert") sit in
        CustomData layers. Both are read and merged."""
        out = {}
        t, off, size, n = self.field(sname, fname)
        cd = base + off
        layers = self.get(cd, "CustomData", "layers")
        totlayer = self.get(cd, "CustomData", "totlayer")
        blk = self.block(layers)
        if blk and totlayer:
            lsz = self.tlen[self.types.index("CustomDataLayer")]
            for i in range(totlayer):
                lb = blk[5] + i * lsz
                nm = self.get(lb, "CustomDataLayer", "name")
                data = self._array(self.get(lb, "CustomDataLayer", "data"))
                if data is not None:
                    out[nm] = data
        if self.has(sname, "attribute_storage"):
            t, off, size, n = self.field(sname, "attribute_storage")
            st = base + off
            arr = self.get(st, "AttributeStorage", "dna_attributes")
            num = self.get(st, "AttributeStorage", "dna_attributes_num")
            blk = self.block(arr)
            want = {"vdata": 0, "edata": 1, "pdata": 2, "ldata": 3}[fname]
            if blk and num:
                asz = self.tlen[self.types.index("Attribute")]
                for i in range(num):
                    ab = blk[5] + i * asz
                    if self.get(ab, "Attribute", "domain") != want:
                        continue
                    nm = self._cstr(self.get(ab, "Attribute", "name"))
                    ad = self.block(self.get(ab, "Attribute", "data"))
                    if not ad:
                        continue
                    # AttributeArray { void *data; int64 size; ... }
                    dp = self.get(ad[5], "AttributeArray", "data")
                    data = self._array(dp)
                    if data is not None:
                        out[nm] = data
        return out

    def _cstr(self, addr):
        d = self._array(addr)
        return d.split(b"\0", 1)[0].decode("utf-8", "replace") if d else ""


def quat_mat(q):
    w, x, y, z = np.asarray(q, float) / (np.linalg.norm(q) or 1.0)
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
                     [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
                     [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]])


def axis_angle_mat(axis, ang):
    a = np.asarray(axis, float)
    n = np.linalg.norm(a)
    if n == 0:
        return np.eye(3)
    a = a / n
    return quat_mat([np.cos(ang / 2)] + list(a * np.sin(ang / 2)))


def euler_mat(e, mode):
    """Blender's euler orders, 1..6 = XYZ XZY YXZ YZX ZXY ZYX; the named axis order is the
    order the rotations are APPLIED, so XYZ is Rz @ Ry @ Rx."""
    c = [np.cos(v) for v in e]
    s = [np.sin(v) for v in e]
    R = {"X": np.array([[1, 0, 0], [0, c[0], -s[0]], [0, s[0], c[0]]]),
         "Y": np.array([[c[1], 0, s[1]], [0, 1, 0], [-s[1], 0, c[1]]]),
         "Z": np.array([[c[2], -s[2], 0], [s[2], c[2], 0], [0, 0, 1]])}
    order = {1: "XYZ", 2: "XZY", 3: "YXZ", 4: "YZX", 5: "ZXY", 6: "ZYX"}[mode]
    M = np.eye(3)
    for ax in order:
        M = R[ax] @ M
    return M


def main(argv):
    b = Blend(argv[0])
    print("BLEND|version %s|%d blocks" % (b.version, len(b.blocks)))
    for o in b.objects():
        w = o["world"]
        line = "OB|%s|type %d|parent %s|origin %.3f %.3f %.3f" % (
            o["name"], o["type"], o["parent"], w[0, 3], w[1, 3], w[2, 3])
        if o["type"] == 1 and o["data"]:
            nm, pos, polys, mat = b.mesh(o["data"])
            wp = pos @ w[:3, :3].T + w[:3, 3]
            line += "|%d verts %d polys|min %.3f %.3f %.3f|max %.3f %.3f %.3f" % (
                (len(pos), len(polys)) + tuple(wp.min(0)) + tuple(wp.max(0)))
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
