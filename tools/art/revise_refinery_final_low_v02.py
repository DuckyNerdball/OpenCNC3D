"""Restore actual high-led silo rim chamfers; retain full triangular sheet layout.
Mesh review only, no new production UVs or bakes before renewed approval.
"""
import bpy,bmesh,math,json,hashlib,sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/final_low_v02';SOURCE=ROOT/'art/refinery/final_low_v01/PROC_FinalLow_v01.blend';SHA='99890ef345b0df1f2ba148cc12dd1601dbf03bb29f2590ade9dfb7f473062a08'
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA
bpy.ops.wm.open_mainfile(filepath=str(SOURCE));scene=bpy.context.scene;low=bpy.data.collections['PROC_FinalLow_v01_RUNTIME'];low.name='PROC_FinalLow_v02_RUNTIME';high=bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE'];bpy.context.view_layer.layer_collection.children[high.name].exclude=True
records=[];sheet_rows=[]

def sig(o):
 import struct
 return hashlib.sha256(b''.join(struct.pack('3f',*v.co) for v in o.data.vertices)+b''.join(struct.pack('3f',*n.vector) for n in o.data.corner_normals)).hexdigest()
before={o.name:{'hash':sig(o),'triangles':len(o.data.polygons),'matrix':[list(r) for r in o.matrix_world]} for o in low.objects}

def finish(o,vs,fs,flat=False):
 old=o.data;me=bpy.data.meshes.new(o.name+'_chamfer_v02');me.from_pydata(vs,[],fs);me.update()
 for m in old.materials:me.materials.append(m)
 o.data=me;bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(me);bm.free()
 for f in me.polygons:f.use_smooth=not flat
 me.set_sharp_from_angle(angle=math.radians(35))
 bpy.context.view_layer.objects.active=o
 w=o.modifiers.new('Planar_normal_control','WEIGHTED_NORMAL');w.keep_sharp=True;w.weight=50;bpy.ops.object.modifier_apply(modifier=w.name)
 tr=o.modifiers.new('Frozen_runtime_triangles','TRIANGULATE');tr.keep_custom_normals=True;bpy.ops.object.modifier_apply(modifier=tr.name)
 if old.users==0:bpy.data.meshes.remove(old)

def inset(poly,w):
 area=sum(a.x*b.y-b.x*a.y for a,b in zip(poly,poly[1:]+poly[:1]));assert area>0
 lines=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  d=(b-a).normalized();lines.append((a+Vector((-d.y,d.x))*w,d))
 pts=[]
 for i,(p,d) in enumerate(lines):
  q,e=lines[i-1];den=d.x*e.y-d.y*e.x;assert abs(den)>1e-8;delta=q-p;t=(delta.x*e.y-delta.y*e.x)/den;pts.append(p+d*t)
 return pts

def top_chamfer(o,src,w):
 # Raw source's top cap supplies the original ordered footprint and lower seat.
 me=src.data;zs=[v.co.z for v in me.vertices];z0,z1=min(zs),max(zs)
 cap=max([f for f in me.polygons if all(abs(me.vertices[i].co.z-z1)<1e-5 for i in f.vertices)],key=lambda f:len(f.vertices))
 poly=[Vector(me.vertices[i].co[:2]) for i in cap.vertices]
 if sum(a.x*b.y-b.x*a.y for a,b in zip(poly,poly[1:]+poly[:1]))<0:poly.reverse()
 if '_rim_panel_' in o.name:
  # Low sector has four rows of three vertices: bottom inner/outer,
  # top inner/outer. Preserve that exact ordered concave annular outline.
  assert len(o.data.vertices)==12,(o.name,len(o.data.vertices))
  poly=[Vector(o.data.vertices[i].co[:2]) for i in [9,10,11,8,7,6]]
  if sum(a.x*b.y-b.x*a.y for a,b in zip(poly,poly[1:]+poly[:1]))<0:poly.reverse()
 n=len(poly);inner=inset(poly,w);assert z1-z0>w*2
 vs=[(p.x,p.y,z) for z,ring in [(z0,poly),(z1-w,poly),(z1,inner)] for p in ring]
 fs=[tuple(reversed(range(n))),tuple(range(2*n,3*n))]+[(j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i) for j in range(2) for i in range(n)]
 finish(o,vs,fs)
 scale=o.matrix_world.to_scale();worldcuts=[]
 for i,p in enumerate(poly):
  a=poly[i];b=poly[(i+1)%n];t=(b-a).normalized();normal=Vector((-t.y,t.x,0));offset=Vector((inner[i].x-a.x,inner[i].y-a.y,0));worldcuts.append(abs((o.matrix_world.to_3x3()@offset).dot((o.matrix_world.to_3x3()@normal).normalized())))
 return {'nominal_source_bevel':w,'measured_horizontal_setbacks_world':worldcuts,'vertical_cut_world':w*scale.z,'footprint_vertices':n,'seat_z_local':z0,'top_z_local':z1,'method':'Explicit top chamfer ring; original bottom seat and outer side contour retained; no overlap clamping'}

