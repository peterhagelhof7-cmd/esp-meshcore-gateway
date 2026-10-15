# Hardware-Testplan (wenn das Heltec V4 da ist)

Stand 0.7.1 (2026-10-14). Erster HW-Test am Standard-V4 (COM4, 192.168.178.151) begonnen: [x] = am Gerät
bestätigt, [-] = entfallen. Befunde und Fixes: `docs/entscheidungen.md` E29.

## 0. Vorbereitung (nur lesen, nichts schreiben)
- [x] Platine per USB-C anschließen, COM-Port notieren
- [x] Variante auslesen: `esptool --port COMx flash_id` + Boot-Log → **2 MB PSRAM = V4**, **8 MB = V4-R8**
      → passendes Env wählen (`gateway_heltec_v4` bzw. `gateway_heltec_v4_r8`)
- [-] Falls schon Firmware drauf ist: **Flash-Abbild sichern** (`esptool read_flash 0 0x1000000 backup.bin`) — entfallen 2026-10-14: nur Werks-FW drauf, `read_flash` über USB-JTAG brach ab, User-OK ohne Backup
- [ ] Antenne anschließen, **bevor** gesendet wird (V4 mit PA)

## 1. Erstes Flashen (nur nach OK)
- [x] `pio run -e <env> -t upload` (volles USB-Flash, Partitionstabelle default_16MB)
- [x] Seriell 115200: `MeshCore Gateway 0.5.0 … CPU 240 MHz`, `[OTA] MCGW-FW-ID:…`
- [x] Erststart: Konsole zeigt „Erststart: wende Profil an“ + 23 Profil-Befehle mit OK
- [ ] OLED: Startbild, dann „Einrichtung: WLAN installer / installer http://192.168.4.1“

## 2. WLAN / Installer (M1)
- [ ] Mit AP `installer` verbinden → http://192.168.4.1 → Login `admin` / `password`
- [x] Netze suchen, Heim-WLAN speichern → Neustart → Station, IP im OLED und auf der Statusseite
- [ ] Falsches Passwort testen → nach 30 s zurück im Installer-AP
- [ ] Router aus/an → Reconnect ohne Neustart

## 3. Konsole + Pakete (M2)
- [x] `ver`, `board`, `get radio` → **869.618,62.5,8,8**
- [x] `stats-radio` → **Rauschpegel um −110 dBm** (sonst prüfen: rxgain/fem.rxgain aus?)
- [x] Paket-Log zeigt Adverts aus der Umgebung

## 4. Einstellungen (M3)
- [x] Empfehlungs-Tabelle: „Alles wie empfohlen“
- [ ] Name setzen (Standort + Kontakt), Admin-Passwort ändern → Neu-Login klappt
- [ ] Backup herunterladen (mit Schlüssel) und sicher ablegen
- [x] Uhrzeit: Konsole `clock` = aktuelle UTC-Zeit (NTP)
- [ ] **ERP prüfen:** Antenne + Kabel → tx 17 noch ≤ 27 dBm ERP? Sonst `set tx 16`

## 5. Kanäle (M4) – mit offizieller App auf dem Handy
- [ ] Im Gateway `#test` anlegen, in der App ebenfalls `#test`
- [ ] App → `#test` schreiben → erscheint im Gateway (Kanäle-Tab) mit SNR/Hops
- [ ] Gateway → `#test` schreiben → erscheint in der App als „<Gateway-Name>: …“
- [ ] Privater Kanal: Schlüssel aus dem Gateway (Base64) in der App importieren → beidseitig lesbar

## 6. OTA + Rollback (M5)
- [x] System-Tab: gleiche `firmware.bin` hochladen → Neustart → Partition wechselt, Status „Prüflauf“,
      nach 60 s „gültig“
