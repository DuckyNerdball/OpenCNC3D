/* ---------------------------------------------------------------------------------- *
 *  HOW MUCH MEMORY THE GAME MAY STILL ASK FOR, measured in one place.
 *
 *  Every test that decides whether optional work fits (the building preparation, the soft
 *  water) used to read the machine on its own. This header is the one reading they share,
 *  and the one admission helper they call before a large allocation.
 *
 *  THE ROOM. On Windows it is the smaller of two numbers:
 *
 *    job room     the memory cap of a job object the process runs in, minus what this
 *                 process has committed (PrivateUsage). A capped process can have no
 *                 commit left while the machine has gigabytes free, and the machine's
 *                 numbers do not show the cap.
 *    system room  the machine's free commit (commit limit minus commit total) plus what
 *                 the page file can still grow by. A page file Windows manages, or one
 *                 given a maximum above its current size, grows by itself when commit
 *                 runs short, so its free commit alone understates the room (measured:
 *                 a commit limit of about 24 GB grew to 29 GB during one Red Alert
 *                 mission, and shrank back after it). The growth is the page file's
 *                 maximum minus its current size, capped by its volume's free space minus
 *                 2 GB; it is 0 for a page file of fixed size, for no page file, and
 *                 whenever it cannot be read.
 *
 *  OPTIONAL WORK SEES HALF THE GROWTH. Work the current picture does not need (the building
 *  preparation's other damage stages and cut meshes) is tested against the optional room:
 *  the free commit plus only half of what the page file can still grow by, and never more
 *  than the job room. It is not known yet whether an allocation that makes the page file
 *  grow succeeds at once, and work that can wait should not be what finds out. Required
 *  work sees the whole room. With a page file of fixed size the two are the same, and both
 *  are at least what was tested before the growth was counted at all.
 *
 *  Off Windows the room is unlimited: there the system grows its swap by itself and no
 *  admission test ran before this header existed, so none runs now.
 *
 *  THE COST OF A READING. The cheap calls only: GlobalMemoryStatusEx, the job object query
 *  and K32GetProcessMemoryInfo. The page file's size and maximum are read once (the first
 *  reading), and later growth is taken from the change in the commit limit, which moves
 *  only when a page file does. Its volume's free space is read at most once a second. A
 *  reading is kept for the rest of a frame unless something has been admitted since (an
 *  admitted allocation is exactly what changes it) or it is more than 16 ms old (a long
 *  load draws no frames). The MEM|budget line at each mission start gives the measured
 *  time of one reading (probe_us) and of the volume query (disk_us).
 *
 *  ADMISSION. lowmem_fits(need, site) asks whether a piece of work fits; lowmem_charge(need)
 *  records it once its memory is taken; lowmem_admit does both for a single allocation.
 *  A need is given by kind, and each kind is charged what a 64-bit Windows graphics driver
 *  was measured to charge the process per byte: textures 1.05, render targets 1.02,
 *  buffers 2.1, system memory 1.0. Physical memory is tested only against the system-memory and
 *  transient bytes, never against the driver's charge, which is a reservation, not memory
 *  in use. A refusal prints one LOWMEM|refused line per site per mission and is counted.
 *
 *  THE TEST SWITCHES. OPENCNC3D_TEST_COMMIT_ROOM_MB=N (read once, every platform, for tests
 *  only) makes the free commit N MB at each mission start, falling by every admitted byte
 *  and by nothing else, so the paths a short machine takes run the same way on every
 *  machine and in every run. OPENCNC3D_TEST_PAGEFILE_GROWTH_MB=G, only beside it, adds a
 *  page file that can grow by G MB: admitted bytes take the free commit first and then the
 *  growth, as a real page file's do, so the room is N + G and the optional room is the free
 *  commit plus half the growth left. On macOS they test the logic only: what the driver
 *  really charges, and whether a page file grows in time, are Windows measurements.
 *
 *  The memory ledger (memledger.h) carries lowmem_room, lowmem_room_optional,
 *  lowmem_charged (admitted this mission) and lowmem_refusals.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_LOWMEM_H
#define OPENCNC3D_LOWMEM_H

#ifdef _WIN32
#include <psapi.h>
#endif

static const unsigned long long LOWMEM_UNLIMITED = ~0ull;
/* what required work keeps free: the driver holds about 256 MB of freed memory for a few
   seconds before it returns, and the building preparation always kept this much */
