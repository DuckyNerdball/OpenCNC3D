"""Shared Blender helpers for the fix-and-improve recipe. Imported by the stage scripts,
which run inside Blender (blender -b --python ...), never as plain Python."""
import json
import math
import os

import bpy
import bmesh
from mathutils import Vector

CELL = 1024.0


def to_blender(p):
    """Pack frame (x east, y up, z south, 1024 to the cell) to Blender cells, Z up.
    The inverse of blender_c3m.py's (X, Y, Z) -> (1024X, 1024Z, -1024Y)."""
    return (p[0] / CELL, -p[2] / CELL, p[1] / CELL)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def load_image(path, name=None):
    img = bpy.data.images.load(path, check_existing=True)
    if name:
        img.name = name
    return img


def flat_material(name, img, xlu=False):
    """What the game's pack path draws: texture times baked vertex colour, no light.
    Used for review renders only, so the fixed model can be compared with the cartridge
    as it appears in play."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs[0].default_value = 1.0
    nt.links.new(vc.outputs[0], mul.inputs[6])
    if img is not None:
        tx = nt.nodes.new("ShaderNodeTexImage")
        tx.image = img
        tx.interpolation = "Linear"
        nt.links.new(tx.outputs[0], mul.inputs[7])
    else:
        mul.inputs[7].default_value = (1, 1, 1, 1)
    nt.links.new(mul.outputs[2], em.inputs[0])
    if xlu:
        tr = nt.nodes.new("ShaderNodeBsdfTransparent")
        mix = nt.nodes.new("ShaderNodeMixShader")
        mix.inputs[0].default_value = 0.5
        nt.links.new(tr.outputs[0], mix.inputs[1])
        nt.links.new(em.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs[0])
        m.blend_method = "BLEND" if hasattr(m, "blend_method") else m.blend_method
    else:
        nt.links.new(em.outputs[0], out.inputs[0])
    return m


def build_cartridge(src_dir, code, house="gdi", with_shadow=False):
    """Rebuild the cartridge mesh exactly as the pack holds it: one object per part,
    one material per texture, UVs and the baked vertex colour kept. Returns the objects."""
    doc = json.load(open(os.path.join(src_dir, code + ".json")))
    mats = {}
    objs = []
    for pi, part in enumerate(doc["parts"]):
        me = bpy.data.meshes.new("%s_p%d" % (code, pi))
        bm = bmesh.new()
        uvl = bm.loops.layers.uv.new("UVMap")
        col = bm.loops.layers.color.new("Col")
        tex_of = bm.faces.layers.int.new("pack_tex")
        mode_of = bm.faces.layers.int.new("pack_mode")
        tri_of = bm.faces.layers.int.new("pack_tri")
        slot = {}
        for ti in range(part["tri0"], part["tri0"] + part["ntri"]):
            t = doc["tris"][ti]
            if t["mode"] == "shadow" and not with_shadow:
                continue
            vs = [bm.verts.new(to_blender(c["p"])) for c in t["v"]]
            try:
                f = bm.faces.new(vs)
            except ValueError:
                continue
            key = (t["tex"], t["mode"])
            if key not in slot:
                slot[key] = len(slot)
            f.material_index = slot[key]
            f[tex_of] = t["tex"]
            f[mode_of] = ["opaque", "cutout", "shadow", "xlu"].index(t["mode"])
            f[tri_of] = ti
            for lp, c in zip(f.loops, t["v"]):
                lp[uvl].uv = (c["uv"][0], 1.0 - c["uv"][1])
                lp[col] = tuple(x / 255.0 for x in c["c"])
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new(me.name, me)
        bpy.context.scene.collection.objects.link(ob)
        for (tex, mode), s in sorted(slot.items(), key=lambda kv: kv[1]):
            mk = (tex, mode)
            if mk not in mats:
                img = None
                if tex >= 0:
                    info = doc["textures"][str(tex)]
                    img = load_image(os.path.join(src_dir, info[house]))
                mats[mk] = flat_material("%s_t%03d_%s" % (code, tex, mode), img, mode == "xlu")
            me.materials.append(mats[mk])
        ob["pack_part"] = pi
        ob["pack_pivot"] = part["pivot"]
        objs.append(ob)
    return doc, objs


def mark_backfaces(colour=(0.0, 1.0, 1.0, 1.0)):
    """Paint every back face flat cyan, so a wall the cartridge never modelled reads as
    a cyan patch in the review render instead of as a plausible shadow."""
    for m in bpy.data.materials:
        nt = m.node_tree
        out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
        src = out.inputs[0].links[0].from_socket
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        em = nt.nodes.new("ShaderNodeEmission")
        em.inputs[0].default_value = colour
        mix = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(geo.outputs["Backfacing"], mix.inputs[0])
        nt.links.new(src, mix.inputs[1])
        nt.links.new(em.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs[0])


def add_wire_overlay(objs, thickness=0.004):
    """Draw every edge as a thin black strut, for review renders only."""
    m = bpy.data.materials.new("wire_black")
    m.use_nodes = True
    em = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    em.inputs["Base Color"].default_value = (0, 0, 0, 1)
    em.inputs["Roughness"].default_value = 1.0
    for ob in list(objs):
        w = ob.copy()
        w.data = ob.data.copy()
        w.data.materials.clear()
        w.data.materials.append(m)
        bpy.context.scene.collection.objects.link(w)
        mod = w.modifiers.new("wire", "WIREFRAME")
        mod.thickness = thickness
        mod.use_replace = True
        mod.material_offset = 0


def bounds(objs):
    lo = Vector((1e9,) * 3)
    hi = Vector((-1e9,) * 3)
    for ob in objs:
        for v in ob.data.vertices:
            w = ob.matrix_world @ v.co
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
    return lo, hi


def review_renders(objs, out_prefix, size=640, views=8, elev=35.0, bg=(1.0, 0.0, 1.0), az0=0.0):
    """Orbit the model at the game camera's rough elevation and write one PNG per view,
    against a magenta world so a hole in the shell cannot pass for shading."""
    sc = bpy.context.scene
    try:
        sc.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = size
    sc.render.resolution_y = size
    sc.render.film_transparent = False
    if sc.world is None:
        sc.world = bpy.data.worlds.new("W")
    sc.world.use_nodes = True
    bgn = next(n for n in sc.world.node_tree.nodes if n.type == "BACKGROUND")
    bgn.inputs[0].default_value = (*bg, 1.0)
    sc.view_settings.view_transform = "Standard"
    lo, hi = bounds(objs)
    c = (lo + hi) / 2
    r = (hi - lo).length * 0.5
    cam_d = bpy.data.cameras.new("rev")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = r * 2.2
    cam = bpy.data.objects.new("rev", cam_d)
    sc.collection.objects.link(cam)
    sc.camera = cam
    paths = []
    for i in range(views):
        az = math.radians(az0 + 360.0 * i / views)
        el = math.radians(elev)
        d = Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el)))
        cam.location = c + d * (r * 4)
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        sc.render.filepath = "%s_%d.png" % (out_prefix, i)
        bpy.ops.render.render(write_still=True)
        paths.append(sc.render.filepath)
    bpy.data.objects.remove(cam)
    return paths


def export_view_json(objs, out_dir, name, house_images=None, anim=None):
    """Write <out_dir>/<name>.json for the side-by-side viewer: one group per material,
    each with its texture file and flat arrays of position (Blender cells, Z up), uv and
    corner colour. Textures are copied beside it as PNG. house_images maps an image name
    to {"gdi": path, "nod": path} so the viewer can switch palettes."""
    import shutil
    os.makedirs(out_dir, exist_ok=True)
    dg = bpy.context.evaluated_depsgraph_get()
    groups = {}
    ntri = 0
    for ob in objs:
        ev = ob.evaluated_get(dg)
        me = ev.to_mesh()
        me.calc_loop_triangles()
        uv = me.uv_layers.active.data if me.uv_layers.active else None
        col = me.color_attributes.get("Col")
        cdom = col.domain if col else None
        for lt in me.loop_triangles:
            mat = me.materials[lt.material_index] if me.materials else None
            part = int(ob.get("pack_part", 0))
            key = (part, mat.name if mat else "none")
            g = groups.get(key)
            if g is None:
                tex = None
                xlu = False
                if mat and mat.node_tree:
                    im = next((n.image for n in mat.node_tree.nodes
                               if n.type == "TEX_IMAGE" and n.image), None)
                    if im:
                        tex = im
                    xlu = mat.name.endswith("_xlu") or mat.get("xlu", False)
                cut = bool(mat and (mat.name.endswith("_cutout") or mat.get("cutout", False)))
                g = groups[key] = dict(mat=key[1], part=part, tex=tex, xlu=bool(xlu), cut=cut, p=[], uv=[], c=[])
            for li, vi in zip(lt.loops, lt.vertices):
                w = ob.matrix_world @ me.vertices[vi].co
                g["p"] += [round(w.x, 5), round(w.y, 5), round(w.z, 5)]
                if uv:
                    u = uv[li].uv
                    g["uv"] += [round(u.x, 5), round(u.y, 5)]
                else:
                    g["uv"] += [0.0, 0.0]
                if col:
                    c = col.data[li if cdom == "CORNER" else vi].color
                    g["c"] += [round(c[0], 4), round(c[1], 4), round(c[2], 4)]
                else:
                    g["c"] += [1.0, 1.0, 1.0]
            ntri += 1
        ev.to_mesh_clear()
    out = []
    for g in groups.values():
        entry = dict(mat=g["mat"], part=g["part"], xlu=g["xlu"], cut=g["cut"], p=g["p"], uv=g["uv"], c=g["c"])
        im = g["tex"]
        if im is not None:
            files = {}
            variants = (house_images or {}).get(im.name) or {"gdi": bpy.path.abspath(im.filepath)}
            for house, srcp in variants.items():
                fn = "%s_%s" % (name, os.path.basename(srcp))
                if srcp and os.path.exists(srcp):
                    shutil.copyfile(srcp, os.path.join(out_dir, fn))
                else:
                    im.filepath_raw = os.path.join(out_dir, fn)
                    im.file_format = "PNG"
                    im.save()
                files[house] = fn
            entry["tex"] = files
        out.append(entry)
    with open(os.path.join(out_dir, name + ".json"), "w") as f:
        json.dump(dict(name=name, triangles=ntri, groups=out, anim=anim), f)
    return ntri


def thumbnail(objs, path, size=192, az=-70.0, elev=38.0):
    """A small transparent render at roughly the play camera's angle, for the viewer's
    list. Drawn the way the game draws cartridge meshes (texture times baked colour)."""
    sc = bpy.context.scene
    try:
        sc.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = sc.render.resolution_y = size
    sc.render.film_transparent = True
    sc.view_settings.view_transform = "Standard"
    lo, hi = bounds(objs)
    c = (lo + hi) / 2
    r = (hi - lo).length * 0.5
    cam_d = bpy.data.cameras.new("thumb")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = r * 2.05
    cam = bpy.data.objects.new("thumb", cam_d)
    sc.collection.objects.link(cam)
    sc.camera = cam
    a, e = math.radians(az), math.radians(elev)
    d = Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e)))
    cam.location = c + d * (r * 4)
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)
