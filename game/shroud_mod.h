/* ==================================================================================
 *  SHROUD + HEALTH BARS module                                        shroud_mod.h
 *
 *  Two independent pieces, both driven by the brain's own state and by numbers read
 *  out of the engine source rather than invented:
 *
 *  1. SHROUD. GAME_STATE_SHROUD (dllinterface.h:856, CNCShroudStruct) is fetched once
 *     per engine tick and drawn as flat ground quads in the tactical view:
 *        - unexplored cells (IsMapped == false)             -> opaque black
 *        - mapped but not currently seen (IsVisible false)  -> darkened, blended
 *     The entry grid the DLL exports carries no origin or pitch of its own. It is
 *     EXACTLY the rectangle GAME_STATE_STATIC_MAP reports in MapCellX/Y/Width/Height:
 *     both exports start from the engine's playable rect (Map.MapCellX..) and apply
 *     the identical grow-by-one steps (compare dllinterface.cpp:2863..2880 for the
 *     static map with dllinterface.cpp:5496..5513 for the shroud), so the already-
 *     grown static-map rect must be handed to shroud_init AS IS. Growing it again
 *     was this module's first bug; the Count guard below caught it: shroud_fetch()
 *     refuses the snapshot (and says so once) if the exported Count disagrees with
 *     rect width x height, because a silently mis-shaped grid would paint the shroud
 *     one row off and look almost right.
 *
 *     Semantics of the two dark states follow the DOS original: units standing in a
 *     mapped-but-unseen cell stay bright (Tiberian Dawn has no unit fog), so the dark
 *     quad is drawn at ground level with depth test ON and units drawn earlier win.
 *     Unexplored cells hide everything: the OBJECT PASSES must skip objects whose
 *     centre cell reports shroud_cell_hidden(), because a ground-level black quad
 *     cannot occlude a mesh standing on top of it.
 *
 *  2. HEALTH BARS. DOS-style bar drawn above an object, geometry and colours from
 *     TechnoClass::Draw_It (tiberiandawn/techno.cpp):
 *        - shown when: Strength > 0 and (selected OR damaged)     (techno.cpp:1069)
 *          [engine gates the damaged case on Special.HealthBarDisplayMode ==
 *           HB_DAMAGED; vanilla's default is HB_SELECTED (special.h:75). Which of the
 *           two applies is a RUNTIME mode here rather than a compile-time one, read
 *           from the F5 dial hb_mode, and HB_MODE_SELECTED is its default -- the engine's
 *           own. The enum
 *           beside hb_should_show carries the reasoning and the third position, off.]
 *        - ratio = Cardinal_To_Fixed(MaxStrength, Strength)       (object.cpp:1707)
 *                = (Strength << 8) / MaxStrength                  (common/misc.cpp:18)
 *        - colour: LTGREEN; YELLOW when ratio < 0x7F; RED when ratio < 0x3F
 *                                                                 (techno.cpp:1091..1096)
 *        - fill width: Fixed_To_Cardinal(interior, ratio), bounded [1, interior]
 *                                                                 (techno.cpp:1086..1088)
 *        - shape: 4-row outlined box, rows 1..2 are the fill      (techno.cpp:1080..1097)
 *     Colour values are the fixed first-16 palette slots (common/wwstd.h:198 ColorType:
 *     LTGREEN=4, YELLOW=5, RED=8, BLACK=12), taken from the shipped TEMPERAT.PAL /
 *     DESERT.PAL (identical in the fixed range) at the engine's canonical 6-to-8 bit
 *     scaling, v << 2:  LTGREEN (84,252,84)  YELLOW (252,252,84)  RED (168,0,0).
 *
 *  GL 1.1 only: immediate mode quads, no textures, no extensions. Plain C (C89-ish),
 *  compiles inside a C++ translation unit the way cnc_sidebar.h does. Include AFTER
 *  dllinterface.h (needs GAME_STATE_SHROUD, CNCShroudStruct, MAP_MAX_CELL_WIDTH) and
 *  after the GL header.
 *
 *  The brain's ShadowIndex is retained for the DOS Classic path. Its original
 *  SHADOW.SHP frame is drawn over visible-but-unmapped edge cells. The 3D
 *  shroud still uses its own soft corner coverage and does not use those frames.
 *  Other limitations:
 *    - Cloak: the engine also hides the bar for cloaked objects (Is_Cloaked,
 *      techno.cpp:1069). The object dump carries no cloak flag, so a cloaked stealth
 *      tank would show a bar. No cloaked unit exists in the prototype missions.
 *    - The engine's one-off vertical nudges (infantry -6, barracks -5, techno.cpp
 *      1060..1066) are the caller's business since only the caller knows object kind;
 *      hb_draw_one() takes the final screen-space anchor.
 * ================================================================================== */

#ifndef SHROUD_MOD_H
#define SHROUD_MOD_H

/* ---------------------------------------------------------------------------------- *
 *  Shroud state
 * ---------------------------------------------------------------------------------- */

#define SHROUD_CLEAR  0   /* mapped and currently visible: draw nothing */
#define SHROUD_DARK   1   /* mapped, not visible now: darkened ground   */
#define SHROUD_HIDDEN 2   /* unexplored (or off the exported grid): opaque black */

/* Same signature as CNC_Get_Game_State, so the caller hands its dlsym'd pointer in. */
typedef bool (*shroud_getstate_fn)(GameStateRequestEnum, uint64, unsigned char*, unsigned int);
/* The renderer's own per-cell view test (cnc_eyes cell_shown), int for C linkage. */
typedef int (*shroud_cellshown_fn)(int cellx, int celly);
/* Narrows the cell range [*x0,*x1) x [*y0,*y1) to the cells the camera can see. */
typedef void (*shroud_viewrect_fn)(int* x0, int* y0, int* x1, int* y1);

static int g_shroudOn = 1;   /* master switch; CLI flips it */

/* The exported entry grid: origin and size, derived in shroud_init and REQUIRED to
   match the DLL's Count on every fetch. */
static int g_shroudGX = 0, g_shroudGY = 0, g_shroudGW = 0, g_shroudGH = 0;
static int g_shroudValid = 0;          /* a good snapshot is in g_shroudCell */
/* Set whenever the cell grid changes, cleared when the per-corner coverage averages are
   rebuilt from it (shroud_build_corners). The TERRAIN pass reads those corners too now,
   and it runs BEFORE the blanket pass, so building them can no longer be a side effect
   of drawing the blanket or the ground would light itself from the previous frame's
   shroud. */
static int g_shroudCornersDirty = 1;
static int g_shroudWhined = 0;         /* count-mismatch reported once, not per frame */

/* ---------------------------------------------------------------------------------- *
 *  KEEPING WHAT THE SHROUD DERIVES UNTIL THE SHROUD CHANGES
 *
 *  The engine's shroud moves at most once a tick, and in most ticks it does not move at
 *  all. What is drawn from it (the corner field, the blanket's vertices, the Enhanced
 *  fog field and its texture, the air's visibility texture, the shroud term of the mesh
 *  batch keys, the ground patches' colours) used to be worked out again every frame
 *  anyway, the blanket twice a frame under Enhanced. Each is now built when what it is
 *  made of changes, and kept otherwise; a frame redoes only the view test, which moves
 *  with the camera. The blanket's cells and the ground patches are worked out one by one
 *  as a frame first draws them, never the ones off screen, so a frame in which the
 *  shroud moves does no more of that work than a frame did before anything was kept. A
 *  kept copy holds the very floats the per-frame code made, made by the same code, so
 *  the picture does not change by a bit.
 *
 *  g_shroudFieldGen is the shroud's own part of every such key. It moves when the
 *  per-cell opacity the corner field is averaged from changes, and only then: a fetched
 *  snapshot equal to the last one, or a mode switch that lands where it started, leaves
 *  it where it was.
 *
 *  OPENCNC3D_SHROUD_CACHE, read once:
 *     1 (or unset)  keep and reuse: the default;
 *     0             build everything again every frame, as before the copies existed;
 *     verify        keep and reuse, and every frame build everything again from scratch
 *                   and compare it bit for bit with what is kept. A difference prints
 *                   SHROUDCACHE|verify|mismatch|<what>|<count> and is counted, and the
 *                   shroudcache script verb prints the counts. Slow; for gates and checks.
 * ---------------------------------------------------------------------------------- */
static int g_shroudCacheMode = -1;
static int shroud_cache_mode(void)
{
    if (g_shroudCacheMode < 0) {
        const char* e = getenv("OPENCNC3D_SHROUD_CACHE");
        g_shroudCacheMode = (e && !strcmp(e, "verify")) ? 2 : (e && !strcmp(e, "0")) ? 0 : 1;
        if (g_shroudCacheMode != 1)
            printf("SHROUDCACHE|mode|%s\n", g_shroudCacheMode ? "verify" : "off");
    }
    return g_shroudCacheMode;
}
/* What the copies did, for the shroudcache verb: built is a copy made again because its
   inputs moved, kept is one reused as it stood. */
