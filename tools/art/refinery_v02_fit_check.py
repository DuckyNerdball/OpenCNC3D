"""Compare preserved inner wells and chamfer seats to approved FinalLow v01."""
import bpy,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'art/refinery/final_low_v02'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'art/refinery/final_low_v01/PROC_FinalLow_v01.blend'))
old={}
for o in bpy.data.collections['PROC_FinalLow_v01_RUNTIME'].objects:
    if '_continuous_sealed_wall' in o.name:
        n=len(o.data.vertices)//7
        old[o.name]=('inner_well',[(o.matrix_world@v.co).copy() for v in list(o.data.vertices)[5*n:]])
    elif '_rim_panel_' in o.name or '_rim_clamp_' in o.name:
        z=min(v.co.z for v in o.data.vertices)
        old[o.name]=('bottom_seat',[(o.matrix_world@v.co).copy() for v in o.data.vertices if abs(v.co.z-z)<1e-5])
file=OUT/'PROC_FinalLow_v02.blend'
bpy.ops.wm.open_mainfile(filepath=str(file))
rows=[]
for name,(kind,points) in old.items():
    o=bpy.data.objects[name]
    new=[o.matrix_world@v.co for v in o.data.vertices]
    delta=max(min((v-p).length for v in new) for p in points)
    assert delta<1e-4,(name,delta)
    rows.append({'object':name,'preserved_feature':kind,'compared_vertices':len(points),'max_world_displacement':delta})
report={'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'passed':True,'max_displacement':max(r['max_world_displacement'] for r in rows),'comparisons':rows,'scope':'Original inner-well circle sampling and both inner profile rings preserved; original rim/clamp bottom-seat vertices preserved. Visual matched renders check exposed fit.'}
(OUT/'FITTED_SEATS_QA.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
