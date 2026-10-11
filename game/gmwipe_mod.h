/* ====================================================================================
 *  gmwipe_mod.h -- SPACE cycles the graphics mode, live, behind an energy line.
 *
 *  WHAT A PLAYER SEES. SPACE takes the game to the next graphics mode (Classic, then
 *  N64, then Enhanced, then Classic again; where N64 is not offered, as in Red Alert,
 *  Classic and Enhanced in turn). A bright line of energy sweeps
 *  across the screen from left to right, easing in and out; everything left of it is
 *  already the new mode and everything right of it is still the old one. The new mode's
 *  name comes up large at the top of the screen, holds, and fades.
 *
 *  HOW. The old mode's last finished frame is copied into a texture (a snapshot), the
 *  mode is switched through the same apply the Visuals screen uses (so the choice is
 *  saved like any other), and while the line travels each new frame is drawn normally
 *  with the snapshot laid back over the part of the screen the line has not reached.
 *  Nothing is rendered twice: the right half is a still picture for the half second the
 *  line takes, which is what makes the switch free of cost while it happens.
 *
 *  WHERE IN THE FRAME. After fx_present, when the finished picture (post chain, sidebar
 *  and all) is on the real framebuffer, and before the pointer: while a snapshot is due
 *  or on screen the pointer is drawn after this, as it is while the F5 panel is up, so
 *  it is never frozen into the snapshot and never covered by it.
 *
 *  THE CLOCK. Presentation only, so it runs on the wall clock, but a frame counts for at
 *  most a thirtieth of a second: the first frame of a newly chosen mode can be slow (the
 *  bushes, rocks and grass build their batches on first sight), and a line that jumped
 *  forward by that frame's length would read as a stutter. A slow first frame therefore
 *  holds the line still for a moment instead. A scripted run can stand the clock still
 *  and place it by hand (`gfxwipe`), which is how a gate photographs the middle of it.
 *
 *  THE TITLE'S LETTERS use the original C&C GRAD6FNT from the DOS UI pack, with whole
 *  pixel enlargement and nearest filtering. Green lettering has a crisp dark drop shadow.
 *  No font data is added to the build.
 *
 *  TIER 1: NONE, TIER 2 ONLY. The draw is fixed function (a copied texture, quads, lines
 *  and additive blending), but the thing it switches between is not: Windows 98 has the
 *  Classic picture only, so there is nothing for SPACE to cycle there. Listed in
 *  docs/tier1-gap.md.
 * ==================================================================================== */
#ifndef OPENCNC3D_GMWIPE_MOD_H
#define OPENCNC3D_GMWIPE_MOD_H

#include "../audio/wavio.h"

#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

static const float GMW_WIPE_S     = 0.55f;  /* the line's crossing, eased in and out   */
static const float GMW_TITLE_IN   = 0.15f;  /* the title's fade in                     */
static const float GMW_TITLE_HOLD = 3.00f;  /* how long it stands before fading        */
static const float GMW_TITLE_FADE = 0.60f;  /* its fade out                            */

enum { GMW_IDLE = 0, GMW_CAPTURE = 1, GMW_WIPE = 2 };

static int    g_gmwState   = GMW_IDLE;
static int    g_gmwQueued  = 0;       /* a press during a wipe, taken when it ends      */
static int    g_gmwTarget  = 0;       /* the mode being switched to                     */
static int    g_gmwFrom    = 0;       /* and the one it came from                       */
static float  g_gmwT       = 0.0f;    /* seconds since the line set off                 */
static float  g_gmwTitleT  = -1.0f;   /* seconds since the title came up; <0 none       */
static int    g_gmwTitleMode = 0;
static Uint32 g_gmwLastMs  = 0;
static int    g_gmwFrozen  = 0;       /* a script stands the clock still               */
static unsigned g_gmwSeed  = 1;       /* one per wipe, so no two look alike             */
static Uint64 g_gmwSwitchAt = 0;
/* THE HIDDEN WARM-UP (gmw_warm): a mode about to be drawn for one frame under the
   snapshot, and the mode to come back to afterwards. */
static int    g_gmwWarmDue  = 0;      /* capture this frame and switch to g_gmwWarmMode   */
static int    g_gmwWarmMode = 0;
static int    g_gmwWarmBack = 0;
static unsigned g_gmwWarmed = 0;      /* bit per mode drawn at least once this mission    */
static char   g_gmwWarmScen[20] = "";      /* when the switch was made, to time the next frame */

static GLuint g_gmwSnap = 0;
static int    g_gmwSnapW = 0, g_gmwSnapH = 0;

/* ---- the colours: one per mode, the line takes the colour of the mode arriving ------ */
static void gmw_mode_rgb(int mode, float* r, float* g, float* b)
{
    if (mode == FX_MODE_CLASSIC)    { *r = 0.36f; *g = 1.00f; *b = 0.40f; }  /* the dialog's green */
    else if (mode == FX_MODE_N64) { *r = 0.30f; *g = 0.82f; *b = 1.00f; }  /* electric cyan      */
    else                            { *r = 1.00f; *g = 0.74f; *b = 0.26f; }  /* amber              */
}

static const char* gmw_mode_title(int mode)
{
    return mode == FX_MODE_CLASSIC ? "CLASSIC" : mode == FX_MODE_N64 ? "N64"
                                                                       : "ENHANCED";
}

