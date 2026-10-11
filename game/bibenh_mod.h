/* ================================================================================== *
 *  bibenh_mod.h -- the remastered building aprons, Enhanced only.
 *
 *  WHAT IT REPLACES. The cartridge draws the dirt apron in front of a bibbed building
 *  (BIB1 4x2, BIB2 3x2, BIB3 2x2 cells) from 24-texel frames with a 1-bit alpha key,
 *  alpha-blended over the ground and lit by the ground's own corner shade
 *  (smudge_mod.h). Under Enhanced, with the bib_enh dial on and bibenh.pack loaded,
 *  those three are drawn from new art instead:
 *
 *   - THE MS-DOS APRONS, one set per theater as the PC game had, redrawn at 50 percent
 *     more resolution (36 texels a cell; the pack says how many) with their own ragged
 *     outline and loose specks, so the apron keeps the shape it always had.
 *   - A SOFT EDGE: the outline carries a little partial alpha instead of a 1-bit key.
 *   - MULTIPLIED INTO THE GROUND: the ground already drawn under the apron is scaled by
 *     the art, so the grass or sand, its light and the shroud carry through the dirt
 *     instead of being painted over.
 *
 *  WHY A MULTIPLY ALONE IS NOT ENOUGH. The apron has to turn Enhanced's dark green grass
 *  brown: measured on the DOS terrain art Enhanced draws, the cartridge apron sits at
 *  3.98 x the red, 1.97 x the green and 2.03 x the blue of the ground it covers (K, per
 *  theater, in the pack). A multiply
 *  that does that to grass does it to everything under the apron: grey rock at the edge
 *  of a cliff comes out pink, and the reddish texels in the grass come out as red
 *  specks. So the apron is two passes, and bib_enh_cover (c) sets the split:
 *
 *   1. COVER. The painted dirt, alpha-blended at a * c and lit by the ground's shade,
 *      exactly as the cartridge draws its apron. On typical ground this alone would
 *      land at the old apron's colour.
 *   2. MULTIPLY. What cover left of the ground's own colour is corrected by
 *      H = K / ((1 - c) + c K), and the painting's detail rides on it at (1 - c):
 *
 *          M = (1 + (1 - c) (dirt / mean - 1)) * H
 *
 *  c = 0 is a pure multiply (every hue of the ground carries through); c = 1 is the
 *  painted dirt laid over the ground, as opaque as the fade. Whatever c is, over typical
 *  ground the apron comes out the old apron's colour.
 *
 *  THE MULTIPLY GOES UP TO 4 X. A plain multiply can only darken. Each apron cell is
 *  drawn TWICE with a doubled multiply,
 *
 *      dst' = dst * src + src * dst = dst * src * 2,  twice:  dst * (2 src)^2
 *
 *  i.e. glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR), so the texel is 0.5 + a (sqrt(M)/2 - 0.5):
 *  128 leaves the ground as it is, and the fade takes the factor to neutral.
 *
 *  NO GROUND SHADE ON THE MULTIPLY. Its destination is the lit ground, so it carries the
 *  light already; shading it as well would darken the apron twice.
 *
 *  THE DIALS MOVE THE TEXTURES, NOT THE DRAW. bib_enh_strength scales a in both passes
 *  and bib_enh_cover sets c; the textures are rebuilt on the CPU and re-uploaded when
 *  either moves, because a doubled multiply has no vertex-colour form of "less", and the
 *  texture combiners that would give one are not on the Voodoo 2.
 *
 *  FRAMES. The engine's frame number for an apron cell is w + h * Width
 *  (SmudgeClass::Mark), so a cell's rectangle in the apron's texture is column
 *  frame % Width, row frame / Width. Each apron is its own texture, padded up to a
 *  power of two with neutral texels, so nothing bleeds between aprons.
 *
 *  TIER 1. Everything here is fixed function: per theater, textures of 256x128, 128x128
 *  and 128x128 after padding, immediate-mode triangles drawn three times, an ordinary alpha blend
 *  and a DST_COLOR / SRC_COLOR blend, both of which the Voodoo 2's blender has. In
 *  16-bit colour each multiply pass rounds to 5 or 6 bits, which may band the fade; a
 *  single multiply pass capped at 2 x, with the cover dial raised, is the fallback if it
 *  does. Absent on Win98 today only because Enhanced is.
 *
 *  A missing pack is announced ONCE and the cartridge's aprons are drawn as before.
 *
 *  WHY THE MULTIPLY ALONE TURNED ROADS PINK, and what Tier 2 draws instead. K is one
 *  colour ratio per theater, right for one ground colour only: the one it was
 *  calibrated on. The temperate K lifts red 3.3 x and green and blue about 1.9 x,
 *  which takes dark green grass to brown, but takes anything that is already brown or
 *  grey (a dirt road, bare earth, rock, the shore) to salmon pink, because that ground
 *  has no excess green for the red gain to cancel. Measured on the road in front of
 *  SCG15EA's Temple of Nod: ground (56, 51, 42) came out (111, 81, 72).
 *
 *  The cure has to know the ground under each pixel, which a blend cannot: so the
 *  Tier 2 path draws every apron cell ONCE through a shader that reads the ground
 *  already drawn there (a copy of the scene under the aprons' screen rectangle) and
 *  splits it into two parts. With L the ground's luminance and G the calibration
 *  ground's colour scaled to luminance 1 (in the pack, per theater):
 *
 *      gc = G * L                  the ground as the calibration colour, same brightness
 *      oc = cover(gc) * M          what the passes above make of THAT, exactly
 *      o  = oc + (1 - a c) (1 - f) Ms (g - gc)
 *
 *  gc goes through the cover and the multiply exactly as before, so on the ground K
 *  was calibrated on (g = gc) the apron is what it always was. The rest of the ground,
 *  its own colour away from the calibration colour (it has no luminance of its own),
 *  is carried through the multiply as a plain brightness gain Ms (the multiply's
 *  luminance gain on gc) instead of per channel, and only the share f of it the apron
 *  does NOT recolour: f is how far the apron takes gc toward the dirt's own colour.
 *  Where the multiply is neutral (the fade's edge, the cover dial at 1) f is 0 and
 *  the result is the old one; at an apron's core on a road the road's own colour is
 *  mostly replaced by the apron's, as it is on grass, and nothing turns pink.
 *
 *  TIER 1 has no shader and no copy of the scene, so it keeps the three passes above,
 *  and with them the pink cast on any ground that is not the calibration colour.
 * ================================================================================== */

