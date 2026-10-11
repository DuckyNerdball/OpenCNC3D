"""PROC approved final-low v01 UV preparation and read-only fresh-reopen audit.

Default: open immutable approved v01, preserve its frozen triangles and
corner normals, unwrap copies, transfer UV/seams, save distinct texture checkpoint.
--verify: open the saved checkpoint and read-only compare it against UV_Report.
"""
import bpy,sys,json,math,hashlib
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
import refinery_uv_audit as audit
from power_plant_bake_common import assert_closed_orientation
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'art/refinery/texture_v02'
SOURCE=ROOT/'art/refinery/final_low_v02/PROC_FinalLow_v02.blend'
DEST=OUT/'PROC_TextureUV_v02.blend'
SOURCE_SHA='52b90144b7446fa94f0c2de2c4ecca8969c4a4e652fac40e6c8199a4997b9b1e'
COLLECTION='PROC_FinalLow_v02_RUNTIME'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def objs():return sorted([o for o in bpy.data.collections[COLLECTION].objects if o.type=='MESH'],key=lambda o:o.name)
def digest(x):return hashlib.sha256(np.ascontiguousarray(x).tobytes()).hexdigest()
def soup(o):
    m=o.data;m.calc_loop_triangles()
    return {'vertices':np.asarray([tuple(v.co) for v in m.vertices],np.float32),'triangles':np.asarray([tuple(t.vertices) for t in m.loop_triangles],np.int32),'normals':np.asarray([[tuple(m.corner_normals[i].vector) for i in t.loops] for t in m.loop_triangles],np.float32),'material':np.asarray([m.polygons[t.polygon_index].material_index for t in m.loop_triangles],np.int32),'smooth':np.asarray([m.polygons[t.polygon_index].use_smooth for t in m.loop_triangles],np.bool_)}
def freeze(objects):
    rows={}
    for o in objects:
        old=o.data;before=soup(o);oldmat=list(old.materials);sharp={tuple(sorted(e.vertices)):e.use_edge_sharp for e in old.edges}
        candidates=[]
        for normal_fan_policy in ('preserve_source_sharp_edges','independent_corner_fans'):
            me=bpy.data.meshes.new(old.name+'_FrozenTriangles');me.from_pydata(before['vertices'].tolist(),[],before['triangles'].tolist());me.update()
            for mat in oldmat:me.materials.append(mat)
            for p,mi,sm in zip(me.polygons,before['material'],before['smooth']):p.material_index=int(mi);p.use_smooth=bool(sm)
            for e in me.edges:e.use_edge_sharp=sharp.get(tuple(sorted(e.vertices)),False) if normal_fan_policy=='preserve_source_sharp_edges' else True
            me.update();me.normals_split_custom_set(before['normals'].reshape(-1,3).tolist());o.data=me
            after=soup(o);error=float(np.max(np.linalg.norm(after['normals']-before['normals'],axis=-1)))
            candidates.append((error,normal_fan_policy,me,after))
            if error<.001:break
        error,normal_fan_policy,me,after=min(candidates,key=lambda x:x[0]);o.data=me
        for _,_,unused,_ in candidates:
            if unused!=me and unused.users==0:bpy.data.meshes.remove(unused)
        for key in ('vertices','triangles','material','smooth'):assert np.array_equal(before[key],after[key]),(o.name,key)
        assert error<.001,(o.name,'corner normal re-encoding',error)
        rows[o.name]={'source_vertices_sha256':digest(before['vertices']),'source_render_triangles_sha256':digest(before['triangles']),'source_triangle_normals_sha256':digest(before['normals']),'frozen_triangle_normals_sha256':digest(after['normals']),'material_smooth_assignment_exact':True,'source_polygons':len(old.polygons),'frozen_triangles':len(me.polygons),'maximum_corner_normal_vector_error':error,'normal_fan_policy':normal_fan_policy}
        if old.users==0:bpy.data.meshes.remove(old)
    return rows

