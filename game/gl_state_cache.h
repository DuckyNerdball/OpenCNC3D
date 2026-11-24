/* ---------------------------------------------------------------------------------- *
 *  GL STATE THE RENDERER ALREADY KNOWS, kept on the CPU so it is not asked for.
 *
 *  Every glGet* and glIsEnabled is a question to the driver, and on a driver that runs
 *  its own worker thread (NVIDIA's threaded optimisation on Windows is the common case)
 *  the answer waits until that thread has executed every command queued before it. A
 *  frame that asks hundreds of times serialises the game and the driver hundreds of
 *  times. Measured on the 1995 GDI 1 map under Enhanced: about 1,900 queries a frame,
 *  1,700 of them from the model batcher (mesh_batch.h) asking the same four things for
 *  every model it draws, while two Windows PCs with current NVIDIA cards spent 40 to
 *  50 ms a frame on it with the GPU idle, and the same frame took under 10 ms on a Mac.
 *
 *  Three answers are cached here, each with the rule that makes it stale:
 *
 *    the clip matrix (projection x modelview) the batcher culls against: stale after
 *      anything that changes either matrix, counted by g_glsMatEpoch, which the macros
 *      below bump on every fixed-function matrix call in this translation unit;
 *    GL_CULL_FACE and GL_POLYGON_OFFSET_FILL: set by glEnable/glDisable, stale after
 *      glPopAttrib, which can restore either behind our back;
 *    the GL_ARRAY_BUFFER binding: set by gr_glBindBuffer (grass_bake.h), stale after
 *      glPopClientAttrib and after a glDeleteBuffers of the bound buffer.
 *
 *  More has been added since (the texture units, the client arrays, the bound program,
 *  and the whole of "a frame that asks nothing" below), each under the same rule.
 *
 *  Anything the cache cannot see (another translation unit drawing, a library touching
 *  the context) is covered by gls_frame_begin(), which forgets all of it at the top of a
 *  frame unless the frame before ended in one of this file's own loops with nothing
 *  foreign drawn since (gls_frame_end, gls_forget_foreign). A forgotten answer is asked
 *  for once, then known again: the cache can cost a query, never return a wrong value.
 *
 *  Included after every system GL header, because the macros would otherwise rewrite
 *  the headers' own declarations. Tier 1 is a separate program and never sees this.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_GL_STATE_CACHE_H
#define OPENCNC3D_GL_STATE_CACHE_H

/* The frame profiler (cnc_eyes.cpp, OPENCNC3D_FRAMEPROF), declared here so the post chain
   and the water, which are included before it, can split their sections. */
static void fprof_mark(const char* name);
static bool mesh_perf_switch(const char* name);

/* ---- every question asked of the driver, counted ----
   Each glGet*, glIsEnabled, status check and read-back in this translation unit passes
   through gls_query_note on its way to the driver, so a frame's real round trips can be
   counted (an answer the cache gives itself is not one). The frame profiler prints the
   count beside its own report as GLQUERY|, and OPENCNC3D_GLQUERY_SITES=1 adds one
   GLQUERYSITE| line per asking line of source, with the profiler section it was first
   asked after, which is how the questions a frame still asks are found. */
struct GlsQuerySite { const char* what; const char* file; int line; unsigned long n; const char* after; };
static unsigned long g_glsQueries = 0;          /* real queries since the frame began */
static const char*   g_glsQueryAfter = "frame start";   /* the frame profiler's last mark */
static GlsQuerySite  g_glsQuerySite[160];
static int           g_glsQuerySiteN = 0;
static bool gls_query_sites(void)
{
    static const bool on = [](){ const char* p = getenv("OPENCNC3D_GLQUERY_SITES"); return p && strcmp(p, "0"); }();
    return on;
}
static void gls_query_note(const char* what, const char* file, int line)
{
    g_glsQueries++;
    if (!gls_query_sites()) return;
    for (int i = 0; i < g_glsQuerySiteN; i++) {
        GlsQuerySite& s = g_glsQuerySite[i];
        if (s.line == line && !strcmp(s.what, what) && !strcmp(s.file, file)) { s.n++; return; }
    }
    if (g_glsQuerySiteN < (int)(sizeof g_glsQuerySite / sizeof g_glsQuerySite[0]))
        g_glsQuerySite[g_glsQuerySiteN++] = { what, file, line, 1, g_glsQueryAfter };
}
#define GLS_ASK(what, call) (gls_query_note(what, __FILE__, __LINE__), call)
#define glGetFloatv(p, o)       GLS_ASK("glGetFloatv", glGetFloatv(p, o))
#define glGetBooleanv(p, o)     GLS_ASK("glGetBooleanv", glGetBooleanv(p, o))
#define glGetDoublev(p, o)      GLS_ASK("glGetDoublev", glGetDoublev(p, o))
#define glGetIntegerv(p, o)     GLS_ASK("glGetIntegerv", glGetIntegerv(p, o))
#define glIsEnabled(c)          GLS_ASK("glIsEnabled", glIsEnabled(c))
#define glGetError()            GLS_ASK("glGetError", glGetError())
#define glGetTexEnviv(t, p, o)  GLS_ASK("glGetTexEnviv", glGetTexEnviv(t, p, o))
#define glGetTexEnvfv(t, p, o)  GLS_ASK("glGetTexEnvfv", glGetTexEnvfv(t, p, o))
#define glGetTexParameteriv(t, p, o) GLS_ASK("glGetTexParameteriv", glGetTexParameteriv(t, p, o))
#define glGetTexLevelParameteriv(t, l, p, o) GLS_ASK("glGetTexLevelParameteriv", glGetTexLevelParameteriv(t, l, p, o))
/* a whole texture level copied back is also counted in the memory ledger (memledger.h) */
#define glGetTexImage(t, l, f, y, p) (memledger_readback_note(__FILE__, __LINE__), GLS_ASK("glGetTexImage", glGetTexImage(t, l, f, y, p)))
#define glReadPixels(x, y, w, h, f, t, p) GLS_ASK("glReadPixels", glReadPixels(x, y, w, h, f, t, p))

/* ---- the matrices ---- */
static unsigned g_glsMatEpoch = 1;
static unsigned g_glsClipEpoch = 0;
static float    g_glsClip[16];

static bool gls_query_elide(void);
static bool gls_query_verify(void);

/* WHAT EACH MATRIX IS MADE OF. The driver's matrix is a fixed function of the calls that
   built it since the last load: the same calls with the same arguments give the same
   bits every time. So each of the two stacks the renderer reads (modelview and
   projection) keeps, beside every level, the exact list of calls that made that level,
   argument bits included, and an answer the driver once gave for a list is reused for
   the same list later, in this frame or any other. Nothing is computed on the CPU, so
   no answer can differ from the driver's by rounding; a camera standing still asks
   nothing, and a camera that moved asks once for each new list. A level built from
   something unseen (a call before the first load, a mode that is not known, another
   translation unit) is unknown and is asked for, as before.
   OPENCNC3D_QUERY_ELIDE=0 leaves the lists unused. */
enum { GLS_OP_IDENTITY = 1, GLS_OP_LOADF, GLS_OP_LOADD, GLS_OP_MULTF, GLS_OP_MULTD, GLS_OP_TRANSLATEF,
       GLS_OP_TRANSLATED, GLS_OP_ROTATEF, GLS_OP_ROTATED, GLS_OP_SCALEF, GLS_OP_SCALED, GLS_OP_ORTHO,
       GLS_OP_FRUSTUM };
struct GlsMatLevel { bool known; std::string ops; };
static std::vector<GlsMatLevel> g_glsMatStack[2];          /* [0] modelview, [1] projection */
static bool  g_glsMatModeKnown = false;
static GLint g_glsMatMode = GL_MODELVIEW;
static GLint g_glsMatDepthLimit[2] = { 0, 0 };
static std::unordered_map<std::string, std::vector<GLfloat> > g_glsMatAnswer[2];
static unsigned long g_glsMatReused = 0;
static int gls_mat_stack_of(GLint mode)
{
    return mode == GL_MODELVIEW ? 0 : mode == GL_PROJECTION ? 1 : -1;
}
static void gls_mat_forget(void)
{
    for (int k = 0; k < 2; k++) { g_glsMatStack[k].assign(1, GlsMatLevel()); g_glsMatStack[k][0].known = false; }
    g_glsMatModeKnown = false;
}
/* The stack a matrix call acts on, or -1 for one the lists do not follow. With the mode
   unknown the call could have changed either, so both forget their top. */
static int gls_mat_target(void)
{
    if (g_glsMatStack[0].empty()) gls_mat_forget();
    if (!g_glsMatModeKnown) {
        g_glsMatStack[0].back().known = g_glsMatStack[1].back().known = false;
        return -1;
    }
    return gls_mat_stack_of(g_glsMatMode);
}
static void gls_mat_load(int op, const void* bits, size_t n)
{
    const int k = gls_mat_target();
    if (k < 0) return;
    GlsMatLevel& top = g_glsMatStack[k].back();
    top.known = true;
    top.ops.assign((const char*)&op, sizeof op);
    if (n) top.ops.append((const char*)bits, n);
}
static void gls_mat_apply(int op, const void* bits, size_t n)
{
    const int k = gls_mat_target();
    if (k < 0) return;
    GlsMatLevel& top = g_glsMatStack[k].back();
    if (!top.known) return;
    if (top.ops.size() > 16384) { top.known = false; top.ops.clear(); return; }
    top.ops.append((const char*)&op, sizeof op);
    top.ops.append((const char*)bits, n);
}
static void gls_mat_depth_limits(void)
{
    if (g_glsMatDepthLimit[0]) return;
    glGetIntegerv(GL_MAX_MODELVIEW_STACK_DEPTH, &g_glsMatDepthLimit[0]);
    glGetIntegerv(GL_MAX_PROJECTION_STACK_DEPTH, &g_glsMatDepthLimit[1]);
    if (g_glsMatDepthLimit[0] < 1) g_glsMatDepthLimit[0] = 1;
    if (g_glsMatDepthLimit[1] < 1) g_glsMatDepthLimit[1] = 1;
}
static inline void gls_push_matrix(void)
{
    if (g_glsMatStack[0].empty()) gls_mat_forget();
    const int k = g_glsMatModeKnown ? gls_mat_stack_of(g_glsMatMode) : -1;
    if (!g_glsMatModeKnown) {
        /* Which stack grew is unknown, so neither stack's levels can be trusted. */
        gls_mat_forget();
    } else if (k >= 0) {
        gls_mat_depth_limits();
        if ((GLint)g_glsMatStack[k].size() >= g_glsMatDepthLimit[k]) {
            /* GL refuses a push past its limit; what the next pop leaves is not known. */
            g_glsMatStack[k].assign(1, GlsMatLevel()); g_glsMatStack[k][0].known = false;
        } else {
            g_glsMatStack[k].push_back(g_glsMatStack[k].back());
        }
    }
    (glPushMatrix)();
}
static inline void gls_pop_matrix(void)
{
    g_glsMatEpoch++;
    if (g_glsMatStack[0].empty()) gls_mat_forget();
    const int k = g_glsMatModeKnown ? gls_mat_stack_of(g_glsMatMode) : -1;
    if (!g_glsMatModeKnown) gls_mat_forget();
    else if (k >= 0) {
        if (g_glsMatStack[k].size() > 1) g_glsMatStack[k].pop_back();
        else g_glsMatStack[k][0].known = false;   /* below the first known level */
    }
    (glPopMatrix)();
}
static inline void gls_matrix_mode(GLenum mode)
{
    (glMatrixMode)(mode);
    g_glsMatModeKnown = mode == GL_MODELVIEW || mode == GL_PROJECTION || mode == GL_TEXTURE;
    g_glsMatMode = (GLint)mode;
}
/* The driver's answer for the current top of stack k, from an earlier answer for the same
   list of calls; false when the list is unknown or has never been answered. */
static bool gls_mat_known_answer(int k, GLfloat* out)
{
    if (g_glsMatStack[0].empty()) gls_mat_forget();
    const GlsMatLevel& top = g_glsMatStack[k].back();
    if (!top.known) return false;
    auto it = g_glsMatAnswer[k].find(top.ops);
    if (it == g_glsMatAnswer[k].end()) return false;
    memcpy(out, it->second.data(), 16 * sizeof(GLfloat));
    g_glsMatReused++;
    return true;
}
static void gls_mat_learn(int k, const GLfloat* m)
{
    if (g_glsMatStack[0].empty()) gls_mat_forget();
    const GlsMatLevel& top = g_glsMatStack[k].back();
    if (!top.known) return;
    if (g_glsMatAnswer[k].size() > 8192) g_glsMatAnswer[k].clear();
    g_glsMatAnswer[k][top.ops].assign(m, m + 16);
}
static inline void gls_load_identity(void) { g_glsMatEpoch++; gls_mat_load(GLS_OP_IDENTITY, NULL, 0); (glLoadIdentity)(); }
static inline void gls_load_matrixf(const GLfloat* m) { g_glsMatEpoch++; gls_mat_load(GLS_OP_LOADF, m, 16 * sizeof *m); (glLoadMatrixf)(m); }
static inline void gls_load_matrixd(const GLdouble* m) { g_glsMatEpoch++; gls_mat_load(GLS_OP_LOADD, m, 16 * sizeof *m); (glLoadMatrixd)(m); }
static inline void gls_mult_matrixf(const GLfloat* m) { g_glsMatEpoch++; gls_mat_apply(GLS_OP_MULTF, m, 16 * sizeof *m); (glMultMatrixf)(m); }
static inline void gls_mult_matrixd(const GLdouble* m) { g_glsMatEpoch++; gls_mat_apply(GLS_OP_MULTD, m, 16 * sizeof *m); (glMultMatrixd)(m); }
static inline void gls_translatef(GLfloat x, GLfloat y, GLfloat z)
{ const GLfloat a[3] = { x, y, z }; g_glsMatEpoch++; gls_mat_apply(GLS_OP_TRANSLATEF, a, sizeof a); (glTranslatef)(x, y, z); }
static inline void gls_translated(GLdouble x, GLdouble y, GLdouble z)
{ const GLdouble a[3] = { x, y, z }; g_glsMatEpoch++; gls_mat_apply(GLS_OP_TRANSLATED, a, sizeof a); (glTranslated)(x, y, z); }
static inline void gls_rotatef(GLfloat r, GLfloat x, GLfloat y, GLfloat z)
{ const GLfloat a[4] = { r, x, y, z }; g_glsMatEpoch++; gls_mat_apply(GLS_OP_ROTATEF, a, sizeof a); (glRotatef)(r, x, y, z); }
static inline void gls_rotated(GLdouble r, GLdouble x, GLdouble y, GLdouble z)
{ const GLdouble a[4] = { r, x, y, z }; g_glsMatEpoch++; gls_mat_apply(GLS_OP_ROTATED, a, sizeof a); (glRotated)(r, x, y, z); }
static inline void gls_scalef(GLfloat x, GLfloat y, GLfloat z)
{ const GLfloat a[3] = { x, y, z }; g_glsMatEpoch++; gls_mat_apply(GLS_OP_SCALEF, a, sizeof a); (glScalef)(x, y, z); }
static inline void gls_scaled(GLdouble x, GLdouble y, GLdouble z)
{ const GLdouble a[3] = { x, y, z }; g_glsMatEpoch++; gls_mat_apply(GLS_OP_SCALED, a, sizeof a); (glScaled)(x, y, z); }
static inline void gls_ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{ const GLdouble a[6] = { l, r, b, t, n, f }; g_glsMatEpoch++; gls_mat_apply(GLS_OP_ORTHO, a, sizeof a); (glOrtho)(l, r, b, t, n, f); }
static inline void gls_frustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{ const GLdouble a[6] = { l, r, b, t, n, f }; g_glsMatEpoch++; gls_mat_apply(GLS_OP_FRUSTUM, a, sizeof a); (glFrustum)(l, r, b, t, n, f); }

