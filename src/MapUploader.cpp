#include "MapUploader.h"
#include "GatewayMesh.h"
#include "GatewayStatus.h"
#include "EventLog.h"

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ed_25519.h>
#include <MeshCore.h>
#include <helpers/AdvertDataHelpers.h>

namespace {

const char* NVS_NS   = "mapup";
const char* API_URL  = "https://map.meshcore.io/api/v1/uploader/node";
const int   MAX_KNOWN = 24;      // gehörte Repeater/Rooms im Speicher
const int   RAW_MAX   = MAX_TRANS_UNIT;   // 255

struct Advert {
  uint8_t  raw[RAW_MAX];
  uint8_t  len = 0;
  uint8_t  pub[PUB_KEY_SIZE];
  char     name[32] = {0};
  uint8_t  type = 0;
  bool     has_loc = false;
  uint32_t adv_ts = 0;      // Zeitstempel aus dem Advert
  uint32_t heard_ms = 0;    // millis() beim Empfang (0 = leer)
  // Ergebnis des letzten Uploads dieses Eintrags:
  uint8_t  up_state = 0;    // 0=nie, 1=ok, 2=Fehler
  uint32_t up_ms = 0;
};

// Upload-Schlüssel
uint8_t  g_seed[32];
uint8_t  g_pub[32];
uint8_t  g_priv[64];
bool     g_has_key = false;

// Speicher gehörter Adverts + eigenes Advert
Advert   g_known[MAX_KNOWN];
Advert   g_self;              // heard_ms>0 sobald gebaut
SemaphoreHandle_t g_mtx = nullptr;

// Anforderungen / Zustand
volatile bool g_want_self_capture = false;   // loopTask soll eigenes Advert bauen
volatile bool g_self_ready = false;          // eigenes Advert für Upload bereit
volatile bool g_pending_self = false;        // Task: eigenen Eintrag hochladen
volatile bool g_pending_known = false;       // Task: bekannte Repeater hochladen
volatile bool g_busy = false;
char     g_last_msg[96] = {0};
TaskHandle_t g_task = nullptr;

// Automatisches Melden
const uint16_t DEFAULT_AUTO_HOURS = 6;
const uint32_t BOOT_DELAY_MS = 120000;     // erst ~2 min nach Boot (WLAN/NTP/Adverts)
volatile uint16_t g_auto_hours = DEFAULT_AUTO_HOURS;
uint32_t g_next_auto_ms = 0;               // millis() des nächsten automatischen Meldens

void lock()   { if (g_mtx) xSemaphoreTake(g_mtx, portMAX_DELAY); }
void unlock() { if (g_mtx) xSemaphoreGive(g_mtx); }

void toHex(const uint8_t* in, int n, char* out) {
  static const char* H = "0123456789abcdef";
  for (int i = 0; i < n; i++) { out[2*i] = H[in[i] >> 4]; out[2*i+1] = H[in[i] & 0xF]; }
  out[2*n] = 0;
}

void deriveFromSeed() {
  ed25519_create_keypair(g_pub, g_priv, g_seed);
  g_has_key = true;
}

void saveKey() {
  char hx[65];
  Preferences p; p.begin(NVS_NS, false);
  toHex(g_seed, 32, hx); p.putString("seed", hx);
  toHex(g_pub, 32, hx);  p.putString("pub", hx);
  p.end();
}

bool loadKey() {
  Preferences p; p.begin(NVS_NS, true);
  String seed = p.getString("seed", "");
  p.end();
  if (seed.length() != 64) return false;
  for (int i = 0; i < 32; i++) g_seed[i] = (uint8_t)strtoul(seed.substring(2*i, 2*i+2).c_str(), nullptr, 16);
  deriveFromSeed();
  return true;
}

// freq/bw als knappe Dezimalzahl (62.5, 869.618, 250) ohne Nachkomma-Nullen.
String numStr(float v) {
  char b[24]; snprintf(b, sizeof(b), "%.4f", v);
  String s(b);
  while (s.indexOf('.') >= 0 && s.endsWith("0")) s.remove(s.length() - 1);
  if (s.endsWith(".")) s.remove(s.length() - 1);
  return s;
}

String jsonEscape(const String& in) {
  String o; o.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else o += c;
  }
  return o;
}

// Baut das zu signierende data-JSON (exakt diese Bytes werden gehasht/gesendet).
String buildData(const uint8_t* raw, uint8_t len, const MeshSnapshot& s) {
  char hex[2*RAW_MAX + 1];
  toHex(raw, len, hex);
  String d = "{\"params\":{\"freq\":";
  d += numStr(s.freq);
  d += ",\"cr\":"; d += String((int)s.cr);
  d += ",\"sf\":"; d += String((int)s.sf);
  d += ",\"bw\":"; d += numStr(s.bw);
  d += "},\"links\":[\"meshcore://";
  d += hex;
  d += "\"]}";
  return d;
}

