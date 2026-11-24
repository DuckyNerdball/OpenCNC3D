/* ====================================================================================
 *  RALLY: THE EYES OF THE RACE MODE
 *
 *  Rally is a kart race on a course map with the game's own vehicles: laps of a
 *  circuit, or one run of a point-to-point course. The brain's Rally module owns the
 *  race: the vehicle physics, crates and items, damage and respawn, the AI drivers and
 *  the finishing order. It reports all of it once per engine tick through the plain-C
 *  contract in rally_abi.h, and this file is the other half of that contract. It
 *
 *    - resolves the brain's Rally exports and arms a match with its laps,
 *    - reads the race state once per engine tick and keeps the previous copy, so every
 *      racer is drawn between the two at the frame's own fraction of the real tick,
 *    - sends the local player's controls (keyboard and mouse) as lockstep events,
 *    - draws the local player's own racer where the controls it has already sent will
 *      put it (rly_predict_frame), so the race answers the keys at once in a network
 *      match whose orders execute several ticks after they are given,
 *    - follows the player's vehicle with the camera,
 *    - draws each racer at the brain's position, height, pitch, roll and heading,
 *    - draws the race's HUD in the game's own pixel style (the lap, the results and the
 *      deciding player's RACE AGAIN or END), the hazards the brain simulates itself, the
 *      start and finish lines, and the crosshair,
 *    - and gives headless scripts their verbs (rallyauto, rallykeys, rallystate...).
 *
 *  Included once by cnc_eyes.cpp, just before draw_frame. The hooks that reach back into
 *  code compiled above this file (the heading, lift and lean every vehicle draw path asks
 *  for) are forward-declared beside SimObject, and every one of them answers at its first
 *  test outside a Rally match, so a game that is not a Rally draws what it always drew.
 * ==================================================================================== */
#ifndef OPENCNC3D_RALLY_MOD_H
#define OPENCNC3D_RALLY_MOD_H

#include "rally_abi.h"
#include "rally_icons.h"

/* ---- the brain's exports -------------------------------------------------------------
   Resolved beside every other brain function. A Rally match with any one of them missing
   refuses to start (rly_arm) rather than half-running. */
struct RlyApi {
    int (*enable)(int, int);
    int (*get_state)(uint64_t, RallyState*);
    int (*get_course)(RallyCoursePoint*, int);
    int (*get_roster)(int32_t*, int);
    int (*pick)(uint64_t, int);
    int (*input)(uint64_t, uint32_t, int32_t, int32_t);
    int (*autopilot)(uint64_t, int);
    uint32_t (*hash)(void);
    int (*configure)(int);              /* ABI 2: the laps of the next race */
    int (*choose)(uint64_t, int);       /* ABI 2: RACE AGAIN or END on the results */
    /* ABI 2: where this machine's racer will be once its sent controls have executed.
       Optional: without it the racer is drawn as executed, which is what ABI 1 did. */
    int (*predict)(uint64_t, const RallyPredictStep*, int, RallyRacer*);
    int (*debug)(int, int, int, int);   /* test hooks; optional, never needed to race */
};
static RlyApi g_rlyApi;

static void rly_resolve(void* hnd)
{
    g_rlyApi.enable     = (int (*)(int, int))dlsym(hnd, "OpenCNC3D_Rally_Enable");
    g_rlyApi.get_state  = (int (*)(uint64_t, RallyState*))dlsym(hnd, "OpenCNC3D_Rally_Get_State");
    g_rlyApi.get_course = (int (*)(RallyCoursePoint*, int))dlsym(hnd, "OpenCNC3D_Rally_Get_Course");
    g_rlyApi.get_roster = (int (*)(int32_t*, int))dlsym(hnd, "OpenCNC3D_Rally_Get_Roster");
    g_rlyApi.pick       = (int (*)(uint64_t, int))dlsym(hnd, "OpenCNC3D_Rally_Pick");
    g_rlyApi.input      = (int (*)(uint64_t, uint32_t, int32_t, int32_t))dlsym(hnd, "OpenCNC3D_Rally_Input");
    g_rlyApi.autopilot  = (int (*)(uint64_t, int))dlsym(hnd, "OpenCNC3D_Rally_Autopilot");
    g_rlyApi.hash       = (uint32_t (*)(void))dlsym(hnd, "OpenCNC3D_Rally_Hash");
    g_rlyApi.configure  = (int (*)(int))dlsym(hnd, "OpenCNC3D_Rally_Configure");
    g_rlyApi.choose     = (int (*)(uint64_t, int))dlsym(hnd, "OpenCNC3D_Rally_Choose");
    g_rlyApi.predict    = (int (*)(uint64_t, const RallyPredictStep*, int, RallyRacer*))
                          dlsym(hnd, "OpenCNC3D_Rally_Predict");
    g_rlyApi.debug      = (int (*)(int, int, int, int))dlsym(hnd, "OpenCNC3D_Rally_Debug");
}

/* The first export this brain lacks, by name, or NULL when it has all of them. */
static const char* rly_missing_export(void)
{
    if (!g_rlyApi.enable)     return "OpenCNC3D_Rally_Enable";
    if (!g_rlyApi.get_state)  return "OpenCNC3D_Rally_Get_State";
    if (!g_rlyApi.get_course) return "OpenCNC3D_Rally_Get_Course";
    if (!g_rlyApi.get_roster) return "OpenCNC3D_Rally_Get_Roster";
    if (!g_rlyApi.pick)       return "OpenCNC3D_Rally_Pick";
    if (!g_rlyApi.input)      return "OpenCNC3D_Rally_Input";
    if (!g_rlyApi.autopilot)  return "OpenCNC3D_Rally_Autopilot";
    if (!g_rlyApi.hash)       return "OpenCNC3D_Rally_Hash";
    if (!g_rlyApi.configure)  return "OpenCNC3D_Rally_Configure";
    if (!g_rlyApi.choose)     return "OpenCNC3D_Rally_Choose";
    return NULL;
}

/* The id the eyes passes to CNC_Advance_Instance for this machine's player. */
static const uint64_t RLY_PLAYER = 0;

/* ---- what the eyes knows about the vehicles ---------------------------------------
   Indexed by the engine's UnitType. The name is the game's own. top, turn and grip
   mirror the brain's kart table (cells a second x100, degrees a second, grip x100) and
   strength is the vehicle's engine strength, which is a racer's health; the chooser's
   three bars are worked out from them (rly_bars). Nothing that decides the race reads
   this table. */
struct RlyVehicle { const char* code; const char* name; int top, turn, grip, strength; };
static const RlyVehicle RLY_VEHICLES[] = {
    { "HTNK", "MAMMOTH TANK",    620, 160, 100, 600  },   /* UNIT_HTANK */
    { "MTNK", "MEDIUM TANK",     640, 170, 95,  400  },   /* UNIT_MTANK */
    { "LTNK", "LIGHT TANK",      650, 180, 95,  300  },   /* UNIT_LTANK */
    { "STNK", "STEALTH TANK",    670, 200, 90,  110  },   /* UNIT_STANK */
    { "FTNK", "FLAME TANK",      640, 170, 95,  300  },   /* UNIT_FTANK */
    { "VICE", "VISCEROID",       0,   0,   0,   150  },   /* UNIT_VICE: not a racer */
    { "APC",  "APC",             660, 190, 90,  200  },   /* UNIT_APC */
    { "MSAM", "ROCKET LAUNCHER", 630, 170, 90,  100  },   /* UNIT_MLRS, whose INI name is MSAM */
    { "JEEP", "HUM-VEE",         680, 210, 90,  150  },   /* UNIT_JEEP */
    { "BGGY", "NOD BUGGY",       690, 225, 85,  140  },   /* UNIT_BUGGY */
    { "HARV", "HARVESTER",       630, 165, 100, 600  },   /* UNIT_HARVESTER */
    { "ARTY", "ARTILLERY",       630, 165, 90,  75   },   /* UNIT_ARTY */
    { "MLRS", "SSM LAUNCHER",    650, 180, 90,  120  },   /* UNIT_MSAM, whose INI name is MLRS */
    { "LST",  "HOVERCRAFT",      680, 190, 55,  400  },   /* UNIT_HOVER */
    { "MHQ",  "MOBILE HQ",       665, 185, 90,  110  },   /* UNIT_MHQ */
    { "BOAT", "GUNBOAT",         0,   0,   0,   700  },   /* UNIT_GUNBOAT: not a racer */
    { "MCV",  "MCV",             625, 160, 100, 600  },   /* UNIT_MCV */
    { "BIKE", "RECON BIKE",      700, 240, 80,  160  },   /* UNIT_BIKE */
};
static const int RLY_VEHICLE_COUNT = (int)(sizeof RLY_VEHICLES / sizeof RLY_VEHICLES[0]);

/* Speed, handling and armour, 1 to 10 across the roster's own range. */
static void rly_bars(const RlyVehicle& v, int out[3])
{
    const auto clamp10 = [](float f) { const int i = (int)(f + 0.5f); return i < 1 ? 1 : (i > 10 ? 10 : i); };
    /* The top speeds sit in a narrow band (620 to 700), so the speed bar spreads that
       band over the whole bar; handling is mostly turn rate (160 to 240) with some grip
       (55 to 100). */
    out[0] = clamp10(1.0f + 9.0f * (v.top - 620) / 80.0f);
    out[1] = clamp10(1.0f + 9.0f * (0.7f * (v.turn - 160) / 80.0f + 0.3f * (v.grip - 55) / 45.0f));
    out[2] = clamp10(v.strength / 60.0f);
}

static const RlyVehicle* rly_vehicle(int unitType)
{
    return (unitType >= 0 && unitType < RLY_VEHICLE_COUNT) ? &RLY_VEHICLES[unitType] : NULL;
}

static const char* const RLY_ITEM_NAME[RALLY_ITEM_COUNT] = {
    "", "NITRO", "TRIPLE NITRO", "MINE", "ROCKET", "HOMING MISSILE", "ION CANNON",
    "ARMOUR", "CLOAK", "REPAIR"
};

/* ---- the match ------------------------------------------------------------------- */
struct RlySeat { bool known; char name[16]; char tag[16]; float r, g, b; };
struct RlySmoke { float x, y, z; int frame; };
struct RlyPose { float x, z, h, yaw, tur, pitch, roll, vx, vz; };

/* ONE SENT CONTROL: what this machine asked for and the engine frame it executes on.
   stamp is -1 while the order waits in the brain's queue for the network to take it
   (rly_on_drain gives it its frame then); in a skirmish the next tick runs it, so it is
   known at once. */
struct RlyCtl { uint32_t bits; int32_t ax, ay; int stamp; };

/* WHAT THE LAST DRAWN FRAME PREDICTED, kept to measure the correction when the next
   executed tick arrives: the base it started from, the frame it predicted, the steps it
   assumed for every frame in between, and the racer it got. */
struct RlyPredCache {
    bool valid;
    int base, hor;                              /* executed-through + 1, and the last frame */
    int unitId;
    RallyPredictStep steps[RALLY_PREDICT_MAX];  /* frames base .. hor */
    RallyRacer at;                              /* the racer at the end of frame hor */
};

struct RlyMatch {
    bool armed;              /* the brain accepted Enable(1) for this mission */
    bool live;               /* a state has arrived and its phase is not OFF */
    int  frame;              /* engine frame `cur` was read on */
    bool havePrev;
    RallyState prev, cur;
    /* this frame's fraction of the way from prev to cur, from the live loop's clock */
    bool clockSet;
    float clockAlpha;
    float alpha;
    float scriptAlpha;       /* rallyalpha: a scripted frame's fraction, -1 for the newest */
    Uint64 camCounter;       /* when the camera last stepped, to step the spring by time */
    bool camInit;
    float camX, camZ, camVX, camVZ;
    /* the course and roster, read once at race start, and the seats once they are dealt */
    bool infoRead;
    int seatsRead;
    int courseN;
    RallyCoursePoint course[RALLY_MAX_COURSE];
    int rosterN;
    int32_t roster[RALLY_MAX_ROSTER];
    RlySeat seat[RALLY_MAX_RACERS];
    /* the course's own [Rally] lines, read from the mission INI: the finish segment and the
       start grid, which the state does not carry */
    bool haveFinish;
    float finish[4];                  /* x1, z1, x2, z2 in cells */
    int gridN, gridFacing;
    float grid[RALLY_MAX_RACERS][2];  /* cells */
    /* input */
    bool sentAny;
    uint32_t sentBits;
    int32_t sentAx, sentAy;
    int sentFrame;
    uint32_t latched;                 /* fire and item presses not sent yet (rly_send) */
    bool aimValid;
    float aimX, aimZ;                 /* cells, the ground under the crosshair */
    bool scriptAim;
    int32_t scriptAx, scriptAy;       /* fine units, from rallyaim */
    /* ZERO-LAG DRIVING (rly_predict_frame). The controls this machine sent, oldest first,
       and the brain's own frame counter now: the dump's frame, plus the ticks advanced
       since the dump was read (rly_after_advance). */
    std::vector<RlyCtl> ctl;
    int brainFrame;
    int drainedFrame;                 /* brainFrame when the network last took the orders */
    bool predOff;                     /* OPENCNC3D_RALLY_PREDICT=0 or rallypredict off */
    bool predActive;                  /* this frame's local racer is the prediction */
    RlyPose predPose;                 /* what was drawn for it, correction included */
    RlyPose lastDrawn;                /* the local racer as the previous frame drew it */
    bool haveLastDrawn, lastWasPred;
    int lastLocalUnit;
    RlyPredCache cache;
    /* THE CORRECTION BEING BLENDED OUT: drawn = predicted + this, decaying to nothing in
       about 150 ms. Cells for the position, DirType units for the two headings. */
    float offX, offZ, offH, offYaw, offTur;
    Uint64 predCounter;               /* when the correction last decayed */
    float scriptDt;                   /* rallydrive: a scripted frame's length, seconds */
    /* the last frame's numbers, for rallydrive's log */
    int predN, predBase, predHor, predCorrections, predSnaps;
    float predSnapCells;              /* this frame's limit for a blended correction (rly_snap_cells) */
    float predLastErr;
    /* the chooser */
    int hover;
    int asked;                        /* the roster index this machine asked for, -1 none */
    /* the results */
    int raceSeen;                     /* race_number the chooser and results were set up for */
    int chose;                        /* RALLY_CHOICE_* this machine sent, NONE before */
    int resultsHover;                 /* 1 RACE AGAIN, 2 END, 0 neither */
    /* what the mission replaced, handed back at shutdown */
    bool sbSaved, sbBefore;
    bool gfxStuck;
    char fxpSavePath[512];            /* the F5 panel's preset, held back while forced */
    FxState fxForced;                 /* the settings as forced, to tell a player's change */
    bool camSaved;
    int camBefore;                    /* the camera mode the race replaced */
    bool versionBad;                  /* the brain's state is another layout: said once */
    /* art */
    bool iconsMade;
    GLuint iconTex[RALLY_ITEM_COUNT];
    GLuint crossTex, flagTex;
    GLuint cameoTex[RALLY_MAX_ROSTER];
    signed char cameoState[RALLY_MAX_ROSTER];   /* 0 not made, 1 made, -1 no art */
    std::vector<RlySmoke> smoke;
    std::map<std::string, float> halfHeight;
};
static RlyMatch g_rly;

/* What the draw hooks above this file read: one entry per racer placed this frame. */
struct RlyDrawn {
    int id;
    float face, tface;       /* DirType, fractional */
    float height;            /* above the ground, cells: what alt_lift answers for it */
    float lift;              /* along the drawn up axis, cells: the pack path's ylift */
    float rise;              /* added to the anchor's ground height, cells */
    float lx, ly, lz;        /* lift and rise as one world vector, for the authored model */
    float cx, cz;            /* the anchor the draw stands the model on */
    float ax, az;            /* the lean about world X then world Z */
    bool ghost;              /* the player's own cloaked racer, drawn see-through */
};
static RlyDrawn g_rlyDrawn[RALLY_MAX_RACERS];
static bool g_rlyPlacing = false;   /* the lean of the ground itself is being asked */

static bool rly_live(void) { return g_rly.armed && g_rly.live; }
static bool rly_owns_input(void) { return rly_live() && !g_gameOver.valid; }
static int rly_phase(void) { return rly_live() ? g_rly.cur.phase : RALLY_PHASE_OFF; }
static const RallyRacer* rly_local(void)
{
    if (!rly_live()) return NULL;
    const int l = g_rly.cur.local;
    return (l >= 0 && l < g_rly.cur.racer_count && l < RALLY_MAX_RACERS) ? &g_rly.cur.racer[l] : NULL;
}
static int rly_tps(void)
{
    const int t = rly_live() ? g_rly.cur.ticks_per_second : 0;
    return t > 0 ? t : (int)(SPEED_HZ_TABLE[game_speed_setting()] + 0.5);
}

/* ---- the hooks the draw paths above call ------------------------------------------ */
static const RlyDrawn* rly_drawn_of(const SimObject& o)
{
    if (o.kind != K_UNIT) return NULL;
    for (int i = 0; i < g_rlyDraw; i++)
        if (g_rlyDrawn[i].id == o.id) return &g_rlyDrawn[i];
    return NULL;
}
static bool rly_heading(const SimObject& o, bool secondary, float* out)
{
    const RlyDrawn* d = rly_drawn_of(o);
    if (!d) return false;
    *out = secondary ? d->tface : d->face;
    return true;
}
/* alt_lift's answer: the racer's height above the ground, which is what everything that
   anchors to a vehicle (its muzzle flashes, its health bar) wants to know. */
static bool rly_lift(const SimObject& o, float* out)
{
    const RlyDrawn* d = rly_drawn_of(o);
    if (!d) return false;
    *out = d->height;
    return true;
}
/* What the paths that pose the model take instead: the lift along its up axis and the
   rise of its anchor, keyed by the anchor like the lean. */
static bool rly_draw_at(float cx, float cz, float* lift, float* ground)
{
    for (int i = 0; i < g_rlyDraw; i++)
        if (g_rlyDrawn[i].cx == cx && g_rlyDrawn[i].cz == cz) {
            *lift = g_rlyDrawn[i].lift;
            *ground += g_rlyDrawn[i].rise;
            return true;
        }
    return false;
}
/* Keyed by the anchor, because mesh_wobble is handed a position and nothing else. The
   anchor is the exact float this file wrote into the object, so the comparison is exact. */
static bool rly_tilt(float cx, float cz, float* ax, float* az)
{
    if (g_rlyPlacing) return false;
    for (int i = 0; i < g_rlyDraw; i++)
        if (g_rlyDrawn[i].cx == cx && g_rlyDrawn[i].cz == cz) {
            *ax = g_rlyDrawn[i].ax;
            *az = g_rlyDrawn[i].az;
            return true;
        }
    return false;
}
static bool rly_lift_world(const SimObject& o, float* dx, float* dy, float* dz)
{
    const RlyDrawn* d = rly_drawn_of(o);
    if (!d) return false;
    *dx = d->lx; *dy = d->ly; *dz = d->lz;
    return true;
}
static bool rly_defer_opaque(const SimObject& o)
{
    const RlyDrawn* d = rly_drawn_of(o);
    return d && d->ghost;
}

/* ---- arming and the mission's start and end --------------------------------------- */

/* The mission's INI, where the course's [Rally] lines live. */
static bool rly_ini_path(const GameOpts* o, char* path, size_t n)
{
    if (!o || !o->scen || !*o->scen) return false;
    snprintf(path, n, "%s%s%s.INI", o->dir ? o->dir : "",
             (o->dir && *o->dir && o->dir[strlen(o->dir) - 1] != '/') ? "/" : "", o->scen);
    return true;
}

/* A COURSE is a map whose INI carries a [Rally] section; nothing else can be raced. */
static bool rly_scen_is_course(const GameOpts* o)
{
    char path[1024];
    if (!rly_ini_path(o, path, sizeof path)) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char line[512];
    bool yes = false;
    while (!yes && fgets(line, sizeof line, f)) {
        const char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        yes = !strncasecmp(p, "[Rally]", 7);
    }
    fclose(f);
    return yes;
}

