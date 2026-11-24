// OpenCNC3D Tiberian Dawn Gun Game abilities.
//
// A crate gives a player one held ability. The renderer runs the activation mini-game
// and queues GUN_GAME_ABILITY events through OpenCNC3D_Gun_Ability_Use; every peer then
// applies the ability here on the same frame. Computer players decide inside the
// simulation itself, so they need no events. All state lives in this file, is cleared
// with the match and is folded into Gun_Game_Hash, so a peer that disagrees about any
// of it is reported as a desync rather than drifting silently.
#include "function.h"
#include "gun_game.h"
#include "gun_game_ability.h"
#include "../../../game/gun_game_ability_abi.h"
#include "fair_ai_td.h"
#include <stdlib.h>

namespace {

constexpr int CellLeptons = 256;
constexpr int Never = -0x40000000;

// Durations in tenths of a real second (Gun_Game_Tenths turns them into ticks).
constexpr int StealthTime = 120;
constexpr int IronTime = 60;
constexpr int OverdriveTime = 100;
constexpr int VortexTime = 150;
constexpr int VortexPulse = 5;
constexpr int VortexTurn = 10;
constexpr int MineTime = 600;
constexpr int HunterTime = 100;
constexpr int ChainTime = 80;
constexpr int ExtraLifeTime = 300;
constexpr int SmokeTime = 100;
constexpr int SmokePuff = 5;
constexpr int DecoyTime = 200;
constexpr int DecoyWander = 20;
constexpr int GiantTime = 100;
constexpr int LastLaughTime = 150;
constexpr int PotatoFuse = 80;
constexpr int PotatoPass = 10;
constexpr int WormholeTime = 200;
constexpr int WormholeCooldown = 20;
constexpr int DanceTime = 30;
constexpr int MirrorTime = 60;
constexpr int AssemblyTime = 20;

// Ranges in cells.
constexpr int HopRange = 8;
constexpr int SwapRange = 10;
constexpr int MineRange = 4;
constexpr int MineCount = 5;
constexpr int DanceRange = 5;
constexpr int ChainRange = 3;
constexpr int ChainTargets = 2;
constexpr int DecoyCount = 3;

// Effect ages in ticks, as the contract states them.
constexpr int ArcAge = 8;
constexpr int AnnounceAge = 45;
constexpr int BlastAge = 30;

// Damage.
constexpr int VortexDamage = 30;
constexpr int MineDamage = 200;
constexpr int PotatoSplash = 150;
constexpr int LaughCentre = 250;
constexpr int LaughRing = 120;

constexpr int MaxVortices = 8;
constexpr int MaxMines = 64;
constexpr int MaxClouds = 8;
constexpr int MaxHoles = 8;
constexpr int MaxHops = 32;
constexpr int MaxArcs = 32;
constexpr int MaxNews = 16;
constexpr int MaxBlasts = 16;
constexpr int MaxPotatoes = 8;
constexpr int MaxBooby = 16;
constexpr int MaxDecoys = 24;
constexpr int MaxLaughs = 8;
constexpr int MaxShocks = 16;

struct Player
{
    int Held;
    int Until[GA_COUNT]; // frame each effect on this player ends
    CELL RespawnCell;    // Extra Life: where the player fell, -1 when unused
    // An activation is assembled from one GUN_GAME_ABILITY event and, for more than
    // four cells, GUN_GAME_ABILITY_CELL events; they may execute in either order.
    int Serial;
    unsigned Mask;
    int Ability;
    int Target;
    int Count;
    int Life;
    int Expire;
    CELL Cells[GA_MAX_CELLS];
    // Computer players.
    int ThinkAt;
    int OfferToken;
    int OfferAt;
    int UseAt;
    int UseSince;
    int StrikeAt;
    int RoamAt;
    CELL CrateCell;  // the crate being walked to
    int CrateSince;
    CELL SkipCrate;  // a crate given up on, until SkipUntil
    int SkipUntil;
};

struct Vortex
{
    int Owner;
    COORDINATE Coord;
    int Dir;
    int Until;
    int NextPulse;
    int NextTurn;
};

struct Mine
{
    int Owner;
    CELL Cell;
    int Until;
};

struct Cloud
{
    int Owner;
    int Until;
    int NextPuff;
    int Puff;
    int Count;
    CELL Cells[GA_MAX_CELLS];
};

struct Hole
{
    int Owner;
    CELL Ends[2];
    int Until;
};

struct Hop
{
    TARGET Unit;
    int Until;
};

/* Not "Arc": on Windows that is also a GDI drawing function, and the two names are
   ambiguous wherever the brain sees windows.h. */
struct ShockArc
{
    int Owner;
    COORDINATE From;
    COORDINATE To;
    int At;
};

struct News
{
    int Owner;
    int Ability;
    int At;
};

struct Blast
{
    int Owner;
    COORDINATE Where;
    int At;
};

struct Potato
{
    int Thrower;
    int Holder;
    int Until;
    int PassReady;
};

struct Booby
{
    int Owner;
    CELL Cell;
};

struct Decoy
{
    int Owner;
    TARGET Unit;
    int Until;
    int WanderAt;
};

struct Laugh
{
    int Owner;
    COORDINATE Where;
};

struct Shock
{
    int Owner;
    TARGET Victim;
    COORDINATE Where;
    int Damage;
};

// A booby crate taken by an enemy. The crate is found inside the engine's movement code
// (Start_Driver, the track and dodge steps), which goes on using the unit and reserving
// cells for it after Goodie_Check returns, so the blast waits for the next ability tick.
struct Trap
{
    int Owner;
    int Victim; // the house whose combatant took the crate
    TARGET Unit;
    unsigned Life;
};

Player Seats[HOUSE_COUNT];
Vortex Vortices[MaxVortices];
Mine Mines[MaxMines];
Cloud Clouds[MaxClouds];
Hole Holes[MaxHoles];
Hop Hops[MaxHops];
ShockArc Arcs[MaxArcs];
News Newsreel[MaxNews];
Blast Blasts[MaxBlasts];
Potato Potatoes[MaxPotatoes];
Booby Boobies[MaxBooby];
Decoy Decoys[MaxDecoys];
Laugh Laughs[MaxLaughs];
Shock Shocks[MaxShocks];
Trap Traps[MaxBooby];
int LaughCount;
int ShockCount;
int TrapCount;
int NextArc;
int NextNews;
int NextBlast;
bool Chaining;
HousesType Laughing = HOUSE_NONE; // the owner whose Last Laugh is going off, while it does

// Clouds that share a cell are one smoke. Worked out from Clouds and Frame alone, so it
// is a cache rather than synchronised state: SmokeVersion moves whenever Clouds change
// other than by running out, and Frame covers the running out.
int SmokeOf[MaxClouds]; // per cloud, the lowest index of the smoke it belongs to; -1 when gone
int SmokeFrame = Never;
unsigned SmokeVersion = 1;
unsigned SmokeBuilt = 0;

int Time(int tenths)
{
    return Gun_Game_Tenths(tenths);
}

Player* Of(HousesType owner)
{
    return Gun_Game_In_Match(owner) ? &Seats[owner] : nullptr;
}

bool On(HousesType owner, int ability)
{
    const Player* p = Of(owner);
    return p && p->Until[ability] > Frame;
}

bool Live(const TechnoClass* unit)
{
    return unit && unit->IsActive && !unit->IsInLimbo && unit->Strength > 0;
}

// The player's combatant when it can act, else null.
TechnoClass* Fighter(HousesType owner)
{
    TechnoClass* unit = Gun_Game_Combatant(owner);
    if (!Live(unit) || unit->House->IsToDie || unit->House->IsDefeated) return nullptr;
    return unit;
}

bool Is_Combatant(const TechnoClass* unit)
{
    return unit && Gun_Game_Combatant(unit->Owner()) == unit;
}

bool In_Map(CELL cell)
{
    return cell >= 0 && cell < MAP_CELL_TOTAL && Map.In_Radar(cell);
}

bool Walkable(CELL cell)
{
    return In_Map(cell) && Ground[Map[cell].Land_Type()].Cost[SPEED_FOOT] != 0;
}

bool Ground_Unit(const TechnoClass* unit)
{
    const RTTIType kind = unit->What_Am_I();
    return (kind == RTTI_INFANTRY || kind == RTTI_UNIT) && !Gun_Game_Airborne(unit);
}

bool Flying(const TechnoClass* unit)
{
    return unit->What_Am_I() == RTTI_AIRCRAFT && static_cast<const AircraftClass*>(unit)->Altitude > 0;
}

bool Fits(const FootClass* unit, CELL cell)
{
    if (!In_Map(cell)) return false;
    return Flying(unit) || unit->Can_Enter_Cell(cell) == MOVE_OK;
}

bool Needs_Target(int ability)
{
    return ability == GA_CHRONO_SWAP || ability == GA_HUMILIATION || ability == GA_HOT_POTATO
        || ability == GA_MIRROR;
}

int Cloud_Of(CELL cell)
{
    for (int k = 0; k < MaxClouds; ++k) {
        const Cloud& cloud = Clouds[k];
        if (cloud.Until <= Frame) continue;
        for (int i = 0; i < cloud.Count; ++i) {
            if (cloud.Cells[i] == cell) return k;
        }
    }
    return -1;
}

bool Overlap(const Cloud& a, const Cloud& b)
{
    for (int i = 0; i < a.Count; ++i) {
        for (int j = 0; j < b.Count; ++j) {
            if (a.Cells[i] == b.Cells[j]) return true;
        }
    }
    return false;
}

void Join_Smoke()
{
    if (SmokeFrame == Frame && SmokeBuilt == SmokeVersion) return;
    SmokeFrame = Frame;
    SmokeBuilt = SmokeVersion;
    for (int k = 0; k < MaxClouds; ++k) SmokeOf[k] = Clouds[k].Until > Frame ? k : -1;
    for (int a = 0; a < MaxClouds; ++a) {
        for (int b = a + 1; b < MaxClouds; ++b) {
            if (SmokeOf[a] < 0 || SmokeOf[b] < 0 || SmokeOf[a] == SmokeOf[b] || !Overlap(Clouds[a], Clouds[b])) continue;
            const int from = MAX(SmokeOf[a], SmokeOf[b]), to = MIN(SmokeOf[a], SmokeOf[b]);
            for (int& label : SmokeOf) {
                if (label == from) label = to;
            }
        }
    }
}

// The smoke a cell stands in, or -1. Every cloud holding the cell shares that cell, so
// they all belong to the same smoke.
int Smoke_At(CELL cell)
{
    const int k = Cloud_Of(cell);
    if (k < 0) return -1;
    Join_Smoke();
    return SmokeOf[k];
}

// Smoke hides what stands in it from anyone whose combatant is outside that smoke. Two
// units in the same smoke see each other whichever of its clouds each stands in, and
// two in separate smokes see neither way, so the rule reads the same from both ends.
bool Smoked_From(HousesType viewer, COORDINATE where)
{
    const int smoke = Smoke_At(Coord_Cell(where));
    if (smoke < 0) return false;
    const TechnoClass* eye = viewer == HOUSE_NONE ? nullptr : Fighter(viewer);
    return !eye || Smoke_At(Coord_Cell(eye->Coord)) != smoke;
}

bool Holding(HousesType owner)
{
    for (const Potato& potato : Potatoes) {
        if (potato.Holder == owner && potato.Until > Frame) return true;
    }
    return false;
}

bool Is_Decoy(const TechnoClass* unit)
{
    if (!unit) return false;
    const TARGET target = unit->As_Target();
    for (const Decoy& decoy : Decoys) {
        if (decoy.Owner != HOUSE_NONE && decoy.Unit == target && decoy.Owner == unit->Owner()) return true;
    }
    return false;
}

// Ends a decoy: its unit goes with the slot. A slot freed without deleting the unit would
// leave that copy on the map for good, armed, since only the slot made it a decoy.
void Retire(Decoy& decoy)
{
    TechnoClass* unit = As_Techno(decoy.Unit);
    const bool standing = unit && unit->IsActive && unit->Owner() == decoy.Owner && unit->Strength > 0
        && !Is_Combatant(unit);
    decoy.Owner = HOUSE_NONE;
    decoy.Unit = TARGET_NONE;
    decoy.Until = Never;
    if (!standing) return;
    if (!unit->IsInLimbo) new AnimClass(ANIM_PIFF, unit->Coord);
    delete unit;
}

HousesType Leader()
{
    HousesType best = HOUSE_NONE;
    for (int i = 0; i < MPlayerCount; ++i) {
        const HousesType house = MPlayerHouses[i];
        if (best == HOUSE_NONE || Gun_Game_Tier(house) > Gun_Game_Tier(best)
            || (Gun_Game_Tier(house) == Gun_Game_Tier(best) && Gun_Game_Kills(house) > Gun_Game_Kills(best))) {
            best = house;
        }
    }
    return best;
}

// The first free slot, or the one closest to running out.
template <class T, int N> T& Slot(T (&slots)[N])
{
    T* best = &slots[0];
    for (T& slot : slots) {
        if (slot.Until <= Frame) return slot;
        if (slot.Until < best->Until) best = &slot;
    }
    return *best;
}

void Announce(HousesType owner, int ability)
{
    News& news = Newsreel[NextNews];
    NextNews = (NextNews + 1) % MaxNews;
    news.Owner = owner;
    news.Ability = ability;
    news.At = Frame;
}

void Record_Blast(HousesType owner, COORDINATE where)
{
    Blast& blast = Blasts[NextBlast];
    NextBlast = (NextBlast + 1) % MaxBlasts;
    blast.Owner = owner;
    blast.Where = where;
    blast.At = Frame;
}

void Record_Arc(HousesType owner, COORDINATE from, COORDINATE to)
{
    ShockArc& arc = Arcs[NextArc];
    NextArc = (NextArc + 1) % MaxArcs;
    arc.Owner = owner;
    arc.From = from;
    arc.To = to;
    arc.At = Frame;
}

void Begin(Player& p, int serial)
{
    p.Serial = serial;
    p.Mask = 0;
    p.Ability = GA_NONE;
    p.Target = -1;
    p.Count = 0;
    p.Life = 0;
    p.Expire = 0;
    for (CELL& cell : p.Cells) cell = -1;
}

void Halt(FootClass* unit)
{
    unit->Assign_Target(TARGET_NONE);
    unit->Assign_Destination(TARGET_NONE);
    unit->Stop_Driver();
    unit->Path[0] = FACING_NONE;
}

void Resume(FootClass* unit)
{
    unit->Assign_Mission(MISSION_GUARD);
    unit->Commence();
}

// Unlimbo without its Can_Enter_Cell test, the way the engine sets down airborne
// reinforcements. That test is the landing rule for aircraft (a clear cell that can
// be built on), which a craft in the air does not have to meet.
bool Unlimbo_Anywhere(FootClass* unit, COORDINATE where, DirType facing)
{
    ScenarioInit++;
    const bool placed = unit->Unlimbo(where, facing);
    ScenarioInit--;
    return placed;
}

bool Arrive(FootClass* unit, COORDINATE where, DirType facing)
{
    return Flying(unit) ? Unlimbo_Anywhere(unit, where, facing) : unit->Unlimbo(where, facing);
}

// Puts a unit back where it stood a moment ago. This must not fail: a combatant left in
// limbo is never respawned, so its player would sit out the rest of the match.
void Restore(FootClass* unit, COORDINATE where, DirType facing)
{
    Unlimbo_Anywhere(unit, where, facing);
}

// Moves a unit through limbo, as the Chrono Tank's own jump does. With nearby set, a
// taken destination falls back to the free cell next to it.
bool Teleport(FootClass* unit, CELL cell, bool nearby)
{
    const int tries = nearby ? 9 : 1;
    const auto candidate = [cell](int n) { return n == 0 ? cell : Adjacent_Cell(cell, (FacingType)(n - 1)); };
    // A unit with nowhere to land is left exactly as it was. Halting it on every try
    // would hold anything standing on a wormhole whose far end is blocked in place.
    bool room = false;
    for (int n = 0; n < tries && !room; ++n) room = Fits(unit, candidate(n));
    if (!room) return false;
    const COORDINATE old = unit->Coord;
    const DirType facing = unit->PrimaryFacing.Current();
    Halt(unit);
    unit->Limbo();
    for (int n = 0; n < tries; ++n) {
        const CELL to = candidate(n);
        if (!Fits(unit, to)) continue;
        if (Arrive(unit, Cell_Coord(to), facing)) {
            Resume(unit);
            new AnimClass(ANIM_PIFF, old);
            new AnimClass(ANIM_PIFF, unit->Coord);
            return true;
        }
    }
    Restore(unit, old, facing);
    Resume(unit);
    return false;
}

bool Swap(FootClass* a, FootClass* b)
{
    const COORDINATE where_a = a->Coord, where_b = b->Coord;
    const DirType facing_a = a->PrimaryFacing.Current(), facing_b = b->PrimaryFacing.Current();
    Halt(a);
    Halt(b);
    a->Limbo();
    b->Limbo();
    const CELL cell_a = Coord_Cell(where_a), cell_b = Coord_Cell(where_b);
    bool swapped = Fits(a, cell_b) && Fits(b, cell_a) && Arrive(a, Cell_Coord(cell_b), facing_a);
    if (swapped && !Arrive(b, Cell_Coord(cell_a), facing_b)) {
        a->Limbo();
        swapped = false;
    }
    if (!swapped) {
        Restore(a, where_a, facing_a);
        Restore(b, where_b, facing_b);
    }
    Resume(a);
    Resume(b);
    if (swapped) {
        new AnimClass(ANIM_PIFF, where_a);
        new AnimClass(ANIM_PIFF, where_b);
    }
    return swapped;
}

CELL Booby_Cell(const TechnoClass* unit)
{
    const CELL centre = Coord_Cell(unit->Coord);
    for (int face = FACING_N; face < FACING_COUNT; ++face) {
        const CELL cell = Adjacent_Cell(centre, (FacingType)face);
        if (!Walkable(cell)) continue;
        const CellClass& c = Map[cell];
        if (c.Overlay == OVERLAY_NONE && c.Is_Generally_Clear()) return cell;
    }
    return -1;
}

void Puff(Cloud& cloud, int phase)
{
    for (int i = phase; i < cloud.Count; i += 4) {
        new AnimClass(ANIM_SMOKE_M, Cell_Coord(cloud.Cells[i]));
    }
}

void Freeze(TechnoClass* unit)
{
    if (unit->TarCom != TARGET_NONE) unit->Assign_Target(TARGET_NONE);
    if (unit->What_Am_I() == RTTI_BUILDING) return;
    FootClass* foot = static_cast<FootClass*>(unit);
    if (foot->NavCom != TARGET_NONE) foot->Assign_Destination(TARGET_NONE);
    if (foot->Get_Mission() != MISSION_GUARD) {
        foot->Assign_Mission(MISSION_GUARD);
        foot->Commence();
    }
    if (unit->What_Am_I() == RTTI_INFANTRY) {
        InfantryClass* soldier = static_cast<InfantryClass*>(unit);
        if (!soldier->IsDriving && soldier->Doing != DO_SALUTE1 && soldier->Doing != DO_GESTURE1) {
            soldier->Do_Action((Frame / Gun_Game_Ticks_Per_Second()) & 1 ? DO_GESTURE1 : DO_SALUTE1, true);
        }
    } else if (unit->What_Am_I() == RTTI_UNIT) {
        UnitClass* vehicle = static_cast<UnitClass*>(unit);
        if (!vehicle->IsDriving && !vehicle->PrimaryFacing.Is_Rotating()) {
            vehicle->PrimaryFacing.Set_Desired((DirType)((vehicle->PrimaryFacing.Current() + 96) & 0xff));
        }
    }
}

void Cool_Down(const TechnoClass* unit)
{
    const TARGET target = unit->As_Target();
    Hop* slot = nullptr;
    for (Hop& hop : Hops) {
        if (hop.Unit == target) {
            slot = &hop;
            break;
        }
    }
    if (!slot) slot = &Slot(Hops);
    slot->Unit = target;
    slot->Until = Frame + Time(WormholeCooldown);
}

bool Cooling(const TechnoClass* unit)
{
    const TARGET target = unit->As_Target();
    for (const Hop& hop : Hops) {
        if (hop.Unit == target && hop.Until > Frame) return true;
    }
    return false;
}

bool On_Wormhole(CELL cell)
{
    for (const Hole& hole : Holes) {
        if (hole.Until > Frame && (hole.Ends[0] == cell || hole.Ends[1] == cell)) return true;
    }
    return false;
}

// Every ability's rule, run when the renderer asks, again when the event executes on
// every peer, and for computer players.
bool Check(HousesType owner, int ability, int target, int count, const CELL* cells)
{
    const Player* p = Of(owner);
    if (!p || Gun_Game_Over() || ability <= GA_NONE || ability >= GA_COUNT || p->Held != ability) return false;
    if (count < 0 || count > GA_MAX_CELLS || (count && !cells)) return false;
    for (int i = 0; i < count; ++i) {
        if (!In_Map(cells[i])) return false;
    }
    TechnoClass* me = Fighter(owner);
    // A dancer is frozen and uses nothing until the dance ends, as computer players do.
    if (!me || On(owner, GA_DANCE_OFF)) return false;
    TechnoClass* them = nullptr;
    if (Needs_Target(ability)) {
        if (target == owner || target < 0 || target >= HOUSE_COUNT || !Gun_Game_In_Match((HousesType)target)) {
            return false;
        }
        them = Fighter((HousesType)target);
        if (!them || Gun_Ability_Hidden(owner, them)) return false;
    }
    switch (ability) {
    case GA_CHRONO_HOP:
        return count == 1 && !Gun_Game_Dodging(me) && Coord_Cell(me->Coord) != cells[0]
            && Distance(me->Coord, Cell_Coord(cells[0])) <= HopRange * CellLeptons + CellLeptons / 2
            && Fits(static_cast<FootClass*>(me), cells[0]);
    case GA_CHRONO_SWAP:
        return !Gun_Game_Dodging(me) && !Gun_Game_Dodging(them)
            && Distance(me->Coord, them->Coord) <= SwapRange * CellLeptons + CellLeptons / 2;
    case GA_HUMILIATION:
        return Gun_Game_Tier((HousesType)target) > 0;
    case GA_HOT_POTATO:
        return !Holding((HousesType)target);
    case GA_VORTEX:
        return count == 1;
    case GA_MINEFIELD:
        if (count < 1 || count > MineCount) return false;
        for (int i = 0; i < count; ++i) {
            if (!Walkable(cells[i])
                || Distance(me->Coord, Cell_Coord(cells[i])) > MineRange * CellLeptons + CellLeptons / 2) {
                return false;
            }
            // One mine to a cell: five on one cell would be one mine five times as strong.
            for (int j = 0; j < i; ++j) {
                if (cells[j] == cells[i]) return false;
            }
        }
        return true;
    case GA_SMOKE:
        return count >= 1;
    case GA_WORMHOLE:
        return count == 2 && cells[0] != cells[1] && Walkable(cells[0]) && Walkable(cells[1]);
    case GA_BOOBY_CRATE:
        return Booby_Cell(me) != -1;
    default:
        return true;
    }
}

// Applies a checked activation. Returns false, keeping the ability, when the world
// refused it (a teleport with nowhere to land, no room for a decoy).
bool Apply(HousesType owner, int ability, int target, int count, const CELL* cells)
{
    Player& p = Seats[owner];
    TechnoClass* me = Fighter(owner);
    TechnoClass* them = Needs_Target(ability) ? Fighter((HousesType)target) : nullptr;
    const auto lasting = [&](int tenths) { p.Until[ability] = Frame + Time(tenths); };
    switch (ability) {
    case GA_STEALTH:
        lasting(StealthTime);
        break;
    case GA_CHRONO_HOP:
        if (!Teleport(static_cast<FootClass*>(me), cells[0], false)) return false;
        break;
    case GA_CHRONO_SWAP:
        if (!Swap(static_cast<FootClass*>(me), static_cast<FootClass*>(them))) return false;
        break;
    case GA_IRON_CURTAIN:
        lasting(IronTime);
        break;
    case GA_OVERDRIVE:
        lasting(OverdriveTime);
        break;
    case GA_VORTEX: {
        Vortex& v = Slot(Vortices);
        v.Owner = owner;
        v.Coord = Cell_Coord(cells[0]);
        v.Dir = Random_Pick(0, 255);
        v.Until = Frame + Time(VortexTime);
        v.NextPulse = Frame;
        v.NextTurn = Frame + Time(VortexTurn);
        break;
    }
    case GA_MINEFIELD:
        for (int i = 0; i < count; ++i) {
            Mine& mine = Slot(Mines);
            mine.Owner = owner;
            mine.Cell = cells[i];
            mine.Until = Frame + Time(MineTime);
        }
        break;
    case GA_HUNTERS_EYE:
        lasting(HunterTime);
        break;
    case GA_HUMILIATION:
        if (!Gun_Game_Demote((HousesType)target)) return false;
        break;
    case GA_CHAIN_SHOCK:
        lasting(ChainTime);
        break;
    case GA_EXTRA_LIFE:
        lasting(ExtraLifeTime);
        break;
    case GA_SMOKE: {
        Cloud& cloud = Slot(Clouds);
        cloud.Owner = owner;
        cloud.Until = Frame + Time(SmokeTime);
        cloud.NextPuff = Frame + Time(SmokePuff);
        cloud.Puff = 0;
        cloud.Count = count;
        for (int i = 0; i < GA_MAX_CELLS; ++i) cloud.Cells[i] = i < count ? cells[i] : -1;
        ++SmokeVersion;
        for (int phase = 0; phase < 4; ++phase) Puff(cloud, phase);
        break;
    }
    case GA_DECOYS: {
        int made = 0;
        for (int i = 0; i < DecoyCount; ++i) {
            TechnoClass* copy = Gun_Game_Spawn_Copy(owner, Coord_Cell(me->Coord));
            if (!copy) break;
            copy->Strength = 1;
            Decoy& decoy = Slot(Decoys);
            // A slot still in use goes first: every slot taken (the oldest decoy goes), or
            // a decoy that runs out on this very frame, before Tick_Decoys has removed it.
            if (decoy.Owner != HOUSE_NONE) Retire(decoy);
            decoy.Owner = owner;
            decoy.Unit = copy->As_Target();
            decoy.Until = Frame + Time(DecoyTime);
            decoy.WanderAt = Frame;
            new AnimClass(ANIM_PIFF, copy->Coord);
            ++made;
        }
        if (!made) return false;
        break;
    }
    case GA_GIANT:
        lasting(GiantTime);
        break;
    case GA_LAST_LAUGH:
        lasting(LastLaughTime);
        break;
    case GA_HOT_POTATO: {
        Potato& potato = Slot(Potatoes);
        potato.Thrower = owner;
        potato.Holder = target;
        potato.Until = Frame + Time(PotatoFuse);
        potato.PassReady = Frame + Time(PotatoPass);
        break;
    }
    case GA_BOOBY_CRATE: {
        const CELL cell = Booby_Cell(me);
        if (cell == -1) return false;
        Booby* slot = nullptr;
        for (Booby& booby : Boobies) {
            if (booby.Owner == HOUSE_NONE) {
                slot = &booby;
                break;
            }
        }
        if (!slot) return false;
        CellClass& c = Map[cell];
        c.Overlay = OVERLAY_WOOD_CRATE;
        c.OverlayData = 0;
        c.Redraw_Objects();
        slot->Owner = owner;
        slot->Cell = cell;
        break;
    }
    case GA_WORMHOLE: {
        Hole& hole = Slot(Holes);
        hole.Owner = owner;
        hole.Ends[0] = cells[0];
        hole.Ends[1] = cells[1];
        hole.Until = Frame + Time(WormholeTime);
        new AnimClass(ANIM_PIFF, Cell_Coord(cells[0]));
        new AnimClass(ANIM_PIFF, Cell_Coord(cells[1]));
        break;
    }
    case GA_DANCE_OFF:
        for (int i = 0; i < MPlayerCount; ++i) {
            const HousesType other = MPlayerHouses[i];
            TechnoClass* victim = other == owner ? nullptr : Fighter(other);
            if (!victim || Distance(me->Coord, victim->Coord) > DanceRange * CellLeptons + CellLeptons / 2) continue;
            Seats[other].Until[GA_DANCE_OFF] = Frame + Time(DanceTime);
            Freeze(victim);
        }
        break;
    case GA_MIRROR:
        Seats[target].Until[GA_MIRROR] = Frame + Time(MirrorTime);
        break;
    default:
        return false;
    }
    p.Held = GA_NONE;
    Announce(owner, ability);
    return true;
}

bool Use(HousesType owner, int ability, int target, int count, const CELL* cells)
{
    return Check(owner, ability, target, count, cells) && Apply(owner, ability, target, count, cells);
}

void Try_Apply(HousesType owner)
{
    Player& p = Seats[owner];
    if (p.Ability == GA_NONE) return;
    const unsigned need = p.Count ? (p.Count >= 32 ? ~0u : (1u << p.Count) - 1) : 0;
    if ((p.Mask & need) != need) return;
    CELL cells[GA_MAX_CELLS];
    memcpy(cells, p.Cells, sizeof cells);
    const int ability = p.Ability, target = p.Target, count = p.Count, life = p.Life;
    Begin(p, -1);
    // A token from an earlier life means the combatant that asked has since died.
    if ((unsigned)life != Gun_Game_Life(owner)) return;
    Use(owner, ability, target, count, cells);
}

void Blow_Up(TechnoClass* victim, HousesType owner)
{
    const int damage = victim->Strength * 8 + 100;
    Gun_Game_Strike_Hit(victim, damage, 0, owner, WARHEAD_HE);
}

// Every unit a computer player may take for an enemy combatant: the combatants, then
// the decoys, which look the same as the real one to anyone but their owner.
template <class F> void Each_Enemy(HousesType owner, F visit)
{
    for (int i = 0; i < MPlayerCount; ++i) {
        const HousesType other = MPlayerHouses[i];
        if (TechnoClass* enemy = other == owner ? nullptr : Fighter(other)) visit(enemy);
    }
    for (const Decoy& decoy : Decoys) {
        if (decoy.Owner == HOUSE_NONE || decoy.Owner == owner) continue;
        TechnoClass* unit = As_Techno(decoy.Unit);
        if (Live(unit) && unit->Owner() == decoy.Owner && !unit->House->IsDefeated) visit(unit);
    }
}

bool Seen_By(HousesType owner, const TechnoClass* enemy)
{
    const HouseClass* house = HouseClass::As_Pointer(owner);
    return house->IsHuman || TD_Fair_AI_Knows(house, enemy);
}

// The nearest enemy the computer may act on. One it has already seen (the skirmish
// fairness rule drops any target it has not) comes before one it has not, which it only
// walks towards.
TechnoClass* Nearest_Enemy(HousesType owner, const TechnoClass* me, int* distance, bool* known)
{
    TechnoClass* best = nullptr;
    int best_distance = 0x7fffffff;
    bool best_known = false;
    Each_Enemy(owner, [&](TechnoClass* enemy) {
        if (Gun_Ability_Hidden(owner, enemy)) return;
        const bool seen = Seen_By(owner, enemy);
        const int d = me ? Distance(me->Coord, enemy->Coord) : 0;
        if (!best || (seen && !best_known) || (seen == best_known && d < best_distance)) {
            best = enemy;
            best_distance = d;
            best_known = seen;
        }
    });
    if (distance) *distance = best_distance;
    if (known) *known = best_known;
    return best;
}

// Mirror Madness on a computer player: its orders land mirrored left-right about its own
// position, as a mirrored player's clicks do.
CELL Mirrored(CELL about, CELL cell)
{
    const int x = 2 * Cell_X(about) - Cell_X(cell);
    return XY_Cell(MAX(Map.MapCellX, MIN(Map.MapCellX + Map.MapCellWidth - 1, x)), Cell_Y(cell));
}

bool Own_Booby(HousesType owner, CELL cell)
{
    for (const Booby& booby : Boobies) {
        if (booby.Owner == owner && booby.Cell == cell) return true;
    }
    return false;
}

CELL Nearest_Crate(HousesType owner, const FootClass* unit, int radius, int* distance)
{
    const Player& p = Seats[owner];
    const CELL centre = Coord_Cell(unit->Coord);
    CELL best = -1;
    int best_distance = 0x7fffffff;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int x = Cell_X(centre) + dx, y = Cell_Y(centre) + dy;
            if (x < 0 || y < 0 || x >= MAP_CELL_W || y >= MAP_CELL_H) continue;
            const CELL cell = XY_Cell(x, y);
            if (!In_Map(cell)) continue;
            const OverlayType overlay = Map[cell].Overlay;
            if (overlay == OVERLAY_NONE || !OverlayTypeClass::As_Reference(overlay).IsCrate) continue;
            if (Own_Booby(owner, cell) || (cell == p.SkipCrate && Frame < p.SkipUntil)) continue;
            if (unit->Can_Enter_Cell(cell) == MOVE_NO) continue;
            const int d = Distance(Cell_Coord(centre), Cell_Coord(cell));
            if (d < best_distance) {
                best = cell;
                best_distance = d;
            }
        }
    }
    if (distance) *distance = best_distance;
    return best;
}

