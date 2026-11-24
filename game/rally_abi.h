/* RALLY: the contract between the Tiberian Dawn brain's Rally module
 * (brain/vanilla/tiberiandawn/rally.cpp) and the eyes (game/rally_mod.h).
 *
 * Rally is a kart race on a course map: laps of a circuit, or one run of a
 * point-to-point course. The brain owns the race:
 * every racer's vehicle physics, laps of the course, crates and items, damage and
 * respawn, the AI drivers and the finishing order. It runs in fixed point from
 * lockstep events, so every LAN peer computes the same race. The eyes only send
 * the local player's controls and draw what the brain reports.
 *
 * Plain C, fixed-width types only: this header is compiled into the engine and the
 * renderer on macOS (LP64) and 64-bit Windows (LLP64), where `long` differs.
 *
 * UNITS
 *   Position: "fine" units, 1/256 of a lepton. One map cell is 256 leptons, so one
 *     cell is 65536 fine units. x grows east, y grows south, the engine's own axes.
 *   Height z: fine units above the ground the unit stands on (0 on the ground).
 *   Angles: 16-bit binary angles, 65536 = one turn. yaw 0 faces north and grows
 *     clockwise, so (yaw >> 8) is the engine's DirType. pitch > 0 is nose up,
 *     roll > 0 is right side down; both are signed (-32768..32767 = -180..180 deg).
 */
#ifndef OPENCNC3D_RALLY_ABI_H
#define OPENCNC3D_RALLY_ABI_H

#include <stdint.h>

#define RALLY_ABI_VERSION   2
#define RALLY_MAX_RACERS    8
#define RALLY_MAX_CRATES    64
#define RALLY_MAX_HAZARDS   48
#define RALLY_MAX_COURSE    512
#define RALLY_MAX_ROSTER    24
#define RALLY_FINE_PER_CELL 65536
#define RALLY_LAPS_MIN      1
#define RALLY_LAPS_MAX      9
#define RALLY_LAPS_DEFAULT  3

/* The race's phase. */
enum {
    RALLY_PHASE_OFF = 0,       /* not a Rally match */
    RALLY_PHASE_PICK = 1,      /* every player chooses a vehicle */
    RALLY_PHASE_COUNTDOWN = 2, /* 3, 2, 1 on the grid */
    RALLY_PHASE_RACE = 3,      /* racing */
    RALLY_PHASE_DONE = 4,      /* results, until the deciding player chooses (RallyState.decider):
                                  RALLY_CHOICE_RETRY goes back to PICK on the same course,
                                  RALLY_CHOICE_END goes to OVER */
    RALLY_PHASE_OVER = 5       /* the match is ending with the winner's house victorious */
};

/* What the deciding player chooses on the results (OpenCNC3D_Rally_Choose). */
enum {
    RALLY_CHOICE_NONE = 0,
    RALLY_CHOICE_RETRY = 1,    /* race again: same course, same laps, same seats */
    RALLY_CHOICE_END = 2       /* end the match */
};

/* Items a crate can give. RALLY_ITEM_NONE is an empty slot. */
enum {
    RALLY_ITEM_NONE = 0,
    RALLY_ITEM_NITRO,        /* one speed burst */
    RALLY_ITEM_NITRO3,       /* three speed bursts */
    RALLY_ITEM_MINE,         /* dropped behind; whoever drives over it is thrown */
    RALLY_ITEM_ROCKET,       /* fired straight ahead along the hull */
    RALLY_ITEM_HOMING,       /* seeks the racer one place ahead */
    RALLY_ITEM_ION,          /* the Ion Cannon strikes the race leader */
    RALLY_ITEM_ARMOUR,       /* invulnerable, faster, throws whoever it rams */
    RALLY_ITEM_CLOAK,        /* cannot be targeted by towers or racers */
    RALLY_ITEM_REPAIR,       /* full health */
    RALLY_ITEM_COUNT
};

