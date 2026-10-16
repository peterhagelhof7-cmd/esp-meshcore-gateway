#include "SmPoller.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <new>

#include "AckTracker.h"
#include "GatewayStatus.h"
#include "ChannelStore.h"
#include "EventLog.h"
#include "LogBuffer.h"
#include "MeshActions.h"
#include "NetCheck.h"
#include "NetworkManager.h"
#include "RemoteAdmin.h"
#include "RuleEngine.h"
#include "SmConfig.h"
#include "SmJson.h"
#include "ZbxFormat.h"

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
  uint32_t jobId = 0;     // entfernter Repeater: laufende Abfrage (E42)
  char route[24] = {0};   // entfernter Repeater: "Flood" / "direkt, 2 Hops"
};

struct RuleRuntime {
  rules::RuleState st;
  uint32_t lastEventMs = 0;
  char lastEvent[12] = {0};
};

struct PingRuntime {
  rules::PingState st;
  uint32_t lastCheckMs = 0;
  bool checked = false;
  bool lastOk = false;
  uint32_t rttMs = 0;
  char ip[16] = {0};
  char error[48] = {0};
};

struct Runtime {
  Config cfg;
  uint32_t gen = 0;
  DevRuntime dev[MAX_DEVICES];
  DevRuntime internal;            // Pseudo-Gerät INTERNAL_DEVICE (E30)
  RuleRuntime rule[MAX_RULES];
  PingRuntime ping[MAX_PINGS];
  rules::RateLimiter limiter;
  ack::Tracker acks;               // Bestätigungspflicht (E39)
  volatile bool pollFlag[MAX_DEVICES];
};

// Kanalantwort "ACK [Nr]" aus dem loopTask an den Poller-Task (E39)
struct AckMsg {
  char channel[ack::CHANNEL_LEN];
  char sender[32];
  char body[40];
};

Runtime* R = nullptr;              // PSRAM
SemaphoreHandle_t s_lock = nullptr;   // schützt R (außer der HTTP-Abfrage selbst)
QueueHandle_t s_ackQueue = nullptr;
volatile float s_chipTemp = NAN;      // letzte Messung, auch ohne Überwachung

DevRuntime& runtimeOf(int di) { return di == INTERNAL_DEVICE ? R->internal : R->dev[di]; }

void logLine(char type, const char* text) {
  alert_log.push(type, (uint32_t)time(nullptr), text);
}

// Meldung erzeugen und (begrenzt) senden. Aufrufer hält s_lock. true = in die
// Sende-Warteschlange gestellt.
bool emit(const char* channelName, const char* text, bool countLimit) {
  int channel = channel_store::findByName(channelName);
  if (!channelName || !*channelName) {
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "kein Kanal gewählt: %s", text);
    logLine('X', l);
    return false;
  }
  GwChannel c;
  if (!channel_store::get(channel, c)) {
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "Kanal \"%s\" existiert nicht: %s", channelName, text);
    logLine('X', l);
    return false;
  }
  if (countLimit && !R->limiter.allow(millis())) {
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "Sendelimit (%d/h) erreicht, unterdrückt: %s", R->cfg.settings.maxPerHour, text);
    logLine('X', l);
    return false;
  }
  if (!mesh_actions::sendChannel(channel, text, mesh_actions::Origin::ALERT)) {
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "Warteschlange voll, verworfen: %s", text);
    logLine('X', l);
    return false;
  }
  char l[LogBuffer::LINE_LEN];
  snprintf(l, sizeof(l), "%s -> %s", c.name, text);
  logLine('A', l);
  return true;
}

// Zabbix-Kennung einer Meldung (E41), src leer = ohne Kennung
struct Zev {
  char src[8];
  char ev;
  bool hasValue;
  float value;
};

Zev zev(char kind, const char* key, char ev, bool hasValue = false, float value = 0) {
  Zev z;
  zbx::makeSrc(kind, key, z.src);
  z.ev = ev;
  z.hasValue = hasValue;
  z.value = value;
  return z;
}

// Platz für den Text in einer Kanalnachricht: MeshCore stellt "Name: " voran
// und kürzt alles auf MAX_TEXT_LEN (160).
size_t textRoom() {
  MeshSnapshot s = gateway_status::get();
  size_t used = strlen(s.nodeName) + 2;
  return used < mesh_actions::MAX_TEXT - 40 ? mesh_actions::MAX_TEXT - used : 40;
}

