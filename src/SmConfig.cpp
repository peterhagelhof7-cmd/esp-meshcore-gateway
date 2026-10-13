#include "SmConfig.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

namespace sm {

// Kurz halten: jede Nachricht kostet Airtime, und der Knotenname wird von
// sendChannelText ohnehin vorangestellt ("Gateway: ...").
const char* DEFAULT_ALERT = "{state} {device}: {metric} {value}{unit} ({op}{threshold}{unit})";
const char* DEFAULT_RECOVER = "OK {device}: {metric} {value}{unit} wieder normal";
const char* DEFAULT_OFFLINE = "OFFLINE {device} nicht erreichbar";
const char* DEFAULT_ONLINE = "ONLINE {device} wieder erreichbar";

static const char* NVS_NS = "gw";
static const char* NVS_KEY = "smcfg";

static Config* s_cfg = nullptr;   // im PSRAM, ca. 5 KB
static SemaphoreHandle_t s_lock = nullptr;
static uint32_t s_gen = 1;

static void defaults(Config& c) {
  memset(&c, 0, sizeof(c));
  c.settings.enabled = true;
  c.settings.maxPerHour = 6;
  c.settings.offlineAfterFails = 3;
}

static void cpy(char* dst, size_t n, const char* src) { strlcpy(dst, src ? src : "", n); }

static bool validHost(const char* h) {
  if (!*h) return false;
  for (const char* p = h; *p; p++) {
    char c = *p;
    if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_')) return false;
  }
  return true;
}

// JSON -> Config. old = bisherige Konfig (für Passwort "***").
static bool parse(const char* json, size_t len, const Config& old, Config& c, String& err) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json, len);
  if (e) { err = String("JSON: ") + e.c_str(); return false; }
  defaults(c);

  JsonObject st = doc["settings"];
  c.settings.enabled = st["enabled"] | true;
  cpy(c.settings.defaultChannel, sizeof(c.settings.defaultChannel), st["defaultChannel"] | "");
  c.settings.maxPerHour = constrain((int)(st["maxPerHour"] | 6), 0, 60);
  c.settings.offlineAfterFails = constrain((int)(st["offlineAfterFails"] | 3), 0, 100);
  cpy(c.settings.alertText, sizeof(c.settings.alertText), st["alertText"] | "");
  cpy(c.settings.recoverText, sizeof(c.settings.recoverText), st["recoverText"] | "");
  cpy(c.settings.offlineText, sizeof(c.settings.offlineText), st["offlineText"] | "");
  cpy(c.settings.onlineText, sizeof(c.settings.onlineText), st["onlineText"] | "");

  JsonArray devs = doc["devices"];
  if (devs.size() > MAX_DEVICES) { err = "Maximal 8 Geräte"; return false; }
  int i = 0;
  for (JsonObject d : devs) {
    Device& x = c.devices[i];
    x.used = true;
    x.enabled = d["enabled"] | true;
    cpy(x.name, sizeof(x.name), d["name"] | "");
    cpy(x.host, sizeof(x.host), d["host"] | "");
    x.port = d["port"] | 80;
    cpy(x.user, sizeof(x.user), d["user"] | "");
    const char* pw = d["pass"] | "";
    int oldIdx = d["origIdx"] | -1;   // Position vor dem Bearbeiten (für "***")
    if (!strcmp(pw, "***") && oldIdx >= 0 && oldIdx < MAX_DEVICES) cpy(x.pass, sizeof(x.pass), old.devices[oldIdx].pass);
    else cpy(x.pass, sizeof(x.pass), pw);
    x.intervalS = constrain((int)(d["intervalS"] | 60), 10, 3600);
    x.offlineAlert = d["offlineAlert"] | true;
    if (!x.name[0]) { err = "Gerät " + String(i + 1) + ": Name fehlt"; return false; }
    if (!validHost(x.host)) { err = String("Gerät ") + x.name + ": Adresse ungültig"; return false; }
    i++;
  }

  JsonArray rules = doc["rules"];
  if (rules.size() > MAX_RULES) { err = "Maximal 16 Regeln"; return false; }
  i = 0;
  for (JsonObject r : rules) {
    Rule& x = c.rules[i];
    x.used = true;
    x.enabled = r["enabled"] | true;
    x.device = r["device"] | -1;
    cpy(x.path, sizeof(x.path), r["path"] | "");
    x.op = (r["op"] | 0) ? 1 : 0;
    x.threshold = r["threshold"] | 0.0f;
    x.hysteresis = fabsf(r["hysteresis"] | 0.0f);
    x.minDurationS = constrain((long)(r["minDurationS"] | 0), 0L, 86400L);
    x.repeatMin = constrain((int)(r["repeatMin"] | 0), 0, 1440);
    cpy(x.channel, sizeof(x.channel), r["channel"] | "");
    x.notifyRecovery = r["notifyRecovery"] | true;
    cpy(x.text, sizeof(x.text), r["text"] | "");
    if (x.device < 0 || x.device >= MAX_DEVICES || !c.devices[x.device].used) {
      err = "Regel " + String(i + 1) + ": Gerät ungültig"; return false;
    }
    if (!x.path[0] || !strchr(x.path, '.')) { err = "Regel " + String(i + 1) + ": Messwert fehlt"; return false; }
    if (isnan(x.threshold)) { err = "Regel " + String(i + 1) + ": Grenzwert ungültig"; return false; }
    i++;
  }
  return true;
}

