param(
    [string]$OldVer = "00023",
    [string]$NewVer = "00024"
)

$ErrorActionPreference = "Stop"
$Repo = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $Repo

$PlatformZip = "zsilencer-windows-x64.zip"

# Resolve vcpkg toolchain. Prefer an existing install; bootstrap a repo-local
# one into .vcpkg/ if the user has none. Mirrors release.yml so the test
# harness and CI resolve SDL2/zlib/curl/minizip the same way.
#
# Inlined (not a function) because PowerShell functions capture native-command
# stdout into their output pipeline — bootstrap-vcpkg.bat's lines would get
# concatenated with the return value and turn $VcpkgRoot into an array.
$VcpkgRoot = $null
if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'))) {
    $VcpkgRoot = (Resolve-Path $env:VCPKG_ROOT).Path
} elseif ($env:VCPKG_INSTALLATION_ROOT -and (Test-Path (Join-Path $env:VCPKG_INSTALLATION_ROOT 'scripts/buildsystems/vcpkg.cmake'))) {
    $VcpkgRoot = (Resolve-Path $env:VCPKG_INSTALLATION_ROOT).Path
} else {
    $VcpkgRoot = Join-Path $Repo '.vcpkg'
    if (-not (Test-Path (Join-Path $VcpkgRoot 'scripts/buildsystems/vcpkg.cmake'))) {
        Write-Host "=== Bootstrapping vcpkg into $VcpkgRoot (one-time) ===" -ForegroundColor Cyan
        if (-not (Test-Path $VcpkgRoot)) {
            git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgRoot
            if ($LASTEXITCODE -ne 0) { throw "git clone vcpkg failed" }
        }
        & (Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
        if ($LASTEXITCODE -ne 0) { throw "bootstrap-vcpkg.bat failed" }
    }
}
$Toolchain = Join-Path $VcpkgRoot 'scripts/buildsystems/vcpkg.cmake'
if (-not (Test-Path $Toolchain)) { throw "vcpkg toolchain not found at $Toolchain" }
Write-Host "Using vcpkg toolchain: $Toolchain"

function Invoke-CMakeConfigure {
    param([string]$BuildDir, [string]$Version)
    # --fresh drops CMakeCache.txt + CMakeFiles/ so the toolchain flag
    # actually takes effect on a build dir seeded by a toolchain-less
    # configure. But it also nukes MSBuild .obj/.tlog files, turning every
    # run into a full rebuild. Only force it when the build dir clearly
    # hasn't been configured with vcpkg yet; presence of vcpkg_installed/
    # is a reliable signal that the previous configure was toolchain-aware,
    # so the cache + incremental compile are both trustworthy.
    $fresh = @()
    if (-not (Test-Path (Join-Path $BuildDir 'vcpkg_installed'))) {
        $fresh = @('--fresh')
    }
    # Quote every -D value. PowerShell fragments unquoted `127.0.0.1` into
    # `-D...=127` plus a stray `.0.0.1` positional arg, which cmake then warns
    # about and silently drops — leaving LOBBY_HOST baked in as "127".
    cmake -B $BuildDir -S . @fresh `
        -A x64 `
        "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
        "-DVCPKG_TARGET_TRIPLET=x64-windows" `
        "-DZSILENCER_VERSION=$Version" `
        "-DZSILENCER_LOBBY_HOST=127.0.0.1" `
        "-DZSILENCER_LOBBY_PORT=15170"
    if ($LASTEXITCODE -ne 0) { throw "cmake configure ($BuildDir) failed" }
    cmake --build $BuildDir --config Release -j
    if ($LASTEXITCODE -ne 0) { throw "cmake build ($BuildDir) failed" }
}

Write-Host "=== Building NEW version ($NewVer) ===" -ForegroundColor Cyan
Invoke-CMakeConfigure -BuildDir "build-new" -Version $NewVer

# Stage files under a `zsilencer/` wrapper dir and zip that dir, matching
# release.yml's `Compress-Archive -Path "build/package/zsilencer"` layout.
# Stage-2 detects the single-top-dir wrapper and hoists its contents into
# the install path during the atomic swap.
$stage = "build-new/package/zsilencer"
if (Test-Path "build-new/package") { Remove-Item -Recurse -Force "build-new/package" }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item build-new/Release/zsilencer.exe $stage/ -Force
# Ship the same vcpkg runtime DLLs production does, so the unzipped install
# can actually launch without the system having vcpkg.
$vcpkgBin = "build-new/vcpkg_installed/x64-windows/bin"
if (Test-Path $vcpkgBin) { Copy-Item "$vcpkgBin/*.dll" $stage/ -Force }
Copy-Item -Recurse -Force shared/assets  $stage/data

New-Item -ItemType Directory -Force -Path test-update-host | Out-Null
if (Test-Path "test-update-host/$PlatformZip") { Remove-Item "test-update-host/$PlatformZip" }
Compress-Archive -Path $stage -DestinationPath "test-update-host/$PlatformZip" -Force

$sha = (Get-FileHash "test-update-host/$PlatformZip" -Algorithm SHA256).Hash.ToLower()
Write-Host "NEW zip sha256=$sha"

Write-Host "=== Building OLD version ($OldVer) ===" -ForegroundColor Cyan
Invoke-CMakeConfigure -BuildDir "build-old" -Version $OldVer

# Stage the OLD install the same way production ships it, so the install
# parent directory actually has a `zsilencer/` dir that stage-2 can
# rename to `zsilencer.old` and replace atomically.
$oldInstall = "build-old/install/zsilencer"
if (Test-Path "build-old/install") { Remove-Item -Recurse -Force "build-old/install" }
New-Item -ItemType Directory -Force -Path $oldInstall | Out-Null
Copy-Item build-old/Release/zsilencer.exe $oldInstall/ -Force
$vcpkgBinOld = "build-old/vcpkg_installed/x64-windows/bin"
if (Test-Path $vcpkgBinOld) { Copy-Item "$vcpkgBinOld/*.dll" $oldInstall/ -Force }
Copy-Item -Recurse -Force shared/assets  $oldInstall/data

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
# -NoNewWindow streams HTTP logs into this console so you can see GETs for
# the update zip. Without it, Start-Process opens a detached window.
$http = Start-Process -PassThru -NoNewWindow python `
    -ArgumentList "-m","http.server","8000" `
    -WorkingDirectory "$Repo/test-update-host"

Push-Location server
go build
if ($LASTEXITCODE -ne 0) { Pop-Location; throw "go build failed" }
Pop-Location

Write-Host "=== Starting lobby on :15170 ==="
# Pass the manifest as a relative path ("update.json") with -WorkingDirectory
# instead of the full $Repo\update.json. Start-Process's -ArgumentList does
# NOT auto-quote array elements, so the space in "Space Command" silently
# truncates the path to "C:\Users\Space", LoadManifest fails, and the client
# gets a bare reject + legacy "software is out of date" message.
$lobby = Start-Process -PassThru -NoNewWindow .\server\zsilencer-lobby.exe `
    -ArgumentList "-addr",":15170","-version","$NewVer","-update-manifest","update.json" `
    -WorkingDirectory $Repo

Start-Sleep -Seconds 1
Write-Host "=== Launching OLD client — expect update modal ===" -ForegroundColor Cyan
try {
    # Start-Process -Wait because zsilencer.exe is built WIN32 subsystem;
    # PowerShell's `&` call operator returns immediately for GUI apps,
    # which would drop us straight into `finally` and kill the servers
    # before the client even gets past the lobby handshake.
    Start-Process -FilePath ".\$oldInstall\zsilencer.exe" -Wait
} finally {
    Stop-Process -Id $http.Id -ErrorAction SilentlyContinue
    Stop-Process -Id $lobby.Id -ErrorAction SilentlyContinue
}
