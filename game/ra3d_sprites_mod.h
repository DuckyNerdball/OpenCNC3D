/* =====================================================================================
 *  ra3d_sprites_mod.h -- Red Alert in the 3D modes: its own sprites, on the 3D ground.
 *
 *  Tiberian Dawn's 3D modes draw the cartridge's models (and the remastered ones) by
 *  object type. Red Alert has neither, and Tiberian Dawn's look-alikes are the wrong art:
 *  its jeep is not the Ranger and its minigunner is not the Allied rifleman, and every
 *  Red Alert building without a Tiberian Dawn namesake fell back to a pale box. So under
 *  Red Alert, outside Classic, the model passes stand down and every object is drawn as
 *  the very sprite Classic draws -- the engine's own draw record (asset, frame, remap,
 *  sort order, from GAME_STATE_LAYERS and GAME_STATE_DYNAMIC_MAP, exactly what
 *  dos2d_scene reads), stood in the 3D world:
 *
 *    - an object is a card facing the camera (the infantry billboards' own basis,
 *      g_bbRight and g_bbUp), its centre at the object's point on the 3D ground. The 2D
 *      game centres every one of these sprites on the object's coordinate, and the art
 *      is drawn from about the camera's own three-quarter view, so the card reproduces
 *      the Classic picture at the object's place and height on the relief;
 *    - an overlay that lies on the ground (ore, gems, a smudge) is a decal draped over
 *      the terrain's four corner heights; a wall or a fence (the sellable overlays)
 *      stands up like an object;
 *    - the engine's animations (explosions, the move flash, muzzle fire) are cards too,
 *      a ground animation lying flat.
 *
 *  Cards retain the engine's painter order with depth testing against the ground and
 *  imported solid buildings. The shroud follows this pass.
 *
 *  This is a first pass: the sprites keep the 1995 game's pixel art and its eight or
 *  thirty-two facings. 3D models replace them type by type as they are made.
 * ===================================================================================== */
#pragma once

static bool g_ra3dSprites = false;   /* this frame: Red Alert, drawn in a 3D mode */
static int  g_ra3dCards = 0, g_ra3dDecals = 0, g_ra3dMissing = 0;
static int  g_ra3dModelRoots = 0, g_ra3dModelSubs = 0, g_ra3dFallbackBuildings = 0;

static void ra3d_frame_begin(void)
{
    g_ra3dSprites = g_raModule && !g_fx.dos_classic;
    /* The theater letter that picks a theater's art (S/, T/, I/) reads the brain's static
       map, which only Classic's terrain pass fetched: a mission that never drew a Classic
       frame looked up its interior crates as temperate ones. */
    if (g_ra3dSprites) ra2d_fetch_map();
}

/* ONLY A CONVERTED MISSION LEAVES CLASSIC. A Red Alert mission's 3D scene is its own pack
   (ra3d/3d/<SCEN>.pack, theater RASNOW and its kin); a mission not converted yet is handed
   the shared placeholder, which is Tiberian Dawn's ground and not this mission's. Asked
   for N64 or Enhanced there, the mode itself goes back to Classic -- not just the picture,
   because the click mapping, the camera and the HUD all follow the mode. Called at the top
   of every frame, before anything reads it. */
static void ra3d_enforce_scene(void)
{
    static int said = 0;
    if (!g_raModule || g_fx.dos_classic) return;
    if (g_pack.theater[0] == 'R' && g_pack.theater[1] == 'A') return;
    g_fx.dos_classic = 1;
    g_fx.enabled = 0;
    if (!said) {
        fprintf(stderr, "RA3D|visuals|%s has no 3D scene yet; it stays in Classic\n", g_bootScen);
        said = 1;
    }
}

static bool ra3d_ore_off(void)
{
    static int off = -1;
    if (off < 0) { const char* e = getenv("OPENCNC3D_RA3DORE"); off = (e && !strcmp(e, "off")) ? 1 : 0; }
    return off != 0;
}

