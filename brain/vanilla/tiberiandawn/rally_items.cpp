// OpenCNC3D Tiberian Dawn Rally: crates, items and the hazards they leave.
//
// A racer that drives into a course crate gets a one second roulette and then an item,
// weighted by its place the way kart racers do it: the leader gets defensive items,
// the back of the pack the strong ones. One item is held at a time. Mines, rockets,
// homing missiles and the ion cannon's warning mark are simulated here as hazards;
// their explosions are ordinary engine animations at the spot, so they are drawn and
// heard like any other, and their damage goes through the engine's Take_Damage.
#include "function.h"
#include "rally_internal.h"

namespace rally {

namespace {

struct Weight
{
    int Item;
    int Chance;
};

const Weight Leader[] = {
    {RALLY_ITEM_NITRO, 35}, {RALLY_ITEM_MINE, 30}, {RALLY_ITEM_ROCKET, 25}, {RALLY_ITEM_REPAIR, 10}};
const Weight Middle[] = {{RALLY_ITEM_NITRO, 15},  {RALLY_ITEM_NITRO3, 10}, {RALLY_ITEM_MINE, 15},
                         {RALLY_ITEM_ROCKET, 15}, {RALLY_ITEM_HOMING, 15}, {RALLY_ITEM_ION, 3},
                         {RALLY_ITEM_ARMOUR, 10}, {RALLY_ITEM_CLOAK, 10},  {RALLY_ITEM_REPAIR, 7}};
const Weight Back[] = {{RALLY_ITEM_NITRO3, 25}, {RALLY_ITEM_HOMING, 25}, {RALLY_ITEM_ION, 10},
                       {RALLY_ITEM_ARMOUR, 20}, {RALLY_ITEM_CLOAK, 20}};

template <int N> int Draw(const Weight (&table)[N])
{
    int total = 0;
    for (int i = 0; i < N; ++i) total += table[i].Chance;
    int roll = Random_Pick(0, total - 1);
    for (int i = 0; i < N; ++i) {
        if (roll < table[i].Chance) return table[i].Item;
        roll -= table[i].Chance;
    }
    return table[0].Item;
}

int Pick_Item(const Racer& r)
{
    const int n = M.RacerCount;
    if (r.Place <= 1) return Draw(Leader);
    if (r.Place == n || r.Place > n - n / 3) return Draw(Back);
    return Draw(Middle);
}

int32_t Radius_Of(const Racer& r)
{
    return (int32_t)((int64_t)Kart_Of(r).Radius * FINE / 100);
}

int64_t Distance2(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    const int64_t dx = (int64_t)ax - bx, dy = (int64_t)ay - by;
    return dx * dx + dy * dy;
}

COORDINATE Coord_Of(int32_t x, int32_t y)
{
    return XY_Coord(x >> 8, y >> 8);
}

// Rockets fly over water and units; walls, buildings, trees and the map edge stop them.
bool Hazard_Blocked(int32_t x, int32_t y)
{
    if (x < 0 || y < 0) return true;
    const int cx = x >> 16, cy = y >> 16;
    if (cx < Map.MapCellX || cy < Map.MapCellY || cx >= Map.MapCellX + Map.MapCellWidth
        || cy >= Map.MapCellY + Map.MapCellHeight) {
        return true;
    }
    const CellClass& c = Map[XY_Cell(cx, cy)];
    if (c.Overlay != OVERLAY_NONE && OverlayTypeClass::As_Reference(c.Overlay).IsWall) return true;
    if (c.Land_Type() == LAND_ROCK || c.Land_Type() == LAND_WALL) return true;
    return c.Cell_Building() != nullptr || c.Cell_Terrain() != nullptr;
}

Hazard* Add_Hazard(int kind, int owner, int32_t x, int32_t y, int32_t yaw)
{
    if (M.HazardCount >= RALLY_MAX_HAZARDS) {
        // Full: the oldest mine makes room; anything else is simply not made.
        int oldest = -1;
        for (int i = 0; i < M.HazardCount; ++i) {
            if (M.Hazards[i].Kind == RALLY_HAZ_MINE) {
                oldest = i;
                break;
            }
        }
        if (oldest < 0) return nullptr;
        for (int i = oldest; i + 1 < M.HazardCount; ++i) M.Hazards[i] = M.Hazards[i + 1];
        --M.HazardCount;
    }
    Hazard& h = M.Hazards[M.HazardCount++];
    memset(&h, 0, sizeof h);
    h.Kind = kind;
    h.Owner = owner;
    h.X = x;
    h.Y = y;
    h.Yaw = yaw & 0xFFFFFF;
    h.Target = -1;
    ++M.Stats[STAT_HAZARDS];
    return &h;
}

// Damage and a throw for every racer within reach of a blast; the direct victim, if
// any, takes the full damage and the rest half.
void Blast(int32_t x, int32_t y, AnimType anim, int owner, int victim, int damage, int reach, int throw_scale)
{
    new AnimClass(anim, Coord_Of(x, y));
    ++M.Stats[STAT_BLASTS];
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        if (!Racer_Alive(r) || (r.Flags & RALLY_F_FINISHED)) continue;
        const int64_t limit = (int64_t)reach + Radius_Of(r);
        const bool direct = i == victim;
        if (!direct && Distance2(x, y, r.X, r.Y) > limit * limit) continue;
        const int dealt = direct ? damage : damage / 2;
        const bool immune = r.Shield > 0 || r.Armour > 0;
        Damage_Racer(r, dealt, owner);
        if (!immune && Racer_Alive(r)) {
            Throw_Racer(r, dealt, throw_scale);
            r.Spin = Ticks(8);
            r.SpinRate = Turn_Rate(Random_Pick(0, 1) ? 720 : -720);
        }
    }
}

// The hostile building a rocket ran into, if any, takes the rocket's damage.
void Hit_Building(int32_t x, int32_t y, int owner, int damage)
{
    if (x < 0 || y < 0 || (x >> 16) >= MAP_CELL_W || (y >> 16) >= MAP_CELL_H) return;
    BuildingClass* building = Map[XY_Cell(x >> 16, y >> 16)].Cell_Building();
    if (!building || !building->IsActive || building->Strength <= 0) return;
    TechnoClass* source = Racer_Index_Valid(owner) ? (TechnoClass*)Racer_Unit(M.Racers[owner]) : nullptr;
    int dealt = damage;
    building->Take_Damage(dealt, 0, WARHEAD_HE, source);
}

// Nothing that stops a rocket on the straight line between two points, sampled every
// quarter cell.
bool Hazard_Line_Clear(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    const int64_t dx = (int64_t)x1 - x0, dy = (int64_t)y1 - y0;
    const int64_t len = Isqrt((uint64_t)(dx * dx + dy * dy));
    const int steps = (int)(len / (FINE / 4)) + 1;
    for (int i = 1; i < steps; ++i) {
        if (Hazard_Blocked((int32_t)(x0 + dx * i / steps), (int32_t)(y0 + dy * i / steps))) return false;
    }
    return true;
}

// A racer a homing missile or the ion cannon may pick: on the course, not finished,
// still in the match; a homing missile also cannot see a cloaked one.
bool Item_Can_Target(const Racer& t, bool see_cloak)
{
    return Racer_Alive(t) && !(t.Flags & (RALLY_F_FINISHED | RALLY_F_RETIRED)) && (see_cloak || t.Cloak == 0);
}

int Racer_Hit(const Hazard& h, int32_t x, int32_t y, int self_grace)
{
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& r = M.Racers[i];
        if (!Racer_Alive(r) || (r.Flags & RALLY_F_FINISHED) || r.Z > FINE / 2) continue;
        if (i == h.Owner && h.Ticks < self_grace) continue;
        const int64_t reach = Radius_Of(r) + FINE / 5;
        if (Distance2(x, y, r.X, r.Y) < reach * reach) return i;
    }
    return -1;
}

