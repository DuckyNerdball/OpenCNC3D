from refinery_review_font import load_review_font
"""Read-only matched PROC physical-bake controls, plus contact sheets.

Blender -b -t4 --python this.py -- [--pilot] [--views roof pipe tower gameplay]
python3 this.py --sheets [--pilot]
No .blend is saved. Normal reductions are linear vector box averages normalized
back to unit tangent vectors; their files are review data, not runtime approval.
"""
import argparse,hashlib,json,math,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'art/refinery/texture_v01'
p=argparse.ArgumentParser();p.add_argument('--pilot',action='store_true');p.add_argument('--sheets',action='store_true');p.add_argument('--views',nargs='+',default=['tank','core','gameplay']);p.add_argument('--variants',nargs='+',default=['high','plain_low','normal_4096','normal_2048','normal_1024']);p.add_argument('--lights',nargs='+',default=['key','reverse']);p.add_argument('--samples',type=int,default=16)
args=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else sys.argv[1:])
OUT=BASE/'bake_review'/('pilot' if args.pilot else 'complete');OUT.mkdir(parents=True,exist_ok=True)
sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
labels={'high':'ACTUAL HIGH v04','plain_low':'PLAIN FINAL LOW','normal_4096':'PHYSICAL NORMAL 4K','normal_2048':'PHYSICAL NORMAL 2K','normal_1024':'PHYSICAL NORMAL 1K'}

