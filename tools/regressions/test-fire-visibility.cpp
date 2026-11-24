/* State invariants; native shot/expiry/save checks exercise the engine integration. */
#include "../../brain/vanilla/common/fire_visibility.h"
#include <cassert>
#include <cstdio>
CncFireVisibility g_cncFireVisibility;
int main() {
    CncFireVisibility& s=g_cncFireVisibility;
    s.nextTemporary=true;
    { CncFireSightScope firing;
      s.touch(100,1,false,false,10);s.touch(100,1,true,false,11);
      assert(s.cells.at({100,1}).until==56&&!s.cells.at({100,1}).mapped);
      s.touch(101,1,true,true,11);assert(s.cells.size()==1); // explored terrain stays permanent
      {CncFireSightScope normal; s.touch(100,1,true,true,12);}
      assert(s.cells.empty()&&s.temporary); // nested normal scouting wins
      s.touch(100,1,false,false,20);s.touch(100,2,false,true,22);
    }
    assert(!s.temporary&&s.cells.size()==2);
    s.restoring=true;s.touch(100,1,true,true,23);s.restoring=false;
    assert(s.cells.size()==2); // save/expiry restoration cannot promote itself
    s.nextTemporary=true;
    {CncFireSightScope firing;s.touch(100,1,true,true,30);}
    assert(s.cells.at({100,1}).until==75&&!s.cells.at({100,1}).mapped);
    s.touch(100,1,true,true,31);assert(s.cells.size()==1&&s.cells.count({100,2}));
    s.clear();assert(s.cells.empty()&&!s.temporary&&!s.nextTemporary&&!s.restoring);
    puts("FIREVIS-STATE|PASS|original flags, repeated shots, normal/nested scouting, independent houses, save restoration, mission reset");
}
