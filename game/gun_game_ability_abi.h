/* GUN GAME ABILITIES: the contract between the Tiberian Dawn engine (brain) and the
   renderer. Plain C types only, so the engine's dllinterface.cpp and the renderer's
   cnc_eyes.cpp read the same numbers. The engine owns every rule and every timer (they
   run in lockstep); the renderer owns the cameo, the activation mini-games, the drawing
   and the player's input.

   Flow: a crate gives either a superweapon (as before) or one ability. The player holds
   at most one ability; a new pickup replaces it. The renderer shows its cameo beside the
   superweapon cameo, runs that ability's activation mini-game, and on success calls
   OpenCNC3D_Gun_Ability_Use, which validates and queues lockstep events. Every peer then
   applies the ability on the same frame. Failure or Escape keeps the ability; the
   renderer waits two seconds before the next try. */
#ifndef GUN_GAME_ABILITY_ABI_H
#define GUN_GAME_ABILITY_ABI_H

#include <stdint.h>

enum GunAbility {
    GA_NONE = 0,
    GA_STEALTH = 1,       /* self: invisible to enemies 12 s; firing or taking damage breaks it */
    GA_CHRONO_HOP = 2,    /* cell: teleport up to 8 cells */
    GA_CHRONO_SWAP = 3,   /* enemy: trade places with an enemy combatant within 10 cells */
    GA_IRON_CURTAIN = 4,  /* self: invulnerable 6 s */
    GA_OVERDRIVE = 5,     /* self: 75% faster and double rate of fire 10 s */
    GA_VORTEX = 6,        /* cell: a drifting vortex 15 s that damages anything it touches, owner included */
    GA_MINEFIELD = 7,     /* cells: five hidden mines, each within 4 cells of the combatant */
    GA_HUNTERS_EYE = 8,   /* self: every enemy visible through fog 10 s; the leader is marked */
    GA_HUMILIATION = 9,   /* enemy: the target drops one rank (not below the first) */
    GA_CHAIN_SHOCK = 10,  /* self: 8 s; each hit arcs to up to two more enemies near the target */
    GA_EXTRA_LIFE = 11,   /* self: the next death within 30 s does not count; respawn where you fell */
    GA_SMOKE = 12,        /* cells: smoke on up to GA_MAX_CELLS cells for 10 s; nobody sees or targets into it */
    GA_DECOYS = 13,       /* self: three copies of your unit, one hit each, unarmed, 20 s */
    GA_GIANT = 14,        /* self: double size and half damage taken 10 s */
    GA_LAST_LAUGH = 15,   /* self: 15 s; dying in that time sets off a large blast where you fall */
    GA_HOT_POTATO = 16,   /* enemy: bomb sticks to the target, explodes after 8 s; passes on contact */
    GA_BOOBY_CRATE = 17,  /* self: drops a fake crate beside you; the first enemy to take it is blown up */
    GA_WORMHOLE = 18,     /* cells [entry, exit]: linked portals 20 s, anyone may use them */
    GA_DANCE_OFF = 19,    /* self: enemy combatants within 5 cells freeze 3 s (soldiers salute, vehicles spin) */
    GA_MIRROR = 20,       /* enemy: the target's left-right controls are mirrored 6 s */
    GA_COUNT = 21
};

#define GA_MAX_CELLS 24          /* most cells one activation may carry (smoke) */
#define GA_RETRY_MS 2000         /* renderer pause after a failed or cancelled try */

/* Per rendered object (CNCObjectStruct Type and ID), OpenCNC3D_Gun_Object_Flags. */
enum GunAbilityObjectFlag {
    GAF_STEALTH = 1 << 0,     /* owner sees it ghosted; every other viewer must not draw it, list it or name it */
    GAF_IRON = 1 << 1,        /* red invulnerability tint */
    GAF_OVERDRIVE = 1 << 2,
    GAF_GIANT = 1 << 3,       /* draw at twice the size */
    GAF_CHAIN = 1 << 4,
    GAF_EXTRA_LIFE = 1 << 5,
    GAF_LAST_LAUGH = 1 << 6,
    GAF_HOT_POTATO = 1 << 7,  /* carries the bomb */
    GAF_DANCING = 1 << 8,
    GAF_MIRRORED = 1 << 9,
    GAF_DECOY = 1 << 10,      /* a decoy (owner's view only; other viewers get 0 for this bit) */
    GAF_SMOKED = 1 << 11      /* stands in smoke: hidden from viewers outside it */
};

