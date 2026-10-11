# data/radata: the 1996 Red Alert data

The original Red Alert data for the RA3D module, from the Allied and Soviet discs of
the 1996 release, which Electronic Arts made freeware in 2008.

## Where it comes from

The two disc images of that release, as the Internet Archive item `cnc-red-alert` holds
them:

```
redalert_allied.iso   653725696 bytes  md5 13e213c788890e5172c766ed0059ea22
redalert_soviets.iso  677222400 bytes  md5 b6eddcf8daefcd633ea804f9813e25b7
```

Mount or copy each disc, then:

```
tools/ra3d/extract_ra_disc.py <allied disc folder> <soviet disc folder>
```

## What goes here

```
data/radata/
  CONQUER.MIX   GENERAL.MIX   ALLIES.MIX   RUSSIAN.MIX    from MAIN.MIX on either disc
  TEMPERAT.MIX  SNOW.MIX      INTERIOR.MIX
  SOUNDS.MIX    SCORES.MIX
  LOCAL.MIX     HIRES.MIX     LORES.MIX    NCHIRES.MIX     from INSTALL/REDALERT.MIX
  SPEECH.MIX
  _XXXXXXXX.MIX  three inner archives the source names no file for, kept by name hash
  movies/        the cinematics: MOVIES1.MIX (Allied) and MOVIES2.MIX (Soviet) split into
                 their .VQA and .VQP files, plus two unnamed entries kept by hash
  MANIFEST.txt   every file with its size, md5 and the archive it came from
```

The archives are written exactly as they sit on the disc, still encrypted, because that
is the form Red Alert's own engine reads. `tools/ra3d/ramix.py` lists and extracts them,
taking the public key and the Blowfish tables from the Red Alert source in
`brain/vanilla`. Every archive found on both discs is identical on both; the extractor
stops if that ever stops being true.

The two discs' own movie archives are too large to commit whole (369 MB for the Allied
one), which is why only they are split.
