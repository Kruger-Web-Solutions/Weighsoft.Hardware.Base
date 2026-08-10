#Requires -Version 5.1
<#
.SYNOPSIS
  Operator QA smoke suite for Weighsoft.Hardware.Base relay board (QA-001..QA-012).

.DESCRIPTION
  Proves guest operator REST, auth gates, MQTT feature flag, and optional SPA shell reachability.
  Exit 0 = all required checks PASS; non-zero = failure.
  Blank-screen UI fix is browser-only - QA-012 only checks HTTP/JS 404.

.PARAMETER BaseUrl
  Board base URL (no trailing slash). Default: http://esp8266-relayboard.local
  Hostname, not an IP - DHCP moves the board (it has been .3.117, .2.67, .2.55).
  Pass -BaseUrl http://<ip> if mDNS is unavailable on this network.

.PARAMETER User
  Admin username for JWT checks. Default: admin

.PARAMETER Password
  Admin password. Default: admin (lab only - do not commit other secrets)

.PARAMETER SkipSpa
  Skip QA-012 SPA shell check.

.PARAMETER ExpectBoardId
  Board id from /rest/liveWeightDiscovery (last 6 of the MAC). QA-000 fails if a
  different device answers on this address. Pass "" to skip the identity check.

.EXAMPLE
  .\scripts\qa\operator-smoke.ps1

.EXAMPLE
  .\scripts\qa\operator-smoke.ps1 -BaseUrl http://192.168.2.55
#>
[CmdletBinding()]
param(
  [string]$BaseUrl = "http://esp8266-relayboard.local",
  [string]$User = "admin",
  [string]$Password = "admin",
  [switch]$SkipSpa,
  [string]$ExpectBoardId = "97cbc0"
)

$ErrorActionPreference = "Stop"
$BaseUrl = $BaseUrl.TrimEnd("/")

# Ring size from LIVE_WEIGHT_MAX_TX. A full log stops growing, which must not read as a miss.
$LiveWeightMaxTx = 40

$script:PassCount = 0
$script:FailCount = 0
$script:SkipCount = 0
$script:Results = New-Object System.Collections.Generic.List[string]

function Write-CheckResult {
  param(
    [string]$Id,
    [ValidateSet("PASS", "FAIL", "SKIP")]
    [string]$Status,
    [string]$Detail
  )
  $line = "$Status  $Id  $Detail"
  [void]$script:Results.Add($line)
  switch ($Status) {
    "PASS" { $script:PassCount++; Write-Host $line -ForegroundColor Green }
    "FAIL" { $script:FailCount++; Write-Host $line -ForegroundColor Red }
    "SKIP" { $script:SkipCount++; Write-Host $line -ForegroundColor Yellow }
  }
}

function Invoke-BoardRequest {
  param(
    [Parameter(Mandatory = $true)][string]$Method,
    [Parameter(Mandatory = $true)][string]$Path,
    [hashtable]$Headers = @{},
    [object]$Body = $null,
    [int]$TimeoutSec = 12
  )

  $uri = "$BaseUrl$Path"
  $result = [ordered]@{
    Ok         = $false
    StatusCode = 0
    Content    = $null
    Json       = $null
    Error      = $null
    Headers    = $null
  }

  $params = @{
    Uri             = $uri
    Method          = $Method
    TimeoutSec      = $TimeoutSec
    UseBasicParsing = $true
    Headers         = $Headers
  }

  if ($null -ne $Body) {
    $jsonBody = if ($Body -is [string]) { $Body } else { ($Body | ConvertTo-Json -Compress -Depth 8) }
    $params["ContentType"] = "application/json; charset=utf-8"
    $params["Body"] = [System.Text.Encoding]::UTF8.GetBytes($jsonBody)
  }

  try {
    $resp = Invoke-WebRequest @params
    $result.Ok = $true
    $result.StatusCode = [int]$resp.StatusCode
    $result.Content = $resp.Content
    $result.Headers = $resp.Headers
    if ($resp.Content) {
      $trim = $resp.Content.Trim()
      if ($trim.StartsWith("{") -or $trim.StartsWith("[")) {
        try { $result.Json = $resp.Content | ConvertFrom-Json } catch { }
      }
    }
  }
  catch {
    $ex = $_.Exception
    $result.Error = $ex.Message
    if ($ex.Response) {
      try {
        $result.StatusCode = [int]$ex.Response.StatusCode.value__
        if (-not $result.StatusCode) {
          $result.StatusCode = [int]$ex.Response.StatusCode
        }
      }
      catch {
        try { $result.StatusCode = [int]$ex.Response.StatusCode } catch { }
      }
      try {
        $stream = $ex.Response.GetResponseStream()
        if ($stream) {
          $reader = New-Object System.IO.StreamReader($stream)
          $result.Content = $reader.ReadToEnd()
          $reader.Close()
          if ($result.Content) {
            try { $result.Json = $result.Content | ConvertFrom-Json } catch { }
          }
        }
      }
      catch { }
    }
  }

  return [pscustomobject]$result
}

