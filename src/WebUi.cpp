#include "WebUi.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <memory>
#include <vector>

#include "ChannelStore.h"
#include "CliBridge.h"
#include "EventLog.h"
#include "GatewayConfig.h"
#include "GatewayStatus.h"
#include "LogBuffer.h"
#include "MeshActions.h"
#include "NetCheck.h"
#include "NetworkManager.h"
#include "OtaUpdate.h"
#include "Profile.h"
#include "SmConfig.h"
#include "SmPoller.h"
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

static String formParam(AsyncWebServerRequest* req, const char* name) {
  return req->hasParam(name, true) ? req->getParam(name, true)->value() : String();
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
  float chip = sm_poller::chipTempC();
  j += ",\"chip_temp_c\":";
  j += isnan(chip) ? String("null") : String(chip, 1);
  j += ",\"project\":\"";
  j += ota_update::projectId();
  j += "\",\"partition\":\"";
  j += ota_update::runningPartition();
  j += "\",\"image_state\":\"";
  j += jsonEscape(ota_update::imageState());
  j += "\"";
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
  j += "\",\"hostname_auto\":";
  j += network.hostnameAuto() ? "true" : "false";
  // Feste IP (E40): aktuelle Werte und Einstellung
  const GatewaySettings& g = gw_config.get();
  bool sta = network.mode() == NetworkManager::Mode::STATION;
  j += ",\"static\":";
  j += network.staticActive() ? "true" : "false";
  j += ",\"mask\":\"" + (sta ? WiFi.subnetMask().toString() : String()) + "\"";
  j += ",\"gw\":\"" + (sta ? WiFi.gatewayIP().toString() : String()) + "\"";
  j += ",\"dns\":\"" + (sta ? WiFi.dnsIP(0).toString() : String()) + "\"";
  j += ",\"cfg\":{\"ip\":\"" + jsonEscape(g.ipAddr) + "\",\"mask\":\"" + jsonEscape(g.ipMask) + "\",\"gw\":\"" +
       jsonEscape(g.ipGw) + "\",\"dns1\":\"" + jsonEscape(g.ipDns1) + "\",\"dns2\":\"" + jsonEscape(g.ipDns2) +
       "\",\"pending\":" + (g.ipPendingCheck ? "true" : "false") + "}";
  j += ",\"ip_note\":\"" + jsonEscape(network.ipNote()) + "\"";
  j += "}}";

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
  // Feste IP (E40): fehlt das Feld = unverändert, leer = DHCP
  if (req->hasParam("ip", true)) {
    String err;
    if (!GatewayConfig::applyIp(s, formParam(req, "ip"), formParam(req, "mask"), formParam(req, "gw"),
                                formParam(req, "dns1"), formParam(req, "dns2"), err)) {
      req->send(400, "application/json", "{\"error\":\"" + jsonEscape(err) + "\"}");
      return;
    }
  }
  // Leeres Passwort bei unveränderter SSID = altes Passwort behalten
  // (das Formular zeigt gespeicherte Passwörter nie an).
  if (!(pass.length() == 0 && ssid == s.wifiSsid)) s.wifiPass = pass;
  s.wifiSsid = ssid;
  // Leer = automatisch (folgt dem Gerätenamen), fehlt das Feld = unverändert.
  if (req->hasParam("hostname", true)) s.hostname = GatewayConfig::hostSetting(host);
  s.wifiPendingTest = true;
  gw_config.set(s);
  event_log::addf("SYS", "WLAN-Einstellungen gespeichert (%s, %s), Neustart", ssid.c_str(),
                  s.staticIp() ? ("feste IP " + s.ipAddr).c_str() : "DHCP");

  req->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
  s_rebootAt = millis() + 1500;
}

