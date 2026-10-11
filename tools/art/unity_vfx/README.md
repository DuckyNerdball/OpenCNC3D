# Unity particle effects in the renderer

Unity Shuriken particle-system prefabs, drawn in the game the way Unity draws them, under
experimental art (`game/unityvfx_mod.h`, F5 > EXPERIMENTAL > Unity explosions):

- **Explosions.** Eleven prefabs from the Vefects "Explosions VFX - URP" pack replace the
  engine's 13 explosion types; which replaces which is `game/vfx3d/claims.txt`.
- **Muzzle flashes.** The 14 muzzle flashes of the Realistic Gun VFX pack play at the
  barrel a shot left, on the 3D rifleman, rocket soldier and Mammoth; which is
  `game/vfx3d/muzzles.txt`, where is `game/vfx3d/sockets.txt`.
- **Projectile trails.** Trails from both packs laid along the projectiles' own flight;
  which is `game/vfx3d/trails.txt`.

All three are chosen in the effect picker (`picker/`).

Both packs are Asset Store content: they ship inside the game and never as files. Their
product, `game/vfx3d/`, and the reference frames under `ref/` are in
`tools/public-exclude.txt`.

## The split

Unity **simulates**; the renderer **draws**.

A prefab's look depends on its noise module, its collisions and Unity's private
per-particle random streams, none of which can be matched outside Unity. So Unity steps
the prefab at 60 Hz with fixed seeds and records every live particle, for several seeds
(`Editor/OpenCNC3DVfxBake.cs`). The renderer replays the recording, and reproduces everything
after the simulation itself: geometry, the materials' shaders, URP's bloom and tonemap.

## Files

| file | what it is |
|---|---|
| `Editor/OpenCNC3DVfxBake.cs` | Unity editor script: records a prefab (`Run`), renders Unity's own reference frames, dumps Unity's baked geometry for checking |
| `uyaml.py` | reads Unity's YAML (prefabs, materials) and an unpacked `.unitypackage` |
| `c3vx.py` | reads what the bake writes |
| `recon.py`, `check_recon.py` | rebuild each render mode's geometry from the recording and check it against Unity's own baked vertices |
| `urp_post.py` | URP's Gaussian bloom and ACES tonemap, ported from the package source; compares against Unity's post-processed frames |
| `make_pack.py` | the recording, the materials and the textures into `game/vfx3d/<effect>/` |
| `ref/` | Unity's own frames (seed 0) of Generic, Generic Large and Molotov, and of the muzzle flashes and trails, used by gates G264 and G271 until both were retired with the experimental look |
| `sockets.py` | measures the gun sockets off the models: the tip of the rifle and launcher on the weapon bone, the Mammoth's barrel rings and missile tubes on its turret |
| `picker/` | the effect picker: `record_explosions.py` films the engine's explosions and every drawable Unity one in the game between a Mammoth and a rifleman (the Unity renders from `OpenCNC3DVfxBake.RenderLibrary` remain for the ones the game cannot draw); `record_guns.py` films every muzzle flash and trail on its unit; `regen.py` films approved effects as approved (size, and what a note asked for) for the page's Re-generate requests; `build_page.py` builds the page |

## Rebuilding the pack

1. A Unity 6 URP project in LINEAR colour space, NOT the one open in the editor (batch
   mode refuses a project that is open). A copy of any URP template project works.
2. Import the package into it:
   `Unity -batchmode -projectPath <proj> -importPackage "<Explosions VFX - URP.unitypackage>" -quit`
