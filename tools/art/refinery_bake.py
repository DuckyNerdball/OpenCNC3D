"""Physical normal/local-AO transfer from approved PROC high v04 to UV-frozen low v01.

blender -b -t 4 --python tools/art/refinery_bake.py -- [--pilot] [--resolution 4096]
No source file is overwritten. Each receiver is baked separately with its intact
triangulation and named source group. Pixels compose by UV ownership, never color.
"""
import argparse
import hashlib
import json
import math
import struct
import sys
import time
import zlib
from collections import Counter
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'art/refinery/texture_v01'
ARGS = argparse.ArgumentParser()
ARGS.add_argument('--resolution', type=int, default=4096)
ARGS.add_argument('--pilot', action='store_true')
ARGS.add_argument('--receivers', nargs='*')
ARGS.add_argument('--samples', type=int, default=8)
ARGS.add_argument('--resume', action='store_true')
ARGS.add_argument('--skip-source-join', action='store_true')
A = ARGS.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
RES = A.resolution
MARGIN = max(2, int(round(8 * RES / 4096)))
PREFIX = ('Pilot' if A.skip_source_join else 'PilotJoined') if A.pilot else 'Working'
sys.path.insert(0,str(ROOT/'tools/art'))
from refinery_bake_common import joined_source, ao_context_names
PAIR_DIR = OUT / (('pilot_pairs' if A.skip_source_join else 'pilot_joined_pairs') if A.pilot else 'physical_pairs')
PAIR_DIR.mkdir(exist_ok=True)
SOURCE = OUT / 'PROC_TextureUV_v01.blend'
MAP = json.loads((OUT / 'SOURCE_RECEIVER_MAP.json').read_text())
SHA = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
source_sha = SHA(SOURCE)
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
scene = bpy.context.scene
high = bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE']
low = bpy.data.collections['PROC_FinalLow_v01_RUNTIME']
receivers = sorted([o for o in low.objects if o.type == 'MESH'], key=lambda o: o.name)
assert len(receivers) == 240
assert sum(len(o.data.polygons) for o in receivers) == 9796
assert all(len(p.vertices) == 3 for o in receivers for p in o.data.polygons)
assert all('BakeUV' in o.data.uv_layers for o in receivers)
sys.path.insert(0,str(Path(__file__).resolve().parent))
from refinery_bake_common import assert_closed_orientation
assert_closed_orientation(receivers)


def geometry_hash():
    h = hashlib.sha256()
    for o in receivers:
        h.update(o.name.encode())
        for v in o.data.vertices:
            h.update(struct.pack('<3f', *v.co))
        for p in o.data.polygons:
            h.update(struct.pack('<3I', *p.vertices))
            h.update(struct.pack('<?', p.use_smooth))
        for n in o.data.corner_normals:
            h.update(struct.pack('<3f', *n.vector))
        for uv in o.data.uv_layers['BakeUV'].data:
            h.update(struct.pack('<2f', *uv.uv))
        h.update(np.asarray(o.matrix_world, np.float64).tobytes())
    return h.hexdigest()


frozen_hash = geometry_hash()
visibility = {o.name: (o.hide_render, o.hide_get()) for o in scene.objects}
high.hide_render = False
high.hide_viewport = False
def enable(lc):
    if lc.collection == high: lc.exclude=False; lc.hide_viewport=False
    for child in lc.children: enable(child)
enable(bpy.context.view_layer.layer_collection)
for o in high.objects:
    o.hide_set(False)
bpy.context.view_layer.update()
deps = bpy.context.evaluated_depsgraph_get()
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = A.samples
scene.render.threads_mode = 'FIXED'
scene.render.threads = 4
scene.render.bake.use_selected_to_active = True
scene.render.bake.use_cage = False
scene.render.bake.use_clear = False
scene.render.bake.margin = MARGIN
scene.render.bake.margin_type = 'EXTEND'
scene.render.bake.normal_space = 'TANGENT'
scene.render.bake.normal_r = 'POS_X'
scene.render.bake.normal_g = 'POS_Y'
scene.render.bake.normal_b = 'POS_Z'
scene.render.bake.target = 'IMAGE_TEXTURES'
scene.cycles.use_denoising = False
for o in scene.objects:
    o.select_set(False)
    o.hide_render = True
