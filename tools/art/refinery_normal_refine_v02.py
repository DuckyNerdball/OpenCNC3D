"""Spatially isolated physical bake batches; immutable production UV/normals.

Temporary world-space copies preserve corners and UVs; 70-unit translations
separate each receiver and its named high sources. AO context is unselected.
"""
import bpy,sys,json,hashlib,struct,time
from pathlib import Path
from collections import defaultdict
import numpy as np
R=Path(__file__).resolve().parents[2];O=R/'art/refinery/texture_v02'
sys.path.insert(0,str(R/'tools/art'))
import refinery_bake_common_v02 as policy
from refinery_material_full_v02 import join,write_png
import refinery_uv_audit as audit
from refinery_cages_v02 import create_batch
CAGES=json.loads((O/"CAGE_POLICY.json").read_text())["receivers"]
RES=4096;SOURCE=O/'PROC_TextureUV_v02.blend';SHA=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(SOURCE));s=bpy.context.scene
low=sorted(bpy.data.collections['PROC_FinalLow_v02_RUNTIME'].objects,key=lambda o:o.name)
policy.assert_closed_orientation(low);frozen=audit.fingerprint(low)
def fingerprint():
 h=hashlib.sha256()
 for o in low:
  h.update(o.name.encode())
  for v in o.data.vertices:h.update(struct.pack('<3f',*v.co))
  for f in o.data.polygons:h.update(struct.pack('<3I',*f.vertices));h.update(struct.pack('<?',f.use_smooth))
  for n in o.data.corner_normals:h.update(struct.pack('<3f',*n.vector))
  for uv in o.data.uv_layers['BakeUV'].data:h.update(struct.pack('<2f',*uv.uv))
  h.update(np.asarray(o.matrix_world,np.float64).tobytes())
 return h.hexdigest()
geometry_hash=fingerprint();mapping=policy.source_mapping()['receiver_to_sources']
policy.prepare_visibility();policy.configure_bake(s,RES,8,4)
temp=bpy.data.collections.new('PROC_PHYSICAL_BATCH_NONEXPORT');s.collection.children.link(temp)
srcmat=bpy.data.materials.new('PROC_SOURCE_PHYSICAL_ONLY');srcmat.use_nodes=True
nodes=srcmat.node_tree.nodes;links=srcmat.node_tree.links;bs=nodes.get('Principled BSDF');out=nodes.get('Material Output')
ao=nodes.new('ShaderNodeAmbientOcclusion');ao.samples=8;ao.inputs['Distance'].default_value=.65
emit=nodes.new('ShaderNodeEmission');links.new(ao.outputs['Color'],emit.inputs['Color'])
dest=bpy.data.materials.new('PROC_DEST_PHYSICAL_ONLY');dest.use_nodes=True;target=dest.node_tree.nodes.new('ShaderNodeTexImage');dest.node_tree.nodes.active=target
canvas={ch:np.ones((RES,RES,4),np.float32) for ch in ['Normal','AO']};canvas['Normal'][:,:,:2]=.5
images={ch:bpy.data.images.new('PROC_PHYSICAL_'+ch,RES,RES,float_buffer=True,alpha=True) for ch in canvas}
for im in images.values():im.colorspace_settings.name='Non-Color'
groups=defaultdict(list)
for o in low:groups[(*policy.cage_for(o.name),CAGES[o.name]["explicit_cage"])].append(o)
batches=json.loads((O/'BATCH_PLAN.json').read_text())['batches']
rows=[];batchrows=[];read=np.empty((RES,RES,4),np.float32);start=time.time()
if (O/'PHYSICAL_PROGRESS.json').exists():
 previous=json.loads((O/'PHYSICAL_PROGRESS.json').read_text())
 assert all(r['uv_source_sha256']==SHA(SOURCE) for r in previous['receivers'])
 data=np.load(O/'PHYSICAL_PROGRESS.npz')
 for ch in canvas:canvas[ch]=data[ch].copy()
 rows=previous['receivers'];batchrows=previous['completed_batches']
 print('RESUMED_PHYSICAL_BATCHES',len(batchrows),flush=True)
