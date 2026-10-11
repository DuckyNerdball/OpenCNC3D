"""Dump exact rest-pose PROC physical triangles and top-view evidence from FBX."""

import argparse
import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--fbx', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args(argv)
    a.out.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(a.fbx.resolve()))
    bpy.context.view_layer.update()
    triangles = []
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or 'SHADOW' in obj.name:
            if obj.type == 'MESH':
                obj.hide_render = True
            continue
        obj.data.calc_loop_triangles()
        for tri in obj.data.loop_triangles:
            pts = [list(obj.matrix_world @ obj.data.vertices[i].co) for i in tri.vertices]
            triangles.append({'object': obj.name, 'material': obj.data.materials[tri.material_index].name
                              if tri.material_index < len(obj.data.materials) else None,
                              'vertices': pts})
    (a.out / 'physical_triangles.json').write_text(json.dumps(triangles, indent=2) + '\n')

    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.render.resolution_x = 1200
    scene.render.resolution_y = 1200
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'Standard'
    world = bpy.data.worlds.new('Inspection world')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (.55, .55, .55, 1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = .9
    scene.world = world
    bpy.ops.object.camera_add(location=(1.4, -.4, 80))
    camera = bpy.context.object
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = 38
    camera.rotation_euler = (0, 0, 0)
    camera.rotation_euler = (Vector((1.4, -.4, 0)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    bpy.ops.object.light_add(type='AREA', location=(-20, -20, 35))
    light = bpy.context.object
    light.data.energy = 3500
    light.data.size = 25
    bpy.context.preferences.filepaths.save_version = 0
    scene.render.filepath = str((a.out / 'original_top.png').resolve())
    bpy.ops.render.render(write_still=True)
    print('PROC_TRIANGLES', len(triangles))


if __name__ == '__main__':
    main()
