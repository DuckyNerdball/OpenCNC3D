/* A real surrender followed by a campaign in the same engine instance. Reuse the
   established ABI loader and capture helpers without starting its lockstep main. */
#define main ra_lockstep_gate_main
#include "ra_twobrain.c"
#undef main

int main(int argc, char** argv)
{
    Brain warm = {0}, fresh = {0};
    size_t warm_size, fresh_size;
    if (argc != 3) {
        fprintf(stderr, "usage: %s RedAlert-library scenario-directory\n", argv[0]);
        return 2;
    }
    if (!load_brain(&warm, argv[1], RB_COPY_A, "warm")
        || !load_brain(&fresh, argv[1], RB_COPY_B, "fresh")) return 2;
    warm.AddModPath(argv[2]); fresh.AddModPath(argv[2]);
    warm.Init("-HEADLESS", noop_callback); fresh.Init("-HEADLESS", noop_callback);
    warm.SetSeed(4242); fresh.SetSeed(4242);
    if (!arm_match(&warm, 0) || !warm.Start(argv[2], argv[2], "SCM08EA", 10, 1)) return 3;
    for (int t = 0; t < 30; ++t) warm.Advance(1ULL);
    if (!warm.Blowup(1ULL)) return 4;
    for (int t = 0; t < 300; ++t) warm.Advance(1ULL);
    typedef unsigned char(CNC_CALL* CampaignStart)(int,int,int,int,const char*,const char*,const char*,int,const char*);
    CampaignStart start_warm = (CampaignStart)cnc_lib_sym(warm.lib, "CNC_Start_Instance_Variation");
    CampaignStart start_fresh = (CampaignStart)cnc_lib_sym(fresh.lib, "CNC_Start_Instance_Variation");
    if (!start_warm || !start_fresh
        || !start_warm(1,0,0,1,"GREECE","GAME_NORMAL",argv[2],0,"SCG01EA.INI")
        || !start_fresh(1,0,0,1,"GREECE","GAME_NORMAL",argv[2],0,"SCG01EA.INI")) return 5;
    for (int t = 0; t < 300; ++t) { warm.Advance(0ULL); fresh.Advance(0ULL); }
    char* a = capture_dump(&warm, &warm_size);
    char* b = capture_dump(&fresh, &fresh_size);
    if (!a || !b) return 6;
    int after = count_lines(a, "OBJ|", "|Greece|");
    int control = count_lines(b, "OBJ|", "|Greece|");
    free(a); free(b);
    printf("RA-SESSION|campaign=SCG01EA|ticks=300|after-surrender=%d|fresh=%d|%s\n",
           after, control, after > 0 && after == control ? "PASS" : "FAIL");
    return after > 0 && after == control ? 0 : 1;
}
