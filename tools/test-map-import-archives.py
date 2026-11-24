#!/usr/bin/env python3
"""Exercise real ZIP, 7Z and unsafe bundle inputs through the native backend."""
from pathlib import Path
import argparse, json, subprocess, zipfile, stat, hashlib
p=argparse.ArgumentParser();p.add_argument('binary',type=Path);p.add_argument('source',type=Path);p.add_argument('evidence',type=Path);a=p.parse_args()
binary=a.binary.resolve();root=a.source.resolve();qa=a.evidence.resolve();qa.mkdir(parents=True,exist_ok=True)
ini=(root/'playable/missions/SCM01EA.INI').read_bytes();terrain=(root/'playable/missions/SCM01EA.BIN').read_bytes()
checks=[]
def run(action,path,expected):
 result=subprocess.run([str(binary),action,str(path),str(expected)],capture_output=True,text=True)
 checks.append(dict(action=action,path=str(path),exit=result.returncode,output=result.stdout+result.stderr))
 assert result.returncode==0,checks[-1]
def archive(name,entries):
 path=qa/name
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
  for k,v in entries:z.writestr(k,v)
 return path
valid=archive('one.zip',[('nested/Map.INI',ini),('nested/Map.BIN',terrain),('enhanced-menu.pack',b'never install'),('RULES.INI',b'[General]\n')])
run('inspect',valid,1)
seven_dir=qa/'seven-input';seven_dir.mkdir(exist_ok=True);(seven_dir/'Map.INI').write_bytes(ini);(seven_dir/'Map.BIN').write_bytes(terrain)
seven=qa/'one.7z';subprocess.run(['/usr/bin/bsdtar','--format=7zip','-cf',str(seven),'-C',str(seven_dir),'Map.INI','Map.BIN'],check=True)
run('inspect',seven,1)
for name,entries,message in [
 ('traversal.zip',[('../outside.INI',ini)],'unsafe path'),
 ('absolute.zip',[('/outside.INI',ini)],'unsafe path'),
 ('windows-absolute.zip',[('C:\\outside.INI',ini)],'unsafe path'),
 ('reserved.zip',[('CON.ini',ini)],'unsafe path'),
 ('duplicate.zip',[('Map.INI',ini),('map.ini',ini)],'duplicate'),
 ('large-map.zip',[('Map.INI',b'x'*(4*1024*1024+1))],'exceeds 4 MB'),
 ('not-maps.zip',[('enhanced-menu.pack',b'untrusted'),('RULES.INI',b'[General]\n')],'No C&C'),
 ('bad-pair.zip',[('Map.INI',ini)],'matching .BIN')]:run('reject',archive(name,entries),message)
link=zipfile.ZipInfo('symlink.INI');link.create_system=3;link.external_attr=(stat.S_IFLNK|0o777)<<16
path=qa/'symlink.zip'
with zipfile.ZipFile(path,'w') as z:z.writestr(link,'/tmp/target')
run('reject',path,'unsafe path')
path=qa/'empty.7z';path.write_bytes(b'7z\xbc\xaf\x27\x1c'+bytes(30));run('reject',path,'')
path=qa/'corrupt.7z';path.write_bytes(b'not an archive');run('reject',path,'Cannot open')
# A sidecar with no INI is recoverable only when it describes a native MP layout.
recovery=qa/'recovery';recovery.mkdir(exist_ok=True);(recovery/'Lost.BIN').write_bytes(terrain)
sidecar=dict(MapTileX=1,MapTileY=1,MapTileWidth=62,MapTileHeight=62,Theater='TEMPERATE',Waypoints=[650,3000])
(recovery/'Lost.JSON').write_text(json.dumps(sidecar))
r=subprocess.run([str(binary),'run',str(recovery/'Lost.BIN'),str(qa/'td'),str(qa/'ra')],capture_output=True,text=True);assert r.returncode==0,r.stderr
checks.append(dict(action='recovery',output=r.stdout))
assert len(list((qa/'td').glob('*.source.json')))==1
assert not list((qa/'td').glob('*.source.ini'))
sidecar['Theater']='WINTER\n[Basic]';(recovery/'Lost.JSON').write_text(json.dumps(sidecar));run('reject',recovery/'Lost.BIN','unsupported theater')
sidecar['Theater']='TEMPERATE'
sidecar['Waypoints']=[650];(recovery/'Lost.JSON').write_text(json.dumps(sidecar));run('reject',recovery/'Lost.BIN','missing singleplayer')
# Exercise writes and repeat import: IDs/files are stable and private assets ignored.
for path in [valid,seven]:
 result=subprocess.run([str(binary),'run',str(path),str(qa/'td'),str(qa/'ra')],capture_output=True,text=True);assert result.returncode==0,result.stderr
before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (qa/'td').iterdir()}
result=subprocess.run([str(binary),'run',str(valid),str(qa/'td'),str(qa/'ra')],capture_output=True,text=True);assert result.returncode==0,result.stderr
assert before=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (qa/'td').iterdir()}
assert not (qa/'td/enhanced-menu.pack').exists() and not (qa/'td/RULES.INI').exists()
(qa/'result.json').write_text(json.dumps(dict(passed=True,checks=checks),indent=2)+'\n');print('PASS archive/recovery edge checks',len(checks))
