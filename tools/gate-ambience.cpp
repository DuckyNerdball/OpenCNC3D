/* Headless signal-level checks for Enhanced ambience. No device or game required. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "../game/ambience_math.h"
#include "../audio/mixer.h"
static void close_to(float a, float b) { assert(fabsf(a-b) < 0.00001f); }
static unsigned g_lcg = 12345u;
static int noise() { g_lcg = g_lcg * 1664525u + 1013904223u; return (int)(g_lcg >> 16) - 32768; }
static double rms_db(const short* x, long n, long stride)
{
    double sum = 0;
    for (long i = 0; i < n; ++i) sum += (double)x[i * stride] * x[i * stride];
    return 10 * log10(sum / n + 1e-9);
}

/* A crossfaded loop on real mixer output. Noise for the level checks, because the two
   ends of a recording are uncorrelated and that is what equal power is for; a smooth
   signal for the seam check, because on noise a click hides among the steps. */
static void crossfaded_loop()
{
    const long rate = MIX_RATE, len = 10 * rate, xf = rate, a = rate / 2, b = len - rate / 2;
    std::vector<short> clip((size_t)len * 2);
    for (long i = 0; i < len; ++i) {
        clip[(size_t)i * 2] = (short)(noise() / 4);
        clip[(size_t)i * 2 + 1] = (short)(noise() / 4);
    }
    Mixer* m = mixer_create(); assert(m);
    int v = mixer_play_stereo(m, MIX_BUS_AMBIENCE, clip.data(), len, 1000, 0, 255, 1);
    assert(v >= 0);
    assert(!mixer_voice_loop_xfade(m, v, a, a + 2 * xf - 1, xf)); // too short for two
    assert(mixer_voice_loop_xfade(m, v, a, b, xf));
    const long period = b - a - xf, total = 3 * period + xf;
    std::vector<short> out((size_t)total * 2);
    mixer_render(m, out.data(), (int)total);
    // The first pass starts at the body, untouched, until the crossfade begins.
    for (long i = 0; i < b - xf - a; ++i) assert(out[(size_t)i * 2] == clip[(size_t)(a + i) * 2]);
    // Then the loop repeats exactly, every period (the very first second is the body's
    // head heard plainly; every later pass hears it inside the crossfade).
    for (long i = xf; i < 2 * period; ++i) assert(out[(size_t)i * 2 + 1] == out[(size_t)(i + period) * 2 + 1]);
    // Level: 250 ms windows across three crossfades stay within 0.5 dB of the body level
    // (a linear crossfade measures 3 dB down at its middle here).
    const double body = rms_db(clip.data() + a * 2, b - a, 2);
    const long w = rate / 4;
    double lo = 99, hi = -99;
    for (long s = 0; s + w <= total; s += w) {
        for (int ch = 0; ch < 2; ++ch) {
            const double d = rms_db(out.data() + s * 2 + ch, w, 2) - body;
            lo = d < lo ? d : lo; hi = d > hi ? d : hi;
        }
    }
    printf("AMBIENCE|xfade|noise level across passes %+.2f..%+.2f dB of the body\n", lo, hi);
    assert(lo > -0.5 && hi < 0.5);
    // A muted bus keeps the crossfaded loop's clock: after silence the pass resumes in phase.
    mixer_bus_gain(m, MIX_BUS_AMBIENCE, 0, 0);
    std::vector<short> quiet((size_t)period * 2);
    mixer_render(m, quiet.data(), (int)(period - xf));
    for (long i = 0; i < period - xf; ++i) assert(quiet[(size_t)i * 2] == 0);
    mixer_bus_gain(m, MIX_BUS_AMBIENCE, 1000, 0);
    mixer_render(m, quiet.data(), 64);
    for (long i = 0; i < 64; ++i) assert(quiet[(size_t)i * 2] == out[(size_t)(total - xf + i) * 2]);
    mixer_destroy(m);

    // The seam: two different slow tones, head and tail. The largest step anywhere in
    // the output, entering the crossfade and jumping back included, is no larger than
    // the largest step of the tones themselves.
    for (long i = 0; i < len; ++i) {
        const double t = (double)i / rate;
        const double f = i < len / 2 ? 110.0 : 173.0;
        clip[(size_t)i * 2] = clip[(size_t)i * 2 + 1] = (short)(12000 * sin(2 * M_PI * f * t + (i < len / 2 ? 0.3 : 2.1)));
    }
    int own = 0;
    for (long i = 1; i < len; ++i) {
        const int d = abs(clip[(size_t)i * 2] - clip[(size_t)(i - 1) * 2]);
        if (i != len / 2) own = d > own ? d : own;
    }
    m = mixer_create(); assert(m);
    v = mixer_play_stereo(m, MIX_BUS_AMBIENCE, clip.data(), len, 1000, 0, 255, 1);
    assert(mixer_voice_loop_xfade(m, v, a, len / 2 - rate / 4, xf)); // tones on both ends
    mixer_render(m, out.data(), (int)total);
    int step = 0;
    for (long i = 1; i < total; ++i) {
        const int d = abs(out[(size_t)i * 2] - out[(size_t)(i - 1) * 2]);
        step = d > step ? d : step;
    }
    printf("AMBIENCE|xfade|largest step %d against the tones' own %d\n", step, own);
    assert(step <= own + 2);
    mixer_destroy(m);
}

