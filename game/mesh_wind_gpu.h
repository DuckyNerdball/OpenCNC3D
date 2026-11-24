/* ====================================================================================
 *  mesh_wind_gpu.h -- the remastered trees' wind, posed by the card.
 *
 *  A remastered tree sways by its PackMesh::sway weights (draw_mesh). Every frame each
 *  tree is posed once per mode it draws in (the opaque trunk and the cutout crown), and
 *  that pose is shared by every pass of the frame through the frame's geometry pose
 *  (MeshGeometryPose::gpu): the sun's depth, the picture, the ground normals and the
 *  bake all read one buffer of world positions. The pose used to be worked out on the
 *  CPU, corner by corner, and sent to the card: on a snowy Red Alert map about 960,000
 *  corners and 11.5 MB a frame, and on a Windows PC about 4 ms of the frame in the
 *  posing alone.
 *
 *  Here the card works it out. Each source (one mesh, one triangle range, one mode) is
 *  sent ONCE as its distinct corner inputs (rest position, the two weights, the four
 *  flutter phases: the same inputs mesh_wind_unique_prepare finds) plus the list that
 *  maps every drawn corner to its input. A pose is then one transform feedback draw: a
 *  vertex program runs the same arithmetic draw_mesh runs, in the same order, over those
 *  inputs, and writes the world positions straight into the frame's pose buffer, with
 *  the rasteriser switched off. Nothing else changes: the passes, their programs, the
 *  kept colour and texture stores and the draw order are exactly as before, so every
 *  pass still reads the one pose and they cannot disagree.
 *
 *  THE SAME ARITHMETIC, NOT THE SAME BITS. Every sine and cosine is still taken on the
 *  CPU, once per draw, and handed in; the vertex program does only the multiplies and
 *  adds, in draw_mesh's order. A card is free to fuse a multiply and an add, and so is
 *  the compiler that built the CPU path, so a position can differ in its last bits.
 *  OPENCNC3D_MESH_WIND_GPU=verify works out every pose on the CPU as well, reads the
 *  card's back and reports the largest difference (MESHWINDGPU|... in the mesh dump).
 *
 *  BOUNDS WITHOUT A READ BACK. The CPU pose measured its own box for culling. The card's
 *  pose is boxed instead from the source's rest box, widened by the most this frame's
 *  wind can move any corner (the weights' maxima and the wind's own values), then
 *  carried through the tree's matrix and its placement. A wider box can only cull less.
 *
 *  ANYTHING MISSING MEANS THE CPU PATH, never a half-posed tree: no transform feedback
 *  in the context, an entry point that does not resolve, a program that does not link,
 *  a store the driver did not make, or the first pose of the run not agreeing with the
 *  CPU's (checked once, read back once). The entry points are loaded here, on their own,
 *  and nothing in fx_gl_load waits on them, so a driver without them keeps Enhanced.
 *  The first pose's check is a guard against a driver that writes something else
 *  (nothing, garbage, another order), not a test of precision: it allows a thousandth of
 *  a cell. The precision is held by the verify mode, which gate G380 runs.
 *
 *  OPENCNC3D_MESH_WIND_GPU: 1 poses on the card, 0 keeps the CPU pose (exactly the
 *  pictures before this file), verify poses on the card and checks every pose. UNSET, the
 *  card poses only where it was measured to pay and checked to agree: NVIDIA's driver,
 *  on any system but macOS. Measured, with the sun's map in the frame profiler, on Red
 *  Alert's snow at 1920x1080 on a Windows PC with an NVIDIA card: the map fell from 8.5
 *  to 3.3 ms and the finished frame from 25.4 to 17.9 ms. Every build for macOS keeps
 *  the CPU pose unless asked, whatever the GPU: on Apple silicon each transform feedback
 *  draw between two ordinary draws costs about a tenth of a millisecond, more than the
 *  CPU's pose and upload cost there (the skirmish view's map rose from about 2.2 to about
 *  6.3 ms), and on an Intel Mac, whose OpenGL names the GPU's maker as its vendor, the
 *  card's pose has not been measured at all. Other drivers (AMD's, Intel's, Mesa's) keep
 *  the CPU pose for the same reason: not measured, not checked. The switch at 1 or
 *  verify still asks the card on any of them.
 *
 *  FAULTS ON PURPOSE, so every way back to the CPU can be run on a driver that never
 *  fails: OPENCNC3D_MESH_WIND_GPU_LOAD_OK=0 acts as a context without transform
 *  feedback, _LINK_OK=0 asks the driver to capture an output the program does not have
 *  (a real link failure), _SOURCE_OK=0 and _STORE_OK=0 treat a source store or a pose
 *  store as one the driver did not make, and _CHECK_OK=0 makes the run's first pose
 *  disagree with the CPU's. Each is read once, and each must leave the frame the switch
 *  at 0 draws, byte for byte.
 *
 *  STATE. Every bind goes through the state cache (gl_state_cache.h) and is put back:
 *  the client arrays by one push and pop of the vertex-array bit (the inputs ride on the
 *  vertex array and two generic attributes, 6 and 7, which alias no fixed-function
 *  array, so no current colour, normal or texture coordinate is disturbed), the bound
 *  program by the program shadow, and the rasteriser and the feedback buffer by hand.
 *  A pose can be asked for inside the ground-normal or bake pass (a tree first seen
 *  there), so the caller's program is always bound again before returning.
 *
 *  TIER 1: none of this exists there. The Win98 renderer has no trees that sway.
 * ==================================================================================== */
#ifndef OPENCNC3D_MESH_WIND_GPU_H
#define OPENCNC3D_MESH_WIND_GPU_H

