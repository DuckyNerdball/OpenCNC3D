#!/usr/bin/env python3
"""Import the Soundly placement impact and rumble into data/cinematic.

  placement_impact.wav     impact.wav, faded out over its last second (the recording
                           stops while still loud), with impact_2.wav laid under it from
                           the same first sample; the pair is as long as the longer one.
                           Scaled down to -1 dBFS only if the sum would pass it.
  construction_rumble.wav  shake.wav, raised to a -1 dBFS peak (the recording peaks
                           near -8 dBFS). The game loops its steady middle with a
                           crossfade for as long as a placement's camera rumble lasts.

Converted to stereo PCM16/44100 with afconvert, as tools/audio/ambience_import.py does.
The other three layers stay with tools/audio/make_cinematic_audio.py; their manifest
entries are kept. macOS only (afconvert).

Run: python3 tools/audio/cinematic_import.py <folder holding the three Soundly exports>
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import wave
import numpy as np

RATE = 44100
CEILING = 10 ** (-1 / 20)  # -1 dBFS

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def load(src, td):
    out = Path(td) / (src.stem + '.wav')
    subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@44100', '-c', '2', '-r', '127',
                    str(src), str(out)], check=True)
    with wave.open(str(out), 'rb') as w:
        assert w.getnchannels() == 2 and w.getframerate() == RATE and w.getsampwidth() == 2
        return np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, 2) / 32768.0

def write(dst, x):
    pcm = np.rint(np.clip(x, -1, 32767 / 32768) * 32768).astype('<i2')
    with wave.open(str(dst), 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    return pcm

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('source', type=Path)
    ap.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[2] / 'data/cinematic')
    args = ap.parse_args()
    srcs = {n: args.source / (n + '.wav') for n in ('impact', 'impact_2', 'shake')}
    with tempfile.TemporaryDirectory() as td:
        a, b, shake = (load(srcs[n], td) for n in ('impact', 'impact_2', 'shake'))
    # impact: a one-second raised-cosine fade to silence at its end
    fade = min(RATE, len(a))
    a = a.copy()
    a[-fade:] *= (0.5 + 0.5 * np.cos(np.pi * np.arange(fade) / fade))[:, None]
    hit = np.zeros((max(len(a), len(b)), 2))
    hit[:len(a)] += a
    hit[:len(b)] += b
    hit_scale = min(1.0, CEILING / np.abs(hit).max())
    hit *= hit_scale
    rumble = shake * (CEILING / np.abs(shake).max())
    rumble_scale = CEILING / np.abs(shake).max()

    manifest_path = args.output / 'manifest.json'
    manifest = json.loads(manifest_path.read_text())
    keep = [e for e in manifest['files'] if e['file'] not in ('placement_impact.wav', 'construction_rumble.wav')]
    made = []
    for name, x, sources, scale in (('placement_impact', hit, ('impact', 'impact_2'), hit_scale),
                                    ('construction_rumble', rumble, ('shake',), rumble_scale)):
        dst = args.output / (name + '.wav')
        pcm = write(dst, x)
        made.append(dict(file=dst.name, sha256=sha(dst), seconds=len(pcm) / RATE,
                         peak=int(np.abs(pcm.astype(np.int32)).max()), rms=float(np.sqrt(np.mean(x * x))),
                         source=[str(Path(args.source.name) / srcs[s].name) for s in sources],
                         source_sha256=[sha(srcs[s]) for s in sources], gain=round(float(scale), 4)))
        print(f'{name}: {len(pcm) / RATE:.2f}s, peak {made[-1]["peak"]}, gain x{scale:.3f}')
    manifest['files'] = [made[0]] + keep + [made[1]]
    manifest['source'] = ('placement_impact and construction_rumble: Soundly recordings imported by '
                          'tools/audio/cinematic_import.py. The other three: original procedural sound '
                          'design by tools/audio/make_cinematic_audio.py; no external samples')
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')

if __name__ == '__main__':
    main()
