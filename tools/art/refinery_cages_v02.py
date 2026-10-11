"""Explicit smooth cage rays for frozen, sharp-normal receiver triangles."""
import bpy
from mathutils import Vector
def offsets(mesh,extrusion):
    # Area-independent face-direction average keeps a tiny adjacent bevel from
    # disappearing beneath the much larger planar face's contribution.
    normals=[[] for v in mesh.vertices]
    for f in mesh.polygons:
        for i in f.vertices:
            n=f.normal.copy()
            if not any(n.dot(a)>.9999 for a in normals[i]):normals[i].append(n)
    result=[]
    for ns in normals:
        n=sum(ns,Vector()).normalized()
        # Miter the offset so every incident plane receives the required gap.
        dot=min((n.dot(a) for a in ns),default=1.)
        result.append(n*(extrusion/max(.3,dot)))
    return result
def create(receiver,extrusion,collection):
    mesh=receiver.data.copy();mesh.name=receiver.name+'_CAGE_MESH'
    for v,d in zip(mesh.vertices,offsets(receiver.data,extrusion)):v.co+=d
    mesh.update();o=bpy.data.objects.new(receiver.name+'_CAGE',mesh);collection.objects.link(o);o.matrix_world=receiver.matrix_world.copy();o.hide_render=True
    return o
def create_batch(receiver,entries,extrusion,collection):
    mesh=receiver.data.copy();mesh.name=receiver.name+'_CAGE_MESH'
    coordinates=[]
    for src,translation,_ in entries:
        coordinates.extend(src.matrix_world@(v.co+d)+Vector(translation) for v,d in zip(src.data.vertices,offsets(src.data,extrusion)))
    assert len(coordinates)==len(mesh.vertices)
    for v,p in zip(mesh.vertices,coordinates):v.co=p
    mesh.update();o=bpy.data.objects.new(receiver.name+'_CAGE',mesh);collection.objects.link(o);o.hide_render=True
    return o