// Ein Advert hochladen. true = vom Server angenommen.
bool uploadOne(const uint8_t* raw, uint8_t len, const MeshSnapshot& s, String& msg) {
  if (WiFi.status() != WL_CONNECTED) { msg = "kein WLAN"; return false; }
  if (!g_has_key)                    { msg = "kein Schlüssel"; return false; }

  String data = buildData(raw, len, s);

  uint8_t hash[32];
  mbedtls_sha256((const uint8_t*)data.c_str(), data.length(), hash, 0);
  uint8_t sig[64];
  ed25519_sign(sig, hash, 32, g_pub, g_priv);
  char sigHex[129], pubHex[65];
  toHex(sig, 64, sigHex);
  toHex(g_pub, 32, pubHex);

  String body = "{\"data\":\"";
  body += jsonEscape(data);
  body += "\",\"signature\":\""; body += sigHex;
  body += "\",\"publicKey\":\"";  body += pubHex;
  body += "\"}";

  WiFiClientSecure client;
  client.setInsecure();              // map.meshcore.io; öffentliche, signierte Daten
  client.setTimeout(15);
  HTTPClient http;
  if (!http.begin(client, API_URL)) { msg = "begin() fehlgeschlagen"; return false; }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t*)body.c_str(), body.length());
  String resp = (code > 0) ? http.getString() : HTTPClient::errorToString(code);
  http.end();

  bool ok = (code == 200);
  if (resp.length() > 60) resp = resp.substring(0, 60);
  msg = "HTTP " + String(code) + " " + resp;
  return ok;
}

// Core-0-Task: HTTPS erledigen, ohne loopTask/Web zu blockieren.
void taskFn(void*) {
  for (;;) {
    // Fälliges automatisches Melden anstoßen (eigener Eintrag + bekannte Repeater).
    if (g_auto_hours && (int32_t)(millis() - g_next_auto_ms) >= 0 &&
        !g_pending_self && !g_pending_known && !g_busy) {
      if (WiFi.status() != WL_CONNECTED) {
        g_next_auto_ms = millis() + 300000UL;   // kein WLAN: in 5 min erneut versuchen
      } else {
        g_want_self_capture = true; g_self_ready = false;
        g_pending_self = true; g_pending_known = true;
        g_next_auto_ms = millis() + (uint32_t)g_auto_hours * 3600000UL;
        event_log::addf("MAP", "Automatisches Melden (alle %u h)", (unsigned)g_auto_hours);
      }
    }

    bool do_self = false, do_known = false;
    lock(); do_self = g_pending_self && g_self_ready; do_known = g_pending_known; unlock();

    if (!do_self && !do_known) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }

    g_busy = true;
    MeshSnapshot snap = gateway_status::get();

    if (do_self) {
      Advert self; lock(); self = g_self; unlock();
      String msg;
      bool ok = uploadOne(self.raw, self.len, snap, msg);
      lock();
      g_self.up_state = ok ? 1 : 2; g_self.up_ms = millis();
      g_pending_self = false; g_self_ready = false;
      snprintf(g_last_msg, sizeof(g_last_msg), "Eigener Eintrag: %s", msg.c_str());
      unlock();
      event_log::addf("MAP", "Eigener Eintrag auf Karte: %s", ok ? "ok" : msg.c_str());
    }

    if (do_known) {
      int ok = 0, fail = 0;
      for (int i = 0; i < MAX_KNOWN; i++) {
        Advert e; lock(); e = g_known[i]; unlock();
        if (e.heard_ms == 0) continue;
        String msg;
        bool good = uploadOne(e.raw, e.len, snap, msg);
        lock(); g_known[i].up_state = good ? 1 : 2; g_known[i].up_ms = millis(); unlock();
        if (good) ok++; else fail++;
        vTaskDelay(pdMS_TO_TICKS(400));   // Server schonen
      }
      lock();
      g_pending_known = false;
      snprintf(g_last_msg, sizeof(g_last_msg), "Bekannte Repeater: %d ok, %d Fehler", ok, fail);
      unlock();
      event_log::addf("MAP", "Bekannte Repeater auf Karte: %d ok, %d Fehler", ok, fail);
    }

    g_busy = false;
  }
}

}  // namespace

