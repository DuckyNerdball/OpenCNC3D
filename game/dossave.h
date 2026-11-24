/*
 * dossave.h -- the save-slot index. Plain C89: no GL, no SDL, no engine header, so the
 * Win98 build takes it unchanged. Same rule dosbar.c and hud640.c follow.
 *
 * WHAT THIS OWNS AND WHAT IT DOES NOT.
 *
 * The ENGINE writes the game. saveload.cpp's Save_Game codes every pointer in the world
 * into a target id, writes the fifteen object heaps, the map, the score, the base and
 * Save_Misc_Values (PlayerPtr, scenario, difficulty, Frame, the win/lose movie names, the
 * selection list, the waypoints, the build level), and DLLExportClass::Save adds the
 * per-player build queues and factory progress on top. None of that is ours and none of it
 * is reimplemented here: CNC_Save_Load is handed an absolute path and does the work.
 *
 * This file owns the INDEX beside it, and it exists for one measured reason: the engine's
 * own header is not host-readable. In a file written by this build the description string
 * sits at offset 10,558, behind a DLL block that explicitly reserves 4095 bytes "for save
 * game expansion", and the one API that could read it (Get_Savefile_Info, saveload.cpp:992)
 * takes an integer id and rebuilds SAVEGAME.%03d itself rather than accepting a path. A
 * host that parsed that header would be parsing a moving target.
 *
 * So each slot is two files: SAVEGAME.nnn, which is 1995's own 8.3-safe name and is the
 * engine's, and one OPENCNC.IDX holding DS_SLOTS fixed 128-byte little-endian records,
 * record n for SAVEGAME.n, written with plain fopen/fwrite exactly the way
 * app/campaign.c writes HALLFAME.DAT.
 *
 * A slot is offered to the player only when BOTH its record is valid AND its payload file
 * is on disk. A row you can select and cannot load is worse than a row that is not there.
 */

#ifndef DOSSAVE_H
#define DOSSAVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* HOW MANY SLOTS. 1995 sets no limit: loaddlg.cpp:612-680 lists every SAVEGAME.* it finds
 * and gives the empty slot the lowest number not in use. The index first held sixteen
 * records, and with all sixteen taken the Save dialog had no empty row left, so a save
 * went over the newest one without a question. A hundred holds one save for every
 * mission of either game with room to spare.
 *
 * The record is unchanged, so an index written with sixteen records reads as it always
 * did: ds_read_index stops at the end of the file and the slots past it are empty. */
#define DS_SLOTS 100
#define DS_REC 128       /* bytes per index record, fixed for ever */
#define DS_DESCR 44      /* DESCRIP_MAX, defines.h:2026 */

typedef struct DS_Slot
{
    int used;
    int slot;                    /* the %03d in SAVEGAME.nnn */
    char descr[DS_DESCR + 1];
    char scen[12];               /* "SCG02EA": which mission, so a load can refuse a
                                    slot saved in a different one */
    char pack[24];               /* "SCG01EA.pack": the terrain, models and tiberium */
    unsigned char camp_active;   /* the campaign record the shell owns */
    unsigned char camp_side;
    unsigned char camp_scenario;
    unsigned char camp_dir;
    unsigned char camp_var;
    unsigned char build;         /* --build, the sidebar's build level */
    unsigned char cam_mode;
    unsigned char game;          /* which game wrote it: DS_GAME_TD (0) or DS_GAME_RA (1).
                                    Byte 103 of the record, which every record written
                                    before the field existed holds as zero, so an old
                                    slot reads as Tiberian Dawn's, which it is. */
    int camx_m, camz_m;          /* camera, in thousandths of a cell */
    int dist_m, zoom_m;
    int music;                   /* ThemeType playing, or -1 */
    int frame;                   /* engine frame, for the list row */
    unsigned int bytes;          /* payload size, for the list row */
    /* NOT IN THE RECORD: the payload file's modification time, read with stat when the
       index is read, so the slot dialog can list newest first the way loaddlg.cpp:690
       sorts on the file's own date. The record is 128 bytes and full, and a timestamp
       that lives in the filesystem anyway does not need a second copy. 0 if unknown. */
    long mtime;
} DS_Slot;

/* WHICH GAME'S SAVES THIS PROCESS SEES. Tiberian Dawn and Red Alert are two modules of one
 * program, and their mission names collide (both have an SCG01EA), so a slot must say which
 * game wrote it: loading one game's payload into the other's engine boots the wrong world.
 * Each module also keeps its saves in a folder of its own (the shell puts Red Alert's in a
 * ra3d/ folder inside the per-user one), and the tag is the second line of defence for a
 * folder that is shared anyway, a --savedir handed to both. The game is Tiberian Dawn until
 * ds_set_game says otherwise, so every caller that predates it behaves as before. */
#define DS_GAME_TD 0
#define DS_GAME_RA 1
void ds_set_game(int game);
int ds_get_game(void);
/* Is this record a slot of the running game? Lists, loads and the slot dialog offer only
 * these; another game's slot keeps its number and is never written over by
 * ds_first_free, because it is still somebody's save. */
