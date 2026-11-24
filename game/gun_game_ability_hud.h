#pragma once

/* GUN GAME ABILITIES, THE RENDERER'S HALF.

   The engine owns every rule and every timer (gun_game_ability_abi.h). This file owns
   what the player sees and does: the ability cameo beside the superweapon cameo, the
   twenty activation mini-games, the drawing of what the abilities do to the world and
   the minimap, and the script verbs that test all of it headless.

   EVERY MINI-GAME IS A RECOGNISER FED WITH ABSTRACT EVENTS (GaEv) ON ITS OWN CLOCK. The
   live mouse and keyboard, the gaauto / gafail script verbs and the gaselftest verb all
   reach the same decision code: nothing below asks SDL for the time or the pointer. A
   world adapter (GaWorld) answers where the player's combatant and the enemies stand on
   screen and how a screen point maps to a cell; the live adapter reads this renderer's
   objects and camera, the self test's is a fixed synthetic map.

   A FAILED, TIMED OUT OR CANCELLED TRY KEEPS THE ABILITY and blocks the next try for
   GA_RETRY_MS. Only a recognised gesture calls OpenCNC3D_Gun_Ability_Use, and a refusal
   from the engine there counts as a failed try too.

   An export the engine does not have turns the feature off rather than failing: with no
   OpenCNC3D_Gun_Ability_Held there is no cameo, with no OpenCNC3D_Gun_Effects nothing
   is drawn. OPENCNC3D_GA_FAKE=1 replaces the engine side with a local stand-in (held
   ability, flags, effects and timers set by the script verbs, or a demonstration set of
   every effect with OPENCNC3D_GA_FAKE_FX=demo) so the drawing can be looked at without a
   match. */

#include <functional>

/* ---- names ------------------------------------------------------------------------ */

static const char* const GA_NAME[GA_COUNT] = {
    "", "STEALTH", "CHRONO HOP", "CHRONO SWAP", "IRON CURTAIN", "OVERDRIVE",
    "CHRONO VORTEX", "MINEFIELD", "HUNTER'S EYE", "HUMILIATION DART", "CHAIN SHOCK",
    "EXTRA LIFE", "SMOKE SCREEN", "DECOYS", "GIANT MODE", "LAST LAUGH", "HOT POTATO",
    "BOOBY-TRAPPED CRATE", "WORMHOLE", "DANCE OFF", "MIRROR MADNESS"};

/* Two short lines for the placeholder cameo, eight characters at most each. */
static const char* const GA_SHORT[GA_COUNT] = {
    "", "STEALTH", "CHRONO HOP", "CHRONO SWAP", "IRON CURTAIN", "OVER DRIVE", "VORTEX",
    "MINE FIELD", "HUNTER'S EYE", "DART", "CHAIN SHOCK", "EXTRA LIFE", "SMOKE",
    "DECOYS", "GIANT", "LAST LAUGH", "HOT POTATO", "BOOBY CRATE", "WORM HOLE",
    "DANCE OFF", "MIRROR"};

static const char* const GA_GAME[GA_COUNT] = {
    "", "HOLD STILL", "SLINGSHOT", "LASSO", "CRANK IT DOWN", "REV THE ENGINE", "SPIRAL",
    "PLANT THEM", "TUNE THE RADAR", "GOLF SWING", "CONNECT THE DOTS", "HEARTBEAT",
    "PAINT THE SMOKE", "SHELL GAME", "PUMP IT UP", "CUT THE RIGHT WIRE", "FLICK",
    "GIFT WRAP", "FOLD THE MAP", "ARROW FLICKS", "MIRROR DRAWING"};

static const char* const GA_HINT[GA_COUNT] = {
    "", "KEEP THE MOUSE STILL IN THE RING", "GRAB YOUR UNIT, PULL BACK, LET GO",
    "DRAW A LOOP AROUND AN ENEMY", "HAMMER SPACE", "SHAKE LEFT-RIGHT, CLICK IN THE RED",
    "DRAW AN INWARD SPIRAL, 2 TURNS", "DRAG FIVE MINES NEAR YOUR UNIT",
    "WHEEL TO TUNE, CLICK WHEN CLEAN", "CLICK THE NEEDLE ON THE NOTCH",
    "ONE STROKE THROUGH 1, 2, 3", "CLICK ON EVERY BEEP", "SCRIBBLE SMOKE ON THE MAP",
    "WATCH YOUR CARD, THEN CLICK IT", "HOLD THE BUTTON, PUMP UP 5 TIMES", "",
    "GRAB THE BOMB, FLICK AT AN ENEMY", "BOMB IN, LID ON, BOW ON TOP",
    "RIGHT-DRAG FROM ONE CELL TO ANOTHER", "FLICK THE MOUSE WITH THE ARROWS",
    "DRAW THE MIRROR IMAGE ON THE RIGHT"};

/* Each try's whole time. Every one of them is finishable in three to four seconds; the
   rest is slack, and the two that first ask for a target have more. */
static const Uint32 GA_LIMIT[GA_COUNT] = {
    0, 4000, 4500, 4500, 4500, 4500, 4500, 5000, 4500, 6000, 4000,
    4000, 4500, 5000, 4500, 5000, 4500, 5000, 4500, 4500, 7000};

/* ---- the engine glue ---------------------------------------------------------------- */

static bool g_gaFake = false;            /* OPENCNC3D_GA_FAKE: the local stand-in */
static bool g_gaFakeDemo = false;        /* OPENCNC3D_GA_FAKE_FX=demo */
static int g_gaFakeHeld = GA_NONE;
static std::map<long long, unsigned> g_gaFakeFlags;
static std::vector<GunEffect> g_gaFakeFx;
static int32_t g_gaFakeTimers[GA_COUNT];
static int g_gaFakeTps = 15;
static unsigned g_gaDumps = 0;          /* object dumps seen, for the live world's cache */

static void ga_env_once()
{
    static bool done = false;
    if (done) return;
    done = true;
    const char* f = getenv("OPENCNC3D_GA_FAKE");
    g_gaFake = f && *f && strcmp(f, "0");
    const char* fx = getenv("OPENCNC3D_GA_FAKE_FX");
    g_gaFakeDemo = g_gaFake && fx && !strcmp(fx, "demo");
    if (g_gaFake) printf("GAHUD|fake|on|demo=%d\n", g_gaFakeDemo ? 1 : 0);
}

static long long ga_key(int kind, int id) { return ((long long)kind << 32) | (unsigned)id; }

static int ga_dll_type(ObjKind k)
{
    switch (k) {
        case K_INFANTRY: return 1;   /* DllObjectTypeEnum INFANTRY */
        case K_UNIT:     return 2;   /* UNIT */
        case K_AIRCRAFT: return 3;   /* AIRCRAFT */
        case K_BUILDING: return 4;   /* BUILDING */
        default:         return 0;
    }
}

/* The engine's HousesType from the house's INI name: GoodGuy 0, BadGuy 1, Neutral 2,
   Special 3, Multi1..Multi6 4..9 (defines.h). -1 for anything else. */
static int ga_house_number(const char* name)
{
    if (!name || !*name) return -1;
    if (!strcmp(name, "GoodGuy")) return 0;
    if (!strcmp(name, "BadGuy")) return 1;
    if (!strcmp(name, "Neutral")) return 2;
    if (!strcmp(name, "Special")) return 3;
    if (!strncmp(name, "Multi", 5) && name[5] >= '1' && name[5] <= '8' && !name[6])
        return 3 + (name[5] - '0');
    return -1;
}

static void ga_house_name(int house, char* out, size_t n)
{
    if (house >= 4 && house <= 11) snprintf(out, n, "Multi%d", house - 3);
    else if (house == 0) snprintf(out, n, "GoodGuy");
    else if (house == 1) snprintf(out, n, "BadGuy");
    else snprintf(out, n, "?");
}

static int ga_held()
{
    ga_env_once();
    if (g_gaFake) return g_gaFakeHeld;
    if (!BrainGaHeld || !g_sb.GunPlayers) return GA_NONE;
    const int a = BrainGaHeld(0);
    return a > GA_NONE && a < GA_COUNT ? a : GA_NONE;
}

static bool ga_feature_on()
{
    ga_env_once();
    return g_gaFake || (g_sb.GunPlayers && BrainGaHeld && BrainGaUse);
}

static bool ga_own(const SimObject& o)
{
    return g_playerHouse[0] && !strcmp(o.house, g_playerHouse);
}

static const SimObject* ga_own_unit()
{
    for (const SimObject& o : g_objects)
        if ((o.kind == K_INFANTRY || o.kind == K_UNIT || o.kind == K_AIRCRAFT)
            && !o.limbo && o.str > 0 && !(o.gaf & GAF_DECOY) && ga_own(o)) return &o;
    return nullptr;
}

/* Remaining ticks of each effect on the local player, read once per drawn frame. */
static int32_t g_gaTimers[GA_COUNT];
static int g_gaTps = 15;
static unsigned g_gaTimerSerial = ~0u;

static const int32_t* ga_timers()
{
    if (g_gaTimerSerial == g_drawFrameSerial) return g_gaTimers;
    g_gaTimerSerial = g_drawFrameSerial;
    memset(g_gaTimers, 0, sizeof g_gaTimers);
    g_gaTps = 15;
    if (g_gaFake) {
        memcpy(g_gaTimers, g_gaFakeTimers, sizeof g_gaTimers);
        g_gaTps = g_gaFakeTps;
    } else if (BrainGaTimers && g_sb.GunPlayers) {
        int32_t tps = 15;
        if (!BrainGaTimers(0, g_gaTimers, &tps)) memset(g_gaTimers, 0, sizeof g_gaTimers);
        g_gaTps = tps > 0 ? tps : 15;
    }
    return g_gaTimers;
}

static bool ga_mirrored()
{
    return (g_sb.GunPlayers || g_gaFake) && ga_timers()[GA_MIRROR] > 0;
}

static std::vector<GunEffect> g_gaFx;
static unsigned g_gaFxSerial = ~0u;

static void ga_demo_effects(std::vector<GunEffect>& out)
{
    const SimObject* me = ga_own_unit();
    if (!me) return;
    const int x = (int)(me->wx * 256.0f), y = (int)(me->wz * 256.0f);
    const int t = g_engineFrame;
    auto add = [&](int kind, int dx, int dy, int dx2, int dy2, int age, int value, int owner) {
        GunEffect e;
        e.kind = kind; e.owner = owner;
        e.x = x + dx * 256; e.y = y + dy * 256; e.x2 = x + dx2 * 256; e.y2 = y + dy2 * 256;
        e.age = age; e.value = value;
        out.push_back(e);
    };
    add(GFX_VORTEX, 4, -3, 0, 0, 0, 120, 4);
    add(GFX_WORMHOLE, -4, -2, -3, 4, 0, 200, 4);
    add(GFX_MINE, 2, 2, 0, 0, 0, 0, 4);
    add(GFX_MINE, 3, 1, 0, 0, 0, 0, 4);
    add(GFX_BOOBY, -2, 2, 0, 0, 0, 0, 4);
    for (int i = 0; i < 4; ++i) add(GFX_SMOKE, 5 + (i & 1), 2 + i / 2, 0, 0, 0, 100, 4);
    add(GFX_ARC, 1, 0, 5, -1, t % 8, 0, 4);
    add(GFX_POTATO, 0, 0, 0, 0, 0, 5 * 15 + 3, 5);
    add(GFX_REVEAL, 7, -4, 0, 0, 0, 1, 5);
    add(GFX_REVEAL, -6, 5, 0, 0, 0, 0, 6);
    add(GFX_ANNOUNCE, 0, 0, 0, 0, t % 45, GA_CHRONO_SWAP, 5);
    add(GFX_BLAST, -1, -4, 0, 0, t % 12, 0, 5);
}

/* SMOKE, JOINED AS THE ENGINE JOINS IT (Smoke_At). The engine reports one GFX_SMOKE per
   cell of each cloud, cloud after cloud; a cloud's cells share its owner and its remaining
   time, and two clouds that share a cell are one smoke. Each smoked cell maps to one label
   for the smoke it belongs to. */
static std::map<long long, int> g_gaSmoke;

static long long ga_cell_key(int cx, int cy) { return ((long long)cx << 32) | (unsigned)cy; }

static void ga_smoke_rebuild(const std::vector<GunEffect>& fx)
{
    g_gaSmoke.clear();
    std::vector<int> parent;
    const auto root = [&parent](int a) {
        while (parent[a] != a) a = parent[a] = parent[parent[a]];
        return a;
    };
    std::map<long long, int> first;      /* cell -> the first cloud holding it */
    std::set<long long> run;             /* the cells of the cloud being read */
    int cloud = -1, owner = 0, value = 0;
    for (const GunEffect& e : fx) {
        if (e.kind != GFX_SMOKE) { cloud = -1; continue; }
        const long long key = ga_cell_key(e.x >> 8, e.y >> 8);
        if (cloud < 0 || e.owner != owner || e.value != value || run.count(key)) {
            cloud = (int)parent.size();
            parent.push_back(cloud);
            owner = e.owner; value = e.value;
            run.clear();
        }
        run.insert(key);
        const auto had = first.find(key);
        if (had == first.end()) first[key] = cloud;
        else parent[root(cloud)] = root(had->second);
    }
    for (const auto& c : first) g_gaSmoke[c.first] = root(c.second);
}

static const std::vector<GunEffect>& ga_effects()
{
    if (g_gaFxSerial == g_drawFrameSerial) return g_gaFx;
    g_gaFxSerial = g_drawFrameSerial;
    g_gaFx.clear();
    if (g_gaFake) {
        for (const GunEffect& f : g_gaFakeFx) {
            GunEffect e = f;
            if (e.kind == GFX_ANNOUNCE && e.age < 0) e.age = g_engineFrame + e.age + 1;
            if (e.kind == GFX_ANNOUNCE && e.age > 45) continue;
            g_gaFx.push_back(e);
        }
        if (g_gaFakeDemo) ga_demo_effects(g_gaFx);
    } else if (BrainGaEffects && g_sb.GunPlayers) {
        GunEffect buf[256];
        const int n = BrainGaEffects(0, buf, 256);
        for (int i = 0; i < n && i < 256; ++i) g_gaFx.push_back(buf[i]);
    }
    ga_smoke_rebuild(g_gaFx);
    return g_gaFx;
}

/* The smoke a cell stands in, or -1. */
static int ga_smoke_at(int cx, int cy)
{
    const auto it = g_gaSmoke.find(ga_cell_key(cx, cy));
    return it == g_gaSmoke.end() ? -1 : it->second;
}

/* SMOKE HIDES WHAT STANDS IN IT FROM A VIEWER WHOSE COMBATANT IS OUTSIDE THAT SMOKE
   (Smoked_From). Two units in the same smoke see each other, and so may fight. A smoked
   cell this viewer's labels do not know (more effects than the engine reports at once)
   is taken as hidden. */
static bool ga_smoke_hides(int cx, int cy, bool smoked)
{
    const int s = ga_smoke_at(cx, cy);
    if (s < 0) return smoked;
    const SimObject* me = ga_own_unit();
    return !me || ga_smoke_at((int)floorf(me->ex), (int)floorf(me->ez)) != s;
}

static bool ga_unitlike(const SimObject& o)
{
    return (o.kind == K_INFANTRY || o.kind == K_UNIT || o.kind == K_AIRCRAFT) && !o.limbo && o.str > 0;
}

/* The flags of every drawn object, asked once per dump (refresh_objects) rather than
   once per question: visible() is asked many times a frame for every object.

   gahide is the one answer to "may this viewer know of it": another player's stealthed
   combatant, or a unit in smoke this viewer's combatant is not in. The engine's object
   dump carries them anyway, so everything that would give one away asks gahide.

   A DECOY MUST LOOK LIKE THE REAL ONE. The engine marks a player's carried bomb, dance and
   mirroring on the real combatant only, and builds decoys with one point of strength, so
   both are copied across every unit of that player another viewer can see. */
static void ga_refresh_flags()
{
    ga_env_once();
    ++g_gaDumps;
    const bool fake = g_gaFake;
    const bool real = !fake && g_sb.GunPlayers && BrainGaFlags;
    for (SimObject& o : g_objects) {
        o.gaf = 0;
        o.gahide = false;
        if (o.kind != K_INFANTRY && o.kind != K_UNIT && o.kind != K_AIRCRAFT) continue;
        if (fake) {
            const auto it = g_gaFakeFlags.find(ga_key(o.kind, o.id));
            if (it != g_gaFakeFlags.end()) o.gaf = it->second;
        } else if (real) {
            o.gaf = BrainGaFlags(0, ga_dll_type(o.kind), o.id);
        }
    }
    if (!fake && !real) return;
    g_gaFxSerial = ~0u;
    ga_effects();
    std::map<std::string, unsigned> houseBits;
    for (SimObject& o : g_objects) {
        if (!o.gaf || ga_own(o)) continue;
        o.gahide = (o.gaf & GAF_STEALTH)
                || ((o.gaf & GAF_SMOKED) && ga_smoke_hides((int)floorf(o.ex), (int)floorf(o.ez), true));
        if (!o.gahide && ga_unitlike(o))
            houseBits[o.house] |= o.gaf & (GAF_HOT_POTATO | GAF_DANCING | GAF_MIRRORED);
    }
    /* the players' houses only (Multi1 and on): two civilians of one type are two people */
    const auto player = [](const SimObject& o) { return ga_unitlike(o) && !ga_own(o) && ga_house_number(o.house) >= 4; };
    std::map<std::pair<std::string, std::string>, std::pair<int, int> > health;
    for (const SimObject& o : g_objects) {
        if (!player(o)) continue;
        std::pair<int, int>& h = health[std::make_pair(std::string(o.house), std::string(o.type))];
        if (o.str > h.first) h = std::make_pair(o.str, o.maxstr);
    }
    for (SimObject& o : g_objects) {
        if (!player(o)) continue;
        const auto bits = houseBits.find(o.house);
        if (bits != houseBits.end()) o.gaf |= bits->second;
        if (real) {
            const std::pair<int, int>& h = health[std::make_pair(std::string(o.house), std::string(o.type))];
            o.str = h.first;
            o.maxstr = h.second;
        }
    }
}

/* ---- events, points and the world adapter ------------------------------------------- */

enum { GA_EV_MOVE, GA_EV_DOWN, GA_EV_UP, GA_EV_WHEEL, GA_EV_KEY };
struct GaEv { int type; float x, y; int button, wheel, key; Uint32 t; };
struct GaPt { float x, y; Uint32 t; };

/* ex, ez: the engine's own position in cells, which its ranges are measured from (wx, wz
   may be the smoothed drawing position). level: the target's standing, 1 for the first
   rank, 0 when unknown. potato: its player already carries a Hot Potato. */
struct GaTarget { float sx, sy, wx, wz, ex, ez; int house, id, level; bool potato; char type[16]; };

struct GaWorld {
    int fbw, fbh;
    float mx, my;                       /* where the pointer is now */
    bool unit;
    float ux, uy, uwx, uwz;             /* the player's combatant, screen and world */
    float uex, uez;                     /* and its engine position, for the engine's ranges */
    int ucx, ucy;                       /* and its cell */
    char utype[16];
    std::vector<GaTarget> enemies;      /* enemy combatants on screen */
    std::function<bool(float, float, float*, float*)> to_world;    /* screen -> cells */
    std::function<bool(float, float, float*, float*)> to_screen;   /* cells -> screen */
    std::function<bool(int, int)> on_map;
    std::function<int(float, float)> pick_enemy;                     /* index or -1 */
};

static float ga_dist(float ax, float ay, float bx, float by) { return hypotf(ax - bx, ay - by); }

/* THE ENGINE'S DISTANCE, in cells: the larger axis gap plus half the smaller, on whole
   leptons (Distance_Coord). Every range the engine checks an activation against is this
   measure, not a straight line, so a game that promises "in range" measures it the same
   way: a straight line accepts cells the engine refuses about 27 degrees off an axis. */
static float ga_engine_dist(float ax, float az, float bx, float bz)
{
    const int dx = abs((int)lroundf(ax * 256.0f) - (int)lroundf(bx * 256.0f));
    const int dz = abs((int)lroundf(az * 256.0f) - (int)lroundf(bz * 256.0f));
    return (float)(dx > dz ? dx + dz / 2 : dz + dx / 2) / 256.0f;
}

/* The engine's limits, in cells (gun_game_ability.cpp: HopRange, SwapRange and MineRange,
   each plus half a cell). The swap keeps a quarter cell in hand, because the engine checks
   it again when the event runs and either unit may have moved by then. */
static const float GA_HOP_RANGE = 8.5f, GA_SWAP_RANGE = 10.25f, GA_MINE_RANGE = 4.5f;

static float ga_wrap(float a)
{
    while (a > (float)M_PI) a -= 2.0f * (float)M_PI;
    while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}

static float ga_length(const std::vector<GaPt>& p)
{
    float l = 0;
    for (size_t i = 1; i < p.size(); ++i) l += ga_dist(p[i - 1].x, p[i - 1].y, p[i].x, p[i].y);
    return l;
}

/* Points spaced `step` apart along the stroke, which takes the hand's speed (and the
   jitter of a slow hand) out of every angle the recognisers measure. */
static std::vector<GaPt> ga_resample(const std::vector<GaPt>& in, float step)
{
    std::vector<GaPt> out;
    if (in.empty()) return out;
    out.push_back(in[0]);
    float carry = 0;
    for (size_t i = 1; i < in.size(); ++i) {
        float x0 = in[i - 1].x, y0 = in[i - 1].y;
        const float x1 = in[i].x, y1 = in[i].y;
        float seg = ga_dist(x0, y0, x1, y1);
        while (carry + seg >= step && seg > 1e-6f) {
            const float k = (step - carry) / seg;
            x0 += (x1 - x0) * k; y0 += (y1 - y0) * k;
            out.push_back({x0, y0, in[i].t});
            seg = ga_dist(x0, y0, x1, y1);
            carry = 0;
        }
        carry += seg;
    }
    if (ga_dist(out.back().x, out.back().y, in.back().x, in.back().y) > step * 0.3f)
        out.push_back(in.back());
    return out;
}

static std::vector<GaPt> ga_resample_n(const std::vector<GaPt>& in, int n)
{
    const float len = ga_length(in);
    if (in.size() < 2 || len <= 1e-4f) return std::vector<GaPt>(n, in.empty() ? GaPt{0, 0, 0} : in[0]);
    std::vector<GaPt> out = ga_resample(in, len / (float)(n - 1));
    while ((int)out.size() < n) out.push_back(in.back());
    out.resize(n);
    return out;
}