3. Copy `Editor/OpenCNC3DVfxBake.cs` into `<proj>/Assets/Editor/`.
4. Record (four seeds, 8.5 s, which covers the scorch mark's eight second life):

   ```
   Unity -batchmode -projectPath <proj> -executeMethod OpenCNC3DVfxBake.Run \
     -vfxPrefab "Assets/Vefects/Explosions VFX URP/Explosions VFX/Generic/Particles/VFX_Explosion_Generic.prefab" \
     -vfxOut <bake> -vfxSeeds 4 -vfxSeconds 8.5 -vfxRef 0 -quit
   ```

5. `python3 tools/art/unity_vfx/make_pack.py --bake <bake> --project <proj> --out game/vfx3d/<name> --texdir game/vfx3d/_textures`
   (textures go to the shared folder once; several prefabs use the same flipbooks), then
   name the pack in `game/vfx3d/claims.txt` against the engine AnimType it replaces.

Reference frames for the gate: the same `Run` with `-vfxSeeds 1 -vfxRef 1 -vfxDist 36
-vfxRefEvery 3` (add `-vfxRaw 1` for the unprocessed float frames `urp_post.py` reads).
Geometry probes: `-vfxProbe 4,12,40,120`, then `python3 check_recon.py <bake>`.

## What was measured, and why the code says what it says

- **Geometry** (`check_recon.py`): every vertex of every system within 2 mm of Unity's,
  custom-data streams exact. Found this way, not from documentation: a horizontal
  billboard is 1/sqrt(2) of its nominal size; a view-aligned billboard turns by its Euler
  rotation with the roll negated; mesh normals take the inverse transpose of the size;
  the particle colour reaches the shader linearised and in eight bits.
- **Post-processing** (`urp_post.py`): 0.08 of a level mean difference against Unity over
  six frames, with bloom scatter 0.5. The pack's profile leaves scatter at its default,
  0.7, and that is 2 to 27 levels out; 0.5 is what the project actually renders with.
- **A scaled prefab** (the Large, Small and Tiny variants are the same prefab at x2, x0.5,
  x0.3): the system's scale multiplies the particle size, except for a mesh aligned to the
  system, and nothing else; positions and velocities are used as recorded. Fitted against
  Unity's geometry for the x2 and x0.5 prefabs, every vertex within 2 cm
  (`recon.system_scale`).
- **Blending** is read from each material's own `_Src`/`_Dst`, not assumed per shader.
- **Material colours**: a property the shader declares as `Color` is converted from sRGB
  to linear before the shader sees it, a `Vector` is not. The smoke tint arrives at about
  a third of its written value. `make_pack.py` reads the declarations.
- **The whole thing** (gate G264, retired with the experimental look, so no longer run): the game's frame of one explosion from Unity's
  reference camera, against Unity's frame, 0.02 to 0.32 of a level mean difference from
  the flash to the last of the smoke, 99th percentile 2 or below.

## Two traps in the bake, both measured

- A `SingleCameraRequest` render skips URP's post-processing entirely, and an eight bit
  target makes the camera render in low range, which clips the fireball at 1.0 before
  bloom sees it. The bake uses `Camera.Render` into a half float target.
- World collisions do not run in an editor-side `Simulate`: debris fell 200 m through a
  ground with a collider. The bake points every World collision module at the ground
  plane as a Planes collision, keeping its bounce and damping.

## Guns: muzzle flashes and trails

**The shot.** The engine reports every projectile `TechnoClass::Fire_At` launches
(`EFX|FIRE`: firer, weapon, barrel, projectile and its type). A flash plays on the tick
the round leaves, small arms included.

**The socket.** `python3 tools/art/unity_vfx/sockets.py` writes `game/vfx3d/sockets.txt`
from the models: a soldier's weapon is the vertices skinned wholly to the Weapon bone,
and its muzzle is the end that points forward in the aim clip; a vehicle's barrels are
the turret part's front ring, split by side. Every drawn frame, each socketed object
leaves its sockets in world space, and a flash starts there, pointing out of the barrel,
and rides the socket while it lives. Two sockets are picked by the engine's barrel (a
first shot leaves on the left, as `TurretClass::Fire_Coord` has it), four as two a side.

**Baking a flash.** The gun pack's flashes loop (they are made for automatic fire), so:

    Unity ... -executeMethod OpenCNC3DVfxBake.Run -vfxPrefab <NS_Muzzle_*.prefab> -vfxOut <bake> \
        -vfxSeeds 4 -vfxSeconds 3 -vfxOnce 1 -vfxRef 0

`-vfxOnce 1` plays one cycle. Its systems sit centimetres along the barrel in their own
space; `make_pack.py` writes that offset (`local=`) and the renderer applies it. Reference
frames: `-vfxRef 1 -vfxRefEvery 1 -vfxDist 1.5`, and the stage takes the distance too
(`uvfxstage SEED SECONDS PACK 1.5`).

**Baking a trail.** A trail is recorded in flight:

    Unity ... -vfxPrefab <prefab> -vfxOut <bake> -vfxSeeds 4 -vfxSeconds 4 \
        -vfxMove 0,0,6 -vfxMoveStop 3 [-vfxOnly "Smoke Main"]

`-vfxMove` flies the prefab along +z (metres a second), `-vfxMoveStop` stops its
emitters. `-vfxOnly` keeps only the named systems emitting, on their own: the chaff
flare's smoke and sparks are sub-emitters of the flares it throws, and flown by
themselves they lay a trail. Every particle of the recording is replayed in the game as
a LIFE: born where the bake's emitter had flown as far, its recorded life relative to that
point, turned to the projectile's heading. The Trails module (Ribbon mode) and the
TrailRenderer component are rebuilt from the particles and from the projectile's path.

**The gun pack's shaders** are Shader Graphs; `OpenCNC3DVfxBake.DumpShaderGraphs` writes the
shader text Shader Graph generates for each, and the five used by flashes and trails are
ported from that text (`UVFX_FS_FUMES`, `_SMOKEM`, `_SMOKETRAIL`, `_SPARK`, `_EMBER`).

## Three more things measured

- **An HDR colour is not linearised.** A plain Color property reaches the shader
  converted from sRGB, an HDR one as stored (the flash's `_EmissiveColor`): converted, the
  glow came out red where Unity's is orange, 77 levels out at the 99th percentile
  against 28 without, and 2 once the next item was found.
- **The reference project's default volume profile is not part of the stage.** The gun
  pack ships its own `Assets/Settings/DefaultVolumeProfile.asset`, with +0.5 EV of post
  exposure; importing it made every later reference frame exactly sqrt(2) brighter. The
  stage pins post exposure at 0. The 14 flashes then match to 0.08 to 0.42 of a level.
- **Bloom scatter 0.5 is the render pipeline asset's** (`Assets/Settings/SampleSceneProfile`,
  which also asks for high quality filtering); fitted here before it was found. The
  plain filter fits Unity's frames better than the high quality one, so it stays.
