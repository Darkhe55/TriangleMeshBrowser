#requires -Version 5.1
<#
.SYNOPSIS
    Prism Model Viewer - environment check.
.NOTES
    Pure ASCII; PowerShell 5.1 on Windows may default to non-UTF8 codepage.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  Prism Model Viewer - environment check" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

$ok = $true

# 1. vcpkg
$vcpkgCmd = Get-Command vcpkg -ErrorAction SilentlyContinue
$vcpkgDir = $null
foreach ($p in @(
    "$env:USERPROFILE\vcpkg",
    "$env:USERPROFILE\Documents\vcpkg",
    'C:\vcpkg'
)) {
    if (Test-Path (Join-Path $p 'vcpkg.exe')) { $vcpkgDir = $p; break }
}
if ($vcpkgCmd) {
    Write-Host "  [OK] vcpkg: $($vcpkgCmd.Source)" -ForegroundColor Green
} elseif ($vcpkgDir) {
    Write-Host "  [OK] vcpkg: $vcpkgDir\vcpkg.exe" -ForegroundColor Green
    Write-Host "    Tip: add to PATH or set VCPKG_ROOT" -ForegroundColor Yellow
} else {
    Write-Host "  [FAIL] vcpkg: not found" -ForegroundColor Red
    $ok = $false
}

# 2. cmake
$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCmd) {
    $cmakeVer = & $cmakeCmd.Source --version 2>&1 | Select-Object -First 1
    Write-Host "  [OK] cmake: $cmakeVer" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] cmake: not found (need 3.20+)" -ForegroundColor Red
    $ok = $false
}

# 3. MSVC
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msvcPath = $null
if (Test-Path $vswhere) {
    $msvcPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if ($msvcPath) {
    Write-Host "  [OK] MSVC:  $msvcPath" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] MSVC: not found (need VS 2019/2022/18 with C++)" -ForegroundColor Red
    $ok = $false
}

# 4. Key files
Write-Host ""
Write-Host "Project files:" -ForegroundColor Yellow
$keyFiles = @(
    'CMakeLists.txt',
    'vcpkg.json',
    'src\main.cpp',
    'src\app\Viewer.h',
    'src\renderer\MeshRenderer.h',
    'src\model\ModelLoader.h',
    'assets\models\cube.obj'
)
foreach ($f in $keyFiles) {
    $full = Join-Path $ScriptDir $f
    if (Test-Path $full) {
        Write-Host "  [OK] $f" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] $f (missing!)" -ForegroundColor Red
        $ok = $false
    }
}

Write-Host ""
if ($ok) {
    Write-Host "[OK] environment check passed" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next:" -ForegroundColor Cyan
    Write-Host "  Build:  .\build.ps1"
    Write-Host "  Run:    .\build.ps1 -Run"
    Write-Host ""
} else {
    Write-Host "[FAIL] environment check failed; fix items above" -ForegroundColor Red
    exit 1
}
