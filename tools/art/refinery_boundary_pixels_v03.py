"""Inspect decoded physical/material pixels at independently missed ray samples."""
import bpy,json
import numpy as np
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'art/refinery/texture_v03'
audit=json.loads((O/'BOUNDARY_RAY_REVIEW.json').read_text());rows=audit['unoccluded_samples']
for channel,file in [('BaseColor','PROC_Full_BaseColor.png'),('Normal','PROC_Working_Normal.png'),('AO','PROC_Working_AO.png'),('Roughness','PROC_Full_Roughness.png')]:
    im=bpy.data.images.load(str(O/file),check_existing=False);im.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color'
    data=np.empty(4096*4096*4,np.float32);im.pixels.foreach_get(data);data=data.reshape(4096,4096,4)
    for row in rows:
        x,y=row['pixel'];row[channel]=data[y,x,:3].tolist()
        if channel=='Normal':row['normal_vector_length']=float(np.linalg.norm(data[y,x,:3]*2-1))
    bpy.data.images.remove(im)
report={'method':'Decoded final 4K PNG values at independent unoccluded missed-ray sample texels; sample coordinates are bottom-left. Independent rays are an approximation, not Cycles hit telemetry.','samples':rows,'black_base_samples':sum(max(r['BaseColor'])<.0001 for r in rows),'invalid_normal_samples':sum(r['normal_vector_length']<.9 or r['normal_vector_length']>1.1 for r in rows)}
(O/'BOUNDARY_PIXEL_INSPECTION.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='samples'})