/* Total signed turning of the stroke's direction, in radians. */
static float ga_turning(const std::vector<GaPt>& p)
{
    float total = 0;
    for (size_t i = 2; i < p.size(); ++i) {
        const float a0 = atan2f(p[i - 1].y - p[i - 2].y, p[i - 1].x - p[i - 2].x);
        const float a1 = atan2f(p[i].y - p[i - 1].y, p[i].x - p[i - 1].x);
        total += ga_wrap(a1 - a0);
    }
    return total;
}

/* INSIDE BY WINDING, NOT BY CROSSINGS: a loop drawn twice round a point winds it twice,
   and the even-odd rule would call that outside. Any winding but zero is inside. */
static bool ga_inside(const std::vector<GaPt>& poly, float x, float y)
{
    int wind = 0;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const GaPt& a = poly[j];
        const GaPt& b = poly[i];
        const float side = (b.x - a.x) * (y - a.y) - (x - a.x) * (b.y - a.y);
        if (a.y <= y) {
            if (b.y > y && side > 0) ++wind;
        } else if (b.y <= y && side < 0) {
            --wind;
        }
    }
    return wind != 0;
}

/* ONE CLOSED LOOP: long enough, ending near where it began, enclosing real area and
   winding once around its own centre. A straight line fails the area and the winding,
   an open arc fails the gap. */
static bool ga_is_loop(const std::vector<GaPt>& raw, float s, float* cx, float* cy)
{
    if (raw.size() < 6) return false;
    const std::vector<GaPt> p = ga_resample(raw, 3.0f * s);
    if (p.size() < 8) return false;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f, mx = 0, my = 0;
    for (const GaPt& q : p) {
        x0 = std::min(x0, q.x); x1 = std::max(x1, q.x);
        y0 = std::min(y0, q.y); y1 = std::max(y1, q.y);
        mx += q.x; my += q.y;
    }
    mx /= p.size(); my /= p.size();
    const float w = x1 - x0, h = y1 - y0, span = std::max(w, h);
    if (span < 24.0f * s) return false;
    if (ga_dist(p.front().x, p.front().y, p.back().x, p.back().y) > std::max(24.0f * s, 0.3f * span))
        return false;
    float area = 0, wind = 0;
    for (size_t i = 0; i < p.size(); ++i) {
        const GaPt& a = p[i];
        const GaPt& b = p[(i + 1) % p.size()];
        area += a.x * b.y - b.x * a.y;
        wind += ga_wrap(atan2f(b.y - my, b.x - mx) - atan2f(a.y - my, a.x - mx));
    }
    area = fabsf(area) * 0.5f;
    if (area < 0.25f * w * h) return false;
    if (fabsf(wind) < 1.6f * (float)M_PI) return false;
    *cx = mx; *cy = my;
    return true;
}

/* AN INWARD SPIRAL OF AT LEAST TWO TURNS: the direction turns through two full turns
   (less a sixth for a sloppy hand), and the outer part of the stroke is clearly further
   from the stroke's centre than the inner part, which ends near it. A circle traced
   twice turns enough but never comes inward; a line does not turn at all. */
static bool ga_is_spiral(const std::vector<GaPt>& raw, float s, float* ex, float* ey)
{
    const std::vector<GaPt> p = ga_resample(raw, 3.0f * s);
    if (p.size() < 16) return false;
    if (fabsf(ga_turning(p)) < 4.0f * (float)M_PI - (float)M_PI / 3.0f) return false;
    float cx = 0, cy = 0;
    for (const GaPt& q : p) { cx += q.x; cy += q.y; }
    cx /= p.size(); cy /= p.size();
    const size_t n = p.size(), quarter = std::max<size_t>(2, n / 4);
    float outer = 0, inner = 0;
    for (size_t i = 0; i < quarter; ++i) outer += ga_dist(p[i].x, p[i].y, cx, cy);
    for (size_t i = n - quarter; i < n; ++i) inner += ga_dist(p[i].x, p[i].y, cx, cy);
    outer /= quarter; inner /= quarter;
    if (outer < 16.0f * s || outer < 1.7f * inner) return false;
    if (ga_dist(p.back().x, p.back().y, cx, cy) > 0.5f * outer) return false;
    *ex = p.back().x; *ey = p.back().y;
    return true;
}

/* Centroid at the origin and a root-mean-square radius of one, so two strokes compare by
   shape alone, whatever their size and wherever they were drawn. */
static std::vector<GaPt> ga_normalize(std::vector<GaPt> p)
{
    float cx = 0, cy = 0;
    for (const GaPt& q : p) { cx += q.x; cy += q.y; }
    cx /= std::max<size_t>(1, p.size()); cy /= std::max<size_t>(1, p.size());
    float r = 0;
    for (GaPt& q : p) { q.x -= cx; q.y -= cy; r += q.x * q.x + q.y * q.y; }
    r = sqrtf(r / std::max<size_t>(1, p.size()));
    if (r < 1e-6f) r = 1;
    for (GaPt& q : p) { q.x /= r; q.y /= r; }
    return p;
}

static float ga_shape_dist(const std::vector<GaPt>& a, const std::vector<GaPt>& b)
{
    const int n = 32;
    const std::vector<GaPt> A = ga_normalize(ga_resample_n(a, n)), B = ga_normalize(ga_resample_n(b, n));
    float fwd = 0, rev = 0;
    for (int i = 0; i < n; ++i) {
        fwd += ga_dist(A[i].x, A[i].y, B[i].x, B[i].y);
        rev += ga_dist(A[i].x, A[i].y, B[n - 1 - i].x, B[n - 1 - i].y);
    }
    return std::min(fwd, rev) / n;
}

/* ---- one try ------------------------------------------------------------------------ */

enum { GA_RUNNING = 0, GA_DONE = 1, GA_FAILED = -1, GA_CANCELLED = -2 };

struct GaGame {
    int id, phase, result;
    Uint32 t0, now, limit;
    std::mt19937 rng;
    char why[48];
    char flash[40];
    Uint32 flash_t;
    bool flash_bad;
    float mx, my;
    bool ldown, rdown;
    std::vector<GaPt> stroke;
    bool drawing;
    /* what a success hands the engine */
    int house, tid;
    float twx, twz;
    int ncells;
    int16_t cells[2 * GA_MAX_CELLS];
    /* each game's own state: what every field means is said where the game uses it */
    float v, ax, ay, bx, by;
    int count, step, sel, carry, lastdir;
    Uint32 mark, mark2;
    float nx[6], ny[6];
    int perm[6], swaps[8][2], dir[4];
    bool flag;
    std::vector<GaPt> shape, recent;
    char clue[48];
    int colour[3];
};

static GaGame g_gaTmpl;   /* a default-built game, copied over a fresh one */

struct GaPanel { int s, x, y, w, h; float ax, ay, aw, ah; bool map; };

static bool ga_map_phase(const GaGame& g)
{
    switch (g.id) {
        case GA_STEALTH: case GA_CHRONO_HOP: case GA_CHRONO_SWAP: case GA_VORTEX:
        case GA_MINEFIELD: case GA_SMOKE: case GA_HOT_POTATO: case GA_WORMHOLE:
            return true;
        case GA_HUMILIATION: case GA_MIRROR:
            return g.phase == 0;
        default:
            return false;
    }
}

static int ga_scale(int fbw, int fbh) { return std::max(1, std::min(fbw / 360, fbh / 270)); }

/* THE PANEL. A game played ON the map draws a short strip at the bottom centre, so the
   ground it is played on stays in view; every other game is a centred panel in the
   launch code's own style. The play area (ax, ay, aw, ah) is in framebuffer pixels. */
static GaPanel ga_panel(const GaGame& g, int fbw, int fbh)
{
    GaPanel p;
    p.s = ga_scale(fbw, fbh);
    const int s = p.s;
    p.map = ga_map_phase(g);
    p.w = 300 * s;
    p.x = (fbw - p.w) / 2;
    if (p.map) {
        p.h = (g.id == GA_MINEFIELD ? 80 : 54) * s;
        p.y = fbh - p.h - 14 * s;
        p.ax = (float)(p.x + 10 * s); p.ay = (float)(p.y + 32 * s);
        p.aw = (float)(p.w - 20 * s); p.ah = (float)(p.h - 48 * s);
    } else {
        p.h = 168 * s;
        p.y = (fbh - p.h) / 2;
        p.ax = (float)(p.x + 10 * s); p.ay = (float)(p.y + 34 * s);
        p.aw = (float)(p.w - 20 * s); p.ah = (float)(100 * s);
    }
    return p;
}

static void ga_at(const GaPanel& p, float u, float v, float* x, float* y)
{
    *x = p.ax + u * p.aw;
    *y = p.ay + v * p.ah;
}

static void ga_uv(const GaPanel& p, float x, float y, float* u, float* v)
{
    *u = (x - p.ax) / p.aw;
    *v = (y - p.ay) / p.ah;
}

static void ga_flash(GaGame& g, const char* text, bool bad)
{
    snprintf(g.flash, sizeof g.flash, "%s", text);
    g.flash_t = g.now;
    g.flash_bad = bad;
}

static void ga_end(GaGame& g, int result, const char* why)
{
    if (g.result != GA_RUNNING) return;
    g.result = result;
    snprintf(g.why, sizeof g.why, "%s", why ? why : "");
}

static float ga_frand(GaGame& g, float lo, float hi)
{
    return lo + (hi - lo) * (float)(g.rng() % 100000u) / 100000.0f;
}

/* Wires: three of these four, each shown in its own ink. */
static const char* const GA_WIRE_NAME[4] = {"RED", "BLUE", "GREEN", "YELLOW"};
static const float GA_WIRE_RGB[4][3] = {{0.90f, 0.20f, 0.16f}, {0.25f, 0.45f, 0.95f},
                                        {0.25f, 0.80f, 0.30f}, {0.95f, 0.85f, 0.20f}};
static const char* const GA_SIDE_NAME[3] = {"LEFT", "MIDDLE", "RIGHT"};
static const float GA_WIRE_U[3] = {0.25f, 0.5f, 0.75f};

/* Shell game: the timeline. */
static const Uint32 GA_SHELL_SHOW = 800, GA_SHELL_SWAP = 300, GA_SHELL_SWAPS = 5;
static Uint32 ga_shell_ready() { return GA_SHELL_SHOW + 200 + GA_SHELL_SWAP * GA_SHELL_SWAPS; }

/* Heartbeat: when the beeps fall, evenly, and how early and how late a click still counts.
   The late side is the wider one: a hand that answers the beep it hears is a reaction time
   and the sound's own delay behind the beat, and the trace shows every spike coming. */
static const Uint32 GA_BEAT[3] = {900, 1700, 2500};
static const Uint32 GA_BEAT_EARLY = 200, GA_BEAT_LATE = 380;

/* Dance off: when each arrow shows and for how long it waits. */
static Uint32 ga_arrow_at(int k) { return 500 + 700u * (Uint32)k; }
static const Uint32 GA_ARROW_WINDOW = 650;
static const float GA_DIR_X[4] = {0, 0, -1, 1}, GA_DIR_Y[4] = {-1, 1, 0, 0};   /* up down left right */

/* Gift wrap: where the three pieces wait and where each must go, as area fractions. */
static const float GA_GIFT_ITEM_U = 0.14f, GA_GIFT_ITEM_V[3] = {0.18f, 0.50f, 0.82f};
static const float GA_GIFT_ZONE[3][4] = {{0.56f, 0.52f, 0.86f, 0.96f},    /* bomb: in the box */
                                         {0.52f, 0.32f, 0.90f, 0.52f},    /* lid: on top of it */
                                         {0.60f, 0.04f, 0.82f, 0.32f}};   /* bow: on the lid */
static const char* const GA_GIFT_NAME[3] = {"BOMB", "LID", "BOW"};

static void ga_card_slot(const GaPanel& p, int slot, float* x, float* y)
{
    ga_at(p, 0.2f + 0.3f * slot, 0.5f, x, y);
}

/* Where card c is drawn at time t: its slot, or on its way between two during a swap. */
static void ga_card_pos(const GaGame& g, const GaPanel& p, int c, Uint32 t, float* x, float* y)
{
    int slot[3] = {0, 1, 2};
    const Uint32 start = GA_SHELL_SHOW + 200;
    for (Uint32 k = 0; k < GA_SHELL_SWAPS; ++k) {
        const Uint32 a = start + k * GA_SHELL_SWAP;
        const int i = g.swaps[k][0], j = g.swaps[k][1];
        if (t >= a + GA_SHELL_SWAP) { std::swap(slot[i], slot[j]); continue; }
        if (t >= a && (c == i || c == j)) {
            const float f = (float)(t - a) / GA_SHELL_SWAP;
            float x0, y0, x1, y1;
            ga_card_slot(p, slot[c], &x0, &y0);
            ga_card_slot(p, slot[c == i ? j : i], &x1, &y1);
            const float e = f * f * (3 - 2 * f);
            *x = x0 + (x1 - x0) * e;
            *y = y0 + (c == i ? -1.0f : 1.0f) * sinf(e * (float)M_PI) * p.ah * 0.22f;
            return;
        }
        break;
    }
    ga_card_slot(p, slot[c], x, y);
}

static float ga_golf_needle_at(const GaGame& g, Uint32 t)
{
    const float f = fmodf((float)(std::max(t, g.mark) - g.mark) / 1400.0f, 1.0f);
    return f < 0.5f ? f * 2.0f : 2.0f - f * 2.0f;
}

static float ga_golf_needle(const GaGame& g) { return ga_golf_needle_at(g, g.now); }

/* THE CLICK IS JUDGED AGAINST THE NEEDLE THE PLAYER SAW. A drawn frame reaches the screen a
   frame or two after it was drawn, and the click is read at the next poll after that, so
   the needle is read GA_GOLF_LAG earlier than the click arrived, and the notch counts a
   little wider than it is drawn. */
static const Uint32 GA_GOLF_LAG = 30;
static const float GA_GOLF_TOLERANCE = 0.07f;

/* The squiggle for the mirror game, in the LEFT half's own fractions (0..1 each way). It
   is chosen so that drawing it unmirrored, or drawing a straight line, is clearly not a
   match, which is what keeps the recogniser honest. */
static void ga_mirror_shape(GaGame& g)
{
    for (int attempt = 0; attempt < 400; ++attempt) {
        float cx[6], cy[6];
        for (int i = 0; i < 6; ++i) {
            cx[i] = 0.15f + 0.7f * i / 5.0f + ga_frand(g, -0.05f, 0.05f);
            cy[i] = ga_frand(g, 0.15f, 0.85f);
        }
        std::vector<GaPt> s;
        for (int i = 0; i < 5; ++i)
            for (int k = 0; k < 8; ++k) {
                const float t = k / 8.0f;
                const int a = std::max(0, i - 1), b = i, c = i + 1, d = std::min(5, i + 2);
                const float t2 = t * t, t3 = t2 * t;
                const auto cr = [&](const float* q) {
                    return 0.5f * (2 * q[b] + (-q[a] + q[c]) * t + (2 * q[a] - 5 * q[b] + 4 * q[c] - q[d]) * t2
                                   + (-q[a] + 3 * q[b] - 3 * q[c] + q[d]) * t3);
                };
                s.push_back({cr(cx), cr(cy), 0});
            }
        s.push_back({cx[5], cy[5], 0});
        std::vector<GaPt> mirrored = s, line = {s.front(), s.back()};
        for (GaPt& q : mirrored) q.x = 1.0f - q.x;
        if (ga_shape_dist(s, mirrored) >= 0.75f && ga_shape_dist(s, line) >= 0.7f) { g.shape = s; return; }
    }
    /* never reached in practice; a lopsided hook that is plainly not its own mirror */
    const std::vector<GaPt> hook = {{0.15f, 0.8f, 0}, {0.3f, 0.2f, 0}, {0.6f, 0.15f, 0}, {0.85f, 0.4f, 0}, {0.7f, 0.85f, 0}};
    g.shape = ga_resample_n(hook, 40);
}

static void ga_begin(GaGame& g, int id, const GaWorld& w, Uint32 t, unsigned seed)
{
    g = g_gaTmpl;
    g.id = id; g.phase = 0; g.result = GA_RUNNING;
    g.t0 = g.now = t; g.limit = GA_LIMIT[id];
    g.rng.seed(seed);
    g.why[0] = g.flash[0] = g.clue[0] = 0;
    g.flash_t = 0; g.flash_bad = false;
    g.mx = w.mx; g.my = w.my;
    g.ldown = g.rdown = g.drawing = false;
    g.house = -1; g.tid = -1; g.twx = g.twz = 0; g.ncells = 0;
    g.v = g.ax = g.ay = g.bx = g.by = 0;
    g.count = g.step = g.sel = 0; g.carry = -1; g.lastdir = 0;
    g.mark = g.mark2 = t;
    g.flag = false;
    const GaPanel p = ga_panel(g, w.fbw, w.fbh);
    switch (id) {
        case GA_STEALTH:         /* ax, ay: where the hand is held; mark: since when */
            g.ax = w.mx; g.ay = w.my;
            break;
        case GA_HUNTERS_EYE:     /* v: the dial; nx[0]: the hidden station */
            g.v = 50;
            do g.nx[0] = (float)(10 + g.rng() % 81); while (fabsf(g.nx[0] - 50) < 20);
            break;
        case GA_HUMILIATION:     /* nx[0]: the notch on the meter */
            g.nx[0] = ga_frand(g, 0.55f, 0.85f);
            break;
        case GA_CHAIN_SHOCK:     /* nx, ny: the three nodes, as area fractions */
            for (int attempt = 0; attempt < 200; ++attempt) {
                for (int i = 0; i < 3; ++i) { g.nx[i] = ga_frand(g, 0.08f, 0.92f); g.ny[i] = ga_frand(g, 0.12f, 0.88f); }
                bool ok = true;
                for (int i = 0; i < 3 && ok; ++i)
                    for (int j = i + 1; j < 3; ++j)
                        if (hypotf((g.nx[i] - g.nx[j]) * p.aw, (g.ny[i] - g.ny[j]) * p.ah) < 70.0f * p.s) ok = false;
                /* the straight way from 1 to 2 must not touch 3 */
                const float ux = (g.nx[1] - g.nx[0]) * p.aw, uy = (g.ny[1] - g.ny[0]) * p.ah;
                const float vx = (g.nx[2] - g.nx[0]) * p.aw, vy = (g.ny[2] - g.ny[0]) * p.ah;
                const float len = hypotf(ux, uy);
                const float along = (ux * vx + uy * vy) / std::max(1e-3f, len);
                const float off = fabsf(ux * vy - uy * vx) / std::max(1e-3f, len);
                if (along > -20.0f * p.s && along < len + 20.0f * p.s && off < 36.0f * p.s) ok = false;
                if (ok) break;
            }
            break;
        case GA_DECOYS:          /* sel: the card hiding you; swaps: the shuffle */
            g.sel = (int)(g.rng() % 3);
            for (Uint32 k = 0; k < GA_SHELL_SWAPS; ++k) {
                g.swaps[k][0] = (int)(g.rng() % 3);
                g.swaps[k][1] = (g.swaps[k][0] + 1 + (int)(g.rng() % 2)) % 3;
            }
            break;
        case GA_LAST_LAUGH: {    /* colour: each wire's ink; sel: the right one */
            int inks[4] = {0, 1, 2, 3};
            std::shuffle(inks, inks + 4, g.rng);
            for (int i = 0; i < 3; ++i) g.colour[i] = inks[i];
            g.sel = (int)(g.rng() % 3);
            int wrong[2], n = 0;
            for (int i = 0; i < 3; ++i) if (i != g.sel) wrong[n++] = i;
            if (g.rng() & 1) std::swap(wrong[0], wrong[1]);
            snprintf(g.clue, sizeof g.clue, "NOT %s, NOT THE %s ONE",
                     GA_WIRE_NAME[g.colour[wrong[0]]], GA_SIDE_NAME[wrong[1]]);
            break;
        }
        case GA_DANCE_OFF:       /* dir: each arrow; step: arrows done; sel: the arrow the
                                    hand is ready for (-1 none); ax, ay: where it rested;
                                    mark2: when it started moving from there (0 not yet) */
            for (int k = 0; k < 4; ++k) {
                do g.dir[k] = (int)(g.rng() % 4); while (k && g.dir[k] == g.dir[k - 1]);
            }
            g.sel = -1;
            g.mark2 = 0;
            break;
        case GA_MIRROR:
            ga_mirror_shape(g);
            break;
        default:
            break;
    }
}

static void ga_set_cell(GaGame& g, int i, int cx, int cy)
{
    g.cells[2 * i] = (int16_t)cx;
    g.cells[2 * i + 1] = (int16_t)cy;
}

static bool ga_cell_at(const GaWorld& w, float x, float y, int* cx, int* cy)
{
    float wx, wz;
    if (!w.to_world(x, y, &wx, &wz)) return false;
    *cx = (int)floorf(wx);
    *cy = (int)floorf(wz);
    return w.on_map(*cx, *cy);
}

/* WHERE A SLINGSHOT PULL LANDS: the cursor's offset from the unit, half again, the other
   way, no further than 8 cells; then the cell it ends in, stepped back along the pull
   until the engine's own measure from the unit to that cell's centre accepts it. The
   preview ring and the activation both come from here, so what is drawn is what is sent.
   Returns the pull's length in cells before the cap. */
static float ga_hop_landing(const GaWorld& w, float wx, float wz, int* cx, int* cy)
{
    float dx = (w.uwx - wx) * 1.5f, dz = (w.uwz - wz) * 1.5f;
    const float len = hypotf(dx, dz);
    if (len > 8.0f) { dx *= 8.0f / len; dz *= 8.0f / len; }
    const float l = std::max(1e-3f, hypotf(dx, dz)), ux = dx / l, uz = dz / l;
    for (int k = 0; k < 40; ++k) {
        *cx = (int)floorf(w.uex + dx);
        *cy = (int)floorf(w.uez + dz);
        if (ga_engine_dist(w.uex, w.uez, *cx + 0.5f, *cy + 0.5f) <= GA_HOP_RANGE) break;
        dx -= ux * 0.25f; dz -= uz * 0.25f;
    }
    return len;
}

static bool ga_hop_too_short(const GaWorld& w, float len, int cx, int cy)
{
    return len < 1.0f || (cx == (int)floorf(w.uex) && cy == (int)floorf(w.uez));
}

/* A mine is in reach when the engine's measure from the combatant to the cell's centre is
   inside MineRange and a half. */
