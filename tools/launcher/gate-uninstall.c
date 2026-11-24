/* The uninstaller's launcher half, --uninstall, on disposable installations only.
 *
 *   gate-uninstall <scratch folder> [<launcher binary>]
 *
 * Every case builds a fresh folder shaped like a Windows install that has had both
 * games, movies, music, a language pack, the launcher's download cache, an update's
 * leftover download, settings, logs and a player's own files put in it, runs the
 * uninstall, and then WALKS THE WHOLE FOLDER: what is left must be exactly the
 * expected set, and no empty folder may remain. A spot check of a few names is what
 * let the old uninstaller leave gigabytes behind while looking finished, so nothing
 * here asks about single files only.
 *
 * With a launcher binary, the settings-removing case runs that program, the way the
 * Windows uninstaller does, instead of the in-process entry point. */
#define main launcher_entry
#include "../../launcher/launcher.c"
#undef main
#include <assert.h>
#include <dirent.h>

#ifdef _WIN32
#define GU_PLATFORM "windows"
#else
#define GU_PLATFORM "macos"
#endif
#define GU_MAX 256
static int gu_fail;
#define CHECK(c, ...) do { if (!(c)) { printf("UNINSTALL-GATE|FAIL|line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); gu_fail = 1; } } while (0)

static void gu_dirs(const char *path)
{
    char buf[1400];
    char *p;
    snprintf(buf, sizeof buf, "%s", path);
    for (p = buf + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(buf, 0755); *p = '/'; }
}
static void gu_put(const char *dir, const char *rel)
{
    char path[1400];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", dir, rel);
    gu_dirs(path);
    f = fopen(path, "wb");
    assert(f);
    fprintf(f, "bytes of %s\n", rel);
    assert(!fclose(f));
}
static void gu_text(const char *dir, const char *rel, const char *text)
{
    char path[1400];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", dir, rel);
    gu_dirs(path);
    f = fopen(path, "wb");
    assert(f);
    fputs(text, f);
    assert(!fclose(f));
}

/* A module ownership list as the packager writes it. */
static void gu_manifest(const char *dir, int ra, const char *const *files)
{
    char text[8192];
    size_t used;
    int i;
    used = (size_t)snprintf(text, sizeof text, "module-files 1\nmodule %s\nversion 0.9.6\nplatform %s\n",
                            ra ? "ra" : "td", GU_PLATFORM);
    for (i = 0; files[i]; i++) {
        used += (size_t)snprintf(text + used, sizeof text - used,
                                 "file 0000000000000000000000000000000000000000000000000000000000000000 9 %s\n",
                                 files[i]);
        gu_put(dir, files[i]);
    }
    gu_text(dir, ra ? "opencnc3d-module-ra.txt" : "opencnc3d-module-td.txt", text);
}

