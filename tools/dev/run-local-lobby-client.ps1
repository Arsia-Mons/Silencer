#!/usr/bin/env pwsh
# Build and run a local Silencer lobby paired with the dev client.

[CmdletBinding()]
param(
    [ValidateSet('win-ninja', 'win-ninja-release', 'win-ninja-unity')]
    [string]$Preset = 'win-ninja',
    [int]$LobbyPort = 15170
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Fail($message) {
    Write-Host "run-local-lobby-client.ps1: $message" -ForegroundColor Red
    exit 1
}

function Wait-ForTcp($hostName, $port, $process, $stdoutPath, $stderrPath) {
    for ($i = 0; $i -lt 80; $i++) {
        if ($process.HasExited) {
            Write-Host "Lobby exited before accepting connections." -ForegroundColor Red
            if (Test-Path $stdoutPath) { Get-Content $stdoutPath | Select-Object -Last 40 }
            if (Test-Path $stderrPath) { Get-Content $stderrPath | Select-Object -Last 40 }
            exit 1
        }
        $client = [System.Net.Sockets.TcpClient]::new()
        try {
            $connect = $client.BeginConnect($hostName, $port, $null, $null)
            if ($connect.AsyncWaitHandle.WaitOne(250)) {
                $client.EndConnect($connect)
                return
            }
        } catch {
        } finally {
            $client.Close()
        }
        Start-Sleep -Milliseconds 250
    }
    Fail "lobby did not open 127.0.0.1:$port"
}

function Get-ClientBinary($clientDir, $preset) {
    $buildDir = switch ($preset) {
        'win-ninja'         { Join-Path $clientDir 'build' }
        'win-ninja-release' { Join-Path $clientDir 'build-release' }
        'win-ninja-unity'   { Join-Path $clientDir 'build-unity' }
    }
    $binary = Join-Path $buildDir 'Silencer.exe'
    if (-not (Test-Path $binary)) {
        Fail "client binary missing at $binary after build"
    }
    return (Resolve-Path $binary).Path
}

if (-not (Get-Command go -ErrorAction SilentlyContinue)) {
    Fail "go is not on PATH"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$clientDir = Join-Path $repoRoot 'clients\silencer'
$lobbyDir = Join-Path $repoRoot 'services\lobby'
$runDir = Join-Path $lobbyDir 'run\local-dev'
$mapsDir = Join-Path $runDir 'maps'
$lobbyBin = Join-Path $runDir 'silencer-lobby.exe'
$lobbyDb = Join-Path $runDir 'lobby.json'
$lobbyOut = Join-Path $runDir 'lobby.out.log'
$lobbyErr = Join-Path $runDir 'lobby.err.log'
$playerAuthPort = $LobbyPort + 1
$mapApiPort = $LobbyPort + 2

New-Item -ItemType Directory -Force -Path $runDir, $mapsDir | Out-Null

Write-Host "Building client ($Preset)..."
& (Join-Path $clientDir 'build.ps1') $Preset
if ($LASTEXITCODE -ne 0) { Fail "client build failed" }
$clientBin = Get-ClientBinary $clientDir $Preset

Write-Host "Building lobby..."
Push-Location $lobbyDir
try {
    & go build -o $lobbyBin
    if ($LASTEXITCODE -ne 0) { Fail "lobby build failed" }
} finally {
    Pop-Location
}

Write-Host "Starting lobby on 127.0.0.1:$LobbyPort..."
$lobbyArgs = @(
    '-addr', ":$LobbyPort",
    '-db', "`"$lobbyDb`"",
    '-game-binary', "`"$clientBin`"",
    '-public-addr', '127.0.0.1',
    '-update-manifest=',
    '-player-auth-addr', ":$playerAuthPort",
    '-map-api-addr', ":$mapApiPort",
    '-maps-dir', "`"$mapsDir`""
)
$lobby = Start-Process `
    -FilePath $lobbyBin `
    -ArgumentList $lobbyArgs `
    -WorkingDirectory $lobbyDir `
    -RedirectStandardOutput $lobbyOut `
    -RedirectStandardError $lobbyErr `
    -WindowStyle Hidden `
    -PassThru

try {
    Wait-ForTcp '127.0.0.1' $LobbyPort $lobby $lobbyOut $lobbyErr
    Write-Host "Launching client against local lobby..."
    Write-Host "Lobby logs: $lobbyOut / $lobbyErr"
    Start-Process `
        -FilePath $clientBin `
        -ArgumentList @('--lobby-host', '127.0.0.1', '--lobby-port', "$LobbyPort") `
        -WorkingDirectory $repoRoot `
        -Wait
} finally {
    if ($lobby -and -not $lobby.HasExited) {
        Write-Host "Stopping lobby..."
        Stop-Process -Id $lobby.Id -Force -ErrorAction SilentlyContinue
        $lobby.WaitForExit(5000) | Out-Null
    }
}
