/*
 * userdir.h -- the name of the per-user folder.
 *
 * Saves, the halls of fame, the log when the game folder is read-only, and the
 * Windows world-dump scratch file all live in the folder SDL_GetPrefPath answers for
 * this pair. Every call names it through these macros, so there is one spelling and
 * no second copy to drift:
 *
 *     SDL_GetPrefPath(OPENCNC3D_PREF_ORG, OPENCNC3D_PREF_APP)
 *
 * The organisation is empty, so SDL puts the folder straight under the platform's
 * per-user data folder with no organisation folder above it:
 *
 *     Windows   %APPDATA%\OpenCNC3D\
 *     macOS     ~/Library/Application Support/OpenCNC3D/
 *     Linux     ~/.local/share/OpenCNC3D/
 *
 * SDL creates the folder as a side effect of asking, and the answer always ends in a
 * separator. An empty string rather than NULL says what is meant; SDL treats the two
 * alike.
 *
 * EARLIER BUILDS kept the same files one level deeper, under an organisation folder
 * named OPENCNC3D_PREF_ORG_EARLIER (and, before that, in a "CNC3D" folder beside the
 * "OpenCNC3D" one there). The game copies a player's saves and halls of fame out of
 * those folders once (userdir_carry_over in app/opencnc3d.cpp) and never writes to
 * them. That name is spelled here only, and must never be handed to SDL_GetPrefPath:
 * asking would create the folder for every player who never had one.
 *
 * Plain C, no declarations: included by C and C++ files alike.
 */
#ifndef OPENCNC3D_USERDIR_H
#define OPENCNC3D_USERDIR_H

#define OPENCNC3D_PREF_ORG ""
#define OPENCNC3D_PREF_APP "OpenCNC3D"

#define OPENCNC3D_PREF_ORG_EARLIER "Slipgate Ironworks"

#endif /* OPENCNC3D_USERDIR_H */
