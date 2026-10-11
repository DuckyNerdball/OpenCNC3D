; opencnc3d.nsi -- OpenCNC3D Setup, the Windows install wizard.
;
; A WEB INSTALLER. Setup carries the launcher and nothing of the game: it installs
; OpenCNC3D.exe and the few files it needs to run, then has that launcher download the
; newest Windows package from the site and the movies and music the player ticked, on
; one progress page. tools/win/make-installer-win.sh is the wrapper that passes the
; defines, and it builds this from the launcher's files alone.
;
; WHY IT CANNOT CARRY THE GAME. An NSIS installer holds at most 2 GB, because its data
; block is addressed with signed 32-bit offsets, and the game is larger than that. Past
; the line makensis does not refuse; it dies part way through the file list. So the game
; comes down the wire, which it already did for every update.
;
; WHY THE LAUNCHER DOES THE DOWNLOADING AND SETUP ONLY DRAWS IT. The launcher already
; finds the newest build, downloads it, checks it and unpacks it over an install,
; replacing itself on the way, and the same program fetches the movies and music from
; the freeware disc images. Writing that again in NSIS script would be a second
; implementation that cannot be tested off Windows, in a language whose integers are 32
; bits wide. So Setup starts the launcher in its headless install mode and reads the
; small progress file the launcher rewrites a few times a second:
;
;     state=fetching|checking|removing|done|failed|cancelled
;     pct=<0..100, never decreasing>
;     text=<"312.4 MB of 1.9 GB (16%)">
;     item=<"GAME: OpenCNC3D-windows-v0.8.0.zip" or "MOVIES: GDI1.VQA (12 OF 75)">
;     beat=<a counter, one higher on every write>
;     reason=<empty, or one sentence saying what went wrong>
;     end
;
; A read whose last line is not "end" is ignored, so a half-written file is never shown.
; Every number on the page is the launcher's own; Setup does no arithmetic on sizes.
;
; IT INSTALLS PER USER, NOT INTO PROGRAM FILES, and that is the load-bearing decision
; about where the files go.
;
;   - The launcher UPDATES THE GAME IN PLACE: it downloads a zip and unpacks it over the
;     install, and it is also what does the first download here. In Program Files that
;     write is denied unless the launcher runs elevated, so a Program Files install would
;     mean a UAC prompt on every update, or a launcher that asks for administrator rights
;     every time it starts. Neither is acceptable for a game.
;   - The game writes its settings and its log beside itself. Same problem, and Windows
;     would silently redirect those writes into VirtualStore, where the next build would
;     not find them.
;   - A per-user install needs no administrator rights at all, so the whole UAC prompt
;     disappears from the experience.
;
; $LOCALAPPDATA\Programs\<app> is where Windows itself puts per-user applications and
; where every other per-user installer goes. The directory page still lets anyone put it
; wherever they like.
;
; ON SMARTSCREEN. This installer is unsigned, so Windows will show "Windows protected
; your PC" the first time. That is an absence of reputation and not a detection;
; docs/windows-signing.md has the research and the costed options. The README Setup
; installs tells the player the one step that gets past it.
;
; SILENT INSTALLS. /S installs with both downloads; /NOMOVIES and /NOMUSIC leave one out
; (they also untick the boxes in an interactive run), and /D=<folder> picks the folder.
; A silent Setup exits 0 when the game and every chosen download are installed, and
; otherwise 11 when the download failed, 12 when it was cancelled, 13 when no download
; server could be reached, 14 when the launcher could not be started, and 19 when the
; launcher stopped in some other way: 10 more than the launcher's own exit codes, which
; keeps them clear of the 1 and 2 NSIS itself returns for a cancelled or aborted Setup.
; 15 means the folder given was the root of a drive or of a network share, which Setup
; refuses (InstDirIsRoot has why), and nothing was installed.

; ANSI, NOT UNICODE, AND IT IS NOT A PREFERENCE.
;
; The Homebrew makensis bottles for macOS (3.12, and 3.13 still) crash with
; std::bad_alloc while writing ANY unicode installer, including a three line one that
; installs nothing:
;
;   $ printf 'OutFile "m.exe"\nSection\nSectionEnd\n' > m.nsi && makensis m.nsi
;   Processed 1 file, writing output (x86-unicode):
;   libc++abi: terminating due to uncaught exception of type std::bad_alloc
;
; The same script with the ANSI target builds and runs. So this line is a workaround for
; a broken toolchain, not a decision about what the installer should be, and it has one
; real consequence: an ANSI installer resolves paths through the system codepage, so a
; Windows account whose NAME contains characters outside that codepage would get a
; mangled install path. Everything ASCII, which is every path this installer chooses for
; itself, is unaffected. The progress file is shown through the same codepage, so text in
; it beyond ASCII would be drawn wrongly, and the launcher writes it in ASCII.
;
; Flip this back to `Unicode true` the moment makensis can write one.
Unicode false

!ifndef PAYLOAD
  !error "PAYLOAD is not defined. Run tools/win/make-installer-win.sh, which passes it."
!endif
!ifndef VERSION
  !error "VERSION is not defined. Run tools/win/make-installer-win.sh."
!endif
!ifndef OUTFILE
  !error "OUTFILE is not defined. Run tools/win/make-installer-win.sh."
!endif
!ifndef ICONFILE
  !error "ICONFILE is not defined. Run tools/win/make-installer-win.sh."
