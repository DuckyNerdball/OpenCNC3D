#!/usr/bin/env python3
"""The green pointer (CUR00, approved first mesh InitialLow01) as a C header the game
compiles in.

    python3 tools/art/cursor_green.py            writes game/cursor_green_mesh.h
    python3 tools/art/cursor_green.py --check    fails if the committed header is stale

WHY A HEADER AND NOT A FILE BESIDE THE GAME. The model is 94 triangles. Compiled in, it
reaches the Mac build, the Windows build and any later port with no packaging step to
forget, and it cannot go missing from a player's folder.

WHERE THE GEOMETRY COMES FROM. art/cursor/build_initial_low_v01.py builds the approved
mesh in Blender entirely from numbers: a seven point outline in the cartridge pointer's
own pack X/Z, inset by a fixed distance at each of seven height levels, joined ring to
ring, capped top and bottom. The same numbers are repeated here and the same arithmetic
is done in plain Python, so no Blender is needed to rebuild it, and the result is
asserted against the bounds that build recorded in initial_low_v01/verification.json.
Blender's convention there is (pack_x, -pack_z, pack_y) * 0.01, so one Blender unit is
100 pack units; every distance below is in Blender units, as that script writes them.

WHERE THE COLOUR COMES FROM. The approved look is a Blender render with five procedural
preview materials, a three light studio and a little emission. The game's cursor path is
unlit vertex colour, which is also what the cartridge's own cursors are (their shading is
baked into the vertices), and what a fixed-function Win98 card can draw. So the colour is
BAKED FROM THE APPROVED RENDER: every surface that faces upward takes its colour from
initial_low_v01/Actual_Top.png, the straight-down orthographic view of the approved model,
at the place that surface projects to. The walls, which that view cannot see, take the
sidewall material's own colour, shaded by which way they face against the render's key
light. The emission's halo is not reproduced: a halo is not geometry.
"""
import json, math, os, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ART = os.path.join(ROOT, "art", "cursor", "initial_low_v01")
OUT = os.path.join(ROOT, "game", "cursor_green_mesh.h")

# ---- geometry: transcribed from build_initial_low_v01.py ------------------------------
OUTLINE = [(17, 7), (15, 777), (174, 675), (254, 914), (372, 838), (293, 599), (445, 501)]
# (inset BU, height BU, material of the strip from this ring to the next)
LEVELS = [(.04, 1.00, 1), (0, 1.04, 1), (0, 1.51, 2), (.045, 1.57, 0),
          (.25, 1.57, 2), (.29, 1.59, 4), (.43, 1.78, 3)]
MAT_RIM, MAT_SIDE, MAT_EDGE, MAT_FACE, MAT_FACET = 0, 1, 2, 3, 4

poly = [(x * .01, -z * .01) for x, z in OUTLINE]
N = len(poly)


def inset(d):
    out = []
    for i, p in enumerate(poly):
        pv, nx = poly[(i - 1) % N], poly[(i + 1) % N]
        ax, ay = p[0] - pv[0], p[1] - pv[1]
        la = math.hypot(ax, ay); ax, ay = ax / la, ay / la
        bx, by = nx[0] - p[0], nx[1] - p[1]
        lb = math.hypot(bx, by); bx, by = bx / lb, by / lb
        bis = (-ay + -by, ax + bx)
        k = d / (1 + ax * bx + ay * by)
        out.append((p[0] + bis[0] * k, p[1] + bis[1] * k))
    return out


