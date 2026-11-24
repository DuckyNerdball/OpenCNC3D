// OpenCNC3D Tiberian Dawn Rally: vehicle physics.
//
// Each racer is a circle that moves in fine units (1/256 lepton) with several
// substeps per engine tick. Its velocity is split into a forward and a sideways part:
// throttle and brake work on the forward part, grip removes the sideways part, and a
// drift removes much less of it. Steering turns the hull at a rate that grows with
// speed. Blocked cells (by the vehicle's locomotion, walls, buildings, trees, the map
// edge and ground units that are not racers) are squares the circle is pushed out of
// with a bounce; racers bounce off each other by mass. A racer can be thrown into the
// air, spin there, and land on its roof, after which it rights itself.
//
// Integers only. Positions in fine units, velocities in fine units per tick with 8
// fraction bits, angles as 16-bit binary angles with 8 fraction bits.
//
// One racer's driving through a tick (Drive_Begin, the substeps, Drive_End) touches
// nothing but that racer and the counters it is handed, and reads the map without
// changing it. That is what lets OpenCNC3D_Rally_Predict run the very same code on a
// copy of the local racer, ahead of the lockstep, without changing the race.
#include "function.h"
#include "rally_internal.h"

namespace rally {

namespace {

const int32_t GRAVITY = 3000;     // cells/s^2 x100
const int DRIFT_CHARGE = 5;       // tenths of a second of drift that charge a mini-turbo
const int RESTITUTION_WALL = 30;  // percent
const int RESTITUTION_RACER = 40; // percent

int32_t Radius_Of(const Racer& r)
{
    return (int32_t)((int64_t)Kart_Of(r).Radius * FINE / 100);
}

int Cps100(int64_t v)
{
    return (int)(v * 10 * M.TicksPerSecondX10 / 16777216);
}

SpeedType Locomotion(const Racer& r)
{
    return UnitTypeClass::As_Reference((UnitType)Kart_Of(r).Type).Speed;
}

// Top speed and acceleration scale by the ground under the vehicle, in percent.
int Ground_Percent(const Racer& r)
{
    const CELL cell = XY_Cell(r.X >> 16, r.Y >> 16);
    if ((unsigned)cell >= MAP_CELL_TOTAL) return 80;
    switch (Map[cell].Land_Type()) {
    case LAND_CLEAR:
    case LAND_ROAD:
        return 100;
    case LAND_BEACH:
        return 85;
    case LAND_TIBERIUM:
        return 75;
    case LAND_WATER:
        return 90; // only a hovercraft gets here
    default:
        return 80;
    }
}

int32_t Wrap_Signed(int32_t angle)
{
    // Keeps a signed angle with 8 fraction bits inside -180..180 degrees.
    return (int32_t)((uint32_t)(angle + (1 << 23)) & 0xFFFFFF) - (1 << 23);
}

void Push_Out(Racer& r, int32_t nx, int32_t ny, int32_t depth, int restitution)
{
    r.X += (int32_t)(((int64_t)nx * depth) >> 16) + (nx > 0 ? 1 : nx < 0 ? -1 : 0);
    r.Y += (int32_t)(((int64_t)ny * depth) >> 16) + (ny > 0 ? 1 : ny < 0 ? -1 : 0);
    const int64_t vn = ((int64_t)r.VX * nx + (int64_t)r.VY * ny) >> 16;
    if (vn < 0) {
        const int64_t j = vn * (100 + restitution) / 100;
        r.VX -= (int32_t)((j * nx) >> 16);
        r.VY -= (int32_t)((j * ny) >> 16);
        if (-vn > r.Impact) r.Impact = (int32_t)-vn;
    }
}

// Pushes the racer's circle out of every blocked cell it overlaps.
void Collide_Static(Racer& r)
{
    const int32_t radius = Radius_Of(r);
    for (int pass = 0; pass < 3; ++pass) {
        bool moved = false;
        const int cx0 = (r.X - radius) >> 16, cx1 = (r.X + radius) >> 16;
        const int cy0 = (r.Y - radius) >> 16, cy1 = (r.Y + radius) >> 16;
        for (int cy = cy0; cy <= cy1; ++cy) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                if (cx < 0 || cy < 0 || cx >= MAP_CELL_W || cy >= MAP_CELL_H) continue;
                if (!Cell_Blocked(XY_Cell(cx, cy), &r)) continue;
                const int32_t x0 = cx << 16, x1 = x0 + FINE;
                const int32_t y0 = cy << 16, y1 = y0 + FINE;
                const int32_t qx = r.X < x0 ? x0 : r.X > x1 ? x1 : r.X;
                const int32_t qy = r.Y < y0 ? y0 : r.Y > y1 ? y1 : r.Y;
                const int64_t dx = (int64_t)r.X - qx, dy = (int64_t)r.Y - qy;
                const int64_t d2 = dx * dx + dy * dy;
                if (d2 >= (int64_t)radius * radius) continue;
                if (d2 == 0) {
                    // The centre is inside the square: leave by the nearest face.
                    const int32_t left = r.X - x0, right = x1 - r.X, top = r.Y - y0, bottom = y1 - r.Y;
                    int32_t best = left, nx = -65536, ny = 0;
                    if (right < best) best = right, nx = 65536, ny = 0;
                    if (top < best) best = top, nx = 0, ny = -65536;
                    if (bottom < best) best = bottom, nx = 0, ny = 65536;
                    Push_Out(r, nx, ny, best + radius, RESTITUTION_WALL);
                } else {
                    const int64_t d = Isqrt((uint64_t)d2);
                    const int32_t nx = (int32_t)(dx * 65536 / d), ny = (int32_t)(dy * 65536 / d);
                    Push_Out(r, nx, ny, (int32_t)(radius - d), RESTITUTION_WALL);
                }
                moved = true;
            }
        }
        if (!moved) break;
    }
}

