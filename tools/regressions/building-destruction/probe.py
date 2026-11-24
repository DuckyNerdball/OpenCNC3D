"""Measure verified building/vehicle kills; hidden, isolated maps and settings."""
import argparse,configparser,hashlib,importlib.util,json,os,re,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--helper',type=Path,default=Path(__file__).resolve().parents[1]/'top20/probe.py');p.add_argument('--case',action='append');p.add_argument('--exe',type=Path);a=p.parse_args()
r=a.runtime.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
sp=importlib.util.spec_from_file_location('fixture',a.helper);helper=importlib.util.module_from_spec(sp);sp.loader.exec_module(helper)
records=[]
for ra in (False,True):
 power='POWR' if ra else 'NUKE';medium='1TNK' if ra else 'MTNK';heavy='4TNK' if ra else 'HTNK'
 cases=[('power',power,False,1),('refinery','PROC',False,1),('yard','FACT',False,1),('tank',medium,True,1),('heavy-tank',heavy,True,1),('vehicle-burst',medium,True,12),('building-burst',power,False,12)]
 for label,code,vehicle,n in cases:
  tag=('ra-' if ra else 'td-')+label
  if a.case and tag not in a.case:continue
  d=out/tag;d.mkdir(exist_ok=True)
  f=helper.fixture(d,ra,'controls');ini=configparser.ConfigParser(strict=False,interpolation=None);ini.optionxform=str;ini.read(f/'SCG88EA.INI')
  good='Greece' if ra else 'GoodGuy';stride=128 if ra else 64
  if n==12:
   cells=[(43+(i%4)*4,43+(i//4)*4) for i in range(12)]
   if vehicle:ini['Units'].clear()
   for i,(x,y) in enumerate(cells):
    if vehicle:ini['Units'][str(i)]=f'{good},{code},256,{y*stride+x},0,Guard,None'
    else:ini['Structures'][str(3+i)]=f'{good},{code},256,{y*stride+x},0,None'
   before=n if vehicle else n+1;after=0 if vehicle else 1;camera=(49,47)
  else:
   if vehicle:
    ini['Units']['0']=ini['Units']['0'].replace(medium,code);cells=[(40,45)];before=1;after=0
   else:
    if code=='PROC':ini['Structures']['1']=ini['Structures']['1'].replace(power,code)
    cells=[(36,36)] if code=='FACT' else [(40,36)];before=2 if code=='FACT' else 1;after=before-1
   camera=(cells[0][0],cells[0][1])
  with (f/'SCG88EA.INI').open('w') as file:ini.write(file,space_around_delimiters=False)
  lines=['gfxmode 2','gfx show_stats 0','tick 5','reveal 1',f'cam {camera[0]} {camera[1]}','dist 2800',f'count {code} {before}','frames 6','echo BEFORE','fprofreport','livebench 3 play','fprofreport','echo DESTROY']
  for x,y in cells:lines += [f'hurtcell {x} {y}']*(16 if ra else 8)
  lines += ['tick 1','objdump','livebench 5 play',f'count {code} {after}','fprofreport','meshperfdump','physicsdump','shot '+str(d/'destroyed.png'),'tick 300','frames 3','echo RECOVERED','fprofreport','livebench 3 play','fprofreport',f'count {code} {after}','quit']
  if label=='building-burst':
   lines=lines[:-1]+['echo BEFORE_SLOT_REUSE','physicsdump','cam 40 36','frames 3','hurtcell 40 36']+[('hurtcell 40 36')]*(15 if ra else 7)+['tick 1','livebench 0.25 play','tick 90','count '+code+' 0','physicsdump','quit']
  script=d/'script.txt';script.write_text('\n'.join(lines)+'\n')
  engine='ra3d/RedAlert.dll' if ra else 'TiberianDawn.dll'
  if os.name!='nt':engine='ra3d/RedAlert.dylib' if ra else 'TiberianDawn.dylib'
  exe=a.exe.resolve() if a.exe else r/('cnc_eyes.exe' if os.name=='nt' else 'cnc_eyes')
  preset=r/('performance-ra.cfg' if ra else 'performance-td.cfg')
  if not preset.exists():preset=r/('ra3d-fx.cfg' if ra else 'opencnc3d-fx.cfg')
  cmd=[str(exe),'--scen','SCG88EA','--pack','ra3d/3d/SCG01EA.pack' if ra else 'SCG01EA.pack','--dir',str(f)+'/', '--dylib',engine,'--content','ra3d/data/' if ra else 'content/','--enhanced','--seed','4242','--nosound','--noshroud','--w','1920','--h','1080','--vsync','0','--gfx',str(preset),'--gfxsave',str(d/'session.cfg'),'--visualscfg',str(d/'visuals.cfg'),'--savedir',str(d/'saves'),'--script',str(script)]
  if ra:cmd+=['--ra3d','--dosdata','ra3d/audio/']
  start=time.monotonic()
  with (d/'run.log').open('w') as log:
   try:proc=subprocess.run(cmd,cwd=r,env={**os.environ,'OPENCNC3D_BACKGROUND':'1','OPENCNC3D_FRAMEPROF':'1','OPENCNC3D_GPUPROF':'1','OPENCNC3D_PERFDETAIL':'1'},stdout=log,stderr=subprocess.STDOUT,timeout=180);rc=proc.returncode
   except subprocess.TimeoutExpired:rc=124
  s=(d/'run.log').read_text(errors='replace');loops=re.findall(r'^LIVEBENCH\|requested=.*$',s,re.M)
  phases={}
  for phase,nextphase in [('BEFORE','DESTROY'),('DESTROY','RECOVERED'),('RECOVERED',None)]:
   part=s.split('ECHO|'+phase,1)[-1]
   if nextphase:part=part.split('ECHO|'+nextphase,1)[0]
   else:part=part.split('ECHO|BEFORE_SLOT_REUSE',1)[0]
   rows=re.findall(r'^FRAMEPROF\|frames=(\d+)\|mean=([\d.]+)\|worst=([\d.]+)',part,re.M)
   # The first report flushes setup/warmup counters. Use the final real-loop window.
   phases[phase]=dict(frames=int(rows[-1][0]),mean_ms=float(rows[-1][1]),worst_ms=float(rows[-1][2])) if rows else None
  row=dict(case=tag,exit=rc,script_ok=bool(re.search(r'SCRIPT\|end .*0 failures',s)),seconds=time.monotonic()-start,target=code,targets=n,expected_remaining=after,launch_controls={k:v for k,v in os.environ.items() if k.startswith('OPENCNC3D_')},phases=phases,loops=loops,deaths=re.findall(r'^(?:EVHC\|death|EBLD\|wreck).*$',s,re.M),renderer=re.findall(r'^GL_RENDERER\s*=\s*(.*)',s,re.M),command=cmd,executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
  budget=[dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',line)) for line in re.findall(r'^DESTRUCTIONCAP\|state\|.*$',s,re.M)]
  first=s.split('ECHO|BEFORE_SLOT_REUSE',1)[0]
  row['building_budget']=budget
  vehicle_budget=[dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',line)) for line in re.findall(r'^VEHICLEDESTRUCTIONCAP\|state\|.*$',s,re.M)]
  row['vehicle_budget']=vehicle_budget
  if label=='building-burst':
   row['cap_ok']=len(budget)==3 and budget[0]==dict(active=2,limit=2,admitted=2,effects_only=10,released=0,high_water=2) and budget[1]==dict(active=0,limit=2,admitted=2,effects_only=10,released=2,high_water=2) and budget[2]['admitted']==3 and budget[2]['effects_only']==10 and budget[2]['high_water']==2 and len(re.findall(r'^EBLD\|boom\|',first,re.M))==12 and len(re.findall(r'^EBLD\|wreck\|.*\|effects_only=1$',first,re.M))==10
  elif vehicle:
   row['cap_ok']=all(x['admitted']==0 and x['effects_only']==0 for x in budget) and len(re.findall(r'^EVHC\|death',s,re.M))==n
   row['cap_ok']=row['cap_ok'] and bool(vehicle_budget) and all(x['active']<=2 and x['high_water']==min(n,2) and x['admitted']==min(n,2) and x['effects_only']==max(0,n-2) for x in vehicle_budget)
  else:row['cap_ok']=all(x['high_water']<=1 and x['effects_only']==0 for x in budget)
  row['verified']=row['cap_ok'] and rc==0 and row['script_ok'] and len(loops)==(4 if label=='building-burst' else 3) and all('result=0' in x and 'taa_sync=0' in x for x in loops) and all(phases.values()) and bool(row['deaths'])
  records.append(row);(out/'results.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps({k:row[k] for k in ['case','verified','exit','script_ok','phases']}),flush=True)
raise SystemExit(0 if all(x['verified'] for x in records) else 1)
