#!/usr/bin/env pwsh
# Visual demo of the stub GUI, PowerShell-native (Windows twin of demo.sh):
# stages a fake ~10MB update on a loopback server and runs the stub WITH its
# window, slowed by SILENCER_STUB_SLOW_MS so both phases are watchable
# (marquee "Checking..." -> "Updating... N%"). The "launcher" that starts at
# the end is the tiny test payload.
#
# Usage:  ./demo.ps1 [-SlowMs 2000]

[CmdletBinding()]
param([int]$SlowMs = 2000)

$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$build = Join-Path $here '..\..\build'
$stub = Join-Path $build 'silencer-launcher-stub.exe'
$payloadSrc = Join-Path $build 'stub-test-payload.exe'
if (-not (Test-Path $stub) -or -not (Test-Path $payloadSrc)) {
    Write-Host 'build first: clients/launcher-stub/build.ps1' -ForegroundColor Red; exit 1
}

$tmp = Join-Path $env:TEMP ("stub-demo-" + [IO.Path]::GetRandomFileName())
$www = Join-Path $tmp 'www'
$store = Join-Path $tmp 'store'
New-Item -ItemType Directory -Force $www, $store | Out-Null
$server = $null

try {
    # v1 = "installed"; v2 = the update, padded with 10MB of random bytes so
    # the throttled download visibly crawls.
    function New-Version($dir, $id) {
        New-Item -ItemType Directory -Force $dir | Out-Null
        Copy-Item $payloadSrc (Join-Path $dir 'silencer-launcher.exe')
        Set-Content (Join-Path $dir 'build-id.txt') $id
    }
    New-Version (Join-Path $store '00001') '00001'
    Set-Content (Join-Path $store 'current.txt') '00001'
    $v2 = Join-Path $tmp 'v2'
    New-Version $v2 '00002'
    $pad = New-Object byte[] (10MB)
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    $rng.GetBytes($pad)
    [IO.File]::WriteAllBytes((Join-Path $v2 'pad.bin'), $pad)

    $zip = Join-Path $www 'payload.zip'
    & "$env:SystemRoot\System32\tar.exe" -a -c -f $zip -C $v2 .
    $sha = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()

    # The npm shim dir puts an extensionless bash 'bun' first on PATH;
    # Start-Process needs a real executable.
    $bun = (Get-Command bun.exe -ErrorAction SilentlyContinue).Source
    if (-not $bun) { $bun = (Get-Command bun.cmd -ErrorAction SilentlyContinue).Source }
    if (-not $bun) { Write-Host 'bun not found on PATH' -ForegroundColor Red; exit 1 }

    $port = Get-Random -Minimum 20000 -Maximum 40000
    # Explicit quotes: -ArgumentList space-joins without quoting, and these
    # paths can contain spaces.
    $server = Start-Process $bun -ArgumentList @(
        ('"{0}"' -f (Join-Path $here 'serve.ts')), ('"{0}"' -f $www), $port
    ) -PassThru -WindowStyle Hidden
    Set-Content (Join-Path $www 'manifest.json') `
        "{`"build_id`": `"00002`", `"windows_payload_url`": `"http://127.0.0.1:$port/payload.zip`", `"windows_payload_sha256`": `"$sha`"}"
    $up = $false
    foreach ($i in 1..30) {
        try {
            Invoke-WebRequest "http://127.0.0.1:$port/manifest.json" -UseBasicParsing -TimeoutSec 2 | Out-Null
            $up = $true; break
        }
        catch { Start-Sleep -Milliseconds 100 }
    }
    if (-not $up) { Write-Host 'server never came up' -ForegroundColor Red; exit 1 }

    Write-Host "watch the window: ~$([int]($SlowMs / 1000))s of Checking (marquee), then Updating with a live percent bar" -ForegroundColor Cyan
    $env:SILENCER_STUB_STORE = $store
    $env:SILENCER_STUB_MANIFEST_URL = "http://127.0.0.1:$port/manifest.json"
    $env:SILENCER_STUB_SLOW_MS = "$SlowMs"
    # Start-Process -Wait: the call operator would NOT wait for a
    # GUI-subsystem exe, and the finally block would kill the server
    # mid-download.
    $p = Start-Process $stub -PassThru -Wait
    Write-Host "stub exited ($($p.ExitCode)); store ended at version $(Get-Content (Join-Path $store 'current.txt') -TotalCount 1)"
}
finally {
    $env:SILENCER_STUB_STORE = $null
    $env:SILENCER_STUB_MANIFEST_URL = $null
    $env:SILENCER_STUB_SLOW_MS = $null
    if ($server) { try { Stop-Process -Id $server.Id -Force -ErrorAction Stop } catch {} }
    Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