for batch_id,batch in enumerate(batches):
 if batch_id not in (5,6,7):continue
 ext,dist,explicit=batch['extrusion'],batch['distance'],batch['explicit_cage'];receivers=[bpy.data.objects[e['receiver']] for e in batch['entries']]
 print('PHYSICAL_BATCH_START',batch_id+1,len(batches),len(receivers),flush=True)
 high_entries=[];low_entries=[];context_entries=[];contexts={}
 for i,o in enumerate(receivers):
  offset=batch['entries'][i]['offset'];low_entries.append((o,offset,dest))
  high_entries.extend((bpy.data.objects[n],offset,srcmat) for n in mapping[o.name])
  ctx=[];contexts[o.name]=ctx
  context_entries.extend((bpy.data.objects[n],offset,srcmat) for n in ctx)
 hs,hr=join('PROC_BATCH_HIGH',high_entries,temp);ls,lr=join('PROC_BATCH_LOW',low_entries,temp,True)
 context,cr=join('PROC_BATCH_AO_CONTEXT',context_entries,temp) if context_entries else (None,None)
 assert lr['normal_encoding_max_vector_error']<.001,lr
 assert hr['normal_encoding_max_vector_error']<.001,hr
 cage=create_batch(ls,low_entries,ext,temp) if explicit else None
 s.render.bake.use_cage=explicit;s.render.bake.cage_object=cage
 for o in s.objects:o.hide_render=True;o.select_set(False)
 for o in (hs,ls):o.hide_render=False;o.hide_set(False);o.select_set(True)
 bpy.context.view_layer.objects.active=ls;s.render.bake.cage_extrusion=ext;s.render.bake.max_ray_distance=dist
 saved=np.asarray([v.uv[:] for v in ls.data.uv_layers['BakeUV'].data],np.float32)
 offsets=[(-.25,-.25),(.25,-.25),(-.25,.25),(.25,.25)] if True else [(0,0)]
 channelrows={}
 for channel in ['Normal']:
  t=time.time();im=images[channel];target.image=im;dest.node_tree.nodes.active=target
  links.new(bs.outputs['BSDF'] if channel=='Normal' else emit.outputs[0],out.inputs['Surface'])
  if context:context.hide_render=channel!='AO'
  accumulated=np.zeros_like(read);baseline=canvas[channel]
  for dx,dy in offsets:
   ls.data.uv_layers['BakeUV'].data.foreach_set('uv',(saved+np.array([dx/RES,dy/RES],np.float32)).ravel());ls.data.update()
   im.pixels.foreach_set(baseline.ravel());im.update();bpy.ops.object.bake(type='NORMAL' if channel=='Normal' else 'EMIT');im.pixels.foreach_get(read.ravel());accumulated+=read/len(offsets)
  ls.data.uv_layers['BakeUV'].data.foreach_set('uv',saved.ravel());ls.data.update()
  if channel=='Normal':
   vec=accumulated[:,:,:3]*2-1;vec/=np.maximum(np.linalg.norm(vec,axis=2,keepdims=True),1e-8);accumulated[:,:,:3]=vec*.5+.5
  assert np.isfinite(accumulated).all();canvas[channel]=accumulated
  channelrows[channel]={'seconds':time.time()-t};print('PHYSICAL_BATCH_PASS',batch_id,channel,round(time.time()-t,2),flush=True)
 for o in receivers:rows.append({'receiver':o.name,'sources':mapping[o.name],'extrusion':ext,'max_ray_distance':dist,'ao_context_names':contexts[o.name],'subtexel_offsets':offsets,'batch':batch_id,'explicit_cage':explicit,'uv_source_sha256':SHA(SOURCE)})
 batchrows.append({'batch':batch_id,'source_join':hr,'receiver_join':lr,'channels':channelrows,'context_objects':len(context_entries)})
 s.render.bake.cage_object=None
 for o in [hs,ls]+([context] if context else [])+([cage] if cage else []):
  me=o.data;bpy.data.objects.remove(o,do_unlink=True);bpy.data.meshes.remove(me)
assert audit.fingerprint(low)==frozen and fingerprint()==geometry_hash
files=[]
for ch,values in [('Normal',canvas['Normal'])]:
 p=O/('PROC_Working_'+ch+'.png');write_png(p,values)
 im=bpy.data.images.load(str(p),check_existing=False);im.colorspace_settings.name='Non-Color';im.pixels.foreach_get(read.ravel());error=float(np.max(np.abs(read-values)));assert error<2/65535
 files.append({'channel':ch,'path':str(p.relative_to(R)),'sha256':SHA(p),'resolution':[RES,RES],'bit_depth':16,'colorspace':'Non-Color','fresh_decode_max_error':error})
report=json.loads((O/'PHYSICAL_BAKE_REPORT.json').read_text())
report['output_maps']=[files[0] if row['channel']=='Normal' else row for row in report['output_maps']]
refinement={'reason':'Four subpixel normal samples for every rim/clamp batch to improve fine-edge antialiasing; geometry/UV/source normals remain unchanged.','batch_ids':[5,6,7],'offsets':[[-.25,-.25],[.25,-.25],[-.25,.25],[.25,.25]],'initial_evidence':'physical_initial/PHYSICAL_BAKE_REPORT.json','new_batches':batchrows[14:]}
report['normal_refinement']=refinement
for row in report['receivers']:
 if row['batch'] in (5,6,7):row['normal_subtexel_offsets']=refinement['offsets']
(O/'PHYSICAL_BAKE_REPORT.json').write_text(json.dumps(report,indent=2)+'\n')
(O/'NORMAL_REFINEMENT_REPORT.json').write_text(json.dumps(refinement,indent=2)+'\n')
print('PROC_NORMAL_REFINEMENT_COMPLETE',flush=True)
