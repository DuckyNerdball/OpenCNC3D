/* ====================================================================================
 *  treadmark_mod.h -- GROUND TRACKS: the two bands a vehicle leaves on the ground
 *                     behind it, laid along the path it actually took.
 *
 *  TIER 2 ONLY, AND OURS. Neither the cartridge nor the 1995 game marks the ground
 *  where a vehicle has driven; nothing here is decoded from the ROM. It is registered
 *  with the known gaps as an authored addition, the same standing as the rain, the sun
 *  and the sea's surface. CLASSIC and the Win98 build never record a point and never
 *  draw a triangle: both halves return on the tier switch.
 *
 *  IT IS TIER 2 BY CHOICE, NOT BY MECHANISM, which is worth saying because the two are
 *  usually the same sentence in this tree. The pass is one texture unit, one blend and
 *  one env mode, fixed function, immediate mode, with the polygon offset the scorch
 *  marks already use. A Voodoo 2 could draw it. The Win98 fallback is therefore
 *  "absent", not "impossible", and it is written down that way.
 *
 *  WHAT IT IS NOT. It is not the grass crush field, which presses BLADES flat where a
 *  vehicle drove and is a map-wide render target sampled by the grass shader. That
 *  field marks grass and only grass; a map with no grass on it, which is every desert
 *  mission and most of the rest, shows nothing for it. This file marks THE GROUND. The
 *  HULL is the same measurement off the same mesh (crush_hull_for_mesh), so the two
 *  features agree about which vehicle is wide and the marks lie along each other rather
 *  than beside each other. The width DIALS are separate and deliberately carry
 *  different numbers: the field's stamp has a one-texel floor under it which is what
 *  sets its width in practice, and a decal has no such floor.
 *
 *  It is also not the running track on the hull. That is the tank's own tread texture
 *  scrolling as it drives (the tread phase, around the SimObject draw), and it shares
 *  nothing with this but the word.
 *
 *  THE TICK HALF AND THE DRAW HALF ARE SEPARATE, and this is the same ruling the tread
 *  phase and the crush field both had to learn: a draw does not happen on every tick
 *  under a script, so anything that advances per DRAWN frame makes two runs of one
 *  script disagree. So the path is recorded once per ENGINE tick, from the object
 *  refresh, deduplicated on the frame number; the draw half only reads it. The only
 *  clock either half reads is the engine's own (g_engineFrame for the birth of a point,
 *  engine_time() for the fade), so two --shot runs of one script are one picture.
 *
 *  THE TWO FADES, which are what a track needs to not become graffiti:
 *
 *    THE TIMER      a mark is laid at full strength and fades linearly to nothing over
 *                   tmark_life seconds of SIMULATED time. A vehicle that stops leaves
 *                   its whole trail behind to fade on this clock.
 *    THE DISTANCE   the trail is only drawn for tmark_dist cells behind the vehicle,
 *                   and the last 40 per cent of that is a ramp, so the oldest end
 *                   thins away instead of ending at a line across the ground.
 *
 *  They multiply, so whichever runs out first is the one the player sees.
 *
 *  THE JOINT IS MITRED, and that is not decoration either. Two quads butted end to end
 *  around a turn leave a wedge of bare ground on the outside of the corner and overlap
 *  on the inside, and an overlap under a multiply is a DOUBLE darkening: a bright notch
 *  and a dark notch at every corner. Mitring means consecutive quads share their
 *  two edge vertices exactly, so there is neither. The mitre scale is capped, because a
 *  reversal (a vehicle turning on the spot) sends 1/cos to infinity.
 *
 *  THE ART IS GENERATED, NOT BAKED, and that is a decision rather than a shortcut. A
 *  pack is a build product that ships beside the binaries and is versioned apart from
 *  them, so art in a pack is art that is missing from an install somebody updated the
 *  binaries in. Sixty-four texels of cleat pattern are cheaper to compute at first draw
 *  than to ship, and an old install draws them.
 *
 *  ONE ENTRY POINT PER JOB, and the host calls four lines:
 *
 *    treadmark_note_tick()  once per engine tick, in the object refresh, beside the
 *                           crush field's own observer. Touches no GL.
 *    treadmark_draw()       in draw_frame, over the ground and the tiberium ground and
 *                           UNDER the scorch marks, which is where a crater belongs
 *                           over a track rather than the other way round.
 *    treadmark_reset()      a scenario change or a save load: the trails belong to the
 *                           timeline just abandoned and the frame numbers restart.
 *    treadmark_free()       the mission teardown, which also drops the texture.
 * ==================================================================================== */