for o in receivers:
    o.data.uv_layers.active = o.data.uv_layers['BakeUV']
    o.data.uv_layers['BakeUV'].active_render = True

# Own copies avoid changing source material graphs when targets are assigned.
material_copies = {}
for o in receivers:
    for slot in o.material_slots:
        source_mat = slot.material
        if source_mat.name not in material_copies:
            mat = source_mat.copy()
            mat.name = 'PROC_PhysicalBake_' + source_mat.name
            mat.use_nodes = True
            material_copies[source_mat.name] = mat
        slot.material = material_copies[source_mat.name]

normal_image = bpy.data.images.new('PROC_' + PREFIX + '_Normal_BAKE', RES, RES, alpha=True, float_buffer=True)
ao_image = bpy.data.images.new('PROC_' + PREFIX + '_AO_BAKE', RES, RES, alpha=True, float_buffer=True)
for im in (normal_image, ao_image):
    im.colorspace_settings.name = 'Non-Color'
normal_canvas = np.ones((RES, RES, 4), np.float32)
normal_canvas[:, :, :2] = .5
ao_canvas = np.ones((RES, RES, 4), np.float32)
neutral_normal = normal_canvas.copy()
neutral_ao = ao_canvas.copy()
readback = np.empty((RES, RES, 4), np.float32)

# All receiver materials can share the target atlas, but only one receiver is active.
for mat in material_copies.values():
    target = mat.node_tree.nodes.new('ShaderNodeTexImage')
    target.name = 'PROC_PHYSICAL_TARGET'
    target.image = normal_image
    mat.node_tree.nodes.active = target