/* Racer flags. */
enum {
    RALLY_F_AIRBORNE   = 1 << 0,
    RALLY_F_FLIPPED    = 1 << 1,  /* lying on its roof or side; rights itself after 3 s */
    RALLY_F_RIGHTING   = 1 << 2,  /* the automatic flip back is playing */
    RALLY_F_DEAD       = 1 << 3,  /* destroyed; respawn_ticks counts down */
    RALLY_F_FINISHED   = 1 << 4,
    RALLY_F_BOOST      = 1 << 5,
    RALLY_F_DRIFT      = 1 << 6,
    RALLY_F_CLOAK      = 1 << 7,
    RALLY_F_ARMOUR     = 1 << 8,
    RALLY_F_SPINNING   = 1 << 9,  /* spun out by a mine or a hit; no control */
    RALLY_F_WRONG_WAY  = 1 << 10,
    RALLY_F_HUMAN      = 1 << 11, /* a player's seat, not an AI driver */
    RALLY_F_SHIELDED   = 1 << 12, /* respawn grace: cannot be damaged */
    RALLY_F_ROULETTE   = 1 << 13, /* the item slot is still spinning */
    RALLY_F_TURBO_READY= 1 << 14, /* a drift has charged a mini-turbo */
    RALLY_F_RETIRED    = 1 << 15  /* the seat left the match (defeated, disconnected or resigned):
                                     it races no further and is ranked last */
};

/* Control bits the eyes send for the local player (RALLY_INPUT event). */
enum {
    RALLY_IN_THROTTLE = 1 << 0,  /* W */
    RALLY_IN_BRAKE    = 1 << 1,  /* S: brake, then reverse */
    RALLY_IN_LEFT     = 1 << 2,  /* A */
    RALLY_IN_RIGHT    = 1 << 3,  /* D */
    RALLY_IN_DRIFT    = 1 << 4,  /* Shift */
    RALLY_IN_FIRE     = 1 << 5,  /* left mouse button held */
    RALLY_IN_ITEM     = 1 << 6,  /* Space or right mouse button (edge triggered by the brain) */
    RALLY_IN_RESCUE   = 1 << 7   /* R: put back on the course at the last checkpoint */
};

typedef struct RallyRacer {
    int32_t house;         /* HousesType of the seat */
    int32_t unit_id;       /* Units.ID() of the vehicle, the id= field of its UNIT line in the
                              object dump (-1 while dead or not yet picked) */
    int32_t unit_type;     /* UnitType of the chosen vehicle (-1 until picked) */
    int32_t x, y, z;       /* fine units */
    int32_t vx, vy, vz;    /* fine units per engine tick */
    int32_t yaw;           /* hull, 0..65535 */
    int32_t turret;        /* turret, 0..65535 (equal to yaw without a turret) */
    int32_t pitch, roll;   /* signed binary angles */
    int32_t speed;         /* forward speed, fine units per tick (negative reversing) */
    int32_t top_speed;     /* this vehicle's top speed, same units, for a speedometer */
    int32_t health, health_max;
    int32_t place;         /* 1-based live race position */
    int32_t progress;      /* 0..65536 along the course */
    int32_t checkpoint;    /* index of the last course point passed */
    int32_t finish_ticks;  /* race time at the line, -1 until finished */
    int32_t item;          /* RALLY_ITEM_*, shown spinning while RALLY_F_ROULETTE */
    int32_t item_count;    /* uses left (3 for NITRO3) */
    int32_t flags;         /* RALLY_F_* */
    int32_t flip_ticks;    /* ticks spent flipped (auto flip back at 3 s) */
    int32_t respawn_ticks; /* ticks until respawn while RALLY_F_DEAD */
    int32_t effect_ticks;  /* remaining ticks of boost/armour/cloak/spin */
    int32_t kills;         /* racers destroyed by this racer */
    /* ABI 2 */
    int32_t lap;           /* 1-based lap being driven (laps once finished) */
    int32_t lap_progress;  /* 0..65536 along the current lap; progress above is the whole race */
    int32_t last_vehicle;  /* roster index of this seat's vehicle in the previous race of the
                              match, -1 in the first race: the chooser's preselection on a retry */
} RallyRacer;

typedef struct RallyCrate {
    int32_t x, y;        /* fine units */
    int32_t active;      /* 1 while it can be picked up */
} RallyCrate;

