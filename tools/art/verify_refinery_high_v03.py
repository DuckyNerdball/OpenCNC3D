"""Fresh saved-file checks for the non-export PROC high source."""
import bpy,bmesh,json,math,hashlib
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/high_v03';FILE=OUT/'PROC_High_v03.blend'
bpy.ops.wm.open_mainfile(filepath=str(FILE));scene=bpy.context.scene;dg=bpy.context.evaluated_depsgraph_get()
high=bpy.data.collections['PROC_High_v03_NONEXPORT_BAKE_SOURCE']
source=json.loads((ROOT/'art/refinery/first_low_v01/FIRST_LOW_REPORT.json').read_text())
revision=json.loads((OUT/'REVISION_MANIFEST.json').read_text());poly=[p[:2] for p in revision['new_foundation_vertices'][6:]]
def edge_clearance(x,y):
 # Counter-clockwise source outline. Minimum inward signed distance.
 vals=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  dx,dy=b[0]-a[0],b[1]-a[1];vals.append((dx*(y-a[1])-dy*(x-a[0]))/math.hypot(dx,dy))
 return min(vals)
triangles=0;degenerate=[];nonmanifold=[];outside=[];boundspts=[];objs=[];assemblies={};nonfinite=[]
for o in high.objects:
 if o.type!='MESH':continue
 e=o.evaluated_get(dg);m=e.to_mesh();m.calc_loop_triangles();pts=[o.matrix_world@v.co for v in m.vertices]
 count=len(m.loop_triangles);triangles+=count;boundspts.extend(pts)
 for p in pts:
  if not all(math.isfinite(x) for x in p):nonfinite.append(o.name)
 planar=('hazard_stripe_ID' in o.name or 'direction_arrow_ID' in o.name)
 bad=sum(t.area<1e-9 for t in m.loop_triangles)
 if bad:degenerate.append({'object':o.name,'triangles':bad})
 bm=bmesh.new();bm.from_mesh(m);edges=sum(not e.is_manifold for e in bm.edges);bm.free()
 if edges and not planar:nonmanifold.append({'object':o.name,'edges':edges})
 if not o.name.startswith('Foundation_'):
  worst=min(edge_clearance(p.x,p.y) for p in pts)
  if worst<-.015:outside.append({'object':o.name,'outside_distance':-worst})
 assembly=o.get('assembly','unknown');assemblies[assembly]=assemblies.get(assembly,0)+count
 objs.append({'name':o.name,'assembly':assembly,'triangles':count,'bounds':{'min':[min(p[k] for p in pts) for k in range(3)],'max':[max(p[k] for p in pts) for k in range(3)]}})
 e.to_mesh_clear()
base=bpy.data.objects['Foundation_FRONT_APRON_REVISED'];expected=[(x,y,-.49) for x,y in source['foundation_bottom_xy']]+[(x,y,.09) for x,y in source['foundation_top_xy']]
base_delta=max((Vector(p)-base.data.vertices[i].co).length for i,p in enumerate(expected))
original_base_delta=base_delta
proposed_base_delta=max((Vector(p)-base.data.vertices[i].co).length for i,p in enumerate(revision['new_foundation_vertices']))
chamfers=[]
for name in ['Dock_jamb_panel_-1_0','Dock_shoulder_1','Core_access_tangent_cover','Right_middle_vessel_rim_clamp_0']:
 o=bpy.data.objects.get(name)
 if not o:continue
 bm=o.modifiers.get('Designed_chamfer');w=bm.width
 raw=[v.co for v in o.data.vertices];minz=min(v.z for v in raw);maxz=max(v.z for v in raw);maxx=max(v.x for v in raw);maxy=max(v.y for v in raw)
 e=o.evaluated_get(dg);m=e.to_mesh()
 cut=[v.co.y for v in m.vertices if abs(v.co.x-maxx)<1e-5 and minz+w*.5<v.co.z<maxz-w*.5]
 setback=maxy-max(cut) if cut else None
 chamfers.append({'object':name,'local_nominal':w,'evaluated_local_xy_corner_setback':setback,'world_xy_setback':setback*o.matrix_world.to_scale().x if setback is not None else None,'world_z_width':w*o.matrix_world.to_scale().z,'measurement':'y setback from original max-y at unchanged max-x, away from z end caps'})
 e.to_mesh_clear()
# Conservative barrel assembly separation (radii include external rim/bolt allowance).
core=(-5.1,6.15,5.29*5.35/4.95);middle=(4.9,-1.92,3.26);front=(2.55,-8.24,2.47)
clearance={}
for a,b,label in[(core,middle,'core_to_middle'),(middle,front,'middle_to_front'),(core,front,'core_to_front')]:clearance[label]=math.hypot(a[0]-b[0],a[1]-b[1])-a[2]-b[2]
# Conservative shoe/front-vessel clearance using evaluated support vertices.
shoe_points=[]
for name in ['Right_service_shoe','Right_service_shoe_access_lid']:
 o=bpy.data.objects[name];e=o.evaluated_get(dg);m=e.to_mesh();shoe_points.extend(o.matrix_world@v.co for v in m.vertices);e.to_mesh_clear()
shoe_clearance=min(math.hypot(v.x-front[0],v.y-front[1])-front[2] for v in shoe_points)
report={'file':str(FILE),'sha256':hashlib.sha256(FILE.read_bytes()).hexdigest(),'high_objects':len(objs),'evaluated_high_triangles':triangles,'counts_are_high_source_not_runtime':True,'runtime_triangle_ceiling_after_later_low':10000,'bounds':{'min':[min(p[k] for p in boundspts) for k in range(3)],'max':[max(p[k] for p in boundspts) for k in range(3)]},'source_foundation_vertex_max_delta':original_base_delta,'proposed_foundation_vertex_max_delta':proposed_base_delta,'original_footprint_exception':'Front apron extension proposed; explicit high-v03 footprint approval pending','objects_outside_proposed_top_polygon':outside,'degenerate_triangles':degenerate,'nonfinite':nonfinite,'nonmanifold_except_designated_planar_markers':nonmanifold,'assemblies_evaluated_triangles':assemblies,'evaluated_chamfer_samples':chamfers,'vessel_circular_envelope_clearances':clearance,'right_shoe_to_front_vessel_envelope_minimum':shoe_clearance,'scope':'Evaluated individual geometry, footprint and vessel envelopes. Deliberate overlapping seats/fasteners are not blanket collision failures; assembled visual joins require review. No harvester runtime fit claim.','objects':objs}
(OUT/'FRESH_REOPEN_QA.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['objects']},indent=2))
