# Kill PowerShell and Python processes to free COM ports and stop agent/tooling.
# Use when you need a clean slate for testing (e.g. before firmware upload).
#
# Run from PowerShell: .\kill-for-test.ps1
# Run from cmd:       powershell -ExecutionPolicy Bypass -File kill-for-test.ps1
#
# To kill EVERYTHING including this session (terminal will close):
#   taskkill /F /IM python.exe; taskkill /F /IM powershell.exe

Write-Host "Killing Python processes..."
taskkill /F /IM python.exe 2>$null
if ($LASTEXITCODE -eq 0) { Write-Host "  Done." } else { Write-Host "  None running." }

Write-Host "Killing PowerShell processes (excluding this script's process)..."
$myPid = $PID
Get-Process -Name powershell -ErrorAction SilentlyContinue | Where-Object { $_.Id -ne $myPid } | ForEach-Object {
  Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
  Write-Host "  Stopped PID $($_.Id)"
}
Write-Host "Done. Current PowerShell session kept alive."
