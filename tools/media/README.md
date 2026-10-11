# tools/media: where the movies and music come from

The packages carry neither the 1995 movies nor the score. The launcher downloads them
when the player ticks the boxes, and removes them when the player unticks them. The game
itself never reads a setting: a movie or a track that is on disk plays, and one that is
not is skipped, exactly as it always has been.

Nothing is redistributed. Each player's launcher reads the files by HTTP byte range
straight out of disc images that public mirrors already host, on archive.org: the movies
from the two 1995 C&C Gold discs, which Electronic Arts released as freeware in 2007 (with
a files.cncnz.com copy of each as one more mirror), and the whole score from the Covert
Operations disc image, because the Gold discs carry only part of it (below). The Covert
Operations image has a single mirror, and its standing as freeware is less clear than
the Gold discs'.

| File | What it is |
|---|---|
| `media-sources.txt` | the candidate mirrors, hand-edited |
| `make-media-manifest.py` | turns the candidates into the manifest; needs the network |
| `media-manifest.txt` | the manifest, generated and committed |
| `media-windows.txt` | SHA-256 of the first and last 64 KB of every file, for the probe |
| `../../launcher/lmedia_manifest.c` | the manifest compiled into the launcher, generated |
| `probe-sources.py` | checks every file on every mirror; run before a release |
| `media-paths.txt` | what the packagers leave out of both packages |
| `medialib.py` | the shared code: ranged reads, zip and ISO9660 walks, MIX index |

Run both tools with `python3 -I` when they read downloaded data, so nothing in the
current directory can be imported in place of the standard library.

## The manifest format

UTF-8 text, one record per line, `#` starts a comment, the first record is the version.

```
OCMEDIA 1
mirror <DISC> <url> <base>
file <component> <relpath> <size> <sha256> <DISC> <offset>
legacy <component> <relpath>
```

- **DISC** is `GDI`, `NOD` or `COVERT`.
- **mirror**: `base` is the byte at which the disc image starts inside the URL's bytes:
  0 for a raw `.ISO`, the stored entry's data offset for a zip. Mirrors are listed in
  order of preference, and every mirror of a disc serves the same image byte for byte.
- **file**: component `movies` or `music`; `relpath` is where it goes in the install
  folder; `offset` is where the file starts inside the disc image. The file is the byte
  range `[base+offset, base+offset+size)` of any mirror of its disc, and must hash to
  `sha256`.
- **legacy**: a path that belongs to a component without being downloaded. Older
  packages put these in the install folder; unticking the component removes them too.

## What the player gets

**Movies**: the 75 files of `data/dosdata/movies`, each one contiguous entry of a disc's
`MOVIES.MIX`, written as loose `dosdata/movies/<NAME>.VQA` because the game's movie
player opens loose files only. The GDI disc is searched first, then the Nod disc, then
the Covert Operations disc; all 75 are on the first two (49 GDI, 26 Nod).

**Music**: whole archives, written where the game looks for them:

- `dosdata/SCORES.MIX`, the Covert Operations disc's. The Gold discs' own `SCORES.MIX`
  holds 23 entries and lacks ten of the development score's: `BEFEARED`, `DIE`, `I_AM`
  and `VALKYRIE`, the `.VAR` recordings that are the only copies of `ROUT` and `HEART`,
  and four more `.VAR` alternates. The Covert Operations disc's holds all 33 entries of
  the development score, byte for byte, plus the eight Covert Operations tracks.
- `dosdata/transit/TRANSIT.MIX`, from the Covert Operations disc's `INSTALL` folder. It
  carries `MAP1.AUD`, the menu theme, and `WIN1.AUD`. Neither Gold disc has a
  `TRANSIT.MIX` or either track anywhere.

The generator does not hard-code that choice: for each archive it takes the first disc
whose copy holds every track the development folder plays, and turns any track no chosen
archive holds into a loose file of its own. It then plays every theme in the game's
table through the game's lookup order and checks each one is the shape the music reader
takes (mono, 22050 Hz, SOS ADPCM): `themes=37/37`.

The 15 loose `dosdata/music/*.AUD` files of the development folder are `legacy`. Seven
are copies of entries in the two archives, and the other eight are the Covert Operations
tracks, which the Covert Operations `SCORES.MIX` already holds. Packages up to v0.7.3
shipped them, so an older install has them, and unticking the music removes them with
the rest. The top-level `movies/` copy of `LOGO.VQA` and `INTRO2.VQA` is `legacy` of the
movies for the same reason.

## Mirrors

- GDI and NOD: archive.org items `gdi-95` and `cn-c-gdi-95` (identical MD5 and SHA-1),
  and the files.cncnz.com zips, whose images are stored and carry the same CRC-32.
- COVERT: the 2048-byte-sector image in the archive.org item of the Worldwide Warfare
  compilation. It is the only copy found that serves byte ranges: the files.cncnz.com zip
  is deflated, and the archive.org copies of the retail disc are raw 2352-byte-sector
  images, in which a file is not one contiguous range. So the music has ONE mirror today.

## Regenerating

```
python3 -I tools/media/make-media-manifest.py --cache <a scratch folder>
python3 -I tools/media/probe-sources.py
```

The generator reads about 170 MB the first time: the two archives whole (83 MB), the
eight movies that were in the development set before it was filled from these images,
whole (55 MB), and 64 KB windows of every file on every mirror (30 MB). The cache makes
a rerun cost almost nothing. It refuses to write a manifest with a gap in it unless told
`--allow-gaps`.

`probe-sources.py` takes about two minutes. It exits 0 when every file has at least one
mirror serving the recorded bytes, 1 when some file has none, 2 when the manifest, its
windows file and the compiled copy disagree, and 3 when nothing is reachable.
`tools/release.sh` runs it before it builds or tags anything, dry runs included, and
refuses to go on for any status but 0.

## What the packages leave out

`media-paths.txt` lists one path per line, relative to the package root and written
anchored there with a leading `/`, the form rsync's `--exclude-from` reads; a trailing `/`
means a whole folder and `#` starts a comment. Every reader also accepts a line without the
leading `/`, so the two forms cannot drift apart: `tools/mac/make-package-mac.sh` and
`tools/win/make-build-win.sh` leave the paths out of the copy and refuse a package holding
any of them, `tools/release.sh` refuses a staged folder or zip holding one, and
`tools/launcher/data-id.sh` leaves them out of the data fingerprint. The paths are
literal; the readers do not agree on what a wildcard matches, so the list holds none.

- `dosdata/movies/`, `dosdata/music/`, `dosdata/SCORES.MIX`, `dosdata/TRANSIT.MIX` and
  `dosdata/transit/TRANSIT.MIX`: every path the game reads movies or music from.
- `movies/`: the legacy top-level copy of the two menu movies.
- `ra3d/movies/`, `ra3d/audio/SCORES.MIX` and `ra3d/data/SCORES.MIX`: the Red Alert
  module's movies and score. Red Alert's media may not ship either, so both packagers
  refuse a package that stages the module, and every release leaves the module out
  (`OPENCNC3D_NO_RA=1`), until Red Alert has downloads of its own. Its `RA_LOCAL.MIX`
  carries two themes but also the campaign text, so it is not on the list.
