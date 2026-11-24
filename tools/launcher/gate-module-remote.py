#!/usr/bin/env python3
"""CPU-only native module route over real loopback HTTP, no game/window."""
import hashlib,json,re,stat,subprocess,threading,zipfile,importlib.util,os,shutil
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
import argparse
p=argparse.ArgumentParser(description=__doc__);p.add_argument('gate',type=Path);p.add_argument('scratch',type=Path);p.add_argument('--future-gate',type=Path);p.add_argument('--launcher',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];work=a.scratch;work.mkdir(parents=True,exist_ok=False)
version=re.search(r'#define LCFG_BUILD "([^"]+)"',(root/'launcher/lcfg.h').read_text())[1]
release_version=(root/'VERSION').read_text().strip()
menus=([Path(os.environ['OPENCNC3D_RA_SOURCE'])/'dosmenu.pack'] if os.environ.get('OPENCNC3D_RA_SOURCE') else [])+[root/'ra3d/dosmenu.pack',root/'playable/ra3d/dosmenu.pack',Path.home()/'CNC3D-latest/playable/ra3d/dosmenu.pack',Path.home()/'CNC3D/playable/ra3d/dosmenu.pack'];menu=next(x for x in menus if x.is_file()).read_bytes()
(work/'dosmenu.pack').write_bytes(menu)
for rel in ('savegame.sav','ra3d-fx.cfg','ra3d/missions/custom-map.ini'):
 f=work/rel;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('KEEP EXACT\n')
needs=re.findall(r'"(ra3d/[^\"]+)"',(root/'launcher/lmodule.c').read_text().split('lmod_ra_needs[] = {',1)[1].split('};',1)[0]);needs=[n for n in needs if not n.endswith(('.exe','.dll'))]
ra={n:(menu if n=='ra3d/dosmenu.pack' else ('fixture '+n).encode()) for n in needs};ra.update({'ra3d/einf/E1.c3m':b'Enhanced infantry','ra3d/3d/ant/ANT.c3m':b'Giant Ant model','ra3d/ebld/manifest.json':b'{}','efx.pack':b'shared effects','einf/SHARED.c3m':b'shared infantry'})
ra.update({'ra3d/expansions.txt':b'counterstrike SCG20EA Fixture\n','ra3d/ants.txt':b'ants SCA01EA Fixture\n','ra3d/expansions-manifest.json':b'{}'})
td={'opencnc3d':b'frontend','TiberianDawn.dylib':b'brain','CONQUER.MIX':b'data','einf/SHARED.c3m':b'shared infantry'}
spec=importlib.util.spec_from_file_location('module_producer',root/'tools/launcher/package-modules.py');producer=importlib.util.module_from_spec(spec);spec.loader.exec_module(producer)
def package(code,files,extra_manifest=''):
 if not extra_manifest:
  staged=work/f'{code}-stage';staged.mkdir(exist_ok=True);inputs={}
  for rel,data in files.items():
   path=staged/rel;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);path.chmod(0o755);inputs[rel]=path
  output=work/f'producer-{code}.zip';producer.write_archive(inputs,output,code,'macos',release_version)
  # The CPU gate uses the exact launcher build string, including dev suffixes.
  # Release identity is replaced below solely for an untagged development gate.
  if release_version==version:return output.read_bytes()
 path=work/f'{code}-{len(files)}-{bool(extra_manifest)}.zip'
 manifest=f'module-files 1\nmodule {code}\nversion {version}\nplatform macos\n'+''.join(f'file {hashlib.sha256(data).hexdigest()} {len(data)} {name}\n' for name,data in files.items())+extra_manifest
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
  for name,data in dict(files,**{f'opencnc3d-module-{code}.txt':manifest.encode()}).items():
   zi=zipfile.ZipInfo(name);zi.external_attr=(stat.S_IFREG|0o755)<<16;zi.compress_type=zipfile.ZIP_DEFLATED;z.writestr(zi,data)
 return path.read_bytes()
assets={2:package('td',td),3:package('ra',ra),8:package('ra',ra,f'file {"0"*64} 0 ra3d-fx.cfg\n')}
(work/'bad-meta.txt').write_text(f'{len(assets[8])} {hashlib.sha256(assets[8]).hexdigest()}\n')
catalog='modules 1\nversion '+version+'\n'+''.join(f'module {code} macos OpenCNC3D-module-{code}-macos-v{version}.zip {len(assets[i])} {hashlib.sha256(assets[i]).hexdigest()}\n' for code,i in [('td',2),('ra',3)])
assets[9]=package('ra',ra,f'file {"0"*64} 0 ra3d/einf/PHANTOM.c3m\n')
(work/'phantom-meta.txt').write_text(f'{len(assets[9])} {hashlib.sha256(assets[9]).hexdigest()}\n')
assets[1]=catalog.encode();builds=json.dumps({'ok':True,'latest':{'tag':'v'+version,'assets':[{'id':1,'name':f'OpenCNC3D-v{version}-modules.txt','size':len(assets[1])}]+[{'id':i,'name':f'OpenCNC3D-module-{code}-macos-v{version}.zip','size':len(assets[i])} for code,i in [('td',2),('ra',3)]]}}).encode()
(work/'builds.json').write_bytes(builds)
# The RA package as a file outside the launcher's download folder, for the installer routes.
(work/'ra-module.zip').write_bytes(assets[3])
future_version='0.99.1'
def future_package(code,files,number):
 stage=work/f'future-{code}-{number}';stage.mkdir(exist_ok=True);inputs={}
 for rel,data in files.items():
  path=stage/rel;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);path.chmod(0o755);inputs[rel]=path
 output=work/f'future-{code}-{number}.zip';producer.write_archive(inputs,output,code,'macos',number);return output.read_bytes()
