/*
 * lmodule.c -- see lmodule.h.
 */

#include "lmodule.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* WHAT MAKES RED ALERT INSTALLED is what a package carries for it, and nothing a
 * package leaves out. Its movies (ra3d/movies/) and its score (ra3d/audio/SCORES.MIX)
 * are on the media list no package may carry, and the game plays without them, by
 * file presence alone: a missing movie is skipped and a missing score is silence. The
 * test once asked for REDINTRO.VQA, PROLOG.VQA and SCORES.MIX as well, so a folder
 * without the movies and the music showed Red Alert's slot as "Coming soon" and
 * --module ra3d --play refused it. tools/launcher/module-check.sh holds a folder
 * shaped like a package to this list. */
const char *const lmod_ra_needs[] = {
#ifdef _WIN32
    "ra3d/opencnc3d-game.exe",
    "ra3d/RedAlert.dll",
#else
    "ra3d/opencnc3d",
    "ra3d/RedAlert.dylib",
#endif
    "ra3d/dosmenu.pack",
    "ra3d/SCG01EA.pack",
    "ra3d/audio/SOUNDS.MIX",
    "ra3d/audio/SPEECH.MIX",
    "ra3d/audio/RA_LOCAL.MIX",
    "ra3d/missions/SCG01EA.INI",
    "ra3d/missions/TUTORIAL.INI",
    "ra3d/missions/RULES.INI"
};

const int lmod_ra_nneeds = (int)(sizeof lmod_ra_needs / sizeof *lmod_ra_needs);

static int lmod_has_file(const char *dir, const char *name)
{
    char path[1200];
    FILE *file;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    file = fopen(path, "rb");
    if (!file)
        return 0;
    fclose(file);
    return 1;
}

int lmod_ra_present(const char *dir, char *missing, int missinglen)
{
    int i;
    for (i = 0; i < lmod_ra_nneeds; i++)
        if (!lmod_has_file(dir, lmod_ra_needs[i])) {
            if (missing && missinglen > 0)
                snprintf(missing, (size_t)missinglen, "%s is missing", lmod_ra_needs[i]);
            return 0;
        }
    return 1;
}

int lmod_known_folder(const char *folder)
{
    return folder && !strcmp(folder, LMOD_RA_DIR);
}

int lmod_folder_installed(const char *dir, const char *folder)
{
    if (!dir || !folder)
        return 0;
    if (!strcmp(folder, LMOD_RA_DIR))
        return lmod_ra_present(dir, NULL, 0);
    return 0;
}

#include "lzip.h"
#include <errno.h>
#include <sys/stat.h>
#include "lmodule_files.h"
#ifdef _WIN32
#include "lsdl_tidy.h"   /* used on Windows only */
#endif

static const char *const *lmod_owned_files(const char *dir, int target);
static const char *const *lmod_all_files(const char *dir);
static int lmod_install_public(const char *dir, int target, const char *build, const char *zip,
                                int (*cb)(void *, int, int), void *user, char *err, int len);

static int lmod_cache_path(const char *dir, int target, const char *suffix, char *out)
{
    char rel[128];
    if (target < 0 || target > 1) return 0;
    snprintf(rel, sizeof rel, ".opencnc3d-modules/%s.zip%s", target ? "ra3d" : "td", suffix);
    return lz_inside(dir, rel, out, 1400) && !lz_is_symlink(out);
}
/* Below, beside the rest of the download folder's handling. */
static int lmod_cache_drop(const char *dir, int target, const char *keep_part, int with_package);
static void lmod_package_done(const char *dir, int target);
static int lmod_is_cache_package(const char *dir, int target, const char *zip);

