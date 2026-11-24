/* Enhanced remaster submission. Keep the existing CPU vertex equations exactly;
   cache their finished world-space output instead of changing shader coordinates.
   Runs retain texture/wrap/order, including partial construction and loose pieces.
   Tier 1 uses its separate renderer. Classic and unavailable VBOs retain the
   original path. The environment switches are diagnostic A/B controls only. */
#include <unordered_set>
#include <array>
#include <list>
#include <deque>
#include <new>
#include "mesh_pose_storage.h"
/* Exact input bytes, held inline: no per-draw string allocation. Hash collisions
   are resolved by the full byte comparison, including float representations. */
struct MeshKey {
    std::array<unsigned char, 512> data;
    size_t length = 0;
    mutable size_t hashValue = 0;
    mutable bool hashed = false;
    void reserve(size_t) {}
    bool empty() const { return length == 0; }
    void append(const char* p, size_t n) {
        if (length + n > data.size()) abort(); /* key schema must stay bounded */
        memcpy(data.data() + length, p, n); length += n; hashed = false;
    }
    bool operator==(const MeshKey& b) const {
        return length == b.length && !memcmp(data.data(), b.data.data(), length);
    }
    size_t hash() const {
        if (!hashed) {
            uint64_t h = 0x9e3779b97f4a7c15ull ^ length;
            for (size_t i = 0; i < length; i += 8) {
                uint64_t v = 0; memcpy(&v, data.data() + i, std::min((size_t)8, length - i));
                v ^= v >> 30; v *= 0xbf58476d1ce4e5b9ull;
                v ^= v >> 27; v *= 0x94d049bb133111ebull;
                h ^= (v ^ (v >> 31)) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            }
            hashValue = (size_t)(h ^ (h >> 32)); hashed = true;
        }
        return hashValue;
    }
};
struct MeshKeyHash { size_t operator()(const MeshKey& k) const { return k.hash(); } };
static bool mesh_perf_switch(const char* name);
static bool ant_perf_switch(const char* name);
struct MeshBatchVertex {
    float p[3], uv[2], n[3];
    unsigned char c[4];
};
struct MeshBatchAttributes { float uv[2], n[3]; unsigned char c[4]; };
struct MeshBatchRun { int tex, wrap; bool x2; size_t first, count; };
struct MeshBatchEntry {
    GLuint vbo = 0;
    size_t bytes = 0;
    unsigned used = 0;
    float lo[3], hi[3];
    std::vector<MeshBatchRun> runs;
    MeshBatchVertex last;
    bool uv = false;
    GLuint positionVbo = 0;
    bool compact = false;
    /* A separate store of RGBA bytes, one per corner, read in place of the colours in
       vbo (the ember glow's kept pieces, mesh_glow_keep). 0 for every other entry. */
    GLuint colourVbo = 0;
    std::list<MeshKey>::iterator lru;
};
static std::unordered_map<MeshKey, MeshBatchEntry, MeshKeyHash> g_meshBatchCache;
static std::list<MeshKey> g_meshBatchLRU;
static unsigned long g_meshBatchEvictions = 0, g_meshBatchMisses = 0;
static unsigned long long g_meshBatchAdmissionRefused = 0;
enum { MESH_BATCH_MAX_ENTRIES = 8192 };
static size_t g_meshBatchBytes = 0;
static GLuint g_meshBatchStream = 0;
/* Animated output survives only for the current draw_frame. Recycle separate
   orphaned stores on the next frame instead of overwriting one in-flight store
   between every object. Both this pool and the static cache have hard bounds. */
struct MeshFrameStore { GLuint vbo=0; size_t bytes=0; unsigned lastUsed=0; };
static std::vector<MeshFrameStore> g_meshFrameStores;
static std::unordered_map<MeshKey,MeshBatchEntry,MeshKeyHash> g_meshFrameCache;
static unsigned g_meshFrameSerial=~0u;
static size_t g_meshFrameNext=0, g_meshFrameBytes=0;
static unsigned long g_meshFrameHits=0;
static unsigned long g_meshFrameFallbacks=0, g_meshFrameTrimmed=0;
static void mesh_frame_store_release(MeshFrameStore& s);
static void mesh_batch_frame_begin() {
    if (g_meshFrameSerial == g_drawFrameSerial) return;
    g_meshFrameCache.clear(); g_meshFrameNext=0; g_meshFrameSerial=g_drawFrameSerial;
    if (mesh_perf_switch("OPENCNC3D_MESH_POOL_TRIM")) {
        for (auto& s : g_meshFrameStores)
            if (s.vbo && g_drawFrameSerial - s.lastUsed > 60u) mesh_frame_store_release(s);
        while (!g_meshFrameStores.empty() && !g_meshFrameStores.back().vbo)
            g_meshFrameStores.pop_back();
    }
}
static const MeshBatchEntry* mesh_batch_frame_find(const MeshKey& key) {
    mesh_batch_frame_begin();
    auto it=g_meshFrameCache.find(key);
    return it==g_meshFrameCache.end()?NULL:&it->second;
}
static void mesh_batch_frame_keep(const MeshKey& key,const MeshBatchEntry& entry) {
    /* A failed copy must not leave a default empty entry for this key. The
       frame store still owns the submitted buffer, so only reuse is lost. */
    try {g_meshFrameCache.emplace(key,entry);}
    catch(const std::bad_alloc&) {g_meshFrameFallbacks++;}
}
/* PLACED PARTS SEEN THIS FRAME AND THE LAST (draw_mesh, placedStill). A key that comes
   back on the next frame belongs to a part that did not move, and that part goes into the
   persistent cache; a key seen once is rebuilt per frame as before. Two sets of keys,
   swapped when a new frame starts. */
static std::unordered_set<MeshKey, MeshKeyHash> g_meshPlacedPrev, g_meshPlacedNow;
static unsigned g_meshPlacedSerial = ~0u;
static unsigned long g_meshPlacedPromoted = 0;
static bool mesh_batch_placed_seen(const MeshKey& key) {
    if (g_meshPlacedSerial != g_drawFrameSerial) {
        g_meshPlacedPrev.swap(g_meshPlacedNow);
        g_meshPlacedNow.clear();
        g_meshPlacedSerial = g_drawFrameSerial;
    }
    g_meshPlacedNow.insert(key);
    const bool again = g_meshPlacedPrev.count(key) != 0;
    if (again) g_meshPlacedPromoted++;
    return again;
}
/* Many small moving pieces can exhaust the descriptor count while most of the
   existing byte budget is unused. Raising only the count keeps the same 64 MiB
   GPU ceiling and every allocation/refusal fallback. Launch-time diagnostic. */
static size_t mesh_frame_pool_entries() {
    static const size_t count=[](){const char* p=getenv("OPENCNC3D_MESH_FRAME_POOL_ENTRIES");
        if(!p||!*p)return size_t(2048);char* end=NULL;const long n=strtol(p,&end,10);
        return end&&!*end&&n>=16&&n<=8192?size_t(n):size_t(2048);}();return count;
}
static GLuint mesh_batch_frame_store(size_t bytes) {
    mesh_batch_frame_begin();
    const size_t cap=64u*1024u*1024u;
    if(g_meshFrameNext>=mesh_frame_pool_entries() || bytes>cap) { g_meshFrameFallbacks++; return 0; }
    if(g_meshFrameNext==g_meshFrameStores.size()) {
        try {g_meshFrameStores.push_back({});}
        catch(const std::bad_alloc&) {g_meshFrameFallbacks++;return 0;}
    }
    auto& s=g_meshFrameStores[g_meshFrameNext];
    if(g_meshFrameBytes-s.bytes+bytes>cap && mesh_perf_switch("OPENCNC3D_MESH_POOL_TRIM"))
        /* Tail stores have not been handed to any current-frame command. Deleting
           them is safe: GL retains storage needed by earlier queued draws. */
        for (size_t i=g_meshFrameStores.size(); i>g_meshFrameNext+1 && g_meshFrameBytes-s.bytes+bytes>cap;)
            mesh_frame_store_release(g_meshFrameStores[--i]);
    if(g_meshFrameBytes-s.bytes+bytes>cap) { g_meshFrameFallbacks++; return 0; }
    if(!s.vbo) gr_glGenBuffers(1,&s.vbo);
    g_meshFrameBytes=g_meshFrameBytes-s.bytes+bytes; s.bytes=bytes;
    s.lastUsed=g_drawFrameSerial;
    g_meshFrameNext++;
    return s.vbo;
}
static bool mesh_perf_switch(const char* name);
static bool g_meshBatchEditor = false;
/* WHETHER A REFILLED STORE TOOK ITS DATA. glBufferData can fail for want of memory and
   leave the store empty, so the size the driver kept is checked after each fill. Asking
   is a round trip to the driver, and the per-frame stores are refilled with nearly the
   same size every frame, so each store remembers the largest fill it has been seen to
   hold and is asked again only for a larger one (remember=false: always ask). The store
   must be the one bound to GL_ARRAY_BUFFER. */
static std::map<GLuint, size_t> g_meshVerified;
static void mesh_frame_store_release(MeshFrameStore& s) {
    if (!s.vbo) return;
    g_meshVerified.erase(s.vbo); gr_glDeleteBuffers(1,&s.vbo);
    g_meshFrameBytes -= s.bytes; s.vbo=0; s.bytes=0; g_meshFrameTrimmed++;
}
static bool mesh_store_holds_at(const char* file, int line, GLuint vbo, size_t bytes, bool remember = true) {
    if (remember) {
        auto it = g_meshVerified.find(vbo);
        if (it != g_meshVerified.end() && it->second >= bytes) return true;
    }
    gls_query_note("glGetBufferParameteriv", file, line);
    GLint allocated = 0; (gr_glGetBufferParameteriv)(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);
    const bool ok = allocated == (GLint)bytes;
    if (ok && remember) { size_t& seen = g_meshVerified[vbo]; if (bytes > seen) seen = bytes; }
    return ok;
}
#define mesh_store_holds(...) mesh_store_holds_at(__FILE__, __LINE__, __VA_ARGS__)
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
/* Collection emits no geometry. Keep its two varying polygon enables logical
   until replay, rather than submitting every piece's toggles twice. The driver
   cache continues to describe actual GL. Every flush applies the final logical
   state, including empty prefixes and ordinary/client fallback boundaries. */
