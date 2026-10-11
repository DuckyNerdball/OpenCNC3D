/* Enhanced remaster submission. Keep the existing CPU vertex equations exactly;
   cache their finished world-space output instead of changing shader coordinates.
   Runs retain texture/wrap/order, including partial construction and loose pieces.
   Tier 1 uses its separate renderer. Classic and unavailable VBOs retain the
   original path. The environment switches are diagnostic A/B controls only. */
struct MeshBatchVertex {
    float p[3], uv[2], n[3];
    unsigned char c[4];
};
struct MeshBatchRun { int tex, wrap; bool x2; size_t first, count; };
struct MeshBatchEntry {
    GLuint vbo = 0;
    size_t bytes = 0;
    unsigned used = 0;
    float lo[3], hi[3];
    std::vector<MeshBatchRun> runs;
    MeshBatchVertex last;
    bool uv = false;
};
static std::map<std::string, MeshBatchEntry> g_meshBatchCache;
static size_t g_meshBatchBytes = 0;
static GLuint g_meshBatchStream = 0;
/* Animated output survives only for the current draw_frame. Recycle separate
   orphaned stores on the next frame instead of overwriting one in-flight store
   between every object. Both this pool and the static cache have hard bounds. */
struct MeshFrameStore { GLuint vbo=0; size_t bytes=0; };
static std::vector<MeshFrameStore> g_meshFrameStores;
static std::map<std::string,MeshBatchEntry> g_meshFrameCache;
static unsigned g_meshFrameSerial=~0u;
static size_t g_meshFrameNext=0, g_meshFrameBytes=0;
static unsigned long g_meshFrameHits=0;
static const MeshBatchEntry* mesh_batch_frame_find(const std::string& key) {
    if(g_meshFrameSerial!=g_drawFrameSerial) {
        g_meshFrameCache.clear(); g_meshFrameNext=0; g_meshFrameSerial=g_drawFrameSerial;
    }
    auto it=g_meshFrameCache.find(key);
    return it==g_meshFrameCache.end()?NULL:&it->second;
}
static GLuint mesh_batch_frame_store(size_t bytes) {
    const size_t cap=64u*1024u*1024u;
    if(g_meshFrameNext>=2048 || bytes>cap) return 0;
    if(g_meshFrameNext==g_meshFrameStores.size()) g_meshFrameStores.push_back({});
    auto& s=g_meshFrameStores[g_meshFrameNext];
    if(g_meshFrameBytes-s.bytes+bytes>cap) return 0;
    if(!s.vbo) gr_glGenBuffers(1,&s.vbo);
    g_meshFrameBytes=g_meshFrameBytes-s.bytes+bytes; s.bytes=bytes;
    g_meshFrameNext++;
    return s.vbo;
}
static bool mesh_perf_switch(const char* name);
static bool g_meshBatchEditor = false;
/* A wreck has hundreds of tiny ranges. Collect one body's loop in its original
   order, upload its unchanged world-space vertices once, and bind the arrays once.
   Only culling/lining offset vary inside these scopes; shader, blend, depth and
   colour masks remain fixed. The caller must end a scope before changing them. */
struct MeshFragmentCommand {
    MeshBatchEntry entry;
    const Pack* src;
    int house, pass;
    bool depth, normals, cull, offset;
};
static bool g_meshFragmentCollect = false;
static std::vector<MeshBatchVertex> g_meshFragmentVertices;
static std::vector<MeshFragmentCommand> g_meshFragmentCommands;
static unsigned long g_meshFragmentFlushes = 0, g_meshFragmentRanges = 0;
static void mesh_fragment_flush(void);
struct MeshFragmentScope {
    bool active;
    MeshFragmentScope() : active(g_fx.enabled && !g_meshBatchEditor
        && mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_BATCH")
        && mesh_perf_switch("OPENCNC3D_MESH_GPU") && grass_buf_load()
        && !g_meshFragmentCollect) {
        if (active) { g_meshFragmentVertices.clear(); g_meshFragmentCommands.clear();
                      g_meshFragmentCollect = true; }
    }
    ~MeshFragmentScope() {
        if (active) { g_meshFragmentCollect = false; mesh_fragment_flush(); }
    }
};
static bool g_meshClassSkip = false;
static bool g_meshBatchRestPose = false;
static unsigned long g_meshBatchHits = 0, g_meshBatchUploads = 0,
                     g_meshBatchCulled = 0, g_meshBatchDraws = 0,
                     g_meshClassSkipped = 0;
