"""Replace only the arch liner with regular topology before mesh approval."""
import bpy,bmesh,math,json,ast
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/final_low_v01';FILE=OUT/'PROC_FinalLow_v01.blend'
# Reuse just the two pure geometry helper definitions, not the build side effects.
source=ast.parse((ROOT/'tools/art/build_refinery_final_low_v01.py').read_text())
selected=[n for n in source.body if isinstance(n,ast.FunctionDef) and n.name in {'replace','arch_liner'}]
from mathutils import Vector
exec(compile(ast.Module(body=selected,type_ignores=[]),'<geometry helpers>','exec'))
bpy.ops.wm.open_mainfile(filepath=str(FILE));o=bpy.data.objects['LOW_Dock_arch_continuous_inner_liner']
arch_liner(o);bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(o.data);bm.free()
o.data.set_sharp_from_angle(angle=math.radians(60));bpy.context.view_layer.objects.active=o
w=o.modifiers.new('Regular_liner_normals','WEIGHTED_NORMAL');w.keep_sharp=True;w.weight=50;bpy.ops.object.modifier_apply(modifier=w.name)
t=o.modifiers.new('Frozen_bake_target_triangles','TRIANGULATE');t.keep_custom_normals=True;bpy.ops.object.modifier_apply(modifier=t.name)
bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(FILE))
p=OUT/'BUILD_MANIFEST.json';m=json.loads(p.read_text())
for row in m['objects']:
    if row['low']==o.name:
        delta=len(o.data.polygons)-row['triangles'];row['triangles']=len(o.data.polygons);row['method']='regular elliptical arch liner, 24 curve intervals and eight-sided tube'
m['runtime_triangles']+=delta;m['assembly_triangles']['dock_arch']+=delta;p.write_text(json.dumps(m,indent=2)+'\n');print('REGULAR_LINER',len(o.data.polygons),'TOTAL',m['runtime_triangles'])