/* Each is the real call with the epoch bumped first and the call written into the list
   of the level it changes. */
#define glLoadIdentity()          gls_load_identity()
#define glLoadMatrixf(m)          gls_load_matrixf(m)
#define glLoadMatrixd(m)          gls_load_matrixd(m)
#define glMultMatrixf(m)          gls_mult_matrixf(m)
#define glMultMatrixd(m)          gls_mult_matrixd(m)
#define glTranslatef(...) gls_translatef(__VA_ARGS__)
#define glTranslated(...) gls_translated(__VA_ARGS__)
#define glRotatef(...) gls_rotatef(__VA_ARGS__)
#define glRotated(...) gls_rotated(__VA_ARGS__)
#define glScalef(...) gls_scalef(__VA_ARGS__)
#define glScaled(...) gls_scaled(__VA_ARGS__)
#define glOrtho(...) gls_ortho(__VA_ARGS__)
#define glFrustum(...) gls_frustum(__VA_ARGS__)
#define glPopMatrix()             gls_pop_matrix()
#define glPushMatrix()            gls_push_matrix()
#define glMatrixMode(m)           gls_matrix_mode(m)

/* Under verify, a matrix answered from its list is read back from the driver as well. */
static void gls_mat_verify_answer(GLenum which, const GLfloat* m);

/* Projection x modelview, column-major like GL's own, asked for only when a matrix has
   changed since the last answer. Each matrix is the driver's own: from the lists above
   when the same calls were answered before, otherwise asked. */
static const float* gls_clip(void)
{
    if (g_glsClipEpoch != g_glsMatEpoch) {
        float m[16], p[16];
        const bool lists = gls_query_elide();
        if (!lists || !gls_mat_known_answer(0, m)) {
            glGetFloatv(GL_MODELVIEW_MATRIX, m);
            if (lists) gls_mat_learn(0, m);
        } else if (gls_query_verify()) gls_mat_verify_answer(GL_MODELVIEW_MATRIX, m);
        if (!lists || !gls_mat_known_answer(1, p)) {
            glGetFloatv(GL_PROJECTION_MATRIX, p);
            if (lists) gls_mat_learn(1, p);
        } else if (gls_query_verify()) gls_mat_verify_answer(GL_PROJECTION_MATRIX, p);
        for (int col = 0; col < 4; col++) for (int row = 0; row < 4; row++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += p[k * 4 + row] * m[col * 4 + k];
            g_glsClip[col * 4 + row] = s;
        }
        g_glsClipEpoch = g_glsMatEpoch;
    }
    return g_glsClip;
}

/* A draw that owns a modelview push/pop and leaves projection untouched can
   restore the exact previously queried clip answer after popping. No CPU matrix
   mirror or multiplication is introduced. Unknown answers remain unknown. */
struct GlsClipRestore {
    bool known;
    float clip[16];
    GlsClipRestore():known(g_glsClipEpoch==g_glsMatEpoch) {
        if(known)memcpy(clip,g_glsClip,sizeof clip);
    }
    void restored() const {
        if(!known)return;
        memcpy(g_glsClip,clip,sizeof clip);g_glsClipEpoch=g_glsMatEpoch;
    }
};

/* ---- two capabilities ---- */
struct GlsCap { bool known, on; };
static GlsCap g_glsCull = { false, false }, g_glsOffset = { false, false };
static GlsCap g_glsBlend={false,false}, g_glsDepth={false,false}, g_glsAlpha={false,false};
struct GlsTextureUnit { bool boundKnown=false; GLuint bound=0; GlsCap enabled={false,false}; };
static GlsTextureUnit g_glsTexture[32];
static GlsCap g_glsVertex={false,false},g_glsColor={false,false},g_glsNormal={false,false},g_glsTexArray[32];
static bool g_glsClientKnown=false;static GLenum g_glsClient=GL_TEXTURE0;
static bool g_glsHasMultitex=false;
static void gls_texture_limits();
static void gls_forget_clients(){
    g_glsVertex.known=g_glsColor.known=g_glsNormal.known=false;g_glsClientKnown=false;
    for(auto& c:g_glsTexArray)c.known=false;
}
static GlsCap* gls_client_cap(GLenum cap) {
    if(cap==GL_VERTEX_ARRAY)return &g_glsVertex;
    if(cap==GL_COLOR_ARRAY)return &g_glsColor;
    if(cap==GL_NORMAL_ARRAY)return &g_glsNormal;
    if(cap==GL_TEXTURE_COORD_ARRAY) {
        if(!g_glsClientKnown){
            gls_texture_limits();GLint unit=GL_TEXTURE0;
            if(g_glsHasMultitex)glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE,&unit);
            g_glsClient=(GLenum)unit;g_glsClientKnown=true;
        }
        if(g_glsClient>=GL_TEXTURE0&&g_glsClient<GL_TEXTURE0+32)return &g_glsTexArray[g_glsClient-GL_TEXTURE0];
    }
    return NULL;
}
static bool g_glsActiveKnown=false;
static GLenum g_glsActive=GL_TEXTURE0;
static GLint g_glsActiveLimit=0,g_glsImageLimit=0,g_glsClientLimit=0,g_glsFixedLimit=0;
static SDL_GLContext g_glsLimitsContext=NULL;
#ifndef GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#endif
#ifndef GL_MAX_TEXTURE_COORDS
#define GL_MAX_TEXTURE_COORDS 0x8871
#endif
static void gls_texture_limits() {
    if(g_glsFixedLimit)return;
    int major=0,minor=0;const char* v=(const char*)glGetString(GL_VERSION);
    if(v)sscanf(v,"%d.%d",&major,&minor);
    g_glsHasMultitex=major>1||(major==1&&minor>=3);
    if(!g_glsHasMultitex) {
        const char* ext=(const char*)glGetString(GL_EXTENSIONS);
        g_glsHasMultitex=ext&&strstr(ext,"GL_ARB_multitexture");
    }
    g_glsFixedLimit=1;
    if(g_glsHasMultitex)glGetIntegerv(GL_MAX_TEXTURE_UNITS,&g_glsFixedLimit);
    g_glsActiveLimit=g_glsImageLimit=g_glsClientLimit=g_glsFixedLimit;
    if(major>=2) {
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&g_glsImageLimit);
        glGetIntegerv(GL_MAX_TEXTURE_COORDS,&g_glsClientLimit);
        g_glsActiveLimit=std::max(g_glsImageLimit,g_glsClientLimit);
    }
}
static std::unordered_map<uint64_t, GLint> g_glsWrap;
static bool gls_reduce_calls() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_STATE_CACHE");return !p||strcmp(p,"0");}();
    return on;
}

/* ---- a frame that asks nothing ----
   With the driver's own worker thread (NVIDIA's threaded optimisation is the common
   case), the first question after a present waits until that present has gone through,
   which waits for the card to finish the frame before it: the frame then cannot start
   until the last one is done on the card, and CPU and card take turns. Measured on a
   Tiberian Dawn base at 1920x1080 on a Windows PC with a current NVIDIA card: 5.7 ms of
   every frame inside the first glGetIntegerv, and about 130 more questions a frame for
   another millisecond.
   So everything a frame used to ask is either known here or set without asking:
     the texture unit selectors and the buffer bindings are SET at the top of the frame
       (gls_frame_begin), which is a command and never waits;
     the state the renderer saves and puts back around a pass (enables, blend, depth,
       colour mask, clear colour, viewport, scissor, matrix mode, framebuffer, the
       current colour and normal) is shadowed from every call in this translation unit
       that writes it, and glPopAttrib / glPopClientAttrib put back what the matching
       push saw instead of forgetting it;
     the two matrices are answered from the lists of calls that built them (above);
     a framebuffer's completeness is remembered for the attachments it was asked with.
   An answer is given only while it is known to be exact; anything else is asked, once,
   and then known. OPENCNC3D_QUERY_ELIDE=0 asks everything as before; =verify, or
   OPENCNC3D_QUERY_ELIDE_VERIFY=1, asks anyway after every answer and reports any
   disagreement as GLQUERYVERIFY|mismatch lines. */
static bool gls_query_elide(void)
{
    static const bool on = [](){ const char* p = getenv("OPENCNC3D_QUERY_ELIDE"); return !p || strcmp(p, "0"); }();
    return on && gls_reduce_calls();
}
static bool gls_query_verify(void)
{
    static const bool on = [](){
        const char* p = getenv("OPENCNC3D_QUERY_ELIDE_VERIFY");
        const char* q = getenv("OPENCNC3D_QUERY_ELIDE");
        return (p && !strcmp(p, "1")) || (q && !strcmp(q, "verify"));
    }();
    return on && gls_query_elide();
}
static unsigned long g_glsVerifyChecked = 0, g_glsVerifyMismatches = 0;
static void gls_verify_report(const char* what, unsigned pname, const char* file, int line,
                              const char* known, const char* driver)
{
    g_glsVerifyMismatches++;
    if (g_glsVerifyMismatches <= 40)
        printf("GLQUERYVERIFY|mismatch|%s|0x%04X|%s:%d|known=%s|driver=%s\n", what, pname, file, line, known, driver);
}

/* A write the cache skips because it believes GL already holds the value is, under
   verify, checked against the driver too: a stale belief there would leave GL holding
   something else, which no answer to a question would ever show. */
static unsigned long g_glsVerifySkips = 0;
static void gls_mat_verify_answer(GLenum which, const GLfloat* m)
{
    GLfloat d[16];
    (glGetFloatv)(which, d);
    g_glsVerifyChecked++;
    if (!memcmp(d, m, sizeof d)) return;
    char a[64], b[64];
    snprintf(a, sizeof a, "%.9g,%.9g,%.9g,%.9g", m[12], m[13], m[14], m[15]);
    snprintf(b, sizeof b, "%.9g,%.9g,%.9g,%.9g", d[12], d[13], d[14], d[15]);
    gls_verify_report("matrix list answer", which, "clip", 0, a, b);
}
static void gls_verify_skip_i(const char* what, GLenum pname, GLint expect)
{
    GLint d[4] = { 0, 0, 0, 0 };
    (glGetIntegerv)(pname, d);
    g_glsVerifyChecked++; g_glsVerifySkips++;
    if (d[0] == expect) return;
    char a[24], b[24];
    snprintf(a, sizeof a, "%d", expect); snprintf(b, sizeof b, "%d", d[0]);
    gls_verify_report(what, pname, "skipped write", 0, a, b);
}
static void gls_verify_skip_cap(const char* what, GLenum cap, bool on)
{
    const bool d = (glIsEnabled)(cap) == GL_TRUE;
    g_glsVerifyChecked++; g_glsVerifySkips++;
    if (d != on) gls_verify_report(what, cap, "skipped write", 0, on ? "1" : "0", d ? "1" : "0");
}

/* What the frame knows beyond the cache above: one value or vector per piece of state,
   and whether it is known. Each belongs to the attribute groups glPushAttrib saves it
   with, which is how a pop puts it back. */
struct GlsKnown { bool known; GLint i[4]; GLfloat f[4]; };
struct GlsShadow {
    GlsKnown clear, colourMask, blend, blendEq, alphaFunc, alphaRef;   /* colour buffer */
    GlsKnown depthFunc, depthMask;                                      /* depth buffer  */
    GlsKnown viewport;                                                  /* viewport      */
    GlsKnown scissor;                                                   /* scissor       */
    GlsKnown matrixMode;                                                /* transform     */
    GlsKnown colour, normal;                                            /* current       */
};
static GlsShadow g_glsShadow;
static unsigned  g_glsTexGenAll = 1;   /* moves on any texture deletion (framebuffer completeness, below) */
static GlsKnown  g_glsFramebuffer, g_glsElement;        /* not saved by any push */
/* Capabilities the cache above does not decide, followed only so glIsEnabled can be
   answered; every glEnable and glDisable of them is still sent. */