bool Update_Mine(Hazard& h)
{
    ++h.Ticks;
    if (h.Ticks < Ticks(5)) return false; // arming
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& r = M.Racers[i];
        if (!Racer_Alive(r) || (r.Flags & RALLY_F_FINISHED) || r.Z > FINE / 4) continue;
        if (i == h.Owner && h.Ticks < Ticks(10)) continue;
        const int64_t reach = (int64_t)FINE * 45 / 100 + Radius_Of(r) / 2;
        if (Distance2(h.X, h.Y, r.X, r.Y) < reach * reach) {
            Blast(h.X, h.Y, ANIM_ART_EXP1, h.Owner, i, 60, FINE * 3 / 4, 100);
            return true;
        }
    }
    return h.Ticks > Ticks(900);
}

bool Update_Missile(Hazard& h)
{
    const bool homing = h.Kind == RALLY_HAZ_HOMING;
    if (homing && Racer_Index_Valid(h.Target) && !Item_Can_Target(M.Racers[h.Target], false)) h.Target = -1;
    const int32_t step = Velocity(homing ? 1200 : 1400) / (256 * SUBSTEPS);
    const int32_t turn = Turn_Rate(homing ? 450 : 300);
    for (int s = 0; s < SUBSTEPS; ++s) {
        if (homing && Racer_Index_Valid(h.Target)) {
            const Racer& t = M.Racers[h.Target];
            // Far from its target, or with something in the way, a homing missile flies
            // along the course toward it, the way the racers drive, rather than straight
            // into the course's walls; close and in sight, it homes.
            int32_t gx = t.X, gy = t.Y;
            const int64_t close_range = (int64_t)FINE * 4;
            if (Distance2(h.X, h.Y, t.X, t.Y) > close_range * close_range || !Hazard_Line_Clear(h.X, h.Y, t.X, t.Y)) {
                int64_t along = 0, target_along = 0;
                h.Segment = Course_Segment_Near(h.X, h.Y, h.Segment, &along, nullptr);
                Course_Segment_Near(t.X, t.Y, t.Segment, &target_along, nullptr);
                const int64_t look = (int64_t)FINE * 3 / 2;
                // The short way round a circuit.
                Course_Point_At(Course_Delta(target_along, along) >= 0 ? along + look : along - look, &gx, &gy,
                                nullptr);
            }
            const int32_t want = Angle_Of((int64_t)gx - h.X, (int64_t)gy - h.Y);
            int32_t diff = Angle_Diff(want, (h.Yaw >> 8) & 0xFFFF) * 256;
            if (diff > turn) diff = turn;
            if (diff < -turn) diff = -turn;
            h.Yaw = (h.Yaw + diff) & 0xFFFFFF;
        }
        const int32_t yaw16 = (h.Yaw >> 8) & 0xFFFF;
        const int32_t nx = h.X + (int32_t)(((int64_t)Sin16(yaw16) * step) >> 16);
        const int32_t ny = h.Y - (int32_t)(((int64_t)Cos16(yaw16) * step) >> 16);
        if (Hazard_Blocked(nx, ny)) {
            if (h.Bounces > 0) {
                // Bounce off the face it ran into.
                const bool bx = Hazard_Blocked(nx, h.Y);
                const bool by = Hazard_Blocked(h.X, ny);
                if (bx && !by) h.Yaw = (-(h.Yaw)) & 0xFFFFFF;
                else if (by && !bx) h.Yaw = ((32768 << 8) - h.Yaw) & 0xFFFFFF;
                else h.Yaw = (h.Yaw + (32768 << 8)) & 0xFFFFFF;
                --h.Bounces;
                continue;
            }
            Hit_Building(nx, ny, h.Owner, homing ? 90 : 80);
            Blast(h.X, h.Y, ANIM_FBALL1, h.Owner, -1, homing ? 90 : 80, FINE / 2, 100);
            return true;
        }
        h.X = nx;
        h.Y = ny;
        const int victim = Racer_Hit(h, h.X, h.Y, Ticks(3));
        if (victim >= 0) {
            Blast(h.X, h.Y, ANIM_FBALL1, h.Owner, victim, homing ? 90 : 80, FINE / 2, 100);
            return true;
        }
    }
    ++h.Ticks;
    if (h.Ticks > Ticks(80)) {
        Blast(h.X, h.Y, ANIM_FBALL1, h.Owner, -1, homing ? 90 : 80, FINE / 2, 100);
        return true;
    }
    return false;
}