CELL Walkable_Near(CELL centre, int radius, int tries)
{
    for (int n = 0; n < tries; ++n) {
        const int x = Cell_X(centre) + Random_Pick(-radius, radius);
        const int y = Cell_Y(centre) + Random_Pick(-radius, radius);
        if (x < 0 || y < 0 || x >= MAP_CELL_W || y >= MAP_CELL_H) continue;
        const CELL cell = XY_Cell(x, y);
        if (cell != centre && Walkable(cell)) return cell;
    }
    return -1;
}

// A computer player picks a target or cells for the ability it holds, or waits.
void Computer_Use(HousesType owner, TechnoClass* me, TechnoClass* enemy, int range)
{
    Player& p = Seats[owner];
    const int ability = p.Held;
    const bool patient = Frame - p.UseSince < Time(150);
    const CELL here = Coord_Cell(me->Coord);
    int target = -1;
    int count = 0;
    CELL cells[GA_MAX_CELLS];
    const auto wait = [&]() { p.UseAt = Frame + Time(10); };
    switch (ability) {
    case GA_STEALTH:
    case GA_IRON_CURTAIN:
    case GA_OVERDRIVE:
    case GA_CHAIN_SHOCK:
    case GA_GIANT:
    case GA_LAST_LAUGH:
    case GA_DECOYS:
        if (patient && (!enemy || range > 8 * CellLeptons)) return wait();
        break;
    case GA_DANCE_OFF:
        if (!enemy || range > (DanceRange - 1) * CellLeptons) return wait();
        break;
    case GA_CHRONO_SWAP:
        if (!enemy || range > SwapRange * CellLeptons) return wait();
        target = enemy->Owner();
        break;
    case GA_HOT_POTATO:
    case GA_MIRROR:
        if (!enemy) return wait();
        target = enemy->Owner();
        break;
    case GA_HUMILIATION: {
        // The leader only once seen, by the same rule as any other target.
        const HousesType leader = Leader();
        const TechnoClass* lead = leader == owner ? nullptr : Fighter(leader);
        if (lead && Gun_Game_Tier(leader) > 0 && !Gun_Ability_Hidden(owner, lead) && Seen_By(owner, lead)) {
            target = leader;
        } else if (enemy && Gun_Game_Tier(enemy->Owner()) > 0) {
            target = enemy->Owner();
        } else {
            return wait();
        }
        break;
    }
    case GA_CHRONO_HOP: {
        if (!enemy || range < 3 * CellLeptons) return wait();
        const int step = MIN(range - 2 * CellLeptons, HopRange * CellLeptons);
        const CELL toward = Coord_Cell(Coord_Move(me->Coord, Direction(me->Coord, enemy->Coord), step));
        for (int n = 0; n < 9 && !count; ++n) {
            const CELL cell = n == 0 ? toward : Adjacent_Cell(toward, (FacingType)(n - 1));
            if (cell != here && Fits(static_cast<FootClass*>(me), cell)
                && Distance(me->Coord, Cell_Coord(cell)) <= HopRange * CellLeptons + CellLeptons / 2) {
                cells[count++] = cell;
            }
        }
        if (!count) return wait();
        break;
    }
    case GA_VORTEX:
        if (!enemy || range < 3 * CellLeptons) return wait();
        cells[count++] = Coord_Cell(enemy->Coord);
        break;
    case GA_MINEFIELD:
        for (int n = 0; n < 24 && count < MineCount; ++n) {
            const CELL cell = Walkable_Near(here, MineRange - 1, 1);
            bool repeat = cell == -1;
            for (int i = 0; i < count && !repeat; ++i) repeat = cells[i] == cell;
            if (!repeat) cells[count++] = cell;
        }
        if (!count) return wait();
        break;
    case GA_SMOKE:
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int x = Cell_X(here) + dx, y = Cell_Y(here) + dy;
                if (x < 0 || y < 0 || x >= MAP_CELL_W || y >= MAP_CELL_H) continue;
                if (In_Map(XY_Cell(x, y))) cells[count++] = XY_Cell(x, y);
            }
        }
        break;
    case GA_WORMHOLE: {
        if (!enemy || range < 6 * CellLeptons) return wait();
        const CELL entry = Walkable_Near(here, 1, 8);
        const CELL near_enemy = Coord_Cell(Coord_Move(enemy->Coord, Direction(enemy->Coord, me->Coord), 2 * CellLeptons));
        const CELL exit = Walkable(near_enemy) ? near_enemy : Walkable_Near(Coord_Cell(enemy->Coord), 2, 8);
        if (entry == -1 || exit == -1 || entry == exit) return wait();
        cells[count++] = entry;
        cells[count++] = exit;
        break;
    }
    default:
        break;
    }
    if (!Use(owner, ability, target, count, cells)) wait();
}

