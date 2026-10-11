from refinery_review_font import load_review_font
"""Labeled contact sheets from immutable actual Blender review renders."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import hashlib,json
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'art/refinery/texture_v02';R=OUT/'material_review';B=OUT/'bake_review/complete'
font=load_review_font('/System/Library/Fonts/Helvetica.ttc',25);small=load_review_font('/System/Library/Fonts/Helvetica.ttc',19)
def sheet(name,title,subtitle,entries,cols=2,cell=(700,600)):
 w,h=cell;rows=(len(entries)+cols-1)//cols;im=Image.new('RGB',(w*cols,rows*h+100),(24,28,31));d=ImageDraw.Draw(im);d.text((18,14),title,font=font,fill='white');d.text((18,54),subtitle,font=small,fill=(202,208,211))
 for i,(path,label) in enumerate(entries):
  pic=Image.open(path).convert('RGB');pic.thumbnail((w,h-44));x=(i%cols)*w+(w-pic.width)//2;y=100+(i//cols)*h;im.paste(pic,(x,y+35));d.text(((i%cols)*w+12,y+7),label,font=small,fill='white')
 path=OUT/name;im.save(path);return {'file':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'source_files':[str(p.relative_to(ROOT)) for p,_ in entries]}
results=[]
if (R/'core_GDI.png').exists():
 results.append(sheet('PROC_FullMaterial_v02_review.png','ACTUAL REFINERY — FULL MATERIAL v02','9,992 triangles | All 240 receivers mapped. Full-asset material review.',[(R/'hero_GDI.png','Whole asset / GDI'),(R/'rear_GDI.png','Rear / complete sheet coverage'),(R/'front_silo_GDI.png','Front silo / full triangular plating'),(R/'apron_GDI.png','Concrete / shaped stripes / three arrows'),(R/'core_GDI.png','Rear grille / physical normal + local AO'),(R/'hero_Nod.png','Red player tint / complete asset')]))
if (R/'hero_Blue.png').exists():
 results.append(sheet('PROC_FullMaterial_v02_player_colours.png','ACTUAL PLAYER-COLOUR CHECK','Same base, light, roughness and metal maps. Separate grayscale player mask.',[(R/('hero_'+v+'.png'),v+' / whole asset') for v in ['GDI','Nod','Blue']],3,(600,530)))
for light in ['key','reverse']:
 if (B/f'core_{light}_normal_4096.png').exists():
  results.append(sheet('PROC_BakeComparison_'+light+'.png','ACTUAL HIGH / PLAIN LOW / PHYSICAL NORMAL — '+light.upper(),'Matched neutral material and light. AO, pigment and microheight OFF. Full asset physical bake.',[(B/f'{v}_{light}_{x}.png',v.upper()+' / '+label) for v in ['front_silo','tank','gameplay'] for x,label in [('high','ACTUAL HIGH v04'),('plain_low','PLAIN LOW'),('normal_4096','BAKED LOW 4K')]],3,(600,500)))
(OUT/'REVIEW_SHEETS.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
if (R/'paint_GDI_reverse.png').exists():
 results.append(sheet('PROC_FullMaterial_v02_light_check.png','ACTUAL MAPPED MATERIAL — MOVING LIGHT CHECK','Same decoded textures, view transform, exposure and cameras; key reflected, fill/world fixed.',[(R/(v+'_GDI'+suffix+'.png'),v.upper()+' / '+label) for v in ['paint','front_silo'] for suffix,label in [('','original key'),('_reverse','reversed key')]]))
if (R/'hero_PlayerColorMask.png').exists():
 results.append(sheet('PROC_FullMaterial_v02_channels.png','ACTUAL MODEL — MATERIAL CHANNELS','Actual mapped data channel, displayed without lighting.',[(R/('hero_'+v+'.png'),v) for v in ['BaseColor','Roughness','Metallic','PlayerColorMask']] ))
(OUT/'REVIEW_SHEETS.json').write_text(json.dumps(results,indent=2)+'\n')
