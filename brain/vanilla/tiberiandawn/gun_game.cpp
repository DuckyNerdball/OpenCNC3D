// OpenCNC3D Tiberian Dawn LAN Gun Game.
#include "function.h"
#include "gun_game.h"
#include "gun_game_ability.h"
#include "sidebarglyphx.h"

namespace {

struct Tier
{
    RTTIType Kind;
    int Choices[2];
};

constexpr int None = -1;
const Tier Ladder[] = {
    {RTTI_INFANTRY, {INFANTRY_E1, None}},
    {RTTI_INFANTRY, {INFANTRY_E2, INFANTRY_E3}},
    {RTTI_INFANTRY, {INFANTRY_E4, INFANTRY_E5}},
    {RTTI_INFANTRY, {INFANTRY_RA_SHOCK, INFANTRY_RA_TANYA}},
    {RTTI_INFANTRY, {INFANTRY_RAMBO, None}},
    {RTTI_UNIT, {UNIT_BUGGY, UNIT_JEEP}},
    {RTTI_UNIT, {UNIT_LTANK, UNIT_MTANK}},
    {RTTI_UNIT, {UNIT_BIKE, UNIT_APC}},
    {RTTI_UNIT, {UNIT_FTANK, UNIT_ARTY}},
    {RTTI_UNIT, {UNIT_STANK, UNIT_MSAM}},
    {RTTI_UNIT, {UNIT_RA_HEAVY, UNIT_RA_CHRONO}},
    {RTTI_UNIT, {UNIT_RA_V2, UNIT_RA_TESLA}},
    {RTTI_UNIT, {UNIT_MLRS, None}},
    {RTTI_AIRCRAFT, {AIRCRAFT_ORCA, AIRCRAFT_HELICOPTER}},
    {RTTI_AIRCRAFT, {AIRCRAFT_RA_HIND, AIRCRAFT_RA_YAK}},
    {RTTI_AIRCRAFT, {AIRCRAFT_RA_MIG, None}},
    {RTTI_UNIT, {UNIT_HTANK, None}},
};
constexpr int LastTier = sizeof Ladder / sizeof Ladder[0] - 1;
constexpr int RespawnDelay = 2 * TICKS_PER_SECOND;
constexpr int CreditWindow = 5 * TICKS_PER_SECOND;
constexpr int RollDistance = 384; // one and a half map cells
constexpr int JumpApex = 256;    // one map cell in height

struct GunPlayer
{
    TARGET Unit;
    int Tier;
    int Choice;
    unsigned Kills;
    int RespawnAt;
    int Token;
    bool Offered;
    unsigned Life;
    HousesType LastAttacker;
    unsigned LastAttackerLife;
    int LastHitAt;
    int DodgeStart;
    int DodgeEnd;
    int DodgeReady;
    COORDINATE DodgeOrigin;
    DirType DodgeFacing;
    bool DodgeBlocked;
    int JumpSpeed;
    int ScudReady;
    int ChronoReady;
};

bool Enabled;
GunPlayer Progress[HOUSE_COUNT];
HousesType Winner;
CELL Starts[MAX_PLAYERS];
int StartCount;
int DodgeRate = 2 * TICKS_PER_SECOND;
int CratesPending;
int NextCrateAt;
bool StrikeDamage;

bool In_Match(HousesType house)
{
    for (int i = 0; i < MPlayerCount; ++i) {
        if (MPlayerHouses[i] == house) return true;
    }
    return false;
}

const TechnoTypeClass* Choice_Type(int tier, int choice)
{
    const Tier& step = Ladder[tier];
    const int type = step.Choices[choice];
    if (type == None) return nullptr;
    if (step.Kind == RTTI_INFANTRY) return &InfantryTypeClass::As_Reference((InfantryType)type);
    if (step.Kind == RTTI_AIRCRAFT) return &AircraftTypeClass::As_Reference((AircraftType)type);
    return &UnitTypeClass::As_Reference((UnitType)type);
}

GunPlayer* Dodging_Player(const TechnoClass* unit)
{
    if (!unit || !Gun_Game_Active() || !In_Match(unit->Owner())) return nullptr;
    GunPlayer& player = Progress[unit->Owner()];
    return player.Unit == unit->As_Target() && Frame < player.DodgeEnd ? &player : nullptr;
}

TechnoClass* Spawn(HouseClass* house, int tier, int choice, CELL center, int max_radius)
{
    const Tier& step = Ladder[tier];
    const int type = step.Choices[choice];
    const HousesType owner = house->Class->House;
    FootClass* unit;
    if (step.Kind == RTTI_INFANTRY) unit = new InfantryClass((InfantryType)type, owner);
    else if (step.Kind == RTTI_AIRCRAFT) {
        AircraftClass* craft = new AircraftClass((AircraftType)type, owner);
        if (craft && !craft->Class->IsFixedWing) craft->Altitude = 0;
        unit = craft;
    } else unit = new UnitClass((UnitType)type, owner);
    if (!unit) return nullptr;

    for (int radius = 0; radius <= max_radius; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (abs(dx) != radius && abs(dy) != radius) continue;
                const int x = Cell_X(center) + dx;
                const int y = Cell_Y(center) + dy;
                if (x < Map.MapCellX || y < Map.MapCellY
                    || x >= Map.MapCellX + Map.MapCellWidth
                    || y >= Map.MapCellY + Map.MapCellHeight) continue;
                const CELL cell = XY_Cell(x, y);
                if (Map[cell].Cell_Techno() || unit->Can_Enter_Cell(cell) != MOVE_OK) continue;
                if (unit->Unlimbo(Cell_Coord(cell), DIR_N)) {
                    unit->Assign_Mission(MISSION_GUARD);
                    return unit;
                }
            }
        }
    }
    delete unit;
    return nullptr;
}

} // namespace

