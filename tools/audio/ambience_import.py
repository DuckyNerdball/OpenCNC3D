#!/usr/bin/env python3
"""Import Soundly ambience/HUD WAVs without altering the sources.
Preserves stereo and full duration, converts to PCM16/44100, and overlaps the
last 500ms with the first 500ms on looping beds. No loudness normalization.
Run: python3 tools/audio/ambience_import.py ~/Desktop/Soundly
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import wave
import numpy as np

NAMES = ('desert_zoomin', 'desert_zoomout', 'temperate_zoomin',
         'temperate_zoomout', 'river_zoomin', 'water_zoomin', 'water_zoomout',
         'hud_click', 'hud_mouseover', 'tank_drive', 'small_tank_drive', 'vehicle_drive')
RATE = 44100

def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source', type=Path)
    ap.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[2] / 'data/ambience')
    ap.add_argument('--only', nargs='+', choices=NAMES, help='Import selected clips and retain other manifest entries')
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest_path = args.output / 'manifest.json'
    entries = json.loads(manifest_path.read_text())['files'] if args.only and manifest_path.exists() else []
    for name in args.only or NAMES:
        entries = [e for e in entries if e['name'] != name]
        src, dst = args.source / (name + '.wav'), args.output / (name + '.wav')
        with tempfile.TemporaryDirectory() as td:
            converted = Path(td) / 'converted.wav'
            subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@44100',
                            '-c', '2', '-r', '127', str(src), str(converted)], check=True)
            with wave.open(str(converted), 'rb') as w:
                assert w.getnchannels() == 2 and w.getframerate() == RATE and w.getsampwidth() == 2
                pcm = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, 2).copy()
            source_frames = len(pcm)
            overlap = 0 if name.startswith('hud_') else min(RATE // 2, len(pcm) // 4)
            if overlap:
                # Begin at the end of the source head; end by blending into that head.
                # Both boundaries continue consecutive source samples, avoiding a cut.
                a = np.arange(overlap, dtype=np.float64)[:, None] / overlap
                blend = np.rint(pcm[-overlap:] * (1 - a) + pcm[:overlap] * a).astype('<i2')
                pcm = np.concatenate((pcm[overlap:-overlap], blend))
            with wave.open(str(dst), 'wb') as w:
                w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE)
                w.writeframes(pcm.tobytes())
            peak = int(np.abs(pcm.astype(np.int32)).max())
            seam = np.abs(pcm[0].astype(np.int32) - pcm[-1].astype(np.int32)).tolist()
            entries.append(dict(name=name, source=str(Path(args.source.name) / src.name), source_sha256=sha(src),
                                file=dst.name, sha256=sha(dst), rate=RATE, channels=2,
                                source_seconds=source_frames/RATE, seconds=len(pcm)/RATE,
                                loop_overlap_frames=overlap, peak=peak, wrap_step=seam))
            print(f'{name}: {len(pcm)/RATE:.2f}s stereo; peak {peak}; wrap {seam}', flush=True)
    (args.output / 'manifest.json').write_text(json.dumps(dict(version=1,
        processing='Stereo PCM16/44100; source gain preserved; 500ms linear tail/head overlap for beds.',
        files=entries), indent=2) + '\n')

if __name__ == '__main__':
    main()