// Meldung senden, je nach Einstellung mit Zabbix-Kennung im Text oder als
// Kopie mit Kennung in den Zabbix-Kanal. text enthält ggf. schon "[ACK n] ".
// Rückgabe: Hauptmeldung gesendet. Aufrufer hält s_lock.
bool emitEvent(const char* channel, const char* text, const Zev* z, uint8_t ackId, bool countLimit) {
  const Settings& st = R->cfg.settings;
  if (!z || !z->src[0] || st.zbxMode == ZBX_OFF) return emit(channel, text, countLimit);
  zbx::Marker m;
  memset(&m, 0, sizeof(m));
  strlcpy(m.src, z->src, sizeof(m.src));
  m.ev = z->ev;
  m.hasValue = z->hasValue;
  m.value = z->value;
  m.ack = ackId;
  char out[mesh_actions::MAX_TEXT + 1];
  zbx::appendMarker(text, m, out, textRoom() + 1);
  if (st.zbxMode == ZBX_APPEND) return emit(channel, out, countLimit);
  bool ok = emit(channel, text, countLimit);
  emit(st.zbxChannel, out, countLimit);   // ZBX_CHANNEL
  return ok;
}

// Alarm senden; mit Bestätigungspflicht vorher offen eintragen (Nummer im
// Text). Wird die erste Meldung unterdrückt (Sendelimit), bleibt der Eintrag
// offen und die Wiederholungen holen sie nach. Aufrufer hält s_lock.
void emitAlarm(ack::Kind kind, int src, bool ackRequired, const char* channelName, const char* text, const Zev& z) {
  if (!ackRequired) { emitEvent(channelName, text, &z, 0, true); return; }
  uint8_t id = R->acks.open(kind, src, channelName, text, millis());
  const Settings& st = R->cfg.settings;
  R->acks.setZbx(id, st.zbxMode == ZBX_CHANNEL ? st.zbxChannel : "", st.zbxMode != ZBX_OFF ? z.src : "",
                 z.hasValue, z.value);
  char out[mesh_actions::MAX_TEXT + 1];
  snprintf(out, sizeof(out), "[ACK %u] %s", (unsigned)id, text);
  emitEvent(channelName, out, &z, id, true);
}

// Erinnerung (repeatMin) einer Quelle mit offener Nummer trägt die Nummer mit.
void emitReminder(ack::Kind kind, int src, const char* channelName, const char* text, const Zev& z) {
  uint8_t id = R->acks.openIdOf(kind, src);
  if (!id) { emitEvent(channelName, text, &z, 0, true); return; }
  char out[mesh_actions::MAX_TEXT + 1];
  snprintf(out, sizeof(out), "[ACK %u] %s", (unsigned)id, text);
  emitEvent(channelName, out, &z, id, true);
}

// Fällige Wiederholungen (höchstens eine pro Durchlauf). Aufrufer hält s_lock.
void processRepeats() {
  uint32_t now = millis();
  int i = R->acks.due(now);
  if (i < 0) return;
  const ack::Entry& e = R->acks.at(i);
  char out[mesh_actions::MAX_TEXT + 1];
  ack::Tracker::format(e, true, out, sizeof(out));
  Zev z;
  strlcpy(z.src, e.marker, sizeof(z.src));
  z.ev = 'W';
  z.hasValue = e.hasValue;
  z.value = e.value;
  if (emitEvent(e.channel, out, &z, e.id, true)) {
    R->acks.repeated(i, now);
    if (e.repeats >= ack::REPEATS) {
      char l[LogBuffer::LINE_LEN];
      snprintf(l, sizeof(l), "[ACK %u] nach %d Wiederholungen weiter unbestätigt (bestätigen bis 24 h möglich)",
               (unsigned)e.id, ack::REPEATS);
      logLine('S', l);
    }
  } else {
    R->acks.postpone(i, now);
  }
}