void Computer_Move(HousesType owner, TechnoClass* me, TechnoClass* enemy, int range, bool known)
{
    Player& p = Seats[owner];
    FootClass* foot = static_cast<FootClass*>(me);
    const bool mirrored = On(owner, GA_MIRROR);
    const CELL here = Coord_Cell(me->Coord);
    const auto go = [foot, mirrored, here](CELL cell) {
        if (mirrored) cell = Mirrored(here, cell);
        const TARGET destination = ::As_Target(cell);
        if (foot->NavCom == destination && foot->Get_Mission() == MISSION_MOVE) return;
        foot->Assign_Mission(MISSION_MOVE);
        foot->Assign_Target(TARGET_NONE);
        foot->Assign_Destination(destination);
    };
    int crate_distance = 0;
    const CELL crate = Flying(me) ? -1 : Nearest_Crate(owner, foot, 6, &crate_distance);
    if (crate != -1 && (!enemy || !known || crate_distance < range)) {
        if (p.CrateCell != crate) {
            p.CrateCell = crate;
            p.CrateSince = Frame;
        } else if (Frame - p.CrateSince > Time(80)) {
            // Eight seconds without reaching it: something is in the way.
            p.SkipCrate = crate;
            p.SkipUntil = Frame + Time(300);
            p.CrateCell = -1;
            return;
        }
        go(crate);
        return;
    }
    p.CrateCell = -1;
    if (enemy && known) {
        if (mirrored) {
            // The attack order lands on the far side as well, where it is a walk.
            go(Coord_Cell(enemy->Coord));
            return;
        }
        if (foot->TarCom != enemy->As_Target() || foot->Get_Mission() != MISSION_ATTACK) {
            // Stop any walk so the queued attack starts at once.
            foot->Assign_Mission(MISSION_ATTACK);
            foot->Assign_Target(enemy->As_Target());
            foot->Assign_Destination(TARGET_NONE);
        }
        return;
    }
    if (Frame < p.RoamAt) return;
    p.RoamAt = Frame + Time(30);
    if (enemy) {
        // Not seen yet: walk towards where the fighting is.
        go(Coord_Cell(enemy->Coord));
        return;
    }
    const CELL cell = Walkable_Near(Coord_Cell(me->Coord), 8, 8);
    if (cell != -1) go(cell);
}

