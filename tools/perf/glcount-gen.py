#!/usr/bin/env python3
"""Generate a GL call counter for the Mac renderer (a DYLD_INSERT_LIBRARIES interposer).

    python3 tools/perf/glcount-gen.py OUT.c
    cc -arch arm64 -dynamiclib -O1 -w -fno-omit-frame-pointer -DGL_SILENCE_DEPRECATION \
       -o glcount.dylib OUT.c -framework OpenGL -L<play folder> -lSDL2-2.0.0 \
       -Wl,-undefined,dynamic_lookup
    DYLD_INSERT_LIBRARIES=$PWD/glcount.dylib ./cnc_eyes ... --script s.txt

Every GL 2.1 entry point in the SDK's gl.h is wrapped, both where the renderer calls it
directly and where it fetches it through SDL_GL_GetProcAddress. Each glFinish and each
buffer swap prints one GLC|finish|total=N|glName=count|... line with every function
called since the last one, so the lines between two script `echo` markers add up to that
window's calls exactly. GLC_SITES=1 also notes where every glBegin block is started (two
return addresses) and how many vertices it sent, and prints the 25 largest as GLSITE
lines every 200 flushes; atos -i against a dSYM of the same binary names them.
GLC_OFF=1 loads it and prints nothing.

What it showed first: a 20-building Dawn3D base sends about 50,000 immediate-mode
vertices a frame, most of them from the shroud, the ground mottle and the particles.
Those had already been converted to arrays behind the overlay_arrays dial and measured
on Windows with NVIDIA, where the conversion saved nothing, so a count here is a lead to
be timed on the platform that pays for it, never a saving in itself.
"""
# by name and prints one line per frame (at glFinish and at SDL_GL_SwapWindow).
import re,subprocess,sys
sdk=subprocess.check_output(['xcrun','--show-sdk-path'],text=True).strip()
src=open(sdk+'/System/Library/Frameworks/OpenGL.framework/Headers/gl.h').read()
src=re.sub(r'/\*.*?\*/','',src,flags=re.S)
protos=[]
for m in re.finditer(r'extern\s+([\w\s\*]+?)\s*\b(gl[A-Z]\w*)\s*\(([^)]*)\)\s*(?:OPENGL_\w+(?:\([^)]*\))?\s*)*;',src):
    ret,name,args=m.group(1).strip(),m.group(2),m.group(3).strip()
    if name in ('glFinish',): continue
    protos.append((ret,name,args))
seen=set();P=[]
for p in protos:
    if p[1] in seen: continue
    seen.add(p[1]);P.append(p)
def argnames(args):
    if args in ('void',''): return [],'void'
    parts=[a.strip() for a in args.split(',')]
    names=[];decl=[]
    for i,a in enumerate(parts):
        nm=f'a{i}'
        # strip existing parameter name if present
        mm=re.match(r'(.*?)(\b\w+)\s*(\[\d*\])?$',a)
        t=a
        if mm and mm.group(1).strip() and mm.group(1).strip() not in ('const','unsigned','struct'):
            t=mm.group(1).strip()+((' '+mm.group(3)) if mm.group(3) else '')
            if mm.group(3): t=mm.group(1).strip()+' *'
        decl.append(f'{t} {nm}');names.append(nm)
    return names,', '.join(decl)
out=['#include <OpenGL/gl.h>','#include <stdio.h>','#include <string.h>','#include <stdlib.h>','#include <dlfcn.h>',
 '#define N %d'%(len(P)+1),'static unsigned long long C[N];','static const char* NM[N];',
 'typedef struct { const void* r; const void* o; } Interpose;',
 '#include <dlfcn.h>',
 'typedef struct { void* a; void* b; unsigned long long n, v; } Site; static Site SITES[4096]; static Site* CUR=0;',
 'static void glc_site(void* a, void* b){ unsigned long h=((unsigned long)a*31u ^ (unsigned long)b)%4093; for(int k=0;k<4096;k++){ Site* s=&SITES[(h+k)%4096]; if(!s->a){s->a=a;s->b=b;} if(s->a==a&&s->b==b){s->n++;CUR=s;return;} } CUR=0; }']
