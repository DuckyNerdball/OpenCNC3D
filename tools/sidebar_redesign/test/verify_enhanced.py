#!/usr/bin/env python3
"""Verify the game's portable raster against independent, approved Lab snapshots."""
from pathlib import Path
import subprocess,tempfile
from PIL import Image
root=Path(__file__).resolve().parents[3];here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='cnc-enhanced-raster-') as work:
 d=Path(work);exe=d/'native'
 subprocess.run(['cc','-std=c89','-O2','-I',str(root/'game'),str(here/'enhanced_native.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(root/'playable/hud640.pack'),str(d)],check=True)
 body=Image.frombytes('RGBA',(245,693),(d/'body.rgba').read_bytes()).crop((61,18,245,693))
 expected=Image.open(here/'approved/body.png').crop((0,18,184,693))
 # Tabs are rendered by the UI control compositor, outside the framed-body oracle.
 for im in [body,expected]:
  for y in range(229-18,264-18):
   for x in range(19,165):im.putpixel((x,y),(0,0,0,0))
 assert body.tobytes()==expected.tobytes(),'Approved framed body differs from runtime'
 for faction in ['gdi','nod']:
  for progress in ['0','38','100']:
   actual=(d/(faction+'-'+progress+'.rgba')).read_bytes();expected=Image.open(here/'approved'/(faction+'-'+progress+'.png')).convert('RGBA').tobytes()
   assert actual==expected,(faction,progress)
 assert (d/'buttons.rgba').read_bytes()==Image.open(here/'approved/buttons.png').convert('RGBA').tobytes(),'Hover-lit controls differ from approved Lab'
 print('PASS: approved frame, 6 faction clocks and 14 hover-lit control states match exactly')
