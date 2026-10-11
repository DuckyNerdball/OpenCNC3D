# The Obelisk of Light's beam, read out of the cartridge

**Read-only reverse engineering of `data/rom/cnc_eu.z64` (EU).** Every claim below is tagged
**DECODED** (the instructions or bytes are quoted), **INFERRED** (a consistent reading, not
proven) or **NOT RECOVERED**.

Address conventions, as in `docs/pip-row.md`:

```
resident      rom = ram - 0x7FFFF400      (ROM 0x001000..0x0C6EA0)
CodeOverlay   rom = ram - 0x8005E210      (ROM 0x1643F0..0x1B67D0)
```

Units: **leptons**, 256 per cell. The cartridge's draw space is world X = lepton X, world Y
= up (leptons above the terrain), world Z = lepton Y (south positive), the same frame the
pip row and health bar use.

---

## What to draw

| item | value | tag |
|---|---|---|
| shape | **one flat triangle**, not camera-facing, in the vertical plane through the source and target points | DECODED |
| vertex A (source, top) | `(sx, H(sx,sz) + 444, sz)` | DECODED |
| vertex B (target) | `(tx, H(tx,tz), tz)`: on the terrain, height 0 above it | DECODED |
| vertex C (under the source) | `(sx, H(sx,sz) + 444 - w, sz)`: straight down from A by `w` | DECODED |
| width `w` at the source | `w = 25 * (1 + 444 / L)`, `L = sqrt(d^2 + 444^2)`, `d` = horizontal distance source to target in leptons. 50 directly under, 46.7 at 1 cell, 41.4 at 2 cells, 30.6 at the Obelisk's full range (1920). Tapers linearly to **0 at the target** | DECODED |
| source point `(sx, sz)` | `Center_Coord() + (-22, +29)`: 22 leptons west, 29 south of the footprint centre. For the 1x2 Obelisk that is `Coord + (106, 284)` from the footprint's top-left corner. Fixed, does not turn toward the target | DECODED |
| source height | 444 leptons (1.734 cells) above the terrain at the source point. The Obelisk mesh's apex is at 444.25 | DECODED (the apex match is INFERRED) |
| target point `(tx, tz)` | `As_Coord(target)`: a unit's or infantry's `Coord`, a building's `Center_Coord()`, or a cell target's centre | DECODED |
| `H(x,z)` | bilinear terrain height in leptons (`0x801F7DFC`), sampled at each end separately | DECODED |
| colour | **RGB (255, 10, 10), alpha 255**, `#FF0A0A`. Constant data, not per house, not animated | DECODED |
| shading | flat primitive colour only: no texture, no vertex colour, no lighting | DECODED |
| blend | **opaque** (`G_RM_RA_ZB_OPA_SURF`): no translucency, no additive, antialiased edges only | DECODED |
| depth | **z-tested and z-written** | DECODED (the `G_ZBUFFER` geometry bit is inherited, INFERRED on) |
| culling | none, both faces drawn | DECODED |
| duration | **6 game frames** (Main_Loop passes) at full colour, then gone. No fade, no flicker, no pulse | DECODED; seconds per frame NOT RECOVERED (0.4 s at a 15 Hz tick) |
| endpoints over time | snapshot at the moment of firing; the beam does not follow a moving target and outlives the Obelisk if it dies | DECODED |
| capacity | at most 20 beams alive at once; a 21st is silently not drawn | DECODED |
| also drawn | a random **scorch smudge** (SC1..SC6) at the target. **No** muzzle flash, **no** glow at either end, **no** impact explosion | DECODED (the SC1..SC6 naming is INFERRED) |

Tier 1 note: one flat-coloured, opaque, depth-tested triangle is fixed-function geometry. It
needs no shader and has a direct Voodoo 2 equivalent.

A consequence of the shape worth knowing before anyone calls a render wrong: because the
triangle stands in a vertical plane and the console camera has zero yaw (`docs/n64-camera.json`),
a beam fired due north or due south is seen nearly edge-on and reads as a hairline, while an
east or west shot shows the full wedge. That is the cartridge's geometry, not a bug.

---

## 1. The call chain

| what | RAM | ROM |
|---|---|---|
| `BuildingClass::Fire_At` (vtable slot `+0x1C8` of `0x80004000`) | `0x8003E550` | `0x03F150` |
| its laser branch (`Fires == 11`) | `0x8003E744`..`0x8003E80C` | `0x03F344`..`0x03F40C` |
| `BuildingClass::Fire_Coord` (vtable slot `+0x58`) | `0x800417DC` | `0x0423DC` |
| its Obelisk arm | `0x8004188C` | `0x04248C` |
| `As_Coord(TARGET)` | `0x8001B8D0` | `0x01C4D0` |
| `Laser_Add(src, dst, srcHeight, dstHeight, frames)` | `0x801CEF48` | `0x170D38` |
| per-frame laser loop (draw and count down) | `0x801CEFB4` | `0x170DA4` |
| laser pool clear | `0x801CEF20` | `0x170D10` |
| laser pool, 20 records of 20 bytes | `0x801FDD90` (BSS) | none |
| type-7 enqueue, sort bucket `0x297` | `0x8004B5BC` | `0x04C1BC` |
| the 2-D dispatcher (reads `cmd+0x08`) | `0x8004BF80` | `0x04CB80` |
| **the type-7 arm: THE LASER CONSUMER** | `0x8004D510`..`0x8004D75C` | `0x04E110`..`0x04E35C` |
| triangle emitter (3 `Vtx` + `G_VTX` + `G_TRI1`) | `0x8004B4D0` | `0x04C0D0` |
| terrain height at (x, z) | `0x801F7DFC` | `0x199BEC` |
| `Map.Render` (calls the laser loop) | `0x8002F928` | `0x030528` |
| `Main_Loop` (`while (!Main_Loop());` at ROM `0x02CDF4`) | `0x8002C998` | `0x02D598` |

