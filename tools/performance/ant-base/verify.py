#!/usr/bin/env python3
"""Sequential hidden pixel/float-target and timing controls for both renderers."""
import argparse,csv,gzip,hashlib,json,os,re,statistics,subprocess,tempfile,shutil
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--suite',choices=['quick','visual','performance','live'],default='quick');p.add_argument('--case',action='append');p.add_argument('--width',type=int);p.add_argument('--height',type=int);p.add_argument('--enable',action='append');p.add_argument('--env',action='append',default=[],metavar='NAME=VALUE');a=p.parse_args()
a.binary=a.binary.resolve();a.runtime=a.runtime.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
CONTROLS=['OPENCNC3D_DAMAGED_FRAGMENT_BATCH','OPENCNC3D_DAMAGED_ARRAY_SCOPE','OPENCNC3D_BODY_PASS_CULL','OPENCNC3D_WAVE_RANGE_SKIP','OPENCNC3D_AUX_ARRAY_SCOPE','OPENCNC3D_FRAGMENT_ACTIVE_REUSE']
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
configs={g:(a.runtime/f).read_bytes() for g,f in [('ra','ra3d-fx.cfg'),('td','opencnc3d-fx.cfg')]}
for g,b in configs.items():(a.output/(g+'.cfg')).write_bytes(b)
fixtures=Path(__file__).resolve().parents[1]/'active-fragments/fixtures'
CASES={'ant':('ra','SCA01EA','cam 67 65\ndist 4200\n'), 'td-first':('td','SCG01EA','dist 4200\n'), 'td-base':('td','SCG05EA','cam 52 12\ndist 3400\n'), 'ra-snow':('ra','SCU02EA','cam 37.5 66\ndist 2200\n'), 'td-fragments':('td','SCG94EA','cam 12 16\ndist 2700\n'), 'ant-fog':('ra','SCA01EA','cam 67 65\ndist 4200\n'), 'td-fog':('td','SCG01EA','dist 4200\n')}
def run(case,arm,iteration=0,perf=False):
 game,scen,camera=CASES[case];ra=game=='ra';d=a.output/(case+'-'+arm+('-'+str(iteration) if iteration else ''));d.mkdir(exist_ok=True)
 setup=f'gfxload {a.output}/{game}.cfg\ngfx show_stats 0\ntick 60\nreveal 1\n'+camera
 if case.endswith('-fog'):setup=setup.replace('reveal 1','reveal 0')
 if case=='ant-fog':setup=f'gfxload {a.output}/{game}.cfg\ngfx show_stats 0\ntick 3\ncam 43 87\ndist 4200\nlclickobj JEEP 0\ncam 52 76\ndist 7000\norder 54 65\ntick 600\n'+camera
 if case=='td-fragments':setup=f'gfxload {a.output}/{game}.cfg\ngfx show_stats 0\ncheatset fog 0\ntick 3\n'+camera+'hurtcell 15 15\nhurtcell 15 15\ntick 1\n'
 def cap(tag):
  text=''
  for name,mode in [('off',0),('on',1),('off-repeat',0)]:
   folder=d/name;folder.mkdir(exist_ok=True)
   text+=f'echo CAPTURE|{tag}|{name}\nantperf {mode}\nframes 3\nshot {folder}/{tag}.png\nrenderdump {folder}/{tag}\nobjdump\nphysicsdump\n'
  return text
 if perf=='live':
  script=setup+'frames 60\nlivebench 5\nmeshperfdump\nquit\n'
 elif perf:
  script=setup+'framebench 20\nfprofreport\necho STEP|active\n'+('tick 1\ntickalpha 0.375\nframebench 1\n'*100)+'meshperfdump\nquit\n'
 else:
  script=setup+'tickalpha 0.375\nframes 3\n'+cap('base')
  if a.suite=='visual':
   script+='gfx rain_fx 1\ngfx rain_wet 0.7\n'+cap('wet')+'gfx rain_fx 0\ngfx rain_wet 0\ntick 20\ntickalpha 0.75\n'+cap('animated')+'dist 1400\n'+cap('zoom')+'dist 6000\n'+cap('wide')+'cam 50 50\n'+cap('edge')
   if case in ['ant','td-base']:
    target='POWR:Spain' if ra else 'SILO:BadGuy'
    script+=f'hurtobj {target} 0\ntick 8\n'+camera+cap('damage')+'tick 65\ntickalpha 0.25\n'+cap('settled')
  script+='meshperfdump\nquit\n'
 (d/'script.txt').write_text(script)
 args=[str(a.binary),'--scen',scen,'--pack',('ra3d/3d/' if ra else '')+scen+'.pack','--dylib','ra3d/RedAlert.dylib' if ra else 'TiberianDawn.dylib','--content','ra3d/data' if ra else 'content/','--dir','ra3d/missions/' if ra else 'missions/','--dosdata','ra3d/audio' if ra else 'dosdata','--build','98','--seed','12345','--enhanced','--nosound','--w',str(a.width or (3008 if perf else 1280)),'--h',str(a.height or (1692 if perf else 720)),'--gfx',str(a.output/(game+'.cfg')),'--gfxsave',str(d/'private.cfg'),'--savedir',str(d/'saves'),'--script',str(d/'script.txt')]+(['--ra3d'] if ra else [])
 if case=='td-fragments':
  fixture=Path(tempfile.mkdtemp(prefix='oc3d-ant-',dir='/private/tmp'))
  for ext in ['INI','BIN']:shutil.copy2(fixtures/('SCG94EA.'+ext),fixture/('SCG94EA.'+ext))
  (fixture/'ebld').symlink_to(a.runtime/'ebld',target_is_directory=True)
  args[args.index('--pack')+1]='SCB01EA.pack';args[args.index('--dir')+1]=str(fixture)+'/'
  args+=['--models3d',str(fixture)+'/']
 env={k:v for k,v in os.environ.items() if not k.startswith('OPENCNC3D_')};env['OPENCNC3D_BACKGROUND']='1'
 for k in CONTROLS:env[k]='1' if arm!='off' and (not a.enable or k in a.enable) else '0'
 for setting in a.env:
  key,value=setting.split('=',1);assert key.startswith('OPENCNC3D_MESH_');env[key]=value
 if perf:env.update(OPENCNC3D_FRAMEPROF='1',OPENCNC3D_PERFDETAIL='1')
 if perf=='live':env['OPENCNC3D_PERFTRACE']=str(d/'trace')
 with (d/'run.log').open('w') as log:r=subprocess.run(args,cwd=a.runtime,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240)
 log=(d/'run.log').read_text(errors='replace');assert r.returncode==0 and re.search(r'SCRIPT\|end .*: \d+ lines, 0 failures',log),(case,arm,log[-1600:])
 row={'case':case,'arm':arm,'command':args,'controls':{k:v for k,v in env.items() if k in CONTROLS or k.startswith('OPENCNC3D_MESH_')},'binary_sha256':digest(a.binary)}
 vals=[float(x) for x in re.findall(r'FRAMEBENCH\|n=1\|median=([0-9.]+)',log)]
 if perf=='live':
  traces=list(d.glob('trace*-mission-*.csv'));assert len(traces)==1,traces
  data=list(csv.DictReader(l for l in traces[0].read_text().splitlines() if not l.startswith('#')))[60:]
  vals=[float(r['total_ms']) for r in data];assert len(vals)>=60
 if vals:row.update(median_ms=statistics.median(vals),p95_ms=sorted(vals)[int(.95*len(vals))],p99_ms=sorted(vals)[min(len(vals)-1,int(.99*len(vals)))],samples=len(vals))
 (d/'receipt.json').write_text(json.dumps(row,indent=2)+'\n');print(case,arm,row.get('median_ms','captured'),flush=True)
 return d,row