bool Gun_Game_Active()
{
    return Enabled && GameToPlay == GAME_GLYPHX_MULTIPLAYER;
}

void Gun_Game_Reset(bool enabled)
{
    Enabled = enabled;
    memset(Progress, 0, sizeof Progress);
    Winner = HOUSE_NONE;
    StartCount = 0;
    CratesPending = 0;
    NextCrateAt = 0;
    Gun_Ability_Reset();
}

void Gun_Game_Set_Rate(int ticks_per_two_seconds)
{
    DodgeRate = MIN(72, MAX(15, ticks_per_two_seconds));
}

bool Gun_Game_Start()
{
    for (int tier = 0; tier <= LastTier; ++tier)
        for (int option = 0; option < 2; ++option)
            if (const auto* type = Choice_Type(tier, option))
                if (!type->Get_Image_Data()) return false;
    // The engine's multiplayer start index is compacted over populated waypoints.
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const CELL cell = Scen.Waypoint[i];
        if (cell != -1) Starts[StartCount++] = cell;
    }
    if (StartCount < MPlayerCount) return false;
    for (int i = 0; i < MPlayerCount; ++i) {
        HouseClass* house = HouseClass::As_Pointer(MPlayerHouses[i]);
        const int index = house->StartLocationOverride;
        if (index < 0 || index >= StartCount) return false;
        TechnoClass* unit = Spawn(house, 0, 0, Starts[index], 8);
        if (!unit) return false;
        GunPlayer& player = Progress[MPlayerHouses[i]];
        player.Unit = unit->As_Target();
        player.Life = 1;
    }
    CratesPending = MIN(4, MPlayerCount);
    return true;
}

void Gun_Game_Hit(TechnoClass* victim, TechnoClass* source)
{
    if (!Gun_Game_Active() || !source || source->Owner() == victim->Owner()
        || !In_Match(victim->Owner()) || !In_Match(source->Owner())) return;
    GunPlayer& hurt = Progress[victim->Owner()];
    const GunPlayer& attacker = Progress[source->Owner()];
    if (hurt.Unit != victim->As_Target() || attacker.Unit != source->As_Target()) return;
    hurt.LastAttacker = source->Owner();
    hurt.LastAttackerLife = attacker.Life;
    hurt.LastHitAt = Frame;
}

