"""Actual Yard foundation versus revised refinery apron under identical lighting."""
import bpy,sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v03'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'art/refinery/texture_v02/PROC_FullMaterial_v02.blend'))
(OUT/'material_review').mkdir(exist_ok=True)
s=bpy.context.scene;s.cycles.samples=24;s.render.engine='CYCLES';s.render.threads_mode='FIXED';s.render.threads=4
for o in bpy.data.collections['PROC_FinalLow_v02_RUNTIME'].objects:o.hide_render=True
source=ROOT/'art/construction_yard/runtime_v01/ConstructionYard_Runtime01.blend'
with bpy.data.libraries.load(str(source),link=False) as (a,b):b.objects=['LP_CY_Foundation_original_footprint']
yard=b.objects[0];s.collection.objects.link(yard);yard.hide_render=False;yard.hide_set(False);yard.scale*=10.24
# Rebase the appended material's source images to the preserved exact map copies.
for mat in yard.data.materials:
 for n in mat.node_tree.nodes:
  if n.type=='TEX_IMAGE' and n.image:
   p=source.parent/Path(n.image.filepath).name
   if p.exists():n.image.filepath=str(p);n.image.reload()
cam=s.camera;target=Vector((0,0,.65));cam.location=target+Vector((9,-24,29));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=30
s.render.resolution_x=1100;s.render.resolution_y=900;s.render.resolution_percentage=100;s.render.image_settings.file_format='PNG';s.render.filepath=str(OUT/'material_review/Yard_actual_foundation_same_light.png');bpy.ops.render.render(write_still=True)
print('ACTUAL_YARD_FOUNDATION_COMPARISON_READY',flush=True)
# Read-only: never save over the shared source or full refinery scene.