/* Live projectiles and dropped things the brain simulates itself (engine weapons
 * fired from turrets are ordinary engine bullets and are drawn as such). */
enum {
    RALLY_HAZ_NONE = 0,
    RALLY_HAZ_MINE,      /* armed mine on the ground */
    RALLY_HAZ_ROCKET,    /* straight rocket */
    RALLY_HAZ_HOMING,    /* homing missile */
    RALLY_HAZ_ION_MARK   /* Ion Cannon target marker; strikes when ticks reaches 0 */
};
typedef struct RallyHazard {
    int32_t kind;        /* RALLY_HAZ_* */
    int32_t x, y, z;     /* fine units */
    int32_t yaw;
    int32_t owner;       /* racer index that made it */
    int32_t ticks;       /* age, or ticks left for ION_MARK */
} RallyHazard;

typedef struct RallyState {
    int32_t version;          /* RALLY_ABI_VERSION */
    int32_t phase;            /* RALLY_PHASE_* */
    int32_t phase_ticks;      /* ticks left in PICK/COUNTDOWN/DONE */
    int32_t race_ticks;       /* ticks since GO */
    int32_t ticks_per_second; /* engine ticks per second at this match's speed */
    int32_t local;            /* index in racer[] of this machine's player, -1 if none */
    int32_t racer_count;
    RallyRacer racer[RALLY_MAX_RACERS];
    int32_t finished_count;
    int32_t finish_order[RALLY_MAX_RACERS]; /* racer indices in finishing order */
    int32_t crate_count;
    RallyCrate crate[RALLY_MAX_CRATES];
    int32_t hazard_count;
    RallyHazard hazard[RALLY_MAX_HAZARDS];
    int32_t winner;           /* racer index of the winner, -1 before anyone finishes */
    /* ABI 2 */
    int32_t laps;             /* laps in this race (always 1 on a point-to-point course) */
    int32_t course_loop;      /* 1 when the course is a circuit: the last course point joins
                                 the first, and the finish line is the start line */
    int32_t race_number;      /* 1 for the match's first race, one more for every retry */
    int32_t decider;          /* racer index of the player whose choice ends the results: the
                                 lowest-seat human still in the match (the host of a network
                                 match), -1 when there is none and the match ends by itself */
    int32_t choice;           /* RALLY_CHOICE_* once made, RALLY_CHOICE_NONE while waiting */
} RallyState;

/* The course: the centre line from the start line round to the finish (on a circuit,
 * round to the point before the first: RallyState.course_loop), for the progress strip
 * and the AI. half_width is the driveable half width in fine units at that point. */
typedef struct RallyCoursePoint {
    int32_t x, y;
    int32_t half_width;
} RallyCoursePoint;

/* One engine tick of the local player's controls, for OpenCNC3D_Rally_Predict. */
#define RALLY_PREDICT_MAX 32
typedef struct RallyPredictStep {
    uint32_t bits;          /* RALLY_IN_* */
    int32_t aim_x, aim_y;   /* fine units */
} RallyPredictStep;

