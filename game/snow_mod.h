/* ====================================================================================
 *  snow_mod.h -- SNOWFALL UNDER ENHANCED, on Red Alert's snow maps.
 *
 *  TIER 2 ONLY, AND OURS. Neither game has weather of any kind; nothing here is decoded
 *  from a disc or a cartridge. It is an authored addition with the same standing as the
 *  rain (rain_mod.h), whose design it follows. CLASSIC, Win98 and any frame the post
 *  chain cannot run draw exactly what they drew before this file existed: the one entry
 *  point returns at once unless the chain is live and the snow switch is on.
 *
 *  WHERE IT FALLS. Only on Red Alert's SNOW theater (pack theater RASNOW, with the Red
 *  Alert module loaded). Never in Tiberian Dawn, whose winter maps are frost on grass
 *  rather than snow cover, never on Red Alert's temperate or interior maps, and never on
 *  the editor's SNOW theater. The switch and its dials are one setting for every
 *  theater; the theater alone decides where the snow shows, so a map change never needs
 *  a saved setting to move.
 *
 *  ONE DRAW CALL, NOTHING UPLOADED PER FRAME. A fixed lattice of small round sprites,
 *  built once into a static vertex buffer, holds only SEEDS; a vertex shader turns a
 *  seed and the engine clock into a flake's position, the way the rain's streaks are
 *  placed:
 *
 *    - a flake's LANDING POINT sits on a periodic lattice, and the shader picks the copy
 *      nearest the middle of the view, so panning does not drag the snow with it. The
 *      copy changes half a period from the middle, which is always off screen;
 *    - its HEIGHT is a phase of the fall that the clock advances, measured from the REAL
 *      GROUND under the point it lands on (a small height texture of the map, below), so
 *      the fall is the same length over a hill as over a valley and ends on the ground;
 *    - the WIND carries it sideways in proportion to the height still to fall, so every
 *      flake drifts at one steady speed along one bearing and still lands on its point;
 *    - its FLUTTER is a small sideways circle with its own phase and rate, a whole number
 *      of turns per fall, so it lands exactly where the circle started;
 *    - once down it COMES TO REST: it stays where it landed, without drift or flutter,
 *      and fades out over snow_settle seconds (a third of a second on open water, which
 *      would not hold it), then is back at the top of the volume with the same seed.
 *      Only when the lattice cannot hold that many resting flakes on top of the falling
 *      ones (a heavy snow left to lie long, or a fast one) is the rest shortened, never
 *      the number in the air (snow_settle_held).
 *
 *  THE GROUND UNDER THE SNOW. The renderer's own answer to "how high is the ground
 *  here", terrain_y (corner heights, the building pads, the Enhanced relief and the
 *  water's surface, the function that stands units, props and trees on the map), is
 *  sampled four times a cell over the play rectangle into a 16-bit height texture once
 *  a map is loaded, and again whenever any of those inputs moves: a building's pad only
 *  where the ground moved, anything else whole. The vertex shader reads it with
 *  texture2DLod. A GL with no vertex texture units links a shader without that read and
 *  keeps the old answer, the terrain's zero. A flake is lifted by its own radius and,
 *  from a couple of its widths above the ground down to where it lies, its depth is drawn
 *  up to a flake's width toward the eye, so the ground it lies on does not swallow its
 *  lower half, while a hill in front of it still hides it.
 *
 *  A ZOOM CHANGES SIZES AND COUNTS, NEVER POSITIONS. The lattice period is fixed in
 *  cells, sized for the farthest view the camera rig allows, and the phase of every
 *  fall runs against the height dial rather than the zoomed height; so a zoom step,
 *  which arrives every frame while the zoom is held, moves a flake only by the change in
 *  its drawn height (the volume follows the zoom) and in the wind's slant over it, a
 *  small fraction of a cell for a step. What the zoom does change is how many
 *  of the lattice are drawn: zoomed in, the same period holds more flakes than the view
 *  needs, so more of them are drawn and the count on screen stays about the same at
 *  every zoom. The extra flakes fade in and out one by one at the end of the drawn
 *  range, so a zoom thins or fills the snow instead of reshuffling it.
 *
 *  A FLAKE HAS A SIZE IN THE WORLD AND A FLOOR ON SCREEN. Its size is in cells, so a
 *  near flake is larger than a far one, scaled with the zoom so that the snow looks
 *  the same at every zoom. A flake that would project smaller than a pixel and a
 *  half is drawn at that floor with its alpha reduced by the area it lost: the same
 *  amount of light lands on the screen, and a far flake stays a faint dot instead of
 *  flickering in and out as the anti-aliasing jitter moves it across pixel centres.
 *
 *  WHERE IT IS DRAWN, and why there. Inside the world pass, after every object and
 *  effect and with the rain, BEFORE the shroud blanket (so the blanket's black covers
 *  the snow over unexplored ground, as it covers an object; the soft rim of the
 *  Enhanced fog is a mask the snow does not read, so a faint flake can show through
 *  it, as a raindrop can) and between the two scene
 *  copies the anti-aliasing takes around the effects, so the reactive mask counts every
 *  flake as an effect and the blend takes it from the current frame instead of smearing
 *  it into a trail behind its motion (fx_taa_snapshot, fx_post.h). Depth-tested against
 *  the world with depth writes off: a hill or a building hides the snow behind it.
 *
 *  The light pass therefore shades a flake's pixels as the ground behind it, as it does
 *  the rain's. That is accepted on purpose. The alternatives were a draw after the light
 *  pass, which falls outside both the shroud and the reactive mask and would need its
 *  own copy of each, or a flag in the scene's alpha for the light pass to undo, which
 *  touches every lighting term in that shader for one weather feature. What the shading
 *  does in practice is mild: on a snow map the ground behind most flakes is snow, which
 *  the light pass keeps at its own brightness in the sun and cools to a blue grey in
 *  shade; a flake over a cast shadow takes the shadow's tone and stays as much brighter
 *  than the shadow as it is brighter than open snow, which reads as a flake falling
 *  through the shade. The flake colour is set a little below white, so that a sunward
 *  slope, which the light pass lifts above level ground, can carry a flake's core to
 *  white but not into the bloom's halo.
 *
 *  DETERMINISM. The only clock is g_fxTime (engine ticks and the frame's interpolation),
 *  FOLDED to each speed's own cycle before it is handed over (snow_time); the only random
 *  numbers are a fixed LCG run once at boot. Two --shot runs of one script are one
 *  picture. Being the engine's clock, it stops when the game is paused, runs faster at a
 *  faster game speed and steps once a tick when smooth animation is off, as the rain and
 *  every other effect on the world does.
 *
 *  EVERY NUMBER IS A DIAL in fx_state.h ("3d2  SNOWFALL  (ours)"), in the F5 panel's
 *  WEATHER tab. snow_fx 0 draws nothing.
 * ==================================================================================== */
