# GitHub-Release mit OTA-fähigen App-Images (E45): nimmt die firmware.bin beider
# Varianten aus docs/fw/<Version>/ (also genau die Dateien im Web-Flasher),
# prüft Merkmal und SHA-256 gegen das Manifest und hängt sie als
# meshcore-gateway-heltec-v4[-r8]-<Version>.bin an das Release zum Tag v<Version>.
# Status "test" im Manifest = Pre-release, "stable" = normales Release.
#   .\tools\github-release.ps1                    # Version aus platformio.ini
#   .\tools\github-release.ps1 -Version 0.10.1    # bestimmte Version
#   .\tools\github-release.ps1 -NotesFile x.md    # eigener Text statt "## Neu in <Version>" aus README.md
#   .\tools\github-release.ps1 -Promote 0.10.1    # Pre-release-Markierung entfernen (nach flasher-update -Promote)
# Vorher: tools\flasher-update.ps1, docs/fw committen, Tag v<Version> pushen.
param(
    [string]$Version,
    [string]$NotesFile,
    [string]$Promote
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding($false)
$repo = 'peterhagelhof7-cmd/esp-meshcore-gateway'
$fwDir = Join-Path $root 'docs\fw'

$variants = [ordered]@{
    v4 = @{ Id = 'MESHCORE-GW-HELTEC-V4';    Asset = 'meshcore-gateway-heltec-v4';    Board = 'Heltec WiFi LoRa 32 V4 / V4.3 (2 MB PSRAM)' }
    r8 = @{ Id = 'MESHCORE-GW-HELTEC-V4-R8'; Asset = 'meshcore-gateway-heltec-v4-r8'; Board = 'Heltec WiFi LoRa 32 V4-R8 (8 MB PSRAM)' }
}

if ($Promote) {
    & gh release edit "v$Promote" -R $repo --prerelease=false --latest
    if ($LASTEXITCODE) { throw "gh release edit v$Promote fehlgeschlagen" }
    Write-Output "v$Promote ist jetzt ein normales Release (latest)"
    return
}

if (-not $Version) {
    $ini = [IO.File]::ReadAllText((Join-Path $root 'platformio.ini'), $utf8)
    if ($ini -notmatch "GATEWAY_VERSION='`"([^`"]+)`"'") { throw 'GATEWAY_VERSION nicht in platformio.ini' }
    $Version = $Matches[1]
}
$tag = "v$Version"

$manifest = [IO.File]::ReadAllText((Join-Path $fwDir 'manifest.json'), $utf8) | ConvertFrom-Json
$rel = $manifest.releases | Where-Object { $_.version -eq $Version }
if (-not $rel) { throw "Version $Version nicht im Flasher-Manifest (erst tools\flasher-update.ps1)" }

if (-not (& git -C $root ls-remote --tags origin "refs/tags/$tag")) { throw "Tag $tag ist nicht auf origin (erst taggen und pushen)" }

$out = Join-Path ([IO.Path]::GetTempPath()) "mcgw-release-$Version"
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

$assets = @()
$rows = @()
$sums = @()
foreach ($key in $variants.Keys) {
    $v = $variants[$key]
    $src = Join-Path $fwDir "$Version\$key\firmware.bin"
    if (-not (Test-Path $src)) { throw "$src fehlt" }
    $bytes = [IO.File]::ReadAllBytes($src)
    $marker = "MCGW-FW-ID:$($v.Id):${Version}:MCGW-FW-END"
    if (-not [Text.Encoding]::GetEncoding(28591).GetString($bytes).Contains($marker)) { throw "$src trägt nicht '$marker'" }
    $sha = (Get-FileHash -Algorithm SHA256 $src).Hash.ToLower()
    $mf = $rel.variants.$key.files | Where-Object { $_.name -eq 'firmware.bin' }
    if (-not $mf -or $mf.sha256.ToLower() -ne $sha) { throw "${src}: SHA-256 weicht vom Manifest ab" }
    $name = "$($v.Asset)-$Version.bin"
    Copy-Item $src (Join-Path $out $name)
    $assets += (Join-Path $out $name)
    $rows += "| $($v.Board) | ``$name`` |"
    $sums += "$sha  $name"
}

if ($NotesFile) {
    $intro = [IO.File]::ReadAllText((Resolve-Path $NotesFile), $utf8).Trim()
} else {
    $readme = [IO.File]::ReadAllText((Join-Path $root 'README.md'), $utf8)
    $m = [regex]::Match($readme, "(?ms)^## Neu in $([regex]::Escape($Version))[^\n]*\n(.*?)(?=^## )")
    if (-not $m.Success) { throw "Abschnitt '## Neu in $Version' fehlt in README.md (oder -NotesFile angeben)" }
    $intro = "## Neu in $Version`n`n" + $m.Groups[1].Value.Trim()
}
$state = if ($rel.status -eq 'stable') { '**Status: bewährt.**' } else { '**Status: in Erprobung.**' }
$mc = if ($rel.meshcore) { " Basis: MeshCore repeater-v$($rel.meshcore)." } else { '' }

$notes = @(
    "$state$mc", '', $intro, '',
    '## OTA-Update (Weboberfläche → System → Firmware-Update)', '',
    'Die passende Datei für dein Board hochladen, alle Einstellungen bleiben erhalten:', '',
    '| Board | Datei |', '|---|---|') + $rows + @('',
    'Das Gateway prüft vor dem Schreiben Board und Version und lehnt falsche Dateien ab. Startet die neue Version nicht sauber, kehrt es automatisch zur vorherigen zurück.', '',
    'SHA-256:', '```') + $sums + @('```', '',
    "**Neuinstallation:** über den [Web-Flasher](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/) (Version $Version auswählen). Die ``.bin`` hier sind nur App-Images für das OTA-Update, nicht zum Flashen an Adresse 0x0.")
$notesPath = Join-Path $out 'notes.md'
[IO.File]::WriteAllText($notesPath, ($notes -join "`n") + "`n", $utf8)

& gh release view $tag -R $repo --json tagName 2>$null | Out-Null
if ($LASTEXITCODE -eq 0) {
    & gh release upload $tag -R $repo --clobber @assets
    if ($LASTEXITCODE) { throw 'gh release upload fehlgeschlagen' }
    & gh release edit $tag -R $repo --notes-file $notesPath
    if ($LASTEXITCODE) { throw 'gh release edit fehlgeschlagen' }
    Write-Output "Release $tag aktualisiert"
} else {
    $flags = @('--verify-tag', '--title', $Version, '--notes-file', $notesPath)
    if ($rel.status -ne 'stable') { $flags += '--prerelease' }
    & gh release create $tag -R $repo @flags @assets
    if ($LASTEXITCODE) { throw 'gh release create fehlgeschlagen' }
    Write-Output "Release $tag angelegt ($($rel.status))"
}
$sums | ForEach-Object { Write-Output $_ }
