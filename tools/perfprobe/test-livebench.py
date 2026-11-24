#!/usr/bin/env python3
"""Compile the actual livebench command against a controlled game loop.

Checks pipeline selection, restoration (including loop errors), input bounds and
the report consumed by the Windows probe, without creating a game window.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    source = (root / "game/cnc_eyes.cpp").read_text()
    start = source.index('    } else if (!strcmp(cmd, "livebench")) {')
    end = source.index('    } else if (!strcmp(cmd, "meshcaptest")) {', start)
    branch = source[start:end]
    cpp = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
typedef unsigned long long Uint64;
static int options=1,*g_scriptOpts=&options,g_scriptFails=0,g_engineFrame=10;
static int g_fxTaaSync=1,calls=0,loopResult=0,seenSync=-1,flushed=0;
static double g_run_seconds=77,seenSeconds=0;
static const int GAME_EXIT_ERROR=-1;
static Uint64 SDL_GetPerformanceCounter(){static Uint64 t=0;return t+=1000;}
static double fprof_now_ms(Uint64 a,Uint64 b){return double(b-a);}
static void fprof_flush(){++flushed;}
static int game_loop(void*,int*){
    ++calls;seenSync=g_fxTaaSync;seenSeconds=g_run_seconds;g_engineFrame+=15;
    return loopResult;
}
static void run(const char* arg){void* win=nullptr;const char* cmd="livebench";
    if(false){
BRANCH
    }
}
int main(){
    int cases=0;
    for(int before=0;before<=1;++before)for(const char* mode:{"1","1 play","1 capture"}){
        g_fxTaaSync=before;int n=calls,f=flushed,fail=g_scriptFails;
        run(mode);
        assert(calls==n+1&&flushed==f+1&&g_scriptFails==fail);
        assert(seenSeconds==1&&seenSync==(strstr(mode,"capture")!=nullptr));
        assert(g_fxTaaSync==before&&g_run_seconds==77);++cases;
    }
    loopResult=GAME_EXIT_ERROR;g_fxTaaSync=1;int fail=g_scriptFails;
    run("0.25");assert(g_scriptFails==fail+1&&g_fxTaaSync==1&&g_run_seconds==77);++cases;
    loopResult=0;
    for(const char* bad:{"","nan","inf","-inf","0","-1","1800.1","1 typo","1 play extra","1 capture extra"}){
        int n=calls,f=flushed,fail=g_scriptFails;run(bad);
        assert(calls==n&&flushed==f&&g_scriptFails==fail+1);
        assert(g_fxTaaSync==1&&g_run_seconds==77);++cases;
    }
    g_scriptOpts=nullptr;int n=calls;fail=g_scriptFails;
    run("1");assert(calls==n&&g_scriptFails==fail+1);++cases;
    g_scriptOpts=&options;run("1800 capture");assert(seenSeconds==1800);++cases;
    printf("LIVEBENCH_TEST|cases=%d|pass\n",cases);
}
'''.replace("BRANCH", branch).replace("#include <cassert>", "#include <cassert>\n#include <initializer_list>")
    with tempfile.TemporaryDirectory(prefix="cnc3d-livebench-") as tmp:
        path = Path(tmp) / "livebench.cpp"
        path.write_text(cpp)
        binary = Path(tmp) / "livebench"
        subprocess.run([args.compiler, "-std=c++14", "-O2", str(path), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
