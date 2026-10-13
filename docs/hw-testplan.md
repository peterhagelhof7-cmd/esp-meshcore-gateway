# Hardware-Testplan (wenn das Heltec V4 da ist)

Stand 0.5.0 (2026-10-14). Alles bis M6 ist gebaut, host- und mock-getestet. Auf echter Hardware ist
noch nichts geprüft. Reihenfolge so wählen, dass ein Fehler früh auffällt und nichts kaputt geht.

## 0. Vorbereitung (nur lesen, nichts schreiben)
- [ ] Platine per USB-C anschließen, COM-Port notieren
- [ ] Variante auslesen: `esptool --port COMx flash_id` + Boot-Log → **2 MB PSRAM = V4**, **8 MB = V4-R8**
      → passendes Env wählen (`gateway_heltec_v4` bzw. `gateway_heltec_v4_r8`)
- [ ] Falls schon Firmware drauf ist: **Flash-Abbild sichern** (`esptool read_flash 0 0x1000000 backup.bin`)
- [ ] Antenne anschließen, **bevor** gesendet wird (V4 mit PA)

## 1. Erstes Flashen (nur nach OK)
- [ ] `pio run -e <env> -t upload` (volles USB-Flash, Partitionstabelle default_16MB)
- [ ] Seriell 115200: `MeshCore Gateway 0.5.0 … CPU 240 MHz`, `[OTA] MCGW-FW-ID:…`
- [ ] Erststart: Konsole zeigt „Erststart: wende Profil an“ + 23 Profil-Befehle mit OK
- [ ] OLED: Startbild, dann „Einrichtung: WLAN installer / installer http://192.168.4.1“

## 2. WLAN / Installer (M1)
- [ ] Mit AP `installer` verbinden → http://192.168.4.1 → Login `admin` / `password`
- [ ] Netze suchen, Heim-WLAN speichern → Neustart → Station, IP im OLED und auf der Statusseite
- [ ] Falsches Passwort testen → nach 30 s zurück im Installer-AP
- [ ] Router aus/an → Reconnect ohne Neustart

## 3. Konsole + Pakete (M2)
- [ ] `ver`, `board`, `get radio` → **869.618,62.5,8,8**
- [ ] `stats-radio` → **Rauschpegel um −110 dBm** (sonst prüfen: rxgain/fem.rxgain aus?)
- [ ] Paket-Log zeigt Adverts aus der Umgebung

## 4. Einstellungen (M3)
- [ ] Empfehlungs-Tabelle: „Alles wie empfohlen“
- [ ] Name setzen (Standort + Kontakt), Admin-Passwort ändern → Neu-Login klappt
- [ ] Backup herunterladen (mit Schlüssel) und sicher ablegen
- [ ] Uhrzeit: Konsole `clock` = aktuelle UTC-Zeit (NTP)
- [ ] **ERP prüfen:** Antenne + Kabel → tx 17 noch ≤ 27 dBm ERP? Sonst `set tx 16`

## 5. Kanäle (M4) – mit offizieller App auf dem Handy
- [ ] Im Gateway `#test` anlegen, in der App ebenfalls `#test`
- [ ] App → `#test` schreiben → erscheint im Gateway (Kanäle-Tab) mit SNR/Hops
- [ ] Gateway → `#test` schreiben → erscheint in der App als „<Gateway-Name>: …“
- [ ] Privater Kanal: Schlüssel aus dem Gateway (Base64) in der App importieren → beidseitig lesbar

## 6. OTA + Rollback (M5)
- [ ] System-Tab: gleiche `firmware.bin` hochladen → Neustart → Partition wechselt, Status „Prüflauf“,
      nach 60 s „gültig“
- [ ] Andere Board-Datei (R8 auf V4) → abgelehnt; Textdatei → abgelehnt
- [ ] Rollback-Test (optional): Testbuild, der nach 10 s absichtlich abstürzt → Gerät kommt mit alter Version zurück
- [ ] OLED-Taste: kurz und lang → nur Seitenwechsel, **kein** Abschalten

## 7. Sensormeter (M6)
- [ ] Privaten Kanal „sensormeter“ anlegen, als Standardkanal setzen
- [ ] Gerät 192.168.178.148 eintragen → Status ✓, Temperatur und Feuchte sichtbar
- [ ] Regel Temperatur > (aktueller Wert − 1) → ALARM kommt in der App an
- [ ] Grenzwert hochsetzen → Entwarnung (Hysterese beachten)
- [ ] Sensormeter vom Strom → nach 3 Intervallen OFFLINE-Meldung, danach ONLINE
- [ ] Gateway-WLAN trennen → **keine** OFFLINE-Flut
- [ ] Mehr als 6 Meldungen/h provozieren → Rest „unterdrückt“ im Meldungs-Log

## 8. Dauerlauf
- [ ] 24 h laufen lassen: freier Heap stabil (Status-Tab), keine Neustarts (Laufzeit), WLAN stabil
