# Tier 1 gap list (what the Windows 98 build gives up)

Two render tiers, see repository rules rule 1. Tier 2 is the modern desktop build and may use
shaders freely. Tier 1 is Win98 with a 3dfx Voodoo 2 through Glide: fixed function,
no shaders, no render targets, 640x480, 16-bit colour.

The experimental Construction Yard (`FACT.c3m`, 24 Sep 2026) is Tier 2 only, like
the authored Mammoth. Tier 1 uses the existing cartridge Construction Yard and its
construction/damage animation. The modern Classic path and disabled Experimental
Art switch also retain it; no PBR shader or new mesh is required on Win98.

**This file is the contract.** Any feature that cannot run on Tier 1 belongs here, with
its Win98 fallback, before it ships. A feature that is Tier 2 only and NOT listed here is
a bug, because it converts the port from a known list of compromises into a surprise.

## Current entries

| Feature | Tier 2 | Tier 1 fallback |
|---|---|---|
| Texture wrap: clamp | `GL_CLAMP_TO_EDGE` (x4 sites, plus one in `game/dosopt_gl.h`) | `GL_CLAMP`; one texel of edge bleed, acceptable |
| Texture wrap: mirror | `GL_MIRRORED_REPEAT` (x1) | pre-mirrored texture baked at pack time, `GL_REPEAT` |
| Shatter: buildings, vehicles and aircraft (pieces fly, roll and settle) | a killed structure's display-list sections become rigid bodies: radial launch from the footprint centre, gravity, bounce, downhill roll on the PK9 heightfield, settle, linger, then sink into the ground while fading. `game/shatter_mod.h`. **Classic keeps it for every structure, the Power Plant included.** Under Enhanced, a building type with an Enhanced building file (every approved remastered structure, `game/ebld/<TYPE>.ebld`) does not shatter this way: `game/ebld_mod.h` owns that building, the Power Plant's own collapse replaces the shatter, every other one keeps its broken pieces and sinks them over a scorch mark, while the pulse and the death shake still fire (see "The Enhanced buildings" below) | the section SHED alone (G90): the building disassembles over its eight-tick death window in the display list's own order and is gone. No flying pieces. **A VEHICLE GETS NOTHING AT ALL on Tier 1**, and cannot get the shed either: a vehicle has no death window to shed across -- measured, a Jeep goes from `str=12/150` straight to MISSING in one tick -- so Win98 keeps exactly what it has today, the cartridge's seven generic VehicleDamage debris chunks (DBRV0..6, baked and already firing: a dying Jeep spawns 129 particles and 15 chunks). That is the cartridge's own answer to a dying vehicle and it is unchanged. NOT a fidelity loss -- the cartridge has no authored building collapse either, it swaps to FRAG3 debris and that is all. **The blocker is not shaders and not the per-vertex 3x4** (12 multiplies on a vertex that already pays a bilinear shroud lookup); it is COUNT x DURATION x DRAW CALLS -- a full 128-piece pool is up to 384 separate `draw_mesh` calls and 128-plus texture binds per frame, for five to six seconds after the buildings are already gone. **The centre PULSE is excepted and ships on both tiers**: one particle in the existing sprite pool, fired above the shatter's own early-outs so switching the shatter off does not take it too. |
| Damaged building overlay | the cartridge's own per-building damage display list appended over the intact model when `Health_Ratio() < 0x80`: 2 to 78 triangles, one or two CI8 textures. BUILT 24 Aug 2026, decoded in `docs/damage-textures.md` | **SHIPS ON TIER 1, no gap.** Measured worst case if all 142 structures on the largest shipped map were visible and below half health at once: 2428 extra triangles, 7285 host vertex transforms, 568 `grTexSource` binds per frame, and 57.0 KB of texture against a 2 MB TMU. Realistic load is one to two orders of magnitude under that. No shader, no render target, no new state. The one Tier 1 detail is the z-fight epsilon, and the answer is the mesh-space Y bias already used at `cnc_eyes.cpp:5568`, not `grDepthBiasLevel`. |
| Landed-helicopter rotor rate (the cartridge's k=1 / k=2 rule) | `rotor_spin(o)` halves the sweep while `o.alt == 0`, so a helicopter on a pad or on the ground turns idling blades at exactly half the airborne rate. The 2:1 ratio is the console's, from RAM 0x80097A50 (k=2) and RAM 0x80097A4C (k=1); the absolute rate is ours. | NONE today, and the blocker is a MISSING FIELD, not the Voodoo. `tier1/w98_glidegame.c:1935` drives every rotor from one clock, `g_mesh.rotor = (ticks * 60) & 255`, with no altitude test, because `W98_Object` (`tier1/w98_brain.h:32`) carries no altitude and `wb_parse` (`tier1/w98_brain.c:820-843`) never reads `|alt=` -- although both brains already emit it (`dllinterface.cpp:8285` vanilla, `:8234` XL). The fix is three lines whenever Win98 is next touched: an `int alt;` in the struct, one `field_int(line, "|alt=", 0)`, and a conditional on the multiplier at :1935. No shader, no render state and no Glide call is involved. |

| Network multiplayer | deterministic lockstep over UDP: peers exchange 22-byte `EventClass` orders through a star, with a rendezvous for address exchange, hole punching, and a relay for the pairs that cannot punch through. 8 players on the classic brain, 16 to 24 on the Enhanced engine | **NONE. TIER 2 ONLY.** Not a Winsock limit, and that is worth stating because it is the guess everyone makes: Windows 98 SE ships Winsock 2.2, plain UDP is all a lockstep order stream needs, and hole punching requires no encryption, so the sockets themselves would be fine. Three other things rule it out, any one of them sufficient. **(1) Tier 1 is a separate program.** `tools/win98/build.sh` builds `w98glide.exe` out of `tier1/*.c` and never compiles `game/cnc_eyes.cpp`, so parity would mean writing the lobby, the turn scheduler and the transport twice, in two codebases that share no code. **(2) Lockstep runs at the pace of the slowest peer**, because every machine simulates every player's army; a Pentium-class box would set the frame rate for everyone else in the match. **(3) 16 to 24 players requires the Enhanced engine**, which never ships to Windows 98, so large matches are inherently Tier 2 whatever the transport does. **What stays possible and is not planned:** a LAN and direct-IP-only Tier 1 build, which needs no rendezvous, no relay and no encryption, and which is exactly what the 1995 game shipped. It would cost a session or two once the transport seam exists, and it is not promised. The menu already tells the truth today: `menu/dosmenu.c:62` marks Multiplayer disabled and `tier1/t1_menu.c:103` refuses the click. |

## The presentation chain (added 19 Aug 2026)

The desktop-only post chain: `game/fx_gl.h`, `fx_state.h`, `fx_post.h`, `fx_panel.h`,
plus `fx_filter.h` which is not part of the chain and is listed separately below.

**The whole chain is one row, because it has one fallback: it does not run.** With
`--gfx` absent and F5 never pressed the program draws exactly the picture it drew
before any of this existed, which gate G36 measures and the other 43 gates assume. On
Tier 1 the master switch is simply never on. Nothing about the Glide backend has to
know the chain exists.

| Feature | Tier 2 | Tier 1 fallback |
|---|---|---|
| Offscreen render target (`GL_EXT_framebuffer_object`, RGBA8 + a sampleable depth texture) | the world is drawn into it and the chain reads it back | none. The Voodoo 2 has no render target; the world goes straight to the front buffer as it does today |
| GLSL 1.20 fragment programs (six of them) | shadow/occlusion/light, bright pass, blur, resolve, CRT, copy | none. Fixed function only |
| Supersampling | world drawn at up to 4x the window, box filtered down | none. The Voodoo 2 renders 640x480 and that is the resolution. Note the console DID anti-alias (RDP coverage plus the VI filter), so this is a gap on Tier 1 against the cartridge as well as against Tier 2 |
| Sun shadow map (depth-only FBO, screen-space application) | replaces the cartridge's four baked shadow mechanisms while it is on | the four baked mechanisms, which is what ships today and is what the console does |
| Terrain shadow strength and terrain brightness/contrast (F5 Terrain) | independent terrain-caster depth plus shaded-slope strength and bounded baked cliff-shade softening at non-neutral shadow strength; terrain-class colour adjustment before lighting; all neutral at 1 | absent with the Tier 2 post chain; retain original baked terrain lighting and colours |
| Sun shadow map: the ground as a caster (`draw_terrain_depth`, `fx_draw_shadow_casters`) and the reflection's ground culled to the mirror's box | the ground goes into the depth map as bare textured triangles with the water-hole alpha cut, in one draw from a static vertex buffer built once per map and rebuilt when a pad or the atlas changes (client arrays while the editor is open or without buffer objects), instead of the full lit ground; the water reflection draws only the cells that can land in it | performance only, no visual gap: Tier 1 has no sun map and no reflection |
| Ground-normal target: the flat ground cells from a static vertex buffer (`ground_normals_static`, `fx_draw_terrain_normals`) | the same triangles the immediate loop drew, held in a vertex buffer with float colours and drawn in runs of on-screen cells; the relief tables built once per frame instead of once per call; the shroud corner field rebuilt only when it is dirty | performance only, no visual gap: Tier 1 has no normal target, and the immediate loop stays as the fallback |
| Frame profiler (`OPENCNC3D_FRAMEPROF=1`, `fprof_mark` / `fprof_end`) | per-pass CPU milliseconds on the log, off unless the variable is set | performance tooling only, no visual gap; plain C timing that a Tier 1 port could keep |
| Bloom, ambient occlusion, dynamic light, colour grade, CRT | shader passes | none, and none of them is a fidelity loss: the cartridge has no equivalent for any of them |
| Experimental temperate rock displacement and normals | a GLSL vertex shader samples grayscale height on a 24x24 tile mesh, submitted as an indexed client array over one shared 25x25 lattice; a normal pass lights rock pixels with roughness 0.82 and metallic 0 | no vertex texture lookup or post light material; keep the original flat terrain and its normal lighting |
| **VI output gamma** | `pow(c, gamma)` in the resolve pass | **THIS ONE IS A REAL LOSS.** `VI_CTRL = 0x0000320E` sets GAMMA_ON on the console and nothing clears it, so Tier 1 without it is less faithful than Tier 2 with it. A Glide answer exists and is not written: `grGammaCorrectionValue()` sets the hardware gamma table on a Voodoo 2. It is one call and it belongs in the Glide backend when that is built |

### `fx_filter.h`: bilinear filtering, and it is NOT a Tier 2 debt

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| One switch flipping every texture drawn IN THE WORLD between `GL_NEAREST` and `GL_LINEAR`: the 3D pack art and terrain atlas, the DOS infantry sprites, the tiberium, the decals, the construction scaffold. **Never the UI** -- not the sidebar, cameos, fonts, cursors, pause dialog, menus or movies | `glTexParameteri` over a register of every uploaded WORLD texture; UI textures are simply never registered, so the switch has no way to reach them | **the Voodoo 2 filters bilinearly in hardware and has since 1996.** `grTexFilterMode(GR_TEXTUREFILTER_BILINEAR)` per TMU, set per texture source rather than globally. This is one of the few additions Tier 1 can simply do |

Worth recording alongside it: the console's RDP filtered its textures bilinearly, so
point sampling is OUR choice, not the cartridge's. Whichever way the project owner sets this switch
it is a decision, and the default (off, point sampled) is the one that keeps every
existing gate's pixels where they were.

## Nothing else is Tier 2 only yet

Everything currently shipped (soft shroud, DOS sprites, particle effects, turret
rotation, the sidebar, the menu, the movies and the pause dialog) runs in fixed
function OpenGL 1.1 and maps to Glide.

The movie player (`video/movieplay.c`) is one `GL_NEAREST` texture and one quad, with
the palette conversion and the fade done on the CPU exactly as the DOS original faded
the VGA palette. The pause dialog (`game/dosopt.c` + `game/dosopt_gl.h`) is one 8-bit
surface, one texture and one quad, drawn with the same `dosbar.c` primitives as the
sidebar. Neither needs anything the Voodoo 2 does not have.

## Audio (added with the sound round)

The audio engine is under the same contract and currently costs Tier 1 **nothing**,
because nothing in the mixer touches a platform API.

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| Mixer, `.AUD` decoders, MIX reader, sound bank, theme playlist, the WAV tap | plain C89, no platform call, no C++ | the identical files, handed to a different C compiler |
| The device | `audio/audio_sdl.c`: an SDL2 callback at 22050 Hz stereo, 1024 frame buffer | **a new file with the same five functions.** A DirectSound secondary buffer or a `waveOut` double buffer, calling `mixer_render()` at the refill point; `audio_backend_lock/unlock` become a critical section, or nothing at all if the refill happens on the main loop |
| Music streaming | `cnc_audio_update()` decodes on the GAME thread and pushes into a ring the device only reads | unchanged, and it is the whole reason a Win98 backend does not have to make stdio interrupt safe |
| Decoded clip cache | 8 MB LRU cap | unchanged. The cap exists precisely for this target: decoding all 249 sounds at once is about 12 MB, which is fine on a modern machine and is not fine on Win98 |

**`audio/audio_win98.c` IS WRITTEN NOW, and it is a new file, not a
rewrite.** It implements the same six functions `audio_sdl.c` does and touches nothing
else, over `waveOut` rather than DirectSound: winmm.dll is on every Windows 98 install
as shipped, where DirectSound would add a DirectX runtime to require, detect and fall
back from, and it buys nothing when the mixer already hands the device a finished stereo
stream.

The one delicate part is the threading, and it is written down in the file: a waveOut
callback runs in an interrupt-like context where Microsoft names the only legal calls,
and `mixer_render` is not among them. So the callback does exactly one legal thing,
`SetEvent`, and a worker thread refills and re-queues the four buffers. That makes
`audio_backend_lock/unlock` a CRITICAL_SECTION around the render, which is precisely the
guarantee `SDL_LockAudioDevice` gives on the other backend.

**What is proven and what is not.** It compiles as 32-bit Windows code with `-Wall
-Wextra` and no warnings, and it links against the whole audio core with `-lwinmm`,
which is what says the seam is satisfied rather than merely typed. **It has never run on
Windows 98**, because the renderer's own Win98 and Glide port has not started, so there
is no build on that machine to hear it. CI compiles it on every push, alone, so that it
cannot rot while it waits.

**A REAL Tier 1 finding fell out of that link, and it is not about audio.** The probe
binary imports `api-ms-win-crt-*.dll`: that is the UCRT, which Homebrew's mingw-w64
links by default and which **does not exist on Windows 98**. Ubuntu's mingw-w64 (what CI
uses) is the MSVCRT variant and imports `msvcrt.dll`, which does. `known-gap notes`
already records that the two toolchains differ and that both run on Windows 11; what is
new is that the difference is load-bearing for Tier 1. **The Win98 build must be made
with the MSVCRT variant**, whichever machine makes it, and that is a toolchain decision
to settle before the port starts rather than a link error to discover during it.

## Construction animation (added with the buildup round, 14 Aug 2026)