#ifndef OPENCNC3D_SNOW_MOD_H
#define OPENCNC3D_SNOW_MOD_H

/* The lattice. SNOW_DENSE flakes is the heaviest snow in a lattice cut tight to the
   view; the amount dial takes a square law of it. The array holds more, because the
   period is sized for the farthest view and a nearer one draws more of it to keep the
   count on screen (see the header), and because a flake spends part of its cycle at
   rest, so more are drawn to keep as many in the air. Each sprite is four vertices of
   eight floats in one static buffer: 65536 sprites is 8 MiB, made once. The seeds of
   the first 32768 are the ones a smaller lattice had, so those flakes did not move.
   It holds the default amount resting the longest the dial allows at the nearest zoom
   (about 62,600); beyond that the rest gives way (snow_settle_held), not the falling snow. */
#define SNOW_DENSE   6000
#define SNOW_LATTICE 65536
/* height samples a cell in the ground texture */
#define SNOW_GROUND_RES 4
/* the drawn range ends in a ramp 64 flakes long (the shader's uCount term), so the
   count moves smoothly; the draw takes that many more than the count */
#define SNOW_COUNT_RAMP 64

static GLuint g_snowProg = 0;
static int    g_snowProgTried = 0;
static std::vector<float> g_snowVerts;    /* 8 floats a vertex: four seeds, the corner, two more */
static GLuint g_snowBuf = 0;              /* the same, in a static vertex buffer           */
static int    g_snowBufTried = 0;
static int    g_snowLogged = -1;          /* last state written to stderr, so it is once  */
static GLuint g_snowGroundTex = 0;        /* the ground's height, SNOW_GROUND_RES a cell   */
static int    g_snowGroundW = 0, g_snowGroundH = 0;
static int    g_snowGroundOk = 0;
static unsigned g_snowGroundKey[12];      /* what the texture was built from               */
static float  g_snowGroundUV[4];          /* world x, z to texture u, v: x*a + b           */
static float  g_snowGroundLo = 0.0f, g_snowGroundSpan = 1.0f;   /* 0..1 back to cells  */
static int    g_snowVtu = -1;             /* vertex texture units, asked once              */
static std::vector<unsigned short> g_snowGroundPx;   /* the texture's texels, for a patch */
static unsigned g_snowGroundSeq = 0;      /* the ground stamp (g_groundSeq) it is current to */
static int    g_snowGroundLoose = 1;      /* the relief moved off a pad change: next is whole */
static unsigned g_snowWatchPad = 0, g_snowWatchSurf = 0;
static float  g_snowSettle = 0.0f;        /* the rest this frame, from snow_settle_held    */
static float  g_snowSettleLogged = -1.0f;
static int    g_snowWaterLogged = -1;

/* ---- is the snow on at all -------------------------------------------------------- */

/* Red Alert's snow theater, and only that. */
static int snow_theater(void)
{
    return g_raModule && !strcmp(g_pack.theater, "RASNOW");
}

static int snow_live(void)
{
    /* the chain must be running this frame: no chain, no snow, whatever the dial says */
    return g_fx.enabled && g_fx.snow_fx && g_fxActive && fx_gl_ready
        && g_fx.snow_amount > 0.0f && g_fx.snow_opacity > 0.0f && snow_theater();
}

/* ---- the clock, folded ------------------------------------------------------------------
   A flake falls at one of four speeds (0.75, 1.0, 1.25 and 1.5 times the dial), so its
   fall takes H / speed seconds, H the height dial (not the zoomed height, so nothing
   here moves with the zoom). It then lies on the ground for snow_settle seconds while
   it fades, and its cycle is the two together. Each speed is handed the engine clock
   folded by ITS OWN cycle, so every phase built from the folded clock is exactly the
   phase the unfolded clock would give and a long session cannot coarsen it; and no
   speed idles through part of a longer common cycle, invisible but still drawn. With
   snow_settle 0 the cycle is the fall alone, which is the snow as it was before a flake
   could rest. */
static float snow_ref_height(void)
{
    return g_fx.snow_height > 0.5f ? g_fx.snow_height : 0.5f;
}
static float snow_speed_dial(void)
{
    return g_fx.snow_speed > 0.05f ? g_fx.snow_speed : 0.05f;
}
static float snow_settle_dial(void)
{
    const float s = g_fx.snow_settle;
    return s > 0.0f ? (s < 10.0f ? s : 10.0f) : 0.0f;
}
/* seconds of the fall for speed class c, 0..3 */
static float snow_fall_secs(int c)
{
    return snow_ref_height() / (snow_speed_dial() * (0.75f + 0.25f * (float)c));
}
static float snow_cycle_secs(int c)
{
    return snow_fall_secs(c) + g_snowSettle;
}
static float snow_time(int c)
{
    return fmodf(g_fxTime, snow_cycle_secs(c));
}
/* The share of the drawn flakes that is in the air at any moment with a rest of `settle`
   seconds, over the four speeds (each a quarter of the lattice): the drawn count is
   divided by it, so the number FALLING is the number the amount dial gives with no
   rest at all. */
static float snow_air_share(float settle)
{
    float a = 0.0f;
    for (int c = 0; c < 4; c++) {
        const float f = snow_fall_secs(c);
        a += f / (f + settle);
    }
    return 0.25f * a;
}
/* THE REST THE LATTICE CAN HOLD. `need` is the count in the air at the nearest zoom the
   rig allows, where the most of the lattice is drawn. Dividing it by the air share must
   leave it inside the lattice, or the draw is capped and the cap takes flakes out of the
   air as well as off the ground. So when the dial's rest would pass it, the rest is
   shortened to the longest that fits, found by halving (the air share falls as the rest
   grows). It is worked out at the nearest zoom, not the current one, so it depends only
   on the dials and the window's shape: a zoom never changes it, and so never moves a
   flake's phase. At the default amount and speed the whole dial fits; at amount 1 the
   rest holds up to about four seconds, and a faster fall holds less. */