- [x] Andere Board-Datei (R8 auf V4) → abgelehnt; Textdatei → abgelehnt
- [x] Rollback-Test (optional): Testbuild, der nach 10 s absichtlich abstürzt → Gerät kommt mit alter Version zurück
- [ ] OLED-Taste: kurz und lang → nur Seitenwechsel, **kein** Abschalten
- [ ] OLED Seite 4/4 „Nachricht“: Nachricht in `#test` aus der App → Kanal, Uhrzeit, Text mit Umlauten; Alarm-Test → „gesendet“
- [ ] Hostname automatisch: nach OTA 0.7.4 heißt das Gateway im WLAN `de-by-sul02-sps-gw` (Fritzbox, Status-Tab); `set name` ändern → Hostname zieht ohne Neustart nach (Ereignis-Log)
- [x] Nachbarn: nach OTA 0.7.5 steht nach ~2,5 min B0A23599 im Status-Tab (Ereignis-Log „Nachbarsuche nach dem Start gesendet“); „Jetzt suchen“ → Countdown, Liste aktualisiert (2026-10-14: Suche 87 s nach Start, B0A23599 +11,25 dB in der Tabelle; „Jetzt suchen“ nur am PC getestet)

## 7. Sensormeter (M6)
- [ ] Privaten Kanal „sensormeter“ anlegen (Scope rhein-main ist vorbelegt), als Standardkanal setzen
- [x] Gerät 192.168.178.148 eintragen → Status ✓, Temperatur und Feuchte sichtbar (2026-10-14: ERSTER-SM .148 + ERSTER-PoE .108, je 60 s, Takt am Gerät bestätigt)
- [ ] Regel Temperatur > (aktueller Wert − 1) → ALARM kommt in der App an
- [ ] Grenzwert hochsetzen → Entwarnung (Hysterese beachten)
- [ ] Sensormeter vom Strom → nach 3 Intervallen OFFLINE-Meldung, danach ONLINE
- [ ] Gateway-WLAN trennen → **keine** OFFLINE-Flut
- [ ] Mehr als 6 Meldungen/h provozieren → Rest „unterdrückt“ im Meldungs-Log

## 8. Ping-Check (M7)
- [ ] Router/NAS als Ping-Ziel (Schwelle 60 s, Prüfabstand 30 s) → Status ✓ mit ms, Tabelle „60 s / alle 30 s“
- [ ] Ziel `192.0.2.1`, Schwelle 30 s → nach ca. 30–40 s PING-ALARM im Alarmkanal (App)
- [ ] Ziel per Hostname (DNS) → IP wird angezeigt
- [ ] Gerät kurz vom Netz (unter Schwelle) → keine Meldung; länger → Alarm, danach PING-OK

## 8a. Backup/Restore + Ereignis-Log (M8)
- [ ] Backup **mit** Haken → Datei enthält prv.key, adminPassword, gw.wifi.pass, guest.password; **ohne** Haken nichts davon
- [ ] Mit der App einen Admin-Client anmelden → `get acl` in der Webkonsole zeigt ihn, Backup enthält `setperm …`
- [ ] Backup auf **zweitem/gelöschtem** Gerät (nach `erase`) einspielen inkl. Admin-Passwort + WLAN → gleiche Identität (Public Key), gleiche Einstellungen, Kanäle, Überwachung, Login mit altem Passwort
- [x] Ereignis-Log: Neustart über Web → nach dem Start enthält das Log „=== Start (Software-Neustart)“ und die Zeilen davor (Shutdown-Flush)
- [ ] Strom ziehen → nach Start „=== Start (Einschalten)“; Verlust höchstens der letzten 15 min
- [ ] Log läuft über 64 KB (z. B. viele `ver` in der Webkonsole) → `/gwlog.1.txt` entsteht, Gesamtgröße bleibt ≤ 128 KB
- [ ] WLAN-Router aus/an → NET-Zeilen „WLAN-Verbindung verloren“ / „WLAN verbunden“

## 9. Dauerlauf
- [ ] 24 h laufen lassen: freier Heap stabil (Status-Tab), keine Neustarts (Laufzeit), WLAN stabil

## Web-Flasher (E35)

