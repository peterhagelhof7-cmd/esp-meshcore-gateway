# MeshCore-Gateway für Heltec WiFi LoRa 32 V4

MeshCore-Repeater mit WLAN: Weboberfläche für alle MeshCore-Einstellungen, Webkonsole für die
offizielle CLI, Kanäle senden und empfangen, Grenzwert-Meldungen von Sensormetern und Ping-Zielen
in MeshCore-Kanäle, Firmware-Update per Web mit Board-Prüfung und Rollback.

Bewährte Version: **0.8.0**, neu und in Erprobung: **0.9.0** (beide auf Basis von MeshCore **repeater-v1.18.0**),
für Heltec WiFi LoRa 32 V4 und V4-R8. Im Web-Flasher sind beide wählbar.

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

## Neu in 0.9.0 (in Erprobung, im Web-Flasher wählbar)

- **Alarme „aktiv bis bestätigt“:** Wichtige Meldungen bekommen eine Nummer (`[ACK 3] …`) und werden
  nach 5, 15 und 30 Minuten wiederholt, bis jemand im Kanal mit `ACK 3` antwortet oder in der
  Weboberfläche bestätigt. Die Bestätigung wird im Kanal mit Namen quittiert.
- **Feste IP-Adresse:** IP, Maske, Router und DNS frei einstellbar. Vor dem Umschalten prüft das Gateway
  per ARP und Ping, ob die Adresse schon belegt ist, und fällt bei Problemen sicher auf DHCP zurück.
- **Zabbix-Anbindung:** Alarme tragen eine maschinenlesbare Kennung. Ein zweites Gateway mit derselben
  Firmware empfängt sie per Funk und leitet sie an Zabbix weiter (Trapper). Zabbix-Probleme öffnen und
  schließen sich automatisch, die Annahme wird per ACK im Kanal bestätigt. Vorlage für Zabbix 7 liegt bei.
- **Speicheranzeige:** freier Heap mit Tiefststand seit dem Start und PSRAM-Belegung im Status.

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

Planung und Entscheidungen: [`docs/planung.md`](docs/planung.md), [`docs/entscheidungen.md`](docs/entscheidungen.md).

## Lizenz

GPL-3.0 (`LICENSE`). MeshCore-Anteile stehen unter MIT, siehe `NOTICE`.
