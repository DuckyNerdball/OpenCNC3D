# Enhanced cinematic effects

Five cinematic layers. The placement impact and the construction rumble are Soundly
recordings imported by `tools/audio/cinematic_import.py`; the three destruction layers
are original NumPy sound design from `tools/audio/make_cinematic_audio.py`, which leaves
the two recordings alone. The manifest records source, duration, level and file hashes.
Runtime WAVs are 44.1 kHz PCM16 stereo.

Placement: machinery rumble follows the actual placement rig's rise and fade;
the impact follows the camera's landing jolt, including the Yard's delayed landing.
Destruction: building collapse, structural damage and ground vehicle death each
trigger their own pressure/metal/debris impact at the corresponding shake event.
These supplement the game's normal combat effects and preserve their timing.

Enhanced only. Sound Volume controls the FX bus. Stereo follows camera projection,
and attenuation fades to silence 12 cells beyond the camera view. Hidden cells,
enemy placements, sold buildings and initial pre-damaged state don't reveal events.
Same-kind simultaneous impacts keep the strongest; a three-tick cooldown controls
stacking. Dedicated cinematic slots never steal bank, vehicle or footstep voices.
Pause, Classic, editor and mission teardown fade/stop these sounds safely.

Audibility revision: impacts are peak-normalized to 0.78, with stronger upper
impact/metal/air layers. The runtime retains the live rig envelope but uses footprint
mass for level rather than treating tiny camera displacement units as an audio volume.
New Audio may remain off; Sound Volume controls these Enhanced layers independently of
that replacement-bank switch.

Recorded revision (placement impact and construction rumble; the three destruction
layers are byte-identical to the audibility revision):

- `placement_impact.wav`, 4.1 s: `impact.wav` faded out over its last second with a
  raised cosine (the recording stops while still loud), with `impact_2.wav` from the
  same first sample under it. The sum is scaled to a -1 dBFS peak (x0.98).
- `construction_rumble.wav`, 2.6 s: `shake.wav`, raised to a -1 dBFS peak (x2.32). The
  recording fades in over its first fifth and out over its last, and a large footprint's
  rumble outlasts it, so the game loops its middle three fifths, each pass running into
  the next through an equal-power crossfade a quarter of that middle long
  (`cinematic_audio_rumble_body`, `mixer_voice_loop_xfade`).
- Runtime: a placement's impact plays at 800 (one-cell footprint) to 1100 per mille
  (four cells, where its peak just reaches full scale); the rumble at 1200 x its
  level, where the level is the camera rumble's strength against the shared preset
  times the footprint's weight (0.55 for one cell to 1.0 for four), capped at 1, times
  the rig's rise and fade.
- The duck: under the impact the rest of the mix (music, ambience and every other
  effect, never EVA or a movie) drops 8 dB within 12 ms, holds 230 ms and returns
  over 1.1 s; under the rumble it sits 4.4 dB down. The depth scales with the layer's
  distance and the Sound Volume, so a muted or distant placement ducks nothing. The
  hold runs on the mixer's own clock, so a pause or teardown never leaves it held.