Every link in this chain has exactly one caller or producer, checked by a whole-ROM scan of
`jal` words (receipt in section 12):

* `0x8004B4D0`, the triangle emitter: one caller, the laser arm (ROM `0x04E2D4`).
* `0x8004B5BC`, the type-7 enqueue: one caller, the laser loop (ROM `0x170E20`).
* `0x801CEFB4`, the laser loop: one caller, `Map.Render` (ROM `0x030A88`).
* `0x801CEF48`, `Laser_Add`: one caller, `BuildingClass::Fire_At` (ROM `0x03F3C4`).
* Of the twelve calls to the command allocator `0x801FCBC8`, only the type-7 enqueue asks for
  bucket `0x297` and stores type 7.
* The pool at `0x801FDD90` is addressed by the three pool functions and nothing else.

The dispatcher sends type 7 to the arm (DECODED):

```
ROM 04CC58 / RAM 8004C058  8F030008  lw    $v1, 8($t8)          ; type
ROM 04CC60 / RAM 8004C060  24020007  addiu $v0, $zero, 7
ROM 04CC6C / RAM 8004C06C  10620528  beq   $v1, $v0, 0x8004d510 ; TYPE 7 -> the laser arm
```

The arm checks display-list space against the budget record `0x8009706C` (ROM `0x04E16C`
`3C048009`, `0x04E170` `2484706C`) and its tail passes the same record to the profiling call
`0x8004BE60(record, dlStart)` (ROM `0x04E330`/`0x04E334`, then `j 0x8004E284`). The record's
name field at RAM `0x8009707C` holds `0x800048BC`, the string `"Laser"`. The preceding record,
`0x80097058`, is named `"RubberBand"` (`0x800048B0`) and belongs to the type-5 arm.

---

## 2. Firing: `BuildingClass::Fire_At`, RAM `0x8003E550` (questions 4, 5, 6)

**DECODED.** The 1995 `TechnoClass::Fire_At` carries the `BULLET_LASER` block
(`techno.cpp:2540`). On the cartridge that block is **gone from `TechnoClass::Fire_At`**
(`0x801D2348`..`0x801D2870`: no comparison against 11, no smudge, no line table) and a
different one sits in `BuildingClass::Fire_At` instead:

```
ROM 03F1BC / RAM 8003E5BC  0C0748D2  jal   0x801d2348          ; bullet = TechnoClass::Fire_At(target, which)
ROM 03F2B0 / RAM 8003E6B0  82030002  lb    $v1, 2($s0)         ; weapon->Fires
ROM 03F2B8 / RAM 8003E6B8  14750022  bne   $v1, $s5(=1), ...   ; BULLET_BULLET -> gun anim
ROM 03F344 / RAM 8003E744  14620033  bne   $v1, $v0(=0xB), ... ; not 11 -> muzzle-flash anim arm
ROM 03F350 / RAM 8003E750  8C420058  lw    $v0, 0x58($v0)      ; vtbl+0x58 = Fire_Coord
ROM 03F354 / RAM 8003E754  0040F809  jalr  $v0                 ; s0 = Fire_Coord(which)
ROM 03F360 / RAM 8003E760  0C006E34  jal   0x8001b8d0          ; v0 = As_Coord(target)
ROM 03F368..03F38C                                            ; this+0x68 &= ~0x04000000, ~0x02000000 (IsCharging, IsCharged)
ROM 03F394 / RAM 8003E794  A4C00000  sh    $zero, ($a2)        ; Stage = 0
ROM 03F3B4..03F3B8                                            ; Timer = 0, Rate = 0
ROM 03F3AC / RAM 8003E7AC  00003821  move  $a3, $zero          ; dstHeight = 0
ROM 03F3BC / RAM 8003E7BC  8C466A54  lw    $a2, 0x6a54($v0)    ; srcHeight = *0x80096A54
ROM 03F3C0 / RAM 8003E7C0  24020006  addiu $v0, $zero, 6
ROM 03F3C4 / RAM 8003E7C4  0C073BD2  jal   0x801cef48          ; Laser_Add(src, dst, srcH, 0, 6)
ROM 03F3C8 / RAM 8003E7C8  AFA20010  sw    $v0, 0x10($sp)      ; arg5 = 6 frames
ROM 03F3CC / RAM 8003E7CC  0C072DFC  jal   0x801cb7f0          ; new (0x18 bytes)
ROM 03F3DC / RAM 8003E7DC  24040006  addiu $a0, $zero, 6
ROM 03F3E0 / RAM 8003E7E0  0C004231  jal   0x800108c4          ; Random_Pick(6, 11)
ROM 03F3E4 / RAM 8003E7E4  2405000B  addiu $a1, $zero, 0xb
ROM 03F3F0 / RAM 8003E7F0  0C006E34  jal   0x8001b8d0          ; As_Coord(target)
ROM 03F404 / RAM 8003E804  0C072E19  jal   0x801cb864          ; SmudgeClass(type, coord, HOUSE_NONE = -1)
```