static bool mesh_fragment_polygon_defer_enabled() {
    static const bool on=[](){
        const char* p=getenv("OPENCNC3D_FRAGMENT_POLYGON_DEFER");
        if(p)return strcmp(p,"0")!=0;
#ifdef _WIN32
        return true;
#else
        /* Driver-call reduction is for Windows. Preserve the existing Mac path
           unless a controlled diagnostic explicitly asks for the same replay. */
        return false;
#endif
    }();
    return on;
}
static bool g_meshFragmentStateDeferred=false;
static bool g_meshFragmentCull=false,g_meshFragmentOffset=false;
static bool mesh_fragment_polygon(GLenum cap) {
    if(g_meshFragmentStateDeferred&&g_meshFragmentCollect)
        return cap==GL_CULL_FACE?g_meshFragmentCull:g_meshFragmentOffset;
    return glIsEnabled(cap)==GL_TRUE;
}
static bool mesh_fragment_polygon_set(GLenum cap,bool on) {
    if(!g_meshFragmentStateDeferred||!g_meshFragmentCollect)return false;
    if(cap==GL_CULL_FACE)g_meshFragmentCull=on;else g_meshFragmentOffset=on;
    return true;
}
static void mesh_fragment_polygon_apply() {
    if(!g_meshFragmentStateDeferred)return;
    if(g_meshFragmentCull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
    if(g_meshFragmentOffset)glEnable(GL_POLYGON_OFFSET_FILL);else glDisable(GL_POLYGON_OFFSET_FILL);
}
static std::vector<MeshBatchVertex> g_meshFragmentVertices;
/* Only bytes consumed by this pass cross the CPU/GL boundary: sun depth uses
   positions (12 B), colour uses positions/UV/colour (24 B), normal output keeps
   the full 36 B record. The exact CPU stream and client fallback stay unchanged. */
static std::vector<unsigned char> g_meshFragmentPacked;
static bool g_meshFragmentPackedCollect=false;
static size_t g_meshFragmentPackedStride=0;
static unsigned char* mesh_fragment_pack_space(size_t count,bool depth) {
    if(!mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_PACK_ALLOC_OK"))return NULL;
    const size_t stride=depth?3*sizeof(float):5*sizeof(float)+4;
    const size_t old=g_meshFragmentPacked.size(),cap=16u*1024u*1024u;
    if(count>cap/stride||old>cap-count*stride)return NULL;
    const size_t bytes=old+count*stride;
    try {
        if(g_meshFragmentPacked.capacity()<bytes) {
            const size_t grow=std::min(cap,std::max(bytes,std::max((size_t)4096,g_meshFragmentPacked.capacity()*2)));
            std::vector<unsigned char> next(grow);
            if(old)memcpy(next.data(),g_meshFragmentPacked.data(),old);
            g_meshFragmentPacked.swap(next);
        }
        g_meshFragmentPacked.resize(bytes);
    } catch(const std::bad_alloc&){return NULL;}
    g_meshFragmentPackedStride=stride;return g_meshFragmentPacked.data()+old;
}
static bool mesh_fragment_pack_append(const std::vector<MeshBatchVertex>& vertices,bool depth) {
    unsigned char* data=mesh_fragment_pack_space(vertices.size(),depth);if(!data)return false;
    const size_t stride=g_meshFragmentPackedStride;
    for(size_t i=0;i<vertices.size();++i) {
        unsigned char* d=data+i*stride;const MeshBatchVertex& v=vertices[i];
        memcpy(d,v.p,sizeof v.p);
        if(!depth){memcpy(d+3*sizeof(float),v.uv,sizeof v.uv);memcpy(d+5*sizeof(float),v.c,sizeof v.c);}
    }
    return true;
}
static unsigned long long g_meshFragmentPackFallbacks=0;
static unsigned long long g_meshFragmentPackedBytes=0,g_meshFragmentUploadBytes=0,g_meshFragmentFullBytes=0;
static std::vector<MeshFragmentCommand> g_meshFragmentCommands;
static unsigned long g_meshFragmentFlushes = 0, g_meshFragmentRanges = 0;
static unsigned long long g_meshFragmentDrawsSaved=0;
/* Keep only the latest exact stream of each fragment scope. No time/opacity keys
   accumulate: a changed stream replaces that instance/pass's previous stream. */
struct MeshFragmentInput {
    MeshKey key;
    size_t first=0, count=0, command=~(size_t)0;
};
struct MeshFragmentStore {
    std::vector<MeshBatchVertex> vertices;
    std::vector<MeshFragmentCommand> commands;
    std::vector<MeshFragmentInput> inputs;
    GLuint vbo=0;
    size_t bytes=0, gpuBytes=0, gpuStride=sizeof(MeshBatchVertex);
    unsigned used=0;
    /* The resting rule (mesh_fragment_rest_mode): the frame before which this scope
       runs without its store, how many frames it rested last time, and how many tries
       in a row reused too little. */
    unsigned restUntil=0, restWait=0, restMisses=0;
};
static std::unordered_map<MeshKey,MeshFragmentStore,MeshKeyHash> g_meshFragmentStores;
static size_t g_meshFragmentStoreBytes=0;
static MeshFragmentStore* g_meshFragmentStore=NULL;
static std::vector<MeshFragmentInput> g_meshFragmentInputs;
static size_t g_meshFragmentCount=0;
static bool g_meshFragmentChanged=true;
static unsigned long g_meshFragmentInputHits=0, g_meshFragmentStreamHits=0;
/* A STORE THAT SAVES NOTHING RESTS. A scope keeps its latest stream so that pieces
   which did not move can be copied instead of prepared again, and a scope whose pieces
   all moved still pays for keeping it: every corner collected as a full 36 byte record,
   copied into the scope's stream, packed again on its way to the card, and the stream
   handed to the store, which on Windows is a fresh allocation and a page fault for every
   page of it, frame after frame. The flying and sinking pieces of a destroyed building
   are exactly that case. Measured on the 32-bit Windows build, on the barrels of Red Alert's first
   Allied mission bringing down the Power Plant beside them: with no store at all while the
   clock moves (OPENCNC3D_FRAGMENT_ACTIVE_REUSE=0) those frames were 9 to 16 percent
   shorter, while the stores' input reuse had saved under a seventh of the preparation.
   This rule, which still tries each store now and then, made them 4 to 10 percent shorter
   once the first 20 engine ticks (which the pauses of first use dominate) were past, and
   the damaged bases measured with the clock running (the first ant mission's, a Tiberian
   Dawn GDI base) kept over 99 percent of their reuse.
   So a scope that used its store and reused less than a quarter of its inputs (and did
   not reuse the whole stream) on two tries in a row rests: for the next 2 frames it draws
   as if it had no store, then tries again, waiting twice as long after each fruitless try,
   up to 16 frames. One try that reuses enough ends the rest. Two in a row, because a scope
   whose pieces change only with the engine's tick (a sinking wreck, an ember glow's
   flicker) misses on the first frame after each tick and reuses everything on the frames
   drawn before the next one: resting it after that one miss would throw those away.
   A resting scope leaves its store as it was, so the try compares against the last stream
   it kept; every input is matched on its exact key, so an older stream is as exact as a
   newer one. It can miss where a stream one frame old would not, when the pieces stopped
   moving during the rest, and the scope then rests once more before its next try finds
   them still. Counting such a miss only as a first one, and trying again the next frame,
   found more reuse but paid for it in tries: measured on the 32-bit Windows build, it gave back
   3 to 5 of the 8 to 9 percent the rule saves while the pieces fly at one frame an engine
   tick, and changed nothing at four.
   While the clock stands still (the game paused, or one moment drawn again until the
   picture settles) nothing that moves with the clock moves, and every stream can be
   reused whole: so no scope rests then. A resting scope tries at once; the first still
   frame refreshes its store, and from the second on it reuses as if it had never rested.
   Without this, a pause right after an explosion stayed slow for its first 30 or so frames
   (measured on the 32-bit Windows build: its first 40 frames took 38 to 45 percent longer than
   with every store in use; with it, only the first still frame costs more, about one
   explosion frame).
   Both paths send the same vertices: this decides only where they are assembled.
   OPENCNC3D_FRAGMENT_REST=0 keeps every store in use every frame, the previous rule;
   =verify does that too and counts the scopes the rule would have rested that reused
   (FRAGMENTREST missed). */
static unsigned long long g_meshFragmentStoreInputs=0, g_meshFragmentCollectedBytes=0;
static unsigned long long g_meshFragmentRestSkips=0, g_meshFragmentRestTries=0,
                          g_meshFragmentRestWarm=0, g_meshFragmentRestMissed=0;
static MeshKey g_meshFragmentRestSlot;
static unsigned long long g_meshFragmentRestStill=0;
static bool g_meshFragmentRestNote=false, g_meshFragmentRestWould=false;
static unsigned long g_meshFragmentRestHits0=0, g_meshFragmentRestStreams0=0;
static unsigned long long g_meshFragmentRestInputs0=0;
static int mesh_fragment_rest_mode() {
    static const int mode=[](){
        const char* v=getenv("OPENCNC3D_FRAGMENT_REST");
        return !v ? 1 : !strcmp(v,"0") ? 0 : !strcmp(v,"verify") ? 2 : 1;
    }();
    return mode;
}
static const size_t MESH_FRAGMENT_STORE_CAP=64u*1024u*1024u;
static unsigned g_meshFragmentClockFrame=~0u;
static int g_meshFragmentClockEngine=-1;
static float g_meshFragmentClockAlpha=-1.0f;
static bool g_meshFragmentClockAgain=false;
static bool mesh_fragment_clock_admit() {
    if(g_meshFragmentClockFrame!=g_drawFrameSerial) {
        g_meshFragmentClockAgain=g_meshFragmentClockFrame+1u==g_drawFrameSerial
            && g_meshFragmentClockEngine==g_engineFrame && g_meshFragmentClockAlpha==g_tickAlpha;
        g_meshFragmentClockFrame=g_drawFrameSerial;g_meshFragmentClockEngine=g_engineFrame;
        g_meshFragmentClockAlpha=g_tickAlpha;
    }
    return g_meshFragmentClockAgain;
}
static void mesh_fragment_store_release(MeshFragmentStore& s) {
    if(s.vbo) { g_meshVerified.erase(s.vbo); gr_glDeleteBuffers(1,&s.vbo); }
    g_meshFragmentStoreBytes-=s.bytes; s=MeshFragmentStore();
}
/* Retained vertex CPU allocation + VBO allocation + inline keys/input allocation +
   commands and their run allocations. The bounded map/node overhead is charged too.
   Existing current-scope assembly scratch remains under its separate64MiB bound. */
static size_t mesh_fragment_store_footprint(size_t gpuBytes,size_t vertices,
        const std::vector<MeshFragmentInput>& inputs,const std::vector<MeshFragmentCommand>& commands) {
    size_t bytes=gpuBytes+vertices*sizeof(MeshBatchVertex)
        +inputs.capacity()*sizeof(MeshFragmentInput)+commands.capacity()*sizeof(MeshFragmentCommand)+96u;
    for(const auto& command:commands)bytes+=command.entry.runs.capacity()*sizeof(MeshBatchRun)
        +(command.entry.runs.capacity()?32u:0u);
    return bytes;
}
static size_t mesh_fragment_store_limit() {
    return ant_perf_switch("OPENCNC3D_FRAGMENT_ACTIVE_REUSE") ? 256u : 64u;
}
static bool mesh_fragment_store_budget(size_t bytes) {
    const size_t cap=mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_BUDGET_FULL") ? MESH_FRAGMENT_STORE_CAP : 256u;
    const size_t nodes=mesh_fragment_store_limit()*(sizeof(MeshKey)+sizeof(MeshFragmentStore)+64u);
    if(bytes+nodes>cap)return false;
    while(g_meshFragmentStoreBytes-g_meshFragmentStore->bytes+bytes+nodes>cap) {
        auto oldest=g_meshFragmentStores.end();
        for(auto i=g_meshFragmentStores.begin();i!=g_meshFragmentStores.end();++i)
            if(&i->second!=g_meshFragmentStore && (oldest==g_meshFragmentStores.end()
               || (unsigned)(g_drawFrameSerial-i->second.used)>(unsigned)(g_drawFrameSerial-oldest->second.used))) oldest=i;
        if(oldest==g_meshFragmentStores.end())return false;
        mesh_fragment_store_release(oldest->second);g_meshFragmentStores.erase(oldest);
    }
    return true;
}
static void mesh_fragment_store_begin(const void* identity,unsigned variant) {
    g_meshFragmentStore=NULL;g_meshFragmentInputs.clear();g_meshFragmentCount=0;g_meshFragmentRestNote=false;
    g_meshFragmentChanged=true;
    if(!identity || !mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_REUSE")
        || (!ant_perf_switch("OPENCNC3D_FRAGMENT_ACTIVE_REUSE") && !mesh_fragment_clock_admit()))return;
    for(auto i=g_meshFragmentStores.begin();i!=g_meshFragmentStores.end();) {
        if(g_drawFrameSerial-i->second.used>60u){mesh_fragment_store_release(i->second);i=g_meshFragmentStores.erase(i);}
        else ++i;
    }
    MeshKey slot;slot.append((const char*)&identity,sizeof identity);
    slot.append((const char*)&variant,sizeof variant);
    const bool layout[]={g_meshSunDepth,g_meshNrmOut,g_meshBakePass,g_meshBakeCutout,g_meshShadowPass};
    slot.append((const char*)layout,sizeof layout);
    auto f=g_meshFragmentStores.find(slot);
    if(f==g_meshFragmentStores.end()) {
        if(g_meshFragmentStores.size()>=mesh_fragment_store_limit()) {
            auto oldest=g_meshFragmentStores.begin();
            for(auto i=g_meshFragmentStores.begin();i!=g_meshFragmentStores.end();++i)
                if((unsigned)(g_drawFrameSerial-i->second.used)>(unsigned)(g_drawFrameSerial-oldest->second.used))oldest=i;
            mesh_fragment_store_release(oldest->second);g_meshFragmentStores.erase(oldest);
        }
        f=g_meshFragmentStores.emplace(std::move(slot),MeshFragmentStore()).first;
    }
    if(const int rest=mesh_fragment_rest_mode()) {
        const bool still=mesh_fragment_clock_admit();   /* asked every scope: it tracks frame to frame */
        const bool due=(int)(f->second.restUntil-g_drawFrameSerial)>0;
        const bool resting=due && !still;
        if(due && still)g_meshFragmentRestStill++;
        if(resting)g_meshFragmentRestSkips++;
        if(resting && rest==1)return;          /* drawn as if no store existed */
        /* (a store with nothing kept yet has nothing to reuse: its first scope is not a try) */
        g_meshFragmentRestNote=!f->second.inputs.empty();g_meshFragmentRestWould=resting;g_meshFragmentRestSlot=f->first;
        g_meshFragmentRestHits0=g_meshFragmentInputHits;g_meshFragmentRestStreams0=g_meshFragmentStreamHits;
        g_meshFragmentRestInputs0=g_meshFragmentStoreInputs;
    }
    g_meshFragmentStore=&f->second;g_meshFragmentStore->used=g_drawFrameSerial;
    g_meshFragmentChanged=g_meshFragmentStore->inputs.empty();
}
/* Prefix hits need no copies. Materialize only when a later input changes. */
static void mesh_fragment_materialize() {
    if(g_meshFragmentStore && !g_meshFragmentChanged) {
        g_meshFragmentVertices.assign(g_meshFragmentStore->vertices.begin(),
             g_meshFragmentStore->vertices.begin()+g_meshFragmentCount);
        g_meshFragmentChanged=true;
    }
}
static void mesh_fragment_store_disable() {
    mesh_fragment_materialize();g_meshFragmentStore=NULL;g_meshFragmentInputs.clear();
}
static bool mesh_fragment_input_reuse(const MeshKey& key,const Pack& src,int house,int pass,
                                      bool depth,bool normals) {
    if(!g_meshFragmentStore)return false;
    const size_t index=g_meshFragmentInputs.size();
    const size_t limit=mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_INPUT_FULL") ? 2048u : 16u;
    if(index>=limit){mesh_fragment_store_disable();return false;}
    if(key.empty() || index>=g_meshFragmentStore->inputs.size())return false;
    const MeshFragmentInput& old=g_meshFragmentStore->inputs[index];
    if(!(old.key==key))return false;
    if(old.count) {
        const MeshFragmentCommand& previous=g_meshFragmentStore->commands[old.command];
        if(previous.src!=&src || previous.house!=house || previous.pass!=pass
            || previous.depth!=depth || previous.normals!=normals)return false;
        MeshFragmentCommand cmd=previous;
        cmd.cull=mesh_fragment_polygon(GL_CULL_FACE);cmd.offset=mesh_fragment_polygon(GL_POLYGON_OFFSET_FILL);
        for(auto& run:cmd.entry.runs)run.first=run.first-old.first+g_meshFragmentCount;
        if(g_meshFragmentChanged)g_meshFragmentVertices.insert(g_meshFragmentVertices.end(),
            g_meshFragmentStore->vertices.begin()+old.first,
            g_meshFragmentStore->vertices.begin()+old.first+old.count);
        MeshFragmentInput input;input.key=key;input.first=g_meshFragmentCount;input.count=old.count;
        input.command=g_meshFragmentCommands.size();g_meshFragmentInputs.push_back(std::move(input));
        g_meshFragmentCommands.push_back(std::move(cmd));g_meshFragmentCount+=old.count;
    } else {
        MeshFragmentInput input;input.key=key;input.first=g_meshFragmentCount;
        g_meshFragmentInputs.push_back(std::move(input));
    }
    g_meshFragmentInputHits++;g_meshFragmentStoreInputs++;return true;
}
static void mesh_fragment_input_record(const MeshKey& key,size_t count,size_t command) {
    if(!g_meshFragmentStore){g_meshFragmentCount+=count;return;}
    const size_t limit=mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_INPUT_FULL") ? 2048u : 16u;
    if(g_meshFragmentInputs.size()>=limit){mesh_fragment_store_disable();g_meshFragmentCount+=count;return;}
    mesh_fragment_materialize();g_meshFragmentStoreInputs++;
    MeshFragmentInput input;input.key=key;input.first=g_meshFragmentCount;
    input.count=count;input.command=command;g_meshFragmentInputs.push_back(std::move(input));
    g_meshFragmentCount+=count;
}

/* Optional bounded Windows diagnostic. Counts are reset at each recorded frame,
   and only work inside that frame is recorded. Startup/script work is excluded. */
static Uint64 g_destructTrace[22] = {};
static int destruction_trace_pass_kind();
static void mesh_fragment_flush(void);
/* The end of a scope that used its store: did it reuse enough to keep using it (the
   resting rule above)? */
static void mesh_fragment_rest_note() {
    if(!g_meshFragmentRestNote)return;
    g_meshFragmentRestNote=false;
    auto f=g_meshFragmentStores.find(g_meshFragmentRestSlot);
    if(f==g_meshFragmentStores.end())return;
    const unsigned long hits=g_meshFragmentInputHits-g_meshFragmentRestHits0;
    const unsigned long long seen=g_meshFragmentStoreInputs-g_meshFragmentRestInputs0;
    if(!seen)return;                       /* drew nothing: no evidence either way */
    const bool reused=g_meshFragmentStreamHits!=g_meshFragmentRestStreams0 || (hits && (unsigned long long)hits*4u>=seen);
    if(g_meshFragmentRestWould) { if(reused)g_meshFragmentRestMissed++; return; }
    MeshFragmentStore& s=f->second;
    g_meshFragmentRestTries++;
    if(reused) { s.restWait=0;s.restMisses=0;s.restUntil=g_drawFrameSerial;g_meshFragmentRestWarm++; }
    else if(++s.restMisses>=2u) {
        s.restWait=s.restWait ? std::min(s.restWait*2u,16u) : 2u;s.restUntil=g_drawFrameSerial+s.restWait+1u;
    }
}
struct MeshFragmentScope {
    bool active;
    MeshFragmentScope(const void* identity=NULL,unsigned variant=0)
        : MeshFragmentScope(true,identity,variant) {}
    MeshFragmentScope(bool wanted,const void* identity,unsigned variant) : active(wanted && g_fx.enabled && !g_meshBatchEditor
        && mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_BATCH")
        && mesh_perf_switch("OPENCNC3D_MESH_GPU") && grass_buf_load()
        && !g_meshFragmentCollect) {
        if (active) {
                      g_meshFragmentStateDeferred=mesh_fragment_polygon_defer_enabled();
                      if(g_meshFragmentStateDeferred) {
                          g_meshFragmentCull=glIsEnabled(GL_CULL_FACE)==GL_TRUE;
                          g_meshFragmentOffset=glIsEnabled(GL_POLYGON_OFFSET_FILL)==GL_TRUE;
                      }
                      g_meshFragmentVertices.clear(); g_meshFragmentCommands.clear();
                      g_meshFragmentCollect = true; mesh_fragment_store_begin(identity,variant);
                      g_meshFragmentPacked.clear();g_meshFragmentPackedStride=0;
                      g_meshFragmentPackedCollect=!g_meshFragmentStore&&!g_meshNrmOut
                          &&mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_PACK")
                          &&mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_PACK_OK"); }
    }
    ~MeshFragmentScope() {
        if (active) { g_meshFragmentCollect = false; mesh_fragment_flush(); mesh_fragment_rest_note();
            g_meshFragmentStore=NULL;g_meshFragmentInputs.clear();g_meshFragmentPackedCollect=false;
            g_meshFragmentStateDeferred=false; }
    }
};
static bool g_meshClassSkip = false;
static bool g_meshBatchRestPose = false;
static unsigned long g_meshBatchHits = 0, g_meshBatchUploads = 0,
                     g_meshBatchCulled = 0, g_meshBatchDraws = 0,
                     g_meshClassSkipped = 0;
/* Vertices draw_mesh still sent one call at a time (glVertex3f), outside the batch. */
static unsigned long g_meshImmVerts = 0;
/* Bytes handed to the card by every vertex upload counted in uploads above. */
static unsigned long long g_meshBatchUploadBytes = 0;
/* READ ONCE PER NAME. These are launch-time A/B switches, and draw_mesh and the cull ask
   for them on every model drawn: about 1,800 lookups a frame under Enhanced. On Windows
   each getenv takes the C runtime's environment lock and compares every variable in the
   block, case-insensitively. Nothing in the game changes these variables after launch. */
static bool mesh_perf_switch_named(const char* name) {
    struct Known { const char* name; bool on; };
    static Known known[256];
    static int nknown = 0;
    for (int i = 0; i < nknown; i++)
        if (known[i].name == name || !strcmp(known[i].name, name)) return known[i].on;
    const char* v = getenv(name);
    const bool on = !v || strcmp(v, "0");
    if (nknown < 256) { known[nknown].name = name; known[nknown].on = on; nknown++; }
    return on;
}
/* AND FOUND BY ADDRESS, NOT BY NAME. The table above was searched in order with a
   string compare against every entry before the one asked for, and dozens of these
   names share their first fifteen characters, so a lookup near the end of the table
   cost dozens of compares; draw_mesh asks for four to six of them per model, in every
   pass. Every caller passes a string literal, whose address never changes, so the
   address is the key: one hash, usually one probe. The name table is still what reads
   each variable, once, so both paths give the same answer for every name.
   OPENCNC3D_MESH_SWITCH_ADDRESS=0 puts the search by name back. */
static bool mesh_perf_switch(const char* name) {
    static const bool byAddress = mesh_perf_switch_named("OPENCNC3D_MESH_SWITCH_ADDRESS");
    if (!byAddress) return mesh_perf_switch_named(name);
    struct Slot { const char* name; bool on; };
    enum { SLOT_BITS = 10, SLOTS = 1 << SLOT_BITS };
    static Slot slots[SLOTS];
    static int used = 0;
    size_t h = (size_t)(((uint64_t)(uintptr_t)name * 0x9E3779B97F4A7C15ull) >> (64 - SLOT_BITS));
    for (int probe = 0; probe < SLOTS; probe++, h = (h + 1) & (SLOTS - 1)) {
        if (slots[h].name == name) return slots[h].on;
        if (!slots[h].name) {
            const bool on = mesh_perf_switch_named(name);
            if (used < SLOTS / 2) { slots[h].on = on; slots[h].name = name; used++; }
            return on;
        }
    }
    return mesh_perf_switch_named(name);
}
/* Script-only paired controls hold the same process, assets and engine state.
   Interactive gameplay keeps the launch-time switches and never changes this. */
static int g_antPerfOverride=-1;
static bool ant_perf_switch(const char* name) {
    return g_antPerfOverride<0 ? mesh_perf_switch(name) : g_antPerfOverride!=0;
}
static void mesh_geometry_clear();
static void mesh_prepared_clear();
static void mesh_scratch_clear();
static void mesh_glow_clear();
static void mesh_direct_glow_clear();
static void mesh_batch_clear(void) {
    mesh_geometry_clear(); mesh_scratch_clear(); mesh_prepared_clear(); mesh_glow_clear(); mesh_direct_glow_clear();
    if (gr_glDeleteBuffers) {
        for (auto& p : g_meshBatchCache)
            if (p.second.vbo) gr_glDeleteBuffers(1, &p.second.vbo);
        for(auto& s:g_meshFrameStores) if(s.vbo) gr_glDeleteBuffers(1,&s.vbo);
        if (g_meshBatchStream) gr_glDeleteBuffers(1, &g_meshBatchStream);
    }
    g_meshBatchCache.clear(); g_meshBatchLRU.clear(); g_meshBatchBytes = 0; g_meshBatchStream = 0;
    g_meshVerified.clear();
    g_meshFrameCache.clear(); g_meshFrameStores.clear(); g_meshFrameBytes=0;
    g_meshFragmentVertices.clear(); g_meshFragmentVertices.shrink_to_fit();
    g_meshFragmentPacked.clear();g_meshFragmentPacked.shrink_to_fit();
    g_meshFragmentCommands.clear(); g_meshFragmentCommands.shrink_to_fit();
    g_meshFragmentCollect = false;g_meshFragmentStateDeferred=false;g_meshFragmentPackedCollect=false;g_meshFragmentPackedStride=0;
    g_meshFragmentStore=NULL;
    for(auto& s:g_meshFragmentStores)mesh_fragment_store_release(s.second);
    g_meshFragmentStores.clear();g_meshFragmentInputs.clear();g_meshFragmentInputs.shrink_to_fit();
    g_meshFragmentCount=0;g_meshFragmentChanged=true;
    g_meshFragmentClockFrame=~0u;g_meshFragmentClockEngine=-1;g_meshFragmentClockAlpha=-1.0f;
    g_meshFragmentClockAgain=false;
    g_meshFrameSerial=~0u; g_meshFrameNext=0;
    g_meshPlacedPrev.clear(); g_meshPlacedNow.clear(); g_meshPlacedSerial=~0u;
}
static const MeshBatchEntry* mesh_batch_kept_find(const MeshKey& key) {
    auto it = g_meshBatchCache.find(key);
    if (it == g_meshBatchCache.end()) { g_meshBatchMisses++; return NULL; }
    it->second.used = g_drawFrameSerial;
    g_meshBatchLRU.splice(g_meshBatchLRU.end(), g_meshBatchLRU, it->second.lru);
    g_meshBatchHits++;
    return &it->second;
}
static void mesh_batch_evict_one() {
    if (g_meshBatchLRU.empty()) return;
    auto it = g_meshBatchCache.find(g_meshBatchLRU.front());
    if (it != g_meshBatchCache.end()) {
        g_meshBatchBytes -= it->second.bytes;
        if (it->second.vbo) {
            g_meshVerified.erase(it->second.vbo);
            gr_glDeleteBuffers(1, &it->second.vbo);
        }
        g_meshBatchCache.erase(it); g_meshBatchEvictions++;
    }
    g_meshBatchLRU.pop_front();
}
static size_t mesh_batch_capacity() {
    return mesh_perf_switch("OPENCNC3D_MESH_CACHE_FULL") ? 128u*1024u*1024u : 1024u*1024u;
}
/* A scene larger than the persistent cache must not evict its own earlier draws
   on every frame. Protect the current and previous two frames' hot set, then
   submit refused entries through bounded transient stores. Cold entries still
   retire in LRU order as the view changes. Admission precedes GPU allocation. */
static bool mesh_batch_admit(size_t bytes) {
    const size_t cap=mesh_batch_capacity();
    if(bytes>cap) {g_meshBatchAdmissionRefused++;return false;}
    const bool protect=mesh_perf_switch("OPENCNC3D_MESH_HOT_ADMISSION");
    while (!g_meshBatchLRU.empty() &&
           (bytes>cap-g_meshBatchBytes || g_meshBatchCache.size() >= MESH_BATCH_MAX_ENTRIES)) {
        auto oldest=g_meshBatchCache.find(g_meshBatchLRU.front());
        if(protect && oldest!=g_meshBatchCache.end()
            && g_drawFrameSerial-oldest->second.used<=2u) {
            g_meshBatchAdmissionRefused++;return false;
        }
        mesh_batch_evict_one();
    }
    return bytes<=cap-g_meshBatchBytes && g_meshBatchCache.size()<MESH_BATCH_MAX_ENTRIES;
}
static void mesh_batch_keep(const MeshKey& key, MeshBatchEntry entry) {
    /* A repeated offer must not grow the LRU or byte charge behind an existing
       key. Its resident store remains owned by that entry. */
    auto existing=g_meshBatchCache.find(key);
    if(existing!=g_meshBatchCache.end()) {
        if(entry.vbo && entry.vbo!=existing->second.vbo) {
            g_meshVerified.erase(entry.vbo);gr_glDeleteBuffers(1,&entry.vbo);
        }
        return;
    }
    if(!mesh_batch_admit(entry.bytes)) {
        if(entry.vbo){g_meshVerified.erase(entry.vbo);gr_glDeleteBuffers(1,&entry.vbo);}
        return;
    }
    const GLuint offered=entry.vbo;
    bool staged=false;
    try {
        g_meshBatchLRU.push_back(key);staged=true;
        entry.lru=--g_meshBatchLRU.end();entry.used=g_drawFrameSerial;
        if(!mesh_perf_switch("OPENCNC3D_MESH_CACHE_ALLOC_OK"))throw std::bad_alloc();
        auto inserted=g_meshBatchCache.emplace(key,std::move(entry));
        g_meshBatchBytes+=inserted.first->second.bytes;
    } catch(const std::bad_alloc&) {
        if(staged)g_meshBatchLRU.pop_back();
        if(offered){g_meshVerified.erase(offered);gr_glDeleteBuffers(1,&offered);}
        g_meshBatchAdmissionRefused++;
    }
}
/* Geometry precedes pass-specific colour/depth packing. Keep the original CPU
   results, including its operation order, once per required pose in this frame.
   Slots retain capacity between frames, with a separate bounded CPU budget. */
struct MeshGeometryPose : MeshPoseStorage {
    bool settled = false;
    GLuint gpu = 0;
    size_t gpuCount = 0;
    unsigned lastUsed = 0;
    float lo[3],hi[3];
    bool boundsValid = false;
};
static std::deque<MeshGeometryPose> g_meshGeometryPool;
static std::unordered_map<MeshKey, size_t, MeshKeyHash> g_meshGeometryKeys;
static unsigned g_meshGeometrySerial=~0u;
static size_t g_meshGeometryNext=0, g_meshGeometryBytes=0;
static unsigned long g_meshGeometryHits=0, g_meshGeometryBuilds=0, g_meshGeometryUploads=0;
static unsigned long long g_meshGeometryRequests=0,g_meshGeometryMisses=0;
static unsigned long long g_meshGeometryRefused=0,g_meshGeometryAllocationFailures=0;
static unsigned long long g_meshGeometryMaterialized=0,g_meshGeometryReleased=0;
static unsigned long long g_meshSettledRefused=0;
static std::vector<float> g_meshGeometryUpload;
static unsigned long long g_meshWindDirectDraws=0,g_meshWindDirectVertices=0,g_meshWindDirectFallbacks=0;
static unsigned long long g_meshWindDirectVerified=0,g_meshWindDirectMismatches=0;
static unsigned long long g_meshWindDepthDraws=0,g_meshWindDepthVertices=0,g_meshWindDepthFallbacks=0;
static unsigned long long g_meshWindDepthVerified=0,g_meshWindDepthMismatches=0;
static unsigned long long g_meshBakeShareDraws=0,g_meshBakeShareVertices=0,g_meshBakeShareStores=0;
static unsigned long long g_meshBakeShareVerified=0,g_meshBakeShareMismatches=0;
/* Reads a buffer back for the verifiers, which compare what the card holds with what
   the full loop makes. Never on a normal frame: a readback waits for the card. */
typedef void (APIENTRY *MeshGetBufferSubData)(GLenum, FxGLsizeiptr, FxGLsizeiptr, void*);
static bool mesh_buffer_read(GLuint vbo, size_t bytes, void* out) {
    static MeshGetBufferSubData get = NULL;
    static bool asked = false;
    if (!asked) {
        asked = true;
        get = (MeshGetBufferSubData)SDL_GL_GetProcAddress("glGetBufferSubData");
        if (!get) get = (MeshGetBufferSubData)SDL_GL_GetProcAddress("glGetBufferSubDataARB");
        if (!get) fprintf(stderr, "MESHVERIFY|unavailable|no glGetBufferSubData\n");
    }
    if (!get || !vbo) return false;
    if (!bytes) return true;
    const GLuint bound = gls_array_binding();
    gr_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    get(GL_ARRAY_BUFFER, 0, (FxGLsizeiptr)bytes, out);
    gr_glBindBuffer(GL_ARRAY_BUFFER, bound);
    return true;
}
static bool mesh_wind_direct_verify_enabled() {
    static const bool enabled=[](){
        const char* value=getenv("OPENCNC3D_MESH_WIND_DIRECT_VERIFY");
        return value && !strcmp(value,"1");
    }();
    return enabled;
}
/* The measured dense live scene needs about 117 MiB of evaluated corners. Keep
   the old eager 64 MiB policy until it actually refuses a pose for lack of space;
   then use lazy arrays with this bounded ceiling until the map/cache clears.
   The ceiling is not a reservation; 32-bit processes retain the legacy limit.
   A smaller diagnostic budget permits the old pressure case in the same binary. */
static size_t mesh_geometry_budget() {
    static const size_t cap=[](){
        const size_t maximum=(sizeof(void*)>=8?256u:64u)*1024u*1024u;
        const char* value=getenv("OPENCNC3D_MESH_GEOMETRY_MB");
        if(!value || !*value)return maximum;
        char* end=NULL;const unsigned long mb=strtoul(value,&end,10);
        return end && !*end && mb>=1 && mb<=maximum/(1024u*1024u)
            ? (size_t)mb*1024u*1024u : maximum;
    }();
    return cap;
}
static MeshPosePolicy& mesh_geometry_policy() {
    static MeshPosePolicy policy([](){
        const char* value=getenv("OPENCNC3D_MESH_POSE_LAZY");
        if(value && !strcmp(value,"0"))return MESH_POSE_EAGER;
        if(value && !strcmp(value,"1"))return MESH_POSE_LAZY;
        return MESH_POSE_AUTO;
    }(),mesh_geometry_budget());
    return policy;
}
static size_t mesh_geometry_bytes(const MeshGeometryPose& p) {
    return mesh_pose_storage_bytes(p);
}
static void mesh_geometry_release(MeshGeometryPose& p,size_t& total) {
    if(mesh_geometry_bytes(p))g_meshGeometryReleased++;
    mesh_pose_storage_release(p,total);p.gpu=0;p.gpuCount=0;p.boundsValid=false;
}
static void mesh_geometry_frame_begin() {
    if (g_meshGeometrySerial == g_drawFrameSerial) return;
    g_meshGeometryKeys.clear(); g_meshGeometryNext=0; g_meshGeometrySerial=g_drawFrameSerial;
    for (auto& p : g_meshGeometryPool)
        if (g_drawFrameSerial-p.lastUsed>60u) mesh_geometry_release(p,g_meshGeometryBytes);
}
struct MeshSettledPose { MeshGeometryPose pose; std::list<MeshKey>::iterator lru; };
static std::unordered_map<MeshKey,MeshSettledPose,MeshKeyHash> g_meshSettled;
static std::list<MeshKey> g_meshSettledLRU;
static size_t g_meshSettledBytes=0;
static unsigned long g_meshSettledHits=0;
static bool mesh_geometry_materialize(MeshGeometryPose& p) {
    if(!p.fresh)return true;
    MeshPosePolicy& policy=mesh_geometry_policy();
    size_t cap=p.settled?32u*1024u*1024u:policy.cap();
    size_t& total=p.settled?g_meshSettledBytes:g_meshGeometryBytes;
    size_t exact=0;
    if(!mesh_pose_required_bytes(p.requestedCount,exact) || exact>cap) {
        g_meshGeometryRefused++;if(p.settled)g_meshSettledRefused++;return false;
    }
    const size_t old=mesh_geometry_bytes(p);
    if(p.settled) {
        /* Eviction never invalidates the descriptor currently being prepared. */
        while(total-old>cap-exact && !g_meshSettledLRU.empty()) {
            auto victim=g_meshSettled.find(g_meshSettledLRU.front());
            if(victim==g_meshSettled.end()){g_meshSettledLRU.pop_front();continue;}
            if(&victim->second.pose==&p) {
                if(g_meshSettledLRU.size()==1)break;
                g_meshSettledLRU.splice(g_meshSettledLRU.end(),g_meshSettledLRU,victim->second.lru);
                continue;
            }
            total-=mesh_geometry_bytes(victim->second.pose);
            g_meshSettled.erase(victim);g_meshSettledLRU.pop_front();
        }
    } else if(total-old>cap-exact) {
        /* Only tail slots have not been handed to a current-frame draw. */
        for(size_t i=g_meshGeometryPool.size();i>g_meshGeometryNext && total-old>cap-exact;)
            mesh_geometry_release(g_meshGeometryPool[--i],total);
    }
    const bool fresh=p.fresh;
    const bool spaceRefused=total>=old && total-old>cap-exact;
    MeshPoseStorageResult result=mesh_pose_storage_materialize(p,cap,total,
        mesh_perf_switch("OPENCNC3D_MESH_GEOMETRY_ALLOC_OK"));
    /* Only the pool's real space refusal after reclaiming unused tail slots
       switches auto mode. Check the retained-capacity accounting as well, so
       malformed counts or an allocation failure never look like map pressure. */
    if(spaceRefused && policy.admit_pressure(result,!p.settled,exact,total-old)) {
        cap=policy.cap();
        result=mesh_pose_storage_materialize(p,cap,total,
            mesh_perf_switch("OPENCNC3D_MESH_GEOMETRY_ALLOC_OK"));
    }
    if(result!=MESH_POSE_READY) {
        g_meshGeometryRefused++;if(p.settled)g_meshSettledRefused++;
        if(result==MESH_POSE_ALLOCATION)g_meshGeometryAllocationFailures++;
        return false;
    }
    if(fresh)g_meshGeometryMaterialized++;
    return true;
}
static MeshGeometryPose* mesh_geometry_settled(const MeshKey& key,size_t n) {
    auto f=g_meshSettled.find(key);
    if(f!=g_meshSettled.end()) {
        auto& p=f->second.pose;
        if(p.lastUsed!=g_drawFrameSerial){p.gpu=0;p.gpuCount=0;p.lastUsed=g_drawFrameSerial;}
        g_meshSettledLRU.splice(g_meshSettledLRU.end(),g_meshSettledLRU,f->second.lru);
        g_meshSettledHits++;
        return !mesh_geometry_policy().lazy() && !mesh_geometry_materialize(p)?NULL:&p;
    }
    while(g_meshSettled.size()>=2048 && !g_meshSettledLRU.empty()) {
        auto old=g_meshSettled.find(g_meshSettledLRU.front());
        if(old!=g_meshSettled.end()){g_meshSettledBytes-=mesh_geometry_bytes(old->second.pose);g_meshSettled.erase(old);}
        g_meshSettledLRU.pop_front();
    }
    bool staged=false;
    try {
        MeshSettledPose entry;entry.pose.requestedCount=n;entry.pose.settled=true;
        entry.pose.lastUsed=g_drawFrameSerial;
        g_meshSettledLRU.push_back(key);staged=true;entry.lru=--g_meshSettledLRU.end();
        auto inserted=g_meshSettled.emplace(key,std::move(entry));
        if(!inserted.second)g_meshSettledLRU.pop_back();
        MeshGeometryPose& p=inserted.first->second.pose;
        return !mesh_geometry_policy().lazy() && !mesh_geometry_materialize(p)?NULL:&p;
    } catch(const std::bad_alloc&) {
        if(staged)g_meshSettledLRU.pop_back();
        g_meshGeometryAllocationFailures++;g_meshGeometryRefused++;g_meshSettledRefused++;return NULL;
    }
}
static MeshGeometryPose* mesh_geometry_find(const MeshKey& key, size_t n) {
    mesh_geometry_frame_begin();g_meshGeometryRequests++;
    auto found=g_meshGeometryKeys.find(key);
    if(found!=g_meshGeometryKeys.end()) {
        g_meshGeometryHits++;MeshGeometryPose& p=g_meshGeometryPool[found->second];
        return !mesh_geometry_policy().lazy() && !mesh_geometry_materialize(p)?NULL:&p;
    }
    g_meshGeometryMisses++;
    if(g_meshGeometryNext>=2048){g_meshGeometryRefused++;return NULL;}
    try {
        if(g_meshGeometryNext==g_meshGeometryPool.size())g_meshGeometryPool.emplace_back();
        const size_t slot=g_meshGeometryNext;
        g_meshGeometryKeys.emplace(key,slot);g_meshGeometryNext++;
        MeshGeometryPose& p=g_meshGeometryPool[slot];
        p.requestedCount=n;p.fresh=true;p.settled=false;
        p.gpu=0;p.gpuCount=0;p.boundsValid=false;p.lastUsed=g_drawFrameSerial;
        return !mesh_geometry_policy().lazy() && !mesh_geometry_materialize(p)?NULL:&p;
    } catch(const std::bad_alloc&) {
        g_meshGeometryAllocationFailures++;g_meshGeometryRefused++;return NULL;
    }
}
static GLuint mesh_geometry_upload(MeshGeometryPose& pose,const float* positions,size_t count,GLuint vbo=0) {
    if(pose.gpu)return pose.gpuCount==count?pose.gpu:0;
    if(count>64u*1024u*1024u/(3*sizeof(float)))return 0;
    const size_t bytes=count*3*sizeof(float);
    if(!vbo)vbo=mesh_batch_frame_store(bytes);
    if (!vbo) return 0;
    const GLuint was=gls_array_binding(); gr_glBindBuffer(GL_ARRAY_BUFFER,vbo);
    gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)bytes,positions,GL_STREAM_DRAW);
    const bool ok=mesh_store_holds(vbo,bytes);
    if (!ok) mesh_frame_store_release(g_meshFrameStores[g_meshFrameNext-1]);
    gr_glBindBuffer(GL_ARRAY_BUFFER,was);
    if (ok) { pose.gpu=vbo; pose.gpuCount=count; g_meshGeometryUploads++; g_meshBatchUploadBytes+=bytes; }
    return ok?vbo:0;
}
static GLuint mesh_geometry_gpu(MeshGeometryPose& p, const std::vector<MeshBatchVertex>& vertices) {
    if (p.gpu) return p.gpuCount==vertices.size()?p.gpu:0;
    if (vertices.size()>64u*1024u*1024u/(3*sizeof(float))) return 0;
    try {g_meshGeometryUpload.resize(vertices.size()*3);}
    catch(const std::bad_alloc&) {g_meshGeometryAllocationFailures++;return 0;}
    for (size_t i=0;i<vertices.size();i++) {
        memcpy(&g_meshGeometryUpload[i*3],vertices[i].p,3*sizeof(float));
        for(int k=0;k<3;k++) {
            if(!i)p.lo[k]=p.hi[k]=vertices[i].p[k];
            else{p.lo[k]=std::min(p.lo[k],vertices[i].p[k]);p.hi[k]=std::max(p.hi[k],vertices[i].p[k]);}
        }
    }
    return mesh_geometry_upload(p,g_meshGeometryUpload.data(),vertices.size());
}
static void mesh_geometry_clear() {
    g_meshSettled.clear();g_meshSettledLRU.clear();g_meshSettledBytes=0;
    g_meshGeometryKeys.clear(); g_meshGeometryPool.clear(); g_meshGeometryBytes=0;
    std::vector<float>().swap(g_meshGeometryUpload); g_meshGeometrySerial=~0u; g_meshGeometryNext=0;
    mesh_geometry_policy().reset();
}
/* A remastered object's pieces share client-array ownership. Legacy helpers can
   still save/restore or change arrays inside the scope; each mesh sets its own
   pointers, and the outer scope restores the caller's state once. */
static unsigned g_meshArrayDepth=0;
static GLuint g_meshArrayWas=0;
struct MeshArrayScope {
    bool active;
    explicit MeshArrayScope(bool wanted = true)
        : active(wanted && g_fx.enabled && mesh_perf_switch("OPENCNC3D_MESH_ARRAY_SCOPE")
                 && mesh_perf_switch("OPENCNC3D_MESH_GPU") && grass_buf_load()) {
        if (active && g_meshArrayDepth++==0) {
            g_meshArrayWas=gls_array_binding(); glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        }
    }
    ~MeshArrayScope() {
        if (active && --g_meshArrayDepth==0) {
            glPopClientAttrib(); gr_glBindBuffer(GL_ARRAY_BUFFER,g_meshArrayWas);
        }
    }
};
/* Vehicle wreck glow has no depth/normal sibling that consumes its pose. Keeping
   CPU corners and uploading separate position/attribute buffers only duplicates
   work. The complete original vertex path remains the fallback and still draws
   the same frame-cache entry. A nested scope cannot re-enable a disabled parent. */
static unsigned long long g_meshSinglePassSkipped = 0;
static bool mesh_vehicle_glow_single_pass() {
    static const bool on = []() {
        const char* value = getenv("OPENCNC3D_VEHICLE_GLOW_SINGLE_PASS");
        if (value) return strcmp(value, "0") != 0;
#ifdef _WIN32
        return true; // Native RTX explosion comparisons improve; Mac keeps its path.
#else
        return false;
#endif
    }();
    return on;
}
struct MeshGeometrySharingScope {
    bool previous;
    explicit MeshGeometrySharingScope(bool enabled) : previous(g_meshGeometrySharing) {
        g_meshGeometrySharing = previous && enabled;
    }
    ~MeshGeometrySharingScope() { g_meshGeometrySharing = previous; }
};

/* Reject only when every corner lies outside one homogeneous clip plane.
   These are the CURRENT matrices: camera, mirrored water camera or sun camera.
   No assumption about ground height, anchor visibility or terrain cull bounds. */
static bool mesh_box_outside(const float lo[3], const float hi[3], const float* clip = NULL) {
    if (!mesh_perf_switch("OPENCNC3D_MESH_CULL")) return false;
    float c[16];
    memcpy(c, clip ? clip : gls_clip(), sizeof c);   /* gls_clip asks only after a matrix changed */
    for (int axis = 0; axis < 3; axis++) for (int sign = -1; sign <= 1; sign += 2) {
        float a[4];
        for (int k = 0; k < 4; k++) a[k] = c[k*4+3] + sign*c[k*4+axis];
        /* Positive support point of the world-space box, with a small conservative
           margin for matrix roundoff. Non-finite bounds never reject geometry. */
        float d = a[3], margin = 0.001f;
        for (int k = 0; k < 3; k++) {
            d += a[k] * (a[k] >= 0 ? hi[k] : lo[k]);
            margin += fabsf(a[k]) * 0.001f;
        }
        if (std::isfinite(d) && d < -margin) return true;
    }
    return false;
}
static unsigned long long g_meshEarlyCullTests=0,g_meshEarlyCullRejected=0;
static unsigned long long g_meshEarlyCullVerified=0,g_meshEarlyCullBad=0;
static bool g_meshEarlyCullWitness=false;
static const MeshBatchEntry* g_meshEarlyCullReplay=NULL;
struct MeshEarlyCullWitness {
    bool previous=g_meshEarlyCullWitness;
    const MeshBatchEntry* previousReplay=g_meshEarlyCullReplay;
    MeshEarlyCullWitness(){g_meshEarlyCullWitness=false;g_meshEarlyCullReplay=NULL;}
    ~MeshEarlyCullWitness(){g_meshEarlyCullWitness=previous;g_meshEarlyCullReplay=previousReplay;}
};
static int mesh_range_cull_mode() {
    static const int mode=[](){const char* p=getenv("OPENCNC3D_MESH_RANGE_CULL");
        if(!p)return 2;if(!strcmp(p,"0"))return 0;if(!strcmp(p,"verify"))return 3;
        return !strcmp(p,"2")?2:1;}();return mode;
}
static unsigned long long g_meshRangeCullOffers=0,g_meshRangeCullBounds=0,g_meshRangeCullTransient=0,g_meshRangeCullRejected=0;
static void mesh_range_cull_dump() {
    printf("MESHRANGECULL|offers=%llu|bounds=%llu|transient=%llu|rejected=%llu\n",g_meshRangeCullOffers,
        g_meshRangeCullBounds,g_meshRangeCullTransient,g_meshRangeCullRejected);
}
static bool mesh_early_cull_verify() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_MESH_EARLY_CULL_VERIFY");return p&&strcmp(p,"0");}();
    return on;
}
static bool mesh_batch_outside(const MeshBatchEntry& e, const float* clip = NULL) {
    if (mesh_box_outside(e.lo, e.hi, clip)) return true;
    /* while the sun's receiver window is armed (fx_post.h): a piece none of whose texels
       is read is dropped the same way, its draw only, every state call kept */
    if (!g_fxRecvArmed || !g_fxRecvArmed->on) return false;
    if (!fx_recv_box_outside(e.lo, e.hi, clip ? clip : gls_clip())) return false;
    g_fxRecvPieceCulled++;
    return true;
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
static void mesh_batch_draw_ranges(const MeshBatchEntry& e,const MeshBatchRun* runs,size_t runCount, const Pack& src, int house,
                            int pass, bool depth, bool normals, const MeshBatchVertex* client = NULL) {
    if (!runCount) return;
    const bool outside = mesh_batch_outside(e);
    if(g_actorDepthWitness){g_actorDepthVerified++;if(!outside)g_actorDepthBad++;}
    if(g_meshEarlyCullWitness) {
        g_meshEarlyCullVerified++;
        bool same=outside;
        if(g_meshEarlyCullReplay) {
            const MeshBatchEntry& expected=*g_meshEarlyCullReplay;
            same=same && e.uv==expected.uv && runCount==expected.runs.size();
            if(!depth && memcmp(e.last.c,expected.last.c,sizeof e.last.c))same=false;
            if(normals && memcmp(e.last.n,expected.last.n,sizeof e.last.n))same=false;
            if(e.uv && memcmp(e.last.uv,expected.last.uv,sizeof e.last.uv))same=false;
            for(size_t i=0;same&&i<runCount;i++) {
                const auto& a=runs[i];const auto& b=expected.runs[i];
                same=a.tex==b.tex&&a.wrap==b.wrap&&a.x2==b.x2&&a.first==b.first&&a.count==b.count;
            }
        }
        if(!same)g_meshEarlyCullBad++;
    }
    if (outside) g_meshBatchCulled++;
    /* The env mode is only needed to put back after a doubled (x2) run, so it is asked
       for just before the first one rather than for every model. */
    GLint env = -1;
    GLuint bound = 0;
    if (!outside) {
    bound = gls_array_binding();
    if (!g_meshArrayDepth) glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    gr_glBindBuffer(GL_ARRAY_BUFFER, e.positionVbo ? e.positionVbo : e.vbo);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, e.positionVbo ? 3*sizeof(float) : sizeof(MeshBatchVertex),
                    e.positionVbo ? NULL : client ? (const void*)client->p : (const void*)offsetof(MeshBatchVertex,p));
    if (!depth) {
        if (e.positionVbo) gr_glBindBuffer(GL_ARRAY_BUFFER,e.vbo);
        const bool compact=e.compact && !client;
        const GLsizei stride=compact ? sizeof(MeshBatchAttributes) : sizeof(MeshBatchVertex);
        glEnableClientState(GL_COLOR_ARRAY);
        if (e.colourVbo && !client) {
            gr_glBindBuffer(GL_ARRAY_BUFFER, e.colourVbo);
            glColorPointer(4, GL_UNSIGNED_BYTE, 4, (const void*)0);
            gr_glBindBuffer(GL_ARRAY_BUFFER, e.vbo);
        } else {
            glColorPointer(4, GL_UNSIGNED_BYTE, stride, client ? (const void*)client->c : compact ? (const void*)offsetof(MeshBatchAttributes,c) : (const void*)offsetof(MeshBatchVertex,c));
        }
        if (normals) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, stride, client ? (const void*)client->n : compact ? (const void*)offsetof(MeshBatchAttributes,n) : (const void*)offsetof(MeshBatchVertex,n));
        } else glDisableClientState(GL_NORMAL_ARRAY);
        if (!g_meshBakePass || g_meshBakeCutout) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, stride, client ? (const void*)client->uv : compact ? (const void*)offsetof(MeshBatchAttributes,uv) : (const void*)offsetof(MeshBatchVertex,uv));
        } else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    } else {
        glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    }
    bool x2 = false;
    for (size_t ri=0;ri<runCount;++ri) {
        const auto& r=runs[ri];
        if (r.tex == -3) glDisable(GL_TEXTURE_2D);
        else if (!depth) {
            const bool flash = g_meshFlash && pass != MODE_SHADOW && pass != MODE_XLU;
            if (r.tex >= 0 && !flash) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, livery_texture(src.tex[r.tex], house));
                set_wrap((unsigned char)r.wrap);
            } else glDisable(GL_TEXTURE_2D);
        }
        if (r.x2 != x2) {
            if (env < 0) glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &env);
            mesh_batch_x2(r.x2, env); x2 = r.x2;
        }
        if (!outside) { glDrawArrays(GL_TRIANGLES, (GLint)r.first, (GLsizei)r.count); g_meshBatchDraws++; }
    }
    if (x2) mesh_batch_x2(false, env);
    if (!outside && !g_meshArrayDepth) {
        glPopClientAttrib(); gr_glBindBuffer(GL_ARRAY_BUFFER,bound);
    }
    /* Array draws leave current attributes unspecified. Match the immediate path's
       last emitted attributes, because later legacy draws can inherit them. */
    if (!depth) glColor4ubv(e.last.c);
    if (normals) glNormal3fv(e.last.n);
    if (e.uv) glTexCoord2fv(e.last.uv);
}
static void mesh_batch_draw(const MeshBatchEntry& e,const Pack& src,int house,
        int pass,bool depth,bool normals,const MeshBatchVertex* client=NULL) {
    mesh_batch_draw_ranges(e,e.runs.data(),e.runs.size(),src,house,pass,depth,normals,client);
}
/* Replay exactly the same ordered runs, culling each fragment in this pass's
   matrices. Bounds and vertices are the original CPU results, including wind,
   shroud, player paint, clipping, fill planes and the fading depth pre-pass. */
