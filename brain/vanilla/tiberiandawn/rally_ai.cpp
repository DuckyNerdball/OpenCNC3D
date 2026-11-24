// OpenCNC3D Tiberian Dawn Rally: the AI drivers.
//
// A computer seat (and a human seat on autopilot) follows the course centre line with
// a look-ahead that grows with speed, slows for the bends it can see coming, steps
// aside for a racer just ahead, backs out when it is stuck and asks for a rescue when
// that does not help. It uses its item when the item makes sense and shoots the
// nearest racer in range, now and then a defence instead. Skill comes from the seat's
// AI difficulty: a harder driver keeps a tighter line and goes faster.
//
// Bends that turn far enough are drifted for the mini-turbo, the way a player does it:
// the driver holds its line into the bend, starts the drift into it, steers into or out
// of the drift to stay on its line and off the walls, and lets go as the bend
// straightens. The best drivers drift every bend that warrants it and let go on time;
// weaker ones drift fewer, turn in early and let go late.
//
// It only chooses controls (the same bits a player sends); the physics are shared.
//
// Round a circuit the look-ahead runs on past the line into the next lap: the course's
// segment numbers wrap round there, so every scan counts its segments forward from where
// the driver is rather than up to a larger number.
#include "function.h"
#include "rally_internal.h"