/* A recording that fades in over 2 s and out over 3 s: the body leaves both fades out
   and keeps everything else but a second either side. */
static void bed_body()
{
    const long rate = MIX_RATE, len = 60 * rate;
    std::vector<short> clip((size_t)len * 2);
    for (long i = 0; i < len; ++i) {
        double g = 1;
        if (i < 2 * rate) g = pow(10, -40.0 * (1 - (double)i / (2 * rate)) / 20);
        if (i > len - 3 * rate) g = pow(10, -40.0 * (double)(i - (len - 3 * rate)) / (3 * rate) / 20);
        clip[(size_t)i * 2] = (short)(noise() / 4 * g);
        clip[(size_t)i * 2 + 1] = (short)(noise() / 4 * g);
    }
    long s = -1, e = -1;
    ambience_body(clip.data(), len, (int)rate, &s, &e);
    printf("AMBIENCE|body|%.2f s .. %.2f s of a %.0f s clip (fades end 2.00 s, start %.2f s)\n",
           (double)s / rate, (double)e / rate, (double)len / rate, (double)(len - 3 * rate) / rate);
    assert(s >= 2 * rate && s <= 3 * rate + rate / 2);
    assert(e <= len - 3 * rate && e >= len - 4 * rate - rate / 2);
    // A clip with no fades keeps all but a second either side; a tiny one is left alone.
    for (long i = 0; i < len * 2; ++i) clip[(size_t)i] = (short)(noise() / 4);
    ambience_body(clip.data(), len, (int)rate, &s, &e);
    assert(s <= rate + rate / 10 && e >= len - rate - rate / 10);
    ambience_body(clip.data(), rate, (int)rate, &s, &e);
    assert(s == 0 && e == rate);
}

/* The shipped beds themselves, through the real mixer, when the gate is handed the
   ambience folder (`gate-ambience data/ambience`). For each bed: the recording's own
   dip where a plain loop wraps (what the beds did before they crossfaded), then the
   crossfaded output, 250 ms windows across the overlap, against the level the two
   halves would sum to if they were unrelated. Fails on a window more than 1.5 dB off
   that sum, or on a sample step at either seam larger than the bed's own largest. */
static bool read_wav(const char* path, std::vector<short>& pcm)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    unsigned char h[12];
    bool ok = fread(h, 1, 12, f) == 12, fmt = false;
    while (ok) {
        unsigned char c[8];
        if (fread(c, 1, 8, f) != 8) { ok = false; break; }
        const long n = c[4] | c[5] << 8 | c[6] << 16 | (long)c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) {
            unsigned char b[16];
            ok = n >= 16 && fread(b, 1, 16, f) == 16;
            fmt = ok && (b[0] | b[1] << 8) == 1 && (b[2] | b[3] << 8) == 2 &&
                  (b[4] | b[5] << 8 | b[6] << 16) == MIX_RATE && (b[14] | b[15] << 8) == 16;
            fseek(f, n - 16 + (n & 1), SEEK_CUR);
        } else if (!memcmp(c, "data", 4)) {
            pcm.resize((size_t)n / 2);
            ok = fmt && fread(pcm.data(), 2, pcm.size(), f) == pcm.size();
            break;
        } else fseek(f, n + (n & 1), SEEK_CUR);
    }
    fclose(f);
    return ok && fmt;
}

