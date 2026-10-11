#!/usr/bin/env python3
"""Bring replacement sounds into data/newaudio/, all at one loudness.

    tools/audio/newaudio_import.py SOURCE_DIR            import every NAME.wav / NAME_n.wav
    tools/audio/newaudio_import.py SOURCE_DIR --probe    measure only, change nothing

Each file is named after the 1995 sound it replaces (BOMB1.wav, BOMB1_2.wav). For each:

  1. read it (integer PCM 16/24/32 bit or 32 bit float, any rate, any channel count)
     and fold it to mono by averaging the channels, because the game places every
     effect in the stereo field itself;
  2. convert it to 44100 Hz with the system's sample-rate converter (afconvert, at its
     highest quality), keeping 32 bit float so nothing clips on the way;
  3. measure its loudness and its peak, and apply ONE gain that puts its loudness at
     TARGET_LUFS;
  4. write 16 bit mono 44100 Hz, the format the game plays without resampling.

LOUDNESS is ITU-R BS.1770: K-weighted (a high shelf at 1.5 kHz and a high pass at 38 Hz,
which is roughly how the ear weights energy), mean square over a 400 ms window, stepped
every 100 ms. The figure used is the LOUDEST window, the momentary maximum, not the
average over the whole file: a sound effect is judged by its hit, and an average would
make a sound with a long quiet tail read as quiet and be pushed up until its hit was far
too loud.

TARGET_LUFS IS FIXED HERE, NOT WORKED OUT PER BATCH, so every batch lands at the same
loudness as the ones before it. It was set from the first batch as the loudest level
every file in it can reach with its peak at or below PEAK_CEILING_DBFS: the file with
the most headroom-hungry transient decides it. A later file that cannot reach it without
clipping is written at the loudest level it CAN reach and reported, rather than clipped
or limited; --probe shows that before anything is written.

Also writes data/newaudio/levels.csv: every file's measured loudness and peak before,
the gain applied, and its loudness and peak after, so the balance is a record rather
than an assumption.
"""

import csv
import difflib
import glob
import math
import os
import re
import struct
import subprocess
import sys
import tempfile

import numpy as np

TARGET_LUFS = -20.0        # momentary maximum, LUFS; see the docstring. The first batch
                           # could all reach -19.9 unclipped, the three MGUN2 takes
                           # deciding it: sharp transients, peak near full scale at
                           # only -19 LUFS. The 1995 sounds they replace measure a
                           # median -13.3 on the same meter, so replacements play
                           # about 7 dB below them.
PEAK_CEILING_DBFS = -1.0   # sample peak after gain
RATE = 44100

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_DIR = os.path.join(REPO, "data", "newaudio")


