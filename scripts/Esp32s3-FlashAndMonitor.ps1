#Requires -Version 5.1
<#
.SYNOPSIS
    Build firmware, upload to ESP32-S3, then open the serial monitor (uses platformio.ini for COM port).

.DESCRIPTION
    For non-developers: run this from PowerShell after opening the project folder, or right-click the file
    and choose "Run with PowerShell". If Windows blocks scripts, run once in PowerShell:
    Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned

.NOTES
    Close Arduino IDE, PuTTY, other PlatformIO monitors, or anything using the same COM port before upload.
#>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

Write-Host ''
Write-Host '=== ESP32-S3: build, upload, serial monitor ===' -ForegroundColor Cyan
Write-Host 'Close any other program using this board USB serial port (Arduino, another monitor, PuTTY).' -ForegroundColor Yellow
Write-Host 'Waiting 5 seconds...' -ForegroundColor DarkGray
Start-Sleep -Seconds 5

Write-Host ''
Write-Host 'Building and uploading (this may take a few minutes)...' -ForegroundColor Green
python -m platformio run -e esp32s3 -t upload
if ($LASTEXITCODE -ne 0) {
    Write-Host ''
    Write-Host 'Build or upload failed. If you see "Access is denied" on COM, close apps using the port and try again.' -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ''
Write-Host 'Opening serial monitor at 115200 baud. Stop with Ctrl+C.' -ForegroundColor Green
python -m platformio device monitor -e esp32s3
