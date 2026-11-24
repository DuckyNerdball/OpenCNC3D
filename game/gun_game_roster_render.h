#pragma once

// The TD brain draws the original SHP frames using its normal object layers.
// Keep only the nine roster types here, including turret and rotor records.
//
// THE FUNPARK DINOSAURS take the same road. TREX, TRIC, STEG and RAPT have no model on
// the cartridge or in the remaster set, so the 3D modes stand their 1995 SHP frames up as
// cards, the frame the engine itself would draw (facing and walk or bite stage) and the
// same art Classic draws, rather than a grey fallback box.
static bool td_dino_name(const char* code)
{
    return !strcmp(code, "TREX") || !strcmp(code, "TRIC") || !strcmp(code, "STEG")
        || !strcmp(code, "RAPT");
}

static bool card_layer_name(const char* code)
{
    return gun_ra_name(code) || (!g_raModule && td_dino_name(code));
}

static std::vector<CNCObjectStruct> g_gunRaLayers;
static int g_gunRaLayerFrame = -1;

static void gun_ra_layers()
{
    if (g_gunRaLayerFrame == g_engineFrame) return;
    g_gunRaLayerFrame = g_engineFrame;
    g_gunRaLayers.clear();
    if (!dos2d_load() || !BrainGetState) return;
    static std::vector<unsigned char> buffer(sizeof(CNCObjectListStruct) + 8192 * sizeof(CNCObjectStruct));
    if (!BrainGetState(GAME_STATE_LAYERS, 0, buffer.data(), (unsigned)buffer.size())) return;
    const auto* list = reinterpret_cast<const CNCObjectListStruct*>(buffer.data());
    for (int i = 0; i < list->Count && i < 8192; ++i) {
        const auto& item = list->Objects[i];
        if (card_layer_name(item.TypeName)) g_gunRaLayers.push_back(item);
    }
}

static const CNCObjectStruct* gun_ra_body(const SimObject& o)
{
    gun_ra_layers();
    for (const auto& item : g_gunRaLayers)
        if (!item.SubObject && item.ID == o.id && !strcmp(item.TypeName, o.type)) return &item;
    return nullptr;
}

static void gun_ra_center(const SimObject& o, const Dos2DShape* shape, int frame, float out[3])
{
    ra3d_stand(shape, frame, o.wx, o.wz, out);
    out[1] += o.kind == K_AIRCRAFT ? o.alt / 24.0f : o.kind == K_UNIT ? o.alt / 256.0f : 0.0f;
}

static bool gun_ra_screen_poly(const SimObject& o, int fbw, int fbh, ScreenPoly* poly)
{
    const auto* body = gun_ra_body(o);
    const Dos2DShape* shape = dos2d_shape(o.type);
    if (!body || !shape) return false;
    float center[3];
    gun_ra_center(o, shape, body->ShapeIndex, center);
    for (int corner = 0; corner < 4; ++corner) {
        const float u = (corner == 0 || corner == 3 ? -1 : 1) * shape->w / 48.0f;
        const float v = (corner < 2 ? -1 : 1) * shape->h / 48.0f;
        float x, y;
        world_to_screen(center[0] + g_bbRight[0] * u + g_bbUp[0] * v,
                        center[1] + g_bbRight[1] * u + g_bbUp[1] * v,
                        center[2] + g_bbRight[2] * u + g_bbUp[2] * v, fbw, fbh, &x, &y);
        dop_add(poly, x, y);
    }
    dop_finish(poly);
    return true;
}

static void gun_ra_roll_card(const SimObject& o, const CNCObjectStruct& body, const float center[3])
{
    float uv[4];
    Dos2DShape* shape = ra3d_shape(o.type, body.ShapeIndex, body.RemapColor, false, false, false, uv);
    if (!shape) return;
    const float angle = o.roll * 6.2831853f / 256.0f;
    const float c = cosf(angle), s = sinf(angle);
    const float u[] = {-shape->w / 48.0f, shape->w / 48.0f};
    const float v[] = {-shape->h / 48.0f, shape->h / 48.0f};
    const int xs[] = {0, 1, 1, 0}, ys[] = {1, 1, 0, 0};
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; ++i) {
        const float x = u[xs[i]] * c - v[ys[i]] * s, y = u[xs[i]] * s + v[ys[i]] * c;
        glTexCoord2f(uv[xs[i] ? 2 : 0], uv[ys[i] ? 1 : 3]);
        glVertex3f(center[0] + g_bbRight[0] * x + g_bbUp[0] * y,
                   center[1] + g_bbRight[1] * x + g_bbUp[1] * y,
                   center[2] + g_bbRight[2] * x + g_bbUp[2] * y);
    }
    glEnd();
}

