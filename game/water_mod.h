/* ====================================================================================
 *  water_mod.h -- THE SEA'S SURFACE UNDER ENHANCED: a shore field, a flow and foam.
 *
 *  TIER 2 ONLY, AND OURS. The cartridge's water is two 32x32 tiles multiplied together
 *  and slid over the terrain's own corners (draw_water, and the decode above it); that
 *  pass is untouched and is still what CLASSIC draws, what Win98 draws, and what draws
 *  here the moment anything in this file cannot run. Nothing below is decoded from the
 *  ROM. It is registered with the known gaps as an authored addition, the same standing
 *  as the sun, the clouds and the occlusion.
 *
 *  WHAT IT ADDS, in the order the complaint came:
 *
 *   1. A SHORE FIELD. The shoreline is not a cell edge: it is the alpha hole punched
 *      in the terrain tile art at texel resolution, and the rocks standing in the sea
 *      are islands in it. At load the cartridge atlas (kept as g_pack.terrainCart) is
 *      sampled through every holed cell's own UV rectangle into a map-wide mask, and
 *      a Euclidean distance transform turns that into a SIGNED distance to the nearest
 *      coast, positive on the water side. It goes up as one RGBA texture:
 *          R  signed distance, 0.5 on the coast, +-WATER_FIELD_RANGE cells at 1 / 0
 *          GB the direction out to sea (the blurred gradient), 0.5 = no direction
 *          A  1 on water, 0 on land
 *      Bilinear sampling of R crosses the coast continuously, so a band drawn at
 *      "sd < 0.7 cells" hugs the art's own edge and rings every rock.
 *
 *   2. A FLOW. The tiles no longer slide as one sheet. Each pixel's UV is pushed
 *      along a flow vector: the current's bearing out at sea, turned toward the coast
 *      inside the foam band, which is what a wave does. Pushing a UV along a field
 *      that varies in space stretches the texture, so the push is done twice half a
 *      cycle apart and the two are cross-faded (the two-phase flow-map trick), which
 *      keeps the stretch bounded and the seam invisible.
 *
 *   3. FOAM AND SHALLOWS. Inside the band, crests roll toward the coast at a fixed
 *      spacing, broken up by the water art itself so no two stretches foam alike, and
 *      a thin wet rim sits right on the edge. The wash thins over the shallows so the
 *      sea floor shows through, and darkens slowly out in deep water.
 *
 *  DETERMINISM. The only clock is g_fxTime, the post chain's, which the host sets from
 *  the ENGINE frame: two --shot runs of the same script are the same picture. The field
 *  is built once per pack from data the pack already carries.
 *
 *  EVERY NUMBER IS A DIAL in fx_state.h ("3c  WATER  (ours)"); water_fx 0 draws the
 *  cartridge's pass through the same binary, which is what the gate uses as its trap.
 *
 *  TWO GROUNDS, ONE SEA. Everything above reads a SOURCE rather than the pack directly:
 *  the cells (their atlas rectangles and which are holed), the cut those holes come from,
 *  the sheet the ground draws (for the land under the water), and the sea's base texture.
 *  Under Tiberian Dawn the source is the pack, exactly as before: the cartridge atlas's
 *  alpha is the cut, WATER1 x WATER2 the base. Under the Red Alert module's Enhanced
 *  ground (ra3d_terrain.h) the source is Red Alert's map: its cut is read off the 1995
 *  tiles' palette indices, the sheet is Red Alert's own, and the base is the painting
 *  Red Alert's ground draws for its open water (W2, or W1), alone, sampled at its own
 *  scale (WATER_RA_BASE below). Every dial, the field, the foam, the waves and the
 *  reflection are the same code for both. What differs on Red Alert's ground, each
 *  because its art is a painting rather than a cut tile:
 *   - the floor under the sea is the painted ground itself, not the cartridge's BOTTOM;
 *   - the soft coast fades the painted water out into the sea rather than straddling
 *     the cut, and the wash and foam lie only where the ground lets the sea through;
 *   - THE HUE IS THE PAINTING'S. water_natural (the ease of the cartridge's cobalt toward
 *     a greyer, greener sea) is compiled out of the Red Alert variant: on Red Alert's
 *     already grey-blue painting it turned the sea teal. Its dial still drives Tiberian
 *     Dawn's sea exactly as before; the darkening, the depth, the rim, the reflection and
 *     the rain's grey change no hue and apply to both;
 *   - where the 1995 cut says sea over a painting's land, the painting decides: a floor
 *     that is not blue against its own brightness is left as painted, and the pale rim
 *     fades out where the painted water is not blue (WATER_FS, SHORE_FS).
 * ==================================================================================== */
#ifndef OPENCNC3D_WATER_MOD_H
#define OPENCNC3D_WATER_MOD_H

#include "rain_glsl.h"

/* ---------------------------------------------------------------------------------- *
 *  THE SOURCE. Red Alert's ground fills g_waterRa when it builds its sheet; Tiberian
 *  Dawn's is the pack and needs nothing filled. water_src_ra() decides per call, and it
 *  is the same test that decides whose ground draw_frame lays.
 * ---------------------------------------------------------------------------------- */
struct WaterRaSource {
    bool ok = false;                    /* a sea can be drawn on this map             */
    std::vector<PackCell> cell;         /* one per map cell, holed where it has water */
    /* THE CUT, at the 1995 tile's 24 texels a cell, laid out exactly as the ground's
       sheet is at 1/k of its size (so a cell's normalised rectangle addresses both):
       0 water, 255 land, one byte a texel. */
    std::vector<unsigned char> cut;
    int cutW = 0, cutH = 0;
    PackTex sheet = PackTex();          /* the sheet the ground draws: gl, w, h, uw, uh */
    int sheetKey = -1;                  /* stands in for terrain_atlas_index()        */
    GLuint base = 0;                    /* the open water's painting, GL_REPEAT       */
    GLuint white = 0;                   /* the second layer's stand-in: one white texel */
    float baseRepeats = 0.5f;           /* repeats of the base a cell                 */
    float grainMean = 0.5f, grainInv = 2.0f;
    int wetCells = 0;
};
static WaterRaSource g_waterRa;
static inline bool water_src_ra(void) { return ra3d_own_ground(); }
/* Red Alert's ground for the mirrored pass (ra3d_terrain.h, included further down). */
static void ra3d_reflect_ground(void);

static const std::vector<PackCell>& water_cells(void)
{
    return water_src_ra() ? g_waterRa.cell : g_pack.cell;
}
/* The atlas the ground is drawing, as an index (Tiberian Dawn's own; Red Alert's stands in
   for one) and as the texture behind it. */
static int water_atlas_index(void)
{
    return water_src_ra() ? g_waterRa.sheetKey : terrain_atlas_index();
}
static const PackTex& water_atlas(int ai)
{
    return water_src_ra() ? g_waterRa.sheet : g_pack.tex[ai];
}
/* THE BASE. Red Alert's painting is one texture repeating every 1 / baseRepeats cells and
   is the sea's colour by itself, so it takes the first unit and the second layer
   multiplies by white. WATER_RA_BASE compiles the shaders' Red Alert variant: the motion
   that slides the base is scaled to the base's own repeat, so the sea moves as far on the
   ground per second as Tiberian Dawn's does. */
static GLuint water_base1(void) { return water_src_ra() ? g_waterRa.base : g_pack.tex[g_pack.waterTex1].gl; }
static GLuint water_base2(void) { return water_src_ra() ? g_waterRa.white : g_pack.tex[g_pack.waterTex2].gl; }
/* The level ground's light, 160/255 (terrain_shade's flat value), which a painting made
   to be drawn as ground is seen under. */
#define WATER_RA_BASE_GAIN (160.0f / 255.0f)
static float  water_base_repeats(void) { return water_src_ra() ? g_waterRa.baseRepeats : WATER_REPEATS_PER_CELL; }

/* THE RAIN'S SIDE OF THE SEA, defined in rain_mod.h (included by the host right after
   this file). Forward-declared the way draw_terrain declares this file's own hooks. */
static void  rain_wave_uniforms(GLuint prog, int step);
static void  rain_water_uniforms(GLuint prog);
static float rain_overcast(void);

/* Texels of shore field per world cell, at most. A 64-cell map gets 12 (the tile art is
   24 to the cell, so the coast is placed to half a texel); bigger maps halve it until
   the field is no wider than WATER_FIELD_MAXW, so an XL map cannot ask for a texture
   the driver will refuse. */
#define WATER_FIELD_RES    12
#define WATER_FIELD_MAXW   1536
/* THE LARGEST TERRAIN SHEET THE COAST IS BUILT FROM (the land under the water and the
   soft coast both read the whole sheet back once per map). It was 4096 x 4096, which every
   sheet fitted until the Enhanced terrain: 144 texels a cell makes a sheet 4212 to 4992
   wide, and over the limit the coast quietly had neither the sand under the water nor the
   fade. 8192 x 8192 holds them with room; the read-back is a temporary copy of 75 to 97 MB
   for the Enhanced sheets, and the soft coast keeps a second sheet of that size. */
#define WATER_ATLAS_MAX_TEXELS (8192L * 8192L)

/* The distance the field can express, in cells, either side of the coast. */
#define WATER_FIELD_RANGE  4.0f

static GLuint g_waterFieldTex = 0;
/* THE NEAREST LAND'S COLOUR, per field texel, read from the atlas that is drawn: what
   the sand under the water looks like, wherever the coast is. */
static GLuint g_waterLandTex = 0;
static int    g_waterFieldAtlas = -1;      /* terrain_atlas_index() it was read from */
static GLuint g_waterFieldAtlasGL = 0;
static int    g_waterFieldW = 0, g_waterFieldH = 0, g_waterFieldRes = 0;
static char   g_waterFieldScen[20] = "";
static int    g_waterFieldRect[4] = { -1, -1, -1, -1 };   /* the play rectangle it was built for */
static int    g_waterFieldWet = 0, g_waterFieldShore = 0, g_waterFieldCells = 0, g_waterFieldRiver = 0;
static float  g_waterFieldBearing = -1.0f, g_waterFieldRiverW = -1.0f;   /* the dials the rivers were run with */
static float  g_waterGrainMean = 0.5f, g_waterGrainInv = 2.0f, g_waterMean2 = 0.5f;
/* 1 for a cell that is holed or touches a holed cell: the cells the shore pass draws. */
static std::vector<unsigned char> g_waterCoastCell;
/* CPU acoustic map: 0 dry, 1 ocean/lake, 2 river, from the same wet/component mask. */
struct WaterAudioSource { float x, z; bool river; };
static std::vector<unsigned char> g_waterAudioClass;
static std::vector<WaterAudioSource> g_waterAudioSources;
/* THE FOAM ITSELF: one tileable RGBA texture, generated from the same value noise the
   cloud deck uses (fx_cloud.h), never shipped. R is the broad wisps (base lattice 4),
   G the finer ones (base 8), two octaves each, both histogram-equalised so a threshold
   on them means a coverage. Built once per process; nothing a dial does can
   invalidate it. */
static GLuint g_waterFoamTex = 0;
static GLuint g_waterFoamGreyTex = 0;
#define WATER_FOAM_SIZE 256
static void water_foam_boot(void)
{
    if (g_waterFoamTex) return;
    const int N = WATER_FOAM_SIZE;
    std::vector<float> a((size_t)N * N), b((size_t)N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = ((float)x + 0.5f) / (float)N, v = ((float)y + 0.5f) / (float)N;
            /* a light domain warp so the wisps curl instead of blobbing */
            const float wx = fxc_fbm(1, u, v, 2, 2, 4451u), wy = fxc_fbm(1, u, v, 2, 2, 6229u);
            /* TWO octaves, not four: a threshold on a field with fine octaves in it
               traces their contours as hairlines, and the first sheet read as
               scratches. Two leave soft blobs, which is what foam is. */
            a[(size_t)y * N + x] = fxc_fbm(0, u + wx * 0.10f, v + wy * 0.10f, 4, 2, 3301u);
            b[(size_t)y * N + x] = fxc_fbm(0, u, v, 8, 2, 7013u);
        }
    fxc_equalise(&a[0], N * N);
    fxc_equalise(&b[0], N * N);
    std::vector<unsigned char> px((size_t)N * N * 4);
    for (size_t i = 0; i < (size_t)N * N; i++) {
        px[i * 4 + 0] = (unsigned char)(a[i] * 255.0f + 0.5f);
        px[i * 4 + 1] = (unsigned char)(b[i] * 255.0f + 0.5f);
        px[i * 4 + 2] = 0; px[i * 4 + 3] = 255;
    }
    /* and a grey copy of the broad octave for the fixed pipe to modulate with: the
       RGBA sheet carries two different fields in R and G and nothing in B, so a quad
       textured with it comes out in rainbow stripes (the first wake did) */
    {
        std::vector<unsigned char> gp((size_t)N * N);
        for (size_t i = 0; i < (size_t)N * N; i++) gp[i] = px[i * 4 + 0];
        glGenTextures(1, &g_waterFoamGreyTex);
        glBindTexture(GL_TEXTURE_2D, g_waterFoamGreyTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, N, N, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, &gp[0]);
    }
    glGenTextures(1, &g_waterFoamTex);
    glBindTexture(GL_TEXTURE_2D, g_waterFoamTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
}
/* THE SURFACE'S NORMAL MAP, generated like the foam sheet: a height field of the same
   value noise (a broad octave and a finer one), its slopes turned into a tangent-space
   normal, tileable by construction. Two reads of it at different scales, drifting in
   different directions, are what make the surface ripple; the Half-Life 2 water the
   director pointed at is built the same way, a scrolled normal map under a Fresnel
   blend of what is below the surface and what is above it, with a sun glint. */
static GLuint g_waterNormalTex = 0;
#define WATER_NORMAL_SIZE 256
static void water_normal_boot(void)
{
    if (g_waterNormalTex) return;
    const int N = WATER_NORMAL_SIZE;
    std::vector<float> hgt((size_t)N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = ((float)x + 0.5f) / (float)N, v = ((float)y + 0.5f) / (float)N;
            hgt[(size_t)y * N + x] = fxc_fbm(0, u, v, 6, 3, 9001u) + 0.45f * fxc_fbm(0, u + 0.31f, v + 0.17f, 16, 2, 9103u);
        }
    std::vector<unsigned char> px((size_t)N * N * 4);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const int xl = (x + N - 1) % N, xr = (x + 1) % N, yu = (y + N - 1) % N, yd = (y + 1) % N;
            const float dx = (hgt[(size_t)y * N + xr] - hgt[(size_t)y * N + xl]) * 0.5f * (float)N * 0.08f;
            const float dy = (hgt[(size_t)yd * N + x] - hgt[(size_t)yu * N + x]) * 0.5f * (float)N * 0.08f;
            float nx = -dx, ny = -dy, nz = 1.0f;
            const float l = sqrtf(nx * nx + ny * ny + nz * nz);
            nx /= l; ny /= l; nz /= l;
            unsigned char* q = &px[((size_t)y * N + x) * 4];
            q[0] = (unsigned char)((nx * 0.5f + 0.5f) * 255.0f + 0.5f);
            q[1] = (unsigned char)((ny * 0.5f + 0.5f) * 255.0f + 0.5f);
            q[2] = (unsigned char)((nz * 0.5f + 0.5f) * 255.0f + 0.5f);
            q[3] = 255;
        }
    glGenTextures(1, &g_waterNormalTex);
    glBindTexture(GL_TEXTURE_2D, g_waterNormalTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* WHAT LIES UNDER THE SURFACE, copied out of the frame just before the water draws: the
   sea floor, which is everything on screen at that point. The shader reads it back
   displaced by the ripple, which is refraction, and composes the wash over it in the
   shader instead of leaving it to the blend. One copy per frame at the scene's size. */
static GLuint g_waterSceneTex = 0;
static int    g_waterSceneW = 0, g_waterSceneH = 0;
static void water_scene_grab(int* outW, int* outH)
{
    /* ON UNIT 0, ALWAYS. This binds a texture on whatever unit happens to be active,
       and it once ran with unit 4 active and left that unit bound to nothing: the wave
       field was bound there before this call, the sampler read zeros, and the whole
       wake was invisible while every uniform and every log line said it was live. */
    glActiveTexture(GL_TEXTURE0);
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    const int w = vp[2], h = vp[3];
    if (!g_waterSceneTex) glGenTextures(1, &g_waterSceneTex);
    glBindTexture(GL_TEXTURE_2D, g_waterSceneTex);
    if (w != g_waterSceneW || h != g_waterSceneH) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        g_waterSceneW = w; g_waterSceneH = h;
    }
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vp[0], vp[1], w, h);
    glBindTexture(GL_TEXTURE_2D, 0);
    *outW = w; *outH = h;
}
/* WHAT LIES ABOVE THE SURFACE: the world drawn again from under it. The camera is
   mirrored about the sea's plane (the mean height of the holed cells in view), the
   half of the world below that plane is clipped away, and the terrain and every
   opaque body are drawn into a half-size target with the frame's own projection, so a
   pixel of that target is what the water at the same screen position reflects. The
   shader reads it displaced by the ripple. This is the planar reflection every
   Source-engine pond does; the sky it is cleared to is what shows where nothing
   stands. One extra draw of the world per frame, at a quarter of the pixels. */