!endif
!ifndef WIZARDBMP
  !error "WIZARDBMP is not defined. Run tools/win/make-installer-win.sh."
!endif
; The sizes beside the two boxes. make-installer-win.sh reads them from the media manifest
; the launcher carries, so they are what the launcher will download.
!ifndef MOVIES_SIZE
  !error "MOVIES_SIZE is not defined. Run tools/win/make-installer-win.sh."
!endif
!ifndef MOVIES_KB
  !error "MOVIES_KB is not defined. Run tools/win/make-installer-win.sh."
!endif
!ifndef MUSIC_SIZE
  !error "MUSIC_SIZE is not defined. Run tools/win/make-installer-win.sh."
!endif
!ifndef MUSIC_KB
  !error "MUSIC_KB is not defined. Run tools/win/make-installer-win.sh."
!endif
; The installed size of the game itself, when the wrapper was given the staged package to
; measure (GAME_KB and GAME_SIZE together). Without it the space line on the directory
; page counts only the launcher and the media, and the welcome page gives no size.
!ifndef GAME_KB
  !define GAME_KB 0
!endif

!define APPNAME    "OpenCNC3D"
!define COMPANY    "Slipgate Ironworks"
!define REGKEY     "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenCNC3D"
!define LAUNCHER   "OpenCNC3D.exe"
; Polled four times a second, so 120 quiet polls are the thirty seconds after which a
; launcher that has stopped writing is taken to have stopped working.
!define DL_TICK_MS     250
!define DL_STALL_TICKS 120
; HOW LONG A STOPPED LAUNCHER IS GIVEN TO END BY ITSELF before Setup ends it: 45 seconds,
; as 180 polls on the download page and as one wait when Setup is cancelled. It has to be
; longer than the 30 second receive timeout of the launcher's WinINet reads, because a
; read that is waiting on a quiet server only returns when that timeout runs out, and the
; launcher can only see the cancel file, keep what has arrived and exit after that. Ended
; any sooner, it loses the piece it was in the middle of.
!define DL_STOP_TICKS  180
!define DL_STOP_MS     45000

; The name, not the number: Setup installs whatever build is newest when it runs, which
; may be later than the Setup that fetched it. Setup's own number is in the branding line
; and the file name.
Name "${APPNAME}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\OpenCNC3D"
; A reinstall or an upgrade finds the previous folder rather than proposing the default
; again, so a player who moved it once does not have to move it twice.
InstallDirRegKey HKCU "Software\OpenCNC3D" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "${APPNAME} Setup ${VERSION}"

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "TextFunc.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "nsDialogs.nsh"
!include "WinMessages.nsh"

; ---------------------------------------------------------------- state

Var MediaArg        ; what --media is given: movies,music | movies | music | none
Var MediaWords      ; the same as a list, ", the movies and the music"
Var MediaWith       ; and as a result, ", with the movies and the music"
Var Installed       ; 1 once the files page has run
Var SetupMadeDir    ; 1 when the install folder was absent or empty before Setup
Var Registered      ; 1 once the shortcuts and the Add/Remove Programs entry exist
Var Playable        ; 1 when the folder holds a game, so Next may lead to the end
Var GameVersion     ; read back from the install record the download wrote
Var FinishText

Var DlDialog
Var DlHeading
Var DlHeadFont
Var DlItem
Var DlBar
Var DlSize
Var DlNote
Var DlStop
Var DlRetry
Var DlProc          ; the launcher's process handle while it runs, 0 when none
Var DlRunning
Var DlExit          ; its exit code, or "stall" or "nostart", once it has ended
Var DlQuiet         ; polls since the beat last moved
Var DlLastBeat
Var DlPctShown
Var DlItemShown
Var DlTextShown
Var DlOk            ; 1 when the last read of the progress file was complete
Var DlRState
Var DlRPct
Var DlRText
Var DlRItem
Var DlRBeat
Var DlRReason
Var DlStopping      ; polls since Stop was pressed, -1 while it has not been

; ---------------------------------------------------------------- pages

!define MUI_ICON   "${ICONFILE}"
!define MUI_UNICON "${ICONFILE}"
; The game itself down the left of the welcome and finish pages, rather than the stock
; installer picture.
!define MUI_WELCOMEFINISHPAGE_BITMAP   "${WIZARDBMP}"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${WIZARDBMP}"
!define MUI_ABORTWARNING
; Cancel while the launcher is downloading stops it before Setup closes, and a fresh
; folder that never received the game is taken away again.
!define MUI_CUSTOMFUNCTION_ABORT OnSetupAbort

!if ${GAME_KB} > 0
  !define WELCOME_SIZE " The game takes about ${GAME_SIZE} once installed, the movies ${MOVIES_SIZE} and the music ${MUSIC_SIZE}."
!else
  !define WELCOME_SIZE ""
!endif
!define MUI_WELCOMEPAGE_TEXT "Setup puts the OpenCNC3D launcher on this computer, and the launcher then downloads the Windows build of the game for this version, ${VERSION}, into the same folder, with the 1995 movies and music if you want them.$\r$\n$\r$\nEverything comes over the internet, so keep the connection up until Setup has finished.${WELCOME_SIZE} Nothing needs administrator rights.$\r$\n$\r$\nClick Next to continue."
!insertmacro MUI_PAGE_WELCOME

