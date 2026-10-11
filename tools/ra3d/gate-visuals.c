/* Verify the shared Visuals menu refuses N64 only when RA3D requests it. */
#include "../../game/dosopt.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    DOPT_State state;
    int x, y, w, h;
    memset(&state, 0, sizeof state);
    state.page = DOPT_PAGE_VISUALS;
    if (dopt_item_disabled(&state, DOPT_V_N64)) return 1;
    state.vis.mode = DOPT_MODE_N64;
    state.advTab = DOPT_MODE_N64;
    dopt_set_n64_unavailable(&state, 1);
    if (state.vis.mode != DOPT_MODE_CLASSIC || state.advTab != DOPT_MODE_CLASSIC)
        return 2;
    if (!dopt_item_disabled(&state, DOPT_V_N64)
        || dopt_item_disabled(&state, DOPT_V_CLASSIC)
        || dopt_item_disabled(&state, DOPT_V_ENHANCED)) return 3;
    if (!dopt_item_rect(&state, DOPT_V_N64, &x, &y, &w, &h)) return 4;
    if (dopt_hit_test(&state, x + w / 2, y + h / 2) == DOPT_V_N64) return 5;
    if (dopt_next_item(&state, DOPT_V_CLASSIC, 1) == DOPT_V_N64) return 6;
    state.page = DOPT_PAGE_ADVANCED;
    if (!dopt_item_disabled(&state, DOPT_A_TAB0 + DOPT_MODE_N64)) return 7;
    if (!dopt_item_rect(&state, DOPT_A_TAB0 + DOPT_MODE_N64, &x, &y, &w, &h))
        return 9;
    dopt_set_n64_unavailable(&state, 0);
    if (dopt_item_disabled(&state, DOPT_A_TAB0 + DOPT_MODE_N64)) return 8;
    printf("RA3D Visuals: N64 is drawn, disabled and skipped by mouse and keyboard\n");
    return 0;
}