static FxRT g_waterReflRT;
static int water_reflection_render(float planeY)
{
    if (!g_fxCasters) return 0;
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    const int rw = vp[2] / 2 > 8 ? vp[2] / 2 : 8, rh = vp[3] / 2 > 8 ? vp[3] / 2 : 8;
    if (!fx_rt_init(&g_waterReflRT, rw, rh, 1 | 2, GL_LINEAR)) return 0;
    float clearc[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearc);

    fx_rt_bind(&g_waterReflRT);
    glViewport(0, 0, rw, rh);
    {
        /* the clear sky, eased toward the overcast grey while it rains (rain_mod.h) */
        const float oc = rain_overcast();
        glClearColor(0.62f, 0.72f - 0.06f * oc, 0.84f - 0.12f * oc, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(0.0f, planeY, 0.0f);
    glScalef(1.0f, -1.0f, 1.0f);
    glTranslatef(0.0f, -planeY, 0.0f);
    {
        /* keep only what stands above the water; the plane is given in world space,
           which the mirrored modelview now loaded turns into eye space for us */
        const GLdouble eq[4] = { 0.0, 1.0, 0.0, -(double)planeY + 0.02 };
        glClipPlane(GL_CLIP_PLANE0, eq);
        glEnable(GL_CLIP_PLANE0);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (water_src_ra()) ra3d_reflect_ground(); else draw_terrain();
    fprof_mark("water: mirror ground");
    /* THE CASTER LIST IS NOT LOOKING AT THE SUN HERE. It is the camera's own projection
       with the modelview mirrored, so a caster cull that used the sun's box would throw
       away half the reflection. Named for the host, and put back afterwards so the sun
       map cannot inherit a plane that belongs to the sea. */
    g_fxCasterPass = FXCAST_REFLECT;
    g_fxCasterPlaneY = planeY;
    g_fxCasters();
    g_fxCasterPass = FXCAST_SUN;
    fprof_mark("water: mirror casters");
    glDisable(GL_CLIP_PLANE0);
    glPopMatrix();

    /* Back to the chain's scene target, named rather than queried: the framebuffer
       binding query answered 0 on this GL, and restoring "0" sent the rest of the frame
       to the window while the chain presented a scene that stopped at the sea floor.
       The water pass only runs while the chain is active, so the scene target is the
       one and only place it can have been drawing into. */
    fx_rt_bind(&g_fxScene);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glClearColor(clearc[0], clearc[1], clearc[2], clearc[3]);
    return 1;
}
static GLuint g_waterProg = 0;
static int    g_waterProgTried = 0;

/* One-dimensional squared Euclidean distance transform (Felzenszwalb & Huttenlocher).
   f is the per-sample cost (0 on a source, WATER_EDT_INF elsewhere); d gets the
   squared distance to the nearest source and arg the index of the sample that won.
   v and z are scratch of n and n+1. */
#define WATER_EDT_INF 1.0e12
static void water_edt_1d(const double* f, double* d, int* arg, int n, int* v, double* z)
{
    int k = 0;
    v[0] = 0;
    z[0] = -WATER_EDT_INF;
    z[1] =  WATER_EDT_INF;
    for (int q = 1; q < n; q++) {
        double s = ((f[q] + (double)q * q) - (f[v[k]] + (double)v[k] * v[k]))
                 / (2.0 * (q - v[k]));
        while (s <= z[k]) {
            k--;
            s = ((f[q] + (double)q * q) - (f[v[k]] + (double)v[k] * v[k]))
              / (2.0 * (q - v[k]));
        }
        k++;
        v[k] = q;
        z[k] = s;
        z[k + 1] = WATER_EDT_INF;
    }
    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k + 1] < (double)q) k++;
        d[q] = (double)(q - v[k]) * (q - v[k]) + f[v[k]];
        arg[q] = v[k];
    }
}

/* Distance from every texel to the nearest texel where src is non-zero, in texels, and
   (when asked) the index of that nearest texel. Two separable passes: the column pass
   remembers which row won, the row pass which column, and the pair is the answer. */
static void water_edt_2d(const unsigned char* src, float* out, int* nearest, int w, int h)
{
    const int n = w > h ? w : h;
    std::vector<double> f(n), d(n), z(n + 1);
    std::vector<int>    v(n), arg(n);
    std::vector<double> tmp((size_t)w * h);
    std::vector<int>    rowOf((size_t)w * h);
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) f[y] = src[(size_t)y * w + x] ? 0.0 : WATER_EDT_INF;
        water_edt_1d(&f[0], &d[0], &arg[0], h, &v[0], &z[0]);
        for (int y = 0; y < h; y++) { tmp[(size_t)y * w + x] = d[y]; rowOf[(size_t)y * w + x] = arg[y]; }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) f[x] = tmp[(size_t)y * w + x];
        water_edt_1d(&f[0], &d[0], &arg[0], w, &v[0], &z[0]);
        for (int x = 0; x < w; x++) {
            const double q = d[x];
            out[(size_t)y * w + x] = q >= WATER_EDT_INF ? 1.0e6f : (float)sqrt(q);
            if (nearest) {
                const int sx = arg[x];
                nearest[(size_t)y * w + x] = q >= WATER_EDT_INF ? -1
                                           : rowOf[(size_t)y * w + sx] * w + sx;
            }
        }
    }
}

/* One separable box blur of radius r, in place. */
static void water_box_blur(float* a, int w, int h, int r)
{
    if (r < 1) return;
    std::vector<float> t((size_t)w * h);
    for (int y = 0; y < h; y++) {
        const float* row = a + (size_t)y * w;
        float* orow = &t[(size_t)y * w];
        for (int x = 0; x < w; x++) {
            float s = 0.0f; int n = 0;
            for (int k = -r; k <= r; k++) {
                const int xx = x + k;
                if (xx < 0 || xx >= w) continue;
                s += row[xx]; n++;
            }
            orow[x] = s / (float)n;
        }
    }
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            float s = 0.0f; int n = 0;
            for (int k = -r; k <= r; k++) {
                const int yy = y + k;
                if (yy < 0 || yy >= h) continue;
                s += t[(size_t)yy * w + x]; n++;
            }
            a[(size_t)y * w + x] = s / (float)n;
        }
    }
}

static void water_land_forget(void);

static void water_field_free(void)
{
    if (g_waterFieldTex) glDeleteTextures(1, &g_waterFieldTex);
    water_land_forget();   /* g_waterLandTex is always one of the cache's */
    g_waterFieldTex = g_waterLandTex = 0;
    g_waterFieldAtlas = -1; g_waterFieldAtlasGL = 0;
    g_waterFieldW = g_waterFieldH = 0;
    g_waterFieldScen[0] = 0;
    g_waterAudioClass.clear();
    g_waterAudioSources.clear();
}

/* The mean and spread of the WATER1 tile's luminance, read back off the GPU once per
   pack. The foam's break-up pattern is the water art itself, and the shader needs to
   know where "bright" sits in that art to threshold it; guessing a constant here would
   be a second copy of a number the tile already carries. Desktop GL only, which is the
   only place this file runs. */
static void water_grain_measure(void)
{
    g_waterGrainMean = 0.5f; g_waterGrainInv = 2.0f;
    if (water_src_ra()) {
        /* Red Alert's painting, measured when its ground built it; the second layer is
           white and the painting takes the ground's light, so the product's mean is the
           painting's own under that light. */
        g_waterGrainMean = g_waterRa.grainMean; g_waterGrainInv = g_waterRa.grainInv;
        g_waterMean2 = WATER_RA_BASE_GAIN;
        return;
    }
    if (g_pack.waterTex1 < 0) return;
    const PackTex& t = g_pack.tex[g_pack.waterTex1];
    if (t.w <= 0 || t.h <= 0 || t.w * t.h > 4096) return;
    std::vector<unsigned char> px((size_t)t.w * t.h * 4);
    glBindTexture(GL_TEXTURE_2D, t.gl);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    double s = 0.0, s2 = 0.0; int n = 0;
    for (int i = 0; i < t.w * t.h; i++) {
        const unsigned char* q = &px[(size_t)i * 4];
        const double l = (0.299 * q[0] + 0.587 * q[1] + 0.114 * q[2]) / 255.0;
        s += l; s2 += l * l; n++;
    }
    if (n < 4) return;
    const double mean = s / n;
    const double var  = s2 / n - mean * mean;
    const double sd   = var > 1.0e-6 ? sqrt(var) : 0.001;
    g_waterGrainMean = (float)mean;
    g_waterGrainInv  = (float)(1.0 / (2.5 * sd));
    /* And WATER2's mean, so the product's average brightness is known too. */
    g_waterMean2 = g_waterGrainMean;
    if (g_pack.waterTex2 >= 0) {
        const PackTex& t2 = g_pack.tex[g_pack.waterTex2];
        if (t2.w > 0 && t2.h > 0 && t2.w * t2.h <= 4096) {
            std::vector<unsigned char> p2((size_t)t2.w * t2.h * 4);
            glBindTexture(GL_TEXTURE_2D, t2.gl);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &p2[0]);
            double s2m = 0.0;
            for (int i = 0; i < t2.w * t2.h; i++) {
                const unsigned char* q = &p2[(size_t)i * 4];
                s2m += (0.299 * q[0] + 0.587 * q[1] + 0.114 * q[2]) / 255.0;
            }
            g_waterMean2 = (float)(s2m / (t2.w * t2.h));
        }
    }
}

/* THE LAND UNDER THE WATER, AND WHY IT IS ITS OWN STEP. Everything else in the field is
   read from the CARTRIDGE atlas's alpha, which is the same whatever the ground draws
   with, so it is built once per map. This step alone reads the atlas on screen, and the
   graphics modes change that atlas: rebuilding the whole field for it cost 0.55 to 0.68 s
   of one frame on SCG01EA, on every SPACE into Enhanced. So the arrays
   this step needs are kept after the build, and each atlas's land texture is kept in a
   small cache (water_land_for). */
static std::vector<unsigned char> g_wfWet, g_wfRiver;
static std::vector<int>           g_wfNearLand, g_wfNearWater;
static std::vector<float>         g_wfDDry;
static int                        g_wfW = 0, g_wfH = 0, g_wfRes = 0;

static GLuint water_land_make(int ai)
{
    /* 5. THE LAND UNDER THE WATER. For every wet texel, the colour of the nearest
       land texel, read out of the atlas the terrain is DRAWING (the DOS sheet, the
       cartridge's or the Remaster's, whichever the dial says) at that land texel's own
       atlas position. Read back off the GPU rather than kept from load, because only
       the cartridge sheet is kept and it is not the one on screen. A first draft had
       the shader walk back through its own cell's tile to find sand; on a coast that
       runs along a cell boundary the water cell's tile is all hole and the walk found
       nothing, which is why the sand never showed. */
    {
        const int w = g_wfW, h = g_wfH, res = g_wfRes;
        const std::vector<unsigned char>& wet = g_wfWet;
        const std::vector<unsigned char>& river = g_wfRiver;
        const std::vector<int>& nearLand = g_wfNearLand;
        const std::vector<int>& nearWater = g_wfNearWater;
        const std::vector<float>& dDry = g_wfDDry;
        const PackTex& dr = water_atlas(ai);
        const std::vector<PackCell>& cells = water_cells();
        std::vector<unsigned char> atl;
        if (dr.w > 0 && dr.h > 0 && (long)dr.w * dr.h <= WATER_ATLAS_MAX_TEXELS) {
            atl.resize((size_t)dr.w * dr.h * 4);
            glBindTexture(GL_TEXTURE_2D, dr.gl);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &atl[0]);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        std::vector<int> cellOf((size_t)g_gridW * g_gridH, -1);
        for (size_t i = 0; i < cells.size(); i++) {
            const PackCell& c = cells[i];
            if (c.x < 0 || c.y < 0 || c.x >= g_gridW || c.y >= g_gridH) continue;
            cellOf[(size_t)c.y * g_gridW + c.x] = (int)i;
        }
        std::vector<unsigned char> lp((size_t)w * h * 4, 0);
        /* ALPHA IS THE CLASS of the water here or nearest here: 255 sea, 128 river. Set
           on land as well as on water so the value is flat across the coast and only
           changes at a river mouth, which is the one place it should. */
        for (size_t i = 0; i < (size_t)w * h; i++) {
            const int wi = wet[i] ? (int)i : nearWater[i];
            lp[i * 4 + 3] = (wi >= 0 && river[wi]) ? 128 : 255;
        }
        if (!atl.empty()) {
            for (size_t i = 0; i < (size_t)w * h; i++) {
                /* a wet texel takes its nearest land's colour; a dry one its own, so the
                   lap over the sand knows what it is lying on. NOT the texel right on
                   the cut, though: the tile art draws a dark rim where sand meets water,
                   and reading it put a dark hairline along every coast. Of the land
                   texels within two of the nearest one, take the one furthest from the
                   water: the sand proper. */
                int ni = wet[i] ? nearLand[i] : (int)i;
                if (ni < 0) continue;
                {
                    const int bx = ni % w, by = ni / w;
                    float bestD = -1.0f;
                    for (int dy = -2; dy <= 2; dy++)
                        for (int dx = -2; dx <= 2; dx++) {
                            const int x = bx + dx, y = by + dy;
                            if (x < 0 || y < 0 || x >= w || y >= h) continue;
                            const size_t j = (size_t)y * w + x;
                            if (wet[j]) continue;
                            if (dDry[j] > bestD) { bestD = dDry[j]; ni = (int)j; }
                        }
                }
                const int lx = ni % w, ly = ni / w;
                const int ci = cellOf[(size_t)(ly / res) * g_gridW + (lx / res)];
                if (ci < 0) continue;
                const PackCell& c = cells[ci];
                const float fu = ((float)(lx % res) + 0.5f) / (float)res;
                const float fv = ((float)(ly % res) + 0.5f) / (float)res;
                int tx = (int)((c.u0 + (c.u1 - c.u0) * fu) * (float)dr.uw);
                int ty = (int)((c.v0 + (c.v1 - c.v0) * fv) * (float)dr.uh);
                if (tx < 0) tx = 0; else if (tx >= dr.w) tx = dr.w - 1;
                if (ty < 0) ty = 0; else if (ty >= dr.h) ty = dr.h - 1;
                const unsigned char* q = &atl[((size_t)ty * dr.w + tx) * 4];
                lp[i * 4 + 0] = q[0]; lp[i * 4 + 1] = q[1]; lp[i * 4 + 2] = q[2];
            }
        }
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, &lp[0]);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }
}

struct WaterLandSlot { GLuint tex; int atlas; GLuint gl; unsigned stamp; };
static WaterLandSlot g_waterLandSlot[3];
static unsigned      g_waterLandStamp = 0;

static void water_land_forget(void)
{
    for (int k = 0; k < 3; k++) {
        if (g_waterLandSlot[k].tex) glDeleteTextures(1, &g_waterLandSlot[k].tex);
        memset(&g_waterLandSlot[k], 0, sizeof g_waterLandSlot[k]);
    }
}

/* The land texture for atlas `ai`: kept from before if there is one, else made, taking
   the least recently used of three slots. */
static GLuint water_land_for(int ai)
{
    const GLuint gl = water_atlas(ai).gl;
    int k, lru = 0;
    for (k = 0; k < 3; k++)
        if (g_waterLandSlot[k].tex && g_waterLandSlot[k].atlas == ai && g_waterLandSlot[k].gl == gl) {
            g_waterLandSlot[k].stamp = ++g_waterLandStamp;
            return g_waterLandSlot[k].tex;
        }
    for (k = 0; k < 3; k++) {
        if (!g_waterLandSlot[k].tex) { lru = k; break; }
        if (g_waterLandSlot[k].stamp < g_waterLandSlot[lru].stamp) lru = k;
    }
    if (g_waterLandSlot[lru].tex) glDeleteTextures(1, &g_waterLandSlot[lru].tex);
    g_waterLandSlot[lru].tex = water_land_make(ai);
    g_waterLandSlot[lru].atlas = ai;
    g_waterLandSlot[lru].gl = gl;
    g_waterLandSlot[lru].stamp = ++g_waterLandStamp;
    return g_waterLandSlot[lru].tex;
}

/* Build the field for the loaded pack. False when the pack cannot supply one (no
   atlas kept, no holed cell), in which case the cartridge's pass draws. */
