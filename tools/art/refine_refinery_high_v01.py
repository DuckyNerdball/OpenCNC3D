"""Apply the final review-driven fitted cover/rear casing corrections to High01.
The build script now contains the same changes for clean rebuilds.
"""
import bpy,math,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/high_v01'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_High_v01.blend'))
col=bpy.data.collections['PROC_High_v01_NONEXPORT_BAKE_SOURCE']
def fit(name,loc,size):
 o=bpy.data.objects[name];o.location=loc
 dims=[max(v.co[i] for v in o.data.vertices)-min(v.co[i] for v in o.data.vertices) for i in range(3)]
 for v in o.data.vertices:
  for i in range(3):v.co[i]*=size[i]/dims[i]
 return o
fit('Left_wing_service_recess',(-10.97,-1.131,1.586),(.16,2.13,2.94))
fit('Left_wing_service_cover',(-11.06,-1.131,1.586),(.14,2.03,2.84))
for o in col.objects:
 if o.name.startswith('Left_wing_cover_fastener'):
  o.location.x+=.02;o.location.y=-1.87 if o.location.y<-2 else -.39;o.location.z=.40 if o.location.z<1.5 else 2.77
for o in col.objects:
 if o.name.startswith('Nozzle_floor_hydraulic'):
  for v in o.data.vertices:
   if v.co.y>-.99999-1e-4:v.co.y+=.4
bpy.ops.mesh.primitive_cube_add(size=1,location=(-3.4,-.12,8.12));o=bpy.context.object
for c in list(o.users_collection):c.objects.unlink(o)
col.objects.link(o);o.name='Dock_closed_rear_casing';o.dimensions=(8.02,.36,15.78);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
o.data.materials.append(bpy.data.materials['HP_PREVIEW_GDI_paint_ID']);o['assembly']='dock_arch';o['stage']='High v01 / NONEXPORT';o['allocation']='keep main rear closure and broad chamfer';o['runtime_candidate']=False
b=o.modifiers.new('Designed_chamfer','BEVEL');b.width=.1;b.segments=3;b.limit_method='ANGLE';b.angle_limit=math.radians(27);b.harden_normals=True
n=o.modifiers.new('Broad_planar_normals','WEIGHTED_NORMAL');n.keep_sharp=True
bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'PROC_High_v01.blend'))
p=OUT/'BUILD_MANIFEST.json';d=json.loads(p.read_text());d['assemblies']['dock_arch'].append(o.name);d['review_corrections']=['Fitted cover fills the inherited left-wing service aperture','Closed rear dock casing follows approved concept','Hydraulic feed endpoints seated into the dark rear bulkhead'];d['views']['concept_match']={'location':[32,-48,35],'target':[-2.8,-.2,7.8],'ortho_scale':43};p.write_text(json.dumps(d,indent=2)+'\n')
print('PROC final fitted-join corrections saved')
