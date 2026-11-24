#include "../../../app/campaign_progress.h"
#include <cassert>
#include <cstdio>
int main() {
    assert(camp_map_row(0,0,6,11)==7);
    for (int ra=0;ra<2;++ra) for(int side=0;side<2;++side)
        for(int mission=1;mission<=15;++mission) for(int type=-1;type<32;++type) {
            const int got=camp_map_row(ra,side,mission,type);
            assert(got==mission || (!ra && !side && mission==6 && type==11 && got==7));
        }
    assert(camp_map_row(0,0,6,-1)+1==7);
    assert(camp_map_row(0,0,6,11)+1==8);
    assert(camp_map_row(1,0,6,11)+1==7);
    puts("CAMPAIGN-PROGRESSION|PASS|original airstrip branch; ordinary route; both sides and games");
}
