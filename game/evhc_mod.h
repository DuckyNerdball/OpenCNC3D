/* ================================================================================
 * ENHANCED VEHICLES (evhc) -- the remastered vehicles and aircraft, drawn, animated and
 * broken apart by the renderer alone, under Enhanced.
 *
 * WHAT IT IS. A vehicle or aircraft type with a baked evhc/<TYPE>.evhc file is not drawn
 * from the cartridge's model under Enhanced. The file (tools/art/remaster/bake_evhc.py)
 * carries the remastered model as one piece per motion and a record per moving piece:
 *
 *   hull        everything fixed to the body
 *   turret      turns with the engine's turret facing, exactly as the cartridge's turret
 *               part does (the tanks, the Hum-vee, the buggy, the gun boat, the rocket
 *               launcher's launcher)
 *   belt, wflip a track's links scroll and its wheels step through their flipbook by how
 *               far that side of the vehicle has travelled
 *   wheel       rolls by its side's travel over its radius, and steers into a turn
 *   rotor       an aircraft's blades and, separately, their blur disc: blades on the
 *               ground, the disc in the air, spinning up on take-off and down on landing
 *   spin        a part turning at its own rate (the hovercraft's propellers, the Mobile
 *               HQ's radar)
 *   hinge       a door, a ramp, a hopper, a launcher's elevation, a duct's tilt, driven by
 *               the unit's own state
 *   clip        follows the cartridge's own animation of one of its parts (the
 *               harvester's collector arms, which work only while it lifts Tiberium:
 *               object_anim_t's harvester gate)
 *   deploy      one of the MCV's five groups, moved by the cartridge's deploy rig
 *   recoil      a barrel that kicks back along its axis when the unit fires
 *
 * and a DESTROYED set: the pieces the vehicle breaks into, each knowing the intact piece it
 * came from, so it starts exactly where that piece stood and turned.
 *
 * THE SIBLING OF ebld_mod.h. The file shares the remastered buildings' texture, piece and
 * triangle records, and this module keeps each model in the buildings' own EbldModel, so
 * the residency (textures read from the file the first time the model is drawn), the
 * skirmish liveries through the house mask, the pack meshes and the rigid-body solver are
 * the buildings' and are not copied. What is a vehicle's own is here: the part records, the
 * per-unit motion state, the draw with a matrix per piece, the dock and deploy rigs, and the
 * death.
 *
 * WHO OWNS WHAT. Everything is gated on one rule: the Enhanced picture is on, this switch
 * is on (--noevhc is the A/B), the object is a unit or an aircraft and a model is attached
 * for its type. Classic never reaches any of it. Under the rule, for such a unit: its
 * cartridge model and transport door stand down, the refinery's dock rig and the MCV's
 * deploy rig draw the remastered vehicle in the cartridge vehicle's place, the old unit
 * shatter does not spawn when it dies (this system's wreck does), and the deck riders on a
 * hovercraft stand on the remastered deck. The authored art set (experimental art) still
 * wins where it has a model, as it does over the remastered buildings.
 *
 * THE CLOCK. Every state change (travel, rotor spin, door, tilt, elevation, a shot, the
 * wreck's physics) advances on the engine tick edge (evhc_step), never per drawn frame;
 * draws lerp between the last two ticks with g_tickAlpha, which is 0 under --shot and
 * --script, so two runs of one script draw the same frames. Randomness is efx_hash keyed
 * on the unit's kind-qualified key, the tick and the piece.
 *
 * THE DRAW is fixed function throughout: every piece is a draw_mesh call with a 3x4 extra
 * matrix and a triangle range; a track's scroll and a wheel's flipbook frame are the
 * texture-coordinate hook the remastered buildings' water uses. Nothing here needs more
 * than GL 1.1.
 *
 * TIER 1. Enhanced does not exist on the Win98 build, so neither does this; see
 * docs/tier1-gap.md. The Classic path, the cartridge models and the old unit shatter, is
 * what that build keeps.
 * ================================================================================ */
#ifndef EVHC_MOD_H
#define EVHC_MOD_H

/* ---- the file's vocabulary (tools/art/remaster/bake_evhc.py) ---------------------- */
enum { EVP_SPIN = 0, EVP_TURRET, EVP_ROTOR, EVP_WHEEL, EVP_BELT, EVP_WFLIP, EVP_HINGE,
       EVP_CLIP, EVP_DEPLOY, EVP_RECOIL, EVP_N };
enum { EVD_NONE = 0, EVD_APC_DOOR, EVD_TRAN_RAMP, EVD_LST_BOW, EVD_C17_RAMP,
       EVD_HARV_HOPPER, EVD_ELEVATION, EVD_DUCT, EVD_LST_STERN, EVD_N };
enum { EVC_TRACKED = 0, EVC_WHEELED, EVC_HALF, EVC_HELI, EVC_PLANE, EVC_HOVER, EVC_BOAT };
enum { EVQ_CLASS = 0, EVQ_GAUGE, EVQ_SINK_AFTER, EVQ_SINK_TIME, EVQ_SINK_DEPTH, EVQ_SHADE,
       EVQ_DOCK_X, EVQ_DOCK_Z, EVQ_DOCK_YAW, EVQ_DECK, EVQ_N_MIN = 6 };
enum { EVF_DISC = 1 };
static const char* const EVP_NAME[EVP_N] = { "spin", "turret", "rotor", "wheel", "belt",
                                             "wflip", "hinge", "clip", "deploy", "recoil" };

struct EvhcPart { int piece, kind, parent, driver, side, flags; float a[12]; };
struct EvhcModel {
    /* THE BUILDINGS' STRUCT, so their residency, liveries, mesh filling and piece drawing
       serve these too: tex, set[EBS_INTACT] (the model) and set[EBS_DESTROYED] (the wreck,
       its parents INTACT pieces), path and size, the attachment (mesh[EBM_INTACT] and
       mesh[EBM_WRECK] with their piece runs in rng[]) and residency. */
    EbldModel m;
    std::vector<EvhcPart> part;
    std::vector<int> partOf;                  /* per INTACT piece, its part or -1 */
    /* per wreck piece: its launch up (a range), outward (a range), cells a second, and its
       spin, radians a second (the lab's blast by role) */
    std::vector<float> blast;
    int cls;
    float gauge;                              /* cells between the two sides' running gear */
    bool turret, rotors, discs;
    int  nbarrel;                             /* recoil parts */
    /* PER PACK: the rig parts the dock (PROCANIM) and the deploy (MCVANIM) move each
       piece with, and the deploy rig's frame fitted onto the remaster (evhc_attach). */
    std::vector<int> rigPart;
    float rigYaw, rigX, rigZ, rigErr;
    bool  rigOk;
    long  drawn;                              /* whole-model draws since the last dump */
    long  rigDrawn;                           /* drawn in the dock or deploy rig, likewise */
    /* RESIDENCY IN TWO HALVES: the textures the standing model draws with go to the card the
       first time it is drawn, the wreck's (its charred atlases, broken material and glow,
       more than half the bytes) only the first time one of its wrecks is. */
    std::vector<unsigned char> texUp;         /* per texture: on the card this pack */
    std::vector<unsigned char> texIntact;     /* per texture: the standing model uses it */
    bool intactUp, allUp;
};
static std::vector<EvhcModel> g_evhc;
static bool g_evhcLoaded = false;
/* --noevhc clears it: the A/B for every judgement about this system. */
static bool g_evhcOn = true;
static int  g_evhcPackSerial = 0;

/* ---- one unit's motion state, advanced on the tick edge ----------------------------- */
struct EvhcUnit {
    int   model, id, kind;
    char  type[16];
    int   tick;                               /* last tick advanced */
    bool  have;
    float ex, ez;                             /* last tick's engine position */
    int   face;                               /* last tick's engine facing (DirType) */
    float trav[2], travP[2];                  /* travel per side, cells: 0 right, 1 left */
    float v, w;                               /* this tick: cells forward, radians left */
    int   td;                                 /* this tick's turret delta (the death's aim) */
    float turn, turnP;                        /* steering, -1 (right) .. 1 (left) */
    float spin, spinP;                        /* rotor clock, seconds at full rate */
    float rate;                               /* rotor rate, 0..1 */
    float disc;                               /* the blur disc's opacity, 0..1 */
    float duct, ductP;                        /* the Orca's duct tilt, degrees */
    float elev, elevP;                        /* the launcher's elevation, degrees */
    float door, doorP;                        /* a door or ramp, 0 shut .. 1 open */
    int   shots;
    int   fire[4];                            /* the tick each barrel last fired */
    bool  seen;
    /* what the colour picture last drew, for the dump */
    int   dTur, dFrame[2];
    float dTrav[2], dRotor, dDisc, dDuct, dElev, dDoor, dTurn, dKick, dClip;
    bool  drawnNow;
};
static std::map<int, EvhcUnit> g_evhcUnit;
static int g_evhcLastTick = -1;

/* ---- a wreck: the DESTROYED pieces, falling, settling, sinking ---------------------- */
struct EvhcWreck {
    int   model, id, kind, house;
    char  type[16];
    int   deathTick, impactTick;
    bool  air, falling, sinking, dropped;
    bool  wet;                                /* it came to rest on a water cell */
    EbldBody carrier;                         /* an aircraft falls whole until it lands */
    std::vector<float> off;                   /* per piece: offset in the carrier, cells */
    std::vector<float> qoff;                  /* per piece: rotation in the carrier */
    std::vector<EbldBody> body;
    float wx, wz, ground;
};
static std::map<long long, EvhcWreck> g_evhcWreck;
static long long g_evhcWreckSerial = 0;

/* WATER IS THE MAP'S OWN: a cell with holes in its ground is one the sea shows through
   (the radar paints those blue). A wreck that comes to rest on one goes down at once,
   whatever it was: a boat, a hovercraft off the beach, an aircraft shot down over the sea.
   A hovercraft killed on dry sand stays on the sand like any vehicle. */
static bool evhc_cell_wet(float wx, float wz)
{
    const int cx = (int)floorf(wx), cz = (int)floorf(wz);
    return cx >= 0 && cx < g_gridW && cz >= 0 && cz < g_gridH
        && g_cellHoles[cz * g_gridW + cx] != 0;
}
static bool evhc_on_water(const EvhcWreck& E)
{
    return E.wet;
}

static bool evhc_finite(float f) { return f == f && f < 3.0e38f && f > -3.0e38f; }

/* ================================================================================
 * THE FILE. Read whole, checked completely, refused whole on any inconsistency.
 * ============================================================================== */