AnimType Crate_Anim(int ability)
{
    switch (ability) {
    case GA_STEALTH:
    case GA_DECOYS:
        return ANIM_CRATE_STEALTH;
    case GA_IRON_CURTAIN:
    case GA_GIANT:
    case GA_EXTRA_LIFE:
        return ANIM_CRATE_INVUN;
    case GA_OVERDRIVE:
    case GA_CHAIN_SHOCK:
        return ANIM_CRATE_RAPID;
    case GA_MINEFIELD:
    case GA_BOOBY_CRATE:
    case GA_HOT_POTATO:
    case GA_LAST_LAUGH:
        return ANIM_CRATE_MINE;
    default:
        return ANIM_CRATE_EMPULSE;
    }
}

void Tick_Traps()
{
    const int count = TrapCount;
    Trap traps[MaxBooby];
    memcpy(traps, Traps, sizeof traps);
    TrapCount = 0;
    for (int i = 0; i < count; ++i) {
        const Trap& trap = traps[i];
        TechnoClass* victim = As_Techno(trap.Unit);
        // Only the life that took the crate pays for it.
        if (!Live(victim) || victim->Owner() != trap.Victim || !Is_Combatant(victim)
            || Gun_Game_Life((HousesType)trap.Victim) != trap.Life) {
            continue;
        }
        Blow_Up(victim, (HousesType)trap.Owner);
    }
}