struct ShroudCacheStats {
    unsigned long fieldBuilt, fieldKept;     /* the corner field, on each rebuild request */
    unsigned long blanketBuilt, blanketKept; /* the soft blanket's kept cells: forgotten, or
                                                still good, per draw */
    unsigned long fogBuilt, fogKept;         /* the Enhanced fog field, per frame */
    unsigned long visBuilt, visKept;         /* the air's visibility texture, per request */
    unsigned long sigReset;                  /* the mesh shroud-key memo, emptied */
    unsigned long mottleBuilt, mottleKept;   /* the ground patches, per frame */
    unsigned long checks, mismatches;        /* verify: comparisons made, and failed */
    unsigned long blanketCells, mottleCells; /* blanket cells and ground patches worked out */
    unsigned long blanketView, blanketRange; /* the last draw's cells on screen, and in all */
};
static ShroudCacheStats g_shroudCache;
static void shroud_cache_mismatch(const char* what, long count)
{
    g_shroudCache.mismatches++;
    if (g_shroudCache.mismatches <= 16)
        printf("SHROUDCACHE|verify|mismatch|%s|%ld\n", what, count);
}
static unsigned g_shroudFieldGen = 1;
/* A key is a list of words, compared whole. Floats go in by their bits. */
static unsigned shroud_key_bits(float f) { unsigned u; memcpy(&u, &f, sizeof u); return u; }

#include "c3d_ceiling.h"

/* One byte per grid entry, SHROUD_* values, sized from the RENDERER's ceiling like every
   other grid in this program. It used to size from the engine's MAP_MAX_CELL_WIDTH, which
   is a different number that moves for different reasons, and that is how the shroud came
   to have a ceiling of its own that nothing named. */
static unsigned char g_shroudCell[C3D_MAP_MAX * C3D_MAP_MAX];
/* Optional engine extension: a three-second firing reveal fades during its last
   second, before the native exploration bits return to their saved state. */
typedef int (*shroud_firestate_fn)(int* rows,int capacity);
static shroud_firestate_fn g_shroudFireState = NULL;
static float g_shroudFireAlpha[C3D_MAP_MAX * C3D_MAP_MAX];
static std::vector<float> g_shroudFireNext;
static std::vector<int> g_shroudFireRows;
static std::vector<int> g_shroudFireFading; /* x,y pairs for Classic's overlay */
static float shroud_dark_alpha(void);

/* Original partial fog frame from DisplayClass::Cell_Shadow, -1 when absent. */
static signed char g_shroudShadow[C3D_MAP_MAX * C3D_MAP_MAX];

/* THE WORLD-GRID CORNER DIMS, (packW+1) x (packH+1): the per-corner coverage field
   further down covers the whole PACK grid, which stopped being a constant 64x64 when
   the PKF map-size format landed. This header is included above the host's own grid
   seam (C3D_MAP_MAX / g_gridW in cnc_eyes.cpp), so it cannot read those names; the
   host calls this setter at the two places the grid changes (load_pack's commit and
   the mission-state reset). Storage stays static at the ceiling -- the same
   MAP_MAX_CELL_WIDTH the two buffers above already size from -- and every corner
   INDEX strides by these. */
#define SHROUD_CORN_MAX C3D_CORN_MAX
static int g_shroudCornW = 65, g_shroudCornH = 65;
static int g_shroudClampWhined = 0;
static void shroud_set_world_grid(int cornW, int cornH)
{
    /* THE CLAMP SAYS SO NOW, and it used to be the only world-grid gate in the host that
       did not. load_pack refuses a pack whose dims exceed the ceiling and prints why;
       edit_set_world_grid refuses a world the same way and prints why; this one quietly
       took the smaller number and carried on, which on a map larger than the ceiling
       produces a shroud covering part of the board and no statement anywhere that
       anything was dropped.
       It is the SAME ceiling as everything else now (c3d_ceiling.h). Until 27 Aug 2026
       it was a second one: this header is included above the renderer's grid seam and so
       sized itself from the ENGINE's map constant, which meant raising the renderer's
       ceiling moved the rest of the program and left the shroud behind, silently.
       It still clamps rather than refusing, because the storage above is static at the
       ceiling and there is nothing safe to do with a larger grid until it is heap-sized.
       Once, not per call: the setter runs on every pack load and every mission reset. */
    if (cornW > SHROUD_CORN_MAX || cornH > SHROUD_CORN_MAX) {
        if (!g_shroudClampWhined) {
            g_shroudClampWhined = 1;
            fprintf(stderr,
                    "*** shroud: asked for a %dx%d corner grid and this build's storage "
                    "stops at %dx%d, so the shroud will cover only part of this map. The "
                    "shroud's ceiling is the BRAIN's MAP_MAX_CELL_WIDTH, separate from the "
                    "renderer's own C3D_MAP_MAX; both have to move together.\n",
                    cornW, cornH, SHROUD_CORN_MAX, SHROUD_CORN_MAX);
        }
    }
    if (cornW < 2) cornW = 2; else if (cornW > SHROUD_CORN_MAX) cornW = SHROUD_CORN_MAX;
    if (cornH < 2) cornH = 2; else if (cornH > SHROUD_CORN_MAX) cornH = SHROUD_CORN_MAX;
    g_shroudCornW = cornW;
    g_shroudCornH = cornH;
    g_shroudCornersDirty = 1;          /* the field's shape changed with its stride */
}

/* Fetch buffer: header + max entries + the DLL's own 256-byte slack (dllinterface.cpp:
   5481 counts that slack against buffer_size, so it must really be there). */
static unsigned char g_shroudBuf[sizeof(CNCShroudStruct)
                                 + C3D_MAP_MAX * C3D_MAP_MAX
                                       * sizeof(CNCShroudEntryStruct)
                                 + 256];

/* The shroud grid is the STATIC MAP rect, verbatim: pass CNCMapDataStruct's
   MapCellX/Y/Width/Height (NOT the Original* fields). Call once after
   GAME_STATE_STATIC_MAP is read; the rect never changes mid-scenario. */
static void shroud_init(int mapX, int mapY, int mapW, int mapH)
{
    g_shroudGX = mapX;
    g_shroudGY = mapY;
    g_shroudGW = mapW;
    g_shroudGH = mapH;
    g_shroudValid = 0;
    g_shroudCornersDirty = 1;          /* the rim moved with the rect */
}

/* Undo shroud_init. The shroud owns no GL objects and no heap, only the snapshot
   grid and the "already whined" latch, so leaving a game and entering another has
   to clear both or mission two starts explored and stays silent about a grid
   mismatch it already reported for mission one. */
static void shroud_free(void)
{
    g_shroudGX = g_shroudGY = g_shroudGW = g_shroudGH = 0;
    g_shroudValid = 0;
    g_shroudWhined = 0;
    memset(g_shroudCell, 0, sizeof(g_shroudCell));
    memset(g_shroudFireAlpha,0,sizeof g_shroudFireAlpha);g_shroudFireFading.clear();
    memset(g_shroudShadow, -1, sizeof(g_shroudShadow));
    g_shroudCornersDirty = 1;          /* no rect, no rim, no snapshot */
}

/* Pull GAME_STATE_SHROUD and reduce each entry to a SHROUD_* byte. Player id 0 is
   correct in single player: Set_Player_Context ignores it for GAME_NORMAL
   (dllinterface.cpp:783 path). Returns 1 on a good snapshot. */
static int shroud_fetch(shroud_getstate_fn getstate)
{
    const CNCShroudStruct* sh;
    int i, n;

    if (!getstate || g_shroudGW <= 0)
        return 0;
    if (!getstate(GAME_STATE_SHROUD, 0, g_shroudBuf, (unsigned int)sizeof(g_shroudBuf)))
        return 0;

    sh = (const CNCShroudStruct*)g_shroudBuf;
    n = g_shroudGW * g_shroudGH;
    if (sh->Count != n) {
        if (!g_shroudWhined) {
            fprintf(stderr,
                    "SHROUD: DLL exported %d entries, expected %dx%d=%d "
                    "-- grid arithmetic out of step with Get_Shroud_State, "
                    "shroud stays OFF\n",
                    sh->Count, g_shroudGW, g_shroudGH, n);
            g_shroudWhined = 1;
        }
        g_shroudValid = 0;
        return 0;
    }
    /* The corner field is marked for a rebuild only when a cell's state really moved, or
       when the snapshot becomes valid: most ticks hand back the same shroud. The partial
       fog frames are read by the Classic view directly and are not part of the field. */
    int changed = !g_shroudValid || shroud_cache_mode() == 0;
    g_shroudFireNext.resize(n);std::fill(g_shroudFireNext.begin(),g_shroudFireNext.end(),0.0f);
    g_shroudFireFading.clear();
    const int fireCount=g_shroudFireState?g_shroudFireState(NULL,0):0;
    if(fireCount>0&&fireCount<=C3D_MAP_MAX*C3D_MAP_MAX) {
        g_shroudFireRows.resize((size_t)fireCount*4);
        const int got=g_shroudFireState(g_shroudFireRows.data(),fireCount);
        if(got==fireCount)for(int fi=0;fi<fireCount;fi++) {
            const int *row=&g_shroudFireRows[(size_t)fi*4];
            const int x=row[0]-g_shroudGX,y=row[1]-g_shroudGY;
            if(x<0||y<0||x>=g_shroudGW||y>=g_shroudGH||row[2]<1||row[2]>=15)continue;
            const float target=row[3]==SHROUD_HIDDEN?1.0f:shroud_dark_alpha();
            g_shroudFireNext[(size_t)y*g_shroudGW+x]=target*(15-row[2])/15.0f;
            g_shroudFireFading.push_back(row[0]);g_shroudFireFading.push_back(row[1]);
        }
    }

    for (i = 0; i < n; i++) {
        /* ASSEMBLY CHANGE (cursors x shroud reconciliation, measured on SCG01EB):
           the engine reports edge-of-reveal cells as IsVisible=1 IsMapped=0 with a
           partial ShadowIndex, and its own click path treats them as attackable
           ground (the cursors module proved an attack order into such a cell sets
           TarCom). The original classification here (!IsMapped -> HIDDEN) painted
           that ring opaque black and culled objects the engine considers visible,
           so the cursor promised an attack the renderer refused to show. Now
           IsVisible decides black-and-cull, and any half-known cell (visible
           edge ring, or classic mapped-but-unseen fog if a future brain exports
           it) takes the blended DARK pass. Deep shroud is unchanged. NOTE: if fog
           (IsMapped && !IsVisible) ever becomes reachable, units standing there
           will need a cull bit DARK cannot carry today. */
        const CNCShroudEntryStruct* e = &sh->Entries[i];
        const unsigned char state = (unsigned char)(e->IsVisible
                                              ? (e->IsMapped ? SHROUD_CLEAR
                                                             : SHROUD_DARK)
                                              : (e->IsMapped ? SHROUD_DARK
                                                             : SHROUD_HIDDEN));
        g_shroudShadow[i] = (signed char)e->ShadowIndex;
        if(g_shroudFireAlpha[i]!=g_shroudFireNext[i])changed=1;
        g_shroudFireAlpha[i]=g_shroudFireNext[i];
        if (g_shroudCell[i] != state) changed = 1;
        g_shroudCell[i] = state;
    }
    g_shroudValid = 1;
    if (changed) g_shroudCornersDirty = 1;
    return 1;
}