/* A COURSE IS ALWAYS RACED. A map with a [Rally] section is built for nothing else (a
   walled track, a start grid, no room for a base), so a Tiberian Dawn skirmish on one is a
   Rally however it was started: the skirmish screen, the network lobby or the command
   line. A network match is a skirmish too, and every peer reads the same map, so the peers
   agree without the match setup carrying a switch; a peer whose files differ is refused
   by the scenario hash before that. A campaign-style start of a course without --skirmish
   plays it as an ordinary map, and --rally there is refused (rly_arm). --rally itself asks
   for nothing more today; it is kept so a command line that names it still works. */
static bool rly_will_race(const GameOpts* o, bool say)
{
    if (!o) return false;
    const char* why = NULL;
    if (o->ra3d) why = "Red Alert has no Rally";
    else if (!rly_scen_is_course(o)) why = "the map has no [Rally] course";
    else if (!o->skirmish && !o->rally) why = "it was not started as a skirmish";
    if (why && say && o->rally)
        fprintf(stderr, "rally: --rally is set and %s, so %s is played as an ordinary match\n",
                why, o->scen ? o->scen : "this mission");
    return !why;
}

/* The race's own state, for the network's desync alarm (net_hash_world): race-only
   state such as a held item or a racer's speed between cells moves no engine object,
   so the object dump alone would not notice two peers disagreeing about it. */
static bool rly_net_hash(unsigned* out)
{
    if (!g_rly.armed || !g_rlyApi.hash) return false;
    *out = (unsigned)g_rlyApi.hash();
    return true;
}

/* Before the scenario loads, on every mission start: a Rally when asked for, and
   explicitly not one otherwise, so a Rally never leaks into the next mission. */
static bool rly_arm(const GameOpts* o, bool skirmish)
{
    g_rly.armed = false;
    g_rly.live = false;
    g_rlySpeed = -1;
    const double hz = SPEED_HZ_TABLE[game_speed_setting()];
    const int hz10 = (int)(hz * 10.0 + 0.5);
    const bool want = rly_will_race(o, true);
    if (!want) {
        if (g_rlyApi.enable) g_rlyApi.enable(0, hz10);
        return true;
    }
    if (!skirmish) {
        fprintf(stderr, "rally: a Rally is a Tiberian Dawn skirmish; start it with --skirmish\n");
        boot_refuse("Rally is played as a Tiberian Dawn skirmish");
        return false;
    }
    if (const char* missing = rly_missing_export()) {
        fprintf(stderr, "rally: this brain has no %s, so the Rally match cannot start; "
                        "update the Tiberian Dawn library\n", missing);
        boot_refuse("Rally needs an updated Tiberian Dawn brain (it lacks %s)", missing);
        return false;
    }
    /* THE LAPS, before Enable as the contract asks: a network match's agreed number (the
       host's, off the room's setup, so every peer passes the same one), otherwise the
       skirmish's own. Zero is "not said", which is the default. */
    int laps = o->rally_laps;
    if (nm_active() && nm_setup() && nm_setup()->rally_laps > 0) laps = nm_setup()->rally_laps;
    if (laps <= 0) laps = RALLY_LAPS_DEFAULT;
    if (laps < RALLY_LAPS_MIN) laps = RALLY_LAPS_MIN;
    if (laps > RALLY_LAPS_MAX) laps = RALLY_LAPS_MAX;
    const int raced = g_rlyApi.configure(laps);
    if (!g_rlyApi.enable(1, hz10)) {
        fprintf(stderr, "rally: the brain refused to arm a Rally match\n");
        boot_refuse("The engine refused to start a Rally match");
        return false;
    }
    g_rly.armed = true;
    g_rlySpeed = game_speed_setting();
    /* ZERO-LAG DRIVING IS ON unless OPENCNC3D_RALLY_PREDICT=0 asks for the executed race,
       which is the A/B switch the latency measurement uses */
    {
        const char* pe = getenv("OPENCNC3D_RALLY_PREDICT");
        g_rly.predOff = pe && pe[0] == '0';
    }
    fprintf(stderr, "rally: armed at %.1f engine ticks a second, %d lap%s asked (a circuit "
                    "races %d), local driving %s\n", hz, laps, laps == 1 ? "" : "s", raced,
            g_rly.predOff ? "drawn as executed" : (g_rlyApi.predict ? "predicted" : "drawn as "
                            "executed (this brain cannot predict)"));
    printf("RALLY|armed|laps=%d|configured=%d|predict=%s\n", laps, raced,
           g_rly.predOff ? "off" : (g_rlyApi.predict ? "on" : "missing"));
    fflush(stdout);
    return true;
}

/* The course's [Rally] lines this file needs and the state does not carry. */
static void rly_read_ini(const GameOpts* o)
{
    g_rly.haveFinish = false;
    g_rly.gridN = 0;
    g_rly.gridFacing = 0;
    char path[1024];
    if (!rly_ini_path(o, path, sizeof path)) return;
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "rally: cannot read %s for the course lines\n", path); return; }
    char line[512];
    bool in = false;
    while (fgets(line, sizeof line, f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '[') { in = !strncasecmp(p, "[Rally]", 7); continue; }
        if (!in) continue;
        char* semi = strchr(p, ';');
        if (semi) *semi = 0;
        int a = 0, b = 0, c = 0, d = 0, k = 0;
        if (sscanf(p, "Finish=%d,%d,%d,%d", &a, &b, &c, &d) == 4) {
            g_rly.haveFinish = true;
            g_rly.finish[0] = a / 256.0f; g_rly.finish[1] = b / 256.0f;
            g_rly.finish[2] = c / 256.0f; g_rly.finish[3] = d / 256.0f;
        } else if (sscanf(p, "GridFacing=%d", &a) == 1) {
            g_rly.gridFacing = a & 255;
        } else if (sscanf(p, "Grid%d=%d,%d", &k, &a, &b) == 3 && k >= 0 && k < RALLY_MAX_RACERS) {
            g_rly.grid[k][0] = a / 256.0f;
            g_rly.grid[k][1] = b / 256.0f;
            if (k + 1 > g_rly.gridN) g_rly.gridN = k + 1;
        }
    }
    fclose(f);
    fprintf(stderr, "rally: course lines from %s: finish %s, %d grid slots facing %d\n",
            path, g_rly.haveFinish ? "yes" : "NO", g_rly.gridN, g_rly.gridFacing);
}

/* Once the brain has started the scenario: the sidebar goes and Enhanced is forced for
   the match, so every racer sees the same picture, and the race is watched through the
   console camera, whose start zoom is the race's: the old camera's start view fits the
   whole map, where a vehicle is a few pixels across, and the C key that would switch back
   is a key the race takes. All three are handed back by rly_shutdown. Called before the
   start zoom is chosen, so the zoom is chosen for the console camera. */
static void rly_boot(const GameOpts* o)
{
    if (!g_rly.armed) return;
    g_rly.sbSaved = true;
    g_rly.sbBefore = g_sbOn;
    g_sbOn = false;
    g_rly.camSaved = true;
    g_rly.camBefore = g_camMode;
    if (g_camMode != CAM_N64) {
        set_cam_mode(CAM_N64);
        fprintf(stderr, "rally: the race is watched through the N64 camera (was %s)\n",
                g_rly.camBefore == CAM_N64 ? "N64" : "OLD");
    }
    g_rlyGfxBefore = fx_mode_of(&g_fx);
    if (g_rlyGfxBefore != FX_MODE_ENHANCED) {
        /* the F5 panel's SAVE would write the forced picture as the player's own */
        snprintf(g_rly.fxpSavePath, sizeof g_rly.fxpSavePath, "%s", g_fxpSavePath);
        g_fxpSavePath[0] = 0;
        gmw_switch_quiet(FX_MODE_ENHANCED);
    }
    g_rly.fxForced = g_fx;
    printf("RALLY|graphics|enhanced|was=%d\n", g_rlyGfxBefore);
    fflush(stdout);
    g_rly.asked = -1;
    g_rly.hover = -1;
    g_rly.scriptAlpha = -1.0f;
    g_rly.drainedFrame = -1;
    g_rly.lastLocalUnit = -2;
    g_rly.raceSeen = 0;
    rly_read_ini(o);
}

static void rly_release_art(void)
{
    for (int i = 0; i < RALLY_ITEM_COUNT; i++)
        if (g_rly.iconTex[i]) glDeleteTextures(1, &g_rly.iconTex[i]);
    if (g_rly.crossTex) glDeleteTextures(1, &g_rly.crossTex);
    if (g_rly.flagTex) glDeleteTextures(1, &g_rly.flagTex);
    for (int i = 0; i < RALLY_MAX_ROSTER; i++)
        if (g_rly.cameoTex[i]) glDeleteTextures(1, &g_rly.cameoTex[i]);
}

static void rly_shutdown(void)
{
    g_rlyDraw = 0;
    g_rlySpeed = -1;
    if (g_rlyGfxBefore >= 0) {
        const int was = g_rlyGfxBefore;
        g_rlyGfxBefore = -1;
        if (was != FX_MODE_ENHANCED) {
            /* A SETTING CHANGED DURING THE RACE (the mouse buttons, the display, the frame
               counter) was held back from the file while the forced picture was on; it is
               written now, with the player's own mode back in place of the forced one.
               When nothing was changed nothing is written: the trip through the forced
               mode resets a few Enhanced-only dials in memory, and writing those would be
               the race changing the player's file. */
            const bool touched = memcmp(&g_fx, &g_rly.fxForced, sizeof g_fx) != 0;
            gmw_switch_quiet(was);
            if (g_rly.fxpSavePath[0]) fxp_set_save_path(g_rly.fxpSavePath);
            if (touched) opt_visuals_sync();
            else opt_visuals_adopt();
        }
    }
    if (g_rly.camSaved && g_rly.camBefore != g_camMode) set_cam_mode(g_rly.camBefore);
    if (g_rly.sbSaved) g_sbOn = g_rly.sbBefore;
    rly_release_art();
    g_rly.smoke.clear();
    g_rly.halfHeight.clear();
    g_rly = RlyMatch();
}

/* ---- the state, once per engine tick ----------------------------------------------- */
static void rly_read_info(void)
{
    g_rly.courseN = g_rlyApi.get_course(g_rly.course, RALLY_MAX_COURSE);
    if (g_rly.courseN > RALLY_MAX_COURSE) g_rly.courseN = RALLY_MAX_COURSE;
    if (g_rly.courseN < 0) g_rly.courseN = 0;
    g_rly.rosterN = g_rlyApi.get_roster(g_rly.roster, RALLY_MAX_ROSTER);
    if (g_rly.rosterN > RALLY_MAX_ROSTER) g_rly.rosterN = RALLY_MAX_ROSTER;
    if (g_rly.rosterN < 0) g_rly.rosterN = 0;
    g_rly.infoRead = true;
    fprintf(stderr, "rally: course %d points, roster %d vehicles\n", g_rly.courseN, g_rly.rosterN);
}

/* WHO EACH RACER IS: the engine's own player info, matched to the racer by house. Read
   once the race has its racers, which is not the first state: the seats are dealt on the
   race's first tick. */
static void rly_read_seats(void)
{
    static unsigned char buf[sizeof(CNCPlayerInfoStruct) + 64];
    for (int r = 0; r < g_rly.cur.racer_count && r < RALLY_MAX_RACERS; r++) {
        RlySeat& s = g_rly.seat[r];
        s.known = false;
        s.tag[0] = 0;
        const int lc = r % LIVERY_COUNT;
        s.r = LIVERY_BAND[lc][2][0] / 255.0f;
        s.g = LIVERY_BAND[lc][2][1] / 255.0f;
        s.b = LIVERY_BAND[lc][2][2] / 255.0f;
        snprintf(s.name, sizeof s.name, "RACER %d", r + 1);
        for (int k = 0; k < g_matchRows && k < 8; k++) {
            memset(buf, 0, sizeof buf);
            if (!BrainGetState || !BrainGetState(GAME_STATE_PLAYER_INFO, g_matchId[k], buf,
                                                 (unsigned int)sizeof buf))
                continue;
            const CNCPlayerInfoStruct& pi = *(const CNCPlayerInfoStruct*)buf;
            if ((int)pi.House != g_rly.cur.racer[r].house) continue;
            if (pi.Name[0]) snprintf(s.name, sizeof s.name, "%s", pi.Name);
            if (pi.ColorIndex >= 0 && pi.ColorIndex < LIVERY_COUNT) {
                s.r = LIVERY_BAND[pi.ColorIndex][2][0] / 255.0f;
                s.g = LIVERY_BAND[pi.ColorIndex][2][1] / 255.0f;
                s.b = LIVERY_BAND[pi.ColorIndex][2][2] / 255.0f;
            }
            s.known = true;
            break;
        }
    }
    /* Two seats with one name (every computer is COMPUTER) are told apart by their seat. */
    for (int r = 0; r < g_rly.cur.racer_count && r < RALLY_MAX_RACERS; r++) {
        bool twin = false;
        for (int k = 0; k < g_rly.cur.racer_count && k < RALLY_MAX_RACERS; k++)
            if (k != r && !strcmp(g_rly.seat[k].name, g_rly.seat[r].name)) twin = true;
        if (!twin) continue;
        char base[16];
        snprintf(base, sizeof base, "%.11s", g_rly.seat[r].name);
        snprintf(g_rly.seat[r].tag, sizeof g_rly.seat[r].tag, "%s %d", base, r + 1);
    }
    for (int r = 0; r < g_rly.cur.racer_count && r < RALLY_MAX_RACERS; r++)
        if (g_rly.seat[r].tag[0]) memcpy(g_rly.seat[r].name, g_rly.seat[r].tag, sizeof g_rly.seat[r].name);
    g_rly.seatsRead = g_rly.cur.racer_count;
    fprintf(stderr, "rally: %d racers, this machine's is %d\n", g_rly.cur.racer_count, g_rly.cur.local);
    for (int r = 0; r < g_rly.cur.racer_count && r < RALLY_MAX_RACERS; r++)
        fprintf(stderr, "rally: racer %d house %d %s%s\n", r, g_rly.cur.racer[r].house,
                g_rly.seat[r].name, g_rly.seat[r].known ? "" : " (no player info)");
}

/* ---- what this machine sent, and the frame each executes on ------------------------ */

/* After every engine advance (brain_advance): the brain's frame counter moved by one, and
   the next dump will say so; until it is read, this keeps count. */
static void rly_after_advance(void)
{
    if (g_rly.armed) g_rly.brainFrame++;
}

/* THE NETWORK TOOK THIS MACHINE'S QUEUED ORDERS (net_drain_cb) and stamped every one of
   them frame_delay frames past the brain's frame now: the frame it runs on at every peer.
   A control sent since the last drain is among them. */
static void rly_on_drain(int frame_delay)
{
    if (!g_rly.armed) return;
    const int stamp = g_rly.brainFrame + frame_delay;
    for (size_t i = 0; i < g_rly.ctl.size(); i++)
        if (g_rly.ctl[i].stamp < 0) g_rly.ctl[i].stamp = stamp;
    g_rly.drainedFrame = g_rly.brainFrame;
}

/* How far ahead the network stamps this machine's orders: the room's agreed lookahead in a
   match, none in a skirmish, whose orders run on the next tick. */
static int rly_ahead(void)
{
    return nm_active() ? nm_ahead() : 0;
}

static void rly_trim_controls(int executedThrough);

static void rly_after_refresh(void)
{
    if (!g_rly.armed) return;
    g_rly.brainFrame = g_engineFrame;
    if (!g_rlyApi.get_state) return;
    /* Get_State is handed no size. Room past the end of the struct means a brain whose
       RallyState has grown writes into the slack rather than past this buffer, and the
       version check below then refuses to draw a layout this file does not know. */
    static union { RallyState s; unsigned char room[sizeof(RallyState) + 16384]; } got;
    RallyState& s = got.s;
    memset(&got, 0, sizeof got);
    if (!g_rlyApi.get_state(RLY_PLAYER, &s) || s.phase == RALLY_PHASE_OFF) {
        g_rly.live = false;
        return;
    }
    if (s.version != RALLY_ABI_VERSION) {
        if (!g_rly.versionBad) {
            g_rly.versionBad = true;
            fprintf(stderr, "rally: the brain reports Rally state version %d and this renderer "
                            "reads version %d, so the race is not drawn; build the Tiberian Dawn "
                            "library and the renderer from one checkout\n",
                    (int)s.version, RALLY_ABI_VERSION);
            printf("RALLY|abi|mismatch|brain=%d|eyes=%d\n", (int)s.version, RALLY_ABI_VERSION);
            fflush(stdout);
        }
        g_rly.live = false;
        return;
    }
    if (s.racer_count > RALLY_MAX_RACERS) s.racer_count = RALLY_MAX_RACERS;
    if (s.racer_count < 0) s.racer_count = 0;
    if (s.crate_count > RALLY_MAX_CRATES) s.crate_count = RALLY_MAX_CRATES;
    if (s.crate_count < 0) s.crate_count = 0;
    if (s.hazard_count > RALLY_MAX_HAZARDS) s.hazard_count = RALLY_MAX_HAZARDS;
    if (s.hazard_count < 0) s.hazard_count = 0;
    const bool newTick = !g_rly.live || g_rly.frame != g_engineFrame;
    if (newTick && g_rly.live) {
        g_rly.prev = g_rly.cur;
        g_rly.havePrev = true;
    } else if (!g_rly.live) {
        g_rly.havePrev = false;
    }
    g_rly.cur = s;
    g_rly.frame = g_engineFrame;
    g_rly.live = true;
    if (!g_rly.infoRead) rly_read_info();
    if (s.racer_count > 0 && g_rly.seatsRead != s.racer_count) rly_read_seats();
    /* A RACE AGAIN: the chooser and the results start over for the new race */
    if (s.race_number != g_rly.raceSeen) {
        if (g_rly.raceSeen != 0) {
            fprintf(stderr, "rally: race %d of the match begins on the same course\n", (int)s.race_number);
            g_rly.smoke.clear();
            g_rly.camInit = false;
        }
        g_rly.raceSeen = s.race_number;
        g_rly.asked = -1;
        g_rly.chose = RALLY_CHOICE_NONE;
        g_rly.cache.valid = false;
        g_rly.haveLastDrawn = false;
        /* THE BRAIN SEATS EVERY RACER AFRESH for a new race, holding no keys and no aim, so
           what this machine sent in the last race is forgotten here too. Without this a key
           held from the finish through the results was never sent again (it had not
           changed), the racer sat on the grid at GO, and the prediction drove it forward
           from a key the brain no longer held. The keys held now go out on the next frame. */
        g_rly.ctl.clear();
        g_rly.sentAny = false;
        g_rly.latched = 0;
    }
    if (!newTick) return;
    rly_trim_controls(g_engineFrame - 1);
    /* the smoke a rocket leaves, one puff a tick where it is now */
    for (int i = 0; i < s.hazard_count; i++) {
        const RallyHazard& h = s.hazard[i];
        if (h.kind != RALLY_HAZ_ROCKET && h.kind != RALLY_HAZ_HOMING) continue;
        RlySmoke p;
        p.x = h.x / 65536.0f; p.z = h.y / 65536.0f; p.y = h.z / 65536.0f;
        p.frame = g_engineFrame;
        g_rly.smoke.push_back(p);
    }
    const int life = rly_tps();
    size_t keep = 0;
    for (size_t i = 0; i < g_rly.smoke.size(); i++)
        if (g_engineFrame - g_rly.smoke[i].frame <= life) g_rly.smoke[keep++] = g_rly.smoke[i];
    g_rly.smoke.resize(keep);
    if (g_rly.smoke.size() > 400) g_rly.smoke.erase(g_rly.smoke.begin(), g_rly.smoke.end() - 400);
}

/* The live loop's clock: how far this frame sits between the last two engine ticks, from
   the period those ticks really have at this match's speed. A scripted frame never sets
   it and is drawn at the newest state. */
static void rly_frame_clock(bool advancing, double a)
{
    if (!g_rly.armed) return;
    if (a < 0.0) a = 0.0;
    if (a > 1.0) a = 1.0;
    g_rly.clockSet = true;
    g_rly.clockAlpha = advancing ? (float)a : 1.0f;
}

