#!/usr/bin/env python3
"""Hidden two-game vehicle destruction admission, viewport and unchanged-effects proof.

Private maps/settings/saves only. Each platform must run this independently.
A small event keeps exact images; an overflow deliberately changes physics alone.
"""
import argparse,configparser,hashlib,importlib.util,json,os,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for key in ['runtime','exe','engine-root','out']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--helper',type=Path,default=Path(__file__).resolve().parents[1]/'top20/probe.py')
p.add_argument('--game',choices=['td','ra','both'],default='both');p.add_argument('--only-defaults',action='store_true');a=p.parse_args()
r=a.runtime.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
sp=importlib.util.spec_from_file_location('fixture',a.helper.resolve());h=importlib.util.module_from_spec(sp);sp.loader.exec_module(h)
rows=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for game in (['td','ra']if a.game=='both'else[a.game]):
 ra=game=='ra';code='4TNK'if ra else'HTNK';stride=128 if ra else 64;owner='Greece'if ra else'GoodGuy'
 cases=[(2,'small',0),(2,'small',1),(2,'crowd',0),(2,'crowd',1),(1,'crowd',1),(0,'small',0),(0,'small',1),(2,'crowd',None)]
 if a.only_defaults:cases=[(2,'crowd',None)]
 for gfx,kind,cap in cases:
  label=f'{game}-{gfx}-{kind}-{cap}';d=out/label;d.mkdir();f=h.fixture(d,ra,'controls')
  ini=configparser.ConfigParser(strict=False,interpolation=None);ini.optionxform=str;ini.read(f/'SCG88EA.INI');ini['Units'].clear();ini['Infantry'].clear()
  cells=[(48,48),(52,48)]if kind=='small'else[(12,12),(16,12),(12,16),(16,16),(48,48),(52,48),(48,52),(52,52)]
  for i,(x,y)in enumerate(cells):ini['Units'][str(i)]=f'{owner},{code},256,{y*stride+x},0,Guard,None'
  with(f/'SCG88EA.INI').open('w')as file:ini.write(file,space_around_delimiters=False)
  lines=[f'gfxmode {gfx}','gfx aa_mode 0','gfx show_stats 0','tick 5','reveal 1','cam 51 51','dist 1500','frames 3',f'count {code} {len(cells)}'];shots=[]
  def sample(name,delta=0):
   if delta:lines.append(f'tick {delta}')
   image=d/(name+'.png');shots.append(image)
   lines.extend(['frames 3','echo SAMPLE_'+name,'physicsdump','evhcdump','efxdump','decaldump','shot '+str(image)])
  def kill(indices):
   for i in indices:
    x,y=cells[i];lines.extend([f'hurtcell {x} {y}']*(16 if ra else 8))
   lines.append('tick 1')
  if kind=='small':
   kill([0,1]);sample('burst');sample('flight',15);sample('settled',100);sample('gone',600)
  else:
   kill([0,1]);sample('offscreen');kill([4,5,6,7]);sample('local');lines.append('cam 15 15');sample('pan');kill([2,3]);sample('new-pair');lines.append('cam 51 51');sample('pan-back');sample('gone',600)
  lines+=[f'count {code} 0','remission',f'gfxmode {gfx}','tick 1','frames 3','echo SAMPLE_restart','physicsdump','quit']
  script=d/'script.txt';script.write_text('\n'.join(lines)+'\n');win=os.name=='nt'
  engine=a.engine_root.resolve()/(('ra3d/RedAlert'if ra else'TiberianDawn')+('.dll'if win else'.dylib'))
  preset=r/(('performance-ra.cfg'if ra else'performance-td.cfg')if win else'ra3d-fx.cfg'if ra else'opencnc3d-fx.cfg')
  cmd=[str(a.exe.resolve()),'--scen','SCG88EA','--pack','ra3d/3d/SCG01EA.pack'if ra else'SCG01EA.pack','--dir',str(f)+'/', '--dylib',str(engine),'--content','ra3d/data/'if ra else'content/','--enhanced','--seed','4242','--nosound','--noshroud','--w','1280','--h','800','--vsync','0','--gfx',str(preset),'--gfxsave',str(d/'session.cfg'),'--visualscfg',str(d/'visuals.cfg'),'--savedir',str(d/'saves'),'--script',str(script)]
  if ra:cmd+=['--ra3d','--dosdata','ra3d/audio/']
  env={**os.environ,'OPENCNC3D_BACKGROUND':'1','OPENCNC3D_QUERY_ELIDE':'verify'}
  if cap is None:env.pop('OPENCNC3D_VEHICLE_DESTRUCTION_VIEW',None)
  else:env['OPENCNC3D_VEHICLE_DESTRUCTION_VIEW']=str(cap)
  with(d/'run.log').open('w')as log:run=subprocess.run(cmd,cwd=r,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
  text=(d/'run.log').read_text(errors='replace');assert run.returncode==0 and re.search(r'SCRIPT\|end .*0 failures',text),(label,text[-2200:])
  states={};effects={};decals={}
  for section in text.split('ECHO|SAMPLE_')[1:]:
   name=section.splitlines()[0];m=re.search(r'^VEHICLEDESTRUCTIONCAP\|state\|.*$',section,re.M);assert m,(label,name)
   states[name]={k:int(v)for k,v in re.findall(r'(\w+)=(\d+)',m[0])}
   effects[name]=re.findall(r'^EFXPART\|pool=.*$',section,re.M)[:1]
   decals[name]=re.findall(r'^DECALENH\|.*$',section,re.M)[:1]
  assert states and all(v['active']<=2 and v['high_water']<=2 for v in states.values()),states
  assert states['gone']['active']==states['restart']['active']==0,states
  assert 'GLQUERYVERIFY|mismatch|'not in text,label
  if gfx==2 and cap!=0:
   expected={'burst':2,'flight':2,'gone':0}if kind=='small'else{'offscreen':0,'local':2,'pan':0,'new-pair':2,'pan-back':0,'gone':0}
   assert all(states[k]['active']==v for k,v in expected.items()),(label,states,expected)
   if kind=='crowd':assert states['local']['effects_only']==4 and states['new-pair']['admitted']==4 and states['pan-back']['released']==4,states
  if cap==0:assert all(v['admitted']==0 for v in states.values()),states
  row=dict(game=game,gfx=gfx,kind=kind,cap=cap,passed=True,states=states,effects=effects,decals=decals,pictures=[sha(x)for x in shots],hashes={str(x):sha(x)for x in [a.exe.resolve(),engine,preset,script]},command=cmd)
  rows.append(row);(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(dict(game=game,gfx=gfx,kind=kind,cap=cap,passed=True,states=states)),flush=True)
 if a.only_defaults:continue
 for gfx in [0,2]:
  off=next(v for v in rows if v['game']==game and v['gfx']==gfx and v['kind']=='small'and not v['cap']);on=next(v for v in rows if v['game']==game and v['gfx']==gfx and v['kind']=='small'and v['cap'])
  assert off['pictures']==on['pictures'],(game,gfx,'Small-event pictures changed')
 off=next(v for v in rows if v['game']==game and v['gfx']==2 and v['kind']=='crowd'and not v['cap']);on=next(v for v in rows if v['game']==game and v['gfx']==2 and v['kind']=='crowd'and v['cap'])
 default=next(v for v in rows if v['game']==game and v['gfx']==2 and v['kind']=='crowd'and v['cap']is None)
 assert default['pictures']==on['pictures'] and default['states']==on['states'],(game,'Compiled vehicle policy differs from explicit enabled policy')
 for name in ['offscreen','local','pan','new-pair','pan-back']:
  assert off['effects'][name]==on['effects'][name],(game,name,'Native explosion particle emission changed')
  assert off['decals'][name]==on['decals'][name],(game,name,'Native scorch/crater drawing changed')
print('PASS|both-game small-event image identity, overflow, viewport release, reuse, lifetime, restart and preserved native effects',flush=True)
