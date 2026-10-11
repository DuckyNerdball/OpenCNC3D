/*
 * lsetup_cli.h -- the launcher's headless setup modes, for the Windows web installer
 * and for the tests. No window is opened by any of them.
 *
 *   --media-install movies,music|movies|music|none  --dir DIR
 *         [--media-manifest FILE] [--progress-file F] [--cancel-file F]
 *
 *       Make DIR's movies and music match the choice (launcher/lmedia.h): fetch what is
 *       ticked and missing, remove what is unticked, write opencnc3d-media.txt.
 *
 *   --install-game --dir DIR [--build 0.8.1] [--media movies,music|movies|music|none]
 *         [--media-manifest FILE] [--progress-file F] [--cancel-file F]
 *
 *       Install the game into DIR, which may hold nothing but this launcher: ask the
 *       site for the newest build for this platform (the same /api/builds an update
 *       reads), download its full package with resume, check it, unpack it over the
 *       folder with the updater's own extractor (which replaces this very launcher if
 *       the package carries a newer one), and then, when --media is given, run the
 *       movies and music job. Without --media the movies and music are not touched and
 *       no choice is recorded, so the launcher's first start asks. A Stop that arrives
 *       once the game is in, while it is being unpacked or after, ends the setup there
 *       as cancelled: the movies and music job is never started, and no choice is
 *       recorded.
 *
 *   --fetch-menu-art --dir DIR [--progress-file F] [--cancel-file F]
 *
 *       Put the newest build's menu art, dosmenu.pack, into DIR, a folder holding only
 *       this launcher, so that the launcher can draw the screen offering to install the
 *       game there (lupdate.h, lu_fetch_pack). A pack already there is left alone.
 *
 * THE PROGRESS FILE, rewritten about four times a second, always whole (a new copy
 * renamed over the old), in this order:
 *
 *   state=fetching|checking|removing|done|failed|cancelled
 *   pct=<0..100, never lower than the last one written>
 *   text=<"312.4 MB of 1.9 GB (16%)", whose two sizes say the same percentage>
 *   item=<"GAME: OpenCNC3D-windows-v0.8.0.zip" or "MOVIES: GDI1.VQA (12 OF 75)">
 *   beat=<one more than the last one written>
 *   reason=<empty, or one sentence saying why it failed or stopped>
 *   end
 *
 * A reader that does not see "end" last has read a file that was still being
 * replaced, and reads again. THE CANCEL FILE is polled as often: once it exists the job
 * stops at the next chance, keeping what arrived.
 *
 * EXIT CODES: 0 done, 1 failed, 2 cancelled, 3 nothing could be reached (offline).
 *
 * --stall-seconds N is for the tests: how long a transfer may stand still before it is
 * tried again (30 unless given).
 */

#ifndef LSETUP_CLI_H
#define LSETUP_CLI_H

/* Called first thing in main(). When the command line asks for one of the modes
 * above, runs it, sets *handled to 1 and returns the exit code; otherwise sets
 * *handled to 0 and returns 0, having done nothing at all. */
int lsetup_cli_main(int argc, char **argv, int *handled);

#endif /* LSETUP_CLI_H */