source_links = {}
source_materials = {m for o in high.objects if o.type == 'MESH' for m in o.data.materials if m}
for mat in source_materials:
    output = next(n for n in mat.node_tree.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
    source_links[mat] = (output, output.inputs['Surface'].links[0].from_socket)
    ao = mat.node_tree.nodes.new('ShaderNodeAmbientOcclusion')
    ao.name = 'PROC_LOCAL_AO_PHYSICAL'
    ao.samples = A.samples
    ao.only_local = False  # Only the explicitly named source group is render-visible.
    ao.inside = False
    ao.inputs['Distance'].default_value = .65
    emission = mat.node_tree.nodes.new('ShaderNodeEmission')
    emission.name = 'PROC_LOCAL_AO_EMIT'
    mat.node_tree.links.new(ao.outputs['Color'], emission.inputs['Color'])


def set_source_shader(pass_name):
    for mat, (output, original_socket) in source_links.items():
        if pass_name == 'Normal':
            mat.node_tree.links.new(original_socket, output.inputs['Surface'])
        else:
            mat.node_tree.links.new(mat.node_tree.nodes['PROC_LOCAL_AO_EMIT'].outputs[0], output.inputs['Surface'])


def set_target(im):
    for mat in material_copies.values():
        target = mat.node_tree.nodes['PROC_PHYSICAL_TARGET']
        target.image = im
        mat.node_tree.nodes.active = target
        for node in mat.node_tree.nodes:
            node.select = node == target


def raster_mask(o):
    mask = np.zeros((RES, RES), np.bool_)
    uv = o.data.uv_layers['BakeUV'].data
    for poly in o.data.polygons:
        p = np.asarray([uv[j].uv for j in poly.loop_indices], np.float64)
        lo = np.maximum(np.ceil(p.min(0) * RES - .5).astype(int), 0)
        hi = np.minimum(np.floor(p.max(0) * RES - .5).astype(int), RES - 1)
        if np.any(hi < lo):
            continue
        aa, bb, cc = p
        den = (bb[1]-cc[1])*(aa[0]-cc[0]) + (cc[0]-bb[0])*(aa[1]-cc[1])
        if abs(den) < 1e-16:
            continue
        xx = (np.arange(lo[0], hi[0]+1)[None, :] + .5)/RES
        yy = (np.arange(lo[1], hi[1]+1)[:, None] + .5)/RES
        wa = ((bb[1]-cc[1])*(xx-cc[0]) + (cc[0]-bb[0])*(yy-cc[1]))/den
        wb = ((cc[1]-aa[1])*(xx-cc[0]) + (aa[0]-cc[0])*(yy-cc[1]))/den
        wc = 1-wa-wb
        mask[lo[1]:hi[1]+1, lo[0]:hi[0]+1] |= (wa >= -1e-7) & (wb >= -1e-7) & (wc >= -1e-7)
    return mask


def dilate(mask, radius):
    result = mask.copy()
    for _ in range(radius):
        old = result.copy()
        result[1:, :] |= old[:-1, :]
        result[:-1, :] |= old[1:, :]
        result[:, 1:] |= old[:, :-1]
        result[:, :-1] |= old[:, 1:]
        result[1:, 1:] |= old[:-1, :-1]
        result[:-1, :-1] |= old[1:, 1:]
        result[1:, :-1] |= old[:-1, 1:]
        result[:-1, 1:] |= old[1:, :-1]
    return result


def cage_for(name):
    if 'faceted_cladding' in name:return .32,.72
    if 'inner_well_floor' in name:return .65,1.4
    if 'dark_tray' in name:return .24,.52
    if 'Foundation' in name:return .12,.28
    if 'rounded_pipe' in name:return .20,.45
    return .20,.45


def source_tree(names):
    verts, polys, owners = [], [], []
    for name in names:
        eo = bpy.data.objects[name].evaluated_get(deps)
        mesh = eo.to_mesh()
        offset = len(verts)
        verts.extend(eo.matrix_world @ v.co for v in mesh.vertices)
        polys.extend(tuple(offset+i for i in p.vertices) for p in mesh.polygons)
        owners.extend([name] * len(mesh.polygons))
        eo.to_mesh_clear()
    return BVHTree.FromPolygons(verts, polys), owners


def diagnostic(o, source_names, extrusion, maxdist):
    tree, owners = source_tree(source_names)
    hits, misses, backface, hit_sources = 0, [], [], Counter()
    m = o.data
    normal_matrix = o.matrix_world.inverted().transposed().to_3x3()
    stride = max(1, len(m.polygons)//160)
    for poly in list(m.polygons)[::stride]:
        locs = [o.matrix_world @ m.vertices[i].co for i in poly.vertices]
        normals = [(normal_matrix @ m.corner_normals[i].vector).normalized() for i in poly.loop_indices]
        for weights in ((1/3, 1/3, 1/3), (.9, .05, .05), (.05, .9, .05), (.05, .05, .9)):
            point = sum((p*w for p,w in zip(locs,weights)), Vector())
            normal = sum((n*w for n,w in zip(normals,weights)), Vector()).normalized()
            pos, hn, face, dist = tree.ray_cast(point+normal*extrusion, -normal, maxdist)
            if pos is None:
                misses.append({'polygon':poly.index,'barycentric':weights})
            else:
                hits += 1
                hit_sources[owners[face]] += 1
                if hn.dot(normal) < 0:
                    backface.append({'polygon':poly.index,'source':owners[face],'normal_dot':hn.dot(normal)})
    return {'method':'evaluated named source BVH; triangle-center and near-corner samples along interpolated receiver corner normal; approximate implicit cage policy, not exact Cycles texel replay',
            'samples':hits+len(misses),'hits':hits,'misses':len(misses),'opposite_facing_hits':len(backface),
            'miss_examples':misses[:16],'opposite_facing_examples':backface[:16],'observed_sources':dict(hit_sources)}


def write_png16(path, values):
    # Non-Color data channels encoded raw: no view transform or gamma conversion.
    arr = np.clip(np.rint(values[::-1]*65535),0,65535).astype('>u2')
    h,w,c = arr.shape
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    body = b''.join(b'\x00'+row.tobytes() for row in arr)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,16,6,0,0,0))+chunk(b'IDAT',zlib.compress(body,4))+chunk(b'IEND',b''))


