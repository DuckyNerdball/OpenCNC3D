// OpenCNC3D Tiberian Dawn Rally: the match.
//
// This file owns the race as a whole: the switch, the course read from the scenario's
// [Rally] section, the seats, the phases (PICK, COUNTDOWN, RACE, DONE, OVER), spawning and
// respawning the vehicles, the course projection that gives progress, laps and place, the
// results and the deciding player's choice to race again or end the match (a retry puts
// the course back the way it stood at the start), the engine hooks for damage and kills,
// the orders, what the exports report, and the prediction of the local racer's driving
// ahead of the lockstep.
// The vehicle physics are in rally_physics.cpp, items in rally_items.cpp and the AI
// drivers in rally_ai.cpp; rally_internal.h is the state they share.
//
// Determinism: integers only, the synchronised random stream only (Random_Pick), and
// every loop runs in seat or index order, so all peers of a lockstep match agree.
#include "function.h"
#include "rally_internal.h"
#include "rally_trig.h"
#include "fair_ai_td.h"
#include <errno.h>

namespace rally {

Match M;

// The roster in chooser order, with the handling of each vehicle. The top speeds sit in
// a narrow band (the slowest is 0.89 of the fastest): a heavy vehicle's armour only
// pays for a small loss of pace, and a vehicle without a weapon (Harvester, Mobile HQ,
// MCV, Hovercraft) is a little faster than an armed one of its weight, so every
// vehicle can win a race.
const KartStats Karts[MAX_ROSTER] = {
    // type          top  accel turn grip mass radius
    {UNIT_BUGGY,     690, 900,  225, 85,  100, 32},
    {UNIT_JEEP,      680, 850,  210, 90,  120, 33},
    {UNIT_BIKE,      700, 1000, 240, 80,  70,  25},
    {UNIT_APC,       660, 750,  190, 90,  200, 40},
    {UNIT_LTANK,     650, 700,  180, 95,  250, 40},
    {UNIT_MTANK,     640, 650,  170, 95,  300, 42},
    {UNIT_STANK,     670, 800,  200, 90,  200, 38},
    {UNIT_FTANK,     640, 650,  170, 95,  300, 42},
    {UNIT_HTANK,     620, 550,  160, 100, 500, 48},
    {UNIT_ARTY,      630, 600,  165, 90,  250, 42},
    {UNIT_MLRS,      630, 650,  170, 90,  250, 42},
    {UNIT_MSAM,      650, 700,  180, 90,  220, 40},
    {UNIT_HOVER,     680, 700,  190, 55,  200, 45},
    {UNIT_HARVESTER, 630, 600,  165, 100, 450, 48},
    {UNIT_MHQ,       665, 750,  185, 90,  250, 42},
    {UNIT_MCV,       625, 550,  160, 100, 550, 50},
};
const int KartCount = MAX_ROSTER;

/* ------------------------------------------------------------ units and maths ---- */

static int Tps10()
{
    return M.TicksPerSecondX10 > 0 ? M.TicksPerSecondX10 : 150;
}

int32_t Velocity(int cps100)
{
    return (int32_t)((int64_t)cps100 * 16777216 / (10 * (int64_t)Tps10()));
}

int32_t Accel(int a100)
{
    const int64_t t = Tps10();
    return (int32_t)((int64_t)a100 * 16777216 / (4 * t * t));
}

int32_t Turn_Rate(int degrees)
{
    return (int32_t)((int64_t)degrees * 167772160 / (1440 * (int64_t)Tps10()));
}

int32_t Decay(int per_second_x100)
{
    const int64_t d = (int64_t)per_second_x100 * 65536 / (40 * (int64_t)Tps10());
    return (int32_t)(d > 65536 ? 65536 : d < 0 ? 0 : d);
}

int Ticks(int tenths)
{
    const int t = (tenths * Tps10() + 50) / 100;
    return t < 1 ? 1 : t;
}

int32_t Sin16(int32_t angle)
{
    angle &= 0xFFFF;
    const int quadrant = angle >> 14;
    int32_t r = angle & 0x3FFF;
    if (quadrant & 1) r = 0x4000 - r;
    const int index = r >> 6;
    const int frac = r & 63;
    int32_t v = Rally_Sine_Quarter[index];
    if (index < 256) v += ((Rally_Sine_Quarter[index + 1] - v) * frac) >> 6;
    return (quadrant & 2) ? -v : v;
}

int32_t Cos16(int32_t angle)
{
    return Sin16(angle + 0x4000);
}

// The largest angle a in 0..8192 (0..45 degrees) with tan(a) <= n / d, for 0 <= n <= d.
static int32_t Atan_Small(uint64_t n, uint64_t d)
{
    int32_t lo = 0, hi = 8192;
    while (lo < hi) {
        const int32_t mid = (lo + hi + 1) >> 1;
        if ((uint64_t)Sin16(mid) * d <= (uint64_t)Cos16(mid) * n) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}

int32_t Angle_Of(int64_t dx, int64_t dy)
{
    if (!dx && !dy) return 0;
    // Keep the products in Atan_Small inside 64 bits.
    while (dx > 0x7FFFFFFFLL || dx < -0x7FFFFFFFLL || dy > 0x7FFFFFFFLL || dy < -0x7FFFFFFFLL) {
        dx /= 2;
        dy /= 2;
    }
    const uint64_t ax = (uint64_t)(dx < 0 ? -dx : dx);
    const uint64_t ay = (uint64_t)(dy < 0 ? -dy : dy);
    const int32_t b = ax <= ay ? Atan_Small(ax, ay) : 16384 - Atan_Small(ay, ax);
    if (dx >= 0 && dy < 0) return b;
    if (dx >= 0) return (32768 - b) & 0xFFFF;
    if (dy >= 0) return (32768 + b) & 0xFFFF;
    return (65536 - b) & 0xFFFF;
}

int64_t Isqrt(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = (uint64_t)1 << 62;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (int64_t)result;
}

int32_t Angle_Diff(int32_t to, int32_t from)
{
    return (int32_t)(int16_t)(uint16_t)((to - from) & 0xFFFF);
}

/* ---------------------------------------------------------------- racers ---- */

bool Racer_Index_Valid(int index)
{
    return index >= 0 && index < M.RacerCount;
}

const KartStats& Kart_Of(const Racer& r)
{
    return Karts[r.Kart >= 0 && r.Kart < KartCount ? r.Kart : 0];
}

UnitClass* Racer_Unit(const Racer& r)
{
    if (r.Unit == TARGET_NONE) return nullptr;
    UnitClass* unit = As_Unit(r.Unit);
    if (!unit || !unit->IsActive || unit->IsInLimbo || unit->Strength <= 0 || unit->House == nullptr
        || unit->House->Class->House != r.House) {
        return nullptr;
    }
    return unit;
}

bool Racer_Alive(const Racer& r)
{
    return !(r.Flags & RALLY_F_DEAD) && Racer_Unit(r) != nullptr;
}

// A racer another racer may shoot at: on the course, not cloaked, shielded or finished.
bool Racer_Fair_Game(const Racer& r)
{
    return Racer_Alive(r) && !(r.Flags & (RALLY_F_FINISHED | RALLY_F_RETIRED)) && r.Cloak == 0 && r.Shield == 0;
}

// The rival of r nearest (x, y) within reach, or -1.
int Racer_Near(const Racer& r, int32_t x, int32_t y, int32_t reach)
{
    int best = -1;
    int64_t best_d2 = (int64_t)reach * reach;
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& o = M.Racers[i];
        if (&o == &r || !Racer_Fair_Game(o)) continue;
        const int64_t dx = (int64_t)o.X - x, dy = (int64_t)o.Y - y;
        const int64_t d2 = dx * dx + dy * dy;
        if (d2 <= best_d2) {
            best_d2 = d2;
            best = i;
        }
    }
    return best;
}

// The racer whose vehicle this is. The owner must match as well as the heap slot: a
// racer's vehicle the engine removed without a kill (blown up with its house, or sold)
// leaves its slot to whichever vehicle is made next, perhaps another racer's, until the
// race notices the old one is gone. Strength is not checked, because the kill hook asks
// about a vehicle that has just reached zero.
static Racer* Racer_Of(const TechnoClass* object)
{
    if (!object || !M.Started || object->What_Am_I() != RTTI_UNIT || !object->House) return nullptr;
    const TARGET target = object->As_Target();
    const HousesType owner = object->House->Class->House;
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& r = M.Racers[i];
        if (r.Unit == target && r.Unit != TARGET_NONE && r.House == owner) return &M.Racers[i];
    }
    return nullptr;
}

bool Racer_Out(const Racer& r)
{
    return (r.Flags & RALLY_F_RETIRED) != 0;
}

static Racer* Racer_Of_House(HousesType house)
{
    for (int i = 0; i < M.RacerCount; ++i) {
        if (M.Racers[i].House == house) return &M.Racers[i];
    }
    return nullptr;
}

static int Index_Of(const Racer& r)
{
    return (int)(&r - M.Racers);
}

int Racer_By_Place(int place)
{
    for (int i = 0; i < M.RacerCount; ++i) {
        if (M.Racers[i].Place == place) return i;
    }
    return -1;
}

/* ---------------------------------------------------------------- course ---- */

// A circuit's segments wrap round; a point-to-point course's are clamped to the course.
int Course_Wrap_Segment(int segment)
{
    if (M.Loop && M.PointCount > 0) {
        segment %= M.PointCount;
        return segment < 0 ? segment + M.PointCount : segment;
    }
    if (segment > M.PointCount - 2) segment = M.PointCount - 2;
    return segment < 0 ? 0 : segment;
}

// The point a segment ends at, and its distance along the lap.
static const CoursePoint& Segment_End(int segment)
{
    return M.Point[segment + 1 < M.PointCount ? segment + 1 : 0];
}

static int64_t Segment_End_Along(int segment)
{
    return segment + 1 < M.PointCount ? M.Point[segment + 1].Along : M.Length;
}

int Course_Segments_Between(int from, int to)
{
    if (M.Loop && M.PointCount > 0) {
        const int n = ((to - from) % M.PointCount + M.PointCount) % M.PointCount;
        return n;
    }
    return to > from ? to - from : 0;
}

static int64_t Lap_Wrap(int64_t along)
{
    if (M.Length <= 0) return along;
    along %= M.Length;
    return along < 0 ? along + M.Length : along;
}

int64_t Course_Delta(int64_t to, int64_t from)
{
    int64_t d = to - from;
    if (M.Loop && M.Length > 0) {
        d = Lap_Wrap(d);
        if (d * 2 > M.Length) d -= M.Length;
    }
    return d;
}

int32_t Course_Direction(int segment)
{
    if (M.PointCount < 2) return 0;
    segment = Course_Wrap_Segment(segment);
    const CoursePoint& a = M.Point[segment];
    const CoursePoint& b = Segment_End(segment);
    return Angle_Of((int64_t)b.X - a.X, (int64_t)b.Y - a.Y);
}

// The nearest point of one segment to (x, y): its distance along the lap and the squared
// distance from (x, y).
static void Project_On(int segment, int32_t x, int32_t y, int64_t* along, int64_t* d2)
{
    const CoursePoint& a = M.Point[segment];
    const CoursePoint& b = Segment_End(segment);
    const int64_t sx = (int64_t)b.X - a.X;
    const int64_t sy = (int64_t)b.Y - a.Y;
    const int64_t px = (int64_t)x - a.X;
    const int64_t py = (int64_t)y - a.Y;
    const int64_t len2 = sx * sx + sy * sy;
    int64_t t = 0; // 0..65536 along the segment
    if (len2 > 0) {
        int64_t dot = px * sx + py * sy;
        if (dot <= 0) {
            t = 0;
        } else if (dot >= len2) {
            t = 65536;
        } else {
            // dot < len2 < 2^47, so scale len2 down rather than dot up.
            const int64_t div = len2 >> 16;
            t = div > 0 ? dot / div : 0;
            if (t > 65536) t = 65536;
        }
    }
    const int64_t cx = a.X + ((sx * t) >> 16);
    const int64_t cy = a.Y + ((sy * t) >> 16);
    const int64_t dx = x - cx;
    const int64_t dy = y - cy;
    *d2 = dx * dx + dy * dy;
    *along = a.Along + (((Segment_End_Along(segment) - a.Along) * t) >> 16);
}

// The course segment nearest (x, y) among those from first to last, counted on from the
// segment `from` (wrapping round a circuit), with the distance along the lap of the
// projection and the distance from the centre line.
static int Course_Segment_In(int32_t x, int32_t y, int from, int first, int last, int64_t* along, int32_t* distance)
{
    int best = Course_Wrap_Segment(from + first);
    int64_t best_d2 = -1;
    int64_t best_along = 0;
    for (int k = first; k <= last; ++k) {
        const int i = Course_Wrap_Segment(from + k);
        int64_t a = 0, d2 = 0;
        Project_On(i, x, y, &a, &d2);
        if (best_d2 < 0 || d2 < best_d2) {
            best_d2 = d2;
            best = i;
            best_along = a;
        }
    }
    if (along) *along = best_along;
    if (distance) *distance = (int32_t)Isqrt((uint64_t)(best_d2 < 0 ? 0 : best_d2));
    return best;
}

