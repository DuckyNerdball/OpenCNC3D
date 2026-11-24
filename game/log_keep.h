/* ---------------------------------------------------------------------------------- *
 *  THE LAST SESSION'S LOG SURVIVES THE NEXT START.
 *
 *  The game writes one log, opencnc3d-log.txt, and used to open it for writing from
 *  zero at every start. The crash lines (game/fatal_guard.h, the crash-stage handler in
 *  cnc_eyes.cpp) and a broken network match's NET| and GAMEOVER| lines were all in that
 *  file, and the natural thing to do after a crash is to start the game again, which
 *  erased them. Both games share the file, so starting Red Alert after a Tiberian Dawn
 *  crash erased it just the same.
 *
 *  So, before the log is opened, log_keep_rotate:
 *    - reads the old log and decides how that session ended. It CRASHED if it carries a
 *      line starting "FATAL:", or if it was written by a build that marks a clean exit
 *      (LOG_KEEP_MARK_LINE near the top) and has no LOG_KEEP_EXIT_LINE. A log from an
 *      older build, which never wrote the exit line, is judged by its FATAL lines alone,
 *      so the first start after an update does not claim a crash that did not happen;
 *    - moves it to the previous-session name, replacing the one before;
 *    - and for a crashed session also copies it to <prefix>YYYYMMDD-HHMMSS.txt, stamped
 *      with the old log's own modification time (when that session last wrote, which is
 *      when it died), keeping the newest LOG_KEEP_CRASH_KEEP of them.
 *  Every step that fails is skipped silently: a folder that cannot be written, or a log
 *  another running copy of the game still holds open (Windows refuses to move it), must
 *  never stop the game, or the new log, from starting.
 *
 *  The exit line is written by an atexit hook, so every ordinary ending writes it (the
 *  menu's Exit, a closed window, the end of a scripted run), and the fatal paths, which
 *  end in abort() or by re-raising a signal, never do.
 *
 *  Header only, plain C that also compiles as C++: the game program and the launcher
 *  both use it (the launcher keeps a previous copy of its own log with no crash
 *  judgement), and a host-compiled gate exercises it in a scratch folder.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_LOG_KEEP_H
#define OPENCNC3D_LOG_KEEP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

/* Not every user of this header needs every helper. */
#if defined(__GNUC__) || defined(__clang__)
#define LK_FN static __attribute__((unused))
#else
#define LK_FN static
#endif

#define LOG_KEEP_MARK_LINE "APP|log|exit-marked"
#define LOG_KEEP_EXIT_LINE "APP|exit|clean"
#define LOG_KEEP_GAME_LOG "opencnc3d-log.txt"
#define LOG_KEEP_GAME_PREVIOUS "opencnc3d-log-previous.txt"
#define LOG_KEEP_CRASH_PREFIX "opencnc3d-crash-"
#define LOG_KEEP_CRASH_KEEP 5

typedef struct LogKeep {
    int had_previous;        /* there was a log from an earlier session                  */
    int moved;               /* it now lives under the previous-session name             */
    int fatal;               /* it carries a line starting "FATAL:"                      */
    int marked;              /* written by a build that marks a clean exit               */
    int ended;               /* it carries the clean-exit line                           */
    int crashed;             /* fatal, or marked and not ended                           */
    int copied;              /* a dated crash copy was written                           */
    int pruned;              /* older crash copies removed to keep the newest few        */
    char previous[1100];     /* where the earlier session's log is now                   */
    char crash_copy[1100];   /* the dated copy of a crashed session, or ""              */
    char fatal_line[240];    /* the first FATAL: line, without its line end             */
} LogKeep;

