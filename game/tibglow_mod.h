/* ====================================================================================
 *  tibglow_mod.h -- the tiberium's colour, the light a field gives off, and the glowing
 *                   fog that hangs over it. Enhanced only, all of it OURS.
 *
 *  THREE THINGS, ONE COLOUR.
 *
 *  1  THE COLOUR. Two dials, a hue on the colour wheel and a saturation, repaint the
 *     crystal sheet and the ground-crack tile on the CPU from the copies tib3d_mod.h
 *     keeps of the originals, and upload them again. Only the crystal part of the sheet
 *     moves: the sheet also carries the brown rock of the pods, and a texel takes the
 *     new hue in proportion to how close its own hue is to the crystals' measured one,
 *     so the rock stays rock. The same turn of the wheel is applied to the colour of
 *     the field's light and of its fog, so the four always agree. At the crystals' own
 *     hue and full saturation nothing is repainted at all, which keeps the default
 *     picture byte-identical to the one before these dials existed.
 *
 *  2  THE GLOW FIELD. The light a field gives off used to be point lights, one per 4x4
 *     block of cells, the densest first, competing with explosions for 24 slots. A large
 *     field came out as two or three bright blobs and a small one as nothing. It is now
 *     a texture instead: two texels a cell over the whole 64x64 map, each cell's growth
 *     stage splatted in and blurred, and the light pass reads it under every pixel. So
 *     every crystal contributes, the glow follows the field's real outline, and it
 *     fades out past the edge by a dial measured in cells. The texture holds three
 *     things: the glow spread for lighting (red), the glow spread for the fog (green),
 *     and the height of the ground (blue), which the fog march needs to know how far
 *     above the ground each of its samples is.
 *
 *  3  THE FOG. A short march along the view ray through a layer that sits on the
 *     ground over and around each field: thick near the ground, thinning upward, broken
 *     into wisps by the same tiling noise the cloud shadows use, drifting slowly on the
 *     engine clock. Every sample is lit by the field's own glow, so the fog shines in
 *     the crystals' colour where a field is dense and goes grey where it is thin.
 *
 *  DETERMINISM. The texture is rebuilt only when the field, the shroud over it or a
 *  dial changes, from the engine's own cell data, and the fog's drift and the glow's
 *  breathing run off the engine clock (g_fxTime). The march's per pixel jitter is
 *  hashed from gl_FragCoord. Two --shot runs stay byte-identical.
 *
 *  TIER 1. The colour is fixed-function work (a repainted texture), so the Voodoo 2
 *  keeps it. The glow field and the fog live in the post chain's light pass and are
 *  Tier 2 only; on Win98 a field glows through its own crystals and lights nothing
 *  around it, and there is no fog. Both are written up in docs/tier1-gap.md.
 * ==================================================================================== */
#ifndef OPENCNC3D_TIBGLOW_MOD_H
#define OPENCNC3D_TIBGLOW_MOD_H

/* The engine's map is 64x64 cells; two texels a cell keeps a one-cell field round
   rather than square without making the blur expensive. */
enum { TIBF_CELLS = 64, TIBF_RES = 2, TIBF_SIDE = TIBF_CELLS * TIBF_RES };

/* The colour every consumer starts from before the hue dial turns it: the light's and
   the fog's. Matched to the crystals' own art, which measures a little yellow of pure
   green. */
static const float TIBG_BASE_R = 0.36f, TIBG_BASE_G = 1.00f, TIBG_BASE_B = 0.30f;

/* ---- the colour --------------------------------------------------------------------- */

static float  g_tibcolNative = -1.0f;      /* the crystals' measured hue, degrees     */
static float  g_tibcolDoneHue = -1.0f, g_tibcolDoneSat = -1.0f;
static bool   g_tibcolDoneIdentity = true; /* the textures hold the original art      */
static GLuint g_tibcolDoneTex = 0;         /* which upload the done-values describe   */

