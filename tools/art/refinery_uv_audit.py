"""Refinery UV audit helpers; copied from the audited Power Plant/Mammoth workflow. No autorun."""
import bpy,math,json,hashlib,struct,zlib
from pathlib import Path
import numpy as np
RES=4096
MIN_GUTTER=18.
UV_TOLERANCE=2e-6
UV_AREA_EPSILON=1e-12
INTERIOR_BARYCENTRIC_EPSILON=1e-7
HARD_NORMAL_ANGLE_DEGREES=1.0

def fingerprint(objects):
    rows={}
    for o in sorted(objects,key=lambda x:x.name):
        m=o.data;h=hashlib.sha256()
        for data,prop,dtype,size in [(m.vertices,'co',np.float32,3),(m.loops,'vertex_index',np.int32,1),
             (m.polygons,'loop_start',np.int32,1),(m.polygons,'loop_total',np.int32,1),
             (m.polygons,'material_index',np.int32,1),(m.polygons,'use_smooth',np.bool_,1),
             (m.corner_normals,'vector',np.float32,3)]:
            a=np.empty(len(data)*size,dtype=dtype);data.foreach_get(prop,a);h.update(a.tobytes())
        for key in ['track_sidewall_mask','tread_scroll_mask']:
            attr=m.attributes.get(key)
            if attr:
                a=np.empty(len(attr.data),np.float32);attr.data.foreach_get('value',a);h.update(key.encode());h.update(a.tobytes())
        h.update(np.asarray(o.matrix_world,np.float64).tobytes());h.update(np.asarray(o.matrix_basis,np.float64).tobytes())
        h.update(np.asarray(o.matrix_parent_inverse,np.float64).tobytes());h.update((o.parent.name if o.parent else '').encode())
        h.update(str([s.material.name if s.material else None for s in o.material_slots]).encode())
        rows[o.name]=h.hexdigest()
    return rows

def topology(m):
    return [tuple(p.vertices) for p in m.polygons]

def edge_loops(m):
    out={e.index:[] for e in m.edges}
    for p in m.polygons:
        ll=list(p.loop_indices)
        for i,a in enumerate(ll):
            b=ll[(i+1)%len(ll)];out[m.loops[a].edge_index].append((p.index,{m.loops[a].vertex_index:a,m.loops[b].vertex_index:b}))
    return out

def split_hard_edges(o):
    m=o.data;uv=m.uv_layers['BakeUV'];adj=edge_loops(m);count=0
    for e in m.edges:
        if len(adj[e.index])!=2:e.use_seam=True;continue
        (_,a),(_,b)=adj[e.index]
        split=any((uv.data[a[v]].uv-uv.data[b[v]].uv).length>1e-7 for v in e.vertices)
        hard=any(m.corner_normals[a[v]].vector.dot(m.corner_normals[b[v]].vector)<math.cos(math.radians(.25)) for v in e.vertices)
        e.use_seam=split or hard;count+=bool(hard)
    return count

def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])

def clipped_area(a,b):
    poly=list(a);sgn=1 if cross(*b)>0 else -1
    for j in range(3):
        p,q=b[j],b[(j+1)%3];new=[]
        if not poly:return 0.
        last=poly[-1];dl=sgn*cross(p,q,last)
        for curr in poly:
            dc=sgn*cross(p,q,curr)
            if (dc>=0)!=(dl>=0):
                t=dl/(dl-dc);new.append((last[0]+t*(curr[0]-last[0]),last[1]+t*(curr[1]-last[1])))
            if dc>=0:new.append(curr)
            last=curr;dl=dc
        poly=new
    return abs(sum(p[0]*q[1]-q[0]*p[1] for p,q in zip(poly,poly[1:]+poly[:1])))*.5 if len(poly)>=3 else 0.