function Assert-Status {
  param(
    [string]$Id,
    [object]$Response,
    [int[]]$Expected,
    [string]$OkDetail,
    [string]$FailHint = ""
  )
  if ($Expected -contains $Response.StatusCode) {
    Write-CheckResult -Id $Id -Status "PASS" -Detail $OkDetail
    return $true
  }
  $got = if ($Response.StatusCode -gt 0) { "$($Response.StatusCode)" } else { "no-response" }
  $err = if ($Response.Error) { " ($($Response.Error))" } else { "" }
  $hint = if ($FailHint) { " | $FailHint" } else { "" }
  Write-CheckResult -Id $Id -Status "FAIL" -Detail ("expected {0} got {1}{2}{3}" -f ($Expected -join "/"), $got, $err, $hint)
  return $false
}

Write-Host ""
Write-Host "Weighsoft.Hardware.Base operator smoke"
Write-Host "BaseUrl: $BaseUrl"
Write-Host "User:    $User"
Write-Host ("-" * 56)

# Resolve the host first so the log shows WHICH machine answered.
# 2026-08-10: DHCP moved the board off .67 and another device answered ping there -
# looked exactly like a crashed web server. Always print the resolved address.
$resolvedIp = $null
try {
  $hostName = ([System.Uri]$BaseUrl).Host
  if ($hostName -as [System.Net.IPAddress]) {
    $resolvedIp = $hostName
  }
  else {
    $resolvedIp = ([System.Net.Dns]::GetHostAddresses($hostName) |
      Where-Object { $_.AddressFamily -eq "InterNetwork" } |
      Select-Object -First 1).IPAddressToString
  }
}
catch { }
if ($resolvedIp) { Write-Host ("Resolved: {0}" -f $resolvedIp) }
else { Write-Host "Resolved: (name did not resolve)" }
Write-Host ("-" * 56)

# QA-001 reachability
$root = Invoke-BoardRequest -Method GET -Path "/"
if ($root.StatusCode -ge 200 -and $root.StatusCode -lt 500) {
  Write-CheckResult -Id "QA-001" -Status "PASS" -Detail ("reachable HTTP {0}" -f $root.StatusCode)
}
else {
  Write-CheckResult -Id "QA-001" -Status "FAIL" -Detail ("board unreachable: {0}" -f $root.Error)
  Write-Host ""
  Write-Host ("SUMMARY  PASS={0}  FAIL={1}  SKIP={2}" -f $script:PassCount, $script:FailCount, $script:SkipCount) -ForegroundColor Red
  Write-Host "Board offline - remaining checks aborted."
  Write-Host "If the address is hardcoded somewhere, try: -BaseUrl http://esp8266-relayboard.local"
  exit 2
}

# QA-000 identity - is this actually OUR board, or a different device on that address?
$disc = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightDiscovery"
if (-not $ExpectBoardId) {
  Write-CheckResult -Id "QA-000" -Status "SKIP" -Detail "identity check disabled"
}
elseif ($disc.StatusCode -ne 200 -or $null -eq $disc.Json) {
  Write-CheckResult -Id "QA-000" -Status "FAIL" -Detail ("no discovery JSON (status={0}) - cannot confirm this is the board" -f $disc.StatusCode)
}
elseif ([string]$disc.Json.id -eq $ExpectBoardId) {
  Write-CheckResult -Id "QA-000" -Status "PASS" -Detail ("board id={0} host={1} ip={2}" -f $disc.Json.id, $disc.Json.host, $disc.Json.ip)
}
else {
  Write-CheckResult -Id "QA-000" -Status "FAIL" -Detail ("WRONG DEVICE: id={0} expected {1}. Something else holds this address." -f $disc.Json.id, $ExpectBoardId)
}

