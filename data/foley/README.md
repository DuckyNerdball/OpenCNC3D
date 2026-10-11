# Infantry grass footsteps

Grass Foot Step Sounds (Yo Frankie!) by Blender Foundation, submitted by Lamoot:
https://opengameart.org/content/grass-foot-step-sounds-yo-frankie
Licensed under Creative Commons Attribution 3.0 Unported:
https://creativecommons.org/licenses/by/3.0/

Original left/right recordings are preserved in `source/`. Runtime variants have
DC removed, matched levels, endpoint fades, stereo duplication and two subtle
speed and brush/sole balance variations, ten clips total. No synthetic grit or metallic buckles are mixed into them.
Attribution and source/license links must remain with redistributed versions.

Regenerate with `python3 tools/audio/make_infantry_foley.py` (NumPy and ffmpeg).
`manifest.json` records source/runtime hashes and processing.
Gait contacts trigger playback while soldiers actually run; the close-camera gain
is 100%, falls to 25% at 50% zoom, and stays 25% through maximum zoom out.
World distance and camera-relative stereo apply. Sound Volume controls these effects.

Each new footfall independently selects a clip, pitch ratio 0.900–1.100 and volume
multiplier 0.900–1.100, using the private audio RNG. These factors remain fixed
for that step as camera zoom, distance and pan change. Pitch uses stereo linear
interpolation in the mixer; unity-rate playback of other sounds is unchanged.