#ifndef GL_TRANSFORM_FEEDBACK_BUFFER_EXT
#define GL_TRANSFORM_FEEDBACK_BUFFER_EXT  0x8C8E
#endif
#ifndef GL_INTERLEAVED_ATTRIBS_EXT
#define GL_INTERLEAVED_ATTRIBS_EXT        0x8C8C
#endif
#ifndef GL_RASTERIZER_DISCARD_EXT
#define GL_RASTERIZER_DISCARD_EXT         0x8C89
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW                    0x88E0
#endif

typedef void (APIENTRY *MWGPFNBEGINTF)(GLenum);
typedef void (APIENTRY *MWGPFNENDTF)(void);
typedef void (APIENTRY *MWGPFNBINDBUFFERBASE)(GLenum, GLuint, GLuint);
typedef void (APIENTRY *MWGPFNTFVARYINGS)(GLuint, GLsizei, const FxGLchar* const*, GLenum);
typedef void (APIENTRY *MWGPFNBINDATTRIBLOCATION)(GLuint, GLuint, const FxGLchar*);
typedef void (APIENTRY *MWGPFNVERTEXATTRIBPOINTER)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void (APIENTRY *MWGPFNVERTEXATTRIBARRAY)(GLuint);

static MWGPFNBEGINTF             mwg_glBeginTransformFeedback;
static MWGPFNENDTF               mwg_glEndTransformFeedback;
static MWGPFNBINDBUFFERBASE      mwg_glBindBufferBase;
static MWGPFNTFVARYINGS          mwg_glTransformFeedbackVaryings;
static MWGPFNBINDATTRIBLOCATION  mwg_glBindAttribLocation;
static MWGPFNVERTEXATTRIBPOINTER mwg_glVertexAttribPointer;
static MWGPFNVERTEXATTRIBARRAY   mwg_glEnableVertexAttribArray;
static MWGPFNVERTEXATTRIBARRAY   mwg_glDisableVertexAttribArray;

/* The two generic attributes. 6 and 7 are the pair no fixed-function array aliases on
   drivers that alias at all (0 is the position, 2 the normal, 3 the colour, 8 and up
   the texture coordinates). */
enum { MWG_ATTR_PHASE = 6, MWG_ATTR_WEIGHT = 7 };

/* One distinct corner input on the card: 48 bytes.
     0  rest x, y, z and the sway weight (0..255, a whole number)   the vertex array
    16  the flutter phases: sin ph, cos ph, sin 1.7ph, cos 1.7ph    attribute 6
    32  the flutter weight (0..255), weighted, phased, unused       attribute 7 */
struct MeshWindGpuInput { float rest[4], phase[4], weight[4]; };
static_assert(sizeof(MeshWindGpuInput) == 48, "the card's input record is 48 bytes");

/* A source on the card: its inputs, the corner list, and what bounds the wind can move it
   to (the rest box in mesh units, the largest weights, the largest length of each phase
   pair). Kept by the same key the CPU's prepared sources use (mesh_wind_unique_source),
   in a table of its own: a source the CPU's four-way sets push out is still on the card,
   so a tree is never sent twice because of where its key happened to hash. */
struct MeshWindGpuSource {
    GLuint input = 0, corners = 0;
    size_t inputs = 0, count = 0, bytes = 0;
    unsigned long long used = 0;
    double lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
    double w0max = 0, w1max = 0, qa = 0, qb = 0;
    bool weighted = false;
};
static std::unordered_map<MeshKey, MeshWindGpuSource, MeshKeyHash> g_meshWindGpuSources;
static unsigned g_meshWindGpuGeneration = 0;
static unsigned long long g_meshWindGpuClock = 0;
enum { MWG_SOURCE_ENTRIES = 512 };
static const size_t MWG_SOURCE_BYTES = 32u * 1024u * 1024u;

static int g_meshWindGpuMode = -1;          /* 0 off, 1 on, 2 verify */
static int g_meshWindGpuAuto = 0;           /* the switch was unset and the driver chose */
static int g_meshWindGpuApple = 0;          /* a build for macOS: Apple's OpenGL, whatever the GPU */
static int g_meshWindGpuNvidia = 0;         /* the context's vendor is NVIDIA */
static bool g_meshWindGpuTried = false, g_meshWindGpuReady = false;
static bool g_meshWindGpuChecked = false;   /* the run's first pose was compared */
static const char* g_meshWindGpuWhy = "not asked yet";
static GLuint g_meshWindGpuProgram = 0;
enum { MWG_U_MOTION, MWG_U_MOTION2, MWG_U_PLACE, MWG_U_LIFT, MWG_U_ROW0, MWG_U_ROW1, MWG_U_ROW2,
       MWG_U_EXTRA, MWG_U_COUNT };
static GLint g_meshWindGpuLoc[MWG_U_COUNT];
/* The values this file last gave its own program. Nothing else sets them, so a value
   already held is not sent again (one tree's matrix, the frame's wind). */
static float g_meshWindGpuHeld[MWG_U_COUNT][4];
static bool g_meshWindGpuHeldKnown[MWG_U_COUNT];
static size_t g_meshWindGpuSourceBytes = 0;
static unsigned long long g_meshWindGpuPoses = 0, g_meshWindGpuVertices = 0, g_meshWindGpuFallbacks = 0,
    g_meshWindGpuUnavailable = 0, g_meshWindGpuSourceBuilds = 0, g_meshWindGpuSourceHits = 0,
    g_meshWindGpuSourceEvictions = 0, g_meshWindGpuVerified = 0,
    g_meshWindGpuExact = 0, g_meshWindGpuDiffering = 0, g_meshWindGpuOutside = 0,
    g_meshWindGpuReadFailures = 0, g_meshWindGpuCheckFailures = 0, g_meshWindGpuSourceUploadBytes = 0;
