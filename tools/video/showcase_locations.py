#!/usr/bin/env python3
"""Persist faction/theater draws and prepare private original-map cinematic missions."""
from pathlib import Path
import argparse,hashlib,json,random,secrets,re,shutil,plistlib
from urllib.parse import unquote,urlparse
POLICY=Path(__file__).with_name('showcase-location-policy.json')
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def resolve_playable():
 dock=plistlib.loads((Path.home()/'Library/Preferences/com.apple.dock.plist').read_bytes())
 paths=[Path(unquote(urlparse(t.get('tile-data',{}).get('file-data',{}).get('_CFURLString','')).path)).parent for t in dock['persistent-apps']if t.get('tile-data',{}).get('file-label')in ['OpenCNC3D','OpenCNC3D']]
 paths=[p for p in paths if(p/'cnc_eyes').is_file()and(p/'TiberianDawn.dylib').is_file()];assert len(set(paths))==1,paths;return paths[0]
def choose(subjects,seed,history=None):
 policy=json.loads(POLICY.read_text());rng=random.Random(seed);state=history if history is not None else {};used=state.setdefault('used_in_cycle',{});last=state.setdefault('last',{});result={}
 def pick(key):
  pool=policy['pools'][key];ids={r['id']for r in pool};seen=used.setdefault(key,[])
  available=[r for r in pool if r['id']not in seen]
  if not available:seen.clear();available=list(pool)
  if len(available)>1:available=[r for r in available if r['id']!=last.get(key)]
  row=rng.choice(available);seen.append(row['id']);last[key]=row['id'];return dict(row)
 for subject,faction in subjects:
  if subject in result:raise ValueError('Subject identifiers must be unique')
  if faction not in policy['order']:raise ValueError('Faction must be GDI or Nod')
  first,final=policy['order'][faction];result[subject]={'starting_faction':faction,'opening':pick(first),'ending':pick(final)}
 return result

