# Entscheidungslog

## E1 – Rolle: Repeater + Kanal-Senden (2026-10-13)
**Kontext:** In der offiziellen MeshCore-Firmware gibt es die CLI (`CommonCLI`) nur in Repeater,
Room Server und Sensor. Kanalnachrichten (Group-Text) sendet nur der Companion, und der hat keine
CLI, sondern das Binärprotokoll für die App.
**Entscheidung:** Als Basis dient der Repeater (`examples/simple_repeater`). Er wird um eine
Kanal-Tabelle (Name + PSK) und `sendChannelText()` ergänzt, gebaut aus `mesh::Mesh::createGroupDatagram()`
und `sendFlood()`.
**Begründung:** Die Webkonsole kann die offizielle CLI 1:1 nutzen, und die Sensormeter-Alarme können
in Kanäle gehen. Am MeshCore-Protokoll ändert sich nichts.
**Folge:** Room-Server-, Companion- und App-Anbindung (TCP 5000) sind nicht Teil des Scopes.
Sie können später als eigene Envs dazukommen.

## E2 – Upstream als Git-Submodul auf Release-Tag (2026-10-13)
**Entscheidung:** `meshcore-dev/MeshCore` kommt als Submodul unter `lib/` bzw. `external/` und wird auf
`repeater-v1.17.1` gepinnt. Eigener Code liegt nur in eigenen Dateien. Nötige Upstream-Änderungen
kommen als minimale Patches nach `patches/` und werden hier begründet.

## E3 – Einstellungen intern über die CLI (2026-10-13)
**Entscheidung:** Die Web-Einstellungsseite erzeugt `set …`-Befehle und liest Werte per `get …`.
Es gibt keinen eigenen Schreibzugriff auf `NodePrefs`.
**Begründung:** So gibt es nur eine Quelle der Wahrheit, und das Verhalten ist identisch zur offiziellen
Firmware, auch nach Upstream-Updates.

## E4 – Web ↔ Mesh nur über eine Queue (2026-10-13)
**Entscheidung:** ESPAsyncWebServer läuft im AsyncTCP-Task, MeshCore im loopTask. Alle Web-Aktionen
(CLI-Befehle, Kanal-Senden) gehen über eine FreeRTOS-Queue in den Mesh-Loop. Antworten kommen
über eine Rück-Queue bzw. den WebSocket zurück.

## E5 – M0: Upstream-Baseline gebaut (2026-10-13)
Submodul `external/MeshCore` auf `repeater-v1.17.1` (Commit d9296435). Die Envs `heltec_v4_repeater`
und `heltec_v4_r8_repeater` bauen **unverändert grün** (`pio run`, Exit 0):

| Env | Flash | Build-Zeit |
|---|---|---|
| heltec_v4_repeater | 1 168 529 B (17,8 % von 6,25 MB) | 4:22 (inkl. Toolchain-Download) |
| heltec_v4_r8_repeater | 1 171 997 B (17,9 %) | 3:10 |

Erkenntnisse:
- Framework ist **Arduino-ESP32 2.0.17** (`framework-arduinoespressif32 3.20017`, Plattform 6.11.0),
  also ESP-IDF 4.4. Für den Webserver nehmen wir Lib-Versionen, die mit Core 2.x kompatibel sind.
- Partitionen: `default_16MB.csv` (2× App à 6,25 MB + SPIFFS ca. 3,4 MB). Upstream-OTA und die
  MeshCore-Prefs liegen dort. Für das eigene Env behalten wir das Layout, damit ein Wechsel
  zwischen Upstream- und Gateway-Firmware die Identität (Schlüssel) nicht löscht.
- Die RAM-Anzeige beim V4-Env bezieht sich auf 2 MB (Board-JSON `maximum_ram_size` = PSRAM).
  Bei R8 sind es 320 KB intern. Der echte interne RAM-Verbrauch ist in beiden Fällen ca. 59 KB.
- Nur harmlose Warnungen aus der Fremd-Lib Melopero_RV3028 (`requestFrom` mehrdeutig).
- Noch nicht auf Hardware geflasht (Platinenvariante weiterhin unbekannt).

## E6 – Installer-AP sofort, wenn kein WLAN konfiguriert ist (2026-10-13)
Die WLAN-Logik kommt aus sensormeter-wlan (`NetworkManager`): Reconnect alle 20 s, nach 5 min ohne IP
der AP „installer“/„installer“ auf 192.168.4.1 im AP_STA-Modus, Selbstheilung, 30-s-Test nach neuer
Eingabe und `WiFi.setSleep(false)`.
**Abweichung:** Ist noch gar keine SSID gespeichert (Erstinbetriebnahme), startet der AP **sofort**.
Bei sensormeter-wlan wartet das Gerät auch dann 5 min, was beim Ersteinrichten nur Zeit kostet.

## E7 – Web-Login = MeshCore-Admin-Passwort (2026-10-13)
Die Weboberfläche nutzt HTTP-Basic-Auth mit Benutzer `admin` und dem Admin-Passwort des Knotens
(CLI `password …`, Standard `password`). Es gibt also nur ein Passwort für Remote-Admin über das Mesh
und für das Web. Ändert man es per CLI, ändert sich auch der Web-Login. Das Passwort steht im
Status-Snapshot, wird aber nie ausgeliefert.
⚠ Basic-Auth über HTTP ist im LAN unverschlüsselt. Für M1 ist das akzeptiert, HTTPS ist auf dem
ESP32 mit Arduino-Core 2.x aufwendig. Neu bewerten, sobald die Webkonsole kommt (M2).

## E8 – Build-Integration: eigenes src_dir + Upstream-INIs (2026-10-13)
- Das Root-`platformio.ini` liest das Upstream-`platformio.ini` und die beiden V4-Varianten per
  `extra_configs` ein. Build-Flags und lib_deps werden per `${heltec_v4_oled.*}` wiederverwendet,
  die Quell-Filter sind eigene (Upstream-Filter sind relativ zu dessen `src/`).
- `lib_dir = external/MeshCore/lib` (ed25519), `boards_dir = external/MeshCore/boards`.
- **Fund:** Die Upstream-V4-Envs listen die OLED-Libs (Adafruit SSD1306/GFX) **nicht** in `lib_deps`.
  Sie landen dort nur indirekt. Bei uns sind sie explizit eingetragen und auf die Versionen
  gepinnt, die der M0-Build aufgelöst hat (SSD1306 2.5.17, GFX 1.12.6).
- `DISABLE_WIFI_OTA=1`: Das Upstream-`start ota` würde einen eigenen SoftAP und einen zweiten
  Webserver auf Port 80 starten. Die CLI antwortet jetzt mit „not supported“, eigenes OTA kommt in M5.
  Nebeneffekt: Die Firmware ist ca. 38 KB kleiner (ohne AsyncElegantOTA).
- `ESP32_CPU_FREQ`: Upstream setzt 80 MHz, per `build_unflags` entfernt und durch 240 ersetzt.
  `main.cpp` bricht den Build per `#error` ab, falls der Wert nicht 240 ist.
- Upstream baut mit `-w` (alle Warnungen aus). Das gilt auch für unseren Code.

## E9 – Status-Snapshot alle 5 s (2026-10-13)
`stats-core` liest die Akkuspannung, und `HeltecV4Board::getBattMilliVolts()` enthält `delay(10)`.
Ein Snapshot pro Sekunde würde den Mesh-Loop 1 % der Zeit blockieren. Deshalb gibt es den Snapshot
nur alle 5 s, die Web-Seite fragt im selben Takt ab.