static bool evhc_read_one(const char* path, EvhcModel& V)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    std::vector<unsigned char> b;
    {
        unsigned char chunk[65536];
        size_t n;
        while ((n = fread(chunk, 1, sizeof chunk, f)) > 0)
            b.insert(b.end(), chunk, chunk + n);
    }
    fclose(f);
    EbldModel& m = V.m;
    EbldIn in;
    in.b = &b; in.o = 0; in.bad = false; in.why[0] = 0;
    char msg[200];
    char magic[4] = { 0, 0, 0, 0 }, type[9];
    in.take(magic, 4);
    if (!in.bad && memcmp(magic, "EVHC", 4) != 0) in.fail("it is not an EVHC file");
    const uint32_t ver = in.u32();
    if (!in.bad && ver != 1) {
        snprintf(msg, sizeof msg, "it is format version %u and this build reads 1", ver);
        in.fail(msg);
    }
    memset(type, 0, sizeof type);
    in.take(type, 8);
    type[8] = 0;
    const uint32_t ntex = in.u32(), nset = in.u32(), npart = in.u32(), nparam = in.u32();
    if (!in.bad && (ntex == 0 || ntex > 64)) in.fail("its texture count is out of range");
    if (!in.bad && nset != 1 && nset != 2) in.fail("it carries neither one set nor two");
    if (!in.bad && npart > 64) in.fail("its part count is out of range");
    if (!in.bad && (nparam < EVQ_N_MIN || nparam > 256))
        in.fail("its parameter table is too short or too long");
    m.type = type;
    m.tag = "evhc";
    m.version = 3;
    m.param.clear();
    for (uint32_t i = 0; i < nparam && !in.bad; i++) m.param.push_back(in.f32());
    /* the textures and the sets are the buildings' records; the wreck is the buildings'
       DESTROYED set, its parents the INTACT pieces */
    static const int SETOF[2] = { EBS_INTACT, EBS_DESTROYED };
    ebld_read_body(in, b, m, 3, ntex, nset, 11, SETOF);
    m.nset = (int)nset;
    const int nip = (int)m.set[EBS_INTACT].piece.size();
    if (!in.bad)
        for (size_t k = 0; k < m.set[EBS_DESTROYED].piece.size(); k++)
            if (m.set[EBS_DESTROYED].piece[k].parent < 0
                || m.set[EBS_DESTROYED].piece[k].parent >= nip) {
                in.fail("a wreck piece names an intact piece that does not exist");
                break;
            }
    V.part.clear();
    int last = 0;
    for (uint32_t k = 0; k < npart && !in.bad; k++) {
        EvhcPart p;
        p.piece = in.u16();
        p.kind = in.u8();
        p.parent = (signed char)in.u8();
        p.driver = in.u8();
        p.side = in.u8();
        p.flags = in.u8();
        (void)in.u8();
        for (int c = 0; c < 12; c++) p.a[c] = in.f32();
        if (in.bad) break;
        if (p.piece <= last || p.piece >= nip || p.kind >= EVP_N || p.parent >= (int)k
            || p.parent < -1 || p.driver >= EVD_N || p.side > 2) {
            snprintf(msg, sizeof msg, "part %u is out of order or impossible", k);
            in.fail(msg);
            break;
        }
        last = p.piece;
        V.part.push_back(p);
    }
    V.blast.clear();
    if (!in.bad) {
        const uint32_t nb = in.u32();
        if (!in.bad && nb != (uint32_t)m.set[EBS_DESTROYED].piece.size())
            in.fail("its blast table does not match the wreck's pieces");
        for (uint32_t k = 0; k < nb * 5 && !in.bad; k++) V.blast.push_back(in.f32());
    }
    if (!in.bad && in.o != b.size()) {
        snprintf(msg, sizeof msg, "%lu bytes follow the blast table",
                 (unsigned long)(b.size() - in.o));
        in.fail(msg);
    }
    if (!in.bad && (int)V.part.size() != nip - 1)
        in.fail("an intact piece moves with no part record, or the hull does");
    if (in.bad) {
        fprintf(stderr, "evhc: %s refused: %s\n", path, in.why);
        return false;
    }
    m.path = path;
    m.fileSize = (long)b.size();
    m.flags = 0;
    m.emit.clear();
    m.move.clear();
    m.moveOf.assign(nip, -1);
    m.emitState[0] = m.emitState[1] = false;
    m.nchip = 0;
    m.attached = false;
    m.tex0 = -1;
    for (int i = 0; i < EBM_N; i++) m.mesh[i] = -1;
    m.shadow0 = m.shadow1 = 0;
    m.drawn = 0;
    m.resident = false;
    for (int s = 0; s < EBS_N; s++) { m.set[s].hasSlab = false; m.set[s].slabTop = 0.0f; }
    V.partOf.assign(nip, -1);
    V.turret = V.rotors = V.discs = false;
    V.nbarrel = 0;
    for (size_t k = 0; k < V.part.size(); k++) {
        const EvhcPart& p = V.part[k];
        V.partOf[p.piece] = (int)k;
        if (p.kind == EVP_TURRET) V.turret = true;
        if (p.kind == EVP_ROTOR) { V.rotors = true; if (p.flags & EVF_DISC) V.discs = true; }
        if (p.kind == EVP_RECOIL) V.nbarrel++;
    }
    V.cls = (int)m.param[EVQ_CLASS];
    V.gauge = m.param[EVQ_GAUGE];
    V.rigOk = false;
    V.rigYaw = V.rigX = V.rigZ = V.rigErr = 0.0f;
    V.drawn = 0;
    V.rigDrawn = 0;
    return true;
}

/* Every vehicle and aircraft the engine knows, scanned by name, so installing a new one is
   dropping its file in. Once per process: the files do not depend on the mission. */
static void evhc_load(const char* dir)
{
    if (g_evhcLoaded) return;
    g_evhcLoaded = true;
    static const char* const TYPES[] = {
        "HTNK", "MTNK", "LTNK", "FTNK", "STNK", "APC", "ARTY", "MSAM", "HARV", "JEEP",
        "BGGY", "BIKE", "MCV", "MHQ", "BOAT", "LST", "HELI", "ORCA", "TRAN", "A10", "C17",
        "MLRS"
    };
    std::string names;
    for (size_t i = 0; i < sizeof TYPES / sizeof TYPES[0]; i++) {
        char path[512];
        snprintf(path, sizeof path, "%sevhc/%s.evhc", dir && *dir ? dir : "", TYPES[i]);
        EvhcModel V;
        if (!evhc_read_one(path, V))
            continue;
        if (V.m.type != TYPES[i]) {
            fprintf(stderr, "evhc: %s says it is %s -- refused\n", path, V.m.type.c_str());
            continue;
        }
        names += (names.empty() ? "" : " ") + V.m.type;
        g_evhc.push_back(V);
    }
    fprintf(stderr, "evhc: %d remastered vehicle%s and aircraft for Enhanced%s%s\n",
            (int)g_evhc.size(), g_evhc.size() == 1 ? "" : "s",
            names.empty() ? "" : ": ", names.c_str());
}

