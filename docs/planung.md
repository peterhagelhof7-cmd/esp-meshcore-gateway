# ESP-MeshCore-Gateway – Planung

Stand: 2026-10-14 · Status: **M0–M7 implementiert (0.6.0), Hardware-Test ausstehend**

## Entscheidungen (2026-10-13, vom User bestätigt)
- **Rolle:** Repeater als Basis + protokollkonformes Kanal-Senden (Details: `entscheidungen.md` E1)
- **Einbindung:** MeshCore als **Git-Submodul**, gepinnt auf Release-Tag (Start: `repeater-v1.17.1`)
- **Repo:** vorerst **nur lokal** (`C:\Users\Admin\esp-meshcore-gateway`), kein Remote
- **Platine:** unbekannt (V4 vs. V4-R8) → beim ersten Anschließen per `esptool flash_id`/Chip-Info
  auslesen (PSRAM 2 MB = V4, 8 MB = V4-R8); bis dahin wird für `heltec_v4` geplant

## Ziel

Heltec WiFi LoRa 32 **V4** als MeshCore-Knoten, der die **offizielle MeshCore-Firmware**
vollständig enthält und zusätzlich das WLAN des ESP32-S3 nutzt:

1. Erstsetup über Installer-AP (wie Sensormeter/Infoscreen), danach Betrieb im Heim-WLAN.
2. Weboberfläche mit **allen MeshCore-Einstellungen**.
3. **Webkonsole** mit den offiziellen CLI-Befehlen (`docs/cli_commands.md` upstream).
4. **Später:** Sensormeter-Geräte im LAN abfragen; beim Über-/Unterschreiten definierbarer
   Grenzwerte eine Nachricht in einen definierbaren MeshCore-Kanal senden.

## Recherche-Ergebnisse (2026-10-13)

### Hardware: Heltec WiFi LoRa 32 V4(.3)
| Punkt | Wert |
|---|---|
| MCU | ESP32-S3R2 (V4, 2 MB PSRAM) bzw. ESP32-S3R8 (V4-R8, 8 MB PSRAM) |
| Flash | 16 MB |
| LoRa | SX1262 + FEM (GC1109 bzw. KCT8103L), High-Power-Version bis 28 dBm |
| Display | SSD1306-OLED (I²C SDA17/SCL18, RST21) oder TFT-Variante |
| Sonstiges | GNSS-Stecker, Solar 4,7–6 V, LiPo, USB-C (nativ USB-Serial-JTAG), User-Button GPIO0 |

⚠ V4 und V4-R8 haben **unterschiedliche Pins** (Vext, LED, GNSS-EN, PA_CTX) → Upstream hat
dafür zwei Varianten (`variants/heltec_v4`, `variants/heltec_v4_r8`). Welche Platine vorliegt,
muss vor M0 feststehen.

### MeshCore upstream (github.com/meshcore-dev/MeshCore, MIT)
- Aktuell: **v1.17.1** (2026-08-14), Releases getrennt je Rolle (`repeater-`, `companion-`, `room-server-`).
- Build: **PlatformIO + Arduino** (`platformio/espressif32@6.11.0`, RadioLib gepinnt), `library.json` vorhanden.
- Heltec V4 wird **offiziell unterstützt** (Envs: `heltec_v4_repeater`, `_room_server`,
  `_companion_radio_usb/ble/wifi`, `_sensor`, `_terminal_chat`, `_repeater_bridge_espnow`).
- Rollen (`examples/`): Companion Radio, Repeater, Room Server, Sensor, KISS-Modem, Secure Chat.

### Wichtige Erkenntnisse für die Architektur
1. **Die CLI gibt es nur in Repeater / Room Server / Sensor** (`CommonCLI::handleCommand(ts, cmd, reply)`).
   Die Companion-Firmware hat **keine** CLI, sondern ein Binärprotokoll für die App
   (BLE/USB/TCP-Port 5000).
2. **Kanäle (Group-Text) sendet nur die Companion-Rolle** von Haus aus. Repeater senden keine
   Kanalnachrichten. Die Mesh-Bibliothek bietet aber `createGroupDatagram()` + `sendFlood()` →
   Kanal-Senden lässt sich in jede Rolle **sauber nachrüsten**, ohne das Protokoll zu verändern.
