; OpenCNC3D Setup installs the empty launcher. Games and optional media are
; selected inside its Module Browser after installation.
; Empty installs perform no network calls; Complete variants embed one verified game
; and offer its movies/music inside this installer.
; Keep ANSI until the documented Homebrew Unicode writer crash is resolved.
Unicode false
!ifndef PAYLOAD
  !error "Run tools/win/make-installer-win.sh (PAYLOAD missing)."
!endif
!ifndef VERSION
  !error "VERSION missing."
!endif
!ifndef OUTFILE
  !error "OUTFILE missing."
!endif
!ifndef ICONFILE
  !error "ICONFILE missing."
!endif
!ifndef WIZARDBMP
  !error "WIZARDBMP missing."
!endif
!define APPNAME "OpenCNC3D"
!define COMPANY "OpenCNC3D"
!define REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenCNC3D"
!define LAUNCHER "OpenCNC3D.exe"
Name "${PACKAGE_TITLE}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\OpenCNC3D"
InstallDirRegKey HKCU "Software\OpenCNC3D" "InstallDir"
RequestExecutionLevel user
!ifdef MODULE
  SetCompressor lzma
!else
  SetCompressor /SOLID lzma
!endif
BrandingText "${APPNAME} Setup ${VERSION}"
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "Sections.nsh"
; OpenCNC3D is 64-bit x86 only. Setup itself is a 32-bit program (NSIS ships only x86
; stubs), so it starts everywhere and says so plainly where the game cannot run,
; instead of installing a launcher that cannot start there:
;   - 32-bit Windows, which cannot run 64-bit programs at all;
;   - Windows 10 on an ARM processor (OS build below 22000), which emulates 32-bit x86
;     programs but not 64-bit ones.
; Windows 11 on ARM (build 22000 and later) emulates x64 and is let through. The game
; has not been run there: it is allowed, and untested.
; Windows 7, 8 and 8.1 on x64 are warned, not refused: the programs load there (none
; imports anything newer than Windows 7, which tools/win/check-pe-arch.py holds), but the
; game has never been run there and needs three things those systems may lack. OK goes
; on, Cancel stops with exit code 22, and a silent install goes on.
!macro RequireX64
  ${If} ${IsNativeIA32}
    MessageBox MB_ICONSTOP "${APPNAME} needs 64-bit Windows 10 or 11.$\r$\n$\r$\nThis PC runs 32-bit Windows, which this build does not support. Nothing was installed or changed." /SD IDOK
    SetErrorLevel 21
    Quit
  ${ElseIf} ${IsNativeARM64}
  ${AndIfNot} ${AtLeastBuild} 22000
    MessageBox MB_ICONSTOP "${APPNAME} needs 64-bit Windows 10 or 11 on an x64 processor, or Windows 11 on an ARM processor.$\r$\n$\r$\nThis PC runs Windows 10 on an ARM processor, which cannot run 64-bit x86 programs. Nothing was installed or changed." /SD IDOK
    SetErrorLevel 21
    Quit
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    ${IfNot} ${Cmd} `MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "${APPNAME} is made for 64-bit Windows 10 and 11. This PC runs an older Windows, where the game has never been tested.$\r$\n$\r$\nTo run at all there it needs: on Windows 7 and 8.1, update KB2999226 (the Universal C Runtime); Internet Explorer 11 or TLS 1.2 turned on, for the launcher's downloads; and a graphics driver from the card's maker with OpenGL 2.1.$\r$\n$\r$\nClick OK to install anyway, or Cancel to stop. Nothing has been installed or changed yet." /SD IDOK IDOK`
      SetErrorLevel 22
      Quit
    ${EndIf}
  ${EndIf}
!macroend
!ifndef MODULE
Function .onInit
  !insertmacro RequireX64
FunctionEnd
!endif
!ifdef MODULE
  !include "TextFunc.nsh"
  !include "nsDialogs.nsh"
  !include "WinMessages.nsh"
  !include "media-state.nsh"
!endif
!define MUI_ICON "${ICONFILE}"
!define MUI_UNICON "${ICONFILE}"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${WIZARDBMP}"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${WIZARDBMP}"
!define MUI_ABORTWARNING
!ifdef MODULE
  !define MUI_CUSTOMFUNCTION_ABORT OnSetupAbort
  !define MUI_WELCOMEPAGE_TEXT "Setup installs ${GAME_TITLE} and the OpenCNC3D launcher.$\r$\n$\r$\nThe game is included. Choose optional movies and music on the next page. Existing saves and settings are preserved."
