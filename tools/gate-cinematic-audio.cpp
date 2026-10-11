#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../game/cinematic_audio_math.h"
#include "../audio/mixer.h"
static void eq(float a,float b){assert(fabsf(a-b)<.00001f);}
/* One voice of a kind alone, a constant 8192 on both channels: what reaches the output. */
static int level_of(Mixer*m,int kind){
 static short mono[64],st[128];for(int i=0;i<64;++i)mono[i]=8192;for(int i=0;i<128;++i)st[i]=8192;
 int h=kind==0?mixer_play_loop(m,MIX_BUS_FX,mono,64,1000,0,100)
      :kind==1?mixer_play_loop(m,MIX_BUS_SPEECH,mono,64,1000,0,100)
      :kind==2?mixer_play_loop(m,MIX_BUS_AMBIENCE,mono,64,1000,0,100)
      :mixer_play_cinematic(m,st,64,0,1);
 if(kind==3)mixer_voice_gain(m,h,1000,0);
 short out[512];mixer_render(m,out,256);mixer_voice_stop(m,h);return out[510];
}
static void duck_test(){
 Mixer*m=mixer_create();assert(m);
 const int fx=level_of(m,0),eva=level_of(m,1),amb=level_of(m,2),cin=level_of(m,3);
 assert(fx>8000&&eva==fx&&amb==fx&&cin==fx&&mixer_duck_get(m)==1000);
 // an impact's duck: FX and ambience drop to 45%, EVA and the cinematic pool do not
 mixer_duck(m,450,0,230,1100);
 assert(abs(level_of(m,0)-fx*45/100)<16&&abs(level_of(m,2)-fx*45/100)<16);
 assert(level_of(m,1)==eva&&level_of(m,3)==cin);
 // a shallower request (the rumble's) cannot lift it while it holds
 mixer_duck(m,850,150,150,600);assert(mixer_duck_get(m)==450);
 // the hold runs out on the output clock and the release brings the mix back to unity
 short out[2048];for(long f=0;f<(long)MIX_RATE*300/1000;f+=1024)mixer_render(m,out,1024);
 const int mid=mixer_duck_get(m);assert(mid>450&&mid<1000);
 for(int i=0;i<60;++i)mixer_render(m,out,1024);
 assert(mixer_duck_get(m)==1000&&level_of(m,0)==fx);
 // a reset (a mission's teardown) is immediate
 mixer_duck(m,300,0,5000,5000);assert(mixer_duck_get(m)==300);mixer_duck_reset(m);assert(mixer_duck_get(m)==1000);
 mixer_destroy(m);
}
/* The rumble recording (2.61 s) loops over its middle three fifths with a quarter-length
   crossfade, so a Construction Yard's 3.5 s rumble never hears it stop or restart. */