int lmod_cached(const char *dir, int target)
{
    char path[1400]; struct stat st;
    return lmod_cache_path(dir, target, "", path) && !stat(path, &st) && S_ISREG(st.st_mode) &&
           lmod_cache_path(dir, target, ".sha256", path) && !stat(path, &st) && S_ISREG(st.st_mode) &&
           lmod_cache_path(dir, target, ".version", path) && !stat(path, &st) && S_ISREG(st.st_mode);
}
int lmod_install_cached(const char *dir, int target, const char *build, int (*cb)(void *, int, int), void *user,
                         char *err, int len)
{
    char zip[1400], sidecar[1400], expected[80], actual[65], version[100]; FILE *f;
#ifdef _WIN32
    static const char *const td_required[] = {"TiberianDawn.dll", "CONQUER.MIX"};
#else
    static const char *const td_required[] = {"TiberianDawn.dylib", "CONQUER.MIX"};
#endif
    if (lz_is_private_developer(dir)) {
        snprintf(err, (size_t)len, "This private source build is updated from its source checkout, not public game packages.");
        return 0;
    }
    snprintf(err, (size_t)len, "The local game package could not be read.");
    if (!lz_held_here(dir) || !lmod_cached(dir, target)) {
        snprintf(err, (size_t)len, "A verified local install package is not available for this game."); return 0;
    }
    if(!lmod_settle(dir,err,len))return 0;
    lmod_cache_path(dir, target, ".version", sidecar); f = fopen(sidecar, "rb");
    if (!f) return 0;
    version[0] = 0; if (fscanf(f, "%99s", version) != 1) version[0] = 0; fclose(f);
    if ((!build || strcmp(version, build))) {
        snprintf(err, (size_t)len, "This game's install package needs refreshing for the current launcher build."); return 0;
    }
    lmod_cache_path(dir, target, ".sha256", sidecar); f = fopen(sidecar, "rb");
    if (!f) return 0;
    expected[0] = 0; if (fscanf(f, "%79s", expected) != 1) expected[0] = 0; fclose(f);
    lmod_cache_path(dir, target, "", zip);
    if (!lz_sha256_file(zip, actual, err, len)) return 0;
    if (strlen(expected) != 64 || strcmp(expected, actual)) {
        snprintf(err, (size_t)len, "The local game package failed its checksum. No game files were changed."); return 0;
    }
    {
        char manifest[80], probe[256]; char *text;
        snprintf(manifest,sizeof manifest,"opencnc3d-module-%s.txt",target?"ra":"td");
        text=lz_read_text(zip,manifest,4*1024*1024,probe,sizeof probe);
        if(text){
            int ok;
            free(text);
            ok=lmod_install_public(dir,target,build,zip,cb,user,err,len);
            if(ok)lmod_package_done(dir,target);
            return ok;
        }
    }
    {
        const int ok = lz_install_owned(dir, zip, target ? lmod_ra_files : lmod_td_files,
                                        target ? lmod_ra_needs : td_required,
                                        target ? lmod_ra_nneeds : 2, cb, user, err, len);
#ifdef _WIN32
        /* (a module of 64-bit programs over an install that held 32-bit ones) */
        if (ok) lsdl_tidy(dir);
#endif
        if (ok) lmod_package_done(dir, target);
        return ok;
    }
}

int lmod_td_present(const char *dir)
{
#ifdef _WIN32
    return lmod_has_file(dir, "TiberianDawn.dll") && lmod_has_file(dir, "opencnc3d-game.exe");
#else
    return lmod_has_file(dir, "TiberianDawn.dylib") && lmod_has_file(dir, "opencnc3d");
#endif
}

static int lmod_paths(const char *dir, const char *rel, char *path, char *staged)
{
    char temp[1200];
    if (snprintf(temp, sizeof temp, "%s.module-removing", rel) >= (int)sizeof temp)
        return 0;
    return lz_inside(dir, rel, path, 1400) && !lz_is_symlink(path) &&
           lz_inside(dir, temp, staged, 1400) && !lz_is_symlink(staged);
}

/* A recoverable transaction: stage exact owned files beside themselves, commit,
 * then unlink. An interrupted staging rolls back; an interrupted cleanup resumes.
 * Never enumerate a player's directory or follow a link, even on Windows.
 *
 * The journal names what is being removed: 0 Dawn3D, 1 RA3D (each keeping what the
 * other game shares), 2 the whole installation (lmod_remove_all). A launcher that
 * predates 2 reads it as damaged and keeps every file, which is the safe answer. */