// The course segment nearest (x, y) within a window around the hint, with the distance
// along the course of the projection and the distance from the centre line.
int Course_Segment_Near(int32_t x, int32_t y, int hint, int64_t* along, int32_t* distance)
{
    if (M.Loop) return Course_Segment_In(x, y, hint, -4, 8, along, distance);
    const int last = M.PointCount - 2;
    int first = hint - 4;
    int end = hint + 8;
    if (first < 0) first = 0;
    if (end > last) end = last;
    return Course_Segment_In(x, y, 0, first, end, along, distance);
}

// The centre line point at a distance along the course (round and round a circuit).
void Course_Point_At(int64_t along, int32_t* x, int32_t* y, int* segment)
{
    if (M.Loop) {
        along = Lap_Wrap(along);
    } else if (along <= 0) {
        *x = M.Point[0].X;
        *y = M.Point[0].Y;
        if (segment) *segment = 0;
        return;
    }
    const int segments = M.Loop ? M.PointCount : M.PointCount - 1;
    for (int i = 0; i < segments; ++i) {
        const CoursePoint& a = M.Point[i];
        const CoursePoint& b = Segment_End(i);
        const int64_t b_along = Segment_End_Along(i);
        if (along <= b_along || i == segments - 1) {
            const int64_t len = b_along - a.Along;
            int64_t t = len > 0 ? ((along - a.Along) << 16) / len : 0;
            if (t > 65536) t = 65536;
            if (t < 0) t = 0;
            *x = (int32_t)(a.X + ((((int64_t)b.X - a.X) * t) >> 16));
            *y = (int32_t)(a.Y + ((((int64_t)b.Y - a.Y) * t) >> 16));
            if (segment) *segment = i;
            return;
        }
    }
    *x = M.Point[M.PointCount - 1].X;
    *y = M.Point[M.PointCount - 1].Y;
    if (segment) *segment = M.PointCount - 2;
}

/* ------------------------------------------------------------- the switch ---- */

static void Reset_Match()
{
    const bool enabled = M.Enabled;
    const int rate = M.TicksPerSecondX10;
    const unsigned seed = M.DebugSeed;
    const bool seed_set = M.DebugSeedSet;
    const int laps = M.ConfigLaps;
    memset(&M, 0, sizeof M);
    M.Enabled = enabled;
    M.TicksPerSecondX10 = rate;
    M.DebugSeed = seed;
    M.DebugSeedSet = seed_set;
    M.ConfigLaps = laps;
    M.Hostile = HOUSE_BAD;
    M.Winner = -1;
    M.Decider = -1;
    M.Laps = 1;
    M.Phase = RALLY_PHASE_OFF;
}

static bool Refuse(const char* why)
{
    char text[160];
    snprintf(text, sizeof text, "Rally: this map cannot be raced (%s); the match is refused.", why);
    GlyphX_Debug_Print(text);
    M.Loaded = false;
    return false;
}

// Reads count comma separated integers. Each is parsed in 64 bits and refused unless it
// lies within -limit..limit, so a course reads the same on every platform (long is 32
// bits on 64-bit Windows and 64 bits elsewhere) and no later scaling can overflow.
static bool Read_Ints(CCINIClass& ini, const char* key3, const char* key, int* v, int count, int limit)
{
    char buf[128];
    buf[0] = 0;
    if (!ini.Get_String("Rally", key3, "", buf, sizeof buf) || !buf[0]) {
        if (!key || !ini.Get_String("Rally", key, "", buf, sizeof buf) || !buf[0]) return false;
    }
    const char* p = buf;
    for (int i = 0; i < count; ++i) {
        while (*p == ' ' || *p == '\t') ++p;
        char* end = nullptr;
        errno = 0;
        const long long value = strtoll(p, &end, 10);
        if (end == p || errno == ERANGE || value < -(long long)limit || value > (long long)limit) return false;
        v[i] = (int)value;
        p = end;
        while (*p == ' ' || *p == '\t') ++p;
        if (i + 1 < count) {
            if (*p != ',') return false;
            ++p;
        }
    }
    return true;
}

// Larger than any lepton on the map, small enough that x * 256 fits in 32 bits.
const int LEPTON_LIMIT = 1 << 20;

static bool Lepton_In_Map(int x, int y)
{
    return x >= 0 && y >= 0 && x < MAP_CELL_W * 256 && y < MAP_CELL_H * 256;
}

} // namespace rally

using namespace rally;

bool Rally_Active()
{
    return M.Enabled && GameToPlay == GAME_GLYPHX_MULTIPLAYER;
}

void Rally_Enable(bool enabled, int ticks_per_second_x10)
{
    M.Enabled = enabled;
    if (ticks_per_second_x10 < 50) ticks_per_second_x10 = 150;
    if (ticks_per_second_x10 > 600) ticks_per_second_x10 = 600;
    M.TicksPerSecondX10 = ticks_per_second_x10;
    Reset_Match();
}

int Rally_Configure(int laps)
{
    if (laps < RALLY_LAPS_MIN) laps = RALLY_LAPS_MIN;
    if (laps > RALLY_LAPS_MAX) laps = RALLY_LAPS_MAX;
    M.ConfigLaps = laps;
    return laps;
}

bool Rally_Read_INI(CCINIClass& ini)
{
    Reset_Match();
    if (!Rally_Active()) return true;
    if (!ini.Section_Present("Rally")) return Refuse("no [Rally] section");

    ini.Get_String("Rally", "Name", "", M.Name, sizeof M.Name);
    char buf[64];
    ini.Get_String("Rally", "Hostile", "BadGuy", buf, sizeof buf);
    M.Hostile = HouseTypeClass::From_Name(buf);
    if (M.Hostile == HOUSE_NONE) return Refuse("Hostile names no house");

    // A circuit: the points go once round the lap from the start line, which is also the
    // finish line, and the last one joins the first.
    M.Loop = ini.Get_Int("Rally", "Loop", 0) != 0;
    const int points = ini.Get_Int("Rally", "Points", 0);
    if (points < (M.Loop ? 3 : 2) || points > RALLY_MAX_COURSE) return Refuse("Points must be 2 to 512 (3 on a circuit)");
    M.PointCount = points;
    int64_t along = 0;
    for (int i = 0; i < points; ++i) {
        char key3[16], key[16];
        snprintf(key3, sizeof key3, "P%03d", i);
        snprintf(key, sizeof key, "P%d", i);
        int v[3];
        if (!Read_Ints(ini, key3, key, v, 3, LEPTON_LIMIT)) return Refuse("a course point is missing or malformed");
        if (!Lepton_In_Map(v[0], v[1])) return Refuse("a course point is off the map");
        if (v[2] <= 0 || v[2] > MAP_CELL_W * 256) return Refuse("a course point's half width is out of range");
        CoursePoint& p = M.Point[i];
        p.X = v[0] * 256;
        p.Y = v[1] * 256;
        p.HalfWidth = v[2] * 256;
        if (i > 0) {
            const int64_t dx = (int64_t)p.X - M.Point[i - 1].X;
            const int64_t dy = (int64_t)p.Y - M.Point[i - 1].Y;
            along += Isqrt((uint64_t)(dx * dx + dy * dy));
        }
        p.Along = along;
    }
    if (M.Loop) {
        const int64_t dx = (int64_t)M.Point[0].X - M.Point[points - 1].X;
        const int64_t dy = (int64_t)M.Point[0].Y - M.Point[points - 1].Y;
        along += Isqrt((uint64_t)(dx * dx + dy * dy));
    }
    M.Length = along;
    if (M.Length <= 0) return Refuse("the course has no length");
    // Laps are asked for before the match; a point-to-point course is one run.
    const int laps = M.ConfigLaps > 0 ? M.ConfigLaps : RALLY_LAPS_DEFAULT;
    M.Laps = M.Loop ? laps : 1;

    M.GridFacing = ini.Get_Int("Rally", "GridFacing", 0) & 255;
    M.GridCount = 0;
    for (int i = 0; i < MAX_GRID; ++i) {
        char key[16];
        snprintf(key, sizeof key, "Grid%d", i);
        int v[2];
        if (!Read_Ints(ini, key, nullptr, v, 2, LEPTON_LIMIT)) break;
        if (!Lepton_In_Map(v[0], v[1])) return Refuse("a grid slot is off the map");
        M.GridX[i] = v[0] * 256;
        M.GridY[i] = v[1] * 256;
        M.GridCount = i + 1;
    }
    if (M.GridCount < MPlayerCount || M.GridCount < 1) return Refuse("fewer grid slots than racers");

    int f[4];
    if (!Read_Ints(ini, "Finish", nullptr, f, 4, LEPTON_LIMIT)) return Refuse("no Finish line");
    if (!Lepton_In_Map(f[0], f[1]) || !Lepton_In_Map(f[2], f[3])) return Refuse("the finish line is off the map");
    M.FinishX1 = f[0] * 256;
    M.FinishY1 = f[1] * 256;
    M.FinishX2 = f[2] * 256;
    M.FinishY2 = f[3] * 256;

    int crates = ini.Get_Int("Rally", "Crates", 0);
    if (crates < 0) crates = 0;
    if (crates > RALLY_MAX_CRATES) crates = RALLY_MAX_CRATES;
    M.CrateCount = 0;
    for (int i = 0; i < crates; ++i) {
        char key3[16], key[16];
        snprintf(key3, sizeof key3, "C%02d", i);
        snprintf(key, sizeof key, "C%d", i);
        int v[2];
        if (!Read_Ints(ini, key3, key, v, 2, LEPTON_LIMIT)) return Refuse("a crate spot is missing or malformed");
        if (!Lepton_In_Map(v[0], v[1])) return Refuse("a crate spot is off the map");
        Crate& c = M.Crates[M.CrateCount++];
        c.X = v[0] * 256;
        c.Y = v[1] * 256;
        c.Cell = XY_Cell(v[0] >> 8, v[1] >> 8);
    }

    // A test hook may ask for a different synchronised random stream; nothing else does.
    if (M.DebugSeedSet) Scen.RandomNumber = RandomClass(M.DebugSeed);

    M.Loaded = true;
    return true;
}

namespace rally {

/* ------------------------------------------------------------ spawning ---- */

static bool Spot_Free(const Racer& r, CELL cell, int32_t x, int32_t y)
{
    if (!Map.In_Radar(cell) || Cell_Blocked(cell, &r)) return false;
    if (Map[cell].Cell_Techno()) return false;
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& o = M.Racers[i];
        if (&o == &r || !Racer_Alive(o)) continue;
        const int64_t dx = (int64_t)o.X - x, dy = (int64_t)o.Y - y;
        const int64_t gap = (int64_t)(Kart_Of(o).Radius + Kart_Of(r).Radius) * FINE / 100 + FINE / 8;
        if (dx * dx + dy * dy < gap * gap) return false;
    }
    return true;
}

// Nothing blocked on the straight line between two points, sampled every quarter cell.
static bool Line_Clear(const Racer& r, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    const int64_t dx = (int64_t)x1 - x0, dy = (int64_t)y1 - y0;
    const int64_t len = Isqrt((uint64_t)(dx * dx + dy * dy));
    const int steps = (int)(len / (FINE / 4)) + 1;
    for (int i = 1; i <= steps; ++i) {
        if (Point_Blocked((int32_t)(x0 + dx * i / steps), (int32_t)(y0 + dy * i / steps), &r)) return false;
    }
    return true;
}

// Searches rings around (x, y) for a cell the racer may stand on: the exact point first,
// then cell centres in sight of the point, so a racer is never put down on the far side
// of a wall in a neighbouring stretch of the course. Returns false when nothing within
// the radius is free.
static bool Find_Spot(const Racer& r, int32_t* x, int32_t* y, int radius)
{
    const CELL center = XY_Cell(*x >> 16, *y >> 16);
    const int32_t ox = *x, oy = *y;
    if (Spot_Free(r, center, *x, *y) && !Point_Blocked(*x, *y, &r)) return true;
    for (int ring = 0; ring <= radius; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (abs(dx) != ring && abs(dy) != ring) continue;
                const int cx = Cell_X(center) + dx;
                const int cy = Cell_Y(center) + dy;
                if (cx < 0 || cy < 0 || cx >= MAP_CELL_W || cy >= MAP_CELL_H) continue;
                const CELL cell = XY_Cell(cx, cy);
                const int32_t px = cx * FINE + FINE / 2;
                const int32_t py = cy * FINE + FINE / 2;
                if (Spot_Free(r, cell, px, py) && !Point_Blocked(px, py, &r) && Line_Clear(r, ox, oy, px, py)) {
                    *x = px;
                    *y = py;
                    return true;
                }
            }
        }
    }
    return false;
}

static void Keep_Idle(UnitClass* unit)
{
    if (unit->IsDriving || Target_Legal(unit->NavCom)) {
        unit->Stop_Driver();
        unit->Force_Track(-1, 0);
    }
    unit->IsDriving = false;
    unit->NavCom = TARGET_NONE;
    unit->SuspendedNavCom = TARGET_NONE;
    unit->Path[0] = FACING_NONE;
    unit->TarCom = TARGET_NONE;
    unit->SuspendedTarCom = TARGET_NONE;
    if (unit->Mission != MISSION_SLEEP) unit->Set_Mission(MISSION_SLEEP);
}

