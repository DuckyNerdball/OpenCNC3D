/* ====================================================================================
 *  fx_input.h -- how the game is DRIVEN: the mouse scheme, which button drags the map,
 *  and the command keys. Included by fx_state.h, whose struct carries the live values,
 *  so they are written to and read from the same settings file as every other dial and
 *  the per-frame sync notices a change made in a menu exactly as it notices any other.
 *
 *  NO SDL HERE. fx_state.h is included by tools that never link SDL, so a key is held as
 *  an SDL2 keycode number. Those numbers are SDL's stable ABI: a printable key is its
 *  lower-case character, and every other key is its scancode with bit 30 set
 *  (SDLK_SCANCODE_MASK). The renderer turns them into names for the settings file through
 *  fx_keyname_hook / fx_keycode_hook (fx_state.h), so the file reads "key.stop S".
 *
 *  THE DEFAULTS ARE THE BINDINGS THE GAME SHIPPED WITH. The first ten rows are the keys
 *  that already answered a command; the rest are the further command keys the GPL engine
 *  source defines (OptionsClass in tiberiandawn/options.cpp: KeyNext N, KeyPrevious B,
 *  KeySelectView E, KeyRepair T, KeySell Y, KeyHome1 Home, KeyBookmark1..4 F9..F12), which
 *  answered nothing here before. That engine never offered rebinding in a menu; its
 *  Windows editions read the same table from a hand-edited INI section.
 *
 *  APPEND, NEVER RENUMBER: the order is the order of FxState::keybind, and the cfg keys
 *  ("stop", "scatter") are the file format.
 * ==================================================================================== */
#ifndef OPENCNC3D_FX_INPUT_H
#define OPENCNC3D_FX_INPUT_H

/* SDL2's SDLK_SCANCODE_MASK applied to a scancode: the keycode of a key with no printable
   character. Home is scancode 74, F9..F12 are 66..69 (SDL_scancode.h). */
#define FX_SDLK_SC(sc) ((int)((1u << 30) | (unsigned)(sc)))

/* THE MOUSE SCHEME (FxState::mouse_scheme).
     CLASSIC     the 1995 scheme: the left button selects AND orders, decided by the
                 engine's own Best_Object_Action; the right button cancels or deselects.
     LEFTHANDED  the same scheme with the two buttons exchanged at the event boundary.
                 A cfg written before the scheme existed says "swap_buttons 1", and that
                 loads as this.
     MODERN      the StarCraft scheme. The left button SELECTS and never orders: a click
                 takes the object under the pointer (Shift adds), a drag is the band, a
                 click on empty ground drops the selection, and a click on the radar
                 moves the camera. The right button ORDERS (move, attack, enter, deploy,
                 rally, on the map and on the radar) instead of cancelling; with nothing
                 selected it does nothing. A mode the pointer is in (placement, an armed
                 attack-move or special, repair, sell) still takes the left click, and
                 Ctrl and Alt still force fire and force move on it. Pressing a
                 control-group number twice in quick succession also centres the camera
                 on the group. */
enum { FX_MOUSE_CLASSIC = 0, FX_MOUSE_LEFTHANDED = 1, FX_MOUSE_MODERN = 2, FX_MOUSE_COUNT = 3 };

/* WHICH BUTTON DRAGS THE MAP (FxState::grab_button): grab the ground and pull it.
     MIDDLE  the default, as shipped.
     RIGHT   the button that cancels (the left one under the left-handed scheme). A press
             that travels grabs the map instead of pushing it; a press that does not is
             still that button's click.
     SIDE1 / SIDE2  the mouse's back and forward thumb buttons (SDL_BUTTON_X1 / X2).
   Only the chosen button drags. */
enum { FX_GRAB_MIDDLE = 0, FX_GRAB_RIGHT = 1, FX_GRAB_SIDE1 = 2, FX_GRAB_SIDE2 = 3, FX_GRAB_COUNT = 4 };

/* THE REBINDABLE COMMANDS. Control groups (the number row) are not here and stay fixed. */
enum {
    FXK_STOP = 0, FXK_SCATTER, FXK_GUARD, FXK_GUARD2, FXK_ATTACKMOVE, FXK_BASE,
    FXK_REBUILD, FXK_RADAR, FXK_CAMERA, FXK_NEXTMODE,
    FXK_NEXT, FXK_PREV, FXK_SELECTVIEW, FXK_REPAIR, FXK_SELL, FXK_CENTRE,
    FXK_VIEW1, FXK_VIEW2, FXK_VIEW3, FXK_VIEW4,
    FXK_COUNT
};

struct FxKeyDef {
    const char* key;      /* the settings file's name, after "key."            */
    const char* label;    /* what the Key Bindings page calls it                */
    int         code;     /* the default, an SDL2 keycode                       */
    int         editor;   /* 1: also answers while the map editor is open        */
};

static const struct FxKeyDef FX_KEYS[FXK_COUNT] = {
    { "stop",       "Stop",                  's', 1 },
    { "scatter",    "Scatter",               'x', 1 },
    { "guard",      "Guard",                 'g', 1 },
    { "guard2",     "Guard (second key)",    'z', 1 },
    { "attackmove", "Attack-move",           'a', 0 },
    { "base",       "Go to base",            'h', 0 },
    { "rebuild",    "Rebuild last",          'd', 0 },
    { "radar",      "Radar on or off",       'm', 1 },
    { "camera",     "Camera view",           'c', 1 },
    { "nextmode",   "Next graphics mode",    ' ', 0 },
    { "next",       "Next unit",             'n', 0 },
    { "prev",       "Previous unit",         'b', 0 },
    { "selectview", "Select all on screen",  'e', 0 },
    { "repair",     "Repair mode",           't', 0 },
    { "sell",       "Sell mode",             'y', 0 },
    { "centre",     "Centre on selection",   FX_SDLK_SC(74), 0 },
    { "view1",      "Camera bookmark 1",     FX_SDLK_SC(66), 0 },
    { "view2",      "Camera bookmark 2",     FX_SDLK_SC(67), 0 },
    { "view3",      "Camera bookmark 3",     FX_SDLK_SC(68), 0 },
    { "view4",      "Camera bookmark 4",     FX_SDLK_SC(69), 0 },
};

static const char* const FX_MOUSE_NAME[FX_MOUSE_COUNT] = { "Classic", "Left-handed", "Modern" };
static const char* const FX_GRAB_NAME[FX_GRAB_COUNT] = { "Middle", "Right", "Side 1", "Side 2" };

#endif