void Move(Racer& r, int32_t dx, int32_t dy)
{
    if (!dx && !dy) return;
    const int32_t radius = Radius_Of(r);
    const int32_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    const int32_t longest = adx > ady ? adx : ady;
    const int32_t step = radius / 2 > 256 ? radius / 2 : 256;
    const int n = 1 + longest / step;
    int32_t done_x = 0, done_y = 0;
    for (int i = 1; i <= n; ++i) {
        const int32_t tx = (int32_t)((int64_t)dx * i / n), ty = (int32_t)((int64_t)dy * i / n);
        const int32_t sx = r.X, sy = r.Y;
        r.X += tx - done_x;
        r.Y += ty - done_y;
        done_x = tx;
        done_y = ty;
        Collide_Static(r);
        // The safety net: a centre must never come to rest in a blocked cell.
        if (Point_Blocked(r.X, r.Y, &r)) {
            r.X = sx;
            r.Y = sy;
            r.VX = r.VY = 0;
            return;
        }
    }
}

void Land(Racer& r, int* stats)
{
    const int32_t roll = (int16_t)(r.Roll >> 8), pitch = (int16_t)(r.Pitch >> 8);
    const int32_t landing = r.VZ;
    r.Z = 0;
    r.VZ = 0;
    r.PitchRate = r.RollRate = 0;
    if (roll > 16384 || roll < -16384 || pitch > 16384 || pitch < -16384) {
        // On its roof or side: it lies there until it rights itself. Thrown again while
        // already flipped, the three seconds keep counting rather than start over.
        if (!(r.Flags & RALLY_F_FLIPPED)) {
            r.Flags |= RALLY_F_FLIPPED;
            r.FlipTicks = 0;
            ++stats[STAT_FLIPS];
        }
        r.Roll = (roll < 0 ? -32767 : 32767) * 256;
        r.Pitch = 0;
        return;
    }
    // Wheels down, even if it went up flipped.
    r.Flags &= ~RALLY_F_FLIPPED;
    r.FlipTicks = 0;
    r.Roll = r.Pitch = 0;
    if (-landing > Velocity(400)) {
        r.VZ = -landing / 4; // a small bounce off a hard landing
        r.Z = 1;
    }
}

