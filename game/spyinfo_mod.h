/* ---- WHAT A SPY SHOWS (Red Alert) ----------------------------------------------------
 *
 * A Red Alert Spy that walks into an enemy building changes the game in a few places the
 * engine handles by itself (a Radar Dome shares the owner's sight, a Sub Pen gives a sonar
 * pulse, an Airfield parabombs), but most of what it buys is INFORMATION, drawn over the
 * spied building whenever the player selects it. None of that was drawn, so a spy that
 * entered a refinery, a factory or a power plant seemed to do nothing at all. These are the
 * 1996 rules, each with the place in the engine that draws it:
 *
 *   - THE FACTORY'S PRODUCT. BuildingClass::Draw_It (redalert/building.cpp) draws the cameo
 *     of the object a spied factory is building, centred on the building, over everything
 *     else it draws. The brain names the object (spyprod=), found the way Draw_It finds it.
 *   - THE OWNER'S MONEY. TechnoClass::Draw_Pips (redalert/techno.cpp) prints the owner's
 *     Available_Money over a spied building with storage, a refinery or a silo, in PIPS.SHP
 *     digits six pixels apart, the last digit first. Spying one storage building spies them
 *     all (BuildingClass::Spied_By), which the brain's spied= already includes. A house with
 *     no money prints nothing, as 1996's loop prints nothing for zero.
 *   - THE OWNER'S POWER. Draw_Pips replaces a power plant's pip row with a column of five
 *     pips, green while the owner has enough power, gold when short, red when the drain is
 *     above twice the output, for any power plant the player does not own but is allowed
 *     the pips of: an ally's, or a spied enemy's (powpip=).
 *   - "DECOY". Draw_Pips writes PIP_DECOY over a fake structure that is spied or is the
 *     player's own.
 *
 * The pips, the digits and the word all come out of Draw_Pips, which TechnoClass::Draw_It
 * calls for a SELECTED object only, at the pip origin five pixels in from the selection
 * box's left edge and three up from its bottom. The cameo too is drawn for a selected
 * factory only. So nothing here is drawn for a building the player has not selected.
 *
 * THE ART IS THE 1996 SHAPES, out of the sidebar pack the Red Alert HUD loads: PIPS.SHP
 * frames for the pips, digits and word, and the sidebar cameo (rah_cameo_plain) for the
 * product. IN CLASSIC dos2d_selection draws them at the 1996 offsets inside the 1996 box.
 * IN THE 3D MODES the anchor is the selection bracket's bottom-left corner as it lands on
 * screen and the building's drawn centre, in screen space below the post-process seam, at
 * the zoom the control-group digit uses, for the reasons that digit gives.
 *
 * The radar's spy report (a spied Radar Dome lets the 1996 radar button cycle to a page of
 * the owner's building, unit and infantry counts) is not drawn here.
 * ------------------------------------------------------------------------------------- */
#define SPY_PIP_FULL      1      /* PipEnum, redalert/defines.h */
#define SPY_PIP_ENGINEER  5
#define SPY_PIP_COMMANDO  7
#define SPY_PIP_DIGIT0    8
#define SPY_PIP_DECOY     18
#define SPY_ATLAS_W       1024
#define SPY_ATLAS_H       32
static unsigned char        g_spyPx[SPY_ATLAS_W * SPY_ATLAS_H];
static unsigned char        g_spyRGBA[SPY_ATLAS_W * SPY_ATLAS_H * 4];
static GLuint               g_spyTex  = 0;
static const unsigned char* g_spyFrom = NULL;
static int                  g_spyW = 0, g_spyH = 0, g_spyFrames = 0;
/* `spyinfo 0` leaves all of it out, Classic and 3D alike: a gate's control leg. */
static bool                 g_spyOn = true;

