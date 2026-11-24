// OpenCNC3D Tiberian Dawn Rally: state shared by the Rally source files.
//
// rally.cpp owns the match (phases, seats, course, crates, progress, the exports),
// rally_physics.cpp moves the vehicles, rally_items.cpp runs the items and the
// hazards they leave on the course, and rally_ai.cpp drives the computer seats.
// Nothing outside those four files includes this header; the engine sees rally.h.
//
// Everything here is simulation state and is computed in integers only, so every
// peer of a lockstep match arrives at the same race from the same orders.
#ifndef TD_RALLY_INTERNAL_H
#define TD_RALLY_INTERNAL_H

#include <stdint.h>
#include "rally.h"
#include "../../../game/rally_abi.h"

namespace rally {

enum
{
    SUBSTEPS = 4,         // physics steps per engine tick
    FINE = 65536,         // fine units per cell
    MAX_GRID = 8,
    MAX_ROSTER = 16,
    STAT_COUNT = RALLY_STAT_COUNT,
    FIXED_CONE = 5461,    // a weapon without a turret fires within 30 degrees of the nose
    DEFENCE_HIT_MAX = 85, // percent of full health one hit from a course defence may take
    RACE_LIMIT = 1800,    // tenths of a second per lap: a race ends after three minutes a lap regardless
    MAX_FIXTURES = 512,   // course objects remembered at the start, for the reset of a retry
};

// Counters for tests and the report (RALLY_DEBUG_STAT); nothing in the race reads them.
enum
{
    STAT_CRATES = RALLY_STAT_CRATES,
    STAT_ITEMS = RALLY_STAT_ITEMS,
    STAT_HAZARDS = RALLY_STAT_HAZARDS,
    STAT_BLASTS = RALLY_STAT_BLASTS,
    STAT_RESPAWNS = RALLY_STAT_RESPAWNS,
    STAT_FLIPS = RALLY_STAT_FLIPS,
    STAT_RIGHTED = RALLY_STAT_RIGHTED,
    STAT_SHOTS = RALLY_STAT_SHOTS,
    STAT_RUN_OVER = RALLY_STAT_RUN_OVER,
    STAT_WALL_HITS = RALLY_STAT_WALL_HITS,
    STAT_RAMS = RALLY_STAT_RAMS,
    STAT_RESCUES = RALLY_STAT_RESCUES,
    STAT_THROWN = RALLY_STAT_THROWN,
    STAT_DEFENCE_HITS = RALLY_STAT_DEFENCE_HITS,
    STAT_RACER_HITS = RALLY_STAT_RACER_HITS,
    STAT_DEFENCE_DAMAGE = RALLY_STAT_DEFENCE_DAMAGE,
    STAT_DEFENCE_KILLS = RALLY_STAT_DEFENCE_KILLS,
    STAT_MINI_TURBOS = RALLY_STAT_MINI_TURBOS,
};

// Per vehicle handling. Speeds are cells per second x100, angles degrees per second,
// grip and mass x100, the radius in cells x100.
struct KartStats
{
    int Type;
    int Top;
    int Accel;
    int Turn;
    int Grip;
    int Mass;
    int Radius;
};

struct Racer
{
    // Seat.
    HousesType House;
    int Seat;
    bool Human;
    bool Autopilot;
    int Skill;   // AI difficulty 0..3
    int Kart;    // index into the kart table, -1 until picked (a retry keeps the last one)
    bool Picked; // chosen in this PICK (computer seats choose at once)
    int LastKart; // the vehicle of the previous race of the match, -1 in the first
    int GridSlot; // the grid slot it started this race from, -1 before the grid is drawn
    TARGET Unit; // the engine unit, TARGET_NONE while dead or not yet spawned

    // Motion. Positions in fine units (1/256 lepton); velocities in fine units per
    // engine tick with 8 extra fraction bits; angles are 16-bit binary angles with
    // 8 extra fraction bits (a full turn is 1 << 24).
    int32_t X, Y, Z;
    int32_t VX, VY, VZ;
    int32_t Yaw, Turret;
    int32_t Pitch, Roll;
    int32_t PitchRate, RollRate, SpinRate;
    int32_t Speed;    // forward speed of the last step, same units as VX
    int32_t Impact;   // hardest wall impact this tick, same units
    int32_t PrevX, PrevY;

    // Controls (the last RALLY_INPUT of a human seat, or the AI's choice).
    uint32_t Bits;
    int32_t AimX, AimY;
    bool ItemLatch, RescueLatch;
    bool DriftWasOn;