static long long g_meshWindGpuMaxUlp = 0;
static double g_meshWindGpuMaxAbs = 0.0, g_meshWindGpuMaxSlack = 0.0;

static int mesh_wind_gpu_mode()
{
    if (g_meshWindGpuMode < 0) {
        /* Asked with the context current: draw_mesh and the mesh dump both run there.
           macOS is decided by the build, not by the vendor string, because an Intel
           Mac's OpenGL reports the GPU's maker ("Intel Inc.", "ATI Technologies Inc.",
           "NVIDIA Corporation") where Apple silicon reports "Apple". */
        const char* vendor = (const char*)glGetString(GL_VENDOR);
        g_meshWindGpuNvidia = vendor && strstr(vendor, "NVIDIA") ? 1 : 0;
#if defined(__APPLE__)
        g_meshWindGpuApple = 1;
#endif
        const char* v = getenv("OPENCNC3D_MESH_WIND_GPU");
        if (v && *v) g_meshWindGpuMode = !strcmp(v, "0") ? 0 : !strcmp(v, "verify") ? 2 : 1;
        else {
            g_meshWindGpuAuto = 1;
            g_meshWindGpuMode = !g_meshWindGpuApple && g_meshWindGpuNvidia ? 1 : 0;
        }
    }
    return g_meshWindGpuMode;
}
/* The card is asked only when the switch is on and the CPU path's own verifier is not
   running: that verifier compares the full loop with the CPU pose, which it needs. */
static bool mesh_wind_gpu_wanted()
{
    return mesh_wind_gpu_mode() != 0 && !mesh_wind_direct_verify_enabled();
}

static void mesh_wind_gpu_drop(MeshWindGpuSource& s)
{
    if (gr_glDeleteBuffers) {
        if (s.input) gr_glDeleteBuffers(1, &s.input);
        if (s.corners) gr_glDeleteBuffers(1, &s.corners);
    }
    g_meshWindGpuSourceBytes -= s.bytes;
    s = MeshWindGpuSource();
}
static void mesh_wind_gpu_release()
{
    for (auto& e : g_meshWindGpuSources) mesh_wind_gpu_drop(e.second);
    g_meshWindGpuSources.clear();
    g_meshWindGpuSourceBytes = 0;
    g_meshWindGpuGeneration = g_groundGen;
}

/* The vertex program: draw_mesh's per-corner wind, fill, tree matrix, facing and
   placement, in its order (see the corner loop there). One input in, one world position
   out, captured by transform feedback. 0.0039215688593685627 is 1.0f/255.0f exactly. */
static const char* MWG_VS =
    "#version 120\n"
    "attribute vec4 aPhase;\n"
    "attribute vec4 aWeight;\n"
    "uniform vec4 uMotion;\n"    /* lean, A1, B1, A2 */
    "uniform vec4 uMotion2;\n"   /* B2, wind x, wind z, flutter scale */
    "uniform vec4 uPlace;\n"     /* cx, cz, cos facing, sin facing */
    "uniform vec4 uLift;\n"      /* model scale, lift, ground, fill height */
    "uniform vec4 uRow0;\n"
    "uniform vec4 uRow1;\n"
    "uniform vec4 uRow2;\n"
    "uniform float uExtra;\n"    /* 1 when the tree's matrix applies */
    "varying vec3 vWorld;\n"
    "void main() {\n"
    "    float vx = gl_Vertex.x, vy = gl_Vertex.y, vz = gl_Vertex.z;\n"
    "    if (aWeight.y > 0.5) {\n"
    "        float amount = uMotion.x * gl_Vertex.w * 0.0039215688593685627;\n"
    "        float up = -0.6 * amount * amount;\n"
    "        if (aWeight.x != 0.0 && aWeight.z > 0.5)\n"
    "            up += uMotion2.w * aWeight.x * 0.0039215688593685627\n"
    "                * (uMotion.y * aPhase.y + uMotion.z * aPhase.x\n"
    "                   + 0.55 * (uMotion.w * aPhase.w + uMotion2.x * aPhase.z));\n"
    "        vx += uMotion2.y * amount;\n"
    "        vy += 1024.0 * up;\n"
    "        vz += uMotion2.z * amount;\n"
    "    }\n"
    "    if (vy > uLift.w) vy = uLift.w;\n"
    "    if (uExtra > 0.5) {\n"
    "        float bx = uRow0.x * vx + uRow0.y * vy + uRow0.z * vz + uRow0.w;\n"
    "        float by = uRow1.x * vx + uRow1.y * vy + uRow1.z * vz + uRow1.w;\n"
    "        float bz = uRow2.x * vx + uRow2.y * vy + uRow2.z * vz + uRow2.w;\n"
    "        vx = bx; vy = by; vz = bz;\n"
    "    }\n"
    "    float mx = vx * uPlace.z + vz * uPlace.w;\n"
    "    float mz = -vx * uPlace.w + vz * uPlace.z;\n"
    "    vWorld = vec3(uPlace.x + mx * uLift.x, vy * uLift.x + 0.0 + uLift.y + uLift.z,\n"
    "                  uPlace.y + mz * uLift.x);\n"
    "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "}\n";

static void* mesh_wind_gpu_proc(const char* first, const char* second)
{
    void* p = SDL_GL_GetProcAddress(first);
    if (!p && second) p = SDL_GL_GetProcAddress(second);
    return p;
}

