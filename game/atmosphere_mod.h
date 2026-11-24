/* Enhanced moving mist and desert dust. The host supplies theatre and shroud state.
   Twelve samples at half resolution integrate extinction and local in-scattering;
   a depth-aware resolve keeps air in front of the surface that ended the ray.
   Classic and Tier 1 skip the entire pass. No external texture or simulation state. */
#ifndef OPENCNC3D_ATMOSPHERE_MOD_H
#define OPENCNC3D_ATMOSPHERE_MOD_H
#include "atmosphere_glsl.h"

static GLuint g_airProgram = 0, g_airComposite = 0, g_airVisibility = 0;
static int g_airTried = 0, g_airVisW = 0, g_airVisH = 0;
/* LIGHT SHAFTS: the same two programs with ATMOSPHERE_SHAFTS_FS compiled in. Separate
   programs rather than a uniform switch, so the shafts dial off runs the very program
   the air ran before they existed. */
static GLuint g_shaftProgram = 0, g_shaftComposite = 0;
static int g_shaftTried = 0;
static FxRT g_airVolume, g_airResult;

static void atmosphere_shutdown(void)
{
    fx_rt_free(&g_airVolume); fx_rt_free(&g_airResult);
    /* The two programs are kept for the life of the process, and so is g_airTried:
       rebuilt every mission they leaked in the driver like the chain's (fx_shutdown). */
    if (g_airVisibility) glDeleteTextures(1, &g_airVisibility);
    g_airVisibility = 0;
    g_airVisW = g_airVisH = 0;
}

static GLuint atmosphere_program(const char* head, const char* body, const char* name,
                                 const char* extra = "")
{
    const size_t n = strlen(head) + strlen(FX_COMMON) + strlen(extra) + strlen(body) + 1;
    char* src = (char*)malloc(n);
    if (!src) return 0;
    strcpy(src, head); strcat(src, FX_COMMON); strcat(src, extra); strcat(src, body);
    GLuint p = fx_program(src, name);
    free(src);
    return p;
}

/* ---- LIGHT SHAFTS ----------------------------------------------------------------
   The haze each theater carries in its lower air when shafts are on, before the
   shafts_haze multiple: extinction per cell at the ground, and the height in cells
   over which it thins by e. It has no colour, because it draws no light of its own
   (atmosphere_glsl.h): where the sun reaches it, it leaves the picture as it was, and
   where the sun does not, it takes away the sunlit glow the picture was tuned with.
   So these numbers set only how deep a beam's shadow reads, never the view's colour.

   The densities are low on purpose. At 0.12 a cell in temperate and winter, the
   shadowed haze laid broad dark bands over ground that was itself in full sun, up to
   half its brightness on a sunlit tiberium field with no caster near it, which reads
   as a wrong ground shadow and not as air; it also took one to one and a half levels
   off whole views. So every outdoor theater now carries the desert's thin haze, and
   snow a little more, because bright snow shows the same fraction as more levels and
   the snow maps showed no phantom bands. A beam still shows where there is weather to
   carry it: the mist and the dust lose the sun's share in shadow whatever this haze
   is. Red Alert's interiors have no sun to
   make a beam with and take none. They hold unchanged under linear light, because the
   air is laid over the encoded picture there too (atmosphere_glsl.h). */
struct ShaftHaze { float density, height; };
static int shafts_indoor(void)
{
    /* Red Alert's interior scenes load the RAINT pack; INTERIOR is the engine's own
       name for the same theater. ("INT" alone would also match WINTER.) */
    return !strcmp(g_pack.theater, "RAINT") || strstr(g_pack.theater, "INTERIOR") != 0;
}
static ShaftHaze shafts_theater_haze(void)
{
    const char* t = g_pack.theater;
    if (strstr(t, "DESERT") || strstr(t, "SAND")) { ShaftHaze h = { 0.050f, 1.2f }; return h; }
    if (strstr(t, "SNOW"))                        { ShaftHaze h = { 0.080f, 1.2f }; return h; }
    if (strstr(t, "WINTER"))                      { ShaftHaze h = { 0.050f, 1.2f }; return h; }
    ShaftHaze h = { 0.050f, 1.2f };
    return h;
}

/* Whether this frame's air takes the shafts programs. Needs the sun's map: with sun
   shadows off there is nothing to cut the air with, so the feature stands down whole
   and the air is the one drawn before. So does a strength of 0, which would leave the
   shafts programs nothing to add but a different march pattern. */
static int shafts_live(void)
{
    return g_fx.shafts_on && g_fx.shafts_strength > 0.0f && g_fx.shadow_on && g_fxShadow.depth &&
           !shafts_indoor();
}

