#include "SmPoller.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <new>

#include "ChannelStore.h"
#include "LogBuffer.h"
#include "MeshActions.h"
#include "NetworkManager.h"
#include "RuleEngine.h"
#include "SmConfig.h"
#include "SmJson.h"

using namespace sm;

namespace {

static const int MAX_VALUES = 12;
static const size_t MAX_BODY = 4096;

struct DevRuntime {
  rules::DeviceState dev;
  uint32_t lastPollMs = 0;
  uint32_t lastOkMs = 0;
  bool everPolled = false;
  bool everOk = false;
  char error[64] = {0};
  int nValues = 0;
  SmValue values[MAX_VALUES];
};

struct RuleRuntime {
  rules::RuleState st;
  uint32_t lastEventMs = 0;
  char lastEvent[12] = {0};
};

struct Runtime {
  Config cfg;
  uint32_t gen = 0;
  DevRuntime dev[MAX_DEVICES];
  RuleRuntime rule[MAX_RULES];
  rules::RateLimiter limiter;
  volatile bool pollFlag[MAX_DEVICES];
};

Runtime* R = nullptr;              // PSRAM
SemaphoreHandle_t s_lock = nullptr;   // schützt R (außer der HTTP-Abfrage selbst)

void logLine(char type, const char* text) {
  alert_log.push(type, (uint32_t)time(nullptr), text);
}

// Meldung erzeugen und (begrenzt) senden. Aufrufer hält s_lock.
void emit(const char* channelName, const char* text, bool countLimit) {
  int channel = channel_store::findByName(channelName);
  if (!channelName || !*channelName) {
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "kein Kanal gewählt: %s", text);
    logLine('X', l);
    return;
  }
  GwChannel c;
  if (!channel_store::get(channel, c)) {
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "Kanal \"%s\" existiert nicht: %s", channelName, text);
    logLine('X', l);
    return;
  }
  if (countLimit && !R->limiter.allow(millis())) {
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "Sendelimit (%d/h) erreicht, unterdrückt: %s", R->cfg.settings.maxPerHour, text);
    logLine('X', l);
    return;
  }
  if (!mesh_actions::sendChannel(channel, text, mesh_actions::Origin::ALERT)) {
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "Warteschlange voll, verworfen: %s", text);
    logLine('X', l);
    return;
  }
  char l[LogBuffer::LINE_LEN];
  snprintf(l, sizeof(l), "%s -> %s", c.name, text);
  logLine('A', l);
}

const char* tplOr(const char* t, const char* def) { return (t && *t) ? t : def; }

void ruleText(const Rule& r, const Device& d, const RuleRuntime& rt, const char* state, const char* tpl,
              char* out, size_t outLen) {
  rules::TemplateVars v{d.name, rules::labelFor(r.path), rt.st.lastValue, rt.st.hasValue,
                        rules::unitFor(r.path), r.op ? rules::Op::BELOW : rules::Op::ABOVE, r.threshold, state};
  rules::renderTemplate(tpl, v, out, outLen);
}

// Gleiches Gerät = gleiche Adresse. Gleiche Regel = gleiches Gerät + gleiche
// Bedingung. Nur dann bleibt der Laufzeitzustand beim Speichern erhalten -
// sonst würde ein anstehender Alarm bei JEDER Konfigurationsänderung (z. B.
// nur Standardkanal geändert) erneut gesendet (E22).
bool sameDevice(const Device& a, const Device& b) {
  return a.used && b.used && !strcmp(a.host, b.host) && a.port == b.port;
}
bool sameRule(const Rule& a, const Device& da, const Rule& b, const Device& db) {
  return a.used && b.used && sameDevice(da, db) && !strcmp(a.path, b.path) && a.op == b.op &&
         a.threshold == b.threshold && a.hysteresis == b.hysteresis && a.minDurationS == b.minDurationS;
}