/* A small deterministic hash, so the arcs and sparks are the same for the same wipe and
   the same instant: two scripted shots of one moment are one picture. */
static float gmw_rand(unsigned a, unsigned b)
{
    unsigned h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ g_gmwSeed * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return (float)(h & 0xFFFFFF) / 16777215.0f;
}

static float gmw_smooth(float e0, float e1, float x)
{
    float t = (x - e0) / (e1 - e0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

static float gmw_ease(float p)       /* cubic, in and out */
{
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    return p < 0.5f ? 4.0f * p * p * p : 1.0f - powf(-2.0f * p + 2.0f, 3.0f) * 0.5f;
}

/* ================================================================================== *
 *  THE TITLE'S LETTERS
 * ================================================================================== */
enum { GMW_UP = 8, GMW_PAD = 1 };          /* growth factor and transparent atlas gutter */
static GLuint g_gmwFontTex = 0;
static int    g_gmwFontW = 0, g_gmwFontH = 0;
static int    g_gmwFontTried = 0;
static int    g_gmwCapH = 6;
struct GmwGlyph { int x, y, w, h; float xo, yo, adv; int have; };
static GmwGlyph g_gmwGlyph[96];

static GLuint gmw_upload_alpha(const unsigned char* px, int w, int h)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, px);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

/* Only the characters a title can use: capitals, digits, the hyphen and the space. */
static int gmw_title_char(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == ' ';
}

static void gmw_font_build(void)
{
    if (g_gmwFontTried) return;
    g_gmwFontTried = 1;
    const DB_Font* f = g_dbPack ? db_font(g_dbPack, "GRAD6FNT") : NULL;
    if (!f) {
        fprintf(stderr, "GFXWIPE|title unavailable: DOS pack has no GRAD6FNT\n");
        return;
    }
    /* Measure the capitals in the shipped glyph cells, excluding their shadow. */
    int capTop = f->maxh, capBottom = -1;
    for (int c = 'A'; c <= 'Z' && c < f->nchars; c++)
        for (int j = 0; j < f->maxh; j++)
            for (int i = 0; i < f->width[c] && i < f->maxw; i++)
                if ((f->cells[((size_t)c * f->maxh + j) * f->maxw + i] == 1 ||
                    f->cells[((size_t)c * f->maxh + j) * f->maxw + i] >= 4)) {
                    if (j < capTop) capTop = j;
                    if (j > capBottom) capBottom = j;
                }
    if (capBottom < capTop) return;
    g_gmwCapH = capBottom - capTop + 1;
    const int AW = 2048;
    int x = 0, y = 0, shelf = 0;
    memset(g_gmwGlyph, 0, sizeof g_gmwGlyph);
    /* Pack first, to know the height. */
    for (int c = 32; c < 127; c++) {
        if (!gmw_title_char(c)) continue;
        if (c >= f->nchars) continue;
        GmwGlyph* o = &g_gmwGlyph[c - 32];
        o->adv = (f->width[c] + DB_FONT6_XSPACING) * GMW_UP;
        o->xo = -GMW_PAD;
        o->yo = -capTop * GMW_UP - GMW_PAD;
        if (!f->width[c] || c == ' ') continue;
        const int w = f->width[c] * GMW_UP + 2 * GMW_PAD, h = f->maxh * GMW_UP + 2 * GMW_PAD;
        if (x + w > AW) { x = 0; y += shelf; shelf = 0; }
        o->x = x; o->y = y; o->w = w; o->h = h; o->have = 1;
        x += w;
        if (h > shelf) shelf = h;
    }
    int AH = 1;
    while (AH < y + shelf) AH <<= 1;
    std::vector<unsigned char> sharp((size_t)AW * AH, 0);
    /* Preserve the original pixel silhouettes, including the forked C&C lettering. */
    for (int c = 32; c < 127; c++) {
        const GmwGlyph* o = &g_gmwGlyph[c - 32];
        if (!o->have) continue;
        for (int j = 0; j < f->maxh * GMW_UP; j++)
            for (int i = 0; i < f->width[c] * GMW_UP; i++) {
                const size_t cell = ((size_t)c * f->maxh + j / GMW_UP) * f->maxw + i / GMW_UP;
                sharp[(size_t)(o->y + GMW_PAD + j) * AW + o->x + GMW_PAD + i] =
                    (f->cells[cell] == 1 || f->cells[cell] >= 4) ? 255 : 0;
            }
    }
    g_gmwFontW = AW; g_gmwFontH = AH;
    g_gmwFontTex = gmw_upload_alpha(sharp.data(), AW, AH);
    fprintf(stderr, "GFXWIPE|title font %dx%d from C&C GRAD6FNT, cap height %d\n",
            AW, AH, g_gmwCapH);
}

/* Width of a string at `scale` (1 = the grown face), with `track` extra between letters. */
static float gmw_text_w(const char* s, float scale, float track)
{
    float w = 0.0f;
    int n = 0;
    for (const char* p = s; *p; p++) {
        const int c = (unsigned char)*p;
        if (c < 32 || c > 126) continue;
        w += g_gmwGlyph[c - 32].adv * scale;
        n++;
    }
    return w + (n > 1 ? (n - 1) * track : 0.0f);
}

/* One pass of a string with one texture; the caller sets the blend and the colour. */
static void gmw_text_pass(GLuint tex, const char* s, float x, float top, float scale,
                          float track)
{
    const float us = 1.0f / (float)g_gmwFontW, vs = 1.0f / (float)g_gmwFontH;
    const float lineTop = floorf(top + 0.5f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    for (const char* p = s; *p; p++) {
        const int c = (unsigned char)*p;
        if (c < 32 || c > 126) continue;
        const GmwGlyph* g = &g_gmwGlyph[c - 32];
        if (g->have) {
            const float gx = x + g->xo * scale, gy = lineTop + g->yo * scale;
            const float gw = g->w * scale, gh = g->h * scale;
            const float u0 = g->x * us, v0 = g->y * vs;
            const float u1 = (g->x + g->w) * us, v1 = (g->y + g->h) * vs;
            glTexCoord2f(u0, v0); glVertex2f(gx, gy);
            glTexCoord2f(u1, v0); glVertex2f(gx + gw, gy);
            glTexCoord2f(u1, v1); glVertex2f(gx + gw, gy + gh);
            glTexCoord2f(u0, v1); glVertex2f(gx, gy + gh);
        }
        x += g->adv * scale + track;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

/* A string with a dark drop shadow, centred on cx, capitals starting at `top`, `caps`
   pixels tall. */
static void gmw_text(const char* s, float cx, float top, float caps, float trackFrac,
                     float r, float g, float b, float a)
{
    const float scale = fmaxf(1.0f, floorf(caps / g_gmwCapH + 0.5f)) / GMW_UP;
    const float track = floorf(caps * trackFrac + 0.5f);
    const float x = floorf(cx - gmw_text_w(s, scale, track) * 0.5f + 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const int rim = (int)fmaxf(1.0f, floorf(caps * 0.06f + 0.5f));
    const int shadow = (int)fmaxf(2.0f, floorf(caps * 0.12f + 0.5f));
    glColor4f(0.0f, 0.0f, 0.0f, a);
    /* A solid surround keeps the ink readable on terrain; fill the longer drop shadow
       continuously so it stays attached to each letter. */
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy)
                gmw_text_pass(g_gmwFontTex, s, x + dx * rim, top + dy * rim, scale, track);
    for (int d = 1; d <= shadow; d++)
        gmw_text_pass(g_gmwFontTex, s, x + d, top + d, scale, track);
    glColor4f(r, g, b, a);
    gmw_text_pass(g_gmwFontTex, s, x, top, scale, track);
}

/* ================================================================================== *
 *  THE LINE
 * ================================================================================== */

/* A vertical band of light centred on x, `half` wide either side, falling off roughly as
   a gaussian (four steps), its brightness breathing along its length. */
static void gmw_band(float x, float half, float h, float r, float g, float b, float peak,
                     float t, float flow)
{
    static const float off[4] = { 0.0f, 0.22f, 0.52f, 1.0f };
    static const float amp[4] = { 1.0f, 0.62f, 0.22f, 0.0f };
    const int SEG = 40;
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 3; k++) {
            glBegin(GL_QUAD_STRIP);
            for (int s = 0; s <= SEG; s++) {
                const float y = h * (float)s / SEG;
                const float yn = (float)s / SEG;
                const float pulse = 1.0f - flow + flow * (0.5f + 0.5f * sinf(yn * 23.0f - t * 19.0f)
                                                               * sinf(yn * 7.0f + t * 11.0f));
                glColor4f(r, g, b, peak * amp[k] * pulse);
                glVertex2f(x + side * off[k] * half, y);
                glColor4f(r, g, b, peak * amp[k + 1] * pulse);
                glVertex2f(x + side * off[k + 1] * half, y);
            }
            glEnd();
        }
}

static void gmw_draw_line(float x, int fbw, int fbh, float p)
{
    float r, g, b;
    gmw_mode_rgb(g_gmwTarget, &r, &g, &b);
    const float H = (float)fbh, u = (float)fbh / 800.0f;
    const float t = g_gmwT;
    /* Brightest in the middle of the crossing, never dark at either end. */
    const float swell = 0.75f + 0.25f * sinf(p * 3.14159f);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glShadeModel(GL_SMOOTH);

    /* The old picture darkens where the line is about to burn through it. */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.0f, 0.0f, 0.55f); glVertex2f(x, 0); glVertex2f(x, H);
    glColor4f(0.0f, 0.0f, 0.0f, 0.0f);  glVertex2f(x + 70.0f * u, H); glVertex2f(x + 70.0f * u, 0);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    /* A wash over the freshly revealed picture that the line leaves behind it. */
    glBegin(GL_QUADS);
    glColor4f(r, g, b, 0.0f);          glVertex2f(x - 260.0f * u, 0); glVertex2f(x - 260.0f * u, H);
    glColor4f(r, g, b, 0.16f * swell); glVertex2f(x, H); glVertex2f(x, 0);
    glEnd();
    gmw_band(x, 90.0f * u, H, r, g, b, 0.32f * swell, t, 0.35f);          /* the halo   */
    gmw_band(x, 22.0f * u, H, r * 0.7f + 0.3f, g * 0.7f + 0.3f, b * 0.7f + 0.3f,
             0.85f * swell, t, 0.55f);                                     /* the body   */
    gmw_band(x, 3.2f * u + 1.0f, H, 1.0f, 1.0f, 1.0f, 1.0f, t, 0.15f);    /* the core   */

    /* ARCS: three jagged threads of current wrapped round the core, re-struck every
       35 ms, so the line crackles rather than glows. */
    {
        const unsigned strike = (unsigned)(t / 0.035f);
        const int N = 36;
        glLineWidth(1.0f + 1.2f * u);
        for (int a = 0; a < 3; a++) {
            glBegin(GL_LINE_STRIP);
            for (int s = 0; s <= N; s++) {
                const float y = H * (float)s / N;
                const float j = (gmw_rand(strike * 7u + a, (unsigned)s) - 0.5f) * 2.0f;
                const float amp = (10.0f + 16.0f * gmw_rand(strike, 100u + a)) * u;
                const float al = 0.35f + 0.55f * gmw_rand(strike * 3u + a, 500u + s);
                glColor4f(r * 0.5f + 0.5f, g * 0.5f + 0.5f, b * 0.5f + 0.5f, al * swell);
                glVertex2f(x + j * amp, y);
            }
            glEnd();
        }
        glLineWidth(1.0f);
    }

    /* SPARKS thrown back off the line into the new picture: each has a birth time on a
       fixed schedule and a deterministic path, so no state is kept between frames. */
    {
        const int PER_S = 320;
        const float LIFE = 0.30f;
        const int first = (int)((t - LIFE) * PER_S), last = (int)(t * PER_S);
        glBegin(GL_LINES);
        for (int i = first < 0 ? 0 : first; i <= last; i++) {
            const float born = (float)i / PER_S;
            const float age = t - born;
            if (age < 0.0f || age > LIFE) continue;
            if (born > GMW_WIPE_S * 0.92f) continue;   /* none once the line has gone */
            /* where the line was when the spark left it */
            const float pb = born / GMW_WIPE_S;
            const float pad = 120.0f * u;
            const float bx = -pad + ((float)fbw + 2.0f * pad) * gmw_ease(pb);
            const float by = H * gmw_rand((unsigned)i, 11u);
            const float sp = (140.0f + 360.0f * gmw_rand((unsigned)i, 12u)) * u;
            const float vy = (gmw_rand((unsigned)i, 13u) - 0.5f) * 260.0f * u;
            const float px = bx - sp * age, py = by + vy * age + 380.0f * u * age * age;
            const float k = 1.0f - age / LIFE;
            const float tail = 0.035f;
            glColor4f(1.0f, 1.0f, 1.0f, k);
            glVertex2f(px, py);
            glColor4f(r, g, b, 0.0f);
            glVertex2f(px + sp * tail, py - (vy + 760.0f * u * age) * tail);
        }
        glEnd();
    }
}

/* ================================================================================== *
 *  THE WHOOSH
 *
 *  The sound of the line going by, SYNTHESISED once at boot like the rain (rain_audio.h):
 *  no clip on either disc is this, so there is no asset to license and the same bytes on
 *  every machine. Built to the line's own motion rather than laid over it:
 *
 *    THE BODY   noise through a resonant band pass whose centre follows the line's SPEED,
 *               the derivative of the same cubic ease the picture uses, so the pitch and
 *               the loudness swell as the line accelerates and fall as it slows. A little
 *               higher on the way in than on the way out, which is how a pass-by sounds.
 *    THE AIR    the noise above the band, quieter, for the rush.
 *    THE SPARK  sparse clicks of a few samples, high, riding the envelope: the arcs.
 *
 *  It is played on the effects bus, so the Sound volume slider is its volume, and panned
 *  left to right with the line while it plays (gmw_frame). Nothing about the game's own
 *  sound changes.
 * ================================================================================== */
static const float GMW_WHOOSH_S = 0.75f;   /* the clip: the crossing and a short tail   */
static short* g_gmwWhoosh = NULL;
static int    g_gmwWhooshN = 0;
static int    g_gmwWhooshVoice = -1;

static float gmw_speed(float p)            /* the ease's slope, 0..1, peak at p = 0.5 */
{
    if (p <= 0.0f || p >= 1.0f) return 0.0f;
    const float d = p < 0.5f ? 12.0f * p * p : 3.0f * (2.0f - 2.0f * p) * (2.0f - 2.0f * p);
    return d / 3.0f;
}

/* Where the whoosh sits, -1000 left to +1000 right, when the line is `p` of the way across:
   the line's own eased position, kept a little in from the edges. One function for play
   and for the WAV, so the gate hears what the player hears. */
static int gmw_whoosh_pan(float p)
{
    return (int)(-700.0f + 1400.0f * gmw_ease(p));
}

static void gmw_whoosh_build(void)
{
    if (g_gmwWhoosh) return;
    const int N = (int)(MIX_RATE * GMW_WHOOSH_S);
    float* b = (float*)malloc(sizeof(float) * (size_t)N);
    short* pcm = (short*)malloc(sizeof(short) * (size_t)N);
    if (!b || !pcm) { free(b); free(pcm); return; }
    unsigned lcg = 0x2468ACE1u;               /* a fixed generator: the same bytes every run */
    float low = 0.0f, band = 0.0f, env = 0.0f, spark = 0.0f, sparkHp = 0.0f, peak = 0.0f;
    float air = 0.0f, sparkLp = 0.0f;
    const float fs = (float)MIX_RATE;
    for (int i = 0; i < N; i++) {
        const float t = (float)i / fs;
        const float p = t / GMW_WIPE_S;
        lcg = lcg * 1664525u + 1013904223u;
        const float x = (float)(int)(lcg >> 8) * (1.0f / 8388608.0f) - 1.0f;
        /* the envelope chases the line's speed: quick to rise, slower to let go */
        const float target = powf(gmw_speed(p), 0.75f);
        env += (target - env) * (target > env ? 0.0020f : 0.00035f) * (44100.0f / fs);
        /* the band's centre: up with the speed, a touch lower once past the middle */
        const float sp = gmw_speed(p);
        const float fc = (320.0f + 2600.0f * sp) * (p < 0.5f ? 1.10f : 0.85f);
        const float f = 2.0f * sinf(3.14159265f * (fc < fs * 0.2f ? fc : fs * 0.2f) / fs);
        const float q = 0.30f;   /* resonant: a pitched rush, not flat noise */
        low += f * band;
        const float high = x - low - q * band;
        band += f * high;
        /* sparse sparks: a click now and then, more of them where the line is fastest */
        lcg = lcg * 1664525u + 1013904223u;
        if ((float)(lcg >> 8) * (1.0f / 16777216.0f) < 0.0009f * env) spark += 1.0f;
        spark *= 0.80f;
        const float sparkH = spark - sparkHp;   /* a one-pole high pass: only the click */
        sparkHp += 0.25f * (spark - sparkHp);
        sparkLp += 0.35f * (sparkH - sparkLp);  /* and not above ~3.5 kHz: a crackle, not a tick */
        air += 0.18f * (high - air);            /* the rush above the band, rounded off */
        const float v = env * (band * 1.00f + low * 0.15f + air * 0.12f) + sparkLp * 0.30f * env;
        b[i] = v;
        if (fabsf(v) > peak) peak = fabsf(v);
    }
    /* to a fixed peak, with 3 ms either end faded so the clip starts and stops on zero */
    const float sc = peak > 1e-6f ? 21000.0f / peak : 0.0f;
    const int ramp = (int)(fs * 0.003f);
    for (int i = 0; i < N; i++) {
        float v = b[i] * sc;
        if (i < ramp) v *= (float)i / ramp;
        if (i > N - 1 - ramp) v *= (float)(N - 1 - i) / ramp;
        pcm[i] = (short)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : (int)v);
    }
    free(b);
    g_gmwWhoosh = pcm;
    g_gmwWhooshN = N;
    fprintf(stderr, "GFXWIPE|whoosh built: %d samples at %d Hz\n", N, MIX_RATE);
}

static void gmw_whoosh_play(void)
{
    if (!g_au) return;
    gmw_whoosh_build();
    if (!g_gmwWhoosh) return;
    Mixer* m = cnc_audio_mixer(g_au);
    if (g_gmwWhooshVoice >= 0 && mixer_voice_active(m, g_gmwWhooshVoice))
        mixer_voice_stop(m, g_gmwWhooshVoice);
    /* priority 200: above the battle's ordinary effects, so a busy fight cannot steal
       the one sound that says the picture just changed */
    g_gmwWhooshVoice = mixer_play(m, MIX_BUS_FX, g_gmwWhoosh, g_gmwWhooshN, 800, gmw_whoosh_pan(0.0f), 200);
    printf("GFXWIPE|whoosh|%s\n", g_gmwWhooshVoice >= 0 ? "playing" : "REFUSED");
    fflush(stdout);
}

/* THE WHOOSH ON ITS OWN, rendered through a fresh copy of the game's mixer with the same
   left-to-right pan steps gmw_whoosh_follow gives it at 60 frames a second, and written
   as a stereo WAV. For a gate, and for a listener who is not at the machine: the headless
   recorder only advances with engine ticks, so a switch between ticks never reaches it. */
static int gmw_whoosh_wav(const char* path)
{
    gmw_whoosh_build();
    if (!g_gmwWhoosh) return 0;
    Mixer* m = mixer_create();
    WavOut* w = m ? wav_open(path, MIX_RATE, 2) : NULL;
    if (!w) { if (m) mixer_destroy(m); return 0; }
    const int v = mixer_play(m, MIX_BUS_FX, g_gmwWhoosh, g_gmwWhooshN, 800, gmw_whoosh_pan(0.0f), 200);
    const int frame = MIX_RATE / 60;                 /* one display frame of samples */
    std::vector<short> buf((size_t)frame * 2);
    const long total = (long)g_gmwWhooshN + MIX_RATE / 4;
    for (long at = 0; at < total; at += frame) {
        const float p = (float)at / (float)MIX_RATE / GMW_WIPE_S;
        if (v >= 0 && mixer_voice_active(m, v))
            mixer_voice_gain(m, v, 800, gmw_whoosh_pan(p));
        mixer_render(m, &buf[0], frame);
        wav_write(w, &buf[0], (long)frame * 2);
    }
    const int pk = wav_peak(w);
    const double rms = wav_rms(w);
    wav_close(w);
    mixer_destroy(m);
    printf("GFXWIPE|whoosh wav|%s|peak=%d|rms=%.0f\n", path, pk, rms);
    fflush(stdout);
    return 1;
}

/* Left to right with the line, while it plays. */
static void gmw_whoosh_follow(float p)
{
    if (!g_au || g_gmwWhooshVoice < 0) return;
    Mixer* m = cnc_audio_mixer(g_au);
    if (!mixer_voice_active(m, g_gmwWhooshVoice)) { g_gmwWhooshVoice = -1; return; }
    mixer_voice_gain(m, g_gmwWhooshVoice, 800, gmw_whoosh_pan(p));
}

/* ================================================================================== *
 *  THE SWITCH, AND THE FRAME HOOK
 * ================================================================================== */

/* The mode in use becomes `mode`, through the Visuals screen's own apply, so every mode
   keeps its own settings and the choice is written to the preset like a dialog click. */
static void gmw_switch_quiet(int mode)
{
    DOPT_Visuals v;
    memset(&v, 0, sizeof v);
    vis_from_fx(&v);
    v.mode = mode;
    opt_apply_visuals(NULL, &v);
    dopt_set_visuals(&g_optState, &v);
}

static void gmw_switch(int mode)
{
    gmw_switch_quiet(mode);
    printf("GFXMODE|%s|space\n", FX_MODE_NAME[mode]);
    fflush(stdout);
}

/* SPACE. A press while the line is crossing is kept, one deep, and taken when it lands,
   so a quick double press goes two modes on rather than being lost. */
static void gmw_request(void)
{
    if (g_gmwState != GMW_IDLE || g_gmwWarming) { g_gmwQueued = 1; return; }
    g_gmwFrom = fx_mode_of(&g_fx);
    g_gmwTarget = (g_gmwFrom + 1) % FX_MODE_COUNT;
    /* WHERE N64 IS NOT OFFERED (Red Alert, which has only Classic and Enhanced: the
       cartridge never had its art), SPACE steps over it, as the Visuals screen does */
    if (g_gmwTarget == FX_MODE_N64 && g_optState.n64_unavailable)
        g_gmwTarget = (g_gmwTarget + 1) % FX_MODE_COUNT;
    g_gmwState = GMW_CAPTURE;
}

/* While a snapshot is due or on screen the pointer is drawn after this hook. */
static bool gmw_holds_pointer(void)
{
    return g_gmwState != GMW_IDLE || g_gmwWarming || g_gmwWarmDue;
}

static void gmw_capture(int fbw, int fbh)
{
    if (!g_gmwSnap) glGenTextures(1, &g_gmwSnap);
    glBindTexture(GL_TEXTURE_2D, g_gmwSnap);
    if (g_gmwSnapW != fbw || g_gmwSnapH != fbh) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fbw, fbh, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        g_gmwSnapW = fbw; g_gmwSnapH = fbh;
    }
    glReadBuffer(GL_BACK);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, fbw, fbh);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* ================================================================================== *
 *  AHEAD OF THE PRESS
 *
 *  Every mode builds things the first time it is drawn, and a SPACE into a mode that has
 *  never been drawn this mission used to pay for all of them in one frame. Measured on
 *  SCG01EA with the Remastered Collection installed, before any of this: 4.1 s for the
 *  first Enhanced frame, 1.6 s for the first Enhanced one after starting in Classic,
 *  0.55 to 0.68 s for every return to either afterwards.
 *
 *  The returns are gone at their source (the water keeps what it built per atlas). The
 *  first visits are paid here instead, while another mode is on screen, one task a frame
 *  in the first frames of the mission, where the map has only just appeared and a short
 *  pause reads as part of loading rather than as a stutter in play. The Remastered sheet,
 *  the one piece measured in seconds, is built on a thread (terrain_remaster_prebuild_tick
 *  in cnc_eyes.cpp) and reaches the GPU a band at a time.
 *
 *  Each task runs only when the mode on screen does not already need it (that mode
 *  builds it itself, on its own first frame, as it always has), and only in a player's
 *  run: an automated one keeps the old order unless a script asks (`gfxprebuild`).
 * ================================================================================== */
