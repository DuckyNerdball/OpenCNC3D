#pragma once

struct GunStanding {
    int seat, level;
    unsigned kills;
    const char* unit;
    bool alive;
};

static void gun_hud_fit(char* text, int width)
{
    const int limit = std::max(0, width / g_sbFontCellW);
    const int len = (int)strlen(text);
    if (len <= limit) return;
    text[limit] = 0;
    for (int i = std::max(0, limit - 3); i < limit; ++i) text[i] = '.';
}

static const char* gun_hud_name(int seat)
{
    const char* name = gun_seat_name(seat);
    return *name ? name : "PLAYER";
}

static const char* gun_hud_unit_name(const char* code)
{
    if (const char* title = gun_ra_name(code)) return title;
    if (!strcmp(code, "HELI")) return "Apache";
    const char* title = sb_title_for(code);
    return title ? title : code;
}

static void gun_hud_swatch(int seat, int x, int y, int size)
{
    const unsigned rgb = net_seat_rgb(seat);
    sb_rect(x, y, x + size, y + size,
            ((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, 1);
}

static void gun_game_draw_hover(int fbw, int fbh)
{
    if (!g_sb.GunPlayers || g_gunHoverSeat < 0 || g_gameOver.valid) return;
    const int s = sb_gun_layout(fbw, fbh).scale;
    const int inset = 5 * s;
    const char* name = gun_hud_name(g_gunHoverSeat);
    const int w = (int)sb_text_w(name, s) + 22 * s;
    const int h = (g_sbFontCellH + 8) * s;
    if (w + 2 * inset > fbw || h + 2 * inset > fbh) return;
    const int col = (int)g_mouseScrC, row = (int)g_mouseScrR;
    const int beside = col + 14 * s;
    const int x = std::max(inset, std::min(fbw - inset - w, beside + w + inset <= fbw ? beside : col - w - 9 * s));
    const int y = std::max(inset, std::min(fbh - inset - h, row - h - 7 * s));
    begin_overlay(fbw, fbh);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    sb_rect(x, y, x + w, y + h, 0.094f, 0.09f, 0.075f, 0.96f);
    sb_frame(x, y, x + w, y + h, 0.51f, 0.45f, 0.36f, 1);
    gun_hud_swatch(g_gunHoverSeat, x + 6 * s, y + (h - 8 * s) / 2, 8 * s);
    sb_text(x + 17 * s, y + 4 * s, name, s, 0.91f, 0.89f, 0.84f);
    glDisable(GL_BLEND);
    end_overlay();
}

static bool gun_game_draw_winner(int fbw, int fbh)
{
    if (!g_sb.GunPlayers || g_gunWinnerSeat < 0 || !g_gameOver.valid) return false;
    char line[NM_PLAYER_NAME_MAX + sizeof " WINS"];
    snprintf(line, sizeof line, "%s WINS", gun_hud_name(g_gunWinnerSeat));
    const int text_w = (int)sb_text_w(line, 1);
    const int s = std::max(1, std::min(fbw / std::max(320, text_w + 28), fbh / 165));
    const int w = (text_w + 28) * s, h = (g_sbFontCellH + 24) * s;
    const int x = (fbw - w) / 2, y = (fbh - h) / 2;
    begin_overlay(fbw, fbh);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    sb_rect(x, y, x + w, y + h, 0.094f, 0.09f, 0.075f, 0.97f);
    sb_frame(x, y, x + w, y + h, 0.67f, 0.55f, 0.33f, 1);
    sb_text(x + 14 * s, y + 12 * s, line, s, 1.0f, 0.85f, 0.54f);
    glDisable(GL_BLEND);
    end_overlay();
    return true;
}

static const SbEntry* gun_game_charge()
{
    if (!g_sb.GunPlayers) return nullptr;
    for (const SbEntry& e : g_sbState.entry)
        if (e.type == SPECIAL && e.progress >= 1.0f) return &e;
    return nullptr;
}

static void gun_game_arm_crate()
{
    if (sb_super_armed()) sb_super_disarm("cancelled");
    else if (const SbEntry* charge = gun_game_charge()) sb_super_arm(*charge);
}

struct GunRewardButton {
    int x, y, scale;
    enum { Width = 74, Height = 72 };

    bool contains(float col, float row) const
    {
        return col >= x && col < x + Width * scale && row >= y && row < y + Height * scale;
    }
};

/* Two fixed slots under the minimap, so neither cameo moves when the other comes or
   goes: the superweapon on the left, the ability (gun_game_ability_hud.h) on the right. */
static int gun_game_reward_slots_x(const GunHudLayout& l)
{
    const int pair = 2 * GunRewardButton::Width + 6;
    const int fbw = l.radar_x + (l.radar_w + 8) * l.scale;
    return std::min(l.radar_x + ((l.radar_w - pair) / 2) * l.scale, fbw - (pair + 8) * l.scale);
}

static GunRewardButton gun_game_reward_button(const GunHudLayout& l)
{
    return {gun_game_reward_slots_x(l), (14 + l.radar_h) * l.scale, l.scale};
}

static GunRewardButton gun_game_ability_button(const GunHudLayout& l)
{
    return {gun_game_reward_slots_x(l) + (GunRewardButton::Width + 6) * l.scale, (14 + l.radar_h) * l.scale, l.scale};
}

static int ga_held();
static bool ga_try_open(int fbw, int fbh, Uint32 now);
static void gun_game_draw_ability(const GunRewardButton& button);
static void gun_game_draw_ability_timers(const GunHudLayout& l, int y);
static void ga_draw_minimap(int fbw, int fbh);

static bool gun_game_ability_over(float col, float row, int fbw, int fbh)
{
    return !g_gameOver.valid && ga_held() != GA_NONE
        && gun_game_ability_button(sb_gun_layout(fbw, fbh)).contains(col, row);
}

/* Either cameo: the sidebar asks this so a click on one never falls through to the map. */
static bool gun_game_reward_over(float col, float row, int fbw, int fbh)
{
    return (!g_gameOver.valid && gun_game_charge()
            && gun_game_reward_button(sb_gun_layout(fbw, fbh)).contains(col, row))
        || gun_game_ability_over(col, row, fbw, fbh);
}

static bool gun_game_reward_click(int x, int y, int fbw, int fbh)
{
    if (g_gunLaunch.active) return false;
    if (gun_game_ability_over(x, y, fbw, fbh)) {
        ga_try_open(fbw, fbh, SDL_GetTicks());
        return true;
    }
    if (!gun_game_reward_over(x, y, fbw, fbh)) return false;
    gun_game_arm_crate();
    return true;
}

static void gun_game_draw_reward(const GunHudLayout& l)
{
    const SbEntry* charge = gun_game_charge();
    if (!charge || g_gameOver.valid) return;
    const GunRewardButton button = gun_game_reward_button(l);
    const int x = button.x, y = button.y, s = button.scale;
    const int w = button.Width * s, h = button.Height * s;
    const bool armed = sb_super_armed();
    const bool hot = armed || button.contains(g_mouseScrC, g_mouseScrR);
    sb_rect(x, y, x + w, y + h, 0.15f, 0.13f, 0.10f, 0.97f);
    sb_frame(x, y, x + w, y + h, hot ? 1.0f : 0.67f, hot ? 0.80f : 0.55f, 0.33f, 1);
    unsigned char cameo[64 * 48 * 4];
    eh_cameo(cameo, charge->name, charge);
    h6_upload(&g_gunRewardTex, cameo, 64, 48);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);
    h6_tex_quad(g_gunRewardTex, 64, 48, x + 5 * s, y + 5 * s, x + 69 * s, y + 53 * s);
    const char* label = armed ? "AIMING" : "Q  AIM";
    sb_text(x + (w - sb_text_w(label, s)) / 2, y + 57 * s, label, s, 0.95f, 0.86f, 0.57f);
}

static bool gun_game_draw_puzzle(int fbw, int fbh)
{
    if (!g_gunLaunch.active || g_gameOver.valid) return false;
    const GunLaunchPuzzle& p = g_gunLaunch;
    const int s = std::max(1, std::min(fbw / 360, fbh / 270));
    const int w = 300 * s, h = 102 * s;
    const int x = (fbw - w) / 2, y = (fbh - h) / 2;
    const int key_w = 38 * s, key_h = 38 * s, pitch = 44 * s, row = y + 34 * s;
    const bool wrong = p.wrong && SDL_GetTicks() - p.wrong_at < 240;

    begin_overlay(fbw, fbh);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    sb_rect(x, y, x + w, y + h, 0.094f, 0.09f, 0.075f, 0.98f);
    sb_frame(x, y, x + w, y + h, 0.67f, 0.55f, 0.33f, 1);
    const auto centered = [&](const char* label, int row, float r, float g, float b) {
        sb_text((fbw - sb_text_w(label, s)) / 2, row, label, s, r, g, b);
    };
    centered("ENTER LAUNCH CODE", y + 11 * s, 1, 0.85f, 0.54f);
    for (int i = 0; i < (int)sizeof p.keys; ++i) {
        const int col = x + 21 * s + i * pitch;
        const bool passed = i < p.next;
        const bool failed = i == p.next && wrong;
        sb_rect(col, row, col + key_w, row + key_h, 0.10f, 0.09f, 0.07f, 1);
        sb_rect(col + s, row + s, col + key_w - s, row + key_h - 4 * s,
                failed ? 0.54f : passed ? 0.15f : 0.23f,
                failed ? 0.11f : passed ? 0.39f : 0.22f, passed ? 0.15f : 0.16f, 1);
        sb_frame(col, row, col + key_w, row + key_h,
                 failed ? 0.96f : passed ? 0.39f : 0.68f,
                 failed ? 0.29f : passed ? 0.80f : 0.58f,
                 failed ? 0.24f : passed ? 0.36f : 0.42f, 1);
        if (i == p.next && !wrong)
            sb_rect(col + 4 * s, row + key_h - 3 * s, col + key_w - 4 * s, row + key_h - s,
                    1, 0.83f, 0.50f, 1);
        const char label[] = {p.keys[i], 0};
        sb_text(col + (key_w - sb_text_w(label, 2 * s)) / 2, row + 5 * s,
                label, 2 * s, 0.96f, 0.94f, 0.86f);
    }
    centered("ESC CANCEL", y + 82 * s, 0.79f, 0.75f, 0.66f);
    glDisable(GL_BLEND);
    end_overlay();
    return true;
}

static void sb_draw_gun_hud(int fbw, int fbh)
{
    const GunHudLayout l = sb_gun_layout(fbw, fbh);
    const int s = l.scale;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // Keep the existing Options action without the sidebar's other plates.
    if (g_h6Pack) {
        const int state = g_sbPress == SBH_OPTIONS ? 2
            : sb_chrome_hit(g_mouseScrC, g_mouseScrR, fbw, fbh) == SBH_OPTIONS ? 1 : 0;
        hud640_draw_tab(g_h6Tab, g_h6Pack, "OPTIONS", 0, state);
        h6_upload(&g_h6TexOpt, g_h6Tab, H6_BAR_W, H6_TAB_H);
        glEnable(GL_TEXTURE_2D);
        glColor4f(1, 1, 1, 1);
        h6_tex_quad(g_h6TexOpt, H6_BAR_W, H6_TAB_H, 0, 0, H6_BAR_W * s, H6_TAB_H * s);
    } else {
        sb_rect(0, 0, H6_BAR_W * s, H6_TAB_H * s, 0.18f, 0.17f, 0.15f, 1);
        sb_text(52 * s, 3 * s, "OPTIONS", s, 0.91f, 0.88f, 0.83f);
    }

    // The separate radar shares the sidebar plotter, palette, shroud and map hit test.
    if (g_dbPack) {
        h6_radar_plot(g_h6Radar, l.map_w, l.map_h, g_dbPack, 1);
        const H6_Asset* frame = !l.compact && g_h6Pack ? hud640_asset(g_h6Pack, "eh_map") : nullptr;
        const int mx1 = l.map_x + l.map_w * s, my1 = l.map_y + l.map_h * s;
        sb_rect(l.map_x, l.map_y, mx1, my1, 0.031f, 0.035f, 0.043f, 1);
        h6_upload(&g_h6TexBar, g_h6Radar, l.map_w, l.map_h);
        glEnable(GL_TEXTURE_2D);
        glColor4f(1, 1, 1, 1);
        h6_tex_quad(g_h6TexBar, l.map_w, l.map_h, l.map_x, l.map_y, mx1, my1);
        if (frame) {
            h6_upload(&g_h6TexSide, frame->rgba, frame->w, frame->h);
            h6_tex_quad(g_h6TexSide, frame->w, frame->h, l.radar_x, 8 * s,
                        l.radar_x + frame->w * s, (8 + frame->h) * s);
        } else {
            sb_frame(l.map_x - s, l.map_y - s, mx1 + s, my1 + s, 0.51f, 0.45f, 0.36f, 1);
        }
    }

    gun_game_draw_reward(l);
    gun_game_draw_ability(gun_game_ability_button(l));
    gun_game_draw_ability_timers(l, (14 + l.radar_h + GunRewardButton::Height + 6) * s);
    ga_draw_minimap(fbw, fbh);

    GunStanding rows[NM_MAX_SEATS];
    int count = 0;
    for (int i = 0; i < g_matchRows && i < NM_MAX_SEATS; ++i) {
        GunStanding r = {};
        r.seat = g_matchSeat[i];
        r.unit = BrainGunStanding(g_matchId[i], &r.level, &r.kills, &r.alive);
        if (r.unit) rows[count++] = r;
    }
    std::sort(rows, rows + count, [](const GunStanding& a, const GunStanding& b) {
        if (a.level != b.level) return a.level > b.level;
        if (a.kills != b.kills) return a.kills > b.kills;
        return a.seat < b.seat;
    });

    const int x = 8 * s, y = 28 * s, right = x + l.score_w * s;
    const int bottom = y + l.score_h * s;
    const int unit = l.score_w - std::min(146, l.score_w / 3);
    const bool short_labels = l.score_w < 325;
    const int level = unit - 12, kills = level - (short_labels ? 32 : 48);
    const int name = short_labels ? 30 : 36;
    const int kill_width = (short_labels ? 3 : 5) * g_sbFontCellW;
    if (kills - name >= kill_width + 2 * g_sbFontCellW) {
        sb_rect(x, y, right, bottom, 0.094f, 0.09f, 0.075f, 0.96f);
        sb_rect(x + s, y + s, right - s, y + 17 * s, 0.196f, 0.16f, 0.12f, 1);
        sb_frame(x, y, right, bottom, 0.51f, 0.45f, 0.36f, 1);
        sb_text(x + 8 * s, y + 4 * s, "GUN GAME", s, 0.90f, 0.77f, 0.50f);
        for (int i = 0; i < count; ++i) {
            if (rows[i].seat != gun_seat_self() || strcmp(rows[i].unit, "CTNK")) continue;
            const char* hint = "X  CHRONOSHIFT";
            const float w = sb_text_w(hint, s);
            if (w + 96 * s < l.score_w * s)
                sb_text(right - w - 8 * s, y + 4 * s, hint, s, 0.90f, 0.77f, 0.50f);
            break;
        }
        const auto header = [&](const char* title, int at) {
            sb_text(x + at * s, y + 20 * s, title, s, 0.82f, 0.76f, 0.64f);
        };
        header("PLAYER", name);
        header(short_labels ? "K" : "KILLS", kills - (short_labels ? g_sbFontCellW : kill_width));
        const char* level_label = short_labels ? "LVL" : "LEVEL";
        header(level_label, level - (int)strlen(level_label) * g_sbFontCellW);
        header("UNIT", unit);
        for (int i = 0; i < count; ++i) {
            const GunStanding& r = rows[i];
            const int row_y = y + (33 + 16 * i) * s;
            const bool own = r.seat == gun_seat_self();
            if (own || (i % 2))
                sb_rect(x + s, row_y - 2 * s, right - s, row_y + 14 * s,
                        own ? 0.21f : 0.12f, own ? 0.17f : 0.115f, own ? 0.12f : 0.10f, 1);
            const float ink = r.alive ? 0.91f : 0.67f;
            gun_hud_swatch(r.seat, x + (name - 12) * s, row_y + 2 * s, 8 * s);
            const auto number = [&](unsigned value, int at) {
                char text[16];
                snprintf(text, sizeof text, "%u", value);
                sb_text(x + at * s - sb_text_w(text, s), row_y, text, s, ink, ink, ink);
            };
            number(i + 1, short_labels ? 14 : 16);
            number(r.kills, kills);
            number(r.level, level);
            char handle[NM_PLAYER_NAME_MAX], title[48];
            snprintf(handle, sizeof handle, "%s", gun_hud_name(r.seat));
            gun_hud_fit(handle, kills - kill_width - name - 6);
            snprintf(title, sizeof title, "%s", gun_hud_unit_name(r.unit));
            gun_hud_fit(title, l.score_w - unit - 8);
            sb_text(x + name * s, row_y, handle, s, ink, ink, ink);
            sb_text(x + unit * s, row_y, title, s, ink, ink, ink);
        }
    }
    glDisable(GL_BLEND);
}
