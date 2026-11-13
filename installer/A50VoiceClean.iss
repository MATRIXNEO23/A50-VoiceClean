; A50 VoiceClean — unified Win10/Win11 installer, native C++ x64 binaries.
; VB-CABLE remains a clearly identified, independently managed third-party driver.
#define MyAppName "A50 VoiceClean"
#define MyAppVersion "0.3.0-preview"
#define MyPublisher "MATRIXNEO23"
#define MyAppExeName "A50VoiceClean-Control.exe"

[Setup]
AppId=com.matrixneo23.a50voiceclean
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
AppPublisherURL=https://github.com/MATRIXNEO23/A50-VoiceClean
AppSupportURL=https://github.com/MATRIXNEO23/A50-VoiceClean/issues
DefaultDirName={autopf}\A50 VoiceClean
DefaultGroupName=A50 VoiceClean
AllowNoIcons=yes
OutputDir=..\dist
OutputBaseFilename=A50-VoiceClean-Setup-Win10-Win11-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
InfoBeforeFile=VBCABLE_NOTICE.txt

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Types]
Name: "full"; Description: "Completa (con driver audio VB-CABLE)"
Name: "compact"; Description: "Solo A50 VoiceClean"
Name: "custom"; Description: "Personalizzata"; Flags: iscustom

[Components]
Name: "app"; Description: "A50 VoiceClean - programma e controlli live"; Types: full compact custom; Flags: fixed
Name: "vbcable"; Description: "VB-CABLE (VB-Audio, driver donationware indipendente: installazione guidata)"; Types: full custom

[Files]
Source: "..\build\Release\A50VoiceClean-Control.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "..\build\Release\A50VoiceClean-Live.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "..\third_party_licenses\RNNOISE_COPYING.txt"; DestDir: "{app}\Licenze"; Flags: ignoreversion; Components: app
Source: "VBCABLE_NOTICE.txt"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "..\vendor\staged\*"; DestDir: "{app}\ThirdParty\VB-CABLE"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vbcable

[Icons]
Name: "{group}\A50 VoiceClean"; Filename: "{app}\{#MyAppExeName}"; Components: app
Name: "{group}\Audio Windows - seleziona microfono predefinito"; Filename: "{sys}\control.exe"; Parameters: "mmsys.cpl"; Components: app
Name: "{group}\VB-CABLE - installa o rimuovi driver"; Filename: "{app}\ThirdParty\VB-CABLE\VBCABLE_Setup_x64.exe"; Components: vbcable
Name: "{group}\Disinstalla A50 VoiceClean"; Filename: "{uninstallexe}"; Components: app
Name: "{autodesktop}\A50 VoiceClean"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Crea collegamento sul desktop"; GroupDescription: "Collegamenti:"; Flags: unchecked

[Run]
Filename: "{app}\ThirdParty\VB-CABLE\VBCABLE_Setup_x64.exe"; Description: "Apri installazione ufficiale VB-CABLE (admin, poi riavvia Windows)"; WorkingDir: "{app}\ThirdParty\VB-CABLE"; Flags: postinstall skipifsilent; Components: vbcable
Filename: "{app}\{#MyAppExeName}"; Description: "Avvia A50 VoiceClean"; Flags: postinstall nowait skipifsilent; Components: app

[Code]
var
  RemovePersonalSettings: Boolean;

function InitializeUninstall(): Boolean;
var
  VendorSetup: String;
  ResultCode: Integer;
begin
  Result := True;
  RemovePersonalSettings :=
    MsgBox(
      'Vuoi eliminare anche i profili personali e i registri diagnostici di A50 VoiceClean?' + #13#10 +
      'Scegli NO per mantenere le tue impostazioni se installerai di nuovo il programma.',
      mbConfirmation, MB_YESNO) = IDYES;
  VendorSetup := ExpandConstant('{app}\ThirdParty\VB-CABLE\VBCABLE_Setup_x64.exe');
  if FileExists(VendorSetup) then begin
    if MsgBox(
      'Se durante l’installazione hai aggiunto VB-CABLE, puoi rimuoverlo separatamente.' + #13#10 +
      'ATTENZIONE: altre applicazioni potrebbero usarlo.' + #13#10 +
      'Vuoi aprire adesso il programma originale VB-Audio per scegliere "Remove Driver"?' + #13#10 +
      'Se lo rimuovi, riavvia Windows prima di usare altri dispositivi.',
      mbConfirmation, MB_YESNO) = IDYES then begin
      if not ShellExec('runas', VendorSetup, '',
                        ExtractFileDir(VendorSetup), SW_SHOWNORMAL,
                        ewWaitUntilTerminated, ResultCode) then
        MsgBox('Impossibile aprire il gestore ufficiale VB-CABLE.' + #13#10 +
               'A50 VoiceClean verrà comunque disinstallato.',
               mbInformation, MB_OK);
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and RemovePersonalSettings then begin
    DelTree(ExpandConstant('{userappdata}\A50VoiceClean'), True, True, True);
    DelTree(ExpandConstant('{localappdata}\A50VoiceClean'), True, True, True);
  end;
end;