So the cartridge's `Fire_At` laser branch is the 1995 `BuildingClass::Fire_At` laser case
(`IsCharging = IsCharged = false; Set_Stage(0); Set_Rate(0)`) merged with the 1995
`TechnoClass::Fire_At` laser case (`new SmudgeClass(Random_Pick(SMUDGE_SCORCH1,
SMUDGE_SCORCH6), As_Coord(target))`), with the screen-space line table replaced by one call
into a world-space beam pool. The 1995 `SpecialDialog == SDLG_NONE` guard is not present.

**`Fires == 11` is `BULLET_LASER` on the cartridge** (the 1995 enum has it at 12). Two
independent confirmations:

* The weapon table at RAM `0x801C73B0` (10-byte records, `Fires` at `+2`) holds, at index
  19, `07 80 0B C8 5A 00 00 10 FF 00`: range `0x0780`, fires 11, damage 200, rate 90, sound
  `0x10`, anim none. That is the 1995 `WEAPON_OBELISK_LASER` row (`const.cpp:100`) to the byte.
  The shift is explained by the grenade (11 in 1995) being 10 here while napalm is 9: the
  console's list has no `BULLET_CHEMSPRAY`.
* `AircraftClass::Fire_At` (`0x801E1E10`, slot `+0x1C8` of `0x801C39D8`) tests the same
  `Fires == 0xB` at ROM `0x183C7C`/`0x183C80` and prints the debug string `"Aircraft was firing
  LASER!\n"` (RAM `0x801C38E4`, ROM `0x1656D4`). It draws nothing. (`docs/animation-drivers.md`
  lists the aircraft vtable at `0x801C39E0` with this string as the class name found near
  it; the string itself is `0xFC` bytes earlier, now noted there.)

The bullet type itself is the inline record after the identifier `"LASER"` at ROM
`0x169548`; its type, speed, warhead and explosion bytes at ROM `0x169560` are
`0B FF 04 FF`: `BULLET_LASER`, light speed, `WARHEAD_LASER`, **explosion none**. So the shot
spawns no impact animation of its own (INFERRED field order, from the neighbouring BOMBLET
and BOMB records whose type bytes 9 and 10 match napalm and grenade).

---

## 3. The source point: `BuildingClass::Fire_Coord`, RAM `0x800417DC` (question 5)

**DECODED.** `Fire_Coord` (vtable `+0x58` of `0x80004000` is `0x800417DC`) starts from
`Center_Coord()` (vtable `+0x4C`, `0x800424A4`: `Coord + CenterOffset[Class->Size]`, the
table at RAM `0x80003CA4` identical to `building.cpp:143`) and switches on
`StructType - 1` through the table at RAM `0x80003F00`: entries 0, 1 (GTWR, ATWR) go to the
default `0x8004183C`, **entry 2 (OBLI) goes to `0x8004188C`**, entries 4 and 9 (GUN, SAM) go
to `0x800418C0`. The Obelisk arm, complete:

```
ROM 04248C / RAM 8004188C  02002021  move  $a0, $s0           ; coord = Center_Coord()
ROM 042490 / RAM 80041890  240500C0  addiu $a1, $zero, 0xc0   ; DIR_W
ROM 042494 / RAM 80041894  0C07538F  jal   0x801d4e3c         ; Coord_Move
ROM 042498 / RAM 80041898  24060016  addiu $a2, $zero, 0x16   ;   22 leptons
ROM 0424A0 / RAM 800418A0  24050080  addiu $a1, $zero, 0x80   ; DIR_S
ROM 0424A4 / RAM 800418A4  0C07538F  jal   0x801d4e3c         ; Coord_Move
ROM 0424A8 / RAM 800418A8  2406001D  addiu $a2, $zero, 0x1d   ;   29 leptons
ROM 0424B4 / RAM 800418B4  240201BC  addiu $v0, $zero, 0x1bc  ; 444
ROM 0424BC / RAM 800418BC  AC626A54  sw    $v0, 0x6a54($v1)   ; *0x80096A54 = 444 (beam source height)
```

`Coord_Move` (`0x801D4E3C`) is `Move_Point` (`0x801D4F54`) on the two halves of the
coordinate, `x += (s8)Sin[(dir+0xC0)&0xFF] * dist >> 7`, `y -= (s8)Sin[dir] * dist >> 7`,
with the 1995 sine table at RAM `0x801C2E70` (`7F 7E 7E ...`). For the two cardinal moves
the table entry is `0x82` (-126) and the arithmetic shift floors, so the moves are exact:
`-126*22 >> 7 = -22` on X and `-(-126*29 >> 7) = +29` on Y.

