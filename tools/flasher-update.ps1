# Web-Flasher aktualisieren (E35, E38): Builds beider Varianten nach
# docs/fw/<Version>/<Variante>/ kopieren und die Version in docs/fw/manifest.json
# mit Adressen, Größen und SHA-256 eintragen. Andere Versionen bleiben im
# Flasher zur Auswahl (Status "stable" = bewährt, "test" = in Erprobung).
#   .\tools\flasher-update.ps1                  # vorhandene Builds aus .pio\build, Status test
#   .\tools\flasher-update.ps1 -Build           # baut vorher beide Envs mit PlatformIO
#   .\tools\flasher-update.ps1 -Status stable   # aktuelle Version gleich als bewährt eintragen
#   .\tools\flasher-update.ps1 -Promote 0.8.0   # nur Status einer Version auf bewährt setzen
#   .\tools\flasher-update.ps1 -Remove 0.7.6    # Version samt Dateien aus dem Flasher nehmen
# Prüft, dass jede firmware.bin das Merkmal ihres Boards in der aktuellen
# Version trägt - so landet nie ein veralteter oder vertauschter Build im Flasher.
param(
    [switch]$Build,
    [ValidateSet('test', 'stable')][string]$Status = 'test',
    [string]$Promote,
    [string]$Remove
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding($false)
$fwDir = Join-Path $root 'docs\fw'
$manifestPath = Join-Path $fwDir 'manifest.json'

# psram_cap = eFuse PSRAM_CAP des ESP32-S3 (esptool: 1 = 8 MB, 2 = 2 MB)
$variants = [ordered]@{
    v4 = @{ Env = 'gateway_heltec_v4';    Id = 'MESHCORE-GW-HELTEC-V4';    Name = 'Heltec WiFi LoRa 32 V4 (ESP32-S3R2, 2 MB PSRAM)'; PsramCap = 2 }
    r8 = @{ Env = 'gateway_heltec_v4_r8'; Id = 'MESHCORE-GW-HELTEC-V4-R8'; Name = 'Heltec WiFi LoRa 32 V4-R8 (ESP32-S3R8, 8 MB PSRAM)'; PsramCap = 1 }
}

# MeshCore-Version, die zu einem Gateway-Stand gehört (Submodul-Commit -> Tag repeater-vX.Y.Z)
function Get-MeshCoreVersion([string]$rev) {
    $line = & git -C $root ls-tree $rev external/MeshCore
    if (-not $line) { return '' }
    $sha = ($line -split '\s+')[2]
    $tag = & git -C (Join-Path $root 'external\MeshCore') describe --tags --exact-match --match 'repeater-v*' $sha 2>$null
    if ($tag) { return ($tag -replace '^repeater-v', '') }
    return ''
}

function Read-Releases {
    if (-not (Test-Path $manifestPath)) { return @() }
    $m = [IO.File]::ReadAllText($manifestPath, $utf8) | ConvertFrom-Json
    if ($m.releases) { return @($m.releases) }
    # Altes Format (eine Version, Dateien unter fw/<Variante>/): in Unterordner umziehen
    $old = $m.version
    $dst = Join-Path $fwDir $old
    New-Item -ItemType Directory -Force $dst | Out-Null
    foreach ($key in $variants.Keys) {
        $src = Join-Path $fwDir $key
        if (Test-Path $src) { Move-Item $src (Join-Path $dst $key) }
    }
    Write-Host "altes Manifest umgestellt: $old -> fw\$old\"   # nicht Write-Output: wäre Teil des Rückgabewerts
    return @([pscustomobject][ordered]@{
        version = $old; built = $m.built; status = 'stable'
        meshcore = (Get-MeshCoreVersion "v$old"); variants = $m.variants
    })
}

function Write-Manifest($releases) {
    # neueste Version zuerst
    $sorted = @($releases | Sort-Object { [version]$_.version } -Descending)
    $out = [ordered]@{ flash_size = '16MB'; releases = $sorted }
    $json = ConvertTo-Json -InputObject $out -Depth 8
    [IO.File]::WriteAllText($manifestPath, $json + "`n", $utf8)
    Write-Output ('docs\fw\manifest.json geschrieben: ' + (($sorted | ForEach-Object { "$($_.version) ($($_.status))" }) -join ', '))
}

$releases = @(Read-Releases)

if ($Promote) {
    $r = $releases | Where-Object { $_.version -eq $Promote }
    if (-not $r) { throw "Version $Promote nicht im Flasher" }
    $r.status = 'stable'
    Write-Manifest $releases
    return
}
if ($Remove) {
    if (-not ($releases | Where-Object { $_.version -eq $Remove })) { throw "Version $Remove nicht im Flasher" }
    $releases = @($releases | Where-Object { $_.version -ne $Remove })
    if ($releases.Count -eq 0) { throw 'Mindestens eine Version muss im Flasher bleiben' }
    Remove-Item -Recurse -Force (Join-Path $fwDir $Remove)
    Write-Manifest $releases
    return
}

$ini = [IO.File]::ReadAllText((Join-Path $root 'platformio.ini'), $utf8)
if ($ini -notmatch "GATEWAY_VERSION='`"([^`"]+)`"'") { throw 'GATEWAY_VERSION nicht gefunden' }
$version = $Matches[1]

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
$outVariants = [ordered]@{}

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

    $dst = Join-Path $fwDir "$version\$key"
    New-Item -ItemType Directory -Force $dst | Out-Null
    $files = @()
    foreach ($p in $parts) {
        $bytes = [IO.File]::ReadAllBytes($p.Src)
        [IO.File]::WriteAllBytes((Join-Path $dst $p.Name), $bytes)
        $hash = -join ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') })
        $files += [ordered]@{ name = $p.Name; address = $p.Address; size = $bytes.Length; sha256 = $hash }
    }
    $outVariants[$key] = [ordered]@{ name = $v.Name; project = $v.Id; psram_cap = $v.PsramCap; files = $files }
    Write-Output ("{0} {1}: {2} Dateien, Firmware {3:N0} Byte, Merkmal ok" -f $version, $key, $files.Count, $fw.Length)
}

$new = [pscustomobject][ordered]@{
    version = $version; built = (Get-Date -Format 'yyyy-MM-dd'); status = $Status
    meshcore = (Get-MeshCoreVersion 'HEAD'); variants = $outVariants
}
$releases = @($releases | Where-Object { $_.version -ne $version }) + $new
Write-Manifest $releases
