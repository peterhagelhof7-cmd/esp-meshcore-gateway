#pragma once

#include <Arduino.h>

// Weboberfläche (ESPAsyncWebServer, Port 80).
// M1: Statusseite + WLAN-Einrichtung + Neustart.
// M2: Webkonsole (POST /api/cli, GET /api/console) + Paket-Log (GET /api/packets).
// M3: Einstellungen über CLI-Batches (POST /api/cli/batch, GET /api/cli/job),
//     Empfehlungsprofil (GET /api/profile), Backup/Restore im Browser.
// Login: HTTP-Basic-Auth, Benutzer "admin", Passwort = MeshCore-Admin-Passwort
// des Knotens (CLI "password", docs/entscheidungen.md E7).
namespace web_ui {
  void begin();
  // Aus dem loopTask: führt angeforderte Aktionen (Neustart) aus.
  void loop();
}