/* SHROUD_* for a map cell. Off-grid is HIDDEN: the world beyond the exported rect has
   by definition never been seen. With no valid snapshot (fetch failed, or shroud off)
   everything reports CLEAR so the renderer degrades to exactly the old behaviour. */
/* THE RIM. The outermost ring of cells of the play rectangle, and
   everything beyond it, is shroud that nothing lifts: not the shroud switch, not
   --noshroud, not a revealed map after a match, not a unit standing on it. The
   director: "the furthermost right, left, top, down cells of every map in the entire
   game should always be covered by Shroud, EVEN if shroud / fog of war is turned off,
   and should never be able to be revealed." It is asked FIRST in every question below,
   ahead of every switch, which is what makes it unliftable. The one exception is the
   editor (g_shroudEditorOff), which must be able to paint the edge cells it hides.
   --norim / the `rim` verb exist so a gate can photograph the rule against its own
   absence. */
static int g_shroudRimOn = 1;
static int shroud_is_rim(int cellx, int celly)
{
    if (!g_shroudRimOn || g_shroudGW <= 0 || g_shroudGH <= 0) return 0;
    return cellx <= g_shroudGX || cellx >= g_shroudGX + g_shroudGW - 1
        || celly <= g_shroudGY || celly >= g_shroudGY + g_shroudGH - 1;
}
static int shroud_state_at(int cellx, int celly)
{
    int gx, gy;
    if (shroud_is_rim(cellx, celly))
        return SHROUD_HIDDEN;
    if (!g_shroudOn || !g_shroudValid)
        return SHROUD_CLEAR;
    gx = cellx - g_shroudGX;
    gy = celly - g_shroudGY;
    if (gx < 0 || gx >= g_shroudGW || gy < 0 || gy >= g_shroudGH)
        return SHROUD_HIDDEN;
    return (int)g_shroudCell[gy * g_shroudGW + gx];
}

/* The one question the object passes ask. */
/* THE EDITOR SEES EVERYTHING. A mission opens with most of its map shrouded, which is
   the point in play and useless in an editor: you cannot place a building on ground you
   cannot see, and the black is not a mistake anyone would recognise as shroud. The
   projection is left alone -- only whether it HIDES is overridden -- so pressing Play
   restores the mission's real fog exactly. */
static int g_shroudEditorOff = 0;

/* AND THE SAME OVERRIDE FOR A PLAYER WHO IS NO LONGER PLAYING. A match that has been
   decided shows everybody the whole map for a moment before the banner, and a player who
   surrendered or was defeated keeps seeing it, so they can watch the rest of the game out
   rather than stare at a black rectangle.
   
   PRESENTATION ONLY, and that is load bearing in a match: the engine dumps every object
   regardless of visibility, this renderer culls at draw time, and the desync hash is
   taken over that dump. Revealing the map therefore cannot part two worlds. The ENGINE's
   own unshroud (DEBUG_REQUEST_UNSHROUD) would, which is why it is not used here.
   
   Separate from g_shroudOn on purpose: that one also stops the per-tick snapshot being
   fetched, which is wrong for a spectator, whose world keeps moving underneath the
   override and must stay fresh. */
static int g_shroudRevealed = 0;

static void shroud_reveal(int on);

/* The blanket is drawn for the rim alone when the shroud proper is not in play. */
static int shroud_rim_only(void)
{
    return !g_shroudOn || !g_shroudValid || g_shroudRevealed;
}


static int shroud_shadow_at(int cellx, int celly)
{
    if (!g_shroudOn || !g_shroudValid || g_shroudEditorOff || g_shroudRevealed
        || shroud_is_rim(cellx, celly)) return -1;
    const int gx = cellx - g_shroudGX, gy = celly - g_shroudGY;
    if (gx < 0 || gx >= g_shroudGW || gy < 0 || gy >= g_shroudGH) return -1;
    return (int)g_shroudShadow[gy * g_shroudGW + gx];
}

static int shroud_cell_hidden(int cellx, int celly)
{
    if (g_shroudEditorOff) return 0;
    if (shroud_is_rim(cellx, celly)) return 1;      /* before the reveal: never lifted */
    if (g_shroudRevealed) return 0;
    return shroud_state_at(cellx, celly) == SHROUD_HIDDEN;
}

/* The corner coverage field is cached, so a reveal that only flipped the flag would show
   the old soft edges for one more frame. Dirty it here and the next frame rebuilds. */
static void shroud_reveal(int on)
{
    const int want = on ? 1 : 0;
    if (g_shroudRevealed == want) return;
    g_shroudRevealed = want;
    g_shroudCornersDirty = 1;
}

/* SYNTHETIC TEST ONLY: reclassify every CLEAR cell of the local snapshot as DARK so
   the blended pass can be photographed. Tiberian Dawn itself can never produce a
   mapped-but-unseen cell: DisplayClass::Map_Cell (display.cpp:1851) sets Mapped and
   Visible together, always, and only the DARKNESS crate clears them, together again
   (cell.cpp:2160). This mutates the local PROJECTION for one frame, never the engine;
   the next shroud_fetch overwrites it. */
static void shroud_test_darken(void)
{
    int i, n = g_shroudGW * g_shroudGH;
    if (!g_shroudValid)
        return;
    for (i = 0; i < n; i++)
        if (g_shroudCell[i] == SHROUD_CLEAR)
            g_shroudCell[i] = SHROUD_DARK;
    g_shroudCornersDirty = 1;
}

/* Totals for scripts and harnesses: measured evidence, not an impression. */
static void shroud_counts(int* hidden, int* dark, int* clear)
{
    int i, n = g_shroudGW * g_shroudGH;
    *hidden = *dark = *clear = 0;
    if (!g_shroudValid)
        return;
    for (i = 0; i < n; i++) {
        if (g_shroudCell[i] == SHROUD_HIDDEN)
            (*hidden)++;
        else if (g_shroudCell[i] == SHROUD_DARK)
            (*dark)++;
        else
            (*clear)++;
    }
}

/* How dark a mapped-but-unseen cell goes. The DOS original used translucency table
   entries for this job; a flat alpha is the GL 1.1 equivalent. */
static int shroud_fow_enhanced(void) { return fx_mode_of(&g_fx)==FX_MODE_ENHANCED; }
static float shroud_dark_alpha(void) { return shroud_fow_enhanced()?g_fx.fow_darkness:0.45f; }
#define SHROUD_DARK_ALPHA (shroud_dark_alpha())
static int g_shroudMaskPass=0;
static void shroud_colour(float a) {
    const float c=g_shroudMaskPass?1.0f:0.0f;
    glColor4f(c,c,c,a);
}
/* Quad heights: terrain is y=0, water -0.015, selection brackets 0.09. The shroud sits
   between ground and brackets, dark slightly under black so coincident edges cannot
   z-fight. */
#define SHROUD_DARK_Y  0.045f
#define SHROUD_BLACK_Y 0.050f

/* ASSEMBLY ADDITION (terrain x shroud): the blanket rides the PK9 heightfield.
   Drawn flat above lowered ground, the old y = 0 quads let the player see UNDER
   the shroud from a pitched camera (a bright band of hidden terrain at the screen
   edge). The host sets this to its terrain sampler; NULL keeps the flat plane. */
static float (*g_shroudGroundY)(float wx, float wz) = 0;
static float shroud_gy(float wx, float wz)
{
    return g_shroudGroundY ? g_shroudGroundY(wx, wz) : 0.0f;
}

