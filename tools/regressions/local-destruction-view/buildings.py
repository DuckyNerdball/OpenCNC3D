import argparse,configparser,importlib.util,json,os,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--exe',type=Path,required=True);p.add_argument('--helper',type=Path,default=Path(__file__).resolve().parents[1]/'top20/probe.py');p.add_argument('--out',type=Path,required=True);a=p.parse_args();r=a.runtime.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('h',a.helper);h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h);rows=[]
for ra in [False,True]:
 d=out/('ra' if ra else 'td');d.mkdir(exist_ok=True);f=h.fixture(d,ra,'controls');ini=configparser.ConfigParser(strict=False,interpolation=None);ini.optionxform=str;ini.read(f/'SCG88EA.INI');owner='Greece' if ra else 'GoodGuy';stride=128 if ra else 64;power='POWR' if ra else 'NUKE';ini['Structures'].pop('1');cells=[(12,12),(16,12),(12,16),(16,16),(48,48),(52,48),(48,52),(52,52)]
 for i,(x,y) in enumerate(cells):ini['Structures'][str(i+3)]=f'{owner},{power},256,{y*stride+x},0,None'
 with (f/'SCG88EA.INI').open('w') as file:ini.write(file,space_around_delimiters=False)
 lines=['gfxmode 2','gfx show_stats 0','tick 3','reveal 1','cam 51 51','dist 1500','frames 3']
 def kill(indices):
  for i in indices:
   x,y=cells[i];lines.extend([f'hurtcell {x+1} {y+1}']*(16 if ra else 8))
  lines.extend(['tick 1','frames 3','physicsdump'])
 kill([0,1]);kill([4,5]);lines+=['cam 15 15','frames 3','physicsdump'];kill([2,3]);lines+=['cam 51 51','frames 3','physicsdump'];kill([6,7]);lines+=['shot '+str(d/'last.png'),'quit'];script=d/'script.txt';script.write_text('\n'.join(lines)+'\n');win=os.name=='nt';engine=('ra3d/RedAlert' if ra else 'TiberianDawn')+('.dll' if win else '.dylib');preset=r/(('performance-ra.cfg' if ra else 'performance-td.cfg') if win else ('ra3d-fx.cfg' if ra else 'opencnc3d-fx.cfg'))
 cmd=[str(a.exe.resolve()),'--scen','SCG88EA','--pack','ra3d/3d/SCG01EA.pack' if ra else 'SCG01EA.pack','--dir',str(f)+'/', '--dylib',engine,'--content','ra3d/data/' if ra else 'content/','--enhanced','--seed','4242','--nosound','--noshroud','--w','1280','--h','800','--gfx',str(preset),'--gfxsave',str(d/'session.cfg'),'--visualscfg',str(d/'visuals.cfg'),'--savedir',str(d/'saves'),'--script',str(script)]
 if ra:cmd+=['--ra3d','--dosdata','ra3d/audio/']
 with (d/'run.log').open('w') as log:c=subprocess.run(cmd,cwd=r,env={**os.environ,'OPENCNC3D_BACKGROUND':'1'},stdout=log,stderr=subprocess.STDOUT,timeout=180)
 s=(d/'run.log').read_text(errors='replace');states=[dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)',v)) for v in re.findall(r'^DESTRUCTIONCAP\|state\|.*$',s,re.M)]
 expected=[(0,0,2,0),(2,2,2,0),(0,2,2,2),(2,4,2,2),(0,4,2,4),(2,6,2,4)]
 passed=c.returncode==0 and bool(re.search(r'SCRIPT\|end .*0 failures',s)) and len(states)==6 and [(x['active'],x['admitted'],x['effects_only'],x['released']) for x in states]==expected and max(x['high_water'] for x in states)==2
 row=dict(game='ra' if ra else 'td',passed=passed,states=states);rows.append(row);print(json.dumps(row),flush=True);(out/'results.json').write_text(json.dumps(rows,indent=2))
assert all(x['passed'] for x in rows)