static bool mesh_wind_gpu_load()
{
    if (g_meshWindGpuTried) return g_meshWindGpuReady;
    g_meshWindGpuTried = true;
    if (!fx_gl_ready || !grass_buf_load()) {
        g_meshWindGpuWhy = "no shaders or no buffer objects";
    } else {
        /* Transform feedback is GL_EXT_transform_feedback on a 2.1 context (Apple's) and
           core from 3.0 (any current Windows driver). The listed extension's own
           spellings are asked for first; a 3.0 context without it is asked for the core
           names. */
        const char* ext = (const char*)glGetString(GL_EXTENSIONS);
        const char* at = ext ? strstr(ext, "GL_EXT_transform_feedback") : NULL;
        bool listed = at && (at[25] == ' ' || at[25] == 0);
        int major = 0, minor = 0;
        const char* ver = (const char*)glGetString(GL_VERSION);
        if (ver) sscanf(ver, "%d.%d", &major, &minor);
        if (!mesh_perf_switch("OPENCNC3D_MESH_WIND_GPU_LOAD_OK")) { listed = false; major = 0; }
        if (!listed && major < 3) {
            g_meshWindGpuWhy = "no transform feedback in this context";
        } else {
            const bool e = listed;
            mwg_glBeginTransformFeedback = (MWGPFNBEGINTF)mesh_wind_gpu_proc(
                e ? "glBeginTransformFeedbackEXT" : "glBeginTransformFeedback",
                e ? "glBeginTransformFeedback" : "glBeginTransformFeedbackEXT");
            mwg_glEndTransformFeedback = (MWGPFNENDTF)mesh_wind_gpu_proc(
                e ? "glEndTransformFeedbackEXT" : "glEndTransformFeedback",
                e ? "glEndTransformFeedback" : "glEndTransformFeedbackEXT");
            mwg_glBindBufferBase = (MWGPFNBINDBUFFERBASE)mesh_wind_gpu_proc(
                e ? "glBindBufferBaseEXT" : "glBindBufferBase",
                e ? "glBindBufferBase" : "glBindBufferBaseEXT");
            mwg_glTransformFeedbackVaryings = (MWGPFNTFVARYINGS)mesh_wind_gpu_proc(
                e ? "glTransformFeedbackVaryingsEXT" : "glTransformFeedbackVaryings",
                e ? "glTransformFeedbackVaryings" : "glTransformFeedbackVaryingsEXT");
            mwg_glBindAttribLocation = (MWGPFNBINDATTRIBLOCATION)mesh_wind_gpu_proc("glBindAttribLocation", "glBindAttribLocationARB");
            mwg_glVertexAttribPointer = (MWGPFNVERTEXATTRIBPOINTER)mesh_wind_gpu_proc("glVertexAttribPointer", "glVertexAttribPointerARB");
            mwg_glEnableVertexAttribArray = (MWGPFNVERTEXATTRIBARRAY)mesh_wind_gpu_proc("glEnableVertexAttribArray", "glEnableVertexAttribArrayARB");
            mwg_glDisableVertexAttribArray = (MWGPFNVERTEXATTRIBARRAY)mesh_wind_gpu_proc("glDisableVertexAttribArray", "glDisableVertexAttribArrayARB");
            if (!mwg_glBeginTransformFeedback || !mwg_glEndTransformFeedback || !mwg_glBindBufferBase
                || !mwg_glTransformFeedbackVaryings || !mwg_glBindAttribLocation || !mwg_glVertexAttribPointer
                || !mwg_glEnableVertexAttribArray || !mwg_glDisableVertexAttribArray) {
                g_meshWindGpuWhy = "a transform feedback or attribute entry point is not exported";
            } else {
                GLuint vs = fx_compile(GL_VERTEX_SHADER, MWG_VS, "tree wind");
                if (!vs) g_meshWindGpuWhy = "the wind program did not compile";
                else {
                    GLuint p = fx_glCreateProgram();
                    fx_glAttachShader(p, vs);
                    mwg_glBindAttribLocation(p, MWG_ATTR_PHASE, "aPhase");
                    mwg_glBindAttribLocation(p, MWG_ATTR_WEIGHT, "aWeight");
                    const FxGLchar* outs[1] = {
                        mesh_perf_switch("OPENCNC3D_MESH_WIND_GPU_LINK_OK") ? "vWorld" : "vNoSuchOutput" };
                    mwg_glTransformFeedbackVaryings(p, 1, outs, GL_INTERLEAVED_ATTRIBS_EXT);
                    fx_glLinkProgram(p);
                    GLint ok = 0;
                    fx_glGetProgramiv(p, GL_LINK_STATUS, &ok);
                    fx_glDeleteShader(vs);
                    if (!ok) {
                        char log[2048]; GLsizei n = 0;
                        if (fx_glGetProgramInfoLog) fx_glGetProgramInfoLog(p, (GLsizei)sizeof log, &n, log);
                        log[n < (GLsizei)sizeof log ? n : (GLsizei)sizeof log - 1] = 0;
                        fprintf(stderr, "MESHWINDGPU|link|failed\n%s\n", log);
                        fx_glDeleteProgram(p);
                        g_meshWindGpuWhy = "the wind program did not link";
                    } else {
                        static const char* const names[MWG_U_COUNT] = {
                            "uMotion", "uMotion2", "uPlace", "uLift", "uRow0", "uRow1", "uRow2", "uExtra" };
                        for (int i = 0; i < MWG_U_COUNT; i++) {
                            g_meshWindGpuLoc[i] = fx_glGetUniformLocation(p, names[i]);
                            g_meshWindGpuHeldKnown[i] = false;
                        }
                        g_meshWindGpuProgram = p;
                        g_meshWindGpuReady = true;
                        g_meshWindGpuWhy = listed ? "GL_EXT_transform_feedback" : "core transform feedback";
                    }
                }
            }
        }
    }
    fprintf(stderr, "MESHWINDGPU|%s|%s\n", g_meshWindGpuReady ? "ready" : "unavailable", g_meshWindGpuWhy);
    return g_meshWindGpuReady;
}

