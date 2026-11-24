#include "../../../game/wave_static_cache.h"
#include <cstdio>
#include <chrono>
/* Independent original expressions; deliberately do not call the cache's math. */
static void original_envelope(float x,float L,float p,float* e,float* d) {
    *e=powf(x,p);*d=p/L*powf(x,p-1.0f);
}
static void original_rest(float s,const float* a,float* r,float* rs) {
    const float L=a[7],x=(s>1e-6f?s:1e-6f)/L,P=2.0f*3.14159265f/a[16];
    const float env=powf(x,a[17]),denv=a[17]/L*powf(x,a[17]-1.0f);
    *r=a[15]*env*sinf(P*s);
    *rs=a[15]*(denv*sinf(P*s)+env*cosf(P*s)*P);
}
static WaveStaticCache cache;
static unsigned rng=4242;
static float random_float(float lo,float hi){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return lo+(hi-lo)*(float)(rng&65535u)/65535.0f;}
int main(){
    unsigned long long checked=0;float checksum=0;
    const auto start=std::chrono::steady_clock::now();
    for(int round=0;round<160000;++round){
        float a[33]={};a[7]=random_float(0.01f,8);a[15]=random_float(-2,2);a[16]=random_float(0.01f,8);a[17]=random_float(0,5);
        const float s=round%8==0?0:random_float(-0.1f,8),p=random_float(0,5),x=(s>1e-6f?s:1e-6f)/a[7];
        float expected[4];original_envelope(x,a[7],p,expected,expected+1);original_rest(s,a,expected+2,expected+3);
        for(int repeat=0;repeat<3;++repeat){
            float got[4];cache.envelope(x,a[7],p,got,got+1);cache.rest(s,a[7],a[15],a[16],a[17],got+2,got+3);
#ifdef OPENCNC3D_NEGATIVE_CONTROL
            if(round==10&&repeat==1)got[2]+=1.0f;
#endif
            if(memcmp(expected,got,sizeof got)){fprintf(stderr,"WAVESTATIC_UNIT|bad|case=%d|repeat=%d\n",round,repeat);return 2;}
            ++checked;checksum+=got[0]*0.000001f;
        }
    }
    /* Revisit old inputs after forced direct-map evictions. Key every parameter,
       including signed zero and changed model wave coefficients. */
    for(int r=0;r<1024;++r){
        float a[33]={};a[7]=1;a[15]=r&1?-0.0f:0.0f;a[16]=2;a[17]=1;
        float old[2],got[2];original_rest(0.25f,a,old,old+1);cache.rest(0.25f,a[7],a[15],a[16],a[17],got,got+1);
        if(memcmp(old,got,sizeof got))return 3;
        ++checked;
    }
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    printf("WAVESTATIC_UNIT|checks=%llu|bad=0|hits=%llu|rest_hits=%llu|bytes=%zu|seconds=%.6f|checksum=%.9g\n",checked,cache.hits,cache.restHits,sizeof cache,seconds,checksum);
    return cache.hits>300000&&cache.restHits>300000?0:4;
}