static float snow_settle_held(float need, float cap)
{
    const float dial = snow_settle_dial();
    if (need <= 0.0f || need <= cap * snow_air_share(dial)) return dial;
    if (need >= cap) return 0.0f;
    float lo = 0.0f, hi = dial;
    for (int i = 0; i < 24; i++) {
        const float mid = 0.5f * (lo + hi);
        if (need <= cap * snow_air_share(mid)) lo = mid; else hi = mid;
    }
    return lo;
}

/* ---- the flakes ---------------------------------------------------------------------- */

/* THE VERTEX SHADER PLACES THE SNOW. gl_Vertex holds the seeds: x and z the landing
   point on the lattice, y the phase of the fall, and w the flake's index in the lattice
   plus a fourth seed below 1 (the speed class and brightness). gl_MultiTexCoord0 holds
   the corner (x, y in -1..1) and two more seeds: z the size and brightness, w the
   flutter. The sprite is built in clip space around the projected centre, so it is round
   on screen from any camera; its depth is the centre's, drawn up to a flake's width
   nearer the eye as it comes down, so the ground it rests on cannot cut it in half.
   SNOW_GROUND and SNOW_WATER, defined ahead of this text by snow_draw from the vertex
   texture units the GL has, leave out the reads it cannot do, so the program still links
   there (snow_link). */
static const char* SNOW_VS_BODY =
    "uniform vec2 uCentre; uniform float uPeriod; uniform float uHeight;\n"
    "uniform float uCount;\n"
    "uniform vec4 uTimes; uniform vec4 uCycles; uniform vec4 uFalls;\n"
    "uniform float uSettle; uniform float uSettleWater; uniform float uEndFade;\n"
    "uniform vec2 uDrift; uniform float uSway;\n"
    "uniform float uFlutter; uniform float uSize; uniform float uMinPx; uniform float uMaxPx;\n"
    "uniform vec2 uHalfVP;\n"
    "uniform vec3 uUp; uniform vec3 uCamPos; uniform float uNear; uniform float uLook;\n"
    "uniform vec4 uRect;\n"
    "#if SNOW_GROUND\n"
    "uniform sampler2D uGround; uniform float uGroundOn; uniform vec4 uGroundUV; uniform vec2 uGroundRange;\n"
    "#endif\n"
    "#if SNOW_WATER\n"
    "uniform sampler2D uWater; uniform float uWaterOn; uniform vec2 uWaterScale;\n"
    "#endif\n"
    "varying vec2 vUV; varying float vFade; varying float vBright;\n"
    /* how far a ground point is inside the play rectangle, in cells */
    "float inside(vec2 q) {\n"
    "    return min(min(q.x - uRect.x, uRect.z - q.x), min(q.y - uRect.y, uRect.w - q.y));\n"
    "}\n"
    /* the ground's height under a point; without the texture, the terrain's zero */
    "float ground(vec2 q) {\n"
    "#if SNOW_GROUND\n"
    "    if (uGroundOn < 0.5) return 0.0;\n"
    "    return uGroundRange.x + uGroundRange.y\n"
    "         * texture2DLod(uGround, q * uGroundUV.xz + uGroundUV.yw, 0.0).r;\n"
    "#else\n"
    "    return 0.0;\n"
    "#endif\n"
    "}\n"
    /* 1 where the water field says open water, 0 elsewhere or without the field */
    "float wetAt(vec2 q) {\n"
    "#if SNOW_WATER\n"
    "    return uWaterOn > 0.5 ? step(0.35, texture2DLod(uWater, q * uWaterScale, 0.0).a) : 0.0;\n"
    "#else\n"
    "    return 0.0;\n"
    "#endif\n"
    "}\n"
    "void main() {\n"
    "    vec4 s = gl_Vertex;\n"
    "    vec4 t = gl_MultiTexCoord0;\n"
    "    vec2 c = t.xy;\n"
    "    float idx = floor(s.w);\n"
    "    float sw = s.w - idx;\n"
    /* the copy of this flake's landing point nearest the view's centre */
    "    vec2 base = s.xz * uPeriod;\n"
    "    vec2 gp = base + floor((uCentre - base) / uPeriod + 0.5) * uPeriod;\n"
    /* one of four speeds, so the snow does not fall as one sheet; each its own cycle of
       fall and rest, with the clock folded by that cycle. The fall runs against the
       unzoomed height, so a zoom cannot scrub it. ph is the fall's progress, held at 1
       once down; rest is the seconds it has lain there */
    "    vec4 pick = vec4(equal(vec4(floor(sw * 4.0)), vec4(0.0, 1.0, 2.0, 3.0)));\n"
    "    float cyc = dot(uCycles, pick);\n"
    "    float fall = dot(uFalls, pick);\n"
    "    float tc = fract(s.y + dot(uTimes, pick) / cyc) * cyc;\n"
    "    float ph = min(tc / fall, 1.0);\n"
    "    float rest = max(tc - fall, 0.0);\n"
    "    float y = uHeight * (1.0 - ph);\n"
    /* the wind: carried sideways by the height still to fall, so it lands on gp */
    "    vec2 p = gp - uDrift * y;\n"
    /* the flutter: a whole number of small turns per fall, each flake its own size,
       axis and starting point on the turn. Held at the end of its last turn while the
       flake rests, so it lies exactly where it landed */
    "    float k = max(floor(uFlutter * (2.0 + 4.0 * t.w) + 0.5), 1.0);\n"
    "    float th = 6.2831853 * (k * ph + t.w * 3.7);\n"
    "    float th1 = 6.2831853 * (k + t.w * 3.7);\n"
    "    float ax = 6.2831853 * fract(t.w * 5.3 + s.x * 2.1);\n"
    "    vec2 dir = vec2(cos(ax), sin(ax));\n"
    "    vec2 side = vec2(-dir.y, dir.x);\n"
    "    float amp = uSway * (0.5 + fract(t.z * 7.31 + s.y * 3.3));\n"
    "    p += amp * (dir * sin(th) + side * (0.45 * cos(th)));\n"
    /* WHERE IT COMES TO REST: the same expression at the end of the fall. The whole fall
       stands on the ground there, so it is the dial's length over a hill as over a
       valley, and it ends on the ground */
    "    vec2 land = gp + amp * (dir * sin(th1) + side * (0.45 * cos(th1)));\n"
    "    float gy = ground(land);\n"
    /* mostly small, a few large: the square of a seed */
    "    float size = uSize * (0.55 + 0.95 * t.z * t.z);\n"
    /* lifted by its own radius (half the size, see r below), so a resting flake sits on
       the ground rather than in it */
    "    vec3 centre = vec3(p.x, gy + 0.5 * size + y, p.y);\n"
    /* THE MAP'S EDGE. Beyond the play rectangle there is no ground, only the clear
       colour, so the snow fades out over the last cell and a half. What matters is the
       ground SEEN BEHIND the flake, not the ground under it: a flake a few cells up
       projects several cells further up the screen than its landing point, and near
       the far edge that is over the void. So the eye's ray through the flake is carried
       on to the ground plane, and the nearest of that point, the flake's own column and
       its landing point to the edge decides the fade. */
    "    float dy = max(uCamPos.y - centre.y, 0.05);\n"
    "    vec2 behind = centre.xz + (centre.xz - uCamPos.xz) * ((centre.y - gy) / dy);\n"
    "    float inMap = smoothstep(0.0, 1.5, min(inside(behind), min(inside(p), inside(gp))));\n"
    "    vec4 c0 = gl_ModelViewProjectionMatrix * vec4(centre, 1.0);\n"
    "    vec4 c1 = gl_ModelViewProjectionMatrix * vec4(centre + uUp * size, 1.0);\n"
    "    vec3 toEye = uCamPos - centre;\n"
    "    float d = length(toEye);\n"
    "    vec4 cb = gl_ModelViewProjectionMatrix * vec4(centre + toEye * (size / max(d, 0.001)), 1.0);\n"
    /* AT REST it fades out over the settle time, smoothly from full to nothing; open
       water does not hold a flake, so there it is gone within a third of a second. The
       field that says where the water is is the one the light pass masks the wet term
       with, asked at the resting point */
    "    float wet = wetAt(land);\n"
    "    float settle = max(mix(uSettle, uSettleWater, wet), 0.0001);\n"
    "    float lie = 1.0 - smoothstep(0.0, 1.0, rest / settle);\n"
    /* fade in at the top of the volume (the respawn), out as it settles, out close to
       the eye, a little fainter toward the far edge of the view, and in or out one by
       one at the end of the drawn range. With no rest at all (uEndFade 1) it fades over
       the last tenth of the fall instead, as it did before a flake could rest */
    "    vFade = smoothstep(0.0, 0.08, ph) * (1.0 - uEndFade * smoothstep(0.90, 1.0, ph)) * lie\n"
    "          * smoothstep(uNear, uNear * 2.5, d) * inMap\n"
    "          * (1.0 - 0.45 * smoothstep(0.85, 1.6, d / uLook))\n"
    "          * clamp((uCount - idx) / 64.0, 0.0, 1.0);\n"
    "    vBright = 0.82 + 0.18 * fract(t.z * 13.7 + sw * 5.1);\n"
    "    vUV = c;\n"
    "    if (c0.w <= 0.05 || c1.w <= 0.05 || cb.w <= 0.05 || vFade <= 0.002)\n"
    "        { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); vFade = 0.0; return; }\n"
    /* the radius in pixels, with its floor: alpha pays for what the floor added */
    "    float r = 0.5 * length((c1.xy / c1.w - c0.xy / c0.w) * uHalfVP);\n"
    "    if (r < uMinPx) { vFade *= (r * r) / (uMinPx * uMinPx); r = uMinPx; }\n"
    /* and a ceiling: a flake brushing the lens would otherwise fill a tenth of the
       screen with a grey disc */
    "    r = min(r, uMaxPx);\n"
    "    gl_Position = c0 + vec4(c * (r / uHalfVP) * c0.w, 0.0, 0.0);\n"
    /* the depth up to a flake's width nearer the eye: the sprite is flat across the view,
       and on a slope that rises toward the camera the ground in front of a resting
       flake's lower half would otherwise be nearer than the flake's centre. Only near
       the ground, easing in over the last two flake widths of the fall and whole at
       rest: a flake in the air keeps its own depth, so it does not draw in front of the
       edge of a unit or a building it is just behind */
    "    float pull = clamp(1.0 - y / (2.0 * size), 0.0, 1.0);\n"
    "    gl_Position.z = mix(c0.z / c0.w, cb.z / cb.w, pull) * c0.w;\n"
    "}\n";