    // Race.
    int Checkpoint; // last course point passed; on a circuit, in the lap being driven
    int Segment;    // course segment the projection lies on
    int Progress;   // 0..65536 over the whole race
    int LapProgress; // 0..65536 over the lap being driven
    int LapsDone;   // laps completed (crossings of the line that counted)
    int64_t Dist;   // fine units along the course from the start line, counted on over
                    // every lap and negative behind the line; on a point-to-point course the
                    // projection itself (negative behind the start line as well)
    int Place;
    int FinishTicks;
    int Flags;
    int Item, ItemCount;
    int Kills;
    int DriftCharge;
    int DriftDir;     // -1 left, 1 right: the way a drift turns, fixed when it starts
    int Boost, BoostTop, BoostAccel;
    int Armour, Cloak, Spin, Shield;
    int Roulette;
    int Dead;
    int FlipTicks, Righting;
    int32_t RightingRoll, RightingPitch;
    int WrongWay;
    int RescueCooldown;
    int Reload;
    int LastHurt; // race tick of the last damage, for the cloak decision

    // AI driver.
    int32_t AiOffset;
    int AiOffsetTimer;
    int AiReverse;
    int AiStuck;
    int AiSlow;
    int AiItemTicks; // how long the AI has held its item
    TARGET AiTarget; // what the AI shoots at
    int AiTargetTimer;
    int AiDrift;     // ticks the AI has held its current drift, 0 when it is not drifting
    int AiDriftPlan; // the bend ahead: 0 none, +-1 to be drifted, +-2 driven through (sign: the way it turns)
    int AiDriftHold; // ticks spent holding the line into a bend before the drift starts
    int AiDriftLate; // ticks a charged drift is still held before letting go, 0 until that is due
};

struct CoursePoint
{
    int32_t X, Y, HalfWidth; // fine units
    int64_t Along;           // fine units of course length from point 0
};

struct Crate
{
    int32_t X, Y;
    CELL Cell;
    bool Active;
    int Return; // ticks until it comes back
};

struct Hazard
{
    int Kind;
    int32_t X, Y, Z;
    int32_t Yaw;   // 16-bit << 8
    int Owner;     // racer index
    int Ticks;     // age, or ticks left for an ion mark
    int Target;    // racer index for homing and ion, -1 none
    int Bounces;
    int Segment;   // course segment a homing missile is following
};

// An object of the course as it stood when the match started (a defence, a building, a
// unit or a soldier of a house that is not a seat, or a tree), so that a retry can put the
// course back the way it was.
enum
{
    FIXTURE_BUILDING,
    FIXTURE_UNIT,
    FIXTURE_INFANTRY,
    FIXTURE_TERRAIN,
};

struct Fixture
{
    int Kind;          // FIXTURE_*
    int Type;          // StructType, UnitType, InfantryType or TerrainType
    HousesType House;
    COORDINATE Coord;  // where it stood (a tree: the top left of its cell)
    int Facing;        // DirType it faced
    int Strength;
    TARGET Live;       // the object standing for it now, TARGET_NONE once it is gone
};

struct Match
{
    bool Enabled;
    bool Loaded;   // a [Rally] section was read for this scenario
    bool Started;  // the first Rally tick has set the match up
    bool Over;     // the result has been handed to the engine
    int TicksPerSecondX10;
    int ConfigLaps; // laps asked for before the match (OpenCNC3D_Rally_Configure)

    char Name[64];
    HousesType Hostile;
    bool Loop;     // a circuit: the last point joins the first and the line is crossed every lap
    int Laps;      // laps in a race (1 on a point-to-point course)
    int PointCount;
    CoursePoint Point[RALLY_MAX_COURSE];
    int64_t Length; // one lap (on a circuit, the closing segment included)
    int GridFacing;
    int GridCount;
    int32_t GridX[MAX_GRID], GridY[MAX_GRID];
    int32_t FinishX1, FinishY1, FinishX2, FinishY2;
    int CrateCount;
    Crate Crates[RALLY_MAX_CRATES];

    int Phase;
    int PhaseTicks;
    int RaceTicks;
    int AfterWinner;
    int RacerCount;
    Racer Racers[RALLY_MAX_RACERS];
    int FinishedCount;
    int FinishOrder[RALLY_MAX_RACERS];
    int Winner;
    int HazardCount;
    Hazard Hazards[RALLY_MAX_HAZARDS];