static bool ga_mine_in_reach(const GaWorld& w, int cx, int cy)
{
    return ga_engine_dist(w.uex, w.uez, cx + 0.5f, cy + 0.5f) <= GA_MINE_RANGE;
}

static void ga_take_target(GaGame& g, const GaTarget& t)
{
    g.house = t.house;
    g.tid = t.id;
    g.twx = t.wx;
    g.twz = t.wz;
}

/* The time-driven half of every game: the clock, and what moves on its own. */
static void ga_tick(GaGame& g, const GaWorld& w, Uint32 t)
{
    (void)w;
    if (g.result != GA_RUNNING) return;
    const Uint32 prev = g.now;
    if (t > g.now) g.now = t;
    const float dt = (float)(g.now - prev) / 1000.0f;
    const Uint32 e = g.now - g.t0;
    switch (g.id) {
        case GA_STEALTH:
            if (g.now - g.mark >= 1500) { ga_end(g, GA_DONE, "held still"); return; }
            break;
        case GA_IRON_CURTAIN:
            g.v = std::max(0.0f, g.v - 0.12f * dt);
            break;
        case GA_OVERDRIVE:
            g.v = std::max(0.0f, g.v - 0.35f * dt);
            break;
        case GA_EXTRA_LIFE:
            if (g.step < 3 && e > GA_BEAT[g.step] + GA_BEAT_LATE) { ga_end(g, GA_FAILED, "MISSED A BEAT"); return; }
            break;
        case GA_DANCE_OFF:
            if (g.step < 4 && e > ga_arrow_at(g.step) + GA_ARROW_WINDOW) { ga_end(g, GA_FAILED, "TOO SLOW"); return; }
            /* A FLICK STARTS FROM REST, AFTER ITS ARROW. Until the hand has been still for
               80 ms with the arrow up, nothing it does is judged: not the move from the
               cameo to the panel, and not the way back from the last flick. */
            if (g.step < 4 && e >= ga_arrow_at(g.step) && g.sel != g.step) {
                const float s = (float)ga_scale(w.fbw, w.fbh);
                bool rest = true;
                for (const GaPt& q : g.recent)
                    if (g.now - q.t < 80 && ga_dist(q.x, q.y, g.mx, g.my) > 3.0f * s) rest = false;
                if (rest) { g.sel = g.step; g.ax = g.mx; g.ay = g.my; g.mark2 = 0; }
            }
            break;
        default:
            break;
    }
    if (e >= g.limit) ga_end(g, GA_FAILED, "OUT OF TIME");
}

/* What one event does to a game. */
static void ga_event(GaGame& g, const GaWorld& w, const GaEv& ev)
{
    if (g.result != GA_RUNNING) return;
    ga_tick(g, w, ev.t);
    if (g.result != GA_RUNNING) return;
    if (ev.type == GA_EV_KEY && ev.key == SDLK_ESCAPE) { ga_end(g, GA_CANCELLED, "CANCELLED"); return; }
    const GaPanel p = ga_panel(g, w.fbw, w.fbh);
    const float s = (float)p.s;
    const Uint32 e = g.now - g.t0;
    const float px = g.mx, py = g.my;
    if (ev.type == GA_EV_MOVE || ev.type == GA_EV_DOWN || ev.type == GA_EV_UP) { g.mx = ev.x; g.my = ev.y; }
    const bool ldown = ev.type == GA_EV_DOWN && ev.button == 1;
    const bool lup = ev.type == GA_EV_UP && ev.button == 1;
    const bool rdown = ev.type == GA_EV_DOWN && ev.button == 3;
    const bool rup = ev.type == GA_EV_UP && ev.button == 3;
    if (ldown) g.ldown = true;
    if (lup) g.ldown = false;
    if (rdown) g.rdown = true;
    if (rup) g.rdown = false;
    const bool move = ev.type == GA_EV_MOVE;

    switch (g.id) {
    case GA_STEALTH:
        if ((move || ldown || lup) && ga_dist(g.mx, g.my, g.ax, g.ay) > 3.0f * s) {
            g.ax = g.mx; g.ay = g.my; g.mark = g.now;
            ga_flash(g, "TWITCHED, AGAIN", true);
        }
        break;

    case GA_CHRONO_HOP:
        /* phase 1 while the unit is held; the pull is the cursor's offset from the unit,
           the landing is that much and half again the other way, no further than 8 */
        if (ldown && w.unit && ga_dist(ev.x, ev.y, w.ux, w.uy) <= std::max(26.0f * s, 34.0f)) g.phase = 1;
        else if (ldown) ga_flash(g, "GRAB YOUR OWN UNIT", true);
        if (lup && g.phase == 1) {
            g.phase = 0;
            float wx, wz;
            if (!w.to_world(ev.x, ev.y, &wx, &wz)) { ga_flash(g, "OFF THE MAP", true); break; }
            int cx, cy;
            const float len = ga_hop_landing(w, wx, wz, &cx, &cy);
            if (ga_hop_too_short(w, len, cx, cy)) { ga_flash(g, "PULL BACK FURTHER", true); break; }
            if (!w.on_map(cx, cy)) { ga_flash(g, "OFF THE MAP", true); break; }
            ga_set_cell(g, 0, cx, cy);
            g.ncells = 1;
            ga_end(g, GA_DONE, "hop");
        }
        break;

    case GA_CHRONO_SWAP:
    case GA_VORTEX:
    case GA_MIRROR:
        if (g.id == GA_MIRROR && g.phase == 0) {
            if (ldown) {
                const int k = w.pick_enemy(ev.x, ev.y);
                if (k < 0) { ga_flash(g, "CLICK AN ENEMY UNIT", true); break; }
                ga_take_target(g, w.enemies[k]);
                g.phase = 1;
                g.ldown = false;
            }
            break;
        }
        if (ldown) {
            if (g.id == GA_MIRROR && (ev.x < p.ax + p.aw * 0.5f || ev.x > p.ax + p.aw || ev.y < p.ay || ev.y > p.ay + p.ah)) {
                ga_flash(g, "DRAW ON THE RIGHT HALF", true);
                break;
            }
            g.stroke.clear();
            g.stroke.push_back({ev.x, ev.y, g.now});
            g.drawing = true;
        } else if (move && g.drawing) {
            if (ga_dist(ev.x, ev.y, g.stroke.back().x, g.stroke.back().y) >= 1.5f) g.stroke.push_back({ev.x, ev.y, g.now});
        } else if (lup && g.drawing) {
            g.drawing = false;
            g.stroke.push_back({ev.x, ev.y, g.now});
            if (g.id == GA_CHRONO_SWAP) {
                float cx, cy;
                if (!ga_is_loop(g.stroke, s, &cx, &cy)) { ga_flash(g, "NOT A CLOSED LOOP", true); break; }
                int best = -1;
                float bestd = 1e9f;
                bool too_far = false;
                for (size_t i = 0; i < w.enemies.size(); ++i) {
                    const GaTarget& t = w.enemies[i];
                    if (!ga_inside(g.stroke, t.sx, t.sy)) continue;
                    if (ga_engine_dist(w.uex, w.uez, t.ex, t.ez) > GA_SWAP_RANGE) { too_far = true; continue; }
                    const float d = ga_dist(t.sx, t.sy, cx, cy);
                    if (d < bestd) { bestd = d; best = (int)i; }
                }
                if (best < 0) { ga_flash(g, too_far ? "TOO FAR, 10 CELLS AT MOST" : "NO ENEMY IN THE LOOP", true); break; }
                ga_take_target(g, w.enemies[best]);
                ga_end(g, GA_DONE, "lasso");
            } else if (g.id == GA_VORTEX) {
                float ex, ey;
                int cx, cy;
                if (!ga_is_spiral(g.stroke, s, &ex, &ey)) { ga_flash(g, "NOT AN INWARD SPIRAL", true); break; }
                if (!ga_cell_at(w, ex, ey, &cx, &cy)) { ga_flash(g, "OFF THE MAP", true); break; }
                ga_set_cell(g, 0, cx, cy);
                g.ncells = 1;
                ga_end(g, GA_DONE, "spiral");
            } else {
                /* the target, mirrored about the divider, against the stroke, both in the
                   right half's own fractions */
                std::vector<GaPt> drawn, target;
                const float hx = p.ax + p.aw * 0.5f, hw = p.aw * 0.5f;
                for (const GaPt& q : g.stroke) drawn.push_back({(q.x - hx) / hw, (q.y - p.ay) / p.ah, 0});
                for (const GaPt& q : g.shape) target.push_back({1.0f - q.x, q.y, 0});
                const float ratio = ga_length(drawn) / std::max(1e-3f, ga_length(target));
                const float d = ga_shape_dist(drawn, target);
                if (drawn.size() < 6 || ratio < 0.5f || ratio > 2.2f || d > 0.45f) {
                    ga_flash(g, "NOT ITS MIRROR, AGAIN", true);
                    break;
                }
                ga_end(g, GA_DONE, "mirror");
            }
        }
        break;

    case GA_IRON_CURTAIN:
        if (ev.type == GA_EV_KEY && ev.key == SDLK_SPACE) {
            g.v += 0.075f;
            if (g.v >= 1.0f) { g.v = 1.0f; ga_end(g, GA_DONE, "cranked"); }
        }
        break;

    case GA_OVERDRIVE:
        /* v: the rev needle. It rises only while the hand is SHAKING: a reversal of
           direction after real travel opens a 350 ms window in which horizontal travel
           revs the engine. lastdir and ax: the current direction and its travel; flag: a
           real reversal has happened. */
        if (move) {
            const float dx = (ev.x - px) / s;
            const int dir = dx > 0 ? 1 : dx < 0 ? -1 : 0;
            if (dir && dir != g.lastdir) {
                if (g.lastdir && g.ax >= 6.0f) { g.mark = g.now; g.flag = true; }
                g.lastdir = dir;
                g.ax = 0;
            }
            g.ax += fabsf(dx);
            if (g.flag && g.now - g.mark < 350) g.v += fabsf(dx) / 450.0f;
            if (g.v > 1.0f) { g.v = 0; ++g.count; ga_flash(g, "STALLED, REV AGAIN", true); }
        }
        /* the click commits, as in every timing game here: a click out of the red ends
           the try, so clicking at random while shaking does not win */
        if (ldown) {
            if (g.v >= 0.78f && g.v <= 0.96f) ga_end(g, GA_DONE, "revved");
            else ga_end(g, GA_FAILED, "NOT IN THE RED");
        }
        break;

    case GA_MINEFIELD: {
        /* carry: the token in the hand (always the next one: they are all alike); count:
           tokens planted */
        const auto token = [&](int i, float* x, float* y) {
            *x = (float)(p.x + p.w / 2 + (i - 2) * 30 * p.s);
            *y = (float)(p.y + 46 * p.s);
        };
        if (ldown) {
            for (int i = g.count; i < 5; ++i) {
                float x, y;
                token(i, &x, &y);
                if (ga_dist(ev.x, ev.y, x, y) <= 12.0f * s) { g.carry = g.count; break; }
            }
            if (g.carry < 0) ga_flash(g, "DRAG A MINE FROM THE TRAY", true);
        }
        if (lup && g.carry >= 0) {
            g.carry = -1;
            int cx, cy;
            if (!ga_cell_at(w, ev.x, ev.y, &cx, &cy)) { ga_flash(g, "NOT ON THE MAP", true); break; }
            if (!ga_mine_in_reach(w, cx, cy)) { ga_flash(g, "TOO FAR, 4 CELLS AT MOST", true); break; }
            bool dup = false;
            for (int i = 0; i < g.ncells; ++i) if (g.cells[2 * i] == cx && g.cells[2 * i + 1] == cy) dup = true;
            if (dup) { ga_flash(g, "ALREADY MINED", true); break; }
            ga_set_cell(g, g.ncells++, cx, cy);
            ++g.count;
            /* THE ENGINE MEASURES EVERY MINE FROM WHERE THE UNIT IS WHEN THE TRY ENDS, and
               refuses all five for one out of reach. The unit may have moved while they
               were planted, so the ones it has left behind go back to the tray. */
            if (g.count == 5) {
                int kept = 0;
                for (int i = 0; i < g.ncells; ++i)
                    if (ga_mine_in_reach(w, g.cells[2 * i], g.cells[2 * i + 1]))
                        ga_set_cell(g, kept++, g.cells[2 * i], g.cells[2 * i + 1]);
                if (kept < g.ncells) {
                    g.ncells = g.count = kept;
                    ga_flash(g, "YOU MOVED, PLANT AGAIN", true);
                } else {
                    ga_end(g, GA_DONE, "planted");
                }
            }
        }
        break;
    }

    case GA_HUNTERS_EYE:
        if (ev.type == GA_EV_WHEEL) g.v = std::max(0.0f, std::min(100.0f, g.v + 2.0f * (float)ev.wheel));
        /* a click on static ends the try: the panel says CLEAN exactly where a click
           wins, so a sweep of the wheel with the button mashed does not */
        if (ldown) {
            if (fabsf(g.v - g.nx[0]) <= 2.5f) ga_end(g, GA_DONE, "tuned");
            else ga_end(g, GA_FAILED, "STILL STATIC");
        }
        break;

    case GA_HUMILIATION:
        if (g.phase == 0) {
            if (ldown) {
                const int k = w.pick_enemy(ev.x, ev.y);
                if (k < 0) { ga_flash(g, "CLICK AN ENEMY UNIT", true); break; }
                /* the engine never drops anyone below the first rank */
                if (w.enemies[k].level == 1) { ga_flash(g, "ALREADY AT THE BOTTOM", true); break; }
                ga_take_target(g, w.enemies[k]);
                g.phase = 1;
                g.mark = g.now;
            }
        } else if (ldown) {
            const Uint32 seen = g.now > g.mark + GA_GOLF_LAG ? g.now - GA_GOLF_LAG : g.mark;
            if (fabsf(ga_golf_needle_at(g, seen) - g.nx[0]) <= GA_GOLF_TOLERANCE) ga_end(g, GA_DONE, "on the notch");
            else ga_end(g, GA_FAILED, "MISSED THE NOTCH");
        }
        break;

    case GA_CHAIN_SHOCK: {
        /* step: nodes passed while the stroke is down */
        const float r = 13.0f * s;
        const auto on_node = [&](int i) {
            float x, y;
            ga_at(p, g.nx[i], g.ny[i], &x, &y);
            return ga_dist(ev.x, ev.y, x, y) <= r;
        };
        if (ldown) {
            if (on_node(0)) { g.drawing = true; g.step = 1; g.stroke.assign(1, {ev.x, ev.y, g.now}); }
            else ga_flash(g, "START ON 1", true);
        } else if (move && g.drawing) {
            g.stroke.push_back({ev.x, ev.y, g.now});
            if (on_node(g.step)) {
                if (++g.step == 3) ga_end(g, GA_DONE, "connected");
            } else {
                for (int i = g.step + 1; i < 3; ++i)
                    if (on_node(i)) { g.drawing = false; g.step = 0; g.stroke.clear(); ga_flash(g, "IN ORDER: 1, 2, 3", true); break; }
            }
        } else if (lup && g.drawing) {
            g.drawing = false; g.step = 0; g.stroke.clear();
            ga_flash(g, "ONE STROKE, DON'T LET GO", true);
        }
        break;
    }

    case GA_EXTRA_LIFE:
        if (ldown) {
            if (g.step < 3 && (e + GA_BEAT_EARLY >= GA_BEAT[g.step]) && e <= GA_BEAT[g.step] + GA_BEAT_LATE) {
                ga_flash(g, "BEAT", false);
                if (++g.step == 3) ga_end(g, GA_DONE, "in time");
            } else {
                ga_end(g, GA_FAILED, "OFF THE BEAT");
            }
        }
        break;

    case GA_SMOKE:
        if (ldown || (move && g.ldown)) {
            int cx, cy;
            if (ga_cell_at(w, ev.x, ev.y, &cx, &cy)) {
                bool have = false;
                for (int i = 0; i < g.ncells; ++i) if (g.cells[2 * i] == cx && g.cells[2 * i + 1] == cy) have = true;
                if (!have && g.ncells < GA_MAX_CELLS) ga_set_cell(g, g.ncells++, cx, cy);
            }
            g.stroke.push_back({ev.x, ev.y, g.now});
            if (g.ncells >= GA_MAX_CELLS) ga_end(g, GA_DONE, "out of ink");
        } else if (lup) {
            if (g.ncells >= 6) ga_end(g, GA_DONE, "painted");
            else ga_flash(g, "PAINT MORE SMOKE", true);
        }
        break;

    case GA_DECOYS:
        if (ldown) {
            if (e < ga_shell_ready()) { ga_flash(g, "WAIT FOR THE SHUFFLE", true); break; }
            for (int c = 0; c < 3; ++c) {
                float x, y;
                ga_card_pos(g, p, c, e, &x, &y);
                if (fabsf(ev.x - x) <= 34.0f * s && fabsf(ev.y - y) <= 26.0f * s) {
                    if (c == g.sel) ga_end(g, GA_DONE, "found you");
                    else ga_end(g, GA_FAILED, "WRONG CARD");
                    break;
                }
            }
        }
        break;

    case GA_GIANT:
        /* count: firm upward strokes; ay: the low point of the stroke under way; flag: a
           stroke has been counted and the hand has not come back down yet */
        if (ldown) { g.count = 0; g.ay = ev.y; g.ax = ev.x; g.flag = false; }
        else if (lup) {
            if (g.count < 5) { g.count = 0; ga_flash(g, "KEEP THE BUTTON HELD", true); }
        } else if (move && g.ldown) {
            if (g.flag) {
                if (ev.y - g.by >= 15.0f * s) { g.flag = false; g.ay = ev.y; g.ax = ev.x; }
                else g.by = std::min(g.by, ev.y);
            } else {
                if (ev.y > g.ay) { g.ay = ev.y; g.ax = ev.x; }
                const float up = g.ay - ev.y, side = fabsf(ev.x - g.ax);
                if (up >= 40.0f * s && side <= 0.6f * up) {
                    g.flag = true; g.by = ev.y;
                    if (++g.count == 5) ga_end(g, GA_DONE, "pumped");
                }
            }
        }
        break;

    case GA_LAST_LAUGH:
        if (move && g.ldown) {
            for (int i = 0; i < 3; ++i) {
                float x, y0, y1, dummy;
                ga_at(p, GA_WIRE_U[i], 0.05f, &x, &y0);
                ga_at(p, GA_WIRE_U[i], 0.95f, &dummy, &y1);
                if (((px < x) != (ev.x < x)) && ev.y >= y0 && ev.y <= y1 && py >= y0 && py <= y1) {
                    g.step = i;
                    if (i == g.sel) ga_end(g, GA_DONE, "right wire");
                    else ga_end(g, GA_FAILED, "WRONG WIRE");
                    break;
                }
            }
        }
        break;

    case GA_HOT_POTATO: {
        /* flag: the bomb is in the hand; recent: the last moves, for the flick's speed */
        const float bx = w.ux, by = w.uy - 34.0f * s;
        if (ldown) {
            if (w.unit && ga_dist(ev.x, ev.y, bx, by) <= 16.0f * s) { g.flag = true; g.recent.assign(1, {ev.x, ev.y, g.now}); }
            else ga_flash(g, "GRAB THE BOMB", true);
        } else if (move && g.flag) {
            g.recent.push_back({ev.x, ev.y, g.now});
        } else if (lup && g.flag) {
            g.flag = false;
            g.recent.push_back({ev.x, ev.y, g.now});
            size_t first = g.recent.size() - 1;
            while (first > 0 && g.now - g.recent[first - 1].t <= 90) --first;
            const GaPt& a = g.recent[first];
            const float dt = (float)std::max<Uint32>(16, g.now - a.t);
            const float vx = ev.x - a.x, vy = ev.y - a.y;
            if (hypotf(vx, vy) / dt / s < 0.45f) { ga_flash(g, "FLICK HARDER", true); break; }
            const float dir = atan2f(vy, vx);
            int best = -1;
            float bestd = 0.62f;   /* about 35 degrees */
            bool holder = false;
            /* a player already carrying a bomb cannot be given another (the engine refuses
               it), so the flick passes over them to the next one that way */
            for (size_t i = 0; i < w.enemies.size(); ++i) {
                const float d = fabsf(ga_wrap(atan2f(w.enemies[i].sy - by, w.enemies[i].sx - bx) - dir));
                if (d >= 0.62f) continue;
                if (w.enemies[i].potato) { holder = true; continue; }
                if (d < bestd) { bestd = d; best = (int)i; }
            }
            if (best < 0 && holder) { ga_flash(g, "THAT ONE HAS A BOMB", true); break; }
            if (best < 0) { ga_end(g, GA_FAILED, "NO ONE THAT WAY"); break; }
            ga_take_target(g, w.enemies[best]);
            ga_end(g, GA_DONE, "flicked");
        }
        break;
    }

    case GA_BOOBY_CRATE: {
        /* step: pieces in place; carry: the piece in the hand */
        if (ldown) {
            for (int i = 0; i < 3; ++i) {
                float x, y;
                ga_at(p, GA_GIFT_ITEM_U, GA_GIFT_ITEM_V[i], &x, &y);
                if (i >= g.step && fabsf(ev.x - x) <= 18.0f * s && fabsf(ev.y - y) <= 15.0f * s) { g.carry = i; break; }
            }
        } else if (lup && g.carry >= 0) {
            const int i = g.carry;
            g.carry = -1;
            if (i != g.step) {
                char text[40];
                snprintf(text, sizeof text, "%s FIRST", GA_GIFT_NAME[g.step]);
                ga_flash(g, text, true);
                break;
            }
            float u, v;
            ga_uv(p, ev.x, ev.y, &u, &v);
            const float* z = GA_GIFT_ZONE[i];
            if (u < z[0] || u > z[2] || v < z[1] || v > z[3]) { ga_flash(g, "MISSED, TRY AGAIN", true); break; }
            if (++g.step == 3) ga_end(g, GA_DONE, "wrapped");
        }
        break;
    }

    case GA_WORMHOLE:
        if (ldown) ga_flash(g, "USE THE RIGHT BUTTON", true);
        if (rdown) {
            int cx, cy;
            if (ga_cell_at(w, ev.x, ev.y, &cx, &cy)) { g.flag = true; g.ax = ev.x; g.ay = ev.y; ga_set_cell(g, 0, cx, cy); }
            else ga_flash(g, "START ON THE MAP", true);
        } else if (rup && g.flag) {
            g.flag = false;
            int cx, cy;
            if (!ga_cell_at(w, ev.x, ev.y, &cx, &cy)) { ga_flash(g, "END ON THE MAP", true); break; }
            if (hypotf((float)(cx - g.cells[0]), (float)(cy - g.cells[1])) < 3.0f) { ga_flash(g, "FOLD IT FURTHER", true); break; }
            ga_set_cell(g, 1, cx, cy);
            g.ncells = 2;
            ga_end(g, GA_DONE, "folded");
        }
        break;

    case GA_DANCE_OFF:
        /* recent: the last moves, for the rest test in ga_tick. A flick is judged only
           once the hand is ready for this arrow (sel, armed at rest by ga_tick), measured
           from where it rested: 20 units of travel within 220 ms of starting. Slower
           than that is a drift, and the hand has to settle again. The distance is short
           enough to make from the ability cameo, at the screen's right edge, rightwards. */
        if (move) {
            g.recent.push_back({ev.x, ev.y, g.now});
            while (g.recent.size() > 1 && g.now - g.recent.front().t > 140) g.recent.erase(g.recent.begin());
            if (g.step >= 4 || g.sel != g.step || e < ga_arrow_at(g.step)) break;
            const float dx = ev.x - g.ax, dy = ev.y - g.ay, d = hypotf(dx, dy);
            if (d <= 3.0f * s) { g.mark2 = 0; break; }   /* still at rest */
            if (!g.mark2) g.mark2 = g.now;
            if (d < 20.0f * s) break;
            if (g.now - g.mark2 > 220) { g.sel = -1; break; }
            const float want = atan2f(GA_DIR_Y[g.dir[g.step]], GA_DIR_X[g.dir[g.step]]);
            if (fabsf(ga_wrap(atan2f(dy, dx) - want)) <= 0.7f) {
                ga_flash(g, "NICE", false);
                if (++g.step == 4) ga_end(g, GA_DONE, "danced");
            } else {
                ga_end(g, GA_FAILED, "WRONG WAY");
            }
        }
        break;

    default:
        break;
    }
}