static const unsigned long long LOWMEM_RESERVE_REQUIRED = 256ull << 20;

enum LowmemKind { LOWMEM_TEXTURE = 0, LOWMEM_TARGET = 1, LOWMEM_BUFFER = 2, LOWMEM_HEAP = 3 };

/* bytes of each kind a piece of work allocates and keeps, plus what it holds only while
   it runs (transient: tested against the room, never charged) */
struct LowmemNeed {
    unsigned long long texture, target, buffer, heap, transient;
    LowmemNeed() : texture(0), target(0), buffer(0), heap(0), transient(0) {}
};

struct LowmemReading {
    bool ok;                          /* false: the system could not be read (Windows) */
    unsigned long long room;          /* LOWMEM_UNLIMITED off Windows without the switch */
    unsigned long long jobRoom, sysRoom, commitFree, commitLimit, physFree, cap, privateUsage, growable;
    const char* source;               /* "job", "system", "test", "unlimited", "unknown" */
};

/* g_drawFrameSerial, which is defined further down cnc_eyes.cpp */
static unsigned lowmem_frame_serial(void);

static unsigned long long lowmem_sat_add(unsigned long long a, unsigned long long b)
{
    return a > LOWMEM_UNLIMITED - b ? LOWMEM_UNLIMITED : a + b;
}
/* bytes x num / den, rounded up, saturating */
static unsigned long long lowmem_scaled(unsigned long long bytes, unsigned long long num, unsigned long long den)
{
    if (bytes > LOWMEM_UNLIMITED / num) return LOWMEM_UNLIMITED;
    return (bytes * num + den - 1) / den;
}
/* what the work is charged against the room: each kind times its measured factor */
static unsigned long long lowmem_charge_of(const LowmemNeed& n)
{
    unsigned long long c = lowmem_scaled(n.texture, 105, 100);
    c = lowmem_sat_add(c, lowmem_scaled(n.target, 102, 100));
    c = lowmem_sat_add(c, lowmem_scaled(n.buffer, 21, 10));
    return lowmem_sat_add(c, n.heap);
}

