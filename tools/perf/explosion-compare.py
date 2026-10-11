#!/usr/bin/env python3
"""Preserve geometry, draw order and physical timing across flight, depth priming, fade and cleanup. Frozen-simulation GPU-finished timings are not live FPS."""
import os,subprocess,re,json,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
import argparse
parser=argparse.ArgumentParser(description="Hidden A/B performance and pixel regression for a 465-fragment NUK2 death. Run sequentially with no concurrent compiler or renderer.")
parser.add_argument('--runtime',type=Path,required=True)
parser.add_argument('--baseline',type=Path,required=True)
parser.add_argument('--candidate',type=Path)
parser.add_argument('--gfx',type=Path)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
OUT=args.output.resolve(); OUT.mkdir(parents=True,exist_ok=True)
RUN=args.runtime.resolve()
BASE=args.baseline.resolve()
NEW=args.candidate.resolve() if args.candidate else RUN/'cnc_eyes'
GFX=args.gfx.resolve() if args.gfx else RUN/'opencnc3d-fx.cfg' 
script='gfx enabled 1\ngfx dos_classic 0\ngfx ss_scale 1\ngfx show_stats 0\ntick 30\ncam 50 12\ndist 3000\n'
phases=[('intact',''),('burst','hurtobj NUK2\ntick 1\n'),('flight','tick 15\n'),('settling','tick 25\n'),('sink','tick 20\n'),('late','tick 60\n'),('gone','tick 500\n')]
metrics={'fixture':{'scenario':'SCB20EA','camera':[50,12,3000],'size':[2560,1440],'frames_per_phase':50,'gfx_sha256':hashlib.sha256(GFX.read_bytes()).hexdigest(),'baseline_sha256':hashlib.sha256(BASE.read_bytes()).hexdigest(),'candidate_sha256':hashlib.sha256(NEW.read_bytes()).hexdigest()}}
for label,exe,env in [('old',BASE,{}),('new',NEW,{}),('disabled',NEW,{'OPENCNC3D_MESH_FRAGMENT_BATCH':'0'})]:
    lines=script
    for phase,extra in phases:
        lines+=extra+'echo PHASE '+phase+'\nframebench 50\n'+f'shot {OUT/(label+"-"+phase+".png")}\nmeshperfdump\n'
    lines+='quit\n'
    path=OUT/(label+'.txt');path.write_text(lines)
    cmd=[str(exe),'--scen','SCB20EA','--pack','SCB20EA.pack','--dylib','TiberianDawn.dylib','--dir','missions/','--content','content/','--nosound','--w','2560','--h','1440','--enhanced','--gfx',str(GFX),'--noshroud','--script',str(path)]
    with (OUT/(label+'.log')).open('w') as log:
        subprocess.run(cmd,cwd=RUN,env={**os.environ,**env},stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
    text=(OUT/(label+'.log')).read_text()
    assert re.search(r'SCRIPT\|end .*: \d+ lines, 0 failures',text),text[-1000:]
    timings=re.findall(r'FRAMEBENCH\|n=50\|median=([\d.]+)\|p90=([\d.]+)',text)
    assert len(timings)==len(phases)
    if label=='new':
        batches=re.findall(r'fragment_ranges=(\d+)',text)
        assert batches and max(map(int,batches))>1000, 'Fragment batching was not exercised'
    metrics[label]={phase:{'median_ms':float(t[0]),'p90_ms':float(t[1])} for (phase,_),t in zip(phases,timings)}
    print(label,metrics[label],flush=True)
for phase,_ in phases:
    a=np.asarray(Image.open(OUT/('old-'+phase+'.png')),dtype=np.int16)
    for label in ['new','disabled']:
        b=np.asarray(Image.open(OUT/(label+'-'+phase+'.png')),dtype=np.int16)
        d=np.abs(a-b).max(axis=2)
        count,maxd=int(np.count_nonzero(d)),int(d.max())
        metrics[label][phase].update(changed_pixels=count,max_delta=maxd)
        print(phase,label,'pixels',count,'delta',maxd,flush=True)
        assert maxd<=1 and count<1000,(phase,label,count,maxd)
(OUT/'metrics.json').write_text(json.dumps(metrics,indent=2))
print('PASS: intact, fragment flight, settlement, depth priming, fade, scorch and cleanup',flush=True)