/* Draw both shroud passes over cell range [x0,x1) x [y0,y1) (caller passes its drawn
   terrain range, i.e. map rect + margin). `shown` is the renderer's own view-clip test,
   NULL means draw every cell in range.

   GL contract: called inside the 3D scene AFTER every object pass. Leaves blend and
   texture state off, depth mask on, exactly as draw_frame's later passes expect.
   Depth test stays ON so explored-side buildings nearer the camera keep occluding the
   shroud behind them; depth WRITE is off so these quads never mask later overlays. */
static void shroud_draw(int x0, int y0, int x1, int y1, shroud_cellshown_fn shown)
{
    int x, y, s;

    if (!g_shroudRimOn && (!g_shroudOn || !g_shroudValid))
        return;                       /* the rim alone still draws, see shroud_is_rim */

    /* The same one-cell overhang the soft path takes. G17 compares these two paths
       against each other, so they have to agree on the range or the gate measures the
       difference between two clip rectangles instead of the seal it is meant to test. */
    if (x0 < -1) x0 = -1;
    if (y0 < -1) y0 = -1;
    if (x1 > g_shroudCornW) x1 = g_shroudCornW;   /* world cells + the one-cell ring */
    if (y1 > g_shroudCornH) y1 = g_shroudCornH;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_FALSE);

    /* Written through an OverlayArrays (overlay_arrays.h): immediate mode by default,
       or with the overlay_arrays dial one array draw a pass. Both passes are one flat
       colour each, set before the pass, so the batch carries positions only. */
    static OverlayArrays quads;
    quads.reset(false, false);

    /* Pass 1: darkened (blended) */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    shroud_colour(SHROUD_DARK_ALPHA);
    quads.begin(GL_QUADS);
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            if (shroud_state_at(x, y) != SHROUD_DARK)
                continue;
            if (shown && !shown(x, y))
                continue;
            quads.vertex((float)x, SHROUD_DARK_Y + shroud_gy((float)x, (float)y), (float)y);
            quads.vertex((float)x + 1.0f, SHROUD_DARK_Y + shroud_gy((float)x + 1.0f, (float)y), (float)y);
            quads.vertex((float)x + 1.0f, SHROUD_DARK_Y + shroud_gy((float)x + 1.0f, (float)y + 1.0f), (float)y + 1.0f);
            quads.vertex((float)x, SHROUD_DARK_Y + shroud_gy((float)x, (float)y + 1.0f), (float)y + 1.0f);
        }
    }
    quads.end();
    quads.flush();
    glDisable(GL_BLEND);

    /* Pass 2: unexplored (opaque black) */
    shroud_colour(1.0f);
    quads.begin(GL_QUADS);
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            s = shroud_state_at(x, y);
            if (s != SHROUD_HIDDEN)
                continue;
            if (shown && !shown(x, y))
                continue;
            quads.vertex((float)x, SHROUD_BLACK_Y + shroud_gy((float)x, (float)y), (float)y);
            quads.vertex((float)x + 1.0f, SHROUD_BLACK_Y + shroud_gy((float)x + 1.0f, (float)y), (float)y);
            quads.vertex((float)x + 1.0f, SHROUD_BLACK_Y + shroud_gy((float)x + 1.0f, (float)y + 1.0f), (float)y + 1.0f);
            quads.vertex((float)x, SHROUD_BLACK_Y + shroud_gy((float)x, (float)y + 1.0f), (float)y + 1.0f);
        }
    }
    quads.end();
    quads.flush();

    glDepthMask(GL_TRUE);
}

/* ---------------------------------------------------------------------------------- *
 *  Soft shroud (the N64 look)
 *
 *  The console does not draw per-cell black squares. Read out of the EU ROM
 *  (resident segment, RAM = ROM - 0x1000 + 0x80000400):
 *
 *    - draw_shroud_cell at ROM 0x4ACC4 (RAM 0x8004A0C4) takes (shapeIndex, cellX,
 *      cellY, ...), looks the shape up in a 16-byte-entry table at RAM 0x80099A38
 *      and skips the cell when entry+0xC (the art pointer) is null: per-cell EDGE
 *      SHAPE frames, the console's descendant of the DOS SHADOW.SHP frame set.
 *    - When the global at 0x80097A04 is set it reads a per-cell coverage byte via
 *      0x801F7B48(cx,cy) and stores alpha = ~byte & 0xFF into the node through the
 *      helper at RAM 0x80055FFC (ROM 0x56BFC), which CLAMPS TO 0xC8: the blended
 *      shroud sprite never exceeds 200/255 = 0.784 opacity. A CONTINUOUS alpha,
 *      not a bit.
 *    - The node scale is set to 0.25f (lui $a1,0x3e80 at ROM 0x4ADD0): the blob art
 *      is drawn scaled down through the RDP's bilinear filter, which is where the
 *      feathered gradient comes from.
 *    - The path then writes G_SETOTHERMODE_L words 0x00504240 / 0x00552230 into the
 *      display list (ROM 0x4AE08..0x4AE38): FORCE_BL | IM_RD render modes, i.e. a
 *      translucent blend over the framebuffer, depth-compared.
 *
 *  GL 1.1 translation: the RDP's "filtered blob sprite" becomes a VERTEX-FADED mesh.
 *  Each cell corner carries alpha = average of the four adjacent cells' opacity
 *  (HIDDEN 1.0, DARK 0.45, CLEAR 0.0; off-grid counts HIDDEN), the cell centre
 *  carries the cell's own opacity, and each boundary cell is drawn as a 4-triangle
 *  fan so Gouraud interpolation produces the gradient. Cells whose centre and all
 *  four corners are fully opaque are drawn as plain UNBLENDED black quads (the deep
 *  black interior; also the cheap path, no blending for the bulk of the map).
 *  No shaders, no textures, no extensions: glColor4f per vertex and one blend pass.
 * ---------------------------------------------------------------------------------- */

static int g_shroudSoft = 1;   /* 1 = soft (N64 look), 0 = hard per-cell squares */

/* Feather ceiling: the ROM's own number. The alpha helper at ROM 0x56BFC clamps the
   edge-cell blob to 0xC8 (200/255), so on the console the feather band keeps a faint
   ~22% terrain glimmer; the separate opaque pass owns the interior. The vertex fade
   applies the clamp PER VERTEX below: the centre and every corner not fully inside
   the shroud are held at the ceiling, while fully-interior corners stay 1.0, so the
   fan's shared edge with an opaque core quad interpolates 1.0 -> 1.0 and the glimmer
   saturates exactly at the core boundary. That is the ROM's ceiling without the step
   a blanket clamp would cut at that edge (r031 verifier minor: v0.3.0 shipped 1.0f
   here and faded fully to black, losing the glimmer). */
#define SHROUD_SOFT_MAXA 0.784f
#define SHROUD_SOFT_OPAQ 0.9999f   /* >= this counts as fully interior */

static float shroud_fire_alpha(int cellx,int celly) {
    if(!g_shroudOn||!g_shroudValid||g_shroudEditorOff||g_shroudRevealed)return 0.0f;
    const int x=cellx-g_shroudGX,y=celly-g_shroudGY;
    return x>=0&&y>=0&&x<g_shroudGW&&y<g_shroudGH?g_shroudFireAlpha[(size_t)y*g_shroudGW+x]:0.0f;
}
static float shroud_opacity(int cellx, int celly)
{
    if (shroud_is_rim(cellx, celly)) return 1.0f;
    if (g_shroudRevealed) return 0.0f;             /* rim-only: the rest is open */
    switch (shroud_state_at(cellx, celly)) {
        case SHROUD_HIDDEN: return 1.0f;
        case SHROUD_DARK:   return std::max(SHROUD_DARK_ALPHA,shroud_fire_alpha(cellx,celly));
        default:            return shroud_fire_alpha(cellx,celly);
    }
}

/* Corner (x,y) sits between cells (x-1,y-1) (x,y-1) (x-1,y) (x,y): the classic
   vertex-faded fog average. (packW+1) x (packH+1) corners cover the whole pack grid;
   the buffer holds the 129x129 ceiling and the live dims stride it (see
   shroud_set_world_grid at the top of this file). */
static float g_shroudCornerA[SHROUD_CORN_MAX * SHROUD_CORN_MAX];

/* The opacity of every cell a corner reads, cells -1 .. CornW-1 by -1 .. CornH-1, row
   by row at a stride of CornW+1. The field is a function of this grid and nothing else,
   so the grid is what says whether the field moved. */
static std::vector<float> g_shroudOpac;
static int g_shroudOpacW = 0, g_shroudOpacH = 0;
static void shroud_opacity_grid(std::vector<float>& op, int ow, int oh)
{
    op.resize((size_t)ow * oh);
    for (int y = -1; y < oh - 1; y++)
        for (int x = -1; x < ow - 1; x++)
            op[(size_t)(y + 1) * ow + (x + 1)] = shroud_opacity(x, y);
}

