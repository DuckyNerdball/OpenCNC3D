/*
 * OpenCNC3D -- rally_probe: does the Tiberian Dawn brain run a Rally race on its own?
 *
 * Starts a skirmish Rally (game/rally_abi.h) on a course with four seats, seat 0 a human
 * player driven by the brain's autopilot and seats 1 to 3 computer drivers, and ticks it
 * through PICK, COUNTDOWN, RACE and DONE, races again on the same course (RALLY_CHOICE_RETRY)
 * and after the second race ends the match (RALLY_CHOICE_END, OVER), at 18 engine ticks a
 * second, with no window and no renderer. It prints one compact line per second and asserts
 * against the engine's own state:
 *
 *   - every racer moves forward along the course;
 *   - laps: a circuit is raced over the laps asked for (a point-to-point course once); a
 *     racer's lap only ever goes up by one, at the start/finish line, and a finisher has
 *     driven every lap; the race-wide progress agrees with the lap and the progress into
 *     it; the places agree with laps and progress across the line (a racer a lap ahead is
 *     ahead), finishers ahead of the rest;
 *   - the results wait for the deciding player (seat 0): a choice sent for another seat is
 *     ignored; seat 0's retry puts the course back the way it started (a defence destroyed
 *     and one damaged by the test hook stand again at full strength, every course object
 *     stands as it started, crates back, hazards gone, the racers off the course with no
 *     item) and returns to PICK, where each seat's last vehicle is preselected and kept if
 *     it chooses nothing; the second race is finished; seat 0's END goes to OVER and the
 *     match ends with the second race's winner;
 *   - OpenCNC3D_Rally_Predict, the local racer's driving run ahead of the lockstep: seat 0
 *     is driven by hand in short windows of varied controls (steering, drift, brake, fire
 *     and a nitro), and where nothing else touches it (no rival near, no hit) the predicted
 *     racer is exactly where the race then puts it; and calling it many times a tick, with
 *     any controls, changes nothing (run B does so and must hash the same as run A; only
 *     its calls for seat 0 while driven by hand run the physics, and the report says how
 *     many that was);
 *   - a racer is never put back on the course ahead of where it was: a lap (or the finish)
 *     counts only after the racer has come near three quarters of the course's points since
 *     the last one; in race 2 a rescue pressed in the countdown is dropped, one pressed at
 *     GO leaves seat 0 behind the start line on its own grid slot, a computer driver
 *     destroyed on the grid comes back behind the line on lap 1, and seat 0 put back on the
 *     line just after a new lap (rescue, or a kill when the rescue is cooling down) does not
 *     count that lap again; in the countdown the places are the grid's rows, front first;
 *   - no racer's engine unit ever ends a tick in a cell its locomotion cannot enter, or
 *     outside the playable map, or away from the position the race reports;
 *   - at least one racer finishes and the match ends with a winner (the engine's own
 *     multiplayer GAME_OVER callback, with one winning player);
 *   - a racer thrown upside down by the test hook lands flipped and rights itself after
 *     three seconds;
 *   - a racer destroyed through the engine's damage path respawns;
 *   - crates give items and items are used and explode; the hostile defences fire at the
 *     racers and the hostile house counts as powered; on SCM71EA an obelisk fires;
 *   - no crate ever appears on the map away from the course's own crate spots, although
 *     the match is started with the multiplayer crates option on;
 *   - the computer drivers drift for the mini-turbo: together they earn at least three in
 *     a race, and the best of them (skill 3) at least one;
 *   - no computer driver hits a wall within two seconds of getting control back after a
 *     spin (a spun car turns back round slowly, it does not race into the outside wall);
 *   - two runs of the same seed produce identical hash sequences.
 *
 * Besides the assertions it reports, per racer, the drift time, the mini-turbos earned and
 * the hard wall hits, and ends with one SUMMARY line (course, seed, winner, times,
 * counters) for batch runs.
 *
 * The last check needs two clean engines, so the parent process forks one child per run
 * before it loads the library, compares their hash sequences and exits non-zero if
 * either run failed an assertion or the sequences differ.
 *
 * Build (from the repository root):
 *   clang++ -std=c++17 -g -O1 -fms-extensions -fdeclspec -I brain/vanilla/tiberiandawn \
 *       -o <scratch>/rally_probe brain/host/rally_probe.cpp
 *
 * Run from a SMALL directory holding CONQUER.MIX and content/TEMPERAT.MIX (the reason is
 * in skirmish_probe.cpp: CNC_Init scans the working directory once per candidate file).
 * The scenario directory must end in a slash:
 *   cd <dir with CONQUER.MIX and content/>
 *   <scratch>/rally_probe <path>/TiberianDawn.dylib <repo>/brain/host/rally_test/ RALLYT01 \
 *       content/ <scratch>/rally.log [seed] [max_seconds] [laps] [races]
 *
 * laps is passed to OpenCNC3D_Rally_Configure (a circuit's laps, default 3); races is 1 to
 * end the match on the first results or 2 (the default) to race again first.
 *
 * The test courses are generated by brain/host/rally_test/make_test_course.py (RALLYT01, a
 * point-to-point course) and make_loop_course.py (RALLYT02, a small circuit).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#include <csignal>
#include <sys/wait.h>

#define TIBERIAN_DAWN 1
#define MEGAMAPS 1
#define _MAX_FNAME 256
#define _MAX_EXT 256
typedef uint64_t uint64;
#define __declspec(x)
#define __cdecl

#include "dllinterface.h"
#include "../../game/rally_abi.h"

typedef void (*CNC_Event_Callback_Type)(const EventCallbackStruct&);
typedef void (*CNC_Init_t)(const char*, CNC_Event_Callback_Type);
typedef void (*CNC_Config_t)(const CNCRulesDataStruct&);
typedef bool (*CNC_Start_Custom_t)(const char*, const char*, const char*, int, bool);
typedef bool (*CNC_Advance_t)(uint64);
typedef bool (*CNC_Set_MP_t)(int, CNCMultiplayerOptionsStruct&, int, CNCPlayerInfoStruct*, int);
typedef void (*Set_AI_Levels_t)(const unsigned char*, int);

typedef int (*Rally_Enable_t)(int, int);
typedef int (*Rally_Get_State_t)(uint64_t, RallyState*);
typedef int (*Rally_Get_Course_t)(RallyCoursePoint*, int);
typedef int (*Rally_Get_Roster_t)(int32_t*, int);
typedef int (*Rally_Pick_t)(uint64_t, int);
typedef int (*Rally_Input_t)(uint64_t, uint32_t, int32_t, int32_t);
typedef int (*Rally_Autopilot_t)(uint64_t, int);
typedef uint32_t (*Rally_Hash_t)(void);
typedef int (*Rally_Configure_t)(int);
typedef int (*Rally_Choose_t)(uint64_t, int);
typedef int (*Rally_Predict_t)(uint64_t, const RallyPredictStep*, int, RallyRacer*);
typedef int (*Rally_Debug_t)(int, int, int, int);

static int g_tick = -1;
static FILE* g_rep = NULL; /* the real stdout; the engine's chatter goes to the log file */
static int g_failures = 0;

static void crash(int sig)
{
    fflush(stdout);
    void* bt[48];
    int n = backtrace(bt, 48);
    fprintf(stderr, "\n*** SIGNAL %d during tick %d ***\n", sig, g_tick);
    backtrace_symbols_fd(bt, n, 2);
    if (g_rep) {
        fprintf(g_rep, "\n*** SIGNAL %d during tick %d ***\n", sig, g_tick);
        fflush(g_rep);
    }
    _exit(139);
}

#define CHECK(cond, ...)                                                                                              \
    do {                                                                                                              \
        if (!(cond)) {                                                                                                \
            ++g_failures;                                                                                             \
            fprintf(g_rep, "  FAIL @tick %d: ", g_tick);                                                              \
            fprintf(g_rep, __VA_ARGS__);                                                                              \
            fprintf(g_rep, "\n");                                                                                     \
        }                                                                                                             \
    } while (0)

/* ----------------------------------------------------------------- events ---- */

static int g_gameover = 0;
static int g_gameover_tick = -1;
static int g_winners = 0;
static uint64 g_winner_id = 0;

static void ev_cb(const EventCallbackStruct& e)
{
    if (e.EventType == CALLBACK_EVENT_DEBUG_PRINT) {
        const char* s = e.DebugPrint.PrintString;
        if (s && strstr(s, "CNC_Advance_Instance")) return;
        printf("  [dbg] %s\n", s ? s : "(null)");
        if (s && strstr(s, "Rally") && g_rep) fprintf(g_rep, "  [brain] %s\n", s);
    } else if (e.EventType == CALLBACK_EVENT_GAME_OVER) {
        ++g_gameover;
        if (g_gameover_tick < 0) g_gameover_tick = g_tick;
        /* The engine reports the end once per human player context; read the first. */
        const bool first = g_gameover == 1;
        if (g_rep) {
            fprintf(g_rep, "  [GAME_OVER @tick %d] multiplayer=%d players=%d\n", g_tick, (int)e.GameOver.Multiplayer,
                    e.GameOver.MultiPlayerTotalPlayers);
            for (int i = 0; i < e.GameOver.MultiPlayerTotalPlayers && i < 8; i++) {
                const auto& p = e.GameOver.MultiPlayerPlayersData[i];
                fprintf(g_rep, "      player[%d] id=%llu human=%d winner=%d\n", i,
                        (unsigned long long)p.GlyphXPlayerID, (int)p.IsHuman, (int)p.IsWinner);
                if (p.IsWinner && first) {
                    ++g_winners;
                    g_winner_id = p.GlyphXPlayerID;
                }
            }
        }
    }
}

static void fill_rules(CNCRulesDataStruct& rules)
{
    memset(&rules, 0, sizeof(rules));
    for (int i = 0; i < 3; i++) {
        rules.Difficulties[i].FirepowerBias = 1.0f;
        rules.Difficulties[i].GroundspeedBias = 1.0f;
        rules.Difficulties[i].AirspeedBias = 1.0f;
        rules.Difficulties[i].ArmorBias = 1.0f;
        rules.Difficulties[i].ROFBias = 1.0f;
        rules.Difficulties[i].CostBias = 1.0f;
        rules.Difficulties[i].BuildSpeedBias = 1.0f;
        rules.Difficulties[i].RepairDelay = 0.02f;
        rules.Difficulties[i].BuildDelay = 0.03f;
        rules.Difficulties[i].IsBuildSlowdown = false;
        rules.Difficulties[i].IsWallDestroyer = true;
        rules.Difficulties[i].IsContentScan = true;
    }
}

static const char* phase_name(int p)
{
    static const char* names[] = {"OFF", "PICK", "COUNT", "RACE", "DONE", "OVER"};
    return p >= 0 && p <= 5 ? names[p] : "?";
}

static const char* item_name(int i)
{
    static const char* names[] = {"-", "nitro", "nitro3", "mine", "rocket", "homing", "ion", "armour", "cloak", "repair"};
    return i >= 0 && i < RALLY_ITEM_COUNT ? names[i] : "?";
}

static const char* stat_names[RALLY_STAT_COUNT] = {"crates", "items", "hazards", "blasts", "respawns", "flips",
                                                   "righted", "shots", "run_over", "wall_hits", "rams", "rescues",
                                                   "thrown", "defence_hits", "racer_hits", "defence_dmg",
                                                   "defence_kills", "mini_turbos"};

/* ------------------------------------------------------------------ a race ---- */

enum
{
    TPS = 18,
    SEATS = 4
};

/* AI difficulty per seat (0 easiest .. 3 hardest). The engine gives seat i levels[i]; seat 0
   is the autopilot's human seat, so the computer drivers are seats 1 to 3 at 0, 2 and 3. */
static const unsigned char g_levels[8] = {1, 0, 2, 3, 1, 1, 1, 1};

/* A small deterministic mixer for the test's own control patterns. */
static uint32_t mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/* The controls of one hand-driven prediction window: mostly throttle, steering held for a
   few ticks at a time, now and then a drift, the brake or the trigger, and in the windows
   given a nitro, the item key pressed twice. */
