"""Focused enlarged-core/support surface intersections and apron checks."""
import bpy,json,hashlib
from pathlib import Path
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/high_v02'
FILE=OUT/'PROC_High_v02.blend';bpy.ops.wm.open_mainfile(filepath=str(FILE))
dg=bpy.context.evaluated_depsgraph_get();high=bpy.data.collections['PROC_High_v02_NONEXPORT_BAKE_SOURCE']
def tree(objects):
 vertices=[];polygons=[]
 for o in objects:
  ev=o.evaluated_get(dg);m=ev.to_mesh();offset=len(vertices)
  vertices.extend(o.matrix_world@v.co for v in m.vertices)
  polygons.extend([offset+i for i in p.vertices] for p in m.polygons);ev.to_mesh_clear()
 return BVHTree.FromPolygons(vertices,polygons)
core=tree([o for o in high.objects if o.get('assembly')=='Rear_processing_core'])
checks={}
for name in ['Left_wing_PROXY_CONTOUR_REBUILT_PLANAR','Dock_closed_rear_casing','Dock_arch_proxy_footprint_rebuilt_taller']:
 checks[name]=len(core.overlap(tree([bpy.data.objects[name]])))
arrows=[o.name for o in high.objects if o.name.startswith('Apron_direction_arrow_ID_')]
plates={a:len([o for o in high.objects if o.get('assembly')==a and '_triangle_sheet_' in o.name]) for a in ['Rear_processing_core','Right_middle_vessel','Right_front_vessel']}
report={'sha256':hashlib.sha256(FILE.read_bytes()).hexdigest(),'rear_core_surface_intersection_pairs':checks,'approach_arrow_count':len(arrows),'triangle_sheet_counts':plates,'limits':'Scoped static surface intersection check, supplemented by evaluated footprint and inspected actual renders. Not runtime harvester validation.'}
(OUT/'JOINT_QA.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
assert not any(checks.values()),checks
assert len(arrows)==3
