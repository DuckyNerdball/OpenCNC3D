// OpenCNC3D Tiberian Dawn Gun Game abilities: the engine half of the contract in
// game/gun_game_ability_abi.h. Every rule and timer here runs in lockstep, so it reads
// only synchronised state and the engine's own random number generator.
#ifndef TD_GUN_GAME_ABILITY_H
#define TD_GUN_GAME_ABILITY_H

#include <stdint.h>

class HouseClass;
class ObjectClass;
class TechnoClass;
class FootClass;
struct GunEffect;

// Lifetime, called from gun_game.cpp.
void Gun_Ability_Reset();
void Gun_Ability_Tick();
void Gun_Ability_AI();
unsigned Gun_Ability_Hash(unsigned hash);

// Crates and deaths.
bool Gun_Ability_Booby_Crate(FootClass* unit, CELL cell);
void Gun_Ability_Give(HousesType owner, CELL cell);
void Gun_Ability_Died(TechnoClass* victim);
bool Gun_Ability_Spare(TechnoClass* victim);
CELL Gun_Ability_Respawn_Cell(HousesType owner);
bool Gun_Ability_Decoy(const TechnoClass* unit);
// Puts a unit taken out for a teleport back where it stood; it never fails.
void Gun_Ability_Restore(FootClass* unit, COORDINATE where, DirType facing);

// Combat hooks in the engine classes.
int Gun_Ability_Damage(const TechnoClass* unit, int damage);
void Gun_Ability_Damaged(TechnoClass* victim, TechnoClass* source, int damage, ResultType result);
void Gun_Ability_Fired(const TechnoClass* shooter);
bool Gun_Ability_Fire_Blocked(const TechnoClass* shooter, TARGET target);
bool Gun_Ability_Hidden(HousesType viewer, const ObjectClass* object);
bool Gun_Ability_Crush_Blocked(const TechnoClass* crusher, const ObjectClass* victim);
bool Gun_Ability_Dancing(HousesType owner);
int Gun_Ability_Speed(const TechnoClass* unit, int speed);
int Gun_Ability_Rearm(const TechnoClass* unit, int delay);
CELL Gun_Ability_Strike_Cell(HousesType owner);

// Lockstep events.
void Gun_Ability_Activate(HouseClass* house, int ability, int target, int count, int serial, int life,
                          const CELL* cells);
void Gun_Ability_Cells(HouseClass* house, int serial, int index, const CELL* cells);
void Gun_Ability_Grant(HouseClass* house, int ability);

// Export layer.
int Gun_Ability_Held(HousesType owner);
bool Gun_Ability_Check(HousesType owner, int ability, int target, int count, const CELL* cells);
unsigned Gun_Ability_Life(HousesType owner);
unsigned Gun_Ability_Flags(HousesType viewer, const ObjectClass* object);
int Gun_Ability_Effects(HousesType viewer, GunEffect* out, int max);
int Gun_Ability_Timers(HousesType owner, int32_t* remaining, int32_t* ticks_per_second);
bool Gun_Ability_Test_Enabled();

#endif