#ifdef _WIN32
/* Paths are UTF-8 (SDL hands them over that way); Windows wants them wide. */
LK_FN int lk_wide(const char *s, wchar_t *w, int n)
{
    return MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) > 0;
}
LK_FN FILE *lk_open(const char *path, const char *mode)
{
    wchar_t w[1100], m[8];
    int i;
    if (!lk_wide(path, w, 1100)) return NULL;
    for (i = 0; mode[i] && i < 7; i++) m[i] = (wchar_t)mode[i];
    m[i] = 0;
    return _wfopen(w, m);
}
LK_FN int lk_move(const char *from, const char *to)
{
    wchar_t a[1100], b[1100];
    return lk_wide(from, a, 1100) && lk_wide(to, b, 1100)
        && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING) != 0;
}
LK_FN int lk_remove(const char *path)
{
    wchar_t w[1100];
    return lk_wide(path, w, 1100) && DeleteFileW(w) != 0;
}
LK_FN int lk_exists(const char *path)
{
    wchar_t w[1100];
    return lk_wide(path, w, 1100) && GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES;
}
LK_FN int lk_mtime(const char *path, struct tm *out)
{
    wchar_t w[1100];
    WIN32_FILE_ATTRIBUTE_DATA d;
    FILETIME local;
    SYSTEMTIME st;
    if (!lk_wide(path, w, 1100) || !GetFileAttributesExW(w, GetFileExInfoStandard, &d)) return 0;
    if (!FileTimeToLocalFileTime(&d.ftLastWriteTime, &local) || !FileTimeToSystemTime(&local, &st))
        return 0;
    memset(out, 0, sizeof *out);
    out->tm_year = st.wYear - 1900; out->tm_mon = st.wMonth - 1; out->tm_mday = st.wDay;
    out->tm_hour = st.wHour; out->tm_min = st.wMinute; out->tm_sec = st.wSecond;
    return 1;
}
#else
LK_FN FILE *lk_open(const char *path, const char *mode) { return fopen(path, mode); }
LK_FN int lk_move(const char *from, const char *to) { return rename(from, to) == 0; }
LK_FN int lk_remove(const char *path) { return remove(path) == 0; }
LK_FN int lk_exists(const char *path) { struct stat sb; return stat(path, &sb) == 0; }
LK_FN int lk_mtime(const char *path, struct tm *out)
{
    struct stat sb;
    time_t t;
    struct tm *lt;
    if (stat(path, &sb) != 0) return 0;
    t = sb.st_mtime;
    lt = localtime(&t);
    if (!lt) return 0;
    *out = *lt;
    return 1;
}
#endif

/* dir + name. An empty dir means the working folder; a dir without a trailing
   separator gets one. */
LK_FN void lk_join(char *out, size_t n, const char *dir, const char *name)
{
    size_t dl = dir ? strlen(dir) : 0;
    const char *sep = "";
    if (dl && dir[dl - 1] != '/' && dir[dl - 1] != '\\')
#ifdef _WIN32
        sep = "\\";
#else
        sep = "/";
#endif
    snprintf(out, n, "%s%s%s", dl ? dir : "", sep, name);
}

LK_FN int lk_copy(const char *from, const char *to)
{
    static char buf[1 << 15];
    FILE *in = lk_open(from, "rb");
    FILE *out;
    size_t got;
    int ok = 1;
    if (!in) return 0;
    out = lk_open(to, "wb");
    if (!out) { fclose(in); return 0; }
    while ((got = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, got, out) != got) { ok = 0; break; }
    if (ferror(in)) ok = 0;
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) lk_remove(to);
    return ok;
}

LK_FN int lk_cmp(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* Remove the oldest <prefix>*.txt in dir beyond keep. The names carry the date in
   sortable form, so the oldest are the first in name order. */
LK_FN int lk_prune(const char *dir, const char *prefix, int keep)
{
    enum { MAXN = 256, NAMEN = 96 };
    static char names[MAXN][NAMEN];
    int count = 0, removed = 0, i;
    size_t pl = strlen(prefix);
    char path[1200];
#ifdef _WIN32
    wchar_t pattern[1100];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    char pat[1100];
    snprintf(pat, sizeof pat, "%s*.txt", prefix);
    lk_join(path, sizeof path, dir, pat);
    if (!lk_wide(path, pattern, 1100)) return 0;
    h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        char nm[NAMEN];
        if (WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nm, NAMEN, NULL, NULL) <= 0) continue;
        if (count < MAXN && !strncmp(nm, prefix, pl)) snprintf(names[count++], NAMEN, "%s", nm);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir && *dir ? dir : ".");
    struct dirent *e;
    if (!d) return 0;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (count < MAXN && l < NAMEN && l > pl + 4 && !strncmp(e->d_name, prefix, pl)
            && !strcmp(e->d_name + l - 4, ".txt"))
            snprintf(names[count++], NAMEN, "%s", e->d_name);
    }
    closedir(d);
#endif
    qsort(names, (size_t)count, NAMEN, lk_cmp);
    for (i = 0; i + keep < count; i++) {
        lk_join(path, sizeof path, dir, names[i]);
        if (lk_remove(path)) removed++;
    }
    return removed;
}