static int real_beds(const char* dir)
{
    static const char* const beds[] = { "desert_zoomin", "desert_zoomout", "temperate_zoomin",
        "temperate_zoomout", "river_zoomin", "water_zoomin", "water_zoomout" };
    const long rate = MIX_RATE, xf = (long)(AMBIENCE_XFADE_SECONDS * rate), w = rate / 4;
    int failed = 0;
    for (const char* name : beds) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.wav", dir, name);
        std::vector<short> clip;
        if (!read_wav(path, clip)) { printf("AMBIENCE|bed|%s|unreadable\n", name); ++failed; continue; }
        const long len = (long)clip.size() / 2;
        long a, b;
        ambience_body(clip.data(), len, (int)rate, &a, &b);
        // A plain loop: the quietest 250 ms window within 4 s of the wrap, against the
        // median 250 ms level of the whole recording.
        std::vector<double> lv;
        for (long s = 0; s + w <= len; s += w) lv.push_back(rms_db(clip.data() + s * 2, w * 2, 1));
        std::vector<double> med(lv);
        std::nth_element(med.begin(), med.begin() + med.size() / 2, med.end());
        double hole = 99;
        for (long s = len - 4 * rate; s + w <= len + 4 * rate; s += w / 5) {
            std::vector<short> seg((size_t)w * 2);
            for (long i = 0; i < w; ++i) for (int c = 0; c < 2; ++c)
                seg[(size_t)i * 2 + c] = clip[(size_t)(((s + i) % len) * 2 + c)];
            const double d = rms_db(seg.data(), w * 2, 1) - med[med.size() / 2];
            hole = d < hole ? d : hole;
        }
        Mixer* m = mixer_create(); assert(m);
        int v = mixer_play_stereo(m, MIX_BUS_AMBIENCE, clip.data(), len, 1000, 0, 255, 1);
        assert(mixer_voice_loop_xfade(m, v, a, b, xf));
        const long pre = b - xf - a, total = pre + xf + rate;
        std::vector<short> out((size_t)total * 2);
        mixer_render(m, out.data(), (int)total);
        mixer_destroy(m);
        double worst = 0, quiet = 99;
        for (long s = 0; s + w <= xf; s += w) {
            double pt = 0, ph = 0, po = 0;
            for (long i = s; i < s + w; ++i) {
                const double th = 1.5707963 * (double)i / xf, gi = sin(th), go = cos(th);
                for (int c = 0; c < 2; ++c) {
                    const double t = clip[(size_t)(b - xf + i) * 2 + c], hd = clip[(size_t)(a + i) * 2 + c];
                    const double o = out[(size_t)(pre + i) * 2 + c];
                    pt += go * go * t * t; ph += gi * gi * hd * hd; po += o * o;
                }
            }
            const double d = 10 * log10((po + 1) / (pt + ph + 1));
            worst = fabs(d) > fabs(worst) ? d : worst;
            const double q = 10 * log10(po / (2.0 * w) + 1e-9) - med[med.size() / 2];
            quiet = q < quiet ? q : quiet;
        }
        int own = 0, seam = 0;
        for (long i = a + 1; i < b; ++i) for (int c = 0; c < 2; ++c) {
            const int d = abs(clip[(size_t)i * 2 + c] - clip[(size_t)(i - 1) * 2 + c]);
            own = d > own ? d : own;
        }
        for (long at : { pre, pre + xf }) for (long i = at - 64; i < at + 64; ++i) for (int c = 0; c < 2; ++c) {
            const int d = abs(out[(size_t)i * 2 + c] - out[(size_t)(i - 1) * 2 + c]);
            seam = d > seam ? d : seam;
        }
        const bool pass = fabs(worst) <= 1.5 && seam <= own;
        printf("AMBIENCE|bed|%s|%s|body %.2f..%.2f s of %.2f s|plain wrap dips %.1f dB|"
               "crossfade %.0f s: quietest window %+.1f dB of median, worst %+.2f dB off the "
               "unrelated sum|seam step %d, bed's own largest %d\n", name, pass ? "PASS" : "FAIL",
               (double)a / rate, (double)b / rate, (double)len / rate, hole, (double)xf / rate,
               quiet, worst, seam, own);
        failed += !pass;
    }
    return failed;
}

