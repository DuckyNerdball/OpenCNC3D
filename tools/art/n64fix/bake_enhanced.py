#!/usr/bin/env python3
"""Bake every APPROVED fixed cartridge model into the file the renderer draws in Enhanced.

    python3 tools/art/n64fix/bake_enhanced.py [--pack P] [--models DIR] [--out DIR] [TYPE ...]

Reads reviews.json beside this script and, for each type whose review is
state "approved" with "enhanced": "replace", reads the viewer export of the fixed model
(viewer/models/<TYPE>/new.json and its 4x textures) and writes ONE binary per type,
<out>/<TYPE>.n64x, plus <out>/manifest.json saying where each one came from.

THE OUTPUT IS COMMITTED ART, like game/models3d/*.c3m and for the same reason: the
viewer export is made by Blender (export_new.py), no build machine has Blender, and
the export folder is not tracked. So the product is tracked and a build copies it.
Re-run this after a model is approved or re-approved, and commit what it writes.

WHAT THE RENDERER DOES WITH IT. In Enhanced only, a finished and healthy building of
an approved type draws this mesh through the same pack-mesh path as the cartridge's
own model: same facing, ground, shroud, house colour, parts, node animation, tint and
flash. The file therefore keeps the cartridge's rendering model exactly: texture times
baked vertex colour, triangles grouped by the cartridge's own parts, positions already
posed the way the pack's are, so the pack's per-part animation deltas and pivots apply
to it unchanged. The pivots, roles, clips and the ground-shadow faces are NOT stored
here: they are read from the mission pack's cartridge model at load, so there is one
copy of each and the fixed model cannot disagree with the cartridge about them.

THE EXPORT'S VERTEX COLOURS ARE LINEAR (Blender decodes the colour attribute from
sRGB on the way out), so they are encoded back to sRGB bytes here; see srgb8.

THREE FACTS THE VIEWER EXPORT DROPS are restored from the cartridge pack (--pack):
  - WRAP. The cartridge clamps or mirrors some textures per face, and a face whose
    UVs leave 0..1 draws a different part of the texture under repeat. The Airfield
    shows it at once: its tower and its runway emblem share one mirrored and clamped
    sheet, and under repeat the emblem splits in two and the tower turns the colour of
    the far edge of the sheet. Each group takes the wrap the cartridge gives its
    texture on the same part (else anywhere in the model), when every such face
    agrees; otherwise repeat, and the bake says so.
  - VERTEX ALPHA. The export carries RGB only. The cartridge's translucent faces (the
    power plants' coolant pools) carry alpha 101, and alpha 255 would draw them solid.
    Each group takes the one alpha its part's cartridge faces of the same draw mode
    share; 255 where they do not agree or there are none.
  - EXACT POSITIONS. The export rounds to 1e-5 of a cell, 0.01 mesh units. A vertex
    within 0.02 units of a cartridge vertex of the same part is snapped back onto it,
    so geometry the fix kept is bit-identical to the cartridge's. That matters where
    a second draw lands on the same triangles (the Construction Yard's fan book is
    drawn over its vent under a less-or-equal depth test).
The side-by-side viewer draws both models with repeat on every texture, so a face the
cartridge mirrors or clamps reads differently there from the game on either side.

THE TEXTURE BOOKS are restored the same way. Six structures animate by swapping one
texture for another over time (the Construction Yard's fans, the Refinery's warning
light and storage strip, the Helipad's lights, the Repair Bay's plates, the Silo's fill
readout). In the pack a book is a second mesh per time slot, <TYPE>T0.., <TYPE>U0..,
drawn over the base model's own copy of those triangles. That only works while the base
model IS the cartridge's: the repair moved or rebuilt most of those triangles, so a
cartridge book drawn over a fixed model either lands on nothing or paints the
cartridge's shape back over the repair. Measured on the six books against the
approved models: the Helipad's 6 animated triangles, the Repair Bay's 32 and the Silo's
24 land on no triangle of the fixed mesh at all.
Every one of the six is a PURE TEXTURE SWAP, checked here rather than assumed: one
texture A, used by no triangle outside the book, is replaced per slot by a texture of
the same size with the UVs unchanged. So the book is carried over to the fixed model as
exactly that: "the fixed triangles textured from A take texture B_k in slot k". Each
B_k is raised 4x by upscale.py, the function the fixed textures themselves came from
(on the six A textures its output is byte-identical to the export's), so an animated
surface is no blurrier than the one beside it. A book that is not a pure swap, or whose
texture A the fixed model no longer uses, is left out with a warning, and the renderer
then shows that surface at its first slot, unanimated.

FORMAT, version 2, all little endian:

    char[4]  magic "N64X"
    u32      version = 2
    char[8]  type code, NUL padded
    u32      nparts    part count of the cartridge model this was made for
    u32      ntex
    u32      ngroup
    u32      ntris     total over all groups
    then ntex textures:
        u16 w, u16 h            powers of two, at most 256 a side
        u8  flags               bit 0: a separate GDI decode follows
        u8  pad[3]
        u8  base[w*h*4]         RGBA, the Nod / neutral decode (the pack's `gl`)
        u8  gdi[w*h*4]          RGBA, only when flags bit 0 (the pack's `gl_gdi`)
    then ngroup groups, in ascending part order:
        i16 part                cartridge part index, 0 .. nparts-1
        i16 tex                 index into the textures above, -1 = untextured
        u8  mode                0 opaque, 1 cutout, 3 translucent (the pack's TriMode)
        u8  wrap                the pack's wrap byte: bits 0-1 S, bits 2-3 T, each
                                0 repeat, 1 mirror, 2 clamp; the top nibble is 0
        u16 pad
        u32 ntri
        ntri * 3 vertices:
            f32 x, y, z         pack units: x east, y up, z south, 1024 to the cell
            f32 u, v            pack convention, 0..1 over the whole texture, v down
            u8  r, g, b, a      baked colour and alpha
    u32      nbook              (version 2 on)
    then nbook books:
        u8  letter              'T', 'U', 'V' or 'W', as in the pack's <TYPE><letter><slot>
        u8  pad
        i16 from                the texture above that the book replaces
        u32 nslot               slots 0 .. nslot-1, the pack's own count for this book
        i16 tex[nslot]          the texture each slot draws in place of `from`; equal
                                to `from` where the slot shows the unanimated image

Needs Pillow and numpy (the system Python, not Blender's).
"""
import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "bakery"))
import packinspect as PI  # noqa: E402
sys.path.insert(0, HERE)
import upscale as UP  # noqa: E402