void Substep(Racer& r, int* stats)
{
    const KartStats& k = Kart_Of(r);
    const bool righting = (r.Flags & RALLY_F_RIGHTING) != 0;
    const bool flipped = (r.Flags & RALLY_F_FLIPPED) != 0;
    const bool finished = (r.Flags & RALLY_F_FINISHED) != 0;
    const bool air = !righting && (r.Z > 0 || r.VZ > 0);
    const bool control = M.Phase == RALLY_PHASE_RACE && !air && !flipped && !righting && r.Spin == 0 && !finished;
    const uint32_t bits = control ? r.Bits : 0;

    const int ground = Ground_Percent(r);
    int64_t top = (int64_t)Velocity(k.Top) * ground / 100;
    int64_t accel = (int64_t)Accel(k.Accel) * ground / 100;
    if (r.Boost > 0) {
        top = top * r.BoostTop / 100;
        accel = accel * r.BoostAccel / 100;
    }
    if (r.Armour > 0) top = top * 125 / 100;
    if (!r.Human || r.Autopilot) {
        static const int skill_speed[4] = {90, 95, 100, 100};
        top = top * skill_speed[r.Skill & 3] / 100;
    }

    // Forward and sideways parts of the velocity in the hull's frame.
    int32_t yaw16 = (r.Yaw >> 8) & 0xFFFF;
    int64_t fx = Sin16(yaw16), fy = -Cos16(yaw16);
    int64_t fv = ((int64_t)r.VX * fx + (int64_t)r.VY * fy) >> 16;

    // Steering grows with speed up to a third of the top speed, and reverses in reverse.
    int steer = ((bits & RALLY_IN_RIGHT) ? 1 : 0) - ((bits & RALLY_IN_LEFT) ? 1 : 0);
    // A drift starts with the drift key and a steer key at speed. It keeps turning that
    // way for as long as the drift key is held and the speed holds up, whatever the steer
    // keys do: steering into it tightens the turn, steering out of it widens it.
    if (!control || !(bits & RALLY_IN_DRIFT) || fv <= top * 40 / 100) {
        r.DriftDir = 0;
    } else if (r.DriftDir == 0 && steer != 0) {
        r.DriftDir = steer;
    }
    const bool drifting = r.DriftDir != 0;
    if (steer || drifting) {
        const int64_t afv = fv < 0 ? -fv : fv;
        const int64_t ramp = top / 3 > 0 ? top / 3 : 1;
        int64_t factor = afv * 100 / ramp;
        if (factor > 100) factor = 100;
        int64_t turn = (int64_t)Turn_Rate(k.Turn) * factor / 100;
        if (drifting) {
            turn = turn * (steer == r.DriftDir ? 120 : steer == -r.DriftDir ? 60 : 90) / 100;
            steer = r.DriftDir;
        } else if (fv < 0) {
            steer = -steer;
        }
        r.Yaw = (int32_t)((r.Yaw + steer * turn) & 0xFFFFFF);
    }
    if (r.Spin > 0) r.Yaw = (r.Yaw + r.SpinRate) & 0xFFFFFF;
    if (drifting) r.Flags |= RALLY_F_DRIFT;

    yaw16 = (r.Yaw >> 8) & 0xFFFF;
    fx = Sin16(yaw16);
    fy = -Cos16(yaw16);
    const int64_t rx = -fy, ry = fx; // right of the hull
    fv = ((int64_t)r.VX * fx + (int64_t)r.VY * fy) >> 16;
    int64_t lv = ((int64_t)r.VX * rx + (int64_t)r.VY * ry) >> 16;

    if (control) {
        const bool throttle = (bits & RALLY_IN_THROTTLE) != 0;
        const bool brake = (bits & RALLY_IN_BRAKE) != 0;
        if (throttle && !brake) {
            if (fv < 0) fv += accel * 2; // throttle while rolling back brakes first
            if (fv < top) {
                fv += accel;
                if (fv > top) fv = top;
            } else {
                fv -= ((fv - top) * Decay(200)) >> 16;
            }
        } else if (brake) {
            if (fv > 0) {
                int64_t brake_rate = (int64_t)Accel(k.Accel * 5 / 2 > 800 ? k.Accel * 5 / 2 : 800);
                fv -= brake_rate;
                if (fv < 0) fv = 0;
            } else {
                const int64_t back = -(top * 40 / 100);
                if (fv > back) {
                    fv -= accel * 70 / 100;
                    if (fv < back) fv = back;
                } else {
                    fv -= ((fv - back) * Decay(200)) >> 16;
                }
            }
        } else {
            fv -= (fv * Decay(80)) >> 16;
            const int64_t roll_drag = Accel(100);
            if (fv > roll_drag) fv -= roll_drag;
            else if (fv < -roll_drag) fv += roll_drag;
            else fv = 0;
        }
        if (fv > top && !throttle) fv -= ((fv - top) * Decay(200)) >> 16;
    } else if (air) {
        fv -= (fv * Decay(10)) >> 16;
    } else {
        fv -= (fv * Decay(flipped ? 500 : finished ? 250 : 150)) >> 16;
    }

    // Grip removes the sideways slide; a drift and the air keep most of it.
    int grip_rate;
    if (air) grip_rate = 10;
    else if (flipped || righting) grip_rate = 500;
    else if (drifting) grip_rate = k.Grip * 8;
    else grip_rate = k.Grip * 18;
    lv -= (lv * Decay(grip_rate)) >> 16;

    r.VX = (int32_t)((fv * fx + lv * rx) >> 16);
    r.VY = (int32_t)((fv * fy + lv * ry) >> 16);
    r.Speed = (int32_t)fv;

    if (air) {
        r.VZ -= Accel(GRAVITY);
        r.Z += r.VZ / 1024;
        r.Pitch = Wrap_Signed(r.Pitch + r.PitchRate);
        r.Roll = Wrap_Signed(r.Roll + r.RollRate);
        if (r.Z <= 0) Land(r, stats);
    }

    Move(r, r.VX / (256 * SUBSTEPS), r.VY / (256 * SUBSTEPS));
}

