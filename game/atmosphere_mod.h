/* Enhanced moving mist and desert dust. The host supplies theatre and shroud state.
   Twelve samples at half resolution integrate extinction and local in-scattering;
   a depth-aware resolve keeps air in front of the surface that ended the ray.
   Classic and Tier 1 skip the entire pass. No external texture or simulation state. */
#ifndef OPENCNC3D_ATMOSPHERE_MOD_H
#define OPENCNC3D_ATMOSPHERE_MOD_H
#include "atmosphere_glsl.h"

static GLuint g_airProgram = 0, g_airComposite = 0, g_airVisibility = 0;
static GLuint g_terrainCliffMask = 0;
static int g_airTried = 0, g_airVisW = 0, g_airVisH = 0;
static FxRT g_airVolume, g_airResult;

static void atmosphere_shutdown(void)
{
    fx_rt_free(&g_airVolume); fx_rt_free(&g_airResult);
    /* The two programs are kept for the life of the process, and so is g_airTried:
       rebuilt every mission they leaked in the driver like the chain's (fx_shutdown). */
    if (g_airVisibility) glDeleteTextures(1, &g_airVisibility);
    if (g_terrainCliffMask) glDeleteTextures(1, &g_terrainCliffMask);
    g_terrainCliffMask = 0;
    g_airVisibility = 0;
    g_airVisW = g_airVisH = 0;
}

static GLuint atmosphere_program(const char* head, const char* body, const char* name)
{
    const size_t n = strlen(head) + strlen(FX_COMMON) + strlen(body) + 1;
    char* src = (char*)malloc(n);
    if (!src) return 0;
    strcpy(src, head); strcat(src, FX_COMMON); strcat(src, body);
    GLuint p = fx_program(src, name);
    free(src);
    return p;
}

/* Shared smooth visibility for the air volume and ground-only colour controls.
   The caller selects a texture unit; this upload never touches framebuffer state. */
static GLuint atmosphere_visibility_update(void)
{
    /* Weather follows the continuous corner field used by the soft shroud.
       Mapped-but-unseen darkness remains zero; clear edges fade instead of stepping
       at each cell. Texel centres sit exactly on world corners. */
    const int mw = g_shroudCornW-1, mh = g_shroudCornH-1;
    if (mw < 1 || mh < 1) return 0;
    const int tw = mw+1, th = mh+1;
    std::vector<unsigned char> visibility((size_t)tw*th);
    for (int z = 0; z < th; z++) for (int x = 0; x < tw; x++) {
        const float v = (shroud_vis_at((float)x,(float)z)-(1.0f-SHROUD_DARK_ALPHA))/SHROUD_DARK_ALPHA;
        visibility[(size_t)z*tw+x] = (unsigned char)(255.0f*std::max(0.0f,std::min(1.0f,v))+0.5f);
    }
    if (!g_airVisibility) glGenTextures(1, &g_airVisibility);
    glBindTexture(GL_TEXTURE_2D, g_airVisibility);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (tw != g_airVisW || th != g_airVisH) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tw, th, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, visibility.data());
        g_airVisW = tw; g_airVisH = th;
    } else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tw, th, GL_LUMINANCE, GL_UNSIGNED_BYTE, visibility.data());
    return g_airVisibility;
}

/* Painted cliff shadows cannot be separated exactly from rock pigment. The
   terrain dial softens only dark texels of known cliff templates, with a bounded
   colour-preserving gain; it does not brighten grass, roads or source highlights.
   Rebuild this tiny cell field from the current pack, so editor/theatre changes
   cannot leave a stale mask. No artwork is rewritten. */
