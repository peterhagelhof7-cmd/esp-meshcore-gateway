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
  // Feste IP (E40). ipAddr leer = DHCP. dns1 leer = Gateway als DNS.
  String ipAddr, ipMask, ipGw, ipDns1, ipDns2;
  // Einmal-Flag: neue feste IP beim nächsten Verbinden erst per DHCP
  // anmelden, Adresse auf Belegung prüfen, dann umschalten und das Gateway
  // (Router) prüfen; bei Fehler zurück auf DHCP.
  bool ipPendingCheck = false;
  // Schlüssel der Kanal-API (E43), 32 Hex-Zeichen. Leer = API aus.
  String apiKey;
  // Senden über die API erlaubt in diesem Kanal (Name, E44). Leer = nur lesen.
  String apiSendChannel;

  bool staticIp() const { return ipAddr.length() > 0; }
};

class GatewayConfig {
public:
  void begin();
  const GatewaySettings& get() const { return _s; }
  void set(const GatewaySettings& s);

  // Eingabe aus Formular/Backup -> gespeicherter Wert: leer oder der alte
  // Werkswert "meshcore-gw" = automatisch (""), sonst bereinigter Name.
  static String hostSetting(const String& input);

  // Feste IP prüfen und normiert in s übernehmen. ip leer = DHCP (übrige
  // Felder werden dann geleert). false = ungültig, err = Grund. Setzt
  // ipPendingCheck, wenn sich die feste Adresse ändert.
  static bool applyIp(GatewaySettings& s, const String& ip, const String& mask, const String& gw,
                      const String& dns1, const String& dns2, String& err);

private:
  GatewaySettings _s;
};

extern GatewayConfig gw_config;
