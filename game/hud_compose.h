/* ====================================================================================
 *  hud_compose.h -- HUD SURFACES COMPOSED ONLY WHEN SOMETHING THEY SHOW HAS CHANGED.
 *
 *  Every HUD surface (the Red Alert and Tiberian Dawn Enhanced bars, the Red Alert wing,
 *  the header plates, the mission timer, the unit card, the queue strip, and the 8-bit
 *  DOS and Win95 bars) is rasterised on the CPU into a buffer of its own and uploaded into
 *  a texture of its own. Composing the Red Alert bar alone blends about 360k pixels, and
 *  until this file existed every surface was composed again on every drawn frame even
 *  when nothing on it had moved; only the upload was skipped, after a full-surface
 *  compare.
 *
 *  THE KEY. Each surface builds a small byte record of exactly the values its compose
 *  reads: pack pointers, sizes, the derived button frames (never the raw pointer state),
 *  the strings it prints, the float bits of every animated quantity it draws. Floats go
 *  in as their exact bits and strings as their bytes, and the record is compared byte for
 *  byte, so there is no hash and nothing that can collide. When the record equals the one
 *  the surface's texture was last composed from, the compose and the upload are both
 *  skipped and only the quads are drawn. Leaving an input out of a key would show a stale
 *  surface, which is what the verify mode below exists to catch.
 *
 *  WHAT INVALIDATES A SURFACE besides its key: a texture name of 0 (a fresh or deleted
 *  texture), a texture name that is not the one the key was stored against, g_hudGen
 *  (bumped by sb_init, sb_free, rah_free, the HUD and UI-scale setters and any layout
 *  change), and any h6_upload into the same texture from a path that keeps no key: two
 *  paths share some textures (the legacy 640 bar and the Enhanced bar, the two unit
 *  cards), and h6_upload forgets every key stored against the texture it writes.
 *
 *  THE SWITCH, read once: OPENCNC3D_HUD_COMPOSE
 *     0       compose every surface on every frame, the behaviour before this file;
 *     1       the default: skip, or patch, what has not changed;
 *     verify  take exactly the decisions 1 takes, so the pictures are 1's pictures, and
 *             also compose every skipped or patched surface from scratch and compare it
 *             byte for byte with what the skip path left; each difference prints a
 *             HUDCOMPOSEVERIFY line and counts in the HUDCOMPOSE line's mismatch=.
 *  The HUDCOMPOSE line is printed by mesh_batch_dump beside HUDUPLOAD.
 *
 *  TIER 1. Nothing here touches the GPU beyond skipping an upload, so the Voodoo answer
 *  is the same as the composing it skips: a blit of a CPU buffer.
 * ==================================================================================== */
#ifndef HUD_COMPOSE_H
#define HUD_COMPOSE_H

static int hud_compose_mode(void)
{
    static int mode = -1;
    if (mode < 0) {
        const char* v = getenv("OPENCNC3D_HUD_COMPOSE");
        mode = (!v || !*v) ? 1 : !strcmp(v, "0") ? 0 : !strcmp(v, "verify") ? 2 : 1;
    }
    return mode;
}
static bool hud_compose_verify(void) { return hud_compose_mode() == 2; }

static unsigned g_hudGen = 1;
static void hud_gen_bump(void) { g_hudGen++; }

static unsigned long g_hudComposed = 0, g_hudSkipped = 0, g_hudCellPatches = 0,
                     g_hudRadarPatches = 0, g_hudVerified = 0, g_hudMismatches = 0;

/* A key under construction. Its buffer is kept between frames, so building one costs no
   allocation once it has reached its size. */
struct HudKey {
    std::vector<unsigned char> b;
    void clear(void) { b.clear(); }
    void raw(const void* p, size_t n) {
        const unsigned char* c = (const unsigned char*)p;
        b.insert(b.end(), c, c + n);
    }
    void i(int v) { raw(&v, sizeof v); }
    void f(float v) { raw(&v, sizeof v); }               /* exact bits, never a tolerance */
    void p(const void* v) { raw(&v, sizeof v); }
    void s(const char* t) {
        if (!t) { i(-1); return; }
        const size_t n = strlen(t);
        i((int)n); raw(t, n);
    }
};

/* One surface: the key its texture's pixels were composed from, and in verify mode a
   copy of those pixels, for surfaces that compose into a scratch buffer they share. */
struct HudSurface {
    const char* name;
    GLuint* tex;
    GLuint texName;
    unsigned gen;
    bool valid, registered;
    std::vector<unsigned char> key, pixels;
};
#define HUD_MAX_SURFACES 32
static HudSurface* g_hudSurfaces[HUD_MAX_SURFACES];
static int g_hudSurfaceN = 0;

