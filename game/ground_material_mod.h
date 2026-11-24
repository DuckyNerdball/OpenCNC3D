/* ====================================================================================
 *  ground_material_mod.h -- GROUND MATERIALS: the light pass's uniforms for the snow,
 *  wet shore, dry sand and grass shading (the shader text and its account are in
 *  ground_material_glsl.h). Included after the sea, so the shore field's range is known.
 *
 *  The theater decides which materials can exist at all; the shader decides, per pixel,
 *  how much of each a pixel is. With ground_mat_on at 0 the shader's branch is not taken
 *  and nothing else here reaches the picture.
 * ==================================================================================== */
#ifndef OPENCNC3D_GROUND_MATERIAL_MOD_H
#define OPENCNC3D_GROUND_MATERIAL_MOD_H

/* shoreBound: 1 when this frame's light pass bound the sea's shore field and its
   visibility mask (terrain_visibility_bind), which is when uTerrainWater means anything. */
static void ground_material_uniforms(GLuint prog, int shoreBound)
{
    const int on = g_fx.ground_mat_on && fx_unified_lighting();
    fx_set1i(prog, "uGmOn", on ? 1 : 0);
    if (!on) return;
    const char* th = g_pack.theater;
    /* Snow is Red Alert's snow theater (and the editor's SNOW); desert and its sand
       variant are sand; temperate and winter, in either game, are grass. Red Alert's
       interior matches none of them and keeps the plain ground light. */
    const float snow  = strstr(th, "SNOW") ? 1.0f : 0.0f;
    const float sand  = (!strcmp(th, "DESERT") || !strcmp(th, "SAND")) ? 1.0f : 0.0f;
    const float grass = (strstr(th, "TEMP") || strstr(th, "WINTER")) ? 1.0f : 0.0f;
    fx_set3f(prog, "uGmTheater", snow, sand, grass);
    const float strength[4] = { g_fx.ground_snow, g_fx.ground_wet, g_fx.ground_sand,
                                g_fx.ground_grass };
    fx_set4fv(prog, "uGmStrength", 1, strength);
    fx_set1f(prog, "uGmSparkle", g_fx.ground_sparkle);   /* the snow's glints; sand's ride ground_sand */
    /* The band's width from cells into the shore field's units: the field holds a signed
       distance of +-WATER_FIELD_RANGE cells across 0 to 1. Its inner edge is the swash's
       inland reach, the same edge the terrain grade stands back from.
       ONLY WHERE THE GROUND ITSELF MEETS THE WATER. On Tiberian Dawn's Enhanced temperate
       coast the painted cut puts the whole beach on the water side of the field: the
       sea's own pass draws the sand there, and the first ground inland is grass. A band
       measured from that coast would land on the grass, so there it is off, and the
       beach's wetness belongs to the sea's shading. */
    const float range2 = 2.0f * WATER_FIELD_RANGE;
    const float width = (g_fx.ground_wet_width > 0.05f ? g_fx.ground_wet_width : 0.05f) / range2;
    const int groundShore = shoreBound && (water_src_ra() || !water_painted_cut_td());
    const float dryEdge = 0.5f - (g_fx.water_foam_cross + 0.25f) / range2;
    const float wet[4] = { width, groundShore ? 1.0f : 0.0f, dryEdge, 0.15f / range2 };
    fx_set4fv(prog, "uGmWet", 1, wet);
    /* The sky's tint on snow in shade: a clear sky's blue, scaled to luminance 1 so it
       moves the hue of the shade and not its brightness. */
    const float c[3] = { 0.72f, 0.90f, 1.32f };
    const float y = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
    fx_set3f(prog, "uGmSnowSky", c[0] / y, c[1] / y, c[2] / y);
    /* A look at the masks themselves, for tuning: OPENCNC3D_GROUND_MAT_VIEW=1 paints snow
       blue, sand orange, grass green and the wet band magenta over a dimmed picture; =2
       paints the shore field; =3 paints the art's relief (red), the albedo's luminance
       (green) and the stone weight (blue). An instrument only, off unless asked for, and
       read once. */
    static int view = -1;
    if (view < 0) { const char* e = getenv("OPENCNC3D_GROUND_MAT_VIEW"); view = e ? atoi(e) : 0; }
    fx_set1i(prog, "uGmView", view);
}

#endif
