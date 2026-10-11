from refinery_review_font import load_review_font
"""Label actual Blender renders and matched v01/v02 comparisons."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageOps
import hashlib,json
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/high_v02';OLD=ROOT/'art/refinery/high_v01'
font='/System/Library/Fonts/Supplemental/Arial.ttf'
def img(n):return OUT/f'PROC_High_v02_{n}.png'
def sheet(name,title,items):
 im=Image.new('RGB',(2400,2080),'#20262b');d=ImageDraw.Draw(im)
 d.text((26,23),title,font=load_review_font(font,30),fill='#f2f2ec')
 for i,(file,label) in enumerate(items):
  tile=ImageOps.contain(Image.open(file).convert('RGB'),(1200,945),Image.Resampling.LANCZOS)
  x=i%2*1200;y=80+i//2*1000;im.paste(tile,(x+(1200-tile.width)//2,y+(945-tile.height)//2))
  d.text((x+20,y+956),label,font=load_review_font(font,25),fill='#eeeeee')
 im.save(OUT/name)
sheet('PROC_High_v02_contact_sheet.png','PROC HIGH v02 | ACTUAL BLENDER MESH | provisional material IDs',[(img(n),label) for n,label in [('hero','Front: lowered structure, filled bay and three arrows'),('rear','Rear: shorter, wider tank; triangular metal sheets'),('side','True side elevation — same camera and scale as v01'),('top','Top: shaped base and fitted assemblies')]])
sheet('PROC_High_v02_before_after.png','ACTUAL HIGH v01 / v02 | MATCHED CAMERAS, SCALE AND LIGHTING',[(OLD/'PROC_High_v01_hero.png','Before — High v01'),(img('hero'),'Revised — High v02'),(OLD/'PROC_High_v01_side.png','Before — true side'),(img('side'),'Revised — true side')])
sheet('PROC_High_v02_details.png','PROC HIGH v02 | ACTUAL MESH / CONSTRUCTION DETAILS',[(img('apron_detail'),'Shaped stripe field and three approach arrows'),(img('core_detail'),'Rear tank: interlocking triangular metal sheets'),(img('pipe_detail_clay'),'Neutral clay: paired tanks and pipe connections'),(img('core_detail_clay'),'Neutral clay: rear tank and fitted connections')])
files=[p for p in OUT.iterdir() if p.suffix in {'.png','.blend','.md','.json'} and p.name!='REVIEW_MANIFEST.json']
(OUT/'REVIEW_MANIFEST.json').write_text(json.dumps({p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)},indent=2)+'\n')
print('Review sheets and hashes saved.')
