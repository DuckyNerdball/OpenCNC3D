"""Turns an OpenCNC3DVfxBake recording into the pack the renderer loads (game/vfx3d/<name>/).

    python3 tools/art/unity_vfx/make_pack.py --bake <bakedir> --project <unity project> \
        --out game/vfx3d/<name> --texdir game/vfx3d/_textures

The bake (Editor/OpenCNC3DVfxBake.cs) holds the particles and the meshes. Everything the
material side needs comes from the Unity project the pack was imported into: each
renderer's .mat for its shader and parameters, and each texture's source file plus its
.meta, because the IMPORT SETTINGS are part of the look. Unity samples a texture the way
its importer says to, so the pack carries the same answers: the size cap (every
flipbook in the Vefects pack is authored at 4096 and imported at 2048), sRGB or linear,
wrap, point or bilinear, mipmaps or not.

Output:
  fx.txt            the text description the renderer parses (format at write_fx below)
  <texdir>/tex_<name>.c3t  "C3TX" w h comps flags, then zlib(rows bottom-up, 8 bits a
                    channel); shared by every pack, one copy per texture
  mesh_<name>.bin   the bake's mesh files, unchanged
  seed<k>.c3vz      "C3VZ" rawsize, then zlib(the bake's seed<k>.bin)
"""
import argparse, json, os, re, shutil, struct, zlib, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import uyaml, c3vx

SHADER_KIND = {
    'SH_Vefects_URP_VFX_Pyro_MV_Emission_Dissolve': 'PYRO',
    'SH_Vefects_URP_VFX_Flash': 'FLASH',
    'SH_Vefects_URP_VFX_Emissive_Wicks': 'WICKS',
    'SH_Vefects_URP_VFX_Fake_Decal': 'DECAL',
    'SH_Vefects_URP_VFX_Sprite_Debris': 'SPRITEDEBRIS',
    'SH_Vefects_URP_VFX_Concrete': 'CONCRETE',
    'SH_Vefects_URP_VFX_Distortion_Shockwave': 'DISTORT',
    'SH_Vefects_URP_VFX_Pyro_MV_New': 'PYRONEW',
    'SH_Vefects_URP_VFX_Pyro_MV_Fire': 'PYROFIRE',
    'SH_Vefects_URP_VFX_Dirt_Debris': 'DIRTDEBRIS',
    'SH_Vefects_URP_VFX_Pyro_MV_New_Unlit': 'PYRONEWU',
    # the gun pack's Shader Graphs (Realistic Gun VFX, URP)
    'S_Fumes_Master': 'FUMES',
    'S_Smoke_Master': 'SMOKEM',
    'S_SmokeTrail': 'SMOKETRAIL',
    'S_Spark': 'SPARK',
    'S_Ember': 'EMBER',
}

# A SHADER GRAPH WITHOUT MATERIAL OVERRIDE FIXES ITS OWN BLEND AND CULL in the shader it
# generates, and its .mat carries no _SrcBlend to read. These are read off the generated
# forward pass (OpenCNC3DVfxBake.DumpShaderGraphs): Unity blend factor numbers, Unity cull.
SG_FIXED_STATE = {
    'FUMES': (1, 10, 2),        # Blend One OneMinusSrcAlpha, Cull Back
    'SPARK': (5, 10, 2),        # Blend SrcAlpha OneMinusSrcAlpha, Cull Back
    'EMBER': (5, 10, 2),
}
# The two lit graphs allow material override: blend and cull are the .mat's own floats.
SG_MATERIAL_STATE = ('SMOKEM', 'SMOKETRAIL')

def meta_field(text, key, default=None):
    m = re.search(r'\n\s*' + key + r': (\S+)', text)
    return m.group(1) if m else default

def guid_index(project):
    """GUID -> asset path, from every .meta under Assets."""
    idx = {}
    for root, _, files in os.walk(os.path.join(project, 'Assets')):
        for f in files:
            if f.endswith('.meta'):
                p = os.path.join(root, f)
                g = meta_field(open(p, errors='replace').read(), 'guid')
                if g: idx[g] = p[:-5]
    return idx

