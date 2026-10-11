"""Render saved actual low/high with exactly matched review cameras."""
import bpy,json,argparse,sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/final_low_v01'
p=argparse.ArgumentParser();p.add_argument('--views',default='hero,rear,side,top');p.add_argument('--high',action='store_true');p.add_argument('--clay',action='store_true');p.add_argument('--samples',type=int,default=24);a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_FinalLow_v01.blend'))
scene=bpy.context.scene;cam=scene.camera;views=json.loads((ROOT/'art/refinery/high_v04/REVIEW_CAMERAS.json').read_text());scene.cycles.samples=a.samples
high=bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE'];low=bpy.data.collections['PROC_FinalLow_v01_RUNTIME'];markers=bpy.data.collections['QA_PaintLayout_NONEXPORT']
bpy.context.view_layer.layer_collection.children[high.name].exclude=not a.high
high.hide_render=not a.high;low.hide_render=a.high;markers.hide_render=a.high
for o in high.objects:o.hide_render=not a.high
if a.clay:
    mat=bpy.data.materials.get('HP_PREVIEW_neutral_clay') or bpy.data.materials.new('HP_PREVIEW_neutral_clay')
    mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.43,.45,.46,1);bs.inputs['Roughness'].default_value=.67;bs.inputs['Metallic'].default_value=0
    for o in (high if a.high else low).objects:
        o.data.materials.clear();o.data.materials.append(mat)
        for f in o.data.polygons:f.material_index=0
for name in a.views.split(','):
    v=views[name];cam.location=v['location'];cam.rotation_euler=(Vector(v['target'])-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=v['ortho_scale']
    scene.render.filepath=str(OUT/('PROC_'+('High_v04_matched' if a.high else 'FinalLow_v01')+'_'+name+('_clay' if a.clay else '')+'.png'));bpy.ops.render.render(write_still=True)
print('PROC_LOW_REVIEW_RENDERED',a.views)
