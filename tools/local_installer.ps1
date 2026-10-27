# Local TamaDigi installer for Windows (called by start.bat).
#
#  1. Fetches the ready-built firmware from GitHub Pages (the workflow builds
#     it on every push to main). Without internet, the last firmware
#     downloaded stays in place.
#  2. Starts a small web server on http://localhost:8765 and opens the
#     installer in Chrome or Edge. Web Serial (USB) only works over https or
#     on localhost, which is why double-clicking index.html is not enough.
#
# Runs on Windows PowerShell 5.1 (included in every Windows 10/11) and
# PowerShell 7. No Python and no Arduino needed.
param(
  [int]$Port = 8765,
  [string]$Pages = 'https://jaredbt.github.io/TamaDigi/web',
  [switch]$NoDownload,
  [switch]$NoBrowser
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$web  = Join-Path $root 'web'
if (-not (Test-Path (Join-Path $web 'index.html'))) {
  Write-Host "web\index.html not found. start.bat must be in the TamaDigi folder." -ForegroundColor Red
  exit 1
}

# ---- 1. Fetch the firmware ---------------------------------------------------
# All five files must match each other (manifest.json lists the hashes of the
# four parts), so download them all into a staging folder first, then swap.
$fw = Join-Path $web 'firmware'
$loaded = $null
New-Item -ItemType Directory -Force -Path $fw | Out-Null
if (-not $NoDownload) {
  Write-Host "Downloading the current firmware from $Pages ..."
  try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}
  $tmp = Join-Path $fw '_new'
  if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
  New-Item -ItemType Directory -Path $tmp | Out-Null
  $parts = 'bootloader.bin', 'partitions.bin', 'boot_app0.bin', 'app.bin'
  try {
    foreach ($p in $parts) {
      Invoke-WebRequest -UseBasicParsing -Uri "$Pages/firmware/$p" -OutFile (Join-Path $tmp $p)
    }
    Invoke-WebRequest -UseBasicParsing -Uri "$Pages/manifest.json" -OutFile (Join-Path $tmp 'manifest.json')
    foreach ($p in $parts) { Move-Item -Force (Join-Path $tmp $p) (Join-Path $fw $p) }
    Move-Item -Force (Join-Path $tmp 'manifest.json') (Join-Path $web 'manifest.json')
    $ver = (Get-Content -Raw (Join-Path $web 'manifest.json') | ConvertFrom-Json).version
    Write-Host "  Firmware $ver downloaded." -ForegroundColor Green
    $loaded = $true
  } catch {
    Write-Host "  GitHub Pages not reachable ($($_.Exception.Message))." -ForegroundColor Yellow
    $loaded = $false
  } finally {
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
  }
}

# ---- 1b. ...or from tamadigi-firmware.zip -------------------------------------
# A private repo without GitHub Pro has no Pages site. In that case:
# GitHub -> Actions -> latest run -> Artifacts -> download tamadigi-firmware and
# put it next to start.bat (or leave it in the Downloads folder).
if ($NoDownload -or ($loaded -eq $false)) {
  $zips = @(
    (Join-Path $root 'tamadigi-firmware.zip'),
    (Join-Path ([Environment]::GetFolderPath('UserProfile')) 'Downloads/tamadigi-firmware.zip')
  ) | Where-Object { Test-Path -LiteralPath $_ }
  $zip = $zips | Sort-Object { (Get-Item -LiteralPath $_).LastWriteTime } -Descending | Select-Object -First 1
  if ($zip) {
    $tmp = Join-Path $fw '_zip'
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    try {
      Expand-Archive -LiteralPath $zip -DestinationPath $tmp -Force
      $parts = 'bootloader.bin', 'partitions.bin', 'boot_app0.bin', 'app.bin'
      foreach ($p in $parts) {
        if (-not (Test-Path (Join-Path $tmp "firmware/$p"))) { throw "$p is missing from the zip" }
      }
      if (-not (Test-Path (Join-Path $tmp 'manifest.json'))) { throw "manifest.json is missing from the zip" }
      foreach ($p in $parts) { Move-Item -Force (Join-Path $tmp "firmware/$p") (Join-Path $fw $p) }
      Move-Item -Force (Join-Path $tmp 'manifest.json') (Join-Path $web 'manifest.json')
      $ver = (Get-Content -Raw (Join-Path $web 'manifest.json') | ConvertFrom-Json).version
      Write-Host "  Firmware $ver taken from $zip." -ForegroundColor Green
    } catch {
      Write-Host "  $zip is not usable: $($_.Exception.Message)" -ForegroundColor Yellow
    } finally {
      if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    }
  } elseif (-not (Test-Path (Join-Path $fw 'app.bin'))) {
    Write-Host "  No tamadigi-firmware.zip found. Download it: GitHub -> Actions -> latest run -> Artifacts."
  }
}
if (-not (Test-Path (Join-Path $fw 'app.bin'))) {
  Write-Host "  Note: no firmware yet; the 'Install firmware' button only works once there is." -ForegroundColor Yellow
}

