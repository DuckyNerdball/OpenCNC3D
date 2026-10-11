# trextract

Dumps assets out of an **uncooked Unreal Engine 5.4 project** (the editor's own `.uasset`
files, not a shipped game's paks) into plain files that `tools/art/ue_c3m.py` turns into a
`.c3m`. Built on [CUE4Parse](https://github.com/FabianFG/CUE4Parse) (Apache 2.0), which
reads the package format; the few layouts it does not handle for editor assets are read
here instead (see the comments in `Program.cs`).

## Build

A CUE4Parse checkout beside this repository (or anywhere, named by `CUE4PARSE`), and the
.NET 10 SDK:

    git clone --depth 1 --recurse-submodules https://github.com/FabianFG/CUE4Parse.git ../CUE4Parse
    CUE4PARSE_SKIP_NATIVE=1 dotnet build -c Release tools/art/trextract

`CUE4PARSE_SKIP_NATIVE=1` skips CUE4Parse's native helper library, which nothing here uses
and which does not build with every compiler.

## Use

`<projectDir>` is the folder holding the `.uproject`. Object paths are as the package
names them, project name first:

    trextract <projectDir> mesh  Tempest/Content/.../SK_GDI_Rifleman.SK_GDI_Rifleman out/SK_GDI_Rifleman
    trextract <projectDir> skel  Tempest/Content/.../SKEL_MASTER_Infantry.SKEL_MASTER_Infantry out/skel.json
    trextract <projectDir> anim  Tempest/Content/.../Anim_Rifleman_Idle1.Anim_Rifleman_Idle1 out/Anim_Rifleman_Idle1.json
    trextract <projectDir> props Tempest/Content/.../BS_Rifleman_Movement

- **mesh** writes the editor LOD model: the reference skeleton, material slots, sections,
  and per vertex position, normal, first UV and up to eight bone weights.
- **anim** writes every key of every bone track. It reads both layouts an editor
  AnimSequence can have: the legacy raw tracks, and UE5's `AnimDataModel` subobject.
  An additive clip is written as its full source pose, which is what the asset stores.
- **props** prints every export of a package as JSON (blend spaces, montages, materials).

## Textures: ue_texsrc.py

An editor texture's source art is an `FCompressedBuffer` whose blocks are Oodle-compressed,
which CUE4Parse does not decompress without Oodle. `ue_texsrc.py` reads the buffer straight
out of the `.uasset` and decompresses it through an Oodle shared library you supply (its
header says where an Unreal source install keeps Oodle's source and how it was built;
neither is in this repository):

    python3 tools/art/trextract/ue_texsrc.py liboodle.dylib T_Body_D.uasset T_Body_D.png
