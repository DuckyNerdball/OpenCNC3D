"""Persist every v03 source graph at its original rest-world coordinates."""
import bpy,sys,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v03';sys.path.insert(0,str(ROOT/'tools/art'))
import refinery_material_graph_v03 as graph
import refinery_uv_audit as audit
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_TextureUV_v02.blend'))
objects=sorted([o for o in bpy.data.collections['PROC_FinalLow_v02_RUNTIME'].objects if o.type=='MESH'],key=lambda o:o.name);before=audit.fingerprint(objects)
mapping=json.loads((OUT/'SOURCE_RECEIVER_MAP.json').read_text())['receiver_to_sources'];names=sorted({n for row in mapping.values() for n in row})
pigment=bpy.data.images.load(str(OUT/'source/PROC_Ochre_Pigment_v01.png'));pigment.colorspace_settings.name='sRGB';pigment.pack();records=[]
for i,name in enumerate(names):
 src=bpy.data.objects[name];role=graph.role_for_source(name,src.data.materials[0].name);g=graph.build('PROC_V03_SOURCE_'+name,role,pigment,src)
 for ch,socket in g.channels.items():
  n=g.node('NodeReroute');n.name='CHANNEL_'+ch;n.label=ch;g.set(n.inputs[0],socket)
 g.set(g.emit.inputs['Color'],g.channels['BaseColor'])
 g.mat['microheight_note']='Coating loss only; concrete uses exact shared Yard normal detail in the ConcreteDetailDelta channel.'
 g.mat['authoring_coordinates']='Original rest-world source, zero batching translation. Channel reroutes expose all bake outputs.'
 records.append({'source':name,'material':g.mat.name,'role':role,'edge_segments':len(json.loads(g.mat['physical_edge_segments']))})
 if i%100==0:print('SOURCE_GRAPH_LIBRARY',i,len(names),flush=True)
assert audit.fingerprint(objects)==before
f=OUT/'PROC_MaterialAuthoring_v03.blend';bpy.ops.wm.save_as_mainfile(filepath=str(f),compress=True)
(OUT/'SOURCE_MATERIAL_LIBRARY.json').write_text(json.dumps({'blend':str(f.relative_to(ROOT)),'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'graphs':records,'geometry_unchanged':True},indent=2)+'\n');print('SOURCE_LIBRARY_COMPLETE',flush=True)
