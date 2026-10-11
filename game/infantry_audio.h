#ifndef OPENCNC3D_INFANTRY_AUDIO_H
#define OPENCNC3D_INFANTRY_AUDIO_H
#include "infantry_audio_math.h"
struct InfantryAudioClip { Uint8* pcm; Uint32 bytes; bool tried; };
struct InfantryAudioStep { int beat; float phase; };
struct InfantryAudioVoice { int handle, id, gain, pan, variant, pitch, volume; float x, z; };
static InfantryAudioClip g_infantryClips[INFANTRY_AUDIO_VARIANTS] = {};
static std::map<int, InfantryAudioStep> g_infantrySteps;
static std::vector<InfantryAudioVoice> g_infantryVoices;
static unsigned g_infantryRng = 0, g_infantryPlays = 0;
static int g_infantryFrame = -1;
static float g_infantryZoom = 0;

static InfantryAudioClip* infantry_audio_clip(int n)
{
    InfantryAudioClip& c = g_infantryClips[n];
    if (c.tried) return c.pcm ? &c : NULL;
    c.tried = true;
    const char* custom = getenv("OPENCNC3D_FOLEY_DIR");
    const char* dirs[] = {"foley", "data/foley", "../data/foley"};
    for (int i=0;i<(custom && *custom ? 1:3);++i) {
        char path[1024]; SDL_AudioSpec spec;
        snprintf(path,sizeof path,"%s/infantry_step_%02d.wav",custom && *custom ? custom:dirs[i],n+1);
        if (!SDL_LoadWAV(path,&spec,&c.pcm,&c.bytes)) continue;
        if (spec.freq==MIX_RATE && spec.channels==2 && spec.format==AUDIO_S16LSB && c.bytes && c.bytes%4==0) {
            fprintf(stderr,"INFANTRYAUDIO|loaded|variant=%d|frames=%u\n",n+1,c.bytes/4);
            return &c;
        }
        SDL_FreeWAV(c.pcm); c.pcm=NULL;
    }
    fprintf(stderr,"INFANTRYAUDIO|missing/invalid|variant=%d\n",n+1); return NULL;
}
static void infantry_audio_stop(void)
{
    if (g_au) {
        audio_backend_lock();
        for (size_t i=0;i<g_infantryVoices.size();++i)
            mixer_voice_stop(cnc_audio_mixer(g_au),g_infantryVoices[i].handle);
        audio_backend_unlock();
    }
    g_infantryVoices.clear(); g_infantrySteps.clear(); g_infantryFrame=-1;
}
static void infantry_audio_free(void)
{
    infantry_audio_stop();
    for (int i=0;i<INFANTRY_AUDIO_VARIANTS;++i) { SDL_FreeWAV(g_infantryClips[i].pcm); g_infantryClips[i]=InfantryAudioClip(); }
}
static void infantry_audio_update(int fbw,int fbh,bool running)
{
    if (!g_au || !g_fx.enabled || g_editOn) { infantry_audio_stop(); return; }
    if (g_infantryFrame>g_engineFrame) infantry_audio_stop();
    g_infantryZoom = g_camMode==CAM_N64
        ? ambience_zoom_fraction(g_dist,dist_min_now(),DIST_MAX_OURS)
        : ambience_zoom_fraction(g_zoom,zoom_max_now(),ZOOM_MIN);
    Mixer* mix=cnc_audio_mixer(g_au);
    float vx0,vx1,vz0,vz1; view_ground_bounds(fbw,fbh,&vx0,&vx1,&vz0,&vz1);
    audio_backend_lock();
    for (size_t i=0;i<g_infantryVoices.size();) {
        InfantryAudioVoice& v=g_infantryVoices[i];
        if (!mixer_voice_active(mix,v.handle)) {g_infantryVoices.erase(g_infantryVoices.begin()+i);continue;}
        float col,row;
        const float dx=v.x<vx0?vx0-v.x:v.x>vx1?v.x-vx1:0;
        const float dz=v.z<vz0?vz0-v.z:v.z>vz1?v.z-vz1:0;
        v.gain=infantry_audio_gain(g_infantryZoom,ambience_attenuation(sqrtf(dx*dx+dz*dz)),v.volume);
        if (project_point(v.x,terrain_y(v.x,v.z),v.z,fbw,fbh,&col,&row)) v.pan=vehicle_audio_screen_pan(col,(float)fbw);
        if (running) mixer_voice_fade(mix,v.handle,v.gain,v.pan,12);
        else mixer_voice_fade_stop(mix,v.handle,25);
        ++i;
    }
    audio_backend_unlock();
    if (!running) {g_infantrySteps.clear();g_infantryFrame=g_engineFrame;return;}
    if (g_infantryFrame==g_engineFrame) return;  // render/probe calls never duplicate steps
    g_infantryFrame=g_engineFrame;
    std::map<int,InfantryAudioStep> next;
    for (size_t i=0;i<g_objects.size();++i) {
        const SimObject& o=g_objects[i];
        if (o.kind!=K_INFANTRY || o.id<0 || o.str<=0 || o.limbo || !visible(o) || !infantry_audio_is_soldier(o.type)) continue;
        std::map<int,AnimState>::const_iterator anim=g_anim.find(o.id);
        if (anim==g_anim.end()) continue;
        const AnimState& a=anim->second;
        // Two contacts per authored gait cycle. Sprite-only soldiers use distance,
        // with a 0.6-cell stride, rather than a timer that runs while blocked.
        const float phase=m3inf_gait_stride(o,a.spd)>1e-4f ? a.gph : a.dist/0.6f;
        const int beat=(int)floorf(phase*2);
        next[o.id]=InfantryAudioStep{beat,phase};
        if (a.slot!=M3INF_RUN || a.step<=0 || a.step>1.5f) continue;
        std::map<int,InfantryAudioStep>::const_iterator previous=g_infantrySteps.find(o.id);
        if (previous!=g_infantrySteps.end() && (beat<=previous->second.beat || phase-previous->second.phase>2)) continue;
        float col,row;
        if (!project_point(o.ex,terrain_y(o.ex,o.ez),o.ez,fbw,fbh,&col,&row)) continue;
        const float dx=o.ex<vx0?vx0-o.ex:o.ex>vx1?o.ex-vx1:0;
        const float dz=o.ez<vz0?vz0-o.ez:o.ez>vz1?o.ez-vz1:0;
        const float distance=ambience_attenuation(sqrtf(dx*dx+dz*dz));
        if (distance<=0) continue;
        if (!g_infantryRng) g_infantryRng=vehicle_audio_seed()^0x9e3779b9u;
        const int variant=(int)vehicle_audio_random_offset(g_infantryRng,INFANTRY_AUDIO_VARIANTS);
        const int pitch=900+(int)vehicle_audio_random_offset(g_infantryRng,201);
        const int volume=900+(int)vehicle_audio_random_offset(g_infantryRng,201);
        const int gain=infantry_audio_gain(g_infantryZoom,distance,volume);
        InfantryAudioClip* clip=infantry_audio_clip(variant);
        if (!clip) continue;
        const int pan=vehicle_audio_screen_pan(col,(float)fbw);
        audio_backend_lock();
        const int handle=mixer_play_foley_pitched(mix,(short*)clip->pcm,clip->bytes/4,pan,pitch);
        if (handle>=0) {
            mixer_voice_fade(mix,handle,gain,pan,6);
            g_infantryVoices.push_back(InfantryAudioVoice{handle,o.id,gain,pan,variant,pitch,volume,o.ex,o.ez});
            ++g_infantryPlays;
        }
        audio_backend_unlock();
    }
    g_infantrySteps.swap(next);
}
static void infantry_audio_dump(void)
{
    printf("INFANTRYAUDIO|state|zoom=%.4f|zoomgain=%.4f|plays=%u|voices=%zu\n",
        g_infantryZoom,infantry_audio_zoom_gain(g_infantryZoom),g_infantryPlays,g_infantryVoices.size());
    for (size_t i=0;i<g_infantryVoices.size();++i) {
        const InfantryAudioVoice& v=g_infantryVoices[i];
        printf("INFANTRYAUDIO|voice|id=%d|gain=%d|pan=%d|variant=%d|pitch=%d|volume=%d\n",v.id,v.gain,v.pan,v.variant+1,v.pitch,v.volume);
    }
}
#endif