int ds_slot_mine(const DS_Slot *s);

/* Where the two files live. Called once by the shell with SDL_GetPrefPath's answer, or
 * with --savedir so a gate owns a scratch directory and never touches the player's.
 * A NULL or empty directory degrades to "no persistence", the same way the hall of fame
 * does, rather than writing into the working directory by accident. */
void ds_set_dir(const char *dir);
const char *ds_get_dir(void);

/* "<dir>/SAVEGAME.%03d", absolute. CDFileClass::Open short-circuits on an absolute path
 * for both read and write (cdfile.cpp:485-517), so this is exact and portable and the
 * engine's own user-path redirection never has to be reasoned about. */
const char *ds_payload_path(int slot);

/* The index. Both return the number of live records, or -1 when there is no directory. */
int ds_read_index(DS_Slot out[DS_SLOTS]);
int ds_write_index(const DS_Slot in[DS_SLOTS]);

/* Lowest unused slot, or -1 when all DS_SLOTS are taken. */
int ds_first_free(const DS_Slot t[DS_SLOTS]);

/* Does slot's payload file actually exist? */
int ds_payload_exists(int slot);

/* The payload's modification time (seconds), or 0 when it cannot be read. */
long ds_payload_mtime(int slot);

/* Delete a slot: the payload, the ".enh" sidecar beside it if there is one, and its
   index record. Returns 1 when the payload is gone afterwards (a payload that was not
   there counts), 0 when the directory is unset or the file could not be removed. */
int ds_delete(int slot);

/* WHICH KIND OF BUILD WROTE A PAYLOAD. The engine's save begins with one 32-bit word,
 * little-endian, at offset 0 of SAVEGAME.nnn: CNC_DLL_API_VERSION (0x102), written before
 * anything else so that a load can look at it first. A 64-bit Windows build sets a width
 * tag in bits 16-23 of that word, because its payload holds the engine's objects at that
 * build's own pointer and long widths, and it refuses a payload whose word does not carry
 * the tag before it touches the running game. Every other build (the Mac, and every
 * Windows build before the 64-bit one) writes the bare word and still loads whatever word
 * it is given, exactly as it always has.
 *
 * The 128-byte index record has no spare byte, so the payload's first word is the one
 * place the width can be read from. The two values are the engine's own,
 * OPENCNC3D_SAVE_WIDTH_MASK and OPENCNC3D_SAVE_WIDTH_WIN64 in dllinterfaceversion.h; this
 * file includes no engine header, so they are restated here under a DS_ prefix, and
 * cnc_eyes.cpp, which sees both, asserts at compile time that the copies agree. */
#define DS_SAVE_WIDTH_MASK  0x00FF0000u
#define DS_SAVE_WIDTH_WIN64 0x00640000u

/* Should a host of this kind offer its engine a payload that begins with first_word? win64
   is 1 for a 64-bit Windows build and 0 for every other. A 64-bit Windows host offers only
   a tagged word, which is all its engine accepts; any other host offers only an untagged
   one, which is every word it has ever written. That second half is the host's policy,
   not the engine's: a Mac or 32-bit engine checks no width and would load a tagged word
   too. Pure: no file, no state. */
int ds_word_loadable(unsigned int first_word, int win64);

/* The first four bytes of slot's payload as a little-endian word, into *word. Returns 1, or
   0 with *word untouched when there is no save directory, no payload, or a payload shorter
   than four bytes. */
int ds_payload_word(int slot, unsigned int *word);

/* WHICH KIND OF BUILD THIS IS, the win64 every production caller passes: 1 in a 64-bit
   Windows build, 0 in every other. A gate passes either value to see both answers. */
#if defined(_WIN64)
#define DS_HOST_WIN64 1
#else
#define DS_HOST_WIN64 0
#endif

/* Can a host of this kind load slot's payload? 0 when the payload's first word says the
   other kind of build wrote it (ds_word_loadable), 1 otherwise. A payload whose word cannot
   be read is not this test's to refuse: 1, and the engine answers for it as before. Every
   way a slot is listed or loaded asks this first, so such a payload is never handed to the
   engine and a running mission is never left for it. Nothing is deleted or rewritten: the
   slot stays listed, and saving over it is an ordinary save. */
int ds_slot_loadable(int slot, int win64);

/* What the player is told about such a slot, one definition each, shared by the renderer's
   slot dialog and the shell. The label stands where the side would in the row's "(...)"
   prefix; the caption and the sentence make the notice a refused load shows. Each says
   what is true for the host asking: a 64-bit Windows build cannot load a save without the
   mark, which a 32-bit Windows build or a Mac made, and any other build cannot load one
   with it, which only a 64-bit Windows build makes. */
const char *ds_width_label(int win64);
const char *ds_width_refusal(int win64);
#define DS_WIDTH_CAPTION "Unable to load mission"

#ifdef __cplusplus
}
#endif

#endif /* DOSSAVE_H */
