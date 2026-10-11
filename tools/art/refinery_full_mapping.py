"""Record explicit full-asset high-to-low ownership, preserving trial assignments."""
import bpy,json,sys,collections
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
R=Path(__file__).resolve().parents[2];O=R/'art/refinery/texture_v02'
bpy.ops.wm.open_mainfile(filepath=str(O/'PROC_TextureUV_v02.blend'))
low=sorted(bpy.data.collections['PROC_FinalLow_v02_RUNTIME'].objects,key=lambda o:o.name)
high=bpy.data.collections['PROC_High_v04_NONEXPORT_BAKE_SOURCE'];bpy.context.view_layer.layer_collection.children[high.name].exclude=False;bpy.context.view_layer.update()
mapping={o.name:[o['high_source']] for o in low}
old=json.loads((R/'art/refinery/texture_v01/SOURCE_RECEIVER_MAP.json').read_text())['receiver_to_sources']
mapping.update(old)
for group in ['Rear_processing_core','Right_middle_vessel','Right_front_vessel']:
    mapping['LOW_'+group+'_faceted_cladding']=[o.name for o in high.objects if o.name.startswith(group) and any(k in o.name for k in ('triangle_sheet','panel_fastener','vertical_seam_rib'))]
    # Continuous wall supplies the recessed seams beneath individual sheet prisms.
    mapping['LOW_'+group+'_faceted_cladding'].append(group+'_continuous_sealed_wall')
trees={o.name:BVHTree.FromPolygons([o.matrix_world@v.co for v in o.data.vertices],[tuple(f.vertices) for f in o.data.polygons]) for o in low}
assigned={n for row in mapping.values() for n in row};automatic=[];excluded=[]
for src in high.objects:
    n=src.name
    if n in assigned:continue
    if 'hazard_stripe_ID' in n or 'direction_arrow_ID' in n:
        excluded.append({'source':n,'reason':'Authoring paint guide projected analytically in foundation material graph'});continue
    target=None
    if '_slat_' in n:target='LOW_'+n.split('_slat_')[0]+'_dark_tray'
    if '_fan_blade_' in n:target='LOW_Dock_roof_fan_dark_well'
    if '_inner_process_node_' in n:target='LOW_'+n.split('_inner_process_node_')[0]+'_inner_well_floor'
    if '_inner_well_seat' in n:target='LOW_'+n.split('_inner_well_seat')[0]+'_continuous_sealed_wall'
    if '_rolled_band_' in n:target='LOW_'+n.split('_rolled_band_')[0]+'_continuous_sealed_wall'
    if '_rim_clamp_bolt_' in n:
        prefix,suffix=n.split('_rim_clamp_bolt_');target='LOW_'+prefix+'_rim_clamp_'+suffix.split('_')[0]
    if n.startswith('Chimney_panel_rivet_'):target='LOW_Chimney_tapered_panel_course_'+n.split('_')[3]
    if n.startswith('Jamb_bolt_'):
        s=n.split('_');target='LOW_Dock_jamb_panel_'+s[2]+'_'+s[3]
    pts=[src.matrix_world@v.co for v in src.data.vertices];center=sum(pts,Vector())/len(pts)
    if target is None:
        candidates=[(tree.find_nearest(center)[3],name) for name,tree in trees.items()]
        distance,target=min(candidates)
        automatic.append({'source':n,'receiver':target,'center_surface_distance':distance,'method':'Nearest actual receiver surface; named high detail only, evaluated ray check follows'})
    assert target in mapping,(n,target)
    mapping[target].append(n)
for name in mapping:mapping[name]=list(dict.fromkeys(mapping[name]))
used={n for row in mapping.values() for n in row};assert used|{e['source'] for e in excluded}=={o.name for o in high.objects}
materials=collections.Counter((o.data.materials[0].name if o.data.materials else '<NONE>') for o in high.objects)
report={'scope':'Complete approved FinalLow v02, 240 receivers; original approved High v04 sources','receiver_to_sources':mapping,'nearest_surface_assignments':automatic,'excluded_authoring_guides':excluded,'source_material_counts':dict(materials),'source_count':len(used),'receiver_count':len(mapping),'preserved_trial_receivers':list(old)}
(O/'SOURCE_RECEIVER_MAP.json').write_text(json.dumps(report,indent=2)+'\n')
print('MAPPING',len(mapping),len(used),'materials',dict(materials),flush=True)
print('NEAREST_MAX',max(r['center_surface_distance'] for r in automatic))
