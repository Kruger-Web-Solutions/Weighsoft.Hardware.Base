# Serial data streamer for COM12
param(
    [string]$Port = "COM13",
    [int]$BaudRate = 9600,
    [int]$DelayMs = 500
)

Write-Host "=== Serial Streamer ===" -ForegroundColor Cyan
Write-Host "Port: $Port at $BaudRate baud" -ForegroundColor Yellow
Write-Host ""

try {
    $serialPort = New-Object -TypeName System.IO.Ports.SerialPort -ArgumentList $Port,$BaudRate
    $serialPort.Open()
    
    Write-Host "Streaming data on $Port..." -ForegroundColor Green
    Write-Host ""
    
    $counter = 0
    
    while ($true) {
        # Simulate weight data in scale format: WN0000.04kg
        $weight = 0.01 * ($counter % 1000)  # 0.00 to 9.99
        $weightStr = $weight.ToString("0.00")
        $paddedWeight = $weightStr.PadLeft(7, '0')  # Pad to 7 chars: 0000.04
        $line = "WN${paddedWeight}kg"
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
    }
}