#ifndef OPENCNC3D_BIBENH_MOD_H
#define OPENCNC3D_BIBENH_MOD_H

struct BibEnhApron {
    int      wcells, hcells;
    int      w, h;                      /* texels as baked */
    int      texw, texh;                /* padded to a power of two */
    float    mean[3];                   /* the dirt's mean over the fade, 0..1 */
    std::vector<unsigned char> rgba;    /* as baked: dirt RGB, fade A */
    GLuint   cover;                     /* the cover pass's texture */
    GLuint   mult;                      /* the multiply pass's texture */
};
struct BibEnhTheater {                  /* each theater has its own art, as on DOS */
    unsigned    theaterId;
    float       k[3];
    float       ground[3];              /* the ground K was calibrated on, luminance 1 */
    int         hasGround;              /* 0 for a version 3 pack, which has none */
    BibEnhApron ap[3];
};

static std::vector<BibEnhTheater> g_beTh;
static int   g_beCell = 36;             /* texels per cell, read from the pack */
static bool  g_beHave = false;
static bool  g_beOn   = true;           /* --nobibenh is the A/B */
static bool  g_beHue  = true;           /* --nobibhue / `bibhue 0`: the three passes only */
static float g_beStrBuilt = -1.0f, g_beCoverBuilt = -1.0f;

static int bibenh_pot(int v) { int p = 1; while (p < v) p <<= 1; return p; }

static GLuint bibenh_newtex(void)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, fx_filter_mode(GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, fx_filter_mode(GL_NEAREST));
    fx_filter_note(t, GL_NEAREST);      /* the F5 bilinear toggle reaches it */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static void bibenh_free(void)
{
    for (size_t i = 0; i < g_beTh.size(); i++)
        for (int b = 0; b < 3; b++) {
            if (g_beTh[i].ap[b].cover) glDeleteTextures(1, &g_beTh[i].ap[b].cover);
            if (g_beTh[i].ap[b].mult)  glDeleteTextures(1, &g_beTh[i].ap[b].mult);
        }
    g_beTh.clear();
    g_beHave = false;
    g_beStrBuilt = g_beCoverBuilt = -1.0f;
}