void Gun_Game_Kill(TechnoClass* victim, TechnoClass* source)
{
    if (!Gun_Game_Active()) return;
    Gun_Ability_Died(victim);
    if (Winner != HOUSE_NONE || !In_Match(victim->Owner())) return;
    GunPlayer& dead = Progress[victim->Owner()];
    if (dead.Unit != victim->As_Target()) return;
    dead.Unit = TARGET_NONE;
    dead.DodgeEnd = 0;
    dead.JumpSpeed = 0;
    if (Gun_Ability_Spare(victim)) {
        // Extra Life: this death does not count, nobody scores it and the player
        // comes straight back where they fell, keeping a pending promotion.
        dead.RespawnAt = Frame;
        dead.LastAttackerLife = 0;
        return;
    }
    dead.Offered = false;
    dead.RespawnAt = Frame + RespawnDelay;

    HousesType scorer = HOUSE_NONE;
    const bool strike_plane = source && source->What_Am_I() == RTTI_AIRCRAFT
        && *static_cast<const AircraftClass*>(source) == AIRCRAFT_A10;
    const bool direct = source && In_Match(source->Owner())
        && Progress[source->Owner()].Unit == source->As_Target();
    if (source && source->Owner() != victim->Owner() && (direct || (strike_plane && In_Match(source->Owner())))) {
        scorer = source->Owner();
    } else if (!source && !StrikeDamage && dead.LastAttackerLife && Frame - dead.LastHitAt <= CreditWindow
        && Progress[dead.LastAttacker].Life == dead.LastAttackerLife) {
        // Fire and Tiberium can finish off a unit after an enemy hit. Ability and
        // superweapon damage never comes here: it has no source only when its owner has
        // no combatant or hit itself, and then the kill belongs to nobody, never to
        // whoever happened to hit the victim last.
        scorer = dead.LastAttacker;
    }
    dead.LastAttackerLife = 0;
    if (scorer == HOUSE_NONE) return;
    GunPlayer& killer = Progress[scorer];
    const TechnoClass* combatant = As_Techno(killer.Unit);
    if (!combatant || combatant->Strength <= 0
        || combatant->House->IsToDie || combatant->House->IsDefeated) return;
    if (killer.Tier == LastTier && (!direct || StrikeDamage)) return;
    ++killer.Kills;
    if (killer.Tier == LastTier) {
        Winner = scorer;
    } else if (!killer.Offered) {
        ++killer.Token;
        killer.Offered = true;
    }
}

void Gun_Game_Tick()
{
    if (!Gun_Game_Active()) return;
    if (Winner != HOUSE_NONE) {
        for (int i = 0; i < MPlayerCount; ++i) {
            HouseClass::As_Pointer(MPlayerHouses[i])->IsDefeated = MPlayerHouses[i] != Winner;
        }
        PlayerWins = true; // the existing multiplayer result reads IsDefeated for each player
        return;
    }
    Gun_Ability_Tick();
    if (CratesPending && Frame >= NextCrateAt) {
        while (CratesPending && Map.Place_Random_Crate()) --CratesPending;
        NextCrateAt = Frame + TICKS_PER_SECOND;
    }
    for (int i = 0; i < MPlayerCount; ++i) {
        const HousesType owner = MPlayerHouses[i];
        HouseClass* house = HouseClass::As_Pointer(owner);
        GunPlayer& player = Progress[owner];
        if (AircraftClass* craft = As_Aircraft(player.Unit))
            if (!craft->Altitude && !craft->IsInLimbo) Map[Coord_Cell(craft->Coord)].Goodie_Check(craft);
        if (house->IsDefeated || house->IsToDie || player.Unit != TARGET_NONE || Frame < player.RespawnAt) continue;
        TechnoClass* unit = nullptr;
        const CELL fell = Gun_Ability_Respawn_Cell(owner);
        if (fell != -1) unit = Spawn(house, player.Tier, player.Choice, fell, 8);
        const int first = unit ? 0 : Random_Pick(0, StartCount - 1);
        for (int n = 0; !unit && n < StartCount; ++n) {
            unit = Spawn(house, player.Tier, player.Choice, Starts[(first + n) % StartCount], 8);
        }
        if (unit) {
            player.Unit = unit->As_Target();
            ++player.Life;
            player.ScudReady = 0;
            player.ChronoReady = 0;
        }
        player.RespawnAt = Frame + RespawnDelay;
    }
    Gun_Ability_AI();
}