VERSION = 2
BOOK_LETTERS = "TUVW"
MODE = {"opaque": 0, "cutout": 1, "xlu": 3}
SNAP = 0.02            # mesh units; the export's rounding is at most 0.0052


def rel(p):
    return os.path.relpath(p, ROOT)


def git(*args):
    try:
        return subprocess.check_output(["git", "-C", ROOT] + list(args),
                                       stderr=subprocess.DEVNULL).decode().strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def group_mode(g):
    if g.get("xlu") or g["mat"].endswith("_xlu"):
        return "xlu"
    if g.get("cut") or g["mat"].endswith("_cutout"):
        return "cutout"
    return "opaque"


def cartridge(pack, code):
    """The cartridge model's parts, its vertex positions per part and the vertex alpha
    per (part, mode), straight out of the pack the fix was extracted from."""
    idx = [m for c, m, f in pack["types"] if c == code]
    if not idx:
        raise SystemExit("bake_enhanced: %s has no model in the pack" % code)
    mesh = pack["meshes"][idx[0]]
    parts = mesh["parts"]
    pos = [[] for _ in parts]
    alpha = {}
    wrap = {}
    for i in range(mesh["ntris"]):
        f = struct.unpack_from(PI.TRI_FMT, mesh["tris"], i * PI.TRI_STRIDE)
        mode = f[1]
        pi = next(k for k, p in enumerate(parts) if p[0] <= i < p[0] + p[1])
        for k in range(3):
            v = f[3 + k * 9:12 + k * 9]
            pos[pi].append(v[0:3])
            alpha.setdefault((pi, mode), set()).add(v[8])
        if f[0] >= 0 and mode != 2:
            wrap.setdefault((pi, f[0]), set()).add(f[2])
            wrap.setdefault((None, f[0]), set()).add(f[2])
    return len(parts), pos, alpha, wrap


def mat_tex(mat):
    """The cartridge texture a group's material was made from. The material is named
    <TYPE>_t<index>_<mode>, the index being the extraction pack's; -1 is untextured.
    None when the name does not say."""
    try:
        return int(mat.split("_t")[1].split("_")[0])
    except (IndexError, ValueError):
        return None


def cart_wrap(cwrap, part, mat):
    """The wrap byte the cartridge gives this texture: on this part if the part uses it,
    else anywhere in the model, and only when every such face agrees. None when there
    is no single answer."""
    tex = mat_tex(mat)
    if tex is None:
        return None
    for key in ((part, tex), (None, tex)):
        w = cwrap.get(key)
        if w:
            return next(iter(w)) if len(w) == 1 else None
    return None