static bool bibenh_read_apron(FILE* f, const char* path, const char* nm, int b, BibEnhApron& A)
{
    unsigned wc = 0, hc = 0;
    if (fread(&wc, 4, 1, f) != 1 || fread(&hc, 4, 1, f) != 1
        || wc < 1 || wc > 4 || hc < 1 || hc > 4) {
        fprintf(stderr, "bibenh: %s %s apron %d header unreadable\n", path, nm, b + 1);
        return false;
    }
    A.wcells = (int)wc; A.hcells = (int)hc;
    A.w = A.wcells * g_beCell; A.h = A.hcells * g_beCell;
    A.texw = bibenh_pot(A.w); A.texh = bibenh_pot(A.h);
    A.rgba.resize((size_t)A.w * A.h * 4);
    if (fread(&A.rgba[0], 1, A.rgba.size(), f) != A.rgba.size()) {
        fprintf(stderr, "bibenh: %s %s apron %d truncated\n", path, nm, b + 1);
        return false;
    }
    double s[3] = {0, 0, 0}, sa = 0;
    for (size_t k = 0; k < A.rgba.size(); k += 4) {
        const double a = A.rgba[k + 3] / 255.0;
        for (int c = 0; c < 3; c++) s[c] += a * A.rgba[k + c] / 255.0;
        sa += a;
    }
    for (int c = 0; c < 3; c++)
        A.mean[c] = (sa > 0 && s[c] > 0) ? (float)(s[c] / sa) : 0.5f;
    A.cover = bibenh_newtex();
    A.mult = bibenh_newtex();
    return true;
}

static bool bibenh_load(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "bibenh: no pack at %s; Enhanced draws the cartridge's "
                        "building aprons\n", path);
        return false;
    }
    /* Version 4 adds the calibration ground after K. A version 3 pack (an older play
       folder) still loads; its aprons are drawn by the three passes alone. */
    char magic[8];
    unsigned ver = 0, nth = 0, cell = 0;
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "BIBENH", 6) != 0
        || (magic[6] != '3' && magic[6] != '4')
        || fread(&ver, 4, 1, f) != 1 || ver != (unsigned)(magic[6] - '0')
        || fread(&nth, 4, 1, f) != 1 || fread(&cell, 4, 1, f) != 1
        || nth < 1 || nth > 8 || cell < 8 || cell > 256) {
        fprintf(stderr, "bibenh: %s is not a readable apron pack\n", path);
        fclose(f);
        return false;
    }
    g_beCell = (int)cell;
    for (unsigned t = 0; t < nth; t++) {
        g_beTh.push_back(BibEnhTheater());
        BibEnhTheater& T = g_beTh.back();
        char nm[13]; memset(nm, 0, sizeof nm);
        bool ok = fread(nm, 1, 12, f) == 12 && fread(&T.theaterId, 4, 1, f) == 1
               && fread(T.k, 4, 3, f) == 3;
        T.hasGround = 0;
        if (ok && ver >= 4) {
            ok = fread(T.ground, 4, 3, f) == 3;
            T.hasGround = ok && T.ground[0] > 0.0f && T.ground[1] > 0.0f
                       && T.ground[2] > 0.0f;
            if (T.hasGround) {          /* the shader wants its colour at luminance 1 */
                const float l = 0.299f * T.ground[0] + 0.587f * T.ground[1]
                              + 0.114f * T.ground[2];
                for (int c = 0; c < 3; c++) T.ground[c] /= l;
            }
        }
        if (!ok)
            fprintf(stderr, "bibenh: %s theater %u unreadable\n", path, t);
        for (int b = 0; ok && b < 3; b++)
            ok = bibenh_read_apron(f, path, nm, b, T.ap[b]);
        if (!ok) {
            fclose(f);
            bibenh_free();
            return false;
        }
        if (T.hasGround)
            fprintf(stderr, "bibenh: %s, multiply %.2f %.2f %.2f, calibrated on %.3f %.3f %.3f\n",
                    nm, T.k[0], T.k[1], T.k[2], T.ground[0], T.ground[1], T.ground[2]);
        else
            fprintf(stderr, "bibenh: %s, multiply %.2f %.2f %.2f, no calibration ground "
                            "(version %u pack): the multiply alone, which tints ground "
                            "that is not that colour\n", nm, T.k[0], T.k[1], T.k[2], ver);
    }
    fclose(f);
    fprintf(stderr, "bibenh: three aprons a theater at %d texels a cell\n", g_beCell);
    g_beHave = !g_beTh.empty();
    return g_beHave;
}

