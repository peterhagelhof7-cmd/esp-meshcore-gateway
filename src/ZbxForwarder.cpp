#include "ZbxForwarder.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "EventLog.h"
#include "GatewayStatus.h"
#include "LogBuffer.h"
#include "MeshActions.h"
#include "NetworkManager.h"
#include "ZbxFormat.h"

namespace {

struct Config {
  bool enabled = false;
  char server[64] = {0};      // Proxy oder Server (IP/Name)
  uint16_t port = 10051;
  char rxHost[64] = {0};      // eigener Zabbix-Host (Lebenszeichen), leer = eigener Hostname
  char hostPrefix[24] = {0};  // vor den Gateway-Namen
  char channels[96] = {0};    // Kanalnamen mit Komma, leer = alle
  bool autoAck = true;
  bool forwardUnmarked = false;
  uint8_t heartbeatMin = 5;   // 0 = aus
};

static const size_t TEXT_MAX = 200;   // > MAX_PACKET_PAYLOAD (184)

struct Incoming {
  int8_t channelIdx;
  char channel[32];
  char text[TEXT_MAX + 1];
  float snr;
  uint8_t hops;
  uint32_t rxMs;
};

static const int MAX_PENDING = 16;
static const uint32_t RETRY_MS = 30000;
static const uint32_t GIVE_UP_MS = 3600000UL;

struct Pending {
  bool used = false;
  int8_t channelIdx = -1;
  char sender[32] = {0};
  uint8_t ack = 0;
  char host[96] = {0};
  char key[32] = {0};
  char value[512] = {0};
  uint32_t firstMs = 0, nextMs = 0;
  uint8_t tries = 0;
};

// Zuletzt bestätigte ACKs: Wiederholung desselben Alarms nicht nochmal an
// Zabbix, nur erneut bestätigen (höchstens alle 60 s).
struct Done {
  char sender[32];
  uint8_t ack;
  uint32_t ms;
  uint32_t lastAckMs;
};

Config s_cfg;
SemaphoreHandle_t s_lock = nullptr;
QueueHandle_t s_in = nullptr;
Pending* s_pend = nullptr;   // PSRAM, ca. 11 KB
Done s_done[16];
volatile bool s_testReq = false;

struct Stats {
  uint32_t received = 0, forwarded = 0, rejected = 0, errors = 0, acks = 0, dropped = 0;
  uint32_t lastOkMs = 0;
  char lastError[96] = {0};
  char lastTest[96] = {0};
  uint32_t hbOkMs = 0;
} s_st;

void logLine(char type, const char* text) { alert_log.push(type, (uint32_t)time(nullptr), text); }

void setError(const char* e) {
  strlcpy(s_st.lastError, e, sizeof(s_st.lastError));
  char l[LogBuffer::LINE_LEN];
  snprintf(l, sizeof(l), "Zabbix: %s", e);
  logLine('X', l);
}

bool channelAllowed(const Config& c, const char* name) {
  if (!c.channels[0]) return true;
  const char* p = c.channels;
  size_t n = strlen(name);
  while (*p) {
    while (*p == ' ' || *p == ',') p++;
    const char* e = p;
    while (*e && *e != ',') e++;
    const char* t = e;
    while (t > p && t[-1] == ' ') t--;
    if ((size_t)(t - p) == n && !strncmp(p, name, n)) return true;
    p = e;
  }
  return false;
}

// Ein Wert an Zabbix. Rückgabe: 1 = angenommen, 0 = abgelehnt (failed),
// -1 = Netzwerk/Protokollfehler (später erneut). err bekommt den Grund.
int sendOne(const Config& c, const char* host, const char* key, const char* value, char* err, size_t errLen) {
  if (network.mode() != NetworkManager::Mode::STATION) { snprintf(err, errLen, "kein WLAN"); return -1; }
  if (!c.server[0]) { snprintf(err, errLen, "kein Zabbix-Server eingetragen"); return -1; }
  static char req[1400];
  char eh[200], ek[64], ev[1100];
  zbx::jsonEscape(host, eh, sizeof(eh));
  zbx::jsonEscape(key, ek, sizeof(ek));
  zbx::jsonEscape(value, ev, sizeof(ev));
  int n = snprintf(req, sizeof(req), "{\"request\":\"sender data\",\"data\":[{\"host\":\"%s\",\"key\":\"%s\",\"value\":\"%s\"}]}",
                   eh, ek, ev);
  if (n <= 0 || n >= (int)sizeof(req)) { snprintf(err, errLen, "Wert zu lang"); return 0; }

  WiFiClient cl;
  if (!cl.connect(c.server, c.port, 3000)) {
    snprintf(err, errLen, "keine Verbindung zu %s:%u", c.server, (unsigned)c.port);
    return -1;
  }
  uint8_t hdr[zbx::HEADER_LEN];
  zbx::writeHeader(hdr, (uint32_t)n);
  cl.write(hdr, sizeof(hdr));
  cl.write((const uint8_t*)req, n);

  uint8_t rh[zbx::HEADER_LEN];
  uint32_t t0 = millis();
  size_t got = 0;
  while (got < sizeof(rh) && millis() - t0 < 5000) {
    int r = cl.read(rh + got, sizeof(rh) - got);
    if (r > 0) got += r;
    else if (!cl.connected()) break;
    else delay(10);
  }
  long len = got == sizeof(rh) ? zbx::readHeader(rh, got) : -1;
  if (len < 0 || len > 1024) {
    cl.stop();
    snprintf(err, errLen, "keine gültige Zabbix-Antwort (Port richtig? Trapper aktiv?)");
    return -1;
  }
  char resp[1025];
  got = 0;
  while (got < (size_t)len && millis() - t0 < 5000) {
    int r = cl.read((uint8_t*)resp + got, len - got);
    if (r > 0) got += r;
    else if (!cl.connected()) break;
    else delay(10);
  }
  resp[got] = 0;
  cl.stop();
  int processed, failed;
  if (!zbx::parseResponse(resp, processed, failed)) {
    snprintf(err, errLen, "Zabbix-Antwort: %.60s", resp);
    return -1;
  }
  if (processed >= 1) { err[0] = 0; return 1; }
  snprintf(err, errLen, "abgelehnt (Host \"%s\" / Item %s fehlt, nicht Trapper oder Allowed hosts?)", host, key);
  return 0;
}

String ownRxHost(const Config& c) {
  return c.rxHost[0] ? String(c.rxHost) : network.hostname();
}

// Eingang in die Warteschlange übernehmen. Aufrufer hält s_lock.
void accept(const Config& c, const Incoming& in) {
  // "Absender: Text"
  const char* sep = strstr(in.text, ": ");
  if (!sep) return;
  char sender[32];
  size_t sl = (size_t)(sep - in.text);
  if (sl >= sizeof(sender)) sl = sizeof(sender) - 1;
  memcpy(sender, in.text, sl);
  sender[sl] = 0;
  const char* body = sep + 2;

  zbx::Marker m;
  size_t textLen = 0;
  bool marked = zbx::parseMarker(body, m, textLen);
  if (!marked && !c.forwardUnmarked) return;
  s_st.received++;
  uint32_t now = millis();

  if (marked && m.ack) {
    for (Done& d : s_done) {
      if (d.ms && d.ack == m.ack && !strcmp(d.sender, sender) && now - d.ms < 24UL * 3600000UL) {
        // Wiederholung eines schon an Zabbix gemeldeten Alarms: nur erneut bestätigen
        if (c.autoAck && now - d.lastAckMs > 60000) {
          char a[64];
          snprintf(a, sizeof(a), "@[%s] ACK %u", sender, (unsigned)m.ack);
          if (mesh_actions::sendChannel(in.channelIdx, a, mesh_actions::Origin::ZBX)) {
            d.lastAckMs = now;
            s_st.acks++;
          }
        }
        return;
      }
    }
  }

  int slot = -1;
  for (int i = 0; i < MAX_PENDING; i++) if (!s_pend[i].used) { slot = i; break; }
  if (slot < 0) { s_st.dropped++; setError("Warteschlange voll, Meldung verworfen"); return; }
  Pending& p = s_pend[slot];
  p = Pending();
  p.used = true;
  p.channelIdx = in.channelIdx;
  strlcpy(p.sender, sender, sizeof(p.sender));
  p.firstMs = p.nextMs = now;

  char text[TEXT_MAX + 1];
  size_t tl = marked ? textLen : strlen(body);
  if (tl >= sizeof(text)) tl = sizeof(text) - 1;
  memcpy(text, body, tl);
  text[tl] = 0;
  MeshSnapshot snap = gateway_status::get();

  JsonDocument doc;
  if (marked) {
    p.ack = c.autoAck ? m.ack : 0;
    String host = String(c.hostPrefix) + NetworkManager::sanitizeHostname(sender);
    strlcpy(p.host, host.c_str(), sizeof(p.host));
    strlcpy(p.key, "meshcore.event", sizeof(p.key));
    doc["src"] = m.src;
    char ev[2] = {m.ev, 0};
    doc["ev"] = ev;
    doc["state"] = zbx::stateOf(m.ev);
    if (m.hasValue) doc["value"] = m.value;
    if (m.ack) doc["ack"] = m.ack;
  } else {
    strlcpy(p.host, ownRxHost(c).c_str(), sizeof(p.host));
    strlcpy(p.key, "meshcore.rx.unparsed", sizeof(p.key));
  }
  doc["text"] = text;
  doc["gw"] = sender;
  doc["channel"] = in.channel;
  doc["snr"] = in.snr;
  doc["hops"] = in.hops;
  doc["rx"] = snap.nodeName;
  String v;
  serializeJson(doc, v);
  strlcpy(p.value, v.c_str(), sizeof(p.value));
}

void process(const Config& c) {
  uint32_t now = millis();
  for (int i = 0; i < MAX_PENDING; i++) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool due = s_pend[i].used && (int32_t)(now - s_pend[i].nextMs) >= 0;
    Pending p;
    if (due) p = s_pend[i];
    xSemaphoreGive(s_lock);
    if (!due) continue;

    char err[96];
    int r = sendOne(c, p.host, p.key, p.value, err, sizeof(err));
    xSemaphoreTake(s_lock, portMAX_DELAY);
    Pending& q = s_pend[i];
    if (r == 1) {
      s_st.forwarded++;
      s_st.lastOkMs = millis();
      s_st.lastError[0] = 0;
      char l[LogBuffer::LINE_LEN];
      snprintf(l, sizeof(l), "Zabbix <- %s %s: %.80s", q.host, q.key, q.value);
      logLine('A', l);
      if (q.ack) {
        char a[64];
        snprintf(a, sizeof(a), "@[%s] ACK %u", q.sender, (unsigned)q.ack);
        if (mesh_actions::sendChannel(q.channelIdx, a, mesh_actions::Origin::ZBX)) s_st.acks++;
        int slot = 0;
        for (int k = 1; k < 16; k++) if (s_done[k].ms < s_done[slot].ms) slot = k;
        strlcpy(s_done[slot].sender, q.sender, sizeof(s_done[slot].sender));
        s_done[slot].ack = q.ack;
        s_done[slot].ms = s_done[slot].lastAckMs = millis();
      }
      q.used = false;
    } else if (r == 0) {
      s_st.rejected++;
      setError(err);
      event_log::addf("ZBX", "Zabbix lehnt ab: %s", err);
      q.used = false;   // dauerhaft: nicht wiederholen, kein ACK
    } else {
      s_st.errors++;
      setError(err);
      q.tries++;
      if (millis() - q.firstMs > GIVE_UP_MS) {
        s_st.dropped++;
        event_log::addf("ZBX", "Meldung nach 1 h verworfen (%s)", err);
        q.used = false;
      } else {
        q.nextMs = millis() + RETRY_MS;
      }
    }
    xSemaphoreGive(s_lock);
    if (r == -1) break;   // Zabbix nicht erreichbar: die übrigen später
  }
}