void Tick_Laughs()
{
    const int count = LaughCount;
    Laugh laughs[MaxLaughs];
    memcpy(laughs, Laughs, sizeof laughs);
    LaughCount = 0;
    for (int i = 0; i < count; ++i) {
        const COORDINATE at = laughs[i].Where;
        const CELL centre = Coord_Cell(at);
        const HousesType owner = (HousesType)laughs[i].Owner;
        Record_Blast(owner, at);
        new AnimClass(ANIM_ART_EXP1, at);
        // Ability damage: it scores for the owner only while the owner has a combatant
        // (an Extra Life respawn), and it spares the owner's own side, that respawn
        // included, which can already stand where the owner fell.
        Laughing = owner;
        Gun_Game_Strike_Damage(at, LaughCentre, owner, WARHEAD_HE);
        for (int face = FACING_N; face < FACING_COUNT; ++face) {
            const CELL cell = Adjacent_Cell(centre, (FacingType)face);
            if (!In_Map(cell)) continue;
            new AnimClass(ANIM_FBALL1, Cell_Coord(cell));
            Gun_Game_Strike_Damage(Cell_Coord(cell), LaughRing, owner, WARHEAD_HE);
        }
        Laughing = HOUSE_NONE;
    }
}

void Tick_Shocks()
{
    const int count = ShockCount;
    Shock shocks[MaxShocks];
    memcpy(shocks, Shocks, sizeof shocks);
    ShockCount = 0;
    Chaining = true;
    for (int s = 0; s < count; ++s) {
        const Shock& shock = shocks[s];
        TechnoClass* hits[ChainTargets] = {};
        int distances[ChainTargets] = {};
        for (int i = 0; i < MPlayerCount; ++i) {
            const HousesType other = MPlayerHouses[i];
            TechnoClass* enemy = other == shock.Owner ? nullptr : Fighter(other);
            if (!enemy || enemy->As_Target() == shock.Victim || Gun_Ability_Hidden((HousesType)shock.Owner, enemy)) {
                continue;
            }
            int d = Distance(shock.Where, enemy->Center_Coord());
            if (d > ChainRange * CellLeptons + CellLeptons / 2) continue;
            for (int k = 0; k < ChainTargets && enemy; ++k) {
                if (!hits[k] || d < distances[k]) {
                    TechnoClass* bumped = hits[k];
                    const int bumped_distance = distances[k];
                    hits[k] = enemy;
                    distances[k] = d;
                    enemy = bumped;
                    d = bumped_distance;
                }
            }
        }
        for (TechnoClass* enemy : hits) {
            if (!Live(enemy)) continue;
            Record_Arc((HousesType)shock.Owner, shock.Where, enemy->Center_Coord());
            // An arc is ability damage: it scores, but cannot win from the last rank.
            const HousesType owner = Fighter((HousesType)shock.Owner) ? (HousesType)shock.Owner : HOUSE_NONE;
            Gun_Game_Strike_Hit(enemy, shock.Damage, 0, owner, WARHEAD_PB);
        }
    }
    Chaining = false;
}

void Tick_Vortices()
{
    const int step = MAX(4, 384 / Gun_Game_Ticks_Per_Second());
    for (Vortex& v : Vortices) {
        if (v.Until <= Frame) continue;
        if (Frame >= v.NextTurn) {
            v.Dir = (v.Dir + Random_Pick(-48, 48)) & 0xff;
            v.NextTurn = Frame + Time(VortexTurn);
        }
        COORDINATE next = Coord_Move(v.Coord, (DirType)v.Dir, step);
        if (!In_Map(Coord_Cell(next))) {
            v.Dir = (v.Dir + 128) & 0xff;
            next = Coord_Move(v.Coord, (DirType)v.Dir, step);
            if (!In_Map(Coord_Cell(next))) next = v.Coord;
        }
        v.Coord = next;
        if (Frame < v.NextPulse) continue;
        v.NextPulse = Frame + Time(VortexPulse);
        new AnimClass(ANIM_CRATE_EMPULSE, v.Coord);
        Gun_Game_Strike_Damage(v.Coord, VortexDamage, (HousesType)v.Owner, WARHEAD_HE);
        // Explosion_Damage spares its source, and the vortex spares nobody.
        TechnoClass* self = Fighter((HousesType)v.Owner);
        const int d = self ? Distance(v.Coord, self->Center_Coord()) : 0x7fffffff;
        if (self && !Flying(self) && d < CellLeptons + CellLeptons / 2) {
            Gun_Game_Strike_Hit(self, VortexDamage, d, HOUSE_NONE, WARHEAD_HE);
        }
    }
}

void Tick_Mines()
{
    for (Mine& mine : Mines) {
        if (mine.Until <= Frame) continue;
        bool tripped = false;
        for (ObjectClass* o = Map[mine.Cell].Cell_Occupier(); o && !tripped; o = o->Next) {
            if (!o->IsActive || o->IsInLimbo || !o->Is_Techno()) continue;
            const TechnoClass* t = static_cast<const TechnoClass*>(o);
            tripped = t->Owner() != mine.Owner && Ground_Unit(t);
        }
        if (!tripped) continue;
        const COORDINATE at = Cell_Coord(mine.Cell);
        const HousesType owner = (HousesType)mine.Owner;
        mine.Until = Never;
        new AnimClass(ANIM_ART_EXP1, at);
        Gun_Game_Strike_Damage(at, MineDamage, owner, WARHEAD_HE);
    }
}

void Tick_Clouds()
{
    for (Cloud& cloud : Clouds) {
        if (cloud.Until <= Frame || Frame < cloud.NextPuff) continue;
        cloud.NextPuff = Frame + Time(SmokePuff);
        Puff(cloud, cloud.Puff);
        cloud.Puff = (cloud.Puff + 1) & 3;
    }
}

void Tick_Wormholes()
{
    for (Hop& hop : Hops) {
        if (hop.Until <= Frame) continue;
        const TechnoClass* unit = As_Techno(hop.Unit);
        // A unit that came through waits until it steps off before it can go back.
        if (Live(unit) && On_Wormhole(Coord_Cell(unit->Coord))) hop.Until = Frame + Time(WormholeCooldown);
    }
    for (Hole& hole : Holes) {
        if (hole.Until <= Frame) continue;
        for (int end = 0; end < 2; ++end) {
            FootClass* travellers[8];
            int count = 0;
            for (ObjectClass* o = Map[hole.Ends[end]].Cell_Occupier(); o && count < 8; o = o->Next) {
                if (!o->IsActive || o->IsInLimbo || !o->Is_Techno()) continue;
                TechnoClass* t = static_cast<TechnoClass*>(o);
                if (Ground_Unit(t) && !Cooling(t) && !Gun_Game_Dodging(t)) travellers[count++] = static_cast<FootClass*>(t);
            }
            for (int i = 0; i < count; ++i) {
                if (Teleport(travellers[i], hole.Ends[1 - end], true)) Cool_Down(travellers[i]);
            }
        }
    }
}