static int terrain_cliff_mask_bind(GLuint prog)
{
    if (g_raModule || terrain_texset_drawn() != FX_TEX_ENH ||
        g_fx.terrain_shadow_strength == 1.0f || g_gridW < 1 || g_gridH < 1) return 0;
    glActiveTexture(GL_TEXTURE0 + 11);
    terrain_seams_ensure();
    if (!g_seams.ok) return 0;
    int nslots = 0;
    const TtSlot* tab = tt_table(g_pack.theater, &nslots);
    std::vector<int> templates(nslots, -1);
    for (int i = 0; i < nslots; i++)
        if (tab[i].slot < (unsigned)nslots) templates[tab[i].slot] = tab[i].tmpl;
    std::vector<unsigned char> mask((size_t)g_gridW*g_gridH, 0);
    for (size_t i = 0; i < g_pack.cell.size(); i++) {
        const PackCell& c = g_pack.cell[i];
        const int slot = ts_slot_of_cell(c, nslots);
        if (c.x < 0 || c.y < 0 || c.x >= g_gridW || c.y >= g_gridH || slot < 0) continue;
        const int tmpl = templates[slot];
        if (tmpl >= 0 && tmpl < (int)g_seams.cliffTmpl.size() && g_seams.cliffTmpl[tmpl])
            mask[(size_t)c.y*g_gridW+c.x] = 255;
    }
    if (!g_terrainCliffMask) glGenTextures(1, &g_terrainCliffMask);
    glBindTexture(GL_TEXTURE_2D, g_terrainCliffMask);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, g_gridW, g_gridH, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, mask.data());
    fx_set1i(prog, "uTerrainCliff", 11);
    return 1;
}

static int terrain_visibility_bind(GLuint prog)
{
    glActiveTexture(GL_TEXTURE0 + 9);
    const GLuint tex = atmosphere_visibility_update();
    glBindTexture(GL_TEXTURE_2D, tex);
    fx_set1i(prog, "uTerrainVisibility", 9);
    fx_set2f(prog, "uTerrainVisSize", (float)g_airVisW, (float)g_airVisH);
    fx_set1i(prog, "uTerrainCliffOn", tex && terrain_cliff_mask_bind(prog));
    /* Water is translucent and retains the ground's depth. Its shore field keeps
       terrain colour controls off both the surface and its inland foam wash. */
    glActiveTexture(GL_TEXTURE0 + 10);
    const int water = water_field_sync() && g_waterFieldTex;
    glBindTexture(GL_TEXTURE_2D, water ? g_waterFieldTex : 0);
    fx_set1i(prog, "uTerrainWater", 10);
    fx_set1i(prog, "uTerrainWaterOn", water);
    fx_set2f(prog, "uTerrainWaterScale", g_gridW > 0 ? 1.0f/g_gridW : 0.0f,
                                      g_gridH > 0 ? 1.0f/g_gridH : 0.0f);
    fx_set1f(prog, "uTerrainDryEdge", 0.5f-(g_fx.water_foam_cross+0.25f)/(2.0f*WATER_FIELD_RANGE));
    glActiveTexture(GL_TEXTURE0);
    return tex != 0;
}