struct GlsOtherCap { GLenum cap; GLbitfield groups; GlsCap c; };
static GlsOtherCap g_glsOtherCaps[] = {
    { GL_SCISSOR_TEST,   GL_ENABLE_BIT | GL_SCISSOR_BIT,        { false, false } },
    { GL_STENCIL_TEST,   GL_ENABLE_BIT | GL_STENCIL_BUFFER_BIT, { false, false } },
    { GL_LIGHTING,       GL_ENABLE_BIT | GL_LIGHTING_BIT,       { false, false } },
    { GL_COLOR_MATERIAL, GL_ENABLE_BIT | GL_LIGHTING_BIT,       { false, false } },
    { GL_FOG,            GL_ENABLE_BIT | GL_FOG_BIT,            { false, false } },
    { GL_NORMALIZE,      GL_ENABLE_BIT | GL_TRANSFORM_BIT,      { false, false } },
    { GL_DITHER,         GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT,   { false, false } },
    { GL_LINE_SMOOTH,    GL_ENABLE_BIT | GL_LINE_BIT,           { false, false } },
    { GL_POLYGON_SMOOTH, GL_ENABLE_BIT | GL_POLYGON_BIT,        { false, false } },
};
static GlsCap* gls_other_cap(GLenum cap, GLbitfield* groups)
{
    for (auto& o : g_glsOtherCaps) if (o.cap == cap) { if (groups) *groups = o.groups; return &o.c; }
    return NULL;
}
static void gls_shadow_forget(void)
{
    memset(&g_glsShadow, 0, sizeof g_glsShadow);
    g_glsFramebuffer.known = g_glsElement.known = false;
    for (auto& o : g_glsOtherCaps) o.c.known = false;
}
static GlsTextureUnit* gls_texture_unit() {
    gls_texture_limits();
    if (!g_glsActiveKnown) {
        GLint unit=GL_TEXTURE0;
        if(g_glsHasMultitex)glGetIntegerv(GL_ACTIVE_TEXTURE,&unit);
        g_glsActive=(GLenum)unit; g_glsActiveKnown=true;
    }
    return g_glsActive>=GL_TEXTURE0 && g_glsActive<GL_TEXTURE0+32
        && g_glsActive<GL_TEXTURE0+g_glsImageLimit ? &g_glsTexture[g_glsActive-GL_TEXTURE0] : NULL;
}
static void gls_forget_textures() {
    g_glsActiveKnown=false;
    for(auto& u:g_glsTexture) {u.boundKnown=false;u.enabled.known=false;}
    g_glsWrap.clear();
}
static inline void gls_active_texture(GLenum unit) {
    gls_texture_limits();
    /* Binding and client coordinates have different limits from fixed-function
       texture enables. Invalid selectors leave the previous selector unchanged. */
    if (unit<GL_TEXTURE0 || unit>=GL_TEXTURE0+g_glsActiveLimit) {
        glActiveTexture(unit); return;
    }
    if (!gls_reduce_calls() || !g_glsActiveKnown || g_glsActive!=unit) glActiveTexture(unit);
    else if (gls_query_verify()) gls_verify_skip_i("skip active unit", GL_ACTIVE_TEXTURE, (GLint)unit);
    g_glsActive=unit;g_glsActiveKnown=true;
}
/* The selectors as the platform spells them, for the verify mode's own reads, which put
   back what they change and so must not pass through the cache. */
static inline void gls_raw_active_texture(GLenum u) { glActiveTexture(u); }
static inline void gls_raw_client_active(GLenum u) { glClientActiveTexture(u); }
static inline void gls_bind_texture(GLenum target, GLuint name) {
    GlsTextureUnit* u=target==GL_TEXTURE_2D?gls_texture_unit():NULL;
    if (!u || !gls_reduce_calls() || !u->boundKnown || u->bound!=name) glBindTexture(target,name);
    else if (gls_query_verify()) gls_verify_skip_i("skip texture bind", GL_TEXTURE_BINDING_2D, (GLint)name);
    if(u){u->boundKnown=true;u->bound=name;}
}
static inline void gls_tex_parameter(GLenum target, GLenum parameter, GLint value) {
    if (target==GL_TEXTURE_2D && (parameter==GL_TEXTURE_WRAP_S || parameter==GL_TEXTURE_WRAP_T)) {
        GlsTextureUnit* u=gls_texture_unit();
        if (u && u->boundKnown) {
            const uint64_t key=((uint64_t)u->bound<<32)|parameter;
            auto i=g_glsWrap.find(key);
            if (gls_reduce_calls() && i!=g_glsWrap.end() && i->second==value) {
                if (gls_query_verify()) {
                    GLint d = 0; (glGetTexParameteriv)(target, parameter, &d);
                    g_glsVerifyChecked++; g_glsVerifySkips++;
                    if (d != value) {
                        char a[24], b[24]; snprintf(a, sizeof a, "%d", value); snprintf(b, sizeof b, "%d", d);
                        gls_verify_report("skip wrap mode", parameter, "skipped write", 0, a, b);
                    }
                }
                return;
            }
            glTexParameteri(target,parameter,value);g_glsWrap[key]=value;return;
        }
    }
    glTexParameteri(target,parameter,value);
}
static inline void gls_tex_parameterf(GLenum target, GLenum parameter, GLfloat value) {
    if(parameter==GL_TEXTURE_WRAP_S || parameter==GL_TEXTURE_WRAP_T) g_glsWrap.clear();
    glTexParameterf(target,parameter,value);
}
/* Scalar fixed-function texture environment state belongs to the active server
   texture unit, not the texture object. Only recognized legal scalar values are
   remembered. All unsupported targets, parameters and values reach GL unchanged.
   No colour vector, shader value, material or texture pixel is altered. */
struct GlsEnvScalar { bool known=false; GLint value=0; };
static GlsEnvScalar g_glsEnv[32][17];
static bool gls_env_reduce_calls() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_TEXENV_CACHE");return !p||strcmp(p,"0");}();
    return on && gls_reduce_calls();
}
static void gls_forget_env() {
    for(auto& unit:g_glsEnv)for(auto& p:unit)p.known=false;
}
static int gls_env_slot(GLenum p) {
    switch(p) {
    case GL_TEXTURE_ENV_MODE:return 0;
    case GL_COMBINE_RGB:return 1;case GL_COMBINE_ALPHA:return 2;
    case GL_SOURCE0_RGB:return 3;case GL_SOURCE1_RGB:return 4;case GL_SOURCE2_RGB:return 5;
    case GL_SOURCE0_ALPHA:return 6;case GL_SOURCE1_ALPHA:return 7;case GL_SOURCE2_ALPHA:return 8;
    case GL_OPERAND0_RGB:return 9;case GL_OPERAND1_RGB:return 10;case GL_OPERAND2_RGB:return 11;
    case GL_OPERAND0_ALPHA:return 12;case GL_OPERAND1_ALPHA:return 13;case GL_OPERAND2_ALPHA:return 14;
    case GL_RGB_SCALE:return 15;case GL_ALPHA_SCALE:return 16;
    default:return -1;
    }
}
static bool gls_env_value(GLenum p,GLint v) {
    switch(p) {
    case GL_TEXTURE_ENV_MODE:return v==GL_MODULATE||v==GL_DECAL||v==GL_BLEND||v==GL_REPLACE||v==GL_ADD||v==GL_COMBINE;
    case GL_COMBINE_RGB:return v==GL_REPLACE||v==GL_MODULATE||v==GL_ADD||v==GL_ADD_SIGNED||v==GL_INTERPOLATE||v==GL_SUBTRACT||v==GL_DOT3_RGB||v==GL_DOT3_RGBA;
    case GL_COMBINE_ALPHA:return v==GL_REPLACE||v==GL_MODULATE||v==GL_ADD||v==GL_ADD_SIGNED||v==GL_INTERPOLATE||v==GL_SUBTRACT;
    case GL_SOURCE0_RGB:case GL_SOURCE1_RGB:case GL_SOURCE2_RGB:
    case GL_SOURCE0_ALPHA:case GL_SOURCE1_ALPHA:case GL_SOURCE2_ALPHA:
        return v==GL_TEXTURE||v==GL_CONSTANT||v==GL_PRIMARY_COLOR||v==GL_PREVIOUS;
    case GL_OPERAND0_RGB:case GL_OPERAND1_RGB:case GL_OPERAND2_RGB:
        return v==GL_SRC_COLOR||v==GL_ONE_MINUS_SRC_COLOR||v==GL_SRC_ALPHA||v==GL_ONE_MINUS_SRC_ALPHA;
    case GL_OPERAND0_ALPHA:case GL_OPERAND1_ALPHA:case GL_OPERAND2_ALPHA:
        return v==GL_SRC_ALPHA||v==GL_ONE_MINUS_SRC_ALPHA;
    case GL_RGB_SCALE:case GL_ALPHA_SCALE:return v==1||v==2||v==4;
    default:return false;
    }
}
static GlsEnvScalar* gls_env_scalar(GLenum target,GLenum p) {
    if(target!=GL_TEXTURE_ENV || !gls_env_reduce_calls())return NULL;
    const int slot=gls_env_slot(p);if(slot<0)return NULL;
    gls_texture_limits();gls_texture_unit();
    /* Combine requires GL1.3. An older context retains the unchanged driver path,
       including its unsupported-enum errors. The normal desktop context is2.1. */
    static SDL_GLContext checked=NULL;static bool supported=false;
    SDL_GLContext context=g_glsLimitsContext;
    if(checked!=context) {
        checked=context;int major=0,minor=0;const char* v=(const char*)glGetString(GL_VERSION);
        if(v)sscanf(v,"%d.%d",&major,&minor);
        supported=major>1||(major==1&&minor>=3);
    }
    if(!supported || !g_glsActiveKnown || g_glsActive<GL_TEXTURE0 || g_glsActive>=GL_TEXTURE0+32
       ||g_glsActive>=GL_TEXTURE0+g_glsFixedLimit)return NULL;
    return &g_glsEnv[g_glsActive-GL_TEXTURE0][slot];
}
static void gls_verify_skip_env(GLenum target,GLenum p,GLint value) {
    GLint d=0;(glGetTexEnviv)(target,p,&d);
    g_glsVerifyChecked++;g_glsVerifySkips++;
    if(d!=value){char a[24],b[24];snprintf(a,sizeof a,"%d",value);snprintf(b,sizeof b,"%d",d);
        gls_verify_report("skip texture environment",p,"skipped write",0,a,b);}
}
static inline void gls_tex_env_i(GLenum target,GLenum p,GLint value) {
    GlsEnvScalar* s=gls_env_scalar(target,p);
    if(!s||!gls_env_value(p,value)){glTexEnvi(target,p,value);if(s)s->known=false;return;}
    if(!s->known||s->value!=value)glTexEnvi(target,p,value);
    else if(gls_query_verify())gls_verify_skip_env(target,p,value);
    s->known=true;s->value=value;
}
static inline void gls_tex_env_f(GLenum target,GLenum p,GLfloat value) {
    GlsEnvScalar* s=gls_env_scalar(target,p);
    /* Avoid converting NaN, infinities, huge floats or rounded enum values. */
    const bool integral=std::isfinite(value)&&value>=0.0f&&value<=65536.0f
        &&value==(GLfloat)(GLint)value;
    if(!s||!integral||!gls_env_value(p,(GLint)value)){glTexEnvf(target,p,value);if(s)s->known=false;return;}
    const GLint i=(GLint)value;
    if(!s->known||s->value!=i)glTexEnvf(target,p,value);
    else if(gls_query_verify())gls_verify_skip_env(target,p,i);
    s->known=true;s->value=i;
}
static inline void gls_tex_env_iv(GLenum target,GLenum p,const GLint* value) {
    if(gls_env_slot(p)>=0&&value)gls_tex_env_i(target,p,value[0]);
    else glTexEnviv(target,p,value);
}
static inline void gls_tex_env_fv(GLenum target,GLenum p,const GLfloat* value) {
    if(gls_env_slot(p)>=0&&value)gls_tex_env_f(target,p,value[0]);
    else glTexEnvfv(target,p,value);
}
static inline void gls_get_tex_env_i(GLenum target,GLenum p,GLint* value) {
    GlsEnvScalar* s=gls_env_scalar(target,p);
    if(s&&s->known&&value){*value=s->value;return;}
    glGetTexEnviv(target,p,value);
    if(s&&value&&gls_env_value(p,*value)){s->known=true;s->value=*value;}
}
static inline void gls_get_tex_env_f(GLenum target,GLenum p,GLfloat* value) {
    GlsEnvScalar* s=gls_env_scalar(target,p);
    if(s&&s->known&&value){*value=(GLfloat)s->value;return;}
    glGetTexEnvfv(target,p,value);
    if(s&&value&&std::isfinite(*value)&&*value>=0&&*value<=65536
        &&*value==(GLfloat)(GLint)*value&&gls_env_value(p,(GLint)*value)){
        s->known=true;s->value=(GLint)*value;
    }
}

/* ---- writes to one watched texture ----
   The relief surface (terrain_surface.h) keeps the coast cut it read back from the
   terrain atlas across building placements, instead of reading the whole atlas again.
   That copy is valid exactly as long as the texture it came from is not written, so
   every upload, copy or delete in this translation unit that reaches the watched name
   moves g_glsWatchGen, and the copy is read again. One name at a time is enough: only
   the atlas the current surface was built from is ever watched. Nothing renders into
   a terrain atlas, so a framebuffer attachment is not a write this has to see.
   A NEW WAY OF FILLING AN ATLAS must come through these macros (this translation unit,
   glTexImage2D/glTexSubImage2D/glCopyTex*Image2D, deletes through gls_delete_textures),
   or it must bump g_glsWatchGen itself: an upload from another file, through a function
   pointer, as compressed data or by rendering would leave the kept coast cut stale.
   OPENCNC3D_SURFACE_VERIFY=1 compares the kept cut with a fresh readback. */
static GLuint   g_glsWatchTex = 0;
static unsigned g_glsWatchGen = 1;
static inline void gls_watch_write(GLenum target)
{
    if (!g_glsWatchTex || target != GL_TEXTURE_2D) return;
    GlsTextureUnit* u = gls_texture_unit();
    GLuint bound = 0;
    if (u && u->boundKnown) bound = u->bound;
    else { GLint b = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &b); bound = (GLuint)b; }
    if (bound == g_glsWatchTex) g_glsWatchGen++;
}
static bool gls_query_elide(void);
static inline void gls_delete_textures(GLsizei n, const GLuint* names) {
    if (g_glsWatchTex && names)
        for (GLsizei i = 0; i < n; i++) if (names[i] == g_glsWatchTex) g_glsWatchGen++;
    glDeleteTextures(n,names);g_glsTexGenAll++;
    if (!gls_query_elide() || !names) { gls_forget_textures(); return; }
    /* A deleted texture that is bound on any unit leaves that unit bound to none, and
       nothing else: the selector and the enables are untouched. */
    for (auto& u : g_glsTexture)
        if (u.boundKnown)
            for (GLsizei i = 0; i < n; i++) if (names[i] && u.bound == names[i]) u.bound = 0;
    g_glsWrap.clear();
}

