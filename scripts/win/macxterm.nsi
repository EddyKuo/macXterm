; macXterm Windows installer (NSIS). Build with: makensis scripts\win\macxterm.nsi
; Expects the staged portable tree at dist\macXterm\ (run scripts\win\package.ps1 first).
;
; This produces an UNSIGNED installer. Sign both the installer and the exe with your
; own Authenticode certificate before distribution:
;   signtool sign /fd SHA256 /a /tr http://timestamp.digicert.com /td SHA256 <file>

!define APPNAME    "macXterm"
!define COMPANY    "macXterm"
!define VERSION    "0.1.0"
!define EXENAME    "macXterm.exe"

Name "${APPNAME}"
OutFile "..\..\dist\macXterm-setup.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKCU "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel admin
Unicode true
SetCompressor /SOLID lzma

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "Install"
    SetOutPath "$INSTDIR"
    ; Stage the whole portable tree produced by package.ps1.
    File /r "..\..\dist\macXterm\*.*"

    ; Start-menu + desktop shortcuts.
    CreateDirectory "$SMPROGRAMS\${APPNAME}"
    CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\${EXENAME}"
    CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXENAME}"

    ; URL protocol + file association (per-machine; the app can also self-register
    ; per-user via File > Register with Windows).
    WriteRegStr HKCR "macxterm" "" "URL:macXterm Protocol"
    WriteRegStr HKCR "macxterm" "URL Protocol" ""
    WriteRegStr HKCR "macxterm\shell\open\command" "" '"$INSTDIR\${EXENAME}" "%1"'
    WriteRegStr HKCR ".mxtsession" "" "macXterm.Session"
    WriteRegStr HKCR "macXterm.Session\shell\open\command" "" '"$INSTDIR\${EXENAME}" "%1"'

    ; Add/Remove Programs entry.
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
        "DisplayName" "${APPNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
        "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
        "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "Software\${APPNAME}" "InstallDir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
    RMDir  "$SMPROGRAMS\${APPNAME}"
    Delete "$DESKTOP\${APPNAME}.lnk"
    DeleteRegKey HKCR "macxterm"
    DeleteRegKey HKCR ".mxtsession"
    DeleteRegKey HKCR "macXterm.Session"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
    DeleteRegKey HKCU "Software\${APPNAME}"
    RMDir /r "$INSTDIR"
SectionEnd