def ear_clip(pts):
    """Triangulate a simple counter-clockwise polygon; returns index triples."""
    idx = list(range(len(pts)))
    cross = lambda o, a, b: (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    def inside(p, a, b, c):
        return cross(a, b, p) >= 0 and cross(b, c, p) >= 0 and cross(c, a, p) >= 0
    tris = []
    while len(idx) > 3:
        for j in range(len(idx)):
            i0, i1, i2 = idx[j - 1], idx[j], idx[(j + 1) % len(idx)]
            a, b, c = pts[i0], pts[i1], pts[i2]
            if cross(a, b, c) <= 0:
                continue
            if any(inside(pts[o], a, b, c) for o in idx if o not in (i0, i1, i2)):
                continue
            tris.append((i0, i1, i2)); idx.pop(j); break
        else:
            raise SystemExit("ear clipping found no ear: the outline is not simple")
    tris.append(tuple(idx))
    return tris


assert sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(poly, poly[1:] + poly[:1])) > 0
rings = [[(x, y, z) for x, y in inset(d)] for d, z, _ in LEVELS]
tris = []                                     # (three Blender-space points, material)
for j in range(len(LEVELS) - 1):
    for i in range(N):
        k = (i + 1) % N
        a, b, c, d = rings[j][i], rings[j][k], rings[j + 1][k], rings[j + 1][i]
        tris.append(((a, b, c), LEVELS[j][2]))
        tris.append(((a, c, d), LEVELS[j][2]))
for i0, i1, i2 in ear_clip([p[:2] for p in rings[0]]):
    tris.append(((rings[0][i2], rings[0][i1], rings[0][i0]), MAT_SIDE))     # bottom
for i0, i1, i2 in ear_clip([p[:2] for p in rings[-1]]):
    tris.append(((rings[-1][i0], rings[-1][i1], rings[-1][i2]), MAT_FACE))  # top face
assert len(tris) == 94, len(tris)

centre = [sum(p[q] for t, _ in tris for p in t) / (3 * len(tris)) for q in range(3)]


def normal(t):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
    ux, uy, uz = bx - ax, by - ay, bz - az
    vx, vy, vz = cx - ax, cy - ay, cz - az
    n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
    l = math.sqrt(sum(q * q for q in n)) or 1.0
    n = tuple(q / l for q in n)
    # Outward: away from the solid's middle, measured horizontally for the walls.
    m = [sum(p[q] for p in t) / 3 for q in range(3)]
    out = (m[0] - centre[0], m[1] - centre[1], m[2] - centre[2])
    if abs(n[2]) > 0.9:                       # a cap: up for the top, down for the bottom
        want = 1 if m[2] > centre[2] else -1
        return n if n[2] * want > 0 else tuple(-q for q in n)
    return n if n[0] * out[0] + n[1] * out[1] > 0 else tuple(-q for q in n)