static bool mesh_perf_switch(const char* name) {
    const char* v = getenv(name);
    return !v || strcmp(v, "0");
}
static void mesh_batch_clear(void) {
    if (gr_glDeleteBuffers) {
        for (auto& p : g_meshBatchCache)
            if (p.second.vbo) gr_glDeleteBuffers(1, &p.second.vbo);
        for(auto& s:g_meshFrameStores) if(s.vbo) gr_glDeleteBuffers(1,&s.vbo);
        if (g_meshBatchStream) gr_glDeleteBuffers(1, &g_meshBatchStream);
    }
    g_meshBatchCache.clear(); g_meshBatchBytes = 0; g_meshBatchStream = 0;
    g_meshFrameCache.clear(); g_meshFrameStores.clear(); g_meshFrameBytes=0;
    g_meshFragmentVertices.clear(); g_meshFragmentVertices.shrink_to_fit();
    g_meshFragmentCommands.clear(); g_meshFragmentCommands.shrink_to_fit();
    g_meshFragmentCollect = false;
    g_meshFrameSerial=~0u; g_meshFrameNext=0;
}
/* Reject only when every corner lies outside one homogeneous clip plane.
   These are the CURRENT matrices: camera, mirrored water camera or sun camera.
   No assumption about ground height, anchor visibility or terrain cull bounds. */
static bool mesh_batch_outside(const MeshBatchEntry& e, const float* clip = NULL) {
    if (!mesh_perf_switch("OPENCNC3D_MESH_CULL")) return false;
    float m[16], p[16], c[16];
    if (clip) memcpy(c, clip, sizeof c);
    else {
    glGetFloatv(GL_MODELVIEW_MATRIX, m); glGetFloatv(GL_PROJECTION_MATRIX, p);
    for (int col = 0; col < 4; col++) for (int row = 0; row < 4; row++) {
        c[col*4+row] = 0;
        for (int k = 0; k < 4; k++) c[col*4+row] += p[k*4+row] * m[col*4+k];
    }
    }
    for (int axis = 0; axis < 3; axis++) for (int sign = -1; sign <= 1; sign += 2) {
        float a[4];
        for (int k = 0; k < 4; k++) a[k] = c[k*4+3] + sign*c[k*4+axis];
        /* Positive support point of the world-space box, with a small conservative
           margin for matrix roundoff. Non-finite bounds never reject geometry. */
        float d = a[3], margin = 0.001f;
        for (int k = 0; k < 3; k++) {
            d += a[k] * (a[k] >= 0 ? e.hi[k] : e.lo[k]);
            margin += fabsf(a[k]) * 0.001f;
        }
        if (std::isfinite(d) && d < -margin) return true;
    }
    return false;
}
static void mesh_batch_x2(bool on, GLint env) {
    if (!on) {
        glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env);
        return;
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
}
static void mesh_batch_draw(const MeshBatchEntry& e, const Pack& src, int house,
                            int pass, bool depth, bool normals, const MeshBatchVertex* client = NULL) {
    if (e.runs.empty()) return;
    const bool outside = mesh_batch_outside(e);
    if (outside) g_meshBatchCulled++;
    GLint env; glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &env);
    GLint bound=0; glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&bound);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    gr_glBindBuffer(GL_ARRAY_BUFFER, e.vbo);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(MeshBatchVertex), client ? (const void*)client->p : (const void*)offsetof(MeshBatchVertex,p));
    if (!depth) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(MeshBatchVertex), client ? (const void*)client->c : (const void*)offsetof(MeshBatchVertex,c));
        if (normals) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, sizeof(MeshBatchVertex), client ? (const void*)client->n : (const void*)offsetof(MeshBatchVertex,n));
        } else glDisableClientState(GL_NORMAL_ARRAY);
        if (!g_meshBakePass) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, sizeof(MeshBatchVertex), client ? (const void*)client->uv : (const void*)offsetof(MeshBatchVertex,uv));
        } else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    } else {
        glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    bool x2 = false;
    for (const auto& r : e.runs) {
        if (r.tex == -3) glDisable(GL_TEXTURE_2D);
        else if (!depth) {
            const bool flash = g_meshFlash && pass != MODE_SHADOW && pass != MODE_XLU;
            if (r.tex >= 0 && !flash) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, livery_texture(src.tex[r.tex], house));
                set_wrap((unsigned char)r.wrap);
            } else glDisable(GL_TEXTURE_2D);
        }
        if (r.x2 != x2) { mesh_batch_x2(r.x2, env); x2 = r.x2; }
        if (!outside) { glDrawArrays(GL_TRIANGLES, (GLint)r.first, (GLsizei)r.count); g_meshBatchDraws++; }
    }
    if (x2) mesh_batch_x2(false, env);
    glPopClientAttrib();
    gr_glBindBuffer(GL_ARRAY_BUFFER,(GLuint)bound);
    /* Array draws leave current attributes unspecified. Match the immediate path's
       last emitted attributes, because later legacy draws can inherit them. */
    if (!depth) glColor4ubv(e.last.c);
    if (normals) glNormal3fv(e.last.n);
    if (e.uv) glTexCoord2fv(e.last.uv);
}
/* Replay exactly the same ordered runs, culling each fragment in this pass's
   matrices. Bounds and vertices are the original CPU results, including wind,
   shroud, player paint, clipping, fill planes and the fading depth pre-pass. */
