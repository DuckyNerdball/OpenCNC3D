#!/usr/bin/env python3
"""Sum a macOS `sample` call tree by function, and break chosen functions into their callees.

    sample <pid> 5 -file /tmp/s.txt           # while the game (or cnc_eyes) is running
    python3 tools/perf/sample_breakdown.py /tmp/s.txt                 # totals
    python3 tools/perf/sample_breakdown.py /tmp/s.txt draw_frame draw_mesh(

With no function names it prints the renderer's usual suspects summed over the whole tree
(the same function reached from different callers is added up, since those samples are
disjoint). With names it prints, for each, the direct callees and their sample counts.
A sample is roughly a millisecond of the thread's time, so the samples a function takes
over an N second sample, divided by the frames drawn in those N seconds, is its cost per
frame in milliseconds.
"""
import collections, re, sys

PAT = re.compile(r'^([\s+!:|]*)(\d+) (.+?)  \(in ')
SUSPECTS = ['game_loop', 'draw_frame(', 'Cocoa_GL_SwapWindow', 'fx_draw_shadow_casters',
            'water_fx_draw', 'fx_world_end', 'fx_draw_terrain_normals', 'draw_terrain(',
            'draw_cliff_relief', 'draw_object_mesh(', 'draw_mesh(', 'tree3d_draw(',
            'draw_tiberium_solid', 'foliage3d_draw', 'm3inf_draw_as', 'm3_draw_object_normals',
            'grass_draw', 'draw_sprite(', 'draw_walls', 'refresh_objects', 'brain_advance']


def load(path):
    out = []
    for line in open(path, errors='ignore').read().split('\n'):
        m = PAT.match(line)
        if m:
            out.append((len(m.group(1)), int(m.group(2)), m.group(3)))
    return out


def totals(rows):
    agg = collections.Counter()
    for depth, n, name in rows:
        for s in SUSPECTS:
            if name.startswith(s):
                agg[s] += n
    for k, v in agg.most_common():
        print('%7d  %s' % (v, k))


def callees(rows, target, top=12):
    agg = collections.Counter()
    for i, (depth, n, name) in enumerate(rows):
        if not name.startswith(target):
            continue
        for d2, n2, name2 in rows[i + 1:]:
            if d2 <= depth:
                break
            if d2 == depth + 2:
                agg[name2[:70]] += n2
    print('== %s (%d)' % (target, sum(agg.values())))
    for k, v in agg.most_common(top):
        print('   %6d  %s' % (v, k))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    rows = load(sys.argv[1])
    if len(sys.argv) == 2:
        totals(rows)
    for t in sys.argv[2:]:
        callees(rows, t)