void Tick_Potatoes()
{
    for (Potato& potato : Potatoes) {
        if (potato.Until <= Frame) continue;
        TechnoClass* holder = Fighter((HousesType)potato.Holder);
        if (!holder) {
            // The holder died with it: it goes off harmlessly.
            potato.Until = Never;
            continue;
        }
        if (Frame + 1 >= potato.Until) {
            const COORDINATE at = holder->Center_Coord();
            const HousesType thrower = (HousesType)potato.Thrower;
            potato.Until = Never;
            Record_Blast(thrower, at);
            new AnimClass(ANIM_ART_EXP1, at);
            Blow_Up(holder, thrower == potato.Holder ? HOUSE_NONE : thrower);
            Gun_Game_Strike_Damage(at, PotatoSplash, thrower, WARHEAD_HE);
            continue;
        }
        if (Frame < potato.PassReady) continue;
        TechnoClass* next = nullptr;
        int best = CellLeptons + 1;
        for (int i = 0; i < MPlayerCount; ++i) {
            const HousesType other = MPlayerHouses[i];
            TechnoClass* unit = other == potato.Holder ? nullptr : Fighter(other);
            if (!unit) continue;
            const int d = Distance(holder->Coord, unit->Coord);
            if (d < best) {
                best = d;
                next = unit;
            }
        }
        if (next) {
            potato.Holder = next->Owner();
            potato.PassReady = Frame + Time(PotatoPass);
        }
    }
}

void Tick_Decoys()
{
    for (Decoy& decoy : Decoys) {
        if (decoy.Owner == HOUSE_NONE) continue;
        TechnoClass* unit = As_Techno(decoy.Unit);
        if (!unit || !unit->IsActive || unit->Owner() != decoy.Owner || unit->Strength <= 0 || Is_Combatant(unit)) {
            decoy.Owner = HOUSE_NONE;
            decoy.Until = Never;
            continue;
        }
        if (Frame >= decoy.Until) {
            Retire(decoy);
            continue;
        }
        // One hit each: nothing repairs a decoy, the Mammoth's own self-repair included.
        if (unit->Strength > 1) unit->Strength = 1;
        if (unit->IsInLimbo || Frame < decoy.WanderAt) continue;
        decoy.WanderAt = Frame + Time(DecoyWander) + Random_Pick(0, Time(10));
        const CELL cell = Walkable_Near(Coord_Cell(unit->Coord), 4, 4);
        if (cell != -1) {
            FootClass* foot = static_cast<FootClass*>(unit);
            foot->Assign_Mission(MISSION_MOVE);
            foot->Assign_Target(TARGET_NONE);
            foot->Assign_Destination(::As_Target(cell));
        }
    }
}

// Nothing keeps a target it may no longer fire at: a stealthed combatant, something
// inside a cloud the shooter is outside, or anything at all for a decoy or a dancer.
void Tick_Targets()
{
    const auto purge = [](TechnoClass* t) {
        if (t->IsActive && t->TarCom != TARGET_NONE && Gun_Ability_Fire_Blocked(t, t->TarCom)) {
            t->Assign_Target(TARGET_NONE);
        }
    };
    for (int i = 0; i < Infantry.Count(); ++i) purge(Infantry.Ptr(i));
    for (int i = 0; i < Units.Count(); ++i) purge(Units.Ptr(i));
    for (int i = 0; i < Aircraft.Count(); ++i) purge(Aircraft.Ptr(i));
}

} // namespace

void Gun_Ability_Reset()
{
    memset(Seats, 0, sizeof Seats);
    for (Player& p : Seats) {
        Begin(p, -1);
        p.RespawnCell = -1;
        p.OfferToken = -1;
        p.CrateCell = -1;
        p.SkipCrate = -1;
    }
    for (Vortex& v : Vortices) v = Vortex{HOUSE_NONE, 0, 0, Never, 0, 0};
    for (Mine& m : Mines) m = Mine{HOUSE_NONE, -1, Never};
    for (Cloud& c : Clouds) {
        memset(&c, 0, sizeof c);
        c.Owner = HOUSE_NONE;
        c.Until = Never;
    }
    for (Hole& h : Holes) h = Hole{HOUSE_NONE, {-1, -1}, Never};
    for (Hop& h : Hops) h = Hop{TARGET_NONE, Never};
    for (ShockArc& a : Arcs) a = ShockArc{HOUSE_NONE, 0, 0, Never};
    for (News& n : Newsreel) n = News{HOUSE_NONE, GA_NONE, Never};
    for (Blast& b : Blasts) b = Blast{HOUSE_NONE, 0, Never};
    for (Potato& p : Potatoes) p = Potato{HOUSE_NONE, HOUSE_NONE, Never, 0};
    for (Booby& b : Boobies) b = Booby{HOUSE_NONE, -1};
    for (Decoy& d : Decoys) d = Decoy{HOUSE_NONE, TARGET_NONE, Never, 0};
    memset(Laughs, 0, sizeof Laughs);
    memset(Shocks, 0, sizeof Shocks);
    memset(Traps, 0, sizeof Traps);
    LaughCount = ShockCount = TrapCount = 0;
    NextArc = NextNews = NextBlast = 0;
    Chaining = false;
    Laughing = HOUSE_NONE;
    ++SmokeVersion;
}

void Gun_Ability_Tick()
{
    if (!Gun_Game_Active()) return;
    for (int i = 0; i < MPlayerCount; ++i) {
        Player& p = Seats[MPlayerHouses[i]];
        if (p.Serial != -1 && Frame > p.Expire) Begin(p, -1);
    }
    Tick_Traps();
    Tick_Laughs();
    Tick_Shocks();
    Tick_Vortices();
    Tick_Mines();
    Tick_Clouds();
    Tick_Wormholes();
    Tick_Potatoes();
    Tick_Decoys();
    for (int i = 0; i < MPlayerCount; ++i) {
        const HousesType owner = MPlayerHouses[i];
        if (!On(owner, GA_DANCE_OFF)) continue;
        if (TechnoClass* dancer = Fighter(owner)) Freeze(dancer);
    }
    Tick_Targets();
}

void Gun_Ability_AI()
{
    if (!Gun_Game_Active() || Gun_Game_Over()) return;
    for (int i = 0; i < MPlayerCount; ++i) {
        const HousesType owner = MPlayerHouses[i];
        HouseClass* house = HouseClass::As_Pointer(owner);
        if (!house || house->IsHuman || house->IsDefeated || house->IsToDie) continue;
        Player& p = Seats[owner];
        int token = 0;
        const char* left = nullptr;
        const char* right = nullptr;
        if (Gun_Game_Offer(house, &token, &left, &right)) {
            if (p.OfferToken != token) {
                p.OfferToken = token;
                p.OfferAt = Frame + Time(10);
            } else if (Frame >= p.OfferAt) {
                p.OfferAt = Frame + Time(10);
                Gun_Game_Choose(house, token * 2 + (right ? Random_Pick(0, 1) : 0));
            }
        }
        TechnoClass* me = Fighter(owner);
        if (!me || Frame < p.ThinkAt) continue;
        p.ThinkAt = Frame + Time(5);
        if (On(owner, GA_DANCE_OFF) || Gun_Game_Dodging(me)) continue;
        int range = 0;
        bool known = false;
        TechnoClass* enemy = Nearest_Enemy(owner, me, &range, &known);
        if (p.Held != GA_NONE && Frame >= p.UseAt) {
            Computer_Use(owner, me, known ? enemy : nullptr, range);
            me = Fighter(owner);
            if (!me) continue;
            enemy = Nearest_Enemy(owner, me, &range, &known);
        }
        Computer_Move(owner, me, enemy, range, known);
    }
}