/* ---- small matrix helpers: 3x4 row-major, v' = M v, pack units ---------------------- */
static void ev_ident(float* M)
{
    static const float I[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
    memcpy(M, I, sizeof I);
}
/* R = A * B: B first, then A */
static void ev_mul(const float* A, const float* B, float* R)
{
    float o[12];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++)
            o[i * 4 + j] = A[i * 4] * B[j] + A[i * 4 + 1] * B[4 + j] + A[i * 4 + 2] * B[8 + j];
        o[i * 4 + 3] = A[i * 4] * B[3] + A[i * 4 + 1] * B[7] + A[i * 4 + 2] * B[11] + A[i * 4 + 3];
    }
    memcpy(R, o, sizeof o);
}
static void ev_apply(const float* M, const float* v, float* o)
{
    for (int i = 0; i < 3; i++)
        o[i] = M[i * 4] * v[0] + M[i * 4 + 1] * v[1] + M[i * 4 + 2] * v[2] + M[i * 4 + 3];
}
static bool ev_is_ident(const float* M)
{
    static const float I[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
    for (int i = 0; i < 12; i++) if (fabsf(M[i] - I[i]) > 1e-6f) return false;
    return true;
}

/* THE CARTRIDGE'S CLIP, for one part at one time: the same lerp between two baked frames
   draw_mesh makes for its own animated parts, handed out as the matrix instead of spent on
   the cartridge's triangles. False (identity) when the mesh or part carries none. `vis`
   takes whether the node is drawn on that frame. */
static bool evhc_anim_mat(int mi, int part, float animT, float* M, bool* vis)
{
    ev_ident(M);
    if (vis) *vis = true;
    if (mi < 0 || mi >= (int)g_pack.mesh.size() || animT < 0.0f) return false;
    const PackMesh& mesh = g_pack.mesh[mi];
    const size_t np = mesh.parts.size();
    if (mesh.animFrames <= 0 || part < 0 || part >= (int)np
        || mesh.animMat.size() < (size_t)mesh.animFrames * np * 12)
        return false;
    const int nf = mesh.animFrames;
    int f0 = (int)animT;
    float mix = animT - (float)f0;
    if (f0 >= nf) { f0 = nf - 1; mix = 0.0f; }
    int f1 = f0 + 1;
    if (f1 >= nf) f1 = nf - 1;
    if (vis && !mesh.animVis.empty()) *vis = mesh.animVis[(size_t)f0 * np + part] != 0;
    const float* m0 = &mesh.animMat[((size_t)f0 * np + part) * 12];
    const float* m1 = &mesh.animMat[((size_t)f1 * np + part) * 12];
    for (int q = 0; q < 12; q++) M[q] = m0[q] + (m1[q] - m0[q]) * mix;
    return true;
}

/* A rotation about the vertical by `ang` radians in draw_mesh's own sense (its turret
   and facing turn: x' = x c + z s, z' = -x s + z c), about a point in the plane. */
static void ev_yaw_about(float px, float pz, float ang, float* M)
{
    const float c = cosf(ang), s = sinf(ang);
    const float R[12] = { c, 0, s, px - c * px - s * pz,
                          0, 1, 0, 0,
                          -s, 0, c, pz + s * px - c * pz };
    memcpy(M, R, sizeof R);
}

/* ================================================================================
 * ATTACHING TO THE PACK just loaded: textures reserved for residency, the intact and
 * the wreck meshes built, and the dock and deploy rigs matched to the pieces.
 * ============================================================================== */
static int procrig_mesh(void);
static int mcvrig_mesh(void);

/* The centre of a rig part's triangles, pack units, at its rest (frame 0 of its clip). */
static bool evhc_rig_centre(int rmi, int part, float* c)
{
    if (rmi < 0 || rmi >= (int)g_pack.mesh.size()) return false;
    const PackMesh& rm = g_pack.mesh[rmi];
    if (part < 0 || part >= (int)rm.parts.size() || rm.parts[part].ntris <= 0) return false;
    double s[3] = { 0, 0, 0 };
    int n = 0;
    float A[12];
    bool vis = true;
    evhc_anim_mat(rmi, part, 0.0f, A, &vis);
    for (int t = rm.parts[part].tri0; t < rm.parts[part].tri0 + rm.parts[part].ntris; t++)
        for (int k = 0; k < 3; k++) {
            const float v[3] = { rm.tris[t].v[k].x, rm.tris[t].v[k].y, rm.tris[t].v[k].z };
            float w[3];
            ev_apply(A, v, w);
            for (int q = 0; q < 3; q++) s[q] += w[q];
            n++;
        }
    if (!n) return false;
    for (int q = 0; q < 3; q++) c[q] = (float)(s[q] / n);
    return true;
}
/* The centre of an intact piece, pack units. */
static void evhc_piece_centre(const EvhcModel& V, int k, float* c)
{
    const EbldPiece& P = V.m.set[EBS_INTACT].piece[k];
    for (int q = 0; q < 3; q++) c[q] = P.c[q];
}

/* THE HARVESTER'S PLACE IN THE REFINERY'S DOCK, from the file (build_HARV.py's fit of the
   remaster onto the rig's own harvester): a turn and a shift in the refinery's frame. */
static void evhc_dock_matrix(const EvhcModel& V, float* M)
{
    const float yaw = V.m.param.size() > EVQ_DOCK_YAW ? V.m.param[EVQ_DOCK_YAW] : 0.0f;
    const float dx = V.m.param.size() > EVQ_DOCK_X ? V.m.param[EVQ_DOCK_X] : 0.0f;
    const float dz = V.m.param.size() > EVQ_DOCK_Z ? V.m.param[EVQ_DOCK_Z] : 0.0f;
    const float c = cosf(yaw), s = sinf(yaw);
    const float R[12] = { c, 0, s, dx,  0, 1, 0, 0,  -s, 0, c, dz };
    memcpy(M, R, sizeof R);
}

static void evhc_attach(void)
{
    g_evhcPackSerial++;
    if (g_evhc.empty()) return;
    int attached = 0;
    for (size_t i = 0; i < g_evhc.size(); i++) {
        EvhcModel& V = g_evhc[i];
        EbldModel& M = V.m;
        M.attached = false;
        V.rigOk = false;
        V.rigPart.assign(M.set[EBS_INTACT].piece.size(), -1);
        for (int k = 0; k < EBM_N; k++) { M.mesh[k] = -1; M.rng[k].clear(); }
        std::map<std::string, PackType>::const_iterator it = g_pack.type.find(M.type);
        if (it == g_pack.type.end()) continue;
        M.tex0 = (int)g_pack.tex.size();
        M.resident = false;
        V.texUp.assign(M.tex.size(), 0);
        V.texIntact.assign(M.tex.size(), 0);
        V.intactUp = V.allUp = false;
        for (size_t t = 0; t < M.set[EBS_INTACT].tri.size(); t++) {
            const int ti = M.set[EBS_INTACT].tri[t].tex;
            if (ti >= 0 && ti < (int)M.tex.size()) V.texIntact[ti] = 1;
        }
        for (size_t t = 0; t < M.tex.size(); t++) {
            PackTex pt;
            pt.w = pt.uw = M.tex[t].w;
            pt.h = pt.uh = M.tex[t].h;
            pt.gl = pt.gl_gdi = 0;
            for (int e = 0; e < 6; e++) pt.gl_extra[e] = 0;
            g_pack.tex.push_back(pt);
        }
        const char* ty = M.type.c_str();
        {
            PackMesh pm;
            memset(pm.name, 0, sizeof pm.name);
            snprintf(pm.name, sizeof pm.name, "evhc:%s:intact", ty);
            ebld_fill_mesh(pm, M, M.set[EBS_INTACT], EBT_TEX, -1, M.rng[EBM_INTACT]);
            M.shadow0 = M.shadow1 = (int)pm.tris.size();
            M.mesh[EBM_INTACT] = ebld_mesh_end(pm);
        }
        if (M.nset == 2) {
            PackMesh pm;
            memset(pm.name, 0, sizeof pm.name);
            snprintf(pm.name, sizeof pm.name, "evhc:%s:wreck", ty);
            ebld_fill_mesh(pm, M, M.set[EBS_DESTROYED], EBT_TEX, -1, M.rng[EBM_WRECK]);
            M.mesh[EBM_WRECK] = ebld_mesh_end(pm);
            /* and its glow: the triangles with an emit map, drawn additively over it */
            PackMesh pg;
            memset(pg.name, 0, sizeof pg.name);
            snprintf(pg.name, sizeof pg.name, "evhc:%s:glowx", ty);
            ebld_fill_mesh(pg, M, M.set[EBS_DESTROYED], EBT_GLOW, -1, M.rng[EBM_GLOWX]);
            M.mesh[EBM_GLOWX] = ebld_mesh_end(pg);
        }
        M.attached = true;
        attached++;
        /* THE RIGS. The refinery's dock rig moves the harvester's hull as its part 0 and
           its hopper as part 2 (the file's hinge record names it); its collector arms
           ride whichever rig part lies nearest their own centre once the harvester is in
           the dock. The MCV's deploy groups name their rig part in the file. */
        const EbldSet& S = M.set[EBS_INTACT];
        if (M.type == "HARV") {
            const int rmi = procrig_mesh();
            if (rmi >= 0 && rmi < (int)g_pack.mesh.size()) {
                float D[12];
                evhc_dock_matrix(V, D);
                const int nrp = (int)g_pack.mesh[rmi].parts.size();
                for (size_t k = 0; k < S.piece.size(); k++) {
                    const int pi = V.partOf[k];
                    int rp = 0;
                    if (pi >= 0 && V.part[pi].kind == EVP_HINGE
                        && V.part[pi].driver == EVD_HARV_HOPPER)
                        rp = (int)V.part[pi].a[9];
                    else if (pi >= 0 && V.part[pi].kind == EVP_CLIP) {
                        float c[3], w[3];
                        evhc_piece_centre(V, (int)k, c);
                        ev_apply(D, c, w);
                        float best = 1e30f;
                        for (int p = 1; p < nrp; p++) {
                            if (p == 2) continue;
                            float rc[3];
                            if (!evhc_rig_centre(rmi, p, rc)) continue;
                            const float d = (rc[0] - w[0]) * (rc[0] - w[0])
                                          + (rc[2] - w[2]) * (rc[2] - w[2]);
                            if (d < best) { best = d; rp = p; }
                        }
                    }
                    if (rp < 0 || rp >= nrp) rp = 0;
                    V.rigPart[k] = rp;
                }
                V.rigOk = true;
            }
        }
        if (M.type == "MCV") {
            /* THE DEPLOY RIG'S FRAME, fitted: the rig holds the MCV turned and shifted from
               the way it drives. The best turn and shift in the plane that puts the five
               deploy groups' centres onto their rig parts' centres at the rig's first
               frame (a two-dimensional Procrustes fit, closed form). */
            const int rmi = mcvrig_mesh();
            std::vector<float> A, B;
            for (size_t k = 0; k < S.piece.size(); k++) {
                const int pi = V.partOf[k];
                if (pi < 0 || V.part[pi].kind != EVP_DEPLOY) continue;
                const int rp = (int)V.part[pi].a[9];
                float rc[3], c[3];
                if (!evhc_rig_centre(rmi, rp, rc)) continue;
                evhc_piece_centre(V, (int)k, c);
                A.push_back(c[0]); A.push_back(c[2]);
                B.push_back(rc[0]); B.push_back(rc[2]);
                V.rigPart[k] = rp;
            }
            const int n = (int)A.size() / 2;
            if (n >= 2) {
                double ax = 0, az = 0, bx = 0, bz = 0;
                for (int j = 0; j < n; j++) { ax += A[2*j]; az += A[2*j+1]; bx += B[2*j]; bz += B[2*j+1]; }
                ax /= n; az /= n; bx /= n; bz /= n;
                double sc = 0, ss = 0;
                for (int j = 0; j < n; j++) {
                    const double x = A[2*j] - ax, z = A[2*j+1] - az;
                    const double X = B[2*j] - bx, Z = B[2*j+1] - bz;
                    /* the angle in ev_yaw_about's sense: X = x c + z s, Z = -x s + z c */
                    sc += x * X + z * Z;
                    ss += z * X - x * Z;
                }
                const double ang = atan2(ss, sc);
                const double c = cos(ang), s = sin(ang);
                V.rigYaw = (float)ang;
                V.rigX = (float)(bx - (ax * c + az * s));
                V.rigZ = (float)(bz - (-ax * s + az * c));
                double err = 0;
                for (int j = 0; j < n; j++) {
                    const double x = A[2*j] * c + A[2*j+1] * s + V.rigX - B[2*j];
                    const double z = -A[2*j] * s + A[2*j+1] * c + V.rigZ - B[2*j+1];
                    err += sqrt(x * x + z * z);
                }
                V.rigErr = (float)(err / n);
                V.rigOk = true;
                /* the running gear rides its group: the same rig part as its parent */
                for (size_t k = 0; k < S.piece.size(); k++) {
                    int pi = V.partOf[k];
                    while (pi >= 0 && V.part[pi].kind != EVP_DEPLOY) pi = V.part[pi].parent;
                    if (pi >= 0) V.rigPart[k] = (int)V.part[pi].a[9];
                }
                fprintf(stderr, "evhc: MCV deploy rig fitted: turned %.1f degrees, shifted "
                        "%.3f, %.3f cells, %.3f cells off on average over %d groups\n",
                        V.rigYaw * 180.0f / 3.14159265f, V.rigX * MODEL_SCALE,
                        V.rigZ * MODEL_SCALE, V.rigErr * MODEL_SCALE, n);
            }
        }
    }
    fprintf(stderr, "evhc: %d of %d remastered vehicles and aircraft attached to %s\n",
            attached, (int)g_evhc.size(), g_pack.scen);
}

/* THE UPLOAD, the buildings' (ebld_resident) done for a subset: the standing model's
   textures, or every one. Read back out of the file, which must still be the one checked;
   bled, filtered and wrapped as a remastered building's; its liveries painted if the pack's
   are already built. A file that cannot be read back detaches the model, and the cartridge
   draws its type. */
static bool evhc_resident(EvhcModel& V, bool all)
{
    EbldModel& M = V.m;
    if (all ? V.allUp : V.intactUp) return true;
    if (!M.attached) return false;
    const Uint64 t0 = SDL_GetPerformanceCounter();
    GLint bound = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    FILE* tf = fopen(M.path.c_str(), "rb");
    if (tf) {
        fseek(tf, 0, SEEK_END);
        if (ftell(tf) != M.fileSize) { fclose(tf); tf = NULL; }
    }
    bool readOk = (tf != NULL);
    long bytes = 0;
    int n = 0;
    const size_t tex0 = (size_t)M.tex0;
    for (size_t t = 0; t < M.tex.size() && readOk; t++) {
        if (V.texUp[t] || (!all && !V.texIntact[t])) continue;
        const EbldTex& T = M.tex[t];
        const size_t sz = (size_t)T.w * T.h * 4;
        std::vector<unsigned char> rgba(sz), nod, mask;
        fseek(tf, T.off, SEEK_SET);
        readOk = fread(&rgba[0], 1, sz, tf) == sz;
        if (readOk && (T.house & 1)) { nod.resize(sz); readOk = fread(&nod[0], 1, sz, tf) == sz; }
        if (readOk && (T.house & 2)) {
            mask.resize(sz / 4);
            readOk = fread(&mask[0], 1, sz / 4, tf) == sz / 4;
        }
        if (!readOk || tex0 + t >= g_pack.tex.size()) { readOk = false; break; }
        PackTex& pt = g_pack.tex[tex0 + t];
        if (nod.empty()) {
            pt.gl = n64fix_upload(rgba, T.w, T.h);
            bytes += (long)sz;
        } else {
            pt.gl = n64fix_upload(nod, T.w, T.h);
            pt.gl_gdi = n64fix_upload(rgba, T.w, T.h);
            pt.gdi.swap(rgba);
            pt.mask.swap(mask);
            bytes += 2 * (long)sz;
        }
        V.texUp[t] = 1;
        livery_build_range(tex0 + t, tex0 + t + 1);
        n++;
    }
    if (tf) fclose(tf);
    glBindTexture(GL_TEXTURE_2D, (GLuint)bound);
    if (!readOk) {
        fprintf(stderr, "evhc: %s changed, went away or could not be read back -- %s is drawn "
                        "by the cartridge from here on\n", M.path.c_str(), M.type.c_str());
        M.attached = false;
        return false;
    }
    V.intactUp = true;
    if (all) V.allUp = true;
    M.resident = V.allUp;
    g_ebldResidentBytes += bytes;
    fprintf(stderr, "evhc: %s %s textures on the card, %d textures, %.1f MB (%.1f MB of "
            "remastered textures in all), %.1f ms\n", M.type.c_str(),
            all ? "wreck" : "standing", n, (double)bytes / 1048576.0,
            (double)g_ebldResidentBytes / 1048576.0,
            (double)(SDL_GetPerformanceCounter() - t0) * 1000.0
                / (double)SDL_GetPerformanceFrequency());
    return true;
}

/* ---- the rule ---------------------------------------------------------------------- */
static bool evhc_rule_now(void) { return g_evhcOn && g_fx.enabled; }

static bool evhc_meshes_ok(const EvhcModel& V)
{
    const EbldModel& M = V.m;
    if (!M.attached) return false;
    if (M.mesh[EBM_INTACT] < 0 || M.mesh[EBM_INTACT] >= (int)g_pack.mesh.size()) return false;
    if (M.nset == 2 && (M.mesh[EBM_WRECK] < 0 || M.mesh[EBM_WRECK] >= (int)g_pack.mesh.size()))
        return false;
    return true;
}
static int evhc_model_of(const char* type)
{
    for (size_t i = 0; i < g_evhc.size(); i++) {
        const EvhcModel& V = g_evhc[i];
        if (V.m.attached && V.m.type == type) return evhc_meshes_ok(V) ? (int)i : -1;
    }
    return -1;
}
/* The authored art set's choice for an object (model3d_mod.h). */
static int object_model3d(const SimObject& o);
/* Whether this system draws this object: a unit or aircraft of a type with a model, under
   the rule, where the authored art set has none. */
static int evhc_for(const SimObject& o)
{
    if ((o.kind != K_UNIT && o.kind != K_AIRCRAFT) || g_evhc.empty() || !evhc_rule_now())
        return -1;
    const int m = evhc_model_of(o.type);
    if (m < 0) return -1;
    if (object_model3d(o) >= 0) return -1;
    return m;
}

/* The deck the hovercraft's riders stand on: the remaster's, where the hovercraft is
   drawn remastered; the cartridge's otherwise. */
static float evhc_deck_y(const char* type, float cart)
{
    if (!evhc_rule_now()) return cart;
    const int m = evhc_model_of(type);
    if (m < 0) return cart;
    const EvhcModel& V = g_evhc[m];
    if ((int)V.m.param.size() <= EVQ_DECK || V.m.param[EVQ_DECK] <= 0.0f) return cart;
    return V.m.param[EVQ_DECK];
}

/* The hull's facing: PrimaryFacing for a vehicle (the rocket launcher's too: its launcher
   is a turret part here and follows the turret facing on its own), the aircraft's body
   facing for an aircraft, both through the type's authored-forward bias. */
static int evhc_face(const SimObject& o)
{
    if (o.kind == K_AIRCRAFT) return draw_facing(o);
    if (o.face < 0) return -1;
    const int bias = g_nofacefix ? (((int)g_faceBias) & 255) : model_face_bias(o.type);
    return (o.face + bias) & 255;
}
static int evhc_turret_delta(const EvhcModel& V, const SimObject& o)
{
    if (!V.turret || o.kind != K_UNIT || o.face < 0 || o.tface < 0 || g_legacy) return 0;
    return (o.tface - o.face) & 255;
}

/* ================================================================================
 * THE TICK. From the deduplicated tick edge, after efx_step (the shots are read off the
 * newborn muzzle anims) and after the smoothing window has been recorded.
 * ============================================================================== */
static EvhcUnit* evhc_unit(const SimObject& o, int m)
{
    const int key = smooth_key(o);
    std::map<int, EvhcUnit>::iterator it = g_evhcUnit.find(key);
    if (it != g_evhcUnit.end() && it->second.model == m && !strcmp(it->second.type, o.type))
        return &it->second;
    EvhcUnit U;
    memset(&U, 0, sizeof U);
    U.model = m;
    U.id = o.id;
    U.kind = (int)o.kind;
    snprintf(U.type, sizeof U.type, "%s", o.type);
    U.tick = -1;
    for (int b = 0; b < 4; b++) U.fire[b] = -100000;
    /* A craft first seen in the air is already flying: its rotor at speed, its disc up. */
    if (o.kind == K_AIRCRAFT && o.alt > 0) { U.rate = 1.0f; U.disc = o.alt >= 8 ? 1.0f : o.alt / 8.0f; }
    else U.rate = 0.35f;
    U.elev = U.elevP = 18.7f;
    g_evhcUnit[key] = U;
    return &g_evhcUnit[key];
}

static float ev_ease(float cur, float want, float step)
{
    if (cur < want) return cur + step > want ? want : cur + step;
    if (cur > want) return cur - step < want ? want : cur - step;
    return cur;
}

/* THE DOOR'S TARGET, 0 shut .. 1 open, from what the unit itself says. */
static float evhc_door_want(const EvhcModel& V, const SimObject& o, int driver)
{
    switch (driver) {
    case EVD_APC_DOOR:
    case EVD_TRAN_RAMP:
        /* The engine's own door: for a unit or an aircraft the brain exports -1 while it
           is shut, 0 while it closes, 1 + Door_Stage() while it opens or stands open
           (dllinterface.cpp). An older brain says -1 always, and the door stays shut. */
        return o.doorstage >= 1 ? 1.0f : 0.0f;
    case EVD_LST_BOW:
        /* NOTHING IN THE ENGINE OPENS A HOVERCRAFT'S RAMP; the renderer decides from its
           unload: a hovercraft on its unload run that has stopped (no destination, not
           driving) is at the beach putting its cargo ashore, and raises the ramp as it
           drives off again. */
        return (!strcmp(o.mission, "Unload") && !o.navset && !o.driving) ? 1.0f : 0.0f;
    default:
        (void)V;
        return 0.0f;
    }
}

static void evhc_note_fire(int key, int frame);
static void evhc_wreck_step(int frame);

static void evhc_step(int frame)
{
    if (g_evhc.empty()) return;
    if (frame == g_evhcLastTick) return;
    if (frame < g_evhcLastTick) { g_evhcUnit.clear(); g_evhcWreck.clear(); }
    g_evhcLastTick = frame;
    const bool rule = evhc_rule_now();
    for (std::map<int, EvhcUnit>::iterator it = g_evhcUnit.begin(); it != g_evhcUnit.end(); ++it)
        it->second.seen = false;
    for (size_t i = 0; rule && i < g_objects.size(); i++) {
        const SimObject& o = g_objects[i];
        if ((o.kind != K_UNIT && o.kind != K_AIRCRAFT) || o.id < 0) continue;
        const int m = evhc_model_of(o.type);
        if (m < 0) continue;
        const EvhcModel& V = g_evhc[m];
        EvhcUnit* U = evhc_unit(o, m);
        U->seen = true;
        for (int s = 0; s < 2; s++) U->travP[s] = U->trav[s];
        U->turnP = U->turn;
        U->spinP = U->spin;
        U->ductP = U->duct;
        U->elevP = U->elev;
        U->doorP = U->door;
        if (o.limbo) { U->have = false; U->tick = frame; continue; }
        /* TRAVEL, from the engine's own position: the forward distance along the hull's
           heading this tick, and the turn from the facing's change. Each side's travel is
           v - w g / 2 (left) and v + w g / 2 (right), w positive in a left turn, so turning
           on the spot runs the two sides opposite and reversing runs both backward. A jump
           of more than a cell is not travel (a unit put down somewhere new). */
        float v = 0.0f, w = 0.0f;
        if (U->have && o.face >= 0 && U->face >= 0) {
            const float dx = o.ex - U->ex, dz = o.ez - U->ez;
            if (dx * dx + dz * dz <= 1.0f) {
                const float a = (float)o.face * (2.0f * 3.14159265f / 256.0f);
                v = dx * sinf(a) - dz * cosf(a);      /* heading: sin a east, -cos a south */
                const int df = ((o.face - U->face + 128) & 255) - 128;
                w = -(float)df * (2.0f * 3.14159265f / 256.0f);
            }
        }
        U->v = v;
        U->w = w;
        const float g = V.gauge;
        U->trav[1] += v - w * g * 0.5f;
        U->trav[0] += v + w * g * 0.5f;
        /* THE STEER, as the viewer's: (right - left) / 2v of full lock, eased so a one-tick
           facing step does not flick the wheels */
        float want = 0.0f;
        if (fabsf(w) > 1e-6f) {
            want = (fabsf(v) > 1e-4f) ? (w * g) / (2.0f * fabsf(v)) : (w > 0 ? 1.0f : -1.0f);
            if (want > 1.0f) want = 1.0f;
            if (want < -1.0f) want = -1.0f;
        }
        U->turn += (want - U->turn) * 0.35f;
        U->ex = o.ex; U->ez = o.ez;
        U->face = o.face;
        U->td = evhc_turret_delta(V, o);
        U->have = true;
        /* THE ROTORS: idling on the ground at the cartridge's landed rate (half, and a
           little under: the blades are seen there), up to speed as the craft lifts, over
           about a second each way; the blur disc comes up over the first third of the
           climb and goes as the craft settles. */
        if (V.rotors) {
            const float rw = (o.kind == K_AIRCRAFT && o.alt > 0) ? 1.0f : 0.35f;
            U->rate = ev_ease(U->rate, rw, 0.045f);
            U->spin += U->rate / 15.0f;
            float dw = o.kind == K_AIRCRAFT ? (float)o.alt / 8.0f : 0.0f;
            if (dw > 1.0f) dw = 1.0f;
            U->disc = ev_ease(U->disc, dw, 0.08f);
        } else {
            U->spin += 1.0f / 15.0f;
        }
        /* THE ORCA'S DUCTS: tilted forward with its speed at flight level (90 degrees at
           the engine's fast speed, 0.156 cells a tick), upright while it hovers, lands or
           takes off, eased at 90 degrees a second */
        {
            float want = 0.0f;
            if (o.kind == K_AIRCRAFT && o.alt >= 24) {
                const float sp = sqrtf(U->v * U->v) / 0.15625f;
                want = 90.0f * (sp > 1.0f ? 1.0f : sp);
            }
            U->duct = ev_ease(U->duct, want, 6.0f);
        }
        /* THE LAUNCHER: lowered to drive, raised to fire, at its modelled rest otherwise */
        {
            float want = 18.7f;
            if (o.driving || o.navset) want = 0.0f;
            else if (o.tar >= 0) want = 40.0f;
            U->elev = ev_ease(U->elev, want, 2.5f);
        }
        /* DOORS AND RAMPS */
        for (size_t k = 0; k < V.part.size(); k++) {
            const EvhcPart& p = V.part[k];
            if (p.kind != EVP_HINGE) continue;
            if (p.driver == EVD_APC_DOOR || p.driver == EVD_TRAN_RAMP || p.driver == EVD_LST_BOW) {
                const float step = p.driver == EVD_APC_DOOR ? 0.1f
                                 : p.driver == EVD_TRAN_RAMP ? 1.0f / 15.0f : 1.0f / 20.0f;
                U->door = ev_ease(U->door, evhc_door_want(V, o, p.driver), step);
                break;
            }
        }
        U->tick = frame;
    }
    /* units no longer in the dump: their entries go (a death has already taken what it
       needed from them, in the vanish sweep) */
    for (std::map<int, EvhcUnit>::iterator it = g_evhcUnit.begin(); it != g_evhcUnit.end(); ) {
        if (!it->second.seen) g_evhcUnit.erase(it++);
        else ++it;
    }
    /* THE SHOTS: a muzzle anim born this tick on a unit this system draws */
    for (size_t a = 0; rule && a < g_efxAnims.size(); a++) {
        const EfxAnim& an = g_efxAnims[a];
        if (an.attref < 0) continue;
        const int r = efx_recipe_index(an.type);
        if (r < 0) continue;
        const char* nm = EFX_RECIPE[r].name;
        if (!nm || (strcmp(nm, "MUZZLE_FLASH") != 0 && strncmp(nm, "GUN_", 4) != 0)) continue;
        std::map<int, EfxLiveAnim>::const_iterator la =
            g_efxAnimLive.find((an.type << 16) | (an.id & 0xFFFF));
        if (la == g_efxAnimLive.end() || la->second.bornFrame != frame) continue;
        const ObjKind k = efx_att_objkind(an.attkind);
        if (k != K_UNIT && k != K_AIRCRAFT) continue;
        evhc_note_fire((int)k * 1000000 + an.attref, frame);
    }
    evhc_wreck_step(frame);
    fflush(stdout);
}

/* A shot: the next barrel in turn kicks (a twin gun fires its barrels alternately, as the
   engine's two-shooters do, one shot and one muzzle anim each). */
static void evhc_note_fire(int key, int frame)
{
    std::map<int, EvhcUnit>::iterator it = g_evhcUnit.find(key);
    if (it == g_evhcUnit.end()) return;
    EvhcUnit& U = it->second;
    const EvhcModel& V = g_evhc[U.model];
    const int nb = V.nbarrel > 0 ? (V.nbarrel > 4 ? 4 : V.nbarrel) : 1;
    /* A TWIN GUN'S BARRELS TAKE TURNS, the left first. The engine does pick a side per
       shot (its second-shot flag, techno.cpp Fire_At), but the Mammoth's missiles flip the
       same flag in the same tick as the cannon, so the flag the tick leaves behind names no
       barrel: measured, it read the same after every cannon shot of a force-fire. The
       barrel that kicks is therefore ours, alternating per muzzle flash. */
    const int b = U.shots % nb;
    U.fire[b] = frame;
    U.shots++;
    printf("EVHC|fire|key=%d|type=%s|barrel=%d|frame=%d\n", key, U.type, b, frame);
}

/* ================================================================================
 * THE POSE. Every part's matrix, in pack units, inside the hull's (draw_mesh then turns
 * the lot with the facing and lifts it with the altitude). A child rides its parent's
 * matrix: an Orca's fan inside its tilting duct, the launcher's box on the turret.
 * ============================================================================== */
struct EvhcPose {
    int   td;                                 /* turret delta, DirType */
    float trav[2], turn, spin, disc, duct, elev, door, t;
    float clipT;                              /* the cartridge clip's time, -1 for none */
    int   cartMi;
    bool  air;
};

/* THE KICK, the lab's curve (its recoil declaration): back to the full distance over
   `back` seconds, then home on (1 - u)^2 over `ret`; a machine gun's burst repeats it
   `count` times `every` seconds apart, and the furthest-back of them is what shows. The
   offset, in the part's own units, along the bore away from the muzzle. */
static float evhc_kick_curve(float t, float d, float back, float ret)
{
    if (t < 0.0f) return 0.0f;
    if (t < back) return back > 0.0f ? d * t / back : d;
    const float u = ret > 0.0f ? (t - back) / ret : 1.0f;
    if (u >= 1.0f) return 0.0f;
    return d * (1.0f - u) * (1.0f - u);
}
static float evhc_recoil(const EvhcUnit* U, const EvhcPart& p, float now)
{
    if (!U) return 0.0f;
    const int b = (int)p.a[8];
    if (b < 0 || b > 3) return 0.0f;
    const float t = (now - (float)U->fire[b]) / 15.0f;
    const int count = p.driver > 0 ? p.driver : 1;
    float off = 0.0f;
    for (int n = 0; n < count; n++) {
        const float k = evhc_kick_curve(t - (float)n * p.a[11], p.a[6], p.a[10], p.a[7]);
        if (k > off) off = k;
    }
    return off;
}

static void evhc_part_local(const EvhcModel& V, const EvhcPart& p, const EvhcPose& P,
                            const EvhcUnit* U, float* L)
{
    ev_ident(L);
    const float* c = p.a;
    const float* ax = p.a + 3;
    switch (p.kind) {
    case EVP_TURRET: {
        const float ang = -(float)P.td * (2.0f * 3.14159265f / 256.0f);
        ev_yaw_about(c[0], c[2], ang, L);
        break;
    }
    case EVP_SPIN: {
        double turns = (double)P.t / 15.0 * (double)p.a[6] / 60.0;
        turns -= floor(turns);
        const float ph = (float)(turns * 2.0 * 3.14159265358979);
        const float ang = (p.a[7] > 0.0f) ? -p.a[7] * (3.14159265f / 180.0f) * sinf(ph) : -ph;
        ebld_pivot_matrix(c, ax, ang, L);
        break;
    }
    case EVP_ROTOR: {
        double turns = (double)P.spin * (double)p.a[6] / 60.0;
        turns -= floor(turns);
        ebld_pivot_matrix(c, ax, -(float)(turns * 2.0 * 3.14159265358979), L);
        break;
    }
    case EVP_WHEEL: {
        const int side = p.side == 0 ? 0 : 1;
        const float r = p.a[6] > 1.0f ? p.a[6] * MODEL_SCALE : 0.05f;
        const float roll = P.trav[side] / r;
        float R1[12], R2[12];
        ebld_pivot_matrix(c, ax, roll, R1);
        if (p.a[7] > 0.0f) {
            const float up[3] = { 0.0f, 1.0f, 0.0f };
            ebld_pivot_matrix(c, up, p.a[7] * (3.14159265f / 180.0f) * P.turn, R2);
            ev_mul(R2, R1, L);
        } else {
            memcpy(L, R1, sizeof R1);
        }
        break;
    }
    case EVP_HINGE: {
        float ang = 0.0f;
        switch (p.driver) {
        case EVD_APC_DOOR: case EVD_TRAN_RAMP: case EVD_LST_BOW:
            ang = p.a[6] * P.door; break;
        case EVD_ELEVATION: ang = P.elev - p.a[7]; break;
        case EVD_DUCT:      ang = P.duct; break;
        default:            ang = 0.0f; break;
        }
        if (ang != 0.0f) ebld_pivot_matrix(c, ax, ang * (3.14159265f / 180.0f), L);
        break;
    }
    case EVP_CLIP:
        if (P.clipT >= 0.0f) evhc_anim_mat(P.cartMi, (int)p.a[9], P.clipT, L, NULL);
        break;
    case EVP_RECOIL: {
        const float k = evhc_recoil(U, p, P.t);
        L[3] = -ax[0] * k; L[7] = -ax[1] * k; L[11] = -ax[2] * k;
        break;
    }
    default:
        break;
    }
    (void)V;
}

/* Every part's matrix, parents first (the file orders them so). */
static void evhc_pose_all(const EvhcModel& V, const EvhcPose& P, const EvhcUnit* U,
                          std::vector<float>& mats)
{
    mats.assign(V.part.size() * 12, 0.0f);
    for (size_t k = 0; k < V.part.size(); k++) {
        float L[12];
        evhc_part_local(V, V.part[k], P, U, L);
        const int par = V.part[k].parent;
        if (par >= 0) ev_mul(&mats[par * 12], L, &mats[k * 12]);
        else memcpy(&mats[k * 12], L, sizeof L);
    }
}

static void evhc_pose_of(const EvhcModel& V, const SimObject& o, const EvhcUnit* U,
                         EvhcPose& P)
{
    const float a = g_tickAlpha;
    memset(&P, 0, sizeof P);
    P.td = evhc_turret_delta(V, o);
    P.t = engine_time();
    P.cartMi = mesh_for(o);
    P.clipT = (P.cartMi >= 0) ? object_anim_t(P.cartMi, o) : -1.0f;
    P.air = o.kind == K_AIRCRAFT && o.alt > 0;
    if (U) {
        for (int s = 0; s < 2; s++) P.trav[s] = U->travP[s] + (U->trav[s] - U->travP[s]) * a;
        P.turn = U->turnP + (U->turn - U->turnP) * a;
        P.spin = U->spinP + (U->spin - U->spinP) * a;
        P.disc = U->disc;
        P.duct = U->ductP + (U->duct - U->ductP) * a;
        P.elev = U->elevP + (U->elev - U->elevP) * a;
        P.door = U->doorP + (U->door - U->doorP) * a;
    } else {
        P.elev = 18.7f;
        P.spin = P.t / 15.0f * 0.35f;
        P.disc = (o.kind == K_AIRCRAFT && o.alt > 0) ? 1.0f : 0.0f;
    }
}

/* ================================================================================
 * THE DRAW
 * ============================================================================== */
/* THE UV HOOK for one piece: a belt's scroll and a wheel flipbook's frame, both from the
   side's travel. The scroll is -(travel / pitch) mod 1 on the pack's v-down coordinate
   (the viewer adds +(travel / pitch) to its v-up one); the frame is floor(travel / loop *
   frames) mod frames of the grid x grid sheet, row-major from the top, as the remastered
   buildings' flipbooks are addressed. `frame` takes the frame for the dump. */
static void evhc_uv(const EvhcModel& V, const EvhcPart& p, int texIdx, const EvhcPose& P,
                    int* frame)
{
    const int side = p.side == 0 ? 0 : 1;
    const float trav = P.trav[side];
    if (p.kind == EVP_BELT) {
        const float pitch = p.a[6] * MODEL_SCALE;
        float s = pitch > 1e-5f ? trav / pitch : 0.0f;
        s -= floorf(s);
        g_meshUVScaleU = g_meshUVScaleV = 1.0f;
        g_meshUVOffU = 0.0f;
        g_meshUVOffV = -s;
        return;
    }
    if (p.kind == EVP_WFLIP && texIdx >= 0 && texIdx < (int)V.m.tex.size()) {
        const EbldTex& T = V.m.tex[texIdx];
        const float loop = p.a[6] * MODEL_SCALE;
        const int n = T.frames > 0 ? T.frames : 1;
        int f = loop > 1e-5f ? (int)floorf(trav / loop * (float)n) : 0;
        f %= n;
        if (f < 0) f += n;
        if (frame) *frame = f;
        const int grid = T.grid > 0 ? T.grid : 1;
        g_meshUVScaleU = g_meshUVScaleV = 1.0f / (float)grid;
        g_meshUVOffU = (float)(f % grid) / (float)grid;
        g_meshUVOffV = (float)(f / grid) / (float)grid;
    }
}

/* One piece of a model at a matrix, through draw_mesh with every argument the cartridge
   model would have been given. */
struct EvhcDraw {
    int mi, face, pass, house, wob;
    float x, z, lift;
    void run(int a, int b, const float* Mx) const
    {
        if (b <= a) return;
        draw_mesh(mi, x, z, face, pass, 0, 0.0f, house, 1.0f, lift, false, wob, -1.0f,
                  Mx, (size_t)a, (size_t)b);
    }
};

/* Whether a piece shows now: an aircraft's blades on the ground and its blur disc in the
   air (the disc's opacity is set by the caller). */
static bool evhc_piece_shows(const EvhcModel& V, int pi, const EvhcPose& P)
{
    if (pi < 0) return true;
    const EvhcPart& p = V.part[pi];
    if (p.kind != EVP_ROTOR) return true;
    if (p.flags & EVF_DISC) return P.disc > 0.01f;
    return !(V.discs && P.disc >= 0.5f);
}

/* THE STANDING VEHICLE, asked by draw_object_mesh after the authored model and before the
   cartridge's. True: this system has answered for the object in this pass. The riders on
   a hovercraft's deck are asked the same (their lift is the deck's). */
static bool evhc_draw_object(const SimObject& o, int pass)
{
    const int m = evhc_for(o);
    if (m < 0) return false;
    EvhcModel& V = g_evhc[m];
    /* no ground-shadow triangles: a vehicle's plate is the cartridge's own quad, drawn by
       type elsewhere, whatever model stands over it */
    if (pass == MODE_SHADOW) return true;
    if (!evhc_resident(V, false)) return false;
    const EbldModel& M = V.m;
    const EbldSet& S = M.set[EBS_INTACT];
    const std::vector<int>& rg = M.rng[EBM_INTACT];
    std::map<int, EvhcUnit>::iterator ui = g_evhcUnit.find(smooth_key(o));
    EvhcUnit* U = (ui != g_evhcUnit.end() && ui->second.model == m) ? &ui->second : NULL;
    if (o.ylift > 0.0f) U = NULL;               /* a rider's copy: posed at rest */
    EvhcPose P;
    evhc_pose_of(V, o, U, P);
    std::vector<float> mats;
    evhc_pose_all(V, P, U, mats);
    EvhcDraw D;
    D.mi = M.mesh[EBM_INTACT]; D.face = evhc_face(o); D.pass = pass; D.house = mesh_house(o);
    D.wob = wobble_of(o); D.x = o.wx; D.z = o.wz; D.lift = alt_lift(o) + o.ylift;
    const bool colour = (pass == MODE_OPAQUE || pass == MODE_CUTOUT || pass == MODE_XLU)
                        && !ebld_aux_pass();
    if (pass == MODE_OPAQUE && colour) { V.drawn++; if (U) U->drawnNow = true; }
    g_meshFlash = g_flashOn && o.blush != 0;
    int dframe[2] = { -1, -1 };
    for (size_t k = 0; k < S.piece.size(); k++) {
        const int* r = &rg[k * EBR];
        const int r0 = r[EBR_A], r1 = r[EBR_B];
        if (r1 <= r0) continue;
        const int pi = V.partOf[k];
        if (!evhc_piece_shows(V, pi, P)) continue;
        const float* Mx = NULL;
        if (pi >= 0 && !ev_is_ident(&mats[pi * 12])) Mx = &mats[pi * 12];
        bool hook = false;
        unsigned char alpha = 255;
        if (pi >= 0) {
            const EvhcPart& p = V.part[pi];
            if (p.kind == EVP_BELT || p.kind == EVP_WFLIP) {
                int f = -1;
                evhc_uv(V, p, r[EBR_TEX], P, &f);
                if (f >= 0) dframe[p.side == 0 ? 0 : 1] = f;
                hook = true;
            }
            if (p.kind == EVP_ROTOR && (p.flags & EVF_DISC)) {
                float d = P.disc;
                if (d > 1.0f) d = 1.0f;
                alpha = (unsigned char)(255.0f * d + 0.5f);
                if (!colour && alpha < 128) continue;
            }
        }
        g_meshAlphaScale = alpha;
        D.run(r0, r1, Mx);
        g_meshAlphaScale = 255;
        if (hook) ebld_uv_clear();
    }
    g_meshFlash = false;
    if (U && pass == MODE_OPAQUE && colour) {
        U->dTur = P.td;
        U->dTrav[0] = P.trav[0]; U->dTrav[1] = P.trav[1];
        U->dFrame[0] = dframe[0]; U->dFrame[1] = dframe[1];
        U->dTurn = P.turn;
        U->dRotor = P.spin;
        U->dDisc = P.disc;
        U->dDuct = P.duct; U->dElev = P.elev; U->dDoor = P.door;
        U->dClip = P.clipT;
        float kick = 0.0f;
        for (size_t k = 0; k < V.part.size(); k++) {
            if (V.part[k].kind != EVP_RECOIL || V.part[k].a[6] <= 0.0f) continue;
            const float f = evhc_recoil(U, V.part[k], P.t) / V.part[k].a[6];
            if (f > kick) kick = f;
        }
        U->dKick = kick;
    }
    return true;
}

/* ================================================================================
 * THE RIGS. The refinery's dock and the MCV's deploy are the cartridge's own clips on a
 * model of their own, drawn at the building's anchor; each carries the cartridge vehicle
 * inside it. Under the rule the remastered vehicle is drawn there instead, each piece
 * moved by the rig part that moves the same thing, and only the rest of the rig (the
 * Construction Yard rising round the MCV) is drawn from the cartridge.
 * ============================================================================== */
static int procrig_for(const SimObject& o, float* animT);
static int mcvrig_for(const SimObject& o, float* animT);
static int draw_facing(const SimObject& o);

/* The docked harvester, drawn for the refinery `o` while its dock rig plays. True: drawn
   here, and the rig's own harvester must not be. */
static bool evhc_dock_draw(const SimObject& o, int pass)
{
    if (o.kind != K_BUILDING || strcmp(o.type, "PROC") != 0 || !evhc_rule_now()) return false;
    const int m = evhc_model_of("HARV");
    if (m < 0) return false;
    EvhcModel& V = g_evhc[m];
    if (!V.rigOk) return false;
    float rigT = -1.0f;
    const int rig = procrig_for(o, &rigT);
    if (rig < 0) return false;
    if (pass == MODE_SHADOW) return true;
    if (!evhc_resident(V, false)) return false;
    const EbldSet& S = V.m.set[EBS_INTACT];
    const std::vector<int>& rg = V.m.rng[EBM_INTACT];
    float Dk[12];
    evhc_dock_matrix(V, Dk);
    EvhcPose P;
    memset(&P, 0, sizeof P);
    P.t = engine_time();
    P.clipT = -1.0f;
    P.cartMi = -1;
    std::vector<float> mats;
    evhc_pose_all(V, P, NULL, mats);
    if (pass == MODE_OPAQUE && !ebld_aux_pass()) V.rigDrawn++;
    for (size_t k = 0; k < S.piece.size(); k++) {
        const int* r = &rg[k * EBR];
        if (r[EBR_B] <= r[EBR_A]) continue;
        const int pi = V.partOf[k];
        const int rp = (k < V.rigPart.size() && V.rigPart[k] >= 0) ? V.rigPart[k] : 0;
        float A[12], T1[12], T2[12];
        bool vis = true;
        evhc_anim_mat(rig, rp, rigT, A, &vis);
        if (!vis) continue;
        ev_mul(A, Dk, T1);
        if (pi >= 0 && V.part[pi].kind != EVP_CLIP && V.part[pi].kind != EVP_HINGE)
            ev_mul(T1, &mats[pi * 12], T2);
        else
            memcpy(T2, T1, sizeof T1);
        draw_mesh(V.m.mesh[EBM_INTACT], o.wx, o.wz, -1, pass, 0, 0.0f, mesh_house(o), 1.0f,
                  0.0f, false, WOBBLE_NONE, -1.0f, T2, (size_t)r[EBR_A], (size_t)r[EBR_B]);
    }
    return true;
}

/* The MCV unfolding, drawn for the Construction Yard `o` while its deploy rig plays. True:
   the MCV was drawn here, and the rig's own MCV parts (1 to 5) must not be. */
static bool evhc_deploy_draw(const SimObject& o, int pass, int rig, float rigT)
{
    if (o.kind != K_BUILDING || strcmp(o.type, "FACT") != 0 || !evhc_rule_now()) return false;
    const int m = evhc_model_of("MCV");
    if (m < 0 || rig < 0) return false;
    EvhcModel& V = g_evhc[m];
    if (!V.rigOk) return false;
    if (pass == MODE_SHADOW) return true;
    if (!evhc_resident(V, false)) return false;
    const EbldSet& S = V.m.set[EBS_INTACT];
    const std::vector<int>& rg = V.m.rng[EBM_INTACT];
    if (pass == MODE_OPAQUE && !ebld_aux_pass()) V.rigDrawn++;
    float F[12];
    ev_yaw_about(0.0f, 0.0f, V.rigYaw, F);
    F[3] += V.rigX;
    F[11] += V.rigZ;
    for (size_t k = 0; k < S.piece.size(); k++) {
        const int* r = &rg[k * EBR];
        if (r[EBR_B] <= r[EBR_A]) continue;
        const int rp = (k < V.rigPart.size()) ? V.rigPart[k] : -1;
        if (rp < 0) continue;
        float A0[12], A0i[12], A[12], T1[12], T2[12];
        bool vis = true;
        evhc_anim_mat(rig, rp, rigT, A, &vis);
        if (!vis) continue;
        /* the rig's matrices are deltas from ITS first frame, which is where the fit put
           the remaster: frame 0's own delta is taken back out */
        evhc_anim_mat(rig, rp, 0.0f, A0, NULL);
        {
            /* the inverse of a rigid 3x4 (a scale in it is kept: it is the rig's own) */
            float R[9] = { A0[0], A0[1], A0[2], A0[4], A0[5], A0[6], A0[8], A0[9], A0[10] };
            const float det = R[0] * (R[4] * R[8] - R[5] * R[7]) - R[1] * (R[3] * R[8] - R[5] * R[6])
                            + R[2] * (R[3] * R[7] - R[4] * R[6]);
            if (fabsf(det) < 1e-8f) { ev_ident(A0i); }
            else {
                const float id = 1.0f / det;
                float I[9];
                I[0] = (R[4] * R[8] - R[5] * R[7]) * id; I[1] = (R[2] * R[7] - R[1] * R[8]) * id;
                I[2] = (R[1] * R[5] - R[2] * R[4]) * id; I[3] = (R[5] * R[6] - R[3] * R[8]) * id;
                I[4] = (R[0] * R[8] - R[2] * R[6]) * id; I[5] = (R[2] * R[3] - R[0] * R[5]) * id;
                I[6] = (R[3] * R[7] - R[4] * R[6]) * id; I[7] = (R[1] * R[6] - R[0] * R[7]) * id;
                I[8] = (R[0] * R[4] - R[1] * R[3]) * id;
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) A0i[i * 4 + j] = I[i * 3 + j];
                    A0i[i * 4 + 3] = -(I[i * 3] * A0[3] + I[i * 3 + 1] * A0[7] + I[i * 3 + 2] * A0[11]);
                }
            }
        }
        ev_mul(A, A0i, T1);
        ev_mul(T1, F, T2);
        draw_mesh(V.m.mesh[EBM_INTACT], o.wx, o.wz, draw_facing(o), pass, 0, 0.0f,
                  mesh_house(o), 1.0f, 0.0f, false, WOBBLE_NONE, -1.0f, T2, (size_t)r[EBR_A],
                  (size_t)r[EBR_B]);
    }
    return true;
}