struct RamHit
{
    int Damage;
    int Source;
};

void Collide_Racers(RamHit* hits)
{
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& a = M.Racers[i];
        if (!Racer_Alive(a) || (a.Flags & RALLY_F_FINISHED)) continue;
        for (int j = i + 1; j < M.RacerCount; ++j) {
            Racer& b = M.Racers[j];
            if (!Racer_Alive(b) || (b.Flags & RALLY_F_FINISHED)) continue;
            if (abs(a.Z - b.Z) > FINE / 2) continue;
            const int64_t dx = (int64_t)b.X - a.X, dy = (int64_t)b.Y - a.Y;
            const int64_t reach = Radius_Of(a) + Radius_Of(b);
            const int64_t d2 = dx * dx + dy * dy;
            if (d2 >= reach * reach) continue;
            int64_t d = Isqrt((uint64_t)d2);
            int64_t nx = 65536, ny = 0;
            if (d > 0) {
                nx = dx * 65536 / d;
                ny = dy * 65536 / d;
            }
            const int64_t ma = Kart_Of(a).Mass, mb = Kart_Of(b).Mass;
            const int64_t depth = reach - d + 2;
            a.X -= (int32_t)((nx * depth * mb / (ma + mb)) >> 16);
            a.Y -= (int32_t)((ny * depth * mb / (ma + mb)) >> 16);
            b.X += (int32_t)((nx * depth * ma / (ma + mb)) >> 16);
            b.Y += (int32_t)((ny * depth * ma / (ma + mb)) >> 16);
            const int64_t vrel = (((int64_t)b.VX - a.VX) * nx + ((int64_t)b.VY - a.VY) * ny) >> 16;
            if (vrel < 0) {
                const int64_t imp = -vrel * (100 + RESTITUTION_RACER) / 100;
                a.VX -= (int32_t)(((imp * mb / (ma + mb)) * nx) >> 16);
                a.VY -= (int32_t)(((imp * mb / (ma + mb)) * ny) >> 16);
                b.VX += (int32_t)(((imp * ma / (ma + mb)) * nx) >> 16);
                b.VY += (int32_t)(((imp * ma / (ma + mb)) * ny) >> 16);
                const int impact = Cps100(-vrel);
                if (impact > 300) {
                    // A hard ram hurts the lighter vehicle (both when equal).
                    const int base = (impact - 300) * 6 / 100;
                    if (ma <= mb) {
                        const int dmg = (int)(base * mb / ma);
                        if (dmg > hits[i].Damage) hits[i] = {dmg, j};
                    }
                    if (mb <= ma) {
                        const int dmg = (int)(base * ma / mb);
                        if (dmg > hits[j].Damage) hits[j] = {dmg, i};
                    }
                }
                // Armour throws whatever it rams.
                if (a.Armour > 0 && b.Armour == 0 && hits[j].Damage < 60) hits[j] = {60, i};
                if (b.Armour > 0 && a.Armour == 0 && hits[i].Damage < 60) hits[i] = {60, j};
            }
            Collide_Static(a);
            Collide_Static(b);
            if (Point_Blocked(a.X, a.Y, &a)) {
                a.X = a.PrevX;
                a.Y = a.PrevY;
            }
            if (Point_Blocked(b.X, b.Y, &b)) {
                b.X = b.PrevX;
                b.Y = b.PrevY;
            }
        }
    }
}