static inline void gls_client_active(GLenum unit) {
    gls_texture_limits();
    if(unit<GL_TEXTURE0||unit>=GL_TEXTURE0+g_glsClientLimit){glClientActiveTexture(unit);return;}
    if(!gls_reduce_calls()||!g_glsClientKnown||g_glsClient!=unit)glClientActiveTexture(unit);
    else if(gls_query_verify())gls_verify_skip_i("skip client unit",GL_CLIENT_ACTIVE_TEXTURE,(GLint)unit);
    g_glsClient=unit;g_glsClientKnown=true;
}
static inline void gls_client_enable(GLenum cap,bool on) {
    GlsCap* c=gls_client_cap(cap);
    if(!c||!gls_reduce_calls()||!c->known||c->on!=on){if(on)glEnableClientState(cap);else glDisableClientState(cap);}
    else if(gls_query_verify())gls_verify_skip_cap("skip client array",cap,on);
    if(c){c->known=true;c->on=on;}
}
static inline GlsCap* gls_cap(GLenum cap)
{
    if(cap==GL_CULL_FACE)return &g_glsCull;
    if(cap==GL_POLYGON_OFFSET_FILL)return &g_glsOffset;
    if(cap==GL_BLEND)return &g_glsBlend;
    if(cap==GL_DEPTH_TEST)return &g_glsDepth;
    if(cap==GL_ALPHA_TEST)return &g_glsAlpha;
    if(cap==GL_TEXTURE_2D){
        gls_texture_limits();auto* u=gls_texture_unit();
        return u&&g_glsActive<GL_TEXTURE0+g_glsFixedLimit?&u->enabled:NULL;
    }
    return NULL;
}
static inline void gls_enable(GLenum cap)
{
    GlsCap* c=gls_cap(cap);
    if (!c || !gls_reduce_calls() || !c->known || !c->on) glEnable(cap);
    else if (gls_query_verify()) gls_verify_skip_cap("skip enable", cap, true);
    if (c) { c->known = true; c->on = true; }
    else if (GlsCap* o = gls_other_cap(cap, NULL)) { o->known = true; o->on = true; }
}
static inline void gls_disable(GLenum cap)
{
    GlsCap* c=gls_cap(cap);
    if (!c || !gls_reduce_calls() || !c->known || c->on) glDisable(cap);
    else if (gls_query_verify()) gls_verify_skip_cap("skip disable", cap, false);
    if (c) { c->known = true; c->on = false; }
    else if (GlsCap* o = gls_other_cap(cap, NULL)) { o->known = true; o->on = false; }
}
static inline GLboolean gls_is_enabled(GLenum cap, const char* file, int line)
{
    GlsCap* c = gls_cap(cap);
    if(!c)c=gls_client_cap(cap);
    if(!c&&gls_query_elide())c=gls_other_cap(cap,NULL);
    if (!c) { gls_query_note("glIsEnabled", file, line); return (glIsEnabled)(cap); }
    if (!c->known) {
        gls_query_note("glIsEnabled", file, line);
        c->on = (glIsEnabled)(cap) == GL_TRUE; c->known = true;
    } else if (gls_query_verify()) {
        g_glsVerifyChecked++;
        const bool driver = (glIsEnabled)(cap) == GL_TRUE;
        if (driver != c->on) gls_verify_report("glIsEnabled", cap, file, line, c->on ? "1" : "0", driver ? "1" : "0");
    }
    return c->on ? GL_TRUE : GL_FALSE;
}
struct GlsSelectorSave { GLbitfield mask; bool known; GLenum unit; };
static std::vector<GlsSelectorSave> g_glsServerStack,g_glsClientStack;
static GLint g_glsServerStackLimit=0,g_glsClientStackLimit=0;
static void gls_server_snap_take(size_t depth, GLbitfield mask);
static inline void gls_push_attrib(GLbitfield mask)
{
    if(!g_glsServerStackLimit)glGetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH,&g_glsServerStackLimit);
    glPushAttrib(mask);
    if(g_glsServerStack.size()<(size_t)g_glsServerStackLimit) {
        if(gls_query_elide())gls_server_snap_take(g_glsServerStack.size(),mask);
        g_glsServerStack.push_back({mask,g_glsActiveKnown,g_glsActive});
    }
}
/* Polygon scalars used by the building/wall guards. These exact GL1.x state
   values are already set by this translation unit. Keep them until an attribute
   restore, context/frame boundary or invalid/unsuitable setter makes them unknown.
   Unknown values are queried once; no matrix/depth arithmetic is changed. */
struct GlsPolygonState {
    bool cullKnown=false,frontKnown=false,offsetKnown=false;
    GLint cull=GL_BACK,front=GL_CCW;
    GLfloat factor=0.0f,units=0.0f;
};
static GlsPolygonState g_glsPolygon;
static inline void gls_forget_polygon() {
    g_glsPolygon.cullKnown=g_glsPolygon.frontKnown=g_glsPolygon.offsetKnown=false;
}
static inline GLint gls_cull_mode() {
    if(!gls_reduce_calls()||!g_glsPolygon.cullKnown) {
        glGetIntegerv(GL_CULL_FACE_MODE,&g_glsPolygon.cull);g_glsPolygon.cullKnown=true;
    }
    return g_glsPolygon.cull;
}
static inline GLint gls_front_face() {
    if(!gls_reduce_calls()||!g_glsPolygon.frontKnown) {
        glGetIntegerv(GL_FRONT_FACE,&g_glsPolygon.front);g_glsPolygon.frontKnown=true;
    }
    return g_glsPolygon.front;
}
static inline void gls_polygon_offset_values(GLfloat* factor,GLfloat* units) {
    if(!gls_reduce_calls()||!g_glsPolygon.offsetKnown) {
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR,&g_glsPolygon.factor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS,&g_glsPolygon.units);g_glsPolygon.offsetKnown=true;
    }
    *factor=g_glsPolygon.factor;*units=g_glsPolygon.units;
}
static inline void gls_cull_face(GLenum mode) {
    /* Invalid enums must reach GL, leave its error semantics intact and retain the
       last valid state. All callers are outside glBegin/glEnd. */
    if(mode!=GL_FRONT&&mode!=GL_BACK&&mode!=GL_FRONT_AND_BACK){glCullFace(mode);return;}
    if(!gls_reduce_calls()||!g_glsPolygon.cullKnown||g_glsPolygon.cull!=(GLint)mode)glCullFace(mode);
    else if(gls_query_verify())gls_verify_skip_i("skip cull face",GL_CULL_FACE_MODE,(GLint)mode);
    g_glsPolygon.cull=(GLint)mode;g_glsPolygon.cullKnown=true;
}
static inline void gls_front_face_set(GLenum mode) {
    if(mode!=GL_CW&&mode!=GL_CCW){glFrontFace(mode);return;}
    if(!gls_reduce_calls()||!g_glsPolygon.frontKnown||g_glsPolygon.front!=(GLint)mode)glFrontFace(mode);
    else if(gls_query_verify())gls_verify_skip_i("skip front face",GL_FRONT_FACE,(GLint)mode);
    g_glsPolygon.front=(GLint)mode;g_glsPolygon.frontKnown=true;
}
static inline void gls_polygon_offset(GLfloat factor,GLfloat units) {
    if(!std::isfinite(factor)||!std::isfinite(units)) {
        glPolygonOffset(factor,units);g_glsPolygon.offsetKnown=false;return;
    }
    const bool same=g_glsPolygon.offsetKnown
        &&memcmp(&g_glsPolygon.factor,&factor,sizeof factor)==0
        &&memcmp(&g_glsPolygon.units,&units,sizeof units)==0;
    if(!gls_reduce_calls()||!same)glPolygonOffset(factor,units);
    else if(gls_query_verify()) {
        GLfloat d[2]={0.0f,0.0f};(glGetFloatv)(GL_POLYGON_OFFSET_FACTOR,d);(glGetFloatv)(GL_POLYGON_OFFSET_UNITS,d+1);
        g_glsVerifyChecked++;g_glsVerifySkips++;
        if(memcmp(&d[0],&factor,sizeof factor)||memcmp(&d[1],&units,sizeof units)) {
            char a[48],b[48];snprintf(a,sizeof a,"%.9g,%.9g",factor,units);snprintf(b,sizeof b,"%.9g,%.9g",d[0],d[1]);
            gls_verify_report("skip polygon offset",GL_POLYGON_OFFSET_FACTOR,"skipped write",0,a,b);
        }
    }
    g_glsPolygon.factor=factor;g_glsPolygon.units=units;g_glsPolygon.offsetKnown=true;
}

/* WHAT A PUSH SAW, for the pop to put back. GL restores exactly the groups the push
   named, to the values they had at the push; everything the cache knew about those
   groups at that moment is therefore true again after the pop, and everything outside
   them is untouched by it. Only the groups the mask names are copied. */
struct GlsServerSnap {
    GlsCap cull, offset, blend, depth, alpha;
    GlsTextureUnit tex[32];
    GlsEnvScalar env[32][17];
    GlsPolygonState polygon;
    bool activeKnown; GLenum active;
    GlsShadow shadow;
    GlsCap other[sizeof g_glsOtherCaps / sizeof g_glsOtherCaps[0]];
};
enum { GLS_SERVER_SNAPS = 64 };
static GlsServerSnap g_glsServerSnap[GLS_SERVER_SNAPS];
static bool g_glsServerSnapValid[GLS_SERVER_SNAPS];
static void gls_server_snap_take(size_t depth, GLbitfield m)
{
    if (depth >= GLS_SERVER_SNAPS) return;
    GlsServerSnap& p = g_glsServerSnap[depth];
    g_glsServerSnapValid[depth] = true;
    p.cull = g_glsCull; p.offset = g_glsOffset; p.blend = g_glsBlend; p.depth = g_glsDepth; p.alpha = g_glsAlpha;
    if (m & (GL_TEXTURE_BIT | GL_ENABLE_BIT)) memcpy(p.tex, g_glsTexture, sizeof p.tex);
    if (m & GL_TEXTURE_BIT) memcpy(p.env, g_glsEnv, sizeof p.env);
    p.polygon = g_glsPolygon;
    p.activeKnown = g_glsActiveKnown; p.active = g_glsActive;
    p.shadow = g_glsShadow;
    p.shadow.matrixMode.known = g_glsMatModeKnown; p.shadow.matrixMode.i[0] = g_glsMatMode;
    for (size_t i = 0; i < sizeof p.other / sizeof p.other[0]; i++) p.other[i] = g_glsOtherCaps[i].c;
}
/* The pop under OPENCNC3D_QUERY_ELIDE: each piece of state the mask covers takes the
   knowledge its push saw. A pop the cache has no push for forgets everything. */
static bool gls_server_snap_restore(size_t depth, GLbitfield m)
{
    if (depth >= GLS_SERVER_SNAPS || !g_glsServerSnapValid[depth]) return false;
    g_glsServerSnapValid[depth] = false;
    const GlsServerSnap& p = g_glsServerSnap[depth];
    const bool en = m & GL_ENABLE_BIT, col = m & GL_COLOR_BUFFER_BIT, dep = m & GL_DEPTH_BUFFER_BIT;
    const bool pol = m & GL_POLYGON_BIT, tex = m & GL_TEXTURE_BIT;
    if (en || pol) { g_glsCull = p.cull; g_glsOffset = p.offset; }
    if (en || col) { g_glsBlend = p.blend; g_glsAlpha = p.alpha; }
    if (en || dep) g_glsDepth = p.depth;
    for (int u = 0; u < 32; u++) {
        if (tex) { g_glsTexture[u].boundKnown = p.tex[u].boundKnown; g_glsTexture[u].bound = p.tex[u].bound; }
        if (tex || en) g_glsTexture[u].enabled = p.tex[u].enabled;
    }
    if (tex) {
        memcpy(g_glsEnv, p.env, sizeof g_glsEnv);
        g_glsActiveKnown = p.activeKnown; g_glsActive = p.active;
        g_glsWrap.clear();      /* the bound textures' own parameters are put back too */
    }
    if (pol) g_glsPolygon = p.polygon;
    /* A colour-buffer or pixel-mode pop also puts back the bound framebuffer's draw and
       read buffers, which the completeness memo (below) does not follow; the one draw
       buffer set inside such a push (water_plane_render.h) is the default it puts back. */
    if (col) {
        g_glsShadow.clear = p.shadow.clear; g_glsShadow.colourMask = p.shadow.colourMask;
        g_glsShadow.blend = p.shadow.blend; g_glsShadow.blendEq = p.shadow.blendEq;
        g_glsShadow.alphaFunc = p.shadow.alphaFunc; g_glsShadow.alphaRef = p.shadow.alphaRef;
    }
    if (dep) { g_glsShadow.depthFunc = p.shadow.depthFunc; g_glsShadow.depthMask = p.shadow.depthMask; }
    if (m & GL_VIEWPORT_BIT) g_glsShadow.viewport = p.shadow.viewport;
    if (m & GL_SCISSOR_BIT) g_glsShadow.scissor = p.shadow.scissor;
    if (m & GL_TRANSFORM_BIT) { g_glsMatModeKnown = p.shadow.matrixMode.known; g_glsMatMode = p.shadow.matrixMode.i[0]; }
    if (m & GL_CURRENT_BIT) { g_glsShadow.colour = p.shadow.colour; g_glsShadow.normal = p.shadow.normal; }
    for (size_t i = 0; i < sizeof p.other / sizeof p.other[0]; i++)
        if (m & g_glsOtherCaps[i].groups) g_glsOtherCaps[i].c = p.other[i];
    return true;
}
static void gls_server_forget_all(void);
static inline void gls_pop_attrib(void)
{
    if (gls_query_elide()) {
        const bool have = !g_glsServerStack.empty();
        GLbitfield mask = 0;
        if (have) { mask = g_glsServerStack.back().mask; g_glsServerStack.pop_back(); }
        glPopAttrib();
        if (!have || !gls_server_snap_restore(g_glsServerStack.size(), mask)) gls_server_forget_all();
        return;
    }
    bool known=false;GLenum unit=GL_TEXTURE0;
    if(!g_glsServerStack.empty()) {
        const auto save=g_glsServerStack.back();g_glsServerStack.pop_back();
        known=(save.mask&GL_TEXTURE_BIT)?save.known:g_glsActiveKnown;
        unit=(save.mask&GL_TEXTURE_BIT)?save.unit:g_glsActive;
    }
    glPopAttrib();
    gls_forget_polygon();gls_forget_env();
    g_glsCull.known = g_glsOffset.known = false;
    g_glsBlend.known=g_glsDepth.known=g_glsAlpha.known=false;
    gls_forget_textures();
    /* The paired push already recorded the selector that GL restored. Keep
       bindings/enables conservative without asking the driver for this again. */
    g_glsActiveKnown=known;g_glsActive=unit;
}

/* ---- the array buffer binding ---- */
static bool   g_glsArrayKnown = false;
static GLuint g_glsArray = 0;

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER          0x8892
#endif
#ifndef GL_ARRAY_BUFFER_BINDING
#define GL_ARRAY_BUFFER_BINDING  0x8894
#endif