int lmod_settle(const char *dir, char *err, int len)
{
    char journal[1400], path[1400], staged[1400];
    const char *const *files;
    FILE *f;
    struct stat st;
    int target, committed, i, ok = 1;
    if (!lz_held_here(dir) || !lz_inside(dir, "opencnc3d-module-removal.txt", journal, sizeof journal) || lz_is_symlink(journal))
        goto failure;
    f = fopen(journal, "rb");
    if (!f) {if (errno == ENOENT) return 1;goto failure;}
    if (fscanf(f, "%d %d", &target, &committed) != 2 || target < 0 || target > 2 || committed < 0 || committed > 1) {
        fclose(f); goto failure;
    }
    fclose(f);
    files = target == 2 ? lmod_all_files(dir) : lmod_owned_files(dir, target);
    if (!files) goto failure;
    for (i = 0; files[i]; i++) {
        if (!lmod_paths(dir, files[i], path, staged)) {ok = 0;continue;}
        if (stat(staged, &st)) {if (errno != ENOENT) ok = 0;continue;}
        if (!S_ISREG(st.st_mode)) {ok = 0;continue;}
        if (committed) {
            /* The manifest is the only durable list of Enhanced files. Keep it
             * for retry if any earlier cleanup failed (e.g. a Windows file lock). */
            if (!ok && !strncmp(files[i], "opencnc3d-module-", 17)) continue;
            if (remove(staged)) ok = 0;
        }
        else if (stat(path, &st) == 0 || errno != ENOENT || rename(staged, path)) ok = 0;
    }
    if (ok && remove(journal) == 0) return 1;
failure:
    snprintf(err, (size_t)len, "Module removal needs recovery; files were kept. Close other launchers and retry.");
    return 0;
}

/* Stage, commit and remove one exact list under the journal code `code`. The caller
 * holds the lock and has settled any earlier transaction. */
static int lmod_remove_files(const char *dir, int code, const char *const *files, char *err, int len)
{
    char path[1400], staged[1400], journal[1400];
    FILE *f;
    struct stat st;
    int i, bad = 0;
    /* Preflight the entire transaction before moving a single file. */
    for (i = 0; files[i]; i++) {
        if (!lmod_paths(dir, files[i], path, staged)) {bad = 1;break;}
        if (stat(path, &st) == 0) {if (!S_ISREG(st.st_mode)) {bad = 1;break;}}
        else if (errno != ENOENT) {bad = 1;break;}
        if (stat(staged, &st) == 0 || errno != ENOENT) {bad = 1;break;}
    }
    if (bad) {
        snprintf(err, (size_t)len, "Removal refused: an owned file is linked, blocked or has a recovery copy.");
        return 0;
    }
    if (!lz_inside(dir, "opencnc3d-module-removal.txt", journal, sizeof journal) || lz_is_symlink(journal)) return 0;
    f = fopen(journal, "wb");
    if (!f) goto failure;
    bad = fprintf(f, "%d 0\n", code) < 0;
    if (fclose(f)) bad = 1;
    if (bad) goto failure;
    for (i = 0; files[i]; i++) {
        if (!lmod_paths(dir, files[i], path, staged)) goto rollback;
        if (stat(path, &st)) {if (errno == ENOENT) continue;goto rollback;}
        if (rename(path, staged)) goto rollback;
    }
    /* The single commit byte changes only after every move succeeded. A torn
     * staging journal is never interpreted as permission to delete. */
    f = fopen(journal, "r+b");
    if (!f) goto rollback;
    if (fseek(f, 2, SEEK_SET) || fputc('1', f) == EOF) {fclose(f);goto rollback;}
    if (fclose(f)) goto failure;
    return lmod_settle(dir, err, len);
rollback:
    lmod_settle(dir, err, len);
failure:
    snprintf(err, (size_t)len, "Module removal could not finish. Close the game and retry; saves and settings are kept.");
    return 0;
}

int lmod_remove(const char *dir, int target, char *err, int len)
{
    const char *const *files;
    if (target < 0 || target > 1 || !lz_held_here(dir)) {
        snprintf(err, (size_t)len, "Cannot remove a module without the installation lock.");
        return 0;
    }
    if (!lmod_settle(dir, err, len)) return 0;
    files = lmod_owned_files(dir, target);
    if (!files) {
        snprintf(err, (size_t)len, "Module removal could not finish. Close the game and retry; saves and settings are kept.");
        return 0;
    }
    return lmod_remove_files(dir, target, files, err, len);
}

