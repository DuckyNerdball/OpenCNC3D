"""Editable PROC material selections for selected-to-active emission bakes.

Only creates material datablocks. Geometry, scenes, UVs and source files are never
modified. Coordinates are the original rest-world authoring units (~20 across
the complete Refinery); pass a batching translation to remove that offset.
Generated pigment supplies colour variation only, never material classification,
roughness or relief. Structural relief comes from the separate physical bake.
"""
import json
import math

import bpy
from mathutils import Vector


class Graph:
    def __init__(self, name):
        self.mat = bpy.data.materials.new(name)
        self.mat.use_nodes = True
        self.nodes = self.mat.node_tree.nodes
        self.nodes.clear()
        self.links = self.mat.node_tree.links
        self.out = self.node('ShaderNodeOutputMaterial')
        self.emit = self.node('ShaderNodeEmission')
        self.links.new(self.emit.outputs[0], self.out.inputs['Surface'])
        self.geometry = self.node('ShaderNodeNewGeometry')
        self.position = self.geometry.outputs['Position']
        self.destination = self.node('ShaderNodeTexImage')
        self.destination.name = 'BAKE_DESTINATION'
        self.nodes.active = self.destination

    def node(self, kind):
        return self.nodes.new(kind)

    def set(self, socket, value):
        if hasattr(value, 'is_output'):
            self.links.new(value, socket)
        else:
            if socket.type == 'RGBA' and isinstance(value, (int, float)):
                value = (value, value, value, 1)
            if socket.type == 'VECTOR' and isinstance(value, (int, float)):
                value = (value, value, value)
            socket.default_value = value

    def math(self, op, a, b=None):
        n = self.node('ShaderNodeMath')
        n.operation = op
        self.set(n.inputs[0], a)
        if b is not None:
            self.set(n.inputs[1], b)
        return n.outputs[0]

    def vec(self, op, a, b=None):
        n = self.node('ShaderNodeVectorMath')
        n.operation = op
        self.set(n.inputs[0], a)
        if b is not None:
            self.set(n.inputs[1], b)
        return n.outputs['Value' if op in ('LENGTH', 'DISTANCE', 'DOT_PRODUCT') else 'Vector']

    def color(self, a, b, t):
        n = self.node('ShaderNodeMixRGB')
        n.blend_type = 'MIX'
        self.set(n.inputs[0], t)
        self.set(n.inputs[1], a)
        self.set(n.inputs[2], b)
        return n.outputs[0]

    def noise(self, scale, vector=None, detail=2):
        n = self.node('ShaderNodeTexNoise')
        n.inputs['Scale'].default_value = scale
        n.inputs['Detail'].default_value = detail
        n.inputs['Roughness'].default_value = .6
        self.set(n.inputs['Vector'], self.position if vector is None else vector)
        return n.outputs['Fac']

    def ramp(self, value, lo, hi):
        n = self.node('ShaderNodeMapRange')
        n.clamp = True
        self.set(n.inputs['Value'], value)
        n.inputs['From Min'].default_value = lo
        n.inputs['From Max'].default_value = hi
        return n.outputs['Result']

    def output(self, channel, image):
        """Select one clean channel and make image the active bake destination."""
        for link in list(self.emit.inputs['Color'].links):
            self.links.remove(link)
        self.set(self.emit.inputs['Color'], self.channels[channel])
        self.destination.image = image
        for n in self.nodes:
            n.select = False
        self.destination.select = True
        self.nodes.active = self.destination