def prepare():
    OUT.mkdir(parents=True,exist_ok=True);assert sha(SOURCE)==SOURCE_SHA
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE));bpy.context.view_layer.update();objects=objs();assert len(objects)==240
    assert_closed_orientation(objects)
    original_matrix={o.name:[list(r) for r in o.matrix_world] for o in objects}
    freeze_report={o.name:{'maximum_corner_normal_vector_error':0.,'method':'Approved source is already frozen triangles; no geometry or normals re-encoding'} for o in objects};assert sum(len(o.data.polygons) for o in objects)==9992
    frozen=audit.fingerprint(objects)
    temp=bpy.data.collections.new('PROC_TEMP_UV_ONLY');bpy.context.scene.collection.children.link(temp);copies=[]
    for o in bpy.context.scene.objects:o.select_set(False)
    for src in objects:
        ob=src.copy();ob.data=src.data.copy();ob.name='UVTEMP_'+src.name;ob.parent=None;ob.matrix_world=src.matrix_world.copy();temp.objects.link(ob)
        ob.hide_set(False);ob.hide_viewport=False;ob.hide_render=True
        while ob.data.uv_layers:ob.data.uv_layers.remove(ob.data.uv_layers[0])
        ob.data.uv_layers.new(name='BakeUV');ob.select_set(True);copies.append(ob)
    bpy.context.view_layer.objects.active=copies[0]
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.006,area_weight=.15,correct_aspect=True,scale_to_bounds=True)
    bpy.ops.object.mode_set(mode='OBJECT');hard_count=sum(audit.split_hard_edges(o) for o in copies)
    iterations=[];pack=.0028
    for attempt in range(8):
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.unwrap(method='ANGLE_BASED',margin=pack,margin_method='FRACTION')
        bpy.ops.uv.average_islands_scale();bpy.ops.uv.pack_islands(rotate=True,margin=pack,margin_method='FRACTION')
        bpy.ops.object.mode_set(mode='OBJECT')
        result,triangles=audit.atlas_audit(copies,resolutions=(4096,))
        summary={'attempt':attempt,'pack':pack,'overlap_pairs':result['exact_overlap']['positive_area_pairs'],'invalid':len(result['invalid_uv_triangles']),'hard_seam_errors':result['hard_edges_without_uv_seam'],'spacing':result['spacing']};iterations.append(summary);print('PROC_UV_ATTEMPT',json.dumps(summary),flush=True)
        ok=(not result['exact_overlap']['positive_area_pairs'] and not result['invalid_uv_triangles'] and not result['hard_edges_without_uv_seam'])
        if ok and not result['spacing']['violations_below_18_pixels'] and result['spacing']['minimum_border_pixels']>=10:break
        for collision in result['exact_overlap']['pairs']:
            for key in ('a','b'):
                row=triangles[collision[key]];ob=bpy.data.objects[row['object']]
                for li in ob.data.polygons[row['polygon']].loop_indices:ob.data.edges[ob.data.loops[li].edge_index].use_seam=True
        for name,pi,_ in result['invalid_uv_triangles']:
            ob=bpy.data.objects[name]
            for li in ob.data.polygons[pi].loop_indices:ob.data.edges[ob.data.loops[li].edge_index].use_seam=True
        for row in result['hard_normal_audit']:
            ob=bpy.data.objects[row['object']]
            for missing in row['missing_seam_edges']:ob.data.edges[missing['edge']].use_seam=True
        if result['spacing']['violations_below_18_pixels'] or result['spacing']['minimum_border_pixels']<10:pack*=1.20
    assert not result['exact_overlap']['positive_area_pairs'],result['exact_overlap']
    assert not result['invalid_uv_triangles'] and not result['hard_edges_without_uv_seam']
    assert not result['spacing']['violations_below_18_pixels'] and result['spacing']['minimum_border_pixels']>=10,result['spacing']
    for src,tmp in zip(objects,copies):
        assert audit.topology(src.data)==audit.topology(tmp.data)
        while src.data.uv_layers:src.data.uv_layers.remove(src.data.uv_layers[0])
        uv=src.data.uv_layers.new(name='BakeUV');coords=np.empty(len(tmp.data.loops)*2,np.float32);tmp.data.uv_layers['BakeUV'].data.foreach_get('uv',coords);uv.data.foreach_set('uv',coords)
        for a,b in zip(src.data.edges,tmp.data.edges):a.use_seam=b.use_seam
        uv.active_render=True
    for ob in copies:
        me=ob.data;bpy.data.objects.remove(ob,do_unlink=True);bpy.data.meshes.remove(me)
    bpy.data.collections.remove(temp)
    assert audit.fingerprint(objects)==frozen
    final,_=audit.atlas_audit(objects,write_layout=OUT/'UV_Atlas_4096.png')
    assert all(not x['sampled_overlap_pixels'] for x in final['resolutions'].values())
    uvhash={o.name:digest(np.asarray([tuple(v.uv) for v in o.data.uv_layers['BakeUV'].data],np.float32)) for o in objects}
    scene=bpy.context.scene;scene['stage']='Approved final low v01, frozen render triangles and BakeUV only; material trial pending';scene['source_final_low_sha256']=SOURCE_SHA;scene['runtime_integrated']=False
    bpy.ops.wm.save_as_mainfile(filepath=str(DEST))
    report={'source_file':str(SOURCE),'source_sha256':SOURCE_SHA,'output_file':str(DEST),'output_sha256':sha(DEST),'runtime_collection':COLLECTION,'objects':240,'triangles':9992,'working_resolution':4096,'render_triangle_geometry_exact':True,'material_smooth_assignments_exact':True,'corner_normals_reencoded_tolerance':.001,'maximum_corner_normal_vector_error':max(r['maximum_corner_normal_vector_error'] for r in freeze_report.values()),'freeze_per_object':freeze_report,'frozen_fingerprints':frozen,'uv_sha256_per_object':uvhash,'object_world_matrices':original_matrix,'hard_normal_seams_marked':hard_count,'iterations':iterations,'atlas':final,'source_unchanged':sha(SOURCE)==SOURCE_SHA,'limitation':'4K working atlas and 2K/1K density diagnostics are UV proof only; actual delivered-resolution physical/detail transfer must pass the representative material trial.'}
    (OUT/'UV_Report.json').write_text(json.dumps(report,indent=2)+'\n');print('PROC_UV_SAVED',DEST,report['output_sha256'],flush=True)