const char* Gun_Game_Standing(HousesType owner, int* level, unsigned* kills, bool* alive)
{
    if (!Gun_Game_Active() || !In_Match(owner)) return nullptr;
    const GunPlayer& player = Progress[owner];
    const HouseClass* house = HouseClass::As_Pointer(owner);
    *level = player.Tier + 1;
    *kills = player.Kills;
    *alive = player.Unit != TARGET_NONE && !house->IsToDie && !house->IsDefeated;
    return Choice_Type(player.Tier, player.Choice)->IniName;
}

bool Gun_Game_Offer(HouseClass* house, int* token, const char** left, const char** right)
{
    if (!Gun_Game_Active() || !house || !In_Match(house->Class->House)
        || house->IsToDie || house->IsDefeated || Winner != HOUSE_NONE) return false;
    const GunPlayer& player = Progress[house->Class->House];
    if (!player.Offered) return false;
    *token = player.Token;
    *left = Choice_Type(player.Tier + 1, 0)->IniName;
    const TechnoTypeClass* alternative = Choice_Type(player.Tier + 1, 1);
    *right = alternative ? alternative->IniName : nullptr;
    return true;
}

void Gun_Game_Choose(HouseClass* house, int command)
{
    const char* left;
    const char* right;
    int token;
    const int choice = command & 1;
    if (!Gun_Game_Offer(house, &token, &left, &right) || command / 2 != token
        || (choice && !right)) return;
    GunPlayer& player = Progress[house->Class->House];
    TechnoClass* current = As_Techno(player.Unit);
    if (!current || current->Strength <= 0) return;
    TechnoClass* next = Spawn(house, player.Tier + 1, choice, Coord_Cell(current->Coord), 8);
    if (!next) return; // keep the current unit and offer so the player can move and retry
    player.Unit = next->As_Target();
    ++player.Tier;
    player.Choice = choice;
    ++player.Life;
    player.Offered = false;
    player.DodgeEnd = 0;
    player.JumpSpeed = 0;
    player.ScudReady = 0;
    player.ChronoReady = 0;
    player.LastAttackerLife = 0;
    delete current;
}

int Gun_Game_Dodge_Token(HousesType owner)
{
    if (!Gun_Game_Active() || Winner != HOUSE_NONE || !In_Match(owner)) return 0;
    const GunPlayer& player = Progress[owner];
    const TechnoClass* unit = As_Techno(player.Unit);
    if (Frame < player.DodgeReady || !unit || unit->What_Am_I() == RTTI_AIRCRAFT
        || unit->Strength <= 0 || unit->IsInLimbo
        || unit->House->IsToDie || unit->House->IsDefeated || Gun_Ability_Dancing(owner)) return 0;
    return (int)player.Life;
}

