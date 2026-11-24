"""Normal sidebar construction on original sloped maps, with isolated owned bases.

Private INIs change ownership and disable mission automation. Every standing
building keeps its original position/type; map BIN and rendered pack are copied
or read unchanged. This is a construction fixture, not the reporter's save.
"""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

W=Path(__file__).resolve().parents[3]
Q=RUN=INSTALL=T=None
sys.path.insert(0,str(W/'tools/bakery'))
import packinspect

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def parse_pads(s):
 rows={}
 for line in s.splitlines():
  if not re.match(r'PADCHECK\|[^|]+\|id=',line):continue
  fields=dict(t.split('=',1) for t in line.split('|')[2:] if '=' in t)
  rows[int(fields['id'])]=fields
 return rows

def phase(s,name):
 return s.split('ECHO|'+name+'\n',1)[1].split('\nECHO|',1)[0]

def fixture(name,ra,scen):
 d=T/name;d.mkdir(parents=True,exist_ok=True)
 source=INSTALL/('ra3d/missions' if ra else 'missions')/(scen+'.INI')
 ini=configparser.ConfigParser(strict=False,interpolation=None)
 ini.optionxform=str;ini.read(source)
 human='Greece' if ra else 'GoodGuy'
 changed=0
 for k,v in list(ini['STRUCTURES'].items()):
  parts=v.split(',');parts[0]=human;ini['STRUCTURES'][k]=','.join(parts);changed+=1
 for section in ['Triggers','Trigs','TeamTypes','CellTriggers','Base']:
  if section in ini:ini[section].clear()
 for k in list(ini['Basic']):
  if k.lower()=='player':ini['Basic'][k]=human
 ini['Basic']['BuildLevel']='98'
 ini['Basic']['BaseBuilding']='yes'
 ini[human]['TechLevel']='10'
 for key in ['MaxBuilding','MaxUnit','MaxInfantry','MaxAircraft']:ini[human][key]='150'
 # Initial native discovery is done during scenario load. A human observer loaded
 # beside the yard (rather than debug-spawned afterwards) scouts its inherited base.
 for v in ini['STRUCTURES'].values():
  parts=v.split(',')
  if parts[1].upper()=='FACT':
   cell=int(parts[3]);stride=128 if ra else 64
   if 'INFANTRY' not in ini:ini.add_section('INFANTRY')
   ini['INFANTRY']['998']=f'{human},E1,256,{cell+2*stride+1},0,Guard,0,None'
   ini['Waypoints']['26']=str(cell)
   break
 # A unique scenario key avoids the installed mission/MIX lookup shadowing the
 # private INI when the original scenario name already exists in the runtime.
 target=d/'SCG86EA.INI'
 with target.open('w') as f:ini.write(f,space_around_delimiters=False)
 binary=source.with_suffix('.BIN')
 if binary.exists():shutil.copy2(binary,d/'SCG86EA.BIN')
 pack=INSTALL/('ra3d/3d' if ra else '')/(scen+'.pack')
 (d/'inputs.json').write_text(json.dumps(dict(source=str(source),source_sha256=sha(source),
   fixture_sha256=sha(target),pack=str(pack),pack_sha256=sha(pack),
   structures_ownership_changed=changed,automation='cleared only in private INI',
   geometry='original positions/types, unchanged original map BIN and rendered pack',
   fixture_changes='owned bases, human observer and start waypoint, tech/limits enabled; original mission automation cleared'),indent=2)+'\n')
 return d,pack

def run(d,pack,scen,ra,lines,label,exe):
 (d/'saves').mkdir(exist_ok=True)
 script=d/(label+'.txt');script.write_text('\n'.join(lines)+'\n')
 engine=('RedAlert' if ra else 'TiberianDawn')+('.dll' if os.name=='nt' else '.dylib')
 cmd=[str(exe),'--scen','SCG86EA','--pack',str(pack),'--dir',str(d)+'/',
  '--dylib',str(INSTALL/('ra3d' if ra else '')/engine),
  '--content','ra3d/data/' if ra else 'content/','--nosound','--w','960','--h','600',
  '--enhanced','--noshroud','--build','98','--script',str(script),
  '--gfxsave',str(d/(label+'.cfg')),'--visualscfg',str(d/(label+'-visuals.cfg')),
  '--savedir',str(d/'saves')]
 if ra:cmd+=['--ra3d','--dosdata','ra3d/audio/']
 (d/(label+'-command.json')).write_text(json.dumps(cmd,indent=2)+'\n')
 with (d/(label+'.log')).open('w') as f:
  rc=subprocess.run(cmd,cwd=RUN,env={**os.environ,'OPENCNC3D_BACKGROUND':'1'},stdout=f,stderr=subprocess.STDOUT,timeout=180).returncode
 s=(d/(label+'.log')).read_text(errors='replace')
 assert rc==0 and re.search(r'SCRIPT\|end .*0 failures',s), (label,rc)
 return s

