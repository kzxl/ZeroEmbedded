# ZeroEmbedded Interactive Device Demo Runner
$demoExe = Join-Path $PSScriptRoot "..\build\Release\zero_device_demo.exe"

if (-not (Test-Path $demoExe)) {
    Write-Host "[BUILD] Compiling zero_device_demo first..." -ForegroundColor Yellow
    & "$PSScriptRoot\build_and_test.ps1"
}

Write-Host "`nLaunching ZeroEmbedded Real-Time Edge Node Console...`n" -ForegroundColor Green
& $demoExe