/* ---- one racer between two ticks -------------------------------------------------- */

static float rly_angle_mix(int32_t from, int32_t to, float t)
{
    const int32_t d = (int32_t)((uint32_t)(to - from + 32768) & 65535u) - 32768;
    return (float)from + (float)d * t;
}

/* A RACER BETWEEN TWO OF ITS STATES, t of the way from p to c. A respawn, a rescue or a
   new vehicle is a jump, not a journey, and is drawn where it landed. */
static RlyPose rly_mix_racers(const RallyRacer* p, const RallyRacer& c, float t)
{
    if (!p || p->unit_id != c.unit_id || abs(c.x - p->x) > 2 * RALLY_FINE_PER_CELL
        || abs(c.y - p->y) > 2 * RALLY_FINE_PER_CELL) {
        p = &c;
        t = 1.0f;
    }
    const double F = 1.0 / RALLY_FINE_PER_CELL;
    RlyPose P;
    P.x = (float)((p->x + (c.x - p->x) * (double)t) * F);
    P.z = (float)((p->y + (c.y - p->y) * (double)t) * F);
    P.h = (float)((p->z + (c.z - p->z) * (double)t) * F);
    if (P.h < 0.0f) P.h = 0.0f;
    P.vx = (float)((p->vx + (c.vx - p->vx) * (double)t) * F);
    P.vz = (float)((p->vy + (c.vy - p->vy) * (double)t) * F);
    const float TO_DIR = 256.0f / 65536.0f, TO_RAD = 6.2831853f / 65536.0f;
    P.yaw = heading_wrap(rly_angle_mix(p->yaw & 65535, c.yaw & 65535, t) * TO_DIR);
    P.tur = heading_wrap(rly_angle_mix(p->turret & 65535, c.turret & 65535, t) * TO_DIR);
    P.pitch = rly_angle_mix(p->pitch, c.pitch, t) * TO_RAD;
    P.roll = rly_angle_mix(p->roll, c.roll, t) * TO_RAD;
    return P;
}

/* A racer as the engine has executed it, between its last two ticks. */
static RlyPose rly_pose_executed(int i)
{
    const RallyRacer& c = g_rly.cur.racer[i];
    const RallyRacer* p = (g_rly.havePrev && i < g_rly.prev.racer_count) ? &g_rly.prev.racer[i] : NULL;
    return rly_mix_racers(p, c, g_rly.alpha);
}

/* Every racer as it is drawn: the local player's own from the prediction (rly_predict_frame),
   every other as executed. */
static RlyPose rly_pose(int i)
{
    if (g_rly.predActive && i == g_rly.cur.local) return g_rly.predPose;
    return rly_pose_executed(i);
}

/* ---- zero-lag driving ----------------------------------------------------------------
 *
 * WHY THE LOCAL RACER IS PREDICTED. A network match executes every order a fixed number of
 * ticks after it is given (the room's lookahead, three to twelve turns), so the same frame
 * on every peer runs the same orders; that is the whole of lockstep. Drawn as executed, a
 * racer therefore answers its own steering three to twelve ticks late, 170 to 660 ms at
 * the race's tick rate, which no racing game asks of a player. The order wire and the
 * simulation stay exactly as they are. What changes is only where this machine DRAWS its
 * own racer: where the brain's own driving physics (OpenCNC3D_Rally_Predict, pure, drawing
 * no random number) says the controls this machine has already sent will put it, one step
 * per engine tick from the newest executed tick up to the frame the next sent control
 * would run on. A key pressed now is sent now and is part of that, so the race answers on
 * the next drawn frame. Everybody else's racer is drawn as executed.
 *
 * THE PREDICTION LEAVES OUT WHAT OTHER MACHINES DECIDE (a ram, a hit, an item), so now and
 * then an executed tick disagrees with what was drawn. The difference is measured where the
 * two can be compared exactly, at the frame the previous drawing predicted (RlyPredCache),
 * and drawn away over about 150 ms rather than snapped; one larger than the racer could
 * have driven in the ticks it is drawn ahead (rly_snap_cells) is snapped, because sliding a
 * racer across the track is worse than a cut, and a respawn or a rescue is a cut anyway.
 * When the prediction stops (the racer spun out, flipped or died, or the race ended) the
 * racer goes back to the executed state through the same blend. */
static const float RLY_BLEND_SECONDS = 0.05f;   /* time constant: 95% gone after 150 ms */
static const float RLY_SNAP_CELLS = 3.0f;       /* the least a correction may be before it snaps */
static const float RLY_SNAP_DIR = 64.0f;        /* a quarter turn, in DirType units */

/* HOW FAR A CORRECTION MAY BE AND STILL BE BLENDED. The drawn racer runs as many ticks
   ahead of the executed one as the prediction has steps, so whenever the prediction stops
   or guesses wrong (the finish, a spin, a flip, a hit) the two can be that many ticks of
   driving apart: 1.4 cells at the shortest lookahead and nearly 5 at the longest, measured.
   A fixed limit turned every such moment at a long lookahead into a jump back of several
   cells. The limit is that distance at the fastest the racer can go (half as fast again
   as its top speed, for a boost) plus a cell, and never less than RLY_SNAP_CELLS; anything
   beyond it is not driving (a rescue or a respawn is caught as a cut before it gets here)
   and is snapped. */
static float rly_snap_cells(const RallyRacer& me, int steps)
{
    const int32_t sp = me.speed < 0 ? -me.speed : me.speed;
    const float fastest = (float)(sp > me.top_speed ? sp : me.top_speed) * 1.5f / RALLY_FINE_PER_CELL;
    const float reach = (float)(steps + 1) * fastest + 1.0f;
    return reach > RLY_SNAP_CELLS ? reach : RLY_SNAP_CELLS;
}

static float rly_dir_diff(float a, float b)
{
    float d = fmodf(a - b + 128.0f, 256.0f);
    if (d < 0.0f) d += 256.0f;
    return d - 128.0f;
}

/* The engine frame a control sent now would run on: the next drain's stamp in a match (or
   the one after, when this tick's orders have already been drained and the turn is still
   waiting), the next tick in a skirmish. */
static int rly_pending_stamp(void)
{
    if (!nm_active()) return g_rly.brainFrame;
    const int next = g_rly.drainedFrame == g_rly.brainFrame ? g_rly.brainFrame + 1 : g_rly.brainFrame;
    return next + rly_ahead();
}

/* The controls in force on engine frame f: the newest sent control that runs by then. */
static void rly_step_at(int f, int pending, const RallyRacer& me, RallyPredictStep* out)
{
    for (int i = (int)g_rly.ctl.size() - 1; i >= 0; i--) {
        const RlyCtl& c = g_rly.ctl[i];
        const int st = c.stamp < 0 ? pending : c.stamp;
        if (st <= f) { out->bits = c.bits; out->aim_x = c.ax; out->aim_y = c.ay; return; }
    }
    /* nothing sent yet runs by then: no keys, aimed straight ahead */
    const float a = (me.yaw & 65535) * 6.2831853f / 65536.0f;
    out->bits = 0;
    out->aim_x = me.x + (int32_t)(sinf(a) * 4 * RALLY_FINE_PER_CELL);
    out->aim_y = me.y - (int32_t)(cosf(a) * 4 * RALLY_FINE_PER_CELL);
}

/* Forget the controls that can no longer matter: everything older than the newest one
   already executed, which is the control the brain holds now. */
static void rly_trim_controls(int executedThrough)
{
    int keep = -1;
    for (int i = 0; i < (int)g_rly.ctl.size(); i++)
        if (g_rly.ctl[i].stamp >= 0 && g_rly.ctl[i].stamp <= executedThrough) keep = i;
    if (keep > 0) g_rly.ctl.erase(g_rly.ctl.begin(), g_rly.ctl.begin() + keep);
    if (g_rly.ctl.size() > 256) g_rly.ctl.erase(g_rly.ctl.begin(), g_rly.ctl.end() - 256);
}

static bool rly_predict_n(const RallyPredictStep* steps, int n, const RallyRacer& me, RallyRacer* out)
{
    if (n <= 0) { *out = me; return true; }
    if (n > RALLY_PREDICT_MAX) n = RALLY_PREDICT_MAX;
    return g_rlyApi.predict(RLY_PLAYER, steps, n, out) == 1;
}

/* Take a jump in what is drawn as a correction to blend away (or to snap, when large). */
static void rly_take_correction(float ex, float ez, float eh, float eyaw, float etur)
{
    const float err = sqrtf(ex * ex + ez * ez);
    g_rly.predLastErr = err;
    if (err > g_rly.predSnapCells || fabsf(eyaw) > RLY_SNAP_DIR) {
        g_rly.offX = g_rly.offZ = g_rly.offH = g_rly.offYaw = g_rly.offTur = 0.0f;
        g_rly.predSnaps++;
        return;
    }
    if (err < 1e-4f && fabsf(eyaw) < 1e-3f && fabsf(etur) < 1e-3f && fabsf(eh) < 1e-4f) return;
    g_rly.offX += ex; g_rly.offZ += ez; g_rly.offH += eh;
    g_rly.offYaw += eyaw; g_rly.offTur += etur;
    g_rly.predCorrections++;
}

/* Once per drawn frame, before any racer is placed: what the local racer is drawn as. dt
   is the frame's length in seconds (0 for a scripted frame with no clock, which is drawn
   with no correction pending). */
static void rly_predict_frame(float dt)
{
    g_rly.predActive = false;
    g_rly.predN = 0;
    const RallyState& S = g_rly.cur;
    const RallyRacer* me = rly_local();
    if (!me || g_rly.predOff || me->unit_id < 0) {
        g_rly.haveLastDrawn = false;
        g_rly.cache.valid = false;
        g_rly.offX = g_rly.offZ = g_rly.offH = g_rly.offYaw = g_rly.offTur = 0.0f;
        return;
    }
    const int li = S.local;
    const RlyPose exec = rly_pose_executed(li);
    /* a new vehicle, a respawn or a rescue is a cut, and so is the executed racer jumping */
    const bool newUnit = me->unit_id != g_rly.lastLocalUnit;
    const RallyRacer* pr = (g_rly.havePrev && li < g_rly.prev.racer_count) ? &g_rly.prev.racer[li] : NULL;
    const bool jumped = pr && (abs(me->x - pr->x) > 2 * RALLY_FINE_PER_CELL
                               || abs(me->y - pr->y) > 2 * RALLY_FINE_PER_CELL);
    if (newUnit || jumped) {
        g_rly.offX = g_rly.offZ = g_rly.offH = g_rly.offYaw = g_rly.offTur = 0.0f;
        g_rly.haveLastDrawn = false;
        g_rly.cache.valid = false;
    }
    g_rly.lastLocalUnit = me->unit_id;

    /* THE STEPS: one an engine tick, from the newest executed one (base: the dump's frame,
       every frame below it has run) to the frame a control sent now would run on */
    const int base = g_rly.frame;
    const int pending = rly_pending_stamp();
    int n = pending - base + 1;
    if (n < 1) n = 1;
    if (n > RALLY_PREDICT_MAX) n = RALLY_PREDICT_MAX;
    RallyPredictStep steps[RALLY_PREDICT_MAX];
    for (int k = 0; k < n; k++) rly_step_at(base + k, pending, *me, &steps[k]);
    /* A TURN THAT IS LATE. This tick's orders have gone out but the turn cannot run yet (a
       peer's orders have not arrived), so the world holds still and the frame clock sits at
       the end of the tick. The horizon moved on a tick with the orders, and drawn there the
       racer would leap a tick ahead for as long as the wait lasts and fall most of a tick
       back when the turn runs. It is held where the last frame before the wait drew it
       instead: one step short of the horizon, at the end of the tick, which is exactly
       where the turn picks it up again. */
    const bool waiting = nm_active() && g_rly.drainedFrame == g_rly.brainFrame && n > 1;
    const int nd = waiting ? n - 1 : n;
    const float at = waiting ? 1.0f : g_rly.alpha;
    g_rly.predSnapCells = rly_snap_cells(*me, nd);
    RallyRacer hi, lo;
    bool ok = g_rlyApi.predict && S.phase == RALLY_PHASE_RACE
              && rly_predict_n(steps, nd, *me, &hi) && rly_predict_n(steps, nd - 1, *me, &lo);
    RlyPose raw = ok ? rly_mix_racers(&lo, hi, at) : exec;

    /* THE CORRECTION. A new executed tick: what the previous drawing predicted for its last
       frame, against what the same steps give from the new tick. */
    if (ok && g_rly.cache.valid && g_rly.cache.base != base && g_rly.cache.unitId == me->unit_id) {
        const int m = g_rly.cache.hor - base + 1;
        RallyRacer now;
        if (m >= 0 && g_rly.cache.hor < g_rly.cache.base + RALLY_PREDICT_MAX
            && rly_predict_n(&g_rly.cache.steps[base - g_rly.cache.base], m, *me, &now)) {
            const double F = 1.0 / RALLY_FINE_PER_CELL;
            const float D = 256.0f / 65536.0f;
            rly_take_correction((float)((g_rly.cache.at.x - now.x) * F), (float)((g_rly.cache.at.y - now.y) * F),
                                (float)((g_rly.cache.at.z - now.z) * F),
                                rly_dir_diff((g_rly.cache.at.yaw & 65535) * D, (now.yaw & 65535) * D),
                                rly_dir_diff((g_rly.cache.at.turret & 65535) * D, (now.turret & 65535) * D));
        } else if (g_rly.haveLastDrawn) {
            /* the frame it predicted is already behind the new tick: keep the picture still */
            rly_take_correction(g_rly.lastDrawn.x - raw.x - g_rly.offX, g_rly.lastDrawn.z - raw.z - g_rly.offZ,
                                g_rly.lastDrawn.h - raw.h - g_rly.offH,
                                rly_dir_diff(g_rly.lastDrawn.yaw, raw.yaw) - g_rly.offYaw,
                                rly_dir_diff(g_rly.lastDrawn.tur, raw.tur) - g_rly.offTur);
        }
    }
    /* THE PREDICTION STARTING OR STOPPING (a spin, a flip, the finish): the two pictures are
       ticks apart, so the step between them is blended like any correction */
    if (g_rly.haveLastDrawn && ok != g_rly.lastWasPred) {
        rly_take_correction(g_rly.lastDrawn.x - raw.x - g_rly.offX, g_rly.lastDrawn.z - raw.z - g_rly.offZ,
                            g_rly.lastDrawn.h - raw.h - g_rly.offH,
                            rly_dir_diff(g_rly.lastDrawn.yaw, raw.yaw) - g_rly.offYaw,
                            rly_dir_diff(g_rly.lastDrawn.tur, raw.tur) - g_rly.offTur);
    }
    /* THE BLEND, by the frame's own length */
    if (dt > 0.0f) {
        const float k = expf(-dt / RLY_BLEND_SECONDS);
        g_rly.offX *= k; g_rly.offZ *= k; g_rly.offH *= k; g_rly.offYaw *= k; g_rly.offTur *= k;
    } else {
        g_rly.offX = g_rly.offZ = g_rly.offH = g_rly.offYaw = g_rly.offTur = 0.0f;
    }
    RlyPose P = raw;
    P.x += g_rly.offX; P.z += g_rly.offZ; P.h += g_rly.offH;
    if (P.h < 0.0f) P.h = 0.0f;
    P.yaw = heading_wrap(P.yaw + g_rly.offYaw);
    P.tur = heading_wrap(P.tur + g_rly.offTur);
    const bool blending = fabsf(g_rly.offX) + fabsf(g_rly.offZ) + fabsf(g_rly.offYaw) > 1e-4f;
    g_rly.predActive = ok || blending;
    g_rly.predPose = P;
    g_rly.lastDrawn = P;
    g_rly.haveLastDrawn = true;
    g_rly.lastWasPred = ok;
    g_rly.predN = ok ? nd : 0;
    g_rly.predBase = base;
    g_rly.predHor = base + nd - 1;
    if (ok) {
        g_rly.cache.valid = true;
        g_rly.cache.base = base;
        g_rly.cache.hor = base + nd - 1;
        g_rly.cache.unitId = me->unit_id;
        memcpy(g_rly.cache.steps, steps, sizeof(RallyPredictStep) * (size_t)nd);
        g_rly.cache.at = hi;
    } else {
        g_rly.cache.valid = false;
    }
}

/* A CONTROL WAS JUST SENT, before the tick it may run on. What the last drawn frame predicted
   is predicted again with it, from the same executed tick, so the next tick's comparison
   measures what other machines did and not this machine's own key: a key press is the
   response the player wants to see at once, never a correction to blend away. When a tick
   has run since that frame there is no same tick to start from, and the comparison is
   simply skipped once. */
static void rly_cache_after_send(void)
{
    if (!g_rly.cache.valid) return;
    const RallyRacer* me = rly_local();
    if (!me || g_rly.cache.base != g_rly.frame || g_rly.cache.unitId != me->unit_id || !g_rlyApi.predict) {
        g_rly.cache.valid = false;
        return;
    }
    const int pending = rly_pending_stamp();
    const int n = g_rly.cache.hor - g_rly.cache.base + 1;
    if (n < 1 || n > RALLY_PREDICT_MAX) { g_rly.cache.valid = false; return; }
    for (int k = 0; k < n; k++) rly_step_at(g_rly.cache.base + k, pending, *me, &g_rly.cache.steps[k]);
    RallyRacer at;
    if (rly_predict_n(g_rly.cache.steps, n, *me, &at)) g_rly.cache.at = at;
    else g_rly.cache.valid = false;
}

/* Half a vehicle's height, in cells: the pivot a flip turns it about. Read off the
   cartridge model once per type; a type with no model takes a tank's. */
static float rly_half_height(const SimObject& o)
{
    std::map<std::string, float>::iterator it = g_rly.halfHeight.find(o.type);
    if (it != g_rly.halfHeight.end()) return it->second;
    float h = 0.15f;
    const int mi = mesh_for(o);
    if (mi >= 0 && mi < (int)g_pack.mesh.size()) {
        float top = 0.0f;
        const std::vector<PackTri>& tris = g_pack.mesh[mi].tris;
        for (size_t k = 0; k < tris.size(); k++)
            for (int v = 0; v < 3; v++)
                if (tris[k].v[v].y > top) top = tris[k].v[v].y;
        const float half = top * MODEL_SCALE * 0.5f;
        if (half > 0.04f) h = half > 0.45f ? 0.45f : half;
    }
    g_rly.halfHeight[o.type] = h;
    return h;
}

/* 3x3 rotations in the draw's own conventions (draw_mesh, m3_matrix): Ry turns by the
   facing angle, Rx and Rz are the lean about world X and world Z. */
static void rly_mul3(const float a[3][3], const float b[3][3], float o[3][3])
{
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            o[r][c] = a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c];
}
static void rly_rx(float t, float m[3][3])
{
    const float c = cosf(t), s = sinf(t);
    const float v[3][3] = { { 1, 0, 0 }, { 0, c, -s }, { 0, s, c } };
    memcpy(m, v, sizeof v);
}
static void rly_ry(float t, float m[3][3])
{
    const float c = cosf(t), s = sinf(t);
    const float v[3][3] = { { c, 0, s }, { 0, 1, 0 }, { -s, 0, c } };
    memcpy(m, v, sizeof v);
}
static void rly_rz(float t, float m[3][3])
{
    const float c = cosf(t), s = sinf(t);
    const float v[3][3] = { { c, -s, 0 }, { s, c, 0 }, { 0, 0, 1 } };
    memcpy(m, v, sizeof v);
}

