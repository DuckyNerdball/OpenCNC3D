/* Deterministic fair skirmish controller. Included by house.cpp after function.h.
 * All opponent information enters through Knows; TD's explored cells remain
 * visible under its original shroud rules. No live hidden target is retained.
 */
#include "fair_ai_td.h"
#include "../../fair_ai.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern unsigned int RandNumb;
// Defined with the placement rule below; the planner asks it before ordering a building.
static int Ground_Pieces(HouseClass const* h, int speed, short const* footprint, CELL origin);
namespace {
struct TDPolicyState {
    uint32_t level, rng, next_think, next_tactic, next_scout, next_attack;
    uint32_t thoughts, orders, scouting, attacks, cursor;
    uint32_t scout[2], scout_goal[2], scout_position[2], objective[3], attacking;
    uint32_t known_cells, contacts, infantry, armor, air;
};
static_assert(sizeof(TDPolicyState)==26u*sizeof(uint32_t), "TD fair AI save layout must remain explicit words");
TDPolicyState td_policy[HOUSE_COUNT];
unsigned char td_pending[fair_ai::Seats] = {1,1,1,1,1,1,1,1};
TDPolicyState& Policy(HouseClass const* h) { return td_policy[(int)h->Class->House]; }
bool Live(TechnoClass const* o) { return o && o->IsActive && !o->IsInLimbo && o->Strength > 0; }
bool Friendly(HouseClass const* h, TechnoClass const* o) { return Live(o) && o->House == h; }
int BuildingCount(HouseClass const* h, StructType type) {
    int n=0; for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i);
        if(Friendly(h,b) && b->Class->Type==type && b->Mission!=MISSION_CONSTRUCTION && b->Mission!=MISSION_DECONSTRUCTION) ++n; } return n;
}
int UnitCount(HouseClass const* h, UnitType type) {
    int n=0; for(int i=0;i<Units.Count();++i) { UnitClass* u=Units.Ptr(i); if(u && u->IsActive && u->House==h && u->Strength>0 && u->Class->Type==type) ++n; } return n;
}
// Live counts read from the object heaps. The house's CurUnits counts every foot
// soldier and vehicle together, so it cannot tell an army's mix apart.
int InfantryCount(HouseClass const* h) {
    int n=0; for(int i=0;i<Infantry.Count();++i) { InfantryClass* u=Infantry.Ptr(i); if(u && u->IsActive && u->House==h && u->Strength>0) ++n; } return n;
}
int CombatVehicleCount(HouseClass const* h) {
    int n=0; for(int i=0;i<Units.Count();++i) { UnitClass* u=Units.Ptr(i);
        if(u && u->IsActive && u->House==h && u->Strength>0 && u->Class->Type!=UNIT_HARVESTER && u->Class->Type!=UNIT_MCV && u->Class->Primary!=WEAPON_NONE) ++n; } return n;
}
int Planned(HouseClass const* h,StructType type) {
    int n=0;for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i);
        if(b && b->IsActive && b->House==h && b->Strength>0 && b->Class->Type==type && b->Mission!=MISSION_DECONSTRUCTION) ++n; }return n;
}
bool BuildingQueue(HouseClass const* h) {
    for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i);if(b && b->House==h && ((b->Factory && b->Class->ToBuild==RTTI_BUILDINGTYPE) || b->Mission==MISSION_CONSTRUCTION)) return true; }return false;
}
// A house with a working refinery and a harvester can count on income. Every factory
// draws its price in per-tick installments and pauses while the house is broke, which
// is the same rule a human's sidebar follows, so an order started on income simply
// waits for that income instead of for a lump sum the storage could never hold.
bool HasIncome(HouseClass const* h) {
    return !h->IsTiberiumShort && BuildingCount(h,STRUCT_REFINERY)>0 && UnitCount(h,UNIT_HARVESTER)>0;
}
// Whether the base has room for this building now. The same test the engine's own
// placement search makes for a computer house (an explored cell inside the base
// zones, clear ground, next to an owned building), run over the zones' bounding box.
// A finished building that cannot be placed is abandoned by the engine and its money
// refunded, so ordering it again would tie the construction yard up for good.
// The engine also refuses, for a skirmish computer, a spot that would wall part of the
// ground off (TD_Fair_AI_Keeps_Paths), so a spot only counts here if it passes that rule
// too; otherwise a base whose only free spot is its own exit would build, abandon and
// rebuild the same building. The same bound on refused spots applies.
static const int KEEP_PATHS_TRIES=24;
bool CanPlace(HouseClass* h, BuildingTypeClass const& b) {
    if(!h->Center) return true;
    short const* list=b.Occupy_List(true);
    CELL center=Coord_Cell(h->Center);
    int r=h->Radius*4/CELL_LEPTON_W+3, cx=Cell_X(center), cy=Cell_Y(center);
    int speed=UnitTypeClass::As_Reference(UNIT_HARVESTER).Speed, before=-1, refused=0;
    for(int y=max(Map.MapCellY,cy-r);y<=min(Map.MapCellY+Map.MapCellHeight-1,cy+r);++y)
        for(int x=max(Map.MapCellX,cx-r);x<=min(Map.MapCellX+Map.MapCellWidth-1,cx+r);++x) {
            CELL cell=XY_Cell(x,y);
            if(!Map.In_Radar(cell) || h->Which_Zone(cell)==ZONE_NONE || !Map[cell].Is_Mapped(h->Class->House)) continue;
            if(!b.Legal_Placement(cell) || !Map.Passes_Proximity_Check(&b,h->Class->House,list,cell)) continue;
            if(before<0) before=Ground_Pieces(h,speed,NULL,0);
            if(Ground_Pieces(h,speed,b.Occupy_List(false),cell)<=before) return true;
            if(++refused>=KEEP_PATHS_TRIES) return false;
        }
    return false;
}
bool Build(HouseClass* h, StructType t, bool urgent=false) {
    BuildingTypeClass const& b=BuildingTypeClass::As_Reference(t);
    if(!h->Can_Build(&b,h->ActLike)) return false;
    if(!urgent && h->Available_Money()<b.Cost_Of() && !HasIncome(h)) return false;
    // A building that would black the base out gets its power plant first.
    if(b.Drain>0 && h->Power<h->Drain+b.Drain && t!=STRUCT_POWER && t!=STRUCT_ADVANCED_POWER) {
        StructType plant=h->Can_Build(STRUCT_ADVANCED_POWER,h->ActLike) ? STRUCT_ADVANCED_POWER:STRUCT_POWER;
        if(!h->Can_Build(plant,h->ActLike) || !CanPlace(h,BuildingTypeClass::As_Reference(plant))) {
            plant=STRUCT_POWER;
            if(!h->Can_Build(plant,h->ActLike)) return false;
        }
        t=plant;
    }
    if(!CanPlace(h,BuildingTypeClass::As_Reference(t))) return false;
    h->BuildStructure=t; return true;
}
// The structure the construction yard is producing right now, if any.
StructType InProduction(HouseClass const* h) {
    for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i);
        if(b && b->House==h && b->Factory && b->Class->ToBuild==RTTI_BUILDINGTYPE) {
            TechnoClass* product=b->Factory->Get_Object();
            if(product && product->What_Am_I()==RTTI_BUILDING) return ((BuildingClass*)product)->Class->Type;
        } }
    return STRUCT_NONE;
}
int CellDistance(CELL a, CELL b) { return abs(Cell_X(a)-Cell_X(b))+abs(Cell_Y(a)-Cell_Y(b)); }
CELL Home(HouseClass* h) {
    if(h->Center) return Coord_Cell(h->Center);
    for(int i=0;i<Units.Count();++i) if(Friendly(h,Units.Ptr(i))) return Coord_Cell(Units.Ptr(i)->Center_Coord());
    return XY_Cell(Map.MapCellX+Map.MapCellWidth/2,Map.MapCellY+Map.MapCellHeight/2);
}
// The army gathers a few cells outside the base, on the side facing the middle of
// the map, never in the middle of the base: a soldier standing on a cell blocks a
// new building there, and a base full of idle units has nowhere left to build.
CELL Rally(HouseClass* h) {
    CELL home=Home(h);
    int cx=Cell_X(home), cy=Cell_Y(home);
    int dx=Map.MapCellX+Map.MapCellWidth/2-cx, dy=Map.MapCellY+Map.MapCellHeight/2-cy;
    int d=abs(dx)+abs(dy);
    if(d<=0) return home;
    int step=min(7,d);
    int x=cx+dx*step/d, y=cy+dy*step/d;
    x=max(Map.MapCellX+1,min(Map.MapCellX+Map.MapCellWidth-2,x));
    y=max(Map.MapCellY+1,min(Map.MapCellY+Map.MapCellHeight-2,y));
    return XY_Cell(x,y);
}
void OwnSight(HouseClass* h) {
    // Legacy Revealed uses one shared computer bit and may skip the owner's
    // look when another computer already saw the object. Refresh legitimate
    // own-unit sight per house; sight radii/terrain rules remain engine-owned.
    for(int i=0;i<Buildings.Count();++i) if(Friendly(h,Buildings.Ptr(i))) Buildings.Ptr(i)->Look(false);
    for(int i=0;i<Units.Count();++i) if(Friendly(h,Units.Ptr(i))) Units.Ptr(i)->Look(false);
    for(int i=0;i<Infantry.Count();++i) if(Friendly(h,Infantry.Ptr(i))) Infantry.Ptr(i)->Look(false);
}
void Observe(HouseClass* h) {
    TDPolicyState& s=Policy(h); s.known_cells=s.contacts=s.infantry=s.armor=s.air=0;
    for(int y=Map.MapCellY;y<Map.MapCellY+Map.MapCellHeight;++y)
        for(int x=Map.MapCellX;x<Map.MapCellX+Map.MapCellWidth;++x) if(Map[XY_Cell(x,y)].Is_Mapped(h)) ++s.known_cells;
    h->Enemy=HOUSE_NONE; int nearest=0x7fffffff;
    for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i); if(!TD_Fair_AI_Knows(h,b)) continue;
        ++s.contacts; int d=CellDistance(Home(h),Coord_Cell(b->Center_Coord())); if(d<nearest) { nearest=d;h->Enemy=b->Owner(); } }
    for(int i=0;i<Units.Count();++i) if(TD_Fair_AI_Knows(h,Units.Ptr(i))) { ++s.armor; ++s.contacts; }
    for(int i=0;i<Infantry.Count();++i) if(TD_Fair_AI_Knows(h,Infantry.Ptr(i))) { ++s.infantry; ++s.contacts; }
    for(int i=0;i<Aircraft.Count();++i) if(TD_Fair_AI_Knows(h,Aircraft.Ptr(i))) { ++s.air; ++s.contacts; }
}
int RemainingCost(FactoryClass* factory) {
    TechnoClass* product=factory->Get_Object();if(!product) return 0;
    // The legacy public interface exposes stage and purchase price. Replay the
    // same integer installments as FactoryClass::AI/Cost_Per_Tick to preserve
    // already-paid money exactly without changing its saved object layout.
    int balance=product->PurchasePrice;
    for(int stage=1;stage<=min(108,factory->Completion());++stage) {
        int remaining=108-stage;balance-=remaining ? balance/remaining : balance;
    }
    return max(0,balance);
}
bool RecoverIncome(HouseClass* h,TDPolicyState& s) {
    int refs=BuildingCount(h,STRUCT_REFINERY), harvesters=UnitCount(h,UNIT_HARVESTER);
    int war=BuildingCount(h,STRUCT_WEAP)+BuildingCount(h,STRUCT_AIRSTRIP);
    int power=BuildingCount(h,STRUCT_POWER)+BuildingCount(h,STRUCT_ADVANCED_POWER);
    int queued_harvesters=0;
    for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i);
        if(b && b->House==h && b->Factory && b->Factory->Get_Object()
            && b->Factory->Get_Object()->What_Am_I()==RTTI_UNIT
            && ((UnitClass*)b->Factory->Get_Object())->Class->Type==UNIT_HARVESTER) ++queued_harvesters;
    }
    if(refs && harvesters>queued_harvesters) return false;
    // With no room yet for a refinery (the base has explored too little ground), the
    // ordinary plan runs instead: a barracks and its soldiers widen the explored area.
    if(!refs && !Planned(h,STRUCT_REFINERY) && BuildingCount(h,STRUCT_CONST)
        && !CanPlace(h,BuildingTypeClass::As_Reference(STRUCT_REFINERY))) return false;
    h->BuildStructure=STRUCT_NONE;h->BuildUnit=UNIT_NONE;
    h->BuildInfantry=INFANTRY_NONE;h->BuildAircraft=AIRCRAFT_NONE;
    int needed=0,pending_refund=0,cancelled=0;
    bool queued_income=false,queued_power=false;
    for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i);
        if(!b || !b->IsActive || b->House!=h || b->Strength<=0) continue;
        if(b->Mission==MISSION_DECONSTRUCTION || b->MissionQueue==MISSION_DECONSTRUCTION) pending_refund+=b->Refund_Amount();
        if(!b->Factory) continue;
        TechnoClass* product=b->Factory->Get_Object();if(!product) continue;
        bool income=product->What_Am_I()==RTTI_UNIT && ((UnitClass*)product)->Class->Type==UNIT_HARVESTER;
        bool essential=income;
        if(product->What_Am_I()==RTTI_BUILDING) {
            StructType type=((BuildingClass*)product)->Class->Type;
            if(type==STRUCT_REFINERY) income=essential=true;
            if(type==STRUCT_POWER || type==STRUCT_ADVANCED_POWER) {
                essential=!power || h->Power<h->Drain+20;
                if(essential) queued_power=true;
            }
            if(!war && (type==STRUCT_WEAP || type==STRUCT_AIRSTRIP)) essential=true;
        }
        if(essential) { needed+=RemainingCost(b->Factory);queued_income|=income; }
        else if(cancelled<max(1,fair_ai::Get(s.level).max_orders-1)) {
            b->Factory->Abandon();delete b->Factory;b->Factory=NULL;
            ++cancelled;++s.orders;
        }
    }
    StructType structure=STRUCT_NONE;UnitType unit=UNIT_NONE;
    bool growing_refinery=Planned(h,STRUCT_REFINERY)>refs;
    if(!queued_income && !growing_refinery) {
        if(refs && war && h->Can_Build(UNIT_HARVESTER,h->ActLike)) {
            unit=UNIT_HARVESTER;needed+=(int)(UnitTypeClass::As_Reference(unit).Cost_Of()*h->CostBias);
        } else if(BuildingCount(h,STRUCT_CONST)) {
            structure=STRUCT_REFINERY;
            if(!h->Can_Build(structure,h->ActLike)) {
                structure=h->Can_Build(STRUCT_ADVANCED_POWER,h->ActLike) ? STRUCT_ADVANCED_POWER:STRUCT_POWER;
            }
            if(!h->Can_Build(structure,h->ActLike) || (queued_power && (structure==STRUCT_POWER || structure==STRUCT_ADVANCED_POWER))) structure=STRUCT_NONE;
            if(structure!=STRUCT_NONE) needed+=(int)(BuildingTypeClass::As_Reference(structure).Cost_Of()*h->CostBias);
        }
    }
    bool recoverable=queued_income || growing_refinery || queued_power || structure!=STRUCT_NONE || unit!=UNIT_NONE;
    if(recoverable && h->Available_Money()+pending_refund<needed) {
        BuildingClass* sale=NULL;int value=-1;
        for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i);
            if(!Friendly(h,b) || b->Mission==MISSION_CONSTRUCTION || b->Mission==MISSION_DECONSTRUCTION
                || b->MissionQueue==MISSION_DECONSTRUCTION) continue;
            StructType type=b->Class->Type;
            if(type==STRUCT_CONST || type==STRUCT_REFINERY || ((type==STRUCT_WEAP || type==STRUCT_AIRSTRIP) && war<=1)) continue;
            if((type==STRUCT_POWER || type==STRUCT_ADVANCED_POWER) && (power<=1 || h->Power-b->Power_Output()<h->Drain)) continue;
            int score=b->Refund_Amount();if(type==STRUCT_BARRACKS || type==STRUCT_HAND) score/=4;
            if(score>value) { value=score;sale=b; }
        }
        // Last resort: with a refinery and a war factory still standing, a harvester
        // is the whole way back to an income, and the Construction Yard is the only
        // building left whose ordinary refund can pay for one.
        if(!sale && refs && war && (unit==UNIT_HARVESTER || queued_harvesters>0)) {
            for(int i=0;i<Buildings.Count() && !sale;++i) { BuildingClass* b=Buildings.Ptr(i);
                if(Friendly(h,b) && b->Class->Type==STRUCT_CONST && b->Mission!=MISSION_CONSTRUCTION
                    && b->Mission!=MISSION_DECONSTRUCTION && b->MissionQueue!=MISSION_DECONSTRUCTION) sale=b; }
        }
        if(sale) { sale->Sell_Back(1);++s.orders; }
    }
    // Starting a normally paid essential queue may consume available cash in
    // installments; no subsequent discretionary queue can take its reserve.
    if(structure!=STRUCT_NONE && !queued_income && !BuildingQueue(h)) h->BuildStructure=structure;
    if(unit!=UNIT_NONE && !queued_income) h->BuildUnit=unit;
    return true;
}
// A Construction Yard deployed against the map edge sees only a few cells, and a
// building can only go where the house has explored, so a corner start could hold no
// refinery at all and the house never built again. In the first half minute an MCV
// that stands closer than this to an edge drives inward before it deploys, the way a
// player would; after that, or once it is clear of the edges, it deploys where it is.
static const int MCV_EDGE_MARGIN=8;
void PlaceMCV(HouseClass* h) {
    if(Planned(h,STRUCT_CONST)) return;
    for(int i=0;i<Units.Count();++i) { UnitClass* u=Units.Ptr(i);
        if(!Friendly(h,u) || u->Class->Type!=UNIT_MCV) continue;
        CELL c=Coord_Cell(u->Center_Coord()); int x=Cell_X(c), y=Cell_Y(c);
        int x0=Map.MapCellX+MCV_EDGE_MARGIN, x1=Map.MapCellX+Map.MapCellWidth-1-MCV_EDGE_MARGIN;
        int y0=Map.MapCellY+MCV_EDGE_MARGIN, y1=Map.MapCellY+Map.MapCellHeight-1-MCV_EDGE_MARGIN;
        if(x0>x1 || y0>y1) return;
        bool cramped=x<x0 || x>x1 || y<y0 || y>y1;
        if(cramped && (uint32_t)Frame<450u) {
            if(!Target_Legal(u->NavCom) && u->Mission!=MISSION_MOVE) {
                u->Assign_Mission(MISSION_MOVE);u->Assign_Destination(::As_Target(XY_Cell(max(x0,min(x1,x)),max(y0,min(y1,y)))));
                ++Policy(h).orders;
            }
        } else if(u->Mission!=MISSION_HUNT && !Target_Legal(u->NavCom)) {
            u->Assign_Mission(MISSION_HUNT);++Policy(h).orders;
        }
        return;
    }
}
void Economy(HouseClass* h) {
    TDPolicyState& s=Policy(h); fair_ai::Profile const& p=fair_ai::Get(s.level);
    int ref=BuildingCount(h,STRUCT_REFINERY), harv=UnitCount(h,UNIT_HARVESTER);
    int barr=BuildingCount(h,STRUCT_BARRACKS)+BuildingCount(h,STRUCT_HAND);
    int war=BuildingCount(h,STRUCT_WEAP)+BuildingCount(h,STRUCT_AIRSTRIP);
    int power=BuildingCount(h,STRUCT_POWER)+BuildingCount(h,STRUCT_ADVANCED_POWER);
    int money=h->Available_Money();
    int level=fair_ai::Clamp((int)s.level);
    // Simulation minutes (15 ticks a second); every timed goal reads game time only.
    int minute=(int)((uint32_t)Frame/900u);
    // Pending requests are ordinary proposed commands. A lost prerequisite or
    // captured faction factory must not leave a now-illegal request wedged.
    if(h->BuildStructure!=STRUCT_NONE && !h->Can_Build(h->BuildStructure,h->ActLike)) h->BuildStructure=STRUCT_NONE;
    if(h->BuildUnit!=UNIT_NONE && !h->Can_Build(h->BuildUnit,h->ActLike)) h->BuildUnit=UNIT_NONE;
    if(h->BuildInfantry!=INFANTRY_NONE && !h->Can_Build(h->BuildInfantry,h->ActLike)) h->BuildInfantry=INFANTRY_NONE;
    if(h->BuildAircraft!=AIRCRAFT_NONE && !h->Can_Build(h->BuildAircraft,h->ActLike)) h->BuildAircraft=AIRCRAFT_NONE;
    if(RecoverIncome(h,s)) return;
    bool income=HasIncome(h);
    // Evaluate a new plan only after production consumed the preceding request.
    // The goals are tried in order and a goal that cannot start now is skipped, so
    // one unaffordable or unavailable building never stalls everything after it.
    if(h->BuildStructure==STRUCT_NONE && !BuildingQueue(h)) {
        // Refineries come on a game-time schedule, later on the easier levels and up
        // to the level's expansion limit. Each one also delivers a harvester. Hard
        // and Expert want their second refinery from the start.
        static const int second_ref[4]={6,4,0,0}, third_ref[4]={99,10,8,6}, fourth_ref[4]={99,99,14,11};
        int late=3-level;
        int want_ref=1+(minute>=second_ref[level])+(minute>=third_ref[level])+(minute>=fourth_ref[level]);
        if(want_ref>p.expansion_refineries) want_ref=p.expansion_refineries;
        int defences=Planned(h,STRUCT_GTOWER)+Planned(h,STRUCT_TURRET)+Planned(h,STRUCT_OBELISK)+Planned(h,STRUCT_ATOWER);
        int want_def=1+level/2+(minute>=15 ? level/2:0);
        bool radar=Planned(h,STRUCT_RADAR)>0;
        bool top=Planned(h,STRUCT_EYE)>0 || Planned(h,STRUCT_TEMPLE)>0;
        // Storage: one refinery holds 1000 and a silo 1500. Tiberium past the limit
        // is lost, so a silo goes up once the stores are two thirds full.
        bool full=h->Capacity>0 && (h->Capacity-h->Tiberium<300 || h->Tiberium*3>=h->Capacity*2);
        bool done=false;
        if(!power || h->Power<h->Drain+20) { done=Build(h,STRUCT_ADVANCED_POWER) || Build(h,STRUCT_POWER,true); }
        if(!done && !Planned(h,STRUCT_REFINERY)) done=Build(h,STRUCT_REFINERY,true);
        if(!done && !Planned(h,STRUCT_BARRACKS) && !Planned(h,STRUCT_HAND)) done=Build(h,STRUCT_BARRACKS) || Build(h,STRUCT_HAND,true);
        // Hard and Expert open with their second refinery before the war factory:
        // one harvester alone earns too little to pay for the rest of the base.
        bool expand=Planned(h,STRUCT_REFINERY)<want_ref;
        if(!done && expand && level>=2 && Planned(h,STRUCT_REFINERY)<2) done=Build(h,STRUCT_REFINERY);
        if(!done && !Planned(h,STRUCT_WEAP) && !Planned(h,STRUCT_AIRSTRIP)) done=Build(h,STRUCT_WEAP) || Build(h,STRUCT_AIRSTRIP,true);
        if(!done && full && Planned(h,STRUCT_STORAGE)<2+ref) done=Build(h,STRUCT_STORAGE);
        // The third and fourth refineries wait for the Comm Center.
        if(!done && expand && Planned(h,STRUCT_REFINERY)<2) done=Build(h,STRUCT_REFINERY);
        if(!done && s.air && !Planned(h,STRUCT_SAM) && h->ActLike==HOUSE_BAD) done=Build(h,STRUCT_SAM);
        if(!done && !radar) done=Build(h,STRUCT_RADAR);
        if(!done && expand && radar) done=Build(h,STRUCT_REFINERY);
        if(!done && radar && !Planned(h,STRUCT_REPAIR) && level>0) done=Build(h,STRUCT_REPAIR);
        // One defence, then the top of the tech tree and the helipad, then the rest
        // of the defences: the faction's strong one once the Comm Center allows it,
        // the cheaper tower or turret otherwise.
        for(int pass=0;pass<2 && !done;++pass) {
            if(defences<(pass ? want_def:1)) {
                if(level>0) done=Build(h,STRUCT_OBELISK) || Build(h,STRUCT_ATOWER);
                if(!done) done=Build(h,STRUCT_GTOWER) || Build(h,STRUCT_TURRET);
            }
            if(pass) break;
            if(!done && !top && level>0 && minute>=6+3*late) done=Build(h,STRUCT_EYE) || Build(h,STRUCT_TEMPLE);
            if(!done && !Planned(h,STRUCT_HELIPAD) && level>0 && minute>=8+2*late) done=Build(h,STRUCT_HELIPAD);
        }
        if(!done && level==0 && radar && !Planned(h,STRUCT_REPAIR) && minute>=12) done=Build(h,STRUCT_REPAIR);
        if(!done && s.air && !Planned(h,STRUCT_ATOWER) && h->ActLike==HOUSE_GOOD) done=Build(h,STRUCT_ATOWER);
        if(!done && level>=2 && minute>=12 && ref>=2 && war<2 && Planned(h,STRUCT_WEAP)+Planned(h,STRUCT_AIRSTRIP)<2) done=Build(h,STRUCT_WEAP) || Build(h,STRUCT_AIRSTRIP);
    }
    bool critical=(!ref || !power || !war || !harv);
    int reserve=critical ? 0 : p.cash_reserve;
    // While the yard builds income (power, a refinery, a silo) new army orders wait
    // unless the house holds more than its reserve; otherwise all factories draw in
    // parallel, as a human's would.
    StructType building=InProduction(h);
    bool saving=building==STRUCT_REFINERY || building==STRUCT_STORAGE || building==STRUCT_POWER || building==STRUCT_ADVANCED_POWER;
    bool spend=critical || money>reserve || (income && !saving);
    // One harvester per refinery (each refinery brings its own); on Hard and Expert
    // half as many again once the army has a few vehicles. A lost one is replaced first.
    int veh=CombatVehicleCount(h), inf=InfantryCount(h);
    int want_harv=ref+(level>=2 && veh>=3 ? ref/2:0);
    if(h->BuildUnit==UNIT_NONE && ref && (harv<ref || (harv<want_harv && spend)) && h->Can_Build(UNIT_HARVESTER,h->ActLike)) {
        h->BuildUnit=UNIT_HARVESTER;
    } else if(h->BuildUnit==UNIT_NONE && war && h->CurUnits<h->MaxUnit && spend && money>10) {
        int score[UNIT_COUNT]; int total=0;
        for(int j=UNIT_FIRST;j<UNIT_COUNT;++j) {
            UnitType t=(UnitType)j; UnitTypeClass const& u=UnitTypeClass::As_Reference(t); score[j]=0;
            if(t==UNIT_HARVESTER || t==UNIT_MCV || u.Primary==WEAPON_NONE || !h->Can_Build(&u,h->ActLike)) continue;
            // Without income only what the house can pay for outright.
            if(!income && u.Cost_Of()>money-reserve) continue;
            int w=20; // Mixed armies on all modes, increasingly evidence-led counters.
            if(t==UNIT_MTANK || t==UNIT_LTANK || t==UNIT_HTANK || t==UNIT_STANK || t==UNIT_FTANK) w+=30+(s.armor ? p.counter_weight/2:0);
            if(t==UNIT_HTANK || t==UNIT_STANK) w+=10*level;
            if((t==UNIT_FTANK || t==UNIT_BUGGY || t==UNIT_JEEP) && s.infantry>s.armor) w+=p.counter_weight;
            if((t==UNIT_MLRS || t==UNIT_BIKE) && s.air) w+=p.counter_weight;
            if(t==UNIT_ARTY || t==UNIT_MSAM) w = level>=2 ? 18:5;
            if(t==UNIT_APC) w=10;
            w=w*4/(2+UnitCount(h,t)); score[j]=max(1,w); total+=score[j];
        }
        unsigned n=fair_ai::Random(s.rng,total); for(int j=UNIT_FIRST;total>0 && j<UNIT_COUNT;++j) { if(n<(unsigned)score[j]) { h->BuildUnit=(UnitType)j;break; } n-=score[j]; }
    }
    // Infantry fills in beside the vehicles rather than replacing them, but always
    // leaves room for a full attack squad above the home reserve. Only a house with
    // a war factory and no combat vehicle it may build (a low tech level) keeps the
    // plain infantry limit.
    bool vehicles_possible=!war;
    for(int j=UNIT_FIRST;war && j<UNIT_COUNT && !vehicles_possible;++j) { UnitType t=(UnitType)j; UnitTypeClass const& u=UnitTypeClass::As_Reference(t);
        vehicles_possible=t!=UNIT_HARVESTER && t!=UNIT_MCV && u.Primary!=WEAPON_NONE && h->Can_Build(&u,h->ActLike); }
    int inf_cap=8+4*ref+4*level;
    if(vehicles_possible) inf_cap=min(inf_cap,max(6+2*veh+2*ref,p.squad_min+p.squad_min/4+1));
    // A handful of riflemen guard the base even while the yard saves for income.
    if(h->BuildInfantry==INFANTRY_NONE && barr && harv && (money>reserve+150 || (income && (!saving || inf<4))) && inf<inf_cap) {
        InfantryType t=INFANTRY_E1;
        unsigned pick=fair_ai::Random(s.rng,100);
        if((s.armor || s.air) && pick<(unsigned)p.counter_weight) t=INFANTRY_E3;
        else if(pick<25) t=INFANTRY_E2;
        InfantryTypeClass const& i=InfantryTypeClass::As_Reference(t);
        if(!h->Can_Build(&i,h->ActLike)) t=INFANTRY_E1;
        if(h->Can_Build(t,h->ActLike)) h->BuildInfantry=t;
    }
    if(h->BuildAircraft==AIRCRAFT_NONE && level>0 && harv && (money>reserve+1400 || (income && !saving && money>reserve)) && h->CurAircraft<(unsigned)BuildingCount(h,STRUCT_HELIPAD)) {
        AircraftType t=h->ActLike==HOUSE_GOOD ? AIRCRAFT_ORCA:AIRCRAFT_HELICOPTER;
        if(h->Can_Build(t,h->ActLike)) h->BuildAircraft=t;
    }
    // Normal repair consumes normal money and observes the same strength cap.
    int repaired=0;
    for(int i=0;i<Buildings.Count() && repaired<1+level;++i) { BuildingClass* b=Buildings.Ptr(i);
        if(Friendly(h,b) && b->Strength<b->Class->MaxStrength && money>reserve+200 && b->Mission!=MISSION_DECONSTRUCTION) { b->Repair(1); ++repaired; } }
}
int Army(HouseClass* h,FootClass** army,int maxn) {
    int n=0;
    for(int i=0;i<Units.Count() && n<maxn;++i) { UnitClass* u=Units.Ptr(i); if(Friendly(h,u) && u->Is_Weapon_Equipped() && u->Class->Type!=UNIT_HARVESTER && u->Class->Type!=UNIT_MCV && !u->Team && !u->IsTethered) army[n++]=u; }
    for(int i=0;i<Infantry.Count() && n<maxn;++i) { InfantryClass* u=Infantry.Ptr(i); if(Friendly(h,u) && u->Is_Weapon_Equipped() && !u->Team && !u->IsTethered) army[n++]=u; }
    return n;
}
void MoveArea(FootClass* u,CELL c) {
    u->Assign_Target(TARGET_NONE);u->ArchiveTarget=::As_Target(c);
    u->Assign_Mission(MISSION_GUARD_AREA);u->Assign_Destination(::As_Target(c));
}
CELL Frontier(HouseClass* h,TDPolicyState& s,CELL from) {
    // Choose unexplored destinations using explored topology and map dimensions
    // only. Pathing learns obstructions through ordinary movement.
    CELL best=0; int score=-0x7fffffff;
    // Candidate start locations belong to the static map, not enemy live
    // houses. Scout the possible starts before sweeping unexplored terrain.
    for(int k=0;k<8;++k) {
        CELL c=Scen.Waypoint[k];
        if(c<=0 || !Map.In_Radar(c) || Map[c].Is_Mapped(h)) continue;
        bool assigned=false;for(int z=0;z<2;++z) if(s.scout_goal[z]==(unsigned)c) assigned=true;
        if(assigned) continue;
        int value=-CellDistance(from,c);if(value>score) { score=value;best=c; }
    }
    if(best) return best;
    score=-0x7fffffff;
    int w=Map.MapCellWidth, height=Map.MapCellHeight, total=w*height;
    if(total<=0) return 0;
    for(int k=0;k<total;++k) { int idx=(k+(int)s.cursor)%total;
        CELL c=XY_Cell(Map.MapCellX+idx%w,Map.MapCellY+idx/w);
        if(Map[c].Is_Mapped(h)) continue;
        int d=CellDistance(from,c); int merit=-d;
        if(merit>score) { score=merit;best=c; }
    }
    s.cursor=(s.cursor+137u)%(unsigned)total; return best;
}
void Tactics(HouseClass* h) {
    TDPolicyState& s=Policy(h); fair_ai::Profile const& p=fair_ai::Get(s.level);
    FootClass* army[512]; int count=Army(h,army,512), orders=0; CELL home=Home(h), rally=Rally(h);
    // Clear stale scouts by owned live identity, never by an enemy reference.
    for(int z=0;z<2;++z) if(s.scout[z]) { bool exists=false; for(int i=0;i<count;++i) if(army[i]->As_Target()==s.scout[z]) exists=true; if(!exists) s.scout[z]=s.scout_goal[z]=0; }
    if((uint32_t)Frame>=s.next_scout) {
        s.next_scout=Frame+p.scout_ticks;
        for(int z=0;z<p.scouts;++z) {
            FootClass* scout=NULL;
            for(int i=0;i<count;++i) if(army[i]->As_Target()==s.scout[z]) scout=army[i];
            if(!scout) {
                int cheapest=0x7fffffff;
                for(int i=0;i<count;++i) { bool used=false;for(int j=0;j<2;++j) if(army[i]->As_Target()==s.scout[j]) used=true;
                    int cost=army[i]->Techno_Type_Class()->Cost_Of(); if(!used && cost<cheapest && army[i]->Mission!=MISSION_ATTACK) { cheapest=cost;scout=army[i]; } }
            }
            if(scout && orders<p.max_orders) {
                CELL position=Coord_Cell(scout->Center_Coord());
                CELL previous=(CELL)s.scout_position[z];s.scout_position[z]=position;
                CELL current=(CELL)s.scout_goal[z];
                if(current && !Map[current].Is_Mapped(h) && Target_Legal(scout->NavCom)
                    && position!=previous && CellDistance(position,current)>2) continue;
                CELL goal=Frontier(h,s,position);
                if(goal) { s.scout[z]=scout->As_Target();s.scout_goal[z]=goal;scout->Assign_Target(TARGET_NONE);scout->Assign_Mission(MISSION_MOVE);scout->Assign_Destination(::As_Target(goal));++s.scouting;++orders; }
            }
        }
    }
    // The home reserve is the cheapest share of the army, so the tanks go out with
    // the wave and the riflemen stay. Ties keep army order (a stable insertion
    // sort), which every lockstep peer computes identically.
    int reserve=max(1,count*p.reserve_percent/100);
    int rank_order[512];
    for(int i=0;i<count;++i) {
        int cost=army[i]->Techno_Type_Class()->Cost_Of(), j=i;
        while(j>0 && army[rank_order[j-1]]->Techno_Type_Class()->Cost_Of()>cost) { rank_order[j]=rank_order[j-1];--j; }
        rank_order[j]=i;
    }
    int rank_of[512];
    for(int r=0;r<count;++r) rank_of[rank_order[r]]=r;
    bool launch=(uint32_t)Frame>=s.next_attack && count-reserve>=p.squad_min;
    if(launch) {
        s.next_attack=Frame+p.attack_ticks;
        for(int z=0;z<3;++z) s.objective[z]=0;
        int n=0;
        // Select objectives from currently observed buildings. Distinct groups
        // pressure separate objectives while the cheapest forces remain home.
        for(int z=0;z<p.attack_groups;++z) {
            int best=-0x7fffffff; CELL chosen=0;
            for(int i=0;i<Buildings.Count();++i) { BuildingClass* b=Buildings.Ptr(i);if(!TD_Fair_AI_Knows(h,b)) continue;
                CELL c=Coord_Cell(b->Center_Coord());bool used=false;for(int j=0;j<z;++j) if(s.objective[j]==(unsigned)c) used=true;
                if(used) continue; int value=b->Class->Cost_Of()/20-CellDistance(home,c);
                if(s.level>=2 && (b->Class->Type==STRUCT_REFINERY || b->Class->Type==STRUCT_WEAP || b->Class->Type==STRUCT_AIRSTRIP)) value+=100;
                if(value>best) { best=value;chosen=c; }
            }
            if(!chosen) {
                // Finish mobile survivors and punish exposed harvesters even
                // when the opponent has never deployed a construction yard.
                for(int i=0;i<Units.Count();++i) { UnitClass* u=Units.Ptr(i);if(!TD_Fair_AI_Knows(h,u)) continue;
                    CELL c=Coord_Cell(u->Center_Coord());bool used=false;for(int j=0;j<z;++j) if(s.objective[j]==(unsigned)c) used=true;
                    if(used) continue;int value=u->Class->Cost_Of()/20-CellDistance(home,c);
                    if(value>best) { best=value;chosen=c; }
                }
                for(int i=0;i<Infantry.Count() && !chosen;++i) { InfantryClass* u=Infantry.Ptr(i);if(!TD_Fair_AI_Knows(h,u)) continue;
                    CELL c=Coord_Cell(u->Center_Coord());bool used=false;for(int j=0;j<z;++j) if(s.objective[j]==(unsigned)c) used=true;
                    if(!used) chosen=c;
                }
            }
            if(chosen) { s.objective[z]=chosen;++n; }
        }
        if(!n) { s.objective[0]=Frontier(h,s,home);n=s.objective[0]?1:0; }
        s.attacking=n;
        if(n) ++s.attacks;
    }
    // A wave is sent together. Units that become free after its window closes gather
    // at the rally point for the next wave instead of walking to the old objective one
    // by one. The window opens at launch, which is next_attack less one interval.
    uint32_t window=(uint32_t)max(600,p.attack_ticks/2);
    bool wave=s.attacking && (uint32_t)Frame+(uint32_t)p.attack_ticks<s.next_attack+window;
    for(int q=0;q<count && orders<p.max_orders;++q) {
        // Rotate the command budget through the full army. Large armies get
        // bounded attention rather than starving high-index units.
        int i=(q+(int)(s.orders%max(1,count)))%count; FootClass* u=army[i];
        bool scout=false;for(int z=0;z<2;++z) if(u->As_Target()==s.scout[z]) scout=true;
        if(scout) continue;
        bool hurt=u->Strength*100 < u->Techno_Type_Class()->MaxStrength*p.retreat_percent;
        if(hurt && s.level>0 && CellDistance(rally,Coord_Cell(u->Center_Coord()))>4) { MoveArea(u,rally);++orders;continue; }
        TechnoClass* threat=NULL;int nearest=0x7fffffff;
        // Defenders react to observed threats to the economy, using a bounded
        // range. Attacking units focus their nearest visible local opponent.
        for(int j=0;j<Units.Count();++j) { UnitClass* e=Units.Ptr(j);if(!TD_Fair_AI_Knows(h,e)) continue;
            int d=CellDistance(Coord_Cell(u->Center_Coord()),Coord_Cell(e->Center_Coord()));
            if(d<nearest && d<=6 && (rank_of[i]>=reserve || CellDistance(home,Coord_Cell(e->Center_Coord()))<12)) { nearest=d;threat=e; } }
        if(threat && s.level>0 && u->TarCom!=threat->As_Target()) { u->Assign_Mission(MISSION_ATTACK);u->Assign_Target(threat->As_Target());++orders;continue; }
        if(!hurt && wave && rank_of[i]>=reserve) {
            CELL goal=(CELL)s.objective[(rank_of[i]-reserve)%s.attacking];
            if(goal && u->ArchiveTarget!=::As_Target(goal) && !Target_Legal(u->TarCom)) { MoveArea(u,goal);++orders; }
        } else if(!Target_Legal(u->NavCom) && !Target_Legal(u->TarCom) && CellDistance(rally,Coord_Cell(u->Center_Coord()))>4) { MoveArea(u,rally);++orders; }
    }
    // Aircraft use known targets and ordinary ammunition/landing missions.
    for(int i=0;i<Aircraft.Count() && orders<p.max_orders;++i) { AircraftClass* a=Aircraft.Ptr(i); if(!Friendly(h,a) || !a->Is_Weapon_Equipped() || a->Ammo<=0 || Target_Legal(a->TarCom) || a->Mission==MISSION_ENTER) continue;
        TechnoClass* target=NULL;
        for(int j=0;j<Buildings.Count() && !target;++j) if(TD_Fair_AI_Knows(h,Buildings.Ptr(j))) target=Buildings.Ptr(j);
        for(int j=0;j<Units.Count() && !target;++j) if(TD_Fair_AI_Knows(h,Units.Ptr(j))) target=Units.Ptr(j);
        if(target) { a->Assign_Mission(MISSION_ATTACK);a->Assign_Target(target->As_Target());++orders; }
    }
    s.orders+=orders;
}
}
bool TD_Fair_AI_Knows(HouseClass const* h,TechnoClass const* o) {
    if(!h || !Live(o) || h->Is_Ally(o) || o->Is_Cloaked(h)) return false;
    // On a Rally course the hostile house sees every racer that is not cloaked, fog or not.
    const int rally_knows=Rally_Knows(h,o); if(rally_knows>=0) return rally_knows!=0;
    CELL c=Coord_Cell(o->Center_Coord());return c>=0 && c<MAP_CELL_TOTAL && Map[c].Is_Mapped(h->Class->House);
}
// Ground pieces of at least this many cells count as places a unit can be; smaller
// pockets between a building and a rock are not worth refusing a placement over.
static const int KEEP_PATHS_MIN_PIECE=3;
// Counts the separate pieces of ground a harvester could drive over, with an optional
// footprint treated as built over. Cells join only through their four edges, so a gap
// that is passable only corner to corner counts as closed. The ground's land type is the
// map itself, which the engine's own path finding reads for every player alike; a
// building or tree blocks only on a cell this house has explored, so an enemy base
// hidden under the shroud never steers where the computer builds.
static int Ground_Pieces(HouseClass const* h, int speed, short const* footprint, CELL origin) {
    HousesType house=h->Class->House;
    static unsigned char blocked[MAP_CELL_TOTAL];
    static CELL queue[MAP_CELL_TOTAL];
    for(int y=Map.MapCellY;y<Map.MapCellY+Map.MapCellHeight;++y)
        for(int x=Map.MapCellX;x<Map.MapCellX+Map.MapCellWidth;++x) {
            CELL c=XY_Cell(x,y); CellClass& cell=Map[c];
            blocked[c]=!::Ground[cell.Land_Type()].Cost[speed] || ((cell.Cell_Building() || cell.Cell_Terrain()) && cell.Is_Mapped(house));
        }
    if(footprint) for(short const* o=footprint;*o!=REFRESH_EOL;++o) { int c=origin+*o; if(c>=0 && c<MAP_CELL_TOTAL) blocked[c]=1; }
    int pieces=0;
    for(int y=Map.MapCellY;y<Map.MapCellY+Map.MapCellHeight;++y)
        for(int x=Map.MapCellX;x<Map.MapCellX+Map.MapCellWidth;++x) {
            CELL start=XY_Cell(x,y); if(blocked[start]) continue;
            int head=0,tail=0,size=0; queue[tail++]=start; blocked[start]=1;
            while(head<tail) { CELL c=queue[head++]; ++size; int cx=Cell_X(c), cy=Cell_Y(c);
                static const int step[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
                for(int k=0;k<4;++k) { int nx=cx+step[k][0], ny=cy+step[k][1];
                    if(nx<Map.MapCellX || ny<Map.MapCellY || nx>=Map.MapCellX+Map.MapCellWidth || ny>=Map.MapCellY+Map.MapCellHeight) continue;
                    CELL n=XY_Cell(nx,ny); if(!blocked[n]) { blocked[n]=1; queue[tail++]=n; } } }
            if(size>=KEEP_PATHS_MIN_PIECE) ++pieces;
        }
    return pieces;
}
// The engine's placement search for a computer house accepts any clear spot next to the
// base, including the one ramp out of a plateau; a base that has built over its own exit
// can no longer send a harvester to the Tiberium. A placement is kept only when it does
// not split the ground into more pieces than there were before.
bool TD_Fair_AI_Keeps_Paths(HouseClass const* h, BuildingTypeClass const* b, int origin) {
    if(!h || !b || origin<0 || origin>=MAP_CELL_TOTAL) return true;
    if(Map.MapCellWidth<=0 || Map.MapCellHeight<=0) return true;
    int speed=UnitTypeClass::As_Reference(UNIT_HARVESTER).Speed;
    return Ground_Pieces(h,speed,b->Occupy_List(false),(CELL)origin)<=Ground_Pieces(h,speed,NULL,0);
}
int TD_Fair_AI_Level(HouseClass const* h) { return h ? (int)td_policy[(int)h->Class->House].level : fair_ai::Normal; }
void TD_Fair_AI_Reset() {
    memset(td_policy,0,sizeof(td_policy));
    for(int i=0;i<HOUSE_COUNT;++i) { td_policy[i].level=1;td_policy[i].rng=0x51f15e5du ^ (uint32_t)(i+1)*2654435761u; }
}
void TD_Fair_AI_Set_Levels(unsigned char const* levels,int count) { for(int i=0;i<fair_ai::Seats;++i) td_pending[i]=(levels && i<count)?fair_ai::Clamp(levels[i]):fair_ai::Normal; }
void TD_Fair_AI_Assign(HouseClass* h,int seat) { if(h) {
    TDPolicyState& s=Policy(h);memset(&s,0,sizeof(s));
    s.rng=0x51f15e5du ^ (uint32_t)(h->Class->House+1)*2654435761u;
    s.level=seat>=0 && seat<fair_ai::Seats?td_pending[seat]:fair_ai::Normal;
} }
void TD_Fair_AI_Tick(HouseClass* h) {
    if (Film_Isolation_Enabled()) return;
    if(!h || !h->IsActive || h->IsHuman || GameToPlay==GAME_NORMAL || h->IsDefeated
        || h->Class->House<HOUSE_MULTI1 || h->Class->House>HOUSE_MULTI8) return;
    TDPolicyState& s=Policy(h); fair_ai::Profile const& p=fair_ai::Get(s.level);
    // Restore the legal faction statistics, independently from difficulty INIs.
    // Assignment itself preserves faction identity; no skill changes statistics.
    if((uint32_t)Frame>=s.next_think) { s.next_think=Frame+p.think_ticks;++s.thoughts;h->Recalc_Center();OwnSight(h);Observe(h);PlaceMCV(h);Economy(h); }
    // A Gun Game combatant takes its orders from the Gun Game's own computer player.
    if((uint32_t)Frame>=s.next_tactic && !Gun_Game_Active()) { s.next_tactic=Frame+p.tactical_ticks;Tactics(h); }
}
unsigned int TD_Fair_AI_Hash() {
    uint32_t hash=fair_ai::PolicyHash();
    for(int i=0;i<HOUSE_COUNT;++i) { uint32_t const* words=(uint32_t const*)&td_policy[i];for(unsigned j=0;j<sizeof(TDPolicyState)/4;++j) hash=fair_ai::HashWord(hash,words[j]); } return hash;
}
bool TD_Fair_AI_Save(FileClass& file) {
    uint32_t header[4]={0x54444149u,3u,sizeof(td_policy)+4u*sizeof(uint32_t),fair_ai::PolicyHash()};
    uint32_t random[4]={Scen.RandomNumber.Seed,NonCriticalRandomNumber.Seed,RandNumb,(uint32_t)Seed};
    return file.Write(header,sizeof(header))==sizeof(header) && file.Write(td_policy,sizeof(td_policy))==sizeof(td_policy)
        && file.Write(random,sizeof(random))==sizeof(random);
}
bool TD_Fair_AI_Load(FileClass& file) {
    TD_Fair_AI_Reset(); uint32_t header[4];
    int got=file.Read(header,sizeof(header));
    if(got==0) return true; // Old saves end here and start at Normal.
    if(got!=sizeof(header) || header[0]!=0x54444149u || header[1]!=3u || header[2]!=sizeof(td_policy)+4u*sizeof(uint32_t) || header[3]!=fair_ai::PolicyHash()) return false;
    TDPolicyState state[HOUSE_COUNT];
    if(file.Read(state,sizeof(state))!=sizeof(state)) return false;
    for(int i=0;i<HOUSE_COUNT;++i) {
        if(state[i].level>3 || state[i].attacking>3 || state[i].cursor>=(uint32_t)MAP_CELL_TOTAL) return false;
        for(int j=0;j<2;++j) if(state[i].scout_goal[j]>=(uint32_t)MAP_CELL_TOTAL || state[i].scout_position[j]>=(uint32_t)MAP_CELL_TOTAL) return false;
        for(int j=0;j<3;++j) if(state[i].objective[j]>=(uint32_t)MAP_CELL_TOTAL) return false;
        // Scout handles are compared against owned live army IDs on the next
        // tactic tick; they are never dereferenced from this external state.
    }
    uint32_t random[4];if(file.Read(random,sizeof(random))!=sizeof(random)) return false;
    memcpy(td_policy,state,sizeof(state));
    Scen.RandomNumber.Seed=random[0];NonCriticalRandomNumber.Seed=random[1];RandNumb=random[2];Seed=(int)random[3];
    return true;
}
int TD_Fair_AI_Diagnostics(char* out,int size) {
    if(!out || size<=0) return 0;int used=0;out[0]=0;
    for(int i=HOUSE_MULTI1;i<HOUSE_COUNT && used<size-1;++i) { HouseClass* h=HouseClass::As_Pointer((HousesType)i);if(!h || !h->IsActive) continue;
        int n=snprintf(out+used,size-used,"FAIRRULE|game=TD|house=%d|act=%d|human=%d|difficulty=%d|fire=%d|ground=%d|air=%d|armor=%d|rof=%d|cost=%d|build=%d|harvest=%d\n",i,(int)h->ActLike,(int)h->IsHuman,(int)h->Difficulty,(int)(h->FirepowerBias*1000),(int)(h->GroundspeedBias*1000),(int)(h->AirspeedBias*1000),(int)(h->ArmorBias*1000),(int)(h->ROFBias*1000),(int)(h->CostBias*1000),(int)(h->BuildSpeedBias*1000),UnitTypeClass::FULL_LOAD_CREDITS);
        if(n<0) break;used+=min(n,size-used-1);
    }
    for(int i=0;i<HOUSE_COUNT && used<size-1;++i) { HouseClass* h=HouseClass::As_Pointer((HousesType)i); if(!h || !h->IsActive || h->IsHuman || i<HOUSE_MULTI1) continue;
        TDPolicyState const& s=td_policy[i];int n=snprintf(out+used,size-used,"FAIRAI|game=TD|house=%d|level=%u|name=%s|think=%u|orders=%u|scouts=%u|attacks=%u|known_cells=%u|contacts=%u|harvested=%u|hash=%08x\n",i,s.level,fair_ai::Name(s.level),s.thoughts,s.orders,s.scouting,s.attacks,s.known_cells,s.contacts,h->HarvestedCredits,TD_Fair_AI_Hash());
        if(n<0) break;used+=min(n,size-used-1);
    } return used;
}