static void shafts_uniforms(GLuint prog, const ShaftHaze& haze)
{
    fx_set1i(prog, "uShaftShadow", 4);
    fx_set1i(prog, "uShaftTerrainShadow", 5);
    fx_set1i(prog, "uShaftCloud", 6);
    fx_setmat(prog, "uShaftSunVP", g_fxSunVP);
    fx_set3f(prog, "uShaftSunDir", g_fxSunDir[0], g_fxSunDir[1], g_fxSunDir[2]);
    fx_set1i(prog, "uShaftShadowOn", 1);
    fx_set1i(prog, "uShaftTerrainOn", g_fxTerrainShadowLive);
    fx_set1f(prog, "uShaftTerrainStrength", fx_terrain_shadow_strength());
    /* The surfaces' bias, cells over the map's depth span, and no slope term: a point
       in the air has no face to be edge-on with. */
    fx_set1f(prog, "uShaftBias", g_fx.shadow_bias / (g_fxSunSpan > 0.001f ? g_fxSunSpan : 1.0f));
    fx_set1f(prog, "uShaftStrength", fx_clampf(g_fx.shafts_strength, 0.0f, 1.0f));
    /* The lobe's normaliser: the value of numerator that makes the lobe average 1 over
       a 5x5 grid of rays across the view, each unprojected from the near to the far
       plane through the same inverse the shader's fx_world uses (column major). With
       it the dial moves where the air's shadows are deeper, not how deep they are on
       average. */
    const float g = fx_clampf(g_fx.shafts_phase, 0.0f, 0.9f);
    const float* M = g_fxInvVP;
    double lobe = 0.0; int rays = 0;
    for (int gy = 0; gy < 5; gy++) for (int gx = 0; gx < 5; gx++) {
        const float x = -0.8f + 0.4f*(float)gx, y = -0.8f + 0.4f*(float)gy;
        float ends[2][3];
        for (int e = 0; e < 2; e++) {
            const float z = e ? 1.0f : -1.0f;
            const float w = M[3]*x + M[7]*y + M[11]*z + M[15];
            for (int i = 0; i < 3; i++)
                ends[e][i] = (M[i]*x + M[i+4]*y + M[i+8]*z + M[i+12]) / (fabsf(w) > 1e-8f ? w : 1e-8f);
        }
        const float rd[3] = { ends[1][0]-ends[0][0], ends[1][1]-ends[0][1], ends[1][2]-ends[0][2] };
        const float rl = sqrtf(rd[0]*rd[0] + rd[1]*rd[1] + rd[2]*rd[2]);
        if (rl <= 1e-6f) continue;
        const float c = -(g_fxSunDir[0]*rd[0] + g_fxSunDir[1]*rd[1] + g_fxSunDir[2]*rd[2]) / rl;
        lobe += pow(fmax(1.0 + g*g - 2.0*g*c, 1e-4), -1.5);
        rays++;
    }
    const float phase0 = rays ? (float)pow(lobe / rays, -1.0/1.5) : 1.0f;
    fx_set1f(prog, "uShaftG", g);
    fx_set1f(prog, "uShaftPhase0", phase0);
    /* The cloud deck, in the form the light pass hands it to the surfaces, with its
       strength scaled by shafts_clouds: the deck darkens the air over whole screens
       at once, offset from its ground shadow by the air's height, which reads as
       patchy dimming long before it reads as a shadow. */
    const int clOn = (g_fx.cloud_on && g_fxCloudTex && g_fx.shafts_clouds > 0.0f) ? 1 : 0;
    const float clScale = g_fx.cloud_scale > 1.0f ? g_fx.cloud_scale : 1.0f;
    const float clRad = g_fx.cloud_rot * (3.14159265358979f / 180.0f);
    fx_set1i(prog, "uShaftCloudOn", clOn);
    fx_set1f(prog, "uShaftCStrength", g_fx.cloud_strength * fx_clampf(g_fx.shafts_clouds, 0.0f, 1.0f));
    fx_set1f(prog, "uShaftCEdge", 1.0f - fx_clampf(g_fx.cloud_cover, 0.0f, 1.0f));
    fx_set1f(prog, "uShaftCWidth", 0.40f - 0.39f * fx_clampf(g_fx.cloud_sharp, 0.0f, 1.0f));
    fx_set1f(prog, "uShaftCInvScale", 1.0f / clScale);
    fx_set1f(prog, "uShaftCHeight", g_fx.cloud_height);
    fx_set2f(prog, "uShaftCDir", sinf(clRad), -cosf(clRad));
    fx_set1f(prog, "uShaftCDrift", fmodf(g_fx.cloud_speed * g_fxTime / clScale, 1.0f));
    fx_set1f(prog, "uShaftHaze", haze.density * fmaxf(0.0f, g_fx.shafts_haze));
    fx_set1f(prog, "uShaftHazeHeight", fmaxf(0.1f, haze.height));

    /* The mask is read inside the march, after per-pixel early returns, where the
       implicit derivative that picks a mip level is undefined. Its base level is
       never minified there (512 texels over 20 cells or more against a half
       resolution volume), so for this pass it is read from the base level alone and
       its mip filter is put back in shafts_unbind. */
    glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, clOn ? g_fxCloudTex : 0);
    if (clOn) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, g_fxTerrainShadowLive ? g_fxTerrainShadow.depth : 0);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, g_fxShadow.depth);
    glActiveTexture(GL_TEXTURE0);
}

