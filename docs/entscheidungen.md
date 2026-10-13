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
