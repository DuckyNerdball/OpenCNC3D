#ifndef BUILDING_DESTRUCTION_BUDGET_H
#define BUILDING_DESTRUCTION_BUDGET_H
#include <cstdint>
#include <cstdio>
/* Two full building collapses in the local tactical viewport, including their
   visible settling/fade lifetime. Callers retire families that leave the view.
   Tickets survive recycled engine IDs and orphaned renderer entries. Vehicles,
   particles and scorch marks do not consume slots. */
static std::uint64_t g_destructionSlots[2]={0,0},g_destructionNext=0;
static unsigned g_destructionAdmitted=0,g_destructionEffectsOnly=0,g_destructionReleased=0,g_destructionHigh=0;
static unsigned destruction_budget_active() {
    return (g_destructionSlots[0]!=0)+(g_destructionSlots[1]!=0);
}
static void destruction_budget_effects_only(long long key,const char* owner,int frame,const char* reason) {
    g_destructionEffectsOnly++;
    printf("DESTRUCTIONCAP|effects-only|key=%lld|owner=%s|frame=%d|active=%u|limit=2|reason=%s\n",key,owner,frame,destruction_budget_active(),reason);
}
static std::uint64_t destruction_budget_begin(long long key,const char* owner,int frame) {
    for(unsigned i=0;i<2;i++)if(!g_destructionSlots[i]) {
        if(!++g_destructionNext)++g_destructionNext;
        g_destructionSlots[i]=g_destructionNext;g_destructionAdmitted++;
        const unsigned active=destruction_budget_active();if(active>g_destructionHigh)g_destructionHigh=active;
        printf("DESTRUCTIONCAP|admit|key=%lld|owner=%s|frame=%d|ticket=%llu|active=%u|limit=2\n",key,owner,frame,(unsigned long long)g_destructionNext,active);
        return g_destructionNext;
    }
    destruction_budget_effects_only(key,owner,frame,"capacity");
    return 0;
}
static void destruction_budget_end(std::uint64_t ticket) {
    if(!ticket)return;
    for(unsigned i=0;i<2;i++)if(g_destructionSlots[i]==ticket) {
        g_destructionSlots[i]=0;g_destructionReleased++;
        printf("DESTRUCTIONCAP|release|ticket=%llu|active=%u|limit=2\n",(unsigned long long)ticket,destruction_budget_active());return;
    }
}
static void destruction_budget_reset() {
    g_destructionSlots[0]=g_destructionSlots[1]=0; /* keep ticket generations distinct */
    g_destructionAdmitted=g_destructionEffectsOnly=g_destructionReleased=g_destructionHigh=0;
}
static void destruction_budget_dump() {
    printf("DESTRUCTIONCAP|state|active=%u|limit=2|admitted=%u|effects_only=%u|released=%u|high_water=%u\n",destruction_budget_active(),g_destructionAdmitted,g_destructionEffectsOnly,g_destructionReleased,g_destructionHigh);
}
#endif