/* Every PIPS frame up to the decoy word, side by side, cut again when the pack changes. */
static bool spy_atlas(void)
{
    if (!g_raModule || !g_dbPack) return false;
    const DB_Shape* pips = db_shape(g_dbPack, "PIPS");
    if (!pips || pips->frames <= SPY_PIP_DECOY || pips->w <= 0 || pips->h <= 0) return false;
    const int frames = SPY_PIP_DECOY + 1;
    if (pips->w * frames > SPY_ATLAS_W || pips->h > SPY_ATLAS_H) return false;
    if (g_spyTex && g_spyFrom == pips->pixels) return true;
    DB_Surface surf;
    db_surface_init(&surf, SPY_ATLAS_W, SPY_ATLAS_H, g_spyPx);
    for (int f = 0; f < frames; f++)
        db_draw_shape(&surf, pips, f, f * pips->w, 0);
    db_surface_to_rgba(&surf, g_dbPack->pal8, g_spyRGBA, 1);
    if (!g_spyTex) {
        glGenTextures(1, &g_spyTex);
        glBindTexture(GL_TEXTURE_2D, g_spyTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, g_spyTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SPY_ATLAS_W, SPY_ATLAS_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, g_spyRGBA);
    g_spyFrom = pips->pixels;
    g_spyW = pips->w;
    g_spyH = pips->h;
    g_spyFrames = frames;
    return true;
}

/* The cameo of a type, 64x48 in a 64x64 texture, uploaded once per name. 0 for a name the
   packs have no cameo for, which then draws nothing. */
static GLuint spy_cameo_tex(const char* name)
{
    static std::map<std::string, GLuint> cache;
    if (!name || !name[0]) return 0;
    std::map<std::string, GLuint>::iterator it = cache.find(name);
    if (it != cache.end()) return it->second;
    const char* art = sb_art_name(name);
    const bool have = (g_ra95Pack && db_shape(g_ra95Pack, art)) || (g_dbPack && db_shape(g_dbPack, art));
    GLuint tex = 0;
    if (have) {
        std::vector<unsigned char> pot((size_t)64 * 64 * 4, 0);
        memcpy(&pot[0], rah_cameo_plain(name), (size_t)64 * 48 * 4);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pot[0]);
    }
    cache[name] = tex;
    return tex;
}

/* One thing drawn: a PIPS frame (frame >= 0) or the product's cameo (frame -1). */
struct SpyQuad {
    int   frame;
    float x0, y0, x1, y1;       /* screen pixels, y down */
};

/* One building's worth, for spydump: what was drawn and with which numbers. */
struct SpyRec {
    char  type[16];
    int   id, spied, fake;
    char  prod[16];             /* the cameo drawn, "" for none   */
    char  money[16];            /* the digits drawn, "" for none  */
    int   powpips, powframe;    /* pips drawn and their PIPS frame */
    int   decoy;
    float x0, y0, x1, y1;       /* the union of everything drawn  */
    bool  classic;
};
static std::vector<SpyRec> g_spyRecs;   /* the last frame's, Classic or 3D */

/* Lay out everything one selected building shows, as 1996 does, from the pip origin
   (px, py) and the building's centre (cx, cy), in screen pixels, at sc screen pixels per
   1996 pixel. Pips first, then the cameo, which Draw_It draws last. Returns false when
   there is nothing to draw. */
static bool spy_layout(const SimObject& o, float px, float py, float cx, float cy, float sc,
                       std::vector<SpyQuad>& qs, SpyRec& rec)
{
    memset(&rec, 0, sizeof rec);
    snprintf(rec.type, sizeof rec.type, "%s", o.type);
    rec.id = o.id; rec.spied = o.spied; rec.fake = o.fake;
    const size_t first = qs.size();
    const float fw = (float)g_spyW * sc, fh = (float)g_spyH * sc;
    /* SHAPE_CENTER puts the frame's middle on the point, by whole 1996 pixels */
    const float hw = (float)(g_spyW / 2) * sc, hh = (float)(g_spyH / 2) * sc;
    if (o.powpip >= 0 && o.powpip <= 2) {
        const int frame = o.powpip == 0 ? SPY_PIP_FULL : o.powpip == 1 ? SPY_PIP_ENGINEER
                        : SPY_PIP_COMMANDO;
        for (int i = 0; i < 5; i++) {
            SpyQuad q; q.frame = frame;
            q.x0 = px - hw; q.y0 = py - (float)(i * 3) * sc - hh;
            q.x1 = q.x0 + fw; q.y1 = q.y0 + fh;
            qs.push_back(q);
        }
        rec.powpips = 5; rec.powframe = frame;
    }
    if (o.fake && (o.spied || is_players(o))) {
        /* the one shape Draw_Pips places by its corner rather than its middle */
        SpyQuad q; q.frame = SPY_PIP_DECOY;
        q.x0 = px; q.y0 = py - 16.0f * sc;
        q.x1 = q.x0 + fw; q.y1 = q.y0 + fh;
        qs.push_back(q);
        rec.decoy = 1;
    }
    if (o.spied && o.spymoney > 0) {
        int money = o.spymoney, digits, factor = 10;
        for (digits = 1; digits < 9; digits++) {
            if (money < factor) break;
            factor *= 10;
        }
        char text[16];
        snprintf(text, sizeof text, "%d", money);
        snprintf(rec.money, sizeof rec.money, "%s", text);
        float sx = px + (float)(6 * digits - 3) * sc;
        while (money) {
            SpyQuad q; q.frame = SPY_PIP_DIGIT0 + money % 10;
            money /= 10;
            q.x0 = sx - hw; q.y0 = py - 6.0f * sc - hh;
            q.x1 = q.x0 + fw; q.y1 = q.y0 + fh;
            qs.push_back(q);
            sx -= 6.0f * sc;
        }
    }
    if (o.spied && o.spyprod[0] && spy_cameo_tex(o.spyprod)) {
        SpyQuad q; q.frame = -1;
        q.x0 = cx - 32.0f * sc; q.y0 = cy - 24.0f * sc;
        q.x1 = q.x0 + 64.0f * sc; q.y1 = q.y0 + 48.0f * sc;
        qs.push_back(q);
        snprintf(rec.prod, sizeof rec.prod, "%s", o.spyprod);
    }
    if (qs.size() == first) return false;
    rec.x0 = rec.y0 = 1e9f; rec.x1 = rec.y1 = -1e9f;
    for (size_t i = first; i < qs.size(); i++) {
        rec.x0 = fminf(rec.x0, qs[i].x0); rec.y0 = fminf(rec.y0, qs[i].y0);
        rec.x1 = fmaxf(rec.x1, qs[i].x1); rec.y1 = fmaxf(rec.y1, qs[i].y1);
    }
    return true;
}