static void shroud_build_corners(void)
{
    int x, y;
    g_shroudCornersDirty = 0;
    if (shroud_cache_mode() == 0) {
        for (y = 0; y < g_shroudCornH; y++)
            for (x = 0; x < g_shroudCornW; x++)
                g_shroudCornerA[y * g_shroudCornW + x] =
                    0.25f * (shroud_opacity(x - 1, y - 1) + shroud_opacity(x, y - 1) +
                             shroud_opacity(x - 1, y)     + shroud_opacity(x, y));
        g_shroudFieldGen++;
        g_shroudCache.fieldBuilt++;
        return;
    }
    /* Each cell's opacity once instead of four times, then the same four-term average
       summed in the same order, which gives the same bits. Nothing moves, and the
       generation stays, when the grid is the one the field was last built from. */
    const int ow = g_shroudCornW + 1, oh = g_shroudCornH + 1;
    static std::vector<float> op;
    shroud_opacity_grid(op, ow, oh);
    if (ow == g_shroudOpacW && oh == g_shroudOpacH
        && !memcmp(op.data(), g_shroudOpac.data(), op.size() * sizeof(float))) {
        g_shroudCache.fieldKept++;
        return;
    }
    g_shroudOpac.swap(op);
    g_shroudOpacW = ow;
    g_shroudOpacH = oh;
    const float* o = g_shroudOpac.data();
    for (y = 0; y < g_shroudCornH; y++)
        for (x = 0; x < g_shroudCornW; x++)
            g_shroudCornerA[y * g_shroudCornW + x] =
                0.25f * (o[y * ow + x]       + o[y * ow + x + 1] +
                         o[(y + 1) * ow + x] + o[(y + 1) * ow + x + 1]);
    g_shroudFieldGen++;
    g_shroudCache.fieldBuilt++;
}

/* The corner field depends on which MODE built it (the shroud proper, or the rim
   alone), and the mode can change between frames without anyone dirtying the cache:
   --noshroud at boot, a reveal at match end, the rim verb in a script. */
static int g_shroudCornersMode = -1;
static void shroud_corners_sync(void)
{
    const int mode = (g_shroudRimOn << 2) | (shroud_rim_only() << 1) | (g_shroudRevealed ? 1 : 0);
    static float lastDark=-1.0f;
    if (g_shroudCornersDirty || mode != g_shroudCornersMode || lastDark!=SHROUD_DARK_ALPHA) {
        lastDark=SHROUD_DARK_ALPHA;
        shroud_build_corners();
        g_shroudCornersMode = mode;
    }
}

static float shroud_corner_a(int x, int y)
{
    if (x < 0) x = 0; else if (x >= g_shroudCornW) x = g_shroudCornW - 1;
    if (y < 0) y = 0; else if (y >= g_shroudCornH) y = g_shroudCornH - 1;
    return g_shroudCornerA[y * g_shroudCornW + x];
}

/* Verify, once a frame: the field worked out again from the live shroud, the way the
   per-frame build made it, against the kept one. This is also the check that every
   writer of the shroud still marks the field for a rebuild. */
static void shroud_cache_check_field(void)
{
    if (shroud_cache_mode() != 2) return;
    shroud_corners_sync();
    g_shroudCache.checks++;
    long bad = 0;
    for (int y = 0; y < g_shroudCornH; y++)
        for (int x = 0; x < g_shroudCornW; x++) {
            const float a = 0.25f * (shroud_opacity(x - 1, y - 1) + shroud_opacity(x, y - 1) +
                                     shroud_opacity(x - 1, y)     + shroud_opacity(x, y));
            if (memcmp(&a, &g_shroudCornerA[y * g_shroudCornW + x], sizeof a)) bad++;
        }
    static std::vector<float> op;
    shroud_opacity_grid(op, g_shroudCornW + 1, g_shroudCornH + 1);
    if (g_shroudOpacW != g_shroudCornW + 1 || g_shroudOpacH != g_shroudCornH + 1
        || memcmp(op.data(), g_shroudOpac.data(), op.size() * sizeof(float)))
        bad++;
    if (bad) shroud_cache_mismatch("field", bad);
}

/* Continuous Enhanced edge. Shared by the blanket and final fog mask. Four-by-four
   tessellation removes the centre-fan ridges; endpoints stay exactly clear/black. */
static float shroud_fow_curve(float a) {
    if(!shroud_fow_enhanced() || a<=0.0f || a>=1.0f)return a;
    return powf(a, g_fx.fow_falloff);
}
static float shroud_fow_at(float wx,float wz) {
    const int x=(int)floorf(wx), y=(int)floorf(wz);
    const float u=wx-x,v=wz-y;
    const float a=(shroud_corner_a(x,y)*(1-u)+shroud_corner_a(x+1,y)*u)*(1-v)
                 +(shroud_corner_a(x,y+1)*(1-u)+shroud_corner_a(x+1,y+1)*u)*v;
    return shroud_fow_curve(a);
}

/* 0 reproduces the older draw, in which the ground under the shroud was lit as though
   the shroud were not there. Kept as a switch so both can be photographed out of one
   binary and the difference counted in pixels. */
static int g_shroudTerrainLight = 1;

/* THE CONSOLE'S SHROUD BYTE FOR ONE TERRAIN CORNER, as a fraction of full light.

   gterrain.c's colour pass writes each corner's vertex alpha as shade * shroudByte / 255
   (ROM 0x1965C0..0x1965E4), indexing the shroud with the SAME corner index it uses for
   the heightmap. On the console, therefore, ground under the shroud is DRAWN DARK, and
   the translucent shroud sprite over it lies on ground that is already dim.

   Our shroud is a separate blanket layer, and until this existed the byte was simply
   left at 255: the ground under the blanket was drawn at full light and the blanket's
   own translucency alone decided how much of it reached the player. That is a lot of
   ground. A mapped-but-unseen cell is covered at SHROUD_DARK_ALPHA, which leaves 55% of
   fully lit terrain showing; the feather band is capped at SHROUD_SOFT_MAXA, which
   leaves 21.6% showing along the entire reveal perimeter. Measured on the first GDI
   mission at 1280x720, 105101 pixels of lit ground were still lit through the shroud.

   The byte returned here is one minus the same per-corner coverage the blanket's own
   vertex fade uses. Sharing the one array is what keeps the two layers agreeing corner
   by corner instead of drifting apart across the feather band.

   SOFT PATH ONLY, and that is not a preference. The coverage array is the SOFT
   blanket's four-cell corner average; the hard path draws one flat square per DARK or
   HIDDEN cell and has no per-corner term at all. Applied under the hard path this
   returned half light on two corners of a fully explored cell whose neighbour merely
   happened to be unseen, with no blanket anywhere over it to explain the dimming, so
   ground the player had already uncovered went dark for no visible reason. */
static float shroud_corner_vis(int x, int y)
{
    /* THE EDITOR SEES EVERYTHING -- and this is the half that was missed.
       g_shroudEditorOff already stopped the blanket being drawn, but the terrain's
       vertex colour is multiplied by THIS per corner, so unexplored ground was still
       drawn pure black. On a shipped mission that only darkened the far corners and
       read as "outside the map"; on a new blank map it made almost the whole world
       black and looked like the terrain had failed to draw at all. */
    if (g_shroudEditorOff)
        return 1.0f;
    if (!g_shroudTerrainLight || !g_shroudSoft)
        return 1.0f;
    /* With the shroud proper out of play the field still carries the rim, so the
       ground under the rim goes dark with its blanket rather than glowing beneath it. */
    if (!g_shroudRimOn && (!g_shroudOn || !g_shroudValid || g_shroudRevealed))
        return 1.0f;
    shroud_corners_sync();
    return 1.0f - shroud_fow_curve(shroud_corner_a(x, y));
}

/* ---- the shroud on WORLD OBJECTS, not just on the ground -------------------------
   the project owner: "if a tree is half covered in shroud, the covered side will be covered in
   shadow, while the visible part will be lit up."

   Our build had NO shroud term on objects at all: visible() culled on the object's
   CENTRE cell and draw_mesh emitted the baked vertex colour untouched, so an object
   popped from full brightness to gone at a cell boundary.

   The cartridge samples a CONTINUOUS visibility and subtracts it from the object's
   lighting inside the RDP combiner. Shroud_Vis_At (RAM 0x801F7B48 / ROM 0x199938) is a
   BILINEAR interpolation over the same four per-corner bytes the terrain shroud uses --
   literally the same data, which is why the console's objects and ground never disagree.
   The object dispatcher (RAM 0x8004BF80) calls it once per object at eight sites and
   does:
       v = Shroud_Vis_At(x, y) & 0xFF
       if (v == 0) { discard the node; }            <- not drawn at all
       set_node_env(node, (255 - v) & 0xFF)         <- RAM 0x80055FFC
   and that setter CLAMPS the darkening at 0xC8 = 200/255, which is the same ceiling
   SHROUD_SOFT_MAXA already carries. The combiner then computes (SHADE - ENV) * TEXEL0,
   which is exactly our GL_MODULATE against a per-vertex colour with env subtracted. */
static float shroud_vis_at(float wx, float wz)
{
    if (g_shroudEditorOff)                        /* see shroud_corner_vis */
        return 1.0f;
    if (!g_shroudRimOn && (!g_shroudOn || !g_shroudValid || g_shroudRevealed))
        return 1.0f;
    shroud_corners_sync();
    const int cx = (int)floorf(wx), cz = (int)floorf(wz);
    if(shroud_fow_enhanced() && g_fx.fow_smooth)return 1.0f-shroud_fow_at(wx,wz);
    const float fx = wx - (float)cx, fz = wz - (float)cz;
    const float v00 = 1.0f - shroud_corner_a(cx,     cz);
    const float v10 = 1.0f - shroud_corner_a(cx + 1, cz);
    const float v01 = 1.0f - shroud_corner_a(cx,     cz + 1);
    const float v11 = 1.0f - shroud_corner_a(cx + 1, cz + 1);
    return (v00 * (1.0f - fx) + v10 * fx) * (1.0f - fz)
         + (v01 * (1.0f - fx) + v11 * fx) * fz;
}

