# Lokaler TamaDigi-Installer fuer Windows (wird von start.bat aufgerufen).
#
#  1. Holt die fertig kompilierte Firmware von GitHub Pages (das baut der
#     Workflow bei jedem Push auf main). Ohne Internet bleibt die zuletzt
#     geladene Firmware liegen.
#  2. Startet einen kleinen Webserver auf http://localhost:8765 und oeffnet den
#     Installer in Chrome oder Edge. Web Serial (USB) funktioniert nur auf
#     https oder localhost, deshalb reicht es nicht, index.html per Doppelklick
#     zu oeffnen.
#
# Laeuft mit Windows PowerShell 5.1 (in jedem Windows 10/11 dabei) und
# PowerShell 7. Kein Python, kein Arduino noetig.
param(
  [int]$Port = 8765,
  [string]$Pages = 'https://joop0810.github.io/TamaDigi/web',
  [switch]$NoDownload,
  [switch]$NoBrowser
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$web  = Join-Path $root 'web'
if (-not (Test-Path (Join-Path $web 'index.html'))) {
  Write-Host "web\index.html nicht gefunden. start.bat muss im TamaDigi-Ordner liegen." -ForegroundColor Red
  exit 1
}

# ---- 1. Firmware holen -------------------------------------------------------
# Alle fuenf Dateien muessen zusammenpassen (manifest.json nennt die Hashes der
# vier Teile), also erst komplett in einen Zwischenordner, dann austauschen.
$fw = Join-Path $web 'firmware'
$loaded = $null
New-Item -ItemType Directory -Force -Path $fw | Out-Null
if (-not $NoDownload) {
  Write-Host "Lade die aktuelle Firmware von $Pages ..."
  try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}
  $tmp = Join-Path $fw '_neu'
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
    Write-Host "  Firmware $ver geladen." -ForegroundColor Green
    $loaded = $true
  } catch {
    Write-Host "  GitHub Pages nicht erreichbar ($($_.Exception.Message))." -ForegroundColor Yellow
    $loaded = $false
  } finally {
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
  }
}

# ---- 1b. ...oder aus tamadigi-firmware.zip ------------------------------------
# Bei einem privaten Repo ohne GitHub Pro gibt es keine Pages-Seite. Dann:
# GitHub -> Actions -> letzter Lauf -> Artifacts -> tamadigi-firmware laden und
# neben start.bat legen (oder im Download-Ordner lassen).
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
        if (-not (Test-Path (Join-Path $tmp "firmware/$p"))) { throw "$p fehlt im Zip" }
      }
      if (-not (Test-Path (Join-Path $tmp 'manifest.json'))) { throw "manifest.json fehlt im Zip" }
      foreach ($p in $parts) { Move-Item -Force (Join-Path $tmp "firmware/$p") (Join-Path $fw $p) }
      Move-Item -Force (Join-Path $tmp 'manifest.json') (Join-Path $web 'manifest.json')
      $ver = (Get-Content -Raw (Join-Path $web 'manifest.json') | ConvertFrom-Json).version
      Write-Host "  Firmware $ver aus $zip uebernommen." -ForegroundColor Green
    } catch {
      Write-Host "  $zip ist nicht brauchbar: $($_.Exception.Message)" -ForegroundColor Yellow
    } finally {
      if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    }
  } elseif (-not (Test-Path (Join-Path $fw 'app.bin'))) {
    Write-Host "  Kein tamadigi-firmware.zip gefunden. Laden: GitHub -> Actions -> letzter Lauf -> Artifacts."
  }
}
if (-not (Test-Path (Join-Path $fw 'app.bin'))) {
  Write-Host "  Hinweis: Noch keine Firmware vorhanden, der Knopf 'Firmware installieren' geht erst danach." -ForegroundColor Yellow
}

# ---- 2. Webserver -------------------------------------------------------------
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
  Write-Host "Port $Port ist belegt. Laeuft der Installer schon in einem anderen Fenster?" -ForegroundColor Red
  exit 1
}
$url = "http://localhost:$Port/web/"
Write-Host ""
Write-Host "TamaDigi-Installer laeuft: $url" -ForegroundColor Cyan
Write-Host "Zum Beenden dieses Fenster schliessen (oder Strg+C)."

if (-not $NoBrowser) {
  # Web Serial gibt es nur in Chrome und Edge, nicht in Firefox.
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
