#include "NetworkManager.h"
#include "EventLog.h"
#include "GatewayConfig.h"
#include "NetCheck.h"

#include <esp_sntp.h>

NetworkManager network;

// Wird vom SNTP-Callback (lwIP-Task) gesetzt, im loopTask abgeholt.
static volatile bool s_timeSynced = false;

static void onTimeSync(struct timeval* tv) {
  s_timeSynced = true;
}

bool NetworkManager::takeTimeSynced() {
  if (!s_timeSynced) return false;
  s_timeSynced = false;
  return true;
}

static const unsigned long WLAN_CHECK_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const unsigned long WLAN_TEST_TIMEOUT_MS = 30UL * 1000UL;
static const unsigned long RECONNECT_RETRY_INTERVAL_MS = 20UL * 1000UL;
static const unsigned long AP_REJOIN_INTERVAL_MS = 2UL * 60UL * 1000UL;

static const char* AP_SSID = "installer";
static const char* AP_PSK = "installer";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

String NetworkManager::sanitizeHostname(const String& name) {
  String out;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      out += c;
    } else if ((c == ' ' || c == '_') && out.length() > 0 && out[out.length() - 1] != '-') {
      out += '-';
    }
  }
  while (out.length() > 0 && out[0] == '-') out.remove(0, 1);
  if (out.length() > 31) out.remove(31);
  while (out.length() > 0 && out[out.length() - 1] == '-') out.remove(out.length() - 1);
  if (out.isEmpty()) out = FALLBACK_HOSTNAME;
  return out;
}

String NetworkManager::hostname() const {
  char buf[sizeof(_hostname)];
  portENTER_CRITICAL(&_hostMux);
  memcpy(buf, _hostname, sizeof(buf));
  portEXIT_CRITICAL(&_hostMux);
  return String(buf);
}

void NetworkManager::setHostnameNow(const String& h) {
  char buf[sizeof(_hostname)];
  strlcpy(buf, h.c_str(), sizeof(buf));
  portENTER_CRITICAL(&_hostMux);
  memcpy(_hostname, buf, sizeof(buf));
  portEXIT_CRITICAL(&_hostMux);
}

bool NetworkManager::hostnameAuto() const {
  return gw_config.get().hostname.isEmpty();
}

String NetworkManager::wantedHostname() const {
  const GatewaySettings& s = gw_config.get();
  if (!s.hostname.isEmpty()) return s.hostname;
  return sanitizeHostname(_nameSource ? String(_nameSource()) : String());
}

// Automatischer Hostname: Gerätename geändert (Web, CLI, Fern-Admin) ->
// 3 s abwarten (Antwort an den Browser), dann übernehmen. Die Station
// verbindet kurz neu, damit DHCP den neuen Namen an den Router meldet.
void NetworkManager::followDeviceName(unsigned long now) {
  if (now - _hostCheckMs < 1000) return;
  _hostCheckMs = now;
  String want = wantedHostname();
  if (want == hostname()) {
    _hostChangedMs = 0;
    return;
  }
  if (_hostChangedMs == 0) {
    _hostChangedMs = now;
    return;
  }
  if (now - _hostChangedMs < 3000) return;
  _hostChangedMs = 0;
  String old = hostname();
  setHostnameNow(want);
  WiFi.setHostname(want.c_str());
  event_log::addf("NET", "Hostname %s -> %s (folgt Gerätename)", old.c_str(), want.c_str());
  Serial.printf("[NET] Hostname %s -> %s\n", old.c_str(), want.c_str());
  if (_gotIp && !_apActive) WiFi.reconnect();
}

String NetworkManager::ipNote() const {
  char buf[sizeof(_ipNote)];
  portENTER_CRITICAL(&_hostMux);
  memcpy(buf, _ipNote, sizeof(buf));
  portEXIT_CRITICAL(&_hostMux);
  return String(buf);
}

void NetworkManager::setIpNote(const String& n) {
  char buf[sizeof(_ipNote)];
  strlcpy(buf, n.c_str(), sizeof(buf));
  portENTER_CRITICAL(&_hostMux);
  memcpy(_ipNote, buf, sizeof(buf));
  portEXIT_CRITICAL(&_hostMux);
}

void NetworkManager::applyStaticIp() {
  const GatewaySettings& s = gw_config.get();
  IPAddress ip, mask, gw, dns1, dns2;
  ip.fromString(s.ipAddr);
  mask.fromString(s.ipMask);
  gw.fromString(s.ipGw);
  if (!s.ipDns1.length() || !dns1.fromString(s.ipDns1)) dns1 = gw;   // Router als DNS
  if (!s.ipDns2.length() || !dns2.fromString(s.ipDns2)) dns2 = IPAddress(0, 0, 0, 0);
  _staticActive = WiFi.config(ip, gw, mask, dns1, dns2);
  Serial.printf("[NET] Feste IP %s/%s, Gateway %s %s\n", s.ipAddr.c_str(), s.ipMask.c_str(), s.ipGw.c_str(),
                _staticActive ? "gesetzt" : "FEHLER");
}