static void gmw_prewarm(void)
{
    static char scen[20] = "";
    static int done = 0;
    if (g_rtpAllow != 1) return;
    if (strcmp(scen, g_pack.scen)) {
        snprintf(scen, sizeof scen, "%s", g_pack.scen);
        done = 0;
        g_gmwWarmed = 0;
    }
    const int mode = fx_mode_of(&g_fx);
    const int f = g_rtpFrames;
    const bool chainWanted = true;
    const bool waterWanted = g_fx.mode_row[FX_MODE_ENHANCED][FXR_WATER] >= 0.5f;
    Uint64 t0;
    const char* what = NULL;

    t0 = SDL_GetPerformanceCounter();
    if (f >= 2 && !(done & 1)) {
        done |= 1;
        /* THE CHAIN'S SHADERS, the cloud mask and the framebuffers: only Classic never
           boots them. */
        if (mode != FX_MODE_ENHANCED && chainWanted) { fx_boot(); what = "post chain"; }
    } else if (f >= 3 && !(done & 2)) {
        done |= 2;
        /* THE WATER FIELD, read from the cartridge's own tiles, so it serves every mode;
           only Classic never builds it. */
        if (mode != FX_MODE_ENHANCED && waterWanted) { water_field_sync(); what = "water field"; }
    } else if (f >= 4 && !(done & 4)) {
        done |= 4;
        /* THE GRASS: its mask and blades come from the cartridge's tiles too; only the
           colour step is redone for Enhanced's own ground when it arrives. */
        if (mode != FX_MODE_ENHANCED && g_fx.mode_row[FX_MODE_ENHANCED][FXR_GRASS] >= 0.5f
            && !strcmp(g_pack.theater, "TEMPERAT")) {
            grass_bake_sync(grass_bake_cfg_default());
            what = "grass";
        }
    } else if (f >= 5 && !(done & 8)) {
        done |= 8;
        /* ENHANCED'S SOFT COAST, the sheet its sea draws the shore through, for the atlas
           Enhanced's own settings pick. */
        if (mode != FX_MODE_ENHANCED && g_fx.mode_row[FX_MODE_ENHANCED][FXR_WATER] >= 0.5f) {
            const int want = (int)(g_fx.mode_row[FX_MODE_ENHANCED][FXR_TEXSET] + 0.5f);
            const int ai = (want == FX_TEX_ENH && g_pack.terrainTexEnh >= 0) ? g_pack.terrainTexEnh
                         : (want != FX_TEX_N64 && want != FX_TEX_REMASTER && g_pack.terrainTexDos >= 0)
                               ? g_pack.terrainTexDos
                         : (want == FX_TEX_REMASTER && g_pack.terrainTexRemaster >= 0)
                               ? g_pack.terrainTexRemaster
                         : g_pack.terrainTex;
            water_soft_prepare(ai, g_fx.mode_row[FX_MODE_ENHANCED][FXR_BILINEAR] >= 0.5f ? 1 : 0);
            what = "Enhanced's soft coast";
        }
    }
    /* THEN THE REST, whatever it is: each mode not yet drawn this mission is drawn once,
       for one frame, under a snapshot of the frame before (gmw_frame), which is the only
       way to be sure every shader, target and upload it will ask for exists before a
       player asks for it. Enhanced waits for its ground to be ready, or the frame
       would build the ground itself, synchronously, for seconds. */
    if (!what && f >= 8 && g_gmwState == GMW_IDLE && !g_gmwWarmDue && !g_gmwWarming) {
        g_gmwWarmed |= 1u << mode;
        for (int m = 0; m < FX_MODE_COUNT; m++) {
            if (g_gmwWarmed & (1u << m)) continue;
            if (m == FX_MODE_N64 && g_optState.n64_unavailable) continue;   /* never shown */
            g_gmwWarmDue = 1;
            g_gmwWarmMode = m;
            break;
        }
    }
    if (what)
        fprintf(stderr, "GFXWIPE|%s ready ahead of time in %.1f ms\n", what,
                (double)(SDL_GetPerformanceCounter() - t0) * 1000.0
                    / (double)SDL_GetPerformanceFrequency());
}