static inline GLuint gls_array_binding(void)
{
    if (!g_glsArrayKnown) {
        GLint v = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &v);
        g_glsArray = (GLuint)v;
        g_glsArrayKnown = true;
    }
    return g_glsArray;
}
/* ---- the two auxiliary world passes ----
   The ground-normal target and the bake target (fx_draw_terrain_normals and
   fx_draw_mesh_bake) draw the visible world a second and a third time, object by object,
   through the colour picture's own draw functions, and each draw sets its state as if
   nothing were set yet. Measured on a Tiberian Dawn base under Enhanced: 13,500 GL calls
   for 731 draws in the first and 6,300 for 354 in the second. Most of those calls change
   something, but three kinds set a value GL already holds:
     a texture bound on a second unit for every textured run, with the unit selected and
     deselected around a bind the cache already skips (gls_bind_texture_on, below);
     a uniform set again to the value its program already holds, per object or per batch
     (gls_uniform_held, after the bound program);
     an array switch set again after a glPopClientAttrib, which made the cache forget the
     switches it knew although the pop had put back exactly the ones the push saw.
   The last two are answered only while one of those passes runs (GlsAuxScope), and are
   forgotten as it begins and ends, so nothing drawn between them can leave a stale
   answer. OPENCNC3D_AUX_STATE_ELIDE=0 sends every one of these calls, as before. */
static int g_glsAuxDepth = 0;
static bool gls_aux_elide(void) {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_AUX_STATE_ELIDE");return !p||strcmp(p,"0");}();
    return on && gls_reduce_calls();
}
/* The array switches as the cache knew them at each glPushClientAttrib, kept beside
   g_glsClientStack and pushed and popped with it. Only a push of the vertex-array bit
   made inside one of the passes is put back by its pop. */
/* Under OPENCNC3D_QUERY_ELIDE every push of the vertex-array bit keeps the switches and
   both buffer bindings as the cache knew them, and its pop puts them back: the pop
   restores exactly those (with the client texture unit), and nothing else. */
struct GlsClientCaps { bool valid, buffers; GlsCap vertex, color, normal, tex[32]; bool arrayKnown; GLuint array; GlsKnown element; };
static std::vector<GlsClientCaps> g_glsClientCaps;
static inline void gls_push_client_attrib(GLbitfield mask)
{
    if(!g_glsClientStackLimit)glGetIntegerv(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH,&g_glsClientStackLimit);
    glPushClientAttrib(mask);
    if(g_glsClientStack.size()<(size_t)g_glsClientStackLimit) {
        g_glsClientStack.push_back({mask,g_glsClientKnown,g_glsClient});
        GlsClientCaps caps;
        caps.buffers=(mask&GL_CLIENT_VERTEX_ARRAY_BIT)&&gls_query_elide();
        caps.valid=caps.buffers||(g_glsAuxDepth>0&&(mask&GL_CLIENT_VERTEX_ARRAY_BIT)&&gls_aux_elide());
        if(caps.valid) {
            caps.vertex=g_glsVertex;caps.color=g_glsColor;caps.normal=g_glsNormal;
            memcpy(caps.tex,g_glsTexArray,sizeof caps.tex);
            caps.arrayKnown=g_glsArrayKnown;caps.array=g_glsArray;caps.element=g_glsElement;
        }
        g_glsClientCaps.push_back(caps);
    }
}
static inline void gls_pop_client_attrib(void)
{
    if(gls_query_elide()) {
        const bool have=!g_glsClientStack.empty();
        GlsSelectorSave save={0,false,GL_TEXTURE0};
        GlsClientCaps caps;caps.valid=caps.buffers=false;
        if(have) {
            save=g_glsClientStack.back();g_glsClientStack.pop_back();
            if(!g_glsClientCaps.empty()){caps=g_glsClientCaps.back();g_glsClientCaps.pop_back();}
        }
        glPopClientAttrib();
        if(have&&!(save.mask&GL_CLIENT_VERTEX_ARRAY_BIT))return;   /* nothing followed changed */
        if(have&&caps.buffers) {
            g_glsVertex=caps.vertex;g_glsColor=caps.color;g_glsNormal=caps.normal;
            memcpy(g_glsTexArray,caps.tex,sizeof caps.tex);
            g_glsClientKnown=save.known;g_glsClient=save.unit;
            g_glsArrayKnown=caps.arrayKnown;g_glsArray=caps.array;g_glsElement=caps.element;
            return;
        }
        gls_forget_clients();g_glsArrayKnown=false;g_glsElement.known=false;
        return;
    }
    bool known=false;GLenum unit=GL_TEXTURE0;
    GlsClientCaps caps;caps.valid=false;
    if(!g_glsClientStack.empty()) {
        const auto save=g_glsClientStack.back();g_glsClientStack.pop_back();
        known=(save.mask&GL_CLIENT_VERTEX_ARRAY_BIT)?save.known:g_glsClientKnown;
        unit=(save.mask&GL_CLIENT_VERTEX_ARRAY_BIT)?save.unit:g_glsClient;
        if(!g_glsClientCaps.empty()){caps=g_glsClientCaps.back();g_glsClientCaps.pop_back();}
    }
    glPopClientAttrib();
    gls_forget_clients();
    if(caps.valid&&g_glsAuxDepth>0) {
        g_glsVertex=caps.vertex;g_glsColor=caps.color;g_glsNormal=caps.normal;
        memcpy(g_glsTexArray,caps.tex,sizeof caps.tex);
    }
    g_glsClientKnown=known;g_glsClient=unit;
    g_glsArrayKnown = false;
}

/* ---- the bound program ----
   Every program bind in this file goes through fx_glUseProgram, which fx_gl.h routes to
   gls_use_program, so the bound program is known without asking: the building normal
   pass used to ask the driver for it at every textured run (stone_mesh_normal_bind), and
   each object pass asks again before it binds its own. A bind of the program already
   bound is a no-op in GL, so it is skipped too. Linking or deleting any program bumps
   g_glsProgramEpoch, which is what cached uniform locations are checked against.
   OPENCNC3D_PROGRAM_SHADOW=0 asks and binds every time, as before;
   OPENCNC3D_PROGRAM_SHADOW_VERIFY=1 asks anyway and reports any disagreement. */
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif
static bool     g_glsProgramKnown = false;
static GLuint   g_glsProgram = 0;
static unsigned g_glsProgramEpoch = 1;
static unsigned long g_glsProgramBindsSkipped = 0, g_glsProgramQueriesSkipped = 0, g_glsProgramMismatches = 0;
static bool gls_program_shadow(void)
{
    static const bool on = mesh_perf_switch("OPENCNC3D_PROGRAM_SHADOW");
    return on;
}
static bool gls_program_verify(void)
{
    static const bool on = [](){ const char* p = getenv("OPENCNC3D_PROGRAM_SHADOW_VERIFY"); return p && !strcmp(p, "1"); }();
    return on;
}
static void gls_program_check(const char* where)
{
    GLint p = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &p);
    if ((GLuint)p != g_glsProgram) {
        if (g_glsProgramMismatches < 8)
            fprintf(stderr, "GLSHADOW|program|mismatch|%s|known=%u|bound=%d\n", where, g_glsProgram, (int)p);
        g_glsProgramMismatches++;
    }
}
/* ---- the answers ----
   Each known value, in the type the renderer asks for it in; anything asked in another
   type, or not known, goes to the driver, and the driver's answer is then known. */
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER_BINDING
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#endif
#ifndef GL_BLEND_SRC_RGB
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_DST_ALPHA 0x80CA
#endif
#ifndef GL_BLEND_EQUATION_RGB
#define GL_BLEND_EQUATION_RGB 0x8009
#endif
#ifndef GL_BLEND_EQUATION_ALPHA
#define GL_BLEND_EQUATION_ALPHA 0x883D
#endif
static GLint gls_known_count(GLenum pname)
{
    switch (pname) {
    case GL_VIEWPORT: case GL_SCISSOR_BOX: case GL_COLOR_CLEAR_VALUE: case GL_CURRENT_COLOR:
    case GL_COLOR_WRITEMASK: return 4;
    case GL_CURRENT_NORMAL: return 3;
    case GL_MODELVIEW_MATRIX: case GL_PROJECTION_MATRIX: return 16;
    default: return 1;
    }
}
/* The GLint view of one known piece, or NULL. */
static const GlsKnown* gls_known_int(GLenum pname, int* index)
{
    *index = 0;
    switch (pname) {
    case GL_VIEWPORT:            return &g_glsShadow.viewport;
    case GL_SCISSOR_BOX:         return &g_glsShadow.scissor;
    case GL_DEPTH_FUNC:          return &g_glsShadow.depthFunc;
    case GL_ALPHA_TEST_FUNC:     return &g_glsShadow.alphaFunc;
    case GL_BLEND_SRC: case GL_BLEND_SRC_RGB: return &g_glsShadow.blend;
    case GL_BLEND_DST: case GL_BLEND_DST_RGB: *index = 1; return &g_glsShadow.blend;
    case GL_BLEND_SRC_ALPHA:     *index = 2; return &g_glsShadow.blend;
    case GL_BLEND_DST_ALPHA:     *index = 3; return &g_glsShadow.blend;
    case GL_BLEND_EQUATION_RGB:  return &g_glsShadow.blendEq;
    case GL_BLEND_EQUATION_ALPHA: *index = 1; return &g_glsShadow.blendEq;
    case GL_FRAMEBUFFER_BINDING: return &g_glsFramebuffer;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING: return &g_glsElement;
    default: return NULL;
    }
}
static bool gls_answer_integerv(GLenum pname, GLint* out)
{
    switch (pname) {
    case GL_ACTIVE_TEXTURE:
        if (!g_glsActiveKnown) return false;
        *out = (GLint)g_glsActive; return true;
    case GL_CLIENT_ACTIVE_TEXTURE:
        if (!g_glsClientKnown) return false;
        *out = (GLint)g_glsClient; return true;
    case GL_TEXTURE_BINDING_2D:
        if (!g_glsActiveKnown || g_glsActive < GL_TEXTURE0 || g_glsActive >= GL_TEXTURE0 + 32
            || !g_glsTexture[g_glsActive - GL_TEXTURE0].boundKnown) return false;
        *out = (GLint)g_glsTexture[g_glsActive - GL_TEXTURE0].bound; return true;
    case GL_ARRAY_BUFFER_BINDING:
        if (!g_glsArrayKnown) return false;
        *out = (GLint)g_glsArray; return true;
    case GL_MATRIX_MODE:
        if (!g_glsMatModeKnown) return false;
        *out = g_glsMatMode; return true;
    case GL_CULL_FACE_MODE:
        if (!g_glsPolygon.cullKnown) return false;
        *out = g_glsPolygon.cull; return true;
    case GL_FRONT_FACE:
        if (!g_glsPolygon.frontKnown) return false;
        *out = g_glsPolygon.front; return true;
    default: break;
    }
    int index = 0;
    const GlsKnown* k = gls_known_int(pname, &index);
    if (!k || !k->known) return false;
    const GLint n = gls_known_count(pname);
    if (n == 1) *out = k->i[index];
    else memcpy(out, k->i, (size_t)n * sizeof(GLint));
    return true;
}
static void gls_learn_integerv(GLenum pname, const GLint* v)
{
    switch (pname) {
    case GL_ACTIVE_TEXTURE: g_glsActive = (GLenum)v[0]; g_glsActiveKnown = true; return;
    case GL_CLIENT_ACTIVE_TEXTURE: g_glsClient = (GLenum)v[0]; g_glsClientKnown = true; return;
    case GL_ARRAY_BUFFER_BINDING: g_glsArray = (GLuint)v[0]; g_glsArrayKnown = true; return;
    case GL_MATRIX_MODE:
        g_glsMatModeKnown = v[0] == GL_MODELVIEW || v[0] == GL_PROJECTION || v[0] == GL_TEXTURE;
        g_glsMatMode = v[0]; return;
    case GL_CULL_FACE_MODE: g_glsPolygon.cull = v[0]; g_glsPolygon.cullKnown = true; return;
    case GL_FRONT_FACE: g_glsPolygon.front = v[0]; g_glsPolygon.frontKnown = true; return;
    case GL_VIEWPORT: case GL_SCISSOR_BOX: case GL_DEPTH_FUNC: case GL_ALPHA_TEST_FUNC:
    case GL_FRAMEBUFFER_BINDING: case GL_ELEMENT_ARRAY_BUFFER_BINDING: {
        int index = 0;
        GlsKnown* k = (GlsKnown*)gls_known_int(pname, &index);
        memcpy(k->i, v, (size_t)gls_known_count(pname) * sizeof(GLint));
        k->known = true; return;
    }
    default: return;   /* a blend factor alone does not make the four known */
    }
}

static inline void gls_get_integerv(GLenum pname, GLint* out, const char* file, int line)
{
    if (pname == GL_CURRENT_PROGRAM && gls_program_shadow()) {
        if (g_glsProgramKnown) {
            if (gls_program_verify()) gls_program_check("query");
            if (gls_query_verify()) {
                GLint d = 0; (glGetIntegerv)(GL_CURRENT_PROGRAM, &d); g_glsVerifyChecked++;
                if ((GLuint)d != g_glsProgram) {
                    char a[24], b[24]; snprintf(a, sizeof a, "%u", g_glsProgram); snprintf(b, sizeof b, "%d", d);
                    gls_verify_report("glGetIntegerv", pname, file, line, a, b);
                }
            }
            *out = (GLint)g_glsProgram; g_glsProgramQueriesSkipped++;
            return;
        }
        gls_query_note("glGetIntegerv", file, line);
        (glGetIntegerv)(pname, out);
        g_glsProgram = (GLuint)*out; g_glsProgramKnown = true;
        return;
    }
    if (gls_query_elide()) {
        if (gls_answer_integerv(pname, out)) {
            if (gls_query_verify()) {
                GLint d[4] = { 0, 0, 0, 0 }, n = gls_known_count(pname);
                (glGetIntegerv)(pname, d); g_glsVerifyChecked++;
                if (memcmp(d, out, (size_t)n * sizeof(GLint))) {
                    char a[64], b[64];
                    snprintf(a, sizeof a, "%d,%d,%d,%d", out[0], n > 1 ? out[1] : 0, n > 2 ? out[2] : 0, n > 3 ? out[3] : 0);
                    snprintf(b, sizeof b, "%d,%d,%d,%d", d[0], d[1], d[2], d[3]);
                    gls_verify_report("glGetIntegerv", pname, file, line, a, b);
                }
            }
            return;
        }
        gls_query_note("glGetIntegerv", file, line);
        (glGetIntegerv)(pname, out);
        gls_learn_integerv(pname, out);
        return;
    }
    gls_query_note("glGetIntegerv", file, line);
    (glGetIntegerv)(pname, out);
}
#undef glGetIntegerv
#define glGetIntegerv(pname, out) gls_get_integerv(pname, out, __FILE__, __LINE__)

