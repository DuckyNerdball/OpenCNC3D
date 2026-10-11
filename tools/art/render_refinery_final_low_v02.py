"""Read-only actual mesh review; triangle ID/wire guides explicitly not final textures."""
import bpy,json,sys,argparse
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/final_low_v02'
p=argparse.ArgumentParser();p.add_argument('--before',action='store_true');p.add_argument('--clay',action='store_true');p.add_argument('--reverse',action='store_true');p.add_argument('--views',nargs='+',default=['hero','rear','front_silo']);a=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
version='v01' if a.before else 'v02';bpy.ops.wm.open_mainfile(filepath=str(ROOT/f'art/refinery/final_low_{version}/PROC_FinalLow_{version}.blend'));s=bpy.context.scene;low=bpy.data.collections[f'PROC_FinalLow_{version}_RUNTIME'];s.render.engine='CYCLES';s.cycles.device='CPU';s.render.threads_mode='FIXED';s.render.threads=4;s.cycles.samples=20;s.cycles.use_denoising=True;s.render.resolution_x=1400;s.render.resolution_y=1100;s.render.resolution_percentage=100
views=json.loads((ROOT/'art/refinery/high_v04/REVIEW_CAMERAS.json').read_text());views['front_silo']={'location':(17,-26,14),'target':(2.55,-8.24,3.7),'ortho_scale':9};views['silo_rims']={'location':(23,-23,19),'target':(3.7,-4.6,5.6),'ortho_scale':15}
if a.clay:
 mat=bpy.data.materials.new('REVIEW_ONLY_Chamfer_Clay');mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.30,.30,.30,1);bs.inputs['Metallic'].default_value=0;bs.inputs['Roughness'].default_value=.45
 for o in low.objects:
  for slot in o.material_slots:slot.material=mat
 bpy.data.collections['QA_PaintLayout_NONEXPORT'].hide_render=True
if a.reverse:
 lights=[o for o in s.objects if o.type=='LIGHT'];key=max(lights,key=lambda o:o.data.energy);c=Vector((-2.8,-.2,7.8));key.location=(2*c.x-key.location.x,2*c.y-key.location.y,key.location.z);key.rotation_euler=(c-key.location).to_track_quat('-Z','Y').to_euler()
rows=[]
for name in a.views:
 v=views[name];cam=s.camera;cam.location=v['location'];cam.rotation_euler=(Vector(v['target'])-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=v['ortho_scale'];file=OUT/(f'PROC_FinalLow_{version}_{name}'+('_clay' if a.clay else '')+('_reverse' if a.reverse else '')+'.png');s.render.filepath=str(file);bpy.ops.render.render(write_still=True);rows.append({'view':name,'file':str(file.relative_to(ROOT)),'camera':v});print('PROC_CHAMFER_REVIEW',version,name,a.clay,a.reverse,flush=True)
(OUT/(f'RENDERS_{version}'+('_clay' if a.clay else '')+('_reverse' if a.reverse else '')+'.json')).write_text(json.dumps({'actual_mesh':True,'source_version':version,'clay':a.clay,'reversed_key':a.reverse,'view_transform':s.view_settings.view_transform,'exposure':s.view_settings.exposure,'samples':20,'renders':rows},indent=2)+'\n')
