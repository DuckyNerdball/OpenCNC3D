#ifndef CNC3D_WATER_SHORE_FRAME_CACHE_H
#define CNC3D_WATER_SHORE_FRAME_CACHE_H
/* Exact whole-film reuse. The key contains the ordered visible cells, source UVs,
   atlas ratios, current corner light/fog, coarse/fine choice and the existing
   geometry-generation key. Camera matrices and shader time stay live. A reference
   mode rebuilds the original complete CPU array before comparing a predicted hit.
   A refused underlying GPU store cannot accept a new metadata key. */
struct ShoreFrameInput {
    int x,z,coarse,painted;
    float uv[4],light[4],visible[4],ratio[2];
};
static std::vector<ShoreFrameInput> g_shoreFrameInputs;
static WaterShoreCacheKey g_shoreFrameGeometry;
static bool g_shoreFrameValid=false;
static unsigned long long g_shoreFrameOffers=0,g_shoreFrameHits=0,g_shoreFrameVerified=0,g_shoreFrameBad=0,g_shoreFrameFallbacks=0;
static int shore_frame_mode()
{
    static const int mode=[](){const char* p=getenv("OPENCNC3D_SHORE_FRAME_CACHE");if(p)return !strcmp(p,"0")?0:!strcmp(p,"verify")?2:1;
#ifdef _WIN32
        return 1;
#else
        return 0;
#endif
}();
    return mode;
}
static bool water_array_snapshot(int,const float*&,size_t&,GLuint&,bool);
static bool shore_frame_inputs(std::vector<ShoreFrameInput>& inputs,bool painted,bool ra,bool oneSun,bool forceFine)
{
    const size_t cap=2u*1024u*1024u;
    float ratio[2]={0,0};
    if(painted){const auto& at=water_atlas(water_atlas_index());ratio[0]=ra?1.f:(float)at.uw/at.w;ratio[1]=ra?1.f:(float)at.uh/at.h;}
    if(!mesh_perf_switch("OPENCNC3D_SHORE_FRAME_ALLOC_OK"))return false;
    try {
        inputs.reserve(std::min(water_cells().size(),cap/sizeof(ShoreFrameInput)));
        if(inputs.capacity()>cap/sizeof(ShoreFrameInput))return false;
        for(const auto& c:water_cells()) {
            if(c.x<0||c.y<0||c.x>=g_gridW||c.y>=g_gridH)continue;
            if(!g_waterCoastCell[(size_t)c.y*g_gridW+c.x]||!cell_shown(c.x,c.y)||!cell_in_view(c.x,c.y))continue;
            if(inputs.size()>=cap/sizeof(ShoreFrameInput))return false;
            ShoreFrameInput row={};row.x=c.x;row.z=c.y;row.painted=painted;
            row.uv[0]=c.u0;row.uv[1]=c.u1;row.uv[2]=c.v0;row.uv[3]=c.v1;
            row.ratio[0]=ratio[0];row.ratio[1]=ratio[1];
            const int cx[4]={c.x,c.x+1,c.x,c.x+1},cz[4]={c.y,c.y,c.y+1,c.y+1};float tint[3];
            for(int k=0;k<4;k++)water_terrain_corner(cx[k],cz[k],false,oneSun,&row.light[k],&row.visible[k],tint);
            water_shore_node(c.x,c.y,0,0);
            row.coarse=!forceFine&&g_waterShoreLastCell->coarse&&water_shore_linear_weights(row.light)&&water_shore_linear_weights(row.visible);
            inputs.push_back(row);
            if(inputs.capacity()>cap/sizeof(ShoreFrameInput))return false;
        }
        return true;
    }catch(const std::bad_alloc&){return false;}
}
static bool shore_frame_matches(const std::vector<ShoreFrameInput>& inputs)
{
    return g_shoreFrameValid&&g_waterShoreCacheValid&&g_shoreFrameGeometry==g_waterShoreCacheKey
        &&inputs.size()==g_shoreFrameInputs.size()
        &&(inputs.empty()||!memcmp(inputs.data(),g_shoreFrameInputs.data(),inputs.size()*sizeof(ShoreFrameInput)));
}
static void shore_frame_remember(std::vector<ShoreFrameInput>& inputs)
{
    const float* data=NULL;size_t count=0;GLuint vbo=0;
    if(!water_array_snapshot(0,data,count,vbo,false)||count!=g_waterArrayN
        ||(count&&memcmp(data,g_waterArray.data(),count*sizeof(float)))){g_shoreFrameFallbacks++;return;}
    try{g_shoreFrameGeometry=g_waterShoreCacheKey;g_shoreFrameInputs.swap(inputs);g_shoreFrameValid=true;}
    catch(const std::bad_alloc&){g_shoreFrameValid=false;g_shoreFrameFallbacks++;}
}
static void shore_frame_dump()
{
    printf("SHOREFRAMECACHE|offers=%llu|hits=%llu|verified=%llu|bad=%llu|fallbacks=%llu|bytes=%zu\n",g_shoreFrameOffers,g_shoreFrameHits,g_shoreFrameVerified,g_shoreFrameBad,g_shoreFrameFallbacks,g_shoreFrameInputs.capacity()*sizeof(ShoreFrameInput));
}

#endif
