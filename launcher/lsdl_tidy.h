/*
 * lsdl_tidy.h -- removes an SDL2.dll that no program in the install still loads.
 *
 * WHY. Every Windows build up to v0.9.2 was 32-bit and carried SDL2.dll beside its
 * programs, in the install's root and in ra3d/. The 64-bit programs link SDL in and no
 * package carries the DLL any more, but unpacking a package only writes files, so an
 * install that started as v0.9.2 keeps both old copies after Setup or a module update
 * has replaced every program in it. They are inert (no 64-bit program imports them),
 * and they are the one 32-bit file left in a 64-bit install, so the architecture check
 * of an installed folder reads them as a mixed install.
 *
 * WHEN A COPY STAYS. A 32-bit program finds SDL2.dll in its own folder first, and in the
 * folder it was started from after that, so:
 *   - ra3d/SDL2.dll stays while any program or library in ra3d/ is not 64-bit;
 *   - the root's SDL2.dll stays while any program or library in the root, or in ra3d/,
 *     is not 64-bit (the launcher starts Red Alert's game from the root, and a 32-bit
 *     launcher, which a module update never replaces, is in the root itself).
 * Anything that is not plainly a 64-bit x86 image (a 32-bit one, an ARM one, a file that
 * cannot be read) counts as not 64-bit, so a doubt keeps the DLL. The one exception is
 * Setup's uninstaller, uninstall.exe, which is 32-bit by design and loads no SDL.
 *
 * Called by the launcher on Windows, under its install lock, at start and after every
 * module install (lmodule.c). Header only, so the source lists do not change; a gate
 * compiles it on any platform against folders of hand-made images.
 */
#ifndef LSDL_TIDY_H
#define LSDL_TIDY_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#define LSDL_I386  0x014cu
#define LSDL_AMD64 0x8664u

/* The PE machine of an image: 0x14c i386, 0x8664 x86-64, 0 when it is not a PE image or
 * cannot be read. */
static unsigned lsdl_pe_machine(const char *path)
{
    unsigned char dos[64], pe[6];
    unsigned long off;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fread(dos, 1, sizeof dos, f) != sizeof dos || dos[0] != 'M' || dos[1] != 'Z') {
        fclose(f);
        return 0;
    }
    off = (unsigned long)dos[0x3c] | (unsigned long)dos[0x3d] << 8
        | (unsigned long)dos[0x3e] << 16 | (unsigned long)dos[0x3f] << 24;
    if (off < sizeof dos || off > 0x100000ul || fseek(f, (long)off, SEEK_SET)
        || fread(pe, 1, sizeof pe, f) != sizeof pe || memcmp(pe, "PE\0\0", 4)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return (unsigned)pe[4] | (unsigned)pe[5] << 8;
}

static int lsdl_same(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return !*a && !*b;
}

static int lsdl_is_image(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && (lsdl_same(name + n - 4, ".exe") || lsdl_same(name + n - 4, ".dll"));
}

/* One folder's entries, in whatever order the system lists them; returns how many were
 * read, -1 when the folder cannot be opened. */
typedef int (*LSDL_Visit)(const char *dir, const char *name, void *user);
static int lsdl_each(const char *dir, LSDL_Visit visit, void *user)
{
    int n = 0;
#ifdef _WIN32
    char pattern[1200];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    if (snprintf(pattern, sizeof pattern, "%s\\*", dir) >= (int)sizeof pattern) return -1;
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        n++;
        if (visit(dir, fd.cFileName, user)) break;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    struct dirent *e;
    DIR *d = opendir(dir);
    if (!d) return -1;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        n++;
        if (visit(dir, e->d_name, user)) break;
    }
    closedir(d);
#endif
    return n;
}

typedef struct {
    char why[256];   /* the first file that keeps the DLL, as "name (machine 0x...)" */
    char sdl[256];   /* the DLL's own name as the folder spells it, or "" */
} LSDL_Folder;

static int lsdl_visit(const char *dir, const char *name, void *user)
{
    LSDL_Folder *f = (LSDL_Folder *)user;
    char path[1400];
    unsigned m;
    if (lsdl_same(name, "SDL2.dll")) {
        snprintf(f->sdl, sizeof f->sdl, "%s", name);
        return 0;
    }
    if (f->why[0] || !lsdl_is_image(name) || lsdl_same(name, "uninstall.exe")) return 0;
    if (snprintf(path, sizeof path, "%s/%s", dir, name) >= (int)sizeof path) {
        snprintf(f->why, sizeof f->why, "%.200s (path too long to read)", name);
        return 0;
    }
    m = lsdl_pe_machine(path);
    if (m != LSDL_AMD64)
        snprintf(f->why, sizeof f->why, "%.200s (machine 0x%04x)", name, m);
    return 0;
}

static void lsdl_scan(const char *dir, LSDL_Folder *f)
{
    memset(f, 0, sizeof *f);
    lsdl_each(dir, lsdl_visit, f);
}

static int lsdl_drop(const char *dir, const LSDL_Folder *f, const char *keeper, const char *label)
{
    char path[1400];
    if (!f->sdl[0]) return 0;
    if (keeper && *keeper) {
        printf("LSDL|kept|%s|%s is not 64-bit\n", label, keeper);
        return 0;
    }
    if (snprintf(path, sizeof path, "%s/%s", dir, f->sdl) >= (int)sizeof path) return 0;
    if (remove(path) != 0) {
        printf("LSDL|kept|%s|it could not be removed (in use?)\n", label);
        return 0;
    }
    printf("LSDL|removed|%s|no program beside it loads it\n", label);
    return 1;
}

/* Returns how many copies were removed (0, 1 or 2). */
static int lsdl_tidy(const char *dir)
{
    char ra[1200], keeper[300];
    LSDL_Folder root, sub;
    int removed = 0;
    if (!dir || !*dir || snprintf(ra, sizeof ra, "%s/ra3d", dir) >= (int)sizeof ra) return 0;
    lsdl_scan(dir, &root);
    lsdl_scan(ra, &sub);
    removed += lsdl_drop(ra, &sub, sub.why, "ra3d/SDL2.dll");
    if (root.why[0]) snprintf(keeper, sizeof keeper, "%s", root.why);
    else if (sub.why[0]) snprintf(keeper, sizeof keeper, "ra3d/%s", sub.why);
    else keeper[0] = 0;
    removed += lsdl_drop(dir, &root, keeper, "SDL2.dll");
    fflush(stdout);
    return removed;
}

#endif