static void window_controls(int window, bool nitro, int32_t ax, int32_t ay, RallyPredictStep* steps, int n)
{
    uint32_t steer = 0;
    int hold = 0;
    for (int k = 0; k < n; ++k) {
        const uint32_t h = mix32((uint32_t)(window * 7919 + k * 104729 + 17));
        if (hold <= 0) {
            const uint32_t pick = h % 5;
            steer = pick == 0 ? RALLY_IN_LEFT : pick == 1 ? RALLY_IN_RIGHT : pick == 2 ? (RALLY_IN_LEFT | RALLY_IN_DRIFT)
                    : pick == 3                                           ? (RALLY_IN_RIGHT | RALLY_IN_DRIFT)
                                                                          : 0;
            hold = 3 + (int)((h >> 8) % 6);
        }
        --hold;
        uint32_t bits = steer;
        const uint32_t r = (h >> 12) % 10;
        bits |= r < 8 ? RALLY_IN_THROTTLE : r == 8 ? RALLY_IN_BRAKE : 0;
        if ((h >> 20) % 4 == 0) bits |= RALLY_IN_FIRE;
        if (nitro && (k == 5 || k == 13)) bits |= RALLY_IN_ITEM;
        steps[k].bits = bits;
        steps[k].aim_x = ax;
        steps[k].aim_y = ay;
    }
}

/* Whether any other racer on the course is within that many cells of seat 0. */
static bool rival_within(const RallyState& s, double cells)
{
    for (int i = 1; i < s.racer_count; ++i) {
        const RallyRacer& o = s.racer[i];
        if (o.unit_id >= 0 && hypot((double)o.x - s.racer[0].x, (double)o.y - s.racer[0].y) < cells * 65536) return true;
    }
    return false;
}