static void tibcol_rgb2hsv(float r, float g, float b, float* h, float* s, float* v)
{
    const float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const float d = mx - mn;
    *v = mx;
    *s = mx > 1e-6f ? d / mx : 0.0f;
    if (d < 1e-6f) { *h = 0.0f; return; }
    float hh;
    if (mx == r)      hh = (g - b) / d;
    else if (mx == g) hh = 2.0f + (b - r) / d;
    else              hh = 4.0f + (r - g) / d;
    hh *= 60.0f;
    if (hh < 0.0f) hh += 360.0f;
    *h = hh;
}

static void tibcol_hsv2rgb(float h, float s, float v, float* r, float* g, float* b)
{
    h = fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    const float c = v * s, hp = h / 60.0f;
    const float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float r1 = 0, g1 = 0, b1 = 0;
    if      (hp < 1.0f) { r1 = c; g1 = x; }
    else if (hp < 2.0f) { r1 = x; g1 = c; }
    else if (hp < 3.0f) { g1 = c; b1 = x; }
    else if (hp < 4.0f) { g1 = x; b1 = c; }
    else if (hp < 5.0f) { r1 = x; b1 = c; }
    else                { r1 = c; b1 = x; }
    const float m = v - c;
    *r = r1 + m; *g = g1 + m; *b = b1 + m;
}

/* THE DIALS AS THE SHEET SEES THEM. The shipped dials (hue 73.93, saturation 1.16) were
   tuned on the version 1/2 sheet, whose crystals sit at 105 degrees, and turn ANY sheet's
   crystals to 73.9 and richer. On the remastered sheet (a version 3 pack, the rings rule)
   that reads too yellow and loud, so while the dials stand at those shipped values the
   rings pack is drawn at its own: TIB3D_RINGS_HUE and _SAT (tib3d_mod.h), a little
   yellower than the painted crystals, as tuned against game shots. A value moved by hand
   is used as it is, and an old pack never sees the substitution. */
static bool tibcol_dials_shipped(void)
{
    return fabsf(g_fx.tib_hue - 73.92857f) < 0.01f && fabsf(g_fx.tib_sat - 1.16071f) < 0.01f;
}
static float tibcol_hue(void)
{ return (g_tib3dRings && tibcol_dials_shipped()) ? TIB3D_RINGS_HUE : g_fx.tib_hue; }
static float tibcol_sat(void)
{ return (g_tib3dRings && tibcol_dials_shipped()) ? TIB3D_RINGS_SAT : g_fx.tib_sat; }

/* The crystals' own hue, as the circular mean over the sheet's saturated texels that
   are green at all. Measured rather than written in, so a re-baked sheet carries its
   own answer. */
static float tibcol_native(void)
{
    if (g_tibcolNative >= 0.0f) return g_tibcolNative;
    double sx = 0.0, sy = 0.0;
    const size_t n = (size_t)g_tib3dSheetW * g_tib3dSheetH;
    for (size_t i = 0; i < n && !g_tib3dSheetPx.empty(); i++) {
        const unsigned char* p = &g_tib3dSheetPx[i * 4];
        float h, s, v;
        tibcol_rgb2hsv(p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, &h, &s, &v);
        if (s < 0.2f || v < 0.08f || h < 60.0f || h > 170.0f) continue;
        sx += cos(h * 0.0174532925) * s;
        sy += sin(h * 0.0174532925) * s;
    }
    float h = (sx == 0.0 && sy == 0.0) ? 105.0f : (float)(atan2(sy, sx) * 57.2957795);
    if (h < 0.0f) h += 360.0f;
    g_tibcolNative = h;
    return h;
}

/* How far round the wheel the dial asks the crystals to turn, in degrees. */
static float tibcol_turn(void)
{
    float t = tibcol_hue() - tibcol_native();
    while (t > 180.0f) t -= 360.0f;
    while (t < -180.0f) t += 360.0f;
    return t;
}

/* At the crystals' own hue (within half a degree) and full saturation, nothing moves.
   That tolerance is what lets the default draw the original sheets untouched. */
