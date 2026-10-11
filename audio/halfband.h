/* halfband.h -- the exact 2x upsampler that carries the 1995 sound (all of it 22050 Hz)
 * into a 44100 Hz mix, shared by the bank (clips, once, at load) and the mixer (the
 * music and movie streams, as they are pushed).
 *
 * Upsampling by two keeps every source sample and fills one new sample between each
 * pair. The new one is a windowed sinc across HB_TAPS neighbours on each side:
 *
 *   between x[n] and x[n+1]:   y = sum_k HB_COEF[k] * (x[n - k] + x[n + 1 + k])   / 32768
 *
 * Kaiser window, beta 7, 24 taps a side. Measured on this exact integer table: flat
 * to within 0.002 dB up to 9 kHz and 0.00 dB at 10 kHz, and everything from 13 kHz up
 * (the mirror image of the source band) at least 73 dB down. So the 1995 material
 * leaves the upsampler as it entered it. The taps are fixed integers rather than
 * computed from sin() at start-up, so every platform produces the same samples and
 * a recording made on one can be compared bit for bit with a recording made on
 * another. Twice the sum of the table is exactly 32768: a constant input comes out
 * as the same constant.
 *
 * Header only, so no build's source list has to change to carry it. */

#ifndef HALFBAND_H
#define HALFBAND_H

#define HB_TAPS 24

static const int HB_COEF[HB_TAPS] = {
    20837, -6870, 4034, -2789, 2076, -1608, 1273, -1020, 822, -663, 533, -427,
    339,   -267,  207,  -159,  119,  -88,   63,   -44,   30,  -19,  11,  -6,
};

/* The new sample between w[0] and w[1] of a window laid out so that w[-k] and w[1+k]
 * are valid for k < HB_TAPS. Rounded, clamped to 16 bits. */
#define HB_MID(w, out)                                                                  \
    do {                                                                                \
        long hb_acc_ = 16384;                                                           \
        int hb_k_;                                                                      \
        for (hb_k_ = 0; hb_k_ < HB_TAPS; hb_k_++)                                       \
            hb_acc_ += (long)HB_COEF[hb_k_] * ((long)(w)[-hb_k_] + (long)(w)[1 + hb_k_]); \
        hb_acc_ >>= 15;                                                                 \
        (out) = (short)(hb_acc_ > 32767 ? 32767 : hb_acc_ < -32768 ? -32768 : hb_acc_);  \
    } while (0)

#endif /* HALFBAND_H */