static void Clear_Motion(Racer& r)
{
    r.Z = 0;
    r.VX = r.VY = r.VZ = 0;
    r.Pitch = r.Roll = 0;
    r.PitchRate = r.RollRate = r.SpinRate = 0;
    r.Speed = 0;
    r.Spin = 0;
    r.FlipTicks = 0;
    r.Righting = 0;
    r.DriftCharge = 0;
    r.DriftDir = 0;
    r.Flags &= ~(RALLY_F_FLIPPED | RALLY_F_RIGHTING | RALLY_F_AIRBORNE | RALLY_F_SPINNING | RALLY_F_DRIFT
                 | RALLY_F_TURBO_READY);
}

// Puts a new engine unit for the racer at (r.X, r.Y) or the nearest free cell.
static bool Spawn_Unit(Racer& r, int radius)
{
    int32_t x = r.X, y = r.Y;
    if (!Find_Spot(r, &x, &y, radius)) return false;
    UnitClass* unit = new UnitClass((UnitType)Kart_Of(r).Type, r.House);
    if (!unit) return false;
    const DirType dir = (DirType)((r.Yaw >> 16) & 255);
    // The spot has passed Rally's own checks, which are stricter than the engine's except
    // that the engine also keeps computer units off crate cells; placed the way the engine
    // places a unit it has already found room for (ScenarioInit set around Unlimbo).
    ++ScenarioInit;
    const bool placed = unit->Unlimbo(Cell_Coord(XY_Cell(x >> 16, y >> 16)), dir);
    --ScenarioInit;
    if (!placed) {
        delete unit;
        return false;
    }
    r.X = x;
    r.Y = y;
    r.PrevX = x;
    r.PrevY = y;
    unit->Mark(MARK_UP);
    unit->Coord = XY_Coord(x >> 8, y >> 8);
    unit->Mark(MARK_DOWN);
    unit->PrimaryFacing.Set(dir);
    unit->SecondaryFacing.Set(dir);
    unit->Cloak = UNCLOAKED;
    Keep_Idle(unit);
    r.Unit = unit->As_Target();
    r.Turret = r.Yaw;
    return true;
}

// How far (x, y) lies along the course's first segment from the start line, in fine units,
// negative behind it: where a racer stands before a point-to-point course begins.
static int64_t Along_From_Start(int32_t x, int32_t y)
{
    if (M.PointCount < 2) return 0;
    const CoursePoint& a = M.Point[0];
    const CoursePoint& b = M.Point[1];
    const int64_t sx = (int64_t)b.X - a.X;
    const int64_t sy = (int64_t)b.Y - a.Y;
    const int64_t len = Isqrt((uint64_t)(sx * sx + sy * sy));
    if (len <= 0) return 0;
    return (((int64_t)x - a.X) * sx + ((int64_t)y - a.Y) * sy) / len;
}

// Where a racer put on the grid stands on the course. A circuit's grid is behind the line,
// on the last points of the lap, so it is looked for round the start line, and a racer
// there is a negative distance along: it has not started lap 1 yet. A point-to-point
// course's grid is behind its first point, the same negative distance along.
static void Locate_On_Grid(Racer& r)
{
    r.Checkpoint = 0;
    r.LapsDone = 0;
    r.Segment = 0;
    r.Dist = 0;
    if (!M.Loop) {
        const int64_t d = Along_From_Start(r.X, r.Y);
        r.Dist = d < 0 ? d : 0;
        return;
    }
    const int back = M.PointCount / 4 < 16 ? M.PointCount / 4 : 16;
    int64_t along = 0;
    r.Segment = Course_Segment_In(r.X, r.Y, 0, -back, 8, &along, nullptr);
    r.Dist = along * 2 > M.Length ? along - M.Length : along;
}

static void Spawn_Grid()
{
    int order[RALLY_MAX_RACERS];
    for (int i = 0; i < M.RacerCount; ++i) order[i] = i;
    for (int i = 0; i < M.RacerCount - 1; ++i) {
        const int j = Random_Pick(i, M.RacerCount - 1);
        const int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (int slot = 0; slot < M.RacerCount; ++slot) {
        Racer& r = M.Racers[order[slot]];
        if (Racer_Out(r)) continue;
        r.GridSlot = slot % M.GridCount;
        r.X = M.GridX[r.GridSlot];
        r.Y = M.GridY[r.GridSlot];
        r.Yaw = M.GridFacing << 16;
        Clear_Motion(r);
        const bool spawned = Spawn_Unit(r, 4);
        Locate_On_Grid(r);
        if (!spawned) {
            // Tried again every tick until it fits, the same way a respawn waits.
            r.Flags |= RALLY_F_DEAD;
            r.Dead = 1;
        }
    }
}

/* PUTTING A RACER BACK ON THE COURSE (the rescue key, or a respawn after being destroyed)
   must never move it on: it goes back to the last course point it passed, and its distance
   along the course is counted from that point, never from whichever segment the nearest
   one search happens to settle on. On a circuit's start line the last segment of the lap
   is exactly as near as the first, and reading the racer's progress off it once handed out
   a lap that was never driven. A racer still behind the start line at the start of a race
   has passed no point at all (point 0 is the line, ahead of the whole grid), so it goes
   back to its own grid slot. */
static bool Behind_Start(const Racer& r)
{
    return r.LapsDone == 0 && r.Checkpoint == 0 && r.GridSlot >= 0 && M.GridCount > 0 && r.Dist < 0;
}

static void Put_Back_Target(const Racer& r, int32_t* x, int32_t* y, int32_t* yaw, int* cp, bool* grid)
{
    *grid = Behind_Start(r);
    if (*grid) {
        const int slot = r.GridSlot % M.GridCount;
        *x = M.GridX[slot];
        *y = M.GridY[slot];
        *yaw = M.GridFacing << 16;
        *cp = 0;
        return;
    }
    int c = r.Checkpoint;
    if (!M.Loop && c > M.PointCount - 2) c = M.PointCount - 2;
    if (c < 0) c = 0;
    *x = M.Point[c].X;
    *y = M.Point[c].Y;
    *yaw = Course_Direction(c) << 8;
    *cp = c;
}

// Where a racer put back near course point cp (or on its grid slot) now stands on the
// course: counted from that point, and from the laps it has done.
static void Locate_Put_Back(Racer& r, int cp, bool grid)
{
    if (grid) {
        Locate_On_Grid(r);
        return;
    }
    int64_t along = 0;
    r.Segment = Course_Segment_Near(r.X, r.Y, cp, &along, nullptr);
    if (M.Loop) {
        r.Dist = (int64_t)r.LapsDone * M.Length + M.Point[cp].Along + Course_Delta(along, M.Point[cp].Along);
    } else {
        r.Dist = along;
    }
}

bool Teleport_To_Checkpoint(Racer& r)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit) return false;
    int32_t x = 0, y = 0, yaw = 0;
    int cp = 0;
    bool grid = false;
    Put_Back_Target(r, &x, &y, &yaw, &cp, &grid);
    // Its own unit must not count as the obstacle.
    unit->Mark(MARK_UP);
    const bool found = Find_Spot(r, &x, &y, 6);
    if (found) {
        r.X = r.PrevX = x;
        r.Y = r.PrevY = y;
        unit->Coord = XY_Coord(x >> 8, y >> 8);
    }
    unit->Mark(MARK_DOWN);
    if (!found) return false;
    Clear_Motion(r);
    r.Yaw = yaw;
    Locate_Put_Back(r, cp, grid);
    unit->PrimaryFacing.Set((DirType)((r.Yaw >> 16) & 255));
    return true;
}

static void Respawn(Racer& r)
{
    if (Racer_Out(r)) return;
    int32_t x = 0, y = 0, yaw = 0;
    int cp = 0;
    bool grid = false;
    Put_Back_Target(r, &x, &y, &yaw, &cp, &grid);
    r.X = x;
    r.Y = y;
    r.Yaw = yaw;
    Clear_Motion(r);
    if (!Spawn_Unit(r, 6)) {
        r.Dead = 1; // nothing free yet; try again next tick
        return;
    }
    Locate_Put_Back(r, cp, grid);
    r.Flags &= ~RALLY_F_DEAD;
    r.Shield = Ticks(20);
    ++M.Stats[STAT_RESPAWNS];
}

static void Mark_Dead(Racer& r)
{
    r.Unit = TARGET_NONE;
    r.Flags = (r.Flags & (RALLY_F_HUMAN | RALLY_F_FINISHED | RALLY_F_ROULETTE | RALLY_F_RETIRED)) | RALLY_F_DEAD;
    r.Dead = Ticks(30);
    r.Boost = r.Armour = r.Cloak = r.Shield = 0;
    Clear_Motion(r);
}

// A racer's vehicle can leave the engine without a kill being recorded (blown up with
// its house while damage is switched off, or sold); the race then counts it as destroyed.
static void Sweep_Stale(Racer& r)
{
    if (!(r.Flags & RALLY_F_DEAD) && r.Unit != TARGET_NONE && !Racer_Unit(r)) Mark_Dead(r);
}

/* ------------------------------------------------------------- damage ---- */

void Damage_Racer(Racer& r, int damage, int source_racer)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit || damage <= 0) return;
    TechnoClass* source = nullptr;
    if (Racer_Index_Valid(source_racer) && source_racer != Index_Of(r)) source = Racer_Unit(M.Racers[source_racer]);
    const bool was = M.RallyDamage;
    M.RallyDamage = true;
    // The laser warhead is 100% against every armour, so the item's damage is exact.
    unit->Take_Damage(damage, 0, WARHEAD_LASER, source);
    M.RallyDamage = was;
}

// Throws a racer into the air with spin, harder for more damage and lighter vehicles.
void Throw_Racer(Racer& r, int damage, int scale_percent)
{
    if (!Racer_Alive(r) || (r.Flags & (RALLY_F_RIGHTING | RALLY_F_FINISHED)) || damage <= 0) return;
    const int mass = Kart_Of(r).Mass;
    int up = (300 + damage * 500 / mass) * scale_percent / 100;
    if (up > 1200) up = 1200;
    int spin = damage * 600 / mass * scale_percent / 100;
    if (spin > 900) spin = 900;
    if (r.VZ < Velocity(up)) r.VZ = Velocity(up);
    if (r.Z <= 0) r.Z = 1;
    const int32_t rate = Turn_Rate(spin);
    switch (Random_Pick(0, 3)) {
    case 0: r.RollRate = rate; break;
    case 1: r.RollRate = -rate; break;
    case 2: r.PitchRate = rate; break;
    default: r.PitchRate = -rate; break;
    }
    r.Flags |= RALLY_F_AIRBORNE;
    ++M.Stats[STAT_THROWN];
}

/* ------------------------------------------------------------- the match ---- */

static bool Is_Seat(HousesType house)
{
    for (int i = 0; i < MPlayerCount; ++i) {
        if (MPlayerHouses[i] == house) return true;
    }
    return false;
}

void Place_Crate(Crate& c)
{
    CellClass& cell = Map[c.Cell];
    if (!Map.In_Radar(c.Cell) || (cell.Overlay != OVERLAY_NONE && cell.Overlay != OVERLAY_WOOD_CRATE)) {
        c.Active = false;
        return;
    }
    cell.Overlay = OVERLAY_WOOD_CRATE;
    cell.OverlayData = 0;
    cell.Redraw_Objects();
    c.Active = true;
    c.Return = 0;
}

/* ------------------------------------------------- the course as it started ---- */

static void Record(int kind, int type, HousesType house, COORDINATE coord, int facing, int strength, TARGET live)
{
    if (M.FixtureCount >= MAX_FIXTURES) return;
    Fixture& f = M.Fixtures[M.FixtureCount++];
    f.Kind = kind;
    f.Type = type;
    f.House = house;
    f.Coord = coord;
    f.Facing = facing;
    f.Strength = strength;
    f.Live = live;
}

static bool Course_Object(const TechnoClass* t)
{
    return t && t->IsActive && !t->IsInLimbo && t->Strength > 0 && t->House && !Is_Seat(t->House->Class->House);
}

// Everything on the course that is not a racer, as it stands before the first race: every
// building, unit and soldier of a house that is not a seat (the hostile defences and base,
// civilian buildings) and every tree.
static void Record_Fixtures()
{
    M.FixtureCount = 0;
    for (int i = 0; i < Buildings.Count(); ++i) {
        const BuildingClass* b = Buildings.Ptr(i);
        if (!Course_Object(b)) continue;
        Record(FIXTURE_BUILDING, b->Class->Type, b->House->Class->House, Cell_Coord(Coord_Cell(b->Coord)),
               b->PrimaryFacing.Current(), b->Strength, b->As_Target());
    }
    for (int i = 0; i < Units.Count(); ++i) {
        const UnitClass* u = Units.Ptr(i);
        if (!Course_Object(u)) continue;
        Record(FIXTURE_UNIT, u->Class->Type, u->House->Class->House, u->Coord, u->PrimaryFacing.Current(), u->Strength,
               u->As_Target());
    }
    for (int i = 0; i < Infantry.Count(); ++i) {
        const InfantryClass* n = Infantry.Ptr(i);
        if (!Course_Object(n)) continue;
        Record(FIXTURE_INFANTRY, n->Class->Type, n->House->Class->House, n->Coord, n->PrimaryFacing.Current(),
               n->Strength, n->As_Target());
    }
    for (int i = 0; i < Terrains.Count(); ++i) {
        const TerrainClass* t = Terrains.Ptr(i);
        if (!t || !t->IsActive || t->IsInLimbo || t->Strength <= 0) continue;
        Record(FIXTURE_TERRAIN, t->Class->Type, HOUSE_NONE, Coord_Whole(t->Coord), 0, t->Strength, t->As_Target());
    }
}