#define glCullFace(mode) gls_cull_face(mode)
#define glFrontFace(mode) gls_front_face_set(mode)
#define glPolygonOffset(factor,units) gls_polygon_offset(factor,units)
#define glEnable(cap)       gls_enable(cap)
#define glDisable(cap)      gls_disable(cap)
#undef glIsEnabled
#define glIsEnabled(cap)    gls_is_enabled(cap, __FILE__, __LINE__)
#define glPushAttrib(mask)  gls_push_attrib(mask)
#define glPopAttrib()       gls_pop_attrib()
#define glPushClientAttrib(mask) gls_push_client_attrib(mask)
#define glPopClientAttrib() gls_pop_client_attrib()
#undef glClientActiveTexture
#define glClientActiveTexture(unit) gls_client_active(unit)
#define glEnableClientState(cap) gls_client_enable(cap,true)
#define glDisableClientState(cap) gls_client_enable(cap,false)
#undef glActiveTexture
#define glActiveTexture(unit) gls_active_texture(unit)
#define glBindTexture(target,name) gls_bind_texture(target,name)
#undef glTexEnvi
#undef glTexEnvf
#undef glGetTexEnviv
#undef glTexEnviv
#undef glTexEnvfv
#undef glGetTexEnvfv
#define glTexEnvi(target,param,value) gls_tex_env_i(target,param,value)
#define glTexEnvf(target,param,value) gls_tex_env_f(target,param,value)
#define glTexEnviv(target,param,value) gls_tex_env_iv(target,param,value)
#define glTexEnvfv(target,param,value) gls_tex_env_fv(target,param,value)
#define glGetTexEnviv(target,param,value) gls_get_tex_env_i(target,param,value)
#define glGetTexEnvfv(target,param,value) gls_get_tex_env_f(target,param,value)
#define glTexParameteri(target,param,value) gls_tex_parameter(target,param,value)
#define glTexParameterf(target,param,value) gls_tex_parameterf(target,param,value)
#define glDeleteTextures(n,names) gls_delete_textures(n,names)
#define glTexImage2D(t,l,i,w,h,b,f,y,p) (gls_watch_write(t), gls_tex_respecified(t), glTexImage2D(t,l,i,w,h,b,f,y,p))
#define glTexSubImage2D(t,l,x,y,w,h,f,ty,p) (gls_watch_write(t), glTexSubImage2D(t,l,x,y,w,h,f,ty,p))
#define glCopyTexSubImage2D(t,l,ox,oy,x,y,w,h) (gls_watch_write(t), glCopyTexSubImage2D(t,l,ox,oy,x,y,w,h))
#define glCopyTexImage2D(t,l,i,x,y,w,h,b) (gls_watch_write(t), gls_tex_respecified(t), glCopyTexImage2D(t,l,i,x,y,w,h,b))

/* ---- a texture on a second unit ----
   The same as glActiveTexture(unit); glBindTexture(GL_TEXTURE_2D, name);
   glActiveTexture(after), and it sends all three whenever anything is unknown. When
   `after` is already the selected unit and `unit` already holds `name`, the three calls
   would leave every value where it is, so none is sent: the bind in the middle was
   already skipped by the cache, and the two selections around it changed nothing. */
static inline void gls_bind_texture_on(GLenum unit, GLuint name, GLenum after)
{
    gls_texture_limits();
    if (gls_aux_elide() && g_glsActiveKnown && g_glsActive == after
        && unit >= GL_TEXTURE0 && unit < GL_TEXTURE0 + 32
        && unit < GL_TEXTURE0 + (GLenum)g_glsImageLimit) {
        const GlsTextureUnit& u = g_glsTexture[unit - GL_TEXTURE0];
        if (u.boundKnown && u.bound == name) {
            if (gls_query_verify()) {
                gls_verify_skip_i("skip bind on unit: selected", GL_ACTIVE_TEXTURE, (GLint)after);
                gls_raw_active_texture(unit);
                gls_verify_skip_i("skip bind on unit: binding", GL_TEXTURE_BINDING_2D, (GLint)name);
                gls_raw_active_texture(after);
            }
            return;
        }
    }
    glActiveTexture(unit); glBindTexture(GL_TEXTURE_2D, name); glActiveTexture(after);
}

/* ---- uniform values, inside the two auxiliary passes ----
   A uniform's value belongs to its program and changes only by a glUniform call made
   while that program is bound, or by linking it. Every uniform call in this file goes
   through fx_gl.h's wrappers, which ask gls_uniform_held first, and every program bind
   goes through the program shadow above, so the value each (program, location) was last
   given is known for as long as the pass runs. A set of the same kind of call with the
   same bytes is then a no-op and is not sent. Bytes, not values: -0 and 0, or two NaNs
   with different bits, are sent again. A call the shadow cannot follow (an array of more
   than one element, a transposed matrix) forgets every value, and so does a link or a
   delete, through g_glsProgramEpoch. Outside the passes nothing is remembered.
   The values sit in a fixed open-addressed table rather than a node map: the passes make
   about 1,600 uniform calls a frame, and a lookup that allocated on the first set of each
   pass would cost about what the skipped call saves. Forgetting is one counter step. A key
   that finds no place within a few probes is simply not remembered, and its call is sent. */
struct GlsUniformValue { uint64_t key; unsigned stamp; unsigned char kind, bytes; unsigned char v[64]; };
enum { GLS_UNIFORM_SLOTS = 1024, GLS_UNIFORM_PROBES = 8 };
static GlsUniformValue g_glsUniforms[GLS_UNIFORM_SLOTS];
static unsigned g_glsUniformStamp = 1;   /* a slot holds a value only while its stamp matches */
static unsigned g_glsUniformEpoch = 0;
static inline void gls_uniforms_forget(void)
{
    if (++g_glsUniformStamp == 0) {
        for (auto& u : g_glsUniforms) u.stamp = 0;
        g_glsUniformStamp = 1;
    }
    g_glsUniformEpoch = g_glsProgramEpoch;
}
/* True when the bound program's uniform at `loc` already holds these bytes from a call of
   the same `kind` (1 and up); otherwise remembers them and returns false, and the caller
   sends the call. */
static inline bool gls_uniform_held(GLint loc, unsigned char kind, const void* v, size_t n)
{
    if (!g_glsAuxDepth || loc < 0 || n > 64 || !g_glsProgramKnown || !g_glsProgram
        || !gls_program_shadow() || !gls_aux_elide()) return false;
    if (g_glsUniformEpoch != g_glsProgramEpoch) gls_uniforms_forget();
    const uint64_t key = ((uint64_t)g_glsProgram << 32) | (uint32_t)loc;
    const unsigned h = (unsigned)((key * 0x9E3779B97F4A7C15ull) >> 54);
    for (unsigned p = 0; p < GLS_UNIFORM_PROBES; p++) {
        GlsUniformValue& u = g_glsUniforms[(h + p) & (GLS_UNIFORM_SLOTS - 1)];
        if (u.stamp == g_glsUniformStamp && u.key != key) continue;
        if (u.stamp == g_glsUniformStamp && u.kind == kind && u.bytes == n
            && memcmp(u.v, v, n) == 0) return true;
        u.key = key; u.stamp = g_glsUniformStamp;
        u.kind = kind; u.bytes = (unsigned char)n; memcpy(u.v, v, n);
        return false;
    }
    return false;
}

/* AN ARRAY SET FORGETS ONLY WHAT IT WRITES. glUniform4fv(loc, n, v) with n above one (or
   a transposed matrix) changes the bound program's values at loc .. loc + n - 1 and
   nothing else: GL gives an array's elements consecutive locations, and no caller sets an
   element past an array's first one through that element's own location, so a remembered
   value can only sit at one of those. A bone palette is set this way for every soldier in
   both passes, and forgetting the whole table each time threw away every other value the
   passes had kept.
   gls_uniform_held puts a value in the first slot of its probes that is empty or already
   its own, so a location is never held past an empty slot and never held twice. A value
   forgotten here keeps its slot, under a key no location has (GLS_UNIFORM_GONE): the slots
   before every held value stay full, and each location's search stops at its first empty
   slot, one or two probes for a location nothing set, which is every element of a palette.
   When the bound program is not known, or a program was linked or deleted since the table
   was filled, everything is forgotten as before. Counted: the full forgets an array set
   still made inside the passes, and the ranged ones. */
static unsigned g_glsUniformArrayFull = 0, g_glsUniformArrayRange = 0;
static const uint64_t GLS_UNIFORM_GONE = ~(uint64_t)0;   /* location -1: never held */
static inline void gls_uniforms_forget_range(GLint loc, GLsizei n)
{
    if (!g_glsAuxDepth || loc < 0 || n <= 0) return;   /* nothing is written, or nothing kept */
    if (!g_glsProgramKnown || !g_glsProgram || g_glsUniformEpoch != g_glsProgramEpoch || n > 256) {
        gls_uniforms_forget();
        g_glsUniformArrayFull++;
        return;
    }
    for (GLsizei i = 0; i < n; i++) {
        const uint64_t key = ((uint64_t)g_glsProgram << 32) | (uint32_t)(loc + i);
        const unsigned h = (unsigned)((key * 0x9E3779B97F4A7C15ull) >> 54);
        for (unsigned p = 0; p < GLS_UNIFORM_PROBES; p++) {
            GlsUniformValue& u = g_glsUniforms[(h + p) & (GLS_UNIFORM_SLOTS - 1)];
            if (u.stamp != g_glsUniformStamp) break;
            if (u.key == key) { u.key = GLS_UNIFORM_GONE; break; }
        }
    }
    g_glsUniformArrayRange++;
}

/* One of the two auxiliary passes, from its first call to its last. Nested scopes are
   one scope. */
struct GlsAuxScope {
    GlsAuxScope() { if (g_glsAuxDepth++ == 0) gls_uniforms_forget(); }
    ~GlsAuxScope() { if (--g_glsAuxDepth == 0) gls_uniforms_forget(); }
};

/* ---- the state a pass saves and puts back, shadowed from its writers ----
   Each writer is the real call, and then the value it set is known when GL is certain
   to hold exactly that value: an argument GL would reject, clamp or convert is not
   remembered, and the next question about it goes to the driver. */
static bool gls_compare_func(GLenum f) { return f >= GL_NEVER && f <= GL_ALWAYS; }
static bool gls_blend_factor(GLenum f, bool source)
{
    switch (f) {
    case GL_ZERO: case GL_ONE: case GL_SRC_COLOR: case GL_ONE_MINUS_SRC_COLOR: case GL_DST_COLOR:
    case GL_ONE_MINUS_DST_COLOR: case GL_SRC_ALPHA: case GL_ONE_MINUS_SRC_ALPHA: case GL_DST_ALPHA:
    case GL_ONE_MINUS_DST_ALPHA: return true;
    case GL_SRC_ALPHA_SATURATE: return source;
    default: return false;
    }
}
static bool gls_unit_float(GLfloat v) { return std::isfinite(v) && v >= 0.0f && v <= 1.0f; }
static inline void gls_clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    (glClearColor)(r, g, b, a);
    GlsKnown& k = g_glsShadow.clear;
    k.known = gls_unit_float(r) && gls_unit_float(g) && gls_unit_float(b) && gls_unit_float(a);
    k.f[0] = r; k.f[1] = g; k.f[2] = b; k.f[3] = a;
}
static inline void gls_colour_mask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    (glColorMask)(r, g, b, a);
    GlsKnown& k = g_glsShadow.colourMask;
    k.known = true;
    k.i[0] = r ? GL_TRUE : GL_FALSE; k.i[1] = g ? GL_TRUE : GL_FALSE;
    k.i[2] = b ? GL_TRUE : GL_FALSE; k.i[3] = a ? GL_TRUE : GL_FALSE;
}
static inline void gls_blend_func(GLenum src, GLenum dst)
{
    (glBlendFunc)(src, dst);
    GlsKnown& k = g_glsShadow.blend;
    k.known = gls_blend_factor(src, true) && gls_blend_factor(dst, false);
    k.i[0] = k.i[2] = (GLint)src; k.i[1] = k.i[3] = (GLint)dst;
}
/* The two calls the cache cannot read the arguments of in its own spelling (both reach
   GL through pointers resolved at run time) only make what they set unknown. */
static inline void gls_blend_written(void) { g_glsShadow.blend.known = false; }
static inline void gls_blend_equation_written(void) { g_glsShadow.blendEq.known = false; }
static inline void gls_alpha_func(GLenum f, GLfloat ref)
{
    (glAlphaFunc)(f, ref);
    g_glsShadow.alphaFunc.known = gls_compare_func(f);
    g_glsShadow.alphaFunc.i[0] = (GLint)f;
    g_glsShadow.alphaRef.known = gls_compare_func(f) && gls_unit_float(ref);
    g_glsShadow.alphaRef.f[0] = ref;
}
static inline void gls_depth_func(GLenum f)
{
    (glDepthFunc)(f);
    g_glsShadow.depthFunc.known = gls_compare_func(f);
    g_glsShadow.depthFunc.i[0] = (GLint)f;
}
static inline void gls_depth_mask(GLboolean m)
{
    (glDepthMask)(m);
    g_glsShadow.depthMask.known = true;
    g_glsShadow.depthMask.i[0] = m ? GL_TRUE : GL_FALSE;
}
static inline void gls_viewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    (glViewport)(x, y, w, h);
    GlsKnown& k = g_glsShadow.viewport;
    /* GL clamps a size past its own limit; every limit is at least 8192 here. */
    k.known = w >= 0 && h >= 0 && w <= 8192 && h <= 8192;
    k.i[0] = x; k.i[1] = y; k.i[2] = w; k.i[3] = h;
}
static inline void gls_scissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
    (glScissor)(x, y, w, h);
    GlsKnown& k = g_glsShadow.scissor;
    k.known = w >= 0 && h >= 0;
    k.i[0] = x; k.i[1] = y; k.i[2] = w; k.i[3] = h;
}
/* The current colour and normal. A float call sets exactly its floats; a call that
   converts bytes leaves the converted value to the driver, so it is not remembered. An
   array draw with the colour (or normal) array possibly on leaves the current value
   unspecified, so it is forgotten. */
