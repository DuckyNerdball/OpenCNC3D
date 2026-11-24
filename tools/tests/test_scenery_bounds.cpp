#include <cassert>
#include <cfloat>
#include <cstdio>
#include <cstdint>
#include "../../game/scenery_bounds.h"

static uint32_t state = 0x64335444u;
static float pick(float lo, float hi)
{
    state = state * 1664525u + 1013904223u;
    return lo + (hi - lo) * (float)(state >> 8) / 16777215.0f;
}
static void transform(float p[3], const float m[12])
{
    const float x = m[0]*p[0] + m[1]*p[1] + m[2]*p[2] + m[3];
    const float y = m[4]*p[0] + m[5]*p[1] + m[6]*p[2] + m[7];
    const float z = m[8]*p[0] + m[9]*p[1] + m[10]*p[2] + m[11];
    p[0] = x; p[1] = y; p[2] = z;
}

int main()
{
    unsigned contained = 0;
    /* These vertices execute the renderer's float sway, flutter and two transform
       stages independently of the interval construction. Include source extrema,
       byte-weight extremes, tuned scale/variation/lean extremes, negative transforms,
       all wind phases and both shadow nudges. */
    for (int trial = 0; trial < 300; trial++) {
        float source[64][3];
        SceneryBounds box;
        for (auto& p : source) {
            p[0] = pick(-4096, 4096); p[1] = pick(-512, 8192); p[2] = pick(-4096, 4096);
            assert(box.add(p[0], p[1], p[2]));
        }
        const float sway = trial % 5 == 0 ? 0.20f : pick(-0.20f, 0.20f);
        const float flutter = trial % 5 == 0 ? 1.0f : pick(-1.0f, 1.0f);
        const float width = pick(0.50f, 1.80f) * pick(0.60f, 1.40f);
        const float height = pick(0.50f, 1.80f) * pick(0.60f, 1.40f);
        const float lean = pick(-0.20f, 0.20f), turn = pick(-6.284f, 6.284f);
        const float tree[12] = {width,sinf(turn)*lean,0,0, 0,height,0,0,
            0,cosf(turn)*lean,width,0};
        const float angle = pick(-6.284f, 6.284f), scale = (trial % 7 ? 1.0f : -1.0f)/1024.0f;
        const float world[12] = {cosf(angle)*scale,0,sinf(angle)*scale,pick(-20,148),
            0,scale,0,pick(-15,30), -sinf(angle)*scale,0,cosf(angle)*scale,pick(-20,148)};
        assert(scenery_bounds_wind(box, sway, flutter));
        assert(scenery_bounds_affine(box, tree));
        assert(scenery_bounds_affine(box, world));
        float lo[3], hi[3];
        assert(scenery_bounds_export(box, lo, hi));
        for (int tick = 0; tick < 96; tick++) {
            const float t = pick(-100000, 100000), T = 6.2831853f;
            const float leanNow = (sway / 0.04107f) * (0.012f + 0.040f
                * (0.55f*sinf(T*0.16f*t) + 0.30f*sinf(T*0.27f*t+1.3f)
                   + 0.15f*sinf(T*0.53f*t+2.1f)));
            const float a1=sinf(T*1.6f*t),b1=cosf(T*1.6f*t);
            const float a2=sinf(T*2.9f*t),b2=cosf(T*2.9f*t);
            const float bearing = pick(-720, 720);
            const float windX=1024.0f*sinf(bearing),windZ=-1024.0f*cosf(bearing);
            for (const auto& v : source) {
                const unsigned weight = tick % 3 ? (unsigned)pick(0,255) : 255;
                const unsigned needles = tick % 3 ? (unsigned)pick(0,255) : 255;
                const float phase=(9.0f*v[0]+4.0f*v[1]+6.0f*v[2])*(1.0f/1024.0f);
                const float m = leanNow * (float)weight * (1.0f/255.0f);
                const float up=-0.6f*m*m + (0.007f*flutter/0.07143f) * (float)needles
                    * (1.0f/255.0f) * (a1*cosf(phase)+b1*sinf(phase)
                        +0.55f*(a2*cosf(1.7f*phase)+b2*sinf(1.7f*phase)));
                float p[3]={v[0]+windX*m,v[1]+1024.0f*up,v[2]+windZ*m};
                transform(p,tree); transform(p,world);
                for(int k=0;k<3;k++) assert(p[k]>=lo[k] && p[k]<=hi[k]);
                contained++;
            }
        }
    }
    SceneryBounds none;
    float lo[3],hi[3],identity[12]={1,0,0,0,0,1,0,0,0,0,1,0};
    assert(!scenery_bounds_export(none,lo,hi));
    assert(!scenery_bounds_wind(none,0,0));
    assert(none.add(1,2,3));
    assert(!none.add(NAN,2,3));
    assert(!scenery_bounds_wind(none,NAN,0));
    assert(!scenery_bounds_wind(none,0,INFINITY));
    identity[5]=INFINITY;
    assert(!scenery_bounds_affine(none,identity));
    SceneryBounds huge;
    assert(huge.add(FLT_MAX,FLT_MAX,FLT_MAX));
    const float twice[12]={2,0,0,0,0,2,0,0,0,0,2,0};
    assert(scenery_bounds_affine(huge,twice));
    assert(!scenery_bounds_export(huge,lo,hi));
    printf("SCENERY-BOUNDS|%u-wind-tune-transform-vertices-contained|nonfinite-overflow-empty-fail-open|PASS\n",contained);
}