struct LowmemState {
    int testRead;                     /* the switch has been read */
    bool testOn;
    unsigned long long testRoom;      /* bytes of free commit, at each mission start */
    unsigned long long testGrowth;    /* bytes the test's page file can grow by, at each mission start */
    unsigned long long charged;       /* admitted since the mission began */
    unsigned long long refusals;
    std::set<std::string> refusedSites;
    bool have;                        /* a reading is kept */
    unsigned readFrame;
    Uint64 readAt;
    unsigned long long chargedAtRead;
    LowmemReading r;
    double probeUs, diskUs, initUs;
};
static LowmemState& lowmem_state(void)
{
    static LowmemState s;
    static bool init = false;
    if (!init) {
        init = true;
        s.testRead = 0; s.testOn = false; s.testRoom = 0; s.testGrowth = 0; s.charged = 0; s.refusals = 0;
        s.have = false; s.readFrame = 0; s.readAt = 0; s.chargedAtRead = 0;
        memset(&s.r, 0, sizeof s.r);
        s.r.source = "unknown";
        s.probeUs = s.diskUs = s.initUs = -1.0;
    }
    return s;
}
static bool lowmem_test_on(void)
{
    LowmemState& s = lowmem_state();
    if (!s.testRead) {
        s.testRead = 1;
        const char* v = getenv("OPENCNC3D_TEST_COMMIT_ROOM_MB");
        if (v && *v) {
            char* end = NULL;
            const unsigned long long mb = strtoull(v, &end, 10);
            if (end && *end == 0 && mb <= (LOWMEM_UNLIMITED >> 21)) {
                s.testOn = true;
                s.testRoom = mb << 20;
            } else {
                fprintf(stderr, "LOWMEM|test-switch-ignored|OPENCNC3D_TEST_COMMIT_ROOM_MB=%s"
                                " (want a whole number of MB)\n", v);
            }
        }
        const char* g = getenv("OPENCNC3D_TEST_PAGEFILE_GROWTH_MB");
        if (g && *g) {
            char* end = NULL;
            const unsigned long long mb = strtoull(g, &end, 10);
            if (s.testOn && end && *end == 0 && mb <= (LOWMEM_UNLIMITED >> 21)) {
                s.testGrowth = mb << 20;
            } else {
                fprintf(stderr, "LOWMEM|test-switch-ignored|OPENCNC3D_TEST_PAGEFILE_GROWTH_MB=%s"
                                " (want a whole number of MB, beside OPENCNC3D_TEST_COMMIT_ROOM_MB)\n", g);
            }
        }
    }
    return s.testOn;
}
/* Under the test switches: what is left of the free commit and of the page file's growth
   once the admitted bytes have taken the free commit first and then the growth. */
static void lowmem_test_left(unsigned long long* commitFree, unsigned long long* growable)
{
    const LowmemState& s = lowmem_state();
    const unsigned long long over = s.charged > s.testRoom ? s.charged - s.testRoom : 0;
    *commitFree = s.testRoom > s.charged ? s.testRoom - s.charged : 0;
    *growable = s.testGrowth > over ? s.testGrowth - over : 0;
}
/* whether anything is ever refused: always on Windows, elsewhere only under the switch */
static bool lowmem_limited(void)
{
#ifdef _WIN32
    return true;
#else
    return lowmem_test_on();
#endif
}

#ifdef _WIN32
/* THE PAGE FILE, read once. NtQuerySystemInformation's page-file class with sizes
   (SystemPageFileInformationEx, 144) gives each page file's current size and its maximum
   in pages; the plain class (18) gives only the size, and then the growth is unknown and
   counted as 0. Laid out as the system returns it on x64. */
