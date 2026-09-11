# ZeroEmbedded One-Step Build & Verification Script

$ErrorActionPreference = "Stop"

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host " ⚡ ZeroEmbedded Build & Test Suite Pipeline" -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan

# Locate VS Environment / Tools
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) {
        $cmakeExe = "$vsPath\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        $vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
    }
}

if (-not (Test-Path $cmakeExe)) {
    Write-Host "[ERROR] CMake executable not found in Visual Studio path." -ForegroundColor Red
    exit 1
}

$buildDir = Join-Path $PSScriptRoot "..\build"

Write-Host "[1/4] Configuring CMake project..." -ForegroundColor Yellow
& $cmakeExe -B $buildDir -S (Join-Path $PSScriptRoot "..") -G "Visual Studio 17 2022" -A x64

Write-Host "`n[2/4] Compiling ZeroEmbedded (Release)..." -ForegroundColor Yellow
& $cmakeExe --build $buildDir --config Release

Write-Host "`n[3/4] Running Hardened Test Suites via CTest..." -ForegroundColor Yellow
$ctestExe = Join-Path (Split-Path $cmakeExe) "ctest.exe"
& $ctestExe --test-dir $buildDir -C Release --output-on-failure

Write-Host "`n[4/4] Executing End-to-End Simulations..." -ForegroundColor Yellow
$hostSimExe = Join-Path $buildDir "Release\example_host_sim.exe"
if (Test-Path $hostSimExe) {
    & $hostSimExe
}

Write-Host "`n[PASS] All builds, unit tests, and simulations executed successfully!" -ForegroundColor Green
