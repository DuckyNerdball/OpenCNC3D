"""Preflight exact isolated offsets; bake precision-sensitive parts at origin."""
import bpy,sys,json
from pathlib import Path
from collections import defaultdict
R=Path(__file__).resolve().parents[2];O=R/'art/refinery/texture_v02';sys.path.insert(0,str(R/'tools/art'))
from refinery_material_full_v02 import join
from refinery_bake_common_v02 import cage_for
bpy.ops.wm.open_mainfile(filepath=str(O/'PROC_TextureUV_v02.blend'))
c=bpy.data.collections.new('TEMP');bpy.context.scene.collection.children.link(c);m=bpy.data.materials.new('TEMP')
cages=json.loads((O/'CAGE_POLICY.json').read_text())['receivers'];groups=defaultdict(list)
for o in sorted(bpy.data.collections['PROC_FinalLow_v02_RUNTIME'].objects,key=lambda o:o.name):
    precision=any(k in o.name for k in ['faceted_cladding','inner_well_floor','Dock_arch_proxy'])
    groups[(*cage_for(o.name),cages[o.name]['explicit_cage'],precision)].append(o)
plan=[]
def clean():
    for ob in list(c.objects):me=ob.data;bpy.data.objects.remove(ob,do_unlink=True);bpy.data.meshes.remove(me)
def accept(batch,key):
    try:
        ob,row=join('TEST',batch,c,True)
    except AssertionError:
        clean()
        if len(batch)>1:
            cut=len(batch)//2;accept(batch[:cut],key);accept(batch[cut:],key);return
        ob=row=None
        single=[(batch[0][0],(0,0,0),m)]
        ob,row=join('TEST',single,c,True);batch=single
    plan.append({'extrusion':key[0],'distance':key[1],'explicit_cage':key[2],'subpixels':4 if key[3] else 1,'entries':[{'receiver':o.name,'offset':off} for o,off,_ in batch],'max_normal_error':row['normal_encoding_max_vector_error']})
    clean()
for key,objects in groups.items():
    for start in range(0,len(objects),48):
        batch=[(o,((i%8-4)*70.,(i//8-3)*70.,0.),m) for i,o in enumerate(objects[start:start+48])]
        accept(batch,key)
(O/'BATCH_PLAN.json').write_text(json.dumps({'batches':plan,'receiver_count':sum(len(b['entries']) for b in plan),'max_normal_error':max(b['max_normal_error'] for b in plan)},indent=2)+'\n')
print('PREFLIGHT_BATCHES',len(plan),'MAX_ERROR',max(b['max_normal_error'] for b in plan),flush=True)
