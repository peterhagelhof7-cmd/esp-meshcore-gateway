# Hardware-Testplan (wenn das Heltec V4 da ist)

Stand 0.7.1 (2026-10-14). Erster HW-Test am Standard-V4 (COM4, 10.10.0.151) begonnen: [x] = am Gerät
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
- [ ] Hostname automatisch: nach OTA 0.7.4 heißt das Gateway im WLAN `de-xx-ort02-gw` (Fritzbox, Status-Tab); `set name` ändern → Hostname zieht ohne Neustart nach (Ereignis-Log)
- [ ] Nachbarn: nach OTA 0.7.5 steht nach ~2,5 min A1B2C3D4 im Status-Tab (Ereignis-Log „Nachbarsuche nach dem Start gesendet“); „Jetzt suchen“ → Countdown, Liste aktualisiert

## 7. Sensormeter (M6)
- [ ] Privaten Kanal „sensormeter“ anlegen (Scope rhein-main ist vorbelegt), als Standardkanal setzen
- [x] Gerät 10.10.0.148 eintragen → Status ✓, Temperatur und Feuchte sichtbar (2026-10-14: ERSTER-SM .148 + ERSTER-PoE .108, je 60 s, Takt am Gerät bestätigt)
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