static void shafts_unbind(void)
{
    glActiveTexture(GL_TEXTURE6);
    if (g_fxCloudTex) {
        glBindTexture(GL_TEXTURE_2D, g_fxCloudTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    for (int unit = 6; unit >= 4; unit--) {
        glActiveTexture(GL_TEXTURE0+unit); glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
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
    /* ONCE A FRAME, AND SENT ONLY WHEN IT CHANGED. Two or three passes ask for this every
       frame, and the shroud it reads cannot move inside a frame. Rewriting a texture the
       GPU is still reading from an earlier pass makes the driver either wait for that
       pass or copy the texture, and the field is usually identical to the last one sent:
       it changes only when the shroud does. */
    static unsigned s_frame = ~0u;
    static std::vector<unsigned char> s_vis, s_sent, s_check;
    /* AND WORKED OUT ONLY WHEN THE SHROUD CHANGED. The values are shroud_vis_at's, which
       reads the corner field and the switches and dials in the key below and nothing
       else, so a frame whose key matches the last build reuses it
       (OPENCNC3D_SHROUD_CACHE, shroud_mod.h). Under =0 it is worked out once a frame. */
    static std::vector<unsigned> s_key, key;
    const int cache = shroud_cache_mode();
    if (cache) {
        shroud_corners_sync();
        key.clear();
        key.push_back(g_shroudFieldGen);
        key.push_back(shroud_key_bits(SHROUD_DARK_ALPHA));
        key.push_back((unsigned)shroud_fow_enhanced());
        key.push_back(g_fx.fow_smooth ? 1u : 0u);
        key.push_back(shroud_key_bits(g_fx.fow_falloff));
        key.push_back((unsigned)g_shroudEditorOff);
        key.push_back((unsigned)g_shroudRimOn);
        key.push_back((unsigned)g_shroudOn);
        key.push_back((unsigned)g_shroudValid);
        key.push_back((unsigned)g_shroudRevealed);
        key.push_back((unsigned)tw);
        key.push_back((unsigned)th);
    }
    const auto build = [&](std::vector<unsigned char>& out) {
        out.resize((size_t)tw*th);
        for (int z = 0; z < th; z++) for (int x = 0; x < tw; x++) {
            const float v = (shroud_vis_at((float)x,(float)z)-(1.0f-SHROUD_DARK_ALPHA))/SHROUD_DARK_ALPHA;
            out[(size_t)z*tw+x] = (unsigned char)(255.0f*std::max(0.0f,std::min(1.0f,v))+0.5f);
        }
    };
    if (g_airVisibility && tw == g_airVisW && th == g_airVisH
        && (cache ? key == s_key : s_frame == g_drawFrameSerial)) {
        if (s_frame != g_drawFrameSerial) {
            g_shroudCache.visKept++;
            if (cache == 2) {               /* verify: worked out again, once a frame */
                build(s_check);
                g_shroudCache.checks++;
                if (s_check != s_vis || s_check != s_sent)
                    shroud_cache_mismatch("air visibility", (long)s_check.size());
            }
        }
        s_frame = g_drawFrameSerial;
        glBindTexture(GL_TEXTURE_2D, g_airVisibility);
        return g_airVisibility;
    }
    s_frame = g_drawFrameSerial;
    s_key = key;
    g_shroudCache.visBuilt++;
    build(s_vis);
    if (!g_airVisibility) glGenTextures(1, &g_airVisibility);
    glBindTexture(GL_TEXTURE_2D, g_airVisibility);
    if (tw != g_airVisW || th != g_airVisH) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tw, th, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_vis.data());
        g_airVisW = tw; g_airVisH = th;
        s_sent = s_vis;
    } else if (s_vis != s_sent) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tw, th, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_vis.data());
        s_sent = s_vis;
    }
    return g_airVisibility;
}

