#ifndef OPENCNC3D_INFANTRY_AUDIO_MATH_H
#define OPENCNC3D_INFANTRY_AUDIO_MATH_H
#include "ambience_math.h"
#include <string.h>
enum { INFANTRY_AUDIO_VARIANTS = 10 };
static int infantry_audio_gain(float zoom, float distance, int volume);
static float infantry_audio_zoom_gain(float zoom)
{
    return 0.25f + 0.75f * ambience_clamp((zoom - 0.5f) * 2.0f);
}
/* Keep the step's random volume factor across camera/attenuation retargets. */
static int infantry_audio_gain(float zoom, float distance, int volume)
{
    return (int)(volume * infantry_audio_zoom_gain(zoom) * distance + 0.5f);
}
static bool infantry_audio_is_soldier(const char* type)
{
    return (type[0]=='E' && type[1]>='1' && type[1]<='9' && type[2]==0)
        || !strcmp(type,"E1TR") || !strcmp(type,"RMBO")
        || !strcmp(type,"MEDI") || !strcmp(type,"MECH")
        || !strcmp(type,"SHOK") || !strcmp(type,"SNIPER");
}
#endif