void Gun_Game_Dodge(HouseClass* house, int token)
{
    if (!house || token <= 0 || Gun_Game_Dodge_Token(house->Class->House) != token) return;
    GunPlayer& player = Progress[house->Class->House];
    FootClass* unit = static_cast<FootClass*>(As_Techno(player.Unit));
    const bool roll = unit->What_Am_I() == RTTI_INFANTRY;
    unit->IsFiring = false;
    if (roll) {
        unit->Assign_Target(TARGET_NONE);
        unit->Assign_Destination(TARGET_NONE);
        unit->Path[0] = FACING_NONE;
        unit->Stop_Driver();
        unit->Assign_Mission(MISSION_GUARD);
        unit->Commence();
        static_cast<InfantryClass*>(unit)->Do_Action(DO_STAND_READY, true);
    } else {
        UnitClass* vehicle = static_cast<UnitClass*>(unit);
        player.JumpSpeed = vehicle->Forward_Speed();
        vehicle->Stop_Driver(); // Release the old track's reservations before clearing it.
        vehicle->Force_Track(-1, 0);
        vehicle->Path[0] = FACING_NONE;
        vehicle->SimLeptonX = vehicle->SimLeptonY = 0;
    }
    player.DodgeStart = Frame;
    player.DodgeEnd = Frame + (roll ? (DodgeRate + 1) / 2 : (3 * DodgeRate + 2) / 4);
    player.DodgeReady = player.DodgeEnd + (roll ? (DodgeRate + 1) / 2 : DodgeRate);
    player.DodgeOrigin = unit->Coord;
    player.DodgeFacing = unit->PrimaryFacing.Current();
    player.DodgeBlocked = false;
}

bool Gun_Game_Dodging(const TechnoClass* unit)
{
    return Dodging_Player(unit) != nullptr;
}

bool Gun_Game_Airborne(const TechnoClass* unit)
{
    return unit && unit->What_Am_I() == RTTI_UNIT && Gun_Game_Dodging(unit);
}

bool Gun_Game_Scud_Ready(const TechnoClass* unit)
{
    return !Gun_Game_Active() || !In_Match(unit->Owner())
        || Frame >= Progress[unit->Owner()].ScudReady;
}

void Gun_Game_Scud_Fired(const TechnoClass* unit)
{
    if (Gun_Game_Active() && In_Match(unit->Owner()))
        Progress[unit->Owner()].ScudReady = Frame + 400;
}

int Gun_Game_Chrono_Command(HousesType owner, int x, int y)
{
    const TechnoClass* unit = Gun_Game_Combatant(owner);
    if (Winner != HOUSE_NONE || !unit || unit->What_Am_I() != RTTI_UNIT
        || *static_cast<const UnitClass*>(unit) != UNIT_RA_CHRONO || unit->Strength <= 0
        || Frame < Progress[owner].ChronoReady || Gun_Game_Dodging(unit) || Gun_Ability_Dancing(owner)) return 0;
    if (x < Map.MapCellX || y < Map.MapCellY
        || x >= Map.MapCellX + Map.MapCellWidth || y >= Map.MapCellY + Map.MapCellHeight) return 0;
    const CELL cell = XY_Cell(x, y);
    if (Coord_Cell(unit->Coord) == cell
        || static_cast<const UnitClass*>(unit)->Can_Enter_Cell(cell) != MOVE_OK) return 0;
    return (Progress[owner].Life << 16) | cell;
}

void Gun_Game_Chrono(HouseClass* house, int command)
{
    if (!house || !command) return;
    const HousesType owner = house->Class->House;
    const CELL cell = command & 0xffff;
    if (Gun_Game_Chrono_Command(owner, Cell_X(cell), Cell_Y(cell)) != command) return;
    UnitClass* unit = static_cast<UnitClass*>(Gun_Game_Combatant(owner));
    const COORDINATE old = unit->Coord;
    const DirType facing = unit->PrimaryFacing.Current();
    unit->Assign_Target(TARGET_NONE);
    unit->Assign_Destination(TARGET_NONE);
    unit->Stop_Driver();
    unit->Path[0] = FACING_NONE;
    unit->Limbo();
    if (!unit->Unlimbo(Cell_Coord(cell), facing)) {
        // Back where it stood: a combatant left in limbo is never respawned.
        Gun_Ability_Restore(unit, old, facing);
        unit->Assign_Mission(MISSION_GUARD);
        unit->Commence();
        return;
    }
    unit->Assign_Mission(MISSION_GUARD);
    unit->Commence();
    new AnimClass(ANIM_PIFF, old);
    new AnimClass(ANIM_PIFF, unit->Coord);
    Progress[owner].ChronoReady = Frame + 10 * DodgeRate;
}