/* Painted terrain shade uses one response on every Enhanced surface in both
   games and every theater. Restricting it to whole cliff cells also lightened
   their ground, leaving a bright border beside unchanged ordinary cells. The
   normal/class buffer excludes objects; shroud and the shore field protect fog
   and water. No template names or artwork-derived colour masks gate this dial. */
static int terrain_visibility_bind(GLuint prog)
{
    glActiveTexture(GL_TEXTURE0 + 9);
    const GLuint tex = atmosphere_visibility_update();
    glBindTexture(GL_TEXTURE_2D, tex);
    fx_set1i(prog, "uTerrainVisibility", 9);
    fx_set2f(prog, "uTerrainVisSize", (float)g_airVisW, (float)g_airVisH);
    fx_set1i(prog, "uTerrainArtShadeOn", tex && !fx_unified_lighting() && fx_texset_get() == FX_TEX_ENH
        && fx_terrain_shadow_strength() != 1.0f && g_gridW > 0 && g_gridH > 0);
    /* Water is translucent and retains the ground's depth. Its shore field keeps
       terrain colour controls off both the surface and its inland foam wash. */
    glActiveTexture(GL_TEXTURE0 + 10);
    const int water = water_field_sync() && g_waterFieldTex;
    glBindTexture(GL_TEXTURE_2D, water ? g_waterFieldTex : 0);
    fx_set1i(prog, "uTerrainWater", 10);
    fx_set1i(prog, "uTerrainWaterOn", water);
    fx_set2f(prog, "uTerrainWaterScale", g_gridW > 0 ? 1.0f/g_gridW : 0.0f,
                                      g_gridH > 0 ? 1.0f/g_gridH : 0.0f);
    const float dryEdge = 0.5f-(g_fx.water_foam_cross+0.25f)/(2.0f*WATER_FIELD_RANGE);
    fx_set1f(prog, "uTerrainDryEdge", dryEdge);
    /* Every exterior biome shares one continuous grade-to-wash transition. A
       hard distance contour drew a brown/pale ring, made worse by colour tint. */
    fx_set2f(prog, "uTerrainDryFade", dryEdge-0.5f/(2.0f*WATER_FIELD_RANGE),
                                          dryEdge+0.25f/(2.0f*WATER_FIELD_RANGE));
    glActiveTexture(GL_TEXTURE0);
    return tex != 0;
}

