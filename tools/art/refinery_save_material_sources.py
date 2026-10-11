"""Preserve editable material source graphs with explicit fake users and fresh-reopen proof."""
import bpy,sys,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v01';sys.path.insert(0,str(Path(__file__).resolve().parent))
import refinery_material_graph as graph
import refinery_bake_common as policy
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_TextureUV_v01.blend'));pigment=bpy.data.images.load(str(OUT/'source/PROC_Ochre_Pigment_v01.png'),check_existing=False);pigment.colorspace_settings.name='sRGB';pigment.pack()
mapping=policy.source_mapping()['receiver_to_sources'];rows=[]
for name in sorted({n for names in mapping.values() for n in names}):
 src=bpy.data.objects[name];role=graph.role_for_source(name,src.data.materials[0].name);g=graph.build('PROC_TRIAL_SOURCE_'+name,role,pigment,src)
 g.mat.use_fake_user=True;g.set(g.emit.inputs['Color'],g.channels['BaseColor']);g.mat['source_preview_channel']='Clean neutral BaseColor; player tint belongs in mapped review shader'
 rows.append({'source':name,'role':role,'material':g.mat.name,'fake_user':True})
file=OUT/'PROC_MaterialSources_v01.blend';bpy.ops.wm.save_as_mainfile(filepath=str(file),compress=True)
bpy.ops.wm.open_mainfile(filepath=str(file));actual={m.name for m in bpy.data.materials if m.name.startswith('PROC_TRIAL_SOURCE_')};assert actual=={r['material'] for r in rows}
report={'source_graphs':rows,'fresh_reopen_count':len(actual),'source_graph_code_sha256':hashlib.sha256((ROOT/'tools/art/refinery_material_graph.py').read_bytes()).hexdigest(),'file':str(file.relative_to(ROOT)),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'preview':'Editable clean BaseColor source graphs; original low meshes remain provisional here. Actual mapped trial is a separate packed file.'}
(OUT/'MATERIAL_SOURCE_GRAPHS.json').write_text(json.dumps(report,indent=2)+'\n');print('PROC_SOURCE_GRAPHS_PRESERVED',len(actual),flush=True)
