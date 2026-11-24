"""Native local-camera corpse/effect ownership tests; all settings/maps are private."""
import argparse,configparser,hashlib,importlib.util,json,os,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--exe',type=Path,required=True);p.add_argument('--helper',type=Path,default=Path(__file__).resolve().parents[1]/'top20/probe.py');p.add_argument('--out',type=Path,required=True);p.add_argument('--game',choices=['td','ra','both'],default='both');a=p.parse_args();r=a.runtime.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
sp=importlib.util.spec_from_file_location('fixture',a.helper);h=importlib.util.module_from_spec(sp);sp.loader.exec_module(h);rows=[]
for ra in [False,True]:
 if a.game!='both' and (a.game=='ra')!=ra:continue
 for label,count,enabled in [('small-on',12,True),('small-off',12,False),('crowded',80,True)]:
  d=out/(('ra-' if ra else 'td-')+label);d.mkdir(exist_ok=True);f=h.fixture(d,ra,'controls');ini=configparser.ConfigParser(strict=False,interpolation=None);ini.optionxform=str;ini.read(f/'SCG88EA.INI');good='Greece' if ra else 'GoodGuy';stride=128 if ra else 64;ini['Infantry'].clear()
  for i in range(count):
   base=(12,14) if i<40 else (48,49);j=i%40;x=base[0]+j%8;y=base[1]+j//8
   ini['Infantry'][str(i)]=f'{good},E1,256,{y*stride+x},0,Guard,0,None'
  with (f/'SCG88EA.INI').open('w') as file:ini.write(file,space_around_delimiters=False)
  lines=['gfxmode 2','gfx show_stats 0','tick 3','reveal 1','cam 16 16','dist 1200','frames 3']
  lines += [f'hurtobj E1 {i}' for i in range(count)]+['tick 120','frames 3','echo VIEW_A','m3dump','objdump','shot '+str(d/'a.png')]
  if count==80:
   lines+=['cam 52 51','frames 4','echo VIEW_B','m3dump','shot '+str(d/'b.png'),'cam 33 33','frames 4','echo VIEW_EMPTY','m3dump','shot '+str(d/'empty.png'),'cam 16 16','frames 4','echo VIEW_A_RETURN','m3dump','shot '+str(d/'a-return.png')]
  lines+=['quit'];script=d/'script.txt';script.write_text('\n'.join(lines)+'\n');win=os.name=='nt';engine=('ra3d/RedAlert' if ra else 'TiberianDawn')+('.dll' if win else '.dylib')
  preset=r/(('performance-ra.cfg' if ra else 'performance-td.cfg') if win else ('ra3d-fx.cfg' if ra else 'opencnc3d-fx.cfg'))
  cmd=[str(a.exe.resolve()),'--scen','SCG88EA','--pack','ra3d/3d/SCG01EA.pack' if ra else 'SCG01EA.pack','--dir',str(f)+'/', '--dylib',engine,'--content','ra3d/data/' if ra else 'content/','--enhanced','--seed','4242','--nosound','--noshroud','--w','1280','--h','800','--vsync','0','--gfx',str(preset),'--gfxsave',str(d/'session.cfg'),'--visualscfg',str(d/'visuals.cfg'),'--savedir',str(d/'saves'),'--script',str(script)]
  if ra:cmd+=['--ra3d','--dosdata','ra3d/audio/']
  with (d/'run.log').open('w') as log:c=subprocess.run(cmd,cwd=r,env={**os.environ,'OPENCNC3D_BACKGROUND':'1','OPENCNC3D_CORPSE_VIEW_LIMIT':'1' if enabled else '0'},stdout=log,stderr=subprocess.STDOUT,timeout=180)
  s=(d/'run.log').read_text(errors='replace');views={}
  for part in s.split('ECHO|VIEW_')[1:]:
   name=part.splitlines()[0];state=re.search(r'^CORPSEVIEW\|limit=.*$',part,re.M);body=re.search(r'^INFDEATH\|.*$',part,re.M)
   views[name]=dict(fields=dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',state[0])) if state else {},bodies=dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',body[0])) if body else {},owners=re.findall(r'^CORPSEVIEW\|owner=(-?\d+)',part,re.M))
  passed=c.returncode==0 and bool(re.search(r'SCRIPT\|end .*0 failures',s)) and bool(views)
  if label=='crowded':
   passed=passed and len(views)==4 and all(x['bodies'].get('corpses')==80 and x['bodies'].get('bodies')==80 for x in views.values()) and all(views[x]['fields'].get('selected')==20 and views[x]['fields'].get('drawn',99)<=20 for x in ['A','B','A_RETURN']) and views['EMPTY']['fields'].get('selected')==0 and views['A']['owners']==views['A_RETURN']['owners'] and not(set(views['A']['owners'])&set(views['B']['owners']))
  else:passed=passed and views['A']['bodies'].get('corpses')==12 and views['A']['fields'].get('selected')==12
  for view in views.values():
   fields=view['fields'];selected=fields.get('selected',999)
   passed=passed and fields.get('drawn',999)<=selected and fields.get('blood',999)<=selected and fields.get('ground_splats',999999)<=selected*90
   if not selected:passed=passed and all(fields.get(k,999)==0 for k in ['drawn','blood','ground_splats','drops','object_splats'])
  row=dict(game='ra' if ra else 'td',case=label,passed=passed,exit=c.returncode,views=views,sha256=hashlib.sha256(a.exe.read_bytes()).hexdigest());rows.append(row);(out/'results.json').write_text(json.dumps(rows,indent=2));print(json.dumps(row),flush=True)
for game in ['td','ra']:
 if a.game!='both' and a.game!=game:continue
 for first,second in [(out/(game+'-small-on/a.png'),out/(game+'-small-off/a.png')),(out/(game+'-crowded/a.png'),out/(game+'-crowded/a-return.png'))]:
  assert first.read_bytes()==second.read_bytes(),(first,second,'unexpected picture change')
raise SystemExit(0 if all(x['passed'] for x in rows) else 1)