So the source is **22 leptons west and 29 south of the footprint centre**, with no
dependence on facing or target. The Obelisk is `OBLI`, record 5 of the structure array at
RAM `0x801C41E4` (stride `0xAC`), `StructType` 3, `Size` byte (`+0xA7`) = 2 = `BSIZE_12`, so
`CenterOffset` is `0x00FF0080` and the source sits at `Coord + (106, 284)`.

The 1995 arm (`building.cpp:2951`) moved the point 0xA8 north and 0x18 west to lift it up a
2-D sprite. The console replaced the screen lift with a real height, carried out of
`Fire_Coord` in the global `0x80096A54`. That global is written only here and read only by
`Fire_At` (a 400-instruction xref window finds exactly those two sites); its initial value in
the image is 0.

**Cross-check against the model (INFERRED).** Walking the Obelisk's display list (ROM
`0x01513D8`, the project's own `tools/bakery` walker, read-only) gives 45 vertices whose apex
is model `(-107, 1777, 148)`. At 4 model units per lepton (1024 per cell, the renderer's
`MODEL_SCALE`, and the same `/4` the cartridge applies to `bbox.max.y` in its `0x800EBC88`
table, `docs/pip-row.md` section 6), that is `(-26.75, 444.25, +37)` leptons from the model
origin. The beam's height of 444 matches the apex to a quarter lepton; horizontally the
source sits about 5 leptons east and 8 north of the apex, in the same south-west quadrant.
The beam starts at the tip of the spire. (This assumes the model origin is placed at
`Center_Coord()`, the renderer's convention; the cartridge's own placement was not
re-derived here.)

---

## 4. The far end: `As_Coord(target)` (question 5)

**DECODED.** `0x8001B8D0` is `As_Coord`: zero for `TARGET_NONE`; for a cell target
(`target >> 12 == 1`) the cell centre, `x = (cell & 0x3F) << 8 | 0x80`,
`y = (cell & 0xFC0) << 2 | 0x80`; for an object, `obj->vtbl[+0x48]()`, which is
`Target_Coord`:

| class | slot `+0x48` | returns |
|---|---|---|
| unit | `0x80015498` | `0x800209EC`: `Coord` |
| infantry | `0x800209C8` (base) | `Center_Coord()` = `0x800209EC`: `Coord` |
| building | `0x80045738` | `Center_Coord()` (no occupy-list adjustment) |
| aircraft | `0x801E3438` | `Coord` shifted by the altitude, `XYP_Coord(0, -Altitude)` |

`Laser_Add` is always called with `dstHeight = 0`, so the far vertex sits **on the terrain
under that point**. For a vehicle or soldier that is the ground under its centre, inside the
base of its model. The endpoint is a copy taken at the moment of firing (section 5).

---

## 5. The pool, and timing (question 4)

**DECODED.** The 1995 per-object line table (`Lines[3][5]`, `LineCount`, `LineFrame`,
`LineMaxFrames = 5`, drawn by `TechnoClass::Draw_It` and expired by `TechnoClass::AI`) is
replaced by a global pool of 20 records at RAM `0x801FDD90`, 20 bytes each:

| offset | contents |
|---|---|
| `+0x00` | source `COORDINATE` (s16 Y at `+0`, s16 X at `+2`, big-endian) |
| `+0x04` | target `COORDINATE` (Y at `+4`, X at `+6`) |
| `+0x08` | source height above terrain (444) |
| `+0x0C` | target height above terrain (0) |
| `+0x10` | frames left (6 on firing; 0 = free) |

`Laser_Add` (`0x801CEF48`) writes the five words into the first record whose `+0x10` is 0
and returns; if all 20 are busy the beam is dropped (`slti $v0, $t3, 0x14` at ROM
`0x170D90`). The clear routine `0x801CEF20` zeroes all twenty counters; it is called from an
initialisation routine at RAM `0x8002D5D8`.

The per-frame loop `0x801CEFB4`, complete in effect:

```
ROM 170DE4 / RAM 801CEFF4  2AA20014  slti  $v0, $s5, 0x14     ; for 20 records
ROM 170DF0 / RAM 801CF000  8E020000  lw    $v0, ($s0)         ; frames left
ROM 170DF4 / RAM 801CF004  1840000C  blez  $v0, skip
ROM 170DF8 / RAM 801CF008  2442FFFF  addiu $v0, $v0, -1
ROM 170DFC / RAM 801CF00C  AE020000  sw    $v0, ($s0)         ; frames left -= 1
ROM 170E00..170E1C                                           ; load the six fields
ROM 170E20 / RAM 801CF030  0C012D6F  jal   0x8004b5bc         ; enqueue type 7
```

The enqueue receives `(srcX, srcH, srcY, dstX, dstH, dstY)` and stores them at
`cmd+0x0C..+0x20` in that order. **The counter is not passed**: nothing in the draw path can
see how old the beam is, so there is **no fade and no flicker**. Every one of its frames is
drawn identically.