// Konfiguration übernehmen; Zustände unveränderter Geräte/Regeln behalten.
void syncConfig() {
  if (sm::generation() == R->gen) return;
  static Config oldCfg;            // statisch: ca. 5 KB, nicht auf den Task-Stack
  static DevRuntime oldDev[MAX_DEVICES];
  static RuleRuntime oldRule[MAX_RULES];
  oldCfg = R->cfg;
  memcpy(oldDev, R->dev, sizeof(oldDev));
  memcpy(oldRule, R->rule, sizeof(oldRule));

  R->gen = sm::copy(R->cfg);
  int keptDev = 0, keptRule = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    R->dev[i] = DevRuntime();
    R->pollFlag[i] = true;   // gleich neu abfragen
    for (int j = 0; j < MAX_DEVICES; j++) {
      if (sameDevice(R->cfg.devices[i], oldCfg.devices[j])) { R->dev[i] = oldDev[j]; keptDev++; break; }
    }
  }
  for (int i = 0; i < MAX_RULES; i++) {
    R->rule[i] = RuleRuntime();
    const Rule& r = R->cfg.rules[i];
    if (!r.used) continue;
    for (int j = 0; j < MAX_RULES; j++) {
      const Rule& o = oldCfg.rules[j];
      if (o.used && sameRule(r, R->cfg.devices[r.device], o, oldCfg.devices[o.device])) {
        R->rule[i] = oldRule[j];
        keptRule++;
        break;
      }
    }
  }
  R->limiter.setMaxPerHour(R->cfg.settings.maxPerHour);
  char l[96];
  snprintf(l, sizeof(l), "Konfiguration übernommen (%d Geräte, %d Regeln mit Zustand behalten)", keptDev, keptRule);
  logLine('S', l);
}

// HTTP-Abfrage ohne gehaltenen Mutex. Rückgabe: Anzahl Werte oder -1.
int fetch(const Device& d, SmValue* vals, char* err, size_t errLen) {
  if (network.mode() != NetworkManager::Mode::STATION) {
    snprintf(err, errLen, "kein WLAN");
    return -1;
  }
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  http.setReuse(false);
  String url = String("http://") + d.host + ":" + d.port + "/api/sensors";
  if (!http.begin(client, url)) {
    snprintf(err, errLen, "URL ungültig");
    return -1;
  }
  if (d.user[0]) http.setAuthorization(d.user, d.pass);
  int code = http.GET();
  if (code != 200) {
    if (code < 0) snprintf(err, errLen, "%s", HTTPClient::errorToString(code).c_str());
    else snprintf(err, errLen, "HTTP %d", code);
    http.end();
    return -1;
  }
  int size = http.getSize();
  if (size > (int)MAX_BODY) {
    snprintf(err, errLen, "Antwort zu groß (%d B)", size);
    http.end();
    return -1;
  }
  String body = http.getString();
  http.end();
  if (body.length() > MAX_BODY) {
    snprintf(err, errLen, "Antwort zu groß");
    return -1;
  }
  int n = smFlatten(body.c_str(), body.length(), vals, MAX_VALUES, err, errLen);
  if (n == 0) snprintf(err, errLen, "keine gültigen Messwerte");
  return n;
}

