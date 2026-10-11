"""Fresh-reopen geometry check for the saved PROC first-low Blender file."""

import argparse
import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--report', type=Path, required=True)
    a = p.parse_args(argv)
    original = json.loads(a.report.read_text())
    objects = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o.get('runtime_candidate') is True]
    depsgraph = bpy.context.evaluated_depsgraph_get()
    counts = {}
    pts = []
    degenerate = 0
    for obj in objects:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        mesh.calc_loop_triangles()
        counts[obj.name] = len(mesh.loop_triangles)
        for v in mesh.vertices:
            pts.append(obj.matrix_world @ v.co)
        degenerate += sum(t.area < 1e-10 for t in mesh.loop_triangles)
        evaluated.to_mesh_clear()
    bounds = {'min': [min(v[i] for v in pts) for i in range(3)],
              'max': [max(v[i] for v in pts) for i in range(3)]}
    if counts != original['runtime_objects']:
        raise RuntimeError(f'Fresh-reopen object/count drift: {counts}')
    if any(abs(bounds[k][i] - original['runtime_bounds'][k][i]) > 1e-5
           for k in ('min', 'max') for i in range(3)):
        raise RuntimeError(f'Fresh-reopen bounds drift: {bounds}')
    foundation = bpy.data.objects['PROC_SourceFitFoundation']
    actual = [list(v.co) for v in foundation.data.vertices]
    expected = [[x, y, -.49] for x, y in original['foundation_bottom_xy']]
    expected += [[x, y, .09] for x, y in original['foundation_top_xy']]
    if any(abs(actual[j][i] - expected[j][i]) > 1e-5 for j in range(12) for i in range(3)):
        raise RuntimeError('Source foundation contour changed after save')
    archive = bpy.data.objects['REF_ArchivedProxyGDI_Unchanged_NONEXPORT']
    if not archive.hide_render or not archive.hide_get():
        raise RuntimeError('Unchanged proxy copy is not hidden from export/review')
    report = {'saved_blend': bpy.data.filepath, 'reopened_blender': bpy.app.version_string,
              'runtime_objects': counts, 'evaluated_runtime_triangles': sum(counts.values()),
              'runtime_bounds': bounds, 'degenerate_runtime_triangles': degenerate,
              'foundation_contour_matches_source': True,
              'unchanged_proxy_hidden': True}
    out = a.report.parent / 'FRESH_REOPEN_VERIFY.json'
    out.write_text(json.dumps(report, indent=2) + '\n')
    print('PROC_FRESH_REOPEN', json.dumps(report))


if __name__ == '__main__':
    main()
