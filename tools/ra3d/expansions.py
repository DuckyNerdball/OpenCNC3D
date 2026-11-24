#!/usr/bin/env python3
"""Import and stage 16 Counterstrike, 18 Aftermath and four secret Ant missions.

prepare --source DIRECTORY extracts the exact ExpandNames roster from original
GENERAL.MIX archives under counterstrike/ and aftermath/. bake [--family NAME]
builds their existing RA3D terrain/autoheight scenes. stage --module DIRECTORY
requires all 38 original missions and their own scenes, then installs the catalog,
expansion rules, sprites and audio without replacing base-game source archives.
"""
from __future__ import annotations
import argparse,hashlib,importlib.util,json,re,shutil,struct,subprocess,sys,tempfile
from pathlib import Path
from ramix import RaMix
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
STORE=ROOT/'data/ra3d/expansions'

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def roster():
    source=(ROOT/'brain/vanilla/redalert/expand.cpp').read_text(encoding='cp437')
    body=source.split('const char* ExpandNames[] = {',1)[1].split('NULL};',1)[0]
    names=re.findall(r'"(SC[GU][0-9]{2}EA)"',body)
    if len(names)!=34 or len(set(names))!=34:raise ValueError('Original expansion roster changed')
    ants=re.findall(r'"(SCA[0-9]{2}EA)\.INI"',(ROOT/'brain/vanilla/redalert/mapsel.cpp').read_text(encoding='cp437'))
    if ants!=['SCA01EA','SCA02EA','SCA03EA','SCA04EA']:raise ValueError('Original Ant roster changed')
    return [(name,'counterstrike' if i<16 else 'aftermath') for i,name in enumerate(names)]+[(name,'ants') for name in ants]
def metadata():
    data=json.loads((STORE/'manifest.json').read_text())
    if [(m['code'],m['family']) for m in data['missions']]!=roster():raise ValueError('Expansion catalog differs from original roster')
    for m in data['missions']:
        if digest(STORE/'missions'/(m['code']+'.INI'))!=m['sha256']:raise ValueError('Original mission changed: '+m['code'])
    return data

def prepare(source):
    archives={family:RaMix(source/family/'GENERAL.MIX') for family in ('counterstrike','aftermath')}
    target=STORE/'missions';target.mkdir(parents=True,exist_ok=True)
    rows=[]
    for code,family in roster():
        raw=archives['counterstrike' if family=='ants' else family].read(code+'.INI')
        if raw is None:raise ValueError('Missing original mission: '+code)
        text=raw.decode('cp437');basic=text.split('[Basic]',1)[1].split('\n[',1)[0]
        name=re.search(r'^Name\s*=\s*(.+)',basic,re.M|re.I)
        if not name or '[MapPack]' not in text:raise ValueError('Incomplete original mission: '+code)
        path=target/(code+'.INI');path.write_bytes(raw)
        rows.append(dict(code=code,family=family,name=name[1].strip().replace('\r',''),sha256=digest(path)))
    data=dict(format=1,source=str(source.resolve().relative_to(ROOT)),roster_source='brain/vanilla/redalert/expand.cpp',missions=rows,
              archives={f:digest(source/f/'GENERAL.MIX') for f in archives})
    data['source_receipts']={family:json.loads((source/family/'SOURCE-RECEIPT.json').read_text()) for family in archives}
    for row in rows:
        scene=STORE/'3d'/(row['code']+'.pack')
        height=ROOT/'data/ra3d/autoheight'/('RA'+row['code'][2:]+'.HGT')
        if scene.is_file() and height.is_file():
            row['scene_sha256']=digest(scene);row['height_sha256']=digest(height)
    (STORE/'manifest.json').write_text(json.dumps(data,indent=2)+'\n')
    print('RAEXPANSIONS|original-missions=38|counterstrike=16|aftermath=18|ants=4')

def bake(family):
    data=metadata();rows=[m for m in data['missions'] if not family or m['family']==family]
    for m in rows:
        subprocess.run([sys.executable,str(HERE/'ra_heights.py'),m['code'],str(ROOT/'data/ra3d/autoheight'/('RA'+m['code'][2:]+'.HGT'))],check=True)
    subprocess.run([sys.executable,str(HERE/'bake-3d-mission.py'),str(STORE/'3d')]+[m['code'] for m in rows],check=True)
    for m in rows:
        m['height_sha256']=digest(ROOT/'data/ra3d/autoheight'/('RA'+m['code'][2:]+'.HGT'))
        m['scene_sha256']=digest(STORE/'3d'/(m['code']+'.pack'))
    data['height_recipe']='docs/auto-heightmap.md'
    data['generator_sha256']={str(p.relative_to(ROOT)):digest(p) for p in [HERE/'ra_heights.py',ROOT/'game/autoheight_mod.h',ROOT/'game/autoheight_tables.h']}
    (STORE/'manifest.json').write_text(json.dumps(data,indent=2)+'\n')