// Antworten "ACK [Nr]" aus den Kanälen. Aufrufer hält s_lock.
void processAckReplies() {
  AckMsg m;
  char own[32];
  bool haveOwn = false;
  while (s_ackQueue && xQueueReceive(s_ackQueue, &m, 0) == pdTRUE) {
    if (!haveOwn) {
      strlcpy(own, gateway_status::get().nodeName, sizeof(own));
      haveOwn = true;
    }
    uint8_t ids[ack::MAX_OPEN];
    int n = R->acks.acknowledge(m.channel, m.body, own, ids, ack::MAX_OPEN);
    if (n < 0) continue;   // kein ACK oder an ein anderes Gateway (@[Name])
    char l[LogBuffer::LINE_LEN];
    if (n == 0) {
      snprintf(l, sizeof(l), "%s: \"%s\" von %s - nichts Offenes dazu", m.channel, m.body, m.sender);
      logLine('S', l);
      continue;
    }
    char list[48] = {0};
    size_t p = 0;
    for (int k = 0; k < n && k < ack::MAX_OPEN; k++) {
      int w = snprintf(list + p, sizeof(list) - p, "%s%u", k ? "," : "", (unsigned)ids[k]);
      if (w < 0 || (size_t)w >= sizeof(list) - p) break;
      p += w;
    }
    // Rückmeldung in den Kanal (zählt nicht gegen das Sendelimit: eine
    // Nachricht je Bestätigung)
    char out[mesh_actions::MAX_TEXT + 1];
    snprintf(out, sizeof(out), "ACK %s bestätigt (%s)", list, m.sender);
    emit(m.channel, out, false);
    snprintf(l, sizeof(l), "[ACK %s] bestätigt von %s in %s", list, m.sender, m.channel);
    logLine('S', l);
    event_log::addf("ALARM", "ACK %s bestätigt von %s (%s)", list, m.sender, m.channel);
  }
}

// Lebenszeichen für Zabbix (E41): nur mit Kennung, im Modus "eigener Kanal"
// nur dorthin, sonst in den Alarmkanal. Aufrufer hält s_lock.
void processHeartbeat() {
  static uint32_t lastMs = 0;
  static bool first = true;
  const Settings& st = R->cfg.settings;
  if (st.zbxMode == ZBX_OFF || !st.heartbeatH) { first = true; return; }
  uint32_t now = millis();
  // erstes Lebenszeichen 2 min nach dem Start bzw. nach dem Einschalten
  if (first) { lastMs = now - st.heartbeatH * 3600000UL + 120000UL; first = false; }
  if (now - lastMs < st.heartbeatH * 3600000UL) return;
  lastMs = now;
  float hours = now / 3600000.0f;
  char text[64];
  snprintf(text, sizeof(text), "LEBENSZEICHEN Laufzeit %.1f h", (double)hours);
  Zev z = zev('G', "gateway", 'H', true, hours);
  zbx::Marker m;
  memset(&m, 0, sizeof(m));
  strlcpy(m.src, z.src, sizeof(m.src));
  m.ev = 'H';
  m.hasValue = true;
  m.value = hours;
  char out[mesh_actions::MAX_TEXT + 1];
  zbx::appendMarker(text, m, out, textRoom() + 1);
  emit(st.zbxMode == ZBX_CHANNEL ? st.zbxChannel : st.defaultChannel, out, true);
}

const char* tplOr(const char* t, const char* def) { return (t && *t) ? t : def; }

// Stabiler Schlüssel einer Regel für die Zabbix-Quelle: Gerät, Messwert,
// Richtung. Grenzwert/Hysterese ändern ändert die Quelle nicht.
String ruleKey(const Rule& r, const Device& d) {
  return String(d.name) + "|" + r.path + "|" + (r.op ? "<" : ">");
}

void ruleText(const Rule& r, const Device& d, const RuleRuntime& rt, const char* state, const char* tpl,
              char* out, size_t outLen) {
  rules::TemplateVars v{d.name, rules::labelFor(r.path), rt.st.lastValue, rt.st.hasValue,
                        rules::unitFor(r.path), r.op ? rules::Op::BELOW : rules::Op::ABOVE, r.threshold, state, nullptr};
  rules::renderTemplate(tpl, v, out, outLen);
}

// Gleiches Gerät = gleiche Adresse. Gleiche Regel = gleiches Gerät + gleiche
// Bedingung. Nur dann bleibt der Laufzeitzustand beim Speichern erhalten -
// sonst würde ein anstehender Alarm bei JEDER Konfigurationsänderung (z. B.
// nur Standardkanal geändert) erneut gesendet (E22).
bool sameDevice(const Device& a, const Device& b) {
  if (!a.used || !b.used || a.type != b.type) return false;
  if (a.type == DEV_REPEATER) return !memcmp(a.key, b.key, sizeof(a.key));
  return !strcmp(a.host, b.host) && a.port == b.port;
}
bool sameRule(const Rule& a, const Device& da, const Rule& b, const Device& db) {
  return a.used && b.used && sameDevice(da, db) && !strcmp(a.path, b.path) && a.op == b.op &&
         a.threshold == b.threshold && a.hysteresis == b.hysteresis && a.minDurationS == b.minDurationS;
}