#ifndef OPENCNC3D_TREADMARK_MOD_H
#define OPENCNC3D_TREADMARK_MOD_H

/* THE PATH IS SAMPLED, NOT RECORDED TICK BY TICK. A Medium Tank moves about 0.05 cells
   a tick, so one point a tick is twenty points a cell and a trail of any length is
   thousands of triangles for a curve the eye cannot tell from a third of that. A point
   every TMARK_STEP cells is the same curve at a twentieth the cost; the engine turns
   units on eight facings, so the corner it rounds off is a corner the vehicle did not
   take smoothly either. */
#define TMARK_STEP        0.35f
/* The cap on one trail, and with the step above it is what the distance dial can
   actually reach: 63 segments at 0.35 cells is 22 cells of ground. The dial's own
   ceiling is below that on purpose, so the dial is never the thing that is lying. */
#define TMARK_MAX_POINTS  64
/* How many vehicles' trails are kept at once. A trail outlives its vehicle, because a
   destroyed tank's tracks are still on the ground, so this is not the same number as
   the vehicle count: 256 trails is a full skirmish plus everything that has died in the
   last minute of it. Past it the least recently moved trail goes, which is the one
   closest to fading out anyway. */
#define TMARK_MAX_TRAILS  256
/* The pattern tile. 64 across the band and 64 along it, mipmapped down to 1x1: a band
   is about five pixels wide on screen at the usual zoom, so this is minified eight
   times over and without mip levels it crawls. */
#define TMARK_TEX         64

/* One sampled point of one vehicle's path. NO across-vector is stored: the mitre needs
   the NEXT point to be known, and computing it at draw time from the neighbours is both
   simpler and correct for the last point of a trail that is still growing. */
struct TMarkPoint {
    float x, z;      /* engine truth, in cells                                    */
    float run;       /* path distance since this trail began, in cells            */
    int   frame;     /* the engine tick it was laid on                            */
    float red;       /* how much of it is laid in blood, 0..1 (tmark_blood)       */
};

struct TMarkTrail {
    std::vector<TMarkPoint> pts;
    float beam;          /* the hull, in cells, from its own mesh                 */
    float run;           /* path distance so far, in cells                        */
    int   lastFrame;     /* the last tick a point was added                       */
    int   bloodAt;       /* 1 + the last tick it drove through blood, 0 never     */
};

/* KEYED ON smooth_key, NEVER ON o.id, and the reason is recorded where the crush field
   records it: units and aircraft are separate heaps sharing one integer id space, and a
   heap slot is recycled into a new object on the tick after the old one died. Keyed on
   the bare id, the first tick of a replacement tank finds its predecessor's trail and
   draws a track from the war factory door to wherever that one was killed. The jump
   guard below is the second defence against the same thing, and against a reinforcement
   drop and a harvester coming back out of a refinery. */
static std::map<int, TMarkTrail> g_tmTrail;
static int    g_tmNoteFrame = -1;
static GLuint g_tmTex = 0;
/* THE SAME PATTERN AS COVERAGE, for the blood: a track laid through a body is painted
   dark red over the ground as well as pressed into it. Paint and not the multiply,
   because a multiply toward red over dark green grass comes out brown-black and reads as
   the ordinary track; the blood under the bodies learned the same thing. */
static GLuint g_tmTexA = 0;
static int    g_tmRedSegs = 0, g_tmBloodHits = 0;
static bool infdeath_blood_at(float x, float z, float rad);   /* infdeath_mod.h */
static int    g_tmDrawnSegs = 0;    /* last drawn frame, for the dump line */

/* ---- the tier switch, asked once so both halves agree -------------------------- */

/* NOT g_fxActive. That flag means "the post chain is running RIGHT NOW" and it is
   raised by the world pass and lowered when the chain presents, so the tick half --
   which runs between frames, from the object refresh -- would see it down on every tick
   a mission ever ran and record nothing at all, for ever, while the draw half kept
   working. That is the exact shape of a bug this tree has already shipped once. And the
   draw half does not want it either: these are ordinary blended triangles and they do
   not need a chain, only the tier. g_fx.enabled IS the Enhanced switch and it is stable
   across a frame. */
static int treadmark_live(void)
{
    return g_fx.enabled && g_fx.tmark_on && g_fx.tmark_dark > 0.0f;
}

