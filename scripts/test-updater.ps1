param(
    [string]$OldVer = "00023",
    [string]$NewVer = "00024"
)

$ErrorActionPreference = "Stop"
$Repo = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $Repo

$PlatformZip = "zsilencer-windows-x64.zip"

Write-Host "=== Building NEW version ($NewVer) ===" -ForegroundColor Cyan
cmake -B build-new -S . `
    -A x64 `
    -DZSILENCER_VERSION="$NewVer" `
    -DZSILENCER_LOBBY_HOST=127.0.0.1 `
    -DZSILENCER_LOBBY_PORT=15170
cmake --build build-new --config Release -j

# Stage files under a `zsilencer/` wrapper dir and zip that dir, matching
# release.yml's `Compress-Archive -Path "build/package/zsilencer"` layout.
# Stage-2 detects the single-top-dir wrapper and hoists its contents into
# the install path during the atomic swap.
$stage = "build-new/package/zsilencer"
if (Test-Path "build-new/package") { Remove-Item -Recurse -Force "build-new/package" }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item build-new/Release/zsilencer.exe $stage/ -Force
Copy-Item build-new/Release/*.dll        $stage/ -Force -ErrorAction SilentlyContinue
Copy-Item -Recurse -Force data           $stage/

New-Item -ItemType Directory -Force -Path test-update-host | Out-Null
if (Test-Path "test-update-host/$PlatformZip") { Remove-Item "test-update-host/$PlatformZip" }
Compress-Archive -Path $stage -DestinationPath "test-update-host/$PlatformZip" -Force

$sha = (Get-FileHash "test-update-host/$PlatformZip" -Algorithm SHA256).Hash.ToLower()
Write-Host "NEW zip sha256=$sha"

Write-Host "=== Building OLD version ($OldVer) ===" -ForegroundColor Cyan
cmake -B build-old -S . `
    -A x64 `
    -DZSILENCER_VERSION="$OldVer" `
    -DZSILENCER_LOBBY_HOST=127.0.0.1 `
    -DZSILENCER_LOBBY_PORT=15170
cmake --build build-old --config Release -j

# Stage the OLD install the same way production ships it, so the install
# parent directory actually has a `zsilencer/` dir that stage-2 can
# rename to `zsilencer.old` and replace atomically.
$oldInstall = "build-old/install/zsilencer"
if (Test-Path "build-old/install") { Remove-Item -Recurse -Force "build-old/install" }
New-Item -ItemType Directory -Force -Path $oldInstall | Out-Null
Copy-Item build-old/Release/zsilencer.exe $oldInstall/ -Force
Copy-Item build-old/Release/*.dll        $oldInstall/ -Force -ErrorAction SilentlyContinue
Copy-Item -Recurse -Force data           $oldInstall/

$manifest = @"
{
  "version":        "$NewVer",
  "macos_url":      "http://127.0.0.1:8000/$PlatformZip",
  "macos_sha256":   "$sha",
  "windows_url":    "http://127.0.0.1:8000/$PlatformZip",
  "windows_sha256": "$sha"
}
"@
Set-Content -Path update.json -Value $manifest

Write-Host "=== Starting HTTP server on :8000 ==="
$http = Start-Process -PassThru python -ArgumentList "-m","http.server","8000" -WorkingDirectory "$Repo/test-update-host"

Push-Location server
go build
Pop-Location

Write-Host "=== Starting lobby on :15170 ==="
$lobby = Start-Process -PassThru .\server\zsilencer-lobby.exe `
    -ArgumentList "-addr",":15170","-version","$NewVer","-update-manifest","$Repo\update.json"

Start-Sleep -Seconds 1
Write-Host "=== Launching OLD client — expect update modal ===" -ForegroundColor Cyan
try {
    & ".\$oldInstall\zsilencer.exe"
} finally {
    Stop-Process -Id $http.Id -ErrorAction SilentlyContinue
    Stop-Process -Id $lobby.Id -ErrorAction SilentlyContinue
}