def srgb(value):
    """Hex authored colour to linear scene-referred RGB for clean BaseColor."""
    rgb = [int(value[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in rgb) + (1,)


def role_for_source(name,original_material):
    m=str(original_material).lower()
    if name.startswith('Chimney_tapered_panel_course_') or name=='Chimney_crown_cap':return 'gray_paint'
    if 'fixed_hazard_yellow' in m:return 'hazard'
    if 'process_surface' in m:return 'process'
    if any(k in name for k in ('bolt','fastener','grille_slat','core_radial_grille','core_concentric_grille')):return 'metal'
    if 'triangle_sheet' in name:return 'gray_dark' if 'dark_steel' in m else 'gray_paint'
    if 'GDI' in str(original_material):return 'paint'
    if 'concrete' in m:return 'concrete'
    if 'machined_steel' in m:return 'metal'
    if 'dark_steel' in m or 'vessel_steel' in m:return 'dark_metal'
    if 'recess' in m:return 'dark'
    raise ValueError((name,original_material))


def edge_segments(original, limit=32, min_length=.10):
    """Select actual convex construction edges, excluding round tessellation.

    Input is the unmodified original source mesh before evaluated bevels. A
    bounded, distributed subset deliberately leaves protected edges intact. It
    does not manufacture high-frequency lines across broad faces.
    """
    if original is None or original.type != 'MESH':
        return []
    mesh = original.data
    matrix = original.matrix_world
    points = [matrix @ v.co for v in mesh.vertices]
    adjacent = {edge.index: [] for edge in mesh.edges}
    for face in mesh.polygons:
        for loop_index in face.loop_indices:
            adjacent[mesh.loops[loop_index].edge_index].append(face)
    candidates = []
    for edge in mesh.edges:
        faces = adjacent[edge.index]
        if len(faces) != 2 or faces[0].normal.dot(faces[1].normal) > .80:
            continue
        a, b = [points[i] for i in edge.vertices]
        length = (b - a).length
        if length < min_length:
            continue
        local_mid = (mesh.vertices[edge.vertices[0]].co + mesh.vertices[edge.vertices[1]].co) * .5
        if (faces[1].center - local_mid).dot(faces[0].normal) > 1e-5:
            continue
        candidates.append((edge.index, length, tuple(a), tuple(b)))
    # Mesh order follows each contour; sample across that order instead of
    # selecting only the long radial sides of a curved roof panel.
    if len(candidates) > limit:
        selected = {round(i * (len(candidates) - 1) / (limit - 1)) for i in range(limit)}
        candidates = [candidate for i, candidate in enumerate(candidates) if i in selected]
    return [(a, b) for _, _, a, b in candidates]


def build(name, role, pigment, original=None, translation=(0, 0, 0)):
    g = Graph(name)
    g.mat.use_fake_user = True
    g.position = g.vec('SUBTRACT', g.position, translation)
    axes = g.node('ShaderNodeSeparateXYZ')
    g.set(axes.inputs[0], g.position)
    z = axes.outputs['Z']
    normal_axes = g.node('ShaderNodeSeparateXYZ')
    g.set(normal_axes.inputs[0], g.geometry.outputs['True Normal'])

    def mul(*values):
        result = values[0]
        for value in values[1:]:
            result = g.math('MULTIPLY', result, value)
        return result

    def inv(value):
        return g.math('SUBTRACT', 1, value)

    def clamp(value):
        return g.math('MINIMUM', 1, g.math('MAXIMUM', 0, value))

    def mix(a, b, t):
        return g.math('ADD', mul(a, inv(t)), mul(b, t))

    def tag(key, value):
        node = g.node('ShaderNodeMath')
        node.operation = 'MULTIPLY'
        g.set(node.inputs[0], clamp(value))
        node.inputs[1].default_value = 1
        node.name = 'PROC_MASK_' + key
        node.label = key + ' | shared material selection'
        return node.outputs[0]

    def pack(a, b, c):
        node = g.node('ShaderNodeCombineColor')
        node.mode = 'RGB'
        for socket, value in zip(list(node.inputs)[:3], (a, b, c)):
            g.set(socket, value)
        return node.outputs[0]

    points = [original.matrix_world @ v.co for v in original.data.vertices] if original else []
    local_base = min(p.z for p in points) if points else .55
    local_top = max(p.z for p in points) if points else 3.5
    source_name = original.get('physical_original_name', original.get('source_original', original.name)) if original else name
    is_tower = 'tower_' in source_name.lower()
    coarse = g.noise(.47)
    meso = g.noise(3.6)
    wear_breakup = g.ramp(g.noise(19), .40, .64)
    # Distinct spatial random field: material roughness is not pigment brightness.
    rough_vector = g.vec('ADD', g.position, (13.71, -8.23, 4.61))
    rough_coarse = g.noise(.82, rough_vector)
    rough_fine = g.noise(24, rough_vector, 1)
    lower = inv(g.ramp(z, local_base + .08, local_base + min(1.1, max(.3, (local_top - local_base) * .28))))
    upward = g.ramp(normal_axes.outputs['Z'], .15, .88)
    paint = primer = exposed = fixed = concrete = player = 0.
    dust = grime = oil = 0.
    color = srgb('6A6C6C')
    rough = .6
    metal = 0.
    height = .5
    physical_edges = []

    if role in ('paint', 'gray_paint', 'gray_dark'):
        # Edge bands follow real source chamfers, and are independently broken
        # into exposed substrate and a wider, rougher primer halo.
        physical_edges = edge_segments(original, 32 if role == 'paint' else 20,
                                       .10 if role == 'paint' else .02)
        distance = 100.
        for aa, bb in physical_edges:
            a = Vector(aa)
            delta = Vector(bb) - a
            relative = g.vec('SUBTRACT', g.position, a)
            t = clamp(g.math('DIVIDE', g.vec('DOT_PRODUCT', relative, delta), delta.length_squared))
            closest = g.vec('ADD', a, g.vec('MULTIPLY', delta, t))
            distance = g.math('MINIMUM', distance, g.vec('DISTANCE', g.position, closest))
        bevel = max([float(m.width) * max(original.scale) for m in original.modifiers if m.type == 'BEVEL'] + [0.]) if original else 0.
        width = max(.035, min(.085, bevel * 1.5))
        wide_edge = inv(g.ramp(distance, width * .30, width * 1.65))
        core_edge = inv(g.ramp(distance, width * .18, width * .95))
        selective = g.ramp(g.noise(2.7), .39, .63)
        primer_coverage = g.math('MAXIMUM', mul(wide_edge, selective, wear_breakup), mul(g.ramp(g.noise(4.6), .63, .75), g.ramp(g.noise(26), .40, .59), .85))
        core_fraction = g.math('MAXIMUM', mul(core_edge, selective, g.ramp(g.noise(32), .46, .63)), mul(g.ramp(g.noise(4.6), .68, .79), .8))
        # Steel is a subset of the shared removed-coating coverage.
        exposed = mul(core_fraction, primer_coverage)
        primer = mul(primer_coverage, inv(core_fraction))
        paint = inv(primer_coverage)
        metal = exposed
        if role == 'paint':
            if pigment is None:
                raise ValueError('Player paint requires the immutable pigment source image')
            texture = g.node('ShaderNodeTexImage')
            texture.name = 'PIGMENT_ONLY_IMMUTABLE_SOURCE'
            texture.image = pigment
            texture.projection = 'BOX'
            texture.projection_blend = .18
            texture.extension = 'EXTEND'
            coordinate = g.vec('ADD', g.vec('MULTIPLY', g.position, (1/48, 1/48, 1/48)), (.5, .5, .05))
            g.set(texture.inputs['Vector'], coordinate)
            bw = g.node('ShaderNodeRGBToBW')
            g.set(bw.inputs[0], texture.outputs['Color'])
            neutral = clamp(g.math('ADD', .34, mul(g.math('SUBTRACT', bw.outputs[0], .4), .80)))
            color = neutral
            player = paint
        else:
            color = g.color(srgb('8A9090') if role == 'gray_paint' else srgb('444D50'),
                            srgb('A2A5A3') if role == 'gray_paint' else srgb('586163'), mul(coarse, .55))
        rough = g.math('ADD', .46, mul(rough_coarse, .18))
        rough = g.math('ADD', rough, mul(g.math('SUBTRACT', rough_fine, .5), .028))
        substrate_rough = mix(.74, g.math('ADD', .29, mul(rough_coarse, .12)), core_fraction)
        rough = mix(rough, substrate_rough, primer_coverage)
        substrate_color = g.color(srgb('655B46'), srgb('848B8C'), core_fraction)
        color = g.color(color, substrate_color, primer_coverage)
        # Small losses at actual edges; manufacturing texture is independently
        # authored and does not emboss the generated pigment.
        height = g.math('ADD', .5, mul(g.math('SUBTRACT', g.noise(28), .5), .008))
        height = g.math('SUBTRACT', height, g.math('ADD', mul(primer, .025), mul(exposed, .055)))
        grime = mul(wide_edge, g.ramp(g.noise(.75), .43, .73), .17)
        dust = mul(upward, g.ramp(g.noise(1.9), .59, .76), .08)

    elif role in ('concrete', 'pale_concrete'):
        concrete = 1.
        color = g.color(srgb('93928C') if role == 'concrete' else srgb('BFB8AA'),
                        srgb('C2C0B7') if role == 'concrete' else srgb('D0CABE'), mul(coarse, .6))
        # Sparse aggregate and pores have their own physical field. No uniform
        # crackle, invented joints, or albedo-derived bump is introduced.
        aggregate = g.noise(11.5, g.vec('ADD', g.position, (5.31, 3.77, 8.21)), 1)
        pores = g.ramp(g.noise(17.5), .68, .82)
        height = g.math('ADD', .5, mul(g.math('SUBTRACT', aggregate, .5), .13))
        height = g.math('SUBTRACT', height, mul(pores, .12))
        color = g.color(color, srgb('8D8779'), mul(pores, .12))
        rough = g.math('ADD', .74, mul(rough_coarse, .14))
        rough = g.math('ADD', rough, mul(g.math('SUBTRACT', rough_fine, .5), .025))
        runoff_vector = g.vec('MULTIPLY', g.position, (3.0, 3.0, .13))
        runoff = mul(g.ramp(g.noise(1.7, runoff_vector), .57, .72), g.ramp(coarse, .40, .72))
        # On the tall shell, elongated stains continue down a few selected
        # paths; lower buildup follows each real component's measured foot.
        color = g.color(color, srgb('777268'), mul(g.ramp(g.noise(.9), .52, .76), .65))
        grime = g.math('MAXIMUM', mul(lower, g.ramp(meso, .38, .75), .40),
                       mul(runoff, .23 if is_tower else .12))
        dust = mul(upward, g.ramp(coarse, .45, .78), .11)

    elif role in ('metal', 'dark_metal'):
        fixed = metal = 1.
        color = srgb('777E80') if role == 'metal' else srgb('454D50')
        color = g.color(color, srgb('525957') if role == 'metal' else srgb('343D3F'), mul(meso, .24))
        rough = g.math('ADD', .30 if role == 'metal' else .40, mul(rough_coarse, .16))
        rough = g.math('ADD', rough, mul(g.math('SUBTRACT', rough_fine, .5), .025))
        dust = mul(lower, upward, g.ramp(coarse, .51, .78), .16)
        grime = mul(lower, g.ramp(meso, .54, .75), .22)
        if 'sidepipe' in source_name.lower() or 'manifold' in source_name.lower():
            oil = mul(lower, g.ramp(g.noise(2.1, rough_vector), .60, .77), .20)
        height = g.math('ADD', .5, mul(g.math('SUBTRACT', g.noise(26), .5), .006))

    elif role == 'dark':
        color = srgb('22292B')
        rough = g.math('ADD', .69, mul(rough_coarse, .14))
        grime = mul(lower, g.ramp(meso, .51, .77), .15)

    elif role == 'hazard':
        paint = 1.
        color = g.color(srgb('C2A33F'), srgb('998341'), mul(coarse, .22))
        rough = g.math('ADD', .55, mul(rough_coarse, .12))
        grime = mul(lower, g.ramp(meso, .53, .76), .10)

    elif role == 'process':
        color = g.color(srgb('738C28'), srgb('CBCE59'), g.ramp(coarse, .23, .74))
        rough = g.math('ADD', .25, mul(rough_coarse, .10))
        height = g.math('ADD', .5, mul(g.math('SUBTRACT', g.noise(2.5), .5), .016))

    elif role == 'amber':
        color = srgb('BA5B15')
        rough = .31

    elif role == 'coolant':
        color = srgb('263C46')
        rough = .35

    else:
        raise ValueError('Unknown Refinery role: ' + str(role))

    if role == 'concrete' and original and 'Foundation_inset_top_course' in source_name:
        layout=bpy.data.collections.get('QA_PaintLayout_NONEXPORT')
        guides=sorted([o for o in layout.objects if o.type=='MESH'],key=lambda o:(0 if 'dark_field' in o.name else 1,o.name))
        for guide in guides:
            coverage=0.
            for f in guide.data.polygons:
                pts=[guide.matrix_world @ guide.data.vertices[i].co for i in f.vertices]
                area=sum(a.x*b.y-b.x*a.y for a,b in zip(pts,pts[1:]+pts[:1]));sign=1 if area>0 else -1
                inside=1.
                for a,b in zip(pts,pts[1:]+pts[:1]):
                    n=Vector((-(b.y-a.y)*sign,(b.x-a.x)*sign,0))
                    dot=g.vec('DOT_PRODUCT',g.vec('SUBTRACT',g.position,a),n)
                    inside=mul(inside,g.math('GREATER_THAN',dot,-.000001))
                coverage=g.math('MAXIMUM',coverage,inside)
            coverage=mul(coverage,g.ramp(normal_axes.outputs['Z'],.8,.95),g.ramp(z,.12,.20))
            loss=g.ramp(g.noise(18),.64,.78)
            coverage=mul(coverage,inv(loss))
            yellow='stripe_ID_' in guide.name and 'dark_field' not in guide.name
            color=g.color(color,srgb('C2A33F') if yellow else srgb('353D3E'),coverage)
            rough=mix(rough,.67,coverage)
            concrete=mul(concrete,inv(coverage))
            paint=g.math('MAXIMUM',paint,coverage)

    # Deposits occlude the same underlying material identity in every channel.
    # Semantic material classes represent final coverage after those deposits.
    dust = clamp(dust)
    grime = clamp(grime)
    oil = clamp(oil)
    deposit_survival = mul(inv(dust), inv(grime), inv(oil))
    masks = {
        'Paint': tag('Paint', mul(paint, deposit_survival)),
        'Primer': tag('Primer', mul(primer, deposit_survival)),
        'ExposedSteel': tag('ExposedSteel', mul(exposed, deposit_survival)),
        'FixedMetal': tag('FixedMetal', mul(fixed, deposit_survival)),
        'Concrete': tag('Concrete', mul(concrete, deposit_survival)),
        'Dust': tag('Dust', dust),
        'DryGrime': tag('DryGrime', mul(grime, inv(dust))),
        'Oil': tag('Oil', mul(oil, inv(grime), inv(dust))),
        'PlayerColor': tag('PlayerColor', mul(player, deposit_survival)),
    }
    for selection, tint, scattering in ((oil, '302D24', .30), (grime, '514C40', .81), (dust, '9B8F78', .88)):
        color = g.color(color, srgb(tint), selection)
        rough = mix(rough, scattering, selection)
    metal = mul(metal, deposit_survival)
    g.semantic_masks = masks
    g.channels = {
        'BaseColor': color,
        'Roughness': clamp(rough),
        'Metallic': clamp(metal),
        'PlayerColorMask': masks['PlayerColor'],
        'SurfaceHeight': clamp(height),
        'SemanticA': pack(masks['Paint'], masks['Primer'], masks['ExposedSteel']),
        'SemanticB': pack(masks['Dust'], masks['DryGrime'], masks['Oil']),
        'SemanticC': pack(masks['FixedMetal'], masks['Concrete'], masks['PlayerColor']),
    }
    g.mat['role'] = role
    g.mat['source_original'] = source_name
    g.mat['clean_BaseColor_no_AO'] = True
    g.mat['roughness_independent_of_pigment'] = True
    g.mat['player_mask_is_authored'] = True
    g.mat['semantic_layout'] = 'A=paint/primer/exposedSteel; B=dust/grime/oil; C=fixedmetal/concrete/player'
    g.mat['pigment_projection'] = 'BOX: original rest-world XYZ/48 + (.5,.5,.05), blend .18, EXTEND; no tiling'
    g.mat['physical_edge_segments'] = json.dumps(physical_edges)
    g.mat['microheight_nominal_bump_distance'] = .03
    g.mat['microheight_note'] = 'Normalized .5: independently authored concrete pores/aggregate and coating loss; compose after physical normal. No pigment embossing.'
    g.mat['scale_units'] = 'original Blender authoring units; full Refinery about 34 across'
    return g