bool Update_Ion(Hazard& h)
{
    if (Racer_Index_Valid(h.Target)) {
        const Racer& t = M.Racers[h.Target];
        if (Racer_Alive(t) && !(t.Flags & RALLY_F_FINISHED)) {
            h.X = t.X;
            h.Y = t.Y;
        } else {
            h.Target = -1; // stays where its target fell
        }
    }
    if (--h.Ticks > 0) return false;
    // The beam: the engine's own ion animation, whose engine damage Rally turns off.
    new AnimClass(ANIM_ION_CANNON, Coord_Of(h.X, h.Y));
    ++M.Stats[STAT_BLASTS];
    const int64_t reach = (int64_t)FINE * 5 / 4;
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        if (!Racer_Alive(r) || (r.Flags & RALLY_F_FINISHED)) continue;
        if (Distance2(h.X, h.Y, r.X, r.Y) > reach * reach) continue;
        const bool immune = r.Shield > 0 || r.Armour > 0;
        Damage_Racer(r, 150, h.Owner);
        if (!immune && Racer_Alive(r)) Throw_Racer(r, 150, 140);
    }
    return true;
}

} // namespace

// Whom a homing missile (the racer one place ahead, or the nearest one behind for the
// leader) or the ion cannon (the best placed racer) of racer r would go for, or -1.
// Racers that have finished or left are skipped: an item spent on them is wasted.
int Item_Target(const Racer& r, int item)
{
    const int self = (int)(&r - M.Racers);
    if (item == RALLY_ITEM_ION) {
        for (int place = 1; place <= M.RacerCount; ++place) {
            const int i = Racer_By_Place(place);
            if (i >= 0 && i != self && Item_Can_Target(M.Racers[i], true)) return i;
        }
        return -1;
    }
    for (int place = r.Place - 1; place >= 1; --place) {
        const int i = Racer_By_Place(place);
        if (i >= 0 && i != self && Item_Can_Target(M.Racers[i], false)) return i;
    }
    for (int place = r.Place + 1; place <= M.RacerCount; ++place) {
        const int i = Racer_By_Place(place);
        if (i >= 0 && i != self && Item_Can_Target(M.Racers[i], false)) return i;
    }
    return -1;
}

