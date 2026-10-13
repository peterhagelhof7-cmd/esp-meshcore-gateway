#include "ChannelStore.h"

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

using namespace channel_store;

// Öffentlicher Kanal, Schlüssel aus docs/companion_protocol.md
// (8b3387e9c5cdea6ac9e5edbaa115cd72 = Base64 izOH6cXN6mrJ5e26oRXNcg==).
static const uint8_t PUBLIC_KEY[16] = {0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
                                       0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72};

static const char* NVS_NS = "gw";
static const char* NVS_KEY = "channels";
static const uint8_t STORE_VERSION = 1;

static GwChannel s_ch[MAX_CHANNELS];
static SemaphoreHandle_t s_lock = nullptr;

static void sha256(const uint8_t* in, size_t len, uint8_t out[32]) {
  mbedtls_sha256(in, len, out, 0);
}

static void save() {
  // Aufrufer hält den Mutex.
  uint8_t buf[1 + sizeof(s_ch)];
  buf[0] = STORE_VERSION;
  memcpy(buf + 1, s_ch, sizeof(s_ch));
  Preferences p;
  p.begin(NVS_NS, false);
  p.putBytes(NVS_KEY, buf, sizeof(buf));
  p.end();
}

void channel_store::begin() {
  if (!s_lock) s_lock = xSemaphoreCreateMutex();
  memset(s_ch, 0, sizeof(s_ch));
  uint8_t buf[1 + sizeof(s_ch)];
  Preferences p;
  p.begin(NVS_NS, true);
  size_t n = p.getBytes(NVS_KEY, buf, sizeof(buf));
  p.end();
  if (n == sizeof(buf) && buf[0] == STORE_VERSION) {
    memcpy(s_ch, buf + 1, sizeof(s_ch));
  }
}

void channel_store::list(GwChannel out[MAX_CHANNELS]) {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  memcpy(out, s_ch, sizeof(s_ch));
  xSemaphoreGive(s_lock);
}

bool channel_store::get(int idx, GwChannel& out) {
  if (idx < 0 || idx >= MAX_CHANNELS) return false;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  out = s_ch[idx];
  xSemaphoreGive(s_lock);
  return out.used;
}

bool channel_store::validScope(const char* s) {
  if (!s || !*s) return true;   // leer = unscoped
  if (strlen(s) >= sizeof(GwChannel::scope)) return false;
  const char* p = (*s == '#') ? s + 1 : s;
  if (!*p) return false;
  for (; *p; p++) {
    char c = *p;
    // wie RegionMap::is_name_char (Buchstaben, Ziffern, '-', '_', '.')
    if (!(isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.')) return false;
  }
  return true;
}

static bool parseKey(const char* text, uint8_t out[16]) {
  size_t len = strlen(text);
  if (len == 32) {   // Hex
    for (size_t i = 0; i < 16; i++) {
      char hi = text[2 * i], lo = text[2 * i + 1];
      if (!isxdigit((unsigned char)hi) || !isxdigit((unsigned char)lo)) return false;
      char tmp[3] = {hi, lo, 0};
      out[i] = (uint8_t)strtoul(tmp, nullptr, 16);
    }
    return true;
  }
  uint8_t dec[48];
  size_t olen = 0;
  if (mbedtls_base64_decode(dec, sizeof(dec), &olen, (const unsigned char*)text, len) != 0) return false;
  if (olen != 16) return false;   // 32-Byte-Kanalschlüssel nutzt die App nicht
  memcpy(out, dec, 16);
  return true;
}

int channel_store::add(Kind kind, const char* name, const char* keyText, const char* scope, String& err) {
  GwChannel c;
  memset(&c, 0, sizeof(c));

  String n = name ? String(name) : String();
  n.trim();
  if (kind == Kind::PUBLIC) {
    if (n.isEmpty()) n = "Public";
    memcpy(c.secret, PUBLIC_KEY, 16);
  } else if (kind == Kind::HASHTAG) {
    if (!n.startsWith("#")) n = "#" + n;
    // hansemesh: Hash ist case-sensitive, Konvention alles klein, keine
    // Leerzeichen/Umlaute -> wir erzwingen genau das, damit derselbe Kanal
    // nicht doppelt entsteht.
    n.toLowerCase();
    for (size_t i = 1; i < n.length(); i++) {
      char ch = n[i];
      if (!(isalnum((unsigned char)ch) || ch == '-' || ch == '_')) {
        err = "Hashtag-Kanal: nur a-z, 0-9, - und _";
        return -1;
      }
    }
    if (n.length() < 2) { err = "Name fehlt"; return -1; }
    uint8_t h[32];
    sha256((const uint8_t*)n.c_str(), n.length(), h);
    memcpy(c.secret, h, 16);
  } else {
    if (n.isEmpty()) { err = "Name fehlt"; return -1; }
    if (n.startsWith("#")) { err = "Private Kanäle ohne #"; return -1; }
    if (keyText && *keyText) {
      if (!parseKey(keyText, c.secret)) { err = "Schlüssel: 16 Byte als Base64 oder 32 Hex-Zeichen"; return -1; }
    } else {
      esp_fill_random(c.secret, 16);
    }
  }
  if (n.length() >= sizeof(c.name)) { err = "Name zu lang (max. 31)"; return -1; }
  if (!validScope(scope)) { err = "Scope: Regionsname aus a-z, 0-9, - _ ."; return -1; }

  strlcpy(c.name, n.c_str(), sizeof(c.name));
  strlcpy(c.scope, scope ? scope : "", sizeof(c.scope));
  uint8_t h[32];
  sha256(c.secret, 16, h);
  c.hash = h[0];
  c.used = true;

  xSemaphoreTake(s_lock, portMAX_DELAY);
  int idx = -1;
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (s_ch[i].used && memcmp(s_ch[i].secret, c.secret, 16) == 0) {
      xSemaphoreGive(s_lock);
      err = String("Kanal mit diesem Schlüssel existiert schon: ") + s_ch[i].name;
      return -1;
    }
  }
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (!s_ch[i].used) { idx = i; break; }
  }
  if (idx >= 0) {
    s_ch[idx] = c;
    save();
  }
  xSemaphoreGive(s_lock);
  if (idx < 0) err = "Maximal 8 Kanäle";
  return idx;
}