/* The shape and its frame rectangle, uploaded for this remap/ghost/flash/shadow. */
static Dos2DShape* ra3d_shape(const char* name, int frame, int remap, bool ghost, bool flash,
                              bool shadow, float uv[4])
{
    Dos2DShape* s = dos2d_shape(name);
    if (!s) {
        static std::set<std::string> missing;
        g_ra3dMissing++;
        if (missing.insert(name).second) fprintf(stderr, "RA3D|sprite|missing original SHP %s\n", name);
        return NULL;
    }
    if (!dos2d_upload(s, remap, ghost, flash, shadow, false)) return NULL;
    const int fr = frame < s->frames && frame >= 0 ? frame : 0;
    const int sx = (fr % s->cols) * s->w, sy = (fr / s->cols) * s->h;
    glBindTexture(GL_TEXTURE_2D, s->tex[dos2d_tex_slot(remap, ghost, flash, shadow, false)]);
    uv[0] = (float)sx / s->tw; uv[1] = (float)sy / s->th;
    uv[2] = (float)(sx + s->w) / s->tw; uv[3] = (float)(sy + s->h) / s->th;
    return s;
}

static void ra3d_blend(bool on)
{
    if (on) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glAlphaFunc(GL_GREATER, 0.0f);
    } else {
        glDisable(GL_BLEND); glAlphaFunc(GL_GREATER, 0.5f);
    }
}

/* An upright card: centre (cx, cz) in cells, on the ground there. */
static void ra3d_card(const char* name, int frame, float cx, float cz, int remap = -1,
                      bool ghost = false, bool flash = false, bool shadow = false,
                      float lift = 0.0f)
{
    float uv[4];
    Dos2DShape* s = ra3d_shape(name, frame, remap, ghost, flash, shadow, uv);
    if (!s) return;
    const float hw = s->w / 48.0f, hh = s->h / 48.0f;     /* 24 texels a cell, halved */
    const float gy = terrain_y(cx, cz) + lift;
    const float R[3] = { g_bbRight[0] * hw, g_bbRight[1] * hw, g_bbRight[2] * hw };
    const float U[3] = { g_bbUp[0] * hh, g_bbUp[1] * hh, g_bbUp[2] * hh };
    ra3d_blend(ghost || shadow);
    glBegin(GL_QUADS);
    glTexCoord2f(uv[0], uv[1]); glVertex3f(cx - R[0] + U[0], gy - R[1] + U[1], cz - R[2] + U[2]);
    glTexCoord2f(uv[2], uv[1]); glVertex3f(cx + R[0] + U[0], gy + R[1] + U[1], cz + R[2] + U[2]);
    glTexCoord2f(uv[2], uv[3]); glVertex3f(cx + R[0] - U[0], gy + R[1] - U[1], cz + R[2] - U[2]);
    glTexCoord2f(uv[0], uv[3]); glVertex3f(cx - R[0] - U[0], gy - R[1] - U[1], cz - R[2] - U[2]);
    glEnd();
    if (ghost || shadow) ra3d_blend(false);
    g_ra3dCards++;
}

/* Building SHPs are centred on the ground coordinate. Their lower half must be
   raised above the terrain before depth testing. Use the body's last occupied row
   as the foot, and apply its lift to every subobject so overlays stay registered. */
static float ra3d_building_lift(const Dos2DShape* s, int frame)
{
    if (!s) return 0.0f;
    static std::map<std::string, std::vector<int> > feet;
    std::vector<int>& rows = feet[s->name];
    if (rows.empty()) {
        const size_t size = (size_t)s->w * s->h * s->frames;
        std::vector<unsigned char> indices(size);
        uLongf bytes = (uLongf)size;
        rows.assign(s->frames, s->h - 1);
        if (uncompress(indices.data(), &bytes, s->zipped.data(), (uLong)s->zipped.size()) == Z_OK
            && bytes == size) {
            for (int f = 0; f < s->frames; f++) {
                bool found = false;
                for (int y = s->h - 1; y >= 0 && !found; y--)
                    for (int x = 0; x < s->w; x++)
                        if (indices[((size_t)f * s->h + y) * s->w + x]) {
                            rows[f] = y; found = true; break;
                        }
            }
        }
    }
    const int f = frame >= 0 && frame < s->frames ? frame : 0;
    return fmaxf(0.0f, g_bbUp[1] * ((rows[f] + 1.0f) - s->h * 0.5f) / 24.0f) + 0.02f;
}

/* A decal: the sprite's own rectangle (top-left x0, z0 in cells) laid on the ground,
   each corner at the terrain's height there, lifted a hair so it does not fight it. */