bool Collect_Crate(Racer& r, CELL cell)
{
    Crate* crate = nullptr;
    for (int i = 0; i < M.CrateCount; ++i) {
        if (M.Crates[i].Active && M.Crates[i].Cell == cell) {
            crate = &M.Crates[i];
            break;
        }
    }
    if (!crate) return false;
    if (!Racer_Alive(r) || r.Item != RALLY_ITEM_NONE || r.Roulette > 0 || (r.Flags & RALLY_F_FINISHED)) return false;
    crate->Active = false;
    crate->Return = Ticks(50);
    CellClass& c = Map[cell];
    c.Redraw_Objects();
    c.Overlay = OVERLAY_NONE;
    c.OverlayData = 0;
    r.Roulette = Ticks(10);
    r.Flags |= RALLY_F_ROULETTE;
    ++M.Stats[STAT_CRATES];
    return true;
}

// A nitro's speed burst. The prediction of the local racer fires it the same way.
void Nitro_Burst(Racer& r)
{
    r.Boost = Ticks(12);
    r.BoostTop = 160;
    r.BoostAccel = 250;
}

void Use_Item(Racer& r)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit || (r.Flags & RALLY_F_FINISHED)) return;
    const int self = (int)(&r - M.Racers);
    const int32_t yaw16 = (r.Yaw >> 8) & 0xFFFF;
    const int64_t fx = Sin16(yaw16), fy = -Cos16(yaw16);
    const int32_t radius = Radius_Of(r);
    switch (r.Item) {
    case RALLY_ITEM_NITRO:
    case RALLY_ITEM_NITRO3:
        Nitro_Burst(r);
        break;
    case RALLY_ITEM_MINE: {
        const int64_t back = radius + FINE * 45 / 100;
        int32_t x = (int32_t)(r.X - ((fx * back) >> 16)), y = (int32_t)(r.Y - ((fy * back) >> 16));
        if (Hazard_Blocked(x, y)) {
            x = r.X;
            y = r.Y;
        }
        Add_Hazard(RALLY_HAZ_MINE, self, x, y, r.Yaw);
        break;
    }
    case RALLY_ITEM_ROCKET:
    case RALLY_ITEM_HOMING: {
        const int64_t ahead = radius + FINE * 3 / 10;
        int32_t x = (int32_t)(r.X + ((fx * ahead) >> 16)), y = (int32_t)(r.Y + ((fy * ahead) >> 16));
        if (Hazard_Blocked(x, y)) {
            x = r.X;
            y = r.Y;
        }
        const bool homing = r.Item == RALLY_ITEM_HOMING;
        const int target = homing ? Item_Target(r, RALLY_ITEM_HOMING) : -1;
        if (homing && target < 0) return; // nobody to seek: keep the item
        Hazard* h = Add_Hazard(homing ? RALLY_HAZ_HOMING : RALLY_HAZ_ROCKET, self, x, y, r.Yaw);
        if (h) {
            h->Z = FINE / 4;
            h->Bounces = homing ? 1 : 2;
            h->Target = target;
            h->Segment = r.Segment;
        }
        break;
    }
    case RALLY_ITEM_ION: {
        const int target = Item_Target(r, RALLY_ITEM_ION);
        if (target < 0) return; // nobody to strike: keep the item
        const Racer& t = M.Racers[target];
        Hazard* h = Add_Hazard(RALLY_HAZ_ION_MARK, self, t.X, t.Y, 0);
        if (!h) return;
        h->Target = target;
        h->Ticks = Ticks(15);
        break;
    }
    case RALLY_ITEM_ARMOUR:
        r.Armour = Ticks(60);
        break;
    case RALLY_ITEM_CLOAK:
        r.Cloak = Ticks(60);
        break;
    case RALLY_ITEM_REPAIR:
        unit->Strength = unit->Class->MaxStrength;
        unit->Mark(MARK_CHANGE);
        break;
    default:
        return;
    }
    ++M.Stats[STAT_ITEMS];
    if (--r.ItemCount <= 0) {
        r.Item = RALLY_ITEM_NONE;
        r.ItemCount = 0;
    }
}