/* ================================================================================
 * THE DEATH. The vanish sweep in refresh_objects decides a unit died (a newborn explosion
 * on the cell it left); under the rule, for a type with a model, this system takes the
 * death instead of the old unit shatter: the wreck's pieces start where the intact pieces
 * stood (turned as they were, a turret at its aim), with the unit's own motion.
 *
 * A VEHICLE is thrown apart at once: outward from its centre and up, spinning.
 * AN AIRCRAFT falls whole from where it flew, keeping its forward speed and turning (a
 * helicopter spins down), under the engine's own debris gravity, until any piece touches
 * the ground; there it breaks, the pieces thrown from the impact with the craft's speed,
 * with fire and smoke where it came down and a jolt.
 * Then, as the remastered buildings' wreck: they settle on the terrain, lie a few seconds,
 * sink as they fade, and are gone.
 * ============================================================================== */
struct EvhcDeathIn {
    char  type[16];
    int   kind, id, house, face;
    float ex, ez, vx, vz, lift;
};

static bool evhc_claims_death(const char* type, int kind)
{
    if (kind != K_UNIT && kind != K_AIRCRAFT) return false;
    if (!evhc_rule_now()) return false;
    const int m = evhc_model_of(type);
    return m >= 0 && g_evhc[m].m.nset == 2;
}

