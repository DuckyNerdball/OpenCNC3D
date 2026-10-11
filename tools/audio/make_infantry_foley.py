#!/usr/bin/env python3
"""Prepare recorded grass steps; requires NumPy and ffmpeg. Sources/credits ship with assets."""
import hashlib,json,subprocess,wave
from pathlib import Path
import numpy as np
RATE=44100
URL='https://opengameart.org/content/grass-foot-step-sounds-yo-frankie'
def main():
    root=Path(__file__).resolve().parents[2];out=root/'data/foley';files=[];sources=[];clips=[]
    for side in ('l','r'):
        p=out/'source'/f'sfx_step_grass_{side}.flac'
        raw=subprocess.check_output(['ffmpeg','-v','error','-i',str(p),'-f','s16le','-ac','1','-ar',str(RATE),'-'])
        clips.append(np.frombuffer(raw,dtype='<i2').astype(float)/32768)
        sources.append(dict(file=str(p.relative_to(out)),sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
    specs=((0,1,0),(1,1,0),(0,.97,0),(1,1.03,0),
           (0,1.01,-.22),(1,.99,-.22),(0,.985,.18),
           (1,1.015,.18),(0,1.025,-.1),(1,.975,.1))
    for v,(side,speed,tone) in enumerate(specs):
        src=clips[side];x=np.interp(np.arange(0,len(src)-1,speed),np.arange(len(src)),src)
        if tone:
            # Gentle brush/sole balance from the accepted recordings, no added noise.
            body=np.convolve(x,np.ones(51)/51,mode="same")
            x+=tone*(x-body)
        x-=x.mean();x*=.14/max(abs(x));n=len(x)
        x[:44]*=np.linspace(0,1,44);x[-882:]*=np.linspace(1,0,882)
        pcm=np.rint(np.column_stack((x,x))*32767).astype('<i2');p=out/f'infantry_step_{v+1:02d}.wav'
        with wave.open(str(p),'wb') as w:
            w.setnchannels(2);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(pcm.tobytes())
        files.append(dict(file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest(),source=sources[side]['file'],speed=speed,brush_eq=tone,seconds=n/RATE,peak=int(np.abs(pcm.astype(int)).max())))
    (out/'manifest.json').write_text(json.dumps(dict(version=3,author='Blender Foundation (Yo Frankie!)',license='CC-BY-3.0',source=URL,sources=sources,changes='DC removal, level adjustment, endpoint fades, mono duplicated to stereo; ten variants with gentle brush/sole EQ and subtle speed differences',files=files),indent=2)+'\n')
    print('Prepared ten recorded grass variants.')
if __name__=='__main__':main()
