Unicode true
RequestExecutionLevel user
SetCompressor /SOLID lzma
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!ifndef VERSION
!define VERSION "0.1.0"
!endif
!ifndef PAYLOAD
!error "Define PAYLOAD: runtime-only CMake installation directory"
!endif
!ifndef OUTPUT
!define OUTPUT "HarvestMoonBTN-${VERSION}-windows-x64-setup.exe"
!endif
Name "Harvest Moon BTN (PC)"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\HarvestMoonBTN"
InstallDirRegKey HKCU "Software\HarvestMoonBTN" "InstallDir"
!define MUI_ICON "${PAYLOAD}\hmbtn-native.ico"
!define MUI_UNICON "${PAYLOAD}\hmbtn-native.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchGame
!define MUI_FINISHPAGE_RUN_TEXT "Iniciar Harvest Moon BTN"
!define MUI_FINISHPAGE_TEXT "O porte nativo foi instalado.$\r$\nOs dados extraidos ficam na pasta do usuario e sao preservados nas atualizacoes.$\r$\n$\r$\nEste porte ainda e parcial; o jogo completo esta em desenvolvimento."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PAYLOAD}\licenses\CC0.txt"
!insertmacro MUI_PAGE_DIRECTORY
Page custom DiscPage DiscLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "PortugueseBR"
Var DiscPath
Var DiscInput
Function DiscPage
    !insertmacro MUI_HEADER_TEXT "Recursos do jogo" "Escolha sua copia .bin ou importe ao iniciar pela primeira vez."
    nsDialogs::Create 1018
    Pop $0
    ${NSD_CreateLabel} 0 0 100% 42u "O instalador contem somente o porte para PC. Selecione um disco americano SLUS_011.15 (MODE2/2352), incluindo traducoes compativeis. O .bin nao sera copiado nem alterado."
    Pop $0
    ${NSD_CreateFileRequest} 0 52u 78% 13u "$DiscPath"
    Pop $DiscInput
    ${NSD_CreateBrowseButton} 80% 51u 20% 15u "Procurar..."
    Pop $0
    ${NSD_OnClick} $0 BrowseDisc
    ${NSD_CreateLabel} 0 80u 100% 40u "Opcional: deixe vazio para selecionar o disco no primeiro inicio. Nao e necessario instalar Python, SDL ou emulador. Apenas os recursos serao extraidos; executaveis do PS1 nao sao instalados."
    Pop $0
    nsDialogs::Show
FunctionEnd
Function BrowseDisc
    nsDialogs::SelectFileDialog open "" "Disco PS1 (*.bin)|*.bin"
    Pop $0
    ${If} $0 != ""
        ${NSD_SetText} $DiscInput $0
    ${EndIf}
FunctionEnd
Function DiscLeave
    ${NSD_GetText} $DiscInput $DiscPath
    ${If} $DiscPath != ""
        IfFileExists "$DiscPath" valid
        MessageBox MB_OK|MB_ICONEXCLAMATION "O arquivo selecionado nao existe."
        Abort
        valid:
    ${EndIf}
FunctionEnd
Function LaunchGame
    Exec '"$INSTDIR\hmbtn_native.exe"'
FunctionEnd
Section "Porte nativo" Main
    SetShellVarContext current
    SetOutPath "$INSTDIR"
    File "${PAYLOAD}\hmbtn_native.exe"
!if /FileExists "${PAYLOAD}/SDL3.dll"
    File "${PAYLOAD}\SDL3.dll"
!endif
    File "${PAYLOAD}\hmbtn-native.ico"
    File "${PAYLOAD}\README.txt"
    File /r "${PAYLOAD}\licenses"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\Harvest Moon BTN"
    CreateShortCut "$SMPROGRAMS\Harvest Moon BTN\Harvest Moon BTN.lnk" "$INSTDIR\hmbtn_native.exe" "" "$INSTDIR\hmbtn-native.ico"
    CreateShortCut "$SMPROGRAMS\Harvest Moon BTN\Preparar dados.lnk" "$INSTDIR\hmbtn_native.exe" "--setup" "$INSTDIR\hmbtn-native.ico"
    CreateShortCut "$DESKTOP\Harvest Moon BTN.lnk" "$INSTDIR\hmbtn_native.exe" "" "$INSTDIR\hmbtn-native.ico"
    WriteRegStr HKCU "Software\HarvestMoonBTN" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\HarvestMoonBTN" "DisplayName" "Harvest Moon BTN (PC)"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\HarvestMoonBTN" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\HarvestMoonBTN" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\HarvestMoonBTN" "DisplayIcon" "$INSTDIR\hmbtn-native.ico"
    ${If} $DiscPath != ""
        DetailPrint "Extraindo recursos do jogo..."
        ExecWait '"$INSTDIR\hmbtn_native.exe" --import-bin "$DiscPath"' $0
        ${If} $0 != 0
            MessageBox MB_OK|MB_ICONINFORMATION "Os arquivos do aplicativo foram instalados, mas a importacao nao foi concluida (ou ja existem dados). Dados existentes foram preservados. Inicie o jogo para consultar ou preparar os recursos."
        ${EndIf}
    ${EndIf}
SectionEnd
Section "Uninstall"
    SetShellVarContext current
    Delete "$DESKTOP\Harvest Moon BTN.lnk"
    Delete "$SMPROGRAMS\Harvest Moon BTN\Harvest Moon BTN.lnk"
    Delete "$SMPROGRAMS\Harvest Moon BTN\Preparar dados.lnk"
    RMDir "$SMPROGRAMS\Harvest Moon BTN"
    Delete "$INSTDIR\hmbtn_native.exe"
    Delete "$INSTDIR\SDL3.dll"
    Delete "$INSTDIR\hmbtn-native.ico"
    Delete "$INSTDIR\README.txt"
    Delete "$INSTDIR\licenses\CC0.txt"
    Delete "$INSTDIR\licenses\SDL3.txt"
    RMDir "$INSTDIR\licenses"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    DeleteRegKey HKCU "Software\HarvestMoonBTN"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\HarvestMoonBTN"
    ; User resources/saves are outside INSTDIR. Never delete them here.
SectionEnd
