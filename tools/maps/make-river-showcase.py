#!/usr/bin/env python3
"""Build USER60, a filming scene on River Raid's unchanged terrain tiles."""
from collections import deque
import configparser
import hashlib
from pathlib import Path
import sys
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/autoheight'))
import corpus as C

OUT = ROOT / 'game/examples'
SCEN = 'USER60'

def build():
    src = ROOT / 'game/missions/SCM04EA'
    c = configparser.ConfigParser(strict=False)
    c.optionxform = str
    c.read(src.with_suffix('.INI'))
    raw = src.with_suffix('.BIN').read_bytes()
    tiles = np.frombuffer(raw, np.uint8).reshape(64, 64, 2)
    land = np.array([[C.land_of(*map(int, tiles[y,x])) for x in range(64)] for y in range(64)])
    terrain = {int(k):v for k,v in c['TERRAIN'].items()}
    overlay = {int(k):v for k,v in c['OVERLAY'].items()}
    hgt = np.frombuffer((ROOT/'data/autoheight/SCM04EA.HGT').read_bytes(), np.uint8).reshape(65,65).copy()
    structures, units, infantry = [], [], []
    occupied = set()
    footprints = C.FOOTPRINT
    pads = [(5,10,28,20),(15,27,27,34),(35,46,58,54),(49,4,59,15)]
    def cell(x,y): return y*64+x
    def clear(x,y): return 4 <= x < 60 and 2 <= y < 61 and land[y,x] in (0,1)
    def inside_pad(x,y): return any(x0 <= x <= x1 and y0 <= y <= y1 for x0,y0,x1,y1 in pads)
    # Remove scenery only where the occupied footprint would intrude into a compound.
    for n,v in list(terrain.items()):
        x,y=n%64,n//64
        if any(inside_pad(x+dx,y+dy) for dx in range(-1,3) for dy in range(-1,3)):
            del terrain[n]
    for n in list(overlay):
        if inside_pad(n%64,n//64): del overlay[n]
    def structure(house,kind,x,y):
        cells={(x+dx,y+dy) for dx,dy in footprints[kind]}
        assert all(clear(*p) for p in cells),(kind,x,y,'ground')
        assert not cells & occupied,(kind,x,y,'overlap',cells & occupied)
        occupied.update(cells)
        for px,py in cells:
            overlay.pop(cell(px,py),None)
        structures.append(f'{house},{kind},256,{cell(x,y)},0,None')
        # Level the entire fitted plot, including its bib, using the source terrace.
        x0=min(px for px,py in cells);x1=max(px for px,py in cells)+1
        y0=min(py for px,py in cells);y1=max(py for px,py in cells)+1
        level=int(round(float(np.median(hgt[y0:y1+1,x0:x1+1]))/64)*64)
        hgt[y0:y1+1,x0:x1+1]=max(0,min(255,level))
    def bridge(x,y):
        t=int(tiles[y,x,0]);return t < len(C.TN) and C.TN[t].startswith('BRIDGE')
    def wall(kind,x,y):
        if clear(x,y) and not bridge(x,y) and (x,y) not in occupied:
            overlay[cell(x,y)]=kind
    def perimeter(kind,box,gaps):
        x0,y0,x1,y1=box
        pts={(x,y0) for x in range(x0,x1+1)}|{(x,y1) for x in range(x0,x1+1)}
        pts|={(x0,y) for y in range(y0,y1+1)}|{(x1,y) for y in range(y0,y1+1)}
        for p in sorted(pts):
            if p not in gaps: wall(kind,*p)
    parked=set()
    def spot(x,y):
        options=sorted(((dx,dy) for dx in range(-3,4) for dy in range(-3,4)), key=lambda p:(p[0]*p[0]+p[1]*p[1],p))
        for dx,dy in options:
            px,py=x+dx,y+dy; n=cell(px,py)
            if clear(px,py) and (px,py) not in occupied|parked and n not in terrain and (n not in overlay or overlay[n].startswith('TI')):
                parked.add((px,py));return px,py
        raise AssertionError(('parking',x,y))
    def unit(house,kind,x,y,face=0,mission='Guard'):
        x,y=spot(x,y)
        assert clear(x,y) and (x,y) not in occupied,(kind,x,y)
        assert cell(x,y) not in overlay or overlay[cell(x,y)].startswith('TI'),(kind,x,y,'wall')
        units.append(f'{house},{kind},256,{cell(x,y)},{face},{mission},None')
    def soldier(house,kind,x,y,sub=0,face=128):
        x,y=spot(x,y)
        assert clear(x,y) and (x,y) not in occupied,(kind,x,y)
        assert overlay.get(cell(x,y)) not in ('BRIK','SBAG'),(kind,x,y,'wall')
        infantry.append(f'{house},{kind},256,{cell(x,y)},{sub},Guard,{face},None')

    for kind,x,y in [('FACT',9,11),('WEAP',15,11),('EYE',21,11),('HQ',25,11),
                     ('NUK2',6,11),('NUK2',6,15),('NUK2',24,15),('NUK2',26,15),
                     ('PYLE',10,15),('PROC',16,15),('SILO',21,16),('GTWR',8,19),
                     ('GTWR',25,19),('ATWR',13,17)]:
        structure('GoodGuy',kind,x,y)
    for kind,x,y in [('PYLE',16,28),('HQ',19,27),('NUK2',25,27),('PROC',22,30),
                     ('GTWR',16,32),('GTWR',25,33),('SAM',18,32)]:
        structure('GoodGuy',kind,x,y)
    for kind,x,y in [('FACT',37,47),('TMPL',42,46),('HAND',47,47),('NUK2',52,47),
                     ('NUK2',55,47),('PROC',37,50),('AFLD',42,51),('HQ',47,51),
                     ('NUK2',52,50),('NUK2',55,50),('OBLI',50,48),('GUN',36,48),
                     ('GUN',57,53),('SAM',49,53)]:
        structure('BadGuy',kind,x,y)
    for kind,x,y in [('HAND',50,5),('PROC',54,5),('NUK2',57,9),('HQ',54,10),
                     ('SAM',53,13),('GUN',57,13)]:
        structure('BadGuy',kind,x,y)
    perimeter('BRIK',pads[0],{(x,10) for x in range(10,13)}|{(x,20) for x in range(17,21)}|{(5,y) for y in range(14,17)})
    perimeter('SBAG',pads[1],{(x,34) for x in range(20,25)}|{(15,y) for y in range(29,32)}|{(x,27) for x in range(17,20)})
    perimeter('BRIK',pads[2],{(35,y) for y in range(49,53)}|{(58,y) for y in range(49,53)}|{(x,46) for x in range(38,41)})
    perimeter('CYCL',pads[3],{(59,y) for y in range(8,11)}|{(x,15) for x in range(53,57)})
    for x in (6,7,8,9,12,13,14,23,24,25,26): wall('SBAG',x,19)
    for x in (36,37,38,39,40,47,48,49,50): wall('SBAG',x,55)
    # Each free concrete end is a four-piece square post, away from its gate.
    concrete={(n%64,n//64) for n,v in overlay.items() if v=='BRIK'}
    ends=[]
    for x,y in sorted(concrete):
        neighbours=[p for p in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)) if p in concrete]
        if len(neighbours)==1: ends.append(((x,y),neighbours[0]))
    for (x,y),(nx,ny) in ends:
        dx,dy=nx-x,ny-y
        for attempt in range(8):
            candidates=[{(x,y),(x+dx,y+dy),(x-dy*q,y+dx*q),(x+dx-dy*q,y+dy+dx*q)} for q in (1,-1)]
            fit=next((pts for pts in candidates if all(clear(*p) and not bridge(*p) and p not in occupied and cell(*p) not in terrain and overlay.get(cell(*p),'') not in ('CYCL',) for p in pts)),None)
            if fit:
                for px,py in fit: wall('BRIK',px,py)
                break
            # Retreat a shoreline end rather than extending its post into water.
            assert overlay.get(cell(x,y))=='BRIK',('concrete end',x,y)
            overlay.pop(cell(x,y));x+=dx;y+=dy
        else: raise AssertionError(('concrete square post',x,y))
    # Rich, replenishing resource pockets reachable through the open gates.
    for cx,cy,rx,ry in [(9,6,4,3),(11,30,3,3),(55,19,3,3),(56,57,3,3)]:
        for y in range(cy-ry,cy+ry+1):
            for x in range(cx-rx,cx+rx+1):
                n=cell(x,y)
                if clear(x,y) and ((x-cx)/rx)**2+((y-cy)/ry)**2 <= 1 and (x,y) not in occupied and n not in terrain and not inside_pad(x,y) and (n not in overlay or overlay[n].startswith('TI')):
                    overlay[n]='TI'+str(1+(x*7+y*3)%12)
    for house,x,y in [('GoodGuy',17,19),('GoodGuy',25,31),('BadGuy',40,52),('BadGuy',58,8)]:
        unit(house,'HARV',x,y,128,'Harvest')
    for house,kind,x,y,face in [('GoodGuy','MTNK',12,18,128),('GoodGuy','MTNK',14,18,128),
                              ('GoodGuy','HTNK',20,18,128),('GoodGuy','JEEP',18,30,96),
                              ('BadGuy','LTNK',41,50,0),('BadGuy','LTNK',43,50,0),
                              ('BadGuy','STNK',45,50,0),('BadGuy','BGGY',58,12,128)]:
        unit(house,kind,x,y,face)
    for x,y in [(9,18),(10,18),(11,18),(26,19),(16,31),(26,32)]: soldier('GoodGuy','E1',x,y)
    for x,y in [(40,49),(41,49),(51,49),(57,12)]: soldier('BadGuy','E5',x,y)
    # Distinct rosters keep each initial squad on its own looping route.
    for k in range(3): soldier('BadGuy','E3',34+k%2,49+k//2,k,0)
    for k in range(3): soldier('BadGuy','E4',48+k,56,k,0)
    for k in range(3): soldier('BadGuy','E1',55+k%2,16+k//2,k,0)
    wp={0:(34,49),1:(36,44),2:(43,45),3:(34,53),4:(45,55),5:(51,56),6:(50,59),7:(46,57),
        8:(55,16),9:(58,17),10:(53,19),11:(52,17),26:(17,16),27:(17,16)}
    for n,(x,y) in wp.items(): assert clear(x,y),(n,x,y,'waypoint')
    # Patrols have three-cell-wide resource-free lanes; no lethal tiberium walks.
    blocked=occupied|{(n%64,n//64) for n,v in overlay.items() if v in ('BRIK','SBAG','CYCL')}
    blocked|={(n%64,n//64) for n in terrain}
    def route(a,b):
        queue=deque([a]);prev={a:None}
        while queue:
            p=queue.popleft()
            if p==b: break
            for dx,dy in ((0,1),(1,0),(0,-1),(-1,0)):
                q=(p[0]+dx,p[1]+dy)
                if q not in prev and clear(*q) and q not in blocked:
                    prev[q]=p;queue.append(q)
        assert b in prev,('patrol route',a,b)
        path=[];p=b
        while p is not None: path.append(p);p=prev[p]
        return path
    for start in (0,4,8):
        for k in range(4):
            for x,y in route(wp[start+k],wp[start+(k+1)%4]):
                for dx in (-1,0,1):
                    for dy in (-1,0,1):
                        n=cell(x+dx,y+dy)
                        if overlay.get(n,'').startswith('TI'): del overlay[n]
    concrete={(n%64,n//64) for n,v in overlay.items() if v=='BRIK'}
    for x,y in concrete:
        assert sum(p in concrete for p in ((x-1,y),(x+1,y),(x,y-1),(x,y+1))) != 1,('uncapped wall',x,y)
    for n,v in overlay.items():
        if v=='SBAG': assert not bridge(n%64,n//64),('bridge sandbags',n)
    teams={
        'west':'BadGuy,0,0,0,0,0,30,1,0,0,1,E3:3,9,Move:0,Guard:2,Move:1,Guard:2,Move:2,Guard:2,Move:3,Guard:2,Loop:0,0,1',
        'east':'BadGuy,0,0,0,0,0,30,1,0,0,1,E4:3,9,Move:4,Guard:2,Move:5,Guard:2,Move:6,Guard:2,Move:7,Guard:2,Loop:0,0,1',
        'ford':'BadGuy,0,0,0,0,0,30,1,0,0,1,E1:3,9,Move:8,Guard:2,Move:9,Guard:2,Move:10,Guard:2,Move:11,Guard:2,Loop:0,0,1'}
    lines=['; River Raid filming scene. Cell numbers use y*64+x. Terrain BIN is unchanged.',
           '[Basic]','Name=River Raid - Cinematic Bases','Player=GoodGuy','OpenCNC3DKind=Single','Enhanced=1',
           'CarryOverMoney=0','CarryOverCap=-1','BuildLevel=98','Theme=No theme','Intro=x','Brief=x','Action=x','Win=x','Lose=x','Percent=0',
           '', '[MAP]','Theater=TEMPERATE','X=4','Y=2','Width=56','Height=59']
    for house in ('GoodGuy','BadGuy','Neutral','Special'):
        lines += ['',f'[{house}]','Credits=150' if house in ('GoodGuy','BadGuy') else 'Credits=0','MaxBuilding=150','MaxUnit=150',f'Allies={house}','Edge=North']
    def section(name,rows): lines.extend(['',f'[{name}]']+[f'{k}={v}' for k,v in rows])
    section('Base',[('Count',0)])
    section('Waypoints',[(k,cell(*v)) for k,v in wp.items()])
    section('TERRAIN',sorted(terrain.items()))
    section('OVERLAY',sorted(overlay.items()))
    section('SMUDGE',[])
    for name,rows in [('STRUCTURES',structures),('UNITS',units),('INFANTRY',infantry)]: section(name,[(f'{k:03}',v) for k,v in enumerate(rows)])
    section('TeamTypes',teams.items())
    section('Triggers',[(name,'Time,Create Team,1,BadGuy,'+team+',0') for name,team in [('pwst','west'),('pest','east'),('pfrd','ford')]])
    OUT.mkdir(exist_ok=True)
    (OUT/(SCEN+'.INI')).write_text('\n'.join(map(str,lines))+'\n')
    (OUT/(SCEN+'.BIN')).write_bytes(raw)
    (OUT/(SCEN+'.HGT')).write_bytes(hgt.tobytes())
    print(f'{SCEN}: {len(structures)} buildings, {len(units)} vehicles, {len(infantry)} infantry, {len(teams)} patrols')
    print('Terrain SHA256:',hashlib.sha256(raw).hexdigest())

if __name__=='__main__': build()
