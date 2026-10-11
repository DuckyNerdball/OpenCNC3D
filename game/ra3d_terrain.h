/* ra3d_terrain.h -- RED ALERT'S GROUND under Enhanced.
 *
 * Under Enhanced the Red Alert module draws its world in the shared 3D path, and that
 * path's ground used to be a placeholder: the Tiberian Dawn pack the module loads so the
 * renderer has a pack at all, 64 cells wide on Red Alert's 128-cell maps. This replaces
 * the placeholder's ground (its sea, its terrain, its coast, its ground detail) with the
 * Red Alert mission's own map, and nothing else: everything the mode draws on the ground
 * (the infantry, the buildings and vehicles, the effects, the shroud) still draws, on it.
 *
 * THE MAP is the one the engine exports (GAME_STATE_STATIC_MAP: per cell the template's
 * INI name and the icon within it, the mission rectangle and the theater), laid flat on
 * the world grid at Red Alert's own cell coordinates.
 *
 * THE SHEET. terrainenh_RA_<TH>.pack (in the module's ra3d/ folder) carries one 144-texel
 * RGB painting per (template name, icon, variant), with the template's size in cells.
 * Every key in the pack gets a slot of its own in one sheet, laid out like Tiberian
 * Dawn's Enhanced sheet (each cell with a gutter of its own edge, one texel for each of
 * the 1995 cell's 24, the sheet at its own size rather than a power of two). Red Alert
 * has no cartridge bank, so the slots come from the pack's keys rather than a bank table.
 * A cell the map uses and the pack lacks takes the 1995 tile out of ra3d/terrain.pack,
 * enlarged the way Tiberian Dawn's sheet enlarges its own 1995 fallback, so there are no
 * holes. The 1995 set (the DOS texset, and Interior, which has no Enhanced set) is the
 * same sheet built from those tiles alone at 24 texels a cell.
 *
 * A variant is chosen per PLACEMENT by the same rule as Tiberian Dawn's
 * (enh_variant_pick): the placement's corner counted in template widths and heights.
 *
 * THE JOIN BLEND. terrainseam_RA_<TH>.pack carries the strips, drawn by the same code as
 * Tiberian Dawn's (terrain_seam.h): the right or lower piece's left or top strip over its
 * neighbour, one direction per join, a feature's strip never over a cliff, per variant.
 * The clear ground (CLEAR1) takes no strip, as Tiberian Dawn's grass takes none.
 *
 * WATER is Tiberian Dawn's Enhanced sea (water_mod.h), on Red Alert's map. Red Alert's
 * templates carry no alpha, so the cut is read off the 1995 tiles: a texel of a template
 * that can hold water (the open sea W1 and W2, the shores SH, the sea cliffs WC, the
 * rivers RV and river cliffs RC, the bridges, the fords, the falls and F01..F06) is water
 * where its palette index is one of the sea's (RA3D_WATER_INDEX below), cleaned of single
 * stray texels. Each slot of the sheet carries that cut in its alpha, bilinear up to the
 * cell, the land solid and the painted water fading out into the sea over the width of
 * Tiberian Dawn's soft coast. When the sea is drawn the ground is cut and blended there
 * over it, with the same painted ground under it as the sea floor (ra3d_draw_floor); the
 * sea's base texture is Red Alert's own open-water painting (W2, else W1) from the same
 * set. When it is not (water_fx off, Interior, the chain off) the ground draws opaque as
 * it always did and the painted water shows. Surface ships leave Tiberian Dawn's boats'
 * wake (water_is_boat). The cut is the 1995 tile's: where a 144-texel painting puts its
 * coast somewhere else, its painted water stays opaque on the land side of the cut, and
 * on the water side the sea yields to the painting wherever the painting is not blue
 * (water_mod.h). The sea's hue is the painting's own: C&C's water_natural is not
 * applied to it.
 *
 * THE HEIGHTS are the mission's own scene's (ra3d/3d/<SCEN>.pack, its auto heightmap on
 * Red Alert's 128-cell grid, read through terrain_corner_y), and the ground is lit as
 * Tiberian Dawn's Enhanced ground is: the console's corner shade off those heights
 * (g_shadeGrid; 160/255 on level ground) times the shroud's corner light, with the
 * per-pixel sun of the post chain over it, its normal the same corner normal. A mission
 * with no scene of its own draws flat at 0 (ra3d_flat_ground in cnc_eyes.cpp).
 *
 * Tier 2 only, like the Tiberian Dawn Enhanced terrain it follows: Win98 has no Red Alert
 * module at all.
 */
#ifndef OPENCNC3D_RA3D_TERRAIN_H
#define OPENCNC3D_RA3D_TERRAIN_H

#define RA3D_ENH_MAGIC  "C3DRA144"
/* GL 1.4's automatic mipmaps, which the Windows headers (GL 1.1) do not name */
#define RA3D_GENERATE_MIPMAP 0x8191
#define RA3D_TEXTURE_LOD_BIAS 0x8501
#define RA3D_SEAM_MAGIC "C3DRASEM"

struct Ra3dSlot { int name, icon, var, w, h; };

struct Ra3dGround {
    bool built = false, ok = false;
    std::string key;                 /* theater, set and scenario the sheet was built for */
    char theater = 0;                /* 'S' snow, 'T' temperate, 'I' interior          */
    int set = -1;                    /* FX_TEX_ENH or FX_TEX_DOS, as built             */
    GLuint tex = 0;
    int w = 0, h = 0, ts = 0, gutter = 0, pitch = 0, cols = 1;
    std::vector<Ra3dSlot> slot;
    std::unordered_map<std::string, int> slotOf;   /* "name/icon/var" -> slot           */
    std::vector<std::string> names;
    std::unordered_map<std::string, int> nameOf;
    int clearName = -1;
    std::vector<int> cellBase;       /* per map cell: its first painting's slot          */
    std::vector<unsigned char> varHas;   /* per slot: bit v set if variant v exists      */
    std::vector<int> varSlot;        /* slot * TS_VARS + v -> the variant's slot, or -1  */
    std::vector<unsigned char> avg;  /* per slot: its painting's mean colour, for the radar */
    std::vector<unsigned char> wet;  /* per slot: 1 when its cut holds water          */
    int fromPack = 0, fromTiles = 0, missing = 0, variants = 0;
    int wetSlots = 0;
};
static Ra3dGround g_ra3d;
static TerrainSeams g_ra3dSeams;     /* indexed by Red Alert slot and template name      */
static bool g_ra3dSeamsTried = false;

/* 'S' snow, 'T' temperate, 'I' interior (exported as NONE, see ra2d_pal_index) */
static char ra3d_theater(void) { return ra2d_theater_letter(); }
static const char* ra3d_theater_name(char th)
{
    return th == 'S' ? "SNOW" : th == 'I' ? "INTERIOR" : "TEMPERAT";
}