pilot_names = list(MAP['receiver_to_sources'])
selected_names = set(A.receivers or pilot_names)
assert selected_names <= {o.name for o in receivers}
chosen = [o for o in receivers if o.name in selected_names]
records = []
composition_ownership = np.full((RES,RES),-1,np.int16)
start = time.time()
for i,o in enumerate(chosen):
    source_names = MAP['receiver_to_sources'][o.name]
    srcs = [bpy.data.objects[n] for n in source_names]
    ext,maxdist = cage_for(o.name)
    pair_path = PAIR_DIR/(o.name+'.npz')
    pair_json = PAIR_DIR/(o.name+'.json')
    ownership = raster_mask(o)
    padded = dilate(ownership,MARGIN)
    overlap = padded & (composition_ownership >= 0)
    assert not np.any(overlap),f'Padding overlap for {o.name}: {int(overlap.sum())} pixels'
    indices = np.flatnonzero(padded)
    composition_ownership[padded] = i
    print('PROC_PAIR_START',i+1,len(chosen),o.name,len(source_names),'pixels',int(ownership.sum()),flush=True)
    t0 = time.time()
    if A.resume and pair_path.exists() and pair_json.exists():
        rec = json.loads(pair_json.read_text())
        assert rec['uv_source_sha256'] == source_sha and rec['resolution'] == RES
        pair = np.load(pair_path)
        assert np.array_equal(pair['indices'],indices)
        normal_canvas.reshape(-1,4)[indices] = pair['Normal']
        ao_canvas.reshape(-1,4)[indices] = pair['AO']
        records.append(rec)
        print('PROC_PAIR_RESUMED',o.name,flush=True)
        continue
    joined=None
    joined_diagnostic=None
    if not A.skip_source_join and len(srcs)>1:
        joined,joined_diagnostic=joined_source(source_names)
    active_sources=[joined] if joined else srcs
    context_names=ao_context_names(o,source_names,.65)
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    for ob in [o]+active_sources:
        ob.hide_render = False
        ob.hide_set(False)
        ob.select_set(True)
    bpy.context.view_layer.objects.active = o
    scene.render.bake.cage_extrusion = ext
    scene.render.bake.max_ray_distance = maxdist
    bpy.context.view_layer.update()
    deps = bpy.context.evaluated_depsgraph_get()
    rays = diagnostic(o,source_names,ext,maxdist)
    pair_values = {}
    channels = {}
    for channel,im,neutral,canvas in [('Normal',normal_image,neutral_normal,normal_canvas),('AO',ao_image,neutral_ao,ao_canvas)]:
        set_source_shader(channel)
        for context_name in context_names:
            bpy.data.objects[context_name].hide_render = channel!='AO'
        set_target(im)
        im.pixels.foreach_set(neutral.ravel())
        im.update()
        tb = time.time()
        # Four subtexel ray samples for narrow physical seams; final map stays 4096.
        # Uniform UV translation preserves tangent directions and exact corner normals.
        uv_layer=o.data.uv_layers['BakeUV'];saved_uv=np.empty(len(uv_layer.data)*2,np.float32);uv_layer.data.foreach_get('uv',saved_uv)
        offsets=[(-.25,-.25),(.25,-.25),(-.25,.25),(.25,.25)] if ('faceted_cladding' in o.name or 'inner_well_floor' in o.name) else [(0,0)]
        accumulated=np.zeros_like(readback)
        for jx,jy in offsets:
            translated=saved_uv.reshape(-1,2)+np.array([jx/RES,jy/RES],np.float32)
            uv_layer.data.foreach_set('uv',translated.ravel());o.data.update()
            im.pixels.foreach_set(neutral.ravel());im.update()
            bpy.ops.object.bake(type='NORMAL' if channel=='Normal' else 'EMIT')
            im.pixels.foreach_get(readback.ravel());accumulated+=readback/len(offsets)
        uv_layer.data.foreach_set('uv',saved_uv);o.data.update()
        readback[:]=accumulated
        if channel=='Normal':
            vec=readback[:,:,:3]*2-1;vec/=np.maximum(np.linalg.norm(vec,axis=2,keepdims=True),1e-8);readback[:,:,:3]=vec*.5+.5
        assert np.isfinite(readback).all(),(o.name,channel,'nonfinite')
        channels[channel] = {'seconds':time.time()-tb,'covered_black_pixels':int(np.sum(np.max(readback[:,:,:3],axis=2)[ownership] < .0001)),
                             'covered_min':float(readback[:,:,:3][ownership].min()) if ownership.any() else None,
                             'covered_max':float(readback[:,:,:3][ownership].max()) if ownership.any() else None}
        pair_values[channel] = readback.reshape(-1,4)[indices].copy()
        canvas.reshape(-1,4)[indices] = pair_values[channel]
        print('PROC_PASS_DONE',o.name,channel,round(channels[channel]['seconds'],2),flush=True)
    rec = {'receiver':o.name,'sources':source_names,'uv_source_sha256':source_sha,'resolution':RES,'covered_pixels':int(ownership.sum()),
           'dilated_pixels':len(indices),'extrusion':ext,'max_ray_distance':maxdist,'ao_distance':.65,'ao_scope':'project named sources; AO rays see original high neighbors within .65 of receiver bounds; guides/background/low hidden',
           'ao_context_names':context_names,'joined_source':joined_diagnostic,
           'normal_convention':'tangent OpenGL +Y','subtexel_offsets':offsets,'source_coverage':rays,'channels':channels,'total_seconds':time.time()-t0,
           'pair_buffer':'indices address bottom-left row-major atlas; Normal/AO contain original unmodified float RGBA for UV-owned texels plus declared dilation; neutral outside pixels omitted'}
    np.savez_compressed(pair_path,indices=indices.astype(np.int32),owned_indices=np.flatnonzero(ownership).astype(np.int32),**pair_values)
    pair_json.write_text(json.dumps(rec,indent=2)+'\n')
    records.append(rec)
    for ob in [o]+active_sources+[bpy.data.objects[n] for n in context_names]:
        ob.hide_render = True
        ob.select_set(False)
    if joined:
        mesh=joined.data
        bpy.data.objects.remove(joined,do_unlink=True)
        bpy.data.meshes.remove(mesh)
    print('PROC_PAIR_DONE',o.name,round(time.time()-t0,2),'ray_misses',rays['misses'],'opposite',rays['opposite_facing_hits'],flush=True)

