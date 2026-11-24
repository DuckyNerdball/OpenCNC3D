#!/usr/bin/env python3
"""Stage only a hash-verified archive reader and its notices, without other assets."""
from pathlib import Path
import argparse,hashlib,json,shutil
p=argparse.ArgumentParser();p.add_argument('dependency',type=Path);p.add_argument('runtime',type=Path);a=p.parse_args()
d=a.dependency.resolve();r=a.runtime.resolve();m=json.loads((d/'manifest.json').read_text());name='archive.dll' if m['platform']=='windows' else 'libmaparchive.dylib';b=d/name
assert hashlib.sha256(b.read_bytes()).hexdigest()==m['sha256']
r.mkdir(parents=True,exist_ok=True)
for target in [r/name,r/'map-archive.json',r/'map-archive-LICENSE.txt']:
 assert not target.is_symlink(),target
shutil.copy2(b,r/name);shutil.copy2(d/'manifest.json',r/'map-archive.json')
(r/'map-archive-LICENSE.txt').write_text('OpenCNC3D map archive reader\n\n'+json.dumps(m['sources'],indent=2)+'\n\n'+''.join('\n'+n+'\n'+(d/n).read_text() for n in ['COPYING-libarchive','COPYING-xz','COPYING-xz-0BSD']))
print('Staged map archive reader:',r/name)
