"""Flatten approved Yard PBR graphs, including concrete relief, into two engine atlases.

blender -b APPROVED.blend -t 6 --python tools/art/prepare_construction_yard.py -- OUTDIR
The source is never saved. The resulting clean blend is the replay input for blender_c3m.py.
"""
from pathlib import Path
import bpy,sys,json,hashlib
import numpy as np

out=Path(sys.argv[sys.argv.index('--')+1]);out.mkdir(parents=True,exist_ok=True)
source=Path(bpy.data.filepath);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source_sha=sha(source)
assert source_sha=='d25c5b5211d6d0faab18da9b45c6179d75eb313f378462a25e5b4674ac9b0a23'
lows=[o for o in bpy.context.scene.objects if o.get('final_low') and o.type=='MESH']
assert len(lows)==211 and not any(o.modifiers for o in lows)
def geometry(objects):
    rows={}
    for o in objects:
        me=o.data;me.calc_loop_triangles()
        rows[o.name]=dict(triangles=len(me.loop_triangles),matrix=[list(v) for v in o.matrix_world],
            vertices=[list(v.co) for v in me.vertices],uv=[list(v.uv) for v in me.uv_layers['BakeUV'].data],
            normals=[list(v.vector) for v in me.corner_normals])
    return sha_bytes(json.dumps(rows,sort_keys=True).encode())
sha_bytes=lambda b:hashlib.sha256(b).hexdigest()
before=geometry(lows);assert sum(len(o.data.loop_triangles) for o in lows)==9820
scene=bpy.context.scene
bpy.data.batch_remove(ids=[o for o in bpy.data.objects if o not in lows])
print('YARD_CLEANED_OBJECTS',flush=True)
for s in list(bpy.data.scenes):
    if s!=scene:bpy.data.scenes.remove(s)