/* Called once a frame, after fx_present. */
static void gmw_frame(int fbw, int fbh)
{
    gmw_font_build();   /* once, on the first frame, so the first press has no build in it */
    if (g_au) gmw_whoosh_build();
    /* And everything the other two modes build the first time they are drawn. */
    terrain_remaster_prebuild_tick();
    gmw_prewarm();

    {
        const Uint32 now = SDL_GetTicks();
        float dt = g_gmwLastMs ? (float)(now - g_gmwLastMs) / 1000.0f : 0.0f;
        g_gmwLastMs = now;
        if (dt > 1.0f / 30.0f) dt = 1.0f / 30.0f;
        if (g_gmwFrozen) dt = 0.0f;
        if (g_gmwState == GMW_WIPE) g_gmwT += dt;
        if (g_gmwTitleT >= 0.0f) g_gmwTitleT += dt;
    }

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    cnc_screen_viewport(fbw, fbh);

    /* THE HIDDEN WARM-UP. First frame: this finished picture is kept and the other mode
       switched in, quietly. Second frame: that mode has just drawn everything it needs,
       under a snapshot that covers the whole screen, and the player's own mode comes
       straight back. Nothing is written to the preset on the way (g_gmwWarming). */
    if (g_gmwWarming) {
        const Uint64 a = SDL_GetPerformanceCounter();
        begin_overlay(fbw, fbh);
        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_gmwSnap);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f((float)fbw, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f((float)fbw, (float)fbh);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, (float)fbh);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        end_overlay();
        gmw_switch_quiet(g_gmwWarmBack);
        g_gmwWarming = 0;
        fprintf(stderr, "GFXWIPE|%s drawn once, hidden, to warm it up (%.1f ms since the switch)\n",
                FX_MODE_NAME[g_gmwWarmMode],
                (double)(SDL_GetPerformanceCounter() - g_gmwSwitchAt) * 1000.0
                    / (double)SDL_GetPerformanceFrequency());
        g_gmwSwitchAt = 0;
        (void)a;
    } else if (g_gmwWarmDue && g_gmwState == GMW_IDLE) {
        g_gmwWarmDue = 0;
        g_gmwWarmBack = fx_mode_of(&g_fx);
        if (g_gmwWarmMode != g_gmwWarmBack) {
            gmw_capture(fbw, fbh);
            g_gmwWarming = 1;
            g_gmwWarmed |= 1u << g_gmwWarmMode;
            gmw_switch_quiet(g_gmwWarmMode);
            g_gmwSwitchAt = SDL_GetPerformanceCounter();
        }
    }

    if (g_gmwState == GMW_CAPTURE) {
        /* This frame is the old mode's last, already on the glass: keep it, switch, and
           the line sets off on the next one. */
        gmw_capture(fbw, fbh);
        {
            const Uint64 a = SDL_GetPerformanceCounter();
            gmw_switch(g_gmwTarget);
            g_gmwSwitchAt = SDL_GetPerformanceCounter();
            fprintf(stderr, "GFXWIPE|switch took %.1f ms\n",
                    (double)(g_gmwSwitchAt - a) * 1000.0 / (double)SDL_GetPerformanceFrequency());
        }
        g_gmwState = GMW_WIPE;
        g_gmwT = 0.0f;
        g_gmwTitleT = 0.0f;
        gmw_whoosh_play();
        g_gmwTitleMode = g_gmwTarget;
        g_gmwSeed = g_gmwSeed * 1664525u + 1013904223u;
    } else if (g_gmwState == GMW_WIPE && g_gmwT >= GMW_WIPE_S && !g_gmwFrozen) {
        g_gmwState = GMW_IDLE;
        if (g_gmwQueued) { g_gmwQueued = 0; gmw_request(); }
    }

    /* THE FIRST FRAME OF THE NEW MODE, timed from the switch to here: the cost a player
       would feel as a hitch, and the reason the clock is capped. */
    if (g_gmwSwitchAt && g_gmwState == GMW_WIPE && g_gmwT > 0.0f) {
        fprintf(stderr, "GFXWIPE|first frame of %s took %.1f ms\n", FX_MODE_NAME[g_gmwTarget],
                (double)(SDL_GetPerformanceCounter() - g_gmwSwitchAt) * 1000.0
                    / (double)SDL_GetPerformanceFrequency());
        g_gmwSwitchAt = 0;
    }
    const bool wiping = (g_gmwState == GMW_WIPE);
    if (wiping) gmw_whoosh_follow(g_gmwT / GMW_WIPE_S);
    const bool title = g_gmwTitleT >= 0.0f && g_gmwTitleT < GMW_TITLE_HOLD + GMW_TITLE_FADE;
    if (!wiping && !title) {
        if (g_gmwTitleT >= GMW_TITLE_HOLD + GMW_TITLE_FADE) g_gmwTitleT = -1.0f;
        glViewport(vp[0], vp[1], vp[2], vp[3]);
        return;
    }

    begin_overlay(fbw, fbh);
    glDisable(GL_CULL_FACE);
    if (wiping) {
        const float u = (float)fbh / 800.0f;
        const float p = g_gmwT / GMW_WIPE_S;
        const float pad = 120.0f * u;
        const float x = -pad + ((float)fbw + 2.0f * pad) * gmw_ease(p);
        /* The old picture, right of the line. */
        if (x < (float)fbw && g_gmwSnap) {
            const float x0 = x < 0.0f ? 0.0f : x;
            const float u0 = x0 / (float)fbw;
            glDisable(GL_BLEND);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, g_gmwSnap);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
            glTexCoord2f(u0, 1.0f);   glVertex2f(x0, 0.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex2f((float)fbw, 0.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex2f((float)fbw, (float)fbh);
            glTexCoord2f(u0, 0.0f);   glVertex2f(x0, (float)fbh);
            glEnd();
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
        }
        gmw_draw_line(x, fbw, fbh, p);
    }
    if (title && g_gmwFontTex) {
        const float tt = g_gmwTitleT;
        const float a = gmw_smooth(0.0f, GMW_TITLE_IN, tt)
                      * (1.0f - gmw_smooth(GMW_TITLE_HOLD, GMW_TITLE_HOLD + GMW_TITLE_FADE, tt));
        /* It settles into place: a touch large and a touch low as it arrives. */
        const float settle = 1.0f - gmw_ease(gmw_smooth(0.0f, 0.35f, tt));
        const float caps = (float)fbh * 0.036f * (1.0f + 0.06f * settle);
        const float top = (float)fbh * 0.085f + caps * 0.25f * settle;
        const float cx = (float)fbw * 0.5f;
        const unsigned char* green = g_dbPack->pal8 + DB_GREEN * 3;
        const float r = green[0] / 255.0f, g = green[1] / 255.0f, b = green[2] / 255.0f;
        gmw_text("GRAPHICS MODE", cx, top - caps * 0.70f, caps * 0.40f, 0.45f,
                 r, g, b, a * 0.75f);
        gmw_text(gmw_mode_title(g_gmwTitleMode), cx, top, caps, 0.12f, r, g, b, a);
        /* A rule under it that draws itself out from the centre. */
        {
            const float scale = fmaxf(1.0f, floorf(caps / g_gmwCapH + 0.5f)) / GMW_UP;
            const float full = gmw_text_w(gmw_mode_title(g_gmwTitleMode), scale, floorf(caps * 0.12f + 0.5f));
            const float half = 0.5f * full * gmw_ease(gmw_smooth(0.05f, 0.45f, tt)) + caps * 0.4f;
            const float ry = top + caps * 1.28f, th = (float)fbh / 400.0f + 0.5f;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_QUADS);
            glColor4f(r, g, b, 0.0f);    glVertex2f(cx - half, ry);      glVertex2f(cx - half, ry + th);
            glColor4f(r, g, b, 0.9f * a); glVertex2f(cx, ry + th);        glVertex2f(cx, ry);
            glColor4f(r, g, b, 0.9f * a); glVertex2f(cx, ry);             glVertex2f(cx, ry + th);
            glColor4f(r, g, b, 0.0f);    glVertex2f(cx + half, ry + th); glVertex2f(cx + half, ry);
            glEnd();
        }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    end_overlay();
    glViewport(vp[0], vp[1], vp[2], vp[3]);
}

#endif /* OPENCNC3D_GMWIPE_MOD_H */
