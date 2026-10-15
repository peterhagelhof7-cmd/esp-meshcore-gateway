# Host-Tests (ohne Hardware): reine C++-Module mit g++ bauen und pruefen.
#   .\tools\host-test.ps1            # nutzt vorhandene firmware.bin aus .pio\build
#   .\tools\host-test.ps1 -Build     # baut vorher beide Envs mit PlatformIO
param([switch]$Build)

# Continue statt Stop: PowerShell 5.1 macht sonst aus jeder stderr-Zeile nativer
# Programme (pio, g++) einen Abbruch. Exit-Codes werden unten explizit geprueft.
$ErrorActionPreference = 'Continue'
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

# gnu++11 wie die Firmware (Arduino-ESP32 2.0.x). -static: sonst lädt Windows ggf.
# eine fremde libstdc++-6.dll (z. B. aus Git/mingw64) -> ABI-Absturz im Test.

# 1) Firmware-Merkmal (OTA)
$exe = Join-Path $out 'fw_marker_test.exe'
& g++ -std=gnu++11 -O1 -static -Wall -Wextra -I (Join-Path $root 'src') `
    (Join-Path $root 'test/host/fw_marker_test.cpp') (Join-Path $root 'src/FwMarker.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Compile fw_marker_test fehlgeschlagen' }
foreach ($e in $envs) {
    $bin = Join-Path $root ".pio/build/$($e.Name)/firmware.bin"
    if (-not (Test-Path $bin)) { Write-Warning "$bin fehlt (mit -Build bauen)"; $failed++; continue }
    Write-Host "== Merkmal $($e.Name) ($version)"
    Invoke-Test $exe @($bin, $e.Id, $version)
}

# 2) Regel-Engine + Sensormeter-JSON
$ruleTest = Join-Path $root 'test/host/rule_engine_test.cpp'
if (Test-Path $ruleTest) {
    $exe = Join-Path $out 'rule_engine_test.exe'
    $aj = Join-Path $root '.pio/libdeps/gateway_heltec_v4/ArduinoJson/src'
    if (-not (Test-Path $aj)) { throw "ArduinoJson fehlt ($aj) - einmal mit -Build bauen" }
    & g++ -std=gnu++11 -O2 -static -Wall -Wextra -I (Join-Path $root 'src') -I $aj `
        $ruleTest (Join-Path $root 'src/RuleEngine.cpp') (Join-Path $root 'src/SmJson.cpp') -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Compile rule_engine_test fehlgeschlagen' }
    Write-Host '== Regel-Engine'
    Invoke-Test $exe @((Join-Path $root 'test/host/fixtures'))
}

# 3) Bestaetigungspflicht (E39)
$exe = Join-Path $out 'ack_tracker_test.exe'
& g++ -std=gnu++11 -O2 -static -Wall -Wextra -I (Join-Path $root 'src') `
    (Join-Path $root 'test/host/ack_tracker_test.cpp') (Join-Path $root 'src/AckTracker.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Compile ack_tracker_test fehlgeschlagen' }
Write-Host '== Bestaetigungspflicht'
Invoke-Test $exe @()

# 4) Zabbix-Kennung + Sender-Protokoll (E41)
$exe = Join-Path $out 'zbx_format_test.exe'
& g++ -std=gnu++11 -O2 -static -Wall -Wextra -I (Join-Path $root 'src') `
    (Join-Path $root 'test/host/zbx_format_test.cpp') (Join-Path $root 'src/ZbxFormat.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Compile zbx_format_test fehlgeschlagen' }
Write-Host '== Zabbix-Format'
Invoke-Test $exe @()

if ($failed) { Write-Host "$failed Testlauf/-laeufe mit Fehlern" -ForegroundColor Red; exit 1 }
Write-Host 'Alle Host-Tests OK' -ForegroundColor Green
