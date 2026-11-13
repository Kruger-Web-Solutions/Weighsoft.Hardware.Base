# Checks the screen of a W-Soft board over its USB port, without anybody at the board.
#
#   powershell -File tools/wsoft-ui/shot.ps1 -Port COM13 -Panel yellow -Do CRC,SHOT
#   powershell -File tools/wsoft-ui/shot.ps1 -Port COM11 -Panel wave -Do "TAP 156 213",SHOT,"TAP 238 212",SHOT
#
# Steps for -Do, run in order on one open port:
#   CRC          the board paints every test screen; each CRC must equal test/test_wsoft_ui/golden.h
#   SHOT         the screen as it is now, saved as a PNG in tools/wsoft-ui/out
#   SHOT n       test screen n, saved as a PNG
#   TAP x y      a tap at layout position x, y (320 x 240)
#   WAIT ms      wait
# The firmware side is ui_handle_command in lib/wsoft_esp/src/wsoft_ui.cpp.
param(
  [Parameter(Mandatory = $true)][string]$Port,
  [ValidateSet('yellow', 'wave')][string]$Panel = 'yellow',
  [string[]]$Do = @('CRC', 'SHOT'),
  [string]$Name = '',
  [int]$BootWaitMs = 4000
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here 'out'
New-Item -ItemType Directory -Force $out | Out-Null
if (-not $Name) { $Name = $Panel }

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class WsoftShot {
  // "SCR 1f*0b16 3*ffff ..." lines to 16-bit dots. Returns null when the dot count is wrong.
  public static ushort[] Decode(IEnumerable<string> lines, int width, int height) {
    var dots = new ushort[width * height];
    int at = 0;
    foreach (string line in lines) {
      if (!line.StartsWith("SCR ")) continue;
      foreach (string token in line.Substring(4).Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries)) {
        int star = token.IndexOf('*');
        if (star < 1) return null;
        long run = Convert.ToInt64(token.Substring(0, star), 16);
        ushort color = Convert.ToUInt16(token.Substring(star + 1), 16);
        if (at + run > dots.Length) return null;
        for (long i = 0; i < run; i++) dots[at++] = color;
      }
    }
    return at == dots.Length ? dots : null;
  }

  // CRC-32 (zip) of the dots, low byte first: the same as wsoft_crc32 on the board.
  public static uint Crc(ushort[] dots) {
    uint crc = 0xFFFFFFFF;
    foreach (ushort dot in dots) {
      foreach (byte b in new[] { (byte)(dot & 255), (byte)(dot >> 8) }) {
        crc ^= b;
        for (int k = 0; k < 8; k++) crc = (crc & 1) != 0 ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
      }
    }
    return ~crc;
  }

  public static void SavePng(ushort[] dots, int width, int height, string file) {
    using (var bitmap = new Bitmap(width, height, PixelFormat.Format24bppRgb)) {
      BitmapData data = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
      var row = new byte[data.Stride];
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          int v = dots[y * width + x], r = v >> 11, g = (v >> 5) & 63, b = v & 31;
          row[x * 3] = (byte)((b << 3) | (b >> 2));
          row[x * 3 + 1] = (byte)((g << 2) | (g >> 4));
          row[x * 3 + 2] = (byte)((r << 3) | (r >> 2));
        }
        Marshal.Copy(row, 0, IntPtr.Add(data.Scan0, y * data.Stride), row.Length);
      }
      bitmap.UnlockBits(data);
      bitmap.Save(file, ImageFormat.Png);
    }
  }
}
'@

# The CRCs the reference drawing gives (written by gen.mjs).
$goldenText = Get-Content (Join-Path $here '..\..\test\test_wsoft_ui\golden.h') -Raw
$array = if ($Panel -eq 'yellow') { 'GOLDEN_1X' } else { 'GOLDEN_2X' }
if ($goldenText -notmatch "$array\[\] = \{([^}]*)\}") { throw "no $array in golden.h" }
$golden = $Matches[1] -split ',' | ForEach-Object { $_.Trim().ToUpper().Replace('0X', '') } | Where-Object { $_ }