The 1995 buildup pass costs Tier 1 **nothing** by construction: `dosmake.pack` sheets
are all 256x256 or smaller (the baker PAGES a long strip across several sheets rather
than exceeding the Voodoo 2 texture limit -- TMPL's 36 frames span 4 sheets), the
loader uses GL 1.1 calls only (`GL_CLAMP`, no new tokens, same audited wrap-ledger
reasoning as the infantry loader), and the draw is one alpha-tested quad per building
under construction. The only Tier 1 cost is one texture bind per constructing
building per frame, on a 5-second one-shot animation.

## Wave 4: water, per-object shroud, health bars

Everything in this round is fixed-function and lands inside the Tier 1 contract. The one
item that adds a hardware requirement is the water, and the Voodoo 2 is the machine it was
designed for.

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **Soft decal edges** (`decal_soft`, 21 Aug 2026) | A box blur of the smudge sheet's ALPHA channel at load, per frame cell, then `GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA` with a floor-level alpha test instead of the 0.5 test | **NATIVE, and it is not even a compromise.** The blur is a CPU pass over 270 KB at load, which the Pentium-class target does once. The draw needs `grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA)` plus `grAlphaTestFunction(GR_CMP_GREATER)` at a low reference, which is one state change and exactly what the console's own FORCE_BL composite does. The alpha test path is kept for `decal_soft 0` and is the cheaper of the two, so a Voodoo 2 that wants the frames back can have the cartridge's hard edge for free. **This is a DEVIATION, not a fidelity feature:** the console draws these hard, and 0 restores it byte for byte |
| **Water surface** | Two texture units, both `GL_MODULATE`, primary colour `(1,1,1,170/255)`, one `GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA` blend, depth write off, `GL_LEQUAL` | **Native.** The Voodoo 2 has TWO TMUs: `grTexCombine` MODULATE on TMU0 against TMU1's output, iterated (Gouraud) alpha, `grAlphaBlendFunction(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)`, `grDepthMask(FXFALSE)`, `GR_CMP_LEQUAL`. This is the card's home ground rather than a compromise. Watch item only: 16-bit colour will band the WATER1 x WATER2 product slightly, which is a 16-bit fact and not a feature loss. **If a single-TMU fallback is ever needed it is NOT the console's lerp** and must be registered as a deviation, not shipped quietly. |
| **Water UV warp** | Two `sinf` and two `cosf` per water corner per frame, on the CPU | Unchanged. SCG01EA has 389 water cells, so under 1600 corners per frame; a Pentium-class box handles that. If it is ever tight, a 256-entry sine table matches the console's own quantisation exactly (it truncates S/T to s16, i.e. 1/96 of a repeat). |
| **Seabed (BOTTOM.IMG)** | One extra opaque textured pass over water cells only, single TMU, `GL_REPEAT` on a 32x32 tile | Unchanged, and it is CHEAPER than the water pass above it. |
| **Coincident water/terrain depth** | Depth write off plus `GL_LEQUAL` on surfaces that share vertices | Same on Glide. Watch item: 16-bit Z precision at 640x480 on coincident surfaces. The console faces the identical situation and solves it the identical way, so if it sparkles the answer is a depth-range tweak, **not** `glPolygonOffset`, which Glide does not have. |
| **Per-object shroud darkening** | One bilinear lookup per vertex, subtracted from the vertex colour before submission | Unchanged: this is per-VERTEX COLOUR arithmetic done on the CPU, which is exactly what fixed function can consume. No shader, no second pass, no render target. The only Tier 1 cost is the lookup itself, and it is four array reads and a lerp. |
| **Vehicle washboard lean** | Two `sinf` per drawn vehicle per frame plus a 3x3 rotate folded into the existing per-vertex transform | Unchanged. Pure CPU arithmetic on geometry we already transform by hand. |
| **World-space health bars** | Two untextured quads per bar, depth tested and written, no blend | Unchanged, and strictly cheaper than the screen-space overlay it replaced (which needed an ortho projection switch per frame). |

Nothing in this round introduced a shader, a render target, a new GL token or a texture
larger than 256x256.

## Wave 5: the MCV deploy rig

Nothing here introduces a shader, a render target, a new GL token or a texture larger than
256x256. The rig rides the PKB node-animation path that already existed for the idle
clips, so its Tier 1 answer is that path's answer.

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **Per-node animated draw (`draw_mesh` animT)** | Per drawn part, one lerp between two baked 3x4 matrices, then a 3x4 multiply per vertex. Immediate-mode `glVertex3f`, no VBO, no shader | **Unchanged.** The vertices are transformed on the CPU already, in the same loop that applies facing, mount and lean; the animation is one more 3x4 before `grDrawTriangle`. Cost is bounded by the rig's 278 triangles and it draws for at most one object at a time. |
| **Baked clip storage** | 102 frames x 21 parts x 12 floats = 103 KB of `float` per pack, resident | **Watch item, not a blocker.** A Win98 target machine has this, but it is the largest single animation block in the pack and a 16 MB build should measure the pack's total resident size before adding more one-shot rigs. Halving it to `short` fixed-point is available and was NOT done, because nothing needs it yet. |
| **The rig REPLACES the yard's mesh** | One `draw_mesh` instead of another, same three passes | Unchanged: strictly fewer triangles than the finished Construction Yard it stands in for. |

## Wave 5: the cursor animations

Nothing here introduces a shader, a render target, a new GL token or a texture larger than
256x256. The ten animated cursor states reach the screen through two paths that already
had Tier 1 answers, so this section is mostly a statement that nothing new was spent.

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **Animated cursor nodes (codes 0x03, 0x04, 0x06, 0x08, 0x09)** | The PKB node-animation path: one lerp between two baked 3x4 matrices per part, then a 3x4 multiply per vertex, immediate mode | **Unchanged**, and it is the cheapest user that path will ever have: the whole cursor is 5 parts and at most 65 triangles, drawn once per frame. The clock is an integer multiply and a modulo. |
| **Baked cursor clips** | 100 frames x 5 parts x 12 floats = 24 KB per animated cursor, 5 of them, 120 KB per pack resident | **Watch item, not a blocker**, and it is the same watch item the MCV rig already raised: the packs' total resident animation is now about 230 KB. The same `short` fixed-point halving is available and again was not done because nothing needs it. |
| **Flipbook cursors (0x0A, 0x0B) as per-frame mesh variants** | 7 extra meshes (4 x 124 and 3 x 22 triangles) and 7 extra textures, at most 8x8 and 2x6 texels | **Native, and deliberately so.** The alternative -- rebinding a texture mid-mesh from a per-frame index table -- would also have worked on Glide, but the variant meshes need NO new mechanism at all: the renderer picks a mesh index and draws it, which is what it does for every other model. The textures are 256 and 24 bytes of RGBA each; `grTexDownloadMipMap` will not notice them. |
| **`cursor3dstate` / `cursor3danim` script verbs** | Test-only, no draw cost | Not shipped behaviour: they change which of the cartridge's own 20 state rows is used, and both default to the engine's answer. No Tier 1 surface at all. |


## Wave 5: the terrain CM tint layer

**This is the one place so far where the Voodoo 2 is BETTER than desktop GL fixed
function, and the answer is that it can express the console's combiner exactly, in one
TMU and one pass.** That is worth stating plainly because the reflex on reading
"subtract before modulate" is to assume a Tier 1 gap, and there is none here.

The cartridge's ground combiner (G_SETCOMBINE 0xFC15982B / 0x4433FFFF at ROM 0x19A5AC,
G_CYC_1CYCLE at ROM 0x19C3C0) is, per channel:

```
out = clamp8( ((TEXEL0 - tint) * lit + 0x80) >> 8 )
```

tint = the vertex RGB (the CM layer), lit = the vertex ALPHA (the per-corner light).

**It cannot fold into the vertex colour.** `GL_MODULATE` gives `T * C`, and matching the
console needs `C = lit/256 - (tint*lit)/(256*T)`, a function of the TEXEL. The subtracted
term is an offset, not a scale, so no vertex colour reproduces it on more than one texel
value. Both tiers therefore need a combine form, not a colour.

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **The subtract-then-modulate itself** | TWO texture env stages. Unit 0 `GL_COMBINE` / `GL_SUBTRACT(TEXTURE, PRIMARY_COLOR)`; unit 1 `GL_COMBINE` / `GL_MODULATE(PREVIOUS, PRIMARY_COLOR` with `GL_SRC_ALPHA` as operand 1`)`. Unit 1 samples nothing and is bound to the same atlas only because an env stage must be enabled to apply. New GL tokens: `GL_COMBINE`, `GL_SUBTRACT`, `GL_PREVIOUS`, `GL_PRIMARY_COLOR`, `GL_SOURCE0/1_RGB`, `GL_OPERAND0/1_RGB`, `GL_COMBINE_RGB/ALPHA`, `GL_SOURCE0_ALPHA`, `GL_OPERAND0_ALPHA` (GL 1.3 core / `ARB_texture_env_combine`) | **NATIVE, exactly, in ONE TMU.** Glide's colour combine has the form as a first-class function: `grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_LOCAL_ALPHA, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE)` = `(texture - iterated_rgb) * iterated_alpha`, which IS the cartridge's arm, with the same clamp at the combine output. The Gouraud vertex carries tint in RGB and light in alpha exactly as it does on GL. **So Tier 1 is one pass and one TMU where Tier 2 needs two stages, and TMU1 stays free for the water.** |
| **The alpha lane (the atlas water hole)** | Both stages `GL_REPLACE` the alpha from the texel, so the existing `glAlphaFunc(GL_GREATER, 0.5)` still cuts the river out of the ground | `grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL, ..., GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_TEXTURE, FXFALSE)` plus `grAlphaTestFunction(GR_CMP_GREATER)` / `grAlphaTestReferenceValue(128)`. The vertex alpha is the LIGHT here and must not reach the alpha test, on either tier. |
| **The CM data itself** | 4225 u16 in the pack (8450 bytes per pack) plus a 65x65x3 byte precomputed grid, built once at load with the light | **Unchanged and negligible**: 8 KB in the pack, 12 KB resident, no per-frame arithmetic at all. |
| **Cost when the layer is a no-op** | `terrain_tint_live()` falls back to the single-stage `GL_MODULATE` when the pack has no CM block or the scaled tint is zero everywhere (CMFLAT scenarios), so those maps pay nothing | Same branch, same reasoning. |

**The one deviation, and it is a rounding one on BOTH tiers.** The RDP divides by 256
with a `+0x80` round; GL's modulate divides by 255, and Glide's combine unit likewise
works in 1/255. So a terrain pixel can come out one level brighter than the console's.
Measured by the `cmcombine` script verb against read-back pixels over 180
(texel, tint, lit) triples: **146 exact, 34 one level brighter, 0 darker.** That is the
signature of the divide and nothing else. It is not new -- the plain modulate this
replaced carried the identical /255 -- and it is registered in `known-gap notes` rather
than corrected, because pre-scaling the vertex alpha by 255/256 would move every terrain
pixel that already exists for a sub-level gain.

**Not built, and it is a Tier 1 item too when it is:** the cartridge's SEABED pass uses
the SIBLING combiner arm (ROM 0x19A590, `(TEXEL0 - 0) * SHADE_ALPHA + SHADE`), i.e. it
ADDS the same tint to the sea floor. On Glide that is
`GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL` with the same factor, so it is equally
native when someone builds it. See `known-gap notes`.

## The software renderer (added 19 Aug 2026)

Tier 1 now has TWO possible backends, not one, and this table gained a column in spirit:
a **software rasteriser** (`tier1/softras.c`, running today on the box) and **Glide on the
Voodoo 2** (not yet built). the project owner's call on 19 Aug was to prove the path in software first,
because a CPU rasteriser removes the 3dfx driver from every early failure, can be gated on
the Mac before it reaches Win98 hardware, and unlike a Voodoo is visible over VNC.

The software backend changes what some rows above mean, so they are restated here rather
than left to be misread:

| Feature | Tier 2 | Tier 1 software | Tier 1 Glide |
|---|---|---|---|
| Texture filtering | `GL_LINEAR` or `GL_NEAREST` via `fx_filter.h` | point sampled only. Bilinear in software costs four texel fetches and three lerps per pixel and is not affordable on a 534 MHz Pentium III | bilinear free in hardware, `grTexFilterMode` |
| Texture wrap | `GL_CLAMP_TO_EDGE`, `GL_MIRRORED_REPEAT` | the rasteriser MASKS, so it is `GL_REPEAT` semantics only and every texture must be a power of two. Clamp and mirror have to be baked into the texture at pack time | `GL_CLAMP` equivalent exists |
| Texture size | any | power of two only, and no size limit beyond RAM | power of two, and the TMU is 4 MB total |
| Perspective correction | free | subdivided: one divide per 8 pixels, linear in between. This is the era's technique and it is in `SR_SPAN` | free in hardware |
| Lighting | per-vertex or per-fragment | **a 32 level palette shade table**, one indexed load per pixel. Not a multiply. Colour is palettised end to end, which is what the art already is | per-vertex iterated colour, and **Glide WRAPS a component above 255 to near-black instead of clamping**, so the caller must clamp |
| Depth buffer | 24 bit | a float 1/w buffer today, which is one float compare and one float store per pixel. A 16 bit fixed point buffer is the era's answer and is owed | hardware |
| Near plane clipping | free | **absent.** Triangles with any vertex at or behind the eye are dropped whole. See `known-gap notes` | absent for the same reason until written |
| Output | window or fullscreen GL | a 32 bit DIB section blitted with `BitBlt`. No page flip, no vsync, and it goes through GDI | fullscreen only; a Voodoo 2 has no 2D output of its own |
| VI output gamma | `pow(c, gamma)` in the resolve pass | **free and exact.** The gamma can be folded into the shade table when it is built, at zero per-pixel cost. This closes the one real fidelity loss the Glide row above admits to | `grGammaCorrectionValue()` |

**The software backend's speed is currently unexplained and roughly 50x off**, which is
recorded in `known-gap notes` rather than here, because it is a defect to fix and not a
declared compromise.

### Two Tier 1 decisions that are open, and are the project owner's

**1. The pixel format of the 3D art.** The software rasteriser is palettised end to end:
8-bit texture indices and a 32-level shade table, one indexed load per pixel. That is what
makes it fast enough to exist. The DOS 2D art is already 8-bit and fits perfectly. **The 3D
scenario art does not.** `game/cnc_eyes.cpp:18-20` describes the scenario packs as textures
"already decoded to RGBA" with meshes carrying "per-vertex UV and RGBA", and the terrain's
per-corner RGBA is a CM tint plus a light, which a single scalar cannot carry.

| Option | What it costs | What it keeps |
|---|---|---|
| (a) Quantise scenario art to a shared 256 colour palette at bake time | a bake pipeline change, some colour fidelity, and an answer for the per-vertex tint | the fast inner loop |
| (b) Give the rasteriser an RGBA path | the shade table trick, which is the difference between a playable frame rate and a slideshow | exact art |
| (c) Hybrid: palettised terrain and units, RGBA for the few surfaces that need it | two inner loops to maintain | most of both |

**2. The Tier 1 resolution. ANSWERED, 19 Aug 2026, by measuring the card.**

Fill rate arithmetic against real pack data puts an in-mission frame at roughly 5,700 to
8,000 triangles and about 1.1 million pixel writes at 640x480. Even at 15 cycles per pixel
that is 31 ms of fill alone on a 534 MHz Pentium III, so **640x480 at 30 FPS is not
reachable in SOFTWARE on this machine.**

It is comfortably reachable on the Voodoo 2, and that is now measured rather than argued.
`tier1/t1_glidetest.c` on the real card, drawing full-screen textured, z-buffered,
bilinear-filtered quads so the pixel count is exact rather than estimated from triangle
areas:

| load | FPS | fill |
|---|---|---|
| 4 full-screen quads per frame | 118.87 | 146.1 Mpixel/s |
| 16 full-screen quads per frame | 32.58 | 160.1 Mpixel/s |

**About 160 million textured, depth-tested pixels per second**, converging upward as the
per-frame overhead is amortised. Glide reports one board because an SLI pair presents as
one, which is consistent with roughly 90 Mpixel/s per Voodoo 2.