static bool gun_ra_sprite_fallback(const SimObject& o)
{
    if (!g_raModule && td_dino_name(o.type)) return world_mesh_for(o) < 0;
    if (!g_sb.GunPlayers || !gun_ra_name(o.type)) return false;
    const int model = evhc_for(o);
    return model < 0 || !evhc_resident(g_evhc[model], false);
}

/* A DINOSAUR'S DEATH is an animation of its own SHP (anim.cpp: TRIC, TREX, STEG and RAPT
   name both the unit's art and its death), drawn by 1995 through Map.UnitShadow. The unit
   is gone from the object list the moment it dies, so its death is stood up from the
   engine's animation records, the ones Classic draws it from. */
static bool td_dino_death(const EfxAnim& a)
{
    if (g_raModule || !td_dino_name(a.name)) return false;
    const Dos2DShape* shape = dos2d_shape(a.name);
    return shape && a.stages > 0 && a.stage >= 0 && a.stage < a.stages
        && a.start + a.stage >= 0 && a.start + a.stage < shape->frames
        && !shroud_cell_hidden((int)floorf(a.wx), (int)floorf(a.wz));
}

static void gun_ra_draw()
{
    bool any = false;
    for (const SimObject& o : g_objects)
        if (visible(o) && gun_ra_sprite_fallback(o)) { any = true; break; }
    bool deaths = false;
    if (!g_raModule)
        for (const EfxAnim& a : g_efxAnims)
            if (td_dino_name(a.name)) { deaths = true; break; }
    if (deaths && !dos2d_load()) deaths = false;
    if (!any && !deaths) return;
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glColor4f(1, 1, 1, 1);
    for (const SimObject& o : g_objects) {
        if (!visible(o) || !gun_ra_sprite_fallback(o)) continue;
        if ((o.kind == K_INFANTRY && !g_infantryDraw) || (o.kind != K_INFANTRY && !g_vShowUnits)) continue;
        const auto* body = gun_ra_body(o);
        const Dos2DShape* shape = dos2d_shape(o.type);
        if (!body || !shape) continue;
        float center[3];
        gun_ra_center(o, shape, body->ShapeIndex, center);
        if (o.kind == K_INFANTRY && o.roll > 0) {
            // Rotate around the soldier's body with the same live phase as Dawn infantry.
            gun_ra_roll_card(o, *body, center);
        } else {
            /* SHAPE_GHOST: the engine draws a dinosaur through Map.UnitShadow, which turns
               its LTGREEN shadow pixels into a darkening of the ground (dos2d_upload's
               ghost); drawn plain they were a bright green blot under the animal. */
            ra3d_card_at(o.type, body->ShapeIndex, center[0], center[1], center[2], body->RemapColor,
                         (body->DrawFlags & 0x1000) != 0);
        }
        for (const auto& item : g_gunRaLayers) {
            if (!item.SubObject || item.CNCInternalObjectPointer != body->CNCInternalObjectPointer) continue;
            const char* name = item.AssetName;
            const bool rotor = !strcmp(name, "RROTOR");
            if (!rotor && (item.DrawFlags & 0x0300) == 0x0300) continue;
            if (!dos2d_shape(name)) continue;
            const float u = (item.PositionX - body->PositionX) / 24.0f;
            const float v = (body->PositionY - item.PositionY) / 24.0f;
            ra3d_card_at(name, item.ShapeIndex, center[0] + g_bbRight[0] * u + g_bbUp[0] * v,
                        center[1] + g_bbRight[1] * u + g_bbUp[1] * v,
                        center[2] + g_bbRight[2] * u + g_bbUp[2] * v, rotor ? -1 : item.RemapColor, rotor);
        }
        ++g_nSprite;
    }
    for (const EfxAnim& a : g_efxAnims) {
        if (!deaths || !td_dino_death(a)) continue;
        const int frame = a.start + a.stage;
        float center[3];
        ra3d_stand(dos2d_shape(a.name), frame, a.wx, a.wz, center);
        ra3d_card_at(a.name, frame, center[0], center[1], center[2], -1, true);
        ++g_nSprite;
    }
}
