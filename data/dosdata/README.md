# data/dosdata: the 1995 MS-DOS Command & Conquer data

This directory holds the original Command & Conquer data used by the game. The
shared audio and video archives remain supplied from an existing installation;
`TEMPERAT.MIX` and `DESERT.MIX` are included for the complete DOS Classic scenery.

The N64 cartridge gives this project its 3D. This directory gives it everything 2D and
everything audible: the sidebar, cursors, infantry sprites, fonts, the palette, the music,
the speech and the cinematics.

## What goes here

```
data/dosdata/
  CONQUER.MIX     sidebar art, cursors, infantry sprites, cameos, buildup animations
  LOCAL.MIX       fonts and the authentic palette
  GENERAL.MIX     shared game data
  TEMPERAT.MIX    original temperate trees, tiberium and scenery for DOS Classic
  DESERT.MIX      original desert scenery for DOS Classic
  SOUNDS.MIX      sound effects
  SPEECH.MIX      the EVA lines
  SCORES.MIX      the music score
  AUD.MIX         further audio
  ZOUNDS.MIX      further audio
  movies/         the campaign cinematics, .VQA, 75 files
  music/          loose score tracks, .AUD, 15 files
  transit/        TRANSIT.MIX: the menu theme MAP1 and the win theme WIN1
```

The `.MIX` files sit at the top of your CD or install directory and can be copied straight
across. `movies/` is the one that needs assembling.

## Movies and music are not in the packages

The game packages carry everything above EXCEPT the movies and the music: `movies/`,
`music/`, `SCORES.MIX` and `transit/TRANSIT.MIX` stay out of both the macOS and the
Windows package (`tools/media/media-paths.txt` is the list the packagers read). The
launcher downloads them when the player asks for them, by byte range straight out of disc
images on archive.org, and removes them again when the player unticks them: the movies
from the 1995 Gold discs (EA's 2007 freeware release), the music from the Covert
Operations disc image, because the Gold discs carry only part of the score (Getting the
music, below). `tools/media/README.md` describes where every file comes from.

This directory keeps them all, because the development play folder links to it and the
gates that play the score and the cinematics run there. The game needs no setting for
either: a file that is present plays, and a missing one is skipped, as it always was.

A player's download does not reproduce this directory's layout, but it plays the same
bytes. The player's `SCORES.MIX` is the Covert Operations disc's, which holds every entry
of the `SCORES.MIX` here plus the eight tracks that sit loose in `music/` here. The other
seven loose tracks in `music/` are copies of entries in the two archives.
`tools/media/make-media-manifest.py` checks each of those claims byte for byte.

## Getting the movies

The game asks for 75 `.VQA` files by name and they are split across the two discs:
**the GDI and Nod `MOVIES.MIX` files are different and you need both.**

`tools/extract_movies.py` reconstructs the set from either disc's `MOVIES.MIX`.

The discs are the freeware **C&C95 Gold** release, which Electronic Arts published at no
charge: archive.org item `gdi-95`, files `GDI95.ISO` and `NOD95.ISO`. You do not have to
download either whole. `MOVIES.MIX` is stored contiguously, so reading the ISO9660 volume
descriptor at byte 32768 plus the root directory (about 4 KB) gives its exact extent, and
an HTTP range request fetches only that.

## Getting the music

`SCORES.MIX` here holds 33 entries. The Gold discs' own
`SCORES.MIX` is smaller and lacks ten of them, and neither Gold disc has a
`TRANSIT.MIX`. The Covert Operations disc has both: its `SCORES.MIX` is these 33 entries
plus the eight Covert Operations tracks, and its `INSTALL/TRANSIT.MIX` is the file in
`transit/` here. Here the eight Covert Operations tracks are loose in `music/`; without
them those eight themes are simply absent and nothing else changes.

## Where the project reads it from

`data/dosdata/` relative to the repository root, or set `OPENCNC3D_ROOT` if you are
running a tool from outside the tree. A handful of sounds and two EVA lines are not on
the 1995 discs at all; they stay silent and are listed rather than substituted.

## Why it is not in the repository

The remaining archive art and audio are Electronic Arts' and Westwood's work,
and the movie set alone is over 400 MB. Use your own copy for those files.

## DOS Classic scenery

Classic uses the original 320×200 top-down viewport and SHP unit/building frames from
`CONQUER.MIX`. The included `TEMPERAT.MIX` and `DESERT.MIX` were extracted from the freeware
C&C Gold GDI disc image at `https://archive.org/download/gdi-95/GDI95.ISO` using
ISO9660 byte extents. Their SHA-256 hashes are:

- `TEMPERAT.MIX`: `25b41e31ea7fddc34fa18d9561ca9807b9d0124ff7340e8f3a107461d041ce81`
- `DESERT.MIX`: `0141d371bdb847e085984bca44a0f96cb2f2f410d5eba1410b8e498087954072`

`game/build.sh` bakes the original theater-specific `.TEM` and `.DES` shape frames
into `dosclassic.pack` and merges missing shape entries into the playable theater
archives. The shared units and buildings still come from `CONQUER.MIX` as `.SHP`
frames. The theater archives are required for Classic to show original trees,
rocks, and Tiberium overlays.
