/* Signal checks using the production mixer; no audio device needed. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../game/vehicle_audio_math.h"
#include "../audio/mixer.h"
static void close_to(float a, float b) { assert(fabsf(a-b) < .00001f); }
int main()
{
    close_to(vehicle_audio_zoom_gain(1), 1);
    close_to(vehicle_audio_zoom_gain(.5f), .625f);
    close_to(vehicle_audio_zoom_gain(0), .25f);
    close_to(vehicle_audio_zoom_gain(ambience_zoom_fraction(5000,1000,9000)), .625f);
    close_to(vehicle_audio_zoom_gain(ambience_zoom_fraction(35,50,20)), .625f);
    const char* large[] = {"MTNK", "HTNK", "FTNK", "MLRS", "MSAM"};
    for (int i=0; i<5; ++i) assert(vehicle_audio_clip_index(large[i]) == 9);
    assert(vehicle_audio_clip_index("ARTY") == 10);
    assert(vehicle_audio_clip_index("LTNK") == 10);
    assert(vehicle_audio_clip_index("STNK") == 10);
    assert(vehicle_audio_clip_index("HARV") == 11);
    assert(vehicle_audio_clip_index("JEEP") == 11);
    assert(vehicle_audio_clip_index("APC") == 10);
    assert(vehicle_audio_clip_index("E1") == -1);
    assert(vehicle_audio_screen_pan(0,800) == -1000);
    assert(vehicle_audio_screen_pan(400,800) == 0);
    assert(vehicle_audio_screen_pan(800,800) == 1000);
    assert(vehicle_audio_screen_pan(1200,800) == 1000);
    short pcm[] = {1000,2000, 3000,4000, 5000,6000};
    short out[1024*2];
    Mixer* m = mixer_create(); assert(m);
    int v = mixer_play_vehicle_loop(m,pcm,3,0,4);
    assert(v >= 0 && (v & 255) >= MIX_VOICES);
    mixer_voice_gain(m,v,1000,0);
    mixer_render(m,out,1);
    assert(out[0] == 3000 && out[1] == 4000); // deterministic phase offset wraps
    mixer_voice_gain(m,v,250,0);
    mixer_render(m,out,1);
    assert(out[0] == 1250 && out[1] == 1500);
    mixer_voice_gain(m,v,1000,-1000);
    mixer_render(m,out,1);
    assert(out[0] > 0 && out[1] == 0);
    mixer_voice_gain(m,v,1000,1000);
    mixer_render(m,out,1);
    assert(out[0] == 0 && out[1] > 0);
    // Repeated stop requests cannot keep resetting the fade forever. Restarting
    // during that fade cancels retirement and keeps the same loop phase/handle.
    mixer_voice_gain(m,v,1000,0);
    mixer_voice_fade_stop(m,v,320);
    mixer_render(m,out,512);
    assert(out[1022] > 0 && mixer_voice_active(m,v));
    mixer_voice_fade(m,v,1000,0,220);
    for(int i=0; i<32; ++i) mixer_render(m,out,1024);
    assert(mixer_voice_active(m,v));
    mixer_voice_fade_stop(m,v,320);
    mixer_bus_gain(m,MIX_BUS_FX,0,0);
    for(int i=0; i<32; ++i) {
        mixer_voice_fade_stop(m,v,320);
        mixer_render(m,out,1024);
        for(int j=0;j<2048;++j) assert(out[j] == 0);
    }
    assert(!mixer_voice_active(m,v)); // retired even while Sound Volume is muted
    // Saturating the separate vehicle pool must leave room for environment and EVA.
    int vehicles[MIX_VEHICLE_VOICES];
    for(int i=0;i<MIX_VEHICLE_VOICES;++i) {
        vehicles[i]=mixer_play_vehicle_loop(m,pcm,3,0,i);
        assert(vehicles[i]>=0);
    }
    assert(mixer_play_vehicle_loop(m,pcm,3,0,0) == -1);
    int bed=mixer_play_stereo(m,MIX_BUS_AMBIENCE,pcm,3,1000,0,255,1);
    int eva=mixer_play(m,MIX_BUS_SPEECH,pcm,6,1000,0,255);
    assert(bed>=0 && eva>=0 && (bed&255)<MIX_VOICES && (eva&255)<MIX_VOICES);
    assert(mixer_active_voices(m)==MIX_VEHICLE_VOICES+2);
    for(int i=0;i<64;++i) mixer_play(m,MIX_BUS_FX,pcm,6,1000,0,128);
    assert(mixer_voice_active(m,bed) && mixer_voice_active(m,eva));
    for(int i=0;i<MIX_VEHICLE_VOICES;++i) assert(mixer_voice_active(m,vehicles[i]));
    mixer_stop_bus(m,MIX_BUS_FX);
    for(int i=0;i<MIX_VEHICLE_VOICES;++i) assert(!mixer_voice_active(m,vehicles[i]));
    assert(mixer_voice_active(m,bed) && mixer_voice_active(m,eva));
    // Random restart offsets select whole stereo frames of the actual mixer input.
    // Each fresh voice begins silent and fades in; successive starts vary the signal.
    mixer_stop_bus(m,MIX_BUS_AMBIENCE); mixer_stop_bus(m,MIX_BUS_SPEECH);
    mixer_bus_gain(m,MIX_BUS_FX,1000,0);
    unsigned rng=123456789u;
    int varied=0, previous=-1;
    for (int i=0;i<24;++i) {
        long offset=vehicle_audio_random_offset(rng,3);
        assert(offset>=0 && offset<3);
        int h=mixer_play_vehicle_loop(m,pcm,3,0,offset); assert(h>=0);
        mixer_render(m,out,1); assert(out[0]==0 && out[1]==0);
        mixer_voice_gain(m,h,1000,0); mixer_render(m,out,1);
        assert(out[0]==pcm[((offset+1)%3)*2]);
        if(previous>=0 && out[0]!=previous) ++varied;
        previous=out[0]; mixer_voice_stop(m,h);
    }
    assert(varied>8);
    // Restart while the old journey still fades: old tail and new fade coexist,
    // and neither consumes the bank/EVA pool. The old tail eventually retires.
    int tail=mixer_play_vehicle_loop(m,pcm,3,0,0);
    mixer_voice_gain(m,tail,1000,0); mixer_voice_fade_stop(m,tail,320);
    int fresh=mixer_play_vehicle_loop(m,pcm,3,0,2);
    mixer_voice_fade(m,fresh,1000,0,220);
    assert(tail!=fresh && mixer_voice_active(m,tail) && mixer_voice_active(m,fresh));
    for(int i=0;i<32;++i) mixer_render(m,out,1024);
    assert(!mixer_voice_active(m,tail) && mixer_voice_active(m,fresh));
    mixer_destroy(m);
    puts("VEHICLE AUDIO GATE PASS: types, live zoom, stereo placement, phase, stop/resume fades, muted retirement, full pool isolation");
    return 0;
}
