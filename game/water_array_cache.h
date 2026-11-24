#ifndef CNC3D_WATER_ARRAY_CACHE_H
#define CNC3D_WATER_ARRAY_CACHE_H

/* The film and crest still assemble their exact current CPU records. If those
   bytes have not changed, retain their GPU copy instead of resending client
   arrays. Shader time and uniforms stay live. Two slots, a shared 16 MiB CPU
   capacity ceiling and no larger GPU payload; editor/refusal uses client arrays. */
struct WaterArrayCacheEntry {
    GLuint vbo=0;
    std::vector<float> data;
};
static WaterArrayCacheEntry g_waterArrayCache[2];
static bool water_shore_film_allowed(){return !g_editOn;}
static size_t g_waterArrayCacheBytes=0;
static unsigned long long g_waterArrayCacheOffers=0,g_waterArrayCacheHits=0;
static unsigned long long g_waterArrayCacheVerified=0,g_waterArrayCacheBad=0,g_waterArrayCacheFallbacks=0;
static int water_array_cache_mode()
{
    static const int mode=[](){
        const char* p=getenv("OPENCNC3D_WATER_ARRAY_CACHE");
        if(p)return !strcmp(p,"0")?0:!strcmp(p,"verify")?2:1;
#ifdef _WIN32
        return 1; /* Native CPU-bound gain; Mac keeps its measured path. */
#else
        return 0;
#endif
    }();
    return mode;
}
static void water_array_cache_drop(WaterArrayCacheEntry& entry)
{
    if(entry.vbo)gr_glDeleteBuffers(1,&entry.vbo);
    entry.vbo=0;g_waterArrayCacheBytes-=entry.data.capacity()*sizeof(float);
    std::vector<float>().swap(entry.data);
}
static void water_array_cache_free()
{
    for(auto& entry:g_waterArrayCache)water_array_cache_drop(entry);
}
static void water_array_cache_dump()
{
    printf("WATERARRAYCACHE|offers=%llu|hits=%llu|verified=%llu|bad=%llu|fallbacks=%llu|bytes=%zu\n",
        g_waterArrayCacheOffers,g_waterArrayCacheHits,g_waterArrayCacheVerified,g_waterArrayCacheBad,
        g_waterArrayCacheFallbacks,g_waterArrayCacheBytes);
}
static GLuint water_array_cached_vbo(int slot,const float* data,size_t count)
{
    const int mode=water_array_cache_mode();
    if(!mode || g_editOn || slot<0 || slot>=2 || !count)return 0;
    g_waterArrayCacheOffers++;
    if(!mesh_perf_switch("OPENCNC3D_WATER_ARRAY_LOAD_OK")){g_waterArrayCacheFallbacks++;return 0;}
    auto& entry=g_waterArrayCache[slot];
    const size_t cap=mesh_perf_switch("OPENCNC3D_WATER_ARRAY_BUDGET_FULL")?16u*1024u*1024u:256u;
    const size_t oldBytes=entry.data.capacity()*sizeof(float);
    if(count>cap/sizeof(float) || g_waterArrayCacheBytes-oldBytes>cap-count*sizeof(float)) {
        g_waterArrayCacheFallbacks++;return 0;
    }
    if(entry.vbo && entry.data.size()==count && !memcmp(data,entry.data.data(),count*sizeof(float))) {
        if(mode==2) {
            try {
                std::vector<float> actual(count);
                g_waterArrayCacheVerified++;
                if(!mesh_buffer_read(entry.vbo,count*sizeof(float),actual.data())
                    || memcmp(data,actual.data(),count*sizeof(float)))g_waterArrayCacheBad++;
            } catch(const std::bad_alloc&) {g_waterArrayCacheBad++;}
            return 0; /* Diagnostic image retains the original client-array draw. */
        }
        g_waterArrayCacheHits++;return entry.vbo;
    }
    if(!mesh_perf_switch("OPENCNC3D_WATER_ARRAY_ALLOC_OK")){g_waterArrayCacheFallbacks++;return 0;}
    try {
        std::vector<float> next(data,data+count);
        const size_t bytes=next.capacity()*sizeof(float);
        if(bytes>cap || g_waterArrayCacheBytes-oldBytes>cap-bytes){g_waterArrayCacheFallbacks++;return 0;}
        const GLuint bound=gls_array_binding();
        if(entry.vbo && bound==entry.vbo){g_waterArrayCacheFallbacks++;return 0;}
        GLuint nextVbo=0;gr_glGenBuffers(1,&nextVbo);
        bool ready=nextVbo && mesh_perf_switch("OPENCNC3D_WATER_ARRAY_STORE_OK");
        if(ready) {
            /* A new name has no old store that an allocation failure could
               silently leave behind with the same size and stale contents. */
            gr_glBindBuffer(GL_ARRAY_BUFFER,nextVbo);
            gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)(count*sizeof(float)),data,GL_STATIC_DRAW);
            GLint got=-1;gr_glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&got);
            ready=got>=0 && (size_t)got==count*sizeof(float)
                && mesh_perf_switch("OPENCNC3D_WATER_ARRAY_CHECK_OK");
        }
        gr_glBindBuffer(GL_ARRAY_BUFFER,bound);
        if(!ready) {if(nextVbo)gr_glDeleteBuffers(1,&nextVbo);g_waterArrayCacheFallbacks++;return 0;}
        if(entry.vbo)gr_glDeleteBuffers(1,&entry.vbo);
        entry.vbo=nextVbo;
        entry.data.swap(next);g_waterArrayCacheBytes=g_waterArrayCacheBytes-oldBytes+bytes;
        return mode==2?0:entry.vbo;
    } catch(const std::bad_alloc&) {g_waterArrayCacheFallbacks++;return 0;}
}

static bool water_array_snapshot_enabled()
{
    return !g_editOn&&water_array_cache_mode()!=0&&grass_buf_load()
        &&mesh_perf_switch("OPENCNC3D_WATER_ARRAY_LOAD_OK");
}
static bool water_array_snapshot(int slot,const float*& data,size_t& count,GLuint& vbo,bool direct)
{
    if(slot<0||slot>=2||(direct&&water_array_cache_mode()!=1))return false;
    const auto& entry=g_waterArrayCache[slot];
    if(!entry.vbo||entry.data.empty())return false;
    data=entry.data.data();count=entry.data.size();vbo=entry.vbo;return true;
}
#endif
