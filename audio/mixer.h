/*
 * mixer.h -- the mixer core. Plain C89, no SDL, no Windows, no stdio, no float in
 * the sample path. It owns no device: something else pulls PCM out of it and hands
 * that to SDL here, or to DirectSound / a WAV-out double buffer on Windows 98.
 *
 * Output is signed 16 bit STEREO interleaved at MIX_RATE. That is 44100 Hz on the
 * modern tiers, so replacement sounds keep everything up to 20 kHz, and 22050 Hz on
 * Win98, the rate the whole 1995 data set uses. The 1995 material reaches a 44100 Hz
 * mix through a windowed-sinc 2x upsampler (clips in the bank, the music and movie
 * streams here), which leaves everything below 11 kHz, i.e. all of it, as it was.
 * Stereo, not mono, because the engine hands us a world position with every sound
 * effect and the 1995 game panned them; see mixer_effect_place().
 *
 * Five buses, each with its own gain and its own linear ramp:
 *
 *   MIX_BUS_MUSIC    the score. One streaming voice, fed from a ring the game thread
 *                    refills, because a 6 to 13 MB track cannot be held in RAM on
 *                    the Win98 tier and must not be decoded inside an interrupt.
 *   MIX_BUS_FX       sound effects. N voices, priority stealing.
 *   MIX_BUS_SPEECH   EVA. One voice at a time, matching Speak_AI.
 *   MIX_BUS_MOVIE    VQA audio, pushed by the video decoder as it goes, because the
 *                    movie's clock is the video clock and its audio follows it.
 *   MIX_BUS_AMBIENCE environment loops and rain, controlled independently of FX.
 *
 * Gains are per mille, 1000 = unity, and clamp at MIX_GAIN_MAX (2000) so a caller
 * cannot silently blow the headroom out.
 *
 * Threading: mixer_render() is the only call safe to make from an audio interrupt.
 * Everything else is a control call. If your backend renders on another thread, the
 * platform layer wraps control calls in its own lock; the core deliberately has no
 * opinion about how that lock is spelled.
 */

#ifndef MIXER_H
#define MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

/* The 1995 discs are all 22050 Hz, and the music and movie streams arrive at that
 * rate. Every stream count this header speaks (push, want, level) is in those source
 * samples whatever MIX_RATE is, so a caller pacing a movie against them never sees
 * the output rate. */
#define MIX_SRC_RATE 22050
#ifdef WIN98
#define MIX_RATE 22050 /* Tier 1: half the mixing work and the 1995 rate exactly */
#else
#define MIX_RATE 44100
#endif
#define MIX_STREAM_UP (MIX_RATE / MIX_SRC_RATE) /* 1 or 2 */
#if MIX_RATE != MIX_SRC_RATE && MIX_RATE != 2 * MIX_SRC_RATE
#error MIX_RATE must be 22050 or 44100: the stream upsampler is an exact 2x
#endif
#define MIX_UNITY 1000
#define MIX_GAIN_MAX 2000
#define MIX_VOICES 32
/* Dedicated raw vehicle loops cannot consume the bank's FX/EVA/environment slots. */
#define MIX_VEHICLE_VOICES 64
#define MIX_FOLEY_VOICES 64
#define MIX_CINEMATIC_VOICES 16
#define MIX_TOTAL_VOICES (MIX_VOICES + MIX_VEHICLE_VOICES + MIX_FOLEY_VOICES + MIX_CINEMATIC_VOICES)

enum
{
    MIX_BUS_MUSIC = 0,
    MIX_BUS_FX,
    MIX_BUS_SPEECH,
    MIX_BUS_MOVIE,
    MIX_BUS_AMBIENCE, /* Enhanced environment; independent of effects and EVA */
    MIX_BUS_COUNT
};

typedef struct Mixer Mixer;

Mixer *mixer_create(void);
void mixer_destroy(Mixer *m);

/* --------------------------------------------------------------- bus gains */