static inline void gls_colour_known(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    GlsKnown& k = g_glsShadow.colour;
    k.known = true; k.f[0] = r; k.f[1] = g; k.f[2] = b; k.f[3] = a;
}
static inline void gls_color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (glColor4f)(r, g, b, a); gls_colour_known(r, g, b, a); }
static inline void gls_color3f(GLfloat r, GLfloat g, GLfloat b) { (glColor3f)(r, g, b); gls_colour_known(r, g, b, 1.0f); }
static inline void gls_color4fv(const GLfloat* v) { (glColor4fv)(v); gls_colour_known(v[0], v[1], v[2], v[3]); }
static inline void gls_color3fv(const GLfloat* v) { (glColor3fv)(v); gls_colour_known(v[0], v[1], v[2], 1.0f); }
static inline void gls_color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { (glColor4ub)(r, g, b, a); g_glsShadow.colour.known = false; }
static inline void gls_color3ub(GLubyte r, GLubyte g, GLubyte b) { (glColor3ub)(r, g, b); g_glsShadow.colour.known = false; }
static inline void gls_color4ubv(const GLubyte* v) { (glColor4ubv)(v); g_glsShadow.colour.known = false; }
static inline void gls_color3ubv(const GLubyte* v) { (glColor3ubv)(v); g_glsShadow.colour.known = false; }
static inline void gls_normal3f(GLfloat x, GLfloat y, GLfloat z)
{
    (glNormal3f)(x, y, z);
    GlsKnown& k = g_glsShadow.normal; k.known = true; k.f[0] = x; k.f[1] = y; k.f[2] = z;
}
static inline void gls_normal3fv(const GLfloat* v)
{
    (glNormal3fv)(v);
    GlsKnown& k = g_glsShadow.normal; k.known = true; k.f[0] = v[0]; k.f[1] = v[1]; k.f[2] = v[2];
}
static inline void gls_array_draw_written(void)
{
    if (!g_glsColor.known || g_glsColor.on) g_glsShadow.colour.known = false;
    if (!g_glsNormal.known || g_glsNormal.on) g_glsShadow.normal.known = false;
}
static inline void gls_draw_arrays(GLenum mode, GLint first, GLsizei count)
{
    (glDrawArrays)(mode, first, count);
    gls_array_draw_written();
}
static inline void gls_draw_elements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
    (glDrawElements)(mode, count, type, indices);
    gls_array_draw_written();
}

/* ---- framebuffer completeness, remembered ----
   A framebuffer's completeness is a fixed function of what is attached to it (each
   texture, its level, and that level's format and size) and of its draw and read
   buffers. The cache follows each framebuffer the renderer creates from its creation:
   what is attached where, and how often each texture has been specified anew. The
   answer the driver gave for one such arrangement is the answer for it again. */
struct GlsFboInfo { bool known; std::vector<GLuint> att; GLenum draw, read; };
static std::unordered_map<GLuint, GlsFboInfo> g_glsFbo;
static std::unordered_map<GLuint, unsigned> g_glsTexGen;
static std::unordered_map<std::string, GLenum> g_glsFboStatus;
static void gls_fbo_forget_all(void) { for (auto& f : g_glsFbo) f.second.known = false; }
/* Every answer kept across frames belongs to one GL context; a new context starts
   without them. */
static SDL_GLContext g_glsMatAnswerContext = NULL;
static void gls_fbo_context(void)
{
    const SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (context == g_glsMatAnswerContext) return;
    g_glsMatAnswerContext = context;
    g_glsMatAnswer[0].clear(); g_glsMatAnswer[1].clear(); g_glsFboStatus.clear(); g_glsFbo.clear();
    g_glsMatDepthLimit[0] = g_glsMatDepthLimit[1] = 0;
}
static GlsFboInfo* gls_fbo_bound_info(void)
{
    if (!g_glsFramebuffer.known || !g_glsFramebuffer.i[0]) return NULL;
    auto it = g_glsFbo.find((GLuint)g_glsFramebuffer.i[0]);
    return it == g_glsFbo.end() ? NULL : &it->second;
}
static inline void gls_tex_respecified(GLenum target)
{
    if (target != GL_TEXTURE_2D) { g_glsTexGenAll++; return; }
    GlsTextureUnit* u = (g_glsActiveKnown && g_glsActive >= GL_TEXTURE0 && g_glsActive < GL_TEXTURE0 + 32)
        ? &g_glsTexture[g_glsActive - GL_TEXTURE0] : NULL;
    if (u && u->boundKnown) g_glsTexGen[u->bound]++;
    else g_glsTexGenAll++;
}
static inline void gls_draw_buffer(GLenum b)
{
    (glDrawBuffer)(b);
    if (!g_glsFramebuffer.known) gls_fbo_forget_all();
    else if (GlsFboInfo* f = gls_fbo_bound_info()) f->draw = b;
}
static inline void gls_read_buffer(GLenum b)
{
    (glReadBuffer)(b);
    if (!g_glsFramebuffer.known) gls_fbo_forget_all();
    else if (GlsFboInfo* f = gls_fbo_bound_info()) f->read = b;
}
/* The key the driver's answer is kept under, or false when part of it is not known. */
static bool gls_fbo_key(std::string* key)
{
    GlsFboInfo* f = gls_fbo_bound_info();
    if (!f || !f->known) return false;
    std::vector<GLuint> w;
    w.push_back((GLuint)g_glsFramebuffer.i[0]); w.push_back(g_glsTexGenAll);
    w.push_back(f->draw); w.push_back(f->read);
    for (size_t i = 0; i + 3 < f->att.size(); i += 4) {
        w.insert(w.end(), f->att.begin() + i, f->att.begin() + i + 4);
        w.push_back(g_glsTexGen[f->att[i + 2]]);
    }
    key->assign((const char*)w.data(), w.size() * sizeof(GLuint));
    return true;
}

static void gls_get_floatv(GLenum pname, GLfloat* out, const char* file, int line)
{
    if (gls_query_elide()) {
        bool known = false;
        const GlsKnown* k = NULL;
        switch (pname) {
        case GL_COLOR_CLEAR_VALUE: k = &g_glsShadow.clear; break;
        case GL_CURRENT_COLOR:     k = &g_glsShadow.colour; break;
        case GL_CURRENT_NORMAL:    k = &g_glsShadow.normal; break;
        case GL_ALPHA_TEST_REF:    k = &g_glsShadow.alphaRef; break;
        case GL_POLYGON_OFFSET_FACTOR:
            if ((known = g_glsPolygon.offsetKnown)) *out = g_glsPolygon.factor;
            break;
        case GL_POLYGON_OFFSET_UNITS:
            if ((known = g_glsPolygon.offsetKnown)) *out = g_glsPolygon.units;
            break;
        case GL_MODELVIEW_MATRIX:  known = gls_mat_known_answer(0, out); break;
        case GL_PROJECTION_MATRIX: known = gls_mat_known_answer(1, out); break;
        default: break;
        }
        const GLint n = gls_known_count(pname);
        if (k && k->known) { memcpy(out, k->f, (size_t)n * sizeof(GLfloat)); known = true; }
        if (known) {
            if (gls_query_verify()) {
                GLfloat d[16];
                (glGetFloatv)(pname, d); g_glsVerifyChecked++;
                if (memcmp(d, out, (size_t)n * sizeof(GLfloat))) {
                    char a[96], b[96];
                    snprintf(a, sizeof a, "%.9g,%.9g,%.9g,%.9g", out[0], n > 1 ? out[1] : 0.0f, n > 2 ? out[2] : 0.0f, n > 3 ? out[3] : 0.0f);
                    snprintf(b, sizeof b, "%.9g,%.9g,%.9g,%.9g", d[0], n > 1 ? d[1] : 0.0f, n > 2 ? d[2] : 0.0f, n > 3 ? d[3] : 0.0f);
                    gls_verify_report("glGetFloatv", pname, file, line, a, b);
                }
            }
            return;
        }
        gls_query_note("glGetFloatv", file, line);
        (glGetFloatv)(pname, out);
        GlsKnown* w = NULL;
        switch (pname) {
        case GL_COLOR_CLEAR_VALUE: w = &g_glsShadow.clear; break;
        case GL_CURRENT_COLOR:     w = &g_glsShadow.colour; break;
        case GL_CURRENT_NORMAL:    w = &g_glsShadow.normal; break;
        case GL_ALPHA_TEST_REF:    w = &g_glsShadow.alphaRef; break;
        case GL_MODELVIEW_MATRIX:  gls_mat_learn(0, out); break;
        case GL_PROJECTION_MATRIX: gls_mat_learn(1, out); break;
        default: break;
        }
        if (w) { memcpy(w->f, out, (size_t)n * sizeof(GLfloat)); w->known = true; }
        return;
    }
    gls_query_note("glGetFloatv", file, line);
    (glGetFloatv)(pname, out);
}
static void gls_get_booleanv(GLenum pname, GLboolean* out, const char* file, int line)
{
    const GlsKnown* k = pname == GL_DEPTH_WRITEMASK ? &g_glsShadow.depthMask
                      : pname == GL_COLOR_WRITEMASK ? &g_glsShadow.colourMask : NULL;
    const int n = pname == GL_COLOR_WRITEMASK ? 4 : 1;
    if (gls_query_elide() && k && k->known) {
        for (int i = 0; i < n; i++) out[i] = (GLboolean)k->i[i];
        if (gls_query_verify()) {
            GLboolean d[4] = { 0, 0, 0, 0 };
            (glGetBooleanv)(pname, d); g_glsVerifyChecked++;
            if (memcmp(d, out, (size_t)n)) {
                char a[32], b[32];
                snprintf(a, sizeof a, "%d%d%d%d", out[0], n > 1 ? out[1] : 0, n > 2 ? out[2] : 0, n > 3 ? out[3] : 0);
                snprintf(b, sizeof b, "%d%d%d%d", d[0], d[1], d[2], d[3]);
                gls_verify_report("glGetBooleanv", pname, file, line, a, b);
            }
        }
        return;
    }
    gls_query_note("glGetBooleanv", file, line);
    (glGetBooleanv)(pname, out);
    if (gls_query_elide() && k) {
        GlsKnown* w = (GlsKnown*)k;
        for (int i = 0; i < n; i++) w->i[i] = out[i] ? GL_TRUE : GL_FALSE;
        w->known = true;
    }
}
#undef glGetFloatv
#undef glGetBooleanv
#define glGetFloatv(p, o)   gls_get_floatv(p, o, __FILE__, __LINE__)
#define glGetBooleanv(p, o) gls_get_booleanv(p, o, __FILE__, __LINE__)
#define glClearColor(...) gls_clear_color(__VA_ARGS__)
#define glColorMask(...) gls_colour_mask(__VA_ARGS__)
#define glBlendFunc(...) gls_blend_func(__VA_ARGS__)
#define glAlphaFunc(...) gls_alpha_func(__VA_ARGS__)
#define glDepthFunc(f)           gls_depth_func(f)
#define glDepthMask(m)           gls_depth_mask(m)
#define glViewport(...) gls_viewport(__VA_ARGS__)
#define glScissor(...) gls_scissor(__VA_ARGS__)
#define glColor4f(...) gls_color4f(__VA_ARGS__)
#define glColor3f(...) gls_color3f(__VA_ARGS__)
#define glColor4fv(v)            gls_color4fv(v)
#define glColor3fv(v)            gls_color3fv(v)
#define glColor4ub(...) gls_color4ub(__VA_ARGS__)
#define glColor3ub(...) gls_color3ub(__VA_ARGS__)
#define glColor4ubv(v)           gls_color4ubv(v)
#define glColor3ubv(v)           gls_color3ubv(v)
#define glNormal3f(...) gls_normal3f(__VA_ARGS__)
#define glNormal3fv(v)           gls_normal3fv(v)
#define glDrawArrays(...) gls_draw_arrays(__VA_ARGS__)
#define glDrawElements(...) gls_draw_elements(__VA_ARGS__)
#define glDrawBuffer(b)          gls_draw_buffer(b)
#define glReadBuffer(b)          gls_read_buffer(b)

/* Everything a pop with no matching push, or a foreign draw, could have changed. */
static void gls_server_forget_all(void)
{
    gls_forget_polygon(); gls_forget_env();
    g_glsCull.known = g_glsOffset.known = false;
    g_glsBlend.known = g_glsDepth.known = g_glsAlpha.known = false;
    gls_forget_textures();
    GlsKnown fb = g_glsFramebuffer, el = g_glsElement;
    gls_shadow_forget();
    g_glsFramebuffer = fb; g_glsElement = el;
    g_glsMatModeKnown = false;
}
/* KNOWLEDGE KEPT FROM ONE FRAME TO THE NEXT. Inside this file's own loops (the game
   loop, a script's frames, a still's settling frames) every call that changes GL state
   between the end of one frame and the start of the next is one the cache sees, so what
   it knew at the end of a frame is still true at the start of the next, and the next
   frame asks nothing. A loop marks the end of a frame with gls_frame_end; the next
   gls_frame_begin keeps the cache only if nothing foreign has drawn since this frame
   began. Every call into another translation unit that draws (the Enhanced dialogs,
   the 3D emblems, a movie) is followed by gls_forget_foreign, and leaving a loop drops
   the mark, so any frame that follows foreign drawing starts by forgetting, as every
   frame did before. */
static bool g_glsKeep = false;        /* the last frame ended in an own loop, nothing foreign since */
static bool g_glsFrameClean = false;  /* nothing foreign has drawn since this frame began */
static unsigned long g_glsFramesKept = 0, g_glsFramesForgot = 0;
static inline void gls_frame_end(void) { g_glsKeep = g_glsFrameClean && gls_query_elide(); }
static inline void gls_loop_left(void) { g_glsKeep = false; }
/* One of this file's loops, from entry to exit: nothing is kept into or out of it. */
struct GlsLoopScope { GlsLoopScope() { gls_loop_left(); } ~GlsLoopScope() { gls_loop_left(); } };
/* AROUND A CALL INTO ANOTHER TRANSLATION UNIT THAT DRAWS. Such code changes GL state the
   cache never sees: the 3D emblems leave their texture bound on the active unit, the
   texture environment at MODULATE, their alpha function, current colour and matrix mode,
   and saving only the enables, viewport, scissor and depth groups puts none of that back.
   So everything the cache knows is forgotten after the call (except the framebuffer
   binding: no other translation unit binds one), the questions it answers go to the driver
   until the frame sets them again, and the next frame starts by forgetting. Only frames
   that draw foreign UI pay. Under OPENCNC3D_QUERY_ELIDE=verify everything the cache holds
   is checked against the driver right before the call and again right after the forget,
   so a stale answer on either side is a reported mismatch. */