bool channel_store::remove(int idx) {
  if (idx < 0 || idx >= MAX_CHANNELS) return false;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  bool ok = s_ch[idx].used;
  if (ok) {
    memset(&s_ch[idx], 0, sizeof(GwChannel));
    save();
  }
  xSemaphoreGive(s_lock);
  return ok;
}

bool channel_store::setScope(int idx, const char* scope, String& err) {
  if (idx < 0 || idx >= MAX_CHANNELS) { err = "Kanal unbekannt"; return false; }
  if (!validScope(scope)) { err = "Scope: Regionsname aus a-z, 0-9, - _ ."; return false; }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  bool ok = s_ch[idx].used;
  if (ok) {
    strlcpy(s_ch[idx].scope, scope ? scope : "", sizeof(s_ch[idx].scope));
    save();
  }
  xSemaphoreGive(s_lock);
  if (!ok) err = "Kanal unbekannt";
  return ok;
}

int channel_store::findByName(const char* name) {
  if (!name || !*name) return -1;
  int idx = -1;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (s_ch[i].used && strcmp(s_ch[i].name, name) == 0) { idx = i; break; }
  }
  xSemaphoreGive(s_lock);
  return idx;
}

int channel_store::findByHash(uint8_t hash, int idx[], int max) {
  int n = 0;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  for (int i = 0; i < MAX_CHANNELS && n < max; i++) {
    if (s_ch[i].used && s_ch[i].hash == hash) idx[n++] = i;
  }
  xSemaphoreGive(s_lock);
  return n;
}

channel_store::Kind channel_store::kindOf(const GwChannel& c) {
  if (memcmp(c.secret, PUBLIC_KEY, 16) == 0) return Kind::PUBLIC;
  if (c.name[0] == '#') return Kind::HASHTAG;
  return Kind::PRIVATE;
}

String channel_store::keyBase64(const GwChannel& c) {
  unsigned char out[32];
  size_t olen = 0;
  mbedtls_base64_encode(out, sizeof(out), &olen, c.secret, 16);
  return String((const char*)out).substring(0, olen);
}

String channel_store::keyHex(const GwChannel& c) {
  char buf[33];
  for (int i = 0; i < 16; i++) sprintf(buf + 2 * i, "%02x", c.secret[i]);
  return String(buf);
}