void mixer_bus_gain(Mixer *m, int bus, int permille, int ms);
int mixer_bus_gain_get(const Mixer *m, int bus);
void mixer_master_gain(Mixer *m, int permille, int ms);
int mixer_master_gain_get(const Mixer *m);

/* The cinematic duck. A heavy impact holds the rest of the mix down for a moment so it
 * lands with weight and headroom instead of being summed into the soft knee: music,
 * ambience and every FX voice outside the cinematic pool go to `depth` per mille over
 * attack_ms, stay there hold_ms, then return to unity over release_ms. EVA and movie
 * audio are never ducked. A deeper request wins over a shallower one while it holds,
 * an equal one extends the hold, and the hold runs on the output clock, so a caller
 * that stops asking can never leave the mix held down. Integer only, like the rest. */
void mixer_duck(Mixer *m, int depth, int attack_ms, int hold_ms, int release_ms);
void mixer_duck_reset(Mixer *m);
int mixer_duck_get(const Mixer *m);

/* ------------------------------------------------------------------ voices */

/* Start a one shot on a bus. `pcm` must stay alive until the voice finishes, which
 * is why the bank pins what is playing. Returns a voice handle (>= 0) or -1 if the
 * mixer refused (no free voice and nothing lower priority to steal).
 *
 * gain     per mille, 0..MIX_GAIN_MAX
 * pan      -1000 hard left, 0 centre, +1000 hard right
 * priority 0..255, the engine's SoundEffectName[].Priority
 */
int mixer_play(Mixer *m, int bus, const short *pcm, long samples, int gain, int pan, int priority);

/* Same, but the voice REPEATS its clip until it is stopped. The seam is a sample
 * boundary inside mixer_render, not a block boundary, so there is no hole.
 *
 * 1995 had no looping voice: it re-TRIGGERED the bed instead (intro.cpp:197-201 stops
 * STRUGGLE.AUD and plays it again as soon as Is_Sample_Playing goes false, or when a
 * 0x3f = 63 tick = 1.05 s CountDownTimer expires against a 1.023 s clip). On a
 * block-based output that always leaves the tail of the block the voice died in as
 * digital silence, and the replacement can only start at the next block. Measured
 * against this very mixer and the real SDL block (audio_sdl.c: want.samples = 1024):
 * STRUGGLE.AUD is 22560 samples, 22560 mod 1024 = 32, so the hole is a FIXED 992
 * samples = 45.0 ms every 1.068 s at poll periods of 10 and 26 ms, and 91.4 ms at 50.
 * No polling rate fixes it. This does.
 *
 * A looping voice never retires by itself, so it holds its slot and cannot be stolen
 * by a higher priority sound. The caller MUST mixer_voice_stop it on every exit path.
 *
 * Tier 1: the wrap lives in mixer_render, which is plain C with no platform API, so
 * a Win98 DirectSound backend inherits it unchanged. */
int mixer_play_loop(Mixer *m, int bus, const short *pcm, long samples, int gain, int pan, int priority);

/* Interleaved stereo at MIX_RATE; frames counts stereo pairs. */
int mixer_play_stereo(Mixer *m, int bus, const short *pcm, long frames,
                      int gain, int pan, int priority, int loop);
/* Per-vehicle stereo loop on FX, in the separate vehicle voice pool. A deterministic
 * start offset prevents identical drive recordings from summing in lockstep. */