static void hud_surface_register(HudSurface& s, const char* name, GLuint* tex)
{
    if (s.registered) return;
    s.registered = true; s.name = name; s.tex = tex; s.valid = false;
    if (g_hudSurfaceN < HUD_MAX_SURFACES) g_hudSurfaces[g_hudSurfaceN++] = &s;
}

/* Any upload into `tex` makes every key stored against it stale. */
static void hud_forget_tex(const GLuint* tex)
{
    for (int k = 0; k < g_hudSurfaceN; k++)
        if (g_hudSurfaces[k]->tex == tex) g_hudSurfaces[k]->valid = false;
}

/* True when the surface's texture already shows what `key` describes. Never in mode 0. */
static bool hud_surface_fresh(const HudSurface& s, const HudKey& key)
{
    return hud_compose_mode() != 0 && s.valid && s.tex && *s.tex && *s.tex == s.texName
        && s.gen == g_hudGen && s.key == key.b;
}

/* After the surface's own upload: remember what its texture now shows. */
static void hud_surface_store(HudSurface& s, const HudKey& key,
                              const unsigned char* px = NULL, size_t n = 0)
{
    s.valid = true; s.texName = s.tex ? *s.tex : 0; s.gen = g_hudGen; s.key = key.b;
    if (hud_compose_verify() && px) s.pixels.assign(px, px + n);
}

/* Verify: compare what the skip path left with a compose from scratch. */
static void hud_verify_compare(const char* surface, const unsigned char* kept,
                               const unsigned char* fresh, size_t n)
{
    g_hudVerified++;
    if (!memcmp(kept, fresh, n)) return;
    size_t at = 0, diff = 0;
    for (size_t k = 0; k < n; k++) if (kept[k] != fresh[k]) { if (!diff) at = k; diff++; }
    g_hudMismatches++;
    if (g_hudMismatches <= 40)
        printf("HUDCOMPOSEVERIFY|surface=%s|bytes=%zu|first=%zu|of=%zu\n", surface, diff, at, n);
}

/* The 8-bit bars and the small DOS plates: the indexed pixels and the palette are the
   whole input of their RGBA conversion, so a part whose pixels and palette match the ones
   it last converted needs neither the conversion nor the upload. */
struct HudIndexed {
    GLuint texName;
    unsigned gen;
    bool valid;
    std::vector<unsigned char> px, rgba;   /* rgba: verify mode only */
    unsigned char pal[768];
};
static bool hud_indexed_fresh(const HudIndexed& c, GLuint tex, const unsigned char* px,
                              size_t n, const unsigned char* pal)
{
    return hud_compose_mode() != 0 && c.valid && tex && c.texName == tex && c.gen == g_hudGen
        && c.px.size() == n && !memcmp(c.px.data(), px, n) && !memcmp(c.pal, pal, 768);
}
static void hud_indexed_store(HudIndexed& c, GLuint tex, const unsigned char* px, size_t n,
                              const unsigned char* pal, const unsigned char* rgba)
{
    c.valid = true; c.texName = tex; c.gen = g_hudGen;
    c.px.assign(px, px + n); memcpy(c.pal, pal, 768);
    if (hud_compose_verify()) c.rgba.assign(rgba, rgba + n * 4);
}

/* A small plate re-specified with glTexImage2D (the tooltip, the queue digit, the
   placement blocker's name): the text and the palette it was rasterised from. */
struct HudPlate {
    GLuint texName;
    unsigned gen;
    bool valid;
    std::vector<unsigned char> key, rgba;  /* rgba: verify mode only */
};
static bool hud_plate_fresh(const HudPlate& c, GLuint tex, const HudKey& key)
{
    return hud_compose_mode() != 0 && c.valid && tex && c.texName == tex && c.gen == g_hudGen
        && c.key == key.b;
}
static void hud_plate_store(HudPlate& c, GLuint tex, const HudKey& key,
                            const unsigned char* rgba, size_t n)
{
    c.valid = true; c.texName = tex; c.gen = g_hudGen; c.key = key.b;
    if (hud_compose_verify()) c.rgba.assign(rgba, rgba + n);
}

static void hud_compose_dump(void)
{
    printf("HUDCOMPOSE|composed=%lu|skipped=%lu|cell_patches=%lu|radar_patches=%lu"
           "|mode=%s|verified=%lu|mismatch=%lu\n",
           g_hudComposed, g_hudSkipped, g_hudCellPatches, g_hudRadarPatches,
           hud_compose_mode() == 0 ? "0" : hud_compose_mode() == 2 ? "verify" : "1",
           g_hudVerified, g_hudMismatches);
}

#endif /* HUD_COMPOSE_H */