/* The template's INI name out of the engine's export ("D01_i3.tga" -> "D01"). */
static void ra3d_template_name(const char* exported, char out[17])
{
    const char* suffix = strstr(exported, "_i");
    size_t len = suffix ? (size_t)(suffix - exported) : strcspn(exported, ".");
    if (len > 16) len = 16;
    memcpy(out, exported, len);
    out[len] = 0;
}

static std::string ra3d_key(const char* name, int icon, int var)
{
    return std::string(name) + "/" + std::to_string(icon) + "/" + std::to_string(var);
}

static int ra3d_name_id(const char* name)
{
    std::unordered_map<std::string, int>::const_iterator it = g_ra3d.nameOf.find(name);
    if (it != g_ra3d.nameOf.end()) return it->second;
    const int id = (int)g_ra3d.names.size();
    g_ra3d.names.push_back(name);
    g_ra3d.nameOf[name] = id;
    return id;
}

/* A pack beside the module, in the repository's art folder for a repository run, or
   named outright by an environment variable ("off" turns it off). */
static const char* ra3d_pack_path(const char* env, const char* stem, char th,
                                  char* buf, size_t n)
{
    const char* e = getenv(env);
    if (e && (!strcmp(e, "off") || !strcmp(e, "none"))) return NULL;
    if (e && *e) return e;
    const char* TH = ra3d_theater_name(th);
    for (int w = 0; w < 3; w++) {
        if (w == 0) snprintf(buf, n, "ra3d/%s_RA_%s.pack", stem, TH);
        else snprintf(buf, n, "%sart/remaster/textures/RA_%s/%s_RA_%s.pack",
                      w == 2 ? "../" : "", TH, stem, TH);
        if (rt_override_exists(buf)) return buf;
    }
    return NULL;
}

/* Whether the Enhanced set exists for the mission's theater (the Visuals dialog greys
   the entry otherwise). */
static int ra3d_enh_available(void)
{
    char buf[256];
    const char th = ra3d_theater();
    if (!th || th == 'I') return 0;
    return ra3d_pack_path("OPENCNC3D_RATERRAINENH", "terrainenh", th, buf, sizeof buf) != NULL;
}

static std::vector<unsigned char> ra3d_read_file(const char* path)
{
    std::vector<unsigned char> d;
    FILE* f = fopen(path, "rb");
    if (!f) return d;
    if (fseek(f, 0, SEEK_END) == 0) {
        const long n = ftell(f);
        if (n > 0 && n < 400L * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0) {
            d.resize((size_t)n);
            if (fread(&d[0], 1, d.size(), f) != d.size()) d.clear();
        }
    }
    fclose(f);
    return d;
}

/* One slot's RGB and alpha into the RGBA sheet, its gutter replicated outward
   (rt_place's layout, and the slot's column out of this sheet's own width). A NULL alpha
   is solid ground. */
static void ra3d_place(std::vector<unsigned char>& sheet, int slot, const unsigned char* rgb,
                       const unsigned char* alpha)
{
    const Ra3dGround& G = g_ra3d;
    const int x0 = (slot % G.cols) * G.pitch + G.gutter, y0 = (slot / G.cols) * G.pitch + G.gutter;
    const int ts = G.ts, g = G.gutter;
    for (int y = -g; y < ts + g; y++) {
        const int sy = y < 0 ? 0 : y >= ts ? ts - 1 : y;
        unsigned char* d = &sheet[((size_t)(y0 + y) * G.w + x0 - g) * 4];
        for (int x = -g; x < ts + g; x++) {
            const int sx = x < 0 ? 0 : x >= ts ? ts - 1 : x;
            memcpy(d, rgb + ((size_t)sy * ts + sx) * 3, 3);
            d[3] = alpha ? alpha[(size_t)sy * ts + sx] : 255;
            d += 4;
        }
    }
}

/* The 1995 tile for a template cell, RGB at 24 texels, or false. */
static bool ra3d_tile_rgb(char th, const char* name, int icon, unsigned char out[24 * 24 * 3])
{
    char exported[40];
    snprintf(exported, sizeof exported, "%s_i%d.tga", name, icon);
    const RaClassicTile* t = ra2d_find_tile(th, exported, icon);
    if (!t) return false;
    const unsigned char* pal = g_raTilePal[th == 'S' ? 0 : th == 'I' ? 2 : 1];
    for (int i = 0; i < 24 * 24; i++) memcpy(out + i * 3, pal + t->pixels[i] * 3, 3);
    return true;
}

/* A painting's mean colour, every eighth texel each way. */
static void ra3d_mean(const unsigned char* rgb, int ts, unsigned char out[3])
{
    long sum[3] = { 0, 0, 0 }, n = 0;
    for (int y = 0; y < ts; y += 8)
        for (int x = 0; x < ts; x += 8, n++)
            for (int c = 0; c < 3; c++) sum[c] += rgb[((size_t)y * ts + x) * 3 + c];
    for (int c = 0; c < 3; c++) out[c] = (unsigned char)(n ? sum[c] / n : 0);
}

/* ---- THE WATER CUT ------------------------------------------------------------------ */

/* The templates that can hold water at all. The palette's water colours also turn up,
   a texel here and there, in snow shadows on cliffs and in road dust; gating by template
   keeps every one of those land. Interior has none of these. */
static bool ra3d_water_family(const char* n)
{
    static const char* const pre[] = { "W1", "W2", "SH", "WC", "RV", "RC", "BR", "F0",
                                       "FALLS", "FORD", NULL };
    for (int i = 0; pre[i]; i++)
        if (!strncmp(n, pre[i], strlen(pre[i]))) return true;
    return false;
}

/* THE SEA'S PALETTE INDICES, the same in the snow, temperate and interior palettes:
   46, 47 and 62..67 and 72 are the open water's blues and greys, 96..102 the seven
   colours the 1996 game cycles for the glint, 167 and 176..180 the shallow blue-greys a
   shore and a river lay along the bank. Measured over every template of both theaters:
   with the template gate above, no road, slope, cliff or clear piece is cut. */
static bool ra3d_water_index(unsigned char i)
{
    return i == 46 || i == 47 || (i >= 62 && i <= 67) || i == 72 || (i >= 96 && i <= 102)
        || i == 167 || (i >= 176 && i <= 180);
}

/* A 3x3 majority (the tile's edge replicated outward): what the shore art dithers into
   the sand and the sea flecks into the surf goes to whichever side surrounds it. */
static void ra3d_cut_majority(unsigned char m[576])
{
    unsigned char o[576];
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 24; x++) {
            int n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    const int yy = y + dy < 0 ? 0 : y + dy > 23 ? 23 : y + dy;
                    const int xx = x + dx < 0 ? 0 : x + dx > 23 ? 23 : x + dx;
                    n += m[yy * 24 + xx];
                }
            o[y * 24 + x] = n >= 5 ? 1 : 0;
        }
    memcpy(m, o, 576);
}

/* Islands of `val` smaller than eight texels that do not reach the tile's edge become
   the other side: a speck of sea in the snow, a fleck of foam out at sea. One that
   reaches the edge may continue into the next cell, so it stays. */
