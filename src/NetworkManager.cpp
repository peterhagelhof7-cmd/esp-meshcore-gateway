#include "NetworkManager.h"
#include "EventLog.h"
#include "GatewayConfig.h"

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
  while (out.length() > 0 && out[out.length() - 1] == '-') out.remove(out.length() - 1);
  while (out.length() > 0 && out[0] == '-') out.remove(0, 1);
  if (out.isEmpty()) out = "meshcore-gw";
  return out;
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
  _hostname = sanitizeHostname(s.hostname);

  WiFi.onEvent(onEvent);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.setAutoReconnect(true);
  // Modem-Sleep aus: bei sensormeter-wlan auf HW beobachtete Drops ~alle 20 s
  // (siehe Memory "WLAN stability fixes").
  WiFi.setSleep(false);
  WiFi.setHostname(_hostname.c_str());

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