static bool water_field_build(void)
{
    water_field_free();
    /* THE CUT: the cartridge atlas's alpha (four bytes a texel, alpha last), addressed
       through the cells' rectangles normalised to the atlas's used size; or Red Alert's
       cut (one byte a texel), laid out as its sheet at the 1995 tile's size. */
    const bool ra = water_src_ra();
    const std::vector<PackCell>& cells = water_cells();
    const unsigned char* cart = NULL;
    int aw = 0, ah = 0, cuw = 0, cuh = 0, cstride = 4;
    if (ra) {
        if (!g_waterRa.ok || g_waterRa.cut.empty()) return false;
        cart = &g_waterRa.cut[0];
        aw = cuw = g_waterRa.cutW; ah = cuh = g_waterRa.cutH; cstride = 1;
    } else {
        if (g_pack.terrainCart.empty() || g_pack.terrainTex < 0
            || g_pack.terrainTex >= (int)g_pack.tex.size())
            return false;
        const PackTex& at = g_pack.tex[g_pack.terrainTex];
        aw = g_pack.terrainCartW; ah = g_pack.terrainCartH;
        cuw = at.uw; cuh = at.uh;
        cart = &g_pack.terrainCart[0];
    }
    if (aw <= 0 || ah <= 0 || cuw <= 0 || cuh <= 0) return false;

    int res = WATER_FIELD_RES;
    while (res > 4 && (g_gridW * res > WATER_FIELD_MAXW || g_gridH * res > WATER_FIELD_MAXW))
        res /= 2;
    const int w = g_gridW * res, h = g_gridH * res;
    if (w <= 0 || h <= 0) return false;

    /* 1. the mask: 1 where the art is a water hole, 0 on land. Cells the pack does not
       list (outside the play rectangle) are land, so the sea ends at the map's edge
       rather than continuing under the frame; the shroud hides that edge anyway. */
    std::vector<unsigned char> wet((size_t)w * h, 0);
    /* Land that can be a COAST: only inside the play rectangle. The margin cells the
       pack carries beyond it are drawn (cell_shown) but the sea meeting them is the
       edge of the map, not a shore, and a foam line ruled along the map's border was
       the first thing the first frame showed. Outside the rectangle a texel is
       either sea (holed cell) or nothing, never a source. */
    std::vector<unsigned char> dry((size_t)w * h, 0);
    int holed = 0;
    for (size_t i = 0; i < cells.size(); i++) {
        const PackCell& c = cells[i];
        if (c.x < 0 || c.y < 0 || c.x >= g_gridW || c.y >= g_gridH) continue;
        const bool inRect = c.x >= g_mapX && c.x < g_mapX + g_mapW
                         && c.y >= g_mapY && c.y < g_mapY + g_mapH;
        if (!c.holes) {
            if (!inRect) continue;
            for (int j = 0; j < res; j++)
                memset(&dry[(size_t)(c.y * res + j) * w + c.x * res], 1, (size_t)res);
            continue;
        }
        holed++;
        for (int j = 0; j < res; j++) {
            const float v = c.v0 + (c.v1 - c.v0) * (((float)j + 0.5f) / (float)res);
            int ty = (int)(v * (float)cuh);
            if (ty < 0) ty = 0; else if (ty >= ah) ty = ah - 1;
            for (int k = 0; k < res; k++) {
                const float u = c.u0 + (c.u1 - c.u0) * (((float)k + 0.5f) / (float)res);
                int tx = (int)(u * (float)cuw);
                if (tx < 0) tx = 0; else if (tx >= aw) tx = aw - 1;
                const unsigned char a = cart[((size_t)ty * aw + tx) * cstride + (cstride - 1)];
                const size_t ti = (size_t)(c.y * res + j) * w + (c.x * res + k);
                if (a < 128) wet[ti] = 1;
                else if (inRect) dry[ti] = 1;
            }
        }
    }
    if (!holed) return false;

    /* 2. the two distances: water texel -> nearest coast land, land texel -> nearest
       water. A texel that is neither (beyond the rectangle) is carried by whichever
       neighbour is nearest, which is what keeps the sea continuous to the frame. */
    std::vector<float> dWet((size_t)w * h), dDry((size_t)w * h);
    std::vector<int> nearLand((size_t)w * h);
    water_edt_2d(&dry[0], &dWet[0], &nearLand[0], w, h);   /* on water: distance to land  */
    std::vector<int> nearWater((size_t)w * h);
    water_edt_2d(&wet[0], &dDry[0], &nearWater[0], w, h);  /* on land:  distance to water */

    /* 3. the signed field in cells, clamped to the range, then a blurred copy whose
       gradient is the direction out to sea. The blur is one cell wide so the
       direction is smooth across the medial ridge between two coasts. */
    const float range = WATER_FIELD_RANGE;
    std::vector<float> sd((size_t)w * h);
    int wetN = 0, shoreN = 0;
    for (size_t i = 0; i < sd.size(); i++) {
        /* Beyond the rectangle a texel is neither: it carries the sea's own distance, so
           neither pass can find a coast along the map's border. */
        float d = (wet[i] || !dry[i]) ? dWet[i] : -dDry[i];
        d /= (float)res;
        if (d > range) d = range; else if (d < -range) d = -range;
        sd[i] = d;
        if (wet[i]) { wetN++; if (dWet[i] <= 1.0f) shoreN++; }
    }
    std::vector<float> sm(sd);
    water_box_blur(&sm[0], w, h, res / 2 > 0 ? res / 2 : 1);
    water_box_blur(&sm[0], w, h, res / 2 > 0 ? res / 2 : 1);

    /* 4. the direction out to sea: the blurred field's gradient per cell, clamped to
       the unit disc so the encoding cannot wrap. */
    std::vector<float> gxs((size_t)w * h), gys((size_t)w * h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const size_t i = (size_t)y * w + x;
            const int xl = x > 0 ? x - 1 : x, xr = x < w - 1 ? x + 1 : x;
            const int yu = y > 0 ? y - 1 : y, yd = y < h - 1 ? y + 1 : y;
            float gx = (sm[(size_t)y * w + xr] - sm[(size_t)y * w + xl]) * (float)res / (float)(xr - xl > 0 ? xr - xl : 1);
            float gy = (sm[(size_t)yd * w + x] - sm[(size_t)yu * w + x]) * (float)res / (float)(yd - yu > 0 ? yd - yu : 1);
            const float gl = sqrtf(gx * gx + gy * gy);
            if (gl > 1.0f) { gx /= gl; gy /= gl; }
            gxs[i] = gx; gys[i] = gy;
        }
    }

    /* 5. RIVERS. A water body that is nowhere wider than twice water_river_width is a
       river, and a river does not drift on the sea's current or break waves on its
       banks: it RUNS. Its texels get a flow along the banks (the tangent of the field)
       pointed downstream, where downstream is read off the terrain heights along the
       tangent; on a river the heightmap leaves flat, the whole run is turned to agree
       with the current bearing, so one dial still decides. Each body is labelled by a
       flood fill, the sign is carried along it by a second fill from the texel with the
       steepest fall so a winding river cannot flip halfway, and the result is blurred
       one cell so the middle of the channel takes the average of its two banks. */
    std::vector<unsigned char> river((size_t)w * h, 0);
    /* the water mask blurred a cell, its gradient, and that gradient's tensor blurred
       another cell: what the river tangents are read from */
    std::vector<float> Jxx((size_t)w * h), Jxy((size_t)w * h), Jyy((size_t)w * h);
    {
        std::vector<float> wb((size_t)w * h);
        for (size_t i = 0; i < wb.size(); i++) wb[i] = wet[i] ? 1.0f : 0.0f;
        water_box_blur(&wb[0], w, h, res);
        water_box_blur(&wb[0], w, h, res / 2 > 0 ? res / 2 : 1);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const size_t i = (size_t)y * w + x;
                const int xl = x > 0 ? x - 1 : x, xr = x < w - 1 ? x + 1 : x;
                const int yu = y > 0 ? y - 1 : y, yd = y < h - 1 ? y + 1 : y;
                const float gx = wb[(size_t)y * w + xr] - wb[(size_t)y * w + xl];
                const float gy = wb[(size_t)yd * w + x] - wb[(size_t)yu * w + x];
                Jxx[i] = gx * gx; Jxy[i] = gx * gy; Jyy[i] = gy * gy;
            }
        water_box_blur(&Jxx[0], w, h, res);
        water_box_blur(&Jxy[0], w, h, res);
        water_box_blur(&Jyy[0], w, h, res);
    }
    {
        std::vector<int> comp((size_t)w * h, -1);
        std::vector<int> stack;
        int ncomp = 0;
        const float riverMax = g_fx.water_river_width > 0.1f ? g_fx.water_river_width : 0.1f;
        const float bearing = g_fx.water_current * (float)(M_PI / 180.0);
        const float curX = sinf(bearing), curY = -cosf(bearing);
        for (size_t seed = 0; seed < wet.size(); seed++) {
            if (!wet[seed] || comp[seed] >= 0) continue;
            const int id = ncomp++;
            float maxR = 0.0f;
            std::vector<int> members;
            stack.clear(); stack.push_back((int)seed); comp[seed] = id;
            while (!stack.empty()) {
                const int i = stack.back(); stack.pop_back();
                members.push_back(i);
                if (dWet[i] > maxR) maxR = dWet[i];
                const int x = i % w, y = i / w;
                const int nb[4] = { x > 0 ? i - 1 : -1, x < w - 1 ? i + 1 : -1,
                                    y > 0 ? i - w : -1, y < h - 1 ? i + w : -1 };
                for (int k = 0; k < 4; k++) {
                    const int n = nb[k];
                    if (n < 0 || !wet[n] || comp[n] >= 0) continue;
                    comp[n] = id; stack.push_back(n);
                }
            }
            if (maxR / (float)res >= riverMax) continue;      /* a sea or a lake */
            /* the tangent, and the fall along it, per texel. The tangent is the minor
               axis of the STRUCTURE TENSOR of the blurred water mask: where the
               channel's shape varies least. The distance field's gradient was tried
               first and is useless down the middle of a one-cell channel, where it
               vanishes and the direction went wherever the noise sent it; the tensor
               is well defined bank to bank. */
            std::vector<float> tx(members.size()), ty(members.size()), fall(members.size());
            std::vector<unsigned char> known(members.size(), 0);
            for (size_t m = 0; m < members.size(); m++) {
                const int i = members[m];
                const float a = Jxx[i], b = Jxy[i], c = Jyy[i];
                const float coh = (a + c) > 1.0e-6f ? ((a - c) * (a - c) + 4.0f * b * b) / ((a + c) * (a + c)) : 0.0f;
                if (a + c < 1.0e-6f || coh < 0.15f) { tx[m] = ty[m] = 0.0f; fall[m] = 0.0f; continue; }
                const float th = 0.5f * atan2f(2.0f * b, a - c);      /* the across direction */
                tx[m] = -sinf(th); ty[m] = cosf(th);                 /* along the channel */
                const float px = ((float)(i % w) + 0.5f) / (float)res;
                const float pz = ((float)(i / w) + 0.5f) / (float)res;
                fall[m] = terrain_y(px + tx[m], pz + ty[m]) - terrain_y(px - tx[m], pz - ty[m]);
                known[m] = 1;
            }
            /* the seed: the steepest fall, turned downhill */
            size_t best = 0; float bestFall = -1.0f;
            for (size_t m = 0; m < members.size(); m++)
                if (known[m] && fabsf(fall[m]) > bestFall) { bestFall = fabsf(fall[m]); best = m; }
            if (bestFall < 0.0f) continue;                      /* no bank anywhere: skip */
            if (fall[best] > 0.0f) { tx[best] = -tx[best]; ty[best] = -ty[best]; }
            /* carry the sign along the body */
            std::vector<int> idxOf((size_t)w * h, -1);
            for (size_t m = 0; m < members.size(); m++) idxOf[members[m]] = (int)m;
            std::vector<unsigned char> done(members.size(), 0);
            std::vector<size_t> q; q.push_back(best); done[best] = 1;
            for (size_t qi = 0; qi < q.size(); qi++) {
                const size_t m = q[qi];
                const int i = members[m];
                const int x = i % w, y = i / w;
                const int nb[4] = { x > 0 ? i - 1 : -1, x < w - 1 ? i + 1 : -1,
                                    y > 0 ? i - w : -1, y < h - 1 ? i + w : -1 };
                for (int k = 0; k < 4; k++) {
                    const int n = nb[k];
                    if (n < 0) continue;
                    const int mn = idxOf[n];
                    if (mn < 0 || done[mn]) continue;
                    if (!known[mn]) { tx[mn] = tx[m]; ty[mn] = ty[m]; }
                    else if (tx[mn] * tx[m] + ty[mn] * ty[m] < 0.0f) { tx[mn] = -tx[mn]; ty[mn] = -ty[mn]; }
                    done[mn] = 1; q.push_back(mn);
                }
            }
            /* a flat river runs the way the current points */
            if (bestFall < 0.02f) {
                float sx = 0.0f, sy = 0.0f;
                for (size_t m = 0; m < members.size(); m++) { sx += tx[m]; sy += ty[m]; }
                if (sx * curX + sy * curY < 0.0f)
                    for (size_t m = 0; m < members.size(); m++) { tx[m] = -tx[m]; ty[m] = -ty[m]; }
            }
            for (size_t m = 0; m < members.size(); m++) {
                gxs[members[m]] = tx[m]; gys[members[m]] = ty[m]; river[members[m]] = 1;
            }
        }
        /* smooth the river flow so the channel's middle takes its banks' average */
        if (ncomp) {
            std::vector<float> fx(gxs), fy(gys);
            water_box_blur(&fx[0], w, h, res / 2 > 0 ? res / 2 : 1);
            water_box_blur(&fy[0], w, h, res / 2 > 0 ? res / 2 : 1);
            for (size_t i = 0; i < river.size(); i++) {
                if (!river[i]) continue;
                const float l = sqrtf(fx[i] * fx[i] + fy[i] * fy[i]);
                if (l > 0.02f) { gxs[i] = fx[i] / l; gys[i] = fy[i] / l; }
            }
        }
    }

    /* 6. pack it. */
    std::vector<unsigned char> px((size_t)w * h * 4);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        const float r = 0.5f + sd[i] / (2.0f * range);
        unsigned char* q = &px[i * 4];
        q[0] = (unsigned char)(r  < 0.0f ? 0 : r  > 1.0f ? 255 : (int)(r * 255.0f + 0.5f));
        q[1] = (unsigned char)((int)((gxs[i] * 0.5f + 0.5f) * 255.0f + 0.5f));
        q[2] = (unsigned char)((int)((gys[i] * 0.5f + 0.5f) * 255.0f + 0.5f));
        q[3] = wet[i] ? 255 : 0;
    }
    /* THE FIELD ON DISK, for reading the flow offline: OPENCNC3D_WATER_DUMP=<path> writes
       the packed texels raw (RGBA8, row-major, w x h from the WATER|field line). A
       load-time instrument only; it draws nothing and is off unless asked for. */
    if (const char* dumpPath = getenv("OPENCNC3D_WATER_DUMP")) {
        FILE* df = fopen(dumpPath, "wb");
        if (df) { fwrite(&px[0], 1, px.size(), df); fclose(df); }
    }
    glGenTextures(1, &g_waterFieldTex);
    glBindTexture(GL_TEXTURE_2D, g_waterFieldTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
    /* The cells the shore pass re-draws: holed, or touching a holed cell. */
    g_waterCoastCell.assign((size_t)g_gridW * g_gridH, 0);
    for (size_t i = 0; i < cells.size(); i++) {
        const PackCell& c = cells[i];
        if (!c.holes || c.x < 0 || c.y < 0 || c.x >= g_gridW || c.y >= g_gridH) continue;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                const int x = c.x + dx, y = c.y + dy;
                if (x < 0 || y < 0 || x >= g_gridW || y >= g_gridH) continue;
                g_waterCoastCell[(size_t)y * g_gridW + x] = 1;
            }
    }
    /* 5. THE LAND UNDER THE WATER, for the atlas on screen now. The arrays it reads are
       kept (water_land_make says why), so another atlas later costs only this step. */
    // Template identity, not connected-component width, defines acoustic rivers.
    // A channel connected to the ocean must still sound like a river at its mouth.
    std::vector<unsigned char> riverCell((size_t)g_gridW * g_gridH, 0);
    int templateCount = 0;
    const TtSlot* templates = tt_table(g_pack.theater, &templateCount);
    const bool gutterAtlas = cuw == TT_COLS * TT_PITCH;
    for (size_t ci = 0; !ra && ci < cells.size(); ++ci) {
        const PackCell& c = cells[ci];
        if (!c.holes || c.x < 0 || c.y < 0 || c.x >= g_gridW || c.y >= g_gridH) continue;
        const int pitch = gutterAtlas ? TT_PITCH : TT_TS;
        const int slot = (int)(c.v0 * cuh) / pitch * TT_COLS + (int)(c.u0 * cuw) / pitch;
        for (int ti = 0; ti < templateCount; ++ti) {
            if (templates[ti].slot != slot) continue;
            // Tiberian Dawn defines.h: RIVER1..25, FORD1..2, FALLS1..2 = 136..164.
            // BRIDGE1..4D = 165..172 span the same river channels.
            riverCell[(size_t)c.y * g_gridW + c.x] = templates[ti].tmpl >= 136 && templates[ti].tmpl <= 172;
            break;
        }
    }
    g_waterAudioClass.assign(wet.size(), 0);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const int cx = x / res, cy = y / res;
        if (cx < g_mapX || cy < g_mapY || cx >= g_mapX + g_mapW || cy >= g_mapY + g_mapH) continue;
        const size_t i = (size_t)y * w + x;
        if (wet[i]) g_waterAudioClass[i] = (ra ? river[i] != 0 : riverCell[(size_t)cy * g_gridW + cx] != 0) ? 2 : 1;
    }
    const int stride = res / 4 > 0 ? res / 4 : 1;
    for (int y = 0; y < h; y += stride) for (int x = 0; x < w; x += stride) {
        const int type = g_waterAudioClass[(size_t)y * w + x];
        if (!type) continue;
        WaterAudioSource source = { (x + 0.5f) / res, (y + 0.5f) / res, type == 2 };
        g_waterAudioSources.push_back(source);
    }
    g_wfWet.swap(wet); g_wfRiver.swap(river);
    g_wfNearLand.swap(nearLand); g_wfNearWater.swap(nearWater); g_wfDDry.swap(dDry);
    g_wfW = w; g_wfH = h; g_wfRes = res;
    {
        const int ai = water_atlas_index();
        g_waterLandTex = water_land_for(ai);
        g_waterFieldAtlas = ai;
        g_waterFieldAtlasGL = water_atlas(ai).gl;
    }
    g_waterFieldW = w; g_waterFieldH = h; g_waterFieldRes = res;

    g_waterFieldWet = wetN; g_waterFieldShore = shoreN; g_waterFieldCells = holed;
    snprintf(g_waterFieldScen, sizeof g_waterFieldScen, "%s", g_pack.scen);
    g_waterFieldRect[0] = g_mapX; g_waterFieldRect[1] = g_mapY;
    g_waterFieldRect[2] = g_mapW; g_waterFieldRect[3] = g_mapH;
    g_waterFieldBearing = g_fx.water_current; g_waterFieldRiverW = g_fx.water_river_width;
    water_grain_measure();
    int riverN = 0;
    for (size_t i = 0; i < g_wfRiver.size(); i++) riverN += g_wfRiver[i];
    g_waterFieldRiver = riverN;
    fprintf(stderr, "WATER|field|scen=%s|res=%d|size=%dx%d|holed_cells=%d|wet_texels=%d|"
                    "shore_texels=%d|river_texels=%d|grain=%.3f/%.2f\n",
            g_pack.scen, res, w, h, holed, wetN, shoreN, riverN, g_waterGrainMean, g_waterGrainInv);
    if (ra)
        fprintf(stderr, "WATER|ra|field|scen=%s|holed_cells=%d|wet_texels=%d|base=red alert|"
                        "repeats=%.2f|base_mean=%.3f\n",
                g_pack.scen, holed, wetN, (double)g_waterRa.baseRepeats, (double)g_waterGrainMean);
    return true;
}