def prepare(location,destination,playable=None):
 """Keep original scenery; strip the mission director before the first native tick.
 Original moving actors are omitted, so guard/hunt/harvest behavior cannot survive
 removal of triggers. Only the film's later explicit actor list may be introduced.
 """
 P=Path(playable)if playable else resolve_playable();D=Path(destination).resolve()
 if D.exists():raise FileExistsError(D)
 if P.resolve()==D or P.resolve()in D.parents:raise ValueError('Private film copy cannot be inside the game installation')
 D.mkdir(parents=True);mission=location['mission'];source=P/'missions'/f'{mission}.INI';raw=source.read_text(errors='strict');parts=re.split(r'(?mi)^\[([^\]]+)\]\s*\n',raw)
 houses={'GOODGUY','BADGUY','NEUTRAL','SPECIAL',*[f'MULTI{x}'for x in range(1,7)]};remove={'TRIGGERS','CELLTRIGGERS','TEAMTYPES','TEAMS','UNITS','INFANTRY','AIRCRAFT','VESSELS'};out=[];removed={};preserved={};player='GoodGuy'if location['faction']=='GDI'else'BadGuy'
 for i in range(1,len(parts),2):
  name,body=parts[i:i+2];key=name.upper()
  if key in remove:removed[key]=sum('='in l and not l.startswith(';')for l in body.splitlines());body=''
  elif key=='BASE':body='Count=0\n'
  elif key in houses:
   body=re.sub(r'(?mi)^Credits=.*$', 'Credits=0',body)
   if not re.search(r'(?mi)^Credits=',body):body+='\nCredits=0\n'
  elif key=='BASIC':
   for k,v in {'Player':player,'BuildLevel':'98','CarryOverMoney':'0','CarryOverCap':'0','Intro':'x','Action':'x','Brief':'x','Win':'x','Lose':'x'}.items():
    body=re.sub(r'(?mi)^'+k+'=.*$',k+'='+v,body)if re.search(r'(?mi)^'+k+'=',body)else body+'\n'+k+'='+v+'\n'
  if key in {'MAP','STRUCTURES','OVERLAY','TERRAIN','SMUDGE'}:preserved[key]=hashlib.sha256(body.encode()).hexdigest()
  out.append('['+name+']\n'+body.rstrip()+'\n')
 if not any(parts[i].upper()=='BASE'for i in range(1,len(parts),2)):out.append('[Base]\nCount=0\n')
 private=D/source.name;private.write_text('\n'.join(out));srcbin=source.with_suffix('.BIN');shutil.copy2(srcbin,D/srcbin.name);assert sha(srcbin)==sha(D/srcbin.name)
 # A capture still uses the original mission pack, never a substitute flat map.
 pack=P/f'{mission}.pack';assert pack.is_file();manifest={'location':location,'player':player,'status':'PREPARED_REQUIRES_NATIVE_IDLE_AND_ENCOUNTER_PREFLIGHT','mission_ai_director':'Original triggers, teams, base queue, credits and mobile actors removed before boot','removed_original_entries':removed,'preserved_section_sha256':preserved,'source_ini':str(source),'source_ini_sha256':sha(source),'source_bin_sha256':sha(srcbin),'native_pack':str(pack),'native_pack_sha256':sha(pack),'private_ini_sha256':sha(private),'runtime':str(P/'cnc_eyes'),'runtime_sha256':sha(P/'cnc_eyes'),'engine_sha256':sha(P/'TiberianDawn.dylib'),'encounter_actor_whitelist':[],'encounter_defense_whitelist':[],'no_installed_file_writes':True}
 (D/'mission-isolation.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest

def main():
 p=argparse.ArgumentParser();sub=p.add_subparsers(dest='command',required=True);q=sub.add_parser('select');q.add_argument('plan',type=Path);q.add_argument('subjects',nargs='+',help='SUBJECT:GDI or SUBJECT:Nod');q.add_argument('--seed',type=int);q=sub.add_parser('prepare');q.add_argument('location_id');q.add_argument('destination',type=Path);a=p.parse_args();policy=json.loads(POLICY.read_text())
 if a.command=='select':
  subjects=[tuple(s.rsplit(':',1))for s in a.subjects]
  if a.plan.exists():
   saved=json.loads(a.plan.read_text());assert saved['subjects']==[list(s)for s in subjects],'Existing film selection differs; use a fresh plan path';print(json.dumps(saved['selection'],indent=2));return
  import fcntl,datetime
  workspace=POLICY.parents[2];production_root=next((p for p in a.plan.resolve().parents if p.name=='vehicle-showcase'),None);hp=production_root/'location-history.json'if production_root else workspace/policy['history'];hp.parent.mkdir(parents=True,exist_ok=True)
  with hp.with_suffix('.lock').open('a')as lock:
   fcntl.flock(lock,fcntl.LOCK_EX)
   state=json.loads(hp.read_text())if hp.exists()else{}
   seed=a.seed if a.seed is not None else secrets.randbits(64);before=json.loads(json.dumps(state));selection=choose(subjects,seed,state)
   saved={'seed':seed,'subjects':subjects,'policy_sha256':sha(POLICY),'selection':selection,'policy':policy,'history_before':before,'history_after':state}
   a.plan.parent.mkdir(parents=True,exist_ok=True);a.plan.write_text(json.dumps(saved,indent=2)+'\n')
   state.setdefault('draws',[]).append({'plan':str(a.plan.resolve()),'seed':seed,'selection':selection,'created':datetime.datetime.now(datetime.timezone.utc).isoformat()});hp.write_text(json.dumps(state,indent=2)+'\n')
  print(json.dumps(saved['selection'],indent=2))
 else:
  pool=[r for rows in policy['pools'].values()for r in rows];location=next(c for c in pool if c['id']==a.location_id);print(json.dumps(prepare(location,a.destination),indent=2))
if __name__=='__main__':main()