for mat,(output,socket) in source_links.items():
    mat.node_tree.links.new(socket,output.inputs['Surface'])
    mat.node_tree.nodes.remove(mat.node_tree.nodes['PROC_LOCAL_AO_EMIT'])
    mat.node_tree.nodes.remove(mat.node_tree.nodes['PROC_LOCAL_AO_PHYSICAL'])

output_maps = []
decoded = {}
for channel,canvas in [('Normal',normal_canvas),('AO',ao_canvas)]:
    path = OUT/f'PROC_{PREFIX}_{channel}.png'
    write_png16(path,canvas)
    im = bpy.data.images.load(str(path),check_existing=False)
    im.name = f'PROC_{PREFIX}_{channel}'
    im.colorspace_settings.name = 'Non-Color'
    im.pixels.foreach_get(readback.ravel())
    quant_error = float(np.max(np.abs(readback-canvas)))
    assert quant_error <= 2/65535,(channel,quant_error)
    decoded[channel] = im
    output_maps.append({'channel':channel,'path':str(path.relative_to(ROOT)),'sha256':SHA(path),'resolution':[RES,RES],
                        'bit_depth':16,'colorspace':'Non-Color','fresh_decode_max_error':quant_error})

for mat in material_copies.values():
    nodes,links = mat.node_tree.nodes,mat.node_tree.links
    bs = next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
    uv = nodes.new('ShaderNodeUVMap');uv.uv_map='BakeUV';uv.name='PROC_BakeUV'
    tex = nodes.new('ShaderNodeTexImage');tex.image=decoded['Normal'];tex.name='PROC_Physical_Normal'
    links.new(uv.outputs[0],tex.inputs['Vector'])
    normal = nodes.new('ShaderNodeNormalMap');normal.space='TANGENT';normal.uv_map='BakeUV';normal.name='PROC_Physical_NormalMap'
    links.new(tex.outputs['Color'],normal.inputs['Color']);links.new(normal.outputs['Normal'],bs.inputs['Normal'])
    ao = nodes.new('ShaderNodeTexImage');ao.image=decoded['AO'];ao.name='PROC_Physical_LocalAO';links.new(uv.outputs[0],ao.inputs['Vector'])
    # Local AO is deliberately an available data map; material trial decides its use.
    nodes.remove(nodes['PROC_PHYSICAL_TARGET'])
