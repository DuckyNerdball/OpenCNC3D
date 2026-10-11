"""Read-only sampled material revision checks on actual approved receiver UVs."""
from pathlib import Path
import bpy,json,numpy as np,hashlib
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v03'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_FullMaterial_v03.blend'))
channels={}
for version in ['texture_v02','texture_v03']:
 for channel in ['BaseColor','Roughness','Metallic','SemanticA','SemanticB','PlayerColorMask']:
  p=ROOT/'art/refinery'/version/f'PROC_Full_{channel}.png';im=bpy.data.images.load(str(p),check_existing=False);im.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color';a=np.empty(4096*4096*4,np.float32);im.pixels.foreach_get(a);channels[version,channel]=a.reshape(4096,4096,4)[:,:,:3];bpy.data.images.remove(im)
rows=[]
for name in ['Rear_processing_core','Right_middle_vessel','Right_front_vessel']:
 o=bpy.data.objects['LOW_'+name+'_faceted_cladding'];m=o.data;m.calc_loop_triangles();samples=[]
 for tri in m.loop_triangles:
  if abs(tri.normal.z)>.25:continue
  pts=np.array([o.matrix_world@m.vertices[i].co for i in tri.vertices]);uvs=np.array([m.uv_layers['BakeUV'].data[i].uv[:] for i in tri.loops])
  for i in range(1,15):
   for j in range(1,16-i):
    w=np.array([i/16,j/16,1-(i+j)/16]);samples.append([*(w@uvs),*(w@pts)])
 a=np.array(samples);xy=np.clip((a[:,:2]*4096).astype(int),0,4095);z=a[:,4];top=z.max();bottom=z.min();upper=z>top-(top-bottom)*.25;lower=z<bottom+(top-bottom)*.4
 row={'receiver':o.name,'samples':len(a),'version_comparison':{}}
 for version in ['texture_v02','texture_v03']:
  c={k:channels[version,k][xy[:,1],xy[:,0]] for k in ['BaseColor','Roughness','Metallic','SemanticA','SemanticB','PlayerColorMask']}
  row['version_comparison'][version]={'mean_removed_coating':float(c['SemanticA'][:,1:3].sum(1).mean()),'roughness_stddev':float(c['Roughness'][:,0].std()),'roughness_percentiles':np.percentile(c['Roughness'][:,0],[5,50,95]).tolist(),'upper_mean_dry_deposit':float(c['SemanticB'][upper,1].mean()),'lower_mean_dry_deposit':float(c['SemanticB'][lower,1].mean()),'player_mask_max':float(c['PlayerColorMask'].max())}
  assert c['PlayerColorMask'].max()<.001,'Silo steel must not recolour'
 diff=channels['texture_v03','BaseColor'][xy[:,1],xy[:,0]]-channels['texture_v02','BaseColor'][xy[:,1],xy[:,0]];row['basecolor_mean_absolute_linear_change']=float(abs(diff).mean());assert abs(diff).mean()>.002
 rows.append(row)
r={'scope':'Actual visible vertical cladding UV samples, not entire atlas averages','silos':rows,'geometry_unchanged':'verified separately by MATERIAL_FRESH_REOPEN_QA.json','concrete_provenance':'SHARED_YARD_CONCRETE.json'}
(OUT/'MATERIAL_REVISION_AUDIT.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
