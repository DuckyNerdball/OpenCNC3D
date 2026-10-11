"""Verify every circumferential bay/tier contains both actual outer triangles."""
import bpy,json,math,hashlib
from pathlib import Path
from collections import Counter
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/final_low_v02';F=OUT/'PROC_FinalLow_v02.blend';bpy.ops.wm.open_mainfile(filepath=str(F));rows=[]
for prefix,c,n,tiers in [('Rear_processing_core',(-3.5,5.75),16,3),('Right_middle_vessel',(4.9,-1.92),12,3),('Right_front_vessel',(2.55,-8.24),10,2)]:
 o=bpy.data.objects['LOW_'+prefix+'_faceted_cladding'];faces=list(o.data.polygons)[:n*tiers*2];coverage=Counter();zlevels=sorted({round(o.data.vertices[i].co.z,5) for f in faces for i in f.vertices});assert len(zlevels)==tiers+1
 for face in faces:
  ps=[o.data.vertices[i].co for i in face.vertices];cent=sum(ps,Vector())/3;angle=math.atan2(cent.y-c[1],cent.x-c[0])%(2*math.pi);bay=int(angle/(2*math.pi)*n)%n;tier=zlevels.index(round(min(p.z for p in ps),5));coverage[(bay,tier)]+=1
  radial=Vector((cent.x-c[0],cent.y-c[1],0)).normalized();assert face.normal.dot(radial)>.95
  assert face.area>1e-6
 assert set(coverage)=={(i,j) for i in range(n) for j in range(tiers)}
 assert all(count==2 for count in coverage.values())
 rows.append({'silo':prefix,'circumferential_bays':n,'tiers':tiers,'actual_outer_triangles':len(faces),'every_bay_and_tier_has_two_triangles':True,'missing_bays':[],'gaps_in_coverage':[]})
report={'file_sha256':hashlib.sha256(F.read_bytes()).hexdigest(),'passed':True,'total_outer_triangles':sum(x['actual_outer_triangles'] for x in rows),'silos':rows,'review_representation':'Actual triangle topology with temporary wire/ID layout shader; no claim of final texture completion.'};(OUT/'SHEET_COVERAGE_QA.json').write_text(json.dumps(report,indent=2)+'\n');print('PROC_COMPLETE_SHEETS',report['total_outer_triangles'],flush=True)