bool Gun_Game_Combat_Aircraft(const TechnoClass* unit)
{
    return unit && unit->What_Am_I() == RTTI_AIRCRAFT && Gun_Game_Active() && In_Match(unit->Owner())
        && (Progress[unit->Owner()].Unit == unit->As_Target() || Gun_Ability_Decoy(unit));
}

TechnoClass* Gun_Game_Combatant(HousesType owner)
{
    if (!Gun_Game_Active() || !In_Match(owner)) return nullptr;
    return As_Techno(Progress[owner].Unit);
}

void Gun_Game_Strike_Damage(COORDINATE coord, int damage, HousesType owner, WarheadType warhead)
{
    const bool strike = StrikeDamage;
    StrikeDamage = true;
    Explosion_Damage(coord, damage, Gun_Game_Combatant(owner), warhead);
    StrikeDamage = strike;
}

bool Gun_Game_Collect_Crate(FootClass* unit, CELL cell)
{
    if (!Gun_Game_Active() || !unit || !In_Match(unit->Owner())
        || Progress[unit->Owner()].Unit != unit->As_Target()) return false;
    if (Gun_Ability_Booby_Crate(unit, cell)) return true;
    HouseClass* house = unit->House;
    // Three crates in four hold an ability, and so does every crate while a
    // superweapon is still unused.
    if (house->IonCannon.Is_Present() || house->NukeStrike.Is_Present() || house->AirStrike.Is_Present()
        || Random_Pick(0, 3) != 0) {
        Gun_Ability_Give(unit->Owner(), cell);
        ++CratesPending;
        return true;
    }
    const SpecialWeaponType weapon = (SpecialWeaponType)Random_Pick(SPC_ION_CANNON, SPC_AIR_STRIKE);
    const auto charge = [&](SuperClass& special, AnimType anim) {
        special.Enable(true);
        special.Forced_Charge(false);
        new AnimClass(anim, Cell_Coord(cell));
    };
    switch (weapon) {
    case SPC_ION_CANNON:   charge(house->IonCannon, ANIM_CRATE_EARTH); break;
    case SPC_NUCLEAR_BOMB: charge(house->NukeStrike, ANIM_CRATE_MISSILE); break;
    case SPC_AIR_STRIKE:   charge(house->AirStrike, ANIM_CRATE_DEVIATOR); break;
    default: return false;
    }
    Sidebar_Glyphx_Add(RTTI_SPECIAL, weapon, house);
    ++CratesPending;
    return true;
}

int Gun_Game_Roll_Phase(const TechnoClass* unit)
{
    const GunPlayer* player = Dodging_Player(unit);
    if (!player || unit->What_Am_I() != RTTI_INFANTRY) return 0;
    return 1 + (Frame - player->DodgeStart) * 256 / (player->DodgeEnd - player->DodgeStart);
}

int Gun_Game_Jump_Height(const TechnoClass* unit)
{
    const GunPlayer* player = Dodging_Player(unit);
    if (!player || unit->What_Am_I() != RTTI_UNIT) return 0;
    const int duration = player->DodgeEnd - player->DodgeStart;
    const int elapsed = Frame - player->DodgeStart;
    return 4 * JumpApex * elapsed * (duration - elapsed) / (duration * duration);
}

static bool Dodge_Can_Enter(const FootClass* unit, CELL from, CELL to)
{
    if (from == to) return true;
    if (!Map.In_Radar(to) || unit->Can_Enter_Cell(to) != MOVE_OK) return false;
    return Cell_X(from) == Cell_X(to) || Cell_Y(from) == Cell_Y(to)
        || (unit->Can_Enter_Cell(XY_Cell(Cell_X(from), Cell_Y(to))) == MOVE_OK
            && unit->Can_Enter_Cell(XY_Cell(Cell_X(to), Cell_Y(from))) == MOVE_OK);
}