def exact_overlap(triangles):
    grid={};bad=[];checked=0;step=1/64
    for i,row in enumerate(triangles):
        pts=row['uv'];lo=np.min(pts,axis=0);hi=np.max(pts,axis=0)
        cells=[(x,y) for x in range(math.floor(lo[0]/step),math.floor(hi[0]/step)+1) for y in range(math.floor(lo[1]/step),math.floor(hi[1]/step)+1)]
        cand={k for c in cells for k in grid.get(c,[])}
        for k in cand:
            other=triangles[k]
            if hi[0]<=other['lo'][0] or lo[0]>=other['hi'][0] or hi[1]<=other['lo'][1] or lo[1]>=other['hi'][1]:continue
            checked+=1;area=clipped_area(pts,other['uv'])
            if area>1e-12:bad.append({'a':i,'b':k,'area_uv':area,'area_pixels':area*RES*RES})
        row['lo']=lo;row['hi']=hi
        for c in cells:grid.setdefault(c,[]).append(i)
    return {'positive_area_pairs':len(bad),'pairs':bad,'tested_pairs':checked,'area_tolerance_uv':1e-12}

def point_seg(p,a,b):
    dx=b[0]-a[0];dy=b[1]-a[1];q=dx*dx+dy*dy
    t=max(0,min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/q)) if q else 0.
    return math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy)

def segdist(a,b,c,d):
    if cross(a,b,c)*cross(a,b,d)<0 and cross(c,d,a)*cross(c,d,b)<0:return 0.
    return min(point_seg(a,c,d),point_seg(b,c,d),point_seg(c,a,b),point_seg(d,a,b))

def island_boundaries(objects):
    boundaries=[];total=0;island_rows=[]
    for o in objects:
        m=o.data;uv=m.uv_layers['BakeUV'];adj=edge_loops(m);parent=list(range(len(m.polygons)))
        def find(i):
            while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
            return i
        discontinuous=[]
        for e in m.edges:
            rows=adj[e.index]
            if len(rows)==2:
                (fa,a),(fb,b)=rows
                joined=all((uv.data[a[v]].uv-uv.data[b[v]].uv).length<1e-7 for v in e.vertices)
                if joined:parent[find(fa)]=find(fb);continue
            discontinuous.append((e,rows))
        roots=sorted({find(i) for i in parent});ids={r:total+j for j,r in enumerate(roots)}
        for e,rows in discontinuous:
            for face,loops in rows:
                a,b=[tuple(uv.data[loops[v]].uv) for v in e.vertices]
                if math.dist(a,b)>1e-12:boundaries.append((a,b,ids[find(face)],o.name))
        island_rows.append({'object':o.name,'islands':len(roots)});total+=len(roots)
    return boundaries,total,island_rows

def spacing_audit(boundaries):
    reach=28/RES;cell=.02;grid={};minimum=1.;pair=None;comparisons=0;violations=[]
    for i,(a,b,island,name) in enumerate(boundaries):
        lo=(min(a[0],b[0]),min(a[1],b[1]));hi=(max(a[0],b[0]),max(a[1],b[1]))
        query=[(x,y) for x in range(math.floor((lo[0]-reach)/cell),math.floor((hi[0]+reach)/cell)+1) for y in range(math.floor((lo[1]-reach)/cell),math.floor((hi[1]+reach)/cell)+1)]
        for k in {k for c in query for k in grid.get(c,[])}:
            c,d,j,other=boundaries[k]
            if island==j:continue
            if max(c[0],d[0])+reach<lo[0] or min(c[0],d[0])-reach>hi[0] or max(c[1],d[1])+reach<lo[1] or min(c[1],d[1])-reach>hi[1]:continue
            comparisons+=1;dist=segdist(a,b,c,d)
            if dist<minimum:minimum=dist;pair=[name,other,i,k]
            if dist*RES<MIN_GUTTER-1e-3:violations.append({'a':i,'b':k,'gap_pixels':dist*RES,'objects':[name,other]})
        for x in range(math.floor(lo[0]/cell),math.floor(hi[0]/cell)+1):
            for y in range(math.floor(lo[1]/cell),math.floor(hi[1]/cell)+1):grid.setdefault((x,y),[]).append(i)
    border=min(min(*a,*b,1-a[0],1-a[1],1-b[0],1-b[1]) for a,b,_,_ in boundaries)*RES
    return {'minimum_interisland_gap_pixels':minimum*RES if pair else None,'closest_pair':pair,'minimum_border_pixels':border,
        'violations_below_18_pixels':len(violations),'examples':violations[:30],'segment_pairs_tested':comparisons,'required_pair_gap_pixels':MIN_GUTTER,
        'filtering_policy':'8 px dilation per island + >=2 px filtering separation; atlas border >=10 px'}