static void mesh_wind_gpu_uniform(int which, float a, float b, float c, float d)
{
    const GLint l = g_meshWindGpuLoc[which];
    if (l < 0) return;
    const float v[4] = { a, b, c, d };
    if (g_meshWindGpuHeldKnown[which] && !memcmp(g_meshWindGpuHeld[which], v, sizeof v)) return;
    memcpy(g_meshWindGpuHeld[which], v, sizeof v); g_meshWindGpuHeldKnown[which] = true;
    if (which == MWG_U_EXTRA) (fx_glUniform1f)(l, a);
    else (fx_glUniform4f)(l, a, b, c, d);
}

/* The card's copy of a source, sent the first time its key is asked for. The key is the
   CPU source's own (mesh_wind_unique_source): the mesh, its triangle, weight and phase
   storage, the range, the mode, the corner count, the deformation revision and the
   ground generation. */
static MeshWindGpuSource* mesh_wind_gpu_source(const PackMesh& mesh, size_t first, size_t last,
                                               int pass, size_t count)
{
    if (g_meshWindGpuGeneration != g_groundGen) mesh_wind_gpu_release();
    MeshKey key;
    const uint64_t fields[13] = {
        (uint64_t)(uintptr_t)&mesh, (uint64_t)(uintptr_t)mesh.tris.data(), mesh.tris.size(),
        (uint64_t)(uintptr_t)mesh.sway.data(), mesh.sway.size(),
        (uint64_t)(uintptr_t)mesh.swayPh.data(), mesh.swayPh.size(),
        first, last, (uint64_t)(unsigned)pass, count, mesh.gpuPoseRevision, g_groundGen };
    key.append((const char*)fields, sizeof fields);
    auto found = g_meshWindGpuSources.find(key);
    if (found != g_meshWindGpuSources.end()) {
        found->second.used = ++g_meshWindGpuClock; g_meshWindGpuSourceHits++;
        return found->second.count == count ? &found->second : NULL;
    }
    const MeshWindUniquePrepared* prepared = mesh_wind_unique_source(mesh, first, last, pass, count);
    if (!prepared || prepared->corners.size() != count) return NULL;
    const size_t inputs = prepared->unique.size();
    if (!inputs || inputs > 0x7fffffffu || count > 0x7fffffffu) return NULL;
    const size_t inputBytes = inputs * sizeof(MeshWindGpuInput), cornerBytes = count * sizeof(uint32_t);
    if (inputBytes + cornerBytes > MWG_SOURCE_BYTES) return NULL;
    /* The table is bounded in entries and bytes; the source asked for least recently goes
       first, and only ever on a miss. */
    while (!g_meshWindGpuSources.empty()
           && (g_meshWindGpuSources.size() >= MWG_SOURCE_ENTRIES
               || g_meshWindGpuSourceBytes + inputBytes + cornerBytes > MWG_SOURCE_BYTES)) {
        auto oldest = g_meshWindGpuSources.begin();
        for (auto it = g_meshWindGpuSources.begin(); it != g_meshWindGpuSources.end(); ++it)
            if (it->second.used < oldest->second.used) oldest = it;
        mesh_wind_gpu_drop(oldest->second);
        g_meshWindGpuSources.erase(oldest);
        g_meshWindGpuSourceEvictions++;
    }
    std::vector<MeshWindGpuInput> records;
    try { records.resize(inputs); } catch (const std::bad_alloc&) { return NULL; }
    MeshWindGpuSource s;
    bool weighted = false;
    for (size_t i = 0; i < inputs; i++) {
        const MeshWindUniqueInput& in = prepared->unique[i];
        MeshWindGpuInput& r = records[i];
        r.rest[0] = in.position[0]; r.rest[1] = in.position[1]; r.rest[2] = in.position[2];
        r.rest[3] = in.weighted ? (float)in.weights[0] : 0.0f;
        for (int k = 0; k < 4; k++) r.phase[k] = in.phased ? in.phase[k] : 0.0f;
        r.weight[0] = in.weighted ? (float)in.weights[1] : 0.0f;
        r.weight[1] = in.weighted ? 1.0f : 0.0f;
        r.weight[2] = in.phased ? 1.0f : 0.0f;
        r.weight[3] = 0.0f;
        for (int k = 0; k < 3; k++) {
            if (!i || in.position[k] < s.lo[k]) s.lo[k] = in.position[k];
            if (!i || in.position[k] > s.hi[k]) s.hi[k] = in.position[k];
        }
        if (in.weighted) {
            weighted = true;
            s.w0max = std::max(s.w0max, (double)in.weights[0]);
            if (in.weights[1] && in.phased) {
                s.w1max = std::max(s.w1max, (double)in.weights[1]);
                s.qa = std::max(s.qa, sqrt((double)in.phase[0] * in.phase[0] + (double)in.phase[1] * in.phase[1]));
                s.qb = std::max(s.qb, sqrt((double)in.phase[2] * in.phase[2] + (double)in.phase[3] * in.phase[3]));
            }
        }
    }
    s.weighted = weighted;
    gr_glGenBuffers(1, &s.input); gr_glGenBuffers(1, &s.corners);
    if (!s.input || !s.corners) { mesh_wind_gpu_drop(s); return NULL; }
    /* Both stores are filled through the array target, which is the binding the state
       cache follows; an element buffer is only a store until it is bound as one. */
    const GLuint was = gls_array_binding();
    gr_glBindBuffer(GL_ARRAY_BUFFER, s.input);
    gr_glBufferData(GL_ARRAY_BUFFER, (FxGLsizeiptr)inputBytes, records.data(), GL_STATIC_DRAW);
    bool ok = mesh_perf_switch("OPENCNC3D_MESH_WIND_GPU_SOURCE_OK") && mesh_store_holds(s.input, inputBytes, false);
    gr_glBindBuffer(GL_ARRAY_BUFFER, s.corners);
    gr_glBufferData(GL_ARRAY_BUFFER, (FxGLsizeiptr)cornerBytes, prepared->corners.data(), GL_STATIC_DRAW);
    ok = ok && mesh_store_holds(s.corners, cornerBytes, false);
    gr_glBindBuffer(GL_ARRAY_BUFFER, was);
    s.inputs = inputs; s.count = count; s.bytes = inputBytes + cornerBytes;
    g_meshWindGpuSourceBytes += s.bytes;
    if (!ok) { mesh_wind_gpu_drop(s); return NULL; }
    s.used = ++g_meshWindGpuClock;
    g_meshWindGpuSourceBuilds++; g_meshWindGpuSourceUploadBytes += s.bytes;
    try {
        return &g_meshWindGpuSources.emplace(key, s).first->second;
    } catch (const std::bad_alloc&) { mesh_wind_gpu_drop(s); return NULL; }
}

