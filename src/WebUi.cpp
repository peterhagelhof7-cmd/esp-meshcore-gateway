#include "WebUi.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "GatewayConfig.h"
#include "GatewayStatus.h"
#include "NetworkManager.h"
#include "web/index_html.h"

static AsyncWebServer server(80);
static volatile uint32_t s_rebootAt = 0;   // 0 = kein Neustart angefordert

static const char* WEB_USER = "admin";
static const char* REALM = "MeshCore Gateway";

static bool checkAuth(AsyncWebServerRequest* req) {
  MeshSnapshot s = gateway_status::get();
  if (!s.valid) {
    req->send(503, "text/plain", "Mesh startet noch");
    return false;
  }
  if (!req->authenticate(WEB_USER, s.adminPassword)) {
    req->requestAuthentication(REALM);
    return false;
  }
  return true;
}

static String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char b[8];
          snprintf(b, sizeof(b), "\\u%04x", c);
          out += b;
        } else {
          out += c;
        }
    }
  }
  return out;
}

static const char* modeName(NetworkManager::Mode m) {
  switch (m) {
    case NetworkManager::Mode::STATION: return "station";
    case NetworkManager::Mode::INSTALLER_AP: return "installer_ap";
    default: return "connecting";
  }
}

static void handleStatus(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  MeshSnapshot s = gateway_status::get();

  String j;
  j.reserve(1200);
  j += "{\"gateway\":{\"version\":\"" GATEWAY_VERSION "\",\"uptime_ms\":";
  j += String(millis());
  j += ",\"free_heap\":";
  j += String(ESP.getFreeHeap());
  j += ",\"cpu_mhz\":";
  j += String(getCpuFrequencyMhz());
  j += "},\"node\":{\"name\":\"";
  j += jsonEscape(s.nodeName);
  j += "\",\"pubkey\":\"";
  j += s.pubKeyHex;
  j += "\",\"role\":\"";
  j += jsonEscape(s.role);
  j += "\",\"firmware\":\"";
  j += jsonEscape(s.firmwareVer);
  j += "\",\"build_date\":\"";
  j += jsonEscape(s.buildDate);
  j += "\",\"lat\":";
  j += String(s.lat, 6);
  j += ",\"lon\":";
  j += String(s.lon, 6);
  j += ",\"forwarding\":";
  j += s.forwardingEnabled ? "true" : "false";
  j += "},\"radio\":{\"freq\":";
  j += String(s.freq, 3);
  j += ",\"bw\":";
  j += String(s.bw, 1);
  j += ",\"sf\":";
  j += String(s.sf);
  j += ",\"cr\":";
  j += String(s.cr);
  j += ",\"tx_power\":";
  j += String(s.txPower);
  j += "},\"stats_core\":";
  j += s.coreStatsJson[0] ? s.coreStatsJson : "null";
  j += ",\"stats_radio\":";
  j += s.radioStatsJson[0] ? s.radioStatsJson : "null";
  j += ",\"stats_packets\":";
  j += s.packetStatsJson[0] ? s.packetStatsJson : "null";
  j += ",\"wifi\":{\"mode\":\"";
  j += modeName(network.mode());
  j += "\",\"ssid\":\"";
  j += jsonEscape(network.ssid());
  j += "\",\"ip\":\"";
  j += network.ip().toString();
  j += "\",\"rssi\":";
  j += String(network.rssi());
  j += ",\"mac\":\"";
  j += WiFi.macAddress();
  j += "\",\"hostname\":\"";
  j += jsonEscape(network.hostname());
  j += "\"}}";

  req->send(200, "application/json", j);
}

static void handleWifiScan(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    req->send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    req->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  String j = "{\"scanning\":false,\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    j += "{\"ssid\":\"";
    j += jsonEscape(WiFi.SSID(i));
    j += "\",\"rssi\":";
    j += String(WiFi.RSSI(i));
    j += ",\"secure\":";
    j += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true";
    j += '}';
  }
  j += "]}";
  WiFi.scanDelete();
  req->send(200, "application/json", j);
}

static void handleWifiSave(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  if (!req->hasParam("ssid", true)) {
    req->send(400, "application/json", "{\"error\":\"ssid fehlt\"}");
    return;
  }
  String ssid = req->getParam("ssid", true)->value();
  String pass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : String();
  String host = req->hasParam("hostname", true) ? req->getParam("hostname", true)->value() : String();
  ssid.trim();
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 63) {
    req->send(400, "application/json", "{\"error\":\"SSID 1-32 Zeichen, Passwort max. 63 Zeichen\"}");
    return;
  }

  GatewaySettings s = gw_config.get();
  // Leeres Passwort bei unveränderter SSID = altes Passwort behalten
  // (das Formular zeigt gespeicherte Passwörter nie an).
  if (!(pass.length() == 0 && ssid == s.wifiSsid)) s.wifiPass = pass;
  s.wifiSsid = ssid;
  if (host.length() > 0) s.hostname = NetworkManager::sanitizeHostname(host);
  s.wifiPendingTest = true;
  gw_config.set(s);

  req->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
  s_rebootAt = millis() + 1500;
}

static void handleReboot(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  req->send(200, "application/json", "{\"ok\":true}");
  s_rebootAt = millis() + 1000;
}

void web_ui::begin() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    AsyncWebServerResponse* r = req->beginResponse(200, "text/html", (const uint8_t*)INDEX_HTML, strlen(INDEX_HTML));
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "Nicht gefunden");
  });
  server.begin();
  Serial.println("[WEB] Webserver auf Port 80 gestartet");
}

void web_ui::loop() {
  uint32_t at = s_rebootAt;
  if (at != 0 && (int32_t)(millis() - at) >= 0) {
    Serial.println("[WEB] Neustart angefordert");
    delay(100);
    ESP.restart();
  }
}
