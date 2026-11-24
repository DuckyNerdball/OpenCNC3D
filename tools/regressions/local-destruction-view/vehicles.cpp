// Vehicle and building ownership must remain independent across reused IDs and resets.
#include "../../../game/vehicle_destruction_budget.h"
#include "../../../game/building_destruction_budget.h"
#include <cassert>
int main() {
    vehicle_budget_reset();destruction_budget_reset();
    auto building=destruction_budget_begin(7,"ebld",1);
    auto a=vehicle_budget_begin(7,"evhc",1),b=vehicle_budget_begin(7,"shatter",1);
    assert(a&&b&&a!=b&&vehicle_budget_active()==2&&destruction_budget_active()==1);
    assert(!vehicle_budget_begin(8,"evhc",1));
    vehicle_budget_end(a);vehicle_budget_end(a);
    assert(vehicle_budget_active()==1&&destruction_budget_active()==1);
    auto c=vehicle_budget_begin(7,"evhc",2);
    assert(c&&c!=a&&c!=b&&vehicle_budget_active()==2);
    destruction_budget_end(building);assert(vehicle_budget_active()==2);
    vehicle_budget_reset();auto d=vehicle_budget_begin(7,"evhc",1);
    vehicle_budget_end(a);vehicle_budget_end(b);vehicle_budget_end(c);
    assert(d&&d!=a&&d!=b&&d!=c&&vehicle_budget_active()==1);
    for(int i=0;i<100;++i){auto next=vehicle_budget_begin(7,"shatter",i);assert(next&&next!=d&&vehicle_budget_active()==2);vehicle_budget_end(next);vehicle_budget_end(next);assert(vehicle_budget_active()==1);}
    vehicle_budget_end(d);assert(!vehicle_budget_active());vehicle_budget_dump();puts("PASS|vehicle/building ownership, reused IDs, repeated release and reset");
}
