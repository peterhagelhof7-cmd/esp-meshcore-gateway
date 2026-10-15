#include "GatewayConfig.h"

#include <Preferences.h>

#include "NetCheck.h"
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
  _s.ipAddr = p.getString("ip_addr", "");
  _s.ipMask = p.getString("ip_mask", "");
  _s.ipGw = p.getString("ip_gw", "");
  _s.ipDns1 = p.getString("ip_dns1", "");
  _s.ipDns2 = p.getString("ip_dns2", "");
  _s.ipPendingCheck = p.getBool("ip_check", false);
  p.end();
}

static bool parseIp(const String& in, IPAddress& out) {
  String t = in;
  t.trim();
  return t.length() > 0 && out.fromString(t);
}

bool GatewayConfig::applyIp(GatewaySettings& s, const String& ipIn, const String& maskIn, const String& gwIn,
                            const String& dns1In, const String& dns2In, String& err) {
  String ipT = ipIn;
  ipT.trim();
  if (ipT.isEmpty()) {   // DHCP
    s.ipAddr = s.ipMask = s.ipGw = s.ipDns1 = s.ipDns2 = String();
    s.ipPendingCheck = false;
    return true;
  }
  IPAddress ip, mask, gw, dns1, dns2;
  if (!parseIp(ipT, ip)) { err = "IP-Adresse ungültig"; return false; }
  if (!parseIp(maskIn, mask) || !net_check::validMask(mask)) { err = "Subnetzmaske ungültig (z. B. 255.255.255.0)"; return false; }
  if (!parseIp(gwIn, gw)) { err = "Gateway (Router) ungültig"; return false; }
  String d1 = dns1In, d2 = dns2In;
  d1.trim();
  d2.trim();
  if (d1.length() && !parseIp(d1, dns1)) { err = "DNS 1 ungültig"; return false; }
  if (d2.length() && !parseIp(d2, dns2)) { err = "DNS 2 ungültig"; return false; }
  uint32_t m = (uint32_t)mask, a = (uint32_t)ip, g = (uint32_t)gw;   // Netzreihenfolge, Maske wirkt bytweise gleich
  if ((a & m) != (g & m)) { err = "Gateway liegt nicht im Subnetz der IP-Adresse"; return false; }
  if ((a & ~m) == 0 || (a & ~m) == ~m) { err = "IP-Adresse ist Netz- oder Broadcast-Adresse"; return false; }
  if (a == g) { err = "IP-Adresse und Gateway sind gleich"; return false; }
  String ipStr = ip.toString();
  if (ipStr != s.ipAddr) s.ipPendingCheck = true;   // neue Adresse -> vor dem Umschalten prüfen
  s.ipAddr = ipStr;
  s.ipMask = mask.toString();
  s.ipGw = gw.toString();
  s.ipDns1 = d1.length() ? dns1.toString() : String();
  s.ipDns2 = d2.length() ? dns2.toString() : String();
  return true;
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
  p.putString("ip_addr", _s.ipAddr);
  p.putString("ip_mask", _s.ipMask);
  p.putString("ip_gw", _s.ipGw);
  p.putString("ip_dns1", _s.ipDns1);
  p.putString("ip_dns2", _s.ipDns2);
  p.putBool("ip_check", _s.ipPendingCheck);
  p.end();
}