// Takes a vehicle or soldier off the map without a kill, the way a sold one leaves.
static void Remove_Foot(FootClass* object)
{
    object->Stun();
    object->Limbo();
    object->Delete_This();
}

// The fixture's object if it still stands where it stood, of the same kind and owner.
static ObjectClass* Fixture_Standing(const Fixture& f)
{
    switch (f.Kind) {
    case FIXTURE_BUILDING: {
        BuildingClass* b = As_Building(f.Live);
        if (Course_Object(b) && b->Class->Type == f.Type && b->House->Class->House == f.House
            && Cell_Coord(Coord_Cell(b->Coord)) == f.Coord) {
            return b;
        }
        return nullptr;
    }
    case FIXTURE_UNIT: {
        UnitClass* u = As_Unit(f.Live);
        return Course_Object(u) && u->Class->Type == f.Type && u->House->Class->House == f.House ? u : nullptr;
    }
    case FIXTURE_INFANTRY: {
        InfantryClass* n = As_Infantry(f.Live);
        return Course_Object(n) && n->Class->Type == f.Type && n->House->Class->House == f.House ? n : nullptr;
    }
    case FIXTURE_TERRAIN: {
        ObjectClass* o = As_Object(f.Live);
        if (!o || o->What_Am_I() != RTTI_TERRAIN) return nullptr;
        TerrainClass* t = (TerrainClass*)o;
        if (t->IsActive && !t->IsInLimbo && t->Strength > 0 && t->Class->Type == f.Type
            && Coord_Whole(t->Coord) == f.Coord) {
            return t;
        }
        return nullptr;
    }
    default:
        return nullptr;
    }
}

// How many fixtures stand as they did at the start: in place, at their starting strength
// (units and soldiers also where they started).
static int Fixtures_Intact()
{
    int count = 0;
    for (int i = 0; i < M.FixtureCount; ++i) {
        const Fixture& f = M.Fixtures[i];
        const ObjectClass* o = Fixture_Standing(f);
        if (!o || o->Strength != f.Strength) continue;
        if ((f.Kind == FIXTURE_UNIT || f.Kind == FIXTURE_INFANTRY) && o->Coord != f.Coord) continue;
        ++count;
    }
    return count;
}

// Puts the course back the way it stood before the first race: every vehicle, soldier and
// shell gone from it, buildings that stand repaired, buildings and trees that fell put
// back, the units and soldiers of the course put back where they started, the marks of
// run-over soldiers wiped, and every crate back. Run on every peer at the same tick from
// the same state, so every peer gets the same course.
static void Restore_Course()
{
    for (int i = 0; i < M.RacerCount; ++i) {
        UnitClass* unit = Racer_Unit(M.Racers[i]);
        if (unit) Remove_Foot(unit);
        M.Racers[i].Unit = TARGET_NONE;
    }
    // Units and soldiers of the course (survivors of a fallen building among them) come
    // back from the record; a unit in limbo is cargo and goes with its carrier.
    for (int i = Units.Count() - 1; i >= 0; --i) {
        UnitClass* u = Units.Ptr(i);
        if (Course_Object(u)) Remove_Foot(u);
    }
    for (int i = Infantry.Count() - 1; i >= 0; --i) {
        InfantryClass* n = Infantry.Ptr(i);
        if (Course_Object(n)) Remove_Foot(n);
    }
    for (int i = Bullets.Count() - 1; i >= 0; --i) {
        BulletClass* b = Bullets.Ptr(i);
        if (b && b->IsActive) b->Delete_This();
    }

    // Buildings and trees that stand are repaired first, so a fallen one rebuilt below
    // cannot be mistaken for another that stands in the same heap slot.
    for (int i = 0; i < M.FixtureCount; ++i) {
        Fixture& f = M.Fixtures[i];
        if (f.Kind != FIXTURE_BUILDING && f.Kind != FIXTURE_TERRAIN) continue;
        ObjectClass* o = Fixture_Standing(f);
        if (!o) {
            f.Live = TARGET_NONE;
            continue;
        }
        o->Strength = f.Strength;
        if (f.Kind == FIXTURE_BUILDING) {
            BuildingClass* b = (BuildingClass*)o;
            b->PrimaryFacing.Set((DirType)f.Facing);
            b->Assign_Target(TARGET_NONE);
        }
        o->Mark(MARK_CHANGE);
    }
    ++ScenarioInit;
    for (int i = 0; i < M.FixtureCount; ++i) {
        Fixture& f = M.Fixtures[i];
        if (f.Kind == FIXTURE_UNIT || f.Kind == FIXTURE_INFANTRY) f.Live = TARGET_NONE;
        if (f.Live != TARGET_NONE) continue;
        switch (f.Kind) {
        case FIXTURE_BUILDING: {
            BuildingClass* b = new BuildingClass((StructType)f.Type, f.House);
            if (!b) break;
            if (b->Unlimbo(f.Coord, (DirType)f.Facing)) {
                b->Strength = f.Strength;
                b->IsALemon = false;
                f.Live = b->As_Target();
            } else {
                delete b;
            }
            break;
        }
        case FIXTURE_TERRAIN: {
            // A tree that fell and is still burning out or crumbling holds its cell until it
            // deletes itself, which would leave the course a tree short for the next race:
            // it goes now, and the new one takes its place.
            TerrainClass* dying = Map[Coord_Cell(f.Coord)].Cell_Terrain();
            if (dying && dying->IsActive && dying->Strength <= 0) {
                dying->Detach_All();
                delete dying;
            }
            // The engine places a tree as it makes it, and takes it back if the cell is taken.
            new TerrainClass((TerrainType)f.Type, Coord_Cell(f.Coord));
            const CellClass& cell = Map[Coord_Cell(f.Coord)];
            const TerrainClass* t = cell.Cell_Terrain();
            if (t && t->Class->Type == f.Type) f.Live = t->As_Target();
            break;
        }
        case FIXTURE_UNIT: {
            UnitClass* u = new UnitClass((UnitType)f.Type, f.House);
            if (!u) break;
            if (u->Unlimbo(f.Coord, (DirType)f.Facing)) {
                u->Strength = f.Strength;
                u->Enter_Idle_Mode();
                f.Live = u->As_Target();
            } else {
                delete u;
            }
            break;
        }
        case FIXTURE_INFANTRY: {
            InfantryClass* n = new InfantryClass((InfantryType)f.Type, f.House);
            if (!n) break;
            if (n->Unlimbo(f.Coord, (DirType)f.Facing)) {
                n->Strength = f.Strength;
                n->Enter_Idle_Mode();
                f.Live = n->As_Target();
            } else {
                delete n;
            }
            break;
        }
        default:
            break;
        }
    }
    --ScenarioInit;

    // The marks of soldiers run over (they are back on their feet).
    for (int y = Map.MapCellY; y < Map.MapCellY + Map.MapCellHeight; ++y) {
        for (int x = Map.MapCellX; x < Map.MapCellX + Map.MapCellWidth; ++x) {
            CellClass& cell = Map[XY_Cell(x, y)];
            if (cell.Overlay != OVERLAY_SQUISH) continue;
            cell.Overlay = OVERLAY_NONE;
            cell.OverlayData = 0;
            cell.Redraw_Objects();
        }
    }
    M.HazardCount = 0;
    for (int i = 0; i < M.CrateCount; ++i) {
        Crate& c = M.Crates[i];
        if (!c.Active) Place_Crate(c);
        c.Return = 0;
    }
}

// The player whose choice ends the results: the lowest seat that is a player and still in
// the match (the host of a network match, the player of a skirmish), or -1.
static int Find_Decider()
{
    for (int i = 0; i < M.RacerCount; ++i) {
        if (M.Racers[i].Human && !Racer_Out(M.Racers[i])) return i;
    }
    return -1;
}

static void Seat_Racer(Racer& r, int seat, int kart, int last_kart, bool autopilot, int retired)
{
    memset(&r, 0, sizeof r);
    r.House = MPlayerHouses[seat];
    r.Seat = seat;
    HouseClass* house = HouseClass::As_Pointer(r.House);
    r.Human = house && house->IsHuman;
    r.Skill = house ? TD_Fair_AI_Level(house) : 1;
    r.Kart = kart;
    r.LastKart = last_kart;
    r.Autopilot = autopilot;
    r.Unit = TARGET_NONE;
    r.FinishTicks = -1;
    r.GridSlot = -1;
    r.Place = seat + 1;
    r.AiTarget = TARGET_NONE;
    r.Flags = retired;
    if (r.Human) r.Flags |= RALLY_F_HUMAN;
    // A computer seat (and a seat that has left) has chosen already; a player chooses in PICK,
    // with the last vehicle chosen for them if they let the time run out.
    r.Picked = !r.Human || retired != 0;
}

// Race again: the same seats, laps and course, the course put back as it started, and every
// seat back to PICK with the vehicle it raced preselected.
static void Retry()
{
    Restore_Course();
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        Seat_Racer(r, r.Seat, r.Kart, r.Kart, r.Autopilot, r.Flags & RALLY_F_RETIRED);
    }
    M.Phase = RALLY_PHASE_PICK;
    M.PhaseTicks = Ticks(200);
    M.RaceTicks = 0;
    M.AfterWinner = 0;
    M.FinishedCount = 0;
    memset(M.FinishOrder, 0, sizeof M.FinishOrder);
    M.Winner = -1;
    M.AutoEnd = 0;
    ++M.RaceNumber;
}

static void Start()
{
    M.Started = true;
    if (!M.Loaded) return;
    M.RacerCount = MPlayerCount < RALLY_MAX_RACERS ? MPlayerCount : RALLY_MAX_RACERS;
    for (int i = 0; i < M.RacerCount; ++i) Seat_Racer(M.Racers[i], i, -1, -1, false, 0);
    M.RaceNumber = 1;
    Record_Fixtures();

    // No shroud in a race: every racer sees the whole course from the start.
    for (int i = 0; i < M.RacerCount; ++i) {
        HouseClass* house = HouseClass::As_Pointer(M.Racers[i].House);
        if (!house) continue;
        for (int y = Map.MapCellY; y < Map.MapCellY + Map.MapCellHeight; ++y) {
            for (int x = Map.MapCellX; x < Map.MapCellX + Map.MapCellWidth; ++x) {
                Map.Map_Cell(XY_Cell(x, y), house, false);
            }
        }
    }

    for (int i = 0; i < M.CrateCount; ++i) Place_Crate(M.Crates[i]);

    // Computer seats choose at once.
    for (int i = 0; i < M.RacerCount; ++i) {
        if (!M.Racers[i].Human) M.Racers[i].Kart = Random_Pick(0, KartCount - 1);
    }
    M.Decider = Find_Decider();
    M.Phase = RALLY_PHASE_PICK;
    M.PhaseTicks = Ticks(200);
}

static void Ensure_Started()
{
    if (!M.Started && M.Loaded) Start();
}

// A seat can change hands during a race: a player who leaves is handed to the computer
// (OpenCNC3D_Player_To_AI) or has the house blown up (a leave without takeover, a
// disconnect, a resignation). Every peer applies either to the same house on the same
// frame, so reading the house each tick keeps the race in step.
static void Refresh_Seats()
{
    // Once the result is handed over every loser's house is defeated; that is not a seat
    // leaving.
    if (M.Over) return;
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        if (Racer_Out(r)) continue;
        HouseClass* house = HouseClass::As_Pointer(r.House);
        if (!house || house->IsDefeated || house->IsToDie) {
            // Gone from the match: it races no further, is never put back on the
            // course, and is ranked behind every racer still in the race.
            r.Flags |= RALLY_F_RETIRED;
            r.Bits = 0;
            r.ItemLatch = r.RescueLatch = false;
            r.Autopilot = false;
            continue;
        }
        if (r.Human && !house->IsHuman) {
            // Handed to the computer: an AI driver drives on from here.
            r.Skill = TD_Fair_AI_Level(house);
            r.Bits = 0;
            r.ItemLatch = r.RescueLatch = false;
            if (r.Kart < 0 && M.Phase == RALLY_PHASE_PICK) r.Kart = Random_Pick(0, KartCount - 1);
            r.Picked = true;
        }
        r.Human = house->IsHuman;
    }
}

static bool Segments_Cross(int64_t ax, int64_t ay, int64_t bx, int64_t by, int64_t cx, int64_t cy, int64_t dx,
                           int64_t dy)
{
    const auto side = [](int64_t px, int64_t py, int64_t qx, int64_t qy, int64_t rx, int64_t ry) {
        const int64_t v = (qx - px) * (ry - py) - (qy - py) * (rx - px);
        return v > 0 ? 1 : v < 0 ? -1 : 0;
    };
    const int o1 = side(cx, cy, dx, dy, ax, ay);
    const int o2 = side(cx, cy, dx, dy, bx, by);
    const int o3 = side(ax, ay, bx, by, cx, cy);
    const int o4 = side(ax, ay, bx, by, dx, dy);
    return o1 * o2 <= 0 && o3 * o4 <= 0 && !(o1 == 0 && o2 == 0);
}