/* ---- drawing a game ------------------------------------------------------------------ */

static void ga_quad_line(float x0, float y0, float x1, float y1, float width, float r, float g, float b, float a)
{
    const float dx = x1 - x0, dy = y1 - y0, len = hypotf(dx, dy);
    if (len < 1e-3f) return;
    const float nx = -dy / len * width * 0.5f, ny = dx / len * width * 0.5f;
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x0 + nx, y0 + ny); glVertex2f(x1 + nx, y1 + ny);
    glVertex2f(x1 - nx, y1 - ny); glVertex2f(x0 - nx, y0 - ny);
    glEnd();
}

static void ga_ring(float cx, float cy, float rad, float width, float r, float g, float b, float a, int seg = 40)
{
    for (int i = 0; i < seg; ++i) {
        const float a0 = 2.0f * (float)M_PI * i / seg, a1 = 2.0f * (float)M_PI * (i + 1) / seg;
        ga_quad_line(cx + cosf(a0) * rad, cy + sinf(a0) * rad, cx + cosf(a1) * rad, cy + sinf(a1) * rad, width, r, g, b, a);
    }
}

static void ga_disc(float cx, float cy, float rad, float r, float g, float b, float a, int seg = 28)
{
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; ++i) {
        const float t = 2.0f * (float)M_PI * i / seg;
        glVertex2f(cx + cosf(t) * rad, cy + sinf(t) * rad);
    }
    glEnd();
}

static void ga_strip(const std::vector<GaPt>& p, float width, float r, float g, float b, float a)
{
    for (size_t i = 1; i < p.size(); ++i) ga_quad_line(p[i - 1].x, p[i - 1].y, p[i].x, p[i].y, width, r, g, b, a);
}

/* A ring lying on the ground, through the camera, so it reads as flat in every view. */
static void ga_ground_ring(const GaWorld& w, float wx, float wz, float radius, float width,
                           float r, float g, float b, float a, float spin = 0, bool dashed = false)
{
    const int seg = 36;
    float px = 0, py = 0;
    for (int i = 0; i <= seg; ++i) {
        const float t = spin + 2.0f * (float)M_PI * i / seg;
        float x, y;
        w.to_screen(wx + cosf(t) * radius, wz + sinf(t) * radius, &x, &y);
        if (i && (!dashed || (i & 1))) ga_quad_line(px, py, x, y, width, r, g, b, a);
        px = x; py = y;
    }
}

/* An engine range on the ground, dashed: every point whose engine distance from the
   centre is `range` (ga_engine_dist), an eight-sided shape rather than a circle. */
static void ga_ground_range(const GaWorld& w, float wx, float wz, float range, float width,
                            float r, float g, float b, float a)
{
    const int seg = 72;
    float px = 0, py = 0;
    for (int i = 0; i <= seg; ++i) {
        const float t = 2.0f * (float)M_PI * i / seg;
        const float c = fabsf(cosf(t)), sn = fabsf(sinf(t));
        const float k = range / (std::max(c, sn) + 0.5f * std::min(c, sn));
        float x, y;
        w.to_screen(wx + cosf(t) * k, wz + sinf(t) * k, &x, &y);
        if (i & 1) ga_quad_line(px, py, x, y, width, r, g, b, a);
        px = x; py = y;
    }
}

static void ga_text_c(float cx, float y, const char* text, int s, float r, float g, float b)
{
    sb_text(cx - sb_text_w(text, s) * 0.5f, y, text, s, r, g, b);
}

static void ga_cell_box(const GaWorld& w, int cx, int cy, float r, float g, float b, float a)
{
    float x[4], y[4];
    const float cxs[4] = {0, 1, 1, 0}, cys[4] = {0, 0, 1, 1};
    for (int i = 0; i < 4; ++i) w.to_screen(cx + cxs[i], cy + cys[i], &x[i], &y[i]);
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; ++i) glVertex2f(x[i], y[i]);
    glEnd();
}

static void ga_well(float x0, float y0, float x1, float y1, int s)
{
    sb_rect(x0, y0, x1, y1, 0.10f, 0.09f, 0.07f, 1);
    sb_frame(x0, y0, x1, y1, 0.42f, 0.36f, 0.27f, 1);
    (void)s;
}

/* A map cameo for the shell game: the unit's own sidebar art when there is one. */
static void ga_draw_unit_cameo(const char* type, float x0, float y0, float x1, float y1)
{
    const SbTex* art = type && *type ? sb_cameo(type) : nullptr;
    if (!art && type && *type) art = sb_cameo_true(type);
    if (!art) {
        sb_rect(x0, y0, x1, y1, 0.25f, 0.45f, 0.25f, 1);
        return;
    }
    const float u = (float)art->uw / art->w, v = (float)art->uh / art->h;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, art->gl);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x0, y0);
    glTexCoord2f(u, 0); glVertex2f(x1, y0);
    glTexCoord2f(u, v); glVertex2f(x1, y1);
    glTexCoord2f(0, v); glVertex2f(x0, y1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static void ga_draw_game(const GaGame& g, const GaWorld& w)
{
    const int fbw = w.fbw, fbh = w.fbh;
    const GaPanel p = ga_panel(g, fbw, fbh);
    const int s = p.s;
    const float fs = (float)s;
    const Uint32 e = g.now - g.t0;
    const float pulse = 0.5f + 0.5f * sinf((float)g.now * 0.012f);

    begin_overlay(fbw, fbh);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* ---- what the game draws on the map, under the panel ---- */
    switch (g.id) {
    case GA_STEALTH: {
        const float f = std::min(1.0f, (float)(g.now - g.mark) / 1500.0f);
        const float rad = (40.0f - 30.0f * f) * fs;
        ga_ring(g.ax, g.ay, rad, 2.0f * fs, 0.55f, 0.85f, 1.0f, 0.9f);
        ga_ring(g.ax, g.ay, 40.0f * fs, 1.0f * fs, 0.55f, 0.85f, 1.0f, 0.25f);
        break;
    }
    case GA_CHRONO_HOP:
        if (!w.unit) break;
        if (g.phase == 0) {
            ga_ring(w.ux, w.uy, (18.0f + 4.0f * pulse) * fs, 2.0f * fs, 0.45f, 0.75f, 1.0f, 0.9f);
        } else {
            float wx, wz;
            if (!w.to_world(g.mx, g.my, &wx, &wz)) break;
            int cx, cy;
            const float len = ga_hop_landing(w, wx, wz, &cx, &cy);
            float lx, ly;
            w.to_screen(cx + 0.5f, cy + 0.5f, &lx, &ly);
            ga_quad_line(w.ux, w.uy, g.mx, g.my, 2.0f * fs, 0.85f, 0.75f, 0.45f, 0.8f);
            const float hx = (w.ux + lx) * 0.5f, hy = std::min(w.uy, ly) - (30.0f + len * 6.0f) * fs;
            for (int i = 0; i < 20; i += 2) {
                const float t0 = i / 20.0f, t1 = (i + 1) / 20.0f;
                const auto bez = [&](float t, float* x, float* y) {
                    *x = (1 - t) * (1 - t) * w.ux + 2 * (1 - t) * t * hx + t * t * lx;
                    *y = (1 - t) * (1 - t) * w.uy + 2 * (1 - t) * t * hy + t * t * ly;
                };
                float x0, y0, x1, y1;
                bez(t0, &x0, &y0); bez(t1, &x1, &y1);
                ga_quad_line(x0, y0, x1, y1, 2.0f * fs, 0.45f, 0.75f, 1.0f, 0.95f);
            }
            const bool ok = !ga_hop_too_short(w, len, cx, cy) && w.on_map(cx, cy);
            ga_ground_ring(w, cx + 0.5f, cy + 0.5f, 0.45f, 2.0f * fs, ok ? 0.45f : 0.9f, ok ? 0.75f : 0.3f, ok ? 1.0f : 0.25f, 0.95f);
        }
        break;
    case GA_CHRONO_SWAP:
        for (const GaTarget& t : w.enemies)
            if (ga_engine_dist(w.uex, w.uez, t.ex, t.ez) <= GA_SWAP_RANGE)
                ga_ring(t.sx, t.sy, 12.0f * fs, 1.0f * fs, 0.95f, 0.55f, 0.35f, 0.45f + 0.4f * pulse);
        ga_strip(g.stroke, 2.0f * fs, 0.45f, 0.75f, 1.0f, 0.95f);
        if (w.unit) ga_ground_range(w, w.uex, w.uez, GA_SWAP_RANGE, 1.0f * fs, 0.45f, 0.75f, 1.0f, 0.35f);
        break;
    case GA_VORTEX:
        ga_strip(g.stroke, 2.0f * fs, 0.70f, 0.55f, 1.0f, 0.95f);
        break;
    case GA_MINEFIELD:
        if (w.unit) {
            ga_ground_range(w, w.uex, w.uez, GA_MINE_RANGE, 1.5f * fs, 0.95f, 0.75f, 0.35f, 0.7f);
            for (int i = 0; i < g.ncells; ++i) {
                float x, y;
                w.to_screen(g.cells[2 * i] + 0.5f, g.cells[2 * i + 1] + 0.5f, &x, &y);
                ga_disc(x, y, 6.0f * fs, 0.25f, 0.22f, 0.18f, 1);
                ga_ring(x, y, 6.0f * fs, 1.0f * fs, 0.95f, 0.35f, 0.25f, 1);
            }
        }
        if (g.carry >= 0) {
            ga_disc(g.mx, g.my, 8.0f * fs, 0.25f, 0.22f, 0.18f, 1);
            ga_ring(g.mx, g.my, 8.0f * fs, 1.0f * fs, 0.95f, 0.35f, 0.25f, 1);
        }
        break;
    case GA_SMOKE:
        for (int i = 0; i < g.ncells; ++i) ga_cell_box(w, g.cells[2 * i], g.cells[2 * i + 1], 0.75f, 0.75f, 0.72f, 0.45f);
        break;
    case GA_HOT_POTATO: {
        if (!w.unit) break;
        const float bx = g.flag ? g.mx : w.ux, by = g.flag ? g.my : w.uy - 34.0f * fs;
        ga_disc(bx, by, 9.0f * fs, 0.12f, 0.11f, 0.10f, 1);
        ga_ring(bx, by, 9.0f * fs, 1.0f * fs, 0.95f, 0.55f, 0.30f, 1);
        ga_quad_line(bx + 5 * fs, by - 7 * fs, bx + 9 * fs, by - 12 * fs, 2.0f * fs, 0.85f, 0.75f, 0.45f, 1);
        ga_disc(bx + 9 * fs, by - 12 * fs, (2.0f + 1.5f * pulse) * fs, 1.0f, 0.7f, 0.2f, 1);
        for (const GaTarget& t : w.enemies)
            if (!t.potato) ga_ring(t.sx, t.sy, 12.0f * fs, 1.0f * fs, 0.95f, 0.55f, 0.35f, 0.45f + 0.4f * pulse);
        break;
    }
    case GA_WORMHOLE:
        if (g.flag) {
            float x0, y0;
            w.to_screen(g.cells[0] + 0.5f, g.cells[1] + 0.5f, &x0, &y0);
            ga_ground_ring(w, g.cells[0] + 0.5f, g.cells[1] + 0.5f, 0.6f, 2.0f * fs, 0.75f, 0.45f, 1.0f, 1);
            const float hx = (x0 + g.mx) * 0.5f, hy = std::min(y0, g.my) - 40.0f * fs;
            float lx = x0, ly = y0;
            for (int i = 1; i <= 20; ++i) {
                const float t = i / 20.0f;
                const float x = (1 - t) * (1 - t) * x0 + 2 * (1 - t) * t * hx + t * t * g.mx;
                const float y = (1 - t) * (1 - t) * y0 + 2 * (1 - t) * t * hy + t * t * g.my;
                ga_quad_line(lx, ly, x, y, 2.0f * fs, 0.75f, 0.45f, 1.0f, 0.9f);
                lx = x; ly = y;
            }
            ga_ring(g.mx, g.my, 10.0f * fs, 2.0f * fs, 0.75f, 0.45f, 1.0f, 0.9f);
        }
        break;
    case GA_HUMILIATION:
    case GA_MIRROR:
        if (g.phase == 0)
            for (const GaTarget& t : w.enemies)
                if (g.id != GA_HUMILIATION || t.level != 1)
                    ga_ring(t.sx, t.sy, 12.0f * fs, 1.0f * fs, 0.95f, 0.55f, 0.35f, 0.45f + 0.4f * pulse);
        break;
    default:
        break;
    }

    /* ---- the panel ---- */
    sb_rect(p.x, p.y, p.x + p.w, p.y + p.h, 0.094f, 0.09f, 0.075f, 0.97f);
    sb_frame(p.x, p.y, p.x + p.w, p.y + p.h, 0.67f, 0.55f, 0.33f, 1);
    char title[64];
    snprintf(title, sizeof title, "%s: %s", GA_NAME[g.id], GA_GAME[g.id]);
    if (sb_text_w(title, s) > p.w - 16 * s) snprintf(title, sizeof title, "%s", GA_GAME[g.id]);
    const float cx = p.x + p.w * 0.5f;
    ga_text_c(cx, p.y + (p.map ? 6 : 7) * fs, title, s, 1, 0.85f, 0.54f);
    const char* hint = g.id == GA_LAST_LAUGH ? g.clue
                     : (g.id == GA_HUMILIATION || g.id == GA_MIRROR) && g.phase == 0 ? "FIRST CLICK AN ENEMY UNIT"
                     : GA_HINT[g.id];
    const bool flashing = g.flash[0] && g.now - g.flash_t < 900;
    if (flashing)
        ga_text_c(cx, p.y + (p.map ? 19 : 20) * fs, g.flash, s, g.flash_bad ? 0.96f : 0.45f, g.flash_bad ? 0.35f : 0.85f, g.flash_bad ? 0.28f : 0.40f);
    else
        ga_text_c(cx, p.y + (p.map ? 19 : 20) * fs, hint, s, 0.79f, 0.75f, 0.66f);

    /* the timer */
    const float left = std::max(0.0f, 1.0f - (float)e / (float)g.limit);
    const float secs = std::max(0.0f, (float)(g.limit - std::min(e, g.limit)) / 1000.0f);
    const float barY = p.map ? p.y + p.h - 14 * fs : p.y + 140 * fs;
    const float barX0 = p.x + (p.map ? 40 : 10) * fs, barX1 = p.x + p.w - 46 * fs;
    sb_rect(barX0, barY, barX1, barY + 5 * fs, 0.16f, 0.14f, 0.10f, 1);
    const bool late = secs < 1.0f;
    sb_rect(barX0, barY, barX0 + (barX1 - barX0) * left, barY + 5 * fs, late ? 0.92f : 0.95f, late ? 0.30f : 0.78f, late ? 0.22f : 0.40f, 1);
    char clock[16];
    snprintf(clock, sizeof clock, "%.1f", secs);
    sb_text(barX1 + 6 * fs, barY - 3 * fs, clock, s, 0.91f, 0.89f, 0.84f);
    if (p.map) sb_text(p.x + 10 * fs, barY - 3 * fs, "ESC", s, 0.60f, 0.56f, 0.48f);
    else ga_text_c(cx, p.y + 151 * fs, "ESC CANCEL", s, 0.60f, 0.56f, 0.48f);

    /* ---- the play area ---- */
    const float ax = p.ax, ay = p.ay, aw = p.aw, ah = p.ah;
    switch (g.id) {
    case GA_IRON_CURTAIN: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        const float drop = ah * g.v;
        for (int i = 0; i < 14; ++i) {
            const float x0 = ax + aw * i / 14.0f, x1 = ax + aw * (i + 1) / 14.0f;
            const float shade = (i & 1) ? 0.62f : 0.52f;
            sb_rect(x0, ay, x1, ay + drop, shade, 0.10f, 0.08f, 1);
        }
        sb_rect(ax, ay + drop - 3 * fs, ax + aw, ay + drop, 0.85f, 0.70f, 0.35f, 1);
        char pct[16];
        snprintf(pct, sizeof pct, "%d%%", (int)(g.v * 100.0f));
        ga_text_c(ax + aw * 0.5f, ay + ah - 27 * fs, pct, 2 * s, 0.96f, 0.94f, 0.86f);
        break;
    }
    case GA_OVERDRIVE: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        const float gx = ax + aw * 0.5f, gy = ay + ah * 0.92f, R = ah * 0.8f;
        const auto at = [&](float v, float r, float* x, float* y) {
            const float a = (float)M_PI * (1.0f - v / 1.0f);
            *x = gx + cosf(a) * r; *y = gy - sinf(a) * r;
        };
        for (int i = 0; i < 40; ++i) {
            const float v0 = i / 40.0f, v1 = (i + 1) / 40.0f;
            const bool red = v0 >= 0.78f && v1 <= 0.97f;
            float x0, y0, x1, y1;
            at(v0, R, &x0, &y0); at(v1, R, &x1, &y1);
            ga_quad_line(x0, y0, x1, y1, (red ? 7.0f : 3.0f) * fs, red ? 0.9f : 0.55f, red ? 0.2f : 0.5f, red ? 0.15f : 0.42f, 1);
        }
        float nx, ny;
        at(std::min(1.0f, g.v), R * 0.92f, &nx, &ny);
        ga_quad_line(gx, gy, nx, ny, 3.0f * fs, 0.96f, 0.94f, 0.86f, 1);
        ga_disc(gx, gy, 5.0f * fs, 0.85f, 0.70f, 0.35f, 1);
        ga_text_c(gx, ay + 6 * fs, "RPM", s, 0.79f, 0.75f, 0.66f);
        break;
    }
    case GA_HUNTERS_EYE: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        const float rx = ax + aw * 0.33f, ry = ay + ah * 0.5f, R = ah * 0.44f;
        ga_disc(rx, ry, R, 0.05f, 0.14f, 0.07f, 1);
        ga_ring(rx, ry, R, 1.5f * fs, 0.35f, 0.75f, 0.40f, 1);
        ga_ring(rx, ry, R * 0.5f, 1.0f * fs, 0.25f, 0.55f, 0.30f, 0.8f);
        const float noise = std::min(1.0f, fabsf(g.v - g.nx[0]) / 20.0f);
        std::mt19937 n((unsigned)(g.now / 50));
        const int dots = (int)(220 * noise);
        for (int i = 0; i < dots; ++i) {
            const float a = (n() % 6283) / 1000.0f, r = R * sqrtf((n() % 1000) / 1000.0f);
            const float x = rx + cosf(a) * r, y = ry + sinf(a) * r, l = 0.5f + (n() % 50) / 100.0f;
            sb_rect(x, y, x + fs, y + fs, l, l, l, 0.9f);
        }
        const float sweep = (float)g.now * 0.004f;
        ga_quad_line(rx, ry, rx + cosf(sweep) * R, ry + sinf(sweep) * R, 1.5f * fs, 0.45f, 1.0f, 0.5f, 0.8f);
        if (noise < 0.2f) ga_disc(rx + R * 0.45f, ry - R * 0.3f, 4.0f * fs, 1.0f, 0.3f, 0.25f, 1.0f - noise * 4);
        const float dx0 = ax + aw * 0.66f, dx1 = ax + aw * 0.95f, dy = ay + ah * 0.55f;
        sb_rect(dx0, dy, dx1, dy + 4 * fs, 0.30f, 0.28f, 0.22f, 1);
        for (int i = 0; i <= 10; ++i) sb_rect(dx0 + (dx1 - dx0) * i / 10.0f, dy - 3 * fs, dx0 + (dx1 - dx0) * i / 10.0f + fs, dy, 0.55f, 0.5f, 0.4f, 1);
        const float kx = dx0 + (dx1 - dx0) * g.v / 100.0f;
        sb_rect(kx - 2 * fs, dy - 8 * fs, kx + 2 * fs, dy + 12 * fs, 0.95f, 0.78f, 0.40f, 1);
        char f[16];
        snprintf(f, sizeof f, "%.0f MHZ", 100.0f + g.v);
        ga_text_c((dx0 + dx1) * 0.5f, ay + ah * 0.2f, f, s, 0.91f, 0.89f, 0.84f);
        ga_text_c((dx0 + dx1) * 0.5f, ay + ah * 0.75f, noise < 0.15f ? "CLEAN" : "STATIC", s, noise < 0.15f ? 0.45f : 0.75f, noise < 0.15f ? 0.9f : 0.7f, noise < 0.15f ? 0.45f : 0.6f);
        break;
    }
    case GA_HUMILIATION: {
        if (g.phase == 0) break;
        ga_well(ax, ay, ax + aw, ay + ah, s);
        const float y0 = ay + ah * 0.4f, y1 = ay + ah * 0.62f;
        const float x0 = ax + aw * 0.05f, x1 = ax + aw * 0.95f;
        for (int i = 0; i < 30; ++i) {
            const float f = i / 30.0f;
            sb_rect(x0 + (x1 - x0) * f, y0, x0 + (x1 - x0) * (f + 1 / 30.0f), y1, 0.25f + 0.4f * f, 0.30f, 0.18f, 1);
        }
        const float nx0 = x0 + (x1 - x0) * (g.nx[0] - 0.04f), nx1 = x0 + (x1 - x0) * (g.nx[0] + 0.04f);
        sb_rect(nx0, y0 - 6 * fs, nx1, y1 + 6 * fs, 0.30f, 0.85f, 0.35f, 1);
        const float nd = x0 + (x1 - x0) * ga_golf_needle(g);
        sb_rect(nd - 1.5f * fs, y0 - 10 * fs, nd + 1.5f * fs, y1 + 10 * fs, 0.96f, 0.94f, 0.86f, 1);
        ga_text_c(ax + aw * 0.5f, ay + ah * 0.8f, "CLICK ON THE GREEN NOTCH", s, 0.79f, 0.75f, 0.66f);
        break;
    }
    case GA_CHAIN_SHOCK: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        ga_strip(g.stroke, 3.0f * fs, 0.55f, 0.80f, 1.0f, 0.9f);
        for (int i = 0; i < 3; ++i) {
            float x, y;
            ga_at(p, g.nx[i], g.ny[i], &x, &y);
            const bool done = g.drawing && i < g.step;
            ga_disc(x, y, (13.0f + 2.0f * pulse) * fs, 0.30f, 0.55f, 1.0f, 0.25f);
            ga_disc(x, y, 9.0f * fs, done ? 0.35f : 0.20f, done ? 0.85f : 0.40f, done ? 0.40f : 0.85f, 1);
            char n[4];
            snprintf(n, sizeof n, "%d", i + 1);
            ga_text_c(x, y - 5 * fs, n, s, 1, 1, 1);
        }
        break;
    }
    case GA_EXTRA_LIFE: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        const float span = 3200.0f, mid = ay + ah * 0.55f;
        /* the whole trace, faint, so each spike can be seen coming; the part already
           run is drawn bright over it */
        std::vector<GaPt> line, ahead;
        const float upto = std::min((float)e, span);
        for (int i = 0; i <= 160; ++i) {
            const float t = span * i / 160.0f;
            float y = mid;
            for (int k = 0; k < 3; ++k) {
                const float d = (t - GA_BEAT[k]) / 60.0f;
                if (fabsf(d) < 1.0f) y -= ah * 0.4f * (1.0f - fabsf(d)) * (d < 0 ? 1.0f : -0.6f);
            }
            ahead.push_back({ax + aw * t / span, y, 0});
            if (t <= upto) line.push_back({ax + aw * t / span, y, 0});
        }
        ga_strip(ahead, 1.0f * fs, 0.25f, 0.55f, 0.30f, 0.6f);
        ga_strip(line, 2.0f * fs, 0.35f, 0.95f, 0.45f, 1);
        if (!line.empty()) ga_disc(line.back().x, line.back().y, 3.0f * fs, 0.7f, 1.0f, 0.7f, 1);
        for (int k = 0; k < 3; ++k) {
            const float hx = ax + aw * (0.38f + 0.12f * k), hy = ay + 8 * fs;
            const bool hit = k < g.step;
            ga_disc(hx, hy + 4 * fs, 4.5f * fs, hit ? 0.95f : 0.35f, hit ? 0.25f : 0.20f, hit ? 0.25f : 0.20f, 1);
        }
        break;
    }
    case GA_DECOYS: {
        const bool up = e < GA_SHELL_SHOW;
        for (int c = 0; c < 3; ++c) {
            float x, y;
            ga_card_pos(g, p, c, e, &x, &y);
            const float w2 = 34.0f * fs, h2 = 26.0f * fs;
            sb_rect(x - w2, y - h2, x + w2, y + h2, 0.10f, 0.09f, 0.07f, 1);
            if (up) {
                if (c == g.sel) ga_draw_unit_cameo(w.utype, x - 32 * fs, y - 24 * fs, x + 32 * fs, y + 24 * fs);
                else sb_rect(x - 32 * fs, y - 24 * fs, x + 32 * fs, y + 24 * fs, 0.22f, 0.20f, 0.16f, 1);
            } else {
                for (int i = 0; i < 6; ++i)
                    sb_rect(x - 32 * fs, y - 24 * fs + i * 8 * fs, x + 32 * fs, y - 20 * fs + i * 8 * fs, 0.45f, 0.36f, 0.20f, 1);
                ga_text_c(x, y - 5 * fs, "?", s, 1, 0.85f, 0.54f);
            }
            const bool hot = e >= ga_shell_ready() && fabsf(g.mx - x) <= w2 && fabsf(g.my - y) <= h2;
            sb_frame(x - w2, y - h2, x + w2, y + h2, hot ? 1.0f : 0.67f, hot ? 0.8f : 0.55f, 0.33f, 1);
        }
        if (e >= ga_shell_ready()) ga_text_c(ax + aw * 0.5f, ay + ah - 10 * fs, "WHICH ONE IS YOU?", s, 0.79f, 0.75f, 0.66f);
        break;
    }
    case GA_GIANT: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        for (int i = 0; i < 5; ++i) {
            const float x0 = ax + aw * (0.3f + 0.09f * i), x1 = x0 + aw * 0.07f;
            const bool on = i < g.count;
            sb_rect(x0, ay + ah * 0.2f, x1, ay + ah * 0.8f, on ? 0.35f : 0.18f, on ? 0.85f : 0.16f, on ? 0.40f : 0.12f, 1);
            sb_frame(x0, ay + ah * 0.2f, x1, ay + ah * 0.8f, 0.55f, 0.48f, 0.36f, 1);
        }
        const float px0 = ax + aw * 0.12f, top = ay + ah * 0.15f, bot = ay + ah * 0.85f;
        const float hy = g.ldown ? std::max(top, std::min(bot, g.my)) : top;
        sb_rect(px0 - 2 * fs, hy, px0 + 2 * fs, bot, 0.6f, 0.55f, 0.45f, 1);
        sb_rect(px0 - 12 * fs, hy - 3 * fs, px0 + 12 * fs, hy + 2 * fs, 0.85f, 0.70f, 0.35f, 1);
        break;
    }
    case GA_LAST_LAUGH: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        const bool cut = g.result != GA_RUNNING;
        for (int i = 0; i < 3; ++i) {
            const float* c = GA_WIRE_RGB[g.colour[i]];
            float px0 = 0, py0 = 0;
            for (int k = 0; k <= 16; ++k) {
                const float v = 0.05f + 0.9f * k / 16.0f;
                float x, y;
                ga_at(p, GA_WIRE_U[i], v, &x, &y);
                x += sinf(v * 9.0f + i) * 4.0f * fs;
                if (k && !(cut && i == g.step && k == 8)) ga_quad_line(px0, py0, x, y, 3.0f * fs, c[0], c[1], c[2], 1);
                px0 = x; py0 = y;
            }
            float lx, ly;
            ga_at(p, GA_WIRE_U[i], 1.0f, &lx, &ly);
        }
        ga_quad_line(g.mx - 6 * fs, g.my - 6 * fs, g.mx + 6 * fs, g.my + 6 * fs, 2.0f * fs, 0.85f, 0.85f, 0.85f, 1);
        ga_quad_line(g.mx - 6 * fs, g.my + 6 * fs, g.mx + 6 * fs, g.my - 6 * fs, 2.0f * fs, 0.85f, 0.85f, 0.85f, 1);
        break;
    }
    case GA_BOOBY_CRATE: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        float x0, y0, x1, y1;
        ga_at(p, GA_GIFT_ZONE[0][0], GA_GIFT_ZONE[0][1], &x0, &y0);
        ga_at(p, GA_GIFT_ZONE[0][2], GA_GIFT_ZONE[0][3], &x1, &y1);
        sb_rect(x0, y0, x1, y1, 0.45f, 0.33f, 0.18f, 1);
        sb_frame(x0, y0, x1, y1, 0.65f, 0.50f, 0.30f, 1);
        const auto piece = [&](int i, float x, float y) {
            if (i == 0) { ga_disc(x, y, 9 * fs, 0.12f, 0.11f, 0.10f, 1); ga_disc(x + 6 * fs, y - 8 * fs, 2.5f * fs, 1.0f, 0.7f, 0.2f, 1); }
            else if (i == 1) sb_rect(x - 22 * fs, y - 5 * fs, x + 22 * fs, y + 5 * fs, 0.55f, 0.40f, 0.22f, 1);
            else { ga_disc(x - 6 * fs, y, 6 * fs, 0.9f, 0.25f, 0.30f, 1); ga_disc(x + 6 * fs, y, 6 * fs, 0.9f, 0.25f, 0.30f, 1); ga_disc(x, y, 3 * fs, 1.0f, 0.5f, 0.5f, 1); }
        };
        for (int i = 0; i < 3; ++i) {
            if (i < g.step) {
                const float* z = GA_GIFT_ZONE[i];
                float zx, zy;
                ga_at(p, (z[0] + z[2]) * 0.5f, (z[1] + z[3]) * 0.5f, &zx, &zy);
                if (i == 1) ga_at(p, (z[0] + z[2]) * 0.5f, z[3] - 0.04f, &zx, &zy);
                if (i == 2) ga_at(p, (z[0] + z[2]) * 0.5f, z[3] - 0.08f, &zx, &zy);
                piece(i, zx, zy);
            } else if (i == g.carry) {
                piece(i, g.mx, g.my);
            } else {
                float x, y;
                ga_at(p, GA_GIFT_ITEM_U, GA_GIFT_ITEM_V[i], &x, &y);
                piece(i, x, y);
                if (i == g.step) sb_frame(x - 24 * fs, y - 15 * fs, x + 24 * fs, y + 15 * fs, 0.95f, 0.78f, 0.40f, 0.5f + 0.5f * pulse);
            }
        }
        break;
    }
    case GA_DANCE_OFF: {
        ga_well(ax, ay, ax + aw, ay + ah, s);
        for (int k = 0; k < 4; ++k) {
            float x, y;
            ga_at(p, 0.2f + 0.2f * k, 0.5f, &x, &y);
            const bool shown = e >= ga_arrow_at(k);
            const bool done = k < g.step;
            const bool now = k == g.step && shown;
            const float c = done ? 0.4f : now ? 1.0f : 0.25f;
            sb_frame(x - 18 * fs, y - 18 * fs, x + 18 * fs, y + 18 * fs, 0.55f, 0.48f, 0.36f, 1);
            if (!shown) continue;
            const float dx = GA_DIR_X[g.dir[k]], dy = GA_DIR_Y[g.dir[k]];
            const float tx = x + dx * 12 * fs, ty = y + dy * 12 * fs;
            ga_quad_line(x - dx * 10 * fs, y - dy * 10 * fs, tx, ty, 4.0f * fs, done ? 0.35f : c, done ? 0.85f : c * 0.85f, done ? 0.4f : c * 0.5f, 1);
            ga_quad_line(tx, ty, tx - dx * 7 * fs - dy * 7 * fs, ty - dy * 7 * fs - dx * 7 * fs, 4.0f * fs, done ? 0.35f : c, done ? 0.85f : c * 0.85f, done ? 0.4f : c * 0.5f, 1);
            ga_quad_line(tx, ty, tx - dx * 7 * fs + dy * 7 * fs, ty - dy * 7 * fs + dx * 7 * fs, 4.0f * fs, done ? 0.35f : c, done ? 0.85f : c * 0.85f, done ? 0.4f : c * 0.5f, 1);
            if (now) {
                const float f = 1.0f - std::min(1.0f, (float)(e - ga_arrow_at(k)) / GA_ARROW_WINDOW);
                sb_rect(x - 18 * fs, y + 21 * fs, x - 18 * fs + 36 * fs * f, y + 24 * fs, 0.95f, 0.78f, 0.40f, 1);
            }
        }
        break;
    }
    case GA_MIRROR:
        if (g.phase == 1) {
            ga_well(ax, ay, ax + aw, ay + ah, s);
            const float hx = ax + aw * 0.5f, hw = aw * 0.5f;
            sb_rect(hx - fs, ay, hx + fs, ay + ah, 0.55f, 0.48f, 0.36f, 1);
            std::vector<GaPt> shape;
            for (const GaPt& q : g.shape) shape.push_back({ax + q.x * hw, ay + q.y * ah, 0});
            ga_strip(shape, 3.0f * fs, 0.95f, 0.78f, 0.40f, 1);
            ga_strip(g.stroke, 3.0f * fs, 0.55f, 0.80f, 1.0f, 0.95f);
        }
        break;
    case GA_MINEFIELD:
        for (int i = 0; i < 5; ++i) {
            if (i < g.count || i == g.carry) continue;
            const float x = (float)(p.x + p.w / 2 + (i - 2) * 30 * p.s), y = (float)(p.y + 46 * p.s);
            ga_disc(x, y, 9.0f * fs, 0.25f, 0.22f, 0.18f, 1);
            ga_ring(x, y, 9.0f * fs, 1.0f * fs, 0.95f, 0.35f, 0.25f, 1);
            ga_disc(x, y, 2.5f * fs, 0.95f, 0.35f, 0.25f, 1);
        }
        break;
    case GA_SMOKE: {
        const float f = (float)g.ncells / GA_MAX_CELLS;
        sb_rect(barX0, barY + 7 * fs, barX0 + (barX1 - barX0) * (1.0f - f), barY + 9 * fs, 0.75f, 0.75f, 0.72f, 1);
        break;
    }
    default:
        break;
    }
    glDisable(GL_BLEND);
    end_overlay();
}

