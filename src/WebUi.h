#pragma once

#include <Arduino.h>

// Weboberfläche (ESPAsyncWebServer, Port 80).
// M1: Statusseite + WLAN-Einrichtung + Neustart.
// Login: HTTP-Basic-Auth, Benutzer "admin", Passwort = MeshCore-Admin-Passwort
// des Knotens (CLI "password", docs/entscheidungen.md E7).
namespace web_ui {
  void begin();
  // Aus dem loopTask: führt angeforderte Aktionen (Neustart) aus.
  void loop();
}