bool Gun_Game_Jump_Step(UnitClass* unit)
{
    if (!Gun_Game_Active() || !In_Match(unit->Owner())) return false;
    GunPlayer& player = Progress[unit->Owner()];
    if (player.Unit != unit->As_Target()) return false;
    if (Frame >= player.DodgeEnd) {
        if (player.JumpSpeed) {
            player.JumpSpeed = 0;
            const CELL landed = Coord_Cell(unit->Coord);
            if (landed != Coord_Cell(player.DodgeOrigin)) Map[landed].Goodie_Check(unit);
        }
        return false;
    }
    if (!player.JumpSpeed || player.DodgeBlocked) return true;
    const int elapsed = Frame - player.DodgeStart + 1;
    const COORDINATE to = Coord_Move(player.DodgeOrigin, player.DodgeFacing, player.JumpSpeed * elapsed);
    if (to == unit->Coord) return true;
    if (!Dodge_Can_Enter(unit, Coord_Cell(unit->Coord), Coord_Cell(to))) {
        player.DodgeBlocked = true;
        return true;
    }
    unit->Mark(MARK_UP);
    unit->Coord = to;
    unit->Mark(MARK_DOWN);
    return true;
}

void Gun_Game_Roll_Step(InfantryClass* unit)
{
    GunPlayer* player = Dodging_Player(unit);
    if (!player || player->DodgeBlocked) return;
    const int duration = player->DodgeEnd - player->DodgeStart;
    const int elapsed = Frame - player->DodgeStart;
    const COORDINATE to = Coord_Move(player->DodgeOrigin, player->DodgeFacing, RollDistance * elapsed / duration);
    const CELL from = Coord_Cell(unit->Coord), cell = Coord_Cell(to);
    if (to == unit->Coord) return;
    if (!Dodge_Can_Enter(unit, from, cell)) {
        player->DodgeBlocked = true;
        return;
    }
    if (from != cell || CellClass::Spot_Index(unit->Coord) != CellClass::Spot_Index(to)) {
        const COORDINATE free = Map[cell].Closest_Free_Spot(to);
        if (!free || CellClass::Spot_Index(free) != CellClass::Spot_Index(to)) {
            player->DodgeBlocked = true;
            return;
        }
    }
    unit->Mark(MARK_UP);
    unit->Clear_Occupy_Bit(unit->Coord);
    unit->Coord = to;
    unit->Set_Occupy_Bit(to);
    unit->Mark(MARK_DOWN);
    if (cell != from) {
        Map[cell].Goodie_Check(unit);
        unit->Per_Cell_Process(true);
    }
}

int Gun_Game_Dodge_Damage(const TechnoClass* unit, int damage)
{
    if (damage <= 0) return damage;
    if (Gun_Game_Airborne(unit)) return 0;
    damage = Gun_Ability_Damage(unit, damage);
    if (damage <= 0) return 0;
    const int roll = Gun_Game_Roll_Phase(unit);
    return roll >= 65 && roll <= 193 ? (damage + 1) / 2 : damage;
}

unsigned Gun_Game_Hash()
{
    unsigned hash = 2166136261u;
    for (int i = 0; i < MPlayerCount; ++i) {
        const GunPlayer& p = Progress[MPlayerHouses[i]];
        const HouseClass* house = HouseClass::As_Pointer(MPlayerHouses[i]);
        const unsigned values[] = {unsigned(p.Unit), unsigned(p.Tier), unsigned(p.Choice), p.Kills,
                                   unsigned(p.Token), unsigned(p.RespawnAt), unsigned(p.Offered), p.Life,
                                   p.LastAttackerLife, p.LastAttackerLife ? unsigned(p.LastAttacker) : 0u,
                                   p.LastAttackerLife ? unsigned(p.LastHitAt) : 0u,
                                   unsigned(p.DodgeStart), unsigned(p.DodgeEnd), unsigned(p.DodgeReady),
                                   unsigned(p.DodgeOrigin), unsigned(p.DodgeFacing), unsigned(p.DodgeBlocked),
                                   unsigned(p.JumpSpeed),
                                   unsigned(p.ScudReady), unsigned(p.ChronoReady),
                                   unsigned(house->IonCannon.Is_Ready()), unsigned(house->NukeStrike.Is_Ready()),
                                   unsigned(house->AirStrike.Is_Ready())};
        for (unsigned value : values) hash = (hash ^ value) * 16777619u;
    }
    hash = (hash ^ unsigned(CratesPending)) * 16777619u;
    hash = (hash ^ unsigned(NextCrateAt)) * 16777619u;
    hash = Gun_Ability_Hash(hash);
    return (hash ^ unsigned(Winner)) * 16777619u;
}

