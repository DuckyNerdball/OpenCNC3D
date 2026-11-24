#!/usr/bin/env python3
"""Hidden native temporary-reveal regression; isolated maps, saves and preferences."""
from pathlib import Path
import argparse,subprocess,os,re,json
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--classic',action='store_true');a=p.parse_args();r=a.runtime.resolve();s=a.source.resolve();out=a.out.resolve();out.mkdir();rows=[];q=out
import importlib.util,configparser
spec=importlib.util.spec_from_file_location('fixture_helper',Path(__file__).resolve().parents[1]/'top20/probe.py')
helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
for red in [False,True]:
 folder=helper.fixture(q/'fog-fixtures',red,'temporary-reveal');ini=folder/'SCG88EA.INI';c=configparser.ConfigParser(interpolation=None,strict=False);c.optionxform=str;c.read(ini)
 good,bad=('Greece','USSR')if red else('GoodGuy','BadGuy');stride=128 if red else 64
 c['Structures'].clear();c['Structures']['0']=f'{good},FACT,256,{12*stride+12},0,None';c['Structures']['1']=f'{good},{"POWR"if red else"NUKE"},256,{35*stride+35},0,None'
 c['Units'].clear();c['Units']['0']=f'{bad},{"V2RL"if red else"MSAM"},256,{35*stride+41},0,Sleep,None';c['Infantry'].clear()
 with ini.open('w')as f:c.write(f,space_around_delimiters=False)
for ra in [False,True]:
 d=out/('ra'if ra else'td');d.mkdir();(d/'saves').mkdir();enemy='V2RL'if ra else'MSAM';fixture=q/'fog-fixtures'/('ra-'if ra else'td-')/'temporary-reveal';brain=(s/('RedAlert.dll' if ra else 'TiberianDawn.dll')) if os.name=='nt' else s/'brain/vanilla/build-native'/('redalert/RedAlert.dylib'if ra else'tiberiandawn/TiberianDawn.dylib')
 lines=['gfxmode '+('0'if a.classic else'2'),'gfx show_stats 0','tick 1','cam 39 35','firevisdump','shroudat 41 35','forcefire '+enemy+' 0','tick 1','frames 2','firevisdump','shroudat 41 35','tick 20','forcefire '+enemy+' 0','tick 1','firevisdump','save 0 temporary visibility','tick 30','frames 2','firevisdump','tick 7','frames 2','firevisdump','tick 7','frames 2','firevisdump','tick 16','frames 2','firevisdump','shroudat 41 35','load 0','frames 2','firevisdump','shroudat 41 35','spawnobj E1 41 36','tick 1','forcefire '+enemy+' 0','tick 1','firevisdump','shroudat 41 35','tick 60','frames 2','firevisdump','shroudat 41 35','shot '+str(d/'after.png'),'quit'];script=d/'script.txt';script.write_text('\n'.join(lines)+'\n')
 preset=d/'input.cfg';text=(r/(('performance-ra.cfg'if ra else'performance-td.cfg')if os.name=='nt'else('ra3d-fx.cfg'if ra else'opencnc3d-fx.cfg'))).read_text();text=re.sub(r'(?m)^vsync\s+.*','vsync 0',text);preset.write_text(text)
 cmd=[str(s/'cnc_eyes.exe'if os.name=='nt'else s/'game/cnc_eyes'),'--scen','SCG88EA','--pack','ra3d/3d/SCG01EA.pack'if ra else'SCG01EA.pack','--dir',str(fixture)+'/', '--content','ra3d/data'if ra else'content','--dylib',str(brain),'--build','98','--nosound','--w','1280','--h','720','--gfx',str(preset),'--gfxsave',str(d/'session.cfg'),'--visualscfg',str(d/'visuals.cfg'),'--savedir',str(d/'saves'),'--script',str(script)]
 if ra:cmd+=['--ra3d','--dosdata','ra3d/audio/']
 with(d/'run.log').open('w')as f:rc=subprocess.run(cmd,cwd=r,env={**os.environ,'SDL_MAC_BACKGROUND_APP':'1','OPENCNC3D_BACKGROUND':'1'},stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
 text=(d/'run.log').read_text(errors='replace');states=re.findall(r'^SHROUDAT\|41,35\|(.*)',text,re.M);assert rc==0 and states==['HIDDEN','CLEAR','HIDDEN','HIDDEN','CLEAR','CLEAR'],(rc,states);assert re.search(r'LOAD\|slot=0\|frame=',text);assert re.search(r'FIREFADER\|.*alpha=0\.5333',text);assert re.search(r'FIREVIS\|frame=67\|active=0',text);rows.append(dict(game='ra'if ra else'td',rc=rc,states=re.findall(r'^FIREVIS\|frame=.*',text,re.M),shroud=re.findall(r'^SHROUDAT.*',text,re.M),fires=re.findall(r'^FORCEFIRE.*',text,re.M),script=re.findall(r'^SCRIPT\|end.*',text,re.M)));print(rows[-1],flush=True);(out/'results.json').write_text(json.dumps(rows,indent=2))
