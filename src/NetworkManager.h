#pragma once

#include <Arduino.h>
#include <WiFi.h>

// WLAN-Zustandsautomat, übernommen von sensormeter-wlan (NetworkManager):
//   - konfiguriertes WLAN als Station, aktiver Reconnect alle 20 s
//   - nach 5 min ohne IP (30 s direkt nach neuer Eingabe) eigener Access-Point
//     "installer"/"installer" auf 192.168.4.1, im AP_STA-Modus, damit die
//     Station parallel weiter ins konfigurierte WLAN zu kommen versucht
//   - Selbstheilung: sobald die Station eine IP hat, wird der AP abgeschaltet
// Abweichung: Ist gar kein WLAN konfiguriert (Erstinbetriebnahme), startet der
// AP sofort statt nach 5 min (docs/entscheidungen.md E6).
class NetworkManager {
public:
  enum class Mode : uint8_t { CONNECTING, STATION, INSTALLER_AP };

  void begin();
  void loop();

  Mode mode() const;
  bool isApActive() const { return _apActive; }
  IPAddress ip() const;
  String ssid() const;
  int rssi() const;
  String hostname() const { return _hostname; }

  static String sanitizeHostname(const String& name);
  // Mitteleuropa mit Sommerzeit. Wird schon in setup() gesetzt: die Systemzeit
  // überlebt einen Software-Neustart, sonst stehen die ersten Log-Zeilen in UTC.
  static constexpr const char* TZ_RULE = "CET-1CEST,M3.5.0,M10.5.0/3";

  // true genau einmal nach jeder erfolgreichen NTP-Synchronisation
  // (SNTP startet automatisch, sobald die Station eine IP hat).
  bool takeTimeSynced();

private:
  void startInstallerAp();
  static void onEvent(WiFiEvent_t event);

  String _hostname;
  unsigned long _checkStartedMs = 0;
  unsigned long _checkTimeoutMs = 0;
  unsigned long _lastReconnectMs = 0;
  unsigned long _lastRejoinMs = 0;
  volatile bool _gotIp = false;
  volatile bool _apActive = false;
  bool _sntpStarted = false;
};

extern NetworkManager network;