void Tick_Flip(Racer& r, int* stats)
{
    if (r.Flags & RALLY_F_FLIPPED) {
        ++r.FlipTicks;
        if (r.FlipTicks >= Ticks(30) && r.Z == 0 && r.VZ <= 0) {
            r.Flags &= ~RALLY_F_FLIPPED;
            r.Flags |= RALLY_F_RIGHTING;
            r.Righting = Ticks(6);
            r.RightingRoll = r.Roll;
            r.RightingPitch = r.Pitch;
        }
    } else if (r.Flags & RALLY_F_RIGHTING) {
        // A short hop while it rolls back upright.
        const int total = Ticks(6);
        --r.Righting;
        const int t = total - r.Righting;
        const int64_t apex = FINE * 2 / 5;
        r.Z = (int32_t)(4 * apex * t * (total - t) / ((int64_t)total * total));
        r.VZ = 0;
        r.Roll = (int32_t)((int64_t)r.RightingRoll * r.Righting / total);
        r.Pitch = (int32_t)((int64_t)r.RightingPitch * r.Righting / total);
        if (r.Righting <= 0) {
            r.Flags &= ~RALLY_F_RIGHTING;
            r.Z = 0;
            r.Roll = r.Pitch = 0;
            r.FlipTicks = 0;
            ++stats[STAT_RIGHTED];
        }
    }
}

void Tick_Drift(Racer& r, int* stats)
{
    const bool on = (r.Flags & RALLY_F_DRIFT) != 0;
    if (on) {
        ++r.DriftCharge;
        if (r.DriftCharge >= Ticks(DRIFT_CHARGE)) r.Flags |= RALLY_F_TURBO_READY;
    } else if (r.DriftWasOn || r.DriftCharge) {
        // Letting go of a long drift gives a short mini-turbo.
        if (r.DriftCharge >= Ticks(DRIFT_CHARGE) && r.Boost == 0) {
            ++stats[STAT_MINI_TURBOS];
            r.Boost = Ticks(6);
            r.BoostTop = 130;
            r.BoostAccel = 200;
        }
        r.DriftCharge = 0;
        r.Flags &= ~RALLY_F_TURBO_READY;
    }
    r.DriftWasOn = on;
}

} // namespace

bool Cell_Blocked(CELL cell, const Racer* r)
{
    if ((unsigned)cell >= MAP_CELL_TOTAL) return true;
    const int x = Cell_X(cell), y = Cell_Y(cell);
    if (x < Map.MapCellX || y < Map.MapCellY || x >= Map.MapCellX + Map.MapCellWidth
        || y >= Map.MapCellY + Map.MapCellHeight) {
        return true;
    }
    const CellClass& c = Map[cell];
    if (c.Overlay != OVERLAY_NONE && OverlayTypeClass::As_Reference(c.Overlay).IsWall) return true;
    const SpeedType speed = r && r->Kart >= 0 ? Locomotion(*r) : SPEED_WHEEL;
    if (!Ground[c.Land_Type()].Cost[speed]) return true;
    if (c.Cell_Building() || c.Cell_Terrain()) return true;
    for (ObjectClass* object = c.Cell_Occupier(); object; object = object->Next) {
        if (object->What_Am_I() == RTTI_UNIT && !Rally_Is_Racer((TechnoClass*)object)) return true;
    }
    return false;
}

bool Point_Blocked(int32_t x, int32_t y, const Racer* r)
{
    if (x < 0 || y < 0) return true;
    return Cell_Blocked(XY_Cell(x >> 16, y >> 16), r);
}

void Drive_Begin(Racer& r)
{
    r.Impact = 0;
    r.PrevX = r.X;
    r.PrevY = r.Y;
    r.Flags &= ~RALLY_F_DRIFT;
}

void Drive_Substep(Racer& r, int* stats)
{
    if (Racer_Alive(r)) Substep(r, stats);
}

void Drive_End(Racer& r, int* stats)
{
    if (!Racer_Alive(r)) return;
    Tick_Flip(r, stats);
    Tick_Drift(r, stats);
}