static bool tibcol_is_identity(void)
{
    return fabsf(tibcol_turn()) < 0.5f && fabsf(tibcol_sat() - 1.0f) < 0.005f;
}

/* One colour turned the way the crystals are, for the light and the fog. */
static void tibcol_apply_rgb(float r, float g, float b, float* o)
{
    if (tibcol_is_identity()) { o[0] = r; o[1] = g; o[2] = b; return; }
    float h, s, v;
    tibcol_rgb2hsv(r, g, b, &h, &s, &v);
    s *= tibcol_sat();
    if (s > 1.0f) s = 1.0f;
    tibcol_hsv2rgb(h + tibcol_turn(), s, v, &o[0], &o[1], &o[2]);
}

/* Repaints n RGBA texels from src into dst. `band` keeps the turn to the texels whose
   hue is near the crystals' own (the sheet, which holds the pods' rock as well); the
   crack tile is all crystal light and turns whole. */
static void tibcol_repaint(const unsigned char* src, unsigned char* dst, size_t n, bool band)
{
    const float turn = tibcol_turn(), sat = tibcol_sat(), nat = tibcol_native();
    for (size_t i = 0; i < n; i++) {
        const unsigned char* p = src + i * 4;
        unsigned char* q = dst + i * 4;
        q[3] = p[3];
        float h, s, v;
        tibcol_rgb2hsv(p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, &h, &s, &v);
        float w = 1.0f;
        if (band) {
            float dh = fabsf(h - nat);
            if (dh > 180.0f) dh = 360.0f - dh;
            /* full inside 35 degrees of the crystals' hue, none past 55: the pods'
               browns sit 65 and more away */
            w = dh <= 35.0f ? 1.0f : dh >= 55.0f ? 0.0f : (55.0f - dh) / 20.0f;
            /* and a grey texel has no hue worth turning */
            w *= s <= 0.08f ? 0.0f : s >= 0.20f ? 1.0f : (s - 0.08f) / 0.12f;
        }
        if (w <= 0.0f) { q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; continue; }
        float s2 = s * sat;
        if (s2 > 1.0f) s2 = 1.0f;
        float r, g, b;
        tibcol_hsv2rgb(h + turn, s2, v, &r, &g, &b);
        const float o[3] = { r, g, b };
        for (int k = 0; k < 3; k++) {
            float c = p[k] / 255.0f + (o[k] - p[k] / 255.0f) * w;
            if (c < 0.0f) c = 0.0f;
            if (c > 1.0f) c = 1.0f;
            q[k] = (unsigned char)(c * 255.0f + 0.5f);
        }
    }
}

/* Once a frame: if the dials moved since the sheets were last uploaded, upload them
   again. Each upload replaces the texture's contents in place, so the texture names
   tib3d_mod.h handed out stay valid. */
static void tibcol_update(void)
{
    if (!g_tib3dHave || !g_tib3dTex || g_tib3dSheetPx.empty())
        return;
    if (g_tibcolDoneTex != g_tib3dTex) {      /* a fresh load holds the original art */
        g_tibcolDoneTex = g_tib3dTex;
        g_tibcolDoneIdentity = true;
        g_tibcolDoneHue = g_tibcolDoneSat = -1.0f;
    }
    const bool id = tibcol_is_identity();
    if (id && g_tibcolDoneIdentity)
        return;
    if (!id && !g_tibcolDoneIdentity &&
        tibcol_hue() == g_tibcolDoneHue && tibcol_sat() == g_tibcolDoneSat)
        return;
    g_tibcolDoneIdentity = id;
    g_tibcolDoneHue = tibcol_hue();
    g_tibcolDoneSat = tibcol_sat();

    std::vector<unsigned char> out(g_tib3dSheetPx.size());
    if (id) out = g_tib3dSheetPx;
    else    tibcol_repaint(&g_tib3dSheetPx[0], &out[0], out.size() / 4, true);
    glBindTexture(GL_TEXTURE_2D, g_tib3dTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)g_tib3dSheetW, (GLsizei)g_tib3dSheetH,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, &out[0]);
    if (g_tib3dHaveGround && g_tib3dGround && !g_tib3dGroundPx.empty()) {
        std::vector<unsigned char> gout(g_tib3dGroundPx.size());
        if (id) gout = g_tib3dGroundPx;
        else    tibcol_repaint(&g_tib3dGroundPx[0], &gout[0], gout.size() / 4, false);
        glBindTexture(GL_TEXTURE_2D, g_tib3dGround);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)g_tib3dGroundSide,
                     (GLsizei)g_tib3dGroundSide, 0, GL_RGBA, GL_UNSIGNED_BYTE, &gout[0]);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    float c[3];
    tibcol_apply_rgb(TIBG_BASE_R, TIBG_BASE_G, TIBG_BASE_B, c);
    fprintf(stderr, "FX|tibcolour|hue %.1f (crystals' own %.1f, turned %.1f) saturation %.2f; "
                    "glow colour %.2f %.2f %.2f%s\n",
            tibcol_hue(), tibcol_native(), tibcol_turn(), tibcol_sat(), c[0], c[1], c[2],
            id ? " (the original art)" : "");
}