/* ONE RACER'S TRANSFORM, IN THE TERMS EVERY DRAW PATH ALREADY USES.
 *
 * A vehicle is drawn as: scale, the facing about Y, a lift along the model's up axis,
 * then a lean about world X and then world Z (mesh_wobble's two angles), then the anchor
 * on the ground. Five places build that chain (draw_mesh, the authored-model matrix, the
 * remastered vehicle's depth cull, its lamps and its pick frame), and all five take the
 * facing, the lean, the lift and the anchor's height from the same questions. So a racer
 * is not given a new rotation anywhere: its whole attitude is folded into the answers,
 * and the five paths cannot disagree.
 *
 * The attitude is the ground's lean (the renderer's own slope and washboard at that
 * spot) times the racer's yaw times its body pitch and roll. Any rotation can be written
 * as Rz(B) Rx(A) Ry(C), so it is decomposed into exactly that: A and B become the lean
 * and C the facing. That is exact at every angle, which is what lets a flipped racer lie
 * on its roof rather than lean a little.
 *
 * The racer turns about its own middle, not about the ground under it: the lift is minus
 * half its height, which puts the middle on the anchor before the lean, and the anchor
 * rises by the racer's height plus that half. Upright the two cancel and the model stands
 * exactly where an ordinary vehicle stands; on its roof its roof is on the ground; in the
 * air the whole of it is at the race's height. */
static void rly_place(SimObject& o, const RlyPose& P, RlyDrawn& D)
{
    const float TWO_PI = 6.2831853f;
    float wax = 0.0f, waz = 0.0f;
    g_rlyPlacing = true;
    mesh_wobble(wobble_of(o), P.x, P.z, &wax, &waz);
    g_rlyPlacing = false;
    float Rx0[3][3], Rz0[3][3], W[3][3], Y[3][3], Bp[3][3], Br[3][3], body[3][3], t1[3][3], R[3][3];
    rly_rx(wax, Rx0);
    rly_rz(waz, Rz0);
    rly_mul3(Rz0, Rx0, W);
    const float yawAngle = -P.yaw * TWO_PI / 256.0f;
    rly_ry(yawAngle, Y);
    rly_rx(P.pitch, Bp);      /* nose up: the forward axis (-Z) turns toward +Y */
    rly_rz(-P.roll, Br);      /* right side down: the right axis (+X) turns toward -Y */
    rly_mul3(Bp, Br, body);
    rly_mul3(Y, body, t1);
    rly_mul3(W, t1, R);
    float sA = R[2][1];
    if (sA > 1.0f) sA = 1.0f;
    if (sA < -1.0f) sA = -1.0f;
    const float A = asinf(sA);
    float B, C;
    if (cosf(A) > 1e-3f) {
        B = atan2f(-R[0][1], R[1][1]);
        C = atan2f(-R[2][0], R[2][2]);
    } else {
        /* standing on its nose or tail: keep the yaw and take the rest as the Z lean */
        C = yawAngle;
        float Yi[3][3], N[3][3];
        rly_ry(-C, Yi);
        rly_mul3(R, Yi, N);
        B = atan2f(N[1][0], N[0][0]);
    }
    const float face = heading_wrap(-C * 256.0f / TWO_PI);
    float delta = fmodf(P.tur - P.yaw + 128.0f, 256.0f);
    if (delta < 0.0f) delta += 256.0f;
    delta -= 128.0f;
    const float tface = heading_wrap(face + delta);
    const float half = rly_half_height(o);
    const float lift = -half, rise = P.h + half;
    o.wx = P.x;
    o.wz = P.z;
    o.face = ((int)floorf(face)) & 255;
    o.tface = ((int)floorf(tface)) & 255;
    D.id = o.id;
    D.face = (float)o.face + (face - floorf(face));
    D.tface = (float)o.tface + (tface - floorf(tface));
    D.height = P.h;
    D.lift = lift;
    D.rise = rise;
    /* the authored-model matrix lifts straight up after its lean, so it is handed the
       same displacement as one world vector: the leaned up axis times the lift, plus
       the rise */
    D.lx = R[0][1] * lift;
    D.ly = R[1][1] * lift + rise;
    D.lz = R[2][1] * lift;
    D.cx = P.x; D.cz = P.z;
    D.ax = A; D.az = B;
    D.ghost = false;
}

/* ---- the camera ------------------------------------------------------------------- */

/* Where the camera looks, in cells, and the racer it follows (-1 for a grid slot). */
static bool rly_camera_target(float* tx, float* tz, int* who)
{
    const RallyState& S = g_rly.cur;
    *who = -1;
    int follow = S.local;
    const RallyRacer* me = rly_local();
    if (S.phase == RALLY_PHASE_RACE && me && (me->flags & RALLY_F_FINISHED)) {
        /* finished: watch whoever is leading the racers still on the course */
        int best = -1;
        for (int i = 0; i < S.racer_count; i++) {
            if (S.racer[i].flags & RALLY_F_FINISHED) continue;
            if (best < 0 || S.racer[i].place < S.racer[best].place) best = i;
        }
        if (best >= 0) follow = best;
    } else if (!me) {
        int best = -1;
        for (int i = 0; i < S.racer_count; i++)
            if (best < 0 || S.racer[i].place < S.racer[best].place) best = i;
        follow = best;
    }
    if (follow >= 0 && follow < S.racer_count && (S.racer[follow].x || S.racer[follow].y)) {
        const RlyPose P = rly_pose(follow);
        *tx = P.x; *tz = P.z; *who = follow;
        return true;
    }
    const int slot = (S.local >= 0 && S.local < g_rly.gridN) ? S.local : 0;
    if (g_rly.gridN > 0) { *tx = g_rly.grid[slot][0]; *tz = g_rly.grid[slot][1]; return true; }
    if (g_rly.courseN > 0) {
        *tx = g_rly.course[0].x / 65536.0f; *tz = g_rly.course[0].y / 65536.0f;
        return true;
    }
    return false;
}

/* A critically damped spring on the look-at point: no overshoot and no jitter, whatever
   the frame rate. The target leads the racer by half a second of its own travel. */
static void rly_camera(bool timed)
{
    float tx, tz;
    int who;
    if (!rly_camera_target(&tx, &tz, &who)) return;
    if (who >= 0 && g_rly.cur.phase == RALLY_PHASE_RACE) {
        const RlyPose P = rly_pose(who);
        const float tps = (float)rly_tps();
        float lx = P.vx * tps * 0.5f, lz = P.vz * tps * 0.5f;
        const float l = sqrtf(lx * lx + lz * lz);
        if (l > 4.0f) { lx *= 4.0f / l; lz *= 4.0f / l; }
        tx += lx; tz += lz;
    }
    const Uint64 now = SDL_GetPerformanceCounter();
    float dt = 0.0f;
    if (timed && g_rly.camCounter)
        dt = (float)((double)(now - g_rly.camCounter) / (double)SDL_GetPerformanceFrequency());
    g_rly.camCounter = timed ? now : 0;
    const float far2 = (tx - g_rly.camX) * (tx - g_rly.camX) + (tz - g_rly.camZ) * (tz - g_rly.camZ);
    if (!g_rly.camInit || !timed || dt <= 0.0f || dt > 0.5f || far2 > 144.0f) {
        g_rly.camX = tx; g_rly.camZ = tz;
        g_rly.camVX = g_rly.camVZ = 0.0f;
        g_rly.camInit = true;
    } else {
        const float w = 5.0f;
        int steps = (int)ceilf(dt / 0.01f);
        if (steps < 1) steps = 1;
        const float h = dt / (float)steps;
        for (int k = 0; k < steps; k++) {
            const float axx = w * w * (tx - g_rly.camX) - 2.0f * w * g_rly.camVX;
            const float azz = w * w * (tz - g_rly.camZ) - 2.0f * w * g_rly.camVZ;
            g_rly.camVX += axx * h; g_rly.camVZ += azz * h;
            g_rly.camX += g_rly.camVX * h; g_rly.camZ += g_rly.camVZ * h;
        }
    }
    g_camX = g_rly.camX;
    g_camZ = g_rly.camZ;
}

/* ---- per frame: the start of draw_frame, then after the movement smoothing -------- */
static void rly_frame_begin(void)
{
    if (!g_rly.armed) return;
    /* The editor's toggle and the Visuals page can rewrite both; the match keeps them.
       A switch that does not take is said once and not retried every frame. */
    g_sbOn = false;
    if (g_rlyGfxBefore >= 0 && fx_mode_of(&g_fx) != FX_MODE_ENHANCED && !g_rly.gfxStuck) {
        gmw_switch_quiet(FX_MODE_ENHANCED);
        if (fx_mode_of(&g_fx) != FX_MODE_ENHANCED) {
            g_rly.gfxStuck = true;
            fprintf(stderr, "rally: the Enhanced picture did not take; the race is drawn in "
                            "mode %d\n", fx_mode_of(&g_fx));
        }
    }
}

static void rly_frame(int fbw, int fbh)
{
    (void)fbw; (void)fbh;
    g_rlyDraw = 0;
    const bool timed = g_rly.clockSet;
    g_rly.alpha = timed ? g_rly.clockAlpha : (g_rly.scriptAlpha >= 0.0f ? g_rly.scriptAlpha : 1.0f);
    g_rly.clockSet = false;
    if (!rly_live()) return;
    /* the frame's length, for the correction's blend: the live loop's clock, or a scripted
       drive's own (rallydrive), or none */
    float dt = 0.0f;
    {
        const Uint64 now = SDL_GetPerformanceCounter();
        if (timed && g_rly.predCounter)
            dt = (float)((double)(now - g_rly.predCounter) / (double)SDL_GetPerformanceFrequency());
        g_rly.predCounter = timed ? now : 0;
        if (dt > 0.25f) dt = 0.25f;
        if (!timed && g_rly.scriptDt > 0.0f) dt = g_rly.scriptDt;
    }
    rly_predict_frame(dt);
    const RallyState& S = g_rly.cur;
    for (int i = 0; i < S.racer_count; i++) {
        const RallyRacer& r = S.racer[i];
        if (r.unit_id < 0 || (r.flags & RALLY_F_DEAD)) continue;
        SimObject* o = NULL;
        for (size_t k = 0; k < g_objects.size(); k++)
            if (g_objects[k].kind == K_UNIT && g_objects[k].id == r.unit_id) { o = &g_objects[k]; break; }
        if (!o) continue;
        RlyDrawn& D = g_rlyDrawn[g_rlyDraw];
        rly_place(*o, rly_pose(i), D);
        /* CLOAKED: see-through to its owner, and to everybody else not there at all,
           which is the engine's own cloak arm (cloak_alpha, visible). */
        if (r.flags & RALLY_F_CLOAK) {
            o->cloak = 2;
            o->cstage = 38;
            D.ghost = g_playerHouse[0] && !strcmp(o->house, g_playerHouse);
        }
        g_rlyDraw++;
    }
    rly_camera(timed);
}

/* The player's own cloaked racer, after everything translucent: a constant share of the
   model over what is behind it, the way a dived submarine of the player's is drawn. Its
   solid and its cut-out parts are both drawn here and in no other pass (the frame's own
   solid, shadow, cut-out and translucent passes and the sun's shadow map all step over
   it, rly_defer_opaque), so no part of it is solid and it casts no shadow. */
static void rly_draw_ghosts(void)
{
    bool any = false;
    for (int i = 0; i < g_rlyDraw; i++) any = any || g_rlyDrawn[i].ghost;
    if (!any) return;
    EVHCPFNBLENDCOLOR blendColor = evhc_glBlendColor();
    if (!blendColor) return;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    blendColor(0.0f, 0.0f, 0.0f, 0.40f);
    glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    for (int pass = 0; pass < 2; pass++) {
        /* the cut-out parts keep their own shape test, as the frame's cut-out pass has it */
        if (pass) { glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.5f); }
        for (size_t k = 0; k < g_objects.size(); k++) {
            const SimObject& o = g_objects[k];
            if (!rly_defer_opaque(o) || !visible(o)) continue;
            const int mi = world_mesh_for(o);
            if (mi >= 0) draw_object_mesh(mi, o, pass ? MODE_CUTOUT : MODE_OPAQUE);
        }
        if (pass) glDisable(GL_ALPHA_TEST);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
}

/* ---- the ground: the start and finish lines --------------------------------------- */
static void rly_checker_strip(float x1, float z1, float x2, float z2, float width)
{
    const float dx = x2 - x1, dz = z2 - z1;
    const float len = sqrtf(dx * dx + dz * dz);
    if (len < 0.05f) return;
    const float ax = dx / len, az = dz / len;     /* along the line */
    const float nx = -az, nz = ax;                /* across it */
    const float sq = 0.25f;
    const int nAlong = (int)ceilf(len / sq);
    const int nAcross = (int)ceilf(width / sq);
    const float stepA = len / (float)nAlong, stepW = width / (float)nAcross;
    glBegin(GL_QUADS);
    for (int i = 0; i < nAlong; i++)
        for (int j = 0; j < nAcross; j++) {
            const float c = ((i + j) & 1) ? 0.95f : 0.08f;
            glColor4f(c, c, c, 0.92f);
            const float a0 = i * stepA, a1 = a0 + stepA;
            const float w0 = -width * 0.5f + j * stepW, w1 = w0 + stepW;
            const float px[4] = { x1 + ax * a0 + nx * w0, x1 + ax * a1 + nx * w0,
                                  x1 + ax * a1 + nx * w1, x1 + ax * a0 + nx * w1 };
            const float pz[4] = { z1 + az * a0 + nz * w0, z1 + az * a1 + nz * w0,
                                  z1 + az * a1 + nz * w1, z1 + az * a0 + nz * w1 };
            for (int v = 0; v < 4; v++)
                glVertex3f(px[v], terrain_y(px[v], pz[v]) + 0.03f, pz[v]);
        }
    glEnd();
}

static void rly_draw_ground(void)
{
    if (!rly_live()) return;
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);    /* the frame's own test, whatever the pass before left */
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);
    /* THE FINISH: the course's own segment, or across the last course point */
    if (g_rly.haveFinish) {
        rly_checker_strip(g_rly.finish[0], g_rly.finish[1], g_rly.finish[2], g_rly.finish[3], 0.5f);
    } else if (g_rly.courseN >= 2) {
        const RallyCoursePoint& a = g_rly.course[g_rly.courseN - 2];
        const RallyCoursePoint& b = g_rly.course[g_rly.courseN - 1];
        float dx = (b.x - a.x) / 65536.0f, dz = (b.y - a.y) / 65536.0f;
        const float l = sqrtf(dx * dx + dz * dz);
        if (l > 1e-3f) {
            dx /= l; dz /= l;
            const float hw = b.half_width / 65536.0f, cx = b.x / 65536.0f, cz = b.y / 65536.0f;
            rly_checker_strip(cx + dz * hw, cz - dx * hw, cx - dz * hw, cz + dx * hw, 0.5f);
        }
    }
    /* THE START: across the front of the grid, as wide as the grid and the track */
    if (g_rly.gridN > 0) {
        const float ang = g_rly.gridFacing * 6.2831853f / 256.0f;
        const float fx = sinf(ang), fz = -cosf(ang);         /* DirType 0 is north, -Z */
        const float sx = -fz, sz = fx;                       /* across the grid */
        float front = -1e9f, lo = 1e9f, hi = -1e9f, mx = 0.0f, mz = 0.0f;
        for (int k = 0; k < g_rly.gridN; k++) {
            const float gx = g_rly.grid[k][0], gz = g_rly.grid[k][1];
            front = std::max(front, gx * fx + gz * fz);
            lo = std::min(lo, gx * sx + gz * sz);
            hi = std::max(hi, gx * sx + gz * sz);
            mx += gx; mz += gz;
        }
        mx /= g_rly.gridN; mz /= g_rly.gridN;
        float hw = 0.5f * (hi - lo) + 0.75f;
        if (g_rly.courseN > 0) hw = std::max(hw, g_rly.course[0].half_width / 65536.0f);
        const float along = front + 0.75f - (mx * fx + mz * fz);
        const float cx = mx + fx * along, cz = mz + fz * along;
        rly_checker_strip(cx - sx * hw, cz - sz * hw, cx + sx * hw, cz + sz * hw, 0.5f);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

/* ---- the world: crates, hazards and smoke ------------------------------------------- */
static void rly_box(float x, float y, float z, float hx, float hy, float hz, float yaw,
                    float r, float g, float b)
{
    const float c = cosf(yaw), s = sinf(yaw);
    float px[8], py[8], pz[8];
    for (int k = 0; k < 8; k++) {
        const float lx = (k & 1) ? hx : -hx, ly = (k & 2) ? hy : -hy, lz = (k & 4) ? hz : -hz;
        px[k] = x + lx * c + lz * s;
        pz[k] = z - lx * s + lz * c;
        py[k] = y + ly;
    }
    static const int F[6][4] = { { 2, 3, 7, 6 }, { 0, 1, 5, 4 }, { 1, 3, 7, 5 },
                                 { 0, 2, 6, 4 }, { 4, 5, 7, 6 }, { 0, 1, 3, 2 } };
    static const float shade[6] = { 1.0f, 0.45f, 0.8f, 0.6f, 0.7f, 0.65f };
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++) {
        glColor4f(r * shade[f], g * shade[f], b * shade[f], 1.0f);
        for (int v = 0; v < 4; v++) glVertex3f(px[F[f][v]], py[F[f][v]], pz[F[f][v]]);
    }
    glEnd();
}

static void rly_ring(float x, float z, float radius, float lift, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_LINE_LOOP);
    for (int k = 0; k < 32; k++) {
        const float t = k * 6.2831853f / 32.0f;
        const float px = x + cosf(t) * radius, pz = z + sinf(t) * radius;
        glVertex3f(px, terrain_y(px, pz) + lift, pz);
    }
    glEnd();
}

static void rly_disc(float x, float z, float radius, float lift, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(x, terrain_y(x, z) + lift, z);
    for (int k = 0; k <= 16; k++) {
        const float t = k * 6.2831853f / 16.0f;
        const float px = x + cosf(t) * radius, pz = z + sinf(t) * radius;
        glVertex3f(px, terrain_y(px, pz) + lift, pz);
    }
    glEnd();
}

/* A crate the engine is not already drawing as one of its own overlays. */
static bool rly_engine_crate_at(int cx, int cy)
{
    for (size_t i = 0; i < g_crates.size(); i++)
        if (g_crates[i].x == cx && g_crates[i].y == cy) return true;
    return false;
}

/* A HAZARD BETWEEN TWO TICKS, the way rly_pose places a racer, so a rocket flies as
   smoothly as the racer that fired it and keeps its place beside it. Its copy in the
   previous state is the one of the same kind and owner whose age is one tick less (an
   Ion Cannon mark counts down, so one tick more): the list closes up when one is
   removed, so the index alone does not follow a hazard. One with no such copy, new this
   tick, is drawn where it is now. Cells, and the yaw in radians as rly_box takes it. */
static void rly_hazard_at(int i, float* x, float* z, float* up, float* yaw)
{
    const RallyHazard& c = g_rly.cur.hazard[i];
    const RallyHazard* p = NULL;
    if (g_rly.havePrev && c.kind != RALLY_HAZ_MINE) {
        const int want = c.kind == RALLY_HAZ_ION_MARK ? c.ticks + 1 : c.ticks - 1;
        const int n = std::min(g_rly.prev.hazard_count, (int32_t)RALLY_MAX_HAZARDS);
        for (int k = 0; k < n && !p; k++) {
            const RallyHazard& q = g_rly.prev.hazard[k];
            if (q.kind == c.kind && q.owner == c.owner && q.ticks == want) p = &q;
        }
    }
    float t = g_rly.alpha;
    if (!p || abs(c.x - p->x) > 2 * RALLY_FINE_PER_CELL || abs(c.y - p->y) > 2 * RALLY_FINE_PER_CELL) {
        p = &c;
        t = 1.0f;
    }
    const double F = 1.0 / RALLY_FINE_PER_CELL;
    *x = (float)((p->x + (c.x - p->x) * (double)t) * F);
    *z = (float)((p->y + (c.y - p->y) * (double)t) * F);
    *up = (float)((p->z + (c.z - p->z) * (double)t) * F);
    *yaw = -rly_angle_mix(p->yaw & 65535, c.yaw & 65535, t) * 6.2831853f / 65536.0f;
}