**Who runs the loop, and how often.** Its only caller is `Map.Render` (`0x8002F928`, ROM
`0x030A88`), after the object-layer loops, on a path with no branch around it. `Map.Render`
is called once per pass of `Main_Loop` (`0x8002C998`, the call at ROM `0x02D63C`), followed in
the same pass by `Map.Layer[LAYER_GROUND].Sort()` (`0x8003CE44` on `0x800936BC`) and
`Logic.AI()` (`0x80045990` on `0x80096A64`), whose first act is an unconditional counter
increment, `*0x80096A60 += 1` (INFERRED to be the game's `Frame`). That is the 1995 `Main_Loop` order. So:

* the Obelisk fires inside `Logic.AI` of pass N, after pass N's render;
* the beam is drawn in the renders of passes N+1 to N+6 (counter 6, 5, 4, 3, 2, 1), then
  the slot is free;
* **6 game frames, one beam draw per game frame.** The 1995 game showed 5.

How many seconds a `Main_Loop` pass takes on the console is **NOT RECOVERED** here; the
console's frame clock is unresolved for the whole port. At the engine's 15 Hz tick the beam
lasts 0.4 s.

Because the pool holds coordinates, not object pointers, the beam stays where it was fired
for all six frames, does not track a moving target, and survives the Obelisk's own
destruction.

---

## 6. Geometry: the type-7 arm (question 2)

**DECODED.** The arm (RAM `0x8004D510`) reads the command at `0x470($sp)`:

```
f30 = x0 = cmd+0x0C   f6 = y0 = cmd+0x10 (src height)   f26 = z0 = cmd+0x14
f28 = x1 = cmd+0x18   f8 = y1 = cmd+0x1C (dst height)   f24 = z1 = cmd+0x20

ROM 04E184  C42649A0  lwc1   $f6, 0x49a0($at)    ; *0x800049A0 = 1.0 (3F800000)
ROM 04E188  46001084  sqrt.s $f2, $f2            ; L = sqrt(dx^2 + dy^2 + dz^2), dy = y1 - y0
ROM 04E18C  4602B083  div.s  $f2, $f22, $f2      ; dy / L
ROM 04E190  46023081  sub.s  $f2, $f6, $f2       ; 1 - dy/L
ROM 04E194  C4407A28  lwc1   $f0, 0x7a28($v0)    ; *0x80097A28 = 25
ROM 04E19C  46020582  mul.s  $f22, $f0, $f2      ; w = 25 * (1 - dy/L)
```

`dy` is the difference of the two **raw height fields**, terrain not included, so for the
Obelisk it is always `0 - 444` and `w = 25 * (1 + 444/L)` with `L` the distance from the
source point (444 up) to the target's ground point, terrain ignored.

Then two terrain lookups, `H0 = 0x801F7DFC(x0, z0)` (ROM `0x04E260`) and
`H1 = 0x801F7DFC(x1, z1)` (ROM `0x04E278`), both with `a2 = a3 = 0`. `0x801F7DFC` clamps to
the map, scales by `*0x801C8BC4 = 1/256` to cells, and bilinearly interpolates a 65-wide
byte heightmap whose samples it multiplies by 4: a height in leptons. Its two pointer
arguments, when non-null, receive slopes; here they are null.

The emitter is called as `0x8004B4D0(dl, A.x, A.y, A.z, B.x, B.y, B.z, C.x, C.y, C.z)`, all
truncated to integers (ROM `0x04E280`..`0x04E2D4`):

```
A = (x0, trunc(H0 + y0),     z0)      source
B = (x1, trunc(H1 + y1),     z1)      target, y1 = 0
C = (x0, trunc(H0 + y0 - w), z0)      w below the source
```

`0x8004B4D0` takes three vertices from a pool (`0x80073038(0x801225F8, "../src/n64engine.cpp",
4540)`), writes only their `ob[]` fields (`sh` at `+0/+2/+4`, `+0x10/+0x12/+0x14`,
`+0x20/+0x22/+0x24`), and emits two words:

| word0 / word1 | meaning |
|---|---|
| `01003006 <vtx>` | F3DEX2 `G_VTX`, 3 vertices into slot 0 |
| `05000204 00000000` | F3DEX2 `G_TRI1`, triangle 0-1-2 |

If the vertex pool is empty it sets the overflow flag `*0x80097568 = 1` and emits neither.
the requirement's "four points, perhaps a quad" is three points and one triangle.

So the beam is **one triangle in the vertical plane through source and target**: its upper
edge is the straight line from the spire tip to the target's ground point, its lower edge
runs from `w` below the tip to the same point. Width `w` at the source, **tapering linearly
to zero at the target**. It is not billboarded, not crossed, not a line primitive.

| horizontal distance `d` | `L` | `w` (leptons) | `w` (cells) |
|---|---|---|---|
| 0 | 444.0 | 50.0 | 0.195 |
| 256 (1 cell) | 512.5 | 46.7 | 0.182 |
| 512 | 677.7 | 41.4 | 0.162 |
| 1024 | 1116.1 | 35.0 | 0.137 |
| 1920 (weapon range `0x780`) | 1970.7 | 30.6 | 0.120 |

---

## 7. Colour (question 1)

**DECODED.** The bytes the arm reads, RAM `0x80097A1C`..`0x80097A2B` (ROM `0x09861C`):

```
0x80097A18  00000014   (20, read elsewhere, by 0x8004B37C)
0x80097A1C  000000FF   R
0x80097A20  0000000A   G
0x80097A24  0000000A   B
0x80097A28  00000019   width, 25
```

The arm takes the low byte of each int:

```
ROM 04E218  90427A1F  lbu $v0, 0x7a1f($v0)   ; R = 0xFF
ROM 04E210  90647A23  lbu $a0, 0x7a23($v1)   ; G = 0x0A
ROM 04E21C  90637A27  lbu $v1, 0x7a27($v1)   ; B = 0x0A
ROM 04E234  344200FF  ori $v0, $v0, 0xff     ; A = 0xFF
ROM 04E200  3C0DFA00  lui $t5, 0xfa00        ; G_SETPRIMCOLOR, minLOD 0, LOD frac 0
```

So `G_SETPRIMCOLOR FA000000 FF0A0AFF`. **Constant data**: no house index, no counter, no
random term reaches it. A scan of every load and store whose low half is `0x7A10`..`0x7A3F`
with a matching `lui 0x8009` within 400 instructions finds only the four loads above (and
the width load); no word in the ROM points at the block. Three `int`s that are only ever
read through their low byte look like tuning variables (an `n64engine.cpp` static block,
INFERRED), but nothing in the cartridge writes them.

If the console's framebuffer is 16-bit (not checked here), `10` quantises to `8` on screen.

---

## 8. Render state (question 3)

**DECODED.** Ten commands, written from the arm's start pointer. Three words come from the
dispatcher's stack, filled in its prologue (ROM `0x04CBA4`..`0x04CC28`):
`0x47C($sp) = E200001C`, `0x488($sp) = FCFFFFFF`, `0x48C($sp) = FFFCF279`.

| # | word0 / word1 | written at ROM | meaning |
|---|---|---|---|
| 0 | `FA000000 FF0A0AFF` | `04E20C`, `04E238` | `G_SETPRIMCOLOR` (255, 10, 10, 255) |
| 1 | `FCFFFFFF FFFDF6FB` | `04E248`, `04E244` | `G_SETCOMBINE` = `G_CC_PRIMITIVE` in both cycles: colour = PRIM, alpha = PRIM |
| 2 | `E200001C 00552038` | `04E258`, `04E254` | `G_SETOTHERMODE_L` render mode = `G_RM_RA_ZB_OPA_SURF` / `..._SURF2` |
| 3 | `D9DDF9FB 00000000` | `04E25C`, `04E264` | `G_GEOMETRYMODE` clear `0x220604` = `G_SHADING_SMOOTH \| G_LIGHTING \| G_CULL_FRONT \| G_CULL_BACK \| G_SHADE`, set nothing |
| 4 | `01003006 <vtx>` | emitter | `G_VTX` 3 vertices |
| 5 | `05000204 00000000` | emitter | `G_TRI1` 0-1-2 |
| 6 | `D9FFFFFF 00200404` | `04E314`, `04E318` | restore: set `G_SHADING_SMOOTH \| G_CULL_BACK \| G_SHADE` (lighting is not set back) |
| 7 | `E200001C 00552230` | `04E328`, `04E324` | restore: `G_RM_ZB_OPA_SURF` |
| 8 | `FCFFFFFF FFFCF279` | `04E338`, `04E344` | restore: `G_CC_DECALRGBA` both cycles |
| 9 | `FA000000 FFFFFFFF` | `04E350`, `04E354` | restore: primitive colour white |

Decoding the two load-bearing words:

* **Combiner `FFFDF6FB`:** colour `(0 - 0) * 0 + PRIMITIVE`, alpha `(0 - 0) * 0 + PRIMITIVE`,
  identical in cycle 2. No texel, no shade, no environment term. **Untextured, unlit, flat.**
* **Render mode `00552038`:** `AA_EN | Z_CMP | Z_UPD | ALPHA_CVG_SEL | CVG_DST_CLAMP |
  ZMODE_OPA`, blender `CLR_IN * A_IN + CLR_MEM * A_MEM` in both cycles, no `FORCE_BL`, no
  `IM_RD`. That is exactly libultra's `G_RM_RA_ZB_OPA_SURF` (reduced-aliasing, z-buffered,
  opaque). **Opaque, depth-compared, depth-written**; the blender only acts on partially
  covered edge pixels. Not translucent, not additive.