def srgb8(x):
    """The export's colours are LINEAR: Blender hands a colour attribute back decoded
    from sRGB, so the cartridge's byte 255 * 0.5 arrives as 0.214. Encoding it again is
    exact: on the unmodified cartridge model (old.json) every corner of every matched
    triangle comes back to the pack's own byte. Written as bytes without it, every
    fixed building drew about 56 levels darker than the cartridge's."""
    x = max(0.0, min(1.0, x))
    s = 12.92 * x if x <= 0.0031308 else 1.055 * x ** (1.0 / 2.4) - 0.055
    return max(0, min(255, int(round(s * 255.0))))


def pack_tris(mesh):
    """(tex, mode, uv triple, position triple) per triangle, in list order."""
    out = []
    for i in range(mesh["ntris"]):
        f = struct.unpack_from(PI.TRI_FMT, mesh["tris"], i * PI.TRI_STRIDE)
        vs = [f[3 + k * 9:12 + k * 9] for k in range(3)]
        out.append((f[0], f[1], tuple(tuple(v[3:5]) for v in vs),
                    tuple(tuple(v[0:3]) for v in vs)))
    return out


def cartridge_books(pack, code):
    """This type's texture books in the pack, as {letter: [slot mesh index, ...]}, with
    the slots counted the way the renderer counts them: from 0 up to the first gap."""
    types = {c: m for c, m, f in pack["types"]}
    books = {}
    for letter in BOOK_LETTERS:
        slots = []
        while "%s%s%d" % (code, letter, len(slots)) in types:
            slots.append(types["%s%s%d" % (code, letter, len(slots))])
        if slots:
            books[letter] = slots
    return books


def book_swap(pack, code, letter, slots):
    """The book as a pure texture swap: (A, [B per slot]), or (None, reason). A is the
    one texture the book replaces and B the texture each slot shows in its place."""
    base = pack_tris(pack["meshes"][[m for c, m, f in pack["types"] if c == code][0]])
    V = [pack_tris(pack["meshes"][m]) for m in slots]
    n = len(V[0])
    if any(len(v) != n for v in V):
        return None, "its slots differ in triangle count"
    anim = [i for i in range(n) if len(set((v[i][0], v[i][2]) for v in V)) > 1]
    if not anim:
        return None, "no slot differs from the first"
    if any(len(set(v[i][2] for v in V)) > 1 for i in anim):
        return None, "a slot changes UVs as well as the texture"
    A = set(V[0][i][0] for i in anim)
    if len(A) != 1 or -1 in A:
        return None, "its first slot uses %d textures on the animated faces" % len(A)
    A = A.pop()
    Bs = []
    for v in V:
        b = set(v[i][0] for i in anim)
        if len(b) != 1:
            return None, "a slot swaps texture A for more than one texture"
        B = b.pop()
        if B < 0 or (pack["tex"][B]["w"], pack["tex"][B]["h"]) != (pack["tex"][A]["w"],
                                                                   pack["tex"][A]["h"]):
            return None, "slot texture %d is not the size of texture %d" % (B, A)
        Bs.append(B)
    # A must be the book's alone: a static face of the base model textured from A
    # would start animating when every A face of the fixed model swaps.
    anim_pos = set(V[0][i][3] for i in anim)
    static = sum(1 for t in base if t[0] == A and t[3] not in anim_pos)
    if static:
        return None, "texture %d is also on %d faces the book does not animate" % (A, static)
    return A, Bs


def upscaled(tx):
    """A pack texture raised exactly as the fixed textures were: upscale.py's default."""
    out = []
    for data in (tx["data"], tx["gdi"] or tx["data"]):
        im = Image.frombytes("RGBA", (tx["w"], tx["h"]), data)
        out.append(UP.upscale(im).tobytes())
    w, h = tx["w"] * 4, tx["h"] * 4
    if w > 256 or h > 256:
        raise SystemExit("bake_enhanced: a book texture raised 4x is %dx%d, over 256" % (w, h))
    return w, h, out[0], out[1]


def load_rgba(path):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    for n in (w, h):
        if n & (n - 1) or n > 256:
            raise SystemExit("bake_enhanced: %s is %dx%d; textures must be powers of two "
                             "and at most 256 a side" % (path, w, h))
    return w, h, im.tobytes()