static void rly_draw_world(void)
{
    if (!rly_live()) return;
    const RallyState& S = g_rly.cur;
    const float tps = (float)rly_tps();
    const float t = (float)g_engineFrame + g_rly.alpha;   /* in ticks, for the blinking */
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    /* CRATES: a wooden box turning slowly */
    for (int i = 0; i < S.crate_count; i++) {
        const RallyCrate& c = S.crate[i];
        if (!c.active) continue;
        const float x = c.x / 65536.0f, z = c.y / 65536.0f;
        if (rly_engine_crate_at((int)x, (int)z)) continue;
        const float bob = 0.04f * sinf(t / tps * 3.0f + i);
        rly_box(x, terrain_y(x, z) + 0.2f + bob, z, 0.13f, 0.13f, 0.13f, t / tps * 1.5f + i,
                0.72f, 0.52f, 0.28f);
    }
    for (int i = 0; i < S.hazard_count; i++) {
        const RallyHazard& h = S.hazard[i];
        float x, z, up, yaw;
        rly_hazard_at(i, &x, &z, &up, &yaw);
        switch (h.kind) {
        case RALLY_HAZ_MINE: {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            rly_disc(x, z, 0.17f, 0.05f, 0.12f, 0.12f, 0.10f, 1.0f);
            const bool on = ((int)(t / (tps * 0.25f))) & 1;
            rly_disc(x, z, 0.05f, 0.07f, on ? 1.0f : 0.35f, 0.08f, 0.05f, 1.0f);
            glDisable(GL_BLEND);
            break;
        }
        case RALLY_HAZ_ROCKET:
        case RALLY_HAZ_HOMING: {
            const float y = terrain_y(x, z) + up + 0.25f;
            const bool homing = h.kind == RALLY_HAZ_HOMING;
            rly_box(x, y, z, 0.035f, 0.035f, 0.14f, yaw,
                    homing ? 0.85f : 0.70f, homing ? 0.20f : 0.70f, homing ? 0.15f : 0.65f);
            /* the flame at the tail */
            const float fx = x - sinf(-yaw) * 0.17f, fz = z + cosf(-yaw) * 0.17f;
            rly_box(fx, y, fz, 0.03f, 0.03f, 0.03f, yaw, 1.0f, 0.75f, 0.2f);
            break;
        }
        case RALLY_HAZ_ION_MARK: {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            const float pulse = 0.5f + 0.5f * sinf(t / tps * 12.0f);
            const float left = (float)h.ticks / tps;            /* seconds to the strike */
            const float rad = 0.5f + 0.3f * (left > 3.0f ? 1.0f : left / 3.0f);
            rly_ring(x, z, rad, 0.06f, 0.45f, 0.75f, 1.0f, 0.5f + 0.5f * pulse);
            rly_ring(x, z, rad * 0.55f, 0.06f, 0.45f, 0.75f, 1.0f, 0.3f + 0.4f * pulse);
            if (left < 0.5f) {
                const float g = terrain_y(x, z);
                glBegin(GL_QUADS);
                glColor4f(0.6f, 0.85f, 1.0f, 0.55f);
                glVertex3f(x - 0.12f, g, z); glVertex3f(x + 0.12f, g, z);
                glVertex3f(x + 0.12f, g + 8.0f, z); glVertex3f(x - 0.12f, g + 8.0f, z);
                glVertex3f(x, g, z - 0.12f); glVertex3f(x, g, z + 0.12f);
                glVertex3f(x, g + 8.0f, z + 0.12f); glVertex3f(x, g + 8.0f, z - 0.12f);
                glEnd();
            }
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
            break;
        }
        default: break;
        }
    }
    /* SMOKE: grey puffs, thick in the middle and gone at the rim, that grow and fade over
       a second, flat to the view */
    if (!g_rly.smoke.empty()) {
        float mv[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, mv);
        const float rx = mv[0], ry = mv[4], rz = mv[8];    /* the view's right, in the world */
        const float ux = mv[1], uy = mv[5], uz = mv[9];    /* and its up */
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (size_t i = 0; i < g_rly.smoke.size(); i++) {
            const RlySmoke& p = g_rly.smoke[i];
            const float age = ((float)(g_engineFrame - p.frame) + g_rly.alpha) / tps;
            if (age < 0.0f || age > 1.0f) continue;
            const float r = 0.07f + 0.18f * age, a = 0.6f * (1.0f - age);
            const float y = terrain_y(p.x, p.z) + p.y + 0.25f + 0.2f * age;
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(0.66f, 0.64f, 0.60f, a);
            glVertex3f(p.x, y, p.z);
            glColor4f(0.66f, 0.64f, 0.60f, 0.0f);
            for (int k = 0; k <= 10; k++) {
                const float t = k * 6.2831853f / 10.0f, c = cosf(t) * r, s = sinf(t) * r;
                glVertex3f(p.x + rx * c + ux * s, y + ry * c + uy * s, p.z + rz * c + uz * s);
            }
            glEnd();
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

/* ---- the art the HUD draws --------------------------------------------------------- */
static GLuint rly_upload(const unsigned char* rgba, int w, int h)
{
    int tw = 1, th = 1;
    while (tw < w) tw <<= 1;
    while (th < h) th <<= 1;
    std::vector<unsigned char> pot((size_t)tw * th * 4, 0);
    for (int y = 0; y < h; y++)
        memcpy(&pot[(size_t)y * tw * 4], rgba + (size_t)y * w * 4, (size_t)w * 4);
    return sb_upload(tw, th, &pot[0]);
}

static GLuint rly_indexed(const unsigned char* px, int w, int h)
{
    std::vector<unsigned char> rgba((size_t)w * h * 4, 0);
    for (int i = 0; i < w * h; i++) {
        const unsigned char k = px[i];
        if (!k) continue;
        memcpy(&rgba[(size_t)i * 4], rally_icon_palette[k], 4);
    }
    return rly_upload(&rgba[0], w, h);
}

static void rly_make_icons(void)
{
    if (g_rly.iconsMade) return;
    g_rly.iconsMade = true;
    for (int i = 0; i < RALLY_ITEM_COUNT; i++)
        g_rly.iconTex[i] = rly_indexed(rally_icon_pixels[i], RALLY_ICON_W, RALLY_ICON_H);
    g_rly.crossTex = rly_indexed(rally_crosshair_pixels, RALLY_CROSSHAIR_W, RALLY_CROSSHAIR_H);
    g_rly.flagTex = rly_indexed(rally_flag_pixels, RALLY_FLAG_W, RALLY_FLAG_H);
}

/* A texture of w x h art (inside its power-of-two texture) at whole-pixel scale. */
static void rly_blit(GLuint tex, int w, int h, float x, float y, int scale, float alpha = 1.0f)
{
    if (!tex) return;
    int tw = 1, th = 1;
    while (tw < w) tw <<= 1;
    while (th < h) th <<= 1;
    const float u = (float)w / tw, v = (float)h / th;
    x = floorf(x); y = floorf(y);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1, 1, 1, alpha);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(u, 0); glVertex2f(x + w * scale, y);
    glTexCoord2f(u, v); glVertex2f(x + w * scale, y + h * scale);
    glTexCoord2f(0, v); glVertex2f(x, y + h * scale);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

/* The vehicle's own cameo, 64 x 48: the true-colour art or the 1995 shape behind it
   (eh_cameo), and failing both the cartridge's cameo set. */
static bool rly_cameo(int rosterIndex, float x, float y, int scale)
{
    if (rosterIndex < 0 || rosterIndex >= g_rly.rosterN) return false;
    const RlyVehicle* v = rly_vehicle(g_rly.roster[rosterIndex]);
    if (!v) return false;
    if (g_rly.cameoState[rosterIndex] == 0) {
        g_rly.cameoState[rosterIndex] = -1;
        static unsigned char rgba[64 * 48 * 4];
        SbEntry e = SbEntry();
        snprintf(e.name, sizeof e.name, "%s", v->code);
        eh_cameo(rgba, v->code, &e);
        bool any = false;
        for (int i = 0; i < 64 * 48 && !any; i++) any = rgba[i * 4 + 3] != 0;
        if (any) {
            g_rly.cameoTex[rosterIndex] = rly_upload(rgba, 64, 48);
            g_rly.cameoState[rosterIndex] = 1;
        }
    }
    if (g_rly.cameoState[rosterIndex] == 1) {
        rly_blit(g_rly.cameoTex[rosterIndex], 64, 48, x, y, scale);
        return true;
    }
    const SbTex* t = sb_cameo(v->code);
    if (!t || !t->gl) t = sb_cameo_true(v->code);
    if (!t || !t->gl) {
        sb_rect(x, y, x + 64 * scale, y + 48 * scale, 0.16f, 0.14f, 0.11f, 1.0f);
        sb_text(x + (64 * scale - sb_text_w(v->code, scale)) / 2, y + 18 * scale, v->code, scale,
                0.94f, 0.91f, 0.83f);
        return true;
    }
    const float u = (t->uw > 0 && t->w > 0) ? (float)t->uw / t->w : 1.0f;
    const float vv = (t->uh > 0 && t->h > 0) ? (float)t->uh / t->h : 1.0f;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, t->gl);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);  glVertex2f(x, y);
    glTexCoord2f(u, 0);  glVertex2f(x + 64 * scale, y);
    glTexCoord2f(u, vv); glVertex2f(x + 64 * scale, y + 48 * scale);
    glTexCoord2f(0, vv); glVertex2f(x, y + 48 * scale);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    return true;
}

static int rly_roster_of_type(int unitType)
{
    for (int i = 0; i < g_rly.rosterN; i++)
        if (g_rly.roster[i] == unitType) return i;
    return -1;
}

/* ---- the HUD ----------------------------------------------------------------------- */

/* THE HUD'S ZOOM. Every panel of the race is drawn at one whole-number zoom, its art and
   its text alike, and that zoom comes from the window: the race's text is the 1995
   six-point font, which needs about twice its size to be read at a desk, where the
   game's own Enhanced HUD (sb_scale, from its 693-row chrome) is at 1x up to 1385 rows.
   Never below the game's own zoom, and divided like it by the UI Scale setting. Each
   panel then takes the largest zoom up to this one at which it fits (rly_fit). */
static int rly_scale(int fbw, int fbh)
{
    sb_layout(fbw, fbh);
    int s = std::min(fbw / 400, fbh / 300);
    if (g_sbUiDiv > 1) s = (s + g_sbUiDiv / 2) / g_sbUiDiv;
    return std::max(std::max(1, s), sb_scale());
}

/* The largest whole zoom, from s0 down to 1, at which a panel of w1 x h1 (its size at
   1x) fits the window with a small margin. */
static int rly_fit(int s0, int w1, int h1, int fbw, int fbh)
{
    int s = std::max(1, s0);
    while (s > 1 && (w1 * s > fbw - 8 || h1 * s > fbh - 8)) s--;
    return s;
}

/* The race's own panels at the HUD's zoom, unless the place panel and the item slot
   would meet across the top (344 HUD pixels) or the progress strip would have no length
   (172, under a circuit's taller place panel): then the largest zoom at which they do not. */
static int rly_race_scale(int fbw, int fbh)
{
    return rly_fit(rly_scale(fbw, fbh), 344, 172, fbw, fbh);
}

static void rly_panel(float x0, float y0, float x1, float y1, float alpha = 0.92f)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    sb_rect(x0, y0, x1, y1, 0.085f, 0.08f, 0.075f, alpha);
    sb_frame(x0, y0, x1, y1, 0.63f, 0.53f, 0.32f, 1.0f);
}

static void rly_text_c(float cx, float y, const char* s, int scale, float r, float g, float b)
{
    sb_text(cx - sb_text_w(s, (float)scale) * 0.5f, y, s, (float)scale, r, g, b);
}

static const char* rly_ordinal(int place)
{
    static char buf[8];
    const char* suf = "TH";
    if (place % 100 < 11 || place % 100 > 13) {
        if (place % 10 == 1) suf = "ST";
        else if (place % 10 == 2) suf = "ND";
        else if (place % 10 == 3) suf = "RD";
    }
    snprintf(buf, sizeof buf, "%d%s", place, suf);
    return buf;
}

static void rly_time(int ticks, char* out, size_t n)
{
    const int tps = rly_tps();
    if (ticks < 0) { snprintf(out, n, "--:--.-"); return; }
    const int tenths = (int)((long long)ticks * 10 / tps);
    snprintf(out, n, "%02d:%02d.%d", tenths / 600, (tenths / 10) % 60, tenths % 10);
}

static void rly_bar(float x, float y, int segs, int filled, int scale, float r, float g, float b)
{
    for (int k = 0; k < segs; k++) {
        const float sx = x + k * 5 * scale;
        if (k < filled) sb_rect(sx, y, sx + 4 * scale, y + 5 * scale, r, g, b, 1.0f);
        else sb_rect(sx, y, sx + 4 * scale, y + 5 * scale, 0.22f, 0.20f, 0.17f, 1.0f);
    }
}

/* RACING AGAIN, the vehicle this seat drove last race (RallyRacer.last_vehicle), which the
   chooser preselects and keeps if nothing else is chosen; -1 in a match's first race. */
static int rly_keep_vehicle(void)
{
    const RallyRacer* me = rly_local();
    if (!me || g_rly.cur.race_number <= 1) return -1;
    return (me->last_vehicle >= 0 && me->last_vehicle < g_rly.rosterN) ? me->last_vehicle : -1;
}

/* THE CHOOSER'S GEOMETRY, one function for the draw and the click so they cannot part.
   Four columns and as many rows as the roster needs, up to six (RALLY_MAX_ROSTER). */
struct RlyPickLayout {
    int s, x, y, w, h;
    int gx, gy, cols, rows, cellW, cellH;
    int infoX;
    int seatsY;
};
static RlyPickLayout rly_pick_layout(int fbw, int fbh)
{
    RlyPickLayout L;
    L.cols = 4;
    L.rows = std::max(1, std::min(6, (g_rly.rosterN + 3) / 4));
    const int seatRows = (std::min(g_rly.cur.racer_count, (int32_t)RALLY_MAX_RACERS) + 1) / 2;
    const int gridH1 = std::max(L.rows * 52, 96);
    const int w1 = 10 + L.cols * 68 + 10 + 132 + 10;
    const int h1 = 26 + gridH1 + 8 + seatRows * 13 + 10;
    L.s = rly_fit(rly_scale(fbw, fbh), w1, h1, fbw, fbh);
    const int s = L.s;
    L.cellW = 68 * s; L.cellH = 52 * s;
    L.w = w1 * s;
    L.h = h1 * s;
    L.x = (fbw - L.w) / 2;
    L.y = std::max(4, (fbh - L.h) / 2);
    L.gx = L.x + 10 * s; L.gy = L.y + 26 * s;
    L.infoX = L.gx + L.cols * L.cellW + 10 * s;
    L.seatsY = L.gy + gridH1 * s + 8 * s;
    return L;
}
static int rly_pick_hit(const RlyPickLayout& L, float c, float r)
{
    if (c < L.gx || r < L.gy) return -1;
    const int col = (int)((c - L.gx) / L.cellW), row = (int)((r - L.gy) / L.cellH);
    if (col < 0 || col >= L.cols || row < 0 || row >= L.rows) return -1;
    const int i = row * L.cols + col;
    return i < g_rly.rosterN ? i : -1;
}

/* What the chooser's seat list said beside each seat the last time it was drawn, for
   rallystate: READY, CHOOSING or OUT. */
static char g_rlyPickWord[RALLY_MAX_RACERS][12];
static int g_rlyPickWords = 0;

static void rly_draw_pick(int fbw, int fbh)
{
    g_rlyPickWords = 0;
    const RallyState& S = g_rly.cur;
    const RlyPickLayout L = rly_pick_layout(fbw, fbh);
    const int s = L.s;
    const RallyRacer* me = rly_local();
    const int mine = me ? rly_roster_of_type(me->unit_type) : -1;
    const int keep = rly_keep_vehicle();
    /* opaque: the checkered start line under it must not show through a modal panel */
    rly_panel(L.x, L.y, L.x + L.w, L.y + L.h, 1.0f);
    sb_text(L.x + 10 * s, L.y + 8 * s, "CHOOSE YOUR VEHICLE", s, 1.0f, 0.85f, 0.54f);
    if (S.phase_ticks > 0) {
        char secs[16];
        snprintf(secs, sizeof secs, "%d", (S.phase_ticks + rly_tps() - 1) / rly_tps());
        sb_text(L.x + L.w - 10 * s - sb_text_w(secs, s), L.y + 8 * s, secs, s, 1.0f, 0.85f, 0.54f);
    }
    const int hover = rly_pick_hit(L, g_mouseScrC, g_mouseScrR);
    g_rly.hover = hover;
    for (int i = 0; i < g_rly.rosterN && i < L.cols * L.rows; i++) {
        const int cx = L.gx + (i % L.cols) * L.cellW, cy = L.gy + (i / L.cols) * L.cellH;
        /* the vehicle this seat has, or asked for, or (racing again) drove last time */
        const bool hot = i == hover,
                   chosen = i == mine || (mine < 0 && i == g_rly.asked)
                            || (mine < 0 && g_rly.asked < 0 && i == keep);
        sb_rect(cx, cy, cx + 66 * s, cy + 50 * s, hot ? 0.30f : 0.15f, hot ? 0.26f : 0.13f,
                hot ? 0.19f : 0.10f, 1.0f);
        rly_cameo(i, cx + s, cy + s, s);
        sb_frame(cx, cy, cx + 66 * s, cy + 50 * s, chosen ? 1.0f : (hot ? 0.9f : 0.47f),
                 chosen ? 0.85f : (hot ? 0.75f : 0.37f), chosen ? 0.30f : (hot ? 0.45f : 0.23f), 1.0f);
        if (chosen) sb_frame(cx + s, cy + s, cx + 65 * s, cy + 49 * s, 1.0f, 0.85f, 0.30f, 1.0f);
    }
    /* THE VEHICLE UNDER THE POINTER, or the one picked */
    const int shown = hover >= 0 ? hover : (mine >= 0 ? mine : (g_rly.asked >= 0 ? g_rly.asked
                                                                : (keep >= 0 ? keep : 0)));
    if (shown >= 0 && shown < g_rly.rosterN) {
        const RlyVehicle* v = rly_vehicle(g_rly.roster[shown]);
        if (v) {
            int y = L.gy;
            sb_text(L.infoX, y, v->name, s, 0.94f, 0.91f, 0.83f);
            y += 18 * s;
            const char* lab[3] = { "SPEED", "HANDLING", "ARMOUR" };
            int val[3];
            rly_bars(*v, val);
            for (int k = 0; k < 3; k++) {
                sb_text(L.infoX, y, lab[k], s, 0.75f, 0.70f, 0.60f);
                rly_bar(L.infoX, y + 12 * s, 10, val[k], s, 0.95f, 0.78f, 0.30f);
                y += 24 * s;
            }
            /* RACING AGAIN: last race's vehicle is kept unless another is chosen */
            if (keep >= 0 && g_rly.asked < 0 && (mine < 0 || mine == keep))
                sb_text(L.infoX, y + 2 * s, "ENTER KEEPS IT", s, 1.0f, 0.85f, 0.54f);
        }
    }
    /* EVERY SEAT, and whether it has chosen */
    for (int r = 0; r < S.racer_count && r < RALLY_MAX_RACERS; r++) {
        const int x = L.x + 10 * s + (r % 2) * (L.w - 20 * s) / 2;
        const int y = L.seatsY + (r / 2) * 13 * s;
        const RlySeat& seat = g_rly.seat[r];
        sb_rect(x, y + 2 * s, x + 7 * s, y + 9 * s, seat.r, seat.g, seat.b, 1.0f);
        char name[16];
        snprintf(name, sizeof name, "%s", seat.name);
        sb_text(x + 10 * s, y, name, s, r == S.local ? 1.0f : 0.94f, r == S.local ? 0.85f : 0.91f,
                r == S.local ? 0.54f : 0.83f);
        /* A seat that left the match (destroyed, disconnected or resigned) keeps the
           vehicle it last raced, so it reads as picked, and RACE AGAIN keeps it out of
           the race: it is OUT here as on the results, never READY beside the drivers. */
        const bool out = (S.racer[r].flags & RALLY_F_RETIRED) != 0;
        const bool picked = !out && S.racer[r].unit_type >= 0;
        const char* st = out ? "OUT" : picked ? "READY" : "CHOOSING";
        sb_text(x + (L.w - 20 * s) / 2 - 12 * s - sb_text_w(st, s), y, st, s,
                picked ? 0.55f : (out ? 0.75f : 0.6f), picked ? 0.9f : (out ? 0.70f : 0.57f),
                picked ? 0.45f : (out ? 0.60f : 0.5f));
        snprintf(g_rlyPickWord[r], sizeof g_rlyPickWord[r], "%s", st);
        g_rlyPickWords = r + 1;
    }
}