static void ra3d_decal(const char* name, int frame, float x0, float z0, bool ghost = false)
{
    float uv[4];
    Dos2DShape* s = ra3d_shape(name, frame, -1, ghost, false, false, uv);
    if (!s) return;
    const float x1 = x0 + s->w / 24.0f, z1 = z0 + s->h / 24.0f, lift = 0.01f;
    ra3d_blend(ghost);
    glBegin(GL_QUADS);
    glTexCoord2f(uv[0], uv[1]); glVertex3f(x0, terrain_y(x0, z0) + lift, z0);
    glTexCoord2f(uv[2], uv[1]); glVertex3f(x1, terrain_y(x1, z0) + lift, z0);
    glTexCoord2f(uv[2], uv[3]); glVertex3f(x1, terrain_y(x1, z1) + lift, z1);
    glTexCoord2f(uv[0], uv[3]); glVertex3f(x0, terrain_y(x0, z1) + lift, z1);
    glEnd();
    if (ghost) ra3d_blend(false);
    g_ra3dDecals++;
}

static bool ra3d_standing_overlay(const CNCDynamicMapEntryStruct& d)
{
    static const char* const walls[] = { "SBAG", "CYCL", "BRIK", "BARB", "WOOD", "FENC", NULL };
    if (d.IsSellable) return true;
    for (int i = 0; walls[i]; i++)
        if (!strcmp(d.AssetName, walls[i])) return true;
    return false;
}

