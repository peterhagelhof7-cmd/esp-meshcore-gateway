# MeshCore-Gateway für Heltec WiFi LoRa 32 V4

MeshCore-Repeater mit WLAN: Weboberfläche für alle MeshCore-Einstellungen, Webkonsole für die
offizielle CLI, Kanäle senden und empfangen, Grenzwert-Meldungen von Sensormetern und Ping-Zielen
in MeshCore-Kanäle, Firmware-Update per Web mit Board-Prüfung und Rollback.

Aktuelle bewährte Version: **0.10.0** (auf Basis von MeshCore **repeater-v1.18.0**),
für Heltec WiFi LoRa 32 V4 und V4-R8. Im Web-Flasher sind auch ältere Versionen wählbar.

## Funktionen

**Repeater**
- Vollwertiger MeshCore-Repeater (offizielle Firmware als Grundlage, unverändertes Funkverhalten)
- Alle Repeater-Einstellungen in der Weboberfläche, Profilprüfung „wie empfohlen“ mit Hinweisen
- Regionen anlegen und prüfen, Nachbarn-Tabelle mit SNR
- Webkonsole für die offizielle MeshCore-CLI, Paket- und Ereignis-Log

**Kanäle**
- Gruppenkanäle (öffentlich, Hashtag oder eigener Schlüssel) anlegen, Nachrichten lesen und senden
- Senden mit Region (Scope), Anzeige von SNR und Hops

**Überwachung und Alarme**
- Sensormeter im LAN abfragen, Regeln mit Grenzwerten, Meldung in einen MeshCore-Kanal
- Ping-Check für beliebige Geräte im Netz (erreichbar / nicht erreichbar)
- Chiptemperatur des Gateways als eigener Messwert
- Sendelimit, Texte mit Platzhaltern, Meldung auch bei Rückkehr in den Normalbereich

**Betrieb**
- Firmware-Update per Weboberfläche mit Board- und Versionsprüfung, automatischer Rollback bei Absturz
- Web-Flasher im Browser (Chrome/Edge) mit Versionsauswahl und Erkennung V4 / V4-R8
- WLAN-Einrichtung über Hotspot, Hostname folgt dem Gerätenamen, NTP-Zeit
- Statusseiten auf dem OLED-Display, Sicherung und Wiederherstellung der Einstellungen

## Neu in 0.9.0

- **Alarme „aktiv bis bestätigt“:** Wichtige Meldungen bekommen eine Nummer (`[ACK 3] …`) und werden
  nach 5, 15 und 30 Minuten wiederholt, bis jemand im Kanal mit `ACK 3` antwortet oder in der
  Weboberfläche bestätigt. Die Bestätigung wird im Kanal mit Namen quittiert.
- **Feste IP-Adresse:** IP, Maske, Router und DNS frei einstellbar. Vor dem Umschalten prüft das Gateway
  per ARP und Ping, ob die Adresse schon belegt ist, und fällt bei Problemen sicher auf DHCP zurück.
- **Zabbix-Anbindung:** Alarme tragen eine maschinenlesbare Kennung. Ein zweites Gateway mit derselben
  Firmware empfängt sie per Funk und leitet sie an Zabbix weiter (Trapper). Zabbix-Probleme öffnen und
  schließen sich automatisch, die Annahme wird per ACK im Kanal bestätigt. Vorlage für Zabbix 7 liegt bei.
- **Speicheranzeige:** freier Heap mit Tiefststand seit dem Start und PSRAM-Belegung im Status.

## Neu in 0.10.0

- **Fernkonsole:** andere Repeater im Mesh per CLI verwalten, direkt im Konsolen-Tab. Ziel aus der Nachbarsuche
  und empfangenen Adverts wählen, mit dem Admin-Passwort anmelden, Befehle gehen per Funk.
- **Entfernte Repeater überwachen:** Akku, Temperatur, Laufzeit und Rauschen anderer Repeater regelmäßig per Funk
  abfragen (Gast-Passwort genügt). Grenzwerte, Neustart-Erkennung und „nicht erreichbar“ melden in den Alarmkanal.