void heartbeat(const Config& c, bool force) {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (!force && (!c.heartbeatMin || (lastMs && now - lastMs < c.heartbeatMin * 60000UL))) return;
  lastMs = now;
  char err[96], val[16];
  snprintf(val, sizeof(val), "%lu", (unsigned long)(now / 1000));
  String host = ownRxHost(c);
  int r = sendOne(c, host.c_str(), "meshcore.rx.heartbeat", val, err, sizeof(err));
  xSemaphoreTake(s_lock, portMAX_DELAY);
  if (r == 1) {
    s_st.hbOkMs = millis();
    if (force) snprintf(s_st.lastTest, sizeof(s_st.lastTest), "OK: %s / meshcore.rx.heartbeat angenommen", host.c_str());
  } else {
    if (force) snprintf(s_st.lastTest, sizeof(s_st.lastTest), "Fehler: %s", err);
    setError(err);
  }
  xSemaphoreGive(s_lock);
}

void task(void*) {
  Incoming in;
  for (;;) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    Config c = s_cfg;
    xSemaphoreGive(s_lock);
    while (xQueueReceive(s_in, &in, c.enabled ? 0 : pdMS_TO_TICKS(500)) == pdTRUE) {
      if (!c.enabled) continue;
      xSemaphoreTake(s_lock, portMAX_DELAY);
      if (channelAllowed(c, in.channel)) accept(c, in);
      xSemaphoreGive(s_lock);
    }
    if (c.enabled) {
      bool test = s_testReq;
      s_testReq = false;
      if (network.mode() == NetworkManager::Mode::STATION) {
        process(c);
        heartbeat(c, test);
      } else if (test) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        strlcpy(s_st.lastTest, "Fehler: kein WLAN", sizeof(s_st.lastTest));
        xSemaphoreGive(s_lock);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

bool parse(const char* json, size_t len, Config& c, String& err) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json, len);
  if (e) { err = String("JSON: ") + e.c_str(); return false; }
  c = Config();
  c.enabled = doc["enabled"] | false;
  strlcpy(c.server, doc["server"] | "", sizeof(c.server));
  c.port = doc["port"] | 10051;
  strlcpy(c.rxHost, doc["rxHost"] | "", sizeof(c.rxHost));
  strlcpy(c.hostPrefix, doc["hostPrefix"] | "", sizeof(c.hostPrefix));
  strlcpy(c.channels, doc["channels"] | "", sizeof(c.channels));
  c.autoAck = doc["autoAck"] | true;
  c.forwardUnmarked = doc["forwardUnmarked"] | false;
  c.heartbeatMin = constrain((int)(doc["heartbeatMin"] | 5), 0, 120);
  for (const char* p = c.server; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '.' || *p == '-' || *p == '_')) { err = "Zabbix-Server: IP oder Hostname"; return false; }
  }
  if (c.enabled && !c.server[0]) { err = "Zabbix-Server fehlt"; return false; }
  if (c.port == 0) { err = "Port ungültig"; return false; }
  return true;
}