static void mesh_fragment_flush(void) {
    if (g_meshFragmentCommands.empty()) {mesh_fragment_polygon_apply();return;}
    bool streamHit=g_meshFragmentStore && !g_meshFragmentChanged
        && g_meshFragmentInputs.size()==g_meshFragmentStore->inputs.size()
        && g_meshFragmentCount==g_meshFragmentStore->vertices.size()
        && g_meshFragmentStore->vbo;
    if(g_meshFragmentStore && !mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_GPU_OK"))streamHit=false;
    if(!streamHit)mesh_fragment_materialize();
    const bool directPacked=g_meshFragmentPackedCollect;
    if(!directPacked)g_meshFragmentCollectedBytes+=g_meshFragmentVertices.size()*sizeof(MeshBatchVertex);
    const size_t vertexCount=streamHit||directPacked ? g_meshFragmentCount : g_meshFragmentVertices.size();
    const auto& first=g_meshFragmentCommands.front();
    size_t stride=sizeof(MeshBatchVertex),colourOffset=offsetof(MeshBatchVertex,c);
    const bool wantPack=mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_PACK") && !first.normals;
    bool pack=wantPack && mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_PACK_OK");
    const size_t packedStride=first.depth ? 3*sizeof(float) : 5*sizeof(float)+4;
    if(vertexCount>(16u*1024u*1024u)/packedStride)pack=false;
    if(wantPack&&!pack&&!streamHit)g_meshFragmentPackFallbacks++;
    if(directPacked){stride=g_meshFragmentPackedStride;colourOffset=5*sizeof(float);}
    else if(pack) {
        try {
            const size_t packedBytes=vertexCount*packedStride;
            /* Exact growth and a16MiB scratch ceiling. A failed allocation keeps
               the original full-record upload/client path. No geometry is lost. */
            if(g_meshFragmentPacked.capacity()<packedBytes)std::vector<unsigned char>(packedBytes).swap(g_meshFragmentPacked);
            else g_meshFragmentPacked.resize(packedBytes);
            stride=packedStride;colourOffset=5*sizeof(float);
        } catch(const std::bad_alloc&) {pack=false;g_meshFragmentPackFallbacks++;}
    }
    if(streamHit && g_meshFragmentStore->gpuStride!=stride) {
        streamHit=false;mesh_fragment_materialize();
    }
    const size_t bytes=vertexCount*stride;
    const GLuint bound = gls_array_binding();
    GLint env = -1;                       /* asked for before the first doubled run */
    const bool cullWas=glIsEnabled(GL_CULL_FACE), offsetWas=glIsEnabled(GL_POLYGON_OFFSET_FILL);
    /* Transfer ownership of changed streams instead of copying their full payload.
       Compact only excessive scratch slack, so tiny streams cannot retain a large
       preceding allocation. Charge the actual retained capacity, not size. */
    if(g_meshFragmentStore && !streamHit && g_meshFragmentVertices.capacity()>std::max((size_t)4096,vertexCount*2))
        std::vector<MeshBatchVertex>(g_meshFragmentVertices).swap(g_meshFragmentVertices);
    const size_t retained=g_meshFragmentStore ? mesh_fragment_store_footprint(bytes,
        streamHit ? g_meshFragmentStore->vertices.capacity() : g_meshFragmentVertices.capacity(),
        g_meshFragmentInputs,g_meshFragmentCommands) : 0;
    const bool keep=g_meshFragmentStore && mesh_fragment_store_budget(retained);
    GLuint vbo=keep ? g_meshFragmentStore->vbo : 0;
    if(keep && !vbo)gr_glGenBuffers(1,&vbo);
    if(!keep) { streamHit=false;mesh_fragment_materialize();vbo=mesh_batch_frame_store(bytes); }
    if (!vbo) { if (!g_meshBatchStream) gr_glGenBuffers(1,&g_meshBatchStream); vbo=g_meshBatchStream; }
    const void* data=directPacked ? (const void*)g_meshFragmentPacked.data() : (const void*)g_meshFragmentVertices.data();
    if(directPacked)g_meshFragmentPackedBytes+=bytes;
    if(pack && !directPacked && !streamHit) {
        for(size_t i=0;i<vertexCount;++i) {
            unsigned char* d=&g_meshFragmentPacked[i*stride];const MeshBatchVertex& v=g_meshFragmentVertices[i];
            memcpy(d,v.p,sizeof v.p);
            if(!first.depth){memcpy(d+3*sizeof(float),v.uv,sizeof v.uv);memcpy(d+colourOffset,v.c,sizeof v.c);}
        }
        data=g_meshFragmentPacked.data();g_meshFragmentPackedBytes+=bytes;
    }
    gr_glBindBuffer(GL_ARRAY_BUFFER,vbo);
    if(!streamHit) {
        gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)bytes,data,GL_STREAM_DRAW);
        g_meshFragmentUploadBytes+=bytes;g_meshFragmentFullBytes+=vertexCount*sizeof(MeshBatchVertex);
    }
    /* A kept store is refilled in place frame after frame, like the per-frame stores,
       and under OPENCNC3D_QUERY_ELIDE it is asked, as they are, only when a fill is larger
       than any it has been seen to hold: the size check is a round trip to the driver,
       and kept wrecks asked it on every refill, measured at 47 a frame on a Tiberian Dawn
       base while the world ticks (10 a frame when it stands still). The cost of that is
       the per-frame stores' own: a refill no larger than an earlier one that the driver
       nonetheless fails to allocate is drawn from the store rather than from the client
       arrays. */
    const bool gpu=(streamHit || mesh_store_holds(vbo,bytes,!keep || gls_query_elide()))
        && (!keep || mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_GPU_OK"))
        && mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_BUFFER_OK");
    if(streamHit)g_meshFragmentStreamHits++;
    if(keep && !gpu) {
        /* A failed replacement must not leave a deleted prior handle reusable. */
        if(vbo!=g_meshFragmentStore->vbo){g_meshVerified.erase(vbo);gr_glDeleteBuffers(1,&vbo);}
        mesh_fragment_store_release(*g_meshFragmentStore);g_meshFragmentStore=NULL;vbo=0;
    }
    if(keep && gpu && !streamHit) {
        g_meshFragmentStore->gpuBytes=bytes;g_meshFragmentStore->vbo=vbo;g_meshFragmentStore->gpuStride=stride;

    }
    if (!gpu) g_meshVerified.erase(vbo);
    if (!gpu) gr_glBindBuffer(GL_ARRAY_BUFFER,0); /* preserve the bounded client fallback */
    const unsigned char* client=gpu ? NULL : directPacked ? g_meshFragmentPacked.data()
        : (const unsigned char*)g_meshFragmentVertices.data();
    if(client&&!directPacked){stride=sizeof(MeshBatchVertex);colourOffset=offsetof(MeshBatchVertex,c);}
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3,GL_FLOAT,(GLsizei)stride,client ? (const void*)client : (const void*)offsetof(MeshBatchVertex,p));
    const bool traceFragments = perf_trace_frame_active();
    const int traceBucket = traceFragments ? 4 * destruction_trace_pass_kind() : 0;
    if (traceFragments) {
        g_destructTrace[traceBucket] += g_meshFragmentCommands.size();
        g_destructTrace[traceBucket+2] += vertexCount;
        if(!streamHit)g_destructTrace[traceBucket+3] += bytes;
    }
    if (!first.depth) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4,GL_UNSIGNED_BYTE,(GLsizei)stride,client ? (const void*)(client+colourOffset) : (const void*)colourOffset);
        if(first.normals) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT,(GLsizei)stride,client ? (const void*)(client+offsetof(MeshBatchVertex,n)) : (const void*)offsetof(MeshBatchVertex,n));
        } else glDisableClientState(GL_NORMAL_ARRAY);
        if(!g_meshBakePass || g_meshBakeCutout) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2,GL_FLOAT,(GLsizei)stride,client ? (const void*)(client+offsetof(MeshBatchVertex,uv)) : (const void*)offsetof(MeshBatchVertex,uv));
        } else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    } else {
        glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    float clip[16];
    memcpy(clip, gls_clip(), sizeof clip);
    bool cull=cullWas, offset=offsetWas, x2=false;
    const bool coalesce=mesh_perf_switch("OPENCNC3D_MESH_FRAGMENT_COALESCE");
    size_t pendingFirst=0,pendingCount=0;
    const Pack* pendingSrc=NULL;int pendingHouse=0,pendingPass=0,pendingTex=0,pendingWrap=0;
    bool pendingDepth=false,pendingX2=false;
    const auto drawPending=[&]() {
        if(!pendingCount)return;
        glDrawArrays(GL_TRIANGLES,(GLint)pendingFirst,(GLsizei)pendingCount);g_meshBatchDraws++;
        if(traceFragments)g_destructTrace[traceBucket+1]++;
        pendingCount=0;
    };
    for(const auto& cmd:g_meshFragmentCommands) {
        /* A draw captures state when submitted. Drain before every state change,
           culled gap, or material boundary; only contiguous complete triangles
           under exactly the same state can join the pending ordered range. */
        if(cmd.cull!=cull || cmd.offset!=offset)drawPending();
        if(cmd.cull!=cull) { cull=cmd.cull; if(cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE); }
        if(cmd.offset!=offset) { offset=cmd.offset; if(offset) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL); }
        const bool outside=mesh_batch_outside(cmd.entry,clip);
        if(outside) g_meshBatchCulled++;
        for(const auto& r:cmd.entry.runs) {
            const bool join=coalesce && pendingCount && !outside
                && pendingFirst+pendingCount==r.first && pendingCount%3==0 && r.count%3==0
                && pendingSrc==cmd.src && pendingHouse==cmd.house && pendingPass==cmd.pass
                && pendingDepth==cmd.depth && pendingTex==r.tex && pendingWrap==r.wrap && pendingX2==r.x2;
            if(!join)drawPending();
            if(r.tex==-3) glDisable(GL_TEXTURE_2D);
            else if(!cmd.depth) {
                const bool flash=g_meshFlash && cmd.pass!=MODE_SHADOW && cmd.pass!=MODE_XLU;
                if(r.tex>=0 && !flash) { glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D,livery_texture(cmd.src->tex[r.tex],cmd.house));
                    set_wrap((unsigned char)r.wrap);
                } else glDisable(GL_TEXTURE_2D);
            }
            if(r.x2!=x2) {
                if(env<0) glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&env);
                mesh_batch_x2(r.x2,env); x2=r.x2;
            }
            if(!outside) {
                if(join) {pendingCount+=r.count;g_meshFragmentDrawsSaved++;}
                else {pendingFirst=r.first;pendingCount=r.count;pendingSrc=cmd.src;pendingHouse=cmd.house;
                    pendingPass=cmd.pass;pendingDepth=cmd.depth;pendingTex=r.tex;pendingWrap=r.wrap;pendingX2=r.x2;}
            }
        }
    }
    drawPending();
    if(x2) mesh_batch_x2(false,env);
    if(cull!=cullWas) { if(cullWas) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE); }
    if(offset!=offsetWas) { if(offsetWas) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL); }
    mesh_fragment_polygon_apply();
    glPopClientAttrib(); gr_glBindBuffer(GL_ARRAY_BUFFER,bound);
    const auto& last=g_meshFragmentCommands.back();
    if(!last.depth) glColor4ubv(last.entry.last.c);
    if(last.normals) glNormal3fv(last.entry.last.n);
    /* An untextured last range inherits the last textured range's current UV. */
    for(auto it=g_meshFragmentCommands.rbegin();it!=g_meshFragmentCommands.rend();++it)
        if(it->entry.uv) { glTexCoord2fv(it->entry.last.uv); break; }
    if(!streamHit){g_meshBatchUploads++;g_meshBatchUploadBytes+=bytes;} g_meshFragmentFlushes++;
    g_meshFragmentRanges+=(unsigned long)g_meshFragmentCommands.size();
    if(keep && gpu) {
        g_meshFragmentStoreBytes-=g_meshFragmentStore->bytes;
        if(!streamHit)g_meshFragmentStore->vertices.swap(g_meshFragmentVertices);
        g_meshFragmentStore->commands.swap(g_meshFragmentCommands);
        g_meshFragmentStore->inputs.swap(g_meshFragmentInputs);
        g_meshFragmentStore->bytes=mesh_fragment_store_footprint(g_meshFragmentStore->gpuBytes,
            g_meshFragmentStore->vertices.capacity(),g_meshFragmentStore->inputs,g_meshFragmentStore->commands);
        g_meshFragmentStoreBytes+=g_meshFragmentStore->bytes;
    }
    g_meshFragmentVertices.clear(); g_meshFragmentCommands.clear();
    if(directPacked)g_meshFragmentPacked.clear();
    g_meshFragmentInputs.clear();g_meshFragmentCount=0;g_meshFragmentChanged=true;
}
static unsigned long long g_meshAlphaAttributesSkipped=0,g_meshAlphaKeptUploads=0,g_meshAlphaKeptBytes=0;
static std::vector<MeshBatchVertex> g_meshVertexScratch;
static std::vector<MeshBatchAttributes> g_meshAttributeScratch;
static void mesh_scratch_clear() {
    std::vector<MeshBatchVertex>().swap(g_meshVertexScratch);
    std::vector<MeshBatchAttributes>().swap(g_meshAttributeScratch);
}
struct MeshBatch {
    std::vector<MeshBatchVertex> vertices;
    MeshBatchEntry entry;
    MeshBatchVertex current = {};
    size_t first = 0;
    int tex = -2, wrap = -1; bool x2 = false;
    MeshGeometryPose* geometry = NULL;
    MeshKey attributesKey;
    bool fragmentQueued=false;
    void prepare(size_t count) {
        if (!vertices.capacity()) vertices.swap(g_meshVertexScratch);
        vertices.clear(); vertices.reserve(count);
    }
    ~MeshBatch() {
        if (vertices.capacity()>g_meshVertexScratch.capacity()
            && vertices.capacity()*sizeof(MeshBatchVertex)<=16u*1024u*1024u)
            vertices.swap(g_meshVertexScratch);
    }
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
    void finish(const MeshKey& key, const Pack& src, int house,int pass,bool depth,bool normals,bool frame=false) {
        if(vertices.empty()) {
            if(g_meshFragmentCollect)mesh_fragment_input_record(key,0,~(size_t)0);
            /* NOTHING TO DRAW IS AN ANSWER TOO. A part offered to a pass none of its
               triangles belong to (the opaque pass for an all-cutout piece, and the
               reverse) emits nothing, and without an entry the whole triangle walk ran
               again on every draw only to find that out. An entry with no runs draws
               nothing (mesh_batch_draw returns at once). Admit through the same LRU,
               so cap pressure cannot make empty pieces repeatedly walk triangles. */
            if (!key.empty() && !g_meshFragmentCollect) {
                if (frame) mesh_batch_frame_keep(key,MeshBatchEntry());
                else mesh_batch_keep(key, MeshBatchEntry());
            }
            return;
        }
        entry.last=vertices.back(); entry.bytes=vertices.size()*sizeof(MeshBatchVertex);
        if(geometry) {
            memcpy(geometry->lo,entry.lo,sizeof entry.lo);
            memcpy(geometry->hi,entry.hi,sizeof entry.hi);geometry->boundsValid=true;
        }
        if (g_meshFragmentCollect) {
            /* Bound scratch storage. An unusually large asset drains in ordered chunks. */
            if (g_meshFragmentCount*sizeof(MeshBatchVertex)+entry.bytes > 64u*1024u*1024u) {
                mesh_fragment_store_disable();mesh_fragment_flush();
            }
            mesh_fragment_materialize();
            if(g_meshFragmentPackedCollect) {
                const size_t stride=depth?3*sizeof(float):5*sizeof(float)+4;
                if(vertices.size()>(16u*1024u*1024u)/stride
                    ||g_meshFragmentPacked.size()>16u*1024u*1024u-vertices.size()*stride
                    ||(g_meshFragmentCount&&g_meshFragmentPackedStride!=stride))mesh_fragment_flush();
                if(normals||!mesh_fragment_pack_append(vertices,depth)) {
                    /* A compact scratch allocation can fail after earlier pieces.
                       Submit that ordered prefix, then this piece through the existing
                       ordinary batch/client path. Continue with full-record collection. */
                    mesh_fragment_flush();g_meshFragmentPackedCollect=false;g_meshFragmentPackFallbacks++;
                    g_meshFragmentCollect=false;finish(key,src,house,pass,depth,normals,frame);
                    g_meshFragmentCollect=true;return;
                }
            }
            const size_t firstVertex=g_meshFragmentCount;
            mesh_fragment_input_record(key,vertices.size(),g_meshFragmentCommands.size());
            MeshFragmentCommand cmd;
            cmd.entry=std::move(entry); cmd.src=&src; cmd.house=house; cmd.pass=pass;
            cmd.depth=depth; cmd.normals=normals;
            cmd.cull=mesh_fragment_polygon(GL_CULL_FACE); cmd.offset=mesh_fragment_polygon(GL_POLYGON_OFFSET_FILL);
            for(auto& r:cmd.entry.runs)r.first+=firstVertex;
            if(!g_meshFragmentPackedCollect)
                g_meshFragmentVertices.insert(g_meshFragmentVertices.end(),vertices.begin(),vertices.end());
            g_meshFragmentCommands.push_back(std::move(cmd));
            return;
        }
        const GLuint bound = gls_array_binding();
        bool keep=!key.empty() && !frame;
        bool keepAttributes=false;
        bool frameStore=false;
        /* THE BAKE CONSUMES THE FRAME'S POSE AND NEVER MAKES ONE. It drops untextured
           triangles, so a pose it posted first would carry a shorter count and every
           later pass of the frame would fail the count test below and lose sharing.
           When an earlier pass already posted the pose and the counts agree, every
           triangle is textured, so the bake's corners are that pose's corners in the
           same order; its own colour and UV then go into a kept store like the lit
           pass's. */
        const bool bakeConsumes = g_meshBakePass && geometry && geometry->gpu
            && mesh_perf_switch("OPENCNC3D_BAKE_SHARED_POSE");
        if (frame && geometry && (!g_meshBakePass || bakeConsumes) && mesh_perf_switch("OPENCNC3D_MESH_SHARED_POSITIONS")) {
            entry.positionVbo=mesh_geometry_gpu(*geometry,vertices);
            if (entry.positionVbo && depth) {
                entry.vbo=entry.positionVbo; entry.bytes=vertices.size()*3*sizeof(float);
                mesh_batch_draw(entry,src,house,pass,depth,normals);
                mesh_batch_frame_keep(key,entry); return;
            }
            entry.compact=entry.positionVbo!=0;
            keepAttributes=entry.compact && !attributesKey.empty();
            if(keepAttributes) {
                const MeshBatchEntry* cached=mesh_batch_kept_find(attributesKey);
                if(cached && cached->compact) {
                    MeshBatchEntry draw=*cached;draw.positionVbo=entry.positionVbo;
                    memcpy(draw.lo,entry.lo,sizeof draw.lo);memcpy(draw.hi,entry.hi,sizeof draw.hi);
                    mesh_batch_draw(draw,src,house,pass,depth,normals);return;
                }
                keep=true;
            }
        }
        const void* data=vertices.data();
        if (entry.compact) {
            auto& attributes=g_meshAttributeScratch;
            attributes.resize(vertices.size());
            for (size_t i=0;i<vertices.size();i++) {
                memcpy(attributes[i].uv,vertices[i].uv,sizeof attributes[i].uv);
                memcpy(attributes[i].n,vertices[i].n,sizeof attributes[i].n);
                memcpy(attributes[i].c,vertices[i].c,sizeof attributes[i].c);
            }
            data=attributes.data(); entry.bytes=attributes.size()*sizeof(MeshBatchAttributes);
        }
        if(keep && !mesh_batch_admit(entry.bytes)) {keep=false;frame=true;}
        if(keep && g_meshAlphaScale<255 && !depth && !normals && !g_meshBakePass && !g_meshShadowPass) {
            ++g_meshAlphaKeptUploads;g_meshAlphaKeptBytes+=entry.bytes;
        }
        if(keep) gr_glGenBuffers(1,&entry.vbo);
        else {
            if(frame) {entry.vbo=mesh_batch_frame_store(entry.bytes);frameStore=entry.vbo!=0;}
            if(!entry.vbo) {if(!g_meshBatchStream) gr_glGenBuffers(1,&g_meshBatchStream);entry.vbo=g_meshBatchStream;}
        }
        gr_glBindBuffer(GL_ARRAY_BUFFER,entry.vbo);
        gr_glBufferData(GL_ARRAY_BUFFER,(FxGLsizeiptr)entry.bytes,data,keep?GL_STATIC_DRAW:GL_STREAM_DRAW);
        g_meshBatchUploads++; g_meshBatchUploadBytes+=entry.bytes;
        /* A kept store is new every time and always asked; the per-frame and stream
           stores are refilled every frame at much the same size and are asked only
           when they grow past what they have already been seen to hold. */
        if (!mesh_store_holds(entry.vbo, entry.bytes, !keep)) {
            g_meshVerified.erase(entry.vbo);
            gr_glDeleteBuffers(1,&entry.vbo); entry.vbo=0;
            if(frameStore) {
                auto& s=g_meshFrameStores[g_meshFrameNext-1];
                g_meshFrameBytes-=s.bytes; s.bytes=0; s.vbo=0;
            } else if(!keep) g_meshBatchStream=0;
        }
        mesh_batch_draw(entry,src,house,pass,depth,normals,entry.vbo?NULL:vertices.data());
        gr_glBindBuffer(GL_ARRAY_BUFFER,bound);
        if(frameStore && entry.vbo) mesh_batch_frame_keep(key,entry);
        if(keep && entry.vbo) {
            if(keepAttributes && g_meshBakePass)g_meshBakeShareStores++;
            if(keepAttributes){entry.positionVbo=0;mesh_batch_keep(attributesKey,std::move(entry));}
            else mesh_batch_keep(key,std::move(entry));
        }
    }
};
/* THE EMBER GLOW'S STANDING PIECES, KEPT WITH THEIR COLOUR APART (ebld_draw_glow, through
   draw_mesh). The glow lies over a damaged building's standing pieces at an opacity that
   flickers from frame to frame, and that opacity is the only input of those draws that
   changes: draw_mesh multiplies each corner's own alpha by it (g_meshAlphaScale). Kept
   like any still piece, every new opacity would be another copy of the same geometry in
   the persistent cache, so the glow was posed from its triangles, packed and sent to the
   card again for every piece in every frame instead (g_meshTransientColor).
   Here a piece's corners are posed once, with the opacity at full so that each corner
   keeps its own alpha, and kept in a static store under the still-piece key less the
   opacity. A second store holds the colours alone, four bytes a corner. When the opacity
   changes, the alpha bytes are worked out again from the kept alphas by the integer
   expression draw_mesh uses, (alpha * opacity) / 255, and only that store is rewritten.
   The card is given the same positions, texture coordinates and colours as the per-frame
   path gave it, in the same runs and the same order.
   A piece whose key does not come back for 60 frames is released: a piece that falls,
   a building that changes stage or is sold, the shroud moving under it, all give new
   keys. The store is aged at the start of every frame (draw_frame), not only when a
   glow is drawn, so it empties while none is. Entries and bytes are bounded; past either
   bound a draw takes the per-frame path. OPENCNC3D_MESH_GLOW_KEEP=0 puts the per-frame
   path back for every glow piece. */
