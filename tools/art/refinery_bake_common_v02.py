"""Reusable geometry-preserving PROC bake policy for physical and material passes.

Caller opens PROC_TextureUV_v01.blend or a derived bake scene and creates active
image target nodes. Each selected receiver keeps its original topology/BakeUV/
corner normals. Call prepare_visibility once, then select_pair per receiver.
"""
import json
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[2]

def assert_closed_orientation(objects):
    """Reject inverted closed components before expensive projection work.

    Open tower/pipe shells require separate reference-side checks; signed volume
    is only a valid automatic orientation test on closed, consistently wound parts.
    """
    import bmesh
    bad=[]
    for o in objects:
        if o.type!='MESH':continue
        bm=bmesh.new();bm.from_mesh(o.data)
        if bm.faces:
            if any(e.is_manifold and not e.is_contiguous for e in bm.edges):
                bad.append((o.name,'inconsistent neighboring face winding'))
            elif all(e.is_manifold for e in bm.edges) and bm.calc_volume(signed=True)<-1e-9:
                bad.append((o.name,'inward closed component'))
        bm.free()
    assert not bad,('Inward closed bake receivers; repair and renew mesh approval before UV/bake',bad)

def source_mapping():
    return json.loads((ROOT/'art/refinery/texture_v02/SOURCE_RECEIVER_MAP.json').read_text())

def cage_for(name):
    if 'faceted_cladding' in name:return .32,.72
    if 'inner_well_floor' in name:return .65,1.4
    if 'dark_tray' in name:return .24,.52
    if 'Foundation' in name:return .12,.28
    if 'rounded_pipe' in name:return .20,.45
    return .20,.45


def configure_bake(scene,resolution=4096,samples=8,threads=4):
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=samples
    scene.render.threads_mode='FIXED';scene.render.threads=threads
    scene.render.bake.use_selected_to_active=True;scene.render.bake.use_cage=False
    scene.render.bake.use_clear=False;scene.render.bake.margin=max(2,round(8*resolution/4096))
    scene.render.bake.margin_type='EXTEND';scene.render.bake.target='IMAGE_TEXTURES'
    scene.render.bake.normal_space='TANGENT';scene.render.bake.normal_r='POS_X'
    scene.render.bake.normal_g='POS_Y';scene.render.bake.normal_b='POS_Z'

def prepare_visibility():
    high=bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE']
    bpy.context.view_layer.layer_collection.children[high.name].exclude=False
    high.hide_render=False;high.hide_viewport=False
    for obj in high.objects:obj.hide_set(False)
    bpy.context.view_layer.update()
    for obj in bpy.context.scene.objects:
        obj.hide_render=True;obj.select_set(False)

def select_pair(receiver,source_names,previous=None):
    if previous:
        for ob in previous:ob.hide_render=True;ob.select_set(False)
    for ob in bpy.context.selected_objects:ob.select_set(False)
    objects=[receiver]+[bpy.data.objects[name] for name in source_names]
    for ob in objects:ob.hide_set(False);ob.hide_render=False;ob.select_set(True)
    receiver.data.uv_layers.active=receiver.data.uv_layers['BakeUV']
    receiver.data.uv_layers['BakeUV'].active_render=True
    bpy.context.view_layer.objects.active=receiver
    ext,dist=cage_for(receiver.name)
    bpy.context.scene.render.bake.cage_extrusion=ext
    bpy.context.scene.render.bake.max_ray_distance=dist
    bpy.context.view_layer.update()
    return objects

def joined_source(source_names, name='PROC_TEMP_JOINED_HIGH'):
    """Evaluate source modifiers; preserve polygon boundaries/flags/sharp edges/normals.

    Geometry remains at original world positions. The returned object is a
    temporary non-export source. Caller owns cleanup. Source material IDs resolve
    back to permanent bpy.data.materials, never an evaluated temporary reference.
    """
    import numpy as np
    from mathutils import Vector
    bpy.context.view_layer.update();deps=bpy.context.evaluated_depsgraph_get()
    verts=[];faces=[];normals=[];smooth=[];face_mats=[];materials=[];sharp=[];owners=[]
    for source_name in source_names:
        obj=bpy.data.objects[source_name];eo=obj.evaluated_get(deps);me=eo.to_mesh()
        off=len(verts);verts.extend(tuple(eo.matrix_world@v.co) for v in me.vertices)
        normal_matrix=eo.matrix_world.inverted().transposed().to_3x3()
        slots=[]
        for mat in me.materials:
            original=bpy.data.materials.get(mat.name) if mat else None
            if original not in materials:materials.append(original)
            slots.append(materials.index(original))
        for poly in me.polygons:
            faces.append(tuple(off+i for i in poly.vertices));smooth.append(poly.use_smooth)
            face_mats.append(slots[poly.material_index] if slots else 0);owners.append(source_name)
            normals.extend(tuple((normal_matrix@me.corner_normals[i].vector).normalized()) for i in poly.loop_indices)
        sharp.extend(tuple(sorted((off+e.vertices[0],off+e.vertices[1]))) for e in me.edges if e.use_edge_sharp)
        eo.to_mesh_clear()
    mesh=bpy.data.meshes.new(name+'_mesh');mesh.from_pydata(verts,[],faces);mesh.update()
    for mat in materials:mesh.materials.append(mat)
    for poly,flag,index in zip(mesh.polygons,smooth,face_mats):poly.use_smooth=flag;poly.material_index=index
    sharp=set(sharp)
    for edge in mesh.edges:edge.use_edge_sharp=tuple(sorted(edge.vertices)) in sharp
    mesh.normals_split_custom_set(normals);mesh.update()
    got=np.asarray([n.vector[:] for n in mesh.corner_normals],np.float64)
    error=float(np.max(np.linalg.norm(got-np.asarray(normals,np.float64),axis=1)))
    if error>.001:raise RuntimeError(f'Joined high source normal error {error} for {source_names}')
    obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj)
    obj['non_export']=True;obj['source_names_json']=json.dumps(source_names)
    obj['maximum_source_corner_normal_error']=error
    return obj,{'source_count':len(source_names),'source_names':source_names,'vertices':len(verts),'polygons':len(faces),'max_corner_normal_error':error,'preserved_sharp_edges':len(sharp)}

def ao_context_names(receiver, source_names, distance=.65):
    """Conservative high-only AABB candidates for local contact occlusion.

    These candidates are render-visible but NOT selected for projection. Keep
    generated/old low/reference/studio geometry hidden. AO rays still have their
    explicit distance, so conservative candidates cannot add distant shadows.
    """
    from mathutils import Vector
    bpy.context.view_layer.update()
    points=[receiver.matrix_world@Vector(c) for c in receiver.bound_box]
    lo=[min(p[i] for p in points)-distance for i in range(3)]
    hi=[max(p[i] for p in points)+distance for i in range(3)]
    exclude=set(source_names);out=[]
    for ob in bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE'].objects:
        if ob.type!='MESH' or ob.name in exclude:continue
        pts=[ob.matrix_world@Vector(c) for c in ob.bound_box]
        if all(max(p[i] for p in pts)>=lo[i] and min(p[i] for p in pts)<=hi[i] for i in range(3)):out.append(ob.name)
    return out