void evaluateRules(int di, uint32_t now);

// Konfiguration übernehmen; Zustände unveränderter Geräte/Regeln behalten.
void syncConfig() {
  if (sm::generation() == R->gen) return;
  static Config oldCfg;            // statisch: ca. 5 KB, nicht auf den Task-Stack
  static DevRuntime oldDev[MAX_DEVICES];
  static RuleRuntime oldRule[MAX_RULES];
  static PingRuntime oldPing[MAX_PINGS];
  oldCfg = R->cfg;
  memcpy(oldDev, R->dev, sizeof(oldDev));
  memcpy(oldRule, R->rule, sizeof(oldRule));
  memcpy(oldPing, R->ping, sizeof(oldPing));

  R->gen = sm::copy(R->cfg);
  int keptDev = 0, keptRule = 0;
  // Offene Bestätigungen folgen ihrer Regel bzw. ihrem Ping-Ziel (E39);
  // gelöscht, geändert oder Haken entfernt -> Eintrag weg.
  int8_t ruleMap[MAX_RULES], pingMap[MAX_PINGS];
  memset(ruleMap, -1, sizeof(ruleMap));
  memset(pingMap, -1, sizeof(pingMap));
  for (int i = 0; i < MAX_DEVICES; i++) {
    R->dev[i] = DevRuntime();
    // gleich neu abfragen - entfernte Repeater nicht bei jeder Änderung
    // (Funkzeit); neue Repeater fragt remoteCycle ohnehin sofort ab
    R->pollFlag[i] = R->cfg.devices[i].type != DEV_REPEATER;
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
      if (o.used && sameRule(r, deviceOf(R->cfg, r.device), o, deviceOf(oldCfg, o.device))) {
        R->rule[i] = oldRule[j];
        if (r.ackRequired) ruleMap[j] = (int8_t)i;
        keptRule++;
        break;
      }
    }
  }
  // Ping-Ziel mit gleicher Adresse behält seinen Zustand (kein erneuter Alarm)
  for (int i = 0; i < MAX_PINGS; i++) {
    R->ping[i] = PingRuntime();
    if (!R->cfg.pings[i].used) continue;
    for (int j = 0; j < MAX_PINGS; j++) {
      if (oldCfg.pings[j].used && !strcmp(oldCfg.pings[j].host, R->cfg.pings[i].host)) {
        R->ping[i] = oldPing[j];
        if (R->cfg.pings[i].ackRequired) pingMap[j] = (int8_t)i;
        break;
      }
    }
  }
  R->acks.remap(ack::Kind::RULE, ruleMap, MAX_RULES);
  R->acks.remap(ack::Kind::PING, pingMap, MAX_PINGS);
  // Entfernte Repeater werden nicht bei jeder Änderung neu abgefragt (E42):
  // neue/geänderte Regeln gleich mit den letzten Werten auswerten, sonst
  // griffen sie erst bei der nächsten Abfrage (bis zu Stunden später).
  for (int i = 0; i < MAX_DEVICES; i++) {
    const DevRuntime& dr = R->dev[i];
    if (R->cfg.devices[i].used && R->cfg.devices[i].type == DEV_REPEATER && dr.everOk && dr.dev.fails == 0 && dr.nValues > 0) {
      evaluateRules(i, millis());
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
  const Device& d = deviceOf(R->cfg, di);
  DevRuntime& dr = runtimeOf(di);
  rules::DevEvent de = rules::deviceResult(dr.dev, ok, R->cfg.settings.offlineAfterFails, now);
  if (de != rules::DevEvent::NONE && d.offlineAlert) {
    char text[mesh_actions::MAX_TEXT + 1];
    rules::TemplateVars v{d.name, "", 0, false, "", rules::Op::ABOVE, 0,
                          de == rules::DevEvent::OFFLINE ? "OFFLINE" : "ONLINE", nullptr};
    const char* tpl = de == rules::DevEvent::OFFLINE ? tplOr(R->cfg.settings.offlineText, DEFAULT_OFFLINE)
                                                     : tplOr(R->cfg.settings.onlineText, DEFAULT_ONLINE);
    rules::renderTemplate(tpl, v, text, sizeof(text));
    Zev z = zev('D', d.name, de == rules::DevEvent::OFFLINE ? 'F' : 'N');
    emitEvent(R->cfg.settings.defaultChannel, text, &z, 0, true);
  }
  if (!ok) return;
  evaluateRules(di, now);
}

// Regeln eines Geräts mit den zuletzt gemessenen Werten auswerten. Aufrufer hält s_lock.
void evaluateRules(int di, uint32_t now) {
  const Device& d = deviceOf(R->cfg, di);
  DevRuntime& dr = runtimeOf(di);
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
    const char* ch = r.channel[0] ? r.channel : R->cfg.settings.defaultChannel;
    char ev = e == rules::Event::ALERT ? 'A' : (e == rules::Event::REMIND ? 'E' : 'O');
    Zev z = zev('R', ruleKey(r, d).c_str(), ev, rt.st.hasValue, rt.st.lastValue);
    if (e == rules::Event::ALERT) {
      emitAlarm(ack::Kind::RULE, ri, r.ackRequired, ch, text, z);
    } else if (e == rules::Event::REMIND) {
      emitReminder(ack::Kind::RULE, ri, ch, text, z);
    } else {
      R->acks.markCleared(ack::Kind::RULE, ri);   // bleibt bis ACK offen (E39)
      emitEvent(ch, text, &z, 0, true);
    }
  }
}