bool Gun_Game_In_Match(HousesType owner)
{
    return Gun_Game_Active() && owner != HOUSE_NONE && In_Match(owner);
}

bool Gun_Game_Over()
{
    return Winner != HOUSE_NONE;
}

// Real-time durations follow DodgeRate, the game ticks in two real seconds, so an
// effect lasts the same number of seconds at every game speed.
int Gun_Game_Tenths(int tenths)
{
    return MAX(1, tenths * DodgeRate / 20);
}

int Gun_Game_Ticks_Per_Second()
{
    return MAX(1, (DodgeRate + 1) / 2);
}

unsigned Gun_Game_Life(HousesType owner)
{
    return Gun_Game_In_Match(owner) ? Progress[owner].Life : 0;
}

int Gun_Game_Tier(HousesType owner)
{
    return Gun_Game_In_Match(owner) ? Progress[owner].Tier : 0;
}

unsigned Gun_Game_Kills(HousesType owner)
{
    return Gun_Game_In_Match(owner) ? Progress[owner].Kills : 0;
}

// Humiliation: the player's combatant is replaced in place by the previous rank's unit.
bool Gun_Game_Demote(HousesType owner)
{
    if (!Gun_Game_In_Match(owner) || Winner != HOUSE_NONE) return false;
    GunPlayer& player = Progress[owner];
    TechnoClass* current = As_Techno(player.Unit);
    if (player.Tier <= 0 || !current || current->Strength <= 0 || current->IsInLimbo) return false;
    const int tier = player.Tier - 1;
    const int choice = Ladder[tier].Choices[player.Choice] != None ? player.Choice : 0;
    TechnoClass* next = Spawn(HouseClass::As_Pointer(owner), tier, choice, Coord_Cell(current->Coord), 8);
    if (!next) return false;
    next->PrimaryFacing.Set(current->PrimaryFacing.Current());
    player.Unit = next->As_Target();
    player.Tier = tier;
    player.Choice = choice;
    ++player.Life;
    player.DodgeEnd = 0;
    player.JumpSpeed = 0;
    player.ScudReady = 0;
    player.ChronoReady = 0;
    player.LastAttackerLife = 0;
    new AnimClass(ANIM_PIFF, current->Coord);
    delete current;
    return true;
}

// A copy of the player's current unit beside the given cell, never the combatant.
TechnoClass* Gun_Game_Spawn_Copy(HousesType owner, CELL center)
{
    if (!Gun_Game_In_Match(owner)) return nullptr;
    const GunPlayer& player = Progress[owner];
    return Spawn(HouseClass::As_Pointer(owner), player.Tier, player.Choice, center, 3);
}

// Ability damage counts like a superweapon strike: it scores for the owner but cannot
// win the match from the last rank.
void Gun_Game_Strike_Hit(TechnoClass* victim, int damage, int distance, HousesType owner, WarheadType warhead)
{
    if (!victim || damage <= 0) return;
    TechnoClass* source = owner == HOUSE_NONE ? nullptr : Gun_Game_Combatant(owner);
    if (source == victim) source = nullptr;
    const bool strike = StrikeDamage;
    StrikeDamage = true;
    victim->Take_Damage(damage, distance, warhead, source);
    StrikeDamage = strike;
}
