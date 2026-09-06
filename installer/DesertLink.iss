#define MyAppName "DesertLink"
#define MyAppVersion "1.0.7"
#define MyAppPublisher "DesertLink"
#define MyAppExeName "DesertLink.exe"

[Setup]
AppId={{A6D07D02-8A96-4AA7-B7BC-9DD9AFA86042}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\DesertLink
DefaultGroupName=DesertLink
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=DesertLink_Setup_v1.0.7
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
InfoBeforeFile=INFO_BEFORE.txt

[Files]
; build\runtime is the official Electron Windows x64 runtime, prepared at build time.
; It already contains resources\app with DesertLink's JavaScript application files.
Source: "..\build\runtime\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\DesertLink"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autoprograms}\DesertLink"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch DesertLink"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