// ---------------------------------------------------------------------------
// Ping-Check (E25)
// ---------------------------------------------------------------------------

void evaluatePing(int pi, bool ok, uint32_t now) {
  // Aufrufer hält s_lock
  const PingTarget& t = R->cfg.pings[pi];
  PingRuntime& pr = R->ping[pi];
  rules::PingEvent e = rules::pingResult(pr.st, ok, t.thresholdS, now);
  if (e == rules::PingEvent::NONE) return;
  bool down = e == rules::PingEvent::DOWN;
  rules::TemplateVars v{t.name, "", (float)rules::pingDownSeconds(pr.st, now), down, "s",
                        rules::Op::ABOVE, (float)t.thresholdS, down ? "PING-ALARM" : "PING-OK", t.host};
  const char* tpl = down ? tplOr(R->cfg.settings.pingDownText, DEFAULT_PING_DOWN)
                         : tplOr(R->cfg.settings.pingUpText, DEFAULT_PING_UP);
  char text[mesh_actions::MAX_TEXT + 1];
  rules::renderTemplate(tpl, v, text, sizeof(text));
  // Alarmkanal = Standardkanal
  Zev z = zev('P', t.name, down ? 'A' : 'O', down, (float)rules::pingDownSeconds(pr.st, now));
  if (down) {
    emitAlarm(ack::Kind::PING, pi, t.ackRequired, R->cfg.settings.defaultChannel, text, z);
  } else {
    R->acks.markCleared(ack::Kind::PING, pi);
    emitEvent(R->cfg.settings.defaultChannel, text, &z, 0, true);
  }
}

void pingCycle() {
  for (int pi = 0; pi < MAX_PINGS; pi++) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    PingTarget t = R->cfg.pings[pi];
    uint32_t now = millis();
    bool due = t.used && t.enabled && (!R->ping[pi].checked || now - R->ping[pi].lastCheckMs >= t.intervalS * 1000UL);
    uint32_t gen = R->gen;
    xSemaphoreGive(s_lock);
    if (!due) continue;

    uint32_t rtt = 0;
    char ip[16] = {0}, err[48] = {0};
    bool ok = net_check::ping(t.host, rtt, ip, sizeof(ip), err, sizeof(err));

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (R->gen == gen) {
      PingRuntime& pr = R->ping[pi];
      now = millis();
      pr.checked = true;
      pr.lastCheckMs = now;
      pr.lastOk = ok;
      if (ok) pr.rttMs = rtt;
      if (ip[0]) strlcpy(pr.ip, ip, sizeof(pr.ip));
      strlcpy(pr.error, err, sizeof(pr.error));
      evaluatePing(pi, ok, now);
    }
    xSemaphoreGive(s_lock);
  }
}

// Nach einem WLAN-Ausfall des GATEWAYS die Ping-Uhren neu ansetzen: sonst wäre
// die letzte Antwort "uralt" und der erste Fehlversuch sofort ein Alarm.
void rebasePings() {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  uint32_t now = millis();
  for (int i = 0; i < MAX_PINGS; i++) {
    if (!R->ping[i].st.down) R->ping[i].st.lastOkMs = now;
  }
  xSemaphoreGive(s_lock);
}

