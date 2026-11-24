/* The in-game map importer. File ownership and conversion stay with the app;
 * the shell owns browsing, worker lifetime and responsive progress presentation. */
#ifndef CNC_MAP_IMPORT_UI_H
#define CNC_MAP_IMPORT_UI_H
#ifdef __cplusplus
extern "C" {
#endif
#include "dosmenu.h"
struct DMS;
typedef struct DMS_MapImportInfo {
    char name[128], game[32], kind[32], theater[32], message[512], scenario[16];
    int width, height, starts, preview_w, preview_h;
    unsigned char preview[128 * 128 * 4];
} DMS_MapImportInfo;
typedef struct DMS_MapImportHooks {
    int (*inspect)(const char*, DMS_MapImportInfo*);
    int (*run)(const char*, DMS_MapImportInfo*, void (*progress)(void*, int, const char*), void*);
} DMS_MapImportHooks;
enum { MI_NONE = -1, MI_BACK = 1, MI_HOME, MI_PARENT, MI_LOCATION, MI_GO,
       MI_IMPORT, MI_PREVIOUS, MI_NEXT, MI_ROW_BASE = 100 };
#define MI_VISIBLE_ROWS 9
typedef struct DMS_MapImportState {
    char path[1024], location[1024], selected_file[256], message[512];
    char rows[MI_VISIBLE_ROWS][256];
    int row_directory[MI_VISIBLE_ROWS];
    int row_count, selected_row, top, total, focus, pressed;
    int busy, importing, ready, completed, progress;
    DMS_MapImportInfo info;
} DMS_MapImportState;
/* Returns 1 after a successful import is acknowledged; otherwise DMS_CANCEL/QUIT. */
int dms_import_map(struct DMS*, const DMS_MapImportHooks*);
/* Shared Classic drawing/hit contract; Enhanced uses the same action IDs/state. */
void mi_draw(DB_Surface*, const DB_Pack*, const DMS_MapImportState*);
int mi_hit_test(const DMS_MapImportState*, int x, int y);
int mi_item_rect(const DMS_MapImportState*, int item, int*, int*, int*, int*);
/* Hidden integration probe. A path selects a real file through the browser; no
 * import occurs unless `run` is set. The result follows the ordinary worker loop. */
void dms_import_probe(const char* path, const char* shotdir, int run);
#ifdef __cplusplus
}
#endif
#endif