- [x] https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/ in Chrome: „Verbinden & prüfen“ am Standard-V4 → „Erkannt: Heltec WiFi LoRa 32 V4 (ESP32-S3R2, 2 MB PSRAM)“, Ausstattung zeigt „Embedded PSRAM 2MB“ (nur prüfen reicht, danach „Neu prüfen“/Tab schließen; das Gateway startet dabei neu) (2026-10-14 User: erkannt V4 2 MB; Gateway danach normal neu gestartet, 0.7.5 gültig)
- [ ] Optional: Installieren ohne Löschen → 0.7.5 bleibt mit allen Einstellungen

## 10. Aktiv bis bestätigt (E39, 0.9.0)
- [ ] Regel mit Haken „aktiv bis bestätigt“, die sofort auslöst (z. B. Chiptemperatur > 30 °C, Kanal sps-alarm) → App zeigt `[ACK n] ALARM …`
- [ ] Nach 5 min `[ACK n] WDH 1/3: …` in der App, Überwachung → Offene Bestätigungen „1 von 3“
- [ ] In der App im Kanal `ACK n` antworten → Gateway antwortet `ACK n bestätigt (App-Name)`, keine weitere Wiederholung
- [ ] Antwort über „Antworten“ in der App (`@[Gateway-Name] ack`) bestätigt ebenfalls
- [ ] Neuer Alarm, Bestätigen über die Weboberfläche → `ACK n bestätigt (Web)` kommt in der App an
- [ ] Ping-Ziel mit Haken auf nicht erreichbare Adresse → `[ACK n] PING-ALARM …`, Wiederholung, ACK
- [ ] Testregel danach wieder löschen

## 11. Feste IP (E40, 0.9.0)
- [ ] System → WLAN: Haken „feste IP“ → Felder mit aktuellen Werten (IP, 255.255.255.0, Router 192.168.178.1)
- [ ] „Adresse prüfen“ mit IP eines vorhandenen Geräts (z. B. Sensormeter .148) → „BELEGT“ mit MAC; Speichern abgelehnt
- [ ] „Adresse prüfen“ mit Windows-PC-IP (Ping meist geblockt) → trotzdem „BELEGT“ (ARP)
- [ ] Freie Adresse speichern → Neustart, Ereignis-Log: „prüfe, ob die Adresse frei ist“ → „Feste IP … aktiv, Router … erreichbar“; Weboberfläche unter neuer IP, Status „Adressvergabe: fest“
- [ ] Danach Neustart ohne Prüfung (feste IP sofort), NTP/Sensormeter-Abfrage/Ping-Check laufen
- [ ] Falschen Router eintragen (z. B. .253, im Subnetz, nicht vorhanden) → nach Neustart zurück auf DHCP, Grund im WLAN-Tab und Log
- [ ] Haken weg → wieder DHCP

## 12. Zabbix-Anbindung (E41, 0.9.0) – braucht zweites Gerät + Zabbix
- [ ] Template `docs/zabbix-template-meshcore.yaml` importieren (Fehler beim Import notieren), Host `<gateway-hostname>` mit „MeshCore Gateway Alarme“, Empfänger-Host mit „MeshCore Zabbix-Empfaenger“, beide am richtigen Proxy
- [ ] Empfänger: Zabbix-Weiterleitung an, Proxy eintragen, „Testwert senden“ → OK, in Zabbix `meshcore.rx.heartbeat` Latest data
- [ ] Gateway: Kennung „im Alarmtext“, Regel mit „aktiv bis bestätigt“ auslösen → App zeigt Text mit `#Z1;…`, Empfänger-Status „An Zabbix übergeben 1“, Zabbix-Problem mit Alarmtext, App zeigt `@[Gateway] ACK n` vom Empfänger, Gateway `ACK n bestätigt (Empfänger)`, keine Wiederholung
- [ ] Entwarnung → Zabbix-Problem geschlossen
- [ ] Zabbix-Proxy stoppen → Alarm: kein ACK, Gateway wiederholt nach 5 min; Proxy starten → Empfänger liefert nach (≤ 30 s), ACK
- [ ] Host in Zabbix umbenennen → Empfänger zeigt „abgelehnt“, kein ACK
- [ ] Modus „eigener Zabbix-Kanal“: Alarmkanal ohne Kennung, Zabbix-Kanal mit; Empfänger-Kanalfilter auf Zabbix-Kanal