/* A soft round flake: a solid core a third of its radius across, easing out to nothing
   at the rim, so there is no hard edge to alias and a small flake still has a centre. */
static const char* SNOW_FS =
    "#version 120\n"
    "varying vec2 vUV; varying float vFade; varying float vBright; uniform vec4 uColor;\n"
    "void main() {\n"
    "    float a = 1.0 - smoothstep(0.30, 1.0, length(vUV));\n"
    "    gl_FragColor = vec4(uColor.rgb * vBright, uColor.a * a * vFade);\n"
    "}\n";

/* The lattice, built once. A fixed LCG rather than rand(): the seeds must be the same
   on every machine and every run, and rand() promises neither. Every flake's seeds are
   independent, so the first n of the lattice are as evenly spread as all of it, which
   is what lets the drawn count change without moving a flake. */
static void snow_boot(void)
{
    if (!g_snowVerts.empty() || g_snowBuf) return;
    g_snowVerts.resize((size_t)SNOW_LATTICE * 4 * 8);
    unsigned lcg = 0x5EED5A0Du;
    static const float cx[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
    static const float cy[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
    for (int i = 0; i < SNOW_LATTICE; i++) {
        float s[6];
        for (int k = 0; k < 6; k++) {
            lcg = lcg * 1664525u + 1013904223u;
            s[k] = (float)(lcg >> 8) * (1.0f / 16777216.0f);
        }
        /* the index rides in w beside the fourth seed; a float holds both exactly
           while the index stays far below 2^24 */
        const float w = (float)i + s[3] * 0.99f;
        for (int v = 0; v < 4; v++) {
            float* p = &g_snowVerts[((size_t)i * 4 + v) * 8];
            p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = w;
            p[4] = cx[v]; p[5] = cy[v]; p[6] = s[4]; p[7] = s[5];
        }
    }
    /* Into a static buffer once, so no frame sends it again. A driver without buffer
       objects, or one that made a store of the wrong size, keeps the client array, which
       draws the same picture and sends the lattice each frame. */
    if (!g_snowBufTried) {
        g_snowBufTried = 1;
        if (grass_buf_load()) {
            const FxGLsizeiptr bytes = (FxGLsizeiptr)(g_snowVerts.size() * sizeof(float));
            GLint got = 0;
            gr_glGenBuffers(1, &g_snowBuf);
            gr_glBindBuffer(GL_ARRAY_BUFFER, g_snowBuf);
            gr_glBufferData(GL_ARRAY_BUFFER, bytes, &g_snowVerts[0], GL_STATIC_DRAW);
            gr_glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &got);
            gr_glBindBuffer(GL_ARRAY_BUFFER, 0);
            if ((FxGLsizeiptr)got != bytes) {
                gr_glDeleteBuffers(1, &g_snowBuf);
                g_snowBuf = 0;
            } else {
                std::vector<float>().swap(g_snowVerts);
            }
        }
        fprintf(stderr, "SNOW|lattice|%d flakes|%s\n", SNOW_LATTICE,
                g_snowBuf ? "static buffer" : "client array");
    }
}

/* THE TIGHT PERIOD for a view at eye distance `look`: the lattice that just covers it.
   The visible ground is about 2.7 to 3.2 times the eye's distance across on a 16:9
   window (the larger figure zoomed in, where the camera tilts down further), and wider
   in proportion on a wider one; 2.15 times the distance per unit of aspect is 3.8 on
   16:9, which keeps the re-tiling, half a period from the middle, off screen. The wind's
   and the flutter's reach are added, and it is never so small that the period shows. */
static float snow_zoom(float look)
{
    /* scaled against the standard view's distance, 13.3 cells */
    float zoom = look / 13.3f;
    if (zoom < 0.3f) zoom = 0.3f;
    if (zoom > 3.0f) zoom = 3.0f;
    return zoom;
}
/* THE VOLUME STAYS BELOW THE EYE. Zoomed in, the eye is only five cells above the ground,
   and a volume taller than that spends most of its flakes behind the camera or so close
   to it that they fade out. So the dial is the height at the standard zoom, the drawn
   height follows the zoom, and it never reaches past 0.45 of the eye's distance. */
static float snow_height_at(float look)
{
    float h = snow_ref_height() * snow_zoom(look);
    if (h > 0.45f * look) h = 0.45f * look;
    if (h < 0.5f) h = 0.5f;
    return h;
}
static float snow_period_at(float look, float aspect, float drift, float sway)
{
    float period = look * 2.15f * aspect + 2.0f * snow_height_at(look) * drift + 3.0f * sway + 2.0f;
    return period < 8.0f ? 8.0f : period;
}

/* THE GROUND UNDER THE SNOW, as a texture the vertex shader can read. terrain_y is
   sampled SNOW_GROUND_RES times a cell over the play rectangle (the snow fades out at
   its edge, and the texture's edge texels carry on beyond it) and stored as 16 bits
   over the range the samples cover. Rebuilt only when something terrain_y reads has
   moved: a new pack or ground generation, a building pad, the relief surface, the
   water plane, the terrain's base or the play rectangle. Measured: 45,000 samples on
   Soviet 1's snow map (74 by 38 cells) take under a millisecond, the 96 by 96 skirmish
   map about two. Linear filtering between samples a quarter cell apart follows the
   relief closely: over 200,000 random points the drawn ground stood above the texture
   by more than 0.02 cells at under 1 percent of them and by at most 0.12, on the
   sharpest rock, which a resting flake's lift and depth pull mostly cover.

   A PAD REDOES ONLY ITS CELLS. A building going down moves the pad epoch, and usually
   the relief around the pad with it; the ground's revision stamps (g_groundRev, issued
   by ground_rev_sync on exactly those frames) say which cells could have moved, so only
   their texels are sampled again and only the rows they lie in are sent. The whole
   texture is rebuilt instead when anything else in the key moved, when the stamps are
   not current or say "everything" (g_groundWholeSeq), when a new height falls outside
   the range the 16 bits were spread over, or when the relief moved on a frame the pads
   did not: the stamps owe such a move until the next pad change, so snow_ground_watch
   notes it every frame, drawn or not. */
static void snow_ground_watch(void)
{
    const unsigned pad = g_padEpoch, surf = terrain_surface_generation();
    if (surf != g_snowWatchSurf && pad == g_snowWatchPad) g_snowGroundLoose = 1;
    g_snowWatchPad = pad;
    g_snowWatchSurf = surf;
}
static void snow_ground_send(int j0, int j1)
{
    const int w = g_snowGroundW;
    /* on unit 1, where the draw binds it, so unit 0's binding is left as it was */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_snowGroundTex);
    GLint align = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, j0, w, j1 - j0 + 1, GL_LUMINANCE, GL_UNSIGNED_SHORT,
                    &g_snowGroundPx[(size_t)j0 * w]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, align);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
}
/* the cells a pad moved, or 0 when the whole texture has to be rebuilt */
static int snow_ground_patch(const unsigned* key)
{
    for (int k = 0; k < 12; k++)
        if (k != 6 && k != 8 && key[k] != g_snowGroundKey[k]) return 0;
    if (key[6] == g_snowGroundKey[6] || g_snowGroundLoose || !g_snowGroundOk) return 0;
    if (!ground_rev_current() || g_groundWholeSeq > g_snowGroundSeq) return 0;
    const int R = SNOW_GROUND_RES, w = g_snowGroundW, h = g_snowGroundH;
    if (g_snowGroundPx.size() != (size_t)w * h || w != g_mapW * R + 1 || h != g_mapH * R + 1) return 0;
    const Uint64 t0 = SDL_GetPerformanceCounter();
    const float lo = g_snowGroundLo, span = g_snowGroundSpan;
    int cells = 0, texels = 0, j0 = h, j1 = -1;
    /* a texel on a cell's edge is redone when either cell beside it was stamped, so the
       scan reaches one cell past the play rectangle on every side */
    for (int z = g_mapY - 1; z <= g_mapY + g_mapH; z++) {
        if (z < 0 || z >= g_gridH) continue;
        for (int x = g_mapX - 1; x <= g_mapX + g_mapW; x++) {
            if (x < 0 || x >= g_gridW) continue;
            if (g_groundRev[(size_t)z * g_gridW + x] <= g_snowGroundSeq) continue;
            cells++;
            const int i0 = (x - g_mapX) * R, k0 = (z - g_mapY) * R;
            for (int j = k0 < 0 ? 0 : k0; j <= k0 + R && j < h; j++)
                for (int i = i0 < 0 ? 0 : i0; i <= i0 + R && i < w; i++) {
                    const float v = terrain_y((float)g_mapX + (float)i / (float)R,
                                              (float)g_mapY + (float)j / (float)R);
                    const float q = (v - lo) / span * 65535.0f + 0.5f;
                    if (q < 0.0f || q > 65535.5f) return 0;   /* out of range: whole */
                    g_snowGroundPx[(size_t)j * w + i] = (unsigned short)(q > 65535.0f ? 65535.0f : q);
                    texels++;
                    if (j < j0) j0 = j;
                    if (j > j1) j1 = j;
                }
        }
    }
    if (j1 >= j0) snow_ground_send(j0, j1);
    g_snowGroundSeq = g_groundSeq;
    const double ms = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0
                    / (double)SDL_GetPerformanceFrequency();
    fprintf(stderr, "SNOW|ground|patched|cells=%d|texels=%d|rows=%d|ms=%.2f\n",
            cells, texels, j1 >= j0 ? j1 - j0 + 1 : 0, ms);
    if (ground_patch_verify()) {
        /* OPENCNC3D_GROUND_PATCH_VERIFY: every texel sampled again, so a cell the stamps
           missed shows as a texel that differs from what the patch left */
        int bad = 0, worst = 0;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                const float v = terrain_y((float)g_mapX + (float)i / (float)R,
                                          (float)g_mapY + (float)j / (float)R);
                float q = (v - lo) / span * 65535.0f + 0.5f;
                q = q < 0.0f ? 0.0f : (q > 65535.0f ? 65535.0f : q);
                const int d = abs((int)(unsigned short)q - (int)g_snowGroundPx[(size_t)j * w + i]);
                if (d > 0) bad++;
                if (d > worst) worst = d;
            }
        fprintf(stderr, "SNOW|ground|verify|texels=%d|differ=%d|worst=%d\n", w * h, bad, worst);
    }
    return 1;
}
/* Returns whether the texture can be used. */
static int snow_ground_sync(void)
{
    if (g_snowVtu <= 0 || g_mapW < 1 || g_mapH < 1) return 0;
    unsigned key[12];
    const float tb = g_terrainBase;
    key[0] = (unsigned)g_gridW; key[1] = (unsigned)g_gridH;
    key[2] = (unsigned)g_mapX;  key[3] = (unsigned)g_mapY;
    key[4] = (unsigned)g_mapW;  key[5] = (unsigned)g_mapH;
    key[6] = g_padEpoch;        key[7] = g_groundGen;
    key[8] = terrain_surface_generation();
    key[9] = water_plane_epoch();
    memcpy(&key[10], &tb, sizeof key[10]);
    key[11] = ra3d_flat_ground() ? 1u : 0u;
    if (g_snowGroundTex && !memcmp(key, g_snowGroundKey, sizeof key)) return g_snowGroundOk;
    if (g_snowGroundTex && snow_ground_patch(key)) {
        memcpy(g_snowGroundKey, key, sizeof key);
        return g_snowGroundOk;
    }
    memcpy(g_snowGroundKey, key, sizeof key);

    const Uint64 t0 = SDL_GetPerformanceCounter();
    const int R = SNOW_GROUND_RES;
    const int w = g_mapW * R + 1, h = g_mapH * R + 1;
    std::vector<float> hv((size_t)w * h);
    float lo = 1e30f, hi = -1e30f;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            const float v = terrain_y((float)g_mapX + (float)i / (float)R,
                                      (float)g_mapY + (float)j / (float)R);
            hv[(size_t)j * w + i] = v;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    float span = hi - lo;
    if (span < 0.01f) span = 0.01f;
    std::vector<unsigned short>& px = g_snowGroundPx;
    px.resize(hv.size());
    for (size_t i = 0; i < hv.size(); i++) {
        float q = (hv[i] - lo) / span * 65535.0f + 0.5f;
        px[i] = (unsigned short)(q < 0.0f ? 0.0f : (q > 65535.0f ? 65535.0f : q));
    }
    /* on unit 1, where the draw binds it, so unit 0's binding is left as it was */
    glActiveTexture(GL_TEXTURE1);
    if (!g_snowGroundTex) glGenTextures(1, &g_snowGroundTex);
    glBindTexture(GL_TEXTURE_2D, g_snowGroundTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLint align = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    if (w == g_snowGroundW && h == g_snowGroundH)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_LUMINANCE, GL_UNSIGNED_SHORT, &px[0]);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE16, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, &px[0]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, align);
    GLint bits = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_LUMINANCE_SIZE, &bits);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    g_snowGroundW = w; g_snowGroundH = h;
    g_snowGroundSeq = g_groundSeq;
    g_snowGroundLoose = 0;
    /* u = (x - mapX) * R / w + 0.5 / w, the centre of the texel on the sample */
    g_snowGroundUV[0] = (float)R / (float)w;
    g_snowGroundUV[1] = (0.5f - (float)g_mapX * (float)R) / (float)w;
    g_snowGroundUV[2] = (float)R / (float)h;
    g_snowGroundUV[3] = (0.5f - (float)g_mapY * (float)R) / (float)h;
    g_snowGroundLo = lo;
    g_snowGroundSpan = span;
    /* eight bits would still place a flake to a hundredth or two of a cell over the
       few cells a map's heights span, so a driver that keeps fewer is used as it is */
    g_snowGroundOk = bits > 0 ? 1 : 0;
    const double ms = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0
                    / (double)SDL_GetPerformanceFrequency();
    fprintf(stderr, "SNOW|ground|%dx%d|res=%d|range=%.3f..%.3f|bits=%d|ms=%.2f\n",
            w, h, R, lo, lo + span, (int)bits, ms);
    return g_snowGroundOk;
}

