"""Independent fresh reopen checks for packed refinery trial maps and frozen geometry."""
import bpy,json,hashlib,struct,sys
import numpy as np
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v03';REPORT=json.loads((OUT/'MATERIAL_FULL_REPORT.json').read_text());SHA=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
FILE=ROOT/REPORT['blend'];assert SHA(FILE)==REPORT['blend_sha256'];bpy.ops.wm.open_mainfile(filepath=str(FILE));low=bpy.data.collections['PROC_FinalLow_v02_RUNTIME'];objects=sorted([o for o in low.objects if o.type=='MESH'],key=lambda o:o.name)
h=hashlib.sha256()
for o in objects:
 h.update(o.name.encode())
 for v in o.data.vertices:h.update(struct.pack('<3f',*v.co))
 for p in o.data.polygons:h.update(struct.pack('<3I',*p.vertices));h.update(struct.pack('<?',p.use_smooth))
 for n in o.data.corner_normals:h.update(struct.pack('<3f',*n.vector))
 for uv in o.data.uv_layers['BakeUV'].data:h.update(struct.pack('<2f',*uv.uv))
 h.update(np.asarray(o.matrix_world,np.float64).tobytes())
assert h.hexdigest()==REPORT['geometry_uv_normals_sha256'];assert len(objects)==240;assert sum(len(o.data.polygons) for o in objects)==9992
packed=[]
for channel,row in REPORT['files'].items():
 im=bpy.data.images['PROC_Full_'+channel];f=ROOT/row['path'];assert SHA(f)==row['sha256'];assert im.packed_file
 encoded=bytes(im.packed_file.data);assert hashlib.sha256(encoded).hexdigest()==row['sha256'];assert encoded[24]==16
 assert tuple(im.size)==(4096,4096);assert im.colorspace_settings.name==row['colorspace']
 fresh=bpy.data.images.load(str(f),check_existing=False);fresh.colorspace_settings.name=im.colorspace_settings.name;fresh.reload()
 x=np.empty(4096*4096*4,np.float32);y=np.empty_like(x);im.pixels.foreach_get(x);fresh.pixels.foreach_get(y);error=float(np.abs(x-y).max());assert error==0,(channel,error)
 packed.append({'channel':channel,'file_sha256':row['sha256'],'packed_bytes_match':True,'fresh_decode_difference':error,'bit_depth':encoded[24],'colorspace':im.colorspace_settings.name});bpy.data.images.remove(fresh)
source_checks={}
for file,expected in [('art/refinery/final_low_v02/PROC_FinalLow_v02.blend','52b90144b7446fa94f0c2de2c4ecca8969c4a4e652fac40e6c8199a4997b9b1e'),('art/refinery/high_v04/PROC_High_v04.blend','f457ee9dbe5ae29a2189e17a0d63300fc340d3d71d5d1ac72059f1e560d2fb5c')]:
 source_checks[file]=SHA(ROOT/file);assert source_checks[file]==expected
mat=bpy.data.materials['PROC_Full_MappedMaterial'];trial=set(REPORT['trial_receivers']);assert {o.name for o in objects if any(s.material==mat for s in o.material_slots)}==trial
result={'passed':True,'fresh_blend_sha256':SHA(FILE),'runtime_meshes':len(objects),'runtime_triangles':9992,'geometry_uv_normals_sha256':h.hexdigest(),'packed_maps':packed,'unchanged_approved_sources':source_checks,'trial_receivers_only':sorted(trial),'export_or_runtime_validation':False}
(OUT/'MATERIAL_FRESH_REOPEN_QA.json').write_text(json.dumps(result,indent=2)+'\n');print('PROC_MATERIAL_FRESH_QA_PASSED',flush=True)