/* The box every corner of this pose lies in, in world units, from the source's rest box
   and this draw's wind: the wind moves a corner across by at most the lean times the
   largest sway weight along the wind's bearing, down by the bough's dip, and up or down
   by the flutter's largest swing. Worked in double and padded by more than the float
   arithmetic of either path can stray. */
static void mesh_wind_gpu_bounds(const MeshWindGpuSource& s, float cx, float cz, float rc, float rs,
        float ground, float ylift, float lean, float A1, float B1, float A2, float B2,
        float windX, float windZ, float flutter, float fillY, const float* extraMat,
        float* low, float* high)
{
    double lo[3], hi[3];
    for (int k = 0; k < 3; k++) { lo[k] = s.lo[k]; hi[k] = s.hi[k]; }
    if (s.weighted) {
        const double a = (double)lean * s.w0max / 255.0;
        const double aLo = std::min(0.0, a), aHi = std::max(0.0, a), aMax = fabs(a);
        const double dxa = (double)windX * aLo, dxb = (double)windX * aHi;
        const double dza = (double)windZ * aLo, dzb = (double)windZ * aHi;
        const double swing = fabs((double)flutter) * s.w1max / 255.0
            * (sqrt((double)A1 * A1 + (double)B1 * B1) * s.qa
               + 0.55 * sqrt((double)A2 * A2 + (double)B2 * B2) * s.qb);
        lo[0] += std::min(dxa, dxb); hi[0] += std::max(dxa, dxb);
        lo[2] += std::min(dza, dzb); hi[2] += std::max(dza, dzb);
        lo[1] += 1024.0 * (-0.6 * aMax * aMax - swing); hi[1] += 1024.0 * swing;
    }
    if (hi[1] > fillY) hi[1] = fillY;
    if (lo[1] > fillY) lo[1] = fillY;
    double c[3], h[3];
    for (int k = 0; k < 3; k++) {
        const double pad = 1e-3 + 1e-5 * std::max(fabs(lo[k]), fabs(hi[k]));
        c[k] = 0.5 * (lo[k] + hi[k]); h[k] = 0.5 * (hi[k] - lo[k]) + pad;
    }
    if (extraMat) {
        double c2[3], h2[3];
        for (int r = 0; r < 3; r++) {
            const float* m = &extraMat[r * 4];
            c2[r] = m[0] * c[0] + m[1] * c[1] + m[2] * c[2] + m[3];
            h2[r] = fabs((double)m[0]) * h[0] + fabs((double)m[1]) * h[1] + fabs((double)m[2]) * h[2];
            h2[r] += 1e-3 + 1e-5 * fabs(c2[r]);
        }
        memcpy(c, c2, sizeof c); memcpy(h, h2, sizeof h);
    }
    const double S = MODEL_SCALE, aS = fabs(S);
    const double wc[3] = { cx + S * (rc * c[0] + rs * c[2]), S * c[1] + ylift + ground,
                           cz + S * (-rs * c[0] + rc * c[2]) };
    const double wh[3] = { aS * (fabs((double)rc) * h[0] + fabs((double)rs) * h[2]), aS * h[1],
                           aS * (fabs((double)rs) * h[0] + fabs((double)rc) * h[2]) };
    for (int k = 0; k < 3; k++) {
        const double pad = 2e-4 + 1e-5 * (fabs(wc[k]) + wh[k]);
        low[k] = (float)(wc[k] - wh[k] - pad);
        high[k] = (float)(wc[k] + wh[k] + pad);
    }
}

/* Floats in an order where neighbours are one apart, for a distance in units of the
   last place. +0 and -0 are both 0. */
static long long mesh_wind_gpu_ordered(float f)
{
    int32_t i; memcpy(&i, &f, sizeof i);
    return i >= 0 ? (long long)i : (long long)INT32_MIN - (long long)i;
}

/* The CPU's pose of the same draw against the card's, read back. Returns whether they
   agree to within `tolerance` world units (any NaN or infinity disagrees), and keeps the
   largest difference, in world units and in units of the last place, and how many
   corners of either fell outside the box the culling was given. */