def bake(code, view, pack, out_dir):
    doc = json.load(open(os.path.join(view, "new.json")))
    nparts, cpos, calpha, cwrap = cartridge(pack, code)
    wrap_note = []

    texs, tex_of = [], {}
    groups = []
    snapped = total_v = 0
    for g in doc["groups"]:
        part = int(g["part"])
        if not 0 <= part < nparts:
            raise SystemExit("bake_enhanced: %s group %s names part %d; the cartridge "
                             "model has %d" % (code, g["mat"], part, nparts))
        mode = group_mode(g)
        ti = -1
        if g.get("tex"):
            key = (g["tex"]["nod"], g["tex"]["gdi"])
            if key not in tex_of:
                w, h, base = load_rgba(os.path.join(view, key[0]))
                w2, h2, gdi = load_rgba(os.path.join(view, key[1]))
                if (w, h) != (w2, h2):
                    raise SystemExit("bake_enhanced: %s: the two decodes of %s differ in "
                                     "size" % (code, key[0]))
                tex_of[key] = len(texs)
                texs.append((w, h, base, gdi if gdi != base else None))
            ti = tex_of[key]
        al = calpha.get((part, MODE[mode]), set())
        a = al.pop() if len(al) == 1 else 255
        p, uv, c = g["p"], g["uv"], g["c"]
        nv = len(p) // 3
        if nv % 3 or len(uv) != nv * 2 or len(c) != nv * 3:
            raise SystemExit("bake_enhanced: %s group %s has ragged arrays" % (code, g["mat"]))
        verts = []
        for i in range(nv):
            x, y, z = 1024.0 * p[i * 3], 1024.0 * p[i * 3 + 2], -1024.0 * p[i * 3 + 1]
            best, bd = None, SNAP
            for q in cpos[part]:
                d = max(abs(q[0] - x), abs(q[1] - y), abs(q[2] - z))
                if d <= bd:
                    best, bd = q, d
            if best is not None:
                x, y, z = best
                snapped += 1
            total_v += 1
            col = [srgb8(v) for v in c[i * 3:i * 3 + 3]]
            verts.append((x, y, z, uv[i * 2], 1.0 - uv[i * 2 + 1], col[0], col[1], col[2], a))
        wrap = 0
        if ti >= 0:
            wrap = cart_wrap(cwrap, part, g["mat"])
            if wrap is None:
                wrap_note.append(g["mat"])
                wrap = 0
        groups.append(dict(part=part, tex=ti, mode=MODE[mode], wrap=wrap, verts=verts,
                           mat=g["mat"]))
    groups.sort(key=lambda g: g["part"])          # stable: export order within a part

    # THE TEXTURE BOOKS, carried over as texture swaps on the fixed mesh (see the top
    # of this file). from_cart: cartridge texture -> the fixed texture made from it.
    from_cart = {}
    for g in groups:
        if g["tex"] >= 0:
            from_cart.setdefault(mat_tex(g["mat"]), set()).add(g["tex"])
    books, book_note = [], []
    for letter, slots in sorted(cartridge_books(pack, code).items()):
        A, Bs = book_swap(pack, code, letter, slots)
        if A is None:
            book_note.append("book %s left out: %s" % (letter, Bs))
            continue
        fr = from_cart.get(A, set())
        if len(fr) != 1:
            book_note.append("book %s left out: the fixed model draws texture %d with %d "
                             "textures, not one" % (letter, A, len(fr)))
            continue
        fr = fr.pop()
        raised = {}
        slot_tex = []
        for B in Bs:
            if B == A:
                slot_tex.append(fr)
                continue
            if B not in raised:
                w, h, base, gdi = upscaled(pack["tex"][B])
                if (w, h) != texs[fr][:2]:
                    raise SystemExit("bake_enhanced: %s book %s: texture %d raised is %dx%d, "
                                     "the fixed texture it replaces is %dx%d"
                                     % ((code, letter, B, w, h) + texs[fr][:2]))
                raised[B] = len(texs)
                texs.append((w, h, base, gdi if gdi != base else None))
            slot_tex.append(raised[B])
        books.append((letter, fr, slot_tex))

    ntris = sum(len(g["verts"]) // 3 for g in groups)
    blob = bytearray()
    blob += b"N64X" + struct.pack("<I", VERSION) + code.encode("ascii").ljust(8, b"\0")[:8]
    blob += struct.pack("<4I", nparts, len(texs), len(groups), ntris)
    for w, h, base, gdi in texs:
        blob += struct.pack("<HHB3x", w, h, 1 if gdi else 0) + base + (gdi or b"")
    for g in groups:
        blob += struct.pack("<hhBBHI", g["part"], g["tex"], g["mode"], g["wrap"], 0,
                            len(g["verts"]) // 3)
        for v in g["verts"]:
            blob += struct.pack("<5f4B", *v)
    blob += struct.pack("<I", len(books))
    for letter, fr, slot_tex in books:
        blob += struct.pack("<BBhI", ord(letter), 0, fr, len(slot_tex))
        blob += struct.pack("<%dh" % len(slot_tex), *slot_tex)
    path = os.path.join(out_dir, code + ".n64x")
    with open(path, "wb") as f:
        f.write(blob)

    fix = os.path.join(HERE, "fix_%s.py" % code)
    info = dict(
        file=os.path.basename(path), bytes=len(blob), sha256=hashlib.sha256(blob).hexdigest(),
        triangles=ntris, groups=len(groups), textures=len(texs),
        house_textures=sum(1 for t in texs if t[3]), parts=nparts,
        vertices_snapped=snapped, vertices=total_v,
        wraps={"%s/part%d" % (g["mat"], g["part"]): g["wrap"] for g in groups if g["wrap"]},
        wrap_unresolved=wrap_note,
        translucent_alpha=sorted({v[8] for g in groups if g["mode"] == 3 for v in g["verts"]}),
        books={letter: dict(replaces=fr, slots=len(st), images=len(set(st)))
               for letter, fr, st in books},
        books_left_out=book_note,
        fix_script=rel(fix),
        fix_commit=git("log", "-1", "--format=%H", "--", rel(fix)),
        fix_script_modified=bool(git("status", "--porcelain", "--", rel(fix))),
        export_sha256=sha(os.path.join(view, "new.json")),
        export_triangles=doc.get("triangles"))
    print("%s: %d triangles in %d groups, %d textures (%d with a GDI decode), %d parts, "
          "%d of %d vertices snapped to the cartridge, %.1f KB"
          % (code, ntris, len(groups), len(texs), info["house_textures"], nparts,
             snapped, total_v, len(blob) / 1024.0))
    for letter, fr, st in books:
        print("  book %s: %d slots, %d images, as a swap of texture %d"
              % (letter, len(st), len(set(st)), fr))
    for note in book_note:
        print("  WARNING: %s; that surface shows its first image, unanimated" % note)
    if wrap_note:
        print("  WARNING: no single cartridge wrap for %s; written as repeat"
              % ", ".join(wrap_note))
    if info["fix_script_modified"] or not info["fix_commit"]:
        print("  WARNING: %s is not committed as it stands; the manifest says so"
              % info["fix_script"])
    return info


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--pack", default=os.environ.get(
        "N64FIX_PACK", os.path.join(ROOT, "tools", "bakery", "game", "SCG01EA.pack")),
        help="the cartridge pack the fixes were extracted from")
    ap.add_argument("--models", default=os.path.join(HERE, "viewer", "models"),
                    help="the viewer's model folders (export_new.py writes them)")
    ap.add_argument("--out", default=os.path.join(ROOT, "game", "n64fix"))
    ap.add_argument("types", nargs="*")
    a = ap.parse_args()

    reviews = json.load(open(os.path.join(HERE, "reviews.json")))
    approved = sorted(t for t, r in reviews.items()
                      if r.get("state") == "approved" and r.get("enhanced") == "replace")
    todo = a.types or approved
    for t in todo:
        if t not in approved:
            raise SystemExit("bake_enhanced: %s is not approved for Enhanced in reviews.json" % t)
    pack = PI.read(a.pack)
    os.makedirs(a.out, exist_ok=True)

    mpath = os.path.join(a.out, "manifest.json")
    manifest = json.load(open(mpath)) if os.path.exists(mpath) else {}
    models = manifest.get("models", {})
    for t in todo:
        view = os.path.join(a.models, t)
        if not os.path.exists(os.path.join(view, "new.json")):
            raise SystemExit("bake_enhanced: no viewer export for %s in %s; run "
                             "tools/art/n64fix/run.sh %s first" % (t, view, t))
        models[t] = bake(t, view, pack, a.out)
    # a model no longer approved is taken out of the set, file and all
    for t in list(models):
        if t not in approved:
            p = os.path.join(a.out, t + ".n64x")
            if os.path.exists(p):
                os.remove(p)
            del models[t]
            print("%s: no longer approved, removed" % t)
    manifest = dict(
        format="N64X version %d; layout in tools/art/n64fix/bake_enhanced.py" % VERSION,
        what="fixed cartridge building models, drawn in Enhanced in place of the "
             "cartridge's for approved types; made by tools/art/n64fix/bake_enhanced.py",
        cartridge_pack=os.path.basename(a.pack),
        models=dict(sorted(models.items())))
    with open(mpath, "w") as f:
        json.dump(manifest, f, indent=1, sort_keys=False)
        f.write("\n")
    print("manifest: %s (%d models)" % (rel(mpath), len(models)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
