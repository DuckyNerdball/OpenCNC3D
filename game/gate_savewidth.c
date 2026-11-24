/*
 * gate_savewidth.c -- G404, the save width word, with no game, no engine and no window.
 *
 * A saved game's payload begins with one 32-bit little-endian word. A 64-bit Windows build
 * marks it (DS_SAVE_WIDTH_WIN64 in bits 16-23, the engine's OPENCNC3D_SAVE_WIDTH_WIN64)
 * and its engine refuses an unmarked payload. Every other build writes the bare word, and
 * its engine checks no width: it loads whatever word it is given, a marked one included.
 * dossave.c answers, from the host side, which payloads a host of each kind should offer
 * its engine, in two pieces, and this file holds both to their contract:
 *
 *   ds_word_loadable(word, win64)  the host's policy, for both kinds of build (every
 *                                  slot list and load path asks it through
 *                                  ds_slot_loadable, G406);
 *   ds_payload_word(slot, &word)   reading the word off a payload on disk, little-endian,
 *                                  and refusing a payload too short to hold it.
 *
 * Compiled with -DSAVEWIDTH_ENGINE_HEADER and one game's engine folder on the include path,
 * it also holds dossave.h's copy of the two values to that game's dllinterfaceversion.h,
 * which is where the engine takes them from. gates.sh builds it once against each game.
 *
 * Usage: gate_savewidth <scratch directory>. The directory must exist and is written to.
 * One SAVEWIDTH| line per check, then SAVEWIDTH|verdict=PASS|checks=N or verdict=FAIL;
 * the exit status is the number of failed checks.
 */

#include <stdio.h>
#include <string.h>
#include "dossave.h"
#ifdef SAVEWIDTH_ENGINE_HEADER
#include "dllinterfaceversion.h"
#endif

static int g_checks = 0, g_fails = 0;

static void check(const char *what, unsigned int got, unsigned int want)
{
    g_checks++;
    if (got != want) g_fails++;
    printf("SAVEWIDTH|%s|got=0x%08X|want=0x%08X|%s\n", what, got, want,
           got == want ? "ok" : "WRONG");
}

static void word_case(const char *name, unsigned int word, int win64, int want)
{
    char what[96];
    sprintf(what, "loadable|%s|word=0x%08X|win64=%d", name, word, win64);
    check(what, (unsigned int)ds_word_loadable(word, win64), (unsigned int)want);
}

/* Writes n bytes as slot's payload in the scratch directory. */
static int write_payload(int slot, const unsigned char *b, size_t n)
{
    FILE *f = fopen(ds_payload_path(slot), "wb");
    if (!f) return 0;
    if (n && fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    /* The bare word every build before the 64-bit Windows one wrote, and the Mac still
       writes: CNC_DLL_API_VERSION. */
    const unsigned int bare = 0x102u;
    const unsigned int tagged = 0x102u | DS_SAVE_WIDTH_WIN64;
    /* A payload of each kind as the engine lays it out: the word first, little-endian,
       then the rest of the save (two bytes stand in for it here). */
    static const unsigned char pay_bare[6]   = { 0x02, 0x01, 0x00, 0x00, 0xAA, 0x55 };
    static const unsigned char pay_tagged[6] = { 0x02, 0x01, 0x64, 0x00, 0xAA, 0x55 };
    static const unsigned char pay_short[3]  = { 0x02, 0x01, 0x64 };
    unsigned int w;

    if (argc < 2) {
        printf("SAVEWIDTH|usage: gate_savewidth <scratch directory>\n");
        return 99;
    }

    /* The two constants are the engine's: a changed value is a changed save format. */
    check("const|mask", DS_SAVE_WIDTH_MASK, 0x00FF0000u);
    check("const|win64", DS_SAVE_WIDTH_WIN64, 0x00640000u);
    check("const|tagged-word", tagged, 0x00640102u);
#ifdef SAVEWIDTH_ENGINE_HEADER
    check("engine|mask", OPENCNC3D_SAVE_WIDTH_MASK, DS_SAVE_WIDTH_MASK);
    check("engine|win64", OPENCNC3D_SAVE_WIDTH_WIN64, DS_SAVE_WIDTH_WIN64);
    check("engine|version-word", CNC_DLL_API_VERSION, bare);
#else
    printf("SAVEWIDTH|engine|not compiled against an engine header\n");
#endif

    /* THE HOST'S POLICY. A 64-bit Windows host offers its engine only a marked save, and
       any other host only an unmarked one. This is advice to the host, not the engines'
       rule: only a 64-bit Windows engine checks the word at all. */
    word_case("bare", bare, 0, 1);
    word_case("bare", bare, 1, 0);
    word_case("tagged", tagged, 1, 1);
    word_case("tagged", tagged, 0, 0);
    /* Only bits 16-23 decide: the low half is the interface version, which is not the
       width, and the top byte is not read. */
    word_case("bare-other-version", 0x00000103u, 0, 1);
    word_case("bare-other-version", 0x00000103u, 1, 0);
    word_case("tagged-top-byte-set", 0xFF640102u, 1, 1);
    word_case("other-width-byte", 0x00200102u, 1, 0);
    word_case("other-width-byte", 0x00200102u, 0, 1);

    /* THE READER, off real files in a scratch save folder. */
    ds_set_dir(argv[1]);
    if (!write_payload(0, pay_bare, sizeof pay_bare) || !write_payload(1, pay_tagged, sizeof pay_tagged)
        || !write_payload(2, pay_short, sizeof pay_short)) {
        printf("SAVEWIDTH|cannot write payloads into %s\n", argv[1]);
        return 98;
    }
    remove(ds_payload_path(3));

    w = 0xDEADBEEFu;
    check("read|bare|ok", (unsigned int)ds_payload_word(0, &w), 1u);
    check("read|bare|word", w, bare);
    check("read|bare|offered-by-mac-host", (unsigned int)ds_word_loadable(w, 0), 1u);
    check("read|bare|offered-by-win64-host", (unsigned int)ds_word_loadable(w, 1), 0u);

    w = 0xDEADBEEFu;
    check("read|tagged|ok", (unsigned int)ds_payload_word(1, &w), 1u);
    check("read|tagged|word", w, tagged);
    check("read|tagged|offered-by-mac-host", (unsigned int)ds_word_loadable(w, 0), 0u);
    check("read|tagged|offered-by-win64-host", (unsigned int)ds_word_loadable(w, 1), 1u);

    w = 0xDEADBEEFu;
    check("read|short|ok", (unsigned int)ds_payload_word(2, &w), 0u);
    check("read|short|untouched", w, 0xDEADBEEFu);
    check("read|missing|ok", (unsigned int)ds_payload_word(3, &w), 0u);
    check("read|missing|untouched", w, 0xDEADBEEFu);

    remove(ds_payload_path(0));
    remove(ds_payload_path(1));
    remove(ds_payload_path(2));

    if (g_fails) printf("SAVEWIDTH|verdict=FAIL|checks=%d|wrong=%d\n", g_checks, g_fails);
    else printf("SAVEWIDTH|verdict=PASS|checks=%d\n", g_checks);
    return g_fails;
}
