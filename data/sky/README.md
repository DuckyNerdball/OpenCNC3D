# The sky the authored models are lit by

`sky_cross.webp` is a 2000x1500 horizontal cross: six 500x500 cube faces laid out as

```
       +Y
  -X   +Z   +X   -Z
       -Y
```

`game/bake_sky.py` turns it into `sky.pack`, which is a cube map plus two convolutions of
it: a roughness chain for the specular half of the lighting equation and an irradiance
map for the diffuse half. The bake is two and a half seconds and runs as part of a build,
the same way the tiberium crystals are baked, so the pack is never stale against its
source and does not have to be committed.

**The source is committed and the pack is not**, which is the opposite of the trees and is
the right way round for this one: the tree pack is committed because its source is a sixty
megabyte glTF that has no business in this repository, and this source is a quarter of a
megabyte.

**IT IS A PLACEHOLDER AND ITS PROVENANCE IS NOT ESTABLISHED.** It was supplied to stand in
while the lighting was built. Before anything ships publicly, either its licence has to be
established and recorded in `NOTICE.md` beside the other third-party art, or it has to be
replaced. The renderer does not care which sky it is handed.

**It is eight bits, so its sun is clipped flat at white.** That is why the pack supplies
ambient and reflections only and the key light stays the renderer's own analytic sun with
its own shadow map. A true high-dynamic-range sky would let the sun come out of the image
instead, and nothing in the format prevents that later.