/* The console's ENV term, 0..1 instead of 0..255. */
static float shroud_env_at(float wx, float wz)
{
    const float e = 1.0f - shroud_vis_at(wx, wz);
    if (e < 0.0f) return 0.0f;
    return e > SHROUD_SOFT_MAXA ? SHROUD_SOFT_MAXA : e;
}

/* THE SOFT BLANKET, CELL BY CELL. What one cell of the blanket draws depends on the shroud
   around it and the ground under it, and on nothing the camera does: the core quad's four
   corners, or the feather fan's vertices with an alpha each. So a cell is worked out the
   first time a draw needs it after the shroud or the ground last changed, kept, and
   handed over again by every later draw that needs it. A draw takes the cells of the
   camera's box, row by row and column by column, which is the order the blanket always
   drew them in, and a cell off screen is not worked out at all: a frame in which the
   shroud changes costs no more than one blanket cost before anything was kept, and a
   scroll works out only the strip that comes into view. The colour is not kept, because
   it is 0 in the picture and 1 in the fog mask (shroud_colour), the same for every
   vertex of a pass. */
enum { SHROUD_SOFT_CORE = 1, SHROUD_SOFT_FEATHER = 2 };
#define SHROUD_SOFT_CELL_VERTS 96            /* the smooth Enhanced fan, 4 x 4 x 6 */

/* One cell, worked out: the blocks it draws in, the core quad's corners (x y z each),
   and the feather vertices with their alphas. */
struct ShroudSoftCell {
    int blocks, n;
    float core[12];
    float pos[3 * SHROUD_SOFT_CELL_VERTS], a[SHROUD_SOFT_CELL_VERTS];
};
static void shroud_soft_put(float* p, float x, float y, float z)
{
    p[0] = x; p[1] = y; p[2] = z;
}
/* One feather vertex: its alpha, and where it lies. */
static void shroud_soft_feather(ShroudSoftCell& c, float a, float x, float y, float z)
{
    c.a[c.n] = a;
    shroud_soft_put(&c.pos[3 * c.n], x, y, z);
    c.n++;
}
/* One vertex of the continuous Enhanced edge, which the fog mask shares. */
static void shroud_fow_vertex(ShroudSoftCell& c, float x, float y)
{
    shroud_soft_feather(c, shroud_fow_at(x, y), x, SHROUD_BLACK_Y + shroud_gy(x, y), y);
}

/* Works out cell (x, y), which `ring` must admit (NULL admits every cell): the tests and
   the arithmetic of the two blocks the blanket draws, unchanged. */
static void shroud_soft_work(ShroudSoftCell& c, int x, int y, shroud_cellshown_fn ring)
{
    c.blocks = 0;
    c.n = 0;

    /* Block 1: the deep black core, unblended. A cell is core when its centre and all
       four corners are fully opaque, i.e. it and every neighbour that touches it
       through a corner is HIDDEN. */
    if (!(shroud_opacity(x, y) < 1.0f) &&
        !(shroud_corner_a(x, y)         < 1.0f ||
          shroud_corner_a(x + 1, y)     < 1.0f ||
          shroud_corner_a(x, y + 1)     < 1.0f ||
          shroud_corner_a(x + 1, y + 1) < 1.0f) &&
        !(ring && !ring(x, y))) {
        /* ALL FOUR corners ride the heightfield. Two of them once did not: the
           west pair was pinned to the absolute plane while the east pair rode
           the ground, so every deep-shroud cell drew as a RAMP, not a lid.
           Uphill the ramp sank under the terrain and let the ground show
           through (black stripes); downhill it floated a whole cell up and
           painted black over sand and water (the shoreline wedges). */
        c.blocks |= SHROUD_SOFT_CORE;
        shroud_soft_put(&c.core[0], (float)x,        SHROUD_BLACK_Y + shroud_gy((float)x, (float)y), (float)y);
        shroud_soft_put(&c.core[3], (float)x + 1.0f, SHROUD_BLACK_Y + shroud_gy((float)x + 1.0f, (float)y), (float)y);
        shroud_soft_put(&c.core[6], (float)x + 1.0f, SHROUD_BLACK_Y + shroud_gy((float)x + 1.0f, (float)y + 1.0f), (float)y + 1.0f);
        shroud_soft_put(&c.core[9], (float)x,        SHROUD_BLACK_Y + shroud_gy((float)x, (float)y + 1.0f), (float)y + 1.0f);
    }

    /* Block 2: the feather ring, one blended pass. Every remaining cell that any
       non-zero alpha touches becomes a 4-triangle fan; the fan (rather than one quad)
       keeps the interpolation symmetric so no diagonal seam shows. */
    float a00, a10, a01, a11, ac, cx, cy;
    a00 = shroud_corner_a(x, y);
    a10 = shroud_corner_a(x + 1, y);
    a01 = shroud_corner_a(x, y + 1);
    a11 = shroud_corner_a(x + 1, y + 1);
    ac  = shroud_opacity(x, y);
    if (ac >= 1.0f && a00 >= 1.0f && a10 >= 1.0f && a01 >= 1.0f && a11 >= 1.0f)
        return;                                 /* the core block already owns it */
    if (a00 <= 0.0f && a10 <= 0.0f && a01 <= 0.0f && a11 <= 0.0f && ac <= 0.0f)
        return;                                 /* fully clear, nothing to draw */
    if (ring && !ring(x, y))
        return;
    c.blocks |= SHROUD_SOFT_FEATHER;
    if(shroud_fow_enhanced() && g_fx.fow_smooth) {
        for(int j=0;j<4;j++)for(int i=0;i<4;i++) {
            const float u0=x+i*0.25f,v0=y+j*0.25f,u1=u0+0.25f,v1=v0+0.25f;
            shroud_fow_vertex(c,u0,v0);shroud_fow_vertex(c,u1,v0);shroud_fow_vertex(c,u1,v1);
            shroud_fow_vertex(c,u0,v0);shroud_fow_vertex(c,u1,v1);shroud_fow_vertex(c,u0,v1);
        }
        return;
    }
    /* ROM 0xC8 ceiling (see SHROUD_SOFT_MAXA), applied per vertex and on the
       SAME test for all five: clamp a value only when it is inside the band,
       which is to say when that vertex is not fully interior. The centre used
       to be clamped unconditionally, which held the centre of a cell that is
       itself completely unexplored at the 0.784 ceiling merely because one of
       its corners touched the reveal edge: 21.6% of the ground under it stayed
       visible in the middle of territory the player has never seen. */
    if (a00 < SHROUD_SOFT_OPAQ && a00 > SHROUD_SOFT_MAXA) a00 = SHROUD_SOFT_MAXA;
    if (a10 < SHROUD_SOFT_OPAQ && a10 > SHROUD_SOFT_MAXA) a10 = SHROUD_SOFT_MAXA;
    if (a01 < SHROUD_SOFT_OPAQ && a01 > SHROUD_SOFT_MAXA) a01 = SHROUD_SOFT_MAXA;
    if (a11 < SHROUD_SOFT_OPAQ && a11 > SHROUD_SOFT_MAXA) a11 = SHROUD_SOFT_MAXA;
    if (ac  < SHROUD_SOFT_OPAQ && ac  > SHROUD_SOFT_MAXA) ac  = SHROUD_SOFT_MAXA;
    cx = (float)x + 0.5f;
    cy = (float)y + 0.5f;

    /* four triangles around the centre vertex */
    shroud_soft_feather(c, ac,  cx, SHROUD_BLACK_Y + shroud_gy(cx, cy), cy);
    shroud_soft_feather(c, a00, (float)x, SHROUD_BLACK_Y + shroud_gy((float)x, (float)y), (float)y);
    shroud_soft_feather(c, a10, (float)x + 1.0f, SHROUD_BLACK_Y + shroud_gy((float)x + 1.0f, (float)y), (float)y);

    shroud_soft_feather(c, ac,  cx, SHROUD_BLACK_Y + shroud_gy(cx, cy), cy);
    shroud_soft_feather(c, a10, (float)x + 1.0f, SHROUD_BLACK_Y + shroud_gy((float)x + 1.0f, (float)y), (float)y);
    shroud_soft_feather(c, a11, (float)x + 1.0f, SHROUD_BLACK_Y + shroud_gy((float)x + 1.0f, (float)y + 1.0f), (float)y + 1.0f);

    shroud_soft_feather(c, ac,  cx, SHROUD_BLACK_Y + shroud_gy(cx, cy), cy);
    shroud_soft_feather(c, a11, (float)x + 1.0f, SHROUD_BLACK_Y + shroud_gy((float)x + 1.0f, (float)y + 1.0f), (float)y + 1.0f);
    shroud_soft_feather(c, a01, (float)x, SHROUD_BLACK_Y + shroud_gy((float)x, (float)y + 1.0f), (float)y + 1.0f);

    shroud_soft_feather(c, ac,  cx, SHROUD_BLACK_Y + shroud_gy(cx, cy), cy);
    shroud_soft_feather(c, a01, (float)x, SHROUD_BLACK_Y + shroud_gy((float)x, (float)y + 1.0f), (float)y + 1.0f);
    shroud_soft_feather(c, a00, (float)x, SHROUD_BLACK_Y + shroud_gy((float)x, (float)y), (float)y);
}