void serialize(const Config& c, String& out) {
  JsonDocument doc;
  doc["enabled"] = c.enabled;
  doc["server"] = c.server;
  doc["port"] = c.port;
  doc["rxHost"] = c.rxHost;
  doc["hostPrefix"] = c.hostPrefix;
  doc["channels"] = c.channels;
  doc["autoAck"] = c.autoAck;
  doc["forwardUnmarked"] = c.forwardUnmarked;
  doc["heartbeatMin"] = c.heartbeatMin;
  String tmp;   // ArduinoJson 7 überschreibt das Ziel (siehe SmConfig)
  serializeJson(doc, tmp);
  out += tmp;
}

}  // namespace

void zbx_fwd::begin() {
  if (s_lock) return;
  s_lock = xSemaphoreCreateMutex();
  s_in = xQueueCreate(6, sizeof(Incoming));
  void* mem = heap_caps_calloc(MAX_PENDING, sizeof(Pending), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!mem) mem = calloc(MAX_PENDING, sizeof(Pending));
  s_pend = (Pending*)mem;
  memset(s_done, 0, sizeof(s_done));

  Preferences p;
  p.begin("gw", true);
  String j = p.getString("zbxcfg", "");
  p.end();
  if (j.length()) {
    String err;
    Config c;
    if (parse(j.c_str(), j.length(), c, err)) s_cfg = c;
  }
  xTaskCreatePinnedToCore(task, "zbxfwd", 8192, nullptr, 1, nullptr, 0);
}

