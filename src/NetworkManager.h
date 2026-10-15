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
  String hostname() const;   // aktuell verwendeter (thread-sicher)
  bool hostnameAuto() const;

  // Quelle des Gerätenamens (MeshCore node_name) für den automatischen
  // Hostnamen (E32). Vor begin() setzen, Aufruf nur im loopTask.
  void setNameSource(const char* (*fn)()) { _nameSource = fn; }

  static constexpr const char* FALLBACK_HOSTNAME = "meshcore-gw";
  // a-z, 0-9, '-', max. 31 Zeichen (Puffer im WLAN-Treiber), leer -> FALLBACK
  static String sanitizeHostname(const String& name);
  // Mitteleuropa mit Sommerzeit. Wird schon in setup() gesetzt: die Systemzeit
  // überlebt einen Software-Neustart, sonst stehen die ersten Log-Zeilen in UTC.
  static constexpr const char* TZ_RULE = "CET-1CEST,M3.5.0,M10.5.0/3";

  // true genau einmal nach jeder erfolgreichen NTP-Synchronisation
  // (SNTP startet automatisch, sobald die Station eine IP hat).
  bool takeTimeSynced();

  // Feste IP (E40): true = gerade aktiv (nicht DHCP)
  bool staticActive() const { return _staticActive; }
  // Ergebnis der letzten Prüfung einer neuen festen IP (leer = keins).
  // Nur loopTask schreibt; Web liest eine Kopie.
  String ipNote() const;

private:
  void applyStaticIp();
  void useDhcp();
  void ipCheckLoop(unsigned long now);
  void setIpNote(const String& n);
  enum class IpPhase : uint8_t { NONE, WAIT_DHCP, CHECK_ADDR, SETTLE, CHECK_GW };
  IpPhase _ipPhase = IpPhase::NONE;
  unsigned long _ipPhaseMs = 0;
  bool _staticActive = false;
  char _ipNote[96] = {0};

  void startInstallerAp();
  String wantedHostname() const;
  void followDeviceName(unsigned long now);
  static void onEvent(WiFiEvent_t event);

  void setHostnameNow(const String& h);
  char _hostname[32] = {0};
  mutable portMUX_TYPE _hostMux = portMUX_INITIALIZER_UNLOCKED;
  const char* (*_nameSource)() = nullptr;
  unsigned long _hostCheckMs = 0;
  unsigned long _hostChangedMs = 0;   // 0 = keine Änderung anstehend
  unsigned long _checkStartedMs = 0;
  unsigned long _checkTimeoutMs = 0;
  unsigned long _lastReconnectMs = 0;
  unsigned long _lastRejoinMs = 0;
  volatile bool _gotIp = false;
  volatile bool _apActive = false;
  bool _sntpStarted = false;
};

extern NetworkManager network;
