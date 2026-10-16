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

## E10 – Konsole und Paket-Log per HTTP-Polling statt WebSocket (2026-10-13)
**Abweichung vom Plan:** In `planung.md` war ein WebSocket vorgesehen. Umgesetzt ist Polling:
- `POST /api/cli` legt den Befehl in die Queue (E4) und antwortet sofort.
- `GET /api/console?since=N` und `GET /api/packets?since=N` liefern neue Zeilen aus Ringpuffern
  (150 bzw. 300 Zeilen, im PSRAM). Jede Zeile hat eine fortlaufende Sequenznummer, ein Überlauf
  wird als `gap` gemeldet.
- Die Seite fragt nur den sichtbaren Tab ab, jede Sekunde. Nach einem Befehl fragt sie zusätzlich
  nach 150, 400 und 900 ms nach, die Antwort ist also praktisch sofort da.

**Begründung:** Basic-Auth gilt damit einheitlich für jeden Request. Ob Browser die gecachten
Basic-Credentials beim WebSocket-Handshake mitschicken, ist nicht verlässlich geregelt. Außerdem
schreibt der Mesh-Loop nur in einen Puffer und sendet nie selbst über den Netzwerk-Stack. Eine
Latenz von unter 1 s reicht für eine Admin-Konsole.

## E11 – Webkonsole mit den Rechten der seriellen Konsole (2026-10-13)
Web-Befehle laufen wie die serielle CLI mit `sender_timestamp = 0`. Damit gehen auch die
„Serial Only“-Befehle: `erase`, `set freq`, `get/set prv.key`, `stats-*`, `log`.
- `log` schreibt den Datei-Log weiterhin **nur seriell** (Upstream `dumpLogFile()`). In der
  Webkonsole kommt keine Antwort. Ein Download des Datei-Logs ist ein Kandidat für M3/M5.
- Serielle Befehle erscheinen zusätzlich in der Webkonsole (Mitprotokoll).
- Leerzeilen sind erlaubt, weil `region load` mit einer Leerzeile beendet wird.
- Die Bestätigungsdialoge der Oberfläche (`erase`, `reboot`, `poweroff` …) sind nur UX, **keine**
  Sicherheitsgrenze.

**Sicherheit (Neubewertung von E7):** Wer das Admin-Passwort hat, kann über HTTP im LAN auch den
privaten Schlüssel lesen oder den Knoten löschen. Das Passwort selbst geht dabei unverschlüsselt
über die Leitung (Basic-Auth). Für den Betrieb im Heimnetz ist das akzeptiert, weil das
Admin-Passwort übers Mesh dieselben Rechte gibt.
Offene Option für später: Schalter „Schlüssel-Befehle im Web sperren“ oder HTTPS.

## E12 – Kein Akku, kein Bluetooth (2026-10-13, Vorgabe User)
Akku und Bluetooth sind derzeit nicht zur Nutzung vorgesehen.
- Die Statusseite zeigt keine Akkuspannung mehr. `stats-core` in der Konsole liefert `battery_mv`
  weiterhin unverändert wie im Original.
- Die Repeater-Firmware enthält ohnehin keinen BLE-Stack. Es gibt nichts abzuschalten.
- **Hinweis:** `poweroff`/`shutdown` versetzt den ESP in Deep-Sleep ohne Weckquelle. Ohne Akku und
  Taster-Wake ist das Gateway danach nur per Reset oder Stromtrennung zurückzuholen. Die
  Oberfläche warnt davor.

## E13 – Exaktes Routen-Matching im Webserver (2026-10-13)
Alle Routen sind mit `AsyncURIMatcher::exact()` registriert. In ESPAsyncWebServer matcht ein
String-URI sonst auch Unterpfade (`/api/wifi` → `/api/wifi/scan`). Dieser bekannte Fallstrick
aus der sensormeter-Familie wird hier von Anfang an vermieden.

## E14 – Web-UI-Test ohne Hardware (2026-10-13)
`tools/webui-test/`: `mock_server.py` liest die Seite direkt aus `src/web/index_html.h` und simuliert
die API. `test.js` (puppeteer-core mit dem installierten Chrome) prüft Status, Konsole (Echo,
mehrzeilige Antwort, Schnellbuttons, Verlauf, abgelehnter `erase`), Paket-Filter, Mobilbreite und
JS-Fehler. Stand M2: **16/16 PASS**. Das ersetzt keinen Hardware-Test.

## E15 – Empfohlene Einstellungen nach hansemesh.de (2026-10-13, Vorgabe User)
Quellen (abgerufen 2026-10-13): hansemesh.de/anleitungen/repeater-checkliste/,
/einstellungen-deutschland/, /heltec-v3-v4/. Standortwerte hat der User festgelegt.

| Befehl (1.17.1) | Wert | Herkunft |
|---|---|---|
| `set radio` | 869.618,62.5,8,8 | EU/UK Narrow (DE-Standard) |
| `set tx` | **17** | User (hansemesh: 16 sicher, 17 ≈ 520 mW knapp an 500 mW ERP). Im Web änderbar. |
| `set dutycycle` | 10 | gesetzlich |
| `set flood.max` / `.unscoped` / `.advert` | 10 / **5** / **5** | User: ländlich |
| `set advert.interval` | 240 | Zero-Hop-Advert alle 4 h |
| `set flood.advert.interval` | **48** | User |
| `set txdelay` / `direct.txdelay` / `rxdelay` | 0.7 / 0.5 / 0 | Rand-Repeater |
| `set path.hash.mode` | 2 | 3-Byte-Pfade |
| `set loop.detect` | minimal | |
| `set cad` / `int.thresh` | off / 0 | |
| `set agc.reset.interval` | 60 | Checklisten-Tabelle (4 nur bei tauben Repeatern) |
| `set multi.acks` | 1 | |
| `set radio.rxgain` / `radio.fem.rxgain` | off / off | V4 auf 1.17.1 (Noise Floor sonst ca. -84 dBm) |
| `region put` | **de-he, de-by, rhein-main** + `region save` | User |

**⚠ Abweichungen hansemesh ↔ MeshCore 1.17.1 (gegen CommonCLI.cpp geprüft):**
- `set advert.interval.zero` / `set advert.interval.flood` gibt es nicht. Richtig sind
  `set advert.interval <min>` (60–240) und `set flood.advert.interval <h>` (3–168, 0 = aus).
- `get version` gibt es nicht, der Befehl heißt `ver`.
- `set timezone` gibt es in der Repeater-CLI nicht. MeshCore rechnet in UTC.
- `region denyf de` (von hansemesh empfohlen) ist **nicht** gesetzt, der User hat es nicht
  gewünscht. Achtung beim Nachholen: `findByNamePrefix("de")` trifft als Präfix auch `de-he`
  oder `de-by`, wenn keine Region exakt „de“ heißt. Vorher also `region put de` anlegen.
- Die Werte für Name, Admin- und Gast-Passwort sind Sache des Betreibers und nicht im Profil.

**Umsetzung:**
- Build-Defaults: `LORA_CR=8` (Upstream-Repeater fällt sonst auf **CR 5** zurück, genau die
  „nach Update CR 5“-Falle aus der Checkliste) und `LORA_TX_POWER=17` statt 10. Beides prüft
  `main.cpp` per `#error`.
- Profil `src/Profile.cpp`: jeder Eintrag ist Befehl + Prüf-`get` + Sollwert + Begründung.
  Beim **Erststart** (kein `/prefs.json` und kein `/com_prefs`) wird es automatisch angewendet.
  Die Einstellungsseite zeigt jederzeit Ist/Soll und übernimmt Abweichungen auf Knopfdruck.
- Die ERP-Rechnung hängt von der Antenne ab, die noch nicht feststeht. Bei einer Antenne über ca.
  2 dBi tx reduzieren (hansemesh-Formel ERP = tx_out + dBd − Verluste).

## E16 – Einstellungsseite über CLI-Batches (2026-10-13)
Umsetzung von E3:
- `POST /api/cli/batch` (bis 40 Befehle, `quiet=1` für reine Abfragen) und
  `GET /api/cli/job?id=`. CliBridge hält 4 Job-Plätze im PSRAM und arbeitet pro loop() einen Befehl ab.
- Felder: alle Repeater-Prefs außer Bridge/GPS/Powersaving (nicht genutzt, siehe E12). Geschickt
  werden nur geänderte Felder als `set …`, danach wird alles neu gelesen. Was die CLI ablehnt
  (z. B. `advert.interval 30`), zeigt die Seite an.
- `get` mit „Error: unsupported“ → Feld ausgegraut (V4 ohne KCT8103L: `radio.fem.txgain`).
- Funk- oder Schlüsseländerung → Banner „Neustart nötig“.
- Regionen: Rohausgabe von `region` plus Aktionen put/remove/allowf/denyf/home und `region save`.
  `region load` wird bewusst nicht genutzt (friert laut hansemesh die CLI in 1.17.x ein).
- **Backup** = JSON mit einer Liste offizieller CLI-Befehle (`set …`, `region …`, optional
  `set prv.key`). **Restore** spielt sie als Batch ein und lässt nur `set …` und `region put/denyf/home/save` zu.
  Nicht enthalten: Admin-Passwort (die CLI kann es nicht auslesen), WLAN-Zugangsdaten.
- **Geheimnisse:** `password …` und `set prv.key …` werden im Konsolen-Log, seriell und in der
  Job-Antwort maskiert. Die CLI wiederholt das Passwort sonst im Klartext („password now: …“).

## E17 – NTP stellt die MeshCore-Uhr (2026-10-13, vorgezogen aus M5)
`ESP32RTCClock` liest und schreibt die ESP-Systemzeit. Sobald die Station eine IP hat, startet
SNTP (`pool.ntp.org`, `time.cloudflare.com`, UTC) und stellt damit direkt die Mesh-Uhr. Bei jeder
Synchronisation (stündlich) wird zusätzlich `rtc_clock.setCurrentTime()` aufgerufen, damit eine
eventuell angeschlossene externe RTC (DS3231 via AutoDiscover) mitläuft.
Grund: Laut hansemesh verwirft eine falsche Uhr Adverts und leert die Nachbarliste. Ohne RTC
startet der ESP sonst bei Mai 2024.

## E18 – M4: Kanäle im Repeater (2026-10-14, autonom entschieden)
**Umsetzung (E1):** `GatewayMesh` überschreibt `searchChannelsByHash()` und `onGroupDataRecv()` und
bekommt `sendChannelText()`. Das Paket wird **byte-gleich** zu `BaseChatMesh::sendGroupMessage`
aufgebaut: 4 Byte Zeitstempel (`getCurrentTimeUnique`), `TXT_TYPE_PLAIN`, „Knotenname: Text“, max.
160 Byte inklusive Präfix (`MAX_TEXT_LEN`), danach `createGroupDatagram(PAYLOAD_TYPE_GRP_TXT)`.
Das Weiterleiten fremder Kanalpakete bleibt unverändert Sache des Repeaters.