/* THE LAST BUILD THAT FOUND NO WATER, and exactly what it was asked with. A map with no
   holed cell answers "no field" on every frame, and finding that out costs two full-grid
   masks and a pass over every cell, so the answer is kept until something it depends on
   moves: any input the positive check in water_field_sync compares, or g_cellHolesGen,
   which is how a map that gains water in the editor still builds its field on the next
   frame. A failed build has already freed the field, so skipping the retry leaves every
   global exactly as the retry would have left it. */
struct WaterFieldNone {
    bool     valid;
    char     scen[sizeof g_pack.scen];
    int      gridW, gridH, rect[4], atlas;
    GLuint   atlasGL;
    float    bearing, riverW;
    unsigned holesGen;
};
static WaterFieldNone g_waterFieldNone = { false };

static GLuint water_field_atlas_gl(int ai)
{
    if (water_src_ra()) return water_atlas(ai).gl;
    return (ai >= 0 && ai < (int)g_pack.tex.size()) ? g_pack.tex[ai].gl : 0;
}

static bool water_field_none_holds(void)
{
    const WaterFieldNone& n = g_waterFieldNone;
    const int ai = water_atlas_index();
    return n.valid && n.holesGen == g_cellHolesGen && !strcmp(n.scen, g_pack.scen)
        && n.gridW == g_gridW && n.gridH == g_gridH
        && n.rect[0] == g_mapX && n.rect[1] == g_mapY
        && n.rect[2] == g_mapW && n.rect[3] == g_mapH
        && n.atlas == ai && n.atlasGL == water_field_atlas_gl(ai)
        && n.bearing == g_fx.water_current && n.riverW == g_fx.water_river_width;
}

static void water_field_none_note(void)
{
    WaterFieldNone& n = g_waterFieldNone;
    const int ai = water_atlas_index();
    n.valid = true;
    n.holesGen = g_cellHolesGen;
    snprintf(n.scen, sizeof n.scen, "%s", g_pack.scen);
    n.gridW = g_gridW; n.gridH = g_gridH;
    n.rect[0] = g_mapX; n.rect[1] = g_mapY; n.rect[2] = g_mapW; n.rect[3] = g_mapH;
    n.atlas = ai; n.atlasGL = water_field_atlas_gl(ai);
    n.bearing = g_fx.water_current; n.riverW = g_fx.water_river_width;
}

/* The field belongs to one pack; a new scenario rebuilds it. A new ATLAS only changes
   the land under the water, which is a cache lookup or one short step. */
static bool water_field_sync(void)
{
    const int ai = water_atlas_index();
    const GLuint agl = water_atlas(ai).gl;
    if (g_waterFieldTex && !strcmp(g_waterFieldScen, g_pack.scen)
        && g_waterFieldW == g_gridW * g_waterFieldRes && g_waterFieldH == g_gridH * g_waterFieldRes
        && g_waterFieldRect[0] == g_mapX && g_waterFieldRect[1] == g_mapY
        && g_waterFieldRect[2] == g_mapW && g_waterFieldRect[3] == g_mapH
        && g_waterFieldBearing == g_fx.water_current && g_waterFieldRiverW == g_fx.water_river_width) {
        if (g_waterFieldAtlas != ai || g_waterFieldAtlasGL != agl) {
            g_waterLandTex = water_land_for(ai);
            g_waterFieldAtlas = ai;
            g_waterFieldAtlasGL = agl;
        }
        return true;
    }
    if (water_field_none_holds()) return false;
    if (water_field_build()) { g_waterFieldNone.valid = false; return true; }
    water_field_none_note();
    return false;
}

/* ---------------------------------------------------------------------------------- *
 *  THE WAVE FIELD: a light physics simulation on the surface, and the boats' wakes.
 *
 *  The director, on the first wake: "the wake has clear hard lines and doesn't look
 *  like it's actually affecting the water either. You could add some light physics
 *  simulation to the water as well, to make it bounce more realistically." So the
 *  wake is no longer a strip painted on the sea: it is the sea. A height field over the
 *  whole map (four texels a cell, two floats a texel: this step's height and the last
 *  step's) runs the wave equation once per ENGINE tick on the GPU,
 *
 *      h[t+1] = (2 h[t] - h[t-1] + c2 * laplacian(h[t])) * damping
 *
 *  with every boat pressing on it where the hull is DRAWN (the smoothed position, so
 *  the source glides), harder the faster it goes. Land damps the height to nothing, so
 *  a ripple reaching the shore reflects off it, which is the bounce. Two textures
 *  ping-pong; a step is one quad through one small shader. The water shader then reads
 *  the height field's gradient into its normal (so the wake ripples, refracts and
 *  catches the sun like the rest of the surface) and its crests into a soft white,
 *  and nothing about it has an edge, because a wave equation has none.
 *
 *  DETERMINISM. One step per engine tick, and only when the tick advances: a paused
 *  game holds the sea still and two --shot runs step the same number of times. The
 *  arithmetic is half-float on the GPU, so the field is bit-identical on one machine
 *  and close on another, which is the same footing the rest of the chain stands on.
 * ---------------------------------------------------------------------------------- */
/* Texels a cell. It was 4, and that is the whole reason the wake broke into patches:
   the press had been narrowed to 0.045 cells, which is a FIFTH of a texel at that
   resolution, so the Gaussian fell between the samples and the field picked it up only
   where a texel happened to sit near the hull. A press must never be smaller than the
   grid that carries it: 12 texels a cell resolves a tenth of a cell, and the radius
   below is floored at one texel whatever the dial says. */
#define WAVE_RES      12        /* texels a cell */
#define WAVE_MAXW     1024
#define WAVE_MAXBOATS 8
#ifndef GL_RGBA16F_ARB
#define GL_RGBA16F_ARB 0x881A
#endif
static FxRT   g_waveRT[2];
static int    g_waveCur = 0;
static int    g_waveW = 0, g_waveH = 0;
static int    g_waveFrame = -1;
static int    g_waveStepped = 0;         /* steps taken since the field was cleared */
static GLuint g_waveProg = 0;
static int    g_waveProgTried = 0;
static char   g_waveScen[20] = "";

/* One boat as the simulation sees it: where it is drawn, and how fast it moves. */
struct WaveBoat { int id; float x, z; float lx, lz; int lastFrame; int have; };
static std::vector<WaveBoat> g_waveBoats;

static int water_is_boat(const SimObject& o)
{
    /* RED ALERT'S NAVY on its own map: the gunboat, the destroyer, the cruiser and the
       transport, out of limbo. Not the submarines: a wake would show where a submerged
       one is, which the game hides. */
    if (water_src_ra())
        return !o.limbo && (!strcmp(o.type, "PT") || !strcmp(o.type, "DD")
                            || !strcmp(o.type, "CA") || !strcmp(o.type, "LST"));
    /* by type alone: the kind a vessel is exported under is the brain's business */
    return !strcmp(o.type, "BOAT") || !strcmp(o.type, "LST");
}

/* No "#version" line of its own: the program is assembled by rain_glsl_source, which
   puts the version first and the shared hash functions (rain_glsl.h) after it. */
static const char* WAVE_FS =
    "uniform sampler2D uPrev; uniform sampler2D uField;\n"
    "uniform vec2 uTexel; uniform float uC2; uniform float uDamp; uniform float uTrailDecay;\n"
    "uniform int uNBoat; uniform vec4 uBoat[8]; uniform vec4 uBoatPrev[8];\n"
    /* THE RAIN: uRainP is the chance, per texel per step, that a drop lands there;
       uRainF how hard it presses; uRainSeed the step count, so every step scatters
       the drops afresh and every run scatters them the same. */
    "uniform float uRainP; uniform float uRainF; uniform float uRainSeed;\n"
    /* uBoat is (x, z) where the source is NOW, its strength and its radius, all in
       field uv; uBoatPrev is where it was at the last step. The press is swept along
       the SEGMENT between them: at one press a tick and a hull narrower than the
       distance it covers in that tick, stamping at a point left a row of separate
       blobs with clear water between them, which is what a trail must not have. */
    "float segDist(vec2 p, vec2 a, vec2 b) {\n"
    "    vec2 ab = b - a; float d = dot(ab, ab);\n"
    "    float t = d > 1e-9 ? clamp(dot(p - a, ab) / d, 0.0, 1.0) : 0.0;\n"
    "    return length(p - (a + ab * t));\n"
    "}\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "    vec2 hh = texture2D(uPrev, uv).rg;\n"
    "    float h = hh.r, hp = hh.g;\n"
    "    float l = texture2D(uPrev, uv + vec2(uTexel.x, 0.0)).r + texture2D(uPrev, uv - vec2(uTexel.x, 0.0)).r\n"
    "            + texture2D(uPrev, uv + vec2(0.0, uTexel.y)).r + texture2D(uPrev, uv - vec2(0.0, uTexel.y)).r - 4.0 * h;\n"
    "    float hn = (2.0 * h - hp + uC2 * l) * uDamp;\n"
    "    float press = 0.0;\n"
    "    for (int i = 0; i < 8; i++) {\n"
    "        if (i >= uNBoat) break;\n"
    "        float d = segDist(uv, uBoatPrev[i].xy, uBoat[i].xy) / uBoat[i].w;\n"
    "        press += uBoat[i].z * exp(-d * d * 2.0);\n"
    "    }\n"
    "    hn -= press;\n"                                        /* the hull presses the water down */
    /* A DROP IS ITS OWN PRESS, never added to `press`: the trail channel below reads
       press as white water, and rain must ring the sea without foaming it. Each texel
       asks its 3x3 neighbourhood whether a drop landed there this step, so a drop is a
       Gaussian two or three texels wide rather than a single-texel spike, which the
       field's grid would otherwise alias. */
    "    if (uRainP > 0.0) {\n"
    "        vec2 tx = floor(gl_FragCoord.xy);\n"
    "        float rain = 0.0;\n"
    "        for (int j = -1; j <= 1; j++) {\n"
    "            for (int i = -1; i <= 1; i++) {\n"
    "                vec2 cc = tx + vec2(float(i), float(j));\n"
    "                vec2 h = rn_h2(cc + uRainSeed * vec2(0.913, 0.571) + 11.0);\n"
    "                if (h.x < uRainP) {\n"
    "                    vec2 dd = (tx - cc) + (rn_h2(cc + uRainSeed + 7.0) - 0.5);\n"
    "                    rain += uRainF * (0.5 + h.y) * exp(-dot(dd, dd) * 0.9);\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "        hn -= rain;\n"
    "    }\n"
    "    float wet = texture2D(uField, uv).a;\n"                /* land holds still: the wall a wave bounces off */
    "    hn *= smoothstep(0.35, 0.65, wet);\n"
    "    hn = clamp(hn, -0.6, 0.6);\n"
    /* THE TRAIL, in its own channel and OUTSIDE the wave equation. The height field
       oscillates by construction: whatever the hull pushes down comes back up and
       radiates away, so a wake read off it always breaks into separate crests or
       rings, however it is tuned. This third channel only remembers where the hull has
       passed and fades: it is swept along the same segment, so it cannot have a gap in
       it, and it is what the surface draws as white water. The ripples the press
       throws off still come from the height field, through the normal. */
    "    float trail = max(texture2D(uPrev, uv).b * uTrailDecay, press * 3.2);\n"
    "    trail *= smoothstep(0.35, 0.65, wet);\n"
    "    gl_FragColor = vec4(hn, h, clamp(trail, 0.0, 1.0), 1.0);\n"
    "}\n";

static int wave_rt_init(FxRT* rt, int w, int h)
{
    if (rt->fbo && rt->w == w && rt->h == h) return 1;
    fx_rt_free(rt);
    glGenTextures(1, &rt->tex);
    glBindTexture(GL_TEXTURE_2D, rt->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
    fx_glGenFramebuffers(1, &rt->fbo);
    fx_glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
    fx_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->tex, 0);
    const GLenum st = fx_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    rt->w = w; rt->h = h; rt->depth = 0;
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "WATER|wave|half-float target refused (0x%x); the sea holds still\n", (unsigned)st);
        fx_rt_free(rt);
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return 1;
}