static void ra3d_cut_specks(unsigned char m[576], unsigned char val)
{
    unsigned char seen[576];
    memset(seen, 0, sizeof seen);
    int stack[576], comp[576];
    for (int s0 = 0; s0 < 576; s0++) {
        if (m[s0] != val || seen[s0]) continue;
        int sp = 0, nc = 0;
        bool edge = false;
        stack[sp++] = s0; seen[s0] = 1;
        while (sp) {
            const int c = stack[--sp];
            comp[nc++] = c;
            const int x = c % 24, y = c / 24;
            if (x == 0 || y == 0 || x == 23 || y == 23) edge = true;
            const int nb[4] = { x > 0 ? c - 1 : -1, x < 23 ? c + 1 : -1,
                                y > 0 ? c - 24 : -1, y < 23 ? c + 24 : -1 };
            for (int k = 0; k < 4; k++)
                if (nb[k] >= 0 && !seen[nb[k]] && m[nb[k]] == val) { seen[nb[k]] = 1; stack[sp++] = nb[k]; }
        }
        if (!edge && nc < 8)
            for (int k = 0; k < nc; k++) m[comp[k]] = val ? 0 : 1;
    }
}

/* The water in one template cell's 1995 tile: 1 water, 0 land; the count of water texels. */
static int ra3d_water_cut(char th, const char* name, int icon, unsigned char out[576])
{
    memset(out, 0, 576);
    if (th == 'I' || !ra3d_water_family(name)) return 0;
    char exported[40];
    snprintf(exported, sizeof exported, "%s_i%d.tga", name, icon);
    const RaClassicTile* t = ra2d_find_tile(th, exported, icon);
    if (!t) return 0;
    for (int i = 0; i < 576; i++) out[i] = ra3d_water_index(t->pixels[i]) ? 1 : 0;
    ra3d_cut_majority(out);
    ra3d_cut_majority(out);
    ra3d_cut_specks(out, 1);
    ra3d_cut_specks(out, 0);
    int n = 0;
    for (int i = 0; i < 576; i++) n += out[i];
    return n;
}

/* A slot's alpha at the sheet's cell size: the cut bilinear up to it (rt_alpha_up's
   sampling), thresholded at the half, and ramped from that line out over the water by
   WATER_SOFT_RADIUS texels of the 1995 cell, scaled to this one: the width of Tiberian
   Dawn's soft coast, laid on the water side, where Red Alert's texels are its painted
   water (water_soft_ramp says why). */
static void ra3d_water_alpha(const unsigned char cut[576], int ts, unsigned char* alpha)
{
    std::vector<unsigned char> solid((size_t)ts * ts), hole((size_t)ts * ts);
    for (int y = 0; y < ts; y++) {
        const float fy = ((float)y + 0.5f) * 24.0f / (float)ts - 0.5f;
        const int y0 = (int)(fy < 0 ? 0 : fy), y1 = y0 + 1 < 24 ? y0 + 1 : 23;
        const float wy = fy - (float)y0 < 0 ? 0.0f : fy - (float)y0;
        for (int x = 0; x < ts; x++) {
            const float fx = ((float)x + 0.5f) * 24.0f / (float)ts - 0.5f;
            const int x0 = (int)(fx < 0 ? 0 : fx), x1 = x0 + 1 < 24 ? x0 + 1 : 23;
            const float wx = fx - (float)x0 < 0 ? 0.0f : fx - (float)x0;
            const float a00 = cut[y0 * 24 + x0] ? 0.0f : 255.0f, a01 = cut[y0 * 24 + x1] ? 0.0f : 255.0f;
            const float a10 = cut[y1 * 24 + x0] ? 0.0f : 255.0f, a11 = cut[y1 * 24 + x1] ? 0.0f : 255.0f;
            const float top = a00 + (a01 - a00) * wx, bot = a10 + (a11 - a10) * wx;
            const unsigned char a = (unsigned char)(top + (bot - top) * wy + 0.5f);
            solid[(size_t)y * ts + x] = a >= 128 ? 1 : 0;
            hole[(size_t)y * ts + x] = a >= 128 ? 0 : 1;
        }
    }
    water_soft_ramp(&solid[0], &hole[0], ts, ts, WATER_SOFT_RADIUS * (float)ts / 24.0f,
                    alpha, (size_t)ts, 1, true);
}

static void ra3d_free(void)
{
    if (g_ra3d.tex) glDeleteTextures(1, &g_ra3d.tex);
    g_ra3d = Ra3dGround();
    /* the sea's source goes with the sheet; its one white texel is kept for the next */
    if (g_waterRa.base) glDeleteTextures(1, &g_waterRa.base);
    const GLuint white = g_waterRa.white;
    g_waterRa = WaterRaSource();
    g_waterRa.white = white;
    if (g_ra3dSeams.texL) glDeleteTextures(1, &g_ra3dSeams.texL);
    if (g_ra3dSeams.texT) glDeleteTextures(1, &g_ra3dSeams.texT);
    g_ra3dSeams = TerrainSeams();
    g_ra3dSeamsTried = false;
}

/* The set this frame asks for: Enhanced's own when its pack is there, the 1995 tiles
   otherwise (Red Alert has no cartridge art and no Remastered terrain). */
static int ra3d_set_wanted(void)
{
    return (fx_texset_get() == FX_TEX_ENH && ra3d_theater() != 'I' && ra3d_enh_available())
           ? FX_TEX_ENH : FX_TEX_DOS;
}

static void ra3d_seams_load(void);

static int ra3d_cell_slot(int i, int x, int y);
static void ra3d_slot_uv(int slot, float* u0, float* v0, float* u1, float* v1);

/* THE SEA'S BASE: the open water Red Alert's ground paints, as one repeating texture.
   W2 is the 2x2 sea (icons 0..3 left to right, top to bottom), so its four paintings
   make one 2-cell tile; a set without it takes W1, one cell. Each painting is the set's
   own (the pack's at its size, else the 1995 tile enlarged), as the ground draws it. */
static bool ra3d_water_base(const std::vector<const unsigned char*>& pixels,
                            std::vector<unsigned char>& out, int* side, const char** what)
{
    const Ra3dGround& G = g_ra3d;
    const int ts = G.ts;
    std::vector<unsigned char> tile(24 * 24 * 3), rgb((size_t)ts * ts * 3);
    auto painting = [&](const char* name, int icon, int* fromPack) -> bool {
        std::unordered_map<std::string, int>::const_iterator it = G.slotOf.find(ra3d_key(name, icon, 0));
        if (it != G.slotOf.end() && pixels[it->second]) {
            memcpy(&rgb[0], pixels[it->second], rgb.size());
            (*fromPack)++;
            return true;
        }
        if (!ra3d_tile_rgb(G.theater, name, icon, &tile[0])) return false;
        if (ts == 24) memcpy(&rgb[0], &tile[0], tile.size());
        else rt_bilinear_rgb(&tile[0], 24, 24, 24 * 3, &rgb[0], ts, ts);
        return true;
    };
    int fromPack = 0;
    out.assign((size_t)4 * ts * ts * 3, 0);
    bool w2 = true;
    for (int i = 0; i < 4 && w2; i++) {
        if (!painting("W2", i, &fromPack)) { w2 = false; break; }
        const int ox = (i & 1) * ts, oy = (i >> 1) * ts;
        for (int y = 0; y < ts; y++)
            memcpy(&out[((size_t)(oy + y) * 2 * ts + ox) * 3], &rgb[(size_t)y * ts * 3], (size_t)ts * 3);
    }
    if (w2) {
        *side = 2 * ts;
        *what = fromPack == 4 ? "W2 painted" : "W2 1995";
        return true;
    }
    fromPack = 0;
    if (!painting("W1", 0, &fromPack)) return false;
    out.assign(rgb.begin(), rgb.end());
    *side = ts;
    *what = fromPack ? "W1 painted" : "W1 1995";
    return true;
}