static void Finish(Racer& r)
{
    r.Flags |= RALLY_F_FINISHED;
    r.FinishTicks = M.RaceTicks;
    r.WrongWay = 0;
    M.FinishOrder[M.FinishedCount++] = Index_Of(r);
    if (M.Winner < 0) M.Winner = Index_Of(r);
    r.Boost = r.Armour = r.Cloak = 0;
}

// Against the course for more than a second is the wrong way.
static void Check_Wrong_Way(Racer& r, int seg)
{
    const int32_t dir = Course_Direction(seg);
    const int64_t along_speed = ((int64_t)r.VX * Sin16(dir) - (int64_t)r.VY * Cos16(dir)) >> 16;
    if (along_speed < -(int64_t)Velocity(Kart_Of(r).Top * 15 / 100)) {
        ++r.WrongWay;
    } else {
        r.WrongWay = 0;
    }
}

// Progress over the race and over the lap, from the laps done and the distance into the
// lap being driven (nothing before the line, a whole lap at most until the line counts it).
static void Set_Progress(Racer& r)
{
    const int64_t lap = M.Length;
    int64_t into = r.Dist - (int64_t)r.LapsDone * lap;
    if (into < 0) into = 0;
    if (into > lap) into = lap;
    if (r.Flags & RALLY_F_FINISHED) into = lap;
    const int64_t laps = M.Laps > 0 ? M.Laps : 1;
    int64_t race = (int64_t)(r.LapsDone < laps ? r.LapsDone : laps - 1) * lap + into;
    if (r.LapsDone >= laps) race = laps * lap;
    r.Progress = (int)(race * 65536 / (laps * lap));
    r.LapProgress = (int)(into * 65536 / lap);
}

static void Update_Progress(Racer& r)
{
    if (!Racer_Alive(r) || Racer_Out(r)) return;
    int64_t along = 0;
    int32_t distance = 0;
    const int seg = Course_Segment_Near(r.X, r.Y, r.Segment, &along, &distance);
    r.Segment = seg;

    if (!M.Loop) {
        // Point to point: one run from the start line to the finish line. Behind the start
        // line the distance is negative, so the grid's rows keep their order until GO.
        r.Dist = along;
        if (seg == 0 && along <= M.Point[0].Along) {
            const int64_t d = Along_From_Start(r.X, r.Y);
            if (d < 0) r.Dist = d;
        }
        int64_t p = along * 65536 / M.Length;
        if (p < 0) p = 0;
        if (p > 65536) p = 65536;
        r.Progress = (int)p;
        r.LapProgress = r.Progress;
        int passed = seg;
        if (along >= M.Point[seg + 1].Along) passed = seg + 1;
        if (passed > r.Checkpoint) r.Checkpoint = passed;

        if (r.Flags & RALLY_F_FINISHED) return;
        Check_Wrong_Way(r, seg);

        const bool enough = (int64_t)r.Checkpoint * 10 >= (int64_t)(M.PointCount - 1) * 9;
        if (enough
            && (Segments_Cross(r.PrevX, r.PrevY, r.X, r.Y, M.FinishX1, M.FinishY1, M.FinishX2, M.FinishY2)
                || along >= M.Length)) {
            r.LapsDone = 1;
            r.LapProgress = 65536;
            Finish(r);
        }
        return;
    }

    // A circuit. The distance along goes on counting past the line from lap to lap (and is
    // negative behind it on the grid), so it is the projection's step since the last tick,
    // the short way round, added on.
    r.Dist += Course_Delta(along, Lap_Wrap(r.Dist));
    const int64_t into = r.Dist - (int64_t)r.LapsDone * M.Length;
    if (into >= 0 && into < M.Length) {
        // The points passed in this lap, read off the distance into it. Not off the nearest
        // segment: on the line itself the lap's last segment is as near as its first, and
        // its end would say the whole lap had been passed.
        int passed = r.Checkpoint;
        while (passed + 1 < M.PointCount && M.Point[passed + 1].Along <= into) ++passed;
        r.Checkpoint = passed;
    }
    if (!(r.Flags & RALLY_F_FINISHED)) {
        Check_Wrong_Way(r, seg);
        // A lap counts when the racer crosses the line forward, having passed at least 90%
        // of the lap's points since the last time it counted. (Should the line ever be
        // missed, by a racer put back beyond it, two cells past the end of the lap count.)
        const bool enough =
            (int64_t)r.Checkpoint * 10 >= (int64_t)M.PointCount * 9 || r.Checkpoint >= M.PointCount - 1;
        const int32_t dir = Course_Direction(0);
        const int64_t forward = ((int64_t)(r.X - r.PrevX) * Sin16(dir) - (int64_t)(r.Y - r.PrevY) * Cos16(dir));
        if (enough
            && ((forward > 0
                 && Segments_Cross(r.PrevX, r.PrevY, r.X, r.Y, M.FinishX1, M.FinishY1, M.FinishX2, M.FinishY2))
                || into >= M.Length + (int64_t)FINE * 2)) {
            ++r.LapsDone;
            r.Checkpoint = 0;
            if (r.LapsDone >= M.Laps) Finish(r);
        }
    }
    Set_Progress(r);
}

static bool Ahead(const Racer& a, const Racer& b)
{
    // A seat that has left the match is behind every racer still in it.
    if (Racer_Out(a) != Racer_Out(b)) return Racer_Out(b);
    const bool fa = (a.Flags & RALLY_F_FINISHED) != 0;
    const bool fb = (b.Flags & RALLY_F_FINISHED) != 0;
    if (fa != fb) return fa;
    if (fa) {
        if (a.FinishTicks != b.FinishTicks) return a.FinishTicks < b.FinishTicks;
        for (int i = 0; i < M.FinishedCount; ++i) {
            if (M.FinishOrder[i] == Index_Of(a)) return true;
            if (M.FinishOrder[i] == Index_Of(b)) return false;
        }
        return a.Seat < b.Seat;
    }
    if (a.Progress != b.Progress) return a.Progress > b.Progress;
    // Round a circuit the progress is coarse over many laps; the distance along settles it,
    // and on the grid (negative, behind the line) it is the order of the rows.
    if (a.Dist != b.Dist) return a.Dist > b.Dist;
    if (a.Checkpoint != b.Checkpoint) return a.Checkpoint > b.Checkpoint;
    return a.Seat < b.Seat;
}

static void Update_Places()
{
    int order[RALLY_MAX_RACERS];
    for (int i = 0; i < M.RacerCount; ++i) order[i] = i;
    for (int i = 1; i < M.RacerCount; ++i) {
        const int v = order[i];
        int j = i - 1;
        while (j >= 0 && Ahead(M.Racers[v], M.Racers[order[j]])) {
            order[j + 1] = order[j];
            --j;
        }
        order[j + 1] = v;
    }
    for (int i = 0; i < M.RacerCount; ++i) M.Racers[order[i]].Place = i + 1;
}

static void Update_Flags(Racer& r)
{
    int f = r.Flags & (RALLY_F_DEAD | RALLY_F_FINISHED | RALLY_F_FLIPPED | RALLY_F_RIGHTING | RALLY_F_ROULETTE
                       | RALLY_F_DRIFT | RALLY_F_TURBO_READY | RALLY_F_RETIRED);
    if (r.Human) f |= RALLY_F_HUMAN;
    if (r.Z > 0 && !(f & RALLY_F_RIGHTING)) f |= RALLY_F_AIRBORNE;
    if (r.Boost > 0) f |= RALLY_F_BOOST;
    if (r.Cloak > 0) f |= RALLY_F_CLOAK;
    if (r.Armour > 0) f |= RALLY_F_ARMOUR;
    if (r.Spin > 0) f |= RALLY_F_SPINNING;
    if (r.Shield > 0) f |= RALLY_F_SHIELDED;
    if (r.WrongWay > Ticks(10)) f |= RALLY_F_WRONG_WAY;
    if (r.Roulette > 0) f |= RALLY_F_ROULETTE;
    else f &= ~RALLY_F_ROULETTE;
    r.Flags = f;
}

static void Count_Down(int& timer)
{
    if (timer > 0) --timer;
}

void Fire_Weapon(Racer& r, TARGET target, int32_t tx, int32_t ty, bool clamp)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit) return;
    const UnitTypeClass& type = *unit->Class;
    if (type.Primary == WEAPON_NONE || unit->Arm > 0) return;
    if (type.MaxAmmo > 0 && unit->Ammo == 0) return;
    if (r.Z > 0 || (r.Flags & (RALLY_F_FLIPPED | RALLY_F_RIGHTING | RALLY_F_FINISHED))) return;
    const int64_t range = (int64_t)Weapons[type.Primary].Range * 256;
    int64_t dx = (int64_t)tx - r.X, dy = (int64_t)ty - r.Y;
    int64_t dist = Isqrt((uint64_t)(dx * dx + dy * dy));
    if (dist < FINE * 3 / 4) return; // never shoot the ground under its own wheels
    if (!type.IsTurretEquipped) {
        // A weapon fixed to the hull fires only near straight ahead. A player's aim
        // outside that cone is turned to its edge; a computer driver holds its fire.
        const int32_t hull = (r.Yaw >> 8) & 0xFFFF;
        const int32_t off = Angle_Diff(Angle_Of(dx, dy), hull);
        if (off > FIXED_CONE || off < -FIXED_CONE) {
            if (!clamp) return;
            const int32_t dir = (hull + (off > 0 ? FIXED_CONE : -FIXED_CONE)) & 0xFFFF;
            dx = ((int64_t)Sin16(dir) * dist) >> 16;
            dy = -(((int64_t)Cos16(dir) * dist) >> 16);
            target = TARGET_NONE;
        }
    }
    if (dist > range) {
        if (!clamp) return;
        dx = dx * range / dist;
        dy = dy * range / dist;
        target = TARGET_NONE;
    }
    if (target == TARGET_NONE) {
        // Aimed at a spot of ground: the cell under it, inside the map.
        const int64_t limit = (int64_t)MAP_CELL_W * FINE - 1;
        int64_t gx = r.X + dx, gy = r.Y + dy;
        gx = gx < 0 ? 0 : gx > limit ? limit : gx;
        gy = gy < 0 ? 0 : gy > limit ? limit : gy;
        target = As_Target(XY_Cell((int)(gx >> 16), (int)(gy >> 16)));
    }
    if (type.IsTurretEquipped) {
        const int32_t want = Angle_Of(dx, dy);
        const int32_t diff = Angle_Diff(want, r.Turret >> 8);
        if (diff > 8 * 256 || diff < -8 * 256) return; // still turning
    }
    if (unit->TechnoClass::Fire_At(target, 0)) ++M.Stats[STAT_SHOTS];
}

void Aim_Turret(Racer& r, int32_t tx, int32_t ty)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit || !unit->Class->IsTurretEquipped) {
        r.Turret = r.Yaw;
        return;
    }
    const int32_t want = Angle_Of((int64_t)tx - r.X, (int64_t)ty - r.Y) << 8;
    // The engine turns a turret ROT+1 facing steps per 15 Hz tick; keep that real speed.
    const int degrees = ((int)unit->Class->ROT + 1) * 15 * 360 / 256;
    const int32_t step = Turn_Rate(degrees) * SUBSTEPS;
    int32_t diff = Angle_Diff(want >> 8, r.Turret >> 8) * 256;
    if (diff > step) diff = step;
    if (diff < -step) diff = -step;
    r.Turret = (r.Turret + diff) & 0xFFFFFF;
}

static void Racer_Controls(Racer& r)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit) return;
    const bool ai = !r.Human || r.Autopilot;
    if (ai && !(r.Flags & RALLY_F_FINISHED)) {
        AI_Drive(r);
        AI_Items(r);
    }
    if (r.Flags & RALLY_F_FINISHED) {
        r.Bits = 0;
        r.ItemLatch = r.RescueLatch = false;
    }

    if (ai) {
        AI_Fire(r);
    } else {
        Aim_Turret(r, r.AimX, r.AimY);
        if (r.Bits & RALLY_IN_FIRE) {
            // A crosshair on or beside a rival fires at that rival, so the round is
            // aimed where the rival will be when it lands rather than at the ground.
            TARGET target = As_Target(XY_Cell(r.AimX >> 16, r.AimY >> 16));
            int32_t tx = r.AimX, ty = r.AimY;
            const int rival = Racer_Near(r, r.AimX, r.AimY, FINE * 3 / 4);
            if (rival >= 0) {
                target = M.Racers[rival].Unit;
                tx = M.Racers[rival].X;
                ty = M.Racers[rival].Y;
            }
            Fire_Weapon(r, target, tx, ty, true);
        }
    }

    if (r.ItemLatch) {
        r.ItemLatch = false;
        if (r.Item != RALLY_ITEM_NONE && r.Roulette == 0) Use_Item(r);
    }
    if (r.RescueLatch) {
        r.RescueLatch = false;
        if (r.RescueCooldown == 0 && !(r.Flags & RALLY_F_FINISHED) && Teleport_To_Checkpoint(r)) {
            r.RescueCooldown = Ticks(30);
            ++M.Stats[STAT_RESCUES];
        }
    }

    // Launchers with a magazine reload one round at a time.
    if (unit->Class->MaxAmmo > 0 && unit->Ammo < unit->Class->MaxAmmo) {
        if (++r.Reload >= Ticks(40)) {
            ++unit->Ammo;
            r.Reload = 0;
        }
    }
}

