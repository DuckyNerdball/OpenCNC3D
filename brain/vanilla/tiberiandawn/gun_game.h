#ifndef TD_GUN_GAME_H
#define TD_GUN_GAME_H

class HouseClass;
class TechnoClass;
class UnitClass;

bool Gun_Game_Active();
void Gun_Game_Reset(bool enabled);
void Gun_Game_Set_Rate(int ticks_per_two_seconds);
bool Gun_Game_Start();
void Gun_Game_Tick();
void Gun_Game_Hit(TechnoClass* victim, TechnoClass* source);
void Gun_Game_Kill(TechnoClass* victim, TechnoClass* source);
bool Gun_Game_Offer(HouseClass* house, int* token, const char** left, const char** right);
const char* Gun_Game_Standing(HousesType owner, int* level, unsigned* kills, bool* alive);
void Gun_Game_Choose(HouseClass* house, int command);
int Gun_Game_Dodge_Token(HousesType owner);
void Gun_Game_Dodge(HouseClass* house, int token);
bool Gun_Game_Dodging(const TechnoClass* unit);
bool Gun_Game_Airborne(const TechnoClass* unit);
bool Gun_Game_Jump_Step(UnitClass* unit);
bool Gun_Game_Scud_Ready(const TechnoClass* unit);
void Gun_Game_Scud_Fired(const TechnoClass* unit);
int Gun_Game_Chrono_Command(HousesType owner, int x, int y);
void Gun_Game_Chrono(HouseClass* house, int command);
bool Gun_Game_Combat_Aircraft(const TechnoClass* unit);
TechnoClass* Gun_Game_Combatant(HousesType owner);
void Gun_Game_Strike_Damage(COORDINATE coord, int damage, HousesType owner, WarheadType warhead);
bool Gun_Game_Collect_Crate(FootClass* unit, CELL cell);
int Gun_Game_Roll_Phase(const TechnoClass* unit);
int Gun_Game_Jump_Height(const TechnoClass* unit);
void Gun_Game_Roll_Step(InfantryClass* unit);
int Gun_Game_Dodge_Damage(const TechnoClass* unit, int damage);
unsigned Gun_Game_Hash();

// Ladder state for the abilities (gun_game_ability.cpp).
bool Gun_Game_In_Match(HousesType owner);
bool Gun_Game_Over();
int Gun_Game_Tenths(int tenths);
int Gun_Game_Ticks_Per_Second();
unsigned Gun_Game_Life(HousesType owner);
int Gun_Game_Tier(HousesType owner);
unsigned Gun_Game_Kills(HousesType owner);
bool Gun_Game_Demote(HousesType owner);
TechnoClass* Gun_Game_Spawn_Copy(HousesType owner, CELL center);
void Gun_Game_Strike_Hit(TechnoClass* victim, int damage, int distance, HousesType owner, WarheadType warhead);

#endif