/* THE START. The brain's countdown is four seconds and the race begins the tick it runs
   out, so GO lands on the start: READY, then 3, 2, 1 for the last three seconds of the
   countdown, and GO for the first second of the race. */
static void rly_draw_countdown(int fbw, int fbh)
{
    const RallyState& S = g_rly.cur;
    const int tps = rly_tps();
    const char* word = NULL;
    float r = 1, g = 1, b = 1;
    int size = 6;
    if (S.phase == RALLY_PHASE_COUNTDOWN) {
        const int left = (S.phase_ticks + tps - 1) / tps;     /* whole seconds to go */
        if (left >= 4)      { word = "READY"; r = 1.0f; g = 0.85f; b = 0.54f; size = 4; }
        else if (left == 3) { word = "3"; r = 1.0f; g = 0.30f; b = 0.25f; }
        else if (left == 2) { word = "2"; r = 1.0f; g = 0.60f; b = 0.20f; }
        else                { word = "1"; r = 1.0f; g = 0.90f; b = 0.30f; }
    } else if (S.phase == RALLY_PHASE_RACE && S.race_ticks < tps) {
        word = "GO!"; r = 0.45f; g = 1.0f; b = 0.40f;
    }
    if (!word) return;
    /* above the middle of the screen, where the player's own vehicle is, and below the
       item slot */
    const int big = size * rly_race_scale(fbw, fbh);
    rly_text_c(fbw * 0.5f, fbh * 0.27f - 5.5f * big, word, big, r, g, b);
}

static void rly_draw_race(int fbw, int fbh)
{
    const RallyState& S = g_rly.cur;
    const int s = rly_race_scale(fbw, fbh);
    const RallyRacer* me = rly_local();
    const int tps = rly_tps();
    char buf[64];
    /* TOP LEFT: the place, the race time, the lap on a circuit and the health */
    const bool lapRow = S.course_loop || S.laps > 1;
    if (me) {
        const int x = 8 * s, y = 8 * s, w = 124 * s, h = (lapRow ? 70 : 58) * s;
        rly_panel(x, y, x + w, y + h);
        const int place = me->place > 0 ? me->place : 1;
        const float px = sb_text(x + 6 * s, y + 5 * s, rly_ordinal(place), 2 * s, 1.0f, 0.85f, 0.54f);
        snprintf(buf, sizeof buf, "/%d", S.racer_count);
        sb_text(px + 2 * s, y + 16 * s, buf, s, 0.94f, 0.91f, 0.83f);
        char t[24];
        rly_time((me->flags & RALLY_F_FINISHED) ? me->finish_ticks
                 : (S.phase == RALLY_PHASE_RACE ? S.race_ticks : 0), t, sizeof t);
        sb_text(x + 6 * s, y + 30 * s, t, s, 0.94f, 0.91f, 0.83f);
        /* THE LAP, its own row under the time, on a circuit (a point-to-point course is one
           run and says nothing): LAP 2/3, the lap being driven, gold on the last */
        if (lapRow) {
            const int laps = S.laps > 0 ? S.laps : 1;
            int lap = me->lap > 0 ? me->lap : 1;
            if (lap > laps) lap = laps;
            snprintf(buf, sizeof buf, "LAP %d/%d", lap, laps);
            const bool last = lap == laps && laps > 1 && !(me->flags & RALLY_F_FINISHED);
            sb_text(x + 6 * s, y + 43 * s, buf, s,
                    last ? 1.0f : 0.94f, last ? 0.85f : 0.91f, last ? 0.54f : 0.83f);
        }
        const float frac = me->health_max > 0 ? (float)me->health / me->health_max : 0.0f;
        const float f = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
        const float bx = x + 6 * s, by = y + (lapRow ? 57 : 45) * s, bw = (float)(w - 12 * s);
        sb_rect(bx, by, bx + bw, by + 6 * s, 0.15f, 0.13f, 0.10f, 1.0f);
        sb_rect(bx, by, bx + bw * f, by + 6 * s, f > 0.5f ? 0.30f : 1.0f, f > 0.25f ? 0.85f : 0.25f,
                0.20f, 1.0f);
        sb_frame(bx, by, bx + bw, by + 6 * s, 0.47f, 0.37f, 0.23f, 1.0f);
    }
    /* TOP CENTRE: the item slot */
    if (me) {
        const int w = 72 * s, h = 56 * s, x = (fbw - w) / 2, y = 8 * s;
        rly_panel(x, y, x + w, y + h);
        int item = me->item;
        if (me->flags & RALLY_F_ROULETTE)
            item = 1 + (int)((SDL_GetTicks() / 70) % (RALLY_ITEM_COUNT - 1));
        if (item < 0 || item >= RALLY_ITEM_COUNT) item = RALLY_ITEM_NONE;
        rly_blit(g_rly.iconTex[item], RALLY_ICON_W, RALLY_ICON_H, x + 4 * s, y + 4 * s, s);
        if (me->item_count > 1 && !(me->flags & RALLY_F_ROULETTE)) {
            snprintf(buf, sizeof buf, "X%d", me->item_count);
            sb_text(x + w - 4 * s - sb_text_w(buf, s), y + h - 14 * s, buf, s, 1.0f, 0.85f, 0.54f);
        }
        if (item != RALLY_ITEM_NONE && !(me->flags & RALLY_F_ROULETTE))
            rly_text_c(fbw * 0.5f, y + h + 3 * s, RLY_ITEM_NAME[item], s, 0.94f, 0.91f, 0.83f);
    }
    /* LEFT EDGE: the course from start (bottom) to finish (top), one marker a racer */
    {
        /* below the place panel and the flag over the strip, which the lap row makes taller */
        const int x = 10 * s, top = (lapRow ? 104 : 92) * s, bottom = fbh - 24 * s, w = 6 * s;
        if (bottom - top > 40 * s) {
            rly_blit(g_rly.flagTex, RALLY_FLAG_W, RALLY_FLAG_H, x + w / 2 - RALLY_FLAG_W * s / 2,
                     top - (RALLY_FLAG_H + 3) * s, s);
            glEnable(GL_BLEND);
            sb_rect(x, top, x + w, bottom, 0.10f, 0.09f, 0.08f, 0.85f);
            sb_frame(x, top, x + w, bottom, 0.63f, 0.53f, 0.32f, 1.0f);
            /* the strip is the whole race, so a circuit's laps are marked across it */
            for (int k = 1; k < S.laps && S.laps <= RALLY_LAPS_MAX; k++) {
                const float ly = bottom - (float)k / S.laps * (bottom - top);
                sb_rect(x - 2 * s, ly - (s > 1 ? s / 2 : 0), x + w + 2 * s, ly - (s > 1 ? s / 2 : 0) + s,
                        0.63f, 0.53f, 0.32f, 1.0f);
            }
            for (int pass = 0; pass < 2; pass++)
                for (int r = 0; r < S.racer_count && r < RALLY_MAX_RACERS; r++) {
                    if ((r == S.local) != (pass == 1)) continue;    /* the player's on top */
                    const RallyRacer& R = S.racer[r];
                    float p = (R.flags & RALLY_F_FINISHED) ? 1.0f : R.progress / 65536.0f;
                    p = p < 0 ? 0 : (p > 1 ? 1 : p);
                    const float my = bottom - p * (bottom - top);
                    const RlySeat& seat = g_rly.seat[r];
                    const float mw = (r == S.local ? 12 : 9) * s, mh = (r == S.local ? 6 : 4) * s;
                    const float mx = x + w / 2 - mw / 2 + (r == S.local ? 3 * s : 0);
                    sb_rect(mx, my - mh / 2, mx + mw, my + mh / 2, seat.r, seat.g, seat.b, 1.0f);
                    if (r == S.local) sb_frame(mx, my - mh / 2, mx + mw, my + mh / 2, 1, 1, 1, 1);
                    else sb_frame(mx, my - mh / 2, mx + mw, my + mh / 2, 0, 0, 0, 1);
                }
        }
    }
    /* CENTRE: what the player has to know now, above the player's vehicle, which the
       camera holds in the middle of the screen */
    if (me && S.phase == RALLY_PHASE_RACE) {
        /* below the item slot and its name (8 + 56 + 3 + 11 HUD pixels) */
        const float cy = std::max(fbh * 0.20f, (float)(84 * s));
        const bool blink = (SDL_GetTicks() / 300) & 1;
        if (me->flags & RALLY_F_DEAD) {
            rly_text_c(fbw * 0.5f, cy, "DESTROYED", 3 * s, 1.0f, 0.30f, 0.25f);
            snprintf(buf, sizeof buf, "RESPAWN IN %d", (me->respawn_ticks + tps - 1) / tps);
            rly_text_c(fbw * 0.5f, cy + 38 * s, buf, 2 * s, 0.94f, 0.91f, 0.83f);
        } else if (me->flags & (RALLY_F_FLIPPED | RALLY_F_RIGHTING)) {
            rly_text_c(fbw * 0.5f, cy, "FLIPPED", 3 * s, 1.0f, 0.60f, 0.20f);
            if (!(me->flags & RALLY_F_RIGHTING)) {
                const int left = 3 * tps - me->flip_ticks;
                snprintf(buf, sizeof buf, "%d", left > 0 ? (left + tps - 1) / tps : 0);
                rly_text_c(fbw * 0.5f, cy + 38 * s, buf, 3 * s, 0.94f, 0.91f, 0.83f);
            }
        } else if ((me->flags & RALLY_F_WRONG_WAY) && blink) {
            rly_text_c(fbw * 0.5f, cy, "WRONG WAY", 3 * s, 1.0f, 0.25f, 0.20f);
        } else if (me->flags & RALLY_F_FINISHED) {
            snprintf(buf, sizeof buf, "FINISHED %s", rly_ordinal(me->place > 0 ? me->place : 1));
            rly_text_c(fbw * 0.5f, cy, buf, 3 * s, 1.0f, 0.85f, 0.54f);
        }
    }
}

/* THE RESULTS' GEOMETRY, one function for the draw and the click so they cannot part:
   the table, then a footer for the deciding player's two buttons or the line that says
   who everybody is waiting for. */
struct RlyResultsLayout {
    int s, x, y, w, h, rowH, n;
    bool cameos;
    int order[RALLY_MAX_RACERS];
    int footY;                  /* the footer's top */
    int bx[2], by, bw, bh;      /* RACE AGAIN, END */
};
static RlyResultsLayout rly_results_layout(int fbw, int fbh)
{
    const RallyState& S = g_rly.cur;
    RlyResultsLayout L;
    memset(&L, 0, sizeof L);
    int n = 0;
    bool used[RALLY_MAX_RACERS] = { false };
    for (int k = 0; k < S.finished_count && k < RALLY_MAX_RACERS; k++) {
        const int r = S.finish_order[k];
        if (r >= 0 && r < S.racer_count && !used[r]) { L.order[n++] = r; used[r] = true; }
    }
    while (n < S.racer_count && n < RALLY_MAX_RACERS) {
        int best = -1;
        for (int r = 0; r < S.racer_count && r < RALLY_MAX_RACERS; r++)
            if (!used[r] && (best < 0 || S.racer[r].place < S.racer[best].place)) best = r;
        if (best < 0) break;
        L.order[n++] = best; used[best] = true;
    }
    L.n = n;
    /* ONE ZOOM FOR THE WHOLE PANEL, the cameos at it like the text. A row is a cameo high
       when the panel then fits at the HUD's zoom; when it does not (many racers in a low
       window) the rows are a line of text high and name the vehicle instead, rather than
       the panel's zoom dropping or the cameo being drawn at another texel size. The
       footer is 48 HUD pixels: an 18 pixel button row and the line that names its keys. */
    const int foot1 = 48;
    const int s0 = rly_scale(fbw, fbh);
    const int sCam = rly_fit(s0, 300, 46 + n * (48 + 4) + 8 + foot1, fbw, fbh);
    L.cameos = sCam == s0;
    /* a vehicle's name is up to fifteen characters, wider than its cameo */
    const int w1 = L.cameos ? 300 : 360, rowH1 = L.cameos ? 48 + 4 : 16;
    L.s = L.cameos ? sCam : rly_fit(s0, w1, 46 + n * rowH1 + 8 + foot1, fbw, fbh);
    const int s = L.s;
    L.rowH = rowH1 * s;
    L.w = w1 * s;
    L.h = 46 * s + n * L.rowH + 8 * s + foot1 * s;
    L.x = (fbw - L.w) / 2;
    L.y = std::max(4, (fbh - L.h) / 2);
    L.footY = L.y + 46 * s + n * L.rowH + 4 * s;
    L.bw = 120 * s; L.bh = 18 * s;
    L.by = L.footY + 8 * s;
    const int gap = 12 * s;
    L.bx[0] = L.x + (L.w - 2 * L.bw - gap) / 2;
    L.bx[1] = L.bx[0] + L.bw + gap;
    return L;
}

/* Which of the two buttons is under (c, r): 1 RACE AGAIN, 2 END, 0 neither. Only the
   deciding player has them, and only until a choice is made. */
static bool rly_is_decider(void)
{
    const RallyState& S = g_rly.cur;
    return rly_live() && S.decider >= 0 && S.decider == S.local;
}
static bool rly_choice_open(void)
{
    return rly_is_decider() && g_rly.cur.phase == RALLY_PHASE_DONE
           && g_rly.cur.choice == RALLY_CHOICE_NONE && g_rly.chose == RALLY_CHOICE_NONE;
}
static int rly_results_hit(const RlyResultsLayout& L, float c, float r)
{
    if (!rly_choice_open()) return 0;
    for (int b = 0; b < 2; b++)
        if (c >= L.bx[b] && c < L.bx[b] + L.bw && r >= L.by && r < L.by + L.bh) return b + 1;
    return 0;
}

/* THE DECIDING PLAYER'S CHOICE, as a lockstep order: every peer acts on it on the same
   frame (the brain takes it only in DONE and only from RallyState.decider). */
static bool rly_choose(int choice, const char* how)
{
    if (!rly_choice_open() || !g_rlyApi.choose) {
        printf("RALLY|choice|%s|refused|%s\n", choice == RALLY_CHOICE_RETRY ? "retry" : "end",
               !g_rlyApi.choose ? "no export"
               : !rly_live() ? "no race"
               : g_rly.cur.phase != RALLY_PHASE_DONE ? "not the results"
               : !rly_is_decider() ? "not the deciding player" : "already chosen");
        fflush(stdout);
        return false;
    }
    const int ok = g_rlyApi.choose(RLY_PLAYER, choice);
    if (ok) g_rly.chose = choice;
    printf("RALLY|choice|%s|%s|by=%s|race=%d\n", choice == RALLY_CHOICE_RETRY ? "retry" : "end",
           ok ? "queued" : "refused", how, (int)g_rly.cur.race_number);
    fflush(stdout);
    return ok != 0;
}

static void rly_button(int x, int y, int w, int h, int s, const char* label, bool hot)
{
    sb_rect(x, y, x + w, y + h, hot ? 0.34f : 0.20f, hot ? 0.29f : 0.17f, hot ? 0.18f : 0.12f, 1.0f);
    sb_frame(x, y, x + w, y + h, hot ? 1.0f : 0.63f, hot ? 0.85f : 0.53f, hot ? 0.45f : 0.32f, 1.0f);
    rly_text_c(x + w * 0.5f, y + (h - 11 * s) * 0.5f + s, label, s, hot ? 1.0f : 0.94f,
               hot ? 0.85f : 0.91f, hot ? 0.54f : 0.83f);
}

static void rly_draw_results(int fbw, int fbh)
{
    const RallyState& S = g_rly.cur;
    const RlyResultsLayout L = rly_results_layout(fbw, fbh);
    const int s = L.s, x = L.x, y = L.y, w = L.w, rowH = L.rowH;
    /* opaque: the checkered finish line under it must not show through a modal panel */
    rly_panel(x, y, x + w, y + L.h, 1.0f);
    char title[32];
    if (S.race_number > 1) snprintf(title, sizeof title, "RACE %d RESULTS", (int)S.race_number);
    else snprintf(title, sizeof title, "RESULTS");
    rly_text_c(fbw * 0.5f, y + 8 * s, title, 2 * s, 1.0f, 0.85f, 0.54f);
    if (S.winner >= 0 && S.winner < S.racer_count) {
        char line[48];
        snprintf(line, sizeof line, "%s WINS", g_rly.seat[S.winner].name);
        rly_text_c(fbw * 0.5f, y + 30 * s, line, s, 0.94f, 0.91f, 0.83f);
    }
    for (int k = 0; k < L.n; k++) {
        const int r = L.order[k];
        const RallyRacer& R = S.racer[r];
        const RlySeat& seat = g_rly.seat[r];
        const int ry = y + 46 * s + k * rowH;
        const int ty = ry + (rowH - 11 * s) / 2;
        if (r == S.local) sb_rect(x + 4 * s, ry, x + w - 4 * s, ry + rowH - 2 * s, 0.22f, 0.19f, 0.12f, 1.0f);
        sb_text(x + 8 * s, ty, rly_ordinal(k + 1), s, 1.0f, 0.85f, 0.54f);
        sb_rect(x + 44 * s, ty + 2 * s, x + 52 * s, ty + 10 * s, seat.r, seat.g, seat.b, 1.0f);
        char name[16];
        snprintf(name, sizeof name, "%s", seat.name);
        sb_text(x + 58 * s, ty, name, s, 0.94f, 0.91f, 0.83f);
        const int ri = rly_roster_of_type(R.unit_type);
        if (ri >= 0 && L.cameos) {
            rly_cameo(ri, x + 162 * s, ry + (rowH - 48 * s) / 2, s);
        } else if (ri >= 0) {
            const RlyVehicle* v = rly_vehicle(g_rly.roster[ri]);
            if (v) sb_text(x + 162 * s, ty, v->name, s, 0.75f, 0.70f, 0.60f);
        }
        /* A finisher's time; a seat that left the match, OUT; a racer still on the course
           when the race was called, how far along it got, which is also the order it is
           ranked in. Never 100% without the line: that is what a time is for. */
        char t[24];
        const bool fin = (R.flags & RALLY_F_FINISHED) != 0;
        if (fin) {
            rly_time(R.finish_ticks, t, sizeof t);
        } else if (R.flags & RALLY_F_RETIRED) {
            snprintf(t, sizeof t, "OUT");
        } else {
            long long pc = (long long)R.progress * 100 / 65536;
            pc = pc < 0 ? 0 : (pc > 99 ? 99 : pc);
            snprintf(t, sizeof t, "%d%%", (int)pc);
        }
        sb_text(x + w - 8 * s - sb_text_w(t, s), ty, t, s,
                fin ? 0.94f : 0.75f, fin ? 0.91f : 0.70f, fin ? 0.83f : 0.60f);
    }
    /* THE FOOTER: the deciding player's RACE AGAIN and END, or what everybody else is
       waiting for */
    sb_rect(x + 8 * s, L.footY, x + w - 8 * s, L.footY + s, 0.47f, 0.37f, 0.23f, 1.0f);
    const int choice = S.choice != RALLY_CHOICE_NONE ? S.choice : g_rly.chose;
    const float ty = L.by + (L.bh - 11 * s) * 0.5f + s;
    char line[64];
    if (S.phase == RALLY_PHASE_OVER || choice == RALLY_CHOICE_END) {
        rly_text_c(fbw * 0.5f, ty, "THE MATCH IS ENDING", s, 1.0f, 0.85f, 0.54f);
    } else if (choice == RALLY_CHOICE_RETRY) {
        rly_text_c(fbw * 0.5f, ty, "RACING AGAIN ON THIS COURSE", s, 1.0f, 0.85f, 0.54f);
    } else if (rly_choice_open()) {
        const int hot = rly_results_hit(L, g_mouseScrC, g_mouseScrR);
        g_rly.resultsHover = hot;
        rly_button(L.bx[0], L.by, L.bw, L.bh, s, "RACE AGAIN", hot == 1);
        rly_button(L.bx[1], L.by, L.bw, L.bh, s, "END", hot == 2);
        rly_text_c(fbw * 0.5f, L.by + L.bh + 5 * s,
                   nm_active() ? "THE OTHERS WAIT FOR YOUR CHOICE" : "ENTER RACES AGAIN   ESC ENDS", s, 0.75f,
                   0.70f, 0.60f);
    } else if (S.decider >= 0 && S.decider < S.racer_count) {
        snprintf(line, sizeof line, "WAITING FOR %s", g_rly.seat[S.decider].name);
        rly_text_c(fbw * 0.5f, ty, line, s, 0.94f, 0.91f, 0.83f);
    } else {
        const int tps = rly_tps();
        snprintf(line, sizeof line, "THE MATCH ENDS IN %d", S.phase_ticks > 0 ? (S.phase_ticks + tps - 1) / tps : 0);
        rly_text_c(fbw * 0.5f, ty, line, s, 0.94f, 0.91f, 0.83f);
    }
}