/* The world rotation (quaternion w, x, y, z) of a yaw by draw_mesh's facing. */
static void ev_q_yaw(float ang, float* q)
{
    q[0] = cosf(ang * 0.5f); q[1] = 0.0f; q[2] = sinf(ang * 0.5f); q[3] = 0.0f;
}
static void ev_q_mul(const float* a, const float* b, float* o)
{
    const float r[4] = {
        a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
        a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
        a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
        a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0] };
    memcpy(o, r, sizeof r);
}
/* A rotation matrix (the 3x3 of a 3x4) as a quaternion. */
static void ev_q_of(const float* M, float* q)
{
    const float m00 = M[0], m01 = M[1], m02 = M[2], m10 = M[4], m11 = M[5], m12 = M[6];
    const float m20 = M[8], m21 = M[9], m22 = M[10];
    const float tr = m00 + m11 + m22;
    if (tr > 0.0f) {
        const float s = sqrtf(tr + 1.0f) * 2.0f;
        q[0] = 0.25f * s; q[1] = (m21 - m12) / s; q[2] = (m02 - m20) / s; q[3] = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
        q[0] = (m21 - m12) / s; q[1] = 0.25f * s; q[2] = (m01 + m10) / s; q[3] = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
        q[0] = (m02 - m20) / s; q[1] = (m01 + m10) / s; q[2] = 0.25f * s; q[3] = (m12 + m21) / s;
    } else {
        const float s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
        q[0] = (m10 - m01) / s; q[1] = (m02 + m20) / s; q[2] = (m12 + m21) / s; q[3] = 0.25f * s;
    }
    eb_qnorm(q);
}