future_files={'td':dict(td,**{'TiberianDawn.dylib':b'FUTURE TD BRAIN'}),'ra':dict(ra,**{'ra3d/RedAlert.dylib':b'FUTURE RA BRAIN'})}
assets[32]=future_package('td',future_files['td'],future_version);assets[33]=future_package('ra',future_files['ra'],future_version)
future_catalog='modules 1\nversion '+future_version+'\n'+''.join(f'module {code} macos OpenCNC3D-module-{code}-macos-v{future_version}.zip {len(assets[i])} {hashlib.sha256(assets[i]).hexdigest()}\n' for code,i in [('td',32),('ra',33)])
assets[31]=future_catalog.encode()
future_builds=json.dumps({'ok':True,'latest':{'tag':'v0.99.3','assets':[{'id':134,'name':'OpenCNC3D-v0.99.3-modules.txt','size':100},{'id':135,'name':'OpenCNC3D-module-ra-windows-v0.99.3.zip','size':100}]},'releases':[
 {'tag':'v0.99.9-preview','prerelease':True,'assets':[{'id':136,'name':'OpenCNC3D-v0.99.9-preview-modules.txt','size':100}]},
 {'tag':'v0.99.2','prerelease':True,'assets':[{'id':137,'name':'OpenCNC3D-v0.99.2-modules.txt','size':100},{'id':138,'name':'OpenCNC3D-module-ra-macos-v0.99.2.zip','size':100}]},
 {'tag':'v0.99.2','draft':True,'assets':[{'id':137,'name':'OpenCNC3D-v0.99.2-modules.txt','size':100},{'id':138,'name':'OpenCNC3D-module-ra-macos-v0.99.2.zip','size':100}]},
 {'tag':'v'+future_version,'prerelease':True,'channel':'experimental','assets':[{'id':31,'name':f'OpenCNC3D-v{future_version}-modules.txt','size':len(assets[31])}]+[{'id':i,'name':f'OpenCNC3D-module-{code}-macos-v{future_version}.zip','size':len(assets[i])} for code,i in [('td',32),('ra',33)]]}]}).encode()
requests=[]
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  requests.append((self.path,self.headers.get('Range')))
  if self.path=='/future/api/builds':data=future_builds
  elif self.path=='/disc':data=bytes(range(256))*32
  elif self.path=='/api/builds':data=builds
  elif self.path.startswith(('/api/download?asset=','/future/api/download?asset=')):data=assets[int(self.path.rsplit('=',1)[1])]
  else:self.send_error(404);return
  r=self.headers.get('Range');status=200;start=0;end=len(data)-1
  if r:
   m=re.fullmatch(r'bytes=(\d+)-(\d+)',r);assert m;start,end=map(int,m.groups());status=206
  self.send_response(status)
  if r:self.send_header('Content-Range',f'bytes {start}-{end}/{len(data)}')
  body=data[start:end+1];self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
media=f'OCMEDIA 1\nmodule ra3d\nmirror GDI http://127.0.0.1:{server.server_port}/disc 0\n'
body=bytes(range(256))*4;sha=hashlib.sha256(body).hexdigest()
for component,rel in [('movies','dosdata/movies/fixture.VQA'),('music','dosdata/SCORES.MIX'),('movies','ra3d/movies/fixture.VQA'),('music','ra3d/audio/SCORES.MIX')]:
 media+=f'file {component} {rel} 1024 {sha} GDI 0\n'
