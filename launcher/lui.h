/*
 * lui.h -- the launcher's widgets, drawn with the game's own 1995 DOS primitives.
 *
 * There is no new look here and that is the point. Everything below composes
 * game/dosbar.c and menu/dosmenu.c: the same 8-bit 320x200 surface, the same
 * Draw_Box bevels out of dialog.cpp's ButtonColorsClassic table, the same 4bpp
 * font printer, the same green ramp out of TITLE.CPS's palette. The launcher is
 * therefore a C&C dialog rather than a program that resembles one, and an empty
 * module tile is the engine's real BOXSTYLE_GREEN_DIS_RAISED (dm_green_button's
 * disabled arm) rather than a dimmed green.
 *
 * What this file adds is only what a main menu never needed: a scrolling text
 * panel, a progress gauge, a checkbox, a small icon button, and the scaled blit a
 * picture tile is drawn with.
 */

#ifndef LUI_H
#define LUI_H

#include "dosmenu.h" /* which includes dosbar.h: DB_Surface, DB_Pack, DB_Font */

/* ------------------------------------------------------------------------ *
 * Buttons.
 *
 * A table-driven button rather than a widget tree, for the same reason
 * dosmenu.c uses one: a new button is one row, and the geometry, hit test,
 * keyboard walk and draw all read the row instead of testing for an item.
 * ------------------------------------------------------------------------ */

typedef struct
{
    int x, y, w, h;
    const char *label;
    int disabled; /* GadgetClass::IsDisabled: drawn, never selectable */
    /* Not on this screen at all: neither drawn nor hit tested. The launcher's one
     * dialog now shows more than one screen in the same well, and a button that
     * belongs to one of them has to be absent from the others rather than merely
     * greyed. Absent is not the same as disabled: a disabled button is a promise
     * that the thing exists and cannot be done now, which is a lie about a control
     * that is not on the screen you are looking at. */
    int hidden;
    /* WHAT THE ROW IS: a push button, a checkbox, or the small icon button. One table
     * still, so a checkbox is walked by the keyboard, hit by the mouse and skipped
     * while hidden exactly as a button is, and zero is a plain button, which is what
     * every row that predates the other two already holds. */
    int kind;
    int checked; /* a checkbox's tick                                        */
    /* A checkbox's second column, printed against the row's right edge: what the
     * tick will cost or what it already is ("428.5 MB", "INSTALLED"). */
    const char *note;
} LUI_Button;

#define LUI_KIND_BUTTON 0
#define LUI_KIND_CHECK 1
#define LUI_KIND_GEAR 2

/* `hot` is the keyboard highlight (menus.cpp:755 Turn_On), `pressed` the mouse. */
void lui_button_draw(DB_Surface *s, const DB_Font *grad, const LUI_Button *b, int pressed,
                     int hot);
int lui_button_hit(const LUI_Button *b, int mx, int my);

/* ------------------------------------------------------------------------ *
 * A checkbox row.
 *
 * The 1995 toolkit has no checkbox, and the game's own menus already settled what
 * one looks like here: menu/dosmp.c's mp_check, a 9 pixel well in the dialog's
 * shadow green with the dark inset line along its top and left, and a light green
 * square inside it when ticked. The word beside it is printed in the button face,
 * medium green, bright when the highlight is on the row, which is how a live
 * gadget is marked everywhere else (menus.cpp:755 Turn_On).
 *
 * The whole row is the control, not only the box: b->x..b->x+b->w is what the
 * mouse hits, so the label can be clicked as it can in every other dialog.
 * ------------------------------------------------------------------------ */

#define LUI_CHECK_BOX 9   /* mp_check's MP_HIDE_BOX */
#define LUI_CHECK_TEXT 13 /* the word, from the box's left edge */

void lui_check_draw(DB_Surface *s, const DB_Font *grad, const DB_Font *f6,
                    const LUI_Button *b, int pressed, int hot);

/* ------------------------------------------------------------------------ *
 * The gear: the one control drawn as a picture rather than a word.
 *
 * A raised green button like the rest, with an 11x11 gear printed on it in the ink
 * a label would take: medium green, bright under the highlight or the mouse, and
 * flat CC_GREEN on the grey slab when disabled. It is a bitmap in this file rather
 * than a shape in the pack, because a picture in the data would leave every pack
 * baked before it without one.
 * ------------------------------------------------------------------------ */

#define LUI_GEAR_PX 11

void lui_gear_draw(DB_Surface *s, const LUI_Button *b, int pressed, int hot);

/* Any row of the table, by its kind: one call in the draw loop for all three. */
void lui_control_draw(DB_Surface *s, const DB_Font *grad, const DB_Font *f6,
                      const LUI_Button *b, int pressed, int hot);

/* ------------------------------------------------------------------------ *
 * An inset panel: the framed well a text block sits in.
 *
 * dialog.cpp:159-161, BOXSTYLE_GREEN_BORDER: a black fill with one green
 * rectangle inset a pixel, no bevel. It is the style the main menu's own dialog
 * is drawn in, so a panel inside that dialog reads as part of the same object
 * rather than as a widget stuck onto it.
 * ------------------------------------------------------------------------ */

void lui_panel(DB_Surface *s, int x, int y, int w, int h);