void zbx_fwd::onChannelText(int channelIdx, const char* channelName, const char* text, float snr, uint8_t hops) {
  if (!s_in || !s_cfg.enabled) return;   // enabled ohne Lock: nur ein Vorfilter
  Incoming in;
  in.channelIdx = (int8_t)channelIdx;
  strlcpy(in.channel, channelName ? channelName : "", sizeof(in.channel));
  strlcpy(in.text, text ? text : "", sizeof(in.text));
  in.snr = snr;
  in.hops = hops;
  in.rxMs = millis();
  xQueueSend(s_in, &in, 0);
}

void zbx_fwd::configJson(String& out, bool) {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  Config c = s_cfg;
  xSemaphoreGive(s_lock);
  serialize(c, out);
}

bool zbx_fwd::setFromJson(const char* json, size_t len, String& err) {
  Config c;
  if (!parse(json, len, c, err)) return false;
  String stored;
  serialize(c, stored);
  Preferences p;
  p.begin("gw", false);
  size_t w = p.putString("zbxcfg", stored);
  p.end();
  if (w != stored.length()) { err = "Speichern in NVS fehlgeschlagen"; return false; }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  s_cfg = c;
  xSemaphoreGive(s_lock);
  event_log::addf("ZBX", "Zabbix-Weiterleitung %s (%s:%u)", c.enabled ? "an" : "aus", c.server, (unsigned)c.port);
  return true;
}

