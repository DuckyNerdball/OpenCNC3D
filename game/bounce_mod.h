/* ====================================================================================
 *  bounce_mod.h -- the light the ground throws back up. Enhanced only, all of it OURS.
 *
 *  WHAT IT CHANGES. Every lit surface's ambient has an upper half, the sky, and a lower
 *  half, the ground. The lower half used to be one fixed grey: a fifth of the sky for
 *  the relit models and buildings, a fixed olive for the foliage, a fixed warm grey for
 *  the 3D soldiers. The same over white snow, orange sand and dark grass. With this on,
 *  the lower half is the light the ground under the surface actually reflects: its own
 *  colour times the sun and sky falling on it. A tank on sand gets a warm orange belly,
 *  a building on snow is lit from below, the eaves over grass go faintly green.
 *
 *  THE MAP. One texture over the whole map, two texels a cell, holding the ground's
 *  albedo softened over the spread dial (three quarters of a cell): the light a hull
 *  sees comes from the ground around it, not from the single cell under its centre, and
 *  the light pass reads it a cell out in front of each face. It is built on the
 *  CPU from data the renderer already has: each cell's mean colour off the terrain sheet
 *  that is actually drawn, taken at load for the radar (PackCell::avg, and Red Alert's
 *  own sheet through ra3d_radar_cell). The ground's colour grade (brightness, contrast,
 *  saturation and the biome balance the light pass applies to terrain pixels) is applied
 *  here too, so the bounce agrees with the ground on screen. The blur is done on LINEAR
 *  light, decoded from sRGB, which is where averaging light belongs; the result is
 *  stored sRGB-encoded again, which keeps eight bits enough for dark grass.
 *
 *  WHAT READS IT.
 *    the light pass (fx_post.h, BOUNCE_GLSL): relit models, remastered buildings and
 *      vehicles, remastered trees, untextured faces. Per pixel, under the pixel's own
 *      world position, with the cloud over it taken as the cloud over the ground. It
 *      is a second build of the pass with the bounce compiled in, so with the switch
 *      off the pass runs exactly the code it ran before (bounce_light_select).
 *    the foliage (TREE_FS in cnc_eyes.cpp): per pixel, the same texture.
 *    the 3D soldiers and authored models (model3d_mod.h): once per draw, on the CPU,
 *      from the copy of the map this file keeps.
 *  The terrain's own lighting is not reached: a slope already lights by its own normal
 *  and its own sky, and a valley lit by itself would only lift contrast that was tuned.
 *  (The bloom still spreads a brighter wall's glow a level or two onto the ground
 *  beside it.)
 *
 *  THE UNITS. What the ground throws up is its colour times the light falling on it:
 *  sun on level ground plus sky on level ground, over the sky part alone. That product
 *  is divided by the same product for a reference ground, BOUNCE_REF, which is Tiberian
 *  Dawn's temperate grass under its own daylight, the ground every fixed fill below was
 *  tuned over. So T, the ground's light over the reference's, is about 1 on grass, 3
 *  to 4 on snow and on sand (and orange there), and well under 1 on water. What the
 *  ground adds is then the SAME light for every reader, against its own sky:
 *
 *      lower = old + strength x 0.2 x grey(sky) x (T - 1)
 *
 *  0.2 being the relit models' old ground, a fifth of their sky. So over grass every
 *  reader keeps the lower half it was tuned with (only its hue leans green), and over
 *  sand or snow each gains the same light, whatever fill it was given by eye before:
 *  the foliage's old ground is half its sky, and scaling that by T rather than adding
 *  to it turned a desert bush orange.
 *
 *  LINEAR LIGHT. Today's light pass multiplies gamma-encoded colour by its light, and
 *  the ground on screen is its sRGB colour times its light. T is taken the same way, off
 *  the sRGB albedo, so the bounce is in the same convention as everything beside it,
 *  and in it a ratio reads stronger than the same ratio of linear light. THE STRENGTH
 *  DEFAULT, 0.75, is set by linear light: a shaded wall sees half sky and half ground,
 *  so on linear light its irradiance is 0.5 + 0.5 x albedo x daylight, in units of the
 *  level sky. Over the maps' own decoded ground that wall comes out about 1.75 times
 *  as bright over snow and 1.67 times over sand as over grass, which on screen (the
 *  2.2 power) is 29% and 26% brighter. The rule above gives a wall (half its lower
 *  half, against 0.6 of the sky) those same lifts at a strength of 0.74 on snow and
 *  0.72 on sand. 1 is a third more than that.
 *  WITH linear_on the light pass lights radiance and builds its bounce separately
 *  (BOUNCE_LINEAR in fx_post.h): the map is decoded on read, the ground's light is its
 *  decoded albedo times its daylight converted as the pass converts the ground's own
 *  light, the reference is BOUNCE_REF_LIN_ALBEDO under the same conversion, and the sum
 *  is radiance added to the surface's light, not a ratio of gamma colour. The same dial
 *  at the same 0.75 then gives about the gamma build's lift: measured over the shot
 *  set, a little more on the deepest shade over snow, less on sunlit faces, where in
 *  radiance a small addition to a bright face barely shows. The foliage and the 3D
 *  soldiers light in their own shaders before the light pass and write display colour
 *  in both modes, so their share of the bounce is the same with linear light on or off.
 *
 *  REBUILT when the cells' colours, the map's size or a dial that colours the ground
 *  changes, found by hashing those each frame (a few thousand bytes). Not on time:
 *  two runs of one script build the same map on the same frames.
 *
 *  KNOWN GAPS. The concrete under buildings (pads and bibs), tiberium, scorch marks and
 *  water's own reflection are not in the cell colours, so they do not tint the bounce.
 *  The ground's light ignores the shadows of the objects standing on it.
 * ==================================================================================== */
