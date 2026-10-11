"""Project a representative PROC material trial to frozen BakeUV; never propagate.

High sources and receivers are rigidly separated in temporary emission batches.
Mesh corner vectors and source positions are preserved; no tangent bake occurs here.
"""
import bpy,sys,json,math,hashlib,struct,zlib,time,argparse
from pathlib import Path
from collections import defaultdict
import numpy as np
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'art/refinery/texture_v02'
sys.path.insert(0,str(Path(__file__).resolve().parent))
import refinery_bake_common_v02 as policy
import refinery_material_graph_v02 as graph
import refinery_uv_audit as audit
from refinery_cages_v02 import create_batch
parser=argparse.ArgumentParser()
parser.add_argument('--resolution',type=int,default=4096)
parser.add_argument('--pilot',action='store_true')
parser.add_argument('--resume',action='store_true')
parser.add_argument('--subpixels',type=int,default=1)
parser.add_argument('--receivers',nargs='*')
A=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
RES=A.resolution
PREFIX='PROC_Pilot' if A.pilot else 'PROC_Full'
CHANNELS=['BaseColor','RMP','PlayerColorMask','SemanticA','SemanticB','SemanticC']

def is_trial(n):
    return n in policy.source_mapping()['receiver_to_sources']

def write_png(path,values,srgb=False):
    values=np.asarray(values,dtype=np.float32).copy()
    if srgb:values[:,:,:3]=np.where(values[:,:,:3]<=.0031308,12.92*values[:,:,:3],1.055*np.maximum(values[:,:,:3],0)**(1/2.4)-.055)
    a=np.clip(np.rint(values[::-1]*65535),0,65535).astype('>u2');h,w,c=a.shape
    def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v)&0xffffffff)
    body=b''.join(b'\x00'+row.tobytes() for row in a)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,16,6,0,0,0))+chunk(b'IDAT',zlib.compress(body,6))+chunk(b'IEND',b''))

def join(name,entries,collection,uv=False):
    verts=[];faces=[];normals=[];smooth=[];materials=[];mi=[];uvs=[];sharp=set();blocks=[]
    deps=bpy.context.evaluated_depsgraph_get()
    for src,offset,mat in entries:
        eo=src if uv else src.evaluated_get(deps);m=eo.data if uv else eo.to_mesh();m.calc_loop_triangles()
        world=eo.matrix_world;nm=world.inverted().transposed().to_3x3();vbase=len(verts);pbase=len(faces)
        verts.extend(tuple(world@v.co+Vector(offset)) for v in m.vertices)
        for e in m.edges:
            if e.use_edge_sharp:sharp.add(tuple(sorted(vbase+i for i in e.vertices)))
        if mat not in materials:materials.append(mat)
        material_index=materials.index(mat)
        for f in m.polygons:
            faces.append(tuple(vbase+i for i in f.vertices));smooth.append(f.use_smooth);mi.append(material_index)
            normals.extend(tuple((nm@m.corner_normals[li].vector).normalized()) for li in f.loop_indices)
            if uv:uvs.extend(tuple(m.uv_layers['BakeUV'].data[li].uv) for li in f.loop_indices)
        blocks.append({'source':src.name,'polygons':[pbase,len(faces)],'translation':list(offset)})
        if not uv:eo.to_mesh_clear()
    m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
    for mat in materials:m.materials.append(mat)
    for p,s,i in zip(m.polygons,smooth,mi):p.use_smooth=s;p.material_index=i
    for e in m.edges:e.use_edge_sharp=tuple(sorted(e.vertices)) in sharp
    m.update();m.normals_split_custom_set(normals)
    if uv:m.uv_layers.new(name='BakeUV').data.foreach_set('uv',np.asarray(uvs,dtype=np.float32).ravel())
    o=bpy.data.objects.new(name,m);collection.objects.link(o)
    err=float(np.max(np.linalg.norm(np.asarray(normals)-np.asarray([n.vector[:] for n in m.corner_normals]),axis=1)))
    if err>.001:
        # Independent temporary corner fans avoid loss in Blender's custom
        # normal encoding after nonuniform world transforms. Runtime unchanged.
        for edge in m.edges:edge.use_edge_sharp=True
        for face in m.polygons:face.use_smooth=True
        m.update();m.normals_split_custom_set(normals)
        err=float(np.max(np.linalg.norm(np.asarray(normals)-np.asarray([n.vector[:] for n in m.corner_normals]),axis=1)))
        desired=np.asarray(normals,dtype=np.float64)
        encoded=desired.copy()
        for attempt in range(6):
            if err<.001:break
            got=np.asarray([n.vector[:] for n in m.corner_normals])
            encoded+=desired-got
            encoded/=np.maximum(np.linalg.norm(encoded,axis=1,keepdims=True),1e-12)
            m.normals_split_custom_set(encoded.tolist());m.update()
            err=float(np.max(np.linalg.norm(desired-np.asarray([n.vector[:] for n in m.corner_normals]),axis=1)))
    if err>=.001:
        got=np.asarray([n.vector[:] for n in m.corner_normals]);d=np.linalg.norm(np.asarray(normals)-got,axis=1);ix=int(d.argmax());poly=next(p for p in m.polygons if ix in p.loop_indices)
        print('NORMAL_JOIN_FAILURE',name,err,'loop',ix,'poly',poly.index,'source',next(b['source'] for b in blocks if b['polygons'][0]<=poly.index<b['polygons'][1]),'wanted',normals[ix],'got',got[ix].tolist(),'smooth',poly.use_smooth,flush=True)
    # Unselected AO blockers contribute only geometric intersection, never
    # source shading normals or projected material channels.
    assert err<.001 or 'AO_CONTEXT' in name,(name,err)
    return o,{'object':name,'normal_encoding_max_vector_error':err,'blocks':blocks}

