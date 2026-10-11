/* ====================================================================================
 *  distfog_mod.h -- OpenCNC3D distance fog, Enhanced only.
 *
 *  WHAT IT DRAWS. The far part of the view sinks into a pale haze of the theater's own
 *  colour while the ground near the middle of the screen keeps its full colour and
 *  contrast. Under this tilted camera "far" is the top of the screen, so the picture
 *  gains the atmospheric depth of a later 3D RTS: a dusty warm haze over the desert, a
 *  cool damp one over temperate grass. Neither the cartridge nor the 1995 game draws
 *  anything like it; it is an addition, authored here.
 *
 *  WHERE IT IS DRAWN. One block at the end of the light pass (fx_post.h), which already
 *  has the world position behind every pixel. This file only decides the numbers that
 *  block is handed: whether the fog is live, the theater's thickness and colour, and the
 *  distance from the eye at which it begins.
 *
 *  THE DISTANCE IS MEASURED FROM THE GROUND THE CAMERA LOOKS AT, not from the eye. From
 *  this rig the nearest and the farthest ground on screen differ by only a few cells
 *  against a much longer eye distance, so a fog that starts at the eye washes the whole
 *  frame one even tint (the rain's haze learned this first). Starting it at the look-at
 *  point plus a dial leaves the middle of the view clear, and because the start follows
 *  the camera, zooming keeps the same layout of clear middle and hazy top.
 *
 *  ONE SET OF DIALS PER THEATER: Temperate and Desert. Any other theater draws no fog,
 *  rather than borrowing a colour made for different ground.
 *
 *  TIER 1. A Voodoo 2 has hardware fog (Glide's fog table, OpenGL's GL_FOG on the
 *  fixed pipeline), which can draw the same linear ramp from the same start and depth
 *  per vertex; the per-pixel luminance gate that keeps the shroud black has no
 *  fixed-function equivalent there, so the shroud would take the fog too.
 * ==================================================================================== */
#ifndef OPENCNC3D_DISTFOG_MOD_H
#define OPENCNC3D_DISTFOG_MOD_H

/* The engine's TheaterType: DESERT 0, JUNGLE 1, TEMPERATE 2, WINTER 3. */
enum { DFOG_TH_DESERT = 0, DFOG_TH_TEMPERATE = 2 };

/* What the last frame logged, so the FX| line is written once per change rather than
   once per frame. */
static int   g_dfogSaidOn = -1, g_dfogSaidTh = -99;
static float g_dfogSaidNear = -1.0f, g_dfogSaidFar = -1.0f;

/* Where the top edge of the view meets the ground at height gy, as a distance from the
   eye; 0 when the top edge looks above the horizon and never meets it. A reading for
   the log only, so the dials can be read against what the screen actually spans. */
static float distfog_top_edge_dist(float gy)
{
    float a[4], b[4];
    const float cn[4] = { 0.0f, 1.0f, -1.0f, 1.0f }, cf[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
    for (int i = 0; i < 4; i++) {
        a[i] = b[i] = 0.0f;
        for (int j = 0; j < 4; j++) {
            a[i] += g_fxInvVP[j * 4 + i] * cn[j];
            b[i] += g_fxInvVP[j * 4 + i] * cf[j];
        }
    }
    if (fabsf(a[3]) < 1e-9f || fabsf(b[3]) < 1e-9f) return 0.0f;
    for (int i = 0; i < 3; i++) { a[i] /= a[3]; b[i] /= b[3]; }
    const float dy = b[1] - a[1];
    if (fabsf(dy) < 1e-6f) return 0.0f;
    const float t = (gy - a[1]) / dy;
    if (t <= 0.0f) return 0.0f;
    const float px = a[0] + (b[0] - a[0]) * t, pz = a[2] + (b[2] - a[2]) * t;
    const float ex = px - g_fxCamPos[0], ey = gy - g_fxCamPos[1], ez = pz - g_fxCamPos[2];
    return sqrtf(ex * ex + ey * ey + ez * ez);
}

/* Forward-declared in fx_post.h and called with the light program bound. Zero thickness
   whenever the fog is off, the mode is not Enhanced or the theater has no dials, so the
   shader's block is a uniform branch that costs nothing. */
static void distfog_light_uniforms(GLuint prog)
{
    const int th = g_theater;
    /* RA's snow maps share the cool temperate dials. TD's winter remains unchanged. */
    const int haveTh = (th == DFOG_TH_TEMPERATE || th == DFOG_TH_DESERT
                        || (g_raModule && th == CNC_THEATER_WINTER));
    const int on = g_fx.dfog_on && haveTh && fx_mode_of(&g_fx) == FX_MODE_ENHANCED;
    const int des = (th == DFOG_TH_DESERT);

    const float amt   = des ? g_fx.dfog_d_amount : g_fx.dfog_t_amount;
    const float start = des ? g_fx.dfog_d_start  : g_fx.dfog_t_start;
    const float depth = des ? g_fx.dfog_d_depth  : g_fx.dfog_t_depth;

    const float ty = terrain_y(g_camX, g_camZ);
    const float dx = g_fxCamPos[0] - g_camX, dy = g_fxCamPos[1] - ty, dz = g_fxCamPos[2] - g_camZ;
    const float look = sqrtf(dx * dx + dy * dy + dz * dz);

    fx_set1f(prog, "uFogAmt", on ? amt : 0.0f);
    fx_set1f(prog, "uFogNear", look + start);
    fx_set1f(prog, "uFogInvDepth", 1.0f / (depth > 0.1f ? depth : 0.1f));
    if (des) fx_set3f(prog, "uFogCol", g_fx.dfog_d_r, g_fx.dfog_d_g, g_fx.dfog_d_b);
    else     fx_set3f(prog, "uFogCol", g_fx.dfog_t_r, g_fx.dfog_t_g, g_fx.dfog_t_b);

    /* THE LOG LINE: on or off, the theater, and the two distances that matter for
       reading the dials, both in cells past the look-at point: where the haze reaches
       its fullest, and where the top edge of the screen meets the ground. A haze whose
       fullest lies far beyond the top edge never shows at full thickness. */
    const float topDist = distfog_top_edge_dist(ty);
    const float topPast = topDist > 0.0f ? topDist - look : -1.0f;
    if (on != g_dfogSaidOn || th != g_dfogSaidTh
        || (on && (fabsf(look - g_dfogSaidNear) > 0.5f || fabsf(topPast - g_dfogSaidFar) > 0.5f))) {
        if (on)
            fprintf(stderr, "FX|distfog|on theater %d thickness %.2f starts %.1f full %.1f "
                    "cells past the look-at point; top edge %.1f past it (eye %.1f)\n",
                    th, amt, start, start + depth, topPast, look);
        else
            fprintf(stderr, "FX|distfog|off (theater %d%s)\n", th,
                    !g_fx.dfog_on ? "" : !haveTh ? ", no fog for this theater"
                                                 : ", not Enhanced");
        g_dfogSaidOn = on; g_dfogSaidTh = th;
        g_dfogSaidNear = look; g_dfogSaidFar = topPast;
    }
}

#endif /* OPENCNC3D_DISTFOG_MOD_H */