unsigned Gun_Ability_Hash(unsigned hash)
{
    const auto mix = [&hash](unsigned value) { hash = (hash ^ value) * 16777619u; };
    for (int i = 0; i < MPlayerCount; ++i) {
        const Player& p = Seats[MPlayerHouses[i]];
        mix(unsigned(p.Held));
        for (int a = 0; a < GA_COUNT; ++a) mix(unsigned(p.Until[a] > Frame ? p.Until[a] : 0));
        mix(unsigned(p.RespawnCell));
        mix(unsigned(p.Serial));
        mix(p.Mask);
        mix(unsigned(p.Ability));
        mix(unsigned(p.Target));
        mix(unsigned(p.Count));
        mix(unsigned(p.Life));
        mix(unsigned(p.Expire));
        for (CELL cell : p.Cells) mix(unsigned(cell));
        mix(unsigned(p.ThinkAt));
        mix(unsigned(p.OfferToken));
        mix(unsigned(p.OfferAt));
        mix(unsigned(p.UseAt));
        mix(unsigned(p.UseSince));
        mix(unsigned(p.StrikeAt));
        mix(unsigned(p.RoamAt));
        mix(unsigned(p.CrateCell));
        mix(unsigned(p.CrateSince));
        mix(unsigned(p.SkipCrate));
        mix(unsigned(p.SkipUntil));
    }
    for (const Vortex& v : Vortices) {
        if (v.Until <= Frame) continue;
        mix(unsigned(v.Owner));
        mix(unsigned(v.Coord));
        mix(unsigned(v.Dir));
        mix(unsigned(v.Until));
        mix(unsigned(v.NextPulse));
        mix(unsigned(v.NextTurn));
    }
    for (const Mine& m : Mines) {
        if (m.Until <= Frame) continue;
        mix(unsigned(m.Owner));
        mix(unsigned(m.Cell));
        mix(unsigned(m.Until));
    }
    for (const Cloud& c : Clouds) {
        if (c.Until <= Frame) continue;
        mix(unsigned(c.Owner));
        mix(unsigned(c.Until));
        mix(unsigned(c.NextPuff));
        mix(unsigned(c.Puff));
        mix(unsigned(c.Count));
        for (int i = 0; i < c.Count; ++i) mix(unsigned(c.Cells[i]));
    }
    for (const Hole& h : Holes) {
        if (h.Until <= Frame) continue;
        mix(unsigned(h.Owner));
        mix(unsigned(h.Ends[0]));
        mix(unsigned(h.Ends[1]));
        mix(unsigned(h.Until));
    }
    for (const Hop& h : Hops) {
        if (h.Until <= Frame) continue;
        mix(unsigned(h.Unit));
        mix(unsigned(h.Until));
    }
    for (const ShockArc& a : Arcs) {
        mix(unsigned(a.Owner));
        mix(unsigned(a.From));
        mix(unsigned(a.To));
        mix(unsigned(a.At));
    }
    for (const News& n : Newsreel) {
        mix(unsigned(n.Owner));
        mix(unsigned(n.Ability));
        mix(unsigned(n.At));
    }
    for (const Blast& b : Blasts) {
        mix(unsigned(b.Owner));
        mix(unsigned(b.Where));
        mix(unsigned(b.At));
    }
    for (const Potato& p : Potatoes) {
        if (p.Until <= Frame) continue;
        mix(unsigned(p.Thrower));
        mix(unsigned(p.Holder));
        mix(unsigned(p.Until));
        mix(unsigned(p.PassReady));
    }
    for (const Booby& b : Boobies) {
        mix(unsigned(b.Owner));
        mix(unsigned(b.Cell));
    }
    for (const Decoy& d : Decoys) {
        if (d.Owner == HOUSE_NONE) continue;
        mix(unsigned(d.Owner));
        mix(unsigned(d.Unit));
        mix(unsigned(d.Until));
        mix(unsigned(d.WanderAt));
    }
    mix(unsigned(LaughCount));
    for (int i = 0; i < LaughCount; ++i) {
        mix(unsigned(Laughs[i].Owner));
        mix(unsigned(Laughs[i].Where));
    }
    mix(unsigned(ShockCount));
    for (int i = 0; i < ShockCount; ++i) {
        mix(unsigned(Shocks[i].Owner));
        mix(unsigned(Shocks[i].Victim));
        mix(unsigned(Shocks[i].Where));
        mix(unsigned(Shocks[i].Damage));
    }
    mix(unsigned(TrapCount));
    for (int i = 0; i < TrapCount; ++i) {
        mix(unsigned(Traps[i].Owner));
        mix(unsigned(Traps[i].Victim));
        mix(unsigned(Traps[i].Unit));
        mix(Traps[i].Life);
    }
    mix(unsigned(NextArc));
    mix(unsigned(NextNews));
    mix(unsigned(NextBlast));
    return hash;
}

bool Gun_Ability_Booby_Crate(FootClass* unit, CELL cell)
{
    for (Booby& booby : Boobies) {
        if (booby.Owner == HOUSE_NONE || booby.Cell != cell) continue;
        const HousesType owner = (HousesType)booby.Owner;
        booby.Owner = HOUSE_NONE;
        booby.Cell = -1;
        if (unit->Owner() != owner) {
            new AnimClass(ANIM_ART_EXP1, Cell_Coord(cell));
            // Never damaged here: see Trap. Each queued trap is a booby crate taken since
            // the last tick, so the queue is as long as the list of booby crates.
            if (TrapCount < MaxBooby) {
                Traps[TrapCount++] = Trap{owner, unit->Owner(), unit->As_Target(), Gun_Game_Life(unit->Owner())};
            }
        } else {
            new AnimClass(ANIM_PIFF, Cell_Coord(cell));
        }
        return true;
    }
    return false;
}

void Gun_Ability_Give(HousesType owner, CELL cell)
{
    Player* p = Of(owner);
    if (!p) return;
    const int ability = Random_Pick((int)GA_STEALTH, (int)GA_COUNT - 1);
    p->Held = ability;
    p->UseSince = Frame;
    p->UseAt = Frame + Time(15);
    new AnimClass(Crate_Anim(ability), Cell_Coord(cell));
}

void Gun_Ability_Died(TechnoClass* victim)
{
    if (!Gun_Game_Active() || !victim) return;
    const TARGET target = victim->As_Target();
    for (Decoy& decoy : Decoys) {
        if (decoy.Owner != HOUSE_NONE && decoy.Unit == target) {
            decoy.Owner = HOUSE_NONE;
            decoy.Until = Never;
        }
    }
    for (Hop& hop : Hops) {
        if (hop.Unit == target) hop.Until = Never;
    }
    if (!Is_Combatant(victim)) return;
    const HousesType owner = victim->Owner();
    Player& p = Seats[owner];
    if (p.Until[GA_LAST_LAUGH] > Frame && LaughCount < MaxLaughs) {
        Laughs[LaughCount++] = Laugh{owner, victim->Center_Coord()};
    }
    // What was on the combatant ends with it; the player's own sight and spare life stay.
    for (int a = 0; a < GA_COUNT; ++a) {
        if (a != GA_HUNTERS_EYE && a != GA_EXTRA_LIFE) p.Until[a] = 0;
    }
}

bool Gun_Ability_Spare(TechnoClass* victim)
{
    Player* p = victim ? Of(victim->Owner()) : nullptr;
    if (!p || p->Until[GA_EXTRA_LIFE] <= Frame) return false;
    p->Until[GA_EXTRA_LIFE] = 0;
    p->RespawnCell = Coord_Cell(victim->Coord);
    return true;
}

void Gun_Ability_Restore(FootClass* unit, COORDINATE where, DirType facing)
{
    Restore(unit, where, facing);
}

CELL Gun_Ability_Respawn_Cell(HousesType owner)
{
    Player* p = Of(owner);
    if (!p) return -1;
    const CELL cell = p->RespawnCell;
    p->RespawnCell = -1;
    return cell;
}

bool Gun_Ability_Decoy(const TechnoClass* unit)
{
    return Gun_Game_Active() && Is_Decoy(unit);
}

int Gun_Ability_Damage(const TechnoClass* unit, int damage)
{
    if (!Gun_Game_Active() || damage <= 0) return damage;
    const HousesType owner = unit->Owner();
    if (Laughing != HOUSE_NONE && owner == Laughing) return 0;
    if (!Is_Combatant(unit)) return damage;
    if (On(owner, GA_IRON_CURTAIN)) return 0;
    if (On(owner, GA_GIANT)) return (damage + 1) / 2;
    return damage;
}

void Gun_Ability_Damaged(TechnoClass* victim, TechnoClass* source, int damage, ResultType result)
{
    if (!Gun_Game_Active() || result == RESULT_NONE || damage <= 0) return;
    if (Is_Combatant(victim) && On(victim->Owner(), GA_STEALTH)) Seats[victim->Owner()].Until[GA_STEALTH] = 0;
    if (Chaining || !source || source->Owner() == victim->Owner() || !Is_Combatant(source)
        || !On(source->Owner(), GA_CHAIN_SHOCK) || ShockCount >= MaxShocks) {
        return;
    }
    Shocks[ShockCount++] = Shock{source->Owner(), victim->As_Target(), victim->Center_Coord(), MAX(10, damage / 2)};
}

void Gun_Ability_Fired(const TechnoClass* shooter)
{
    if (!Gun_Game_Active() || !Is_Combatant(shooter)) return;
    Seats[shooter->Owner()].Until[GA_STEALTH] = 0;
}

bool Gun_Ability_Fire_Blocked(const TechnoClass* shooter, TARGET target)
{
    if (!Gun_Game_Active() || !shooter || !Target_Legal(target)) return false;
    const HousesType me = shooter->Owner();
    if (Is_Decoy(shooter)) return true;
    if (Is_Combatant(shooter) && On(me, GA_DANCE_OFF)) return true;
    const ObjectClass* object = As_Object(target);
    if (object && object->Is_Techno()) {
        const TechnoClass* victim = static_cast<const TechnoClass*>(object);
        if (victim->Owner() == me) return false;
        if (Is_Combatant(victim) && On(victim->Owner(), GA_STEALTH)) return true;
    }
    const COORDINATE where = object ? object->Coord : As_Coord(target);
    const int smoke = Smoke_At(Coord_Cell(where));
    return smoke >= 0 && Smoke_At(Coord_Cell(shooter->Coord)) != smoke;
}

// Crushing kills without Take_Damage, so it needs its own test: an unarmed decoy crushes
// nobody, and nobody crushes a combatant inside its Iron Curtain.
bool Gun_Ability_Crush_Blocked(const TechnoClass* crusher, const ObjectClass* victim)
{
    if (!Gun_Game_Active()) return false;
    if (Is_Decoy(crusher)) return true;
    if (!victim || !victim->Is_Techno()) return false;
    const TechnoClass* unit = static_cast<const TechnoClass*>(victim);
    return Is_Combatant(unit) && On(unit->Owner(), GA_IRON_CURTAIN);
}

bool Gun_Ability_Dancing(HousesType owner)
{
    return Gun_Game_Active() && On(owner, GA_DANCE_OFF);
}

bool Gun_Ability_Hidden(HousesType viewer, const ObjectClass* object)
{
    if (!Gun_Game_Active() || !object || !object->Is_Techno()) return false;
    const TechnoClass* unit = static_cast<const TechnoClass*>(object);
    const HousesType owner = unit->Owner();
    if (owner == viewer || !Gun_Game_In_Match(owner)) return false;
    if (Is_Combatant(unit) && On(owner, GA_STEALTH)) return true;
    return Smoked_From(viewer, unit->Coord);
}

int Gun_Ability_Speed(const TechnoClass* unit, int speed)
{
    if (!Gun_Game_Active() || !Is_Combatant(unit) || !On(unit->Owner(), GA_OVERDRIVE)) return speed;
    return MIN(speed * 7 / 4, (int)MPH_LIGHT_SPEED);
}

int Gun_Ability_Rearm(const TechnoClass* unit, int delay)
{
    if (!Gun_Game_Active() || !Is_Combatant(unit) || !On(unit->Owner(), GA_OVERDRIVE)) return delay;
    return (delay + 1) / 2;
}