/* ---- the glow field ----------------------------------------------------------------- */

static GLuint   g_tibfTex = 0;
static unsigned g_tibfSig = 0;
static bool     g_tibfSigOk = false;
static int      g_tibfCells = 0;          /* tiberium cells the last build counted       */
static float    g_tibfHBase = 0.0f, g_tibfHRange = 1.0f;

/* Live when Enhanced's chain is on and either the light or the fog wants the field. */
static bool tibf_wanted(void)
{
    if (!g_fx.enabled || fx_mode_of(&g_fx) != FX_MODE_ENHANCED)
        return false;
    const bool light = g_fx.lights_on && g_fx.light_tiberium && g_fx.tib_light > 0.0f;
    const bool fog = g_fx.tibfog_on && g_fx.tibfog_density > 0.0f;
    return light || fog;
}

/* A separable Gaussian over a TIBF_SIDE square, sigma in texels. Clamped at the map's
   edge: nothing glows past it. */
static void tibf_blur(std::vector<float>& f, float sigma)
{
    if (sigma < 0.3f) return;
    int rad = (int)ceilf(sigma * 3.0f);
    if (rad > TIBF_SIDE / 2) rad = TIBF_SIDE / 2;
    std::vector<float> k((size_t)rad * 2 + 1);
    float sum = 0.0f;
    for (int i = -rad; i <= rad; i++) {
        k[(size_t)(i + rad)] = expf(-(float)(i * i) / (2.0f * sigma * sigma));
        sum += k[(size_t)(i + rad)];
    }
    for (size_t i = 0; i < k.size(); i++) k[i] /= sum;
    std::vector<float> t(f.size(), 0.0f);
    for (int y = 0; y < TIBF_SIDE; y++)
        for (int x = 0; x < TIBF_SIDE; x++) {
            float a = 0.0f;
            for (int i = -rad; i <= rad; i++) {
                const int xx = x + i;
                if (xx < 0 || xx >= TIBF_SIDE) continue;
                a += f[(size_t)y * TIBF_SIDE + xx] * k[(size_t)(i + rad)];
            }
            t[(size_t)y * TIBF_SIDE + x] = a;
        }
    for (int y = 0; y < TIBF_SIDE; y++)
        for (int x = 0; x < TIBF_SIDE; x++) {
            float a = 0.0f;
            for (int i = -rad; i <= rad; i++) {
                const int yy = y + i;
                if (yy < 0 || yy >= TIBF_SIDE) continue;
                a += t[(size_t)yy * TIBF_SIDE + x] * k[(size_t)(i + rad)];
            }
            f[(size_t)y * TIBF_SIDE + x] = a;
        }
}

/* Once a frame, before the world is drawn. Rebuilds the field only when what it is made
   of has changed: the cells and their growth, how much of each the shroud shows, the
   two spreads, and the ground's height range. */
