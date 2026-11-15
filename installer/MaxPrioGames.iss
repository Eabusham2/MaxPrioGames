#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BinaryPath
  #define BinaryPath "..\build\Release\MaxPrioGames.exe"
#endif
#ifndef ReleaseDir
  #define ReleaseDir "..\dist"
#endif

[Setup]
AppId={{8D427351-F025-42A0-9F4A-2B5F7E62D1AC}
AppName=MaxPrioGames
AppVersion={#AppVersion}
AppPublisher=Eyad / Eabusham2
AppPublisherURL=https://github.com/Eabusham2/MaxPrioGames
AppSupportURL=https://github.com/Eabusham2/MaxPrioGames/issues
AppUpdatesURL=https://github.com/Eabusham2/MaxPrioGames/releases
DefaultDirName={autopf}\MaxPrioGames
DefaultGroupName=MaxPrioGames
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\MaxPrioGames.exe
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#ReleaseDir}
OutputBaseFilename=MaxPrioGames-{#AppVersion}-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
VersionInfoVersion={#AppVersion}
VersionInfoDescription=MaxPrioGames installer

[Files]
Source: "{#BinaryPath}"; DestDir: "{app}"; DestName: "MaxPrioGames.exe"; Flags: ignoreversion
Source: "installed.flag"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
; A Start menu entry makes Windows Search discover the app. No desktop icon is created.
Name: "{commonprograms}\MaxPrioGames"; Filename: "{app}\MaxPrioGames.exe"; Comment: "Realtime game priority and all-core affinity"

[Run]
Filename: "{app}\MaxPrioGames.exe"; Description: "Start MaxPrioGames"; Flags: nowait runascurrentuser

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  AppExe: String;
begin
  Result := '';
  AppExe := ExpandConstant('{app}\MaxPrioGames.exe');
  if FileExists(AppExe) then
    Exec(AppExe, '--quit', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then begin
    if not Exec(ExpandConstant('{app}\MaxPrioGames.exe'), '--enable-startup', '',
                SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      RaiseException('Could not configure MaxPrioGames startup. Re-run Setup as the Windows account that will use MaxPrioGames.');
    if ResultCode <> 0 then
      RaiseException('MaxPrioGames startup registration failed (exit code ' + IntToStr(ResultCode) + ').');
  end;
end;

function InitializeUninstall(): Boolean;
var
  AppExe: String;
  ResultCode: Integer;
begin
  Result := True;
  AppExe := ExpandConstant('{app}\MaxPrioGames.exe');
  if not FileExists(AppExe) then
    exit;
  Exec(AppExe, '--quit', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  if not Exec(AppExe, '--disable-startup', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    Result := False
  else if ResultCode <> 0 then
    Result := False;
  if not Result then begin
    Log('Startup cleanup failed; leaving application installed to avoid a dangling startup task.');
    if not UninstallSilent then
      MsgBox('MaxPrioGames could not remove its startup task. Please run this uninstaller from the Windows account used to install it.', mbError, MB_OK);
  end;
end;