/* Build both passes' textures for the dials as they stand, when either has moved. */
static void bibenh_build(float str, float cover)
{
    if (str < 0.0f) str = 0.0f;
    if (str > 1.0f) str = 1.0f;
    if (cover < 0.0f) cover = 0.0f;
    if (cover > 1.0f) cover = 1.0f;
    if (str == g_beStrBuilt && cover == g_beCoverBuilt)
        return;
    g_beStrBuilt = str; g_beCoverBuilt = cover;
    std::vector<unsigned char> px;
    for (size_t t = 0; t < g_beTh.size(); t++) {
        const BibEnhTheater& T = g_beTh[t];
        float h[3];
        for (int c = 0; c < 3; c++)
            h[c] = T.k[c] / ((1.0f - cover) + cover * T.k[c]);
        for (int b = 0; b < 3; b++) {
            const BibEnhApron& A = T.ap[b];
            const size_t n = (size_t)A.texw * A.texh * 4;

            /* COVER: the dirt at a * c * strength; the padding is (0,0,0,0). */
            px.assign(n, 0);
            for (int y = 0; y < A.h; y++)
                for (int x = 0; x < A.w; x++) {
                    const unsigned char* s = &A.rgba[((size_t)y * A.w + x) * 4];
                    unsigned char* d = &px[((size_t)y * A.texw + x) * 4];
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                    d[3] = (unsigned char)(s[3] * cover * str + 0.5f);
                }
            glBindTexture(GL_TEXTURE_2D, A.cover);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, A.texw, A.texh, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);

            /* MULTIPLY: 0.5 + a s (sqrt(M)/2 - 0.5); the padding is 128. */
            px.assign(n, 128);
            for (int y = 0; y < A.h; y++)
                for (int x = 0; x < A.w; x++) {
                    const unsigned char* s = &A.rgba[((size_t)y * A.w + x) * 4];
                    unsigned char* d = &px[((size_t)y * A.texw + x) * 4];
                    const float a = s[3] / 255.0f * str;
                    for (int c = 0; c < 3; c++) {
                        const float det = 1.0f + (1.0f - cover) * (s[c] / 255.0f / A.mean[c] - 1.0f);
                        float m = det * h[c];
                        if (m < 0.0f) m = 0.0f;
                        if (m > 4.0f) m = 4.0f;
                        const float v = 0.5f + a * (sqrtf(m) * 0.5f - 0.5f);
                        d[c] = (unsigned char)(v * 255.0f + 0.5f);
                    }
                    d[3] = s[3];
                }
            glBindTexture(GL_TEXTURE_2D, A.mult);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, A.texw, A.texh, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
        }
    }
}

static const BibEnhTheater* bibenh_set_for(int theater)
{
    for (size_t i = 0; i < g_beTh.size(); i++)
        if ((int)g_beTh[i].theaterId == theater)
            return &g_beTh[i];
    return NULL;
}

/* ---- TIER 2: one pass that reads the ground (see "WHY THE MULTIPLY ALONE" above) ----
   The cover and multiply textures are the ones bibenh_build makes for the three passes,
   so the dials shape both paths the same way. The vertex colour is the ground's corner
   shade and shroud, which only the cover's dirt takes, as in the cover pass. ftransform
   keeps the cell on exactly the vertices the fixed pipeline would give it, so the
   polygon offset resolves against the ground as it does for the passes. */
static const char* BIBENH_VS =
    "#version 120\n"
    "varying vec2 uv;\n"
    "varying vec3 shade;\n"
    "void main() {\n"
    "    uv = gl_MultiTexCoord0.xy;\n"
    "    shade = gl_Color.rgb;\n"
    "    gl_Position = ftransform();\n"
    "}\n";

static const char* BIBENH_FS =
    "#version 120\n"
    "uniform sampler2D uCover;\n"   /* rgb the dirt, a = alpha * cover * strength */
    "uniform sampler2D uMult;\n"    /* rgb the doubled-multiply texel t, a the art's alpha */
    "uniform sampler2D uScene;\n"   /* the ground, as drawn before the aprons */
    "uniform vec4 uVp;\n"           /* the viewport the copy was taken in */
    "uniform vec3 uGround;\n"       /* the calibration ground, luminance 1 */
    "uniform vec3 uDirt;\n"         /* the dirt's own mean colour, luminance 1 */
    "varying vec2 uv;\n"
    "varying vec3 shade;\n"
    "const vec3 W = vec3(0.299, 0.587, 0.114);\n"
    "void main() {\n"
    "    vec4 mt = texture2D(uMult, uv);\n"
    "    if (mt.a <= 0.0) discard;\n"           /* outside the outline: the ground as it is */
    "    vec4 cv = texture2D(uCover, uv);\n"
    "    vec3 g = texture2D(uScene, (gl_FragCoord.xy - uVp.xy) / uVp.zw).rgb;\n"
    "    vec3 m = 2.0 * mt.rgb;\n"
    "    m = m * m;\n"                          /* the two doubled multiplies: (2t)^2 */
    "    float L = dot(g, W);\n"
    "    vec3 gc = uGround * L;\n"
    "    vec3 g1c = mix(gc, cv.rgb * shade, cv.a);\n"
    "    vec3 oc = g1c * m;\n"
    "    float L1 = max(dot(g1c, W), 1e-5), Lo = max(dot(oc, W), 1e-5);\n"
    "    vec3 h1 = g1c / L1, ho = oc / Lo, v = uDirt - h1;\n"
    "    float f = clamp(dot(ho - h1, v) / max(dot(v, v), 1e-6), 0.0, 1.0);\n"
    "    vec3 o = oc + (1.0 - cv.a) * (1.0 - f) * (Lo / L1) * (g - gc);\n"
    "    gl_FragColor = vec4(max(o, 0.0), 1.0);\n"
    "}\n";