Against the software renderer's measured 254,541 pixels in 36.00 ms, which is
**7.07 Mpixel/s**, the card is **about 23 times faster at the thing that was the
constraint**. The same terrain frame costs 36.00 ms in software and about 1.6 ms on the
Voodoo. The depth clear (6.80 ms) and the GDI `BitBlt` (15.35 ms) both disappear entirely,
because the card clears and page-flips in hardware.

So the resolution question is not a compromise to be chosen: **Tier 1 runs at 640x480 and
the frame stops being fill bound at all.** What is left is CPU side, which is scene
assembly, the brain tick and the object dump.

Separately, the triangle rate was measured at roughly 15,900 per second with these large
triangles, and it stayed flat as the count went 50 -> 200 -> 800 while FPS scaled inversely,
confirming the card is fill bound and not setup bound at this geometry.


## Wave 7: what the SECOND Win98 pass added to Tier 1

Nothing in this round is a Tier 2 debt. Three of the four are places where Tier 1 needed a
mechanism the desktop build gets for free from GL, and the fourth is a straight Tier 1 WIN.

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **Texture coordinates** | GL takes normalised 0..1 and the driver scales by the bound texture's size | **A Voodoo's coordinates live in a fixed 0..256 space stretched over whatever LOD is bound**, so texels must be scaled by 256/max(w,h) at submission. There is no Tier 2 equivalent because there is no problem on Tier 2. Getting this wrong drew ONE TEXEL of every object texture over the whole polygon, and it did it silently for a fortnight because every page-shaped texture in the game is 256 across, where the scale is 1 |
| **Shadow textures** | `GL_ALPHA` or an RGBA upload; the desktop keeps the source RGBA and lets `GL_MODULATE` do the rest | **A dedicated `GR_TEXFMT_ALPHA_8` bank**, 18 KB for all 35, because the palettised bank cannot hold them at all: every shadow texture is white RGB with its coverage in ALPHA and 34 of the 35 peak below the converter's alpha >= 128 threshold, so they were already fully transparent |
| **The shadow combiner** | `glColor4ub` plus `GL_MODULATE` and an RGBA texture | **NATIVE, and closer to the cartridge than GL is.** The RDP's shadow arm is FCFFFFFF/FFFDF2F9 = "RGB = PRIM, alpha = TEXEL0_A", and Glide expresses each half as a first-class function: `grColorCombine(FUNCTION_LOCAL, ..., LOCAL_ITERATED, OTHER_NONE)` for the colour and `grAlphaCombine(FUNCTION_SCALE_OTHER, FACTOR_LOCAL, LOCAL_ITERATED, OTHER_TEXTURE)` for the alpha. That is the ROM's combiner term for term, in two calls |
| **VQA movie frames** | Decoded indices expanded to RGBA on the CPU every frame, then `glTexSubImage2D` | **NATIVE and CHEAPER.** A decoded VQA frame is 8-bit palette indices plus 768 bytes of palette, which is exactly `GR_TEXFMT_P_8` plus `grTexDownloadTable`. The whole RGBA expansion does not happen on this tier at all; the decoded rows are memcpy'd into four pages and uploaded as they are |

**One new watch item, and it is a real one.** The mesh bank grew from 0.75 MB to 1.92 MB
when the baked node animation (1,091 KB of pose) and the shadow alpha planes (18 KB) were
converted. That is resident on a machine with about 388 MB free, so it is comfortable
today, but it is the largest single block in the build and halving the pose to `short`
fixed point is still available and still not needed.


## Team colours on infantry (added with the per-seat livery round)

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **A skirmish seat's colour on its FOOT SOLDIERS** | up to six extra copies of the 1995 infantry sheets, the uniform band (palette 176..191) substituted per `PlayerColorType`. Measured on the shipping pack: 102 of its 397 strips qualify, 1.43 M texels, so **5.5 MB on the card per colour and 33 MB with all six in play** | **none by default, and that is a deliberate cut rather than an oversight.** It is thirty times what the mesh liveries cost (0.18 MB per colour) and a Voodoo 2 has 4 MB of texture memory in total, so Tier 1 keeps the pack's own two colourways on infantry and puts the seat's colour on vehicles, buildings, selection boxes, radar blips and rally flags only. **A much cheaper Glide answer exists and is not written:** these sheets are 8-bit and Glide is natively palettised, so `GR_TEXFMT_P_8` plus one `grTexDownloadTable` per colour makes a livery cost a 768 byte palette instead of a second copy of the art. On that route Tier 1 could carry all six for less than one costs on Tier 2 |


## The production queue strip

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **The production queue strip beside the Enhanced panel** (one 38x30 card per item in production or queued: half-size cameo, build clock, percentage, count in the corner, a "+N" card for overflow; cards slide out from under the panel, slide back under it and rise to close a gap, smoothstep eased over 0.28 to 0.32 s of real time) | composed on the CPU in `game/hud_enhanced_impl.h` (`hud_enhanced_queue_card`, plain C89, the same clock wipe as the full cameos) into one 104-column RGBA strip, uploaded through the bar's own `h6_upload` and drawn as ONE textured quad per frame BEFORE the panel quad, alpha blended only for the cut corners; the motion is whole-native-unit offsets in that CPU composition | **SHIPS WITH THE ENHANCED PANEL, no new mechanism.** No shader, no render target: a CPU raster, one texture upload and four vertices, the bar's own path. The strip is 104 x up to 693 RGBA, which on a Voodoo 2 has to be split into 256-tall pages (three quads) exactly as the bar itself would be; the alpha on the cut corners can be a chroma key on Glide. The clock is `SDL_GetPerformanceCounter`, which Win98 has. Tier 1 never enters Enhanced, so on Win98 the strip is absent, like the rest of the Enhanced panel. |

## The unit card

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **The selection card in the bottom-left corner of the Enhanced HUD** (cameo, name, health, damage, the rest of the selection as smaller cameos, ten control-group tabs) | composed on the CPU in `game/hud640.c` (`hud640_draw_card`, plain C89) out of assets in `hud640.pack`, uploaded through the bar's own `h6_upload` and drawn as ONE 240x166 textured quad per frame, alpha blended | **SHIPS WHEN THE 640x480 HUD SHIPS, with no new mechanism.** It is the same rasteriser and the same blit the bar already is on this tier; the one texture is 240x166 RGBA in a 256x256 page, 256 KB once. The alpha blend is only for the bezel's rounded corners and can be a chroma key on Glide. The Enhanced-only gate is the codex's gate (`codex_available`), and Tier 1 never enters Enhanced, so on Win98 the card is simply absent, exactly like the DATABASE plate. What Tier 1 does not have is the `group=` field in its own brain reader (`tier1/w98_brain.c`), which is the same three-line shape as the `alt=` gap recorded above. |

## The primary-factory label

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **The word "Primary" under a selected factory** | one 8-bit DOS surface rasterised once with `db_print` in 8POINT, widened to RGBA, uploaded as a 64x16 GL texture and drawn as one alpha-tested screen quad per label | **the same thing minus the widening step, and it is CHEAPER here than on the desktop.** `game/dosbar.c` is compiled into the Win98 build UNEDITED, so `db_font`, `db_print`, `db_string_width`, `db_font_palette_full` and the 8POINT glyphs are all present today at no additional cost. The plate is 8-bit palettised, which is the Voodoo's native `GR_TEXFMT_P_8`, so this tier skips `db_surface_to_rgba` entirely and lets the chroma key drop index 0 the way the HUD pages already do. 1 KB of a 4 MB TMU, uploaded once, because the string never changes. The projection is `t1_world_to_eye` plus the same divide `draw_brackets` already performs per selected object, and the brain field arrives through `field_int(line, "|primary=", 0)` beside `o->sel`, so an older brain reads 0 |

**The plate is authored 64x16 on BOTH tiers for this tier's sake.** The software rasteriser
masks rather than clamps, so every texture there must be a power of two; 42x11 of ink inside a
64x16 page satisfies that and costs the desktop nothing.

**What is actually absent on Tier 1 today is the whole per-object overlay family, not this.**
There are no health bars, no pip rows and no repair wrench on the Win98 build: it draws
selection brackets off the engine's own `sel` flag and nothing else per object. So the honest
row is that the label ships on Tier 1 with no new mechanism and no shader, one chroma-keyed
textured quad and 1 KB of texture, and it lands when that family lands, which it has not yet.
It is not a Tier 2 exclusive and it is not a blocker; it is one more quad on a list that is
currently empty.

Two traps to carry over when it is written, both already on the recorded list: texture
coordinates live in a fixed 0..256 space stretched over the bound LOD, so the UVs cropping the
ink out of the 64x16 page must be scaled by 256/64, and an unbounded quad wedges the card.

One placement difference to declare rather than discover: Tier 2 anchors the label to the
bottom of the object's projected MESH silhouette, and Tier 1 has no mesh-silhouette bound, so
it anchors to the object's ground point plus a fixed offset in HUD pixels. Identical on a squat
building, a little high on a tall one.