/* The kept cells of one cell range. A cell's stamp says which epoch worked it out; a new
   epoch (the shroud, the ground or the range moved) leaves every cell to be worked out
   again on its next draw. The vertices go into three pools in the order cells are
   worked out, which is not the order they are drawn in: each cell knows where its own
   are. */
struct ShroudSoftCells {
    int x0, y0, w, h;
    unsigned epoch;
    std::vector<unsigned> stamp;
    std::vector<unsigned char> blocks;
    std::vector<unsigned> core;              /* the first float of its quad in corePos */
    std::vector<unsigned> first;             /* its first feather vertex */
    std::vector<unsigned short> n;           /* and how many it has */
    std::vector<float> corePos, featherPos, featherA;
};
static ShroudSoftCells g_shroudSoftCells;

/* Forgets every kept cell, and sizes the table for range [x0,x1) x [y0,y1). */
static void shroud_soft_forget(ShroudSoftCells& t, int x0, int y0, int x1, int y1)
{
    const int w = x1 > x0 ? x1 - x0 : 0, h = y1 > y0 ? y1 - y0 : 0;
    if (w != t.w || h != t.h || x0 != t.x0 || y0 != t.y0 || ++t.epoch == 0) {
        const size_t n = (size_t)w * h;
        t.x0 = x0; t.y0 = y0; t.w = w; t.h = h;
        t.stamp.assign(n, 0u);
        t.blocks.resize(n); t.core.resize(n); t.first.resize(n); t.n.resize(n);
        t.epoch = 1;
    }
    t.corePos.clear(); t.featherPos.clear(); t.featherA.clear();
}

/* Cell (x, y) of the table, worked out now if this epoch has not done it yet. */
static size_t shroud_soft_at(ShroudSoftCells& t, int x, int y, shroud_cellshown_fn ring)
{
    const size_t i = (size_t)(y - t.y0) * t.w + (size_t)(x - t.x0);
    if (t.stamp[i] == t.epoch)
        return i;
    static ShroudSoftCell c;
    shroud_soft_work(c, x, y, ring);
    t.blocks[i] = (unsigned char)c.blocks;
    if (c.blocks & SHROUD_SOFT_CORE) {
        t.core[i] = (unsigned)t.corePos.size();
        t.corePos.insert(t.corePos.end(), c.core, c.core + 12);
    }
    if (c.blocks & SHROUD_SOFT_FEATHER) {
        t.first[i] = (unsigned)t.featherA.size();
        t.n[i] = (unsigned short)c.n;
        t.featherA.insert(t.featherA.end(), c.a, c.a + c.n);
        t.featherPos.insert(t.featherPos.end(), c.pos, c.pos + 3 * c.n);
    }
    t.stamp[i] = t.epoch;
    g_shroudCache.blanketCells++;
    return i;
}

/* Draws the cells of [vx0,vx1) x [vy0,vy1), the camera's part of the table, working out
   any not yet kept. Written through OverlayArrays (overlay_arrays.h): immediate mode by
   default, or with the overlay_arrays dial one array draw a block. The core is positions
   under one colour, the feather carries a colour per vertex; the calls are the ones the
   blanket made when it worked each vertex out as it drew it, in the same order with the
   same values. */
static void shroud_soft_replay(ShroudSoftCells& t, int vx0, int vy0, int vx1, int vy1,
                               shroud_cellshown_fn ring)
{
    int x, y;
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_FALSE);

    static OverlayArrays core, feather;
    core.reset(false, false);
    feather.reset(true, false);

    glDisable(GL_BLEND);
    shroud_colour(1.0f);
    core.begin(GL_QUADS);
    for (y = vy0; y < vy1; y++)
        for (x = vx0; x < vx1; x++) {
            const size_t i = shroud_soft_at(t, x, y, ring);
            if (!(t.blocks[i] & SHROUD_SOFT_CORE))
                continue;
            const float* p = &t.corePos[t.core[i]];
            core.vertex(p[0], p[1], p[2]);
            core.vertex(p[3], p[4], p[5]);
            core.vertex(p[6], p[7], p[8]);
            core.vertex(p[9], p[10], p[11]);
        }
    core.end();
    core.flush();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const float k = g_shroudMaskPass ? 1.0f : 0.0f;   /* shroud_colour's own value */
    feather.begin(GL_TRIANGLES);
    for (y = vy0; y < vy1; y++)
        for (x = vx0; x < vx1; x++) {
            const size_t i = (size_t)(y - t.y0) * t.w + (size_t)(x - t.x0);
            if (!(t.blocks[i] & SHROUD_SOFT_FEATHER))
                continue;
            const float* a = &t.featherA[t.first[i]];
            const float* p = &t.featherPos[3 * (size_t)t.first[i]];
            for (int v = 0; v < t.n[i]; v++, p += 3) {
                feather.color(k, k, k, a[v]);
                feather.vertex(p[0], p[1], p[2]);
            }
        }
    feather.end();
    feather.flush();
    glDisable(GL_BLEND);

    glDepthMask(GL_TRUE);
}

/* Verify, per draw: every drawn cell worked out again from scratch against its kept
   copy, and the camera's rectangle against the per-cell view test it stands for. */
static void shroud_soft_check(ShroudSoftCells& t, int x0, int y0, int x1, int y1,
                              int vx0, int vy0, int vx1, int vy1,
                              shroud_cellshown_fn ring, shroud_cellshown_fn inview)
{
    static ShroudSoftCell c;
    long bad = 0, badView = 0;
    for (int y = vy0; y < vy1; y++)
        for (int x = vx0; x < vx1; x++) {
            const size_t i = (size_t)(y - t.y0) * t.w + (size_t)(x - t.x0);
            shroud_soft_work(c, x, y, ring);
            g_shroudCache.checks++;
            if (t.stamp[i] != t.epoch || c.blocks != t.blocks[i]) { bad++; continue; }
            if ((c.blocks & SHROUD_SOFT_CORE)
                && memcmp(c.core, &t.corePos[t.core[i]], sizeof c.core))
                bad++;
            else if ((c.blocks & SHROUD_SOFT_FEATHER)
                     && (c.n != t.n[i]
                         || memcmp(c.a, &t.featherA[t.first[i]], (size_t)c.n * sizeof(float))
                         || memcmp(c.pos, &t.featherPos[3 * (size_t)t.first[i]], (size_t)c.n * 3 * sizeof(float))))
                bad++;
        }
    if (inview)
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
                if ((inview(x, y) != 0) != (x >= vx0 && x < vx1 && y >= vy0 && y < vy1)) badView++;
    if (bad) shroud_cache_mismatch("blanket", bad);
    if (badView) shroud_cache_mismatch("blanket view", badView);
}

/* Same GL contract as shroud_draw: inside the 3D scene after every object pass,
   depth test ON (near buildings keep occluding), depth write OFF, leaves blend and
   texture off. The view test comes in two halves: `ring`, which depends only on the
   map rectangle and the drawn margin, is asked when a cell is worked out; the camera's
   box is asked per draw, as the rectangle `viewrect` narrows the range to (NULL draws
   the whole range). `inview` is the same box asked cell by cell, used only by verify to
   check the rectangle. hostKey lists everything else the cells are made of that this
   module cannot see (the ground heights' generations, the rectangle `ring` reads); the
   kept cells are forgotten when it or the shroud changes. */