!else
!define MUI_WELCOMEPAGE_TEXT "Setup installs the OpenCNC3D launcher.$\r$\n$\r$\nAfter installation, open Add Game to choose Dawn3D or RA3D. Each game downloads separately. Movies and music are optional.$\r$\n$\r$\nNo game is downloaded by Setup. Nothing needs administrator rights.$\r$\n$\r$\nClick Next to continue."
!endif
!insertmacro MUI_PAGE_WELCOME
!ifdef MODULE
  !define MUI_COMPONENTSPAGE_TEXT_TOP "The game is included. Movies and music are optional downloads; choose either, both or neither."
  !insertmacro MUI_PAGE_COMPONENTS
!endif
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!ifdef MODULE
  Page custom DlPageCreate DlPageLeave
!endif
!ifdef MODULE
  !define MUI_FINISHPAGE_TEXT "$FinishText"
!else
!define MUI_FINISHPAGE_TEXT "The OpenCNC3D launcher is installed.$\r$\n$\r$\nOpen Add Game to download a game. Installed games, saves and settings from an existing installation are preserved."
!endif
!define MUI_FINISHPAGE_RUN "$INSTDIR\${LAUNCHER}"
!define MUI_FINISHPAGE_RUN_TEXT "Start OpenCNC3D"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\READ-ME-WINDOWS.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Read the notes for this build"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!insertmacro MUI_PAGE_FINISH
; The uninstaller asks one question, whether the settings and logs go too, and says
; plainly that saves are kept: they live in the per-user folder, which it never touches.
!define MUI_COMPONENTSPAGE_TEXT_TOP "Settings and logs are kept unless ticked below. Your own files in the folder (maps, screenshots) and saved games are always kept."
!insertmacro MUI_UNPAGE_COMPONENTS
!define MUI_UNCONFIRMPAGE_TEXT_TOP "OpenCNC3D, its games, movies, music and language packs will be removed from the folder below. Saved games are kept: they live in %APPDATA%\OpenCNC3D. Earlier builds kept them in %APPDATA%\Slipgate Ironworks\OpenCNC3D, which is left in place as a backup."
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "${GAME_TITLE}" SecMain
!ifdef MODULE
  AddSize ${MODULE_KB}