for o in list(low.objects):
 n=o.name;source=o['high_source'];src=bpy.data.objects[source];kind=None;detail={}
 if '_rim_clamp_' in n or '_rim_panel_' in n:
  width=float(next(m.width for m in src.modifiers if m.type=='BEVEL'));detail=top_chamfer(o,src,width);kind='retained visible top chamfer from high'
 elif '_continuous_sealed_wall' in n:
  # Preserve original inner-well sampling: coarse inner chords would
  # occlude the fitted light inner rim panels. Fund those samples by removing
  # two outer shoulder rings that sit behind the retained cladding/rim.
  count=16 if 'Rear_' in n else 12 if 'middle_' in n else 10
  pts=src.data.vertices;oldn=128;assert len(pts)==oldn*7
  spec=[(0,count),(1,count),(4,count),(5,count*2),(6,count*2)];vs=[];rings=[]
  for row,segments in spec:
   ring=[pts[row*oldn+i].co for i in range(oldn)];c=sum(ring,Vector())/oldn;r=math.hypot(ring[0].x-c.x,ring[0].y-c.y);start=math.atan2(ring[0].y-c.y,ring[0].x-c.x)
   rings.append((len(vs),segments))
   vs += [(c.x+r*math.cos(start+2*math.pi*i/segments),c.y+r*math.sin(start+2*math.pi*i/segments),c.z) for i in range(segments)]
  fs=[]
  for j,(aa,na) in enumerate(rings):
   bb,nb=rings[(j+1)%len(rings)]
   if na==nb:
    fs += [(aa+i,aa+(i+1)%na,bb+(i+1)%nb,bb+i) for i in range(na)]
   elif nb==na*2:
    for i in range(na):
     a0=aa+i;a1=aa+(i+1)%na;b0=bb+2*i;b1=bb+(2*i+1)%nb;b2=bb+(2*i+2)%nb
     fs += [(a0,a1,b1),(a1,b2,b1),(a0,b1,b0)]
   else:
    assert na==nb*2
    for i in range(nb):
     b0=bb+i;b1=bb+(i+1)%nb;a0=aa+2*i;a1=aa+(2*i+1)%na;a2=aa+(2*i+2)%na
     fs += [(a0,a1,b0),(a1,b1,b0),(a1,a2,b1)]
  finish(o,vs,fs);kind='reallocate covered outer shoulder rings; preserve original inner-well fit';detail={'outer_radial_segments':count,'inner_radial_segments':count*2,'retained_source_profile_rows':[0,1,4,5,6],'removed_profile_rows':[2,3],'removed_rows_visibility':'covered by outer cladding and segmented rim; outer radius shift below .075 local units'}
 elif n=='LOW_Dock_roof_fan_seat':
  pts=[v.co for v in src.data.vertices];rs=[math.hypot(p.x,p.y) for p in pts];major=(max(rs)+min(rs))*.5;minor=(max(rs)-min(rs))*.5;N=16
  vs=[((major+minor*math.cos(2*math.pi*j/4))*math.cos(2*math.pi*i/N),(major+minor*math.cos(2*math.pi*j/4))*math.sin(2*math.pi*i/N),minor*math.sin(2*math.pi*j/4)) for i in range(N) for j in range(4)]
  fs=[(i*4+j,((i+1)%N)*4+j,((i+1)%N)*4+(j+1)%4,i*4+(j+1)%4) for i in range(N) for j in range(4)]
  finish(o,vs,fs);kind='small roof fan ring 20 to 16 radial segments; same radius/profile';detail={'radial_segments':16,'tube_segments':4}
 if kind:records.append({'object':n,'source':source,'change':kind,'triangles_before':before[n]['triangles'],'triangles_after':len(o.data.polygons),**detail})
 # Complete 360-degree visible sheet-layout guide on actual outer triangles.
 if 'faceted_cladding' in n:
  expected=96 if 'Rear_' in n else 72 if 'middle_' in n else 40
  mats=[]
  for k,color in enumerate([(.33,.36,.37,1),(.255,.285,.30,1)]):
   mat=bpy.data.materials.new('REVIEW_ONLY_TriangleLayout_'+n+'_'+str(k));mat.use_nodes=True;nodes=mat.node_tree.nodes;links=mat.node_tree.links;bs=nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=color;bs.inputs['Metallic'].default_value=.25;bs.inputs['Roughness'].default_value=.6
   wire=nodes.new('ShaderNodeWireframe');wire.inputs['Size'].default_value=.035;wire.use_pixel_size=False;mix=nodes.new('ShaderNodeMixRGB');mix.inputs[1].default_value=color;mix.inputs[2].default_value=(.045,.055,.06,1);links.new(wire.outputs['Fac'],mix.inputs[0]);links.new(mix.outputs[0],bs.inputs['Base Color']);mat['purpose']='Mesh triangle-layout review guide, not shipped texture or bake proof';mats.append(mat)
  o.data.materials.clear()
  for m in mats:o.data.materials.append(m)
  for f in o.data.polygons:f.material_index=f.index%2 if f.index<expected else 1
  sheet_rows.append({'object':n,'outer_sheet_triangles':expected,'whole_circumference_bays':16 if 'Rear_' in n else 12 if 'middle_' in n else 10,'tiers':3 if 'front_' not in n else 2,'review_material':'Temporary edge/ID guide on actual frozen outer triangles, to make complete coverage visible without new UV/bakes.'})
