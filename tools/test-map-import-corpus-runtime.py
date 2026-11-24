#!/usr/bin/env python3
"""Cold hidden native world probes for every imported fan-map ID, not a sample."""
import argparse, concurrent.futures, json, os, re, subprocess, time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('runtime',type=Path);p.add_argument('imports',type=Path);p.add_argument('evidence',type=Path);p.add_argument('--jobs',type=int,default=2);p.add_argument('--enhanced',action='store_true');a=p.parse_args()
play=a.runtime.resolve();imports=a.imports.resolve();qa=a.evidence.resolve();qa.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,OPENCNC3D_BACKGROUND='1',SDL_MAC_BACKGROUND_APP='1')

def fields(path):
 section='';values={}
 for line in path.read_text(errors='replace').splitlines():
  line=line.strip()
  if line.startswith('['):section=line.split(']',1)[0][1:].lower()
  elif '=' in line:
   k,v=line.split('=',1);values[section+'.'+k.strip().lower()]=v.strip()
 return values

def run(path):
 f=fields(path);red=path.parent.name=='ra';mp=f.get('basic.opencnc3dkind','').lower().startswith('multi');tag=('ra' if red else 'td')+'-'+path.stem;d=qa/tag;d.mkdir(exist_ok=True);(d/'saves').mkdir(exist_ok=True)
 theater=f.get('map.theater','').upper();pack='ra3d/SCG01EA.pack' if red else {'DESERT':'SCB01EA.pack','WINTER':'SCW01EA.pack','SNOW':'SCS01EA.pack','SAND':'SCA01EA.pack'}.get(theater,'SCG01EA.pack')
 script=d/'probe.txt';script.write_text(('gfxmode 2\n' if a.enhanced else '')+f'tick 120\nplayer\nshroudstat\nshot {d / "world.png"}\nquit\n')
 cmd=[str(play/'cnc_eyes'),'--nosound','--w','1280','--h','720','--scen',path.stem,'--dir',str(path.parent)+'/', '--content','ra3d/data/' if red else 'content/','--dylib','ra3d/RedAlert.dylib' if red else 'TiberianDawn.dylib','--pack',pack,'--gfx',str(play/('ra3d-fx.cfg' if red else 'opencnc3d-fx.cfg')),'--gfxsave',str(d/'unused.cfg'),'--visualscfg',str(d/'visuals.cfg'),'--savedir',str(d/'saves'),'--script',str(script)]
 cmd+=['--ra3d'] if red else ['--reskin']
 if mp:cmd+=['--skirmish','--ai','1','--seed','42']
 start=time.monotonic()
 try:
  with (d/'world.log').open('w') as log:r=subprocess.run(cmd,cwd=play,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
  status=r.returncode
 except subprocess.TimeoutExpired:status='timeout'
 text=(d/'world.log').read_text(errors='replace');tick=re.search(r'TICK\|120\|objects=(\d+)\|frame=(\d+)',text);sight=re.search(r'SHROUDSTAT\|[^\n]*\|clear=(\d+)',text);house=re.search(r'PLAYER\|house=([^|]+)',text);grid=re.search(r'MAP theater=(\d+) origin(\d+),(\d+) size(\d+)x(\d+)',text)
 passed=status==0 and tick is not None and int(tick[1])>0 and int(tick[2])==120 and sight is not None and int(sight[1])>0
 if a.enhanced and 'GFXMODE|enhanced|script|texset=3' not in text:passed=False
 reason=[]
 if status:reason.append('process '+str(status))
 if not tick or tick[2]!='120':reason.append('simulation did not reach frame120')
 if not sight or int(sight[1])==0:reason.append('no player sight')
 if house and not mp and house[1].lower()!=f.get('basic.player','').lower():passed=False;reason.append('wrong player house')
 result=dict(map=path.stem,name=f.get('basic.name'),game='ra' if red else 'td',mode='mp' if mp else 'sp',theater=theater,source=str(path),passed=passed,exit=status,objects=int(tick[1]) if tick else None,frame=int(tick[2]) if tick else None,clear_cells=int(sight[1]) if sight else None,player=house[1] if house else None,grid=grid.groups() if grid else None,reason=reason,seconds=round(time.monotonic()-start,2),command=cmd,log=str(d/'world.log'))
 print(('PASS' if passed else 'FAIL'),tag,result['name'],reason,flush=True);return result

paths=sorted(p for kind in ['td','ra'] for p in (imports/kind).glob('IMP*.INI'))
with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:results=list(pool.map(run,paths))
(qa/'result.json').write_text(json.dumps(dict(passed=all(r['passed'] for r in results),count=len(results),results=results),indent=2)+'\n')
print('NATIVE CORPUS',sum(r['passed'] for r in results),'/',len(results),flush=True)
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