/* Called from draw_frame with the rain, after the combat effects and before the shroud
   blanket, in the world's own projection. Draws nothing unless the snow is live. */
/* The program, linked once. Vertex texture units are asked first, and the reads this GL
   cannot do are left out of the shader (SNOW_GROUND needs one unit, SNOW_WATER a second),
   so a GL without them still draws the snow, ending at the terrain's zero. */
static GLuint snow_link(void)
{
    GLint u = 0;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &u);
    g_snowVtu = (int)u;
    if (g_snowVtu < 2)
        fprintf(stderr, "SNOW|ground|this GL has %d vertex texture units: %s\n", g_snowVtu,
                g_snowVtu <= 0 ? "the snow ends at the terrain's zero" : "the ground but no water test");
    std::string vs = "#version 120\n";
    vs += g_snowVtu >= 1 ? "#define SNOW_GROUND 1\n" : "#define SNOW_GROUND 0\n";
    vs += g_snowVtu >= 2 ? "#define SNOW_WATER 1\n" : "#define SNOW_WATER 0\n";
    vs += SNOW_VS_BODY;
    return rain_link(vs.c_str(), SNOW_FS, "snow");
}

static void snow_draw(void)
{
    /* every frame, drawn or not, so a relief move between pad changes is never missed */
    snow_ground_watch();
    const int live = snow_live();
    if (g_snowLogged != live) {
        g_snowLogged = live;
        fprintf(stderr, "SNOW|%s|theater=%s|amount=%.2f|size=%.3f|speed=%.2f\n",
                live ? "on" : "off", g_pack.theater, g_fx.snow_amount, g_fx.snow_size,
                g_fx.snow_speed);
    }
    if (!live) return;
    if (!g_snowProg && !g_snowProgTried) {
        g_snowProgTried = 1;
        g_snowProg = snow_link();
    }
    if (!g_snowProg) return;
    snow_boot();

    GLint vp[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] < 1 || vp[3] < 1) return;

    /* THE ZOOM IS THE DISTANCE FROM THE EYE TO THE GROUND IT LOOKS AT. The camera rides
       the terrain, so the visible rectangle grows and shrinks by a cell or so as the view
       crosses a rise, while the eye-to-ground distance is the rig's own zoom and holds
       still under a pan. */
    const float ty = terrain_y(g_camX, g_camZ);
    const float lx = g_fxCamPos[0] - g_camX, ly = g_fxCamPos[1] - ty, lz = g_fxCamPos[2] - g_camZ;
    float look = sqrtf(lx * lx + ly * ly + lz * lz);
    if (look < 1.0f) look = 1.0f;
    const float zoom = snow_zoom(look);
    const float height = snow_height_at(look);

    float aspect = (float)vp[2] / (float)vp[3];
    if (aspect < 1.0f) aspect = 1.0f;
    const float drift = g_fx.snow_drift > 0.0f ? g_fx.snow_drift : 0.0f;
    const float sway = g_fx.snow_sway > 0.0f ? g_fx.snow_sway : 0.0f;
    /* THE PERIOD IS FIXED for every zoom the rig allows: the tight period of its
       farthest view, with a tenth more for the terrain under the camera. Only a camera
       pulled back beyond that (none the player drives) grows it again. */
    const float lookFar = 1.1f * DIST_MAX_OURS / 256.0f;
    const float period = snow_period_at(look > lookFar ? look : lookFar, aspect, drift, sway);
    /* HOW MANY: a square law of the dial in a tight lattice, so the bottom of the dial is
       a few flakes drifting rather than a thin storm, then as many more as the fixed
       period's extra area needs to hold that density over the view */
    float dens = g_fx.snow_amount;
    if (dens > 1.0f) dens = 1.0f;
    const float tight = snow_period_at(look, aspect, drift, sway);
    float count = (float)SNOW_DENSE * dens * dens * (period / tight) * (period / tight);
    {
        /* the rest the lattice can hold with this many in the air at the rig's nearest
           zoom (snow_settle_held); the rig's distance is in leptons, 256 a cell */
        const float lookNear = dist_min_now() / 256.0f;
        const float tightNear = snow_period_at(lookNear, aspect, drift, sway);
        const float need = (float)SNOW_DENSE * dens * dens * (period / tightNear) * (period / tightNear);
        g_snowSettle = snow_settle_held(need, (float)(SNOW_LATTICE - SNOW_COUNT_RAMP));
        if (g_snowSettle != g_snowSettleLogged) {
            g_snowSettleLogged = g_snowSettle;
            fprintf(stderr, "SNOW|settle|dial=%.2f|held=%.2f|nearest=%.0f of %d\n",
                    snow_settle_dial(), g_snowSettle, need / snow_air_share(g_snowSettle) + 0.5f * SNOW_COUNT_RAMP,
                    SNOW_LATTICE);
        }
    }
    /* and as many more as the flakes lying on the ground take out of the air */
    count /= snow_air_share(g_snowSettle);
    if (count < 0.5f) return;
    count += 0.5f * (float)SNOW_COUNT_RAMP;   /* the ramp's half below the count */
    if (count > (float)SNOW_LATTICE) count = (float)SNOW_LATTICE;
    int n = (int)ceilf(count);
    if (n > SNOW_LATTICE) n = SNOW_LATTICE;

    const float cx = 0.5f * (g_viewX0 + g_viewX1), cz = 0.5f * (g_viewZ0 + g_viewZ1);
    /* the wind: a compass bearing, read the same way the rain, the clouds and the
       current read theirs */
    const float rad = g_fx.snow_wind * (3.14159265358979f / 180.0f);
    /* THE FLOOR is a pixel and a half of the scene target, the grid the anti-aliasing
       jitters on. THE CEILING is a size on the display, so it counts window points:
       on a Retina backing a point is two pixels, and a ceiling in pixels would clip the
       nearest flakes to half their size. */
    float ss = (g_fxNativeA.h > 0 && g_fxScene.h > 0) ? (float)g_fxScene.h / (float)g_fxNativeA.h : 1.0f;
    if (ss < 1.0f) ss = 1.0f;
    const float pt = g_fxHidpi > 1 ? (float)g_fxHidpi : 1.0f;

    fx_glUseProgram(g_snowProg);
    fx_set2f(g_snowProg, "uCentre", cx, cz);
    fx_set1f(g_snowProg, "uPeriod", period);
    fx_set1f(g_snowProg, "uHeight", height);
    fx_set1f(g_snowProg, "uCount", count);
    {
        /* each speed's fall, its cycle of fall and rest, and the clock folded by it */
        GLint l = fx_glGetUniformLocation(g_snowProg, "uFalls");
        if (l >= 0) fx_glUniform4f(l, snow_fall_secs(0), snow_fall_secs(1), snow_fall_secs(2), snow_fall_secs(3));
        l = fx_glGetUniformLocation(g_snowProg, "uCycles");
        if (l >= 0) fx_glUniform4f(l, snow_cycle_secs(0), snow_cycle_secs(1), snow_cycle_secs(2), snow_cycle_secs(3));
        l = fx_glGetUniformLocation(g_snowProg, "uTimes");
        if (l >= 0) fx_glUniform4f(l, snow_time(0), snow_time(1), snow_time(2), snow_time(3));
    }
    {
        /* the rest: the dial on the ground, a third of a second at most on open water,
           and with no rest at all the old fade over the last tenth of the fall, reached
           smoothly as the dial goes below a quarter of a second */
        const float settle = g_snowSettle;
        float endFade = 1.0f - settle / 0.25f;
        if (endFade < 0.0f) endFade = 0.0f;
        fx_set1f(g_snowProg, "uSettle", settle);
        fx_set1f(g_snowProg, "uSettleWater", settle < 0.3f ? settle : 0.3f);
        fx_set1f(g_snowProg, "uEndFade", endFade);
    }
    /* the ground's height and the water field, read in the vertex shader (units 1 and 2;
       the snow uses no texture of its own on unit 0) */
    const int groundOn = snow_ground_sync();
    /* The field is the coastline of the map's water cells, built from the cut whichever
       sea draws it, and the light pass keeps it current with or without the Enhanced
       sea; so the test does not ask for the sea, only for this map's field. */
    const int waterOn = (g_snowVtu >= 2 && g_waterFieldTex && g_gridW > 0 && g_gridH > 0
                         && !strcmp(g_waterFieldScen, g_pack.scen)) ? 1 : 0;
    if (waterOn != g_snowWaterLogged) {
        g_snowWaterLogged = waterOn;
        fprintf(stderr, "SNOW|water|%s\n", waterOn ? "field" : "none");
    }
    fx_set1i(g_snowProg, "uGround", 1);
    fx_set1f(g_snowProg, "uGroundOn", groundOn ? 1.0f : 0.0f);
    fx_set2f(g_snowProg, "uGroundRange", g_snowGroundLo, g_snowGroundSpan);
    {
        GLint l = fx_glGetUniformLocation(g_snowProg, "uGroundUV");
        if (l >= 0) fx_glUniform4f(l, g_snowGroundUV[0], g_snowGroundUV[1], g_snowGroundUV[2], g_snowGroundUV[3]);
    }
    fx_set1i(g_snowProg, "uWater", 2);
    fx_set1f(g_snowProg, "uWaterOn", waterOn ? 1.0f : 0.0f);
    fx_set2f(g_snowProg, "uWaterScale", g_gridW > 0 ? 1.0f / (float)g_gridW : 0.0f,
                                        g_gridH > 0 ? 1.0f / (float)g_gridH : 0.0f);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, groundOn ? g_snowGroundTex : 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, waterOn ? g_waterFieldTex : 0);
    glActiveTexture(GL_TEXTURE0);
    fx_set2f(g_snowProg, "uDrift", sinf(rad) * drift, -cosf(rad) * drift);
    fx_set1f(g_snowProg, "uSway", sway);
    fx_set1f(g_snowProg, "uFlutter", g_fx.snow_flutter > 0.1f ? g_fx.snow_flutter : 0.1f);
    fx_set1f(g_snowProg, "uSize", g_fx.snow_size * zoom);
    fx_set1f(g_snowProg, "uMinPx", 0.75f * ss);
    fx_set1f(g_snowProg, "uMaxPx", 9.0f * ss * pt);
    fx_set2f(g_snowProg, "uHalfVP", 0.5f * (float)vp[2], 0.5f * (float)vp[3]);
    fx_set3f(g_snowProg, "uUp", g_bbUp[0], g_bbUp[1], g_bbUp[2]);
    fx_set3f(g_snowProg, "uCamPos", g_fxCamPos[0], g_fxCamPos[1], g_fxCamPos[2]);
    /* the near fade follows the zoom as well: a fixed cell and a half, right for the
       rain at the standard view, emptied the bottom of the screen zoomed in, where the
       ground nearest the camera is only a few cells from the eye */
    fx_set1f(g_snowProg, "uNear", 0.10f * look);
    fx_set1f(g_snowProg, "uLook", look);
    {
        /* the play rectangle inset by its rim ring, which is always shroud */
        GLint l = fx_glGetUniformLocation(g_snowProg, "uRect");
        if (l >= 0) fx_glUniform4f(l, (float)(g_mapX + 1), (float)(g_mapY + 1),
                                   (float)(g_mapX + g_mapW - 1), (float)(g_mapY + g_mapH - 1));
    }
    {
        /* white with the faintest cool cast, a little below 1 so the light pass's lift on
           a sunward slope takes a flake's core to white at most, not into the bloom */
        float a = g_fx.snow_opacity;
        if (a > 1.0f) a = 1.0f;
        GLint l = fx_glGetUniformLocation(g_snowProg, "uColor");
        if (l >= 0) fx_glUniform4f(l, 0.90f, 0.92f, 0.95f, a);
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    const GLsizei stride = 8 * (GLsizei)sizeof(float);
    const char* src = 0;
    if (g_snowBuf) gr_glBindBuffer(GL_ARRAY_BUFFER, g_snowBuf);
    else           src = (const char*)&g_snowVerts[0];
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(4, GL_FLOAT, stride, src);
    glTexCoordPointer(4, GL_FLOAT, stride, src + 4 * sizeof(float));
    glDrawArrays(GL_QUADS, 0, n * 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    /* every other array in this renderer is a client array: nothing may find a buffer
       bound behind it */
    if (g_snowBuf) gr_glBindBuffer(GL_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    fx_glUseProgram(0);
    /* exactly the state the effects pass left: depth writes on, blend off, white */
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

#endif /* OPENCNC3D_SNOW_MOD_H */