/* ---- the auto player: a correct, or a deliberately wrong, gesture ------------------------
   It plays the game the way a hand would: pointer moves, presses and releases, the wheel
   and the keys, each at its own time, through ga_event. Where a real player reacts to what
   is on screen (the rev needle, the golf needle, the static) it reads the same state the
   panel draws. It never writes a game's result itself. */

struct GaQueue {
    std::vector<GaEv> ev;
    void at(Uint32 t, int type, float x, float y, int button = 0, int wheel = 0, int key = 0)
    {
        ev.push_back({type, x, y, button, wheel, key, t});
    }
    void drag(Uint32 t, float x0, float y0, float x1, float y1, Uint32 dur, int button = 1, int steps = 10)
    {
        at(t, GA_EV_MOVE, x0, y0);
        at(t, GA_EV_DOWN, x0, y0, button);
        for (int i = 1; i <= steps; ++i) {
            const float f = (float)i / steps;
            at(t + dur * i / steps, GA_EV_MOVE, x0 + (x1 - x0) * f, y0 + (y1 - y0) * f);
        }
        at(t + dur, GA_EV_UP, x1, y1, button);
    }
    void stroke(Uint32 t, const std::vector<GaPt>& pts, Uint32 dur, int button = 1)
    {
        if (pts.empty()) return;
        at(t, GA_EV_MOVE, pts[0].x, pts[0].y);
        at(t, GA_EV_DOWN, pts[0].x, pts[0].y, button);
        for (size_t i = 1; i < pts.size(); ++i)
            at(t + (Uint32)(dur * i / pts.size()), GA_EV_MOVE, pts[i].x, pts[i].y);
        at(t + dur, GA_EV_UP, pts.back().x, pts.back().y, button);
    }
    void click(Uint32 t, float x, float y, int button = 1)
    {
        at(t, GA_EV_MOVE, x, y);
        at(t, GA_EV_DOWN, x, y, button);
        at(t + 30, GA_EV_UP, x, y, button);
    }
};

static int ga_nearest_enemy(const GaWorld& w, float range)
{
    int best = -1;
    float bestd = 1e9f;
    for (size_t i = 0; i < w.enemies.size(); ++i) {
        const float d = ga_engine_dist(w.uex, w.uez, w.enemies[i].ex, w.enemies[i].ez);
        if (d <= range && d < bestd) { bestd = d; best = (int)i; }
    }
    return best;
}

static void ga_circle_pts(std::vector<GaPt>& out, float cx, float cy, float r, float turns, float r_end, int n, float phase = 0)
{
    for (int i = 0; i <= n; ++i) {
        const float f = (float)i / n;
        const float a = phase + f * turns * 2.0f * (float)M_PI;
        const float rr = r + (r_end - r) * f;
        out.push_back({cx + cosf(a) * rr, cy + sinf(a) * rr, 0});
    }
}

/* The scripted half of a gesture, as events relative to the game's start. The reactive
   half (games 5, 8 and 9) is decided step by step in ga_auto_react. */
static void ga_auto_plan(const GaGame& g, const GaWorld& w, bool good, GaQueue& q)
{
    const GaPanel p = ga_panel(g, w.fbw, w.fbh);
    const float s = (float)p.s;
    const Uint32 t = g.t0;
    switch (g.id) {
    case GA_STEALTH:
        if (!good)
            for (int i = 0; i < 25; ++i) q.at(t + 100 + 150 * i, GA_EV_MOVE, w.mx + ((i & 1) ? 8 : -8) * s, w.my);
        break;
    case GA_CHRONO_HOP: {
        if (good) {
            float x, y;
            w.to_screen(w.uwx - 3.0f, w.uwz + 0.3f, &x, &y);
            q.drag(t + 100, w.ux, w.uy, x, y, 300);
        } else {
            q.drag(t + 100, w.ux + 120 * s, w.uy - 80 * s, w.ux + 40 * s, w.uy - 80 * s, 300);
            q.drag(t + 600, w.ux, w.uy, w.ux + 2, w.uy + 1, 200);
        }
        break;
    }
    case GA_CHRONO_SWAP: {
        const int k = ga_nearest_enemy(w, GA_SWAP_RANGE);
        const float ex = k >= 0 ? w.enemies[k].sx : w.ux + 100 * s, ey = k >= 0 ? w.enemies[k].sy : w.uy;
        std::vector<GaPt> pts;
        if (good) {
            ga_circle_pts(pts, ex, ey, 30 * s, 1.0f, 30 * s, 28);
            q.stroke(t + 100, pts, 450);
        } else {
            pts.push_back({ex - 60 * s, ey - 10 * s, 0});
            pts.push_back({ex + 60 * s, ey + 10 * s, 0});
            q.stroke(t + 100, ga_resample(pts, 6 * s), 300);
            pts.clear();
            ga_circle_pts(pts, ex, ey, 30 * s, 0.7f, 30 * s, 20);
            q.stroke(t + 700, pts, 350);
        }
        break;
    }
    case GA_IRON_CURTAIN:
        for (int i = 0; i < 40; ++i) q.at(t + 80 + (good ? 110 : 500) * i, GA_EV_KEY, 0, 0, 0, 0, SDLK_SPACE);
        break;
    case GA_VORTEX: {
        float cx, cy;
        w.to_screen(w.uwx + 3.0f, w.uwz - 1.0f, &cx, &cy);
        std::vector<GaPt> pts;
        if (good) {
            ga_circle_pts(pts, cx, cy, 60 * s, 2.6f, 6 * s, 90);
            q.stroke(t + 100, pts, 1000);
        } else {
            pts.push_back({cx - 80 * s, cy, 0});
            pts.push_back({cx + 80 * s, cy + 20 * s, 0});
            q.stroke(t + 100, ga_resample(pts, 6 * s), 300);
            pts.clear();
            ga_circle_pts(pts, cx, cy, 50 * s, 2.0f, 50 * s, 80);
            q.stroke(t + 600, pts, 900);
        }
        break;
    }
    case GA_MINEFIELD: {
        static const int off[5][2] = {{2, 0}, {0, 2}, {-2, 0}, {0, -2}, {1, 1}};
        for (int i = 0; i < 5; ++i) {
            const float tx = (float)(p.x + p.w / 2 + (i - 2) * 30 * p.s), ty = (float)(p.y + 46 * p.s);
            float x, y;
            const float d = good ? 1.0f : 3.5f;
            w.to_screen(w.ucx + 0.5f + off[i][0] * d, w.ucy + 0.5f + off[i][1] * d, &x, &y);
            q.drag(t + 100 + 520 * i, tx, ty, x, y, 300, 1, 6);
        }
        break;
    }
    case GA_HUMILIATION:
    case GA_MIRROR: {
        const int k = ga_nearest_enemy(w, 1e9f);
        if (k >= 0) q.click(t + 100, w.enemies[k].sx, w.enemies[k].sy);
        break;
    }
    case GA_CHAIN_SHOCK: {
        float x[3], y[3];
        for (int i = 0; i < 3; ++i) ga_at(p, g.nx[i], g.ny[i], &x[i], &y[i]);
        if (good) {
            const std::vector<GaPt> pts = {{x[0], y[0], 0}, {x[1], y[1], 0}, {x[2], y[2], 0}};
            q.stroke(t + 100, ga_resample(pts, 4 * s), 600);
            break;
        }
        /* wrong: 1 straight to 3 where that way misses 2 (out of order), then 3, 2, 1
           (starts on the wrong node), then a line that touches none */
        const float ux = x[2] - x[0], uy = y[2] - y[0], len = std::max(1.0f, hypotf(ux, uy));
        const float along = ((x[1] - x[0]) * ux + (y[1] - y[0]) * uy) / len;
        const float off = fabsf((x[1] - x[0]) * uy - (y[1] - y[0]) * ux) / len;
        Uint32 at = t + 100;
        if (along < -30 * s || along > len + 30 * s || off > 30 * s) {
            const std::vector<GaPt> skip = {{x[0], y[0], 0}, {x[2], y[2], 0}};
            q.stroke(at, ga_resample(skip, 4 * s), 400);
            at += 600;
        }
        const std::vector<GaPt> back = {{x[2], y[2], 0}, {x[1], y[1], 0}, {x[0], y[0], 0}};
        q.stroke(at, ga_resample(back, 4 * s), 500);
        const std::vector<GaPt> line = {{p.ax + 4 * s, p.ay + 4 * s, 0}, {p.ax + p.aw - 4 * s, p.ay + 4 * s, 0}};
        q.stroke(at + 700, ga_resample(line, 6 * s), 300);
        break;
    }
    case GA_EXTRA_LIFE: {
        float x, y;
        ga_at(p, 0.5f, 0.5f, &x, &y);
        if (good) for (int k = 0; k < 3; ++k) q.click(t + GA_BEAT[k] + 20, x, y);
        else q.click(t + GA_BEAT[0] + 400, x, y);
        break;
    }
    case GA_SMOKE: {
        float cx, cy;
        w.to_screen(w.uwx + 3.0f, w.uwz, &cx, &cy);
        if (good) {
            std::vector<GaPt> pts;
            float x, y;
            for (int row = 0; row < 3; ++row)
                for (int k = 0; k <= 4; ++k) {
                    w.to_screen(w.uwx + 1.5f + ((row & 1) ? 4 - k : k), w.uwz - 1.0f + row, &x, &y);
                    pts.push_back({x, y, 0});
                }
            q.stroke(t + 100, ga_resample(pts, 5 * s), 900);
        } else {
            q.click(t + 100, cx, cy);
        }
        break;
    }
    case GA_DECOYS: {
        const Uint32 at = ga_shell_ready() + 100;
        const int card = good ? g.sel : (g.sel + 1) % 3;
        float x, y;
        ga_card_pos(g, p, card, at, &x, &y);
        q.click(t + at, x, y);
        break;
    }
    case GA_GIANT: {
        const float x = p.ax + p.aw * 0.5f, y = p.ay + p.ah * 0.8f, d = 55 * s;
        q.at(t + 100, GA_EV_MOVE, x, y);
        q.at(t + 100, GA_EV_DOWN, x, y, 1);
        Uint32 at = t + 120;
        for (int k = 0; k < 6; ++k) {
            for (int i = 1; i <= 8; ++i, at += 16)
                q.at(at, GA_EV_MOVE, good ? x : x + d * i / 8.0f, good ? y - d * i / 8.0f : y);
            for (int i = 1; i <= 8; ++i, at += 16)
                q.at(at, GA_EV_MOVE, good ? x : x + d - d * i / 8.0f, good ? y - d + d * i / 8.0f : y);
        }
        q.at(at + 20, GA_EV_UP, x, y, 1);
        break;
    }
    case GA_LAST_LAUGH: {
        const int wire = good ? g.sel : (g.sel + 1) % 3;
        float x, y;
        ga_at(p, GA_WIRE_U[wire], 0.5f, &x, &y);
        q.drag(t + 400, x - 0.1f * p.aw, y, x + 0.1f * p.aw, y + 4 * s, 250);
        break;
    }
    case GA_HOT_POTATO: {
        const float bx = w.ux, by = w.uy - 34 * s;
        float dx = -1, dy = 0;
        const int k = ga_nearest_enemy(w, 1e9f);
        if (k >= 0) {
            dx = w.enemies[k].sx - bx; dy = w.enemies[k].sy - by;
            const float l = std::max(1e-3f, hypotf(dx, dy));
            dx /= l; dy /= l;
        }
        if (!good) { dx = -dx; dy = -dy; }
        q.drag(t + 100, bx, by, bx + dx * 70 * s, by + dy * 70 * s, 64, 1, 4);
        break;
    }
    case GA_BOOBY_CRATE: {
        const int order[3] = {good ? 0 : 1, good ? 1 : 2, good ? 2 : 0};
        for (int k = 0; k < 3; ++k) {
            const int i = order[k];
            float x0, y0, x1, y1;
            ga_at(p, GA_GIFT_ITEM_U, GA_GIFT_ITEM_V[i], &x0, &y0);
            const float* z = GA_GIFT_ZONE[i];
            if (good) ga_at(p, (z[0] + z[2]) * 0.5f, (z[1] + z[3]) * 0.5f, &x1, &y1);
            else ga_at(p, 0.35f, 0.9f, &x1, &y1);
            q.drag(t + 100 + 700 * k, x0, y0, x1, y1, 350, 1, 8);
        }
        break;
    }
    case GA_WORMHOLE: {
        float x0, y0, x1, y1;
        w.to_screen(w.ucx + 1.5f, w.ucy + 0.5f, &x0, &y0);
        if (good) {
            w.to_screen(w.ucx + 6.5f, w.ucy + 2.5f, &x1, &y1);
            q.drag(t + 100, x0, y0, x1, y1, 400, 3);
        } else {
            w.to_screen(w.ucx + 2.5f, w.ucy + 0.5f, &x1, &y1);
            q.drag(t + 100, x0, y0, x1, y1, 300, 3);
            w.to_screen(w.ucx + 7.5f, w.ucy + 0.5f, &x1, &y1);
            q.drag(t + 700, x0, y0, x1, y1, 300, 1);
        }
        break;
    }
    case GA_DANCE_OFF: {
        float x = w.mx, y = w.my;
        for (int k = 0; k < 4; ++k) {
            const int d = good ? g.dir[k] : (g.dir[k] ^ 1);
            const float dx = GA_DIR_X[d] * 48 * s, dy = GA_DIR_Y[d] * 48 * s;
            const Uint32 at = t + ga_arrow_at(k) + 60;
            for (int i = 1; i <= 3; ++i) q.at(at + 16 * i, GA_EV_MOVE, x + dx * i / 3, y + dy * i / 3);
            for (int i = 1; i <= 20; ++i) q.at(at + 48 + 15 * i, GA_EV_MOVE, x + dx - dx * i / 20, y + dy - dy * i / 20);
            if (!good) break;
        }
        break;
    }
    default:
        break;
    }
}