; The movies and the music are the two boxes this page exists for, and both start ticked.
; The game itself is drawn ticked and grey: it is what Setup is for.
!define MUI_COMPONENTSPAGE_TEXT_TOP "Untick the movies or the music to install without them. The gear button in the launcher adds or removes them at any time."
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
; Installs the launcher only, which takes a moment; the finish page's auto close moves
; straight on to the download.
!insertmacro MUI_PAGE_INSTFILES
Page custom DlPageCreate DlPageLeave

!define MUI_PAGE_CUSTOMFUNCTION_SHOW FinishShow
!define MUI_FINISHPAGE_TEXT "$FinishText"
; The tall text box: with both checkboxes drawn, the ordinary one has room for five lines.
!define MUI_FINISHPAGE_TEXT_LARGE
; The finish page offers to start the launcher, which is the thing this whole installer
; exists to put on the machine.
!define MUI_FINISHPAGE_RUN "$INSTDIR\${LAUNCHER}"
!define MUI_FINISHPAGE_RUN_TEXT "Start OpenCNC3D"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\READ-ME-WINDOWS.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Read the notes for this build"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; One settings file from another folder, copied in under this install's name. CopyFile's
; last argument refuses to replace a file already there, so a setting this install has of
; its own always wins. LogicLib rather than a relative jump, because a plugin call
; compiles to several instructions and a "+2" would land inside it.
!macro CarrySetting FROMDIR WHICH
  ${If} ${FileExists} "${FROMDIR}\cnc3d-${WHICH}.cfg"
    System::Call 'kernel32::CopyFile(t "${FROMDIR}\cnc3d-${WHICH}.cfg", t "$INSTDIR\opencnc3d-${WHICH}.cfg", i 1) i .r1'
  ${EndIf}
!macroend

; ---------------------------------------------------------------- install

Section "OpenCNC3D (the game)" SecMain
  SectionIn RO
  !if ${GAME_KB} > 0
    AddSize ${GAME_KB}
  !endif

  ; NEVER THE ROOT OF A DRIVE OR A SHARE. The directory page cannot offer one
  ; (.onVerifyInstDir), so this is for a silent install's /D=, asked before anything is
  ; written. Then any trailing backslash goes, so the folder reaches the launcher's
  ; command line without one.
  Call InstDirIsRoot
  Pop $0
  ${If} $0 == 1
    MessageBox MB_ICONSTOP "OpenCNC3D cannot be installed straight into $INSTDIR. Choose a folder on that drive instead." /SD IDOK
    SetErrorLevel 15
    Quit
  ${EndIf}
  ${Do}
    StrCpy $0 "$INSTDIR" "" -1
    ${If} $0 != "\"
      ${ExitDo}
    ${EndIf}
    StrCpy $INSTDIR "$INSTDIR" -1
  ${Loop}

  ; WHETHER THE FOLDER IS SETUP'S OWN, asked before anything is written into it. Only a
  ; folder that was absent or empty may be taken away again if Setup ends before the
  ; game arrives; a folder that already held anything keeps whatever it held.
  StrCpy $SetupMadeDir 1
  ${If} ${FileExists} "$INSTDIR\*.*"
    FindFirst $0 $1 "$INSTDIR\*.*"
    ${DoWhile} $1 != ""
      ${If} $1 != "."
      ${AndIf} $1 != ".."
        StrCpy $SetupMadeDir 0
        ${ExitDo}
      ${EndIf}
      FindNext $0 $1
    ${Loop}
    FindClose $0
  ${EndIf}

  SetOutPath "$INSTDIR"

  ; AN UPDATE THE LAUNCHER DID NOT FINISH IS ABANDONED BEFORE ANY FILE GOES IN.
  ; The launcher finishes such an update when it next starts, by putting back every
  ; file named in opencnc3d-update.journal, and it cannot tell a file this installer
  ; has just written from one that update wrote. Left in place, the journal would
  ; undo part of this install the first time the launcher ran, starting with the
  ; launcher Setup is about to put down. Renamed to opencnc3d-update.done instead, the
  ; launcher reads it as an update that is over: it puts nothing back, and removes only
  ; the old and half-written copies the journal's lines record that update making, by
  ; those exact names. The launcher never looks for such copies by any other means. If
  ; that name is still taken by an earlier one, the journal is removed instead, and
  ; those copies stay. The package the launcher then downloads is laid over the whole
  ; folder, so nothing that update left half done survives it.
  Rename "$INSTDIR\opencnc3d-update.journal" "$INSTDIR\opencnc3d-update.done"
  Delete "$INSTDIR\opencnc3d-update.journal"

  ; The launcher and what it needs to run: OpenCNC3D.exe, SDL2.dll, the icon, the notes,
  ; the licence and the notice of modification. Nothing of the game; the wrapper refuses a
  ; payload with anything else in it.
  File /r "${PAYLOAD}/*"

  ; Settings from an install in an earlier folder, copied when this one has none of its
  ; own. Copied, never moved, and nothing else there is touched.
  ReadRegStr $0 HKCU "Software\CNC3D" "InstallDir"
  ${If} $0 != ""
    !insertmacro CarrySetting "$0" "fx"
    !insertmacro CarrySetting "$0" "controls"
    !insertmacro CarrySetting "$0" "multiplayer"
  ${EndIf}

  ; The uninstaller goes in with the launcher, so a folder that holds anything of Setup's
  ; can always be removed the ordinary way. The shortcuts and the Add/Remove Programs
  ; entry wait until there is a game for them to start (RegisterInstall).
  WriteUninstaller "$INSTDIR\uninstall.exe"
  StrCpy $Installed 1