## Solid tiberium

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Crystal clumps standing on a tiberium cell** (`game/tib3d_mod.h`, `draw_tiberium_ground`, `draw_tiberium_solid`) | three passes out of `tib3d.pack`: the ground cracks (one additive quad per CRYSTAL of a generated 128x128 tile, `GL_ONE, GL_ONE` over a premultiplied sheet), the clumps themselves (four indexed meshes on one 256x256 sheet, opaque; on the modern build every clump keeps one fixed slot of a buffer object for its whole life, rewritten and uploaded only when that clump grows, shrinks, is reshaded or goes, its glow bytes sent only when its glow byte changes, and drawn with 32-bit indices; without buffer objects the same slots go through client vertex arrays), and a glow (the same batch added over itself with `GL_SRC_ALPHA, GL_ONE` through the emissive mask baked into that sheet's alpha) | **SHIPS ON TIER 1 as far as the API goes, and is NOT WIRED THERE.** Nothing in the three passes needs anything above GL 1.1: no shader, no render target, no multitexture, and no buffer object: the buffer objects and 32-bit indices are the modern build's speed, not a requirement, and a Tier 1 draw of the same slots through client arrays, flushed in batches under 65535 vertices, takes `GL_UNSIGNED_SHORT`. Two textures in the whole feature, 256x256 and 128x128, both powers of two at or under the Voodoo 2's per-side limit; at 16-bit that is 160 KB of a 4 MB TMU. The remastered candidate pack (a version 3 pack, not shipped yet) carries a 512x512 sheet; its Tier 1 answer is that sheet halved once more to 256 at bake time, the same box filter that made it, and the rings rule's table (`game/tib3d_layout.h`) is plain data a second reader would take as it is. Two further passes come with a version 3 pack, both fixed function: the CONTACT patch (`draw_tiberium_contact`, a 64 x 64 generated texture multiplied into the ground under each mound with `GL_ZERO, GL_SRC_COLOR`, on a 3 x 3 grid of terrain samples, `GL_CLAMP` with its rim white so the border adds nothing) and the crystals' FLOW (`tib3d_flow_begin`, a 64 x 64 generated streak tile added `GL_ONE, GL_ONE` over the crystals' own triangles through the sheet's texture coordinates, scaled and scrolled by the texture matrix on the engine clock). Glide does both: the multiply is `grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_SRC_COLOR, ...)` and the scroll is a per-frame offset on the texture coordinates, which Glide takes from the CPU anyway (it has no texture matrix, so the port adds the offset while it submits the vertices). If the extra fill of the flow is too much for a field, the Tier 1 answer is to leave the flow out: the crystals are drawn and glow without it. The mounds' skirt fades through the vertex alpha alone (`tib3d_skirt_begin`: `GL_COMBINE` taking the colour from the sheet times the vertex shade and the alpha from the vertex colour, blended, with an alpha test); Glide's alpha combine takes the iterated alpha the same way (`grAlphaCombine` with the local iterated alpha), so it ports, and without it the skirt simply draws opaque. The mounds' GROUND BLEND (the colour of the ground a mound stands on creeping up its lower part) is a second, untextured pass over the mounds' own triangles, its colour per vertex the drawn terrain's per-cell average (`PackCell::avg`, the radar's colour) and its alpha the blend, `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` with depth writes off: on Tier 1 it is the same pass with Gouraud colour and iterated alpha, which Glide draws natively. The temperate theater's darker mound colour is a per-channel multiply on the mounds' vertex colour, Gouraud colour on Tier 1 as well; the choice of pack (the remastered one, the old as fallback) is a file choice at mission load, nothing a Tier 1 reader would draw differently. The clump pass is opaque with depth writes on, so there is nothing to sort and no alpha test. **The emissive glow is the one part that costs the Voodoo a second look:** it is `GL_SRC_ALPHA, GL_ONE` over the same geometry, which Glide does natively, but it doubles the fill for a field and the Voodoo 2 is fill limited before it is triangle limited. Its dial is `tib3d_glow` and 0 removes the pass entirely, so the Tier 1 answer if it turns out too expensive is a number rather than a rewrite. The crack decals are additive too and there is one per crystal rather than per cell, so they are the other half of that fill; `tib3d_ground` is their own dial and 0 removes them. They bind with `GL_CLAMP`, chosen over `GL_CLAMP_TO_EDGE` for this tier, and rely on the tile's black border making a rotated sample outside 0..1 add nothing. What is missing is the same thing missing for the shatter and the overlay family: **Tier 1 is a separate program** (`tools/win98/build.sh` builds `w98glide.exe` out of `tier1/*.c` and never compiles `game/cnc_eyes.cpp`), so the pack reader, the instance list and the draw would have to be written a second time. Until they are, Win98 draws the flat cartridge overlay alone, which is the console's own picture and therefore not a fidelity loss. |

**The cost to watch when it is ported is triangle COUNT, not the API.** A field on screen
is a few hundred clumps of 84 to 252 triangles each: measured on the largest field in
SCG05EA, 450 clumps drawn of 618 placed over 140 cells. The `tib3d_density` dial exists
partly for this -- at 0.25 the same field is a quarter of the geometry -- and the view-bounds
cull is already in the pass rather than left to the card.

## Real grass on the ground

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Blades standing on every grass cell** (`game/grass_bake.h`, `game/grass_field.h`, `game/grass_draw.h`, `grass_draw()`) | one static vertex buffer built at load holding every blade in the map as two triangles, drawn alpha blended with depth writes OFF through a GLSL 1.20 program that makes TWO VERTEX-STAGE texture fetches; a map-wide RGBA16F render target carrying tread, cover and shroud, updated with `GL_MAX` and a reverse-subtract blend equation; and the blade's colour read back off the GPU from whichever terrain atlas is drawn | **ABSENT ON TIER 1, and this is a deeper gap than the tiberium one.** Four separate things in it are above the Voodoo 2's line, and three of them have no fallback that keeps the feature recognisable. The **vertex texture fetch** is the hard one: the blade reads the ground colour and the crush field in the VERTEX stage so a whole card fades, presses and takes its colour as one thing, and a fixed-function pipeline cannot sample a texture per vertex at all. The **half-float render target** is the second: `GL_RGBA16F` needs a float-format framebuffer, and the field's exactness argument rests on one fp16 unit in the last place being the decay step, which an 8-bit target cannot express. The **buffer object** is the third, and it is the only one with a clean answer: without `GL_ARB_vertex_buffer_object` the geometry can be submitted as client arrays, which the measured numbers say costs 5 to 39 ms a frame against 0.07 to 0.30 from a buffer, so on a Voodoo 2 that answer is "do not draw grass" rather than "draw it slowly". The fourth is **fill**: a blade is a blended card two to eight screen pixels tall with no alpha test, and there are tens of thousands of them; the Voodoo 2 is fill limited before it is triangle limited and this is the worst kind of fill there is. **So Win98 draws the cartridge's own ground with no grass, which is the picture this project has always shipped there and is not a fidelity loss.** The whole feature is behind one dial, `grass`, and every entry point is gated on `g_fxActive`, so nothing here reaches the Voodoo path even if `tier1/*.c` later grows a reader. |

**What a Tier 1 grass would have to be, if it is ever wanted.** Not this feature with parts
removed. The blade would have to become a texture: the mask that says where grass grows is
already read per terrain texel at load, so a second ground layer drawn as a tiling alpha sheet
modulated by that mask is the shape a Voodoo 2 could carry, at one extra pass over the ground
rather than one card per blade. It would move in the wind only as a scrolling texture matrix,
it could not be pressed flat by a vehicle without a second sheet to modulate it, and it would
have no silhouette at all at a grazing camera. That is a different feature wearing the same
name, which is why it is written here as a rewrite and not as a fallback.
## The four lighting additions

Enhanced only, every one of them, and each has the same Tier 1 answer for the same
reason: **Win98 does not run the post chain at all**, so none of this is reached there
and the picture is the one that build has always had. That is a complete answer rather
than debt, and it is recorded per feature anyway so the list is findable by name.

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **High dynamic range and the highlight rolloff** (`game/fx_gl.h` `fx_rt_init`, `FX_RT_FLOAT`; the curve in `FX_FS_RESOLVE`) | the scene, lit and bloom targets are allocated `GL_RGBA16F` so a value may exceed white, and a rolloff at the end bends what used to clip into the headroom instead. Identity below the knee, to the bit. A driver that refuses a float target falls back to the eight bit format and the curve switches itself off | **ABSENT. Glide has no floating point render target at any size**, and the Voodoo 2's frame buffer is 16 bit fixed with no alternative. There is nothing to port: the whole idea of a value above white has no representation there. Win98 clips at white as it always has. Note the fallback path in `fx_rt_init` is exactly what a Tier 1 machine would take if it ever ran the chain, so the code already describes the degradation |
| **Sky cube map and image-based lighting** (`game/sky3d_mod.h`, `game/bake_sky.py`, the `uEnvOn` branch in `M3_FS`) | a prefiltered cube map of roughness levels plus nine spherical-harmonic coefficients replace the two-colour hemisphere in the authored-model shader: split-sum specular against an analytic environment BRDF, and the diffuse reconstructed from the harmonics | **ABSENT, and doubly so.** A cube map is OpenGL 1.3 and Glide has no such target; the shader that would sample it does not exist on Tier 1 either, and neither does the authored model it lights. Win98 draws the cartridge's own model with the console's baked vertex colours |
| **Contact shadows** (the march inside `if (uShadowOn == 1)` in `FX_FS_LIGHT_BODY`) | eight depth fetches along the sun ray from each pixel, folded into the shadow term with `max()` so it cannot double-darken what the shadow map already found | **ABSENT. It is a fragment-stage loop over a depth texture** and Glide has neither a fragment stage nor a readable depth buffer. The thing it draws -- the darkness directly under a hull -- is also drawn on Tier 1 by the cartridge's own PKD shadow quad, which is a different and cruder answer to the same question and is what that build ships |
| **Relighting the cartridge's own art** (`fx_draw_mesh_bake` in `cnc_eyes.cpp`, the `uBakeOn` branch in the light pass) | the world's meshes are drawn a second time with textures unbound, which gives the console's baked vertex shade alone; the light pass divides it out of the finished colour and multiplies a real one back in, as a ratio so the gamma encoding cancels | **ABSENT, AND IT WOULD BE THE WRONG THING THERE EVEN IF IT RAN.** It needs a second render target, a second geometry submission and a fragment stage, none of which Glide has. More to the point, Tier 1's whole promise is the console's presentation, and the console's presentation IS the baked lighting this removes. Win98 keeps it, on purpose, and that is not a compromise |

## The authored vehicle models

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **A vehicle drawn from an authored model and lit by a real light** (`game/model3d_mod.h`, `game/models3d/*.c3m`; one branch at the top of `draw_object_mesh`) | a type that has a `.c3m` installed is drawn from it instead of from the cartridge's mesh: about ten thousand triangles drawn with `glDrawElements` from one vertex buffer and one index buffer per model, filled once at load (a performance measure only: a driver without buffer objects draws the same arrays from system memory, and the picture is the same either way), three RGBA atlases per material (base colour with the player mask in its alpha, a tangent-space normal map, and occlusion/roughness/metal packed into one), and a GLSL 120 program that evaluates Cook-Torrance GGX with a Smith height-correlated visibility term against the chain's own sun plus a two-colour hemisphere fill. The tangent frame is derived from screen-space derivatives rather than carried in the vertex. A second, cheaper pass writes the same per-pixel normal into the post chain's normal target under a class of its own, so the occlusion and the wet-surface response read the hull rather than a plane reconstructed from depth | **ABSENT, AND IT IS A COMPLETE ANSWER RATHER THAN DEBT.** Glide is fixed function: no vertex stage, no fragment stage, nothing that can evaluate a microfacet term or sample a normal map into a tangent frame. There is no budget at which this draws on a Voodoo 2, so there is nothing to port and nothing to schedule. Win98 draws the cartridge's own model for every vehicle, which is 138 triangles for a Mammoth with five 16x16 textures and the console's lighting baked into its vertex colours -- the picture that build has always had and the one the charter promises. **The three constraints that make the gap total, and not merely large:** the shader stage does not exist; the TMU is 4 MB and unvirtualised against a single model's 12.8 MB of atlas; and the whole thing is reached only from Enhanced Visuals, which Win98 does not run at all. **What Tier 1 would need is not this feature but a different one** -- a mid-poly model with one baked-lighting texture per part, which is what the cartridge already ships. |

## The 3D soldiers: skinned, animated infantry

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **An infantry type drawn as an animated, skinned model** (`game/model3d_mod.h` `m3_pose`, `game/cnc_eyes.cpp` `m3inf_draw`, `game/models3d/E1.c3m`, converted by `tools/art/fbx_c3m.py`) | a type with a version 2 `.c3m` (skeleton, per-vertex weights, clips) is posed on the CPU every frame from its idle, run or shoot clip and drawn through the authored-model shader; poses it has no clip for fall back to the sprite | **ABSENT, for the lighting and not for the skinning.** The pose is computed on the CPU into an ordinary vertex array, deliberately, so the skinning itself is not what keeps this off a Voodoo 2: fixed function draws a vertex array. Each soldier is skinned once a frame, however many passes draw him (the frame's pose cache in `m3_skin`), though at 13,356 vertices at full detail and 5,705 at the coarsest level it would not be cheap on a Pentium II. What keeps it off is the same wall as the vehicles: the colour is a base map that means nothing until the microfacet shader has lit it, and Glide has no such stage. Win98 draws the 1995 sprite for every soldier, which is what it has always drawn. **A Tier 1 soldier is possible and would be a different asset**: the same skinned mesh with lighting baked into its texture or its vertex colours, drawn untextured-lit the way the cartridge's own meshes are. Not built, and not scheduled. |
| **E3, the GDI rocketeer** (`game/models3d/E3.c3m`) | the same skinned path and levels of detail; 17,282 triangles at full detail, 4,320 at the coarsest | **ABSENT, as the E1TR row below.** Win98 draws the Rocket Soldier's 1995 sprite. |
| **E1TR, the test infantry, and the ground and death states** (`game/models3d/E1TR.c3m`, `m3inf_slot_clip`, `g_m3Corpses`) | the same skinned path with 24 clips: fidgets, aim, lying down, prone, crawl, fire prone, get up and deaths are posed from the model, and a dead soldier's body is kept and drawn after the engine deletes him | **ABSENT, same wall as the row above, and heavier.** 13,356 vertices (5,705 at the coarsest level of detail, which is what the default camera draws), posed on the CPU once a frame and reused by every pass that draws the soldier, is still not cheap on a Pentium II. Win98 draws E1TR as the Minigunner's sprite (the brain loads E1's shapes for it, and the renderer reads it as E1 everywhere but the model), and dead soldiers vanish when the engine deletes them, as they always have. |
| **The 3D soldiers' side colours: team colour and cloth colour** (`SOLDIER_TEAM` / `SOLDIER_CLOTH` in `game/cnc_eyes.cpp`, `M3_MAT_CLOTH` in `game/model3d_mod.h`, masks by `tools/art/ue_c3m.py` and `tools/art/cloth_mask.py`) | the model shader multiplies the helmet shell and plates by the side's colour and replaces the uniform's paint with the side's cloth colour at the paint's own brightness, so a Nod soldier wears a dark uniform and a GDI one the tan it was painted; the shoulder pad takes the team colour and the side's emblem, one of two stencils picked by side | **ABSENT with the models themselves.** Win98 draws the 1995 sprites, which already carry the two sides' colourways in their own palettes, so a Tier 1 Nod soldier is told apart from a GDI one exactly as he always was. A baked-lighting Tier 1 soldier (the row above) would need one texture per side for the cloth, since fixed function cannot recolour by mask. |
| **The remastered soldiers** (`game/einf/<TYPE>.c3m`, made by `tools/art/remaster/infantry`; `m3inf_available` / `m3inf_set_now` in `game/model3d_mod.h`; F5 > 12j `einf_on`) | Enhanced's own animated infantry, drawn with experimental art off: the same skinned path as the experimental soldiers (posed on the CPU, the authored-model shader, the side's uniform colour through the cloth mask), 20 types of 460 to 680 triangles, each with a 384 body atlas (480 for the tank carriers: not a power of two, and over the Voodoo 2's 256 limit, so a Tier 1 soldier would take a downsampled copy) | **ABSENT, same wall as the rows around it**: the base map means nothing until the microfacet shader lights it, and the cloth mask needs a shader to recolour. Win98 draws the 1995 sprite for every soldier, as it always has. A Tier 1 soldier would be this mesh with its light baked into the texture (the painting already carries most of it: the albedo is scaled 0.55 in the game only because the game lights it again) and one texture per side for the uniform; about 1,300 vertices posed on the CPU is cheap on a Pentium II. Not built. |
| **What a 3-D soldier leaves and throws** (`game/infdeath_mod.h`; F5 > 12j: bodies stay, most bodies, body brightness, ragdoll falls, blood, muzzle flashes, spent casings) | Bodies kept and darkened (the model's tint), a ragdoll (points and distances, CPU), blood (an alpha-blended decal on the ground), flashes (the Enhanced particle sheet), casings (immediate-mode boxes) | **Bodies, ragdolls and flashes ABSENT with the 3-D soldiers they belong to** (Win98 draws the 1995 sprites and their own death frames). **Blood and casings could stand on their own**: both are fixed-function (one alpha-blended textured grid, a few untextured boxes), so a Tier 1 build could lay the blood under the sprite's last death frame and throw casings from the sprite's muzzle height. Not built. |
| **Soldiers coming apart, their spray and bloody tracks** (`game/infdeath_mod.h`, `game/treadmark_mod.h`; F5 > 12j dismemberment, blood spray; tracks: bloody tracks) | Torn bodies skinned on the CPU and drawn with the model's shader; drops as immediate-mode lines; ground splats as alpha-blended textured quads; splats on vehicles and buildings as boxes whose fragment shader reads a copy of the depth buffer (GLSL 1.20, a depth texture copy per frame); red tracks as a second alpha-blended pass over the track's quads | **ABSENT, with the 3-D soldiers they belong to** (Win98 draws the 1995 sprites and runs no Enhanced tier). The drops, the ground splats and the red tracks are fixed function and could stand on their own; the splats on hulls could not (no depth texture, no fragment program on a Voodoo 2): they would be dropped, or laid as flat quads on the hull's top. Not built. |
| **The 3D soldiers' own light and their own sun shadow map** (`m3inf_sun` / `m3inf_fill` / `m3inf_paint` / `m3inf_shadow` in `game/fx_state.h`; `fx_render_inf_shadow_map` and the `uInfShadow` taps in `game/fx_post.h`; `fx_draw_inf_casters` in `game/cnc_eyes.cpp`) | the soldiers' sun and fill are shares of the model light, fed to the same shader; their team plates take a brightness scale; the posed soldiers are drawn into a second depth-only render target from the sun, the size of the shared one (4096 square by default, 64 MB of depth), and the light pass tests it with a one texel kernel so the shadow keeps his limbs; the contact march skips any depth sample the normal target marks with the soldier class | **ABSENT with the models themselves**, and it adds one render target and one shader branch that Glide has neither of. Win98's soldiers are the 1995 sprites, whose shadow is the cartridge's own blob, as it always was. |

## The tracks a vehicle leaves on the ground

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Ground tracks under Enhanced** (`game/treadmark_mod.h`; one call in the object refresh and one in the frame between the tiberium ground and the scorch marks) | each vehicle's path is sampled once per engine tick into a short trail of points, and each trail is drawn as two mitred bands of triangles lying on the terrain's own surface, textured with a generated cleat pattern and multiplied into the ground through `GL_BLEND` as the texture env with a white env colour and `glBlendFunc(GL_ZERO, GL_SRC_COLOR)`. Two fades, a timer and a distance behind the vehicle, ride in the per-vertex colour | **ABSENT ON TIER 1, AND BY CHOICE RATHER THAN BY CAPABILITY, which makes this row different from every other one in this file.** Nothing in the pass is above OpenGL 1.1: one texture unit, one blend, one env mode, immediate-mode triangles, no shader, no render target, no vertex texture fetch, no float format. Glide has all three pieces -- `GR_TEXTURECOMBINE` can do the blend against a constant, `grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_SRC_COLOR, ...)` is the multiply, and the pattern is a 64x64 one-channel texture with mip levels that fits any TMU. The two things that would have to be decided on the day are the polygon offset, which Glide does not have and which the water row above already owns as debt (the answer there is a depth-range tweak, and a track can equally well take a small Y lift since nothing else is drawn at that depth), and the fill, which is small: a trail is a few dozen thin quads and the whole book is capped at 256 trails. **It is absent because Win98 does not run the Enhanced tier at all**, not because it could not draw this. If Tier 1 ever wants it, this is a port and not a rewrite. |

## The Remastered infantry sprites, and the fold at the ground line

| Piece | Tier 2 | Tier 1 answer |
|---|---|---|
| **The player's own Remastered Collection infantry art under Enhanced** (`game/remaster_inf.h`, the `infset` row of Visuals/Advanced) | strips built at run time out of the install's TGA frames, one per (type, action), on RGBA power-of-two sheets sized to the frame count (a 128-frame attack strip lands on a 2048-a-side sheet) at about five times the DOS texel density; drawn by the same `dosinf_draw_sprite` as the 1995 sprites | **NONE. TIER 2 ONLY, and it was never listed before.** The art is not in the tree and Win98 has no reader for it; the sheets are far past the Voodoo 2's 256x256 limit and would have to be cut down to fit 4 MB. Win98 draws the 1995 sprites, which is the picture this project has always shipped, and nothing is lost that was ever there. |
| **The fold: a cell's rows below the man's ground line lie flat on the ground in front of him** (`dosinf_draw_sprite`, the `dn > 0` branch) | two fixed-function `GL_QUADS` in place of one, sharing the crease's two vertices, same texture, alpha test and modulate colour; the flat quad's far corners take their own `terrain_y` and the same +0.012 z-fight lift the vehicle shadows use | **NOTHING ABOVE GL 1.1 IN IT, and Tier 1 never reaches it anyway:** every strip the 1995 pack bakes has `drop` 0, so the branch is only entered for the Remastered set above. If Tier 1 ever carries a strip with rows below the line, the fold is two textured quads and a division, which Glide draws as it is. |

## Rain

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Rain under Enhanced** (`game/rain_mod.h`, `game/rain_glsl.h`; hooks in the light pass, the sea's wave field and surface, and one call in the frame after the effects) | three things on one switch: a lattice of 6000 thin quads placed by a GLSL 1.20 vertex shader from the engine clock alone (one client-array draw, no per-frame upload); a wet block inside the existing light program (darkening, a Fresnel sheen, a sun glint, drop-impact rings bending the reconstructed normal, puddles from the cloud deck's noise, distance haze, an overcast tint), which needs the offscreen scene, its depth texture and the ground-normal target; and drops pressed into the sea's RGBA16F wave field each step plus the same rings on its surface. Cost measured at 0.11 ms a frame over the chain at 1280x800, taking the minimum frame of 120 in alternating passes, because the mean drifts upward on a machine that is doing anything else | **NONE. TIER 2 ONLY, by the same row the whole chain has: on Win98 the chain does not run and the master switch is never on.** Stated piece by piece so the port is a list and not a surprise. The wet look cannot exist on Tier 1 at all: it is a screen-space term over a depth texture and a normal target, and the Voodoo 2 has neither a render target nor a shader. The drops on the water ride the wave field, which is itself a half-float render target with no Tier 1 form. The streaks are the one part a Glide build could draw, as fixed-function alpha-blended quads placed on the CPU from the same lattice and clock, and the number is 2545 of them at the shipped dials, not a few hundred: the count is 6000 times the square of the amount times the streak dial, which at 0.768 and 0.848 is 0.424 of the lattice. No shader is essential to them; the vertex shader is only what makes them free. A Glide build should be budgeted from that figure at 640x480, or from a lower streak dial chosen for this tier. None of it is written: with no wet ground under them they would be rain that leaves nothing wet, and the decision is to ship no rain on Win98 rather than half of it. There is no Visuals row at all: the rain is reached only through the F5 panel, which is compiled out of every release build, so on both tiers a player cannot turn it on and Tier 1 has nothing to answer for yet. |
| **The splash crowns** (`rain_splash_draw`) | a few hundred camera-facing quads a frame, placed on the CPU from the terrain heightfield and its corner normals, culled in a GLSL 1.20 vertex shader that samples the cloud noise and the sea's shore field with `texture2DLod`. Needs VERTEX TEXTURE FETCH, asked for once with `GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS`; a GL reporting none draws no crowns and says so | **NONE, and it would be the easiest part of the rain to give one.** The placing is already CPU work and the geometry is a few hundred blended quads, which a Voodoo 2 draws without complaint. What has no Tier 1 form is the CULL: the puddle rule reads a texture from a vertex shader, and the field that says where the sea is is a texture too. Both are readable on the CPU at the cost of keeping the baked cloud mask's level 0 in memory, 256 KB, and sampling the shore field there instead. It is not written, and it would not be worth writing before the wet ground the crowns stand in has a Tier 1 answer, which it does not. |
| **The rain's own sound** (`game/rain_audio.h`) | four seconds of mono 22050 synthesised at boot from a fixed generator and looped through `mixer_play_loop`. No shader, no GPU, no asset: plain C, integers and a noise generator | **IT ALREADY WORKS ON TIER 1 AND IS ABSENT ONLY BECAUSE THE RAIN IS.** This is the one part of the rain with no Tier 1 obstacle at all. `mixer_play_loop` is plain C whose wrap lives in `mixer_render`, so a Win98 DirectSound backend inherits it unchanged, and the synthesis is a few hundred lines of arithmetic over a 176 KB buffer. If Win98 ever gets rain of any kind, it can have the sound on the same day. |

## The texture-book second draw

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **A structure's texture-book variant drawn over its base mesh** (`texbook_overdraws`, the book loop in `draw_object_mesh`) | the building is drawn, then the book's variant mesh is drawn again at the same place. For PROC's first book, HPAD and FIX the variant's triangles head the base list and the second draw is pulled toward the camera with `glPolygonOffset(-1,-1)`; for FACT, SILO and PROC's second book the variant is bit-identical to the copy the base mesh already carries (posed with the book node's own transform, this round) and the second draw simply wins the `GL_LEQUAL` tie with no bias | **Half native, half a registered gap that predates this round and was never written down.** The bias-free half is Glide's home ground: `GR_CMP_LEQUAL` on identical vertices is exactly what the Voodoo 2 does with the water, and the silo's fill dome and the refinery's storage strip take that path on purpose. The biased half has no Glide form: `glPolygonOffset` does not exist there (see the water row above). The Tier 1 answer for those three books is the same bias-free path, which needs nothing more than drawing the variant after the base with depth `LEQUAL`; on a 16-bit Z buffer at 640x480 that tie may sparkle, and if it does the answer is a depth-range tweak, not an offset. None of this is wired: Tier 1 is a separate program that draws no texture books at all yet. |

## A mission start that is refused says why

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **The engine survives a missing or damaged theater archive** (`DisplayClass::Init_Theater` in both brains; the header guard in `common/mixfile.h`) | a `content/<THEATER>.MIX` that is absent, zero length or junk no longer faults inside `CNC_Start_Custom_Instance`: the palette is zeroed, one `OPENCNC3D\|theater\|` line goes to the log, and the mission plays | **ALREADY COVERED, and by the same object.** `tier1/w98_brain.c` loads the same DLL through `LoadLibraryA` and calls the same `CNC_Start_Custom_Instance` (`wb_start`), so the guard is in the engine it runs and nothing in the Tier 1 program has to change. |
| **The "Unable to start mission" box** (`game_notice_open` in the renderer, `DOPT_PAGE_NOTICE` in `dosopt.c`, drawn by the shell after `game_boot` refuses) | the pause dialog's rasteriser lends its objective box: a caption, the refusal wrapped at 255 px and a lone OK, over black, until dismissed. One GL 1.1 textured quad, nothing above the Voodoo 2's line in the drawing itself | **TIER 2 ONLY, by decision rather than by capability.** The Win98 program has no dialog module and no shell menu to put a box in front of: a refused start there already comes back through `wb_open`/`wb_start`'s `err` string (`LoadLibraryA(...) failed, GetLastError()=...`, `missing CNC_* exports ...`), printed by the caller. That line is the Tier 1 answer, and it carries the same facts the box does. The box is not mirrored because there is nothing there to mirror it onto yet; if the Tier 1 program ever grows a menu, `dosopt.c` is plain C89 with no GL in it and the page comes with it. |
| **`BOOTFAIL\|` in the log** (`boot_refuse` in the renderer, printed by the shell as `BOOTFAIL\|<scen>\|<reason>`) | every refusal on the boot path writes one sentence naming the file or the step | **Equivalent exists.** The Win98 program's `err` string is the same sentence in a different prefix; it is not spelled `BOOTFAIL\|` and no gate reads it, which is written here so nobody greps a Win98 log for a line it will not carry. |

## Unity muzzle flashes and projectile trails

| Feature | What it needs | Tier 1 answer |
|---|---|---|
| **Muzzle flashes at the 3D models' barrels and trails along projectiles, under experimental art** (`game/unityvfx_mod.h`: sockets, `uvfx_muzzle_fires`, `uvfx_trails_step`, ribbons and trail renderers) | Everything the Unity explosion needs (the same layer, bloom and composite), plus five more GLSL 1.20 programs, per-vertex colour on the ribbons, and the authored 3D models the sockets are measured on | **NONE. TIER 2 ONLY**, for the same reasons as the explosion, and one more: the sockets live on the authored models, which Win98 never draws. The engine's shot record costs nothing and is harmless there; the cartridge's own gun flashes and rocket puffs draw as they always have. |

## The Unity explosion

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Vehicle deaths and shell hits drawn with the Vefects Generic explosion under experimental art** (`game/unityvfx_mod.h`, hooks in `efx_step`'s claim and after the chain's resolve) | Unity's simulation replayed from a recording (4 seeds, 60 Hz, 8.5 s); seven GLSL 1.20 programs ported from the pack's URP shaders; a RGBA16F layer at the scene's size, depth tested by hand against the scene's depth texture, with soft particles; URP's Gaussian bloom (six half float mips) and its ACES tonemap in a composite over the finished view; `glBlendFuncSeparate` (GL 1.4) to keep the layer's coverage; sRGB textures (`GL_SRGB8_ALPHA8`, GL 2.1), three of them 2048 a side, about 100 MB with mipmaps. Measured at 1280x800: 1.2 ms median, 1.7 ms 90th percentile with up to three explosions live; 360 ms texture upload at load and a 140 ms warm-up on the first frame | **NONE. TIER 2 ONLY.** Every part of it is past the Voodoo 2: the shaders, the float layer, the depth read-back, the bloom chain, and 100 MB of textures against 4 MB of texture memory. On Win98 the claim never fires (it needs the chain, which never runs there), so the cartridge's explosion draws exactly as it always has. A Tier 1 version would be a different effect, a few pre-rendered sprite frames of this one baked out of Unity, and that is not planned. |


## The green pointer

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **The plain pointer drawn as the new green arrow under experimental art** (`game/cursor3d_mod.h` `c3d_green_add` / `c3d_mesh_for`, `game/cursor_green_mesh.h` from `tools/art/cursor_green.py`) | 94 untextured triangles with the approved render's shading baked into their vertex colours, appended to the loaded pack as one more mesh and drawn through `draw_mesh` exactly as the cartridge's own cursor slab is, with its own size dial | **DRAWS AS IS, NOTHING TO PORT.** It is the cartridge cursor's own form: untextured Gouraud triangles, no texture, no shader, no render target, per-vertex yaw done on the CPU. A Voodoo 2 draws it unchanged. It is reached only through experimental art, which Win98 does not run, so the Win98 pointer stays the white slab until someone chooses to expose it there; the geometry would need no work if they did. The **approved preview's green glow** (Blender emission plus a small bloom) is not drawn on either tier: the cursor path has no emission, and a halo is not geometry. |

## New Audio and the 44100 Hz mix

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **The mix runs at 44100 Hz** (`MIX_RATE` in `audio/mixer.h`) | the device, the mixer and the `--audiowav` tap all at 44100 Hz stereo. The 1995 clips reach it through the fixed 2x upsampler in `audio/halfband.h` at load, the music and movie streams through the same filter as they are pushed, so every 1995 sample is kept exactly and nothing below 11 kHz changes | **22050 Hz, as before, by the `WIN98` define the Tier 1 build already sets.** `MIX_RATE` follows it, `MIX_STREAM_UP` becomes 1 and the stream path is the plain copy it always was, so Win98 does half the mixing work and plays the 1995 data at its own rate. The clip cache budget scales with the rate, so it stays 8 MB there. `audio_win98.c` already asks waveOut for `MIX_RATE`. |
| **New Audio: replacement sounds from `newaudio/`, with random variants** (`bank_set_new_audio` in `audio/sndbank.c`, the F5 NEW AUDIO switch) | 44100 Hz mono WAVs, played in place of the 1995 sound of the same name while the switch is on | **Works, at 22050 Hz, if the program ever grows the switch.** The bank code is plain C89 and compiles for Win98 unchanged; loaded there, a replacement is resampled down to 22050 at load, so it loses everything above 11 kHz, the same ceiling the 1995 sounds have. The Tier 1 program has no F5 panel, so today nothing turns it on and Win98 plays the 1995 sounds. The files are about 8 MB for the first two dozen, which would share the 8 MB clip cache with the 1995 set. |

## The scattered bushes and grass clumps

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Bushes and grass clumps scattered over plain grass under Enhanced** (`game/foliage3d_mod.h`, `foliage3d_draw` called from the cutout pass, the sun caster pass and the ground-normal pass; pack from `tools/art/foliage/bake_foliage3d.py`) | the trees' GLSL 1.20 program (normal-mapped, wrapped diffuse, transmission, baked occlusion, wind in the vertex stage); per 8x8-cell chunk and per model, one static vertex buffer and one index buffer (`GL_ARB_vertex_buffer_object`) holding every plant already placed in world space; alpha-cut foliage sheets with coverage-held mip chains; the plants go into the sun's shadow map and the ground-normal buffer like the trees. Measured on SCG10EA at 1280x720: about 2 ms a frame at the default amounts (1,161 plants), 3 ms at a heavy setting (4,340 plants, 976,000 triangles); a whole-map build of 66 ms at the defaults, 222 ms at the heavy end | **NONE. TIER 2 ONLY.** The lighting is the trees' shader, the placement lives in vertex buffers, and a default scatter is several hundred thousand alpha-tested triangles, which on a fill-limited Voodoo 2 is the worst case even before the shader is taken away. It is reached only through experimental art, which Win98 does not run, so the Win98 picture is the cartridge's plain grass, as it always was. A Tier 1 version would be a different feature: a handful of camera-facing sprite cards per cell, baked from these models, drawn with the fixed-function alpha test. Not planned. |


## Free cam

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **F5's FREE CAM toggle** (`freecam_set` in `game/cnc_eyes.cpp`, the toggle in `game/fx_panel.h`) | the map editor's Unreal fly camera (yaw, free pitch, free eye height) lent to the game, with the far plane pushed out as the editor pushes it | **NOTHING TO PORT, NOTHING TO REACH.** It is the same fixed-function frustum, rotations and translate the console camera already uses, so a Voodoo 2 could draw it; but it is reached only from the F5 panel, which the Win98 program does not have. |

## The pointer keeps its size through the zoom

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Under Enhanced the 3D pointer and the sidebar arrow hold their default-zoom size** (`cursor_zoom_factor` in `game/cnc_eyes.cpp`, applied in `c3d_draw` and `cursor_scale_at`) | one ratio of two ground depths per frame, multiplied into `MODEL_SCALE` and into the sprite's latched scale | **DRAWS AS IS.** A few float multiplies on the CPU before an ordinary mesh draw; no shader, no render target. It is reached only under Enhanced, so Win98 keeps the cartridge's world-sized pointer unless someone exposes it; the arithmetic would need no work if they did. |


## The ground patches

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Patches of ground variation over plain Temperate and Desert/Sand ground, under Enhanced** (`game/mottle_mod.h`, `mottle_draw` after the terrain and the shore; `mottle.pack` / `mottle_desert.pack`) | blended, trilinear-mipmapped RGBA quads on each cell's own four terrain corners, depth writes off, about 6% of the plain cells carrying one, laid only on a high-resolution ground (the Enhanced or Remastered sheet), each patch about four cells across | **ABSENT.** Win98 draws the cartridge's own ground art and lays no patch on it. It would port as far as the drawing goes (blended textured quads and per-vertex light are fixed function), but the 1024-texel patch textures are four times the Voodoo 2's 256-texel limit, so a Tier 1 version would take them at 256, and on the 24-texel ground a patch a few cells across would be far sharper than the ground under it. Not planned. |

## The rocks

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Stones at the foot of rock faces under Enhanced, with rigid-body physics** (`game/foliage3d_mod.h`: `rock_place_cell`, `rock_tick`, `rock_step`; pack from `tools/art/rocks/bake_rock3d.py`) | the trees' GLSL program as bark, batched into the foliage's per-chunk vertex buffers while still; a moved rock drawn alone with a full rotation (`uRock`, `uRot` in TREE_VS); a per-rock state texel read in the vertex stage hides the still copy of a moved one. The physics is CPU arithmetic on the engine tick | **NONE. TIER 2 ONLY** for the drawing, for the foliage's reasons: shader, vertex buffers, vertex texture fetch. The physics itself is plain C and would run anywhere, and the stones are opaque meshes a Voodoo 2 could draw untextured-lit; a Tier 1 version would be the same bodies drawn through the fixed-function mesh path. It is reached only through experimental art, which Win98 does not run. Not planned. |


## Terrain displacement by parallax

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Cliff and road relief by parallax occlusion mapping** (`CLIFF_POM_FS`, `cliff_relief_draw_pom`), the default method of the experimental art's displacement | a fragment program marching up to 48 steps through the height atlas per pixel, reading the neighbour tiles past the cell's edge | **ABSENT, like the lattice path it replaces.** Per-pixel ray marching is a shader or nothing; Win98 does not run experimental art and draws the plain cliff and road tiles. |

## The graphics modes and SPACE's wipe

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Three graphics modes** (Classic / Enhanced / Re-Engineered) on the Visuals screen, each with its own Advanced settings | a choice between the chain off, the chain on, and the chain on with experimental art; per-mode rows in `FxState::mode_row` | **ONLY CLASSIC EXISTS ON WIN98.** Enhanced and Re-Engineered are the post chain and the experimental art, both Tier 2 only (see above). The dialog itself is 8-bit DOS art and would draw on Tier 1 unchanged; a Win98 build would offer one mode, or grey the other two. |
| **SPACE's energy-line wipe and mode title** (`game/gmwipe_mod.h`) | a `glCopyTexSubImage2D` snapshot of the finished frame, quads, lines and additive blending, and an upscaled alpha font; plus ahead-of-time work (a worker thread for the Remastered atlas, a hidden warm-up frame per mode) | **ABSENT, NOTHING TO CYCLE.** The draw itself is fixed function and a Voodoo 2 could do most of it (the snapshot would be a `grLfbReadRegion` into a texture), but with one mode there is no switch to animate. |


## The fixed cartridge buildings

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **A finished, healthy building drawn from its fixed and improved model** (`game/n64fix_mod.h`, `game/n64fix/<TYPE>.n64x` baked by `tools/art/n64fix/bake_enhanced.py`; one decision, `n64fix_mesh_for`, ahead of the base draw in `draw_object_mesh`, and the Database tab's turntable through `n64fix_codex_mesh`) | the approved model (north sides completed, about twice the triangles, textures raised 4x, 22 military types) is appended to the mission pack as one more `PackMesh` and drawn through the SAME fixed-function pack path as the cartridge's: vertex colour times texture, the cartridge's parts, pivots and node animation, its per-face wrap, its ground-shadow faces. The texture books (fans, silo fill, refinery, helipad and repair bay lights) are carried as retextured copies of the fixed triangles and drawn over them bias-free under `GL_LEQUAL`. Nothing in it needs a shader. Construction, selling, damage below half and the collapse keep the cartridge model, **except the structures with an Enhanced building file under Enhanced** (every approved remastered one): their fixed model is never used at all and they no longer keep the cartridge model below half health or in their collapse; `game/ebld_mod.h` draws every one of them intact, hit, broken and dying (see "The Enhanced buildings" below). Their construction and selling still show the cartridge's | **ABSENT ON WIN98, because Enhanced is.** Win98 draws the cartridge's buildings, which is the picture that build has always had. **The drawing itself would port unchanged**: it is the pack path, textures are powers of two at most 256 a side, and the book overdraw is the bias-free tie Glide does natively (see the texture-book row above). **What it would cost is texture memory**: 4.73 MB of RGBA for the set as approved, against a Voodoo 2's 2 to 4 MB per TMU, so a Tier 1 version would take the set at 16-bit and one mip down (about 0.6 MB), or the cartridge-resolution textures on the fixed geometry. Not planned; it would be a switch on Win98 rather than a port. |


## The remastered particle effects

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Remastered effect art and Enhanced's added particles** (`efx_enh.pack` from `tools/art/remaster/fx/make_fx.py`; `game/efx_enhanced.h` from `gen_efx_enhanced.py`; the `efx_enh` dial) | the same particle pool and the same GL 1.1 draw as the cartridge's effects: immediate-mode quads, alpha blending, one power-of-two texture per frame, batched by system and frame. What changes is the texture on each quad (new sheets, at most 16 frames, 16 to 64 texels a side), four extra draw slots for the sheets single sources wear, and a few extra particles (muzzle flashes, the landing-zone flare) | **WOULD PORT UNCHANGED.** Nothing in it needs a shader, a render target or an extension. Texture memory: 0.52 MB of RGBA for the whole pack, against the cartridge pack's 0.05 MB, well inside a Voodoo 2 TMU even before 16-bit. Absent on Win98 today only because Enhanced is. The debris trails' embers drawn as streaks rather than squares (`efx_streak_batched`, Enhanced only) are four vertices each in the same batches, one texture row stretched along the quad: they port unchanged too. |
| **Remastered building aprons** (`bibenh.pack` from `tools/art/remaster/bib/bake_bibenh.py`; `game/bibenh_mod.h`; the `bib_enh`, `bib_enh_strength`, `bib_enh_cover` dials) | three textures per theater and pass (256x128, 128x128, 128x128 after power-of-two padding), immediate-mode triangles on the terrain's own corners drawn three times: an ordinary alpha blend (the cover pass, lit by the ground's corner shade like the cartridge apron), then twice a DST_COLOR / SRC_COLOR blend (a doubled multiply, dst * (2 src)^2 in all). The dials rebuild the textures on the CPU, so no texture combiner is needed. **Tier 2 draws them in ONE pass through a shader instead** (`BIBENH_FS`), reading a copy of the scene taken under the aprons' screen rectangle just before they draw (`glCopyTexSubImage2D` into a viewport-sized texture, float when the scene target is): the colour ratio K is right only for the ground it was calibrated on (the pack's per-theater calibration ground), and on a dirt road, bare earth or rock it turned the ground salmon pink, so the shader passes the calibration-coloured part of the ground through the old cover and multiply exactly and carries the rest as a plain brightness gain. Gate G322 | **WOULD PORT, WITH ONE RISK AND ONE KNOWN DEFECT.** The three passes are the Tier 1 answer, and are what Tier 2 draws when the shader is unavailable, a version 3 pack has no calibration ground, or `bibhue 0` asks: both blends are native on the Voodoo 2. In 16-bit colour each multiply pass rounds to 5 or 6 bits, which may band the soft fade; the fallback is one multiply pass capped at 2 x with the cover dial raised. **The defect:** with no shader and no copy of the scene, Tier 1 cannot read the ground per pixel, so an apron over ground that is not the calibration grass or sand (roads, bare earth, rock) keeps the pink cast (measured on SCG15EA's Temple of Nod: 18% of its pixels in the pink band). Raising the cover dial towards 1 on Tier 1 removes it at the cost of the multiply's look. Absent on Win98 today only because Enhanced is. |



## The Enhanced buildings

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **A building drawn from its remastered model and broken apart hit by hit, under Enhanced** (`game/ebld_mod.h`; `game/ebld/<TYPE>.ebld` baked by `tools/art/remaster/bake_ebld.py`, today only NUKE, the Power Plant; `--noebld` switches it off; gate G273) | intact, the remastered model (796 triangles, painted light) with its water surface as a 4x4 flipbook. Each 5% of strength lost knocks a chip off the side the hit came from and fades a patchy damaged overlay into that side (an alpha test at a moving threshold); at half strength the loose pieces break off as rigid bodies, the water drains and a waterfall pours, the damaged texture and a flickering ember glow cover it and the camera jolts; at death about 200 shards are blasted clear, then sink and fade over a scorch mark. Steam, embers, smoke and fire go into the existing particle pool. Every piece is one `draw_mesh` call with a per-piece 3x4 through the fixed-function pack path; the flipbook and the waterfall scroll are a scale and offset applied to the texture coordinates on the CPU; the glow is `GL_SRC_ALPHA, GL_ONE` blending with depth writes off. As in the lab viewer, the pieces are drawn one-sided (back faces culled, except the few the file marks two-sided) and their broken faces, the rims along the cuts and the linings behind the surface, are drawn with `glPolygonOffset(1, 2)` so a surface always wins where they meet it. The physics is plain C++ on the engine tick (four substeps, gravity, hull-point contact with the ground, friction, sleep). Nothing in it needs a shader. The pieces are also drawn into the sun shadow map, the reflection and the relight bake, which are Tier 2 passes | **ABSENT ON WIN98, because Enhanced is.** Win98 draws the cartridge's Power Plant, its damage overlay and its section shed, as it always has. **It would port**: immediate-mode GL 1.1 through `draw_mesh`, the alpha test, additive blending and per-vertex texture coordinates are all things Glide does natively, and the physics would run anywhere. **What it would cost**: texture memory, because the base, damaged, broken, glow, overlay, water, waterfall and scorch textures go up to 1024 a side and one 1024-square RGBA texture is 4 MB, a whole Voodoo 2 TMU, so a Tier 1 version would take them at 16-bit and at most 256 a side; and draw calls, because about 200 shards is up to 400 `draw_mesh` calls a pass (surfaces and broken faces are separate calls) for the 8.5 seconds a wreck lasts, the same count x duration x draw calls blocker as the shatter row above. **Glide has no polygon offset**, so the broken faces would need the same kind of answer as the damage overlay row: a small inward inset of the rims and linings, baked into the mesh, instead of a depth bias. Face culling is native (`grCullMode`). The shadow, reflection and relight passes it joins do not exist on Tier 1, where it would simply not be drawn into them. Not planned. |
| **Every other approved remastered structure's damaged stage and effects, and the remastered walls, under Enhanced** (`game/ebld_mod.h`, format version 4; `game/ebld/<TYPE>.ebld` for AFLD, ATWR, BIO, EYE, FACT, FIX, GTWR, GUN, HAND, HOSP, HPAD, HQ, MISS, NUK2, OBLI, PROC, PYLE, SAM, SILO, TMPL, WEAP (version 4) and the walls SBAG, CYCL, BRIK (version 3); `--noebld` switches it off; gates G274, G295) | intact, the remastered model with its moving parts (per-piece 3x4 matrices on the CPU), flipbooks (a texture-coordinate scale and offset) and the Silo's fill (vertices clamped to a level on the CPU). Hit, the Power Plant's whole scheme on a cut model of 3,400 to 16,500 triangles: chips, the patchy zone overlay (alpha test), at half strength loose pieces as rigid bodies that also rest on the standing building's upward faces (a CPU grid), turning parts that keep turning, looping and bending parts (CPU matrices on the tick clock), a rolling sphere, draining pools and pouring spills (CPU scale and texture scroll), two-sided glass, an additive flickering glow; at death every piece is held or keeps falling and all of them sink and fade over the scorch mark. Its emitters go into the existing particle pool with the lab's bursts, gravity and gates (sparks skip off the ground on the CPU), and a point light each where the lab has one, which is the Tier 2 light pass. Nothing in it needs a shader. Textures go to the card in two halves, the whole model's at its first sight and the damage's at a hit building's first draw; the cut's meshes are made at a type's first hit | **ABSENT ON WIN98, because Enhanced is.** Win98 draws the cartridge's buildings and walls, their damage overlay, section shed and shatter, as it always has. **It would port** in the same way as the Power Plant row above: immediate-mode GL 1.1 through `draw_mesh`, alpha test, additive blending, face culling, CPU physics and matrices. The broken faces would need the baked inset instead of `glPolygonOffset`, and the lights would be dropped (the light pass is Tier 2). **What it would cost** is the Power Plant row's two blockers, larger: texture memory (1024-square atlases with their Nod decodes, damaged copies and overlays, 443 MB of files for the set, so a Tier 1 version would take every texture at 16-bit and at most 256 a side), and triangles, because a broken building is ten to twenty times its whole model's triangles, in immediate mode, which the Voodoo 2 target cannot afford for more than one or two on screen: a Tier 1 version would draw the held stage from the intact model and pre-bake each damaged stage's fall as a static broken model. Not planned. |
| **The hand-painted remasters in the game, format version 5** (`game/ebld_mod.h`, `game/ebld/<TYPE>.ebld` for all 22 structures and the three walls, baked by `tools/art/remaster/bake_ebld.py`; `--noebld` switches all of it off, `--noebldnormals` only the normals; gates G273, G274, G295, G303): (1) **atlases of 2048 and 1024 texels a side**, stored as zlib streams and inflated on the CPU at a model's first draw, uploaded with a mipmap chain built by hand (no GLU); (2) **a smooth normal per corner** drawn into the light pass's normal target with a class of its own (0.38), and **self-lit surfaces** (molten pools, lit lenses, status lights) with another (0.15) that the light pass leaves without sun, shadow, cloud, relight or occlusion; (3) **the Barracks' flag waving**, its corners, normals and fold light rewritten on the CPU each frame and the light above white drawn with the texture environment's colour scale at 2; (4) **a destroyed set for every structure** (180 to 605 shards blasted, sunk and faded over the type's own scorch); (5) **the walls' damaged and destroyed stages per cell**, from the engine's wall damage level, each connection mask its own cut and shards | the draw is still immediate-mode GL 1.1 through `draw_mesh`; the mipmaps, the decompression and the wave are CPU work; the colour scale is `GL_COMBINE` with `GL_RGB_SCALE` 2 (GL 1.3 fixed function); the normal target and its classes are a Tier 2 render target and a GLSL program (`ebld_draw_normals`, `FX_FS_LIGHT_BODY` in `game/fx_post.h`) | **ABSENT ON WIN98, because Enhanced is.** Item by item: (1) the 2048 atlases are eight times a Voodoo 2's 256 limit and 16 MB each; a Tier 1 version would take every texture at 256 a side, 16-bit, its own mipmaps (Glide mipmaps natively); (2) **TIER 2 ONLY**: there is no light pass on Tier 1, so there is nothing to give a normal to and nothing to leave a self-lit pixel out of; the remasters' painted light stands alone there, as the cartridge's baked shade does; (3) the wave would port (CPU vertices, and the Voodoo 2 combiner can scale by 2), or the flag would be drawn darken-only with its fold light clamped at the painted colour; (4) and (5) are the count x duration x draw calls blocker of the rows above, larger: up to 605 shards at two draw calls each for 8.5 seconds, and a wall cell's twenty shards per cell, so a Tier 1 version would draw the cartridge's section shed and the cartridge walls' own damage sets. Not planned. |
| **A remastered structure going up part by part, under Enhanced** (`game/ebld_place.h`; `game/eplc/<TYPE>.eplc` baked by `tools/art/remaster/bake_eplc.py` from the lab's placements, all 22 structures and 3 walls; `--noeplc` switches it off; gate G304) | the remastered model split into the lab's actors (6 to 73 a type) and drawn as runs of its intact mesh, each a `draw_mesh` call with a 3x4 computed on the CPU per actor (slides, hinges, scales, shakes, a parent chain); pipes and braces that grow, and a part that bends (the Hand of Nod's hand closing round its globe, the format's version 2 morph), have their vertices moved on the CPU; up to five user clip planes (`glClipPlane`) per actor cut it at the ground, the slab's top and what it comes out of; the dust is ordinary particles in the existing pool; the ring of broken earth an alpha-blended textured grid on the terrain; the rumble a camera offset. Nothing in it needs a shader | **ABSENT ON WIN98, because Enhanced is.** Win98 shows the cartridge's construction reveal, as it always has. **It would port, with one loss**: the matrices, the CPU vertex moves, the particles, the ring and the camera offset are all native on Glide. **The clip planes are not** (Glide has none). Most are not needed there: a part below the slab is below the terrain, which is drawn first and writes depth, and a part sunk inside a closed block fails the depth test against its faces. The few that show outside what they come out of (a stack's hoops, a beam's ends behind its wall, a door's upper panel raised into the wall over a doorway) would be drawn only once their section or slide has finished. Draw calls: up to about 280 runs a pass for the Weapons Factory for its four seconds, the shatter's count x duration x draw calls question again, smaller. Not planned. |
| **The civilian buildings and the two fences drawn, worn down and destroyed from their remasters, format version 6, under Enhanced** (`game/ebld_mod.h`; `game/ebld/V01..V37.ebld`, `BARB.ebld`, `WOOD.ebld` baked by `tools/art/remaster/bake_ebld.py`; `--noebld` switches it off; gate G308): (1) **thirty-seven civilian buildings** (houses, farms, churches, the fields and haystacks in their structure form, the oil pump, the desert town, the studio) with their cut, their destroyed set, their emitters and their own scorch, read from file at first sight; (2) **per-frame part animation** (CLIP): the oil pump's beam, cranks, pitman arms, carrier bar and ropes moved by a 3x4 matrix per frame and part on the CPU, on the cartridge's own clip frame while whole or held and on the cut's own looping clip once damaged; the same records carry an **idle loop** on the engine clock and **bursts sync'd to it** for the terrain objects; (3) **the death's centre** moved to the cells a civilian blocks (the blast's direction and the scorch's anchor); (4) **the barbed wire and the wooden fence as walls** with sixteen connection pieces and per-mask stages; (5) **a theater's own look** of a type, chosen at the pack load | the same immediate-mode GL 1.1 draw through `draw_mesh` as the structures above: per-piece 3x4 matrices (the clip is a matrix looked up per frame instead of computed), texture-coordinate flipbooks, alpha test, additive glow, `glPolygonOffset` on the broken faces; the physics, the clips and the bursts are plain C++ on the tick | **ABSENT ON WIN98, because Enhanced is.** Win98 draws the cartridge's civilians and fences, the pump nodding on the cartridge's clip, and their section shed, as it always has. **The clip itself would port unchanged**: a matrix table per part is what the cartridge's own node animation already is on Tier 1 (`mesh_anim_t`), and the remaster's 141 + 488 frames of 5 to 6 parts are 40 KB of floats. The theater choice and the moved blast centre are bookkeeping and port as they are. **The rest is the structure rows' cost again**: atlases up to 2048 a side (a Tier 1 version takes them at 256 and 16-bit), a broken civilian of 2,000 to 13,000 triangles and a wreck of 35 to 500 shards at two draw calls each, the baked inset instead of the polygon offset, and no light pass for the self-lit windows. Not planned. |
| **A remastered structure dies as the lab's Destroy plays it, and the engine's own death and damage effects stand down, under Enhanced** (`game/ebld_mod.h` `ebld_boom_step`, `ebld_anims_filter`, `ebld_smudges_filter`; the BOOM trailer of `game/ebld/<TYPE>.ebld`, carried by `tools/art/remaster/bake_ebld.py --boom`; `g_efxBeforeIntegrate` in `game/effects_mod.h`; `--noebld` switches it off, `--eboomlab` hashes every explosion as the lab does; gate G309) | one explosion per death, the lab's FRAG3 recipe read from the file (29 slots played: flash, big fireball, flat shock ring, debris chunks with their ember streaks, sparks, dust, the smoke column, fire, lingering smoke, embers on the scorch and their wisps), emitted through the existing particle pool at the death's centre, a few hundred to about 3600 sprites over its 15 seconds; the engine's fireballs, fires, smoke column and craters for a remastered building taken out of the anim and smudge lists before anything reads them; the death's jolt sized by the lab's cells | **the same, nothing Tier 2 in it**: the explosion is sprites and chunk meshes in the pool every Enhanced effect already uses (textured quads, the sheets' alpha, no shader), and the stand-downs are list edits on the CPU. The cost on a Voodoo 2 is fill rate: the big fireball and the smoke column are large overlapping alpha quads, so a death on a crowded screen may want the pool's sprite count or the smoke sheet's size cut on Win98. The particle sheets are the approved Enhanced remasters (efx_enh.pack); with Enhanced effects off the same recipe draws in the cartridge's own sheets. |

| **The remastered terrain objects and cell decorations, under Enhanced** (`game/ebld_terr.h` and the terrain path of `game/ebld_mod.h`; `game/ebld/ROCK1..7`, `SPLIT2`, `SPLIT3`, `SPLIT2_WIN`, `SPLIT3_DES`, `SPLIT3_WIN`, `P04`, `P04_DES`, `SCRATE`, `WCRATE`, the field overlays drawing `V13..V18`; `--noeterr`; gate G307): the desert rocks and the blossom trees drawn whole from their remasters, the trees' eight-second idle loop moving their parts (nine or ten) by per-frame matrices and its spore bursts (glow motes with gravity, a haze puff, a flash of light) synced to it; the bonus crates, the fields and haystacks placed as overlays and the crashed Orca on rock patch P04 drawn from theirs at the cartridge's anchor, a look per theater; all of it cast into the sun's shadow map and drawn into the light pass's normal target; and under every blossom tree a GROUND STAIN, green Tiberium veins spreading from its roots (`eterr_draw_stains`, `--noeterrstain`): the 3D crystals' own ground crack, six quads of it about the roots, each a 6 x 6 grid on the terrain whose vertex colour fades to nothing at its rim | the draw is immediate-mode GL 1.1 through `draw_mesh` (130 to 4,140 triangles a model, 1024 and 512 atlases with mipmaps), the clip a CPU matrix per part per frame, the particles the existing efx pool; the normal target and the sun map are Tier 2 render targets; the stain is GL 1.1 immediate mode, one 128 texture (the crystals' crack, GL_CLAMP), additive `GL_ONE, GL_ONE` with depth writes off and per-vertex colour, no shader | **ABSENT ON WIN98, because Enhanced is**: Win98 draws the cartridge's meshes of every one of them (10 to 60 triangles), as Classic does on every platform. **What would port**: the rocks, crates, fields and wreck as plain textured meshes at 256 texels a side (a few hundred triangles each would want a lower-detail bake), the trees' loop as its CPU matrices, the spores as the Win98 sprite particles; the normals and the sun map would be absent, the painted light standing alone as the cartridge's baked shade does. The ground stain would port as it stands (one small texture, an additive blend and per-vertex colour are all Glide's, as the crystals' crack it copies is declared Voodoo 2 safe), but it belongs to the remastered tree and to Enhanced's crystals, neither of which Win98 has, so it is absent with them. Not planned. |
| **The remastered trees (Foliage: T01 and T16, T02, T03, T05, T06, T07, T10 to T15, T17 temperate; the cacti T04, T09 and the acacias T08, T18 desert)** (the intact model drawn under Enhanced in its own theater through the terrain path of `game/ebld_mod.h` and `game/ebld_terr.h`, `game/ebld/T01_TEM.ebld` and the other trees' `<CODE>_TEM` / `<CODE>_DES` files, T16 drawn from T01's file; the rebuilt photogrammetry tree stands down for them; the wind played in the game from the file's weights; the burning stage and the snag are the lab's only so far; `tools/art/remaster/foliage`) | alpha-cut needle-spray cards on limbs: an alpha test at one half, two-sided, a 1024 spray sheet with a coverage-held mip chain uploaded as ordinary levels, the light as per-vertex colour, a per-vertex wind weight and needle flutter weight, played by the lab every frame (a slow lean and a faster shimmer of the cards); the burning stage (1,447 triangles) whose fire is a painted flame flipbook on crossed cards up the trunk and inner limbs (cut out, self-lit, three flipbook clocks) with a soft glow flipbook inside the crown, and the charred snag (517), with particle smoke and embers; about 1,450 triangles a tree | **WOULD PORT**: an alpha test, two-sided drawing, mip levels and vertex colour are all fixed-function and Voodoo 2 safe; the sheet would go down to 256 for the texture limit (coarser sprays) and THE WIND: as built it is NOT a shader: `draw_mesh` moves each corner on the CPU from the two weights (the lean once a draw, the shimmer from two phases kept per corner, no sine per corner), each tree phased by its cell, in every pass, so it ports as it stands (about 4,400 corners a tree a pass; on a Voodoo 2 machine the CPU cost for a forest may not be affordable, and the tree standing still, `--notreewind`, is the declared fallback). THE FIRE: Tier 2 the flame cards with their frame picked per draw (a UV or texture-matrix offset), alpha-tested and unlit; Tier 1 the same, fixed-function (an alpha test and a per-frame texture offset are Glide's), the glow cards alpha-blended or left out. The smoke and embers use the existing particle path (Win98 sprite particles). THE COVERAGE-HELD MIP CHAIN is built by the engine at upload (`ebld_upload_cover`, levels uploaded one by one, no shader). In the game today only Enhanced draws the remaster, so Win98 draws the cartridge's tree, as Classic does. |

## The Enhanced textures

| Feature (Tier 2) | What it is | Tier 1 |
|---|---|---|
| **The Enhanced terrain textures** (`FX_TEX_ENH`, Enhanced's default terrain; `rt_build_atlas_enh` in `game/remaster_tex.h`, `terrain_enh_ensure` in `game/cnc_eyes.cpp`; packs `terrainenh_<THEATER>.pack` from `tools/bakery/pack_remaster_terrain.py`) | every 1995 tile template of Temperate, Desert and Winter remastered at 144 texels a cell, and SNOW's (Red Alert's snow art on the winter template set) the same way, its sheet 4992 x 4212. 144 is 24 x 6, so each cell goes into the theater's sheet one texel for one (tile 24k at k = 6, the cells' UVs unchanged), and the sheet is kept at its own size rather than padded to a power of two: 4992 wide and 3744 (Temperate), 4212 (Winter) or 4836 (Desert) tall, one RGBA sheet of 75 to 97 MB per theater, no mipmaps | **ABSENT. Win98 draws the 1995 DOS atlas**, which is what Enhanced drew before and what DOS Textures still draws (on SNOW, which has no DOS atlas, the SNOW bank the cartridge path draws). The sheet is about twenty times the Voodoo 2's 256x256 texture limit and twenty times its 4 MB of texture memory. A Tier 1 answer would be the cells reduced to 48 texels (24 x 2, still a whole multiple, so the UVs stay simple), five to a 256 page, paged per visible cell. Not built. |
| **The cliff variants** (`enh_cell_variant` in `game/cnc_eyes.cpp`; extra cells in `terrainenh_<THEATER>.pack`) | two more paintings of each cliff piece that the maps lay beside a copy of itself (eleven pieces each in Temperate and Winter, nine in the Desert), placed in rows of their own below the sheet's used height (two to four rows of 156 texels), and a cell's texture coordinates moved to its placement's variant | **ABSENT, with the Enhanced textures they belong to.** Nothing in them needs more than a texture coordinate offset, so they would come down to 48 texels with the cells if Tier 1 ever paged the Enhanced terrain. Not planned. |
| **The join blend** (`game/terrain_seam.h`, `draw_terrain_seams` in `game/cnc_eyes.cpp`; packs `terrainseam_<THEATER>.pack` from `tools/bakery/pack_terrain_seams.py`) | each terrain piece's painting carried a quarter cell over the piece beside it, so neighbouring cliffs, roads, beaches and rivers read as one painting: 892 to 1102 strips per theater, 144 x 36 texels each, in two RGBA sheets of 16 MB. A cliff variant has strips of its own, keyed with its variant. Drawn after the ground as textured, alpha-blended quads on the neighbour's own surface, lit with the ground's own per-vertex light and tint, the strip's alpha multiplied on a second texture unit by the neighbour cell's water cut (so no strip lands on the sea), a polygon offset, no depth write | **ABSENT, with the Enhanced textures it belongs to.** It would port: two TMUs with `grTexCombine` for the alpha multiply, alpha blending and Gouraud colour are what the Voodoo 2 does natively. Glide has no polygon offset, so the strips would be lifted a hair off the ground instead, and the strips would come down to 48 x 12 with the cells. Not planned. |

## The remastered cursors

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Every cursor drawn as its remastered model with a glow, under Enhanced** (`game/cursor3d_mod.h` `ecur_*`, `game/ecur/ecur.bin` baked by `tools/art/remaster/cursors/bake_ecur.py`) | 23 models (the 14 state-table shapes, the edge arrow, the move marker, the 7 flipbook frames) appended to the loaded pack as ordinary meshes and drawn through `draw_mesh`: 21 RGBA textures at 256 x 256, texture times vertex colour, the cartridge meshes' own baked node animation, and a glow of MODE_XLU triangles in one extra pass with `glBlendFunc(GL_ONE, GL_ONE)`, depth tested, depth writes off | **DRAWS AS IS, NOTHING TO PORT.** Every texture is 256 a side, the Voodoo 2's limit; texture times vertex colour, per-triangle mode filtering and a pure additive blend are fixed function; the mipmaps are built by hand, no GLU. 16-bit colour will band the glow's falloff a little. |


## The remastered vehicles and aircraft

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Every vehicle and aircraft drawn from its remastered model, its parts moved by the unit's own state, under Enhanced** (`game/evhc_mod.h`; `game/evhc/<TYPE>.evhc` for all 21 types, baked by `tools/art/remaster/bake_evhc.py`; `--noevhc` switches it off; gate G296) | the remastered model (375 to 758 triangles) as one piece per motion, each a `draw_mesh` call with a per-piece 3x4 computed on the CPU: the turret on the turret facing, wheels rolled by each side's travel and steered, rotors, hinges (doors, ramps, the launcher's elevation, the Orca's ducts), a barrel's recoil, the harvester's arms on the cartridge's own clip, the harvester in the refinery's dock rig and the MCV's five groups in its deploy rig on the rig's own matrices. A track's belt scrolls and its wheels step a flipbook by the draw hook's texture-coordinate scale and offset (the remastered buildings' water). A rotor's blur disc is a MODE_XLU group faded by a per-draw alpha. House paint is the two-decode pack texture plus the skirmish liveries painted through the mask on the CPU. Nothing in it needs a shader. Its textures go to the card at first sight: 47 MB for the sixteen ground types of the test map (two decodes of 512 and 256 atlases, a link tile, a wheel flipbook sheet up to 1024 wide) | **ABSENT ON WIN98, because Enhanced is.** Win98 draws the cartridge's vehicles and aircraft, their transport doors and rigs, as it always has. **It would port**: immediate-mode GL 1.1 through `draw_mesh`, CPU matrices, texture-coordinate offsets and an alpha-scaled translucent group are all things Glide does natively; about eight to fourteen draw calls a vehicle instead of one. **What it would cost is texture memory**: every model carries a 512-square atlas and its Nod decode (2 MB of RGBA), so a Tier 1 version would take them at 16-bit and 256 a side, and the wheel flipbook sheets (up to 1024 x 256) at half size, as the vehicle recipe's own Tier 1 note says. Not planned. |
| **A destroyed vehicle breaks into its remastered wreck; an aircraft falls whole and breaks where it lands** (`game/evhc_mod.h`, the lab's destroyed models: 1280 to 4886 triangles in 15 to 40 pieces a type) | the pieces start where the intact pieces stood (a turret at its aim), with the unit's own motion, and are thrown by their role's blast; the remastered buildings' rigid-body solver (plain C++, the tick edge) settles them on the terrain; after 3.5 s they sink and fade. A shot-down aircraft falls under the engine's own debris gravity as one body turning, and breaks at the ground with fire and smoke from the particle pool. Surfaces charred, caps and linings in the broken material pushed back by `glPolygonOffset(1, 2)`, an additive ember glow. The charred textures go to the card only when a wreck of that type is first drawn | **ABSENT ON WIN98, because Enhanced is**: Win98 keeps the cartridge's own answer to a dying vehicle (its VehicleDamage chunks), as the shatter row at the top says. **It would not port as it is**: up to 4886 triangles and 40 pieces for one death, two draw calls a piece, for about nine seconds, is the count x duration x draw calls blocker of the building shatter, larger; and Glide has no polygon offset (the caps would need an inset baked into the mesh). A Tier 1 version would take the derived wreck the baker can still make (a handful of whole pieces from the intact model, no caps) if it wanted one. Not planned. |

## The tiberium's colour, light and fog

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Crystal colour** (`game/tibglow_mod.h`, `tib_hue`, `tib_sat`; F5 > 12b) | the crystal sheet (256x256) and the crack tile (128x128) are repainted on the CPU from the originals kept at load, turning only the texels near the crystals' own hue so the pods' rock stays brown, and uploaded again with `glTexImage2D` when a dial moves | **WOULD SHIP ON TIER 1.** A repainted texture is a texture: same sizes, same formats, one re-upload per dial move. It inherits the solid tiberium row's status above, which is the real gap: Tier 1 does not draw the crystals at all yet. |
| **The light a tiberium field gives off** (`game/tibglow_mod.h`, `light_tiberium`, `tib_light`, `tib_light_spread`, `tib_light_height`; F5 > 6) | a 128x128 RGBA texture of the whole map (glow for light, glow for fog, ground height), rebuilt on the CPU when the field or the shroud changes, read under every pixel in the light pass of `game/fx_post.h` | **ABSENT ON WIN98**, like every light in the post chain. The fixed-function answer would be the texture itself drawn as a second additive pass over the terrain (it is already a map-sized texture in world coordinates, so it projects with the terrain's own texture coordinates scaled by 1/64), which lights the ground and nothing standing on it. Not planned. |
| **The glowing fog over a field** (`game/tibglow_mod.h`, `tibfog_*`; F5 > 12o) | a ray march of up to 32 samples per pixel in the light pass, each reading the field texture and two octaves of the cloud deck's noise | **ABSENT ON WIN98, AND NO CHEAP ANSWER.** A march per pixel is a shader by definition. The nearest fixed-function look is a few large additive camera-facing cards over each field, sorted back to front, which reads as a glow rather than a fog and costs fill the Voodoo 2 does not have to spare over a large field. Not planned. |

## The distance fog

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Distance fog under Enhanced** (`game/distfog_mod.h`; one block at the end of the light pass in `game/fx_post.h`; F5 > 3e, `dfog_on`, off by default, one set of dials each for Temperate and Desert) | every lit pixel's world position, reconstructed from the depth texture, is measured from the eye; past the look-at distance plus a start dial it is mixed toward the theater's haze colour, rising on a smoothstep over a depth dial to a thickness dial. Gated per pixel on the scene's luminance so the shroud and the void past the map edge stay black. No pass, target or texture of its own: four uniforms and a branch in a program that already runs | **ABSENT ON WIN98, because Enhanced is. It is the one Enhanced look that would port almost whole.** The Voodoo 2 has hardware fog: Glide's `grFogTable` (a 64-entry table indexed by w) or `GL_FOG` on the fixed pipeline, `GL_LINEAR` with `GL_FOG_START` at the look-at distance plus the start dial and `GL_FOG_END` that plus the depth dial, `GL_FOG_COLOR` the theater's haze, and the thickness as the colour's own blend (a table that tops out below 255). Two differences from Tier 2: the fog is per vertex rather than per pixel, which on a 24-texel terrain cell is invisible, and the luminance gate has no fixed-function form, so the shroud blanket would be fogged too unless it is drawn with fog disabled, which is one state change around its draw. Not planned. |

## The modern placement cursor

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Placing a building in Enhanced shows a modern RTS placement cursor** (`game/cnc_sidebar.h` `sb_draw_place_world` and `sb_draw_place_signs`, `game/cnc_eyes.cpp` `draw_place_ghost` and `place_origin_at`): a ground grid of "+" marks over every cell the building could reach within about seven cells, fading with distance and under the shroud; the base's own cells washed amber; the footprint green or red per cell with a bright rim; a red prohibition sign on each refused cell; and a see-through, tinted copy of the building itself centred on the pointer | immediate-mode quads draped on the terrain's corner heights in two materials made in code at start-up (a glowing mark, 64 x 32, and a glass cell panel with rim, corner brackets and scan lines, 128 x 128; RGBA, mipmapped by hand, `GL_CLAMP`, coloured through `GL_MODULATE`), each drawn once blended and once added for the glow; the grid and the amber cells depth tested with `glPolygonOffset(-2, -8)`, the footprint with the depth test off; the ghost is the building's own model through `draw_mesh` (tint and per-draw alpha hooks) after a colour-masked depth pass, then one blended and one additive colour pass; the signs are triangle strips in the pixel pass | **ABSENT ON WIN98, because Enhanced is**: Win98 keeps the 1995 cursor (the footprint's cells, green or red), which is what Classic draws on every platform. **It would port almost as it stands**: blending, a colour mask (`grColorMask`), a depth mask and an additive blend are all Glide; the one call Glide lacks is the polygon offset on the grid and the amber cells, which would draw with the depth test off as the footprint already does (a mark then shows over a tree standing on its cell). The ghost costs three passes of one building model a frame. |

## The structures group of the F5 panel

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Buildings and walls tuned on their own under Enhanced** (`game/fx_state.h`, F5 > 12p, `st_gamma`, `st_bright`, `st_contrast`, `st_sat`, `st_shade`, `st_sun`, `st_glow`, `st_ao`; the STRUCTURES blocks of the light pass in `game/fx_post.h`; `str_draw_class` in `game/cnc_eyes.cpp`; gate G305) | the light pass tells a structure's pixels by the class in the normal target's alpha (a remaster's 0.38 and its self-lit 0.15, and 0.05 written for the cartridge's buildings and walls by one extra alpha-only draw of them with `GL_MAX` blending while a dial is off neutral). On those pixels it grades the colour before lighting (a power curve, a gain, contrast about middle grey, saturation about luminance), splits the sun-shadow tint into a shadow colour scaled by shade depth and a sunlit lift scaled by sun strength, scales the occlusion's darkening and a self-lit pixel's colour. All neutral at 1, where the old path is taken unchanged | **ABSENT ON WIN98, because Enhanced and its light pass are.** Nothing here can be per pixel on a Voodoo 2: no normal target, no class, no sun shadow or occlusion to scale. **What would port is a per-draw colour on the structure itself**, which is exactly a structure's own draw: brightness is a vertex colour scale (`draw_mesh`'s existing tint hook, `g_meshTintR/G/B`, or Glide's constant colour in the colour combine), so it would ship as is; saturation could be approximated per draw by mixing the vertex colour toward its own grey, which reaches only the shade, not the texture; gamma and contrast are curves on the texel and have no fixed-function form short of a second set of textures, so they would be absent. Shade depth and sun strength have nothing to scale on Win98 (it draws the cartridge's baked shade) and would be absent; the glow likewise (no self-lit surfaces without the remasters). Not planned. |

## The field overlays

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **The civilian fields and haystacks placed as overlays** (`draw_fields` in `game/cnc_eyes.cpp`, fed by the brain's `FIELD\|` lines; gate G306) | one `draw_mesh` per cell of the cartridge's own V12..V18 mesh (an overlay V12 with V13's), at the cell centre, in the opaque pass and the cartridge shadow pass, Classic and Enhanced alike; Enhanced also draws them into its shadow casters and relight bake, as it does the walls, and draws the remastered V13..V18 in their place where those are in the game (the terrain decorations' row under the Enhanced buildings) | **NATIVE: nothing in it needs a shader.** It is the walls' and crates' path, immediate-mode GL 1.1 through `draw_mesh`, 10 to 30 triangles a field. **The Win98 client in `tier1/` does not draw them yet**, and does not draw the crates either: `w98_brain.c` parses `TIB\|`, `SMUDGE\|` and `WALL\|` and ignores the rest. Adding them there is a parse of `FIELD\|` into `t1_over.c` beside the walls and one mesh draw per cell; not done. |

## Red Alert's own terrain at 144

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Red Alert's Snow and Temperate templates remastered at 144 texels a cell, with their join strips** (`art/remaster/textures/RASNOW`, `RATEMP`; `terrainenh_RASNOW.pack`, `terrainenh_RATEMP.pack` and their `terrainseam_` packs from the standard `tools/bakery/pack_remaster_terrain.py`, shipped in the play folder's `ra3d/`; drawn by the RA module's 3D scenes through the RASNOW and RATEMP banks) | about 1500 cells a theater at 144 x 144 RGB, keyed by Red Alert's own template numbers, two cliff variants in RATEMP, and RGBA strips 144 x 36 for the join blend, on exactly the terms of Tiberian Dawn's Enhanced terrain | **ABSENT ON WIN98, with the Enhanced textures they belong to**: a Win98 build draws Red Alert's 1995 tiles, which meet by design. Like Tiberian Dawn's set, the cells would come down to 48 texels if Tier 1 ever paged the Enhanced terrain. Not planned. |

## Red Alert's ground under Enhanced

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Red Alert's mission ground under Enhanced** (`game/ra3d_terrain.h`; `ra3d_world_ground`, drawn by `draw_frame` in `game/cnc_eyes.cpp` in the Red Alert module in place of the placeholder pack's sea, terrain and coast; packs `ra3d/terrainenh_RA_<TH>.pack` and `ra3d/terrainseam_RA_<TH>.pack` for SNOW and TEMPERAT; gate G310) | the map the Red Alert engine exports, flat at height 0, one cell per map cell from one RGB sheet of every key in the theater's pack at 144 texels a cell (about 6240 x 6084, 114 MB), the 1995 tile enlarged for a cell the pack lacks; a variant per placement (`enh_variant_pick`, shared with Tiberian Dawn's); the join strips laid by the code Tiberian Dawn's use (`ts_strips_begin`, `ts_strip_emit`, `ts_strips_end` in `game/terrain_seam.h`); lit as Tiberian Dawn's Enhanced flat ground (160/255 times the shroud, the post chain's sun over it). The 1995 set (texset 1, and Interior) is the same sheet at 24 texels a cell | **ABSENT. Tier 2 only.** Win98 has no Red Alert module at all, and the sheet is far past a Voodoo 2's 256 x 256 texture limit. If a Tier 1 Red Alert ever exists, the 1995 set at 24 texels a cell is the answer: one 24-texel tile per cell, which is what Classic already draws, with the strips left off (the 1995 tiles meet by design). Not planned. |

## Red Alert's sea under Enhanced

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Red Alert's sea under Enhanced** (`game/water_mod.h` reading Red Alert's map as its source, filled by `ra3d_water_source` in `game/ra3d_terrain.h`; the shader variants `WATER_RA_BASE`; gate G311) | Tiberian Dawn's Enhanced sea, unchanged in kind: the shore field (an RGBA texture from two distance transforms), the GLSL 1.20 surface and shore programs with the scene copy, the planar reflection render target and the half-float wave field, all on Red Alert's map. Red Alert's ground sheet becomes RGBA, its alpha the water cut read off the 1995 tiles' palette indices (bilinear to 144, faded on the water side); the ground is drawn alpha-tested and blended over the sea; the floor under the sea is the painted ground's holed cells drawn first; the base texture is Red Alert's painted open water (W2, 288 x 288 RGB, mipmapped with a LOD bias of -1), multiplied by a one-texel white texture in the second layer | **ABSENT. Tier 2 only**, as Tiberian Dawn's Enhanced sea is and as Red Alert's Enhanced ground is: Win98 has no Red Alert module, no shaders and no render targets. If a Tier 1 Red Alert ever exists its water is the 1995 tiles as painted, which is what Classic draws and what this ground draws with water_fx off. Not planned. |

## Enhanced structure submission

`game/mesh_batch.h` caches the existing remaster vertex pipeline's world-space
output in bounded VBOs, streams changing vertices and culls against the current
camera/shadow/reflection clip planes. This is a shared Tier 2 performance path for
Tiberian Dawn and Red Alert Enhanced. It changes no asset geometry or visual contract. Tier 1 continues
its separate fixed-function renderer; Classic and modern drivers without VBO support retain the original immediate submission.

The second pass adds a 64 MiB animated-store pool, lookup expiry each frame and
cloth revision invalidation, plus orphaned crystal glow updates. These use the
same Tier 2 buffers. Tier 1 remains on its separate renderer; Classic remaster
submission remains on the existing immediate path. No Tier 1 performance gain is
claimed.


## Enhanced moving air, 7 October 2026

Moving Temperate mist and Desert dust are Tier 2 only. The half-resolution 12-sample
volume march and depth-aware resolve require shaders, scene depth and render targets.
Tier 1 and Classic skip the pass and retain their existing clear air. See
`handoff-atmosphere.md` for the field-light integration, shroud contract and checks.

## The movies and music download

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **The movies and the music downloaded at install** (no package carries them; the Windows web installer, `tools/win/installer/opencnc3d.nsi`, and the launcher's Setup screen and Settings, `launcher/lmedia.c`, fetch them by HTTP byte range through `launcher/lnet.c`) | the launcher reads each movie and both music archives straight out of disc images on archive.org over HTTPS (WinINet on Windows, libcurl on the Mac), checks each file's SHA-256 and writes it into `dosdata`; Setup draws the launcher's progress | **ABSENT ON WIN98. Tier 2 only.** Not a renderer question, since no draw call is involved: the mirrors are HTTPS hosts, and the WinINet of Windows 98 stops at TLS 1.0, older than anything those hosts can be expected to accept (not measured on a Windows 98 machine). **The fallback is the 1995 way: the player's own discs.** Copy `SCORES.MIX` into `dosdata/`, the Covert Operations disc's `INSTALL/TRANSIT.MIX` into `dosdata/transit/`, and the movies as loose `.VQA` files into `dosdata/movies/` (they sit inside each disc's `MOVIES.MIX`, which `tools/extract_movies.py` unpacks, because the movie player opens loose files only). The game finds both by file presence, as it does on every platform, and plays without them. |

## The Obelisk of Light's beam (October 2026)

The engine reports each beam once (`EFX|LINE`, written by `OpenCNC3D_Note_Line` from the
block of `TechnoClass::Fire_At` that sets up the 1995 laser lines) and `game/laser_mod.h`
keeps it for as long as the mode shows it. Gate G320.

| Feature | Tier 2 | Tier 1 answer |
|---|---|---|
| **Classic**: the 1995 lines (`dos2d_draw_lasers`, `game/dosclassic_mod.h`) | three Bresenham lines in 1995 pixels, palette 0x7D, 0x7D and 0x7F, each pixel a scaled solid quad, drawn after the firer's sprite, for the engine's five frames | **the same drawing, fixed function**: solid-colour pixels, no texture, no blending. A Glide backend fills the same rectangles (or draws `grDrawLine` at 1:1). No shader, no render target |
| **N64**: the cartridge's beam (`laser_draw_n64`, decoded in `docs/n64-laser.md`) | one flat opaque triangle per beam, (255, 10, 10), depth tested and written, no culling, no texture, six frames; at most twenty at once | **the same triangle**: `grConstantColorValue` plus `grDrawTriangle` with the depth buffer on, which is what the console itself does. The console antialiases its edges; Tier 2 draws them hard too, so there is nothing more to give up |
| **Enhanced**: a bright beam with flares at both ends, the self-lit class under its core, a red dynamic light where it lands (`laser_draw_enhanced`, `laser_draw_normals`, `laser_light`) | a GLSL 1.20 program writing scene-referred colour (core about 5x white) into the scene target, so the bloom takes it; a class pass into the normal target; a slot in the dynamic lights | **none, Tier 2 only**, like everything under Enhanced. Tier 1 draws the N64 triangle above |
| **The Win98 program does not draw any of it yet.** `tier1/w98_brain.c` reads `EFX|BULLET` and `EFX|ANIM` from the brain's dump and not `EFX|LINE`, so on Win98 the Obelisk still fires unseen | n/a | the work is three pieces, none of them a rendering question: parse `EFX|LINE` (the firer's centre `clx`/`cly`, the target `tlx`/`tly`) into a list stamped with the dump's frame, drop a beam six frames after it was born, and draw the N64 triangle from it with the Glide calls above |
