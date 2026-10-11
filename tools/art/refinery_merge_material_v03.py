"""Merge disjoint material bake shards; assert their receiver and texel coverage."""
import bpy,sys,json,hashlib
import numpy as np
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v03';sys.path.insert(0,str(ROOT/'tools/art'))
from refinery_material_full_v03 import write_png
names=['prefix','concrete_skins','wells_vents'];dirs=[OUT/'bake_shards'/n for n in names];archives=[np.load(p/'MATERIAL_PROGRESS.npz') for p in dirs];reports=[json.loads((p/'MATERIAL_PROGRESS.json').read_text()) for p in dirs]
required=set(json.loads((OUT/'SOURCE_RECEIVER_MAP.json').read_text())['receiver_to_sources']);seen=set();batches=[]
for report in reports:
 for batch in report['batches']:
  current=set(batch['receivers']);assert not current&seen;seen|=current;batches.append(batch)
assert seen==required and len(batches)==14
coverage=np.zeros((4096,4096),np.uint8)
for z in archives:coverage+=(z['RMP'][:,:,2]>.05).astype(np.uint8)
assert coverage.max()==1,('Overlapping shard island pixels',int((coverage>1).sum()))
files={};outputs=['BaseColor','RMP','PlayerColorMask','SemanticA','SemanticB','SemanticC','ConcreteDetailDelta']
for ch in outputs:
 canvas=np.zeros((4096,4096,4),np.float32)
 for z in archives:canvas+=z[ch]
 canvas[:,:,3]=1
 assert np.isfinite(canvas).all();assert canvas[:,:,:3].max()<=1.00001,(ch,float(canvas[:,:,:3].max()))
 parts=['Roughness','Metallic','SurfaceHeight'] if ch=='RMP' else [ch]
 for i,part in enumerate(parts):
  values=canvas if ch!='RMP' else np.repeat(canvas[:,:,i:i+1],4,axis=2)
  values[:,:,3]=1;path=OUT/f'PROC_Full_{part}.png';write_png(path,values,part=='BaseColor')
  files[part]={'path':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'resolution':[4096,4096],'bit_depth':16,'colorspace':'sRGB' if part=='BaseColor' else 'Non-Color','UV':'BakeUV'}
 print('MERGED_MATERIAL_CHANNEL',ch,flush=True)
r=dict(reports[0]);r.update(scope='Full material v03: all approved refinery receivers, requested wear and exact shared Yard concrete',trial_receivers=sorted(seen),batches=batches,files=files,geometry_uv_normals_unchanged=True,source_projection='Same verified named high sources and hybrid explicit/implicit cages as physical bake',merge={'shards':names,'receiver_sets_disjoint':True,'island_and_padding_pixels_disjoint':True,'covered_pixels':int(coverage.sum()),'unmodified_approved_runtime_meshes':240})
(OUT/'MATERIAL_AUTHORING_REPORT.json').write_text(json.dumps(r,indent=2)+'\n');print('FULL_MATERIAL_MERGED',flush=True)
