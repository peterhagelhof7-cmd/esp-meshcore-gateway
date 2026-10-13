#include "GatewayConfig.h"

#include <Preferences.h>

GatewayConfig gw_config;

static const char* NVS_NS = "gw";

void GatewayConfig::begin() {
  Preferences p;
  p.begin(NVS_NS, true);
  _s.wifiSsid = p.getString("wifi_ssid", "");
  _s.wifiPass = p.getString("wifi_pass", "");
  _s.hostname = p.getString("hostname", "meshcore-gw");
  _s.wifiPendingTest = p.getBool("wifi_test", false);
  p.end();
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