/* Brain exports (extern "C" __declspec(dllexport) ... __cdecl, resolved by the eyes with
 * dlsym/GetProcAddress). player_id is the same id the eyes passes to CNC_Advance_Instance;
 * the brain maps it to the local player's house (OpenCNC3D_House_Of_Player).
 *
 *   int OpenCNC3D_Rally_Configure(int laps);                                        (ABI 2)
 *       Before the scenario loads (before or after OpenCNC3D_Rally_Enable): the laps of
 *       the races of the next match, clamped to RALLY_LAPS_MIN..RALLY_LAPS_MAX
 *       (RALLY_LAPS_DEFAULT if never called). The number is kept until it is changed, so
 *       call it for every match. Every peer passes the same number, from the match setup.
 *       A point-to-point course is one run whatever it says. Returns the laps that will
 *       be raced on a circuit.
 *   int OpenCNC3D_Rally_Enable(int enabled, int ticks_per_second_x10);
 *       Before the scenario loads: makes the next match a Rally (or not) and tells the
 *       brain the real engine tick rate of the match (18 Hz is 180). Returns 1 if the
 *       brain has Rally. Every Rally speed and timer is worked out from that rate, so a
 *       host must keep the engine at it for the whole match (no game speed change while
 *       a Rally runs). The brain itself keeps the rules a race needs whatever the
 *       multiplayer options say: no random crates and no destroy-structures defeat.
 *   int OpenCNC3D_Rally_Get_State(uint64_t player_id, RallyState* out);
 *       Fills *out (out->local is that player's racer); returns 1 while a Rally match
 *       is running.
 *   int OpenCNC3D_Rally_Get_Course(RallyCoursePoint* out, int max);
 *       Copies up to max course points; returns how many the course has.
 *   int OpenCNC3D_Rally_Get_Roster(int32_t* unit_types, int max);
 *       The vehicles a player may choose, in chooser order; returns the count.
 *   int OpenCNC3D_Rally_Pick(uint64_t player_id, int roster_index);
 *       Queues that player's vehicle choice (a lockstep event).
 *   int OpenCNC3D_Rally_Input(uint64_t player_id, uint32_t bits, int32_t aim_x, int32_t aim_y);
 *       Queues that player's controls (a lockstep event). aim is the world point under
 *       the crosshair in fine units; the brain keeps the last value. Call it when the
 *       controls change, at most once per engine tick.
 *   int OpenCNC3D_Rally_Autopilot(uint64_t player_id, int on);
 *       Testing aid: the brain's AI driver drives that player's racer (a lockstep event,
 *       so it is the same on every peer). Used by headless runs that cannot press keys.
 *   int OpenCNC3D_Rally_Choose(uint64_t player_id, int choice);                     (ABI 2)
 *       Queues that player's RALLY_CHOICE_* (a lockstep event). The brain acts on it only
 *       in DONE and only from RallyState.decider; any other is ignored, so every peer
 *       acts on the same one. A retry resets the course to its start (defences rebuilt
 *       and repaired, crates back, hazards gone) and returns to PICK.
 *   int OpenCNC3D_Rally_Predict(uint64_t player_id, const RallyPredictStep* steps, int n,
 *                               RallyRacer* out);                                   (ABI 2)
 *       Where that player's racer will be once the controls it has already sent have
 *       executed. Starts from the racer as the newest executed tick left it, applies
 *       steps[0..n-1] (n <= RALLY_PREDICT_MAX), one engine tick each, through the race's
 *       own driving physics (throttle, brake, steering, drift and its mini-turbo, an
 *       active boost, a nitro fired from the item slot, grip, the ground under it, walls
 *       and the map's blocked cells, the turret turning to the aim), and writes the result
 *       to *out: the whole racer, its motion, turret and flags (drift, boost, airborne,
 *       turbo ready) predicted and the rest as executed. It leaves out what other machines
 *       decide or the random stream draws (the other racers and collisions with them,
 *       hits and throws, every other item, crates, the rescue key, wall damage, crossing
 *       the finish line), so the eyes draw the local racer and its camera from it and the
 *       next executed tick corrects it. Pure: it changes nothing in the simulation and
 *       draws no random number, so calling it any number of times on one peer cannot
 *       desync a match. Returns 1, or 0 when there is nothing to predict (not in RACE, or
 *       the racer dead, flipped, righting, spinning, retired or finished, or driven by an
 *       AI driver or the autopilot test aid); *out then holds the executed state, which
 *       the eyes draw. With n = 0 it returns 1 and the executed state.
 *   uint32_t OpenCNC3D_Rally_Hash(void);
 *       A hash of the race state, for desync checks between peers.
 */

/* Test hooks for headless runs. They act on the simulation directly instead of through
 * the order queue, so they belong to single machine tests only, never a lockstep match.
 *
 *   int OpenCNC3D_Rally_Debug(int op, int a, int b, int c);
 *       Returns the answer for a query, 1 or 0 for an action, -1 for a bad request.
 */