static void wave_clear(void)
{
    for (int i = 0; i < 2; i++) {
        fx_rt_bind(&g_waveRT[i]);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    g_waveStepped = 0;
}

/* Advance the field to this engine frame: one step per tick advanced, at most four
   (a script that steps thirty ticks between frames is not asked to simulate them all).
   Returns 1 when a field exists to be read. */
static int water_wave_sim(void)
{
    if (!g_fx.water_wake_sim || !g_waterFieldTex) return 0;
    if (!fx_gl_ready) return 0;
    if (!g_waveProg && !g_waveProgTried) {
        g_waveProgTried = 1;
        char* src = rain_glsl_source(WAVE_FS);
        g_waveProg = src ? fx_program(src, "wave") : 0;
        free(src);
    }
    if (!g_waveProg) return 0;

    int res = WAVE_RES;
    while (res > 1 && (g_gridW * res > WAVE_MAXW || g_gridH * res > WAVE_MAXW)) res /= 2;
    const int w = g_gridW * res, h = g_gridH * res;
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    const int fresh = (w != g_waveW || h != g_waveH || strcmp(g_waveScen, g_pack.scen) != 0);
    if (!wave_rt_init(&g_waveRT[0], w, h) || !wave_rt_init(&g_waveRT[1], w, h)) {
        fx_rt_bind(&g_fxScene); glViewport(vp[0], vp[1], vp[2], vp[3]);
        return 0;
    }
    if (fresh) {
        g_waveW = w; g_waveH = h;
        snprintf(g_waveScen, sizeof g_waveScen, "%s", g_pack.scen);
        wave_clear();
        g_waveFrame = g_engineFrame;
        g_waveBoats.clear();
    }

    /* the boats: where they are drawn, and how far they moved since the last frame */
    float boats[WAVE_MAXBOATS][4], boatsPrev[WAVE_MAXBOATS][4]; int nb = 0;
    const int advanced = g_engineFrame - g_waveFrame;
    for (size_t i = 0; i < g_objects.size() && nb < WAVE_MAXBOATS; i++) {
        const SimObject& o = g_objects[i];
        if (!water_is_boat(o)) continue;
        WaveBoat* b = NULL;
        for (size_t k = 0; k < g_waveBoats.size(); k++) if (g_waveBoats[k].id == o.id) { b = &g_waveBoats[k]; break; }
        if (!b) { WaveBoat nbt; nbt.id = o.id; nbt.have = 0; nbt.lx = nbt.lz = 0.0f; nbt.x = nbt.z = 0.0f; nbt.lastFrame = g_engineFrame; g_waveBoats.push_back(nbt); b = &g_waveBoats.back(); }
        float speed = 0.0f;
        if (b->have && advanced > 0) {
            const float dx = o.wx - b->x, dz = o.wz - b->z;
            speed = sqrtf(dx * dx + dz * dz) / (float)advanced;   /* cells a tick */
        }
        if (advanced > 0) { b->lx = b->x; b->lz = b->z; b->x = o.wx; b->z = o.wz; b->have = 1; b->lastFrame = g_engineFrame; }
        const float push = g_fx.water_wake * (speed > 0.002f ? (0.18f + 6.0f * speed) : 0.0f);
        /* A Red Alert map can float twenty ships, most of them at anchor; the four
           presses go to the ones under way. */
        if (water_src_ra() && push <= 0.0f) continue;
        /* TWO SOURCES, ONE EITHER SIDE OF THE KEEL. A single point leaves one band
           astern; a hull displaces water at both shoulders, and two trains a beam apart
           interfere into the two arms of a V. The beam is across the direction of
           travel, taken from the step just made. */
        float dxn = o.wx - b->lx, dzn = o.wz - b->lz;
        float dl = sqrtf(dxn * dxn + dzn * dzn);
        if (dl < 1.0e-4f) { dxn = 1.0f; dzn = 0.0f; dl = 1.0f; }
        dxn /= dl; dzn /= dl;
        const float beam = 0.045f * g_fx.water_wake_width;
        /* ASTERN, NOT UNDER THE KEEL. A hull's wake begins where the hull ENDS: pressing
           at the boat's own centre put the disturbance under the boat, which is where no
           wake has ever been. Half a cell back is the gunboat's stern at this scale. */
        const float stern = 0.45f;
        for (int side = 0; side < 2 && nb < WAVE_MAXBOATS; side++) {
            const float sgn = side ? 1.0f : -1.0f;
            const float sx = o.wx - dxn * stern - dzn * beam * sgn;
            const float sz = o.wz - dzn * stern + dxn * beam * sgn;
            /* and where the same shoulder was at the last step, so the press sweeps */
            const float px = b->lx - dxn * stern - dzn * beam * sgn;
            const float pz = b->lz - dzn * stern + dxn * beam * sgn;
            boats[nb][0] = sx / (float)g_gridW;
            boats[nb][1] = sz / (float)g_gridH;
            boatsPrev[nb][0] = (b->have ? px : sx) / (float)g_gridW;
            boatsPrev[nb][1] = (b->have ? pz : sz) / (float)g_gridH;
            boatsPrev[nb][2] = 0.0f; boatsPrev[nb][3] = 0.0f;
            boats[nb][2] = push * 0.6f;
            /* A SHOULDER, NOT A CRATER, and measured against the hull rather than
               guessed: at 0.26 the disturbance spanned three and a half boat lengths
               on screen. */
            /* never below one texel of the field, or the press falls between samples */
            const float rCells = 0.045f * g_fx.water_wake_width;
            const float rFloor = 1.0f / (float)WAVE_RES;
            boats[nb][3] = (rCells > rFloor ? rCells : rFloor) / (float)g_gridW;
            nb++;
        }
    }
    for (size_t k = 0; k < g_waveBoats.size(); ) {
        if (g_engineFrame - g_waveBoats[k].lastFrame > 30) g_waveBoats.erase(g_waveBoats.begin() + k); else k++;
    }

    int steps = advanced;
    if (steps > 4) steps = 4;
    if (steps < 0) steps = 0;
    g_waveFrame = g_engineFrame;
    /* WHAT THE FULLSCREEN QUAD TAKES AWAY, AND THE WORLD STILL NEEDS BACK.
       fx_fullscreen_quad disables the depth test, blend, the alpha test and culling and
       puts NONE of them back -- see fx_gl.h. Inside the post chain that is harmless,
       because every pass there states its own and the chain restores for the HUD. THIS
       SIM RUNS IN THE MIDDLE OF THE WORLD PASS: draw_water is reached before the
       terrain, the decals, the crystals and every object, so leaving the depth test off
       drew all of that with no depth test at all.

       AND ONLY ON THE FRAMES THE ENGINE TICKED, because that is the only time there are
       steps to take. At sixty drawn frames against a fifteen-hertz brain that is one
       frame in four, which is exactly what the flashing shadows were: three frames
       correct, one drawn flat, over and over. Zoom made it obvious rather than caused
       it -- a soft shadow edge covers more screen close in. The bits are read here and
       put back below, so nothing outside this function can tell the sim ran. */
    const GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean wasBlend = glIsEnabled(GL_BLEND);
    const GLboolean wasAlpha = glIsEnabled(GL_ALPHA_TEST);
    const GLboolean wasCull  = glIsEnabled(GL_CULL_FACE);
    GLboolean wasMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &wasMask);
    if (steps > 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_waterFieldTex);
        glActiveTexture(GL_TEXTURE0);
        for (int st = 0; st < steps; st++) {
            const int src = g_waveCur, dst = 1 - g_waveCur;
            fx_rt_bind(&g_waveRT[dst]);
            glBindTexture(GL_TEXTURE_2D, g_waveRT[src].tex);
            fx_glUseProgram(g_waveProg);
            fx_set1i(g_waveProg, "uPrev", 0);
            fx_set1i(g_waveProg, "uField", 1);
            fx_set2f(g_waveProg, "uTexel", 1.0f / (float)w, 1.0f / (float)h);
            /* c2 is the square of the wave speed in texels a step, and the explicit
               scheme is stable up to 0.5. At 0.22 the ripples travelled a tenth of a
               cell a tick, slower than the boat, so they piled up under the hull and
               never opened into a wake; 0.45 puts them well ahead of it. */
            /* THE WAVE SPEED DECIDES WHETHER THERE IS A V AT ALL, and it is a dial
               because it is a judgement about a specific boat. A wake opens into a V
               only where the hull outruns its own waves; where the waves are faster
               they close ahead of it and the wake is a RING, which is what the first
               two settings drew (0.45 and 0.06, both above the gunboat's 0.045 cells a
               tick). water_wake_speed is that speed in cells a tick, and the field's
               constant is its square in texels a step: 0.02 against the gunboat's
               0.045 puts the hull comfortably ahead and opens the V. */
            {
                const float cellsPerTick = g_fx.water_wake_speed;
                const float texPerStep = cellsPerTick * (float)WAVE_RES;
                fx_set1f(g_waveProg, "uC2", texPerStep * texPerStep);
            }
            fx_set1f(g_waveProg, "uDamp", 0.980f + 0.019f * g_fx.water_wake_ring);
            /* the trail's own memory, in the same dial's terms: about a second and a
               half at the shipped value, which is one boat length of visible wake */
            /* the trail's own memory, and it is NOT the ripple's damping: at 0.87 a
               tick the wake was gone within half a cell of the stern. 0.96 gives about
               two seconds, which at the gunboat's pace is a boat length of wake. */
            fx_set1f(g_waveProg, "uTrailDecay", 0.955f + 0.03f * g_fx.water_wake_ring);
            fx_set1i(g_waveProg, "uNBoat", nb);
            fx_set4fv(g_waveProg, "uBoat", WAVE_MAXBOATS, &boats[0][0]);
            fx_set4fv(g_waveProg, "uBoatPrev", WAVE_MAXBOATS, &boatsPrev[0][0]);
            /* the rain's drops, seeded by the step about to be taken (rain_mod.h) */
            rain_wave_uniforms(g_waveProg, g_waveStepped);
            fx_fullscreen_quad();
            fx_glUseProgram(0);
            g_waveCur = dst;
            g_waveStepped++;
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    fx_rt_bind(&g_fxScene);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    /* and the world's own state, exactly as it was handed over */
    if (wasDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (wasBlend) glEnable(GL_BLEND);      else glDisable(GL_BLEND);
    if (wasAlpha) glEnable(GL_ALPHA_TEST); else glDisable(GL_ALPHA_TEST);
    if (wasCull)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    glDepthMask(wasMask);
    if (getenv("OPENCNC3D_WATER_WAVEDBG")) {
        GLint mtu = 0, mtiu = 0, mctiu = 0;
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &mtu);
        glGetIntegerv(0x8872 /* GL_MAX_TEXTURE_IMAGE_UNITS */, &mtiu);
        glGetIntegerv(0x8B4D /* GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS */, &mctiu);
        fprintf(stderr, "WAVE|steps=%d|total=%d|size=%dx%d|boats=%d|push=%.3f|prog=%u|units=%d/%d/%d|err=0x%x\n",
                steps, g_waveStepped, w, h, nb, nb ? boats[0][2] : 0.0f, (unsigned)g_waveProg,
                (int)mtu, (int)mtiu, (int)mctiu, (unsigned)glGetError());
    }
    return g_waveStepped > 0;
}

/* The world-space programs. fx_program's vertex half is a clip-space quad for the post
   passes; the sea is geometry under the fixed pipe's own camera, so these carry their
   own vertex shaders and link the pairs here.

   THE SURFACE (WATER_VS/FS) draws the holed cells between the seabed and the terrain,
   exactly where draw_water draws, and takes with it everything draw_terrain would know
   about the cell: its atlas rectangle, the per-corner light and tint. Near the coast it
   walks back along the field to the LAND texel and lets that colour show through the
   wash, still carrying the ripple pattern: sand seen through shallow water. That is the
   "shore going into the water and fading away"; the first two drafts painted the sand
   OVER the wash and it read as a pale halo.

   THE SHORE (SHORE_VS/FS) draws after the terrain over every holed cell and its eight
   neighbours: the wash lapping a little way UP the sand (fading to nothing
   water_foam_cross cells inland), and the foam, whose crests roll over the edge. The
   opaque art edge is then a soft tone change inside a band of water rather than the
   one-texel step it used to be. */
static const char* WATER_VS =
    "#version 120\n"
    "varying vec2 vUV1; varying vec2 vUV2; varying vec3 vTint;\n"
    "varying vec3 vWorld; varying vec4 vCol;\n"
    "void main() {\n"
    "    vUV1 = gl_MultiTexCoord0.xy;\n"
    "    vUV2 = gl_MultiTexCoord1.xy;\n"
    "    vTint = gl_MultiTexCoord2.xyz;\n"
    "    vWorld = gl_Vertex.xyz;\n"
    "    vCol = gl_Color;\n"                       /* rgb = shroud, a = terrain light */
    "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
    "}\n";

/* No "#version" line of its own: assembled by rain_glsl_source, see WAVE_FS. */
static const char* WATER_FS =
    "uniform sampler2D uW1; uniform sampler2D uW2; uniform sampler2D uField; uniform sampler2D uLand;\n"
    "#ifdef WATER_RA_BASE\n"
    "uniform float uBaseScale; uniform float uBaseGain;\n"
    "#endif\n"
    /* THE RAIN on the surface (rain_mod.h sets these, zero when it is off): impact
       rings over the wave field, a wider window on the field's small ripples so a
       drop's ring counts, and the overcast sky the water then mirrors. */
    "uniform float uRainRipple; uniform float uRainRippleScale; uniform float uRainRippleRate; uniform float uRainTime;\n"
    "uniform float uRainLive; uniform float uRainOvercast; uniform vec3 uRainSky; uniform float uRainSea; uniform float uRainRippleDensity;\n"
    "uniform vec2  uFieldScale;\n"
    "uniform float uTime; uniform float uRange; uniform float uAlpha;\n"
    "uniform vec2  uCurrent;\n"
    "uniform float uFlow; uniform float uFlowRate; uniform float uPan; uniform float uRiverSpeed;\n"
    "uniform float uFoamWidth; uniform float uShallow; uniform float uDeep;\n"
    "uniform float uFade; uniform float uFadeWidth; uniform float uProductMean;\n"
    "uniform float uEdge; uniform float uWaveLen; uniform float uWaveSpeed;\n"
    "uniform sampler2D uNormal; uniform sampler2D uScene;\n"
    "uniform vec2 uViewport; uniform vec3 uCamPos; uniform vec3 uSunDir;\n"
    "uniform sampler2D uRefl; uniform int uReflOn;\n"
    "uniform float uBump; uniform float uReflect; uniform float uSpec; uniform float uRefract; uniform float uFresnel;\n"
    "uniform float uNatural; uniform float uDark;\n"
    "uniform sampler2D uWave; uniform int uWaveOn; uniform vec2 uWaveTexel; uniform float uWaveBump; uniform float uWaveFoam;\n"
    /* EVERY STRETCH OF COAST ON ITS OWN CLOCK. A breathing term that is a function of
       time and distance alone pulses in step along the whole shoreline, and the loop
       reads as one metronome (the director: "the entire shoreline has the exact same
       loop, instead of it being broken up in random sections, with their own loops").
       So each breathing term takes a phase and a period from a broad noise field in
       world space, one repeat per seventeen cells: neighbouring sections drift in and
       out of step and no two loops are the same length. */
    /* the local clock, read off the ripple normal map at a broad scale: eight texture
       units is all this GL has (GL_MAX_TEXTURE_UNITS 8, and glActiveTexture refuses the
       ninth with GL_INVALID_OPERATION), so the phase field and the wave field share one
       slot's worth of budget and this one comes out of a texture already bound. */
    "vec2 clockAt(vec2 p) { vec4 n = texture2D(uNormal, p * 0.06); return vec2((n.r + n.g) * 3.1415927, 0.7 + 0.6 * n.b); }\n"
    "varying vec2 vUV1; varying vec2 vUV2; varying vec3 vTint;\n"
    "varying vec3 vWorld; varying vec4 vCol;\n"
    "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "void main() {\n"
    /* RED ALERT: how blue the painted floor under this pixel is against its own
       brightness (five taps, so no single texel speckles). Painted water measures 0.3 to
       0.45; mud, rock, grass and sand measure under 0.1. Used twice below, where the 1995
       cut and the 144 painting disagree about the coast. */
    "#ifdef WATER_RA_BASE\n"
    "    vec2 fsuv = gl_FragCoord.xy / uViewport, fpx = 2.0 / uViewport;\n"
    "    vec3 fl = texture2D(uScene, fsuv).rgb + texture2D(uScene, fsuv + vec2(fpx.x, 0.0)).rgb\n"
    "            + texture2D(uScene, fsuv - vec2(fpx.x, 0.0)).rgb + texture2D(uScene, fsuv + vec2(0.0, fpx.y)).rgb\n"
    "            + texture2D(uScene, fsuv - vec2(0.0, fpx.y)).rgb;\n"
    "    float floorBlue = (fl.b - fl.r) / (dot(fl, vec3(0.299, 0.587, 0.114)) + 0.02);\n"
    "#endif\n"
    "    vec4 f = texture2D(uField, vWorld.xz * uFieldScale);\n"
    "    float sd = (f.r - 0.5) * 2.0 * uRange;\n"           /* cells, + on the water */
    "    vec2 away = f.gb * 2.0 - 1.0;\n"                     /* out to sea */
    "    float al = length(away);\n"
    "    vec2 awayN = al > 0.08 ? away / al : vec2(0.0);\n"
    "    vec4 land = texture2D(uLand, vWorld.xz * uFieldScale);\n"
    /* SEA OR RIVER: the land texture's alpha, flat across the coast. */
    "    float river = 1.0 - smoothstep(0.55, 0.95, land.a);\n"
    /* THE MOTION. The sea PANS along the current, one uniform slide that can neither
       stretch nor seam, and turns shoreward inside the band by a spatially varying push;
       a river does not pan, it RUNS along its stored downstream tangent by the same push.
       The push is done twice half a cycle apart and cross-faded so the wrap is never on
       screen (the two-phase flow-map trick). Sample offsets are NEGATIVE along the
       motion: a pattern is seen to move toward +d when the sample point moves toward -d,
       and a first draft had it the other way round, so the sea ran against its own
       current. */
    "    float shoreW = clamp(1.0 - sd / (uFoamWidth * 3.0), 0.0, 1.0) * clamp(al * 3.0, 0.0, 1.0);\n"
    "    vec2 seaPush = uFlow * mix(vec2(0.0), -awayN, shoreW);\n"
    "    float bankDrag = 0.45 + 0.55 * smoothstep(0.0, 0.5, sd);\n"      /* a river slows at its banks */
    "    vec2 push = mix(seaPush, awayN * uFlow * uRiverSpeed * bankDrag, river);\n"
    "    vec2 pan = uCurrent * uPan * uTime * (1.0 - river);\n"
    "    float t  = uTime * uFlowRate;\n"
    "    float p0 = fract(t), p1 = fract(t + 0.5);\n"
    "    float w  = abs(1.0 - 2.0 * p0);\n"
    "    vec2 o0 = -pan - push * (p0 - 0.5), o1 = -pan - push * (p1 - 0.5);\n"
    "#ifdef WATER_RA_BASE\n"
    /* Red Alert's painting repeats every two cells, not every half cell: the slide is
       scaled to it so the sea moves as far on the ground as Tiberian Dawn's does. */
    "    vec3 c0 = texture2D(uW1, vUV1 + o0 * uBaseScale).rgb * texture2D(uW2, vUV2 + o0 * uBaseScale * 0.6).rgb;\n"
    "    vec3 c1 = texture2D(uW1, vUV1 + o1 * uBaseScale).rgb * texture2D(uW2, vUV2 + o1 * uBaseScale * 0.6).rgb;\n"
    "#else\n"
    "    vec3 c0 = texture2D(uW1, vUV1 + o0).rgb * texture2D(uW2, vUV2 + o0 * 0.6).rgb;\n"
    "    vec3 c1 = texture2D(uW1, vUV1 + o1).rgb * texture2D(uW2, vUV2 + o1 * 0.6).rgb;\n"
    "#endif\n"
    "    vec3 col = mix(c0, c1, w);\n"
    /* a ground painting is drawn under the terrain's light (160/255 on level ground);
       the sea is not lit by the terrain, so the painting takes that light here */
    "#ifdef WATER_RA_BASE\n"
    "    col *= uBaseGain;\n"
    "#endif\n"
    /* A MORE NATURAL COLOUR. The console's product is a saturated cobalt; real water is
       greyer and a touch greener. Part desaturation, part a tint that eases the blue
       and lifts the green, on one dial (water_natural) so the cartridge's colour is
       one turn away. The director: "the water is very blue, turn down the blue
       slightly to make it a bit more realistically coloured". */
    /* NOT ON RED ALERT'S SEA. The ease is aimed at the cartridge's cobalt; Red Alert's
       base is its own painted water, already a grey blue, and the same ease turned it
       teal. Its hue is the painting's, so the step is compiled out of that variant.
       The plain darkening below changes no hue and stays for both. */
    "#ifndef WATER_RA_BASE\n"
    "    col = mix(col, vec3(luma(col)), uNatural * 0.25);\n"
    "    col *= mix(vec3(1.0), vec3(0.92, 1.03, 0.84), uNatural);\n"
    "#endif\n"
    /* and a plain darkening on top of the tint, because "less blue" and "darker" are
       two different asks and one dial cannot serve both */
    "    col *= 1.0 - uDark;\n"
    /* SHALLOWS AND DEPTH. */
    "    float shallow = clamp(1.0 - sd / uFoamWidth, 0.0, 1.0);\n"
    "    float deep    = smoothstep(uFoamWidth, uRange, sd);\n"
    "    col *= 1.0 - uDeep * 0.5 * deep;\n"
    "    float a = uAlpha * (1.0 - uShallow * 0.6 * shallow);\n"
    /* THE BEACH THROUGH THE WATER. */
    "    if (uFade > 0.0 && sd < uFadeWidth) {\n"
    "        if (land.a > 0.3) {\n"
    "            vec3 sand = max(land.rgb - vTint, 0.0) * vCol.a;\n"
    /* the ripple pattern stays on the sand: the product's brightness about its mean */
    "            float ripple = clamp(luma(col) / max(uProductMean, 0.02), 0.55, 1.45);\n"
    "            vec3 wet = sand * 0.9 * ripple;\n"
    "            float x = clamp(sd / uFadeWidth, 0.0, 1.0);\n"
    "            float k = uFade * pow(1.0 - x, 1.6);\n"
    /* At the coast the water is a film: three quarters sand, a quarter tile, the ripple
       kept on the sand by the factor above, and the floor hidden. The shore pass laps
       the same quarter of tile onto the dry sand on the other side of the art edge, so
       the coast is continuous through the cut. */
    "            col = mix(col, wet, k * 0.9);\n"
    "            a = a + (1.0 - a) * k;\n"
    "        }\n"
    "    }\n"
    /* THE BRIGHT EDGE. Water arriving at a shore is paler than the water behind it,
       and that pale band is what reads as "the edge" from this camera: a soft rim from
       the coast out to the edge width, brightest at the coast, breathing a little on
       the crest rhythm, and half as wide on a river so it hugs both banks instead of
       filling the channel. Not foam: no texture, no threshold, no patches. */
    "    float rimW = uFoamWidth * (1.0 - 0.5 * river);\n"
    /* A long, gentle falloff: strongest at the coast, gone at the edge width, with most
       of the way out spent in the pale tail rather than in the bright part. */
    "    float rim = uEdge * pow(1.0 - clamp(sd / rimW, 0.0, 1.0), 1.6);\n"
    /* Red Alert: the pale rim belongs to the painted water. Where the cut says sea over a
       painting's wet slate or mud, the rim at full strength drew a pale smudge on land. */
    "#ifdef WATER_RA_BASE\n"
    "    rim *= smoothstep(0.15, 0.30, floorBlue);\n"
    "#endif\n"
    "    vec2 clk = clockAt(vWorld.xz);\n"
    "    rim *= 0.82 + 0.18 * sin(6.2831853 * (sd / uWaveLen + uTime * uWaveSpeed * clk.y / uWaveLen) + clk.x);\n"
    "    col = mix(col, vec3(0.70, 0.86, 1.0), rim * 0.55);\n"
    "    a = a + (1.0 - a) * rim * 0.40;\n"
    /* THE SURFACE. Two reads of the normal map, a broad one riding with the water and
       a finer one crossing it, give a per-pixel normal. What is UNDER the water is the
       frame as it stood before this pass (the sea floor), read back displaced by that
       normal: refraction. What is ABOVE it is, for now, the sky, by the reflected view
       ray: Fresnel decides how much of each. The sun adds a glint. The wash the console
       draws is composed over the refracted floor here, in the shader, so the pixel
       leaves opaque. */
    "    vec3 t1 = texture2D(uNormal, vWorld.xz * 0.55 + o0 * 0.6).xyz * 2.0 - 1.0;\n"
    "    vec3 t2 = texture2D(uNormal, vWorld.xz * 1.35 - o1 * 0.45 + 0.5).xyz * 2.0 - 1.0;\n"
    "    vec2 nxy = (t1.xy + t2.xy) * uBump;\n"
    /* THE WAVE FIELD: the boats' wakes and whatever else moved the water, read as a
       slope into the normal, and as a soft white where a crest is steep. */
    "    float crest = 0.0;\n"
    "    if (uWaveOn == 1) {\n"
    "        vec2 wuv = vWorld.xz * uFieldScale;\n"
    "        float amp = abs(texture2D(uWave, wuv).r);\n"
    "        float hx = texture2D(uWave, wuv + vec2(uWaveTexel.x, 0.0)).r - texture2D(uWave, wuv - vec2(uWaveTexel.x, 0.0)).r;\n"
    "        float hz = texture2D(uWave, wuv + vec2(0.0, uWaveTexel.y)).r - texture2D(uWave, wuv - vec2(0.0, uWaveTexel.y)).r;\n"
    /* the difference is per TEXEL of the field, and a texel is a quarter of a cell, so
       the slope in world terms is that difference times the field's resolution; without
       the scale the wake tilted the surface by a fiftieth of what it should and read as
       nothing at all */
    "        vec2 g = vec2(hx, hz) * 0.5 / uWaveTexel.x * uFieldScale.x;\n"
    /* ONE WAKE, NOT FIVE. The field carries every ripple the hull has ever made, and
       drawn on slope alone they all read equally: the director saw five wakes where
       there should be one clear one and a fading remainder. Weighting by AMPLITUDE
       does it, because damping is what tells the ripples apart -- the freshest is the
       tallest, and the fifth one back is a tenth of it. The curve is steep on purpose. */
    /* the window is in field units and has to follow the press: a narrower, gentler
       hull makes a smaller wave, and at the old floor the whole wake fell under it */
    "        float live = smoothstep(0.004 * uRainLive, 0.045 * uRainLive, amp);\n"
    "        live *= live;\n"
    "        nxy += g * uWaveBump * live;\n"
    /* WHAT IS SEEN IS THE TROUGH, NOT THE CRESTS. The hull's press is swept along the
       path it took, so the water it has pushed DOWN is a continuous furrow; the crests
       are the ripples that furrow throws off, and reading them is what put a chain of
       separate rings behind the boat, however they were tuned. So the white follows
       the depression, which cannot have gaps in it, and the ripples keep the surface
       shading they already had through the normal above. */
    "        float trail = texture2D(uWave, wuv).b;\n"
    /* The white is a FILM on the water, not paint: at 0.55 the middle of the trail
       saturated and read as an opaque triangle behind the stern. Half that, over a
       wider ramp so the centre never reaches full white, leaves the sea visible
       through it. */
    "        crest = smoothstep(0.03, 0.32, trail) * uWaveFoam * 0.30;\n"
    "    }\n"
    /* THE RAIN'S RINGS on the surface itself, the same rings the ground gets, so a
       drop landing on the water and one landing on the sand beside it look alike. */
    "    if (uRainRipple > 0.0)\n"
    "        nxy += rn_ripple(vWorld.xz, uRainTime, uRainRippleScale, uRainRippleRate, uRainRippleDensity) * (uRainRipple * 0.15);\n"
    "    vec3 N = normalize(vec3(nxy.x, 1.0, nxy.y));\n"
    "    vec3 V = normalize(uCamPos - vWorld);\n"
    "    vec3 L = normalize(uSunDir);\n"
    "    vec3 H = normalize(L + V);\n"
    "    float spec = pow(max(dot(N, H), 0.0), 48.0) * uSpec;\n"
    "    float fres = uFresnel + (1.0 - uFresnel) * pow(1.0 - max(dot(N, V), 0.0), 5.0);\n"
    "    float bend = uRefract * smoothstep(0.0, 0.6, sd);\n"
    "    vec2 suv = gl_FragCoord.xy / uViewport;\n"
    "    vec3 under = texture2D(uScene, clamp(suv + nxy * bend, vec2(0.001), vec2(0.999))).rgb;\n"
    "    vec3 body = mix(under, col, a);\n"
    /* UNDER RAIN THE SEA TAKES THE WEATHER TOO, and for a long time it did not: the
       overcast was folded into the CLEAR-SKY colour alone, and that colour is thrown
       away whenever the planar reflection is on, which it normally is. So the land
       greyed out around a sea that stayed a bright sunny blue. Three places now, all
       on ONE DIAL OF THEIR OWN rather than on the overcast: tried as a factor of the
       overcast first, and at that dial's own setting the whole term came to under half
       strength and moved the sea's saturation by a twentieth, which reads as nothing.
       What the water does under weather is its own decision to make. Zero draws exactly
       what drew before. The three are: the mirrored sky; the reflection itself, a redraw of a dry,
       unlit, rainless world and so is eased toward the overcast rather than made
       right; and the water's own body, because an overcast changes the light falling
       ON the water and not only what it mirrors. The glint softens as it always did. */
    "    float seaw = uRainSea;\n"
    /* THE WEATHER GOES ON THE BODY, not on the finished pixel. At this camera the
       Fresnel term is small, so what leaves this shader is almost entirely the water
       body and the seabed under it; tinting the result moved the sea's saturation by
       a twentieth and read as no change at all. The body is the sea, so the body is
       what a grey day changes: drained toward the luminance of the overcast sky and
       darkened with it, because water under cloud is both greyer and dimmer. */
    "    body *= mix(vec3(1.0), vec3(0.72, 0.76, 0.82), seaw);\n"
    "    body = mix(body, uRainSky * (dot(body, vec3(0.299, 0.587, 0.114)) * 1.25), 0.70 * seaw);\n"
    "    vec3 R = reflect(-V, N);\n"
    "    vec3 sky = mix(vec3(0.66, 0.74, 0.82), vec3(0.34, 0.50, 0.76), clamp(R.y, 0.0, 1.0));\n"
    "    sky = mix(sky, uRainSky, uRainOvercast);\n"
    "    spec *= 1.0 - 0.6 * uRainOvercast;\n"
    "    vec3 above = uReflOn == 1 ? texture2D(uRefl, clamp(suv + nxy * uRefract * 2.5, vec2(0.001), vec2(0.999))).rgb : sky;\n"
    "    above = mix(above, uRainSky, seaw);\n"
    "    vec3 outc = mix(body, above, clamp(fres * uReflect, 0.0, 1.0)) + spec * vec3(1.0, 0.97, 0.90);\n"
    "    outc = mix(outc, vec3(0.90, 0.94, 0.98), crest);\n"
    /* WHERE THE PAINTING SAYS LAND (Red Alert): the cut is the 1995 tile's, and where a
       144-texel painting moved its coast the cut puts water over painted mud. There the
       surface would lay its rim, wash and foam on land, a pale smudge. The floor under
       this pixel is that painting, so it is asked: painted water is blue against its own
       brightness, land (mud, rock, grass, sand) is not. Five taps, so single texels of
       either cannot speckle; the measured split over every shore painting is about one
       texel in twenty either way at these limits. Where it is land the painted floor is
       left as it is. */
    "#ifdef WATER_RA_BASE\n"
    "    {\n"
    "        float paintedLand = 1.0 - smoothstep(0.05, 0.20, floorBlue);\n"
    "        gl_FragColor = vec4(mix(outc * vCol.rgb, texture2D(uScene, suv).rgb, paintedLand), 1.0);\n"
    "        return;\n"
    "    }\n"
    "#endif\n"
    "    gl_FragColor = vec4(outc * vCol.rgb, 1.0);\n"
    "}\n";

static const char* SHORE_VS =
    "#version 120\n"
    "varying vec3 vWorld; varying vec4 vCol;\n"
    "void main() {\n"
    "    vWorld = gl_Vertex.xyz;\n"
    "    vCol = gl_Color;\n"                       /* rgb = shroud, a = terrain light */
    "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
    "}\n";
/* Red Alert's: the same, and the ground sheet's coordinate for the cell (unit 5), so the
   shore can read the ground's own cut (SHORE_FS, WATER_RA_BASE). */
static const char* SHORE_VS_RA =
    "#version 120\n"
    "varying vec3 vWorld; varying vec4 vCol; varying vec2 vSheet;\n"
    "void main() {\n"
    "    vWorld = gl_Vertex.xyz;\n"
    "    vCol = gl_Color;\n"
    "    vSheet = gl_MultiTexCoord5.xy;\n"
    "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
    "}\n";

static const char* SHORE_FS =
    "#version 120\n"
    "%s"                                       /* the variant's #define, or nothing */
    "uniform sampler2D uW1; uniform sampler2D uW2; uniform sampler2D uField; uniform sampler2D uLand; uniform sampler2D uFoamTex;\n"
    "uniform vec2  uFieldScale;\n"
    "uniform float uTime; uniform float uRange; uniform float uAlpha;\n"
    "uniform vec2  uPanV; uniform vec2 uCurrent; uniform float uPan;\n"
    "uniform float uFlow; uniform float uFlowRate; uniform float uRiverSpeed;\n"
    "uniform float uFoam; uniform float uFoamWidth; uniform float uFoamCross;\n"
    "uniform float uWaveLen; uniform float uWaveSpeed; uniform float uEdge; uniform float uFade;\n"
    "uniform float uProductMean; uniform float uNatural; uniform float uDark;\n"
    "#ifdef WATER_RA_BASE\n"
    "uniform float uBaseRepeat; uniform float uBaseGain;\n"
    "uniform sampler2D uSheet; varying vec2 vSheet;\n"
    "#endif\n"
    "varying vec3 vWorld; varying vec4 vCol;\n"
    "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    /* the same local clock the surface uses (see WATER_FS), read off the foam sheet */
    "vec2 clockAt(vec2 p) { vec4 n = texture2D(uFoamTex, p * 0.06); return vec2(n.r * 6.2831853, 0.7 + 0.6 * n.g); }\n"
    "void main() {\n"
    "    vec4 f = texture2D(uField, vWorld.xz * uFieldScale);\n"
    "    float sd = (f.r - 0.5) * 2.0 * uRange;\n"
    "    if (sd <= -uFoamCross || sd >= uFoamWidth) discard;\n"
    /* Red Alert: the painting's own blueness here, as WATER_FS measures its floor */
    "#ifdef WATER_RA_BASE\n"
    "    vec2 stx = vec2(dFdx(vSheet.x), dFdy(vSheet.y));\n"
    "    vec3 pl = texture2D(uSheet, vSheet).rgb + texture2D(uSheet, vSheet + vec2(stx.x, 0.0)).rgb\n"
    "            + texture2D(uSheet, vSheet - vec2(stx.x, 0.0)).rgb + texture2D(uSheet, vSheet + vec2(0.0, stx.y)).rgb\n"
    "            + texture2D(uSheet, vSheet - vec2(0.0, stx.y)).rgb;\n"
    "    float paintBlue = (pl.b - pl.r) / (luma(pl) + 0.02);\n"
    "#endif\n"
    "    vec2 dir = f.gb * 2.0 - 1.0;\n"
    "    float dl = length(dir);\n"
    "    vec2 dirN = dl > 0.08 ? dir / dl : vec2(1.0, 0.0);\n"
    "    float river = 1.0 - smoothstep(0.55, 0.95, texture2D(uLand, vWorld.xz * uFieldScale).a);\n"
    "#ifdef WATER_RA_BASE\n"
    "    vec2 base = vWorld.xz * uBaseRepeat;\n"
    "#else\n"
    "    vec2 base = vWorld.xz * 1.92;\n"
    "#endif\n"
    /* THE WASH UP THE SAND, and it is what makes the art's cut disappear. On the water
       side of the cut the surface shows sand under a pale wash (WATER_FS); this draws
       THE SAME THING over the dry sand, near enough opaque right at the cut and fading
       out over the reach, so a pixel one texel inland looks like a pixel one texel
       offshore and the line between them is gone. The reach breathes on the crest
       rhythm, so the water line creeps up the beach and draws back. A first lap was
       the bare blue tile at a third of the opacity, which left the cut as sharp as it
       had been and merely tinted the sand beside it. */
    "    float washA = 0.0; vec3 wash = vec3(0.0);\n"
    "    vec2 clk = clockAt(vWorld.xz);\n"
    "    float crestT = sin(6.2831853 * (sd / uWaveLen + uTime * uWaveSpeed * clk.y / uWaveLen) + clk.x);\n"
    /* ...and it overshoots a tenth of a cell PAST the field's zero line: the field is
       twelve texels a cell and the art twenty-four, so a sliver of opaque terrain can
       sit just on the water side of the field, and without this it showed through as
       a dark hairline of the art's own rim texels along every coast. */
    "    if (sd < 0.1) {\n"
    "        float reach = uFoamCross * (0.75 + 0.25 * sin(6.2831853 * uTime * uWaveSpeed * clk.y / max(uWaveLen, 0.05) + clk.x));\n"
    "        float k = sd < 0.0 ? max(1.0 + sd / reach, 0.0) : 1.0;\n"
    "        vec3 tile = texture2D(uW1, base - uPanV).rgb * texture2D(uW2, base - uPanV * 0.6).rgb;\n"
    "#ifdef WATER_RA_BASE\n"
    "        tile *= uBaseGain;\n"
    "#endif\n"
    "#ifdef WATER_RA_BASE\n"
    "        tile *= 1.0 - uDark;\n"                 /* the painting's own hue, as WATER_FS */
    "#else\n"
    "        tile = mix(tile, vec3(luma(tile)), uNatural * 0.25) * mix(vec3(1.0), vec3(0.92, 1.03, 0.84), uNatural) * (1.0 - uDark);\n"
    "#endif\n"
    "        vec4 land = texture2D(uLand, vWorld.xz * uFieldScale);\n"
    "        float ripple = clamp(luma(tile) / max(uProductMean, 0.02), 0.55, 1.45);\n"
    "        vec3 wet = land.rgb * vCol.a * 0.9 * ripple;\n"
    "        wash = mix(tile, wet, uFade * 0.9);\n"
    "        float rim = uEdge * (0.82 + 0.18 * crestT);\n"
    "#ifdef WATER_RA_BASE\n"
    "        rim *= smoothstep(0.15, 0.30, paintBlue);\n"     /* as WATER_FS's rim */
    "#endif\n"
    "        wash = mix(wash, vec3(0.70, 0.86, 1.0), rim * 0.55);\n"
    "        washA = 0.92 * k * k * (0.6 + 0.4 * k);\n"
    "    }\n"
    /* THE FOAM. The sheet is read in plain WORLD space, one repeat per three and a half
       cells, so a wisp is about a cell across. A first draft read it in a frame turned
       along the coast, to stretch the wisps into shore-parallel streaks; the frame
       swings through a full turn around every rock and the sample coordinate with it,
       and the result was contour lines, not foam. The shore-parallel structure comes
       from the crest term below instead, which is a function of the distance and so
       cannot swirl. The broad octave rides with the water at half its speed (on the sea
       the pan, on a river the same two-phase push the surface uses), the finer one
       crawls slowly on its own so nothing sits still. The threshold breathes with the
       crest rhythm, so patches swell toward the shore and thin again, and it is SOFT:
       a third of the range from nothing to full, so no edge is a paint stroke. */
    "    float t  = uTime * uFlowRate;\n"
    "    float p0 = fract(t), p1 = fract(t + 0.5);\n"
    "    float w  = abs(1.0 - 2.0 * p0);\n"
    "    vec2 fw = vWorld.xz * 0.28;\n"
    "    vec2 driftV = mix(uCurrent * uPan * uTime * 0.5, vec2(0.0), river);\n"
    "    vec2 pushV = mix(vec2(0.0), dirN * uFlow * uRiverSpeed * 0.5, river);\n"
    "    float nA = mix(texture2D(uFoamTex, fw - driftV - pushV * (p0 - 0.5)).r,\n"
    "                   texture2D(uFoamTex, fw - driftV - pushV * (p1 - 0.5)).r, w);\n"
    "    float nB = texture2D(uFoamTex, fw * 1.7 + vec2(0.37 + uTime * 0.015, 0.37)).g;\n"
    "    float n = nA * 0.75 + nB * 0.25;\n"
    "    float tt = (sd + uFoamCross) / (uFoamWidth + uFoamCross);\n"
    "    float band = pow(max(sin(3.14159265 * tt), 0.0), 1.5);\n"
    "    float crest = 0.5 + 0.5 * crestT;\n"
    "    float thr = 0.68 - 0.12 * crest;\n"
    "    float foam = uFoam * band * smoothstep(thr, thr + 0.35, n);\n"
    "    foam += 0.0;\n"
    /* A river a cell wide has two banks inside one surf band: a third of the foam. */
    "    foam  = clamp(foam, 0.0, 1.0) * (1.0 - 0.65 * river);\n"
    /* ON RED ALERT'S GROUND THE LAND IS THE PAINTING'S. Its sheet is solid wherever its
       cut says land and fades its own painted water into the sea on the water side, so
       there is no hard cut for the wash to hide; laid up the land as on Tiberian Dawn's
       sand it whitewashed every rock in the sea (a rock a cell across lies wholly inside
       the reach) into a pale ghost. So the wash and the foam lie only where the ground
       lets the sea through. */
    "#ifdef WATER_RA_BASE\n"
    "    float landA = texture2D(uSheet, vSheet).a;\n"
    /* and where the painting itself is land under a 1995 cut that says water (WATER_FS
       says how this is told) */
    "    landA = max(landA, 1.0 - smoothstep(0.05, 0.20, paintBlue));\n"
    "    washA *= 1.0 - landA;\n"
    "    foam  *= 1.0 - landA;\n"
    "#endif\n"
    "    vec3 foamCol = vec3(0.86, 0.92, 0.98);\n"
    "    float A = washA + foam * (1.0 - washA);\n"
    "    if (A < 0.002) discard;\n"
    "    vec3 rgb = (wash * washA * (1.0 - foam) + foamCol * foam) / A;\n"
    "    gl_FragColor = vec4(rgb * vCol.rgb, A);\n"
    "}\n";

static GLuint g_shoreProg = 0;
static int    g_shoreProgTried = 0;
/* THE RED ALERT VARIANTS of both programs (WATER_RA_BASE), built the first time Red
   Alert's ground asks; Tiberian Dawn's programs are compiled from the same text with the
   define absent, which the preprocessor makes the text they always were. */
static GLuint g_waterProgRa = 0, g_shoreProgRa = 0;
static int    g_waterProgRaTried = 0, g_shoreProgRaTried = 0;

/* Link one vertex + fragment pair; the post chain's fx_program fixes the vertex half. */
static GLuint water_link(const char* vs_src, const char* fs_src, const char* label)
{
    if (!fx_gl_ready) return 0;
    GLuint vs = fx_compile(GL_VERTEX_SHADER, vs_src, label);
    if (!vs) return 0;
    GLuint fs = fx_compile(GL_FRAGMENT_SHADER, fs_src, label);
    if (!fs) { fx_glDeleteShader(vs); return 0; }
    GLuint p = fx_glCreateProgram();
    fx_glAttachShader(p, vs);
    fx_glAttachShader(p, fs);
    fx_glLinkProgram(p);
    GLint ok = 0;
    fx_glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096]; GLsizei n = 0;
        if (fx_glGetProgramInfoLog) fx_glGetProgramInfoLog(p, (GLsizei)sizeof log, &n, log);
        log[n < (GLsizei)sizeof log ? n : (GLsizei)sizeof log - 1] = 0;
        fprintf(stderr, "FX|program|%s FAILED TO LINK\n%s\n", label, log);
        fx_glDeleteProgram(p);
        p = 0;
    }
    fx_glDeleteShader(vs);
    fx_glDeleteShader(fs);
    return p;
}