static void rumble_loop_test(){
 long s0,s1,xf;const long n=115110;cinematic_audio_rumble_body(n,&s0,&s1,&xf);
 assert(s0==23022&&s1==92088&&xf==17266);
 static short pcm[115110*2];for(long i=0;i<n*2;++i)pcm[i]=4000;
 Mixer*m=mixer_create();assert(m);int h=mixer_play_cinematic(m,pcm,n,0,1);assert(h>=0);
 assert(mixer_voice_loop_xfade(m,h,s0,s1,xf));mixer_voice_gain(m,h,1000,0);
 // five seconds on it still sounds and never dips below the recording's level. A constant
 // signal is its own worst case: both ends identical, the equal-power seam rises to sqrt 2
 // (a real recording's two ends are uncorrelated and sum to a steady level).
 short out[2048];int lo=32767,hi=0;
 for(long f=0;f<(long)MIX_RATE*5;f+=1024){mixer_render(m,out,1024);if(f>MIX_RATE/10)for(int j=0;j<2048;++j){if(out[j]<lo)lo=out[j];if(out[j]>hi)hi=out[j];}}
 assert(mixer_voice_active(m,h)&&lo>=3990&&hi<=5660);
 mixer_destroy(m);
}
int main(){
 eq(cinematic_audio_rise(0,.4f,1.6f,.4f),0);
 eq(cinematic_audio_rise(1,.4f,1.6f,.4f),.5f);
 eq(cinematic_audio_rise(1.6f,.4f,1.6f,.4f),1);
 eq(cinematic_audio_rise(1.8f,.4f,1.6f,.4f),.5f);
 eq(cinematic_audio_rise(2,.4f,1.6f,.4f),0);
 eq(cinematic_audio_rise(1,-1,2,.4f),0);
 eq(cinematic_audio_distance(0),1);eq(cinematic_audio_distance(6),.5f);eq(cinematic_audio_distance(12),0);
 // the rumble: the preset strength gives the footprint's weight, scaled by the envelope
 eq(cinematic_audio_rumble_level(.006f,1,1),.55f);
 eq(cinematic_audio_rumble_level(.006f,2,1),.70f);
 eq(cinematic_audio_rumble_level(.006f,4,1),1);
 eq(cinematic_audio_rumble_level(.006f,2,.5f),.35f);
 eq(cinematic_audio_rumble_level(.003f,4,1),.5f);
 eq(cinematic_audio_rumble_level(.0075f,2,1),.875f);
 eq(cinematic_audio_rumble_level(.03f,4,1),1);
 eq(cinematic_audio_rumble_level(0,2,1),0);
 // the impact: a placement is heavier than the destruction layers, and grows with its footprint
 assert(cinematic_audio_impact_gain(1,1)==800&&cinematic_audio_impact_gain(1,2)==900&&cinematic_audio_impact_gain(1,4)==1100);
 assert(cinematic_audio_impact_gain(0,2)==800&&cinematic_audio_impact_gain(0,4)==1000);
 // the duck: full depth at unity, none when muted or out of earshot
 assert(cinematic_audio_duck_depth(.55f,1,1000)==450&&cinematic_audio_duck_depth(.55f,2,1000)==450);
 assert(cinematic_audio_duck_depth(.55f,1,0)==1000&&cinematic_audio_duck_depth(.55f,0,1000)==1000);
 assert(cinematic_audio_duck_depth(.30f,.5f,1000)==850);
 short pcm[]={1000,2000,3000,4000,5000,6000},out[2048];Mixer*m=mixer_create();assert(m);
 int bank=mixer_play_loop(m,MIX_BUS_AMBIENCE,pcm,6,0,0,255);
 int veh=mixer_play_vehicle_loop(m,pcm,3,0,0),foot=mixer_play_foley(m,pcm,3,0);
 int rumble=mixer_play_cinematic(m,pcm,3,0,1);
 assert((rumble&255)==MIX_VOICES+MIX_VEHICLE_VOICES+MIX_FOLEY_VOICES);
 for(int i=0;i<64;++i) assert(mixer_play_cinematic(m,pcm,3,0,0)>=0);
 assert(mixer_voice_active(m,bank)&&mixer_voice_active(m,veh)&&mixer_voice_active(m,foot)&&mixer_voice_active(m,rumble));
 for(int i=0;i<100;++i) assert(mixer_play_foley(m,pcm,3,0)>=0);
 assert(mixer_voice_active(m,rumble));mixer_stop_bus(m,MIX_BUS_FX);
 int h=mixer_play_cinematic(m,pcm,3,-1000,0);mixer_voice_gain(m,h,1000,-1000);
 mixer_render(m,out,1);assert(out[0]==1000&&out[1]==0);mixer_render(m,out,3);assert(!mixer_voice_active(m,h));
 h=mixer_play_cinematic(m,pcm,3,0,1);mixer_voice_gain(m,h,1000,0);mixer_voice_fade_stop(m,h,100);mixer_bus_gain(m,MIX_BUS_FX,0,0);
 for(int i=0;i<12;++i){mixer_voice_fade_stop(m,h,100);mixer_render(m,out,1024);for(int j=0;j<2048;++j)assert(out[j]==0);}
 assert(!mixer_voice_active(m,h));assert(mixer_voice_active(m,bank));mixer_destroy(m);
 duck_test();
 rumble_loop_test();
 puts("CINEMATIC AUDIO PASS: placement envelope, rumble strength, impact gain, attenuation, stereo, mute retirement, four independent voice pools, the duck, the rumble's crossfaded loop");
}
