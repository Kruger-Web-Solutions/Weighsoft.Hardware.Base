# PowerShell script to stream test data on COM12

param(
    [string]$Port = "COM12",
    [int]$BaudRate = 9600,
    [int]$DelayMs = 500
)

Write-Host "=== Serial Data Streamer ===" -ForegroundColor Cyan
Write-Host "Port: $Port at $BaudRate baud" -ForegroundColor Yellow
Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
Write-Host ""

try {
    $serialPort = New-Object -TypeName System.IO.Ports.SerialPort -ArgumentList $Port,$BaudRate
    $serialPort.Open()
    
    Write-Host "Port opened successfully!" -ForegroundColor Green
    Write-Host "Streaming weight data..." -ForegroundColor Green
    Write-Host ""
    
    $counter = 0
    
    while ($true) {
        $weight = 100 + ($counter % 100)
        $line = "WEIGHT: $weight.$counter kg"
        $message = $line + "`r`n"
        
        $serialPort.Write($message)
        
        Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $line"
        
        $counter++
        Start-Sleep -Milliseconds $DelayMs
    }
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}
finally {
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        Write-Host "`nPort closed" -ForegroundColor Gray
    }
}