static GLuint water_prog_get(void)
{
    if (water_src_ra()) {
        if (g_waterProgRa || g_waterProgRaTried) return g_waterProgRa;
        g_waterProgRaTried = 1;
        const std::string body = std::string("#define WATER_RA_BASE 1\n") + WATER_FS;
        char* src = rain_glsl_source(body.c_str());
        g_waterProgRa = src ? water_link(WATER_VS, src, "water (red alert)") : 0;
        free(src);
        return g_waterProgRa;
    }
    if (g_waterProg || g_waterProgTried) return g_waterProg;
    g_waterProgTried = 1;
    {
        char* src = rain_glsl_source(WATER_FS);
        g_waterProg = src ? water_link(WATER_VS, src, "water") : 0;
        free(src);
    }
    return g_waterProg;
}
/* SHORE_FS carries one %s right after its #version line for the variant's define. */
static GLuint shore_prog_build(const char* define, const char* label, const char* vs)
{
    std::string src(SHORE_FS);
    const size_t at = src.find("%s");
    if (at != std::string::npos) src.replace(at, 2, define);
    return water_link(vs, src.c_str(), label);
}
static GLuint shore_prog_get(void)
{
    if (water_src_ra()) {
        if (g_shoreProgRa || g_shoreProgRaTried) return g_shoreProgRa;
        g_shoreProgRaTried = 1;
        g_shoreProgRa = shore_prog_build("#define WATER_RA_BASE 1\n", "shore (red alert)", SHORE_VS_RA);
        return g_shoreProgRa;
    }
    if (g_shoreProg || g_shoreProgTried) return g_shoreProg;
    g_shoreProgTried = 1;
    g_shoreProg = shore_prog_build("", "shore", SHORE_VS);
    return g_shoreProg;
}