#ifndef OPENCNC3D_BOUNCE_MOD_H
#define OPENCNC3D_BOUNCE_MOD_H

static bool write_png(const char* path, int w, int h, const unsigned char* rgb);

/* Two texels a cell, so a one-cell patch of road blurs round rather than square; a map
   wider than 128 cells drops to one. */
enum { BOUNCE_RES = 2, BOUNCE_MAX_SIDE = 256 };
/* The grey the lower hemisphere held before, relative to the sky above it: the relight's
   0.6 + 0.4 n.y is a sky of 1 and a ground of 0.2. */
static const float BOUNCE_OLD_GREY = 0.2f;
/* THE REFERENCE GROUND: Tiberian Dawn's temperate grass, its graded sRGB luminance
   (0.20, measured over the base mission's whole map) times its daylight ratio (about 2:
   the sun at 31 degrees and 0.96 over a level-ground sky of 0.55, plus the second sun). */
static const float BOUNCE_REF = 0.40f;
/* THE SAME GROUND WITH LINEAR LIGHT ON: the luminance of its decoded albedo, measured
   over the same map as the mean of each cell's decoded colour, and the daylight ratio
   inside BOUNCE_REF. The light pass's linear build measures a ground against their
   product with the ratio raised to linear_light_k, as it raises the ground's own light
   (BOUNCE_GLSL in fx_post.h). */
static const float BOUNCE_REF_LIN_ALBEDO = 0.0363f;
static const float BOUNCE_REF_DAYLIGHT = 2.0f;

static GLuint   g_bounceTex = 0;
static unsigned g_bounceSig = 0;
static bool     g_bounceSigOk = false;
static int      g_bounceCellsW = 0, g_bounceCellsH = 0;   /* cells the map spans   */
static int      g_bounceW = 0, g_bounceH = 0, g_bounceRes = BOUNCE_RES;
static std::vector<float> g_bounceAlb;   /* sRGB albedo per texel, the CPU's copy */

static bool bounce_wanted(void)
{
    return g_fx.enabled && !g_fx.dos_classic && fx_mode_of(&g_fx) == FX_MODE_ENHANCED
        && g_fx.bounce_on && fx_unified_lighting();
}

/* The mission is over: the map's texture goes back with the chain's own targets, and the
   next mission builds its own (the signature test is what decides that). */
static void bounce_shutdown(void)
{
    if (g_bounceTex) glDeleteTextures(1, &g_bounceTex);
    g_bounceTex = 0;
    g_bounceSigOk = false;
}

/* The map is there and the switch is on: what every reader asks first. */
static bool bounce_live(void)
{
    return bounce_wanted() && g_bounceTex && !g_bounceAlb.empty();
}