What the arm does **not** touch, so is inherited: othermode H (cycle type, texture filter,
TLUT), `G_TEXTURE`, and the `G_ZBUFFER` and `G_FOG` geometry bits. None of them can change the
colour under this combiner and blender. `G_ZBUFFER` has to be on for `Z_CMP`/`Z_UPD` to mean
anything; the arm leaves it as found (INFERRED on, since every 3-D pass of the game is
z-buffered).

`G_CULL_FRONT | G_CULL_BACK` cleared means **both faces draw**, which the vertical-plane
triangle needs since the camera can see either side.

---

## 9. Anything else drawn with the beam (question 6)

**DECODED unless noted.**

* **Scorch smudge at the target.** `new SmudgeClass(Random_Pick(6, 11), As_Coord(target),
  HOUSE_NONE)` on every shot. Types 6..11 are `SMUDGE_SCORCH1..6` in the 1995 enum; the
  cartridge's smudge identifier pool at ROM `0x168FCC` runs `CR1..CR6, SC1..SC6, BIB1..BIB3`
  in that order (INFERRED: same enum).
* **No muzzle flash, no glow at the source.** The `AnimClass` creations in
  `BuildingClass::Fire_At` are in the `Fires == 1` arm (gun flash) and the `else` arm
  (`0x1B`, muzzle flash); the `Fires == 11` arm creates none.