# QA-002 guest GET liveWeight
$lw = Invoke-BoardRequest -Method GET -Path "/rest/liveWeight"
[void](Assert-Status -Id "QA-002" -Response $lw -Expected @(200) -OkDetail "GET /rest/liveWeight 200")

# QA-003 guest GET products (200 + count >= 1)
$products = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightProducts"
if ($products.StatusCode -eq 200) {
  $count = 0
  if ($null -ne $products.Json.count) { $count = [int]$products.Json.count }
  elseif ($products.Json.products) { $count = @($products.Json.products).Count }
  if ($count -ge 1) {
    Write-CheckResult -Id "QA-003" -Status "PASS" -Detail ("GET products 200 count={0}" -f $count)
  }
  else {
    Write-CheckResult -Id "QA-003" -Status "FAIL" -Detail ("GET products 200 but count={0}; need >=1" -f $count)
  }
}
else {
  $got = if ($products.StatusCode -gt 0) { "$($products.StatusCode)" } else { "no-response" }
  Write-CheckResult -Id "QA-003" -Status "FAIL" -Detail ("GET products expected 200 got {0} {1}" -f $got, $products.Error)
}

$selectPlu = $null
if ($products.Json -and $products.Json.products) {
  $first = @($products.Json.products) | Select-Object -First 1
  if ($first -and $first.plu) { $selectPlu = [string]$first.plu }
}

# QA-004 guest GET transactions
$tx = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightTransactions"
[void](Assert-Status -Id "QA-004" -Response $tx -Expected @(200) -OkDetail "GET /rest/liveWeightTransactions 200")

# QA-005 guest POST liveWeight
$weightBody = @{
  weight    = 1.51
  last_line = "qa-smoke"
  count     = 1
}
$postLw = Invoke-BoardRequest -Method POST -Path "/rest/liveWeight" -Body $weightBody
[void](Assert-Status -Id "QA-005" -Response $postLw -Expected @(200) -OkDetail "POST /rest/liveWeight 200")

# QA-013 an on-screen "Next" must RECORD the weigh, not just bump the counter.
# 2026-08-10: it did not. A DI press logged via handleDiAction; the REST/UI path bumped
# the count and recorded nothing, so every weigh done from the screen was lost.
$txBefore = 0
$txPre = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightTransactions"
if ($txPre.StatusCode -eq 200 -and $null -ne $txPre.Json.count) {
  $txBefore = [int]$txPre.Json.count
}
$trig = Invoke-BoardRequest -Method POST -Path "/rest/liveWeight" -Body @{ trigger_action = "next" }
if ($trig.StatusCode -ne 200) {
  Write-CheckResult -Id "QA-013" -Status "FAIL" -Detail ("trigger_action=next returned {0}" -f $trig.StatusCode)
}
else {
  Start-Sleep -Milliseconds 1500   # the board defers the file write to its main loop
  $txPost = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightTransactions"
  $txAfter = if ($txPost.StatusCode -eq 200 -and $null -ne $txPost.Json.count) { [int]$txPost.Json.count } else { -1 }
  if ($txAfter -gt $txBefore) {
    Write-CheckResult -Id "QA-013" -Status "PASS" -Detail ("on-screen Next logged the weigh ({0} -> {1})" -f $txBefore, $txAfter)
  }
  elseif ($txBefore -ge $LiveWeightMaxTx -and $txAfter -eq $txBefore) {
    Write-CheckResult -Id "QA-013" -Status "PASS" -Detail ("log full at {0}, ring held steady - not a miss" -f $txAfter)
  }
  else {
    Write-CheckResult -Id "QA-013" -Status "FAIL" -Detail ("on-screen Next did NOT log the weigh ({0} -> {1}). Weighs done from the screen are being lost." -f $txBefore, $txAfter)
  }
}

