/* EVERY NAME A GAME MODULE CARRIES IS ONE THE LAUNCHER WILL INSTALL.
 *
 *   check-module-manifest td|ra VERSION MANIFEST...
 *
 * A module ZIP carries its file list (opencnc3d-module-td.txt, opencnc3d-module-ra.txt), and
 * the launcher refuses the whole module if a single name in it is outside the fixed list of
 * runtime names and folders it may write (lmod_runtime_name, launcher/lmodule_remote.h), so
 * an update can never write over player data. The player then sees "The module file
 * manifest is invalid or names protected player data" and has no game.
 *
 * This reads each list through the launcher's OWN parser, compiled from the launcher's
 * source, so the rule checked is the rule players run, and names every refused file.
 * The platform line is the launcher's exact-match sanity check and the only part of the
 * list that differs between the Mac and Windows modules (the name rules do not), so it is
 * read as this build's own platform: one Mac-built checker judges both platforms' lists.
 *
 * The launchers already in players' hands carry their own copy of the list, which only
 * ever grows. A name refused here is refused by every one of them; a name accepted here
 * but new to the list would still be refused by older launchers, which is why the module
 * packager places files only under names the list has long carried. */
#include "../../launcher/lmodule.c"

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    buf[n] = 0;
    fclose(f);
    return buf;
}

/* the list with its platform line read as this build's platform */
static char *as_this_platform(const char *text)
{
    size_t cap = strlen(text) + 64, o = 0;
    char *out = (char *)malloc(cap);
    const char *p = text;
    if (!out) return NULL;
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len >= 9 && !strncmp(p, "platform ", 9)) {
            o += (size_t)snprintf(out + o, cap - o, "platform %s", LMOD_PLATFORM);
        } else {
            memcpy(out + o, p, len);
            o += len;
        }
        if (e) out[o++] = '\n';
        p = e ? e + 1 : p + len;
    }
    out[o] = 0;
    return out;
}

int main(int argc, char **argv)
{
    int target, i, bad = 0;
    if (argc < 4 || (strcmp(argv[1], "td") && strcmp(argv[1], "ra"))) {
        fprintf(stderr, "usage: check-module-manifest td|ra VERSION MANIFEST...\n");
        return 2;
    }
    target = !strcmp(argv[1], "ra");
    for (i = 3; i < argc; i++) {
        char err[512] = {0};
        char *raw = read_all(argv[i]), *text;
        LMOD_Files files;
        if (!raw) { fprintf(stderr, "MANIFEST|%s|unreadable\n", argv[i]); bad++; continue; }
        text = as_this_platform(raw);
        if (text && lmod_files_parse(text, target, argv[2], &files, err, (int)sizeof err)) {
            printf("MANIFEST|%s|ok|files=%d\n", argv[i], files.count - 1);
            lmod_files_free(&files);
        } else {
            /* say which names the launcher refuses, one line each */
            char *line, *save = NULL, *copy = text ? strdup(text) : NULL;
            int named = 0;
            for (line = copy ? strtok_r(copy, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
                char sha[80], name[1024];
                long long size;
                if (sscanf(line, "file %79s %lld %1023s", sha, &size, name) == 3
                    && !lmod_runtime_name(target, name)) {
                    printf("MANIFEST|%s|refused|%s\n", argv[i], name);
                    named++;
                }
            }
            if (!named) printf("MANIFEST|%s|refused|the list itself (%s)\n", argv[i], err);
            free(copy);
            bad++;
        }
        free(text);
        free(raw);
    }
    return bad ? 1 : 0;
}
