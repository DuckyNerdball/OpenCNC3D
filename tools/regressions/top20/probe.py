#!/usr/bin/env python3
"""Private, hidden native regressions for the November 2 community bug pass.

Creates synthetic flat missions only inside --out. All player saves/preferences
are redirected there. No network, visible window, input injection or live install.
Run separately for each executable/engine pair to establish before/after results.
"""
import argparse, base64, json, os, re, struct, subprocess
from pathlib import Path


def packed(raw):
    blocks = bytearray()
    for pos in range(0, len(raw), 8192):
        part = raw[pos:pos+8192]
        assert len(set(part)) == 1
        compressed = b'\xfe' + struct.pack('<H',len(part)) + part[:1] + b'\x80'
        blocks += struct.pack('<HH', len(compressed), len(part)) + compressed
    text = base64.b64encode(blocks).decode()
    return '\n'.join(f'{i//70+1}={text[i:i+70]}' for i in range(0,len(text),70))


def fixture(out, ra=False, kind='controls'):
    d=out/('ra-' if ra else 'td-')/kind; d.mkdir(parents=True,exist_ok=True)
    good,bad=('Greece','USSR') if ra else ('GoodGuy','BadGuy')
    stride=128 if ra else 64
    cell=lambda x,y:y*stride+x
    units=[f'0={good},{"1TNK" if ra else "MTNK"},256,{cell(40,45)},0,Guard,None']
    infantry=[]
    if kind=='controls':
        infantry=[f'{i}={good},E1,256,{cell(x,38)},0,Guard,0,None' for i,x in enumerate((36,40))]
    buildings=[f'0={good},FACT,256,{cell(35,35)},0,None',
               f'1={good},{"POWR" if ra else "NUKE"},256,{cell(39,35)},0,None',
               f'2={bad},FACT,256,{cell(55,25)},0,None']
    if kind=='defense':
        units=[f'0={good},{"1TNK" if ra else "MTNK"},256,{cell(42,43)},0,Guard,None']
        buildings += [f'3={good},{"GUN" if ra else "ATWR"},256,{cell(40,40)},0,None',
                      f'4={good},{"POWR" if ra else "NUKE"},256,{cell(42,40)},0,None']
    if kind=='ambush': infantry=[f'0={bad},E1,256,{cell(46,46)},0,Sleep,0,None']
    if kind=='apc':
        units=[f'0={good},APC,256,{cell(40,45)},0,Guard,None']
        infantry=[f'0={good},E1,256,{cell(40,46)},0,Guard,0,None']
    text=f'''[Basic]
Name=Private November regression
Player={good}
BuildLevel=98
NewINIFormat=3
BaseBuilding=yes
[Map]
Theater=TEMPERATE
X=1
Y=1
Width=60
Height=60
[{good}]
Credits=10000
MaxBuilding=150
MaxUnit=150
MaxInfantry=150
MaxAircraft=150
TechLevel=10
Allies={good}
[{bad}]
Credits=1000
MaxBuilding=150
MaxUnit=150
MaxInfantry=150
MaxAircraft=150
TechLevel=10
Allies={bad}
[Waypoints]
0={cell(30,30)}
1={cell(50,50)}
[Structures]
'''+ '\n'.join(buildings)+'\n[Units]\n'+'\n'.join(units)+'\n[Infantry]\n'+'\n'.join(infantry)+'\n'
    if ra: text+='[MapPack]\n'+packed(b'\xff'*(128*128*2)+b'\0'*(128*128))+'\n'
    else: (d/'SCG88EA.BIN').write_bytes(b'\0\0'*(64*64))
    (d/'SCG88EA.INI').write_text(text)
    return d


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path,required=True);p.add_argument('--exe',required=True)
    p.add_argument('--td-brain',default='TiberianDawn.dylib');p.add_argument('--ra-brain',default='ra3d/RedAlert.dylib')
    p.add_argument('--out',type=Path,required=True);p.add_argument('--case',action='append')
    a=p.parse_args();run=a.runtime.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    cases=[]
    for ra in (False,True):
        for mode in (0,1,2):
            ty='1TNK' if ra else 'MTNK'
            lines=[f'gfxmode {mode}','frames 1','tick 10','look '+ty+' 0','frames 1',
                   'actclickobj '+ty+' 0','expectsel 1','repairmode 1','sbstates','rdown 640 400','rup','sbstates',
                   'sellmode 1','sbstates','rdown 640 400','rup','sbstates','quit']
            cases.append(('controls-'+str(mode),ra,'controls',lines,False))
        ty='1TNK' if ra else 'MTNK'
        cases.append(('fog-route',ra,'controls',['gfxmode 1','frames 1','tick 5','look '+ty+' 0','zoom min',
             'actclickobj '+ty+' 0','order 48 45','tick 360','obj '+ty+' 0','reveal 1','order 40 45',
             'tick 360','obj '+ty+' 0','quit'],False))
        cases.append(('ambush',ra,'ambush',['gfxmode 1','frames 1','cheatset fog 0','tick 5','cam 48 45','zoom min',
             'actclickobj '+ty+' 0','amove','actcell 58 45','tick 120','obj '+ty+' 0','obj E1 0',
             'seldump','tick 5880','obj '+ty+' 0','obj E1 0','seldump','quit'],False))
        defense='GUN' if ra else 'ATWR';power='POWR' if ra else 'NUKE'
        cases.append(('defense-stop',ra,'defense',['gfxmode 1','frames 1','cheatset fog 0','player','tick 5','look '+defense+' 0',
            'zoom min','actclickobj '+defense+' 0','expectsel 1','actclickobj '+power+' 1 c','tick 5',
            'echo before-stop','seldump','stop','tick 1','echo after-stop','seldump','quit'],True))
        cases.append(('defense-ground',ra,'defense',['gfxmode 1','frames 1','cheatset fog 0','tick 5','look '+defense+' 0',
            'zoom min','actclickobj '+defense+' 0','expectsel 1','actcell 45 40 c','tick 5',
            'echo before-stop','seldump','stop','tick 1','echo after-stop','seldump','quit'],True))
        cases.append(('apc-combat',ra,'apc',['gfxmode 1','frames 1','tick 5','look APC 0','zoom min',
            'actclickobj E1 0','orderobj APC 0','tick 180','obj APC 0','obj E1 0','objdump',
            'spawnobj '+('1TNK' if ra else 'LTNK')+' 43 45 enemy','tick 10',
            'actclickobj APC 0','actclickobj APC 0','tick 180','obj APC 0','obj E1 0','objdump','quit'],True))
        power='POWR' if ra else 'NUKE'
        cases.append(('placement',ra,'controls',['gfxmode 1','frames 1','cheatset buildany 0',
            'cheatset fog 0','tick 60','sidebar before','objdump','count '+power+' 1','build '+power,'ready '+power+' 6000',
            'startplace '+power,'gateplace 58 58','tick 5','echo rejected-placement','objdump',
            'startplace '+power,'gateplace 41 35','tick 5','echo accepted-placement','objdump','count '+power+' 2','quit'],True))
        # Exercise the complete south strip, including the sidebar, in every HUD.
        # A centre-only guard assertion passed while the sidebar blocked scrolling.
        for mode in (0, 1, 2):
            lines=[f'gfxmode {mode}','frames 1','tick 5','look '+ty+' 0',
                   'actclickobj '+ty+' 0','frames 1','zoom max']
            for x in (1,100,320,640,960,1100,1200,1278):
                lines += ['cam 30 30',f'edgeat {x} 719 60']
            for x,y in ((640,360),(640,720),(-1,719)):
                lines += ['cam 30 30',f'edgeat {x} {y} 60']
            cases.append(('bottom-scroll-'+str(mode),ra,'controls',lines+['quit'],True))
    cases.append(('nod-helipad',False,'controls',['gfxmode 2','frames 1','tick 2','look MCV:Multi6 0',
        'actclickobj MCV:Multi6 0','actclickobj MCV:Multi6 0','tick 90','cheatset instant 1',
        'spawnobj HAND 35 40','tick 5','player','sidebar before','build HPAD','ready HPAD 100',
        'startplace HPAD','place auto','tick 30','count HPAD 1','sidebar after',
        'build HELI','tick 60','count HELI','quit'],True))
    results=[]
    for name,ra,kind,lines,unshroud in cases:
        label=('ra-' if ra else 'td-')+name
        power='POWR' if ra else 'NUKE'
        if a.case and label not in a.case:continue
        d=fixture(out,ra,kind);script=out/(label+'.txt');script.write_text('\n'.join(lines)+'\n')
        cmd=[str((run/a.exe).resolve()),'--scen','SCG88EA','--pack','ra3d/SCG01EA.pack' if ra else 'SCG01EA.pack',
             '--dir',str(d)+'/', '--content','ra3d/data' if ra else 'content/', '--dylib',a.ra_brain if ra else a.td_brain,
             '--dosinf','dosinfantry.pack','--dospack','dossidebar.pack','--cameos','cameos.pack',
             '--script',str(script),'--w','1280','--h','720','--nosound','--build','98',
             '--savedir',str(out/'saves'),'--gfxsave',str(out/(label+'.cfg'))]
        if name=='nod-helipad':cmd+=['--skirmish','--side','nod','--ai','1','--credits','10000','--seed','1234']
        if ra:cmd+=['--ra3d']
        else:cmd+=['--reskin']
        if unshroud:cmd+=['--noshroud']
        log=out/(label+'.log')
        with log.open('w') as f:
            try:r=subprocess.run(cmd,cwd=run,env={**os.environ,'OPENCNC3D_BACKGROUND':'1'},stdout=f,stderr=subprocess.STDOUT,timeout=180);rc=r.returncode
            except subprocess.TimeoutExpired:rc=124
        text=log.read_text(errors='replace')
        record=dict(case=label,exit=rc,script_ok=bool(re.search(r'^SCRIPT\|end .*0 failures',text,re.M)),log=str(log))
        if name.startswith('controls'):
            record['selection_ok']='EXPECTSEL|want=1|have=1|PASS' in text
            record['cancel_ok']=all('SIDEBAR-INPUT|'+m+'|on' in text and 'SIDEBAR-INPUT|'+m+'|off' in text for m in ['repair','sell'])
        if name in ('defense-stop','defense-ground'):
            after=text.split('SCRIPT|echo after-stop')[-1] if 'SCRIPT|echo after-stop' in text else text.split('after-stop')[-1]
            before=text.split('ECHO|before-stop')[-1].split('ECHO|after-stop')[0]
            record['target_was_active']=bool(re.search(r'^SEL\|BUILDING\|.*\|tarcell=(?!-1)[0-9]+',before,re.M))
            record['stop_clears_target']=bool(re.search(r'^SEL\|BUILDING\|.*\|tarcell=-1',after,re.M))
        if name=='fog-route':
            record['out_and_back']=('|cell=48,45|' in text and '|cell=40,45|' in text)
        if name=='apc-combat':
            record['boarded']='|pips=1|' in text and bool(re.search(r'^OBJ\|INFANTRY\|E1\|.*\|limbo=1\|',text,re.M))
            record['unloaded']=bool(re.search(r'^OBJ\|APC#0\|UNIT\|[^|]+\|cell=[^|]+\|str=(?!200/200)[0-9]+/200',text,re.M)) and '|limbo=0|' in text.split('OBJDUMP-BEGIN')[-1]
        if name.startswith('bottom-scroll-'):
            cameras=re.findall(r'^CAM\|edgeat\|x=([-.0-9]+)\|z=([-.0-9]+)',text,re.M)
            moved=[abs(float(x)-30)+abs(float(z)-30)>0.01 for x,z in cameras]
            record['all_south_positions_move']=len(moved)==11 and all(moved[:8])
            record['interior_and_outside_stay']=len(moved)==11 and not any(moved[8:])
        if name=='placement':
            def placed(section):
                return [line for line in section.splitlines() if line.startswith('OBJ|BUILDING|'+power+'|') and '|limbo=0|' in line]
            rejected=text.split('ECHO|rejected-placement')[-1].split('ECHO|accepted-placement')[0]
            accepted=text.split('ECHO|accepted-placement')[-1]
            record['remote_rejected']=len(placed(rejected))==1
            record['nearby_accepted']=len(placed(accepted))==2 and any('|x=41|y=35|' in line for line in placed(accepted))
        if name=='ambush':
            record['engages']=bool(re.search(r'^SEL\|UNIT\|.*\|tarcell=(?!-1)[0-9]+',text,re.M))
            record['kills_and_resumes']=('OBJ|E1#0|MISSING' in text and '|cell=58,45|' in text)
        results.append(record);print(json.dumps(record),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    return 0 if all(r['exit']==0 and all(v for k,v in r.items() if isinstance(v,bool)) for r in results) else 1

if __name__=='__main__':raise SystemExit(main())