$serial = New-Object System.IO.Ports.SerialPort $Port, 115200
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.ReadTimeout = 500
$serial.NewLine = "`n"
$serial.Open()
$failed = 0
$shots = 0

# Reads lines until one starts with $until (returned as the last line) or the time is up.
function Read-Until([string]$until, [int]$timeoutMs) {
  $lines = New-Object System.Collections.Generic.List[string]
  $end = (Get-Date).AddMilliseconds($timeoutMs)
  while ((Get-Date) -lt $end) {
    try { $line = $serial.ReadLine().TrimEnd("`r") } catch [TimeoutException] { continue }
    $lines.Add($line)
    if ($line.StartsWith($until)) { return , $lines }
  }
  throw "no '$until' from the board within $timeoutMs ms (last line: $(if ($lines.Count) { $lines[$lines.Count - 1] } else { 'none' }))"
}

try {
  # Opening the port can restart some boards: let the screen come up, then drop what was sent.
  Start-Sleep -Milliseconds $BootWaitMs
  $serial.DiscardInBuffer()
  foreach ($step in $Do) {
    $parts = $step.Trim() -split '\s+'
    switch ($parts[0].ToUpper()) {
      'WAIT' { Start-Sleep -Milliseconds ([int]$parts[1]) }
      'TAP' {
        $serial.Write("SCREEN TAP $($parts[1]) $($parts[2])`n")
        $reply = Read-Until 'SCREEN_' 5000
        "tap $($parts[1]) $($parts[2]): $($reply[$reply.Count - 1])"
        if ($reply[$reply.Count - 1] -notmatch '^SCREEN_OK') { $failed++ }
      }
      'CRC' {
        $serial.Write("SCREEN CRC`n")
        $reply = Read-Until 'SCREEN_OK crc' 60000
        $got = @{}
        foreach ($line in $reply) { if ($line -match '^SCREEN_CRC (\d+) ([0-9A-F]{8})') { $got[[int]$Matches[1]] = $Matches[2] } }
        $bad = @()
        for ($n = 0; $n -lt $golden.Count; $n++) { if ($got[$n] -ne $golden[$n]) { $bad += "$n (board $($got[$n]), mock-up $($golden[$n]))" } }
        if ($bad.Count -eq 0) { "crc: all $($golden.Count) test screens on the board equal the mock-up" }
        else { $failed++; "crc: $($bad.Count) of $($golden.Count) test screens DIFFER: $($bad -join '; ')" }
      }
      'SHOT' {
        $which = if ($parts.Count -gt 1) { " $($parts[1])" } else { '' }
        $serial.Write("SCREEN SHOT$which`n")
        $begin = Read-Until 'SCREEN_' 5000
        if ($begin[$begin.Count - 1] -notmatch '^SCREEN_BEGIN (\d+) (\d+)') { $failed++; "shot: $($begin[$begin.Count - 1])"; break }
        $w = [int]$Matches[1]; $h = [int]$Matches[2]
        $body = Read-Until 'SCREEN_END' 180000
        $dots = [WsoftShot]::Decode($body, $w, $h)
        $file = Join-Path $out ("board-{0}-{1:00}{2}.png" -f $Name, $shots, $(if ($which) { "-preset$($which.Trim())" } else { '' }))
        $shots++
        if ($null -eq $dots) { $failed++; "shot: the picture did not arrive whole"; break }
        $crc = '{0:X8}' -f [WsoftShot]::Crc($dots)
        $sent = ($body[$body.Count - 1] -split ' ')[1]
        [WsoftShot]::SavePng($dots, $w, $h, $file)
        if ($crc -ne $sent) { $failed++; "shot: checksum differs (board $sent, received $crc): $file" }
        elseif ($which -and $crc -ne $golden[[int]$which]) { $failed++; "shot: test screen$which differs from the mock-up: $file" }
        else { "shot: ${w}x${h} ok $crc $file" }
      }
      default { throw "unknown step '$step'" }
    }
  }
} finally {
  $serial.Close()
}
if ($failed) { "FAILED: $failed step(s)"; exit 1 }
'ALL OK'