static const FxRT* atmosphere_apply(const FxRT* source, int w, int h)
{
    const int desert = !strcmp(g_pack.theater, "DESERT") || !strcmp(g_pack.theater, "SAND");
    const int on = desert ? g_fx.sand_fx : g_fx.fog_fx;
    const float density = desert ? g_fx.sand_density : g_fx.fog_density;
    /* LIGHT SHAFTS: with them the pass also runs for the haze alone, where the
       theater's weather is switched off. */
    const int weather = on && density > 0.0f;
    const int shafts = g_fxActive && shafts_live();
    const ShaftHaze haze = shafts_theater_haze();
    const int hazeOn = shafts && haze.density * g_fx.shafts_haze > 0.0f;
    if (!g_fxActive || (!weather && !hazeOn) || !g_fxScene.depth) return source;
    if (shafts && !g_shaftTried) {
        g_shaftTried = 1;
        g_shaftProgram = atmosphere_program(ATMOSPHERE_FS_HEADER, ATMOSPHERE_FS_BODY, "air-volume-shafts",
                                            ATMOSPHERE_SHAFTS_FS);
        g_shaftComposite = atmosphere_program(ATMOSPHERE_COMPOSITE_HEADER, ATMOSPHERE_COMPOSITE_BODY,
                                              "air-composite-shafts", "#define AIR_SHAFTS 1\n");
        fprintf(stderr, "FX|shafts|%s\n", (g_shaftProgram && g_shaftComposite) ? "ready" : "unavailable");
    }
    const int useShafts = shafts && g_shaftProgram && g_shaftComposite;
    if (!useShafts && !weather) return source;
    if (!g_airTried) {
        g_airTried = 1;
        g_airProgram = atmosphere_program(ATMOSPHERE_FS_HEADER, ATMOSPHERE_FS_BODY, "air-volume");
        g_airComposite = atmosphere_program(ATMOSPHERE_COMPOSITE_HEADER, ATMOSPHERE_COMPOSITE_BODY, "air-composite");
        fprintf(stderr, "FX|atmosphere|%s\n", (g_airProgram && g_airComposite) ? "ready" : "unavailable");
    }
    if (!useShafts && (!g_airProgram || !g_airComposite)) return source;
    const GLuint volumeProg = useShafts ? g_shaftProgram : g_airProgram;
    const GLuint compositeProg = useShafts ? g_shaftComposite : g_airComposite;
    const int vw = (w+1)/2, vh = (h+1)/2;
    if (!fx_rt_init(&g_airVolume, vw, vh, FX_RT_COLOUR | fx_hdr_want(), GL_NEAREST) ||
        !fx_rt_init(&g_airResult, w, h, FX_RT_COLOUR | fx_hdr_want(), GL_LINEAR)) return source;

    glActiveTexture(GL_TEXTURE2);
    if (!atmosphere_visibility_update()) { glActiveTexture(GL_TEXTURE0); return source; }
    glBindTexture(GL_TEXTURE_2D, g_airVisibility);
    const int mw = g_airVisW-1, mh = g_airVisH-1;
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_fxScene.depth);
    glActiveTexture(GL_TEXTURE0);

    fx_glUseProgram(volumeProg);
    fx_set1i(volumeProg, "uDepth", 1); fx_set1i(volumeProg, "uVisibility", 2);
    fx_setmat(volumeProg, "uInvVP", g_fxInvVP);
    fx_set2f(volumeProg, "uMapSize", (float)mw, (float)mh);
    fx_set1f(volumeProg, "uDensity", weather ? density : 0.0f);
    fx_set1f(volumeProg, "uHeight", fmaxf(0.1f, desert ? g_fx.sand_height : g_fx.fog_height));
    const float scale = fmaxf(2.0f, desert ? g_fx.sand_scale : g_fx.fog_scale);
    const float speed = desert ? g_fx.sand_speed : g_fx.fog_speed;
    const float rad = (desert ? g_fx.sand_wind : g_fx.fog_wind) * (3.14159265358979f/180.0f);
    fx_set1f(volumeProg, "uScale", scale);
    fx_set2f(volumeProg, "uWind", sinf(rad), -cosf(rad));
    fx_set2f(volumeProg, "uDrift", (float)fmod((double)speed*g_fxTime/scale,64.0),
             (float)fmod((double)speed*1.6*2.7*g_fxTime/scale,64.0));
    fx_set1f(volumeProg, "uScatter", weather ? (desert ? g_fx.sand_light : g_fx.fog_light) : 0.0f);
    fx_set1f(volumeProg, "uFalloff", fmaxf(0.01f,g_fx.light_falloff));
    if (desert) fx_set3f(volumeProg, "uAirColour", 0.56f,0.40f,0.22f);
    else fx_set3f(volumeProg, "uAirColour", 0.55f,0.62f,0.67f);
    /* With no weather (the shafts' haze alone) nothing in the air scatters local or
       Tiberium light: that is the weather's, by the weather's own dial. */
    const int nl = (g_fx.lights_on && weather) ? std::min(g_fxNLight, FX_MAX_LIGHTS) : 0;
    float positions[FX_MAX_LIGHTS*4], colours[FX_MAX_LIGHTS*4];
    for (int i = 0; i < nl; i++) {
        const FxLight& light = g_fxLight[i];
        positions[i*4] = light.x; positions[i*4+1] = light.y;
        positions[i*4+2] = light.z; positions[i*4+3] = light.radius;
        colours[i*4] = light.r; colours[i*4+1] = light.g;
        colours[i*4+2] = light.b; colours[i*4+3] = light.power;
    }
    fx_set1i(volumeProg, "uNLights", nl);
    if (nl) {
        fx_set4fv(volumeProg, "uLightPos", nl, positions);
        fx_set4fv(volumeProg, "uLightCol", nl, colours);
    }
    tibglow_light_uniforms(volumeProg);
    tibglow_light_bind();
    if (useShafts) shafts_uniforms(volumeProg, haze);
    fx_rt_bind(&g_airVolume); fx_fullscreen_quad();
    tibglow_light_unbind();
    if (useShafts) shafts_unbind();

    fx_glUseProgram(compositeProg);
    fx_set1i(compositeProg, "uScene", 0); fx_set1i(compositeProg, "uVolume", 3);
    fx_set1i(compositeProg, "uLinOn", g_fxLinLive);
    fx_set1i(compositeProg, "uDepth", 1); fx_set1i(compositeProg, "uVisibility", 2);
    fx_setmat(compositeProg, "uInvVP", g_fxInvVP);
    fx_set2f(compositeProg, "uMapSize", (float)mw, (float)mh);
    fx_set2f(compositeProg, "uVolumeSize", (float)vw, (float)vh);
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