def texture(project, path, out, done):
    """Into the SHARED folder (--texdir): several packs use the same 2048 flipbooks, and
    one copy of each is what the renderer loads. A texture already there is kept."""
    name = os.path.splitext(os.path.basename(path))[0]
    if name in done: return name
    meta = open(path + '.meta').read()
    srgb = int(meta_field(meta, 'sRGBTexture', '1'))
    cap = int(meta_field(meta, 'maxTextureSize', '2048'))
    wrap = int(meta_field(meta, 'wrapU', '0'))            # 0 repeat, 1 clamp, 2 mirror
    filt = int(meta_field(meta, 'filterMode', '1'))       # 0 point, 1 bilinear, 2 trilinear
    mips = int(meta_field(meta, 'enableMipMap', '1'))
    im = Image.open(path)
    im = im.convert('RGBA')
    w, h = im.size
    s = min(1.0, cap / max(w, h))
    if s < 1.0:
        # Downscaled the way the importer does, in linear light for an sRGB texture, so
        # a 4096 flipbook arrives at 2048 without its smoke darkening at the edges.
        a = np.asarray(im).astype(np.float32) / 255.0
        if srgb:
            a[..., :3] = np.where(a[..., :3] <= 0.04045, a[..., :3] / 12.92,
                                  ((a[..., :3] + 0.055) / 1.055) ** 2.4)
        nw, nh = int(w * s), int(h * s)
        k = int(round(1 / s))
        a = a.reshape(nh, k, nw, k, 4).mean(axis=(1, 3))
        if srgb:
            a[..., :3] = np.where(a[..., :3] <= 0.0031308, a[..., :3] * 12.92,
                                  1.055 * np.power(np.maximum(a[..., :3], 0), 1 / 2.4) - 0.055)
        im = Image.fromarray(np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGBA')
        w, h = nw, nh
    # GL's first row is the bottom one, which is also where Unity's v = 0 is.
    data = np.asarray(im.transpose(Image.FLIP_TOP_BOTTOM)).tobytes()
    flags = (srgb << 0) | (min(wrap, 1) << 1) | ((1 if filt == 0 else 0) << 2) | (mips << 3)
    dst = os.path.join(TEXDIR, f'tex_{name}.c3t')
    if not os.path.exists(dst):
        with open(dst, 'wb') as f:
            f.write(b'C3TX' + struct.pack('<4I', w, h, 4, flags) + zlib.compress(data, 9))
    done[name] = (w, h, srgb, wrap, filt, mips, os.path.relpath(path, project))
    return name

def color_properties(shader_path):
    if shader_path.endswith('.shadergraph'):
        return graph_color_properties(shader_path)
    return text_color_properties(shader_path)

def graph_color_properties(path):
    """A Shader Graph's Color properties, by reference name, from its own JSON -- the
    plain ones only. An HDR colour (m_ColorMode 1) reaches the shader AS STORED: measured
    on the gun pack's flash (_EmissiveColor, HDR), converting it put the flash's glow red
    where Unity's is orange, 77 levels out at the 99th percentile against 28 without."""
    t = open(path).read()
    dec = json.JSONDecoder()
    i, out = 0, set()
    while i < len(t):
        while i < len(t) and t[i].isspace():
            i += 1
        if i >= len(t):
            break
        o, i = dec.raw_decode(t, i)
        if o.get('m_Type', '').endswith('ColorShaderProperty') and o.get('m_ColorMode', 0) != 1:
            out.add(o.get('m_OverrideReferenceName') or o.get('m_DefaultReferenceName'))
    return out

def text_color_properties(shader_path):
    """The properties a shader declares as Color, as opposed to Vector. Both are stored
    under m_Colors in a .mat, but in a linear colour space project Unity hands a COLOR
    to the shader converted from sRGB to linear and a vector untouched. Measured on this
    pack: the smoke tint arrives at about a third of its written value, which is exactly
    what separated the game's smoke from Unity's before this was done."""
    src = open(shader_path, errors='replace').read()
    props = src[src.find('Properties'):src.find('SubShader')]
    # a property marked [HDR] is passed as stored (see graph_color_properties)
    return set(m.group(2) for m in re.finditer(r'(\[HDR\]\s*)?(\w+)\s*\(\s*"[^"]*"\s*,\s*Color\s*\)', props)
               if not m.group(1))

def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def material(project, gidx, mat_path, out, texdone):
    d = uyaml.load(os.path.join(project, mat_path))
    (c, t, b), = [v for v in d.values() if v[1] == 'Material']
    shader_path = gidx[b['m_Shader']['guid']]
    shader = os.path.splitext(os.path.basename(shader_path))[0]
    kind = SHADER_KIND[shader]
    colors = color_properties(shader_path)
    P = b['m_SavedProperties']
    params = {}
    for e in P['m_Floats']:
        (k, v), = e.items(); params[k] = float(v)
    for e in P['m_Colors']:
        (k, v), = e.items(); params[k] = [float(v[x]) for x in 'rgba']
        if k in colors:
            params[k] = [srgb_to_linear(x) for x in params[k][:3]] + params[k][3:]
    tex = {}
    for e in P['m_TexEnvs']:
        (k, v), = e.items()
        g = v['m_Texture'].get('guid')
        if g and g in gidx:
            tex[k] = texture(project, gidx[g], out, texdone)
            params[k + '_ST'] = [v['m_Scale']['x'], v['m_Scale']['y'], v['m_Offset']['x'], v['m_Offset']['y']]
    if kind in SG_FIXED_STATE:
        params['_Src'], params['_Dst'], params['_Cull'] = (float(x) for x in SG_FIXED_STATE[kind])
    elif kind in SG_MATERIAL_STATE:
        params['_Src'] = params.get('_SrcBlend', 5.0)
        params['_Dst'] = params.get('_DstBlend', 10.0)
    return kind, shader, tex, params

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--bake', required=True)
    ap.add_argument('--project', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--texdir', required=True, help='the shared texture folder, game/vfx3d/_textures')
    a = ap.parse_args()
    global TEXDIR
    TEXDIR = a.texdir
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(TEXDIR, exist_ok=True)
    meta = c3vx.load_meta(a.bake)
    gidx = guid_index(a.project)
    texdone, mats, matidx, meshes = {}, [], {}, []
    lines = ['C3FX 1', f'fps {meta["fps"]}', f'frames {meta["frames"]}']
    # A TRAIL RECORDING: the prefab flew at `move` metres a second until `movestop`, so
    # every particle it left can be replayed relative to where the emitter was at its birth
    mv = meta.get('move', [0, 0, 0])
    if any(abs(x) > 1e-6 for x in mv):
        lines.append(f'move {mv[0]} {mv[1]} {mv[2]}')
        lines.append(f'movestop {meta.get("moveStop", 1e9)}')
    seeds = sorted(f for f in os.listdir(a.bake) if re.match(r'seed\d+\.bin$', f))
    lines.append(f'seeds {len(seeds)}')
    sysl = []
    def local_of(s):
        """A LOCAL-SPACE SYSTEM records its particles in its own space, and the gun pack's
        sit a few centimetres along the barrel from the prefab's origin: their offset and
        turn (never the scale, which recon.system_scale accounts for) go with them."""
        l2w = np.array(s['l2w']).reshape(4, 4).T
        sc = float(np.linalg.norm(l2w[:3, 0]))
        if s.get('simSpace') != 0:
            return ''
        R = l2w[:3, :3] / max(sc, 1e-9)
        off = l2w[:3, 3]
        if np.abs(R - np.eye(3)).max() <= 1e-5 and np.abs(off).max() <= 1e-6:
            return ''
        return (f'local={",".join("%.6g" % x for x in off)},'
                f'{",".join("%.6g" % x for x in R.reshape(-1))} ')
    for s in meta['systems']:
        tr = s.get('trail')
        if tr and tr.get('material') and s.get('rendererEnabled'):
            # THE TRAILS MODULE's ribbon, drawn with the renderer's second material; its
            # width and colour along the trail are Unity's own curves, sampled at 33 points
            tm = tr['material']
            if tm not in matidx:
                matidx[tm] = len(mats)
                mats.append((tm,) + material(a.project, gidx, tm, a.out, texdone))
            w = ','.join('%.6g' % x for x in tr['width'])
            c = ','.join('%.6g' % x for col in tr['color'] for x in col)
            sysl.append(f'ribbon {s["index"]} mat={matidx[tm]} mode={tr["mode"]} tex={tr["textureMode"]} '
                        f'sizeWidth={tr["sizeAffectsWidth"]} inherit={tr["inheritParticleColor"]} '
                        f'attach={tr["attachRibbonsToTransform"]} widthMul={tr["widthMultiplier"]} '
                        f'lifetime={tr["lifetime"]} minVD={tr["minVertexDistance"]} '
                        f'{local_of(s)}width={w} color={c}')
        if not s.get('rendererEnabled') or s.get('renderMode') in (None, 'None') or not s.get('material'):
            if s.get('lights'):
                L = s['lights']
                sysl.append(f'light {s["index"]} color={L["color"][0]},{L["color"][1]},{L["color"][2]} '
                            f'intensity={L["intensity"]} range={L["range"]} '
                            f'intensityMultiplier={L["intensityMultiplier"]} rangeMultiplier={L["rangeMultiplier"]} '
                            f'useParticleColor={L["useParticleColor"]} sizeAffectsRange={L["sizeAffectsRange"]} '
                            f'alphaAffectsIntensity={L["alphaAffectsIntensity"]} {local_of(s)}'.rstrip())
            continue
        mp = s['material']
        if mp not in matidx:
            matidx[mp] = len(mats)
            mats.append((mp,) + material(a.project, gidx, mp, a.out, texdone))
        mids = []
        for mn in s.get('meshes', []):
            safe = mn.replace(' ', '_').replace('/', '_')
            if safe not in meshes:
                meshes.append(safe)
                shutil.copy(os.path.join(a.bake, f'mesh_{safe}.bin'), os.path.join(a.out, f'mesh_{safe}.bin'))
            mids.append(str(meshes.index(safe)))
        tsa = s.get('tsa') or {}
        # the system's scale, which a "Large" or "Small" variant sets on its root (see
        # recon.system_scale for what it does and does not scale, measured)
        l2w = np.array(s['l2w']).reshape(4, 4).T
        scale = float(np.linalg.norm(l2w[:3, 0]))
        local = local_of(s)
        sysl.append(f'sys {s["index"]} mode={s["renderMode"]} align={s["alignment"]} mat={matidx[mp]} {local}'
                    f'sim={s.get("simSpace", 1)} '
                    f'meshes={",".join(mids) or "-"} fudge={s["sortingFudge"]} '
                    f'lengthScale={s["lengthScale"]} velocityScale={s["velocityScale"]} '
                    f'tiles={tsa.get("tilesX", 1)},{tsa.get("tilesY", 1)} '
                    f'maxSize={s["maxParticleSize"]} minSize={s["minParticleSize"]} '
                    f'scale={scale:.6g} '
                    f'name={s["name"].replace(" ", "_")}')
    # TRAIL RENDERERS: a ribbon through the emitter's own past positions (the flare's smoke)
    for L in meta.get('lines', []):
        if not L.get('enabled') or not L.get('material'):
            continue
        lm = L['material']
        if lm not in matidx:
            matidx[lm] = len(mats)
            mats.append((lm,) + material(a.project, gidx, lm, a.out, texdone))
        w = ','.join('%.6g' % x for x in L['width'])
        c = ','.join('%.6g' % x for col in L['color'] for x in col)
        sysl.append(f'line {L["index"]} mat={matidx[lm]} time={L["time"]} minVD={L["minVertexDistance"]} '
                    f'widthMul={L["widthMultiplier"]} tex={L["textureMode"]} '
                    f'texScale={L["textureScale"][0]},{L["textureScale"][1]} align={L["alignment"]} '
                    f'width={w} color={c}')
    for name, (w, h, srgb, wrap, filt, mips, src) in texdone.items():
        lines.append(f'tex {name} {w} {h} srgb={srgb} wrap={wrap} filter={filt} mips={mips}')
    for i, m in enumerate(meshes):
        lines.append(f'mesh {i} {m}')
    for i, (mp, kind, shader, tex, params) in enumerate(mats):
        kv = ' '.join(f'{k}={",".join(str(x) for x in v) if isinstance(v, list) else v}'
                      for k, v in sorted(params.items()))
        tv = ' '.join(f'{k}={v}' for k, v in sorted(tex.items()))
        lines.append(f'mat {i} {kind} {tv} {kv}')
    lines += sysl
    open(os.path.join(a.out, 'fx.txt'), 'w').write('\n'.join(lines) + '\n')
    for k, f in enumerate(seeds):
        raw = open(os.path.join(a.bake, f), 'rb').read()
        with open(os.path.join(a.out, f'seed{k}.c3vz'), 'wb') as o:
            o.write(b'C3VZ' + struct.pack('<I', len(raw)) + zlib.compress(raw, 9))
    tot = sum(os.path.getsize(os.path.join(a.out, f)) for f in os.listdir(a.out))
    # (the pack's own files; its textures are counted in the shared folder)
    print(f'{a.out}: {len(texdone)} textures, {len(meshes)} meshes, {len(mats)} materials, '
          f'{len(seeds)} seeds, {tot / 1e6:.1f} MB')

if __name__ == '__main__':
    main()