    int RaceNumber; // 1 for the first race of the match, one more for every retry
    int Decider;    // racer index of the player whose choice ends the results, -1 none
    int Choice;     // RALLY_CHOICE_* of the deciding player, NONE while waiting
    int AutoEnd;    // ticks of results left before the match ends by itself with no decider

    int FixtureCount;
    Fixture Fixtures[MAX_FIXTURES];

    int Stats[STAT_COUNT];
    bool RallyDamage;     // Rally itself is applying this damage: no automatic throw
    bool InTick;          // inside Rally_Tick, where racers fire before they move
    bool DebugIgnoreShield;
    unsigned DebugSeed;
    bool DebugSeedSet;
};

extern Match M;
extern const KartStats Karts[MAX_ROSTER];
extern const int KartCount;

// Units and conversions (rally.cpp).
int32_t Velocity(int cells_per_second_x100);   // -> fine per tick << 8
int32_t Accel(int cells_per_second2_x100);     // -> (fine per tick << 8) per substep
int32_t Turn_Rate(int degrees_per_second);     // -> angle << 8 per substep
int32_t Decay(int per_second_x100);            // -> fraction of 65536 removed per substep
int Ticks(int tenths_of_second);
int32_t Sin16(int32_t angle);                  // 16-bit angle -> 16.16
int32_t Cos16(int32_t angle);
int32_t Angle_Of(int64_t dx, int64_t dy);      // vector -> 16-bit angle (0 north, clockwise)
int64_t Isqrt(uint64_t value);
int32_t Angle_Diff(int32_t to, int32_t from);  // signed 16-bit difference

// Racers and the engine (rally.cpp).
UnitClass* Racer_Unit(const Racer& r);
const KartStats& Kart_Of(const Racer& r);
bool Racer_Alive(const Racer& r);
bool Racer_Out(const Racer& r);        // the seat has left the match
bool Racer_Fair_Game(const Racer& r);  // alive and neither cloaked, shielded, finished nor out
int Racer_Near(const Racer& r, int32_t x, int32_t y, int32_t reach);
bool Racer_Index_Valid(int index);
int Racer_By_Place(int place);
void Damage_Racer(Racer& r, int damage, int source_racer);
void Throw_Racer(Racer& r, int damage, int scale_percent);
bool Teleport_To_Checkpoint(Racer& r);
void Racer_Timers(Racer& r);

// The course (rally.cpp). Segment s runs from point s to point s + 1; on a circuit the
// last segment runs from the last point back to point 0, and segment numbers wrap round.
int32_t Course_Direction(int segment);
int Course_Segment_Near(int32_t x, int32_t y, int hint, int64_t* along, int32_t* distance);
void Course_Point_At(int64_t along, int32_t* x, int32_t* y, int* segment); // along wraps on a circuit
int Course_Wrap_Segment(int segment);
int Course_Segments_Between(int from, int to); // segments forward from one to the other
int64_t Course_Delta(int64_t to, int64_t from); // signed distance along; the short way round a circuit

// Physics (rally_physics.cpp). One engine tick of one racer's driving is Drive_Begin, then
// SUBSTEPS of Drive_Substep, then Drive_End; the race interleaves the racers' substeps with
// their collisions, and OpenCNC3D_Rally_Predict runs the same three on a copy. stats is
// M.Stats in the race and a scratch array in a prediction, which changes nothing else.
bool Cell_Blocked(CELL cell, const Racer* r);
bool Point_Blocked(int32_t x, int32_t y, const Racer* r);
void Drive_Begin(Racer& r);
void Drive_Substep(Racer& r, int* stats);
void Drive_End(Racer& r, int* stats);
void Physics_Tick();
void Sync_Engine(Racer& r);
void Run_Over(Racer& r);

// Weapons (rally.cpp).
void Aim_Turret(Racer& r, int32_t tx, int32_t ty);
void Fire_Weapon(Racer& r, TARGET target, int32_t tx, int32_t ty, bool clamp);

// Items (rally_items.cpp).
void Items_Tick();
void Use_Item(Racer& r);
void Nitro_Burst(Racer& r);
int Item_Target(const Racer& r, int item);  // whom a homing missile or the ion cannon would go for
bool Collect_Crate(Racer& r, CELL cell);
void Place_Crate(Crate& c);

// AI (rally_ai.cpp).
void AI_Drive(Racer& r);
void AI_Fire(Racer& r);
void AI_Items(Racer& r);

} // namespace rally

#endif