- **Kanal-API:** Kanäle und Nachrichten für andere Systeme im LAN abrufbar, geschützt mit einem API-Schlüssel, der
  auf dem Gerät erzeugt wird – z. B. zum Weiterleiten eines Kanals nach Telegram. Optional Senden in einen
  freigegebenen Kanal (max. 10 pro Stunde), etwa für Unwetter- und Katastrophenwarnungen (DWD, NINA/KATWARN).

## Neu in 0.10.1 (in Erprobung)

- **Heltec V4.3 (FEM KCT8103L) und V4.2 (GC1109):** Die Empfehlung wertet `radio.fem.rxgain` auf V4.2 als
  „nicht vorhanden“ statt als Abweichung. Das wirkungslose Feld „FEM-Sendeverstärker“ ist entfernt. Hinweis zum
  FEM-Empfangsverstärker nach Messung auf V4.3: Er hebt Signal und Rauschen gleich (kein SNR-Gewinn), daher aus.

## Bekannte Probleme

Betrifft 0.10.0, behoben in 0.10.1.

- **Empfehlung meldet auf V4.2 dauerhaft eine Abweichung bei `radio.fem.rxgain`.** Den FEM-Empfangsverstärker
  gibt es nur auf Boards mit KCT8103L (V4.3). Auf V4.2 (GC1109) antwortet MeshCore mit `Error: unsupported`,
  die Empfehlung zeigt das als Abweichung, „Abweichungen übernehmen“ schlägt für diesen Eintrag fehl.
  Funk und alle anderen Einstellungen sind nicht betroffen, die Meldung kann ignoriert werden.
  Welche Revision vorliegt, zeigt der Konsolenbefehl `board` („Heltec V4.3 OLED“ bzw. „Heltec V4 OLED“).
- **Feld „FEM-Sendeverstärker“ (`radio.fem.txgain`) ohne Wirkung.** MeshCore repeater-v1.18.0 speichert den
  Wert, die Board-Unterstützung für V4/V4.3 wertet ihn aber nicht aus – auf keiner Revision. Der Hinweis
  „nur bei Board-Revision mit KCT8103L“ in den Einstellungen ist irreführend.

## Installieren

**Admin-Handbuch** (Installation, Erstinbetriebnahme, **Regionen**, Betrieb, Updates): [`docs/admin-guide.md`](docs/admin-guide.md)

**Web-Flasher:** https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/

Chrome oder Edge am Computer, Board per USB anschließen. Der Flasher prüft vor dem Schreiben, welches
Board angeschlossen ist (ESP32-S3 mit 16 MB Flash; 2 MB PSRAM = V4, 8 MB PSRAM = V4-R8), und spielt nur
die dazu passende Firmware auf. Danach öffnet das Gateway den WLAN-Hotspot `installer` (Passwort
`installer`), Einrichtung unter http://192.168.4.1.

Updates eines laufenden Gateways: Weboberfläche → System → Firmware-Update (behält alle Einstellungen).

## Bauen

PlatformIO, Envs `gateway_heltec_v4` und `gateway_heltec_v4_r8`. MeshCore liegt als Submodul in
`external/MeshCore` (`git clone --recursive`).

- Host-Tests: `tools\host-test.ps1 -Build`
- Web-UI-Test gegen Mock: `tools/webui-test` (`README.md` dort)
- Flasher-Dateien aktualisieren: `tools\flasher-update.ps1 -Build` (neue Version als „in Erprobung“, ältere bleiben
  wählbar), nach Bewährung `-Promote <Version>`, alte mit `-Remove <Version>`; danach `docs/fw/` committen
  und die Download-Links in `docs/admin-guide.md` Abschnitt 7 anpassen

Kanal-API für andere Systeme (ab 0.10.0): [`docs/api.md`](docs/api.md).

Planung und Entscheidungen: [`docs/planung.md`](docs/planung.md), [`docs/entscheidungen.md`](docs/entscheidungen.md).

## Lizenz

GPL-3.0 (`LICENSE`). MeshCore-Anteile stehen unter MIT, siehe `NOTICE`.