int main(int argc, char** argv)
{
    close_to(ambience_zoom_fraction(1920,1920,4200),1);
    close_to(ambience_zoom_fraction(3060,1920,4200),.5f);
    close_to(ambience_zoom_fraction(3630,1920,4200),.25f);
    close_to(ambience_zoom_fraction(4200,1920,4200),0);
    // Changed camera limits must still put the fades at the same percentages.
    close_to(ambience_zoom_fraction(5000,1000,9000),.5f);
    close_to(ambience_zoom_fraction(7000,1000,9000),.25f);
    close_to(ambience_zoom_fraction(50,50,20),1);
    close_to(ambience_zoom_fraction(35,50,20),.5f);
    close_to(ambience_zoom_fraction(27.5f,50,20),.25f);
    close_to(ambience_near_mix(1),1);
    close_to(ambience_near_mix(.5f),1);
    close_to(ambience_near_mix(.375f),.5f);
    close_to(ambience_near_mix(.25f),0);
    close_to(ambience_near_mix(0),0);
    close_to(ambience_river_mix(.5f),1);
    close_to(ambience_river_mix(.25f),.5f);
    close_to(ambience_river_mix(0),0);
    close_to(ambience_attenuation(0),1);
    close_to(ambience_attenuation(4),.5f);
    close_to(ambience_attenuation(8),0);
    assert(ambience_attenuation(12)==0);
    // A non-block-aligned stereo loop wraps exactly, preserves channel separation,
    // and continues its phase while the bus is muted.
    short pcm[] = {1000,2000, 3000,4000, 5000,6000};
    short out[4096*2];
    Mixer* m = mixer_create(); assert(m);
    int bed = mixer_play_stereo(m,MIX_BUS_AMBIENCE,pcm,3,1000,0,255,1);
    assert(bed>=0);
    mixer_bus_gain(m,MIX_BUS_FX,0,0);
    mixer_bus_gain(m,MIX_BUS_SPEECH,0,0);
    mixer_render(m,out,8);
    for(int i=0;i<16;i++) assert(out[i]==pcm[i%6]);
    mixer_bus_gain(m,MIX_BUS_AMBIENCE,0,0);
    mixer_render(m,out,8);
    for(int i=0;i<16;i++) assert(out[i]==0);
    mixer_bus_gain(m,MIX_BUS_AMBIENCE,1000,0);
    mixer_render(m,out,1);
    assert(out[0]==3000 && out[1]==4000);
    mixer_voice_fade(m,bed,0,0,50);
    mixer_render(m,out,4096);
    assert(out[8190]==0 && out[8191]==0);
    assert(mixer_voice_active(m,bed));
    // A full FX pool cannot steal a looping environment voice.
    for(int i=0;i<64;i++) mixer_play(m,MIX_BUS_FX,pcm,6,1000,0,255);
    assert(mixer_voice_active(m,bed));
    mixer_voice_stop(m,bed); assert(!mixer_voice_active(m,bed));
    // A one-shot or a pitched voice cannot become a crossfaded loop.
    int once = mixer_play_stereo(m,MIX_BUS_AMBIENCE,pcm,3,1000,0,255,0);
    assert(!mixer_voice_loop_xfade(m,once,0,3,1));
    mixer_destroy(m);
    crossfaded_loop();
    bed_body();
    if (argc > 1 && real_beds(argv[1])) { puts("AMBIENCE GATE FAIL: a shipped bed"); return 1; }
    puts("AMBIENCE GATE PASS: camera endpoints/changed limits, crossfades, river attenuation, stereo wrapping, mute phase, ramps, voice protection, crossfaded loop (equal power, no seam, exact period, muted clock), bed body excludes the recording's fades");
    return 0;
}
