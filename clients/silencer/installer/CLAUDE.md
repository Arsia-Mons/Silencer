# clients/silencer/installer — Windows installer

Inno Setup script that produces the per-user Windows installer
(`silencer-windows-x64-setup-<version>.exe`). Built by CI in
`.github/workflows/release.yml` after the zip Package step and
shipped as a second release asset alongside the existing zip.

## Why an installer (and not just the zip)

Users who extracted `silencer-windows-x64.zip` into
`%USERPROFILE%\Downloads\` hit auto-updater failures because every
extracted file carries Mark-of-the-Web, and Windows Defender's
real-time scanner holds short-lived handles on those files. Stage-2's
directory rename (`MoveFileA` on the install dir) requires every
descendant handle to have been opened with `FILE_SHARE_DELETE` — AV
scans don't, so the rename gets `STATUS_ACCESS_DENIED` and the update
fails silently. Installing into `%LOCALAPPDATA%\Programs\Silencer\`
sidesteps Mark-of-the-Web (the installer process writes the files,
not an extractor) and is outside Defender's Downloads hot path.

The installer is the recommended channel; the zip stays as a fallback
for advanced users.

## Building locally

```powershell
choco install innosetup -y
# Stage files the way release.yml does:
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"   # one-time
cd clients\silencer
cmake --preset win-ninja-release
cmake --build --preset win-ninja-release
$stage = "build-release/package/silencer"
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item build-release/Silencer.exe $stage/
Copy-Item "build-release/vcpkg_installed/x64-windows/bin/*.dll" $stage/
Copy-Item -Recurse ../../shared/assets $stage/assets

# Compile the installer:
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" `
  /DMyAppVersion=00045 installer/silencer.iss
```

Output lands in `clients/silencer/installer/Output/`.

## Install location

Per-user: `%LOCALAPPDATA%\Programs\Silencer\`. No UAC, no admin. Matches
how Chrome, Slack, Discord, and VS Code install for non-managed users.
The auto-updater (running as the user) can `MoveFileEx` files there
freely.

## Gotchas

- **`AppId` is permanent.** Windows keys upgrade detection and
  uninstaller registration off this GUID. Don't regenerate it — every
  prior install would be orphaned and the user would end up with two
  Start Menu entries.
- **No code signing.** SmartScreen warns *"Windows protected your PC"*
  on first launch until reputation accumulates. A standard code-signing
  cert (~$200/yr from Sectigo or DigiCert) eliminates the warning.
  Skip until reputation cost matters.
- **`SourceDir` macro.** Defaults to `..\..\..\build\package\silencer`
  (relative to the `.iss`). CI overrides via `/DSourceDir=...` if
  staging lives elsewhere.