void evaluateDevice(int di, bool ok, uint32_t now) {
  // Aufrufer hält s_lock
  const Device& d = R->cfg.devices[di];
  DevRuntime& dr = R->dev[di];
  rules::DevEvent de = rules::deviceResult(dr.dev, ok, R->cfg.settings.offlineAfterFails, now);
  if (de != rules::DevEvent::NONE && d.offlineAlert) {
    char text[mesh_actions::MAX_TEXT + 1];
    rules::TemplateVars v{d.name, "", 0, false, "", rules::Op::ABOVE, 0,
                          de == rules::DevEvent::OFFLINE ? "OFFLINE" : "ONLINE"};
    const char* tpl = de == rules::DevEvent::OFFLINE ? tplOr(R->cfg.settings.offlineText, DEFAULT_OFFLINE)
                                                     : tplOr(R->cfg.settings.onlineText, DEFAULT_ONLINE);
    rules::renderTemplate(tpl, v, text, sizeof(text));
    emit(R->cfg.settings.defaultChannel, text, true);
  }
  if (!ok) return;

  for (int ri = 0; ri < MAX_RULES; ri++) {
    const Rule& r = R->cfg.rules[ri];
    if (!r.used || !r.enabled || r.device != di) continue;
    float value = NAN;
    bool valid = false;
    for (int k = 0; k < dr.nValues; k++) {
      if (!strcmp(dr.values[k].path, r.path)) { value = dr.values[k].value; valid = true; break; }
    }
    rules::RuleParams p{r.op ? rules::Op::BELOW : rules::Op::ABOVE, r.threshold, r.hysteresis,
                        r.minDurationS, r.repeatMin, r.notifyRecovery};
    RuleRuntime& rt = R->rule[ri];
    rules::Event e = rules::evaluate(p, rt.st, value, valid, now);
    if (e == rules::Event::NONE) continue;
    rt.lastEventMs = now;
    const char* state = e == rules::Event::ALERT ? "ALARM" : (e == rules::Event::REMIND ? "ERINNERUNG" : "OK");
    strlcpy(rt.lastEvent, state, sizeof(rt.lastEvent));
    const char* tpl = e == rules::Event::RECOVER ? tplOr(R->cfg.settings.recoverText, DEFAULT_RECOVER)
                                                 : tplOr(r.text, tplOr(R->cfg.settings.alertText, DEFAULT_ALERT));
    char text[mesh_actions::MAX_TEXT + 1];
    ruleText(r, d, rt, state, tpl, text, sizeof(text));
    emit(r.channel[0] ? r.channel : R->cfg.settings.defaultChannel, text, true);
  }
}

void task(void*) {
  static SmValue vals[MAX_VALUES];
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    xSemaphoreTake(s_lock, portMAX_DELAY);
    syncConfig();
    bool enabled = R->cfg.settings.enabled;
    xSemaphoreGive(s_lock);
    if (!enabled) continue;
    // Ohne eigenes WLAN gar nicht abfragen: sonst würde jedes Sensormeter als
    // "nicht erreichbar" gezählt und per Funk (geht ja weiter) gemeldet,
    // obwohl das Gateway selbst das Problem ist.
    if (network.mode() != NetworkManager::Mode::STATION) continue;

    for (int di = 0; di < MAX_DEVICES; di++) {
      xSemaphoreTake(s_lock, portMAX_DELAY);
      Device d = R->cfg.devices[di];
      uint32_t now = millis();
      bool due = d.used && d.enabled &&
                 (R->pollFlag[di] || !R->dev[di].everPolled || now - R->dev[di].lastPollMs >= d.intervalS * 1000UL);
      uint32_t gen = R->gen;
      xSemaphoreGive(s_lock);
      if (!due) continue;

      char err[64] = {0};
      int n = fetch(d, vals, err, sizeof(err));

      xSemaphoreTake(s_lock, portMAX_DELAY);
      if (R->gen == gen) {   // Konfig nicht zwischendurch geändert
        DevRuntime& dr = R->dev[di];
        now = millis();
        R->pollFlag[di] = false;
        dr.everPolled = true;
        dr.lastPollMs = now;
        bool ok = n > 0;
        if (ok) {
          dr.nValues = n;
          memcpy(dr.values, vals, sizeof(SmValue) * n);
          dr.lastOkMs = now;
          dr.everOk = true;
          dr.error[0] = 0;
        } else {
          strlcpy(dr.error, err, sizeof(dr.error));
        }
        evaluateDevice(di, ok, now);
      }
      xSemaphoreGive(s_lock);
    }
  }
}

}  // namespace

void sm_poller::begin() {
  if (R) return;
  s_lock = xSemaphoreCreateMutex();
  void* mem = heap_caps_calloc(1, sizeof(Runtime), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!mem) mem = calloc(1, sizeof(Runtime));
  R = new (mem) Runtime();
  // Core 0 (WLAN-Stack), niedrige Priorität. 8 KB Stack: HTTPClient + ArduinoJson.
  xTaskCreatePinnedToCore(task, "smpoll", 8192, nullptr, 1, nullptr, 0);
}

bool sm_poller::pollNow(int deviceIdx) {
  if (!R || deviceIdx < 0 || deviceIdx >= MAX_DEVICES) return false;
  R->pollFlag[deviceIdx] = true;
  return true;
}