def main():
 global Q,T,RUN,INSTALL
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--exe',type=Path,required=True)
 parser.add_argument('--runtime',type=Path,required=True,help='Private runtime with complete installed assets')
 parser.add_argument('--installation',type=Path,required=True,help='Source of original maps, packs and paired engines')
 parser.add_argument('--out',type=Path,required=True)
 parser.add_argument('--expect-stable',action='store_true',help='Legacy independent-height diagnostic')
 parser.add_argument('--expect-connected',action='store_true',help='Require zero vertical foundation edges in every pass')
 parser.add_argument('--capture',action='store_true')
 parser.add_argument('--save-load',action='store_true')
 args=parser.parse_args();args.exe=args.exe.resolve();Q=T=args.out.resolve()
 RUN=args.runtime.resolve();INSTALL=args.installation.resolve();results=[]
 for name,scen,ra in [('td-n64-ops1','SCG30EA',False),('td-ops1','SCG60EA',False),('ra-allied8','SCG08EA',True)]:
  d,pack=fixture(name,ra,scen);code='TENT' if ra else 'PYLE'
  setup=['cheatset money 1','cheatset invuln 1','cheatset instant 1','cheatset fog 0','tick 60','frames 1',
   'padcheck','sbrstate','player','count FACT:@me','sidebar before','build '+code,'ready '+code+' 100','startplace '+code]
  s=run(d,pack,scen,ra,setup+['placemap','quit'],'legal',args.exe)
  before=parse_pads(s)
  origin=re.search(r'PLACEMAP\|origin=(\d+),(\d+)',s);assert origin,name
  xoff=int(origin[1]);sites=[]
  p=packinspect.read(pack);corner=p['corner'];stride=p['mapW']+1
  for row in re.finditer(r'^PLACEMAP\|\s*(\d+) ([#pc.]+)$',s,re.M):
   y=int(row[1])
   for x,ch in enumerate(row[2],xoff):
    if ch!='#':continue
    top=max(corner[(y+dy)*stride+x+dx] for dy in range(3) for dx in range(3))
    jump=0;touch=[]
    for bid,b in before.items():
     bx,by=map(int,b['cell'].split(','));bw,bh=map(int,b['size'].split('x'))
     if x<=bx+bw and bx<=x+2 and y<=by+bh and by<=y+2:
      jump=max(jump,top-float(b['anchor']));touch.append(bid)
    sites.append(dict(x=x,y=y,raw_top=top,predicted_direct_lift=jump,touch=touch))
  assert sites,(name,'no legal placement')
  sites.sort(key=lambda t:(t['predicted_direct_lift'],t['raw_top']),reverse=True)
  (d/'ranked-sites.json').write_text(json.dumps(sites,indent=2)+'\n')
  for index,site in enumerate(sites[:3]):
   camera=[f"cam {site['x']+4} {site['y']+4}",'dist 2400','frames 3']
   before_shot=['shot '+str(d/'before.png')] if args.capture and index==0 else []
   after_shot=['shot '+str(d/'after.png')] if args.capture and index==0 else []
   reload=['save 0 Foundation_roundtrip','build '+code,'ready '+code+' 100','startplace '+code,
    'place auto','tick 60','frames 2','echo abandoned','padcheck','padsteps',
    'load 0','frames 3','echo reloaded','padcheck','padsteps','sbrstate'] if args.save_load else []
   lines=setup+camera+['echo before','padcheck','padsteps','sbrstate']+before_shot+[f"place {site['x']} {site['y']}",
    'tick 60','frames 3','echo after','padcheck','padsteps','sbrstate']+after_shot+['tick 300','frames 2',
    'echo settled','padcheck','sbrstate']+reload+['quit']
   s=run(d,pack,scen,ra,lines,'site'+str(index),args.exe)
   bef=parse_pads(phase(s,'before'))
   aft=parse_pads(phase(s,'after'))
   settled=parse_pads(phase(s,'settled'))
   assert len(aft)==len(bef)+1,(name,site,'building not placed')
   assert all(int(b['buried'])==0 and b['pad']==b['low'] for b in aft.values())
   assert aft==settled,(name,site,'pads continue to change without construction')
   if args.save_load:
    abandoned=parse_pads(phase(s,'abandoned'))
    assert len(abandoned)==len(aft)+1,(name,site,'save/load did not change the construction graph')
    reloaded=parse_pads(phase(s,'reloaded'))
    assert aft==reloaded,(name,site,'save/load changes foundations')
   hashes=re.findall(r'^SBRSTATE\|.*',s,re.M)
   lifts={str(b):float(aft[b]['anchor'])-float(v['anchor']) for b,v in bef.items()
          if b in aft and float(aft[b]['anchor'])!=float(v['anchor'])}
   if args.expect_stable:assert not lifts,(name,site,lifts)
   edges=re.findall(r'^PADSTEPS\|edges=(\d+)\|max-height=(\d+)\|colour=(\d+)\|normals=(\d+)\|depth=(\d+)',s,re.M)
   if args.expect_connected:assert len(edges)>1 and all(all(int(v)==0 for v in row)for row in edges),(name,site,edges)
   results.append(dict(case=name,site=site,buildings_before=len(bef),buildings_after=len(aft),
    existing_building_lifts=lifts,all_flat=True,buried=0,stable_after_300_ticks=True,
    save_reload_verified=args.save_load,continuous_edges_verified=args.expect_connected,raw_states=hashes))
   (T/'results.json').write_text(json.dumps(results,indent=2)+'\n')
   print(name,site['x'],site['y'],'existing lifts',lifts,flush=True)

if __name__=='__main__':main()