static void gls_verify_known_all(const char* where);
static unsigned long g_glsVerifyForeign = 0;
static void gls_foreign_begin(const char* what)
{
    if (!gls_query_verify()) return;
    g_glsVerifyForeign++;
    /* Each kind named once, so a run can show which foreign drawing it went through. */
    static std::vector<const char*> seen;
    if (std::find(seen.begin(), seen.end(), what) == seen.end()) {
        seen.push_back(what);
        printf("GLQUERYVERIFY|foreign|%s\n", what);
    }
    gls_verify_known_all("before foreign drawing");
}
static void gls_forget_foreign(void)
{
    g_glsFrameClean = false; g_glsKeep = false;
    const GlsKnown fb = g_glsFramebuffer;
    gls_forget_polygon(); gls_forget_env();
    g_glsCull.known = g_glsOffset.known = false;
    g_glsBlend.known = g_glsDepth.known = g_glsAlpha.known = false;
    gls_forget_textures();
    gls_forget_clients();
    g_glsArrayKnown = false;
    g_glsProgramKnown = false; g_glsProgramEpoch++;
    g_glsMatEpoch++;
    gls_shadow_forget(); gls_mat_forget();
    if (gls_query_elide()) g_glsFramebuffer = fb;
    if (gls_query_verify()) gls_verify_known_all("after forgetting foreign drawing");
}
/* Set at the top of the frame by command: the two buffer bindings (grass_bake.h, where
   the buffer entry points are). */
static void gls_frame_buffers_zero(void);
/* And the blend equation (grass_field.h, where its entry point is), and the framebuffer
   binding (fx_gl.h). */
static void gls_frame_blend_equation(void);
static void gls_frame_framebuffer_zero(void);

/* The top of every frame: forget everything, so nothing drawn outside this file between
   two frames can leave a stale answer behind; then, under OPENCNC3D_QUERY_ELIDE, set the
   few values the frame would otherwise ask for first. */
/* OPENCNC3D_QUERY_ELIDE=verify at the top of a frame that kept the last frame's
   knowledge, and around foreign drawing: every value the cache holds is read back from
   the driver and compared, so a writer the cache does not see shows up as a mismatch. */
static const char* g_glsVerifyWhere = "frame start (kept)";
static void gls_verify_one_i(const char* what, GLenum pname, GLint known, GLint driver)
{
    g_glsVerifyChecked++;
    if (known == driver) return;
    char a[24], b[24];
    snprintf(a, sizeof a, "%d", known); snprintf(b, sizeof b, "%d", driver);
    gls_verify_report(what, pname, g_glsVerifyWhere, 0, a, b);
}
static void gls_verify_cap(const GlsCap& c, GLenum cap)
{
    if (c.known) gls_verify_one_i("glIsEnabled", cap, c.on ? 1 : 0, (glIsEnabled)(cap) == GL_TRUE ? 1 : 0);
}
static void gls_verify_known_all(const char* where)
{
    g_glsVerifyWhere = where;
    GLint v[4], d[4]; GLfloat f[16]; GLboolean b[4];
    GLint active = GL_TEXTURE0, client = GL_TEXTURE0;
    if (g_glsHasMultitex) { (glGetIntegerv)(GL_ACTIVE_TEXTURE, &active); (glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &client); }
    if (g_glsActiveKnown) gls_verify_one_i("active", GL_ACTIVE_TEXTURE, (GLint)g_glsActive, active);
    if (g_glsClientKnown) gls_verify_one_i("client", GL_CLIENT_ACTIVE_TEXTURE, (GLint)g_glsClient, client);
    gls_verify_cap(g_glsCull, GL_CULL_FACE); gls_verify_cap(g_glsOffset, GL_POLYGON_OFFSET_FILL);
    gls_verify_cap(g_glsBlend, GL_BLEND); gls_verify_cap(g_glsDepth, GL_DEPTH_TEST); gls_verify_cap(g_glsAlpha, GL_ALPHA_TEST);
    gls_verify_cap(g_glsVertex, GL_VERTEX_ARRAY); gls_verify_cap(g_glsColor, GL_COLOR_ARRAY); gls_verify_cap(g_glsNormal, GL_NORMAL_ARRAY);
    for (auto& o : g_glsOtherCaps) gls_verify_cap(o.c, o.cap);
    const int units = g_glsHasMultitex ? std::min(32, (int)std::max(g_glsImageLimit, g_glsClientLimit)) : 1;
    for (int u = 0; u < units; u++) {
        if (g_glsHasMultitex) { gls_raw_active_texture(GL_TEXTURE0 + u); gls_raw_client_active(GL_TEXTURE0 + u); }
        const GlsTextureUnit& t = g_glsTexture[u];
        if (t.boundKnown && u < g_glsImageLimit) { (glGetIntegerv)(GL_TEXTURE_BINDING_2D, v); gls_verify_one_i("binding", GL_TEXTURE_BINDING_2D + u * 0x10000, (GLint)t.bound, v[0]); }
        if (t.enabled.known && u < g_glsFixedLimit) gls_verify_cap(t.enabled, GL_TEXTURE_2D);
        if (g_glsTexArray[u].known && u < g_glsClientLimit) gls_verify_cap(g_glsTexArray[u], GL_TEXTURE_COORD_ARRAY);
        if (u < g_glsFixedLimit && u < 32) {
            static const GLenum slots[17] = { GL_TEXTURE_ENV_MODE, GL_COMBINE_RGB, GL_COMBINE_ALPHA, GL_SOURCE0_RGB, GL_SOURCE1_RGB,
                GL_SOURCE2_RGB, GL_SOURCE0_ALPHA, GL_SOURCE1_ALPHA, GL_SOURCE2_ALPHA, GL_OPERAND0_RGB, GL_OPERAND1_RGB,
                GL_OPERAND2_RGB, GL_OPERAND0_ALPHA, GL_OPERAND1_ALPHA, GL_OPERAND2_ALPHA, GL_RGB_SCALE, GL_ALPHA_SCALE };
            for (int k = 0; k < 17; k++) if (g_glsEnv[u][k].known) {
                GLfloat e = 0.0f; (glGetTexEnvfv)(GL_TEXTURE_ENV, slots[k], &e);
                gls_verify_one_i("texenv", slots[k], g_glsEnv[u][k].value, (GLint)e);
            }
        }
    }
    if (g_glsHasMultitex) { gls_raw_active_texture((GLenum)active); gls_raw_client_active((GLenum)client); }
    if (g_glsArrayKnown) { (glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, v); gls_verify_one_i("array", GL_ARRAY_BUFFER_BINDING, (GLint)g_glsArray, v[0]); }
    if (g_glsProgramKnown) { (glGetIntegerv)(GL_CURRENT_PROGRAM, v); gls_verify_one_i("program", GL_CURRENT_PROGRAM, (GLint)g_glsProgram, v[0]); }
    if (g_glsPolygon.cullKnown) { (glGetIntegerv)(GL_CULL_FACE_MODE, v); gls_verify_one_i("polygon", GL_CULL_FACE_MODE, g_glsPolygon.cull, v[0]); }
    if (g_glsPolygon.frontKnown) { (glGetIntegerv)(GL_FRONT_FACE, v); gls_verify_one_i("polygon", GL_FRONT_FACE, g_glsPolygon.front, v[0]); }
    if (g_glsPolygon.offsetKnown) {
        (glGetFloatv)(GL_POLYGON_OFFSET_FACTOR, f); (glGetFloatv)(GL_POLYGON_OFFSET_UNITS, f + 1);
        gls_verify_one_i("polygon", GL_POLYGON_OFFSET_FACTOR, memcmp(f, &g_glsPolygon.factor, 4) || memcmp(f + 1, &g_glsPolygon.units, 4), 0);
    }
    if (g_glsMatModeKnown) { (glGetIntegerv)(GL_MATRIX_MODE, v); gls_verify_one_i("matrix mode", GL_MATRIX_MODE, g_glsMatMode, v[0]); }
    static const GLenum ints[] = { GL_VIEWPORT, GL_SCISSOR_BOX, GL_DEPTH_FUNC, GL_ALPHA_TEST_FUNC, GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB,
        GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA, GL_FRAMEBUFFER_BINDING,
        GL_ELEMENT_ARRAY_BUFFER_BINDING };
    for (GLenum p : ints) {
        if (!gls_answer_integerv(p, v)) continue;
        memset(d, 0, sizeof d); (glGetIntegerv)(p, d);
        gls_verify_one_i("state", p, memcmp(v, d, (size_t)gls_known_count(p) * sizeof(GLint)) ? 1 : 0, 0);
    }
    static const GLenum floats[] = { GL_COLOR_CLEAR_VALUE, GL_CURRENT_COLOR, GL_CURRENT_NORMAL, GL_ALPHA_TEST_REF };
    for (GLenum p : floats) {
        const GlsKnown* k = p == GL_COLOR_CLEAR_VALUE ? &g_glsShadow.clear : p == GL_CURRENT_COLOR ? &g_glsShadow.colour
                          : p == GL_CURRENT_NORMAL ? &g_glsShadow.normal : &g_glsShadow.alphaRef;
        if (!k->known) continue;
        (glGetFloatv)(p, f);
        gls_verify_one_i("state", p, memcmp(f, k->f, (size_t)gls_known_count(p) * sizeof(GLfloat)) ? 1 : 0, 0);
    }
    if (g_glsShadow.depthMask.known) { (glGetBooleanv)(GL_DEPTH_WRITEMASK, b); gls_verify_one_i("state", GL_DEPTH_WRITEMASK, g_glsShadow.depthMask.i[0], b[0] ? GL_TRUE : GL_FALSE); }
    if (g_glsShadow.colourMask.known) {
        (glGetBooleanv)(GL_COLOR_WRITEMASK, b);
        for (int i = 0; i < 4; i++) gls_verify_one_i("state", GL_COLOR_WRITEMASK, g_glsShadow.colourMask.i[i], b[i] ? GL_TRUE : GL_FALSE);
    }
    for (int k = 0; k < 2; k++) if (gls_mat_known_answer(k, f)) {
        GLfloat m[16]; (glGetFloatv)(k ? GL_PROJECTION_MATRIX : GL_MODELVIEW_MATRIX, m);
        gls_verify_one_i("matrix", k ? GL_PROJECTION_MATRIX : GL_MODELVIEW_MATRIX, memcmp(m, f, sizeof m) ? 1 : 0, 0);
    }
}
static void gls_frame_begin(void)
{
    const SDL_GLContext context=SDL_GL_GetCurrentContext();
    const bool keep = g_glsKeep && gls_query_elide() && context == g_glsLimitsContext
                      && g_glsServerStack.empty() && g_glsClientStack.empty();
    g_glsKeep = false; g_glsFrameClean = true;
    if (keep) {
        g_glsFramesKept++;
        if (gls_query_verify()) gls_verify_known_all("frame start (kept)");
        /* As every frame does: the clip is worked out again (from the kept answers),
           and values kept per program are set again once. */
        g_glsMatEpoch++; g_glsProgramEpoch++;
        return;
    }
    g_glsFramesForgot++;
    if(context!=g_glsLimitsContext) {
        g_glsLimitsContext=context;
        g_glsActiveLimit=g_glsImageLimit=g_glsClientLimit=g_glsFixedLimit=0;
        g_glsServerStackLimit=g_glsClientStackLimit=0;
    }
    g_glsServerStack.clear();g_glsClientStack.clear();g_glsClientCaps.clear();
    gls_forget_polygon();gls_forget_env();
    g_glsMatEpoch++;gls_forget_clients();
    g_glsCull.known = g_glsOffset.known = false;
    g_glsBlend.known=g_glsDepth.known=g_glsAlpha.known=false;
    gls_forget_textures();
    g_glsArrayKnown = false;
    g_glsProgramKnown = false; g_glsProgramEpoch++;
    gls_shadow_forget(); gls_mat_forget();
    if (g_glsMatAnswerContext && context != g_glsMatAnswerContext) gls_fbo_context();
    g_glsMatAnswerContext = context;
    if (!gls_query_elide()) return;
    /* Nothing is asked: the selectors and buffer bindings a frame used to ask for first
       are set instead. Every frame ends with these values already in place (the verify
       mode checks it), so setting them changes nothing GL holds. */
    gls_texture_limits();
    if (gls_query_verify() && g_glsHasMultitex) {
        GLint a = 0, c = 0;
        (glGetIntegerv)(GL_ACTIVE_TEXTURE, &a); (glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &c);
        g_glsVerifyChecked += 2;
        char d[32];
        if (a != GL_TEXTURE0) { snprintf(d, sizeof d, "0x%X", (unsigned)a); gls_verify_report("frame start", GL_ACTIVE_TEXTURE, __FILE__, __LINE__, "0x84C0", d); }
        if (c != GL_TEXTURE0) { snprintf(d, sizeof d, "0x%X", (unsigned)c); gls_verify_report("frame start", GL_CLIENT_ACTIVE_TEXTURE, __FILE__, __LINE__, "0x84C0", d); }
    }
    if (g_glsHasMultitex) { gls_active_texture(GL_TEXTURE0); gls_client_active(GL_TEXTURE0); }
    else { g_glsActive = g_glsClient = GL_TEXTURE0; g_glsActiveKnown = g_glsClientKnown = true; }
    gls_frame_buffers_zero();
    /* And the few other values a frame asks about before it sets them, each of which
       every frame is seen to end with: the client arrays off, the first unit's alpha
       scale at one, the blend equation adding, the window's own framebuffer bound. */
    static const GLenum arrays[4] = { GL_VERTEX_ARRAY, GL_COLOR_ARRAY, GL_NORMAL_ARRAY, GL_TEXTURE_COORD_ARRAY };
    for (GLenum a : arrays) {
        if (gls_query_verify()) {
            g_glsVerifyChecked++;
            if ((glIsEnabled)(a)) gls_verify_report("frame start", a, __FILE__, __LINE__, "0", "1");
        }
        gls_client_enable(a, false);
    }
    if (gls_query_verify()) {
        GLfloat scale = 0.0f; (glGetTexEnvfv)(GL_TEXTURE_ENV, GL_ALPHA_SCALE, &scale);
        g_glsVerifyChecked++;
        char d[32]; snprintf(d, sizeof d, "%g", scale);
        if (scale != 1.0f) gls_verify_report("frame start", GL_ALPHA_SCALE, __FILE__, __LINE__, "1", d);
    }
    gls_tex_env_f(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0f);
    gls_frame_blend_equation();
    gls_frame_framebuffer_zero();
}

#endif /* OPENCNC3D_GL_STATE_CACHE_H */