static float bounce_srgb_to_linear(float c)
{
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}
static float bounce_linear_to_srgb(float c)
{
    if (c <= 0.0f) return 0.0f;
    if (c >= 1.0f) return 1.0f;
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

/* THE LIGHT ON LEVEL GROUND OVER ITS SKY PART, exactly as the light pass lights level
   ground under unified daylight: the sky is terrain_ambient x sky fill, the sun is its
   energy times the sine of its height, the second sun likewise and unshadowed. */
static void bounce_ground_light(float* sunR, float* sun2R)
{
    fx_sun_dir();
    float sky = g_fx.terrain_ambient * fx_sky_energy();
    if (sky < 0.05f) sky = 0.05f;
    const float up1 = g_fxSunDir[1] < 0.0f ? -g_fxSunDir[1] : 0.0f;
    const float up2 = g_fxSecondSunDir[1] < 0.0f ? -g_fxSecondSunDir[1] : 0.0f;
    *sunR = fx_sun_energy() * up1 / sky;
    *sun2R = fx_second_sun_energy() * up2 / sky;
}

/* A separable Gaussian over a w x h RGB float field, sigma in texels. Clamped at the
   map's edge by renormalising the kernel, so the border keeps its own colour rather
   than fading toward black. */
static void bounce_blur(std::vector<float>& f, int w, int h, float sigma)
{
    if (sigma < 0.3f) return;
    int rad = (int)ceilf(sigma * 3.0f);
    if (rad > 32) rad = 32;
    std::vector<float> k((size_t)rad * 2 + 1);
    for (int i = -rad; i <= rad; i++)
        k[(size_t)(i + rad)] = expf(-(float)(i * i) / (2.0f * sigma * sigma));
    std::vector<float> t(f.size(), 0.0f);
    for (int pass = 0; pass < 2; pass++) {
        const std::vector<float>& src = pass ? t : f;
        std::vector<float>& dst = pass ? f : t;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float a[3] = { 0.0f, 0.0f, 0.0f }, ws = 0.0f;
                for (int i = -rad; i <= rad; i++) {
                    const int xx = pass ? x : x + i, yy = pass ? y + i : y;
                    if (xx < 0 || xx >= w || yy < 0 || yy >= h) continue;
                    const float kw = k[(size_t)(i + rad)];
                    const float* s = &src[((size_t)yy * w + xx) * 3];
                    a[0] += s[0] * kw; a[1] += s[1] * kw; a[2] += s[2] * kw;
                    ws += kw;
                }
                float* d = &dst[((size_t)y * w + x) * 3];
                d[0] = a[0] / ws; d[1] = a[1] / ws; d[2] = a[2] / ws;
            }
    }
}

