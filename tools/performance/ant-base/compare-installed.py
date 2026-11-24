#!/usr/bin/env python3
"""Compare candidate checkpoints to the pre-change installed renderer's pixels/state."""
from pathlib import Path
import argparse,json,os,subprocess,re
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);results=[]
for pair in sorted(a.evidence.glob('*-pair')):
 receipt=json.loads((pair/'receipt.json').read_text());out=a.output/pair.name;out.mkdir(exist_ok=True)
 for name in ['off','on','off-repeat']:(out/name).mkdir(exist_ok=True)
 script='\n'.join(line.replace(str(pair),str(out)) for line in (pair/'script.txt').read_text().splitlines() if not line.startswith('antperf '))+'\n';sp=out/'script.txt';sp.write_text(script)
 args=receipt['command'];args[0]=str(a.runtime/'cnc_eyes');args[args.index('--script')+1]=str(sp);args[args.index('--gfxsave')+1]=str(out/'private.cfg');args[args.index('--savedir')+1]=str(out/'saves')
 env={k:v for k,v in os.environ.items() if not k.startswith('OPENCNC3D_')};env['OPENCNC3D_BACKGROUND']='1'
 with (out/'run.log').open('w') as log:r=subprocess.run(args,cwd=a.runtime,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
 text=(out/'run.log').read_text(errors='replace');assert r.returncode==0 and re.search(r'SCRIPT\|end .*: \d+ lines, 0 failures',text),text[-1500:]
 comparisons=[]
 for target in sorted((pair/'on').glob('*.png')):
  x=Image.open(target).convert('RGBA');y=Image.open(out/'on'/target.name).convert('RGBA');comparisons.append({'name':target.name,'exact':x.size==y.size and x.tobytes()==y.tobytes()})
 def state(path):return [l for l in path.read_text().splitlines() if l.startswith(('OBJ|','HOUSE|','PHYSICSCAPTURE|','BUILDINGPHYSICSCAPTURE|'))]
 same=state(pair/'run.log')==state(out/'run.log');report={'case':receipt['case'],'pixels':comparisons,'state_exact':same,'passed':bool(comparisons) and all(x['exact'] for x in comparisons) and same};results.append(report);print('INSTALLED',receipt['case'],report['passed'],[x['name'] for x in comparisons if not x['exact']],flush=True);(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
assert results and all(x['passed'] for x in results)