void zbx_fwd::requestTest() { s_testReq = true; }

void zbx_fwd::statusJson(String& out) {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  uint32_t now = millis();
  int pending = 0;
  for (int i = 0; i < MAX_PENDING; i++) pending += s_pend[i].used;
  char buf[512];
  char le[200], lt[200];
  zbx::jsonEscape(s_st.lastError, le, sizeof(le));
  zbx::jsonEscape(s_st.lastTest, lt, sizeof(lt));
  snprintf(buf, sizeof(buf),
           "{\"received\":%lu,\"forwarded\":%lu,\"rejected\":%lu,\"errors\":%lu,\"acks\":%lu,\"dropped\":%lu,"
           "\"pending\":%d,\"lastOkAgoS\":%s,\"heartbeatAgoS\":%s,\"lastError\":\"%s\",\"lastTest\":\"%s\",\"rxHost\":\"",
           (unsigned long)s_st.received, (unsigned long)s_st.forwarded, (unsigned long)s_st.rejected,
           (unsigned long)s_st.errors, (unsigned long)s_st.acks, (unsigned long)s_st.dropped, pending,
           s_st.lastOkMs ? String((now - s_st.lastOkMs) / 1000).c_str() : "null",
           s_st.hbOkMs ? String((now - s_st.hbOkMs) / 1000).c_str() : "null", le, lt);
  Config c = s_cfg;
  xSemaphoreGive(s_lock);
  out += buf;
  char rh[140];
  zbx::jsonEscape(ownRxHost(c).c_str(), rh, sizeof(rh));
  out += rh;
  out += "\"}";
}