# ---- 2. Web server ------------------------------------------------------------
$mime = @{
  '.html' = 'text/html; charset=utf-8'; '.js' = 'text/javascript; charset=utf-8'
  '.mjs' = 'text/javascript; charset=utf-8'; '.css' = 'text/css; charset=utf-8'
  '.json' = 'application/json; charset=utf-8'; '.png' = 'image/png'; '.svg' = 'image/svg+xml'
  '.ico' = 'image/x-icon'; '.woff2' = 'font/woff2'; '.woff' = 'font/woff'; '.ttf' = 'font/ttf'
  '.txt' = 'text/plain; charset=utf-8'; '.md' = 'text/plain; charset=utf-8'
}
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:$Port/")
try { $listener.Start() } catch {
  Write-Host "Port $Port is in use. Is the installer already running in another window?" -ForegroundColor Red
  exit 1
}
$url = "http://localhost:$Port/web/"
Write-Host ""
Write-Host "TamaDigi installer running: $url" -ForegroundColor Cyan
Write-Host "To stop it, close this window (or press Ctrl+C)."

if (-not $NoBrowser) {
  # Web Serial only exists in Chrome and Edge, not in Firefox.
  $opened = $false
  foreach ($b in 'chrome', 'msedge') {
    if (-not $opened) { try { Start-Process $b $url; $opened = $true } catch {} }
  }
  if (-not $opened) { Start-Process $url }
}

$rootPrefix = $root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
while ($listener.IsListening) {
  try { $ctx = $listener.GetContext() } catch { break }
  $res = $ctx.Response
  try {
    $rel = [Uri]::UnescapeDataString($ctx.Request.Url.AbsolutePath).TrimStart('/')
    if ($rel -eq '') { $res.Redirect('/web/'); continue }
    $full = [IO.Path]::GetFullPath((Join-Path $root $rel))
    if (-not $full.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -and $full -ne $root) {
      $res.StatusCode = 403; continue
    }
    if (Test-Path -LiteralPath $full -PathType Container) {
      if (-not $rel.EndsWith('/')) { $res.Redirect('/' + $rel + '/'); continue }
      $full = Join-Path $full 'index.html'
    }
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { $res.StatusCode = 404; continue }
    $ext = [IO.Path]::GetExtension($full).ToLowerInvariant()
    $type = $mime[$ext]
    if (-not $type) { $type = 'application/octet-stream' }
    $bytes = [IO.File]::ReadAllBytes($full)
    $res.ContentType = $type
    $res.Headers['Cache-Control'] = 'no-store'
    $res.ContentLength64 = $bytes.Length
    $res.OutputStream.Write($bytes, 0, $bytes.Length)
  } catch {
    try { $res.StatusCode = 500 } catch {}
  } finally {
    try { $res.Close() } catch {}
  }
}