static Uint32 g_gaNextReact = 0;

static void ga_auto_react(GaGame& g, const GaWorld& w, bool good, GaQueue& q, Uint32 t)
{
    const GaPanel p = ga_panel(g, w.fbw, w.fbh);
    const float s = (float)p.s;
    const Uint32 e = t - g.t0;
    switch (g.id) {
    case GA_OVERDRIVE: {
        const float cx = p.ax + p.aw * 0.5f, cy = p.ay + p.ah * 0.5f;
        if (good) {
            if (g.v >= 0.82f && g.v <= 0.92f) { q.click(t, g.mx, g.my); g_gaNextReact = t + 200; return; }
            const float phase = fmodf((float)e / 240.0f, 1.0f);
            const float x = cx + (phase < 0.5f ? -60 + 240 * phase : 180 - 240 * phase) * s;
            q.at(t, GA_EV_MOVE, x, cy);
        } else {
            q.at(t, GA_EV_MOVE, p.ax + fmodf((float)e * 0.2f, p.aw), cy);
            if (e > 1000 && e < 1020) q.click(t, g.mx, g.my);
        }
        break;
    }
    case GA_HUNTERS_EYE:
        if (t < g_gaNextReact) return;
        g_gaNextReact = t + 50;
        if (good) {
            if (fabsf(g.v - g.nx[0]) <= 2.5f) { q.click(t, p.ax + p.aw * 0.5f, p.ay + p.ah * 0.5f); g_gaNextReact = t + 300; }
            else q.at(t, GA_EV_WHEEL, g.mx, g.my, 0, g.nx[0] > g.v ? 1 : -1);
        } else if (e == 300 || (e > 300 && e % 500 < 50)) {
            q.click(t, p.ax + p.aw * 0.5f, p.ay + p.ah * 0.5f);
        }
        break;
    case GA_HUMILIATION:
        if (g.phase != 1 || t < g_gaNextReact) return;
        if (good ? fabsf(ga_golf_needle(g) - g.nx[0]) <= 0.02f : t - g.mark >= 200) {
            q.click(t, p.ax + p.aw * 0.5f, p.ay + p.ah * 0.5f);
            g_gaNextReact = t + 400;
        }
        break;
    case GA_MIRROR:
        if (g.phase != 1 || t < g_gaNextReact) return;
        g_gaNextReact = t + 100000;
        {
            const float hx = p.ax + p.aw * 0.5f, hw = p.aw * 0.5f;
            std::vector<GaPt> pts;
            if (good) {
                const std::vector<GaPt> sh = ga_resample_n(g.shape, 40);
                for (size_t i = 0; i < sh.size(); ++i)
                    pts.push_back({hx + (1.0f - sh[i].x) * hw + ((i % 3) - 1.0f) * s, p.ay + sh[i].y * p.ah, 0});
                q.stroke(t + 50, pts, 600);
            } else {
                for (const GaPt& r : g.shape) pts.push_back({hx + r.x * hw, p.ay + r.y * p.ah, 0});
                q.stroke(t + 50, pts, 500);
                std::vector<GaPt> line = {{hx + hw * 0.1f, p.ay + p.ah * 0.5f, 0}, {hx + hw * 0.9f, p.ay + p.ah * 0.55f, 0}};
                q.stroke(t + 800, ga_resample(line, 6 * s), 300);
            }
        }
        break;
    default:
        break;
    }
}

/* Plays the open game from its clock forward, in 16 ms steps, until it ends, the limit
   passes, or `stop_ms` of the game's own time has gone by (0: no stop). The world is
   rebuilt each step by `world`, so a live run follows the camera. */
static void ga_drive(GaGame& g, const std::function<GaWorld()>& world, bool good, Uint32 stop_ms)
{
    GaWorld w = world();
    GaQueue q;
    g_gaNextReact = 0;
    ga_auto_plan(g, w, good, q);
    std::stable_sort(q.ev.begin(), q.ev.end(), [](const GaEv& a, const GaEv& b) { return a.t < b.t; });
    size_t next = 0;
    /* resuming a game a partial run stopped: what was already played is not played twice */
    if (g.now != g.t0)
        while (next < q.ev.size() && q.ev[next].t <= g.now) ++next;
    for (Uint32 t = g.now; g.result == GA_RUNNING; t += 16) {
        if (stop_ms && t - g.t0 >= stop_ms) break;
        if (t - g.t0 > g.limit + 400) break;
        w = world();
        GaQueue react;
        ga_auto_react(g, w, good, react, t);
        if (!react.ev.empty()) {
            q.ev.insert(q.ev.end(), react.ev.begin(), react.ev.end());
            std::stable_sort(q.ev.begin() + next, q.ev.end(), [](const GaEv& a, const GaEv& b) { return a.t < b.t; });
        }
        while (next < q.ev.size() && q.ev[next].t <= t && g.result == GA_RUNNING) {
            GaEv ev = q.ev[next++];
            ev.t = std::max(ev.t, g.now);
            ga_event(g, w, ev);
        }
        ga_tick(g, w, t);
    }
}

/* ---- the self test's world ------------------------------------------------------------ */

static GaWorld ga_world_fake(int fbw, int fbh)
{
    GaWorld w;
    w.fbw = fbw; w.fbh = fbh;
    w.mx = fbw * 0.5f; w.my = fbh * 0.3f;
    w.unit = true;
    w.uwx = 30.5f; w.uwz = 30.5f; w.uex = 30.5f; w.uez = 30.5f; w.ucx = 30; w.ucy = 30;
    snprintf(w.utype, sizeof w.utype, "MTNK");
    const float ox = fbw * 0.5f, oy = fbh * 0.55f, cell = 40.0f;
    w.ux = ox; w.uy = oy;
    w.to_screen = [=](float wx, float wz, float* x, float* y) {
        *x = ox + (wx - 30.5f) * cell; *y = oy + (wz - 30.5f) * cell; return true;
    };
    w.to_world = [=](float x, float y, float* wx, float* wz) {
        *wx = 30.5f + (x - ox) / cell; *wz = 30.5f + (y - oy) / cell; return true;
    };
    w.on_map = [](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < 64 && cy < 64; };
    const float at[3][2] = {{34.5f, 30.5f}, {36.5f, 27.5f}, {41.5f, 33.5f}};
    for (int i = 0; i < 3; ++i) {
        GaTarget t;
        t.wx = t.ex = at[i][0]; t.wz = t.ez = at[i][1];
        w.to_screen(t.wx, t.wz, &t.sx, &t.sy);
        t.house = 5 + i; t.id = 10 + i;
        t.level = 2; t.potato = false;
        snprintf(t.type, sizeof t.type, "LTNK");
        w.enemies.push_back(t);
    }
    std::vector<GaTarget> enemies = w.enemies;
    const float pick = 20.0f * ga_scale(fbw, fbh);
    w.pick_enemy = [enemies, pick](float x, float y) {
        int best = -1;
        float bestd = pick;
        for (size_t i = 0; i < enemies.size(); ++i) {
            const float d = ga_dist(x, y, enemies[i].sx, enemies[i].sy);
            if (d <= bestd) { bestd = d; best = (int)i; }
        }
        return best;
    };
    return w;
}

/* Every recogniser, against a correct and a wrong gesture on the synthetic map, each on
   twelve seeds (the random parts: nodes, notch, station, wires, cards, arrows, squiggle).
   A good run must end DONE with a sensible target or cells on every seed; a bad run must
   not end DONE on any. */
static const unsigned GA_SELFTEST_SEEDS = 12;
static int ga_selftest()
{
    int pass = 0;
    const int fbw = 1280, fbh = 720;
    for (int id = 1; id < GA_COUNT; ++id) {
        bool ok[2] = {true, true};
        char note[2][112];
        int fails[2] = {0, 0};
        for (int pass_kind = 0; pass_kind < 2; ++pass_kind)
        for (unsigned seed = 0; seed < GA_SELFTEST_SEEDS; ++seed) {
            const bool good = pass_kind == 0;
            GaGame g;
            const GaWorld w0 = ga_world_fake(fbw, fbh);
            ga_begin(g, id, w0, 100000, 1234u + id * 7u + seed * 7919u);
            ga_drive(g, [&]() { return ga_world_fake(fbw, fbh); }, good, 0);
            bool sane = true;
            if (good && g.result == GA_DONE) {
                switch (id) {
                    case GA_CHRONO_SWAP: case GA_HUMILIATION: case GA_HOT_POTATO: case GA_MIRROR:
                        sane = g.house >= 0; break;
                    case GA_CHRONO_HOP: case GA_VORTEX: sane = g.ncells == 1; break;
                    case GA_MINEFIELD: sane = g.ncells == 5; break;
                    case GA_SMOKE: sane = g.ncells >= 6; break;
                    case GA_WORMHOLE: sane = g.ncells == 2; break;
                    default: break;
                }
            }
            const bool held = good ? (g.result == GA_DONE && sane) : (g.result != GA_DONE);
            if (!held) ++fails[pass_kind];
            if (seed && held && ok[pass_kind]) continue;   /* the note keeps the first, or the first failure */
            if (!held) ok[pass_kind] = false;
            snprintf(note[pass_kind], sizeof note[pass_kind], "%s:%s%s%s%s",
                     g.result == GA_DONE ? "done" : g.result == GA_FAILED ? "failed" : g.result == GA_CANCELLED ? "cancelled" : "running",
                     g.why, g.flash[0] ? " (last: " : "", g.flash, g.flash[0] ? ")" : "");
        }
        pass += ok[0] + ok[1];
        printf("GAQA|%d|good=%s|bad=%s|%s|seeds=%u|good_fails=%d|bad_fails=%d|good:%s|bad:%s\n", id, ok[0] ? "PASS" : "FAIL",
               ok[1] ? "PASS" : "FAIL", GA_GAME[id], GA_SELFTEST_SEEDS, fails[0], fails[1], note[0], note[1]);
    }
    printf("GAQA|summary|pass=%d/%d\n", pass, 2 * (GA_COUNT - 1));
    fflush(stdout);
    return 2 * (GA_COUNT - 1) - pass;
}

/* ---- the live adapter ------------------------------------------------------------------ */

static bool ga_combatant(const SimObject& o)
{
    return (o.kind == K_INFANTRY || o.kind == K_UNIT || o.kind == K_AIRCRAFT)
        && !o.limbo && o.str > 0 && !o.dying;
}

static void ga_screen_of(const SimObject& o, int fbw, int fbh, float* x, float* y)
{
    world_to_screen(o.wx, terrain_y(o.wx, o.wz) + alt_lift(o) + 0.25f, o.wz, fbw, fbh, x, y);
}

/* A player's standing level, 1 for the first rank, read where the scoreboard reads it; 0
   when this renderer knows no seat for the house. */
static int ga_house_level(const char* house)
{
    if (g_gaFake || !BrainGunStanding) return 0;
    const auto seat = g_gunSeatOfHouse.find(house);
    if (seat == g_gunSeatOfHouse.end()) return 0;
    for (int r = 0; r < g_matchRows && r < 8; ++r) {
        if (g_matchSeat[r] != seat->second) continue;
        int level = 0;
        unsigned kills = 0;
        bool alive = false;
        return BrainGunStanding(g_matchId[r], &level, &kills, &alive) ? level : 0;
    }
    return 0;
}

static GaWorld ga_world_live(int fbw, int fbh)
{
    GaWorld w;
    w.fbw = fbw; w.fbh = fbh;
    w.mx = g_mouseScrC; w.my = g_mouseScrR;
    if (w.mx < 0 || w.my < 0) { w.mx = fbw * 0.5f; w.my = fbh * 0.5f; }
    w.unit = false;
    w.ux = w.uy = w.uwx = w.uwz = w.uex = w.uez = 0; w.ucx = w.ucy = 0; w.utype[0] = 0;
    if (const SimObject* me = ga_own_unit()) {
        w.unit = true;
        ga_screen_of(*me, fbw, fbh, &w.ux, &w.uy);
        w.uwx = me->wx; w.uwz = me->wz;
        w.uex = me->ex; w.uez = me->ez;
        w.ucx = me->cx; w.ucy = me->cy;
        snprintf(w.utype, sizeof w.utype, "%s", me->type);
    }
    /* an enemy is a target when it is in view, or when the player's Hunter's Eye marks
       it through the fog (GFX_REVEAL) */
    const std::vector<GunEffect>& fx = ga_effects();
    const auto revealed = [&fx](const SimObject& o) {
        for (const GunEffect& e : fx)
            if (e.kind == GFX_REVEAL && fabsf(e.x / 256.0f - o.wx) < 0.75f && fabsf(e.y / 256.0f - o.wz) < 0.75f) return true;
        return false;
    };
    for (const SimObject& o : g_objects) {
        if (!ga_combatant(o) || ga_own(o) || !strcmp(o.house, "Neutral")) continue;
        if (o.gahide || o.limbo) continue;
        if (!visible(o) && !revealed(o)) continue;
        if (ga_house_number(o.house) < 0) continue;
        GaTarget t;
        ga_screen_of(o, fbw, fbh, &t.sx, &t.sy);
        if (t.sx < 0 || t.sy < 0 || t.sx >= fbw || t.sy >= fbh) continue;
        t.wx = o.wx; t.wz = o.wz;
        t.ex = o.ex; t.ez = o.ez;
        t.house = ga_house_number(o.house);
        t.id = o.id;
        t.level = ga_house_level(o.house);
        t.potato = (o.gaf & GAF_HOT_POTATO) != 0;
        snprintf(t.type, sizeof t.type, "%s", o.type);
        w.enemies.push_back(t);
    }
    w.to_world = [fbw, fbh](float x, float y, float* wx, float* wz) { return screen_to_world(x, y, fbw, fbh, wx, wz); };
    w.to_screen = [fbw, fbh](float wx, float wz, float* x, float* y) {
        world_to_screen(wx, terrain_y(wx, wz), wz, fbw, fbh, x, y);
        return true;
    };
    w.on_map = [](int cx, int cy) {
        return cx >= g_mapX && cy >= g_mapY && cx < g_mapX + g_mapW && cy < g_mapY + g_mapH;
    };
    std::vector<GaTarget> enemies = w.enemies;
    const float reach = 22.0f * ga_scale(fbw, fbh);
    w.pick_enemy = [enemies, fbw, fbh, reach](float x, float y) {
        const PickInfo pi = pick_at(x, y, fbw, fbh);
        if (pi.index >= 0) {
            const SimObject& o = g_objects[pi.index];
            for (size_t i = 0; i < enemies.size(); ++i)
                if (enemies[i].id == o.id && !strcmp(enemies[i].type, o.type)) return (int)i;
        }
        int best = -1;
        float bestd = reach;
        for (size_t i = 0; i < enemies.size(); ++i) {
            const float d = ga_dist(x, y, enemies[i].sx, enemies[i].sy);
            if (d <= bestd) { bestd = d; best = (int)i; }
        }
        return best;
    };
    return w;
}

/* ---- the live controller --------------------------------------------------------------- */

struct GaHud {
    bool open;
    GaGame g;
    Uint32 blocked_until;
    /* THE LAST ABILITY HANDED TO THE ENGINE, until the engine answers. Use only queues
       it: the engine checks every rule again when the lockstep event runs and may keep it
       then (the target dodged or moved away, nowhere to land, no room). Held is the
       answer. While it is pending the cameo says SENT and the same try is not offered
       again; the success sound waits for the ability to leave Held. sent_frame: the
       engine frame it was sent on, which the wait is counted in, so a peer that holds
       the match back does not run the clock out. */
    bool pending;
    int sent_id, sent_frame;
    char toast[64];
    Uint32 toast_at;
    bool toast_bad;
    int beeps;
    /* the mouse buttons the open game has seen go down: a release it never saw the press
       of (the click on the cameo that opened it) is not the game's */
    unsigned buttons;
    /* the combatant the game was opened for; another one (a death, a respawn) ends it */
    int unit_kind, unit_id;
};
static GaHud g_ga;

/* How long a sent activation may wait for its event, in seconds of game time. */
static const int GA_SENT_SECONDS = 3;

static bool ga_open() { return g_ga.open; }
static bool ga_takes_keys() { return g_ga.open && g_ga.g.id == GA_IRON_CURTAIN; }

static void ga_sound(const char* name)
{
    if (g_au) cnc_audio_play_named(g_au, name, MIX_BUS_FX, MIX_UNITY, 0, 200);
}

static void ga_toast(const char* text, bool bad, Uint32 now)
{
    snprintf(g_ga.toast, sizeof g_ga.toast, "%s", text);
    g_ga.toast_at = now;
    g_ga.toast_bad = bad;
}

static bool ga_use_now(const GaGame& g)
{
    bool ok;
    if (g_gaFake) {
        ok = g_gaFakeHeld == g.id;
        if (ok) {
            g_gaFakeHeld = GA_NONE;
            GunEffect a = {};
            a.kind = GFX_ANNOUNCE; a.owner = ga_house_number(g_playerHouse); a.value = g.id;
            a.age = -g_engineFrame - 1;   /* aged from this frame by ga_effects */
            g_gaFakeFx.push_back(a);
        }
    } else {
        ok = BrainGaUse && BrainGaUse(0, g.id, g.house, g.ncells, g.ncells ? g.cells : nullptr);
    }
    printf("GAHUD|use|id=%d|name=%s|target=%d|cells=%d", g.id, GA_NAME[g.id], g.house, g.ncells);
    for (int i = 0; i < g.ncells; ++i) printf("%s%d,%d", i ? ";" : "|", g.cells[2 * i], g.cells[2 * i + 1]);
    printf("|%s\n", ok ? "OK" : "REFUSED");
    fflush(stdout);
    return ok;
}

