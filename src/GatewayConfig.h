#pragma once

#include <Arduino.h>

// Eigene Gateway-Konfiguration (WLAN, Hostname) in NVS, Namespace "gw".
// Bewusst getrennt von den MeshCore-Prefs auf SPIFFS: ein MeshCore-"erase"
// löscht die WLAN-Zugangsdaten nicht und umgekehrt (docs/planung.md).
struct GatewaySettings {
  String wifiSsid;
  String wifiPass;
  // Leer = automatisch: Hostname folgt dem MeshCore-Gerätenamen (E32).
  String hostname;
  // Einmal-Flag: beim nächsten Boot das neu eingetragene WLAN mit kurzem
  // Timeout testen (wie sensormeter-wlan).
  bool wifiPendingTest = false;
};

class GatewayConfig {
public:
  void begin();
  const GatewaySettings& get() const { return _s; }
  void set(const GatewaySettings& s);

  // Eingabe aus Formular/Backup -> gespeicherter Wert: leer oder der alte
  // Werkswert "meshcore-gw" = automatisch (""), sonst bereinigter Name.
  static String hostSetting(const String& input);

private:
  GatewaySettings _s;
};

extern GatewayConfig gw_config;
