# Map archive dependency

The importer uses libarchive 3.8.10 with statically paired XZ 5.8.2 for ZIP,
RAR/RAR5 and 7Z. Sources are official upstream release assets, verified against
fixed SHA256 values in `build.py` before extraction. See
[libarchive release](https://github.com/libarchive/libarchive/releases/tag/v3.8.10)
and [XZ release](https://github.com/tukaani-project/xz/releases/tag/v5.8.2).
No shell extractor, Python installation or Homebrew library is needed at runtime.

`python3 tools/map-archive/build.py mac` produces a universal arm64/x86_64
`build-native/map-archive/mac/libmaparchive.dylib` (macOS 14 floor).
`python3 tools/map-archive/build.py windows` cross-builds i686 `archive.dll` using
the project's existing WinSDK zlib. XZ is static; the reader has no GCC, pthread,
Homebrew, or liblzma DLL dependency. Windows system/UCRT DLLs remain native platform
dependencies. Compilation is separate from native Windows execution verification.

`stage.py DEPENDENCY RUNTIME` checks the built hash and copies the reader,
source manifest and exact license notices. The frontend loads only its paired
reader beside the executable. Mac development probes without that reader can use
the OS libarchive. The shipped runtime carries the pinned reader in both games.
The app builder, runtime stager and Windows/RA package inputs preserve that pairing.

Only validated map INI/BIN/MPR/MAP/HGT/JSON inputs are written into a private
temporary workspace. Paths, links, duplicates, entry counts and compressed/expanded
sizes are bounded; extraction never writes into the game's installation. Bundled
menus, executable files, shared rules and renderer packs cannot replace game assets.
Every candidate is inspected before any map import begins. A later I/O failure
reports how many maps were committed; each map retains the existing INI-last
transaction. Closing the menu waits for an active write, preserving its lifetime.
Encrypted and split archives require a complete unlocked archive first.

A bundle imports all its scenarios and previews the first. Classification remains
per map; it is never inferred from the ZIP/RAR filename. Reimports retain stable
IDs. A missing TD INI can be recovered from the limited Remastered JSON terrain
schema only as explicitly labelled terrain-only multiplayer. Mission objects,
resources, scripts and campaign linkage absent from that schema are not invented.
# v0.9.4 public module compatibility

The Windows and universal macOS game frontends now link the pinned libarchive and
xz static libraries. `MAPARCHIVE|builtin|3.8.10` identifies this compiled path;
typed wrappers use the same bounded extraction/controller code as the dynamic
test path. Existing launchers reject new root library/receipt names, so public
modules retain their established filenames and carry reader provenance and
licences in their existing `NOTICE.md`. Public packages omit the unused paired
reader. Private build outputs retain the paired library for standalone tools and
older binaries; it is not needed by the built-in frontend. `manifest.json` binds
both static input libraries as well as the dynamic test library.

Verify actual ZIP/7Z/recovery fixtures and an original RAR5 through the static
backend, package/controller fixtures, and the final packaged native frontends.
Keep source hashes, release package hashes and native execution results separate.