static void tibf_update(void)
{
    if (!tibf_wanted())
        return;
    unsigned sig = 2166136261u;
    #define TIBF_MIX(v) do { sig ^= (unsigned)(v); sig *= 16777619u; } while (0)
    int cells = 0;
    for (size_t i = 0; i < g_tib.size(); i++) {
        const TibCell& tc = g_tib[i];
        if (!cell_shown(tc.x, tc.y) || shroud_cell_hidden(tc.x, tc.y)) continue;
        const float vis = 0.25f * (shroud_corner_vis(tc.x,     tc.y) +
                                   shroud_corner_vis(tc.x + 1, tc.y) +
                                   shroud_corner_vis(tc.x,     tc.y + 1) +
                                   shroud_corner_vis(tc.x + 1, tc.y + 1));
        TIBF_MIX(tc.x); TIBF_MIX(tc.y); TIBF_MIX(tc.stage);
        TIBF_MIX((unsigned)(vis * 64.0f));
        cells++;
    }
    TIBF_MIX((unsigned)(g_fx.tib_light_spread * 100.0f));
    TIBF_MIX((unsigned)(g_fx.tibfog_spread * 100.0f));
    TIBF_MIX((unsigned)(g_terrainHTop * 100.0f));
    TIBF_MIX((unsigned)(g_terrainHBot * 100.0f));
    #undef TIBF_MIX
    if (g_tibfTex && g_tibfSigOk && sig == g_tibfSig)
        return;
    g_tibfSig = sig;
    g_tibfSigOk = true;
    g_tibfCells = cells;

    /* Each cell's weight: a thin cell glows a third as hard as a full one. The engine's
       stage is its density, 0..11, and it falls as the cell is harvested. */
    std::vector<float> src((size_t)TIBF_SIDE * TIBF_SIDE, 0.0f);
    for (size_t i = 0; i < g_tib.size(); i++) {
        const TibCell& tc = g_tib[i];
        if (tc.x < 0 || tc.y < 0 || tc.x >= TIBF_CELLS || tc.y >= TIBF_CELLS) continue;
        if (!cell_shown(tc.x, tc.y) || shroud_cell_hidden(tc.x, tc.y)) continue;
        const float vis = 0.25f * (shroud_corner_vis(tc.x,     tc.y) +
                                   shroud_corner_vis(tc.x + 1, tc.y) +
                                   shroud_corner_vis(tc.x,     tc.y + 1) +
                                   shroud_corner_vis(tc.x + 1, tc.y + 1));
        const float st = (float)(tc.stage > 11 ? 11 : tc.stage) / 11.0f;
        const float w = (0.35f + 0.65f * st) * vis;
        for (int dy = 0; dy < TIBF_RES; dy++)
            for (int dx = 0; dx < TIBF_RES; dx++)
                src[(size_t)(tc.y * TIBF_RES + dy) * TIBF_SIDE + tc.x * TIBF_RES + dx] = w;
    }
    std::vector<float> lit = src, fog = src;
    tibf_blur(lit, g_fx.tib_light_spread * (float)TIBF_RES * 0.5f);
    tibf_blur(fog, g_fx.tibfog_spread * (float)TIBF_RES * 0.5f);

    g_tibfHBase = g_terrainHBot;
    g_tibfHRange = g_terrainHTop - g_terrainHBot;
    if (g_tibfHRange < 0.01f) g_tibfHRange = 0.01f;

    std::vector<unsigned char> px((size_t)TIBF_SIDE * TIBF_SIDE * 4);
    for (int y = 0; y < TIBF_SIDE; y++)
        for (int x = 0; x < TIBF_SIDE; x++) {
            const size_t i = (size_t)y * TIBF_SIDE + x;
            /* the blur leaves a full interior at 1 and its edge at about a half; doubled
               then clamped, so a field's middle reads full and its edge is the soft half */
            float a = lit[i] * 1.6f, b = fog[i] * 1.6f;
            if (a > 1.0f) a = 1.0f;
            if (b > 1.0f) b = 1.0f;
            const float wx = ((float)x + 0.5f) / (float)TIBF_RES;
            const float wz = ((float)y + 0.5f) / (float)TIBF_RES;
            float h = (terrain_y(wx, wz) - g_tibfHBase) / g_tibfHRange;
            if (h < 0.0f) h = 0.0f;
            if (h > 1.0f) h = 1.0f;
            px[i * 4 + 0] = (unsigned char)(a * 255.0f + 0.5f);
            px[i * 4 + 1] = (unsigned char)(b * 255.0f + 0.5f);
            px[i * 4 + 2] = (unsigned char)(h * 255.0f + 0.5f);
            px[i * 4 + 3] = 255;
        }
    if (!g_tibfTex) {
        glGenTextures(1, &g_tibfTex);
        glBindTexture(GL_TEXTURE_2D, g_tibfTex);
        /* CLAMP_TO_EDGE: this texture is read by the Tier 2 light pass only. A sample
           past the map takes the border texel, which is the map edge's own value. */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindTexture(GL_TEXTURE_2D, g_tibfTex);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TIBF_SIDE, TIBF_SIDE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
    fprintf(stderr, "FX|tibglow|field rebuilt: %d cells shown, light spread %.2f, fog "
                    "spread %.2f cells\n", cells, g_fx.tib_light_spread, g_fx.tibfog_spread);
}

static void tibglow_frame(void)
{
    tibcol_update();
    tibf_update();
}

/* ---- the light pass's side ------------------------------------------------------------
   Forward-declared in fx_post.h and called with the light program bound. Both strengths
   are zero whenever the field is not wanted or not built, so the shader's two blocks are
   uniform branches that cost nothing. */
static void tibglow_light_uniforms(GLuint prog)
{
    const bool have = tibf_wanted() && g_tibfTex && g_tibfCells > 0;
    const bool light = have && g_fx.lights_on && g_fx.light_tiberium;
    const bool fog = have && g_fx.tibfog_on;
    float c[3];
    tibcol_apply_rgb(TIBG_BASE_R, TIBG_BASE_G, TIBG_BASE_B, c);
    fx_set1i(prog, "uTibField", 7);
    fx_set1f(prog, "uTibLight", light ? g_fx.tib_light : 0.0f);
    fx_set1f(prog, "uTibLightH", g_fx.tib_light_height > 0.05f ? g_fx.tib_light_height : 0.05f);
    fx_set1f(prog, "uTibFog", fog ? g_fx.tibfog_density : 0.0f);
    fx_set1f(prog, "uTibFogH", g_fx.tibfog_height > 0.05f ? g_fx.tibfog_height : 0.05f);
    fx_set1f(prog, "uTibFogGlow", g_fx.tibfog_glow);
    fx_set1f(prog, "uTibFogAmb", g_fx.tibfog_ambient);
    fx_set1f(prog, "uTibFogWisp", g_fx.tibfog_wisps);
    fx_set1i(prog, "uTibFogSteps", (int)(g_fx.tibfog_steps + 0.5f));
    fx_set1f(prog, "uTibFogDrift", g_fxTime * g_fx.tibfog_drift);
    fx_set1f(prog, "uTibPulse", g_fx.tib3d_pulse);
    fx_set1f(prog, "uTibTime", g_fxTime);
    fx_set3f(prog, "uTibCol", c[0], c[1], c[2]);
    fx_set2f(prog, "uTibH", g_tibfHBase, g_tibfHRange);
    fx_set1f(prog, "uTibInv", 1.0f / (float)TIBF_CELLS);
    /* the fog's ceiling: the highest ground plus three of its own heights, past which
       its density has fallen to a twentieth */
    fx_set1f(prog, "uTibFogTop", g_tibfHBase + g_tibfHRange + 3.0f * g_fx.tibfog_height);
}

static void tibglow_light_bind(void)
{
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, g_tibfTex);
    glActiveTexture(GL_TEXTURE0);
}

static void tibglow_light_unbind(void)
{
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
}

#endif /* OPENCNC3D_TIBGLOW_MOD_H */