count=sum(len(o.data.polygons) for o in low.objects);assert count<=10000,count
assert count==9992,count
for o in low.objects:
 assert [list(r) for r in o.matrix_world]==before[o.name]['matrix']
 if o.name not in {r['object'] for r in records}:assert sig(o)==before[o.name]['hash'],o.name
scene['final_low_review_state']='FinalLow v02 actual chamfer revision, pending renewed mesh approval. Approved material direction retained separately.';scene['runtime_triangle_ceiling']=10000;scene['triangle_layout_review_guides']='Temporary non-delivery wire/ID material on 208 real outer triangles; not baked plating proof.'
bpy.context.preferences.filepaths.save_version=0;dest=OUT/'PROC_FinalLow_v02.blend';bpy.ops.wm.save_as_mainfile(filepath=str(dest),compress=True)
manifest={'source':str(SOURCE.relative_to(ROOT)),'source_sha256':SHA,'file':str(dest.relative_to(ROOT)),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'runtime_triangles':count,'runtime_meshes':len(low.objects),'triangle_ceiling':10000,'changed_geometry':records,'complete_sheet_layout':sheet_rows,'unchanged_geometry_objects':len(low.objects)-len(records),'source_file_unchanged':hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SHA,'stage':'Renewed actual final-low mesh review; no production UV or bakes on revised geometry.'}
(OUT/'BUILD_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n');print('PROC_V02_BUILT',count,len(records),flush=True)
