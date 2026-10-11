"""Build the reusable Blender swatch scene from art/style/enhanced_v1.json.

blender -b --factory-startup --python tools/art/create_style_kit.py -- \
  --out art/style/OpenCNC3D_Enhanced_Style_v1.blend \
  --preview art/style/OpenCNC3D_Enhanced_Style_v1.png
"""

import argparse
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]


def linear(srgb):
    c = srgb / 255.0
    return c / 12.92 if c <= .04045 else ((c + .055) / 1.055) ** 2.4


def material(row):
    mat = bpy.data.materials.new(row['name'])
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    shader = nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*[linear(x) for x in row['srgb']], 1)
    shader.inputs['Roughness'].default_value = row['roughness']
    shader.inputs['Metallic'].default_value = row['metallic']
    mat['OpenCNC3D_class'] = row['class']
    mat['OpenCNC3D_srgb_anchor'] = json.dumps(row['srgb'])
    mat['OpenCNC3D_roughness_anchor'] = row['roughness']
    mat['OpenCNC3D_metallic_anchor'] = row['metallic']
    mat['OpenCNC3D_authoring_note'] = 'Starting values only; bake separate BaseColor, Normal, AO, Roughness, Metallic, PlayerColorMask maps.'
    return mat


def point(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def make():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, default=ROOT / 'art/style/OpenCNC3D_Enhanced_Style_v1.blend')
    parser.add_argument('--preview', type=Path, default=ROOT / 'art/style/OpenCNC3D_Enhanced_Style_v1.png')
    args = parser.parse_args(argv)
    config = json.loads((ROOT / 'art/style/enhanced_v1.json').read_text())
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    scene = bpy.context.scene
    scene['OpenCNC3D_style_version'] = config['version']
    scene['OpenCNC3D_engine_authority'] = 'USER92 beside HTNK and FACT at 1600x1000, Enhanced + Experimental Art'
    scene['OpenCNC3D_note'] = 'This neutral swatch scene is an authoring aid. The game renderer is the visual approval authority.'
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 32
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'Medium High Contrast'
    scene.render.image_settings.file_format = 'PNG'
    world = bpy.data.worlds.new('Neutral reference environment')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (.5, .5, .5, 1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = .65
    scene.world = world

    swatches = bpy.data.collections.new('OpenCNC3D material and chamfer presets')
    scene.collection.children.link(swatches)
    rows = config['material_presets']
    for i, row in enumerate(rows):
        x = (i - (len(rows) - 1) / 2) * 2.4
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 0, .6))
        cube = bpy.context.object
        cube.name = f"{i + 1:02d} {row['name']} | bevel {row['bevel_cell']:.3f} cell"
        cube.dimensions = (1.5, 1.5, 1.2)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        cube.rotation_euler.z = math.radians(18)
        cube.data.materials.append(material(row))
        bevel = cube.modifiers.new('Screen-scale chamfer starting point', 'BEVEL')
        bevel.width = row['bevel_cell']
        bevel.segments = 2
        bevel.affect = 'EDGES'
        normal = cube.modifiers.new('Weighted corner normals', 'WEIGHTED_NORMAL')
        normal.keep_sharp = True
        cube['OpenCNC3D_bevel_cell_start'] = row['bevel_cell']
        cube['OpenCNC3D_chamfer_rule'] = 'At USER92 max zoom, prominent edges should read as 1–3 lit pixels; validate in game.'
        for coll in list(cube.users_collection):
            coll.objects.unlink(cube)
        swatches.objects.link(cube)

        bpy.ops.object.text_add(location=(x, -2.25, .06))
        label = bpy.context.object
        label.name = f'Label {row["name"]}'
        label.data.body = row['name']
        label.data.align_x = 'CENTER'
        label.data.size = .18
        label.rotation_euler = (math.pi / 2, 0, 0)
        for coll in list(label.users_collection):
            coll.objects.unlink(label)
        swatches.objects.link(label)

    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.03))
    ground = bpy.context.object
    ground.name = 'Neutral ground'
    ground_mat = bpy.data.materials.new('Neutral matte ground')
    ground_mat.diffuse_color = (.18, .18, .18, 1)
    ground_mat.use_nodes = True
    bsdf = ground_mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (.18, .18, .18, 1)
    bsdf.inputs['Roughness'].default_value = .9
    ground.data.materials.append(ground_mat)

    bpy.ops.object.camera_add(location=(0, -11, 8))
    camera = bpy.context.object
    point(camera, (0, 0, .45))
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = 14.5
    scene.camera = camera
    bpy.ops.object.light_add(type='AREA', location=(-4, -5, 8))
    key = bpy.context.object
    key.name = 'Shared neutral key'
    key.data.energy = 1100
    key.data.shape = 'DISK'
    key.data.size = 7
    point(key, (0, 0, 0))

    args.out = args.out.resolve()
    args.preview = args.preview.resolve()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.preview.parent.mkdir(parents=True, exist_ok=True)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(args.out))
    scene.render.filepath = str(args.preview)
    bpy.ops.render.render(write_still=True)
    print(f'OPENCNC3D_STYLE_KIT={args.out}')
    print(f'OPENCNC3D_STYLE_PREVIEW={args.preview}')


if __name__ == '__main__':
    make()
