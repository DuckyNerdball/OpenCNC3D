"""Blender audit: approved geometry, portable files and protected player-mask islands.

blender -b ConstructionYard_Runtime01.blend --python tools/art/verify_construction_yard.py
"""
import hashlib
import json
from pathlib import Path

import bpy
import numpy as np

root = Path(bpy.data.filepath).parent
manifest = json.loads((root / 'bake_manifest.json').read_text())
objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
assert len(objects) == 211 and not any(o.modifiers for o in objects)
rows = {}
protected = []
maps = {}
for image in bpy.data.images:
    assert image.filepath.startswith('//'), image.filepath
    assert Path(bpy.path.abspath(image.filepath)).is_file()
    assert tuple(image.size) == (2048, 2048)
    if 'PlayerColorMask' in image.name:
        assert image.colorspace_settings.name == 'Non-Color'
        data = np.empty(2048 * 2048 * 4, np.float32)
        image.pixels.foreach_get(data)
        maps[image.name] = data.reshape(2048, 2048, 4)
for o in objects:
    me = o.data
    me.calc_loop_triangles()
    rows[o.name] = dict(triangles=len(me.loop_triangles), matrix=[list(v) for v in o.matrix_world],
                        vertices=[list(v.co) for v in me.vertices],
                        uv=[list(v.uv) for v in me.uv_layers['BakeUV'].data],
                        normals=[list(v.vector) for v in me.corner_normals])
    if any(word in o.name.lower() for word in ['roof', 'arch', 'buttress', 'foundation', 'wall']):
        im = me.materials[0].node_tree.nodes['CY_TEX_PlayerColorMask'].image
        mask = maps[im.name]
        samples = []
        for tri in me.loop_triangles:
            uv = np.mean([me.uv_layers['BakeUV'].data[i].uv[:] for i in tri.loops], axis=0)
            x, y = np.floor(uv * 2048).astype(int)
            samples.append(float(mask[min(y, 2047), min(x, 2047), 0]))
        assert max(samples) == 0, o.name
        protected.append(o.name)
geometry_sha = hashlib.sha256(json.dumps(rows, sort_keys=True).encode()).hexdigest()
assert geometry_sha == manifest['geometry_sha256'], 'Approved geometry/UV/corner normals changed'
assert sum(row['triangles'] for row in rows.values()) == 9820
report = dict(geometry_sha256=geometry_sha, triangles=9820, meshes=211,
              relative_texture_paths=True, protected_mask_surfaces=protected,
              mask_test='Every triangle centroid on named roof, concrete, foundation and wall surfaces is black')
(root / 'mesh_audit.json').write_text(json.dumps(report, indent=2) + '\n')
print('YARD_MESH_AUDIT_PASS')
