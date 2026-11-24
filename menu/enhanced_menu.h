#ifndef CNC_ENHANCED_MENU_H
#define CNC_ENHANCED_MENU_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "dosmenu.h"
#include "dosops.h"
#include "doslobby.h"
#include "dosmp.h"
#include "dosopt.h"
/* Art and hit seats share native pixels. The legacy controllers still own actions. */
int game_enhanced_menu_enabled(void);
void game_enhanced_menu_sound(int click);
/* Ask the driver for the player's vertical sync setting when it differs from the interval
   last asked for; the menu loops call it once per pass. */
void game_menu_vsync(void);
union SDL_Event;
struct SDL_Window;
/* Shared pointer feedback for every Enhanced menu controller. */
void em_sound_event(const union SDL_Event *event, struct SDL_Window *window);
int em_available(void);
/* Draw Red Alert's Enhanced kit (ra3d/enhanced-menu.pack) instead of Tiberian Dawn's, with
   this version line on the main menu and this legal notice under it. Once per process,
   before the first screen. */
void em_set_red_alert(const char *version, const char *legal);
int em_red_alert(void);
/* The side of the Red Alert mission being played (0 Allied, 1 Soviet): its briefing paper. */
void em_set_mission_side(int soviet);
/* Red Alert's routes whose engine side is not connected yet. A route not marked ready keeps
   its button and answers with em_route_notice's text in a notice box. Always ready for
   Tiberian Dawn. */
enum { EM_ROUTE_SAVES, EM_ROUTE_SKIRMISH, EM_ROUTE_NETWORK, EM_ROUTE_USERMAPS, EM_ROUTE_DATABASE, EM_ROUTE_COUNT };
void em_set_route_ready(int route, int ready);
int em_route_ready(int route);
const char *em_route_notice(int route);
int em_density(void);
void em_main(const DM_State *st);
int em_ra_speaker_rect(int* x,int* y,int* w,int* h);
int em_main_next(const DM_State *st, int item, int delta);
void em_ops(DO_State *st);
struct DMS_MapImportState;
void em_import_map(const struct DMS_MapImportState *st);
void em_ops_move(DO_State *st, int delta);
void em_ops_filter(DO_State *st, int category);
void em_ops_step(DO_State *st, int delta);
void em_lobby(const SK_State *st, const DB_Pack *p);
void em_browser(const MP_State *st);
void em_options(DOPT_State *st);
void em_title_mode(int enabled);
/* THE LANGUAGE ROW of the Settings screen's Gameplay tab and its download box, answered by
   the game (game/lang_mod.h through cnc_eyes.cpp). Language 0 is English. Without hooks
   (a gate, a tool) the row is not drawn. */
typedef struct {
    int (*count)(void);                         /* languages, English included          */
    const char *(*name)(int i);                 /* "English", "Deutsch"                  */
    int (*current)(void);
    int (*installed)(int i);                    /* its pack complete on the disk         */
    long long (*missing)(int i);                /* bytes still to fetch                  */
    int (*choose)(int i);                       /* use it now (installed or English)     */
    int (*download)(int i);                     /* start fetching its pack               */
    int (*progress)(long long *done, long long *total, char *msg, int msglen);
    void (*cancel)(void);
    const char *(*translate)(const char *text); /* optional built-in menu strings */
} EM_LangHooks;
void em_set_language_hooks(const EM_LangHooks *hooks);
void em_viewport(int width, int height);
void em_draw(int width, int height, int clear);
/* Menu-owned overlay; coordinates are drawable pixels, negative hides it. */
void em_menu_overlay(const DB_Pack *pack, int x, int y);
void em_to_legacy(int fx, int fy, int *x, int *y);
int em_item_rect(int item, int *x, int *y, int *w, int *h);
int em_seat_point(const char *label, int *x, int *y);
int em_opt_press(DOPT_State *st, int x, int y);
int em_opt_motion(DOPT_State *st, int x, int y);
int em_opt_release(DOPT_State *st, int x, int y);
int em_sk_press(SK_State *st, int x, int y);
int em_sk_motion(SK_State *st, int x, int y);
int em_sk_release(SK_State *st, int x, int y);
int em_mp_press(MP_State *st, int x, int y);
void em_mp_motion(MP_State *st, int x, int y);
int em_ops_hit(const DO_State *st, int x, int y);
void em_ops_action(DO_State *st, int hit);
/* RGBA witness of the native scene, for the existing hidden menu harness. */
int em_write_shot(const char *path, int width, int height);
void em_scroll(int delta);
void em_opt_scroll(DOPT_State *st, int delta);
int em_opt_key(DOPT_State *st, int key);
int em_mp_row_at(const MP_State *st, int x, int y);
void em_reset(void);
/* ON: the screen builders that follow (em_main, em_ops, em_lobby, em_browser,
   em_import_map, em_choice, em_side_choice, em_match_debrief) only place their seats, for a
   hit test against the current screen, and draw nothing; OFF again before the drawing
   call. Those builders redraw the panel only when what they would draw has changed. */
