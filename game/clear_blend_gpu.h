#ifndef CNC3D_CLEAR_BLEND_GPU_H
#define CNC3D_CLEAR_BLEND_GPU_H
/* The same world-space client arrays, retained on the card. No shader, material,
   topology or vertex arithmetic changes; only changed float ranges are uploaded. */
#include "lowmem.h"
typedef void(APIENTRY* CbgGen)(GLsizei,GLuint*);
typedef void(APIENTRY* CbgDelete)(GLsizei,const GLuint*);
typedef void(APIENTRY* CbgBind)(GLenum,GLuint);
typedef void(APIENTRY* CbgData)(GLenum,FxGLsizeiptr,const void*,GLenum);
typedef void(APIENTRY* CbgSub)(GLenum,FxGLsizeiptr,FxGLsizeiptr,const void*);
typedef void(APIENTRY* CbgSize)(GLenum,GLenum,GLint*);
typedef void(APIENTRY* CbgRead)(GLenum,FxGLsizeiptr,FxGLsizeiptr,void*);
static CbgGen cbgGen;static CbgDelete cbgDelete;static CbgBind cbgBind;
static CbgData cbgData;static CbgSub cbgSub;static CbgSize cbgSize;static CbgRead cbgRead;
static const GLenum CBG_ARRAY=0x8892,CBG_DYNAMIC=0x88E8,CBG_SIZE=0x8764;
static unsigned long long g_cbGpuDraws=0,g_cbGpuUploads=0,g_cbGpuUpdates=0,g_cbGpuBytes=0,g_cbGpuVerified=0,g_cbGpuBad=0,g_cbGpuRefused=0;
static int clear_blend_gpu_mode(){static const int mode=[](){const char* p=getenv("OPENCNC3D_CLEARBLEND_VBO");return !p?1:!strcmp(p,"0")?0:!strcmp(p,"verify")?2:1;}();return mode;}
static void* cbg_proc(const char* name,const char* fallback){void* p=SDL_GL_GetProcAddress(name);return p?p:SDL_GL_GetProcAddress(fallback);}
static bool cbg_load(){static const bool ready=[](){
    if(!mesh_perf_switch("OPENCNC3D_CLEARBLEND_VBO_LOAD_OK"))return false;
    cbgGen=(CbgGen)cbg_proc("glGenBuffers","glGenBuffersARB");cbgDelete=(CbgDelete)cbg_proc("glDeleteBuffers","glDeleteBuffersARB");
    cbgBind=(CbgBind)cbg_proc("glBindBuffer","glBindBufferARB");cbgData=(CbgData)cbg_proc("glBufferData","glBufferDataARB");
    cbgSub=(CbgSub)cbg_proc("glBufferSubData","glBufferSubDataARB");cbgSize=(CbgSize)cbg_proc("glGetBufferParameteriv","glGetBufferParameterivARB");
    cbgRead=(CbgRead)cbg_proc("glGetBufferSubData","glGetBufferSubDataARB");
    return cbgGen&&cbgDelete&&cbgBind&&cbgData&&cbgSub&&cbgSize;
}();return ready;}
static void cbg_bind(GLuint vbo){cbgBind(CBG_ARRAY,vbo);g_glsArray=vbo;g_glsArrayKnown=true;}
static void clear_blend_gpu_release(ClearBlendState& B,bool failed=false){
    if(cbgDelete)for(int k=0;k<4;k++)if(B.gpu[k])cbgDelete(1,&B.gpu[k]);
    for(int k=0;k<4;k++){B.gpu[k]=0;B.gpuSize[k]=B.gpuLo[k]=B.gpuHi[k]=0;}
    B.gpuFailed=failed;g_glsArrayKnown=false;
}
static void clear_blend_gpu_dirty(ClearBlendState& B,int field,size_t lo,size_t hi){
    if(B.gpuLo[field]==B.gpuHi[field]){B.gpuLo[field]=lo;B.gpuHi[field]=hi;}
    else{B.gpuLo[field]=std::min(B.gpuLo[field],lo);B.gpuHi[field]=std::max(B.gpuHi[field],hi);}
}
static bool clear_blend_gpu_upload(ClearBlendState& B){
    if(!clear_blend_gpu_mode()||B.gpuFailed)return false;
    if(!cbg_load()){B.gpuFailed=true;g_cbGpuRefused++;return false;}
    const std::vector<float>* data[4]={&B.geometry,&B.colours,&B.maskUV,&B.groundEdges};
    const size_t cap=32u*1024u*1024u;size_t total=0;bool rebuild=false;
    for(int k=0;k<4;k++){
        if(data[k]->size()>cap/sizeof(float)||data[k]->size()*sizeof(float)>cap-total){B.gpuFailed=true;g_cbGpuRefused++;return false;}
        const size_t bytes=data[k]->size()*sizeof(float);total+=bytes;
        if(!B.gpu[k]||B.gpuSize[k]!=bytes)rebuild=true;
    }
    const GLuint before=gls_array_binding();
    const auto refused=[&](){clear_blend_gpu_release(B,true);cbg_bind(before);g_cbGpuRefused++;return false;};
    if(rebuild){
        clear_blend_gpu_release(B);
        LowmemNeed need;need.buffer=total;
        if(!mesh_perf_switch("OPENCNC3D_CLEARBLEND_VBO_ALLOC_OK")||!lowmem_fits(need,"clear_blend_buffers",LOWMEM_RESERVE_REQUIRED,true))return refused();
        cbgGen(4,B.gpu);
        for(int k=0;k<4;k++){
            if(!B.gpu[k])return refused();
            cbg_bind(B.gpu[k]);const size_t bytes=data[k]->size()*sizeof(float);
            cbgData(CBG_ARRAY,(FxGLsizeiptr)bytes,data[k]->data(),CBG_DYNAMIC);
            GLint actual=-1;gls_query_note("glGetBufferParameteriv",__FILE__,__LINE__);cbgSize(CBG_ARRAY,CBG_SIZE,&actual);
            if(actual<0||(size_t)actual!=bytes||(k==1&&!mesh_perf_switch("OPENCNC3D_CLEARBLEND_VBO_STORE_OK")))return refused();
            B.gpuSize[k]=bytes;B.gpuLo[k]=B.gpuHi[k]=0;g_cbGpuUploads++;g_cbGpuBytes+=bytes;
        }
        lowmem_charge(need);
    }else for(int k=0;k<4;k++)if(B.gpuLo[k]!=B.gpuHi[k]){
        cbg_bind(B.gpu[k]);const size_t lo=B.gpuLo[k],n=B.gpuHi[k]-lo;
        cbgSub(CBG_ARRAY,(FxGLsizeiptr)(lo*sizeof(float)),(FxGLsizeiptr)(n*sizeof(float)),data[k]->data()+lo);
        B.gpuLo[k]=B.gpuHi[k]=0;g_cbGpuUpdates++;g_cbGpuBytes+=n*sizeof(float);
    }
    if(clear_blend_gpu_mode()==2){
        if(!cbgRead)return refused();
        try{std::vector<float> read;
            for(int k=0;k<4;k++){
                read.resize(data[k]->size());cbg_bind(B.gpu[k]);gls_query_note("glGetBufferSubData",__FILE__,__LINE__);
                cbgRead(CBG_ARRAY,0,(FxGLsizeiptr)B.gpuSize[k],read.data());g_cbGpuVerified++;
                if(memcmp(read.data(),data[k]->data(),B.gpuSize[k])){g_cbGpuBad++;return refused();}
            }
        }catch(const std::bad_alloc&){return refused();}
    }
    cbg_bind(before);return true;
}
static void clear_blend_gpu_dump(){
    size_t retained=0;for(int k=0;k<4;k++)retained+=g_clearBlend.gpuSize[k];
    printf("CLEARBLENDGPU|mode=%d|draws=%llu|uploads=%llu|updates=%llu|upload_bytes=%llu|verified=%llu|bad=%llu|refused=%llu|retained_bytes=%zu\n",
        clear_blend_gpu_mode(),g_cbGpuDraws,g_cbGpuUploads,g_cbGpuUpdates,g_cbGpuBytes,g_cbGpuVerified,g_cbGpuBad,g_cbGpuRefused,retained);
}
#endif
