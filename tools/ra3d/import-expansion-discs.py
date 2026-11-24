#!/usr/bin/env python3
"""Extract the two supported, hash-pinned archive.org original expansion data tracks.

import-expansion-discs.py counterstrike|aftermath DISC.bin --out SOURCE_DIRECTORY
Outputs GENERAL/SOUNDS/SCORES plus Aftermath's uncompressed installed MIX records.
No game executables are run. SOURCE-RECEIPT.json records every retained file hash.
"""
import argparse,hashlib,json,sys
from pathlib import Path
from ramix import RaMix
DISCS={
 'counterstrike':('redump-id-9855','d25da331642b77437857820de66d35ab3711d72a',37368,236034607),
 'aftermath':('redump-id-2199','695e28f911bf7bec9a9c2b47ecde0d541b6ad685',42682,270673111),
}
def read_raw(path,offset,size):
 first,inner=divmod(offset,2048);last=(offset+size+2047)//2048
 with path.open('rb') as stream:
  stream.seek(first*2352);raw=stream.read((last-first)*2352)
 if len(raw)!=(last-first)*2352:raise ValueError('Incomplete original data track')
 data=b''.join(raw[i+16:i+2064] for i in range(0,len(raw),2352))
 return data[inner:inner+size]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('family',choices=DISCS);p.add_argument('disc',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 ident,sha1,sector,size=DISCS[a.family]
 if hashlib.file_digest(a.disc.open('rb'),'sha1').hexdigest()!=sha1:raise ValueError('Disc version/hash differs from supported original')
 archive=RaMix(data=read_raw(a.disc,sector*2048,size),name=a.family+'/MAIN.MIX');out=a.out/a.family;out.mkdir(parents=True,exist_ok=True)
 for name in ['GENERAL.MIX','SOUNDS.MIX','SCORES.MIX']:
  data=archive.read(name)
  if data is None:raise ValueError('Missing original '+name)
  (out/name).write_bytes(data)
 if a.family=='aftermath':
  # Original NEW-file payloads in PATCH.RTP are uncompressed. Offsets also
  # match OpenRA's original-disc recipe; supported disc identity is checked above.
  patch=read_raw(a.disc,31383*2048,5330398)
  for name,offset,length in [('EXPAND2.MIX',4712984,469922),('HIRES1.MIX',5182981,90264),('LORES1.MIX',5273320,57076)]:
   data=patch[offset:offset+length];RaMix(data=data,name=name);(out/name).write_bytes(data)
 receipt={'family':a.family,'source':'https://archive.org/details/'+ident,'disc_sha1':sha1,
          'extractor_reference':'https://github.com/OpenRA/OpenRA/blob/bleed/mods/ra-content/installer/aftermath.yaml',
          'files':{f.name:hashlib.file_digest(f.open('rb'),'sha256').hexdigest() for f in sorted(out.glob('*.MIX'))}}
 (out/'SOURCE-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps(receipt,sort_keys=True))
if __name__=='__main__':main()