void em_layout_pass(int on);
/* How many times a panel has been drawn in software, and how many times one has been sent
   to the card, since the program started: what the menu bench measures. */
unsigned em_raster_count(void);
unsigned em_upload_count(void);
void em_choice(int which, int selected, int pressed);
int em_pick(int fx, int fy);
/* Red Alert's side choice (em_side_choice) and the items em_pick returns on it. */
enum { EM_SIDE_BACK = 0, EM_SIDE_ALLIES = 1, EM_SIDE_SOVIET = 2, EM_SIDE_START = 3,
       EM_SIDE_EASY = 10, EM_SIDE_NORMAL = 11, EM_SIDE_HARD = 12 };
void em_side_choice(int side, int difficulty, int pressed);

/* THE MATCH DEBRIEF, after a skirmish or a network match, in both games: the 1995 and 1996
 * multiplayer score's three columns (each commander in its colour with what it played,
 * battles won this session, kills in each of the session's last four games) in the loaded
 * game's own kit, the result and the map above, this machine's own numbers for the match
 * below. Any click or key continues, as on the old screens; em_pick names
 * EM_DEBRIEF_CONTINUE for its button. */
enum { EM_DEBRIEF_CONTINUE = 1 };
/* THE DEBRIEF'S PAGES, a row of tabs under its title: the TOP SCORES table, each
 * commander's score and ratings, units, structures, money, and the match drawn over time.
 * em_pick names EM_DEBRIEF_TAB + page for a tab and EM_DEBRIEF_SERIES + series for the
 * graph's series buttons. */
enum { EM_DEBRIEF_TAB = 10, EM_DEBRIEF_SERIES = 20 };
enum { EM_PAGE_SUMMARY = 0, EM_PAGE_SCORE, EM_PAGE_UNITS, EM_PAGE_STRUCTURES, EM_PAGE_ECONOMY,
       EM_PAGE_GRAPH, EM_PAGES };
/* The graph's series, in the order the match timeline records them: score, army standing,
   structures standing, harvested, money held, kills. */
#define EM_SERIES 6
#define EM_SAMPLES 240
/* The Settings screen's Gameplay and Audio seats: EM_SETTING_SEAT - DOPT_ES_* for a slider,
   EM_GAMEPLAY_SEAT - n for the nth input switch. */
enum { EM_SETTING_SEAT = -300, EM_GAMEPLAY_SEAT = -320 };
/* THE KEY BINDINGS PAGE of the Settings screen (opened from the Gameplay tab), answered by
   the game (cnc_eyes.cpp), which owns the command table, the reserved keys and the drag
   button. Without hooks (a gate, a tool) the page's button is not drawn. Seats: the key box
   of command n is EM_KEYS_SEAT - n, the drag button's choices EM_KEYS_DRAG - b, the page's
   button on the Gameplay tab EM_KEYS_OPEN, and the mouse scheme's three buttons on both
   Gameplay pages EM_SCHEME_SEAT - s. */
typedef struct {
    const char *(*key)(int cmd);                /* FXK_* (fx_input.h) to "S", or ""        */
    /* bind command cmd to this SDL keycode/scancode; 1 done, 0 refused. msg says either way */
    int (*assign)(int cmd, int keycode, int scancode, char *msg, int msglen);
    void (*reset)(void);                        /* every key and the drag button          */
    int (*grab)(void);                          /* the drag button, FX_GRAB_*             */
    void (*set_grab)(int button);
} EM_KeyHooks;
void em_set_key_hooks(const EM_KeyHooks *hooks);
/* A key box was pressed and the page is waiting for a key: the host hands the next key
   press to em_keys_capture instead of the dialog. Escape cancels the wait. */