/* A wreck piece's launch: up and out as the lab's blast for its role gives them, and a
   spin, from two hashes. */
static void evhc_blast(const EvhcModel& V, size_t i, unsigned h1, unsigned h2, float* up,
                       float* out, float* spin)
{
    float b[5] = { 0.9f, 1.5f, 0.3f, 0.7f, 3.0f };
    if (V.blast.size() >= (i + 1) * 5)
        for (int c = 0; c < 5; c++) b[c] = V.blast[i * 5 + c];
    *up = b[0] + (b[1] - b[0]) * eb_r(h1, 1);
    *out = b[2] + (b[3] - b[2]) * eb_r(h1, 0);
    *spin = b[4];
    (void)h2;
}

static void evhc_note_death(const EvhcDeathIn& d, int key, int frame)
{
    const int m = evhc_model_of(d.type);
    if (m < 0) return;
    const EvhcModel& V = g_evhc[m];
    const EbldSet& W = V.m.set[EBS_DESTROYED];
    if (W.piece.empty()) return;
    /* the pose it died in: last tick's motion state, if it had one */
    std::map<int, EvhcUnit>::iterator ui = g_evhcUnit.find(key);
    const EvhcUnit* U = (ui != g_evhcUnit.end() && ui->second.model == m) ? &ui->second : NULL;
    SimObject o;
    memset(&o, 0, sizeof o);
    o.kind = (ObjKind)d.kind;
    snprintf(o.type, sizeof o.type, "%s", d.type);
    o.face = U ? U->face : -1;
    o.tface = -1;
    o.alt = d.lift > 0.01f ? 24 : 0;
    EvhcPose P;
    evhc_pose_of(V, o, U, P);
    P.td = U ? U->td : 0;
    std::vector<float> mats;
    evhc_pose_all(V, P, U, mats);
    const float ground = terrain_y(d.ex, d.ez);
    /* the hull's facing as this system drew it (the sweep's is the cartridge draw's, which
       turns a turretless rocket launcher's whole body to its launcher) */
    int face = d.face;
    if (U && d.kind == K_UNIT && U->face >= 0)
        face = (U->face + (g_nofacefix ? (((int)g_faceBias) & 255) : model_face_bias(d.type))) & 255;
    const float fa = face >= 0 ? -(float)face * (2.0f * 3.14159265f / 256.0f) : 0.0f;
    float qf[4];
    ev_q_yaw(fa, qf);
    const float cf = cosf(fa), sf = sinf(fa);
    const unsigned hk = (unsigned)key;
    EvhcWreck E;
    E.model = m; E.id = d.id; E.kind = d.kind; E.house = d.house;
    snprintf(E.type, sizeof E.type, "%s", d.type);
    E.deathTick = frame; E.impactTick = -1;
    E.air = d.lift > 0.05f;
    E.falling = E.air;
    E.sinking = E.dropped = false;
    E.wx = d.ex; E.wz = d.ez; E.ground = ground;
    E.wet = !E.air && evhc_cell_wet(E.wx, E.wz);
    E.body.resize(W.piece.size());
    E.off.assign(W.piece.size() * 3, 0.0f);
    E.qoff.assign(W.piece.size() * 4, 0.0f);
    memset(&E.carrier, 0, sizeof E.carrier);
    const float base[3] = { d.ex, ground + d.lift, d.ez };
    for (size_t i = 0; i < W.piece.size(); i++) {
        const EbldPiece& Pc = W.piece[i];
        const int par = Pc.parent;
        const int pi = (par >= 0 && par < (int)V.partOf.size()) ? V.partOf[par] : -1;
        float Mx[12];
        if (pi >= 0) memcpy(Mx, &mats[pi * 12], sizeof Mx); else ev_ident(Mx);
        float lc[3];
        ev_apply(Mx, Pc.c, lc);                  /* its centre, posed, pack units */
        /* into the world: the facing, the model scale, the anchor */
        float p[3];
        p[0] = base[0] + (lc[0] * cf + lc[2] * sf) * MODEL_SCALE;
        p[1] = base[1] + lc[1] * MODEL_SCALE;
        p[2] = base[2] + (-lc[0] * sf + lc[2] * cf) * MODEL_SCALE;
        float qp[4], q[4];
        ev_q_of(Mx, qp);
        ev_q_mul(qf, qp, q);
        EbldBody& B = E.body[i];
        ebld_body_at(B, p, q);
        const unsigned h1 = efx_hash(0x45564844u, hk, (unsigned)frame, (unsigned)i);
        const unsigned h2 = efx_hash(h1, 0x2545F491u, (unsigned)i, hk);
        const float ivx = d.vx * 15.0f, ivz = d.vz * 15.0f;
        if (E.air) {
            /* riding the falling craft: its offset and turn inside the carrier */
            for (int c = 0; c < 3; c++) E.off[i * 3 + c] = p[c] - base[c];
            for (int c = 0; c < 4; c++) E.qoff[i * 4 + c] = q[c];
            B.v[0] = ivx; B.v[2] = ivz;
            continue;
        }
        float dx = p[0] - d.ex, dz = p[2] - d.ez;
        float dl = sqrtf(dx * dx + dz * dz);
        if (dl < 0.03f) {
            dx = eb_r(h2, 3) - 0.5f; dz = eb_r(h1, 3) - 0.5f;
            dl = sqrtf(dx * dx + dz * dz);
            if (dl < 1e-4f) { dx = 1.0f; dz = 0.0f; dl = 1.0f; }
        }
        dx /= dl; dz /= dl;
        /* THE BLAST, by the piece's role as the lab gave it: a turret leaps highest (the
           shot everyone expects of a tank killed), shards fly furthest and spin fastest */
        float up, out, spin;
        evhc_blast(V, i, h1, h2, &up, &out, &spin);
        B.v[0] = ivx + dx * out; B.v[1] = up; B.v[2] = ivz + dz * out;
        for (int c = 0; c < 3; c++) B.w[c] = (eb_r(h2, c) - 0.5f) * 2.0f * spin;
    }
    if (E.air) {
        /* THE CARRIER: the craft's origin in the air, its forward speed, a little sink, and
           a turn: a helicopter spins down about its mast, a plane rolls and noses over */
        const float q0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
        ebld_body_at(E.carrier, base, q0);
        const unsigned h = efx_hash(0x45564341u, hk, (unsigned)frame, 7u);
        E.carrier.v[0] = d.vx * 15.0f;
        E.carrier.v[2] = d.vz * 15.0f;
        E.carrier.v[1] = -0.2f;
        const bool heli = V.cls == EVC_HELI;
        const float spinDir = (h & 1) ? 1.0f : -1.0f;
        E.carrier.w[1] = heli ? spinDir * (3.0f + 2.0f * eb_r(h, 1)) : 0.4f * spinDir;
        /* roll about the direction of flight, pitch the nose down */
        E.carrier.w[0] = (eb_r(h, 2) - 0.5f) * (heli ? 1.2f : 2.0f);
        E.carrier.w[2] = (eb_r(h, 3) - 0.5f) * (heli ? 1.2f : 2.0f);
    }
    const long long wk = ++g_evhcWreckSerial;
    g_evhcWreck[wk] = E;
    printf("EVHC|death|key=%d|type=%s|air=%d|pieces=%d|lift=%.3f|frame=%d\n", key, d.type,
           E.air ? 1 : 0, (int)W.piece.size(), d.lift, frame);
}

