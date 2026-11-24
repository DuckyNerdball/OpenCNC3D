#!/usr/bin/env python3
"""Install four personal effects galleries. User maps are excluded by all public packagers."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('runtime',type=Path);a=p.parse_args()
r=a.runtime.resolve()
if 'private_developer 1' not in (r/'opencnc3d-install.txt').read_text():raise SystemExit('Private developer installation required')
folder=r/'missions/user_maps';folder.mkdir(exist_ok=True,parents=True)
rows=[]
for code,name,engine,theater in [('USER96','Temperate','TEMPERATE','TEMPERAT'),('USER97','Winter','WINTER','WINTER'),('USER98','Snow','WINTER','SNOW'),('USER99','Desert','DESERT','DESERT')]:
 ini=f"""; Private developer effects gallery. Excluded from public packages.
[Basic]
Name=Effects - {name}
Player=GoodGuy
BuildLevel=15
CNC3DKind=Single
Intro=x
Brief=x
Action=x
Win=x
Lose=x
[Map]
Theater={engine}
OpenCNC3DTheater={theater}
X=5
Y=5
Width=54
Height=54
[GoodGuy]
Credits=100000
Allies=GoodGuy,BadGuy
[BadGuy]
Credits=0
Allies=GoodGuy,BadGuy
[Waypoints]
26=2080
[Units]
0=GoodGuy,MCV,256,2080,0,Guard,None
1=BadGuy,MCV,256,3642,0,Guard,None
[EffectsLab]
Enabled=1
"""
 target=folder/(code+'.INI')
 if target.exists() and '[EffectsLab]' not in target.read_text():raise SystemExit('Reserved user-map slot occupied: '+str(target))
 target.write_text(ini)
 # An empty clear1 ground and its own level 65x65 corners. No donor heights survive reskin.
 (folder/(code+'.BIN')).write_bytes(bytes([255,0])*4096)
 (folder/(code+'.HGT')).write_bytes(bytes(65*65))
 rows.append(dict(code=code,name='Effects - '+name,own_height_sha256=hashlib.sha256(bytes(65*65)).hexdigest()))
print(json.dumps(dict(private=True,maps=rows),indent=2))