void Physics_Tick()
{
    RamHit hits[RALLY_MAX_RACERS];
    memset(hits, 0, sizeof hits);
    for (int i = 0; i < M.RacerCount; ++i) Drive_Begin(M.Racers[i]);
    for (int step = 0; step < SUBSTEPS; ++step) {
        for (int i = 0; i < M.RacerCount; ++i) Drive_Substep(M.Racers[i], M.Stats);
        Collide_Racers(hits);
    }
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        if (!Racer_Alive(r)) continue;
        Drive_End(r, M.Stats);
        Sync_Engine(r);
    }
    // Damage last: it can destroy a unit, which must not happen inside the steps.
    for (int i = 0; i < M.RacerCount; ++i) {
        Racer& r = M.Racers[i];
        if (!Racer_Alive(r)) continue;
        const int impact = Cps100(r.Impact);
        if (impact > 300) {
            ++M.Stats[STAT_WALL_HITS];
            Damage_Racer(r, (impact - 300) * 6 / 100 + 1, -1);
        }
        if (hits[i].Damage > 0 && Racer_Alive(r)) {
            ++M.Stats[STAT_RAMS];
            const bool thrown = Racer_Index_Valid(hits[i].Source) && M.Racers[hits[i].Source].Armour > 0;
            Damage_Racer(r, hits[i].Damage, hits[i].Source);
            if (thrown) Throw_Racer(r, 60, 100);
        }
    }
}

void Sync_Engine(Racer& r)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit) return;
    const COORDINATE coord = XY_Coord(r.X >> 8, r.Y >> 8);
    if (coord != unit->Coord) {
        const CELL from = Coord_Cell(unit->Coord);
        unit->Mark(MARK_UP);
        unit->Coord = coord;
        unit->Mark(MARK_DOWN);
        const CELL to = Coord_Cell(coord);
        if (to != from) {
            Map[to].Goodie_Check(unit);
            if (Racer_Unit(r)) {
                // The engine gives artillery a setup delay when it comes to a halt in a cell;
                // a racer passes through cells without halting, so its rearm count is kept.
                const int arm = unit->Arm;
                unit->Per_Cell_Process(false);
                if (Racer_Unit(r)) unit->Arm = arm;
            }
        }
    }
    if (!Racer_Unit(r)) return;
    unit->PrimaryFacing.Set((DirType)((r.Yaw >> 16) & 255));
    if (unit->Class->IsTurretEquipped) {
        unit->SecondaryFacing.Set((DirType)((r.Turret >> 16) & 255));
    } else {
        r.Turret = r.Yaw;
    }
}

// Infantry under a racer is run over, the way a tracked vehicle crushes it.
void Run_Over(Racer& r)
{
    UnitClass* unit = Racer_Unit(r);
    if (!unit || r.Z > FINE / 4 || M.Phase != RALLY_PHASE_RACE) return;
    const int32_t radius = Radius_Of(r) + FINE / 4;
    const int cx0 = (r.X - radius) >> 16, cx1 = (r.X + radius) >> 16;
    const int cy0 = (r.Y - radius) >> 16, cy1 = (r.Y + radius) >> 16;
    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            if (cx < 0 || cy < 0 || cx >= MAP_CELL_W || cy >= MAP_CELL_H) continue;
            const CELL cell = XY_Cell(cx, cy);
            ObjectClass* object = Map[cell].Cell_Occupier();
            while (object) {
                ObjectClass* next = object->Next;
                if (object->What_Am_I() == RTTI_INFANTRY && !unit->House->Is_Ally(object)
                    && !object->OpenCNC3D_Is_Invincible()) {
                    const COORDINATE c = object->Center_Coord();
                    const int64_t dx = (int64_t)Coord_X(c) * 256 - r.X, dy = (int64_t)Coord_Y(c) * 256 - r.Y;
                    if (dx * dx + dy * dy < (int64_t)radius * radius) {
                        Sound_Effect(VOC_SQUISH2, c);
                        object->Record_The_Kill(unit);
                        object->Mark(MARK_UP);
                        object->Limbo();
                        delete object;
                        if (Map[cell].Overlay == OVERLAY_NONE) new OverlayClass(OVERLAY_SQUISH, cell);
                        ++M.Stats[STAT_RUN_OVER];
                    }
                }
                object = next;
            }
        }
    }
}

} // namespace rally