/* Once a frame, before the world is drawn. */
static void bounce_frame(void)
{
    if (!bounce_wanted())
        return;
    const int CW = g_gridW > 0 ? g_gridW : 64, CH = g_gridH > 0 ? g_gridH : 64;
    /* Kept between frames: this runs every frame and only rarely finds a change. */
    static std::vector<unsigned char> cell, have;
    cell.assign((size_t)CW * CH * 3, 0);
    have.assign((size_t)CW * CH, 0);
    if (ra3d_radar_on()) {
        /* EVERY CELL OF THE MAP IS PAINTED, black ones included: an indoor mission's
           pits are cells whose painting is black, and black is what they throw back.
           Only the margin past the map's own rectangle is left to the fill below; a
           pit taken for the margin lit the walls over it as if a floor lay beneath. */
        const int n = ra3d_radar_count();
        for (int i = 0; i < n; i++) {
            int x, y; unsigned char rgb[3];
            ra3d_radar_cell(i, &x, &y, rgb);
            if (x < 0 || y < 0 || x >= CW || y >= CH) continue;
            const size_t c = (size_t)y * CW + x;
            memcpy(&cell[c * 3], rgb, 3);
            have[c] = 1;
        }
    } else {
        const int ts = terrain_texset_drawn();
        for (size_t i = 0; i < g_pack.cell.size(); i++) {
            const PackCell& pc = g_pack.cell[i];
            if (pc.x < 0 || pc.y < 0 || pc.x >= CW || pc.y >= CH) continue;
            const size_t c = (size_t)pc.y * CW + pc.x;
            const unsigned char* a = pc.avg[(ts >= 0 && ts < 4) ? ts : 0];
            /* A cell with any water in it is half sea, which reflects little: the radar
               paints such a cell as water outright, this only leans it that way. */
            static const unsigned char WATER[3] = { 28, 51, 74 };
            for (int k = 0; k < 3; k++)
                cell[c * 3 + k] = pc.holes ? (unsigned char)((a[k] + WATER[k] + 1) / 2) : a[k];
            have[c] = 1;
        }
    }

    const bool snow = fx_snow_terrain(), desert = fx_desert_terrain();
    const float bright = g_fx.terrain_brightness * (snow ? g_fx.snow_brightness : desert ? g_fx.sand_brightness : 1.0f);
    const float contrast = g_fx.terrain_contrast * (snow ? g_fx.snow_contrast : desert ? g_fx.sand_contrast : 1.0f);
    const float sat = snow ? g_fx.snow_saturation : 1.0f;
    float bal[3];
    fx_terrain_colour_balance(bal);

    unsigned sig = 2166136261u;
    #define BOUNCE_MIX(v) do { sig ^= (unsigned)(v); sig *= 16777619u; } while (0)
    BOUNCE_MIX(CW); BOUNCE_MIX(CH);
    for (size_t i = 0; i < cell.size(); i++) BOUNCE_MIX(cell[i]);
    for (size_t i = 0; i < have.size(); i++) BOUNCE_MIX(have[i]);
    BOUNCE_MIX((unsigned)(bright * 1000.0f)); BOUNCE_MIX((unsigned)(contrast * 1000.0f));
    BOUNCE_MIX((unsigned)(sat * 1000.0f));
    BOUNCE_MIX((unsigned)(bal[0] * 1000.0f)); BOUNCE_MIX((unsigned)(bal[1] * 1000.0f));
    BOUNCE_MIX((unsigned)(bal[2] * 1000.0f));
    BOUNCE_MIX((unsigned)(g_fx.bounce_spread * 100.0f));
    #undef BOUNCE_MIX
    if (g_bounceTex && g_bounceSigOk && sig == g_bounceSig)
        return;
    g_bounceSig = sig;
    g_bounceSigOk = true;

    /* Each cell's colour graded the way the light pass grades a terrain pixel, then
       decoded to linear light. A cell outside the map's rectangle (the margin, which
       nothing paints) takes the mean of the cells that are painted, so the edge does not
       darken toward nothing. */
    std::vector<float> lin((size_t)CW * CH * 3, 0.0f);
    double mean[3] = { 0.0, 0.0, 0.0 };
    long nHave = 0;
    for (size_t c = 0; c < have.size(); c++) {
        if (!have[c]) continue;
        float g[3] = { cell[c * 3] / 255.0f, cell[c * 3 + 1] / 255.0f, cell[c * 3 + 2] / 255.0f };
        if (sat != 1.0f) {
            const float l = 0.299f * g[0] + 0.587f * g[1] + 0.114f * g[2];
            for (int k = 0; k < 3; k++) g[k] = l + (g[k] - l) * sat;
        }
        for (int k = 0; k < 3; k++) {
            float v = g[k] * bright;
            v = (v - 0.5f) * contrast + 0.5f;
            v *= bal[k];
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            lin[c * 3 + k] = bounce_srgb_to_linear(v);
            mean[k] += lin[c * 3 + k];
        }
        nHave++;
    }
    for (int k = 0; k < 3; k++) mean[k] = nHave ? mean[k] / (double)nHave : 0.1;
    for (size_t c = 0; c < have.size(); c++)
        if (!have[c])
            for (int k = 0; k < 3; k++) lin[c * 3 + k] = (float)mean[k];

    int res = BOUNCE_RES;
    while (res > 1 && (CW * res > BOUNCE_MAX_SIDE || CH * res > BOUNCE_MAX_SIDE)) res--;
    const int W = CW * res, H = CH * res;
    std::vector<float> f((size_t)W * H * 3);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            memcpy(&f[((size_t)y * W + x) * 3], &lin[((size_t)(y / res) * CW + x / res) * 3], 3 * sizeof(float));
    bounce_blur(f, W, H, g_fx.bounce_spread * (float)res);

    std::vector<unsigned char> px((size_t)W * H * 4);
    double meanS = 0.0;
    for (size_t i = 0; i < (size_t)W * H; i++) {
        for (int k = 0; k < 3; k++)
            px[i * 4 + k] = (unsigned char)(bounce_linear_to_srgb(f[i * 3 + k]) * 255.0f + 0.5f);
        px[i * 4 + 3] = 255;
        meanS += 0.2126 * px[i * 4] + 0.7152 * px[i * 4 + 1] + 0.0722 * px[i * 4 + 2];
    }
    meanS /= 255.0 * (double)((size_t)W * H);
    /* The CPU's copy holds what the card holds, quantised the same way, so a soldier and
       the hull beside him read the same ground. */
    g_bounceAlb.resize((size_t)W * H * 3);
    for (size_t i = 0; i < (size_t)W * H; i++)
        for (int k = 0; k < 3; k++) g_bounceAlb[i * 3 + k] = px[i * 4 + k] / 255.0f;
    g_bounceW = W; g_bounceH = H; g_bounceRes = res;
    g_bounceCellsW = CW; g_bounceCellsH = CH;

    if (!g_bounceTex) {
        glGenTextures(1, &g_bounceTex);
        glBindTexture(GL_TEXTURE_2D, g_bounceTex);
        /* CLAMP_TO_EDGE: a sample past the map takes the border texel, the map edge's
           own ground. */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindTexture(GL_TEXTURE_2D, g_bounceTex);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
    /* With it, the daylight on level ground over the sky's share (cloud left out) and
       what the mean ground throws up against the reference's: T in the rule above. */
    float sunR, sun2R;
    bounce_ground_light(&sunR, &sun2R);
    fprintf(stderr, "FX|bounce|ground map rebuilt: %dx%d cells (%s), %ld painted, %dx%d texels, "
                    "spread %.2f cells, mean albedo %.3f %.3f %.3f linear, luminance %.3f sRGB, "
                    "daylight %.2f of the sky, T %.2f, strength %.2f\n",
            CW, CH, ra3d_radar_on() ? "Red Alert sheet" : "pack cells", nHave, W, H,
            g_fx.bounce_spread, mean[0], mean[1], mean[2], meanS, 1.0f + sunR + sun2R,
            meanS * (1.0f + sunR + sun2R) / BOUNCE_REF, g_fx.bounce_strength);
    /* OPENCNC3D_BOUNCE_DUMP=<file.png>: the map as it was built, sRGB again for viewing,
       north up as the radar draws it. A check that it lines up with the ground. */
    if (const char* dump = getenv("OPENCNC3D_BOUNCE_DUMP")) {
        std::vector<unsigned char> rgb((size_t)W * H * 3);
        for (size_t i = 0; i < (size_t)W * H; i++)
            for (int k = 0; k < 3; k++) rgb[i * 3 + k] = px[i * 4 + k];
        write_png(dump, W, H, &rgb[0]);
    }
}

/* The blurred sRGB albedo under a world position in cells, bilinear as the card
   filters it. */
static void bounce_albedo_at(float wx, float wz, float out[3])
{
    out[0] = out[1] = out[2] = 0.0f;
    if (g_bounceAlb.empty() || g_bounceW <= 0 || g_bounceH <= 0) return;
    const float fx = wx * (float)g_bounceRes - 0.5f, fz = wz * (float)g_bounceRes - 0.5f;
    int x0 = (int)floorf(fx), z0 = (int)floorf(fz);
    const float tx = fx - (float)x0, tz = fz - (float)z0;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            int x = x0 + i, z = z0 + j;
            if (x < 0) x = 0;
            if (x >= g_bounceW) x = g_bounceW - 1;
            if (z < 0) z = 0;
            if (z >= g_bounceH) z = g_bounceH - 1;
            const float w = (i ? tx : 1.0f - tx) * (j ? tz : 1.0f - tz);
            const float* s = &g_bounceAlb[((size_t)z * g_bounceW + x) * 3];
            out[0] += s[0] * w; out[1] += s[1] * w; out[2] += s[2] * w;
        }
}