struct LowmemPagefileEx {
    ULONG NextEntryOffset, TotalSize, TotalInUse, PeakUsage;
    USHORT NameLength, NameMaximumLength;
    PWSTR NameBuffer;
    ULONG MinimumSize, MaximumSize;
};
struct LowmemPagefile {
    bool tried, known;
    int files;
    unsigned long long size0, max, limit0;   /* bytes; limit0 = the commit limit when read */
    int volumes;
    wchar_t root[4][4];                      /* "C:\" of each volume holding a page file */
    unsigned long long diskRoom;             /* free space minus 2 GB, over those volumes */
    unsigned long long diskFree;             /* their free space */
    bool diskKnown;
    ULONGLONG diskAt;
};
static LowmemPagefile& lowmem_pagefile(void)
{
    static LowmemPagefile p;
    static bool init = false;
    if (!init) { init = true; memset(&p, 0, sizeof p); }
    return p;
}
static double lowmem_us_since(LARGE_INTEGER t0)
{
    LARGE_INTEGER t1, f;
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&f);
    return f.QuadPart ? (double)(t1.QuadPart - t0.QuadPart) * 1e6 / (double)f.QuadPart : -1.0;
}
static void lowmem_pagefile_read(unsigned long long commitLimit)
{
    LowmemPagefile& p = lowmem_pagefile();
    if (p.tried) return;
    p.tried = true;
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    p.limit0 = commitLimit;
    typedef LONG (WINAPI *Nqsi)(ULONG, PVOID, ULONG, PULONG);
    const HMODULE nt = GetModuleHandleA("ntdll.dll");
    const Nqsi query = nt ? (Nqsi)(void*)GetProcAddress(nt, "NtQuerySystemInformation") : NULL;
    SYSTEM_INFO si;
    memset(&si, 0, sizeof si);
    GetSystemInfo(&si);
    const unsigned long long page = si.dwPageSize ? si.dwPageSize : 4096;
    static unsigned long long buf[1024];                   /* 8 KB, 8-byte aligned */
    memset(buf, 0, sizeof buf);
    ULONG got = 0;
    bool read = false, ex = false;
    if (query) {
        if (query(144, buf, sizeof buf, &got) == 0) read = ex = true;
        else if (query(18, buf, sizeof buf, &got) == 0) read = true;
    }
    if (!read) { lowmem_state().initUs = lowmem_us_since(t0); return; }
    const unsigned char* at = (const unsigned char*)buf;
    const unsigned char* end = at + sizeof buf;
    for (int guard = 0; guard < 16 && at + sizeof(LowmemPagefileEx) <= end; guard++) {
        const LowmemPagefileEx* e = (const LowmemPagefileEx*)at;
        if (!e->TotalSize && !e->NextEntryOffset && !guard) break;   /* no page file at all */
        p.files++;
        p.size0 += (unsigned long long)e->TotalSize * page;
        if (ex) p.max += (unsigned long long)e->MaximumSize * page;
        /* the volume: the first "X:" in a name such as \??\C:\pagefile.sys */
        const wchar_t* name = e->NameBuffer;
        const size_t len = e->NameLength / sizeof(wchar_t);
        for (size_t k = 0; name && k + 1 < len; k++) {
            const wchar_t c = name[k];
            if (name[k + 1] == L':' && ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z'))) {
                bool seen = false;
                for (int v = 0; v < p.volumes; v++)
                    if ((p.root[v][0] | 0x20) == (c | 0x20)) seen = true;
                if (!seen && p.volumes < 4) {
                    p.root[p.volumes][0] = c; p.root[p.volumes][1] = L':';
                    p.root[p.volumes][2] = L'\\'; p.root[p.volumes][3] = 0;
                    p.volumes++;
                }
                break;
            }
        }
        if (!e->NextEntryOffset) break;
        at += e->NextEntryOffset;
    }
    p.known = ex && p.files > 0;
    lowmem_state().initUs = lowmem_us_since(t0);
}
/* free space on the page files' volumes, minus 2 GB each; read at most once a second */
static unsigned long long lowmem_disk_room(void)
{
    LowmemPagefile& p = lowmem_pagefile();
    const ULONGLONG now = GetTickCount64();
    if (p.diskKnown && now - p.diskAt < 1000) return p.diskRoom;
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    unsigned long long room = 0, freeSum = 0;
    bool any = false;
    for (int v = 0; v < p.volumes; v++) {
        ULARGE_INTEGER avail, total, freeAll;
        if (!GetDiskFreeSpaceExW(p.root[v], &avail, &total, &freeAll)) continue;
        any = true;
        const unsigned long long keep = 2ull << 30;
        freeSum = lowmem_sat_add(freeSum, freeAll.QuadPart);
        if (freeAll.QuadPart > keep) room = lowmem_sat_add(room, freeAll.QuadPart - keep);
    }
    p.diskRoom = any ? room : 0;
    p.diskFree = any ? freeSum : 0;
    p.diskKnown = true;
    p.diskAt = now;
    lowmem_state().diskUs = lowmem_us_since(t0);
    return p.diskRoom;
}
/* what the page files can still grow by, given today's commit limit */
static unsigned long long lowmem_growable(unsigned long long commitLimit)
{
    lowmem_pagefile_read(commitLimit);
    const LowmemPagefile& p = lowmem_pagefile();
    if (!p.known) return 0;
    const long long grown = (long long)commitLimit - (long long)p.limit0;
    const long long sizeNow = (long long)p.size0 + grown;
    const long long left = (long long)p.max - sizeNow;
    if (left <= 0) return 0;
    const unsigned long long disk = lowmem_disk_room();
    return (unsigned long long)left < disk ? (unsigned long long)left : disk;
}
#endif

