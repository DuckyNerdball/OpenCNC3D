/*
 * lmodule.h -- the modules an install can hold beside OpenCNC3D, and what makes one
 * installed.
 *
 * ONE ANSWER FOR EVERY QUESTION ABOUT IT. The module grid draws Red Alert's slot as
 * installed, --module ra3d --play starts it, and the movies and music fetch its own
 * movies and score (lmedia.c), all on the same test. A test kept in two places would let
 * the grid show Red Alert as missing while the download fetched 724 MB for it, or the
 * other way round.
 */
#ifndef LMODULE_H
#define LMODULE_H

/* The Red Alert 3D module's folder, at the top of the install. */
#define LMOD_RA_DIR "ra3d"

/* The files a Red Alert install needs, for this platform. */
extern const char *const lmod_ra_needs[];
extern const int lmod_ra_nneeds;

/* 1 when every file of lmod_ra_needs is in `dir`. Otherwise 0, with the first one
 * missing named in `missing` ("ra3d/RedAlert.dylib is missing") when it is not NULL. */
int lmod_ra_present(const char *dir, char *missing, int missinglen);

/* Whether the module folder `folder` ("ra3d") is installed in `dir`. A folder this
 * launcher knows no module for is never installed. */
int lmod_folder_installed(const char *dir, const char *folder);

/* 1 when `folder` names a module this launcher knows. */
int lmod_known_folder(const char *folder);

/* Exact module-owned runtime removal. Shared assets and optional media are kept.
 * Requires the updater's held install lock. settle recovers interrupted removals. */
/* A verified install package waiting in the launcher's download folder
 * (.opencnc3d-modules). It is there only until its game is in: every install that
 * succeeds (from it, from a download, or from the copy a Mac installer package puts
 * there) removes it, so a later Repair or reinstall downloads again. A stopped or
 * failed install keeps it for the next try. */
int lmod_cached(const char *dir, int module);
int lmod_install_cached(const char *dir, int module, const char *build,
                         int (*progress)(void *, int, int), void *user,
                         char *err, int errlen);

int lmod_td_present(const char *dir);
int lmod_remove(const char *dir, int module, char *err, int errlen);
int lmod_settle(const char *dir, char *err, int errlen);

/* THE WHOLE INSTALLATION, for the uninstaller (lsetup_cli.h, --uninstall). Every file
 * either game's ownership list names, shared ones included, or each game's legacy
 * names where a folder has no list, in the same journaled transaction as
 * lmod_remove. `each` (may be NULL) is told every name first, so the caller can take
 * the emptied folders away afterwards. Requires the held install lock. */
int lmod_remove_all(const char *dir, void (*each)(void *user, const char *rel), void *user,
                    char *err, int errlen);
/* The launcher's module download cache (.opencnc3d-modules): each game's package, its
 * checksum and version notes, and any download pieces, then the folder if empty. */
int lmod_cache_clear(const char *dir, char *err, int errlen);
/* At a launcher's start, under its held install lock: each game's package that can never
 * be installed again, because that game is in at the package's version or a newer one,
 * is removed with its notes and any download piece of that game. That is how a folder
 * an older launcher left a package in (about 4 GB a game) gets the space back. A
 * package for a game that is missing or older stays: it is a stopped install's or an
 * update's, and the next try uses it. Returns how many packages went. */
int lmod_cache_tidy(const char *dir);
/* Remove the folders above `rel` inside `dir`, deepest first, while they are empty. */
void lmod_prune_parents(const char *dir, const char *rel);

/* Version-bound public catalog. An available entry always binds the whole ZIP's
 * mandatory SHA-256 and byte length to one asset of this exact release. */
typedef struct { int available; long long id, size; char name[256], sha[65]; } LMOD_Asset;
typedef struct { char version[100], site[1024]; LMOD_Asset module[2]; } LMOD_Catalog;
int lmod_catalog_read(const char *build, const char *builds_json, const char *catalog,
                       LMOD_Catalog *out, char *err, int errlen);
/* NULL build discovers the newest approved catalog for this platform.
 * A named build retains exact pinned-release validation. */
int lmod_catalog_fetch(const char *build, LMOD_Catalog *out, char *err, int errlen);
int lmod_catalog_fetch_at(const char *site, const char *build, LMOD_Catalog *out, char *err, int errlen);
int lmod_install_file(const char *dir, int module, const char *build, const char *zip,
                       const char *sha, int (*progress)(void *, int, int),
                       void *user, char *err, int errlen);
/* Installed game identity, separate from the bootstrap launcher record. */
int lmod_module_version(const char *dir, int module, char *version, int versionlen);
int lmod_module_current(const char *dir, int module, const char *build);
int lmod_module_update_available(const char *dir, int module, const char *build);
int lmod_install_remote(const char *dir, int module, const char *build,
                         const LMOD_Catalog *catalog, int (*progress)(void *, int, int),
                         void *user, char *err, int errlen);

#endif /* LMODULE_H */
