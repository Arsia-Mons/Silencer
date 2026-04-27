# Verifies a packaged Silencer Windows build is self-contained: every DLL
# imported by Silencer.exe (transitively) must resolve to a file inside the
# package dir or to a system DLL under C:\Windows\System32 (or SysWOW64).
#
# Usage: pwsh tests/e2e/check-bundle-windows.ps1 -PackageDir <dir>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $PackageDir
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $PackageDir)) {
    throw "Package dir not found: $PackageDir"
}
$PackageDir = (Resolve-Path $PackageDir).Path

$exe = Join-Path $PackageDir 'Silencer.exe'
if (-not (Test-Path $exe)) {
    throw "Silencer.exe not found in $PackageDir"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found at $vswhere — VS Build Tools required"
}
$vsInstall = & $vswhere -latest -property installationPath
$dumpbin = Get-ChildItem (Join-Path $vsInstall 'VC\Tools\MSVC') -Directory |
    ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe' } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1
if (-not $dumpbin) {
    throw "dumpbin.exe not found under $vsInstall\VC\Tools\MSVC"
}

function Get-ImportedDlls {
    param([string] $Path)
    $output = & $dumpbin /nologo /dependents $Path 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed (exit $LASTEXITCODE) for ${Path}:`n$($output -join "`n")"
    }
    return $output |
        Where-Object { $_ -match '^\s+\S+\.dll\s*$' } |
        ForEach-Object { $_.Trim() }
}

function Resolve-Dll {
    param([string] $Dll)
    # API Set DLLs (api-ms-win-*, ext-ms-*) are virtual — the OS loader
    # redirects them to the real implementation (ucrtbase.dll etc.) at
    # runtime, so they don't appear as files under System32. Guaranteed
    # present on Windows 10+; treat as system-resolved.
    if ($Dll -match '^(api|ext)-ms-') { return $true }
    $candidates = @(
        Join-Path $PackageDir $Dll
        Join-Path "$env:WINDIR\System32" $Dll
        Join-Path "$env:WINDIR\SysWOW64" $Dll
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    return $null
}

$checked = @{}
$queue = [System.Collections.Queue]::new()
$queue.Enqueue($exe)
$missing = @()

while ($queue.Count -gt 0) {
    $current = $queue.Dequeue()
    Write-Host "scanning $current"
    foreach ($dll in (Get-ImportedDlls $current)) {
        $key = $dll.ToLower()
        if ($checked.ContainsKey($key)) { continue }
        $checked[$key] = $true
        $resolved = Resolve-Dll $dll
        if (-not $resolved) {
            $missing += "$dll (referenced by $current)"
            continue
        }
        if ($resolved -is [string] -and $resolved.StartsWith($PackageDir, [System.StringComparison]::OrdinalIgnoreCase)) {
            $queue.Enqueue($resolved)
        }
    }
}

if ($missing.Count -gt 0) {
    Write-Host "----"
    Write-Host "FAIL: $($missing.Count) unbundled DLL reference(s) — present neither in $PackageDir nor in System32:"
    $missing | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "PASS: all imported DLLs resolve in package dir or system dirs"
