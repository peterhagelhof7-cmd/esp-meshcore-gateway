# Web-Flasher aktualisieren (E35): Builds beider Varianten nach docs/fw/ kopieren
# und docs/fw/manifest.json mit Adressen, Größen und SHA-256 schreiben.
#   .\tools\flasher-update.ps1          # nutzt vorhandene Builds aus .pio\build
#   .\tools\flasher-update.ps1 -Build   # baut vorher beide Envs mit PlatformIO
# Prüft, dass jede firmware.bin das Merkmal ihres Boards in der aktuellen
# Version trägt - so landet nie ein veralteter oder vertauschter Build im Flasher.
param([switch]$Build)

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding($false)

$ini = [IO.File]::ReadAllText((Join-Path $root 'platformio.ini'), $utf8)
if ($ini -notmatch "GATEWAY_VERSION='`"([^`"]+)`"'") { throw 'GATEWAY_VERSION nicht gefunden' }
$version = $Matches[1]

# psram_cap = eFuse PSRAM_CAP des ESP32-S3 (esptool: 1 = 8 MB, 2 = 2 MB)
$variants = [ordered]@{
    v4 = @{ Env = 'gateway_heltec_v4';    Id = 'MESHCORE-GW-HELTEC-V4';    Name = 'Heltec WiFi LoRa 32 V4 (ESP32-S3R2, 2 MB PSRAM)'; PsramCap = 2 }
    r8 = @{ Env = 'gateway_heltec_v4_r8'; Id = 'MESHCORE-GW-HELTEC-V4-R8'; Name = 'Heltec WiFi LoRa 32 V4-R8 (ESP32-S3R8, 8 MB PSRAM)'; PsramCap = 1 }
}

if ($Build) {
    foreach ($v in $variants.Values) {
        & pio run -d $root -e $v.Env
        if ($LASTEXITCODE -ne 0) { throw "Build $($v.Env) fehlgeschlagen" }
    }
}

# Layout default_16MB.csv: Bootloader 0x0 (ESP32-S3), Partitionstabelle 0x8000,
# otadata 0xe000 (boot_app0 = "starte app0"), app0 0x10000
$bootApp0 = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin'
if (-not (Test-Path $bootApp0)) { throw "boot_app0.bin fehlt: $bootApp0" }

$sha = [Security.Cryptography.SHA256]::Create()
$latin1 = [Text.Encoding]::GetEncoding(28591)
$out = [ordered]@{ version = $version; built = (Get-Date -Format 'yyyy-MM-dd'); flash_size = '16MB'; variants = [ordered]@{} }

foreach ($key in $variants.Keys) {
    $v = $variants[$key]
    $bdir = Join-Path $root ".pio\build\$($v.Env)"
    $parts = @(
        @{ Name = 'bootloader.bin'; Src = Join-Path $bdir 'bootloader.bin'; Address = 0x0 },
        @{ Name = 'partitions.bin'; Src = Join-Path $bdir 'partitions.bin'; Address = 0x8000 },
        @{ Name = 'boot_app0.bin';  Src = $bootApp0;                        Address = 0xe000 },
        @{ Name = 'firmware.bin';   Src = Join-Path $bdir 'firmware.bin';   Address = 0x10000 }
    )
    foreach ($p in $parts) { if (-not (Test-Path $p.Src)) { throw "fehlt: $($p.Src) (erst bauen: -Build)" } }

    $fw = [IO.File]::ReadAllBytes($parts[3].Src)
    $marker = "MCGW-FW-ID:$($v.Id):${version}:MCGW-FW-END"
    if (-not $latin1.GetString($fw).Contains($marker)) {
        throw "$($v.Env): firmware.bin trägt nicht '$marker' (veralteter Build? -Build nutzen)"
    }

    $dst = Join-Path $root "docs\fw\$key"
    New-Item -ItemType Directory -Force $dst | Out-Null
    $files = @()
    foreach ($p in $parts) {
        $bytes = [IO.File]::ReadAllBytes($p.Src)
        [IO.File]::WriteAllBytes((Join-Path $dst $p.Name), $bytes)
        $hash = -join ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') })
        $files += [ordered]@{ name = $p.Name; address = $p.Address; size = $bytes.Length; sha256 = $hash }
    }
    $out.variants[$key] = [ordered]@{ name = $v.Name; project = $v.Id; psram_cap = $v.PsramCap; files = $files }
    Write-Output ("{0}: {1} Dateien, Firmware {2:N0} Byte, Merkmal ok" -f $key, $files.Count, $fw.Length)
}

$json = $out | ConvertTo-Json -Depth 6
[IO.File]::WriteAllText((Join-Path $root 'docs\fw\manifest.json'), $json + "`n", $utf8)
Write-Output "docs\fw\manifest.json geschrieben (Version $version)"