struct MeshGlowEntry {
    MeshBatchEntry entry;
    std::vector<unsigned char> base;    /* each corner's alpha at full opacity */
    std::vector<unsigned char> colour;  /* RGBA per corner, what the colour store holds */
    int scale = -1;                     /* the opacity the colour store was made for */
    size_t bytes = 0;
};
static std::unordered_map<MeshKey, MeshGlowEntry, MeshKeyHash> g_meshGlowCache;
static size_t g_meshGlowBytes = 0;
static unsigned g_meshGlowSerial = ~0u;
static unsigned long g_meshGlowHits = 0, g_meshGlowBuilds = 0, g_meshGlowRecolours = 0;
static unsigned long g_meshGlowRefused = 0;
enum { MESH_GLOW_MAX_ENTRIES = 4096 };
static const size_t MESH_GLOW_CAP = 64u*1024u*1024u;
static void mesh_glow_release(MeshGlowEntry& g) {
    if (g.entry.vbo) { g_meshVerified.erase(g.entry.vbo); gr_glDeleteBuffers(1, &g.entry.vbo); }
    if (g.entry.colourVbo) {
        g_meshVerified.erase(g.entry.colourVbo); gr_glDeleteBuffers(1, &g.entry.colourVbo);
    }
    g_meshGlowBytes -= g.bytes; g.bytes = 0;
}
static void mesh_glow_clear() {
    if (gr_glDeleteBuffers)
        for (auto& g : g_meshGlowCache) mesh_glow_release(g.second);
    g_meshGlowCache.clear(); g_meshGlowBytes = 0; g_meshGlowSerial = ~0u;
}
/* Once a frame: draw_frame calls it first, and the glow's own calls below find the
   frame already aged (they cover a glow drawn outside draw_frame). */