SectionEnd

; The two downloads. Neither carries /o, so both start ticked, and neither has anything to
; install here: the launcher fetches them on the download page, and these sections only
; carry the choice and the space it needs. make-installer-win.sh refuses a script in which
; either has gained a /o.
Section "Movies (download, ${MOVIES_SIZE})" SecMovies
  AddSize ${MOVIES_KB}
SectionEnd

Section "Music (download, ${MUSIC_SIZE})" SecMusic
  AddSize ${MUSIC_KB}
SectionEnd

; A SILENT INSTALL SHOWS NO PAGES, the download page included, so the download happens
; here instead, waited for, with the result handed back as Setup's exit code.
Section "-download when silent" SecSilent
  ${If} ${Silent}
    Call MediaChoice
    ClearErrors
    ExecWait '"$INSTDIR\${LAUNCHER}" --dir "$INSTDIR" --install-game --build ${VERSION} --media $MediaArg' $0
    ${If} ${Errors}
      SetErrorLevel 14
    ${ElseIf} $0 == 1
      SetErrorLevel 11
    ${ElseIf} $0 == 2
      SetErrorLevel 12
    ${ElseIf} $0 == 3
      SetErrorLevel 13
    ${ElseIf} $0 != 0
      SetErrorLevel 19
    ${EndIf}
    ${If} ${FileExists} "$INSTDIR\opencnc3d-install.txt"
      Call RegisterInstall
    ${Else}
      Call TakeBackFreshFolder
    ${EndIf}
  ${EndIf}
SectionEnd

LangString DESC_SecMain   ${LANG_ENGLISH} \
  "The game, and the launcher that keeps it up to date. Setup installs the launcher, and the launcher downloads the newest build of the game."
LangString DESC_SecMovies ${LANG_ENGLISH} \
  "The 1995 cutscenes, ${MOVIES_SIZE}, downloaded while Setup runs, from the 1995 Gold discs (EA's 2007 freeware release). Without them the game plays with no cutscenes."
LangString DESC_SecMusic  ${LANG_ENGLISH} \
  "The 1995 soundtrack, ${MUSIC_SIZE}, downloaded while Setup runs, from the Covert Operations disc. Without it the game plays with no music."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain}   $(DESC_SecMain)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMovies} $(DESC_SecMovies)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMusic}  $(DESC_SecMusic)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---------------------------------------------------------------- functions

Function .onInit
  InitPluginsDir
  StrCpy $DlProc 0
  StrCpy $DlRunning 0
  StrCpy $DlStopping -1
  StrCpy $Installed 0
  StrCpy $SetupMadeDir 0
  StrCpy $Registered 0
  StrCpy $Playable 0
  StrCpy $FinishText "OpenCNC3D is installed."
  ; /NOMOVIES and /NOMUSIC untick the boxes, which is how a silent install says no.
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/NOMOVIES" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SecMovies}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/NOMUSIC" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SecMusic}
  ${EndIf}
FunctionEnd

