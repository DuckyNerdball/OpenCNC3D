/* gate-log-keep.c -- the last session's log kept, checked in a scratch folder.
 *
 *   cc -std=gnu89 -Wall -Wextra -I game tools/gate-log-keep.c -o gate-log-keep
 *   ./gate-log-keep <empty scratch folder>
 *
 * Exercises game/log_keep.h, the rotation the game and the launcher run before they
 * open their logs, on the host: how a previous session is judged (clean, crashed with a
 * FATAL line, crashed with no clean-exit line, an older build's unmarked log), that the
 * old log moves to the previous-session name, that a crash leaves a dated copy and only
 * the newest five copies are kept, that the launcher's rotation (no crash judgement)
 * makes no copy, that a FATAL: which is not at the start of a line is not a crash, that
 * a folder that cannot be written is survived, and the launcher's words for a game that
 * closed as it started. Prints one LOGKEEP|ok line, or a LOGKEEP|FAIL line per fault.
 *
 * What it cannot show: the Windows launch itself (WaitForSingleObject on the game's
 * process), which only a run on Windows can.
 */
#include "log_keep.h"
#include <sys/stat.h>

static int fails = 0, checks = 0;
static void check(int ok, const char *what)
{
    checks++;
    if (!ok) { fails++; printf("LOGKEEP|FAIL|%s\n", what); }
}

static char g_dir[900];
static void put(const char *name, const char *text)
{
    char p[1100];
    FILE *f;
    lk_join(p, sizeof p, g_dir, name);
    f = fopen(p, "wb");
    if (f) { fputs(text, f); fclose(f); }
}
static int has(const char *name)
{
    char p[1100];
    lk_join(p, sizeof p, g_dir, name);
    return lk_exists(p);
}
static int contains(const char *path, const char *needle)
{
    static char buf[1 << 16];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, needle) != NULL;
}
static int crash_copies(void)
{
    char cmd[1200];
    FILE *p;
    int n = 0;
    snprintf(cmd, sizeof cmd, "ls '%s' | grep -c '^opencnc3d-crash-.*\\.txt$'", g_dir);
    p = popen(cmd, "r");
    if (p) { if (fscanf(p, "%d", &n) != 1) n = -1; pclose(p); }
    return n;
}
static void clear(void)
{
    char cmd[1200];
    snprintf(cmd, sizeof cmd, "rm -f '%s'/*.txt", g_dir);
    if (system(cmd) != 0) printf("LOGKEEP|note|could not clear the scratch folder\n");
}
static int rotate(LogKeep *k)
{
    return log_keep_rotate(g_dir, LOG_KEEP_GAME_LOG, LOG_KEEP_GAME_PREVIOUS,
                           LOG_KEEP_CRASH_PREFIX, LOG_KEEP_CRASH_KEEP, k);
}