/* The whole folder, as relative names, files only; and a count of empty folders. */
typedef struct { char name[GU_MAX][300]; int n, empty_dirs; char empty[300]; } GU_Walk;
static int gu_walk_into(const char *root, const char *sub, GU_Walk *w)
{
    char path[1400], rel[600];
    DIR *d;
    struct dirent *e;
    struct stat st;
    int entries = 0;
    snprintf(path, sizeof path, "%s%s%s", root, sub[0] ? "/" : "", sub);
    d = opendir(path);
    if (!d) return 0;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        entries++;
        snprintf(rel, sizeof rel, "%s%s%s", sub, sub[0] ? "/" : "", e->d_name);
        snprintf(path, sizeof path, "%s/%s", root, rel);
        if (lstat(path, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!gu_walk_into(root, rel, w)) {
                w->empty_dirs++;
                snprintf(w->empty, sizeof w->empty, "%s", rel);
            }
        } else if (w->n < GU_MAX) {
            snprintf(w->name[w->n++], 300, "%s", rel);
        }
    }
    closedir(d);
    return entries;
}
static int gu_cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static void gu_walk(const char *root, GU_Walk *w)
{
    memset(w, 0, sizeof *w);
    gu_walk_into(root, "", w);
    qsort(w->name, (size_t)w->n, sizeof w->name[0], gu_cmp);
}
/* The walk equals `want` exactly. Prints the difference when it does not. */
static int gu_exactly(const char *what, const GU_Walk *w, const char *const *want)
{
    int i, j, nwant = 0, ok = 1;
    for (i = 0; want[i]; i++) nwant++;
    for (i = 0; i < w->n; i++) {
        for (j = 0; want[j] && strcmp(want[j], w->name[i]); j++) {}
        if (!want[j]) { printf("UNINSTALL-GATE|%s|left behind: %s\n", what, w->name[i]); ok = 0; }
    }
    for (j = 0; want[j]; j++) {
        for (i = 0; i < w->n && strcmp(want[j], w->name[i]); i++) {}
        if (i == w->n) { printf("UNINSTALL-GATE|%s|removed but should stay: %s\n", what, want[j]); ok = 0; }
    }
    if (w->empty_dirs) { printf("UNINSTALL-GATE|%s|%d empty folder(s) left, e.g. %s\n", what, w->empty_dirs, w->empty); ok = 0; }
    return ok && w->n == nwant;
}
/* Every file's bytes, for "nothing changed" checks. */
static unsigned long gu_digest(const char *root, const GU_Walk *w)
{
    unsigned long h = 5381;
    int i, c;
    char path[1400];
    for (i = 0; i < w->n; i++) {
        FILE *f;
        const char *p;
        for (p = w->name[i]; *p; p++) h = h * 33 + (unsigned char)*p;
        snprintf(path, sizeof path, "%s/%s", root, w->name[i]);
        f = fopen(path, "rb");
        if (!f) continue;
        while ((c = fgetc(f)) != EOF) h = h * 33 + (unsigned long)c;
        fclose(f);
    }
    return h;
}

static const char *const gu_td[] = {
    "TiberianDawn.dylib", "opencnc3d", "CONQUER.MIX", "SCG01EA.pack", "dosmenu.pack", "enhanced-menu.pack",
    "LICENSE", "NOTICE.md", "content/a.pack", "missions/SCG01EA.INI", "dosdata/x/GENERAL.MIX",
    "ebld/x/y.ebld", "vfx3d/z.bin", "newaudio/n.wav", "models3d/deep/er/m.bin", "ecur/ecur.bin",
    "libSDL2-2.0.0.dylib", NULL};
static const char *const gu_ra[] = {
    "ra3d/RedAlert.dylib", "ra3d/opencnc3d", "ra3d/data/GENERAL.MIX", "ra3d/missions/SCG01EA.INI",
    "ra3d/dosmenu.pack", "ebld/x/y.ebld", "clearblend_RA_SNOW.pack", "gunra-cameos.pack", NULL};

static char gu_movie_td[600], gu_movie_ra[600], gu_music[600], gu_legacy[600], gu_lang[600];

/* Real names from the launcher's own manifests, so the removal is asked about the
 * files a player's folder would hold. */
static void gu_media_names(const char *scratch)
{
    LM_State *m = lm_create(scratch, NULL);
    char rel[600];
    int i;
    assert(m);
    for (i = 0; lm_name(m, i, rel, sizeof rel); i++) {
        if (!gu_movie_td[0] && strstr(rel, "dosdata/movies/")) snprintf(gu_movie_td, sizeof gu_movie_td, "%s", rel);
        if (!gu_movie_ra[0] && strstr(rel, "ra3d/movies/")) snprintf(gu_movie_ra, sizeof gu_movie_ra, "%s", rel);
        if (!gu_music[0] && strstr(rel, "SCORES.MIX") && strncmp(rel, "ra3d/", 5)) snprintf(gu_music, sizeof gu_music, "%s", rel);
        if (!gu_legacy[0] && strstr(rel, "dosdata/music/")) snprintf(gu_legacy, sizeof gu_legacy, "%s", rel);
    }
    lm_destroy(m);
    m = lm_create_language(scratch, 0, "de", NULL);
    assert(m && lm_name(m, 0, gu_lang, sizeof gu_lang));
    lm_destroy(m);
    assert(gu_movie_td[0] && gu_movie_ra[0] && gu_music[0] && gu_lang[0]);
}

