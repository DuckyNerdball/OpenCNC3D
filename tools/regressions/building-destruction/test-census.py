#!/usr/bin/env python3
"""Compile the actual resource census with inert GL/scene declarations."""
import argparse,hashlib,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[3]
fixture=(repo/'tools/regressions/resource-preparation/preparation-test.cpp').read_text().split('#ifdef TEST_WINDOWS_HEADROOM')[0]
fixture=fixture.replace('int texOwner=-1,','float param[1]={-1};int texOwner=-1,').replace('bool orphan=false,shown=true;','bool orphan=false,shown=true,effectsOnly=false;')
source=(repo/'game/ebld_prepare.h').read_text();function=source.split('static std::map<int, uint64_t> ebld_prepare_required_census(void)',1)[1].split('static void ebld_prepare_required_loading_impl',1)[0]
extra='''
#include <cassert>
const int EBP_SCORCH_TEX=0;
bool ebld_packless_type(const char*){return false;}
bool ebld_type_has_file(const char*){return false;}
uint64_t ebld_texture_bit(const EbldModel& m,int slot){return slot>=0 && slot<64 && (size_t)slot<m.tex.size()?uint64_t(1)<<slot:0;}
'''
main='''
int main(){
 EbldModel model;model.tex.resize(4);model.param[0]=2;g_ebld.push_back(model);
 EbldBld overflow;overflow.stage=EB_WRECKED;overflow.effectsOnly=true;g_ebldBld[1]=overflow;
 auto census=ebld_prepare_required_census();assert(census.size()==1 && census[0]==4);
 EbldBld full=overflow;full.effectsOnly=false;g_ebldBld[2]=full;
 census=ebld_prepare_required_census();assert(census[0]==6); // union with real wreck texture
 g_ebldBld.erase(2);g_ebld[0].param[0]=-1;
 census=ebld_prepare_required_census();assert(census[0]==0); // native mark, no authored texture
 g_ebld[0].param[0]=2;g_ebldBld[1].shown=false;
 assert(ebld_prepare_required_census().empty());
 g_ebldBld[1].shown=true;EbldModel alias=model;alias.texOwner=0;alias.param[0]=3;g_ebld.push_back(alias);g_ebldBld[1].model=1;
 census=ebld_prepare_required_census();assert(census.size()==1 && census[0]==8); // exact slot on owner
 g_ebldBld[1].effectsOnly=false;
 census=ebld_prepare_required_census();assert(census.size()==1 && census[0]==15); // full shared-owner fallback unchanged
 puts("PASS: six actual census admission/union/visibility/shared-owner checks");
}
'''
cpp=a.out/'census.cpp';cpp.write_text(fixture+extra+'static std::map<int, uint64_t> ebld_prepare_required_census(void)'+function+main)
exe=a.out/'census';subprocess.run(['clang++','-std=c++14',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe.resolve())],check=True)
(a.out/'source.sha256').write_text(hashlib.sha256(source.encode()).hexdigest()+'  game/ebld_prepare.h\n')