3. Die Sensor-Rolle hat zwar „Alerts“, schickt sie aber als **Direktnachricht an Admin-Kontakte**,
   nicht in Kanäle → passt nicht direkt zur Anforderung.
4. Die V4-Envs setzen `ESP32_CPU_FREQ=80` (Stromsparen) → für WLAN + Webserver auf 160/240 MHz anheben.
5. Die CLI-Callbacks (`CommonCLICallbacks`) sind sauber abstrahiert → eine Webkonsole kann
   `handleCommand()` 1:1 aufrufen; Antworten sind identisch zur seriellen/Remote-CLI.

## Architektur-Vorschlag

```
┌────────────────────── ESP32-S3 (Heltec V4) ──────────────────────┐
│ loopTask (Arduino)                                               │
│   MeshCore-Kern (unverändert, Git-Submodul, auf Release-Tag)     │
│   └─ GatewayMesh  : erbt von Repeater-MyMesh                     │
│        + Kanal-Tabelle (Name + PSK) + sendChannelText()          │
│        + CLI-Queue  (Befehle aus Web → im Mesh-Loop ausführen)   │
│        + Paket-Hook (Live-Log, Kanal-Empfang fürs Web)           │
│                                                                  │
│ AsyncTCP-Task                                                    │
│   ESPAsyncWebServer + WebSocket (Konsole, Live-Log)              │
│   REST: /api/status, /api/mesh/*, /api/channels, /api/wifi/*,    │
│         /api/ota, /api/backup ...                                │
│                                                                  │
│ Netzwerk-Manager: STA ↔ Installer-AP "installer" (wie SM)        │
│ Später: Sensormeter-Poller (HTTP GET /api/sensors) + Regel-Engine│
└──────────────────────────────────────────────────────────────────┘
```

**Leitprinzipien**
- **Upstream unverändert** als Git-Submodul, gepinnt auf ein Release-Tag. Eigener Code nur in
  eigenen Dateien (Subklasse/Hooks). Updates = Submodul-Tag hochziehen + bauen + testen.
  Falls ein Hook fehlt: minimaler Patch in `patches/` mit Begründung im Entscheidungslog.
- **Einstellungen laufen intern über die CLI.** Das Web-Formular erzeugt `set …`-Befehle und liest
  per `get …` zurück. So gibt es nur *eine* Wahrheit (MeshCore-Prefs auf dem FS) und das Verhalten
  ist garantiert identisch zur offiziellen Firmware.
- **Thread-Sicherheit:** Der Webserver läuft in einem anderen Task als MeshCore. Web-Aktionen
  werden **nur über eine Queue** in den Mesh-Loop gegeben, nie direkt aufgerufen.
- Eigene Konfig (WLAN, Web-Login, Kanäle, später Sensormeter-Regeln) getrennt von MeshCore-Prefs
  (NVS bzw. eigene JSON-Datei), damit ein MeshCore-`erase` die WLAN-Konfig nicht verliert (und umgekehrt).

### Weboberfläche (Seiten)
| Seite | Inhalt |
|---|---|
| Status | Name, Public Key, Rolle, Version, Uptime, Akku, Funkparameter, Noise Floor, RSSI/SNR, Nachbarn, Paketzähler, WLAN/IP |
| MeshCore | alle Prefs: Name, Position, Radio (freq/bw/sf/cr), TX-Power, RX-Gain, FEM-Gain, Repeat an/aus, Flood-Max, Advert-Intervalle, Admin-/Gast-Passwort, Regionen, GPS, Power … |
| Kanäle | Kanäle anlegen (Name + PSK / Public-Channel), Testnachricht senden, empfangene Nachrichten |
| Konsole | WebSocket-Terminal für offizielle CLI-Befehle, History, Autovervollständigung aus Befehlsliste |
| Paket-Log | Live-Mitschnitt (RX/TX, Typ, RSSI/SNR, Pfad) |
| Netzwerk | WLAN, Hostname, NTP (setzt auch die MeshCore-Uhr → `clock sync` entfällt) |
| System | OTA-Upload (mit Identitäts-/Downgrade-Check), Backup/Restore (Prefs + Identität), Reboot, Werksreset |
| Sensormeter *(Phase 2)* | Geräteliste, Abfrageintervall, Regeln |

