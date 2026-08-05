; Build:  ISCC.exe /DMyAppVersion=<ver> [/DSourceDir=<staged>] silencer-launcher.iss

#define MyAppName "Silencer Launcher"
#define MyAppExe "silencer-launcher.exe"
#define MyAppPublisher "Arsia Mons"
#define MyAppURL "https://arsiamons.com"

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\..\build-launcher\package\silencer-launcher"
#endif

[Setup]
; Stable across all releases. Do not regenerate — Windows keys upgrade and
; uninstall off this GUID, so a change orphans every prior install. Distinct
; from the game's F6A1252E-... GUID: the two are separate products and must be
; separately upgradable and uninstallable.
AppId={{D661CF3A-0B4F-4F6C-A653-AF573B11F5BE}
AppName={#MyAppName}
AppVerName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
; NOT {localappdata}\Programs\Silencer — that is the launcher's own default
; base_dir for GAME installs (see clients/launcher/src/config.cpp). Installing
; the launcher there would drop it on top of the channel dirs it manages.
DefaultDirName={localappdata}\Programs\Silencer Launcher
DefaultGroupName={#MyAppName}
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=Output
OutputBaseFilename=silencer-launcher-windows-x64-setup-{#MyAppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExe}
DisableDirPage=auto
DisableProgramGroupPage=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\{#MyAppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
; fonts\ and assets\ are the two dirs resolve_resource_dir() (src/main.cpp)
; probes beside the executable. Drop either and the launcher exits before a
; window opens — missing fonts are fatal, not a soft fallback.
Source: "{#SourceDir}\fonts\*"; DestDir: "{app}\fonts"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "Launch {#MyAppName}"; Flags: postinstall nowait skipifsilent

[UninstallDelete]
; Wipe the install dir, not just tracked files. Safe: the launcher's config
; lives at %APPDATA%\Silencer Launcher\ and the games it installed live under
; %LOCALAPPDATA%\Programs\Silencer\ — uninstalling the launcher must not
; delete either.
Type: filesandordirs; Name: "{app}"