bool sm_poller::sendTest(int ruleIdx, String& err) {
  if (!R || ruleIdx < 0 || ruleIdx >= MAX_RULES) { err = "Regel unbekannt"; return false; }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  bool ok = false;
  const Rule& r = R->cfg.rules[ruleIdx];
  if (!r.used) {
    err = "Regel unbekannt (erst speichern)";
  } else {
    const Device& d = R->cfg.devices[r.device];
    RuleRuntime& rt = R->rule[ruleIdx];
    const char* tpl = tplOr(r.text, tplOr(R->cfg.settings.alertText, DEFAULT_ALERT));
    char body[mesh_actions::MAX_TEXT - 6];
    ruleText(r, d, rt, "TEST", tpl, body, sizeof(body));
    // Ohne {state} in der Vorlage wäre ein Test nicht von einem echten Alarm
    // zu unterscheiden -> dann "TEST " voranstellen.
    char text[mesh_actions::MAX_TEXT + 1];
    snprintf(text, sizeof(text), "%s%s", strstr(tpl, "{state}") ? "" : "TEST ", body);
    const char* ch = r.channel[0] ? r.channel : R->cfg.settings.defaultChannel;
    if (channel_store::findByName(ch) < 0) {
      err = "kein gültiger Kanal";
    } else {
      emit(ch, text, false);
      ok = true;
    }
  }
  xSemaphoreGive(s_lock);
  return ok;
}

static void esc(String& out, const char* s) {
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { out += '\\'; out += *s; }
    else if ((uint8_t)*s < 0x20) out += ' ';
    else out += *s;
  }
}

void sm_poller::statusJson(String& out) {
  out += "{\"devices\":[";
  if (!R) { out += "],\"rules\":[]}"; return; }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  uint32_t now = millis();
  bool first = true;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!R->cfg.devices[i].used) continue;
    const DevRuntime& d = R->dev[i];
    if (!first) out += ',';
    first = false;
    out += "{\"idx\":" + String(i);
    out += ",\"polled\":" + String(d.everPolled ? "true" : "false");
    out += ",\"online\":" + String(d.everOk && !d.dev.offline && d.dev.fails == 0 ? "true" : "false");
    out += ",\"offline\":" + String(d.dev.offline ? "true" : "false");
    out += ",\"fails\":" + String(d.dev.fails);
    out += ",\"lastOkAgoS\":" + (d.everOk ? String((now - d.lastOkMs) / 1000) : String("null"));
    out += ",\"error\":\"";
    esc(out, d.error);
    out += "\",\"values\":[";
    for (int k = 0; k < d.nValues; k++) {
      if (k) out += ',';
      out += "{\"path\":\"";
      esc(out, d.values[k].path);
      out += "\",\"sensor\":\"";
      esc(out, d.values[k].sensorName);
      out += "\",\"label\":\"";
      esc(out, rules::labelFor(d.values[k].path));
      out += "\",\"unit\":\"";
      esc(out, rules::unitFor(d.values[k].path));
      out += "\",\"value\":" + String(d.values[k].value, 2) + "}";
    }
    out += "]}";
  }
  out += "],\"rules\":[";
  first = true;
  for (int i = 0; i < MAX_RULES; i++) {
    if (!R->cfg.rules[i].used) continue;
    const RuleRuntime& r = R->rule[i];
    if (!first) out += ',';
    first = false;
    const char* st = r.st.state == rules::State::ALERT ? "alert" : (r.st.state == rules::State::PENDING ? "pending" : "normal");
    out += "{\"idx\":" + String(i) + ",\"state\":\"" + st + "\"";
    out += ",\"value\":" + (r.st.hasValue ? String(r.st.lastValue, 2) : String("null"));
    out += ",\"lastEvent\":\"" + String(r.lastEvent) + "\"";
    out += ",\"lastEventAgoS\":" + (r.lastEventMs ? String((now - r.lastEventMs) / 1000) : String("null")) + "}";
  }
  out += "],\"limit\":{\"used\":" + String(R->limiter.usedInWindow(now)) + ",\"max\":" + String(R->cfg.settings.maxPerHour) + "}}";
  xSemaphoreGive(s_lock);
}
