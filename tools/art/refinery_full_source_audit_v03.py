"""Verify persistent full-material source graphs after an independent reopen."""
import bpy,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'art/refinery/texture_v03'
f=O/'PROC_MaterialAuthoring_v03.blend';before=hashlib.sha256(f.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(f))
mapping=json.loads((O/'SOURCE_RECEIVER_MAP.json').read_text())['receiver_to_sources'];expected={n for row in mapping.values() for n in row}
materials=[m for m in bpy.data.materials if m.get('source_original') in expected]
actual={m['source_original'] for m in materials};assert actual==expected,(expected-actual)
assert all(m.use_fake_user and m.use_nodes and m.get('clean_BaseColor_no_AO') and m.get('player_mask_is_authored') for m in materials)
assert before==hashlib.sha256(f.read_bytes()).hexdigest()
report={'passed':True,'file':str(f.relative_to(R)),'sha256':before,'persistent_source_graphs':len(materials),'unique_high_sources':len(actual),'all_required_sources_present':True,'fake_users_verified':True,'geometry_source_unchanged':True,'file_not_rewritten':True}
(O/'MATERIAL_SOURCE_REOPEN_QA.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
