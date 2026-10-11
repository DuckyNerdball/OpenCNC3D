/*
 * userdir.h -- the name of the per-user folder.
 *
 * Saves, the halls of fame, the log when the game folder is read-only, and the
 * Windows world-dump scratch file all live in the folder SDL_GetPrefPath answers for
 * this pair. Every call names it through these two macros, so there is one spelling
 * and no second copy to drift:
 *
 *     SDL_GetPrefPath(OPENCNC3D_PREF_ORG, OPENCNC3D_PREF_APP)
 *
 * which on macOS is ~/Library/Application Support/Slipgate Ironworks/OpenCNC3D/
 * (measured), and by SDL's own rules %APPDATA%\Slipgate Ironworks\OpenCNC3D\ on
 * Windows and ~/.local/share/Slipgate Ironworks/OpenCNC3D/ on Linux. SDL creates the
 * folder as a side effect of asking, and the answer always ends in a separator.
 *
 * Plain C, no declarations: included by C and C++ files alike.
 */
#ifndef OPENCNC3D_USERDIR_H
#define OPENCNC3D_USERDIR_H

#define OPENCNC3D_PREF_ORG "Slipgate Ironworks"
#define OPENCNC3D_PREF_APP "OpenCNC3D"

#endif /* OPENCNC3D_USERDIR_H */