// Chiptemperatur des ESP32-S3 (E30): alle 60 s messen, Regeln auswerten (auch
// ohne WLAN - Meldungen gehen per Funk), stündlich ins Ereignis-Log.
void measureInternal(bool evaluate) {
  static uint32_t lastMs = 0, lastLogMs = 0;
  static bool first = true;
  uint32_t now = millis();
  if (!first && now - lastMs < INTERNAL_INTERVAL_MS) return;
  lastMs = now;
  float t = temperatureRead();
  bool ok = !isnan(t) && t > -40 && t < 150;
  s_chipTemp = ok ? t : NAN;
  if (ok && (first || now - lastLogMs >= 3600000UL)) {
    lastLogMs = now;
    event_log::addf("SYS", "Chiptemperatur %.1f \xC2\xB0" "C", t);   // °C (UTF-8)
  }
  first = false;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  DevRuntime& dr = R->internal;
  dr.everPolled = true;
  dr.lastPollMs = now;
  if (ok) {
    dr.nValues = 1;
    strlcpy(dr.values[0].path, INTERNAL_PATH_CHIP_TEMP, sizeof(dr.values[0].path));
    dr.values[0].sensorName[0] = 0;
    dr.values[0].value = t;
    dr.lastOkMs = now;
    dr.everOk = true;
    dr.error[0] = 0;
  } else {
    dr.nValues = 0;
    strlcpy(dr.error, "Sensor liefert keinen Wert", sizeof(dr.error));
  }
  if (evaluate) evaluateDevice(INTERNAL_DEVICE, ok, now);
  xSemaphoreGive(s_lock);
}

// ---------------------------------------------------------------------------
// Entfernte Repeater (E42): Abfrage per Funk über RemoteAdmin
// ---------------------------------------------------------------------------

void addValue(DevRuntime& dr, const char* path, float v) {
  if (dr.nValues >= MAX_VALUES) return;
  SmValue& x = dr.values[dr.nValues++];
  strlcpy(x.path, path, sizeof(x.path));
  x.sensorName[0] = 0;
  x.value = v;
}

// Ergebnis einer fertigen Abfrage übernehmen. Aufrufer hält s_lock.
void remoteResult(int di, const Device& d, DevRuntime& dr, const remote::JobInfo& ji, uint32_t now) {
  dr.everPolled = true;
  bool ok = ji.state == remote::State::DONE && ji.poll.hasStatus;
  if (ok) {
    const remote::PollData& p = ji.poll;
    dr.nValues = 0;
    // Akku aus dem Status (mV); fehlt er dort (0), aus der Telemetrie
    if (p.battMv) addValue(dr, RPT_PATH_BATTERY, p.battMv / 1000.0f);
    else if (p.hasVoltage) addValue(dr, RPT_PATH_BATTERY, p.voltage);
    if (p.hasTemp) addValue(dr, RPT_PATH_TEMP, p.tempC);
    addValue(dr, RPT_PATH_UPTIME, p.uptimeS / 3600.0f);
    addValue(dr, RPT_PATH_NOISE, p.noiseFloor);
    dr.lastOkMs = now;
    dr.everOk = true;
    dr.error[0] = 0;
    if (ji.flood) snprintf(dr.route, sizeof(dr.route), "Flood, %u ms", (unsigned)ji.ms);
    else snprintf(dr.route, sizeof(dr.route), "direkt %u Hop%s, %u ms", ji.hops, ji.hops == 1 ? "" : "s", (unsigned)ji.ms);
  } else {
    strlcpy(dr.error, ji.state == remote::State::DONE ? "Status fehlt" : ji.error, sizeof(dr.error));
    dr.route[0] = 0;
    // Fehlversuch: in 10 min erneut statt erst nach dem vollen Abstand, damit
    // "nicht erreichbar" nach N Fehlversuchen nicht Stunden dauert.
    uint32_t iv = d.intervalS * 1000UL;
    if (iv > 600000UL) dr.lastPollMs = now - iv + 600000UL;
  }
  evaluateDevice(di, ok, now);
}

