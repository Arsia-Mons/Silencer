# Automated end-to-end auto-updater test for Windows (headless; runs in CI).
#
# Mirror of infra/scripts/test-updater.sh. Proves the FULL self-update path:
# an installed client downloads a new build over loopback HTTP, verifies its
# sha256, self-replaces in place via stage-2, and the NEW version relaunches
# AUTOMATICALLY — confirmed by pinging the auto-relaunched process over the
# control socket and asserting its version.
#
# Drives the REAL cppx "Update" button headlessly; bypasses the lobby
# (show_update_screen injects a real url+sha256). The relaunched client comes
# up via env fallbacks (SILENCER_HEADLESS + SILENCER_CONTROL_PORT) that the
# binary honors when the matching flag is absent — stage-2 relaunches with no
# argv, but the environment IS inherited across CreateProcess.
param(
    [string]$OldVer = "00023",
    [string]$NewVer = "99999",
    [string]$OldBuildDir = "",
    [string]$NewBuildDir = "",
    [switch]$KeepWork
)

$ErrorActionPreference = "Stop"
$Repo = (Resolve-Path "$PSScriptRoot\..\..").Path
Set-Location $Repo
$ZipName = "silencer-update.zip"
$CliJs = "$Repo\clients\cli\index.ts"

function Pick-Port {
    $l = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
    $l.Start(); $p = $l.LocalEndpoint.Port; $l.Stop(); return $p
}
function Get-PingVersion([int]$Port) {
    $out = & bun $CliJs --port $Port ping 2>$null
    if (-not $out) { return "" }
    try { return [string]((($out -join "`n") | ConvertFrom-Json).version) } catch { return "" }
}
function Wait-ForLabel([int]$Port, [string]$Label, [int]$Tries = 150) {
    for ($i = 0; $i -lt $Tries; $i++) {
        $out = & bun $CliJs --port $Port inspect 2>$null
        if ($out) {
            try {
                $nodes = (($out -join "`n") | ConvertFrom-Json).nodes
                if ($nodes | Where-Object { $_.label -eq $Label -or $_.control_id -eq $Label }) { return $true }
            } catch {}
        }
        Start-Sleep -Milliseconds 100
    }
    return $false
}

# Lay a runnable install (silencer\ dir: exe + vcpkg DLLs + assets + fonts)
# matching release.yml's package layout, and return its exe path.
function Stage-Install([string]$BuildDir, [string]$DestDir) {
    $app = Join-Path $DestDir "silencer"
    New-Item -ItemType Directory -Force -Path $app | Out-Null
    Copy-Item (Join-Path $BuildDir "Silencer.exe") $app -Force
    $vcpkgBin = Join-Path $BuildDir "vcpkg_installed\x64-windows\bin"
    if (Test-Path $vcpkgBin) { Copy-Item "$vcpkgBin\*.dll" $app -Force }
    Copy-Item -Recurse -Force "$Repo\shared\assets" (Join-Path $app "assets")
    New-Item -ItemType Directory -Force -Path (Join-Path $app "assets\fonts") | Out-Null
    Copy-Item "$Repo\shared\fonts\*.otf" (Join-Path $app "assets\fonts") -Force
    return (Join-Path $app "Silencer.exe")
}

if (-not $OldBuildDir) { $OldBuildDir = "$Repo\e2e-build-old" }
if (-not $NewBuildDir) { $NewBuildDir = "$Repo\e2e-build-new" }
$buildOld = -not (Test-Path $OldBuildDir)
$buildNew = -not (Test-Path $NewBuildDir)

