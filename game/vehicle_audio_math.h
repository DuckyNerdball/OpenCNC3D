#ifndef OPENCNC3D_VEHICLE_AUDIO_MATH_H
#define OPENCNC3D_VEHICLE_AUDIO_MATH_H
#include <string.h>
#include "ambience_math.h"
/* Runtime type names from TD udata.cpp. JEEP is the GDI Humvee/Jeep. */
static int vehicle_audio_clip_index(const char* type)
{
    if (!strcmp(type, "MTNK") || !strcmp(type, "HTNK") || !strcmp(type, "FTNK") ||
        !strcmp(type, "MLRS") || !strcmp(type, "MSAM")) return 9;
    if (!strcmp(type, "LTNK") || !strcmp(type, "STNK") || !strcmp(type, "ARTY") || !strcmp(type, "APC")) return 10;
    if (!strcmp(type, "HARV") || !strcmp(type, "JEEP")) return 11;
    return -1;
}
/* Private audio RNG: never advance the simulation's random state. Seeded once by
   the renderer clock; successive driving starts draw fresh whole-frame offsets. */
static long vehicle_audio_random_offset(unsigned& state, long frames)
{
    if (frames <= 0) return 0;
    if (!state) state = 0x6d2b79f5u;
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return (long)(state % (unsigned long)frames);
}
static float vehicle_audio_zoom_gain(float zoom)
{
    return 0.25f + 0.75f * ambience_clamp(zoom);
}
static int vehicle_audio_screen_pan(float col, float width)
{
    if (width <= 0) return 0;
    return (int)((2.0f * ambience_clamp(col / width) - 1.0f) * 1000);
}
#endif
