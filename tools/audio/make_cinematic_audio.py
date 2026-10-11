#!/usr/bin/env python3
"""Original cinematic destruction layers. NumPy; no sampled material.

Deterministic: the same NumPy gives the same bytes, and the manifest records each file's
hash. Run from anywhere; it writes data/cinematic/ of the checkout it lives in.

  building_collapse, vehicle_impact, structural_damage
                       a pressure/metal/debris impact each (impact() below).

The placement impact and the construction rumble are recordings, imported by
tools/audio/cinematic_import.py; this script keeps their manifest entries as they are.
"""
import hashlib,json,wave
from pathlib import Path
import numpy as np
RATE=44100
SEED=202610071

def noise(rng,n,lo,hi):
    x=rng.normal(0,1,n);f=np.fft.rfftfreq(n,1/RATE);spec=np.fft.rfft(x)
    mask=np.clip((f-lo)/max(20,lo*.4),0,1)*np.clip((hi-f)/max(20,hi*.3),0,1)
    x=np.fft.irfft(spec*mask,n);return x/max(1e-9,np.sqrt(np.mean(x*x)))
def impact(kind):
    durations=[1.45,2.75,.90,1.1];n=int(RATE*durations[kind]);t=np.arange(n)/RATE
    rng=np.random.default_rng(SEED+kind);weight=[1,1.3,.8,.65][kind]
    attack=1-np.exp(-t/.003)
    freq=43+54*np.exp(-t*18)
    phase=2*np.pi*np.cumsum(freq)/RATE
    # Mid-bass harmonics carry the impact on speakers that cannot reproduce the sub.
    body=(np.sin(phase)*.22+np.sin(2*phase)*.10+np.sin(3*phase)*.045)*attack*np.exp(-t*(5/weight))
    pressure=noise(rng,n,22,180)*attack*np.exp(-t*(6/weight))*.08
    slam=noise(rng,n,170,2000)*attack*np.exp(-t*32)*.115
    air=noise(rng,n,850,6500)*(1-np.exp(-t/.018))*np.exp(-t*7)*.050
    metal=np.zeros(n)
    for hz in (172,239,431,677):
        metal+=np.sin(2*np.pi*hz*t+rng.uniform(0,6.28))*(1-np.exp(-t/.002))*np.exp(-t*rng.uniform(10,18))*.012
    debris=np.zeros(n)
    for _ in range(34 if kind==1 else 12):
        at=rng.uniform(.045,durations[kind]*.66);u=np.maximum(t-at,0)
        env=(t>=at)*(1-np.exp(-u*1800))*np.exp(-u*rng.uniform(65,160))
        debris+=noise(rng,n,300,4200)*env*rng.uniform(.0008,.004)*np.exp(-at)
    dry=body+pressure+slam+air+metal+debris
    # Quiet staggered reflections widen the tail; direct impact remains centered.
    stereo=np.column_stack((dry,dry))
    for ch,delays in enumerate(((.053,.091,.137),(.061,.103,.151))):
        for j,delay in enumerate(delays):
            d=int(delay*RATE);stereo[d:,ch]+=dry[:-d]*(.075/(j+1))
    stereo[-int(.06*RATE):]*=np.linspace(1,0,int(.06*RATE))[:,None]
    return stereo

# ---- the placement impact and the construction rumble ------------------------------------
def main():
    root=Path(__file__).resolve().parents[2];out=root/'data/cinematic';out.mkdir(parents=True,exist_ok=True)
    names=['placement_impact','building_collapse','vehicle_impact','structural_damage','construction_rumble']
    mpath=out/'manifest.json'
    old={e['file']:e for e in json.loads(mpath.read_text())['files']} if mpath.exists() else {}
    files=[]
    for k,name in enumerate(names):
        # The placement impact and rumble are recordings (tools/audio/cinematic_import.py).
        if k in (0,4):
            if name+'.wav' in old: files.append(old[name+'.wav'])
            continue
        x=impact(k);x-=x.mean(axis=0)
        audio=np.tanh(x*2);audio-=audio.mean(axis=0)
        audio[:128]*=np.linspace(0,1,128)[:,None]
        audio[-1102:]*=np.linspace(1,0,1102)[:,None]
        audio*=.78/np.max(np.abs(audio))
        pcm=np.rint(audio*32767).astype('<i2');p=out/(name+'.wav')
        with wave.open(str(p),'wb') as w:
            w.setnchannels(2);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(pcm.tobytes())
        files.append(dict(file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest(),seconds=len(x)/RATE,peak=int(np.abs(pcm.astype(int)).max()),rms=float(np.sqrt(np.mean(audio*audio)))))
    m=json.loads(mpath.read_text()) if mpath.exists() else dict(version=3)
    m.update(seed=SEED,generator='tools/audio/make_cinematic_audio.py',files=files)
    mpath.write_text(json.dumps(m,indent=2)+'\n')
    print('Created the three original destruction layers.')
if __name__=='__main__':main()