**Schlüssel und Hash wie in der Companion-App** (geprüft gegen docs/companion_protocol.md):
- Public = `8b3387e9…cd72` (Base64 `izOH6cXN6mrJ5e26oRXNcg==`) → Hash **11**
- Hashtag = erste 16 Byte von `sha256("#name")`, z. B. `#test` → `9cd8fcf2…` → Hash **d9**
- Privat = 16 Byte zufällig (`esp_fill_random`) oder importiert (Base64 oder Hex)
- Hash = erstes Byte von `sha256(secret[16])`, wie `BaseChatMesh::addChannel`

**Entscheidungen:**
- **Max. 8 Kanäle**, gespeichert in NVS (Namespace `gw`, Blob mit Versionsbyte). Sie überleben ein
  MeshCore-`erase`.
- **Hashtag-Namen werden kleingeschrieben**, erlaubt sind nur `a-z 0-9 - _`. hansemesh: Der Hash ist
  case-sensitive, und iOS/Android haben dadurch schon doppelte Kanäle erzeugt.
- **Scope pro Kanal** (wie in der App „ein Scope pro Kanal“). Der Transport-Schlüssel ist
  `sha256("#region")[0..16]`, also dieselbe Ableitung wie `RegionMap::getTransportKeysFor` für
  öffentliche Regionen, gesendet über `MyMesh::sendFloodScoped()`. Private `$`-Regionen werden nicht
  unterstützt. **Standard: kein Scope**, weil ein Scope nur über Repeater läuft, die die Region
  kennen. Das muss lokal abgesprochen sein.
- Gesendet wird mit `path_hash_mode + 1` als Pfad-Hash-Größe, passend zur eigenen Einstellung
  (Profil: 3 Byte).
- **Kein Kanal ist vorbelegt.** Der Betreiber legt bewusst an, was er braucht. Für M6 bietet sich
  ein privater Kanal an.
- Empfangene Kanalnachrichten gehen in einen eigenen Ringpuffer (200 Zeilen, Typ M/S) mit Kanal,
  SNR und Hops. Es gibt keine Persistenz, nach einem Neustart ist der Puffer leer. `GRP_DATA` wird
  nicht angezeigt.
- Senden aus dem Web läuft über eine eigene Queue (`MeshActions`, E4), die M6 mitnutzt.
- Der Duty Cycle (10 %) wird vom Dispatcher für alle Sendungen durchgesetzt, auch für die des Gateways.
- Kanäle stehen im Backup (privat mit Schlüssel). Beim Restore werden bereits vorhandene
  übersprungen.
- Die LogBuffer-Zeilenlänge ist von 200 auf 256 Byte gestiegen (Kanalname plus 160 Byte Text).

**Verifikation:** Build grün. UI-Test gegen Mock: Hashtag-Normalisierung, Duplikat- und
Umlaut-Ablehnung, Hash-Werte d9 und 11, Hex→Base64-Schlüssel, Senden und Empfangen, Kanäle im Backup.
**Nicht verifiziert:** Interoperabilität über Funk mit einer echten App. Das ist der erste Test, wenn
die Hardware da ist: Nachricht in `#test` aus der App → im Gateway sichtbar und umgekehrt.

## E19 – M5: Firmware-Update per Web mit Merkmal, Downgrade-Schutz und Rollback (2026-10-14)
Die Pflichtanforderung aus der Sensormeter-Familie gilt auch hier: OTA nimmt nur Firmware desselben
Projekts an, ältere Versionen nur mit „Downgrade erzwingen“.
- **Merkmal** im Binary: `MCGW-FW-ID:<PROJEKT-ID>:<VERSION>:MCGW-FW-END`. Die Projekt-ID ist **pro Board**
  verschieden (`MESHCORE-GW-HELTEC-V4` bzw. `…-V4-R8`), weil V4 und R8 unterschiedliche Pins haben
  und eine falsche Firmware die Hardware falsch ansteuern würde.
- **Scanner** `src/FwMarker.cpp`: Byte-für-Byte-Zustandsautomat ohne Arduino-Abhängigkeit. Er ist
  NUL-sicher, sicher an Chunkgrenzen und kopiert nichts in gedeckelte Puffer. Damit sind beide
  Sensormeter-Bugs (strstr an NUL, Chunk-Kappung) konstruktiv ausgeschlossen.
  **Fund:** Der Suchbegriff `MCGW-FW-ID:` steht als Konstante selbst im Binary, bei der V4 an
  Offset 11788 und damit vor dem echten Merkmal (14321). Der Automat verwirft diesen Köder, weil
  danach eine NUL statt gültiger Feldzeichen folgt.
- **Host-Test** `test/host/fw_marker_test.cpp` (`tools/host-test.ps1`) gegen die **echten**
  `firmware.bin` beider Boards, mit Chunk-Größen 1 Byte bis zur ganzen Datei. Dazu kommen der Marker über
  jede Chunkgrenze, Negativfälle (Zufall, Text, Köder, abgeschnitten, ohne Version) und der
  Versionsvergleich (`a.b.c[-rcN]`, Release > RC): **35/35 PASS je Board**.
- **Login schon im Upload-Callback:** ESPAsyncWebServer ruft den Upload-Handler *vor* dem
  Request-Handler auf. Ohne eigene Prüfung könnte ein anonymer Client in die OTA-Partition schreiben.
- `_tempObject` wird **nicht** als Markierung genutzt (die Lib gibt es im Destruktor mit `free()`
  frei). Stattdessen wird der Owner-Request gemerkt, bei `onDisconnect` aufgeräumt und ein hängender
  Upload nach 30 s ohne Daten freigegeben.
- Das Feld `force` steht im Formular **vor** der Datei (Multipart wird in Reihenfolge geparst).
- **Rollback:** Der Arduino-Bootloader hat `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Mit
  `verifyRollbackLater() → true` markiert der Core die App nicht sofort als gültig. Das Gateway tut
  es erst nach **60 s stabiler Laufzeit**. Stürzt die neue Firmware vorher ab, startet der Bootloader
  die vorherige.
  ⚠ Ein Stromausfall oder manueller Neustart in diesen 60 s nach einem Update führt ebenfalls zum
  Rollback (gewollt konservativ).
- Upstream-`start ota` bleibt deaktiviert (E8).
- **Nicht HW-verifiziert:** echter OTA-Upload (Update.h, Multipart-`force`) und Rollback. Erster Test
  auf Hardware: gleiche Version hochladen → Neustart → „Prüflauf“ → nach 60 s „gültig“.

## E20 – M5: Eigene OLED-Anzeige statt Upstream-UITask (2026-10-14)
**Fund:** Im Upstream-UITask schaltet ein **langer Tastendruck den Knoten ab** (`board.powerOff()`,
Deep-Sleep ohne Weckquelle). Ohne Akku (E12) wäre das Gateway danach nur per Stromtrennung
erreichbar, und das schon durch versehentliches Drücken.
`GatewayUITask` ersetzt deshalb den UITask (das Logo stammt aus dem Upstream, MIT):
- Startbild 4 s, danach 3 Seiten: **Netz** (Name, WLAN, IP), **Funk** (Frequenz/SF/BW/CR/TX,
  Rauschpegel), **Mesh** (RX/TX, Weiterleitung, Uhrzeit)
- Kurzer **und** langer Druck: Anzeige an bzw. nächste Seite. Abschalten geht nur per CLI
  (`poweroff`, mit Warnung im Web).
- Auto-Aus nach 60 s (Upstream 20 s) gegen Einbrennen. **Im Installer-AP bleibt sie an** und zeigt
  „installer / installer, http://192.168.4.1“, damit die Ersteinrichtung ohne Doku klappt.
- Die Uhrzeit auf dem OLED ist Lokalzeit (`configTzTime` mit CET/CEST-Regel). Die Systemzeit (Epoch)
  bleibt UTC, so rechnet MeshCore.

## E21 – Tab „WLAN“ heißt jetzt „System“ (2026-10-14)
Dort liegen WLAN-Einrichtung, Neustart und das Firmware-Update (Version, Board, Partition und
Abbild-Status wie „Prüflauf“/„gültig“).

## E22 – M6: Sensormeter-Abfrage und Grenzwert-Meldungen (2026-10-14, autonom entschieden)
**Datenquelle:** `GET http://<adresse>:<port>/api/sensors`. Das ist bei sensormeter, -poe und -wlan
**ohne Login** abrufbar (geprüft in den Firmware-Quellen: `handleApiSensors` ruft kein
`checkAuth` auf). Ein optionaler Basic-Auth ist trotzdem einstellbar. Live geprüft mit zwei
Sensormetern im LAN (`10.10.0.148`, `…108`). Deren echte Antworten liegen als Fixtures in
`test/host/fixtures/`.

**JSON → Messwerte** (`src/SmJson.cpp`, host-getestet): Jedes Objekt der obersten Ebene ist ein
Sensor (`sensor1`, `sensor2`, bei sm-wlan `sensor`). `valid:false` überspringt alle seine Werte,
`null`/NaN/Text werden übersprungen. Der Pfad ist `sensor.feld`, z. B. `sensor2.lux`. Einheit und
Anzeigename kommen aus dem Feldnamen (temperature °C, humidity %, pressureHpa hPa, lux lx, eco2Ppm ppm).

**Regel-Logik** (`src/RuleEngine.cpp`, host-getestet, ohne Arduino):
- Bedingung „über“ oder „unter“ einem Grenzwert
- **Mindestdauer**: Ein Ausreißer, der vorher wieder verschwindet, löst nichts aus
- **Hysterese**: Entwarnung erst bei Grenzwert ∓ Hysterese, gegen Flattern. Standard in der UI: 0,5.
- **Erinnerung** optional alle N Minuten, solange der Wert verletzt bleibt
- **Entwarnung** optional
- Gerät **offline** nach N Fehlversuchen in Folge (Standard 3) und **wieder online**, je eine
  Meldung (pro Gerät abschaltbar)
- **Sendebegrenzung**: max. N Meldungen pro gleitender Stunde (Standard **6**). Was darüber
  hinausgeht, wird nur protokolliert, nicht gesendet. Grund: Jede Meldung ist ein Flood im ganzen
  Netz, und hansemesh nennt 600–700 Pakete pro Stunde als Kapazität des *gesamten* Kanals.
- Die Zeitlogik ist `millis()`-überlaufsicher (getestet über den Wrap)

**Ausführung:** eigener FreeRTOS-Task `smpoll` auf **Core 0** (WLAN-Core, Priorität 1, 8 KB Stack). HTTP
mit 3 s Connect- und 4 s Lese-Timeout, Antworten über 4 KB werden verworfen. Meldungen gehen über die
`MeshActions`-Queue in den Mesh-Loop (E4). Der Mesh-Loop auf Core 1 wird nie blockiert.

