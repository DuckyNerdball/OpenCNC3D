"""Read-only early source-material preview; not low-poly bake evidence."""
import bpy,sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v03';sys.path.insert(0,str(ROOT/'tools/art'))
import refinery_material_graph_v03 as graph
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_TextureUV_v02.blend'));s=bpy.context.scene
for o in s.objects:
 if o.type=='MESH':o.hide_render=True
bpy.data.collections['PROC_FinalLow_v02_RUNTIME'].hide_render=True
high=bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE'];high.hide_render=False;high.hide_viewport=False;bpy.context.view_layer.layer_collection.children[high.name].exclude=False
for o in high.objects:
 if not o.name.startswith('Right_front_vessel'):continue
 o.hide_render=False;o.hide_set(False)
 if 'triangle_sheet' in o.name:
  g=graph.build('V03_SOURCE_PROBE_'+o.name,graph.role_for_source(o.name,o.data.materials[0].name),None,o)
  bs=g.node('ShaderNodeBsdfPrincipled');g.set(bs.inputs['Base Color'],g.channels['BaseColor']);g.set(bs.inputs['Roughness'],g.channels['Roughness']);g.set(bs.inputs['Metallic'],g.channels['Metallic'])
  bump=g.node('ShaderNodeBump');g.set(bump.inputs['Height'],g.channels['SurfaceHeight']);bump.inputs['Distance'].default_value=.03;g.set(bs.inputs['Normal'],bump.outputs['Normal']);g.links.new(bs.outputs[0],g.out.inputs['Surface']);o.data.materials.clear();o.data.materials.append(g.mat)
cam=s.camera;cam.location=(17,-26,14);target=Vector((2.55,-8.24,3.7));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=9
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=16;s.cycles.use_denoising=True;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1100;s.render.resolution_y=900;s.render.resolution_percentage=100;s.render.image_settings.file_format='PNG';s.render.filepath=str(OUT/'source/SILO_SOURCE_PROBE.png');bpy.ops.render.render(write_still=True)
print('SOURCE_SILO_PROBE_READY',flush=True)
