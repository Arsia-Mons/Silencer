#!/usr/bin/env pwsh
# Build the launcher bootstrap stub (Windows). The stub has no dependencies -
# no vcpkg - so this only resolves MSVC the same way clients/silencer/build.ps1
# does and runs CMake + Ninja.
#
# Usage:  ./build.ps1 [-Clean]

[CmdletBinding()]
param([switch]$Clean)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Fail($msg) { Write-Host "build.ps1: $msg" -ForegroundColor Red; exit 1 }

$dir = $PSScriptRoot
$build = Join-Path $dir 'build'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { Fail "vswhere.exe not found at $vswhere" }
$vsPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (-not $vsPath) { Fail "No Visual Studio install with the C++ x64 toolset found (vswhere)." }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { Fail "vcvars64.bat not found under $vsPath." }

if ($Clean) {
    Remove-Item (Join-Path $build 'CMakeCache.txt') -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $build 'CMakeFiles') -Recurse -Force -ErrorAction SilentlyContinue
}

$inner = 'call "{0}" >nul && cd /d "{1}" && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build' -f $vcvars, $dir
& cmd /c $inner
if ($LASTEXITCODE -ne 0) { Fail "build failed (exit $LASTEXITCODE)." }
Write-Host "build.ps1: OK -> $build" -ForegroundColor Green