**Entscheidungen:**
- **Ohne eigenes WLAN fragt das Gateway nicht ab und zählt keine Fehlversuche.** Sonst würde
  ein WLAN-Ausfall des Gateways alle Sensormeter per Funk als OFFLINE melden (Funk geht ja weiter).
- **Zustände bleiben beim Speichern erhalten**, solange Gerät (Adresse und Port) und Regel
  (Messwert, Bedingung, Grenzwert, Hysterese, Mindestdauer) gleich bleiben.
  **Fund aus dem UI-Test:** Die erste Fassung setzte bei jedem Speichern alles zurück, und ein
  anstehender Alarm wurde bei *jeder* Konfigurationsänderung erneut gesendet. Das ist behoben und
  per Test abgesichert.
- **Kanal pro Regel** oder der **Standardkanal**. Offline- und Online-Meldungen gehen immer in den
  Standardkanal. Ohne gültigen Kanal wird nichts gesendet, sondern nur protokolliert.
- **Texte:** Pro Regel eigene Vorlage oder die globale Vorlage. Platzhalter: `{state} {device}
  {metric} {value} {unit} {op} {threshold}`. Die Standardtexte sind kurz gehalten (Airtime), etwa
  „ALARM Heizraum: Temperatur 35°C (>30°C)“. Der Knotenname wird vom Kanal-Senden davorgesetzt
  (max. 160 Byte gesamt).
- **Testnachricht** pro Regel. Sie zählt nicht zum Limit und trägt immer „TEST“ (bei einer eigenen
  Vorlage ohne `{state}` als Präfix).
- **Grenzen:** max. 8 Geräte und 16 Regeln, Intervall 10–3600 s (Standard 60 s).
- **Speicherung:** JSON-Blob in NVS (`gw/smcfg`), übersteht MeshCore-`erase`. Passwörter liefert
  die API maskiert („***“), beim Speichern bleibt „***“ das alte Passwort (Zuordnung über `origIdx`).
- **Gerät löschen** entfernt auch dessen Regeln und nummeriert die übrigen um (macht die UI).
- ⚠ **Bekannte Grenze:** Regeln merken sich den Kanal als **Index**. Wird ein Kanal gelöscht und
  später ein anderer in denselben Platz angelegt, sendet die Regel dorthin. Die UI zeigt den
  Kanalnamen an, das fällt also auf. Eine Lösung über Kanalnamen wäre robuster (später).
- ⚠ **Nicht im Backup:** Die Sensormeter-Konfiguration ist im MeshCore-/Kanal-Backup (M3/M4) noch
  nicht enthalten.
- ⚠ **Keine mDNS-Namen** (`*.local`): HTTPClient löst nur DNS auf, also IP oder DNS-Namen eintragen.

**Verifikation:** Host-Tests Regel-Engine + JSON (u. a. echte Antworten der LAN-Sensormeter) grün
bei `-O2`. UI-Test gegen Mock **78/78**, davon 18 Sensormeter-Prüfungen. Dabei fragt der Mock das
echte Sensormeter `.148` live ab. **Nicht HW-verifiziert:** Poller-Task und HTTPClient auf dem ESP
sowie die tatsächliche Kanalnachricht per Funk.

## E23 – Qualitätschecks über alle Meilensteine (2026-10-14)
- **Warnungen:** Upstream baut mit `-w`. Ein einmaliger Build mit `-Wall -Wextra` (ohne `-w`) zeigt
  **0 Warnungen in `src/`**. Alle 398 Warnungen stammen aus Upstream-Headern und Bibliotheken.
- **Testfehler gefunden:** Im Host-Test las `check(…, f()==-1, err)` den Puffer `err` je nach
  (unspezifizierter) Argument-Reihenfolge, bevor `f()` ihn füllte. Das führte zu einem Absturz bei `-O1`. Der Fehler lag im
  Test, nicht im Produktcode, und ist korrigiert.
- `tools/host-test.ps1`: `ErrorActionPreference = Continue`, weil PowerShell 5.1 sonst jede
  stderr-Zeile von pio/g++ als Abbruch wertet.
- Nach Änderungen an `lib_deps` baut PlatformIO die Build-Ordner neu auf, dann fehlen die
  `firmware.bin` des jeweils anderen Envs. Der UI-Test prüft das jetzt vorab mit klarer Meldung.

## E24 – Nach Rückfrage (2026-10-14): Limit, Scope, Lücken, Repo
Antworten des Users auf die Fragen am Ende von M6:
- **Sendebegrenzung 6/h** bestätigt.
- **Scope „rhein-main“** für den Alarm-Kanal: Beim Anlegen eines Kanals ist das Scope-Feld mit
  `rhein-main` vorbelegt (änderbar, leer = ohne Scope). Es gibt bewusst keine harte Vorgabe in der
  Firmware: Ein Scope pro Kanal bleibt wie in der App die richtige Ebene.
- **Lücke 1 behoben:** Regeln und der Standardkanal verweisen jetzt per **Kanalname** statt per
  Platznummer (`Rule.channel`, `Settings.defaultChannel` als `char[32]`,
  `channel_store::findByName`). Ein gelöschter Kanal erscheint in der UI als „Name (fehlt!)“, und
  es wird nichts gesendet (Log-Eintrag). Legt man ihn mit demselben Namen neu an (auch an anderer
  Platznummer), greift die Regel wieder. Per UI-Test abgesichert.
- **Lücke 2 behoben:** Das Backup enthält die Sensormeter-Konfig (`sensormeter`). Geräte-Passwörter
  sind nicht enthalten, weil die API sie nur maskiert liefert und Sensormeter für `/api/sensors` keins
  brauchen. Restore spielt sie **nach** den Kanälen ein, weil die Regeln per Name auf sie verweisen.
- **Repo:** privates GitHub-Repo (wie home-central).
- Version 0.5.1. UI-Test **85/85**, Host-Tests grün, beide Boards bauen.

## E25 – Ping-Check (2026-10-14, Anforderung User, Details autonom entschieden)
Vorgabe: max. 5 Ziele, Name oder IP als Ziel, Bezeichner Pflicht, Schwellwert in Sekunden,
bei Überschreitung Nachricht in den Alarmkanal.

**Auslegung (bitte bestätigen):** Der Schwellwert ist die **maximale Ausfallzeit**. Antwortet
ein Ziel länger als X Sekunden auf keinen Ping, geht *eine* Meldung in den Alarmkanal. Antwortet es wieder,
folgt eine Entwarnung. Nicht gemeint ist eine Antwortzeit (RTT) in Sekunden; die RTT wird nur angezeigt.

**Umsetzung:**
- Konfig in der Sensormeter-Konfig (`pings[]`, max. 5, NVS, im Backup): Bezeichner (Pflicht),
  Name/IP, Schwellwert **10–65535 s** (Standard 60), aktiv.
- Prüfung **alle 10 s** pro Ziel im Poller-Task (Core 0): **3 ICMP-Echos** im Abstand von 200 ms
  mit 1 s Timeout über die IDF-API `esp_ping` (`ping/ping_sock.h`). **Eine Antwort genügt.**
  Hostnamen werden per DNS aufgelöst (`WiFi.hostByName`), die IP erscheint in der UI.
- Logik `rules::pingResult()` (host-getestet): Die Ausfallzeit zählt ab der letzten Antwort bzw. ab dem
  ersten Versuch, falls das Ziel seit dem Start nie geantwortet hat. Es gibt genau eine DOWN-Meldung pro
  Ausfall, Aussetzer unter der Schwelle lösen nichts aus, und die Logik ist millis()-überlaufsicher.
- **Alarmkanal = Standardkanal** der Überwachung (umbenannt). Es gilt dieselbe Sendebegrenzung
  (6/h) wie für die Sensormeter-Meldungen.
- Texte änderbar, Standard: `PING-ALARM {device} ({host}) seit {value} s ohne Antwort` bzw.
  `PING-OK {device} ({host}) antwortet wieder`. Neuer Platzhalter `{host}`.
- Ohne eigenes WLAN wird nicht gepingt (wie bei den Sensormetern). Nach der Rückkehr des WLANs
  starten die Ausfall-Uhren neu, sonst wäre der erste Fehlversuch sofort ein Alarm.
- Beim Speichern behält ein Ziel mit gleicher Adresse seinen Zustand (kein erneuter Alarm).
- Der Tab heißt jetzt **„Überwachung“** (Sensormeter + Ping-Check).

**Gefunden beim Testen:**
- **Produkt:** Die Speichern-Meldung erschien, bevor das Neuladen fertig war. Der anschließende
  Formular-Reset konnte schnelle Folgeeingaben löschen. Jetzt wird erst neu geladen und dann gemeldet.
- **Host-Test-Umgebung:** Abstürze bei `-O1`/`-O2` kamen von einer **fremden `libstdc++-6.dll`**
  (Git/mingw64) zur Laufzeit (ABI-Konflikt mit WinLibs g++ 16). Die Host-Tests werden jetzt **statisch**
  gelinkt und mit **gnu++11** gebaut, wie die Firmware (Arduino-ESP32 2.0.x).
- `TemplateVars` ohne Default-Member-Initializer: Unter C++11 wäre die Struktur sonst kein Aggregat mehr.

**Verifikation:** Host-Tests inkl. 11 Ping-Fälle grün. UI-Test **95/95**. Der Mock pingt dabei
**echt**: das Sensormeter `.148` antwortet in ca. 3 ms, `192.0.2.1` (TEST-NET) nie, und der Alarm kommt
nach 10 s im Alarmkanal an. **Nicht HW-verifiziert:** `esp_ping` auf dem ESP32.
Version 0.6.0.

## E26 – Ping-Check: Auslegung bestätigt, Prüfabstand pro Ziel (2026-10-14, User)
User-Antwort zu E25: (1) Schwellwert = maximale Ausfallzeit **bestätigt**, (2) Entwarnung PING-OK
**bestätigt**, (3) Prüfabstand **pro Ziel einstellbar**.

**Umsetzung:**
- Neues Feld `intervalS` je Ziel: **10–3600 s**, Standard 10 s. Fehlt das Feld (Konfig/Backup vor 0.6.1),
  gilt 10 s, also das bisherige Verhalten. Gespeichert wird weiter als JSON in NVS, eine Migration ist nicht nötig.
- Der Schwellwert muss **mindestens so groß wie der Prüfabstand** sein (sonst Fehlermeldung beim Speichern).
  Kleinere Werte wären ohnehin erst bei der nächsten Prüfung wirksam.
- Pro Prüfung weiter 3 Echos, eine Antwort genügt. Der Alarm kommt frühestens nach dem Schwellwert und spätestens
  nach Schwellwert + Prüfabstand.
