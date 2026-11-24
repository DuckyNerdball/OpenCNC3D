#ifndef OPENCNC3D_AMBIENCE_MATH_H
#define OPENCNC3D_AMBIENCE_MATH_H
#include <math.h>
#include <algorithm>
#include <vector>
/* zoom is a fraction: 1 closest, 0 furthest. Read camera limits at every update. */
static float ambience_clamp(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float ambience_zoom_fraction(float value, float near_limit, float far_limit)
{
    return near_limit == far_limit ? 1.0f :
        ambience_clamp((value - far_limit) / (near_limit - far_limit));
}
/* Where the zoom sits between the near bed (1) and the far bed (0). Callers turn it
   into equal-power gains, sin and cos of mix * pi/2, so the sum stays level.

   From 1 to 0.5 it is all near, and from 0.5 to 0.375 a straight line down to 0.5,
   the crossover where the two beds sit equal. Below the crossover the near bed does
   NOT stop at 0.25 any more: it keeps fading to the furthest zoom, evenly in decibels.
   The near and far recordings differ most in their top end (insects, birds and air
   against a dark wind), so when the line ran on to 0 at 0.25 the near bed fell from
   -10 dB to -31 dB in the last 100 leptons, one wheel step of the N64 camera, and from
   -16 dB to silence in one step of the old camera, and its highs went with it: heard
   as the ambience cutting off at one point while zooming out. Now a wheel step takes
   it down about 3 to 4 dB on the N64 camera and 6 to 8 on the old one while it is
   still clearly audible, and it is below -35 dB by the last step before it ends.

   The lower piece is 0.5 * (10^(k u) - 1) / (10^k - 1) with u = zoom / 0.375: 0 at the
   furthest zoom, 0.5 at the crossover, and with k = 1.2253 its slope there equals the
   straight line's (4 per unit of zoom), so the curve has no corner. */
static float ambience_near_mix(float zoom)
{
    zoom = ambience_clamp(zoom);
    if (zoom >= 0.375f) return ambience_clamp((zoom - 0.25f) / 0.25f);
    const float k = 1.2253f;
    return 0.5f * (powf(10.0f, k * zoom / 0.375f) - 1.0f) / (powf(10.0f, k) - 1.0f);
}
static float ambience_river_mix(float zoom)
{
    zoom = ambience_clamp(zoom);
    /* 50% zoom -> 1, 25% -> 0.5, 0% -> 0; both segments have slope 2. */
    return zoom >= 0.5f ? 1.0f : 2.0f * zoom;
}
/* Full level at the source, smooth falloff to silence eight cells away. */
static float ambience_attenuation(float distance)
{
    float t = ambience_clamp(distance / 8.0f);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

/* Consecutive passes of a looping bed overlap by this long, on an equal-power curve.
   The beds are wind, birds, insects, surf and running water: their events (a gust, a
   call, a breaking wave) last one to three seconds, and a crossfade shorter than one
   event cuts it audibly, while a much longer one holds two unrelated stretches of the
   recording on top of each other long enough to hear as a doubled crowd. Four seconds
   covers an event and is under a tenth of the shortest bed (57 s). */
static const float AMBIENCE_XFADE_SECONDS = 4.0f;

/* The steady part of a bed recording, as stereo frames [*start, *end). The recordings
   fade in over their first one to three seconds and out over their last two to five;
   looping across those plays a 20 to 35 dB hole at every pass, which is heard as one
   bed ending and the next starting. Level is measured over one second windows stepped
   by 100 ms. The body begins at the END of the first window within 4 dB of the
   recording's median window level and ends at the START of the last such window, so
   whatever is left of a fade inside the body is less than 4 dB down. Each side is limited
   to the outer quarter of the clip; a side that never qualifies keeps the clip edge. */
static void ambience_body(const short* pcm, long frames, int rate, long* start, long* end)
{
    *start = 0;
    *end = frames;
    const long step = rate / 10;
    const long wins = step > 0 ? frames / step : 0;
    if (wins < 40) return;
    std::vector<double> e((size_t)wins);
    for (long w = 0; w < wins; ++w) {
        double sum = 0;
        const short* p = pcm + w * step * 2;
        for (long i = 0; i < step * 2; ++i) sum += (double)p[i] * p[i];
        e[(size_t)w] = sum / (double)(step * 2);
    }
    const long n = wins - 9; // one-second windows
    std::vector<double> lv((size_t)n);
    for (long w = 0; w < n; ++w) {
        double sum = 0;
        for (int k = 0; k < 10; ++k) sum += e[(size_t)(w + k)];
        lv[(size_t)w] = 10.0 * log10(sum / 10.0 + 1e-9);
    }
    std::vector<double> sorted(lv);
    std::nth_element(sorted.begin(), sorted.begin() + n / 2, sorted.end());
    const double qualify = sorted[(size_t)(n / 2)] - 4.0;
    long first = -1, last = -1;
    for (long w = 0; w < n; ++w) if (lv[(size_t)w] >= qualify) { first = w; break; }
    for (long w = n - 1; w >= 0; --w) if (lv[(size_t)w] >= qualify) { last = w; break; }
    if (first >= 0 && (first + 10) * step <= frames / 4) *start = (first + 10) * step;
    if (last >= 0 && last * step >= frames - frames / 4) *end = last * step;
}
#endif