int lmod_remove_all(const char *dir, void (*each)(void *, const char *), void *user, char *err, int len)
{
    const char *const *files;
    int i;
    if (!lz_held_here(dir)) {
        snprintf(err, (size_t)len, "Cannot remove the games without the installation lock.");
        return 0;
    }
    if (!lmod_settle(dir, err, len)) return 0;
    files = lmod_all_files(dir);
    if (!files) {
        snprintf(err, (size_t)len, "A game file list in this folder is damaged, so no game file was removed.");
        return 0;
    }
    /* Named before anything moves: the list's storage is rebuilt by the settle that
     * ends the transaction. */
    if (each)
        for (i = 0; files[i]; i++) each(user, files[i]);
    return lmod_remove_files(dir, 2, files, err, len);
}

#include "lmodule_remote.h"

#include <dirent.h>

/* ONE GAME'S SHARE OF THE LAUNCHER'S DOWNLOAD FOLDER: its package and the two notes
 * beside it, the notes' half-written copies (with_package), and the pieces of any
 * download of that game still arriving, except the one named `keep_part`. That one
 * folder is the launcher's alone (no package may carry a name under it), so it is the
 * one place here that is listed rather than named, and only names shaped like the
 * launcher's own pieces of this game are taken from it. The names are read first and
 * removed after, because removing while a folder is being listed may skip entries.
 * With the package, the folder itself goes once empty; anything else in it stays, and
 * the other game's share is never touched. 1 when nothing of that share is left, or
 * there was no folder. */
static int lmod_cache_drop(const char *dir, int target, const char *keep_part, int with_package)
{
    static const char *const sides[] = {"", ".sha256", ".version", ".sha256.new", ".version.new", NULL};
    const char *prefix = target ? "ra-" : "td-";
    char path[1400], cache[1400], rel[400];
    char **names = NULL;
    struct stat st;
    int i, n = 0, cap = 0, ok = 1;
    DIR *d;
    struct dirent *e;
    if (target < 0 || target > 1 || !lz_inside(dir, ".opencnc3d-modules", cache, sizeof cache) ||
        lz_is_symlink(cache))
        return 0;
    if (stat(cache, &st))
        return errno == ENOENT;
    if (!S_ISDIR(st.st_mode))
        return 0;
    for (i = 0; with_package && sides[i]; i++) {
        if (!lmod_cache_path(dir, target, sides[i], path)) {ok = 0;continue;} /* a link */
        if (stat(path, &st)) {if (errno != ENOENT) ok = 0;continue;}
        if (!S_ISREG(st.st_mode) || remove(path)) ok = 0;
    }
    d = opendir(cache);
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len <= 7 || len >= 300 || strcmp(e->d_name + len - 7, ".ocpart") ||
            strncmp(e->d_name, prefix, 3) || !lz_name_safe(e->d_name) ||
            (keep_part && !strcmp(e->d_name, keep_part)))
            continue;
        if (n == cap) {
            int grow = cap ? cap * 2 : 16;
            char **more = (char **)realloc(names, (size_t)grow * sizeof *more);
            if (!more) {ok = 0;break;}
            names = more;cap = grow;
        }
        names[n] = strdup(e->d_name);
        if (!names[n]) {ok = 0;break;}
        n++;
    }
    closedir(d);
    for (i = 0; i < n; i++) {
        snprintf(rel, sizeof rel, ".opencnc3d-modules/%s", names[i]);
        if (!lz_inside(dir, rel, path, sizeof path) || lz_is_symlink(path) || stat(path, &st) ||
            !S_ISREG(st.st_mode) || remove(path))
            ok = 0;
        free(names[i]);
    }
    free(names);
    /* Only once empty, and never ahead of a download about to write into it. */
    if (with_package)
        LMOD_RMDIR(cache);
    return ok;
}

/* AN INSTALL THAT SUCCEEDED NEEDS ITS PACKAGE NO MORE. A game's package is about 4 GB,
 * and keeping it for an offline Repair or reinstall cost every player that space for a
 * convenience few use; a Repair or reinstall now downloads the package again. A package
 * that could not be removed (a file another program holds open on Windows, say) is not
 * a failed install: the game is in, and the next launcher start removes it
 * (lmod_cache_tidy). */
static void lmod_package_done(const char *dir, int target)
{
    if (!lmod_cache_drop(dir, target, NULL, 1))
        fprintf(stderr, "LAUNCHER|module|package kept|%s|it could not be removed now; the next start retries\n",
                target ? "ra3d" : "td");
}