static bool mesh_wind_gpu_compare(GLuint vbo, size_t count, const float* box_lo, const float* box_hi,
        const PackMesh& mesh, size_t first, size_t last, int pass,
        float cx, float cz, float rc, float rs, float ground, float ylift,
        float lean, float A1, float B1, float A2, float B2, const float* extraMat, double tolerance)
{
    std::vector<float> card;
    try { card.resize(count * 3); } catch (const std::bad_alloc&) { g_meshWindGpuReadFailures++; return false; }
    if (!mesh_buffer_read(vbo, count * 3 * sizeof(float), card.data())) { g_meshWindGpuReadFailures++; return false; }
    float clo[3], chi[3];
    if (!mesh_wind_positions_prepared(mesh, first, last, pass, count, cx, cz, rc, rs, ground, ylift,
                                      lean, A1, B1, A2, B2, extraMat, clo, chi)
        || g_meshGeometryUpload.size() < count * 3) {
        g_meshWindGpuReadFailures++; return false;
    }
    const float* cpu = g_meshGeometryUpload.data();
    long long worstUlp = 0; double worstAbs = 0.0; unsigned long long differing = 0, outside = 0;
    bool finite = true;
    for (size_t v = 0; v < count; v++) {
        bool out = false;
        for (int k = 0; k < 3; k++) {
            const float a = cpu[v * 3 + k], b = card[v * 3 + k];
            if (!std::isfinite(a) || !std::isfinite(b)) { finite = false; continue; }
            if (memcmp(&a, &b, sizeof a)) differing++;
            const long long u = llabs(mesh_wind_gpu_ordered(a) - mesh_wind_gpu_ordered(b));
            worstUlp = std::max(worstUlp, u);
            worstAbs = std::max(worstAbs, fabs((double)a - (double)b));
            if (a < box_lo[k] || a > box_hi[k] || b < box_lo[k] || b > box_hi[k]) out = true;
        }
        if (out) outside++;
    }
    /* How much wider the box is than the CPU pose's own, on its loosest side. */
    for (int k = 0; k < 3; k++)
        g_meshWindGpuMaxSlack = std::max(g_meshWindGpuMaxSlack,
            std::max((double)box_hi[k] - chi[k], (double)clo[k] - box_lo[k]));
    g_meshWindGpuMaxUlp = std::max(g_meshWindGpuMaxUlp, worstUlp);
    g_meshWindGpuMaxAbs = std::max(g_meshWindGpuMaxAbs, worstAbs);
    g_meshWindGpuDiffering += differing; g_meshWindGpuOutside += outside;
    g_meshWindGpuVerified++;
    if (!differing && finite) g_meshWindGpuExact++;
    if ((!finite || outside) && g_meshWindGpuCheckFailures < 8)
        fprintf(stderr, "MESHWINDGPU|verify|mesh=%s|pass=%d|count=%zu|finite=%d|outside=%llu|max_abs=%.9g|max_ulp=%lld\n",
                mesh.name, pass, count, finite ? 1 : 0, outside, worstAbs, worstUlp);
    return finite && worstAbs <= tolerance;
}

/* Poses one draw on the card into a frame store and hands it to `geometry` with its
   box. False leaves `geometry` untouched, and the caller poses on the CPU as before. */