static GLuint g_beProg = 0;
static int    g_beProgTried = 0;

static GLuint bibenh_prog(void)
{
    if (g_beProg || g_beProgTried) return g_beProg;
    g_beProgTried = 1;
    if (!fx_gl_ready) return 0;
    GLuint vs = fx_compile(GL_VERTEX_SHADER, BIBENH_VS, "bibenh");
    if (!vs) return 0;
    GLuint fs = fx_compile(GL_FRAGMENT_SHADER, BIBENH_FS, "bibenh");
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
        fprintf(stderr, "FX|program|bibenh FAILED TO LINK\n%s\n", log);
        fx_glDeleteProgram(p);
        p = 0;
    }
    fx_glDeleteShader(vs);
    fx_glDeleteShader(fs);
    if (!p)
        fprintf(stderr, "bibenh: no shader, so the aprons are the three passes, which "
                        "tint ground that is not the calibration colour\n");
    g_beProg = p;
    return p;
}

/* THE GROUND UNDER THE APRONS, copied out of the frame just before they draw. The
   texture is the viewport's size and only the aprons' rectangle is copied into it, at
   its own place, so the shader reads it at the fragment's own window position. Float
   when the scene target is (the post chain's high range), so nothing is clipped. */
static GLuint g_beScene = 0;
static int    g_beSceneW = 0, g_beSceneH = 0, g_beSceneHdr = -1;

static void bibenh_grab(const GLint vp[4], const int r[4], int hdr)
{
    if (!g_beScene) glGenTextures(1, &g_beScene);
    glBindTexture(GL_TEXTURE_2D, g_beScene);
    if (vp[2] != g_beSceneW || vp[3] != g_beSceneH || hdr != g_beSceneHdr) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (hdr)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vp[2], vp[3], 0, GL_RGBA, GL_FLOAT, NULL);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vp[2], vp[3], 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, NULL);
        g_beSceneW = vp[2]; g_beSceneH = vp[3]; g_beSceneHdr = hdr;
    }
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, r[0] - vp[0], r[1] - vp[1], r[0], r[1], r[2], r[3]);
}

/* WHAT THE LAST APRON DRAW DID, for `bibdump`: which path, how many cells, where. And
   on request (`bibdump arm`) a read-back of the aprons' rectangle before and after the
   draw, measured over the pixels the apron changed: the ground there, the result, and
   how many came out in the PINK BAND. The band is HSV hue below 22 or from 330 degrees,
   saturation at least 0.12, value at least 0.10: the salmon the road went to sits at
   hue 13 to 17, the approved apron on grass at 40 to 60, the dirt painting at 26.
   gate_bibhue.py uses the same band on the screen shot. */
struct BibEnhStat {
    int   path;              /* 0 nothing drawn, 1 the three passes, 2 the shader */
    int   cells, theater, hue;
    int   rect[4];
    int   armed, probed, px, pink;
    double ground[3], out[3];
};
static BibEnhStat g_beStat;

static int bibenh_pink(double r, double g, double b)
{
    const double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    if (mx < 0.10 || mx <= 0.0 || (mx - mn) / mx < 0.12) return 0;
    double h;
    if (mx == r)      h = 60.0 * fmod((g - b) / (mx - mn), 6.0);
    else if (mx == g) h = 60.0 * ((b - r) / (mx - mn) + 2.0);
    else              h = 60.0 * ((r - g) / (mx - mn) + 4.0);
    if (h < 0.0) h += 360.0;
    return (h < 22.0 || h >= 330.0) ? 1 : 0;
}

#endif /* OPENCNC3D_BIBENH_MOD_H */