/* Whether a death of this type is this system's, asked by the sweep before it looks for
   the explosion (an aircraft's is placed off its ground point). */
static bool evhc_claims_type(const char* type)
{
    return evhc_claims_death(type, K_AIRCRAFT) || evhc_claims_death(type, K_UNIT);
}

/* THE SWEEP'S ENTRY, declared far above where the death is observed: true when this
   system takes the death (and the old unit shatter must not). */
static bool evhc_take_death(const char* type, int kind, int id, int house, int face,
                            float ex, float ez, float vx, float vz, float lift, int key,
                            int frame)
{
    if (!evhc_claims_death(type, kind)) return false;
    EvhcDeathIn d;
    snprintf(d.type, sizeof d.type, "%s", type);
    d.kind = kind; d.id = id; d.house = house; d.face = face;
    d.ex = ex; d.ez = ez; d.vx = vx; d.vz = vz; d.lift = lift;
    evhc_note_death(d, key, frame);
    return true;
}

/* The debris gravity the engine's own chunks fall with (EFX_CHUNK_YACC over EFX_U, per
   tick squared), in cells a second squared, so a falling craft keeps pace with the
   cartridge's own chunks thrown out beside it. */
static const float EVHC_FALL_G = 2.0f / 256.0f * 225.0f;

/* The impact: the craft breaks where it came down. Fire and smoke there, and a jolt. */
static void evhc_impact(EvhcWreck& E, const EvhcModel& V, int frame, long long key)
{
    E.falling = false;
    E.impactTick = frame;
    if (evhc_finite(E.carrier.p[0]) && evhc_finite(E.carrier.p[2])) {
        E.wx = E.carrier.p[0]; E.wz = E.carrier.p[2];
    }
    E.wet = evhc_cell_wet(E.wx, E.wz);
    float R[9];
    eb_qmat(E.carrier.q, R);
    const EbldSet& W = V.m.set[EBS_DESTROYED];
    const unsigned hk = (unsigned)key;
    for (size_t i = 0; i < E.body.size() && i < W.piece.size(); i++) {
        EbldBody& B = E.body[i];
        float ro[3];
        eb_mulv(R, &E.off[i * 3], ro);
        float p[3], q[4];
        for (int c = 0; c < 3; c++) p[c] = E.carrier.p[c] + ro[c];
        ev_q_mul(E.carrier.q, &E.qoff[i * 4], q);
        eb_qnorm(q);
        ebld_body_at(B, p, q);
        /* the carrier's velocity at the piece (v + w x r), then thrown out of the impact */
        float wr[3];
        eb_cross(E.carrier.w, ro, wr);
        const unsigned h1 = efx_hash(0x45564958u, hk, (unsigned)frame, (unsigned)i);
        const unsigned h2 = efx_hash(h1, 0x7F4A7C15u, (unsigned)i, hk);
        float dx = ro[0], dz = ro[2];
        float dl = sqrtf(dx * dx + dz * dz);
        if (dl < 0.03f) { dx = eb_r(h2, 3) - 0.5f; dz = eb_r(h1, 3) - 0.5f; dl = sqrtf(dx * dx + dz * dz) + 1e-4f; }
        dx /= dl; dz /= dl;
        float up, out, spin;
        evhc_blast(V, i, h1, h2, &up, &out, &spin);
        B.v[0] = E.carrier.v[0] * 0.6f + wr[0] + dx * out;
        B.v[1] = up * 0.8f;
        B.v[2] = E.carrier.v[2] * 0.6f + wr[2] + dz * out;
        for (int c = 0; c < 3; c++) B.w[c] = (eb_r(h2, c) - 0.5f) * 2.0f * spin;
    }
    /* fire and black smoke where it hit, into the effects pool */
    if (g_efxPoolLive) {
        for (int n = 0; n < 18; n++) {
            const unsigned h = efx_hash(0x45564650u, hk, (unsigned)frame, (unsigned)n);
            EfxPart p;
            memset(&p, 0, sizeof p);
            const float a = 6.2831853f * eb_r(h, 0), rr = 0.35f * sqrtf(eb_r(h, 1));
            p.x = E.carrier.p[0] + rr * cosf(a);
            p.z = E.carrier.p[2] + rr * sinf(a);
            p.y = efx_ground(p.x, p.z) + 0.05f;
            p.sx = p.x; p.sy = p.y; p.sz = p.z;
            const bool fire = n < 10;
            p.vx = (eb_r(h, 2) - 0.5f) * 0.03f;
            p.vz = (eb_r(h, 3) - 0.5f) * 0.03f;
            p.vy = fire ? 0.02f + 0.02f * eb_r(h, 1) : 0.015f + 0.01f * eb_r(h, 2);
            p.ay = fire ? 0.0006f : 0.0004f;
            p.hw = fire ? 0.18f + 0.12f * eb_r(h, 3) : 0.22f;
            const int life = fire ? 14 + (int)(10 * eb_r(h, 0)) : 40 + (int)(30 * eb_r(h, 1));
            p.grow = (fire ? 0.004f : 0.012f);
            p.life = (short)life;
            p.sys = fire ? 5 : 6;
            p.r = p.g = p.b = 255;
            p.a = fire ? 255 : 200;
            int ar = (int)((float)p.a / (0.4f * (float)life) + 0.5f);
            p.arate = (unsigned char)(ar < 1 ? 1 : ar > 255 ? 255 : ar);
            p.seed = h;
            efx_push(p);
        }
    }
    shake_tap(SHAKE_VEHICLE * 2.0f);
    printf("EVHC|impact|key=%lld|type=%s|frame=%d|at=%.3f,%.3f\n", key, E.type, frame,
           E.carrier.p[0], E.carrier.p[2]);
}


/* The wrecks' physics and timeline, on the tick edge. */
static void evhc_wreck_step(int frame)
{
    for (std::map<long long, EvhcWreck>::iterator it = g_evhcWreck.begin(); it != g_evhcWreck.end(); ) {
        EvhcWreck& E = it->second;
        const long long key = it->first;
        if (E.model < 0 || E.model >= (int)g_evhc.size()) { g_evhcWreck.erase(it++); continue; }
        const EvhcModel& V = g_evhc[E.model];
        const EbldSet& W = V.m.set[EBS_DESTROYED];
        const float sinkAfter = V.m.param[EVQ_SINK_AFTER], sinkTime = V.m.param[EVQ_SINK_TIME];
        if (E.falling) {
            /* the carrier: integrated as the engine's chunks are (position, then speed) */
            EbldBody& C = E.carrier;
            for (int c = 0; c < 3; c++) C.pp[c] = C.p[c];
            for (int c = 0; c < 4; c++) C.pq[c] = C.q[c];
            const float h = 1.0f / 15.0f;
            for (int c = 0; c < 3; c++) C.p[c] += C.v[c] * h;
            C.v[1] -= EVHC_FALL_G * h;
            eb_qspin(C.q, C.w, h);
            /* the pieces ride it (their last pose kept for the draw's lerp) */
            float R[9];
            eb_qmat(C.q, R);
            bool hit = false;
            for (size_t i = 0; i < E.body.size() && i < W.piece.size(); i++) {
                EbldBody& B = E.body[i];
                for (int c = 0; c < 3; c++) B.pp[c] = B.p[c];
                for (int c = 0; c < 4; c++) B.pq[c] = B.q[c];
                float ro[3];
                eb_mulv(R, &E.off[i * 3], ro);
                for (int c = 0; c < 3; c++) B.p[c] = C.p[c] + ro[c];
                ev_q_mul(C.q, &E.qoff[i * 4], B.q);
                eb_qnorm(B.q);
                /* touching: any contact point of any piece at or under the ground */
                const EbldPiece& P = W.piece[i];
                float Rb[9];
                eb_qmat(B.q, Rb);
                for (int k = 0; k < P.nhull && !hit; k++) {
                    float hp[3] = { P.hull[k][0] * MODEL_SCALE, P.hull[k][1] * MODEL_SCALE,
                                    P.hull[k][2] * MODEL_SCALE }, r[3];
                    eb_mulv(Rb, hp, r);
                    if (B.p[1] + r[1] <= efx_ground(B.p[0] + r[0], B.p[2] + r[2])) hit = true;
                }
            }
            /* and a fall that never lands (off the map, into water under the terrain) is
               ended by the clock */
            if (hit || frame - E.deathTick > 120) evhc_impact(E, V, frame, key);
            ++it;
            continue;
        }
        const int t0 = E.impactTick >= 0 ? E.impactTick : E.deathTick;
        const int t = frame - t0;
        int sinkAt = (int)(sinkAfter * 15.0f + 0.5f);
        int sinkEnd = sinkAt + (int)(sinkTime * 15.0f + 0.5f);
        /* A WRECK ON WATER GOES DOWN WHERE IT CAME TO REST: the pieces would otherwise
           lie on the seabed the ground height describes. They are thrown, then sink
           within the second as they fade. */
        if (evhc_on_water(E)) { sinkAt = 8; sinkEnd = sinkAt + 30; }
        if (!E.sinking) {
            EbldGround G;
            memset(&G, 0, sizeof G);
            for (size_t i = 0; i < E.body.size() && i < W.piece.size(); i++)
                ebld_body_tick(E.body[i], W.piece[i], NULL, G);
            if (t >= sinkAt) {
                E.sinking = true;
                for (size_t i = 0; i < E.body.size(); i++) {
                    EbldBody& B = E.body[i];
                    for (int c = 0; c < 3; c++) B.pp[c] = B.p[c];
                    for (int c = 0; c < 4; c++) B.pq[c] = B.q[c];
                    B.y0 = B.p[1];
                }
                printf("EVHC|sink|key=%lld|type=%s|frame=%d|wet=%d\n", key, E.type, frame, E.wet ? 1 : 0);
            }
        }
        if (E.sinking && t >= sinkEnd) {
            printf("EVHC|gone|key=%lld|type=%s|frame=%d\n", key, E.type, frame);
            g_evhcWreck.erase(it++);
            continue;
        }
        ++it;
    }
}

/* A wreck's piece pose, lerped between the last two ticks (not while sinking: the sink is
   the draw's own clock), as the 3x4 draw_mesh takes and the world point it stands at. */
static void evhc_body_pose(const EbldBody& B, const EbldPiece& P, bool sinking, float sink,
                           float* Mx, float* p)
{
    const float a = sinking ? 0.0f : g_tickAlpha;
    float q[4];
    for (int c = 0; c < 3; c++) p[c] = B.pp[c] + (B.p[c] - B.pp[c]) * a;
    const float dd = B.pq[0] * B.q[0] + B.pq[1] * B.q[1] + B.pq[2] * B.q[2] + B.pq[3] * B.q[3];
    const float sg = dd < 0.0f ? -1.0f : 1.0f;
    for (int c = 0; c < 4; c++) q[c] = B.pq[c] + (sg * B.q[c] - B.pq[c]) * a;
    eb_qnorm(q);
    if (sinking) p[1] = B.y0 - sink;
    float R[9];
    eb_qmat(q, R);
    for (int i = 0; i < 3; i++) {
        for (int c = 0; c < 3; c++) Mx[i * 4 + c] = R[i * 3 + c];
        Mx[i * 4 + 3] = -(R[i * 3] * P.c[0] + R[i * 3 + 1] * P.c[1] + R[i * 3 + 2] * P.c[2]);
    }
}
/* How far a wreck has sunk: e 0..1 over the sink time, the depth on a smoothstep. */
static void evhc_sink(const EvhcWreck& E, const EvhcModel& V, float now, float* e, float* sink)
{
    *e = 0.0f;
    *sink = 0.0f;
    if (!E.sinking) return;
    const int t0 = E.impactTick >= 0 ? E.impactTick : E.deathTick;
    const float after = evhc_on_water(E) ? 8.0f / 15.0f : V.m.param[EVQ_SINK_AFTER];
    const float time = evhc_on_water(E) ? 2.0f : V.m.param[EVQ_SINK_TIME];
    float k = ((now - (float)t0) / 15.0f - after) / (time > 0.01f ? time : 0.01f);
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    *e = k;
    *sink = (evhc_on_water(E) ? 0.6f : V.m.param[EVQ_SINK_DEPTH]) * k * k * (3.0f - 2.0f * k);
}
static bool evhc_wreck_shown(const EvhcWreck& E)
{
    if (!g_vShowUnits) return false;
    return g_objShroud ? shroud_vis_at(E.wx, E.wz) > 0.0f
                       : !shroud_cell_hidden((int)E.wx, (int)E.wz);
}