void NetworkManager::useDhcp() {
  WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));   // 0 = DHCP
  _staticActive = false;
}

// Neue feste IP (E40): erst per DHCP im Netz, dann Adresse prüfen (ARP+Ping),
// umschalten, Router prüfen. Belegt oder Router unerreichbar -> DHCP bleibt
// und die feste IP wird verworfen (Grund im Ereignis-Log und im WLAN-Tab).
void NetworkManager::ipCheckLoop(unsigned long now) {
  GatewaySettings s = gw_config.get();
  char detail[48];
  switch (_ipPhase) {
    case IpPhase::NONE:
      return;
    case IpPhase::WAIT_DHCP: {
      if (!_gotIp) return;
      IPAddress ip;
      ip.fromString(s.ipAddr);
      if (!net_check::probe(ip)) return;   // andere Prüfung läuft noch
      event_log::addf("NET", "Feste IP %s: prüfe, ob die Adresse frei ist (aktuell DHCP %s)", s.ipAddr.c_str(),
                      WiFi.localIP().toString().c_str());
      _ipPhase = IpPhase::CHECK_ADDR;
      return;
    }
    case IpPhase::CHECK_ADDR: {
      net_check::State st = net_check::result(detail, sizeof(detail));
      if (st == net_check::State::RUNNING) return;
      if (st == net_check::State::PRESENT) {
        String note = "Feste IP " + s.ipAddr + " ist belegt (" + detail + "), DHCP bleibt aktiv";
        event_log::add("NET", note.c_str());
        Serial.println("[NET] " + note);
        setIpNote(note);
        GatewayConfig::applyIp(s, "", "", "", "", "", note);   // auf DHCP zurück
        gw_config.set(s);
        _ipPhase = IpPhase::NONE;
        return;
      }
      if (st == net_check::State::UNCHECKABLE) {
        event_log::addf("NET", "Feste IP %s: Belegung nicht prüfbar (%s), setze trotzdem", s.ipAddr.c_str(), detail);
      }
      applyStaticIp();
      _ipPhase = IpPhase::SETTLE;
      _ipPhaseMs = now;
      return;
    }
    case IpPhase::SETTLE: {
      if (now - _ipPhaseMs < 3000) return;
      IPAddress gw;
      gw.fromString(s.ipGw);
      if (!net_check::probe(gw)) return;
      _ipPhase = IpPhase::CHECK_GW;
      return;
    }
    case IpPhase::CHECK_GW: {
      net_check::State st = net_check::result(detail, sizeof(detail));
      if (st == net_check::State::RUNNING) return;
      if (st == net_check::State::PRESENT && _staticActive) {
        String note = "Feste IP " + s.ipAddr + " aktiv, Router " + s.ipGw + " erreichbar";
        event_log::add("NET", note.c_str());
        setIpNote(note);
        s.ipPendingCheck = false;
        gw_config.set(s);
      } else {
        String note = "Feste IP " + s.ipAddr + ": Router " + s.ipGw + " nicht erreichbar (" + detail +
                      "), zurück auf DHCP";
        event_log::add("NET", note.c_str());
        Serial.println("[NET] " + note);
        setIpNote(note);
        useDhcp();
        GatewayConfig::applyIp(s, "", "", "", "", "", note);
        gw_config.set(s);
      }
      _ipPhase = IpPhase::NONE;
      return;
    }
  }
}

void NetworkManager::onEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[NET] WLAN-IP: ");
      Serial.println(WiFi.localIP());
      event_log::addf("NET", "WLAN verbunden (%s), IP %s, RSSI %d dBm", WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
      network._gotIp = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (network._gotIp) {
        Serial.println("[NET] WLAN-Verbindung verloren");
        event_log::add("NET", "WLAN-Verbindung verloren");
      }
      network._gotIp = false;
      break;
    default:
      break;
  }
}

void NetworkManager::startInstallerAp() {
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
  _apActive = WiFi.softAP(AP_SSID, AP_PSK);

  const GatewaySettings& s = gw_config.get();
  if (s.wifiSsid.length() > 0) {
    WiFi.begin(s.wifiSsid.c_str(), s.wifiPass.c_str());
  }
  _lastRejoinMs = millis();

  if (_apActive) {
    Serial.print("[NET] Installer-AP \"installer\" aktiv, IP ");
    Serial.println(WiFi.softAPIP());
    event_log::add("NET", "Installer-AP \"installer\" aktiv (192.168.4.1)");
  } else {
    Serial.println("[NET] Installer-AP konnte nicht gestartet werden");
  }
}

