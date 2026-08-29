#requires -Version 5.1
<#
.SYNOPSIS
    Prism Model Viewer - one-click build script.
.NOTES
    Pure ASCII; PowerShell 5.1 on Windows may default to non-UTF8 codepage.
    Usage:
        .\build.ps1                  # default Release
        .\build.ps1 -Clean           # clean build/ and rebuild
        .\build.ps1 -DebugBuild      # Debug build
        .\build.ps1 -Run             # build then launch
        .\build.ps1 -Reconfigure     # force cmake reconfigure
#>

[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$DebugBuild,
    [switch]$Run,
    [switch]$Reconfigure
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'Continue'

# ---------- paths ----------
$ScriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = $ScriptDir
$BuildDir    = Join-Path $ProjectRoot 'build'
$SourceDir   = $ProjectRoot
$ExePath     = if ($DebugBuild) { Join-Path $BuildDir 'Debug\PrismViewer.exe' }
               else        { Join-Path $BuildDir 'Release\PrismViewer.exe' }

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  Prism Model Viewer - build script" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Project: $ProjectRoot"
Write-Host "Build:   $BuildDir"
Write-Host "Config:  $(if ($DebugBuild) {'Debug'} else {'Release'})"
Write-Host ""

# ---------- locate vcpkg ----------
function Find-Vcpkg {
    $candidates = @(
        "$env:USERPROFILE\vcpkg",
        "$env:USERPROFILE\Documents\vcpkg",
        'C:\vcpkg',
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\18\BuildTools\VC\vcpkg",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\BuildTools\VC\vcpkg",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\vcpkg",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\vcpkg"
    )
    foreach ($p in $candidates) {
        if ($p -and (Test-Path (Join-Path $p 'vcpkg.exe'))) {
            return (Resolve-Path $p).Path
        }
    }
    $cmd = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($cmd) { return Split-Path -Parent $cmd.Source }
    return $null
}

# ---------- locate cmake ----------
function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        'C:\Program Files\CMake\bin\cmake.exe',
        'C:\Program Files (x86)\CMake\bin\cmake.exe'
    )
    foreach ($p in $candidates) { if (Test-Path $p) { return $p } }
    return $null
}

# ---------- locate MSVC ----------
function Find-Msvc {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($path) { return $path }
    return $null
}

Write-Host "[1/5] locating toolchain..." -ForegroundColor Yellow
$vcpkg = Find-Vcpkg
$cmake = Find-CMake
$msvc  = Find-Msvc

if (-not $vcpkg) { throw "[FAIL] vcpkg.exe not found; install vcpkg or add to PATH" }
if (-not $cmake) { throw "[FAIL] cmake not found; install CMake 3.20+ or add to PATH" }
if (-not $msvc)  { throw "[FAIL] MSVC not found; install VS 2019/2022/18 with C++ desktop" }

Write-Host "  [OK] vcpkg: $vcpkg"
Write-Host "  [OK] cmake: $cmake"
Write-Host "  [OK] MSVC:  $msvc"

# ---------- load MSVC env ----------
$vcvars = Join-Path $msvc 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "[FAIL] vcvars64.bat not found: $vcvars" }

Write-Host ""
Write-Host "[2/5] loading MSVC env..." -ForegroundColor Yellow
# cmd.exe supports '&&' inside /c argument; the string is opaque to PowerShell.
# Use ';' as a safer cross-Windows-version separator.
$envContent = & cmd /c "call `"$vcvars`" >NUL 2>&1; set" 2>&1
foreach ($line in $envContent) {
    if ($line -match '^([^=]+)=(.*)$') {
        $name  = $matches[1]
        $value = $matches[2]
        Set-Item -Path "Env:\$name" -Value $value -ErrorAction SilentlyContinue
    }
}

# ---------- triplet ----------
# x64-windows-static => all deps statically linked into the exe.
$triplet = 'x64-windows-static'
Write-Host "  [OK] vcpkg triplet: $triplet (full static)"

# ---------- clean ----------
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host ""
    Write-Host "[3/5] cleaning build/..." -ForegroundColor Yellow
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        & $python -c "import shutil; shutil.rmtree(r'$BuildDir', ignore_errors=True)" 2>&1 | Out-Null
    } else {
        cmd /c "rmdir /s /q `"$BuildDir`"" 2>&1 | Out-Null
    }
    Write-Host "  [OK] cleaned"
}

# ---------- configure ----------
if (-not (Test-Path $BuildDir) -or $Reconfigure) {
    Write-Host ""
    Write-Host "[4/5] CMake configure (first time / forced)..." -ForegroundColor Yellow
    New-Item -Path $BuildDir -ItemType Directory -Force | Out-Null
    $toolchain = Join-Path $vcpkg 'scripts\buildsystems\vcpkg.cmake'
    $overlayPorts = Join-Path $ProjectRoot 'vcpkg-overlays\ports'

    $configArgs = @(
        '-S', "`"$SourceDir`""
        '-B', "`"$BuildDir`""
        # 生成器必须与 vcpkg 缓存库的工具集一致 (v145),否则静态库 STL 符号链接失败;
        # 详见 README 构建章节。
        '-G', 'Visual Studio 18 2026'
        '-A', 'x64'
        "-DCMAKE_TOOLCHAIN_FILE=`"$toolchain`""
        "-DVCPKG_TARGET_TRIPLET=$triplet"
        "-DVCPKG_OVERLAY_PORTS=`"$overlayPorts`""
        '-DCMAKE_POLICY_DEFAULT_CMP0077=NEW'
    )
    Write-Host "  -> cmake $($configArgs -join ' ')"
    & $cmake @configArgs
    if ($LASTEXITCODE -ne 0) { throw "[FAIL] CMake configure failed, exit $LASTEXITCODE" }
} else {
    Write-Host ""
    Write-Host "[3/5] reusing existing build/ (use -Reconfigure to force)" -ForegroundColor Yellow
}

# ---------- build ----------
$config = if ($DebugBuild) { 'Debug' } else { 'Release' }
Write-Host ""
Write-Host "[4/5] building ($config)..." -ForegroundColor Yellow
Write-Host "  First build: 5-8 min (vcpkg install + link)"
Write-Host "  Incremental: seconds"
Write-Host ""

$buildArgs = @(
    '--build', "`"$BuildDir`""
    '--config', $config
    '--parallel'
)
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& $cmake @buildArgs
$sw.Stop()
$elapsed = $sw.Elapsed.ToString('mm\:ss')

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[FAIL] build failed (took $elapsed)" -ForegroundColor Red
    Write-Host "   Retry:`n   .\build.ps1 -Clean" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "[OK] build succeeded (took $elapsed)" -ForegroundColor Green
Write-Host "   Output: $ExePath"

if (Test-Path $ExePath) {
    $size = (Get-Item $ExePath).Length / 1MB
    Write-Host ("   Size: {0:N2} MB" -f $size) -ForegroundColor Green
}

# ---------- run ----------
if ($Run) {
    Write-Host ""
    Write-Host "[5/5] launching PrismViewer..." -ForegroundColor Yellow
    & $ExePath
} else {
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  Run:    &$ExePath"
    Write-Host "  With model:  &$ExePath `"assets\models\icosahedron.obj`""
    Write-Host "  Rebuild:    .\build.ps1 -Reconfigure"
    Write-Host "  Clean:      .\build.ps1 -Clean"
}