enum {
    RALLY_DEBUG_THROW = 1,             /* a racer, b upward speed cells/s x100, c roll deg/s */
    RALLY_DEBUG_KILL = 2,              /* a racer: destroyed through the engine damage path */
    RALLY_DEBUG_GIVE = 3,              /* a racer, b RALLY_ITEM_*, c uses */
    RALLY_DEBUG_CHECK = 4,             /* a racer: RALLY_CHECK_* bits read from the engine unit */
    RALLY_DEBUG_SEED = 5,              /* a: seed of the synchronised random stream, applied at
                                          the next Rally scenario load */
    RALLY_DEBUG_STAT = 6,              /* a RALLY_STAT_*: a counter */
    RALLY_DEBUG_STRENGTH = 7,          /* a racer: engine strength of its unit, -1 while dead */
    RALLY_DEBUG_HOSTILE_POWER = 8,     /* 1 when the hostile house has full power */
    RALLY_DEBUG_HOSTILE_BUILDINGS = 9, /* live buildings of the hostile house */
    RALLY_DEBUG_HOSTILE_AIMING = 10,   /* hostile objects whose target is a racer */
    RALLY_DEBUG_HOSTILE_SHOTS = 11,    /* shots the hostile house has fired so far; call once a tick */
    RALLY_DEBUG_HOSTILE_SHOTS_BY = 12, /* a StructType: of those shots, fired by that building type */
    RALLY_DEBUG_RAM_WALL = 13,         /* a racer: turned toward the nearest blocked cell within six
                                          cells and sent at it at b cells/s x100 */
    RALLY_DEBUG_STRAY_CRATES = 14,     /* crates on the map that are not at one of the course's crate
                                          spots (there must never be any) */
    RALLY_DEBUG_PICK = 15,             /* a racer, b roster index: that seat's vehicle, during PICK only
                                          (computer seats choose at random otherwise) */
    RALLY_DEBUG_BREAK = 16,            /* the a-th live building of the hostile house (heap order) takes
                                          b percent of its full strength through the engine's damage
                                          path; b >= 100 destroys it */
    RALLY_DEBUG_FIXTURES = 17,         /* a 0: the course objects recorded when the match started
                                          (buildings, units and soldiers of houses that are not seats,
                                          trees); a 1: how many of them stand as they started */
    RALLY_DEBUG_BITS = 18              /* a racer: the RALLY_IN_* controls it drives with now */
};

/* RALLY_DEBUG_CHECK bits. */
enum {
    RALLY_CHECK_UNIT = 1,     /* the racer has a live engine unit */
    RALLY_CHECK_BLOCKED = 2,  /* that unit's cell is blocked for its locomotion */
    RALLY_CHECK_OFF_MAP = 4,  /* that unit's cell is outside the playable map */
    RALLY_CHECK_DESYNC = 8    /* the unit's engine coordinate is not the racer's position */
};

/* RALLY_DEBUG_STAT counters. */
enum {
    RALLY_STAT_CRATES = 0,    /* crates picked up */
    RALLY_STAT_ITEMS,         /* items used */
    RALLY_STAT_HAZARDS,       /* mines, rockets, homing missiles and ion marks made */
    RALLY_STAT_BLASTS,        /* of those, exploded */
    RALLY_STAT_RESPAWNS,      /* racers put back after being destroyed */
    RALLY_STAT_FLIPS,         /* landings past 90 degrees */
    RALLY_STAT_RIGHTED,       /* automatic flips back completed */
    RALLY_STAT_SHOTS,         /* engine weapons fired by racers */
    RALLY_STAT_RUN_OVER,      /* infantry run over */
    RALLY_STAT_WALL_HITS,     /* hard hits on blocked cells that cost health */
    RALLY_STAT_RAMS,          /* hard racer on racer hits that cost health */
    RALLY_STAT_RESCUES,       /* rescues to the last checkpoint */
    RALLY_STAT_THROWN,        /* racers thrown into the air */
    RALLY_STAT_DEFENCE_HITS,  /* engine damage to racers from anything that is not a racer (the
                                 course's defences); walls, rams and items are not counted */
    RALLY_STAT_RACER_HITS,    /* engine weapon hits on a racer by another racer */
    RALLY_STAT_DEFENCE_DAMAGE,/* health taken from racers by the course's defences */
    RALLY_STAT_DEFENCE_KILLS, /* racers destroyed by the course's defences */
    RALLY_STAT_MINI_TURBOS,   /* mini-turbos earned by a drift */
    RALLY_STAT_COUNT
};

#endif
