"""Round the accepted larger apron, preserving every other mesh and transform."""
import bpy,bmesh,json,hashlib,math,struct,shutil
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OLD=ROOT/'art/refinery/high_v03';OUT=ROOT/'art/refinery/high_v04';OUT.mkdir(exist_ok=True)
SOURCE=OLD/'PROC_High_v03.blend';expected='5402c7cf6e9ebca458e4fb3b93a8ed08f86667f01828ba3f673e326280f0a925'
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==expected
bpy.ops.wm.open_mainfile(filepath=str(SOURCE));scene=bpy.context.scene
high=bpy.data.collections['PROC_High_v03_NONEXPORT_BAKE_SOURCE']
names=['Foundation_FRONT_APRON_REVISED','Foundation_inset_top_course']
def fingerprint(o):
 return hashlib.sha256(b''.join(struct.pack('3f',*v.co) for v in o.data.vertices)).hexdigest(),[list(r) for r in o.matrix_world]
unchanged={o.name:fingerprint(o) for o in high.objects if o.type=='MESH' and o.name not in names}
(OUT/'SOURCE_REVIEW.json').write_text(json.dumps({'source':str(SOURCE),'sha256':expected,'opened_mesh_count':len([o for o in high.objects if o.type=='MESH']),'preserved_actual_views':str(OLD/'PROC_High_v03_review_sheet.png'),'user_direction':'After is much better. Just make sure the concrete floor, has rounded corners, rather than sharp ones'},indent=2)+'\n')
def fillet(poly,radius,segments=16):
 result=[]
 for i,xy in enumerate(poly):
  v=Vector(xy);a=(Vector(poly[i-1])-v).normalized();b=(Vector(poly[(i+1)%len(poly)])-v).normalized()
  theta=math.acos(max(-1,min(1,a.dot(b))));d=radius/math.tan(theta/2)
  assert d<min((Vector(poly[i-1])-v).length,(Vector(poly[(i+1)%len(poly)])-v).length)*.45
  c=v+(a+b).normalized()*radius/math.sin(theta/2)
  p=v+a*d;q=v+b*d;aa=math.atan2(p.y-c.y,p.x-c.x);bb=math.atan2(q.y-c.y,q.x-c.x)
  sweep=(bb-aa)%(2*math.pi);assert 0<sweep<math.pi
  result.extend([(c.x+radius*math.cos(aa+sweep*j/segments),c.y+radius*math.sin(aa+sweep*j/segments)) for j in range(segments+1)])
 return result
dg=bpy.context.evaluated_depsgraph_get();old_count=0;records=[]
for name,radius in zip(names,[2.0,1.75]):
 o=bpy.data.objects[name];e=o.evaluated_get(dg);m=e.to_mesh();m.calc_loop_triangles();old_count+=len(m.loop_triangles);e.to_mesh_clear()
 old=[tuple(v.co) for v in o.data.vertices];assert len(old)==12
 rings=[fillet([p[:2] for p in old[k:k+6]],radius) for k in (0,6)]
 n=len(rings[0]);verts=[(x,y,old[k*6][2]) for k,ring in enumerate(rings) for x,y in ring]
 faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
 me=bpy.data.meshes.new(name+'_rounded_contour');me.from_pydata(verts,[],faces);me.update()
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(me);bm.free()
 for mat in o.data.materials:me.materials.append(mat)
 o.data=me
 for p in me.polygons:p.use_smooth=len(p.vertices)==4
 me.set_sharp_from_angle(angle=math.radians(30))
 # Broad plan-view rounding is real contour geometry; existing edge bevel stays.
 o['corner_radius']=radius;o['corner_segments']=16;o['stage']='High v04 / NONEXPORT'
 records.append({'object':name,'radius':radius,'segments_per_corner':16,'old_vertices':old,'rounded_top_xy':rings[1],'rounded_bottom_xy':rings[0]})
high.name='PROC_High_v04_NONEXPORT_BAKE_SOURCE'
poly=records[0]['rounded_top_xy']
def clearance(x,y):
 return min(((b[0]-a[0])*(y-a[1])-(b[1]-a[1])*(x-a[0]))/math.hypot(b[0]-a[0],b[1]-a[1]) for a,b in zip(poly,poly[1:]+poly[:1]))
bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();checks=[];new_count=0;points=[]
for name in names:
 o=bpy.data.objects[name];e=o.evaluated_get(dg);m=e.to_mesh();m.calc_loop_triangles();new_count+=len(m.loop_triangles)
 bm=bmesh.new();bm.from_mesh(m);bad=sum(not e.is_manifold for e in bm.edges);bm.free()
 deg=sum(t.area<1e-9 for t in m.loop_triangles);assert bad==deg==0,(name,bad,deg)
 points.extend(o.matrix_world@v.co for v in m.vertices)
 checks.append({'object':name,'triangles':len(m.loop_triangles),'nonmanifold_edges':bad,'degenerate_triangles':deg});e.to_mesh_clear()
outside=[]
for name,fp in unchanged.items():
 o=bpy.data.objects[name];assert fingerprint(o)==fp,name
 e=o.evaluated_get(dg);m=e.to_mesh();worst=min(clearance(*(o.matrix_world@v.co)[:2]) for v in m.vertices);e.to_mesh_clear()
 if worst<-.015:outside.append({'object':name,'outside':-worst})
assert not outside,outside
harv=bpy.data.collections['QA_HARV_SIZE_REFERENCE_NONEXPORT'];harv_margin=min(clearance(*(o.matrix_world@v.co)[:2]) for o in harv.objects for v in o.data.vertices)
assert harv_margin>0
qa={'source_sha256':expected,'changed_meshes':checks,'unchanged_other_meshes_and_transforms':len(unchanged),'objects_outside_rounded_apron':outside,'harvester_static_minimum_edge_clearance':harv_margin,'evaluated_high_triangles':452700-old_count+new_count,'apron_bounds':{'min':[min(p[k] for p in points) for k in range(3)],'max':[max(p[k] for p in points) for k in range(3)]},'scope':'Rounded concrete contour and inherited unchanged structure. Existing static harvester collision result remains applicable; no runtime docking claim.'}
shutil.copyfile(OLD/'REVIEW_CAMERAS.json',OUT/'REVIEW_CAMERAS.json')
(OUT/'ROUNDED_APRON.json').write_text(json.dumps({'accepted_source':'High v03 longer front apron, subject to rounded-corner revision','foundation_contours':records,'approval':'High v04 actual mesh pending renewed approval'},indent=2)+'\n')
scene['high_review_state']='High v04 rounded apron; larger approach accepted, revised mesh pending approval'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'PROC_High_v04.blend'))
qa['sha256']=hashlib.sha256((OUT/'PROC_High_v04.blend').read_bytes()).hexdigest()
(OUT/'ROUNDING_QA.json').write_text(json.dumps(qa,indent=2)+'\n')
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==expected
print(json.dumps(qa,indent=2))