- UI: Feld „Prüfabstand (s)“ im Formular, Tabelle zeigt „60 s / alle 30 s“.

**Verifikation:** Build V4 + R8 grün (0 Warnungen in src/), Host-Tests grün, UI-Test **100/100**
(neu: Prüfabstand < 10 s abgelehnt, Schwellwert < Prüfabstand abgelehnt, Anzeige, Bearbeiten, Backup).
Version 0.6.1. **Nicht HW-verifiziert.**

## E27 – Dauerhaftes Ereignis-Log im Flash (2026-10-14, User: "log downloadbar, sinnvolle Größe, die den Flash schont")
Bisher gab es nur RAM-Ringpuffer (Konsole, Pakete, Kanäle, Meldungen), die bei jedem Neustart leer sind.

**Umsetzung (`src/EventLog.*`):**
- Datei `/gwlog.txt` auf dem vorhandenen SPIFFS (3,4 MB, gemeinsam mit MeshCore), Wechsel zu `/gwlog.1.txt`
  bei **64 KB**, also **höchstens 128 KB** (ca. 1500–2000 Zeilen). Das ältere Drittel fällt weg, nicht alles.
- **Flash-Schonung:** Zeilen sammeln sich in einem 8-KB-RAM-Puffer (PSRAM). Geschrieben wird erst ab **4 KB**, spätestens
  nach **15 min**, und vor jedem Neustart (`esp_register_shutdown_handler`, greift bei `ESP.restart()`, CLI `reboot`,
  OTA). Bei normalem Betrieb (einige KB pro Tag) sind das wenige Schreibvorgänge pro Stunde. SPIFFS verteilt sie über
  die ganze Partition, also kein messbarer Verschleiß. Preis: Bei Absturz oder Stromausfall fehlen höchstens die
  letzten 15 min. Der Startgrund (`esp_reset_reason`, z. B. „ABSTURZ (Panic)“, „Task-Watchdog“, „Unterspannung“)
  steht in der ersten Zeile nach jedem Start.
- **Inhalt:** Systemmeldungen (Start, NTP, OTA, Profil), WLAN-Ereignisse, CLI-Befehle aus Web **und** serieller
  Konsole mit Antwort, Meldungen der Überwachung (gesendet/unterdrückt), vom Gateway gesendete Kanalnachrichten.
  **Nicht:** Paketverkehr und empfangene Kanalnachrichten (Volumen, bleiben in den Live-Logs).
- **Geheimnisse:** `password …`, `set prv.key`, `set guest.password`, `set bridge.secret` werden maskiert, Antworten auf
  `get prv.key`/`get guest.password`/`password` nie geschrieben. Dabei gefunden: Die **serielle** Konsole schrieb
  `password <neu>` bisher im Klartext ins Web-Konsolen-Log. Jetzt ist es dort auch maskiert.
- Zeilenformat `2026-10-14 07:12:03 CMD   set tx 17`, vor NTP `+12s SYS …`.
- Web (System-Tab): Belegung, Herunterladen (`/api/log/download`, Dateiname mit Hostname), Anzeigen (`?inline=1`),
  Löschen (`POST /api/log/clear`), Info (`/api/log/info`).
- Upstream-Befehl `log start` (Paket-Log `/packet_log` von MeshCore) bleibt unverändert und unabhängig davon.

## E28 – Backup vollständig (2026-10-14, User: "können alle Einstellungen heruntergeladen und wieder eingespielt werden?")
Bisher fehlten: Admin-Passwort, WLAN/Hostname, Zugriffsliste (ACL), Sensormeter-Passwörter.
Upstream gibt die ACL nur seriell aus (`get acl` → `Serial.printf`), das Admin-Passwort ist per CLI nicht lesbar.

**Umsetzung (Backup-Format version 2, abwärtskompatibel beim Einspielen von v1):**
- `GET /api/backup/extra[?secrets=1]` liefert Hostname, WLAN-SSID, ACL (aus `/s_contacts`, Satzformat wie
  `ClientACL::save`, 136 Byte) und die Überwachungs-Konfig. Mit `secrets=1` zusätzlich WLAN-Passwort,
  Admin-Passwort und Sensormeter-Passwörter unmaskiert.
- ACL landet als offizielle CLI-Befehle `setperm <pubkey> <perm>` in `commands`. `get acl` funktioniert jetzt auch
  in der Webkonsole (liest die Datei statt Serial).
- Haken „Schlüssel und Passwörter einschließen“: prv.key, Admin-, Gast-, WLAN- und Sensormeter-Passwörter.
  Ohne Haken sind auch `guest.password` und die Passwörter **nicht** enthalten (vorher war guest.password immer drin).
  Dateiname dann mit `-VOLL`.
- Einspielen: CLI-Befehle → Kanäle → Überwachung → zuletzt `POST /api/backup/apply` (Admin-Passwort per
  `password`-Befehl und/oder WLAN+Hostname mit Neustart nach 3 s). Serverseitig in einem Rutsch, weil der Browser
  nach der Passwortänderung keinen gültigen Login mehr hat. Beides sind Optionen beim Einspielen. Admin-Passwort
  ist vorbelegt, WLAN nicht (Gerätetausch vs. gleiches Gerät). WLAN ist nur wählbar, wenn das Passwort im Backup ist
  oder die SSID gleich bleibt (Server prüft das ebenfalls).
- Nach dem Einspielen wird die Dateiauswahl zurückgesetzt. Gefunden im Test: Dieselbe Datei erneut zu wählen,
  löste sonst kein `change` aus.
- Nicht im Backup (bewusst): Nachbarn, Statistiken, Logs, Uhrzeit.
- **Restgrenze:** MeshCore schreibt `/s_contacts` verzögert. Ein `setperm` unmittelbar vor dem Backup kann
  fehlen.

**Nebenbei:** Zwei `-Wmissing-field-initializers`-Warnungen aus M7 (`TemplateVars::host`) beseitigt. Der Build mit
`-Wall -Wextra` zeigt wieder **0 Warnungen in `src/`**.

**Verifikation:** Build V4 + R8 grün, Host-Tests grün, UI-Test **119/119** (neu: volles und reduziertes Backup,
ACL, Restore mit Admin-Passwort + WLAN, WLAN-Sperre ohne Passwort, Log-Inhalt/Maskierung/Download/Löschen).
Version 0.7.0, firmware.bin 1,33 MB (20 % des App-Slots). **Nicht HW-verifiziert.**

## E29 – Erster Hardware-Test: sechs Fehler gefunden und behoben (2026-10-14, 0.7.1)

Platine: Standard-Heltec-V4 (ESP32-S3 rev v0.2, 2 MB QSPI-PSRAM, 16 MB Flash), `gateway_heltec_v4`.
Erstes Flashen per USB (erase + upload), danach alle Updates per OTA über die Web-API.

Gefunden am echten Gerät (Mock und Host-Tests konnten das nicht zeigen):

1. **Rollback-Schutz wirkungslos.** `verifyRollbackLater()` war C++ ohne `extern "C"`. Der Arduino-Core
   deklariert die schwache Funktion in C, unsere Definition bekam ein anderes Symbol, der Core bestätigte jede
   neue Firmware sofort (Status „gültig“ nach 8 s). Fix: `extern "C"`. Danach am Gerät: „Prüflauf“ → nach 62 s
   „gültig“. Echter Rollback-Test mit einem Build, der nach 20 s `abort()` ruft: Bootloader startet wieder die
   vorherige Partition. Neue Log-Zeile „ROLLBACK: neue Firmware auf appX ist im Prüflauf abgestürzt“.
2. **`/api/sm` und `/api/backup/extra` lieferten kaputtes JSON.** ArduinoJson 7 ersetzt mit
   `serializeJson(doc, String&)` den Inhalt, statt anzuhängen. Damit fehlten `{"config":` bzw. Hostname, WLAN
   und ACL. Folge: Überwachungs-Tab und Backup-Download wären auf HW kaputt gewesen. Fix: erst in ein eigenes
   String serialisieren, dann anhängen.
3. **„Änderungen speichern“ blockiert.** MeshCore gibt float32 ungerundet aus (`txdelay` 0.6999999, Frequenz
   869.6179809). In ein Zahlenfeld mit `step=0.1` geschrieben, verweigert der Browser das Absenden. Fix: beim
   Laden auf 4 Nachkommastellen runden (nicht lat/lon). Das Backup schreibt jetzt `set radio 869.618,…`.
4. **Profilvergleich zeigte die Frequenz als Abweichung** (feste Toleranz 1e-6). Fix: relative Toleranz 1e-6.
5. **Log-Zeiten nach Neustart in UTC.** Die Uhr überlebt einen Software-Neustart, die Zeitzone wurde aber erst
   mit NTP gesetzt. Fix: TZ schon in `setup()`.
6. **Neustart über Weboberfläche und WLAN-Speichern standen nicht im Ereignis-Log.** Ergänzt. Dazu die
   Kopfzeile von `get acl` im Log. Startgrund `ESP_RST_UNKNOWN` (z. B. USB-Reset nach dem Flashen) heißt
   jetzt so, andere Codes werden als Zahl ausgegeben. Das IDF des Frameworks kennt `ESP_RST_USB` noch nicht.

Mock angepasst: `get radio`/`txdelay` liefern float32-Werte wie die HW. Gegenprobe: Der UI-Test schlägt mit
dem alten `index_html.h` fehl. Neu: `tools/webui-test/live_check.js`, eine Nur-Lese-Prüfung der Oberfläche
gegen ein echtes Gerät.

Am Gerät bestätigt: Profil (22 Werte) wie empfohlen, Login (altes Passwort 401), OTA-Ablehnung (Textdatei,
R8-Image), OTA gleiche Version, Partitionswechsel, Shutdown-Flush des Logs, Repeater leitet Pakete weiter
(Grundrauschen −109 dBm, Nachbar RSSI −71…−76 dBm).

**Verifikation:** Build V4 + R8 grün, 0 Warnungen in `src/`, Host-Tests grün, UI-Test 119/119, Live-Check 6/6.

## E30 – Chiptemperatur als Pseudo-Gerät „Gateway (intern)“ (2026-10-14, User: „gleich einbauen“), 0.7.2

- Das Heltec V4 hat keinen Umgebungssensor. Gemessen wird `temperatureRead()`, also die Temperatur im
  ESP32-S3. Sie liegt 15–40 °C über der Raumtemperatur und eignet sich als Überhitzungswächter, nicht als
  Raumthermometer. Die Oberfläche sagt das („Chip, nicht Raum“).
- Messung alle 60 s im Poller-Task, **vor** der Prüfung von WLAN und „Überwachung an“. Gemessen wird immer,
  Regeln wertet das Gateway auch ohne WLAN aus, weil die Meldung per Funk rausgeht.