CELL Gun_Ability_Strike_Cell(HousesType owner)
{
    Player* p = Of(owner);
    if (!p) return -1;
    if (!p->StrikeAt) {
        p->StrikeAt = Frame + Time(20);
        return -1;
    }
    if (Frame < p->StrikeAt) return -1;
    const TechnoClass* me = Fighter(owner);
    const TechnoClass* best = nullptr;
    int best_distance = 0x7fffffff;
    Each_Enemy(owner, [&](const TechnoClass* enemy) {
        if (Gun_Ability_Hidden(owner, enemy) || !TD_Fair_AI_Knows(HouseClass::As_Pointer(owner), enemy)) return;
        const int d = me ? Distance(me->Coord, enemy->Coord) : 0;
        if (me && d < 3 * CellLeptons) return; // too close to strike without being caught in it
        if (!best || d < best_distance) {
            best = enemy;
            best_distance = d;
        }
    });
    if (!best) {
        p->StrikeAt = Frame + Time(10);
        return -1;
    }
    p->StrikeAt = 0;
    return Coord_Cell(best->Coord);
}

void Gun_Ability_Activate(HouseClass* house, int ability, int target, int count, int serial, int life,
                          const CELL* cells)
{
    if (!house || !Gun_Game_Active()) return;
    const HousesType owner = house->Class->House;
    Player* p = Of(owner);
    if (!p || count < 0 || count > GA_MAX_CELLS || ability <= GA_NONE || ability >= GA_COUNT) return;
    if (p->Serial != serial || p->Ability != GA_NONE) Begin(*p, serial);
    p->Ability = ability;
    p->Target = target;
    p->Count = count;
    p->Life = life;
    p->Expire = Frame + Time(AssemblyTime);
    for (int i = 0; i < 4 && i < count; ++i) {
        p->Cells[i] = cells[i];
        p->Mask |= 1u << i;
    }
    Try_Apply(owner);
}

void Gun_Ability_Cells(HouseClass* house, int serial, int index, const CELL* cells)
{
    if (!house || !Gun_Game_Active() || index < 0 || index >= GA_MAX_CELLS) return;
    const HousesType owner = house->Class->House;
    Player* p = Of(owner);
    if (!p) return;
    if (p->Serial != serial) {
        Begin(*p, serial);
        p->Expire = Frame + Time(AssemblyTime);
    }
    for (int i = 0; i < 7 && index + i < GA_MAX_CELLS; ++i) {
        p->Cells[index + i] = cells[i];
        p->Mask |= 1u << (index + i);
    }
    Try_Apply(owner);
}

bool Gun_Ability_Test_Enabled()
{
    const char* value = getenv("OPENCNC3D_GUN_TEST");
    return value && *value && strcmp(value, "0") != 0;
}

void Gun_Ability_Grant(HouseClass* house, int ability)
{
    if (!house || !Gun_Ability_Test_Enabled() || ability <= GA_NONE || ability >= GA_COUNT) return;
    Player* p = Of(house->Class->House);
    if (!p) return;
    p->Held = ability;
    p->UseSince = Frame;
    p->UseAt = Frame + Time(15);
}

int Gun_Ability_Held(HousesType owner)
{
    const Player* p = Of(owner);
    return p ? p->Held : GA_NONE;
}

bool Gun_Ability_Check(HousesType owner, int ability, int target, int count, const CELL* cells)
{
    return Gun_Game_Active() && Check(owner, ability, target, count, cells);
}

unsigned Gun_Ability_Flags(HousesType viewer, const ObjectClass* object)
{
    if (!Gun_Game_Active() || !object || !object->Is_Techno()) return 0;
    const TechnoClass* unit = static_cast<const TechnoClass*>(object);
    const HousesType owner = unit->Owner();
    if (!Of(owner)) return 0;
    unsigned flags = Cloud_Of(Coord_Cell(unit->Coord)) >= 0 ? GAF_SMOKED : 0;
    const bool decoy = Is_Decoy(unit);
    if (decoy && viewer == owner) return flags | GAF_DECOY;
    if (!decoy && !Is_Combatant(unit)) return flags;
    // To anyone but its owner a decoy wears the same visible effects as the real one.
    if (On(owner, GA_IRON_CURTAIN)) flags |= GAF_IRON;
    if (On(owner, GA_OVERDRIVE)) flags |= GAF_OVERDRIVE;
    if (On(owner, GA_GIANT)) flags |= GAF_GIANT;
    if (On(owner, GA_CHAIN_SHOCK)) flags |= GAF_CHAIN;
    if (On(owner, GA_EXTRA_LIFE)) flags |= GAF_EXTRA_LIFE;
    if (On(owner, GA_LAST_LAUGH)) flags |= GAF_LAST_LAUGH;
    if (decoy) return flags;
    if (On(owner, GA_STEALTH)) flags |= GAF_STEALTH;
    if (Holding(owner)) flags |= GAF_HOT_POTATO;
    if (On(owner, GA_DANCE_OFF)) flags |= GAF_DANCING;
    if (On(owner, GA_MIRROR)) flags |= GAF_MIRRORED;
    return flags;
}

int Gun_Ability_Effects(HousesType viewer, GunEffect* out, int max)
{
    if (!Gun_Game_Active() || !out || max <= 0) return 0;
    int n = 0;
    const auto add = [&](int kind, int owner, COORDINATE a, COORDINATE b, int age, int value) {
        if (n >= max) return;
        GunEffect& e = out[n++];
        e.kind = kind;
        e.owner = owner;
        e.x = Coord_X(a);
        e.y = Coord_Y(a);
        e.x2 = Coord_X(b);
        e.y2 = Coord_Y(b);
        e.age = age;
        e.value = value;
    };
    for (const News& news : Newsreel) {
        if (news.At == Never || Frame - news.At > AnnounceAge) continue;
        // Traps are secret, like their GFX_MINE and GFX_BOOBY marks: an announcement to
        // everyone, on the frame a crate appears beside the owner, would give them away.
        if ((news.Ability == GA_BOOBY_CRATE || news.Ability == GA_MINEFIELD) && news.Owner != viewer) continue;
        add(GFX_ANNOUNCE, news.Owner, 0, 0, Frame - news.At, news.Ability);
    }
    for (const Vortex& v : Vortices) {
        if (v.Until > Frame) add(GFX_VORTEX, v.Owner, v.Coord, v.Coord, 0, v.Until - Frame);
    }
    for (const Hole& h : Holes) {
        if (h.Until > Frame) add(GFX_WORMHOLE, h.Owner, Cell_Coord(h.Ends[0]), Cell_Coord(h.Ends[1]), 0, h.Until - Frame);
    }
    for (const Cloud& c : Clouds) {
        if (c.Until <= Frame) continue;
        for (int i = 0; i < c.Count; ++i) {
            add(GFX_SMOKE, c.Owner, Cell_Coord(c.Cells[i]), Cell_Coord(c.Cells[i]), 0, c.Until - Frame);
        }
    }
    for (const ShockArc& arc : Arcs) {
        if (arc.At != Never && Frame - arc.At <= ArcAge) add(GFX_ARC, arc.Owner, arc.From, arc.To, Frame - arc.At, 0);
    }
    for (const Potato& potato : Potatoes) {
        if (potato.Until <= Frame) continue;
        const TechnoClass* holder = Fighter((HousesType)potato.Holder);
        if (!holder || Gun_Ability_Hidden(viewer, holder)) continue;
        add(GFX_POTATO, potato.Thrower, holder->Center_Coord(), holder->Center_Coord(), 0, potato.Until - Frame);
    }
    for (const Blast& blast : Blasts) {
        if (blast.At != Never && Frame - blast.At <= BlastAge) add(GFX_BLAST, blast.Owner, blast.Where, blast.Where, Frame - blast.At, 0);
    }
    for (const Mine& mine : Mines) {
        if (mine.Until > Frame && mine.Owner == viewer) {
            add(GFX_MINE, mine.Owner, Cell_Coord(mine.Cell), Cell_Coord(mine.Cell), 0, mine.Until - Frame);
        }
    }
    for (const Booby& booby : Boobies) {
        if (booby.Owner != HOUSE_NONE && booby.Owner == viewer) {
            add(GFX_BOOBY, booby.Owner, Cell_Coord(booby.Cell), Cell_Coord(booby.Cell), 0, 0);
        }
    }
    if (On(viewer, GA_HUNTERS_EYE)) {
        const HousesType leader = Leader();
        for (int i = 0; i < MPlayerCount; ++i) {
            const HousesType other = MPlayerHouses[i];
            const TechnoClass* enemy = other == viewer ? nullptr : Fighter(other);
            // It sees through fog, not through stealth or smoke the viewer is outside of.
            if (!enemy || Gun_Ability_Hidden(viewer, enemy)) continue;
            add(GFX_REVEAL, other, enemy->Center_Coord(), enemy->Center_Coord(), 0, other == leader ? 1 : 0);
        }
    }
    return n;
}

int Gun_Ability_Timers(HousesType owner, int32_t* remaining, int32_t* ticks_per_second)
{
    if (remaining) {
        for (int a = 0; a < GA_COUNT; ++a) remaining[a] = 0;
    }
    if (ticks_per_second) *ticks_per_second = Gun_Game_Ticks_Per_Second();
    const Player* p = Gun_Game_Active() ? Of(owner) : nullptr;
    if (!p) return 0;
    if (!remaining) return 1;
    for (int a = GA_STEALTH; a < GA_COUNT; ++a) {
        if (p->Until[a] > Frame) remaining[a] = p->Until[a] - Frame;
    }
    for (const Potato& potato : Potatoes) {
        if (potato.Until > Frame && potato.Holder == owner) remaining[GA_HOT_POTATO] = MAX(remaining[GA_HOT_POTATO], potato.Until - Frame);
    }
    return 1;
}
