#include "AckTracker.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace ack {

static void cpy(char* dst, size_t n, const char* src) {
  size_t i = 0;
  if (src) for (; i + 1 < n && src[i]; i++) dst[i] = src[i];
  dst[i] = 0;
}

uint8_t Tracker::nextId() {
  // 1..99, keine Nummer doppelt vergeben, solange sie offen ist
  for (int tries = 0; tries < 99; tries++) {
    _lastId = _lastId >= 99 ? 1 : _lastId + 1;
    bool taken = false;
    for (int i = 0; i < MAX_OPEN; i++) taken = taken || (_e[i].used && _e[i].id == _lastId);
    if (!taken) return _lastId;
  }
  return _lastId;
}

uint8_t Tracker::open(Kind kind, int src, const char* channel, const char* text, uint32_t nowMs) {
  int slot = -1;
  for (int i = 0; i < MAX_OPEN; i++) {
    if (_e[i].used && _e[i].kind == kind && _e[i].src == src) { slot = i; break; }
  }
  if (slot < 0) {
    for (int i = 0; i < MAX_OPEN; i++) if (!_e[i].used) { slot = i; break; }
  }
  if (slot < 0) {   // voll: ältesten verdrängen
    slot = 0;
    for (int i = 1; i < MAX_OPEN; i++) if (nowMs - _e[i].sentMs > nowMs - _e[slot].sentMs) slot = i;
  }
  drop(slot);
  Entry& e = _e[slot];
  e.used = true;
  e.id = nextId();
  e.kind = kind;
  e.src = (int8_t)src;
  cpy(e.channel, sizeof(e.channel), channel);
  cpy(e.text, sizeof(e.text), text);
  e.sentMs = nowMs;
  e.lastMs = nowMs;
  return e.id;
}

void Tracker::setZbx(uint8_t id, const char* channel2, const char* marker, bool hasValue, float value) {
  for (int i = 0; i < MAX_OPEN; i++) {
    Entry& e = _e[i];
    if (!e.used || e.id != id) continue;
    cpy(e.channel2, sizeof(e.channel2), channel2);
    cpy(e.marker, sizeof(e.marker), marker);
    e.hasValue = hasValue;
    e.value = value;
  }
}

void Tracker::markCleared(Kind kind, int src) {
  for (int i = 0; i < MAX_OPEN; i++) {
    if (_e[i].used && _e[i].kind == kind && _e[i].src == src) _e[i].cleared = true;
  }
}

uint8_t Tracker::openIdOf(Kind kind, int src) const {
  for (int i = 0; i < MAX_OPEN; i++) {
    if (_e[i].used && _e[i].kind == kind && _e[i].src == src) return _e[i].id;
  }
  return 0;
}

int Tracker::due(uint32_t nowMs) {
  for (int i = 0; i < MAX_OPEN; i++) {
    Entry& e = _e[i];
    if (!e.used) continue;
    uint32_t age = nowMs - e.sentMs;
    if (age >= KEEP_MS) { drop(i); continue; }
    if (e.repeats < REPEATS && age >= REPEAT_AFTER_MIN[e.repeats] * 60000UL && nowMs - e.lastMs >= MIN_GAP_MS) return i;
  }
  return -1;
}

void Tracker::repeated(int idx, uint32_t nowMs) {
  if (idx < 0 || idx >= MAX_OPEN || !_e[idx].used) return;
  // Jede Wiederholung zählt genau eine Stufe; nach einer Pause (z. B.
  // Sendelimit) sorgt MIN_GAP_MS dafür, dass nicht alle auf einmal kommen.
  if (_e[idx].repeats < REPEATS) _e[idx].repeats++;
  _e[idx].lastMs = nowMs;
}

bool Tracker::parseAck(const char* body, int& id, char* target, size_t targetLen) {
  id = 0;
  if (target && targetLen) target[0] = 0;
  if (!body) return false;
  const char* p = body;
  while (*p == ' ') p++;
  // Antwort-Erwähnung der App bzw. gezieltes ACK des Zabbix-Empfängers: "@[Name] ..."
  if (p[0] == '@' && p[1] == '[') {
    const char* end = strchr(p, ']');
    if (!end) return false;
    if (target && targetLen) {
      size_t n = (size_t)(end - (p + 2));
      if (n >= targetLen) n = targetLen - 1;
      memcpy(target, p + 2, n);
      target[n] = 0;
    }
    p = end + 1;
    while (*p == ' ') p++;
  }
  if (!(toupper((unsigned char)p[0]) == 'A' && toupper((unsigned char)p[1]) == 'C' &&
        toupper((unsigned char)p[2]) == 'K')) return false;
  p += 3;
  while (*p == ' ' || *p == '#') p++;
  if (isdigit((unsigned char)*p)) {
    int n = 0;
    while (isdigit((unsigned char)*p) && n < 1000) n = n * 10 + (*p++ - '0');
    if (n < 1 || n > 99) return false;
    id = n;
  }
  while (*p == ' ' || *p == '.' || *p == '!') p++;
  return *p == 0;   // nur "ACK [Nr]" - ganze Sätze mit "ack..." zählen nicht
}

static bool sameName(const char* a, const char* b) {
  for (; *a && *b; a++, b++) if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
  return *a == *b;
}

int Tracker::acknowledge(const char* channel, const char* body, const char* ownName, uint8_t* ids, int maxIds) {
  int want;
  char target[32];
  if (!parseAck(body, want, target, sizeof(target))) return -1;
  if (target[0] && !sameName(target, ownName ? ownName : "")) return -2;
  const char* ch = channel ? channel : "";
  int n = 0;
  for (int i = 0; i < MAX_OPEN; i++) {
    Entry& e = _e[i];
    if (!e.used || !*ch || (strcmp(e.channel, ch) != 0 && strcmp(e.channel2, ch) != 0)) continue;
    if (want && e.id != want) continue;
    if (n < maxIds) ids[n] = e.id;
    n++;
    drop(i);
  }
  return n;
}

bool Tracker::acknowledgeId(uint8_t id) {
  for (int i = 0; i < MAX_OPEN; i++) {
    if (_e[i].used && _e[i].id == id) { drop(i); return true; }
  }
  return false;
}

void Tracker::remap(Kind kind, const int8_t* map, int n) {
  for (int i = 0; i < MAX_OPEN; i++) {
    Entry& e = _e[i];
    if (!e.used || e.kind != kind) continue;
    int to = (e.src >= 0 && e.src < n) ? map[e.src] : -1;
    if (to < 0) drop(i);
    else e.src = (int8_t)to;
  }
}

size_t Tracker::format(const Entry& e, bool repeat, char* out, size_t outLen) {
  int n;
  if (repeat) {
    n = snprintf(out, outLen, "[ACK %u] WDH %u/%d%s: %s", (unsigned)e.id, (unsigned)e.repeats + 1, REPEATS,
                 e.cleared ? " (inzw. OK)" : "", e.text);
  } else {
    n = snprintf(out, outLen, "[ACK %u] %s", (unsigned)e.id, e.text);
  }
  if (n < 0) { if (outLen) out[0] = 0; return 0; }
  return (size_t)n < outLen ? (size_t)n : outLen - 1;
}

}  // namespace ack