static void mesh_fragment_flush(void) {
    if (g_meshFragmentCommands.empty()) return;
    const size_t bytes = g_meshFragmentVertices.size() * sizeof(MeshBatchVertex);
    GLint bound=0, env=GL_MODULATE;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &env);
    const bool cullWas=glIsEnabled(GL_CULL_FACE), offsetWas=glIsEnabled(GL_POLYGON_OFFSET_FILL);
    GLuint vbo=mesh_batch_frame_store(bytes);
    if (!vbo) { if (!g_meshBatchStream) gr_glGenBuffers(1,&g_meshBatchStream); vbo=g_meshBatchStream; }
    gr_glBindBuffer(GL_ARRAY_BUFFER,vbo);
    gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)bytes,g_meshFragmentVertices.data(),GL_STREAM_DRAW);
    GLint allocated=0; gr_glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&allocated);
    const bool gpu = allocated == (GLint)bytes;
    if (!gpu) gr_glBindBuffer(GL_ARRAY_BUFFER,0); /* preserve the bounded client fallback */
    const MeshBatchVertex* client=gpu ? NULL : g_meshFragmentVertices.data();
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3,GL_FLOAT,sizeof(MeshBatchVertex),client ? (const void*)client->p : (const void*)offsetof(MeshBatchVertex,p));
    const auto& first=g_meshFragmentCommands.front();
    if (!first.depth) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(MeshBatchVertex),client ? (const void*)client->c : (const void*)offsetof(MeshBatchVertex,c));
        if(first.normals) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT,sizeof(MeshBatchVertex),client ? (const void*)client->n : (const void*)offsetof(MeshBatchVertex,n));
        } else glDisableClientState(GL_NORMAL_ARRAY);
        if(!g_meshBakePass) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2,GL_FLOAT,sizeof(MeshBatchVertex),client ? (const void*)client->uv : (const void*)offsetof(MeshBatchVertex,uv));
        } else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    } else {
        glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    float m[16],p[16],clip[16];
    glGetFloatv(GL_MODELVIEW_MATRIX,m); glGetFloatv(GL_PROJECTION_MATRIX,p);
    for(int col=0;col<4;col++) for(int row=0;row<4;row++) {
        clip[col*4+row]=0;
        for(int k=0;k<4;k++) clip[col*4+row]+=p[k*4+row]*m[col*4+k];
    }
    bool cull=cullWas, offset=offsetWas, x2=false;
    for(const auto& cmd:g_meshFragmentCommands) {
        if(cmd.cull!=cull) { cull=cmd.cull; if(cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE); }
        if(cmd.offset!=offset) { offset=cmd.offset; if(offset) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL); }
        const bool outside=mesh_batch_outside(cmd.entry,clip);
        if(outside) g_meshBatchCulled++;
        for(const auto& r:cmd.entry.runs) {
            if(r.tex==-3) glDisable(GL_TEXTURE_2D);
            else if(!cmd.depth) {
                const bool flash=g_meshFlash && cmd.pass!=MODE_SHADOW && cmd.pass!=MODE_XLU;
                if(r.tex>=0 && !flash) { glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D,livery_texture(cmd.src->tex[r.tex],cmd.house));
                    set_wrap((unsigned char)r.wrap);
                } else glDisable(GL_TEXTURE_2D);
            }
            if(r.x2!=x2) { mesh_batch_x2(r.x2,env); x2=r.x2; }
            if(!outside) { glDrawArrays(GL_TRIANGLES,(GLint)r.first,(GLsizei)r.count); g_meshBatchDraws++; }
        }
    }
    if(x2) mesh_batch_x2(false,env);
    if(cull!=cullWas) { if(cullWas) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE); }
    if(offset!=offsetWas) { if(offsetWas) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL); }
    glPopClientAttrib(); gr_glBindBuffer(GL_ARRAY_BUFFER,(GLuint)bound);
    const auto& last=g_meshFragmentCommands.back();
    if(!last.depth) glColor4ubv(last.entry.last.c);
    if(last.normals) glNormal3fv(last.entry.last.n);
    /* An untextured last range inherits the last textured range's current UV. */
    for(auto it=g_meshFragmentCommands.rbegin();it!=g_meshFragmentCommands.rend();++it)
        if(it->entry.uv) { glTexCoord2fv(it->entry.last.uv); break; }
    g_meshBatchUploads++; g_meshFragmentFlushes++;
    g_meshFragmentRanges+=(unsigned long)g_meshFragmentCommands.size();
    g_meshFragmentVertices.clear(); g_meshFragmentCommands.clear();
}
struct MeshBatch {
    std::vector<MeshBatchVertex> vertices;
    MeshBatchEntry entry;
    MeshBatchVertex current = {};
    size_t first = 0;
    int tex = -2, wrap = -1; bool x2 = false;
    void begin(int t, int w, bool x) { first = vertices.size(); tex=t; wrap=w; x2=x; }
    void end() {
        if (vertices.size() > first) entry.runs.push_back({tex,wrap,x2,first,vertices.size()-first});
    }
    void uv(float u, float v) { current.uv[0]=u; current.uv[1]=v; entry.uv=true; }
    void normal(float x,float y,float z) { current.n[0]=x;current.n[1]=y;current.n[2]=z; }
    void color(unsigned char r,unsigned char g,unsigned char b,unsigned char a) {
        current.c[0]=r;current.c[1]=g;current.c[2]=b;current.c[3]=a;
    }
    void vertex(float x,float y,float z) {
        current.p[0]=x;current.p[1]=y;current.p[2]=z;
        for(int k=0;k<3;k++) {
            if(vertices.empty()) entry.lo[k]=entry.hi[k]=current.p[k];
            else {entry.lo[k]=std::min(entry.lo[k],current.p[k]);entry.hi[k]=std::max(entry.hi[k],current.p[k]);}
        }
        vertices.push_back(current);
    }
    void finish(const std::string& key, const Pack& src, int house,int pass,bool depth,bool normals,bool frame=false) {
        if(vertices.empty()) return;
        entry.last=vertices.back(); entry.bytes=vertices.size()*sizeof(MeshBatchVertex);
        if (g_meshFragmentCollect) {
            /* Bound scratch storage. An unusually large asset drains in ordered chunks. */
            if (g_meshFragmentVertices.size()*sizeof(MeshBatchVertex)+entry.bytes > 64u*1024u*1024u)
                mesh_fragment_flush();
            MeshFragmentCommand cmd;
            cmd.entry=entry; cmd.src=&src; cmd.house=house; cmd.pass=pass;
            cmd.depth=depth; cmd.normals=normals;
            cmd.cull=glIsEnabled(GL_CULL_FACE); cmd.offset=glIsEnabled(GL_POLYGON_OFFSET_FILL);
            for(auto& r:cmd.entry.runs) r.first+=g_meshFragmentVertices.size();
            g_meshFragmentVertices.insert(g_meshFragmentVertices.end(),vertices.begin(),vertices.end());
            g_meshFragmentCommands.push_back(std::move(cmd));
            return;
        }
        GLint bound=0; glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&bound);
        const bool keep=!key.empty() && !frame;
        bool frameStore=false;
        if(keep) gr_glGenBuffers(1,&entry.vbo);
        else {
            if(frame) {entry.vbo=mesh_batch_frame_store(entry.bytes);frameStore=entry.vbo!=0;}
            if(!entry.vbo) {if(!g_meshBatchStream) gr_glGenBuffers(1,&g_meshBatchStream);entry.vbo=g_meshBatchStream;}
        }
        gr_glBindBuffer(GL_ARRAY_BUFFER,entry.vbo);
        gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)entry.bytes,vertices.data(),keep?GL_STATIC_DRAW:GL_STREAM_DRAW);
        g_meshBatchUploads++;
        GLint allocated=0; gr_glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&allocated);
        if (allocated != (GLint)entry.bytes) {
            gr_glDeleteBuffers(1,&entry.vbo); entry.vbo=0;
            if(frameStore) {
                auto& s=g_meshFrameStores[g_meshFrameNext-1];
                g_meshFrameBytes-=s.bytes; s.bytes=0; s.vbo=0;
            } else if(!keep) g_meshBatchStream=0;
        }
        mesh_batch_draw(entry,src,house,pass,depth,normals,entry.vbo?NULL:vertices.data());
        gr_glBindBuffer(GL_ARRAY_BUFFER,(GLuint)bound);
        if(frameStore && entry.vbo) g_meshFrameCache[key]=entry;
        if(keep && entry.vbo) {
            const size_t cap=128u*1024u*1024u;
            while(!g_meshBatchCache.empty() && (g_meshBatchBytes+entry.bytes>cap || g_meshBatchCache.size()>=2048)) {
                auto oldest=g_meshBatchCache.begin();
                for(auto it=g_meshBatchCache.begin();it!=g_meshBatchCache.end();++it)
                    if(it->second.used<oldest->second.used) oldest=it;
                g_meshBatchBytes-=oldest->second.bytes;gr_glDeleteBuffers(1,&oldest->second.vbo);
                g_meshBatchCache.erase(oldest);
            }
            if(entry.bytes<=cap) {entry.used=g_drawFrameSerial;g_meshBatchBytes+=entry.bytes;g_meshBatchCache[key]=entry;}
            else gr_glDeleteBuffers(1,&entry.vbo);
        }
    }
};
template<class T> static void mesh_batch_key(std::string& key,const T& v) {
    key.append((const char*)&v,sizeof v);
}
static void mesh_batch_dump(void) {
    printf("MESHPERF|hits=%lu|uploads=%lu|culled=%lu|draws=%lu|class_skip=%lu|entries=%zu|bytes=%zu|pose_hits=%lu|pose_entries=%zu|pose_bytes=%zu|fragment_flushes=%lu|fragment_ranges=%lu\n",
      g_meshBatchHits,g_meshBatchUploads,g_meshBatchCulled,g_meshBatchDraws,g_meshClassSkipped,
      g_meshBatchCache.size(),g_meshBatchBytes,g_meshFrameHits,g_meshFrameCache.size(),g_meshFrameBytes,g_meshFragmentFlushes,g_meshFragmentRanges);
}
