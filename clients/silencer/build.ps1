#!/usr/bin/env pwsh
# Canonical Silencer client build entry point (Windows).
#
# All agents and humans build/configure the client ONLY through this
# script. It pins the newest installed Visual Studio (this codebase is
# MSVC-only (CLion's bundled MinGW cannot link it), resolves
# VCPKG_ROOT, and serializes against concurrent builds so CLion, CLI
# agents, and parallel agents can't corrupt the shared CMake cache.
#
# Usage:  ./build.ps1 [win-ninja|win-ninja-release|win-ninja-unity] [-Clean]
#   default preset : win-ninja (Debug)
#   -Clean         : wipe CMakeCache.txt + CMakeFiles (keep vcpkg_installed) first

[CmdletBinding()]
param(
    [ValidateSet('win-ninja', 'win-ninja-release', 'win-ninja-unity')]
    [string]$Preset = 'win-ninja',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Fail($msg) { Write-Host "build.ps1: $msg" -ForegroundColor Red; exit 1 }

$silencerDir = $PSScriptRoot
$binaryDir = switch ($Preset) {
    'win-ninja'         { Join-Path $silencerDir 'build' }
    'win-ninja-release' { Join-Path $silencerDir 'build-release' }
    'win-ninja-unity'   { Join-Path $silencerDir 'build-unity' }
}

# --- Resolve Visual Studio: newest install with the C++ x64 toolset ---
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { Fail "vswhere.exe not found at $vswhere - install Visual Studio (2022 BuildTools or newer)." }
$vsPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (-not $vsPath) { Fail "No Visual Studio install with the C++ x64 toolset found (vswhere)." }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { Fail "vcvars64.bat not found under $vsPath." }

# --- Resolve VCPKG_ROOT: process env -> persisted User env -> error ---
$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot) { $vcpkgRoot = [Environment]::GetEnvironmentVariable('VCPKG_ROOT', 'User') }
if (-not $vcpkgRoot -or -not (Test-Path (Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'))) {
    Fail "VCPKG_ROOT is not set or invalid. Set it persistently, then reopen your shell:`n  [Environment]::SetEnvironmentVariable('VCPKG_ROOT','<path-to-vcpkg>','User')"
}

# --- Abort if a build is actively running (idle CLion is fine; only a
#     live configure/compile corrupts the shared cache) ---
$busy = Get-Process -Name 'cmake', 'ninja', 'cl', 'MSBuild' -ErrorAction SilentlyContinue
if ($busy) {
    Fail ("a build is already running (" + (($busy | ForEach-Object Name | Sort-Object -Unique) -join ', ') + ") - wait for it to finish; concurrent configures corrupt the CMake cache.")
}

# --- Refuse if a previously-built instance is running: it locks the
#     link target, so a full compile would die at a cryptic LNK1168 ---
$running = @(Get-Process -Name 'Silencer' -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq (Join-Path $binaryDir 'Silencer.exe') })
if ($running) {
    Fail ("Silencer.exe from $binaryDir is running (PID " + (($running | ForEach-Object Id) -join ', ') + ") - it locks the link target. Stop that instance, then rebuild.")
}

# --- Cooperative build lock (atomic create; reclaim if owner is dead) ---
New-Item -ItemType Directory -Force -Path $binaryDir | Out-Null
$lockFile = Join-Path $binaryDir '.silencer-build.lock'
$lockStream = $null
$code = 1
try {
    try {
        $lockStream = [System.IO.File]::Open($lockFile, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    } catch [System.IO.IOException] {
        $owner = Get-Content $lockFile -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($owner -and (Get-Process -Id ([int]$owner) -ErrorAction SilentlyContinue)) {
            Fail "another build holds the lock (PID $owner) on $binaryDir - wait for it to finish."
        }
        Write-Host "build.ps1: stale lock from dead PID $owner - reclaiming." -ForegroundColor Yellow
        Remove-Item $lockFile -Force
        $lockStream = [System.IO.File]::Open($lockFile, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    }
    $writer = New-Object System.IO.StreamWriter($lockStream)
    $writer.WriteLine($PID); $writer.WriteLine((Get-Date -Format o)); $writer.Flush()

    if ($Clean) {
        Write-Host "build.ps1: cleaning cache in $binaryDir (keeping vcpkg_installed)" -ForegroundColor Cyan
        Remove-Item (Join-Path $binaryDir 'CMakeCache.txt') -Force -ErrorAction SilentlyContinue
        Remove-Item (Join-Path $binaryDir 'CMakeFiles') -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Host "build.ps1: VS         = $vsPath" -ForegroundColor DarkGray
    Write-Host "build.ps1: VCPKG_ROOT = $vcpkgRoot" -ForegroundColor DarkGray
    Write-Host "build.ps1: $Preset -> $binaryDir" -ForegroundColor DarkGray
    $inner = 'call "{0}" >nul && set "VCPKG_ROOT={1}" && cd /d "{2}" && cmake --preset {3} && cmake --build --preset {3}' -f $vcvars, $vcpkgRoot, $silencerDir, $Preset
    & cmd /c $inner
    $code = $LASTEXITCODE
}
finally {
    if ($lockStream) { $lockStream.Close() }
    Remove-Item $lockFile -Force -ErrorAction SilentlyContinue
}

if ($code -ne 0) { Fail "build failed (exit $code)." }
Write-Host "build.ps1: OK ($Preset)" -ForegroundColor Green