static void mesh_glow_frame_begin() {
    if (g_meshGlowSerial == g_drawFrameSerial) return;
    g_meshGlowSerial = g_drawFrameSerial;
    for (auto i = g_meshGlowCache.begin(); i != g_meshGlowCache.end();) {
        if (g_drawFrameSerial - i->second.entry.used > 60u) {
            mesh_glow_release(i->second); i = g_meshGlowCache.erase(i);
        } else ++i;
    }
}
static MeshGlowEntry* mesh_glow_find(const MeshKey& key) {
    mesh_glow_frame_begin();
    auto it = g_meshGlowCache.find(key);
    if (it == g_meshGlowCache.end()) return NULL;
    it->second.entry.used = g_drawFrameSerial;
    g_meshGlowHits++;
    return &it->second;
}
/* The kept piece at this draw's opacity: the alpha bytes made again and the colour store
   rewritten only when the opacity differs from the one it holds. */
static void mesh_glow_draw(MeshGlowEntry& g, const Pack& src, int house, int pass,
                           bool depth, bool normals) {
    const size_t n = g.base.size();
    if (n && g.scale != (int)g_meshAlphaScale) {
        for (size_t i = 0; i < n; i++)
            g.colour[i * 4 + 3] = (unsigned char)(((int)g.base[i] * (int)g_meshAlphaScale) / 255);
        const GLuint was = gls_array_binding();
        gr_glBindBuffer(GL_ARRAY_BUFFER, g.entry.colourVbo);
        gr_glBufferSubData(GL_ARRAY_BUFFER, 0, (FxGLsizeiptr)(n * 4), g.colour.data());
        gr_glBindBuffer(GL_ARRAY_BUFFER, was);
        g.entry.last.c[3] = g.colour[(n - 1) * 4 + 3];
        g.scale = g_meshAlphaScale;
        g_meshGlowRecolours++;
        g_meshBatchUploadBytes += n * 4;
    }
    mesh_batch_draw(g.entry, src, house, pass, depth, normals);
}
/* A glow piece just posed by draw_mesh, its corners' alphas at full opacity (draw_mesh
   walks a kept glow piece with the opacity at 255): kept, then drawn at this draw's
   opacity. Refused, the corners get this draw's opacity the same way and go through the
   ordinary per-frame store, as before. */
