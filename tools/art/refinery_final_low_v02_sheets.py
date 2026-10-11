from refinery_review_font import load_review_font
"""Actual before/after chamfer evidence and complete three-silo layout review."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import json,hashlib
R=Path(__file__).resolve().parents[2];O=R/'art/refinery/final_low_v02';font=load_review_font('/System/Library/Fonts/Helvetica.ttc',25);small=load_review_font('/System/Library/Fonts/Helvetica.ttc',19)
def sheet(name,title,subtitle,rows):
 w,h=850,735;im=Image.new('RGB',(2*w,100+h*len(rows)),(24,28,31));d=ImageDraw.Draw(im);d.text((16,12),title,font=font,fill='white');d.text((16,53),subtitle,font=small,fill=(206,211,214))
 for y,row in enumerate(rows):
  for x,(file,label) in enumerate(row):
   pic=Image.open(O/file).convert('RGB');pic.thumbnail((w,h-50));im.paste(pic,(x*w+(w-pic.width)//2,100+y*h+40));d.text((x*w+12,106+y*h),label,font=small,fill='white')
 p=O/name;im.save(p)
sheet('PROC_FinalLow_v02_chamfer_comparison.png','ACTUAL MESH CHAMFERS — BEFORE / AFTER','Identical clay, cameras and lighting. No normal maps or texture highlights.',[
 [('PROC_FinalLow_v01_front_silo_clay.png','Before / sharp low-poly rim and clamps'),('PROC_FinalLow_v02_front_silo_clay.png','After / real chamfers on every silo rim and clamp')],
 [('PROC_FinalLow_v01_hero_clay.png','Before / whole asset'),('PROC_FinalLow_v02_hero_clay.png','After / 9,992 triangles, same footprint')]])
sheet('PROC_FinalLow_v02_review.png','REFINERY FINAL LOW v02 — CHAMFERS AND COMPLETE SHEET LAYOUT','Actual mesh. Temporary triangle-layout guides; approved finish will be rebaked after mesh approval.',[
 [('PROC_FinalLow_v02_hero.png','Front view / full front-silo sheet pattern'),('PROC_FinalLow_v02_rear.png','Rear view / sheet layout continues around all three silos')],
 [('PROC_FinalLow_v02_front_silo.png','Front silo / 40 triangles, two tiers around all 10 bays'),('PROC_FinalLow_v02_silo_rims.png','Silo rims / real top-edge chamfers, fitted seats retained')]])
print('PROC_V02_SHEETS_READY')