/* A fresh reading of the room, into the kept one. */
static void lowmem_take_reading(void)
{
    LowmemState& s = lowmem_state();
    LowmemReading r;
    memset(&r, 0, sizeof r);
    r.ok = true;
    r.room = r.jobRoom = r.sysRoom = r.physFree = LOWMEM_UNLIMITED;
    r.source = "unlimited";
#ifdef _WIN32
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    MEMORYSTATUSEX ms;
    memset(&ms, 0, sizeof ms);
    ms.dwLength = sizeof ms;
    if (!GlobalMemoryStatusEx(&ms)) {
        r.ok = false;
        r.room = r.sysRoom = r.physFree = 0;
        r.source = "unknown";
    } else {
        r.commitFree = (unsigned long long)ms.ullAvailPageFile;
        r.commitLimit = (unsigned long long)ms.ullTotalPageFile;
        r.physFree = (unsigned long long)ms.ullAvailPhys;
        r.growable = lowmem_growable(r.commitLimit);
        r.sysRoom = lowmem_sat_add(r.commitFree, r.growable);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION job;
        memset(&job, 0, sizeof job);
        if (QueryInformationJobObject(NULL, JobObjectExtendedLimitInformation, &job, sizeof job, NULL)) {
            if (job.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_PROCESS_MEMORY)
                r.cap = (unsigned long long)job.ProcessMemoryLimit;
            if ((job.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_JOB_MEMORY)
                && (!r.cap || (unsigned long long)job.JobMemoryLimit < r.cap))
                r.cap = (unsigned long long)job.JobMemoryLimit;
        }
        /* (K32GetProcessMemoryInfo is kernel32's own export, so no psapi.dll is needed) */
        typedef BOOL (WINAPI *ProcMem)(HANDLE, PROCESS_MEMORY_COUNTERS*, DWORD);
        PROCESS_MEMORY_COUNTERS_EX pm;
        memset(&pm, 0, sizeof pm);
        pm.cb = sizeof pm;
        const HMODULE k32 = GetModuleHandleA("kernel32.dll");
        const ProcMem procMem = k32 ? (ProcMem)(void*)GetProcAddress(k32, "K32GetProcessMemoryInfo") : NULL;
        if (procMem && procMem(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pm, sizeof pm)) {
            r.privateUsage = (unsigned long long)pm.PrivateUsage;
            if (r.cap) r.jobRoom = r.privateUsage < r.cap ? r.cap - r.privateUsage : 0;
        }
        r.room = r.jobRoom < r.sysRoom ? r.jobRoom : r.sysRoom;
        r.source = r.jobRoom < r.sysRoom ? "job" : "system";
    }
    s.probeUs = lowmem_us_since(t0);
#endif
    if (lowmem_test_on()) {
        /* the test's free commit and growth replace the machine's; no job cap applies */
        r.ok = true;
        lowmem_test_left(&r.commitFree, &r.growable);
        r.room = r.sysRoom = lowmem_sat_add(r.commitFree, r.growable);
        r.jobRoom = LOWMEM_UNLIMITED;
        r.cap = 0;
        r.physFree = LOWMEM_UNLIMITED;
        r.source = "test";
    }
    s.r = r;
    s.have = true;
    s.readFrame = lowmem_frame_serial();
    s.readAt = SDL_GetPerformanceCounter();
    s.chargedAtRead = s.charged;
}
/* The room now. fresh = read the system again whatever is kept. */
static const LowmemReading& lowmem_reading(bool fresh)
{
    LowmemState& s = lowmem_state();
    if (!fresh && s.have && s.readFrame == lowmem_frame_serial() && s.chargedAtRead == s.charged) {
        const Uint64 freq = SDL_GetPerformanceFrequency();
        if (freq && (SDL_GetPerformanceCounter() - s.readAt) * 1000 < 16 * freq) return s.r;
    }
    lowmem_take_reading();
    return s.r;
}
static unsigned long long lowmem_room(void) { return lowmem_reading(false).room; }
/* The optional room of a reading: its free commit plus half the page file's growth, never
   more than its job room. Unlimited where the room is. */
