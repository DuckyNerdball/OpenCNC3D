"""BaseColor-only stencil contrast revision; preserve all other PBR channels."""
import bpy,sys,json,time,hashlib
import numpy as np
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v03';sys.path.insert(0,str(ROOT/'tools/art'))
from refinery_material_full_v03 import join,write_png
import refinery_material_graph_v03 as graph
import refinery_bake_common_v02 as policy
from refinery_cages_v02 import create_batch
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_MaterialAuthoring_v03.blend'));s=bpy.context.scene;policy.prepare_visibility();policy.configure_bake(s,4096,1,6)
plan=json.loads((OUT/'BATCH_PLAN.json').read_text())['batches'][10];mapping=policy.source_mapping()['receiver_to_sources'];temp=bpy.data.collections.new('ARROW_CONTRAST_TEMP');s.collection.children.link(temp)
dest=bpy.data.materials.new('ARROW_CONTRAST_DEST');dest.use_nodes=True;target=dest.node_tree.nodes.new('ShaderNodeTexImage');dest.node_tree.nodes.active=target
im=bpy.data.images.load(str(OUT/'PROC_Full_BaseColor.png'),check_existing=False);im.colorspace_settings.name='sRGB';before=np.empty(4096*4096*4,np.float32);im.pixels.foreach_get(before);bpy.data.images.remove(im)
im=bpy.data.images.new('ARROW_CONTRAST_BUFFER',4096,4096,alpha=True,float_buffer=True);im.colorspace_settings.name='Non-Color';im.pixels.foreach_set(before);im.update();target.image=im
lowentries=[];highentries=[];graphs=[];sources=[]
for entry in plan['entries']:
 receiver=bpy.data.objects[entry['receiver']];offset=entry['offset'];lowentries.append((receiver,offset,dest))
 for name in mapping[receiver.name]:
  src=bpy.data.objects[name];role=graph.role_for_source(name,src.data.materials[0].name);g=graph.build('ARROW_BAKE_'+name,role,None,src,offset);g.output('BaseColor',im);graphs.append(g);highentries.append((src,offset,g.mat));sources.append((src,role))
high,hr=join('ARROW_CONTRAST_HIGH',highentries,temp);low,lr=join('ARROW_CONTRAST_LOW',lowentries,temp,True)
cage=create_batch(low,lowentries,plan['extrusion'],temp) if plan['explicit_cage'] else None
s.render.bake.use_cage=plan['explicit_cage'];s.render.bake.cage_object=cage;s.render.bake.cage_extrusion=plan['extrusion'];s.render.bake.max_ray_distance=plan['distance']
for o in (high,low):o.hide_render=False;o.hide_set(False);o.select_set(True)
bpy.context.view_layer.objects.active=low;bpy.ops.object.bake(type='EMIT');after=np.empty_like(before);im.pixels.foreach_get(after)
path=OUT/'PROC_Full_BaseColor.png';previous=hashlib.sha256(path.read_bytes()).hexdigest();write_png(path,after.reshape(4096,4096,4),True)
s.render.bake.cage_object=None
for o in [high,low]+([cage] if cage else []):m=o.data;bpy.data.objects.remove(o,do_unlink=True);bpy.data.meshes.remove(m)
for g in graphs:bpy.data.materials.remove(g.mat,do_unlink=True)
bpy.data.collections.remove(temp);bpy.data.materials.remove(dest,do_unlink=True);bpy.data.images.remove(im)
source_names={src.name for src,_ in sources}
for mat in list(bpy.data.materials):
 if mat.get('source_original') in source_names:bpy.data.materials.remove(mat,do_unlink=True)
for src,role in sources:
 g=graph.build('PROC_V03_SOURCE_'+src.name,role,None,src)
 for ch,socket in g.channels.items():n=g.node('NodeReroute');n.name='CHANNEL_'+ch;n.label=ch;g.set(n.inputs[0],socket)
 g.set(g.emit.inputs['Color'],g.channels['BaseColor']);g.mat['microheight_note']='Coating loss only; concrete uses exact shared Yard normal detail in ConcreteDetailDelta.'
for o in bpy.data.collections['PROC_FinalLow_v02_RUNTIME'].objects:o.hide_render=False
bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE'].hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'PROC_MaterialAuthoring_v03.blend'),compress=True)
report=json.loads((OUT/'MATERIAL_AUTHORING_REPORT.json').read_text());report['files']['BaseColor']['sha256']=hashlib.sha256(path.read_bytes()).hexdigest();report['arrow_contrast_revision']={'previous_basecolor_sha256':previous,'new_basecolor_sha256':report['files']['BaseColor']['sha256'],'paint':'8A8B80 sRGB','scope':'Approach arrows only; stronger contrast over the exact shared dark Yard concrete. All non-BaseColor maps retained byte-for-byte.','changed_linear_pixels_above_1e_4':int(np.sum(np.max(np.abs(after.reshape(-1,4)[:,:3]-before.reshape(-1,4)[:,:3]),axis=1)>.0001))};(OUT/'MATERIAL_AUTHORING_REPORT.json').write_text(json.dumps(report,indent=2)+'\n')
r=json.loads((OUT/'SOURCE_MATERIAL_LIBRARY.json').read_text());r['sha256']=hashlib.sha256((OUT/'PROC_MaterialAuthoring_v03.blend').read_bytes()).hexdigest();(OUT/'SOURCE_MATERIAL_LIBRARY.json').write_text(json.dumps(r,indent=2)+'\n')
print('ARROW_CONTRAST_COMPLETE',report['arrow_contrast_revision'],flush=True)
