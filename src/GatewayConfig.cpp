#include "GatewayConfig.h"

#include <Preferences.h>

#include "NetworkManager.h"

GatewayConfig gw_config;

static const char* NVS_NS = "gw";

void GatewayConfig::begin() {
  Preferences p;
  p.begin(NVS_NS, true);
  _s.wifiSsid = p.getString("wifi_ssid", "");
  _s.wifiPass = p.getString("wifi_pass", "");
  // Bis 0.7.3 war "meshcore-gw" vorbelegt und wurde beim WLAN-Einrichten
  // mitgespeichert -> gilt als "automatisch".
  _s.hostname = hostSetting(p.getString("hostname", ""));
  _s.wifiPendingTest = p.getBool("wifi_test", false);
  p.end();
}

String GatewayConfig::hostSetting(const String& input) {
  String h = input;
  h.trim();
  if (h.isEmpty()) return String();
  h = NetworkManager::sanitizeHostname(h);
  return h == NetworkManager::FALLBACK_HOSTNAME ? String() : h;
}

void GatewayConfig::set(const GatewaySettings& s) {
  _s = s;
  Preferences p;
  p.begin(NVS_NS, false);
  p.putString("wifi_ssid", _s.wifiSsid);
  p.putString("wifi_pass", _s.wifiPass);
  p.putString("hostname", _s.hostname);
  p.putBool("wifi_test", _s.wifiPendingTest);
  p.end();
}