def hard_edge_audit(mesh, uv):
    normals = getattr(mesh, 'corner_normals', None)
    if normals is None or len(normals) != len(mesh.loops):
        return {'status': 'unavailable', 'reason': 'Actual per-corner normals unavailable'}
    by_edge = {}
    for polygon in mesh.polygons:
        loops = list(polygon.loop_indices)
        for i, li in enumerate(loops):
            lj = loops[(i + 1) % len(loops)]
            endpoint_loops = {mesh.loops[li].vertex_index: li,
                              mesh.loops[lj].vertex_index: lj}
            by_edge.setdefault(mesh.loops[li].edge_index, []).append(
                (polygon.index, endpoint_loops))
    counts = {'manifold_edges': 0, 'boundary_edges': 0, 'nonmanifold_edges': 0,
              'hard_edges': 0, 'hard_edges_with_uv_seam': 0,
              'hard_edges_without_uv_seam': 0, 'actual_uv_seams': 0,
              'flagged_seams_without_uv_split': 0}
    missing = []
    for edge_index, adjacent in by_edge.items():
        if len(adjacent) == 1:
            counts['boundary_edges'] += 1
            continue
        if len(adjacent) != 2:
            counts['nonmanifold_edges'] += 1
            continue
        counts['manifold_edges'] += 1
        (pa, la), (pb, lb) = adjacent
        angles, deltas = [], []
        for vertex in mesh.edges[edge_index].vertices:
            a, b = la[vertex], lb[vertex]
            na, nb = normals[a].vector, normals[b].vector
            dot = float(na.dot(nb) / max(na.length * nb.length, 1e-20))
            angles.append(math.degrees(math.acos(max(-1.0, min(1.0, dot)))))
            deltas.append((uv.data[a].uv - uv.data[b].uv).length)
        hard = max(angles) > HARD_NORMAL_ANGLE_DEGREES
        split = max(deltas) > UV_TOLERANCE
        counts['actual_uv_seams'] += int(split)
        counts['flagged_seams_without_uv_split'] += int(mesh.edges[edge_index].use_seam and not split)
        if hard:
            counts['hard_edges'] += 1
            counts['hard_edges_with_uv_seam' if split else 'hard_edges_without_uv_seam'] += 1
            if not split:
                missing.append({'edge': edge_index, 'polygons': [pa, pb],
                                'normal_angle_degrees': max(angles)})
    return {'status': 'checked', 'method': 'Actual corner normals and endpoint UV differences',
            'normal_angle_threshold_degrees': HARD_NORMAL_ANGLE_DEGREES,
            **counts, 'missing_seam_edges': missing}

def rasterize_interior(uvs, triangle_id, counts, owner, collisions):
    """Rasterize strict triangle interiors at pixel centers; no edge expansion."""
    p = np.asarray(uvs, dtype=np.float64)
    resolution = counts.shape[0]
    low = np.maximum(np.ceil(p.min(axis=0) * resolution - 0.5).astype(int), 0)
    high = np.minimum(np.floor(p.max(axis=0) * resolution - 0.5).astype(int), resolution - 1)
    if np.any(high < low):
        return 0
    a, b, c = p
    denominator = ((b[1] - c[1]) * (a[0] - c[0])
                   + (c[0] - b[0]) * (a[1] - c[1]))
    if abs(denominator) <= 2 * UV_AREA_EPSILON:
        return 0
    xs = (np.arange(low[0], high[0] + 1, dtype=np.float64) + 0.5) / resolution
    ys = (np.arange(low[1], high[1] + 1, dtype=np.float64) + 0.5) / resolution
    x, y = xs[None, :], ys[:, None]
    wa = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / denominator
    wb = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / denominator
    wc = 1.0 - wa - wb
    inside = (wa > INTERIOR_BARYCENTRIC_EPSILON) & (wb > INTERIOR_BARYCENTRIC_EPSILON) & (wc > INTERIOR_BARYCENTRIC_EPSILON)
    count_view = counts[low[1]:high[1] + 1, low[0]:high[0] + 1]
    owner_view = owner[low[1]:high[1] + 1, low[0]:high[0] + 1]
    overlap = inside & (count_view > 0)
    if np.any(overlap):
        previous, hits = np.unique(owner_view[overlap], return_counts=True)
        for previous_id, hit_count in zip(previous, hits):
            pair = (int(previous_id), triangle_id)
            collisions[pair] = collisions.get(pair, 0) + int(hit_count)
    owner_view[inside & (count_view == 0)] = triangle_id
    count_view[inside] += 1
    return int(inside.sum())