/* HOW MUCH OF THE THEATER CAN HOLD WATER: every template and icon of its 1995 set run
   through the cut, once per theater (the gate's and the log reader's count). */
static void ra3d_water_census(char th)
{
    static std::string done;
    if (done.find(th) != std::string::npos) return;
    done += th;
    std::set<std::string> names;
    int icons = 0, total = 0;
    unsigned char cut[576];
    for (size_t i = 0; i < g_raTiles.size(); i++) {
        const RaClassicTile& t = g_raTiles[i];
        if (t.theater != th) continue;
        total++;
        if (ra3d_water_cut(th, t.name, t.icon, cut) > 0) { icons++; names.insert(t.name); }
    }
    fprintf(stderr, "WATER|ra|cut|theater=%s|templates=%d|icons=%d|of=%d\n",
            ra3d_theater_name(th), (int)names.size(), icons, total);
}

/* THE SEA'S SOURCE (water_mod.h's g_waterRa), filled from the sheet just built: the
   map's cells with their rectangles and holes, the sheet, the base and its grain. The
   cut atlas was laid with the sheet. No sea on Interior, on a map with no water, or on a
   mission drawn flat on the placeholder pack (its grid is not this map's). */
static void ra3d_water_source(const std::vector<const unsigned char*>& pixels)
{
    const Ra3dGround& G = g_ra3d;
    WaterRaSource& WR = g_waterRa;
    WR.ok = false;
    ra3d_water_census(G.theater);
    if (G.theater == 'I') { fprintf(stderr, "WATER|ra|none|theater=INTERIOR\n"); return; }
    if (ra3d_flat_ground()) { fprintf(stderr, "WATER|ra|none|no scene of its own\n"); return; }
    const int W = g_raStaticMap.MapCellWidth, H = g_raStaticMap.MapCellHeight;
    WR.cell.clear();
    WR.cell.reserve((size_t)W * H);
    WR.wetCells = 0;
    for (int i = 0; i < W * H; i++) {
        const int cx = g_raStaticMap.MapCellX + i % W, cy = g_raStaticMap.MapCellY + i / W;
        const int base = G.cellBase[i];
        if (base < 0 || cx < 0 || cy < 0 || cx >= g_gridW || cy >= g_gridH) continue;
        PackCell c;
        memset(&c, 0, sizeof c);
        c.x = (short)cx; c.y = (short)cy;
        c.holes = G.wet[base];
        ra3d_slot_uv(ra3d_cell_slot(i, cx, cy), &c.u0, &c.v0, &c.u1, &c.v1);
        WR.cell.push_back(c);
        WR.wetCells += c.holes;
    }
    if (!WR.wetCells) { fprintf(stderr, "WATER|ra|none|%s has no water\n", g_raStaticScene.c_str()); return; }
    std::vector<unsigned char> base;
    int side = 0;
    const char* what = "";
    if (!ra3d_water_base(pixels, base, &side, &what)) {
        fprintf(stderr, "WATER|ra|none|no open-water painting in this set\n");
        return;
    }
    /* its luminance's mean and spread, as water_grain_measure reads WATER1's */
    double sum = 0.0, sum2 = 0.0;
    const size_t np = (size_t)side * side;
    for (size_t i = 0; i < np; i++) {
        const unsigned char* q = &base[i * 3];
        const double l = (0.299 * q[0] + 0.587 * q[1] + 0.114 * q[2]) / 255.0;
        sum += l; sum2 += l * l;
    }
    const double mean = sum / (double)np, var = sum2 / (double)np - mean * mean;
    const double sd = var > 1.0e-6 ? sqrt(var) : 0.001;
    WR.grainMean = (float)mean;
    WR.grainInv = (float)(1.0 / (2.5 * sd));
    WR.baseRepeats = (float)G.ts / (float)side;     /* a painting is a cell */
    if (WR.base) glDeleteTextures(1, &WR.base);
    glGenTextures(1, &WR.base);
    glBindTexture(GL_TEXTURE_2D, WR.base);
    /* MIPMAPPED: a cell is a few dozen pixels on screen and the painting 144 texels, so
       the slide would shimmer through a plain linear filter */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, RA3D_GENERATE_MIPMAP, GL_TRUE);
    /* a level sharper than the footprint asks for: the painting's ripples are what the
       sea is made of, and the plain choice blurred them to a wash */
    glTexParameterf(GL_TEXTURE_2D, RA3D_TEXTURE_LOD_BIAS, -1.0f);
    GLint align = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, side, side, 0, GL_RGB, GL_UNSIGNED_BYTE, &base[0]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, align);
    if (!WR.white) {
        const unsigned char px[4] = { 255, 255, 255, 255 };
        glGenTextures(1, &WR.white);
        glBindTexture(GL_TEXTURE_2D, WR.white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    WR.sheet = PackTex();
    WR.sheet.w = WR.sheet.uw = G.w;
    WR.sheet.h = WR.sheet.uh = G.h;
    WR.sheet.gl = G.tex;
    WR.sheetKey = 1000 + G.set;
    WR.ok = true;
    fprintf(stderr, "WATER|ra|source|%s|%s|%s|wet_slots=%d|wet_cells=%d|base=%s|%dx%d|repeats=%.2f|"
                    "mean=%.3f|spread=%.3f\n",
            g_raStaticScene.c_str(), ra3d_theater_name(G.theater),
            G.set == FX_TEX_ENH ? "enhanced" : "1995", G.wetSlots, WR.wetCells, what, side, side,
            (double)WR.baseRepeats, mean, sd);
}

/* BUILD THE SHEET for the mission's theater and the set asked for. Once per mission and
   set: a theater's Enhanced pack is about 95 MB, read and laid out in well under a
   second. */
static bool ra3d_ground_ensure(void)
{
    if (!ra2d_load_terrain() || !ra2d_fetch_map()) return false;
    const char th = ra3d_theater();
    const int set = ra3d_set_wanted();
    const std::string key = std::string(1, th) + "/" + std::to_string(set) + "/" + g_raStaticScene;
    if (g_ra3d.built && g_ra3d.key == key) return g_ra3d.ok;
    const Uint64 t0 = SDL_GetPerformanceCounter();
    ra3d_free();
    Ra3dGround& G = g_ra3d;
    G.built = true;
    G.key = key;
    G.theater = th;
    G.set = set;
    G.clearName = ra3d_name_id("CLEAR1");

    /* the pack's keys, each a slot */
    std::vector<unsigned char> pack;
    int cell = 24;
    size_t count = 0;
    const char* packPath = NULL;
    char pbuf[256];
    if (set == FX_TEX_ENH) {
        packPath = ra3d_pack_path("OPENCNC3D_RATERRAINENH", "terrainenh", th, pbuf, sizeof pbuf);
        if (packPath) pack = ra3d_read_file(packPath);
        bool good = pack.size() >= 20 && !memcmp(&pack[0], RA3D_ENH_MAGIC, 8)
                    && rt_le32(&pack[8]) == 1;
        if (good) {
            cell = (int)rt_le32(&pack[12]);
            count = rt_le32(&pack[16]);
            good = cell % 24 == 0 && cell >= 24 && cell <= 24 * RT_ENH_KMAX
                   && pack.size() == 20 + count * 20 + count * (size_t)cell * cell * 3;
        }
        if (!good) {
            fprintf(stderr, "RA3D|enh|%s is not a version 1 Red Alert terrain pack; the 1995 tiles draw\n",
                    packPath ? packPath : "(no pack)");
            pack.clear(); count = 0; cell = 24; G.set = FX_TEX_DOS;
        }
    }
    const int k = cell / 24;
    G.ts = cell; G.gutter = k; G.pitch = cell + 2 * k;
    std::vector<const unsigned char*> pixels;       /* per slot: the pack's RGB, or NULL */
    for (size_t i = 0; i < count; i++) {
        const unsigned char* e = &pack[20 + i * 20];
        char name[17];
        memcpy(name, e, 16); name[16] = 0;
        const int icon = e[16] | (e[17] << 8), var = e[18], sz = e[19];
        if (var >= TS_VARS) continue;
        const std::string kk = ra3d_key(name, icon, var);
        if (G.slotOf.count(kk)) continue;
        Ra3dSlot s;
        s.name = ra3d_name_id(name); s.icon = icon; s.var = var;
        s.w = (sz & 15) + 1; s.h = ((sz >> 4) & 15) + 1;
        G.slotOf[kk] = (int)G.slot.size();
        G.slot.push_back(s);
        pixels.push_back(&pack[20 + count * 20 + i * (size_t)cell * cell * 3]);
        if (var) G.variants++;
    }
    /* the map's cells: each to its first painting's slot, a 1995 tile where the pack has none */
    const int W = g_raStaticMap.MapCellWidth, H = g_raStaticMap.MapCellHeight;
    G.cellBase.assign((size_t)W * H, -1);
    for (int i = 0; i < W * H; i++) {
        const CNCStaticCellStruct& c = g_raStaticMap.StaticCells[i];
        char name[17];
        ra3d_template_name(c.TemplateTypeName, name);
        const std::string kk = ra3d_key(name, c.IconNumber, 0);
        std::unordered_map<std::string, int>::const_iterator it = G.slotOf.find(kk);
        if (it == G.slotOf.end()) {
            Ra3dSlot s;
            s.name = ra3d_name_id(name); s.icon = c.IconNumber; s.var = 0; s.w = s.h = 1;
            G.slotOf[kk] = (int)G.slot.size();
            it = G.slotOf.find(kk);
            G.slot.push_back(s);
            pixels.push_back(NULL);
        }
        G.cellBase[i] = it->second;
        /* OPENCNC3D_RA3DDUMP=1: every cell's template and icon, for checking a picture
           against the map the engine exported */
        if (getenv("OPENCNC3D_RA3DDUMP"))
            fprintf(stderr, "RA3D|cell|%d,%d|%s|%d|%s\n", g_raStaticMap.MapCellX + i % W,
                    g_raStaticMap.MapCellY + i / W, name, c.IconNumber,
                    pixels[it->second] ? "pack" : "tile");
    }
    /* each first painting's variants */
    const int n = (int)G.slot.size();
    G.varHas.assign((size_t)n, 0);
    G.varSlot.assign((size_t)n * TS_VARS, -1);
    for (int s = 0; s < n; s++) {
        if (!G.slot[s].var) continue;
        const std::string base = ra3d_key(G.names[G.slot[s].name].c_str(), G.slot[s].icon, 0);
        std::unordered_map<std::string, int>::const_iterator it = G.slotOf.find(base);
        if (it == G.slotOf.end()) continue;
        G.varHas[it->second] |= (unsigned char)(1 << G.slot[s].var);
        G.varSlot[(size_t)it->second * TS_VARS + G.slot[s].var] = s;
    }

    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    G.cols = (int)ceil(sqrt((double)n));
    if (G.cols < 1) G.cols = 1;
    G.w = G.cols * G.pitch;
    G.h = ((n + G.cols - 1) / G.cols) * G.pitch;
    if (maxTex > 0 && (G.w > maxTex || G.h > maxTex)) {
        fprintf(stderr, "RA3D|enh|a %dx%d sheet is over this GL's %d limit; nothing drawn\n",
                G.w, G.h, (int)maxTex);
        return false;
    }
    std::vector<unsigned char> sheet((size_t)G.w * G.h * 4, 0);
    std::vector<unsigned char> rgb((size_t)G.ts * G.ts * 3), tile(24 * 24 * 3);
    std::vector<unsigned char> alpha((size_t)G.ts * G.ts);
    G.avg.assign((size_t)n * 3, 0);
    G.wet.assign((size_t)n, 0);
    /* THE CUT ATLAS for the sea's field (water_mod.h): every slot's 1995 cut at 24 texels,
       in the sheet's own layout at 1/k of its size, 255 land and 0 water */
    WaterRaSource& WR = g_waterRa;
    WR.cutW = G.w / k; WR.cutH = G.h / k;
    WR.cut.assign((size_t)WR.cutW * WR.cutH, 255);
    unsigned char cut[576];
    for (int s = 0; s < n; s++) {
        const Ra3dSlot& sl = G.slot[s];
        const char* name = G.names[sl.name].c_str();
        const unsigned char* a = NULL;
        if (ra3d_water_cut(th, name, sl.icon, cut) > 0) {
            G.wet[s] = 1;
            G.wetSlots++;
            ra3d_water_alpha(cut, G.ts, &alpha[0]);
            a = &alpha[0];
            const int cx0 = (s % G.cols) * (G.pitch / k) + 1, cy0 = (s / G.cols) * (G.pitch / k) + 1;
            for (int y = -1; y <= 24; y++)
                for (int x = -1; x <= 24; x++) {
                    const int sy = y < 0 ? 0 : y > 23 ? 23 : y, sx = x < 0 ? 0 : x > 23 ? 23 : x;
                    WR.cut[(size_t)(cy0 + y) * WR.cutW + cx0 + x] = cut[sy * 24 + sx] ? 0 : 255;
                }
        }
        if (pixels[s]) {
            ra3d_place(sheet, s, pixels[s], a);
            ra3d_mean(pixels[s], G.ts, &G.avg[(size_t)s * 3]);
            G.fromPack++;
            continue;
        }
        if (!ra3d_tile_rgb(th, name, sl.icon, &tile[0])
            && !ra3d_tile_rgb(th, "CLEAR1", 0, &tile[0])) {   /* Classic's own stand-in */
            G.missing++;
            continue;
        }
        if (G.ts == 24) memcpy(&rgb[0], &tile[0], tile.size());
        else rt_bilinear_rgb(&tile[0], 24, 24, 24 * 3, &rgb[0], G.ts, G.ts);
        ra3d_place(sheet, s, &rgb[0], a);
        ra3d_mean(&rgb[0], G.ts, &G.avg[(size_t)s * 3]);
        G.fromTiles++;
    }
    glGenTextures(1, &G.tex);
    glBindTexture(GL_TEXTURE_2D, G.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, fx_filter_mode(GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, fx_filter_mode(GL_NEAREST));
    fx_filter_note(G.tex, GL_NEAREST);   /* the bilinear toggle has to reach it too */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* RGBA: the alpha is the water cut, solid everywhere but the sea's slots. Drawn
       without the sea the alpha is never read and the picture is the RGB it always was. */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, G.w, G.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, &sheet[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
    G.ok = true;
    ra3d_water_source(pixels);
    fprintf(stderr, "RA3D|enh|sheet|%s|%s|%s|%dx%d|cell=%d|slots=%d|pack=%d|variants=%d|"
                    "tiles=%d|missing=%d|%.0f ms\n",
            g_raStaticScene.c_str(), ra3d_theater_name(th),
            G.set == FX_TEX_ENH ? "enhanced" : "1995", G.w, G.h, G.ts, n, G.fromPack,
            G.variants, G.fromTiles, G.missing,
            (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency());
    if (G.set == FX_TEX_ENH) ra3d_seams_load();
    return true;
}

/* The painting map cell i draws: its first painting, or a variant by placement. */
static int ra3d_cell_slot(int i, int x, int y)
{
    const int base = g_ra3d.cellBase[i];
    if (base < 0 || !g_ra3d.varHas[base] || enh_variants_off()) return base;
    const Ra3dSlot& s = g_ra3d.slot[base];
    const int v = enh_variant_pick(x, y, s.icon, s.w, s.h, g_ra3d.varHas[base]);
    const int vs = v ? g_ra3d.varSlot[(size_t)base * TS_VARS + v] : -1;
    return vs >= 0 ? vs : base;
}

static void ra3d_slot_uv(int slot, float* u0, float* v0, float* u1, float* v1)
{
    const Ra3dGround& G = g_ra3d;
    const int x0 = (slot % G.cols) * G.pitch + G.gutter, y0 = (slot / G.cols) * G.pitch + G.gutter;
    *u0 = (float)x0 / (float)G.w;        *v0 = (float)y0 / (float)G.h;
    *u1 = (float)(x0 + G.ts) / (float)G.w; *v1 = (float)(y0 + G.ts) / (float)G.h;
}

/* THE JOIN BLEND'S STRIPS for the theater: the same arrangement terrain_seams_ensure
   makes for Tiberian Dawn, keyed by Red Alert slot (first painting) times TS_VARS plus
   the variant, and by template name for "is this a cliff". OPENCNC3D_TERRAINSEAM=off turns
   both games' strips off; OPENCNC3D_RATERRAINSEAM names Red Alert's pack outright. */
static void ra3d_seams_load(void)
{
    g_ra3dSeamsTried = true;
    const char* td = getenv("OPENCNC3D_TERRAINSEAM");
    if (td && (!strcmp(td, "off") || !strcmp(td, "none"))) {
        fprintf(stderr, "RA3D|seams|off\n");
        return;
    }
    char buf[256];
    const char* path = ra3d_pack_path("OPENCNC3D_RATERRAINSEAM", "terrainseam", g_ra3d.theater, buf, sizeof buf);
    if (!path) { fprintf(stderr, "RA3D|seams|no pack for %s\n", ra3d_theater_name(g_ra3d.theater)); return; }
    std::vector<unsigned char> d = ra3d_read_file(path);
    if (d.size() < 24 || memcmp(&d[0], RA3D_SEAM_MAGIC, 8) != 0 || rt_le32(&d[8]) != 1) {
        fprintf(stderr, "RA3D|seams|%s is not a version 1 Red Alert seam pack\n", path);
        return;
    }
    const unsigned C = rt_le32(&d[12]), M = rt_le32(&d[16]), cnt = rt_le32(&d[20]);
    const size_t keys = 24, body = keys + (size_t)cnt * 24;
    if (!C || !M || M >= C || d.size() != body + (size_t)cnt * C * M * 4) {
        fprintf(stderr, "RA3D|seams|%s has the wrong size\n", path);
        return;
    }
    TerrainSeams& S = g_ra3dSeams;
    const size_t nv = g_ra3d.slot.size() * TS_VARS;
    S.slotL.assign(nv, -1); S.slotT.assign(nv, -1);
    S.featL.assign(nv, 0); S.featT.assign(nv, 0);
    S.cliffTmpl.assign(65536, 0);
    std::vector<const unsigned char*> L, T;
    int unused = 0;
    for (unsigned k = 0; k < cnt; k++) {
        const unsigned char* e = &d[keys + (size_t)k * 24];
        char name[17];
        memcpy(name, e, 16); name[16] = 0;
        const int icon = e[16] | (e[17] << 8), side = e[18], feat = e[19] == 1, var = e[20];
        const unsigned char* px = &d[body + (size_t)k * C * M * 4];
        if (!feat) {
            const int id = ra3d_name_id(name);
            if (id < 65536) S.cliffTmpl[id] = 1;
        }
        std::unordered_map<std::string, int>::const_iterator it = g_ra3d.slotOf.find(ra3d_key(name, icon, 0));
        if (it == g_ra3d.slotOf.end() || var >= TS_VARS) { unused++; continue; }
        const size_t slot = (size_t)it->second * TS_VARS + var;
        if (side == 0) { S.slotL[slot] = (int)L.size(); S.featL[slot] = feat; L.push_back(px); }
        else           { S.slotT[slot] = (int)T.size(); S.featT[slot] = feat; T.push_back(px); }
    }
    S.cell = (int)C; S.margin = (int)M;
    if (!L.empty()) S.texL = ts_sheet(L, (int)M, (int)C, &S.wL, &S.hL, &S.colsL);
    if (!T.empty()) S.texT = ts_sheet(T, (int)C, (int)M, &S.wT, &S.hT, &S.colsT);
    S.ok = S.texL || S.texT;
    S.tried = true;
    fprintf(stderr, "RA3D|seams|%s|%zu left|%zu top|%d unmatched|margin %u at %u a cell\n",
            path, L.size(), T.size(), unused, M, C);
}

/* A corner's light before the shroud: the console's shade off the scene's heights, or
   the level ground's 160/255 where the ground is flat. */
static float ra3d_corner_light(int x, int z)
{
    if (ra3d_flat_ground() || g_pack.corner.empty() || !g_shadeReady) return 160.0f / 255.0f;
    if (x < 0) x = 0; else if (x > g_gridW) x = g_gridW;
    if (z < 0) z = 0; else if (z > g_gridH) z = g_gridH;
    return (float)g_shadeGrid[z * (g_gridW + 1) + x] * (1.0f / 255.0f);
}

/* THE GROUND: one textured cell per map cell, two triangles split SW-NE as Tiberian
   Dawn's ground is, on the scene's heights, lit with the corner shade times the shroud.
   With `sea` the sea is drawn under it: the sheet's alpha (the water cut with its soft
   ramp) is blended over it, as Tiberian Dawn's terrain is over its sea; without, the
   alpha is not read and the ground is opaque. */
static int g_ra3dCellsDrawn = 0;
static void ra3d_draw_ground(bool sea)
{
    g_ra3dCellsDrawn = 0;
    const Ra3dGround& G = g_ra3d;
    const int W = g_raStaticMap.MapCellWidth, H = g_raStaticMap.MapCellHeight;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, G.tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (sea) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.02f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);
    }
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < W * H; i++) {
        const int cx = g_raStaticMap.MapCellX + i % W, cy = g_raStaticMap.MapCellY + i / W;
        if (!cell_shown(cx, cy) || !cell_in_view(cx, cy)) continue;
        const int s = ra3d_cell_slot(i, cx, cy);
        if (s < 0) continue;
        float u0, v0, u1, v1;
        ra3d_slot_uv(s, &u0, &v0, &u1, &v1);
        g_ra3dCellsDrawn++;
        const float x = (float)cx, z = (float)cy;
        const float s00 = ra3d_corner_light(cx, cy) * shroud_corner_vis(cx, cy);
        const float s10 = ra3d_corner_light(cx + 1, cy) * shroud_corner_vis(cx + 1, cy);
        const float s11 = ra3d_corner_light(cx + 1, cy + 1) * shroud_corner_vis(cx + 1, cy + 1);
        const float s01 = ra3d_corner_light(cx, cy + 1) * shroud_corner_vis(cx, cy + 1);
        const float y00 = terrain_corner_y(cx, cy), y10 = terrain_corner_y(cx + 1, cy);
        const float y11 = terrain_corner_y(cx + 1, cy + 1), y01 = terrain_corner_y(cx, cy + 1);
        glColor3f(s00, s00, s00); glTexCoord2f(u0, v0); glVertex3f(x,        y00, z);
        glColor3f(s01, s01, s01); glTexCoord2f(u0, v1); glVertex3f(x,        y01, z + 1.0f);
        glColor3f(s10, s10, s10); glTexCoord2f(u1, v0); glVertex3f(x + 1.0f, y10, z);
        glColor3f(s10, s10, s10); glTexCoord2f(u1, v0); glVertex3f(x + 1.0f, y10, z);
        glColor3f(s01, s01, s01); glTexCoord2f(u0, v1); glVertex3f(x,        y01, z + 1.0f);
        glColor3f(s11, s11, s11); glTexCoord2f(u1, v1); glVertex3f(x + 1.0f, y11, z + 1.0f);
    }
    glEnd();
    if (sea) {
        glDisable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.5f);
    }
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/* THE SEA FLOOR: the holed cells drawn opaque from the same sheet, before the sea, so
   what the surface lets through and bends is Red Alert's own painted water (and the
   painted shore under the shallows). Tiberian Dawn's floor is the cartridge's BOTTOM
   tile, a sandy bed that turned Red Alert's blue to a milky green under the wash. Same
   corners and diagonal as the ground, so the ground drawn over it meets it exactly. */
static void ra3d_draw_floor(void)
{
    const Ra3dGround& G = g_ra3d;
    const std::vector<PackCell>& cells = g_waterRa.cell;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, G.tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < cells.size(); i++) {
        const PackCell& c = cells[i];
        if (!c.holes || !cell_shown(c.x, c.y) || !cell_in_view(c.x, c.y)) continue;
        const int cx = c.x, cy = c.y;
        const float x = (float)cx, z = (float)cy;
        const float s00 = ra3d_corner_light(cx, cy) * shroud_corner_vis(cx, cy);
        const float s10 = ra3d_corner_light(cx + 1, cy) * shroud_corner_vis(cx + 1, cy);
        const float s11 = ra3d_corner_light(cx + 1, cy + 1) * shroud_corner_vis(cx + 1, cy + 1);
        const float s01 = ra3d_corner_light(cx, cy + 1) * shroud_corner_vis(cx, cy + 1);
        const float y00 = terrain_corner_y(cx, cy), y10 = terrain_corner_y(cx + 1, cy);
        const float y11 = terrain_corner_y(cx + 1, cy + 1), y01 = terrain_corner_y(cx, cy + 1);
        glColor3f(s00, s00, s00); glTexCoord2f(c.u0, c.v0); glVertex3f(x,        y00, z);
        glColor3f(s01, s01, s01); glTexCoord2f(c.u0, c.v1); glVertex3f(x,        y01, z + 1.0f);
        glColor3f(s10, s10, s10); glTexCoord2f(c.u1, c.v0); glVertex3f(x + 1.0f, y10, z);
        glColor3f(s10, s10, s10); glTexCoord2f(c.u1, c.v0); glVertex3f(x + 1.0f, y10, z);
        glColor3f(s01, s01, s01); glTexCoord2f(c.u0, c.v1); glVertex3f(x,        y01, z + 1.0f);
        glColor3f(s11, s11, s11); glTexCoord2f(c.u1, c.v1); glVertex3f(x + 1.0f, y11, z + 1.0f);
    }
    glEnd();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/* THE GROUND IN THE SEA'S MIRROR (water_mod.h's reflection, and the casters' reflection
   pass): cut where the sea is, so the mirror shows the sky there, as Tiberian Dawn's
   terrain does in its own. */
static void ra3d_reflect_ground(void)
{
    if (!g_ra3d.ok) return;
    ra3d_draw_ground(water_fx_wanted());
}

/* THE JOIN BLEND over the ground, by Tiberian Dawn's rules and drawing code. */
static int g_ra3dStripsDrawn = 0;
static void ra3d_draw_seams(void)
{
    g_ra3dStripsDrawn = 0;
    const TerrainSeams& S = g_ra3dSeams;
    const Ra3dGround& G = g_ra3d;
    if (!S.ok || G.set != FX_TEX_ENH) return;
    const int W = g_raStaticMap.MapCellWidth, H = g_raStaticMap.MapCellHeight;
    const int X0 = g_raStaticMap.MapCellX, Y0 = g_raStaticMap.MapCellY;
    static const unsigned char notint[4][3] = { {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0} };
    ts_strips_begin(false, G.tex);
    for (int pass = 0; pass < 2; pass++) {
        TsPass P;
        if (!ts_pass(S, pass, &P)) continue;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, P.tex);
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < W * H; i++) {
            const int ax = i % W, ay = i / W, cx = X0 + ax, cy = Y0 + ay;
            if (!cell_shown(cx, cy) || !cell_in_view(cx, cy)) continue;
            const int sa = G.cellBase[i];
            if (sa < 0 || G.slot[sa].name == G.clearName) continue;          /* clear: nothing */
            const int bx = ax + (pass ? 0 : 1), by = ay + (pass ? 1 : 0);
            if (bx >= W || by >= H) continue;
            const int bi = by * W + bx;
            if (!cell_shown(X0 + bx, Y0 + by)) continue;
            const int sb = G.cellBase[bi];
            if (sb < 0 || G.slot[sb].name == G.clearName) continue;
            /* the strips of the painting that cell draws */
            const int drawn = ra3d_cell_slot(bi, X0 + bx, Y0 + by);
            const int sbv = sb * TS_VARS + (drawn == sb ? 0 : G.slot[drawn].var);
            const int k = (*P.idx)[sbv];
            if (k < 0) continue;
            if ((*P.feat)[sbv] && S.cliffTmpl[G.slot[sa].name & 0xFFFF]) continue;   /* never over a cliff */
            float s[4];
            const int xs[4] = { cx, cx + 1, cx + 1, cx }, zs[4] = { cy, cy, cy + 1, cy + 1 };
            for (int q = 0; q < 4; q++) s[q] = ra3d_corner_light(xs[q], zs[q]) * shroud_corner_vis(xs[q], zs[q]);
            float u0, v0, u1, v1;
            ra3d_slot_uv(ra3d_cell_slot(i, cx, cy), &u0, &v0, &u1, &v1);
            ts_strip_emit(S, pass, P, k, cx, cy, u0, u1, v0, v1, 1.0f, 1.0f, s, notint, false);
            g_ra3dStripsDrawn++;
        }
        glEnd();
    }
    ts_strips_end(false, false, 0, G.tex);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* The ground's normal for the post chain's sun: the scene's corner normal (g_normGrid,
   terrain_shade_build's encoding), straight up where the ground is flat, over the same
   cells the ground draws. */