static void mesh_glow_keep(const MeshKey& key, MeshBatch& out, const Pack& src, int house,
                           int pass, bool depth, bool normals) {
    mesh_glow_frame_begin();
    std::vector<MeshBatchVertex>& v = out.vertices;
    const size_t n = v.size();
    const size_t bytes = n * (sizeof(MeshBatchVertex) + 4 + 4 + 1) + 256;
    MeshGlowEntry* g = NULL;
    if (g_meshGlowCache.size() < MESH_GLOW_MAX_ENTRIES && bytes <= MESH_GLOW_CAP - g_meshGlowBytes) {
        try {
            auto slot = g_meshGlowCache.emplace(key, MeshGlowEntry());
            if (slot.second) {
                g = &slot.first->second;
                g->base.resize(n); g->colour.resize(n * 4);
            }
        } catch (const std::bad_alloc&) {
            if (g) { g_meshGlowCache.erase(key); g = NULL; }
        }
    }
    bool ok = g != NULL;
    if (ok && n) {
        for (size_t i = 0; i < n; i++) {
            g->base[i] = v[i].c[3];
            memcpy(&g->colour[i * 4], v[i].c, 3);
            g->colour[i * 4 + 3] = (unsigned char)(((int)v[i].c[3] * (int)g_meshAlphaScale) / 255);
        }
        const GLuint bound = gls_array_binding();
        MeshBatchEntry& e = g->entry;
        e.bytes = n * sizeof(MeshBatchVertex);
        gr_glGenBuffers(1, &e.vbo); gr_glGenBuffers(1, &e.colourVbo);
        gr_glBindBuffer(GL_ARRAY_BUFFER, e.vbo);
        gr_glBufferData(GL_ARRAY_BUFFER, (FxGLsizeiptr)e.bytes, v.data(), GL_STATIC_DRAW);
        ok = mesh_store_holds(e.vbo, e.bytes, false);
        gr_glBindBuffer(GL_ARRAY_BUFFER, e.colourVbo);
        gr_glBufferData(GL_ARRAY_BUFFER, (FxGLsizeiptr)(n * 4), g->colour.data(), GL_DYNAMIC_DRAW);
        ok = mesh_store_holds(e.colourVbo, n * 4, false) && ok;
        gr_glBindBuffer(GL_ARRAY_BUFFER, bound);
        g_meshBatchUploads += 2; g_meshBatchUploadBytes += e.bytes + n * 4;
        if (ok) {
            e.runs.swap(out.entry.runs);
            memcpy(e.lo, out.entry.lo, sizeof e.lo); memcpy(e.hi, out.entry.hi, sizeof e.hi);
            e.uv = out.entry.uv;
            e.last = v.back(); e.last.c[3] = g->colour[(n - 1) * 4 + 3];
        }
    }
    if (!ok) {
        if (g) { mesh_glow_release(*g); g_meshGlowCache.erase(key); }
        g_meshGlowRefused++;
        for (auto& c : v) c.c[3] = (unsigned char)(((int)c.c[3] * (int)g_meshAlphaScale) / 255);
        out.finish(MeshKey(), src, house, pass, depth, normals, false);
        return;
    }
    g->scale = g_meshAlphaScale;
    g->entry.used = g_drawFrameSerial;
    g->bytes = bytes; g_meshGlowBytes += bytes; g_meshGlowBuilds++;
    mesh_glow_draw(*g, src, house, pass, depth, normals);
}
template<class T> static void mesh_batch_key(MeshKey& key,const T& v) {
    key.append((const char*)&v,sizeof v);
}
/* Active pieces keep their exact CPU positions, but used to re-walk source
   texture/colour/run preparation in every pass after those positions were ready.
   These immutable source templates remove that repeated walk. They never contain
   a pose, opacity or camera, and the caller opts in only where all vertex-varying
   source effects are absent. Other cases keep the original vertex loop. */
struct MeshPreparedCorner {
    float uv[2];
    unsigned char c[4];
    uint32_t source;
    unsigned char selflit;
    bool textured;
};
struct MeshPreparedEntry {
    std::vector<MeshPreparedCorner> corners;
    std::vector<MeshBatchVertex> flat;
    std::vector<MeshBatchRun> runs;
    bool uv=false;
    bool compatible=true;
    bool boundsReady=false,boundsFinite=true;
    float lo[3]={},hi[3]={};
    size_t bytes=0;
    std::list<MeshKey>::iterator lru;
};
/* A culled rigid range needs its bounds, material runs and final attributes,
   not a copy of every corner. This fixed four-way table survives source-LRU
   evictions, but shares the source cache's explicit pack-generation clears.
   Its complete allocation is reserved from the same source memory budget. */
static bool mesh_cull_plan_enabled() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_MESH_CULL_PLAN");return !p||strcmp(p,"0");}();return on;
}
static bool mesh_cull_plan_borrow_enabled() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_MESH_CULL_PLAN_BORROW_RUNS");
#ifdef __APPLE__
        return !p||strcmp(p,"0"); // Qualified Mac bundle; Windows gain was below drift.
#else
        return p&&strcmp(p,"0");
#endif
}();return on;
}
static unsigned long long g_meshCullPlanBorrows=0;
struct MeshCullPlan {
    unsigned char key[96]={};size_t keySize=0;
    unsigned long long generation=0;
    std::array<MeshBatchRun,8> runs;size_t runCount=0;
    MeshPreparedCorner last={};float lo[3]={},hi[3]={};bool uv=false;
};
static MeshCullPlan g_meshCullPlans[256][4];
static unsigned char g_meshCullPlanNext[256]={};
static unsigned long long g_meshCullPlanGeneration=1,g_meshCullPlanHits=0,g_meshCullPlanMisses=0,g_meshCullPlanRefusals=0;
static_assert(sizeof(g_meshCullPlans)+sizeof(g_meshCullPlanNext)<1024u*1024u,"cull plan budget");
static size_t mesh_cull_plan_bytes() {return mesh_cull_plan_enabled()?sizeof(g_meshCullPlans)+sizeof(g_meshCullPlanNext):0;}
static std::unordered_map<MeshKey,MeshPreparedEntry,MeshKeyHash> g_meshPrepared;
static std::list<MeshKey> g_meshPreparedLRU;
static size_t g_meshPreparedBytes=0;
static size_t mesh_prepared_capacity() {
    static const size_t cap=[](){const size_t limit=sizeof(void*)>=8?128u:32u;
        const char* p=getenv("OPENCNC3D_MESH_PREPARED_MB");
        const size_t defaultMB=sizeof(void*)>=8?64u:32u;
        if(!p||!*p)return size_t(defaultMB*1024u*1024u);
        char* end=NULL;const unsigned long mb=strtoul(p,&end,10);
        return end&&!*end&&mb>=16&&mb<=limit?size_t(mb)*1024u*1024u:size_t(defaultMB*1024u*1024u);}();return cap;
}
static size_t mesh_prepared_available_capacity() {return mesh_prepared_capacity()-mesh_cull_plan_bytes();}
static unsigned long long g_meshPreparedHits=0,g_meshPreparedBuilds=0,g_meshPreparedVertices=0,g_meshPreparedFallbacks=0;
static unsigned long long g_meshPreparedLookups=0,g_meshPreparedSourceHits=0,g_meshPreparedEvictions=0;
static bool mesh_source_compact_enabled() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_MESH_SOURCE_COMPACT");return !p||strcmp(p,"0");}();return on;
}
static unsigned long long g_meshPreparedEpoch=0,g_meshPreparedMemoHits=0,g_meshPreparedPromotions=0;
struct MeshPreparedMemo {MeshKey key;MeshPreparedEntry* entry=NULL;unsigned long long epoch=~0ull;};
static MeshPreparedMemo g_meshPreparedMemo[2];static unsigned g_meshPreparedMemoNext=0;
static void mesh_prepared_remember(const MeshKey& key,MeshPreparedEntry& entry) {
    auto& m=g_meshPreparedMemo[g_meshPreparedMemoNext++&1u];
    memcpy(m.key.data.data(),key.data.data(),key.length);m.key.length=key.length;m.key.hashed=false;
    m.entry=&entry;m.epoch=g_meshPreparedEpoch;
}
static void mesh_prepared_evict_front() {
    auto old=g_meshPrepared.find(g_meshPreparedLRU.front());
    g_meshPreparedBytes-=old->second.bytes;g_meshPrepared.erase(old);g_meshPreparedLRU.pop_front();
    ++g_meshPreparedEvictions;++g_meshPreparedEpoch;
}
/* A range's UV/material source and its optional flat draw records are one entry.
   Promote in place, retaining the same bytes as the original flat constructor.
   No caller loses the old loop when allocation or the bounded budget refuses. */
