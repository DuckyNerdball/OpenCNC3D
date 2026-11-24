/* ---------------------------------------------------------------------------------- *
 *  WHICH ART A MISSION START LOADS, and the two switches that put the old loads back.
 *
 *  Two kinds of art used to go onto the card at every start, whatever could draw it:
 *
 *  - THE RETIRED EXPERIMENTAL ART: the Unity explosions (unityvfx_mod.h) and the
 *    textures of the experimental 3D models (model3d_mod.h, the M3| set in models3d/).
 *    Both are drawn only under experimental_textures, and nothing switches that on:
 *    every settings load and every Visuals apply set it to 0, no panel row or script
 *    dial names it, and no command-line switch sets it. The Unity explosions are now
 *    loaded at their first use (uvfx_ensure), which in practice is one of the picker's
 *    script verbs. The experimental models still read their bones, clips and meshes at
 *    the start, because those step every soldier whenever the remastered soldiers are
 *    off (Classic and N64 by default); only their texture pixels are let go, and read
 *    again from the model's file by the first draw that needs them (m3_upload).
 *
 *  - THE THEATER ART: the mottle patches (mottle_mod.h) and the remastered crater and
 *    scorch sheets (decalenh_mod.h). Each is put on the card only when the mission's
 *    theater can draw it; the test that decides is always wider than the one that draws,
 *    and it does not look at the graphics mode, because Enhanced can be switched on in
 *    the middle of a mission. A new mission is a new start (game_shutdown, then
 *    game_boot), so it loads what its own theater draws. The editor loads every theater's
 *    art, as before: a map's theater can change under it.
 *
 *  OPENCNC3D_RETIRED_ART=1 loads the retired art at the start again, and
 *  OPENCNC3D_THEATER_ART_ALL=1 loads every theater's art again. Each is read once. They
 *  exist so the old way and the new can be compared picture for picture; neither changes
 *  what is drawn. Every start says which way it went in BOOTART| lines.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_BOOT_ART_H
#define OPENCNC3D_BOOT_ART_H

static bool boot_art_switch(const char* name, int* cache)
{
    if (*cache < 0) {
        const char* v = getenv(name);
        *cache = (v && *v && *v != '0') ? 1 : 0;
        if (*cache) {
            printf("BOOTART|switch|%s=1\n", name);
            fflush(stdout);
        }
    }
    return *cache != 0;
}

/* The Unity explosions and the experimental models' textures loaded at the start. */
static bool boot_art_retired_at_start(void)
{
    static int on = -1;
    return boot_art_switch("OPENCNC3D_RETIRED_ART", &on);
}

/* Every theater's mottle and decal sheets loaded, whatever the mission's theater. */
static bool boot_art_all_theaters(void)
{
    static int on = -1;
    return boot_art_switch("OPENCNC3D_THEATER_ART_ALL", &on);
}

#endif /* OPENCNC3D_BOOT_ART_H */