!endif
  SectionIn RO
  Call InstDirIsRoot
  Pop $0
  ${If} $0 == 1
    MessageBox MB_ICONSTOP "Choose a folder below the root of the drive or network share." /SD IDOK
    SetErrorLevel 15
    Quit
  ${EndIf}
  ; An interrupted module/update transaction must recover with its own launcher.
  ; Do not discard its journal or silently overwrite files it may roll back.
  ${If} ${FileExists} "$INSTDIR\opencnc3d-update.journal"
  ${OrIf} ${FileExists} "$INSTDIR\opencnc3d-module-removal.txt"
    MessageBox MB_ICONSTOP "Open the existing launcher once to finish recovering its interrupted update, then run Setup again." /SD IDOK
    SetErrorLevel 16
    Quit
  ${EndIf}
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD}/*"
!ifdef MODULE
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  SetCompress off
  File /oname=module.zip "${MODULE_ZIP}"
  SetCompress auto
  DetailPrint "Installing ${GAME_TITLE} from the included verified package..."
  ClearErrors
  ExecWait '"$INSTDIR\${LAUNCHER}" --dir "$INSTDIR" --install-module ${MODULE} --module-file "$PLUGINSDIR\module.zip" --module-sha ${MODULE_SHA}' $0
  ${If} ${Errors}
  ${OrIf} $0 != 0
    MessageBox MB_ICONSTOP "The included game could not be installed. Existing saves and settings were preserved." /SD IDOK
    SetErrorLevel 20
    Abort
  ${EndIf}
  SetOutPath "$INSTDIR"
!endif
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\${LAUNCHER}" "" "$INSTDIR\opencnc3d.ico" 0
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${LAUNCHER}" "" "$INSTDIR\opencnc3d.ico" 0
  WriteRegStr HKCU "Software\OpenCNC3D" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKCU "${REGKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${REGKEY}" "Publisher" "${COMPANY}"
  WriteRegStr HKCU "${REGKEY}" "DisplayIcon" "$INSTDIR\opencnc3d.ico"
  WriteRegStr HKCU "${REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegDWORD HKCU "${REGKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${REGKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${REGKEY}" "EstimatedSize" "$0"
  SetErrorLevel 0
SectionEnd

!ifdef MODULE
Section "Movies (optional download)" SecMovies
SectionEnd
Section "Music (optional download)" SecMusic
SectionEnd
Section "-optional media when silent" SecSilent
  ${If} ${Silent}
    Call MediaChoice
    ClearErrors
    ExecWait '"$INSTDIR\${LAUNCHER}" --dir "$INSTDIR" --media-install $MediaArg --media-game ${MEDIA_GAME} --media-add-only' $0
    ${If} ${Errors}
      SetErrorLevel 14
    ${ElseIf} $0 != 0
      IntOp $0 $0 + 10
      SetErrorLevel $0
    ${EndIf}
  ${EndIf}
SectionEnd
!include "media-page.nsh"
!endif

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


; THE UNINSTALLER TAKES OUT WHAT SETUP AND THE LAUNCHER PUT IN THE FOLDER. The launcher
; is the only program that knows what each game owns, so it is asked first, headless:
; both games (every file either game's ownership list names, shared files included),
; the movies, music and language packs, its download cache and an update's leftover
; download, its records, and the settings and logs when that box is ticked
; (launcher/lsetup_cli.h, --uninstall). It removes by name only and never recursively,
; so a player's own files and the folders holding them stay. If it cannot finish (the
; game running, another launcher open, a file in use), nothing below runs: the
; launcher, the shortcuts and the Add/Remove Programs entry stay, so the uninstall can
; simply be run again.
Section "un.OpenCNC3D, its games, movies, music and language packs" unSecGames
  SectionIn RO
SectionEnd
Section /o "un.Settings and logs" unSecSettings
SectionEnd
Section "-un.Remove" unSecRemove
  IfFileExists "$INSTDIR\${LAUNCHER}" +3 0
    MessageBox MB_ICONSTOP "That does not look like an OpenCNC3D launcher installation." /SD IDOK
    Abort
  StrCpy $1 "--keep-settings"
  ${If} ${SectionIsSelected} ${unSecSettings}
    StrCpy $1 ""
  ${EndIf}
  DetailPrint "Removing the games, movies, music and language packs..."
  ClearErrors
  ExecWait '"$INSTDIR\${LAUNCHER}" --uninstall $1 --dir "$INSTDIR"' $0
  ${If} ${Errors}
  ${OrIf} $0 != 0
    MessageBox MB_ICONSTOP "Some game files are in use and were kept. Close OpenCNC3D and run the uninstaller again." /SD IDOK
    SetErrorLevel 2
    Abort
  ${EndIf}
  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"
  Delete "$DESKTOP\${APPNAME}.lnk"
  ; Then the launcher's own files, which it could not remove while it was running.
  Delete "$INSTDIR\${LAUNCHER}"
  Delete "$INSTDIR\opencnc3d.ico"
  Delete "$INSTDIR\READ-ME-WINDOWS.txt"
  Delete "$INSTDIR\uninstall.exe"
  ; the launcher's own log and its previous copy (the game's logs went above, if chosen)
  Delete "$INSTDIR\opencnc3d-launcher-log.txt"
  Delete "$INSTDIR\opencnc3d-launcher-log-previous.txt"
  ; The bootstrap's files, which the launcher already removed with the games when a
  ; game's list named them, and an old SDL2.dll. A brain DLL still here means the
  ; launcher above kept a game, so they stay for it.
  ${IfNot} ${FileExists} "$INSTDIR\TiberianDawn.dll"
  ${AndIfNot} ${FileExists} "$INSTDIR\ra3d\RedAlert.dll"
    Delete "$INSTDIR\SDL2.dll"
    Delete "$INSTDIR\dosmenu.pack"
    Delete "$INSTDIR\enhanced-menu.pack"
    Delete "$INSTDIR\ecur\ecur.bin"
    RMDir "$INSTDIR\ecur"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\NOTICE.md"
  ${EndIf}
  ; Only if empty: a folder still holding the player's files, or the settings when
  ; they were kept, stays with them.
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${REGKEY}"
  DeleteRegKey HKCU "Software\OpenCNC3D"
SectionEnd