namespace map_uploader {

void begin() {
  g_mtx = xSemaphoreCreateMutex();
  if (!loadKey()) {                 // beim ersten Start automatisch ein Paar erzeugen
    esp_fill_random(g_seed, 32);
    deriveFromSeed();
    saveKey();
  }
  Preferences p; p.begin(NVS_NS, true);
  g_auto_hours = p.getUShort("auto_h", DEFAULT_AUTO_HOURS);
  p.end();
  if (g_auto_hours) g_next_auto_ms = millis() + BOOT_DELAY_MS;
  xTaskCreatePinnedToCore(taskFn, "mapup", 16384, nullptr, 1, &g_task, 0);   // TLS braucht Stack
}

void loop(GatewayMesh& mesh) {
  if (!g_want_self_capture) return;
  uint8_t buf[RAW_MAX];
  int n = mesh.buildSelfAdvertRaw(buf, sizeof(buf));
  lock();
  if (n > 0) {
    memcpy(g_self.raw, buf, n);
    g_self.len = n;
    memcpy(g_self.pub, mesh.getSelfId().pub_key, PUB_KEY_SIZE);
    g_self.heard_ms = millis();
    g_self_ready = true;
  }
  g_want_self_capture = false;
  unlock();
}

void captureHeard(const uint8_t* raw, uint8_t len, const uint8_t* pub,
                  const char* name, uint8_t type, bool has_loc, uint32_t adv_timestamp) {
  if (type != ADV_TYPE_REPEATER && type != ADV_TYPE_ROOM) return;   // nur Infrastruktur
  if (len == 0 || len > RAW_MAX) return;
  lock();
  int slot = -1, oldest = 0;
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (g_known[i].heard_ms && memcmp(g_known[i].pub, pub, PUB_KEY_SIZE) == 0) { slot = i; break; }
    if (g_known[i].heard_ms == 0) { if (slot < 0) slot = i; }
    else if (g_known[i].heard_ms < g_known[oldest].heard_ms) oldest = i;
  }
  if (slot < 0) slot = oldest;      // voll: ältesten ersetzen
  Advert& e = g_known[slot];
  memcpy(e.raw, raw, len); e.len = len;
  memcpy(e.pub, pub, PUB_KEY_SIZE);
  if (name) { strncpy(e.name, name, sizeof(e.name) - 1); e.name[sizeof(e.name) - 1] = 0; }
  e.type = type; e.has_loc = has_loc; e.adv_ts = adv_timestamp;
  e.heard_ms = millis();
  e.up_state = 0;
  unlock();
}

bool generateKey(String& err) {
  lock();
  esp_fill_random(g_seed, 32);
  deriveFromSeed();
  saveKey();
  // Vorherige Upload-Ergebnisse gelten nicht mehr für den neuen Schlüssel.
  g_self.up_state = 0;
  for (int i = 0; i < MAX_KNOWN; i++) g_known[i].up_state = 0;
  unlock();
  event_log::addf("MAP", "Neuer Karten-Upload-Schlüssel erzeugt");
  return true;
}

void requestSelfUpload()  { g_want_self_capture = true; g_self_ready = false; g_pending_self = true; }
void requestKnownUpload() { g_pending_known = true; }

void setAutoHours(uint16_t hours) {
  g_auto_hours = hours;
  Preferences p; p.begin(NVS_NS, false);
  p.putUShort("auto_h", hours);
  p.end();
  g_next_auto_ms = hours ? millis() + (uint32_t)hours * 3600000UL : 0;
  event_log::addf("MAP", hours ? "Automatisches Melden: alle %u h" : "Automatisches Melden: aus", (unsigned)hours);
}

uint16_t getAutoHours() { return g_auto_hours; }

String statusJson() {
  char pubHex[65]; toHex(g_pub, 32, pubHex);
  MeshSnapshot s = gateway_status::get();
  lock();
  String j = "{";
  j += "\"hasKey\":"; j += (g_has_key ? "true" : "false");
  j += ",\"key\":\""; j += (g_has_key ? pubHex : ""); j += "\"";
  j += ",\"busy\":"; j += (g_busy ? "true" : "false");
  j += ",\"autoHours\":"; j += String((unsigned)g_auto_hours);
  j += ",\"lastMsg\":\""; j += jsonEscape(String(g_last_msg)); j += "\"";
  // eigener Eintrag
  j += ",\"self\":{\"name\":\""; j += jsonEscape(String(s.nodeName));
  j += "\",\"hasLoc\":"; j += ((s.lat != 0 || s.lon != 0) ? "true" : "false");
  j += ",\"upState\":"; j += String((int)g_self.up_state); j += "}";
  // bekannte Repeater
  j += ",\"known\":[";
  bool first = true;
  uint32_t now = millis();
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (g_known[i].heard_ms == 0) continue;
    char kp[13]; toHex(g_known[i].pub, 6, kp);
    if (!first) j += ",";
    first = false;
    j += "{\"name\":\""; j += jsonEscape(String(g_known[i].name));
    j += "\",\"key\":\""; j += kp;
    j += "\",\"type\":"; j += String((int)g_known[i].type);
    j += ",\"hasLoc\":"; j += (g_known[i].has_loc ? "true" : "false");
    j += ",\"agoS\":"; j += String((now - g_known[i].heard_ms) / 1000);
    j += ",\"upState\":"; j += String((int)g_known[i].up_state);
    j += "}";
  }
  j += "]}";
  unlock();
  return j;
}

}  // namespace map_uploader
