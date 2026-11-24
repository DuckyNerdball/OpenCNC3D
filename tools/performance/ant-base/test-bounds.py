#!/usr/bin/env python3
"""Exercise actual body bounds and quaternion helpers against transformed vertices."""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
s=(a.repo/'game/ebld_mod.h').read_text()
def function(name):
 start=s.index('static ',s.index(name)-20);brace=s.index('{',s.index(name,start));i=brace+1;depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
code=r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <vector>
struct PackVert {float x,y,z;};struct PackTri{PackVert v[3];};
struct PackMesh{std::vector<PackTri> tris;bool gpuDeformed=false;std::vector<int>sway;int animFrames=0;mutable std::map<std::array<int,7>,float>gpuBodyRadius;};
struct Pack{std::vector<PackMesh>mesh;}g_pack;
struct EbldPiece{float c[3];};
struct Fx{bool enabled=true;}g_fx;static bool g_meshBatchEditor=false;
static bool debug=false,enabled=true;static bool debug_meshes_on(){return debug;}static bool ant_perf_switch(const char*){return enabled;}
static float MODEL_SCALE=1.0f/1024.0f,recordLo[3],recordHi[3];static int calls=0;
static bool mesh_box_outside(const float*lo,const float*hi){calls++;memcpy(recordLo,lo,12);memcpy(recordHi,hi,12);return true;}
static unsigned long long g_ebldBodyBoundsTests=0,g_ebldBodyBoundsCulled=0;
enum {EBR_A=0,EBR_B,EBR_TEX,EBR_RA,EBR_RB,EBR_FA,EBR};
'''+function('ebld_body_outside(')+'\n'+function('eb_qnorm(')+'\n'+function('eb_qmat(')+r'''
static unsigned seed=982451653;static float randomValue(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return ((int)(seed%200001)-100000)*.01f;}
int main(){g_pack.mesh.resize(1);auto&m=g_pack.mesh[0];m.tris.resize(4);int ranges[EBR]={0,2,0,2,4,-1};unsigned long checks=0;
for(int sample=0;sample<20000;sample++){
 m.gpuBodyRadius.clear();EbldPiece piece;float p[3];for(int k=0;k<3;k++){piece.c[k]=randomValue();p[k]=randomValue();}
 for(auto&t:m.tris)for(auto&v:t.v){v.x=randomValue();v.y=randomValue();v.z=randomValue();}
 if(!ebld_body_outside(0,ranges,piece,p,3e38f))return 1;
 float lo[3],hi[3];memcpy(lo,recordLo,12);memcpy(hi,recordHi,12);
 for(int rotation=0;rotation<16;rotation++){
  float q[4];for(float&v:q)v=randomValue();eb_qnorm(q);float R[9],Mx[12];eb_qmat(q,R);
  for(int k=0;k<3;k++){for(int c=0;c<3;c++)Mx[k*4+c]=R[k*3+c];Mx[k*4+3]=-(R[k*3]*piece.c[0]+R[k*3+1]*piece.c[1]+R[k*3+2]*piece.c[2]);}
  for(const auto&t:m.tris)for(const auto&v:t.v){const float local[3]={v.x,v.y,v.z};for(int k=0;k<3;k++){float world=(Mx[k*4]*local[0]+Mx[k*4+1]*local[1]+Mx[k*4+2]*local[2]+Mx[k*4+3])*MODEL_SCALE+p[k];if(!(world>=lo[k]&&world<=hi[k]))return 2;checks++;}}
 }
 // Same source ranges must retain the same conservative answer on cache reuse.
 int before=calls;if(!ebld_body_outside(0,ranges,piece,p,3e38f)||calls!=before+1||memcmp(lo,recordLo,12)||memcmp(hi,recordHi,12))return 3;
}
EbldPiece piece={{0,0,0}};float p[3]={0,0,0};auto fallback=[&](){int before=calls;return !ebld_body_outside(0,ranges,piece,p,3e38f)&&before==calls;};
m.gpuDeformed=true;if(!fallback())return 4;m.gpuDeformed=false;m.sway.push_back(1);if(!fallback())return 5;m.sway.clear();m.animFrames=1;if(!fallback())return 6;m.animFrames=0;debug=true;if(!fallback())return 7;debug=false;enabled=false;if(!fallback())return 8;enabled=true;
if(ebld_body_outside(0,ranges,piece,p,0))return 9;p[0]=std::numeric_limits<float>::quiet_NaN();if(!fallback())return 10;
printf("BODYBOUNDS|checks=%lu|failures=0|actual_helpers=1\n",checks);return 0;}
'''
source=a.out/'bounds.cpp';source.write_text(code);exe=a.out/'bounds';subprocess.run(['clang++','-std=c++14','-O2',str(source),'-o',str(exe)],check=True);r=subprocess.run([str(exe)],text=True,capture_output=True,check=True);(a.out/'results.txt').write_text(r.stdout);print(r.stdout,end='')

# A deliberately undersized radius must fail the independent transformed-vertex oracle.
mutated=code.replace('*1.001f+0.02f','*0.1f+0.02f');assert mutated!=code
source.write_text(mutated);subprocess.run(['clang++','-std=c++14','-O2',str(source),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)]);assert r.returncode==2,'Bounds negative control did not fail'
source.write_text(code)
print('BODYBOUNDS|negative_control=detected')