- Regeln verweisen mit `device = 100` (`sm::INTERNAL_DEVICE`) auf das Pseudo-Gerät, Messwert `gateway.chipTemp`
  („Chiptemperatur“, °C). Damit funktionieren Hysterese, Mindestdauer, Erinnerung, Entwarnung, Meldungslimit,
  Test-Button und Backup unverändert. `{device}` in Meldungen = „Gateway“. `sm::deviceOf()` kapselt den
  Sonderfall. Beim Löschen eines Sensormeters wird `device 100` nicht umnummeriert.
- `/api/status` liefert `gateway.chip_temp_c`, der Status-Tab zeigt sie an. `/api/sm` liefert `status.internal`, im
  Überwachungs-Tab steht eine feste Zeile. Im Ereignis-Log gibt es eine Zeile beim Start und dann stündlich.

**Kanalschlüssel-Anzeige (User-Meldung):** „anzeigen“ zeigte Base64 (`YaNVHXWm…`), die MeshCore-App zeigt Hex
(`61a3551d…`). Der Schlüssel war derselbe, die Anzeige wirkte aber wie ein anderer Schlüssel. Jetzt steht Hex
vorne, Base64 klein darunter.

**Verifikation:** Build V4 + R8 grün, 0 Warnungen in `src/`, Host-Tests grün, UI-Test 123/123 (neu: interne
Zeile, Status, Regel → ALARM-Text, Umnummerierung, Hex-Schlüssel). Am Gerät per OTA: Live-Check 9/9,
Chiptemperatur 51,7 °C, Log-Zeile, Schlüssel `haus-alarm` = Hex aus der App.

## E31 – OLED-Seite 4 „Nachricht“ (2026-10-14, User: „bau nur die Display-Seite“), 0.7.3

- Vorgeschlagen war zusätzlich ein „Kanal-Lauscher“, der #-Kanäle per Namensliste und Prüfsumme erkennt
  (Pakete tragen nur 1 Byte Kanal-Hash, der Name ist nicht rückrechenbar). Der User hat sich **nur für die
  Display-Seite** entschieden, der Lauscher wird nicht gebaut.
- Seite 4 zeigt die neueste Zeile des Nachrichten-Logs (`channel_log`, empfangen **oder** gesendet, damit
  auch Alarme des Gateways): Kanalname links, Uhrzeit rechts (ohne gültige Uhrzeit das Alter „-5m“/„-2h“),
  darunter bis zu 4 Zeilen Text mit Wortumbruch (21 Zeichen, hart getrennt nur Wörter länger als eine Zeile,
  Rest „...“), Fußzeile „SNR x hop“ bzw. „gesendet“.
- Neu `LogBuffer::latest()`. Kein eigener Speicher, kein Aufwecken der Anzeige bei neuer Nachricht.
- Umlaute: Der SSD1306-Treiber nutzt Code Page 437. UTF-8 wird für ä ö ü Ä Ö Ü ß ° é umgesetzt, alles
  andere (Emojis) wird „?“. Upstreams `translateUTF8ToBlocks` hätte Umlaute als Blöcke gezeigt.

**Verifikation:** Build V4 + R8 grün, 0 Warnungen in `src/`, Host-Tests grün. Die Seite selbst per
PC-Test mit dem echten Code (21×6-Zeichenraster statt OLED): leer, empfangen mit Umlauten, gesendeter
Alarm ohne Uhrzeit, Überlänge mit Emoji und Langwort. Dabei gefunden und behoben: Umbruch trennte mitten im
Wort, wenn das letzte Leerzeichen in der ersten Zeilenhälfte lag. Auf dem Gerät noch nicht gesehen.

## E32 – Hostname folgt dem Gerätenamen (2026-10-14, User: „bau variante 2“), 0.7.4

- Anlass: WLAN-Name `meshcore-gw` wich vom LoRa-Namen `DE-XX-ORT02-GW` ab. Beide waren getrennte
  Einstellungen, der Hostname war mit `meshcore-gw` vorbelegt.
- Gespeicherter Hostname **leer = automatisch**: wirksamer Hostname = bereinigter MeshCore-Gerätename
  (a-z, 0-9, „-“, Leerzeichen/_ → „-“, max. 31 Zeichen wegen des 32-Byte-Puffers im WLAN-Treiber; bleibt nichts
  übrig → `meshcore-gw`). Ein eigener Hostname bleibt möglich (Haken „wie Gerätename (LoRa)“ entfernen).
- **Migration:** Der alte Werkswert `meshcore-gw` (wurde beim WLAN-Einrichten immer mitgespeichert) gilt als
  automatisch, beim Laden aus NVS, beim Speichern und beim Einspielen alter Backups (`GatewayConfig::hostSetting`).
  Folge: `meshcore-gw` lässt sich nicht als fester eigener Name wählen.
- **Zur Laufzeit:** `NetworkManager` prüft jede Sekunde den Gerätenamen (egal ob per Web, CLI oder Fern-Admin
  geändert). Ist er 3 s stabil anders, wird der Hostname gesetzt, ins Ereignis-Log geschrieben und die Station
  verbindet kurz neu (`WiFi.reconnect()`), damit DHCP den neuen Namen an den Router meldet. Kein Neustart.
- Der Hostname liegt als `char[32]` unter Spinlock, weil der Web-Task ihn liest, während der loopTask ihn ändern kann.
- API: `/api/status` → `wifi.hostname` (wirksam) + `wifi.hostname_auto`; `/api/wifi` mit `hostname` leer =
  automatisch, Feld fehlt = unverändert; Backup `gw.hostname` leer = automatisch. Status-Tab: „… (wie Gerätename)“.
- Die Fritzbox kann den alten Namen weiter anzeigen, bis sie den Eintrag aktualisiert (Heimnetz → Netzwerk).

**Verifikation:** Build V4 + R8 grün, 0 Warnungen in `src/`, Host-Tests grün, UI-Test 131/131 (neu: automatisch =
Gerätename, Feld gesperrt, Status-Hinweis, folgt `set name`, eigener Name bereinigt gespeichert, zurück auf
automatisch, Altwert `meshcore-gw`, Backup leer, Log-Dateiname). Am Gerät noch offen.

## E33 – Repo öffentlich, Lizenz GPL-3.0 (2026-10-14, User: „auf public stellen“, Lizenz „GPL-3.0“)

- Vor dem Umstellen die gesamte Historie auf Geheimnisse geprüft: keine Passwörter, kein privater Schlüssel,
  keine WLAN-Daten. Kanalschlüssel `haus-alarm` nur gekürzt (4 von 16 Byte) in E30, private LAN-IPs und der
  ohnehin per Funk öffentliche Knotenname bleiben.
- Autor-E-Mail in allen Commits und Tags auf die GitHub-noreply-Adresse umgeschrieben (Inhalt und Datum gleich,
  Commit-Kennungen neu). Sicherung vorher als Bundle außerhalb des Repos.
- Versions-Tags `v0.1.0` … `v0.7.4` nachgezogen, jeweils auf dem Commit, der die Version einführt.
- Lizenz GPL-3.0 (`LICENSE`); MeshCore-Anteile (Submodul, Logo, übernommene Stellen) bleiben MIT, Hinweis in `NOTICE`.

## E34 – Nachbarsuche nach dem Start + Nachbarn im Status-Tab (2026-10-14, User: „bau das mit Webanzeige“), 0.7.5

- Anlass: `neighbors` war nach den Updates leer, obwohl Pakete über Repeater ankamen. Upstream trägt Repeater nur
  bei einem **0-Hop-Advert** (alle `advert.interval`, hansemesh 240 min) oder als Antwort auf `discover.neighbors`
  ein, die Liste steht nur im RAM. Ein manuelles `discover.neighbors` fand sofort A1B2C3D4 mit +12,25 dB.
- **90 s nach dem Start** ruft der loopTask einmal `MyMesh::sendNodeDiscoverReq()` auf (ein 0-Hop-Steuerpaket,
  Antworten innerhalb 60 s, Antwortende sind upstream auf 4 je 2 min begrenzt). Ereignis-Log: „Nachbarsuche nach dem Start
  gesendet“. Keine periodische Suche, danach füllen die Adverts der Nachbarn die Liste wie bisher.
- **Status-Tab „Nachbarn“:** Kennung (8 Hex = Anfang des Public Keys), „vor x min/h“, SNR in dB (Upstream-Rohwert / 4).
  Aktualisierung beim Öffnen und alle 30 s per `neighbors` im Quiet-Modus; `neighbors` zählt dafür jetzt als reine
  Abfrage (`CliBridge`), landet also weder in der Konsole noch im Flash-Log. Knopf „Jetzt suchen“ sendet
  `discover.neighbors` und aktualisiert 60 s lang alle 5 s.
- Namen der Nachbarn zeigt die Liste nicht: der Repeater speichert zu Nachbarn nur Schlüssel, Zeit und SNR.

**Verifikation:** Build V4 + R8 grün, 0 Warnungen in `src/`, Host-Tests grün, UI-Test 136/136 (neu: Tabelle aus
`neighbors`, nicht in der Konsole, Suche mit Countdown, neuer Nachbar, Knopf gesperrt). Am Gerät noch offen.

## E35 – Web-Flasher mit Board-Prüfung (2026-10-14, User: „webflasher für beide varianten, vorher ein test damit man die korrekte firmware flasht“)

- `docs/index.html` über GitHub Pages (Quelle `/docs`, `.nojekyll`), eigene Seite mit **esptool-js 0.6.1** (jsDelivr,
  gepinnt) statt ESP Web Tools: Dessen Manifest unterscheidet nur die Chipfamilie, V4 und V4-R8 sind beide ESP32-S3.
- **Test vor dem Flashen** (gleiche Verbindung wie das Schreiben):
  1. Chip muss ESP32-S3 sein, Flash 16 MB (`detectFlashSize`).
  2. eFuse **PSRAM_CAP** (`getPsramCap`) wählt die Variante: 2 = 2 MB = V4 (ESP32-S3R2, QSPI), 1 = 8 MB = V4-R8
     (ESP32-S3R8, OPI). Alles andere → „Nicht geeignet“, Verbindung getrennt, nichts geschrieben.
  3. Dateien gegen `docs/fw/manifest.json` (Größe + SHA-256), `firmware.bin` muss das Merkmal
     `MCGW-FW-ID:<Board>:<Version>:MCGW-FW-END` tragen (fängt vertauschte Dateien auf dem Server).
  4. Direkt vor dem Schreiben PSRAM_CAP erneut lesen (Board gewechselt → Abbruch).