reports=[]
for case in (a.case or (['ant','td-first'] if a.suite=='quick' else CASES)):
 if a.suite in ['performance','live']:
  rows=[run(case,arm,i,'live' if a.suite=='live' else True)[1] for i,arm in enumerate(['off','on','on','off'])]
  before=statistics.mean(r['median_ms'] for r in rows if r['arm']=='off');after=statistics.mean(r['median_ms'] for r in rows if r['arm']=='on')
  reports.append({'case':case,'before_ms':before,'after_ms':after,'improvement_percent':100*(before-after)/before,'runs':rows})
 else:
  pair,_=run(case,'pair');off=pair/'off';on=pair/'on';comparisons=[]
  for f in sorted(off.glob('*.png')):
   x=Image.open(f).convert('RGBA');y=Image.open(on/f.name).convert('RGBA');comparisons.append({'file':f.name,'exact':x.size==y.size and x.tobytes()==y.tobytes()})
  for f in sorted(off.glob('*.bin.gz')):
   original=gzip.decompress(f.read_bytes());comparisons.append({'file':f.name,'exact':original==gzip.decompress((on/f.name).read_bytes()),'reference_repeat_exact':original==gzip.decompress((pair/'off-repeat'/f.name).read_bytes())})
  def state(d):return [s for s in (d/'run.log').read_text().splitlines() if s.startswith(('OBJ|','HOUSE|','PHYSICSCAPTURE|','BUILDINGPHYSICSCAPTURE|'))]
  groups={};active=None
  for line in (pair/'run.log').read_text().splitlines():
   if line.startswith('ECHO|CAPTURE|'):
    _,_,tag,name=line.split('|');active=(tag,name);groups[active]=[]
   elif active and line.startswith(('OBJ|','HOUSE|','PHYSICSCAPTURE|','BUILDINGPHYSICSCAPTURE|')):groups[active].append(line)
  same=bool(groups) and all(groups[(tag,'off')]==groups[(tag,'on')]==groups[(tag,'off-repeat')] for tag,name in groups if name=='off')
  report={'case':case,'comparisons':comparisons,'state_exact':same,'passed':bool(comparisons) and all(x['exact'] and x.get('reference_repeat_exact',True) for x in comparisons) and same};reports.append(report)
  print('PARITY',case,report['passed'],'mismatches',[x['file'] for x in comparisons if not x['exact']],flush=True)
 (a.output/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
for g,f in [('ra','ra3d-fx.cfg'),('td','opencnc3d-fx.cfg')]:assert (a.runtime/f).read_bytes()==configs[g]
if a.suite not in ['performance','live']:assert all(x['passed'] for x in reports),'Pixel/target/state mismatch'