# ---- colour ---------------------------------------------------------------------------
# Run as a script only: the geometry above is imported by the cursor remaster
# (tools/art/remaster/cursors), which paints its own colour.
def main():
    top = Image.open(os.path.join(ART, "Actual_Top.png")).convert("RGB")
    W, H = top.size
    # The review camera, from build_initial_low_v01.py: orthographic, ortho_scale 12.2,
    # straight down from target + (0,0,18), image up = Blender +Y.
    TX, TY, OSCALE = 2.05, -4.65, 12.2


    def px(x, y):
        return (W / 2 + (x - TX) / OSCALE * W, H / 2 - (y - TY) / OSCALE * H)


    def sample(x, y, r=1):
        cx, cy = px(x, y)
        acc = [0, 0, 0]; n = 0
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                p = top.getpixel((int(round(cx)) + dx, int(round(cy)) + dy))
                for q in range(3): acc[q] += p[q]
                n += 1
        return [a / n for a in acc]


    def median(cols):
        return [sorted(c[q] for c in cols)[len(cols) // 2] for q in range(3)]


    def lin2srgb(c):
        return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


    # The sidewall material's base colour, linear in Blender, converted for display, and
    # the key light's direction from the review target (the render's brightest light).
    SIDE = [lin2srgb(c) * 255 for c in (.002, .018, .001)]
    KEY = (-4 - TX, -1 - TY, 11 - 1.35)
    kl = math.sqrt(sum(q * q for q in KEY)); KEY = tuple(q / kl for q in KEY)

    # A thin strip is only a few pixels wide in the render, so a sample at one vertex lands
    # on its neighbour. Those take ONE colour for the whole strip: the median along the
    # strip's own centre line.
    strip_colour = {}
    for j in range(len(LEVELS) - 1):
        mids = []
        for i in range(N):
            k = (i + 1) % N
            for f in (0.2, 0.35, 0.5, 0.65, 0.8):
                a = [rings[j][i][q] * (1 - f) + rings[j][k][q] * f for q in range(2)]
                b = [rings[j + 1][i][q] * (1 - f) + rings[j + 1][k][q] * f for q in range(2)]
                mids.append(sample((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 0))
        strip_colour[j] = median(mids)

    # The wide surfaces (the dark band, the broad facet and the lime face) take a colour PER
    # CORNER, which is what carries the render's gradient. Each corner is sampled at ONE
    # place whichever triangle it belongs to, or neighbouring triangles would disagree at a
    # shared vertex and the face would show its triangulation as seams: a band's corner at
    # the middle of the band at that corner, the face's corner a little inside the face.
    FACE_IN = inset(LEVELS[-1][0] + 0.07)
    ring_of = {}
    for r, ring in enumerate(rings):
        for i, p in enumerate(ring):
            ring_of[p] = (r, i)


    def corner_colour(p, band):
        r, i = ring_of[p]
        if band is None:                          # the top face
            return sample(FACE_IN[i][0], FACE_IN[i][1])
        a, b = rings[band][i], rings[band + 1][i]
        return sample((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)


    out = []
    for n_t, (t, mat) in enumerate(tris):
        n = normal(t)
        band = n_t // (2 * N) if n_t < 2 * N * (len(LEVELS) - 1) else None
        cols = []
        for p in t:
            if n[2] < 0.3:
                # A wall or the underside: the render never sees it from above.
                lam = max(0.0, sum(n[q] * KEY[q] for q in range(3)))
                c = [s * (0.55 + 0.9 * lam) for s in SIDE]
            elif mat == MAT_EDGE:
                c = strip_colour[band]
            else:
                c = corner_colour(p, band)
            # Never let a colour look like a packed normal (one channel near 128, two near
            # zero): the cursor draw whitens those, which is how the cartridge's own slabs
            # get their white tops.
            cols.append([max(3, min(255, int(round(v)))) for v in c])
        # Blender (x, y, z) -> pack (x, y, z) = (bx, bz, -by) * 100
        out.append([(p[0] * 100, p[2] * 100, -p[1] * 100) + tuple(c) for p, c in zip(t, cols)])

    # ---- verify against what the approved build recorded ----------------------------------
    ver = json.load(open(os.path.join(ART, "verification.json")))
    want = ver["new_physical_bounds_pack"]
    got = {"min": [min(v[q] for t in out for v in t) for q in range(3)],
           "max": [max(v[q] for t in out for v in t) for q in range(3)]}
    for key in ("min", "max"):
        for q in range(3):
            assert abs(got[key][q] - want[key][q]) < 0.01, (key, q, got[key][q], want[key][q])
    assert ver["triangles_evaluated"] == len(out)

    lines = [
        "/* GENERATED by tools/art/cursor_green.py from art/cursor/initial_low_v01. Do not edit;",
        "   run the script. The green pointer: %d triangles, pack mesh units, unlit vertex" % len(out),
        "   colour. Bounds checked against the approved build's own verification.json. */",
        "#ifndef CNC_CURSOR_GREEN_MESH_H",
        "#define CNC_CURSOR_GREEN_MESH_H",
        "struct CursorGreenVert { float x, y, z; unsigned char r, g, b; };",
        "static const int CURSOR_GREEN_NTRIS = %d;" % len(out),
        "static const CursorGreenVert CURSOR_GREEN_TRIS[%d][3] = {" % len(out),
    ]
    for t in out:
        lines.append("    { " + ", ".join("{%.3ff, %.3ff, %.3ff, %d, %d, %d}" % v for v in t) + " },")
    lines += ["};", "#endif", ""]
    text = "\n".join(lines).replace("-0.000f", "0.000f")

    if "--check" in sys.argv:
        have = open(OUT).read() if os.path.exists(OUT) else ""
        if have != text:
            raise SystemExit("game/cursor_green_mesh.h is stale: run tools/art/cursor_green.py")
        print("cursor_green: header up to date")
    else:
        open(OUT, "w").write(text)
        print("cursor_green: wrote %s, %d triangles, bounds %s" % (os.path.relpath(OUT, ROOT), len(out), got))


if __name__ == "__main__":
    main()