* **No impact explosion.** The `LASER` bullet type's explosion byte is `FF` (section 2).
  Whatever the warhead's damage does to the target (a burning infantry death, for example) is
  the target's own effect, not the beam's.
* **No charge-up glow.** The firing branch zeroes the stage and rate that the Obelisk's
  charging block in `BuildingClass::AI` advances, but the Obelisk's draw arm writes frame 0
  for the static 20-triangle mesh (`docs/animation-drivers.md` section 3, `OBLI` arm
  `0x8003E1F8`), so that counter has nothing to show.
* **No second draw.** The pool is read by the draw loop alone (section 1), so there is no
  light, screen flash or second primitive keyed off a live beam.
* A sound: `0x8002B3E0(weapon->Sound = 0x10, Coord, 0xA00)` at ROM `0x03F2A8`. Not visual.

---

## 10. Compared with the 1995 code

| | 1995 (`techno.cpp`) | cartridge |
|---|---|---|
| where | `TechnoClass::Fire_At` | `BuildingClass::Fire_At` only |
| storage | per-object `Lines[3][5]` in screen pixels | global 20-slot pool, world leptons |
| primitive | three 1-pixel 2-D lines (outer `0x7D` at x+1 and x-1, centre `0x7F`) | one world-space triangle |
| colour | palette entries `0x7D`, `0x7F` | constant `#FF0A0A` |
| source | `Fire_Coord`: centre + 0xA8 N, 0x18 W (a screen lift) | centre + 22 W, 29 S, at height 444 |
| target | `As_Coord(target)` | `As_Coord(target)`, on the terrain |
| duration | `LineMaxFrames = 5` draws | 6 game frames |
| expiry | `TechnoClass::AI` resets the line table | counter in the pool, decremented at draw |
| dialog guard | `SpecialDialog == SDLG_NONE` | none |
| scorch | `Random_Pick(SCORCH1, SCORCH6)` | `Random_Pick(6, 11)`, same |

---

## 11. A recorded anomaly: the arm's view mode never reaches the RSP

**DECODED, consequence INFERRED.** Before writing its commands the arm calls
`0x801FCE80(dl, camera, 6)`. That function keeps the current view mode at `*0x802149D8`;
when the mode changes it copies the 0x60-byte camera block, adds a per-mode pair from the
table at RAM `0x80214970` (ROM `0x1B6760`) to the block's first two floats, and calls
`0x8007C120`, which emits `G_MTX` (projection, `DA380007`; modelview, `DA380003`) and the
perspective-normalise and clip-ratio `G_MOVEWORD`s, built by `guPerspectiveF`
(`0x8007D4A0`, `docs/n64-camera.json`). The first two floats are **near** and **far**, so the
modes are depth-range variants of the one camera:

| mode | near, far offset | used by |
|---|---|---|
| 2 | -10, -1000 | dispatcher prologue, models with index >= 0x61 |
| 5 | 0, 0 | arms at ROM `0x04EAFC`, `0x04EC48` |
| **6** | **-5, -1000** | **laser**, rubber band (type 5) |
| 7 | +200, +1800 | models with index < 0x61 |
| 9 | +250, +4000 | pips, health bar and three other call sites |

The type-5 and model arms store the returned pointer and write after it. **The laser arm
does not**: it loaded the display-list pointer into `$s0`/`$s1` at ROM `0x04E17C`, stores
`0x801FCE80`'s result back to `0x198($s7)` at ROM `0x04E1FC`, and then writes its ten commands
starting at the old `$s1` (ROM `0x04E20C` onward), finally storing its own end pointer at ROM
`0x04E35C`. Whenever the mode actually changes, the mode-6 matrices are written and then
overwritten by the laser's commands, while the cached mode says 6. The pip arm has the same
shape (`$fp` loaded at ROM `0x04E364`, mode-9 call at `0x04E474`).

Models push and pop their own modelview (`G_MTX` `DA380000` at ROM `0x04D28C`, `G_POPMTX`
`D8380002` at `0x04D450`), so the camera's modelview is intact and **the beam lands in the
right place on screen**. What differs is only the depth range of the projection left loaded
by the previous command in the sorted list. A re-implementation should simply depth-test the
triangle against the scene under the scene's own projection.

---

## 12. Not recovered, and dead ends