/* ---- the light pass's side, forward-declared in fx_post.h --------------------------
   The pass has a build with BOUNCE_LIGHT defined, for the gamma pass, and one with
   BOUNCE_LINEAR as well, for linear light (fx_post.h says why builds and not a branch),
   each with and without GTAO: fx_light_variant makes and keeps them, each the first time
   it is wanted (a mission's first frame; a player who never turns the bounce on never
   pays for any). g_fxPLight is pointed at the one this frame draws with, just before the
   pass uses it and after g_fxLinLive is settled for the frame; with the bounce off it
   stays the build the pass already chose, so the pass runs exactly the code it ran
   before. */
static void bounce_light_select(int gtao)
{
    if (!g_fxPLight || !bounce_live()) return;
    const GLuint p = fx_light_variant(gtao, g_fxLinLive ? 2 : 1);
    if (p) g_fxPLight = p;
}

/* The pass is drawing with the bounce compiled in, in any build. */
static bool bounce_light_active(void)
{
    return fx_light_variant_bounce(g_fxPLight) != 0;
}

static void bounce_light_uniforms(GLuint prog)
{
    if (!bounce_light_active()) return;
    fx_set1i(prog, "uBounceMap", 11);
    fx_set2f(prog, "uBounceXf", 1.0f / (float)g_bounceCellsW, 1.0f / (float)g_bounceCellsH);
    float sunR, sun2R;
    bounce_ground_light(&sunR, &sun2R);
    const float e[4] = { sunR, sun2R, g_fx.bounce_strength * BOUNCE_OLD_GREY, 1.0f / BOUNCE_REF };
    fx_set4fv(prog, "uBounceE", 1, e);
    if (fx_light_variant_bounce(prog) == 2)
        fx_set2f(prog, "uBounceL", g_fx.bounce_strength,
                 BOUNCE_REF_LIN_ALBEDO * powf(BOUNCE_REF_DAYLIGHT, g_fx.linear_light_k));
}