static void ra3d_ground_normals(void)
{
    if (!g_ra3d.ok) return;
    const int W = g_raStaticMap.MapCellWidth, H = g_raStaticMap.MapCellHeight;
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    const bool level = ra3d_flat_ground() || g_pack.corner.empty() || !g_shadeReady;
    glColor3ub(128, 255, 128);
    auto corner = [&](int cx, int cz) {
        if (!level) {
            const int x = cx < 0 ? 0 : cx > g_gridW ? g_gridW : cx;
            const int z = cz < 0 ? 0 : cz > g_gridH ? g_gridH : cz;
            glColor3ubv(g_normGrid[z * (g_gridW + 1) + x]);
        }
        glVertex3f((float)cx, terrain_corner_y(cx, cz), (float)cz);
    };
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < W * H; i++) {
        const int cx = g_raStaticMap.MapCellX + i % W, cy = g_raStaticMap.MapCellY + i / W;
        if (!cell_shown(cx, cy) || !cell_in_view(cx, cy)) continue;
        corner(cx, cy);     corner(cx, cy + 1); corner(cx + 1, cy);
        corner(cx + 1, cy); corner(cx, cy + 1); corner(cx + 1, cy + 1);
    }
    glEnd();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/* THE GROUND PASS, in draw_frame's place for the sea, the terrain and the coast: the
   sheet built (or found built), the cells, then the strips. Returns false with nothing
   drawn when the map or the 1995 tiles cannot be read. */