1. **Seconds per frame.** The beam is six `Main_Loop` passes; the console's real rate for a
   pass is not recovered (the console's frame clock is an open question for the whole port).
2. **The exact depth bias the beam ends up with**, per section 11: it depends on which
   command precedes bucket `0x297` in a given frame. Not settleable statically.
3. **The `G_ZBUFFER` geometry bit and the cycle type** at the moment of the draw are
   inherited; neither changes colour or blending under this combiner and blender.
4. **The vertex fields other than position** are stale pool memory. They cannot matter: the
   combiner reads only `PRIMITIVE` and shading is cleared.
5. **The building model's placement relative to `Center_Coord()`** was taken from the
   renderer's convention for the apex cross-check in section 3, not re-derived from the ROM.

Dead ends, recorded so they are not repeated:

* ROM `0x01513D8` is the Obelisk's display list, not its vertex array; reading it as `Vtx`
  gives nonsense. The vertices come out of the project's display-list walker.
* A straight xref with a short window finds the colour loads but cannot prove the absence of
  stores; the window was widened to 400 instructions and a whole-ROM search for pointer words
  into `0x800979F0`..`0x80097A44` was run. Both found nothing. A store through a pointer built
  some other way cannot be excluded by static means.
* `0x801C39E0` is the aircraft vtable, not the laser string, which sits `0xFC` bytes before it
  (section 2).

---

## 13. Receipt

The load-bearing words re-derive from the cartridge with no disassembler. Run at the repo
root; it printed **24/24 pass** on `data/rom/cnc_eu.z64` when this file was written. The rest
of this decode was produced with a capstone MIPS64BE wrapper and an address-xref scanner kept
outside the repository; nothing under `tools/` was modified.

```python
import struct
ROM = open("data/rom/cnc_eu.z64", "rb").read()
u32 = lambda o: struct.unpack_from(">I", ROM, o)[0]
ok = []
def chk(name, got, want):
    ok.append(got == want)
    print(("PASS " if got == want else "FAIL ") + name)
jal = lambda t: 0x0C000000 | ((t & 0x0FFFFFFF) >> 2)
callers = lambda t: [i for i in range(0, len(ROM), 4) if u32(i) == jal(t)]

chk("dispatcher: type 7 -> 0x8004D510",           (u32(0x04CC60), u32(0x04CC6C)), (0x24020007, 0x10620528))
chk("budget record 0x8009706C is named 'Laser'",  (u32(0x097C7C), ROM[0x0054BC:0x0054C2]), (0x800048BC, b"Laser\0"))
chk("laser arm passes the Laser record",          (u32(0x04E16C), u32(0x04E170)), (0x3C048009, 0x2484706C))
chk("colour ints 255,10,10 and width 25",         [u32(0x09861C + 4*i) for i in range(4)], [255, 10, 10, 25])
chk("colour byte loads R,G,B and alpha 0xFF",     [u32(o) for o in (0x04E218, 0x04E210, 0x04E21C, 0x04E234)],
                                                  [0x90427A1F, 0x90647A23, 0x90637A27, 0x344200FF])
chk("width load and the 1.0 it is offset from",   (u32(0x04E194), u32(0x0055A0)), (0xC4407A28, 0x3F800000))
chk("combiner word1 0xFFFDF6FB (G_CC_PRIMITIVE)", (u32(0x04E1CC), u32(0x04E1D0)), (0x3C0CFFFD, 0x358CF6FB))
chk("rendermode 0x00552038 (G_RM_RA_ZB_OPA_SURF)",(u32(0x04E1DC), u32(0x04E1E0)), (0x3C0B0055, 0x356B2038))
chk("geometry clear 0x220604",                    (u32(0x04E1EC), u32(0x04E1F0)), (0x3C0AD9DD, 0x354AF9FB))
chk("emitter: G_VTX 3 verts, G_TRI1 0-1-2",       [u32(o) for o in (0x04C130, 0x04C13C, 0x04C160, 0x04C16C)],
                                                  [0x3C060100, 0x34C63006, 0x3C050500, 0x34A50204])
chk("triangle emitter has one caller, the arm",   callers(0x8004B4D0), [0x04E2D4])
chk("type-7 enqueue: bucket 0x297, type 7",       (u32(0x04C1E8), u32(0x04C1F4)), (0x24040297, 0x24020007))
chk("type-7 enqueue has one caller, the pool",    callers(0x8004B5BC), [0x170E20])
chk("pool draw loop has one caller, Map render",  callers(0x801CEFB4), [0x030A88])
chk("pool holds 20 beams",                        u32(0x170DE4), 0x2AA20014)
chk("pool counter decremented per draw",          (u32(0x170DF4), u32(0x170DF8)), (0x1840000C, 0x2442FFFF))
chk("Laser_Add has one caller, Building Fire_At", callers(0x801CEF48), [0x03F3C4])
chk("Fire_At laser test is Fires == 11",          u32(0x03F2BC), 0x2402000B)
chk("Obelisk weapon record fires type 11",        ROM[0x16925E:0x169263].hex(), "07800bc85a")
chk("Laser_Add args: src height, 0, 6 frames",    [u32(o) for o in (0x03F3BC, 0x03F3AC, 0x03F3C0)],
                                                  [0x8C466A54, 0x00003821, 0x24020006])
chk("Fire_Coord jump table: OBLI -> 0x8004188C",  u32(0x004B08), 0x8004188C)
chk("Obelisk Fire_Coord: W 22, S 29, height 444", [u32(o) for o in (0x042490, 0x042498, 0x0424A0, 0x0424A8, 0x0424B4, 0x0424BC)],
                                                  [0x240500C0, 0x24060016, 0x24050080, 0x2406001D, 0x240201BC, 0xAC626A54])
chk("scorch smudge Random_Pick(6, 11)",           (u32(0x03F3DC), u32(0x03F3E4)), (0x24040006, 0x2405000B))
chk("LASER bullet: type 11, speed 255, warhead 4, no explosion", ROM[0x169560:0x169564].hex(), "0bff04ff")
print("%d/%d pass" % (sum(ok), len(ok)))
```