for o in lows:o.hide_render=False;o.hide_set(False)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=1
scene.render.threads_mode='FIXED';scene.render.threads=6
scene.render.bake.use_selected_to_active=False;scene.render.bake.use_clear=False
scene.render.bake.margin=8;scene.render.bake.normal_space='TANGENT'
scene.render.bake.normal_r='POS_X';scene.render.bake.normal_g='POS_Y';scene.render.bake.normal_b='POS_Z'
records=[];newmats={}
for atlas in ['Structure','Mechanisms']:
    objects=[o for o in lows if o['atlas_set']==atlas];materials={o.data.materials[0] for o in objects}
    for o in bpy.context.selected_objects:o.select_set(False)
    # One temporary joined receiver avoids rebuilding the Cycles scene per part.
    # Original objects remain untouched and are the final export geometry.
    copies=[]
    for o in lows:o.hide_render=True
    for o in objects:
        ob=o.copy();ob.data=o.data.copy();scene.collection.objects.link(ob);ob.hide_render=False;ob.hide_set(False)
        ob.select_set(True);ob.data.uv_layers.active=ob.data.uv_layers['BakeUV'];copies.append(ob)
    bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();receiver=bpy.context.view_layer.objects.active
    print('YARD_JOINED_RECEIVER',atlas,len(receiver.data.polygons),flush=True)
    saved={m:m.node_tree.nodes.get('Material Output') or next(n for n in m.node_tree.nodes if n.type=='OUTPUT_MATERIAL') for m in materials}
    links={m:saved[m].inputs['Surface'].links[0].from_socket for m in materials}
    images={}
    for channel in ['Normal','BaseColor','Roughness','Metallic','AO','PlayerColorMask']:
        stem='Yard_'+atlas+'_'+channel;path=out/(stem+'.png')
        image=bpy.data.images.new(stem+'_BAKE',width=2048,height=2048,alpha=True,float_buffer=True)
        image.colorspace_settings.name='Non-Color';image.generated_color=(.5,.5,1,1) if channel=='Normal' else (0,0,0,1)
        for m in materials:
            nodes=m.node_tree.nodes;node=nodes.get('CY_RUNTIME_BAKE_TARGET') or nodes.new('ShaderNodeTexImage');node.name='CY_RUNTIME_BAKE_TARGET';node.image=image;nodes.active=node
            for n in nodes:n.select=False
            node.select=True
            if channel=='Normal':m.node_tree.links.new(links[m],saved[m].inputs['Surface'])
            else:
                em=nodes.get('CY_RUNTIME_EMIT') or nodes.new('ShaderNodeEmission');em.name='CY_RUNTIME_EMIT'
                m.node_tree.links.new(nodes['CY_TEX_'+channel].outputs['Color'],em.inputs['Color'])
                m.node_tree.links.new(em.outputs[0],saved[m].inputs['Surface'])
        print('YARD_BAKE_START',atlas,channel,len(objects),flush=True)
        bpy.ops.object.bake(type='NORMAL' if channel=='Normal' else 'EMIT')
        values=np.empty(2048*2048*4,np.float32);image.pixels.foreach_get(values);values=values.reshape(2048,2048,4)
        assert np.isfinite(values).all()
        if channel=='BaseColor':
            rgb=values[:,:,:3];values[:,:,:3]=np.where(rgb<=.0031308,rgb*12.92,1.055*np.maximum(rgb,0)**(1/2.4)-.055)
        image.pixels.foreach_set(np.clip(values,0,1).ravel());image.update()
        image.filepath_raw=str(path);image.file_format='PNG';image.save()
        for m in materials:m.node_tree.nodes['CY_RUNTIME_BAKE_TARGET'].image=None
        bpy.data.images.remove(image)
        decoded=bpy.data.images.load(str(path),check_existing=False);decoded.name=stem
        decoded.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color';images[channel]=decoded
        records.append(dict(atlas=atlas,channel=channel,file=path.name,sha256=sha(path),dimensions=[2048,2048],colorspace=decoded.colorspace_settings.name))
        print('YARD_BAKE_DONE',atlas,channel,flush=True)
    bpy.data.objects.remove(receiver,do_unlink=True)
    for o in lows:o.hide_render=False
    for m in materials:m.node_tree.links.new(links[m],saved[m].inputs['Surface'])
    mat=bpy.data.materials.new('Yard_'+atlas);mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links;n.clear()
    output=n.new('ShaderNodeOutputMaterial');bs=n.new('ShaderNodeBsdfPrincipled');l.new(bs.outputs[0],output.inputs['Surface'])
    uv=n.new('ShaderNodeUVMap');uv.uv_map='BakeUV';tex={}
    for channel,im in images.items():
        node=n.new('ShaderNodeTexImage');node.name='CY_TEX_'+channel;node.image=im;l.new(uv.outputs[0],node.inputs['Vector']);tex[channel]=node
    normal=n.new('ShaderNodeNormalMap');normal.uv_map='BakeUV';l.new(tex['Normal'].outputs['Color'],normal.inputs['Color']);l.new(normal.outputs[0],bs.inputs['Normal'])
    for channel in ['Roughness','Metallic']:l.new(tex[channel].outputs['Color'],bs.inputs[channel])
    tint=n.new('ShaderNodeRGB');tint.name='CY_PLAYER_TINT';tint.outputs[0].default_value=(1,.68,.17,1)
    multiply=n.new('ShaderNodeMixRGB');multiply.blend_type='MULTIPLY';multiply.inputs[0].default_value=1
    l.new(tex['BaseColor'].outputs['Color'],multiply.inputs[1]);l.new(tint.outputs[0],multiply.inputs[2])
    mix=n.new('ShaderNodeMixRGB');l.new(tex['PlayerColorMask'].outputs['Color'],mix.inputs[0]);l.new(tex['BaseColor'].outputs['Color'],mix.inputs[1]);l.new(multiply.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],bs.inputs['Base Color'])
    for o in objects:o.data.materials.clear();o.data.materials.append(mat)
    newmats[atlas]=mat
assert geometry(lows)==before
for m in list(bpy.data.materials):
    if m not in newmats.values():bpy.data.materials.remove(m,do_unlink=True)
used={node.image for m in newmats.values() for node in m.node_tree.nodes if node.type=='TEX_IMAGE'}
for im in list(bpy.data.images):
    if im not in used:bpy.data.images.remove(im,do_unlink=True)
scene.name='Construction Yard runtime';scene['material_approval']='Trial04f approved for in-engine test by user';scene['runtime_integrated']=False
scene['source_sha256']=source_sha
dest=out/'ConstructionYard_Runtime01.blend'
for image in bpy.data.images:
    if image.source=='FILE':image.filepath='//'+Path(bpy.path.abspath(image.filepath)).name
bpy.ops.wm.save_as_mainfile(filepath=str(dest),compress=True)
assert sha(source)==source_sha
report=dict(source_sha256=source_sha,output=dest.name,output_sha256=sha(dest),geometry_sha256=before,meshes=211,triangles=9820,
            basis='Approved Trial04f; bake final shader tangent normals, including concrete and coating relief; all six channels from live assigned material for every receiver',maps=records)
(out/'bake_manifest.json').write_text(json.dumps(report,indent=2));print('YARD_RUNTIME_READY',flush=True)