/* ------------------------------------------------------------------------ *
 * A wrapped, scrollable text block.
 *
 * Built once from a blob of text, then drawn as a window onto it. The changelog
 * is markdown, so the small amount of markdown it actually uses is recognised
 * and given a colour rather than printed as punctuation: `## ` is a version
 * heading, `### ` a category, `- ` a bullet. Anything else is body text. HTML
 * comments are dropped, which is how the `<!-- discord: ... -->` marker that
 * rides on every version heading stays off the screen.
 *
 * Nothing is clipped silently: a line too wide for the panel is word-wrapped,
 * and a word too long to wrap is broken rather than run off the edge.
 * ------------------------------------------------------------------------ */

#define LUI_LINE_HEAD 0 /* ## a version heading   */
#define LUI_LINE_CAT 1  /* ### a category heading */
#define LUI_LINE_BULLET 2
#define LUI_LINE_BODY 3
#define LUI_LINE_BLANK 4

typedef struct
{
    char *text;         /* owned */
    unsigned char kind; /* LUI_LINE_*  */
    unsigned char cont; /* a wrapped continuation, so a bullet is not re-marked */
} LUI_Line;

typedef struct
{
    LUI_Line *lines;
    int count, cap;
    int top;     /* first visible line          */
    int visible; /* rows the panel can show     */
} LUI_Text;

/* `width` is the pixel width available for the widest line. Returns 0 and leaves
 * an empty block if the text is NULL, which is a legitimate state: it is what a
 * launcher shows before it has read a changelog. */
int lui_text_build(LUI_Text *t, const DB_Font *f, const char *blob, int width);
void lui_text_free(LUI_Text *t);
void lui_text_scroll(LUI_Text *t, int delta);
void lui_text_draw(DB_Surface *s, const DB_Font *f, const LUI_Text *t, int x, int y, int w,
                   int h);

/* The scrollbar beside the panel. Drawn only when there is more text than fits,
 * because a full-height thumb that can never move is furniture, not information. */
void lui_scrollbar_draw(DB_Surface *s, const LUI_Text *t, int x, int y, int w, int h);

/* ------------------------------------------------------------------------ *
 * A progress gauge.
 *
 * The engine's own is GaugeClass (gauge.cpp): a BOXSTYLE_DOWN well with a filled
 * bar in it. `frac` is thousandths so a caller with byte counts does not have to
 * carry a float, and -1 means "working, length unknown", which draws a barber
 * pole rather than a lie about how far along it is.
 * ------------------------------------------------------------------------ */

void lui_gauge_draw(DB_Surface *s, int x, int y, int w, int h, int frac, int phase);

/* ------------------------------------------------------------------------ *
 * Text helpers.
 * ------------------------------------------------------------------------ */

void lui_print(DB_Surface *s, const DB_Font *f, const char *text, int x, int y,
               unsigned char colour);
void lui_print_centered(DB_Surface *s, const DB_Font *f, const char *text, int cx, int y,
                        unsigned char colour);
void lui_print_right(DB_Surface *s, const DB_Font *f, const char *text, int rx, int y,
                     unsigned char colour);
/* Over the title art rather than over a dialog: classes 2 and 3 become BLACK so
 * the letters keep an outline (menus.cpp:814 TPF_FULLSHADOW). */
void lui_print_shadowed(DB_Surface *s, const DB_Font *f, const char *text, int x, int y,
                        unsigned char colour);

/* A short paragraph, word wrapped to `w` pixels and printed a row at a time from
 * `y`, rows 7 apart as the changelog's are; `centered` centres each row on x + w/2.
 * At most `max_rows` rows (0 for no limit): the last one allowed carries whatever
 * is left, cut with "..." if it still does not fit, so a long message stops at the
 * edge of the space it was given instead of running into the next control.
 * Returns the number of rows, and with a NULL surface only counts them, so a
 * caller can measure a sentence before deciding where the next thing goes. Nothing
 * is allocated: this is for a few lines drawn every frame, where the changelog
 * panel's built-once text block would be the wrong tool. */
int lui_print_wrapped(DB_Surface *s, const DB_Font *f, const char *text, int x, int y,
                      int w, unsigned char colour, int centered, int max_rows);

/* As lui_print, but cut to `maxw` pixels with "..." when it does not fit, so a long
 * line stops at the edge it was given rather than running across a border. */
void lui_print_fit(DB_Surface *s, const DB_Font *f, const char *text, int x, int y,
                   int maxw, unsigned char colour);

/* ------------------------------------------------------------------------ *
 * A scaled blit out of a shape the pack already holds.
 *
 * Nearest sampling, integers only, and palette index 0 transparent exactly as the
 * shape blitter treats it. It exists so a tile can show a piece of art at a size
 * the pack does not carry: the C&C tile draws the emblem off the 320x200 title
 * plate, which is the one copy of that artwork every install is certain to have.
 * A second, smaller copy baked into the pack would put the picture in the data
 * instead of in the program, and every install made before that day would then
 * have an empty tile.
 * ------------------------------------------------------------------------ */

void lui_blit_scaled(DB_Surface *s, const DB_Shape *sh, int frame, int sx, int sy, int sw,
                     int sh_h, int dx, int dy, int dw, int dh);

/* A plus sign, drawn as two filled bars rather than printed as a character. The
 * 6 point '+' is four pixels across; this has to read as the control you press to
 * add something. `arm` is the distance from the centre to either end. */
void lui_plus(DB_Surface *s, int cx, int cy, int arm, int thick, unsigned char colour);

/* A one pixel rectangle just inside a box, for the tile you are on. The engine
 * marks a live gadget by brightening its text (menus.cpp:755 Turn_On); a picture
 * tile has no text to brighten, so the frame carries it instead. */
void lui_outline(DB_Surface *s, int x, int y, int w, int h, unsigned char colour);

#endif /* LUI_H */