void NetworkManager::begin() {
  GatewaySettings s = gw_config.get();
  setHostnameNow(wantedHostname());

  WiFi.onEvent(onEvent);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.setAutoReconnect(true);
  // Modem-Sleep aus: bei sensormeter-wlan auf HW beobachtete Drops ~alle 20 s
  // (siehe Memory "WLAN stability fixes").
  WiFi.setSleep(false);
  WiFi.setHostname(hostname().c_str());

  // Feste IP (E40): bekannte Adresse sofort, neue erst nach Prüfung
  if (s.staticIp()) {
    if (s.ipPendingCheck) {
      _ipPhase = IpPhase::WAIT_DHCP;
      Serial.printf("[NET] Neue feste IP %s wird nach dem Verbinden geprüft\n", s.ipAddr.c_str());
    } else {
      applyStaticIp();
    }
  }

  if (s.wifiPendingTest) {
    _checkTimeoutMs = WLAN_TEST_TIMEOUT_MS;
    s.wifiPendingTest = false;
    gw_config.set(s);
    Serial.println("[NET] Neu eingetragenes WLAN wird getestet (30 s)");
  } else {
    _checkTimeoutMs = WLAN_CHECK_TIMEOUT_MS;
  }

  _checkStartedMs = millis();
  _lastReconnectMs = millis();

  if (s.wifiSsid.length() == 0) {
    Serial.println("[NET] Kein WLAN konfiguriert -> Installer-AP sofort");
    startInstallerAp();
    return;
  }
  WiFi.begin(s.wifiSsid.c_str(), s.wifiPass.c_str());
  Serial.printf("[NET] Verbinde mit \"%s\" ...\n", s.wifiSsid.c_str());
}

void NetworkManager::loop() {
  const GatewaySettings& s = gw_config.get();
  unsigned long now = millis();
  followDeviceName(now);

  if (_apActive) {
    if (_gotIp) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_MODE_STA);
      _apActive = false;
      Serial.println("[NET] Konfiguriertes WLAN erreicht -> Installer-AP aus");
      event_log::add("NET", "Konfiguriertes WLAN erreicht, Installer-AP aus");
      return;
    }
    if (s.wifiSsid.length() > 0 && now - _lastRejoinMs > AP_REJOIN_INTERVAL_MS) {
      Serial.println("[NET] Installer-AP aktiv - erneuter Versuch ins konfigurierte WLAN");
      WiFi.begin(s.wifiSsid.c_str(), s.wifiPass.c_str());
      _lastRejoinMs = now;
    }
    return;
  }

  if (_gotIp) {
    ipCheckLoop(now);
    if (!_sntpStarted) {
      // Systemzeit (Epoch) ist immer UTC, so rechnet MeshCore. Die TZ-Regel
      // betrifft nur localtime() fürs OLED. SNTP synchronisiert danach selbst
      // weiter (Standard 1 h), jede Synchronisation meldet onTimeSync.
      sntp_set_time_sync_notification_cb(onTimeSync);
      configTzTime(TZ_RULE, "pool.ntp.org", "time.cloudflare.com");
      _sntpStarted = true;
      Serial.println("[NET] NTP gestartet");
    }
    _checkStartedMs = now;   // Timeout läuft erst ab Verbindungsverlust
    _checkTimeoutMs = WLAN_CHECK_TIMEOUT_MS;
    _lastReconnectMs = now;
    return;
  }

  if (now - _checkStartedMs > _checkTimeoutMs) {
    Serial.println("[NET] Kein WLAN erreichbar -> Installer-AP");
    startInstallerAp();
  } else if (now - _lastReconnectMs > RECONNECT_RETRY_INTERVAL_MS) {
    WiFi.reconnect();
    _lastReconnectMs = now;
  }
}

NetworkManager::Mode NetworkManager::mode() const {
  if (_apActive) return Mode::INSTALLER_AP;
  return _gotIp ? Mode::STATION : Mode::CONNECTING;
}

IPAddress NetworkManager::ip() const {
  if (_gotIp) return WiFi.localIP();
  if (_apActive) return WiFi.softAPIP();
  return IPAddress(0, 0, 0, 0);
}

String NetworkManager::ssid() const {
  if (_gotIp) return WiFi.SSID();
  if (_apActive) return String(AP_SSID);
  return gw_config.get().wifiSsid;
}

int NetworkManager::rssi() const {
  return _gotIp ? WiFi.RSSI() : 0;
}