/* Whether `zip` is the package lmod_cached would find for `target`: the copy a Mac
 * installer package puts in the download folder before it asks for the install. A
 * file anywhere else belongs to whoever named it and is never removed. */
static int lmod_is_cache_package(const char *dir, int target, const char *zip)
{
    char path[1400];
    struct stat a, b;
    if (!zip || !lmod_cache_path(dir, target, "", path) || stat(path, &a) || stat(zip, &b) ||
        !S_ISREG(a.st_mode) || !S_ISREG(b.st_mode))
        return 0;
#ifdef _WIN32
    /* No inode numbers here (stat gives 0 for every file), so the two names are compared,
     * either separator and either case being the same. */
    {
        const char *x = path, *y = zip;
        for (; *x && *y; x++, y++) {
            int cx = (unsigned char)*x, cy = (unsigned char)*y;
            if (cx == '\\') cx = '/';
            if (cy == '\\') cy = '/';
            if (cx >= 'A' && cx <= 'Z') cx += 'a' - 'A';
            if (cy >= 'A' && cy <= 'Z') cy += 'a' - 'A';
            if (cx != cy) return 0;
        }
        return !*x && !*y;
    }
#else
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
#endif
}

/* The launcher's own download cache, for the uninstaller: both games' shares of it
 * (lmod_cache_drop), then the folder if that left it empty. */
int lmod_cache_clear(const char *dir, char *err, int len)
{
    char cache[1400];
    int target, ok = 1;
    if (!lz_inside(dir, ".opencnc3d-modules", cache, sizeof cache) || lz_is_symlink(cache)) {
        snprintf(err, (size_t)len, "The launcher's download folder is a link, so it was left alone.");
        return 0;
    }
    for (target = 0; target < 2; target++)
        if (!lmod_cache_drop(dir, target, NULL, 1))
            ok = 0;
    if (!ok)
        snprintf(err, (size_t)len, "A file in the launcher's download folder could not be removed. Close OpenCNC3D and retry.");
    return ok;
}

int lmod_cache_tidy(const char *dir)
{
    char path[1400], version[100], installed[100];
    FILE *f;
    int target, gone = 0;
    if (!lz_held_here(dir) || lz_is_private_developer(dir))
        return 0;
    for (target = 0; target < 2; target++) {
        if (!(target ? lmod_ra_present(dir, NULL, 0) : lmod_td_present(dir)) ||
            !lmod_installed_version(dir, target, installed) ||
            !lmod_cache_path(dir, target, ".version", path))
            continue;
        f = fopen(path, "rb");
        if (!f)
            continue;
        version[0] = 0;
        if (fscanf(f, "%99s", version) != 1)
            version[0] = 0;
        fclose(f);
        /* A note that names no build cannot say the package is still wanted. */
        if (version[0] && lmod_record_order(installed, version) < 0)
            continue;
        if (lmod_cache_drop(dir, target, NULL, 1)) {
            fprintf(stderr, "LAUNCHER|module|package removed|%s|installed %s, package %s\n",
                    target ? "ra3d" : "td", lmod_version(installed), version[0] ? lmod_version(version) : "unnamed");
            gone++;
        } else
            fprintf(stderr, "LAUNCHER|module|package kept|%s|it could not be removed now\n", target ? "ra3d" : "td");
    }
    return gone;
}

/* Once a file is gone, the folders it sat in go too, deepest first, while they are
 * empty: removing a folder that still holds anything fails, which is where the walk
 * stops, and it never goes above the installation or through a link. */
void lmod_prune_parents(const char *dir, const char *rel)
{
    char sub[1024], path[1400];
    char *slash;
    struct stat st;
    if (!rel || strlen(rel) >= sizeof sub || !lz_name_safe(rel))
        return;
    snprintf(sub, sizeof sub, "%s", rel);
    while ((slash = strrchr(sub, '/')) != NULL) {
        *slash = '\0';
        if (!sub[0] || !lz_inside(dir, sub, path, sizeof path) || lz_is_symlink(path))
            return;
        if (stat(path, &st)) {
            if (errno == ENOENT) continue;
            return;
        }
        if (!S_ISDIR(st.st_mode) || LMOD_RMDIR(path))
            return;
    }
}