static void treadmark_reset(void)
{
    g_tmTrail.clear();
    g_tmNoteFrame = -1;
    g_tmDrawnSegs = 0;
}

static void treadmark_free(void)
{
    treadmark_reset();
    if (g_tmTex) { glDeleteTextures(1, &g_tmTex); g_tmTex = 0; }
    if (g_tmTexA) { glDeleteTextures(1, &g_tmTexA); g_tmTexA = 0; }
}

/* ====================================================================================
 *  THE TICK HALF. No GL. Called once per engine tick from refresh_objects.
 * ==================================================================================== */

static void treadmark_note_tick(void)
{
    /* Switched off, the trails go. A feature that is switched on again then starts from
       the tick after rather than drawing a minute of history the player did not watch
       being laid, which is the same promise the crush field makes. */
    if (!treadmark_live()) {
        if (!g_tmTrail.empty()) g_tmTrail.clear();
        return;
    }

    /* SELF-DEDUPLICATED ON THE FRAME NUMBER. refresh_objects is called more than once
       for some ticks: the shot path re-dumps and the interactive loop re-dumps on a
       pause. A second record for one tick would lay two points at one position and age
       the trail twice for it. */
    if (g_engineFrame == g_tmNoteFrame)
        return;
    g_tmNoteFrame = g_engineFrame;

    const float life = g_fx.tmark_life > 0.1f ? g_fx.tmark_life : 0.1f;
    /* In TICKS, because that is what a point's birth is stored in. */
    const float lifeTicks = life * 15.0f;

    for (size_t i = 0; i < g_objects.size(); i++) {
        const SimObject& o = g_objects[i];
        /* Every exclusion here is a mark that would otherwise be drawn and should not
           be. K_UNIT only: an Orca is in the air (o.alt is in leptons) and would grind
           a trench along its flight path. Not a boat: BOAT and LST are on water. Not
           limboed: a harvester docking at a refinery stays in the dump for the whole
           599-tick dock, and without this it lays a point at the refinery door on every
           one of them. id >= 0 or smooth_key means nothing. */
        if (o.kind != K_UNIT || o.id < 0 || o.limbo) continue;
        if (water_is_boat(o)) continue;

        const int key = smooth_key(o);
        TMarkTrail& tr = g_tmTrail[key];

        /* THROUGH A BODY OR ITS BLOOD: from now its tracks are laid red (tmark_blood) */
        if (g_fx.tmark_blood > 0.0f && !tr.pts.empty()) {
            const float beam = tr.beam > 0.1f ? tr.beam : 0.4f;
            if (infdeath_blood_at(o.ex, o.ez, beam * 0.5f)) {
                if (!(tr.bloodAt > 0 && tr.bloodAt - 1 == g_engineFrame - 1)) g_tmBloodHits++;
                tr.bloodAt = g_engineFrame + 1;
            }
        }

        /* POSITION IS ENGINE TRUTH. o.wx is assigned from o.ex in the parse and only
           the DRAW overwrites it with the smoothed value, so at this point the two are
           the same number and the record carries no sub-tick phase. */
        const float x = o.ex, z = o.ez;

        if (tr.pts.empty()) {
            tr.beam = crush_hull_for_mesh(mesh_for(o)).beam;
            tr.run  = 0.0f;
        } else {
            const TMarkPoint& last = tr.pts.back();
            const float dx = x - last.x, dz = z - last.z;
            const float d  = sqrtf(dx * dx + dz * dz);
            /* THE SAME THRESHOLD THE MOVEMENT FILTER AND THE RUNNING TRACK USE. A step
               of more than a cell in one tick is not driving: it is a recycled heap
               slot, a reinforcement arriving, or a unit coming back out of a transport.
               The trail is dropped rather than bridged, because bridging it is the line
               across the map this guard exists to prevent. */
            if (d > SMOOTH_JUMP_CELLS) {
                tr.pts.clear();
                tr.bloodAt = 0;
                tr.beam = crush_hull_for_mesh(mesh_for(o)).beam;
                tr.run  = 0.0f;
            } else if (d < TMARK_STEP) {
                continue;               /* it has not travelled far enough to mark */
            } else {
                tr.run += d;
            }
        }

        TMarkPoint p;
        p.x = x; p.z = z; p.run = tr.run; p.frame = g_engineFrame;
        p.red = 0.0f;
        if (tr.bloodAt > 0 && g_fx.tmark_blood > 0.0f) {
            /* full red for the first half of tmark_blood, fading back over the second */
            const float t = (float)(g_engineFrame - (tr.bloodAt - 1)) / (g_fx.tmark_blood * 15.0f);
            p.red = t < 0.5f ? 1.0f : (t < 1.0f ? (1.0f - t) * 2.0f : 0.0f);
        }
        tr.pts.push_back(p);
        tr.lastFrame = g_engineFrame;
        /* The oldest point goes when the trail is full. It is also the faintest, since
           it is both the oldest in time and the furthest behind. */
        if ((int)tr.pts.size() > TMARK_MAX_POINTS)
            tr.pts.erase(tr.pts.begin());
    }

    /* ---- THE SWEEP: drop what has finished fading, and cap the book.
       A trail whose NEWEST point is past its life has nothing left to draw, whether its
       vehicle is still alive and parked or died a minute ago. */
    for (std::map<int, TMarkTrail>::iterator it = g_tmTrail.begin(); it != g_tmTrail.end(); ) {
        if (it->second.pts.empty()
            || (float)(g_engineFrame - it->second.pts.back().frame) > lifeTicks) {
            std::map<int, TMarkTrail>::iterator dead = it++;
            g_tmTrail.erase(dead);
        } else {
            ++it;
        }
    }
    while ((int)g_tmTrail.size() > TMARK_MAX_TRAILS) {
        std::map<int, TMarkTrail>::iterator oldest = g_tmTrail.begin();
        for (std::map<int, TMarkTrail>::iterator it = g_tmTrail.begin();
             it != g_tmTrail.end(); ++it)
            if (it->second.lastFrame < oldest->second.lastFrame) oldest = it;
        g_tmTrail.erase(oldest);
    }
}

