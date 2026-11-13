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
Filename: "{app}\ThirdParty\VB-CABLE\VBCABLE_Setup_x64.exe"; Description: "Apri installazione ufficiale VB-CABLE (admin, poi riavvia Windows)"; WorkingDir: "{app}\ThirdParty\VB-CABLE"; Flags: postinstall skipifsilent; Components: vbcable; Check: ShouldLaunchVendorSetup
Filename: "{app}\{#MyAppExeName}"; Description: "Avvia A50 VoiceClean"; Flags: postinstall nowait skipifsilent; Components: app

[Code]
var
  RemovePersonalSettings: Boolean;

// Look for the VB-Audio PnP driver, not just a particular endpoint name.
// A user can rename the Windows CABLE Input/Output endpoint labels.
// Return false if WMI is unavailable; never launch a vendor setup on a guess.
function DetectInstalledVBCable(var Installed: Boolean): Boolean;
var
  Locator, WmiService, Devices: Variant;
begin
  Installed := False;
  Result := False;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    WmiService := Locator.ConnectServer('.', 'root\CIMV2');
    Devices := WmiService.ExecQuery(
      'SELECT Name FROM Win32_PnPEntity WHERE Name LIKE ''%VB-Audio Virtual Cable%''');
    Installed := Devices.Count > 0;
    Result := True;
  except
    Log('Impossibile verificare il driver VB-CABLE tramite WMI.');
  end;
end;

procedure InitializeWizard();
var
  Installed, DetectionOK: Boolean;
begin
  DetectionOK := DetectInstalledVBCable(Installed);
  if DetectionOK and Installed then begin
    // The application does not require a second installation of the driver.
    // In silent packaging-only mode, keep explicit /COMPONENTS selections
    // so CI can inspect the vendor payload; [Run] is still suppressed.
    if not WizardSilent then begin
      WizardSelectComponents('!vbcable');
      MsgBox('VB-CABLE e'' gia'' installato in Windows.' + #13#10 +
             'VoiceClean riutilizzera'' il driver senza reinstallarlo.' + #13#10 +
             'L''uscita audio normale deve rimanere sulle cuffie ASTRO A50 Game.',
             mbInformation, MB_OK);
    end;
  end else if not DetectionOK then begin
    // Fail safe when Windows cannot report driver state.
    if not WizardSilent then begin
      WizardSelectComponents('!vbcable');
      MsgBox('Impossibile verificare se VB-CABLE e'' gia'' installato.' + #13#10 +
             'Per sicurezza l''avvio automatico del setup driver e'' disabilitato.' + #13#10 +
             'Puoi gestirlo manualmente dal pacchetto ufficiale VB-Audio.',
             mbInformation, MB_OK);
    end;
  end;
end;

function ShouldLaunchVendorSetup(): Boolean;
var
  Installed, DetectionOK: Boolean;
begin
  // Recheck immediately before execution to prevent reinstalling a driver
  // that was installed after the first wizard screen.
  DetectionOK := DetectInstalledVBCable(Installed);
  Result := DetectionOK and not Installed;
  if not DetectionOK then
    Log('Setup VB-CABLE non avviato: stato driver non verificabile.')
  else if Installed then
    Log('Setup VB-CABLE non avviato: driver gia'' presente.');
end;

function InitializeUninstall(): Boolean;
var
  VendorSetup: String;
  ResultCode: Integer;
begin
  Result := True;
  // Automated/unattended uninstall must never remove third-party drivers or user profiles.
  if UninstallSilent then begin
    RemovePersonalSettings := False;
    Exit;
  end;
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