/* The wrecks, drawn from the same sites as the remastered buildings' pieces, in the same
   state: one-sided, the broken faces (caps and linings) pushed back in depth so a surface
   they touch always wins (ebld_gl_begin). */
static void evhc_draw_wrecks(int pass, bool fading)
{
    if (g_evhcWreck.empty() || pass == MODE_SHADOW) return;
    const bool aux = ebld_aux_pass();
    if (aux && fading) return;
    const float now = engine_time();
    bool began = false;
    EbldGL st;
    memset(&st, 0, sizeof st);
    for (std::map<long long, EvhcWreck>::const_iterator it = g_evhcWreck.begin();
         it != g_evhcWreck.end(); ++it) {
        const EvhcWreck& E = it->second;
        if (E.model < 0 || E.model >= (int)g_evhc.size()) continue;
        EvhcModel& V = g_evhc[E.model];
        if (!evhc_meshes_ok(V) || V.m.nset != 2 || !evhc_wreck_shown(E)) continue;
        if (!evhc_resident(V, true)) continue;
        float e = 0.0f, sink = 0.0f;
        evhc_sink(E, V, now, &e, &sink);
        int as = (int)(255.0f * (1.0f - e) + 0.5f);
        if (as <= 0) continue;
        if (aux) { if (as < 128) continue; as = 255; }
        else if ((as < 255) != fading) continue;
        if (!began) { st = ebld_gl_begin(aux); began = true; }
        const int mi = V.m.mesh[EBM_WRECK];
        const EbldSet& W = V.m.set[EBS_DESTROYED];
        const std::vector<int>& rg = V.m.rng[EBM_WRECK];
        g_meshAlphaScale = (unsigned char)as;
        for (size_t k = 0; k < W.piece.size() && k < E.body.size(); k++) {
            const int* r = &rg[k * EBR];
            if (r[EBR_B] <= r[EBR_A] && r[EBR_RB] <= r[EBR_RA]) continue;
            const EbldPiece& P = W.piece[k];
            float Mx[12], p[3];
            evhc_body_pose(E.body[k], P, E.sinking, sink, Mx, p);
            const float lift = p[1] - terrain_y(p[0], p[2]);
            const bool two = (P.flags & EBF_DOUBLE) != 0;
            if (two) ebld_gl_sides(st, true);
            for (int blk = 0; blk < 2; blk++) {
                const int r0 = blk ? r[EBR_RA] : r[EBR_A], r1 = blk ? r[EBR_RB] : r[EBR_B];
                if (r1 <= r0) continue;
                ebld_gl_broken(st, blk == 1);
                draw_mesh(mi, p[0], p[2], -1, pass, 0, 0.0f, E.house, 1.0f, lift, false,
                          WOBBLE_NONE, -1.0f, Mx, (size_t)r0, (size_t)r1);
            }
            ebld_gl_broken(st, false);
            if (two) ebld_gl_sides(st, false);
        }
        g_meshAlphaScale = 255;
    }
    if (began) ebld_gl_end(st);
}

/* THE WRECKS' EMBER GLOW: the destroyed model's emit maps, added over the pieces in the
   colour picture only, flickering on two sines of the engine clock phased per wreck and
   fading with the wreck, as the remastered buildings' glow does. */
static void evhc_draw_glow(void)
{
    if (g_evhcWreck.empty() || ebld_aux_pass()) return;
    const float now = engine_time();
    GLint depthFunc = GL_LEQUAL;
    GLboolean depthMask = GL_FALSE;
    bool began = false;
    EbldGL st;
    memset(&st, 0, sizeof st);
    for (std::map<long long, EvhcWreck>::const_iterator it = g_evhcWreck.begin();
         it != g_evhcWreck.end(); ++it) {
        const EvhcWreck& E = it->second;
        if (E.model < 0 || E.model >= (int)g_evhc.size()) continue;
        EvhcModel& V = g_evhc[E.model];
        if (!evhc_meshes_ok(V) || V.m.nset != 2) continue;
        const int mi = V.m.mesh[EBM_GLOWX];
        if (mi < 0 || mi >= (int)g_pack.mesh.size() || g_pack.mesh[mi].tris.empty()) continue;
        if (!evhc_wreck_shown(E) || !evhc_resident(V, true)) continue;
        float e = 0.0f, sink = 0.0f;
        evhc_sink(E, V, now, &e, &sink);
        const unsigned ph = efx_hash(0x4556474Cu, (unsigned)it->first, 0u, 0u);
        const float t = now / 15.0f;
        float fl = 0.5f + 0.3f * sinf(t * 2.3f + 6.2831853f * eb_r(ph, 0))
                        + 0.2f * sinf(t * 5.7f + 6.2831853f * eb_r(ph, 1));
        if (fl < 0.0f) fl = 0.0f;
        if (fl > 1.0f) fl = 1.0f;
        const float k = (0.45f + 0.55f * fl) * (1.0f - e);
        if (k <= 0.0f) continue;
        if (!began) {
            began = true;
            glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
            st = ebld_gl_begin(false);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDepthMask(GL_FALSE);
            glDepthFunc(GL_LEQUAL);
        }
        const EbldSet& W = V.m.set[EBS_DESTROYED];
        const std::vector<int>& rg = V.m.rng[EBM_GLOWX];
        g_meshAlphaScale = (unsigned char)(255.0f * k + 0.5f);
        for (size_t i = 0; i < W.piece.size() && i < E.body.size(); i++) {
            const int* r = &rg[i * EBR];
            if (r[EBR_B] <= r[EBR_A] && r[EBR_RB] <= r[EBR_RA]) continue;
            float Mx[12], p[3];
            evhc_body_pose(E.body[i], W.piece[i], E.sinking, sink, Mx, p);
            const float lift = p[1] - terrain_y(p[0], p[2]);
            for (int blk = 0; blk < 2; blk++) {
                const int r0 = blk ? r[EBR_RA] : r[EBR_A], r1 = blk ? r[EBR_RB] : r[EBR_B];
                if (r1 <= r0) continue;
                ebld_gl_broken(st, blk == 1);
                draw_mesh(mi, p[0], p[2], -1, MODE_OPAQUE, 0, 0.0f, E.house, 1.0f, lift, false,
                          WOBBLE_NONE, -1.0f, Mx, (size_t)r0, (size_t)r1);
            }
            ebld_gl_broken(st, false);
        }
    }
    g_meshAlphaScale = 255;
    if (began) {
        ebld_gl_end(st);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(depthMask);
        glDepthFunc((GLenum)depthFunc);
    }
}

/* Mission boundary and shutdown. */
static void evhc_reset(void)
{
    g_evhcUnit.clear();
    g_evhcWreck.clear();
    g_evhcLastTick = -1;
    g_evhcWreckSerial = 0;
}

/* ================================================================================
 * THE DUMP. evhcdump: the models, every unit this system holds and what the colour
 * picture last drew of it. The event lines (fire, death, impact, sink, gone) print as
 * they happen.
 * ============================================================================== */
static void evhc_dump(void)
{
    int attached = 0;
    for (size_t i = 0; i < g_evhc.size(); i++) if (g_evhc[i].m.attached) attached++;
    printf("EVHC|loaded=%d|attached=%d|on=%d|enhanced=%d\n", (int)g_evhc.size(), attached,
           evhc_rule_now() ? 1 : 0, g_fx.enabled ? 1 : 0);
    static const char* const CLS[7] = { "tracked", "wheeled", "half-track", "helicopter",
                                        "fixed-wing", "hovercraft", "boat" };
    for (size_t i = 0; i < g_evhc.size(); i++) {
        EvhcModel& V = g_evhc[i];
        const EbldModel& M = V.m;
        int nk[EVP_N];
        for (int k = 0; k < EVP_N; k++) nk[k] = 0;
        for (size_t k = 0; k < V.part.size(); k++) nk[V.part[k].kind]++;
        /* where the remaster lies against the cartridge model it stands in for, pack units:
           x west to east, z north to south, and the highest y */
        float b[2][5];
        for (int w = 0; w < 2; w++) { b[w][0] = b[w][2] = b[w][4] = -1e9f; b[w][1] = b[w][3] = 1e9f; }
        if (M.attached) {
            const EbldSet& S = M.set[EBS_INTACT];
            for (size_t t = 0; t < S.tri.size(); t++)
                for (int q = 0; q < 3; q++) {
                    const EbldVert& v = S.tri[t].v[q];
                    if (v.x < b[0][1]) b[0][1] = v.x;
                    if (v.x > b[0][0]) b[0][0] = v.x;
                    if (v.z < b[0][3]) b[0][3] = v.z;
                    if (v.z > b[0][2]) b[0][2] = v.z;
                    if (v.y > b[0][4]) b[0][4] = v.y;
                }
            std::map<std::string, PackType>::const_iterator ct = g_pack.type.find(M.type);
            if (ct != g_pack.type.end() && ct->second.mesh >= 0
                && ct->second.mesh < (int)g_pack.mesh.size()) {
                const PackMesh& cm = g_pack.mesh[ct->second.mesh];
                for (size_t t = 0; t < cm.tris.size(); t++) {
                    if (cm.tris[t].mode == MODE_SHADOW) continue;
                    for (int q = 0; q < 3; q++) {
                        const PackVert& v = cm.tris[t].v[q];
                        if (v.x < b[1][1]) b[1][1] = v.x;
                        if (v.x > b[1][0]) b[1][0] = v.x;
                        if (v.z < b[1][3]) b[1][3] = v.z;
                        if (v.z > b[1][2]) b[1][2] = v.z;
                        if (v.y > b[1][4]) b[1][4] = v.y;
                    }
                }
            }
        }
        printf("EVHCART|%s|class=%s|pieces=%d|wreck=%d|turret=%d|belt=%d|wflip=%d|wheel=%d"
               "|rotor=%d|spin=%d|hinge=%d|clip=%d|deploy=%d|recoil=%d|tex=%d|drawn=%ld"
               "|x=%.0f,%.0f|z=%.0f,%.0f|top=%.0f|cartx=%.0f,%.0f|cartz=%.0f,%.0f|carttop=%.0f"
               "|rig=%d|rigdrawn=%ld\n",
               M.type.c_str(), CLS[V.cls >= 0 && V.cls < 7 ? V.cls : 0],
               (int)M.set[EBS_INTACT].piece.size(), (int)M.set[EBS_DESTROYED].piece.size(),
               nk[EVP_TURRET], nk[EVP_BELT], nk[EVP_WFLIP], nk[EVP_WHEEL], nk[EVP_ROTOR],
               nk[EVP_SPIN], nk[EVP_HINGE], nk[EVP_CLIP], nk[EVP_DEPLOY], nk[EVP_RECOIL],
               (int)M.tex.size(), V.drawn, b[0][1], b[0][0], b[0][3], b[0][2], b[0][4],
               b[1][1], b[1][0], b[1][3], b[1][2], b[1][4], V.rigOk ? 1 : 0, V.rigDrawn);
        V.drawn = 0;
        V.rigDrawn = 0;
    }
    for (std::map<int, EvhcUnit>::iterator it = g_evhcUnit.begin(); it != g_evhcUnit.end(); ++it) {
        EvhcUnit& U = it->second;
        printf("EVHCOBJ|%s|key=%d|id=%d|drawn=%d|tur=%d|trav=%.4f,%.4f|frame=%d,%d|turn=%.3f"
               "|rotor=%.4f|disc=%.2f|duct=%.1f|elev=%.1f|door=%.2f|shots=%d|kick=%.3f|clip=%.1f\n",
               U.type, it->first, U.id, U.drawnNow ? 1 : 0, U.dTur, U.dTrav[1], U.dTrav[0],
               U.dFrame[1], U.dFrame[0], U.dTurn, U.dRotor, U.dDisc, U.dDuct, U.dElev,
               U.dDoor, U.shots, U.dKick, U.dClip);
        U.drawnNow = false;
    }
    for (std::map<long long, EvhcWreck>::const_iterator it = g_evhcWreck.begin();
         it != g_evhcWreck.end(); ++it) {
        const EvhcWreck& E = it->second;
        int asleep = 0;
        float top = -1e9f, low = 1e9f;
        for (size_t i = 0; i < E.body.size(); i++) {
            if (E.body[i].asleep) asleep++;
            if (E.body[i].p[1] > top) top = E.body[i].p[1];
            if (E.body[i].p[1] < low) low = E.body[i].p[1];
        }
        printf("EVHCWRECK|%s|key=%lld|air=%d|falling=%d|sinking=%d|pieces=%d|asleep=%d"
               "|high=%.3f|low=%.3f|ground=%.3f|wet=%d|at=%.2f,%.2f\n", E.type, it->first,
               E.air ? 1 : 0, E.falling ? 1 : 0, E.sinking ? 1 : 0, (int)E.body.size(), asleep,
               top, low, E.ground, E.wet ? 1 : 0, E.wx, E.wz);
    }
    fflush(stdout);
}
#endif
