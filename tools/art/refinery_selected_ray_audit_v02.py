"""Evaluate actual 4K texel-center rays, explicitly separating hidden shell closures."""
import bpy,sys,json,hashlib
from pathlib import Path
from collections import Counter
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v02';RES=4096
sys.path.insert(0,str(Path(__file__).resolve().parent));import refinery_bake_common_v02 as policy
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_TextureUV_v02.blend'));policy.prepare_visibility();deps=bpy.context.evaluated_depsgraph_get();high=bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE']
triangles=0
for o in high.objects:
 if o.type=='MESH':
  eo=o.evaluated_get(deps);me=eo.to_mesh();me.calc_loop_triangles();triangles+=len(me.loop_triangles);eo.to_mesh_clear()
assert triangles==454428,triangles
from refinery_cages_v02 import offsets as cage_offsets
mapping=policy.source_mapping()['receiver_to_sources'];reports=[];closures=[]
cage_policy=json.loads((OUT/'CAGE_POLICY.json').read_text())['receivers']
vs_all=[];fs_all=[];owner_all=[]
for ob in bpy.data.collections['PROC_FinalLow_v02_RUNTIME'].objects:
 off=len(vs_all);vs_all.extend(ob.matrix_world@v.co for v in ob.data.vertices);fs_all.extend(tuple(off+i for i in f.vertices) for f in ob.data.polygons);owner_all.extend([ob.name]*len(ob.data.polygons))
low_tree=BVHTree.FromPolygons(vs_all,fs_all)
for name,names in mapping.items():
 o=bpy.data.objects[name];local_offsets=cage_offsets(o.data,policy.cage_for(name)[0]);vs=[];fs=[];owners=[]
 for n in names:
  src=bpy.data.objects[n];eo=src.evaluated_get(deps);me=eo.to_mesh();off=len(vs);vs.extend(eo.matrix_world@v.co for v in me.vertices);fs.extend(tuple(off+i for i in f.vertices) for f in me.polygons);owners.extend([n]*len(me.polygons));eo.to_mesh_clear()
 tree=BVHTree.FromPolygons(vs,fs);ext,dist=policy.cage_for(name);uv=o.data.uv_layers['BakeUV'].data;nm=o.matrix_world.inverted().transposed().to_3x3();hits=0;miss=[];opposite=[];by_source=Counter();total=0
 for f in o.data.polygons:
  hidden='faceted_cladding' in name and f.index>=(96 if 'Rear_' in name else 72 if 'middle_' in name else 40)
  if hidden:closures.append({'object':name,'polygon':f.index,'reason':'Closed shell inner wall or end-cap; interior behind outer cladding, rim/base bands. Not an exposed source-transfer surface.'});continue
  p=np.asarray([uv[i].uv for i in f.loop_indices]);lo=np.maximum(np.ceil(p.min(0)*RES-.5).astype(int),0);hi=np.minimum(np.floor(p.max(0)*RES-.5).astype(int),RES-1)
  if np.any(hi<lo):continue
  aa,bb,cc=p;den=(bb[1]-cc[1])*(aa[0]-cc[0])+(cc[0]-bb[0])*(aa[1]-cc[1]);xx,yy=np.meshgrid(np.arange(lo[0],hi[0]+1),np.arange(lo[1],hi[1]+1));u=(xx+.5)/RES;v=(yy+.5)/RES
  wa=((bb[1]-cc[1])*(u-cc[0])+(cc[0]-bb[0])*(v-cc[1]))/den;wb=((cc[1]-aa[1])*(u-cc[0])+(aa[0]-cc[0])*(v-cc[1]))/den;wc=1-wa-wb;inside=np.flatnonzero((wa>=0)&(wb>=0)&(wc>=0));indices=inside[np.linspace(0,len(inside)-1,min(64,len(inside)),dtype=int)] if len(inside) else []
  points=[o.matrix_world@o.data.vertices[i].co for i in f.vertices];norms=[(nm@o.data.corner_normals[i].vector).normalized() for i in f.loop_indices]
  for ix in indices:
   weights=[wa.ravel()[ix],wb.ravel()[ix],wc.ravel()[ix]];pos=sum((p*float(w) for p,w in zip(points,weights)),Vector());normal=sum((n*float(w) for n,w in zip(norms,weights)),Vector()).normalized();cage_delta=sum(((o.matrix_world.to_3x3()@local_offsets[vi])*float(w) for vi,w in zip(f.vertices,weights)),Vector()) if cage_policy[name]['explicit_cage'] else normal*ext;direction=cage_delta.normalized();hit,hn,fi,d=tree.ray_cast(pos+cage_delta,-direction,dist);total+=1
   row={'polygon':f.index,'pixel':[int(xx.ravel()[ix]),int(yy.ravel()[ix])]}
   if hit is None:
    cover,_,ci,cd=low_tree.ray_cast(pos+normal*.001,normal,.75)
    row.update(world_position=list(pos),occluded_by=owner_all[ci] if cover is not None and owner_all[ci]!=name else None,occlusion_distance=cd if cover is not None else None)
    miss.append(row)
   else:
    hits+=1;by_source[owners[fi]]+=1
    if hn.dot(normal)<0:row.update(source=owners[fi],dot=float(hn.dot(normal)));opposite.append(row)
 reports.append({'receiver':name,'texel_center_samples':total,'hits':hits,'misses':len(miss),'opposite_facing':len(opposite),'miss_examples':miss,'occluded_misses':sum(bool(r.get('occluded_by')) for r in miss),'opposite_examples':opposite[:40],'source_owners':dict(by_source)})
 print('PROC_TEXEL_RAYS',name,total,'miss',len(miss),'opposite',len(opposite),flush=True)
report={'method':'Deterministic distributed actual 4096px texel-center samples, up to64 per exposed triangle, evaluated named source BVH, interpolated receiver corner normals and explicit averaged-face miter cage policy. Independent approximation to Cycles, not internal Cycles hit telemetry.','evaluated_high_triangles':triangles,'receivers':reports,'hidden_closure_faces':closures,'exposed_misses':sum(r['misses'] for r in reports),'exposed_opposite_facing':sum(r['opposite_facing'] for r in reports)}
(OUT/'TEXEL_RAY_SELECTED_DIAGNOSTICS.json').write_text(json.dumps(report,indent=2)+'\n')