static int run_race(int argc, char** argv, const char* hashpath, bool heavy_predict)
{
    const char* dylib = argv[1];
    const char* dir = argv[2];
    const char* scen = argv[3];
    const char* content = argv[4];
    const char* logpath = argv[5];
    const int seed = argc > 6 ? atoi(argv[6]) : 0;
    const int max_seconds = argc > 7 && atoi(argv[7]) > 0 ? atoi(argv[7]) : 900;
    const int laps_asked = argc > 8 ? atoi(argv[8]) : RALLY_LAPS_DEFAULT;
    const int races = argc > 9 && atoi(argv[9]) == 1 ? 1 : 2;

    signal(SIGSEGV, crash);
    signal(SIGBUS, crash);
    signal(SIGILL, crash);
    signal(SIGABRT, crash);

    int real1 = dup(1);
    g_rep = fdopen(real1, "w");
    setvbuf(g_rep, NULL, _IOLBF, 0);
    if (!freopen(logpath, "w", stdout)) {
        fprintf(g_rep, "cannot open log %s\n", logpath);
        return 1;
    }
    FILE* hashes = fopen(hashpath, "w");
    if (!hashes) {
        fprintf(g_rep, "cannot open %s\n", hashpath);
        return 1;
    }

    void* h = dlopen(dylib, RTLD_NOW);
    if (!h) {
        fprintf(g_rep, "dlopen FAILED: %s\n", dlerror());
        return 1;
    }
#define SYM(type, name) type name = (type)dlsym(h, #name)
    SYM(CNC_Init_t, CNC_Init);
    SYM(CNC_Config_t, CNC_Config);
    SYM(CNC_Set_MP_t, CNC_Set_Multiplayer_Data);
    SYM(CNC_Start_Custom_t, CNC_Start_Custom_Instance);
    SYM(CNC_Advance_t, CNC_Advance_Instance);
    SYM(Set_AI_Levels_t, OpenCNC3D_Set_AI_Difficulties);
    SYM(Rally_Enable_t, OpenCNC3D_Rally_Enable);
    SYM(Rally_Get_State_t, OpenCNC3D_Rally_Get_State);
    SYM(Rally_Get_Course_t, OpenCNC3D_Rally_Get_Course);
    SYM(Rally_Get_Roster_t, OpenCNC3D_Rally_Get_Roster);
    SYM(Rally_Pick_t, OpenCNC3D_Rally_Pick);
    SYM(Rally_Input_t, OpenCNC3D_Rally_Input);
    SYM(Rally_Autopilot_t, OpenCNC3D_Rally_Autopilot);
    SYM(Rally_Hash_t, OpenCNC3D_Rally_Hash);
    SYM(Rally_Debug_t, OpenCNC3D_Rally_Debug);
    SYM(Rally_Configure_t, OpenCNC3D_Rally_Configure);
    SYM(Rally_Choose_t, OpenCNC3D_Rally_Choose);
    SYM(Rally_Predict_t, OpenCNC3D_Rally_Predict);
#undef SYM
    if (!CNC_Init || !CNC_Config || !CNC_Set_Multiplayer_Data || !CNC_Start_Custom_Instance || !CNC_Advance_Instance
        || !OpenCNC3D_Rally_Enable || !OpenCNC3D_Rally_Get_State || !OpenCNC3D_Rally_Get_Course
        || !OpenCNC3D_Rally_Get_Roster || !OpenCNC3D_Rally_Pick || !OpenCNC3D_Rally_Input
        || !OpenCNC3D_Rally_Autopilot || !OpenCNC3D_Rally_Hash || !OpenCNC3D_Rally_Debug
        || !OpenCNC3D_Rally_Configure || !OpenCNC3D_Rally_Choose || !OpenCNC3D_Rally_Predict) {
        fprintf(g_rep, "dlsym FAILED: an export is missing\n");
        return 1;
    }

    CNC_Init("", ev_cb);
    CNCRulesDataStruct rules;
    fill_rules(rules);
    CNC_Config(rules);

    /* The laps and the switch go on before the scenario loads, with the real tick rate. */
    const int laps_set = OpenCNC3D_Rally_Configure(laps_asked);
    CHECK(laps_set >= RALLY_LAPS_MIN && laps_set <= RALLY_LAPS_MAX, "Rally_Configure answered %d laps", laps_set);
    CHECK(OpenCNC3D_Rally_Enable(1, TPS * 10) == 1, "Rally_Enable did not answer 1");
    if (seed) OpenCNC3D_Rally_Debug(RALLY_DEBUG_SEED, seed, 0, 0);
    if (OpenCNC3D_Set_AI_Difficulties) OpenCNC3D_Set_AI_Difficulties(g_levels, 8);

    CNCMultiplayerOptionsStruct opts;
    memset(&opts, 0, sizeof(opts));
    opts.MPlayerCount = SEATS;
    opts.MPlayerBases = 1;
    opts.MPlayerCredits = 0;
    opts.MPlayerTiberium = 0;
    opts.MPlayerGoodies = 1; /* Rally must ignore the random crates */
    opts.MPlayerSolo = 1;
    opts.MPlayerUnitCount = 10;
    opts.EnableSuperweapons = false;
    opts.DestroyStructures = false;

    CNCPlayerInfoStruct players[8];
    memset(players, 0, sizeof(players));
    for (int i = 0; i < SEATS; i++) {
        snprintf(players[i].Name, sizeof(players[i].Name), i == 0 ? "RACER" : "DRIVER%d", i);
        players[i].House = (unsigned char)(i & 1);
        players[i].ColorIndex = i;
        players[i].GlyphxPlayerID = 1000 + i;
        players[i].Team = i < 2 ? 0 : i; /* seats 0 and 1 share a team: Rally must not ally them */
        players[i].StartLocationIndex = i;
        players[i].IsAI = i != 0;
    }
    const uint64 me = 1000;
    if (!CNC_Set_Multiplayer_Data(1, opts, SEATS, players, 8)) {
        fprintf(g_rep, "CNC_Set_Multiplayer_Data refused the setup\n");
        return 1;
    }
    if (!CNC_Start_Custom_Instance(content, dir, scen, 7, true)) {
        fprintf(g_rep, "CNC_Start_Custom_Instance refused %s%s (a [Rally] section problem is logged above)\n", dir, scen);
        return 1;
    }

    static RallyCoursePoint course[RALLY_MAX_COURSE];
    const int points = OpenCNC3D_Rally_Get_Course(course, RALLY_MAX_COURSE);
    int32_t roster[RALLY_MAX_ROSTER];
    const int roster_count = OpenCNC3D_Rally_Get_Roster(roster, RALLY_MAX_ROSTER);
    static RallyState s;
    OpenCNC3D_Rally_Get_State(me, &s);
    const bool loop = s.course_loop != 0;
    const int laps = s.laps;
    double course_length = 0; /* fine units along the centre line, once round a circuit */
    for (int p = 0; p + 1 < points; ++p) {
        course_length += hypot((double)course[p + 1].x - course[p].x, (double)course[p + 1].y - course[p].y);
    }
    if (loop && points > 1) {
        course_length += hypot((double)course[0].x - course[points - 1].x, (double)course[0].y - course[points - 1].y);
    }
    const double race_length = course_length * laps;
    fprintf(g_rep, "course %s: %s, %d points, %.1f cells a lap, %d lap(s), roster %d vehicles, seed %d%s\n", scen,
            loop ? "a circuit" : "point to point", points, course_length / 65536, laps, roster_count, seed,
            heavy_predict ? ", predicting heavily every tick" : "");
    CHECK(laps == (loop ? laps_set : 1), "the race has %d laps; %d were asked for on a %s", laps, laps_set,
          loop ? "circuit" : "point-to-point course");
    CHECK(points >= 2, "the course has %d points", points);
    CHECK(roster_count == 16, "the roster has %d vehicles", roster_count);

    /* Seat 0 is a human player: the autopilot drives it, and it picks the Nod buggy. */
    CHECK(OpenCNC3D_Rally_Autopilot(me, 1) == 1, "Rally_Autopilot refused");
    CHECK(OpenCNC3D_Rally_Pick(me, 0) == 1, "Rally_Pick refused");

    int start_progress[RALLY_MAX_RACERS];
    int best_progress[RALLY_MAX_RACERS];
    for (int i = 0; i < RALLY_MAX_RACERS; ++i) start_progress[i] = best_progress[i] = -1;
    int check_violations = 0;
    int phases_seen = 0;
    int race_start_tick = -1;
    int last_phase = -1;

    /* Scripted tests, in race seconds. */
    const int flip_racer = 1, kill_racer = 2;
    int flip_at = -1, flipped_at = -1, righting_at = -1, righted_at = -1, flip_retries = 0;
    int kill_at = -1, dead_seen_at = -1, respawned_at = -1, old_unit = -1, new_unit = -1;
    int gave_ion_at = -1, gave_homing_at = -1;
    int hostile_aim_ticks = 0, hostile_shots = 0;
    int stray_crates = 0, stray_first = -1;
    int wall_at = -1, wall_hits0 = 0, wall_hits = -1, wall_health0 = 0, wall_health = 0;
    int wall_try_at = 2 * TPS, wall_rams0 = 0, wall_retries = 0;
    int manual_from = -1, manual_to = -1, manual_fast = 0, manual_shots0 = 0, manual_shots = -1;
    int manual_rescues0 = 0, manual_rescues = -1;
    int manual_start = 25 * TPS, manual_retries = 0;

    /* Drifts per racer: race ticks spent drifting, and mini-turbos earned. The brain counts
       mini-turbos for the whole race; each one it counts on a tick goes to a racer whose
       charged drift (RALLY_F_TURBO_READY) was let go on that tick with a boost running, or
       that was destroyed on that tick (the drift is let go before the damage lands). */
    int prev_flags[RALLY_MAX_RACERS] = {0};
    int drift_ticks[RALLY_MAX_RACERS] = {0};
    int turbos[RALLY_MAX_RACERS] = {0};
    int turbo_count = 0;

    /* Hard wall hits per racer. The brain counts them for the whole race, and a hit takes at
       least 3 cells/s off the speed into the wall in that tick and costs health unless the
       racer is shielded or armoured; each one counted on a tick goes to the racer that lost
       health (or could not) on that tick with the largest change of velocity, if that change
       is at least 2.5 cells/s. A hit within two seconds of a racer getting control back after
       a spin (in control: not spinning, airborne, flipped, righting or dead) is also kept
       apart, with what it looked like. */
    const double hit_dv = 2.5 * 65536.0 / TPS;
    int wall_count = 0;
    int ai_wall_hits = 0, ai_wall_hits_line = 0; /* computer drivers' hard wall hits, and those within 6 cells of P000 */
    int wall_hits_by[RALLY_MAX_RACERS] = {0};
    int spin_hits_by[RALLY_MAX_RACERS] = {0};
    int prev_health[RALLY_MAX_RACERS] = {0}, was_flags[RALLY_MAX_RACERS] = {0};
    int32_t prev_vx[RALLY_MAX_RACERS] = {0}, prev_vy[RALLY_MAX_RACERS] = {0};
    int spun[RALLY_MAX_RACERS] = {0};
    int control_back_at[RALLY_MAX_RACERS];
    for (int i = 0; i < RALLY_MAX_RACERS; ++i) control_back_at[i] = -1;
    char spin_hit_note[RALLY_MAX_RACERS][160];
    memset(spin_hit_note, 0, sizeof spin_hit_note);
    const int out_of_control =
        RALLY_F_SPINNING | RALLY_F_AIRBORNE | RALLY_F_FLIPPED | RALLY_F_RIGHTING | RALLY_F_DEAD;
    /* How far the hull points away from the way the course runs at the nearest centre-line
       point, in degrees. */
    const auto off_course_deg = [&](const RallyRacer& r) {
        int best = 0;
        double best_d2 = 1e30;
        for (int p = 0; p + (loop ? 0 : 1) < points; ++p) {
            const double dx = (double)course[p].x - r.x, dy = (double)course[p].y - r.y;
            if (dx * dx + dy * dy < best_d2) {
                best_d2 = dx * dx + dy * dy;
                best = p;
            }
        }
        const int next = (best + 1) % points;
        const double cdir = atan2((double)course[next].x - course[best].x, -((double)course[next].y - course[best].y));
        double d = r.yaw * 6.283185307 / 65536 - cdir;
        while (d > 3.141592654) d -= 6.283185307;
        while (d < -3.141592654) d += 6.283185307;
        return (int)lround(fabs(d) * 180 / 3.141592654);
    };

    /* Laps: each racer's lap and finish as last seen in this race, the lap counts seen, and
       the checks that failed (each reported once, with the first case). */
    int last_lap[RALLY_MAX_RACERS], lap_ups[RALLY_MAX_RACERS], last_flags[RALLY_MAX_RACERS];
    int lap_race = 0, lap_bad_step = 0, lap_bad_where = 0, lap_bad_finish = 0, lap_bad_range = 0;
    int place_bad = 0, progress_bad = 0, lap_counts_seen = 0, finished_all_laps = 0;
    for (int i = 0; i < RALLY_MAX_RACERS; ++i) last_lap[i] = lap_ups[i] = last_flags[i] = 0;
    const int lap_from = 0;

    /* The results and the choice. */
    int done_at[3] = {-1, -1, -1}, retry_seen_at = -1, over_at = -1, nondecider_at = -1, retry_sent_at = -1;
    int end_sent_at = -1, broke_at = -1, fixtures_total = -1, fixtures_broken = -1, hostile_start = -1;
    int decider_seen = -2, choice_seen_done = -2, still_done_after_nondecider = -1, phase_ticks_done = -1;
    int race2_kept = -1, race2_presel_ok = -1, race2_pick_hidden = -1, race2_finished = -1;
    int race1_type[RALLY_MAX_RACERS], race1_roster[RALLY_MAX_RACERS];
    static RallyState s1, s2;     /* the results of race 1 and race 2 as they first showed */
    int stats1[RALLY_STAT_COUNT]; /* the counters at race 1's results */
    memset(stats1, 0, sizeof stats1);
    for (int i = 0; i < RALLY_MAX_RACERS; ++i) race1_type[i] = race1_roster[i] = -1;
    char reset_note[256] = "";
    int reset_ok = -1;

    /* Prediction windows: seat 0 driven by hand in race 1, from 8 s into it, a window at most
       every two seconds, each one tick of lead-in (autopilot off) and PW_TICKS predicted
       ticks. A window starts only with seat 0 at speed along the course (so that random
       controls do not leave it wedged against a wall for the autopilot to dig out), and not
       from 23 s until the hand-driving test has run. */
    enum { PW_TICKS = 24, PW_MAX = 16 };
    RallyPredictStep pw_steps[PW_TICKS];
    RallyRacer pw_pred;
    memset(&pw_pred, 0, sizeof pw_pred);
    int pw_phase = 0, pw_k = 0, pw_next = 8 * TPS, pw_window = 0, pw_thrown0 = 0, pw_crates0 = 0;
    bool pw_disturbed = false, pw_nitro = false, pw_item_touched = false;
    int pw_exact = 0, pw_mismatch = 0, pw_disturbed_count = 0, pw_refused = 0, pw_bits_bad = 0, pw_nitro_exact = 0;
    double pw_worst_undisturbed = 0, pw_worst_disturbed = 0;
    char pw_note[256] = "";
    int predicts_called = 0, predicts_simulated = 0;

    /* Put back, never on (see the header). near_point marks the course points each racer has
       come near since its last lap counted; travel_* is what was checked. The race 2 tests:
       pb_cd the countdown press, pb_go the press at GO, pb_kill a computer driver destroyed on
       the grid, pb_line seat 0 put back on the line after a new lap. Each *_ok is -1 until
       the test has run, then 1 or 0. */
    static unsigned char near_point[RALLY_MAX_RACERS][RALLY_MAX_COURSE];
    memset(near_point, 0, sizeof near_point);
    int near_count[RALLY_MAX_RACERS], travel_lap[RALLY_MAX_RACERS], travel_fin[RALLY_MAX_RACERS];
    for (int i = 0; i < RALLY_MAX_RACERS; ++i) near_count[i] = travel_lap[i] = travel_fin[i] = 0;
    int travel_race = 0, travel_bad = 0, travel_checked = 0, grid_place_bad = 0;
    double travel_worst = 1.0;
    int32_t pb_grid_x[RALLY_MAX_RACERS] = {0}, pb_grid_y[RALLY_MAX_RACERS] = {0};
    int pb_cd_tick = -1, pb_cd_rescues = 0, pb_cd_ok = -1;
    int pb_go_state = 0, pb_go_tick = -1, pb_go_rescues = 0, pb_go_ok = -1;
    double pb_go_ahead = 0, pb_go_from_grid = 0;
    const int pb_kill_racer = 3;
    int pb_kill_state = 0, pb_kill_tick = -1, pb_kill_ok = -1, pb_kill_watch = -1;
    double pb_kill_ahead = 0;
    int pb_line_state = 0, pb_line_tick = -1, pb_line_lap = -1, pb_line_rescues = 0, pb_line_ok = -1;
    int pb_line_watch = -1, pb_line_how = 0; /* 1 rescue, 2 kill */
    /* Cells past the start line along the way the course leaves it (negative behind it). */
    const auto past_line = [&](const RallyRacer& r) {
        if (points < 2) return 0.0;
        const double sx = (double)course[1].x - course[0].x, sy = (double)course[1].y - course[0].y;
        const double len = hypot(sx, sy);
        if (len <= 0) return 0.0;
        return (((double)r.x - course[0].x) * sx + ((double)r.y - course[0].y) * sy) / len / 65536.0;
    };
    const auto rescues_now = [&]() { return OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_RESCUES, 0, 0); };

    /* Before the first tick the match is in PICK, waiting for the vehicles. */
    if (OpenCNC3D_Rally_Get_State(me, &s)) {
        phases_seen |= 1 << s.phase;
        last_phase = s.phase;
        fprintf(g_rep, "  phase %s before the first tick, %d racers\n", phase_name(s.phase), s.racer_count);
    }

    const int max_ticks = max_seconds * TPS;
    int ended_at = -1;
    for (int t = 0; t < max_ticks; ++t) {
        g_tick = t;
        if (!CNC_Advance_Instance(me)) {
            ended_at = t;
            /* The tick that ended the match: END has taken it to OVER. */
            if (OpenCNC3D_Rally_Get_State(me, &s)) {
                phases_seen |= 1 << s.phase;
                if (s.phase == RALLY_PHASE_OVER && over_at < 0) {
                    over_at = t;
                    CHECK(s.choice == RALLY_CHOICE_END, "OVER with choice %d", s.choice);
                }
            }
            break;
        }
        if (!OpenCNC3D_Rally_Get_State(me, &s)) {
            CHECK(false, "Rally_Get_State says no Rally match is running");
            break;
        }
        if (s.phase != last_phase) {
            fprintf(g_rep, "  phase %s at tick %d\n", phase_name(s.phase), t);
            phases_seen |= 1 << s.phase;
            last_phase = s.phase;
            if (s.phase == RALLY_PHASE_RACE) race_start_tick = t;
        }
        CHECK(s.local == 0, "local racer is %d, not 0", s.local);

        /* Run B asks for predictions all the time, with any controls, for every seat, in every
           phase; none of that may change the race (its hashes must equal run A's). Only the
           calls for seat 0 while it is driven by hand in a race actually simulate anything
           (the report counts them: a few percent of the calls); every other call answers 0
           before the physics runs, so it proves only that the refusal is pure. */
        if (heavy_predict) {
            static RallyPredictStep junk[RALLY_PREDICT_MAX + 8];
            RallyRacer out;
            for (int j = 0; j < 4; ++j) {
                const int n = (int)(mix32((uint32_t)(t * 31 + j)) % (RALLY_PREDICT_MAX + 1));
                for (int k = 0; k < RALLY_PREDICT_MAX + 8; ++k) {
                    const uint32_t h = mix32((uint32_t)(t * 977 + j * 131 + k));
                    junk[k].bits = h;
                    junk[k].aim_x = (int32_t)(mix32(h) & 0x7FFFFF);
                    junk[k].aim_y = (int32_t)(mix32(h + 1) & 0x7FFFFF);
                }
                /* A call that answers 1 for n > 0 ran the driving physics; one that answers 0
                   (a computer seat, seat 0 on the autopilot, any phase but RACE) returned the
                   executed racer without simulating anything. */
                if (OpenCNC3D_Rally_Predict(me, junk, n, &out) == 1 && n > 0) ++predicts_simulated;
                if (OpenCNC3D_Rally_Predict(me + 1 + j % 3, junk, n, &out) == 1 && n > 0) ++predicts_simulated;
                predicts_called += 2;
            }
            OpenCNC3D_Rally_Predict(me, junk, RALLY_PREDICT_MAX + 8, &out);
            OpenCNC3D_Rally_Predict(me, NULL, 0, &out);
            OpenCNC3D_Rally_Predict(me, junk, -3, &out);
            predicts_called += 3;
        }

        /* Laps, progress and places, every tick of a race. */
        if (s.phase == RALLY_PHASE_RACE || s.phase == RALLY_PHASE_DONE) {
            if (s.race_number != lap_race) {
                lap_race = s.race_number;
                for (int i = 0; i < RALLY_MAX_RACERS; ++i) {
                    last_lap[i] = 1;
                    lap_ups[i] = 0;
                    last_flags[i] = 0;
                }
            }
            CHECK(s.laps == laps && s.course_loop == (loop ? 1 : 0), "the state says %d laps, loop %d", s.laps,
                  s.course_loop);
            for (int i = 0; i < s.racer_count; ++i) {
                const RallyRacer& r = s.racer[i];
                if (r.lap < 1 || r.lap > laps) {
                    if (lap_bad_range++ == 0) CHECK(false, "racer %d is on lap %d of %d", i, r.lap, laps);
                }
                if (r.lap != last_lap[i]) {
                    if (r.lap != last_lap[i] + 1) {
                        if (lap_bad_step++ == 0) CHECK(false, "racer %d went from lap %d to %d", i, last_lap[i], r.lap);
                    } else {
                        ++lap_ups[i];
                        ++lap_counts_seen;
                        /* A lap counts at the line, which crosses the circuit at its first point. */
                        const double d = hypot((double)r.x - course[0].x, (double)r.y - course[0].y);
                        if (loop && d > course[0].half_width + 2.0 * 65536) {
                            if (lap_bad_where++ == 0) {
                                CHECK(false, "racer %d began lap %d %.1f cells from the line", i, r.lap, d / 65536);
                            }
                        }
                    }
                }
                if ((r.flags & RALLY_F_FINISHED) && !(last_flags[i] & RALLY_F_FINISHED)) {
                    /* Finished: on the last lap, and on a circuit at the line. */
                    const double d = hypot((double)r.x - course[0].x, (double)r.y - course[0].y);
                    if (r.lap != laps || last_lap[i] != laps || lap_ups[i] != laps - 1
                        || (loop && d > course[0].half_width + 2.0 * 65536)) {
                        if (lap_bad_finish++ == 0) {
                            CHECK(false, "racer %d finished on lap %d (was %d, %d laps counted of %d) %.1f cells from "
                                  "the line", i, r.lap, last_lap[i], lap_ups[i], laps, d / 65536);
                        }
                    } else {
                        ++finished_all_laps;
                    }
                }
                last_lap[i] = r.lap;
                last_flags[i] = r.flags;
                /* The race-wide progress is the laps done and the progress into this one. */
                if (!(r.flags & (RALLY_F_FINISHED | RALLY_F_RETIRED))) {
                    const double want = ((r.lap - 1) * 65536.0 + r.lap_progress) / laps;
                    if (fabs(want - r.progress) > 2.0) {
                        if (progress_bad++ == 0) {
                            CHECK(false, "racer %d progress %d, but lap %d of %d at %d", i, r.progress, r.lap, laps,
                                  r.lap_progress);
                        }
                    }
                }
            }
            /* Places: a finisher before anyone still racing; among racers on the course, more
               laps or clearly further into the same lap is ahead. */
            for (int a = 0; a < s.racer_count; ++a) {
                for (int b = 0; b < s.racer_count; ++b) {
                    const RallyRacer& ra = s.racer[a];
                    const RallyRacer& rb = s.racer[b];
                    if (a == b || ((ra.flags | rb.flags) & RALLY_F_RETIRED)) continue;
                    bool ahead = false;
                    if ((ra.flags & RALLY_F_FINISHED) && !(rb.flags & RALLY_F_FINISHED)) ahead = true;
                    if (!((ra.flags | rb.flags) & (RALLY_F_FINISHED | RALLY_F_DEAD))
                        && (ra.lap > rb.lap || (ra.lap == rb.lap && ra.lap_progress > rb.lap_progress + 64))) {
                        ahead = true;
                    }
                    if (ahead && ra.place > rb.place) {
                        if (place_bad++ == 0) {
                            CHECK(false, "racer %d (lap %d at %d, place %d) is behind racer %d (lap %d at %d, place %d)",
                                  a, ra.lap, ra.lap_progress, ra.place, b, rb.lap, rb.lap_progress, rb.place);
                        }
                    }
                }
            }
        }
        (void)lap_from;

        /* Put back, never on: the course points each racer comes near, checked whenever a lap
           or the finish counts. */
        if (s.phase == RALLY_PHASE_RACE || s.phase == RALLY_PHASE_DONE) {
            if (s.race_number != travel_race) {
                travel_race = s.race_number;
                memset(near_point, 0, sizeof near_point);
                for (int i = 0; i < RALLY_MAX_RACERS; ++i) {
                    near_count[i] = travel_fin[i] = 0;
                    travel_lap[i] = 1;
                }
            }
            for (int i = 0; i < s.racer_count; ++i) {
                const RallyRacer& r = s.racer[i];
                const bool fin = (r.flags & RALLY_F_FINISHED) != 0;
                if (r.lap != travel_lap[i] || (fin && !travel_fin[i])) {
                    ++travel_checked;
                    const double frac = points > 0 ? near_count[i] / (double)points : 1.0;
                    if (frac < travel_worst) travel_worst = frac;
                    if (frac < 0.75 && travel_bad++ == 0) {
                        CHECK(false, "racer %d was credited with %s having come near only %d of the course's %d "
                              "points since the last lap counted (race %d, %.1f s)", i,
                              fin ? "the finish" : "a lap", near_count[i], points, s.race_number,
                              s.race_ticks / (double)TPS);
                    }
                    memset(near_point[i], 0, sizeof near_point[i]);
                    near_count[i] = 0;
                    travel_lap[i] = r.lap;
                    if (fin) travel_fin[i] = 1;
                }
                if (s.phase != RALLY_PHASE_RACE || r.unit_id < 0
                    || (r.flags & (RALLY_F_DEAD | RALLY_F_FINISHED | RALLY_F_RETIRED))) {
                    continue;
                }
                for (int p = 0; p < points; ++p) {
                    if (near_point[i][p]) continue;
                    const double reach = course[p].half_width + 2.0 * 65536;
                    const double dx = (double)r.x - course[p].x, dy = (double)r.y - course[p].y;
                    if (dx * dx + dy * dy <= reach * reach) {
                        near_point[i][p] = 1;
                        ++near_count[i];
                    }
                }
            }
        }

        /* In the countdown the places are the grid's rows: a racer half a cell or more nearer
           the line is placed ahead. */
        if (s.phase == RALLY_PHASE_COUNTDOWN) {
            for (int a = 0; a < s.racer_count; ++a) {
                for (int b = 0; b < s.racer_count; ++b) {
                    const RallyRacer& ra = s.racer[a];
                    const RallyRacer& rb = s.racer[b];
                    if (a == b || ra.unit_id < 0 || rb.unit_id < 0 || ((ra.flags | rb.flags) & (RALLY_F_DEAD | RALLY_F_RETIRED))) {
                        continue;
                    }
                    if (past_line(ra) > past_line(rb) + 0.5 && ra.place > rb.place && grid_place_bad++ == 0) {
                        CHECK(false, "in the countdown racer %d (%.2f cells past the line) is placed %d, behind racer "
                              "%d (%.2f) placed %d", a, past_line(ra), ra.place, b, past_line(rb), rb.place);
                    }
                }
            }
        }

        /* Race 2's put-back tests. */
        int pb_killed_now = -1;
        if (s.race_number == 2 && s.phase == RALLY_PHASE_COUNTDOWN) {
            for (int i = 0; i < s.racer_count; ++i) {
                pb_grid_x[i] = s.racer[i].x;
                pb_grid_y[i] = s.racer[i].y;
            }
            const RallyRacer& m = s.racer[0];
            if (pb_cd_tick < 0 && m.unit_id >= 0) {
                pb_cd_rescues = rescues_now();
                OpenCNC3D_Rally_Input(me, RALLY_IN_RESCUE, m.x, m.y);
                pb_cd_tick = t;
            } else if (pb_cd_tick >= 0 && t == pb_cd_tick + 1) {
                OpenCNC3D_Rally_Input(me, 0, m.x, m.y);
            }
        }
        if (s.race_number == 2 && s.phase == RALLY_PHASE_RACE) {
            const RallyRacer& m = s.racer[0];
            if (pb_cd_tick >= 0 && pb_cd_ok < 0 && s.race_ticks >= 2) {
                pb_cd_ok = rescues_now() == pb_cd_rescues;
                CHECK(pb_cd_ok, "a rescue pressed on the grid during the countdown was used at GO");
                /* The press at GO, and a computer driver destroyed on the grid. */
                pb_go_rescues = rescues_now();
                OpenCNC3D_Rally_Input(me, RALLY_IN_RESCUE, m.x, m.y);
                pb_go_tick = t;
                pb_go_state = 1;
                if (pb_kill_racer < s.racer_count && OpenCNC3D_Rally_Debug(RALLY_DEBUG_KILL, pb_kill_racer, 0, 0) == 1) {
                    pb_kill_state = 1;
                    pb_kill_tick = t;
                    pb_killed_now = pb_kill_racer;
                }
            } else if (pb_go_state == 1) {
                OpenCNC3D_Rally_Input(me, 0, m.x, m.y);
                pb_go_state = 2;
            } else if (pb_go_state == 2 && t >= pb_go_tick + 3) {
                pb_go_ahead = past_line(m);
                pb_go_from_grid = hypot((double)m.x - pb_grid_x[0], (double)m.y - pb_grid_y[0]) / 65536.0;
                const bool rescued = rescues_now() > pb_go_rescues;
                pb_go_ok = rescued && pb_go_ahead < 0 && m.lap == 1;
                CHECK(rescued, "the rescue key pressed at GO did nothing");
                CHECK(pb_go_ahead < 0 && m.lap == 1, "a rescue pressed at GO put seat 0 %.2f cells past the start "
                      "line on lap %d (%.2f cells from its grid slot)", pb_go_ahead, m.lap, pb_go_from_grid);
                pb_go_state = 3;
            }
            if (pb_kill_state == 1 && pb_kill_racer < s.racer_count) {
                const RallyRacer& k = s.racer[pb_kill_racer];
                if (k.flags & RALLY_F_DEAD) pb_kill_state = 2;
                else if (t > pb_kill_tick + 3) pb_kill_state = 4; /* never died: reported below */
            } else if (pb_kill_state == 2 && pb_kill_racer < s.racer_count) {
                const RallyRacer& k = s.racer[pb_kill_racer];
                if (!(k.flags & RALLY_F_DEAD) && k.unit_id >= 0) {
                    pb_kill_ahead = past_line(k);
                    CHECK(pb_kill_ahead < 0 && k.lap == 1, "racer %d destroyed on the grid came back %.2f cells past "
                          "the start line on lap %d", pb_kill_racer, pb_kill_ahead, k.lap);
                    pb_kill_ok = pb_kill_ahead < 0 && k.lap == 1;
                    pb_kill_watch = t + 2 * TPS;
                    pb_kill_state = 3;
                }
            } else if (pb_kill_state == 3 && t == pb_kill_watch) {
                const RallyRacer& k = s.racer[pb_kill_racer];
                if (k.lap != 1) pb_kill_ok = 0;
                CHECK(k.lap == 1, "racer %d, destroyed on the grid, was on lap %d two seconds after it came back",
                      pb_kill_racer, k.lap);
            }
            /* Seat 0 put back on the line just after its first new lap, by hand: the autopilot
               off and no throttle, so it stands where it was put (exactly on the line, the
               spot that once read as the end of the lap), then the throttle for two seconds. */
            const double myaw = m.yaw * 6.283185307 / 65536;
            const int32_t max_ = m.x + (int32_t)(sin(myaw) * 65536 * 3), may_ = m.y - (int32_t)(cos(myaw) * 65536 * 3);
            if (loop && laps >= 2 && pb_line_state == 0 && m.lap == 2 && !(m.flags & RALLY_F_DEAD)) {
                pb_line_state = 1;
                pb_line_tick = t;
                pb_line_lap = m.lap;
                pb_line_rescues = rescues_now();
                OpenCNC3D_Rally_Autopilot(me, 0);
                OpenCNC3D_Rally_Input(me, RALLY_IN_RESCUE, max_, may_);
            } else if (pb_line_state == 1) {
                OpenCNC3D_Rally_Input(me, 0, max_, may_);
                pb_line_state = 2;
            } else if (pb_line_state == 2 && t >= pb_line_tick + 3) {
                if (getenv("RALLY_PROBE_TRACE"))
                    fprintf(g_rep, "  line test: seat 0 lap %d at %d, checkpoint %d, %.3f cells past the line, %.3f "
                            "from P000\n", m.lap, m.lap_progress, m.checkpoint, past_line(m),
                            hypot((double)m.x - course[0].x, (double)m.y - course[0].y) / 65536.0);
                if (rescues_now() > pb_line_rescues) {
                    pb_line_how = 1;
                    pb_line_watch = t + 2 * TPS;
                    pb_line_state = 4;
                    OpenCNC3D_Rally_Input(me, RALLY_IN_THROTTLE, max_, may_);
                } else if (OpenCNC3D_Rally_Debug(RALLY_DEBUG_KILL, 0, 0, 0) == 1) {
                    /* the rescue is cooling down: a respawn puts it back the same way */
                    pb_line_how = 2;
                    pb_line_state = 3;
                    pb_killed_now = 0;
                } else {
                    OpenCNC3D_Rally_Autopilot(me, 1);
                    pb_line_state = 9;
                }
            } else if (pb_line_state == 3 && !(m.flags & RALLY_F_DEAD) && m.unit_id >= 0) {
                pb_line_watch = t + 2 * TPS;
                pb_line_state = 4;
                OpenCNC3D_Rally_Input(me, RALLY_IN_THROTTLE, max_, may_);
            } else if (pb_line_state == 4) {
                OpenCNC3D_Rally_Input(me, RALLY_IN_THROTTLE, max_, may_);
                const bool more = m.lap != pb_line_lap || (m.flags & RALLY_F_FINISHED);
                if (more && pb_line_ok != 0) {
                    pb_line_ok = 0;
                    CHECK(false, "seat 0, put back on the line by a %s just after lap %d began, was credited with "
                          "another lap %.1f s later", pb_line_how == 1 ? "rescue" : "respawn", pb_line_lap,
                          (t - pb_line_tick) / (double)TPS);
                }
                if (t >= pb_line_watch) {
                    if (pb_line_ok < 0) pb_line_ok = 1;
                    OpenCNC3D_Rally_Input(me, 0, max_, may_);
                    OpenCNC3D_Rally_Autopilot(me, 1);
                    pb_line_state = 5;
                }
            }
        }

        /* Engine state, every tick, for every racer on the course (but one the put-back tests
           above destroyed this tick, whose state was read before it went). */
        for (int i = 0; i < s.racer_count; ++i) {
            const RallyRacer& r = s.racer[i];
            if (r.unit_id < 0 || s.phase < RALLY_PHASE_COUNTDOWN || i == pb_killed_now) continue;
            const int bits = OpenCNC3D_Rally_Debug(RALLY_DEBUG_CHECK, i, 0, 0);
            if (bits != RALLY_CHECK_UNIT) {
                ++check_violations;
                if (check_violations <= 10) {
                    CHECK(false, "racer %d engine check bits 0x%x (unit=%d blocked=%d offmap=%d desync=%d) at %d,%d", i,
                          bits, bits & 1, (bits >> 1) & 1, (bits >> 2) & 1, (bits >> 3) & 1, r.x >> 16, r.y >> 16);
                }
            }
        }

        if (s.phase >= RALLY_PHASE_PICK && t % TPS == 0) {
            const int stray = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STRAY_CRATES, 0, 0, 0);
            if (stray > stray_crates) stray_crates = stray;
            if (stray > 0 && stray_first < 0) stray_first = t;
        }
        if (s.phase == RALLY_PHASE_RACE) {
            const int aiming = OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_AIMING, 0, 0, 0);
            hostile_shots = OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_SHOTS, 0, 0, 0);
            hostile_aim_ticks += aiming > 0;
        }
        /* The tick that ends the race is already DONE, and can still let a drift go. */
        if (s.phase == RALLY_PHASE_RACE || s.phase == RALLY_PHASE_DONE) {
            const int count = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_MINI_TURBOS, 0, 0);
            int fired = count - turbo_count;
            turbo_count = count;
            for (int i = 0; i < s.racer_count; ++i) {
                const int f = s.racer[i].flags;
                if ((f & RALLY_F_DRIFT) && s.phase == RALLY_PHASE_RACE) ++drift_ticks[i];
                if (fired > 0 && (prev_flags[i] & RALLY_F_TURBO_READY) && !(f & RALLY_F_TURBO_READY)
                    && (f & (RALLY_F_BOOST | RALLY_F_DEAD))) {
                    ++turbos[i];
                    --fired;
                }
                prev_flags[i] = f;
            }
            const int walls = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_WALL_HITS, 0, 0);
            int new_hits = walls - wall_count;
            wall_count = walls;
            bool hit_now[RALLY_MAX_RACERS] = {false};
            while (new_hits > 0) {
                int who = -1;
                double most = hit_dv;
                for (int i = 0; i < s.racer_count; ++i) {
                    const RallyRacer& r = s.racer[i];
                    const bool unhurt = (r.flags | was_flags[i]) & (RALLY_F_SHIELDED | RALLY_F_ARMOUR);
                    if (hit_now[i] || (r.health >= prev_health[i] && !unhurt)) continue;
                    const double dv = hypot((double)r.vx - prev_vx[i], (double)r.vy - prev_vy[i]);
                    if (dv >= most) {
                        most = dv;
                        who = i;
                    }
                }
                if (who < 0) break;
                hit_now[who] = true;
                --new_hits;
                ++wall_hits_by[who];
                const RallyRacer& r = s.racer[who];
                if (!(r.flags & RALLY_F_HUMAN)) {
                    ++ai_wall_hits;
                    if (hypot((double)r.x - course[0].x, (double)r.y - course[0].y) < 6.0 * 65536) ++ai_wall_hits_line;
                }
                if (!(r.flags & RALLY_F_HUMAN) && control_back_at[who] >= 0 && t - control_back_at[who] <= 2 * TPS) {
                    if (spin_hits_by[who]++ == 0) {
                        snprintf(spin_hit_note[who], sizeof spin_hit_note[who],
                                 "%.1f s after the spin, at %.1f s, health %d -> %d, %d%% of top speed, %d degrees "
                                 "off the course",
                                 (t - control_back_at[who]) / (double)TPS, s.race_ticks / (double)TPS, prev_health[who],
                                 r.health,
                                 (int)(hypot((double)prev_vx[who], (double)prev_vy[who]) * 100 / (r.top_speed ? r.top_speed : 1)),
                                 off_course_deg(r));
                    }
                }
            }
            for (int i = 0; i < s.racer_count; ++i) {
                const RallyRacer& r = s.racer[i];
                if (r.flags & RALLY_F_DEAD) {
                    spun[i] = 0;
                    control_back_at[i] = -1;
                } else if (r.flags & RALLY_F_SPINNING) {
                    spun[i] = 1;
                    control_back_at[i] = -1;
                } else if (spun[i] && !(r.flags & out_of_control)) {
                    spun[i] = 0;
                    control_back_at[i] = t;
                }
                prev_health[i] = r.health;
                was_flags[i] = r.flags;
                prev_vx[i] = r.vx;
                prev_vy[i] = r.vy;
            }
        }
        if (s.phase == RALLY_PHASE_RACE || s.phase == RALLY_PHASE_DONE) {
            const int rt = s.race_ticks;
            for (int i = 0; i < s.racer_count; ++i) {
                if (start_progress[i] < 0) start_progress[i] = s.racer[i].progress;
                if (s.racer[i].progress > best_progress[i]) best_progress[i] = s.racer[i].progress;
            }
            /* 2 s into the race seat 0 lets go of the controls and is sent into the nearest
               wall at 9 cells/s; a second later the autopilot takes over again. If it rams
               another racer on the way instead (the field is still packed together), the
               test is run again at least 3 s later, once seat 0 is on the course and no
               longer shielded, up to three times. */
            if (s.phase == RALLY_PHASE_RACE && wall_at < 0 && rt >= wall_try_at && rt < 20 * TPS && pw_phase == 0
                && s.racer[0].unit_id >= 0
                && !(s.racer[0].flags & (RALLY_F_DEAD | RALLY_F_FINISHED | RALLY_F_SHIELDED | RALLY_F_ARMOUR
                                         | RALLY_F_AIRBORNE | RALLY_F_FLIPPED | RALLY_F_RIGHTING))) {
                wall_hits0 = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_WALL_HITS, 0, 0);
                wall_rams0 = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_RAMS, 0, 0);
                wall_health0 = s.racer[0].health;
                OpenCNC3D_Rally_Autopilot(me, 0);
                OpenCNC3D_Rally_Input(me, 0, s.racer[0].x, s.racer[0].y);
                if (OpenCNC3D_Rally_Debug(RALLY_DEBUG_RAM_WALL, 0, 900, 0) == 1) wall_at = t;
                else OpenCNC3D_Rally_Autopilot(me, 1);
            }
            if (wall_at >= 0 && t == wall_at + TPS && wall_retries < 3
                && OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_WALL_HITS, 0, 0) == wall_hits0
                && OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_RAMS, 0, 0) > wall_rams0) {
                OpenCNC3D_Rally_Autopilot(me, 1);
                wall_at = -1;
                wall_try_at = rt + 3 * TPS;
                ++wall_retries;
            }
            if (wall_at >= 0 && t == wall_at + TPS) {
                wall_hits = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_WALL_HITS, 0, 0) - wall_hits0;
                wall_health = s.racer[0].health;
                OpenCNC3D_Rally_Autopilot(me, 1);
            }
            /* Throw racer 1 upside down from 8 s into the race, as soon as it is on its
               wheels. If it is destroyed before it has righted itself, the test starts over. */
            const int ff = s.racer[flip_racer].flags;
            if (flip_at >= 0 && righted_at < 0 && (ff & RALLY_F_DEAD)) {
                flip_at = flipped_at = righting_at = -1;
                ++flip_retries;
            }
            if (s.phase == RALLY_PHASE_RACE && flip_at < 0 && rt >= 8 * TPS && rt < 60 * TPS
                && s.racer[flip_racer].unit_id >= 0
                && !(ff & (RALLY_F_DEAD | RALLY_F_AIRBORNE | RALLY_F_FLIPPED | RALLY_F_RIGHTING | RALLY_F_FINISHED))) {
                if (OpenCNC3D_Rally_Debug(RALLY_DEBUG_THROW, flip_racer, 800, 360) == 1) flip_at = t;
            }
            if (flip_at >= 0 && t > flip_at) {
                if (flipped_at < 0 && (ff & RALLY_F_FLIPPED)) flipped_at = t;
                if (flipped_at >= 0 && righting_at < 0 && (ff & RALLY_F_RIGHTING)) righting_at = t;
                if (righting_at >= 0 && righted_at < 0 && !(ff & (RALLY_F_RIGHTING | RALLY_F_FLIPPED))) righted_at = t;
            }
            /* Destroy racer 2 through the engine from 15 s into the race, once it is alive. */
            if (s.phase == RALLY_PHASE_RACE && kill_at < 0 && rt >= 15 * TPS && rt < 60 * TPS
                && s.racer[kill_racer].unit_id >= 0 && !(s.racer[kill_racer].flags & RALLY_F_FINISHED)) {
                old_unit = s.racer[kill_racer].unit_id;
                if (OpenCNC3D_Rally_Debug(RALLY_DEBUG_KILL, kill_racer, 0, 0) == 1) kill_at = t;
            }
            if (kill_at >= 0) {
                const RallyRacer& k = s.racer[kill_racer];
                if (dead_seen_at < 0 && (k.flags & RALLY_F_DEAD) && k.unit_id < 0) dead_seen_at = t;
                if (dead_seen_at >= 0 && respawned_at < 0 && !(k.flags & RALLY_F_DEAD) && k.unit_id >= 0) {
                    respawned_at = t;
                    new_unit = k.unit_id;
                    CHECK(k.flags & RALLY_F_SHIELDED, "respawned racer is not shielded");
                    CHECK(k.health == k.health_max, "respawned racer has %d of %d health", k.health, k.health_max);
                }
            }
            /* Hand out the two items crates give least often, from 20 s and 24 s into the race,
               as soon as the racer's item slot is empty. */
            const int give_unready = RALLY_F_DEAD | RALLY_F_FINISHED | RALLY_F_ROULETTE;
            if (s.phase == RALLY_PHASE_RACE && gave_ion_at < 0 && rt >= 20 * TPS && rt < 60 * TPS
                && s.racer[3].item == RALLY_ITEM_NONE && !(s.racer[3].flags & give_unready)) {
                if (OpenCNC3D_Rally_Debug(RALLY_DEBUG_GIVE, 3, RALLY_ITEM_ION, 1) == 1) gave_ion_at = t;
            }
            if (s.phase == RALLY_PHASE_RACE && gave_homing_at < 0 && rt >= 24 * TPS && rt < 60 * TPS && pw_phase == 0
                && s.racer[0].item == RALLY_ITEM_NONE && !(s.racer[0].flags & give_unready)) {
                if (OpenCNC3D_Rally_Debug(RALLY_DEBUG_GIVE, 0, RALLY_ITEM_HOMING, 1) == 1) gave_homing_at = t;
            }
            /* Seat 0 drives by hand for two seconds through the order queue: full throttle,
               fire at a point ahead, then the rescue key, then back to the autopilot. The test
               starts at the first moment from 25 s into the race that seat 0 is on its wheels,
               in control and more than four seconds at its top speed from the line (so that it
               cannot cross the line before the rescue key). If the defences destroy it inside
               that window before the rescue key has worked, the test starts again as soon as it
               is back on the course in control; if it never gets up to speed because something
               threw or spun it, a second later. A short course has little race left after 25 s,
               and a later retry can find seat 0 too near the line. Once the rescue has worked
               the window runs to its end even if seat 0 is destroyed: the rescue cannot be used
               again for 3 s, so a second attempt would only find the key doing nothing. */
            const RallyRacer& me_r = s.racer[0];
            const int no_control = RALLY_F_FINISHED | RALLY_F_DEAD | RALLY_F_AIRBORNE | RALLY_F_FLIPPED
                                   | RALLY_F_RIGHTING | RALLY_F_SPINNING;
            const bool rescued = manual_from >= 0
                                 && OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_RESCUES, 0, 0) > manual_rescues0;
            if (manual_from >= 0 && manual_to < 0 && manual_retries < 3
                && (((me_r.flags & RALLY_F_DEAD) && !rescued) || (t - manual_from == 2 * TPS && !manual_fast))) {
                OpenCNC3D_Rally_Input(me, 0, me_r.x, me_r.y);
                OpenCNC3D_Rally_Autopilot(me, 1);
                manual_from = -1;
                manual_fast = 0;
                manual_start = (me_r.flags & RALLY_F_DEAD) ? rt : rt + TPS;
                ++manual_retries;
            }
            const double left = (65536.0 - me_r.progress) / 65536.0 * race_length;
            if (s.phase == RALLY_PHASE_RACE && manual_from < 0 && manual_to < 0 && rt >= manual_start && pw_phase == 0
                && rt < 90 * TPS && me_r.unit_id >= 0 && !(me_r.flags & no_control)
                && left > 4.0 * TPS * me_r.top_speed) {
                manual_from = t;
                manual_shots0 = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_SHOTS, 0, 0);
                manual_rescues0 = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_RESCUES, 0, 0);
                CHECK(OpenCNC3D_Rally_Autopilot(me, 0) == 1, "Rally_Autopilot(off) refused");
            }
            if (manual_from >= 0 && manual_to < 0) {
                const int k = t - manual_from;
                const int32_t fx = (int32_t)(sin(me_r.yaw * 6.283185307 / 65536) * 65536);
                const int32_t fy = (int32_t)(-cos(me_r.yaw * 6.283185307 / 65536) * 65536);
                const int32_t ax = me_r.x + fx * 3, ay = me_r.y + fy * 3;
                uint32_t bits = RALLY_IN_THROTTLE | RALLY_IN_FIRE;
                if (k >= 2 * TPS) bits = RALLY_IN_RESCUE;
                OpenCNC3D_Rally_Input(me, bits, ax, ay);
                if (me_r.speed * 100 > me_r.top_speed * 30) manual_fast = 1;
                if (k == 2 * TPS + TPS / 2) {
                    OpenCNC3D_Rally_Input(me, 0, ax, ay);
                    CHECK(OpenCNC3D_Rally_Autopilot(me, 1) == 1, "Rally_Autopilot(on) refused");
                    manual_to = t;
                    manual_shots = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_SHOTS, 0, 0) - manual_shots0;
                    manual_rescues =
                        OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_RESCUES, 0, 0) - manual_rescues0;
                }
            }
        }

        /* Prediction windows (race 1). The lead-in tick turns the autopilot off and sends the
           window's first controls; once that has executed, the prediction is taken from the
           executed state and each later tick sends the next step, which executes on the next
           tick. At the end the race's own state must be the predicted one, unless something
           the prediction leaves out touched seat 0 (a rival within reach, a hit, a throw). */
        {
            const RallyRacer& me_r = s.racer[0];
            const int busy = RALLY_F_FINISHED | RALLY_F_DEAD | RALLY_F_AIRBORNE | RALLY_F_FLIPPED | RALLY_F_RIGHTING
                             | RALLY_F_SPINNING | RALLY_F_SHIELDED;
            const bool manual_busy = manual_from >= 0 && manual_to < 0;
            const bool wall_busy = wall_at >= 0 && t <= wall_at + TPS;
            if (pw_phase == 2) {
                /* The tick that just ran executed step pw_k (a racer that has finished, or is
                   destroyed, drives with no controls at all). */
                if (s.phase == RALLY_PHASE_RACE && !(me_r.flags & (RALLY_F_FINISHED | RALLY_F_DEAD))
                    && (uint32_t)OpenCNC3D_Rally_Debug(RALLY_DEBUG_BITS, 0, 0, 0) != (pw_steps[pw_k].bits & 0xFF)) {
                    ++pw_bits_bad;
                }
                /* A rival two cells off at the end of every tick cannot have touched seat 0 during
                   one (it would have had to close more than three cells in a tick). Damage alone
                   does not move a racer; a throw or a spin does. */
                if (rival_within(s, 2.0)) pw_disturbed = true;
                /* A crate's item comes from the synchronised random stream, which the prediction
                   leaves alone: the item slot is compared only in windows without one. */
                if ((me_r.flags & RALLY_F_ROULETTE)
                    || OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_CRATES, 0, 0) != pw_crates0) {
                    pw_item_touched = true;
                }
                if ((me_r.flags & (RALLY_F_DEAD | RALLY_F_SPINNING | RALLY_F_FINISHED))
                    || OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_THROWN, 0, 0) != pw_thrown0
                    || s.phase != RALLY_PHASE_RACE) {
                    pw_disturbed = true;
                }
                if (++pw_k < PW_TICKS) {
                    OpenCNC3D_Rally_Input(me, pw_steps[pw_k].bits, pw_steps[pw_k].aim_x, pw_steps[pw_k].aim_y);
                } else {
                    const double dev = hypot((double)me_r.x - pw_pred.x, (double)me_r.y - pw_pred.y);
                    const bool exact = me_r.x == pw_pred.x && me_r.y == pw_pred.y && me_r.yaw == pw_pred.yaw
                                       && me_r.speed == pw_pred.speed && me_r.vx == pw_pred.vx && me_r.vy == pw_pred.vy
                                       && me_r.z == pw_pred.z && me_r.turret == pw_pred.turret
                                       && (pw_item_touched || me_r.item_count == pw_pred.item_count)
                                       && (me_r.flags & (RALLY_F_DRIFT | RALLY_F_BOOST | RALLY_F_TURBO_READY))
                                              == (pw_pred.flags & (RALLY_F_DRIFT | RALLY_F_BOOST | RALLY_F_TURBO_READY));
                    if (pw_disturbed) {
                        ++pw_disturbed_count;
                        if (dev > pw_worst_disturbed) pw_worst_disturbed = dev;
                    } else if (exact) {
                        ++pw_exact;
                        if (pw_nitro) ++pw_nitro_exact;
                    } else {
                        ++pw_mismatch;
                        if (dev > pw_worst_undisturbed) pw_worst_undisturbed = dev;
                        if (!pw_note[0]) {
                            snprintf(pw_note, sizeof pw_note,
                                     "window %d: predicted %d,%d yaw %d speed %d turret %d items %d flags %x; raced "
                                     "%d,%d yaw %d speed %d turret %d items %d flags %x",
                                     pw_window, pw_pred.x, pw_pred.y, pw_pred.yaw, pw_pred.speed, pw_pred.turret,
                                     pw_pred.item_count, pw_pred.flags, me_r.x, me_r.y, me_r.yaw, me_r.speed,
                                     me_r.turret, me_r.item_count, me_r.flags);
                        }
                    }
                    OpenCNC3D_Rally_Autopilot(me, 1);
                    pw_phase = 0;
                    pw_next = s.race_ticks + 2 * TPS;
                    ++pw_window;
                }
            } else if (pw_phase == 1) {
                /* The lead-in executed: seat 0 is on its own controls now. */
                const int ok = OpenCNC3D_Rally_Predict(me, pw_steps, PW_TICKS, &pw_pred);
                ++predicts_called;
                if (ok != 1 || s.phase != RALLY_PHASE_RACE) {
                    ++pw_refused;
                    OpenCNC3D_Rally_Autopilot(me, 1);
                    pw_phase = 0;
                    pw_next = s.race_ticks + TPS;
                } else {
                    pw_phase = 2;
                    pw_k = 0;
                    pw_disturbed = false;
                    pw_item_touched = (me_r.flags & RALLY_F_ROULETTE) != 0;
                    pw_crates0 = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_CRATES, 0, 0);
                    pw_thrown0 = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_THROWN, 0, 0);
                    OpenCNC3D_Rally_Input(me, pw_steps[0].bits, pw_steps[0].aim_x, pw_steps[0].aim_y);
                }
            } else if (s.phase == RALLY_PHASE_RACE && s.race_number == 1 && pw_window < PW_MAX
                       && s.race_ticks >= pw_next && !manual_busy && !wall_busy && me_r.unit_id >= 0
                       && (s.race_ticks < 23 * TPS || manual_to >= 0) && !(me_r.flags & busy)
                       && me_r.speed * 100 >= me_r.top_speed * 30 && off_course_deg(me_r) < 40 && !rival_within(s, 3.0)
                       && OpenCNC3D_Rally_Debug(RALLY_DEBUG_STRENGTH, 0, 0, 0) > 0) {
                /* Every third window carries a nitro, fired twice from the item slot. */
                pw_nitro = pw_window % 3 == 1;
                if (pw_nitro && !(me_r.flags & RALLY_F_ROULETTE)) {
                    OpenCNC3D_Rally_Debug(RALLY_DEBUG_GIVE, 0, RALLY_ITEM_NITRO3, 3);
                } else {
                    pw_nitro = false;
                }
                const double yaw = me_r.yaw * 6.283185307 / 65536;
                const int32_t ax = me_r.x + (int32_t)(sin(yaw) * 65536 * 3), ay = me_r.y - (int32_t)(cos(yaw) * 65536 * 3);
                window_controls(pw_window + seed * 100, pw_nitro, ax, ay, pw_steps, PW_TICKS);
                OpenCNC3D_Rally_Autopilot(me, 0);
                OpenCNC3D_Rally_Input(me, RALLY_IN_THROTTLE, ax, ay);
                pw_phase = 1;
            }
        }

        /* The results: the deciding player, a choice from another seat, the course broken by
           the test hook, seat 0's retry and the course put back; after the last race, END. */
        if (s.phase == RALLY_PHASE_DONE && s.race_number >= 1 && s.race_number <= 2 && done_at[s.race_number] < 0) {
            done_at[s.race_number] = t;
            if (s.race_number == 1) {
                s1 = s;
                for (int i = 0; i < RALLY_STAT_COUNT; ++i) stats1[i] = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, i, 0, 0);
                for (int i = 0; i < s.racer_count; ++i) {
                    race1_type[i] = s.racer[i].unit_type;
                    for (int k = 0; k < roster_count; ++k) {
                        if (roster[k] == s.racer[i].unit_type) race1_roster[i] = k;
                    }
                }
            } else {
                s2 = s;
            }
            decider_seen = s.decider;
            choice_seen_done = s.choice;
            phase_ticks_done = s.phase_ticks;
            if (pw_phase != 0) { /* a window cut short by the end of the race */
                OpenCNC3D_Rally_Autopilot(me, 1);
                pw_phase = 0;
            }
        }
        if (s.phase == RALLY_PHASE_DONE && s.race_number == 1 && races == 2) {
            if (t == done_at[1] + 2) {
                /* Seat 1 is a computer seat: its choice must be ignored. */
                OpenCNC3D_Rally_Choose(me + 1, RALLY_CHOICE_RETRY);
                nondecider_at = t;
                /* Break the course: the first live defence destroyed, the next one damaged. */
                fixtures_total = OpenCNC3D_Rally_Debug(RALLY_DEBUG_FIXTURES, 0, 0, 0);
                OpenCNC3D_Rally_Debug(RALLY_DEBUG_BREAK, 0, 100, 0);
                OpenCNC3D_Rally_Debug(RALLY_DEBUG_BREAK, 0, 50, 0);
                broke_at = t;
            }
            if (t == done_at[1] + 6) {
                still_done_after_nondecider = s.choice == RALLY_CHOICE_NONE ? 1 : 0;
                fixtures_broken = OpenCNC3D_Rally_Debug(RALLY_DEBUG_FIXTURES, 1, 0, 0);
                CHECK(OpenCNC3D_Rally_Choose(me, RALLY_CHOICE_RETRY) == 1, "Rally_Choose(retry) refused");
                retry_sent_at = t;
            }
        }
        if (s.phase == RALLY_PHASE_PICK && s.race_number == 2 && retry_seen_at < 0) {
            retry_seen_at = t;
            /* The course as it started, and every racer off it, unpicked, with nothing. */
            const int intact = OpenCNC3D_Rally_Debug(RALLY_DEBUG_FIXTURES, 1, 0, 0);
            const int hostile = OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_BUILDINGS, 0, 0, 0);
            int crates_out = 0, racers_bad = 0;
            for (int i = 0; i < s.crate_count; ++i) crates_out += !s.crate[i].active;
            for (int i = 0; i < s.racer_count; ++i) {
                const RallyRacer& r = s.racer[i];
                const int extra = r.flags & ~(RALLY_F_HUMAN | RALLY_F_RETIRED);
                if (r.unit_id >= 0 || r.item != RALLY_ITEM_NONE || extra || r.lap != 1 || r.progress != 0
                    || r.finish_ticks != -1 || r.kills != 0 || r.last_vehicle != race1_roster[i]) {
                    ++racers_bad;
                }
            }
            snprintf(reset_note, sizeof reset_note,
                     "fixtures %d of %d standing as they started (%d after the break), hostile buildings %d (%d at the "
                     "start), crates out %d, hazards %d, racers not reset %d, stray crates %d",
                     intact, fixtures_total, fixtures_broken, hostile, hostile_start, crates_out, s.hazard_count,
                     racers_bad, OpenCNC3D_Rally_Debug(RALLY_DEBUG_STRAY_CRATES, 0, 0, 0));
            reset_ok = intact == fixtures_total && fixtures_total > 0 && fixtures_broken < fixtures_total
                       && hostile == hostile_start && crates_out == 0 && s.hazard_count == 0 && racers_bad == 0;
            /* Seat 0 (a player) shows no vehicle until it picks; computer seats keep theirs. */
            race2_pick_hidden = s.racer[0].unit_type == -1 ? 1 : 0;
            race2_presel_ok = 1;
            for (int i = 1; i < s.racer_count; ++i) {
                if (s.racer[i].unit_type != race1_type[i]) race2_presel_ok = 0;
            }
        }
        if (s.phase == RALLY_PHASE_COUNTDOWN && s.race_number == 2 && race2_kept < 0) {
            /* Seat 0 chose nothing: it races the vehicle of race 1 again. */
            race2_kept = s.racer[0].unit_type == race1_type[0] ? 1 : 0;
        }
        if (s.phase == RALLY_PHASE_RACE && hostile_start < 0 && s.race_number == 1) {
            hostile_start = OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_BUILDINGS, 0, 0, 0);
        }
        if (s.phase == RALLY_PHASE_DONE && s.race_number == races && done_at[races] >= 0 && t == done_at[races] + 2) {
            if (races == 2) race2_finished = s.finished_count;
            CHECK(OpenCNC3D_Rally_Choose(me, RALLY_CHOICE_END) == 1, "Rally_Choose(end) refused");
            end_sent_at = t;
        }
        if (s.phase == RALLY_PHASE_OVER && over_at < 0) {
            over_at = t;
            CHECK(s.choice == RALLY_CHOICE_END, "OVER with choice %d", s.choice);
        }

        {
            const uint32_t hash = OpenCNC3D_Rally_Hash();
            fprintf(hashes, "%d %08x\n", t, hash);
        }
        if (t % TPS == 0) {
            const uint32_t hash = OpenCNC3D_Rally_Hash();
            char line[512];
            int n = snprintf(line, sizeof line, "t=%3ds %-5s#%d %4d |", t / TPS, phase_name(s.phase), s.race_number,
                             s.phase == RALLY_PHASE_RACE ? s.race_ticks : s.phase_ticks);
            for (int i = 0; i < s.racer_count && n < (int)sizeof line - 64; ++i) {
                const RallyRacer& r = s.racer[i];
                n += snprintf(line + n, sizeof line - n, " %d:P%d L%d %3d%% %3d/%-3d %04x %-6s|", i, r.place, r.lap,
                              r.progress * 100 / 65536, r.health, r.health_max, r.flags & 0xffff,
                              (r.flags & RALLY_F_ROULETTE) ? "spin" : item_name(r.item));
            }
            snprintf(line + n, sizeof line - n, " H=%08x", hash);
            fprintf(g_rep, "%s\n", line);
            if (getenv("RALLY_PROBE_VERBOSE")) {
                for (int i = 0; i < s.racer_count; ++i) {
                    const RallyRacer& r = s.racer[i];
                    fprintf(g_rep, "      %d: at %.2f,%.2f z %.2f yaw %3d speed %.2f c/s cp %d unit %d type %d\n", i,
                            r.x / 65536.0, r.y / 65536.0, r.z / 65536.0, r.yaw * 360 / 65536,
                            r.speed * (double)TPS / 65536.0, r.checkpoint, r.unit_id, r.unit_type);
                }
            }
        }
    }
    fclose(hashes);

    fprintf(g_rep, "\n== results ==\n");
    fprintf(g_rep, "ended at tick %d (%s), GAME_OVER callbacks %d at tick %d\n", ended_at,
                    ended_at >= 0 ? "CNC_Advance_Instance returned false" : "tick budget ran out", g_gameover,
                    g_gameover_tick);
    for (int i = 0; i < RALLY_STAT_COUNT; ++i) {
        fprintf(g_rep, "  %-13s %d\n", stat_names[i], OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, i, 0, 0));
    }
    const int stat_crates = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_CRATES, 0, 0);
    const int stat_items = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_ITEMS, 0, 0);
    const int stat_hazards = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_HAZARDS, 0, 0);
    const int stat_blasts = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_BLASTS, 0, 0);
    const int stat_defence = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_DEFENCE_HITS, 0, 0);
    const int stat_shots = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_SHOTS, 0, 0);

    for (int race = 1; race <= races; ++race) {
        const RallyState& rs = race == 1 ? s1 : s2;
        fprintf(g_rep, "race %d: finished %d, winner racer %d, finish order:", race, rs.finished_count, rs.winner);
        for (int i = 0; i < rs.finished_count; ++i) {
            fprintf(g_rep, " %d(%.1fs)", rs.finish_order[i], rs.racer[rs.finish_order[i]].finish_ticks / (double)TPS);
        }
        fprintf(g_rep, "\n");
    }
    fprintf(g_rep, "laps: %d counted at the line, %d racers finished every lap; failures: range %d, step %d, place "
                   "%d, finish %d, where %d, progress %d\n",
            lap_counts_seen, finished_all_laps, lap_bad_range, lap_bad_step, place_bad, lap_bad_finish, lap_bad_where,
            progress_bad);
    fprintf(g_rep, "results: race 1 at tick %d (decider %d, choice %d, phase_ticks %d), seat 1's choice at %d ignored "
                   "%d, course broken at %d, retry sent %d and seen %d; reset: %s; race 2 preselection %d, seat 0 "
                   "hidden in PICK %d and kept its vehicle %d, race 2 results at %d with %d finished, END at %d, OVER "
                   "at %d\n",
            done_at[1], decider_seen, choice_seen_done, phase_ticks_done, nondecider_at, still_done_after_nondecider,
            broke_at, retry_sent_at, retry_seen_at, reset_note[0] ? reset_note : "-", race2_presel_ok,
            race2_pick_hidden, race2_kept, done_at[2], race2_finished, end_sent_at, over_at);
    fprintf(g_rep, "prediction: %d windows, %d exact (%d with a nitro), %d not exact (worst %.0f fine), %d touched by "
                   "something else (worst %.0f fine), %d refused, %d ticks whose controls were not the step's; %d "
                   "predictions asked for, %d of the purity run's ran the driving physics\n",
            pw_window, pw_exact, pw_nitro_exact, pw_mismatch, pw_worst_undisturbed, pw_disturbed_count,
            pw_worst_disturbed, pw_refused, pw_bits_bad, predicts_called, predicts_simulated);
    fprintf(g_rep, "put back: %d lap counts checked, the fewest course points come near before one %.0f%%, %d "
                   "short; countdown places %d wrong; race 2: countdown press %s, press at GO %s (%.2f cells past the "
                   "line, %.2f from the grid slot), grid kill %s (%.2f cells past the line), back on the line after a "
                   "new lap %s%s\n",
            travel_checked, travel_worst * 100, travel_bad, grid_place_bad,
            pb_cd_ok < 0 ? "not run" : pb_cd_ok ? "dropped" : "USED", pb_go_ok < 0 ? "not run" : pb_go_ok ? "ok" : "FAIL",
            pb_go_ahead, pb_go_from_grid,
            pb_kill_ok < 0 ? (pb_kill_state == 4 ? "never died" : "not run") : pb_kill_ok ? "ok" : "FAIL", pb_kill_ahead,
            pb_line_ok < 0 ? (loop && laps >= 2 ? "not run" : "not asked (one lap)") : pb_line_ok ? "ok" : "FAIL",
            pb_line_how == 1 ? " by a rescue" : pb_line_how == 2 ? " by a respawn" : "");
    if (races >= 2) {
        CHECK(pb_cd_ok >= 0 && pb_go_ok >= 0, "the race 2 rescue tests on the grid never ran");
        CHECK(pb_kill_ok >= 0, "the race 2 grid kill test never ran");
    }
    int turbos_all = 0, turbos_ai = 0, turbos_best = -1, walls_seen = 0;
    for (int i = 0; i < s.racer_count; ++i) {
        const RallyRacer& r = s.racer[i];
        fprintf(g_rep,
                "  racer %d house %d type %d skill %d%s place %d progress %d%% (start %d%%, best %d%%) kills %d, "
                "drifting %.1fs, mini-turbos %d, wall hits %d (%d just after a spin)\n",
                i, r.house, r.unit_type, g_levels[i], (r.flags & RALLY_F_HUMAN) ? " (autopilot)" : "", r.place,
                r.progress * 100 / 65536, start_progress[i] * 100 / 65536, best_progress[i] * 100 / 65536, r.kills,
                drift_ticks[i] / (double)TPS, turbos[i], wall_hits_by[i], spin_hits_by[i]);
        turbos_all += turbos[i];
        walls_seen += wall_hits_by[i];
        if (!(r.flags & RALLY_F_HUMAN)) {
            turbos_ai += turbos[i];
            if (g_levels[i] == 3 && turbos[i] > turbos_best) turbos_best = turbos[i];
        }
    }
    const int stat_turbos = OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_MINI_TURBOS, 0, 0);
    fprintf(g_rep, "mini-turbos: %d seen per racer (%d by computer drivers), %d counted by the brain\n", turbos_all,
            turbos_ai, stat_turbos);
    fprintf(g_rep, "wall hits: %d given to a racer, %d counted by the brain; computer drivers %d, of them %d within six "
                   "cells of the start line\n",
            walls_seen, OpenCNC3D_Rally_Debug(RALLY_DEBUG_STAT, RALLY_STAT_WALL_HITS, 0, 0), ai_wall_hits,
            ai_wall_hits_line);
    fprintf(g_rep, "flip test: thrown %d, flipped %d, righting %d, upright %d (ticks), restarts %d\n", flip_at,
            flipped_at, righting_at, righted_at, flip_retries);
    fprintf(g_rep, "kill test: killed %d, dead %d, respawned %d, unit %d -> %d\n", kill_at, dead_seen_at, respawned_at,
            old_unit, new_unit);
    fprintf(g_rep, "wall test: sent at tick %d, hard wall hits within a second %d, health %d -> %d, restarts %d\n",
            wall_at, wall_hits, wall_health0, wall_health, wall_retries);
    fprintf(g_rep, "items given: ion %d, homing %d\n", gave_ion_at, gave_homing_at);
    fprintf(g_rep, "manual driving: ticks %d to %d, reached 30%% of top speed %d, shots fired meanwhile %d, rescues %d, "
                   "restarts %d\n",
            manual_from, manual_to, manual_fast, manual_shots, manual_rescues, manual_retries);
    /* StructType values of the defences: 1 guard tower, 2 advanced guard tower, 3 obelisk, 5 gun turret. */
    fprintf(g_rep, "hostile defences: %d buildings, aiming at a racer in %d race ticks, %d shots (obelisk %d, "
                   "turret %d, guard tower %d, advanced guard tower %d)\n",
            OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_BUILDINGS, 0, 0, 0), hostile_aim_ticks, hostile_shots,
            OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_SHOTS_BY, 3, 0, 0),
            OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_SHOTS_BY, 5, 0, 0),
            OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_SHOTS_BY, 1, 0, 0),
            OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_SHOTS_BY, 2, 0, 0));

    /* -------------------------------------------------------------- assertions -- */
    g_tick = ended_at;
    CHECK((phases_seen & 0x3e) == 0x3e, "phases seen 0x%x, not PICK COUNTDOWN RACE DONE OVER", phases_seen);
    CHECK(race_start_tick >= 0, "the race never started");
    for (int i = 0; i < s.racer_count; ++i) {
        CHECK(best_progress[i] - start_progress[i] > 65536 / 4, "racer %d only got from %d%% to %d%%", i,
              start_progress[i] * 100 / 65536, best_progress[i] * 100 / 65536);
    }
    CHECK(check_violations == 0, "%d engine state violations", check_violations);
    CHECK(s1.finished_count >= 1 && s1.winner >= 0, "nobody finished race 1 (finished %d, winner %d)",
          s1.finished_count, s1.winner);
    CHECK(s.finished_count >= 1 && s.winner >= 0, "nobody finished the last race (finished %d, winner %d)",
          s.finished_count, s.winner);
    /* Laps. */
    CHECK(lap_bad_range == 0 && lap_bad_step == 0 && lap_bad_where == 0 && lap_bad_finish == 0,
          "lap counting failed (range %d, step %d, where %d, finish %d)", lap_bad_range, lap_bad_step, lap_bad_where,
          lap_bad_finish);
    CHECK(place_bad == 0, "%d ticks with a place against the laps and progress", place_bad);
    CHECK(progress_bad == 0, "%d ticks with the race progress against the lap", progress_bad);
    CHECK(finished_all_laps >= s1.finished_count, "%d finishers in race 1, %d seen finishing every lap",
          s1.finished_count, finished_all_laps);
    if (loop && laps > 1) CHECK(lap_counts_seen >= laps - 1, "only %d laps counted at the line", lap_counts_seen);
    /* The results and the choice. */
    CHECK(done_at[1] >= 0, "race 1 never reached its results");
    CHECK(decider_seen == 0, "the deciding player is racer %d, not seat 0", decider_seen);
    CHECK(choice_seen_done == RALLY_CHOICE_NONE, "the results opened with choice %d", choice_seen_done);
    CHECK(phase_ticks_done == 0, "the results count down (%d ticks) with a player to decide", phase_ticks_done);
    if (races == 2) {
        CHECK(still_done_after_nondecider == 1, "a computer seat's choice was acted on");
        CHECK(retry_seen_at >= 0 && retry_seen_at > retry_sent_at, "the retry never went back to PICK");
        CHECK(reset_ok == 1, "the retry did not put the course back: %s", reset_note);
        CHECK(race2_presel_ok == 1, "a computer seat did not keep its vehicle on the retry");
        CHECK(race2_pick_hidden == 1, "seat 0 showed a vehicle in PICK before choosing");
        CHECK(race2_kept == 1, "seat 0 chose nothing and did not race its last vehicle");
        CHECK(done_at[2] >= 0 && race2_finished >= 1, "race 2 did not finish (results at %d, %d finished)", done_at[2],
              race2_finished);
    }
    CHECK(over_at >= 0 && over_at > end_sent_at && end_sent_at >= 0, "END (sent at %d) did not reach OVER (%d)",
          end_sent_at, over_at);
    /* Prediction. */
    CHECK(pw_mismatch == 0, "%d prediction windows untouched by anything else ended away from the prediction; %s",
          pw_mismatch, pw_note);
    CHECK(pw_exact >= 2, "only %d prediction windows were exact and untouched (%d touched, %d refused)", pw_exact,
          pw_disturbed_count, pw_refused);
    CHECK(pw_bits_bad == 0, "%d ticks drove with controls other than the step sent the tick before", pw_bits_bad);
    CHECK(ended_at >= 0 && g_gameover >= 1, "the match did not end through the engine's game over");
    CHECK(g_winners == 1, "game over reported %d winners", g_winners);
    if (s.winner >= 0 && g_winners == 1) {
        CHECK(g_winner_id == 1000 + (uint64)s.winner, "engine winner id %llu is not racer %d",
              (unsigned long long)g_winner_id, s.winner);
    }
    CHECK(flipped_at >= 0, "the thrown racer never landed flipped");
    CHECK(righting_at >= 0 && righted_at >= 0, "the flipped racer did not right itself");
    if (flipped_at >= 0 && righting_at >= 0) {
        const int flipped_for = righting_at - flipped_at;
        CHECK(flipped_for >= 3 * TPS - 1 && flipped_for <= 3 * TPS + 2, "lay flipped for %d ticks, not 3 s",
              flipped_for);
    }
    CHECK(dead_seen_at >= 0, "the destroyed racer was never reported dead");
    CHECK(respawned_at >= 0 && new_unit >= 0, "the destroyed racer never respawned");
    if (dead_seen_at >= 0 && respawned_at >= 0) {
        const int waited = respawned_at - dead_seen_at;
        CHECK(waited >= 3 * TPS - 2 && waited <= 3 * TPS + 4, "respawned after %d ticks, not 3 s", waited);
    }
    CHECK(wall_at >= 0, "no wall within six cells of seat 0 for the wall test");
    if (wall_at >= 0) {
        CHECK(wall_hits >= 1, "seat 0 sent into a wall at 9 cells/s made no hard wall hit");
        CHECK(wall_health < wall_health0, "the hard wall hit cost no health (%d -> %d)", wall_health0, wall_health);
    }
    CHECK(manual_from >= 0, "the manual driving test never ran");
    if (manual_from >= 0) {
        CHECK(manual_to >= 0, "the manual driving window never closed");
        CHECK(manual_fast, "seat 0 never reached 30%% of its top speed on its own controls");
        CHECK(manual_rescues >= 1, "the rescue key did nothing");
    }
    CHECK(stat_crates > 0, "no crate was picked up");
    CHECK(stat_items > 0, "no item was used");
    CHECK(stat_hazards > 0 && stat_blasts > 0, "items made %d hazards and %d blasts", stat_hazards, stat_blasts);
    CHECK(stat_shots > 0, "no racer fired its weapon");
    CHECK(hostile_aim_ticks > 0 && hostile_shots > 0, "the hostile defences never aimed at and fired on a racer");
    CHECK(OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_POWER, 0, 0, 0) == 1, "the hostile house is not powered");
    /* StructType 3 is the obelisk; SCM71EA has two on the course. */
    if (!strcmp(scen, "SCM71EA")) {
        CHECK(OpenCNC3D_Rally_Debug(RALLY_DEBUG_HOSTILE_SHOTS_BY, 3, 0, 0) >= 1, "no obelisk fired on SCM71EA");
    }
    fprintf(g_rep, "stray crates (not at a course crate spot): at most %d, first at tick %d\n", stray_crates,
            stray_first);
    CHECK(stray_crates == 0, "%d crates appeared away from the course's crate spots", stray_crates);
    /* The computer drivers drift the bends that warrant it and let go for the mini-turbo.
       Measured over 128 seeds a course, the three of them earn at least 5 in a race, and the
       skill 3 driver at least 2 in all but one race of 512 (1 there), so these bounds catch
       drifting that broke for most of the field rather than one unlucky race. */
    CHECK(turbos_ai >= 3, "the computer drivers earned %d drift mini-turbos on %s, not at least 3", turbos_ai, scen);
    CHECK(turbos_best >= 1, "the skill 3 computer driver earned no drift mini-turbo on %s", scen);
    /* A spun car facing the wrong way turns back round slowly, so it does not end up in the
       outside wall. */
    for (int i = 0; i < s.racer_count; ++i) {
        CHECK(spin_hits_by[i] == 0, "computer driver %d hit a wall %d time(s) just after a spin; the first %s", i,
              spin_hits_by[i], spin_hit_note[i]);
    }

    /* One line for batch runs: course seed winner winner_s finishers wall_hits rams respawns
       rescues flips defence_kills turbos_brain, then per racer type:skill:turbos:drift_ticks:
       finish_ticks (-1 unfinished):progress%:wall_hits:wall_hits_just_after_a_spin. */
    {
        char line[1024];
        /* Race 1 and the counters at its results, so a run with a retry reads like one race. */
        int n = snprintf(line, sizeof line, "SUMMARY %s %d %d %.2f %d %d %d %d %d %d %d %d", scen, seed, s1.winner,
                         s1.winner >= 0 ? s1.racer[s1.winner].finish_ticks / (double)TPS : -1.0, s1.finished_count,
                         stats1[RALLY_STAT_WALL_HITS], stats1[RALLY_STAT_RAMS], stats1[RALLY_STAT_RESPAWNS],
                         stats1[RALLY_STAT_RESCUES], stats1[RALLY_STAT_FLIPS], stats1[RALLY_STAT_DEFENCE_KILLS],
                         stats1[RALLY_STAT_MINI_TURBOS]);
        for (int i = 0; i < s1.racer_count && n < (int)sizeof line - 48; ++i) {
            const RallyRacer& r = s1.racer[i];
            n += snprintf(line + n, sizeof line - n, " %d:%d:%d:%d:%d:%d:%d:%d", r.unit_type, g_levels[i], turbos[i],
                          drift_ticks[i], r.finish_ticks, r.progress * 100 / 65536, wall_hits_by[i], spin_hits_by[i]);
        }
        fprintf(g_rep, "%s\n", line);
    }

    fprintf(g_rep, "%s: %d failed assertion(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    fflush(g_rep);
    return g_failures ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc < 6) {
        fprintf(stderr,
                "usage: %s <dylib> <scenario_dir_ending_in_slash> <scenario> <content_dir> <logfile> [seed] "
                "[max_seconds] [laps] [races]\n",
                argv[0]);
        return 2;
    }
    char hash_a[1024], hash_b[1024], log_b[1024];
    snprintf(hash_a, sizeof hash_a, "%s.hashA", argv[5]);
    snprintf(hash_b, sizeof hash_b, "%s.hashB", argv[5]);
    snprintf(log_b, sizeof log_b, "%s.B", argv[5]);

    int status[2] = {-1, -1};
    for (int run = 0; run < 2; ++run) {
        fflush(stdout);
        printf("==== run %c ====\n", 'A' + run);
        fflush(stdout);
        const pid_t pid = fork();
        if (pid == 0) {
            if (run == 1) argv[5] = log_b;
            /* Run B also predicts heavily every tick; it must race exactly as run A. */
            _exit(run_race(argc, argv, run == 0 ? hash_a : hash_b, run == 1));
        }
        int st = 0;
        waitpid(pid, &st, 0);
        status[run] = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
        printf("run %c exit %d\n", 'A' + run, status[run]);
    }

    /* Same seed, same race, with or without predictions: the hash sequences (one line a tick)
       must match line for line. */
    FILE* a = fopen(hash_a, "r");
    FILE* b = fopen(hash_b, "r");
    int lines = 0, differ = 0, first_diff = -1;
    if (a && b) {
        char la[64], lb[64];
        for (;;) {
            char* ra = fgets(la, sizeof la, a);
            char* rb = fgets(lb, sizeof lb, b);
            if (!ra && !rb) break;
            if (!ra || !rb || strcmp(la, lb) != 0) {
                ++differ;
                if (first_diff < 0) first_diff = lines;
            }
            ++lines;
        }
    }
    if (a) fclose(a);
    if (b) fclose(b);
    printf("determinism: %d hash lines compared, %d differ%s\n", lines, differ,
           differ ? " (FAIL)" : "");
    if (differ) printf("  first difference at line %d\n", first_diff);
    const bool ok = status[0] == 0 && status[1] == 0 && lines > 0 && differ == 0;
    printf("%s\n", ok ? "RALLY PROBE PASSED" : "RALLY PROBE FAILED");
    return ok ? 0 : 1;
}
