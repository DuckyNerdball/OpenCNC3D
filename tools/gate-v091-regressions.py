#!/usr/bin/env python3
"""Hidden v0.9.1 cross-game MCV/build-unlock and session regression checks (macOS)."""
from pathlib import Path
import argparse,json,os,re,subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--install',type=Path,default=ROOT/'playable');p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.install=a.install.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
results={}
for game,sides,mapname,power in [('td',['gdi','nod'],'SCM01EA','NUKE'),('ra',['allied','soviet'],'SCM08EA','POWR')]:
 for side in sides:
  d=a.output/(game+'-'+side);d.mkdir(exist_ok=True)
  point='7 18' if game=='td' else '83 56'
  script=f'tick 5\nlclickobj MCV:@me\nactclickobj MCV:@me\ntick 150\nrahdump\nsbclickitem {power}\ntick 1200\nstartplace {power}\ncam {point}\nhover {point}\nlclickcell {point}\ntick 150\nobjdump\nrahdump\nquit\n'
  (d/'script.txt').write_text(script)
  cmd=[str(a.install/'cnc_eyes'),'--scen',mapname,'--pack',f'ra3d/3d/{mapname}.pack' if game=='ra' else mapname+'.pack','--skirmish','--side',side,'--seed','4242','--credits','10000','--ai','1','--nosound','--w','1280','--h','720','--savedir',str(d/'saves'),'--gfxsave',str(d/'settings.cfg'),'--gfx','ra3d-fx.cfg' if game=='ra' else 'opencnc3d-fx.cfg','--script',str(d/'script.txt')]
  if game=='ra':cmd+=['--ra3d','--dylib','ra3d/RedAlert.dylib','--dir','ra3d/missions/','--content','ra3d/data','--dosdata','ra3d/audio']
  else:cmd+=['--dylib','TiberianDawn.dylib','--dir','missions/','--content','content/']
  r=subprocess.run(cmd,cwd=a.install,env={**os.environ,'OPENCNC3D_BACKGROUND':'1'},stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);(d/'run.log').write_text(r.stdout)
  checks={'exit':r.returncode==0,'script':bool(re.search(r'SCRIPT\|end .*: \d+ lines, 0 failures',r.stdout)), 'power_unlocked':f'RAHUD|item|{power}|live=1' in r.stdout,'refinery_unlocked':'RAHUD|item|PROC|live=1' in r.stdout}
  results[game+'-'+side]=checks
  if not all(checks.values()):raise RuntimeError(str(d)+': '+str(checks))
for label,source,args in [('ra-session','ra-session-regression.c',[a.install/'ra3d/RedAlert.dylib',a.install/'ra3d/missions']),('td-session','td-session-regression.c',[a.install/'TiberianDawn.dylib',a.install/'content',a.install/'missions']),('ra-expansions','ra-expansion-startup.c',[a.install/'ra3d/RedAlert.dylib',a.install/'ra3d'])]:
 d=a.output/label;d.mkdir(exist_ok=True);exe=d/'check'
 subprocess.run(['cc','-std=c11','-O2',str(ROOT/'brain/host'/source),'-o',str(exe)],check=True)
 # Engine directory arguments require a terminal separator in the original ABI.
 argv=[str(v)+('/' if v.is_dir() else '') for v in args]
 r=subprocess.run([str(exe)]+argv,cwd=d,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);(d/'run.log').write_text(r.stdout)
 results[label]={'exit':r.returncode==0,'verdict':next((v for v in reversed(r.stdout.splitlines()) if '|PASS' in v),'')}
 if r.returncode or not results[label]['verdict']:raise RuntimeError(str(d)+r.stdout[-1200:])
(a.output/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