static void rly_draw_hud(int fbw, int fbh)
{
    if (!rly_live() || g_gameOver.valid || g_cxOpen) return;
    rly_make_icons();
    begin_overlay(fbw, fbh);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    switch (g_rly.cur.phase) {
    case RALLY_PHASE_PICK:      rly_draw_pick(fbw, fbh); break;
    case RALLY_PHASE_COUNTDOWN: rly_draw_race(fbw, fbh); rly_draw_countdown(fbw, fbh); break;
    case RALLY_PHASE_RACE:      rly_draw_race(fbw, fbh); rly_draw_countdown(fbw, fbh); break;
    case RALLY_PHASE_DONE:      rly_draw_results(fbw, fbh); break;
    case RALLY_PHASE_OVER:      rly_draw_results(fbw, fbh); break;
    default: break;
    }
    glDisable(GL_BLEND);
    end_overlay();
}

/* ---- the pointer -------------------------------------------------------------------- */
static bool rly_aiming(void)
{
    const int ph = rly_phase();
    return rly_owns_input() && (ph == RALLY_PHASE_COUNTDOWN || ph == RALLY_PHASE_RACE);
}

/* update_cursor's arm: no pick of objects and no order cursor in a Rally, only the ground
   point under the pointer, which is where the crosshair aims. */
static bool rly_cursor(int fbw, int fbh)
{
    if (!rly_owns_input()) return false;
    g_cursorOnMap = false;
    cur_set(MOUSE_NORMAL);
    float wx, wz;
    g_rly.aimValid = screen_to_world(g_mouseScrC, g_mouseScrR, fbw, fbh, &wx, &wz);
    if (g_rly.aimValid) {
        g_rly.aimX = wx; g_rly.aimZ = wz;
        g_cursorWX = wx; g_cursorWZ = wz;
        /* the pointer's size, latched here as update_cursor latches it, so the plain
           arrow over the chooser is drawn at the size it has everywhere else */
        g_cursorScale = cursor_scale_at(wx, wz, fbw, fbh);
    }
    return true;
}

/* draw_cursor_top's arm: the crosshair while racing, the plain arrow otherwise. */
static bool rly_draw_pointer(int fbw, int fbh, bool dialogOpen)
{
    if (dialogOpen || !rly_aiming()) return false;
    rly_make_icons();
    const int s = rly_scale(fbw, fbh);
    begin_overlay(fbw, fbh);
    rly_blit(g_rly.crossTex, RALLY_CROSSHAIR_W, RALLY_CROSSHAIR_H,
             g_mouseScrC - (RALLY_CROSSHAIR_W / 2) * s - s * 0.5f,
             g_mouseScrR - (RALLY_CROSSHAIR_H / 2) * s - s * 0.5f, s);
    end_overlay();
    return true;
}

/* ---- the controls ------------------------------------------------------------------ */
/* At most one send a tick, as the contract asks. A press of fire or of the item that
   comes and goes between two sends would then never reach the brain (the item is taken
   on its press, and while racing the aim moves nearly every tick, so most ticks already
   have their send), so those two are LATCHED: held over to the next send, which carries
   them once, and the send after that lets them go if the key is up. */
static void rly_send(uint32_t bits, int32_t ax, int32_t ay, bool force)
{
    if (!g_rlyApi.input || !rly_live()) return;
    g_rly.latched |= bits & (RALLY_IN_FIRE | RALLY_IN_ITEM);
    if (!force && g_rly.sentAny && g_rly.sentFrame == g_engineFrame) return;   /* one a tick */
    const uint32_t out = bits | g_rly.latched;
    const bool moved = !g_rly.sentAny || abs(ax - g_rly.sentAx) > RALLY_FINE_PER_CELL / 16
                       || abs(ay - g_rly.sentAy) > RALLY_FINE_PER_CELL / 16;
    g_rly.latched = 0;
    if (!force && g_rly.sentAny && out == g_rly.sentBits && !moved) return;
    if (g_rlyApi.input(RLY_PLAYER, out, ax, ay) == 1) {
        /* KEPT, with the frame it runs on, for the prediction (rly_predict_frame): a
           skirmish runs it on the next tick; a match learns its frame when the network
           takes it (rly_on_drain) */
        RlyCtl c;
        c.bits = out; c.ax = ax; c.ay = ay;
        c.stamp = nm_active() ? -1 : g_rly.brainFrame;
        g_rly.ctl.push_back(c);
        rly_cache_after_send();
    }
    g_rly.sentAny = true;
    g_rly.sentBits = out;
    g_rly.sentAx = ax; g_rly.sentAy = ay;
    g_rly.sentFrame = g_engineFrame;
}

/* The aim in fine units: the crosshair's ground point, or straight ahead of the racer
   when the pointer is off the ground. */
static void rly_aim_fine(int32_t* ax, int32_t* ay)
{
    const RallyRacer* me = rly_local();
    if (g_rly.aimValid) {
        *ax = (int32_t)(g_rly.aimX * RALLY_FINE_PER_CELL);
        *ay = (int32_t)(g_rly.aimZ * RALLY_FINE_PER_CELL);
    } else if (me && g_rly.predActive) {
        /* straight ahead of the racer as it is drawn, which is where the player sees it */
        const float a = g_rly.predPose.yaw * 6.2831853f / 256.0f;
        *ax = (int32_t)((g_rly.predPose.x + sinf(a) * 4.0f) * RALLY_FINE_PER_CELL);
        *ay = (int32_t)((g_rly.predPose.z - cosf(a) * 4.0f) * RALLY_FINE_PER_CELL);
    } else if (me) {
        const float a = (me->yaw & 65535) * 6.2831853f / 65536.0f;
        *ax = me->x + (int32_t)(sinf(a) * 4 * RALLY_FINE_PER_CELL);
        *ay = me->y - (int32_t)(cosf(a) * 4 * RALLY_FINE_PER_CELL);
    } else {
        *ax = *ay = 0;
    }
}

/* Once a frame from the live loop: the held keys and buttons, sent when they change. */
static void rly_input_frame(const Uint8* ks, Uint32 buttons)
{
    if (!rly_owns_input()) return;
    uint32_t bits = 0;
    if (ks) {
        if (ks[SDL_SCANCODE_W] || ks[SDL_SCANCODE_UP])    bits |= RALLY_IN_THROTTLE;
        if (ks[SDL_SCANCODE_S] || ks[SDL_SCANCODE_DOWN])  bits |= RALLY_IN_BRAKE;
        if (ks[SDL_SCANCODE_A] || ks[SDL_SCANCODE_LEFT])  bits |= RALLY_IN_LEFT;
        if (ks[SDL_SCANCODE_D] || ks[SDL_SCANCODE_RIGHT]) bits |= RALLY_IN_RIGHT;
        if (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]) bits |= RALLY_IN_DRIFT;
        if (ks[SDL_SCANCODE_SPACE])                       bits |= RALLY_IN_ITEM;
        if (ks[SDL_SCANCODE_R])                           bits |= RALLY_IN_RESCUE;
        if (rly_aiming()) {
            /* the left-handed mouse scheme exchanges the two here as it does on the map */
            const bool lefty = fx_mouse_scheme(&g_fx) == FX_MOUSE_LEFTHANDED;
            const Uint32 fire = lefty ? SDL_BUTTON(SDL_BUTTON_RIGHT) : SDL_BUTTON(SDL_BUTTON_LEFT);
            const Uint32 item = lefty ? SDL_BUTTON(SDL_BUTTON_LEFT) : SDL_BUTTON(SDL_BUTTON_RIGHT);
            if (buttons & fire) bits |= RALLY_IN_FIRE;
            if (buttons & item) bits |= RALLY_IN_ITEM;
        }
    }
    int32_t ax, ay;
    rly_aim_fine(&ax, &ay);
    rly_send(bits, ax, ay, false);
}

/* A key the race takes: every key but the ones that leave or talk (Escape, Return, the
   function keys) and the system's shortcuts, so no hotkey and no order fires under it. */
static bool rly_swallows_key(SDL_Keycode sym)
{
    if (!rly_owns_input()) return false;
    if (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI | KMOD_ALT)) return false;
    if (sym == SDLK_ESCAPE || sym == SDLK_RETURN || sym == SDLK_KP_ENTER) return false;
    if (sym >= SDLK_F1 && sym <= SDLK_F12) return false;
    return true;
}

/* THE TWO KEYS A RACE GIVES A MEANING OF ITS OWN, ahead of the game's: on the results of
   a skirmish the player's Enter races again and Escape ends the match; on the chooser of a
   race again, Enter keeps last race's vehicle. In a network match the results leave both
   keys to the game, because there Return opens the chat line and Escape the menu, and the
   deciding player wants both while the others read the results (a "gg" typed after Return
   must not restart everybody's race): the choice is the buttons'. Anywhere else, and for
   anybody else, they are the game's as always. True when the key was taken. */
static bool rly_key(SDL_Keycode sym)
{
    if (!rly_owns_input()) return false;
    const bool enter = sym == SDLK_RETURN || sym == SDLK_KP_ENTER;
    if (rly_phase() == RALLY_PHASE_DONE && rly_choice_open()) {
        if (nm_active()) return false;
        if (enter) return rly_choose(RALLY_CHOICE_RETRY, "enter"), true;
        if (sym == SDLK_ESCAPE) return rly_choose(RALLY_CHOICE_END, "escape"), true;
        return false;
    }
    if (rly_phase() == RALLY_PHASE_PICK && enter) {
        const int keep = rly_keep_vehicle();
        if (keep < 0 || g_rly.asked >= 0 || !g_rlyApi.pick) return false;
        g_rlyApi.pick(RLY_PLAYER, keep);
        g_rly.asked = keep;
        printf("RALLY|pick|%d|%s|kept\n", keep,
               rly_vehicle(g_rly.roster[keep]) ? rly_vehicle(g_rly.roster[keep])->code : "?");
        fflush(stdout);
        return true;
    }
    return false;
}

/* Every press and release on the battlefield in a Rally: the chooser takes the left
   button while vehicles are chosen, and nothing ever selects or orders a unit. */
static void rly_mouse_button(bool down, int button, int c, int r, int fbw, int fbh)
{
    if (!down || button != SDL_BUTTON_LEFT) return;
    if (rly_phase() == RALLY_PHASE_DONE) {
        const int b = rly_results_hit(rly_results_layout(fbw, fbh), (float)c, (float)r);
        if (b) rly_choose(b == 1 ? RALLY_CHOICE_RETRY : RALLY_CHOICE_END, "click");
        return;
    }
    if (rly_phase() != RALLY_PHASE_PICK) return;
    const RlyPickLayout L = rly_pick_layout(fbw, fbh);
    const int i = rly_pick_hit(L, (float)c, (float)r);
    if (i < 0 || !g_rlyApi.pick) return;
    g_rlyApi.pick(RLY_PLAYER, i);
    g_rly.asked = i;
    printf("RALLY|pick|%d|%s\n", i, rly_vehicle(g_rly.roster[i]) ? rly_vehicle(g_rly.roster[i])->code : "?");
    fflush(stdout);
}

/* ---- the script verbs --------------------------------------------------------------- */
static const char* rly_phase_name(int phase)
{
    switch (phase) {
    case RALLY_PHASE_PICK: return "PICK";
    case RALLY_PHASE_COUNTDOWN: return "COUNTDOWN";
    case RALLY_PHASE_RACE: return "RACE";
    case RALLY_PHASE_DONE: return "DONE";
    case RALLY_PHASE_OVER: return "OVER";
    default: return "OFF";
    }
}

/* The value of a rallystate field, for expectrally. */
static bool rly_field(const char* name, long long* out)
{
    const RallyState& S = g_rly.cur;
    const RallyRacer* me = rly_local();
    const bool live = rly_live();
    if (!strcmp(name, "phase"))         *out = live ? S.phase : 0;
    else if (!strcmp(name, "tick"))     *out = g_engineFrame;
    else if (!strcmp(name, "race"))     *out = live ? S.race_ticks : 0;
    else if (!strcmp(name, "phaseticks")) *out = live ? S.phase_ticks : 0;
    else if (!strcmp(name, "local"))    *out = live ? S.local : -1;
    else if (!strcmp(name, "racers"))   *out = live ? S.racer_count : 0;
    else if (!strcmp(name, "finished")) *out = live ? S.finished_count : 0;
    else if (!strcmp(name, "winner"))   *out = live ? S.winner : -1;
    else if (!strcmp(name, "hazards"))  *out = live ? S.hazard_count : 0;
    else if (!strcmp(name, "crates"))   *out = live ? S.crate_count : 0;
    else if (!strcmp(name, "drawn"))    *out = g_rlyDraw;
    else if (!strcmp(name, "place"))    *out = me ? me->place : -1;
    else if (!strcmp(name, "progress")) *out = me ? me->progress : -1;
    else if (!strcmp(name, "health"))   *out = me ? me->health : -1;
    else if (!strcmp(name, "flags"))    *out = me ? me->flags : 0;
    else if (!strcmp(name, "item"))     *out = me ? me->item : 0;
    else if (!strcmp(name, "speed"))    *out = me ? me->speed : 0;
    else if (!strcmp(name, "unit"))     *out = me ? me->unit_type : -1;
    else if (!strcmp(name, "x"))        *out = me ? me->x : 0;
    else if (!strcmp(name, "y"))        *out = me ? me->y : 0;
    else if (!strcmp(name, "z"))        *out = me ? me->z : 0;
    else if (!strcmp(name, "lap"))      *out = me ? me->lap : 0;
    else if (!strcmp(name, "lapprogress")) *out = me ? me->lap_progress : -1;
    else if (!strcmp(name, "lastvehicle")) *out = me ? me->last_vehicle : -1;
    else if (!strcmp(name, "laps"))     *out = live ? S.laps : 0;
    else if (!strcmp(name, "loop"))     *out = live ? S.course_loop : 0;
    else if (!strcmp(name, "racenumber")) *out = live ? S.race_number : 0;
    else if (!strcmp(name, "decider"))  *out = live ? S.decider : -1;
    else if (!strcmp(name, "choice"))   *out = live ? S.choice : 0;
    else if (!strcmp(name, "predicted")) *out = g_rly.predActive ? 1 : 0;
    else return false;
    return true;
}