/* ====================================================================================
 *  THE DRAW HALF.
 * ==================================================================================== */

/* ---- the cleat pattern, built once ------------------------------------------------
   S runs ACROSS one band, T runs ALONG it. ONE CHANNEL, and it is stored INVERTED: 255
   is ground the track does not touch and 0 is the middle of a cleat print. That is what
   the combiner below wants, and the account of why is there.

   BOTH AXES REPEAT, and the across axis is authored to survive it: the alpha falls to
   zero at both edges, so the texel at S=0 and the texel at S=1 are both nothing and the
   filter has nothing to carry across the seam. That is deliberate. GL_CLAMP_TO_EDGE is
   post-1.1 and every use of it in this tree is already on the Win98 debt list; a
   pattern that does not need clamping does not add to it. */
static void tmark_build_tex(void)
{
    if (g_tmTex) return;

    unsigned char lvl[TMARK_TEX * TMARK_TEX];
    for (int t = 0; t < TMARK_TEX; t++) {
        const float v = ((float)t + 0.5f) / (float)TMARK_TEX;
        for (int s = 0; s < TMARK_TEX; s++) {
            const float u = ((float)s + 0.5f) / (float)TMARK_TEX;
            /* Across the band: full in the middle, ramped to nothing over the outer
               third. A hard edge on a band five pixels wide crawls, and there is no
               multisampling in this context to resolve it. */
            const float d = fabsf(u - 0.5f) * 2.0f;
            float across = (1.0f - d) / 0.35f;
            if (across > 1.0f) across = 1.0f; else if (across < 0.0f) across = 0.0f;
            /* Along it: four cleats a tile, each one a shallow chevron pointing the way
               the vehicle is going, which is what a cleat print looks like from above.
               The phase is skewed by the distance from the centre line to bend the bar.  */
            float ph = v * 4.0f - fabsf(u - 0.5f) * 0.55f;
            ph -= floorf(ph);
            float cleat = ph / 0.08f;
            const float tail = (0.46f - ph) / 0.08f;
            if (tail < cleat) cleat = tail;
            if (cleat > 1.0f) cleat = 1.0f; else if (cleat < 0.0f) cleat = 0.0f;
            /* The ground between two cleats is pressed too, just less: a track is a
               continuous band of disturbed earth with the cleats stamped into it, and
               cleats alone read as a row of unconnected dashes. */
            const float a = across * (0.30f + 0.70f * cleat);
            lvl[t * TMARK_TEX + s] = (unsigned char)((1.0f - a) * 255.0f + 0.5f);
        }
    }

    for (int pass = 1; pass >= 0; pass--) {
    /* pass 1 the coverage (alpha, 255 the middle of a cleat), pass 0 the multiply */
    unsigned char inv[TMARK_TEX * TMARK_TEX];
    for (int i = 0; i < TMARK_TEX * TMARK_TEX; i++) inv[i] = (unsigned char)(255 - lvl[i]);
    const GLenum fmt = pass ? GL_ALPHA : GL_LUMINANCE;
    unsigned char* lv0 = pass ? inv : lvl;
    glGenTextures(1, pass ? &g_tmTexA : &g_tmTex);
    glBindTexture(GL_TEXTURE_2D, pass ? g_tmTexA : g_tmTex);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, TMARK_TEX, TMARK_TEX, 0,
                 fmt, GL_UNSIGNED_BYTE, lv0);
    /* THE MIP CHAIN IS BOX FILTERED HERE rather than asked of the driver, because the
       two ways of asking for it are both post-1.1 (GL_GENERATE_MIPMAP is 1.4, and the
       glu builder is a second library this tree does not otherwise need). Seven levels,
       64 down to 1. */
    {
        unsigned char src[TMARK_TEX * TMARK_TEX];
        memcpy(src, lv0, sizeof lvl);
        int w = TMARK_TEX, level = 0;
        while (w > 1) {
            const int hw = w / 2;
            unsigned char dst[TMARK_TEX * TMARK_TEX];
            for (int t = 0; t < hw; t++)
                for (int s = 0; s < hw; s++) {
                    const int a = src[(t * 2)     * w + (s * 2)];
                    const int b = src[(t * 2)     * w + (s * 2 + 1)];
                    const int e = src[(t * 2 + 1) * w + (s * 2)];
                    const int f = src[(t * 2 + 1) * w + (s * 2 + 1)];
                    dst[t * hw + s] = (unsigned char)((a + b + e + f + 2) / 4);
                }
            level++;
            w = hw;
            glTexImage2D(GL_TEXTURE_2D, level, fmt, w, w, 0,
                         fmt, GL_UNSIGNED_BYTE, dst);
            memcpy(src, dst, (size_t)w * w);
        }
    }
    /* DELIBERATELY LINEAR, AND DELIBERATELY NOT IN THE BILINEAR REGISTER. The switch
       that register serves exists so that switching bilinear filtering OFF gives the
       shipped picture back texel for texel, and the shipped picture of authored pixel
       art is point sampled. This is not art, it is a generated gradient with no texel
       grid to preserve, and it is in the same class as the infantry shadow blob and the
       particle sprites, which are linear whatever the switch says. Point sampling it
       would buy nothing and cost the mip chain. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
}

/* Is there ground here to mark at all: on the map, out from under the shroud, and not a
   holed cell, which is where the sea is drawn and where a track would float on the
   water. */
static int tmark_cell_ok(float wx, float wz)
{
    const int cx = (int)floorf(wx), cz = (int)floorf(wz);
    if (cx < 0 || cz < 0 || cx >= g_gridW || cz >= g_gridH) return 0;
    if (g_cellHoles[cz * g_gridW + cx]) return 0;
    return cell_shown(cx, cz) ? 1 : 0;
}

static void treadmark_draw(void)
{
    g_tmDrawnSegs = 0;
    if (!treadmark_live() || g_tmTrail.empty() || g_pack.corner.empty())
        return;

    tmark_build_tex();
    if (!g_tmTex) return;

    const float life   = g_fx.tmark_life > 0.1f ? g_fx.tmark_life : 0.1f;
    /* The dial cannot ask for more ground than the trail is allowed to hold, and a dial
       that silently means something else is worse than a shorter one. */
    const float maxRun = (float)(TMARK_MAX_POINTS - 1) * TMARK_STEP;
    float dist = g_fx.tmark_dist > 0.5f ? g_fx.tmark_dist : 0.5f;
    if (dist > maxRun) dist = maxRun;
    const float tail   = dist * 0.40f;            /* the ramp at the far end        */
    const float cleat  = g_fx.tmark_cleat > 0.02f ? g_fx.tmark_cleat : 0.02f;
    const float now    = engine_time();           /* ticks, with the sub-tick phase */

    /* A TRACK MULTIPLIES THE GROUND, IT DOES NOT PAINT OVER IT, and this is the one
       decision in the file that a picture settles rather than an argument. Painting a
       fixed dark brown at an alpha is what a decal normally does, and on the sand of a
       desert map it looks right; on the dark grass of a temperate one the paint is no
       darker than the ground already is, and a measured 4529 pixels of track read as a
       faint smudge that could be a texture seam. A multiply is proportional: whatever
       the ground's colour, the track is a fraction of it, so it is exactly as legible
       on grass as on sand and it cannot be brighter than the ground it lies on.
 
       HOW ONE FIXED-FUNCTION PASS DOES IT, because the arithmetic is not obvious.
       GL_BLEND as the texture env computes  C = Cf*(1-Lt) + Cc*Lt  from the primary
       colour Cf, the env colour Cc and the one-channel texture Lt. With the env colour
       WHITE that is a mix from Cf to 1 by Lt, so a texel of 1 leaves the ground alone
       and a texel of 0 stamps the primary colour whole -- which is why the pattern is
       stored inverted. The frame buffer then takes dst*src through (GL_ZERO,
       GL_SRC_COLOR). The per-vertex fade rides in Cf, which each vertex sets to
       1 - fade*(1 - tint): at fade 0 that is white and the multiply is the identity, so
       a mark that has faded out costs a blend and changes not one bit.
 
       THE GROUND'S OWN LIGHT IS NOT APPLIED HERE, and the scorch marks DO apply it. The
       difference is the blend, not an inconsistency: a scorch adds its own paint and has
       to be dimmed to belong to a dark cell, while a multiply is already a fraction of
       whatever the ground drew, shade, shroud, terrain tint and all. Multiplying by the
       shade a second time would darken a track on a shaded slope twice over. */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tmTex);
    {
        const GLfloat white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, white);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    /* A DECAL, NOT A LIFT, and the same offset the scorch marks take. The track shares
       the ground's own depth and lets the hardware resolve the tie; depth writes are
       off, so the crystals and the hulls drawn afterwards test against the ground they
       are standing on rather than against a film floating over it. */
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    glDepthMask(GL_FALSE);

    /* the red laid over the blooded stretches, gathered here and painted after */
    static std::vector<float> red;
    red.clear();
    g_tmRedSegs = 0;
    glBegin(GL_TRIANGLES);
    for (std::map<int, TMarkTrail>::const_iterator it = g_tmTrail.begin();
         it != g_tmTrail.end(); ++it) {
        const TMarkTrail& tr = it->second;
        const int n = (int)tr.pts.size();
        if (n < 2) continue;

        /* THE HULL DECIDES THE GAUGE, so a Recon Bike does not lay Mammoth tracks. beam
           is the measured width of this type's own mesh; the two dials are fractions of
           it rather than absolute widths for exactly that reason. */
        const float halfGauge = tr.beam * g_fx.tmark_gauge * 0.5f;
        const float halfBand  = tr.beam * g_fx.tmark_band  * 0.5f;
        if (halfBand <= 0.0f) continue;

        /* The mitre at every point, from its neighbours. Computed for the whole trail
           first because a segment needs both of its ends. */
        float nx[TMARK_MAX_POINTS], nz[TMARK_MAX_POINTS], mi[TMARK_MAX_POINTS];
        for (int i = 0; i < n; i++) {
            float ax = 0.0f, az = 0.0f;       /* the direction arriving at this point */
            float bx = 0.0f, bz = 0.0f;       /* and the one leaving it               */
            if (i > 0) {
                ax = tr.pts[i].x - tr.pts[i - 1].x;
                az = tr.pts[i].z - tr.pts[i - 1].z;
                const float l = sqrtf(ax * ax + az * az);
                if (l > 1e-6f) { ax /= l; az /= l; } else { ax = az = 0.0f; }
            }
            if (i < n - 1) {
                bx = tr.pts[i + 1].x - tr.pts[i].x;
                bz = tr.pts[i + 1].z - tr.pts[i].z;
                const float l = sqrtf(bx * bx + bz * bz);
                if (l > 1e-6f) { bx /= l; bz /= l; } else { bx = bz = 0.0f; }
            }
            if (i == 0)      { ax = bx; az = bz; }
            if (i == n - 1)  { bx = ax; bz = az; }
            float mx = ax + bx, mz = az + bz;
            const float ml = sqrtf(mx * mx + mz * mz);
            if (ml > 1e-6f) { mx /= ml; mz /= ml; } else { mx = 0.0f; mz = 1.0f; }
            /* The across vector is the mitre turned a quarter turn in the ground plane.
               Its length is 1/cos(half the turn), CAPPED: a vehicle reversing on the
               spot sends that to infinity and would throw a band across the map. */
            nx[i] =  mz; nz[i] = -mx;
            float dot = mx * bx + mz * bz;
            if (dot < 0.40f) dot = 0.40f;
            mi[i] = 1.0f / dot;
        }

        for (int i = 0; i < n - 1; i++) {
            const TMarkPoint& p0 = tr.pts[i];
            const TMarkPoint& p1 = tr.pts[i + 1];

            /* THE TWO FADES, per end, so the trail thins along its length instead of in
               steps. Age is in ticks of simulated time; the distance is how far behind
               the vehicle's current position this end of the segment sits. */
            const float a0age = 1.0f - (now - (float)p0.frame) / (life * 15.0f);
            const float a1age = 1.0f - (now - (float)p1.frame) / (life * 15.0f);
            float a0dst = (dist - (tr.run - p0.run)) / tail;
            float a1dst = (dist - (tr.run - p1.run)) / tail;
            if (a0dst > 1.0f) a0dst = 1.0f;
            if (a1dst > 1.0f) a1dst = 1.0f;
            float a0 = a0age < a0dst ? a0age : a0dst;
            float a1 = a1age < a1dst ? a1age : a1dst;
            if (a0 < 0.0f) a0 = 0.0f;
            if (a1 < 0.0f) a1 = 0.0f;
            if (a0 <= 0.0f && a1 <= 0.0f) continue;
            a0 *= g_fx.tmark_dark;
            a1 *= g_fx.tmark_dark;
            /* The fade, turned into the colour the ground is multiplied by. Fade 0 is
               white, which is the identity. */
            const float r0 = 1.0f - a0 * (1.0f - g_fx.tmark_r);
            const float g0 = 1.0f - a0 * (1.0f - g_fx.tmark_g);
            const float b0 = 1.0f - a0 * (1.0f - g_fx.tmark_b);
            const float r1 = 1.0f - a1 * (1.0f - g_fx.tmark_r);
            const float g1 = 1.0f - a1 * (1.0f - g_fx.tmark_g);
            const float b1 = 1.0f - a1 * (1.0f - g_fx.tmark_b);

            /* Off the glass, under the black, or over the sea. Tested per segment
               rather than per vertex: a segment is a third of a cell and the answer
               cannot differ meaningfully across one. */
            if (!tmark_cell_ok(p0.x, p0.z) || !tmark_cell_ok(p1.x, p1.z))
                continue;
            if (g_viewValid) {
                const float lo_x = p0.x < p1.x ? p0.x : p1.x;
                const float hi_x = p0.x > p1.x ? p0.x : p1.x;
                const float lo_z = p0.z < p1.z ? p0.z : p1.z;
                const float hi_z = p0.z > p1.z ? p0.z : p1.z;
                if (hi_x < g_viewX0 - 1.0f || lo_x > g_viewX1 + 1.0f ||
                    hi_z < g_viewZ0 - 1.0f || lo_z > g_viewZ1 + 1.0f)
                    continue;
            }

            const float v0 = p0.run / cleat;
            const float v1 = p1.run / cleat;

            /* Two bands, left and right of the centre line. */
            for (int b = -1; b <= 1; b += 2) {
                const float c0 = (float)b * halfGauge * mi[i];
                const float c1 = (float)b * halfGauge * mi[i + 1];
                const float w0 = halfBand * mi[i];
                const float w1 = halfBand * mi[i + 1];
                const float x0a = p0.x + nx[i]     * (c0 - w0);
                const float z0a = p0.z + nz[i]     * (c0 - w0);
                const float x0b = p0.x + nx[i]     * (c0 + w0);
                const float z0b = p0.z + nz[i]     * (c0 + w0);
                const float x1a = p1.x + nx[i + 1] * (c1 - w1);
                const float z1a = p1.z + nz[i + 1] * (c1 - w1);
                const float x1b = p1.x + nx[i + 1] * (c1 + w1);
                const float z1b = p1.z + nz[i + 1] * (c1 + w1);
                const float y0a = terrain_y(x0a, z0a), y0b = terrain_y(x0b, z0b);
                const float y1a = terrain_y(x1a, z1a), y1b = terrain_y(x1b, z1b);

                glColor3f(r0, g0, b0); glTexCoord2f(0.0f, v0); glVertex3f(x0a, y0a, z0a);
                glColor3f(r0, g0, b0); glTexCoord2f(1.0f, v0); glVertex3f(x0b, y0b, z0b);
                glColor3f(r1, g1, b1); glTexCoord2f(1.0f, v1); glVertex3f(x1b, y1b, z1b);

                glColor3f(r0, g0, b0); glTexCoord2f(0.0f, v0); glVertex3f(x0a, y0a, z0a);
                glColor3f(r1, g1, b1); glTexCoord2f(1.0f, v1); glVertex3f(x1b, y1b, z1b);
                glColor3f(r1, g1, b1); glTexCoord2f(0.0f, v1); glVertex3f(x1a, y1a, z1a);
                g_tmDrawnSegs++;
                if (p0.red > 0.0f || p1.red > 0.0f) {
                    const float q0 = p0.red * a0, q1 = p1.red * a1;
                    const float V[6][6] = {
                        { x0a, y0a, z0a, 0.0f, v0, q0 }, { x0b, y0b, z0b, 1.0f, v0, q0 }, { x1b, y1b, z1b, 1.0f, v1, q1 },
                        { x0a, y0a, z0a, 0.0f, v0, q0 }, { x1b, y1b, z1b, 1.0f, v1, q1 }, { x1a, y1a, z1a, 0.0f, v1, q1 } };
                    red.insert(red.end(), &V[0][0], &V[0][0] + 36);
                    g_tmRedSegs++;
                }
            }
        }
    }
    glEnd();

    /* THE BLOOD, painted over the pressed track through the same pattern as coverage,
       with the ground's own light on it as the blood under the bodies takes it. */
    if (!red.empty() && g_tmTexA) {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glBindTexture(GL_TEXTURE_2D, g_tmTexA);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i + 6 <= red.size(); i += 6) {
            const float* v = &red[i];
            const float sh = terrain_shade((int)floorf(v[0] + 0.5f), (int)floorf(v[2] + 0.5f))
                           * (g_objShroud ? 1.0f - shroud_env_at(v[0], v[2]) : 1.0f);
            glColor4f(0.46f * sh, 0.035f * sh, 0.03f * sh, v[5] * 0.9f);
            glTexCoord2f(v[3], v[4]);
            glVertex3f(v[0], v[1], v[2]);
        }
        glEnd();
    }

    /* THE ENV MODE GOES BACK TO MODULATE. Every other pass in this renderer assumes it
       and sets it only when it happens to care; leaving GL_BLEND bound here would paint
       the next pass that does not say otherwise with this file's env colour. */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