/* Is this pass the one drawing the sea this frame? Enhanced with the chain running (so
   Classic and every measuring gate keep the cartridge's picture), the dial on, and a
   pack that carries the console's two water tiles. */
static bool water_fx_wanted(void)
{
    if (water_src_ra()) return g_fxActive && g_fx.water_fx && g_waterRa.ok;
    return g_fxActive && g_fx.water_fx && g_pack.waterTex1 >= 0 && g_pack.waterTex2 >= 0;
}

/* The terrain's own per-corner light and tint, draw_terrain's arithmetic corner for
   corner, so the sand seen through the water is lit exactly as the sand beside it. */
static void water_terrain_corner(int cx, int cz, bool tinted, bool oneSun,
                                 float* light, float* vis, float tint[3])
{
    *vis = shroud_corner_vis(cx, cz);
    *light = (oneSun ? 160.0f / 255.0f : terrain_shade(cx, cz)) * *vis;
    tint[0] = tint[1] = tint[2] = 0.0f;
    if (tinted) {
        unsigned char t[3];
        terrain_tint_shrouded(cx, cz, *vis, t);
        tint[0] = t[0] / 255.0f; tint[1] = t[1] / 255.0f; tint[2] = t[2] / 255.0f;
    }
}

/* Draw the sea through the shader. Returns false, having drawn nothing, when the
   cartridge's pass should run instead. The geometry, the console's UV law and the
   shroud byte are draw_water's own, verbatim; only the fragment arithmetic differs. */
static bool water_fx_draw(void)
{
    if (!water_fx_wanted()) return false;
    if (!water_field_sync()) return false;
    const GLuint prog = water_prog_get();
    if (!prog) return false;

    const float F = (float)g_engineFrame;
    const float phase = fmodf(F * 0.0625f, 6.2831853f);

    const bool tinted = terrain_tint_live();
    const bool oneSun = g_fxActive && g_fx.sun_lambert && g_fx.terrain_normals;

    water_foam_boot();
    water_normal_boot();
    const int waveOn = water_wave_sim();
    const std::vector<PackCell>& cells = water_cells();
    /* the sea's plane this frame: the mean corner height of the holed cells in view */
    int reflOn = 0;
    if (g_fx.water_reflect_scene) {
        double sumY = 0.0; int nY = 0;
        for (size_t i = 0; i < cells.size(); i++) {
            const PackCell& c = cells[i];
            if (!c.holes || !cell_shown(c.x, c.y) || !cell_in_view(c.x, c.y)) continue;
            sumY += terrain_corner_y(c.x, c.y); nY++;
        }
        if (nY > 0) {
            /* EVERY Nth FRAME, when asked. The mirror target persists between frames,
               so a frame that skips the redraw reflects the previous one; the ripple
               displacement hides a single frame of lag. Always redrawn when there is
               no previous mirror to show, so a skip can never bind an empty target. */
            static unsigned reflSerial = 0;
            static int reflValid = 0;
            const int every = (int)(g_fx.water_reflect_every + 0.5f);
            const int due = every <= 1 || !reflValid || (reflSerial % (unsigned)every) == 0;
            reflSerial++;
            if (due) { reflOn = water_reflection_render((float)(sumY / nY)); reflValid = reflOn; }
            else reflOn = 1;
        }
    }
    fprof_mark("water: mirror");
    int sceneW = 1, sceneH = 1;
    water_scene_grab(&sceneW, &sceneH);
    fx_sun_dir();
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, reflOn ? g_waterReflRT.tex : 0);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, g_waterSceneTex);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, g_waterNormalTex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, waveOn ? g_waveRT[g_waveCur].tex : 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_waterLandTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_waterFieldTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, water_base2());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, water_base1());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    fx_glUseProgram(prog);
    fx_set1i(prog, "uW1", 0);
    fx_set1i(prog, "uW2", 1);
    fx_set1i(prog, "uField", 2);
    fx_set1i(prog, "uLand", 3);
    fx_set1i(prog, "uNormal", 5);
    fx_set1i(prog, "uScene", 6);
    fx_set1i(prog, "uRefl", 7);
    fx_set1i(prog, "uReflOn", reflOn);
    fx_set2f(prog, "uViewport", (float)sceneW, (float)sceneH);
    fx_set3f(prog, "uCamPos", g_fxCamPos[0], g_fxCamPos[1], g_fxCamPos[2]);
    /* g_fxSunDir is the way the light travels; the shader wants the way TO the sun */
    fx_set3f(prog, "uSunDir", -g_fxSunDir[0], -g_fxSunDir[1], -g_fxSunDir[2]);
    fx_set1f(prog, "uBump", g_fx.water_bump);
    fx_set1f(prog, "uReflect", g_fx.water_reflect);
    fx_set1f(prog, "uSpec", g_fx.water_spec);
    fx_set1f(prog, "uRefract", g_fx.water_refract);
    fx_set1f(prog, "uFresnel", g_fx.water_fresnel);
    fx_set2f(prog, "uFieldScale", 1.0f / (float)g_gridW, 1.0f / (float)g_gridH);
    fx_set1f(prog, "uTime", g_fxTime);
    fx_set1f(prog, "uRange", WATER_FIELD_RANGE);
    fx_set1f(prog, "uAlpha", 170.0f / 255.0f);
    {
        /* The bearing is a compass heading like the wind's: 0 is north (up the map,
           -z), 90 is east (+x). */
        const float b = g_fx.water_current * (float)(M_PI / 180.0);
        fx_set2f(prog, "uCurrent", sinf(b), -cosf(b));
    }
    fx_set1f(prog, "uFlow", g_fx.water_flow);
    fx_set1f(prog, "uFlowRate", g_fx.water_flow_rate);
    fx_set1f(prog, "uPan", g_fx.water_pan);
    fx_set1f(prog, "uRiverSpeed", g_fx.water_river_speed);
    fx_set1f(prog, "uFoamWidth", g_fx.water_foam_width > 0.05f ? g_fx.water_foam_width : 0.05f);
    fx_set1f(prog, "uShallow", g_fx.water_shallow);
    fx_set1f(prog, "uDeep", g_fx.water_deep);
    fx_set1f(prog, "uFade", g_fx.water_fade);
    fx_set1f(prog, "uProductMean", g_waterGrainMean * g_waterMean2);
    fx_set1f(prog, "uNatural", g_fx.water_natural);
    fx_set1f(prog, "uDark", g_fx.water_dark);
    fx_set1f(prog, "uFadeWidth", g_fx.water_fade_width > 0.02f ? g_fx.water_fade_width : 0.02f);
    fx_set1i(prog, "uWave", 4);
    fx_set1i(prog, "uWaveOn", waveOn);
    fx_set2f(prog, "uWaveTexel", g_waveW > 0 ? 1.0f / (float)g_waveW : 0.0f, g_waveH > 0 ? 1.0f / (float)g_waveH : 0.0f);
    fx_set1f(prog, "uWaveBump", g_fx.water_wake_bump);
    fx_set1f(prog, "uWaveFoam", g_fx.water_wake);
   fx_set1f(prog, "uEdge", g_fx.water_edge);
    fx_set1f(prog, "uWaveLen", g_fx.water_wave_len > 0.05f ? g_fx.water_wave_len : 0.05f);
    fx_set1f(prog, "uWaveSpeed", g_fx.water_wave_speed);
    rain_water_uniforms(prog);                   /* the rain's rings and its sky (rain_mod.h) */
    /* THE BASE'S OWN SCALE: the UV law below lays it at its repeats a cell, and the
       warp and the slide are kept the same distance on the ground as the cartridge's. */
    const float rpc = water_base_repeats();
    const float warpRep = water_src_ra() ? WATER_WARP_REPEATS * (rpc / WATER_REPEATS_PER_CELL)
                                         : WATER_WARP_REPEATS;
    if (water_src_ra()) {
        fx_set1f(prog, "uBaseScale", rpc / WATER_REPEATS_PER_CELL);
        fx_set1f(prog, "uBaseGain", WATER_RA_BASE_GAIN);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);                       /* console: Z_CMP set, Z_UPD clear */

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < cells.size(); i++) {
        const PackCell& c = cells[i];
        if (!c.holes || !cell_shown(c.x, c.y) || !cell_in_view(c.x, c.y)) continue;
        const int cxs[6] = { c.x, c.x, c.x + 1, c.x + 1, c.x, c.x + 1 };
        const int czs[6] = { c.y, c.y + 1, c.y, c.y, c.y + 1, c.y + 1 };
        for (int k = 0; k < 6; k++) {
            const int cx = cxs[k], cz = czs[k];
            const float ang = phase + 256.0f * (float)(cx + cz);
            const float wu = warpRep * sinf(ang);
            const float wv = warpRep * cosf(ang);
            const float bu = rpc * (float)cx + wu;
            const float bv = rpc * (float)cz + wv;
            float light, vis, tint[3];
            water_terrain_corner(cx, cz, tinted, oneSun, &light, &vis, tint);
            /* The console's own scroll is NOT added: the pan along the current is the
               motion now, and the second layer sits still against it (parallax). */
            glMultiTexCoord2f(GL_TEXTURE0, bu, bv);
            glMultiTexCoord2f(GL_TEXTURE1, bu, bv);
            glMultiTexCoord3f(GL_TEXTURE2, tint[0], tint[1], tint[2]);
            /* RGB carries the shroud byte, ALPHA the terrain's light for the sand. */
            glColor4f(vis, vis, vis, light);
            glVertex3f((float)cx, terrain_corner_y(cx, cz), (float)cz);
        }
    }
    glEnd();

    fx_glUseProgram(0);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    return true;
}

/* Called by the frame right after draw_terrain. Draws nothing unless the water pass
   drew this frame, so the two can never disagree about which sea is on screen. */