static const FxRT* atmosphere_apply(const FxRT* source, int w, int h)
{
    const int desert = !strcmp(g_pack.theater, "DESERT") || !strcmp(g_pack.theater, "SAND");
    const int on = desert ? g_fx.sand_fx : g_fx.fog_fx;
    const float density = desert ? g_fx.sand_density : g_fx.fog_density;
    if (!g_fxActive || !on || density <= 0.0f || !g_fxScene.depth) return source;
    if (!g_airTried) {
        g_airTried = 1;
        g_airProgram = atmosphere_program(ATMOSPHERE_FS_HEADER, ATMOSPHERE_FS_BODY, "air-volume");
        g_airComposite = atmosphere_program(ATMOSPHERE_COMPOSITE_HEADER, ATMOSPHERE_COMPOSITE_BODY, "air-composite");
        fprintf(stderr, "FX|atmosphere|%s\n", (g_airProgram && g_airComposite) ? "ready" : "unavailable");
    }
    if (!g_airProgram || !g_airComposite) return source;
    const int vw = (w+1)/2, vh = (h+1)/2;
    if (!fx_rt_init(&g_airVolume, vw, vh, FX_RT_COLOUR | fx_hdr_want(), GL_NEAREST) ||
        !fx_rt_init(&g_airResult, w, h, FX_RT_COLOUR | fx_hdr_want(), GL_LINEAR)) return source;

    glActiveTexture(GL_TEXTURE2);
    if (!atmosphere_visibility_update()) { glActiveTexture(GL_TEXTURE0); return source; }
    glBindTexture(GL_TEXTURE_2D, g_airVisibility);
    const int mw = g_airVisW-1, mh = g_airVisH-1;
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_fxScene.depth);
    glActiveTexture(GL_TEXTURE0);

    fx_glUseProgram(g_airProgram);
    fx_set1i(g_airProgram, "uDepth", 1); fx_set1i(g_airProgram, "uVisibility", 2);
    fx_setmat(g_airProgram, "uInvVP", g_fxInvVP);
    fx_set2f(g_airProgram, "uMapSize", (float)mw, (float)mh);
    fx_set1f(g_airProgram, "uDensity", density);
    fx_set1f(g_airProgram, "uHeight", fmaxf(0.1f, desert ? g_fx.sand_height : g_fx.fog_height));
    const float scale = fmaxf(2.0f, desert ? g_fx.sand_scale : g_fx.fog_scale);
    const float speed = desert ? g_fx.sand_speed : g_fx.fog_speed;
    const float rad = (desert ? g_fx.sand_wind : g_fx.fog_wind) * (3.14159265358979f/180.0f);
    fx_set1f(g_airProgram, "uScale", scale);
    fx_set2f(g_airProgram, "uWind", sinf(rad), -cosf(rad));
    fx_set2f(g_airProgram, "uDrift", (float)fmod((double)speed*g_fxTime/scale,64.0),
             (float)fmod((double)speed*1.6*2.7*g_fxTime/scale,64.0));
    fx_set1f(g_airProgram, "uScatter", desert ? g_fx.sand_light : g_fx.fog_light);
    fx_set1f(g_airProgram, "uFalloff", fmaxf(0.01f,g_fx.light_falloff));
    if (desert) fx_set3f(g_airProgram, "uAirColour", 0.56f,0.40f,0.22f);
    else fx_set3f(g_airProgram, "uAirColour", 0.55f,0.62f,0.67f);
    const int nl = g_fx.lights_on ? std::min(g_fxNLight, FX_MAX_LIGHTS) : 0;
    float positions[FX_MAX_LIGHTS*4], colours[FX_MAX_LIGHTS*4];
    for (int i = 0; i < nl; i++) {
        const FxLight& light = g_fxLight[i];
        positions[i*4] = light.x; positions[i*4+1] = light.y;
        positions[i*4+2] = light.z; positions[i*4+3] = light.radius;
        colours[i*4] = light.r; colours[i*4+1] = light.g;
        colours[i*4+2] = light.b; colours[i*4+3] = light.power;
    }
    fx_set1i(g_airProgram, "uNLights", nl);
    if (nl) {
        fx_set4fv(g_airProgram, "uLightPos", nl, positions);
        fx_set4fv(g_airProgram, "uLightCol", nl, colours);
    }
    tibglow_light_uniforms(g_airProgram);
    tibglow_light_bind();
    fx_rt_bind(&g_airVolume); fx_fullscreen_quad();
    tibglow_light_unbind();

    fx_glUseProgram(g_airComposite);
    fx_set1i(g_airComposite, "uScene", 0); fx_set1i(g_airComposite, "uVolume", 3);
    fx_set1i(g_airComposite, "uDepth", 1); fx_set1i(g_airComposite, "uVisibility", 2);
    fx_setmat(g_airComposite, "uInvVP", g_fxInvVP);
    fx_set2f(g_airComposite, "uMapSize", (float)mw, (float)mh);
    fx_set2f(g_airComposite, "uVolumeSize", (float)vw, (float)vh);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, g_airVolume.tex);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, source->tex);
    fx_rt_bind(&g_airResult); fx_fullscreen_quad();
    fx_glUseProgram(0);
    for (int unit = 3; unit >= 1; unit--) {
        glActiveTexture(GL_TEXTURE0+unit); glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D);
    }
    glActiveTexture(GL_TEXTURE0);
    return &g_airResult;
}
#endif