/* What the player put there, and the launcher files the Windows uninstaller itself
 * deletes once the launcher has exited. These always stay. */
static const char *const gu_player[] = {
    "custom-map.ini", "notes-from-player.txt", "missions/user_maps/MY-MAP.INI", "ra3d/missions/custom-map.ini",
    "shots/first.png", NULL};
static const char *const gu_launcher_own[] = {
    "OpenCNC3D.exe", "opencnc3d.ico", "READ-ME-WINDOWS.txt", "uninstall.exe", "opencnc3d-launcher-log.txt",
    "libSDL2-2.0.0.dylib", NULL};
static const char *const gu_settings[] = {
    "opencnc3d-fx.cfg", "opencnc3d-controls.cfg", "opencnc3d-log.txt", "opencnc3d-crash-20261116-101010.txt",
    "ra3d/ra3d-fx.cfg", NULL};

static void gu_fixture(const char *dir)
{
    char rel[700];
    int i;
    mkdir(dir, 0755);
    gu_manifest(dir, 0, gu_td);
    gu_manifest(dir, 1, gu_ra);
    gu_text(dir, "opencnc3d-install.txt", "version 0.9.6\nplatform macos\nmodule_install 1\n");
    gu_put(dir, "opencnc3d-media.txt");
    gu_put(dir, "opencnc3d-media-td.txt");
    gu_put(dir, "opencnc3d-media-ra.txt");
    gu_put(dir, "opencnc3d-language-td-de.txt");
    gu_put(dir, "CHANGELOG.txt");
    gu_put(dir, "BUILD-ID.txt");
    /* the movies, the music, a legacy theme, a download still arriving, a language pack */
    gu_put(dir, gu_movie_td);
    gu_put(dir, gu_movie_ra);
    gu_put(dir, gu_music);
    if (gu_legacy[0]) gu_put(dir, gu_legacy);
    snprintf(rel, sizeof rel, "%s.ocpart", gu_movie_td);
    gu_put(dir, rel);
    gu_put(dir, gu_lang);
    /* the module download cache and an update's leftover download */
    gu_put(dir, ".opencnc3d-modules/td.zip");
    gu_put(dir, ".opencnc3d-modules/td.zip.sha256");
    gu_put(dir, ".opencnc3d-modules/td.zip.version");
    gu_put(dir, ".opencnc3d-modules/ra3d.zip");
    gu_put(dir, ".opencnc3d-modules/ra3d.zip.sha256.new");
    gu_put(dir, ".opencnc3d-modules/td-5-0000000000000000000000000000000000000000000000000000000000000000.ocpart");
    gu_put(dir, "opencnc3d-update.zip.ocpart");
    gu_put(dir, "opencnc3d-update.id.ocpart");
    for (i = 0; gu_player[i]; i++) gu_put(dir, gu_player[i]);
    for (i = 0; gu_launcher_own[i]; i++) gu_put(dir, gu_launcher_own[i]);
    for (i = 0; gu_settings[i]; i++) gu_put(dir, gu_settings[i]);
}

static int gu_run(const char *dir, int keep, const char *binary)
{
    if (binary) {
        char cmd[3000];
        int rc;
        snprintf(cmd, sizeof cmd, "'%s' --uninstall%s --dir '%s'", binary, keep ? " --keep-settings" : "", dir);
        rc = system(cmd);
        return WIFEXITED(rc) ? WEXITSTATUS(rc) : 99;
    } else {
        char *args[6];
        int n = 0, handled = 0, rc;
        args[n++] = (char *)"opencnc3d-launcher";
        args[n++] = (char *)"--uninstall";
        if (keep) args[n++] = (char *)"--keep-settings";
        args[n++] = (char *)"--dir";
        args[n++] = (char *)dir;
        args[n] = NULL;
        rc = lsetup_cli_main(n, args, &handled);
        CHECK(handled == 1, "--uninstall was not taken as a headless mode");
        return rc;
    }
}