static unsigned long long lowmem_optional_of(const LowmemReading& r)
{
    if (r.room == LOWMEM_UNLIMITED || !r.ok) return r.room;
    const unsigned long long sys = lowmem_sat_add(r.commitFree, r.growable / 2);
    const unsigned long long room = r.jobRoom < sys ? r.jobRoom : sys;
    return room < r.room ? room : r.room;
}
/* The room for work that can wait (see OPTIONAL WORK SEES HALF THE GROWTH above), for a
   caller that wants the number; an admission test passes optional to lowmem_fits instead. */
static unsigned long long lowmem_room_optional(void) { return lowmem_optional_of(lowmem_reading(false)); }

/* The ledger's rooms, from the kept reading or, under the test switches, from what the
   admitted bytes have left (nothing is read here) */
static void lowmem_ledger_room(void)
{
    const LowmemState& s = lowmem_state();
    if (!lowmem_limited()) {
        memledger_text("lowmem_room", "unlimited");
        memledger_text("lowmem_room_optional", "unlimited");
    } else if (s.testOn) {
        unsigned long long commitFree = 0, growable = 0;
        lowmem_test_left(&commitFree, &growable);
        memledger_bytes("lowmem_room", (long long)lowmem_sat_add(commitFree, growable));
        memledger_bytes("lowmem_room_optional", (long long)lowmem_sat_add(commitFree, growable / 2));
    } else {
        memledger_bytes("lowmem_room", (long long)s.r.room);
        memledger_bytes("lowmem_room_optional", (long long)lowmem_optional_of(s.r));
    }
}

/* Whether the work fits, kept `reserve` bytes beyond it: required work against the whole
   room, optional work against the optional room. Refusals are counted and the first at
   each site in a mission prints its line. Off Windows without the switch, always yes. */
static bool lowmem_fits(const LowmemNeed& n, const char* site,
                        unsigned long long reserve = LOWMEM_RESERVE_REQUIRED, bool optional = false)
{
    if (!lowmem_limited()) return true;
    const LowmemReading& r = lowmem_reading(false);
    const unsigned long long room = optional ? lowmem_optional_of(r) : r.room;
    const unsigned long long commit = lowmem_sat_add(lowmem_sat_add(lowmem_charge_of(n), n.transient), reserve);
    const unsigned long long phys = lowmem_sat_add(lowmem_sat_add(n.heap, n.transient), reserve);
    if (r.ok && room >= commit && r.physFree >= phys) return true;
    LowmemState& s = lowmem_state();
    s.refusals++;
    memledger_count("lowmem_refusals", (long long)s.refusals);
    const std::string key = site ? site : "?";
    if (!s.refusedSites.count(key)) {
        s.refusedSites.insert(key);
        char physText[32];
        if (r.physFree == LOWMEM_UNLIMITED) snprintf(physText, sizeof physText, "unlimited");
        else snprintf(physText, sizeof physText, "%llu", r.physFree >> 20);
        fprintf(stderr, "LOWMEM|refused|site=%s|work=%s|need=%llu|room=%llu|phys_need=%llu|phys=%s|source=%s"
                        "|first of this mission at this site\n",
                key.c_str(), optional ? "optional" : "required", (commit + (1ull << 20) - 1) >> 20,
                room >> 20, (phys + (1ull << 20) - 1) >> 20, physText, r.source);
    }
    return false;
}
/* The work was done: its bytes are taken from the room. */
static void lowmem_charge(const LowmemNeed& n)
{
    LowmemState& s = lowmem_state();
    s.charged = lowmem_sat_add(s.charged, lowmem_charge_of(n));
    memledger_bytes("lowmem_charged", (long long)s.charged);
    lowmem_ledger_room();
}
/* One allocation of one kind: fits, and charged when it does. */
static bool lowmem_admit(unsigned long long bytes, int kind, const char* site,
                         unsigned long long transient = 0)
{
    LowmemNeed n;
    if (kind == LOWMEM_TEXTURE) n.texture = bytes;
    else if (kind == LOWMEM_TARGET) n.target = bytes;
    else if (kind == LOWMEM_BUFFER) n.buffer = bytes;
    else n.heap = bytes;
    n.transient = transient;
    if (!lowmem_fits(n, site)) return false;
    lowmem_charge(n);
    return true;
}