; THE INSTALL FOLDER IS NEVER THE ROOT OF A DRIVE OR OF A NETWORK SHARE, for two reasons.
; The uninstaller, and Setup itself after a cancelled first install, remove the whole
; folder. And a root keeps its backslash ("D:\"), which, inside the quotes Setup puts
; around --dir, the C runtime reading the launcher's command line takes as an escaped
; quote: --install-game and everything after it is swallowed into the folder name, and the
; launcher opens its window instead of installing. A root cannot lose that backslash ("D:"
; alone means the current folder of drive D), so a root is refused; any other folder has a
; trailing backslash taken off in SecMain before the launcher is started.
;
; Leaves 1 on the stack for a root (or an empty path), 0 for anything else, and changes
; no register. GetRoot gives "D:" for anything on drive D and "\\server\share" for a path
; below a share; a share path with nothing below the share has no root it can name.
Function InstDirIsRoot
  Push $0
  Push $1
  Push $2
  StrCpy $0 "$INSTDIR"
  ${Do}
    StrCpy $1 "$0" "" -1
    ${If} $1 != "\"
    ${AndIf} $1 != "/"
      ${ExitDo}
    ${EndIf}
    StrCpy $0 "$0" -1
  ${Loop}
  ${GetRoot} "$0" $1
  StrCpy $2 "$0" 2
  ${If} $0 == ""
  ${OrIf} $0 == $1
    StrCpy $0 1
  ${ElseIf} $1 == ""
  ${AndIf} $2 == "\\"
    StrCpy $0 1
  ${Else}
    StrCpy $0 0
  ${EndIf}
  Pop $2
  Pop $1
  Exch $0
FunctionEnd

; The directory page's Next stays grey for a root.
Function .onVerifyInstDir
  Push $0
  Call InstDirIsRoot
  Pop $0
  ${If} $0 == 1
    Pop $0
    Abort
  ${EndIf}
  Pop $0
FunctionEnd

; The two boxes, as the launcher's --media argument and as words for the page.
Function MediaChoice
  ${If} ${SectionIsSelected} ${SecMovies}
  ${AndIf} ${SectionIsSelected} ${SecMusic}
    StrCpy $MediaArg "movies,music"
    StrCpy $MediaWords ", the movies and the music"
    StrCpy $MediaWith ", with the movies and the music"
  ${ElseIf} ${SectionIsSelected} ${SecMovies}
    StrCpy $MediaArg "movies"
    StrCpy $MediaWords " and the movies"
    StrCpy $MediaWith ", with the movies"
  ${ElseIf} ${SectionIsSelected} ${SecMusic}
    StrCpy $MediaArg "music"
    StrCpy $MediaWords " and the music"
    StrCpy $MediaWith ", with the music"
  ${Else}
    ; "none" is still passed: on a reinstall over a folder that has movies or music,
    ; unticking both is the player asking for them to go, and the launcher removes them.
    StrCpy $MediaArg "none"
    StrCpy $MediaWords ""
    StrCpy $MediaWith ", without the movies and the music"
  ${EndIf}
FunctionEnd

; The version the folder now holds, from the install record the package carries, which can
; be later than this Setup. Setup's own number when there is no record.
Function ReadGameVersion
  StrCpy $GameVersion "${VERSION}"
  ClearErrors
  FileOpen $0 "$INSTDIR\opencnc3d-install.txt" r
  ${IfNot} ${Errors}
    ${Do}
      ClearErrors
      FileRead $0 $1
      ${IfThen} ${Errors} ${|} ${ExitDo} ${|}
      ${TrimNewLines} "$1" $1
      StrCpy $2 $1 8
      ${If} $2 == "version "
        StrCpy $GameVersion $1 "" 8
        ${ExitDo}
      ${EndIf}
    ${Loop}
    FileClose $0
  ${EndIf}
FunctionEnd

; Add/Remove Programs' size, read back off the folder rather than guessed, so the entry
; does not claim a few megabytes for a game of several gigabytes.
Function WriteEstimatedSize
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${REGKEY}" "EstimatedSize" "$0"
FunctionEnd

; THE SHORTCUTS AND THE UNINSTALL ENTRY, written once the folder holds a game. The Start
; Menu and desktop entries point at the LAUNCHER, because that is the thing to
; double-click: it shows which build is installed, offers the update, and starts the
; game. opencnc3d-game.exe beside it is the engine and is not a shortcut anybody should
; be given.
Function RegisterInstall
  ${If} $Registered == 1
    Return
  ${EndIf}
  StrCpy $Registered 1
  ; The working folder every shortcut records is the output folder.
  SetOutPath "$INSTDIR"
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\${LAUNCHER}" \
                 "" "$INSTDIR\opencnc3d.ico" 0
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${LAUNCHER}" "" "$INSTDIR\opencnc3d.ico" 0

  WriteRegStr HKCU "Software\OpenCNC3D" "InstallDir" "$INSTDIR"
  Call ReadGameVersion
  WriteRegStr HKCU "${REGKEY}" "DisplayName"     "${APPNAME}"
  WriteRegStr HKCU "${REGKEY}" "DisplayVersion"  "$GameVersion"
  WriteRegStr HKCU "${REGKEY}" "Publisher"       "${COMPANY}"
  WriteRegStr HKCU "${REGKEY}" "DisplayIcon"     "$INSTDIR\opencnc3d.ico"
  WriteRegStr HKCU "${REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegDWORD HKCU "${REGKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${REGKEY}" "NoRepair" 1
  Call WriteEstimatedSize
FunctionEnd

; A FRESH FOLDER THE GAME NEVER REACHED IS TAKEN AWAY AGAIN, so a cancelled or failed
; first install leaves nothing behind: not a launcher with no game, not a part-finished
; download. Only a folder Setup found absent or empty, only when it holds no install
; record, and only when the launcher is in it, which marks it as the folder this Setup
; wrote into.
Function TakeBackFreshFolder
  ${If} $SetupMadeDir == 1
  ${AndIfNot} ${FileExists} "$INSTDIR\opencnc3d-install.txt"
  ${AndIf} ${FileExists} "$INSTDIR\${LAUNCHER}"
    ; A process cannot remove the folder it is standing in.
    SetOutPath "$TEMP"
    RMDir /r "$INSTDIR"
  ${EndIf}
FunctionEnd

; Asks a running launcher to stop and waits for it, then makes it stop. The cancel file
; is what it watches; it lives in Setup's own temporary folder, which is deleted when
; Setup exits, so Setup must not leave before the launcher has seen it. The wait is
; DL_STOP_MS, longer than one of the launcher's network reads can block (the note at
; DL_STOP_TICKS), and the page says what it is waiting for first, since the window does
; not redraw while it waits.
Function DlStopLauncher
  ${If} $DlProc == 0
  ${OrIf} $DlProc == ""
    Return
  ${EndIf}
  FileOpen $0 "$PLUGINSDIR\c.txt" w
  FileWrite $0 "stop$\r$\n"
  FileClose $0
  ${If} $DlRunning == 1
    ${NSD_SetText} $DlNote "Stopping the download. This can take up to a minute."
    System::Call 'user32::UpdateWindow(i $DlNote) i .r0'
  ${EndIf}
  System::Call 'kernel32::WaitForSingleObject(i $DlProc, i ${DL_STOP_MS}) i .r0'
  ${If} $0 != 0
    System::Call 'kernel32::TerminateProcess(i $DlProc, i 2) i .r0'
    System::Call 'kernel32::WaitForSingleObject(i $DlProc, i 5000) i .r0'
  ${EndIf}
  System::Call 'kernel32::CloseHandle(i $DlProc) i .r0'
  StrCpy $DlProc 0
  StrCpy $DlRunning 0
FunctionEnd

Function OnSetupAbort
  ${If} $Installed != 1
    Return
  ${EndIf}
  Call DlStopLauncher
  ${If} ${FileExists} "$INSTDIR\opencnc3d-install.txt"
    ; The folder can be played, so it can be found and removed like any other install.
    Call RegisterInstall
  ${Else}
    Call TakeBackFreshFolder
  ${EndIf}
FunctionEnd

Function FinishShow
  ; The install cannot be gone back into.
  GetDlgItem $0 $HWNDPARENT 3
  EnableWindow $0 0
FunctionEnd

; ------------------------------------------------- the download page

; Starts the launcher in its install mode, without waiting. CreateProcess rather than
; Exec because Setup needs the process handle: a launcher that has exited is how a
; crash, or a run that ended before its last write, is told from one still working.
Function DlStart
  Delete "$PLUGINSDIR\p.txt"
  Delete "$PLUGINSDIR\c.txt"
  StrCpy $DlQuiet 0
  StrCpy $DlLastBeat ""
  StrCpy $DlPctShown 0
  StrCpy $DlItemShown ""
  StrCpy $DlTextShown ""
  StrCpy $DlRState ""
  StrCpy $DlRReason ""
  StrCpy $DlExit ""
  StrCpy $DlStopping -1
  SendMessage $DlBar ${PBM_SETPOS} 0 0
  ${NSD_SetText} $DlItem "Starting the download..."
  ${NSD_SetText} $DlSize ""
  ${NSD_SetText} $DlNote ""

  StrCpy $0 '"$INSTDIR\${LAUNCHER}" --dir "$INSTDIR" --install-game --build ${VERSION} --media $MediaArg --progress-file "$PLUGINSDIR\p.txt" --cancel-file "$PLUGINSDIR\c.txt"'
  ; STARTUPINFOA is 68 bytes on 32-bit Windows: its own size, then sixteen zeroed fields.
  System::Call '*(i 68, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0, i 0) i .r1'
  ; PROCESS_INFORMATION: the process and thread handles, then their ids.
  System::Call '*(i 0, i 0, i 0, i 0) i .r2'
  System::Call 'kernel32::CreateProcess(i 0, t r0, i 0, i 0, i 0, i 0, i 0, t "$INSTDIR", i r1, i r2) i .r3 ?e'
  Pop $4
  ${If} $3 == 0
    StrCpy $DlProc 0
    StrCpy $DlExit "nostart"
    StrCpy $DlRReason "Setup could not start ${LAUNCHER} (Windows error $4)."
  ${Else}
    System::Call '*$2(i .r5, i .r6, i, i)'
    System::Call 'kernel32::CloseHandle(i r6) i .r7'
    StrCpy $DlProc $5
    StrCpy $DlRunning 1
  ${EndIf}
  System::Free $1
  System::Free $2

  ShowWindow $DlRetry ${SW_HIDE}
  ShowWindow $DlStop ${SW_SHOW}
  EnableWindow $DlStop 1
  GetDlgItem $0 $HWNDPARENT 1
  EnableWindow $0 0
  ${If} $DlExit == "nostart"
    Call DlEnded
  ${EndIf}
FunctionEnd

; One read of the progress file into the DlR* variables, kept only when it is complete.
; The file is opened with every sharing flag, delete included, because the launcher
; replaces it by renaming a new copy over it, and a reader holding it without
; FILE_SHARE_DELETE (which is how FileOpen opens a file) would make that rename fail for
; as long as the read lasts. The handle is a plain Win32 handle, which FileRead accepts.
Function DlRead
  StrCpy $DlOk 0
  System::Call 'kernel32::CreateFile(t "$PLUGINSDIR\p.txt", i 0x80000000, i 7, i 0, i 3, i 0x80, i 0) i .r0'
  ${If} $0 == -1
  ${OrIf} $0 == 0
    Return
  ${EndIf}
  StrCpy $R1 ""
  StrCpy $R2 ""
  StrCpy $R3 ""
  StrCpy $R4 ""
  StrCpy $R5 ""
  StrCpy $R6 ""
  StrCpy $R9 0
  ${Do}
    ClearErrors
    FileRead $0 $R7
    ${IfThen} ${Errors} ${|} ${ExitDo} ${|}
    ${TrimNewLines} "$R7" $R7
    ${If} $R7 == ""
      ${Continue}
    ${EndIf}
    ; "end" counts only as the last line: anything after it means the read is not one
    ; whole write.
    ${If} $R7 == "end"
      StrCpy $R9 1
      ${Continue}
    ${EndIf}
    StrCpy $R9 0
    StrCpy $R8 $R7 6
    ${If} $R8 == "state="
      StrCpy $R1 $R7 "" 6
    ${EndIf}
    StrCpy $R8 $R7 4
    ${If} $R8 == "pct="
      StrCpy $R2 $R7 "" 4
    ${EndIf}
    StrCpy $R8 $R7 5
    ${If} $R8 == "text="
      StrCpy $R3 $R7 "" 5
    ${ElseIf} $R8 == "item="
      StrCpy $R4 $R7 "" 5
    ${ElseIf} $R8 == "beat="
      StrCpy $R5 $R7 "" 5
    ${EndIf}
    StrCpy $R8 $R7 7
    ${If} $R8 == "reason="
      StrCpy $R6 $R7 "" 7
    ${EndIf}
  ${Loop}
  FileClose $0
  ${If} $R9 == 1
    StrCpy $DlRState $R1
    StrCpy $DlRPct $R2
    StrCpy $DlRText $R3
    StrCpy $DlRItem $R4
    StrCpy $DlRBeat $R5
    StrCpy $DlRReason $R6
    StrCpy $DlOk 1
  ${EndIf}
FunctionEnd

; The last complete read, onto the page. Only what changed is set, so the labels do not
; flicker four times a second, and the bar never moves backwards within one run.
Function DlShow
  IntOp $0 $DlRPct + 0
  ${If} $0 < 0
    StrCpy $0 0
  ${ElseIf} $0 > 100
    StrCpy $0 100
  ${EndIf}
  ${If} $0 > $DlPctShown
    StrCpy $DlPctShown $0
    SendMessage $DlBar ${PBM_SETPOS} $0 0
  ${EndIf}
  ${If} $DlRItem != ""
  ${AndIf} $DlRItem != $DlItemShown
    StrCpy $DlItemShown $DlRItem
    ${NSD_SetText} $DlItem $DlRItem
  ${EndIf}
  ${If} $DlRText != $DlTextShown
    StrCpy $DlTextShown $DlRText
    ${NSD_SetText} $DlSize $DlRText
  ${EndIf}
FunctionEnd

Function DlTick
  ${If} $DlRunning != 1
    Return
  ${EndIf}
  Call DlRead
  ${If} $DlOk == 1
    Call DlShow
    ${If} $DlRBeat != $DlLastBeat
      StrCpy $DlLastBeat $DlRBeat
      StrCpy $DlQuiet 0
    ${Else}
      IntOp $DlQuiet $DlQuiet + 1
    ${EndIf}
  ${Else}
    IntOp $DlQuiet $DlQuiet + 1
  ${EndIf}

  ; The launcher's own end decides: its exit code is the verdict, and its last write
  ; supplies the words. 259 is STILL_ACTIVE.
  System::Call 'kernel32::GetExitCodeProcess(i $DlProc, *i .r0) i .r1'
  ${If} $1 != 0
  ${AndIf} $0 != 259
    StrCpy $DlExit $0
    Call DlRead
    ${If} $DlOk == 1
      Call DlShow
    ${EndIf}
    Call DlEnded
    Return
  ${EndIf}

  ; STOPPED, it is given DL_STOP_TICKS to end by itself, beat or no beat, because a network
  ; read can hold it for the 30 seconds of its receive timeout before it sees the cancel
  ; file (the note at DL_STOP_TICKS). Only then is it ended, and what it kept is resumed.
  ${If} $DlStopping >= 0
    IntOp $DlStopping $DlStopping + 1
    ${If} $DlStopping >= ${DL_STOP_TICKS}
      System::Call 'kernel32::TerminateProcess(i $DlProc, i 2) i .r0'
      System::Call 'kernel32::WaitForSingleObject(i $DlProc, i 5000) i .r0'
      StrCpy $DlExit 2
      Call DlEnded
    ${EndIf}
    Return
  ${EndIf}

  ; A launcher that has written nothing new for thirty seconds has stopped working: the
  ; beat moves on every write, whether bytes are arriving or not.
  ${If} $DlQuiet >= ${DL_STALL_TICKS}
    System::Call 'kernel32::TerminateProcess(i $DlProc, i 1) i .r0'
    System::Call 'kernel32::WaitForSingleObject(i $DlProc, i 5000) i .r0'
    StrCpy $DlExit "stall"
    Call DlEnded
  ${EndIf}
FunctionEnd

; The run is over. Decide what it came to, and say so.
Function DlEnded
  StrCpy $DlRunning 0
  ${If} $DlProc != 0
    System::Call 'kernel32::CloseHandle(i $DlProc) i .r0'
    StrCpy $DlProc 0
  ${EndIf}
  ShowWindow $DlStop ${SW_HIDE}

  StrCpy $R0 "failed"
  ${If} $DlExit == 0
    StrCpy $R0 "done"
  ${ElseIf} $DlExit == 2
    StrCpy $R0 "cancelled"
  ${ElseIf} $DlExit == 3
    ${If} $DlRReason == ""
      StrCpy $DlRReason "Setup could not reach the download servers. Check the internet connection, then press Retry."
    ${EndIf}
  ${ElseIf} $DlExit == 1
    ${If} $DlRReason == ""
      StrCpy $DlRReason "The download failed."
    ${EndIf}
  ${ElseIf} $DlExit == "stall"
    ${If} $DlRState == "done"
      StrCpy $R0 "done"
    ${Else}
      StrCpy $DlRReason "The download stopped responding for thirty seconds."
    ${EndIf}
  ${ElseIf} $DlExit == "nostart"
    ; the reason was set where the start failed
  ${Else}
    IntFmt $1 "0x%08X" $DlExit
    StrCpy $DlRReason "${LAUNCHER} stopped before the download finished (exit code $1)."
  ${EndIf}

  ${If} ${FileExists} "$INSTDIR\opencnc3d-install.txt"
    StrCpy $Playable 1
  ${EndIf}

  ${If} $R0 == "done"
    StrCpy $Playable 1
    SendMessage $DlBar ${PBM_SETPOS} 100 0
    ${NSD_SetText} $DlHeading "OpenCNC3D is installed$MediaWith."
    ${NSD_SetText} $DlNote "Press Next to finish."
    StrCpy $FinishText "OpenCNC3D is installed$MediaWith.$\r$\n$\r$\nThe launcher keeps the game up to date. Its gear button adds or removes the movies and the music."
    ShowWindow $DlRetry ${SW_HIDE}
  ${Else}
    ${If} $R0 == "cancelled"
      ${NSD_SetText} $DlHeading "The download is stopped."
      ${NSD_SetText} $DlRetry "&Resume"
      StrCpy $1 "What has arrived is kept."
      StrCpy $2 "Resume"
    ${Else}
      ${NSD_SetText} $DlHeading "The download did not finish."
      ${NSD_SetText} $DlRetry "&Retry"
      StrCpy $1 "$DlRReason"
      StrCpy $2 "Retry"
    ${EndIf}
    ${If} $Playable == 1
      ${NSD_SetText} $DlNote "$1$\r$\n$\r$\nOpenCNC3D can already be played from this folder. $2 carries on from where the download stopped. Next finishes without what is missing, which the launcher fetches later."
      StrCpy $FinishText "OpenCNC3D is installed, but not everything was downloaded.$\r$\n$\r$\nThe launcher's Update brings the game up to date, and its gear button adds the movies and the music."
    ${Else}
      ${NSD_SetText} $DlNote "$1$\r$\n$\r$\nOpenCNC3D is not installed yet. $2 carries on from where the download stopped; Cancel leaves Setup and removes what it installed."
    ${EndIf}
    ShowWindow $DlRetry ${SW_SHOW}
    EnableWindow $DlRetry 1
  ${EndIf}

  ${If} $Playable == 1
    Call RegisterInstall
    Call WriteEstimatedSize
    GetDlgItem $0 $HWNDPARENT 1
    EnableWindow $0 1
    ${If} $R0 == "done"
      SendMessage $HWNDPARENT ${WM_NEXTDLGCTL} $0 1
    ${EndIf}
  ${EndIf}
FunctionEnd

Function DlOnStop
  Pop $0
  ${If} $DlRunning == 1
    FileOpen $0 "$PLUGINSDIR\c.txt" w
    FileWrite $0 "stop$\r$\n"
    FileClose $0
    EnableWindow $DlStop 0
    StrCpy $DlStopping 0
    ${NSD_SetText} $DlNote "Stopping the download. What has arrived is kept, and Resume carries on from there."
  ${EndIf}
FunctionEnd

Function DlOnRetry
  Pop $0
  ${If} $DlRunning != 1
    ${NSD_SetText} $DlHeading "Setup is downloading the game$MediaWords."
    Call DlStart
  ${EndIf}
FunctionEnd

Function DlPageCreate
  Call MediaChoice
  !insertmacro MUI_HEADER_TEXT "Downloading OpenCNC3D" "The launcher is downloading the game$MediaWords."

  nsDialogs::Create 1018
  Pop $DlDialog
  ${If} $DlDialog == error
    Abort
  ${EndIf}

  ${NSD_CreateLabel} 0 0 100% 18u "Setup is downloading the game$MediaWords."
  Pop $DlHeading
  CreateFont $DlHeadFont "$(^Font)" "$(^FontSize)" "700"
  SendMessage $DlHeading ${WM_SETFONT} $DlHeadFont 1

  ${NSD_CreateLabel} 0 22u 100% 10u ""
  Pop $DlItem
  ${NSD_CreateProgressBar} 0 34u 100% 12u ""
  Pop $DlBar
  ${NSD_CreateLabel} 0 50u 100% 10u ""
  Pop $DlSize
  ; Room for seven lines: a reason of two, and what the player can do next.
  ${NSD_CreateLabel} 0 66u 100% 54u ""
  Pop $DlNote

  ${NSD_CreateButton} 0 124u 64u 14u "&Stop"
  Pop $DlStop
  ${NSD_OnClick} $DlStop DlOnStop
  ${NSD_CreateButton} 70u 124u 64u 14u "&Retry"
  Pop $DlRetry
  ${NSD_OnClick} $DlRetry DlOnRetry
  ShowWindow $DlRetry ${SW_HIDE}

  ; No way back into the install, and no way on until there is a game to go on to.
  GetDlgItem $0 $HWNDPARENT 3
  EnableWindow $0 0
  GetDlgItem $0 $HWNDPARENT 1
  EnableWindow $0 0

  Call DlStart
  ${NSD_CreateTimer} DlTick ${DL_TICK_MS}
  nsDialogs::Show
FunctionEnd

Function DlPageLeave
  ${If} $Playable != 1
  ${OrIf} $DlRunning == 1
    Abort
  ${EndIf}
  ${NSD_KillTimer} DlTick
FunctionEnd

; ---------------------------------------------------------------- uninstall

Section "Uninstall"
  ; RMDir /r ON A USER-CHOSEN PATH IS A LOADED GUN, so it is only ever fired at a
  ; folder this installer wrote its own marker into. Without that check, an
  ; $INSTDIR that had been edited to C:\ would delete C:\.
  IfFileExists "$INSTDIR\${LAUNCHER}" +3 0
    MessageBox MB_ICONSTOP "That does not look like an OpenCNC3D install, so nothing was removed."
    Abort

  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk"
  RMDir  "$SMPROGRAMS\${APPNAME}"
  Delete "$DESKTOP\${APPNAME}.lnk"

  ; The whole folder: the launcher, the game it downloaded and every update laid over
  ; it, the movies and the music, the record of that choice, and the player's settings.
  RMDir /r "$INSTDIR"

  DeleteRegKey HKCU "${REGKEY}"
  DeleteRegKey HKCU "Software\OpenCNC3D"
SectionEnd