- Schreiben wie PlatformIO-Upload: DIO, 80 MHz, 16 MB, komprimiert; bootloader 0x0, partitions 0x8000,
  **boot_app0 0xe000** (setzt otadata zurück, sonst bliebe ein Gerät, das zuletzt aus app1 lief, dort), firmware 0x10000.
  esptool-js prüft jede Datei nach dem Schreiben per MD5 (crypto-js 4.2.0 von cdnjs). Option „Flash vorher komplett
  löschen“ standardmäßig an (Erstinstallation); Updates laufender Gateways besser per OTA (behält alles).
- `tools/flasher-update.ps1 [-Build]` kopiert beide Builds nach `docs/fw/{v4,r8}/`, prüft das Merkmal mit der aktuellen
  `GATEWAY_VERSION` und schreibt das Manifest. Bei jedem Release ausführen und `docs/fw/` mitcommitten.
- Bekannt: Die Seite selbst ist nicht mit echter Hardware getestet (Web Serial braucht die Port-Auswahl durch den
  Nutzer). Die PSRAM-Zuordnung deckt sich mit dem esptool-Ergebnis am echten Standard-V4 („Embedded PSRAM 2MB“, E29).

**Verifikation:** `tools/webui-test/flasher_test.js` 29/29 gegen die echte Seite mit nachgebauten Boards: Wahl V4/R8,
Ablehnung (ohne PSRAM, klassischer ESP32, 8 MB Flash), SHA-256-Fehler, R8-Firmware als V4 (Merkmal), kompletter
Ablauf V4 (4 Dateien, Adressen, Parameter, MD5 = Node-MD5, Neustart) und R8 ohne Löschen, Board-Wechsel vor dem
Schreiben, kein Gerät gewählt, CDN-Bibliotheken ladbar, Handybreite. Dabei gefunden: `dl{display:grid}` überstimmte
`hidden` (alte Chipdaten blieben stehen) → `[hidden]{display:none!important}`.

## E36 – Regionen raus aus der Empfehlung (2026-10-14, User: „erst die Regionen-Empfehlung umbauen“), 0.7.6

- Bisher legte das Profil beim Erststart `de-he`, `de-by`, `rhein-main` an (E15) und meldete fehlende als Abweichung.
  Mit dem öffentlichen Web-Flasher bekommen das auch Gateways in anderen Gegenden: dort falsche Regionen, und
  „Abweichungen übernehmen“ hätte sie nach einer Korrektur wieder angelegt.
- Regionen sind standortabhängig, es gibt keine für alle richtige Vorgabe. Deshalb **nicht einstellbar gemacht,
  sondern ganz aus dem Profil genommen** (19 statt 23 Einträge). Erststart legt keine Regionen an.
- Stattdessen warnt **Einstellungen → Regionen** („Keine Region angelegt …“ mit Link ins Admin-Handbuch), solange der
  Baum aus `region` nur `*` enthält.
- Bestehende Geräte behalten ihre Regionen (MeshCore-Prefs), Backups enthalten sie weiterhin.
- Admin-Handbuch Abschnitt 4/5 angepasst.

**Verifikation:** Build V4 + R8 grün, 0 Warnungen in `src/`, Host-Tests grün, UI-Test 140/140 (neu: Profil ohne
Regionen, Warnung bei leerem Baum, Profil-Übernahme legt keine Regionen an, von Hand angelegt → Warnung weg, Profil
bleibt „Alles wie empfohlen“). Live-Check prüft zusätzlich: Gerät hat Regionen, keine Warnung.

## E37 – MeshCore auf repeater-v1.18.0 (2026-10-15, User: „mach 1–4“)

- Submodul `external/MeshCore` von `repeater-v1.17.1` auf `repeater-v1.18.0` (266 Upstream-Commits).
- **CLI-Umbau upstream:** Funkbefehle (`radio`, `tx`, `dutycycle`, `af`, `txdelay`, `direct.txdelay`, `rxdelay`,
  `cad`, `int.thresh`, `agc.reset.interval`, `path.hash.mode`, `multi.acks`, `radio.rxgain`) liegen jetzt in
  `CommonRadioPrefs::handleCommand`. Namen und Antwortformat (`> wert`) unverändert → Profil und Einstellungsseite
  brauchen keine Änderung. `radio.fem.rxgain` behandelt jetzt `HeltecV4Board::handleCommand`, Wert über den
  KeyValueStore `fem_rxgain` (abgebildet auf dasselbe Prefs-Feld). `radio.fem.txgain` gibt es für die V4 nicht mehr
  (nutzen wir nicht). Neu: Befehl `power`.
- **`src/main.cpp` nachgezogen** (Kopie von simple_repeater): `board.loop()` je Durchlauf, Pufferfix für volle
  serielle Befehlszeile (vorher wurde `command[159]` = `'\r'` ohne NUL dahinter gesetzt).
- **Uhr:** `ESP32RTCClock` sichert die Zeit im RTC-RAM und übernimmt sie nach Absturz/WDT; ohne Sicherung Start bei
  1.3.2026 (vorher 15.5.2024). Unsere Prüfungen `time > 1700000000` (EventLog, OLED) hielten schon den alten Startwert
  für „gestellt“; unverändert gelassen, Verhalten wie vorher (falsches Datum bis NTP).
- Automatisch mit drin: keine Antwort auf Nachbarsuche, wenn beide Advert-Intervalle 0 sind (bei uns 240/48),
  Transport-Code 0 = ohne Scope, ACK-Weiterleitung im Direct-Modus mit Verzögerung, `TXT_TYPE_CLI_COMMAND`,
  `loop.detect minimal` ist jetzt Upstream-Default.

**Verifikation:** Build V4 + R8 grün (Flash je 20,5 % vom App-Slot), 0 Warnungen, Host-Tests grün, UI-Test 140/140.
**HW 2026-10-15 (OTA auf .151, app1, Image gültig nach 60 s):** alle 19 Profilwerte vor/nach identisch
(inkl. `radio.fem.rxgain` = off, Migration ok), `ver` = v1.18.0, `power` neu verfügbar, Live-Check bestanden,
Nachbarsuche nach Start findet A1B2C3D4 (SNR 12 dB), Kanalnachricht auf Public gesendet.

## E38 – Web-Flasher mit Versionsauswahl (2026-10-15, User: „lass die vorherige im Webflasher zur Auswahl, bei der wissen wir, dass sie läuft, die andere ist noch keine 24 h gelaufen“)

- `docs/fw/manifest.json` führt jetzt mehrere Versionen (`releases`, neueste zuerst) mit `status` `stable`
  (bewährt) oder `test` (neu, in Erprobung) und der zugehörigen MeshCore-Version. Dateien liegen unter
  `docs/fw/<Version>/<v4|r8>/`.
- Die Flasher-Seite bietet die Versionen in einer Auswahl an. **Vorausgewählt ist die neueste bewährte**, nicht die
  neueste: wer nichts ändert, bekommt die Version, die schon im Echtbetrieb lief. Eine Version in Erprobung zeigt
  einen Warnhinweis. Während des Schreibens ist die Auswahl gesperrt. Merkmal-Prüfung gegen die gewählte Version.
- `tools\flasher-update.ps1`: neue Version standardmäßig als `test`, `-Promote <V>` setzt sie auf bewährt,
  `-Remove <V>` nimmt eine Version samt Dateien heraus. Altes Manifest (eine Version) wird beim ersten Lauf
  automatisch umgestellt (0.7.6 → `fw/0.7.6/`, Status `stable`, Dateien unverändert, Git erkennt reine Umbenennung).
- Stand: 0.7.6 bewährt (vorausgewählt), 0.8.0 in Erprobung. Nach ≥ 24 h fehlerfreiem Lauf auf .151:
  `-Promote 0.8.0`, Download-Links im Admin-Handbuch anpassen, ggf. `-Remove 0.7.6`.
- Die alten Download-Links `fw/v4/firmware.bin` und `fw/r8/firmware.bin` gibt es nicht mehr; das Admin-Handbuch
  verlinkt jetzt versioniert.

**Verifikation:** `flasher_test.js` 38/38 lokal (neu: Versionen im Kopf, Auswahl vollständig, bewährte vorausgewählt,
Warnhinweis, MeshCore-Version in der Option, Dateien beider Versionen × beider Boards, R8-Ablauf mit 0.8.0, Auswahl
während des Schreibens gesperrt).

## E39 – Alarme „aktiv bis bestätigt“ (2026-10-15, User: „je Alarmregel einen Haken … bis bestätigt (über Antwort an das GW, mit dem Text ACK) wiederholen wir nach 5, 15 und 30 min“), 0.9.0

- Anlass: Kanalnachrichten sind Flood ohne Bestätigung und ohne Speicher; wer gerade nicht im Mesh ist, verpasst
  den Alarm. Room Server wurde abgewogen (eigene Identität, upstream nur RAM, Rollen werden upstream gerade getrennt)
  und zurückgestellt.
- Haken `ackRequired` je Regel **und je Ping-Ziel** (beides sind Alarmquellen mit eigener Zeile; Offline-Meldungen
  von Sensormetern nicht).
- **Antwort im Kanal, nicht als Direktnachricht:** Ein Repeater nimmt TXT-Nachrichten nur von angemeldeten
  ACL-Clients an (`onPeerDataRecv`), normale Nutzer kämen nicht durch. Kanalnachrichten liest das Gateway ohnehin
  (OLED-Seite 4). Bestätigt werden nur offene Alarme **desselben Kanals**.
- Format: Alarm bekommt Nummer 1..99, Text `[ACK n] …` (Präfix statt Anhang, damit es beim Kürzen auf 160 Byte
  nicht wegfällt). Antwort `ACK` = alle offenen im Kanal, `ACK n` = nur n; `@[Name]` (App-Antwort) davor erlaubt,
  Groß/klein egal; ganze Sätze („ack, schau …“) zählen nicht. Rückmeldung `ACK n bestätigt (Absender)` in den Kanal
  (ohne Sendelimit, einmal pro Bestätigung).
- Wiederholungen **5, 15, 30 min nach der ersten Meldung** (meine Auslegung von „nach 5, 15 und 30 min“), Text
  `[ACK n] WDH k/3: …`, mit „(inzw. OK)“, wenn der Wert inzwischen normal ist – die Wiederholung läuft weiter, weil
  „aktiv bis bestätigt“ gemeint ist, nicht „solange verletzt“. Danach keine weitere; bestätigbar bis 24 h.
- Sendelimit gilt auch für Wiederholungen; unterdrückt → 5 min später erneut (Stufe bleibt), mindestens 5 min
  Abstand, damit nach einer Pause nicht alle auf einmal kommen.
- Neuer Alarm derselben Quelle ersetzt den offenen Eintrag (neue Nummer). Regel geändert/gelöscht oder Haken weg →
  Eintrag weg; Index-Verschiebungen beim Löschen werden nachgezogen (`Tracker::remap`). Max. 8 offene, sonst fällt
  der älteste weg. Nur RAM (Neustart vergisst; anstehender Alarm wird dann ohnehin neu gemeldet).
