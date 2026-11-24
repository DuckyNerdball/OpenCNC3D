#ifndef OPENCNC3D_WAVE_STATIC_CACHE_H
#define OPENCNC3D_WAVE_STATIC_CACHE_H
#include <cmath>
#include <cstdint>
#include <cstring>
/* Only time-independent arithmetic is retained. Keys contain every input bit,
   rather than a model pointer or an animation time. Fixed direct-mapped tables
   need no allocation, pointer lifetime or pack-generation assumptions. A collision
   recomputes the original expression, including its original float rounding. */
struct WaveStaticCache {
    struct Envelope { uint32_t key[3]; float env,denv; bool valid=false; };
    struct Rest { uint32_t key[5]; float r,rs; bool valid=false; };
    Envelope envelopes[16384]; Rest rests[4096];
    unsigned long long offers=0,hits=0,restOffers=0,restHits=0;
    template<size_t N> static unsigned index(const float (&values)[N],uint32_t (&key)[N]) {
        memcpy(key,values,sizeof key);unsigned h=2166136261u;
        for(size_t i=0;i<N;++i){h=(h^key[i])*16777619u;h^=h>>13;}
        return h;
    }
    void envelope(float x,float length,float power,float* env,float* denv) {
        const float values[]={x,length,power};uint32_t key[3];
        Envelope& entry=envelopes[index(values,key)&16383u];++offers;
        if(entry.valid&&!memcmp(entry.key,key,sizeof key)){++hits;*env=entry.env;*denv=entry.denv;return;}
        *env=powf(x,power);*denv=power/length*powf(x,power-1.0f);
        memcpy(entry.key,key,sizeof key);entry.env=*env;entry.denv=*denv;entry.valid=true;
    }
    void rest(float s,float length,float amplitude,float period,float power,float* r,float* rs) {
        const float values[]={s,length,amplitude,period,power};uint32_t key[5];
        Rest& entry=rests[index(values,key)&4095u];++restOffers;
        if(entry.valid&&!memcmp(entry.key,key,sizeof key)){++restHits;*r=entry.r;*rs=entry.rs;return;}
        const float x=(s>1e-6f?s:1e-6f)/length,P=2.0f*3.14159265f/period;
        const float env=powf(x,power),denv=power/length*powf(x,power-1.0f);
        *r=amplitude*env*sinf(P*s);
        *rs=amplitude*(denv*sinf(P*s)+env*cosf(P*s)*P);
        memcpy(entry.key,key,sizeof key);entry.r=*r;entry.rs=*rs;entry.valid=true;
    }
};
static_assert(sizeof(WaveStaticCache)<768u*1024u,"Fixed cloth arithmetic budget");
#endif