static void bounce_light_bind(void)
{
    if (!bounce_light_active()) return;
    glActiveTexture(GL_TEXTURE0 + 11);
    glBindTexture(GL_TEXTURE_2D, g_bounceTex);
    glActiveTexture(GL_TEXTURE0);
}

static void bounce_light_unbind(void)
{
    if (!bounce_light_active()) return;
    glActiveTexture(GL_TEXTURE0 + 11);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
}

/* The daylight ratio of level ground with no cloud mask to hand: the deck's mean cover
   takes its share off the sun, so a cloudy map bounces a little less everywhere. */
static float bounce_mean_ratio(void)
{
    float sunR, sun2R;
    bounce_ground_light(&sunR, &sun2R);
    const float cloud = g_fx.cloud_on ? fx_clampf(g_fx.cloud_cover, 0.0f, 1.0f) * g_fx.cloud_strength : 0.0f;
    return 1.0f + sunR * (1.0f - cloud) + sun2R;
}

static float bounce_grey(const float c[3])
{
    return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

/* ---- the foliage's side (TREE_FS), forward-declared in foliage3d_mod.h -------------
   The map on `unit`, its transform, and k for the shader's
       ground = uGndCol + k.x * (albedo * k.y - 1)
   which is the module's rule against the plants' own sky colour: k.x the strength times
   the old grey times that sky's grey, k.y the daylight ratio over the reference
   ground's. Returns false, and binds nothing, when off. */
static bool bounce_foliage(int unit, const float skyCol[3], float xf[4], float k[3])
{
    if (!bounce_live()) return false;
    k[0] = g_fx.bounce_strength * BOUNCE_OLD_GREY * bounce_grey(skyCol);
    k[1] = bounce_mean_ratio() / BOUNCE_REF;
    k[2] = 0.0f;
    xf[0] = 1.0f / (float)g_bounceCellsW; xf[1] = 1.0f / (float)g_bounceCellsH; xf[2] = xf[3] = 0.0f;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, g_bounceTex);
    glActiveTexture(GL_TEXTURE0);
    return true;
}

/* ---- the 3D soldiers' side (model3d_mod.h) ---------------------------------------
   The change to the lower half of a model's ambient at (wx, wz), in its own units:
   skyUp is what its ambient gives a face turned straight up. The shader adds it
   weighted by how far a face looks down. False when off. */
static bool bounce_model_delta(float wx, float wz, const float skyUp[3], float out[3])
{
    out[0] = out[1] = out[2] = 0.0f;
    if (!bounce_live()) return false;
    float alb[3];
    bounce_albedo_at(wx, wz, alb);
    const float e = bounce_mean_ratio() / BOUNCE_REF;
    const float g = g_fx.bounce_strength * BOUNCE_OLD_GREY * bounce_grey(skyUp);
    for (int k = 0; k < 3; k++) out[k] = g * (alb[k] * e - 1.0f);
    return true;
}

#endif /* OPENCNC3D_BOUNCE_MOD_H */
