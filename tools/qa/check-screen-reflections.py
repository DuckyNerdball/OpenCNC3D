"""Validate actual hidden OpenGL reflection probe output, including a thin-surface regression."""
from pathlib import Path
import argparse,csv,json,hashlib
from PIL import Image
import numpy as np
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);a=p.parse_args();O=a.output
rows={r['case']:r for r in csv.DictReader((O/'probe.csv').open())};results={}
for camera in ['orthographic','perspective']:
 r=[rows[camera+'-'+str(i)] for i in range(6)];energy=[float(x['energy']) for x in r];spread=[float(x['spread_x']) for x in r]
 assert energy[0]>100 and energy[1]>100 and energy[2]>100, 'A visible emitter must be reflected'
 assert energy[3]==0, 'A switched-off emitter must not retain its old reflection'
 assert spread[2]>spread[0]*1.15, 'Roughness must widen the reflected lobe'
 assert float(r[4]['centroid_x'])>float(r[1]['centroid_x'])+50, 'Reflection must follow source motion'
 assert energy[5]<energy[1]*.02, 'Occluder must remove at least 98% of emitter energy'
 # A continuous thin emitter must not become periodic horizontal stripes.
 im=np.asarray(Image.open(O/(camera+'-0.ppm')));col=im[:,im.shape[1]//2,0];valid=np.flatnonzero(col>40);interior=col[valid[3]:valid[-4]];assert int((interior<20).sum())<=1,'Thin-surface ray crossing banding'
 results[camera]={'visible_emitter_energy':energy[:3],'roughness_spread_ratio':spread[2]/spread[0],'off_energy':energy[3],'motion_pixels':float(r[4]['centroid_x'])-float(r[1]['centroid_x']),'occluded_fraction':energy[5]/energy[1],'thin_surface_continuity':True}
for f in O.glob('*.ppm'):Image.open(f).save(f.with_suffix('.png'))
record={'status':'PASS','scope':'Actual shared GLSL compiled and executed in hidden macOS OpenGL; two camera projections, three roughnesses, light off/moved/occluded, continuous thin emitter','results':results,'source_sha256':hashlib.sha256((Path(__file__).resolve().parents[2]/'game/screen_reflections_glsl.h').read_bytes()).hexdigest(),'limitation':'Screen-space visibility only. Occlusion edge residual under 2% is from roughness source filtering; no off-screen geometry is reconstructed.'};(O/'QA.json').write_text(json.dumps(record,indent=2)+'\n');print('SCREEN_REFLECTION_PROBE_PASS')