if args.sheets:
    from PIL import Image,ImageDraw,ImageFont
    report=json.loads((OUT/'RENDER_REPORT.json').read_text());files=[]
    try:font=load_review_font('/System/Library/Fonts/Helvetica.ttc',20);small=load_review_font('/System/Library/Fonts/Helvetica.ttc',15)
    except OSError:font=small=ImageFont.load_default()
    for light in report['lights']:
        variants=report['variants'];views=report['views'];cw,ch=512,422
        sheet=Image.new('RGB',(cw*len(variants),ch*len(views)+72),(24,27,31));draw=ImageDraw.Draw(sheet)
        draw.text((16,12),('PILOT / PARTIAL RECEIVERS — ' if args.pilot else '')+'PROC PHYSICAL BAKE — '+light.upper(),font=font,fill='white')
        draw.text((16,41),'Actual Blender controls. Gray .30 / roughness .45 / metallic 0. AO and all pigment/microheight disabled.',font=small,fill=(199,207,214))
        for row,view in enumerate(views):
            for col,variant in enumerate(variants):
                path=OUT/f'{view}_{light}_{variant}.png';im=Image.open(path).convert('RGB')
                # Gameplay images stay at their actual384px native dimensions.
                im.thumbnail((512,384));x=col*cw+(cw-im.width)//2;y=72+row*ch+28
                sheet.paste(im,(x,y));draw.text((col*cw+10,72+row*ch+5),view.upper()+' / '+labels[variant],font=small,fill='white')
        path=OUT/f'CONTACT_{light}.jpg';sheet.save(path,quality=94);files.append({'path':str(path.relative_to(ROOT)),'sha256':sha(path)})
    report['contact_sheets']=files;(OUT/'RENDER_REPORT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(files,indent=2));raise SystemExit()

import bpy
import numpy as np
from mathutils import Vector
UV=BASE/'PROC_TextureUV_v01.blend'
NORMAL=BASE/('PROC_Pilot_Normal.png' if args.pilot else 'PROC_Working_Normal.png')
MANIFEST=BASE/('PHYSICAL_BAKE_PILOT.json' if args.pilot else 'PHYSICAL_BAKE_REPORT.json')
manifest=json.loads(MANIFEST.read_text());mapping=json.loads((BASE/'SOURCE_RECEIVER_MAP.json').read_text())
normal_row=next(r for r in manifest['output_maps'] if r['channel']=='Normal')
assert sha(NORMAL)==normal_row['sha256']
assert sha(UV)==manifest['uv_source_sha256']
source_hashes={str(f):sha(f) for f in (UV,NORMAL,MANIFEST)}
chosen={r['receiver'] for r in manifest['receivers']}
required=set(mapping['receiver_to_sources'])
if not args.pilot:assert required<=chosen,('Representative receiver set incomplete',sorted(required-chosen))
bpy.ops.wm.open_mainfile(filepath=str(UV));s=bpy.context.scene
low=bpy.data.collections['PROC_FinalLow_v01_RUNTIME'];high=bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE']
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=args.samples;s.cycles.use_denoising=True;s.render.threads_mode='FIXED';s.render.threads=4
s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA';s.render.image_settings.color_depth='8';s.render.resolution_percentage=100;s.render.film_transparent=False
# Preserve approved view transform/exposure and original studio lights.
look={'view_transform':s.view_settings.view_transform,'look':s.view_settings.look,'exposure':s.view_settings.exposure,'gamma':s.view_settings.gamma}
lighting=[o for o in s.objects if o.type=='LIGHT'];key=max(lighting,key=lambda o:o.data.energy)
original_lights={o.name:{'location':list(o.location),'rotation':list(o.rotation_euler),'energy':o.data.energy,'size':getattr(o.data,'size',None),'type':o.data.type} for o in lighting}
center=Vector((-2.8,-.2,7.8));key_location=key.location.copy();key_rotation=key.rotation_euler.copy()
original_scene_camera=s.camera
camdata=bpy.data.cameras.new('PROC_PhysicalReviewCamera');cam=bpy.data.objects.new('PROC_PhysicalReviewCamera',camdata);s.collection.objects.link(cam);cam.data.type='ORTHO';s.camera=cam
cameras={
 'tank':{'location':(22,-18,15),'target':(4.9,-1.92,4.8),'ortho_scale':12,'resolution':(800,600)},
 'core':{'location':(10,22,28),'target':(-5.1,6.15,6.8),'ortho_scale':15,'resolution':(800,600)},
 'gameplay':{'location':(22,-50,32),'target':(-2.8,-.2,7.8),'ortho_scale':48,'resolution':(384,288)},
}


def material(name,image=None):
    mat=bpy.data.materials.new(name);mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links;n.clear();out=n.new('ShaderNodeOutputMaterial');bs=n.new('ShaderNodeBsdfPrincipled');bs.inputs['Base Color'].default_value=(.30,.30,.30,1);bs.inputs['Metallic'].default_value=0;bs.inputs['Roughness'].default_value=.45;l.new(bs.outputs['BSDF'],out.inputs['Surface'])
    if image:
        uv=n.new('ShaderNodeUVMap');uv.uv_map='BakeUV';tex=n.new('ShaderNodeTexImage');tex.image=image;tex.interpolation='Linear';tex.extension='EXTEND';nm=n.new('ShaderNodeNormalMap');nm.space='TANGENT';nm.uv_map='BakeUV';nm.inputs['Strength'].default_value=1;l.new(uv.outputs['UV'],tex.inputs['Vector']);l.new(tex.outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs['Normal'])
    return mat
plain=material('PROC_REVIEW_Neutral_Gray')
normal=bpy.data.images.load(str(NORMAL),check_existing=False);normal.colorspace_settings.name='Non-Color';normal.reload()
size=normal.size[0];assert size==manifest['resolution']
args.variants=[v for v in args.variants if not v.startswith('normal_') or int(v.split('_')[1])<=size]
buf=np.empty(size*size*4,np.float32);normal.pixels.foreach_get(buf);original=buf.reshape(size,size,4)

def write_png16(path,a):
    a=np.clip(np.rint(np.flipud(a)*65535),0,65535).astype('>u2');h,w,_=a.shape
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    data=b''.join(b'\0'+row.tobytes() for row in a)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,16,6,0,0,0))+chunk(b'IDAT',zlib.compress(data,4))+chunk(b'IEND',b''))

