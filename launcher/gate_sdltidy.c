/*
 * gate_sdltidy.c -- G407: lsdl_tidy.h removes an old SDL2.dll exactly when no program
 * beside it can still load it, and keeps it on any doubt.
 *
 *   cc -std=gnu89 -Wall -Wextra -o gate_sdltidy launcher/gate_sdltidy.c
 *   ./gate_sdltidy <empty scratch folder>
 *
 * Each case lays out an install from hand-made images (a DOS stub and a PE header whose
 * machine field says 64-bit x86, 32-bit x86 or ARM64, or a file that is no image at
 * all), runs lsdl_tidy over it, and checks which SDL2.dll copies are still there. One
 * GATESDL|case|... line per case, then GATESDL|PASSED or GATESDL|FAILED.
 */
#include "lsdl_tidy.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_fail = 0;

static void put(const char *dir, const char *name, unsigned machine)
{
    unsigned char img[0x48];
    char path[1400];
    FILE *f;
    memset(img, 0, sizeof img);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (!f) { printf("GATESDL|setup|cannot write %s\n", path); g_fail++; return; }
    if (machine == 1) {
        fputs("not an image at all", f);              /* unreadable as PE */
    } else {
        img[0] = 'M'; img[1] = 'Z';
        img[0x3c] = 0x40;                              /* e_lfanew */
        memcpy(img + 0x40, "PE\0\0", 4);
        img[0x44] = (unsigned char)(machine & 0xff);
        img[0x45] = (unsigned char)(machine >> 8);
        fwrite(img, 1, sizeof img, f);
    }
    fclose(f);
}

static int here(const char *dir, const char *name)
{
    char path[1400];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void fresh(const char *root, const char *name, char *dir, char *ra)
{
    snprintf(dir, 1200, "%s/%s", root, name);
    snprintf(ra, 1200, "%s/ra3d", dir);
    mkdir(dir, 0755);
    mkdir(ra, 0755);
}

/* want: 1 when that copy must still be there afterwards */
static void judge(const char *name, const char *dir, const char *ra, const char *sdlname,
                  int wantRoot, int wantRa, int wantRemoved, int removed)
{
    const int r = here(dir, sdlname), s = here(ra, "SDL2.dll");
    const int ok = r == wantRoot && s == wantRa && removed == wantRemoved;
    printf("GATESDL|case|%s|%s|root=%s ra3d=%s removed=%d (want root=%s ra3d=%s removed=%d)\n",
           name, ok ? "ok" : "WRONG", r ? "kept" : "gone", s ? "kept" : "gone", removed,
           wantRoot ? "kept" : "gone", wantRa ? "kept" : "gone", wantRemoved);
    if (!ok) g_fail++;
}

int main(int argc, char **argv)
{
    char dir[1200], ra[1200];
    int n;
    if (argc != 2) { fprintf(stderr, "usage: gate_sdltidy <empty folder>\n"); return 2; }

    /* (1) a v0.9.2 install after a 64-bit Setup and both 64-bit modules: both go.
     * Setup's 32-bit uninstaller does not keep them. */
    fresh(argv[1], "upgraded", dir, ra);
    put(dir, "OpenCNC3D.exe", LSDL_AMD64); put(dir, "opencnc3d-game.exe", LSDL_AMD64);
    put(dir, "cnc_eyes.exe", LSDL_AMD64); put(dir, "TiberianDawn.dll", LSDL_AMD64);
    put(dir, "uninstall.exe", LSDL_I386); put(dir, "SDL2.dll", LSDL_I386);
    put(ra, "opencnc3d-game.exe", LSDL_AMD64); put(ra, "RedAlert.dll", LSDL_AMD64);
    put(ra, "SDL2.dll", LSDL_I386);
    n = lsdl_tidy(dir);
    judge("all-64-bit", dir, ra, "SDL2.dll", 0, 0, 2, n);

    /* (2) the 32-bit launcher a module update cannot replace: the root's stays, the
     * ra3d copy (64-bit programs only beside it) goes. */
    fresh(argv[1], "oldlauncher", dir, ra);
    put(dir, "OpenCNC3D.exe", LSDL_I386); put(dir, "opencnc3d-game.exe", LSDL_AMD64);
    put(dir, "SDL2.dll", LSDL_I386);
    put(ra, "opencnc3d-game.exe", LSDL_AMD64); put(ra, "RedAlert.dll", LSDL_AMD64);
    put(ra, "SDL2.dll", LSDL_I386);
    n = lsdl_tidy(dir);
    judge("32-bit-launcher", dir, ra, "SDL2.dll", 1, 0, 1, n);

    /* (3) Tiberian Dawn updated, Red Alert still the 32-bit module: both stay, the root's
     * because Red Alert's 32-bit game may find it from there. */
    fresh(argv[1], "raold", dir, ra);
    put(dir, "OpenCNC3D.exe", LSDL_AMD64); put(dir, "opencnc3d-game.exe", LSDL_AMD64);
    put(dir, "SDL2.dll", LSDL_I386);
    put(ra, "opencnc3d-game.exe", LSDL_I386); put(ra, "RedAlert.dll", LSDL_I386);
    put(ra, "SDL2.dll", LSDL_I386);
    n = lsdl_tidy(dir);
    judge("ra-still-32-bit", dir, ra, "SDL2.dll", 1, 1, 0, n);

    /* (4) a 32-bit game left in the root (Tiberian Dawn not updated): the root's stays. */
    fresh(argv[1], "tdold", dir, ra);
    put(dir, "OpenCNC3D.exe", LSDL_AMD64); put(dir, "cnc_eyes.exe", LSDL_I386);
    put(dir, "SDL2.dll", LSDL_I386);
    put(ra, "opencnc3d-game.exe", LSDL_AMD64); put(ra, "SDL2.dll", LSDL_I386);
    n = lsdl_tidy(dir);
    judge("td-still-32-bit", dir, ra, "SDL2.dll", 1, 0, 1, n);

    /* (5) a doubt keeps it: an image that is no PE, and an ARM64 one. */
    fresh(argv[1], "doubt", dir, ra);
    put(dir, "OpenCNC3D.exe", LSDL_AMD64); put(dir, "mystery.dll", 1);
    put(dir, "SDL2.dll", LSDL_I386);
    put(ra, "opencnc3d-game.exe", 0xAA64u); put(ra, "SDL2.dll", LSDL_I386);
    n = lsdl_tidy(dir);
    judge("unreadable-and-arm64", dir, ra, "SDL2.dll", 1, 1, 0, n);

    /* (6) the DLL spelt in another case is found and removed under that name, and an
     * install with no ra3d folder and no DLL is left alone. */
    fresh(argv[1], "lowercase", dir, ra);
    put(dir, "OpenCNC3D.exe", LSDL_AMD64); put(dir, "sdl2.DLL", LSDL_I386);
    n = lsdl_tidy(dir);
    judge("other-case", dir, ra, "sdl2.DLL", 0, 0, 1, n);
    snprintf(dir, sizeof dir, "%s/none", argv[1]);
    mkdir(dir, 0755);
    put(dir, "OpenCNC3D.exe", LSDL_AMD64);
    n = lsdl_tidy(dir);
    judge("nothing-to-do", dir, "/nonexistent-ra3d", "SDL2.dll", 0, 0, 0, n);

    printf("GATESDL|%s|%d wrong\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail ? 1 : 0;
}