int em_keys_capturing(void);
void em_keys_capture(int keycode, int scancode);
enum { EM_KEYS_SEAT = -400, EM_KEYS_DRAG = -430, EM_KEYS_OPEN = -436, EM_SCHEME_SEAT = -440 };
/* the language row (EM_LANG_SEAT), its list (EM_LANG_SEAT - 1 - n for language n), the box's
   buttons (EM_LANG_SEAT - 20 .. - 22), and the list's own id in View::drop */
enum { EM_LANG_SEAT = -340, EM_LANG_DROP = -40 };
/* Stable pointer seats for committing/cancelling an offline player edit. */
enum { EM_SK_DONE = -380, EM_SK_BACK = -381 };
/* The lobby's Game Mode dropdown: its button is EM_SK_MODE_DROP (-15, in the enhanced menu's
   own numbering) and its list EM_SK_MODE_BASE + SK_MODE_*. em_sk_drop_open says a lobby list
   is open, so the shell keeps Escape and Enter for the list rather than the room. */
enum { EM_SK_MODE_DROP = -15, EM_SK_MODE_BASE = -390 };
int em_sk_drop_open(void);
void em_sk_drop_close(void);
typedef struct EM_DebriefRow {
    char name[16];
    int country;      /* Red Alert: 0..7, Spain Greece USSR England Ukraine Germany France
                         Turkey. Tiberian Dawn: the side, 0 GDI and 1 Nod                  */
    int colour;       /* 0..7, the player colours                                      */
    int wins;         /* battles won this session                                      */
    int kills[4];     /* kills in each of the session's last games, oldest first; -1 none */
    int defeated;     /* beaten in this match                                          */
    int is_local;     /* the commander at this machine                                 */
    /* this match, per commander: the engine's counts, and the renderer's (built, the
       army's peak, what stood at the end, the money held at the start and the end, spent) */
    int32_t killed, razed, units_lost, structures_lost, harvested, score;
    int32_t units_built, structures_built, army_peak, army_end, structures_end;
    int32_t credits_start, credits_end, spent, leadership, efficiency;
} EM_DebriefRow;
typedef struct EM_Debrief {
    EM_DebriefRow row[8];
    int nrows;
    int games;        /* kills[] columns in use, 1..4; the last is this match          */
    int win, decided; /* this machine won; somebody was beaten (0: the match was left) */
    int seconds;      /* the match's length                                            */
    int net;          /* a network match                                               */
    char mapname[48];
    char ended[96];   /* why it ended, when it broke rather than finished              */
    int killed, razed, lost, harvested;   /* this machine's commander, this match: harvested
                                             is the ore or tiberium it brought in        */
    int score;        /* this machine's score: units x2 + structures x5 + harvest / 10 */
    int place;        /* where that put it, 1 first; 0 when no row is this machine's   */
    int places;       /* how many commanders the place is counted among                */
    int page;         /* EM_PAGE_*, the page on show                                   */
    int series;       /* 0..EM_SERIES-1, the graph's series                            */
    int samples;      /* timeline samples held, 0..EM_SAMPLES                          */
    int sample_ticks; /* engine ticks between two samples (fifteen a second)           */
    int32_t timeline[8][EM_SERIES][EM_SAMPLES];   /* [row][series][sample]             */
} EM_Debrief;
void em_match_debrief(const EM_Debrief *d, int pressed);
/* The debrief's result line under the table, into buf: why the match ended when it broke
   (answers 0), "Your score: 1304 - 1st of 4" when this machine has a row (1), and the
   plain "Final standings" when it has none (2). em_match_debrief draws exactly this text. */
int em_match_debrief_result(const EM_Debrief *d, char *buf, int buflen);
typedef struct {
 int category, house, selected, top, count, detail;
 const char **names;
 const char *name, *code, *brief, *armour, *strong, *weak, *unlocks;
 int cost, hp, damage, speed, rof, range256, model_available;
 /* Red Alert: the type's 1996 cameo (RGBA), drawn when it has no Enhanced model */
 const unsigned char *cameo; int cameo_w, cameo_h;
} EM_Database;
void em_database(const EM_Database *st);
int em_database_view(int *x, int *y, int *w, int *h);
/* The framebuffer rectangle of the panel last laid out (by em_viewport or em_draw), frame
   art included: the box a player sees, which is not the 1995 dialog's rectangle. 0 when no
   panel has been laid out. */
int em_panel_rect(int *x, int *y, int *w, int *h);

#ifdef __cplusplus
}
#endif
#endif