(work/'media.txt').write_text(media)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 result=subprocess.run([str(a.gate.resolve()),str(work.resolve()),f'http://127.0.0.1:{server.server_port}',str(work/'media.txt')],text=True,capture_output=True);print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
 assert 'LBROWSER|i=1|name=RA3D|installed=0|status=Install|grey=0|enabled=1' in result.stdout
 assert 'LBROWSER|i=1|name=RA3D|installed=1|status=Update|grey=0|enabled=1' in result.stdout
 assert result.stdout.count('LCHK|label=DOWNLOAD MOVIES|checked=1')>=2
 assert result.stdout.count('LCHK|label=DOWNLOAD MUSIC|checked=1')>=2
 assert result.stdout.count('LAPPLY|movies=1|music=1|asks=0|game=1|enabled=1')>=2
 if a.future_gate:
  for name,has_td,has_ra,target,installed,private in [('td-only',1,0,0,'0.0.1',False),('ra-only',0,1,1,'0.0.1',False),('both-td',1,1,0,'0.0.1',False),('both-ra',1,1,1,'0.0.1',False),('empty',0,0,-1,'0.0.1',False),('current',1,1,-1,future_version,False),('newer',1,1,-1,'0.100.0',False),('private',1,1,-1,'0.0.1',True)]:
   folder=work/name;folder.mkdir()
   for code,present,files in [('td',has_td,td),('ra',has_ra,ra)]:
    if present:
     import io
     with zipfile.ZipFile(io.BytesIO(future_package(code,files,'0.0.0' if private else installed))) as z:z.extractall(folder)
   (folder/'opencnc3d-install.txt').write_text('version '+installed+'\nmodule_install 1\n'+('private_developer 1\n' if private else ''))
   (folder/'dosmenu.pack').write_bytes(menu)
   if a.launcher:
    presentation=Path.home()/'CNC3D/playable/enhanced-menu.pack'
    if presentation.is_file():shutil.copyfile(presentation,folder/'enhanced-menu.pack')
   protected={'savegame.sav':b'KEEP SAVE EXACT','ra3d-fx.cfg':b'KEEP SETTINGS EXACT','opencnc3d-install.txt':(folder/'opencnc3d-install.txt').read_bytes()}
   for rel,data in protected.items():(folder/rel).write_bytes(data)
   records=[rel for rel in ['opencnc3d-module-td.txt','opencnc3d-module-ra.txt','TiberianDawn.dylib','ra3d/RedAlert.dylib'] if (folder/rel).is_file()]
   before={rel:(folder/rel).read_bytes() for rel in records};request_start=len(requests)
   def launcher_shot(tag,label,disabled):
    if not a.launcher:return
    args=[str(a.launcher.resolve()),'--dir',str(folder.resolve()),'--module','ra3d' if target==1 else 'opencnc3d','--shot',str(folder/(tag+'.png')),'--view','modules','--hidden-ui-test',str(folder),'--signal-time','0']
    if not private:args+=['--module-site',f'http://127.0.0.1:{server.server_port}/future']
    shot=subprocess.run(args,text=True,capture_output=True,timeout=30);(folder/(tag+'.log')).write_text(shot.stdout+shot.stderr)
    assert shot.returncode==0,(name,shot.stderr)
    expected_version=future_version if tag=='after' and target>=0 else installed
    assert 'LINSTALLED|module='+str(1 if target==1 or (has_ra and not has_td) else 0)+'|version='+expected_version in shot.stdout,(name,tag,shot.stdout)
    assert re.search(r'LBTN\|i=0\|label='+label+r'\|[^\n]*\|disabled='+str(disabled)+r'\|',shot.stdout),(name,shot.stdout[-2000:])
    # While an update is on offer the installed build can still be played: a second Launch
    # button stands beside Update, and only then.
    launch=re.search(r'LBTN\|i=(\d+)\|label=Launch\|[^\n]*\|disabled=0\|hidden=0',shot.stdout)
    offered=(tag=='before' and target>=0 and not private)
    assert bool(launch and launch.group(1)!='0')==offered,(name,tag,'Launch beside Update',shot.stdout[-2000:])
   launcher_shot('before','Update' if target>=0 else 'Launch',0 if has_td or has_ra else 1)
   result=subprocess.run([str(a.future_gate.resolve()),str(folder.resolve()),f'http://127.0.0.1:{server.server_port}/future',str(work/'media.txt'),str(target)],text=True,capture_output=True);print(name,result.stdout,end='');print(result.stderr,end='');result.check_returncode()
   for rel,data in protected.items():assert (folder/rel).read_bytes()==data
   other=1-target if target>=0 else -1
   for rel,data in before.items():
    is_ra=rel.startswith('ra3d/') or rel=='opencnc3d-module-ra.txt'
    if target<0 or int(is_ra)==other:assert (folder/rel).read_bytes()==data,(name,rel)
   assert (folder/'TiberianDawn.dylib').exists()==bool(has_td)
   assert (folder/'ra3d/RedAlert.dylib').exists()==bool(has_ra)
   downloads=[url for url,_ in requests[request_start:] if '/download?' in url and not url.endswith('asset=31')]
   assert all(url.endswith('asset='+str(32+target)) for url in downloads),(name,downloads)
   if target>=0:
    brain='ra3d/RedAlert.dylib' if target else 'TiberianDawn.dylib';assert (folder/brain).read_bytes()==future_files['ra' if target else 'td'][brain]
   else:assert not downloads,(name,downloads)
   if private:assert len(requests)==request_start
   launcher_shot('after','Launch',0 if has_td or has_ra else 1)
  print('FUTURE-MODULE-MATRIX|PASS|older-launcher-newer-catalog=1|td-only=1|ra-only=1|both-selections=1|empty=1|current=1|no-downgrade=1|private=1|only-selected-installed-download=1')
finally:server.shutdown()
for rel in ('savegame.sav','ra3d-fx.cfg','ra3d/missions/custom-map.ini'):assert (work/rel).read_text()=='KEEP EXACT\n'
assert any(r for _,r in requests);print('MODULE-REMOTE-HTTP|PASS|requests',len(requests))