if ($buildOld -or $buildNew) {
    # --- Build environment: MSVC + bundled Ninja, then a vcpkg toolchain.
    # Skipped when already initialised (a Developer prompt, or CI's
    # ilammy/msvc-dev-cmd + VCPKG_INSTALLATION_ROOT).
    if (-not $env:VSINSTALLDIR) {
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found at $vswhere" }
        $vsInstall = (& $vswhere -latest -property installationPath).Trim()
        if (-not $vsInstall) { throw "no Visual Studio install found by vswhere" }
        $vcvars = Join-Path $vsInstall 'VC\Auxiliary\Build\vcvars64.bat'
        if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }
        & cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
        }
        $bundledNinja = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
        if (Test-Path (Join-Path $bundledNinja 'ninja.exe')) { $env:PATH = "$bundledNinja;$env:PATH" }
    }
    # Prefer VCPKG_INSTALLATION_ROOT (the runner's standalone vcpkg, C:\vcpkg) —
    # this is what build-silencer-windows uses. Do NOT prefer VCPKG_ROOT: vcvars/
    # ilammy-msvc-dev-cmd set it to the VS-bundled vcpkg (...\VC\vcpkg), a classic
    # install with no default-registry baseline, which fails manifest-mode resolve
    # with "requires a manifest with a specified baseline". Local dev (no
    # VCPKG_INSTALLATION_ROOT) falls through to VCPKG_ROOT / bootstrap.
    $VcpkgRoot = $null
    if ($env:VCPKG_INSTALLATION_ROOT -and (Test-Path (Join-Path $env:VCPKG_INSTALLATION_ROOT 'scripts/buildsystems/vcpkg.cmake'))) {
        $VcpkgRoot = (Resolve-Path $env:VCPKG_INSTALLATION_ROOT).Path
    } elseif ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'))) {
        $VcpkgRoot = (Resolve-Path $env:VCPKG_ROOT).Path
    } else {
        $VcpkgRoot = Join-Path $Repo '.vcpkg'
        if (-not (Test-Path (Join-Path $VcpkgRoot 'scripts/buildsystems/vcpkg.cmake'))) {
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

    function Build-Version([string]$BuildDir, [string]$Version) {
        Write-Host "=== building client $Version -> $BuildDir ===" -ForegroundColor Cyan
        $fresh = @(); if (-not (Test-Path (Join-Path $BuildDir 'vcpkg_installed'))) { $fresh = @('--fresh') }
        # Release + unity match release.yml so sccache reuses its objects; the
        # launcher is added only when CI has sccache enabled.
        $sccache = @()
        if ($env:SCCACHE_GHA_ENABLED -eq "true" -and (Get-Command sccache -ErrorAction SilentlyContinue)) {
            $sccache = @("-DCMAKE_C_COMPILER_LAUNCHER=sccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=sccache")
        }
        cmake -B $BuildDir -S clients/silencer @fresh -G Ninja `
            -DCMAKE_BUILD_TYPE=Release -DSILENCER_UNITY_BUILD=ON @sccache `
            "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" "-DVCPKG_TARGET_TRIPLET=x64-windows" `
            "-DSILENCER_VERSION=$Version" `
            "-DSILENCER_LOBBY_HOST=127.0.0.1" "-DSILENCER_LOBBY_PORT=15170"
        if ($LASTEXITCODE -ne 0) { throw "cmake configure ($BuildDir) failed" }
        cmake --build $BuildDir --target silencer -j
        if ($LASTEXITCODE -ne 0) { throw "cmake build ($BuildDir) failed" }
    }
    if ($buildOld) { Build-Version $OldBuildDir $OldVer }
    if ($buildNew) { Build-Version $NewBuildDir $NewVer }
}

$Work = Join-Path ([System.IO.Path]::GetTempPath()) ("silencer-updater-e2e-" + [System.Guid]::NewGuid().ToString("N").Substring(0, 8))
$HostDir = Join-Path $Work "host"
$InstallDir = Join-Path $Work "install"
New-Item -ItemType Directory -Force -Path $HostDir, $InstallDir | Out-Null

$http = $null; $old = $null; $CtrlQ = 0; $exit = 1
try {
    Write-Host "=== staging NEW ($NewVer) update package ==="
    $newPkg = Join-Path $Work "new-pkg"
    Stage-Install $NewBuildDir $newPkg | Out-Null
    $zipPath = Join-Path $HostDir $ZipName
    Compress-Archive -Path (Join-Path $newPkg "silencer") -DestinationPath $zipPath -Force
    # .NET SHA256 directly: Get-FileHash can fail to resolve after vcvars64's
    # env replay mangles PSModulePath.
    $fs = [System.IO.File]::OpenRead($zipPath)
    try { $sha = ([System.Security.Cryptography.SHA256]::Create().ComputeHash($fs) | ForEach-Object { '{0:x2}' -f $_ }) -join '' }
    finally { $fs.Dispose() }
    Write-Host "NEW zip: $zipPath  sha256=$sha"

    Write-Host "=== staging OLD ($OldVer) install (stage-2 swaps this in place) ==="
    $oldBin = Stage-Install $OldBuildDir $InstallDir
    $oldDir = Split-Path $oldBin -Parent

    $HttpPort = Pick-Port
    Write-Host "=== HTTP server on :$HttpPort serving $HostDir ==="
    $http = Start-Process -PassThru -WindowStyle Hidden python -ArgumentList "-m", "http.server", "$HttpPort" -WorkingDirectory $HostDir

    $CtrlP = Pick-Port   # OLD client, driven by us (flag)
    $CtrlQ = Pick-Port   # relaunched NEW client, observed by us (env)
    $url = "http://127.0.0.1:$HttpPort/$ZipName"

    Write-Host "=== launching OLD client (drive-port=$CtrlP, relaunch-env-port=$CtrlQ) ==="
    $env:SILENCER_HEADLESS = "1"
    $env:SILENCER_CONTROL_PORT = "$CtrlQ"
    $old = Start-Process -PassThru -WindowStyle Hidden -FilePath $oldBin `
        -ArgumentList "--headless", "--control-port", "$CtrlP" -WorkingDirectory $oldDir
    # Clear from THIS session so our own CLI calls don't default to Q (we always
    # pass --port, but keep it tidy). OLD already captured the env at launch, so
    # the relaunched NEW still inherits it.
    Remove-Item Env:\SILENCER_HEADLESS -ErrorAction SilentlyContinue
    Remove-Item Env:\SILENCER_CONTROL_PORT -ErrorAction SilentlyContinue

    $oldSeen = ""
    for ($i = 0; $i -lt 60; $i++) { $oldSeen = Get-PingVersion $CtrlP; if ($oldSeen) { break }; Start-Sleep -Milliseconds 500 }
    if (-not $oldSeen) { throw "OLD client never answered ping" }
    if ($oldSeen -eq $NewVer) { throw "OLD and NEW share version $NewVer; pick distinct OldVer/NewVer" }
    Write-Host "OLD client up, version=$oldSeen"

    Write-Host "=== driving the real update UI: present -> consent ==="
    & bun $CliJs --port $CtrlP show_update_screen --url $url --sha256 $sha | Out-Null
    & bun $CliJs --port $CtrlP wait_for_state --state UPDATING --timeout-ms 15000 | Out-Null
    if (-not (Wait-ForLabel $CtrlP "UpdateConsent")) { throw "Update button never rendered" }
    & bun $CliJs --port $CtrlP click --label UpdateConsent | Out-Null
    Write-Host "clicked Update — download -> verify -> STAGING -> stage-2 -> relaunch in flight"

    Write-Host "=== waiting for OLD client to exit (stage-2 spawned) ==="
    for ($i = 0; $i -lt 120; $i++) { if ($old.HasExited) { break }; Start-Sleep -Milliseconds 500 }
    if (-not $old.HasExited) { throw "OLD client did not exit — stage-2 handoff never happened" }
    $old = $null
    Write-Host "OLD client exited; awaiting auto-relaunched NEW client on :$CtrlQ"

    Write-Host "=== polling relaunched client until it reports the NEW version ==="
    $final = ""
    for ($i = 0; $i -lt 120; $i++) { $final = Get-PingVersion $CtrlQ; if ($final -eq $NewVer) { break }; Start-Sleep -Milliseconds 500 }

    if ($final -eq $NewVer) {
        Write-Host ""
        Write-Host "PASS test-updater: $oldSeen auto-updated and relaunched as $final" -ForegroundColor Green
        $exit = 0
    } else {
        Write-Host "FAIL: relaunched client never reported NEW version $NewVer (last ping: '$final')" -ForegroundColor Red
        $log = Join-Path $env:TEMP "silencer-update.log"
        if (Test-Path $log) { Write-Host "--- stage-2 log ---"; Get-Content $log -Tail 30 }
        $exit = 1
    }
} finally {
    if ($CtrlQ -gt 0) { & bun $CliJs --port $CtrlQ quit 2>$null | Out-Null }
    if ($old -and -not $old.HasExited) { Stop-Process -Id $old.Id -ErrorAction SilentlyContinue }
    if ($http) { Stop-Process -Id $http.Id -ErrorAction SilentlyContinue }
    if ($KeepWork) { Write-Host "kept scratch: $Work" } else { Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue }
}
exit $exit
