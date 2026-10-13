# Host-Tests (ohne Hardware): reine C++-Module mit g++ bauen und pruefen.
#   .\tools\host-test.ps1            # nutzt vorhandene firmware.bin aus .pio\build
#   .\tools\host-test.ps1 -Build     # baut vorher beide Envs mit PlatformIO
param([switch]$Build)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $env:TEMP 'meshcore-gw-hosttest'
New-Item -ItemType Directory -Force $out | Out-Null

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    throw 'g++ nicht gefunden (z. B. winget install BrechtSanders.WinLibs.POSIX.UCRT)'
}

$envs = @(
    @{ Name = 'gateway_heltec_v4';    Id = 'MESHCORE-GW-HELTEC-V4' },
    @{ Name = 'gateway_heltec_v4_r8'; Id = 'MESHCORE-GW-HELTEC-V4-R8' }
)
if ($Build) {
    foreach ($e in $envs) {
        & pio run -d $root -e $e.Name
        if ($LASTEXITCODE -ne 0) { throw "Build $($e.Name) fehlgeschlagen" }
    }
}

$ini = Get-Content (Join-Path $root 'platformio.ini') -Raw
if ($ini -notmatch "GATEWAY_VERSION='`"([^`"]+)`"'") { throw 'GATEWAY_VERSION nicht gefunden' }
$version = $Matches[1]

$failed = 0
function Invoke-Test([string]$exe, [string[]]$testArgs) {
    & $exe @testArgs
    if ($LASTEXITCODE -ne 0) { $script:failed++ }
}

# 1) Firmware-Merkmal (OTA)
$exe = Join-Path $out 'fw_marker_test.exe'
& g++ -std=c++17 -O1 -Wall -Wextra -I (Join-Path $root 'src') `
    (Join-Path $root 'test/host/fw_marker_test.cpp') (Join-Path $root 'src/FwMarker.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Compile fw_marker_test fehlgeschlagen' }
foreach ($e in $envs) {
    $bin = Join-Path $root ".pio/build/$($e.Name)/firmware.bin"
    if (-not (Test-Path $bin)) { Write-Warning "$bin fehlt (mit -Build bauen)"; $failed++; continue }
    Write-Host "== Merkmal $($e.Name) ($version)"
    Invoke-Test $exe @($bin, $e.Id, $version)
}

# 2) Regel-Engine (Sensormeter-Alarme)
$ruleTest = Join-Path $root 'test/host/rule_engine_test.cpp'
if (Test-Path $ruleTest) {
    $exe = Join-Path $out 'rule_engine_test.exe'
    & g++ -std=c++17 -O1 -Wall -Wextra -I (Join-Path $root 'src') `
        $ruleTest (Join-Path $root 'src/RuleEngine.cpp') -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Compile rule_engine_test fehlgeschlagen' }
    Write-Host '== Regel-Engine'
    Invoke-Test $exe @()
}

if ($failed) { Write-Host "$failed Testlauf/-laeufe mit Fehlern" -ForegroundColor Red; exit 1 }
Write-Host 'Alle Host-Tests OK' -ForegroundColor Green