for i,(ret,name,args) in enumerate(P):
    names,decl=argnames(args)
    call=', '.join(names)
    rv='' if ret=='void' else 'return '
    if name=='glBegin':
        out.append(f'static {ret} w_{name}({decl}) {{ C[{i}]++; glc_site(__builtin_return_address(0),__builtin_return_address(1)); {rv}{name}({call}); }}')
    elif name.startswith('glVertex'):
        out.append(f'static {ret} w_{name}({decl}) {{ C[{i}]++; if(CUR) CUR->v++; {rv}{name}({call}); }}')
    else:
        out.append(f'static {ret} w_{name}({decl}) {{ C[{i}]++; {rv}{name}({call}); }}')
fin=len(P)
out.append(f'static void glc_flush(const char* why);')
out.append(f'static void w_glFinish(void) {{ C[{fin}]++; glFinish(); glc_flush("finish"); }}')
out.append('struct SDL_Window; extern void SDL_GL_SwapWindow(struct SDL_Window*); extern void* SDL_GL_GetProcAddress(const char*);')
out.append('static void w_swap(struct SDL_Window* w){ SDL_GL_SwapWindow(w); glc_flush("swap"); }')
out.append('typedef struct { const char* n; void* f; } Row; static Row ROWS[]={')
for i,(ret,name,args) in enumerate(P): out.append(f'{{"{name}",(void*)w_{name}}},')
out.append('{"glFinish",(void*)w_glFinish},{0,0}};')
out.append('static void* w_getproc(const char* n){ void* r=SDL_GL_GetProcAddress(n); if(!n||!r) return r;'
           ' char b[96]; size_t l=strlen(n); if(l<sizeof b){ strcpy(b,n); for(const char* sfx[]={"ARB","EXT","APPLE",0},**s=sfx;*s;s++){size_t k=strlen(*s); if(l>k && !strcmp(b+l-k,*s)){b[l-k]=0;break;}} }'
           ' for(Row* x=ROWS;x->n;x++) if(!strcmp(x->n,n)||!strcmp(x->n,b)) return x->f; return r; }')
out.append('static int glc_on=-1;')
out.append('static void glc_flush(const char* why){ if(glc_on<0) glc_on=getenv("GLC_OFF")?0:1; if(!glc_on) return;'
           ' unsigned long long t=0; for(int i=0;i<N;i++) t+=C[i]; if(!t) return;'
           ' if(getenv("GLC_SITES")){ static unsigned fl=0; if(++fl%200==0){ for(int r=0;r<25;r++){ Site* best=0; for(int k=0;k<4096;k++) if(SITES[k].a && SITES[k].v && (!best||SITES[k].v>best->v)) best=&SITES[k]; if(!best) break; Dl_info ia,ib; dladdr(best->a,&ia); dladdr(best->b,&ib); printf("GLSITE|v=%llu|begins=%llu|a=%s+0x%lx|b=%s+0x%lx\\n",best->v,best->n,ia.dli_fname?strrchr(ia.dli_fname,47)+1:"?",(unsigned long)((char*)best->a-(char*)ia.dli_fbase),ib.dli_fname?strrchr(ib.dli_fname,47)+1:"?",(unsigned long)((char*)best->b-(char*)ib.dli_fbase)); best->v=0; best->n=0; } memset(SITES,0,sizeof SITES); CUR=0; } }'
           ' printf("GLC|%s|total=%llu",why,t); for(int i=0;i<N;i++) if(C[i]) printf("|%s=%llu",NM[i],C[i]); printf("\\n"); memset(C,0,sizeof C); }')
out.append('__attribute__((constructor)) static void glc_init(void){')
for i,(ret,name,args) in enumerate(P): out.append(f' NM[{i}]="{name}";')
out.append(f' NM[{fin}]="glFinish"; }}')
out.append('__attribute__((used)) static const Interpose IP[] __attribute__((section("__DATA,__interpose")))={')
for i,(ret,name,args) in enumerate(P): out.append(f'{{(const void*)w_{name},(const void*){name}}},')
out.append('{(const void*)w_glFinish,(const void*)glFinish},{(const void*)w_swap,(const void*)SDL_GL_SwapWindow},{(const void*)w_getproc,(const void*)SDL_GL_GetProcAddress}};')
open(sys.argv[1],'w').write('\n'.join(out)+'\n')
print(len(P),'functions')