// Feste IP vorab prüfen (E40): POST startet, GET liefert den Stand.
static void handleCheckIpStart(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  IPAddress ip;
  String s = formParam(req, "ip");
  s.trim();
  if (!ip.fromString(s)) {
    req->send(400, "application/json", "{\"error\":\"IP-Adresse ungültig\"}");
    return;
  }
  if (!net_check::probe(ip)) {
    req->send(409, "application/json", "{\"error\":\"Es läuft schon eine Prüfung, gleich noch einmal\"}");
    return;
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleCheckIpResult(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  char detail[48];
  net_check::State st = net_check::result(detail, sizeof(detail));
  req->send(200, "application/json",
            String("{\"state\":\"") + net_check::stateName(st) + "\",\"detail\":\"" + jsonEscape(String(detail)) + "\"}");
}

static void handleCli(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  // Leerer Befehl ist erlaubt: beendet z. B. "region load" (Upstream erwartet
  // dafür eine Leerzeile).
  String cmd = req->hasParam("cmd", true) ? req->getParam("cmd", true)->value() : String();
  cmd.replace("\r", "");
  cmd.replace("\n", " ");
  if (cmd.length() > cli_bridge::MAX_CMD_LEN) {
    req->send(400, "application/json", "{\"error\":\"Befehl zu lang (max. 159 Zeichen)\"}");
    return;
  }
  uint32_t id = cli_bridge::submitOne(cmd.c_str());
  if (!id) {
    req->send(503, "application/json", "{\"error\":\"Warteschlange voll, bitte kurz warten\"}");
    return;
  }
  req->send(200, "application/json", String("{\"ok\":true,\"id\":") + id + "}");
}

// Mehrere Befehle als ein Job: beliebig viele Formularfelder "cmd" (Reihenfolge
// bleibt erhalten), optional quiet=1 (get-Abfragen nicht in die Konsole).
static void handleCliBatch(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  static const char* ERR_LEN = "{\"error\":\"Befehl zu lang (max. 159 Zeichen)\"}";
  std::vector<String> cmds;
  bool quiet = false;
  for (size_t i = 0; i < req->params(); i++) {
    const AsyncWebParameter* p = req->getParam(i);
    if (!p->isPost()) continue;
    if (p->name() == "quiet") {
      quiet = p->value() == "1";
    } else if (p->name() == "cmd") {
      String c = p->value();
      c.replace("\r", "");
      c.replace("\n", " ");
      if (c.length() > cli_bridge::MAX_CMD_LEN) {
        req->send(400, "application/json", ERR_LEN);
        return;
      }
      cmds.push_back(c);
    }
  }
  if (cmds.empty() || cmds.size() > cli_bridge::MAX_BATCH) {
    req->send(400, "application/json", "{\"error\":\"1 bis 40 Befehle pro Auftrag\"}");
    return;
  }
  const char* ptrs[cli_bridge::MAX_BATCH];
  for (size_t i = 0; i < cmds.size(); i++) ptrs[i] = cmds[i].c_str();
  uint32_t id = cli_bridge::submit(ptrs, cmds.size(), quiet);
  if (!id) {
    req->send(503, "application/json", "{\"error\":\"Warteschlange voll, bitte kurz warten\"}");
    return;
  }
  req->send(200, "application/json", String("{\"ok\":true,\"id\":") + id + "}");
}

static void handleCliJob(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  uint32_t id = req->hasParam("id") ? strtoul(req->getParam("id")->value().c_str(), nullptr, 10) : 0;
  String j;
  j.reserve(2048);
  if (!cli_bridge::jobJson(id, j)) {
    req->send(404, "application/json", "{\"error\":\"Auftrag unbekannt\"}");
    return;
  }
  req->send(200, "application/json", j);
}

static void handleProfile(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  String j;
  j.reserve(4096);
  j += "{\"name\":\"";
  j += jsonEscape(profile::NAME);
  j += "\",\"entries\":[";
  for (size_t i = 0; i < profile::COUNT; i++) {
    const ProfileEntry& e = profile::ENTRIES[i];
    if (i) j += ',';
    j += "{\"cmd\":\"";
    j += jsonEscape(e.cmd);
    j += "\",\"get\":";
    j += e.get ? "\"" + jsonEscape(e.get) + "\"" : String("null");
    j += ",\"expect\":";
    j += e.expect ? "\"" + jsonEscape(e.expect) + "\"" : String("null");
    j += ",\"why\":\"";
    j += jsonEscape(e.why);
    j += "\"}";
  }
  j += "]}";
  req->send(200, "application/json", j);
}

static void sendLog(AsyncWebServerRequest* req, LogBuffer& log) {
  if (!checkAuth(req)) return;
  uint32_t since = 0;
  if (req->hasParam("since")) since = strtoul(req->getParam("since")->value().c_str(), nullptr, 10);

  String j;
  j.reserve(4096);
  j += "{\"lines\":[";
  bool gap = false;
  uint32_t next = log.appendJson(since, 60, j, gap);
  j += "],\"next\":";
  j += String(next);
  j += ",\"gap\":";
  j += gap ? "true" : "false";
  j += '}';
  req->send(200, "application/json", j);
}

// ---------- M4: Kanäle ----------

static void handleChannelsGet(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  GwChannel ch[channel_store::MAX_CHANNELS];
  channel_store::list(ch);
  String j;
  j.reserve(1536);
  j += "{\"max\":";
  j += String(channel_store::MAX_CHANNELS);
  j += ",\"channels\":[";
  bool first = true;
  for (int i = 0; i < channel_store::MAX_CHANNELS; i++) {
    if (!ch[i].used) continue;
    if (!first) j += ',';
    first = false;
    channel_store::Kind k = channel_store::kindOf(ch[i]);
    j += "{\"idx\":";
    j += String(i);
    j += ",\"name\":\"";
    j += jsonEscape(ch[i].name);
    j += "\",\"kind\":\"";
    j += k == channel_store::Kind::PUBLIC ? "public" : (k == channel_store::Kind::HASHTAG ? "hashtag" : "private");
    j += "\",\"scope\":\"";
    j += jsonEscape(ch[i].scope);
    j += "\",\"hash\":\"";
    char hb[4];
    snprintf(hb, sizeof(hb), "%02x", ch[i].hash);
    j += hb;
    j += "\",\"key_b64\":\"";
    j += channel_store::keyBase64(ch[i]);
    j += "\",\"key_hex\":\"";
    j += channel_store::keyHex(ch[i]);
    j += "\"}";
  }
  j += "]}";
  req->send(200, "application/json", j);
}

static void handleChannelsPost(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  String action = formParam(req, "action");
  String err;
  if (action == "add") {
    String kind = formParam(req, "kind");
    channel_store::Kind k = kind == "public" ? channel_store::Kind::PUBLIC
                          : kind == "hashtag" ? channel_store::Kind::HASHTAG
                          : channel_store::Kind::PRIVATE;
    String key = formParam(req, "key");
    key.trim();
    int idx = channel_store::add(k, formParam(req, "name").c_str(), key.c_str(), formParam(req, "scope").c_str(), err);
    if (idx < 0) {
      req->send(400, "application/json", "{\"error\":\"" + jsonEscape(err) + "\"}");
      return;
    }
    req->send(200, "application/json", String("{\"ok\":true,\"idx\":") + idx + "}");
  } else if (action == "delete") {
    if (!channel_store::remove(formParam(req, "idx").toInt())) {
      req->send(404, "application/json", "{\"error\":\"Kanal unbekannt\"}");
      return;
    }
    req->send(200, "application/json", "{\"ok\":true}");
  } else if (action == "scope") {
    if (!channel_store::setScope(formParam(req, "idx").toInt(), formParam(req, "scope").c_str(), err)) {
      req->send(400, "application/json", "{\"error\":\"" + jsonEscape(err) + "\"}");
      return;
    }
    req->send(200, "application/json", "{\"ok\":true}");
  } else {
    req->send(400, "application/json", "{\"error\":\"unbekannte Aktion\"}");
  }
}

static void handleChannelSend(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  int idx = formParam(req, "idx").toInt();
  String text = formParam(req, "text");
  text.replace("\r", "");
  text.replace("\n", " ");
  text.trim();
  GwChannel c;
  if (!channel_store::get(idx, c)) {
    req->send(404, "application/json", "{\"error\":\"Kanal unbekannt\"}");
    return;
  }
  if (text.isEmpty() || text.length() > mesh_actions::MAX_TEXT) {
    req->send(400, "application/json", "{\"error\":\"Text 1-160 Zeichen\"}");
    return;
  }
  if (!mesh_actions::sendChannel(idx, text.c_str(), mesh_actions::Origin::WEB)) {
    req->send(503, "application/json", "{\"error\":\"Warteschlange voll, bitte kurz warten\"}");
    return;
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

// ---------- M6: Sensormeter ----------

static void handleSmGet(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  String j;
  j.reserve(6144);
  j += "{\"config\":";
  sm::toJson(j);
  j += ",\"status\":";
  sm_poller::statusJson(j);
  j += '}';
  req->send(200, "application/json", j);
}

// Backup-Teile, die nicht über die CLI lesbar sind (E28): Hostname/WLAN,
// Admin-Passwort, Zugriffsliste (ACL), Sensormeter-Passwörter.
// Geheimnisse nur mit ?secrets=1.
static void handleBackupExtra(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  bool secrets = req->hasParam("secrets") && req->getParam("secrets")->value() == "1";
  const GatewaySettings& g = gw_config.get();
  String j;
  j.reserve(8192);
  j += "{\"hostname\":\"" + jsonEscape(g.hostname) + "\",\"wifi\":{\"ssid\":\"" + jsonEscape(g.wifiSsid) + "\"";
  if (secrets) j += ",\"pass\":\"" + jsonEscape(g.wifiPass) + "\"";
  // Feste IP (E40), leer = DHCP
  j += ",\"ip\":\"" + jsonEscape(g.ipAddr) + "\",\"mask\":\"" + jsonEscape(g.ipMask) + "\",\"gw\":\"" +
       jsonEscape(g.ipGw) + "\",\"dns1\":\"" + jsonEscape(g.ipDns1) + "\",\"dns2\":\"" + jsonEscape(g.ipDns2) + "\"";
  j += "}";
  if (secrets) {
    MeshSnapshot s = gateway_status::get();
    j += ",\"adminPassword\":\"" + jsonEscape(String(s.adminPassword)) + "\"";
  }
  j += ",\"acl\":[";
  cli_bridge::AclEntry* acl = (cli_bridge::AclEntry*)malloc(sizeof(cli_bridge::AclEntry) * 32);
  int n = acl ? cli_bridge::readAcl(acl, 32) : 0;
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    j += "{\"perm\":" + String(acl[i].perm) + ",\"pubkey\":\"" + acl[i].pubkeyHex + "\"}";
  }
  free(acl);
  j += "],\"sensormeter\":";
  sm::toJson(j, !secrets);
  j += '}';
  req->send(200, "application/json", j);
}

// Letzter Schritt einer Wiederherstellung: Admin-Passwort und/oder WLAN.
// Serverseitig in einem Rutsch, weil der Browser nach der Passwortänderung
// keinen gültigen Login mehr hat (E28).
static void handleBackupApply(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  String pw = formParam(req, "adminPassword");
  String ssid = formParam(req, "ssid");
  ssid.trim();
  if (pw.length() > 0) {
    bool ok = pw.length() <= 15;
    for (size_t i = 0; i < pw.length(); i++) ok = ok && pw[i] > ' ';
    if (!ok) {
      req->send(400, "application/json", "{\"error\":\"Admin-Passwort: 1-15 Zeichen, keine Leerzeichen\"}");
      return;
    }
  }
  if (req->hasParam("ssid", true)) {
    String pass = formParam(req, "pass");
    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 63) {
      req->send(400, "application/json", "{\"error\":\"SSID 1-32 Zeichen, Passwort max. 63 Zeichen\"}");
      return;
    }
    GatewaySettings s = gw_config.get();
    // Ohne Passwort im Backup nur bei gleicher SSID (altes Passwort bleibt).
    if (!req->hasParam("pass", true) && ssid != s.wifiSsid) {
      req->send(400, "application/json", "{\"error\":\"WLAN-Passwort fehlt im Backup\"}");
      return;
    }
    // Feste IP aus dem Backup (ab 0.9.0), wird nach dem Neustart erst geprüft
    if (req->hasParam("ip", true)) {
      String err;
      if (!GatewayConfig::applyIp(s, formParam(req, "ip"), formParam(req, "mask"), formParam(req, "gw"),
                                  formParam(req, "dns1"), formParam(req, "dns2"), err)) {
        req->send(400, "application/json", "{\"error\":\"Backup: " + jsonEscape(err) + "\"}");
        return;
      }
      if (s.staticIp()) s.ipPendingCheck = true;   // auch gleiche Adresse: Gerät kann woanders stehen
    }
    if (req->hasParam("pass", true)) s.wifiPass = pass;
    s.wifiSsid = ssid;
    // Backups bis 0.7.3 enthalten "meshcore-gw" -> automatisch (hostSetting)
    if (req->hasParam("hostname", true)) s.hostname = GatewayConfig::hostSetting(formParam(req, "hostname"));
    s.wifiPendingTest = true;
    gw_config.set(s);
    event_log::addf("SYS", "WLAN/Hostname aus Backup übernommen (%s)", ssid.c_str());
    s_rebootAt = millis() + 3000;   // Passwort-Job vorher noch abarbeiten lassen
  }
  if (pw.length() > 0) {
    String cmd = "password " + pw;
    if (!cli_bridge::submitOne(cmd.c_str())) {
      req->send(503, "application/json", "{\"error\":\"CLI ausgelastet\"}");
      return;
    }
  }
  req->send(200, "application/json", String("{\"ok\":true,\"reboot\":") + (req->hasParam("ssid", true) ? "true" : "false") + "}");
}

static void handleLogInfo(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  event_log::Info i = event_log::info();
  String j = "{\"fileBytes\":" + String(i.fileBytes) + ",\"pendingBytes\":" + String(i.pendingBytes) +
             ",\"maxBytes\":" + String(2 * event_log::FILE_MAX_BYTES) +
             ",\"lastFlushAgoS\":" + (i.lastFlushAgoS == 0xFFFFFFFFUL ? String("null") : String(i.lastFlushAgoS)) +
             ",\"flushIntervalS\":" + String(event_log::FLUSH_INTERVAL_MS / 1000) +
             ",\"flushes\":" + String(i.flushes) + ",\"dropped\":" + String(i.dropped) + "}";
  req->send(200, "application/json", j);
}

static void handleLogDownload(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  size_t len = 0;
  char* raw = event_log::snapshot(len);
  if (!raw) {
    req->send(503, "text/plain", "Kein Speicher frei");
    return;
  }
  // Puffer lebt so lange wie die Antwort (shared_ptr in der Callback-Lambda).
  std::shared_ptr<char> buf(raw, free);
  AsyncWebServerResponse* r = req->beginResponse("text/plain; charset=utf-8", len,
      [buf, len](uint8_t* out, size_t maxLen, size_t index) -> size_t {
        if (index >= len) return 0;
        size_t n = std::min(maxLen, len - index);
        memcpy(out, buf.get() + index, n);
        return n;
      });
  if (!req->hasParam("inline")) {
    String name = "meshcore-gw-log-" + network.hostname() + ".txt";
    r->addHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  }
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
}

static void handleLogClear(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  event_log::clear();
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleSmSave(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  String json = formParam(req, "json");
  String err;
  if (json.isEmpty() || !sm::setFromJson(json.c_str(), json.length(), err)) {
    if (err.isEmpty()) err = "keine Daten";
    req->send(400, "application/json", "{\"error\":\"" + jsonEscape(err) + "\"}");
    return;
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleSmPoll(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  if (!sm_poller::pollNow(formParam(req, "idx").toInt())) {
    req->send(404, "application/json", "{\"error\":\"Gerät unbekannt\"}");
    return;
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleSmTest(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  String err;
  if (!sm_poller::sendTest(formParam(req, "idx").toInt(), err)) {
    req->send(400, "application/json", "{\"error\":\"" + jsonEscape(err) + "\"}");
    return;
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleSmAck(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  String err;
  if (!sm_poller::ackFromWeb(formParam(req, "id").toInt(), err)) {
    req->send(400, "application/json", "{\"error\":\"" + jsonEscape(err) + "\"}");
    return;
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

// ---------- M5: Firmware-Update ----------

// Der Upload-Callback läuft VOR dem Request-Handler. Ohne eigene Prüfung
// könnte ein nicht angemeldeter Client in die OTA-Partition schreiben (E19).
static void handleOtaUpload(AsyncWebServerRequest* req, const String& filename, size_t index,
                            uint8_t* data, size_t len, bool final) {
  MeshSnapshot s = gateway_status::get();
  if (!s.valid || !req->authenticate(WEB_USER, s.adminPassword)) return;
  ota_update::onUpload(req, filename, index, data, len, final);
}

static void handleOtaDone(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  if (ota_update::onRequestDone(req)) {
    s_rebootAt = millis() + 2000;   // Antwort noch rausschicken
  }
}

static void handleReboot(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  event_log::add("SYS", "Neustart über die Weboberfläche angefordert");
  req->send(200, "application/json", "{\"ok\":true}");
  s_rebootAt = millis() + 1000;
}

void web_ui::begin() {
  // Alle Routen exakt matchen: ein String-URI matcht sonst auch Unterpfade
  // ("/api/wifi" -> "/api/wifi/scan"), siehe Memory zu ESPAsyncWebServer.
  server.on(AsyncURIMatcher::exact("/"), HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!checkAuth(req)) return;
    AsyncWebServerResponse* r = req->beginResponse(200, "text/html", (const uint8_t*)INDEX_HTML, strlen(INDEX_HTML));
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });
  server.on(AsyncURIMatcher::exact("/api/status"), HTTP_GET, handleStatus);
  server.on(AsyncURIMatcher::exact("/api/wifi/scan"), HTTP_GET, handleWifiScan);
  server.on(AsyncURIMatcher::exact("/api/wifi"), HTTP_POST, handleWifiSave);
  server.on(AsyncURIMatcher::exact("/api/wifi/checkip"), HTTP_POST, handleCheckIpStart);
  server.on(AsyncURIMatcher::exact("/api/wifi/checkip"), HTTP_GET, handleCheckIpResult);
  server.on(AsyncURIMatcher::exact("/api/reboot"), HTTP_POST, handleReboot);
  server.on(AsyncURIMatcher::exact("/api/cli"), HTTP_POST, handleCli);
  server.on(AsyncURIMatcher::exact("/api/cli/batch"), HTTP_POST, handleCliBatch);
  server.on(AsyncURIMatcher::exact("/api/cli/job"), HTTP_GET, handleCliJob);
  server.on(AsyncURIMatcher::exact("/api/profile"), HTTP_GET, handleProfile);
  server.on(AsyncURIMatcher::exact("/api/ota"), HTTP_POST, handleOtaDone, handleOtaUpload);
  server.on(AsyncURIMatcher::exact("/api/backup/extra"), HTTP_GET, handleBackupExtra);
  server.on(AsyncURIMatcher::exact("/api/backup/apply"), HTTP_POST, handleBackupApply);
  server.on(AsyncURIMatcher::exact("/api/log/info"), HTTP_GET, handleLogInfo);
  server.on(AsyncURIMatcher::exact("/api/log/download"), HTTP_GET, handleLogDownload);
  server.on(AsyncURIMatcher::exact("/api/log/clear"), HTTP_POST, handleLogClear);
  server.on(AsyncURIMatcher::exact("/api/sm"), HTTP_GET, handleSmGet);
  server.on(AsyncURIMatcher::exact("/api/sm/save"), HTTP_POST, handleSmSave);
  server.on(AsyncURIMatcher::exact("/api/sm/poll"), HTTP_POST, handleSmPoll);
  server.on(AsyncURIMatcher::exact("/api/sm/test"), HTTP_POST, handleSmTest);
  server.on(AsyncURIMatcher::exact("/api/sm/ack"), HTTP_POST, handleSmAck);
  server.on(AsyncURIMatcher::exact("/api/sm/log"), HTTP_GET, [](AsyncWebServerRequest* req) { sendLog(req, alert_log); });
  server.on(AsyncURIMatcher::exact("/api/channels"), HTTP_GET, handleChannelsGet);
  server.on(AsyncURIMatcher::exact("/api/channels"), HTTP_POST, handleChannelsPost);
  server.on(AsyncURIMatcher::exact("/api/channels/send"), HTTP_POST, handleChannelSend);
  server.on(AsyncURIMatcher::exact("/api/messages"), HTTP_GET, [](AsyncWebServerRequest* req) { sendLog(req, channel_log); });
  server.on(AsyncURIMatcher::exact("/api/console"), HTTP_GET, [](AsyncWebServerRequest* req) { sendLog(req, console_log); });
  server.on(AsyncURIMatcher::exact("/api/packets"), HTTP_GET, [](AsyncWebServerRequest* req) { sendLog(req, packet_log); });
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