Login-Schutz der Weboberfläche ist Pflicht (Konsole kann `erase`, Private-Key-Export usw.).

### Phase 2: Sensormeter-Anbindung (Vorplanung)
- Quelle: Sensormeter-REST `GET /api/sensors` (JSON `sensor1{temperature,humidity}`,
  `sensor2{temperature,humidity,pressureHpa,lux,eco2Ppm}`) – gilt für sm / sm-poe / sm-wlan.
  ⚠ Offen: Auth-Pflicht des Endpunkts prüfen.
- Regel = Gerät + Sensor + Messgröße + Operator (`>`/`<`) + Grenzwert + **Hysterese** +
  Mindestdauer + Cooldown + Zielkanal + Textvorlage (`{name} {wert} {einheit} > {grenze}`) +
  optional „Wieder normal“-Meldung + Meldung bei „Gerät nicht erreichbar“.
- **Funk-Budget beachten:** EU 869,4–869,65 MHz = max. 10 % Duty-Cycle, 500 mW ERP → globales
  Sendelimit (z. B. max. N Meldungen/Stunde), Sammelmeldungen statt Einzelflut.

## Meilensteine

| # | Inhalt | Verifikation |
|---|---|---|
| M0 ✅ | Toolchain, upstream `heltec_v4_repeater` **unverändert** bauen | Build grün (V4 + R8, 2026-10-13); Flash-Baseline offen |
| M1 🟡 | Eigenes Repo: Submodul + eigenes Env, Repeater-Kern + WLAN/Installer-AP + Status-Seite, CPU 240 MHz | Build grün V4 + R8 (2026-10-13); **HW-Test offen** |
| M2 🟡 | Webkonsole (HTTP-Polling → CLI-Queue, E10) + Live-Paket-Log | Build grün V4 + R8, UI-Test gegen Mock 16/16 (2026-10-13); **HW-Test offen** |
| M3 🟡 | Komplette MeshCore-Einstellungsseite + hansemesh-Profil (E15) + Backup/Restore + Passwort; NTP vorgezogen (E17) | Build grün V4 + R8, UI-Test gegen Mock 42/42 (2026-10-13); **HW-Test offen** |
| M4 🟡 | Kanäle: Verwaltung (Public/Hashtag/Privat, Scope) + Senden + Empfang (E18) | Build grün, UI-Test gegen Mock (2026-10-14); **Funk-Test mit App offen** |
| M5 🟡 | OTA mit Board-Merkmal + Downgrade-Schutz + Rollback (E19), eigene OLED-Anzeige (E20); NTP schon in M3 | Build grün V4 + R8, Host-Test Merkmal 35/35 je Board, UI-Test (2026-10-14); **HW-Test offen** |
| M6 🟡 | Sensormeter-Poller (Core-0-Task) + Regel-Engine + Web-Tab (E22) | Build grün V4 + R8, Host-Tests grün, UI-Test 78/78 inkl. echtem Sensormeter (2026-10-14); **HW-Test offen** |
| M7 🟡 | Ping-Check: max. 5 Ziele, Ausfall länger als Schwellwert → Alarmkanal (E25) | Build grün V4 + R8, Host-Tests grün, UI-Test 95/95 mit echten Pings (2026-10-14); **HW-Test offen** |

Nach jedem Meilenstein: echter Build, Entscheidungslog `docs/entscheidungen.md`, Commit.
Flashen auf Hardware nur nach Ansage.

## Risiken
| Risiko | Bewertung | Gegenmaßnahme |
|---|---|---|
| Upstream ändert interne APIs | 🟡 | Submodul auf Tag pinnen, Patches minimal + dokumentiert |
| Web-Task ↔ Mesh-Loop Race | 🟡 | strikt Queue-basiert |
| RAM (V4 = 2 MB PSRAM) | 🟢 | reicht; Web-Assets gzip im Flash |
| Rechtliches TX-Limit (FEM bis 28 dBm) | 🟡 | `set tx` in UI mit Hinweis/Grenze je Band |
| Repeater + Kanal-Senden im selben Knoten | 🟢 | Protokoll-konform über `createGroupDatagram`; Absender ist die Knoten-Identität |