static void shroud_draw_soft(int x0, int y0, int x1, int y1, shroud_cellshown_fn ring,
                             shroud_viewrect_fn viewrect, shroud_cellshown_fn inview,
                             const unsigned* hostKey, int nHostKey)
{
    if (!g_shroudRimOn && (!g_shroudOn || !g_shroudValid))
        return;                       /* the rim alone still draws, see shroud_is_rim */

    /* ONE RING OUTSIDE THE WORLD GRID. The blanket rides 0.11 world units above the
       ground it covers (SHROUD_BLACK_Y + the 0.06 pad in shroud_ground_y), and under a
       pitched camera a higher point projects HIGHER on screen. Inside the field that is
       invisible because the next cell's lid covers the gap; at the OUTERMOST ring there
       is no next cell, so the last strip of terrain was left uncovered and read as a
       bright lit line along the map edge -- measured at 3 screen rows on SCG01EB, and
       the second half of why the map's border was so easy to see.
       terrain_y clamps both coordinates to the world grid before indexing, so a ring vertex at
       -1 or 65 reuses the edge corner height and the lid simply extends flat outward:
       no out-of-bounds read and no phantom cliff, because nothing textured is drawn out
       there. shroud_opacity off-grid returns HIDDEN, so the ring is plain opaque black. */
    if (x0 < -1) x0 = -1;
    if (y0 < -1) y0 = -1;
    if (x1 > g_shroudCornW) x1 = g_shroudCornW;
    if (y1 > g_shroudCornH) y1 = g_shroudCornH;

    shroud_corners_sync();

    ShroudSoftCells& t = g_shroudSoftCells;
    static std::vector<unsigned> keptKey, key;
    key.clear();
    key.push_back(g_shroudFieldGen);
    key.push_back((unsigned)g_shroudCornW);
    key.push_back((unsigned)g_shroudCornH);
    key.push_back((unsigned)shroud_fow_enhanced());
    key.push_back(g_fx.fow_smooth ? 1u : 0u);
    key.push_back(shroud_key_bits(g_fx.fow_falloff));
    key.push_back((unsigned)x0); key.push_back((unsigned)y0);
    key.push_back((unsigned)x1); key.push_back((unsigned)y1);
    key.insert(key.end(), hostKey, hostKey + nHostKey);
    const int mode = shroud_cache_mode();
    if (mode == 0 || key != keptKey) {
        shroud_soft_forget(t, x0, y0, x1, y1);
        keptKey = key;
        g_shroudCache.blanketBuilt++;
    } else
        g_shroudCache.blanketKept++;

    int vx0 = x0, vy0 = y0, vx1 = x1, vy1 = y1;
    if (viewrect) viewrect(&vx0, &vy0, &vx1, &vy1);
    if (vx0 < x0) vx0 = x0;
    if (vy0 < y0) vy0 = y0;
    if (vx1 > x1) vx1 = x1;
    if (vy1 > y1) vy1 = y1;
    if (vx1 < vx0) vx1 = vx0;
    if (vy1 < vy0) vy1 = vy0;
    g_shroudCache.blanketView = (unsigned long)(vx1 - vx0) * (unsigned long)(vy1 - vy0);
    g_shroudCache.blanketRange = (unsigned long)t.w * (unsigned long)t.h;
    shroud_soft_replay(t, vx0, vy0, vx1, vy1, ring);
    if (mode == 2)
        shroud_soft_check(t, x0, y0, x1, y1, vx0, vy0, vx1, vy1, ring, inview);
}

/* ---------------------------------------------------------------------------------- *
 *  Health bars
 * ---------------------------------------------------------------------------------- */

static int g_hbOn = 1;

/* THE SHOW RULE IS A RUNTIME MODE, and the three positions are the only ones the two
   originals between them define:
       HB_MODE_OFF          nothing carries a bar, whatever is selected
       HB_MODE_SELECTED     selected only -- the vanilla DOS default (special.h:75
                            HB_SELECTED)
       HB_MODE_DAMAGED_TOO  selected OR below full health -- the remaster's HB_DAMAGED,
                            what this module has always drawn, and the default
   It was a compile-time 0/1, so neither the strict default nor an off position was
   reachable from inside the game, which is what both of the reports against this rule
   come down to. The third position keeps the name HB_MODE_DAMAGED_TOO so that the
   references to it elsewhere in the tree still resolve to the thing they describe.

   IT IS NOT THE CARTRIDGE'S RULE, and that is registered rather than implied. The
   console shows a bar on every ALLIED object whether or not it is damaged, and this
   renderer is handed no ally relation to test: Is_Ally reads a per-house bitfield
   (house.cpp:2068) and the object export carries only the owning house's name, which
   answers "is this mine" and not "is this allied", so the ally arm of the console's
   rule cannot be reproduced without widening the export.

   OFF REACHES THE PIP ROW TOO, because both strips ask this one function. That coupling
   is deliberate: a pip row hovering beside no health bar
   reads as a bug. --nohb is the older switch and still gates the health bars alone. */
enum { HB_MODE_OFF = 0, HB_MODE_SELECTED = 1, HB_MODE_DAMAGED_TOO = 2 };

/* ONE READ OF THE DIAL, and it clamps. Every supported path into g_fx.hb_mode already
   bounds it -- the panel slider, the gfx script verb and the cfg loader all go through
   fx_quant against the row's own 0..2 -- so the clamp is for a value that arrives some
   other way, where falling off the end of the range must not silently mean "draw none".
   g_fx comes from fx_state.h, which the one translation unit that includes this header
   includes well above it; shatter_mod.h reaches it the same way. */
static int hb_show_mode(void)
{
    float m = g_fx.hb_mode;
    if (m < (float)HB_MODE_OFF)         m = (float)HB_MODE_OFF;
    if (m > (float)HB_MODE_DAMAGED_TOO) m = (float)HB_MODE_DAMAGED_TOO;
    return (int)(m + 0.5f);
}

/* Cardinal_To_Fixed(MaxStrength, Strength): common/misc.cpp:18. 0x100 = full health.
   The 0xFFFF degenerate (base 0) matches the engine and falls into "green". */
static unsigned hb_ratio256(int strength, int maxstrength)
{
    if (maxstrength <= 0)
        return 0xFFFFu;
    return ((unsigned)strength << 8) / (unsigned)maxstrength;
}

/* Fill width in pixels for an interior of `interior` px: Fixed_To_Cardinal + the
   engine's Bound(pwidth, 1, width - 2) (techno.cpp:1086..1088, misc.cpp:23). */
static int hb_fill_px(int interior, unsigned ratio)
{
    unsigned tmp = ratio * (unsigned)interior + 128u;
    int pw;
    if (tmp & 0xFF000000u)          /* engine's overflow guard */
        tmp = 0xFFFFu;
    else
        tmp >>= 8;
    pw = (int)tmp;
    if (pw < 1)
        pw = 1;
    if (pw > interior)
        pw = interior;
    return pw;
}

/* techno.cpp:1091..1096. Thresholds are strict less-than, out of 0x100. */
/* THE CARTRIDGE'S colours, not the 1995 DOS palette's. The two engines agree on the
   THRESHOLDS -- 0x3F and 0x7F are the floats 63/256 at RAM 0x801C2B74 and 127/256 at
   0x801C2B78, exactly techno.cpp's -- and disagree on every colour. Producer
   RAM 0x801D0854, colour block ROM 0x17275C..0x1727B4. */
static void hb_colour(unsigned ratio, float* r, float* g, float* b)
{
    if (ratio < 0x3F) {             /* RED    (240,0,0)     */
        *r = 240.0f / 255.0f; *g = 0.0f;            *b = 0.0f;
    } else if (ratio < 0x7F) {      /* YELLOW (200,200,68)  */
        *r = 200.0f / 255.0f; *g = 200.0f / 255.0f; *b = 68.0f / 255.0f;
    } else {                        /* GREEN  (34,200,34)   */
        *r = 34.0f / 255.0f;  *g = 200.0f / 255.0f; *b = 34.0f / 255.0f;
    }
}

/* techno.cpp:1069..1073 (cloak omitted; see header note). */
static int hb_should_show(int strength, int maxstrength, int selected)
{
    int mode;
    if (strength <= 0)
        return 0;
    mode = hb_show_mode();
    if (mode == HB_MODE_OFF)
        return 0;
    if (selected)
        return 1;
    return mode == HB_MODE_DAMAGED_TOO && strength < maxstrength;
}

/* One bar. Screen-space pixels, y down; the caller has an ortho pixel projection up
   (cnc_eyes begin_overlay). cx = horizontal centre, top = the silhouette's top row,
   w = bar width in px (the DOS engine uses the object's logical width; the caller
   passes its measured silhouette width). scale = px-per-cell / 24: the multiplier
   from DOS pixels to screen pixels at this object's depth, floored to 1 by the caller.

   DOS shape at scale 1 (techno.cpp:1078..1097): outline box w x 4 in BLACK, interior
   rows 1..2, fill in the condition colour, unfilled interior darkened (the engine
   remaps through Map.FadingShade; a flat near-black reads the same at a glance). */
static void hb_draw_one(float cx, float top, float w, float scale, int strength, int maxstrength)
{
    int b = (int)(scale + 0.5f);          /* one DOS pixel, scaled */
    int wi, h, x0, y0, inner, pw;
    unsigned ratio;
    float fr, fg, fb;

    if (b < 1)
        b = 1;
    h = 4 * b;                            /* the DOS bar is 4 rows tall */
    wi = (int)(w + 0.5f);
    if (wi < 8 * b)                       /* floor: a 3-px bar carries no information */
        wi = 8 * b;
    inner = wi - 2 * b;
    x0 = (int)(cx - (float)wi * 0.5f);
    y0 = (int)top - h - b;                /* sits just above the silhouette */

    ratio = hb_ratio256(strength, maxstrength);
    pw = hb_fill_px(inner, ratio);
    hb_colour(ratio, &fr, &fg, &fb);

    /* outline: BLACK, palette 12 */
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2i(x0, y0);
    glVertex2i(x0 + wi, y0);
    glVertex2i(x0 + wi, y0 + h);
    glVertex2i(x0, y0 + h);
    /* interior background: darkened */
    glColor3f(0.13f, 0.13f, 0.13f);
    glVertex2i(x0 + b, y0 + b);
    glVertex2i(x0 + b + inner, y0 + b);
    glVertex2i(x0 + b + inner, y0 + h - b);
    glVertex2i(x0 + b, y0 + h - b);
    /* the strength fill */
    glColor3f(fr, fg, fb);
    glVertex2i(x0 + b, y0 + b);
    glVertex2i(x0 + b + pw, y0 + b);
    glVertex2i(x0 + b + pw, y0 + h - b);
    glVertex2i(x0 + b, y0 + h - b);
    glEnd();
}

#endif /* SHROUD_MOD_H */
