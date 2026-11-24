#ifndef VEHICLE_DESTRUCTION_BUDGET_H
#define VEHICLE_DESTRUCTION_BUDGET_H
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
/* Two vehicle/aircraft/vessel destruction families in the local tactical view,
   including their settling/fade lifetime. Enhanced wrecks and cartridge shatter
   share this budget. Building slots remain separate. Engine explosion particles,
   shock pulses and scorch marks run independently of admission.
   The launch-time switch retains the original path for native comparisons. */
static bool vehicle_budget_enabled() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_VEHICLE_DESTRUCTION_VIEW");return !p||strcmp(p,"0");}();return on;
}
static std::uint64_t g_vehicleDestructionSlots[2]={0,0},g_vehicleDestructionNext=0;
static unsigned g_vehicleDestructionAdmitted=0,g_vehicleDestructionEffectsOnly=0,g_vehicleDestructionReleased=0,g_vehicleDestructionHigh=0;
static unsigned vehicle_budget_active() {
    return (g_vehicleDestructionSlots[0]!=0)+(g_vehicleDestructionSlots[1]!=0);
}
static void vehicle_budget_effects_only(long long key,const char* owner,int frame,const char* reason) {
    g_vehicleDestructionEffectsOnly++;
    printf("VEHICLEDESTRUCTIONCAP|effects-only|key=%lld|owner=%s|frame=%d|active=%u|limit=2|reason=%s\n",key,owner,frame,vehicle_budget_active(),reason);
}
static std::uint64_t vehicle_budget_begin(long long key,const char* owner,int frame) {
    for(unsigned i=0;i<2;i++)if(!g_vehicleDestructionSlots[i]) {
        if(!++g_vehicleDestructionNext)++g_vehicleDestructionNext;
        g_vehicleDestructionSlots[i]=g_vehicleDestructionNext;g_vehicleDestructionAdmitted++;
        const unsigned active=vehicle_budget_active();if(active>g_vehicleDestructionHigh)g_vehicleDestructionHigh=active;
        printf("VEHICLEDESTRUCTIONCAP|admit|key=%lld|owner=%s|frame=%d|ticket=%llu|active=%u|limit=2\n",key,owner,frame,(unsigned long long)g_vehicleDestructionNext,active);
        return g_vehicleDestructionNext;
    }
    vehicle_budget_effects_only(key,owner,frame,"capacity");
    return 0;
}
static void vehicle_budget_end(std::uint64_t ticket) {
    if(!ticket)return;
    for(unsigned i=0;i<2;i++)if(g_vehicleDestructionSlots[i]==ticket) {
        g_vehicleDestructionSlots[i]=0;g_vehicleDestructionReleased++;
        printf("VEHICLEDESTRUCTIONCAP|release|ticket=%llu|active=%u|limit=2\n",(unsigned long long)ticket,vehicle_budget_active());return;
    }
}
static void vehicle_budget_reset() {
    g_vehicleDestructionSlots[0]=g_vehicleDestructionSlots[1]=0; /* keep ticket generations distinct */
    g_vehicleDestructionAdmitted=g_vehicleDestructionEffectsOnly=g_vehicleDestructionReleased=g_vehicleDestructionHigh=0;
}
static void vehicle_budget_dump() {
    printf("VEHICLEDESTRUCTIONCAP|state|active=%u|limit=2|admitted=%u|effects_only=%u|released=%u|high_water=%u\n",vehicle_budget_active(),g_vehicleDestructionAdmitted,g_vehicleDestructionEffectsOnly,g_vehicleDestructionReleased,g_vehicleDestructionHigh);
}
#endif
