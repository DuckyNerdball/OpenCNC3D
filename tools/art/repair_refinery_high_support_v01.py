"""Repair the visibly folded inherited left support and refit its chimney seat."""
import bpy,bmesh,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/high_v01'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_High_v01.blend'))
o=bpy.data.objects['Left_wing_RETAINED_PROXY_SHELL'];o.name='Left_wing_PROXY_CONTOUR_REBUILT_PLANAR'
xy=[(-11.871596,-9.438),(-9.5329,-10.807211),(-6.78696,-6.11628),(-6.78696,1.07688),(-10.0298,1.07688),(-10.935,.172),(-10.935,-6.11628)]
def z(y):return 1.22+3.63*min(1,max(0,(y+10.807211)/4.690931))
vs=[(x,y,.22) for x,y in xy]+[(x,y,z(y)) for x,y in xy];fs=[tuple(reversed(range(7))),(7,8,9,13),(13,9,10,11,12)]+[(i,(i+1)%7,(i+1)%7+7,i+7) for i in range(7)]
def replace(o,vs,fs):
 m=bpy.data.meshes.new(o.name+'_corrected');m.from_pydata(vs,[],fs);m.update();bm=bmesh.new();bm.from_mesh(m);bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(m);bm.free();m.materials.append(o.data.materials[0]);o.data=m
replace(o,vs,fs);o.modifiers.clear();b=o.modifiers.new('Designed_chamfer','BEVEL');b.width=.10;b.segments=3;b.limit_method='ANGLE';b.angle_limit=math.radians(27);b.harden_normals=True;n=o.modifiers.new('Broad_planar_normals','WEIGHTED_NORMAL');n.keep_sharp=True
o['reason_rebuilt']='Original proxy surface folded in actual close-up; planar support follows the source footprint, with filled service recess.'
col=bpy.data.collections['PROC_High_v01_NONEXPORT_BAKE_SOURCE']
for ob in col.objects:
 if ob.get('assembly')=='chimney':ob.location.x+=.4;ob.location.y+=.2
seat=bpy.data.objects['Chimney_socket_fitted_to_proxy_support'];seat.location=(0,0,0)
xy2=[(-10.85,-9.6),(-9.05,-9.6),(-9.05,-7.3),(-10.85,-7.3)]
vs=[(x,y,z(y)-.018) for x,y in xy2]+[(x,y,4.058382511) for x,y in xy2]
replace(seat,vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])
bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'PROC_High_v01.blend'))
p=OUT/'BUILD_MANIFEST.json';d=json.loads(p.read_text());d['assemblies']['left_support']=[o.name if name=='Left_wing_RETAINED_PROXY_SHELL' else name for name in d['assemblies']['left_support']];d['review_corrections'].append('Close-up exposed folded proxy support face: rebuilt planar wing from source outline and refitted socket; chimney translated +0.4 X,+0.2 Y inside its support.');p.write_text(json.dumps(d,indent=2)+'\n')
print('PROC support repaired; no final textures or low-poly changed.')