namespace rally {

namespace {

int Cps100(int64_t v)
{
    return (int)(v * 10 * M.TicksPerSecondX10 / 16777216);
}

int32_t Radius_Of(const Racer& r)
{
    return (int32_t)((int64_t)Kart_Of(r).Radius * FINE / 100);
}

int32_t Abs32(int32_t v)
{
    return v < 0 ? -v : v;
}

// How much the course turns, in 16-bit angle units, between two distances along it.
int32_t Course_Turn(int64_t from, int64_t to)
{
    int32_t x, y;
    int s0 = 0, s1 = 0;
    Course_Point_At(from, &x, &y, &s0);
    Course_Point_At(to, &x, &y, &s1);
    int32_t most = 0;
    const int32_t first = Course_Direction(s0);
    const int count = Course_Segments_Between(s0, s1);
    for (int k = 1; k <= count; ++k) {
        const int32_t turn = Abs32(Angle_Diff(Course_Direction(s0 + k), first));
        if (turn > most) most = turn;
    }
    return most;
}

bool Has_Room(const Racer& r, int32_t x, int32_t y)
{
    return !Point_Blocked(x, y, &r);
}

int Segment_At(int64_t along)
{
    int32_t x, y;
    int s = 0;
    Course_Point_At(along, &x, &y, &s);
    return s;
}

// The signed turn of the course from segment a to segment b, summed segment by segment
// so that a hairpin reads as the whole half turn instead of wrapping round.
int32_t Course_Turn_Signed(int a, int b)
{
    int32_t sum = 0;
    const int count = Course_Segments_Between(a, b);
    for (int k = 0; k < count; ++k) sum += Angle_Diff(Course_Direction(a + k + 1), Course_Direction(a + k));
    return sum;
}

// How far the hull still has to turn, signed, to point the way the course runs a given
// distance ahead.
int32_t Turn_Needed(int seg, int64_t along, int64_t ahead, int32_t yaw16)
{
    return Angle_Diff(Course_Direction(seg), yaw16) + Course_Turn_Signed(seg, Segment_At(along + ahead));
}

int32_t Degrees(int degrees)
{
    return (int32_t)((int64_t)degrees * 65536 / 360);
}

// Drift handling by AI difficulty 0..3.
const int DRIFT_CHANCE[4] = {35, 60, 85, 100}; // percent of the bends worth a drift that get one
const int DRIFT_TURN_IN[4] = {70, 80, 90, 100}; // turn-in point, percent of the turn a drift needs
const int DRIFT_DEAD[4] = {12, 8, 5, 3};        // degrees off the line before steering into or out of it
const int DRIFT_LATE[4] = {4, 2, 1, 0};         // most ticks a charged drift is held too long
const int DRIFT_PACE = 95;                      // percent of top speed kept into a bend to be drifted
const int DRIFT_WIDE = 50;                      // percent of the room to the side a drift's line keeps outside

} // namespace

void AI_Drive(Racer& r)
{
    if (!Racer_Alive(r) || M.Phase != RALLY_PHASE_RACE) {
        r.Bits = 0;
        r.AiDrift = r.AiDriftHold = r.AiDriftLate = 0;
        return;
    }
    if ((r.Flags & (RALLY_F_FLIPPED | RALLY_F_RIGHTING)) || r.Z > 0 || r.Spin > 0) {
        r.Bits = 0;
        r.AiSlow = 0;
        r.AiDrift = r.AiDriftHold = r.AiDriftLate = 0;
        return;
    }
    const KartStats& k = Kart_Of(r);
    const int64_t speed = r.Speed < 0 ? -(int64_t)r.Speed : r.Speed;
    const int64_t top = Velocity(k.Top);
    const int cps = Cps100(speed);

    int64_t along = 0;
    int32_t off_line = 0;
    const int seg = Course_Segment_Near(r.X, r.Y, r.Segment, &along, &off_line);

    // Aim at a point ahead on the centre line, farther the faster it goes.
    const int64_t look = (int64_t)FINE * 12 / 10 + (int64_t)cps * FINE * 45 / 10000;
    int32_t tx, ty;
    int tseg = seg;
    Course_Point_At(along + look, &tx, &ty, &tseg);

    // A less skilled driver wanders off the ideal line.
    if (--r.AiOffsetTimer <= 0) {
        r.AiOffsetTimer = Ticks(15 + Random_Pick(0, 10));
        const int spread = (3 - (r.Skill & 3)) * 10 + 10;
        r.AiOffset = (int32_t)((int64_t)M.Point[tseg].HalfWidth * Random_Pick(-spread, spread) / 100);
    }
    const int32_t dir = Course_Direction(tseg);
    const int64_t rx = Cos16(dir), ry = Sin16(dir); // right of the course
    int64_t lateral = r.AiOffset;

    // Step aside for a racer just ahead. One that is slower is in the way of a drift.
    const int32_t yaw16 = (r.Yaw >> 8) & 0xFFFF;
    bool crowded = false;
    for (int i = 0; i < M.RacerCount; ++i) {
        const Racer& o = M.Racers[i];
        if (&o == &r || !Racer_Alive(o) || (o.Flags & RALLY_F_FINISHED)) continue;
        const int64_t dx = (int64_t)o.X - r.X, dy = (int64_t)o.Y - r.Y;
        const int64_t d2 = dx * dx + dy * dy;
        if (d2 > (int64_t)FINE * FINE * 5) continue; // about 2.2 cells
        const int32_t bearing = Angle_Diff(Angle_Of(dx, dy), yaw16);
        if (Abs32(bearing) > 6400) continue; // about 35 degrees either side of the nose
        if (o.Speed < r.Speed) crowded = true;
        // Pass on the side away from it.
        const int64_t side = (dx * rx + dy * ry) >> 16;
        lateral += side > 0 ? -(int64_t)FINE * 9 / 10 : (int64_t)FINE * 9 / 10;
    }
    const int64_t hw = M.Point[tseg].HalfWidth;
    const int64_t limit = hw - Radius_Of(r) - FINE / 4;
    if (limit > 0) {
        if (lateral > limit) lateral = limit;
        if (lateral < -limit) lateral = -limit;
    } else {
        lateral = 0;
    }
    int32_t ax = (int32_t)(tx + ((rx * lateral) >> 16)), ay = (int32_t)(ty + ((ry * lateral) >> 16));
    if (!Has_Room(r, ax, ay)) {
        ax = tx;
        ay = ty;
    }

    const int32_t desired = Angle_Of((int64_t)ax - r.X, (int64_t)ay - r.Y);
    const int32_t error = Angle_Diff(desired, yaw16);
    uint32_t bits = 0;
    const int32_t dead = 546; // three degrees
    if (error > dead) bits |= RALLY_IN_RIGHT;
    if (error < -dead) bits |= RALLY_IN_LEFT;

    // The least a drift turns before it has charged its mini-turbo: the physics charges it in
    // half a second, and a drift turns at no less than 0.6 of the turn rate. A bend that
    // turns about that much or more is worth a drift.
    const int skill = r.Skill & 3;
    const int32_t drift_min = (int32_t)(((int64_t)Turn_Rate(k.Turn) * SUBSTEPS * Ticks(5) * 60 / 100) >> 8);
    if (r.AiDrift == 0) {
        // The largest turn the course makes within about a second of driving.
        const int64_t scan = (int64_t)FINE * 2 + (int64_t)cps * FINE / 100;
        const int count = Course_Segments_Between(seg, Segment_At(along + scan));
        int32_t sum = 0, most = 0, least = 0;
        for (int k = 0; k < count; ++k) {
            const int s = seg + k;
            sum += Angle_Diff(Course_Direction(s + 1), Course_Direction(s));
            if (sum > most) most = sum;
            if (sum < least) least = sum;
        }
        const int way = most >= -least ? 1 : -1;
        const int32_t bend = way > 0 ? most : -least;
        if (bend < drift_min * 9 / 10) {
            r.AiDriftPlan = 0;
        } else if (r.AiDriftPlan == 0 || (r.AiDriftPlan > 0) != (way > 0)) {
            // A new bend: whether to drift it is decided once.
            r.AiDriftPlan = way * (Percent_Chance(DRIFT_CHANCE[skill]) ? 1 : 2);
        }
    }
    const bool planned = r.AiDriftPlan == 1 || r.AiDriftPlan == -1;

    // Slow down for what the course does next.
    const int64_t brake_look = (int64_t)FINE * 2 + (int64_t)cps * FINE * 50 / 10000;
    const int32_t corner = Course_Turn(along, along + brake_look);
    const int corner_deg = (int)((int64_t)corner * 360 / 65536);
    int safe = 100 - corner_deg * 55 / 90;
    if (safe < 35) safe = 35;
    const int error_deg = (int)((int64_t)Abs32(error) * 360 / 65536);
    if (error_deg > 50 && safe > 40 && r.AiDrift == 0) safe = 40;
    // A bend to be drifted is taken at pace: the drift turns the hull, not the brakes. Not
    // with a slower racer just ahead, which would be rammed, and not before the hull points
    // along the course and at its line: a car turning back round (spun by a blast, or
    // facing the wrong way) keeps the slow turn-around speed above, or its wide arc ends in
    // the outside wall.
    const bool on_line = Abs32(Angle_Diff(Course_Direction(seg), yaw16)) < Degrees(45) && error_deg <= 50;
    if (((planned && on_line) || r.AiDrift > 0) && !crowded && safe < DRIFT_PACE) safe = DRIFT_PACE;
    const int64_t safe_speed = top * safe / 100;
    if (r.Speed > safe_speed + top * 15 / 100) {
        bits |= RALLY_IN_BRAKE;
    } else if (r.Speed < safe_speed) {
        bits |= RALLY_IN_THROTTLE;
    }

    const uint32_t steer_bits = RALLY_IN_LEFT | RALLY_IN_RIGHT;
    if (r.AiDrift > 0 && r.DriftDir == 0) {
        // The drift was lost (too slow, thrown, against a wall): this bend is driven through.
        r.AiDrift = 0;
        r.AiDriftLate = 0;
        r.AiDriftPlan = r.AiDriftPlan > 0 ? 2 : r.AiDriftPlan < 0 ? -2 : 0;
    }
    if (r.AiDrift > 0) {
        ++r.AiDrift;
        const int way = r.DriftDir;
        const uint32_t into = way > 0 ? RALLY_IN_RIGHT : RALLY_IN_LEFT;
        const uint32_t out = way > 0 ? RALLY_IN_LEFT : RALLY_IN_RIGHT;
        // A drift aims at the outside half of the course ahead rather than its middle: the
        // drift turns tighter than any bend, and aimed at the middle it clips the inside.
        const int64_t wide = limit > 0 ? -(int64_t)way * limit * DRIFT_WIDE / 100 : 0;
        int32_t wx = (int32_t)(tx + ((rx * wide) >> 16)), wy = (int32_t)(ty + ((ry * wide) >> 16));
        if (!Has_Room(r, wx, wy)) {
            wx = tx;
            wy = ty;
        }
        // How much more that line wants the hull to turn the drift's way.
        const int32_t more = Angle_Diff(Angle_Of((int64_t)wx - r.X, (int64_t)wy - r.Y), yaw16) * way;
        const int32_t dz = Degrees(DRIFT_DEAD[skill]);
        const bool charged = (r.Flags & RALLY_F_TURBO_READY) != 0;
        const int32_t rest = way * Turn_Needed(seg, along, (int64_t)FINE * 3 / 2 + (int64_t)cps * FINE / 400, yaw16);
        bool let_go = false;
        if (charged && (rest < Degrees(15) || more < -Degrees(25))) {
            // The bend is done, or the hull already points past the line: let go, which fires
            // the mini-turbo. A weaker driver holds on a little too long.
            if (r.AiDriftLate == 0) r.AiDriftLate = 1 + Random_Pick(0, DRIFT_LATE[skill]);
            if (--r.AiDriftLate <= 0) let_go = true;
        }
        // Turning far past the line before the charge is in, or drifting for far longer than
        // any bend takes: give the drift up.
        if ((!charged && more < -Degrees(35)) || r.AiDrift > Ticks(25)) let_go = true;
        if (let_go) {
            bits &= ~RALLY_IN_DRIFT;
            r.AiDrift = 0;
            r.AiDriftLate = 0;
            r.AiDriftPlan = 2 * way;
        } else {
            bits = (bits & ~(steer_bits | RALLY_IN_BRAKE)) | RALLY_IN_DRIFT | RALLY_IN_THROTTLE;
            if (more < -dz) bits |= out;
            else if (more > dz) bits |= into;
        }
    } else if (planned) {
        const int way = r.AiDriftPlan;
        const uint32_t into = way > 0 ? RALLY_IN_RIGHT : RALLY_IN_LEFT;
        // The turn needed within the distance the drift will cover, about three quarters
        // of a second, and whether the hull is still roughly along the course.
        const int32_t need = way * Turn_Needed(seg, along, (int64_t)cps * FINE * 75 / 10000, yaw16);
        const bool aligned = Abs32(Angle_Diff(Course_Direction(seg), yaw16)) < Degrees(45);
        const bool fast = r.Speed > top * 55 / 100 && r.Boost <= Ticks(4) && !crowded;
        bool start = fast && aligned && error * way > 0 && need >= drift_min * DRIFT_TURN_IN[skill] / 100;
        if (!start && fast && aligned && error * way > dead) {
            // Hold the line into the bend, so the drift can turn in late and sharp, until a
            // wall comes up ahead or the hold has gone on too long.
            ++r.AiDriftHold;
            bits &= ~into;
            const int64_t reach = Radius_Of(r) + (int64_t)FINE * 3 / 2;
            const int32_t hx = (int32_t)(r.X + ((Sin16(yaw16) * reach) >> 16));
            const int32_t hy = (int32_t)(r.Y - ((Cos16(yaw16) * reach) >> 16));
            if (!Has_Room(r, hx, hy) || r.AiDriftHold > Ticks(15)) {
                if (need >= drift_min * 6 / 10) {
                    start = true;
                } else {
                    r.AiDriftPlan = 2 * way;
                    r.AiDriftHold = 0;
                    bits |= into;
                }
            }
        } else if (!start) {
            r.AiDriftHold = 0;
        }
        if (start) {
            // Into the bend: the physics fixes the drift's way from this steer.
            bits = (bits & ~(steer_bits | RALLY_IN_BRAKE)) | RALLY_IN_DRIFT | RALLY_IN_THROTTLE | into;
            r.AiDrift = 1;
            r.AiDriftHold = 0;
            r.AiDriftLate = 0;
        }
    } else {
        r.AiDriftHold = 0;
    }

    // Stuck against something: back out with the wheels turned the other way.
    if (speed < top * 12 / 100) {
        ++r.AiSlow;
        ++r.AiStuck;
    } else {
        r.AiSlow = 0;
        r.AiStuck = r.AiStuck > 2 ? r.AiStuck - 2 : 0;
    }
    if (r.AiReverse > 0) {
        --r.AiReverse;
        bits = RALLY_IN_BRAKE | (error > 0 ? RALLY_IN_LEFT : RALLY_IN_RIGHT);
        r.AiDrift = 0;
        r.AiDriftHold = 0;
    } else if (r.AiSlow > Ticks(10)) {
        r.AiReverse = Ticks(8);
        r.AiSlow = 0;
    }
    if (r.AiStuck > Ticks(40) && r.RescueCooldown == 0) {
        r.RescueLatch = true;
        r.AiStuck = 0;
        r.AiReverse = 0;
    }
    // A driver going the wrong way just follows the target point back round.
    r.Bits = bits;
}

void AI_Items(Racer& r)
{
    if (!Racer_Alive(r) || r.Item == RALLY_ITEM_NONE || r.Roulette > 0 || M.Phase != RALLY_PHASE_RACE) {
        r.AiItemTicks = 0;
        return;
    }
    ++r.AiItemTicks;
    const int32_t yaw16 = (r.Yaw >> 8) & 0xFFFF;
    const int64_t top = Velocity(Kart_Of(r).Top);
    bool use = false;
    switch (r.Item) {
    case RALLY_ITEM_NITRO:
    case RALLY_ITEM_NITRO3: {
        // Not in a drift: a boost running when it is let go would cost the mini-turbo.
        if (r.Boost > 0 || r.AiDrift > 0 || r.Speed < top * 30 / 100) break;
        int64_t along = 0;
        Course_Segment_Near(r.X, r.Y, r.Segment, &along, nullptr);
        const int32_t turn = Course_Turn(along, along + (int64_t)FINE * 5);
        const int32_t error = Angle_Diff(Course_Direction(r.Segment), yaw16);
        use = turn < 3640 && Abs32(error) < 2730; // under 20 and 15 degrees
        break;
    }
    case RALLY_ITEM_MINE:
        for (int i = 0; i < M.RacerCount && !use; ++i) {
            const Racer& o = M.Racers[i];
            if (&o == &r || !Racer_Alive(o)) continue;
            const int64_t dx = (int64_t)o.X - r.X, dy = (int64_t)o.Y - r.Y;
            if (dx * dx + dy * dy > (int64_t)FINE * FINE * 9) continue;
            if (Abs32(Angle_Diff(Angle_Of(dx, dy), yaw16)) > 16384) use = true; // behind
        }
        if (r.AiItemTicks > Ticks(80)) use = true;
        break;
    case RALLY_ITEM_ROCKET:
        for (int i = 0; i < M.RacerCount && !use; ++i) {
            const Racer& o = M.Racers[i];
            if (&o == &r || !Racer_Alive(o) || o.Cloak > 0) continue;
            const int64_t dx = (int64_t)o.X - r.X, dy = (int64_t)o.Y - r.Y;
            if (dx * dx + dy * dy > (int64_t)FINE * FINE * 64) continue;
            if (Abs32(Angle_Diff(Angle_Of(dx, dy), yaw16)) < 1820) use = true; // ten degrees
        }
        if (r.AiItemTicks > Ticks(100)) use = true;
        break;
    case RALLY_ITEM_REPAIR: {
        // Held Repair blocks every other crate, so it is not saved for long: used below
        // three quarters health, after 8 s with any damage, and after 15 s regardless.
        const UnitClass* unit = Racer_Unit(r);
        if (!unit) break;
        const int strength = unit->Strength, full = unit->Class->MaxStrength;
        use = strength * 4 < full * 3 || (strength < full && r.AiItemTicks > Ticks(80))
              || r.AiItemTicks > Ticks(150);
        break;
    }
    case RALLY_ITEM_CLOAK:
        use = (r.LastHurt > 0 && M.RaceTicks - r.LastHurt < Ticks(10)) || r.AiItemTicks > Ticks(120);
        break;
    case RALLY_ITEM_HOMING:
    case RALLY_ITEM_ION: // as soon as there is someone to send it at
        use = Item_Target(r, r.Item) >= 0;
        break;
    default: // armour: at once
        use = true;
        break;
    }
    if (use) r.ItemLatch = true;
}

void AI_Fire(Racer& r)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit || M.Phase != RALLY_PHASE_RACE) return;
    const int32_t yaw16 = (r.Yaw >> 8) & 0xFFFF;
    // No computer driver opens fire on the packed start grid: the first seconds are a race.
    if (M.RaceTicks < Ticks(30)) {
        Aim_Turret(r, r.X + (Sin16(yaw16) * 4), r.Y - (Cos16(yaw16) * 4));
        return;
    }
    if (unit->Class->Primary == WEAPON_NONE) {
        r.Turret = r.Yaw;
        return;
    }
    const int64_t range = (int64_t)Weapons[unit->Class->Primary].Range * 256;
    // A weapon fixed to the hull only reaches what is near straight ahead.
    const bool fixed = !unit->Class->IsTurretEquipped;
    const auto in_cone = [&](int64_t dx, int64_t dy) {
        return !fixed || Abs32(Angle_Diff(Angle_Of(dx, dy), yaw16)) <= FIXED_CONE;
    };

    // Keep a target for half a second; drop it the moment it is no longer fair game.
    TechnoClass* target = As_Techno(r.AiTarget);
    if (target) {
        bool keep = target->IsActive && !target->IsInLimbo && target->Strength > 0;
        if (keep && target->What_Am_I() == RTTI_UNIT) {
            for (int i = 0; i < M.RacerCount; ++i) {
                const Racer& o = M.Racers[i];
                if (o.Unit == target->As_Target() && !Racer_Fair_Game(o)) keep = false;
            }
        }
        if (keep) {
            const COORDINATE c = target->Target_Coord();
            keep = in_cone((int64_t)Coord_X(c) * 256 - r.X, (int64_t)Coord_Y(c) * 256 - r.Y);
        }
        if (!keep) {
            target = nullptr;
            r.AiTarget = TARGET_NONE;
        }
    }
    if (--r.AiTargetTimer <= 0 || !target) {
        r.AiTargetTimer = Ticks(5);
        target = nullptr;
        int64_t best = range * range;
        for (int i = 0; i < M.RacerCount; ++i) {
            Racer& o = M.Racers[i];
            if (&o == &r || !Racer_Fair_Game(o)) continue;
            const int64_t dx = (int64_t)o.X - r.X, dy = (int64_t)o.Y - r.Y;
            const int64_t d2 = dx * dx + dy * dy;
            if (d2 < best && in_cone(dx, dy)) {
                best = d2;
                target = Racer_Unit(o);
            }
        }
        if (!target && Percent_Chance(30)) {
            int64_t closest = range * range;
            for (int i = 0; i < Buildings.Count(); ++i) {
                BuildingClass* b = Buildings.Ptr(i);
                if (!b || !b->IsActive || b->IsInLimbo || b->Strength <= 0 || b->Owner() != M.Hostile) continue;
                const COORDINATE c = b->Center_Coord();
                const int64_t dx = (int64_t)Coord_X(c) * 256 - r.X, dy = (int64_t)Coord_Y(c) * 256 - r.Y;
                const int64_t d2 = dx * dx + dy * dy;
                if (d2 < closest && in_cone(dx, dy)) {
                    closest = d2;
                    target = b;
                }
            }
        }
        r.AiTarget = target ? target->As_Target() : TARGET_NONE;
    }
    if (!target) {
        Aim_Turret(r, r.X + (Sin16(yaw16) * 4), r.Y - (Cos16(yaw16) * 4));
        return;
    }
    const COORDINATE c = target->Target_Coord();
    const int32_t tx = Coord_X(c) * 256, ty = Coord_Y(c) * 256;
    Aim_Turret(r, tx, ty);
    Fire_Weapon(r, target->As_Target(), tx, ty, false);
}

} // namespace rally