static bool mesh_prepared_promote(MeshPreparedEntry& e,const PackMesh& mesh,size_t first) {
    const size_t n=e.corners.size();if(e.flat.size()==n)return true;
    const size_t cap=mesh_perf_switch("OPENCNC3D_MESH_PREPARED_BUDGET_FULL")?mesh_prepared_available_capacity():256u;
    if(e.bytes>cap||n>(cap-e.bytes)/sizeof(MeshBatchVertex)||!mesh_perf_switch("OPENCNC3D_MESH_PREPARED_ALLOC_OK"))return false;
    try {
        std::vector<MeshBatchVertex> flat(n);const size_t extra=flat.capacity()*sizeof(MeshBatchVertex);
        if(extra>cap-e.bytes)return false;
        for(size_t i=0;i<n;++i) {
            const auto& c=e.corners[i];const PackVert& v=mesh.tris[first+c.source/3].v[c.source%3];
            MeshBatchVertex& out=flat[i];out.p[0]=v.x;out.p[1]=v.y;out.p[2]=v.z;
            memcpy(out.uv,c.uv,sizeof out.uv);memcpy(out.c,c.c,sizeof out.c);
        }
        g_meshPreparedLRU.splice(g_meshPreparedLRU.end(),g_meshPreparedLRU,e.lru);
        while(g_meshPreparedBytes>cap-extra)mesh_prepared_evict_front();
        e.flat.swap(flat);e.bytes+=extra;g_meshPreparedBytes+=extra;++g_meshPreparedPromotions;return true;
    }catch(const std::bad_alloc&){return false;}
}
static const MeshPreparedEntry* mesh_prepared_compact_use(MeshPreparedEntry& e,const PackMesh& mesh,size_t first,bool flat) {
    if(flat&&e.compatible&&!mesh_prepared_promote(e,mesh,first)){++g_meshPreparedFallbacks;return NULL;}
    g_meshPreparedLRU.splice(g_meshPreparedLRU.end(),g_meshPreparedLRU,e.lru);return &e;
}

static void mesh_prepared_cache_dump() {
    printf("MESHCULLPLANBORROW|enabled=%d|draws=%llu\n",mesh_cull_plan_borrow_enabled()?1:0,g_meshCullPlanBorrows);
    printf("MESHCULLPLAN|enabled=%d|hits=%llu|misses=%llu|refusals=%llu|bytes=%zu|generation=%llu\n",mesh_cull_plan_enabled()?1:0,g_meshCullPlanHits,g_meshCullPlanMisses,g_meshCullPlanRefusals,mesh_cull_plan_bytes(),g_meshCullPlanGeneration);
    printf("MESHSOURCECOMPACT|enabled=%d|memo_hits=%llu|promotions=%llu|epoch=%llu\n",mesh_source_compact_enabled()?1:0,g_meshPreparedMemoHits,g_meshPreparedPromotions,g_meshPreparedEpoch);
    printf("MESHPREPAREDCACHE|lookups=%llu|hits=%llu|evictions=%llu|entries=%zu|bytes=%zu|cap_bytes=%zu\n",g_meshPreparedLookups,g_meshPreparedSourceHits,g_meshPreparedEvictions,g_meshPrepared.size(),g_meshPreparedBytes,mesh_prepared_capacity());
}

static void mesh_prepared_clear() {
    if(++g_meshCullPlanGeneration==0){for(auto& set:g_meshCullPlans)for(auto& p:set)p.generation=0;g_meshCullPlanGeneration=1;}
    ++g_meshPreparedEpoch;g_meshPrepared.clear();g_meshPreparedLRU.clear();g_meshPreparedBytes=0;
}
static const MeshPreparedEntry* mesh_prepared_source(const Pack& src,const PackMesh& mesh,
        size_t first,size_t last,int pass,bool depth,bool normals,bool hasX2,bool allowTreads=false,bool flat=false) {
    if(last-first>0xffffffffu/3u)return NULL;
    const bool compact=mesh_source_compact_enabled();
    MeshKey key;const PackTri* triangles=mesh.tris.data();
    mesh_batch_key(key,triangles);mesh_batch_key(key,mesh.tris.size());
    mesh_batch_key(key,first);mesh_batch_key(key,last);mesh_batch_key(key,pass);
    mesh_batch_key(key,depth);mesh_batch_key(key,hasX2);const bool treadKey=compact&&mesh.gpuHasTread==0?false:allowTreads;mesh_batch_key(key,treadKey);if(flat&&!compact){const uint64_t schema=0x464c415450524550ull;mesh_batch_key(key,schema);}
    mesh_batch_key(key,g_meshBakePass);mesh_batch_key(key,g_meshBakeCutout);mesh_batch_key(key,g_meshFlash);
    /* The caller excludes deformed source ranges. A wave elsewhere in this same
       mesh must not invalidate the immutable walls' source attributes each frame. */
    mesh_batch_key(key,&src);
    ++g_meshPreparedLookups;
    if(compact)for(auto& memo:g_meshPreparedMemo)
        if(memo.epoch==g_meshPreparedEpoch&&memo.entry&&memo.key==key) {
            ++g_meshPreparedSourceHits;++g_meshPreparedMemoHits;
            const auto* ready=mesh_prepared_compact_use(*memo.entry,mesh,first,flat);
            if(ready)mesh_prepared_remember(key,*memo.entry);return ready;
        }
    auto found=g_meshPrepared.find(key);
    if(found!=g_meshPrepared.end()) {
        ++g_meshPreparedSourceHits;
        if(compact){const auto* ready=mesh_prepared_compact_use(found->second,mesh,first,flat);
            if(ready)mesh_prepared_remember(key,found->second);return ready;}
        g_meshPreparedLRU.splice(g_meshPreparedLRU.end(),g_meshPreparedLRU,found->second.lru);
        return &found->second;
    }
    const size_t cap=mesh_perf_switch("OPENCNC3D_MESH_PREPARED_BUDGET_FULL") ? mesh_prepared_available_capacity() : 256u;
    size_t n=0;bool compatible=true;
    for(size_t i=first;i<last;++i)
        if((int)mesh.tris[i].mode==pass && (!g_meshBakePass||mesh.tris[i].tex>=0)) {
            n+=3;if(!allowTreads && (mesh.tris[i].wrap>>4))compatible=false;
        }
    if(!compatible)n=0;
    /* Charge both copies of each inline key (map + LRU) and conservative nodes. */
    const size_t overhead=sizeof(MeshPreparedEntry)+2u*sizeof(MeshKey)+192u;
    const size_t cornerBudget=sizeof(MeshPreparedCorner)+sizeof(MeshBatchRun)+(flat?sizeof(MeshBatchVertex):0);
    if(overhead>cap||n>(cap-overhead)/cornerBudget)return NULL;
    if(!mesh_perf_switch("OPENCNC3D_MESH_PREPARED_ALLOC_OK")){g_meshPreparedFallbacks++;return NULL;}
    try {
    MeshPreparedEntry entry;entry.compatible=compatible;entry.corners.reserve(n);if(flat)entry.flat.reserve(n);
    MeshBatchVertex current={};int tex=-2,wrap=-1;bool x2=false,started=false;size_t runFirst=0;
    const auto end=[&]() {if(entry.corners.size()>runFirst)entry.runs.push_back({tex,wrap,x2,runFirst,entry.corners.size()-runFirst});};
    for(size_t i=first;compatible && i<last;++i) {
        const PackTri& t=mesh.tris[i];if((int)t.mode!=pass)continue;
        int nextTex=tex,nextWrap=wrap;
        if(g_meshBakePass && !g_meshBakeCutout) {if(t.tex<0)continue;nextTex=-3;}
        else if(!depth && (t.tex!=tex || (t.tex>=0&&(int)t.wrap!=wrap))) {
            nextTex=t.tex;
            const bool flash=g_meshFlash && t.mode!=MODE_SHADOW && t.mode!=MODE_XLU;
            nextWrap=nextTex>=0&&!flash ? (int)t.wrap : -1;
        }
        const bool nextX2=hasX2 && nextTex>=0 && (mesh.tfl[i]&2)!=0;
        if(started && (nextTex!=tex || nextWrap!=wrap || nextX2!=x2)) {end();started=false;}
        tex=nextTex;wrap=nextWrap;x2=nextX2;
        if(!started){runFirst=entry.corners.size();started=true;}
        for(int k=0;k<3;++k) {
            const PackVert& v=t.v[k];
            const float position[3]={v.x,v.y,v.z};
            for(int axis=0;axis<3;axis++) {
                if(!std::isfinite(position[axis]))entry.boundsFinite=false;
                if(!entry.boundsReady)entry.lo[axis]=entry.hi[axis]=position[axis];
                else {entry.lo[axis]=std::min(entry.lo[axis],position[axis]);entry.hi[axis]=std::max(entry.hi[axis],position[axis]);}
            }
            entry.boundsReady=true;
            if(tex>=0) {
                const PackTex& tx=src.tex[tex];
                current.uv[0]=v.u*(float)tx.uw/(float)tx.w;
                current.uv[1]=v.v*(float)tx.uh/(float)tx.h;entry.uv=true;
            }
            if(!depth) {current.c[0]=v.r;current.c[1]=v.g;current.c[2]=v.b;current.c[3]=v.a;}
            MeshPreparedCorner c;memcpy(c.uv,current.uv,sizeof c.uv);memcpy(c.c,current.c,sizeof c.c);
            c.source=(uint32_t)((i-first)*3+(size_t)k);
            c.selflit=(i<mesh.tfl.size()&&(mesh.tfl[i]&1))?255:0;c.textured=t.tex>=0;
            entry.corners.push_back(c);
            if(flat){MeshBatchVertex base=current;base.p[0]=v.x;base.p[1]=v.y;base.p[2]=v.z;entry.flat.push_back(base);}
        }
    }
    if(started)end();
    entry.bytes=entry.corners.capacity()*sizeof(MeshPreparedCorner)+entry.runs.capacity()*sizeof(MeshBatchRun)
        +entry.flat.capacity()*sizeof(MeshBatchVertex)+overhead;
    if(entry.bytes>cap)return NULL;
    while(!g_meshPreparedLRU.empty()&&(g_meshPreparedBytes+entry.bytes>cap||g_meshPrepared.size()>=4096)) {
        mesh_prepared_evict_front();
    }
    auto inserted=g_meshPrepared.emplace(key,std::move(entry));
    try {g_meshPreparedLRU.push_back(key);}
    catch(const std::bad_alloc&){g_meshPrepared.erase(inserted.first);++g_meshPreparedEpoch;g_meshPreparedFallbacks++;return NULL;}
    inserted.first->second.lru=--g_meshPreparedLRU.end();g_meshPreparedBytes+=inserted.first->second.bytes;
    g_meshPreparedBuilds++;if(compact)mesh_prepared_remember(key,inserted.first->second);return &inserted.first->second;
    } catch(const std::bad_alloc&){g_meshPreparedFallbacks++;return NULL;}
}
static const MeshCullPlan* mesh_cull_plan_source(const Pack& src,const PackMesh& mesh,
        size_t first,size_t last,int pass,bool depth,bool normals,bool hasX2) {
    if(!mesh_cull_plan_enabled())return NULL;
    MeshKey key;const PackTri* triangles=mesh.tris.data();
    mesh_batch_key(key,triangles);mesh_batch_key(key,mesh.tris.size());
    mesh_batch_key(key,first);mesh_batch_key(key,last);mesh_batch_key(key,pass);
    mesh_batch_key(key,depth);mesh_batch_key(key,hasX2);
    mesh_batch_key(key,g_meshBakePass);mesh_batch_key(key,g_meshBakeCutout);mesh_batch_key(key,g_meshFlash);mesh_batch_key(key,&src);
    if(key.length>sizeof(MeshCullPlan::key)){++g_meshCullPlanRefusals;return NULL;}
    const size_t bucket=key.hash()&255u;
    for(const auto& p:g_meshCullPlans[bucket])if(p.generation==g_meshCullPlanGeneration
        &&p.keySize==key.length&&!memcmp(p.key,key.data.data(),key.length)){++g_meshCullPlanHits;return &p;}
    ++g_meshCullPlanMisses;
    const auto* source=mesh_prepared_source(src,mesh,first,last,pass,depth,normals,hasX2);
    if(!source||!source->compatible||!source->boundsReady||!source->boundsFinite
        ||source->corners.empty()||source->runs.size()>8){++g_meshCullPlanRefusals;return NULL;}
    auto& p=g_meshCullPlans[bucket][g_meshCullPlanNext[bucket]++&3u];
    memcpy(p.key,key.data.data(),key.length);p.keySize=key.length;
    p.runCount=source->runs.size();std::copy(source->runs.begin(),source->runs.end(),p.runs.begin());
    p.last=source->corners.back();p.uv=source->uv;
    memcpy(p.lo,source->lo,sizeof p.lo);memcpy(p.hi,source->hi,sizeof p.hi);
    p.generation=g_meshCullPlanGeneration;return &p;
}
/* Replay the exact runs and final current attributes of an invisible rigid part.
   Positions cannot affect later draws; normal/colour/UV and material order can. */