/* What the pass did this frame, for the gate and for a bug report: how many trails are
   on the book, how many band quads were drawn, and the two dials that decide it.

   AND WHERE THE MAP STOPS, which is not a constant and must never be assumed by anything
   reading this line. The sidebar magnifies by whole numbers, so its width follows the UI
   scale dial: the same window width puts the bar's left edge at 960 under one setting and
   1120 under the next, and the 160 columns between them are ordinary ground. A test that
   nothing in this pass reached the sidebar, written against a hard-coded column, therefore
   measures ground as chrome wherever the dial is not the one it was written under, and
   calls a correct build broken. Reported here so the measurement asks the renderer. */
static void treadmark_dump(int fbw, int fbh)
{
    int pts = 0;
    for (std::map<int, TMarkTrail>::const_iterator it = g_tmTrail.begin();
         it != g_tmTrail.end(); ++it)
        pts += (int)it->second.pts.size();
    int redPts = 0;
    for (std::map<int, TMarkTrail>::const_iterator it = g_tmTrail.begin();
         it != g_tmTrail.end(); ++it)
        for (size_t i = 0; i < it->second.pts.size(); i++)
            redPts += it->second.pts[i].red > 0.0f;
    printf("TMARK|on=%d|trails=%d|points=%d|quads=%d|life=%.1f|dist=%.1f|dark=%.2f"
           "|tacright=%d\n",
           treadmark_live() ? 1 : 0, (int)g_tmTrail.size(), pts, g_tmDrawnSegs,
           g_fx.tmark_life, g_fx.tmark_dist, g_fx.tmark_dark,
           (int)sb_tactical_right(fbw, fbh));
    printf("TMARKBLOOD|hits=%d|redpoints=%d|redquads=%d|secs=%.1f\n",
           g_tmBloodHits, redPts, g_tmRedSegs, g_fx.tmark_blood);
}

#endif /* OPENCNC3D_TREADMARK_MOD_H */