def write_png(path, rgba):
    """Small deterministic PNG encoder; no temporary Blender image/user state."""
    height, width, channels = rgba.shape
    if rgba.dtype != np.uint8 or channels != 4:
        raise ValueError('Expected uint8 RGBA pixels')
    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff))
    raw = b''.join(b'\x00' + row.tobytes() for row in rgba[::-1])
    path.write_bytes(b'\x89PNG\r\n\x1a\n'
                     + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))

def atlas_audit(objects,resolutions=(4096,2048,1024),write_layout=None):
    rows=[];hard=[];invalid=[];area=0.;density=[]
    for o in objects:
        m=o.data;uv=m.uv_layers['BakeUV'];hard.append({'object':o.name,**hard_edge_audit(m,uv)})
        for p in m.polygons:
            assert len(p.vertices)==3
            coords=[tuple(uv.data[li].uv) for li in p.loop_indices]
            au=abs(cross(*coords))*.5;area+=au
            if au<1e-14 or not all(math.isfinite(q) and -1e-7<=q<=1+1e-7 for pt in coords for q in pt):invalid.append([o.name,p.index,au])
            ps=[o.matrix_world@m.vertices[vi].co for vi in p.vertices];aw=(ps[1]-ps[0]).cross(ps[2]-ps[0]).length*.5
            if aw>1e-15:density.append(RES*math.sqrt(au/aw))
            rows.append({'object':o.name,'polygon':p.index,'uv':coords})
    overlap=exact_overlap(rows);boundaries,islands,island_rows=island_boundaries(objects);spacing=spacing_audit(boundaries)
    samples={}
    for res in resolutions:
        counts=np.zeros((res,res),np.uint16);owner=np.full((res,res),-1,np.int32);collisions={};unsampled=[]
        for ti,row in enumerate(rows):
            n=rasterize_interior(row['uv'],ti,counts,owner,collisions)
            if not n:unsampled.append({'object':row['object'],'polygon':row['polygon'],'area_pixels':abs(cross(*row['uv']))*.5*res*res})
        samples[str(res)]={'coverage_fraction':float((counts>0).sum())/(res*res),'sampled_overlap_pixels':int((counts>1).sum()),'triangles_without_texel_center':len(unsampled),'unsampled_triangles':unsampled,'minimum_interisland_gap_pixels':None if spacing['minimum_interisland_gap_pixels'] is None else spacing['minimum_interisland_gap_pixels']*res/RES,'minimum_border_pixels':spacing['minimum_border_pixels']*res/RES,'area_equivalent_texels_per_world_unit_min_median_max':[float(min(density))*res/RES,float(np.median(density))*res/RES,float(max(density))*res/RES]}
        if write_layout and res==RES:
            rgba=np.empty((res,res,4),np.uint8);rgba[:]=(17,23,31,255);rgba[counts>0]=(81,156,184,255);rgba[counts>1]=(255,32,35,255)
            for a,b,_,_ in boundaries:
                a=np.array(a)*(res-1);b=np.array(b)*(res-1);n=max(2,int(np.linalg.norm(b-a))+1)
                line=np.rint(np.linspace(a,b,n)).astype(int);line=np.clip(line,0,res-1);rgba[line[:,1],line[:,0]]=(218,232,241,255)
            write_png(write_layout,rgba)
    return {'objects':len(objects),'triangles':len(rows),'islands':islands,'uv_area_fraction':area,'invalid_uv_triangles':invalid,'hard_edges_without_uv_seam':sum(r['hard_edges_without_uv_seam'] for r in hard),'exact_overlap':overlap,'spacing':spacing,'resolutions':samples,'islands_by_object':island_rows,'hard_normal_audit':hard},rows