static void serialize(const Config& c, String& out, bool maskPass) {
  JsonDocument doc;
  JsonObject st = doc["settings"].to<JsonObject>();
  st["enabled"] = c.settings.enabled;
  st["defaultChannel"] = c.settings.defaultChannel;
  st["maxPerHour"] = c.settings.maxPerHour;
  st["offlineAfterFails"] = c.settings.offlineAfterFails;
  st["alertText"] = c.settings.alertText;
  st["recoverText"] = c.settings.recoverText;
  st["offlineText"] = c.settings.offlineText;
  st["onlineText"] = c.settings.onlineText;
  JsonArray devs = doc["devices"].to<JsonArray>();
  for (int i = 0; i < MAX_DEVICES; i++) {
    const Device& x = c.devices[i];
    if (!x.used) continue;
    JsonObject d = devs.add<JsonObject>();
    d["enabled"] = x.enabled;
    d["name"] = x.name;
    d["host"] = x.host;
    d["port"] = x.port;
    d["user"] = x.user;
    d["pass"] = (maskPass && x.pass[0]) ? "***" : x.pass;
    d["intervalS"] = x.intervalS;
    d["offlineAlert"] = x.offlineAlert;
  }
  JsonArray rules = doc["rules"].to<JsonArray>();
  for (int i = 0; i < MAX_RULES; i++) {
    const Rule& x = c.rules[i];
    if (!x.used) continue;
    JsonObject r = rules.add<JsonObject>();
    r["enabled"] = x.enabled;
    r["device"] = x.device;
    r["path"] = x.path;
    r["op"] = x.op;
    r["threshold"] = x.threshold;
    r["hysteresis"] = x.hysteresis;
    r["minDurationS"] = x.minDurationS;
    r["repeatMin"] = x.repeatMin;
    r["channel"] = x.channel;
    r["notifyRecovery"] = x.notifyRecovery;
    r["text"] = x.text;
  }
  serializeJson(doc, out);
}

void begin() {
  if (s_cfg) return;
  s_lock = xSemaphoreCreateMutex();
  s_cfg = (Config*)heap_caps_calloc(1, sizeof(Config), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_cfg) s_cfg = (Config*)calloc(1, sizeof(Config));
  defaults(*s_cfg);

  Preferences p;
  p.begin(NVS_NS, true);
  size_t n = p.getBytesLength(NVS_KEY);
  if (n > 0 && n < 16384) {
    char* buf = (char*)malloc(n + 1);
    if (buf) {
      p.getBytes(NVS_KEY, buf, n);
      buf[n] = 0;
      Config* tmp = (Config*)calloc(1, sizeof(Config));
      String err;
      if (tmp && parse(buf, n, *s_cfg, *tmp, err)) {
        *s_cfg = *tmp;
      } else {
        Serial.println("[SM] gespeicherte Konfig ungültig: " + err);
      }
      free(tmp);
      free(buf);
    }
  }
  p.end();
}

uint32_t copy(Config& out) {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  out = *s_cfg;
  uint32_t g = s_gen;
  xSemaphoreGive(s_lock);
  return g;
}

uint32_t generation() {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  uint32_t g = s_gen;
  xSemaphoreGive(s_lock);
  return g;
}

bool setFromJson(const char* json, size_t len, String& err) {
  Config* old = (Config*)malloc(sizeof(Config));
  Config* neu = (Config*)malloc(sizeof(Config));
  if (!old || !neu) { free(old); free(neu); err = "kein Speicher"; return false; }
  copy(*old);
  bool ok = parse(json, len, *old, *neu, err);
  if (ok) {
    String stored;
    serialize(*neu, stored, false);   // unmaskiert speichern
    Preferences p;
    p.begin(NVS_NS, false);
    size_t w = p.putBytes(NVS_KEY, stored.c_str(), stored.length());
    p.end();
    if (w != stored.length()) {
      err = "Speichern in NVS fehlgeschlagen";
      ok = false;
    } else {
      xSemaphoreTake(s_lock, portMAX_DELAY);
      *s_cfg = *neu;
      s_gen++;
      xSemaphoreGive(s_lock);
    }
  }
  free(old);
  free(neu);
  return ok;
}

void toJson(String& out) {
  Config* c = (Config*)malloc(sizeof(Config));
  if (!c) { out = "{}"; return; }
  copy(*c);
  serialize(*c, out, true);
  free(c);
}

}  // namespace sm
