/* ---------------------------------------------------------------------------------- *
 *  THE GROUND BUILDS' MEMORY: how much a build holds at its worst, two restore
 *  switches, and the mission's refused Enhanced ground.
 *
 *  Red Alert's ground sheet (ra3d_terrain.h ra3d_ground_ensure), the grass mask
 *  (terrain_grass_mask.h) and the ground material (terrain_surface.h) are the largest
 *  one-time builds of a mission's first frame: the Red Alert snow sheet alone is a
 *  6240 x 6084 RGBA picture, 145 MB, laid out on the CPU twice. What each holds at once
 *  decides whether the first frame fits on a machine that is short of memory, so each
 *  build measures it:
 *
 *    GroundMemPeak peak("ground_cpu_peak");   at the top of the build
 *    ground_mem_note();                        after each large allocation inside it
 *
 *  The scope reads the process's memory at its start and at every note, and on the way
 *  out (an exception included) raises the ledger key (memledger.h memledger_peak) to the
 *  highest amount held above its start. Scopes nest: a note inside the grass mask's
 *  build, which Red Alert's ground build calls, counts for both. What is read:
 *
 *    macOS     the bytes in use in every malloc zone (malloc_zone_statistics): the
 *              build's own vectors, measured, not added up from their sizes. A large
 *              block malloc hands back from its cache of freed blocks counts at its
 *              full size, so a reading can exceed what the build asked for by up to a
 *              recently freed block (measured: the ground material's 72.7 MB atlas read
 *              as 91.2 MB in a recycled 90.8 MB block). MallocLargeCache=0 turns that
 *              cache off, and the reading is then the build's own requests;
 *    Windows   the process's commit (PrivateUsage), which there also carries the GL
 *              driver's charge for the textures created inside the build;
 *    others    nothing: the keys are not written.
 *
 *  A note costs one statistics call while a scope is open and one empty-vector test
 *  otherwise. Nothing here changes what is drawn.
 *
 *  THE RESTORE SWITCHES, each read once, for an A/B against the way these builds worked
 *  before they were made to hold less:
 *
 *    OPENCNC3D_RA_SHEET_LEGACY=1   Red Alert's sheet specified twice, its CPU copy and the
 *                                  terrain pack held to the end of the build, and a
 *                                  mid-mission rebuild (a bridge blown, a save loaded)
 *                                  made on new texture names instead of the kept ones
 *    OPENCNC3D_PACK_WHOLE_READ=1   the grass mask and the ground material read whole into
 *                                  memory (and the material decoded whole) before their
 *                                  atlases are laid out, instead of slot by slot
 *
 *  THE REFUSED GROUND. When Red Alert's Enhanced ground cannot be built for want of memory
 *  (a std::bad_alloc inside the build), the mission draws the 1995 tiles instead, and that
 *  choice is kept for the rest of the mission (ra3d_set_wanted honours it), so the build
 *  is not tried again on every frame. A build refused in the middle of a mission (a
 *  bridge blown, a save loaded, a switch to Enhanced) also lets go of what was laid on
 *  the Enhanced sheet before it, so the mission draws as one refused from its first
 *  frame. game_shutdown forgets the refusal. Failure injection sites (memledger.h):
 *  ra3d_ground (the sheet), ra3d_dry (its dry copy, with the main sheet made),
 *  ra3d_seams (each strip sheet), grassmask, material.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_GROUND_MEM_H
#define OPENCNC3D_GROUND_MEM_H
#ifdef __APPLE__
#include <malloc/malloc.h>
#endif

static bool ground_mem_switch(const char* name)
{
    const char* v = getenv(name);
    return v && *v && strcmp(v, "0") != 0;
}
static bool ra3d_sheet_legacy(void)
{
    static const bool on = ground_mem_switch("OPENCNC3D_RA_SHEET_LEGACY");
    return on;
}
static bool ground_pack_whole_read(void)
{
    static const bool on = ground_mem_switch("OPENCNC3D_PACK_WHOLE_READ");
    return on;
}

/* The process's memory as this platform measures it (see above), or -1 for none. */
static long long ground_mem_now(void)
{
#if defined(__APPLE__)
    malloc_statistics_t st;
    memset(&st, 0, sizeof st);
    malloc_zone_statistics(NULL, &st);
    return (long long)st.size_in_use;
#elif defined(_WIN32)
    typedef BOOL (WINAPI *ProcMem)(HANDLE, PROCESS_MEMORY_COUNTERS*, DWORD);
    static ProcMem procMem = NULL;
    static bool looked = false;
    if (!looked) {
        looked = true;
        const HMODULE k32 = GetModuleHandleA("kernel32.dll");
        procMem = k32 ? (ProcMem)(void*)GetProcAddress(k32, "K32GetProcessMemoryInfo") : NULL;
    }
    PROCESS_MEMORY_COUNTERS_EX pm;
    memset(&pm, 0, sizeof pm);
    pm.cb = sizeof pm;
    if (!procMem || !procMem(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pm, sizeof pm)) return -1;
    return (long long)pm.PrivateUsage;
#else
    return -1;
#endif
}

struct GroundMemScope { const char* key; long long start, peak; };
static std::vector<GroundMemScope*>& ground_mem_scopes(void)
{
    static std::vector<GroundMemScope*> open;
    return open;
}
/* After a large allocation inside a measured build: one reading, every open scope's
   high-water mark raised to it. */
static void ground_mem_note(void)
{
    std::vector<GroundMemScope*>& open = ground_mem_scopes();
    if (open.empty()) return;
    const long long now = ground_mem_now();
    if (now < 0) return;
    for (size_t i = 0; i < open.size(); i++)
        if (now - open[i]->start > open[i]->peak) open[i]->peak = now - open[i]->start;
}
struct GroundMemPeak {
    GroundMemScope scope;
    explicit GroundMemPeak(const char* key)
    {
        scope.key = key;
        scope.peak = 0;
        scope.start = ground_mem_now();
        if (scope.start >= 0) ground_mem_scopes().push_back(&scope);
    }
    ~GroundMemPeak()
    {
        if (scope.start < 0) return;
        ground_mem_note();
        std::vector<GroundMemScope*>& open = ground_mem_scopes();
        for (size_t i = open.size(); i-- > 0;)
            if (open[i] == &scope) { open.erase(open.begin() + (long)i); break; }
        memledger_peak(scope.key, scope.peak);
    }
private:
    GroundMemPeak(const GroundMemPeak&);
    GroundMemPeak& operator=(const GroundMemPeak&);
};

/* The scenario whose Enhanced Red Alert ground was refused for want of memory (empty
   for none): that mission draws the 1995 tiles to its end. */
static std::string g_ra3dRefusedScene;

#endif