/* Draw laid-out quads. The caller has a 2D pixel projection, texturing on and an alpha
   test (or nothing that would hide a hard 0/255 alpha) in force. */
static void spy_draw_quads(const std::vector<SpyQuad>& qs, const char* prod)
{
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    const float v1 = (float)g_spyH / (float)SPY_ATLAS_H;
    glBindTexture(GL_TEXTURE_2D, g_spyTex);
    glBegin(GL_QUADS);
    for (size_t i = 0; i < qs.size(); i++) {
        const SpyQuad& q = qs[i];
        if (q.frame < 0) continue;
        const float u0 = (float)(q.frame * g_spyW) / (float)SPY_ATLAS_W;
        const float u1 = (float)((q.frame + 1) * g_spyW) / (float)SPY_ATLAS_W;
        glTexCoord2f(u0, 0.0f); glVertex2f(q.x0, q.y0);
        glTexCoord2f(u1, 0.0f); glVertex2f(q.x1, q.y0);
        glTexCoord2f(u1, v1);   glVertex2f(q.x1, q.y1);
        glTexCoord2f(u0, v1);   glVertex2f(q.x0, q.y1);
    }
    glEnd();
    for (size_t i = 0; i < qs.size(); i++) {
        const SpyQuad& q = qs[i];
        if (q.frame >= 0) continue;
        glBindTexture(GL_TEXTURE_2D, spy_cameo_tex(prod));
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);  glVertex2f(q.x0, q.y0);
        glTexCoord2f(1.0f, 0.0f);  glVertex2f(q.x1, q.y0);
        glTexCoord2f(1.0f, 0.75f); glVertex2f(q.x1, q.y1);
        glTexCoord2f(0.0f, 0.75f); glVertex2f(q.x0, q.y1);
        glEnd();
    }
}

/* Could this building show anything at all? A cheap test before any layout. */
static bool spy_candidate(const SimObject& o)
{
    if (o.kind != K_BUILDING) return false;
    return o.powpip >= 0 || (o.fake && (o.spied || is_players(o)))
        || (o.spied && (o.spymoney > 0 || o.spyprod[0]));
}

/* The building record behind a Classic layer object, by heap slot. */
static const SimObject* spy_building(int id)
{
    for (size_t i = 0; i < g_objects.size(); i++)
        if (g_objects[i].kind == K_BUILDING && g_objects[i].id == id) return &g_objects[i];
    return NULL;
}

/* CLASSIC: called by dos2d_selection for a selected building, with the 1996 pip origin and
   centre in 1996 pixels of the view and the view's own origin and scale. */
static void spy_draw_classic(int id, int pipx, int pipy, int cx, int cy,
                             float ox, float oy, int scale)
{
    if (!g_spyOn || !spy_atlas()) return;
    const SimObject* o = spy_building(id);
    if (!o || !spy_candidate(*o)) return;
    std::vector<SpyQuad> qs;
    SpyRec rec;
    const float sc = (float)scale;
    if (!spy_layout(*o, ox + (float)pipx * sc, oy + (float)pipy * sc,
                    ox + (float)cx * sc, oy + (float)cy * sc, sc, qs, rec)) return;
    spy_draw_quads(qs, o->spyprod);
    rec.classic = true;
    if (g_spyRecs.size() < 256) g_spyRecs.push_back(rec);
}

