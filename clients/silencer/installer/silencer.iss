; Inno Setup script for Silencer (Windows). Produces a per-user installer
; that lands at %LOCALAPPDATA%\Programs\Silencer\ — outside Defender's
; hot-path scanning of Downloads, where in-place directory renames fail.
;
; Build:
;   ISCC.exe /DMyAppVersion=00045 silencer.iss
;
; Source files come from `build\package\silencer\` (staged by release.yml's
; Package step or by infra/scripts/test-updater.ps1). Override with
;   /DSourceDir=path\to\staged\dir

#define MyAppName "Silencer"
#define MyAppPublisher "Arsia Mons"
#define MyAppURL "https://arsiamons.com"

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\..\build\package\silencer"
#endif

[Setup]
; Stable across all releases. Do not regenerate — Windows keys upgrade and
; uninstall off this GUID, so a change orphans every prior install.
AppId={{F6A1252E-1BF3-4768-ABD8-C1A9C140E459}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=Output
OutputBaseFilename=silencer-windows-x64-setup-{#MyAppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Silencer.exe
DisableDirPage=auto
DisableProgramGroupPage=yes
; CloseApplications: ask running Silencer to quit before replacing files,
; so an upgrade install doesn't fight with an open process.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\Silencer.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\Silencer.exe"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\Silencer.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Silencer.exe"; Description: "Launch {#MyAppName}"; Flags: postinstall nowait skipifsilent