void remoteCycle() {
  for (int di = 0; di < MAX_DEVICES; di++) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const Device& d = R->cfg.devices[di];
    DevRuntime& dr = R->dev[di];
    uint32_t now = millis();
    if (!d.used || !d.enabled || d.type != DEV_REPEATER) {
      dr.jobId = 0;
    } else if (dr.jobId) {
      remote::JobInfo ji;
      if (!remote::job(dr.jobId, ji)) {
        dr.jobId = 0;   // zu alt (sollte nicht vorkommen) -> beim nächsten Mal neu
      } else if (ji.state == remote::State::DONE || ji.state == remote::State::FAILED) {
        dr.jobId = 0;
        remoteResult(di, d, dr, ji, now);
      }
    } else if (R->pollFlag[di] || !dr.everPolled || now - dr.lastPollMs >= d.intervalS * 1000UL) {
      String err;
      uint32_t id = remote::submitPoll(d.key, d.pass, err);
      if (id) {
        dr.jobId = id;
        dr.lastPollMs = now;
        R->pollFlag[di] = false;
      } else {
        strlcpy(dr.error, err.c_str(), sizeof(dr.error));
      }
    }
    xSemaphoreGive(s_lock);
  }
}

void task(void*) {
  static SmValue vals[MAX_VALUES];
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    xSemaphoreTake(s_lock, portMAX_DELAY);
    syncConfig();
    bool enabled = R->cfg.settings.enabled;
    // Bestätigungen und Wiederholungen gehen per Funk, also auch ohne WLAN (E39)
    processAckReplies();
    processRepeats();
    processHeartbeat();
    xSemaphoreGive(s_lock);
    measureInternal(enabled);
    if (!enabled) continue;
    remoteCycle();   // entfernte Repeater gehen per Funk, also auch ohne WLAN (E42)
    // Ohne eigenes WLAN gar nicht abfragen: sonst würde jedes Sensormeter als
    // "nicht erreichbar" gezählt und per Funk (geht ja weiter) gemeldet,
    // obwohl das Gateway selbst das Problem ist.
    static bool wifiWasDown = false;
    if (network.mode() != NetworkManager::Mode::STATION) {
      wifiWasDown = true;
      continue;
    }
    if (wifiWasDown) {
      wifiWasDown = false;
      rebasePings();
    }

    for (int di = 0; di < MAX_DEVICES; di++) {
      xSemaphoreTake(s_lock, portMAX_DELAY);
      Device d = R->cfg.devices[di];
      uint32_t now = millis();
      bool due = d.used && d.enabled && d.type == DEV_SENSORMETER &&
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

    pingCycle();
  }
}

}  // namespace