def verify():
    record=json.loads((OUT/'UV_Report.json').read_text());before=sha(DEST);assert before==record['output_sha256']
    bpy.ops.wm.open_mainfile(filepath=str(DEST));bpy.context.view_layer.update();objects=objs()
    assert len(objects)==240 and sum(len(o.data.polygons) for o in objects)==9992
    assert audit.fingerprint(objects)==record['frozen_fingerprints']
    for o in objects:
        assert len(o.data.uv_layers)==1 and o.data.uv_layers[0].name=='BakeUV'
        assert digest(np.asarray([tuple(v.uv) for v in o.data.uv_layers['BakeUV'].data],np.float32))==record['uv_sha256_per_object'][o.name]
        assert [list(r) for r in o.matrix_world]==record['object_world_matrices'][o.name]
    final,_=audit.atlas_audit(objects)
    assert not final['exact_overlap']['positive_area_pairs'] and not final['invalid_uv_triangles'] and not final['hard_edges_without_uv_seam']
    assert not final['spacing']['violations_below_18_pixels'] and all(not x['sampled_overlap_pixels'] for x in final['resolutions'].values())
    frozen_soups={o.name:soup(o) for o in objects}
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE));bpy.context.view_layer.update()
    source_comparison=[]
    for original in objs():
        a=soup(original);b=frozen_soups[original.name]
        for field in ('vertices','triangles','material','smooth'):assert np.array_equal(a[field],b[field]),(original.name,field)
        delta=float(np.max(np.linalg.norm(a['normals']-b['normals'],axis=-1)))
        assert delta<.001,(original.name,delta)
        assert [list(r) for r in original.matrix_world]==record['object_world_matrices'][original.name]
        source_comparison.append({'object':original.name,'source_render_geometry_material_smooth_exact':True,'corner_normal_max_error':delta})
    assert before==sha(DEST) and sha(SOURCE)==SOURCE_SHA
    report={'independent_source_reopen_comparison':source_comparison,'passed':True,'output_file':str(DEST),'output_sha256':before,'source_sha256_unchanged':SOURCE_SHA,'verified_file_not_rewritten':True,'frozen_geometry_normals_material_smooth_transforms_identical':True,'uv_hashes_identical':True,'objects':240,'triangles':9992,'atlas':final}
    (OUT/'UV_Reopen_Report.json').write_text(json.dumps(report,indent=2)+'\n');print('PROC_UV_REOPEN_PASS',before,flush=True)
if __name__=='__main__':
    verify() if '--verify' in sys.argv else prepare()
