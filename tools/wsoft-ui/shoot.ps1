# Renders test screens of the mock-up to PNG with headless Edge, to look at them.
#   powershell -File tools/wsoft-ui/shoot.ps1 0 1 2      (preset numbers; yellow board 3x, Waveshare 1x)
param([Parameter(ValueFromRemainingArguments = $true)][int[]]$Presets = @(0))
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here 'out'
node (Join-Path $here 'build.mjs') | Out-Null
$edge = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
$url = 'file:///' + ($out -replace '\\', '/') + '/preview.html'
foreach ($n in $Presets) {
  $png = Join-Path $out "preset-$n.png"
  Remove-Item $png -ErrorAction SilentlyContinue
  & $edge --headless=new --disable-gpu --hide-scrollbars "--user-data-dir=$out\edge-profile" --window-size=980,1250 `
    --virtual-time-budget=9000 "--screenshot=$png" "$url#dbg-$n" 2>$null | Out-Null
  Start-Sleep -Milliseconds 300
  "$n $(Test-Path $png)"
}