void Racer_Timers(Racer& r)
{
    Count_Down(r.Boost);
    Count_Down(r.Armour);
    Count_Down(r.Cloak);
    Count_Down(r.Spin);
    Count_Down(r.Shield);
    Count_Down(r.RescueCooldown);
    if (r.Spin == 0) r.SpinRate = 0;
}

// The results: they stay up until the deciding player races again or ends the match, or,
// with no player left to decide, for ten seconds.
static void End_Race()
{
    M.Phase = RALLY_PHASE_DONE;
    M.AutoEnd = Ticks(100);
    M.Choice = RALLY_CHOICE_NONE;
    M.Decider = Find_Decider();
    M.PhaseTicks = M.Decider >= 0 ? 0 : M.AutoEnd;
    for (int i = 0; i < M.RacerCount; ++i) {
        M.Racers[i].Bits = 0;
        M.Racers[i].ItemLatch = M.Racers[i].RescueLatch = false;
    }
}

static void Hand_Over_Result()
{
    // A seat that has left the match cannot win it: the best placed racer still in the
    // match does, and nobody does if every seat has left.
    if (!Racer_Index_Valid(M.Winner) || Racer_Out(M.Racers[M.Winner])) {
        M.Winner = -1;
        for (int place = 1; place <= M.RacerCount && M.Winner < 0; ++place) {
            const int i = Racer_By_Place(place);
            if (Racer_Index_Valid(i) && !Racer_Out(M.Racers[i])) M.Winner = i;
        }
    }
    const HousesType winner = Racer_Index_Valid(M.Winner) ? M.Racers[M.Winner].House : HOUSE_NONE;
    for (int i = 0; i < MPlayerCount; ++i) {
        HouseClass* house = HouseClass::As_Pointer(MPlayerHouses[i]);
        if (house) house->IsDefeated = MPlayerHouses[i] != winner;
    }
    // The multiplayer result reads IsDefeated for every player.
    PlayerWins = true;
    M.Over = true;
}

static void End_Match()
{
    M.Phase = RALLY_PHASE_OVER;
    M.PhaseTicks = 0;
    Hand_Over_Result();
}

static void Race_Tick()
{
    ++M.RaceTicks;
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        Sweep_Stale(r);
        if (Racer_Out(r)) {
            r.Bits = 0;
            r.ItemLatch = r.RescueLatch = false;
            continue;
        }
        if (r.Flags & RALLY_F_DEAD) {
            // A key pressed while waiting to respawn is not saved up for afterwards.
            r.ItemLatch = r.RescueLatch = false;
            if (--r.Dead <= 0) Respawn(r);
            continue;
        }
        Racer_Timers(r);
        Racer_Controls(r);
    }
    Physics_Tick();
    Items_Tick();
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        Update_Progress(r);
        if (Racer_Alive(r)) Run_Over(r);
    }
    Update_Places();

    // The race ends when every racer still in it has finished; when every player has
    // finished (computer drivers still out are ranked by progress); 30 s after the
    // winner when only computer drivers are still out; 60 s after the winner when a
    // player is, unless that player is on the last tenth of the course (of the last lap,
    // round a circuit); and in any case after RACE_LIMIT a lap, so a race nobody finishes
    // still ends.
    bool all = true, any_human = false, human_close = false;
    int humans_racing = 0;
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& r = M.Racers[i];
        if (Racer_Out(r)) continue;
        const bool done = (r.Flags & RALLY_F_FINISHED) != 0;
        if (r.Human) {
            any_human = true;
            if (!done) {
                ++humans_racing;
                if (r.LapsDone >= M.Laps - 1 && (int64_t)r.LapProgress * 10 >= (int64_t)65536 * 9) human_close = true;
            }
        }
        if (!done) all = false;
    }
    if (M.Winner >= 0) ++M.AfterWinner;
    bool end = all;
    if (M.Winner >= 0) {
        if (any_human && humans_racing == 0) end = true;
        if (humans_racing == 0 && M.AfterWinner >= Ticks(300)) end = true;
        if (humans_racing > 0 && M.AfterWinner >= Ticks(600) && !human_close) end = true;
    }
    if (M.RaceTicks >= Ticks(RACE_LIMIT * M.Laps)) end = true;
    if (end) End_Race();
}

} // namespace rally

void Rally_Tick()
{
    if (!Rally_Active() || !M.Loaded) return;
    Ensure_Started();
    Refresh_Seats();
    if (!M.Over) M.Decider = Find_Decider();
    M.InTick = true;
    switch (M.Phase) {
    case RALLY_PHASE_PICK: {
        bool all = true;
        for (int i = 0; i < M.RacerCount; ++i) {
            if (!M.Racers[i].Picked && !Racer_Out(M.Racers[i])) all = false;
        }
        if (--M.PhaseTicks <= 0 || all) {
            // A seat that did not choose gets a vehicle at random, or after a retry keeps the
            // one it raced.
            for (int i = 0; i < M.RacerCount; ++i) {
                if (M.Racers[i].Kart < 0) M.Racers[i].Kart = Random_Pick(0, KartCount - 1);
                M.Racers[i].Picked = true;
            }
            Spawn_Grid();
            // The places on the grid are its rows, front first, from the first tick of the
            // countdown (not the seats' order until the first race tick).
            Update_Places();
            M.Phase = RALLY_PHASE_COUNTDOWN;
            M.PhaseTicks = Ticks(40);
        }
        break;
    }
    case RALLY_PHASE_COUNTDOWN:
        for (int i = 0; i < M.RacerCount; ++i) {
            Racer& r = M.Racers[i];
            Sweep_Stale(r);
            if ((r.Flags & RALLY_F_DEAD) && --r.Dead <= 0) Respawn(r);
            if (Racer_Alive(r)) {
                if (!r.Human || r.Autopilot) {
                    Aim_Turret(r, r.X + Sin16(r.Yaw >> 8) * 4, r.Y - Cos16(r.Yaw >> 8) * 4);
                } else {
                    Aim_Turret(r, r.AimX, r.AimY);
                }
                Sync_Engine(r);
            }
        }
        Update_Places();
        if (--M.PhaseTicks <= 0) {
            M.Phase = RALLY_PHASE_RACE;
            M.PhaseTicks = 0;
        }
        break;
    case RALLY_PHASE_RACE:
        Race_Tick();
        break;
    case RALLY_PHASE_DONE:
        // The deciding player's choice (an order executed before this tick, the same on
        // every peer) races again or ends the match; with no player left to choose, the
        // match ends after ten seconds of results.
        if (M.Choice == RALLY_CHOICE_RETRY) {
            Retry();
            break;
        }
        if (M.Choice == RALLY_CHOICE_END) {
            End_Match();
            break;
        }
        for (int i = 0; i < M.RacerCount; ++i) {
            Racer& r = M.Racers[i];
            r.Bits = 0;
            Sweep_Stale(r);
            if (r.Flags & RALLY_F_DEAD) {
                if (--r.Dead <= 0) Respawn(r);
                continue;
            }
            Racer_Timers(r);
        }
        Physics_Tick();
        Items_Tick();
        Update_Places();
        if (M.Decider >= 0) {
            M.PhaseTicks = 0;
        } else {
            M.PhaseTicks = --M.AutoEnd;
            if (M.AutoEnd <= 0) End_Match();
        }
        break;
    case RALLY_PHASE_OVER:
        Hand_Over_Result();
        break;
    default:
        break;
    }
    M.InTick = false;
    for (int i = 0; i < M.RacerCount; ++i) Update_Flags(M.Racers[i]);
}

/* ------------------------------------------------------------ engine hooks ---- */

bool Rally_Unit_AI(UnitClass* unit)
{
    if (!Rally_Active()) return false;
    Racer* r = Racer_Of(unit);
    if (!r) return false;
    // The engine's AI is skipped entirely, so the weapon's rearm count is kept here.
    if (unit->Arm > 0) --unit->Arm;
    if (unit->IsInRecoilState) {
        unit->IsInRecoilState = false;
        unit->Mark(MARK_CHANGE);
    }
    Keep_Idle(unit);
    return true;
}

bool Rally_Is_Racer(const TechnoClass* object)
{
    return Rally_Active() && Racer_Of(object) != nullptr;
}

bool Rally_Blocks_Order(const TechnoClass* object)
{
    return Rally_Is_Racer(object);
}

int Rally_Damage(TechnoClass* victim, int damage, TechnoClass* source)
{
    if (!Rally_Active() || damage <= 0) return damage;
    Racer* r = Racer_Of(victim);
    if (!r || M.DebugIgnoreShield) return damage;
    if (M.Phase != RALLY_PHASE_RACE || r->Shield > 0 || r->Armour > 0 || (r->Flags & RALLY_F_FINISHED)) return 0;
    // No single hit from a course defence takes more than DEFENCE_HIT_MAX percent of a
    // racer's full health, so a racer at full health survives one obelisk blast.
    if (!M.RallyDamage && (!source || !Racer_Of(source))) {
        int cap = (int)victim->Techno_Type_Class()->MaxStrength * DEFENCE_HIT_MAX / 100;
        if (cap < 1) cap = 1;
        if (damage > cap) damage = cap;
    }
    return damage;
}

void Rally_Damaged(TechnoClass* victim, int damage, int warhead, TechnoClass* source)
{
    if (!Rally_Active() || damage <= 0) return;
    Racer* r = Racer_Of(victim);
    if (!r || (r->Flags & RALLY_F_DEAD)) return;
    r->LastHurt = M.RaceTicks;
    // Rally's own damage (walls, rams, items) is not a defence's.
    const Racer* shooter = source ? Racer_Of(source) : nullptr;
    if (!M.RallyDamage && !shooter) {
        ++M.Stats[STAT_DEFENCE_HITS];
        M.Stats[STAT_DEFENCE_DAMAGE] += damage;
    }
    if (!M.RallyDamage && shooter && shooter != r) ++M.Stats[STAT_RACER_HITS];
    if (M.RallyDamage || victim->Strength <= 0) return;
    // A heavy engine warhead throws the vehicle.
    if (warhead == WARHEAD_HE || warhead == WARHEAD_PB || (warhead == WARHEAD_AP && damage >= 50)) {
        Throw_Racer(*r, damage, 50);
    }
}

void Rally_Kill(TechnoClass* victim, TechnoClass* source)
{
    if (!Rally_Active()) return;
    Racer* r = Racer_Of(victim);
    if (!r) return;
    Racer* killer = source ? Racer_Of(source) : nullptr;
    if (!killer && !M.RallyDamage && M.Phase == RALLY_PHASE_RACE) ++M.Stats[STAT_DEFENCE_KILLS];
    Mark_Dead(*r);
    if (killer && killer != r) ++killer->Kills;
}

bool Rally_Mortal(const ObjectClass* object)
{
    if (!Rally_Active() || !object || !object->Is_Techno()) return false;
    return Racer_Of((const TechnoClass*)object) != nullptr;
}

bool Rally_Crate(FootClass* object, CELL cell)
{
    if (!Rally_Active()) return false;
    Racer* r = Racer_Of(object);
    return r && Collect_Crate(*r, cell);
}

bool Rally_Full_Power(const HouseClass* house)
{
    return Rally_Active() && house && house->Class->House == M.Hostile;
}

int Rally_Knows(const HouseClass* house, const TechnoClass* object)
{
    if (!Rally_Active() || !house || house->Class->House != M.Hostile) return -1;
    // The hostile house sees every racer on the course, except a cloaked or finished one;
    // about anything else the skirmish rule decides as before.
    const Racer* r = Racer_Of(object);
    if (!r) return -1;
    return r->Cloak > 0 || (r->Flags & RALLY_F_FINISHED) ? 0 : 1;
}

bool Rally_Ion_Anim()
{
    return Rally_Active();
}

// The engine's rounds fly to a fixed point, and a racer covers a cell in a fraction of a
// second, so a round fired at a racer is aimed at where the racer will be on impact if
// it holds its speed and line. It still misses a racer that brakes or turns.
COORDINATE Rally_Lead(TARGET target, const TechnoClass* firer, int bullet, COORDINATE from, COORDINATE to)
{
    if (!Rally_Active() || !M.Started || bullet < 0 || bullet >= BULLET_COUNT) return to;
    const Racer* r = Racer_Of(As_Techno(target));
    if (!r || !Racer_Alive(*r)) return to;
    const BulletTypeClass& type = BulletTypeClass::As_Reference((BulletType)bullet);
    // The round's speed in leptons per tick, as BulletClass::Unlimbo and
    // TechnoClass::Fire_At will set it.
    int speed = type.MaxSpeed;
    if (OpenCNC3D_FastShells && bullet == BULLET_APDS && firer && firer->What_Am_I() == RTTI_UNIT) {
        speed = speed * 2 < MPH_LIGHT_SPEED - 1 ? speed * 2 : MPH_LIGHT_SPEED - 1;
    }
    int ticks = 1;
    int32_t px = r->X, py = r->Y;
    if (speed != MPH_LIGHT_SPEED) {
        // Two passes: the flight time to where the racer is, then to where it will be.
        for (int pass = 0; pass < 2; ++pass) {
            const int distance = Distance(from, XY_Coord(px >> 8, py >> 8));
            int s = speed;
            if (type.IsArcing) s = speed + (distance >> 5) > 25 ? speed + (distance >> 5) : 25;
            ticks = s > 0 ? (distance - 16 + s - 1) / s : 1;
            if (ticks < 1) ticks = 1;
            if (ticks > 60) ticks = 60;
            px = (int32_t)(r->X + (int64_t)r->VX * ticks / 256);
            py = (int32_t)(r->Y + (int64_t)r->VY * ticks / 256);
        }
    }
    // A round fired during the engine's own logic lands before the racers next move; one
    // a racer fires (inside Rally_Tick) lands after they have moved once more.
    const int moves = M.InTick ? ticks : ticks - 1;
    px = (int32_t)(r->X + (int64_t)r->VX * moves / 256);
    py = (int32_t)(r->Y + (int64_t)r->VY * moves / 256);
    const int32_t limit = MAP_CELL_W * FINE - 1;
    px = px < 0 ? 0 : px > limit ? limit : px;
    py = py < 0 ? 0 : py > limit ? limit : py;
    return XY_Coord(px >> 8, py >> 8);
}

