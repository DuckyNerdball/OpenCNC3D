"""Retain the cladding's planar facets; no geometry/triangle-order changes."""
import bpy,math,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/final_low_v01';FILE=OUT/'PROC_FinalLow_v01.blend'
bpy.ops.wm.open_mainfile(filepath=str(FILE));records=[]
for o in bpy.data.collections['PROC_FinalLow_v01_RUNTIME'].objects:
    if 'faceted_cladding' not in o.name:continue
    before=[tuple(p.vertices) for p in o.data.polygons]
    o.data.set_sharp_from_angle(angle=math.radians(1))
    w=o.modifiers.new('Preserve_planar_sheet_facets','WEIGHTED_NORMAL');w.keep_sharp=True;w.weight=50
    bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=w.name)
    assert before==[tuple(p.vertices) for p in o.data.polygons]
    error=max(p.normal.angle(o.data.corner_normals[i].vector,0)*180/math.pi for p in o.data.polygons for i in p.loop_indices)
    assert error<.1,(o.name,error)
    records.append({'object':o.name,'max_corner_normal_deviation_degrees':error,'triangle_order_unchanged':True,'high_sources':json.loads(o['high_sources'])})
bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(FILE))
(OUT/'CLADDING_NORMALS.json').write_text(json.dumps(records,indent=2)+'\n')
print('PROC_FACET_NORMALS_VERIFIED',[(r['object'],r['max_corner_normal_deviation_degrees']) for r in records])
