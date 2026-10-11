from refinery_review_font import load_review_font
"""Actual mesh and static harvester-fit evidence for the apron proposal."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageOps
import hashlib,json
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/high_v03'
FONT='/System/Library/Fonts/Supplemental/Arial.ttf'
def img(n):return OUT/f'PROC_High_v03_{n}.png'
def sheet(name,title,items):
 im=Image.new('RGB',(2400,2080),'#20262b');d=ImageDraw.Draw(im)
 d.text((26,23),title,font=load_review_font(FONT,29),fill='#f2f2ec')
 for i,(p,label) in enumerate(items):
  tile=ImageOps.contain(Image.open(p).convert('RGB'),(1200,945),Image.Resampling.LANCZOS)
  x=i%2*1200;y=80+i//2*1000;im.paste(tile,(x+(1200-tile.width)//2,y+(945-tile.height)//2))
  d.text((x+20,y+956),label,font=load_review_font(FONT,24),fill='#eeeeee')
 im.save(OUT/name)
sheet('PROC_High_v03_review_sheet.png','PROC HIGH v03 | ACTUAL MESH | FRONT FOOTPRINT EXTENSION PROPOSAL',[(img('hero'),'Revised front apron — structure unchanged'),(img('top'),'Top — original width and rear edge retained'),(img('harvester_fit'),'Original harvester at actual scale — static QA reference'),(img('apron_detail'),'Longer concrete approach with three spaced arrows')])
sheet('PROC_High_v03_before_after.png','ACTUAL v02 / v03 | MATCHED CAMERAS AND SCALE',[(OUT/'PROC_High_v02_matched_hero.png','Before — v02'),(img('hero'),'Proposed v03 — 5.4 units added at front'),(OUT/'PROC_High_v02_matched_top.png','Before — plan view'),(img('top'),'Proposed — plan view')])
files=[p for p in OUT.iterdir() if p.suffix in {'.png','.blend','.json','.md'} and p.name!='REVIEW_MANIFEST.json']
(OUT/'REVIEW_MANIFEST.json').write_text(json.dumps({p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)},indent=2)+'\n')
print('Review images and manifest saved.')
