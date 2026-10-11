"""Import a Refinery source unchanged, save a hash-bound Blender review and clay view."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector


def aim(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    source = args.source.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)

    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    if source.suffix.lower() == '.fbx':
        bpy.ops.import_scene.fbx(filepath=str(source))
    elif source.suffix.lower() == '.obj':
        bpy.ops.wm.obj_import(filepath=str(source))
    else:
        raise ValueError(source)
    bpy.context.view_layer.update()
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not meshes:
        raise RuntimeError(f'No imported meshes: {source}')
    for obj in meshes:
        obj.data.calc_loop_triangles()

    def bounds(objs):
        points = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
        return {'min': [min(v[i] for v in points) for i in range(3)],
                'max': [max(v[i] for v in points) for i in range(3)]}

    report = {
        'source': str(source),
        'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'blender': bpy.app.version_string,
        'bounds_all': bounds(meshes),
        'objects': [{
            'name': o.name,
            'vertices': len(o.data.vertices),
            'faces': len(o.data.polygons),
            'evaluated_triangles': len(o.data.loop_triangles),
            'bounds': bounds([o]),
            'materials': [m.name if m else None for m in o.data.materials],
        } for o in meshes],
        'images': [{'name': im.name, 'filepath': im.filepath} for im in bpy.data.images],
    }
    (out / 'SOURCE_INSPECTION.json').write_text(json.dumps(report, indent=2) + '\n')

    b = report['bounds_all']
    center = Vector((b['min'][i] + b['max'][i]) / 2 for i in range(3))
    size = max(b['max'][i] - b['min'][i] for i in range(3))
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 16
    scene.render.resolution_x = 1100
    scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'Standard'
    world = bpy.data.worlds.new('Inspection world')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (.55, .55, .55, 1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = .8
    scene.world = world
    bpy.ops.object.camera_add(location=center + Vector((size * 1.15, -size * 1.4, size * 1.0)))
    scene.camera = bpy.context.object
    scene.camera.data.type = 'ORTHO'
    scene.camera.data.ortho_scale = size * 1.7
    scene.camera.data.clip_end = max(1000, size * 10)
    aim(scene.camera, center)
    bpy.ops.object.light_add(type='AREA', location=center + Vector((-size, -size, size * 1.5)))
    light = bpy.context.object
    light.data.energy = 2200 * (size / 30) ** 2
    light.data.size = size
    aim(light, center)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(out / 'SOURCE_OPENED.blend'))
    scene.render.filepath = str(out / 'SOURCE_OPENED.png')
    bpy.ops.render.render(write_still=True)
    print('REFINERY_SOURCE_REVIEW', json.dumps({'source': str(source), 'bounds': report['bounds_all'], 'meshes': len(meshes)}))


if __name__ == '__main__':
    main()