/* The live world, built once per drawn frame and once per object dump rather than for
   every pointer move: ga_frame, the panel and the events of one frame share it. */
static GaWorld g_gaWorld;
static unsigned g_gaWorldSerial = ~0u, g_gaWorldDump = ~0u;
static int g_gaWorldW = 0, g_gaWorldH = 0;

static const GaWorld& ga_world_frame(int fbw, int fbh)
{
    if (g_gaWorldSerial != g_drawFrameSerial || g_gaWorldDump != g_gaDumps || g_gaWorldW != fbw || g_gaWorldH != fbh) {
        g_gaWorld = ga_world_live(fbw, fbh);
        g_gaWorldSerial = g_drawFrameSerial;
        g_gaWorldDump = g_gaDumps;
        g_gaWorldW = fbw;
        g_gaWorldH = fbh;
    }
    return g_gaWorld;
}

/* A game has ended one way or another: hand a success to the engine, block a retry on
   anything else. */
static void ga_settle(Uint32 now)
{
    if (!g_ga.open || g_ga.g.result == GA_RUNNING) return;
    GaGame& g = g_ga.g;
    g_ga.open = false;
    if (g.result == GA_DONE) {
        printf("GAHUD|success|id=%d|name=%s|game=%s|ms=%u\n", g.id, GA_NAME[g.id], GA_GAME[g.id], g.now - g.t0);
        if (ga_use_now(g)) {
            /* sent, not done: the sound and the name wait for the engine (ga_poll_sent) */
            g_ga.pending = true;
            g_ga.sent_id = g.id;
            g_ga.sent_frame = g_engineFrame;
            return;
        }
        snprintf(g.why, sizeof g.why, "REFUSED");
    }
    const bool cancelled = g.result == GA_CANCELLED;
    printf("GAHUD|%s|id=%d|name=%s|why=%s|retry_ms=%d\n", cancelled ? "cancel" : "fail", g.id, GA_NAME[g.id], g.why, GA_RETRY_MS);
    fflush(stdout);
    g_ga.blocked_until = now + GA_RETRY_MS;
    if (!cancelled) ga_sound("SCOLD2.AUD");
    char text[64];
    snprintf(text, sizeof text, "%s: %s", GA_GAME[g.id], cancelled ? "CANCELLED" : g.why);
    ga_toast(text, true, now);
}

/* A GAME THE PLAYER DID NOT END: the match is over, the ability was replaced, the
   combatant died or was frozen, or a dialog took the screen. It closes with no retry
   pause and no verdict, because nothing was tried. */
static void ga_close_quiet(const char* why)
{
    if (!g_ga.open) return;
    g_ga.open = false;
    printf("GAHUD|close|id=%d|name=%s|why=%s\n", g_ga.g.id, GA_NAME[g_ga.g.id], why);
    fflush(stdout);
}

/* THE ENGINE'S ANSWER TO A SENT ACTIVATION. Taken: Held is empty, or the engine
   announced this player using it after it was sent (a crate may have handed over another
   ability, or the same one, since). Kept: neither, after GA_SENT_SECONDS of game time;
   the engine refused it when the event ran. */
static void ga_poll_sent(Uint32 now)
{
    if (!g_ga.pending) return;
    const int id = g_ga.sent_id;
    bool taken = ga_held() == GA_NONE;
    if (!taken) {
        const int me = ga_house_number(g_playerHouse);
        for (const GunEffect& e : ga_effects())
            if (e.kind == GFX_ANNOUNCE && e.owner == me && e.value == id && g_engineFrame - e.age >= g_ga.sent_frame) taken = true;
    }
    char text[64];
    if (taken) {
        g_ga.pending = false;
        ga_sound("TONE15.AUD");
        snprintf(text, sizeof text, "%s!", GA_NAME[id]);
        ga_toast(text, false, now);
        printf("GAHUD|taken|id=%d|name=%s|frames=%d\n", id, GA_NAME[id], g_engineFrame - g_ga.sent_frame);
        fflush(stdout);
        return;
    }
    ga_timers();
    if (g_engineFrame - g_ga.sent_frame <= GA_SENT_SECONDS * std::max(1, g_gaTps)) return;
    g_ga.pending = false;
    ga_sound("SCOLD2.AUD");
    snprintf(text, sizeof text, "%s: DID NOT TAKE", GA_NAME[id]);
    ga_toast(text, true, now);
    printf("GAHUD|kept|id=%d|name=%s|frames=%d\n", id, GA_NAME[id], g_engineFrame - g_ga.sent_frame);
    fflush(stdout);
}

/* Why a try cannot start or go on right now, or null. The engine refuses any activation
   while the combatant is dead or respawning, and while Dance Off holds it frozen. The
   stand-in (OPENCNC3D_GA_FAKE) has no engine to refuse and is never stopped here. */
static const char* ga_unable()
{
    if (g_gaFake) return nullptr;
    if (!ga_own_unit()) return "no unit";
    if (ga_timers()[GA_DANCE_OFF] > 0) return "frozen";
    return nullptr;
}

static bool ga_try_open(int fbw, int fbh, Uint32 now)
{
    if (g_ga.open) return true;
    if (!ga_feature_on()) return false;
    ga_poll_sent(now);
    const int held = ga_held();
    if (held == GA_NONE) { printf("GAHUD|open|refused|nothing held\n"); fflush(stdout); return false; }
    if (g_ga.pending && held == g_ga.sent_id) {
        printf("GAHUD|open|refused|already sent\n");
        fflush(stdout);
        return false;
    }
    if (now < g_ga.blocked_until) {
        printf("GAHUD|open|refused|retry in %u ms\n", g_ga.blocked_until - now);
        fflush(stdout);
        return false;
    }
    if (g_gunLaunch.active || sb_super_armed() || g_gameOver.valid) {
        printf("GAHUD|open|refused|busy\n");
        fflush(stdout);
        return false;
    }
    if (const char* why = ga_unable()) {
        printf("GAHUD|open|refused|%s\n", why);
        fflush(stdout);
        return false;
    }
    const GaWorld w = ga_world_live(fbw, fbh);
    ga_begin(g_ga.g, held, w, now, (unsigned)SDL_GetPerformanceCounter());
    g_ga.open = true;
    g_ga.beeps = 0;
    g_ga.buttons = 0;
    const SimObject* me = ga_own_unit();
    g_ga.unit_kind = me ? (int)me->kind : -1;
    g_ga.unit_id = me ? me->id : -1;
    printf("GAHUD|open|id=%d|name=%s|game=%s|limit_ms=%u\n", held, GA_NAME[held], GA_GAME[held], GA_LIMIT[held]);
    fflush(stdout);
    return true;
}

static void ga_cancel(Uint32 now)
{
    if (!g_ga.open) return;
    ga_end(g_ga.g, GA_CANCELLED, "CANCELLED");
    ga_settle(now);
}

/* Once a frame: the answer to a sent try, the clock, the heartbeat's beeps, and closing a
   game that cannot go on. */
static void ga_frame(bool sim_over, int fbw, int fbh)
{
    const Uint32 now = SDL_GetTicks();
    ga_poll_sent(now);
    if (!g_ga.open) return;
    if (sim_over || g_gameOver.valid) { ga_close_quiet("match over"); return; }
    if (ga_held() != g_ga.g.id) { ga_close_quiet("ability changed"); return; }
    if (g_optOpen || g_chatEntry || g_cxOpen) { ga_close_quiet("dialog"); return; }
    if (const char* why = ga_unable()) { ga_close_quiet(why); return; }
    if (!g_gaFake) {
        const SimObject* me = ga_own_unit();
        if (!me || (int)me->kind != g_ga.unit_kind || me->id != g_ga.unit_id) { ga_close_quiet("new unit"); return; }
    }
    const GaWorld& w = ga_world_frame(fbw, fbh);
    ga_tick(g_ga.g, w, now);
    if (g_ga.g.id == GA_EXTRA_LIFE)
        while (g_ga.beeps < 3 && g_ga.g.now - g_ga.g.t0 >= GA_BEAT[g_ga.beeps]) { ++g_ga.beeps; ga_sound("BLEEP2.AUD"); }
    ga_settle(now);
}

/* The live SDL event, already in framebuffer pixels. True when the game took it. */
static bool ga_input(int type, float x, float y, int button, int wheel, int key, bool repeat)
{
    if (!g_ga.open) return false;
    const Uint32 now = std::max(SDL_GetTicks(), g_ga.g.now);
    const bool keys = ga_takes_keys();
    if (type == GA_EV_KEY) {
        if (key == SDLK_ESCAPE) { ga_cancel(now); return true; }
        if (!keys) return false;
        if (repeat) return true;
    }
    const unsigned bit = button > 0 && button < 32 ? 1u << button : 0u;
    if (type == GA_EV_DOWN) g_ga.buttons |= bit;
    if (type == GA_EV_UP) {
        if (!(g_ga.buttons & bit)) return true;   /* pressed before the game opened */
        g_ga.buttons &= ~bit;
    }
    GaEv ev = {type, x, y, button, wheel, key, now};
    ga_event(g_ga.g, ga_world_frame(g_fbW, g_fbH), ev);
    ga_settle(now);
    return type != GA_EV_MOVE;
}

static void ga_auto(int fbw, int fbh, bool good, Uint32 stop_ms)
{
    if (!g_ga.open) { printf("GAHUD|auto|no game open\n"); fflush(stdout); return; }
    ga_drive(g_ga.g, [fbw, fbh]() { return ga_world_live(fbw, fbh); }, good, stop_ms);
    printf("GAHUD|auto|%s|id=%d|result=%d|phase=%d|ms=%u\n", good ? "good" : "bad", g_ga.g.id, g_ga.g.result,
           g_ga.g.phase, g_ga.g.now - g_ga.g.t0);
    fflush(stdout);
    if (!stop_ms || g_ga.g.result != GA_RUNNING) {
        if (g_ga.g.result == GA_RUNNING) ga_end(g_ga.g, GA_FAILED, "OUT OF TIME");
        ga_settle(SDL_GetTicks());
    }
}

/* Mirrored controls: a map click lands mirrored about the combatant's screen column. */
static float ga_mirror_col(float col, int fbw, int fbh)
{
    float ux, uy;
    const SimObject* me = ga_own_unit();
    if (!me) return col;
    ga_screen_of(*me, fbw, fbh, &ux, &uy);
    return std::max(0.0f, std::min((float)fbw - 1.0f, 2.0f * ux - col));
}

/* ---- the cameo ---------------------------------------------------------------------- */

struct GaCameo { int w, h; std::vector<unsigned char> rgba; };
static std::vector<GaCameo> g_gaCameo;
static bool g_gaCameoTried = false;
static GLuint g_gaCameoTex = 0;
static int g_gaCameoUp = -1;

/* gun_abilities.pack: "GABIL01\0", uint32 count, then per ability from GA_STEALTH:
   char name[16], uint32 w, uint32 h, w*h*4 RGBA. */
static void ga_cameo_load()
{
    if (g_gaCameoTried) return;
    g_gaCameoTried = true;
    /* OPENCNC3D_GA_CAMEOS names another copy, for a test run outside the play folder */
    const char* path = getenv("OPENCNC3D_GA_CAMEOS");
    if (!path || !*path) path = "gun_abilities.pack";
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "gun abilities: %s missing, cameos are placeholders\n", path); return; }
    unsigned char head[12];
    const auto u32 = [](const unsigned char* b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; };
    if (fread(head, 1, 12, f) != 12 || memcmp(head, "GABIL01\0", 8)) { fclose(f); fprintf(stderr, "gun abilities: gun_abilities.pack has the wrong header\n"); return; }
    const uint32_t count = u32(head + 8);
    for (uint32_t i = 0; i < count && i + 1 < GA_COUNT; ++i) {
        unsigned char e[24];
        if (fread(e, 1, 24, f) != 24) break;
        GaCameo c;
        c.w = (int)u32(e + 16); c.h = (int)u32(e + 20);
        if (c.w <= 0 || c.h <= 0 || c.w > 256 || c.h > 256) break;
        c.rgba.resize((size_t)c.w * c.h * 4);
        if (fread(c.rgba.data(), 1, c.rgba.size(), f) != c.rgba.size()) break;
        g_gaCameo.push_back(c);
    }
    fclose(f);
    fprintf(stderr, "gun abilities: %zu cameos from gun_abilities.pack\n", g_gaCameo.size());
}

static void ga_draw_cameo(int id, float x0, float y0, int s)
{
    ga_cameo_load();
    const float x1 = x0 + 64 * s, y1 = y0 + 48 * s;
    if (id >= 1 && id <= (int)g_gaCameo.size() && g_gaCameo[id - 1].w <= H6_POT_W && g_gaCameo[id - 1].h <= H6_POT_H) {
        const GaCameo& c = g_gaCameo[id - 1];
        if (g_gaCameoUp != id) { h6_upload(&g_gaCameoTex, c.rgba.data(), c.w, c.h); g_gaCameoUp = id; }
        glEnable(GL_TEXTURE_2D);
        glColor4f(1, 1, 1, 1);
        h6_tex_quad(g_gaCameoTex, c.w, c.h, x0, y0, x1, y1);
        glDisable(GL_TEXTURE_2D);
        return;
    }
    /* the placeholder: a dark well and the name on two lines */
    sb_rect(x0, y0, x1, y1, 0.07f, 0.065f, 0.055f, 1);
    sb_frame(x0, y0, x1, y1, 0.36f, 0.31f, 0.23f, 1);
    char a[24], b[24];
    snprintf(a, sizeof a, "%s", GA_SHORT[id]);
    b[0] = 0;
    if (char* sp = strchr(a, ' ')) { *sp = 0; snprintf(b, sizeof b, "%s", sp + 1); }
    const float cx = (x0 + x1) * 0.5f;
    if (b[0]) {
        ga_text_c(cx, y0 + 12 * s, a, s, 0.91f, 0.80f, 0.55f);
        ga_text_c(cx, y0 + 25 * s, b, s, 0.91f, 0.80f, 0.55f);
    } else {
        ga_text_c(cx, y0 + 18 * s, a, s, 0.91f, 0.80f, 0.55f);
    }
}

/* Beside the superweapon cameo, in the same frame. */
static void gun_game_draw_ability(const GunRewardButton& button)
{
    const Uint32 now = SDL_GetTicks();
    ga_poll_sent(now);
    const int held = ga_held();
    if (held == GA_NONE || g_gameOver.valid) return;
    const int x = button.x, y = button.y, s = button.scale;
    const int w = button.Width * s, h = button.Height * s;
    const bool waiting = now < g_ga.blocked_until;
    const bool unable = !g_ga.open && ga_unable();
    const bool blocked = waiting || unable;
    const bool sent = g_ga.pending && held == g_ga.sent_id;
    const bool hot = g_ga.open || button.contains(g_mouseScrC, g_mouseScrR);
    sb_rect(x, y, x + w, y + h, 0.15f, 0.13f, 0.10f, 0.97f);
    sb_frame(x, y, x + w, y + h, hot ? 1.0f : 0.67f, hot ? 0.80f : 0.55f, 0.33f, 1);
    ga_draw_cameo(held, (float)(x + 5 * s), (float)(y + 5 * s), s);
    char label[24];
    if (blocked) {
        sb_rect(x + 5 * s, y + 5 * s, x + 69 * s, y + 53 * s, 0, 0, 0, 0.55f);
        if (waiting) snprintf(label, sizeof label, "WAIT %.1f", (g_ga.blocked_until - now) / 1000.0f);
        else snprintf(label, sizeof label, "WAIT");
    } else {
        snprintf(label, sizeof label, "%s", g_ga.open ? "ACTIVE" : sent ? "SENT" : "E  USE");
    }
    sb_text(x + (w - sb_text_w(label, s)) / 2, y + 57 * s, label, s, blocked ? 0.75f : 0.95f, blocked ? 0.7f : 0.86f, blocked ? 0.62f : 0.57f);
}

/* The local player's running effects, one short line each, under the cameos. */
static void gun_game_draw_ability_timers(const GunHudLayout& l, int y)
{
    const int32_t* t = ga_timers();
    const int s = l.scale;
    int row = 0;
    for (int a = 1; a < GA_COUNT; ++a) {
        if (t[a] <= 0) continue;
        char line[40];
        snprintf(line, sizeof line, "%s %d", GA_NAME[a], (int)((t[a] + g_gaTps - 1) / g_gaTps));
        const float tw = sb_text_w(line, s);
        const float x = l.radar_x + l.radar_w * s - tw - 2 * s, yy = y + row * 13 * s;
        sb_rect(x - 4 * s, yy - 2 * s, x + tw + 2 * s, yy + 11 * s, 0.094f, 0.09f, 0.075f, 0.85f);
        sb_text(x, yy, line, s, a == GA_MIRROR ? 0.96f : 0.62f, a == GA_MIRROR ? 0.45f : 0.86f, a == GA_MIRROR ? 0.4f : 0.62f);
        if (++row == 6) break;
    }
}

/* ---- the effects, in the world --------------------------------------------------------- */

static void ga_world_pt(const GunEffect& e, bool second, float* wx, float* wz)
{
    *wx = (second ? e.x2 : e.x) / 256.0f;
    *wz = (second ? e.y2 : e.y) / 256.0f;
}

static void ga_seat_name_of_house(int house, char* out, size_t n)
{
    char name[16];
    ga_house_name(house, name, sizeof name);
    const auto seat = g_gunSeatOfHouse.find(name);
    if (seat != g_gunSeatOfHouse.end()) snprintf(out, n, "%s", gun_hud_name(seat->second));
    else if (house == ga_house_number(g_playerHouse)) snprintf(out, n, "YOU");
    else snprintf(out, n, "%s", name);
}

static void ga_bomb_icon(float x, float y, float fs, float pulse)
{
    ga_disc(x, y, 7.0f * fs, 0.10f, 0.09f, 0.08f, 1);
    ga_ring(x, y, 7.0f * fs, 1.0f * fs, 0.95f, 0.55f, 0.30f, 1);
    ga_quad_line(x + 4 * fs, y - 5 * fs, x + 7 * fs, y - 9 * fs, 2.0f * fs, 0.85f, 0.75f, 0.45f, 1);
    ga_disc(x + 7 * fs, y - 9 * fs, (1.5f + 1.5f * pulse) * fs, 1.0f, 0.7f, 0.2f, 1);
}

static std::map<int, int> g_gaAnnounceSeen;   /* owner * 64 + ability -> engine frame of use */

/* WHOSE BOMB IS WHOSE. The engine puts a GFX_POTATO on the real carrier only; each is
   matched to the player whose unit stands there, so the bomb and its count can be drawn
   on every unit of that player, decoys included. Effects matched to nobody are listed in
   `loose` and drawn where they are. */
static std::map<std::string, int> ga_potato_ticks(const std::vector<GunEffect>& fx, std::vector<const GunEffect*>* loose)
{
    std::map<std::string, int> ticks;
    for (const GunEffect& e : fx) {
        if (e.kind != GFX_POTATO) continue;
        const SimObject* at = nullptr;
        float best = 1.0f;
        for (const SimObject& o : g_objects) {
            if (!ga_unitlike(o) || o.gahide || !(o.gaf & GAF_HOT_POTATO)) continue;
            const float d = hypotf(o.ex - e.x / 256.0f, o.ez - e.y / 256.0f);
            if (d < best) { best = d; at = &o; }
        }
        if (at) ticks[at->house] = std::max(ticks[at->house], e.value);
        else if (loose) loose->push_back(&e);
    }
    return ticks;
}

/* The world's projection alone, for drawing: the effects need no enemy list. */
static GaWorld ga_world_view(int fbw, int fbh)
{
    GaWorld w;
    w.fbw = fbw; w.fbh = fbh;
    w.to_screen = [fbw, fbh](float wx, float wz, float* x, float* y) {
        world_to_screen(wx, terrain_y(wx, wz), wz, fbw, fbh, x, y);
        return true;
    };
    return w;
}