# QA-014 CSV weigh report: header, a row per transaction, and a download filename.
# Row count is compared against the JSON endpoint so a truncated stream is caught -
# the report is streamed in chunks, and a silent short read would look like a small
# but plausible report rather than an error.
$rep = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightReport"
if ($rep.StatusCode -ne 200) {
  Write-CheckResult -Id "QA-014" -Status "FAIL" -Detail ("GET /rest/liveWeightReport expected 200 got {0}" -f $rep.StatusCode)
}
else {
  $csv = [string]$rep.Content
  $lines = @($csv -split "`r?`n" | Where-Object { $_.Trim().Length -gt 0 })
  $header = if ($lines.Count -gt 0) { $lines[0] } else { "" }
  $rows = [Math]::Max(0, $lines.Count - 1)

  $txNow = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightTransactions"
  $expectRows = if ($txNow.StatusCode -eq 200 -and $null -ne $txNow.Json.count) { [int]$txNow.Json.count } else { -1 }

  $disp = ""
  if ($rep.Headers -and $rep.Headers["Content-Disposition"]) {
    $disp = [string]$rep.Headers["Content-Disposition"]
  }

  if ($header -notlike "timestamp_ms,reason,plu,product,weight,count,total,unit*") {
    Write-CheckResult -Id "QA-014" -Status "FAIL" -Detail ("bad CSV header: '{0}'" -f $header)
  }
  elseif ($expectRows -ge 0 -and $rows -ne $expectRows) {
    Write-CheckResult -Id "QA-014" -Status "FAIL" -Detail ("CSV has {0} rows, board reports {1} transactions - stream truncated?" -f $rows, $expectRows)
  }
  elseif ($disp -notlike "*attachment*" -or $disp -notlike "*.csv*") {
    Write-CheckResult -Id "QA-014" -Status "FAIL" -Detail ("Content-Disposition missing/not a csv attachment: '{0}'" -f $disp)
  }
  else {
    Write-CheckResult -Id "QA-014" -Status "PASS" -Detail ("CSV report {0} rows, matches transactions, downloads as a file" -f $rows)
  }
}

# QA-006 guest product select
if ($selectPlu) {
  $sel = Invoke-BoardRequest -Method POST -Path "/rest/liveWeightProducts" -Body @{
    action = "select"
    plu    = $selectPlu
  }
  [void](Assert-Status -Id "QA-006" -Response $sel -Expected @(200) -OkDetail ("POST select plu={0} 200" -f $selectPlu))
}
else {
  Write-CheckResult -Id "QA-006" -Status "FAIL" -Detail "no PLU available to select"
}

# QA-007 guest relayBoard GET/POST + status
$rbGet = Invoke-BoardRequest -Method GET -Path "/rest/relayBoard"
$rbGetOk = Assert-Status -Id "QA-007a" -Response $rbGet -Expected @(200) -OkDetail "GET /rest/relayBoard 200"
if ($rbGetOk -and $rbGet.Json) {
  $echo = @{}
  foreach ($prop in $rbGet.Json.PSObject.Properties) {
    $echo[$prop.Name] = $prop.Value
  }
  $rbPost = Invoke-BoardRequest -Method POST -Path "/rest/relayBoard" -Body $echo
  [void](Assert-Status -Id "QA-007b" -Response $rbPost -Expected @(200) -OkDetail "POST /rest/relayBoard echo 200")
}
else {
  Write-CheckResult -Id "QA-007b" -Status "FAIL" -Detail "skipped POST - GET failed or empty body"
}

$status = Invoke-BoardRequest -Method GET -Path "/rest/relayBoardStatus"
$statusOk = Assert-Status -Id "QA-007c" -Response $status -Expected @(200) -OkDetail "GET /rest/relayBoardStatus 200"
if ($statusOk) {
  if ($null -ne $status.Json.free_heap) {
    Write-CheckResult -Id "QA-007d" -Status "PASS" -Detail ("free_heap={0}" -f $status.Json.free_heap)
  }
  else {
    Write-CheckResult -Id "QA-007d" -Status "FAIL" -Detail "free_heap missing in status JSON"
  }
}

# QA-008 guest liveWeightConfig -> 401
$cfgGuest = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightConfig"
[void](Assert-Status -Id "QA-008" -Response $cfgGuest -Expected @(401) -OkDetail "GET /rest/liveWeightConfig 401 guest")

# QA-009 guest product upsert -> 401
$upsert = Invoke-BoardRequest -Method POST -Path "/rest/liveWeightProducts" -Body @{
  action  = "upsert"
  plu     = "QA-SMOKE-SHOULD-401"
  product = "should-not-save"
  unit    = "kg"
}
[void](Assert-Status -Id "QA-009" -Response $upsert -Expected @(401) -OkDetail "POST upsert 401 guest")