- Weboberfläche: Haken in Regel- und Ping-Formular, Zustand „ACK n offen“, Abschnitt „Offene Bestätigungen“ mit
  „bestätigen“ (sendet „ACK n bestätigt (Web)“ in den Kanal, `/api/sm/ack`).
- Logik in `src/AckTracker.*` ohne Arduino, Host-Test `test/host/ack_tracker_test.cpp`; Kanal-Empfang
  (`GatewayMesh::onGroupDataRecv`, loopTask) → Queue → Poller-Task.

## E40 – Feste IP-Adresse mit Belegt-Prüfung (2026-10-15, User: „IP, Subnetz, Gate, DNS als Optionen … überprüfen ob die IP frei ist bevor sie gesetzt wird“), 0.9.0

- Felder IP, Maske, Gateway, DNS 1 (leer = Router), DNS 2; IP leer = DHCP. NVS `ip_addr/ip_mask/ip_gw/ip_dns1/
  ip_dns2`, getrennt von MeshCore. Prüfung serverseitig (`GatewayConfig::applyIp`): Maske zusammenhängend /8–/30,
  Router im Subnetz, keine Netz-/Broadcast-Adresse, IP ≠ Router.
- **Belegt-Prüfung zweistufig:** (1) im Browser vor dem Speichern über `/api/wifi/checkip` (ARP-Anfrage im
  lwIP-Thread + Ping, `src/NetCheck.*`), belegt → nicht speichern; anderes Subnetz/kein WLAN → Rückfrage.
  (2) **maßgeblich beim nächsten Start:** neue Adresse (`ip_check`) → erst per DHCP verbinden, Adresse prüfen,
  dann `WiFi.config`, danach Router per ARP/Ping prüfen. Belegt oder Router stumm → zurück auf DHCP, feste IP
  verworfen, Grund im Ereignis-Log und im WLAN-Tab (`ip_note`). So sperrt man sich nicht aus und Konflikte mit
  Geräten, die beim Speichern gerade aus waren, werden auch erkannt.
- ARP statt nur Ping, weil viele Geräte (Windows-Firewall) Ping verwerfen, ARP aber beantworten müssen. Grenze:
  ein noch gültiger ARP-Eintrag (bis 5 min) eines gerade abgeschalteten Geräts meldet „belegt“ – bewusst die
  vorsichtige Seite.
- Bekannte feste Adresse wird beim Start ohne Prüfung gesetzt. Backup enthält die IP-Felder; Wiederherstellung
  prüft beim Start immer neu (Gerät kann woanders stehen).
- Ping-Funktion aus SmPoller nach `net_check::ping` verschoben (gemeinsam genutzt).

**Verifikation E39+E40:** Build V4 + R8 grün (je ca. 21 % App-Slot), 0 Warnungen in `src/`, Host-Tests grün
(neu: Bestätigungspflicht), UI-Test 174/174 (neu: ACK-Ablauf mit Antwort im Kanal, falsche Nummer, Satz mit „ack“,
App-Erwähnung, Wiederholung, Web-Bestätigung, Ping-Haken; feste IP: Vorbelegung, belegt/eigene/frei/anderes
Subnetz, Router außerhalb, Backup, zurück auf DHCP). **Nicht auf HW getestet:** echte ARP/Ping-Prüfung, Umschalten
nach dem Neustart, Rückfall auf DHCP, ACK per App.

## E41 – Zabbix-Anbindung: Kennung im Alarmtext + Empfänger mit Trapper und ACK (2026-10-15, User: „ack soll kommen wenn die info am zabbix angekommen ist … parsebarer teil im normalen alarm text … alternativ einen sekundären alarmkanal … baue direkt in die 090“), 0.9.0

- **Gegenseite = gleiche Firmware**, Funktion „Zabbix-Weiterleitung“ (`src/ZbxForwarder.*`). Keine eigene
  Empfänger-Firmware: alles Nötige (Kanäle entschlüsseln, WLAN, Web, Backup, OTA) ist schon da; zweimal pflegen
  bei jedem MeshCore-Update wäre der einzige Effekt.
- **Weg nach Zabbix: Trapper (Zabbix-Sender-Protokoll)** statt HTTP-Agent/MQTT: Push ohne Zeitverzug, Proxy puffert,
  und `processed: 1` ist die Annahmebestätigung, an die das ACK geknüpft wird (User: ACK = angekommen). Bei
  Proxy-Hosts muss an den Proxy gesendet werden. `history.push` (API, ab 7.0) nicht genutzt: braucht Token und
  HTTPS-Handling auf dem ESP, Trapper reicht.
- **Kennung** `" #Z1;<Quelle>;<Ereignis>;<Wert>;<ACK>"` am Textende (User: im normalen Alarmtext). Text wird gekürzt,
  Kennung nie (`zbx::appendMarker`, UTF-8-sicher). Quelle = Typbuchstabe + CRC-16 aus Gerätename|Messwert|Richtung
  (Regel), Name (Ping, Gerät), stabil bei Grenzwert-/Reihenfolgeänderung. Version im Präfix (Z1) für spätere
  Formatänderungen. Alternative **Zabbix-Kanal** (User): Alarmkanal ohne Kennung, Kopie mit Kennung in eigenen
  Kanal; ACK aus beiden Kanälen gilt (`Entry::channel2`). Lebenszeichen optional (H), nur mit Kennung.
- **Gezieltes ACK** `@[Name] ACK n`: gilt nur beim Gateway dieses Namens (Groß/klein egal). Nötig, weil jedes
  Gateway seine Nummern 1..99 selbst vergibt – bei mehreren Gateways im Kanal hätte „ACK 3“ alle getroffen.
  Ohne `@[…]` wie bisher an alle.
- Empfänger: Queue aus dem loopTask, eigener Task (Core 0, 8 KB), 16 wartende Meldungen im PSRAM, Wiederholung alle
  30 s bis 1 h bei Verbindungsfehlern; abgelehnt (`failed`) = dauerhaft, kein ACK, Grund im Log. Schon gemeldete
  (Gateway, ACK-Nr) innerhalb 24 h nicht erneut an Zabbix, nur erneut bestätigen (max. alle 60 s) – fängt
  Wiederholungen ab, die vor dem ACK unterwegs waren. Kein `clock` im Request (Zabbix nimmt Empfangszeit; die
  Uhr des Empfängers ist vor NTP nicht verlässlich).
- **Template** `docs/zabbix-template-meshcore.yaml` (Format 7.0): Trapper `meshcore.event` (TEXT/JSON) je Gateway-Host,
  Trigger auf dem Master-Item mit `MULTIPLE` + Tag-Korrelation `src` (Problem bei ev A/F, Ende bei state OK).
  **Nicht** als Trigger-Prototyp über LLD: der erste Wert einer neuen Quelle kommt an, bevor die Discovery das Item
  angelegt hat – der allererste Alarm ginge verloren. LLD zusätzlich für Zustand/Text/Wert je Quelle (Verlauf).
  Empfänger-Template mit `meshcore.rx.heartbeat` + nodata.

**Verifikation:** Build V4 + R8 grün (21,3 % App-Slot), 0 Warnungen in `src/`, Host-Tests grün (neu: Zabbix-Format:
Kennung bauen/lesen/kürzen, CRC, Sender-Kopf, Antwort-Parser, JSON-Escape; ACK mit Ziel + Zweitkanal), UI-Test 198/198
(neu: Kennung im Text, ACK an fremdes Gateway ignoriert, Zabbix-Kanal ohne Kanal abgelehnt, Kopie mit Kennung, ACK
im Zabbix-Kanal; Empfänger: ohne Server abgelehnt, Zabbix offline → wartet ohne ACK, nach Korrektur übergeben + ACK,
Ablehnung ohne ACK, ohne Kennung ignoriert, Kanalfilter, Testwert, Backup). Template: YAML gültig, JS-Vorverarbeitung
und Trigger-Regexe gegen Beispielmeldungen geprüft. **Nicht getestet:** echter Zabbix-Import, echter Trapper-Versand
(Mock simuliert Zabbix), Funkstrecke Gateway → Empfänger.

**Nachtrag E41 (2026-10-16, User: „einbauen dass der Node auch seine eigenen Meldungen sieht“):** Ein Knoten hört
seine eigenen Funknachrichten nicht. `GatewayMesh::sendChannelText` reicht daher jede gesendete Kanalnachricht
zusätzlich an `zbx_fwd::onChannelText` – ein Gerät, das selbst die Zabbix-Weiterleitung macht, meldet so auch die
eigenen Alarme (Host = eigener Name). Das ACK für eigene Meldungen geht nicht über Funk („@[ich] ACK n“ käme nie
an), sondern lokal an den Poller (`sm_poller::onChannelText("Zabbix: @[ich] ACK n")`); die Rückmeldung
„ACK n bestätigt (Zabbix)“ geht wie gewohnt in den Kanal. UI-Test 203/203.

**PSRAM-Anzeige (2026-10-16, User):** Status → Gateway zeigt freien Heap mit Tiefststand seit Start
(`ESP.getMinFreeHeap`) und PSRAM belegt/gesamt mit kleinstem freien Wert (`getPsramSize/getFreePsram/
getMinFreePsram`), API `/api/status` Felder `min_free_heap`, `psram_total`, `psram_free`, `psram_min_free`. Für den
Dauerlauf: Tiefststände zeigen Lecks/Spitzen, ohne ständig hinzusehen. Schätzung 0.8.0 ca. 277 KB, 0.9.0 ca. 290 KB
fest belegt (Live-Logs ~199 KB, CLI-Aufträge 56 KB, Ereignis-Log 8 KB, Überwachung ~14 KB, Zabbix ~12 KB, ACK ~2 KB).

## E42 – Fernkonsole und Überwachung entfernter Repeater (2026-10-16, User: „remote Konsole … einen anderen repeater per cli zu verwalten“, „stündlich die abfrage nach akku und temp? bei über oder unterschreiten -> meldung in alarmkanal“, „du kannst das schon bauen“), 0.10.0

- **Weg wie die MeshCore-App**, keine eigene Protokollerweiterung: Anmeldung per `ANON_REQ` mit Passwort, danach
  `REQ` (Status `0x01`, Telemetrie `0x03`) bzw. `TXT_MSG` mit `TXT_TYPE_CLI_DATA` für CLI-Befehle. Der Ziel-Repeater
  braucht keine Änderung (jede MeshCore-Repeater-Firmware mit Anmeldung). Formate aus repeater-v1.18.0 übernommen
  und in `src/RemoteFormat.*` gekapselt (Host-Test).