int mixer_play_vehicle_loop(Mixer *m, const short *pcm, long frames, int pan, long offset);
/* Soldier one-shot stereo foley has its own slots; never steals bank or vehicles. */
int mixer_play_foley(Mixer *m, const short *pcm, long frames, int pan);
/* Footstep pitch ratio in per mille, clamped to 900..1100; stereo interpolation. */
int mixer_play_foley_pitched(Mixer *m, const short *pcm, long frames, int pan, int pitch);
/* Cinematic impacts/rumble cannot steal footsteps, vehicles, environment or EVA. */
int mixer_play_cinematic(Mixer *m, const short *pcm, long frames, int pan, int loop);
/* Turn a looping unity-rate voice into a CROSSFADED loop over frames [start, end):
 * the last `xfade` frames before `end` play mixed with the first `xfade` frames from
 * `start` on an equal-power (sine/cosine) curve, and playback carries on from
 * start + xfade, so each pass is end - start - xfade frames long and no pass ever
 * starts or stops on its own. Equal power because the two ends of a recording are
 * uncorrelated: a linear crossfade of uncorrelated material sags 3 dB at its middle.
 * Frames outside [start, end) never sound, which is how a recording's own fade-in
 * and fade-out are kept out of the loop. Moves the voice to `start` if it sits outside
 * the range. Returns 0 and changes nothing for a one-shot, a pitched voice, or a range
 * shorter than two crossfades. Integer only, like the rest of the sample path. */
int mixer_voice_loop_xfade(Mixer *m, int handle, long start, long end, long xfade);
/* Fade then retire a loop on the audio clock, including when output is muted. */
void mixer_voice_fade_stop(Mixer *m, int handle, int ms);
/* Audio-clock ramp, including while muted; repeated equal targets do not restart it. */
void mixer_voice_fade(Mixer *m, int handle, int gain, int pan, int ms);
void mixer_voice_stop(Mixer *m, int handle);
void mixer_voice_gain(Mixer *m, int handle, int gain, int pan);
int mixer_voice_active(const Mixer *m, int handle);
void mixer_stop_bus(Mixer *m, int bus);

/* The 1995 tactical rule, kept in one place so the app does not reinvent it.
 * All coordinates in world pixels, which is what the engine's sound callback hands
 * over (PixelX / PixelY, already Lepton_To_Pixel'd).
 *
 * Inside the view: full volume, no pan, exactly as Sound_Effect() does.
 * Outside: volume falls with cell distance to a floor of 25/255 and the pan opens up
 * only once the sound is more than half a view width off centre.
 *
 * Returns 1 if audible and fills *gain and *pan; returns 0 for "not audible".
 * Pass x < 0 for a non positional sound: it comes back centred at full volume. */
int mixer_effect_place(int listener_cx, int listener_cy, int view_w, int view_h, int x, int y,
                       int *gain, int *pan);

/* ------------------------------------------------------------------- music */

/* The music ring. The game thread pushes decoded PCM in with mixer_music_push() from
 * mixer_music_want() bytes of headroom; the render callback only ever reads. */
void mixer_music_reset(Mixer *m);
int mixer_music_want(const Mixer *m);  /* samples of free space in the ring */
int mixer_music_level(const Mixer *m); /* samples currently queued          */
int mixer_music_push(Mixer *m, const short *pcm, int samples);

/* Movie audio, same shape, pushed by the VQA decoder. */
void mixer_movie_reset(Mixer *m);
int mixer_movie_want(const Mixer *m);
int mixer_movie_level(const Mixer *m);
int mixer_movie_push(Mixer *m, const short *pcm, int samples);

/* -------------------------------------------------------------- the output */

/* Renders `frames` stereo frames (2 shorts each) into dst. Always fills, with
 * silence where there is nothing to play. Safe from an audio interrupt. */
void mixer_render(Mixer *m, short *dst, int frames);

/* Diagnostics: how many voices are sounding, and the peak of the last render. */
int mixer_active_voices(const Mixer *m);
int mixer_last_peak(const Mixer *m);
long mixer_frames_rendered(const Mixer *m);

/* How often the output stage had to bend the signal: `knee` counts samples the soft
 * clipper compressed (gentle, mostly inaudible), `hard` counts samples that hit the
 * ceiling anyway (genuine overload). Cumulative since mixer_create. */
void mixer_clip_stats(const Mixer *m, long *knee, long *hard);

#ifdef __cplusplus
}
#endif

#endif /* MIXER_H */
