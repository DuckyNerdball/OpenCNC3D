/* Private diagnostic: draw rigid wreck glow from immutable local vertices.
   No transform-feedback work occurs in the fast path. The verifier captures the
   actual vertex outputs from both the new local source and the original CPU
   world-space stream and compares every byte; it renders the original path.
   Unsupported contexts, source effects and allocation failures retain that path. */
#ifndef CNC3D_MESH_DIRECT_GLOW_H
#define CNC3D_MESH_DIRECT_GLOW_H
static int mesh_direct_glow_mode() {
    static const int mode=[](){const char* p=getenv("OPENCNC3D_MESH_DIRECT_GLOW");
#if defined(_WIN32)
        if(!p)return 1;
#endif
        return p&&!strcmp(p,"verify")?2:p&&strcmp(p,"0")?1:0;}();return mode;
}
static unsigned long long g_mdgOffers=0,g_mdgDraws=0,g_mdgVertices=0,g_mdgChecks=0,
    g_mdgBad=0,g_mdgRefused=0,g_mdgBuilds=0;
static unsigned long long g_mdgDifferent[13]={};
static GLuint g_mdgProgram=0,g_mdgFeedback=0,g_mdgReference=0;
static bool g_mdgTried=false;

static GLint g_mdgDirect=-1,g_mdgPlace=-1,g_mdgLift=-1,g_mdgExtra=-1,g_mdgRow[3]={-1,-1,-1};
typedef void(APIENTRY* MDGBegin)(GLenum);
typedef void(APIENTRY* MDGEnd)();
typedef void(APIENTRY* MDGBindBase)(GLenum,GLuint,GLuint);
typedef void(APIENTRY* MDGVary)(GLuint,GLsizei,const char*const*,GLenum);
typedef void(APIENTRY* MDGRead)(GLenum,ptrdiff_t,ptrdiff_t,void*);
typedef void(APIENTRY* MDGGetIndexed)(GLenum,GLuint,GLint*);
static MDGBegin mdg_begin;static MDGEnd mdg_end;static MDGBindBase mdg_bind_base;
static MDGVary mdg_vary;static MDGRead mdg_read;static MDGGetIndexed mdg_get_indexed;
static const char* MDG_VS=
    "#version 400 compatibility\n"
    "uniform int uDirect;uniform vec4 uPlace,uLift,uExtra,uRow0,uRow1,uRow2;uniform vec4 uAlpha[64];\n"
    "precise out vec3 vWorld;out vec2 vUV;out vec4 vColour,vClip;\n"
    "void main(){\n"
    "float vx=gl_Vertex.x,vy=gl_Vertex.y,vz=gl_Vertex.z;\n"
    "if(uDirect!=0){\n"
    "if(uExtra.x>0.5){\n"
    "float bx=uRow0.x*vx+uRow0.y*vy+uRow0.z*vz+uRow0.w;\n"
    "float by=uRow1.x*vx+uRow1.y*vy+uRow1.z*vz+uRow1.w;\n"
    "float bz=uRow2.x*vx+uRow2.y*vy+uRow2.z*vz+uRow2.w;vx=bx;vy=by;vz=bz;}\n"
    "float mx=vx*uPlace.z+vz*uPlace.w,mz=-vx*uPlace.w+vz*uPlace.z;\n"
    "vWorld=vec3(uPlace.x+mx*uLift.x,vy*uLift.x+0.0+uLift.y+uLift.z,uPlace.y+mz*uLift.x);\n"
    "int alpha=int(gl_Color.a*255.0+0.5);alpha=(alpha*int(uExtra.y))/255;\n"
    "vColour=vec4(gl_Color.rgb,uAlpha[alpha/4][alpha%4]);\n"
    "}else{vWorld=gl_Vertex.xyz;vColour=gl_Color;}\n"
    "vUV=gl_MultiTexCoord0.xy;gl_FrontColor=vColour;gl_BackColor=vColour;\n"
    "gl_FrontSecondaryColor=vec4(0.0);gl_BackSecondaryColor=vec4(0.0);\n"
    "gl_TexCoord[0]=gl_TextureMatrix[0]*gl_MultiTexCoord0;\n"
    "vec4 world=vec4(vWorld,1.0);gl_ClipVertex=gl_ModelViewMatrix*world;\n"
    "vClip=gl_ModelViewProjectionMatrix*world;gl_Position=vClip;}\n";