static bool mesh_wind_gpu_pose(MeshGeometryPose& geometry, const PackMesh& mesh, size_t first, size_t last,
        int pass, size_t count, float cx, float cz, float rc, float rs, float ground, float ylift,
        float lean, float A1, float B1, float A2, float B2, const float* extraMat)
{
    if (!mesh_wind_gpu_wanted() || geometry.gpu || !count) return false;
    if (!mesh_wind_gpu_load()) { g_meshWindGpuUnavailable++; return false; }
    if (count > 64u * 1024u * 1024u / (3 * sizeof(float))) { g_meshWindGpuFallbacks++; return false; }
    MeshWindGpuSource* s = mesh_wind_gpu_source(mesh, first, last, pass, count);
    if (!s) { g_meshWindGpuFallbacks++; return false; }
    const size_t bytes = count * 3 * sizeof(float);
    const GLuint vbo = mesh_batch_frame_store(bytes);
    if (!vbo) { g_meshWindGpuFallbacks++; return false; }
    /* The client arrays and both buffer bindings are put back by the pop below, the
       array binding included, so the pose store is sized under the same push. A store the
       driver did not make is given back and the CPU poses this draw. */
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    gr_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gr_glBufferData(GL_ARRAY_BUFFER, (FxGLsizeiptr)bytes, NULL, GL_STREAM_DRAW);
    if (!mesh_perf_switch("OPENCNC3D_MESH_WIND_GPU_STORE_OK") || !mesh_store_holds(vbo, bytes)) {
        mesh_frame_store_release(g_meshFrameStores[g_meshFrameNext - 1]);
        glPopClientAttrib();
        g_meshWindGpuFallbacks++; return false;
    }
    /* The frame's constants, by draw_mesh's own expressions. */
    const float windX = 1024.0f * sinf((114.086f + g_fx.tree3d_wind_dir - 183.21429f) * 0.0174532925f);
    const float windZ = -1024.0f * cosf((114.086f + g_fx.tree3d_wind_dir - 183.21429f) * 0.0174532925f);
    const float flutter = 0.007f * g_fx.tree3d_flutter / 0.07143f;

    GLint caller = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &caller);
    fx_glUseProgram(g_meshWindGpuProgram);
    mesh_wind_gpu_uniform(MWG_U_MOTION, lean, A1, B1, A2);
    mesh_wind_gpu_uniform(MWG_U_MOTION2, B2, windX, windZ, flutter);
    mesh_wind_gpu_uniform(MWG_U_PLACE, cx, cz, rc, rs);
    mesh_wind_gpu_uniform(MWG_U_LIFT, MODEL_SCALE, ylift, ground, g_meshFillY);
    if (extraMat) {
        mesh_wind_gpu_uniform(MWG_U_ROW0, extraMat[0], extraMat[1], extraMat[2], extraMat[3]);
        mesh_wind_gpu_uniform(MWG_U_ROW1, extraMat[4], extraMat[5], extraMat[6], extraMat[7]);
        mesh_wind_gpu_uniform(MWG_U_ROW2, extraMat[8], extraMat[9], extraMat[10], extraMat[11]);
    }
    mesh_wind_gpu_uniform(MWG_U_EXTRA, extraMat ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

    gr_glBindBuffer(GL_ARRAY_BUFFER, s->input);
    glVertexPointer(4, GL_FLOAT, (GLsizei)sizeof(MeshWindGpuInput), (const void*)0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    mwg_glVertexAttribPointer(MWG_ATTR_PHASE, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(MeshWindGpuInput),
                              (const void*)offsetof(MeshWindGpuInput, phase));
    mwg_glVertexAttribPointer(MWG_ATTR_WEIGHT, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(MeshWindGpuInput),
                              (const void*)offsetof(MeshWindGpuInput, weight));
    mwg_glEnableVertexAttribArray(MWG_ATTR_PHASE);
    mwg_glEnableVertexAttribArray(MWG_ATTR_WEIGHT);
    gr_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->corners);
    glEnable(GL_RASTERIZER_DISCARD_EXT);
    mwg_glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER_EXT, 0, vbo);
    mwg_glBeginTransformFeedback(GL_POINTS);
    glDrawElements(GL_POINTS, (GLsizei)count, GL_UNSIGNED_INT, (const void*)0);
    mwg_glEndTransformFeedback();
    mwg_glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER_EXT, 0, 0);
    glDisable(GL_RASTERIZER_DISCARD_EXT);
    mwg_glDisableVertexAttribArray(MWG_ATTR_PHASE);
    mwg_glDisableVertexAttribArray(MWG_ATTR_WEIGHT);
    glPopClientAttrib();
    fx_glUseProgram((GLuint)caller);

    float lo[3], hi[3];
    mesh_wind_gpu_bounds(*s, cx, cz, rc, rs, ground, ylift, lean, A1, B1, A2, B2,
                         windX, windZ, flutter, g_meshFillY, extraMat, lo, hi);
    /* THE RUN'S FIRST POSE IS CHECKED against the CPU's, once, so a driver that claims
       transform feedback and writes something else is caught before a tree is drawn from
       it: the pose is then made on the CPU, and so is every later one. A thousandth of a
       cell is hundreds of times what the two paths' rounding differs by. */
    if (!g_meshWindGpuChecked || mesh_wind_gpu_mode() == 2) {
        const bool first_check = !g_meshWindGpuChecked;
        g_meshWindGpuChecked = true;
        const double tolerance = first_check && !mesh_perf_switch("OPENCNC3D_MESH_WIND_GPU_CHECK_OK") ? -1.0 : 1e-3;
        const bool agree = mesh_wind_gpu_compare(vbo, count, lo, hi, mesh, first, last, pass, cx, cz, rc, rs,
                                                 ground, ylift, lean, A1, B1, A2, B2, extraMat, tolerance);
        if (first_check && !agree) {
            fprintf(stderr, "MESHWINDGPU|unavailable|the first pose did not match the CPU's; posing on the CPU\n");
            g_meshWindGpuReady = false; g_meshWindGpuWhy = "the first pose did not match the CPU's";
            g_meshWindGpuCheckFailures++;
            g_meshWindGpuFallbacks++;
            return false;
        }
    }
    geometry.gpu = vbo; geometry.gpuCount = count;
    memcpy(geometry.lo, lo, sizeof lo); memcpy(geometry.hi, hi, sizeof hi); geometry.boundsValid = true;
    g_meshWindGpuPoses++; g_meshWindGpuVertices += count;
    return true;
}

static void mesh_wind_gpu_dump()
{
    printf("MESHWINDGPU|mode=%d|auto=%d|apple=%d|nvidia=%d|ready=%d|poses=%llu|vertices=%llu|fallbacks=%llu|unavailable=%llu"
           "|sources=%llu|source_hits=%llu|source_evictions=%llu|source_entries=%zu|source_bytes=%zu"
           "|source_upload_bytes=%llu|verified=%llu|exact=%llu|differing_floats=%llu"
           "|max_ulp=%lld|max_abs_e9=%llu|max_slack_e6=%llu|outside=%llu|read_failures=%llu|check_failures=%llu\n",
           mesh_wind_gpu_mode(), g_meshWindGpuAuto, g_meshWindGpuApple, g_meshWindGpuNvidia, g_meshWindGpuReady ? 1 : 0, g_meshWindGpuPoses, g_meshWindGpuVertices,
           g_meshWindGpuFallbacks, g_meshWindGpuUnavailable, g_meshWindGpuSourceBuilds, g_meshWindGpuSourceHits,
           g_meshWindGpuSourceEvictions, g_meshWindGpuSources.size(), g_meshWindGpuSourceBytes,
           g_meshWindGpuSourceUploadBytes, g_meshWindGpuVerified, g_meshWindGpuExact, g_meshWindGpuDiffering,
           g_meshWindGpuMaxUlp, (unsigned long long)(g_meshWindGpuMaxAbs * 1e9 + 0.5),
           (unsigned long long)(g_meshWindGpuMaxSlack * 1e6 + 0.5), g_meshWindGpuOutside,
           g_meshWindGpuReadFailures, g_meshWindGpuCheckFailures);
    fflush(stdout);
}

#endif