void sm_poller::begin() {
  if (R) return;
  s_lock = xSemaphoreCreateMutex();
  s_ackQueue = xQueueCreate(4, sizeof(AckMsg));
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
    const Device& d = deviceOf(R->cfg, r.device);
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
      Zev z = zev('R', ruleKey(r, d).c_str(), 'T', rt.st.hasValue, rt.st.lastValue);
      emitEvent(ch, text, &z, 0, false);
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

float sm_poller::chipTempC() { return s_chipTemp; }

void sm_poller::onChannelText(const char* channelName, const char* text) {
  if (!s_ackQueue || !channelName || !text) return;
  // Kanaltexte sind "Absender: Nachricht" (BaseChatMesh)
  const char* sep = strstr(text, ": ");
  const char* body = sep ? sep + 2 : text;
  int id;
  if (!ack::Tracker::parseAck(body, id)) return;
  AckMsg m;
  strlcpy(m.channel, channelName, sizeof(m.channel));
  size_t sl = sep ? (size_t)(sep - text) : 0;
  if (sl >= sizeof(m.sender)) sl = sizeof(m.sender) - 1;
  memcpy(m.sender, text, sl);
  m.sender[sl] = 0;
  if (!m.sender[0]) strlcpy(m.sender, "?", sizeof(m.sender));
  strlcpy(m.body, body, sizeof(m.body));
  xQueueSend(s_ackQueue, &m, 0);
}

bool sm_poller::ackFromWeb(int id, String& err) {
  if (!R) { err = "nicht bereit"; return false; }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  char channel[ack::CHANNEL_LEN] = {0};
  for (int i = 0; i < ack::MAX_OPEN; i++) {
    if (R->acks.at(i).used && R->acks.at(i).id == id) strlcpy(channel, R->acks.at(i).channel, sizeof(channel));
  }
  bool ok = id >= 1 && id <= 99 && R->acks.acknowledgeId((uint8_t)id);
  if (ok) {
    // Kanal erfährt es auch, sonst wartet dort jemand auf Wiederholungen
    char out[mesh_actions::MAX_TEXT + 1];
    snprintf(out, sizeof(out), "ACK %d bestätigt (Web)", id);
    emit(channel, out, false);
    char l[LogBuffer::LINE_LEN];
    snprintf(l, sizeof(l), "[ACK %d] in der Weboberfläche bestätigt", id);
    logLine('S', l);
    event_log::addf("ALARM", "ACK %d in der Weboberfläche bestätigt", id);
  } else {
    err = "Nummer nicht (mehr) offen";
  }
  xSemaphoreGive(s_lock);
  return ok;
}

// Ein Gerät als JSON (Sensormeter und Pseudo-Gerät INTERNAL_DEVICE). Aufrufer hält s_lock.
static void devJson(String& out, int i, const DevRuntime& d, uint32_t now) {
  out += "{\"idx\":" + String(i);
  out += ",\"polled\":" + String(d.everPolled ? "true" : "false");
  out += ",\"online\":" + String(d.everOk && !d.dev.offline && d.dev.fails == 0 ? "true" : "false");
  out += ",\"offline\":" + String(d.dev.offline ? "true" : "false");
  out += ",\"fails\":" + String(d.dev.fails);
  out += ",\"lastOkAgoS\":" + (d.everOk ? String((now - d.lastOkMs) / 1000) : String("null"));
  out += ",\"busy\":" + String(d.jobId ? "true" : "false");
  out += ",\"route\":\"";
  esc(out, d.route);
  out += "\",\"error\":\"";
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

void sm_poller::statusJson(String& out) {
  out += "{\"devices\":[";
  if (!R) { out += "],\"internal\":null,\"rules\":[],\"pings\":[],\"acks\":[],\"limit\":{\"used\":0,\"max\":0}}"; return; }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  uint32_t now = millis();
  bool first = true;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!R->cfg.devices[i].used) continue;
    if (!first) out += ',';
    first = false;
    devJson(out, i, R->dev[i], now);
  }
  out += "],\"internal\":";
  devJson(out, INTERNAL_DEVICE, R->internal, now);
  out += ",\"rules\":[";
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
    out += ",\"lastEventAgoS\":" + (r.lastEventMs ? String((now - r.lastEventMs) / 1000) : String("null"));
    out += ",\"ack\":" + String(R->acks.openIdOf(ack::Kind::RULE, i)) + "}";
  }
  out += "],\"pings\":[";
  first = true;
  for (int i = 0; i < MAX_PINGS; i++) {
    if (!R->cfg.pings[i].used) continue;
    const PingRuntime& p = R->ping[i];
    if (!first) out += ',';
    first = false;
    const char* st = !p.checked ? "unknown" : (p.st.down ? "down" : (p.lastOk ? "up" : "failing"));
    out += "{\"idx\":" + String(i) + ",\"state\":\"" + st + "\"";
    out += ",\"rttMs\":" + (p.checked && p.lastOk ? String(p.rttMs) : String("null"));
    out += ",\"noReplyS\":" + (p.checked && !p.lastOk ? String(rules::pingDownSeconds(p.st, now)) : String("null"));
    out += ",\"ip\":\"";
    esc(out, p.ip);
    out += "\",\"error\":\"";
    esc(out, p.error);
    out += "\",\"ack\":" + String(R->acks.openIdOf(ack::Kind::PING, i)) + "}";
  }
  out += "],\"acks\":[";
  first = true;
  for (int i = 0; i < ack::MAX_OPEN; i++) {
    const ack::Entry& e = R->acks.at(i);
    if (!e.used) continue;
    if (!first) out += ',';
    first = false;
    out += "{\"id\":" + String(e.id) + ",\"kind\":\"" + (e.kind == ack::Kind::RULE ? "rule" : "ping") + "\"";
    out += ",\"src\":" + String(e.src) + ",\"channel\":\"";
    esc(out, e.channel);
    out += "\",\"text\":\"";
    esc(out, e.text);
    out += "\",\"ageS\":" + String((now - e.sentMs) / 1000) + ",\"repeats\":" + String(e.repeats);
    out += ",\"max\":" + String(ack::REPEATS) + ",\"cleared\":" + String(e.cleared ? "true" : "false") + "}";
  }
  out += "],\"limit\":{\"used\":" + String(R->limiter.usedInWindow(now)) + ",\"max\":" + String(R->cfg.settings.maxPerHour) + "}}";
  xSemaphoreGive(s_lock);
}