static bool rly_script_verb(const char* cmd, const char* arg, int fbw, int fbh,
                            void (*tick)(int), int* fails)
{
    if (!strcmp(cmd, "rallyuntil")) {
        /* rallyuntil PHASE MAXTICKS: tick one at a time until the race is in PHASE */
        char want[16] = { 0 };
        int mx = 0, n = 0;
        if (sscanf(arg, "%15s %d", want, &mx) < 1) { printf("RALLY|until|usage: rallyuntil PHASE MAXTICKS\n"); (*fails)++; return true; }
        if (mx <= 0) mx = 3000;
        int phase = -1;
        for (int p = 0; p <= RALLY_PHASE_OVER; p++)
            if (!strcasecmp(want, rly_phase_name(p))) phase = p;
        while (rly_phase() != phase && n < mx && !g_gameOver.valid) { tick(1); n++; }
        /* OVER is the match ending, so the verdict that ends it counts as reaching it */
        const bool ok = rly_phase() == phase || (phase == RALLY_PHASE_OVER && g_gameOver.valid);
        printf("RALLY|until|%s|ticks=%d|tick=%d|%s\n", want, n, g_engineFrame, ok ? "reached" : "TIMEOUT");
        if (!ok) (*fails)++;
    } else if (!strcmp(cmd, "rallyclick")) {
        /* rallyclick COL ROW: the left button pressed there, down the mouse's own path */
        float c = 0, r = 0;
        if (sscanf(arg, "%f %f", &c, &r) != 2) { printf("RALLY|click|usage: rallyclick COL ROW\n"); (*fails)++; return true; }
        g_mouseScrC = c; g_mouseScrR = r;
        const int before = g_rly.asked;
        rly_mouse_button(true, SDL_BUTTON_LEFT, (int)c, (int)r, fbw, fbh);
        printf("RALLY|click|%.0f,%.0f|asked=%d|%s\n", c, r, g_rly.asked,
               g_rly.asked != before ? "picked" : "nothing");
    } else if (!strcmp(cmd, "rallydebug")) {
        /* rallydebug OP A B C: the brain's single-machine test hook (RALLY_DEBUG_*), for
           what a headless run cannot drive into, such as a racer thrown into a flip */
        int op = 0, a = 0, b = 0, c = 0;
        /* it changes this machine's simulation and nobody else's */
        if (nm_active()) {
            printf("RALLY|debug|refused|a network match\n");
            (*fails)++;
            return true;
        }
        if (sscanf(arg, "%d %d %d %d", &op, &a, &b, &c) < 1 || !g_rlyApi.debug) {
            printf("RALLY|debug|%s\n", g_rlyApi.debug ? "usage: rallydebug OP A B C" : "no export");
            (*fails)++;
            return true;
        }
        const int r = g_rlyApi.debug(op, a, b, c);
        printf("RALLY|debug|op=%d|a=%d|b=%d|c=%d|answer=%d\n", op, a, b, c, r);
    } else if (!strcmp(cmd, "rallyalpha")) {
        /* rallyalpha A: draw the following scripted frames A of the way from the previous
           tick to the newest, as the live loop does between ticks; -1 is the newest */
        g_rly.scriptAlpha = (float)atof(arg);
        if (g_rly.scriptAlpha > 1.0f) g_rly.scriptAlpha = 1.0f;
        printf("RALLY|alpha|%.3f\n", g_rly.scriptAlpha);
    } else if (!strcmp(cmd, "rallyauto")) {
        const bool on = !strcmp(arg, "on") || !strcmp(arg, "1");
        /* a test aid, and in a match against people it would be the computer driving
           for one of them */
        if (nm_active()) {
            printf("RALLY|auto|refused|a network match\n");
            (*fails)++;
        } else if (!g_rlyApi.autopilot || !rly_live()) {
            printf("RALLY|auto|refused|%s\n", g_rlyApi.autopilot ? "no race" : "no export");
            (*fails)++;
        } else {
            g_rlyApi.autopilot(RLY_PLAYER, on ? 1 : 0);
            printf("RALLY|auto|%s\n", on ? "on" : "off");
        }
    } else if (!strcmp(cmd, "rallykeys")) {
        char keys[32] = { 0 };
        int n = 1;
        if (sscanf(arg, "%31s %d", keys, &n) < 1) { printf("RALLY|keys|usage: rallykeys KEYS TICKS\n"); (*fails)++; return true; }
        if (n < 1) n = 1;
        uint32_t bits = 0;
        for (const char* k = keys; *k; k++)
            switch (*k) {
            case 'w': bits |= RALLY_IN_THROTTLE; break;
            case 's': bits |= RALLY_IN_BRAKE; break;
            case 'a': bits |= RALLY_IN_LEFT; break;
            case 'd': bits |= RALLY_IN_RIGHT; break;
            case 'h': bits |= RALLY_IN_DRIFT; break;
            case 'f': bits |= RALLY_IN_FIRE; break;
            case 'i': bits |= RALLY_IN_ITEM; break;
            case 'r': bits |= RALLY_IN_RESCUE; break;
            default: break;
            }
        if (!rly_live() || !g_rlyApi.input) { printf("RALLY|keys|refused|no race\n"); (*fails)++; return true; }
        int32_t ax, ay;
        if (g_rly.scriptAim) { ax = g_rly.scriptAx; ay = g_rly.scriptAy; }
        else { g_rly.aimValid = false; rly_aim_fine(&ax, &ay); }
        rly_send(bits, ax, ay, true);
        tick(n);
        if (bits) rly_send(0, ax, ay, true);
        printf("RALLY|keys|%s|bits=%u|ticks=%d\n", keys, bits, n);
    } else if (!strcmp(cmd, "rallyaim")) {
        float cx = 0, cy = 0;
        if (sscanf(arg, "%f %f", &cx, &cy) != 2) { printf("RALLY|aim|usage: rallyaim CELLX CELLY\n"); (*fails)++; return true; }
        g_rly.scriptAim = true;
        g_rly.scriptAx = (int32_t)(cx * RALLY_FINE_PER_CELL);
        g_rly.scriptAy = (int32_t)(cy * RALLY_FINE_PER_CELL);
        rly_send(g_rly.sentBits, g_rly.scriptAx, g_rly.scriptAy, true);
        printf("RALLY|aim|%.2f,%.2f\n", cx, cy);
    } else if (!strcmp(cmd, "rallypick")) {
        const int i = atoi(arg);
        if (!g_rlyApi.pick || !rly_live()) { printf("RALLY|pick|refused\n"); (*fails)++; return true; }
        const int ok = g_rlyApi.pick(RLY_PLAYER, i);
        g_rly.asked = i;
        printf("RALLY|pick|%d|%s|%s\n", i,
               (i >= 0 && i < g_rly.rosterN && rly_vehicle(g_rly.roster[i])) ? rly_vehicle(g_rly.roster[i])->code : "?",
               ok ? "queued" : "refused");
    } else if (!strcmp(cmd, "rallychoice")) {
        /* rallychoice retry|end: the deciding player's choice on the results, down the
           same path as its buttons and keys */
        const bool retry = !strcmp(arg, "retry") || !strcmp(arg, "again");
        const bool end = !strcmp(arg, "end");
        if (!retry && !end) { printf("RALLY|choice|usage: rallychoice retry|end\n"); (*fails)++; return true; }
        if (!rly_choose(retry ? RALLY_CHOICE_RETRY : RALLY_CHOICE_END, "script")) (*fails)++;
    } else if (!strcmp(cmd, "rallylaps")) {
        /* rallylaps: what the race says about its laps */
        const RallyState& S = g_rly.cur;
        const RallyRacer* me = rly_local();
        const bool live = rly_live();
        printf("RALLY|laps|laps=%d|loop=%d|lap=%d|lap_progress=%d|progress=%d|race_number=%d\n",
               live ? S.laps : 0, live ? S.course_loop : 0, me ? me->lap : 0,
               me ? me->lap_progress : -1, me ? me->progress : -1, live ? S.race_number : 0);
    } else if (!strcmp(cmd, "rallypredict")) {
        /* rallypredict on|off: draw this machine's racer predicted or as executed (the A/B
           switch OPENCNC3D_RALLY_PREDICT=0 sets for a whole run) */
        g_rly.predOff = !(!strcmp(arg, "on") || !strcmp(arg, "1"));
        g_rly.cache.valid = false;
        g_rly.haveLastDrawn = false;
        printf("RALLY|predict|%s|export=%s\n", g_rly.predOff ? "off" : "on", g_rlyApi.predict ? "yes" : "no");
    } else if (!strcmp(cmd, "rallydrive")) {
        /* rallydrive KEYS TICKS FRAMES [EXTRA]: hold KEYS for TICKS engine ticks (and EXTRA
           frames more) the way the live loop does it, FRAMES drawn frames a tick: every
           frame reads the held keys and sends them when they change (at most one send a
           tick), the first frame of a tick runs the tick, and every frame is placed at its
           own fraction of the tick. The frame's place in its tick carries over from one
           call to the next, so EXTRA lands the next call's key change part way through a
           tick, where a hand's press lands. Each frame logs the executed racer and the
           racer as drawn, which is how the response to a key is measured frame by frame. */
        char keys[32] = { 0 };
        int nt = 1, fpt = 4, extra = 0;
        if (sscanf(arg, "%31s %d %d %d", keys, &nt, &fpt, &extra) < 2) {
            printf("RALLY|drive|usage: rallydrive KEYS TICKS FRAMES [EXTRA]\n"); (*fails)++; return true;
        }
        if (nt < 0) nt = 0;
        if (fpt < 1) fpt = 1;
        if (fpt > 16) fpt = 16;
        if (extra < 0) extra = 0;
        uint32_t bits = 0;
        for (const char* k = keys; *k; k++)
            switch (*k) {
            case 'w': bits |= RALLY_IN_THROTTLE; break;
            case 's': bits |= RALLY_IN_BRAKE; break;
            case 'a': bits |= RALLY_IN_LEFT; break;
            case 'd': bits |= RALLY_IN_RIGHT; break;
            case 'h': bits |= RALLY_IN_DRIFT; break;
            case 'f': bits |= RALLY_IN_FIRE; break;
            case 'i': bits |= RALLY_IN_ITEM; break;
            case 'r': bits |= RALLY_IN_RESCUE; break;
            default: break;
            }
        if (!rly_live() || !g_rlyApi.input) { printf("RALLY|drive|refused|no race\n"); (*fails)++; return true; }
        static int s_driveFrame = 0, s_drivePhase = 0, s_driveFpt = 0;
        if (s_driveFpt != fpt) { s_driveFpt = fpt; s_drivePhase = 0; }
        const double tickSec = 1.0 / (double)rly_tps();
        g_rly.scriptDt = (float)(tickSec / fpt);
        const int total = nt * fpt + extra;
        for (int k = 0; k < total && !g_gameOver.valid; k++) {
            {
                const int f = s_drivePhase;
                s_drivePhase = (s_drivePhase + 1) % fpt;
                int32_t ax, ay;
                if (g_rly.scriptAim) { ax = g_rly.scriptAx; ay = g_rly.scriptAy; }
                else { g_rly.aimValid = false; rly_aim_fine(&ax, &ay); }
                rly_send(bits, ax, ay, false);
                if (f == 0) tick(1);
                g_rly.scriptAlpha = (float)f / (float)fpt;
                rly_frame(fbw, fbh);
                const RallyRacer* me = rly_local();
                const RlyDrawn* d = NULL;
                for (int i = 0; me && i < g_rlyDraw; i++)
                    if (g_rlyDrawn[i].id == me->unit_id) d = &g_rlyDrawn[i];
                printf("RALLYDRIVE|frame=%d|tick=%d|sub=%d|keys=%s|bits=%u|exec_x=%.4f|exec_y=%.4f|"
                       "exec_face=%.3f|exec_speed=%d|drawn_x=%.4f|drawn_y=%.4f|drawn_face=%.3f|"
                       "pred=%d|steps=%d|off=%.4f|offyaw=%.3f|corr=%d|snaps=%d|err=%.4f\n",
                       s_driveFrame++, g_engineFrame, f, keys, bits,
                       me ? me->x / 65536.0 : 0.0, me ? me->y / 65536.0 : 0.0,
                       me ? (me->yaw & 65535) * 256.0 / 65536.0 : -1.0, me ? me->speed : 0,
                       d ? d->cx : 0.0f, d ? d->cz : 0.0f, d ? d->face : -1.0f,
                       g_rly.predActive ? 1 : 0, g_rly.predN,
                       sqrtf(g_rly.offX * g_rly.offX + g_rly.offZ * g_rly.offZ), g_rly.offYaw,
                       g_rly.predCorrections, g_rly.predSnaps, g_rly.predLastErr);
            }
        }
        g_rly.scriptDt = 0.0f;
        g_rly.scriptAlpha = -1.0f;
        printf("RALLY|drive|%s|bits=%u|ticks=%d|frames=%d|extra=%d\n", keys, bits, nt, fpt, extra);
    } else if (!strcmp(cmd, "rallyfollow")) {
        /* rallyfollow TICKS [FRAMES]: drive this machine's racer round the course by its keys,
           the way a player would, for up to TICKS engine ticks or until it finishes or the
           race ends: throttle, and A or D toward the centre line three points ahead of the
           racer as it is drawn (FRAMES drawn frames a tick, as rallydrive). Stuck for two
           seconds, it presses the rescue key. A test aid for a network race, where the
           autopilot is refused: it sends nothing a player's keys could not. */
        int nt = 0, fpt = 4;
        if (sscanf(arg, "%d %d", &nt, &fpt) < 1) {
            printf("RALLY|follow|usage: rallyfollow TICKS [FRAMES]\n"); (*fails)++; return true;
        }
        if (fpt < 1) fpt = 1;
        if (fpt > 16) fpt = 16;
        if (!rly_live() || !g_rlyApi.input || g_rly.courseN < 2) {
            printf("RALLY|follow|refused|%s\n", !rly_live() ? "no race" : "no course");
            (*fails)++;
            return true;
        }
        const double tickSec = 1.0 / (double)rly_tps();
        g_rly.scriptDt = (float)(tickSec / fpt);
        const bool loop = g_rly.cur.course_loop != 0;
        int nearest = -1, stuck = 0, rescues = 0, ticks = 0, lap0 = -1;
        const int corr0 = g_rly.predCorrections, snaps0 = g_rly.predSnaps;
        float ahead_max = 0.0f;   /* the farthest the drawn racer ran from the executed one, cells */
        for (int k = 0; k < nt * fpt && !g_gameOver.valid; k++) {
            const int f = k % fpt;
            const RallyRacer* me = rly_local();
            if (!me || g_rly.cur.phase != RALLY_PHASE_RACE || (me->flags & RALLY_F_FINISHED)) break;
            if (lap0 < 0) lap0 = me->lap;
            /* where the player sees the racer: the drawn pose when there is one */
            const float px = g_rly.predActive ? g_rly.predPose.x : me->x / 65536.0f;
            const float pz = g_rly.predActive ? g_rly.predPose.z : me->y / 65536.0f;
            const float pyaw = g_rly.predActive ? g_rly.predPose.yaw : (me->yaw & 65535) * 256.0f / 65536.0f;
            /* the nearest centre-line point, searched near the last one once it is known */
            const int N = g_rly.courseN;
            float best = 1e30f;
            const int from = nearest < 0 ? 0 : nearest - 6, to = nearest < 0 ? N - 1 : nearest + 12;
            for (int q = from; q <= to; q++) {
                int i = q;
                if (loop) i = ((q % N) + N) % N;
                else if (i < 0 || i >= N) continue;
                const float dx = g_rly.course[i].x / 65536.0f - px, dz = g_rly.course[i].y / 65536.0f - pz;
                if (dx * dx + dz * dz < best) { best = dx * dx + dz * dz; nearest = i; }
            }
            int ti = nearest + 3;
            if (loop) ti %= N;
            else if (ti > N - 1) ti = N - 1;
            const float tx = g_rly.course[ti].x / 65536.0f - px, tz = g_rly.course[ti].y / 65536.0f - pz;
            const float want = atan2f(tx, -tz) * 256.0f / 6.2831853f;
            const float diff = rly_dir_diff(want, pyaw);
            uint32_t bits = 0;
            if (fabsf(diff) < 48.0f) bits |= RALLY_IN_THROTTLE;
            if (diff > 5.0f) bits |= RALLY_IN_RIGHT;
            if (diff < -5.0f) bits |= RALLY_IN_LEFT;
            if (f == 0) {
                const int32_t sp = me->speed < 0 ? -me->speed : me->speed;
                stuck = (sp * 10 < me->top_speed && !(me->flags & RALLY_F_DEAD)) ? stuck + 1 : 0;
            }
            if (stuck >= 2 * rly_tps()) {
                bits = RALLY_IN_RESCUE;
                stuck = 0;
                rescues++;
            }
            const float a = pyaw * 6.2831853f / 256.0f;
            const int32_t ax = (int32_t)((px + sinf(a) * 4.0f) * RALLY_FINE_PER_CELL);
            const int32_t ay = (int32_t)((pz - cosf(a) * 4.0f) * RALLY_FINE_PER_CELL);
            rly_send(bits, ax, ay, false);
            if (f == 0) { tick(1); ticks++; }
            g_rly.scriptAlpha = (float)f / (float)fpt;
            rly_frame(fbw, fbh);
            float ahead = 0.0f;
            if (g_rly.predActive && rly_local()) {
                const float ex = rly_local()->x / 65536.0f - g_rly.predPose.x;
                const float ez = rly_local()->y / 65536.0f - g_rly.predPose.z;
                ahead = sqrtf(ex * ex + ez * ez);
                if (ahead > ahead_max) ahead_max = ahead;
            }
            if (f == 0 && ticks % rly_tps() == 0 && rly_local())
                printf("RALLYFOLLOW|tick=%d|lap=%d|lap_progress=%d|place=%d|x=%.2f|y=%.2f|speed=%d|rescues=%d|"
                       "drawn_ahead=%.3f|corr=%d|snaps=%d|snap_cells=%.2f\n",
                       g_engineFrame, rly_local()->lap, rly_local()->lap_progress, rly_local()->place,
                       rly_local()->x / 65536.0, rly_local()->y / 65536.0, rly_local()->speed, rescues, ahead,
                       g_rly.predCorrections - corr0, g_rly.predSnaps - snaps0, g_rly.predSnapCells);
        }
        g_rly.scriptDt = 0.0f;
        g_rly.scriptAlpha = -1.0f;
        int32_t ax, ay;
        rly_aim_fine(&ax, &ay);
        rly_send(0, ax, ay, false);
        const RallyRacer* me = rly_local();
        printf("RALLY|follow|ticks=%d|lap=%d->%d|finished=%d|rescues=%d|phase=%d|corrections=%d|snaps=%d|"
               "drawn_ahead_max=%.3f\n", ticks, lap0, me ? me->lap : -1,
               me && (me->flags & RALLY_F_FINISHED) ? 1 : 0, rescues, (int)g_rly.cur.phase,
               g_rly.predCorrections - corr0, g_rly.predSnaps - snaps0, ahead_max);
    } else if (!strcmp(cmd, "rallystate")) {
        const RallyState& S = g_rly.cur;
        const RallyRacer* me = rly_local();
        const bool live = rly_live();
        /* where the last frame drew this machine's racer, and its drawn heading */
        const RlyDrawn* drawnMe = NULL;
        for (int i = 0; me && i < g_rlyDraw; i++)
            if (g_rlyDrawn[i].id == me->unit_id) drawnMe = &g_rlyDrawn[i];
        printf("RALLY|phase=%s|tick=%d|local=%d|place=%d|progress=%d|health=%d|flags=%d|item=%d|"
               "winner=%d|race=%d|phaseticks=%d|racers=%d|finished=%d|unit=%d|speed=%d|x=%.2f|y=%.2f|"
               "z=%.2f|hazards=%d|drawn=%d|hash=%08x|drawn_at=%.3f,%.3f|drawn_face=%.2f|hudzoom=%d|"
               "lap=%d|laps=%d|loop=%d|lap_progress=%d|race_number=%d|decider=%d|choice=%d|"
               "last_vehicle=%d|predicted=%d\n",
               rly_phase_name(live ? S.phase : 0), g_engineFrame, live ? S.local : -1,
               me ? me->place : -1, me ? me->progress : -1, me ? me->health : -1,
               me ? me->flags : 0, me ? me->item : 0, live ? S.winner : -1,
               live ? S.race_ticks : 0, live ? S.phase_ticks : 0, live ? S.racer_count : 0,
               live ? S.finished_count : 0, me ? me->unit_type : -1, me ? me->speed : 0,
               me ? me->x / 65536.0 : 0.0, me ? me->y / 65536.0 : 0.0, me ? me->z / 65536.0 : 0.0,
               live ? S.hazard_count : 0, g_rlyDraw, g_rlyApi.hash ? g_rlyApi.hash() : 0u,
               drawnMe ? drawnMe->cx : 0.0f, drawnMe ? drawnMe->cz : 0.0f, drawnMe ? drawnMe->face : -1.0f,
               rly_scale(fbw, fbh), me ? me->lap : 0, live ? S.laps : 0, live ? S.course_loop : 0,
               me ? me->lap_progress : -1, live ? S.race_number : 0, live ? S.decider : -1,
               live ? S.choice : 0, me ? me->last_vehicle : -1, g_rly.predActive ? 1 : 0);
        for (int r = 0; live && r < S.racer_count; r++) {
            const RallyRacer& R = S.racer[r];
            printf("RALLY|racer=%d|house=%d|unit=%d|type=%d|place=%d|progress=%d|health=%d/%d|"
                   "flags=%d|item=%d|x=%.2f|y=%.2f|z=%.2f|yaw=%d|pitch=%d|roll=%d|finish=%d|"
                   "lap=%d|lap_progress=%d|last_vehicle=%d|name=%s\n",
                   r, R.house, R.unit_id, R.unit_type, R.place, R.progress, R.health,
                   R.health_max, R.flags, R.item, R.x / 65536.0, R.y / 65536.0, R.z / 65536.0,
                   R.yaw, R.pitch, R.roll, R.finish_ticks, R.lap, R.lap_progress, R.last_vehicle,
                   g_rly.seat[r].name);
        }
        for (int r = 0; live && S.phase == RALLY_PHASE_PICK && r < g_rlyPickWords; r++)
            printf("RALLY|chooser|seat=%d|word=%s\n", r, g_rlyPickWord[r]);
        for (int h = 0; live && h < S.hazard_count; h++) {
            const RallyHazard& H = S.hazard[h];
            printf("RALLY|hazard=%d|kind=%d|x=%.2f|y=%.2f|z=%.2f|yaw=%d|owner=%d|ticks=%d\n", h,
                   H.kind, H.x / 65536.0, H.y / 65536.0, H.z / 65536.0, H.yaw, H.owner, H.ticks);
        }
    } else if (!strcmp(cmd, "expectrally")) {
        char field[32] = { 0 }, op[8] = { 0 }, val[32] = { 0 };
        if (sscanf(arg, "%31s %7s %31s", field, op, val) != 3) {
            printf("EXPECTRALLY|usage: expectrally FIELD OP VALUE\n");
            (*fails)++;
            return true;
        }
        long long have = 0, want = 0;
        if (!rly_field(field, &have)) { printf("EXPECTRALLY|%s|unknown field|FAIL\n", field); (*fails)++; return true; }
        if (!strcmp(field, "phase") && !isdigit((unsigned char)val[0])) {
            want = -1;
            for (int p = 0; p <= RALLY_PHASE_OVER; p++)
                if (!strcasecmp(val, rly_phase_name(p))) want = p;
        } else {
            want = strtoll(val, NULL, 0);
        }
        bool ok = false;
        if (!strcmp(op, "==")) ok = have == want;
        else if (!strcmp(op, "!=")) ok = have != want;
        else if (!strcmp(op, "<")) ok = have < want;
        else if (!strcmp(op, "<=")) ok = have <= want;
        else if (!strcmp(op, ">")) ok = have > want;
        else if (!strcmp(op, ">=")) ok = have >= want;
        else if (!strcmp(op, "&")) ok = (have & want) == want;
        else if (!strcmp(op, "!&")) ok = (have & want) == 0;
        printf("EXPECTRALLY|%s|%s|%s|have=%lld|%s\n", field, op, val, have, ok ? "PASS" : "FAIL");
        if (!ok) (*fails)++;
    } else {
        return false;
    }
    fflush(stdout);
    return true;
}

#endif /* OPENCNC3D_RALLY_MOD_H */
