/* Native pixel-art Enhanced sidebar. CPU rasterisation works on both render tiers. */
#ifndef HUD_ENHANCED_H
#define HUD_ENHANCED_H
#define EH_WIDTH 245
#define EH_MAIN_X 61
#define EH_INNER_X 73
#define EH_MAIN_W 184
#define EH_MAX_ROWS 8
#define EH_MAX_H 693
#define EH_TOOL_X 19
#define EH_TOOL_Y 52
#define EH_TAB_Y 229
#define EH_GRID_SHIFT 66
#define EH_RADAR_W 160
#define EH_RADAR_H 144
#define EH_TRAVEL 245
/* The production queue strip: one card per item, 38x30 native units, the half-size
   cameo seated at (3,3), corners cut six units deep. */
#define EHQ_W 38
#define EHQ_H 30
#define EHQ_CUT 6
#define EHQ_CAMEO_X 3
#define EHQ_CAMEO_Y 3
#define EHQ_CAMEO_W 32
#define EHQ_CAMEO_H 24
enum { EHQ_BUILDING, EHQ_HOLD, EHQ_WAITING, EHQ_MORE };
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    H6_State base;
    int rows;
    int category_frame[4], category_hover[4];
    int tool_frame[3], tool_hover[3];
} EH_State;
int hud_enhanced_ok(const H6_Pack *p);
int hud_enhanced_height(int rows);
void hud_enhanced_bar(unsigned char *dst, const H6_Pack *p, const EH_State *s);
void hud_enhanced_tab(unsigned char *dst, const H6_Pack *p, const char *label,
                       int value, int frame, int wide);
void hud_enhanced_clock(unsigned char *rgba, float progress, int busy, int nod, float time);
void hud_enhanced_gray(unsigned char *rgba);
/* One queue card, EHQ_W*EHQ_H*4 bytes. cameo is the 64x48 RGBA cameo (NULL for EHQ_MORE,
   whose count is then printed as "+count"). */
void hud_enhanced_queue_card(unsigned char *dst, const H6_Pack *p, const unsigned char *cameo,
                             float progress, int state, int count, int nod, float time);
#ifdef __cplusplus
}
#endif
#endif
