from refinery_review_font import load_review_font
"""Assemble labelled review evidence; source render pixels remain unchanged."""
from pathlib import Path
import json,hashlib
from PIL import Image,ImageDraw,ImageFont,ImageOps
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/high_v01'
fontpath='/System/Library/Fonts/Supplemental/Arial.ttf'
def font(n):return load_review_font(fontpath,n)
def sheet(filename,title,items):
 w=2400;cw=1200;ch=1000;rows=(len(items)+1)//2
 im=Image.new('RGB',(w,80+rows*ch),'#20262b');d=ImageDraw.Draw(im);d.text((26,23),title,font=font(29),fill='#f2f2ec')
 for i,(p,label,crop) in enumerate(items):
  src=Image.open(p).convert('RGB')
  if crop:src=src.crop(crop)
  src=ImageOps.contain(src,(cw,ch-54),Image.Resampling.LANCZOS)
  x=(i%2)*cw;y=80+(i//2)*ch
  im.paste(src,(x+(cw-src.width)//2,y+(ch-54-src.height)//2))
  d.text((x+20,y+ch-42),label,font=font(25),fill='#e5e8e6')
 im.save(OUT/filename)
def img(name):return OUT/f'PROC_High_v01_{name}.png'
sheet('PROC_High_v01_contact_sheet.png','PROC HIGH v01  |  ACTUAL BLENDER MESH  |  provisional material IDs',[(img(n),t,None) for n,t in [('hero','Front three-quarter'),('rear','Rear three-quarter'),('side','True right-side elevation — taller approved proportions'),('top','Top — measured source apron preserved')]])
sheet('PROC_High_v01_detail_sheet.png','PROC HIGH v01  |  PHYSICAL CONSTRUCTION / JOINT REVIEW',[(img(n),t,None) for n,t in [('dock_detail','Dock: thick arch, roof vents, seated nozzle'),('core_detail','Rear core: rim, grille, three struts, access hardware'),('pipe_detail','Vessels: open wells, pipe sockets and flange seats'),('chimney_seat_detail','Chimney: fitted socket on retained proxy support')]])
sheet('PROC_High_v01_clay_sheet.png','PROC HIGH v01  |  ACTUAL NEUTRAL CLAY / NO TEXTURES',[(img(n+'_clay'),t,None) for n,t in [('hero','Whole asset — neutral clay'),('side','True side — neutral clay'),('dock_detail','Arch construction — neutral clay'),('core_detail','Core construction — neutral clay')]])
ref=ROOT/'art/refinery/generated_design_v03/PROC_DesignDirection_v03.png'
sheet('PROC_High_v01_design_comparison.png','APPROVED GENERATED DESIGN v03  /  ACTUAL HIGH v01 — approximate camera match',[(ref,'Approved generated design v03 — finish is concept only',(0,0,783,503)),(img('concept_match'),'Actual High v01 — flat material IDs, no final textures',None),(ref,'Approved generated angled side',(0,505,783,1024)),(img('side'),'Actual High v01 — true orthographic side',None)])
files=[p for p in OUT.iterdir() if p.suffix.lower() in {'.png','.blend','.json','.md'} and p.name!='REVIEW_MANIFEST.json']
manifest={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)}
(OUT/'REVIEW_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Review sheets and file manifest saved.')
