from refinery_review_font import load_review_font
"""Assemble labeled actual render comparisons; no generated image editing."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import json,hashlib
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/final_low_v01';HIGH=ROOT/'art/refinery/high_v04'
font=load_review_font('/System/Library/Fonts/Helvetica.ttc',30)
small=load_review_font('/System/Library/Fonts/Helvetica.ttc',24)
def sheet(name,rows,title,foot):
    w=2800;h=80+1150*len(rows)+65
    im=Image.new('RGB',(w,h),(25,29,33));d=ImageDraw.Draw(im);d.text((25,20),title,fill='white',font=font)
    for j,row in enumerate(rows):
        for i,(path,label) in enumerate(row):
            pic=Image.open(path).convert('RGB');assert pic.size==(1400,1100)
            x=i*1400;y=80+j*1150;im.paste(pic,(x,y));d.text((x+25,y+1110),label,fill='white',font=small)
    d.text((25,h-48),foot,fill=(220,221,224),font=small);im.save(OUT/name)
lp=lambda v:OUT/f'PROC_FinalLow_v01_{v}.png'
hp=lambda v:HIGH/f'PROC_High_v04_{v}.png'
sheet('PROC_FinalLow_v01_review_sheet.png',[
    [(lp('hero'),'Actual low: front three-quarter'),(lp('rear'),'Actual low: rear three-quarter')],
    [(lp('side'),'Actual low: right side'),(lp('top'),'Actual low: top / full rounded apron')]],
    'ACTUAL FINAL LOW v01 — 9,796 TRIANGLES — MESH APPROVAL',
    'Untextured. Fine sheet seams, bolts and grille/vent relief remain in the high source for later bakes.')
sheet('PROC_HighLow_v01_comparison.png',[
    [(hp('hero'),'Approved High v04 — same camera and light'),(lp('hero'),'FinalLow v01 — no baked detail yet')],
    [(hp('apron_detail'),'Approved high: rounded apron and docking approach'),(lp('apron_detail'),'Actual low: retained apron / three arrows / filled hazard bay')]],
    'ACTUAL HIGH / LOW COMPARISON — MATCHED VIEWS',
    'High remains preserved. Coloured markings are non-export layout guides; final materials and masks come later.')
sheet('PROC_FinalLow_v01_clay_sheet.png',[
    [(lp('hero_clay'),'Actual neutral clay: silhouette and broad faces'),(lp('dock_detail_clay'),'Actual clay detail: arch, supports, chamfers and nozzle')]],
    'ACTUAL FINAL LOW v01 — NEUTRAL CLAY CHECK',
    'Real Blender geometry. No generated finish, final textures or baked normal detail in these views.')
manifest={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(OUT.iterdir()) if p.is_file() and p.name!='REVIEW_MANIFEST.json'}
(OUT/'REVIEW_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('PROC final-low actual review sheets and hashes saved')