# QA-010 admin JWT -> config GET 200
$signIn = Invoke-BoardRequest -Method POST -Path "/rest/signIn" -Body @{
  username = $User
  password = $Password
}
if ($signIn.StatusCode -eq 200 -and $signIn.Json.access_token) {
  $token = [string]$signIn.Json.access_token
  Write-CheckResult -Id "QA-010a" -Status "PASS" -Detail "POST /rest/signIn 200 JWT"
  $authHeaders = @{ Authorization = ("Bearer {0}" -f $token) }
  $cfgAdmin = Invoke-BoardRequest -Method GET -Path "/rest/liveWeightConfig" -Headers $authHeaders
  [void](Assert-Status -Id "QA-010b" -Response $cfgAdmin -Expected @(200) -OkDetail "GET /rest/liveWeightConfig 200 Bearer")
}
else {
  Write-CheckResult -Id "QA-010a" -Status "FAIL" -Detail ("signIn failed status={0} {1}" -f $signIn.StatusCode, $signIn.Error)
  Write-CheckResult -Id "QA-010b" -Status "FAIL" -Detail "skipped - no JWT"
}

# QA-011 features mqtt false
$feat = Invoke-BoardRequest -Method GET -Path "/rest/features"
if ($feat.StatusCode -eq 0 -and $feat.Error) {
  Write-CheckResult -Id "QA-011" -Status "SKIP" -Detail ("features endpoint unreachable: {0}" -f $feat.Error)
}
elseif ($feat.StatusCode -eq 404) {
  Write-CheckResult -Id "QA-011" -Status "SKIP" -Detail "/rest/features not found"
}
elseif ($feat.StatusCode -eq 200) {
  if ($null -eq $feat.Json.mqtt) {
    Write-CheckResult -Id "QA-011" -Status "SKIP" -Detail "mqtt field not present"
  }
  elseif ([bool]$feat.Json.mqtt -eq $false) {
    Write-CheckResult -Id "QA-011" -Status "PASS" -Detail "features.mqtt=false"
  }
  else {
    Write-CheckResult -Id "QA-011" -Status "FAIL" -Detail ("features.mqtt={0}; expected false FT_MQTT=0" -f $feat.Json.mqtt)
  }
}
else {
  Write-CheckResult -Id "QA-011" -Status "FAIL" -Detail ("GET /rest/features status={0}" -f $feat.StatusCode)
}

# QA-012 optional SPA shell
if ($SkipSpa) {
  Write-CheckResult -Id "QA-012" -Status "SKIP" -Detail "-SkipSpa set"
}
else {
  $spa = Invoke-BoardRequest -Method GET -Path "/project/live-weight/live" -TimeoutSec 20
  if ($spa.StatusCode -ne 200) {
    Write-CheckResult -Id "QA-012" -Status "FAIL" -Detail ("GET /project/live-weight/live status={0}. Blank UI is browser-only." -f $spa.StatusCode)
  }
  else {
    $html = [string]$spa.Content
    $dq = [char]34
    $sq = [char]39
    $pattern = '(?i)(?:src|href)=[' + $dq + $sq + ']([^' + $dq + $sq + ']+\.js[^' + $dq + $sq + ']*)[' + $dq + $sq + ']'
    $jsRefs = [regex]::Matches($html, $pattern)
    $jsFail = $false
    $checked = 0
    foreach ($m in $jsRefs) {
      $ref = $m.Groups[1].Value
      if ($ref -match '^(https?:)?//') { continue }
      if (-not $ref.StartsWith('/')) { $ref = '/' + $ref }
      $js = Invoke-BoardRequest -Method GET -Path $ref -TimeoutSec 20
      $checked++
      if ($js.StatusCode -eq 404) {
        Write-CheckResult -Id "QA-012" -Status "FAIL" -Detail ("JS 404: {0}. Blank UI is still browser-only." -f $ref)
        $jsFail = $true
        break
      }
    }
    if (-not $jsFail) {
      Write-CheckResult -Id "QA-012" -Status "PASS" -Detail ("SPA HTML 200; js checks={0}. Blank UI is browser-only; open in browser." -f $checked)
    }
  }
}

Write-Host ("-" * 56)
$color = if ($script:FailCount -gt 0) { "Red" } else { "Green" }
Write-Host ("SUMMARY  PASS={0}  FAIL={1}  SKIP={2}" -f $script:PassCount, $script:FailCount, $script:SkipCount) -ForegroundColor $color
Write-Host "Human still: RT-008 printer ticket (needs a printer on the LAN)."
Write-Host "Signed off: RT-007 DI physical (2026-08-10), Product LIVE catalog in browser."
Write-Host "See docs/QA-OPERATOR-CHECKLIST.md"
Write-Host ""

if ($script:FailCount -gt 0) {
  exit 1
}
exit 0
