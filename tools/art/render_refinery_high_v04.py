"""Render actual saved PROC high mesh; colour IDs, neutral clay or grazing clay."""
import bpy,json,sys,argparse
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/high_v04'
p=argparse.ArgumentParser();p.add_argument('--views',default='rear,top,front,dock_detail,core_detail,pipe_detail,chimney_seat_detail');p.add_argument('--pass-type',choices=['ids','clay','grazing'],default='ids');p.add_argument('--before',action='store_true');a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'art/refinery/high_v02/PROC_High_v02.blend' if a.before else OUT/'PROC_High_v04.blend'))
scene=bpy.context.scene;cam=scene.camera;views=json.loads((OUT/'REVIEW_CAMERAS.json').read_text());scene.cycles.samples=24
if a.pass_type!='ids':
 m=bpy.data.materials.get('HP_PREVIEW_neutral_clay') or bpy.data.materials.new('HP_PREVIEW_neutral_clay')
 m.use_nodes=True;m.diffuse_color=(.43,.45,.46,1)
 bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.43,.45,.46,1);bs.inputs['Roughness'].default_value=.67
 for o in bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE'].objects:
  if o.type=='MESH':
   o.data.materials.clear();o.data.materials.append(m)
   for poly in o.data.polygons:poly.material_index=0
if a.pass_type=='grazing':
 key=bpy.data.objects['KEY'];key.location=(25,-32,14);key.data.size=8;key.data.energy=17000;key.rotation_euler=(Vector((-3,0,8))-key.location).to_track_quat('-Z','Y').to_euler()
 bpy.data.objects['FILL'].data.energy=2800;bpy.data.objects['RIM'].data.energy=6500
for name in a.views.split(','):
 harv=bpy.data.collections.get('QA_HARV_SIZE_REFERENCE_NONEXPORT')
 if harv:
  for o in harv.objects:o.hide_render=not name.startswith('harvester_fit')
 v=views[name];cam.location=v['location'];cam.rotation_euler=(Vector(v['target'])-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=v['ortho_scale']
 suffix='' if a.pass_type=='ids' else '_'+a.pass_type
 prefix='PROC_High_v02_matched' if a.before else 'PROC_High_v04';scene.render.filepath=str(OUT/f'{prefix}_{name}{suffix}.png');bpy.ops.render.render(write_still=True)
print('PROC_REVIEW_RENDERED',a.pass_type,a.views)
