// Admission ownership must survive scene resets and recycled engine IDs.
#include "../../../game/building_destruction_budget.h"
#include <cassert>
int main() {
    destruction_budget_reset();
    auto a=destruction_budget_begin(7,"ebld",1);
    auto b=destruction_budget_begin(7,"shatter",1);
    assert(a && b && a!=b && destruction_budget_active()==2);
    assert(!destruction_budget_begin(8,"ebld",1));
    destruction_budget_end(a);
    destruction_budget_end(a); // duplicate completion cannot release another owner
    assert(destruction_budget_active()==1);
    auto c=destruction_budget_begin(7,"ebld",2); // recycled engine object ID
    assert(c && c!=a && c!=b && destruction_budget_active()==2);
    destruction_budget_reset();
    auto d=destruction_budget_begin(7,"ebld",1);
    destruction_budget_end(a);destruction_budget_end(b);destruction_budget_end(c);
    assert(d && d!=a && d!=b && d!=c && destruction_budget_active()==1);
    destruction_budget_end(d);
    assert(!destruction_budget_active());
    destruction_budget_dump();
}