for im in (normal_image,ao_image):
    bpy.data.images.remove(im)
for o in scene.objects:
    if o.name in visibility:
        o.hide_render,o_hidden = visibility[o.name];o.hide_set(o_hidden)
for o in receivers:
    o.hide_render=False;o.hide_set(False)
high.hide_render=True;high.hide_viewport=True
bpy.context.view_layer.update()
assert geometry_hash()==frozen_hash,'Approved geometry, UV or corner normals changed during bake'
assert SHA(SOURCE)==source_sha,'UV source file modified'
scene['physical_bake_status']='normal and isolated local AO maps; diagnostic findings recorded; material trial pending'
scene['physical_bake_uv_source_sha256']=source_sha
scene['physical_bake_resolution']=RES
scene['physical_bake_mapping']='SOURCE_RECEIVER_MAP.json'
report={'status':'physical bake output for review; inspect ray misses and opposite-facing hits before acceptance','uv_source':str(SOURCE.relative_to(ROOT)),'uv_source_sha256':source_sha,
        'mapping_sha256':SHA(OUT/'SOURCE_RECEIVER_MAP.json'),'blender_version':bpy.app.version_string,'resolution':RES,'margin_pixels':MARGIN,'samples':A.samples,'threads':4,
        'mode':'pilot' if A.pilot else 'all' if len(chosen)==240 else 'selected','receivers_baked':len(chosen),'all_receivers':240,'normal_convention':'OpenGL +Y tangent',
        'geometry_uv_corner_normal_hash':frozen_hash,'output_maps':output_maps,'seconds_total':time.time()-start,
        'source_coverage_totals':{k:sum(r['source_coverage'][k] for r in records) for k in ('samples','hits','misses','opposite_facing_hits')},'receivers':records,
        'material_status':'provisional original clay with decoded physical normal attached; local AO available; no artistic final material approval implied'}
file=OUT/(('PROC_PhysicalBake_'+PREFIX+'.blend') if A.pilot else 'PROC_PhysicalBake_v01.blend')
bpy.ops.wm.save_as_mainfile(filepath=str(file),compress=True)
report['blend']=str(file.relative_to(ROOT));report['blend_sha256']=SHA(file)
(OUT/(('PHYSICAL_BAKE_'+PREFIX.upper()+'.json') if A.pilot else 'PHYSICAL_BAKE_REPORT.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PROC_PHYSICAL_BAKE_DONE',json.dumps({k:v for k,v in report.items() if k!='receivers'}),flush=True)