materials={'high':plain,'plain_low':plain,f'normal_{size}':material(f'PROC_REVIEW_Physical{size}',normal)};reduction_rows=[]
for res in (2048,1024):
    if res>=size:continue
    f=size//res;avg=original[:,:,:3].reshape(res,f,res,f,3).mean((1,3));v=avg*2-1;length=np.linalg.norm(v,axis=-1,keepdims=True);v/=np.maximum(length,1e-10);a=np.ones((res,res,4),np.float32);a[:,:,:3]=v*.5+.5
    path=OUT/f'PROC_PhysicalNormal_Filtered_{res}.png';write_png16(path,a);im=bpy.data.images.load(str(path),check_existing=False);im.colorspace_settings.name='Non-Color';im.reload();materials[f'normal_{res}']=material(f'PROC_REVIEW_Physical{res}',im);reduction_rows.append({'resolution':res,'path':str(path.relative_to(ROOT)),'sha256':sha(path),'filter':f'exact fxf box mean of linear tangent vector components, then vector normalization; source{size} decoded16-bit PNG','bit_depth':16})
bpy.context.view_layer.layer_collection.children[high.name].exclude=False
high.hide_viewport=False
# Keep context floor and lights, but override both approved geometry sets neutrally.
for o in list(high.objects)+list(low.objects):
    if o.type!='MESH':continue
    if len(o.material_slots):
        for slot in o.material_slots:slot.material=plain
    else:o.data.materials.append(plain)
    o.hide_set(False)
for o in lighting:o.hide_render=False;o.hide_set(False)
# Source collection visibility is explicit so high modifiers evaluate fully.
for c in (low,high):c.hide_viewport=False
for layer in bpy.context.view_layer.layer_collection.children:
    if layer.collection in (low,high):layer.exclude=False
bpy.data.collections['QA_PaintLayout_NONEXPORT'].hide_render=True
rows=[]
for light in args.lights:
    if light=='key':key.location=key_location;key.rotation_euler=key_rotation
    else:
        key.location=(2*center.x-key_location.x,2*center.y-key_location.y,key_location.z);key.rotation_euler=(center-key.location).to_track_quat('-Z','Y').to_euler()
    for view in args.views:
        v=cameras[view];cam.location=v['location'];cam.rotation_euler=(Vector(v['target'])-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=v['ortho_scale'];s.render.resolution_x,s.render.resolution_y=v['resolution']
        for variant in args.variants:
            is_high=variant=='high';high.hide_render=not is_high;low.hide_render=is_high
            for o in high.objects:o.hide_render=not is_high
            for o in low.objects:
                o.hide_render=is_high
                if o.type=='MESH':
                    for slot in o.material_slots:slot.material=materials[variant] if o.name in chosen or variant=='plain_low' else plain
            bpy.context.view_layer.update();path=OUT/f'{view}_{light}_{variant}.png';s.render.filepath=str(path);bpy.ops.render.render(write_still=True)
            rows.append({'view':view,'light':light,'variant':variant,'file':str(path.relative_to(ROOT)),'sha256':sha(path),'resolution':list(v['resolution'])});print('PROC_BAKE_CONTROL',view,light,variant,flush=True)
assert all(sha(f)==h for f,h in source_hashes.items()),'A source changed during read-only review rendering'
report={'status':'pilot partial controls only' if args.pilot else 'complete required representative receiver controls rendered; visual assessment required','blender_version':bpy.app.version_string,'source_hashes':source_hashes,'pilot':args.pilot,'receivers_baked':len(chosen),'required_representative_receivers':sorted(required),'required_representative_complete':required<=chosen,'missing_representative_receivers':sorted(required-chosen),'views':args.views,'variants':args.variants,'lights':args.lights,'cameras':{k:cameras[k] for k in args.views},'original_lights':original_lights,'reversed_key_policy':'key X/Y reflected about (.45,.2), same height/energy/size; original fill/world fixed','view_settings':look,'structural_shader':{'BaseColor_linear':.30,'roughness':.45,'metallic':0,'normal_strength':1,'AO_connected':False,'microheight_connected':False,'pigment_connected':False},'normal_reductions':reduction_rows,'renders':rows,'source_blends_not_saved':True,'limitation':'These are Blender structural-normal controls, not runtime proof or artistic material approval. Small-size labels refer to384x288 review render.'}
(OUT/'RENDER_REPORT.json').write_text(json.dumps(report,indent=2)+'\n');print('PROC_BAKE_REVIEW_COMPLETE',OUT,flush=True)
