#!/usr/bin/env python3
"""Build pinned map archive readers; all dependencies stay in private build output."""
import argparse, hashlib, json, os, shutil, subprocess, urllib.request
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('platform',choices=['mac','windows']);p.add_argument('--cache',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];out=(a.output or root/'build-native/map-archive'/a.platform).resolve();cache=(a.cache or root/'build-native/map-archive/cache').resolve();out.mkdir(parents=True,exist_ok=True);cache.mkdir(parents=True,exist_ok=True)
versions=[('libarchive','3.8.10','https://github.com/libarchive/libarchive/releases/download/v3.8.10/libarchive-3.8.10.tar.xz','56cad765459d5ee25a3b761f90c47d99379d28297fbfb72d908880d6b875fb96'),('xz','5.8.2','https://github.com/tukaani-project/xz/releases/download/v5.8.2/xz-5.8.2.tar.xz','890966ec3f5d5cc151077879e157c0593500a522f413ac50ba26d22a9a145214')]
def call(args):subprocess.run(list(map(str,args)),check=True)
for name,version,url,digest in versions:
 archive=cache/(name+'-'+version+'.tar.xz')
 if not archive.exists():
  # Python from python.org carries no certificate store until its own installer step is
  # run, and then refuses every HTTPS fetch. curl uses the system's store; the digest below
  # still decides whether the bytes are the pinned release.
  try:urllib.request.urlretrieve(url,archive)
  except OSError:
   archive.unlink(missing_ok=True);call(['curl','-fsSL','--retry','3','-o',archive,url])
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==digest,archive
 if not (cache/(name+'-'+version)).is_dir():call(['tar','-xf',archive,'-C',cache])
lib=cache/'libarchive-3.8.10';xz=cache/'xz-5.8.2'
common=['-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF','-DCMAKE_POSITION_INDEPENDENT_CODE=ON']
opts=['-DENABLE_TEST=OFF','-DENABLE_TAR=OFF','-DENABLE_CPIO=OFF','-DENABLE_CAT=OFF','-DENABLE_UNZIP=OFF','-DENABLE_INSTALL=OFF','-DENABLE_LIBB2=OFF','-DENABLE_OPENSSL=OFF','-DENABLE_MBEDTLS=OFF','-DENABLE_NETTLE=OFF','-DENABLE_LIBXML2=OFF','-DENABLE_EXPAT=OFF','-DENABLE_BZip2=OFF','-DENABLE_LZ4=OFF','-DENABLE_ZSTD=OFF','-DENABLE_LZO=OFF','-DENABLE_ICONV=OFF','-DENABLE_ACL=OFF','-DENABLE_XATTR=OFF','-DENABLE_PCREPOSIX=OFF','-DENABLE_PCRE2POSIX=OFF']
if a.platform=='mac':
 sdk=subprocess.check_output(['xcrun','--show-sdk-path'],text=True).strip()
 # SDK libraries keep the universal reader independent of Homebrew dylibs.
 platform=['-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64','-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0','-DCMAKE_OSX_SYSROOT='+sdk]
 xz_build=out/'xz-build';call(['cmake','-S',xz,'-B',xz_build,*common,*platform,'-DBUILD_SHARED_LIBS=OFF','-DXZ_TOOL_XZ=OFF','-DXZ_TOOL_XZDEC=OFF','-DXZ_TOOL_LZMADEC=OFF','-DXZ_TOOL_LZMAINFO=OFF','-DXZ_TOOL_SCRIPTS=OFF','-DXZ_DOC=OFF','-DXZ_NLS=OFF'])
 call(['cmake','--build',xz_build,'--target','liblzma','-j','4'])
 archive_build=out/'archive-build';call(['cmake','-S',lib,'-B',archive_build,*common,*platform,*opts,'-DENABLE_LZMA=ON','-DLIBLZMA_INCLUDE_DIR='+str(xz/'src/liblzma/api'),'-DLIBLZMA_LIBRARY='+str(xz_build/'liblzma.a'),'-DZLIB_LIBRARY='+sdk+'/usr/lib/libz.tbd','-DZLIB_INCLUDE_DIR='+sdk+'/usr/include'])
 call(['cmake','--build',archive_build,'--target','archive','archive_static','-j','4'])
 binary=out/'libmaparchive.dylib';shutil.copy2(archive_build/'libarchive/libarchive.dylib',binary);call(['install_name_tool','-id','@executable_path/libmaparchive.dylib',binary]);call(['codesign','-s','-','--force',binary])
else:
 host=os.environ.get('OPENCNC3D_WIN_HOST','x86_64-w64-mingw32');sdk=Path(os.environ.get('OPENCNC3D_WINSDK',str(Path.home()/'.opencnc3d-winsdk')))
 platform=['-DCMAKE_SYSTEM_NAME=Windows','-DCMAKE_C_COMPILER='+host+'-gcc','-DCMAKE_RC_COMPILER='+host+'-windres','-DCMAKE_C_FLAGS=-DLZMA_API_STATIC','-DCMAKE_SHARED_LINKER_FLAGS=-static -static-libgcc']
 xz_build=out/'xz-build';call(['cmake','-S',xz,'-B',xz_build,*common,*platform,'-DBUILD_SHARED_LIBS=OFF','-DXZ_TOOL_XZ=OFF','-DXZ_TOOL_XZDEC=OFF','-DXZ_TOOL_LZMADEC=OFF','-DXZ_TOOL_LZMAINFO=OFF','-DXZ_TOOL_SCRIPTS=OFF','-DXZ_DOC=OFF','-DXZ_NLS=OFF'])
 call(['cmake','--build',xz_build,'--target','liblzma','-j','4'])
 # libarchive targets Windows 10 unless told otherwise, and that build calls CreateFile2, a
 # Windows 8 function: the game and the diagnostics program, which link this library, then
 # import it and Windows 7 refuses to load them at all. WIN7 makes it call CreateFileW, the
 # same desktop call. Given to libarchive only; xz has no such setting.
 archive_build=out/'archive-build';call(['cmake','-S',lib,'-B',archive_build,*common,*platform,*opts,'-DWINDOWS_VERSION=WIN7','-DENABLE_LZMA=ON','-DLIBLZMA_INCLUDE_DIR='+str(xz/'src/liblzma/api'),'-DLIBLZMA_LIBRARY='+str(xz_build/'liblzma.a'),'-DZLIB_LIBRARY='+str(sdk/'lib'/host/'libz.a'),'-DZLIB_INCLUDE_DIR='+str(sdk/'include')])
 call(['cmake','--build',archive_build,'--target','archive','archive_static','-j','4'])
 found=list(archive_build.rglob('*archive*.dll'));assert len(found)==1,found
 binary=out/'archive.dll';shutil.copy2(found[0],binary)
static=archive_build/'libarchive'/('libarchive.a' if a.platform=='mac' else 'libarchive_static.a')
assert static.is_file(),static
shutil.copy2(static,out/'libmaparchive.a');shutil.copy2(xz_build/'liblzma.a',out/'libmaplzma.a')
shutil.copy2(lib/'COPYING',out/'COPYING-libarchive');shutil.copy2(xz/'COPYING',out/'COPYING-xz');shutil.copy2(xz/'COPYING.0BSD',out/'COPYING-xz-0BSD')
(out/'manifest.json').write_text(json.dumps(dict(platform=a.platform,sources=[dict(name=n,version=v,url=u,sha256=h) for n,v,u,h in versions],binary=str(binary),sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),static_inputs={n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ['libmaparchive.a','libmaplzma.a']}),indent=2)+'\n')
print('Map archive dependency built:',binary)