int main(int argc, char **argv)
{
    LogKeep k;
    char p[1100];
    if (argc < 2) { fprintf(stderr, "usage: gate-log-keep <scratch folder>\n"); return 2; }
    snprintf(g_dir, sizeof g_dir, "%s", argv[1]);

    /* (a) no earlier log */
    clear();
    check(rotate(&k) == 0 && !k.had_previous && !k.crashed, "(a) a folder with no log claimed a previous session");

    /* (b) a clean session: kept as previous, no crash copy */
    clear();
    put(LOG_KEEP_GAME_LOG, "OpenCNC3D v1 -- log opened at x\n" LOG_KEEP_MARK_LINE "\nAPP|state|MENU\n" LOG_KEEP_EXIT_LINE "\n");
    rotate(&k);
    check(k.had_previous && k.moved && k.marked && k.ended && !k.crashed && !k.copied, "(b) a clean session was judged wrongly");
    check(!has(LOG_KEEP_GAME_LOG) && has(LOG_KEEP_GAME_PREVIOUS) && crash_copies() == 0, "(b) a clean session was not moved to the previous name, or left a crash copy");
    lk_join(p, sizeof p, g_dir, LOG_KEEP_GAME_PREVIOUS);
    check(contains(p, "log opened at x"), "(b) the previous log does not carry the old session's first line");

    /* (c) a FATAL line, written after a Windows CRLF, even with a later exit line */
    clear();
    put(LOG_KEEP_GAME_PREVIOUS, "an older previous log\n");
    put(LOG_KEEP_GAME_LOG, "OpenCNC3D v1 -- log opened at x\r\n" LOG_KEEP_MARK_LINE "\r\n\r\nFATAL: signal 11 while: game_loop tick\r\n" LOG_KEEP_EXIT_LINE "\r\n");
    rotate(&k);
    check(k.crashed && k.fatal && k.copied && !strcmp(k.fatal_line, "FATAL: signal 11 while: game_loop tick"), "(c) a FATAL session was not judged a crash, or its line was not read");
    check(crash_copies() == 1 && contains(k.crash_copy, "FATAL: signal 11") && contains(k.previous, "FATAL: signal 11"), "(c) the crash copy or the previous log does not carry the FATAL line");
    lk_join(p, sizeof p, g_dir, LOG_KEEP_GAME_PREVIOUS);
    check(!contains(p, "an older previous log"), "(c) the previous-session file was not replaced");

    /* (d) marked, no exit line: killed, hung or crashed without a line */
    clear();
    put(LOG_KEEP_GAME_LOG, "OpenCNC3D v1 -- log opened at x\n" LOG_KEEP_MARK_LINE "\nAPP|state|GAME\n");
    rotate(&k);
    check(k.crashed && !k.fatal && k.copied && crash_copies() == 1, "(d) a marked log without its exit line was not judged a crash");

    /* (e) an older build's log: no mark, no FATAL, so not a crash */
    clear();
    put(LOG_KEEP_GAME_LOG, "OpenCNC3D v0.9.6 -- log opened at x\nAPP|state|MENU\n");
    rotate(&k);
    check(!k.crashed && !k.marked && k.moved && crash_copies() == 0, "(e) an older build's unmarked log was judged a crash");

    /* (f) FATAL: in the middle of a line longer than the read buffer is not a crash */
    clear();
    {
        static char big[9000];
        memset(big, 'x', sizeof big);
        memcpy(big, LOG_KEEP_MARK_LINE "\n", strlen(LOG_KEEP_MARK_LINE) + 1);
        /* exactly where the second read of this line starts */
        memcpy(big + strlen(LOG_KEEP_MARK_LINE) + 1 + 4095, "FATAL: not a line start", 23);
        big[8000] = '\n';
        strcpy(big + 8001, LOG_KEEP_EXIT_LINE "\n");
        put(LOG_KEEP_GAME_LOG, big);
    }
    rotate(&k);
    check(!k.fatal && !k.crashed, "(f) FATAL: inside a long line was taken for a crash line");

    /* (g) only the newest five crash copies are kept */
    clear();
    put("opencnc3d-crash-20200101-000001.txt", "1");
    put("opencnc3d-crash-20200101-000002.txt", "2");
    put("opencnc3d-crash-20200101-000003.txt", "3");
    put("opencnc3d-crash-20200101-000004.txt", "4");
    put("opencnc3d-crash-20200101-000005.txt", "5");
    put("opencnc3d-crash-20200101-000006.txt", "6");
    put(LOG_KEEP_GAME_LOG, "FATAL: signal 6 while: x\n");
    rotate(&k);
    check(k.copied && crash_copies() == 5 && k.pruned == 2, "(g) the crash copies were not cut to the newest five");
    check(!has("opencnc3d-crash-20200101-000001.txt") && !has("opencnc3d-crash-20200101-000002.txt")
          && has("opencnc3d-crash-20200101-000003.txt") && lk_exists(k.crash_copy), "(g) the wrong crash copies were removed");

    /* (h) two crashes stamped in the same second keep both copies */
    clear();
    put(LOG_KEEP_GAME_LOG, "FATAL: signal 6 while: x\n");
    rotate(&k);
    {
        char first[1100];
        snprintf(first, sizeof first, "%s", k.crash_copy);
        put(LOG_KEEP_GAME_LOG, "FATAL: signal 11 while: y\n");
        {
            /* same modification second as the first copy's source */
            struct stat sb;
            char cur[1100];
            lk_join(cur, sizeof cur, g_dir, LOG_KEEP_GAME_LOG);
            if (stat(first, &sb) == 0) {
                char cmd[2400];
                snprintf(cmd, sizeof cmd, "touch -r '%s' '%s'", first, cur);
                if (system(cmd) != 0) printf("LOGKEEP|note|touch failed\n");
            }
        }
        rotate(&k);
        check(crash_copies() == 2 && strcmp(first, k.crash_copy) && contains(first, "signal 6")
              && contains(k.crash_copy, "signal 11"), "(h) a second crash in the same second overwrote the first copy");
    }

    /* (i) the launcher's rotation: previous kept, no judgement, no copy */
    clear();
    put("opencnc3d-launcher-log.txt", "OpenCNC3D launcher x -- log opened\nFATAL: whatever\n");
    log_keep_rotate(g_dir, "opencnc3d-launcher-log.txt", "opencnc3d-launcher-log-previous.txt", NULL, 0, &k);
    check(k.moved && !k.crashed && has("opencnc3d-launcher-log-previous.txt") && crash_copies() == 0, "(i) the launcher's log rotation judged or copied");

    /* (j) a folder that cannot be written: nothing moves, nothing fails */
    clear();
    put(LOG_KEEP_GAME_LOG, "FATAL: signal 6 while: x\n");
    chmod(g_dir, 0555);
    rotate(&k);
    chmod(g_dir, 0755);
    check(k.had_previous && !k.moved && !k.copied && has(LOG_KEEP_GAME_LOG), "(j) a read-only folder was not survived quietly");

    /* (k) the launcher's words for a game that closed as it started */
    {
        char err[512];
        log_keep_early_exit_text(err, sizeof err, "opencnc3d-game.exe", 0xC0000005ul, "C:\\Games\\OpenCNC3D", 1,
                                 "C:\\Games\\OpenCNC3D\\opencnc3d-launcher-log.txt");
        printf("LOGKEEP|text|game|%s\n", err);
        check(strstr(err, "opencnc3d-game.exe closed as it started (exit code 0xC0000005)") != NULL
              && strstr(err, "opencnc3d-log.txt in C:\\Games\\OpenCNC3D") != NULL
              && strstr(err, "launcher's own log is C:\\Games\\OpenCNC3D\\opencnc3d-launcher-log.txt") != NULL,
              "(k) the game's early-exit message is wrong");
        log_keep_early_exit_text(err, sizeof err, "cnc_eyes.exe", 0xC0000135ul, "C:\\Games\\OpenCNC3D", 0, "");
        printf("LOGKEEP|text|editor|%s\n", err);
        check(strstr(err, "exit code 0xC0000135") != NULL && strstr(err, "opencnc3d-log.txt") == NULL,
              "(k) the editor's early-exit message names a log it does not write");
    }

    clear();
    if (fails) return 1;
    printf("LOGKEEP|ok|%d checks\n", checks);
    return 0;
}