def read_wav(path):
    """(samples[frames, channels] as float in -1..1, rate). Refuses what it does not know."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file")
    pos, fmt, body = 12, None, None
    while pos + 8 <= len(data):
        cid, clen = data[pos:pos + 4], struct.unpack_from("<I", data, pos + 4)[0]
        chunk = data[pos + 8:pos + 8 + clen]
        if cid == b"fmt ":
            tag, ch, rate = struct.unpack_from("<HHI", chunk, 0)
            bits = struct.unpack_from("<H", chunk, 14)[0]
            if tag == 0xFFFE and len(chunk) >= 26:
                tag = struct.unpack_from("<H", chunk, 24)[0]
            fmt = (tag, ch, rate, bits)
        elif cid == b"data":
            body = chunk
        pos += 8 + clen + (clen & 1)
    if fmt is None or body is None:
        raise ValueError("no fmt or data chunk")
    tag, ch, rate, bits = fmt
    if tag == 3 and bits == 32:
        x = np.frombuffer(body, dtype="<f4").astype(np.float64)
    elif tag == 1 and bits == 16:
        x = np.frombuffer(body, dtype="<i2").astype(np.float64) / 32768.0
    elif tag == 1 and bits == 24:
        b = np.frombuffer(body[: len(body) // 3 * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = np.where(v >= 1 << 23, v - (1 << 24), v).astype(np.float64) / 8388608.0
    elif tag == 1 and bits == 32:
        x = np.frombuffer(body, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError("unsupported WAV: format %d, %d bit" % (tag, bits))
    return x[: len(x) // ch * ch].reshape(-1, ch), rate


def write_float_mono(path, x, rate):
    body = x.astype("<f4").tobytes()
    fmt = struct.pack("<HHIIHH", 3, 1, rate, rate * 4, 4, 32)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 4 + 8 + len(fmt) + 8 + len(body)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<I", len(fmt)) + fmt)
        f.write(b"data" + struct.pack("<I", len(body)) + body)


def write_pcm16_mono(path, x, rate):
    q = np.clip(np.round(x * 32767.0), -32768, 32767).astype("<i2")
    body = q.tobytes()
    fmt = struct.pack("<HHIIHH", 1, 1, rate, rate * 2, 2, 16)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 4 + 8 + len(fmt) + 8 + len(body)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<I", len(fmt)) + fmt)
        f.write(b"data" + struct.pack("<I", len(body)) + body)


def biquad(x, b, a):
    b0, b1, b2 = (v / a[0] for v in b)
    a1, a2 = a[1] / a[0], a[2] / a[0]
    y = np.empty_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, xi in enumerate(x):
        yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, xi, y1, yi
        y[i] = yi
    return y


def k_weight(x, rate):
    """BS.1770 pre-filter, designed for the actual rate rather than the 48 kHz table."""
    g, q, fc = 4.0, 1 / math.sqrt(2), 1500.0
    A, w0 = 10 ** (g / 40), 2 * math.pi * fc / rate
    al, c = math.sin(w0) / (2 * q), math.cos(w0)
    shelf = ([A * ((A + 1) + (A - 1) * c + 2 * math.sqrt(A) * al), -2 * A * ((A - 1) + (A + 1) * c),
              A * ((A + 1) + (A - 1) * c - 2 * math.sqrt(A) * al)],
             [(A + 1) - (A - 1) * c + 2 * math.sqrt(A) * al, 2 * ((A - 1) - (A + 1) * c),
              (A + 1) - (A - 1) * c - 2 * math.sqrt(A) * al])
    q, fc = 0.5, 38.0
    w0 = 2 * math.pi * fc / rate
    al, c = math.sin(w0) / (2 * q), math.cos(w0)
    hp = ([(1 + c) / 2, -(1 + c), (1 + c) / 2], [1 + al, -2 * c, 1 - al])
    return biquad(biquad(x, *shelf), *hp)


def momentary_max(x, rate):
    y = k_weight(x, rate) ** 2
    win, hop = int(0.4 * rate), int(0.1 * rate)
    if len(y) < win:
        y = np.concatenate([y, np.zeros(win - len(y))])
    c = np.concatenate([[0.0], np.cumsum(y)])
    starts = np.arange(0, len(y) - win + 1, hop)
    ms = (c[starts + win] - c[starts]) / win
    return -0.691 + 10 * math.log10(max(ms.max(), 1e-20))


def known_names():
    """Every name the game can ask the bank for: the engine's sound and speech tables
    (audio/sfxtable.c, generated from the 1995 source) and the sounds the campaign
    screens and the renderer play by name. A file named anything else never plays."""
    names = set()
    t = open(os.path.join(REPO, "audio", "sfxtable.c"), encoding="latin-1").read()
    names |= set(re.findall(r'\{"([A-Z0-9_]+)",', t))
    names |= set(re.findall(r'^\s*"([A-Z0-9_]+)",', t, re.M))
    for f in ("app/campaign.c", "game/cnc_eyes.cpp"):
        names |= set(re.findall(r'"([A-Z0-9_]+)\.AUD"', open(os.path.join(REPO, f), encoding="latin-1").read()))
    return names


def db(v):
    return 20 * math.log10(max(v, 1e-20))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    src, probe = sys.argv[1], "--probe" in sys.argv
    files = sorted(glob.glob(os.path.join(src, "*.wav")) + glob.glob(os.path.join(src, "*.WAV")))
    if not files:
        print("no .wav files in %s" % src)
        return 1
    names = known_names()
    unknown = []
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0].upper()
        base = re.sub(r"_\d+$", "", name)
        if base not in names:
            near = difflib.get_close_matches(base, sorted(names), n=1)
            unknown.append("%s (did you mean %s?)" % (name, near[0]) if near else name)
    if unknown:
        # A misnamed file would be imported, committed and shipped, and then never play,
        # with nothing anywhere to say so. Refused instead, all of them named at once.
        print("REFUSED: these names are not sounds the game plays, so they would never be heard:")
        for u in unknown:
            print("   " + u)
        print("Rename them to the 1995 name (with _2, _3 for variants) and run again.")
        return 1
    rows, reach = [], []
    with tempfile.TemporaryDirectory() as tmp:
        for path in files:
            name = os.path.splitext(os.path.basename(path))[0].upper()
            x, rate = read_wav(path)
            mono = x.mean(axis=1)
            a, b = os.path.join(tmp, "in.wav"), os.path.join(tmp, "out.wav")
            write_float_mono(a, mono, rate)
            if rate != RATE:
                subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEF32@%d" % RATE, "-r", "127",
                                "--src-complexity", "bats", a, b], check=True)
                y, _ = read_wav(b)
                y = y[:, 0]
            else:
                y = mono
            loud, peak = momentary_max(y, RATE), db(np.abs(y).max())
            best = loud + (PEAK_CEILING_DBFS - peak)   # the loudest this file can go
            reach.append((best, name))
            gain = TARGET_LUFS - loud
            short = ""
            if peak + gain > PEAK_CEILING_DBFS:
                gain = PEAK_CEILING_DBFS - peak
                short = "%.1f dB SHORT of target: its peak would clip" % (TARGET_LUFS - (loud + gain))
            z = y * 10 ** (gain / 20)
            after_loud, after_peak = momentary_max(z, RATE), db(np.abs(z).max())
            rows.append([name + ".wav", "%.1f" % loud, "%.1f" % peak, "%+.1f" % gain, "%.1f" % after_loud,
                         "%.1f" % after_peak, "%.2f" % (len(z) / RATE)])
            print("%-12s %2d ch %6d Hz  loud %6.1f LUFS peak %5.1f dBFS  gain %+5.1f dB -> %6.1f LUFS, peak %5.1f dBFS %s"
                  % (name, x.shape[1], rate, loud, peak, gain, after_loud, after_peak, short))
            if not probe:
                write_pcm16_mono(os.path.join(OUT_DIR, name + ".wav"), z, RATE)
    best, who = min(reach)
    print("\nthe loudest level every file here can reach unclipped: %.1f LUFS (%s decides it); target %.1f"
          % (best, who, TARGET_LUFS))
    if probe:
        return 0
    have = {}
    csvp = os.path.join(OUT_DIR, "levels.csv")
    if os.path.exists(csvp):
        with open(csvp) as f:
            for r in list(csv.reader(f))[1:]:
                have[r[0]] = r
    for r in rows:
        have[r[0]] = r
    with open(csvp, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["file", "source_lufs", "source_peak_dbfs", "gain_db", "lufs", "peak_dbfs", "seconds"])
        for k in sorted(have):
            w.writerow(have[k])
    print("wrote %d files and %s" % (len(rows), os.path.relpath(csvp, REPO)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