static void ga_draw_world(int fbw, int fbh)
{
    if (!g_sb.GunPlayers && !g_gaFake) return;
    if (g_gameOver.valid) return;
    const std::vector<GunEffect>& fx = ga_effects();
    bool flags = false;
    for (const SimObject& o : g_objects) if (o.gaf) { flags = true; break; }
    if (fx.empty() && !flags) return;
    const GaWorld w = ga_world_view(fbw, fbh);
    const float fs = (float)ga_scale(fbw, fbh);
    const float clock = (float)SDL_GetTicks() * 0.001f;
    const float pulse = 0.5f + 0.5f * sinf(clock * 7.0f);
    ga_timers();
    const int tps = std::max(1, g_gaTps);

    begin_overlay(fbw, fbh);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* smoke first, so everything else reads over it */
    for (const GunEffect& e : fx) {
        if (e.kind != GFX_SMOKE) continue;
        float wx, wz;
        ga_world_pt(e, false, &wx, &wz);
        const float fade = std::min(1.0f, e.value / (float)tps);
        for (int k = 0; k < 5; ++k) {
            const float a = clock * 0.3f + k * 1.3f + (e.x % 97);
            const float ox = cosf(a) * 0.28f, oz = sinf(a * 1.2f) * 0.28f;
            float x, y, x2, y2;
            world_to_screen(wx + ox, terrain_y(wx, wz) + 0.35f + 0.08f * k, wz + oz, fbw, fbh, &x, &y);
            world_to_screen(wx + ox + 0.55f, terrain_y(wx, wz) + 0.35f, wz + oz, fbw, fbh, &x2, &y2);
            const float r = std::max(6.0f, fabsf(x2 - x) + fabsf(y2 - y));
            const float g = 0.62f + 0.06f * k;
            ga_disc(x, y, r, g, g, g * 0.98f, 0.30f * fade);
        }
    }

    for (const GunEffect& e : fx) {
        float wx, wz, x, y;
        ga_world_pt(e, false, &wx, &wz);
        w.to_screen(wx, wz, &x, &y);
        switch (e.kind) {
        case GFX_VORTEX:
            for (int arm = 0; arm < 3; ++arm) {
                float px = 0, py = 0;
                for (int i = 0; i <= 24; ++i) {
                    const float f = i / 24.0f;
                    const float a = clock * 3.0f + arm * 2.094f + f * 5.0f;
                    float sx, sy;
                    w.to_screen(wx + cosf(a) * f * 1.6f, wz + sinf(a) * f * 1.6f, &sx, &sy);
                    if (i) ga_quad_line(px, py, sx, sy, (1.0f + 2.0f * (1 - f)) * fs, 0.55f + 0.3f * f, 0.35f, 1.0f, 0.85f * (1.0f - f * 0.6f));
                    px = sx; py = sy;
                }
            }
            ga_disc(x, y, 4.0f * fs, 0.85f, 0.75f, 1.0f, 0.9f);
            break;
        case GFX_WORMHOLE: {
            float wx2, wz2, x2, y2;
            ga_world_pt(e, true, &wx2, &wz2);
            w.to_screen(wx2, wz2, &x2, &y2);
            for (int end = 0; end < 2; ++end) {
                const float cx = end ? wx2 : wx, cz = end ? wz2 : wz;
                for (int k = 0; k < 3; ++k)
                    ga_ground_ring(w, cx, cz, 0.35f + 0.18f * k, (2.0f - 0.5f * k) * fs, end ? 0.35f : 0.75f, 0.45f, 1.0f,
                                   0.9f - 0.25f * k, (end ? -clock : clock) * (1.5f + k), true);
            }
            for (int i = 0; i < 24; i += 2) {
                const float f0 = i / 24.0f, f1 = (i + 1) / 24.0f;
                const float lift = 40.0f * fs;
                const float ax0 = x + (x2 - x) * f0, ay0 = y + (y2 - y) * f0 - sinf(f0 * (float)M_PI) * lift;
                const float ax1 = x + (x2 - x) * f1, ay1 = y + (y2 - y) * f1 - sinf(f1 * (float)M_PI) * lift;
                ga_quad_line(ax0, ay0, ax1, ay1, 1.0f * fs, 0.65f, 0.5f, 1.0f, 0.35f);
            }
            break;
        }
        case GFX_MINE:
            ga_disc(x, y, 5.0f * fs, 0.25f, 0.22f, 0.18f, 0.9f);
            ga_ring(x, y, 5.0f * fs, 1.0f * fs, 0.95f, 0.35f, 0.25f, 0.6f + 0.4f * pulse);
            break;
        case GFX_BOOBY:
            sb_rect(x - 7 * fs, y - 7 * fs, x + 7 * fs, y + 5 * fs, 0.55f, 0.42f, 0.22f, 0.95f);
            sb_frame(x - 7 * fs, y - 7 * fs, x + 7 * fs, y + 5 * fs, 0.95f, 0.35f, 0.25f, 1);
            ga_text_c(x, y - 6 * fs, "!", (int)fs, 1, 0.4f, 0.3f);
            break;
        case GFX_ARC: {
            if (e.age > 8) break;
            float wx2, wz2, x2, y2;
            ga_world_pt(e, true, &wx2, &wz2);
            /* an arc that starts or ends in smoke this viewer is outside of would draw a
               line to a unit the smoke hides */
            if (ga_smoke_hides((int)floorf(wx), (int)floorf(wz), false)
                || ga_smoke_hides((int)floorf(wx2), (int)floorf(wz2), false)) break;
            w.to_screen(wx2, wz2, &x2, &y2);
            y -= 10 * fs; y2 -= 10 * fs;
            std::mt19937 n((unsigned)(e.x * 31 + e.y2 + SDL_GetTicks() / 60));
            const float a = 1.0f - e.age / 9.0f;
            float px = x, py = y;
            for (int i = 1; i <= 8; ++i) {
                const float f = i / 8.0f;
                float qx = x + (x2 - x) * f, qy = y + (y2 - y) * f;
                if (i < 8) { qx += ((int)(n() % 21) - 10) * fs; qy += ((int)(n() % 21) - 10) * fs; }
                ga_quad_line(px, py, qx, qy, 4.0f * fs, 0.45f, 0.65f, 1.0f, 0.35f * a);
                ga_quad_line(px, py, qx, qy, 1.5f * fs, 0.9f, 0.95f, 1.0f, a);
                px = qx; py = qy;
            }
            break;
        }
        case GFX_POTATO: break;   /* below, on every unit of the carrier */
        case GFX_REVEAL: {
            const bool leader = e.value == 1;
            const float r = (leader ? 9.0f : 7.0f) * fs;
            ga_quad_line(x - r, y, x, y - r, 2 * fs, 1.0f, 0.3f, 0.25f, 0.9f);
            ga_quad_line(x, y - r, x + r, y, 2 * fs, 1.0f, 0.3f, 0.25f, 0.9f);
            ga_quad_line(x + r, y, x, y + r, 2 * fs, 1.0f, 0.3f, 0.25f, 0.9f);
            ga_quad_line(x, y + r, x - r, y, 2 * fs, 1.0f, 0.3f, 0.25f, 0.9f);
            if (leader) ga_text_c(x, y - r - 13 * fs, "LEADER", (int)fs, 1.0f, 0.85f, 0.4f);
            break;
        }
        case GFX_BLAST: {
            if (e.age > 12) break;
            const float f = e.age / 12.0f;
            ga_ground_ring(w, wx, wz, 0.5f + 2.5f * f, 3.0f * fs * (1 - f) + fs, 1.0f, 0.65f - 0.3f * f, 0.25f, 1.0f - f);
            ga_disc(x, y, (14.0f + 30.0f * f) * fs, 1.0f, 0.8f, 0.4f, 0.4f * (1 - f));
            break;
        }
        default:
            break;
        }
    }

    /* the bomb: over every unit of the player carrying it, so it does not pick out the
       real one among decoys, whether that unit is in view or in fog, as the engine
       reports it; and where the engine put it when no unit stands there */
    std::vector<const GunEffect*> loose;
    const std::map<std::string, int> potato = ga_potato_ticks(fx, &loose);
    const auto bomb = [&](float x, float y, int ticks) {
        const float bx = x, by = y - 34 * fs;
        ga_bomb_icon(bx, by, fs, pulse);
        char sec[8];
        snprintf(sec, sizeof sec, "%d", (ticks + tps - 1) / tps);
        sb_rect(bx + 9 * fs, by - 6 * fs, bx + 11 * fs + sb_text_w(sec, fs), by + 6 * fs, 0.094f, 0.09f, 0.075f, 0.85f);
        sb_text(bx + 10 * fs, by - 5 * fs, sec, fs, 1.0f, 0.55f, 0.35f);
    };
    for (const GunEffect* e : loose) {
        float x, y;
        w.to_screen(e->x / 256.0f, e->y / 256.0f, &x, &y);
        bomb(x, y, e->value);
    }
    for (const SimObject& o : g_objects) {
        if (!ga_combatant(o) || o.gahide || !(o.gaf & GAF_HOT_POTATO)) continue;
        const auto it = potato.find(o.house);
        if (it == potato.end()) continue;
        float x, y;
        ga_screen_of(o, fbw, fbh, &x, &y);
        bomb(x, y, it->second);
    }

    /* what the flags say about each drawn combatant */
    for (const SimObject& o : g_objects) {
        if (!o.gaf || !ga_combatant(o) || !visible(o)) continue;
        float x, y;
        ga_screen_of(o, fbw, fbh, &x, &y);
        const unsigned f = o.gaf;
        if ((f & GAF_STEALTH) && ga_own(o))
            ga_ring(x, y, (12.0f + 2.0f * pulse) * fs, 1.0f * fs, 0.55f, 0.85f, 1.0f, 0.55f, 24);
        if (f & GAF_OVERDRIVE) {
            const float a = (o.face >= 0 ? o.face : 0) * (float)M_PI / 128.0f;
            float bx, by;
            world_to_screen(o.wx - sinf(a) * 0.6f, terrain_y(o.wx, o.wz) + 0.2f, o.wz + cosf(a) * 0.6f, fbw, fbh, &bx, &by);
            for (int k = -1; k <= 1; ++k)
                ga_quad_line(x + k * 4 * fs, y + 6 * fs, bx + k * 5 * fs, by + 6 * fs, 1.5f * fs, 1.0f, 0.6f, 0.2f, 0.35f + 0.3f * pulse);
        }
        if (f & GAF_CHAIN) {
            std::mt19937 n((unsigned)(o.id * 977 + SDL_GetTicks() / 80));
            for (int k = 0; k < 3; ++k) {
                const float a = (n() % 628) / 100.0f, r0 = 9 * fs, r1 = 15 * fs;
                ga_quad_line(x + cosf(a) * r0, y + sinf(a) * r0, x + cosf(a + 0.3f) * r1, y + sinf(a + 0.3f) * r1, 1.5f * fs, 0.6f, 0.8f, 1.0f, 0.8f);
            }
        }
        if (f & GAF_DANCING) {
            const float hop = fabsf(sinf(clock * 8.0f)) * 6 * fs;
            ga_text_c(x + 10 * fs, y - 24 * fs - hop, "~", (int)fs, 1.0f, 0.85f, 0.5f);
        }
        if (f & GAF_MIRRORED) ga_text_c(x, y - 26 * fs, "<>", (int)fs, 0.96f, 0.45f, 0.40f);
        if ((f & GAF_DECOY) && ga_own(o)) ga_ring(x, y, 12 * fs, 1.0f * fs, 0.85f, 0.85f, 0.55f, 0.5f, 18);
        if ((f & GAF_EXTRA_LIFE) && ga_own(o)) ga_text_c(x - 14 * fs, y - 22 * fs, "+1", (int)fs, 0.45f, 0.95f, 0.45f);
        if ((f & GAF_LAST_LAUGH) && ga_own(o)) ga_text_c(x + 14 * fs, y - 22 * fs, "!", (int)fs, 1.0f, 0.4f, 0.3f);
        if (f & GAF_IRON) ga_ring(x, y, 13 * fs, 1.0f * fs, 1.0f, 0.3f, 0.25f, 0.3f + 0.3f * pulse, 24);
    }
    glDisable(GL_BLEND);
    end_overlay();
}

/* The announcement, the mirror notice and the last try's verdict: over everything but
   the cursor, every frame of a Gun Game. */
static void ga_draw_banners(int fbw, int fbh)
{
    if ((!g_sb.GunPlayers && !g_gaFake) || g_gameOver.valid) return;
    const std::vector<GunEffect>& fx = ga_effects();
    const int s = ga_scale(fbw, fbh);
    begin_overlay(fbw, fbh);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    int row = 0;
    for (const GunEffect& e : fx) {
        if (e.kind != GFX_ANNOUNCE || e.age > 45 || e.value <= 0 || e.value >= GA_COUNT) continue;
        const int key = e.owner * 64 + e.value, used = g_engineFrame - e.age;
        const auto seen = g_gaAnnounceSeen.find(key);
        if (seen == g_gaAnnounceSeen.end() || abs(seen->second - used) > 3) {
            g_gaAnnounceSeen[key] = used;
            ga_sound("BLEEP2.AUD");
            printf("GAHUD|announce|owner=%d|id=%d|name=%s\n", e.owner, e.value, GA_NAME[e.value]);
            fflush(stdout);
        }
        char who[NM_PLAYER_NAME_MAX + 8], line[96];
        ga_seat_name_of_house(e.owner, who, sizeof who);
        snprintf(line, sizeof line, "%s: %s", who, GA_NAME[e.value]);
        const int bs = std::max(1, std::min(s, fbw / std::max(1, (int)sb_text_w(line, 1) + 40)));
        const int w = (int)sb_text_w(line, bs) + 24 * s, h = (g_sbFontCellH * bs) + 12 * s;
        const int x = (fbw - w) / 2, y = fbh * 16 / 100 + row * (h + 4 * s);
        const float fade = e.age > 35 ? (45 - e.age) / 10.0f : 1.0f;
        sb_rect(x, y, x + w, y + h, 0.094f, 0.09f, 0.075f, 0.92f * fade);
        sb_frame(x, y, x + w, y + h, 0.67f, 0.55f, 0.33f, fade);
        sb_text(x + 12 * s, y + 6 * s, line, bs, 1.0f, 0.85f, 0.54f);
        if (++row == 3) break;
    }
    if (ga_mirrored()) {
        char line[48];
        snprintf(line, sizeof line, "CONTROLS MIRRORED  %d", (int)((ga_timers()[GA_MIRROR] + g_gaTps - 1) / g_gaTps));
        const int w = (int)sb_text_w(line, s) + 16 * s, h = 17 * s;
        const int x = (fbw - w) / 2, y = fbh - h - (g_ga.open ? 90 : 14) * s;
        sb_rect(x, y, x + w, y + h, 0.25f, 0.06f, 0.05f, 0.9f);
        sb_frame(x, y, x + w, y + h, 0.96f, 0.35f, 0.28f, 1);
        sb_text(x + 8 * s, y + 3 * s, line, s, 1.0f, 0.75f, 0.70f);
    }
    const Uint32 now = SDL_GetTicks();
    if (!g_ga.open && g_ga.toast[0] && now - g_ga.toast_at < 1600) {
        const int w = (int)sb_text_w(g_ga.toast, s) + 16 * s, h = 17 * s;
        const int x = (fbw - w) / 2, y = fbh - h - 40 * s;
        sb_rect(x, y, x + w, y + h, 0.094f, 0.09f, 0.075f, 0.95f);
        sb_frame(x, y, x + w, y + h, g_ga.toast_bad ? 0.96f : 0.39f, g_ga.toast_bad ? 0.29f : 0.80f, g_ga.toast_bad ? 0.24f : 0.36f, 1);
        sb_text(x + 8 * s, y + 3 * s, g_ga.toast, s, g_ga.toast_bad ? 1.0f : 0.6f, g_ga.toast_bad ? 0.6f : 0.95f, g_ga.toast_bad ? 0.55f : 0.6f);
    }
    glDisable(GL_BLEND);
    end_overlay();
}

/* The open game's panel. True when one was drawn (the caller puts the cursor on top). */
static bool ga_draw_panel(int fbw, int fbh)
{
    ga_draw_banners(fbw, fbh);
    if (!g_ga.open || g_gameOver.valid) return false;
    ga_draw_game(g_ga.g, ga_world_frame(fbw, fbh));
    return true;
}

/* ---- the minimap -------------------------------------------------------------------- */

static void ga_draw_minimap(int fbw, int fbh)
{
    const std::vector<GunEffect>& fx = ga_effects();
    if (fx.empty() || g_mapW <= 0) return;
    minimap_layout(fbw, fbh);
    if (g_mmW <= 0) return;
    const float z = g_mmScale;
    const float pulse = 0.5f + 0.5f * sinf((float)SDL_GetTicks() * 0.007f);
    const auto at = [&](int lx, int ly, float* x, float* y) {
        *x = g_mmX + (lx / 256.0f - g_mapX) * z;
        *y = g_mmY + (ly / 256.0f - g_mapY) * z;
    };
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const float d = std::max(2.0f, z);
    for (const GunEffect& e : fx) {
        float x, y;
        at(e.x, e.y, &x, &y);
        switch (e.kind) {
        case GFX_SMOKE: sb_rect(x - d * 0.5f, y - d * 0.5f, x + d * 0.5f, y + d * 0.5f, 0.75f, 0.75f, 0.72f, 0.75f); break;
        case GFX_VORTEX: ga_ring(x, y, 1.6f * z, std::max(1.0f, z * 0.4f), 0.7f, 0.45f, 1.0f, 0.9f, 16); break;
        case GFX_WORMHOLE: {
            float x2, y2;
            at(e.x2, e.y2, &x2, &y2);
            ga_disc(x, y, d, 0.75f, 0.45f, 1.0f, 1);
            ga_disc(x2, y2, d, 0.35f, 0.45f, 1.0f, 1);
            ga_quad_line(x, y, x2, y2, std::max(1.0f, z * 0.3f), 0.65f, 0.5f, 1.0f, 0.5f);
            break;
        }
        case GFX_MINE: sb_rect(x - d * 0.4f, y - d * 0.4f, x + d * 0.4f, y + d * 0.4f, 0.95f, 0.35f, 0.25f, 1); break;
        case GFX_BOOBY: sb_rect(x - d * 0.5f, y - d * 0.5f, x + d * 0.5f, y + d * 0.5f, 0.95f, 0.65f, 0.25f, 1); break;
        case GFX_REVEAL: {
            const float r = (e.value == 1 ? 2.2f : 1.5f) * d;
            ga_disc(x, y, r, 1.0f, 0.3f, 0.25f, 0.5f + 0.5f * pulse, 12);
            if (e.value == 1) ga_ring(x, y, r + d, std::max(1.0f, z * 0.3f), 1.0f, 0.85f, 0.4f, 1, 12);
            break;
        }
        default: break;
        }
    }
    /* the bomb's ring, on every unit of its carrier as in the world view */
    std::vector<const GunEffect*> loose;
    const std::map<std::string, int> potato = ga_potato_ticks(fx, &loose);
    for (const GunEffect* e : loose) {
        float x, y;
        at(e->x, e->y, &x, &y);
        ga_ring(x, y, 1.5f * d, std::max(1.0f, z * 0.3f), 1.0f, 0.55f, 0.3f, 0.5f + 0.5f * pulse, 12);
    }
    for (const SimObject& o : g_objects) {
        if (!ga_combatant(o) || o.gahide || !(o.gaf & GAF_HOT_POTATO) || !potato.count(o.house)) continue;
        float x, y;
        at((int)(o.ex * 256.0f), (int)(o.ez * 256.0f), &x, &y);
        ga_ring(x, y, 1.5f * d, std::max(1.0f, z * 0.3f), 1.0f, 0.55f, 0.3f, 0.5f + 0.5f * pulse, 12);
    }
}

/* ---- the script verbs ---------------------------------------------------------------- */

static bool ga_script_verb(const char* cmd, const char* arg, int fbw, int fbh, int* fails)
{
    ga_env_once();
    if (!strcmp(cmd, "gagrant")) {
        const int id = atoi(arg);
        bool ok = false;
        if (id > GA_NONE && id < GA_COUNT) {
            if (g_gaFake) { g_gaFakeHeld = id; ok = true; }
            else if (BrainGaGrant) ok = BrainGaGrant(0, id);
        }
        printf("GAHUD|grant|id=%d|name=%s|%s\n", id, id > 0 && id < GA_COUNT ? GA_NAME[id] : "?",
               ok ? "OK" : BrainGaGrant || g_gaFake ? "REFUSED" : "NO EXPORT");
    } else if (!strcmp(cmd, "gause")) {
        ga_try_open(fbw, fbh, SDL_GetTicks());
    } else if (!strcmp(cmd, "gaauto") || !strcmp(cmd, "gafail")) {
        ga_auto(fbw, fbh, !strcmp(cmd, "gaauto"), (Uint32)atoi(arg));
    } else if (!strcmp(cmd, "gacancel")) {
        ga_cancel(SDL_GetTicks());
    } else if (!strcmp(cmd, "gaclearblock")) {
        /* test only: the retry pause is real time, which a script does not spend */
        g_ga.blocked_until = 0;
        printf("GAHUD|clearblock\n");
    } else if (!strcmp(cmd, "gaselftest")) {
        if (ga_selftest()) ++*fails;
    } else if (!strcmp(cmd, "gaworld")) {
        /* what the live adapter sees: the player's combatant and each other one */
        const GaWorld w = ga_world_live(fbw, fbh);
        printf("GAHUD|world|unit=%d|at=%.0f,%.0f|cell=%d,%d|enemies=%zu\n", w.unit ? 1 : 0, w.ux, w.uy, w.ucx, w.ucy, w.enemies.size());
        for (const SimObject& o : g_objects) {
            if (!ga_combatant(o) || ga_own(o)) continue;
            float x, y;
            ga_screen_of(o, fbw, fbh, &x, &y);
            printf("GAHUD|world|other|%s|%s|id=%d|visible=%d|hidden=%d|house=%d|cell=%d,%d|screen=%.0f,%.0f|gaf=%u|str=%d\n",
                   o.type, o.house, o.id, visible(o) ? 1 : 0, o.gahide ? 1 : 0, ga_house_number(o.house),
                   (int)floorf(o.ex), (int)floorf(o.ez), x, y, o.gaf, o.str);
        }
    } else if (!strcmp(cmd, "gastate")) {
        printf("GAHUD|state|held=%d|open=%d|id=%d|phase=%d|result=%d|blocked_ms=%d|fake=%d|exports=%d%d%d%d%d%d\n",
               ga_held(), g_ga.open ? 1 : 0, g_ga.g.id, g_ga.g.phase, g_ga.g.result,
               (int)std::max<Sint64>(0, (Sint64)g_ga.blocked_until - (Sint64)SDL_GetTicks()), g_gaFake ? 1 : 0,
               BrainGaHeld != nullptr, BrainGaUse != nullptr, BrainGaFlags != nullptr, BrainGaEffects != nullptr,
               BrainGaTimers != nullptr, BrainGaGrant != nullptr);
    } else if (!strcmp(cmd, "gafx")) {
        /* gafx dump | gafx clear | gafx KIND CX CY [CX2 CY2 VALUE AGE OWNER], cells, the
           stand-in only for the last two */
        if (!strncmp(arg, "dump", 4)) {
            g_gaFxSerial = ~0u;
            for (const GunEffect& e : ga_effects())
                printf("GAFX|kind=%d|owner=%d|at=%d,%d|to=%d,%d|age=%d|value=%d\n", e.kind, e.owner, e.x / 256, e.y / 256,
                       e.x2 / 256, e.y2 / 256, e.age, e.value);
            printf("GAFX|count=%zu\n", g_gaFx.size());
            return true;
        }
        if (!strncmp(arg, "clear", 5)) { g_gaFakeFx.clear(); }
        else {
            int k = 0, cx = 0, cy = 0, cx2 = 0, cy2 = 0, value = 150, age = 0, owner = 4;
            if (sscanf(arg, "%d %d %d %d %d %d %d %d", &k, &cx, &cy, &cx2, &cy2, &value, &age, &owner) < 3) {
                printf("SCRIPT|gafx wants clear | KIND CX CY [CX2 CY2 VALUE AGE OWNER]\n");
                ++*fails;
                return true;
            }
            GunEffect e;
            e.kind = k; e.owner = owner; e.x = cx * 256 + 128; e.y = cy * 256 + 128;
            e.x2 = cx2 * 256 + 128; e.y2 = cy2 * 256 + 128; e.age = age; e.value = value;
            g_gaFakeFx.push_back(e);
        }
        g_gaFxSerial = ~0u;
    } else if (!strcmp(cmd, "gatimer")) {
        int id = 0, ticks = 0;
        if (sscanf(arg, "%d %d", &id, &ticks) == 2 && id > 0 && id < GA_COUNT) g_gaFakeTimers[id] = ticks;
        g_gaTimerSerial = ~0u;
    } else {
        return false;
    }
    fflush(stdout);
    return true;
}