def merged_audio(base,additional,destination):
    entries={key:base.data[off:off+size] for key,(off,size) in base.index.items()}
    for archive in additional:
        for key,(off,size) in archive.index.items():entries.setdefault(key,archive.data[off:off+size])
    body=bytearray();index=bytearray()
    for key,value in sorted(entries.items()):index+=struct.pack('<III',key,len(body),len(value));body+=value
    destination.write_bytes(struct.pack('<HI',len(entries),len(body))+index+body)

def stage(module):
    data=metadata();source=ROOT/data['source'];module=module.resolve()
    for family,receipt in data['source_receipts'].items():
        for name,expected in receipt['files'].items():
            if digest(source/family/name)!=expected:raise ValueError('Original expansion archive changed: '+family+'/'+name)
    for m in data['missions']:
        scene=STORE/'3d'/(m['code']+'.pack')
        if not scene.is_file():raise ValueError('Missing 3D scene: '+m['code'])
        if m.get('scene_sha256') and digest(scene)!=m['scene_sha256']:raise ValueError('Expansion scene changed: '+m['code'])
        height=ROOT/'data/ra3d/autoheight'/('RA'+m['code'][2:]+'.HGT')
        if not height.is_file() or len(height.read_bytes())!=129*129 or digest(height)!=m.get('height_sha256'):raise ValueError('Missing or stale own heightmap: '+m['code'])
    (module/'missions').mkdir(parents=True,exist_ok=True);(module/'3d').mkdir(exist_ok=True)
    for m in data['missions']:
        for src,dst in [(STORE/'missions'/(m['code']+'.INI'),module/'missions'/(m['code']+'.INI')),(STORE/'3d'/(m['code']+'.pack'),module/'3d'/(m['code']+'.pack'))]:shutil.copy2(src,dst)
    catalog=''.join(f"{m['family']} {m['code']} {m['name']}\n" for m in data['missions'] if m['family']!='ants')
    (module/'expansions.txt').write_text(catalog,encoding='utf-8')
    (module/'ants.txt').write_text(''.join(f"{m['family']} {m['code']} {m['name']}\n" for m in data['missions'] if m['family']=='ants'),encoding='utf-8')
    (module/'expansions-manifest.json').write_text(json.dumps(data,indent=2)+'\n')
    for name in ['EXPAND.MIX','EXPAND2.MIX','HIRES1.MIX','LORES1.MIX']:
        src=next((source/family/name for family in ('aftermath','counterstrike') if (source/family/name).is_file()),None)
        if src:shutil.copy2(src,module/'data'/name)
    expand=RaMix(source/'aftermath/EXPAND2.MIX')
    rules=expand.read('AFTRMATH.INI')
    if rules is None:raise ValueError('Missing original Aftermath rules')
    (module/'missions/AFTRMATH.INI').write_bytes(rules)
    with tempfile.TemporaryDirectory(prefix='ra-expansion-sprites-') as temp:
        overlay=Path(temp)
        for src in (ROOT/'data/radata').glob('*.MIX'):(overlay/src.name).symlink_to(src)
        for name in ['EXPAND.MIX','EXPAND2.MIX','HIRES1.MIX','LORES1.MIX']:
            src=module/'data'/name
            if src.exists():(overlay/name).symlink_to(src)
        subprocess.run([sys.executable,str(HERE/'bake-classic-sprites.py'),'--data',str(overlay),'--out',str(module/'dosclassic.pack')],check=True)
    for name in ['SOUNDS.MIX','SCORES.MIX']:
        extras=[RaMix(source/family/name) for family in ('counterstrike','aftermath') if (source/family/name).is_file()]
        merged_audio(RaMix(ROOT/'data/radata'/name),extras,module/'audio'/name)
    print('RAEXPANSIONS|staged=38|own-scenes=38|ants=4|rules-and-sprites=original')

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='command',required=True)
    a=sub.add_parser('prepare');a.add_argument('--source',type=Path,required=True)
    a=sub.add_parser('bake');a.add_argument('--family',choices=['counterstrike','aftermath','ants'])
    a=sub.add_parser('stage');a.add_argument('--module',type=Path,required=True)
    args=p.parse_args()
    if args.command=='prepare':prepare(args.source)
    elif args.command=='bake':bake(args.family)
    else:stage(args.module)
if __name__=='__main__':main()