static void water_shore_draw(void)
{
    if (!water_fx_wanted() || !g_waterFieldTex || g_waterCoastCell.empty()) return;
    const GLuint prog = shore_prog_get();
    if (!prog) return;
    if (g_waterFieldW != g_gridW * g_waterFieldRes) return;

    const bool oneSun = g_fxActive && g_fx.sun_lambert && g_fx.terrain_normals;
    const float bearing = g_fx.water_current * (float)(M_PI / 180.0);
    float panU = sinf(bearing) * g_fx.water_pan * g_fxTime;
    float panV = -cosf(bearing) * g_fx.water_pan * g_fxTime;
    if (water_src_ra()) {                    /* the slide at the base's own scale */
        const float k = water_base_repeats() / WATER_REPEATS_PER_CELL;
        panU *= k; panV *= k;
    }

    water_foam_boot();
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_waterFoamTex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_waterLandTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_waterFieldTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, water_base2());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, water_base1());

    fx_glUseProgram(prog);
    fx_set1i(prog, "uW1", 0);
    fx_set1i(prog, "uW2", 1);
    fx_set1i(prog, "uField", 2);
    fx_set1i(prog, "uLand", 3);
    fx_set1i(prog, "uFoamTex", 4);
    fx_set2f(prog, "uCurrent", sinf(bearing), -cosf(bearing));
    fx_set1f(prog, "uPan", g_fx.water_pan);
    fx_set1f(prog, "uFlow", g_fx.water_flow);
    fx_set1f(prog, "uFlowRate", g_fx.water_flow_rate);
    fx_set1f(prog, "uRiverSpeed", g_fx.water_river_speed);
    fx_set2f(prog, "uFieldScale", 1.0f / (float)g_gridW, 1.0f / (float)g_gridH);
    fx_set1f(prog, "uTime", g_fxTime);
    fx_set1f(prog, "uRange", WATER_FIELD_RANGE);
    fx_set1f(prog, "uAlpha", 170.0f / 255.0f);
    fx_set2f(prog, "uPanV", panU, panV);
    fx_set1f(prog, "uFoam", g_fx.water_foam);
    fx_set1f(prog, "uFoamWidth", g_fx.water_foam_width > 0.05f ? g_fx.water_foam_width : 0.05f);
    fx_set1f(prog, "uFoamCross", g_fx.water_foam_cross > 0.01f ? g_fx.water_foam_cross : 0.01f);
    fx_set1f(prog, "uWaveLen", g_fx.water_wave_len > 0.05f ? g_fx.water_wave_len : 0.05f);
    fx_set1f(prog, "uWaveSpeed", g_fx.water_wave_speed);
    fx_set1f(prog, "uEdge", g_fx.water_edge);
    fx_set1f(prog, "uFade", g_fx.water_fade);
    fx_set1f(prog, "uProductMean", g_waterGrainMean * g_waterMean2);
    fx_set1f(prog, "uNatural", g_fx.water_natural);
    fx_set1f(prog, "uDark", g_fx.water_dark);
    const bool ra = water_src_ra();
    if (ra) {
        fx_set1f(prog, "uBaseRepeat", water_base_repeats());
        fx_set1f(prog, "uBaseGain", WATER_RA_BASE_GAIN);
        fx_set1i(prog, "uSheet", 5);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, g_waterRa.sheet.gl);
        glActiveTexture(GL_TEXTURE0);
    }

    GLint depthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_ALPHA_TEST);

    const std::vector<PackCell>& cells = water_cells();
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < cells.size(); i++) {
        const PackCell& c = cells[i];
        if (c.x < 0 || c.y < 0 || c.x >= g_gridW || c.y >= g_gridH) continue;
        if (!g_waterCoastCell[(size_t)c.y * g_gridW + c.x]) continue;
        if (!cell_shown(c.x, c.y) || !cell_in_view(c.x, c.y)) continue;
        const int cxs[6] = { c.x, c.x, c.x + 1, c.x + 1, c.x, c.x + 1 };
        const int czs[6] = { c.y, c.y + 1, c.y, c.y, c.y + 1, c.y + 1 };
        for (int k = 0; k < 6; k++) {
            float light, vis, tint[3];
            water_terrain_corner(cxs[k], czs[k], false, oneSun, &light, &vis, tint);
            if (ra) glMultiTexCoord2f(GL_TEXTURE5, cxs[k] == c.x ? c.u0 : c.u1, czs[k] == c.y ? c.v0 : c.v1);
            glColor4f(vis, vis, vis, light);      /* rgb the shroud, a the terrain's light */
            glVertex3f((float)cxs[k], terrain_corner_y(cxs[k], czs[k]), (float)czs[k]);
        }
    }
    glEnd();

    fx_glUseProgram(0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthFunc((GLenum)depthFunc);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (ra) { glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, 0); }
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/* THE SOFT COAST. The terrain is alpha-cut art: a texel is sand or it is hole, and the
   cut between them is a stair-stepped line at texel resolution with the art's own dark
   rim on it. Every overlay above is field-resolution and smooth, so none of them could
   follow that line, and it stayed: the director's "harsh line from the coast / shore textures
   is still there and not fading out". So under ENHANCED the terrain pass draws the
   coast from THIS copy of its atlas instead, in which every holed tile's alpha is a
   ramp about the original cut (a signed distance to it, WATER_SOFT_RADIUS texels to
   each side), and draws it BLENDED with the test lowered to nearly nothing: the sand
   itself fades into the sea over a few texels, and the 0.5 line, which is where the
   ramp crosses half, is exactly where the cut was, so nothing moves. Interior texels
   are alpha 1 and the blend is a no-op there. The ramp is computed per TILE RECTANGLE
   (the cells' own UV rectangles, one pass per distinct one) and never across it, so a
   tile packed beside a water tile in the atlas cannot inherit a soft edge it does not
   own. Classic and Win98 never see this texture. */
#define WATER_SOFT_RADIUS 2.5f
static GLuint g_waterSoftTex = 0;
static int    g_waterSoftAtlas = -1;
static GLuint g_waterSoftSrc = 0;
static int    g_waterSoftLinear = -1;
/* AND WHICH MAP IT WAS BUILT FOR, which is the key this cache spent a while without.
   The three above cannot tell two missions apart. The atlas INDEX is the same slot in
   every pack the bakery makes, so it is 0 or 1 on every map of every theater. The source
   GL NAME is worse than useless: pack_free returns all of the old pack's texture names
   before load_pack takes new ones, the atlas is the first texture in the bank, and a
   driver that hands back the lowest free name gives the new atlas the SAME name the old
   one had. So on a second mission the guard matched, the sheet was not rebuilt, and
   draw_terrain drew the whole ground of the new map through the OLD map's tile packing:
   every cell addressing the right rectangle of the wrong sheet, and black wherever the
   new map's rectangle fell past the old sheet's used region onto cleared padding.
   The scenario settles it, and the used size is kept beside it because that is the part
   the addressing actually depends on: 832x624 on temperate, 832x806 on desert. The
   sibling cache in this file, water_field_sync, has keyed on the scenario and the map
   rectangle from the day it was written; this one is now the same shape. */
static char   g_waterSoftScen[20] = "";

/* ONE TILE'S RAMP, shared by Tiberian Dawn's soft sheet below and Red Alert's ground
   (ra3d_terrain.h), which builds its own sheet with the ramp in it. solid and hole are
   the tile's hard cut (bw x bh, 1 where it holds); the ramp's alpha goes to dst, one
   texel every `px` bytes, `row` bytes a row.
   CENTRED on the cut (Tiberian Dawn's): half on the sand and half in the hole, because
   the hole's own texels carry no picture worth showing. WATER-SIDE (Red Alert's): the
   land stays whole and the fade runs out over the hole, because there the hole's texels
   are the painted water, which fades into the live sea; centred, the outer half of every
   coast and every rock's own rim would turn translucent for no gain. */
static void water_soft_ramp(const unsigned char* solid, const unsigned char* hole, int bw, int bh,
                            float softR, unsigned char* dst, size_t row, int px,
                            bool waterSide = false)
{
    std::vector<float> dToHole((size_t)bw * bh), dToSolid((size_t)bw * bh);
    water_edt_2d(hole,  &dToHole[0],  NULL, bw, bh);
    water_edt_2d(solid, &dToSolid[0], NULL, bw, bh);
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            const size_t j = (size_t)y * bw + x;
            /* signed distance to the cut, in texels: + on sand, - in the hole. A
               texel beside the cut is at distance 1, so the ramp is centred on the
               half-texel line between the two rows, which is where the cut was. */
            const float sd = solid[j] ? dToHole[j] - 0.5f : -(dToSolid[j] - 0.5f);
            float a = waterSide ? 1.0f + sd / softR : 0.5f + sd / (2.0f * softR);
            if (a < 0.0f) a = 0.0f; else if (a > 1.0f) a = 1.0f;
            dst[(size_t)y * row + (size_t)x * px] = (unsigned char)(a * 255.0f + 0.5f);
        }
}
static int    g_waterSoftUW = -1;
static int    g_waterSoftUH = -1;

/* THE SHEETS BUILT FOR THE OTHER ATLASES, kept rather than thrown away. The graphics
   modes change which atlas the ground draws from (SPACE cycles them live), and without
   this every return to a mode rebuilt its sheet: about a tenth of the 0.6 s first frame
   measured on a coastal map. Two slots, keyed exactly as the live one is, and any slot
   built for another scenario is dropped at the next sync. */
struct WaterSoftSlot { GLuint tex; int atlas; GLuint src; int linear, uw, uh; char scen[20]; };
static WaterSoftSlot g_waterSoftSlot[2];

static void water_soft_slot_drop(WaterSoftSlot* s)
{
    if (s->tex) glDeleteTextures(1, &s->tex);
    memset(s, 0, sizeof *s);
}

static bool water_soft_sync_for(int ai, int linear)
{
    if (ai < 0 || ai >= (int)g_pack.tex.size()) return false;
    const PackTex& at = g_pack.tex[ai];
    if (g_waterSoftTex && g_waterSoftAtlas == ai && g_waterSoftSrc == at.gl
        && g_waterSoftLinear == linear
        && g_waterSoftUW == at.uw && g_waterSoftUH == at.uh
        && !strcmp(g_waterSoftScen, g_pack.scen))
        return true;
    for (int k = 0; k < 2; k++)
        if (g_waterSoftSlot[k].tex && strcmp(g_waterSoftSlot[k].scen, g_pack.scen))
            water_soft_slot_drop(&g_waterSoftSlot[k]);
    /* The live sheet, if it is this scenario's, goes into a slot instead of the bin. */
    if (g_waterSoftTex && !strcmp(g_waterSoftScen, g_pack.scen)) {
        WaterSoftSlot* s = !g_waterSoftSlot[0].tex ? &g_waterSoftSlot[0]
                         : !g_waterSoftSlot[1].tex ? &g_waterSoftSlot[1] : &g_waterSoftSlot[1];
        if (s->tex) water_soft_slot_drop(s);
        s->tex = g_waterSoftTex; s->atlas = g_waterSoftAtlas; s->src = g_waterSoftSrc;
        s->linear = g_waterSoftLinear; s->uw = g_waterSoftUW; s->uh = g_waterSoftUH;
        snprintf(s->scen, sizeof s->scen, "%s", g_waterSoftScen);
        g_waterSoftTex = 0;
    }
    for (int k = 0; k < 2; k++) {
        WaterSoftSlot* s = &g_waterSoftSlot[k];
        if (s->tex && s->atlas == ai && s->src == at.gl && s->linear == linear
            && s->uw == at.uw && s->uh == at.uh) {
            g_waterSoftTex = s->tex; g_waterSoftAtlas = s->atlas; g_waterSoftSrc = s->src;
            g_waterSoftLinear = s->linear; g_waterSoftUW = s->uw; g_waterSoftUH = s->uh;
            snprintf(g_waterSoftScen, sizeof g_waterSoftScen, "%s", s->scen);
            s->tex = 0;
            memset(s, 0, sizeof *s);
            return true;
        }
    }
    if (g_waterSoftTex) { glDeleteTextures(1, &g_waterSoftTex); g_waterSoftTex = 0; }
    if (at.w <= 0 || at.h <= 0 || (long)at.w * at.h > WATER_ATLAS_MAX_TEXELS || at.uw <= 0 || at.uh <= 0)
        return false;
    /* THE RAMP IS A WIDTH ON THE GROUND, not a count of texels. The radius is set for the
       1995 cell of 24 texels; the Enhanced sheet draws a cell from 144, where 2.5 texels
       is a sixth of the fade the other sheets show, and the water cut (the cartridge's
       24-a-cell one, enlarged bilinearly) showed its 24-grid corners through it. Scaled by the cell,
       the fade is as wide on the ground as everywhere else (a tile rectangle is one cell,
       so its width is the cell). The other sheets keep the radius as it was tuned. */
    const bool enhSheet = ai == g_pack.terrainTexEnh;
    float softR = WATER_SOFT_RADIUS;
    std::vector<unsigned char> atl((size_t)at.w * at.h * 4);
    glBindTexture(GL_TEXTURE_2D, at.gl);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &atl[0]);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* every distinct tile rectangle a holed cell draws from */
    std::vector<long long> seen;
    int tiles = 0;
    for (size_t i = 0; i < g_pack.cell.size(); i++) {
        const PackCell& c = g_pack.cell[i];
        if (!c.holes) continue;
        int x0 = (int)(c.u0 * (float)at.uw + 0.5f), x1 = (int)(c.u1 * (float)at.uw + 0.5f);
        int y0 = (int)(c.v0 * (float)at.uh + 0.5f), y1 = (int)(c.v1 * (float)at.uh + 0.5f);
        if (x0 > x1) { const int t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { const int t = y0; y0 = y1; y1 = t; }
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
        if (x1 > at.w) x1 = at.w; if (y1 > at.h) y1 = at.h;
        const int bw = x1 - x0, bh = y1 - y0;
        if (bw < 2 || bh < 2) continue;
        const long long key = ((long long)y0 << 40) | ((long long)x0 << 20) | ((long long)bh << 10) | bw;
        bool dup = false;
        for (size_t k = 0; k < seen.size(); k++) if (seen[k] == key) { dup = true; break; }
        if (dup) continue;
        seen.push_back(key);
        tiles++;
        std::vector<unsigned char> solid((size_t)bw * bh), hole((size_t)bw * bh);
        int nSolid = 0;
        for (int y = 0; y < bh; y++)
            for (int x = 0; x < bw; x++) {
                const unsigned char a = atl[((size_t)(y0 + y) * at.w + (x0 + x)) * 4 + 3];
                solid[(size_t)y * bw + x] = a >= 128 ? 1 : 0;
                hole[(size_t)y * bw + x]  = a >= 128 ? 0 : 1;
                nSolid += a >= 128 ? 1 : 0;
            }
        if (nSolid == 0 || nSolid == bw * bh) continue;          /* no cut in this tile */
        if (enhSheet && bw > 24) softR = WATER_SOFT_RADIUS * (float)bw / 24.0f;   /* a cell is bw texels */
        water_soft_ramp(&solid[0], &hole[0], bw, bh, softR,
                        &atl[((size_t)y0 * at.w + x0) * 4 + 3], (size_t)at.w * 4, 4);
    }
    glGenTextures(1, &g_waterSoftTex);
    glBindTexture(GL_TEXTURE_2D, g_waterSoftTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, at.w, at.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, &atl[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_waterSoftAtlas = ai; g_waterSoftSrc = at.gl; g_waterSoftLinear = linear;
    g_waterSoftUW = at.uw; g_waterSoftUH = at.uh;
    snprintf(g_waterSoftScen, sizeof g_waterSoftScen, "%s", g_pack.scen);
    fprintf(stderr, "WATER|softcoast|atlas=%d|tiles=%d|radius=%.1f|sheet=%dx%d\n", ai, tiles, (double)softR,
            at.w, at.h);
    return true;
}

static bool water_soft_atlas_sync(void)
{
    return water_soft_sync_for(terrain_atlas_index(), g_fx.bilinear ? 1 : 0);
}

/* BUILD ANOTHER ATLAS'S SHEET INTO A SLOT, leaving the live one as it is, so the first
   switch into a mode that draws from that atlas finds it ready (gmwipe_mod.h calls this
   while another mode is on screen). The build is the ordinary one; the live sheet is set
   aside for it and put back after. */
static void water_soft_prepare(int ai, int linear)
{
    WaterSoftSlot live;
    memset(&live, 0, sizeof live);
    live.tex = g_waterSoftTex; live.atlas = g_waterSoftAtlas; live.src = g_waterSoftSrc;
    live.linear = g_waterSoftLinear; live.uw = g_waterSoftUW; live.uh = g_waterSoftUH;
    snprintf(live.scen, sizeof live.scen, "%s", g_waterSoftScen);
    g_waterSoftTex = 0;
    if (water_soft_sync_for(ai, linear) && g_waterSoftTex) {
        WaterSoftSlot* s = !g_waterSoftSlot[0].tex ? &g_waterSoftSlot[0] : &g_waterSoftSlot[1];
        if (s->tex) water_soft_slot_drop(s);
        s->tex = g_waterSoftTex; s->atlas = g_waterSoftAtlas; s->src = g_waterSoftSrc;
        s->linear = g_waterSoftLinear; s->uw = g_waterSoftUW; s->uh = g_waterSoftUH;
        snprintf(s->scen, sizeof s->scen, "%s", g_waterSoftScen);
    }
    g_waterSoftTex = live.tex; g_waterSoftAtlas = live.atlas; g_waterSoftSrc = live.src;
    g_waterSoftLinear = live.linear; g_waterSoftUW = live.uw; g_waterSoftUH = live.uh;
    snprintf(g_waterSoftScen, sizeof g_waterSoftScen, "%s", live.scen);
}

/* The two the terrain pass asks. The texture to bind in place of its atlas (its own when
   the soft coast is not in play), and whether it should blend. */
static GLuint water_terrain_gl(GLuint def)
{
    /* Tiberian Dawn's terrain only: Red Alert's ground carries its soft coast in its own
       sheet (ra3d_terrain.h), and the scene pack's atlas drawn for its depth has no cut. */
    if (water_src_ra()) return def;
    if (!water_fx_wanted() || !water_soft_atlas_sync()) return def;
    return g_waterSoftTex;
}
static bool water_terrain_soft(void)
{
    if (water_src_ra()) return false;
    return water_fx_wanted() && g_waterSoftTex != 0 && g_waterSoftAtlas == terrain_atlas_index();
}

#endif /* OPENCNC3D_WATER_MOD_H */
