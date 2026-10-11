#ifndef OPENCNC3D_CINEMATIC_AUDIO_MATH_H
#define OPENCNC3D_CINEMATIC_AUDIO_MATH_H
#include "ambience_math.h"
/* Same timeline/envelope as eplc_rumble, driven by each placement's live rig. */
static float cinematic_audio_rise(float t, float r0, float r1, float out)
{
    if (r0<0 || r1<=r0) return 0;
    if (t<r1) { float k=ambience_clamp((t-r0)/(r1-r0)); return k*k*(3-2*k); }
    return ambience_clamp(1-(t-r1)/(out>0?out:.35f));
}
/* The rumble follows the camera's: its timing is the rig's envelope, and its strength is
   the placement's rumble strength measured against the shared preset (0.006 cells per
   footprint cell), so a type tuned to shake harder also roars harder. The footprint sets
   the weight, Guard Tower 0.55 to Construction Yard 1.0, rather than the shake's own
   linear cell count, which would leave a one-cell building nearly silent. */
/* The rumble recording fades in over its first fifth and out over its last, and a large
   footprint's rumble outlasts it. It loops over the middle three fifths, each pass running
   into the next through an equal-power crossfade a quarter of that middle long. */
static void cinematic_audio_rumble_body(long frames, long* start, long* end, long* xfade)
{
    *start = frames / 5;
    *end = frames - frames / 5;
    *xfade = (*end - *start) / 4;
}
#define CIN_RUMBLE_PRESET 0.006f
static float cinematic_audio_rumble_level(float strength, float mass, float envelope)
{
    if (strength <= 0) return 0;
    /* capped at 1: a full-strength rumble already peaks just under full scale */
    return ambience_clamp(strength / CIN_RUMBLE_PRESET * (.55f + .45f * ambience_clamp((mass - 1) / 3)))
           * ambience_clamp(envelope);
}
/* Per mille gain of an impact before distance. A placement is the heavy one: 800 for a
   one-cell footprint up to 1100 for four cells, where its -1 dBFS peak just reaches full
   scale; the destruction layers keep their level. */
static int cinematic_audio_impact_gain(int place, float mass)
{
    if (place) return (int)(1000 * (.80f + .30f * ambience_clamp((mass - 1) / 3)));
    return (int)(1000 * (.60f + .40f * ambience_clamp(mass / 4)));
}
/* How far the rest of the mix is ducked under a cinematic layer, per mille of unity:
   `level` is the layer's own sounding level (gain x distance, 1 = unity) and `fx` the
   Sound Volume bus, so a muted or distant layer ducks nothing. */
static int cinematic_audio_duck_depth(float amount, float level, int fx)
{
    return 1000 - (int)(1000 * amount * ambience_clamp(level) * ambience_clamp(fx / 1000.0f));
}
static float cinematic_audio_distance(float distance)
{
    return ambience_clamp(1-distance/12);
}
#endif