/* THE 3D MODES: every selected building with something to show, laid out on the bracket's
   bottom-left corner and the building's drawn centre. Shared by the draw pass and
   spydump, so the dump can never claim what the frame did not draw. */
struct Spy3D {
    size_t               idx;   /* into g_objects */
    SpyRec               rec;
    std::vector<SpyQuad> qs;
};

static std::vector<Spy3D> collect_spy_3d(int fbw, int fbh)
{
    std::vector<Spy3D> out;
    if (!g_spyOn || g_editOn || g_selected.empty()) return out;
    if (!spy_atlas()) return out;
    sb_layout(fbw, fbh);
    /* the control-group digit's zoom (collect_group_digits) */
    const int   hp    = g_fxHidpi > 1 ? g_fxHidpi : 1;
    const int   dosz  = (fbh / hp) / 200 < 1 ? 1 : (fbh / hp) / 200;
    const float sc    = fmaxf(sb_dos_px_half(), (float)((dosz + 1) / 2 * hp));
    const float right = sb_tactical_right(fbw, fbh);
    for (size_t i = 0; i < g_objects.size(); i++) {
        const SimObject& o = g_objects[i];
        if (!spy_candidate(o)) continue;
        if (!is_selected(o)) continue;              /* Draw_Pips' and Draw_It's rule */
        if (!visible(o)) continue;
        float x0, z0, x1, z1;
        sel_bracket_rect(o, &x0, &z0, &x1, &z1);
        /* THE BRACKET'S OWN BOTTOM-LEFT CORNER, as draw_selection strokes it: of the four
           projected corners, the two lowest on screen are the near edge, and the left one
           of those is the corner. The bounding box of all four (leftmost column, lowest
           row) is not a point of the bracket under perspective: the far edge can project
           further left than the near one, and pips anchored there stood just outside the
           bracket instead of inside it, where Draw_Pips puts them. */
        const float bx[4] = { x0, x1, x1, x0 }, bz[4] = { z0, z0, z1, z1 };
        float pc[4], pr[4];
        bool front = true;
        for (int k = 0; k < 4 && front; k++)
            front = project_point(bx[k], terrain_y(bx[k], bz[k]) + SEL_LIFT, bz[k],
                                  fbw, fbh, &pc[k], &pr[k]);
        if (!front) continue;
        int n0 = 0;                                  /* the lowest corner on screen */
        for (int k = 1; k < 4; k++) if (pr[k] > pr[n0]) n0 = k;
        int n1 = -1;                                 /* and the next lowest */
        for (int k = 0; k < 4; k++)
            if (k != n0 && (n1 < 0 || pr[k] > pr[n1])) n1 = k;
        const int   nl     = pc[n1] < pc[n0] ? n1 : n0;
        const float left   = pc[nl];
        const float bottom = pr[nl];
        const ScreenPoly s = object_screen_poly(o, fbw, fbh, false);
        if (!s.ok) continue;
        Spy3D d;
        d.idx = i;
        if (!spy_layout(o, left + 5.0f * sc, bottom - 3.0f * sc,
                        (s.c0 + s.c1) * 0.5f, (s.r0 + s.r1) * 0.5f, sc, d.qs, d.rec)) continue;
        /* CULL, NEVER CLAMP, the primary label's rule. */
        if (d.rec.x1 <= 0.0f || d.rec.x0 >= right || d.rec.y1 <= 0.0f || d.rec.y0 >= (float)fbh)
            continue;
        out.push_back(d);
    }
    return out;
}

/* Drawn below the seam beside the control-group digit, so it is neither bloomed nor
   graded and the sidebar paints over it. */
static void draw_spy_info(int fbw, int fbh)
{
    const std::vector<Spy3D> ds = collect_spy_3d(fbw, fbh);
    if (!g_fx.dos_classic) {
        g_spyRecs.clear();
        for (size_t i = 0; i < ds.size(); i++) g_spyRecs.push_back(ds[i].rec);
    }
    if (ds.empty()) return;
    begin_overlay(fbw, fbh);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    for (size_t i = 0; i < ds.size(); i++)
        spy_draw_quads(ds[i].qs, g_objects[ds[i].idx].spyprod);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    end_overlay();
}