/* Reads how the session that wrote this log ended. */
LK_FN void lk_scan(FILE *f, LogKeep *k)
{
    char line[4096];
    int at_start = 1;
    while (fgets(line, sizeof line, f)) {
        const size_t l = strlen(line);
        if (at_start) {
            if (!strncmp(line, "FATAL:", 6)) {
                if (!k->fatal) {
                    size_t c;
                    snprintf(k->fatal_line, sizeof k->fatal_line, "%s", line);
                    c = strcspn(k->fatal_line, "\r\n");
                    k->fatal_line[c] = 0;
                }
                k->fatal = 1;
            } else if (!strncmp(line, LOG_KEEP_MARK_LINE, strlen(LOG_KEEP_MARK_LINE))) {
                k->marked = 1;
            } else if (!strncmp(line, LOG_KEEP_EXIT_LINE, strlen(LOG_KEEP_EXIT_LINE))) {
                k->ended = 1;
            }
        }
        at_start = l > 0 && line[l - 1] == '\n';
    }
}

/* Before opening dir/name for writing: keep what is there as dir/previous, and when
   crash_prefix is given and that session crashed, a dated copy as well. Returns 1 when
   an earlier log was found. Never fails loudly; see the head of this file. */
LK_FN int log_keep_rotate(const char *dir, const char *name, const char *previous,
                           const char *crash_prefix, int keep, LogKeep *k)
{
    char cur[1100], prev[1100];
    FILE *f;
    memset(k, 0, sizeof *k);
    lk_join(cur, sizeof cur, dir, name);
    lk_join(prev, sizeof prev, dir, previous);
    f = lk_open(cur, "rb");
    if (!f) return 0;
    k->had_previous = 1;
    if (crash_prefix) {
        lk_scan(f, k);
        k->crashed = k->fatal || (k->marked && !k->ended);
    }
    fclose(f);
    {
        struct tm when;
        const int dated = k->crashed ? lk_mtime(cur, &when) : 0;
        k->moved = lk_move(cur, prev);
        snprintf(k->previous, sizeof k->previous, "%s", k->moved ? prev : cur);
        if (crash_prefix && k->crashed && dated) {
            char stamp[64], file[160], path[1100];
            int i;
            strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &when);
            for (i = 1; i < 100; i++) {
                if (i == 1) snprintf(file, sizeof file, "%s%s.txt", crash_prefix, stamp);
                else snprintf(file, sizeof file, "%s%s-%d.txt", crash_prefix, stamp, i);
                lk_join(path, sizeof path, dir, file);
                if (!lk_exists(path)) break;
            }
            if (i < 100 && lk_copy(k->previous, path)) {
                k->copied = 1;
                snprintf(k->crash_copy, sizeof k->crash_copy, "%s", path);
                k->pruned = lk_prune(dir, crash_prefix, keep > 0 ? keep : 1);
            }
        }
    }
    return 1;
}

/* A path as the player can find it: absolute, for a log opened by a relative name. */
LK_FN void log_keep_absolute(const char *path, char *out, size_t n)
{
#ifdef _WIN32
    wchar_t w[1100], full[1100];
    if (lk_wide(path, w, 1100) && GetFullPathNameW(w, 1100, full, NULL) > 0
        && WideCharToMultiByte(CP_UTF8, 0, full, -1, out, (int)n, NULL, NULL) > 0)
        return;
#else
    if (path[0] != '/') {
        char cwd[1024];
        if (getcwd(cwd, sizeof cwd)) { snprintf(out, n, "%s/%s", cwd, path); return; }
    }
#endif
    snprintf(out, n, "%s", path);
}

/* THE LAUNCHER'S WORDS for a game that closed within moments of being started, with
   the code it closed with (0xC0000135 is a DLL Windows could not find, 0xC0000005 an
   access violation). Here rather than in the launcher so the host gate can check the
   wording; the launch itself only exists on Windows. names_log: the program is the game,
   which writes opencnc3d-log.txt (the editor does not). launcher_log: the launcher's own
   log, or "" when it has none. */
LK_FN void log_keep_early_exit_text(char *err, size_t n, const char *exe, unsigned long code,
                                    const char *dir, int names_log, const char *launcher_log)
{
    char tail[1200];
    tail[0] = 0;
    if (launcher_log && *launcher_log)
        snprintf(tail, sizeof tail, "\n\nThe launcher's own log is %s.", launcher_log);
    if (names_log)
        snprintf(err, n, "%s closed as it started (exit code 0x%08lX).\n\n"
                 "Please send its log with a bug report: opencnc3d-log.txt in %s, or in "
                 "%%APPDATA%%\\OpenCNC3D when that folder cannot be "
                 "written.%s", exe, code, dir, tail);
    else
        snprintf(err, n, "%s closed as it started (exit code 0x%08lX).%s", exe, code, tail);
}

#endif /* OPENCNC3D_LOG_KEEP_H */
