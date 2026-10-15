# MeshCore-Gateway für Heltec WiFi LoRa 32 V4

MeshCore-Repeater mit WLAN: Weboberfläche für alle MeshCore-Einstellungen, Webkonsole für die
offizielle CLI, Kanäle senden und empfangen, Grenzwert-Meldungen von Sensormetern und Ping-Zielen
in MeshCore-Kanäle, Firmware-Update per Web mit Board-Prüfung und Rollback.

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