- **Ohne Änderung an MeshCore**: `GatewayMesh` überschreibt `searchPeersByHash`, `getPeerSharedSecret`,
  `onPeerDataRecv`, `onPeerPathRecv`, `onAdvertRecv`. Sitzungs-Ziele stehen bei der Peer-Suche **vor** den eigenen
  Clients (ACL), höhere Indizes gehen um die Trefferzahl verschoben an `MyMesh`. Ist derselbe Knoten zugleich
  Client des Gateways (gleicher ECDH-Schlüssel), bekommt `MyMesh` alles, was nicht zu einem laufenden Auftrag
  passt (z. B. dessen CLI-Befehle an uns) – erkannt über den gleichen Schlüssel (`baseIndexFor`).
- **Aufträge statt direkter Aufrufe** (`src/RemoteAdmin.*`): Web und Poller legen Aufträge ab, der loopTask
  arbeitet sie **nacheinander** ab (immer nur einer im Funk, schont Airtime und hält Antworten eindeutig zuordenbar;
  Status/Telemetrie über den gespiegelten Zeitstempel). Wartezeit wie die App: Flood 16 × Airtime, direkt
  (6 × Airtime + 250 ms) je Hop, hin und zurück, 6–45 s. Pfad lernt das Gateway aus der ersten (Flood-)Antwort,
  danach direkt; bleibt eine Antwort aus, wieder Flood.
- **Fernkonsole** im Konsolen-Tab: Ziel aus empfangenen Adverts (Repeater + Room-Server, 32 im RAM) oder Schlüssel
  von Hand, Admin-Passwort nur für die Anmeldung (danach steht das Gateway in der ACL des Ziels, das Passwort wird
  nicht gehalten). Ein Befehl gleichzeitig, kein automatisches Wiederholen (ein Repeater beantwortet einen Befehl
  mit gleichem Zeitstempel absichtlich leer, und `reboot`/`set` sollen nicht doppelt laufen). Gefährliche Befehle
  fragen mit Zielnamen nach. Geheimnis-Befehle (`get prv.key` …) wie lokal maskiert, Antwort nur in der Ansicht,
  nie im Log. 15 min ohne Befehl → beendet. Gast-Anmeldung zeigt, dass CLI Admin braucht (MeshCore beantwortet
  CLI nur für Admins).
- **Überwachung** als Gerätetyp „Repeater (per Funk)“ in der bestehenden Überwachung, damit Regeln, Kanäle,
  Sendelimit, ACK-Pflicht (E39) und Zabbix-Kennung (E41) ohne Sonderweg gelten. Messwerte `repeater.battery` (V,
  aus Status, sonst Telemetrie), `repeater.temperature` (Telemetrie Kanal 1), `repeater.uptimeH`,
  `repeater.noiseFloor`. **Gast-Passwort genügt** (Status und Basis-Telemetrie sind für Gäste freigegeben). Abstand
  900–43200 s, Standard 3600; nach Fehlversuch neuer Versuch nach 10 min, „nicht erreichbar“ nach N Fehlversuchen
  wie bei Sensormetern. Neustart erkennen = Regel „Laufzeit < 1 h“ (kein eigenes Ereignis nötig).
- Ablauf je Abfrage (`remote_fmt::PollFlow`): angemeldet → Status → Telemetrie; bleibt der Status aus, einmal neu
  anmelden (Gäste stehen beim Repeater nur im RAM, nach dessen Neustart ist die Anmeldung weg). Anmeldung ohne
  Antwort = Passwort falsch oder nicht erreichbar (ein Repeater antwortet auf falsche Passwörter gar nicht).
  Telemetrie fehlt → Ergebnis ohne Temperatur.
- Konfigurationsänderungen fragen entfernte Repeater **nicht** neu ab (Funkzeit); neue/geänderte Regeln werden
  sofort mit den zuletzt gemessenen Werten ausgewertet.
- Flood ohne Region-Scope (wie die App ohne Standard-Scope). Wo Nachbarn Floods ohne Scope ablehnen, kommt die
  erste Anfrage nicht an – dann bleibt nur der direkte Weg, sobald ein Pfad bekannt ist.

**Verifikation:** Build V4 + R8 grün, 0 Warnungen in `src/`, Host-Tests grün (neu: Fernabfrage-Format:
RepeaterStats-Versätze, CayenneLPP mit Fremdkanal/Vorzeichen/Abbruch, Login Admin/Gast, Schlüssel Hex, Wartezeit,
Ablauf mit Neuanmeldung), UI-Test 239/239 (neu 36: Fernkonsole Gast/Admin/falsches Passwort/Schlüssel von Hand/
Geheimnis/keine Antwort/Rückfragen mit Ziel; Repeater-Gerät mit Auswahl aus Adverts, Werte, Regel → Alarm, nicht
erreichbar, feste Messwerte ohne Abfrage, Abstand-Untergrenze, doppelt/ungültig abgelehnt, Bearbeiten).
**Nicht getestet:** echte Funkstrecke (Mock simuliert den Repeater) – HW-Testplan §13.

## E43 – Kanal-API mit API-Schlüssel (2026-10-16, User: „Api über die eine abfrage aller auf dem Gerät angelegten Kanäle möglich ist, zugriff über einen Api key der auf dem gerät generiert wird … home-central kanäle quasi aboniert und alle nachrichten in einem kanal dann per telegram weiterleiten kann. Das muss noch in die 0.10.0“), 0.10.0

- **Nur lesend**, eigener Zugang statt Admin-Login: home-central braucht weder das Admin-Passwort (das alles darf)
  noch Schreibrechte. `GET /api/v1/channels` (ohne Kanalschlüssel), `GET /api/v1/messages?since=N[&channel=Name]`.
  Versioniert unter `/api/v1/`, damit die interne Web-API sich weiter ändern darf.
- **Schlüssel erzeugt das Gerät** (`esp_fill_random`, 128 Bit, 32 Hex), NVS `gw/api_key`; leer = API aus (Standard).
  Neu erzeugen macht den alten sofort ungültig, „Ausschalten“ löscht ihn. Übergabe per Header `X-API-Key` oder
  `Authorization: Bearer`, **nicht** als URL-Parameter (landet sonst in Proxy-/Server-Logs). Vergleich ohne frühen
  Abbruch. Nicht im Backup (nach Wiederherstellung neu erzeugen und in home-central eintragen).
- **Abonnieren per Abfrage mit Zeiger** statt Push/Websocket: home-central fragt z. B. alle 10–30 s mit dem `next`
  der letzten Antwort. Grundlage ist der vorhandene Ringpuffer `channel_log` (200 Zeilen, RAM): empfangene **und**
  gesendete Nachrichten (`dir` rx/tx), zerlegt in Kanal, Absender, Text, SNR, Hops (`src/ChannelLine.*`, Host-Test).
  `gap: true` = Zeilen verpasst (zu lange nicht gefragt). `boot` (Zufallswert je Start) ändert sich bei jedem
  Neustart des Gateways – dann beginnt die Zählung neu, der Client setzt `since` auf 0. Kein eigener Speicher, keine
  Zustellgarantie über Neustarts (der Puffer liegt nur im RAM).
- `time` ist der Zeitstempel der Nachricht vom **Absender** (MeshCore), bei gesendeten der eigene.
- Nur HTTP wie die übrige Weboberfläche: Schlüssel und Inhalte gehen unverschlüsselt durchs LAN. Für ein
  vertrauenswürdiges Heimnetz akzeptiert, im Handbuch genannt.

**Verifikation:** Build V4 + R8 grün, Host-Test Kanal-API (Logzeile zerlegen: Absender bis zum ersten „: “, gesendet
ohne SNR, ohne Absender, ungültige Zeilen), UI-Test 254/254 (neu 15: aus → 403, erzeugen/anzeigen/verbergen, ohne/
falscher Schlüssel → 401, Kanäle ohne Geheimnisse, Bearer, Nachrichten rx/tx, Zeiger ohne Doppelte, Kanalfilter,
neu erzeugt → alter ungültig, Schlüssel nicht im Ereignis-Log, ausgeschaltet → 403). **Nicht getestet:** auf echter
Hardware, Gegenstelle home-central (noch nicht gebaut).

## E44 – Senden über die Kanal-API (2026-10-17, User: „home-central soll warnungen (katwarn / nina etc.) auch in den mesh puschen, baue dies in beiden seiten ein“), 0.10.0

- `POST /api/v1/send` (Formular: `channel`, `text`) mit demselben API-Schlüssel wie E43. Senden ist **zusätzlich**
  freizugeben: System → Kanal-API → „Senden erlauben in“ **genau einem** Kanal (NVS `gw/api_send_ch`, leer = nur
  lesen). Ein abgegriffener Schlüssel kann so höchstens in diesen einen Kanal schreiben, nie in private Kanäle
  oder den Alarmkanal, wenn der nicht freigegeben ist.
- **10 Nachrichten pro Stunde** (eigener `rules::RateLimiter`, getrennt vom Sendelimit der Überwachung), danach
  HTTP 429. Grund: Funkzeit; Warnungen kommen selten, ein fehlerhafter Client soll den Kanal nicht fluten.
  Erst prüfen (`usedInWindow`), gezählt (`allow`) wird erst nach erfolgreicher Übergabe an die Warteschlange –
  ein 503 (Warteschlange voll) verbraucht kein Kontingent (Nachtrag 2026-10-17, User).
- Text: Zeilenumbrüche/Tabs → Leerzeichen, **UTF-8-sicher gekürzt** auf den Platz nach „Gerätename: “
  (`chan_line::utf8Cut`, Host-Test). Die Antwort enthält den tatsächlich gesendeten Text und `truncated` – home-central
  nutzt ihn, um die eigenen Nachrichten beim Weiterleiten Mesh → Telegram herauszufiltern.
- Ereignis-Log `API  Senden in <Kanal> von <IP>` (ohne Text; der steht ohnehin als gesendete Kanalnachricht im Log).
- Gegenseite home-central 0.3.0: DWD, NINA (MoWaS, KATWARN, BIWAPP, Polizei, Hochwasser) und Windböen, je Art
  abschaltbar, jede Warnung einmal (gleicher Mechanismus wie Telegram/ntfy).

**Verifikation:** Build V4 grün, Host-Test utf8Cut (2-, 3-, 4-Byte-Zeichen), UI-Test 266/266 (neu 12: ohne Freigabe/
anderer Kanal → 403, falscher Schlüssel → 401, Freigabe in der Oberfläche, gesendet als tx im Kanal, Kürzung UTF-8-
sicher, leer → 400, 10/h dann 429, Ereignis-Log, Freigabe zurücknehmen). home-central gegen den Mock: Warnung
einmal im Mesh, Zeilen zusammengezogen, eigene Warnung nicht zurück nach Telegram, abgewählte Art nicht gesendet.
**Nicht getestet:** auf echter Hardware.