// A racer is a vehicle of real size, so an explosion is measured from the edge of it.
int Rally_Hit_Distance(const ObjectClass* object, int distance)
{
    if (!Rally_Active() || !object || object->What_Am_I() != RTTI_UNIT) return distance;
    const Racer* r = Racer_Of((const TechnoClass*)object);
    if (!r) return distance;
    const int reach = (int)((int64_t)Kart_Of(*r).Radius * 256 / 100) + 256 / 5;
    return distance > reach ? distance - reach : 0;
}

// An obelisk keeps charging, and keeps its charge, while a racer it could fire on is
// within its range and two cells more.
bool Rally_Obelisk_Hold(const BuildingClass* building)
{
    if (!Rally_Active() || !building || !M.Started || building->Owner() != M.Hostile) return false;
    const int64_t reach = (int64_t)Weapons[building->Class->Primary].Range * 256 + (int64_t)FINE * 2;
    const COORDINATE c = building->Center_Coord();
    const int64_t bx = (int64_t)Coord_X(c) * 256, by = (int64_t)Coord_Y(c) * 256;
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& r = M.Racers[i];
        if (!Racer_Alive(r) || r.Cloak > 0 || (r.Flags & (RALLY_F_FINISHED | RALLY_F_RETIRED))) continue;
        const int64_t dx = r.X - bx, dy = r.Y - by;
        if (dx * dx + dy * dy <= reach * reach) return true;
    }
    return false;
}

// The obelisk charges in four stages, which take four seconds in the original game; in
// a race the four take a little over a second.
int Rally_Obelisk_Rate(int rate)
{
    if (!Rally_Active()) return rate;
    const int stage = (Ticks(12) + 2) / 4;
    return stage < 1 ? 1 : stage;
}

bool Rally_Skip_House_AI(const HouseClass* house)
{
    return Rally_Active() && house && Is_Seat(house->Class->House);
}

/* ---------------------------------------------------------------- orders ---- */

void Rally_Event_Pick(HouseClass* house, int roster_index)
{
    if (!Rally_Active() || !house || !M.Loaded) return;
    Ensure_Started();
    if (M.Phase != RALLY_PHASE_PICK) return;
    Racer* r = Racer_Of_House(house->Class->House);
    if (!r || r->Picked || roster_index < 0 || roster_index >= KartCount) return;
    r->Kart = roster_index;
    r->Picked = true;
}

void Rally_Event_Choose(HouseClass* house, int choice)
{
    if (!Rally_Active() || !house || !M.Loaded || !M.Started) return;
    if (M.Phase != RALLY_PHASE_DONE || M.Choice != RALLY_CHOICE_NONE) return;
    if (choice != RALLY_CHOICE_RETRY && choice != RALLY_CHOICE_END) return;
    // Only the deciding player's choice counts, so every peer acts on the same one.
    const Racer* r = Racer_Of_House(house->Class->House);
    if (!r || Index_Of(*r) != Find_Decider()) return;
    M.Choice = choice;
}

void Rally_Event_Input(HouseClass* house, uint32_t bits, int32_t aim_x, int32_t aim_y)
{
    if (!Rally_Active() || !house || !M.Loaded) return;
    Ensure_Started();
    Racer* r = Racer_Of_House(house->Class->House);
    if (!r) return;
    // Item and rescue are edge triggered; a press is kept until the next tick uses it. A
    // rescue is a press made in the race: one made on the grid is dropped, not kept for GO.
    if ((bits & RALLY_IN_ITEM) && !(r->Bits & RALLY_IN_ITEM)) r->ItemLatch = true;
    if ((bits & RALLY_IN_RESCUE) && !(r->Bits & RALLY_IN_RESCUE) && M.Phase == RALLY_PHASE_RACE) {
        r->RescueLatch = true;
    }
    r->Bits = bits & 0xFF;
    const int32_t limit = MAP_CELL_W * FINE - 1;
    r->AimX = aim_x < 0 ? 0 : aim_x > limit ? limit : aim_x;
    r->AimY = aim_y < 0 ? 0 : aim_y > limit ? limit : aim_y;
}

void Rally_Event_Autopilot(HouseClass* house, int on)
{
    if (!Rally_Active() || !house || !M.Loaded) return;
    Ensure_Started();
    Racer* r = Racer_Of_House(house->Class->House);
    if (r) r->Autopilot = on != 0;
}

/* --------------------------------------------------------------- exports ---- */

static void Export_Racer(const Racer& r, RallyRacer& o)
{
    o.house = r.House;
    UnitClass* unit = Racer_Unit(r);
    o.unit_id = unit && !(r.Flags & RALLY_F_DEAD) ? Units.ID(unit) : -1;
    // In PICK a seat's vehicle shows once it is chosen; a retry's preselection is last_vehicle.
    o.unit_type = r.Kart >= 0 && (r.Picked || M.Phase != RALLY_PHASE_PICK) ? Kart_Of(r).Type : -1;
    o.x = r.X;
    o.y = r.Y;
    o.z = r.Z;
    o.vx = r.VX >> 8;
    o.vy = r.VY >> 8;
    o.vz = r.VZ >> 8;
    o.yaw = (r.Yaw >> 8) & 0xFFFF;
    o.turret = (r.Turret >> 8) & 0xFFFF;
    o.pitch = (int16_t)(r.Pitch >> 8);
    o.roll = (int16_t)(r.Roll >> 8);
    o.speed = r.Speed >> 8;
    o.top_speed = r.Kart >= 0 ? Velocity(Kart_Of(r).Top) >> 8 : 0;
    o.health = unit ? unit->Strength : 0;
    o.health_max = r.Kart >= 0 ? UnitTypeClass::As_Reference((UnitType)Kart_Of(r).Type).MaxStrength : 0;
    o.place = r.Place;
    o.progress = r.Progress;
    o.checkpoint = r.Checkpoint;
    o.finish_ticks = r.FinishTicks;
    o.item = r.Item;
    o.item_count = r.ItemCount;
    o.flags = r.Flags;
    o.flip_ticks = r.FlipTicks;
    o.respawn_ticks = (r.Flags & RALLY_F_DEAD) ? r.Dead : 0;
    int effect = r.Boost;
    if (r.Armour > effect) effect = r.Armour;
    if (r.Cloak > effect) effect = r.Cloak;
    if (r.Spin > effect) effect = r.Spin;
    o.effect_ticks = effect;
    o.kills = r.Kills;
    o.lap = r.LapsDone < M.Laps ? r.LapsDone + 1 : M.Laps;
    o.lap_progress = r.LapProgress;
    o.last_vehicle = r.LastKart;
}

int Rally_Get_State(HousesType local, RallyState* out)
{
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    out->version = RALLY_ABI_VERSION;
    out->local = -1;
    out->winner = -1;
    out->decider = -1;
    out->laps = 1;
    out->race_number = 1;
    if (!Rally_Active() || !M.Loaded) {
        out->phase = RALLY_PHASE_OFF;
        return 0;
    }
    out->phase = M.Started ? M.Phase : RALLY_PHASE_PICK;
    out->phase_ticks = M.PhaseTicks;
    out->race_ticks = M.RaceTicks;
    out->ticks_per_second = (Tps10() + 5) / 10;
    out->racer_count = M.RacerCount;
    for (int i = 0; i < M.RacerCount; ++i) {
        if (M.Racers[i].House == local) out->local = i;
        Export_Racer(M.Racers[i], out->racer[i]);
    }
    out->finished_count = M.FinishedCount;
    for (int i = 0; i < M.FinishedCount && i < RALLY_MAX_RACERS; ++i) out->finish_order[i] = M.FinishOrder[i];
    out->crate_count = M.CrateCount;
    for (int i = 0; i < M.CrateCount; ++i) {
        out->crate[i].x = M.Crates[i].X;
        out->crate[i].y = M.Crates[i].Y;
        out->crate[i].active = M.Crates[i].Active ? 1 : 0;
    }
    out->hazard_count = M.HazardCount;
    for (int i = 0; i < M.HazardCount; ++i) {
        const Hazard& h = M.Hazards[i];
        RallyHazard& o = out->hazard[i];
        o.kind = h.Kind;
        o.x = h.X;
        o.y = h.Y;
        o.z = h.Z;
        o.yaw = (h.Yaw >> 8) & 0xFFFF;
        o.owner = h.Owner;
        o.ticks = h.Ticks;
    }
    out->winner = M.Winner;
    out->laps = M.Laps;
    out->course_loop = M.Loop ? 1 : 0;
    out->race_number = M.Started ? M.RaceNumber : 1;
    out->decider = M.Started ? M.Decider : -1;
    out->choice = M.Choice;
    return 1;
}

// One engine tick of a copy of a player's racer, as the race would drive it from these
// controls with nobody else about: the countdowns, the controls as the order would set
// them, the turret, a nitro the player fires, and the driving itself. Every call here
// changes only the copy and a scratch set of counters.
static void Predict_Tick(Racer& c, const RallyPredictStep& step, int* stats)
{
    const uint32_t bits = step.bits & 0xFF;
    const bool item_pressed = (bits & RALLY_IN_ITEM) && !(c.Bits & RALLY_IN_ITEM);
    c.Bits = bits;
    const int32_t limit = MAP_CELL_W * FINE - 1;
    c.AimX = step.aim_x < 0 ? 0 : step.aim_x > limit ? limit : step.aim_x;
    c.AimY = step.aim_y < 0 ? 0 : step.aim_y > limit ? limit : step.aim_y;
    Racer_Timers(c);
    Aim_Turret(c, c.AimX, c.AimY);
    if (item_pressed && c.Roulette == 0 && (c.Item == RALLY_ITEM_NITRO || c.Item == RALLY_ITEM_NITRO3)) {
        Nitro_Burst(c);
        if (--c.ItemCount <= 0) {
            c.Item = RALLY_ITEM_NONE;
            c.ItemCount = 0;
        }
    }
    Drive_Begin(c);
    for (int step_index = 0; step_index < SUBSTEPS; ++step_index) Drive_Substep(c, stats);
    Drive_End(c, stats);
    const UnitClass* unit = Racer_Unit(c);
    if (unit && !unit->Class->IsTurretEquipped) c.Turret = c.Yaw;
}

int Rally_Predict(HousesType house, const RallyPredictStep* steps, int n, RallyRacer* out)
{
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    if (!Rally_Active() || !M.Loaded || !M.Started) return 0;
    const Racer* live = Racer_Of_House(house);
    if (!live) return 0;
    Export_Racer(*live, *out);
    if (M.Phase != RALLY_PHASE_RACE || !Racer_Alive(*live) || !live->Human || live->Autopilot) return 0;
    if (live->Flags & (RALLY_F_FLIPPED | RALLY_F_RIGHTING | RALLY_F_SPINNING | RALLY_F_RETIRED | RALLY_F_FINISHED)) {
        return 0;
    }
    if (live->Spin > 0) return 0;
    if (!steps || n < 0) n = 0;
    if (n > RALLY_PREDICT_MAX) n = RALLY_PREDICT_MAX;
    Racer copy = *live;
    int stats[STAT_COUNT];
    for (int i = 0; i < n; ++i) Predict_Tick(copy, steps[i], stats);
    // The flags the race would show for the copy, the way Update_Flags builds them.
    int f = copy.Flags & ~(RALLY_F_AIRBORNE | RALLY_F_BOOST | RALLY_F_ARMOUR | RALLY_F_CLOAK | RALLY_F_SPINNING
                           | RALLY_F_SHIELDED);
    if (copy.Z > 0 && !(f & RALLY_F_RIGHTING)) f |= RALLY_F_AIRBORNE;
    if (copy.Boost > 0) f |= RALLY_F_BOOST;
    if (copy.Cloak > 0) f |= RALLY_F_CLOAK;
    if (copy.Armour > 0) f |= RALLY_F_ARMOUR;
    if (copy.Spin > 0) f |= RALLY_F_SPINNING;
    if (copy.Shield > 0) f |= RALLY_F_SHIELDED;
    copy.Flags = f;
    Export_Racer(copy, *out);
    return 1;
}

int Rally_Get_Course(RallyCoursePoint* out, int max)
{
    if (!M.Loaded) return 0;
    for (int i = 0; out && i < M.PointCount && i < max; ++i) {
        out[i].x = M.Point[i].X;
        out[i].y = M.Point[i].Y;
        out[i].half_width = M.Point[i].HalfWidth;
    }
    return M.PointCount;
}