static bool mesh_early_replay_fields(MeshBatchEntry& entry,const Pack& src,const PackMesh& mesh,
        size_t first,size_t last,int pass,bool depth,bool normals,bool hasX2,
        const float* extra,float rc,float rs,bool wob,float wsx,float wcx,float wsz,float wcz,
        const float* lo,const float* hi,const MeshBatchRun* runs,size_t runCount,const MeshPreparedCorner& c,bool uv,bool copyRuns) {
    try{if(copyRuns)entry.runs.assign(runs,runs+runCount);}catch(const std::bad_alloc&){return false;}
    entry.uv=uv;memcpy(entry.lo,lo,sizeof entry.lo);memcpy(entry.hi,hi,sizeof entry.hi);
    memcpy(entry.last.uv,c.uv,sizeof c.uv);memcpy(entry.last.c,c.c,sizeof c.c);
    if(entry.uv && (g_meshUVScaleU!=1.0f || g_meshUVScaleV!=1.0f || g_meshUVOffU!=0.0f || g_meshUVOffV!=0.0f)) {
        bool found=false;
        for(size_t i=last;i>first;) {
            const PackTri& t=mesh.tris[--i];
            if((int)t.mode!=pass || t.tex<0)continue;
            if((size_t)t.tex>=src.tex.size())return false;
            const PackVert& v=t.v[2];const PackTex& tx=src.tex[t.tex];
            const float uu=v.u*g_meshUVScaleU+g_meshUVOffU;
            const float vv=v.v*g_meshUVScaleV+g_meshUVOffV;
            entry.last.uv[0]=uu*(float)tx.uw/(float)tx.w;
            entry.last.uv[1]=vv*(float)tx.uh/(float)tx.h;
            found=true;break;
        }
        if(!found)return false;
    }
    if(normals) {
        const float* nn=&mesh.nrm[(first+c.source/3)*9+(c.source%3)*3];
        float nx=nn[0],ny=nn[1],nz=nn[2];
        if(extra) {
            const float x=extra[0]*nx+extra[1]*ny+extra[2]*nz;
            const float y=extra[4]*nx+extra[5]*ny+extra[6]*nz;
            const float z=extra[8]*nx+extra[9]*ny+extra[10]*nz;
            nx=x;ny=y;nz=z;
        }
        float n[3]={nx*rc+nz*rs,ny,-nx*rs+nz*rc};
        if(wob) {
            const float y1=n[1]*wcx-n[2]*wsx,z1=n[1]*wsx+n[2]*wcx;
            const float x2=n[0]*wcz-y1*wsz,y2=n[0]*wsz+y1*wcz;
            n[0]=x2;n[1]=y2;n[2]=z1;
        }
        memcpy(entry.last.n,n,sizeof n);entry.last.c[0]=entry.last.c[1]=entry.last.c[2]=255;entry.last.c[3]=c.selflit;
    } else if(!depth) {
        if(g_meshBakePass)entry.last.c[3]=255;
        else if(g_meshShadowPass){entry.last.c[0]=entry.last.c[1]=entry.last.c[2]=0;entry.last.c[3]=g_meshShadowAlpha;}
        else if(g_meshFlash&&c.textured&&pass!=MODE_SHADOW&&pass!=MODE_XLU)entry.last.c[3]=g_meshAlphaScale;
        else entry.last.c[3]=(unsigned char)(((int)c.c[3]*(int)g_meshAlphaScale)/255);
    }
    return true;
}
static bool mesh_early_replay(MeshBatchEntry& entry,const Pack& src,const PackMesh& mesh,
        size_t first,size_t last,int pass,bool depth,bool normals,bool hasX2,
        const float* extra,float rc,float rs,bool wob,float wsx,float wcx,float wsz,float wcz,
        const float* lo,const float* hi,const MeshPreparedEntry* supplied=NULL,const MeshCullPlan* plan=NULL,bool copyPlanRuns=true) {
    if(plan)return mesh_early_replay_fields(entry,src,mesh,first,last,pass,depth,normals,hasX2,
        extra,rc,rs,wob,wsx,wcx,wsz,wcz,lo,hi,plan->runs.data(),plan->runCount,plan->last,plan->uv,copyPlanRuns);
    const auto* p=supplied?supplied:mesh_prepared_source(src,mesh,first,last,pass,depth,normals,hasX2);
    if(!p||!p->compatible||p->corners.empty())return false;
    return mesh_early_replay_fields(entry,src,mesh,first,last,pass,depth,normals,hasX2,
        extra,rc,rs,wob,wsx,wcx,wsz,wcz,lo,hi,p->runs.data(),p->runs.size(),p->corners.back(),p->uv,true);
}
static bool mesh_prepare_fragment(MeshBatch& output,const Pack& src,const PackMesh& mesh,
        MeshGeometryPose& geometry,size_t first,size_t last,int pass,bool depth,bool normals,
        bool hasX2,const float* extra,float rc,float rs,int house) {
    if(!geometry.boundsValid || geometry.fresh)return false;
    const MeshPreparedEntry* prepared=mesh_prepared_source(src,mesh,first,last,pass,depth,normals,hasX2);
    if(!prepared||!prepared->compatible)return false;
    /* A template stores only this pass's emitted corners. Other modes in the same
       range may have no resolved position and must neither read nor generate one. */
    for(const auto& c:prepared->corners)if(!(geometry.valid[c.source]&1))return false;
    std::vector<MeshBatchRun> runs;
    try {runs=prepared->runs;}catch(const std::bad_alloc&){g_meshPreparedFallbacks++;return false;}
    unsigned char* direct=NULL;
    if(g_meshFragmentPackedCollect&&!normals) {
        const size_t stride=depth?3*sizeof(float):5*sizeof(float)+4;
        if(prepared->corners.size()>(16u*1024u*1024u)/stride
            ||g_meshFragmentPacked.size()>16u*1024u*1024u-prepared->corners.size()*stride
            ||(g_meshFragmentCount&&g_meshFragmentPackedStride!=stride))mesh_fragment_flush();
        direct=mesh_fragment_pack_space(prepared->corners.size(),depth);
        if(!direct)return false;
    } else output.prepare(prepared->corners.size());
    output.entry.runs.swap(runs);output.entry.uv=prepared->uv;
    memcpy(output.entry.lo,geometry.lo,sizeof geometry.lo);memcpy(output.entry.hi,geometry.hi,sizeof geometry.hi);
    size_t index=0;
    for(const auto& c:prepared->corners) {
        MeshBatchVertex v={};memcpy(v.uv,c.uv,sizeof v.uv);memcpy(v.c,c.c,sizeof v.c);
        memcpy(v.p,geometry.corners[c.source].p,sizeof v.p);
        if(normals) {
            if(!(geometry.valid[c.source]&2)) {
                const size_t i=first+c.source/3,k=c.source%3;
                const float* nn=&mesh.nrm[i*9+k*3];float nx=nn[0],ny=nn[1],nz=nn[2];
                if(extra) {
                    const float ax=extra[0]*nx+extra[1]*ny+extra[2]*nz;
                    const float ay=extra[4]*nx+extra[5]*ny+extra[6]*nz;
                    const float az=extra[8]*nx+extra[9]*ny+extra[10]*nz;
                    nx=ax;ny=ay;nz=az;
                }
                const float n[3]={nx*rc+nz*rs,ny,-nx*rs+nz*rc};
                memcpy(geometry.corners[c.source].n,n,sizeof n);geometry.valid[c.source]|=2;
            }
            memcpy(v.n,geometry.corners[c.source].n,sizeof v.n);
            v.c[0]=v.c[1]=v.c[2]=255;v.c[3]=c.selflit;
        } else if(!depth) {
            if(g_meshBakePass)v.c[3]=255;
            else if(g_meshShadowPass){v.c[0]=v.c[1]=v.c[2]=0;v.c[3]=g_meshShadowAlpha;}
            else if(g_meshFlash&&c.textured&&pass!=MODE_SHADOW&&pass!=MODE_XLU)v.c[3]=g_meshAlphaScale;
            else v.c[3]=(unsigned char)(((int)c.c[3]*(int)g_meshAlphaScale)/255);
        }
        if(direct) {
            unsigned char* d=direct+index*g_meshFragmentPackedStride;memcpy(d,v.p,sizeof v.p);
            if(!depth){memcpy(d+3*sizeof(float),v.uv,sizeof v.uv);memcpy(d+5*sizeof(float),v.c,sizeof v.c);}
            output.entry.last=v;
        } else output.vertices.push_back(v);
        ++index;
    }
    if(direct) {
        output.entry.bytes=index*sizeof(MeshBatchVertex);
        MeshFragmentCommand cmd;cmd.entry=std::move(output.entry);cmd.src=&src;cmd.house=house;cmd.pass=pass;
        cmd.depth=depth;cmd.normals=normals;cmd.cull=mesh_fragment_polygon(GL_CULL_FACE);cmd.offset=mesh_fragment_polygon(GL_POLYGON_OFFSET_FILL);
        for(auto& run:cmd.entry.runs)run.first+=g_meshFragmentCount;
        MeshKey empty;mesh_fragment_input_record(empty,index,g_meshFragmentCommands.size());
        g_meshFragmentCommands.push_back(std::move(cmd));output.fragmentQueued=true;
    }
    g_meshPreparedHits++;g_meshPreparedVertices+=index;return true;
}
static void mesh_fragment_polygon_drain() {
    if(!g_meshFragmentStateDeferred)return;
    mesh_fragment_flush();mesh_fragment_polygon_apply();g_meshFragmentStateDeferred=false;
}
static void mesh_wind_gpu_dump();   /* the trees' poses made by the card (mesh_wind_gpu.h) */
/* A fading wreck can have a different alpha byte every frame. Keeping every
   such UV/RGBA variant as a new persistent object creates short-lived buffers.
   The original bounded frame stream already handles these exact bytes. */
static bool mesh_alpha_attributes_stream_enabled() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_MESH_ALPHA_ATTRIBUTES_STREAM");
#if defined(_WIN32)
        return !p||strcmp(p,"0");
#else
        return p&&strcmp(p,"0");
#endif
}();return on;
}
static int mesh_rigid_attributes_mode() {
    static const int mode=[](){
        const char* p=getenv("OPENCNC3D_MESH_RIGID_ATTRIBUTES");
        if(p)return !strcmp(p,"0")?0:!strcmp(p,"verify")?2:1;
#ifdef _WIN32
        return 1; /* Native RTX measurements improve; Mac keeps its measured path. */
#else
        return 0;
#endif
    }();
    return mode;
}
static unsigned long long g_meshRigidAttributeOffers=0,g_meshRigidAttributeHits=0;
static unsigned long long g_meshRigidAttributeVerified=0,g_meshRigidAttributeBad=0;
static void mesh_batch_dump(void) {
    printf("MESHATTRALPHASTREAM|enabled=%d|skipped=%llu|kept_uploads=%llu|kept_bytes=%llu\n",mesh_alpha_attributes_stream_enabled()?1:0,g_meshAlphaAttributesSkipped,g_meshAlphaKeptUploads,g_meshAlphaKeptBytes);
    printf("VEHICLEGLOWSINGLEPASS|enabled=%d|skipped=%llu|sharing=%d\n",
        mesh_vehicle_glow_single_pass()?1:0,g_meshSinglePassSkipped,g_meshGeometrySharing?1:0);
    printf("MESHRIGIDATTRIBUTES|offers=%llu|hits=%llu|verified=%llu|bad=%llu\n",
        g_meshRigidAttributeOffers,g_meshRigidAttributeHits,g_meshRigidAttributeVerified,g_meshRigidAttributeBad);
    printf("MESHEARLYCULL|tests=%llu|rejected=%llu|verified=%llu|bad=%llu\n",
        g_meshEarlyCullTests,g_meshEarlyCullRejected,g_meshEarlyCullVerified,g_meshEarlyCullBad);
    printf("MESHPERF|hits=%lu|uploads=%lu|culled=%lu|draws=%lu|class_skip=%lu|entries=%zu|bytes=%zu|pose_hits=%lu|pose_entries=%zu|pose_bytes=%zu|fragment_flushes=%lu|fragment_ranges=%lu|placed_promoted=%lu|imm_verts=%lu|upload_bytes=%llu\n",
      g_meshBatchHits,g_meshBatchUploads,g_meshBatchCulled,g_meshBatchDraws,g_meshClassSkipped,
      g_meshBatchCache.size(),g_meshBatchBytes,g_meshFrameHits,g_meshFrameCache.size(),g_meshFrameBytes,g_meshFragmentFlushes,g_meshFragmentRanges,g_meshPlacedPromoted,g_meshImmVerts,g_meshBatchUploadBytes);
    printf("HUDUPLOAD|uploads=%lu|skips=%lu|bytes=%lu\n",g_h6Uploads,g_h6UploadSkips,g_h6UploadBytes);
    hud_compose_dump();
    printf("MESHCACHE|misses=%lu|evictions=%lu|lru=%zu|admission_refused=%llu\n", g_meshBatchMisses, g_meshBatchEvictions, g_meshBatchLRU.size(),g_meshBatchAdmissionRefused);
    printf("MESHPOOL|active_stores=%zu|reserved_stores=%zu|reserved_bytes=%zu|fallbacks=%lu|trimmed=%lu\n",
      g_meshFrameNext,g_meshFrameStores.size(),g_meshFrameBytes,g_meshFrameFallbacks,g_meshFrameTrimmed);
    printf("MESHGEOMETRY|hits=%lu|vertices=%lu|uploads=%lu|poses=%zu|cpu_bytes=%zu"
      "|cap_bytes=%zu|requests=%llu|misses=%llu|refused=%llu|allocation_failures=%llu"
      "|materialized=%llu|released=%llu|lazy=%d|mode=%s|pressure=%d|pressure_activations=%llu|active_cap_bytes=%zu\n",
      g_meshGeometryHits,g_meshGeometryBuilds,g_meshGeometryUploads,g_meshGeometryKeys.size(),g_meshGeometryBytes,
      mesh_geometry_budget(),g_meshGeometryRequests,g_meshGeometryMisses,g_meshGeometryRefused,
      g_meshGeometryAllocationFailures,g_meshGeometryMaterialized,g_meshGeometryReleased,
      mesh_geometry_policy().lazy()?1:0,
      mesh_geometry_policy().mode==MESH_POSE_AUTO?"auto":mesh_geometry_policy().mode==MESH_POSE_EAGER?"eager":"lazy",
      mesh_geometry_policy().pressure?1:0,mesh_geometry_policy().activations,mesh_geometry_policy().cap());
    printf("MESHWINDDIRECT|draws=%llu|vertices=%llu|fallbacks=%llu|verified=%llu|mismatches=%llu|scratch_bytes=%zu\n",
      g_meshWindDirectDraws,g_meshWindDirectVertices,g_meshWindDirectFallbacks,
      g_meshWindDirectVerified,g_meshWindDirectMismatches,g_meshGeometryUpload.capacity()*sizeof(float));
    printf("MESHWINDDEPTH|draws=%llu|vertices=%llu|fallbacks=%llu|verified=%llu|mismatches=%llu\n",
      g_meshWindDepthDraws,g_meshWindDepthVertices,g_meshWindDepthFallbacks,
      g_meshWindDepthVerified,g_meshWindDepthMismatches);
    printf("MESHBAKESHARE|draws=%llu|vertices=%llu|stores=%llu|verified=%llu|mismatches=%llu\n",
      g_meshBakeShareDraws,g_meshBakeShareVertices,g_meshBakeShareStores,
      g_meshBakeShareVerified,g_meshBakeShareMismatches);
    mesh_wind_gpu_dump();
    printf("GLSHADOW|program|binds_skipped=%lu|queries_skipped=%lu|mismatches=%lu\n",
      g_glsProgramBindsSkipped,g_glsProgramQueriesSkipped,g_glsProgramMismatches);
    printf("MESHSETTLED|hits=%lu|entries=%zu|bytes=%zu|refused=%llu\n",g_meshSettledHits,g_meshSettled.size(),g_meshSettledBytes,g_meshSettledRefused);
    printf("FRAGMENTPREP|hits=%llu|builds=%llu|vertices=%llu|entries=%zu|bytes=%zu|draws_saved=%llu|packed_bytes=%llu|pack_scratch=%zu|pack_fallbacks=%llu|source_fallbacks=%llu|upload_bytes=%llu|full_bytes=%llu\n",g_meshPreparedHits,g_meshPreparedBuilds,g_meshPreparedVertices,g_meshPrepared.size(),g_meshPreparedBytes,g_meshFragmentDrawsSaved,g_meshFragmentPackedBytes,g_meshFragmentPacked.capacity(),g_meshFragmentPackFallbacks,g_meshPreparedFallbacks,g_meshFragmentUploadBytes,g_meshFragmentFullBytes);
    printf("FRAGMENTREUSE|inputs=%lu|streams=%lu|entries=%zu|bytes=%zu\n",g_meshFragmentInputHits,g_meshFragmentStreamHits,g_meshFragmentStores.size(),g_meshFragmentStoreBytes);
    printf("FRAGMENTREST|mode=%d|skips=%llu|tries=%llu|warm=%llu|missed=%llu|store_inputs=%llu|collected_bytes=%llu|still=%llu\n",
      mesh_fragment_rest_mode(),g_meshFragmentRestSkips,g_meshFragmentRestTries,g_meshFragmentRestWarm,
      g_meshFragmentRestMissed,g_meshFragmentStoreInputs,g_meshFragmentCollectedBytes,
      g_meshFragmentRestStill);
    printf("MESHGLOW|hits=%lu|builds=%lu|recolours=%lu|refused=%lu|entries=%zu|bytes=%zu\n",
      g_meshGlowHits,g_meshGlowBuilds,g_meshGlowRecolours,g_meshGlowRefused,g_meshGlowCache.size(),g_meshGlowBytes);
}

/* Script-only cap stress: empty ranges obey the entry bound, hot admission must
   not churn the current frame, and cold entries remain replaceable. */
static bool mesh_batch_selftest() {
    mesh_batch_clear();
    for(unsigned i=0;i<9000;i++) {MeshKey k;mesh_batch_key(k,i);mesh_batch_keep(k,MeshBatchEntry());}
    MeshKey first,last;const unsigned a=0,b=8999;mesh_batch_key(first,a);mesh_batch_key(last,b);
    const bool protect=mesh_perf_switch("OPENCNC3D_MESH_HOT_ADMISSION");
    bool ok=g_meshBatchCache.size()==MESH_BATCH_MAX_ENTRIES && g_meshBatchLRU.size()==MESH_BATCH_MAX_ENTRIES
        && !!mesh_batch_kept_find(first)==protect && !!mesh_batch_kept_find(last)!=protect && g_meshBatchBytes==0;
    const unsigned serial=g_drawFrameSerial;g_drawFrameSerial+=3;
    if(protect) {
        mesh_batch_keep(last,MeshBatchEntry());
        ok=ok && mesh_batch_kept_find(last) && g_meshBatchCache.size()==MESH_BATCH_MAX_ENTRIES;
    }
    g_drawFrameSerial=serial;
    printf("MESHCAPTEST|%s|entries=%zu|bytes=%zu|evictions=%lu\n",ok?"ok":"failed",g_meshBatchCache.size(),g_meshBatchBytes,g_meshBatchEvictions);
    mesh_batch_clear();return ok;
}