static bool ra3d_world_ground(void)
{
    if (!ra3d_ground_ensure()) return false;
    static std::string reported;
    if (reported != g_ra3d.key) {
        /* heights=scene: laid on the mission's own scene's heightmap; flat: at 0 */
        fprintf(stderr, "RA3D|scene|ground=red alert|theater=%s|rect=%d,%d %dx%d|set=%s|heights=%s\n",
                ra3d_theater_name(g_ra3d.theater), g_raStaticMap.MapCellX, g_raStaticMap.MapCellY,
                g_raStaticMap.MapCellWidth, g_raStaticMap.MapCellHeight,
                g_ra3d.set == FX_TEX_ENH ? "enhanced" : "1995",
                ra3d_flat_ground() ? "flat" : "scene");
        reported = g_ra3d.key;
    }
    /* THE SEA (water_mod.h, on this map's source): the painted floor, the surface,
       then the ground cut and blended over both, then the wash and foam over the coast.
       If the surface cannot draw, the ground stays opaque and the painted water shows. */
    bool sea = false;
    if (water_fx_wanted()) {
        ra3d_draw_floor();
        sea = water_fx_draw();
    }
    ra3d_draw_ground(sea);
    ra3d_draw_seams();
    if (sea) water_shore_draw();
    {
        static int lastSea = -1;
        if ((int)sea != lastSea) {
            fprintf(stderr, "WATER|ra|frame|%s|sea=%d|field=%d|wet_texels=%d\n",
                    g_raStaticScene.c_str(), (int)sea, g_waterFieldTex ? 1 : 0, g_waterFieldWet);
            lastSea = (int)sea;
        }
    }
    /* OPENCNC3D_RA3DFRAME=1: what the frame laid, each time that changes (the gate's evidence
       that the ground and the strips are drawn, not only loaded) */
    static int lastCells = -1, lastStrips = -1;
    if ((g_ra3dCellsDrawn != lastCells || g_ra3dStripsDrawn != lastStrips)
        && getenv("OPENCNC3D_RA3DFRAME")) {
        fprintf(stderr, "RA3D|frame|%s|cells=%d|strips=%d|cam=%.1f,%.1f\n",
                g_raStaticScene.c_str(), g_ra3dCellsDrawn, g_ra3dStripsDrawn, g_camX, g_camZ);
        lastCells = g_ra3dCellsDrawn; lastStrips = g_ra3dStripsDrawn;
    }
    return true;
}

/* THE RADAR'S GROUND, from the same map and sheet: one entry per map cell, its painting's
   mean colour (radar_plot_into asks while this ground is the one drawn). */
static bool ra3d_radar_on(void) { return ra3d_own_ground() && g_ra3d.ok && !g_ra3d.cellBase.empty(); }
static int ra3d_radar_count(void) { return (int)g_ra3d.cellBase.size(); }
static void ra3d_radar_cell(int i, int* x, int* y, unsigned char rgb[3])
{
    const int W = g_raStaticMap.MapCellWidth > 0 ? g_raStaticMap.MapCellWidth : 1;
    *x = g_raStaticMap.MapCellX + i % W;
    *y = g_raStaticMap.MapCellY + i / W;
    const int s = g_ra3d.cellBase[i];
    if (s < 0) { rgb[0] = rgb[1] = rgb[2] = 0; return; }
    memcpy(rgb, &g_ra3d.avg[(size_t)s * 3], 3);
}

#endif /* OPENCNC3D_RA3D_TERRAIN_H */
