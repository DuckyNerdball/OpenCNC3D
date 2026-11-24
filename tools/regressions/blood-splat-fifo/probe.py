"""Hidden native FIFO state/order equivalence under the existing ground-splat cap."""
from pathlib import Path
import argparse,configparser,hashlib,importlib.util,json,os,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);p.add_argument('--exe',required=True,type=Path);p.add_argument('--baseline',required=True,type=Path);p.add_argument('--helper',required=True,type=Path);p.add_argument('--out',required=True,type=Path);p.add_argument('--game',choices=['td','ra','both'],default='both');p.add_argument('--count',type=int,default=240);a=p.parse_args();r=a.runtime.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
sp=importlib.util.spec_from_file_location('fixture',a.helper.resolve());h=importlib.util.module_from_spec(sp);sp.loader.exec_module(h);rows=[]
for game in(['td','ra']if a.game=='both'else[a.game]):
 ra=game=='ra';owner='Greece'if ra else'GoodGuy';stride=128 if ra else 64;pictures=[];states=[]
 for label,on in [('original',False),('disabled',False),('enabled',True)]:
  d=out/game/label;d.mkdir(parents=True);f=h.fixture(d,ra,'controls');ini=configparser.ConfigParser(strict=False,interpolation=None);ini.optionxform=str;ini.read(f/'SCG88EA.INI');ini['Infantry'].clear();ini[owner]['MaxInfantry']='500'
  for i in range(a.count):ini['Infantry'][str(i)]=f'{owner},E1,256,{(12+(i//20)*2)*stride+12+(i%20)*2},0,Guard,0,None'
  with(f/'SCG88EA.INI').open('w')as file:ini.write(file,space_around_delimiters=False)
  lines=['gfxmode 2','gfx show_stats 0','tick 3','reveal 1','cam 31 24','dist 4200','frames 3',f'count E1 {a.count}']+[f'hurtobj E1 {i}'for i in range(a.count)];shots=[]
  for phase,delta in[('burst',1),('flight',15),('spray',30),('cap',30),('settled',60),('old',300)]:
   pic=d/(phase+'.png');shots.append(pic);lines += [f'tick {delta}','frames 3','m3dump','meshperfdump','shot '+str(pic)]
  lines+=['count E1 0','quit'];script=d/'script.txt';script.write_text('\n'.join(lines)+'\n');win=os.name=='nt';exe=(a.baseline if label=='original'else a.exe).resolve();engine=('ra3d/RedAlert'if ra else'TiberianDawn')+('.dll'if win else'.dylib');gfx=r/(('performance-ra.cfg'if ra else'performance-td.cfg')if win else'ra3d-fx.cfg'if ra else'opencnc3d-fx.cfg')
  cmd=[str(exe),'--scen','SCG88EA','--pack','ra3d/3d/SCG01EA.pack'if ra else'SCG01EA.pack','--dir',str(f)+'/', '--dylib',engine,'--content','ra3d/data/'if ra else'content/','--enhanced','--seed','4242','--nosound','--noshroud','--w','1280','--h','800','--vsync','0','--gfx',str(gfx),'--gfxsave',str(d/'session.cfg'),'--visualscfg',str(d/'visuals.cfg'),'--savedir',str(d/'saves'),'--script',str(script)]
  if ra:cmd+=['--ra3d','--dosdata','ra3d/audio/']
  env={**os.environ,'OPENCNC3D_BACKGROUND':'1','OPENCNC3D_BLOOD_SPLAT_FIFO':'1'if on else'0'}
  with(d/'run.log').open('w')as log:c=subprocess.run(cmd,cwd=r,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=360)
  text=(d/'run.log').read_text(errors='replace');assert c.returncode==0 and re.search(r'SCRIPT\|end .*0 failures',text),text[-2500:]
  hashes=[hashlib.sha256(v.read_bytes()).hexdigest()for v in shots];pictures.append(hashes)
  counters=re.findall(r'^BLOODSPLATFIFO\|enabled=\d+\|overwritten=(\d+)\|rotations=(\d+)\|state=([a-f0-9]+)\|splats=(\d+)\|drops=(\d+)\|objects=(\d+)',text,re.M)
  if label!='original':
   assert len(counters)==6,counters
   assert (int(counters[-1][0])>0)==on,counters[-1]
   states.append([v[2:]for v in counters]);assert not on or states[0]==states[-1],(game,'Effect state changes',states)
  row=dict(game=game,label=label,passed=(pictures[0]==hashes),count=a.count,pictures=hashes,counters=counters,command=cmd,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest());rows.append(row);(out/'partial.json').write_text(json.dumps(rows,indent=2));print(json.dumps(dict(game=game,label=label,passed=True,counters=counters[-1:])),flush=True)
  assert pictures[0]==hashes,(game,label,'Pictures differ')
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