/* World effects visible to one viewer, OpenCNC3D_Gun_Effects. Positions are engine
   leptons (256 per cell, Coord_X / Coord_Y). */
enum GunEffectKind {
    GFX_VORTEX = 1,    /* x,y; value = remaining ticks */
    GFX_WORMHOLE = 2,  /* entry x,y to exit x2,y2; value = remaining ticks */
    GFX_MINE = 3,      /* x,y; reported only to its owner */
    GFX_SMOKE = 4,     /* one smoked cell centre x,y; value = remaining ticks */
    GFX_ARC = 5,       /* chain lightning from x,y to x2,y2; age = ticks since it struck (drop after 8) */
    GFX_ANNOUNCE = 6,  /* owner = house that used it; value = GunAbility; age = ticks since use (drop after 45) */
    GFX_POTATO = 7,    /* the bomb on its holder: x,y; owner = thrower; value = remaining ticks */
    GFX_BOOBY = 8,     /* a fake crate x,y; reported only to its owner */
    GFX_REVEAL = 9,    /* an enemy combatant x,y while the viewer's Hunter's Eye runs; value = 1 for the leader */
    GFX_BLAST = 10     /* a Last Laugh or Hot Potato blast at x,y; age = ticks since */
};

typedef struct GunEffect {
    int32_t kind, owner, x, y, x2, y2, age, value;
} GunEffect;

/* Renderer-side timers for the local player, OpenCNC3D_Gun_Player_Timers: remaining
   ticks of each effect ON this player (index = GunAbility), e.g. [GA_MIRROR] while the
   player's controls are mirrored, [GA_HUNTERS_EYE] while they see through fog.
   ticks_per_second is written through the second argument so the renderer can show
   seconds at any game speed. */

/* Engine exports (extern "C", resolved with dlsym by the renderer):

   int      OpenCNC3D_Gun_Ability_Held(uint64_t player_id);
            GunAbility the player holds, or GA_NONE.

   bool     OpenCNC3D_Gun_Ability_Use(uint64_t player_id, int ability, int target_house,
                                      int ncells, const int16_t *cell_xy);
            Validates and queues the lockstep events for an activation. target_house is
            the engine HousesType of the targeted enemy (-1 when none); cell_xy holds
            ncells (x, y) cell pairs (cell_xy[2*i], cell_xy[2*i+1]). Returns false and
            queues nothing when the ability is not held, the target is not valid or a
            cell is out of reach; the renderer then keeps the ability.

   unsigned OpenCNC3D_Gun_Object_Flags(uint64_t viewer_player_id, int dll_type, int id);
            GAF_* bits for one rendered object as the viewer may know them.

   int      OpenCNC3D_Gun_Effects(uint64_t viewer_player_id, GunEffect *out, int max);
            Fills up to max effects visible to the viewer; returns the count.

   int      OpenCNC3D_Gun_Player_Timers(uint64_t player_id, int32_t *remaining,
                                        int32_t *ticks_per_second);
            remaining[GA_COUNT]; returns 1 when the player is in a Gun Game match.

   bool     OpenCNC3D_Gun_Ability_Grant(uint64_t player_id, int ability);
            Test hook: gives the player an ability. Honoured only when the environment
            variable OPENCNC3D_GUN_TEST is set on every peer (the gates set it); a normal
            match ignores it. Goes through a lockstep event like a real pickup. */

/* Ability cameo pack written by the art bake, read by the renderer:
   "GABIL01\0" (8 bytes), uint32 count (little endian), then per entry in GunAbility order
   starting at GA_STEALTH: char name[16] (e.g. "GA01"), uint32 w, uint32 h, w*h*4 RGBA.
   Cameos are 64 x 48, the size of the game's own cameos. File: gun_abilities.pack in the
   play folder (baked into game/gun_abilities.pack). */

#endif