int Rally_Get_Roster(int32_t* out, int max)
{
    for (int i = 0; out && i < KartCount && i < max; ++i) out[i] = Karts[i].Type;
    return KartCount;
}

int Rally_Roster_Count()
{
    return KartCount;
}

bool Rally_Roster_Valid(int index)
{
    return index >= 0 && index < KartCount;
}

uint32_t Rally_Hash()
{
    if (!Rally_Active() || !M.Loaded) return 0;
    uint32_t hash = 2166136261u;
    const auto mix = [&hash](int64_t value) {
        hash = (hash ^ (uint32_t)value) * 16777619u;
        hash = (hash ^ (uint32_t)(value >> 32)) * 16777619u;
    };
    mix(M.Phase);
    mix(M.PhaseTicks);
    mix(M.RaceTicks);
    mix(M.Winner);
    mix(M.FinishedCount);
    mix(M.AfterWinner);
    mix(M.Laps);
    mix(M.Loop);
    mix(M.RaceNumber);
    mix(M.Decider);
    mix(M.Choice);
    mix(M.AutoEnd);
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& r = M.Racers[i];
        const int64_t values[] = {r.House,      r.Kart,       r.Unit,        r.X,          r.Y,         r.Z,
                                  r.VX,         r.VY,         r.VZ,          r.Yaw,        r.Turret,    r.Pitch,
                                  r.Roll,       r.PitchRate,  r.RollRate,    r.SpinRate,   r.Bits,      r.AimX,
                                  r.AimY,       r.Autopilot,  r.Checkpoint,  r.Segment,    r.Progress,  r.Place,
                                  r.FinishTicks, r.Flags,     r.Item,        r.ItemCount,  r.Kills,     r.DriftCharge,
                                  r.Boost,      r.BoostTop,   r.Armour,      r.Cloak,      r.Spin,      r.Shield,
                                  r.Roulette,   r.Dead,       r.FlipTicks,   r.Righting,   r.WrongWay,  r.RescueCooldown,
                                  r.AiOffset,   r.AiReverse,  r.AiStuck,     r.AiTarget,   r.Human,     r.Skill,
                                  r.AiOffsetTimer, r.AiSlow,  r.AiItemTicks, r.AiTargetTimer, r.LastHurt, r.Reload,
                                  r.BoostAccel, r.RightingRoll, r.RightingPitch, r.DriftWasOn, r.DriftDir,
                                  r.ItemLatch,  r.RescueLatch,  r.AiDrift,     r.AiDriftPlan, r.AiDriftHold,
                                  r.AiDriftLate, r.Dist,        r.LapsDone,    r.LapProgress, r.Picked,
                                  r.LastKart,   r.GridSlot};
        for (int64_t v : values) mix(v);
        const UnitClass* unit = Racer_Unit(r);
        mix(unit ? (int64_t)unit->Strength : -1);
        mix(unit ? (int64_t)unit->Coord : -1);
        mix(unit ? (int64_t)unit->Arm : -1);
        mix(unit ? (int64_t)unit->Ammo : -1);
    }
    for (int i = 0; i < M.CrateCount; ++i) {
        mix(M.Crates[i].Active);
        mix(M.Crates[i].Return);
    }
    mix(M.FixtureCount);
    for (int i = 0; i < M.FixtureCount; ++i) mix(M.Fixtures[i].Live);
    mix(M.HazardCount);
    for (int i = 0; i < M.HazardCount; ++i) {
        const Hazard& h = M.Hazards[i];
        mix(h.Kind);
        mix(h.X);
        mix(h.Y);
        mix(h.Z);
        mix(h.Yaw);
        mix(h.Owner);
        mix(h.Ticks);
        mix(h.Target);
        mix(h.Bounces);
        mix(h.Segment);
    }
    // The synchronised random stream the race draws from.
    mix((int64_t)Scen.RandomNumber.Seed);
    return hash;
}

// Test hooks for headless runs. They change the simulation directly, outside the
// order queue, so they are for single machine tests only, never a lockstep match.
int Rally_Debug(int op, int a, int b, int c)
{
    switch (op) {
    case RALLY_DEBUG_SEED: // seed the synchronised random stream when the next Rally scenario loads
        M.DebugSeed = (unsigned)a;
        M.DebugSeedSet = true;
        return 1;
    case RALLY_DEBUG_STAT: // a counter
        return a >= 0 && a < STAT_COUNT ? M.Stats[a] : 0;
    default:
        break;
    }
    if (!Rally_Active() || !M.Loaded) return -1;
    const bool per_racer = op < RALLY_DEBUG_HOSTILE_POWER || op == RALLY_DEBUG_RAM_WALL || op == RALLY_DEBUG_BITS;
    if (per_racer && !Racer_Index_Valid(a)) return -1;
    switch (op) {
    case RALLY_DEBUG_THROW: { // throw racer a up at b cells/s x100 with c degrees per second of roll
        Racer& r = M.Racers[a];
        if (!Racer_Alive(r)) return 0;
        r.VZ = Velocity(b);
        r.Z = 1;
        r.RollRate = Turn_Rate(c);
        r.Flags |= RALLY_F_AIRBORNE;
        return 1;
    }
    case RALLY_DEBUG_KILL: { // destroy racer a through the engine's damage path
        Racer& r = M.Racers[a];
        UnitClass* unit = Racer_Unit(r);
        if (!unit) return 0;
        int damage = unit->Strength * 8 + 100;
        M.DebugIgnoreShield = true;
        unit->Take_Damage(damage, 0, WARHEAD_LASER, nullptr);
        M.DebugIgnoreShield = false;
        return 1;
    }
    case RALLY_DEBUG_GIVE: // give racer a item b with c uses
        if (b <= RALLY_ITEM_NONE || b >= RALLY_ITEM_COUNT) return 0;
        M.Racers[a].Item = b;
        M.Racers[a].ItemCount = c > 0 ? c : 1;
        M.Racers[a].Roulette = 0;
        return 1;
    case RALLY_DEBUG_CHECK: { // engine state checks for racer a, as bits
        const Racer& r = M.Racers[a];
        const UnitClass* unit = Racer_Unit(r);
        if (!unit) return 0;
        int bits = 1;
        const CELL cell = Coord_Cell(unit->Coord);
        if (Cell_Blocked(cell, &r)) bits |= 2;
        if (!Map.In_Radar(cell)) bits |= 4;
        const int lx = Coord_X(unit->Coord), ly = Coord_Y(unit->Coord);
        if (abs(lx - (r.X >> 8)) > 1 || abs(ly - (r.Y >> 8)) > 1) bits |= 8;
        return bits;
    }
    case RALLY_DEBUG_RAM_WALL: { // aim racer a at the nearest blocked cell and send it there at b cells/s x100
        Racer& r = M.Racers[a];
        if (!Racer_Alive(r) || b <= 0) return 0;
        const int cx = r.X >> 16, cy = r.Y >> 16;
        int best = -1, bx = 0, by = 0;
        for (int dy = -6; dy <= 6; ++dy) {
            for (int dx = -6; dx <= 6; ++dx) {
                const int x = cx + dx, y = cy + dy;
                if (x < 0 || y < 0 || x >= MAP_CELL_W || y >= MAP_CELL_H || !Cell_Blocked(XY_Cell(x, y), &r)) continue;
                const int d = dx * dx + dy * dy;
                if (best < 0 || d < best) {
                    best = d;
                    bx = x;
                    by = y;
                }
            }
        }
        if (best < 0) return 0;
        const int32_t dir = Angle_Of((int64_t)bx * FINE + FINE / 2 - r.X, (int64_t)by * FINE + FINE / 2 - r.Y);
        const int32_t v = Velocity(b);
        r.Yaw = dir << 8;
        r.VX = (int32_t)(((int64_t)v * Sin16(dir)) >> 16);
        r.VY = (int32_t)(-((int64_t)v * Cos16(dir)) >> 16);
        return 1;
    }
    case RALLY_DEBUG_PICK: // the vehicle of seat a, during PICK
        if (b < 0 || b >= KartCount) return 0;
        Ensure_Started();
        if (M.Phase != RALLY_PHASE_PICK || !Racer_Index_Valid(a)) return 0;
        M.Racers[a].Kart = b;
        M.Racers[a].Picked = true;
        return 1;
    case RALLY_DEBUG_STRAY_CRATES: { // crates on the map away from every course crate spot
        int count = 0;
        for (int cell = 0; cell < MAP_CELL_TOTAL; ++cell) {
            const OverlayType o = Map[(CELL)cell].Overlay;
            if (o != OVERLAY_WOOD_CRATE && o != OVERLAY_STEEL_CRATE) continue;
            bool spot = false;
            for (int i = 0; i < M.CrateCount && !spot; ++i) spot = M.Crates[i].Cell == (CELL)cell;
            if (!spot) ++count;
        }
        return count;
    }
    case RALLY_DEBUG_BREAK: { // the a-th live hostile building takes b percent of its full strength
        int seen = 0;
        for (int i = 0; i < Buildings.Count(); ++i) {
            BuildingClass* b2 = Buildings.Ptr(i);
            if (!b2 || !b2->IsActive || b2->IsInLimbo || b2->Strength <= 0 || b2->Owner() != M.Hostile) continue;
            if (seen++ != a) continue;
            int damage = (int)b2->Class->MaxStrength * (b > 0 ? b : 1) / 100;
            if (damage < 1) damage = 1;
            if (b >= 100) damage = (int)b2->Strength * 8 + 100;
            b2->Take_Damage(damage, 0, WARHEAD_HE, nullptr);
            return 1;
        }
        return 0;
    }
    case RALLY_DEBUG_FIXTURES: // a 0: course objects recorded at the start; 1: of those, standing as they started
        return a == 0 ? M.FixtureCount : Fixtures_Intact();
    case RALLY_DEBUG_BITS: // the controls racer a drives with now
        return (int)M.Racers[a].Bits;
    case RALLY_DEBUG_STRENGTH: { // engine strength of racer a
        const UnitClass* unit = Racer_Unit(M.Racers[a]);
        return unit ? unit->Strength : -1;
    }
    case RALLY_DEBUG_HOSTILE_POWER: { // does the hostile house have full power
        const HouseClass* house = HouseClass::As_Pointer(M.Hostile);
        return house ? (house->Power_Fraction() >= 1 ? 1 : 0) : -1;
    }
    case RALLY_DEBUG_HOSTILE_BUILDINGS: { // live hostile buildings
        int count = 0;
        for (int i = 0; i < Buildings.Count(); ++i) {
            const BuildingClass* b = Buildings.Ptr(i);
            if (b && b->IsActive && !b->IsInLimbo && b->Strength > 0 && b->Owner() == M.Hostile) ++count;
        }
        return count;
    }
    case RALLY_DEBUG_HOSTILE_AIMING: { // hostile objects whose target is a racer
        int count = 0;
        const auto look = [&](const TechnoClass* t) {
            if (t && t->IsActive && !t->IsInLimbo && t->Strength > 0 && t->Owner() == M.Hostile
                && Racer_Of(As_Techno(t->TarCom))) {
                ++count;
            }
        };
        for (int i = 0; i < Buildings.Count(); ++i) look(Buildings.Ptr(i));
        for (int i = 0; i < Units.Count(); ++i) look(Units.Ptr(i));
        for (int i = 0; i < Infantry.Count(); ++i) look(Infantry.Ptr(i));
        return count;
    }
    case RALLY_DEBUG_HOSTILE_SHOTS:   // shots fired by the hostile house so far (call once per tick)
    case RALLY_DEBUG_HOSTILE_SHOTS_BY: { // of those, fired by hostile buildings of type a
        // A shot sets the firer's rearm count; seen rising between two calls, it was a shot.
        static int last_arm[3][512];
        static int shots_by_type[STRUCT_COUNT];
        static int shots;
        static int frame = -1;
        if (Frame < frame) {
            memset(last_arm, 0, sizeof last_arm);
            memset(shots_by_type, 0, sizeof shots_by_type);
            shots = 0;
        }
        if (Frame != frame) {
            frame = Frame;
            const auto look = [&](int heap, int index, const TechnoClass* t) {
                if (index >= 512) return;
                const int arm = t && t->IsActive && !t->IsInLimbo && t->Owner() == M.Hostile ? (int)t->Arm : 0;
                if (arm > last_arm[heap][index]) {
                    ++shots;
                    if (heap == 0) ++shots_by_type[((const BuildingClass*)t)->Class->Type];
                }
                last_arm[heap][index] = arm;
            };
            for (int i = 0; i < Buildings.Count(); ++i) look(0, Buildings.ID(Buildings.Ptr(i)), Buildings.Ptr(i));
            for (int i = 0; i < Units.Count(); ++i) look(1, Units.ID(Units.Ptr(i)), Units.Ptr(i));
            for (int i = 0; i < Infantry.Count(); ++i) look(2, Infantry.ID(Infantry.Ptr(i)), Infantry.Ptr(i));
        }
        if (op == RALLY_DEBUG_HOSTILE_SHOTS_BY) return a >= 0 && a < STRUCT_COUNT ? shots_by_type[a] : 0;
        return shots;
    }
    default:
        return -1;
    }
}