/* The whole pass: called from draw_frame in place of the model and infantry passes. */
static void ra3d_draw_sprites(void)
{
    g_ra3dCards = g_ra3dDecals = 0;
    if (!dos2d_load() || !BrainGetState || !BrainGetView) return;
    static std::vector<unsigned char> bytes(sizeof(CNCObjectListStruct) + 8192 * sizeof(CNCObjectStruct));
    static std::vector<unsigned char> dynbytes(sizeof(CNCDynamicMapStruct) + 8192 * sizeof(CNCDynamicMapEntryStruct));
    int view[11]; BrainGetView(view);
    const float vx = (float)((view[4] * 24) / 256), vy = (float)((view[5] * 24) / 256);

    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.5f);
    glColor4f(1, 1, 1, 1);

    /* the ground layer: overlays, ore and smudges, then the ground animations */
    if (BrainGetState(GAME_STATE_DYNAMIC_MAP, 0, dynbytes.data(), (unsigned)dynbytes.size())) {
        const CNCDynamicMapStruct* dyn = (const CNCDynamicMapStruct*)dynbytes.data();
        const int count = dyn->Count < 8192 ? dyn->Count : 8192;
        /* Reuse OpenCNC3D's approved g3s gravel, on RA's own cell/frame layout. RA has
           no desert theater; its exterior aprons use the temperate set on grass and
           snow. Interior keeps its original art. No apron cells are invented. */
        const BibEnhTheater* bib = (g_beOn && g_beHave && g_fx.enabled && g_fx.bib_enh
                                   && ra2d_theater_letter() != 'I') ? bibenh_set_for(2) : NULL;
        if (bib) {
            std::vector<SmudgeCell> cells;
            for (int i = 0; i < count; i++) {
                const CNCDynamicMapEntryStruct& d = dyn->Entries[i];
                if (!d.IsSmudge || shroud_cell_hidden(d.CellX, d.CellY)
                    || strncmp(d.AssetName, "BIB", 3) || d.AssetName[3] < '1'
                    || d.AssetName[3] > '3' || d.AssetName[4] != 0) continue;
                SmudgeCell cell = {};
                cell.x = d.CellX; cell.y = d.CellY;
                cell.type = 12 + d.AssetName[3] - '1'; cell.data = d.ShapeIndex;
                cells.push_back(cell);
            }
            draw_bibenh(bib, cells, ra2d_theater_letter() == 'S');
            /* draw_bibenh restores TD pass defaults; resume this sprite pass. */
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.5f);
            glDepthMask(GL_FALSE);
            static bool said = false;
            if (!said && !cells.empty()) {
                fprintf(stderr, "RA3D|gravel|OpenCNC3D g3s|cells=%zu|texels=%d\n", cells.size(), g_beCell);
                said = true;
            }
        }
        for (int pass = 0; pass < 2; pass++)            /* flat first, then the walls */
            for (int i = 0; i < count; i++) {
                const CNCDynamicMapEntryStruct& d = dyn->Entries[i];
                if (shroud_cell_hidden(d.CellX, d.CellY)) continue;
                if (bib && d.IsSmudge && !strncmp(d.AssetName, "BIB", 3)
                    && d.AssetName[3] >= '1' && d.AssetName[3] <= '3' && !d.AssetName[4]) continue;
                /* OPENCNC3D_RA3DORE=off leaves the ore out. The Red Alert engine seeds its random
                   numbers from the clock, so the same mission's ore patches take different
                   densities from run to run; a gate comparing two runs' ground uses this. */
                if (ra3d_ore_off() && (!strncmp(d.AssetName, "GOLD", 4) || !strncmp(d.AssetName, "GEM", 3)))
                    continue;
                const bool stand = ra3d_standing_overlay(d);
                if (stand != (pass == 1)) continue;
                char key[24];
                snprintf(key, sizeof key, "%c/%s", ra2d_theater_letter(), d.AssetName);
                const char* art = dos2d_shape(key) ? key : d.AssetName;
                const Dos2DShape* s = dos2d_shape(art);
                if (!s) { g_ra3dMissing++; continue; }
                /* PLACED BY ITS CELL, not by its screen position: PositionX/Y are relative
                   to the engine's tactical view, which the engine moves on its own schedule,
                   so the same ore drew a cell off from one run to the next. An overlay or a
                   flag is centred on its cell, anything else hangs from its top left. */
                const bool centred = d.IsOverlay || d.IsFlag;
                const float x0 = centred ? (float)d.CellX + 0.5f - s->w / 48.0f : (float)d.CellX;
                const float z0 = centred ? (float)d.CellY + 0.5f - s->h / 48.0f : (float)d.CellY;
                const bool ghost = (d.DrawFlags & 0x1000) != 0;
                if (stand)
                    ra3d_card(art, d.ShapeIndex, x0 + s->w / 48.0f, z0 + s->h / 48.0f, -1, ghost);
                else
                    ra3d_decal(art, d.ShapeIndex, x0, z0, ghost);
            }
    }
    for (size_t i = 0; i < g_efxAnims.size(); i++) {
        const EfxAnim& a = g_efxAnims[i];
        if (!a.ground || a.stages <= 0 || a.stage < 0 || a.stage >= a.stages) continue;
        if (shroud_cell_hidden((int)floorf(a.wx), (int)floorf(a.wz))) continue;
        char key[24];
        snprintf(key, sizeof key, "%c/%s", ra2d_theater_letter(), a.name);
        const char* art = dos2d_shape(key) ? key : a.name;
        const Dos2DShape* s = dos2d_shape(art);
        if (!s || a.start + a.stage < 0 || a.start + a.stage >= s->frames) continue;
        ra3d_decal(art, a.start + a.stage, a.wx - s->w / 48.0f, a.wz - s->h / 48.0f, true);
    }

    /* Imported intact buildings replace all engine subobjects of that same heap entry,
       including WEAP2 and the sprite shadow. Keep fallback cards in the depth test so a
       soldier or vehicle behind a solid building does not paint through it. */
    glEnable(GL_DEPTH_TEST);
    /* the objects, in the engine's own painter order */
    if (BrainGetState(GAME_STATE_LAYERS, 0, bytes.data(), (unsigned)bytes.size())) {
        const CNCObjectListStruct* list = (const CNCObjectListStruct*)bytes.data();
        const int count = list->Count < 8192 ? list->Count : 8192;
        std::vector<int> order;
        for (int i = 0; i < count; i++) order.push_back(i);
        std::stable_sort(order.begin(), order.end(), [list](int a, int b) {
            return list->Objects[a].SortOrder < list->Objects[b].SortOrder;
        });
        const unsigned int playerMask = dos2d_player_mask();
        std::set<int> models;
        std::map<int, float> lifts;
        g_ra3dModelRoots = g_ra3dModelSubs = g_ra3dFallbackBuildings = 0;
        for (size_t bi = 0; bi < g_objects.size(); bi++) {
            const SimObject& b = g_objects[bi];
            if (b.kind == K_BUILDING && ebld_ra_ready(b)) models.insert(b.id);
        }
        for (int i = 0; i < count; i++) {
            const CNCObjectStruct& o = list->Objects[i];
            if (o.Type != BUILDING || o.SubObject || models.count(o.ID)) continue;
            char key[24];
            snprintf(key, sizeof key, "%c/%s", ra2d_theater_letter(), o.AssetName);
            const Dos2DShape* shape = dos2d_shape(o.IsTheaterSpecific ? key : o.AssetName);
            if (!shape) shape = dos2d_shape(key);
            lifts[o.ID] = ra3d_building_lift(shape, o.ShapeIndex);
        }
        for (size_t j = 0; j < order.size(); j++) {
            const CNCObjectStruct& o = list->Objects[order[j]];
            if (o.Type == ANIM) continue;
            const bool buildingRoot = o.Type == BUILDING && !o.SubObject;
            const bool buildingPart = o.BaseObjectType == BUILDING;
            const int buildingID = buildingRoot ? o.ID : o.BaseObjectID;
            if ((buildingRoot || buildingPart) && models.count(buildingID)) {
                if (buildingRoot) g_ra3dModelRoots++; else g_ra3dModelSubs++;
                continue;
            }
            /* a soldier with a remastered model is drawn by the infantry pass, not here */
            if (o.Type == INFANTRY && m3inf_find(o.AssetName) >= 0) continue;
            if (shroud_cell_hidden(o.CellX, o.CellY) && o.Type != BUILDING && o.Type != TERRAIN)
                continue;
            char key[24];
            snprintf(key, sizeof key, "%c/%s", ra2d_theater_letter(), o.AssetName);
            const bool flash = o.IsBlushing && (o.FlashingFlags & playerMask);
            const bool rotor = !strcmp(o.AssetName, "RROTOR") || !strcmp(o.AssetName, "LROTOR");
            const bool shadow = o.SubObject && (o.DrawFlags & 0x0300) == 0x0300;
            const int remap = flash || rotor || shadow ? -1
                            : (o.Type == BUILDING && o.Owner == 1 ? 2 : o.RemapColor);
            const float cx = (o.PositionX + vx) / 24.0f, cz = (o.PositionY + vy) / 24.0f;
            /* Interior's crates (BOXES01..09) exist only as interior art, yet the brain
               does not flag them theater specific: a name with no generic shape takes the
               theater's, as Classic's layer path does. */
            const bool theater = o.IsTheaterSpecific || !dos2d_shape(o.AssetName);
            if (buildingRoot) g_ra3dFallbackBuildings++;
            ra3d_card(theater && dos2d_shape(key) ? key : o.AssetName, o.ShapeIndex,
                      cx, cz, remap, (o.DrawFlags & 0x1000) != 0, flash, shadow,
                      (buildingRoot || buildingPart) ? lifts[buildingID] : 0.0f);
        }
    }

    /* the air: explosions, muzzle flashes and the move marker */
    for (size_t i = 0; i < g_efxAnims.size(); i++) {
        const EfxAnim& a = g_efxAnims[i];
        if (a.ground || a.stages <= 0 || a.stage < 0 || a.stage >= a.stages) continue;
        if (shroud_cell_hidden((int)floorf(a.wx), (int)floorf(a.wz))) continue;
        char key[24];
        snprintf(key, sizeof key, "%c/%s", ra2d_theater_letter(), a.name);
        const char* art = dos2d_shape(key) ? key : a.name;
        const Dos2DShape* s = dos2d_shape(art);
        if (!s || a.start + a.stage < 0 || a.start + a.stage >= s->frames) continue;
        const bool translucent = !strcmp(a.name, "MOVEFLSH") || !strcmp(a.name, "GUNFIRE")
            || !strcmp(a.name, "CORPSE1") || !strcmp(a.name, "CORPSE2") || !strcmp(a.name, "CORPSE3");
        ra3d_card(art, a.start + a.stage, a.wx, a.wz, -1, translucent);
    }
    glPopAttrib();
    glAlphaFunc(GL_GREATER, 0.5f);
    static char reported[16] = "";
    if (g_ra3dCards > 0 && strcmp(reported, g_bootScen)) {
        snprintf(reported, sizeof reported, "%s", g_bootScen);
        fprintf(stderr, "RA3D|sprites3d|%s|cards=%d|decals=%d|missing=%d|theater=%s"
                "|modelroots=%d|modelsubs=%d|fallbackbuildings=%d\n",
                g_bootScen, g_ra3dCards, g_ra3dDecals, g_ra3dMissing, g_pack.theater,
                g_ra3dModelRoots, g_ra3dModelSubs, g_ra3dFallbackBuildings);
    }
}