def main():
    bpy.ops.wm.open_mainfile(filepath=str(OUT/'PROC_MaterialAuthoring_v02.blend'))
    scene=bpy.context.scene;low=bpy.data.collections['PROC_FinalLow_v02_RUNTIME'];alllow=sorted([o for o in low.objects if o.type=='MESH'],key=lambda x:x.name)
    policy.assert_closed_orientation(alllow)
    before=audit.fingerprint(alllow);trial=[o for o in alllow if is_trial(o.name) and (A.receivers is None or o.name in A.receivers)]
    if A.receivers is not None:assert set(A.receivers)=={o.name for o in trial},'Requested pilot receivers missing or outside trial definition'
    policy.prepare_visibility();policy.configure_bake(scene,RES,1,4)
    mapping=policy.source_mapping()['receiver_to_sources'];pigment=bpy.data.images.load(str(OUT/'source/PROC_Ochre_Pigment_v01.png'),check_existing=False);pigment.colorspace_settings.name='sRGB';pigment.pack()
    temp=bpy.data.collections.new('PROC_EMISSION_BATCH_NON_EXPORT');scene.collection.children.link(temp)
    groups=defaultdict(list)
    cages=json.loads((OUT/"CAGE_POLICY.json").read_text())["receivers"]
    for o in trial:groups[(*policy.cage_for(o.name),cages[o.name]["explicit_cage"])].append(o)
    dest=bpy.data.materials.new('PROC_EMISSION_TARGET');dest.use_nodes=True;target=dest.node_tree.nodes.new('ShaderNodeTexImage');dest.node_tree.nodes.active=target
    canvases={ch:np.zeros((RES,RES,4),np.float32) for ch in CHANNELS}
    for ch,canvas in canvases.items():canvas[:,:,3]=1
    if A.resume:
        for channel in CHANNELS:
            if channel=='RMP':
                for c,part in enumerate(['Roughness','Metallic','SurfaceHeight']):
                    im=bpy.data.images.load(str(OUT/(PREFIX+'_'+part+'.png')),check_existing=False);im.colorspace_settings.name='Non-Color';v=np.empty(RES*RES*4,np.float32);im.pixels.foreach_get(v);canvases[channel][:,:,c]=v.reshape(RES,RES,4)[:,:,0];bpy.data.images.remove(im)
            else:
                im=bpy.data.images.load(str(OUT/(PREFIX+'_'+channel+'.png')),check_existing=False);im.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color';im.pixels.foreach_get(canvases[channel].ravel());bpy.data.images.remove(im)
    target_images={ch:bpy.data.images.new('TRIAL_BUFFER_'+ch,RES,RES,alpha=True,float_buffer=True) for ch in CHANNELS}
    for im in target_images.values():im.colorspace_settings.name='Non-Color'
    report={'scope':'Complete approved refinery: all 240 receivers','trial_receivers':[o.name for o in trial],'working_resolution':RES,'batches':[],'source_low_sha256':hashlib.sha256((ROOT/'art/refinery/final_low_v02/PROC_FinalLow_v02.blend').read_bytes()).hexdigest(),'shader_space':'rest world imported Blender units; no lighting or AO in clean BaseColor','source_projection':'same named high source membership and implicit cages as physical pass','files':{}}
    batches=json.loads((OUT/'BATCH_PLAN.json').read_text())['batches']
    for batch in batches:batch['entries']=[e for e in batch['entries'] if e['receiver'] in {o.name for o in trial}]
    batches=[batch for batch in batches if batch['entries']]
    selected_sources={n for o in trial for n in mapping[o.name]}
    for mat in list(bpy.data.materials):
        if mat.get('source_original') in selected_sources:bpy.data.materials.remove(mat,do_unlink=True)
    for batch_no,batch in enumerate(batches):
        extrusion,distance,explicit=batch['extrusion'],batch['distance'],batch['explicit_cage'];receivers=[bpy.data.objects[e['receiver']] for e in batch['entries']]
        print('MATERIAL_BATCH',batch_no,len(receivers),flush=True);src_entries=[];low_entries=[];graphs=[]
        for i,receiver in enumerate(receivers):
            offset=batch['entries'][i]['offset'];low_entries.append((receiver,offset,dest))
            names=mapping[receiver.name]
            if isinstance(names,dict):names=names['sources']
            for name in names:
                if isinstance(name,dict):name=name['source']
                src=bpy.data.objects[name];original_material=src.data.materials[0].name if src.data.materials else ''
                role=graph.role_for_source(name,original_material)
                g=graph.build('PROC_TRIAL_SOURCE_'+name,role,pigment,src,offset)
                # Three independent data channels share one emission bake.
                n=g.node('ShaderNodeCombineColor');n.mode='RGB'
                for socket,ch in zip(list(n.inputs)[:3],['Roughness','Metallic','SurfaceHeight']):g.set(socket,g.channels[ch])
                g.channels['RMP']=n.outputs[0]
                graphs.append(g);src_entries.append((src,offset,g.mat))
        hs,hr=join('PROC_BATCH_HIGH_'+str(batch_no),src_entries,temp)
        ls,lr=join('PROC_BATCH_LOW_'+str(batch_no),low_entries,temp,True)
        cage=create_batch(ls,low_entries,extrusion,temp) if explicit else None
        scene.render.bake.use_cage=explicit;scene.render.bake.cage_object=cage
        # Source originals stay unmodified and hidden. Only this spatially isolated batch participates.
        for o in scene.objects:o.hide_render=True;o.select_set(False)
        for o in (hs,ls):o.hide_render=False;o.hide_set(False);o.select_set(True)
        bpy.context.view_layer.objects.active=ls;scene.render.bake.cage_extrusion=extrusion;scene.render.bake.max_ray_distance=distance
        batchrow={'receivers':[o.name for o in receivers],'extrusion':extrusion,'max_distance':distance,'explicit_cage':explicit,'source_join':hr,'receiver_join':lr,'seconds':{}}
        for ch in CHANNELS:
            im=target_images[ch];im.pixels.foreach_set(canvases[ch].ravel());im.update();target.image=im;dest.node_tree.nodes.active=target
            for g in graphs:g.output(ch,im)
            t=time.time()
            uv=ls.data.uv_layers['BakeUV'];saved=np.empty(len(uv.data)*2,np.float32);uv.data.foreach_get('uv',saved)
            offsets=[(-.25,-.25),(.25,-.25),(-.25,.25),(.25,.25)] if batch['subpixels']==4 else [(0,0)]
            baseline=canvases[ch].copy();accumulated=np.zeros_like(baseline);read=np.empty_like(baseline)
            for jx,jy in offsets:
                uv.data.foreach_set('uv',(saved.reshape(-1,2)+np.array([jx/RES,jy/RES],np.float32)).ravel());ls.data.update()
                im.pixels.foreach_set(baseline.ravel());im.update();bpy.ops.object.bake(type='EMIT');im.pixels.foreach_get(read.ravel());accumulated+=read/len(offsets)
            uv.data.foreach_set('uv',saved);ls.data.update();canvases[ch][:]=accumulated
            batchrow['subpixel_offsets']=offsets;batchrow['seconds'][ch]=time.time()-t
            print('MATERIAL_PASS',batch_no,ch,round(time.time()-t,2),flush=True)
        report['batches'].append(batchrow)
        np.savez_compressed(OUT/'MATERIAL_PROGRESS.npz',**canvases)
        (OUT/'MATERIAL_PROGRESS.json').write_text(json.dumps(report,indent=2)+'\n')
        scene.render.bake.cage_object=None
        for o in [hs,ls]+([cage] if cage else []):m=o.data;bpy.data.objects.remove(o,do_unlink=True);bpy.data.meshes.remove(m)
    assert audit.fingerprint(alllow)==before,'Material emission changed runtime geometry/UV/normals'
    images={}
    for ch in ['BaseColor','PlayerColorMask','SemanticA','SemanticB','SemanticC']:
        path=OUT/(PREFIX+'_'+ch+'.png');write_png(path,canvases[ch],ch=='BaseColor');images[ch]=path
    for c,ch in enumerate(['Roughness','Metallic','SurfaceHeight']):
        a=np.repeat(canvases['RMP'][:,:,c:c+1],4,axis=2);a[:,:,3]=1;path=OUT/(PREFIX+'_'+ch+'.png');write_png(path,a);images[ch]=path
    for ch,path in images.items():report['files'][ch]={'path':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'resolution':[RES,RES],'bit_depth':16,'colorspace':'sRGB' if ch=='BaseColor' else 'Non-Color','UV':'BakeUV'}
    report['geometry_uv_normals_unchanged']=True
    if A.resume:
        previous=json.loads((OUT/'MATERIAL_AUTHORING_REPORT.json').read_text())
        report['trial_receivers']=sorted(set(previous['trial_receivers'])|set(report['trial_receivers']))
        report['refined_receivers']=[o.name for o in trial]
        report['previous_batches']=previous['batches']
        report['refinement']='Chimney panel/crown classification corrected from dark metal to cool-gray coating to match the approved finish. Yellow bands unchanged. Physical maps and geometry unchanged.'
    (OUT/('MATERIAL_AUTHORING_PILOT.json' if A.pilot else 'MATERIAL_AUTHORING_REPORT.json')).write_text(json.dumps(report,indent=2))
    # Save editable source graph library separately. The actual low presentation loads fresh decoded maps.
    for o in scene.objects:o.hide_render=True
    bpy.data.collections.remove(temp)
    for o in alllow:o.hide_render=False
    bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE'].hide_render=True
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/('PROC_MaterialAuthoring_Pilot.blend' if A.pilot else 'PROC_MaterialAuthoring_v02.blend')))
    print('MATERIAL_MAPS_COMPLETE',flush=True)

if __name__=='__main__':main()
