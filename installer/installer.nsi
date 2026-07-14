!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "Sections.nsh"

; Settings
!define PRODUCT_NAME      "VfW FFmpeg Bridge"
!define PRODUCT_VERSION   "1.0.0-beta"
!define PRODUCT_PUBLISHER "Cvolton"
!define UNINST_KEY        "Software\Microsoft\Windows\CurrentVersion\Uninstall\vfw-ffmpeg-bridge"

Name "${PRODUCT_NAME}"
OutFile "vfw-ffmpeg-bridge-setup.exe"
InstallDir "$PROGRAMFILES64\vfw-ffmpeg-bridge"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show

; Interface
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; VfW Driver
Section "VfW Driver (required)" SEC_VFW
    SectionIn RO

    SetOutPath "$SYSDIR"
    File "files\x86\tmaudio.dll"
    File "files\x86\vfwbrdg.dll"

    SetRegView 32
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32" "vidc.fbrg" "vfwbrdg.dll"

    ${If} ${RunningX64}
        ${DisableX64FSRedirection}
        SetOutPath "$SYSDIR"
        File "files\x64\tmaudio.dll"
        File "files\x64\vfwbrdg.dll"
        ${EnableX64FSRedirection}

        SetRegView 64
        WriteRegStr HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32" "vidc.fbrg" "vfwbrdg.dll"
        SetRegView 32
    ${EndIf}

    SetRegView 64
SectionEnd

; FFmpeg
Section "FFmpeg" SEC_FFMPEG
    SetOutPath "$INSTDIR"
    File "files\ffmpeg\ffmpeg.exe"
SectionEnd

; VCRedist
Section "Visual C++ Redistributable" SEC_VCREDIST
    SetOutPath "$TEMP"

    File "files\vcredist\vc_redist.x86.exe"
    DetailPrint "Installing Visual C++ Redistributable (32-bit)..."
    ExecWait '"$TEMP\vc_redist.x86.exe" /install /quiet /norestart' $0
    DetailPrint "vc_redist.x86.exe exit code: $0"
    Delete "$TEMP\vc_redist.x86.exe"

    ${If} ${RunningX64}
        File "files\vcredist\vc_redist.x64.exe"
        DetailPrint "Installing Visual C++ Redistributable (64-bit)..."
        ExecWait '"$TEMP\vc_redist.x64.exe" /install /quiet /norestart' $0
        DetailPrint "vc_redist.x64.exe exit code: $0"
        Delete "$TEMP\vc_redist.x64.exe"
    ${EndIf}
SectionEnd

; 64-bit specifics
Function .onInit
    ${If} ${RunningX64}
        StrCpy $INSTDIR "$PROGRAMFILES64\vfw-ffmpeg-bridge"
    ${Else}
        StrCpy $INSTDIR "$PROGRAMFILES\vfw-ffmpeg-bridge"

        ; Bundled FFmpeg is 64-bit, so we skip
        SectionSetText  ${SEC_FFMPEG} "FFmpeg (requires 64-bit Windows)"
        SectionSetFlags ${SEC_FFMPEG} ${SF_RO}
    ${EndIf}
FunctionEnd

; Final touches
Section "-Finish"
    SetRegView 64
    WriteRegStr HKLM "Software\vfw-ffmpeg-bridge" "InstallDir" "$INSTDIR"

    SetRegView 32
    WriteRegStr HKLM "Software\vfw-ffmpeg-bridge" "InstallDir" "$INSTDIR"

    SetRegView 64

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

; Tooltip descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_VFW} "Installs the VfW codec driver (tmaudio.dll / vfwbrdg.dll, x86 + x64) into System32/SysWOW64 and registers it as vidc.fbrg. Required."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FFMPEG} "Installs a bundled copy of ffmpeg.exe used by the bridge. Requires 64-bit Windows."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_VCREDIST} "Installs the Visual C++ 2015-2022 Redistributable."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Uninstaller
Section "Uninstall"
    ${If} ${RunningX64}
        SetRegView 64
        DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32" "vidc.fbrg"
        DeleteRegKey HKLM "Software\vfw-ffmpeg-bridge"

        SetRegView 32
        DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32" "vidc.fbrg"
        DeleteRegKey HKLM "Software\vfw-ffmpeg-bridge"

        SetRegView 64
    ${Else}
        DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32" "vidc.fbrg"
        DeleteRegKey HKLM "Software\vfw-ffmpeg-bridge"
    ${EndIf}

    DeleteRegKey HKLM "${UNINST_KEY}"

    Delete "$SYSDIR\tmaudio.dll"
    Delete "$SYSDIR\vfwbrdg.dll"

    ${If} ${RunningX64}
        ${DisableX64FSRedirection}
        Delete "$SYSDIR\tmaudio.dll"
        Delete "$SYSDIR\vfwbrdg.dll"
        ${EnableX64FSRedirection}
    ${EndIf}

    Delete "$INSTDIR\ffmpeg.exe"
    Delete "$INSTDIR\Uninstall.exe"

    RMDir "$INSTDIR"
SectionEnd