struct MdgSource {MeshBatchEntry entry;size_t bytes=0;unsigned used=0;};
static std::unordered_map<MeshKey,MdgSource,MeshKeyHash> g_mdgSources;
static size_t g_mdgBytes=0;static unsigned g_mdgGeneration=0;
struct MdgCheck {float p[3],uv[2],colour[4],clip[4];};
static_assert(sizeof(MdgCheck)==13*sizeof(float),"transform feedback layout");
struct MeshDirectGlowWitness {std::vector<MdgCheck> gpu;bool active=false;};
static void mesh_direct_glow_sources_clear() {
    if(gr_glDeleteBuffers)for(auto& p:g_mdgSources)if(p.second.entry.vbo)gr_glDeleteBuffers(1,&p.second.entry.vbo);
    g_mdgSources.clear();g_mdgBytes=0;g_mdgGeneration=g_groundGen;
    if(gr_glDeleteBuffers){if(g_mdgFeedback)gr_glDeleteBuffers(1,&g_mdgFeedback);if(g_mdgReference)gr_glDeleteBuffers(1,&g_mdgReference);}
    g_mdgFeedback=g_mdgReference=0;
}
static void mesh_direct_glow_clear() {
    mesh_direct_glow_sources_clear();
    if(g_mdgProgram&&fx_glDeleteProgram)fx_glDeleteProgram(g_mdgProgram);
    g_mdgProgram=0;g_mdgTried=false;
}
static bool mesh_direct_glow_program() {
    if(g_mdgTried)return g_mdgProgram!=0;g_mdgTried=true;
    const char* version=(const char*)glGetString(GL_VERSION);int major=0;
    if(version)sscanf(version,"%d",&major);
    if(major<4||!fx_gl_ready||!mesh_perf_switch("OPENCNC3D_MESH_DIRECT_GLOW_LOAD_OK"))return false;
    mdg_begin=(MDGBegin)SDL_GL_GetProcAddress("glBeginTransformFeedback");
    mdg_end=(MDGEnd)SDL_GL_GetProcAddress("glEndTransformFeedback");
    mdg_bind_base=(MDGBindBase)SDL_GL_GetProcAddress("glBindBufferBase");
    mdg_vary=(MDGVary)SDL_GL_GetProcAddress("glTransformFeedbackVaryings");
    mdg_read=(MDGRead)SDL_GL_GetProcAddress("glGetBufferSubData");
    mdg_get_indexed=(MDGGetIndexed)SDL_GL_GetProcAddress("glGetIntegeri_v");
    if(!mdg_begin||!mdg_end||!mdg_bind_base||!mdg_vary||!mdg_read||!mdg_get_indexed)return false;
    GLuint vs=fx_compile(GL_VERTEX_SHADER,MDG_VS,"direct-wreck-glow");if(!vs)return false;
    GLuint p=fx_glCreateProgram();fx_glAttachShader(p,vs);
    const char* outputs[]={"vWorld","vUV","vColour","vClip"};mdg_vary(p,4,outputs,0x8C8C);
    fx_glLinkProgram(p);fx_glDeleteShader(vs);GLint ok=0;fx_glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]={};GLsizei n=0;fx_glGetProgramInfoLog(p,sizeof log-1,&n,log);fprintf(stderr,"MESHDIRECTGLOW|link-failed|%s\n",log);fx_glDeleteProgram(p);return false;}
    /* Match the UBYTE vertex attribute's correctly rounded normalization. A
       float reciprocal multiply gives some opacities one extra ULP on NVIDIA.
       Compute the exact 8-bit ratios in double and store their rounded floats
       once; the vertex stage selects the same value the original byte emitted. */
    GLint previous=0;glGetIntegerv(GL_CURRENT_PROGRAM,&previous);fx_glUseProgram(p);
    float alpha[256];for(int i=0;i<256;++i)alpha[i]=(float)((double)i/255.0);
    const GLint table=fx_glGetUniformLocation(p,"uAlpha");
    if(table<0){fx_glUseProgram((GLuint)previous);fx_glDeleteProgram(p);return false;}
    fx_glUniform4fv(table,64,alpha);fx_glUseProgram((GLuint)previous);
    g_mdgProgram=p;g_mdgDirect=fx_glGetUniformLocation(p,"uDirect");g_mdgPlace=fx_glGetUniformLocation(p,"uPlace");
    g_mdgLift=fx_glGetUniformLocation(p,"uLift");g_mdgExtra=fx_glGetUniformLocation(p,"uExtra");
    for(int i=0;i<3;++i){char name[16];snprintf(name,sizeof name,"uRow%d",i);g_mdgRow[i]=fx_glGetUniformLocation(p,name);}
    return true;
}
static void mdg_uniforms(float cx,float cz,float rc,float rs,float ground,float lift,const float* extra) {
    fx_glUniform1i(g_mdgDirect,1);
    const float place[]={cx,cz,rc,rs},h[]={MODEL_SCALE,lift,ground,0},e[]={extra?1.0f:0.0f,(float)g_meshAlphaScale,0,0};
    fx_glUniform4fv(g_mdgPlace,1,place);fx_glUniform4fv(g_mdgLift,1,h);fx_glUniform4fv(g_mdgExtra,1,e);
    if(extra)for(int i=0;i<3;++i)fx_glUniform4fv(g_mdgRow[i],1,extra+i*4);
}
static MdgSource* mdg_source(const Pack& src,const PackMesh& mesh,size_t first,size_t last,int pass,bool x2,
                            const MeshPreparedEntry& prepared) {
    if(g_mdgGeneration!=g_groundGen)mesh_direct_glow_sources_clear();
    MeshKey key;const void* data=mesh.tris.data();mesh_batch_key(key,data);mesh_batch_key(key,mesh.tris.size());
    mesh_batch_key(key,&src);mesh_batch_key(key,first);mesh_batch_key(key,last);mesh_batch_key(key,pass);mesh_batch_key(key,x2);
    auto found=g_mdgSources.find(key);
    if(found!=g_mdgSources.end()){found->second.used=g_drawFrameSerial;return &found->second;}
    const size_t bytes=prepared.flat.size()*sizeof(MeshBatchVertex)+prepared.runs.size()*sizeof(MeshBatchRun)+sizeof(MeshKey)+256;
    if(g_mdgSources.size()>=4096||bytes>32u*1024u*1024u-g_mdgBytes
        ||!mesh_perf_switch("OPENCNC3D_MESH_DIRECT_GLOW_SOURCE_OK"))return NULL;
    MdgSource fresh;fresh.bytes=bytes;fresh.used=g_drawFrameSerial;
    try {fresh.entry.runs=prepared.runs;}catch(const std::bad_alloc&){return NULL;}
    GLuint was=gls_array_binding();gr_glGenBuffers(1,&fresh.entry.vbo);gr_glBindBuffer(GL_ARRAY_BUFFER,fresh.entry.vbo);
    const size_t size=prepared.flat.size()*sizeof(MeshBatchVertex);
    gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)size,prepared.flat.data(),GL_STATIC_DRAW);
    bool ok=mesh_store_holds(fresh.entry.vbo,size,false)&&mesh_perf_switch("OPENCNC3D_MESH_DIRECT_GLOW_STORE_OK");
    gr_glBindBuffer(GL_ARRAY_BUFFER,was);
    if(!ok){if(fresh.entry.vbo)gr_glDeleteBuffers(1,&fresh.entry.vbo);return NULL;}
    fresh.entry.bytes=size;fresh.entry.uv=prepared.uv;
    try {auto slot=g_mdgSources.emplace(key,std::move(fresh));g_mdgBytes+=bytes;++g_mdgBuilds;return &slot.first->second;}
    catch(const std::bad_alloc&){gr_glDeleteBuffers(1,&fresh.entry.vbo);return NULL;}
}
static bool mdg_capture(GLuint input,size_t count,std::vector<MdgCheck>& output) {
    if(count>(16u*1024u*1024u)/sizeof(MdgCheck))return false;
    try {output.resize(count);}catch(const std::bad_alloc&){return false;}
    const size_t bytes=count*sizeof(MdgCheck);GLuint was=gls_array_binding();
    GLint general=0,indexed=0;glGetIntegerv(0x8C8F,&general);mdg_get_indexed(0x8C8F,0,&indexed);
    const bool discarded=glIsEnabled(0x8C89)==GL_TRUE;
    if(!g_mdgFeedback)gr_glGenBuffers(1,&g_mdgFeedback);
    gr_glBindBuffer(GL_ARRAY_BUFFER,g_mdgFeedback);gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)bytes,NULL,GL_STREAM_DRAW);
    const bool ok=mesh_store_holds(g_mdgFeedback,bytes,false);
    if(ok){
        glPushAttrib(GL_CURRENT_BIT);glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        gr_glBindBuffer(GL_ARRAY_BUFFER,input);glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3,GL_FLOAT,sizeof(MeshBatchVertex),(void*)offsetof(MeshBatchVertex,p));
        glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(MeshBatchVertex),(void*)offsetof(MeshBatchVertex,c));
        glTexCoordPointer(2,GL_FLOAT,sizeof(MeshBatchVertex),(void*)offsetof(MeshBatchVertex,uv));
        glEnable(0x8C89);mdg_bind_base(0x8C8E,0,g_mdgFeedback);mdg_begin(GL_POINTS);glDrawArrays(GL_POINTS,0,(GLsizei)count);mdg_end();
        mdg_bind_base(0x8C8E,0,(GLuint)indexed);gr_glBindBuffer(0x8C8E,(GLuint)general);
        if(!discarded)glDisable(0x8C89);
        glPopClientAttrib();glPopAttrib();gr_glBindBuffer(GL_ARRAY_BUFFER,g_mdgFeedback);
        mdg_read(GL_ARRAY_BUFFER,0,(ptrdiff_t)bytes,output.data());
    }
    gr_glBindBuffer(GL_ARRAY_BUFFER,was);return ok;
}
static bool mesh_direct_glow_draw(const Pack& src,const PackMesh& mesh,size_t first,size_t last,int pass,int house,
        bool x2,float cx,float cz,float rc,float rs,float ground,float lift,const float* extra,
        MeshDirectGlowWitness& witness) {
    ++g_mdgOffers;
    if(!mesh_direct_glow_program()){++g_mdgRefused;return false;}
    GLint caller=0;glGetIntegerv(GL_CURRENT_PROGRAM,&caller);
    if(caller||glIsEnabled(GL_LIGHTING)||glIsEnabled(GL_FOG)){++g_mdgRefused;return false;}
    const MeshPreparedEntry* prepared=mesh_prepared_source(src,mesh,first,last,pass,false,false,x2,false,true);
    if(!prepared||!prepared->compatible||prepared->flat.empty()||!prepared->boundsReady||!prepared->boundsFinite){++g_mdgRefused;return false;}
    MdgSource* source=mdg_source(src,mesh,first,last,pass,x2,*prepared);
    if(!source){++g_mdgRefused;return false;}
    float lo[3]={NAN,NAN,NAN},hi[3]={NAN,NAN,NAN};mesh_source_outside(mesh,cx,cz,ground,lift,rc,rs,extra,false,0,1,0,1,lo,hi,prepared->lo,prepared->hi);
    for(int k=0;k<3;++k)if(!std::isfinite(lo[k])||!std::isfinite(hi[k])){++g_mdgRefused;return false;}
    if(!mesh_early_replay(source->entry,src,mesh,first,last,pass,false,false,x2,extra,rc,rs,false,0,1,0,1,lo,hi,prepared)){++g_mdgRefused;return false;}
    fx_glUseProgram(g_mdgProgram);mdg_uniforms(cx,cz,rc,rs,ground,lift,extra);
    const size_t count=prepared->flat.size();
    if(mesh_direct_glow_mode()==2){witness.active=mdg_capture(source->entry.vbo,count,witness.gpu);fx_glUseProgram((GLuint)caller);return false;}
    mesh_batch_draw(source->entry,src,house,pass,false,false);
    fx_glUseProgram((GLuint)caller);
    ++g_mdgDraws;g_mdgVertices+=count;return true;
}
static void mesh_direct_glow_compare(MeshDirectGlowWitness& witness,const MeshBatch& original) {
    if(!witness.active)return;
    ++g_mdgChecks;bool good=witness.gpu.size()==original.vertices.size();
    std::vector<MdgCheck> reference;
    if(good){
        const GLuint was=gls_array_binding();GLint caller=0;glGetIntegerv(GL_CURRENT_PROGRAM,&caller);
        if(!g_mdgReference)gr_glGenBuffers(1,&g_mdgReference);
        gr_glBindBuffer(GL_ARRAY_BUFFER,g_mdgReference);const size_t bytes=original.vertices.size()*sizeof(MeshBatchVertex);
        gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)bytes,original.vertices.data(),GL_STREAM_DRAW);
        good=mesh_store_holds(g_mdgReference,bytes,false);
        if(good){fx_glUseProgram(g_mdgProgram);fx_glUniform1i(g_mdgDirect,0);good=mdg_capture(g_mdgReference,original.vertices.size(),reference);}
        fx_glUseProgram((GLuint)caller);gr_glBindBuffer(GL_ARRAY_BUFFER,was);
    }
    if(good)good=!memcmp(reference.data(),witness.gpu.data(),reference.size()*sizeof(MdgCheck));
    if(!good){
        if(reference.size()==witness.gpu.size())for(size_t i=0;i<reference.size();++i) {
            float a[13],b[13];memcpy(a,&reference[i],sizeof a);memcpy(b,&witness.gpu[i],sizeof b);
            for(int k=0;k<13;++k)if(memcmp(a+k,b+k,sizeof(float))) {
                ++g_mdgDifferent[k];
                if(g_mdgBad<4&&g_mdgDifferent[k]<5){unsigned ab,bb;memcpy(&ab,a+k,4);memcpy(&bb,b+k,4);
                    fprintf(stderr,"MESHDIRECTGLOW|difference|vertex=%zu|field=%d|old=%.9g|gpu=%.9g|old_bits=%08x|gpu_bits=%08x\n",i,k,a[k],b[k],ab,bb);}
            }
        }
        ++g_mdgBad;if(g_mdgBad<8)fprintf(stderr,"MESHDIRECTGLOW|mismatch|vertices=%zu\n",original.vertices.size());
    }
}
static void mesh_direct_glow_dump() {
    printf("MESHDIRECTGLOWDIFF");for(int i=0;i<13;++i)printf("|f%d=%llu",i,g_mdgDifferent[i]);puts("");
    printf("MESHDIRECTGLOW|mode=%d|program=%u|offers=%llu|draws=%llu|vertices=%llu|verified=%llu|bad=%llu|refused=%llu|builds=%llu|bytes=%zu\n",
        mesh_direct_glow_mode(),g_mdgProgram,g_mdgOffers,g_mdgDraws,g_mdgVertices,g_mdgChecks,g_mdgBad,g_mdgRefused,g_mdgBuilds,g_mdgBytes);
}
#endif
