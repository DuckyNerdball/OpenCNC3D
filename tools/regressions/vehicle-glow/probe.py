"""Hidden native wreck-glow equivalence and shared-pose scope checks.

Run each platform independently; no live installation preferences are changed.
"""
from pathlib import Path
import argparse,configparser,hashlib,importlib.util,json,os,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);p.add_argument('--exe',required=True,type=Path);p.add_argument('--baseline',required=True,type=Path);p.add_argument('--helper',required=True,type=Path);p.add_argument('--out',required=True,type=Path);p.add_argument('--game',choices=['td','ra','both'],default='both');a=p.parse_args()
r=a.runtime.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
sp=importlib.util.spec_from_file_location('fixture',a.helper.resolve());h=importlib.util.module_from_spec(sp);sp.loader.exec_module(h)
rows=[]
for game in (['td','ra']if a.game=='both'else[a.game]):
 ra=game=='ra';code='4TNK'if ra else'HTNK';stride=128 if ra else 64;owner='Greece'if ra else'GoodGuy';pictures=[]
 for label,mode,verify in [('original',0,False),('disabled',0,False),('fast',1,False),('witness',1,True)]:
  d=out/game/label;d.mkdir(parents=True);f=h.fixture(d,ra,'controls');ini=configparser.ConfigParser(strict=False,interpolation=None);ini.optionxform=str;ini.read(f/'SCG88EA.INI');ini['Units'].clear();cells=[(43+(i%4)*4,43+(i//4)*4)for i in range(12)]
  for i,(x,y)in enumerate(cells):ini['Units'][str(i)]=f'{owner},{code},256,{y*stride+x},0,Guard,None'
  with(f/'SCG88EA.INI').open('w')as file:ini.write(file,space_around_delimiters=False)
  lines=['gfxmode 2','gfx show_stats 0','tick 5','reveal 1','cam 49 47','dist 3100',f'count {code} 12'];shots=[]
  for phase,delta in [('intact',0),('burst',1),('flight',15),('settling',25),('sink',40),('late',60),('gone',500)]:
   if phase=='burst':
    for x,y in cells:lines.extend([f'hurtcell {x} {y}']*(16 if ra else 8))
   if delta:lines.append(f'tick {delta}')
   if phase=='flight':
    for view,cam in [('away','cam 100 100'),('return','cam 49 47')]:
     pic=d/(phase+'-'+view+'.png');shots.append(pic);lines.extend([cam,'tickalpha 0.35','frames 5','meshperfdump','shot '+str(pic)])
   shot=d/(phase+'.png');shots.append(shot);lines.extend(['tickalpha 0.35','frames 5','meshperfdump','shot '+str(shot)])
  lines.extend([f'count {code} 0','physicsdump','quit']);script=d/'script.txt';script.write_text('\n'.join(lines)+'\n')
  win=os.name=='nt';engine=('ra3d/RedAlert'if ra else'TiberianDawn')+('.dll'if win else'.dylib');gfx=r/(('performance-ra.cfg'if ra else'performance-td.cfg')if win else'ra3d-fx.cfg'if ra else'opencnc3d-fx.cfg');exe=(a.baseline if label=='original'else a.exe).resolve()
  cmd=[str(exe),'--scen','SCG88EA','--pack','ra3d/3d/SCG01EA.pack'if ra else'SCG01EA.pack','--dir',str(f)+'/', '--dylib',engine,'--content','ra3d/data/'if ra else'content/','--enhanced','--seed','4242','--nosound','--noshroud','--w','1920','--h','1080','--vsync','0','--gfx',str(gfx),'--gfxsave',str(d/'session.cfg'),'--visualscfg',str(d/'visuals.cfg'),'--savedir',str(d/'saves'),'--script',str(script)]
  if ra:cmd+=['--ra3d','--dosdata','ra3d/audio/']
  env={**os.environ,'OPENCNC3D_BACKGROUND':'1','OPENCNC3D_VEHICLE_GLOW_SINGLE_PASS':str(mode)}
  for k in ['MESH_RIGID_ATTRIBUTES','SHORE_FILM_CACHE','WATER_ARRAY_CACHE','SHORE_FRAME_CACHE']:env['OPENCNC3D_'+k]='1'if win else'0'
  if verify:env['OPENCNC3D_QUERY_ELIDE']='verify';env['OPENCNC3D_FRAMEPROF']='1'
  with(d/'run.log').open('w')as log:run=subprocess.run(cmd,cwd=r,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
  text=(d/'run.log').read_text(errors='replace');assert run.returncode==0 and re.search(r'SCRIPT\|end .*0 failures',text),text[-1800:]
  deaths=re.findall(r'^EVHC\|death.*$',text,re.M);assert len(deaths)==12,(game,label,len(deaths))
  counters=re.findall(r'^VEHICLEGLOWSINGLEPASS\|enabled=(\d+)\|skipped=(\d+)\|sharing=(\d+)',text,re.M)
  if label!='original':
   assert counters and all(int(v[2])==1 for v in counters),(game,label,'Scope leaked',counters)
   enabled,skipped,_=map(int,counters[-1]);assert enabled==mode and (skipped>0)==(mode>0),(game,label,counters[-1])
  checks=re.findall(r'^GLQUERYVERIFY\|checked=(\d+)\|mismatches=(\d+)',text,re.M)
  if verify:assert checks and int(checks[-1][0])>100 and all(int(v[1])==0 for v in checks)and'GLQUERYVERIFY|mismatch|'not in text,checks[-1:]
  hashes=[hashlib.sha256(v.read_bytes()).hexdigest()for v in shots];pictures.append(hashes)
  row=dict(game=game,label=label,passed=(pictures[0]==hashes),mode=mode,pictures=hashes,counters=counters[-1:],deaths=len(deaths),query_checks=checks[-1:],command=cmd,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),script_sha256=hashlib.sha256(script.read_bytes()).hexdigest());rows.append(row);(out/'partial.json').write_text(json.dumps(rows,indent=2));print(json.dumps({k:row[k]for k in ['game','label','passed','counters','deaths','query_checks']}),flush=True)
  assert pictures[0]==hashes,(game,label,'Pictures differ')
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