/* At each mission start: the switch's room starts again (the last mission's work went with
   it), the refusal lines may print once more, and MEM|budget says what the room is. */
static void lowmem_mission_begin(void)
{
    LowmemState& s = lowmem_state();
    s.charged = 0;
    s.refusals = 0;
    s.refusedSites.clear();
    lowmem_reading(true);
    memledger_bytes("lowmem_charged", 0);
    memledger_count("lowmem_refusals", 0);
    lowmem_ledger_room();
#ifdef _WIN32
    /* a second reading, with the page file and its volume already read, is the cost of one;
       the line gives that reading throughout */
    const double firstUs = s.probeUs;
    lowmem_take_reading();
#endif
    const LowmemReading& r = s.r;
    char room[32], optional[32];
    if (r.room == LOWMEM_UNLIMITED) snprintf(room, sizeof room, "unlimited");
    else snprintf(room, sizeof room, "%llu", r.room >> 20);
    const unsigned long long roomOptional = lowmem_optional_of(r);
    if (roomOptional == LOWMEM_UNLIMITED) snprintf(optional, sizeof optional, "unlimited");
    else snprintf(optional, sizeof optional, "%llu", roomOptional >> 20);
#ifdef _WIN32
    const LowmemPagefile& p = lowmem_pagefile();
    char pfMax[32], disk[32];
    if (p.known) snprintf(pfMax, sizeof pfMax, "%llu", p.max >> 20);
    else snprintf(pfMax, sizeof pfMax, "unknown");
    if (p.diskKnown) snprintf(disk, sizeof disk, "%llu", p.diskFree >> 20);
    else snprintf(disk, sizeof disk, "unknown");
    printf("MEM|budget|room=%s|cap=%llu|growable=%llu|source=%s|room_optional=%s|commit_free=%llu"
           "|commit_limit=%llu|phys_free=%llu|private=%llu|pagefiles=%d|pagefile=%llu|pagefile_max=%s"
           "|disk_free=%s|probe_us=%.1f|first_us=%.1f|disk_us=%.1f|pagefile_us=%.1f\n",
           room, r.cap >> 20, r.growable >> 20, r.source, optional,
           r.commitFree >> 20, r.commitLimit >> 20, r.physFree >> 20, r.privateUsage >> 20,
           p.files, p.size0 >> 20, pfMax, disk, s.probeUs, firstUs, s.diskUs, s.initUs);
#else
    printf("MEM|budget|room=%s|cap=0|growable=%llu|source=%s|room_optional=%s\n",
           room, r.growable >> 20, r.source, optional);
#endif
    fflush(stdout);
}

/* The page-file growth of the last reading, for the FATAL line (reads nothing new). */
static unsigned long long lowmem_last_growable(void)
{
    const LowmemState& s = lowmem_state();
    return s.have ? s.r.growable : 0;
}

#endif
