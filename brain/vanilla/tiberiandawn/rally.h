// OpenCNC3D Tiberian Dawn Rally: what the engine calls.
//
// Rally is a kart race with the game's own vehicles on a course map that carries a
// [Rally] section: laps of a circuit, or one run of a point-to-point course. The module takes every racer's vehicle away from
// the engine's mission and movement code and drives it with its own integer physics,
// while engine weapons, damage, explosions and the course's hostile defences keep
// working on those vehicles as on any unit. Every hook below is a no-op outside a
// Rally match (Rally_Active() false), so other game modes behave exactly as before.
//
// Include after function.h; the engine types used here come from there.
#ifndef TD_RALLY_H
#define TD_RALLY_H

#include <stdint.h>

class CCINIClass;
class HouseClass;
class ObjectClass;
class TechnoClass;
class FootClass;
class UnitClass;
class BuildingClass;
struct RallyState;
struct RallyCoursePoint;
struct RallyPredictStep;
struct RallyRacer;

// The switch and the match.
bool Rally_Active();
void Rally_Enable(bool enabled, int ticks_per_second_x10);
int Rally_Configure(int laps);         // laps of the next race on a circuit (clamped); kept until changed
bool Rally_Read_INI(CCINIClass& ini); // false: the course is unusable and the match is refused
void Rally_Tick();                    // once per engine tick, after the queued orders ran

// Engine hooks.
bool Rally_Unit_AI(UnitClass* unit);             // true: a racer; the engine's unit AI is skipped
bool Rally_Is_Racer(const TechnoClass* object);
bool Rally_Blocks_Order(const TechnoClass* object); // player orders to a racer are ignored
int Rally_Damage(TechnoClass* victim, int damage, TechnoClass* source); // the damage a racer really takes
void Rally_Damaged(TechnoClass* victim, int damage, int warhead, TechnoClass* source);
void Rally_Kill(TechnoClass* victim, TechnoClass* source);
bool Rally_Mortal(const ObjectClass* object);       // a racer is damageable even if its type is immune
bool Rally_Crate(FootClass* object, CELL cell);     // a crate cell was entered
bool Rally_Full_Power(const HouseClass* house);     // the hostile house never lacks power
int Rally_Knows(const HouseClass* house, const TechnoClass* object); // -1 no opinion, else 0/1
bool Rally_Ion_Anim();                              // the ion animation does no engine damage
bool Rally_Skip_House_AI(const HouseClass* house);  // racer seats run no skirmish AI
// Where a round of BulletType bullet, fired by firer from `from` at target, should be
// aimed: `to` unless the target is a racer, then where that racer will be on impact.
COORDINATE Rally_Lead(TARGET target, const TechnoClass* firer, int bullet, COORDINATE from, COORDINATE to);
int Rally_Hit_Distance(const ObjectClass* object, int distance); // explosion distance to a racer's edge
bool Rally_Obelisk_Hold(const BuildingClass* building);         // a racer near: keep charging
int Rally_Obelisk_Rate(int rate);                                // the obelisk's charge rate per stage

// Orders (event.cpp executes them on every peer).
void Rally_Event_Pick(HouseClass* house, int roster_index);
void Rally_Event_Input(HouseClass* house, uint32_t bits, int32_t aim_x, int32_t aim_y);
void Rally_Event_Autopilot(HouseClass* house, int on);
void Rally_Event_Choose(HouseClass* house, int choice); // RALLY_CHOICE_*, acted on from the deciding player only

// Exports (dllinterface.cpp).
int Rally_Get_State(HousesType local, RallyState* out);
int Rally_Predict(HousesType house, const RallyPredictStep* steps, int n, RallyRacer* out); // changes nothing
int Rally_Get_Course(RallyCoursePoint* out, int max);
int Rally_Get_Roster(int32_t* out, int max);
int Rally_Roster_Count();
bool Rally_Roster_Valid(int index);
uint32_t Rally_Hash();
int Rally_Debug(int op, int a, int b, int c);

#endif