int main(int argc, char **argv)
{
    char dir[1200], path[1400], other[1400], err[256];
    const char *want[64];
    GU_Walk w;
    unsigned long before;
    int n, i, rc, cases = 0;
    long long size_before;
    LZ_Lock lock;

    if (argc < 2) { fprintf(stderr, "usage: gate-uninstall <scratch> [<launcher>]\n"); return 2; }
    mkdir(argv[1], 0755);
    gu_media_names(argv[1]);

    /* 1. --keep-settings, and an uninstall that a crash interrupted before: a
     *    whole-install removal staged one file and died before its commit byte. The
     *    next run puts it back and then takes everything out. */
    snprintf(dir, sizeof dir, "%s/keep", argv[1]);
    gu_fixture(dir);
    gu_walk(dir, &w);
    size_before = w.n;
    snprintf(path, sizeof path, "%s/CONQUER.MIX", dir);
    snprintf(other, sizeof other, "%s.module-removing", path);
    assert(!rename(path, other));
    gu_text(dir, "opencnc3d-module-removal.txt", "2 0\n");
    rc = gu_run(dir, 1, NULL);
    gu_walk(dir, &w);
    n = 0;
    for (i = 0; gu_player[i]; i++) want[n++] = gu_player[i];
    for (i = 0; gu_launcher_own[i]; i++) want[n++] = gu_launcher_own[i];
    for (i = 0; gu_settings[i]; i++) want[n++] = gu_settings[i];
    want[n] = NULL;
    CHECK(rc == 0, "keep-settings uninstall returned %d", rc);
    CHECK(gu_exactly("keep-settings", &w, want), "keep-settings: the folder is not exactly the player's files, the settings and the launcher's own");
    printf("UNINSTALL-GATE|keep-settings|rc=%d|files before=%lld|after=%d (want %d)\n", rc, size_before, w.n, n);
    cases++;

    /* 2. Settings and logs removed too, through the launcher program when one is given. */
    snprintf(dir, sizeof dir, "%s/all", argv[1]);
    gu_fixture(dir);
    rc = gu_run(dir, 0, argc > 2 ? argv[2] : NULL);
    gu_walk(dir, &w);
    n = 0;
    for (i = 0; gu_player[i]; i++) want[n++] = gu_player[i];
    for (i = 0; gu_launcher_own[i]; i++) want[n++] = gu_launcher_own[i];
    want[n] = NULL;
    CHECK(rc == 0, "full uninstall returned %d", rc);
    CHECK(gu_exactly("with-settings", &w, want), "full uninstall: the folder is not exactly the player's files and the launcher's own");
    printf("UNINSTALL-GATE|with-settings|via=%s|rc=%d|after=%d (want %d)\n", argc > 2 ? "launcher program" : "in process", rc, w.n, n);
    cases++;

    /* 3. A foreign file sitting on a staging name refuses the whole run before any
     *    file changes: same names, same bytes. */
    snprintf(dir, sizeof dir, "%s/foreign", argv[1]);
    gu_fixture(dir);
    gu_put(dir, "CONQUER.MIX.module-removing");
    gu_walk(dir, &w);
    before = gu_digest(dir, &w);
    n = w.n;
    rc = gu_run(dir, 0, NULL);
    gu_walk(dir, &w);
    CHECK(rc != 0, "a foreign staging file did not stop the uninstall");
    CHECK(w.n == n && gu_digest(dir, &w) == before, "a refused uninstall changed the folder (%d files, was %d)", w.n, n);
    printf("UNINSTALL-GATE|foreign-staging|rc=%d|unchanged=%d\n", rc, w.n == n && gu_digest(dir, &w) == before);
    cases++;

    /* 4. A folder the game owns that is really a link elsewhere: refused, and the
     *    files behind the link are untouched. */
    snprintf(dir, sizeof dir, "%s/linked", argv[1]);
    gu_fixture(dir);
    snprintf(path, sizeof path, "%s/elsewhere", argv[1]);
    gu_put(argv[1], "elsewhere/a.pack");
    snprintf(other, sizeof other, "%s/content", dir);
    snprintf(path, sizeof path, "%s/content/a.pack", dir);
    assert(!remove(path) && !rmdir(other));
    snprintf(path, sizeof path, "%s/elsewhere", argv[1]);
    assert(!symlink(path, other));
    gu_walk(dir, &w);
    before = gu_digest(dir, &w);
    n = w.n;
    rc = gu_run(dir, 0, NULL);
    gu_walk(dir, &w);
    snprintf(path, sizeof path, "%s/elsewhere/a.pack", argv[1]);
    CHECK(rc != 0 && w.n == n && gu_digest(dir, &w) == before && l_has_named_file(argv[1], "elsewhere/a.pack"),
          "a linked game folder was not refused with everything kept");
    printf("UNINSTALL-GATE|linked-folder|rc=%d|kept=%d\n", rc, w.n == n);
    cases++;

    /* 5. Another launcher holding the folder: refused, nothing changed. */
    snprintf(dir, sizeof dir, "%s/locked", argv[1]);
    gu_fixture(dir);
    gu_walk(dir, &w);
    before = gu_digest(dir, &w);
    n = w.n;
    assert(lz_lock(dir, &lock) == 1);
    {
        /* the other holder is this process here, so ask for a second lock the way
         * the uninstall does; it must be refused */
        LZ_Lock second;
        CHECK(lz_lock_new(dir, &second) == 0, "a second lock on a held folder was granted");
    }
    rc = gu_run(dir, 0, NULL);
    lz_unlock(&lock);
    gu_walk(dir, &w);
    CHECK(rc != 0 && w.n == n && gu_digest(dir, &w) == before, "a held folder was not refused with everything kept");
    printf("UNINSTALL-GATE|held-by-another|rc=%d|unchanged=%d\n", rc, w.n == n);
    cases++;

    /* 6. The single game Remove is unchanged: Dawn3D removed while Red Alert stays
     *    keeps every file the two share, and Red Alert's own. */
    snprintf(dir, sizeof dir, "%s/remove-one", argv[1]);
    gu_fixture(dir);
    assert(lz_lock(dir, &lock) == 1);
    CHECK(lmod_remove(dir, 0, err, sizeof err), "removing Dawn3D alone failed: %s", err);
    lz_unlock(&lock);
    CHECK(!lmod_td_present(dir), "Dawn3D is still present after its Remove");
    for (i = 0; gu_ra[i]; i++) CHECK(l_has_named_file(dir, gu_ra[i]), "Remove of Dawn3D took %s, which Red Alert needs", gu_ra[i]);
    for (i = 0; gu_player[i]; i++) CHECK(l_has_named_file(dir, gu_player[i]), "Remove of Dawn3D took the player's %s", gu_player[i]);
    CHECK(l_has_named_file(dir, "opencnc3d-module-ra.txt"), "Remove of Dawn3D took Red Alert's file list");
    printf("UNINSTALL-GATE|single-game-remove|shared files kept=%d\n", l_has_named_file(dir, "ebld/x/y.ebld") && l_has_named_file(dir, "clearblend_RA_SNOW.pack"));
    cases++;

    if (gu_fail) return 1;
    printf("UNINSTALL-GATE|PASS|%d cases: both games, media, a language pack, the download cache and update leftovers removed by name; player files, settings when kept and the launcher's own left; refusals change nothing\n", cases);
    return 0;
}