void Items_Tick()
{
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        if (r.Roulette > 0 && --r.Roulette == 0) {
            r.Item = Pick_Item(r);
            r.ItemCount = r.Item == RALLY_ITEM_NITRO3 ? 3 : 1;
        }
    }
    for (int i = 0; i < M.CrateCount; ++i) {
        Crate& c = M.Crates[i];
        if (c.Active || c.Return <= 0) continue;
        if (--c.Return > 0) continue;
        // Not back under a vehicle that is standing on the spot.
        bool covered = false;
        for (int j = 0; j < M.RacerCount; ++j) {
            const Racer& r = M.Racers[j];
            if (Racer_Alive(r) && XY_Cell(r.X >> 16, r.Y >> 16) == c.Cell) covered = true;
        }
        if (covered) {
            c.Return = 1;
        } else {
            Place_Crate(c);
        }
    }
    int i = 0;
    while (i < M.HazardCount) {
        Hazard& h = M.Hazards[i];
        bool gone = false;
        switch (h.Kind) {
        case RALLY_HAZ_MINE: gone = Update_Mine(h); break;
        case RALLY_HAZ_ROCKET:
        case RALLY_HAZ_HOMING: gone = Update_Missile(h); break;
        case RALLY_HAZ_ION_MARK: gone = Update_Ion(h); break;
        default: gone = true; break;
        }
        if (gone) {
            for (int j = i; j + 1 < M.HazardCount; ++j) M.Hazards[j] = M.Hazards[j + 1];
            --M.HazardCount;
        } else {
            ++i;
        }
    }
}

} // namespace rally
